#include <fakeLisp/base.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <inttypes.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
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
#include <setjmp.h>
#include <time.h>

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
static inline void call_compound_procedure(FklVM *exe, FklVMvalue *proc) {
    FklVMframe *tmpFrame =
            fklCreateVMframeWithProcValue(exe, proc, exe->top_frame);
    uint32_t lcount = FKL_VM_PROC(proc)->lcount;
    fklVMframeSetBp(exe, tmpFrame, lcount);
    FklVMCompoundFrameVarRef *f = &tmpFrame->c.lr;
    f->lcount = lcount;
    fklPushVMframe(tmpFrame, exe);
}

void fklDBG_printLinkBacktrace(FklVMframe *t, FklVMgc *gc) {
    if (t->type == FKL_FRAME_COMPOUND) {
        if (t->c.sid)
            fklPrintString(fklVMgetSymbolWithId(gc, t->c.sid)->k, stderr);
        else
            fputs("<lambda>", stderr);
        fprintf(stderr, "[%u]", t->c.mark);
    } else
        fputs("<obj>", stderr);

    for (FklVMframe *cur = t->prev; cur; cur = cur->prev) {
        fputs(" --> ", stderr);
        if (cur->type == FKL_FRAME_COMPOUND) {
            if (cur->c.sid)
                fklPrintString(fklVMgetSymbolWithId(gc, cur->c.sid)->k, stderr);
            else
                fputs("<lambda>", stderr);
            fprintf(stderr, "[%u]", cur->c.mark);
        } else
            fputs("<obj>", stderr);
    }
    fputc('\n', stderr);
}

static inline void tail_call_proc(FklVM *exe, FklVMvalue *proc) {
    FklVMframe *frame = exe->top_frame;
    if (fklGetCompoundFrameProc(frame) == proc) {
        frame->c.mark = FKL_VM_COMPOUND_FRAME_MARK_CALL;
    } else {
        FklVMframe *tmpFrame =
                fklCreateVMframeWithProcValue(exe, proc, exe->top_frame->prev);
        uint32_t lcount = FKL_VM_PROC(proc)->lcount;
        fklVMframeSetBp(exe, tmpFrame, lcount);
        FklVMCompoundFrameVarRef *f = &tmpFrame->c.lr;
        f->lcount = lcount;
        fklPushVMframe(tmpFrame, exe);
    }
}

void fklDoPrintCprocBacktrace(FklSid_t sid, FILE *fp, FklVMgc *gc) {
    if (sid) {
        fprintf(fp, "at cproc: ");
        fklPrintRawSymbol(fklVMgetSymbolWithId(gc, sid)->k, fp);
        fputc('\n', fp);
    } else
        fputs("at <cproc>\n", fp);
}

static void cproc_frame_print_backtrace(void *data, FILE *fp, FklVMgc *gc) {
    FklCprocFrameContext *c = (FklCprocFrameContext *)data;
    FklVMcproc *cproc = FKL_VM_CPROC(c->proc);
    fklDoPrintCprocBacktrace(cproc->sid, fp, gc);
}

static void cproc_frame_atomic(void *data, FklVMgc *gc) {
    FklCprocFrameContext *c = (FklCprocFrameContext *)data;
    fklVMgcToGray(c->proc, gc);
}

static void cproc_frame_copy(void *d, const void *s, FklVM *exe) {
    FklCprocFrameContext const *const sc = (FklCprocFrameContext *)s;
    FklCprocFrameContext *dc = (FklCprocFrameContext *)d;
    *dc = *sc;
}

static int cproc_frame_step(void *data, FklVM *exe) {
    FklCprocFrameContext *c = FKL_TYPE_CAST(FklCprocFrameContext *, data);
    return c->func(exe, c, FKL_CPROC_GET_ARG_NUM(exe, c));
}

