#ifndef FOREACH_INCLUDED
#include <fakeLisp/vm.h>

#include "builtin.c"
#endif

#ifndef DEFAULT
#define DEFAULT FKL_VM_NIL
#endif

#ifndef EXIT
#define EXIT
#endif

#ifndef STORE_EXTRA_VALUE
#define STORE_EXTRA_VALUE
#endif

#ifndef MODIFY_EXTRA_VALUE
#define MODIFY_EXTRA_VALUE
#endif

#ifndef MAX_ARG_NUM
#define MAX_ARG_NUM (-1)
#endif

#ifndef DISCARD_HEAD
static int builtin_foreach(FKL_CPROC_ARGL) {
#endif
    switch (ctx->c[0].uptr) {
    case 0: {
        uint32_t const arg_num = FKL_CPROC_GET_ARG_NUM(exe, ctx);
        FKL_CPROC_CHECK_ARG_NUM2(exe, arg_num, 2, MAX_ARG_NUM);
        // FKL_DECL_AND_CHECK_ARG(proc, exe);
        FklVMvalue **arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *proc = arg_base[0];
        FKL_CHECK_TYPE(proc, fklIsCallable, exe);
        // size_t arg_num = FKL_VM_GET_ARG_NUM(exe);
        // if (arg_num == 0)
        //     FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);
        // FklVMvalue **base = &FKL_VM_GET_VALUE(exe, arg_num);
        // uint32_t rtp = fklResBpIn(exe, arg_num);
        // fklResBp(exe);
        // FKL_CHECK_TYPE(base[0], fklIsList, exe);
        FklVMvalue *first_list = arg_base[1];
        FKL_CHECK_TYPE(first_list, fklIsList, exe);
        // size_t len = fklVMlistLength(base[0]);
        size_t len = fklVMlistLength(first_list);
        // for (size_t i = 1; i < arg_num; i++) {
        for (uint32_t i = 2; i < arg_num; ++i) {
            // FKL_CHECK_TYPE(base[i], fklIsList, exe);
            FKL_CHECK_TYPE(arg_base[i], fklIsList, exe);
            // if (fklVMlistLength(base[i]) != len)
            if (fklVMlistLength(arg_base[i]) != len)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_LIST_DIFFER_IN_LENGTH, exe);
        }
        if (len == 0) {
            // FKL_VM_SET_TP_AND_PUSH_VALUE(exe, rtp, DEFAULT);
            FKL_CPROC_RETURN(exe, ctx, DEFAULT);
            return 0;
        }
        ctx->c[0].uptr = 1;
        // store arg_num
        ctx->c[1].u32a = arg_num;
        // ctx->rtp = rtp;
        // FKL_VM_GET_VALUE(exe, arg_num + 1) = FKL_VM_NIL;
        // FKL_VM_PUSH_VALUE(exe, proc);
        STORE_EXTRA_VALUE;
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, proc);
        fklVMstackReserve(exe, exe->tp + arg_num);
        arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue **const end = &arg_base[arg_num];
        FklVMvalue **plist = arg_base + 1;
        // for (FklVMvalue *const *const end = &base[arg_num]; base < end;
        //      base++) {
        //     FKL_VM_PUSH_VALUE(exe, FKL_VM_CAR(*base));
        //     *base = FKL_VM_CDR(*base);
        // }
        for (; plist < end; ++plist) {
            FKL_VM_PUSH_VALUE(exe, FKL_VM_CAR(*plist));
            *plist = FKL_VM_CDR(*plist);
        }
        fklCallObj(exe, proc);
        return 1;
    } break;
    case 1: {
        FklVMvalue *r = FKL_VM_POP_TOP_VALUE(exe);
        uint32_t const arg_num = ctx->c[1].u32a;
        MODIFY_EXTRA_VALUE;
        EXIT;
        // size_t arg_num = exe->tp - ctx->rtp;
        // FklVMvalue **base = &FKL_VM_GET_VALUE(exe, arg_num - 1);
        FklVMvalue **arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue **plist = arg_base + 1;
        // if (base[0] == FKL_VM_NIL)
        if (plist[0] == FKL_VM_NIL)
            // FKL_VM_SET_TP_AND_PUSH_VALUE(exe, ctx->rtp, r);
            FKL_CPROC_RETURN(exe, ctx, r);
        else {
            FklVMvalue *proc = arg_base[0];
            // FklVMvalue *proc = FKL_VM_GET_VALUE(exe, 1);
            fklSetBp(exe);
            FKL_VM_PUSH_VALUE(exe, proc);
            fklVMstackReserve(exe, exe->tp + arg_num);
            arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
            FklVMvalue **const end = &arg_base[arg_num];
            plist = arg_base + 1;
            // for (FklVMvalue *const *const end = &base[arg_num - 2]; base <
            // end;
            //      base++) {
            //     FKL_VM_PUSH_VALUE(exe, FKL_VM_CAR(*base));
            //     *base = FKL_VM_CDR(*base);
            // }
            for (; plist < end; ++plist) {
                FKL_VM_PUSH_VALUE(exe, FKL_VM_CAR(*plist));
                *plist = FKL_VM_CDR(*plist);
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

#undef STORE_EXTRA_VALUE
#undef MODIFY_EXTRA_VALUE
#undef FOREACH_INCLUDED
#undef MAX_ARG_NUM
#undef DISCARD_HEAD
#undef DEFAULT
#undef EXIT
