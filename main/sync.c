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

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	{"make-mutex",    sync_make_mutex,    },
	{"mutex-lock",    sync_mutex_lock,    },
	{"mutex-trylock", sync_mutex_trylock, },
	{"mutex-unlock",  sync_mutex_unlock,  },
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
