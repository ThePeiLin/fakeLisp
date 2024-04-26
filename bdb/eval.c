#include"bdb.h"
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
	if(ln->scope==0)
		return NULL;
	FklCodegenEnv* env=getEnv(ctx,proc->protoId);
	if(env==NULL)
		return NULL;
	*origin_outer_env=env->prev;
	env->prev=NULL;
	FklCodegenEnv* new_env=fklCreateCodegenEnv(env,ln->scope,NULL);
	fklInitCodegenInfo(info
			,NULL
			,NULL
			,ctx->st
			,ctx->kt
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

	if(new_env)
	{
		replace_func_prototype(info
				,env->prototypeId
				,new_env
				,0
				,ctx->curline
				,ctx->st
				,ctx->temp_proc_prototype_id);
		return new_env;
	}
	return NULL;
}

static inline void set_back_origin_prev_env(FklCodegenEnv* new_env,FklCodegenEnv* origin_outer_env)
{
	FklCodegenEnv* env=new_env->prev;
	env->prev=origin_outer_env;
}

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
}

static inline int has_import_op_code(const FklByteCodelnt* lnt)
{
	const FklInstruction* cur=lnt->bc->code;
	const FklInstruction* const end=&cur[lnt->bc->len];
	for(;cur<end;cur++)
		if(cur->op==FKL_OP_LOAD_DLL||cur->op==FKL_OP_LOAD_LIB)
			return 1;
	return 0;
}

static inline FklVMvalue* compile_expression_common_helper(DebugCtx* ctx
		,FklCodegenInfo* info
		,FklCodegenEnv* tmp_env
		,FklNastNode* exp
		,FklCodegenEnv* origin_outer_env
		,FklVM* vm
		,FklVMframe* cur_frame
		,EvalCompileErr* err)
{
	FklByteCodelnt* code=fklGenExpressionCode(exp
			,tmp_env
			,info);
	fklDestroyNastNode(exp);
	set_back_origin_prev_env(tmp_env,origin_outer_env);
	FklVMvalue* proc=NULL;
	if(code)
	{
		if(has_import_op_code(code))
		{
			*err=EVAL_COMP_IMPORT;
			fklDestroyByteCodelnt(code);
			return NULL;
		}
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
		,FklVMframe* cur_frame
		,EvalCompileErr* err)
{
	FklCodegenInfo info;
	FklCodegenEnv* origin_outer_env=NULL;
	FklCodegenEnv* tmp_env=init_codegen_info_with_debug_ctx(ctx,&info,&origin_outer_env,cur_frame);
	if(tmp_env==NULL)
	{
		*err=EVAL_COMP_UNABLE;
		fklDestroyNastNode(exp);
		return NULL;
	}
	*err=0;
	fklMakeNastNodeRef(exp);
	FklVMvalue* proc=compile_expression_common_helper(ctx
			,&info
			,tmp_env
			,exp
			,origin_outer_env
			,vm
			,cur_frame
			,err);

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

	if(new_env)
	{
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
	return NULL;
}

FklVMvalue* compileConditionExpression(DebugCtx* ctx
		,FklVM* vm
		,FklNastNode* exp
		,FklVMframe* cur_frame
		,EvalCompileErr* err)
{
	FklCodegenInfo info;
	FklCodegenEnv* origin_outer_env=NULL;
	FklCodegenEnv* tmp_env=init_codegen_info_for_cond_bp_with_debug_ctx(ctx,&info,&origin_outer_env,cur_frame);
	if(tmp_env==NULL)
	{
		*err=EVAL_COMP_UNABLE;
		fklDestroyNastNode(exp);
		return NULL;
	}
	*err=0;
	fklMakeNastNodeRef(exp);
	FklVMvalue* proc=compile_expression_common_helper(ctx
			,&info
			,tmp_env
			,exp
			,origin_outer_env
			,vm
			,cur_frame
			,err);

	if(!proc)
		fklPushUintStack(tmp_env->prototypeId,&ctx->unused_prototype_id_for_cond_bp);
	info.pts=NULL;
	fklDestroyCodegenEnv(tmp_env);
	fklUninitCodegenInfo(&info);
	return proc;
}

FklVMvalue* callEvalProc(DebugCtx* ctx
		,FklVM* vm
		,FklVMvalue* proc
		,FklVMframe* origin_cur_frame)
{
	FklVMframe* origin_top_frame=vm->top_frame;
	FklVMvalue* retval=NULL;
	vm->state=FKL_VM_READY;
	uint32_t tp=vm->tp;
	uint32_t bp=vm->bp;
	uint32_t ltp=vm->ltp;
	fklSetVMsingleThread(vm);
	fklCallObj(vm,proc);
	fklDontNoticeThreadLock(vm);
	if(fklRunVMinSingleThread(vm,origin_cur_frame))
		fklPrintErrBacktrace(FKL_VM_POP_TOP_VALUE(vm),vm,stderr);
	else
		retval=FKL_VM_POP_TOP_VALUE(vm);
	vm->state=FKL_VM_READY;
	fklNoticeThreadLock(vm);
	fklUnsetVMsingleThread(vm);
	FklVMframe* f=vm->top_frame;
	while(f!=origin_cur_frame)
	{
		FklVMframe* cur=f;
		f=f->prev;
		fklDestroyVMframe(cur,vm);
	}
	vm->tp=tp;
	vm->bp=bp;
	vm->ltp=ltp;
	vm->top_frame=origin_top_frame;
	return retval;
}

static FKL_HASH_SET_KEY_VAL(env_set_key,uint32_t);

static int env_key_equal(const void* a,const void* b)
{
	return *((const uint32_t*)a)==*((const uint32_t*)b);
}

static uintptr_t env_hash_func(const void* k)
{
	return *(const uint32_t*)k;
}

static void env_uninit(void* d)
{
	EnvHashItem* dd=(EnvHashItem*)d;
	fklDestroyCodegenEnv(dd->env);
}

static FklHashTableMetaTable EnvMetaTable=
{
	.size=sizeof(EnvHashItem),
	.__getKey=fklHashDefaultGetKey,
	.__keyEqual=env_key_equal,
	.__hashFunc=env_hash_func,
	.__uninitItem=env_uninit,
	.__setKey=env_set_key,
	.__setVal=env_set_key,
};

void initEnvTable(FklHashTable* ht)
{
	fklInitHashTable(ht,&EnvMetaTable);
}

void putEnv(DebugCtx* ctx,FklCodegenEnv* env)
{
	EnvHashItem* i=fklPutHashItem(&env->prototypeId,&ctx->envs);
	i->env=env;
	env->refcount++;
}

FklCodegenEnv* getEnv(DebugCtx* ctx,uint32_t id)
{
	EnvHashItem* i=fklGetHashItem(&id,&ctx->envs);
	if(i)
		return i->env;
	return NULL;
}

