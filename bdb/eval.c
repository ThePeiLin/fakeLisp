#include"bdb.h"
#include<fakeLisp/builtin.h>
#include<string.h>

static inline FklCodegenEnv* init_codegen_info_with_debug_ctx(DebugCtx* ctx
		,FklCodegenInfo* info
		,FklCodegenEnv** origin_outer_env)
{
	FklVMframe* f=ctx->reached_thread->top_frame;
	for(;f&&f->type==FKL_FRAME_OTHEROBJ;f=f->prev);
	if(f)
	{
		FklVMproc* proc=FKL_VM_PROC(f->c.proc);
		FklByteCodelnt* code=FKL_VM_CO(proc->codeObj);
		const FklLineNumberTableItem* ln=getCurLineNumberItemWithCp(f->c.pc,code);
		FklCodegenEnv* env=ctx->envs.base[proc->protoId-1];
		*origin_outer_env=env->prev;
		env->prev=NULL;
		FklCodegenEnv* new_env=fklCreateCodegenEnv(env,ln->scope,NULL);
		fklInitGlobCodegenEnv(new_env
				,ctx->st);
		fklInitCodegenInfo(info
				,NULL
				,new_env
				,NULL
				,ctx->st
				,0
				,0
				,0
				,&ctx->outer_ctx);
		fklDestroyFuncPrototypes(info->pts);
		info->pts=ctx->reached_thread->pts;
		fklCreateFuncPrototypeAndInsertToPool(info
				,env->prototypeId
				,new_env
				,0
				,ctx->curline
				,ctx->st);
		return new_env;
	}
	return NULL;
}

static inline void set_back_origin_prev_env(FklCodegenEnv* new_env,FklCodegenEnv* origin_outer_env)
{
	FklCodegenEnv* env=new_env->prev;
	env->prev=origin_outer_env;
}

static inline void copy_closure_from_main_proc(FklVMproc* proc
		,FklVMproc* main_proc)
{
	memcpy(proc->closure,main_proc->closure,sizeof(FklVMvalue*)*main_proc->rcount);
}

static inline void resolve_reference(DebugCtx* ctx
		,FklVM* vm
		,FklFuncPrototype* pt
		,FklVMvalue* proc_obj)
{
	FklVMvalue* main_proc_obj=getMainProc(ctx);
	FklVMproc* proc=FKL_VM_PROC(proc_obj);
	FklVMproc* main_proc=FKL_VM_PROC(main_proc_obj);
	proc->rcount=pt->rcount;
	proc->closure=(FklVMvalue**)calloc(proc->rcount,sizeof(FklVMvalue*));
	FKL_ASSERT(proc->closure);
	copy_closure_from_main_proc(proc,main_proc);
	fklCreateVMvalueClosureFrom(vm,proc->closure,vm->top_frame,main_proc->rcount,pt);
}

FklVMvalue* compileExpression(DebugCtx* ctx,FklNastNode* exp)
{
	FklCodegenInfo info;
	FklCodegenEnv* origin_outer_env=NULL;
	fklMakeNastNodeRef(exp);
	FklCodegenEnv* tmp_env=init_codegen_info_with_debug_ctx(ctx,&info,&origin_outer_env);
	tmp_env->refcount++;
	FklByteCodelnt* code=fklGenExpressionCode(exp
			,tmp_env
			,&info);
	fklDestroyNastNode(exp);
	set_back_origin_prev_env(tmp_env,origin_outer_env);
	FklVMvalue* proc=NULL;
	if(code)
	{
		FklFuncPrototypes* pts=info.pts;
		fklUpdatePrototype(pts,tmp_env,ctx->st,ctx->st);

		FklVM* vm=ctx->reached_thread;
		FklFuncPrototype* pt=&vm->pts->pts[tmp_env->prototypeId];
		FklVMvalue* code_obj=fklCreateVMvalueCodeObj(vm,code);
		proc=fklCreateVMvalueProcWithWholeCodeObj(vm
				,code_obj
				,tmp_env->prototypeId);
		resolve_reference(ctx,vm,pt,proc);
	}
	fklDestroyCodegenEnv(tmp_env);
	return proc;
}

typedef struct
{
	DebugCtx* ctx;
	FklVMvalue** store_retval;
}EvalFrameCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(EvalFrameCtx);

static int eval_frame_end(void* data)
{
	return 0;
}

static void eval_frame_step(void* data,FklVM* exe)
{
	EvalFrameCtx* c=(EvalFrameCtx*)data;
	*(c->store_retval)=FKL_VM_POP_TOP_VALUE(exe);
	longjmp(c->ctx->jmpb,DBG_INTERRUPTED);
	return;
}

static void eval_frame_finalizer(void* data)
{
}

static const FklVMframeContextMethodTable EvalFrameCtxMethodTable=
{
	.end=eval_frame_end,
	.step=eval_frame_step,
	.finalizer=eval_frame_finalizer,
};

static int eval_frame_error_callback(FklVMframe* f
		,FklVMvalue* ev
		,FklVM* exe)
{
	EvalFrameCtx* c=(EvalFrameCtx*)f->data;
	fklPrintErrBacktrace(ev,exe,stderr);
	longjmp(c->ctx->jmpb,DBG_ERROR_OCCUR);
	return 0;
}

static inline void init_eval_frame(FklVMframe* f,DebugCtx* ctx,FklVMvalue** store_retval)
{
	EvalFrameCtx* c=(EvalFrameCtx*)f->data;
	c->ctx=ctx;
	c->store_retval=store_retval;
}

static inline FklVMframe* create_eval_frame(DebugCtx* ctx
		,FklVMvalue** store_retval)
{
	FklVM* vm=ctx->reached_thread;
	FklVMframe* f=fklCreateOtherObjVMframe(vm,&EvalFrameCtxMethodTable,vm->top_frame);
	init_eval_frame(f,ctx,store_retval);
	f->errorCallBack=eval_frame_error_callback;
	return f;
}

FklVMvalue* callEvalProc(DebugCtx* ctx,FklVMvalue* proc)
{
	FklVM* vm=ctx->reached_thread;
	FklVMframe* origin_top_frame=vm->top_frame;
	FklVMvalue* retval=NULL;
	int r=setjmp(ctx->jmpb);
	if(r)
	{
		FklVMframe* f=vm->top_frame;
		while(f!=origin_top_frame)
		{
			FklVMframe* cur=f;
			f=f->prev;
			fklDestroyVMframe(cur,vm);
		}
		vm->top_frame=origin_top_frame;
	}
	else
	{
		vm->top_frame=create_eval_frame(ctx,&retval);
		fklCallObj(vm,proc);
		fklRunVMinSingleThread(vm);
	}
	return retval;
}

