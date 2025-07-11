#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <uv.h>

typedef struct SyncPublicData {
#define XX(code, _) FklSid_t uv_err_sid_##code;
    UV_ERRNO_MAP(XX)
#undef XX
} SyncPublicData;

static FklVMudMetaTable SyncPublicDataMetaTable = {
    .size = sizeof(SyncPublicData),
};

static inline FklVMvalue *
create_uv_error(int err_id, FklVM *exe, SyncPublicData *pd) {
    FklSid_t id = 0;
    switch (err_id) {
#define XX(code, _)                                                            \
    case UV_##code:                                                            \
        id = pd->uv_err_sid_##code;                                            \
        break;
        UV_ERRNO_MAP(XX);
    default:
        id = pd->uv_err_sid_UNKNOWN;
    }
    return fklCreateVMvalueError(exe,
            id,
            fklCreateStringFromCstr(uv_strerror(err_id)));
#undef XX
}

noreturn static inline void
raiseUvError(int err_id, FklVM *exe, FklVMvalue *pd_obj) {
    FKL_DECL_VM_UD_DATA(pd, SyncPublicData, pd_obj);
    fklRaiseVMerror(create_uv_error(err_id, exe, pd), exe);
}

#define CHECK_UV_RESULT(R, EXE, PD)                                            \
    if ((R) < 0)                                                               \
    raiseUvError((R), (EXE), (PD))
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(mutex_as_print, "mutex");

static int mutex_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(mutex, uv_mutex_t, ud);
    uv_mutex_destroy(mutex);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable MutexUdMetaTable = {
    .size = sizeof(uv_mutex_t),
    .__as_princ = mutex_as_print,
    .__as_prin1 = mutex_as_print,
    .__finalizer = mutex_finalizer,
};

#define IS_MUTEX_UD(V)                                                         \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->t == &MutexUdMetaTable)

#define PREDICATE(condition)                                                   \
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);                                     \
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);                          \
    FKL_CPROC_RETURN(exe, ctx, (condition) ? FKL_VM_TRUE : FKL_VM_NIL);        \
    return 0;

static int sync_mutex_p(FKL_CPROC_ARGL) { PREDICATE(IS_MUTEX_UD(val)); }

