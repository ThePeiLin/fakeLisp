#include"bdb.h"
#include<fakeLisp/builtin.h>

static inline void init_cmd_read_ctx(CmdReadCtx* ctx)
{
	ctx->replxx=replxx_init();
	fklInitStringBuffer(&ctx->buf);
	fklInitPtrStack(&ctx->symbolStack,16,16);
	fklInitPtrStack(&ctx->stateStack,16,16);
	fklInitUintStack(&ctx->lineStack,16,16);
	fklVMvaluePushState0ToStack(&ctx->stateStack);
}

static inline int init_debug_codegen_outer_ctx(DebugCtx* ctx,const char* filename)
{
	FILE* fp=fopen(filename,"r");
	char* rp=fklRealpath(filename);
	FklCodegenOuterCtx* outer_ctx=&ctx->outer_ctx;
	FklCodegenInfo* codegen=&ctx->main_info;
	fklInitCodegenOuterCtx(outer_ctx,fklGetDir(rp));
	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	fklAddSymbolCstr(filename,pst);
	fklInitGlobalCodegenInfo(codegen,rp,pst,0,outer_ctx);
	free(rp);
	FklByteCodelnt* mainByteCode=fklGenExpressionCodeWithFp(fp,codegen);
	if(mainByteCode==NULL)
	{
		fklUninitCodegenInfo(codegen);
		fklUninitCodegenOuterCtx(outer_ctx);
		return 1;
	}
	fklUpdatePrototype(codegen->pts
			,codegen->globalEnv
			,codegen->globalSymTable
			,pst);
	fklPrintUndefinedRef(codegen->globalEnv,codegen->globalSymTable,pst);

	FklPtrStack* scriptLibStack=codegen->libStack;
	FklVM* anotherVM=fklCreateVM(mainByteCode,codegen->globalSymTable,codegen->pts,1);
	anotherVM->libNum=scriptLibStack->top;
	anotherVM->libs=(FklVMlib*)calloc((scriptLibStack->top+1),sizeof(FklVMlib));
	FKL_ASSERT(anotherVM->libs);
	FklVMframe* mainframe=anotherVM->top_frame;
	fklInitGlobalVMclosure(mainframe,anotherVM);
	fklInitMainVMframeWithProc(anotherVM,mainframe
			,FKL_VM_PROC(fklGetCompoundFrameProc(mainframe))
			,NULL
			,anotherVM->pts);
	FklVMCompoundFrameVarRef* lr=&mainframe->c.lr;

	FklVMgc* gc=anotherVM->gc;
	while(!fklIsPtrStackEmpty(scriptLibStack))
	{
		FklVMlib* curVMlib=&anotherVM->libs[scriptLibStack->top];
		FklCodegenLib* cur=fklPopPtrStack(scriptLibStack);
		FklCodegenLibType type=cur->type;
		fklInitVMlibWithCodegenLib(cur,curVMlib,anotherVM,1,anotherVM->pts);
		if(type==FKL_CODEGEN_LIB_SCRIPT)
			fklInitMainProcRefs(FKL_VM_PROC(curVMlib->proc),lr->ref,lr->rcount);
	}
	fklChdir(outer_ctx->cwd);

	ctx->gc=gc;
	ctx->main_thread=anotherVM;

	return 0;
}

static inline void set_argv_with_list(FklVMgc* gc,FklVMvalue* argv_list)
{
	int argc=fklVMlistLength(argv_list);
	gc->argc=argc;
	char** argv=(char**)malloc(sizeof(char*)*argc);
	FKL_ASSERT(argv);
	for(int i=0;i<argc;i++)
	{
		argv[i]=fklStringToCstr(FKL_VM_STR(FKL_VM_CAR(argv_list)));
		argv_list=FKL_VM_CDR(argv_list);
	}
	gc->argv=argv;
}

DebugCtx* createDebugCtx(FklVM* exe,const char* filename,FklVMvalue* argv)
{
	DebugCtx* ctx=(DebugCtx*)calloc(1,sizeof(DebugCtx));
	FKL_ASSERT(ctx);
	ctx->idler_thread_id=exe->tid;
	if(init_debug_codegen_outer_ctx(ctx,filename))
	{
		free(ctx);
		return NULL;
	}
	uv_mutex_init(&ctx->reach_breakpoint_lock);
	set_argv_with_list(ctx->gc,argv);
	init_cmd_read_ctx(&ctx->read_ctx);

	return ctx;
}

