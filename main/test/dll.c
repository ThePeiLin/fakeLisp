#include<fakeLisp/vm.h>

static void test_func(FKL_DL_PROC_ARGL)
{
	static const char* pname="test-func";
	if(fklResBp(exe))
		FKL_RAISE_BUILTIN_ERROR_CSTR(pname,FKL_ERR_TOOFEWARG,exe);
	fputs("testing dll\n",stdout);
	fklPushVMvalue(exe,FKL_VM_NIL);
}

struct SymFunc
{
	const char* sym;
	FklVMdllFunc f;
}exports[]=
{
	{"test-func",test_func},
};

static const size_t EXPORT_NUM=sizeof(exports)/sizeof(struct SymFunc);

void _fklExportSymbolInit(size_t* pnum,FklSid_t** psyms,FklSymbolTable* table)
{
	*pnum=EXPORT_NUM;
	FklSid_t* symbols=(FklSid_t*)malloc(sizeof(FklSid_t)*EXPORT_NUM);
	FKL_ASSERT(symbols);
	for(size_t i=0;i<EXPORT_NUM;i++)
		symbols[i]=fklAddSymbolCstr(exports[i].sym,table)->id;
	*psyms=symbols;
}

FklVMvalue** _fklImportInit(FklVM* exe,FklVMvalue* dll,uint32_t* pcount)
{
	FklSymbolTable* table=exe->symbolTable;
	*pcount=EXPORT_NUM;
	FklVMvalue** loc=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*EXPORT_NUM);
	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklAddSymbolCstr(exports[i].sym,table)->id;
		FklVMdllFunc func=exports[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueDlproc(exe,func,dll,NULL,id);
		loc[i]=dlproc;
	}
	return loc;
}
