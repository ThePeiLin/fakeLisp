#include "fakeLisp/base.h"
#include<fakeLisp/vm.h>

#define PREDICATE(condition) FKL_DECL_AND_CHECK_ARG(val,exe);\
	FKL_CHECK_REST_ARG(exe);\
	FKL_VM_PUSH_VALUE(exe,(condition)\
			?FKL_VM_TRUE\
			:FKL_VM_NIL);\
	return 0;

static int buf_strbuf_equal(const FklVMud* a,const FklVMud* b)
{
	FKL_DECL_UD_DATA(bufa,FklStringBuffer,a);
	FKL_DECL_UD_DATA(bufb,FklStringBuffer,b);
	return bufa->index==bufb->index&&!memcmp(bufa->buf,bufb->buf,bufa->index);
}

static void buf_strbuf_finalizer(FklVMud* p)
{
	FKL_DECL_UD_DATA(buf,FklStringBuffer,p);
	fklUninitStringBuffer(buf);
}

static void buf_strbuf_as_prin1(const FklVMud* ud,FklStringBuffer* buf,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(bufa,FklStringBuffer,ud);
	fklPrintRawCharBufToStringBuffer(buf,bufa->index,bufa->buf,'"');
}

static void buf_strbuf_as_princ(const FklVMud* ud,FklStringBuffer* buf,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(bufa,FklStringBuffer,ud);
	fklStringBufferConcatWithStringBuffer(buf,bufa);
}

static FklVMudMetaTable StringBufferMetaTable=
{
	.size=sizeof(FklStringBuffer),
	.__equal=buf_strbuf_equal,
	.__as_prin1=buf_strbuf_as_prin1,
	.__as_princ=buf_strbuf_as_princ,
	.__finalizer=buf_strbuf_finalizer,
};

static inline int is_strbuf_ud(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)
		&&FKL_VM_UD(v)->t==&StringBufferMetaTable;
}

static int buf_strbuf_p(FKL_CPROC_ARGL){PREDICATE(is_strbuf_ud(val))}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	// stringbuffer
	{"strbuf?",buf_strbuf_p,},
};

static const size_t EXPORT_NUM=sizeof(exports_and_func)/sizeof(struct SymFunc);

FKL_DLL_EXPORT FklSid_t* _fklExportSymbolInit(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS)
{
	*num=EXPORT_NUM;
	FklSid_t* symbols=(FklSid_t*)malloc(sizeof(FklSid_t)*EXPORT_NUM);
	FKL_ASSERT(symbols);
	for(size_t i=0;i<EXPORT_NUM;i++)
		symbols[i]=fklAddSymbolCstr(exports_and_func[i].sym,st)->id;
	return symbols;
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
