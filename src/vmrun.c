#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <fakeLisp/ins_helper.h>

#include <uv.h>

#include <inttypes.h>
#include <setjmp.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <process.h>
#include <tchar.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

static inline void
push_old_locv(FklVM *exe, uint32_t llast, FklVMvalue **locv) {
    if (llast) {
        FklVMlocvList *n = NULL;
        if (exe->old_locv_count < 8)
            n = &exe->old_locv_cache[exe->old_locv_count];
        else {
            n = (FklVMlocvList *)fklZmalloc(sizeof(FklVMlocvList));
            FKL_ASSERT(n);
        }
        n->next = exe->old_locv_list;
        n->llast = llast;
        n->locv = locv;
        exe->old_locv_list = n;
        exe->old_locv_count++;
    }
}

// call compound procedure
static inline void call_compound_procedure(FklVM *exe, FklVMvalueProc *proc) {
    FklVMframe *f = fklCreateVMframeWithProc(exe, FKL_VM_VAL(proc));
    fklVMframeSetBp(exe, f, proc->local_count);
    fklPushVMframe(f, exe);
}

void fklDBG_printLinkBacktrace(FklVMframe *t, FklCodeBuilder *fp, FklVM *exe) {
    if (t->type == FKL_FRAME_COMPOUND) {
        FklVMvalue *name = FKL_VM_PROC(t->proc)->name;

        if (name)
            fklPrintString2(FKL_VM_SYM(name), fp);
        else
            fklCodeBuilderPuts(fp, "<lambda>");
        fklCodeBuilderFmt(fp, "[%u]", t->mark);
    } else
        fklCodeBuilderPuts(fp, "<obj>");

    for (FklVMframe *cur = t->prev; cur; cur = cur->prev) {
        fklCodeBuilderPuts(fp, " --> ");
        if (cur->type == FKL_FRAME_COMPOUND) {
            FklVMvalue *name = FKL_VM_PROC(cur->proc)->name;
            if (name)
                fklPrintString2(FKL_VM_SYM(name), fp);
            else
                fklCodeBuilderPuts(fp, "<lambda>");
            fklCodeBuilderFmt(fp, "[%u]", cur->mark);
        } else
            fklCodeBuilderPuts(fp, "<obj>");
    }
    fklCodeBuilderPutc(fp, '\n');
}

void fklPrintCprocBacktrace(const char *name, FklCodeBuilder *build) {
    if (name) {
        fklCodeBuilderPuts(build, "cproc: ");
        fklPrintSymLiteral2(name, build);
    } else {
        fklCodeBuilderPuts(build, "<cproc>");
    }
}

static void
cproc_frame_print_backtrace(void *data, FklCodeBuilder *fp, FklVM *vm) {
    FklCprocFrameContext *c = (FklCprocFrameContext *)data;
    FklVMvalueCproc *cproc = FKL_VM_CPROC(c->proc);
    fklPrintCprocBacktrace(cproc->name, fp);
}

static void cproc_frame_atomic(void *data, FklVMgc *gc) {
    FklCprocFrameContext *c = (FklCprocFrameContext *)data;
    fklVMgcToGray(c->proc, gc);
}

static int cproc_frame_step(void *data, FklVM *exe) {
    FklCprocFrameContext *c = FKL_TYPE_CAST(FklCprocFrameContext *, data);
    return c->func(exe, c, FKL_CPROC_GET_ARG_NUM(exe, c));
}

static const FklVMframeContextMethodTable CprocContextMethodTable = {
    .atomic = cproc_frame_atomic,
    .print_backtrace = cproc_frame_print_backtrace,
    .step = cproc_frame_step,
};

static inline void
initCprocFrameContext(void *data, FklVMvalue *proc, FklVM *exe) {
    FklCprocFrameContext *c = (FklCprocFrameContext *)data;
    c->proc = proc;
    c->c[0].uptr = 0;
    c->func = FKL_VM_CPROC(proc)->func;
    c->pd = FKL_VM_CPROC(proc)->pd;
}

typedef struct ImportPostProcessContext {
    uint64_t epc;
    FklVM *exe;
} ImportPostProcessContext;

static inline void call_cproc(FklVM *exe, FklVMvalue *cproc) {
    FklVMframe *f = fklCreateOtherObjVMframe(exe, &CprocContextMethodTable);
    initCprocFrameContext(f->data, cproc, exe);
    fklPushVMframe(f, exe);
}

static inline void B_dummy(FklVM *exe, const FklIns *ins) {
    FklVMvalue *err = fklCreateVMvalueError(exe,
            exe->gc->builtinErrorTypeId[FKL_ERR_INVALIDACCESS],
            fklCreateVMvalueStr1(exe, "reach invalid opcode: `dummy`"));
    fklRaiseVMerror(err, exe);
}

static inline void insert_to_VM_chain(FklVM *cur, FklVM *prev, FklVM *next) {
    cur->prev = prev;
    cur->next = next;
    if (prev)
        prev->next = cur;
    if (next)
        next->prev = cur;
}

static inline FklVMvalue *
fetch_main_env_ref(FklVM *exe, uint32_t i, uint32_t cidx, FklVarRefDef *refs) {
    if (cidx < FKL_BUILTIN_SYMBOL_NUM)
        return exe->gc->builtin_refs[cidx];
    if (FKL_IS_VAR_REF(refs[i].is_local)) {
        FklVMvalue *ref = refs[i].is_local;
        refs[i].is_local = FKL_VM_NIL;
        return ref;
    }
    return fklCreateClosedVMvalueVarRef(exe, NULL);
}

static inline void init_builtin_symbol_ref(FklVM *exe, FklVMvalue *proc_obj) {
    FklVMvalueProc *proc = FKL_VM_PROC(proc_obj);
    FklVMvalueProto *pt = proc->proto;

    proc->ref_count = pt->ref_count;
    FklVMvalue **closure = proc->closure;

    FklVarRefDef *const refs = fklVMvalueProtoVarRefs(pt);

    for (uint32_t i = 0; i < proc->ref_count; ++i) {
        uint32_t cidx = FKL_GET_FIX(refs[i].cidx);
        closure[i] = fetch_main_env_ref(exe, i, cidx, refs);
    }
}

static inline void vm_stack_init(FklVM *exe) {
    exe->last = FKL_VM_STACK_INC_NUM;
    exe->tp = 0;
    exe->bp = 0;
    exe->base = fklAllocLocalVarSpaceFromGC(exe->gc, exe->last, &exe->last);
    FKL_ASSERT(exe->base);
}

