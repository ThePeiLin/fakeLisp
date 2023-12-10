#include<fakeLisp/vm.h>

#define DECL_AND_CHECK_ARG(a,Pname) \
	FklVMvalue* a=FKL_VM_POP_ARG(exe);\
	if(!a)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);

#define DECL_AND_CHECK_ARG2(a,b,Pname) \
	FklVMvalue* a=FKL_VM_POP_ARG(exe);\
	FklVMvalue* b=FKL_VM_POP_ARG(exe);\
	if(!b||!a)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);

static int ht_hashv(FKL_CPROC_ARGL)
{
	static const char Pname[]="ht.hashv";
	DECL_AND_CHECK_ARG(key,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	uint64_t hashv=fklVMvalueEqHashv(key);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(hashv,exe));
	return 0;
}

static int ht_eqv_hashv(FKL_CPROC_ARGL)
{
	static const char Pname[]="ht.hashv";
	DECL_AND_CHECK_ARG(key,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	uint64_t hashv=fklVMvalueEqvHashv(key);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(hashv,exe));
	return 0;
}

static int ht_equal_hashv(FKL_CPROC_ARGL)
{
	static const char Pname[]="ht.equal-hashv";
	DECL_AND_CHECK_ARG(key,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	uint64_t hashv=fklVMvalueEqualHashv(key);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(hashv,exe));
	return 0;
}

typedef struct
{
	FklVMvalue* hash_func;
	FklVMvalue* eq_func;
	FklHashTable ht;
}HashTable;

static int ht_make_hashtable(FKL_CPROC_ARGL)
{
	static const char Pname[]="ht.make-hashtable";
	DECL_AND_CHECK_ARG2(hashv,equal,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	abort();
}


struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	{"hashv",          ht_hashv,          },
	{"eqv-hashv",      ht_eqv_hashv,      },
	{"equal-hashv",    ht_equal_hashv,    },
	{"make-hashtable", ht_make_hashtable, },
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
