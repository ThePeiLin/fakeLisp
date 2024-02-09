#include<fakeLisp/vm.h>

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(mutex_as_print,mutex);

static void mutex_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(mutex,uv_mutex_t,ud);
	uv_mutex_destroy(mutex);
}

static FklVMudMetaTable MutexUdMetaTable=
{
	.size=sizeof(uv_mutex_t),
	.__as_princ=mutex_as_print,
	.__as_prin1=mutex_as_print,
	.__finalizer=mutex_finalizer,
};

#define IS_MUTEX_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&MutexUdMetaTable)

static int sync_mutex_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.mutex?";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,IS_MUTEX_UD(obj)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int sync_make_mutex(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.make-mutex";
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* ud=fklCreateVMvalueUd(exe,&MutexUdMetaTable,ctx->proc);
	FKL_DECL_VM_UD_DATA(mutex,uv_mutex_t,ud);
	if(uv_mutex_init(mutex))
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	else
		FKL_VM_PUSH_VALUE(exe,ud);
	return 0;
}

static int sync_mutex_lock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.mutex-lock";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_MUTEX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(mutex,uv_mutex_t,obj);
	FKL_VM_PUSH_VALUE(exe,obj);
	fklUnlockThread(exe);
	uv_mutex_lock(mutex);
	fklLockThread(exe);
	return 0;
}

static int sync_mutex_unlock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.mutex-unlock";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_MUTEX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(mutex,uv_mutex_t,obj);
	uv_mutex_unlock(mutex);
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int sync_mutex_trylock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.mutex-trylock";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_MUTEX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(mutex,uv_mutex_t,obj);
	FKL_VM_PUSH_VALUE(exe,uv_mutex_trylock(mutex)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(cond_as_print,cond);

static void cond_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(cond,uv_cond_t,ud);
	uv_cond_destroy(cond);
}

static FklVMudMetaTable CondUdMetaTable=
{
	.size=sizeof(uv_cond_t),
	.__as_prin1=cond_as_print,
	.__as_princ=cond_as_print,
	.__finalizer=cond_finalizer,
};

#define IS_COND_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&CondUdMetaTable)

static int sync_cond_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.cond?";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,IS_COND_UD(obj)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int sync_make_cond(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.make-cond";
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* ud=fklCreateVMvalueUd(exe,&CondUdMetaTable,ctx->proc);
	FKL_DECL_VM_UD_DATA(cond,uv_cond_t,ud);
	if(uv_cond_init(cond))
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	else
		FKL_VM_PUSH_VALUE(exe,ud);
	return 0;
}

static int sync_cond_signal(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.cond-signal";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_COND_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(cond,uv_cond_t,obj);
	uv_cond_signal(cond);
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int sync_cond_broadcast(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.cond-broadcast";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_COND_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(cond,uv_cond_t,obj);
	uv_cond_broadcast(cond);
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int sync_cond_wait(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.cond-wait";
	FKL_DECL_AND_CHECK_ARG2(cond_obj,mutex_obj,exe,Pname);
	FklVMvalue* timeout_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(cond_obj,IS_COND_UD,Pname,exe);
	FKL_CHECK_TYPE(mutex_obj,IS_MUTEX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(cond,uv_cond_t,cond_obj);
	FKL_DECL_VM_UD_DATA(mutex,uv_mutex_t,mutex_obj);
	if(timeout_obj)
	{
		FKL_CHECK_TYPE(timeout_obj,fklIsVMint,Pname,exe);
		if(fklIsVMnumberLt0(timeout_obj))
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
		uint64_t timeout=fklGetUint(timeout_obj);
		uint32_t rtp=exe->tp;
		FKL_VM_PUSH_VALUE(exe,mutex_obj);
		FKL_VM_PUSH_VALUE(exe,cond_obj);
		fklUnlockThread(exe);
		if(uv_cond_timedwait(cond,mutex,timeout))
		{
			fklLockThread(exe);
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
		}
		fklLockThread(exe);
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,FKL_VM_NIL);
	}
	else
	{
		uint32_t rtp=exe->tp;
		FKL_VM_PUSH_VALUE(exe,mutex_obj);
		FKL_VM_PUSH_VALUE(exe,cond_obj);
		fklUnlockThread(exe);
		uv_cond_wait(cond,mutex);
		fklLockThread(exe);
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,FKL_VM_NIL);
	}
	return 0;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(rwlock_as_print,rwlock);

static void rwlock_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(rwlock,uv_rwlock_t,ud);
	uv_rwlock_destroy(rwlock);
}

static FklVMudMetaTable RwlockUdMetaTable=
{
	.size=sizeof(uv_rwlock_t),
	.__as_prin1=rwlock_as_print,
	.__as_princ=rwlock_as_print,
	.__finalizer=rwlock_finalizer,
};

#define IS_RWLOCK_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&RwlockUdMetaTable)

static int sync_rwlock_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.rwlock?";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,IS_RWLOCK_UD(obj)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int sync_make_rwlock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.make-rwlock";
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* ud=fklCreateVMvalueUd(exe,&RwlockUdMetaTable,ctx->proc);
	FKL_DECL_VM_UD_DATA(rwlock,uv_rwlock_t,ud);
	if(uv_rwlock_init(rwlock))
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	else
		FKL_VM_PUSH_VALUE(exe,ud);
	return 0;
}

