#include<fakeLisp/vm.h>

FKL_VM_USER_DATA_DEFAULT_PRINT(mutex_print,mutex);

static void mutex_finalizer(FklVMudata* ud)
{
	FKL_DECL_UD_DATA(mutex,uv_mutex_t,ud);
	uv_mutex_destroy(mutex);
}

static FklVMudMetaTable MutexUdMetaTable=
{
	.size=sizeof(uv_mutex_t),
	.__prin1=mutex_print,
	.__princ=mutex_print,
	.__finalizer=mutex_finalizer,
};

#define IS_MUTEX_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&MutexUdMetaTable)

static int sync_mutex_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.mutex?";
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
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
	FklVMvalue* ud=fklCreateVMvalueUdata(exe,&MutexUdMetaTable,ctx->proc);
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
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_MUTEX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(mutex,uv_mutex_t,obj);
	FKL_VM_PUSH_VALUE(exe,obj);
	fklSuspendThread(exe);
	uv_mutex_lock(mutex);
	fklResumeThread(exe);
	return 0;
}

static int sync_mutex_unlock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.mutex-unlock";
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
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
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_MUTEX_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(mutex,uv_mutex_t,obj);
	FKL_VM_PUSH_VALUE(exe,uv_mutex_trylock(mutex)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(cond_print,cond);

static void cond_finalizer(FklVMudata* ud)
{
	FKL_DECL_UD_DATA(cond,uv_cond_t,ud);
	uv_cond_destroy(cond);
}

static FklVMudMetaTable CondUdMetaTable=
{
	.size=sizeof(uv_cond_t),
	.__prin1=cond_print,
	.__princ=cond_print,
	.__finalizer=cond_finalizer,
};

#define IS_COND_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&CondUdMetaTable)

static int sync_cond_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.cond?";
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
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
	FklVMvalue* ud=fklCreateVMvalueUdata(exe,&CondUdMetaTable,ctx->proc);
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
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
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
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
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
	FKL_DECL_AND_CHECK_ARG2(cond_obj,mutex_obj,Pname);
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
		fklSuspendThread(exe);
		if(uv_cond_timedwait(cond,mutex,timeout))
		{
			fklResumeThread(exe);
			FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
		}
		fklResumeThread(exe);
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,FKL_VM_NIL);
	}
	else
	{
		uint32_t rtp=exe->tp;
		FKL_VM_PUSH_VALUE(exe,mutex_obj);
		FKL_VM_PUSH_VALUE(exe,cond_obj);
		fklSuspendThread(exe);
		uv_cond_wait(cond,mutex);
		fklResumeThread(exe);
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,FKL_VM_NIL);
	}
	return 0;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(rwlock_print,rwlock);

static void rwlock_finalizer(FklVMudata* ud)
{
	FKL_DECL_UD_DATA(rwlock,uv_rwlock_t,ud);
	uv_rwlock_destroy(rwlock);
}

static FklVMudMetaTable RwlockUdMetaTable=
{
	.size=sizeof(uv_rwlock_t),
	.__prin1=rwlock_print,
	.__princ=rwlock_print,
	.__finalizer=rwlock_finalizer,
};

#define IS_RWLOCK_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&RwlockUdMetaTable)

static int sync_rwlock_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.rwlock?";
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
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
	FklVMvalue* ud=fklCreateVMvalueUdata(exe,&RwlockUdMetaTable,ctx->proc);
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
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_RWLOCK_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(rwlock,uv_rwlock_t,obj);
	FKL_VM_PUSH_VALUE(exe,obj);
	fklSuspendThread(exe);
	uv_rwlock_rdlock(rwlock);
	fklResumeThread(exe);
	return 0;
}

static int sync_rwlock_rdunlock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.rwlock-rdunlock";
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
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
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
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
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_RWLOCK_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(rwlock,uv_rwlock_t,obj);
	FKL_VM_PUSH_VALUE(exe,obj);
	fklSuspendThread(exe);
	uv_rwlock_wrlock(rwlock);
	fklResumeThread(exe);
	return 0;
}

static int sync_rwlock_wrunlock(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.rwlock-wrunlock";
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
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
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_RWLOCK_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(rwlock,uv_rwlock_t,obj);
	FKL_VM_PUSH_VALUE(exe,uv_rwlock_trywrlock(rwlock)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(sem_print,sem);

static void sem_finalizer(FklVMudata* ud)
{
	FKL_DECL_UD_DATA(sem,uv_sem_t,ud);
	uv_sem_destroy(sem);
}

static FklVMudMetaTable SemUdMetaTable=
{
	.size=sizeof(uv_sem_t),
	.__prin1=sem_print,
	.__princ=sem_print,
	.__finalizer=sem_finalizer,
};

#define IS_SEM_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&SemUdMetaTable)

static int sync_sem_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.sem?";
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,IS_SEM_UD(obj)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int sync_make_sem(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.make-sem";
	FKL_DECL_AND_CHECK_ARG(value_obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(value_obj,fklIsVMint,Pname,exe);
	if(fklIsVMnumberLt0(value_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FklVMvalue* ud=fklCreateVMvalueUdata(exe,&SemUdMetaTable,ctx->proc);
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
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_SEM_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(sem,uv_sem_t,obj);
	FKL_VM_PUSH_VALUE(exe,obj);
	fklSuspendThread(exe);
	uv_sem_wait(sem);
	fklResumeThread(exe);
	return 0;
}

static int sync_sem_post(FKL_CPROC_ARGL)
{
	static const char Pname[]="sync.sem-post";
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
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
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(obj,IS_SEM_UD,Pname,exe);
	FKL_DECL_VM_UD_DATA(sem,uv_sem_t,obj);
	FKL_VM_PUSH_VALUE(exe,uv_sem_trywait(sem)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
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
	FklSymbolTable* table=exe->symbolTable;
	*count=EXPORT_NUM;
	FklVMvalue** loc=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*EXPORT_NUM);
	FKL_ASSERT(loc);
	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklAddSymbolCstr(exports_and_func[i].sym,table)->id;
		FklVMcFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,NULL,id);
		loc[i]=dlproc;
	}
	return loc;
}