FklVM *fklCreateVMwithByteCode(FklVMvalue *co,
        FklVMgc *gc,
        FklVMvalueProto *pt,
        uint64_t spc) {
    FklVM *exe = (FklVM *)fklZcalloc(1, sizeof(FklVM));
    FKL_ASSERT(exe);
    FKL_ASSERT(fklIsVMvalueProto(FKL_TYPE_CAST(FklVMvalue *, pt)));
    exe->prev = exe;
    exe->next = exe;
    exe->gc = gc;
    exe->frame_cache_head = &exe->inplace_frame;
    exe->frame_cache_tail = &exe->frame_cache_head->prev;
    vm_stack_init(exe);
    if (co != NULL) {
        FklByteCodelnt *bcl = FKL_VM_CO(co);
        FklVMvalue *proc = fklCreateVMvalueProc2(exe,
                bcl->bc.code + spc,
                bcl->bc.len - spc,
                co,
                pt);
        init_builtin_symbol_ref(exe, proc);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, proc);
        fklCallObj(exe, proc);
    }
    exe->state = FKL_VM_READY;
    exe->dummy_ins_func = B_dummy;
    uv_mutex_init(&exe->lock);
    return exe;
}

FklVM *fklCreateVMwithByteCode2(FklVMvalue *co,
        FklVMgc *gc,
        FklVMvalueProto *pt,
        uint64_t spc) {
    FklVM *exe = (FklVM *)fklZcalloc(1, sizeof(FklVM));
    FKL_ASSERT(exe);
    FKL_ASSERT(fklIsVMvalueProto(FKL_TYPE_CAST(FklVMvalue *, pt)));
    exe->prev = exe;
    exe->next = exe;
    exe->gc = gc;
    exe->frame_cache_head = &exe->inplace_frame;
    exe->frame_cache_tail = &exe->frame_cache_head->prev;
    vm_stack_init(exe);
    if (co != NULL) {
        FklByteCodelnt *bcl = FKL_VM_CO(co);
        FklVMvalue *proc = fklCreateVMvalueProc2(exe,
                bcl->bc.code + spc,
                bcl->bc.len - spc,
                co,
                pt);
        init_builtin_symbol_ref(exe, proc);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, proc);
        fklCallObj(exe, proc);
    }
    exe->state = FKL_VM_READY;
    exe->dummy_ins_func = B_dummy;
    uv_mutex_init(&exe->lock);
    return exe;
}

static inline void do_finalize_obj_frame(FklVM *vm, FklVMframe *f) {
    if (f->t->finalizer)
        f->t->finalizer((void *)f->data);
    f->prev = NULL;
    *(vm->frame_cache_tail) = f;
    vm->frame_cache_tail = &f->prev;
}

static inline void close_var_ref(FklVMvalue *ref) {
    FKL_VM_VAR_REF(ref)->v = *FKL_VM_VAR_REF_GET(ref);
    atomic_store(&(FKL_VM_VAR_REF(ref)->ref), &(FKL_VM_VAR_REF(ref)->v));
}

void fklCloseVMvalueVarRef(FklVMvalue *ref) { close_var_ref(ref); }

int fklIsClosedVMvalueVarRef(FklVMvalue *ref) {
    FKL_ASSERT(FKL_IS_VAR_REF(ref));
    FklVMvalue **p = FKL_VM_VAR_REF_GET(ref);
    return p == &FKL_VM_VAR_REF(ref)->v;
}

static inline void close_all_var_ref(FklVMframe *f) {
    for (FklVMvalue *l = f->lrefl; FKL_IS_PAIR(l); l = FKL_VM_CDR(l)) {
        close_var_ref(FKL_VM_CAR(l));
    }
    f->lrefl = FKL_VM_NIL;
}

static inline void do_finalize_compound_frame(FklVM *exe, FklVMframe *f) {
    close_all_var_ref(f);

    f->lref = FKL_VM_NIL;
    f->lrefl = FKL_VM_NIL;
    f->ref = NULL;

    f->prev = NULL;
    *(exe->frame_cache_tail) = f;
    exe->frame_cache_tail = &f->prev;
}

void fklDestroyVMframe(FklVMframe *frame, FklVM *exe) {
    if (frame->type == FKL_FRAME_OTHEROBJ)
        do_finalize_obj_frame(exe, frame);
    else
        do_finalize_compound_frame(exe, frame);
}

typedef struct {
    FklVMvalue *error;
} RaiseErrorFrameContext;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(RaiseErrorFrameContext);

static void raise_error_frame_atomic(void *data, FklVMgc *gc) {
    RaiseErrorFrameContext *c = (RaiseErrorFrameContext *)data;
    fklVMgcToGray(c->error, gc);
}

static void
raise_error_frame_print_backtrace(void *data, FklCodeBuilder *fp, FklVM *vm) {
    FKL_UNREACHABLE();
    return;
}

static int raise_error_frame_step(void *data, FklVM *exe) {
    RaiseErrorFrameContext *c = FKL_TYPE_CAST(RaiseErrorFrameContext *, data);
    fklPopVMframe(exe);
    fklRaiseVMerror(c->error, exe);
    return 1;
}

static const FklVMframeContextMethodTable RaiseErrorContextMethodTable = {
    .atomic = raise_error_frame_atomic,
    .print_backtrace = raise_error_frame_print_backtrace,
    .step = raise_error_frame_step,
};

void fklPushVMraiseErrorFrame(FklVM *exe, FklVMvalue *err) {
    FklVMframe *f =
            fklCreateOtherObjVMframe(exe, &RaiseErrorContextMethodTable);
    RaiseErrorFrameContext *ctx =
            FKL_TYPE_CAST(RaiseErrorFrameContext *, f->data);
    ctx->error = err;
    fklPushVMframe(f, exe);
}

void fklPushVMframe(FklVMframe *f, FklVM *exe) {
    f->prev = exe->top_frame;
    exe->top_frame = f;
}

static inline FklVMframe *popFrame(FklVM *exe) {
    FklVMframe *frame = exe->top_frame;
    exe->top_frame = frame->prev;
    return frame;
}

void fklPopVMframe(FklVM *exe) {
    FklVMframe *f = popFrame(exe);
    switch (f->type) {
    case FKL_FRAME_COMPOUND:
        do_finalize_compound_frame(exe, f);
        break;
    case FKL_FRAME_OTHEROBJ:
        do_finalize_obj_frame(exe, f);
        break;
    }
}

void fklPopVMframe2(FklVM *exe, FklVMframe *const bottom) {
    FKL_ASSERT(exe->top_frame != NULL || bottom == NULL);
    while (exe->top_frame != bottom) {
        fklPopVMframe(exe);
    }
}

#define CALL_CALLABLE_OBJ(EXE, V)                                              \
    case FKL_TYPE_CPROC:                                                       \
        call_cproc(EXE, V);                                                    \
        break;                                                                 \
    case FKL_TYPE_USERDATA:                                                    \
        FKL_VM_UD(V)->t->__call(V, EXE);                                       \
        break;                                                                 \
    default:                                                                   \
        break;

