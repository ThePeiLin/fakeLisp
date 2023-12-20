#include<fakeLisp/vm.h>
#include<fakeLisp/codegen.h>

typedef struct
{
	int end;
	FklCodegenOuterCtx outer_ctx;
	FklVM* main_thread;
	FklVMgc* gc;
}DebugCtx;

FKL_VM_USER_DATA_DEFAULT_PRINT(debug_ctx_print,debug-ctx);

static FklVMudMetaTable DebugCtxUdMetaTable=
{
	.size=sizeof(DebugCtx),
	.__prin1=debug_ctx_print,
	.__princ=debug_ctx_print,
};

static int debug_make_debug_ctx(FKL_CPROC_ARGL)
{
	abort();
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	{"make-debug-ctx",        debug_make_debug_ctx, },
	{"debug-ctx-cmd-read",    debug_make_debug_ctx, },
	{"debug-ctx-get-curline", debug_make_debug_ctx, },
	{"debug-ctx-end?",        debug_make_debug_ctx, },
	{"debug-ctx-step",        debug_make_debug_ctx, },
	{"debug-ctx-next",        debug_make_debug_ctx, },
	{"debug-ctx-del-break",   debug_make_debug_ctx, },
	{"debug-ctx-set-break",   debug_make_debug_ctx, },
	{"debug-ctx-continue",    debug_make_debug_ctx, },
	{"debug-ctx-exit",        debug_make_debug_ctx, },
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
