#include<fakeLisp/vm.h>

static void interrupt_handler_mark(FklVMgc* gc,void* arg)
{
	fklVMgcToGray(arg,gc);
}

struct IntFrameCtx
{
	jmp_buf* buf;
};

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(struct IntFrameCtx);

#define DONE (1)
#define ERR_OCCUR (2)

static int int_frame_step(void* data,struct FklVM* exe)
{
	struct IntFrameCtx* ctx=data;
	longjmp(*ctx->buf,DONE);
}

static const FklVMframeContextMethodTable IntFrameCtxTable=
{
	.step=int_frame_step,
};

static int int_err_callback(struct FklVMframe* f,FklVMvalue* ev,struct FklVM* vm)
{
	struct IntFrameCtx* ctx=(struct IntFrameCtx*)f->data;
	FKL_VM_PUSH_VALUE(vm,ev);
	longjmp(*ctx->buf,ERR_OCCUR);
	return 0;
}

static FklVMinterruptResult interrupt_handler(FklVM* exe
		,FklVMvalue* value
		,FklVMvalue** pvalue
		,void* arg)
{
	jmp_buf buf;
	FklVMvalue* proc=arg;
	FklVMframe* origin_top_frame=exe->top_frame;
	FklVMframe* int_frame=fklCreateOtherObjVMframe(exe,&IntFrameCtxTable,exe->top_frame);
	((struct IntFrameCtx*)int_frame->data)->buf=&buf;
	exe->top_frame=int_frame;
	int_frame->errorCallBack=int_err_callback;
	uint32_t tp=exe->tp;
	uint32_t bp=exe->bp;
	fklSetBp(exe);
	FKL_VM_PUSH_VALUE(exe,value);
	fklCallObj(exe,proc);
	FklVMinterruptResult result=FKL_INT_DONE;
	exe->state=FKL_VM_READY;
	int r=setjmp(buf);
	if(r==DONE)
	{
		FklVMvalue* retval=FKL_VM_GET_TOP_VALUE(exe);
		if(retval!=FKL_VM_NIL)
			result=FKL_INT_NEXT;
	}
	else if(r==ERR_OCCUR)
	{
		*pvalue=FKL_VM_GET_TOP_VALUE(exe);
		result=FKL_INT_NEXT;
	}
	else
		exe->thread_run_cb(exe,origin_top_frame);
	exe->state=FKL_VM_READY;
	while(exe->top_frame!=origin_top_frame)
		fklPopVMframe(exe);
	exe->bp=bp;
	exe->tp=tp;
	return result;
}

static int int_rgintrl(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(proc_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(proc_obj,fklIsCallable,exe);
	fklVMpushInterruptHandlerLocal(exe,interrupt_handler,interrupt_handler_mark,NULL,proc_obj);
	FKL_VM_PUSH_VALUE(exe,proc_obj);
	return 0;
}

static int int_rgintrg(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(proc_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(proc_obj,fklIsCallable,exe);
	fklVMpushInterruptHandler(exe->gc,interrupt_handler,interrupt_handler_mark,NULL,proc_obj);
	FKL_VM_PUSH_VALUE(exe,proc_obj);
	return 0;
}

static int int_interrupt(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(value,exe);
	FKL_CHECK_REST_ARG(exe);
	fklVMinterrupt(exe,value,&value);
	FKL_VM_PUSH_VALUE(exe,value);
	return 0;
}

static int int_unrgintrl(FKL_CPROC_ARGL)
{
	FKL_CHECK_REST_ARG(exe);
	struct FklVMinterruptHandleList** l=&exe->int_list;
	struct FklVMinterruptHandleList* cur=*l;
	while(cur&&cur->int_handler!=interrupt_handler)
	{
		l=&cur->next;
		cur=*l;
	}
	if(cur)
	{
		*l=cur->next;
		FklVMvalue* retval=cur->int_handle_arg;
		fklDestroyVMinterruptHandlerList(cur);
		FKL_VM_PUSH_VALUE(exe,retval);
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int int_unrgintrg(FKL_CPROC_ARGL)
{
	FKL_CHECK_REST_ARG(exe);
	fklVMacquireInt(exe->gc);
	struct FklVMinterruptHandleList** l=&exe->gc->int_list;
	struct FklVMinterruptHandleList* cur=*l;
	while(cur&&cur->int_handler!=interrupt_handler)
	{
		l=&cur->next;
		cur=*l;
	}
	if(cur)
	{
		*l=cur->next;
		FklVMvalue* retval=cur->int_handle_arg;
		fklDestroyVMinterruptHandlerList(cur);
		FKL_VM_PUSH_VALUE(exe,retval);
	}
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	fklVMreleaseInt(exe->gc);
	return 0;
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	{"rgintrl",   int_rgintrl,   },
	{"rgintrg",   int_rgintrg,   },
	{"interrupt", int_interrupt, },
	{"unrgintrl", int_unrgintrl, },
	{"unrgintrg", int_unrgintrg, },

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