static int sync_rwlock_rdlock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.rwlock-rdlock";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_RWLOCK_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(rwlock,uv_rwlock_t,obj);
	FKL_VM_PUSH_VALUE(exe,obj);
	fklUnlockThread(exe);
	uv_rwlock_rdlock(rwlock);
	fklLockThread(exe);
	return 0;
}

static int sync_rwlock_rdunlock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.rwlock-rdunlock";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_RWLOCK_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(rwlock,uv_rwlock_t,obj);
	uv_rwlock_rdunlock(rwlock);
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int sync_rwlock_tryrdlock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.rwlock-tryrdlock";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_RWLOCK_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(rwlock,uv_rwlock_t,obj);
	FKL_VM_PUSH_VALUE(exe,uv_rwlock_tryrdlock(rwlock)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int sync_rwlock_wrlock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.rwlock-wrlock";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_RWLOCK_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(rwlock,uv_rwlock_t,obj);
	FKL_VM_PUSH_VALUE(exe,obj);
	fklUnlockThread(exe);
	uv_rwlock_wrlock(rwlock);
	fklLockThread(exe);
	return 0;
}

static int sync_rwlock_wrunlock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.rwlock-wrunlock";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_RWLOCK_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(rwlock,uv_rwlock_t,obj);
	uv_rwlock_wrunlock(rwlock);
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int sync_rwlock_trywrlock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.rwlock-trywrlock";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_RWLOCK_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(rwlock,uv_rwlock_t,obj);
	FKL_VM_PUSH_VALUE(exe,uv_rwlock_trywrlock(rwlock)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(sem_as_print,sem);

static void sem_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(sem,uv_sem_t,ud);
	uv_sem_destroy(sem);
}

static FklVMudMetaTable SemUdMetaTable=
{
	.size=sizeof(uv_sem_t),
	.__as_prin1=sem_as_print,
	.__as_princ=sem_as_print,
	.__finalizer=sem_finalizer,
};

#define IS_SEM_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&SemUdMetaTable)

