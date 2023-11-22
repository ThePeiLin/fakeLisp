#include<fakeLisp/vm.h>

static int test_func(FKL_DL_PROC_ARGL)
{
	static const char* pname="test-func";
	if(fklResBp(exe))
		FKL_RAISE_BUILTIN_ERROR_CSTR(pname,FKL_ERR_TOOFEWARG,exe);
	fputs("testing dll\n",stdout);
	fklPushVMvalue(exe,FKL_VM_NIL);
	return 0;
}

struct SymFunc
{
	const char* sym;
	FklVMdllFunc f;
}exports_and_func[]=
{
	{"test-func",test_func},
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
		FklVMdllFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueDlproc(exe,func,dll,NULL,id);
		loc[i]=dlproc;
	}
	return loc;
}
