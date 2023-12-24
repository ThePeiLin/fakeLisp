#include<fakeLisp/vm.h>
#include<fakeLisp/codegen.h>

static uv_once_t debug_ctx_inited=UV_ONCE_INIT;
uv_mutex_t alive_debug_lock;
static FklPtrStack alive_debug_ctxs;

typedef struct
{
	uv_thread_t idler_thread_id;
	int end;
	FklCodegenOuterCtx outer_ctx;
	FklVM* main_thread;
	FklVMgc* gc;
	FklHashTable break_points;
	FklPtrStack code_objs;

	uv_mutex_t reach_breakpoint_lock;
	struct ReachBreakPoints
	{
		struct ReachBreakPoints* next;
		FklVM* vm;
		uv_cond_t cond;
	}* head;
	struct ReachBreakPoints** tail;
}DebugCtx;

typedef struct
{
	DebugCtx* ctx;
}DebugUdCtx;

FKL_VM_USER_DATA_DEFAULT_PRINT(debug_ctx_print,debug-ctx);

static void uninit_all_alive_debug_ctx(void)
{
	uv_mutex_destroy(&alive_debug_lock);
	fklUninitPtrStack(&alive_debug_ctxs);
	abort();
}

static void init_debug_ctx(void)
{
	atexit(uninit_all_alive_debug_ctx);
	uv_mutex_init(&alive_debug_lock);
	fklInitPtrStack(&alive_debug_ctxs,16,16);
}

static inline DebugCtx* create_debug_ctx(FklVM* exe,const char* file_dir)
{
	DebugCtx* ctx=(DebugCtx*)calloc(1,sizeof(DebugCtx));
	FKL_ASSERT(ctx);
	ctx->idler_thread_id=exe->tid;
	fklInitCodegenOuterCtx(&ctx->outer_ctx,fklCopyCstr(file_dir));
	uv_mutex_init(&ctx->reach_breakpoint_lock);

	uv_mutex_lock(&alive_debug_lock);
	fklPushPtrStack(ctx,&alive_debug_ctxs);
	uv_mutex_unlock(&alive_debug_lock);
	return ctx;
}

static FklVMudMetaTable DebugCtxUdMetaTable=
{
	.size=sizeof(DebugUdCtx),
	.__prin1=debug_ctx_print,
	.__princ=debug_ctx_print,
};

#define IS_DEBUG_CTX_UD(V) (FKL_IS_USERDATA(V)&&FKL_VM_UD(V)->t==&DebugCtxUdMetaTable)

static int bdb_make_debug_ctx(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.make-debug-ctx";
	FKL_DECL_AND_CHECK_ARG2(filename_obj,argv_obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(filename_obj,FKL_IS_STR,Pname,exe);
	FKL_CHECK_TYPE(argv_obj,fklIsList,Pname,exe);
	abort();
	return 0;
}

static int bdb_debug_ctx_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="bdb.debug-ctx?";
	FKL_DECL_AND_CHECK_ARG(obj,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_VM_PUSH_VALUE(exe,IS_DEBUG_CTX_UD(obj)
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
	{"debug-ctx?",            bdb_debug_ctx_p, },
	{"make-debug-ctx",        bdb_make_debug_ctx, },
	{"debug-ctx-cmd-read",    bdb_make_debug_ctx, },
	{"debug-ctx-get-curline", bdb_make_debug_ctx, },
	{"debug-ctx-end?",        bdb_make_debug_ctx, },
	{"debug-ctx-step",        bdb_make_debug_ctx, },
	{"debug-ctx-next",        bdb_make_debug_ctx, },
	{"debug-ctx-del-break",   bdb_make_debug_ctx, },
	{"debug-ctx-set-break",   bdb_make_debug_ctx, },
	{"debug-ctx-continue",    bdb_make_debug_ctx, },
	{"debug-ctx-exit",        bdb_make_debug_ctx, },
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
	uv_once(&debug_ctx_inited,init_debug_ctx);
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