void fklCallObj(FklVM *exe, FklVMvalue *proc) {
    switch (proc->type_) {
    case FKL_TYPE_PROC:
        call_compound_procedure(exe, FKL_VM_PROC(proc));
        break;
    case FKL_TYPE_CPROC:
        call_cproc(exe, proc);
        break;
    case FKL_TYPE_USERDATA:
        FKL_VM_UD(proc)->mt_->call(proc, exe);
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
}

void fklTailCallObj(FklVM *exe, FklVMvalue *proc) {
    FklVMframe *frame = exe->top_frame;
    if (frame->type == FKL_FRAME_OTHEROBJ) {
        exe->top_frame = frame->prev;
        do_finalize_obj_frame(exe, frame);
    }
    fklCallObj(exe, proc);
}

struct DefaultCbValueCreator {
    size_t count;
    FklVMvalue **values;
};

static void default_callback_value_creator(FklVM *exe, void *arg) {
    struct DefaultCbValueCreator *a =
            FKL_TYPE_CAST(struct DefaultCbValueCreator *, arg);

    size_t count = a->count;
    FklVMvalue **cur = a->values;
    FklVMvalue **const end = &cur[count];

    for (; cur < end; ++cur)
        FKL_VM_PUSH_VALUE(exe, *cur);
}

FklVMcallResult fklVMcall3(FklRunVMcb cb,
        FklVM *exe,
        FklVMrecoverArgs *re,
        FklVMvalue *proc,
        FklVMcallbackValueCreator creator,
        void *args) {
    FKL_ASSERT(fklIsCallable(proc));
    fklVMsetRecover(exe, re);

    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, proc);

    if (creator)
        creator(exe, args);
    fklCallObj(exe, proc);

    FklVMcallResult result;

    result.err = cb(exe, re->exit_frame);
    result.v = FKL_VM_GET_TOP_VALUE(exe);

    return result;
}

FklVMcallResult fklVMcall2(FklRunVMcb cb,
        FklVM *exe,
        FklVMrecoverArgs *re,
        FklVMvalue *proc,
        size_t count,
        FklVMvalue *values[]) {

    struct DefaultCbValueCreator args = {
        .count = count,
        .values = values,
    };

    return fklVMcall3(cb, exe, re, proc, default_callback_value_creator, &args);
}

FklVMcallResult fklVMcall(FklVM *exe,
        FklVMrecoverArgs *re,
        FklVMvalue *proc,
        size_t count,
        FklVMvalue *values[]) {
    return fklVMcall2(fklRunVM, exe, re, proc, count, values);
}

static inline void set_recover(FklVMrecoverArgs *re,
        uint32_t bp,
        uint32_t tp,
        FklVMframe *const exit_frame) {
    re->bp = bp;
    re->tp = tp;
    re->exit_frame = exit_frame;
}

FklVMcallResult fklVMcall0(FklRunVMcb cb, FklVM *exe, FklVMrecoverArgs *re) {
    FklVMvalue *callee = FKL_VM_GET_ARG(exe, exe, -1);
    FKL_ASSERT(fklIsCallable(callee));
    set_recover(re,
            FKL_GET_FIX(FKL_VM_GET_ARG(exe, exe, -2)),
            re->bp - 1,
            exe->top_frame);

    fklCallObj(exe, callee);

    FklVMcallResult result;

    result.err = cb(exe, re->exit_frame);
    result.v = FKL_VM_GET_TOP_VALUE(exe);

    return result;
}

void fklVMsetRecover(struct FklVM *exe, FklVMrecoverArgs *re) {
    set_recover(re, exe->bp, exe->tp, exe->top_frame);
}

void fklVMrecover(struct FklVM *vm, const FklVMrecoverArgs *args) {
    FklVMframe *f = vm->top_frame;
    while (f != args->exit_frame) {
        FklVMframe *cur = f;
        f = f->prev;
        fklDestroyVMframe(cur, vm);
    }
    vm->tp = args->tp;
    vm->bp = args->bp;
    vm->top_frame = args->exit_frame;
}

void fklVMsetTpAndPushValue(FklVM *exe, uint32_t rtp, FklVMvalue *retval) {
    exe->tp = rtp + 1;
    exe->base[rtp] = retval;
}

void fklLockThread(FklVM *exe) {
    if (exe->is_single_thread)
        return;
    uv_mutex_lock(&exe->lock);
}

void fklSetThreadReadyToExit(FklVM *exe) { exe->state = FKL_VM_EXIT; }

void fklUnlockThread(FklVM *exe) {
    if (exe->is_single_thread)
        return;
    uv_mutex_unlock(&exe->lock);
}

static inline void do_vm_atexit(FklVM *vm) {
    vm->state = FKL_VM_READY;
    for (struct FklVMatExit *cur = vm->atexit; cur; cur = cur->next)
        cur->func(vm, cur->arg);
    vm->state = FKL_VM_EXIT;
}

static inline void switch_notice_lock_ins(FklVM *exe) {
    if (atomic_load(&exe->notice_lock))
        return;
    atomic_store(&exe->notice_lock, 1);
}

static inline void switch_un_notice_lock_ins(FklVM *exe) {
    if (atomic_load(&exe->notice_lock))
        atomic_store(&exe->notice_lock, 0);
}

int fklRunVM2(FklVM *exe, FklVMframe *const exit_frame) {
    int is_single_thread = exe->is_single_thread;

    exe->is_single_thread = 1;

    int r = fklRunVM(exe, exit_frame);

    exe->is_single_thread = is_single_thread;

    return r;
}

#define DROP_TOP(S) --(S)->tp
#define PROCESS_ADD_RES()                                                      \
    FKL_VM_PUSH_VALUE(exe, fklProcessVMnumAddResult(exe, r64, rd, &bi))
#define PROCESS_ADD(VAR)                                                       \
    if (fklProcessVMnumAdd(VAR, &r64, &rd, &bi))                               \
    FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe)
#define PROCESS_SUB_RES()                                                      \
    FKL_VM_PUSH_VALUE(exe, fklProcessVMnumSubResult(exe, a, r64, rd, &bi))

#define PROCESS_MUL(VAR)                                                       \
    if (fklProcessVMnumMul(VAR, &r64, &rd, &bi))                               \
    FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe)

#define PROCESS_MUL_RES()                                                      \
    FKL_VM_PUSH_VALUE(exe, fklProcessVMnumMulResult(exe, r64, rd, &bi))

#define PROCESS_DIV_RES()                                                      \
    if (r64 == 0 || (bi.num == 0 && bi.digits != NULL)) {                      \
        fklUninitBigInt(&bi);                                                  \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_DIVZEROERROR, exe);                    \
    }                                                                          \
    FKL_VM_PUSH_VALUE(exe, fklProcessVMnumDivResult(exe, a, r64, rd, &bi))

#define PROCESS_IMUL(VAR)                                                      \
    if (fklProcessVMintMul(VAR, &r64, &bi))                                    \
    FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe)

