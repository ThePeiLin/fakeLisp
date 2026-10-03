#include "fakeLisp/vm.h"
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
        VM->bp = (uint32_t)FKL_GET_FIX(FKL_VM_GET_ARG(VM, F, FKL_VM_BP_IDX));
        // copy stack values
        uint32_t const value_count = (VM->tp - F->sp);
        if (value_count > 1 || value_count < 1)
            goto return_value_err;
        memmove(&FKL_VM_GET_ARG(VM, F, FKL_VM_BP_IDX),
                &VM->base[F->sp],
                value_count * sizeof(FklVMvalue *));
        VM->tp = F->bp - 1 + value_count;
        do_finalize_compound_frame(VM, popFrame(VM));
        return;

    return_value_err:;
        FklStrBuilder builder = { 0 };
        fklInitStrBuilderFp(&builder, stderr, NULL);
        fklStrBuilderFmt(&builder,
                "[%s: %d] %s: the return value count should be 1, but is %u\n",
                __FILE__,
                __LINE__,
                __func__,
                value_count);
        fklPrintBacktrace(VM, &builder);
        abort();
    } break;
    case FKL_VM_COMPOUND_FRAME_MARK_CALL: {
        close_all_var_ref(F);
        // copy stack values
        FklVMvalue **const v_start = &VM->base[VM->bp + FKL_VM_FRAME_SIZE];
        uint32_t const v_count = &VM->base[VM->tp] - v_start;
        VM->bp = F->bp;
        VM->tp = VM->bp + v_count + FKL_VM_FRAME_SIZE;

        FklVMframe *ff = fklPrepCall(VM, VM->bp, v_count, v_start);
        FKL_ASSERT(ff == (F));
        if (FKL_UNLIKELY(fklVMframeSetSp(VM, F, F->lcount) != 0)) {
            F->mark = FKL_VM_COMPOUND_FRAME_MARK_RET;
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_STACK_OVERFLOW, exe);
        }

        F->lref = FKL_VM_NIL;
        F->pc = F->spc;
        F->mark = FKL_VM_COMPOUND_FRAME_MARK_RET;
    } break;
    default: {
        FklStrBuilder builder = { 0 };
        fklInitStrBuilderFp(&builder, stderr, NULL);
        fklStrBuilderFmt(&builder,
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
