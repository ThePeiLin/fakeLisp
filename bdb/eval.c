#include"bdb.h"
#include "fakeLisp/codegen.h"
#include<fakeLisp/builtin.h>
#include<string.h>

static inline void replace_func_prototype(FklCodegenInfo* info
		,uint32_t p
		,FklCodegenEnv* env
		,FklSid_t sid
		,uint32_t line
		,FklSymbolTable* pst
		,uint32_t replaced_prototype_id)
{
	FklFuncPrototype* pt=&info->pts->pa[replaced_prototype_id];
	fklUninitFuncPrototype(pt);
	env->prototypeId=replaced_prototype_id;
	fklInitFuncPrototypeWithEnv(pt,info,env,sid,line,pst);
}

static inline FklCodegenEnv* init_codegen_info_common_helper(DebugCtx* ctx
		,FklCodegenInfo* info
		,FklCodegenEnv** origin_outer_env
		,FklVMframe* f
		,FklCodegenEnv** penv)
{
	FklVMproc* proc=FKL_VM_PROC(f->c.proc);
	FklByteCodelnt* code=FKL_VM_CO(proc->codeObj);
	const FklLineNumberTableItem* ln=getCurLineNumberItemWithCp(f->c.pc,code);
	FklCodegenEnv* env=ctx->envs.base[proc->protoId-1];
	*origin_outer_env=env->prev;
	env->prev=NULL;
	FklCodegenEnv* new_env=fklCreateCodegenEnv(env,ln->scope,NULL);
	// fklInitGlobCodegenEnv(new_env
	// 		,ctx->st);
	fklInitCodegenInfo(info
			,NULL
			,NULL
			,ctx->st
			,0
			,0
			,0
			,&ctx->outer_ctx);
	fklDestroyFuncPrototypes(info->pts);
	info->pts=ctx->gc->pts;
	*penv=env;
	new_env->refcount++;
	return new_env;
}

static inline FklCodegenEnv* init_codegen_info_with_debug_ctx(DebugCtx* ctx
		,FklCodegenInfo* info
		,FklCodegenEnv** origin_outer_env
		,FklVMframe* f)
{
	FklCodegenEnv* env=NULL;
	FklCodegenEnv* new_env=init_codegen_info_common_helper(ctx,info,origin_outer_env,f,&env);

	replace_func_prototype(info
			,env->prototypeId
			,new_env
			,0
			,ctx->curline
			,ctx->st
			,ctx->temp_proc_prototype_id);
	return new_env;
}

static inline void set_back_origin_prev_env(FklCodegenEnv* new_env,FklCodegenEnv* origin_outer_env)
{
	FklCodegenEnv* env=new_env->prev;
	env->prev=origin_outer_env;
}

// static inline void copy_closure_from_gc(FklVMvalue** refs
// 		,uint32_t i
// 		,uint32_t to
// 		,FklFuncPrototype* pt
// 		,FklVM* vm)
// {
// 	FklVMgc* gc=vm->gc;
// 	for(;i<to;i++)
// 	{
// 		FklSymbolDef* c=&pt->refs[i];
// 		if(c->cidx<FKL_BUILTIN_SYMBOL_NUM)
// 			refs[i]=gc->builtin_refs[c->cidx];
// 		else
// 			refs[i]=fklCreateClosedVMvalueVarRef(vm,NULL);
// 	}
// }

static inline void resolve_reference(DebugCtx* ctx
		,FklCodegenEnv* env
		,FklVM* vm
		,FklFuncPrototype* pt
		,FklVMvalue* proc_obj
		,FklVMframe* cur_frame)
{
	FklVMproc* proc=FKL_VM_PROC(proc_obj);
	uint32_t rcount=pt->rcount;
	proc->rcount=rcount;
	proc->closure=(FklVMvalue**)calloc(rcount,sizeof(FklVMvalue*));
	FKL_ASSERT(proc->closure);
	fklCreateVMvalueClosureFrom(vm,proc->closure,cur_frame,0,pt);
	FklPtrStack* urefs=&env->prev->uref;
	FklVMvalue** builtin_refs=vm->gc->builtin_refs;
	while(!fklIsPtrStackEmpty(urefs))
	{
		FklUnReSymbolRef* uref=fklPopPtrStack(urefs);
		FklSymbolDef* ref=fklGetCodegenRefBySid(uref->id,ctx->glob_env);
		if(ref)
			proc->closure[uref->idx]=builtin_refs[ref->cidx];
		free(uref);
	}
	// copy_closure_from_gc(proc->closure,start_count,rcount,pt,vm);
}

static inline FklVMvalue* compile_expression_common_helper(DebugCtx* ctx
		,FklCodegenInfo* info
		,FklCodegenEnv* tmp_env
		,FklNastNode* exp
		,FklCodegenEnv* origin_outer_env
		,FklVM* vm
		,FklVMframe* cur_frame)
{
	FklByteCodelnt* code=fklGenExpressionCode(exp
			,tmp_env
			,info);
	fklDestroyNastNode(exp);
	set_back_origin_prev_env(tmp_env,origin_outer_env);
	FklVMvalue* proc=NULL;
	if(code)
	{
		FklFuncPrototypes* pts=info->pts;
		fklUpdatePrototype(pts,tmp_env,ctx->st,ctx->st);

		FklFuncPrototype* pt=&pts->pa[tmp_env->prototypeId];
		FklVMvalue* code_obj=fklCreateVMvalueCodeObj(vm,code);
		proc=fklCreateVMvalueProcWithWholeCodeObj(vm
				,code_obj
				,tmp_env->prototypeId);
		resolve_reference(ctx,tmp_env,vm,pt,proc,cur_frame);
	}
	return proc;
}

