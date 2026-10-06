#include <fakeLisp/bytecode.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/vm.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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

static void reset_vm(FklVM *vm) {
    while (vm->tp)
        (void)FKL_VM_POP_TOP_VALUE(vm);
    vm->top_frame = NULL;
    vm->bp = 0;
}

static void setup_call(FklVM *vm, FklVMframe *f, FklVMvalue *callee) {
    int r = fklCallObj(vm, f, callee);
    (void)r;
}

static FklVMvalue *make_proc(FklVM *vm, uint32_t local_count) {
    static const FklIns code[] = {
        FKL_MAKE_INS_I(FKL_OP_PUSH_NIL),
        FKL_MAKE_INS_I(FKL_OP_RET),
    };
    FklVMvalueProto *pt = fklCreateVMvalueProto(vm, 0);
    pt->local_count = local_count;
    return fklCreateVMvalueProc2(vm, code, 2, NULL, pt);
}

/* --- cproc --- */
static int g_cfunc_calls = 0;
static uint32_t g_cfunc_argc = 0;

static int test_cfunc(FKL_CPROC_ARGL) {
    ++g_cfunc_calls;
    g_cfunc_argc = argc;
    return 0;
}

/* --- userdata with a call method --- */
static const int g_ud_token = 0;
static int g_ud_calls = 0;
static FklVMvalue *g_ud_call_value = NULL;
static FklVMframe *g_ud_call_frame = NULL;

static int test_ud_call(FklVMvalue *v, FklVMframe *f, FklVM *exe) {
    (void)exe;
    ++g_ud_calls;
    g_ud_call_value = v;
    g_ud_call_frame = f;
    return 0;
}

static const FklVMudMetaTable TestUdMt = {
    .name = "test-ud",
    .size = sizeof(FklVMvalueUd),
    .call = test_ud_call,
};

