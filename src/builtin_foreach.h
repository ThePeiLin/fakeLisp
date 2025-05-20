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
#define FOREACH_CALL_STATE_ENTER (0)
#define FOREACH_CALL_STATE_CONT (1)
#define FOREACH_CTX_STATE(ctx) (ctx)->c[0].uptr
#define FOREACH_CTX_ARG_NUM(ctx) (ctx)->c[1].u32a

    switch (FOREACH_CTX_STATE(ctx)) {
    case FOREACH_CALL_STATE_ENTER: {
        FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, MAX_ARG_NUM);
        FklVMvalue **arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *proc = arg_base[0];
        FKL_CHECK_TYPE(proc, fklIsCallable, exe);
        FklVMvalue *first_list = arg_base[1];
        FKL_CHECK_TYPE(first_list, fklIsList, exe);
        size_t len = fklVMlistLength(first_list);
        for (uint32_t i = 2; i < argc; ++i) {
            FKL_CHECK_TYPE(arg_base[i], fklIsList, exe);
            if (fklVMlistLength(arg_base[i]) != len)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_LIST_DIFFER_IN_LENGTH, exe);
        }
        if (len == 0) {
            FKL_CPROC_RETURN(exe, ctx, DEFAULT);
            return 0;
        }
        FOREACH_CTX_STATE(ctx) = FOREACH_CALL_STATE_CONT;
        FOREACH_CTX_ARG_NUM(ctx) = argc;
        STORE_EXTRA_VALUE;
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, proc);
        fklVMstackReserve(exe, exe->tp + argc);
        arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue **const end = &arg_base[argc];
        FklVMvalue **plist = arg_base + 1;
        for (; plist < end; ++plist) {
            FKL_VM_PUSH_VALUE(exe, FKL_VM_CAR(*plist));
            *plist = FKL_VM_CDR(*plist);
        }
        fklCallObj(exe, proc);
        return 1;
    } break;
    case FOREACH_CALL_STATE_CONT: {
        FklVMvalue *r = FKL_VM_POP_TOP_VALUE(exe);
        uint32_t const arg_num = FOREACH_CTX_ARG_NUM(ctx);
        MODIFY_EXTRA_VALUE;
        EXIT;
        FklVMvalue **arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue **plist = arg_base + 1;
        if (plist[0] == FKL_VM_NIL)
            FKL_CPROC_RETURN(exe, ctx, r);
        else {
            FklVMvalue *proc = arg_base[0];
            fklSetBp(exe);
            FKL_VM_PUSH_VALUE(exe, proc);
            fklVMstackReserve(exe, exe->tp + arg_num);
            arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
            FklVMvalue **const end = &arg_base[arg_num];
            plist = arg_base + 1;
            for (; plist < end; ++plist) {
                FKL_VM_PUSH_VALUE(exe, FKL_VM_CAR(*plist));
                *plist = FKL_VM_CDR(*plist);
            }
            fklCallObj(exe, proc);
            return 1;
        }
    } break;
    default:
        fprintf(stderr, "[%s: %d] %s: unreachable!\n", __FILE__, __LINE__,
                __FUNCTION__);
        abort();
        break;
    }
#undef FOREACH_CALL_STATE_ENTER
#undef FOREACH_CALL_STATE_CONT
#undef FOREACH_CTX_STATE
#undef FOREACH_CTX_ARG_NUM
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
