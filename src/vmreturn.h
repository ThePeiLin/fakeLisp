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
    FklVMframe *F = VM->top_frame;
    FKL_ASSERT(VM->top_frame->type == FKL_FRAME_COMPOUND);
#endif

    if (F->ret_cb && F->ret_cb(VM, F))
        return;

    switch (F->mark) {
    case FKL_VM_COMPOUND_FRAME_MARK_RET: {
        VM->bp = FKL_GET_FIX(FKL_VM_GET_ARG(VM, F, -2));
        // copy stack values
        uint32_t const value_count = (VM->tp - F->sp);
        if (value_count > 1 || value_count < 1)
            goto return_value_err;
        memmove(&FKL_VM_GET_ARG(VM, F, -2),
                &VM->base[F->sp],
                value_count * sizeof(FklVMvalue *));
        VM->tp = F->bp - 1 + value_count;
        do_finalize_compound_frame(VM, popFrame(VM));
        return;
    return_value_err: {
        FklCodeBuilder builder = { 0 };
        fklInitCodeBuilderFp(&builder, stderr, NULL);
        fklCodeBuilderFmt(&builder,
                "[%s: %d] %s: the return value count should be 1, but is %u\n",
                __FILE__,
                __LINE__,
                __func__,
                value_count);
        fklPrintBacktrace(VM, &builder);
        abort();
    }
    } break;
    case FKL_VM_COMPOUND_FRAME_MARK_CALL: {
        close_all_var_ref(F);
        // copy stack values
        uint32_t const value_count = (VM->tp - VM->bp);
        memmove(&FKL_VM_GET_ARG(VM, F, -1),
                &VM->base[VM->bp],
                value_count * sizeof(FklVMvalue *));
        VM->bp = F->bp;
        VM->tp = VM->bp + value_count;
        fklVMframeSetSp(VM, F, F->lcount);

        if (F->lrefl) {
            F->lrefl = NULL;
            memset(F->lref, 0, sizeof(FklVMvalue *) * F->lcount);
        }
        F->pc = F->spc;
        F->mark = FKL_VM_COMPOUND_FRAME_MARK_RET;
    } break;
    default: {
        FklCodeBuilder builder = { 0 };
        fklInitCodeBuilderFp(&builder, stderr, NULL);
        fklCodeBuilderFmt(&builder,
                "[%s: %d] %s: unreachable!\n",
                __FILE__,
                __LINE__,
                __func__);
        fklPrintBacktrace(VM, &builder);
        abort();
    } break;
    }
#ifndef RETURN_HEADER
}
#endif

#undef F
#undef VM
#undef RETURN_INCLUDE