#define PROCESS_IDIV_RES()                                                     \
    if (r64 == 0 || (bi.num == 0 && bi.digits != NULL)) {                      \
        fklUninitBigInt(&bi);                                                  \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_DIVZEROERROR, exe);                    \
    }                                                                          \
    FKL_VM_PUSH_VALUE(exe, fklProcessVMnumIdivResult(exe, a, r64, &bi))

static inline FklVMvalue *
get_var_val(FklVMframe *f, uint32_t idx, FklVMvalue **psid) {
    FklVMvalue *v = idx < f->rcount ? *FKL_VM_VAR_REF_GET(f->ref[idx]) : NULL;
    if (v)
        return v;

    FklVMvalueProc *proc = FKL_VM_PROC(f->proc);
    const FklVarRefDef *const refs = fklVMvalueProtoVarRefs(proc->proto);
    FKL_ASSERT(refs[idx].sid);
    *psid = refs[idx].sid;
    return NULL;
}

static inline FklVMvalue *volatile *
get_var_ref(FklVMframe *f, uint32_t idx, FklVMvalue **psid) {
    FklVMvalue *volatile *v =
            (idx >= f->rcount || !(FKL_VM_VAR_REF(f->ref[idx])->ref))
                    ? NULL
                    : FKL_VM_VAR_REF_GET(f->ref[idx]);
    if (v && *v)
        return v;
    FklVMvalueProc *proc = FKL_VM_PROC(f->proc);
    const FklVarRefDef *const refs = fklVMvalueProtoVarRefs(proc->proto);
    FKL_ASSERT(refs[idx].sid);
    *psid = refs[idx].sid;
    return NULL;
}

static inline FklImportDllInitFunc getImportInit(uv_lib_t *handle) {
    return (FklImportDllInitFunc)fklGetAddress("_fklImportInit", handle);
}

static inline void close_var_ref_from(FklVMframe *f, uint32_t start) {
    if (!FKL_IS_VECTOR(f->lref))
        return;
    FklVMvalueVec *lref = FKL_VM_VEC(f->lref);

    FklVMvalue **pcdr = &f->lrefl;
    while (FKL_IS_PAIR(*pcdr)) {
        FklVMvalue *ref_v = FKL_VM_CAR(*pcdr);
        FklVMvalueVarRef *ref = FKL_VM_VAR_REF(ref_v);
        if (ref->idx >= start) {
            close_var_ref(ref_v);
            lref->base[ref->idx] = NULL;
            *pcdr = FKL_VM_CDR(*pcdr);
        } else {
            pcdr = &FKL_VM_CDR(*pcdr);
        }
    }
}

#define uX(ins, frame)                                                         \
    (uB(ins) | (((uint32_t)uB(*(frame->pc++))) << FKL_I16_WIDTH))
#define sX(ins, frame) ((int32_t)uX(ins, frame))
#define uXX(ins, frame)                                                        \
    (frame->pc += 2,                                                           \
            uC(ins) | (((uint64_t)uC(frame->pc[-2])) << FKL_I24_WIDTH)         \
                    | (((uint64_t)uB(frame->pc[-1])) << (FKL_I24_WIDTH * 2)))
#define sXX(ins, frame) ((int64_t)uXX(ins, frame))

static FKL_ALWAYS_INLINE int check_unbound_exported_symbol(FklVM *exe,
        FklVMvalueLib *l) {
    FklVMvalue *const *values = l->values;
    FklVMvalue *name = NULL;
    for (size_t i = 0; i < l->count; ++i) {
        if (values[i] == NULL) {
            name = fklVMvalueLibNames(l)[i];
            break;
        }
    }

    if (name != NULL) {
        FklVMvalue *err = FKL_MAKE_VM_ERR(FKL_ERR_EXPORT_ERROR,
                exe,
                "The symbol %S is not exported in module %S",
                name,
                l->name);
        FKL_VM_PUSH_VALUE(exe, err);
        return 1;
    }

    return 0;
}

static inline void load_lib(FklVM *exe, FklVMvalueLib *l) {
    atomic_store(&l->import_state, FKL_VM_LIB_IMPORTING);
    FklVMrecoverArgs re = { 0 };
    fklVMsetRecover(exe, &re);

    FklVMframe *exit_frame = exe->top_frame;
    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, FKL_VM_VAL(l));
    call_compound_procedure(exe, FKL_VM_PROC(l->proc));

    int r = fklRunVM(exe, exit_frame);

    r = r || check_unbound_exported_symbol(exe, l);
    int import_state = r ? FKL_VM_LIB_ERROR : FKL_VM_LIB_IMPORTED;
    atomic_store(&l->import_state, import_state);
    fklUnlockVMlib(l);

    if (r) {
        fklRaiseVMerror(FKL_VM_GET_TOP_VALUE(exe), exe);
    }

    fklVMrecover(exe, &re);
}

static inline void load_dll(FklVM *exe, FklVMvalueLib *l) {
    atomic_store(&l->import_state, FKL_VM_LIB_IMPORTING);
    FklVMvalue *error_msg = NULL;
    FklVMvalue *dll;
    FklImportDllInitFunc initFunc = NULL;
    if (FKL_IS_STR(l->proc)) {
        dll = fklCreateVMvalueDll(exe, l->proc, &error_msg);
        fklInitVMdll(dll, exe);
    } else {
        dll = l->proc;
    }

    if (dll)
        initFunc = getImportInit(&(FKL_VM_DLL(dll)->dll));
    else {
        atomic_store(&l->import_state, FKL_VM_LIB_ERROR);
        fklUnlockVMlib(l);
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_IMPORTFAILED,
                exe,
                FKL_VM_STR(error_msg)->str);
    }

    if (!initFunc) {
        atomic_store(&l->import_state, FKL_VM_LIB_ERROR);
        fklUnlockVMlib(l);
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_IMPORTFAILED,
                exe,
                "Failed to import dll: %s",
                l->proc);
    }

    uint32_t tp = exe->tp;
    l->proc = dll;
    initFunc(exe, dll, l->count, l->values);
    exe->tp = tp;
    int r = check_unbound_exported_symbol(exe, l);
    int import_state = r ? FKL_VM_LIB_ERROR : FKL_VM_LIB_IMPORTED;
    atomic_store(&l->import_state, import_state);
    fklUnlockVMlib(l);

    if (r) {
        fklRaiseVMerror(FKL_VM_GET_TOP_VALUE(exe), exe);
    }
}

static inline void execute_compound_frame(FklVM *exe, FklVMframe *frame) {
    FklIns ins = FKL_INS_STATIC_INIT;
    FklVMvalueLib *plib;
    uint32_t idx;
    uint64_t size;
    int64_t offset;
start:
    ins = *(frame->pc++);
    switch (FKL_INS_OP(ins)) {
#define DISPATCH_INCLUDED
#define DISPATCH_SWITCH
#include "vmdispatch.h"
#undef DISPATCH_INCLUDED
#undef DISPATCH_SWITCH
    }
    goto start;
}

