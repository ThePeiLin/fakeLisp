#include"fuv.h"

#define PREDICATE(condition,err_infor) FKL_DECL_AND_CHECK_ARG(val,exe,err_infor);\
	FKL_CHECK_REST_ARG(exe,err_infor);\
	FKL_VM_PUSH_VALUE(exe,(condition)\
			?FKL_VM_TRUE\
			:FKL_VM_NIL);\
	return 0;

static int fuv_loop_p(FKL_CPROC_ARGL){PREDICATE(isFuvLoop(val),"fuv.loop?")}

static int fuv_make_loop(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.make-loop";
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* r=createFuvLoop(exe,ctx->proc);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int fuv_loop_close(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-close";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_loop_close(&fuv_loop->loop);
	if(r<0)
		RAISE_FUV_ERROR(Pname,FUV_EBUSY,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(r));
	return 0;
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	// uv_loop
	{"loop?",      fuv_loop_p,     },
	{"make-loop",  fuv_make_loop,  },
	{"loop-close", fuv_loop_close, },
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
	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklVMaddSymbolCstr(exe->gc,exports_and_func[i].sym)->id;
		FklVMcFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,NULL,id);
		loc[i]=dlproc;
	}
	return loc;
}
