#include <fakeLisp/vm.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        ++checks;                                                              \
        if (cond) {                                                            \
            printf("ok: %s\n", msg);                                           \
        } else {                                                               \
            ++failures;                                                        \
            printf("FAILED: %s (line %d)\n", msg, __LINE__);                   \
        }                                                                      \
    } while (0)

static FklVMvalue *g_frame_root = NULL;

static void test_frame_atomic(void *data, FklVMgc *gc) {
    (void)data;
    fklVMgcToGray(g_frame_root, gc);
}

static const FklVMframeCtxMt TestFrameMt = {
    .atomic = test_frame_atomic,
};

static void clear_vm_stack(FklVM *vm) {
    while (vm->tp)
        (void)FKL_VM_POP_TOP_VALUE(vm);
    vm->bp = 0;
}

typedef struct {
    int64_t items[16];
    size_t len;
    size_t stop_after;
} TraverseCtx;

static int collect_fix_cb(FklVMforeachCtx *ctx_, FklVMvalue *v) {
    TraverseCtx *ctx = (TraverseCtx *)ctx_;
    if (ctx->len < 16)
        ctx->items[ctx->len] = FKL_GET_FIX(v);
    ++ctx->len;
    if (ctx->stop_after != 0 && ctx->len >= ctx->stop_after)
        return 1;
    return 0;
}

static int seq_eq(const TraverseCtx *got, const int64_t *want, size_t n) {
    if (got->len != n)
        return 0;
    for (size_t i = 0; i < n; ++i)
        if (got->items[i] != want[i])
            return 0;
    return 1;
}