FklVMvalue* compileEvalExpression(DebugCtx* ctx
		,FklVM* vm
		,FklNastNode* exp
		,FklVMframe* cur_frame)
{
	FklCodegenInfo info;
	FklCodegenEnv* origin_outer_env=NULL;
	fklMakeNastNodeRef(exp);
	FklCodegenEnv* tmp_env=init_codegen_info_with_debug_ctx(ctx,&info,&origin_outer_env,cur_frame);
	FklVMvalue* proc=compile_expression_common_helper(ctx
			,&info
			,tmp_env
			,exp
			,origin_outer_env
			,vm
			,cur_frame);

	info.pts=NULL;
	fklDestroyCodegenEnv(tmp_env);
	fklUninitCodegenInfo(&info);
	return proc;
}

static inline FklCodegenEnv* init_codegen_info_for_cond_bp_with_debug_ctx(DebugCtx* ctx
		,FklCodegenInfo* info
		,FklCodegenEnv** origin_outer_env
		,FklVMframe* f)
{
	FklCodegenEnv* env=NULL;
	FklCodegenEnv* new_env=init_codegen_info_common_helper(ctx,info,origin_outer_env,f,&env);

	if(fklIsUintStackEmpty(&ctx->unused_prototype_id_for_cond_bp))
		fklCreateFuncPrototypeAndInsertToPool(info
				,env->prototypeId
				,new_env
				,0
				,ctx->curline
				,ctx->st);
	else
	{
		uint32_t pid=fklPopUintStack(&ctx->unused_prototype_id_for_cond_bp);
		replace_func_prototype(info
				,env->prototypeId
				,new_env
				,0
				,ctx->curline
				,ctx->st
				,pid);
	}
	return new_env;
}

FklVMvalue* compileConditionExpression(DebugCtx* ctx
		,FklVM* vm
		,FklNastNode* exp
		,FklVMframe* cur_frame)
{
	FklCodegenInfo info;
	FklCodegenEnv* origin_outer_env=NULL;
	fklMakeNastNodeRef(exp);
	FklCodegenEnv* tmp_env=init_codegen_info_for_cond_bp_with_debug_ctx(ctx,&info,&origin_outer_env,cur_frame);
	FklVMvalue* proc=compile_expression_common_helper(ctx
			,&info
			,tmp_env
			,exp
			,origin_outer_env
			,vm
			,cur_frame);

	if(!proc)
		fklPushUintStack(tmp_env->prototypeId,&ctx->unused_prototype_id_for_cond_bp);
	info.pts=NULL;
	fklDestroyCodegenEnv(tmp_env);
	fklUninitCodegenInfo(&info);
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
	longjmp(c->ctx->jmpe,DBG_INTERRUPTED);
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
	fklPrintErrBacktrace(ev,exe,stdout);
	longjmp(c->ctx->jmpe,DBG_ERROR_OCCUR);
	return 0;
}

static inline void init_eval_frame(FklVMframe* f,DebugCtx* ctx,FklVMvalue** store_retval)
{
	EvalFrameCtx* c=(EvalFrameCtx*)f->data;
	c->ctx=ctx;
	c->store_retval=store_retval;
}

static inline FklVMframe* create_eval_frame(DebugCtx* ctx
		,FklVM* vm
		,FklVMvalue** store_retval
		,FklVMframe* cur_frame)
{
	FklVMframe* f=fklCreateOtherObjVMframe(vm,&EvalFrameCtxMethodTable,cur_frame);
	init_eval_frame(f,ctx,store_retval);
	f->errorCallBack=eval_frame_error_callback;
	return f;
}

FklVMvalue* callEvalProc(DebugCtx* ctx
		,FklVM* vm
		,FklVMvalue* proc
		,FklVMframe* origin_cur_frame)
{
	FklVMframe* origin_top_frame=vm->top_frame;
	FklVMvalue* retval=NULL;
	int r=setjmp(ctx->jmpe);
	if(r)
	{
		vm->state=FKL_VM_READY;
		vm->is_single_thread=0;
		FklVMframe* f=vm->top_frame;
		while(f!=origin_cur_frame)
		{
			FklVMframe* cur=f;
			f=f->prev;
			fklDestroyVMframe(cur,vm);
		}
		vm->top_frame=origin_top_frame;
	}
	else
	{
		vm->state=FKL_VM_READY;
		vm->is_single_thread=1;
		vm->top_frame=create_eval_frame(ctx,vm,&retval,origin_cur_frame);
		fklCallObj(vm,proc);
		fklRunVMinSingleThread(vm);
	}
	return retval;
}