#define DISPATCH_INCLUDED
#include "vmdispatch.h"
#undef DISPATCH_INCLUDED

#define RETURN_INCLUDE
#include "vmreturn.h"
#undef RETURN_INCLUDE

static FKL_ALWAYS_INLINE int do_callable_obj_frame_step(FklVMframe *f,
        struct FklVM *exe) {
    return f->t->step((void *)f->data, exe);
}

void fklMoveThreadObjectsToGc(FklVM *vm, FklVMgc *gc) {
    if (vm->obj_head) {
        vm->obj_tail->next_ = gc->head;
        gc->head = vm->obj_head;
        vm->obj_head = NULL;
        vm->obj_tail = NULL;
    }
}

static inline void remove_thread_frame_cache(FklVM *exe) {
    FklVMframe **phead = &exe->frame_cache_head;
    while (*phead) {
        FklVMframe *prev = *phead;
        if (prev == &exe->inplace_frame)
            phead = &prev->prev;
        else {
            *phead = prev->prev;
            fklZfree(prev);
        }
    }
    exe->frame_cache_tail = exe->frame_cache_head ? &exe->frame_cache_head->prev
                                                  : &exe->frame_cache_head;
}

void fklVMgcCheck(FklVM *exe, int forced) {
    FklVMgc *gc = exe->gc;
    if (forced || atomic_load(&gc->alloced_size) > gc->threshold) {
        fklMoveThreadObjectsToGc(&gc->gcvm, gc);
        fklMoveThreadObjectsToGc(exe, gc);
        fklVMgcMoveLocvCache(exe, gc);
        remove_thread_frame_cache(exe);
        for (FklVM *cur = exe->next; cur != exe; cur = cur->next) {
            fklMoveThreadObjectsToGc(cur, gc);
            fklVMgcMoveLocvCache(cur, gc);
            remove_thread_frame_cache(cur);
        }
        fklVMgcMarkAllRootToGray(exe);
        while (!fklVMgcPropagate(gc))
            ;
        fklVMgcUpdateWeakRefs(gc);
        FklVMvalue *white = NULL;
        fklVMgcCollect(gc, &white);
        fklVMgcSweep(gc, white);
        fklVMgcUpdateThreshold(gc);
        if (exe == &gc->gcvm)
            return;

        fklVMstackShrink(exe);
    }
}

int fklRunVM(FklVM *exe, FklVMframe *const exit_frame) {
    FKL_ASSERT(exe != &exe->gc->gcvm);

    jmp_buf *volatile prev_buf = exe->buf;
    jmp_buf buf;
    int r = 0;

    if (exe->top_frame == exit_frame) {
        goto done;
    }

    exe->state = FKL_VM_READY;
    for (;;) {
        switch (exe->state) {
        case FKL_VM_RUNNING: {
            FklVMframe *curframe = exe->top_frame;
            switch (curframe->type) {
            case FKL_FRAME_COMPOUND:
                execute_compound_frame(exe, curframe);
                break;
            case FKL_FRAME_OTHEROBJ:
                if (do_callable_obj_frame_step(curframe, exe))
                    continue;

                if (curframe->ret_cb && curframe->ret_cb(exe, curframe))
                    continue;

                do_finalize_obj_frame(exe, popFrame(exe));
                break;
            }

            fklVMgcCheckPoint(exe, 0);

            if (exe->top_frame == exit_frame)
                goto done;
        } break;
        case FKL_VM_EXIT:
            exe->buf = NULL;
            return 0;
            break;
        case FKL_VM_READY:
            exe->buf = &buf;
            if (setjmp(buf) == FKL_VM_ERR_RAISE) {
                FklVMvalue *ev = FKL_VM_POP_TOP_VALUE(exe);
                FklVMframe *frame =
                        fklIsVMvalueError(ev) ? exe->top_frame : exit_frame;
                for (; frame != exit_frame; frame = frame->prev)
                    if (frame->errorCallBack != NULL
                            && frame->errorCallBack(frame, ev, exe))
                        break;
                if (frame == exit_frame) {
                    if (fklVMinterrupt(exe, ev, &ev) == FKL_INT_DONE)
                        continue;
                    FKL_VM_PUSH_VALUE(exe, ev);
                    r = 1;
                    goto done;
                }
            }
            exe->state = FKL_VM_RUNNING;
            continue;
            break;
        case FKL_VM_WAITING:
            fklVMyield(exe);
            continue;
            break;
        }
    }
done:
    exe->state = FKL_VM_READY;
    exe->buf = prev_buf;
    return r;
}

static void vm_thread_cb(void *arg) {
    FklVM *volatile exe = FKL_TYPE_CAST(FklVM *, arg);

    FKL_VM_LOCK_BLOCK(exe, flag) {

        int r = fklRunVM(exe, NULL);

        exe->state = FKL_VM_EXIT;
        exe->buf = NULL;
        if (r) {
            FklVMvalue *ev = FKL_VM_POP_TOP_VALUE(exe);
            fklPrintErrBacktrace(ev, exe, NULL);
            if (exe->chan) {
                fklChanlSend(FKL_VM_CHANL(exe->chan), ev, exe);
                exe->chan = NULL;
            } else {
                exe->gc->exit_code = 255;
            }
        }

        do_vm_atexit(exe);
        if (exe->chan) {
            FklVMvalue *v = FKL_VM_GET_TOP_VALUE(exe);
            FklVMvalue *resultBox = fklCreateVMvalueBox(exe, v);
            fklChanlSend(FKL_VM_CHANL(exe->chan), resultBox, exe);
            exe->chan = NULL;
        }

        fklDeleteCallChain(exe);

        atomic_fetch_sub(&exe->gc->q.running_count, 1);
    }
}

void fklVMthreadStart(FklVM *exe, FklVMqueue *q) {
    uv_mutex_lock(&q->pre_running_lock);
    fklThreadQueuePush2(&q->pre_running_q, exe);
    uv_mutex_unlock(&q->pre_running_lock);
}

static inline void destroy_vm_atexit(FklVM *vm) {
    struct FklVMatExit *cur = vm->atexit;
    vm->atexit = NULL;
    while (cur) {
        struct FklVMatExit *next = cur->next;
        if (cur->finalizer)
            cur->finalizer(cur->arg);
        fklZfree(cur);
        cur = next;
    }
}

static inline void destroy_vm_interrupt_handler(FklVM *vm) {
    struct FklVMinterruptHandleList *cur = vm->int_list;
    vm->int_list = NULL;
    while (cur) {
        struct FklVMinterruptHandleList *next = cur->next;
        fklZfree(cur);
        cur = next;
    }
}

