#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <uv.h>

#define XX(code, _) FklVMvalue *uv_err_sid_##code;
FKL_VM_DEF_UD_STRUCT(FklVMvalueSyncPd, {
    struct {
        UV_ERRNO_MAP(XX)
    };
});
#undef XX

static FKL_ALWAYS_INLINE FklVMvalueSyncPd *as_sync_pd(const FklVMvalue *v) {
    return FKL_TYPE_CAST(FklVMvalueSyncPd *, v);
}

static void sync_public_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueSyncPd *pd = as_sync_pd(ud);

#define XX(code, _) fklVMgcToGray(pd->uv_err_sid_##code, gc);
    UV_ERRNO_MAP(XX)
#undef XX
}

static FklVMudMetaTable const SyncPublicDataMetaTable = {
    .size = sizeof(FklVMvalueSyncPd),
    .atomic = sync_public_ud_atomic,
};

static inline FklVMvalue *
create_uv_error(int err_id, FklVM *exe, FklVMvalueSyncPd *pd) {
    FklVMvalue *id = 0;
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
            fklCreateVMvalueStrFromCstr(exe, uv_strerror(err_id)));
#undef XX
}

noreturn static inline void
raiseUvError(int err_id, FklVM *exe, FklVMvalue *pd_obj) {
    fklRaiseVMerror(create_uv_error(err_id, exe, as_sync_pd(pd_obj)), exe);
}

#define CHECK_UV_RESULT(R, EXE, PD)                                            \
    if ((R) < 0)                                                               \
    raiseUvError((R), (EXE), (PD))

static FklVMudMetaTable const MutexUdMetaTable;
FKL_VM_DEF_UD_STRUCT(FklVMvalueMutex, { uv_mutex_t l; });

#define IS_MUTEX_UD(V)                                                         \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->mt_ == &MutexUdMetaTable)

static FKL_ALWAYS_INLINE FklVMvalueMutex *as_mutex(const FklVMvalue *v) {
    FKL_ASSERT(IS_MUTEX_UD(v));
    return FKL_TYPE_CAST(FklVMvalueMutex *, v);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(mutex_print, "mutex");

static int mutex_finalize(FklVMvalue *ud, FklVMgc *gc) {
    uv_mutex_t *mutex = &as_mutex(ud)->l;
    uv_mutex_destroy(mutex);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const MutexUdMetaTable = {
    .size = sizeof(FklVMvalueMutex),
    .princ = mutex_print,
    .prin1 = mutex_print,
    .finalize = mutex_finalize,
};

#define PREDICATE(condition)                                                   \
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);                                     \
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);                          \
    FKL_CPROC_RETURN(exe, ctx, (condition) ? FKL_VM_TRUE : FKL_VM_NIL);        \
    return 0;

static int sync_mutex_p(FKL_CPROC_ARGL) { PREDICATE(IS_MUTEX_UD(val)); }