static int sync_make_mutex(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    FklVMvalue *ud = fklCreateVMvalueUd(exe, &MutexUdMetaTable, ctx->proc);
    FKL_DECL_VM_UD_DATA(mutex, uv_mutex_t, ud);
    int r = uv_mutex_init(mutex);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

static int sync_mutex_lock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_MUTEX_UD, exe);
    FKL_DECL_VM_UD_DATA(mutex, uv_mutex_t, obj);
    fklUnlockThread(exe);
    uv_mutex_lock(mutex);
    fklLockThread(exe);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_mutex_unlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_MUTEX_UD, exe);
    FKL_DECL_VM_UD_DATA(mutex, uv_mutex_t, obj);
    uv_mutex_unlock(mutex);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_mutex_trylock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_MUTEX_UD, exe);
    FKL_DECL_VM_UD_DATA(mutex, uv_mutex_t, obj);
    FKL_CPROC_RETURN(exe,
            ctx,
            uv_mutex_trylock(mutex) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(cond_as_print, "cond");

static int cond_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(cond, uv_cond_t, ud);
    uv_cond_destroy(cond);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable CondUdMetaTable = {
    .size = sizeof(uv_cond_t),
    .__as_prin1 = cond_as_print,
    .__as_princ = cond_as_print,
    .__finalizer = cond_finalizer,
};

#define IS_COND_UD(V)                                                          \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->t == &CondUdMetaTable)

static int sync_cond_p(FKL_CPROC_ARGL) { PREDICATE(IS_COND_UD(val)); }

static int sync_make_cond(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    FklVMvalue *ud = fklCreateVMvalueUd(exe, &CondUdMetaTable, ctx->proc);
    FKL_DECL_VM_UD_DATA(cond, uv_cond_t, ud);
    int r = uv_cond_init(cond);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

static int sync_cond_signal(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_COND_UD, exe);
    FKL_DECL_VM_UD_DATA(cond, uv_cond_t, obj);
    uv_cond_signal(cond);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_cond_broadcast(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_COND_UD, exe);
    FKL_DECL_VM_UD_DATA(cond, uv_cond_t, obj);
    uv_cond_broadcast(cond);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_cond_wait(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *cond_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *mutex_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *timeout_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(cond_obj, IS_COND_UD, exe);
    FKL_CHECK_TYPE(mutex_obj, IS_MUTEX_UD, exe);
    FKL_DECL_VM_UD_DATA(cond, uv_cond_t, cond_obj);
    FKL_DECL_VM_UD_DATA(mutex, uv_mutex_t, mutex_obj);
    if (timeout_obj) {
        FKL_CHECK_TYPE(timeout_obj, fklIsVMint, exe);
        if (fklIsVMnumberLt0(timeout_obj))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        uint64_t timeout = fklVMgetUint(timeout_obj);
        fklUnlockThread(exe);
        int r = uv_cond_timedwait(cond, mutex, timeout);
        fklLockThread(exe);
        CHECK_UV_RESULT(r, exe, ctx->pd);
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    } else {
        fklUnlockThread(exe);
        uv_cond_wait(cond, mutex);
        fklLockThread(exe);
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    }
    return 0;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(rwlock_as_print, "rwlock");

static int rwlock_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(rwlock, uv_rwlock_t, ud);
    uv_rwlock_destroy(rwlock);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable RwlockUdMetaTable = {
    .size = sizeof(uv_rwlock_t),
    .__as_prin1 = rwlock_as_print,
    .__as_princ = rwlock_as_print,
    .__finalizer = rwlock_finalizer,
};

#define IS_RWLOCK_UD(V)                                                        \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->t == &RwlockUdMetaTable)

static int sync_rwlock_p(FKL_CPROC_ARGL) { PREDICATE(IS_RWLOCK_UD(val)); }

static int sync_make_rwlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    FklVMvalue *ud = fklCreateVMvalueUd(exe, &RwlockUdMetaTable, ctx->proc);
    FKL_DECL_VM_UD_DATA(rwlock, uv_rwlock_t, ud);
    int r = uv_rwlock_init(rwlock);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

static int sync_rwlock_rdlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);
    FKL_DECL_VM_UD_DATA(rwlock, uv_rwlock_t, obj);
    fklUnlockThread(exe);
    uv_rwlock_rdlock(rwlock);
    fklLockThread(exe);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_rwlock_rdunlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);
    FKL_DECL_VM_UD_DATA(rwlock, uv_rwlock_t, obj);
    uv_rwlock_rdunlock(rwlock);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_rwlock_tryrdlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);
    FKL_DECL_VM_UD_DATA(rwlock, uv_rwlock_t, obj);
    FKL_CPROC_RETURN(exe,
            ctx,
            uv_rwlock_tryrdlock(rwlock) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int sync_rwlock_wrlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);
    FKL_DECL_VM_UD_DATA(rwlock, uv_rwlock_t, obj);
    fklUnlockThread(exe);
    uv_rwlock_wrlock(rwlock);
    fklLockThread(exe);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_rwlock_wrunlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);
    FKL_DECL_VM_UD_DATA(rwlock, uv_rwlock_t, obj);
    uv_rwlock_wrunlock(rwlock);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_rwlock_trywrlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);
    FKL_DECL_VM_UD_DATA(rwlock, uv_rwlock_t, obj);
    FKL_CPROC_RETURN(exe,
            ctx,
            uv_rwlock_trywrlock(rwlock) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(sem_as_print, "sem");

static int sem_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(sem, uv_sem_t, ud);
    uv_sem_destroy(sem);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable SemUdMetaTable = {
    .size = sizeof(uv_sem_t),
    .__as_prin1 = sem_as_print,
    .__as_princ = sem_as_print,
    .__finalizer = sem_finalizer,
};

#define IS_SEM_UD(V) (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->t == &SemUdMetaTable)

static int sync_sem_p(FKL_CPROC_ARGL) { PREDICATE(IS_SEM_UD(val)); }

static int sync_make_sem(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *value_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(value_obj, fklIsVMint, exe);
    if (fklIsVMnumberLt0(value_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FklVMvalue *ud = fklCreateVMvalueUd(exe, &SemUdMetaTable, ctx->proc);
    FKL_DECL_VM_UD_DATA(sem, uv_sem_t, ud);
    int r = uv_sem_init(sem, fklVMgetUint(value_obj));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

static int sync_sem_wait(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_SEM_UD, exe);
    FKL_DECL_VM_UD_DATA(sem, uv_sem_t, obj);
    fklUnlockThread(exe);
    uv_sem_wait(sem);
    fklLockThread(exe);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_sem_post(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_SEM_UD, exe);
    FKL_DECL_VM_UD_DATA(sem, uv_sem_t, obj);
    uv_sem_post(sem);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_sem_trywait(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_SEM_UD, exe);
    FKL_DECL_VM_UD_DATA(sem, uv_sem_t, obj);
    FKL_CPROC_RETURN(exe, ctx, uv_sem_trywait(sem) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(barrier_as_print, "barrier");

static int barrier_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(barrier, uv_barrier_t, ud);
    uv_barrier_destroy(barrier);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable BarrierUdMetaTable = {
    .size = sizeof(uv_barrier_t),
    .__as_prin1 = barrier_as_print,
    .__as_princ = barrier_as_print,
    .__finalizer = barrier_finalizer,
};

#define IS_BARRIER_UD(V)                                                       \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->t == &BarrierUdMetaTable)

static int sync_barrier_p(FKL_CPROC_ARGL) { PREDICATE(IS_BARRIER_UD(val)); }

static int sync_make_barrier(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *count_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(count_obj, fklIsVMint, exe);
    if (fklIsVMnumberLt0(count_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FklVMvalue *ud = fklCreateVMvalueUd(exe, &BarrierUdMetaTable, ctx->proc);
    FKL_DECL_VM_UD_DATA(barrier, uv_barrier_t, ud);
    int r = uv_barrier_init(barrier, fklVMgetUint(count_obj));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

static int sync_barrier_wait(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_BARRIER_UD, exe);
    FKL_DECL_VM_UD_DATA(barrier, uv_barrier_t, obj);
    fklUnlockThread(exe);
    uv_barrier_wait(barrier);
    fklLockThread(exe);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

struct SymFunc {
    const char *sym;
    FklVMcFunc f;
} exports_and_func[] = {
    // clang-format off
    {"mutex?",           sync_mutex_p          },
    {"make-mutex",       sync_make_mutex       },
    {"mutex-lock",       sync_mutex_lock       },
    {"mutex-trylock",    sync_mutex_trylock    },
    {"mutex-unlock",     sync_mutex_unlock     },

    {"rwlock?",          sync_rwlock_p         },
    {"make-rwlock",      sync_make_rwlock      },
    {"rwlock-rdlock",    sync_rwlock_rdlock    },
    {"rwlock-tryrdlock", sync_rwlock_tryrdlock },
    {"rwlock-rdunlock",  sync_rwlock_rdunlock  },
    {"rwlock-wrlock",    sync_rwlock_wrlock    },
    {"rwlock-trywrlock", sync_rwlock_trywrlock },
    {"rwlock-wrunlock",  sync_rwlock_wrunlock  },

    {"cond?",            sync_cond_p           },
    {"make-cond",        sync_make_cond        },
    {"cond-signal",      sync_cond_signal      },
    {"cond-broadcast",   sync_cond_broadcast   },
    {"cond-wait",        sync_cond_wait        },

    {"sem?",             sync_sem_p            },
    {"make-sem",         sync_make_sem         },
    {"sem-wait",         sync_sem_wait         },
    {"sem-trywait",      sync_sem_trywait      },
    {"sem-post",         sync_sem_post         },

    {"barrier?",         sync_barrier_p        },
    {"make-barrier",     sync_make_barrier     },
    {"barrier-wait",     sync_barrier_wait     },
    // clang-format on
};

static const size_t EXPORT_NUM =
        sizeof(exports_and_func) / sizeof(struct SymFunc);

static inline void init_sync_public_data(SyncPublicData *pd,
        FklSymbolTable *st) {
#define XX(code, _)                                                            \
    pd->uv_err_sid_##code = fklAddSymbolCstr("UV_" #code, st)->v;
    UV_ERRNO_MAP(XX);
#undef XX
}

FKL_DLL_EXPORT FklSid_t *_fklExportSymbolInit(
        FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS) {
    *num = EXPORT_NUM;
    FklSid_t *symbols = (FklSid_t *)fklZmalloc(sizeof(FklSid_t) * EXPORT_NUM);
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklAddSymbolCstr(exports_and_func[i].sym, st)->v;
    return symbols;
}

FKL_DLL_EXPORT FklVMvalue **_fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    *count = EXPORT_NUM;
    FklVMvalue **loc =
            (FklVMvalue **)fklZmalloc(sizeof(FklVMvalue *) * EXPORT_NUM);
    FKL_ASSERT(loc);
    FklVMvalue *spd = fklCreateVMvalueUd(exe, &SyncPublicDataMetaTable, dll);
    FKL_DECL_VM_UD_DATA(pd, SyncPublicData, spd);

    fklVMacquireSt(exe->gc);
    FklSymbolTable *st = exe->gc->st;
    init_sync_public_data(pd, st);
    for (size_t i = 0; i < EXPORT_NUM; i++) {
        FklSid_t id = fklAddSymbolCstr(exports_and_func[i].sym, st)->v;
        FklVMcFunc func = exports_and_func[i].f;
        FklVMvalue *dlproc = fklCreateVMvalueCproc(exe, func, dll, spd, id);
        loc[i] = dlproc;
    }
    fklVMreleaseSt(exe->gc);
    return loc;
}