static inline void vm_stack_uninit(FklVM *s) {
    fklVMgcAddLocvCache(s->gc, s->last, s->base);
    s->base = NULL;
    s->last = 0;
    s->tp = 0;
    s->bp = 0;
}

static inline void remove_exited_thread_common(FklVM *cur) {
    fklDeleteCallChain(cur);
    vm_stack_uninit(cur);
    destroy_vm_atexit(cur);
    destroy_vm_interrupt_handler(cur);
    uv_mutex_destroy(&cur->lock);
    remove_thread_frame_cache(cur);
    fklZfree(cur);
}

static inline void remove_exited_thread(FklVMgc *gc) {
    FklVM *main = gc->main_thread;
    FklVM *cur = main->next;
    while (cur != main) {
        if (cur->state == FKL_VM_EXIT) {
            FklVM *prev = cur->prev;
            FklVM *next = cur->next;
            prev->next = next;
            next->prev = prev;

            remove_exited_thread_common(cur);
            cur = next;
        } else
            cur = cur->next;
    }
}

static inline void switch_notice_lock_ins_for_running_threads(
        FklThreadQueue *q) {
    for (FklThreadQueueNode *n = q->head; n; n = n->next)
        switch_notice_lock_ins(n->data);
}

static inline void switch_un_notice_lock_ins_for_running_threads(
        FklThreadQueue *q) {
    for (FklThreadQueueNode *n = q->head; n; n = n->next)
        switch_un_notice_lock_ins(n->data);
}

static inline void lock_all_vm(FklThreadQueue *q) {
    FklThreadQueue locked_queue;
    fklThreadQueueInit(&locked_queue);

    for (FklThreadQueueNode *n = fklThreadQueuePopNode(q); n;
            n = fklThreadQueuePopNode(q)) {
        if (uv_mutex_trylock(&((FklVM *)(n->data))->lock))
            fklThreadQueuePushNode(q, n);
        else
            fklThreadQueuePushNode(&locked_queue, n);
    }

    *q = locked_queue;
}

static inline void unlock_all_vm(FklThreadQueue *q) {
    for (FklThreadQueueNode *n = q->head; n; n = n->next)
        uv_mutex_unlock(&((FklVM *)(n->data))->lock);
}

static inline void move_all_thread_objects_to_gc(FklVMgc *gc) {
    FklVM *vm = gc->main_thread;
    fklMoveThreadObjectsToGc(vm, gc);
    fklVMgcMoveLocvCache(vm, gc);
    remove_thread_frame_cache(vm);
    for (FklVM *cur = vm->next; cur != vm; cur = cur->next) {
        fklMoveThreadObjectsToGc(cur, gc);
        fklVMgcMoveLocvCache(cur, gc);
        remove_thread_frame_cache(cur);
    }
}

static inline void shrink_locv_and_stack(FklThreadQueue *q) {
    for (FklThreadQueueNode *n = q->head; n; n = n->next) {
        FklVM *exe = n->data;
        fklVMstackShrink(exe);
    }
}

static inline void push_idle_work(FklVMgc *gc, struct FklVMidleWork *w) {
    *(gc->workq.tail) = w;
    gc->workq.tail = &w->next;
}

void fklQueueWorkInIdleThread(FklVM *vm,
        void (*cb)(FklVM *exe, void *),
        void *arg) {
    if (vm->is_single_thread) {
        cb(vm, arg);
        return;
    }
    FklVMgc *gc = vm->gc;
    struct FklVMidleWork work = {
        .vm = vm,
        .arg = arg,
        .cb = cb,
    };
    if (uv_cond_init(&work.cond)) {
        FKL_UNREACHABLE();
    }

    FKL_VM_UNLOCK_BLOCK(vm, flag) {
        uv_mutex_lock(&gc->workq_lock);

        push_idle_work(gc, &work);

        atomic_fetch_add(&gc->work_num, 1);

        uv_cond_wait(&work.cond, &gc->workq_lock);
        uv_mutex_unlock(&gc->workq_lock);
    }
    uv_cond_destroy(&work.cond);
}

static inline struct FklVMidleWork *pop_idle_work(FklVMgc *gc) {
    struct FklVMidleWork *r = gc->workq.head;
    if (r) {
        gc->workq.head = r->next;
        if (r->next == NULL)
            gc->workq.tail = &gc->workq.head;
    }
    return r;
}

void fklVMidleLoop(FklVMgc *gc) {
    FklVMqueue *q = &gc->q;
    fklMoveThreadObjectsToGc(&gc->gcvm, gc);
    for (;;) {
        if (atomic_load(&gc->alloced_size) > gc->threshold) {
            switch_notice_lock_ins_for_running_threads(&q->running_q);

            fklVMgcStateSet(gc, FKL_GC_STW);
            lock_all_vm(&q->running_q);

            move_all_thread_objects_to_gc(gc);

            fklVMgcStateSet(gc, FKL_GC_MARK_ROOT);
            FklVM *exe = gc->main_thread;
            fklVMgcMarkAllRootToGray(exe);

            fklVMgcStateSet(gc, FKL_GC_PROPAGATE);
            while (!fklVMgcPropagate(gc))
                ;
            fklVMgcUpdateWeakRefs(gc);
            FklVMvalue *white = NULL;
            fklVMgcStateSet(gc, FKL_GC_COLLECT);
            fklVMgcCollect(gc, &white);

            fklVMgcStateSet(gc, FKL_GC_SWEEPING);
            fklVMgcSweep(gc, white);
            fklVMgcUpdateThreshold(gc);

            fklVMgcStateSet(gc, FKL_GC_DONE);
            FklThreadQueue other_running_q;
            fklThreadQueueInit(&other_running_q);

            for (FklThreadQueueNode *n = fklThreadQueuePopNode(&q->running_q);
                    n;
                    n = fklThreadQueuePopNode(&q->running_q)) {
                FklVM *exe = n->data;
                if (exe->state == FKL_VM_EXIT) {
                    uv_thread_join(&exe->tid);
                    uv_mutex_unlock(&exe->lock);
                    fklThreadQueueDestroyNode(n);
                } else
                    fklThreadQueuePushNode(&other_running_q, n);
            }

            for (FklThreadQueueNode *n =
                            fklThreadQueuePopNode(&other_running_q);
                    n;
                    n = fklThreadQueuePopNode(&other_running_q))
                fklThreadQueuePushNode(&q->running_q, n);

            remove_exited_thread(gc);
            shrink_locv_and_stack(&q->running_q);

            switch_un_notice_lock_ins_for_running_threads(&q->running_q);
            unlock_all_vm(&q->running_q);
            fklVMgcStateSet(gc, FKL_GC_NONE);
        }
        uv_mutex_lock(&q->pre_running_lock);
        for (FklThreadQueueNode *n = fklThreadQueuePopNode(&q->pre_running_q);
                n;
                n = fklThreadQueuePopNode(&q->pre_running_q)) {
            FklVM *exe = n->data;
            if (uv_thread_create(&exe->tid, vm_thread_cb, exe)) {
                if (exe->chan) {
                    exe->state = FKL_VM_EXIT;
                    fklChanlSend(FKL_VM_CHANL(exe->chan),
                            fklCreateVMvalueError(exe,
                                    gc->builtinErrorTypeId[FKL_ERR_THREADERROR],
                                    fklCreateVMvalueStr1(exe,
                                            "Failed to create thread")),
                            exe);
                    fklThreadQueueDestroyNode(n);
                } else {
                    FKL_UNREACHABLE();
                }
            } else {
                atomic_fetch_add(&q->running_count, 1);
                fklThreadQueuePushNode(&q->running_q, n);
            }
        }
        uv_mutex_unlock(&q->pre_running_lock);
        if (atomic_load(&q->running_count) == 0) {
            for (FklThreadQueueNode *n = fklThreadQueuePopNode(&q->running_q);
                    n;
                    n = fklThreadQueuePopNode(&q->running_q)) {
                FklVM *exe = n->data;
                uv_thread_join(&exe->tid);
                fklThreadQueueDestroyNode(n);
            }
            return;
        }
        if (atomic_load(&gc->work_num)) {
            switch_notice_lock_ins_for_running_threads(&q->running_q);
            lock_all_vm(&q->running_q);
            uv_mutex_lock(&gc->workq_lock);

            for (struct FklVMidleWork *w = pop_idle_work(gc); w;
                    w = pop_idle_work(gc)) {
                atomic_fetch_sub(&gc->work_num, 1);
                uv_cond_signal(&w->cond);
                w->cb(w->vm, w->arg);
            }

            uv_mutex_unlock(&gc->workq_lock);
            switch_un_notice_lock_ins_for_running_threads(&q->running_q);
            unlock_all_vm(&q->running_q);
        }
        uv_sleep(0);
    }
}

