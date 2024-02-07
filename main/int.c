#include<fakeLisp/vm.h>

enum IntCallCtxState
{
	INT_RUN_OK=1,
	INT_RUN_ERR_OCCUR,
};

struct IntCallCtx
{
	jmp_buf* buf;
};

static void interrupt_handler_mark(FklVMgc* gc,void* arg)
{
	fklVMgcToGray(arg,gc);
}

static int int_call_frame_end(void* data)
{
	return 0;
}

static int int_call_frame_error_callback(FKL_VM_ERROR_CALLBACK_ARGL)
{
	struct IntCallCtx* ctx=(struct IntCallCtx*)f->data;
	FKL_VM_PUSH_VALUE(vm,ev);
	longjmp(*ctx->buf,INT_RUN_ERR_OCCUR);
	return 1;
}

static void int_call_frame_step(void* data,FklVM* exe)
{
	struct IntCallCtx* ctx=(struct IntCallCtx*)data;
	longjmp(*ctx->buf,INT_RUN_OK);
}

static void int_call_frame_printbacktrace(void* data,FILE* fp,FklVMgc* gc)
{
	fputs("at <interrupt>\n",fp);
}

static const FklVMframeContextMethodTable IntCallFrameCtxMethodTable=
{
	.print_backtrace=int_call_frame_printbacktrace,
	.end=int_call_frame_end,
	.step=int_call_frame_step,
};

static FklVMinterruptResult interrupt_handler(FklVM* exe
		,FklVMvalue* value
		,FklVMvalue** pvalue
		,void* arg)
{
	FklVMvalue* proc=arg;
	FklVMframe* origin_top_frame=exe->top_frame;
	FklVMframe* int_frame=fklCreateOtherObjVMframe(exe,&IntCallFrameCtxMethodTable,origin_top_frame);
	int_frame->errorCallBack=int_call_frame_error_callback;
	struct IntCallCtx* ctx=(struct IntCallCtx*)int_frame->data;
	jmp_buf buf;
	ctx->buf=&buf;
	exe->top_frame=int_frame;
	uint32_t tp=exe->tp;
	uint32_t bp=exe->bp;
	fklSetBp(exe);
	FKL_VM_PUSH_VALUE(exe,value);
	fklCallObj(exe,proc);
	FklVMinterruptResult result=FKL_INT_DONE;
	int r=setjmp(buf);
	if(r)
	{
		exe->state=FKL_VM_READY;
		while(exe->top_frame!=origin_top_frame)
			fklPopVMframe(exe);
		FklVMvalue* retval=FKL_VM_GET_TOP_VALUE(exe);
		if(r==INT_RUN_OK)
		{
			if(retval!=FKL_VM_NIL)
				result=FKL_INT_NEXT;
		}
		else
		{
			*pvalue=retval;
			result=FKL_INT_NEXT;
		}
		exe->bp=bp;
		exe->tp=tp;
	}
	else
	{
		exe->state=FKL_VM_READY;
		exe->thread_run_cb(exe);
	}
	return result;
}

static int int_rgintrl(FKL_CPROC_ARGL)
{
	static const char Pname[]="int.rgintrl";
	FKL_DECL_AND_CHECK_ARG(proc_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(proc_obj,fklIsCallable,Pname,exe);
	fklVMpushInterruptHandlerLocal(exe,interrupt_handler,interrupt_handler_mark,NULL,proc_obj);
	FKL_VM_PUSH_VALUE(exe,proc_obj);
	return 0;
}

static int int_rgintrg(FKL_CPROC_ARGL)
{
	static const char Pname[]="int.rgintrg";
	FKL_DECL_AND_CHECK_ARG(proc_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(proc_obj,fklIsCallable,Pname,exe);
	fklVMpushInterruptHandler(exe->gc,interrupt_handler,interrupt_handler_mark,NULL,proc_obj);
	FKL_VM_PUSH_VALUE(exe,proc_obj);
	return 0;
}

static int int_interrupt(FKL_CPROC_ARGL)
{
	static const char Pname[]="int.interrupt";
	FKL_DECL_AND_CHECK_ARG(value,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	fklVMinterrupt(exe,value,&value);
	FKL_VM_PUSH_VALUE(exe,value);
	return 0;
}

static int int_unrgintrl(FKL_CPROC_ARGL)
{
	static const char Pname[]="int.unrgintrl";
	FKL_CHECK_REST_ARG(exe,Pname);
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
	static const char Pname[]="int.unrgintrg";
	FKL_CHECK_REST_ARG(exe,Pname);
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