static int sync_sem_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.sem?";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,IS_SEM_UD(obj)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int sync_make_sem(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.make-sem";
	FKL_DECL_AND_CHECK_ARG(value_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(value_obj,fklIsVMint,Pname,exe);
	if(fklIsVMnumberLt0(value_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklVMvalue* ud=fklCreateVMvalueUd(exe,&SemUdMetaTable,ctx->proc);
	FKL_DECL_VM_UD_DATA(sem,uv_sem_t,ud);
	if(uv_sem_init(sem,fklGetUint(value_obj)))
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	else
		FKL_VM_PUSH_VALUE(exe,ud);
	return 0;
}

static int sync_sem_wait(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.sem-wait";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_SEM_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(sem,uv_sem_t,obj);
	FKL_VM_PUSH_VALUE(exe,obj);
	fklUnlockThread(exe);
	uv_sem_wait(sem);
	fklLockThread(exe);
	return 0;
}

static int sync_sem_post(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.sem-post";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_SEM_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(sem,uv_sem_t,obj);
	uv_sem_post(sem);
	FKL_VM_PUSH_VALUE(exe,obj);
	return 0;
}

static int sync_sem_trywait(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.sem-trywait";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_SEM_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(sem,uv_sem_t,obj);
	FKL_VM_PUSH_VALUE(exe,uv_sem_trywait(sem)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(barrier_as_print,barrier);

static void barrier_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(barrier,uv_barrier_t,ud);
	uv_barrier_destroy(barrier);
}

static FklVMudMetaTable BarrierUdMetaTable=
{
	.size=sizeof(uv_barrier_t),
	.__as_prin1=barrier_as_print,
	.__as_princ=barrier_as_print,
	.__finalizer=barrier_finalizer,
};

#define IS_BARRIER_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&BarrierUdMetaTable)

static int sync_barrier_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.barrier?";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,IS_BARRIER_UD(obj)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int sync_make_barrier(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.make-barrier";
	FKL_DECL_AND_CHECK_ARG(count_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(count_obj,fklIsVMint,Pname,exe);
	if(fklIsVMnumberLt0(count_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklVMvalue* ud=fklCreateVMvalueUd(exe,&BarrierUdMetaTable,ctx->proc);
	FKL_DECL_VM_UD_DATA(barrier,uv_barrier_t,ud);
	if(uv_barrier_init(barrier,fklGetUint(count_obj)))
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	else
		FKL_VM_PUSH_VALUE(exe,ud);
	return 0;
}

static int sync_barrier_wait(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.barrier-wait";
	FKL_DECL_AND_CHECK_ARG(obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_BARRIER_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(barrier,uv_barrier_t,obj);
	FKL_VM_PUSH_VALUE(exe,obj);
	fklUnlockThread(exe);
	uv_barrier_wait(barrier);
	fklLockThread(exe);
	return 0;
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	{"mutex?",           sync_mutex_p,          },
	{"make-mutex",       sync_make_mutex,       },
	{"mutex-lock",       sync_mutex_lock,       },
	{"mutex-trylock",    sync_mutex_trylock,    },
	{"mutex-unlock",     sync_mutex_unlock,     },

	{"rwlock?",          sync_rwlock_p,         },
	{"make-rwlock",      sync_make_rwlock,      },
	{"rwlock-rdlock",    sync_rwlock_rdlock,    },
	{"rwlock-tryrdlock", sync_rwlock_tryrdlock, },
	{"rwlock-rdunlock",  sync_rwlock_rdunlock,  },
	{"rwlock-wrlock",    sync_rwlock_wrlock,    },
	{"rwlock-trywrlock", sync_rwlock_trywrlock, },
	{"rwlock-wrunlock",  sync_rwlock_wrunlock,  },

	{"cond?",            sync_cond_p,           },
	{"make-cond",        sync_make_cond,        },
	{"cond-signal",      sync_cond_signal,      },
	{"cond-broadcast",   sync_cond_broadcast,   },
	{"cond-wait",        sync_cond_wait,        },

	{"sem?",             sync_sem_p,            },
	{"make-sem",         sync_make_sem,         },
	{"sem-wait",         sync_sem_wait,         },
	{"sem-trywait",      sync_sem_trywait,      },
	{"sem-post",         sync_sem_post,         },

	{"barrier?",         sync_barrier_p,        },
	{"make-barrier",     sync_make_barrier,     },
	{"barrier-wait",     sync_barrier_wait,     },
};

static const size_t EXPORT_NUM=sizeof(exports_and_func)/sizeof(struct SymFunc);

FKL_DLL_EXPORT void _fklExportSymbolInit(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS)
{
	*num=EXPORT_NUM;
	FklSid_t* symbols=(FklSid_t*)malloc(sizeof(FklSid_t)*EXPORT_NUM);
	FKL_ASSERT(symbols);
	for(size_t i=0;i<EXPORT_NUM;i++)
		symbols[i]=fklAddSymbolCstr(exports_and_func[i].sym,st)->id;
	*exports=symbols;
}

FKL_DLL_EXPORT FklVMvalue** _fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS)
{
	*count=EXPORT_NUM;
	FklVMvalue** loc=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*EXPORT_NUM);
	FKL_ASSERT(loc);
	fklVMacquireSt(exe->gc);
	FklSymbolTable* st=exe->gc->st;
	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklAddSymbolCstr(exports_and_func[i].sym,st)->id;
		FklVMcFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,NULL,id);
		loc[i]=dlproc;
	}
	fklVMreleaseSt(exe->gc);
	return loc;
}
