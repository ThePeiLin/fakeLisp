#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <uv.h>

typedef struct SyncPublicData {
#define XX(code, _) FklVMvalue *uv_err_sid_##code;
    UV_ERRNO_MAP(XX)
#undef XX
} SyncPublicData;

FKL_VM_DEF_UD_STRUCT(FklVMvalueSyncPd, { SyncPublicData pd; });

static FKL_ALWAYS_INLINE FklVMvalueSyncPd *as_sync_pd(const FklVMvalue *v) {
    return FKL_TYPE_CAST(FklVMvalueSyncPd *, v);
}

static void sync_public_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(pd, SyncPublicData, ud);
    SyncPublicData *pd = &as_sync_pd(ud)->pd;

#define XX(code, _) fklVMgcToGray(pd->uv_err_sid_##code, gc);
    UV_ERRNO_MAP(XX)
#undef XX
}

static FklVMudMetaTable const SyncPublicDataMetaTable = {
    // .size = sizeof(SyncPublicData),
    .size = sizeof(FklVMvalueSyncPd),
    .__atomic = sync_public_ud_atomic,
};

static inline FklVMvalue *
create_uv_error(int err_id, FklVM *exe, SyncPublicData *pd) {
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
    FKL_DECL_VM_UD_DATA(pd, SyncPublicData, pd_obj);
    fklRaiseVMerror(create_uv_error(err_id, exe, pd), exe);
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

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(mutex_as_print, "mutex");

static int mutex_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(mutex, uv_mutex_t, ud);
    uv_mutex_t *mutex = &as_mutex(ud)->l;
    uv_mutex_destroy(mutex);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const MutexUdMetaTable = {
    // .size = sizeof(uv_mutex_t),
    .size = sizeof(FklVMvalueMutex),
    .__as_princ = mutex_as_print,
    .__as_prin1 = mutex_as_print,
    .__finalizer = mutex_finalizer,
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
    FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_mutex_lock(mutex); }
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

static FklVMudMetaTable const CondUdMetaTable;
#define IS_COND_UD(V)                                                          \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->mt_ == &CondUdMetaTable)

FKL_VM_DEF_UD_STRUCT(FklVMvalueCond, { uv_cond_t c; });

static FKL_ALWAYS_INLINE FklVMvalueCond *as_cond(const FklVMvalue *v) {
    FKL_ASSERT(IS_COND_UD(v));
    return FKL_TYPE_CAST(FklVMvalueCond *, v);
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(cond_as_print, "cond");

static int cond_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(cond, uv_cond_t, ud);
    uv_cond_t *cond = &as_cond(ud)->c;
    uv_cond_destroy(cond);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const CondUdMetaTable = {
    // .size = sizeof(uv_cond_t),
    .size = sizeof(FklVMvalueCond),
    .__as_prin1 = cond_as_print,
    .__as_princ = cond_as_print,
    .__finalizer = cond_finalizer,
};

static int sync_cond_p(FKL_CPROC_ARGL) { PREDICATE(IS_COND_UD(val)); }

static int sync_make_cond(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    FklVMvalue *ud = fklCreateVMvalueUd(exe,
            &CondUdMetaTable,
            FKL_VM_CPROC(ctx->proc)->dll);
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

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(rwlock_as_print, "rwlock");

static int rwlock_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(rwlock, uv_rwlock_t, ud);
    uv_rwlock_t *rwlock = &as_rwlock(ud)->l;
    uv_rwlock_destroy(rwlock);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const RwlockUdMetaTable = {
    // .size = sizeof(uv_rwlock_t),
    .size = sizeof(FklVMvalueRwlock),
    .__as_prin1 = rwlock_as_print,
    .__as_princ = rwlock_as_print,
    .__finalizer = rwlock_finalizer,
};

static int sync_rwlock_p(FKL_CPROC_ARGL) { PREDICATE(IS_RWLOCK_UD(val)); }

static int sync_make_rwlock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    FklVMvalue *ud = fklCreateVMvalueUd(exe,
            &RwlockUdMetaTable,
            FKL_VM_CPROC(ctx->proc)->dll);
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

    FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_rwlock_rdlock(rwlock); }

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
    FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_rwlock_wrlock(rwlock); }
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

FKL_VM_DEF_UD_STRUCT(FklVMvalueSem, { uv_sem_t s; });

#define IS_SEM_UD(V)                                                           \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->mt_ == &SemUdMetaTable)

static FklVMudMetaTable const SemUdMetaTable;
static FKL_ALWAYS_INLINE FklVMvalueSem *as_sem(const FklVMvalue *v) {
    FKL_ASSERT(IS_SEM_UD(v));
    return FKL_TYPE_CAST(FklVMvalueSem *, v);
}

static int sem_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(sem, uv_sem_t, ud);
    uv_sem_t *sem = &as_sem(ud)->s;
    uv_sem_destroy(sem);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const SemUdMetaTable = {
    // .size = sizeof(uv_sem_t),
    .size = sizeof(FklVMvalueSem),
    .__as_prin1 = sem_as_print,
    .__as_princ = sem_as_print,
    .__finalizer = sem_finalizer,
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
    FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_sem_wait(sem); }
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

FKL_VM_DEF_UD_STRUCT(FklVMvalueBarrier, { uv_barrier_t b; });
static FklVMudMetaTable const BarrierUdMetaTable;
#define IS_BARRIER_UD(V)                                                       \
    (FKL_IS_USERDATA(V) && FKL_VM_UD(V)->mt_ == &BarrierUdMetaTable)

static FKL_ALWAYS_INLINE FklVMvalueBarrier *as_barrier(const FklVMvalue *v) {
    FKL_ASSERT(IS_BARRIER_UD(v));
    return FKL_TYPE_CAST(FklVMvalueBarrier *, v);
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(barrier_as_print, "barrier");

static int barrier_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(barrier, uv_barrier_t, ud);
    uv_barrier_t *barrier = &as_barrier(ud)->b;
    uv_barrier_destroy(barrier);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const BarrierUdMetaTable = {
    // .size = sizeof(uv_barrier_t),
    .size = sizeof(FklVMvalueBarrier),
    .__as_prin1 = barrier_as_print,
    .__as_princ = barrier_as_print,
    .__finalizer = barrier_finalizer,
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
    FKL_VM_UNLOCK_BLOCK(exe, flag) { uv_barrier_wait(barrier); }
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

static inline void init_sync_public_data(SyncPublicData *pd, FklVM *vm) {
#define XX(code, _) pd->uv_err_sid_##code = fklVMaddSymbolCstr(vm, "UV_" #code);
    UV_ERRNO_MAP(XX);
#undef XX
}

FKL_DLL_EXPORT FklVMvalue **_fklExportSymbolInit(FklVMgc *gc, uint32_t *num) {
    *num = EXPORT_NUM;
    FklVMvalue **symbols =
            (FklVMvalue **)fklZmalloc(EXPORT_NUM * sizeof(FklVMvalue *));
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklVMaddSymbolCstr(&gc->gcvm, exports_and_func[i].sym);
    // fklAddSymbolCstr(exports_and_func[i].sym, st);
    return symbols;
}

FKL_DLL_EXPORT FklVMvalue **_fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    *count = EXPORT_NUM;
    FklVMvalue **loc =
            (FklVMvalue **)fklZmalloc(sizeof(FklVMvalue *) * EXPORT_NUM);
    FKL_ASSERT(loc);
    FklVMvalue *spd = fklCreateVMvalueUd(exe, &SyncPublicDataMetaTable, dll);
    FKL_DECL_VM_UD_DATA(pd, SyncPublicData, spd);

    init_sync_public_data(pd, exe);

    for (size_t i = 0; i < EXPORT_NUM; i++) {
        FklVMcFunc func = exports_and_func[i].f;
        FklVMvalue *dlproc = fklCreateVMvalueCproc(exe,
                func,
                dll,
                spd,
                exports_and_func[i].sym);
        loc[i] = dlproc;
    }
    return loc;
}