static int g_other_frame_finalized = 0;
static void test_other_frame_finalizer(void *data) {
    (void)data;
    ++g_other_frame_finalized;
}
static const FklVMframeCtxMt TestFinalizerMt = {
    .finalizer = test_other_frame_finalizer,
};

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

    FklVMvalue *proc = make_proc(vm, 2);
    FklVMvalue *cproc = fklCreateVMvalueCproc(vm, test_cfunc, NULL, "test-cproc");
    FklVMvalueType *udt =
            fklCreateVMvalueType(vm, NULL, &g_ud_token, &TestUdMt);
    FklVMvalue *ud = fklCreateVMvalueUd(vm, udt);

    /* --- fklInitVMframe: compound frame fields --- */
    {
        reset_vm(vm);
        FklVMframe *f = fklPrepCall(vm, 1, 0, NULL);
        vm->bp = 1;
        FklVMframe *r = fklInitVMframe(vm, f, FKL_VM_PROC(proc));
        CHECK(r == f, "fklInitVMframe returns the frame");
        CHECK(f->type == FKL_FRAME_COMPOUND, "initVMframe marks COMPOUND");
        CHECK(f->proc == proc, "initVMframe stores the proc");
        CHECK(f->lcount == 2, "initVMframe stores the local count");
        CHECK(f->pc == FKL_VM_PROC(proc)->spc, "initVMframe sets pc to spc");
        CHECK(f->spc == FKL_VM_PROC(proc)->spc, "initVMframe sets spc");
        CHECK(f->end == FKL_VM_PROC(proc)->end, "initVMframe sets end");
        CHECK(f->mark == FKL_VM_COMPOUND_FRAME_MARK_RET,
                "initVMframe resets the mark to RET");
        reset_vm(vm);
    }

    /* --- fklVMframeSetBp / fklVMframeSetSp --- */
    {
        reset_vm(vm);
        FklVMframe *f = fklPrepCall(vm, 1, 0, NULL);
        vm->bp = 1;
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(0)); /* callee */
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(1)); /* arg 0 */
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(2)); /* arg 1 */

        int r = fklVMframeSetBp(vm, f, 4);
        uint32_t const callee_start = f->bp + FKL_VM_FRAME_SIZE;
        CHECK(r == 0, "fklVMframeSetBp succeeds");
        CHECK(f->bp == 1, "setBp stores exe->bp");
        CHECK(f->arg_num == 2, "setBp computes arg_num");
        CHECK(f->sp == callee_start + 1 + 4, "setBp computes sp");
        CHECK(vm->tp == f->sp, "setBp advances tp to sp");
        CHECK(vm->base[callee_start + 1] == FKL_MAKE_VM_FIX(1),
                "setBp keeps the existing argument");
        CHECK(vm->base[f->sp - 1] == NULL, "setBp zeroes the fresh locals");
        reset_vm(vm);
    }

    /* --- fklCallObj: compound proc --- */
    {
        reset_vm(vm);
        FklVMframe *f = fklSetBp(vm, 1);
        FKL_VM_PUSH_VALUE(vm, proc);
        int r = fklCallObj(vm, f, proc);
        CHECK(r == 0, "fklCallObj(proc) returns 0");
        CHECK(vm->top_frame == f, "fklCallObj(proc) pushes the frame");
        CHECK(f->type == FKL_FRAME_COMPOUND, "called proc frame is COMPOUND");
        CHECK(f->proc == proc, "called proc frame keeps the proc");
        reset_vm(vm);
    }

    /* --- fklCallObj: cproc --- */
    {
        reset_vm(vm);
        FklVMframe *f = fklSetBp(vm, 1);
        FKL_VM_PUSH_VALUE(vm, cproc);
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(1));
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX(2));
        int r = fklCallObj(vm, f, cproc);
        CHECK(r == 0, "fklCallObj(cproc) returns 0");
        CHECK(vm->top_frame == f, "fklCallObj(cproc) pushes the frame");
        CHECK(f->type == FKL_FRAME_OTHEROBJ, "called cproc frame is OTHEROBJ");
        FklCprocFrameContext *c = (FklCprocFrameContext *)f->data;
        CHECK(c->func == test_cfunc, "cproc frame context stores the function");
        CHECK(c->proc == cproc, "cproc frame context stores the proc");
        CHECK(FKL_CPROC_GET_ARG_NUM(vm, c) == 2,
                "FKL_CPROC_GET_ARG_NUM counts the arguments");
        reset_vm(vm);
    }

    /* --- fklCallObj: userdata with a call method --- */
    {
        reset_vm(vm);
        g_ud_calls = 0;
        FklVMframe *f = fklSetBp(vm, 1);
        int r = fklCallObj(vm, f, ud);
        CHECK(r == 0, "fklCallObj(userdata) returns 0");
        CHECK(g_ud_calls == 1, "the userdata call method was invoked once");
        CHECK(g_ud_call_value == ud, "the call method received the value");
        CHECK(g_ud_call_frame == f, "the call method received the frame");
        reset_vm(vm);
    }

    /* --- fklCallObjOrRaise success path --- */
    {
        reset_vm(vm);
        FklVMframe *f = fklSetBp(vm, 1);
        FKL_VM_PUSH_VALUE(vm, proc);
        fklCallObjOrRaise(vm, f, proc);
        CHECK(vm->top_frame == f && f->type == FKL_FRAME_COMPOUND,
                "fklCallObjOrRaise calls the proc without raising");
        reset_vm(vm);
    }

    /* --- fklPrepCall links a new frame to the previous top --- */
    {
        reset_vm(vm);
        FklVMframe *f0 = fklSetBp(vm, 1);
        FKL_VM_PUSH_VALUE(vm, proc);
        setup_call(vm, f0, proc); /* top = f0 */
        CHECK(vm->top_frame == f0, "setup: f0 is on top");

        FklVMframe *f1 = fklSetBp(vm, 1); /* a new frame above f0 */
        CHECK(f1->prev == f0,
                "a new prep-call frame links prev to the old top");
        reset_vm(vm);
    }

    /* --- explicit-frame tail call: reuse the given frame --- */
    {
        reset_vm(vm);
        FklVMframe *f0 = fklSetBp(vm, 1);
        FKL_VM_PUSH_VALUE(vm, proc);
        fklCallObjOrRaise(vm, f0, proc);
        FklVMframe *saved_prev = f0->prev;

        FklVMvalue *proc2 = make_proc(vm, 3);
        fklCallObjOrRaise(vm, vm->top_frame, proc2);
        CHECK(vm->top_frame == f0, "explicit-frame tail call reuses the frame");
        CHECK(f0->proc == proc2, "the reused frame runs the new proc");
        CHECK(f0->lcount == 3,
                "the reused frame adopts the new local count");
        CHECK(f0->prev == saved_prev, "the reused frame keeps its prev");
        reset_vm(vm);
    }

    /* --- fklSetBpAt stores the caller-provided prev_bp --- */
    {
        reset_vm(vm);
        uint32_t const at = 4;
        uint32_t const prev_bp = 3;
        vm->bp = 20; /* at < exe->bp: exe->bp must not be used as prev_bp */
        FklVMvalue *arg0 = FKL_MAKE_VM_FIX(7);
        FklVMframe *f = fklSetBpAt(vm, at, prev_bp, 1, 1, &arg0);
        CHECK(f == FKL_SLOT_TO_FRAME(&vm->base[at + 1]),
                "setBpAt embeds the frame at at+1");
        CHECK(FKL_GET_FIX(vm->base[at]) == (int64_t)prev_bp,
                "setBpAt stores the explicit prev_bp (not exe->bp)");
        CHECK(vm->bp == at + 1, "setBpAt sets exe->bp to at+1");
        f->bp = at + 1;
        CHECK(FKL_GET_FIX(FKL_VM_GET_ARG(vm, f, FKL_VM_BP_IDX))
                        == (int64_t)prev_bp,
                "the stored prev_bp is reachable via FKL_VM_BP_IDX");
        CHECK(vm->base[at + 1 + FKL_VM_FRAME_SIZE] == arg0,
                "setBpAt places the argument after the frame");
        reset_vm(vm);
    }

    /* --- fklSetBpAt without prep only stores the prev_bp marker --- */
    {
        reset_vm(vm);
        uint32_t const at = 6;
        uint32_t const prev_bp = 2;
        vm->bp = 30;
        FklVMframe *f = fklSetBpAt(vm, at, prev_bp, 0, 0, NULL);
        CHECK(f == NULL, "setBpAt without prep returns no frame");
        CHECK(FKL_GET_FIX(vm->base[at]) == (int64_t)prev_bp,
                "setBpAt without prep stores the explicit prev_bp");
        CHECK(vm->bp == at + 1, "setBpAt without prep sets bp to at+1");
        reset_vm(vm);
    }

    /* --- fklInitVMraiseErrFrame --- */
    {
        reset_vm(vm);
        FklVMframe *f = fklPrepCall(vm, 1, 0, NULL);
        FklVMvalue *err = FKL_MAKE_VM_FIX(7);
        FklVMframe *r = fklInitVMraiseErrFrame(vm, f, err);
        CHECK(r == f, "fklInitVMraiseErrFrame returns the frame");
        CHECK(f->type == FKL_FRAME_OTHEROBJ, "raise-error frame is OTHEROBJ");
        CHECK(vm->top_frame == f, "raise-error frame is pushed");
        reset_vm(vm);
    }

    /* --- fklUninitVMframe dispatches on the frame type --- */
    {
        reset_vm(vm);
        FklVMframe *fo = fklPrepCall(vm, 1, 0, NULL);
        fklInitVMframeExt(vm, fo, &TestFinalizerMt);
        g_other_frame_finalized = 0;
        fklUninitVMframe(vm, fo);
        CHECK(g_other_frame_finalized == 1,
                "fklUninitVMframe runs the other-obj finalizer");

        FklVMframe *fc = fklPrepCall(vm, 1, 0, NULL);
        fklInitVMframe(vm, fc, FKL_VM_PROC(proc));
        fc->ref = &g_ud_call_value; /* arbitrary non-NULL */
        fklUninitVMframe(vm, fc);
        CHECK(fc->lref == FKL_VM_NIL && fc->lrefl == FKL_VM_NIL
                        && fc->ref == NULL,
                "fklUninitVMframe finalizes a compound frame");
        reset_vm(vm);
    }

    fklDestroyAllVMs(vm);
    fklDestroyVMgc(gc);

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
