#ifndef FOREACH_INCLUDED
#include "builtin.c"
#endif

#ifndef DEFAULT
#define DEFAULT FKL_VM_NIL
#endif

#ifndef EXIT
#define EXIT
#endif

#ifndef DISCARD_HEAD
static int builtin_foreach(FKL_CPROC_ARGL) {
#endif
    switch (ctx->context) {
    case 0: {
        FKL_DECL_AND_CHECK_ARG(proc, exe);
        FKL_CHECK_TYPE(proc, fklIsCallable, exe);
        size_t arg_num = FKL_VM_GET_ARG_NUM(exe);
        if (arg_num == 0)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);
        FklVMvalue **base = &FKL_VM_GET_VALUE(exe, arg_num);
        uint32_t rtp = fklResBpIn(exe, arg_num);
        fklResBp(exe);
        FKL_CHECK_TYPE(base[0], fklIsList, exe);
        size_t len = fklVMlistLength(base[0]);
        for (size_t i = 1; i < arg_num; i++) {
            FKL_CHECK_TYPE(base[i], fklIsList, exe);
            if (fklVMlistLength(base[i]) != len)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_LIST_DIFFER_IN_LENGTH, exe);
        }
        if (len == 0) {
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, rtp, DEFAULT);
            return 0;
        }
        ctx->context = 1;
        ctx->rtp = rtp;
        FKL_VM_GET_VALUE(exe, arg_num + 1) = FKL_VM_NIL;
        FKL_VM_PUSH_VALUE(exe, proc);
        fklSetBp(exe);
        for (FklVMvalue *const *const end = &base[arg_num]; base < end;
             base++) {
            FKL_VM_PUSH_VALUE(exe, FKL_VM_CAR(*base));
            *base = FKL_VM_CDR(*base);
        }
        fklCallObj(exe, proc);
        return 1;
    } break;
    case 1: {
        FklVMvalue *r = FKL_VM_POP_TOP_VALUE(exe);
        EXIT;
        size_t arg_num = exe->tp - ctx->rtp;
        FklVMvalue **base = &FKL_VM_GET_VALUE(exe, arg_num - 1);
        if (base[0] == FKL_VM_NIL)
            FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, r);
        else {
            FklVMvalue *proc = FKL_VM_GET_VALUE(exe, 1);
            fklSetBp(exe);
            for (FklVMvalue *const *const end = &base[arg_num - 2]; base < end;
                 base++) {
                FKL_VM_PUSH_VALUE(exe, FKL_VM_CAR(*base));
                *base = FKL_VM_CDR(*base);
            }
            fklCallObj(exe, proc);
            return 1;
        }
    } break;
    }
    return 0;
#ifndef DISCARD_HEAD
}
#endif

#undef FOREACH_INCLUDED
#undef DISCARD_HEAD
#undef DEFAULT
#undef EXIT