static const FklVMframeContextMethodTable CprocContextMethodTable = {
    .atomic = cproc_frame_atomic,
    .copy = cproc_frame_copy,
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

static inline void callCproc(FklVM *exe, FklVMvalue *cproc) {
    FklVMframe *f =
            fklCreateOtherObjVMframe(exe, &CprocContextMethodTable, NULL);
    initCprocFrameContext(f->data, cproc, exe);
    fklPushVMframe(f, exe);
}

static inline void B_dummy(FKL_VM_INS_FUNC_ARGL) {
    FklVMvalue *err = fklCreateVMvalueError(exe,
            exe->gc->builtinErrorTypeId[FKL_ERR_INVALIDACCESS],
            fklCreateStringFromCstr("reach invalid opcode: `dummy`"));
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

static inline void init_builtin_symbol_ref(FklVM *exe, FklVMvalue *proc_obj) {
    FklVMgc *gc = exe->gc;
    FklVMproc *proc = FKL_VM_PROC(proc_obj);
    FklFuncPrototype *pt = &gc->pts->pa[proc->protoId];
    proc->rcount = pt->rcount;
    FklVMvalue **closure;
    if (!proc->rcount)
        closure = NULL;
    else {
        closure =
                (FklVMvalue **)fklZmalloc(proc->rcount * sizeof(FklVMvalue *));
        FKL_ASSERT(closure);
    }
    for (uint32_t i = 0; i < proc->rcount; i++) {
        FklSymDefHashMapMutElm *cur_ref = &pt->refs[i];
        if (cur_ref->v.cidx < FKL_BUILTIN_SYMBOL_NUM)
            closure[i] = gc->builtin_refs[cur_ref->v.cidx];
        else
            closure[i] = fklCreateClosedVMvalueVarRef(exe, NULL);
    }
    proc->closure = closure;
}

static inline void vm_stack_init(FklVM *exe) {
    exe->last = FKL_VM_STACK_INC_NUM;
    exe->tp = 0;
    exe->bp = 0;
    exe->base = fklAllocLocalVarSpaceFromGC(exe->gc, exe->last, &exe->last);
    FKL_ASSERT(exe->base);
}

FklVM *fklCreateVMwithByteCode(FklByteCodelnt *mainCode,
        FklSymbolTable *runtime_st,
        FklConstTable *runtime_kt,
        FklFuncPrototypes *pts,
        uint32_t pid) {
    FklVM *exe = (FklVM *)fklZcalloc(1, sizeof(FklVM));
    FKL_ASSERT(exe);
    exe->prev = exe;
    exe->next = exe;
    exe->pts = pts;
    exe->gc = fklCreateVMgc(runtime_st, runtime_kt, pts, 0, NULL);
    exe->frame_cache_head = &exe->inplace_frame;
    exe->frame_cache_tail = &exe->frame_cache_head->prev;
    fklInitGlobalVMclosureForGC(exe);
    vm_stack_init(exe);
    if (mainCode != NULL) {
        FklVMvalue *proc = fklCreateVMvalueProc(exe,
                fklCreateVMvalueCodeObjMove(exe, mainCode),
                pid);
        fklDestroyByteCodelnt(mainCode);
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

void fklDoPrintBacktrace(FklVMframe *f, FILE *fp, FklVMgc *gc) {
    void (*backtrace)(void *data, FILE *, FklVMgc *) = f->t->print_backtrace;
    if (backtrace)
        backtrace(f->data, fp, gc);
    else
        fprintf(fp, "at callable-obj\n");
}

void fklDoFinalizeObjFrame(FklVM *vm, FklVMframe *f) {
    if (f->t->finalizer)
        f->t->finalizer(fklGetFrameData(f));
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
    FklVMvalue **p = FKL_VM_VAR_REF_GET(ref);
    return p == &FKL_VM_VAR_REF(ref)->v;
}

static inline void close_all_var_ref(FklVMCompoundFrameVarRef *lr) {
    for (FklVMvarRefList *l = lr->lrefl; l;) {
        close_var_ref(l->ref);
        FklVMvarRefList *c = l;
        l = c->next;
        fklZfree(c);
    }
}

void fklDoFinalizeCompoundFrame(FklVM *exe, FklVMframe *frame) {
    FklVMCompoundFrameVarRef *lr = &frame->c.lr;
    close_all_var_ref(lr);
    fklZfree(lr->lref);

    frame->prev = NULL;
    *(exe->frame_cache_tail) = frame;
    exe->frame_cache_tail = &frame->prev;
}

void fklDoCopyObjFrameContext(FklVMframe *s, FklVMframe *d, FklVM *exe) {
    s->t->copy(fklGetFrameData(d), fklGetFrameData(s), exe);
}

void fklPushVMframe(FklVMframe *f, FklVM *exe) {
    f->prev = exe->top_frame;
    exe->top_frame = f;
}

FklVMframe *fklMoveVMframeToTop(FklVM *exe, FklVMframe *f) {
    FklVMframe *prev = exe->top_frame;
    for (; prev && prev->prev != f; prev = prev->prev)
        ;
    if (prev) {
        prev->prev = f->prev;
        f->prev = exe->top_frame;
        exe->top_frame = f;
    }
    return prev;
}

void fklInsertTopVMframeAsPrev(FklVM *exe, FklVMframe *prev) {
    FklVMframe *f = exe->top_frame;
    exe->top_frame = f->prev;
    f->prev = prev->prev;
    prev->prev = f;
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
        fklDoFinalizeCompoundFrame(exe, f);
        break;
    case FKL_FRAME_OTHEROBJ:
        fklDoFinalizeObjFrame(exe, f);
        break;
    }
}

static inline void doAtomicFrame(FklVMframe *f, FklVMgc *gc) {
    if (f->t->atomic)
        f->t->atomic(fklGetFrameData(f), gc);
}

void fklDoAtomicFrame(FklVMframe *f, FklVMgc *gc) {
    switch (f->type) {
    case FKL_FRAME_COMPOUND:
        for (FklVMvarRefList *l = fklGetCompoundFrameLocRef(f)->lrefl; l;
                l = l->next)
            fklVMgcToGray(l->ref, gc);
        fklVMgcToGray(fklGetCompoundFrameProc(f), gc);
        break;
    case FKL_FRAME_OTHEROBJ:
        doAtomicFrame(f, gc);
        break;
    }
}

#define CALL_CALLABLE_OBJ(EXE, V)                                              \
    case FKL_TYPE_CPROC:                                                       \
        callCproc(EXE, V);                                                     \
        break;                                                                 \
    case FKL_TYPE_USERDATA:                                                    \
        FKL_VM_UD(V)->t->__call(V, EXE);                                       \
        break;                                                                 \
    default:                                                                   \
        break;

static inline void apply_compound_proc(FklVM *exe, FklVMvalue *proc) {
    FklVMframe *tmpFrame =
            fklCreateVMframeWithProcValue(exe, proc, exe->top_frame);
    uint32_t lcount = FKL_VM_PROC(proc)->lcount;
    fklVMframeSetBp(exe, tmpFrame, lcount);
    FklVMCompoundFrameVarRef *f = &tmpFrame->c.lr;
    f->lcount = lcount;
    fklPushVMframe(tmpFrame, exe);
}

void fklCallObj(FklVM *exe, FklVMvalue *proc) {
    switch (proc->type) {
    case FKL_TYPE_PROC:
        apply_compound_proc(exe, proc);
        break;
        CALL_CALLABLE_OBJ(exe, proc);
    }
}

void fklTailCallObj(FklVM *exe, FklVMvalue *proc) {
    FklVMframe *frame = exe->top_frame;
    exe->top_frame = frame->prev;
    fklDoFinalizeObjFrame(exe, frame);
    fklCallObj(exe, proc);
}

void fklVMsetTpAndPushValue(FklVM *exe, uint32_t rtp, FklVMvalue *retval) {
    exe->tp = rtp + 1;
    exe->base[rtp] = retval;
}

#define NOTICE_LOCK(EXE)                                                       \
    {                                                                          \
        uv_mutex_unlock(&(EXE)->lock);                                         \
        uv_mutex_lock(&(EXE)->lock);                                           \
    }

#define DO_ERROR_HANDLING(exe)                                                 \
    {                                                                          \
        FklVMvalue *ev = FKL_VM_POP_TOP_VALUE(exe);                            \
        FklVMframe *frame = fklIsVMvalueError(ev) ? exe->top_frame : NULL;     \
        for (; frame; frame = frame->prev)                                     \
            if (frame->errorCallBack != NULL                                   \
                    && frame->errorCallBack(frame, ev, exe))                   \
                break;                                                         \
        if (frame == NULL) {                                                   \
            if (fklVMinterrupt(exe, ev, &ev) == FKL_INT_DONE)                  \
                continue;                                                      \
            fklPrintErrBacktrace(ev, exe, stderr);                             \
            if (exe->chan) {                                                   \
                fklChanlSend(FKL_VM_CHANL(exe->chan), ev, exe);                \
                exe->state = FKL_VM_EXIT;                                      \
                exe->chan = NULL;                                              \
                continue;                                                      \
            } else {                                                           \
                exe->gc->exit_code = 255;                                      \
                exe->state = FKL_VM_EXIT;                                      \
                continue;                                                      \
            }                                                                  \
        }                                                                      \
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

#define THREAD_EXIT(exe)                                                       \
    do_vm_atexit(exe);                                                         \
    if (exe->chan) {                                                           \
        FklVMvalue *v = FKL_VM_GET_TOP_VALUE(exe);                             \
        FklVMvalue *resultBox = fklCreateVMvalueBox(exe, v);                   \
        fklChanlSend(FKL_VM_CHANL(exe->chan), resultBox, exe);                 \
        exe->chan = NULL;                                                      \
    }

void fklSetVMsingleThread(FklVM *exe) {
    exe->is_single_thread = 1;
    exe->thread_run_cb = fklRunVMinSingleThread;
}

void fklUnsetVMsingleThread(FklVM *exe) {
    exe->is_single_thread = 0;
    exe->thread_run_cb = exe->run_cb;
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

static inline FklVMvalue *get_var_val(FklVMframe *frame,
        uint32_t idx,
        FklFuncPrototypes *pts,
        FklSid_t *psid) {
    FklVMCompoundFrameVarRef *lr = &frame->c.lr;
    FklVMvalue *v = idx < lr->rcount ? *FKL_VM_VAR_REF_GET(lr->ref[idx]) : NULL;
    if (v)
        return v;
    FklVMproc *proc = FKL_VM_PROC(fklGetCompoundFrameProc(frame));
    FklFuncPrototype *pt = &pts->pa[proc->protoId];
    FklSymDefHashMapMutElm *def = &pt->refs[idx];
    *psid = def->k.id;
    return NULL;
}

static inline FklVMvalue *volatile *get_var_ref(FklVMframe *frame,
        uint32_t idx,
        FklFuncPrototypes *pts,
        FklSid_t *psid) {
    FklVMCompoundFrameVarRef *lr = &frame->c.lr;
    FklVMvalue **refs = lr->ref;
    FklVMvalue *volatile *v =
            (idx >= lr->rcount || !(FKL_VM_VAR_REF(refs[idx])->ref))
                    ? NULL
                    : FKL_VM_VAR_REF_GET(refs[idx]);
    if (v && *v)
        return v;
    FklVMproc *proc = FKL_VM_PROC(fklGetCompoundFrameProc(frame));
    FklFuncPrototype *pt = &pts->pa[proc->protoId];
    FklSymDefHashMapMutElm *def = &pt->refs[idx];
    *psid = def->k.id;
    return NULL;
    return v;
}

static inline FklImportDllInitFunc getImportInit(uv_lib_t *handle) {
    return (FklImportDllInitFunc)fklGetAddress("_fklImportInit", handle);
}

static inline void
close_var_ref_between(FklVMvalue **lref, uint32_t sIdx, uint32_t eIdx) {
    if (lref)
        for (; sIdx < eIdx; sIdx++) {
            FklVMvalue *ref = lref[sIdx];
            if (ref) {
                close_var_ref(ref);
                lref[sIdx] = NULL;
            }
        }
}

#define GET_INS_UX(ins, frame)                                                 \
    ((ins)->bu | (((uint32_t)frame->c.pc++->bu) << FKL_I16_WIDTH))
#define GET_INS_IX(ins, frame) ((int32_t)GET_INS_UX(ins, frame))
#define GET_INS_UXX(ins, frame)                                                \
    (frame->c.pc += 2,                                                         \
            FKL_GET_INS_UC(ins)                                                \
                    | (((uint64_t)FKL_GET_INS_UC((ins) + 1)) << FKL_I24_WIDTH) \
                    | (((uint64_t)ins[2].bu) << (FKL_I24_WIDTH * 2)))
#define GET_INS_IXX(ins, frame) ((int64_t)GET_INS_UXX(ins, frame))

static inline FklVMlib *get_importing_lib(FklVMframe *f, FklVMgc *gc) {
    const FklInstruction *first_ins =
            &FKL_VM_CO(FKL_VM_PROC(f->c.proc)->codeObj)->bc->code[0];

    FKL_ASSERT(first_ins->op >= FKL_OP_EXPORT_TO
               && first_ins->op <= FKL_OP_EXPORT_TO_XX);

    FklInstructionArg arg;
    fklGetInsOpArg(first_ins, &arg);

    return &gc->libs[arg.ux];
}

static int
import_lib_error_callback(FklVMframe *f, FklVMvalue *errValue, FklVM *exe) {
    atomic_store(&get_importing_lib(f, exe->gc)->import_state,
            FKL_VM_LIB_ERROR);
    uv_mutex_unlock(&exe->gc->libs_lock);
    return 0;
}

static inline void execute_compound_frame(FklVM *exe, FklVMframe *frame) {
    FklInstruction *ins;
    FklVMlib *plib;
    uint32_t idx;
    uint32_t idx1;
    uint64_t size;
    uint64_t num;
    int64_t offset;
start:
    ins = frame->c.pc++;
    switch ((FklOpcode)ins->op) {
#define DISPATCH_INCLUDED
#define DISPATCH_SWITCH
#include "vmdispatch.h"
#undef DISPATCH_INCLUDED
#undef DISPATCH_SWITCH
    }
    goto start;
}

static inline void execute_compound_frame_check_trap(FklVM *exe,
        FklVMframe *frame) {
    FklInstruction *ins;
    FklVMlib *plib;
    uint32_t idx;
    uint32_t idx1;
    uint64_t size;
    uint64_t num;
    int64_t offset;
start:
    ins = frame->c.pc++;
    switch ((FklOpcode)ins->op) {
#define DISPATCH_INCLUDED
#define DISPATCH_SWITCH
#include "vmdispatch.h"
#undef DISPATCH_INCLUDED
#undef DISPATCH_SWITCH
    }
    if (exe->trapping)
        fklVMinterrupt(exe, FKL_VM_NIL, NULL);
    goto start;
}

#define DISPATCH_INCLUDED
#include "vmdispatch.h"
#undef DISPATCH_INCLUDED

#define RETURN_INCLUDE
#include "vmreturn.h"
#undef RETURN_INCLUDE

int fklRunVMinSingleThread(FklVM *volatile exe, FklVMframe *const exit_frame) {
    jmp_buf buf;
    for (;;) {
        switch (exe->state) {
        case FKL_VM_RUNNING: {
            FklVMframe *curframe = exe->top_frame;
            switch (curframe->type) {
            case FKL_FRAME_COMPOUND:
                execute_compound_frame(exe, curframe);
                break;
            case FKL_FRAME_OTHEROBJ:
                if (fklDoCallableObjFrameStep(curframe, exe))
                    continue;
                fklDoFinalizeObjFrame(exe, popFrame(exe));
                break;
            }
            if (exe->top_frame == exit_frame)
                return 0;
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
                    return 1;
                }
            }
            exe->state = FKL_VM_RUNNING;
            continue;
            break;
        case FKL_VM_WAITING:
            continue;
            break;
        }
    }
    return 0;
}

static inline void move_thread_objects_to_gc(FklVM *vm, FklVMgc *gc) {
    if (vm->obj_head) {
        vm->obj_tail->next = gc->head;
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

void fklCheckAndGCinSingleThread(FklVM *exe) {
    FklVMgc *gc = exe->gc;
    if (atomic_load(&gc->alloced_size) > gc->threshold) {
        move_thread_objects_to_gc(exe, gc);
        fklVMgcMoveLocvCache(exe, gc);
        remove_thread_frame_cache(exe);
        fklVMgcMarkAllRootToGray(exe);
        while (!fklVMgcPropagate(gc))
            ;
        FklVMvalue *white = NULL;
        fklVMgcCollect(gc, &white);
        fklVMgcSweep(white);
        fklVMgcUpdateThreshold(gc);

        fklVMstackShrink(exe);
    }
}

static int vm_run_cb(FklVM *exe, FklVMframe *const exit_frame) {
    jmp_buf buf;
    for (;;) {
        switch (exe->state) {
        case FKL_VM_RUNNING: {
            FklVMframe *curframe = exe->top_frame;
            switch (curframe->type) {
            case FKL_FRAME_COMPOUND:
                execute_compound_frame(exe, curframe);
                break;
            case FKL_FRAME_OTHEROBJ:
                if (fklDoCallableObjFrameStep(curframe, exe))
                    continue;
                fklDoFinalizeObjFrame(exe, popFrame(exe));
                break;
            }
            if (atomic_load(&(exe)->notice_lock))
                NOTICE_LOCK(exe);
            if (exe->top_frame == exit_frame)
                return 0;
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
                    return 1;
                }
            }
            exe->state = FKL_VM_RUNNING;
            continue;
            break;
        case FKL_VM_WAITING:
            fklUnlockThread(exe);
            uv_sleep(0);
            fklLockThread(exe);
            continue;
            break;
        }
    }
    return 0;
}

static void vm_thread_cb(void *arg) {
    FklVM *volatile exe = (FklVM *)arg;
    jmp_buf buf;
    uv_mutex_lock(&exe->lock);
    exe->thread_run_cb = vm_run_cb;
    exe->run_cb = vm_run_cb;
    for (;;) {
        switch (exe->state) {
        case FKL_VM_RUNNING: {
            FklVMframe *curframe = exe->top_frame;
            switch (curframe->type) {
            case FKL_FRAME_COMPOUND:
                execute_compound_frame(exe, curframe);
                break;
            case FKL_FRAME_OTHEROBJ:
                if (fklDoCallableObjFrameStep(curframe, exe))
                    continue;
                fklDoFinalizeObjFrame(exe, popFrame(exe));
                break;
            }
            if (atomic_load(&(exe)->notice_lock))
                NOTICE_LOCK(exe);
            if (exe->top_frame == NULL)
                exe->state = FKL_VM_EXIT;
        }; break;
        case FKL_VM_EXIT:
            exe->buf = NULL;
            THREAD_EXIT(exe);
            atomic_fetch_sub(&exe->gc->q.running_count, 1);
            uv_mutex_unlock(&exe->lock);
            return;
            break;
        case FKL_VM_READY:
            exe->buf = &buf;
            if (setjmp(buf) == FKL_VM_ERR_RAISE)
                DO_ERROR_HANDLING(exe);
            exe->state = FKL_VM_RUNNING;
            continue;
            break;
        case FKL_VM_WAITING:
            fklUnlockThread(exe);
            uv_sleep(0);
            fklLockThread(exe);
            continue;
            break;
        }
    }
}

static int vm_trapping_run_cb(FklVM *exe, FklVMframe *const exit_frame) {
    jmp_buf buf;
    for (;;) {
        switch (exe->state) {
        case FKL_VM_RUNNING: {
            FklVMframe *curframe = exe->top_frame;
            switch (curframe->type) {
            case FKL_FRAME_COMPOUND:
                execute_compound_frame_check_trap(exe, curframe);
                if (exe->trapping)
                    fklVMinterrupt(exe, FKL_VM_NIL, NULL);
                break;
            case FKL_FRAME_OTHEROBJ:
                if (fklDoCallableObjFrameStep(curframe, exe))
                    continue;
                if (atomic_load(&(exe)->notice_lock))
                    NOTICE_LOCK(exe);
                fklDoFinalizeObjFrame(exe, popFrame(exe));
                break;
            }
            if (exe->top_frame == exit_frame)
                return 0;
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
                    return 1;
                }
            }
            exe->state = FKL_VM_RUNNING;
            continue;
            break;
        case FKL_VM_WAITING:
            fklUnlockThread(exe);
            uv_sleep(0);
            fklLockThread(exe);
            continue;
            break;
        }
    }
    return 0;
}

static void vm_trapping_thread_cb(void *arg) {
    FklVM *volatile exe = (FklVM *)arg;
    jmp_buf buf;
    uv_mutex_lock(&exe->lock);
    exe->thread_run_cb = vm_trapping_run_cb;
    exe->run_cb = vm_trapping_run_cb;
    for (;;) {
        switch (exe->state) {
        case FKL_VM_RUNNING: {
            FklVMframe *curframe = exe->top_frame;
            switch (curframe->type) {
            case FKL_FRAME_COMPOUND:
                execute_compound_frame_check_trap(exe, curframe);
                if (exe->trapping)
                    fklVMinterrupt(exe, FKL_VM_NIL, NULL);
                break;
            case FKL_FRAME_OTHEROBJ:
                if (fklDoCallableObjFrameStep(curframe, exe))
                    continue;
                if (atomic_load(&(exe)->notice_lock))
                    NOTICE_LOCK(exe);
                fklDoFinalizeObjFrame(exe, popFrame(exe));
                break;
            }
            if (exe->top_frame == NULL)
                exe->state = FKL_VM_EXIT;
        } break;
        case FKL_VM_EXIT:
            exe->buf = NULL;
            THREAD_EXIT(exe);
            atomic_fetch_sub(&exe->gc->q.running_count, 1);
            uv_mutex_unlock(&exe->lock);
            return;
            break;
        case FKL_VM_READY:
            exe->buf = &buf;
            if (setjmp(buf) == FKL_VM_ERR_RAISE)
                DO_ERROR_HANDLING(exe);
            exe->state = FKL_VM_RUNNING;
            continue;
            break;
        case FKL_VM_WAITING:
            fklUnlockThread(exe);
            uv_sleep(0);
            fklLockThread(exe);
            continue;
            break;
        }
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

#undef NOTICE_LOCK

static inline void switch_notice_lock_ins(FklVM *exe) {
    if (atomic_load(&exe->notice_lock))
        return;
    atomic_store(&exe->notice_lock, 1);
}

void fklNoticeThreadLock(FklVM *exe) { switch_notice_lock_ins(exe); }

static inline void switch_notice_lock_ins_for_running_threads(
        FklThreadQueue *q) {
    for (FklThreadQueueNode *n = q->head; n; n = n->next)
        switch_notice_lock_ins(n->data);
}

static inline void switch_un_notice_lock_ins(FklVM *exe) {
    if (atomic_load(&exe->notice_lock))
        atomic_store(&exe->notice_lock, 0);
}

void fklDontNoticeThreadLock(FklVM *exe) { switch_un_notice_lock_ins(exe); }

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

static inline void
move_all_thread_objects_and_old_locv_to_gc_and_remove_frame_cache(FklVMgc *gc) {
    FklVM *vm = gc->main_thread;
    move_thread_objects_to_gc(vm, gc);
    fklVMgcMoveLocvCache(vm, gc);
    remove_thread_frame_cache(vm);
    for (FklVM *cur = vm->next; cur != vm; cur = cur->next) {
        move_thread_objects_to_gc(cur, gc);
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
    if (uv_cond_init(&work.cond))
        abort();
    fklUnlockThread(vm);
    uv_mutex_lock(&gc->workq_lock);

    push_idle_work(gc, &work);

    atomic_fetch_add(&gc->work_num, 1);

    uv_cond_wait(&work.cond, &gc->workq_lock);
    uv_mutex_unlock(&gc->workq_lock);

    fklLockThread(vm);
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

static inline void vm_idle_loop(FklVMgc *gc) {
    FklVMqueue *q = &gc->q;
    for (;;) {
        if (atomic_load(&gc->alloced_size) > gc->threshold) {
            switch_notice_lock_ins_for_running_threads(&q->running_q);
            lock_all_vm(&q->running_q);

            move_all_thread_objects_and_old_locv_to_gc_and_remove_frame_cache(
                    gc);

            FklVM *exe = gc->main_thread;
            fklVMgcMarkAllRootToGray(exe);
            while (!fklVMgcPropagate(gc))
                ;
            FklVMvalue *white = NULL;
            fklVMgcCollect(gc, &white);
            fklVMgcSweep(white);
            fklVMgcUpdateThreshold(gc);

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
                                    fklCreateStringFromCstr(
                                            "Failed to create thread")),
                            exe);
                    fklThreadQueueDestroyNode(n);
                } else
                    abort();
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

void fklVMidleLoop(FklVMgc *gc) { vm_idle_loop(gc); }

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

void fklVMtrappingIdleLoop(FklVMgc *gc) {
    FklVMqueue *q = &gc->q;
    for (;;) {
        if (atomic_load(&gc->alloced_size) > gc->threshold) {
            switch_notice_lock_ins_for_running_threads(&q->running_q);
            lock_all_vm(&q->running_q);

            move_all_thread_objects_and_old_locv_to_gc_and_remove_frame_cache(
                    gc);

            FklVM *exe = gc->main_thread;
            fklVMgcMarkAllRootToGray(exe);
            while (!fklVMgcPropagate(gc))
                ;
            FklVMvalue *white = NULL;
            fklVMgcCollect(gc, &white);
            fklVMgcSweep(white);
            fklVMgcUpdateThreshold(gc);

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
        }
        uv_mutex_lock(&q->pre_running_lock);
        for (FklThreadQueueNode *n = fklThreadQueuePopNode(&q->pre_running_q);
                n;
                n = fklThreadQueuePopNode(&q->pre_running_q)) {
            FklVM *exe = n->data;
            if (uv_thread_create(&exe->tid, vm_trapping_thread_cb, exe))
                abort();
            atomic_fetch_add(&q->running_count, 1);
            fklThreadQueuePushNode(&q->running_q, n);
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

int fklRunVM(FklVM *volatile exe) {
    FklVMgc *gc = exe->gc;
    gc->main_thread = exe;
    fklVMthreadStart(exe, &gc->q);
    vm_idle_loop(gc);
    return gc->exit_code;
}

void fklChangeGCstate(FklGCstate state, FklVMgc *gc) { gc->running = state; }

FklGCstate fklGetGCstate(FklVMgc *gc) {
    FklGCstate state = FKL_GC_NONE;
    state = gc->running;
    return state;
}

FklVMvalue *fklCreateVMvalueVarRef(FklVM *exe, FklVMframe *f, uint32_t idx) {
    FklVMvalueVarRef *ref =
            (FklVMvalueVarRef *)fklZcalloc(1, sizeof(FklVMvalueVarRef));
    FKL_ASSERT(ref);
    ref->type = FKL_TYPE_VAR_REF;
    ref->ref = &FKL_VM_GET_ARG(exe, f, idx);
    ref->v = NULL;
    ref->idx = idx;
    fklAddToGC((FklVMvalue *)ref, exe);
    return (FklVMvalue *)ref;
}

FklVMvalue *fklCreateClosedVMvalueVarRef(FklVM *exe, FklVMvalue *v) {
    FklVMvalueVarRef *ref =
            (FklVMvalueVarRef *)fklZcalloc(1, sizeof(FklVMvalueVarRef));
    FKL_ASSERT(ref);
    ref->type = FKL_TYPE_VAR_REF;
    ref->v = v;
    ref->ref = &ref->v;
    fklAddToGC((FklVMvalue *)ref, exe);
    return (FklVMvalue *)ref;
}

static inline void inc_lref(FklVMCompoundFrameVarRef *lr, uint32_t lcount) {
    if (!lr->lref) {
        lr->lref = (FklVMvalue **)fklZcalloc(lcount, sizeof(FklVMvalue *));
        FKL_ASSERT(lr->lref);
    }
}

static inline FklVMvalue *
insert_local_ref(FklVMCompoundFrameVarRef *lr, FklVMvalue *ref, uint32_t cidx) {
    FklVMvarRefList *rl =
            (FklVMvarRefList *)fklZmalloc(sizeof(FklVMvarRefList));
    FKL_ASSERT(rl);
    rl->ref = ref;
    rl->next = lr->lrefl;
    lr->lrefl = rl;
    lr->lref[cidx] = ref;
    return ref;
}

static inline FklVMvalue *get_compound_frame_code_obj(FklVMframe *frame) {
    return FKL_VM_PROC(frame->c.proc)->codeObj;
}

void fklCreateVMvalueClosureFrom(FklVM *vm,
        FklVMvalue **closure,
        FklVMframe *f,
        uint32_t i,
        FklFuncPrototype *pt) {
    uint32_t count = pt->rcount;
    FklVMCompoundFrameVarRef *lr = fklGetCompoundFrameLocRef(f);
    FklVMvalue **ref = lr->ref;
    for (; i < count; i++) {
        FklSymDefHashMapMutElm *c = &pt->refs[i];
        if (c->v.isLocal) {
            inc_lref(lr, lr->lcount);
            if (lr->lref[c->v.cidx])
                closure[i] = lr->lref[c->v.cidx];
            else
                closure[i] = insert_local_ref(lr,
                        fklCreateVMvalueVarRef(vm, f, c->v.cidx),
                        c->v.cidx);
        } else {
            if (c->v.cidx >= lr->rcount)
                closure[i] = fklCreateClosedVMvalueVarRef(vm, NULL);
            else
                closure[i] = ref[c->v.cidx];
        }
    }
}

FklVMvalue *fklCreateVMvalueProcWithFrame(FklVM *exe,
        FklVMframe *f,
        size_t cpc,
        uint32_t pid) {
    FklVMvalue *codeObj = get_compound_frame_code_obj(f);
    FklFuncPrototype *pt = &exe->pts->pa[pid];
    FklVMvalue *r = fklCreateVMvalueProc2(exe,
            fklGetCompoundFrameCode(f),
            cpc,
            codeObj,
            pid);
    uint32_t count = pt->rcount;
    FklVMCompoundFrameVarRef *lr = fklGetCompoundFrameLocRef(f);
    FklVMproc *proc = FKL_VM_PROC(r);
    if (count) {
        FklVMvalue **ref = lr->ref;
        FklVMvalue **closure =
                (FklVMvalue **)fklZmalloc(count * sizeof(FklVMvalue *));
        FKL_ASSERT(closure);
        for (uint32_t i = 0; i < count; i++) {
            FklSymDefHashMapMutElm *c = &pt->refs[i];
            if (c->v.isLocal) {
                inc_lref(lr, lr->lcount);
                if (lr->lref[c->v.cidx])
                    closure[i] = lr->lref[c->v.cidx];
                else
                    closure[i] = insert_local_ref(lr,
                            fklCreateVMvalueVarRef(exe, f, c->v.cidx),
                            c->v.cidx);
            } else {
                if (c->v.cidx >= lr->rcount)
                    closure[i] = fklCreateClosedVMvalueVarRef(exe, NULL);
                else
                    closure[i] = ref[c->v.cidx];
            }
        }
        proc->closure = closure;
        proc->rcount = count;
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

    FklVMvalue **nbase =
            fklAllocLocalVarSpaceFromGC(exe->gc, exe->last, &exe->last);

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
        FILE *fp,
        int mode,
        FklVMgc *gc) {
    if (fp != stdout)
        fprintf(fp, "Current stack:\n");
    if (stack->tp == 0)
        fprintf(fp, "[#EMPTY]\n");
    else {
        int64_t i = stack->tp - 1;
        int64_t end = stack->tp - count;
        for (; i >= end; i--) {
            if (mode && stack->bp == i)
                fputs("->", stderr);
            if (fp != stdout)
                fprintf(fp, "%" PRId64 ":", i);
            FklVMvalue *tmp = stack->base[i];
            fklPrin1VMvalue(tmp, fp, gc);
            putc('\n', fp);
        }
    }
}

void fklDBG_printVMvalue(FklVMvalue *v, FILE *fp, FklVMgc *gc) {
    fklPrin1VMvalue(v, fp, gc);
}

FklVM *fklCreateVM(FklVMvalue *proc, FklVMgc *gc) {
    FklVM *exe = (FklVM *)fklZcalloc(1, sizeof(FklVM));
    FKL_ASSERT(exe);
    exe->gc = gc;
    exe->prev = exe;
    exe->next = exe;
    vm_stack_init(exe);
    exe->pts = gc->pts;
    exe->frame_cache_head = &exe->inplace_frame;
    exe->frame_cache_tail = &exe->frame_cache_head->prev;
    exe->state = FKL_VM_READY;
    exe->dummy_ins_func = B_dummy;
    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, proc);
    fklCallObj(exe, proc);
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
    exe->pts = prev->pts;
    exe->frame_cache_head = &exe->inplace_frame;
    exe->frame_cache_tail = &exe->frame_cache_head->prev;
    exe->state = FKL_VM_READY;
    memcpy(exe->rand_state, prev->rand_state, sizeof(uint64_t[4]));
    exe->dummy_ins_func = prev->dummy_ins_func;
    exe->debug_ctx = prev->debug_ctx;
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
    curVM->prev->next = NULL;
    curVM->prev = NULL;
    for (FklVM *cur = curVM; cur;) {
        FklVM *t = cur;
        cur = cur->next;
        fklVMgcMoveLocvCache(t, t->gc);
        if (t->obj_head)
            move_thread_objects_to_gc(t, t->gc);
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

void fklInitVMlib(FklVMlib *lib, FklVMvalue *proc_obj, uint64_t spc) {
    lib->proc = proc_obj;
    lib->loc = NULL;
    lib->import_state = FKL_VM_LIB_NONE;
    lib->count = 0;
    lib->spc = spc;
}

void fklInitVMlibWithCodeObj(FklVMlib *lib,
        FklVMvalue *codeObj,
        FklVM *exe,
        uint32_t protoId,
        uint64_t spc) {
    FklVMvalue *proc = fklCreateVMvalueProc(exe, codeObj, protoId);
    fklInitVMlib(lib, proc, spc);
}

void fklUninitVMlib(FklVMlib *lib) {
    fklZfree(lib->loc);
    memset(lib, 0, sizeof(*lib));
}

void fklDestroyVMlib(FklVMlib *lib) {
    fklUninitVMlib(lib);
    fklZfree(lib);
}

void fklInitMainProcRefs(FklVM *exe, FklVMvalue *proc_obj) {
    init_builtin_symbol_ref(exe, proc_obj);
}