void fklVMstopTheWorld(FklVMgc *gc) {
    FklVMqueue *q = &gc->q;
    switch_notice_lock_ins_for_running_threads(&q->running_q);
    lock_all_vm(&q->running_q);
}

void fklVMcontinueTheWorld(FklVMgc *gc) {
    FklVMqueue *q = &gc->q;
    switch_un_notice_lock_ins_for_running_threads(&q->running_q);
    unlock_all_vm(&q->running_q);
}

void fklVMatExit(FklVM *vm,
        FklVMatExitFunc func,
        FklVMatExitMarkFunc mark,
        void (*finalizer)(void *),
        void *arg) {
    struct FklVMatExit *m =
            (struct FklVMatExit *)fklZmalloc(sizeof(struct FklVMatExit));
    FKL_ASSERT(m);
    m->func = func;
    m->mark = mark;
    m->arg = arg;
    m->finalizer = finalizer;
    m->next = vm->atexit;
    vm->atexit = m;
}

int fklRunVMidleLoop(FklVM *volatile exe) {
    FklVMgc *gc = exe->gc;
    gc->main_thread = exe;
    fklVMthreadStart(exe, &gc->q);
    fklVMidleLoop(gc);
    return gc->exit_code;
}

FklVMvalue *fklCreateVMvalueVarRef(FklVM *exe, FklVMframe *f, uint32_t idx) {
    FklVMvalueVarRef *ref =
            (FklVMvalueVarRef *)fklZcalloc(1, sizeof(FklVMvalueVarRef));
    FKL_ASSERT(ref);
    ref->type_ = FKL_TYPE_VAR_REF;
    ref->ref = &FKL_VM_GET_ARG(exe, f, idx);
    ref->v = NULL;
    ref->idx = idx;
    fklAddToGC(FKL_VM_VAL(ref), exe);
    return FKL_VM_VAL(ref);
}

#define VM_VAR_REF_STATIC_INIT(V)                                              \
    ((FklVMvalueVarRef){                                                       \
        .next_ = NULL,                                                         \
        .gray_next_ = NULL,                                                    \
        .mark_ = FKL_MARK_B,                                                   \
        .type_ = FKL_TYPE_VAR_REF,                                             \
        .idx = UINT32_MAX,                                                     \
        .v = (V),                                                              \
    })

void fklInitClosedVMvalueVarRef(FklVMvalueVarRef *ref, FklVMvalue *v) {
    *ref = VM_VAR_REF_STATIC_INIT(v);
    ref->ref = &ref->v;
}

FklVMvalue *fklCreateClosedVMvalueVarRef(FklVM *exe, FklVMvalue *v) {
    FklVMvalueVarRef *ref =
            (FklVMvalueVarRef *)fklZcalloc(1, sizeof(FklVMvalueVarRef));
    FKL_ASSERT(ref);
    ref->type_ = FKL_TYPE_VAR_REF;
    ref->idx = UINT32_MAX;
    ref->v = v;
    ref->ref = &ref->v;
    fklAddToGC(FKL_VM_VAL(ref), exe);
    return FKL_VM_VAL(ref);
}

static inline void init_lref_vec(FklVM *vm, FklVMframe *f, uint32_t lcount) {
    if (!FKL_IS_VECTOR(f->lref)) {
        f->lref = fklCreateVMvalueVec(vm, lcount);
    }
}

static inline FklVMvalue *
insert_local_ref(FklVM *exe, FklVMframe *f, FklVMvalue *ref, uint32_t cidx) {
    FklVMvalue *rl = fklCreateVMvaluePair(exe, ref, f->lrefl);
    f->lrefl = rl;
    FKL_VM_VEC(f->lref)->base[cidx] = ref;
    return ref;
}

static inline FklVMvalue *get_compound_frame_code_obj(FklVMframe *frame) {
    return FKL_VM_PROC(frame->proc)->bcl;
}

static inline FklVMvalue *
fetch_var_ref(FklVM *exe, const FklVarRefDef *c, FklVMframe *f) {
    uint32_t cidx = FKL_GET_FIX(c->cidx);
    FklVMvalue **ref = f->ref;
    if (FKL_IS_TRUE(c->is_local)) {
        init_lref_vec(exe, f, f->lcount);
        return FKL_VM_VEC(f->lref)->base[cidx]
                     ? FKL_VM_VEC(f->lref)->base[cidx]
                     : insert_local_ref(exe,
                               f,
                               fklCreateVMvalueVarRef(exe, f, cidx),
                               cidx);
    } else {
        return cidx < f->rcount ? ref[cidx]
                                : fklCreateClosedVMvalueVarRef(exe, NULL);
    }
}