int main(void) {
    FklVMgc *gc = fklCreateVMgc(NULL);
    CHECK(gc != NULL, "create gc");
    if (gc == NULL)
        return 1;

    FklVM *vm = fklCreateVM(NULL, gc);
    CHECK(vm != NULL, "create vm");
    if (vm == NULL) {
        fklDestroyVMgc(gc);
        return 1;
    }

    /* --- frame sizing invariants --- */
    CHECK(sizeof(FklVMframe) % sizeof(FklVMvalue *) == 0,
            "frame size is a whole number of stack slots");
    CHECK(FKL_VM_FRAME_SIZE * sizeof(FklVMvalue *) == sizeof(FklVMframe),
            "FKL_VM_FRAME_SIZE matches the frame struct");
    CHECK(FKL_VM_FRAME_SIZE > 0, "frame occupies at least one slot");
    CHECK(FKL_TAG_SKIP < FKL_PTR_TAG_NUM,
            "skip tag fits in the pointer tag set");

    /* --- skip marker round trip --- */
    {
        FklVMvalue **p = &vm->base[8];
        FklVMvalue *skip = FKL_MAKE_VM_SKIP(p);
        CHECK(FKL_GET_TAG(skip) == FKL_TAG_SKIP,
                "skip marker carries SKIP tag");
        CHECK(FKL_VM_SKIP(skip) == p, "skip marker round trips its target");
    }

    /* --- fklPrepCall layout: marker, embedded frame, callee + args --- */
    {
        clear_vm_stack(vm);
        uint32_t at = vm->tp;
        FklVMvalue *args[3] = {
            FKL_MAKE_VM_FIX(101),
            FKL_MAKE_VM_FIX(102),
            FKL_MAKE_VM_FIX(103),
        };
        FklVMframe *f = fklPrepCall(vm, at, 3, args);

        CHECK(f == FKL_SLOT_TO_FRAME(&vm->base[at]),
                "prepCall embeds the frame at the base slot");
        CHECK(vm->tp == at + FKL_VM_FRAME_SIZE + 3,
                "prepCall advances tp past frame and args");
        CHECK(FKL_GET_TAG(vm->base[at]) == FKL_TAG_SKIP,
                "prepCall writes the skip pointer into the frame");
        CHECK(FKL_VM_SKIP(vm->base[at]) == &vm->base[at + FKL_VM_FRAME_SIZE],
                "skip pointer points just past the embedded frame");
        CHECK(vm->base[at + FKL_VM_FRAME_SIZE] == args[0],
                "callee slot holds the first argument value");
        CHECK(vm->base[at + FKL_VM_FRAME_SIZE + 1] == args[1],
                "first arg slot holds the second argument value");
        CHECK(vm->base[at + FKL_VM_FRAME_SIZE + 2] == args[2],
                "second arg slot holds the third argument value");

        /* FKL_VM_GET_ARG indexes from the frame bp: -1 is the callee */
        f->bp = at;
        CHECK(FKL_VM_GET_ARG(vm, f, -1) == args[0],
                "GET_ARG(-1) is the callee");
        CHECK(FKL_VM_GET_ARG(vm, f, 0) == args[1],
                "GET_ARG(0) is the first arg");
        CHECK(FKL_VM_GET_ARG(vm, f, 1) == args[2],
                "GET_ARG(1) is the second arg");
        clear_vm_stack(vm);
    }

    /* --- FKL_VM_BP_IDX maps to the saved bp slot just before bp --- */
    {
        clear_vm_stack(vm);
        uint32_t at = 8;
        FklVMframe *f = fklPrepCall(vm, at, 0, NULL);
        f->bp = at;
        vm->base[at - 1] = FKL_MAKE_VM_FIX(42);
        CHECK(FKL_GET_FIX(FKL_VM_GET_ARG(vm, f, FKL_VM_BP_IDX)) == 42,
                "BP_IDX reads the slot just below bp");
        clear_vm_stack(vm);
    }

    /* --- fklSetBp without prep just pushes the old bp marker --- */
    {
        clear_vm_stack(vm);
        uint32_t saved_tp = vm->tp;
        uint32_t saved_bp = vm->bp;
        FklVMframe *f = fklSetBp(vm, 0);
        CHECK(f == NULL, "setBp without prep returns no frame");
        CHECK(vm->tp == saved_tp + 1, "setBp pushes exactly one marker");
        CHECK(vm->bp == saved_tp + 1, "setBp moves bp past the marker");
        CHECK(FKL_GET_FIX(vm->base[saved_tp]) == (int64_t)saved_bp,
                "setBp stores the old bp");
        vm->bp = 0;
        clear_vm_stack(vm);
    }

    /* --- fklSetBp with prep embeds a call frame --- */
    {
        clear_vm_stack(vm);
        FklVMframe *f = fklSetBp(vm, 1);
        CHECK(f != NULL, "setBp with prep returns a frame");
        CHECK(f == FKL_SLOT_TO_FRAME(&vm->base[vm->bp]),
                "prep frame sits at bp");
        CHECK(FKL_GET_TAG(vm->base[vm->bp]) == FKL_TAG_SKIP,
                "prep skip pointer sits at bp");
        CHECK(vm->tp == vm->bp + FKL_VM_FRAME_SIZE,
                "setBp prep advances tp past the frame");
        CHECK(FKL_VM_SKIP(vm->base[vm->bp])
                        == &vm->base[vm->bp + FKL_VM_FRAME_SIZE],
                "prep skip pointer points past the embedded frame");
        clear_vm_stack(vm);
    }

    /* --- fklSetBpAt sets bp at an absolute slot --- */
    {
        clear_vm_stack(vm);
        uint32_t const at = 4;
        uint32_t const old_bp = vm->bp;
        FklVMvalue *arg0 = FKL_MAKE_VM_FIX(7);
        FklVMframe *f = fklSetBpAt(vm, at, old_bp, 1, 1, &arg0);
        CHECK(f == FKL_SLOT_TO_FRAME(&vm->base[at + 1]),
                "setBpAt embeds the frame at at+1");
        CHECK(vm->bp == at + 1, "setBpAt sets bp to the frame slot");
        CHECK(FKL_GET_FIX(vm->base[at]) == (int64_t)old_bp,
                "setBpAt stores the old bp at slot at");
        CHECK(FKL_GET_TAG(vm->base[at + 1]) == FKL_TAG_SKIP,
                "setBpAt writes the skip pointer into the frame");
        CHECK(vm->base[at + 1 + FKL_VM_FRAME_SIZE] == arg0,
                "setBpAt places the callee arg after the frame");
        clear_vm_stack(vm);
    }

    /* --- fklPushVMframe links frames and top_frame --- */
    {
        clear_vm_stack(vm);
        vm->top_frame = NULL;
        FklVMframe *f1 = fklPrepCall(vm, 1, 0, NULL);
        FklVMframe *f2 = fklPrepCall(vm, vm->tp, 0, NULL);
        fklPushVMframe(vm, f1);
        CHECK(vm->top_frame == f1 && f1->prev == NULL,
                "pushing the first frame sets top_frame");
        fklPushVMframe(vm, f2);
        CHECK(vm->top_frame == f2 && f2->prev == f1,
                "pushing the second frame links prev");
        vm->top_frame = NULL;
        clear_vm_stack(vm);
    }

    /* --- fklInitVMframeExt / fklUninitVMframe --- */
    {
        FklVM *gv = &gc->gcvm;
        clear_vm_stack(gv);
        FklVMframe *f = fklPrepCall(gv, 1, 0, NULL);
        FklVMframe *r = fklInitVMframeExt(gv, f, &TestFrameMt);
        CHECK(r == f, "initExt returns the same embedded slot");
        CHECK(f->type == FKL_FRAME_OTHEROBJ, "initExt marks an OTHEROBJ frame");
        CHECK(f->t == &TestFrameMt, "initExt stores the method table");
        fklUninitVMframe(gv, f);
        CHECK(f->prev == NULL, "uninit clears prev");
        clear_vm_stack(gv);
    }

    /* --- GC must skip the embedded frame and keep real stack roots --- */
    {
        FklVM *gv = &gc->gcvm;
        clear_vm_stack(gv);
        FklVMframe *old_top = gv->top_frame;
        gv->top_frame = NULL;

        g_frame_root = fklCreateVMvalueStr1(gv, "frame-root-string");
        FklVMvalue *stack_keep = fklCreateVMvalueStr1(gv, "stack-keep-string");

        FklVMframe *f = fklPrepCall(gv, 1, 1, &stack_keep);
        fklInitVMframeExt(gv, f, &TestFrameMt);
        fklPushVMframe(gv, f);

        fklVMgcCheck(gv, 1);

        CHECK(FKL_IS_STR(g_frame_root),
                "value rooted via the embedded frame survives GC");
        CHECK(strcmp(FKL_VM_STR(g_frame_root)->str, "frame-root-string") == 0,
                "frame-rooted value content is intact");
        CHECK(FKL_IS_STR(gv->base[1 + FKL_VM_FRAME_SIZE]),
                "stack value past the embedded frame survives GC");
        CHECK(strcmp(FKL_VM_STR(gv->base[1 + FKL_VM_FRAME_SIZE])->str,
                      "stack-keep-string")
                        == 0,
                "stack value content is intact");

        gv->top_frame = old_top;
        clear_vm_stack(gv);
    }

    /* --- walking a stack with a frame followed by values still roots them ---
     */
    {
        FklVM *gv = &gc->gcvm;
        clear_vm_stack(gv);
        FklVMframe *old_top = gv->top_frame;
        gv->top_frame = NULL;

        FklVMvalue *a = fklCreateVMvalueStr1(gv, "after-frame-a");
        FklVMvalue *b = fklCreateVMvalueStr1(gv, "after-frame-b");
        FklVMframe *f = fklPrepCall(gv, 1, 2, (FklVMvalue *[]){ a, b });
        fklInitVMframeExt(gv, f, &TestFrameMt);

        fklVMgcCheck(gv, 1);

        CHECK(FKL_IS_STR(gv->base[1 + FKL_VM_FRAME_SIZE])
                        && strcmp(FKL_VM_STR(gv->base[1 + FKL_VM_FRAME_SIZE])
                                           ->str,
                                   "after-frame-a")
                                   == 0,
                "first value after a frame survives the stack walk");
        CHECK(FKL_IS_STR(gv->base[1 + FKL_VM_FRAME_SIZE + 1])
                        && strcmp(FKL_VM_STR(
                                          gv->base[1 + FKL_VM_FRAME_SIZE + 1])
                                           ->str,
                                   "after-frame-b")
                                   == 0,
                "second value after a frame survives the stack walk");

        gv->top_frame = old_top;
        clear_vm_stack(gv);
    }

    /* --- fklVMframeClear preserves the links and zeroes the content --- */
    {
        clear_vm_stack(vm);
        FklVMvalue **skip_target = &vm->base[42];
        FklVMvalue *const skip = FKL_MAKE_VM_SKIP(skip_target);
        FklVMvalue *const skip_back = FKL_MAKE_VM_FIX(0x55);

        FklVMframe *f = fklPrepCall(vm, 1, 0, NULL);
        f->skip = skip;
        f->skip_back = skip_back;
        f->type = FKL_FRAME_OTHEROBJ;
        f->bp = 42;
        f->sp = 7;
        f->prev = f;
        f->proc = FKL_MAKE_VM_FIX(1);

        fklVMframeClear(f);

        CHECK(f->skip == skip, "frameClear preserves the skip link");
        CHECK(f->skip_back == skip_back, "frameClear preserves skip_back");
        CHECK(f->type == 0 && f->bp == 0 && f->sp == 0,
                "frameClear zeroes the frame header and content");
        CHECK(f->prev == f && f->proc == NULL,
                "frameClear preserves prev and zeroes value fields");
        clear_vm_stack(vm);
    }

    /* --- fklVMforeachStack / fklVMforeachStackReverse --- */
    {
        /* values before, after and between two embedded frames */
        clear_vm_stack(vm);
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(1));
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(2));
        FklVMvalue *arg_a = FKL_MAKE_VM_FIX(3);
        fklPrepCall(vm, 2, 1, &arg_a);
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(4));
        FklVMvalue *args_b[2] = { FKL_MAKE_VM_FIX(5), FKL_MAKE_VM_FIX(6) };
        fklPrepCall(vm, vm->tp, 2, args_b);
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(7));

        TraverseCtx fwd = { 0 };
        TraverseCtx rev = { 0 };
        fklVMforeachStack(vm, (FklVMforeachCtx *)&fwd, collect_fix_cb);
        fklVMforeachStackReverse(vm, (FklVMforeachCtx *)&rev, collect_fix_cb);
        CHECK(seq_eq(&fwd, (int64_t[]){ 1, 2, 3, 4, 5, 6, 7 }, 7),
                "foreachStack visits values in order, skipping frames");
        CHECK(seq_eq(&rev, (int64_t[]){ 7, 6, 5, 4, 3, 2, 1 }, 7),
                "foreachStackReverse is the exact reverse");

        TraverseCtx stop_f = { .stop_after = 3 };
        TraverseCtx stop_r = { .stop_after = 3 };
        fklVMforeachStack(vm, (FklVMforeachCtx *)&stop_f, collect_fix_cb);
        fklVMforeachStackReverse(vm,
                (FklVMforeachCtx *)&stop_r,
                collect_fix_cb);
        CHECK(seq_eq(&stop_f, (int64_t[]){ 1, 2, 3 }, 3),
                "foreachStack stops when the callback returns non-zero");
        CHECK(seq_eq(&stop_r, (int64_t[]){ 7, 6, 5 }, 3),
                "foreachStackReverse stops when the callback returns non-zero");

        clear_vm_stack(vm);
    }

    /* --- traversal of an empty stack --- */
    {
        clear_vm_stack(vm);
        TraverseCtx fwd = { 0 }, rev = { 0 };
        fklVMforeachStack(vm, (FklVMforeachCtx *)&fwd, collect_fix_cb);
        fklVMforeachStackReverse(vm, (FklVMforeachCtx *)&rev, collect_fix_cb);
        CHECK(fwd.len == 0 && rev.len == 0,
                "empty stack yields nothing in both directions");
    }

    /* --- traversal with adjacent frames (nothing between them) --- */
    {
        clear_vm_stack(vm);
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(11));
        fklPrepCall(vm, vm->tp, 0, NULL);
        fklPrepCall(vm, vm->tp, 0, NULL);
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(12));

        TraverseCtx fwd = { 0 }, rev = { 0 };
        fklVMforeachStack(vm, (FklVMforeachCtx *)&fwd, collect_fix_cb);
        fklVMforeachStackReverse(vm, (FklVMforeachCtx *)&rev, collect_fix_cb);
        CHECK(seq_eq(&fwd, (int64_t[]){ 11, 12 }, 2),
                "adjacent empty frames are skipped going forward");
        CHECK(seq_eq(&rev, (int64_t[]){ 12, 11 }, 2),
                "adjacent empty frames are skipped going backward");

        clear_vm_stack(vm);
    }

    /* --- traversal when a frame sits at the top of the stack --- */
    {
        clear_vm_stack(vm);
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(21));
        FklVMvalue *arg = FKL_MAKE_VM_FIX(22);
        fklPrepCall(vm, vm->tp, 1, &arg);

        TraverseCtx fwd = { 0 }, rev = { 0 };
        fklVMforeachStack(vm, (FklVMforeachCtx *)&fwd, collect_fix_cb);
        fklVMforeachStackReverse(vm, (FklVMforeachCtx *)&rev, collect_fix_cb);
        CHECK(seq_eq(&fwd, (int64_t[]){ 21, 22 }, 2),
                "top-of-stack frame traverses forward");
        CHECK(seq_eq(&rev, (int64_t[]){ 22, 21 }, 2),
                "top-of-stack frame traverses backward");

        clear_vm_stack(vm);
    }

    /* --- a frame may live at base[0] (e.g. push_macro_expand_frame);
     * reverse traversal jumps to &base[0] and ends, which is expected --- */
    {
        clear_vm_stack(vm);
        FklVMvalue *arg = FKL_MAKE_VM_FIX(100);
        fklPrepCall(vm, 0, 1, &arg);
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(200));

        TraverseCtx fwd = { 0 }, rev = { 0 };
        fklVMforeachStack(vm, (FklVMforeachCtx *)&fwd, collect_fix_cb);
        fklVMforeachStackReverse(vm, (FklVMforeachCtx *)&rev, collect_fix_cb);
        CHECK(seq_eq(&fwd, (int64_t[]){ 100, 200 }, 2),
                "frame at base[0] traverses forward");
        CHECK(seq_eq(&rev, (int64_t[]){ 200, 100 }, 2),
                "reverse terminates at a frame sitting at base[0]");

        clear_vm_stack(vm);
    }

    /* --- a bare frame at base[0] with nothing else --- */
    {
        clear_vm_stack(vm);
        fklPrepCall(vm, 0, 0, NULL);

        TraverseCtx fwd = { 0 }, rev = { 0 };
        fklVMforeachStack(vm, (FklVMforeachCtx *)&fwd, collect_fix_cb);
        fklVMforeachStackReverse(vm, (FklVMforeachCtx *)&rev, collect_fix_cb);
        CHECK(fwd.len == 0 && rev.len == 0,
                "a bare frame at base[0] yields nothing either way");

        clear_vm_stack(vm);
    }

    fklDestroyAllVMs(vm);
    fklDestroyVMgc(gc);

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
