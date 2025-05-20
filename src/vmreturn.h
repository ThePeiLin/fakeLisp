#ifndef RETURN_INCLUDE
#include "vmrun.c"
#endif

#ifndef VM
#define VM exe
#endif

#ifndef F
#define F frame
#endif

#ifndef RETURN_HEADER
void fklVMcompoundFrameReturn(FklVM *VM) {
    FklVMframe *F = exe->top_frame;
    FKL_ASSERT(VM->top_frame->type == FKL_FRAME_COMPOUND);
#endif
    switch (F->c.mark) {
    case FKL_VM_COMPOUND_FRAME_MARK_RET: {
        VM->bp = FKL_GET_FIX(FKL_VM_GET_ARG(VM, F, -2));
        // copy stack values
        uint32_t const value_count = (VM->tp - F->c.sp);
        if (value_count > 1 || value_count < 1)
            goto return_value_err;
        memmove(&FKL_VM_GET_ARG(VM, F, -2), &VM->base[F->c.sp],
                value_count * sizeof(FklVMvalue *));
        VM->tp = F->bp - 1 + value_count;
        fklDoFinalizeCompoundFrame(VM, popFrame(VM));
        return;
    return_value_err:
        fprintf(stderr,
                "[%s: %d] %s: the return value count should be 1, but is %u\n",
                __FILE__, __LINE__, __FUNCTION__, value_count);
        fklPrintBacktrace(exe, stderr);
        abort();
    } break;
    case FKL_VM_COMPOUND_FRAME_MARK_CALL: {
        close_all_var_ref(&F->c.lr);
        // copy stack values
        uint32_t const value_count = (VM->tp - VM->bp);
        memmove(&FKL_VM_GET_ARG(VM, F, -1), &VM->base[VM->bp],
                value_count * sizeof(FklVMvalue *));
        VM->bp = F->bp;
        VM->tp = VM->bp + value_count;
        fklVMframeSetSp(VM, F, F->c.lr.lcount);

        if (F->c.lr.lrefl) {
            F->c.lr.lrefl = NULL;
            memset(F->c.lr.lref, 0, sizeof(FklVMvalue *) * F->c.lr.lcount);
        }
        F->c.pc = F->c.spc;
        F->c.mark = FKL_VM_COMPOUND_FRAME_MARK_RET;
    } break;
    case FKL_VM_COMPOUND_FRAME_MARK_LOOP:
        F->c.pc = F->c.spc;
        F->c.mark = FKL_VM_COMPOUND_FRAME_MARK_RET;
        break;
    default:
        fprintf(stderr, "[%s: %d] %s: unreachable!\n", __FILE__, __LINE__,
                __FUNCTION__);
        fklPrintBacktrace(exe, stderr);
        abort();
        break;
    }
#ifndef RETURN_HEADER
}
#endif

#undef F
#undef VM
#undef RETURN_INCLUDE