static FKL_ALWAYS_INLINE uint32_t fetch_var_refs(FklVM *exe,
        FklVMvalueProc *proc,
        FklVMframe *f) {
    FklVMvalueProto *pt = proc->proto;
    uint32_t count = pt->ref_count;
    if (count > 0) {
        const FklVarRefDef *refs = fklVMvalueProtoVarRefs(pt);
        FklVMvalue **closure = proc->closure;
        for (uint32_t i = 0; i < count; i++) {

            if (closure[i] != NULL)
                continue;
            closure[i] = fetch_var_ref(exe, &refs[i], f);
        }
    }
    return count;
}

uint32_t fklVMfetchVarRef(FklVM *exe, FklVMvalueProc *proc, FklVMframe *f) {
    return fetch_var_refs(exe, proc, f);
}

FklVMvalue *fklCreateVMvalueProc3(FklVM *exe,
        FklVMframe *f,
        size_t cpc,
        FklVMvalueProto *pt) {
    FklVMvalue *co = get_compound_frame_code_obj(f);
    FklVMvalue *r = fklCreateVMvalueProc2(exe, f->pc, cpc, co, pt);

    uint32_t count = pt->ref_count;
    FklVMvalueProc *proc = FKL_VM_PROC(r);
    proc->ref_count = count;
    if (count) {
        const FklVarRefDef *refs = fklVMvalueProtoVarRefs(pt);
        FklVMvalue **closure = proc->closure;
        for (uint32_t i = 0; i < count; i++) {
            closure[i] = fetch_var_ref(exe, &refs[i], f);
        }
    }
    return r;
}

void fklVMstackReserve(FklVM *exe, uint32_t s) {
    if (exe->last >= s)
        return;
    uint32_t old_last = exe->last;
    exe->last <<= 1;
    if (exe->last < s)
        exe->last = s;

    FklVMvalue **nbase = fklAllocLocalVarSpaceFromGC(exe->gc, //
            exe->last,
            &exe->last);

    FklVMvalue **obase = exe->base;
    memcpy(nbase, obase, exe->tp * sizeof(FklVMvalue *));
    exe->base = nbase;
    fklUpdateAllVarRef(exe, exe->top_frame);
    push_old_locv(exe, old_last, obase);
}

void fklVMstackShrink(FklVM *exe) {
    uint32_t old_last = exe->last;
    exe->last = fklNextPow2(exe->tp);
    if (exe->last < FKL_VM_STACK_INC_NUM)
        exe->last = FKL_VM_STACK_INC_NUM;
    if (exe->last == old_last)
        return;
    FklVMvalue **nbase = fklAllocLocalVarSpaceFromGCwithoutLock(exe->gc,
            exe->last,
            &exe->last);
    FklVMvalue **obase = exe->base;
    memcpy(nbase, obase, exe->tp * sizeof(FklVMvalue *));
    exe->base = nbase;
    fklUpdateAllVarRef(exe, exe->top_frame);
    push_old_locv(exe, old_last, obase);
}

void fklDBG_printVMstack(FklVM *stack,
        uint32_t count,
        FklCodeBuilder *fp,
        int mode,
        FklVM *exe) {
    if (mode)
        fklCodeBuilderPuts(fp, "Current stack:\n");
    if (stack->tp == 0)
        fklCodeBuilderPuts(fp, "[#EMPTY]\n");
    else {
        int64_t i = stack->tp - 1;
        int64_t end = stack->tp - count;
        for (; i >= end; i--) {
            if (mode && stack->bp == i) {
                fklCodeBuilderPuts(fp, "->");
                fklCodeBuilderFmt(fp, "%" PRId64 ":", i);
            }
            FklVMvalue *tmp = stack->base[i];
            fklPrin1VMvalue2(tmp, fp, exe);
            fklCodeBuilderPutc(fp, '\n');
        }
    }
}

FklVM *fklCreateVM(FklVMvalue *proc, FklVMgc *gc) {
    FklVM *exe = (FklVM *)fklZcalloc(1, sizeof(FklVM));
    FKL_ASSERT(exe);
    exe->gc = gc;
    exe->prev = exe;
    exe->next = exe;
    vm_stack_init(exe);
    exe->frame_cache_head = &exe->inplace_frame;
    exe->frame_cache_tail = &exe->frame_cache_head->prev;
    exe->state = FKL_VM_READY;
    exe->dummy_ins_func = B_dummy;
    if (proc != NULL) {
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, proc);
        fklCallObj(exe, proc);
    }
    uv_mutex_init(&exe->lock);
    return exe;
}

FklVM *fklCreateThreadVM(FklVMvalue *nextCall,
        uint32_t arg_num,
        FklVMvalue *const *args,
        FklVM *prev,
        FklVM *next) {
    FklVM *exe = (FklVM *)fklZcalloc(1, sizeof(FklVM));
    FKL_ASSERT(exe);
    exe->gc = prev->gc;
    exe->prev = exe;
    exe->next = exe;
    exe->chan = fklCreateVMvalueChanl(exe, 1);
    vm_stack_init(exe);
    exe->frame_cache_head = &exe->inplace_frame;
    exe->frame_cache_tail = &exe->frame_cache_head->prev;
    exe->state = FKL_VM_READY;
    memcpy(exe->rand_state, prev->rand_state, sizeof(uint64_t[4]));
    exe->dummy_ins_func = prev->dummy_ins_func;
    uv_mutex_init(&exe->lock);
    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, nextCall);
    fklVMstackReserve(exe, exe->tp + arg_num + 1);
    memcpy(&exe->base[exe->tp], args, arg_num * sizeof(FklVMvalue *));
    exe->tp += arg_num;
    fklCallObj(exe, nextCall);
    insert_to_VM_chain(exe, prev, next);
    return exe;
}

void fklDestroyAllVMs(FklVM *curVM) {
    if (curVM == NULL)
        return;
    curVM->prev->next = NULL;
    curVM->prev = NULL;
    for (FklVM *cur = curVM; cur;) {
        FklVM *t = cur;
        cur = cur->next;
        fklVMgcMoveLocvCache(t, t->gc);
        if (t->obj_head)
            fklMoveThreadObjectsToGc(t, t->gc);
        remove_exited_thread_common(t);
    }
}

void fklDeleteCallChain(FklVM *exe) {
    while (exe->top_frame) {
        FklVMframe *cur = exe->top_frame;
        exe->top_frame = cur->prev;
        fklDestroyVMframe(cur, exe);
    }
}

#undef RECYCLE_NUN

void fklDestroyVMframes(FklVMframe *h) {
    while (h) {
        FklVMframe *cur = h;
        h = h->prev;
        fklZfree(cur);
    }
}

void fklInitMainProcRefs(FklVM *exe, FklVMvalue *proc_obj) {
    init_builtin_symbol_ref(exe, proc_obj);
}

#ifdef DEFINE_PUSH_STATE0
void fklVMvaluePushState0ToStack(FklParseStateVector *stateStack) {
    FKL_UNREACHABLE();
}
#endif