static int sync_make_mutex(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    FklVMvalue *ud = fklCreateVMvalueUd(exe,
            &MutexUdMetaTable,
            FKL_VM_CPROC(ctx->proc)->dll);
    uv_mutex_t *mutex = &as_mutex(ud)->l;
    int r = uv_mutex_init(mutex);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

static int sync_mutex_lock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_MUTEX_UD, exe);
    FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_mutex_lock(&as_mutex(obj)->l); }
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_mutex_unlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_MUTEX_UD, exe);
    uv_mutex_unlock(&as_mutex(obj)->l);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_mutex_trylock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_MUTEX_UD, exe);
    FKL_CPROC_RETURN(exe,
            ctx,
            uv_mutex_trylock(&as_mutex(obj)->l) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static FklVMudMetaTable const CondUdMetaTable;
#define IS_COND_UD(V)                                                          \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->mt_ == &CondUdMetaTable)

FKL_VM_DEF_UD_STRUCT(FklVMvalueCond, { uv_cond_t c; });

static FKL_ALWAYS_INLINE FklVMvalueCond *as_cond(const FklVMvalue *v) {
    FKL_ASSERT(IS_COND_UD(v));
    return FKL_TYPE_CAST(FklVMvalueCond *, v);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(cond_print, "cond");

static int cond_finalize(FklVMvalue *ud, FklVMgc *gc) {
    uv_cond_t *cond = &as_cond(ud)->c;
    uv_cond_destroy(cond);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const CondUdMetaTable = {
    .size = sizeof(FklVMvalueCond),
    .prin1 = cond_print,
    .princ = cond_print,
    .finalize = cond_finalize,
};

static int sync_cond_p(FKL_CPROC_ARGL) { PREDICATE(IS_COND_UD(val)); }

static int sync_make_cond(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    FklVMvalue *ud = fklCreateVMvalueUd(exe,
            &CondUdMetaTable,
            FKL_VM_CPROC(ctx->proc)->dll);
    int r = uv_cond_init(&as_cond(ud)->c);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

static int sync_cond_signal(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_COND_UD, exe);
    uv_cond_signal(&as_cond(obj)->c);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_cond_broadcast(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_COND_UD, exe);
    uv_cond_broadcast(&as_cond(obj)->c);
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
    uv_cond_t *cond = &as_cond(cond_obj)->c;
    uv_mutex_t *mutex = &as_mutex(mutex_obj)->l;
    if (timeout_obj) {
        FKL_CHECK_TYPE(timeout_obj, fklIsVMint, exe);
        if (fklIsVMnumberLt0(timeout_obj))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        uint64_t timeout = fklVMgetUint(timeout_obj);
        int r;
        FKL_VM_UNLOCK_BLOCK(exe, flag) {
            r = uv_cond_timedwait(cond, mutex, timeout);
        }
        CHECK_UV_RESULT(r, exe, ctx->pd);
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    } else {
        FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_cond_wait(cond, mutex); }
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    }
    return 0;
}

FKL_VM_DEF_UD_STRUCT(FklVMvalueRwlock, { uv_rwlock_t l; });

static FklVMudMetaTable const RwlockUdMetaTable;
#define IS_RWLOCK_UD(V)                                                        \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->mt_ == &RwlockUdMetaTable)

static FKL_ALWAYS_INLINE FklVMvalueRwlock *as_rwlock(const FklVMvalue *v) {
    FKL_ASSERT(IS_RWLOCK_UD(v));
    return FKL_TYPE_CAST(FklVMvalueRwlock *, v);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(rwlock_print, "rwlock");

static int rwlock_finalize(FklVMvalue *ud, FklVMgc *gc) {
    uv_rwlock_t *rwlock = &as_rwlock(ud)->l;
    uv_rwlock_destroy(rwlock);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const RwlockUdMetaTable = {
    .size = sizeof(FklVMvalueRwlock),
    .prin1 = rwlock_print,
    .princ = rwlock_print,
    .finalize = rwlock_finalize,
};

static int sync_rwlock_p(FKL_CPROC_ARGL) { PREDICATE(IS_RWLOCK_UD(val)); }

static int sync_make_rwlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    FklVMvalue *ud = fklCreateVMvalueUd(exe,
            &RwlockUdMetaTable,
            FKL_VM_CPROC(ctx->proc)->dll);
    int r = uv_rwlock_init(&as_rwlock(ud)->l);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

static int sync_rwlock_rdlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);

    FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_rwlock_rdlock(&as_rwlock(obj)->l); }

    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_rwlock_rdunlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);
    uv_rwlock_rdunlock(&as_rwlock(obj)->l);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_rwlock_tryrdlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);
    FKL_CPROC_RETURN(exe,
            ctx,
            uv_rwlock_tryrdlock(&as_rwlock(obj)->l) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int sync_rwlock_wrlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);
    FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_rwlock_wrlock(&as_rwlock(obj)->l); }
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_rwlock_wrunlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);
    uv_rwlock_wrunlock(&as_rwlock(obj)->l);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_rwlock_trywrlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_RWLOCK_UD, exe);
    FKL_CPROC_RETURN(exe,
            ctx,
            uv_rwlock_trywrlock(&as_rwlock(obj)->l) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(sem_print, "sem");

FKL_VM_DEF_UD_STRUCT(FklVMvalueSem, { uv_sem_t s; });

#define IS_SEM_UD(V)                                                           \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->mt_ == &SemUdMetaTable)

static FklVMudMetaTable const SemUdMetaTable;
static FKL_ALWAYS_INLINE FklVMvalueSem *as_sem(const FklVMvalue *v) {
    FKL_ASSERT(IS_SEM_UD(v));
    return FKL_TYPE_CAST(FklVMvalueSem *, v);
}

static int sem_finalize(FklVMvalue *ud, FklVMgc *gc) {
    uv_sem_t *sem = &as_sem(ud)->s;
    uv_sem_destroy(sem);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const SemUdMetaTable = {
    .size = sizeof(FklVMvalueSem),
    .prin1 = sem_print,
    .princ = sem_print,
    .finalize = sem_finalize,
};

static int sync_sem_p(FKL_CPROC_ARGL) { PREDICATE(IS_SEM_UD(val)); }

static int sync_make_sem(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *value_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(value_obj, fklIsVMint, exe);
    if (fklIsVMnumberLt0(value_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FklVMvalue *ud = fklCreateVMvalueUd(exe,
            &SemUdMetaTable,
            FKL_VM_CPROC(ctx->proc)->dll);
    int r = uv_sem_init(&as_sem(ud)->s, fklVMgetUint(value_obj));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

static int sync_sem_wait(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_SEM_UD, exe);
    FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_sem_wait(&as_sem(obj)->s); }
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_sem_post(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_SEM_UD, exe);
    uv_sem_post(&as_sem(obj)->s);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int sync_sem_trywait(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_SEM_UD, exe);
    FKL_CPROC_RETURN(exe,
            ctx,
            uv_sem_trywait(&as_sem(obj)->s) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

FKL_VM_DEF_UD_STRUCT(FklVMvalueBarrier, { uv_barrier_t b; });
static FklVMudMetaTable const BarrierUdMetaTable;
#define IS_BARRIER_UD(V)                                                       \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->mt_ == &BarrierUdMetaTable)

static FKL_ALWAYS_INLINE FklVMvalueBarrier *as_barrier(const FklVMvalue *v) {
    FKL_ASSERT(IS_BARRIER_UD(v));
    return FKL_TYPE_CAST(FklVMvalueBarrier *, v);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(barrier_print, "barrier");

static int barrier_finalize(FklVMvalue *ud, FklVMgc *gc) {
    uv_barrier_t *barrier = &as_barrier(ud)->b;
    uv_barrier_destroy(barrier);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const BarrierUdMetaTable = {
    .size = sizeof(FklVMvalueBarrier),
    .prin1 = barrier_print,
    .princ = barrier_print,
    .finalize = barrier_finalize,
};

static int sync_barrier_p(FKL_CPROC_ARGL) { PREDICATE(IS_BARRIER_UD(val)); }

static int sync_make_barrier(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *count_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(count_obj, fklIsVMint, exe);
    if (fklIsVMnumberLt0(count_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FklVMvalue *ud = fklCreateVMvalueUd(exe,
            &BarrierUdMetaTable,
            FKL_VM_CPROC(ctx->proc)->dll);
    int r = uv_barrier_init(&as_barrier(ud)->b, fklVMgetUint(count_obj));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, ud);
    return 0;
}

static int sync_barrier_wait(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, IS_BARRIER_UD, exe);
    FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_barrier_wait(&as_barrier(obj)->b); }
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

static inline void init_sync_public_data(FklVMvalueSyncPd *pd, FklVM *vm) {
#define XX(code, _) pd->uv_err_sid_##code = fklVMaddSymbolCstr(vm, "UV_" #code);
    UV_ERRNO_MAP(XX);
#undef XX
}

FKL_DLL_EXPORT FklVMvalue **_fklExportSymbolInit(FklVM *vm, uint32_t *num) {
    *num = EXPORT_NUM;
    FklVMvalue **symbols =
            (FklVMvalue **)fklZmalloc(EXPORT_NUM * sizeof(FklVMvalue *));
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklVMaddSymbolCstr(vm, exports_and_func[i].sym);
    return symbols;
}

FKL_DLL_EXPORT int _fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    FKL_ASSERT(count == EXPORT_NUM);
    FklVMvalue *spd = fklCreateVMvalueUd(exe, &SyncPublicDataMetaTable, dll);

    init_sync_public_data(as_sync_pd(spd), exe);

    for (size_t i = 0; i < EXPORT_NUM; i++) {
        FklVMcFunc func = exports_and_func[i].f;
        FklVMvalue *dlproc = fklCreateVMvalueCproc(exe,
                func,
                dll,
                spd,
                exports_and_func[i].sym);
        values[i] = dlproc;
    }
    return 0;
}

FKL_CHECK_EXPORT_DLL_INIT_FUNC();
FKL_CHECK_IMPORT_DLL_INIT_FUNC();
