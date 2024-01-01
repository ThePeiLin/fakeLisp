#include<fakeLisp/codegen.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/common.h>
#include<fakeLisp/builtin.h>
#include<ctype.h>
#include<string.h>
#ifdef _WIN32
#include<direct.h>
#include<io.h>
#include<process.h>
#else
#include<unistd.h>
#endif

static inline FklNastNode* cadr_nast_node(const FklNastNode* node)
{
	return node->pair->cdr->pair->car;
}

static inline int isExportDefmacroExp(const FklNastNode* c,FklNastNode* const* builtin_pattern_node)
{
	return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFMACRO],c,NULL)
		&&cadr_nast_node(c)->type!=FKL_NAST_VECTOR;
}

static inline int isExportDefReaderMacroExp(const FklNastNode* c,FklNastNode* const* builtin_pattern_node)
{
	return (fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFMACRO],c,NULL)
			&&cadr_nast_node(c)->type==FKL_NAST_VECTOR)
		||fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEF_READER_MACROS],c,NULL);
}

static inline int isExportDefineExp(const FklNastNode* c,FklNastNode* const* builtin_pattern_node)
{
	return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFINE],c,NULL);
}

static inline int isBeginExp(const FklNastNode* c,FklNastNode* const* builtin_pattern_node)
{
	return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_BEGIN],c,NULL);
}

static inline int isExportDefunExp(const FklNastNode* c,FklNastNode* const* builtin_pattern_node)
{
	return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFUN],c,NULL);
}

static inline int isNonRetvalExp(const FklNastNode* c,FklNastNode* const* builtin_pattern_node)
{
	for(size_t i=FKL_CODEGEN_PATTERN_DEFMACRO;i<FKL_CODEGEN_PATTERN_NUM;i++)
		if(fklPatternMatch(builtin_pattern_node[i],c,NULL))
			return 1;
	return 0;
}

static inline int isExportImportExp(FklNastNode* c,FklNastNode* const* builtin_pattern_node)
{
	return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_PREFIX],c,NULL)
		||fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_ONLY],c,NULL)
		||fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_ALIAS],c,NULL)
		||fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_EXCEPT],c,NULL)
		||fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_COMMON],c,NULL)
		||fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT],c,NULL)
		||fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_NONE],c,NULL);
}

static void _default_stack_context_finalizer(void* data)
{
	FklPtrStack* s=data;
	while(!fklIsPtrStackEmpty(s))
		fklDestroyByteCodelnt(fklPopPtrStack(s));
	fklDestroyPtrStack(s);
}

static void _default_stack_context_put_bcl(void* data,FklByteCodelnt* bcl)
{
	FklPtrStack* s=data;
	fklPushPtrStack(bcl,s);
}

static FklPtrStack* _default_stack_context_get_bcl_stack(void* data)
{
	return data;
}

static const FklCodegenQuestContextMethodTable DefaultStackContextMethodTable=
{
	.__finalizer=_default_stack_context_finalizer,
	.__put_bcl=_default_stack_context_put_bcl,
	.__get_bcl_stack=_default_stack_context_get_bcl_stack,
};

static FklCodegenQuestContext* createCodegenQuestContext(void* data,const FklCodegenQuestContextMethodTable* t)
{
	FklCodegenQuestContext* r=(FklCodegenQuestContext*)malloc(sizeof(FklCodegenQuestContext));
	FKL_ASSERT(r);
	r->data=data;
	r->t=t;
	return r;
}

static FklCodegenQuestContext* createDefaultStackContext(FklPtrStack* s)
{
	return createCodegenQuestContext(s,&DefaultStackContextMethodTable);
}

static void _empty_context_finalizer(void* data)
{
}

static const FklCodegenQuestContextMethodTable EmptyContextMethodTable=
{
	.__finalizer=_empty_context_finalizer,
	.__get_bcl_stack=NULL,
	.__put_bcl=NULL,
};

static FklCodegenQuestContext* createEmptyContext(void)
{
	return createCodegenQuestContext(NULL,&EmptyContextMethodTable);
}

#define DO_NOT_NEED_RETVAL (0)
#define ALL_MUST_HAS_RETVAL (1)
#define FIRST_MUST_HAS_RETVAL (2)

static FklCodegenNextExpression* createCodegenNextExpression(const FklNextExpressionMethodTable* t
		,void* context
		,uint8_t must_has_retval)
{
	FklCodegenNextExpression* r=(FklCodegenNextExpression*)malloc(sizeof(FklCodegenNextExpression));
	FKL_ASSERT(r);
	r->t=t;
	r->context=context;
	r->must_has_retval=must_has_retval;
	return r;
}

static FklNastNode* _default_codegen_get_next_expression(void* context,FklCodegenErrorState* errorState)
{
	return fklPopPtrQueue((FklPtrQueue*)context);
}

static void _default_codegen_next_expression_finalizer(void* context)
{
	while(!fklIsPtrQueueEmpty(context))
		fklDestroyNastNode(fklPopPtrQueue(context));
	fklDestroyPtrQueue(context);
}

static const FklNextExpressionMethodTable _default_codegen_next_expression_method_table=
{
	.getNextExpression=_default_codegen_get_next_expression,
	.finalizer=_default_codegen_next_expression_finalizer,
};

static FklCodegenNextExpression* createDefaultQueueNextExpression(FklPtrQueue* queue)
{
	return createCodegenNextExpression(&_default_codegen_next_expression_method_table
			,queue
			,DO_NOT_NEED_RETVAL);
}

static FklCodegenNextExpression* createMustHasRetvalQueueNextExpression(FklPtrQueue* queue)
{
	return createCodegenNextExpression(&_default_codegen_next_expression_method_table
			,queue
			,ALL_MUST_HAS_RETVAL);
}

static FklCodegenNextExpression* createFirstHasRetvalQueueNextExpression(FklPtrQueue* queue)
{
	return createCodegenNextExpression(&_default_codegen_next_expression_method_table
			,queue
			,FIRST_MUST_HAS_RETVAL);
}

static const char* builtInSubPattern[]=
{
	"~(unquote ~value)",
	"~(unqtesp ~value)",
	"~(define ~value)",
	"~(defmacro ~value)",
	"~(import ~value)",
	"~(and,~rest)",
	"~(or,~rest)",
	"~(not ~value)",
	"~(eq ~arg0 ~arg1)",
	"~(match ~arg0 ~arg1)",
	NULL,
};                                      

static inline FklInstruction create_op_ins(FklOpcode op)
{
	return (FklInstruction){.op=op};
}

static inline FklInstruction create_op_imm_u32_ins(FklOpcode op,uint32_t imm)
{
	return (FklInstruction){.op=op,.imm_u32=imm};
}

static inline FklInstruction create_op_imm_u32_u64_ins(FklOpcode op,uint32_t imm,uint64_t imm_u64)
{
	return (FklInstruction){.op=op,.imm=imm,.imm_u64=imm_u64};
}

static inline FklInstruction create_op_imm_u64_ins(FklOpcode op,uint64_t imm)
{ 
	return (FklInstruction){.op=op,.imm_u64=imm};
}

static inline FklInstruction create_op_imm_i64_ins(FklOpcode op,int64_t imm)
{ 
	return (FklInstruction){.op=op,.imm_i64=imm};
}

static FklByteCodelnt* create_bc_lnt(FklByteCode* bc
		,FklSid_t fid
		,uint64_t line)
{
	FklByteCodelnt* r=fklCreateByteCodelnt(bc);
	r->ls=1;
	r->l=(FklLineNumberTableItem*)malloc(sizeof(FklLineNumberTableItem)*1);
	FKL_ASSERT(r->l);
	fklInitLineNumTabNode(&r->l[0],fid,0,bc->len,line);
	return r;
}

static FklByteCodelnt* makePutLoc(uint64_t line
		,FklSid_t sym
		,uint32_t idx
		,FklCodegenInfo* codegen)
{
	FklByteCodelnt* r=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_PUT_LOC,idx),codegen->fid,line);
	return r;
}

static FklByteCodelnt* makePutRefLoc(const FklNastNode* sym
		,uint32_t idx
		,FklCodegenInfo* codegen)
{
	FklByteCodelnt* r=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_PUT_VAR_REF,idx),codegen->fid,sym->curline);
	return r;
}

static inline FklCodegenMacroScope* make_macro_scope_ref(FklCodegenMacroScope* s)
{
	if(s)
		s->refcount++;
	return s;
}

static FklCodegenQuest* createCodegenQuest(FklByteCodeProcesser f
		,FklCodegenQuestContext* context
		,FklCodegenNextExpression* nextExpression
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklCodegenEnv* env
		,uint64_t curline
		,FklCodegenQuest* prev
		,FklCodegenInfo* codegen)
{
	FklCodegenQuest* r=(FklCodegenQuest*)malloc(sizeof(FklCodegenQuest));
	FKL_ASSERT(r);
	r->scope=scope;
	r->macroScope=make_macro_scope_ref(macroScope);
	r->processer=f;
	r->context=context;
	if(env)
		env->refcount+=1;
	r->nextExpression=nextExpression;
	r->env=env;
	r->curline=curline;
	r->codegen=codegen;
	r->prev=prev;
	codegen->refcount+=1;
	r->codegen=codegen;
	return r;
}

static void destroyCodegenQuest(FklCodegenQuest* quest)
{
	FklCodegenNextExpression* nextExpression=quest->nextExpression;
	if(nextExpression)
	{
		if(nextExpression->t->finalizer)
			nextExpression->t->finalizer(nextExpression->context);
		free(nextExpression);
	}
	fklDestroyCodegenEnv(quest->env);
	fklDestroyCodegenMacroScope(quest->macroScope);
	quest->context->t->__finalizer(quest->context->data);
	free(quest->context);
	fklDestroyCodegenInfo(quest->codegen);
	free(quest);
}

#define FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(F,STACK,NEXT_EXPRESSIONS,SCOPE,MACRO_SCOPE,ENV,LINE,CODEGEN,CODEGEN_CONTEXT) fklPushPtrStack(createCodegenQuest((F)\
			,(STACK)\
			,(NEXT_EXPRESSIONS)\
			,(SCOPE)\
			,(MACRO_SCOPE)\
			,(ENV)\
			,(LINE)\
			,NULL\
			,(CODEGEN))\
			,(CODEGEN_CONTEXT))

#define BC_PROCESS(NAME) static FklByteCodelnt* NAME(FklCodegenInfo* codegen\
		,FklCodegenEnv* env\
		,uint32_t scope\
		,FklCodegenMacroScope* cms\
		,FklCodegenQuestContext* context\
		,FklSid_t fid\
		,uint64_t line\
		,FklCodegenErrorState* errorState\
		,FklCodegenOuterCtx* outer_ctx)

#define GET_STACK(CONTEXT) ((CONTEXT)->t->__get_bcl_stack((CONTEXT)->data))
BC_PROCESS(_default_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	if(!fklIsPtrStackEmpty(stack))
		return fklPopPtrStack(stack);
	else
		return NULL;
}

BC_PROCESS(_empty_bc_process)
{
	return NULL;
}

static FklByteCodelnt* sequnce_exp_bc_process(FklPtrStack* stack,FklSid_t fid,uint64_t line)
{
	if(stack->top)
	{
		FklInstruction drop=create_op_ins(FKL_OP_DROP);
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		size_t top=stack->top;
		for(size_t i=0;i<top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			if(cur->bc->len)
			{
				fklCodeLntConcat(retval,cur);
				if(i<top-1)
					fklBytecodeLntPushFrontIns(retval,&drop,fid,line);
			}
			fklDestroyByteCodelnt(cur);
		}
		stack->top=0;
		return retval;
	}
	else
		return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),fid,line);

}

BC_PROCESS(_begin_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	return sequnce_exp_bc_process(stack,fid,line);
}

static inline void pushListItemToQueue(FklNastNode* list,FklPtrQueue* queue,FklNastNode** last)
{
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr)
		fklPushPtrQueue(fklMakeNastNodeRef(list->pair->car),queue);
	if(last)
		*last=list;
}

static inline void pushListItemToStack(FklNastNode* list,FklPtrStack* stack,FklNastNode** last)
{
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr)
		fklPushPtrStack(list->pair->car,stack);
	if(last)
		*last=list;
}

static inline FklBuiltinInlineFunc is_inlinable_func_ref(uint32_t idx
		,FklCodegenEnv* env
		,uint32_t argNum
		,FklCodegenInfo* codegen)
{
	FklSymbolDef* ref=NULL;
	while(env)
	{
		FklHashTableItem* list=env->refs.first;
		for(;list;list=list->next)
		{
			FklSymbolDef* def=(FklSymbolDef*)list->data;
			if(def->idx==idx)
			{
				if(def->isLocal)
					env=NULL;
				else
				{
					if(!env->prev)
						ref=def;
					else
						idx=def->cidx;
					env=env->prev;
				}
				break;
			}
		}
		if(!list)
			break;
	}
	if(ref&&idx<codegen->builtinSymbolNum&&!codegen->builtinSymModiMark[idx])
		return fklGetBuiltinInlineFunc(idx,argNum);
	return NULL;
}

BC_PROCESS(_funcall_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top)
	{
		FklByteCodelnt* func=stack->base[0];
		FklByteCode* funcBc=func->bc;
		uint32_t argNum=stack->top-1;
		FklBuiltinInlineFunc inlFunc=NULL;
		const FklInstruction* ins=&funcBc->code[0];
		if(argNum<4
				&&ins->op==FKL_OP_GET_VAR_REF
				&&(inlFunc=is_inlinable_func_ref(ins->imm_u32
						,env
						,argNum
						,codegen)))
		{
			fklDestroyByteCodelnt(func);
			stack->top=0;
			return inlFunc((FklByteCodelnt**)stack->base+1,fid,line);
		}
		else
		{
			FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
			while(stack->top>1)
			{
				FklByteCodelnt* cur=fklPopPtrStack(stack);
				fklCodeLntConcat(retval,cur);
				fklDestroyByteCodelnt(cur);
			}
			FklInstruction setBp=create_op_ins(FKL_OP_SET_BP);
			FklInstruction call=create_op_ins(FKL_OP_CALL);
			fklBytecodeLntPushBackIns(&setBp,retval,fid,line);
			fklCodeLntConcat(retval,func);
			fklBytecodeLntPushFrontIns(retval,&call,fid,line);
			return retval;
		}
	}
	else
		return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),fid,line);
}

static int pushFuncallListToQueue(FklNastNode* list,FklPtrQueue* queue,FklNastNode** last,FklNastNode* const* builtin_pattern_node)
{
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr)
		fklPushPtrQueue(fklMakeNastNodeRef(list->pair->car),queue);
	if(last)
		*last=list;
	return 0;
}

static void codegen_funcall(FklNastNode* rest
		,FklPtrStack* codegenQuestStack
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklCodegenEnv* env
		,FklCodegenInfo* codegen
		,FklCodegenErrorState* errorState)
{
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklNastNode* last=NULL;
	int r=pushFuncallListToQueue(rest,queue,&last,codegen->outer_ctx->builtin_pattern_node);
	if(r||last->type!=FKL_NAST_NIL)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(rest);
		while(!fklIsPtrQueueEmpty(queue))
			fklDestroyNastNode(fklPopPtrQueue(queue));
		fklDestroyPtrQueue(queue);
	}
	else
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_funcall_exp_bc_process
				,createDefaultStackContext(fklCreatePtrStack(32,16))
				,createMustHasRetvalQueueNextExpression(queue)
				,scope
				,macroScope
				,env
				,rest->curline
				,codegen
				,codegenQuestStack);
}

#define CODEGEN_ARGS FklNastNode* origExp\
		,FklHashTable* ht\
		,FklPtrStack* codegenQuestStack\
		,uint32_t scope\
		,FklCodegenMacroScope* macroScope\
		,FklCodegenEnv* curEnv\
		,FklCodegenInfo* codegen\
		,FklCodegenOuterCtx* outer_ctx\
		,FklCodegenErrorState* errorState

#define CODEGEN_FUNC(NAME) void NAME(CODEGEN_ARGS)

static CODEGEN_FUNC(codegen_begin)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_begin_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createDefaultQueueNextExpression(queue)
			,scope
			,macroScope
			,curEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}

static uintptr_t _codegenenv_hashFunc(const void* key)
{
	FklSid_t sid=*(const FklSid_t*)key;
	return sid;
}

static int _codegenenv_keyEqual(const void* pkey0,const void* pkey1)
{
	const FklSidScope* k0=pkey0;
	const FklSidScope* k1=pkey1;
	return k0->id==k1->id&&k0->scope==k1->scope;
}

static void _codegenenv_setVal(void* d0,const void* d1)
{
	*(FklSymbolDef*)d0=*(FklSymbolDef*)d1;
}

static void _codegenenv_setKey(void* pkey0,const void* pkey1)
{
	*(FklSidScope*)pkey0=*(FklSidScope*)pkey1;
}

static FklHashTableMetaTable CodegenEnvHashMethodTable=
{
	.size=sizeof(FklSymbolDef),
	.__setKey=_codegenenv_setKey,
	.__setVal=_codegenenv_setVal,
	.__hashFunc=_codegenenv_hashFunc,
	.__uninitItem=fklDoNothingUninitHashItem,
	.__keyEqual=_codegenenv_keyEqual,
	.__getKey=fklHashDefaultGetKey,
};

static inline uint32_t enter_new_scope(uint32_t p,FklCodegenEnv* env)
{
	uint32_t r=++env->sc;
	FklCodegenEnvScope* scopes=(FklCodegenEnvScope*)fklRealloc(env->scopes,sizeof(FklCodegenEnvScope)*r);
	FKL_ASSERT(scopes);
	env->scopes=scopes;
	FklCodegenEnvScope* newScope=&scopes[r-1];
	newScope->p=p;
	fklInitHashTable(&newScope->defs,&CodegenEnvHashMethodTable);
	newScope->start=0;
	newScope->end=0;
	if(p)
		newScope->start=scopes[p-1].start+scopes[p-1].end;
	newScope->empty=newScope->start;
	return r;
}

static inline void process_unresolve_ref(FklCodegenEnv* env,FklFuncPrototypes* cp)
{
	FklPtrStack* urefs=&env->uref;
	FklFuncPrototype* pts=cp->pts;
	FklPtrStack urefs1=FKL_STACK_INIT;
	fklInitPtrStack(&urefs1,16,8);
	uint32_t count=urefs->top;
	for(uint32_t i=0;i<count;i++)
	{
		FklUnReSymbolRef* uref=urefs->base[i];
		FklFuncPrototype* cpt=&pts[uref->prototypeId];
		FklSymbolDef* ref=&cpt->refs[uref->idx];
		FklSymbolDef* def=fklFindSymbolDefByIdAndScope(uref->id,uref->scope,env);
		if(def)
		{
			env->slotFlags[def->idx]=FKL_CODEGEN_ENV_SLOT_REF;
			ref->cidx=def->idx;
			ref->isLocal=1;
			free(uref);
		}
		else if(env->prev)
		{
			ref->cidx=fklAddCodegenRefBySid(uref->id,env,uref->fid,uref->line);
			free(uref);
		}
		else
		{
			fklAddCodegenBuiltinRefBySid(uref->id,env);
			fklPushPtrStack(uref,&urefs1);
		}
	}
	urefs->top=0;
	while(!fklIsPtrStackEmpty(&urefs1))
		fklPushPtrStack(fklPopPtrStack(&urefs1),urefs);
	fklUninitPtrStack(&urefs1);
}

static inline int reset_flag_and_check_var_be_refed(uint8_t* flags
		,FklCodegenEnvScope* sc
		,FklCodegenEnv* env
		,FklFuncPrototypes* cp
		,uint32_t* start
		,uint32_t* end)
{
	process_unresolve_ref(env,cp);
	int r=0;
	uint32_t last=sc->start+sc->end;
	uint32_t i=sc->start;
	uint32_t s=i;
	for(;i<last;i++)
	{
		r=flags[i]==FKL_CODEGEN_ENV_SLOT_REF;
		flags[i]=0;
		if(r)
		{
			s=i;
			break;
		}
	}
	uint32_t e=i;
	for(;i<last;i++)
	{
		if(flags[i]==FKL_CODEGEN_ENV_SLOT_REF)
			e=i;
		flags[i]=0;
	}
	*start=s;
	*end=e+1;
	return r;
}

static inline void append_close_ref(FklByteCodelnt* retval
		,uint32_t s
		,uint32_t e
		,FklSid_t fid
		,uint64_t line)
{
	FklInstruction ins={.op=FKL_OP_CLOSE_REF,.imm=s,.imm_u32=e};
	fklBytecodeLntPushFrontIns(retval,&ins,fid,line);
}

static inline void check_and_close_ref(FklByteCodelnt* retval
		,uint32_t scope
		,FklCodegenEnv* env
		,FklFuncPrototypes* pts
		,FklSid_t fid
		,uint64_t line)
{
	FklCodegenEnvScope* cur=&env->scopes[scope-1];
	uint32_t start=cur->start;
	uint32_t end=start+1;
	if(reset_flag_and_check_var_be_refed(env->slotFlags,cur,env,pts,&start,&end))
		append_close_ref(retval,start,end,fid,line);
}

BC_PROCESS(_local_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=sequnce_exp_bc_process(stack,fid,line);
	check_and_close_ref(retval,scope,env,codegen->pts,fid,line);
	return retval;
}

static CODEGEN_FUNC(codegen_local)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	uint32_t cs=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);
	pushListItemToQueue(rest,queue,NULL);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_local_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createDefaultQueueNextExpression(queue)
			,cs
			,cms
			,curEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_let0)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	uint32_t cs=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);
	pushListItemToQueue(rest,queue,NULL);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_local_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createDefaultQueueNextExpression(queue)
			,cs
			,cms
			,curEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}

typedef struct Let1Context
{
	FklPtrStack* stack;
	FklUintStack* ss;
}Let1Context;

static inline Let1Context* createLet1Context(FklUintStack* ss)
{
	Let1Context* ctx=(Let1Context*)malloc(sizeof(Let1Context));
	FKL_ASSERT(ctx);
	ctx->ss=ss;
	ctx->stack=fklCreatePtrStack(8,16);
	return ctx;
}

static FklPtrStack* _let1_get_stack(void* d)
{
	return ((Let1Context*)d)->stack;
}

static void _let1_put_bcl(void* d,FklByteCodelnt* bcl)
{
	fklPushPtrStack(bcl,((Let1Context*)d)->stack);
}

static void _let1_finalizer(void* d)
{
	Let1Context* dd=(Let1Context*)d;
	FklPtrStack* s=dd->stack;
	while(!fklIsPtrStackEmpty(s))
		fklDestroyByteCodelnt(fklPopPtrStack(s));
	fklDestroyPtrStack(s);
	fklDestroyUintStack(dd->ss);
	free(dd);
}

static FklCodegenQuestContextMethodTable Let1ContextMethodTable=
{
	.__finalizer=_let1_finalizer,
	.__get_bcl_stack=_let1_get_stack,
	.__put_bcl=_let1_put_bcl,
};

static FklCodegenQuestContext* createLet1CodegenContext(FklUintStack* ss)
{
	return createCodegenQuestContext(createLet1Context(ss),&Let1ContextMethodTable);
}

static inline size_t nast_list_len(const FklNastNode* list)
{
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr)i++;
	return i;
}

static int is_valid_let_arg(const FklNastNode* node,FklNastNode* const* builtin_pattern_node)
{
	return node->type==FKL_NAST_PAIR
		&&fklIsNastNodeList(node)
		&&nast_list_len(node)==2
		&&node->pair->car->type==FKL_NAST_SYM;
}

static int is_valid_let_args(const FklNastNode* sl
		,FklCodegenEnv* env
		,uint32_t scope
		,FklUintStack* stack
		,FklNastNode* const* builtin_pattern_node)
{
	if(fklIsNastNodeList(sl))
	{
		for(;sl->type==FKL_NAST_PAIR;sl=sl->pair->cdr)
		{
			FklNastNode* cc=sl->pair->car;
			if(!is_valid_let_arg(cc,builtin_pattern_node))
				return 0;
			FklSid_t id=cc->pair->car->sym;
			if(fklIsSymbolDefined(id,scope,env))
				return 0;
			fklPushUintStack(id,stack);
			fklAddCodegenDefBySid(id,scope,env);
		}
		return 1;
	}
	else
		return 0;
}

static inline FklNastNode* caddr_nast_node(const FklNastNode* node)
{
	return node->pair->cdr->pair->cdr->pair->car;
}

static inline FklNastNode* caadr_nast_node(const FklNastNode* node)
{
	return node->pair->cdr->pair->car->pair->car;
}

BC_PROCESS(_let1_exp_bc_process)
{
	Let1Context* ctx=context->data;
	FklPtrStack* bcls=ctx->stack;
	FklUintStack* symbolStack=ctx->ss;
	FklInstruction setBp=create_op_ins(FKL_OP_SET_BP);
	FklInstruction resBp=create_op_ins(FKL_OP_RES_BP);

	FklInstruction popArg=create_op_ins(FKL_OP_POP_ARG);

	FklByteCodelnt* retval=fklPopPtrStack(bcls);
	fklBytecodeLntPushBackIns(&resBp,retval,fid,line);
	while(!fklIsUintStackEmpty(symbolStack))
	{
		FklSid_t id=fklPopUintStack(symbolStack);
		uint32_t idx=fklAddCodegenDefBySid(id,scope,env);
		popArg.imm_u32=idx;
		fklBytecodeLntPushBackIns(&popArg,retval,fid,line);
	}
	if(!fklIsPtrStackEmpty(bcls))
	{
		FklByteCodelnt* args=fklPopPtrStack(bcls);
		fklCodeLntReverseConcat(args,retval);
		fklDestroyByteCodelnt(args);
	}
	fklBytecodeLntPushBackIns(&setBp,retval,fid,line);
	return retval;
}

BC_PROCESS(_let_arg_exp_bc_process)
{
	FklPtrStack* bcls=GET_STACK(context);
	if(bcls->top)
	{
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		while(!fklIsPtrStackEmpty(bcls))
		{
			FklByteCodelnt* cur=fklPopPtrStack(bcls);
			fklCodeLntConcat(retval,cur);
			fklDestroyByteCodelnt(cur);
		}
		return retval;
	}
	else
		return NULL;
}

static CODEGEN_FUNC(codegen_let1)
{
	FklNastNode* const* builtin_pattern_node=outer_ctx->builtin_pattern_node;
	FklUintStack* symStack=fklCreateUintStack(8,16);
	FklNastNode* firstSymbol=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	if(firstSymbol->type!=FKL_NAST_SYM)
	{
		fklDestroyUintStack(symStack);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklPatternMatchingHashTableItem* item=fklGetHashItem(&outer_ctx->builtInPatternVar_args,ht);
	FklNastNode* args=item?item->node:NULL;
	uint32_t cs=enter_new_scope(scope,curEnv);

	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

	fklAddCodegenDefBySid(firstSymbol->sym,cs,curEnv);
	fklPushUintStack(firstSymbol->sym,symStack);

	FklPtrQueue* valueQueue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(value),valueQueue);

	if(args)
	{
		if(!is_valid_let_args(args,curEnv,cs,symStack,builtin_pattern_node))
		{
			cms->refcount=1;
			fklDestroyNastNode(value);
			fklDestroyPtrQueue(valueQueue);
			fklDestroyCodegenMacroScope(cms);
			fklDestroyUintStack(symStack);
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(origExp);
			return;
		}
		for(FklNastNode* cur=args;cur->type==FKL_NAST_PAIR;cur=cur->pair->cdr)
			fklPushPtrQueue(fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)),valueQueue);
	}

	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	FklCodegenQuest* let1Quest=createCodegenQuest(_let1_exp_bc_process
			,createLet1CodegenContext(symStack)
			,NULL
			,cs
			,cms
			,curEnv
			,origExp->curline
			,NULL
			,codegen);

	fklPushPtrStack(let1Quest,codegenQuestStack);

	uint32_t len=fklLengthPtrQueue(queue);

	FklCodegenQuest* restQuest=createCodegenQuest(_local_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(len,16))
			,createDefaultQueueNextExpression(queue)
			,cs
			,cms
			,curEnv
			,rest->curline
			,let1Quest
			,codegen);
	fklPushPtrStack(restQuest,codegenQuestStack);

	len=fklLengthPtrQueue(valueQueue);

	FklCodegenQuest* argQuest=createCodegenQuest(_let_arg_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(len,16))
			,createMustHasRetvalQueueNextExpression(valueQueue)
			,scope
			,macroScope
			,curEnv
			,firstSymbol->curline
			,let1Quest
			,codegen);
	fklPushPtrStack(argQuest,codegenQuestStack);
}

static inline FklNastNode* create_nast_list(FklNastNode** a,size_t num,uint64_t line)
{
	FklNastNode* r=NULL;
	FklNastNode** cur=&r;
	for(size_t i=0;i<num;i++)
	{
		(*cur)=fklCreateNastNode(FKL_NAST_PAIR,a[i]->curline);
		(*cur)->pair=fklCreateNastPair();
		(*cur)->pair->car=a[i];
		cur=&(*cur)->pair->cdr;
	}
	(*cur)=fklCreateNastNode(FKL_NAST_NIL,line);
	return r;
}

static CODEGEN_FUNC(codegen_let81)
{
	FklSid_t letHeadId=fklAddSymbolCstr("let",&outer_ctx->public_symbol_table)->id;
	FklNastNode* letHead=fklCreateNastNode(FKL_NAST_SYM,origExp->pair->car->curline);
	letHead->sym=letHeadId;

	FklNastNode* firstNameValue=cadr_nast_node(origExp);

	FklNastNode* args=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);

	FklNastNode* restLet8=fklCreateNastNode(FKL_NAST_PAIR,args->curline);
	restLet8->pair=fklCreateNastPair();
	restLet8->pair->car=args;
	restLet8->pair->cdr=fklMakeNastNodeRef(rest);
	FklNastNode* old=origExp->pair->cdr;
	origExp->pair->cdr=restLet8;

	FklNastNode* a[3]=
	{
		letHead,
		fklMakeNastNodeRef(firstNameValue),
		fklMakeNastNodeRef(origExp),
	};
	letHead=create_nast_list(a,3,origExp->curline);
	FklPtrQueue* queue=fklCreatePtrQueue();
	fklPushPtrQueue(letHead,queue);
	fklDestroyNastNode(old);
	firstNameValue->pair->cdr=fklCreateNastNode(FKL_NAST_NIL,firstNameValue->pair->cdr->curline);

	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process
			,createDefaultStackContext(fklCreatePtrStack(1,1))
			,createDefaultQueueNextExpression(queue)
			,scope
			,macroScope
			,curEnv
			,letHead->curline
			,codegen
			,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_letrec)
{
	FklNastNode* const* builtin_pattern_node=outer_ctx->builtin_pattern_node;
	FklUintStack* symStack=fklCreateUintStack(8,16);
	FklNastNode* firstSymbol=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	if(firstSymbol->type!=FKL_NAST_SYM)
	{
		fklDestroyUintStack(symStack);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklNastNode* args=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);
	uint32_t cs=enter_new_scope(scope,curEnv);

	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

	fklAddCodegenDefBySid(firstSymbol->sym,cs,curEnv);
	fklPushUintStack(firstSymbol->sym,symStack);

	if(!is_valid_let_args(args,curEnv,cs,symStack,builtin_pattern_node))
	{
		cms->refcount=1;
		fklDestroyCodegenMacroScope(cms);
		fklDestroyUintStack(symStack);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}

	FklPtrQueue* valueQueue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(value),valueQueue);
	for(FklNastNode* cur=args;cur->type==FKL_NAST_PAIR;cur=cur->pair->cdr)
		fklPushPtrQueue(fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)),valueQueue);

	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	FklCodegenQuest* let1Quest=createCodegenQuest(_let1_exp_bc_process
			,createLet1CodegenContext(symStack)
			,NULL
			,cs
			,cms
			,curEnv
			,origExp->curline
			,NULL
			,codegen);

	fklPushPtrStack(let1Quest,codegenQuestStack);

	uint32_t len=fklLengthPtrQueue(queue);

	FklCodegenQuest* restQuest=createCodegenQuest(_local_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(len,16))
			,createDefaultQueueNextExpression(queue)
			,cs
			,cms
			,curEnv
			,rest->curline
			,let1Quest
			,codegen);
	fklPushPtrStack(restQuest,codegenQuestStack);

	len=fklLengthPtrQueue(valueQueue);

	FklCodegenQuest* argQuest=createCodegenQuest(_let_arg_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(len,16))
			,createMustHasRetvalQueueNextExpression(valueQueue)
			,cs
			,cms
			,curEnv
			,firstSymbol->curline
			,let1Quest
			,codegen);
	fklPushPtrStack(argQuest,codegenQuestStack);
}

BC_PROCESS(_do0_exp_bc_process)
{
	FklPtrStack* s=GET_STACK(context);
	FklByteCodelnt* rest=fklPopPtrStack(s);
	FklByteCodelnt* value=fklPopPtrStack(s);
	FklByteCodelnt* cond=fklPopPtrStack(s);

	FklInstruction pop=create_op_ins(FKL_OP_DROP);
	FklInstruction jmp=create_op_ins(FKL_OP_JMP);
	FklInstruction jmpIfTrue=create_op_ins(FKL_OP_JMP_IF_TRUE);

	if(rest->bc->len)
		fklBytecodeLntPushBackIns(&pop,rest,fid,line);

	jmpIfTrue.imm_i64=rest->bc->len+1;
	fklBytecodeLntPushFrontIns(cond,&jmpIfTrue,fid,line);

	jmp.imm_i64=-(rest->bc->len+cond->bc->len+1);
	fklBytecodeLntPushFrontIns(rest,&jmp,fid,line);
	fklCodeLntConcat(cond,rest);
	fklDestroyByteCodelnt(rest);

	if(value->bc->len)
		fklBytecodeLntPushFrontIns(cond,&pop,fid,line);
	fklCodeLntReverseConcat(cond,value);
	fklDestroyByteCodelnt(cond);

	return value;
}

BC_PROCESS(_do_rest_exp_bc_process)
{
	FklPtrStack* s=GET_STACK(context);
	if(s->top)
	{
		FklInstruction pop=create_op_ins(FKL_OP_DROP);
		FklByteCodelnt* r=sequnce_exp_bc_process(s,fid,line);
		fklBytecodeLntPushFrontIns(r,&pop,fid,line);
		return r;
	}
	return fklCreateByteCodelnt(fklCreateByteCode(0));
}

static CODEGEN_FUNC(codegen_do0)
{
	FklNastNode* cond=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_cond,ht);

	FklPatternMatchingHashTableItem* item=fklGetHashItem(&outer_ctx->builtInPatternVar_value,ht);
	FklNastNode* value=item?item->node:NULL;

	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	uint32_t cs=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);

	FklCodegenQuest* do0Quest=createCodegenQuest(_do0_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(3,16))
			,NULL
			,cs
			,cms
			,curEnv
			,cond->curline
			,NULL
			,codegen);
	fklPushPtrStack(do0Quest,codegenQuestStack);

	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_do_rest_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(fklLengthPtrQueue(queue),16))
			,createDefaultQueueNextExpression(queue)
			,cs
			,cms
			,curEnv
			,cond->curline
			,codegen
			,codegenQuestStack);

	if(value)
	{
		FklPtrQueue* vQueue=fklCreatePtrQueue();
		fklPushPtrQueue(fklMakeNastNodeRef(value),vQueue);
		FklCodegenQuest* do0VQuest=createCodegenQuest(_default_bc_process
				,createDefaultStackContext(fklCreatePtrStack(1,16))
				,createMustHasRetvalQueueNextExpression(vQueue)
				,cs
				,cms
				,curEnv
				,origExp->curline
				,do0Quest
				,codegen);
		fklPushPtrStack(do0VQuest,codegenQuestStack);
	}
	else
	{
		FklByteCodelnt* v=fklCreateByteCodelnt(fklCreateByteCode(0));
		FklPtrStack* s=fklCreatePtrStack(1,16);
		fklPushPtrStack(v,s);
		FklCodegenQuest* do0VQuest=createCodegenQuest(_default_bc_process
				,createDefaultStackContext(s)
				,NULL
				,cs
				,cms
				,curEnv
				,origExp->curline
				,do0Quest
				,codegen);
		fklPushPtrStack(do0VQuest,codegenQuestStack);
	}
	FklPtrQueue* cQueue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(cond),cQueue);
	FklCodegenQuest* do0CQuest=createCodegenQuest(_default_bc_process
			,createDefaultStackContext(fklCreatePtrStack(1,16))
			,createMustHasRetvalQueueNextExpression(cQueue)
			,cs
			,cms
			,curEnv
			,origExp->curline
			,do0Quest
			,codegen);
	fklPushPtrStack(do0CQuest,codegenQuestStack);
}

static inline void destroy_next_exp_queue(FklPtrQueue* q)
{
	while(!fklIsPtrQueueEmpty(q))
		fklDestroyNastNode(fklPopPtrQueue(q));
	fklDestroyPtrQueue(q);
}

static inline int is_valid_do_var_bind(const FklNastNode* list,FklNastNode** nextV,FklNastNode* const* builtin_pattern_node)
{
	if(!fklIsNastNodeList(list))
		return 0;
	if(list->pair->car->type!=FKL_NAST_SYM)
		return 0;
	size_t len=nast_list_len(list);
	if(len!=2&&len!=3)
		return 0;
	if(len==3)
	{
		FklNastNode* next=caddr_nast_node(list);
		*nextV=next;
	}
	return 1;
}

static inline int is_valid_do_bind_list(const FklNastNode* sl
		,FklCodegenEnv* env
		,uint32_t scope
		,FklUintStack* stack
		,FklUintStack* nstack
		,FklPtrQueue* valueQueue
		,FklPtrQueue* nextQueue
		,FklNastNode* const* builtin_pattern_node)
{
	if(fklIsNastNodeList(sl))
	{
		for(;sl->type==FKL_NAST_PAIR;sl=sl->pair->cdr)
		{
			FklNastNode* cc=sl->pair->car;
			FklNastNode* nextExp=NULL;
			if(!is_valid_do_var_bind(cc,&nextExp,builtin_pattern_node))
				return 0;
			FklSid_t id=cc->pair->car->sym;
			if(fklIsSymbolDefined(id,scope,env))
				return 0;
			uint32_t idx=fklAddCodegenDefBySid(id,scope,env);
			fklPushUintStack(idx,stack);
			fklPushPtrQueue(fklMakeNastNodeRef(cadr_nast_node(cc)),valueQueue);
			if(nextExp)
			{
				fklPushUintStack(idx,nstack);
				fklPushPtrQueue(fklMakeNastNodeRef(nextExp),nextQueue);
			}
		}
		return 1;
	}
	return 0;
}

BC_PROCESS(_do1_init_val_bc_process)
{
	FklByteCodelnt* ret=fklCreateByteCodelnt(fklCreateByteCode(0));
	Let1Context* ctx=(Let1Context*)(context->data);
	FklPtrStack* s=ctx->stack;
	FklUintStack* ss=ctx->ss;

	FklInstruction pop=create_op_ins(FKL_OP_DROP);
	FklInstruction putLoc=create_op_ins(FKL_OP_PUT_LOC);

	uint64_t* idxbase=ss->base;
	FklByteCodelnt** bclBase=(FklByteCodelnt**)s->base;
	uint32_t top=s->top;
	for(uint32_t i=0;i<top;i++)
	{
		uint32_t idx=idxbase[i];
		FklByteCodelnt* curBcl=bclBase[i];
		putLoc.imm_u32=idx;
		fklBytecodeLntPushFrontIns(curBcl,&putLoc,fid,line);
		fklBytecodeLntPushFrontIns(curBcl,&pop,fid,line);
		fklCodeLntConcat(ret,curBcl);
	}
	return ret;
}

BC_PROCESS(_do1_next_val_bc_process)
{
	Let1Context* ctx=(Let1Context*)(context->data);
	FklPtrStack* s=ctx->stack;
	FklUintStack* ss=ctx->ss;

	if(s->top)
	{
		FklByteCodelnt* ret=fklCreateByteCodelnt(fklCreateByteCode(0));
		FklInstruction pop=create_op_ins(FKL_OP_DROP);

		FklInstruction putLoc=create_op_ins(FKL_OP_PUT_LOC);

		uint64_t* idxbase=ss->base;
		FklByteCodelnt** bclBase=(FklByteCodelnt**)s->base;
		uint32_t top=s->top;
		for(uint32_t i=0;i<top;i++)
		{
			uint32_t idx=idxbase[i];
			FklByteCodelnt* curBcl=bclBase[i];
			putLoc.imm_u32=idx;
			fklBytecodeLntPushFrontIns(curBcl,&putLoc,fid,line);
			fklBytecodeLntPushFrontIns(curBcl,&pop,fid,line);
			fklCodeLntConcat(ret,curBcl);
		}
		return ret;
	}
	return NULL;
}

BC_PROCESS(_do1_bc_process)
{
	FklPtrStack* s=GET_STACK(context);
	FklByteCodelnt* next=s->top==5?fklPopPtrStack(s):NULL;
	FklByteCodelnt* rest=fklPopPtrStack(s);
	FklByteCodelnt* value=fklPopPtrStack(s);
	FklByteCodelnt* cond=fklPopPtrStack(s);
	FklByteCodelnt* init=fklPopPtrStack(s);

	FklInstruction pop=create_op_ins(FKL_OP_DROP);
	FklInstruction jmp=create_op_ins(FKL_OP_JMP);
	FklInstruction jmpIfTrue=create_op_ins(FKL_OP_JMP_IF_TRUE);

	fklBytecodeLntPushFrontIns(rest,&pop,fid,line);
	if(next)
	{
		fklCodeLntConcat(rest,next);
		fklDestroyByteCodelnt(next);
	}

	jmpIfTrue.imm_i64=rest->bc->len+1;
	fklBytecodeLntPushFrontIns(cond,&jmpIfTrue,fid,line);

	jmp.imm_i64=-(rest->bc->len+cond->bc->len+1);
	fklBytecodeLntPushFrontIns(rest,&jmp,fid,line);
	fklCodeLntConcat(cond,rest);
	fklDestroyByteCodelnt(rest);

	if(value->bc->len)
		fklBytecodeLntPushFrontIns(cond,&pop,fid,line);
	fklCodeLntReverseConcat(cond,value);
	fklDestroyByteCodelnt(cond);

	fklCodeLntReverseConcat(init,value);
	fklDestroyByteCodelnt(init);
	check_and_close_ref(value,scope,env,codegen->pts,fid,line);
	return value;
}

static CODEGEN_FUNC(codegen_do1)
{
	FklNastNode* const* builtin_pattern_node=outer_ctx->builtin_pattern_node;
	FklNastNode* bindlist=cadr_nast_node(origExp);

	FklNastNode* cond=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_cond,ht);

	FklPatternMatchingHashTableItem* item=fklGetHashItem(&outer_ctx->builtInPatternVar_value,ht);
	FklNastNode* value=item?item->node:NULL;

	FklUintStack* symStack=fklCreateUintStack(4,16);
	FklUintStack* nextSymStack=fklCreateUintStack(4,16);

	uint32_t cs=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

	FklPtrQueue* valueQueue=fklCreatePtrQueue();
	FklPtrQueue* nextValueQueue=fklCreatePtrQueue();
	if(!is_valid_do_bind_list(bindlist
				,curEnv
				,cs
				,symStack,nextSymStack
				,valueQueue,nextValueQueue
				,builtin_pattern_node))
	{
		cms->refcount=1;
		fklDestroyCodegenMacroScope(cms);
		fklDestroyUintStack(symStack);
		fklDestroyUintStack(nextSymStack);
		destroy_next_exp_queue(valueQueue);
		destroy_next_exp_queue(nextValueQueue);
		
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}

	FklCodegenQuest* do1Quest=createCodegenQuest(_do1_bc_process
			,createDefaultStackContext(fklCreatePtrStack(5,16))
			,NULL
			,cs
			,cms
			,curEnv
			,origExp->curline
			,NULL
			,codegen);
	fklPushPtrStack(do1Quest,codegenQuestStack);

	FklCodegenQuest* do1NextValQuest=createCodegenQuest(_do1_next_val_bc_process
			,createLet1CodegenContext(nextSymStack)
			,createMustHasRetvalQueueNextExpression(nextValueQueue)
			,cs
			,cms
			,curEnv
			,origExp->curline
			,do1Quest
			,codegen);

	fklPushPtrStack(do1NextValQuest,codegenQuestStack);

	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	FklCodegenQuest* do1RestQuest=createCodegenQuest(_do_rest_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(fklLengthPtrQueue(queue),16))
			,createDefaultQueueNextExpression(queue)
			,cs
			,cms
			,curEnv
			,origExp->curline
			,do1Quest
			,codegen);
	fklPushPtrStack(do1RestQuest,codegenQuestStack);

	if(value)
	{
		FklPtrQueue* vQueue=fklCreatePtrQueue();
		fklPushPtrQueue(fklMakeNastNodeRef(value),vQueue);
		FklCodegenQuest* do1VQuest=createCodegenQuest(_default_bc_process
				,createDefaultStackContext(fklCreatePtrStack(1,16))
				,createMustHasRetvalQueueNextExpression(vQueue)
				,cs
				,cms
				,curEnv
				,origExp->curline
				,do1Quest
				,codegen);
		fklPushPtrStack(do1VQuest,codegenQuestStack);
	}
	else
	{
		FklByteCodelnt* v=fklCreateByteCodelnt(fklCreateByteCode(0));
		FklPtrStack* s=fklCreatePtrStack(1,16);	
		fklPushPtrStack(v,s);
		FklCodegenQuest* do1VQuest=createCodegenQuest(_default_bc_process
				,createDefaultStackContext(s)
				,NULL
				,cs
				,cms
				,curEnv
				,origExp->curline
				,do1Quest
				,codegen);
		fklPushPtrStack(do1VQuest,codegenQuestStack);
	}

	FklPtrQueue* cQueue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(cond),cQueue);
	FklCodegenQuest* do1CQuest=createCodegenQuest(_default_bc_process
			,createDefaultStackContext(fklCreatePtrStack(1,16))
			,createMustHasRetvalQueueNextExpression(cQueue)
			,cs
			,cms
			,curEnv
			,origExp->curline
			,do1Quest
			,codegen);
	fklPushPtrStack(do1CQuest,codegenQuestStack);

	FklCodegenQuest* do1InitValQuest=createCodegenQuest(_do1_init_val_bc_process
			,createLet1CodegenContext(symStack)
			,createMustHasRetvalQueueNextExpression(valueQueue)
			,scope
			,macroScope
			,curEnv
			,origExp->curline
			,do1Quest
			,codegen);
	fklPushPtrStack(do1InitValQuest,codegenQuestStack);
}

static inline FklSid_t get_sid_in_gs_with_id_in_ps(FklSid_t id,FklSymbolTable* gs,FklSymbolTable* ps)
{
	return fklAddSymbol(fklGetSymbolWithId(id,ps)->symbol,gs)->id;
}

static inline FklSid_t get_sid_with_idx(FklCodegenEnvScope* sc
		,uint32_t idx
		,FklSymbolTable* gs
		,FklSymbolTable* ps)
{
	for(FklHashTableItem* l=sc->defs.first;l;l=l->next)
	{
		FklSymbolDef* def=(FklSymbolDef*)l->data;
		if(def->idx==idx)
			return get_sid_in_gs_with_id_in_ps(def->k.id,gs,ps);
	}
	return 0;
}

static inline FklSid_t get_sid_with_ref_idx(FklHashTable* refs
		,uint32_t idx
		,FklSymbolTable* gs
		,FklSymbolTable* ps)
{
	for(FklHashTableItem* l=refs->first;l;l=l->next)
	{
		FklSymbolDef* def=(FklSymbolDef*)l->data;
		if(def->idx==idx)
			return get_sid_in_gs_with_id_in_ps(def->k.id,gs,ps);
	}
	return 0;
}

static inline FklByteCodelnt* process_set_var(FklPtrStack* stack
		,FklCodegenInfo* codegen
		,FklCodegenOuterCtx* outer_ctx
		,FklCodegenEnv* env
		,uint32_t scope
		,FklSid_t fid
		,uint64_t line)
{
	FklByteCodelnt* cur=fklPopPtrStack(stack);
	FklByteCodelnt* popVar=fklPopPtrStack(stack);
	if(cur&&popVar)
	{
		const FklInstruction* cur_ins=&cur->bc->code[0];
		if(cur_ins->op==FKL_OP_PUSH_PROC)
		{
			const FklInstruction* popVar_ins=&popVar->bc->code[0];
			if(popVar_ins->op==FKL_OP_PUT_LOC)
			{
				uint32_t prototypeId=cur_ins->imm;;
				uint32_t idx=popVar_ins->imm_u32;
				if(!codegen->pts->pts[prototypeId].sid)
					codegen->pts->pts[prototypeId].sid=get_sid_with_idx(&env->scopes[scope-1]
							,idx
							,codegen->globalSymTable
							,&outer_ctx->public_symbol_table);
			}
			else if(popVar_ins->op==FKL_OP_PUT_VAR_REF)
			{
				uint32_t prototypeId=cur_ins->imm;
				uint32_t idx=popVar_ins->imm_u32;
				if(!codegen->pts->pts[prototypeId].sid)
					codegen->pts->pts[prototypeId].sid=get_sid_with_ref_idx(&env->refs
							,idx
							,codegen->globalSymTable
							,&outer_ctx->public_symbol_table);
			}
		}
		fklCodeLntReverseConcat(cur,popVar);
		fklDestroyByteCodelnt(cur);
	}
	else
	{
		popVar=cur;
		FklInstruction pushNil=create_op_ins(FKL_OP_PUSH_NIL);
		fklBytecodeLntPushBackIns(&pushNil,popVar,fid,line);
	}
	return popVar;
}

typedef struct
{
	FklSid_t id;
	uint32_t scope;
	uint64_t line;
	FklPtrStack stack;
}DefineVarContext;

static void _def_var_context_finalizer(void* data)
{
	DefineVarContext* ctx=data;
	FklPtrStack* s=&ctx->stack;
	while(!fklIsPtrStackEmpty(s))
		fklDestroyByteCodelnt(fklPopPtrStack(s));
	fklUninitPtrStack(s);
	free(ctx);
}

static void _def_var_context_put_bcl(void* data,FklByteCodelnt* bcl)
{
	DefineVarContext* ctx=data;
	FklPtrStack* s=&ctx->stack;
	fklPushPtrStack(bcl,s);
}

static FklPtrStack* _def_var_context_get_bcl_stack(void* data)
{
	return &((DefineVarContext*)data)->stack;
}

static const FklCodegenQuestContextMethodTable DefineVarContextMethodTable=
{
	.__finalizer=_def_var_context_finalizer,
	.__put_bcl=_def_var_context_put_bcl,
	.__get_bcl_stack=_def_var_context_get_bcl_stack,
};

static inline FklCodegenQuestContext* create_def_var_context(FklSid_t id,uint32_t scope,uint64_t line)
{
	DefineVarContext* ctx=(DefineVarContext*)malloc(sizeof(DefineVarContext));
	FKL_ASSERT(ctx);
	ctx->id=id;
	ctx->scope=scope;
	ctx->line=line;
	fklInitPtrStack(&ctx->stack,2,8);
	return createCodegenQuestContext(ctx,&DefineVarContextMethodTable);
}

BC_PROCESS(_def_var_exp_bc_process)
{
	DefineVarContext* ctx=context->data;
	FklPtrStack* stack=&ctx->stack;
	uint32_t idx=fklAddCodegenDefBySid(ctx->id,ctx->scope,env);
	fklPushFrontPtrStack(makePutLoc(ctx->line,ctx->id,idx,codegen),stack);
	return process_set_var(stack,codegen,outer_ctx,env,scope,fid,line);
}

BC_PROCESS(_set_var_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	return process_set_var(stack,codegen,outer_ctx,env,scope,fid,line);
}

BC_PROCESS(_lambda_exp_bc_process)
{
	FklByteCodelnt* retval=NULL;
	FklPtrStack* stack=GET_STACK(context);
	FklInstruction ret=create_op_ins(FKL_OP_RET);
	if(stack->top>1)
	{
		FklInstruction drop=create_op_ins(FKL_OP_DROP);
		retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		size_t top=stack->top;
		for(size_t i=1;i<top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			if(cur->bc->len)
			{
				fklCodeLntConcat(retval,cur);
				if(i<top-1)
					fklBytecodeLntPushFrontIns(retval,&drop,fid,line);
			}
			fklDestroyByteCodelnt(cur);
		}
		fklBytecodeLntPushFrontIns(retval,&ret,fid,line);
	}
	else
	{
		retval=fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),fid,line);
		fklBytecodeLntPushFrontIns(retval,&ret,fid,line);
	}
	fklCodeLntReverseConcat(stack->base[0],retval);
	fklDestroyByteCodelnt(stack->base[0]);
	stack->top=0;
	fklScanAndSetTailCall(retval->bc);
	FklFuncPrototypes* pts=codegen->pts;
	fklUpdatePrototype(pts,env,codegen->globalSymTable,&outer_ctx->public_symbol_table);
	FklInstruction pushProc=create_op_imm_u32_u64_ins(FKL_OP_PUSH_PROC,env->prototypeId,retval->bc->len);
	fklBytecodeLntPushBackIns(&pushProc,retval,fid,line);
	return retval;
}

static FklByteCodelnt* processArgs(const FklNastNode* args
		,FklCodegenEnv* curEnv
		,FklCodegenInfo* codegen)
{
	FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
	FklInstruction popArg=create_op_ins(FKL_OP_POP_ARG);
	FklInstruction popRestArg=create_op_ins(FKL_OP_POP_REST_ARG);

	for(;args->type==FKL_NAST_PAIR;args=args->pair->cdr)
	{
		FklNastNode* cur=args->pair->car;
		if(cur->type!=FKL_NAST_SYM)
		{
			fklDestroyByteCodelnt(retval);
			return NULL;
		}
		if(fklIsSymbolDefined(cur->sym,1,curEnv))
		{
			fklDestroyByteCodelnt(retval);
			return NULL;
		}
		uint32_t idx=fklAddCodegenDefBySid(cur->sym,1,curEnv);

		popArg.imm_u32=idx;
		fklBytecodeLntPushFrontIns(retval,&popArg,codegen->fid,cur->curline);

	}
	if(args->type!=FKL_NAST_NIL&&args->type!=FKL_NAST_SYM)
	{
		fklDestroyByteCodelnt(retval);
		return NULL;
	}
	if(args->type==FKL_NAST_SYM)
	{
		if(fklIsSymbolDefined(args->sym,1,curEnv))
		{
			fklDestroyByteCodelnt(retval);
			return NULL;
		}
		uint32_t idx=fklAddCodegenDefBySid(args->sym,1,curEnv);

		popRestArg.imm_u32=idx;
		fklBytecodeLntPushFrontIns(retval,&popRestArg,codegen->fid,args->curline);

	}
	FklInstruction resBp=create_op_ins(FKL_OP_RES_BP);
	fklBytecodeLntPushFrontIns(retval,&resBp,codegen->fid,args->curline);
	return retval;
}

static FklByteCodelnt* processArgsInStack(FklUintStack* stack
		,FklCodegenEnv* curEnv
		,FklCodegenInfo* codegen
		,uint64_t curline)
{
	FklInstruction popArg=create_op_ins(FKL_OP_POP_ARG);
	FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
	uint32_t top=stack->top;
	uint64_t* base=stack->base;
	for(uint32_t i=0;i<top;i++)
	{
		FklSid_t curId=base[i];

		uint32_t idx=fklAddCodegenDefBySid(curId,1,curEnv);

		popArg.imm_u32=idx;
		fklBytecodeLntPushFrontIns(retval,&popArg,codegen->fid,curline);
	}
	FklInstruction resBp=create_op_ins(FKL_OP_RES_BP);
	fklBytecodeLntPushFrontIns(retval,&resBp,codegen->fid,curline);
	return retval;
}

static inline FklSymbolDef* sid_ht_to_idx_key_ht(FklHashTable* sht
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* pst
		,int is_top)
{
	FklSymbolDef* refs=(FklSymbolDef*)malloc(sizeof(FklSymbolDef)*sht->num);
	FKL_ASSERT(refs);
	FklHashTableItem* list=sht->first;
	if(is_top)
	{
		uint32_t builtin_symbol_number=fklGetBuiltinSymbolNum();
		for(uint32_t i=0;i<builtin_symbol_number;i++)
			list=list->next;
	}
	for(;list;list=list->next)
	{
		FklSymbolDef* sd=(FklSymbolDef*)list->data;
		FklSid_t sid=fklAddSymbol(fklGetSymbolWithId(sd->k.id,pst)->symbol,globalSymTable)->id;
		FklSymbolDef ref={{sid,0},sd->idx,sd->cidx,sd->isLocal};
		refs[sd->idx]=ref;
	}
	return refs;
}

static inline void create_and_insert_to_pool(FklFuncPrototypes* cp
		,uint32_t p
		,FklCodegenEnv* env
		,FklSymbolTable* globalSymTable
		,FklSid_t sid
		,FklSid_t fid
		,uint64_t line
		,FklSymbolTable* pst)
{
	cp->count+=1;
	FklFuncPrototype* pts=(FklFuncPrototype*)fklRealloc(cp->pts,sizeof(FklFuncPrototype)*(cp->count+1));
	FKL_ASSERT(pts);
	cp->pts=pts;
	FklFuncPrototype* cpt=&pts[cp->count];
	env->prototypeId=cp->count;
	cpt->lcount=env->lcount;
	cpt->is_top=env->prev==NULL;
	cpt->refs=sid_ht_to_idx_key_ht(&env->refs,globalSymTable,pst,cpt->is_top);
	cpt->rcount=env->refs.num;
	cpt->sid=sid;
	cpt->fid=fid;
	cpt->line=line;
}

BC_PROCESS(_named_let_set_var_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* cur=fklPopPtrStack(stack);
	FklByteCodelnt* popVar=fklPopPtrStack(stack);
	if(cur&&popVar)
	{
		fklCodeLntReverseConcat(cur,popVar);
		fklDestroyByteCodelnt(cur);
	}
	else
	{
		popVar=cur;
		FklInstruction pushNil=create_op_ins(FKL_OP_PUSH_NIL);
		fklBytecodeLntPushBackIns(&pushNil,popVar,fid,line);
	}
	check_and_close_ref(popVar,scope,env,codegen->pts,fid,line);
	return popVar;
}

static CODEGEN_FUNC(codegen_named_let0)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_arg0,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	uint32_t cs=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_funcall_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(2,1))
			,NULL
			,cs
			,cms
			,curEnv
			,name->curline
			,codegen
			,codegenQuestStack);

	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	FklPtrStack* stack=fklCreatePtrStack(2,1);
	uint32_t idx=fklAddCodegenDefBySid(name->sym,cs,curEnv);
	fklPushPtrStack(makePutLoc(name->curline,name->sym,idx,codegen),stack);

	fklAddReplacementBySid(fklAddSymbolCstr("*func*",pst)->id
			,name
			,cms->replacements);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_named_let_set_var_exp_bc_process
			,createDefaultStackContext(stack)
			,NULL
			,cs
			,cms
			,curEnv
			,name->curline
			,codegen
			,codegenQuestStack);

	FklCodegenEnv* lambdaCodegenEnv=fklCreateCodegenEnv(curEnv,cs,cms);
	FklPtrStack* lStack=fklCreatePtrStack(2,16);
	FklNastNode* argsNode=caddr_nast_node(origExp);
	FklByteCodelnt* argBc=processArgs(argsNode,lambdaCodegenEnv,codegen);
	fklPushPtrStack(argBc,lStack);
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);

	create_and_insert_to_pool(codegen->pts
			,curEnv->prototypeId
			,lambdaCodegenEnv
			,codegen->globalSymTable
			,get_sid_in_gs_with_id_in_ps(name->sym,codegen->globalSymTable,pst)
			,codegen->fid
			,origExp->curline
			,pst);

	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_lambda_exp_bc_process
			,createDefaultStackContext(lStack)
			,createDefaultQueueNextExpression(queue)
			,1
			,lambdaCodegenEnv->macros
			,lambdaCodegenEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_named_let1)
{
	FklNastNode* const* builtin_pattern_node=outer_ctx->builtin_pattern_node;
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_arg0,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklUintStack* symStack=fklCreateUintStack(8,16);
	FklNastNode* firstSymbol=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	if(firstSymbol->type!=FKL_NAST_SYM)
	{
		fklDestroyUintStack(symStack);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklNastNode* args=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);

	uint32_t cs=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

	FklCodegenEnv* lambdaCodegenEnv=fklCreateCodegenEnv(curEnv,cs,cms);
	fklAddCodegenDefBySid(firstSymbol->sym,1,lambdaCodegenEnv);
	fklPushUintStack(firstSymbol->sym,symStack);

	if(!is_valid_let_args(args,lambdaCodegenEnv,1,symStack,builtin_pattern_node))
	{
		lambdaCodegenEnv->refcount=1;
		fklDestroyCodegenEnv(lambdaCodegenEnv);
		fklDestroyUintStack(symStack);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}

	FklPtrQueue* valueQueue=fklCreatePtrQueue();

	fklPushPtrQueue(fklMakeNastNodeRef(value),valueQueue);
	for(FklNastNode* cur=args;cur->type==FKL_NAST_PAIR;cur=cur->pair->cdr)
		fklPushPtrQueue(fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)),valueQueue);

	FklCodegenQuest* funcallQuest=createCodegenQuest(_funcall_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(2,1))
			,NULL
			,cs
			,cms
			,curEnv
			,name->curline
			,NULL
			,codegen);
	fklPushPtrStack(funcallQuest,codegenQuestStack);

	FklPtrStack* stack=fklCreatePtrStack(2,1);
	uint32_t idx=fklAddCodegenDefBySid(name->sym,cs,curEnv);
	fklPushPtrStack(makePutLoc(name->curline,name->sym,idx,codegen),stack);

	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	fklAddReplacementBySid(fklAddSymbolCstr("*func*",pst)->id
			,name
			,cms->replacements);
	fklPushPtrStack(createCodegenQuest(_let_arg_exp_bc_process
				,createDefaultStackContext(fklCreatePtrStack(fklLengthPtrQueue(valueQueue),16))
				,createMustHasRetvalQueueNextExpression(valueQueue)
				,scope
				,macroScope
				,curEnv
				,firstSymbol->curline
				,funcallQuest
				,codegen)
			,codegenQuestStack);

	fklPushPtrStack(createCodegenQuest(_named_let_set_var_exp_bc_process
				,createDefaultStackContext(stack)
				,NULL
				,cs
				,cms
				,curEnv
				,name->curline
				,funcallQuest
				,codegen)
			,codegenQuestStack);

	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	FklPtrStack* lStack=fklCreatePtrStack(2,16);

	FklByteCodelnt* argBc=processArgsInStack(symStack
			,lambdaCodegenEnv
			,codegen
			,firstSymbol->curline);

	fklPushPtrStack(argBc,lStack);

	fklDestroyUintStack(symStack);
	create_and_insert_to_pool(codegen->pts
			,curEnv->prototypeId
			,lambdaCodegenEnv
			,codegen->globalSymTable
	 		,get_sid_in_gs_with_id_in_ps(name->sym,codegen->globalSymTable,pst)
			,codegen->fid
			,origExp->curline
			,pst);

	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_lambda_exp_bc_process
			,createDefaultStackContext(lStack)
			,createDefaultQueueNextExpression(queue)
			,1
			,lambdaCodegenEnv->macros
			,lambdaCodegenEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}

BC_PROCESS(_and_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top)
	{
		FklInstruction drop=create_op_ins(FKL_OP_DROP);
		FklInstruction jmpIfFalse=create_op_ins(FKL_OP_JMP_IF_FALSE);
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		while(!fklIsPtrStackEmpty(stack))
		{
			if(retval->bc->len)
			{
				fklBytecodeLntPushBackIns(&drop,retval,fid,line);
				jmpIfFalse.imm_i64=retval->bc->len;
				fklBytecodeLntPushBackIns(&jmpIfFalse,retval,fid,line);
			}
			FklByteCodelnt* cur=fklPopPtrStack(stack);
			fklCodeLntReverseConcat(cur,retval);
			fklDestroyByteCodelnt(cur);
		}
		return retval;
	}
	else
		return fklCreateSingleInsBclnt(fklCreatePushI32Ins(1),fid,line);
}

static CODEGEN_FUNC(codegen_and)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	uint32_t cs=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_and_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createDefaultQueueNextExpression(queue)
			,cs
			,cms
			,curEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}

BC_PROCESS(_or_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top)
	{
		FklInstruction drop=create_op_ins(FKL_OP_DROP);
		FklInstruction jmpIfTrue=create_op_ins(FKL_OP_JMP_IF_TRUE);
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		while(!fklIsPtrStackEmpty(stack))
		{
			if(retval->bc->len)
			{
				fklBytecodeLntPushBackIns(&drop,retval,fid,line);
				jmpIfTrue.imm_i64=retval->bc->len;
				fklBytecodeLntPushBackIns(&jmpIfTrue,retval,fid,line);
			}
			FklByteCodelnt* cur=fklPopPtrStack(stack);
			fklCodeLntReverseConcat(cur,retval);
			fklDestroyByteCodelnt(cur);
		}
		return retval;
	}
	else
		return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),fid,line);
}

static CODEGEN_FUNC(codegen_or)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	uint32_t cs=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);
	pushListItemToQueue(rest,queue,NULL);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_or_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createDefaultQueueNextExpression(queue)
			,cs
			,cms
			,curEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}

void fklUpdatePrototype(FklFuncPrototypes* cp
		,FklCodegenEnv* env
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* pst)
{
	FklFuncPrototype* pts=&cp->pts[env->prototypeId];
	FklHashTable* eht=NULL;
	pts->lcount=env->lcount;
	process_unresolve_ref(env,cp);
	eht=&env->refs;
	uint32_t count=eht->num;
	FklSymbolDef* refs=(FklSymbolDef*)fklRealloc(pts->refs,sizeof(FklSymbolDef)*count);
	FKL_ASSERT(refs||!count);
	pts->refs=refs;
	pts->rcount=count;
	FklHashTableItem* list=eht->first;
	if(pts->is_top)
	{
		uint32_t builtin_symbol_number=fklGetBuiltinSymbolNum();
		for(uint32_t i=0;i<builtin_symbol_number;i++)
			list=list->next;
	}
	for(;list;list=list->next)
	{
		FklSymbolDef* sd=(FklSymbolDef*)list->data;
		FklSid_t sid=fklAddSymbol(fklGetSymbolWithId(sd->k.id,pst)->symbol,globalSymTable)->id;
		FklSymbolDef ref={.k.id=sid,
			.k.scope=sd->k.scope,
			.idx=sd->idx,
			.cidx=sd->cidx,
			.isLocal=sd->isLocal};
		refs[sd->idx]=ref;
	}
}

FklCodegenEnv* fklCreateCodegenEnv(FklCodegenEnv* prev
		,uint32_t pscope
		,FklCodegenMacroScope* macroScope)
{
	FklCodegenEnv* r=(FklCodegenEnv*)malloc(sizeof(FklCodegenEnv));
	FKL_ASSERT(r);
	r->pscope=pscope;
	r->sc=0;
	r->scopes=NULL;
	enter_new_scope(0,r);
	r->prototypeId=1;
	r->prev=prev;
	r->refcount=0;
	r->lcount=0;
	r->slotFlags=NULL;
	fklInitHashTable(&r->refs,&CodegenEnvHashMethodTable);
	fklInitPtrStack(&r->uref,8,8);
	r->macros=make_macro_scope_ref(fklCreateCodegenMacroScope(macroScope));
	if(prev)
		prev->refcount+=1;
	return r;
}

void fklDestroyCodegenEnv(FklCodegenEnv* env)
{
	while(env)
	{
		env->refcount-=1;
		if(!env->refcount)
		{
			FklCodegenEnv* cur=env;
			env=env->prev;
			uint32_t sc=cur->sc;
			FklCodegenEnvScope* scopes=cur->scopes;
			for(uint32_t i=0;i<sc;i++)
				fklUninitHashTable(&scopes[i].defs);
			free(scopes);
			free(cur->slotFlags);
			fklUninitHashTable(&cur->refs);
			FklPtrStack* unref=&cur->uref;
			while(!fklIsPtrStackEmpty(unref))
				free(fklPopPtrStack(unref));
			fklUninitPtrStack(unref);
			fklDestroyCodegenMacroScope(cur->macros);
			free(cur);
		}
		else
			return;
	}
}

void fklAddReplacementBySid(FklSid_t id,FklNastNode* node,FklHashTable* reps)
{
	FklCodegenReplacement* rep=fklPutHashItem(&id,reps);
	fklDestroyNastNode(rep->node);
	rep->node=fklMakeNastNodeRef(node);
}

int fklIsReplacementDefined(FklSid_t id,FklCodegenEnv* env)
{
	return fklGetHashItem(&id,env->macros->replacements)!=NULL;
}

FklNastNode* fklGetReplacement(FklSid_t id,FklHashTable* rep)
{
	FklCodegenReplacement* replace=fklGetHashItem(&id,rep);
	if(replace)
		return fklMakeNastNodeRef(replace->node);
	return NULL;
}

static CODEGEN_FUNC(codegen_lambda)
{
	FklNastNode* args=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklCodegenEnv* lambdaCodegenEnv=fklCreateCodegenEnv(curEnv,scope,macroScope);
	FklByteCodelnt* argsBc=processArgs(args,lambdaCodegenEnv,codegen);
	if(!argsBc)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		lambdaCodegenEnv->refcount++;
		fklDestroyCodegenEnv(lambdaCodegenEnv);
		return;
	}
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	FklPtrStack* stack=fklCreatePtrStack(32,16);
	fklPushPtrStack(argsBc,stack);
	create_and_insert_to_pool(codegen->pts
			,curEnv->prototypeId
			,lambdaCodegenEnv
			,codegen->globalSymTable
			,0
			,codegen->fid
			,origExp->curline
			,&outer_ctx->public_symbol_table);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_lambda_exp_bc_process
			,createDefaultStackContext(stack)
			,createDefaultQueueNextExpression(queue)
			,1
			,lambdaCodegenEnv->macros
			,lambdaCodegenEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_define)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklPtrQueue* queue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_def_var_exp_bc_process
			,create_def_var_context(name->sym,scope,name->curline)
			,createMustHasRetvalQueueNextExpression(queue)
			,scope
			,macroScope
			,curEnv
			,name->curline
			,codegen
			,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_defun)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* args=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}

	FklCodegenEnv* lambdaCodegenEnv=fklCreateCodegenEnv(curEnv,scope,macroScope);
	FklByteCodelnt* argsBc=processArgs(args,lambdaCodegenEnv,codegen);
	if(!argsBc)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		lambdaCodegenEnv->refcount++;
		fklDestroyCodegenEnv(lambdaCodegenEnv);
		return;
	}

	FklCodegenQuest* prevQuest=createCodegenQuest(_def_var_exp_bc_process
			,create_def_var_context(name->sym,scope,name->curline)
			,NULL
			,scope
			,macroScope
			,curEnv
			,name->curline
			,NULL
			,codegen);
	fklPushPtrStack(prevQuest,codegenQuestStack);

	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	FklPtrStack* lStack=fklCreatePtrStack(32,16);
	fklPushPtrStack(argsBc,lStack);
	create_and_insert_to_pool(codegen->pts
			,curEnv->prototypeId
			,lambdaCodegenEnv
			,codegen->globalSymTable
			,get_sid_in_gs_with_id_in_ps(name->sym,codegen->globalSymTable,pst)
			,codegen->fid
			,origExp->curline
			,pst);
	FklNastNode* name_str=fklCreateNastNode(FKL_NAST_STR,name->curline);
	name_str->str=fklCopyString(fklGetSymbolWithId(name->sym,pst)->symbol);
	fklAddReplacementBySid(fklAddSymbolCstr("*func*",pst)->id
			,name_str
			,lambdaCodegenEnv->macros->replacements);
	fklDestroyNastNode(name_str);
	FklCodegenQuest* cur=createCodegenQuest(_lambda_exp_bc_process
			,createDefaultStackContext(lStack)
			,createDefaultQueueNextExpression(queue)
			,1
			,lambdaCodegenEnv->macros
			,lambdaCodegenEnv
			,rest->curline
			,prevQuest
			,codegen);
	fklPushPtrStack(cur,codegenQuestStack);
}

static inline void mark_modify_builtin_ref(uint32_t idx
		,FklCodegenEnv* env
		,FklCodegenInfo* codegen)
{
	FklSymbolDef* ref=NULL;
	while(env)
	{
		FklHashTableItem* list=env->refs.first;
		for(;list;list=list->next)
		{
			FklSymbolDef* def=(FklSymbolDef*)list->data;
			if(def->idx==idx)
			{
				if(def->isLocal)
					env=NULL;
				else
				{
					if(!env->prev)
						ref=def;
					else
						idx=def->cidx;
					env=env->prev;
				}
				break;
			}
		}
		if(!list)
			break;
	}
	if(ref&&idx<codegen->builtinSymbolNum)
		codegen->builtinSymModiMark[idx]=1;
}

static CODEGEN_FUNC(codegen_setq)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklPtrStack* stack=fklCreatePtrStack(2,1);
	fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
	FklSymbolDef* def=fklFindSymbolDefByIdAndScope(name->sym,scope,curEnv);
	if(def)
		fklPushPtrStack(makePutLoc(name->curline,name->sym,def->idx,codegen),stack);
	else
	{
		uint32_t idx=fklAddCodegenRefBySid(name->sym,curEnv,codegen->fid,name->curline);
		mark_modify_builtin_ref(idx,curEnv,codegen);
		fklPushPtrStack(makePutRefLoc(name,idx,codegen),stack);
	}
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_set_var_exp_bc_process
			,createDefaultStackContext(stack)
			,createMustHasRetvalQueueNextExpression(queue)
			,scope
			,macroScope
			,curEnv
			,name->curline
			,codegen
			,codegenQuestStack);
}

static inline void push_default_codegen_quest(FklNastNode* value
		,FklPtrStack* codegenQuestStack
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklCodegenEnv* curEnv
		,FklCodegenQuest* prev
		,FklCodegenInfo* codegen)
{
	FklPtrStack* stack=fklCreatePtrStack(1,1);
	fklPushPtrStack(create_bc_lnt(fklCodegenNode(value,codegen),codegen->fid,value->curline),stack);
	FklCodegenQuest* quest=createCodegenQuest(_default_bc_process
			,createDefaultStackContext(stack)
			,NULL
			,scope
			,macroScope
			,curEnv
			,value->curline
			,prev
			,codegen);
	fklPushPtrStack(quest,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_macroexpand)
{
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	fklMakeNastNodeRef(value);
	if(value->type==FKL_NAST_PAIR)
		value=fklTryExpandCodegenMacro(value,codegen,curEnv->macros,errorState);
	if(errorState->type)
		return;
	push_default_codegen_quest(value
			,codegenQuestStack
			,scope
			,macroScope
			,curEnv
			,NULL
			,codegen);
	fklDestroyNastNode(value);
}

static inline void uninit_codegen_macro(FklCodegenMacro* macro)
{
	fklDestroyNastNode(macro->pattern);
	fklDestroyNastNode(macro->origin_exp);
	if(macro->own)
		fklDestroyByteCodelnt(macro->bcl);
}

static void add_compiler_macro(FklCodegenMacro** pmacro
		,FklNastNode* pattern
		,FklNastNode* origin_exp
		,FklByteCodelnt* bcl
		,uint32_t prototype_id
		,int own)
{
	int coverState=FKL_PATTERN_NOT_EQUAL;
	FklCodegenMacro** phead=pmacro;
	for(FklCodegenMacro* cur=*pmacro;cur;pmacro=&cur->next,cur=cur->next)
	{
		coverState=fklPatternCoverState(cur->pattern,pattern);
		if(coverState==FKL_PATTERN_BE_COVER)
			phead=&cur->next;
		else if(coverState)
			break;
	}
	if(coverState==FKL_PATTERN_NOT_EQUAL)
	{
		FklCodegenMacro* macro=fklCreateCodegenMacro(pattern,origin_exp,bcl,*phead,prototype_id,own);
		*phead=macro;
	}
	else if(coverState==FKL_PATTERN_EQUAL)
	{
		FklCodegenMacro* macro=*pmacro;
		uninit_codegen_macro(macro);
		macro->own=own;
		macro->prototype_id=prototype_id;
		macro->pattern=pattern;
		macro->bcl=bcl;
		macro->origin_exp=origin_exp;
	}
	else
	{
		FklCodegenMacro* next=*pmacro;
		*pmacro=fklCreateCodegenMacro(pattern,origin_exp,bcl,next,prototype_id,own);
	}
}

BC_PROCESS(_export_macro_bc_process)
{
	FklCodegenMacroScope* target_macro_scope=cms->prev;
	for(;codegen&&!codegen->libMark;codegen=codegen->prev);
	for(FklCodegenMacro* cur=cms->head;cur;cur=cur->next)
	{
		cur->own=0;
		add_compiler_macro(&codegen->export_macro,fklMakeNastNodeRef(cur->pattern),fklMakeNastNodeRef(cur->origin_exp),cur->bcl,cur->prototype_id,1);
		add_compiler_macro(&target_macro_scope->head,fklMakeNastNodeRef(cur->pattern),fklMakeNastNodeRef(cur->origin_exp),cur->bcl,cur->prototype_id,0);
	}
	for(FklHashTableItem* cur=cms->replacements->first;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->data;
		fklAddReplacementBySid(rep->id,rep->node,codegen->export_replacement);
		fklAddReplacementBySid(rep->id,rep->node,target_macro_scope->replacements);
	}
	return NULL;
}

static inline void add_export_symbol(FklCodegenInfo* libCodegen
		,FklNastNode* origExp
		,FklNastNode* rest
		,FklPtrQueue* exportQueue)
{
	FklNastPair* prevPair=origExp->pair;
	FklNastNode* exportHead=origExp->pair->car;
	for(;rest->type==FKL_NAST_PAIR;rest=rest->pair->cdr)
	{
		FklNastNode* restExp=fklNastCons(fklMakeNastNodeRef(exportHead)
				,rest
				,rest->curline);
		fklPushPtrQueue(restExp,exportQueue);
		prevPair->cdr=fklCreateNastNode(FKL_NAST_NIL,rest->curline);
		prevPair=rest->pair;
	}
}

static inline void push_single_bcl_codegen_quest(FklByteCodelnt* bcl
		,FklPtrStack* codegenQuestStack
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklCodegenEnv* curEnv
		,FklCodegenQuest* prev
		,FklCodegenInfo* codegen
		,uint64_t curline)
{
	FklPtrStack* stack=fklCreatePtrStack(1,1);
	fklPushPtrStack(bcl,stack);
	FklCodegenQuest* quest=createCodegenQuest(_default_bc_process
			,createDefaultStackContext(stack)
			,NULL
			,scope
			,macroScope
			,curEnv
			,curline
			,prev
			,codegen);
	fklPushPtrStack(quest,codegenQuestStack);
}

typedef FklNastNode* (*ReplacementFunc)(const FklNastNode* orig
		,const FklCodegenEnv* env
		,const FklCodegenInfo* codegen);

static inline ReplacementFunc findBuiltInReplacementWithId(FklSid_t id,const FklSid_t builtin_replacement_id[]);

static inline int is_replacement_define(const FklNastNode* value
		,const FklCodegenInfo* info
		,const FklCodegenOuterCtx* ctx
		,const FklCodegenMacroScope* macroScope
		,const FklCodegenEnv* curEnv)
{
	FklNastNode* replacement=NULL;
	ReplacementFunc f=NULL;
	for(const FklCodegenMacroScope* cs=macroScope;cs&&!replacement;cs=cs->prev)
		replacement=fklGetReplacement(value->sym,cs->replacements);
	if(replacement)
	{
		fklDestroyNastNode(replacement);
		return 1;
	}
	else if((f=findBuiltInReplacementWithId(value->sym,ctx->builtin_replacement_id)))
		return 1;
	else
		return 0;
}

static inline int is_replacement_true(const FklNastNode* value
		,const FklCodegenInfo* info
		,const FklCodegenOuterCtx* ctx
		,const FklCodegenMacroScope* macroScope
		,const FklCodegenEnv* curEnv)
{
	FklNastNode* replacement=NULL;
	ReplacementFunc f=NULL;
	for(const FklCodegenMacroScope* cs=macroScope;cs&&!replacement;cs=cs->prev)
		replacement=fklGetReplacement(value->sym,cs->replacements);
	if(replacement)
	{
		int r=replacement->type!=FKL_NAST_NIL;
		fklDestroyNastNode(replacement);
		return r;
	}
	else if((f=findBuiltInReplacementWithId(value->sym,ctx->builtin_replacement_id)))
	{
		FklNastNode* t=f(value,curEnv,info);
		int r=t->type!=FKL_NAST_NIL;
		fklDestroyNastNode(t);
		return r;
	}
	else
		return 0;
}

static inline FklNastNode* get_replacement(const FklNastNode* value
		,const FklCodegenInfo* info
		,const FklCodegenOuterCtx* ctx
		,const FklCodegenMacroScope* macroScope
		,const FklCodegenEnv* curEnv)
{
	FklNastNode* replacement=NULL;
	ReplacementFunc f=NULL;
	for(const FklCodegenMacroScope* cs=macroScope;cs&&!replacement;cs=cs->prev)
		replacement=fklGetReplacement(value->sym,cs->replacements);
	if(replacement)
		return replacement;
	else if((f=findBuiltInReplacementWithId(value->sym,ctx->builtin_replacement_id)))
		return f(value,curEnv,info);
	else
		return NULL;
}

static inline char* combineFileNameFromList(const FklNastNode* list,const FklSymbolTable* pst);

static inline int is_valid_compile_check_pattern(const FklNastNode* p)
{
	return p->type==FKL_NAST_PAIR
		&&p->type!=FKL_NAST_NIL
		&&p->pair->car->type==FKL_NAST_SYM
		&&p->pair->cdr->type==FKL_NAST_PAIR
		&&p->pair->cdr->pair->cdr->type==FKL_NAST_NIL;
}

static inline int is_compile_check_pattern_slot_node(FklSid_t slot_id,const FklNastNode* n)
{
	return n->type==FKL_NAST_PAIR
		&&n->type!=FKL_NAST_NIL
		&&n->pair->car->type==FKL_NAST_SYM
		&&n->pair->car->sym==slot_id
		&&n->pair->cdr->type==FKL_NAST_PAIR
		&&n->pair->cdr->pair->cdr->type==FKL_NAST_NIL;
}

static inline int is_compile_check_pattern_matched(const FklNastNode* p,const FklNastNode* e)
{
	FklPtrStack s0;
	fklInitPtrStack(&s0,4,8);
	FklPtrStack s1;
	fklInitPtrStack(&s1,4,8);
	fklPushPtrStack((void*)cadr_nast_node(p),&s0);
	fklPushPtrStack((void*)e,&s1);
	FklSid_t slot_id=p->pair->car->sym;
	int r=1;
	while(!fklIsPtrStackEmpty(&s0))
	{
		const FklNastNode* p=fklPopPtrStack(&s0);
		const FklNastNode* e=fklPopPtrStack(&s1);
		if(is_compile_check_pattern_slot_node(slot_id,p))
			continue;
		else if(p->type==e->type)
		{
			switch(p->type)
			{
				case FKL_NAST_BOX:
					fklPushPtrStack((void*)p->box,&s0);
					fklPushPtrStack((void*)e->box,&s1);
					break;
				case FKL_NAST_PAIR:
					fklPushPtrStack((void*)p->pair->car,&s0);
					fklPushPtrStack((void*)p->pair->cdr,&s0);
					fklPushPtrStack((void*)e->pair->car,&s1);
					fklPushPtrStack((void*)e->pair->cdr,&s1);
					break;
				case FKL_NAST_VECTOR:
					if(p->vec->size!=e->vec->size)
					{
						r=0;
						goto exit;
					}
					for(size_t i=0;i<p->vec->size;i++)
					{
						fklPushPtrStack((void*)p->vec->base[i],&s0);
						fklPushPtrStack((void*)e->vec->base[i],&s1);
					}
					break;
				case FKL_NAST_HASHTABLE:
					if(p->hash->num!=e->hash->num)
					{
						r=0;
						goto exit;
					}
					for(size_t i=0;i<p->hash->num;i++)
					{
						fklPushPtrStack((void*)p->hash->items[i].car,&s0);
						fklPushPtrStack((void*)p->hash->items[i].cdr,&s0);
						fklPushPtrStack((void*)e->hash->items[i].car,&s1);
						fklPushPtrStack((void*)e->hash->items[i].cdr,&s1);
					}
					break;
				default:
					if(!fklNastNodeEqual(p,e))
					{
						r=0;
						goto exit;
					}
			}
		}
		else
		{
			r=0;
			goto exit;
		}
	}

exit:
	fklUninitPtrStack(&s0);
	fklUninitPtrStack(&s1);
	return r;
}

static inline int is_check_subpattern_true(const FklNastNode* value
		,const FklCodegenInfo* info
		,const FklCodegenOuterCtx* ctx
		,uint32_t scope
		,const FklCodegenMacroScope* macroScope
		,const FklCodegenEnv* curEnv
		,FklCodegenErrorState* errorState)
{
	if(value->type==FKL_NAST_PAIR)
	{
#define COND_COMPILE_CHECK_OP_TRUE (0)
#define COND_COMPILE_CHECK_OP_NOT (1)
#define COND_COMPILE_CHECK_OP_AND (2)
#define COND_COMPILE_CHECK_OP_OR (3)

		FklHashTable ht;
		fklInitPatternMatchHashTable(&ht);

		FklPtrStack expStack;
		fklInitPtrStack(&expStack,1,8);
		fklPushPtrStack(NULL,&expStack);
		fklPushPtrStack((void*)value,&expStack);

		FklU8Stack boolStack;
		fklInitU8Stack(&boolStack,1,8);
		fklPushU8Stack(255,&boolStack);

		FklU8Stack opStack;
		fklInitU8Stack(&opStack,1,8);
		fklPushU8Stack(COND_COMPILE_CHECK_OP_TRUE,&opStack);

		FklPtrStack tmpStack;
		fklInitPtrStack(&tmpStack,1,8);
		uint8_t r=0;
		while(!fklIsU8StackEmpty(&opStack))
		{
			uint8_t r=fklPopU8Stack(&boolStack);
			FklNastNode* cur=NULL;
			if(r==255)
			{
				cur=fklPopPtrStack(&expStack);
				if(cur)
				{
					if(cur->type==FKL_NAST_PAIR)
					{
						if(fklPatternMatch(ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_DEFINE],cur,&ht))
						{
							const FklNastNode* value=fklPatternMatchingHashTableRef(ctx->builtInPatternVar_value,&ht);
							if(value->type!=FKL_NAST_SYM)
								goto exit;
							r=fklFindSymbolDefByIdAndScope(value->sym,scope,curEnv)!=NULL;
						}
						else if(fklPatternMatch(ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_IMPORT],cur,&ht))
						{
							const FklNastNode* value=fklPatternMatchingHashTableRef(ctx->builtInPatternVar_value,&ht);
							if(value->type==FKL_NAST_NIL||!fklIsNastNodeListAndHasSameType(value,FKL_NAST_SYM))
							{
								errorState->type=FKL_ERR_SYNTAXERROR;
								errorState->place=fklMakeNastNodeRef(cur);
								goto exit;
							}
							char* filename=combineFileNameFromList(value,&ctx->public_symbol_table);

							char* packageMainFileName=fklStrCat(fklCopyCstr(filename),FKL_PATH_SEPARATOR_STR);
							packageMainFileName=fklStrCat(packageMainFileName,FKL_PACKAGE_MAIN_FILE);

							char* preCompileFileName=fklStrCat(fklCopyCstr(packageMainFileName),FKL_PRE_COMPILE_FKL_SUFFIX_STR);

							char* scriptFileName=fklStrCat(fklCopyCstr(filename),FKL_SCRIPT_FILE_EXTENSION);

							char* dllFileName=fklStrCat(fklCopyCstr(filename),FKL_DLL_FILE_TYPE);

							r=fklIsAccessibleRegFile(packageMainFileName)
								||fklIsAccessibleRegFile(scriptFileName)
								||fklIsAccessibleRegFile(preCompileFileName)
								||fklIsAccessibleRegFile(dllFileName);
							free(filename);
							free(scriptFileName);
							free(dllFileName);
							free(packageMainFileName);
							free(preCompileFileName);
						}
						else if(fklPatternMatch(ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_DEFMACRO],cur,&ht))
						{
							const FklNastNode* value=fklPatternMatchingHashTableRef(ctx->builtInPatternVar_value,&ht);
							if(value->type==FKL_NAST_SYM)
								r=is_replacement_define(value,info,ctx,macroScope,curEnv);
							else if(value->type==FKL_NAST_PAIR
									&&value->pair->cdr->type==FKL_NAST_NIL
									&&value->pair->car->type==FKL_NAST_SYM)
							{
								r=0;
								FklSid_t id=value->pair->car->sym;
								for(const FklCodegenMacroScope* macros=macroScope
										;macros
										;macros=macros->prev)
								{
									for(const FklCodegenMacro* cur=macros->head
											;cur
											;cur=cur->next)
									{
										if(id==cur->pattern->pair->car->sym)
										{
											r=1;
											break;
										}
									}
									if(r==1)
										break;
								}
							}
							else if(value->type==FKL_NAST_VECTOR
									&&value->vec->size==1
									&&value->vec->base[0]->type==FKL_NAST_SYM)
							{
								FklSid_t id=value->vec->base[0]->sym;
								r=*(info->g)!=NULL
									&&fklGetHashItem(&id,info->named_prod_groups)!=NULL;
							}
							else
							{
								errorState->type=FKL_ERR_SYNTAXERROR;
								errorState->place=fklMakeNastNodeRef(cur);
								goto exit;
							}
						}
						else if(fklPatternMatch(ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_EQ],cur,&ht))
						{
							const FklNastNode* arg0=fklPatternMatchingHashTableRef(ctx->builtInPatternVar_arg0,&ht);
							const FklNastNode* arg1=fklPatternMatchingHashTableRef(ctx->builtInPatternVar_arg1,&ht);
							if(arg0->type!=FKL_NAST_SYM)
							{
								errorState->type=FKL_ERR_SYNTAXERROR;
								errorState->place=fklMakeNastNodeRef(cur);
								goto exit;
							}
							FklNastNode* node=get_replacement(arg0,info,ctx,macroScope,curEnv);
							if(node)
							{
								r=fklNastNodeEqual(node,arg1);
								fklDestroyNastNode(node);
							}
							else
								r=0;
						}
						else if(fklPatternMatch(ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_MATCH],cur,&ht))
						{
							const FklNastNode* arg0=fklPatternMatchingHashTableRef(ctx->builtInPatternVar_arg0,&ht);
							const FklNastNode* arg1=fklPatternMatchingHashTableRef(ctx->builtInPatternVar_arg1,&ht);
							if(arg0->type!=FKL_NAST_SYM||!is_valid_compile_check_pattern(arg1))
							{
								errorState->type=FKL_ERR_SYNTAXERROR;
								errorState->place=fklMakeNastNodeRef(cur);
								goto exit;
							}
							FklNastNode* node=get_replacement(arg0,info,ctx,macroScope,curEnv);
							if(node)
							{
								r=is_compile_check_pattern_matched(arg1,node);
								fklDestroyNastNode(node);
							}
							else
								r=0;
						}
						else if(fklPatternMatch(ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_NOT],cur,&ht))
						{
							fklPushU8Stack(255,&boolStack);
							fklPushU8Stack(COND_COMPILE_CHECK_OP_NOT,&opStack);
							fklPushPtrStack((void*)fklPatternMatchingHashTableRef(ctx->builtInPatternVar_value,&ht),&expStack);
							continue;
						}
						else if(fklPatternMatch(ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_AND],cur,&ht))
						{
							const FklNastNode* rest=fklPatternMatchingHashTableRef(ctx->builtInPatternVar_rest,&ht);
							if(rest->type==FKL_NAST_NIL)
								r=1;
							else if(fklIsNastNodeList(rest))
							{
								fklPushU8Stack(255,&boolStack);
								fklPushPtrStack(NULL,&expStack);
								fklPushU8Stack(COND_COMPILE_CHECK_OP_AND,&opStack);
								for(;rest->type==FKL_NAST_PAIR;rest=rest->pair->cdr)
									fklPushPtrStack((void*)rest->pair->car,&tmpStack);
								while(!fklIsPtrStackEmpty(&tmpStack))
									fklPushPtrStack(fklPopPtrStack(&tmpStack),&expStack);
								continue;
							}
							else
							{
								errorState->type=FKL_ERR_SYNTAXERROR;
								errorState->place=fklMakeNastNodeRef(cur);
								goto exit;
							}
						}
						else if(fklPatternMatch(ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_OR],cur,&ht))
						{
							const FklNastNode* rest=fklPatternMatchingHashTableRef(ctx->builtInPatternVar_rest,&ht);
							if(rest->type==FKL_NAST_NIL)
								r=0;
							else if(fklIsNastNodeList(rest))
							{
								fklPushU8Stack(255,&boolStack);
								fklPushPtrStack(NULL,&expStack);
								fklPushU8Stack(COND_COMPILE_CHECK_OP_OR,&opStack);
								for(;rest->type==FKL_NAST_PAIR;rest=rest->pair->cdr)
									fklPushPtrStack((void*)rest->pair->car,&tmpStack);
								while(!fklIsPtrStackEmpty(&tmpStack))
									fklPushPtrStack(fklPopPtrStack(&tmpStack),&expStack);
								continue;
							}
							else
							{
								errorState->type=FKL_ERR_SYNTAXERROR;
								errorState->place=fklMakeNastNodeRef(cur);
								goto exit;
							}
						}
						else
						{
							errorState->type=FKL_ERR_SYNTAXERROR;
							errorState->place=fklMakeNastNodeRef(cur);
							goto exit;
						}
					}
					else if(cur->type==FKL_NAST_NIL)
						r=0;
					else if(cur->type==FKL_NAST_SYM)
						r=is_replacement_true(cur,info,ctx,macroScope,curEnv);
					else
						r=1;
				}
			}
			if(r==255)
				fklPushU8Stack(255,&boolStack);
			uint8_t cond_compile_check_op=fklTopU8Stack(&opStack);
			switch(cond_compile_check_op)
			{
				case COND_COMPILE_CHECK_OP_TRUE:
					fklPushU8Stack(r,&boolStack);
					break;
				case COND_COMPILE_CHECK_OP_NOT:
					fklPushU8Stack(!r,&boolStack);
					break;
				case COND_COMPILE_CHECK_OP_AND:
					if(r==255)
					{
						fklPopU8Stack(&boolStack);
						fklPushU8Stack(1,&boolStack);
					}
					else if(r==0)
					{
						for(;cur;cur=fklPopPtrStack(&expStack));
						fklPushU8Stack(0,&boolStack);
					}
					else
					{
						fklPushU8Stack(255,&boolStack);
						continue;
					}
					break;
				case COND_COMPILE_CHECK_OP_OR:
					if(r==255)
					{
						fklPopU8Stack(&boolStack);
						fklPushU8Stack(0,&boolStack);
					}
					else if(r==1)
					{
						for(;cur;cur=fklPopPtrStack(&expStack));
						fklPushU8Stack(1,&boolStack);
					}
					else
					{
						fklPushU8Stack(255,&boolStack);
						continue;
					}
					break;
			}
			fklPopU8Stack(&opStack);
		}

#undef COND_COMPILE_CHECK_OP_OR
#undef COND_COMPILE_CHECK_OP_AND
#undef COND_COMPILE_CHECK_OP_NOT
#undef COND_COMPILE_CHECK_OP_TRUE

		r=fklPopU8Stack(&boolStack);
exit:
		fklUninitU8Stack(&boolStack);
		fklUninitU8Stack(&opStack);
		fklUninitPtrStack(&tmpStack);
		fklUninitPtrStack(&expStack);
		fklUninitHashTable(&ht);
		return r;
	}
	else if(value->type==FKL_NAST_NIL)
		return 0;
	else if(value->type==FKL_NAST_SYM)
		return is_replacement_true(value,info,ctx,macroScope,curEnv);
	else
		return 1;
}

static CODEGEN_FUNC(codegen_check)
{
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	int r=is_check_subpattern_true(value
			,codegen
			,outer_ctx
			,scope
			,macroScope
			,curEnv
			,errorState);
	if(errorState->type)
		return;
	FklByteCodelnt* bcl=NULL;
	bcl=fklCreateSingleInsBclnt(create_op_ins(r?FKL_OP_PUSH_1:FKL_OP_PUSH_NIL),codegen->fid,origExp->curline);
	push_single_bcl_codegen_quest(bcl
			,codegenQuestStack
			,scope
			,macroScope
			,curEnv
			,NULL
			,codegen
			,origExp->curline);
}

static CODEGEN_FUNC(codegen_cond_compile)
{
	FklNastNode* cond=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_cond,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	if(fklNastListLength(rest)%2==1)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	int r=is_check_subpattern_true(cond
			,codegen
			,outer_ctx
			,scope
			,macroScope
			,curEnv
			,errorState);
	if(errorState->type)
		return;
	if(r)
	{
		FklPtrQueue* q=fklCreatePtrQueue();
		fklPushPtrQueue(fklMakeNastNodeRef(value),q);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process
				,createDefaultStackContext(fklCreatePtrStack(1,16))
				,createDefaultQueueNextExpression(q)
				,scope
				,macroScope
				,curEnv
				,value->curline
				,codegen
				,codegenQuestStack);
	}
	else
	{
		while(rest->type==FKL_NAST_PAIR)
		{
			FklNastNode* cond=rest->pair->car;
			rest=rest->pair->cdr;
			FklNastNode* value=rest->pair->car;
			rest=rest->pair->cdr;
			int r=is_check_subpattern_true(cond
					,codegen
					,outer_ctx
					,scope
					,macroScope
					,curEnv
					,errorState);
			if(errorState->type)
				return;
			if(r)
			{
				FklPtrQueue* q=fklCreatePtrQueue();
				fklPushPtrQueue(fklMakeNastNodeRef(value),q);
				FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process
						,createDefaultStackContext(fklCreatePtrStack(1,16))
						,createDefaultQueueNextExpression(q)
						,scope
						,macroScope
						,curEnv
						,value->curline
						,codegen
						,codegenQuestStack);
				break;
			}
		}
	}
}

static CODEGEN_FUNC(codegen_quote)
{
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	push_default_codegen_quest(value
			,codegenQuestStack
			,scope
			,macroScope
			,curEnv
			,NULL
			,codegen);
}

static inline void unquoteHelperFunc(FklNastNode* value
		,FklPtrStack* codegenQuestStack
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklCodegenEnv* curEnv
		,FklByteCodeProcesser func
		,FklCodegenQuest* prev
		,FklCodegenInfo* codegen)
{
	FklPtrQueue* queue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
	FklCodegenQuest* quest=createCodegenQuest(func
			,createDefaultStackContext(fklCreatePtrStack(1,1))
			,createMustHasRetvalQueueNextExpression(queue)
			,scope
			,macroScope
			,curEnv
			,value->curline
			,prev
			,codegen);
	fklPushPtrStack(quest,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_unquote)
{
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	unquoteHelperFunc(value
			,codegenQuestStack
			,scope
			,macroScope
			,curEnv
			,_default_bc_process
			,NULL
			,codegen);
}

BC_PROCESS(_qsquote_box_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=fklPopPtrStack(stack);
	FklInstruction pushBox=create_op_ins(FKL_OP_PUSH_BOX);
	fklBytecodeLntPushFrontIns(retval,&pushBox,fid,line);
	return retval;
}

BC_PROCESS(_qsquote_vec_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=stack->base[0];
	for(size_t i=1;i<stack->top;i++)
	{
		FklByteCodelnt* cur=stack->base[i];
		fklCodeLntConcat(retval,cur);
		fklDestroyByteCodelnt(cur);
	}
	stack->top=0;
	FklInstruction pushVec=create_op_ins(FKL_OP_PUSH_VECTOR_0);
	FklInstruction setBp=create_op_ins(FKL_OP_SET_BP);
	fklBytecodeLntPushBackIns(&setBp,retval,fid,line);
	fklBytecodeLntPushFrontIns(retval,&pushVec,fid,line);
	return retval;
}

BC_PROCESS(_unqtesp_vec_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=fklPopPtrStack(stack);
	FklByteCodelnt* other=fklPopPtrStack(stack);
	if(other)
	{
		while(!fklIsPtrStackEmpty(stack))
		{
			FklByteCodelnt* cur=fklPopPtrStack(stack);
			fklCodeLntConcat(other,cur);
			fklDestroyByteCodelnt(cur);
		}
		fklCodeLntReverseConcat(other,retval);
		fklDestroyByteCodelnt(other);
	}
	FklInstruction listPush=create_op_ins(FKL_OP_LIST_PUSH);
	fklBytecodeLntPushFrontIns(retval,&listPush,fid,line);
	return retval;
}

BC_PROCESS(_qsquote_pair_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=stack->base[0];
	for(size_t i=1;i<stack->top;i++)
	{
		FklByteCodelnt* cur=stack->base[i];
		fklCodeLntConcat(retval,cur);
		fklDestroyByteCodelnt(cur);
	}
	stack->top=0;
	FklInstruction pushPair=create_op_ins(FKL_OP_PUSH_LIST_0);
	FklInstruction setBp=create_op_ins(FKL_OP_SET_BP);
	fklBytecodeLntPushBackIns(&setBp,retval,fid,line);
	fklBytecodeLntPushFrontIns(retval,&pushPair,fid,line);
	return retval;
}

BC_PROCESS(_qsquote_list_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=stack->base[0];
	for(size_t i=1;i<stack->top;i++)
	{
		FklByteCodelnt* cur=stack->base[i];
		fklCodeLntConcat(retval,cur);
		fklDestroyByteCodelnt(cur);
	}
	stack->top=0;
	FklInstruction pushPair=create_op_ins(FKL_OP_LIST_APPEND);
	fklBytecodeLntPushFrontIns(retval,&pushPair,fid,line);
	return retval;
}

typedef enum
{
	QSQUOTE_NONE=0,
	QSQUOTE_UNQTESP_CAR,
	QSQUOTE_UNQTESP_VEC,
}QsquoteHelperEnum;

typedef struct
{
	QsquoteHelperEnum e;
	FklNastNode* node;
	FklCodegenQuest* prev;
}QsquoteHelperStruct;

QsquoteHelperStruct* createQsquoteHelperStruct(QsquoteHelperEnum e,FklNastNode* node,FklCodegenQuest* prev)
{
	QsquoteHelperStruct* r=(QsquoteHelperStruct*)malloc(sizeof(QsquoteHelperStruct));
	FKL_ASSERT(r);
	r->e=e;
	r->node=node;
	r->prev=prev;
	return r;
}

static CODEGEN_FUNC(codegen_qsquote)
{
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	FklPtrStack valueStack=FKL_STACK_INIT;
	fklInitPtrStack(&valueStack,8,16);
	fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,value,NULL),&valueStack);
	FklNastNode* const* builtInSubPattern=outer_ctx->builtin_sub_pattern_node;
	while(!fklIsPtrStackEmpty(&valueStack))
	{
		QsquoteHelperStruct* curNode=fklPopPtrStack(&valueStack);
		QsquoteHelperEnum e=curNode->e;
		FklCodegenQuest* prevQuest=curNode->prev;
		FklNastNode* curValue=curNode->node;
		free(curNode);
		if(e==QSQUOTE_UNQTESP_CAR)
			unquoteHelperFunc(curValue
					,codegenQuestStack
					,scope
					,macroScope
					,curEnv
					,_default_bc_process
					,prevQuest,codegen);
		else if(e==QSQUOTE_UNQTESP_VEC)
			unquoteHelperFunc(curValue
					,codegenQuestStack
					,scope
					,macroScope
					,curEnv
					,_unqtesp_vec_bc_process
					,prevQuest
					,codegen);
		else
		{
			FklHashTable* unquoteHt=fklCreatePatternMatchingHashTable();
			if(fklPatternMatch(builtInSubPattern[FKL_CODEGEN_SUB_PATTERN_UNQUOTE],curValue,unquoteHt))
			{
				FklNastNode* unquoteValue=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,unquoteHt);
				unquoteHelperFunc(unquoteValue
						,codegenQuestStack
						,scope
						,macroScope
						,curEnv
						,_default_bc_process
						,prevQuest
						,codegen);
			}
			else if(curValue->type==FKL_NAST_PAIR)
			{
				FklCodegenQuest* curQuest=createCodegenQuest(_qsquote_pair_bc_process
						,createDefaultStackContext(fklCreatePtrStack(2,1))
						,NULL
						,scope
						,macroScope
						,curEnv
						,curValue->curline
						,prevQuest
						,codegen);
				fklPushPtrStack(curQuest,codegenQuestStack);
				FklNastNode* node=curValue;
				for(;;)
				{
					if(fklPatternMatch(builtInSubPattern[FKL_CODEGEN_SUB_PATTERN_UNQUOTE],node,unquoteHt))
					{
						FklNastNode* unquoteValue=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,unquoteHt);
						unquoteHelperFunc(unquoteValue
								,codegenQuestStack
								,scope
								,macroScope
								,curEnv
								,_default_bc_process
								,curQuest
								,codegen);
						break;
					}
					FklNastNode* cur=node->pair->car;
					if(fklPatternMatch(builtInSubPattern[FKL_CODEGEN_SUB_PATTERN_UNQTESP],cur,unquoteHt))
					{
						FklNastNode* unqtespValue=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,unquoteHt);
						if(node->pair->cdr->type!=FKL_NAST_NIL)
						{
							FklCodegenQuest* appendQuest=createCodegenQuest(_qsquote_list_bc_process
									,createDefaultStackContext(fklCreatePtrStack(2,1))
									,NULL
									,scope
									,macroScope
									,curEnv
									,curValue->curline
									,curQuest
									,codegen);
							fklPushPtrStack(appendQuest,codegenQuestStack);
							fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_UNQTESP_CAR,unqtespValue,appendQuest),&valueStack);
							fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,node->pair->cdr,appendQuest),&valueStack);
						}
						else
							fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_UNQTESP_CAR,unqtespValue,curQuest),&valueStack);
						break;
					}
					else
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,cur,curQuest),&valueStack);
					node=node->pair->cdr;
					if(node->type!=FKL_NAST_PAIR)
					{
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,node,curQuest),&valueStack);
						break;
					}
				}
			}
			else if(curValue->type==FKL_NAST_VECTOR)
			{
				size_t vecSize=curValue->vec->size;
				FklCodegenQuest* curQuest=createCodegenQuest(_qsquote_vec_bc_process
						,createDefaultStackContext(fklCreatePtrStack(vecSize,16))
						,NULL
						,scope
						,macroScope
						,curEnv
						,curValue->curline
						,prevQuest
						,codegen);
				fklPushPtrStack(curQuest,codegenQuestStack);
				for(size_t i=0;i<vecSize;i++)
				{
					if(fklPatternMatch(builtInSubPattern[FKL_CODEGEN_SUB_PATTERN_UNQTESP],curValue->vec->base[i],unquoteHt))
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_UNQTESP_VEC
									,fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,unquoteHt)
									,curQuest)
								,&valueStack);
					else
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,curValue->vec->base[i],curQuest),&valueStack);
				}
			}
			else if(curValue->type==FKL_NAST_BOX)
			{
				FklCodegenQuest* curQuest=createCodegenQuest(_qsquote_box_bc_process
						,createDefaultStackContext(fklCreatePtrStack(1,1))
						,NULL
						,scope
						,macroScope
						,curEnv
						,curValue->curline
						,prevQuest
						,codegen);
				fklPushPtrStack(curQuest,codegenQuestStack);
				fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,curValue->box,curQuest),&valueStack);
			}
			else
				push_default_codegen_quest(curValue
						,codegenQuestStack
						,scope
						,macroScope
						,curEnv
						,prevQuest
						,codegen);
			fklDestroyHashTable(unquoteHt);
		}
	}
	fklUninitPtrStack(&valueStack);
}

BC_PROCESS(_cond_exp_bc_process_0)
{
	FklByteCodelnt* retval=NULL;
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top)
		retval=fklPopPtrStack(stack);
	else
		retval=fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),fid,line);
	return retval;
}

BC_PROCESS(_cond_exp_bc_process_1)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=NULL;
	if(stack->top>=2)
	{
		FklByteCodelnt* prev=stack->base[0];
		FklByteCodelnt* first=stack->base[1];
		retval=fklCreateByteCodelnt(fklCreateByteCode(0));

		FklInstruction drop=create_op_ins(FKL_OP_DROP);

		fklBytecodeLntPushBackIns(&drop,prev,fid,line);

		FklInstruction jmp=create_op_ins(FKL_OP_JMP);
		jmp.imm_i64=prev->bc->len;

		for(size_t i=2;i<stack->top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			fklBytecodeLntPushFrontIns(retval,&drop,fid,line);
			fklCodeLntConcat(retval,cur);
			fklDestroyByteCodelnt(cur);
		}

		check_and_close_ref(retval,scope,env,codegen->pts,fid,line);

		fklBytecodeLntPushFrontIns(retval,&jmp,fid,line);
		FklInstruction jmpIfFalse=create_op_imm_i64_ins(FKL_OP_JMP_IF_FALSE,retval->bc->len);

		fklBytecodeLntPushBackIns(&jmpIfFalse,retval,fid,line);
		fklCodeLntConcat(retval,prev);
		fklDestroyByteCodelnt(prev);
		fklCodeLntReverseConcat(first,retval);
		fklDestroyByteCodelnt(first);

	}
	else
		retval=fklPopPtrStack(stack);
	stack->top=0;

	return retval;
}

BC_PROCESS(_cond_exp_bc_process_2)
{
	FklPtrStack* stack=GET_STACK(context);
	FklInstruction drop=create_op_ins(FKL_OP_DROP);

	FklByteCodelnt* retval=NULL;
	if(stack->top)
	{
		retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		FklByteCodelnt* first=stack->base[0];
		for(size_t i=1;i<stack->top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			fklBytecodeLntPushFrontIns(retval,&drop,fid,line);
			fklCodeLntConcat(retval,cur);
			fklDestroyByteCodelnt(cur);
		}
		if(retval->bc->len)
		{
			FklInstruction jmpIfFalse=create_op_imm_i64_ins(FKL_OP_JMP_IF_FALSE,retval->bc->len);
			fklBytecodeLntPushBackIns(&jmpIfFalse,retval,fid,line);
		}
		fklCodeLntReverseConcat(first,retval);
		fklDestroyByteCodelnt(first);

		if(!retval->bc->len)
		{
			fklDestroyByteCodelnt(retval);
			retval=fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),fid,line);
		}
	}
	stack->top=0;
	return retval;
}

static inline int is_true_exp(const FklNastNode* exp)
{
	return exp->type!=FKL_NAST_NIL
		&&exp->type!=FKL_NAST_PAIR
		&&exp->type!=FKL_NAST_SYM;
}

static CODEGEN_FUNC(codegen_cond)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklCodegenQuest* quest=createCodegenQuest(_cond_exp_bc_process_0
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,NULL
			,scope
			,macroScope
			,curEnv
			,rest->curline
			,NULL
			,codegen);
	fklPushPtrStack(quest,codegenQuestStack);
	if(rest->type!=FKL_NAST_NIL)
	{
		FklPtrStack tmpStack=FKL_STACK_INIT;
		fklInitPtrStack(&tmpStack,32,16);
		pushListItemToStack(rest,&tmpStack,NULL);
		FklNastNode* lastExp=fklPopPtrStack(&tmpStack);
		FklCodegenQuest* prevQuest=quest;
		for(size_t i=0;i<tmpStack.top;i++)
		{
			FklNastNode* curExp=tmpStack.base[i];
			if(curExp->type!=FKL_NAST_PAIR)
			{
				errorState->type=FKL_ERR_SYNTAXERROR;
				errorState->place=fklMakeNastNodeRef(origExp);
				fklUninitPtrStack(&tmpStack);
				return;
			}
			FklNastNode* last=NULL;
			FklPtrQueue* curQueue=fklCreatePtrQueue();
			pushListItemToQueue(curExp,curQueue,&last);
			if(last->type!=FKL_NAST_NIL)
			{
				errorState->type=FKL_ERR_SYNTAXERROR;
				errorState->place=fklMakeNastNodeRef(origExp);
				while(!fklIsPtrQueueEmpty(curQueue))
					fklDestroyNastNode(fklPopPtrQueue(curQueue));
				fklDestroyPtrQueue(curQueue);
				fklUninitPtrStack(&tmpStack);
				return;
			}
			uint32_t curScope=enter_new_scope(scope,curEnv);
			FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);
			FklCodegenQuest* curQuest=createCodegenQuest(_cond_exp_bc_process_1
					,createDefaultStackContext(fklCreatePtrStack(32,16))
					,createFirstHasRetvalQueueNextExpression(curQueue)
					,curScope
					,cms
					,curEnv
					,curExp->curline
					,prevQuest
					,codegen);
			fklPushPtrStack(curQuest,codegenQuestStack);
			prevQuest=curQuest;
		}
		FklNastNode* last=NULL;
		if(lastExp->type!=FKL_NAST_PAIR)
		{
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(origExp);
			fklUninitPtrStack(&tmpStack);
			return;
		}
		FklPtrQueue* lastQueue=fklCreatePtrQueue();
		FklByteCodeProcesser cond_exp_bc_process=_cond_exp_bc_process_2;
		if(lastExp->pair->cdr->type==FKL_NAST_PAIR&&is_true_exp(lastExp->pair->car))
		{
			pushListItemToQueue(lastExp->pair->cdr,lastQueue,&last);
			cond_exp_bc_process=_local_exp_bc_process;
		}
		else
			pushListItemToQueue(lastExp,lastQueue,&last);
		if(last->type!=FKL_NAST_NIL)
		{
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(origExp);
			while(!fklIsPtrQueueEmpty(lastQueue))
				fklDestroyNastNode(fklPopPtrQueue(lastQueue));
			fklDestroyPtrQueue(lastQueue);
			fklUninitPtrStack(&tmpStack);
			return;
		}
		uint32_t curScope=enter_new_scope(scope,curEnv);
		FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);
		fklPushPtrStack(createCodegenQuest(cond_exp_bc_process
					,createDefaultStackContext(fklCreatePtrStack(32,16))
					,createFirstHasRetvalQueueNextExpression(lastQueue)
					,curScope
					,cms
					,curEnv
					,lastExp->curline
					,prevQuest
					,codegen)
				,codegenQuestStack);
		fklUninitPtrStack(&tmpStack);
	}
}

BC_PROCESS(_if_exp_bc_process_0)
{
	FklPtrStack* stack=GET_STACK(context);
	FklInstruction jmpIfFalse=create_op_ins(FKL_OP_JMP_IF_FALSE);
	FklInstruction drop=create_op_ins(FKL_OP_DROP);

	FklByteCodelnt* exp=fklPopPtrStack(stack);
	FklByteCodelnt* cond=fklPopPtrStack(stack);
	if(exp&&cond)
	{
		fklBytecodeLntPushBackIns(&drop,exp,fid,line);
		jmpIfFalse.imm_i64=exp->bc->len;
		fklBytecodeLntPushFrontIns(cond,&jmpIfFalse,fid,line);
		fklCodeLntConcat(cond,exp);
		fklDestroyByteCodelnt(exp);
		return cond;
	}
	else if(exp)
		return exp;
	else
		return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),fid,line);
}

static CODEGEN_FUNC(codegen_if0)
{
	FklNastNode* cond=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	FklNastNode* exp=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);

	FklPtrQueue* nextQueue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(cond),nextQueue);
	fklPushPtrQueue(fklMakeNastNodeRef(exp),nextQueue);

	uint32_t curScope=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);
	fklPushPtrStack(createCodegenQuest(_if_exp_bc_process_0
				,createDefaultStackContext(fklCreatePtrStack(2,2))
				,createMustHasRetvalQueueNextExpression(nextQueue)
				,curScope
				,cms
				,curEnv
				,origExp->curline
				,NULL
				,codegen)
			,codegenQuestStack);
}

BC_PROCESS(_if_exp_bc_process_1)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* exp0=fklPopPtrStack(stack);
	FklByteCodelnt* cond=fklPopPtrStack(stack);
	FklByteCodelnt* exp1=fklPopPtrStack(stack);

	FklInstruction jmp=create_op_ins(FKL_OP_JMP);
	FklInstruction jmpIfFalse=create_op_ins(FKL_OP_JMP_IF_FALSE);

	FklInstruction drop=create_op_ins(FKL_OP_DROP);

	if(exp0&&cond&&exp1)
	{

		fklBytecodeLntPushBackIns(&drop,exp0,fid,line);
		fklBytecodeLntPushBackIns(&drop,exp1,fid,line);
		jmp.imm_i64=exp1->bc->len;
		fklBytecodeLntPushFrontIns(exp0,&jmp,fid,line);
		jmpIfFalse.imm_i64=exp0->bc->len;
		fklBytecodeLntPushFrontIns(cond,&jmpIfFalse,fid,line);
		fklCodeLntConcat(cond,exp0);
		fklCodeLntConcat(cond,exp1);
		fklDestroyByteCodelnt(exp0);
		fklDestroyByteCodelnt(exp1);
		return cond;
	}
	else if(exp0&&cond)
	{
		fklBytecodeLntPushBackIns(&drop,exp0,fid,line);
		jmpIfFalse.imm_i64=exp0->bc->len;
		fklBytecodeLntPushFrontIns(cond,&jmpIfFalse,fid,line);
		fklCodeLntConcat(cond,exp0);
		fklDestroyByteCodelnt(exp0);
		return cond;
	}
	else if(exp0)
		return exp0;
	else
		return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),fid,line);
}

static CODEGEN_FUNC(codegen_if1)
{
	FklNastNode* cond=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	FklNastNode* exp0=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklNastNode* exp1=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);

	FklPtrQueue* exp0Queue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(cond),exp0Queue);
	fklPushPtrQueue(fklMakeNastNodeRef(exp0),exp0Queue);

	FklPtrQueue* exp1Queue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(exp1),exp1Queue);

	uint32_t curScope=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);
	FklCodegenQuest* prev=createCodegenQuest(_if_exp_bc_process_1
			,createDefaultStackContext(fklCreatePtrStack(2,2))
			,createMustHasRetvalQueueNextExpression(exp0Queue)
			,curScope
			,cms
			,curEnv
			,origExp->curline
			,NULL
			,codegen);
	fklPushPtrStack(prev,codegenQuestStack);

	curScope=enter_new_scope(scope,curEnv);
	cms=fklCreateCodegenMacroScope(macroScope);
	fklPushPtrStack(createCodegenQuest(_default_bc_process
				,createDefaultStackContext(fklCreatePtrStack(2,2))
				,createMustHasRetvalQueueNextExpression(exp1Queue)
				,curScope
				,cms
				,curEnv
				,origExp->curline
				,prev
				,codegen)
			,codegenQuestStack);
}

BC_PROCESS(_when_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top)
	{
		FklByteCodelnt* cond=stack->base[0];
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));

		FklInstruction drop=create_op_ins(FKL_OP_DROP);
		for(size_t i=1;i<stack->top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			fklBytecodeLntPushFrontIns(retval,&drop,fid,line);
			fklCodeLntConcat(retval,cur);
			fklDestroyByteCodelnt(cur);
		}
		stack->top=0;
		FklInstruction jmpIfFalse=create_op_imm_i64_ins(FKL_OP_JMP_IF_FALSE,retval->bc->len);
		if(retval->bc->len)
			fklBytecodeLntPushBackIns(&jmpIfFalse,retval,fid,line);
		fklCodeLntReverseConcat(cond,retval);
		fklDestroyByteCodelnt(cond);
		check_and_close_ref(retval,scope,env,codegen->pts,fid,line);
		return retval;
	}
	else
		return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),fid,line);
}

BC_PROCESS(_unless_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top)
	{
		FklByteCodelnt* cond=stack->base[0];
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));

		FklInstruction drop=create_op_ins(FKL_OP_DROP);
		for(size_t i=1;i<stack->top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			fklBytecodeLntPushFrontIns(retval,&drop,fid,line);
			fklCodeLntConcat(retval,cur);
			fklDestroyByteCodelnt(cur);
		}
		stack->top=0;
		FklInstruction jmpIfFalse=create_op_imm_i64_ins(FKL_OP_JMP_IF_TRUE,retval->bc->len);
		if(retval->bc->len)
			fklBytecodeLntPushBackIns(&jmpIfFalse,retval,fid,line);
		fklCodeLntReverseConcat(cond,retval);
		fklDestroyByteCodelnt(cond);
		check_and_close_ref(retval,scope,env,codegen->pts,fid,line);
		return retval;
	}
	else
		return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),fid,line);
}

static inline void codegen_when_unless(FklHashTable* ht
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklCodegenEnv* curEnv
		,FklCodegenInfo* codegen
		,FklPtrStack* codegenQuestStack
		,FklByteCodeProcesser func
		,FklCodegenErrorState* errorState
		,FklNastNode* origExp)
{
	FklNastNode* cond=fklPatternMatchingHashTableRef(codegen->outer_ctx->builtInPatternVar_value,ht);

	FklNastNode* rest=fklPatternMatchingHashTableRef(codegen->outer_ctx->builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(cond),queue);
	pushListItemToQueue(rest,queue,NULL);

	uint32_t curScope=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(func
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createFirstHasRetvalQueueNextExpression(queue)
			,curScope
			,cms
			,curEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_when)
{
	codegen_when_unless(ht,scope,macroScope,curEnv,codegen,codegenQuestStack,_when_exp_bc_process,errorState,origExp);
}

static CODEGEN_FUNC(codegen_unless)
{
	codegen_when_unless(ht,scope,macroScope,curEnv,codegen,codegenQuestStack,_unless_exp_bc_process,errorState,origExp);
}

static FklCodegenInfo* createCodegenInfo(FklCodegenInfo* prev
		,const char* filename
		,FklCodegenEnv* env
		,int libMark
		,int macroMark
		,FklCodegenOuterCtx* outer_ctx)
{
	FklCodegenInfo* r=(FklCodegenInfo*)malloc(sizeof(FklCodegenInfo));
	FKL_ASSERT(r);
	fklInitCodegenInfo(r
			,filename
			,env
			,prev
			,prev->globalSymTable
			,1
			,libMark
			,macroMark
			,outer_ctx);
	return r;
}

typedef struct
{
	FILE* fp;
	FklCodegenInfo* codegen;
	FklPtrStack symbolStack;
	FklPtrStack stateStack;
	FklUintStack lineStack;
}CodegenLoadContext;

#include<fakeLisp/grammer.h>

static CodegenLoadContext* createCodegenLoadContext(FILE* fp,FklCodegenInfo* codegen)
{
	CodegenLoadContext* r=(CodegenLoadContext*)malloc(sizeof(CodegenLoadContext));
	FKL_ASSERT(r);
	r->codegen=codegen;
	r->fp=fp;
	fklInitPtrStack(&r->stateStack,16,16);
	fklInitUintStack(&r->lineStack,16,16);
	fklInitPtrStack(&r->symbolStack,16,16);
	return r;
}

static void _codegen_load_finalizer(void* pcontext)
{
	CodegenLoadContext* context=pcontext;
	FklPtrStack* symbolStack=&context->symbolStack;
	while(!fklIsPtrStackEmpty(symbolStack))
		fklDestroyAnaysisSymbol(fklPopPtrStack(symbolStack));
	fklUninitPtrStack(symbolStack);
	fklUninitPtrStack(&context->stateStack);
	fklUninitUintStack(&context->lineStack);
	fclose(context->fp);
	free(context);
}

static inline FklNastNode* getExpressionFromFile(FklCodegenInfo* codegen
		,FILE* fp
		,int* unexpectEOF
		,size_t* errorLine
		,FklCodegenErrorState* errorState
		,FklPtrStack* symbolStack
		,FklUintStack* lineStack
		,FklPtrStack* stateStack)
{
	FklSymbolTable* pst=&codegen->outer_ctx->public_symbol_table;
	size_t size=0;
	FklNastNode* begin=NULL;
	char* list=NULL;
	stateStack->top=0;
	FklGrammer* g=*codegen->g;
	if(g)
	{
		codegen->outer_ctx->cur_file_dir=codegen->dir;
		fklPushPtrStack(&g->aTable.states[0],stateStack);
		list=fklReadWithAnalysisTable(g
				,fp
				,&size
				,codegen->curline
				,&codegen->curline
				,pst
				,unexpectEOF
				,errorLine
				,&begin
				,symbolStack
				,lineStack
				,stateStack);
	}
	else
	{
		fklNastPushState0ToStack(stateStack);
		list=fklReadWithBuiltinParser(fp
				,&size
				,codegen->curline
				,&codegen->curline
				,pst
				,unexpectEOF
				,errorLine
				,&begin
				,symbolStack
				,lineStack
				,stateStack);
	}
	if(list)
		free(list);
	if(*unexpectEOF)
		begin=NULL;
	while(!fklIsPtrStackEmpty(symbolStack))
		fklDestroyAnaysisSymbol(fklPopPtrStack(symbolStack));
	lineStack->top=0;
	return begin;
}

#include<fakeLisp/grammer.h>

static FklNastNode* _codegen_load_get_next_expression(void* pcontext,FklCodegenErrorState* errorState)
{
	CodegenLoadContext* context=pcontext;
	FklCodegenInfo* codegen=context->codegen;
	FklPtrStack* stateStack=&context->stateStack;
	FklPtrStack* symbolStack=&context->symbolStack;
	FklUintStack* lineStack=&context->lineStack;
	FILE* fp=context->fp;
	int unexpectEOF=0;
	size_t errorLine=0;
	FklNastNode* begin=getExpressionFromFile(codegen
			,fp
			,&unexpectEOF
			,&errorLine
			,errorState
			,symbolStack
			,lineStack
			,stateStack);
	if(unexpectEOF)
	{
		errorState->place=NULL;
		errorState->line=errorLine;
		errorState->type=unexpectEOF==FKL_PARSE_TERMINAL_MATCH_FAILED?FKL_ERR_UNEXPECTED_EOF:FKL_ERR_INVALIDEXPR;
		return NULL;
	}
	return begin;
}

static int hasLoadSameFile(const char* rpath,const FklCodegenInfo* codegen)
{
	for(;codegen;codegen=codegen->prev)
		if(codegen->realpath&&!strcmp(rpath,codegen->realpath))
			return 1;
	return 0;
}

static const FklNextExpressionMethodTable _codegen_load_get_next_expression_method_table=
{
	.getNextExpression=_codegen_load_get_next_expression,
	.finalizer=_codegen_load_finalizer,
};

static FklCodegenNextExpression* createFpNextExpression(FILE* fp,FklCodegenInfo* codegen)
{
	CodegenLoadContext* context=createCodegenLoadContext(fp,codegen);
	return createCodegenNextExpression(&_codegen_load_get_next_expression_method_table
			,context
			,0);
}

static inline void init_load_codegen_grammer_ptr(FklCodegenInfo* next,FklCodegenInfo* prev)
{
	next->g=&prev->self_g;
	next->named_prod_groups=&prev->self_named_prod_groups;
	next->unnamed_prods=&prev->self_unnamed_prods;
	next->unnamed_ignores=&prev->self_unnamed_ignores;
	next->builtin_prods=&prev->self_builtin_prods;
	next->builtin_ignores=&prev->self_builtin_ignores;
}

static CODEGEN_FUNC(codegen_load)
{
	FklNastNode* filename=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	if(filename->type!=FKL_NAST_STR)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}

	if(rest->type!=FKL_NAST_NIL)
	{
		FklPtrQueue* queue=fklCreatePtrQueue();

		FklNastPair* prevPair=origExp->pair->cdr->pair;

		FklNastNode* loadHead=origExp->pair->car;

		for(;rest->type==FKL_NAST_PAIR;rest=rest->pair->cdr)
		{
			FklNastNode* restExp=fklNastCons(fklMakeNastNodeRef(loadHead),rest,rest->curline);

			prevPair->cdr=fklCreateNastNode(FKL_NAST_NIL,rest->curline);

			fklPushPtrQueue(restExp,queue);

			prevPair=rest->pair;
		}
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_begin_exp_bc_process
				,createDefaultStackContext(fklCreatePtrStack(2,1))
				,createDefaultQueueNextExpression(queue)
				,scope
				,macroScope
				,curEnv
				,origExp->curline
				,codegen
				,codegenQuestStack);
	}

	const FklString* filenameStr=filename->str;
	if(!fklIsAccessibleRegFile(filenameStr->str))
	{
		errorState->type=FKL_ERR_FILEFAILURE;
		errorState->place=fklMakeNastNodeRef(filename);
		return;
	}
	FklCodegenInfo* nextCodegen=createCodegenInfo(codegen,filenameStr->str,curEnv,0,0,codegen->outer_ctx);
	if(hasLoadSameFile(nextCodegen->realpath,codegen))
	{
		errorState->type=FKL_ERR_CIRCULARLOAD;
		errorState->place=fklMakeNastNodeRef(filename);
		nextCodegen->refcount=1;
		fklDestroyCodegenInfo(nextCodegen);
		return;
	}
	init_load_codegen_grammer_ptr(nextCodegen,codegen);
	FILE* fp=fopen(filenameStr->str,"r");
	if(!fp)
	{
		errorState->type=FKL_ERR_FILEFAILURE;
		errorState->place=fklMakeNastNodeRef(filename);
		nextCodegen->refcount=1;
		fklDestroyCodegenInfo(nextCodegen);
		return;
	}
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_begin_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createFpNextExpression(fp,nextCodegen)
			,scope
			,macroScope
			,curEnv
			,origExp->curline
			,nextCodegen
			,codegenQuestStack);
}

static inline char* combineFileNameFromList(const FklNastNode* list,const FklSymbolTable* pst)
{
	char* r=NULL;
	for(const FklNastNode* curPair=list;curPair->type==FKL_NAST_PAIR;curPair=curPair->pair->cdr)
	{
		FklNastNode* cur=curPair->pair->car;
		r=fklCstrStringCat(r,fklGetSymbolWithId(cur->sym,pst)->symbol);
		if(curPair->pair->cdr->type!=FKL_NAST_NIL)
			r=fklStrCat(r,FKL_PATH_SEPARATOR_STR);
	}
	return r;
}

static void add_symbol_to_local_env_in_array(FklCodegenEnv* env
		,FklSymbolTable* symbolTable
		,FklHashTable* exports
		,FklByteCodelnt* bcl
		,FklSid_t fid
		,uint64_t line
		,uint32_t scope
		,FklSymbolTable* pst)
{
	FklInstruction bc=create_op_ins(FKL_OP_IMPORT);
	for(const FklHashTableItem* l=exports->first;l;l=l->next)
	{
		const FklCodegenExportSidIndexHashItem* item=(const FklCodegenExportSidIndexHashItem*)l->data;
		bc.imm=fklAddCodegenDefBySid(item->sid,scope,env);
		bc.imm_u32=item->idx;
		fklBytecodeLntPushFrontIns(bcl,&bc,fid,line);
	}
}

static void add_symbol_with_prefix_to_local_env_in_array(FklCodegenEnv* env
		,FklSymbolTable* symbolTable
		,FklString* prefix
		,FklHashTable* exports
		,FklByteCodelnt* bcl
		,FklSid_t fid
		,uint64_t line
		,uint32_t scope
		,FklSymbolTable* pst)
{
	FklInstruction bc=create_op_ins(FKL_OP_IMPORT);
	FklStringBuffer buf;
	fklInitStringBuffer(&buf);
	for(const FklHashTableItem* l=exports->first;l;l=l->next)
	{
		const FklCodegenExportSidIndexHashItem* item=(const FklCodegenExportSidIndexHashItem*)l->data;
		FklString* origSymbol=fklGetSymbolWithId(item->sid,pst)->symbol;
		fklStringBufferConcatWithString(&buf,prefix);
		fklStringBufferConcatWithString(&buf,origSymbol);

		FklSid_t sym=fklAddSymbolCharBuf(buf.buf,buf.index,pst)->id;
		uint32_t idx=fklAddCodegenDefBySid(sym,scope,env);
		bc.imm=idx;
		bc.imm_u32=item->idx;
		fklBytecodeLntPushFrontIns(bcl,&bc,fid,line);
		fklStringBufferClear(&buf);
	}
	fklUninitStringBuffer(&buf);
}

static FklNastNode* createPatternWithPrefixFromOrig(FklNastNode* orig
		,FklString* prefix
		,FklSymbolTable* pst)
{
	const FklString* head=fklGetSymbolWithId(orig->pair->car->sym,pst)->symbol;
	FklString* symbolWithPrefix=fklStringAppend(prefix,head);
	FklNastNode* patternWithPrefix=fklNastConsWithSym(fklAddSymbol(symbolWithPrefix,pst)->id
			,fklMakeNastNodeRef(orig->pair->cdr)
			,orig->curline
			,orig->pair->car->curline);
	free(symbolWithPrefix);
	return patternWithPrefix;
}

static FklNastNode* add_prefix_for_pattern_origin_exp(FklNastNode* orig
		,FklString* prefix
		,FklSymbolTable* pst)
{
	FklNastNode* retval=fklCopyNastNode(orig);
	FklNastNode* head_node=caadr_nast_node(retval);
	const FklString* head=fklGetSymbolWithId(head_node->sym,pst)->symbol;
	FklString* symbolWithPrefix=fklStringAppend(prefix,head);
	FklSid_t id=fklAddSymbol(symbolWithPrefix,pst)->id;
	head_node->sym=id;
	free(symbolWithPrefix);
	return retval;
}

static FklNastNode* replace_pattern_origin_exp_head(FklNastNode* orig,FklSid_t head)
{
	FklNastNode* retval=fklCopyNastNode(orig);
	FklNastNode* head_node=caadr_nast_node(retval);
	head_node->sym=head;
	return retval;
}

static inline void export_replacement_with_prefix(FklHashTable* replacements
		,FklCodegenMacroScope* macros
		,FklString* prefix
		,FklSymbolTable* pst)
{
	for(FklHashTableItem* cur=replacements->first;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->data;
		FklString* origSymbol=fklGetSymbolWithId(rep->id,pst)->symbol;
		FklString* symbolWithPrefix=fklStringAppend(prefix,origSymbol);
		FklSid_t id=fklAddSymbol(symbolWithPrefix,pst)->id;
		fklAddReplacementBySid(id,rep->node,macros->replacements);
		free(symbolWithPrefix);
	}
}

void fklPrintUndefinedRef(const FklCodegenEnv* env
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* pst)
{
	const FklPtrStack* urefs=&env->uref;
	for(uint32_t i=urefs->top;i>0;i--)
	{
		FklUnReSymbolRef* ref=urefs->base[i-1];
		fprintf(stderr,"warning in compiling: Symbol ");
		fklPrintRawSymbol(fklGetSymbolWithId(ref->id,pst)->symbol,stderr);
		fprintf(stderr," is undefined at line %"FKL_PRT64U,ref->line);
		if(ref->fid)
		{
			fputs(" of ",stderr);
			fklPrintString(fklGetSymbolWithId(ref->fid,globalSymTable)->symbol,stderr);
		}
		fputc('\n',stderr);
	}
}

typedef struct
{
	FklCodegenInfo* codegen;
	FklCodegenEnv* env;
	FklCodegenMacroScope* cms;
	FklPtrStack* stack;
	FklNastNode* prefix;
	FklNastNode* only;
	FklNastNode* alias;
	FklNastNode* except;
	uint32_t scope;
}ExportContextData;

static void export_context_data_finalizer(void* data)
{
	ExportContextData* d=(ExportContextData*)data;
	while(!fklIsPtrStackEmpty(d->stack))
		fklDestroyByteCodelnt(fklPopPtrStack(d->stack));
	fklDestroyPtrStack(d->stack);
	fklDestroyCodegenEnv(d->env);
	fklDestroyCodegenInfo(d->codegen);
	fklDestroyCodegenMacroScope(d->cms);

	if(d->prefix)
		fklDestroyNastNode(d->prefix);
	if(d->only)
		fklDestroyNastNode(d->only);
	if(d->alias)
		fklDestroyNastNode(d->alias);
	if(d->except)
		fklDestroyNastNode(d->except);
	free(d);
}

static FklPtrStack* export_context_data_get_bcl_stack(void* data)
{
	return ((ExportContextData*)data)->stack;
}

static void export_context_data_put_bcl(void* data,FklByteCodelnt* bcl)
{
	ExportContextData* d=(ExportContextData*)data;
	fklPushPtrStack(bcl,d->stack);
}

static const FklCodegenQuestContextMethodTable ExportContextMethodTable=
{
	.__finalizer=export_context_data_finalizer,
	.__get_bcl_stack=export_context_data_get_bcl_stack,
	.__put_bcl=export_context_data_put_bcl,
};

static inline void recompute_ignores_terminal_sid_and_regex(FklGrammerIgnore* igs
		,FklSymbolTable* target_table
		,FklRegexTable* target_rt
		,FklRegexTable* orig_rt)
{
	FklGrammerIgnoreSym* syms=igs->ig;
	size_t len=igs->len;
	for(size_t i=len;i>0;i--)
	{
		size_t idx=i-1;
		FklGrammerIgnoreSym* cur=&syms[idx];
		if(cur->term_type==FKL_TERM_BUILTIN)
		{
			int failed=0;
			cur->b.c=cur->b.t->ctx_create(idx+1<len?syms[idx+1].str:NULL,&failed);
		}
		else if(cur->term_type==FKL_TERM_REGEX)
			cur->re=fklAddRegexStr(target_rt,fklGetStringWithRegex(orig_rt,cur->re,NULL));
		else
			cur->str=fklAddSymbol(cur->str,target_table)->symbol;
	}
}

static inline void recompute_prod_terminal_sid(FklGrammerProduction* prod
		,FklSymbolTable* target_table
		,FklSymbolTable* origin_table)
{
	size_t len=prod->len;
	FklGrammerSym* sym=prod->syms;
	for(size_t i=0;i<len;i++)
	{
		FklGrammerSym* cur=&sym[i];
		if(cur->isterm&&cur->term_type!=FKL_TERM_BUILTIN)
			cur->nt.sid=fklAddSymbol(fklGetSymbolWithId(cur->nt.sid,origin_table)->symbol,target_table)->id;
	}
}

static inline void replace_group_id(FklGrammerProduction* prod,FklSid_t new_id)
{
	size_t len=prod->len;
	if(prod->left.group)
		prod->left.group=new_id;
	FklGrammerSym* sym=prod->syms;
	for(size_t i=0;i<len;i++)
	{
		FklGrammerSym* cur=&sym[i];
		if(!cur->isterm&&cur->nt.group)
			cur->nt.group=new_id;
	}
}

static inline int init_builtin_grammer(FklCodegenInfo* codegen,FklSymbolTable* st);
static inline int add_group_prods(FklGrammer* g,const FklHashTable* prods);
static inline int add_group_ignores(FklGrammer* g,FklGrammerIgnore* igs);
static inline FklGrammerProductionGroup* add_production_group(FklHashTable* named_prod_groups,FklSid_t group_id);
static inline int add_all_group_to_grammer(FklCodegenInfo* codegen);

static inline int import_reader_macro(int is_grammer_inited
		,int* p_is_grammer_inited
		,FklCodegenInfo* codegen
		,FklCodegenErrorState* errorState
		,uint64_t curline
		,FklCodegenLib* lib
		,FklGrammerProductionGroup* group
		,FklSid_t origin_group_id
		,FklSid_t new_group_id)
{
	FklGrammer* g=*codegen->g;
	FklSymbolTable* pst=&codegen->outer_ctx->public_symbol_table;
	if(!is_grammer_inited)
	{
		if(!g)
		{
			if(init_builtin_grammer(codegen,pst))
			{
reader_macro_error:
				errorState->line=curline;
				errorState->type=FKL_ERR_IMPORT_READER_MACRO_ERROR;
				errorState->place=NULL;
				return 1;
			}
			g=*codegen->g;
		}
		else
		{
			fklClearGrammer(g);
			if(add_group_ignores(g,*codegen->builtin_ignores))
				goto reader_macro_error;

			if(add_group_prods(g,codegen->builtin_prods))
				goto reader_macro_error;
		}
		*p_is_grammer_inited=1;
	}

	FklSymbolTable* origin_table=&lib->terminal_table;
	FklRegexTable* orig_rt=&lib->regexes;
	FklGrammerProductionGroup* target_group=add_production_group(codegen->named_prod_groups,new_group_id);

	for(FklGrammerIgnore* igs=group->ignore;igs;igs=igs->next)
	{
		FklGrammerIgnore* ig=(FklGrammerIgnore*)malloc(sizeof(FklGrammerIgnore)+igs->len*sizeof(FklGrammerIgnoreSym));
		FKL_ASSERT(ig);
		ig->len=igs->len;
		ig->next=NULL;
		memcpy(ig->ig,igs->ig,sizeof(FklGrammerIgnoreSym)*ig->len);

		recompute_ignores_terminal_sid_and_regex(ig,&g->terminals,&g->regexes,orig_rt);
		if(fklAddIgnoreToIgnoreList(&target_group->ignore,ig))
		{
			free(ig);
			goto reader_macro_error;
		}
	}

	for(FklHashTableItem* prod_hash_item=group->prods.first
			;prod_hash_item
			;prod_hash_item=prod_hash_item->next)
	{
		FklGrammerProdHashItem* item=(FklGrammerProdHashItem*)prod_hash_item->data;
		for(FklGrammerProduction* prods=item->prods;prods;prods=prods->next)
		{
			FklGrammerProduction* prod=fklCopyUninitedGrammerProduction(prods);
			recompute_prod_terminal_sid(prod,&g->terminals,origin_table);
			if(origin_group_id!=new_group_id)
				replace_group_id(prod,new_group_id);
			if(fklAddProdToProdTable(&target_group->prods,&g->builtins,prod))
			{
				fklDestroyGrammerProduction(prod);
				goto reader_macro_error;
			}
		}
	}
	return 0;
}

static inline FklByteCodelnt* process_import_imported_lib_common(uint32_t libId
		,FklCodegenInfo* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,uint64_t curline
		,FklCodegenErrorState* errorState
		,FklSymbolTable* pst)
{
	for(FklCodegenMacro* cur=lib->head;cur;cur=cur->next)
		add_compiler_macro(&macroScope->head
				,fklMakeNastNodeRef(cur->pattern)
				,fklMakeNastNodeRef(cur->origin_exp)
				,cur->bcl
				,cur->prototype_id
				,0);

	for(FklHashTableItem* cur=lib->replacements->first;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->data;
		fklAddReplacementBySid(rep->id,rep->node,macroScope->replacements);
	}

	if(lib->named_prod_groups.t)
	{
		int is_grammer_inited=0;
		for(FklHashTableItem* prod_group_item=lib->named_prod_groups.first;prod_group_item;prod_group_item=prod_group_item->next)
		{
			FklGrammerProductionGroup* group=(FklGrammerProductionGroup*)prod_group_item->data;
			if(import_reader_macro(is_grammer_inited
						,&is_grammer_inited
						,codegen
						,errorState
						,curline
						,lib
						,group
						,group->id
						,group->id))
				break;
		}
		if(!errorState->type&&add_all_group_to_grammer(codegen))
		{
			errorState->line=curline;
			errorState->type=FKL_ERR_IMPORT_READER_MACRO_ERROR;
			errorState->place=NULL;
			return NULL;
		}
	}

	FklByteCodelnt* importBc=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);
	add_symbol_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,&lib->exports
			,importBc
			,codegen->fid
			,curline
			,scope
			,pst);

	return importBc;
}

static inline FklByteCodelnt* process_import_imported_lib_prefix(uint32_t libId
		,FklCodegenInfo* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,uint64_t curline
		,FklNastNode* prefixNode
		,FklCodegenErrorState* errorState
		,FklSymbolTable* pst)
{
	FklString* prefix=fklGetSymbolWithId(prefixNode->sym,pst)->symbol;
	for(FklCodegenMacro* cur=lib->head;cur;cur=cur->next)
		add_compiler_macro(&macroScope->head
				,createPatternWithPrefixFromOrig(cur->pattern
					,prefix
					,pst)
				,add_prefix_for_pattern_origin_exp(cur->origin_exp
					,prefix
					,pst)
				,cur->bcl
				,cur->prototype_id
				,0);

	export_replacement_with_prefix(lib->replacements
			,macroScope
			,prefix
			,pst);
	if(lib->named_prod_groups.t)
	{
		int is_grammer_inited=0;
		FklStringBuffer buffer;
		fklInitStringBuffer(&buffer);
		for(FklHashTableItem* prod_group_item=lib->named_prod_groups.first;prod_group_item;prod_group_item=prod_group_item->next)
		{
			FklGrammerProductionGroup* group=(FklGrammerProductionGroup*)prod_group_item->data;
			fklStringBufferConcatWithString(&buffer,prefix);
			fklStringBufferConcatWithString(&buffer,fklGetSymbolWithId(group->id,pst)->symbol);

			FklSid_t group_id_with_prefix=fklAddSymbolCharBuf(buffer.buf,buffer.index,pst)->id;

			if(import_reader_macro(is_grammer_inited
						,&is_grammer_inited
						,codegen
						,errorState
						,curline
						,lib
						,group
						,group->id
						,group_id_with_prefix))
				break;

			fklStringBufferClear(&buffer);
		}
		fklUninitStringBuffer(&buffer);
		if(!errorState->type&&add_all_group_to_grammer(codegen))
		{
			errorState->line=curline;
			errorState->type=FKL_ERR_IMPORT_READER_MACRO_ERROR;
			errorState->place=NULL;
			return NULL;
		}
	}
	FklByteCodelnt* importBc=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);
	add_symbol_with_prefix_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,prefix
			,&lib->exports
			,importBc
			,codegen->fid
			,curline
			,scope
			,pst);

	return importBc;
}

static inline FklByteCodelnt* process_import_imported_lib_only(uint32_t libId
		,FklCodegenInfo* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,uint64_t curline
		,FklNastNode* only
		,FklCodegenErrorState* errorState)
{
	FklByteCodelnt* importBc=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);

	FklInstruction bc=create_op_ins(FKL_OP_IMPORT);

	FklHashTable* exports=&lib->exports;
	FklHashTable* replace=lib->replacements;
	FklCodegenMacro* head=lib->head;

	int is_grammer_inited=0;
	for(;only->type==FKL_NAST_PAIR;only=only->pair->cdr)
	{
		FklSid_t cur=only->pair->car->sym;
		int r=0;
		for(FklCodegenMacro* macro=head;macro;macro=macro->next)
		{
			FklNastNode* patternHead=macro->pattern->pair->car;
			if(patternHead->sym==cur)
			{
				r=1;
				add_compiler_macro(&macroScope->head
						,fklMakeNastNodeRef(macro->pattern)
						,fklMakeNastNodeRef(macro->origin_exp)
						,macro->bcl
						,macro->prototype_id
						,0);
			}
		}
		FklNastNode* rep=fklGetReplacement(cur,replace);
		if(rep)
		{
			r=1;
			rep->refcount--;
			fklAddReplacementBySid(cur,rep,macroScope->replacements);
		}

		if(lib->named_prod_groups.t&&fklGetHashItem(&cur,&lib->named_prod_groups))
		{
			r=1;
			FklGrammerProductionGroup* group=fklGetHashItem(&cur,&lib->named_prod_groups);
			if(import_reader_macro(is_grammer_inited
						,&is_grammer_inited
						,codegen
						,errorState
						,curline
						,lib
						,group
						,cur
						,cur))
				break;
		}

		FklCodegenExportSidIndexHashItem* item=fklGetHashItem(&cur,exports);
		if(item)
		{
			uint32_t idx=fklAddCodegenDefBySid(cur,scope,curEnv);
			bc.imm=idx;
			bc.imm_u32=item->idx;
			fklBytecodeLntPushFrontIns(importBc,&bc,codegen->fid,only->curline);
		}
		else if(!r)
		{
			errorState->type=FKL_ERR_IMPORT_MISSING;
			errorState->place=fklMakeNastNodeRef(only->pair->car);
			errorState->line=only->curline;
			break;
		}
	}

	if(!errorState->type&&is_grammer_inited&&add_all_group_to_grammer(codegen))
	{
		errorState->line=curline;
		errorState->type=FKL_ERR_IMPORT_READER_MACRO_ERROR;
		errorState->place=NULL;
	}
	return importBc;
}

static inline int is_in_except_list(const FklNastNode* list,FklSid_t id)
{
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr)
		if(list->pair->car->sym==id)
			return 1;
	return 0;
}

static inline FklByteCodelnt* process_import_imported_lib_except(uint32_t libId
		,FklCodegenInfo* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,uint64_t curline
		,FklNastNode* except
		,FklCodegenErrorState* errorState)
{
	FklHashTable* exports=&lib->exports;

	FklHashTable* replace=lib->replacements;
	FklCodegenMacro* head=lib->head;

	for(FklCodegenMacro* macro=head;macro;macro=macro->next)
	{
		FklNastNode* patternHead=macro->pattern->pair->car;
		if(!is_in_except_list(except,patternHead->sym))
		{
			add_compiler_macro(&macroScope->head
					,fklMakeNastNodeRef(macro->pattern)
					,fklMakeNastNodeRef(macro->origin_exp)
					,macro->bcl
					,macro->prototype_id
					,0);
		}
	}

	for(FklHashTableItem* reps=replace->first;reps;reps=reps->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)reps->data;
		if(!is_in_except_list(except,rep->id))
			fklAddReplacementBySid(rep->id,rep->node,macroScope->replacements);
	}
	if(lib->named_prod_groups.t)
	{
		int is_grammer_inited=0;
		for(FklHashTableItem* prod_group_item=lib->named_prod_groups.first;prod_group_item;prod_group_item=prod_group_item->next)
		{
			FklGrammerProductionGroup* group=(FklGrammerProductionGroup*)prod_group_item->data;
			if(!is_in_except_list(except,group->id))
			{
				if(import_reader_macro(is_grammer_inited
							,&is_grammer_inited
							,codegen
							,errorState
							,curline
							,lib
							,group
							,group->id
							,group->id))
					break;

			}
		}
		if(!errorState->type&&add_all_group_to_grammer(codegen))
		{
			errorState->line=curline;
			errorState->type=FKL_ERR_IMPORT_READER_MACRO_ERROR;
			errorState->place=NULL;
			return NULL;
		}
	}
	
	FklByteCodelnt* importBc=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);

	FklInstruction bc=create_op_ins(FKL_OP_IMPORT);

	FklHashTable excepts;
	fklInitSidSet(&excepts);

	for(FklNastNode* list=except;list->type==FKL_NAST_PAIR;list=list->pair->cdr)
		fklPutHashItem(&list->pair->car->sym,&excepts);

	for(const FklHashTableItem* l=exports->first;l;l=l->next)
	{
		const FklCodegenExportSidIndexHashItem* item=(const FklCodegenExportSidIndexHashItem*)l->data;
		if(!fklGetHashItem(&item->sid,&excepts))
		{
			uint32_t idx=fklAddCodegenDefBySid(item->sid,scope,curEnv);
			bc.imm=idx;
			bc.imm_u32=item->idx;
			fklBytecodeLntPushFrontIns(importBc,&bc,codegen->fid,except->curline);
		}
	}
	fklUninitHashTable(&excepts);
	return importBc;
}

static inline FklByteCodelnt* process_import_imported_lib_alias(uint32_t libId
		,FklCodegenInfo* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,uint64_t curline
		,FklNastNode* alias
		,FklCodegenErrorState* errorState
		,FklSymbolTable* pst)
{
	FklByteCodelnt* importBc=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);

	FklInstruction bc=create_op_ins(FKL_OP_IMPORT);

	FklHashTable* exports=&lib->exports;

	FklHashTable* replace=lib->replacements;
	FklCodegenMacro* head=lib->head;

	int is_grammer_inited=0;
	for(;alias->type==FKL_NAST_PAIR;alias=alias->pair->cdr)
	{
		FklNastNode* curNode=alias->pair->car;
		FklSid_t cur=curNode->pair->car->sym;
		FklSid_t cur0=curNode->pair->cdr->pair->car->sym;
		int r=0;
		for(FklCodegenMacro* macro=head;macro;macro=macro->next)
		{
			FklNastNode* patternHead=macro->pattern->pair->car;
			if(patternHead->sym==cur)
			{
				r=1;

				FklNastNode* orig=macro->pattern;
				FklNastNode* pattern=fklNastConsWithSym(cur0
						,fklMakeNastNodeRef(orig->pair->cdr)
						,orig->curline
						,orig->pair->car->curline);

				add_compiler_macro(&macroScope->head
						,pattern
						,replace_pattern_origin_exp_head(macro->origin_exp,cur0)
						,macro->bcl
						,macro->prototype_id
						,0);
			}
		}
		FklNastNode* rep=fklGetReplacement(cur,replace);
		if(rep)
		{
			r=1;
			rep->refcount--;
			fklAddReplacementBySid(cur0,rep,macroScope->replacements);
		}

		if(lib->named_prod_groups.t&&fklGetHashItem(&cur,&lib->named_prod_groups))
		{
			r=1;
			FklGrammerProductionGroup* group=fklGetHashItem(&cur,&lib->named_prod_groups);
			if(import_reader_macro(is_grammer_inited
						,&is_grammer_inited
						,codegen
						,errorState
						,curline
						,lib
						,group
						,cur
						,cur0))
				break;
		}

		FklCodegenExportSidIndexHashItem* item=fklGetHashItem(&cur,exports);
		if(item)
		{
			uint32_t idx=fklAddCodegenDefBySid(cur0,scope,curEnv);
			bc.imm=idx;
			bc.imm_u32=item->idx;
			fklBytecodeLntPushFrontIns(importBc,&bc,codegen->fid,alias->curline);

		}
		else if(!r)
		{
			errorState->type=FKL_ERR_IMPORT_MISSING;
			errorState->place=fklMakeNastNodeRef(curNode->pair->car);
			errorState->line=alias->curline;
			break;
		}
	}

	if(!errorState->type&&is_grammer_inited&&add_all_group_to_grammer(codegen))
	{
		errorState->line=curline;
		errorState->type=FKL_ERR_IMPORT_READER_MACRO_ERROR;
		errorState->place=NULL;
	}
	return importBc;
}

static inline FklByteCodelnt* process_import_imported_lib(uint32_t libId
		,FklCodegenInfo* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* env
		,uint32_t scope
		,FklCodegenMacroScope* cms
		,uint64_t line
		,FklNastNode* prefix
		,FklNastNode* only
		,FklNastNode* alias
		,FklNastNode* except
		,FklCodegenErrorState* errorState
		,FklSymbolTable* pst)
{
	if(prefix)
		return process_import_imported_lib_prefix(libId
				,codegen
				,lib
				,env
				,scope
				,cms
				,line
				,prefix
				,errorState
				,pst);
	if(only)
		return process_import_imported_lib_only(libId
				,codegen
				,lib
				,env
				,scope
				,cms
				,line
				,only
				,errorState);
	if(alias)
		return process_import_imported_lib_alias(libId
				,codegen
				,lib
				,env
				,scope
				,cms
				,line
				,alias
				,errorState
				,pst);
	if(except)
		return process_import_imported_lib_except(libId
				,codegen
				,lib
				,env
				,scope
				,cms
				,line
				,except
				,errorState);
	return process_import_imported_lib_common(libId
			,codegen
			,lib
			,env
			,scope
			,cms
			,line
			,errorState
			,pst);
}

static inline int is_exporting_outer_ref_group(FklCodegenInfo* codegen)
{
	for(FklHashTableItem* sid_list=codegen->export_named_prod_groups->first
			;sid_list
			;sid_list=sid_list->next)
	{
		FklSid_t id=*(FklSid_t*)sid_list->data;
		FklGrammerProductionGroup* group=fklGetHashItem(&id,codegen->named_prod_groups);
		if(group->is_ref_outer)
			return 1;
	}
	return 0;
}

BC_PROCESS(_library_bc_process)
{
	if(is_exporting_outer_ref_group(codegen))
	{
		errorState->type=FKL_ERR_EXPORT_OUTER_REF_PROD_GROUP;
		errorState->line=line;
		errorState->place=NULL;
		return NULL;
	}
	
	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	fklUpdatePrototype(codegen->pts,env,codegen->globalSymTable,pst);
	fklPrintUndefinedRef(env,codegen->globalSymTable,pst);

	ExportContextData* data=context->data;
	FklByteCodelnt* libBc=sequnce_exp_bc_process(data->stack,fid,line);

	FklInstruction exportOpBc=create_op_imm_u32_ins(FKL_OP_EXPORT,codegen->libStack->top+1);
	FklInstruction ret=create_op_ins(FKL_OP_RET);

	fklBytecodeLntPushFrontIns(libBc,&exportOpBc,fid,line);
	fklBytecodeLntPushFrontIns(libBc,&ret,fid,line);

	FklCodegenLib* lib=fklCreateCodegenScriptLib(codegen
			,libBc
			,env);

	fklPushPtrStack(lib,codegen->libStack);

	codegen->realpath=NULL;

	codegen->export_macro=NULL;
	codegen->export_replacement=NULL;
	return process_import_imported_lib(codegen->libStack->top
				,data->codegen
				,lib
				,data->env
				,data->scope
				,data->cms
				,line
				,data->prefix
				,data->only
				,data->alias
				,data->except
				,errorState
				,pst);
}

static FklCodegenQuestContext* createExportContext(FklCodegenInfo* codegen
		,FklCodegenEnv* targetEnv
		,uint32_t scope
		,FklCodegenMacroScope* cms
		,FklNastNode* prefix
		,FklNastNode* only
		,FklNastNode* alias
		,FklNastNode* except)
{
	ExportContextData* data=(ExportContextData*)malloc(sizeof(ExportContextData));
	FKL_ASSERT(data);

	data->codegen=codegen;
	codegen->refcount++;

	data->scope=scope;
	data->env=targetEnv;
	targetEnv->refcount++;

	cms->refcount++;
	data->cms=cms;

	data->prefix=prefix?fklMakeNastNodeRef(prefix):NULL;

	data->only=only?fklMakeNastNodeRef(only):NULL;
	data->except=except?fklMakeNastNodeRef(except):NULL;
	data->alias=alias?fklMakeNastNodeRef(alias):NULL;
	data->stack=fklCreatePtrStack(16,16);
	return createCodegenQuestContext(data,&ExportContextMethodTable);
}

BC_PROCESS(_export_import_bc_process)
{
	ExportContextData* d=context->data;
	FklPtrStack* stack=d->stack;
	FklByteCodelnt* bcl=sequnce_exp_bc_process(stack,fid,line);
	FklCodegenEnv* targetEnv=d->env;

	FklHashTable* defs=&env->scopes[0].defs;

	FklUintStack idPstack=FKL_STACK_INIT;
	fklInitUintStack(&idPstack,defs->num,16);

	for(FklHashTableItem* list=defs->first;list;list=list->next)
	{
		FklSymbolDef* def=(FklSymbolDef*)(list->data);
		FklSid_t idp=def->k.id;
		fklAddCodegenDefBySid(idp,1,targetEnv);
		fklPushUintStack(idp,&idPstack);
	}

	if(idPstack.top)
	{
		FklHashTable* exports=&codegen->exports;

		uint64_t* idPbase=idPstack.base;
		uint32_t top=idPstack.top;

		for(uint32_t i=0;i<top;i++)
		{
			FklSid_t id=idPbase[i];
			fklPutHashItem(&id,exports);
		}

	}

	fklUninitUintStack(&idPstack);

	FklCodegenMacroScope* macros=targetEnv->macros;

	for(FklCodegenMacro* head=env->macros->head;head;head=head->next)
	{
		add_compiler_macro(&macros->head
				,fklMakeNastNodeRef(head->pattern)
				,fklMakeNastNodeRef(head->origin_exp)
				,head->bcl
				,head->prototype_id,0);

		add_compiler_macro(&codegen->export_macro
				,fklMakeNastNodeRef(head->pattern)
				,fklMakeNastNodeRef(head->origin_exp)
				,head->bcl
				,head->prototype_id,0);
	}

	for(FklHashTableItem* cur=env->macros->replacements->first;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->data;
		fklAddReplacementBySid(rep->id,rep->node,codegen->export_replacement);
		fklAddReplacementBySid(rep->id,rep->node,macros->replacements);
	}

	return bcl;
}

static inline FklCodegenInfo* get_lib_codegen(FklCodegenInfo* libCodegen)
{
	for(;libCodegen
			&&!libCodegen->libMark
			;libCodegen=libCodegen->prev)
		if(libCodegen->macroMark)
			return NULL;
	return libCodegen;
}

BC_PROCESS(exports_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top)
	{
		FklInstruction drop=create_op_ins(FKL_OP_DROP);
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		size_t top=stack->top;
		for(size_t i=0;i<top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			if(cur->bc->len)
			{
				fklCodeLntConcat(retval,cur);
				if(i<top-1)
					fklBytecodeLntPushFrontIns(retval,&drop,fid,line);
			}
			fklDestroyByteCodelnt(cur);
		}
		stack->top=0;
		return retval;
	}
	return NULL;
}

static CODEGEN_FUNC(codegen_export)
{
	FklCodegenInfo* libCodegen=get_lib_codegen(codegen);

	if(libCodegen&&scope==1&&curEnv==codegen->globalEnv&&macroScope==codegen->globalEnv->macros)
	{
		FklPtrQueue* exportQueue=fklCreatePtrQueue();
		FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
		add_export_symbol(libCodegen
				,origExp
				,rest
				,exportQueue);

		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(exports_bc_process
				,createDefaultStackContext(fklCreatePtrStack(2,1))
				,createDefaultQueueNextExpression(exportQueue)
				,scope
				,macroScope
				,curEnv
				,origExp->curline
				,codegen
				,codegenQuestStack);
	}
	else
	{
		errorState->type=FKL_ERR_INVALIDEXPR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
}

static inline FklSid_t get_reader_macro_group_id(const FklNastNode* node)
{
	for(;node->pair->cdr->type!=FKL_NAST_NIL;node=node->pair->cdr);
	const FklNastNode* group_id_node=node->pair->car;
	if(group_id_node->type==FKL_NAST_SYM)
		return group_id_node->sym;
	return 0;
}

static CODEGEN_FUNC(codegen_export_single)
{
	FklCodegenInfo* libCodegen=get_lib_codegen(codegen);
	if(!libCodegen||curEnv!=codegen->globalEnv||scope>1||macroScope!=codegen->globalEnv->macros)
	{
		errorState->type=FKL_ERR_INVALIDEXPR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}

	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	FklNastNode* orig_value=fklMakeNastNodeRef(value);
	value=fklTryExpandCodegenMacro(orig_value,codegen,macroScope,errorState);
	if(errorState->type)
		return;

	FklPtrQueue* queue=fklCreatePtrQueue();
	fklPushPtrQueue(value,queue);

	FklNastNode* const* builtin_pattern_node=outer_ctx->builtin_pattern_node;
	FklNastNode* name=NULL;
	if(!fklIsNastNodeList(value))
		goto error;
	if(isBeginExp(value,builtin_pattern_node))
	{
		uint32_t refcount=value->pair->car->refcount;
		uint64_t line=value->pair->car->curline;
		*(value->pair->car)=*(origExp->pair->car);
		value->pair->car->refcount=refcount;
		value->pair->car->curline=line;
		fklPushPtrStack(createCodegenQuest(exports_bc_process
					,createDefaultStackContext(fklCreatePtrStack(2,16))
					,createDefaultQueueNextExpression(queue)
					,1
					,macroScope
					,curEnv
					,origExp->curline
					,NULL
					,codegen)
				,codegenQuestStack);
	}
	else if(isExportDefunExp(value,builtin_pattern_node))
	{
		name=cadr_nast_node(value)->pair->car;
		goto process_def_in_lib;
	}
	else if(isExportDefineExp(value,builtin_pattern_node))
	{
		name=cadr_nast_node(value);
process_def_in_lib:
		if(name->type!=FKL_NAST_SYM)
			goto error;
		FklCodegenExportSidIndexHashItem* item=fklPutHashItem(&name->sym,&libCodegen->exports);
		item->idx=fklAddCodegenDefBySid(name->sym,1,curEnv);

		fklPushPtrStack(createCodegenQuest(_begin_exp_bc_process
					,createDefaultStackContext(fklCreatePtrStack(2,16))
					,createDefaultQueueNextExpression(queue)
					,1
					,macroScope
					,curEnv
					,origExp->curline
					,NULL
					,codegen)
				,codegenQuestStack);
	}
	else if(isExportDefmacroExp(value,builtin_pattern_node))
	{
		FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

		fklPushPtrStack(createCodegenQuest(_export_macro_bc_process
					,createEmptyContext()
					,createDefaultQueueNextExpression(queue)
					,1
					,cms
					,curEnv
					,origExp->curline
					,NULL
					,codegen)
				,codegenQuestStack);
	}
	else if(isExportDefReaderMacroExp(value,builtin_pattern_node))
	{
		FklSid_t group_id=get_reader_macro_group_id(value);
		if(!group_id)
			goto error;

		fklPutHashItem(&group_id,libCodegen->export_named_prod_groups);
		fklPushPtrStack(createCodegenQuest(_empty_bc_process
					,createEmptyContext()
					,createDefaultQueueNextExpression(queue)
					,1
					,macroScope
					,curEnv
					,origExp->curline
					,NULL
					,codegen)
				,codegenQuestStack);
	}
	else if(isExportImportExp(value,builtin_pattern_node))
	{

		FklCodegenEnv* exportEnv=fklCreateCodegenEnv(NULL,0,macroScope);
		FklCodegenMacroScope* cms=exportEnv->macros;

		fklPushPtrStack(createCodegenQuest(_export_import_bc_process
					,createExportContext(libCodegen,curEnv,1,cms,NULL,NULL,NULL,NULL)
					,createDefaultQueueNextExpression(queue)
					,1
					,cms
					,exportEnv
					,origExp->curline
					,NULL
					,codegen)
				,codegenQuestStack);
	}
	else
	{
error:
		fklDestroyNastNode(value);
		fklDestroyPtrQueue(queue);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
}

static size_t check_loaded_lib(const char* realpath,FklPtrStack* loadedLibStack)
{
	for(size_t i=0;i<loadedLibStack->top;i++)
	{
		FklCodegenLib* cur=loadedLibStack->base[i];
		if(!strcmp(realpath,cur->rp))
			return i+1;
	}
	return 0;
}

static inline void process_import_script_common_header(FklNastNode* origExp
		,FklNastNode* name
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegenInfo* codegen
		,FklCodegenErrorState* errorState
		,FklPtrStack* codegenQuestStack
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklNastNode* prefix
		,FklNastNode* only
		,FklNastNode* alias
		,FklNastNode* except
		,FklSymbolTable* pst)
{
	FklCodegenEnv* libEnv=fklCreateCodegenEnv(NULL,0,NULL);
	FklCodegenInfo* nextCodegen=createCodegenInfo(codegen,filename,libEnv,1,0,codegen->outer_ctx);

	fklInitGlobCodegenEnv(libEnv,pst);

	if(hasLoadSameFile(nextCodegen->realpath,codegen))
	{
		errorState->type=FKL_ERR_CIRCULARLOAD;
		errorState->place=fklMakeNastNodeRef(name);
		nextCodegen->refcount=1;
		fklDestroyCodegenInfo(nextCodegen);
		return;
	}
	size_t libId=check_loaded_lib(nextCodegen->realpath,codegen->libStack);
	if(!libId)
	{
		FILE* fp=fopen(filename,"r");
		if(!fp)
		{
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			nextCodegen->refcount=1;
			fklDestroyCodegenInfo(nextCodegen);
			return;
		}
		create_and_insert_to_pool(nextCodegen->pts
				,0
				,libEnv
				,nextCodegen->globalSymTable
				,0
				,nextCodegen->fid
				,1
				,pst);

		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_library_bc_process
				,createExportContext(codegen,curEnv,scope,macroScope,prefix,only,alias,except)
				,createFpNextExpression(fp,nextCodegen)
				,1
				,libEnv->macros
				,libEnv
				,origExp->curline
				,nextCodegen
				,codegenQuestStack);
	}
	else
	{
		fklDestroyHashTable(nextCodegen->export_replacement);
		nextCodegen->export_replacement=NULL;
		FklCodegenLib* lib=codegen->libStack->base[libId-1];

		FklByteCodelnt* importBc=process_import_imported_lib(libId
				,codegen
				,lib
				,curEnv
				,scope
				,macroScope
				,origExp->curline
				,prefix
				,only
				,alias
				,except
				,errorState
				,pst);
		FklPtrStack* bcStack=fklCreatePtrStack(1,1);
		fklPushPtrStack(importBc,bcStack);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process
				,createDefaultStackContext(bcStack)
				,NULL
				,1
				,nextCodegen->globalEnv->macros
				,nextCodegen->globalEnv
				,origExp->curline
				,nextCodegen
				,codegenQuestStack);
	}
}

FklCodegenDllLibInitExportFunc fklGetCodegenInitExportFunc(uv_lib_t* dll)
{
	return fklGetAddress("_fklExportSymbolInit",dll);
}

static inline FklByteCodelnt* process_import_from_dll_only(FklNastNode* origExp
		,FklNastNode* name
		,FklNastNode* only
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegenInfo* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope
		,FklSymbolTable* pst)
{
	char* realpath=fklRealpath(filename);
	size_t libId=check_loaded_lib(realpath,codegen->libStack);
	uv_lib_t dll;
	FklCodegenLib* lib=NULL;
	if(!libId)
	{
		if(uv_dlopen(realpath,&dll))
		{
			fprintf(stderr,"%s\n",uv_dlerror(&dll));
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			uv_dlclose(&dll);
			return NULL;
		}
		FklCodegenDllLibInitExportFunc initExport=fklGetCodegenInitExportFunc(&dll);
		if(!initExport)
		{
			uv_dlclose(&dll);
			errorState->type=FKL_ERR_IMPORTFAILED;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		lib=fklCreateCodegenDllLib(realpath
				,dll
				,codegen->globalSymTable
				,initExport
				,pst);
		fklPushPtrStack(lib,codegen->libStack);
		libId=codegen->libStack->top;
	}
	else
	{
		lib=codegen->libStack->base[libId-1];
		dll=lib->dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);

	FklInstruction bc=create_op_ins(FKL_OP_IMPORT);

	FklHashTable* exports=&lib->exports;

	for(;only->type==FKL_NAST_PAIR;only=only->pair->cdr)
	{
		FklSid_t cur=only->pair->car->sym;
		FklCodegenExportSidIndexHashItem* item=fklGetHashItem(&cur,exports);
		if(item)
		{
			uint32_t idx=fklAddCodegenDefBySid(cur,scope,curEnv);
			bc.imm=idx;
			bc.imm_u32=item->idx;
			fklBytecodeLntPushFrontIns(importBc,&bc,codegen->fid,only->curline);
		}
		else
		{
			errorState->type=FKL_ERR_IMPORT_MISSING;
			errorState->place=fklMakeNastNodeRef(only->pair->car);
			errorState->line=only->curline;
			break;
		}
	}

	return importBc;
}

static inline FklByteCodelnt* process_import_from_dll_except(FklNastNode* origExp
		,FklNastNode* name
		,FklNastNode* except
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegenInfo* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope
		,FklSymbolTable* pst)
{
	char* realpath=fklRealpath(filename);
	size_t libId=check_loaded_lib(realpath,codegen->libStack);
	uv_lib_t dll;
	FklCodegenLib* lib=NULL;
	if(!libId)
	{
		if(uv_dlopen(realpath,&dll))
		{
			fprintf(stderr,"%s\n",uv_dlerror(&dll));
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			uv_dlclose(&dll);
			return NULL;
		}
		FklCodegenDllLibInitExportFunc initExport=fklGetCodegenInitExportFunc(&dll);
		if(!initExport)
		{
			uv_dlclose(&dll);
			errorState->type=FKL_ERR_IMPORTFAILED;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		lib=fklCreateCodegenDllLib(realpath
				,dll
				,codegen->globalSymTable
				,initExport
				,pst);
		fklPushPtrStack(lib,codegen->libStack);
		libId=codegen->libStack->top;
	}
	else
	{
		lib=codegen->libStack->base[libId-1];
		dll=lib->dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);

	FklInstruction bc=create_op_ins(FKL_OP_IMPORT);

	FklHashTable* exports=&lib->exports;
	FklHashTable excepts;
	fklInitSidSet(&excepts);

	for(FklNastNode* list=except;list->type==FKL_NAST_PAIR;list=list->pair->cdr)
		fklPutHashItem(&list->pair->car->sym,&excepts);

	for(const FklHashTableItem* l=exports->first;l;l=l->next)
	{
		const FklCodegenExportSidIndexHashItem* item=(const FklCodegenExportSidIndexHashItem*)l->data;
		if(!fklGetHashItem(&item->sid,&excepts))
		{
			uint32_t idx=fklAddCodegenDefBySid(item->sid,scope,curEnv);
			bc.imm=idx;
			bc.imm_u32=item->idx;
			fklBytecodeLntPushFrontIns(importBc,&bc,codegen->fid,except->curline);
		}
	}

	return importBc;
}

static inline FklByteCodelnt* process_import_from_dll_common(FklNastNode* origExp
		,FklNastNode* name
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegenInfo* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope
		,FklSymbolTable* pst)
{
	char* realpath=fklRealpath(filename);
	size_t libId=check_loaded_lib(realpath,codegen->libStack);
	uv_lib_t dll;
	FklCodegenLib* lib=NULL;
	if(!libId)
	{
		if(uv_dlopen(realpath,&dll))
		{
			fprintf(stderr,"%s\n",uv_dlerror(&dll));
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			uv_dlclose(&dll);
			return NULL;
		}
		FklCodegenDllLibInitExportFunc initExport=fklGetCodegenInitExportFunc(&dll);
		if(!initExport)
		{
			uv_dlclose(&dll);
			errorState->type=FKL_ERR_IMPORTFAILED;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		lib=fklCreateCodegenDllLib(realpath
				,dll
				,codegen->globalSymTable
				,initExport
				,pst);
		fklPushPtrStack(lib,codegen->libStack);
		libId=codegen->libStack->top;
	}
	else
	{
		lib=codegen->libStack->base[libId-1];
		dll=lib->dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);
	add_symbol_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,&lib->exports
			,importBc
			,codegen->fid
			,origExp->curline
			,scope
			,pst);
	return importBc;
}

static inline FklByteCodelnt* process_import_from_dll_prefix(FklNastNode* origExp
		,FklNastNode* name
		,FklNastNode* prefixNode
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegenInfo* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope
		,FklSymbolTable* pst)
{
	char* realpath=fklRealpath(filename);
	size_t libId=check_loaded_lib(realpath,codegen->libStack);
	uv_lib_t dll;
	FklCodegenLib* lib=NULL;
	if(!libId)
	{
		if(uv_dlopen(realpath,&dll))
		{
			fprintf(stderr,"%s\n",uv_dlerror(&dll));
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			uv_dlclose(&dll);
			return NULL;
		}
		FklCodegenDllLibInitExportFunc initExport=fklGetCodegenInitExportFunc(&dll);
		if(!initExport)
		{
			uv_dlclose(&dll);
			errorState->type=FKL_ERR_IMPORTFAILED;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		lib=fklCreateCodegenDllLib(realpath
				,dll
				,codegen->globalSymTable
				,initExport
				,pst);
		fklPushPtrStack(lib,codegen->libStack);
		libId=codegen->libStack->top;
	}
	else
	{
		lib=codegen->libStack->base[libId-1];
		dll=lib->dll;
		free(realpath);
	}

	FklString* prefix=fklGetSymbolWithId(prefixNode->sym,pst)->symbol;

	FklByteCodelnt* importBc=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);
	add_symbol_with_prefix_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,prefix
			,&lib->exports
			,importBc
			,codegen->fid
			,origExp->curline
			,scope
			,pst);

	return importBc;
}

static inline FklByteCodelnt* process_import_from_dll_alias(FklNastNode* origExp
		,FklNastNode* name
		,FklNastNode* alias
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegenInfo* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope
		,FklSymbolTable* pst)
{
	char* realpath=fklRealpath(filename);
	size_t libId=check_loaded_lib(realpath,codegen->libStack);
	uv_lib_t dll;
	FklCodegenLib* lib=NULL;
	if(!libId)
	{
		if(uv_dlopen(realpath,&dll))
		{
			fprintf(stderr,"%s\n",uv_dlerror(&dll));
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			uv_dlclose(&dll);
			return NULL;
		}
		FklCodegenDllLibInitExportFunc initExport=fklGetCodegenInitExportFunc(&dll);
		if(!initExport)
		{
			uv_dlclose(&dll);
			errorState->type=FKL_ERR_IMPORTFAILED;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		lib=fklCreateCodegenDllLib(realpath
				,dll
				,codegen->globalSymTable
				,initExport
				,pst);
		fklPushPtrStack(lib,codegen->libStack);
		libId=codegen->libStack->top;
	}
	else
	{
		lib=codegen->libStack->base[libId-1];
		dll=lib->dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=fklCreateSingleInsBclnt(create_op_imm_u32_ins(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);

	FklInstruction bc=create_op_ins(FKL_OP_IMPORT);

	FklHashTable* exports=&lib->exports;

	for(;alias->type==FKL_NAST_PAIR;alias=alias->pair->cdr)
	{
		FklNastNode* curNode=alias->pair->car;
		FklSid_t cur=curNode->pair->car->sym;
		FklCodegenExportSidIndexHashItem* item=fklGetHashItem(&cur,exports);

		if(item)
		{
			FklSid_t cur0=curNode->pair->cdr->pair->car->sym;
			uint32_t idx=fklAddCodegenDefBySid(cur0,scope,curEnv);
			bc.imm=idx;
			bc.imm_u32=item->idx;
			fklBytecodeLntPushFrontIns(importBc,&bc,codegen->fid,alias->curline);
		}
		else
		{
			errorState->type=FKL_ERR_IMPORT_MISSING;
			errorState->place=fklMakeNastNodeRef(curNode->pair->car);
			errorState->line=alias->curline;
			break;
		}
	}

	return importBc;
}

static inline FklByteCodelnt* process_import_from_dll(FklNastNode* origExp
		,FklNastNode* name
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegenInfo* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope
		,FklNastNode* prefix
		,FklNastNode* only
		,FklNastNode* alias
		,FklNastNode* except
		,FklSymbolTable* pst)
{
	if(prefix)
		return process_import_from_dll_prefix(origExp
				,name
				,prefix
				,filename
				,curEnv
				,codegen
				,errorState
				,scope
				,pst);
	if(only)
		return process_import_from_dll_only(origExp
				,name
				,only
				,filename
				,curEnv
				,codegen
				,errorState
				,scope
				,pst);
	if(alias)
		return process_import_from_dll_alias(origExp
				,name
				,alias
				,filename
				,curEnv
				,codegen
				,errorState
				,scope
				,pst);
	if(except)
		return process_import_from_dll_except(origExp
				,name
				,except
				,filename
				,curEnv
				,codegen
				,errorState
				,scope
				,pst);
	return process_import_from_dll_common(origExp
			,name
			,filename
			,curEnv
			,codegen
			,errorState
			,scope
			,pst);
}

static inline int is_valid_alias_sym_list(FklNastNode* alias)
{
	for(;alias->type==FKL_NAST_PAIR;alias=alias->pair->cdr)
	{
		FklNastNode* cur=alias->pair->car;
		if(cur->type!=FKL_NAST_PAIR
				||cur->pair->car->type!=FKL_NAST_SYM
				||cur->pair->cdr->type!=FKL_NAST_PAIR
				||cur->pair->cdr->pair->car->type!=FKL_NAST_SYM
				||cur->pair->cdr->pair->cdr->type!=FKL_NAST_NIL)
			return 0;
	}

	return alias->type==FKL_NAST_NIL;
}

static inline void codegen_import_helper(FklNastNode* origExp
		,FklNastNode* name
		,FklNastNode* rest
		,FklCodegenInfo* codegen
		,FklCodegenErrorState* errorState
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklPtrStack* codegenQuestStack
		,FklNastNode* prefix
		,FklNastNode* only
		,FklNastNode* alias
		,FklNastNode* except
		,FklSymbolTable* pst)
{
	if(name->type==FKL_NAST_NIL||!fklIsNastNodeListAndHasSameType(name,FKL_NAST_SYM)
			||(prefix&&prefix->type!=FKL_NAST_SYM)
			||(only&&!fklIsNastNodeListAndHasSameType(only,FKL_NAST_SYM))
			||(except&&!fklIsNastNodeListAndHasSameType(except,FKL_NAST_SYM))
			||(alias&&!is_valid_alias_sym_list(alias)))
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}

	if(rest->type!=FKL_NAST_NIL)
	{
		FklPtrQueue* queue=fklCreatePtrQueue();

		FklNastPair* prevPair=origExp->pair->cdr->pair;

		FklNastNode* importHead=origExp->pair->car;

		for(;rest->type==FKL_NAST_PAIR;rest=rest->pair->cdr)
		{
			FklNastNode* restExp=fklNastCons(fklMakeNastNodeRef(importHead),rest,rest->curline);

			prevPair->cdr=fklCreateNastNode(FKL_NAST_NIL,rest->curline);

			fklPushPtrQueue(restExp,queue);

			prevPair=rest->pair;
		}
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_begin_exp_bc_process
				,createDefaultStackContext(fklCreatePtrStack(2,1))
				,createDefaultQueueNextExpression(queue)
				,scope
				,macroScope
				,curEnv
				,origExp->curline
				,codegen
				,codegenQuestStack);
	}

	char* filename=combineFileNameFromList(name,pst);

	char* packageMainFileName=fklStrCat(fklCopyCstr(filename),FKL_PATH_SEPARATOR_STR);
	packageMainFileName=fklStrCat(packageMainFileName,FKL_PACKAGE_MAIN_FILE);

	char* preCompileFileName=fklStrCat(fklCopyCstr(packageMainFileName),FKL_PRE_COMPILE_FKL_SUFFIX_STR);

	char* scriptFileName=fklStrCat(fklCopyCstr(filename),FKL_SCRIPT_FILE_EXTENSION);

	char* dllFileName=fklStrCat(fklCopyCstr(filename),FKL_DLL_FILE_TYPE);

	if(fklIsAccessibleRegFile(packageMainFileName))
		process_import_script_common_header(origExp
				,name
				,packageMainFileName
				,curEnv
				,codegen
				,errorState
				,codegenQuestStack
				,scope
				,macroScope
				,prefix
				,only
				,alias
				,except
				,pst);
	else if(fklIsAccessibleRegFile(scriptFileName))
		process_import_script_common_header(origExp
				,name
				,scriptFileName
				,curEnv
				,codegen
				,errorState
				,codegenQuestStack
				,scope
				,macroScope
				,prefix
				,only
				,alias
				,except
				,pst);
	else if(fklIsAccessibleRegFile(preCompileFileName))
	{
		size_t libId=check_loaded_lib(preCompileFileName,codegen->libStack);
		if(!libId)
		{
			char* errorStr=NULL;
			if(fklLoadPreCompile(codegen->pts
						,codegen->macro_pts
						,codegen->libStack
						,codegen->macroLibStack
						,codegen->globalSymTable
						,codegen->outer_ctx
						,preCompileFileName
						,NULL
						,&errorStr))
			{
				if(errorStr)
				{
					fprintf(stderr,"%s\n",errorStr);
					free(errorStr);
				}
				errorState->type=FKL_ERR_IMPORTFAILED;
				errorState->place=fklMakeNastNodeRef(name);
				goto exit;
			}
			libId=codegen->libStack->top;
		}
		FklCodegenLib* lib=codegen->libStack->base[libId-1];

		FklByteCodelnt* importBc=process_import_imported_lib(libId
				,codegen
				,lib
				,curEnv
				,scope
				,macroScope
				,origExp->curline
				,prefix
				,only
				,alias
				,except
				,errorState
				,pst);
		FklPtrStack* bcStack=fklCreatePtrStack(1,1);
		fklPushPtrStack(importBc,bcStack);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process
				,createDefaultStackContext(bcStack)
				,NULL
				,1
				,codegen->globalEnv->macros
				,codegen->globalEnv
				,origExp->curline
				,codegen
				,codegenQuestStack);
	}
	else if(fklIsAccessibleRegFile(dllFileName))
	{
		FklByteCodelnt* bc=process_import_from_dll(origExp
				,name
				,dllFileName
				,curEnv
				,codegen
				,errorState
				,scope
				,prefix
				,only
				,alias
				,except
				,pst);
		if(bc)
		{
			FklPtrStack* bcStack=fklCreatePtrStack(1,1);
			fklPushPtrStack(bc,bcStack);
			FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process
					,createDefaultStackContext(bcStack)
					,NULL
					,1
					,NULL
					,NULL
					,origExp->curline
					,codegen
					,codegenQuestStack);
		}
	}
	else
	{
		errorState->type=FKL_ERR_IMPORTFAILED;
		errorState->place=fklMakeNastNodeRef(name);
	}
exit:
	free(filename);
	free(scriptFileName);
	free(dllFileName);
	free(packageMainFileName);
	free(preCompileFileName);
}

static CODEGEN_FUNC(codegen_import)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);

	codegen_import_helper(origExp
			,name
			,rest
			,codegen
			,errorState
			,curEnv
			,scope
			,macroScope
			,codegenQuestStack
			,NULL
			,NULL
			,NULL
			,NULL
			,&outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_none)
{
	fklPushPtrStack(createCodegenQuest(_empty_bc_process
				,createEmptyContext()
				,NULL
				,1
				,macroScope
				,curEnv
				,origExp->curline
				,NULL
				,codegen)
			,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_import_prefix)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* prefix=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);

	codegen_import_helper(origExp
			,name
			,rest
			,codegen
			,errorState
			,curEnv
			,scope
			,macroScope
			,codegenQuestStack
			,prefix
			,NULL
			,NULL
			,NULL
			,&outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_only)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* only=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);

	codegen_import_helper(origExp
			,name
			,rest
			,codegen
			,errorState
			,curEnv
			,scope
			,macroScope
			,codegenQuestStack
			,NULL
			,only
			,NULL
			,NULL
			,&outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_alias)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* alias=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);

	codegen_import_helper(origExp
			,name
			,rest
			,codegen
			,errorState
			,curEnv
			,scope
			,macroScope
			,codegenQuestStack
			,NULL
			,NULL
			,alias
			,NULL
			,&outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_except)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* except=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);

	codegen_import_helper(origExp
			,name
			,rest
			,codegen
			,errorState
			,curEnv
			,scope
			,macroScope
			,codegenQuestStack
			,NULL
			,NULL
			,NULL
			,except
			,&outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_common)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_args,ht);

	codegen_import_helper(origExp
			,name
			,rest
			,codegen
			,errorState
			,curEnv
			,scope
			,macroScope
			,codegenQuestStack
			,NULL
			,NULL
			,NULL
			,NULL
			,&outer_ctx->public_symbol_table);
}

typedef struct
{
	FklPtrStack* stack;
	FklNastNode* pattern;
	FklNastNode* origin_exp;
	FklCodegenMacroScope* macroScope;
	uint32_t prototype_id;
}MacroContext;

static inline MacroContext* createMacroContext(FklNastNode* origin_exp
		,FklNastNode* pattern
		,FklCodegenMacroScope* macroScope
		,uint32_t prototype_id)
{
	MacroContext* r=(MacroContext*)malloc(sizeof(MacroContext));
	FKL_ASSERT(r);
	r->pattern=pattern;
	r->origin_exp=fklMakeNastNodeRef(origin_exp);
	r->macroScope=macroScope?make_macro_scope_ref(macroScope):NULL;
	r->stack=fklCreatePtrStack(1,1);
	r->prototype_id=prototype_id;
	return r;
}

BC_PROCESS(_compiler_macro_bc_process)
{
	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	fklUpdatePrototype(codegen->pts,env,codegen->globalSymTable,pst);
	fklPrintUndefinedRef(env,codegen->globalSymTable,pst);

	MacroContext* d=(MacroContext*)(context->data);
	FklPtrStack* stack=d->stack;
	FklByteCodelnt* macroBcl=fklPopPtrStack(stack);
	FklInstruction ret=create_op_ins(FKL_OP_RET);
	fklBytecodeLntPushFrontIns(macroBcl,&ret,fid,line);

	FklNastNode* pattern=d->pattern;
	FklCodegenMacroScope* macros=d->macroScope;
	uint32_t prototype_id=d->prototype_id;

	add_compiler_macro(&macros->head
			,fklMakeNastNodeRef(pattern)
			,fklMakeNastNodeRef(d->origin_exp)
			,macroBcl
			,prototype_id
			,1);
	return NULL;
}

static FklPtrStack* _macro_stack_context_get_bcl_stack(void* data)
{
	return ((MacroContext*)data)->stack;
}

static void _macro_stack_context_put_bcl(void* data,FklByteCodelnt* bcl)
{
	MacroContext* d=(MacroContext*)data;
	fklPushPtrStack(bcl,d->stack);
}

static void _macro_stack_context_finalizer(void* data)
{
	MacroContext* d=(MacroContext*)data;
	FklPtrStack* s=d->stack;
	while(!fklIsPtrStackEmpty(s))
		fklDestroyByteCodelnt(fklPopPtrStack(s));
	fklDestroyPtrStack(s);
	fklDestroyNastNode(d->pattern);
	fklDestroyNastNode(d->origin_exp);
	if(d->macroScope)
		fklDestroyCodegenMacroScope(d->macroScope);
	free(d);
}

static const FklCodegenQuestContextMethodTable MacroStackContextMethodTable=
{
	.__get_bcl_stack=_macro_stack_context_get_bcl_stack,
	.__put_bcl=_macro_stack_context_put_bcl,
	.__finalizer=_macro_stack_context_finalizer,
};

static inline FklCodegenQuestContext* createMacroQuestContext(FklNastNode* origin_exp,FklNastNode* pattern
		,FklCodegenMacroScope* macroScope
		,uint32_t prototype_id)
{
	return createCodegenQuestContext(createMacroContext(origin_exp,pattern,macroScope,prototype_id),&MacroStackContextMethodTable);
}

struct CustomActionCtx
{
	uint64_t refcount;
	FklByteCodelnt* bcl;
	FklFuncPrototypes* pts;
	FklPtrStack* macroLibStack;
	FklCodegenOuterCtx* codegen_outer_ctx;
	FklSymbolTable* pst;
	uint32_t prototype_id;
};

struct ReaderMacroCtx
{
	FklPtrStack* stack;
	FklFuncPrototypes* pts;
	struct CustomActionCtx* action_ctx;
};

static void* custom_action(void* c
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	FklNastNode* nodes_vector=fklCreateNastNode(FKL_NAST_VECTOR,line);
	nodes_vector->vec=fklCreateNastVector(num);
	for(size_t i=0;i<num;i++)
		nodes_vector->vec->base[i]=fklMakeNastNodeRef(nodes[i]);
	FklHashTable lineHash;
	fklInitLineNumHashTable(&lineHash);
	FklHashTable ht;
	fklInitPatternMatchHashTable(&ht);
	FklNastNode* line_node=NULL;
	if(line>FKL_FIX_INT_MAX)
	{
		line_node=fklCreateNastNode(FKL_NAST_BIG_INT,line);
		line_node->bigInt=fklCreateBigIntU(line);
	}
	else
	{
		line_node=fklCreateNastNode(FKL_NAST_FIX,line);
		line_node->fix=line;
	}

	struct CustomActionCtx* ctx=(struct CustomActionCtx*)c;
	fklPatternMatchingHashTableSet(fklAddSymbolCstr("$$",ctx->pst)->id,nodes_vector,&ht);
	fklPatternMatchingHashTableSet(fklAddSymbolCstr("line",ctx->pst)->id,line_node,&ht);

	FklNastNode* r=NULL;
	const char* cwd=ctx->codegen_outer_ctx->cwd;
	const char* file_dir=ctx->codegen_outer_ctx->cur_file_dir;
	fklChdir(file_dir);

	FklVM* anotherVM=fklInitMacroExpandVM(ctx->bcl,ctx->pts,ctx->prototype_id,&ht,&lineHash,ctx->macroLibStack,&r,line,ctx->pst);
	FklVMgc* gc=anotherVM->gc;

	int e=fklRunVM(anotherVM);

	fklChdir(cwd);

	anotherVM->pts=NULL;
	if(e)
		fklDeleteCallChain(anotherVM);

	fklDestroyNastNode(nodes_vector);
	fklDestroyNastNode(line_node);

	fklUninitHashTable(&ht);
	fklUninitHashTable(&lineHash);
	fklDestroyAllVMs(anotherVM);
	fklDestroyVMgc(gc);
	return r;
}

static void* custom_action_ctx_copy(const void* c)
{
	struct CustomActionCtx* ctx=(struct CustomActionCtx*)c;
	ctx->refcount++;
	return ctx;
}

static void custom_action_ctx_destroy(void* c)
{
	struct CustomActionCtx* ctx=(struct CustomActionCtx*)c;
	if(ctx->refcount)
		ctx->refcount--;
	else
	{
		if(ctx->bcl)
			fklDestroyByteCodelnt(ctx->bcl);
		free(ctx);
	}
}

static inline struct ReaderMacroCtx* createReaderMacroContext(struct CustomActionCtx* ctx
		,FklFuncPrototypes* pts)
{
	struct ReaderMacroCtx* r=(struct ReaderMacroCtx*)malloc(sizeof(struct ReaderMacroCtx));
	FKL_ASSERT(r);
	r->action_ctx=custom_action_ctx_copy(ctx);
	r->pts=pts;
	r->stack=fklCreatePtrStack(1,1);
	return r;
}

BC_PROCESS(_reader_macro_bc_process)
{
	struct ReaderMacroCtx* d=(struct ReaderMacroCtx*)(context->data);
	FklFuncPrototypes* pts=d->pts;
	struct CustomActionCtx* custom_ctx=d->action_ctx;
	d->pts=NULL;
	d->action_ctx=NULL;

	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	custom_action_ctx_destroy(custom_ctx);
	fklUpdatePrototype(codegen->pts,env,codegen->globalSymTable,pst);
	fklPrintUndefinedRef(env,codegen->globalSymTable,pst);

	FklPtrStack* stack=d->stack;
	FklByteCodelnt* macroBcl=fklPopPtrStack(stack);
	FklInstruction ret=create_op_ins(FKL_OP_RET);
	fklBytecodeLntPushFrontIns(macroBcl,&ret,fid,line);

	custom_ctx->pts=pts;
	custom_ctx->bcl=macroBcl;
	return NULL;
}

static FklPtrStack* _reader_macro_stack_context_get_bcl_stack(void* data)
{
	return ((struct ReaderMacroCtx*)data)->stack;
}

static void _reader_macro_stack_context_put_bcl(void* data,FklByteCodelnt* bcl)
{
	struct ReaderMacroCtx* d=(struct ReaderMacroCtx*)data;
	fklPushPtrStack(bcl,d->stack);
}

static void _reader_macro_stack_context_finalizer(void* data)
{
	struct ReaderMacroCtx* d=(struct ReaderMacroCtx*)data;
	FklPtrStack* s=d->stack;
	while(!fklIsPtrStackEmpty(s))
		fklDestroyByteCodelnt(fklPopPtrStack(s));
	fklDestroyPtrStack(s);
	if(d->action_ctx)
		custom_action_ctx_destroy(d->action_ctx);
	free(d);
}

static const FklCodegenQuestContextMethodTable ReaderMacroStackContextMethodTable=
{
	.__get_bcl_stack=_reader_macro_stack_context_get_bcl_stack,
	.__put_bcl=_reader_macro_stack_context_put_bcl,
	.__finalizer=_reader_macro_stack_context_finalizer,
};

static inline FklCodegenQuestContext* createReaderMacroQuestContext(struct CustomActionCtx* ctx
		,FklFuncPrototypes* pts)
{
	return createCodegenQuestContext(createReaderMacroContext(ctx,pts),&ReaderMacroStackContextMethodTable);
}

static FklNastNode* _nil_replacement(const FklNastNode* orig
		,const FklCodegenEnv* env
		,const FklCodegenInfo* codegen)
{
	return fklCreateNastNode(FKL_NAST_NIL,orig->curline);
}

static FklNastNode* _is_main_replacement(const FklNastNode* orig
		,const FklCodegenEnv* env
		,const FklCodegenInfo* codegen)
{
	FklNastNode* n=fklCreateNastNode(FKL_NAST_FIX,orig->curline);
	if(env->prototypeId==1)
		n->fix=1;
	else
		n->type=FKL_NAST_NIL;
	return n;
}

static FklNastNode* _platform_replacement(const FklNastNode* orig
		,const FklCodegenEnv* env
		,const FklCodegenInfo* codegen)
{

#if __linux__
	static const char* platform="linux";
#elif __unix__
	static const char* platform="unix";
#elif defined _WIN32
	static const char* platform="win32";
#elif __APPLE__
	static const char* platform="apple";
#endif
	FklNastNode* n=fklCreateNastNode(FKL_NAST_STR,orig->curline);
	n->str=fklCreateStringFromCstr(platform);
	return n;
}

static FklNastNode* _file_dir_replacement(const FklNastNode* orig
		,const FklCodegenEnv* env
		,const FklCodegenInfo* codegen)
{
	FklString* s=NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_STR,orig->curline);
	if(codegen->filename==NULL)
		s=fklCreateStringFromCstr(codegen->outer_ctx->cwd);
	else
		s=fklCreateStringFromCstr(codegen->dir);
	fklStringCstrCat(&s,FKL_PATH_SEPARATOR_STR);
	r->str=s;
	return r;
}

static FklNastNode* _file_replacement(const FklNastNode* orig
		,const FklCodegenEnv* env
		,const FklCodegenInfo* codegen)
{
	FklNastNode* r=NULL;
	if(codegen->filename==NULL)
		r=fklCreateNastNode(FKL_NAST_NIL,orig->curline);
	else
	{
		r=fklCreateNastNode(FKL_NAST_STR,orig->curline);
		FklString* s=NULL;
		s=fklCreateStringFromCstr(codegen->filename);
		r->str=s;
	}
	return r;
}

static FklNastNode* _file_rp_replacement(const FklNastNode* orig
		,const FklCodegenEnv* env
		,const FklCodegenInfo* codegen)
{
	FklNastNode* r=NULL;
	if(codegen->realpath==NULL)
		r=fklCreateNastNode(FKL_NAST_NIL,orig->curline);
	else
	{
		r=fklCreateNastNode(FKL_NAST_STR,orig->curline);
		FklString* s=NULL;
		s=fklCreateStringFromCstr(codegen->realpath);
		r->str=s;
	}
	return r;
}

static FklNastNode* _line_replacement(const FklNastNode* orig
		,const FklCodegenEnv* env
		,const FklCodegenInfo* codegen)
{
	uint64_t line=orig->curline;
	FklNastNode* r=NULL;
	if(line<FKL_FIX_INT_MAX)
	{
		r=fklCreateNastNode(FKL_NAST_FIX,orig->curline);
		r->fix=line;
	}
	else
	{
		r=fklCreateNastNode(FKL_NAST_BIG_INT,orig->curline);
		r->bigInt=fklCreateBigInt(line);
	}
	return r;
}

static struct SymbolReplacement
{
	const char* s;
	ReplacementFunc func;
}builtInSymbolReplacement[FKL_BUILTIN_REPLACEMENT_NUM]=
{
	{"nil",        _nil_replacement,      },
	{"*line*",     _line_replacement,     },
	{"*file*",     _file_replacement,     },
	{"*file-rp*",  _file_rp_replacement,  },
	{"*file-dir*", _file_dir_replacement, },
	{"*main?*",    _is_main_replacement,  },
	{"*platform*", _platform_replacement, },
};

static inline ReplacementFunc findBuiltInReplacementWithId(FklSid_t id,const FklSid_t builtin_replacement_id[])
{
	for(size_t i=0;i<FKL_BUILTIN_REPLACEMENT_NUM;i++)
	{
		if(builtin_replacement_id[i]==id)
			return builtInSymbolReplacement[i].func;
	}
	return NULL;
}

static void* codegen_prod_action_nil(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	return fklCreateNastNode(FKL_NAST_NIL,line);
}

static void* codegen_prod_action_first(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<1)
		return NULL;
	return fklMakeNastNodeRef(nodes[0]);
}

static void* codegen_prod_action_symbol(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<1)
		return NULL;
    FklNastNode* node=nodes[0];
    if(node->type!=FKL_NAST_STR)
        return NULL;
    FklNastNode* sym=fklCreateNastNode(FKL_NAST_SYM,node->curline);
    sym->sym=fklAddSymbol(node->str,outerCtx)->id;
	return sym;
}

static void* codegen_prod_action_second(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<2)
		return NULL;
	return fklMakeNastNodeRef(nodes[1]);
}

static void* codegen_prod_action_third(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<3)
		return NULL;
	return fklMakeNastNodeRef(nodes[2]);
}

static inline void* codegen_prod_action_pair(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<3)
		return NULL;
	FklNastNode* car=nodes[0];
	FklNastNode* cdr=nodes[2];
	FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);
	pair->pair=fklCreateNastPair();
	pair->pair->car=fklMakeNastNodeRef(car);
	pair->pair->cdr=fklMakeNastNodeRef(cdr);
	return pair;
}

static inline void* codegen_prod_action_cons(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num==1)
	{
		FklNastNode* car=nodes[0];
		FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);
		pair->pair=fklCreateNastPair();
		pair->pair->car=fklMakeNastNodeRef(car);
		pair->pair->cdr=fklCreateNastNode(FKL_NAST_NIL,line);
		return pair;
	}
	else if(num==2)
	{
		FklNastNode* car=nodes[0];
		FklNastNode* cdr=nodes[1];
		FklNastNode* pair=fklCreateNastNode(FKL_NAST_PAIR,line);
		pair->pair=fklCreateNastPair();
		pair->pair->car=fklMakeNastNodeRef(car);
		pair->pair->cdr=fklMakeNastNodeRef(cdr);
		return pair;
	}
	else
		return NULL;
}

static inline void* codegen_prod_action_box(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<2)
		return NULL;
	FklNastNode* box=fklCreateNastNode(FKL_NAST_BOX,line);
	box->box=fklMakeNastNodeRef(nodes[1]);
	return box;
}

static inline void* codegen_prod_action_vector(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<2)
		return NULL;
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_VECTOR,line);
	size_t len=fklNastListLength(list);
	FklNastVector* vec=fklCreateNastVector(len);
	r->vec=vec;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
		vec->base[i]=fklMakeNastNodeRef(list->pair->car);
	return r;
}

static inline void* codegen_prod_action_quote(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<2)
		return NULL;
	FklSid_t id=fklAddSymbolCstr("quote",outerCtx)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* codegen_prod_action_unquote(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<2)
		return NULL;
	FklSid_t id=fklAddSymbolCstr("unquote",outerCtx)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* codegen_prod_action_qsquote(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<2)
		return NULL;
	FklSid_t id=fklAddSymbolCstr("qsquote",outerCtx)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* codegen_prod_action_unqtesp(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<2)
		return NULL;
	FklSid_t id=fklAddSymbolCstr("unqtesp",outerCtx)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline void* codegen_prod_action_hasheq(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<2)
		return NULL;
	FklNastNode* list=nodes[1];
	if(!fklIsNastNodeListAndHasSameType(list,FKL_NAST_PAIR))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQ,len);
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
	}
	return r;
}

static inline void* codegen_prod_action_hasheqv(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<2)
		return 0;
	FklNastNode* list=nodes[1];
	if(!fklIsNastNodeListAndHasSameType(list,FKL_NAST_PAIR))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQV,len);
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
	}
	return r;
}

static inline void* codegen_prod_action_hashequal(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<2)
		return 0;
	FklNastNode* list=nodes[1];
	if(!fklIsNastNodeListAndHasSameType(list,FKL_NAST_PAIR))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQUAL,len);
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
	}
	return r;
}

static inline void* codegen_prod_action_bytevector(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	if(num<2)
		return NULL;
	FklNastNode* list=nodes[1];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_BYTEVECTOR,line);
	size_t len=fklNastListLength(list);
	FklBytevector* bv=fklCreateBytevector(len,NULL);
	r->bvec=bv;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		FklNastNode* cur=list->pair->car;
		if(cur->type==FKL_NAST_FIX)
			bv->ptr[i]=cur->fix>UINT8_MAX?UINT8_MAX:(cur->fix<0?0:cur->fix);
		else
			bv->ptr[i]=cur->bigInt->neg?0:UINT8_MAX;
	}
	return r;
}

static struct CstrIdProdAction
{
	const char* name;
	FklProdActionFunc func;
}CodegenProdActions[FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM]=
{
	{"nil",        codegen_prod_action_nil,        },
	{"symbol",     codegen_prod_action_symbol,     },
	{"first",      codegen_prod_action_first,      },
	{"second",     codegen_prod_action_second,     },
	{"third",      codegen_prod_action_third,      },
	{"pair",       codegen_prod_action_pair,       },
	{"cons",       codegen_prod_action_cons,       },
	{"box",        codegen_prod_action_box,        },
	{"vector",     codegen_prod_action_vector,     },
	{"quote",      codegen_prod_action_quote,      },
	{"unquote",    codegen_prod_action_unquote,    },
	{"qsquote",    codegen_prod_action_qsquote,    },
	{"unqtesp",    codegen_prod_action_unqtesp,    },
	{"hasheq",     codegen_prod_action_hasheq,     },
	{"hasheqv",    codegen_prod_action_hasheqv,    },
	{"hashequal",  codegen_prod_action_hashequal,  },
	{"bytevector", codegen_prod_action_bytevector, },
};

static inline void init_builtin_prod_action_list(FklSid_t* builtin_prod_action_id,FklSymbolTable* pst)
{
	for(size_t i=0;i<FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM;i++)
		builtin_prod_action_id[i]=fklAddSymbolCstr(CodegenProdActions[i].name,pst)->id;
}

static inline FklProdActionFunc find_builtin_prod_action(FklSid_t id,FklSid_t* builtin_prod_action_id)
{
	for(size_t i=0;i<FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM;i++)
	{
		if(builtin_prod_action_id[i]==id)
			return CodegenProdActions[i].func;
	}
	return NULL;
}

static void* simple_action_nth_create(FklNastNode* rest[],size_t rest_len,int* failed)
{
	if(rest_len!=1
			||rest[0]->type!=FKL_NAST_FIX
			||rest[0]->fix<0)
	{
		*failed=1;
		return NULL;
	}
	return (void*)(rest[0]->fix);
}

static void* simple_action_nth(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	uintptr_t nth=(uintptr_t)ctx;
	if(nth>=num)
		return NULL;
	return fklMakeNastNodeRef(nodes[nth]);
}

struct SimpleActionConsCtx
{
	uint64_t car;
	uint64_t cdr;
};

static void* simple_action_cons_create(FklNastNode* rest[],size_t rest_len,int* failed)
{
	if(rest_len!=2
			||rest[0]->type!=FKL_NAST_FIX
			||rest[0]->fix<0
			||rest[1]->type!=FKL_NAST_FIX
			||rest[1]->fix<0)
	{
		*failed=1;
		return NULL;
	}
	struct SimpleActionConsCtx* ctx=(struct SimpleActionConsCtx*)malloc(sizeof(struct SimpleActionConsCtx));
	FKL_ASSERT(ctx);
	ctx->car=rest[0]->fix;
	ctx->cdr=rest[1]->fix;
	return ctx;
}

static void* simple_action_cons_copy(const void* c)
{
	struct SimpleActionConsCtx* ctx=(struct SimpleActionConsCtx*)malloc(sizeof(struct SimpleActionConsCtx));
	FKL_ASSERT(ctx);
	*ctx=*(struct SimpleActionConsCtx*)c;
	return ctx;
}

static void* simple_action_cons(void* c
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	struct SimpleActionConsCtx* cc=(struct SimpleActionConsCtx*)c;
	if(cc->car>=num
			||cc->cdr>=num)
		return NULL;
	FklNastNode* retval=fklCreateNastNode(FKL_NAST_PAIR,line);
	retval->pair=fklCreateNastPair();
	retval->pair->car=fklMakeNastNodeRef(nodes[cc->car]);
	retval->pair->cdr=fklMakeNastNodeRef(nodes[cc->cdr]);
	return retval;
}

struct SimpleActionHeadCtx
{
	FklNastNode* head;
	uint64_t idx_num;
	uint64_t idx[];
};

static void* simple_action_head(void* c
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	struct SimpleActionHeadCtx* cc=(struct SimpleActionHeadCtx*)c;
	for(size_t i=0;i<cc->idx_num;i++)
		if(cc->idx[i]>=num)
			return NULL;
	FklNastNode* head=fklMakeNastNodeRef(cc->head);
	FklNastNode** exps=(FklNastNode**)malloc(sizeof(FklNastNode*)*(1+cc->idx_num));
	FKL_ASSERT(exps);
	exps[0]=head;
	for(size_t i=0;i<cc->idx_num;i++)
		exps[i+1]=fklMakeNastNodeRef(nodes[cc->idx[i]]);
	FklNastNode* retval=create_nast_list(exps,1+cc->idx_num,line);
	free(exps);
	return retval;
}

static void* simple_action_head_create(FklNastNode* rest[],size_t rest_len,int* failed)
{
	if(rest_len<2
			||rest[1]->type!=FKL_NAST_FIX
			||rest[1]->fix<0)
	{
		*failed=1;
		return NULL;
	}
	for(size_t i=1;i<rest_len;i++)
		if(rest[i]->type!=FKL_NAST_FIX||rest[i]->fix<0)
			return NULL;
	struct SimpleActionHeadCtx* ctx=(struct SimpleActionHeadCtx*)malloc(sizeof(struct SimpleActionHeadCtx)+(sizeof(uint64_t)*(rest_len-1)));
	FKL_ASSERT(ctx);
	ctx->head=fklMakeNastNodeRef(rest[0]);
	ctx->idx_num=rest_len-1;
	for(size_t i=1;i<rest_len;i++)
		ctx->idx[i-1]=rest[i]->fix;
	return ctx;
}

static void* simple_action_head_copy(const void* c)
{
	struct SimpleActionHeadCtx* cc=(struct SimpleActionHeadCtx*)c;
	size_t size=sizeof(struct SimpleActionHeadCtx)+(sizeof(uint64_t)*cc->idx_num);
	struct SimpleActionHeadCtx* ctx=(struct SimpleActionHeadCtx*)malloc(size);
	FKL_ASSERT(ctx);
	memcpy(ctx,cc,size);
	fklMakeNastNodeRef(ctx->head);
	return ctx;
}

static void simple_action_head_destroy(void* cc)
{
	struct SimpleActionHeadCtx* ctx=(struct SimpleActionHeadCtx*)cc;
	fklDestroyNastNode(ctx->head);
	free(ctx);
}

static inline void* simple_action_box(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	uintptr_t nth=(uintptr_t)ctx;
	if(nth>=num)
		return NULL;
	FklNastNode* box=fklCreateNastNode(FKL_NAST_BOX,line);
	box->box=fklMakeNastNodeRef(nodes[nth]);
	return box;
}

static inline void* simple_action_vector(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	uintptr_t nth=(uintptr_t)ctx;
	if(nth>=num)
		return NULL;
	FklNastNode* list=nodes[nth];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_VECTOR,line);
	size_t len=fklNastListLength(list);
	FklNastVector* vec=fklCreateNastVector(len);
	r->vec=vec;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
		vec->base[i]=fklMakeNastNodeRef(list->pair->car);
	return r;
}

static inline void* simple_action_hasheq(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	uintptr_t nth=(uintptr_t)ctx;
	if(nth>=num)
		return NULL;
	FklNastNode* list=nodes[nth];
	if(!fklIsNastNodeListAndHasSameType(list,FKL_NAST_PAIR))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQ,len);
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
	}
	return r;
}

static inline void* simple_action_hasheqv(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	uintptr_t nth=(uintptr_t)ctx;
	if(nth>=num)
		return NULL;
	FklNastNode* list=nodes[nth];
	if(!fklIsNastNodeListAndHasSameType(list,FKL_NAST_PAIR))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQV,len);
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
	}
	return r;
}

static inline void* simple_action_hashequal(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	uintptr_t nth=(uintptr_t)ctx;
	if(nth>=num)
		return NULL;
	FklNastNode* list=nodes[nth];
	if(!fklIsNastNodeListAndHasSameType(list,FKL_NAST_PAIR))
		return NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_HASHTABLE,line);
	size_t len=fklNastListLength(list);
	FklNastHashTable* ht=fklCreateNastHash(FKL_HASH_EQUAL,len);
	r->hash=ht;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		ht->items[i].car=fklMakeNastNodeRef(list->pair->car->pair->car);
		ht->items[i].cdr=fklMakeNastNodeRef(list->pair->car->pair->cdr);
	}
	return r;
}

static inline void* simple_action_bytevector(void* ctx
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	uintptr_t nth=(uintptr_t)ctx;
	if(nth>=num)
		return NULL;
	FklNastNode* list=nodes[nth];
	FklNastNode* r=fklCreateNastNode(FKL_NAST_BYTEVECTOR,line);
	size_t len=fklNastListLength(list);
	FklBytevector* bv=fklCreateBytevector(len,NULL);
	r->bvec=bv;
	size_t i=0;
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr,i++)
	{
		FklNastNode* cur=list->pair->car;
		if(cur->type==FKL_NAST_FIX)
			bv->ptr[i]=cur->fix>UINT8_MAX?UINT8_MAX:(cur->fix<0?0:cur->fix);
		else
			bv->ptr[i]=cur->bigInt->neg?0:UINT8_MAX;
	}
	return r;
}

struct SimpleActionSymbolCtx
{
	uint64_t nth;
	FklString* start;
	FklString* end;
};

static void* simple_action_symbol_create(FklNastNode* rest[],size_t rest_len,int* failed)
{
	FklString* start=NULL;
	FklString* end=NULL;
	if(rest_len==0
			||rest[0]->type!=FKL_NAST_FIX
			||rest[0]->fix<0)
	{
		*failed=1;
		return NULL;
	}
	if(rest_len==3)
	{
		if(rest[1]->type!=FKL_NAST_STR
				||rest[1]->str->size==0
				||rest[2]->type!=FKL_NAST_STR
				||rest[2]->str->size==0)
		{
			*failed=1;
			return NULL;
		}
		start=rest[1]->str;
		end=rest[2]->str;
	}
	else if(rest_len==2)
	{
		if(rest[1]->type!=FKL_NAST_STR
				||rest[1]->str->size==0)
		{
			*failed=1;
			return NULL;
		}
		start=rest[1]->str;
		end=start;
	}
	struct SimpleActionSymbolCtx* ctx=(struct SimpleActionSymbolCtx*)malloc(sizeof(struct SimpleActionSymbolCtx));
	FKL_ASSERT(ctx);
	ctx->nth=rest[0]->fix;
	ctx->start=start?fklCopyString(start):NULL;
	ctx->end=end?fklCopyString(end):NULL;
	return ctx;
}

static void* simple_action_symbol_copy(const void* cc)
{
	struct SimpleActionSymbolCtx* ctx=(struct SimpleActionSymbolCtx*)malloc(sizeof(struct SimpleActionSymbolCtx));
	FKL_ASSERT(ctx);
	*ctx=*(struct SimpleActionSymbolCtx*)cc;
	ctx->end=fklCopyString(ctx->end);
	ctx->start=fklCopyString(ctx->start);
	return ctx;
}

static void simple_action_symbol_destroy(void* cc)
{
	struct SimpleActionSymbolCtx* ctx=(struct SimpleActionSymbolCtx*)cc;
	free(ctx->end);
	free(ctx->start);
	free(ctx);
}

static void* simple_action_symbol(void* c
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	struct SimpleActionSymbolCtx* ctx=(struct SimpleActionSymbolCtx*)c;
	if(ctx->nth>=num)
		return NULL;
	FklNastNode* node=nodes[ctx->nth];
	if(node->type!=FKL_NAST_STR)
		return NULL;
	FklNastNode* sym=NULL;
	if(ctx->start)
	{
		const char* start=ctx->start->str;
		size_t start_size=ctx->start->size;
		const char* end=ctx->end->str;
		size_t end_size=ctx->end->size;

		const FklString* str=node->str;
		const char* cstr=str->str;
		size_t cstr_size=str->size;

		FklStringBuffer buffer;
		fklInitStringBuffer(&buffer);
		const char* end_cstr=cstr+str->size;
		while(cstr<end_cstr)
		{
			if(fklCharBufMatch(start,start_size,cstr,cstr_size))
			{
				cstr+=start_size;
				cstr_size-=start_size;
				size_t len=fklQuotedCharBufMatch(cstr,cstr_size,end,end_size);
				if(!len)
					return 0;
				size_t size=0;
				char* s=fklCastEscapeCharBuf(cstr,len-end_size,&size);
				fklStringBufferBincpy(&buffer,s,size);
				free(s);
				cstr+=len;
				cstr_size-=len;
				continue;
			}
			size_t len=0;
			for(;(cstr+len)<end_cstr;len++)
				if(fklCharBufMatch(start,start_size,cstr+len,cstr_size-len))
					break;
			fklStringBufferBincpy(&buffer,cstr,len);
			cstr+=len;
			cstr_size-=len;
		}
		FklSid_t id=fklAddSymbolCharBuf(buffer.buf,buffer.index,outerCtx)->id;
		sym=fklCreateNastNode(FKL_NAST_SYM,node->curline);
		sym->sym=id;
		fklUninitStringBuffer(&buffer);
	}
	else
	{
		FklNastNode* sym=fklCreateNastNode(FKL_NAST_SYM,node->curline);
		sym->sym=fklAddSymbol(node->str,outerCtx)->id;
	}
	return sym;
}

static inline void* simple_action_string(void* c
		,void* outerCtx
		,void* nodes[]
		,size_t num
		,size_t line)
{
	struct SimpleActionSymbolCtx* ctx=(struct SimpleActionSymbolCtx*)c;
	if(ctx->nth>=num)
		return NULL;
	FklNastNode* node=nodes[ctx->nth];
	if(node->type!=FKL_NAST_STR)
		return NULL;
	if(ctx->start)
	{
		size_t start_size=ctx->start->size;
		size_t end_size=ctx->end->size;

		const FklString* str=node->str;
		const char* cstr=str->str;

		size_t size=0;
		char* s=fklCastEscapeCharBuf(&cstr[start_size],str->size-end_size-start_size,&size);
		FklNastNode* retval=fklCreateNastNode(FKL_NAST_STR,node->curline);
		retval->str=fklCreateString(size,s);
		free(s);
		return retval;
	}
	else
		return fklMakeNastNodeRef(node);
}

static struct CstrIdCreatorProdAction
{
	const char* name;
	FklProdActionFunc func;
	void* (*creator)(FklNastNode* rest[],size_t rest_len,int* failed);
	void* (*ctx_copyer)(const void*);
	void (*ctx_destroy)(void*);
}CodegenProdCreatorActions[FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM]=
{
	{"nth",        simple_action_nth,        simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"cons",       simple_action_cons,       simple_action_cons_create,   simple_action_cons_copy,   fklProdCtxDestroyFree,        },
	{"head",       simple_action_head,       simple_action_head_create,   simple_action_head_copy,   simple_action_head_destroy,   },
	{"symbol",     simple_action_symbol,     simple_action_symbol_create, simple_action_symbol_copy, simple_action_symbol_destroy, },
	{"string",     simple_action_string,     simple_action_symbol_create, simple_action_symbol_copy, simple_action_symbol_destroy, },
	{"box",        simple_action_box,        simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"vector",     simple_action_vector,     simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"hasheq",     simple_action_hasheq,     simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"hasheqv",    simple_action_hasheqv,    simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"hashequal",  simple_action_hashequal,  simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"bytevector", simple_action_bytevector, simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
};

static inline void init_simple_prod_action_list(FklSid_t* simple_prod_action_id,FklSymbolTable* pst)
{
	for(size_t i=0;i<FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM;i++)
		simple_prod_action_id[i]=fklAddSymbolCstr(CodegenProdCreatorActions[i].name,pst)->id;
}

static inline struct CstrIdCreatorProdAction* find_simple_prod_action(FklSid_t id,FklSid_t* simple_prod_action_id)
{
	for(size_t i=0;i<FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM;i++)
	{
		if(simple_prod_action_id[i]==id)
			return &CodegenProdCreatorActions[i];
	}
	return NULL;
}

static inline FklGrammerSym* nast_vector_to_production_right_part(const FklNastVector* vec
		,FklHashTable* builtin_term
		,FklSymbolTable* tt
		,FklRegexTable* rt
		,size_t* num
		,int* failed)
{
	FklGrammerSym* retval=NULL;

	FklPtrStack valid_items;
	fklInitPtrStack(&valid_items,vec->size,8);

	uint8_t* delim=(uint8_t*)malloc(sizeof(uint8_t)*vec->size);
	FKL_ASSERT(delim);
	memset(delim,1,sizeof(uint8_t)*vec->size);

	for(size_t i=0;i<vec->size;i++)
	{
		const FklNastNode* cur=vec->base[i];
		if(cur->type==FKL_NAST_STR)
			fklPushPtrStack((void*)cur,&valid_items);
		else if(cur->type==FKL_NAST_SYM)
			fklPushPtrStack((void*)cur,&valid_items);
		else if(cur->type==FKL_NAST_BOX&&cur->box->type==FKL_NAST_STR)
			fklPushPtrStack((void*)cur,&valid_items);
		else if(cur->type==FKL_NAST_VECTOR
				&&cur->vec->size==1
				&&cur->vec->base[0]->type==FKL_NAST_STR)
			fklPushPtrStack((void*)cur,&valid_items);
		else if(cur->type==FKL_NAST_NIL)
		{
			delim[valid_items.top]=0;
			continue;
		}
		else if(cur->type==FKL_NAST_PAIR
				&&cur->pair->car->type==FKL_NAST_SYM
				&&cur->pair->cdr->type==FKL_NAST_SYM)
			fklPushPtrStack((void*)cur,&valid_items);
		else
		{
			*failed=1;
			goto end;
		}
	}
	if(valid_items.top)
	{
		size_t top=valid_items.top;
		retval=(FklGrammerSym*)malloc(sizeof(FklGrammerSym)*valid_items.top);
		FKL_ASSERT(retval);
		FklNastNode* const* base=(FklNastNode* const*)valid_items.base;
		for(size_t i=0;i<top;i++)
		{
			const FklNastNode* cur=base[i];
			FklGrammerSym* ss=&retval[i];
			ss->delim=delim[i];
			ss->end_with_terminal=0;
			if(cur->type==FKL_NAST_STR)
			{
				ss->isterm=1;
				ss->term_type=FKL_TERM_STRING;
				ss->nt.group=0;
				ss->nt.sid=fklAddSymbol(cur->str,tt)->id;
				ss->end_with_terminal=0;
			}
			else if(cur->type==FKL_NAST_PAIR)
			{
				ss->isterm=0;
				ss->term_type=FKL_TERM_STRING;
				ss->nt.group=cur->pair->car->sym;
				ss->nt.sid=cur->pair->cdr->sym;
			}
			else if(cur->type==FKL_NAST_BOX)
			{
				ss->isterm=1;
				ss->term_type=FKL_TERM_REGEX;
				const FklRegexCode* re=fklAddRegexStr(rt,cur->box->str);
				if(!re)
				{
					*failed=2;
					free(retval);
					retval=NULL;
					goto end;
				}
				ss->re=re;
			}
			else if(cur->type==FKL_NAST_VECTOR)
			{
				ss->isterm=1;
				ss->term_type=FKL_TERM_STRING;
				ss->nt.group=0;
				const FklString* str=cur->vec->base[0]->str;
				ss->nt.sid=fklAddSymbol(str,tt)->id;
				ss->end_with_terminal=1;
			}
			else
			{
				if(fklGetBuiltinMatch(builtin_term,cur->sym))
				{
					ss->isterm=1;
					ss->term_type=FKL_TERM_BUILTIN;
					ss->b.t=fklGetBuiltinMatch(builtin_term,cur->sym);
					ss->b.c=NULL;
				}
				else
				{
					ss->isterm=0;
					ss->term_type=FKL_TERM_STRING;
					ss->nt.group=0;
					ss->nt.sid=cur->sym;
				}
			}
		}
	}
	*num=valid_items.top;

end:
	fklUninitPtrStack(&valid_items);
	free(delim);
	return retval;
}

static inline FklGrammerIgnore* nast_vector_to_ignore(const FklNastVector* vec
		,FklHashTable* builtin_term
		,FklSymbolTable* tt
		,FklRegexTable* rt
		,int* failed)
{
	size_t num=0;
	FklGrammerSym* syms=nast_vector_to_production_right_part(vec
			,builtin_term
			,tt
			,rt
			,&num
			,failed);
	if(*failed)
		return NULL;
	FklGrammerIgnore* ig=fklGrammerSymbolsToIgnore(syms,num,tt);
	fklUninitGrammerSymbols(syms,num);
	free(syms);
	return ig;
}

FklGrammerIgnore* fklNastVectorToIgnore(FklNastNode* ast
		,FklSymbolTable* tt
		,FklRegexTable* rt
		,FklHashTable* builtin)
{
	int failed=0;
	return nast_vector_to_ignore(ast->vec,builtin,tt,rt,&failed);
}

FklGrammerProduction* fklCodegenProdPrintingToProduction(FklCodegenProdPrinting* p
		,FklSymbolTable* tt
		,FklRegexTable* rt
		,FklHashTable* builtin_terms
		,FklCodegenOuterCtx* outer_ctx
		,FklFuncPrototypes* pts
		,FklPtrStack* macroLibStack)
{
	const FklNastVector* vec=p->vec->vec;
	size_t len=0;
	int failed=0;
	FklGrammerSym* syms=nast_vector_to_production_right_part(vec
			,builtin_terms
			,tt,rt,&len,&failed);
	FklGrammerProduction* prod=fklCreateProduction(p->sid==0?0:p->group_id
			,p->sid
			,len
			,syms
			,NULL
			,NULL
			,NULL
			,fklProdCtxDestroyDoNothing
			,fklProdCtxCopyerDoNothing);

	if(p->type==FKL_CODEGEN_PROD_BUILTIN)
	{
		FklProdActionFunc action=find_builtin_prod_action(p->forth->sym,outer_ctx->builtin_prod_action_id);
		prod->func=action;
	}
	else if(p->type==FKL_CODEGEN_PROD_SIMPLE)
	{
		struct CstrIdCreatorProdAction* action=find_simple_prod_action(p->forth->vec->base[0]->sym
				,outer_ctx->simple_prod_action_id);
		void* ctx=action->creator(&p->forth->vec->base[1],p->forth->vec->size-1,&failed);
		prod->ctx=ctx;
		prod->func=action->func;
		prod->ctx_destroyer=action->ctx_destroy;
		prod->ctx_copyer=action->ctx_copyer;
	}
	else
	{
		struct CustomActionCtx* ctx=(struct CustomActionCtx*)calloc(1,sizeof(struct CustomActionCtx));
		FKL_ASSERT(ctx);
		ctx->codegen_outer_ctx=outer_ctx;
		ctx->prototype_id=p->prototype_id;
		ctx->pst=&outer_ctx->public_symbol_table;
		ctx->macroLibStack=macroLibStack;
		ctx->bcl=fklCopyByteCodelnt(p->bcl);
		ctx->pts=pts;
		prod->func=custom_action;
		prod->ctx=ctx;
		prod->ctx_destroyer=custom_action_ctx_destroy;
		prod->ctx_copyer=custom_action_ctx_copy;
	}

	return prod;
}

static inline FklCodegenInfo* macro_compile_prepare(FklCodegenInfo* codegen
		,FklCodegenEnv* curEnv
		,FklCodegenMacroScope* macroScope
		,FklHashTable* symbolSet
		,FklCodegenEnv** pmacroEnv
		,FklSymbolTable* pst)
{
	FklCodegenEnv* macroEnv=fklCreateCodegenEnv(curEnv,0,macroScope);

	for(FklHashTableItem* list=symbolSet->first
			;list
			;list=list->next)
	{
		FklSid_t* id=(FklSid_t*)list->data;
		fklAddCodegenDefBySid(*id,1,macroEnv);
	}
	FklCodegenInfo* macroCodegen=createCodegenInfo(codegen
			,NULL
			,macroEnv
			,0
			,1
			,codegen->outer_ctx);
	macroCodegen->builtinSymModiMark=fklGetBuiltinSymbolModifyMark(&macroCodegen->builtinSymbolNum);
	fklDestroyCodegenEnv(macroEnv->prev);
	macroEnv->prev=NULL;
	free(macroCodegen->dir);
	macroCodegen->dir=fklCopyCstr(codegen->dir);
	macroCodegen->filename=fklCopyCstr(codegen->filename);
	macroCodegen->realpath=fklCopyCstr(codegen->realpath);
	macroCodegen->pts=codegen->macro_pts;
	macroCodegen->macro_pts=fklCreateFuncPrototypes(0);

	macroCodegen->globalSymTable=pst;
	macroCodegen->fid=macroCodegen->filename?fklAddSymbolCstr(macroCodegen->filename,pst)->id:0;
	macroCodegen->libStack=macroCodegen->macroLibStack;
	macroCodegen->macroLibStack=fklCreatePtrStack(8,16);
	fklInitGlobCodegenEnv(macroEnv,pst);

	*pmacroEnv=macroEnv;
	return macroCodegen;
}

typedef struct
{
	FklGrammerProduction* prod;
	FklSid_t sid;
	FklSid_t group_id;
	FklNastNode* vec;
	uint8_t add_extra;
	uint8_t type;
	FklNastNode* action;
}AddingProductionCtx;

static void _adding_production_ctx_finalizer(void* data)
{
	AddingProductionCtx* ctx=data;
	if(ctx->prod)
		fklDestroyGrammerProduction(ctx->prod);
	fklDestroyNastNode(ctx->vec);
	if(ctx->type!=FKL_CODEGEN_PROD_CUSTOM)
		fklDestroyNastNode(ctx->action);
	free(data);
}

static const FklCodegenQuestContextMethodTable AddingProductionCtxMethodTable=
{
	.__finalizer=_adding_production_ctx_finalizer,
	.__get_bcl_stack=NULL,
	.__put_bcl=NULL,
};

static inline FklCodegenQuestContext* createAddingProductionCtx(FklGrammerProduction* prod
		,FklSid_t sid
		,FklSid_t group_id
		,uint8_t add_extra
		,FklNastNode* vec
		,FklCodegenProdActionType type
		,FklNastNode* action_ast)
{
	AddingProductionCtx* ctx=(AddingProductionCtx*)malloc(sizeof(AddingProductionCtx));
	FKL_ASSERT(ctx);
	ctx->prod=prod;
	ctx->sid=sid;
	ctx->group_id=group_id;
	ctx->add_extra=add_extra;
	ctx->vec=fklMakeNastNodeRef(vec);
	ctx->type=type;
	if(type!=FKL_CODEGEN_PROD_CUSTOM)
		ctx->action=fklMakeNastNodeRef(action_ast);
	return createCodegenQuestContext(ctx,&AddingProductionCtxMethodTable);
}

static inline int check_group_outer_ref(FklCodegenInfo* codegen
		,FklHashTable* productions
		,FklSid_t group_id)
{
	int is_ref_outer=0;
	for(FklHashTableItem* item=productions->first;item;item=item->next)
	{
		FklGrammerProdHashItem* data=(FklGrammerProdHashItem*)item->data;
		for(FklGrammerProduction* prods=data->prods;prods;prods=prods->next)
		{
			for(size_t i=0;i<prods->len;i++)
			{
				FklGrammerSym* sym=&prods->syms[i];
				if(!sym->isterm)
				{
					if(sym->nt.group)
						is_ref_outer|=sym->nt.group!=group_id;
					else
					{
						FklGrammerNonterm left={.group=group_id,.sid=sym->nt.sid};
						if(fklGetHashItem(&left,productions))
							sym->nt.group=group_id;
						else if(fklGetHashItem(&left,codegen->unnamed_prods))
							is_ref_outer=1;
					}
				}
			}
		}
	}
	return is_ref_outer;
}

FklGrammerProduction* fklCreateExtraStartProduction(FklSid_t group,FklSid_t sid)
{
	FklGrammerProduction* prod=fklCreateEmptyProduction(0,0,1,NULL,NULL,NULL
			,fklProdCtxDestroyDoNothing
			,fklProdCtxCopyerDoNothing);
	prod->func=codegen_prod_action_first;
	prod->idx=0;
	FklGrammerSym* u=&prod->syms[0];
	u->delim=1;
	u->term_type=FKL_TERM_STRING;
	u->isterm=0;
	u->nt.group=group;
	u->nt.sid=sid;
	return prod;
}

static inline FklCodegenProdPrinting* create_codegen_prod_printing(FklSid_t group
		,FklSid_t sid
		,FklNastNode* vec
		,uint8_t type
		,uint8_t add_extra
		,FklNastNode* forth
		,uint32_t prototype_id
		,FklByteCodelnt* bcl)
{
	FklCodegenProdPrinting* p=(FklCodegenProdPrinting*)malloc(sizeof(FklCodegenProdPrinting));
	FKL_ASSERT(p);
	p->group_id=group;
	p->sid=sid;
	p->vec=fklMakeNastNodeRef(vec);
	p->type=type;
	p->add_extra=add_extra;
	if(p->type!=FKL_CODEGEN_PROD_CUSTOM)
		p->forth=fklMakeNastNodeRef(forth);
	else
	{
		p->prototype_id=prototype_id;
		p->bcl=fklCopyByteCodelnt(bcl);
	}
	return p;
}

static inline void destroy_codegen_prod_printing(FklCodegenProdPrinting* p)
{
	fklDestroyNastNode(p->vec);
	if(p->type==FKL_CODEGEN_PROD_CUSTOM)
		fklDestroyByteCodelnt(p->bcl);
	else
		fklDestroyNastNode(p->forth);
	free(p);
}

BC_PROCESS(process_adding_production)
{
	AddingProductionCtx* ctx=context->data;
	FklSid_t group_id=ctx->group_id;
	FklGrammerProduction* prod=ctx->prod;
	ctx->prod=NULL;
	FklGrammer* g=*(codegen->g);
	FklSid_t sid=ctx->sid;
	if(group_id==0)
	{
		if(fklAddProdToProdTableNoRepeat(codegen->unnamed_prods,&g->builtins,prod))
		{
			fklDestroyGrammerProduction(prod);
reader_macro_error:
			errorState->type=FKL_ERR_GRAMMER_CREATE_FAILED;
			errorState->line=line;
			return NULL;
		}
		if(ctx->add_extra)
		{
			FklGrammerProduction* extra_prod=fklCreateExtraStartProduction(group_id,sid);
			if(fklAddProdToProdTable(codegen->unnamed_prods,&g->builtins,extra_prod))
			{
				fklDestroyGrammerProduction(extra_prod);
				goto reader_macro_error;
			}
		}
	}
	else
	{
		FklGrammerProductionGroup* group=add_production_group(codegen->named_prod_groups,group_id);
		if(fklAddProdToProdTableNoRepeat(&group->prods,&g->builtins,prod))
		{
			fklDestroyGrammerProduction(prod);
			goto reader_macro_error;
		}
		if(ctx->add_extra)
		{
			FklGrammerProduction* extra_prod=fklCreateExtraStartProduction(group_id,sid);
			if(fklAddProdToProdTable(&group->prods,&g->builtins,extra_prod))
			{
				fklDestroyGrammerProduction(extra_prod);
				goto reader_macro_error;
			}
		}
		group->is_ref_outer=check_group_outer_ref(codegen,&group->prods,group_id);
		uint32_t prototype_id=0;
		FklByteCodelnt* bcl=NULL;
		if(ctx->type==FKL_CODEGEN_PROD_CUSTOM)
		{
			struct CustomActionCtx* ctx=prod->ctx;
			prototype_id=ctx->prototype_id;
			bcl=ctx->bcl;
		}
		FklCodegenProdPrinting* p=create_codegen_prod_printing(group_id
				,sid
				,ctx->vec
				,ctx->type
				,ctx->add_extra
				,ctx->action
				,prototype_id
				,bcl);
		fklPushPtrStack(p,&group->prod_printing);
	}
	if(codegen->export_named_prod_groups
			&&fklGetHashItem(&group_id,codegen->export_named_prod_groups)
			&&((FklGrammerProductionGroup*)fklGetHashItem(&group_id,codegen->named_prod_groups))->is_ref_outer)
	{
		errorState->type=FKL_ERR_EXPORT_OUTER_REF_PROD_GROUP;
		errorState->line=line;
		errorState->place=NULL;
		return NULL;
	}
	return NULL;
}

static inline FklGrammerProduction* nast_vector_to_production(FklNastNode* vec_node
		,uint64_t line
		,uint8_t add_extra
		,FklHashTable* builtin_term
		,FklSymbolTable* tt
		,FklRegexTable* rt
		,FklSid_t left_group
		,FklSid_t sid
		,FklSid_t action_type
		,FklNastNode* action_ast
		,FklSid_t group_id
		,FklCodegenInfo* codegen
		,FklCodegenEnv* curEnv
		,FklCodegenMacroScope* macroScope
		,FklPtrStack* codegenQuestStack
		,FklSymbolTable* pst
		,int* failed)
{
	size_t len=0;
	const FklNastVector* vec=vec_node->vec;
	FklGrammerSym* syms=nast_vector_to_production_right_part(vec
			,builtin_term
			,tt
			,rt
			,&len
			,failed);
	if(*failed)
		return NULL;
	if(action_type==codegen->outer_ctx->builtInPatternVar_builtin)
	{
		if(action_ast->type!=FKL_NAST_SYM)
			return NULL;
		FklProdActionFunc action=find_builtin_prod_action(action_ast->sym,codegen->outer_ctx->builtin_prod_action_id);
		if(!action)
			return NULL;
		FklGrammerProduction* prod=fklCreateProduction(left_group
				,sid
				,len
				,syms
				,NULL
				,action
				,NULL
				,fklProdCtxDestroyDoNothing
				,fklProdCtxCopyerDoNothing);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(process_adding_production
				,createAddingProductionCtx(prod
					,sid
					,group_id
					,add_extra
					,vec_node
					,FKL_CODEGEN_PROD_BUILTIN
					,action_ast)
				,NULL
				,1
				,macroScope
				,curEnv
				,line
				,codegen
				,codegenQuestStack);
		return prod;
	}
	else if(action_type==codegen->outer_ctx->builtInPatternVar_simple)
	{
		if(action_ast->type!=FKL_NAST_VECTOR
				||!action_ast->vec->size
				||action_ast->vec->base[0]->type!=FKL_NAST_SYM)
			return NULL;
		struct CstrIdCreatorProdAction* action=find_simple_prod_action(action_ast->vec->base[0]->sym,codegen->outer_ctx->simple_prod_action_id);
		if(!action)
			return NULL;
		int failed=0;
		void* ctx=action->creator(&action_ast->vec->base[1],action_ast->vec->size-1,&failed);
		if(failed)
			return NULL;
		FklGrammerProduction* prod=fklCreateProduction(left_group
				,sid
				,len
				,syms
				,NULL
				,action->func
				,ctx
				,action->ctx_destroy
				,action->ctx_copyer);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(process_adding_production
				,createAddingProductionCtx(prod
					,sid
					,group_id
					,add_extra
					,vec_node
					,FKL_CODEGEN_PROD_SIMPLE
					,action_ast)
				,NULL
				,1
				,macroScope
				,curEnv
				,line
				,codegen
				,codegenQuestStack);
		return prod;
	}
	else if(action_type==codegen->outer_ctx->builtInPatternVar_custom)
	{
		FklHashTable symbolSet;
		fklInitSidSet(&symbolSet);
		fklPutHashItem(&fklAddSymbolCstr("$$",pst)->id,&symbolSet);
		FklCodegenEnv* macroEnv=NULL;
		FklCodegenInfo* macroCodegen=macro_compile_prepare(codegen
				,curEnv
				,macroScope
				,&symbolSet
				,&macroEnv
				,pst);
		fklUninitHashTable(&symbolSet);

		create_and_insert_to_pool(macroCodegen->pts
				,0
				,macroEnv
				,macroCodegen->globalSymTable
				,0
				,macroCodegen->fid
				,action_ast->curline
				,pst);
		FklPtrQueue* queue=fklCreatePtrQueue();
		struct CustomActionCtx* ctx=(struct CustomActionCtx*)calloc(1,sizeof(struct CustomActionCtx));
		FKL_ASSERT(ctx);
		ctx->codegen_outer_ctx=codegen->outer_ctx;
		ctx->prototype_id=macroEnv->prototypeId;
		ctx->pst=pst;
		ctx->macroLibStack=codegen->macroLibStack;

		FklGrammerProduction* prod=fklCreateProduction(left_group
				,sid
				,len
				,syms
				,NULL
				,custom_action
				,ctx
				,custom_action_ctx_destroy
				,custom_action_ctx_copy);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(process_adding_production
				,createAddingProductionCtx(prod
					,sid
					,group_id
					,add_extra
					,vec_node
					,FKL_CODEGEN_PROD_CUSTOM
					,NULL)
				,NULL
				,1
				,macroScope
				,curEnv
				,line
				,codegen
				,codegenQuestStack);
		fklPushPtrQueue(fklMakeNastNodeRef(action_ast),queue);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_reader_macro_bc_process
				,createReaderMacroQuestContext(ctx,macroCodegen->pts)
				,createMustHasRetvalQueueNextExpression(queue)
				,1
				,macroEnv->macros
				,macroEnv
				,action_ast->curline
				,macroCodegen
				,codegenQuestStack);
		return prod;
	}
	return NULL;
}

static inline int add_group_prods(FklGrammer* g,const FklHashTable* prods)
{
	for(FklHashTableItem* i=prods->first;i;i=i->next)
	{
		FklGrammerProdHashItem* item=(FklGrammerProdHashItem*)i->data;
		for(FklGrammerProduction* prods=item->prods;prods;prods=prods->next)
		{
			FklGrammerProduction* prod=fklCopyUninitedGrammerProduction(prods);
			if(prod->left.sid==0&&prod->left.group==0)
				prod->left=g->start;
			if(fklAddProdAndExtraToGrammer(g,prod))
			{
				fklDestroyGrammerProduction(prod);
				return 1;
			}
		}
	}
	return 0;
}

static inline int add_group_ignores(FklGrammer* g,FklGrammerIgnore* igs)
{
	for(;igs;igs=igs->next)
	{
		FklGrammerIgnore* ig=(FklGrammerIgnore*)malloc(sizeof(FklGrammerIgnore)+igs->len*sizeof(FklGrammerIgnoreSym));
		FKL_ASSERT(ig);
		ig->len=igs->len;
		ig->next=NULL;
		memcpy(ig->ig,igs->ig,sizeof(FklGrammerIgnoreSym)*ig->len);

		if(fklAddIgnoreToIgnoreList(&g->ignores,ig))
		{
			free(ig);
			return 1;
		}
	}
	return 0;
}

static inline int add_start_prods_to_grammer(FklGrammer* g,FklHashTable* prods)
{
	for(FklHashTableItem* itemlist=prods->first;itemlist;itemlist=itemlist->next)
	{
		FklGrammerProdHashItem* prod_item=(FklGrammerProdHashItem*)itemlist->data;
		for(FklGrammerProduction* prods=prod_item->prods;prods;prods=prods->next)
		{
			if(prods->left.group==0&&prods->left.sid==0)
			{
				FklGrammerProduction* prod=fklCopyUninitedGrammerProduction(prods);
				prod->left=g->start;
				if(fklAddProdAndExtraToGrammer(g,prod))
				{
					fklDestroyGrammerProduction(prod);
					return 1;
				}
			}
		}
	}
	return 0;
}

static inline int add_all_ignores_to_grammer(FklCodegenInfo* codegen)
{
	FklGrammer* g=*codegen->g;

	if(add_group_ignores(g,*codegen->unnamed_ignores))
		return 1;

	for(FklHashTableItem* group_items=codegen->named_prod_groups->first
			;group_items
			;group_items=group_items->next)
	{
		FklGrammerProductionGroup* group=(FklGrammerProductionGroup*)group_items->data;
		if(add_group_ignores(g,group->ignore))
			return 1;
	}
	return 0;
}

static inline int add_all_start_prods_to_grammer(FklCodegenInfo* codegen)
{
	FklGrammer* g=*codegen->g;
	if(add_start_prods_to_grammer(g,codegen->unnamed_prods))
		return 1;

	for(FklHashTableItem* group_items=codegen->named_prod_groups->first
			;group_items
			;group_items=group_items->next)
	{
		FklGrammerProductionGroup* group=(FklGrammerProductionGroup*)group_items->data;
		if(add_start_prods_to_grammer(g,&group->prods))
			return 1;
	}

	return 0;
}

static inline int add_prods_list_to_grammer(FklGrammer* g,FklGrammerProduction* prods)
{
	for(;prods;prods=prods->next)
	{
		FklGrammerProduction* prod=fklCopyUninitedGrammerProduction(prods);
		if(fklAddProdAndExtraToGrammer(g,prod))
		{
			fklDestroyGrammerProduction(prod);
			return 1;
		}
	}
	return 0;
}

static inline int scan_and_add_reachable_productions(FklCodegenInfo* codegen)
{
	FklGrammer* g=*codegen->g;
	FklHashTable* productions=&g->productions;
	for(FklHashTableItem* nonterm_item=productions->first;nonterm_item;nonterm_item=nonterm_item->next)
	{
		FklGrammerProdHashItem* nonterm=(FklGrammerProdHashItem*)nonterm_item->data;
		for(FklGrammerProduction* prods=nonterm->prods;prods;prods=prods->next)
		{
			size_t len=prods->len;
			FklGrammerSym* syms=prods->syms;
			for(size_t i=0;i<len;i++)
			{
				FklGrammerSym* cur=&syms[i];
				if(!cur->isterm&&!fklIsNonterminalExist(&g->productions,cur->nt.group,cur->nt.sid))
				{
					if(cur->nt.group)
					{
						FklGrammerProductionGroup* group=fklGetHashItem(&cur->nt.group,codegen->named_prod_groups);
						if(!group)
							return 1;
						FklGrammerProduction* prods=fklGetProductions(&group->prods,cur->nt.group,cur->nt.sid);
						if(!prods)
							return 1;
						if(add_prods_list_to_grammer(g,prods))
							return 1;
					}
					else
					{
						FklGrammerProduction* prods=fklGetProductions(codegen->unnamed_prods,cur->nt.group,cur->nt.sid);
						if(!prods)
							return 1;
						if(add_prods_list_to_grammer(g,prods))
							return 1;

					}
				}
			}
		}
	}
	return 0;
}

static inline int add_all_group_to_grammer(FklCodegenInfo* codegen)
{
	if(add_all_ignores_to_grammer(codegen))
		return 1;

	if(add_all_start_prods_to_grammer(codegen))
		return 1;

	if(scan_and_add_reachable_productions(codegen))
		return 1;

	FklGrammer* g=*codegen->g;

	if(fklCheckAndInitGrammerSymbols(g))
		return 1;

	FklHashTable* itemSet=fklGenerateLr0Items(g);
	fklLr0ToLalrItems(itemSet,g);
	int r=fklGenerateLalrAnalyzeTable(g,itemSet);
	fklDestroyHashTable(itemSet);

	return r;
}

static void grammer_production_group_set_val(void* k0,const void* k1)
{
	*(FklGrammerProductionGroup*)k0=*(const FklGrammerProductionGroup*)k1;
}

static inline void destroy_grammer_ignore_list(FklGrammerIgnore* ig)
{
	while(ig)
	{
		FklGrammerIgnore* next=ig->next;
		fklDestroyIgnore(ig);
		ig=next;
	}
}

static void grammer_production_group_uninit(void* k0)
{
	FklGrammerProductionGroup* g=(FklGrammerProductionGroup*)k0;
	fklUninitHashTable(&g->prods);
	destroy_grammer_ignore_list(g->ignore);
	while(!fklIsPtrStackEmpty(&g->ignore_printing))
		fklDestroyNastNode(fklPopPtrStack(&g->ignore_printing));
	fklUninitPtrStack(&g->ignore_printing);
	while(!fklIsPtrStackEmpty(&g->prod_printing))
		destroy_codegen_prod_printing(fklPopPtrStack(&g->prod_printing));
	fklUninitPtrStack(&g->prod_printing);
}

static const FklHashTableMetaTable GrammerProductionMetaTable=
{
	.size=sizeof(FklGrammerProductionGroup),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=fklSetSidKey,
	.__keyEqual=fklSidKeyEqual,
	.__hashFunc=fklSidHashFunc,
	.__uninitItem=grammer_production_group_uninit,
	.__setVal=grammer_production_group_set_val,
};

static inline FklGrammerProductionGroup* get_production_group(const FklHashTable* ht,FklSid_t group)
{
	return fklGetHashItem(&group,ht);
}

static inline FklGrammerProductionGroup* add_production_group(FklHashTable* named_prod_groups,FklSid_t group_id)
{
	FklGrammerProductionGroup group_init={.id=group_id,.ignore=NULL,.is_ref_outer=0,.prods.t=NULL};
	FklGrammerProductionGroup* group=fklGetOrPutHashItem(&group_init,named_prod_groups);
	if(!group->prods.t)
	{
		fklInitGrammerProductionTable(&group->prods);
		fklInitPtrStack(&group->prod_printing,8,16);
		fklInitPtrStack(&group->ignore_printing,8,16);
	}
	return group;
}

void fklInitCodegenProdGroupTable(FklHashTable* ht)
{
	fklInitHashTable(ht,&GrammerProductionMetaTable);
}

static inline int init_builtin_grammer(FklCodegenInfo* codegen,FklSymbolTable* st)
{
	*codegen->g=fklCreateEmptyGrammer(st);
	FklGrammer* g=*codegen->g;
	fklInitGrammerProductionTable(codegen->builtin_prods);
	fklInitGrammerProductionTable(codegen->unnamed_prods);
	fklInitCodegenProdGroupTable(codegen->named_prod_groups);
	codegen->self_unnamed_ignores=NULL;
	*codegen->unnamed_ignores=NULL;
	*codegen->builtin_ignores=fklInitBuiltinProductionSet(codegen->builtin_prods
			,st
			,&g->terminals
			,&g->regexes
			,&g->builtins);

	g->ignores=NULL;

	fklClearGrammer(g);
	if(add_group_ignores(g,*codegen->builtin_ignores))
		return 1;

	if(add_group_prods(g,codegen->builtin_prods))
		return 1;
	return 0;
}

static inline int process_add_production(FklSid_t group_id
		,FklCodegenInfo* codegen
		,FklNastNode* vector_node
		,FklCodegenErrorState* errorState
		,FklCodegenEnv* curEnv
		,FklCodegenMacroScope* macroScope
		,FklPtrStack* codegenQuestStack
		,FklSymbolTable* pst)
{
	FklGrammer* g=*codegen->g;
	FklNastVector* vec=vector_node->vec;
	if(vec->size==1)
	{
		if(vec->base[0]->type!=FKL_NAST_VECTOR||vec->base[0]->vec->size==0)
			goto reader_macro_error;
		int failed=0;
		FklGrammerIgnore* ignore=nast_vector_to_ignore(vec->base[0]->vec
				,&g->builtins
				,&g->terminals
				,&g->regexes
				,&failed);
		if(!ignore)
		{
			if(failed==2)
			{
regex_compile_error:
				errorState->type=FKL_ERR_REGEX_COMPILE_FAILED;
				errorState->place=fklMakeNastNodeRef(vector_node);
				return 1;
			}
reader_macro_error:
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(vector_node);
			return 1;
		}
		if(group_id==0)
		{
			if(fklAddIgnoreToIgnoreList(codegen->unnamed_ignores,ignore))
			{
				fklDestroyIgnore(ignore);
				goto reader_macro_error;
			}
		}
		else
		{
			FklGrammerProductionGroup* group=add_production_group(codegen->named_prod_groups,group_id);
			if(fklAddIgnoreToIgnoreList(&group->ignore,ignore))
			{
				fklDestroyIgnore(ignore);
				goto reader_macro_error;
			}
			fklPushPtrStack(fklMakeNastNodeRef(vec->base[0]),&group->ignore_printing);
		}
	}
	else if(vec->size==4)
	{
		FklNastNode** base=vec->base;
		if(base[2]->type!=FKL_NAST_SYM)
			goto reader_macro_error;
		if(base[0]->type==FKL_NAST_NIL
				&&base[1]->type==FKL_NAST_VECTOR)
		{
			int failed=0;
			FklGrammerProduction* prod=nast_vector_to_production(base[1]
					,vector_node->curline
					,0
					,&g->builtins
					,&g->terminals
					,&g->regexes
					,0
					,0
					,base[2]->sym
					,base[3]
					,group_id
					,codegen
					,curEnv
					,macroScope
					,codegenQuestStack
					,pst
					,&failed);
			if(failed==2)
				goto regex_compile_error;
			if(!prod)
				goto reader_macro_error;
		}
		else if(base[0]->type==FKL_NAST_SYM
				&&base[1]->type==FKL_NAST_VECTOR)
		{
			FklNastNode* vect=base[1];
			FklSid_t sid=base[0]->sym;
			if(group_id==0&&(fklGetHashItem(&sid,&g->builtins)
						||fklIsNonterminalExist(codegen->builtin_prods,group_id,sid)))
				goto reader_macro_error;
			int failed=0;
			FklGrammerProduction* prod=nast_vector_to_production(vect
					,vector_node->curline
					,1
					,&g->builtins
					,&g->terminals
					,&g->regexes
					,group_id
					,sid
					,base[2]->sym
					,base[3]
					,group_id
					,codegen
					,curEnv
					,macroScope
					,codegenQuestStack
					,pst
					,&failed);
			if(failed==2)
				goto regex_compile_error;
			if(!prod)
				goto reader_macro_error;
		}
		else if(base[0]->type==FKL_NAST_VECTOR
				&&base[1]->type==FKL_NAST_SYM)
		{
			FklNastNode* vect=base[0];
			FklSid_t sid=base[1]->sym;
			if(group_id==0&&(fklGetHashItem(&sid,&g->builtins)
						||fklIsNonterminalExist(codegen->builtin_prods,group_id,sid)))
				goto reader_macro_error;
			int failed=0;
			FklGrammerProduction* prod=nast_vector_to_production(vect
					,vector_node->curline
					,0
					,&g->builtins
					,&g->terminals
					,&g->regexes
					,group_id
					,sid
					,base[2]->sym
					,base[3]
					,group_id
					,codegen
					,curEnv
					,macroScope
					,codegenQuestStack
					,pst
					,&failed);
			if(failed==2)
				goto regex_compile_error;
			if(!prod)
				goto reader_macro_error;
		}
		else
			goto reader_macro_error;
	}
	else
		goto reader_macro_error;
	return 0;
}

BC_PROCESS(update_grammer)
{
	if(add_all_group_to_grammer(codegen))
	{
		errorState->type=FKL_ERR_GRAMMER_CREATE_FAILED;
		errorState->line=line;
		return NULL;
	}
	return NULL;
}

static CODEGEN_FUNC(codegen_defmacro)
{
	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	FklNastNode* name=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_value,ht);
	if(name->type==FKL_NAST_SYM)
		fklAddReplacementBySid(name->sym,value,macroScope->replacements);
	else if(name->type==FKL_NAST_PAIR)
	{
		FklHashTable* symbolSet=NULL;
		FklNastNode* pattern=fklCreatePatternFromNast(name,&symbolSet);
		if(!pattern)
		{
			errorState->type=FKL_ERR_INVALID_MACRO_PATTERN;
			errorState->place=fklMakeNastNodeRef(name);
			return;
		}
		if(fklGetHashItem(&outer_ctx->builtInPatternVar_orig,symbolSet))
		{
			fklDestroyNastNode(pattern);
			errorState->type=FKL_ERR_INVALID_MACRO_PATTERN;
			errorState->place=fklMakeNastNodeRef(name);
			return;
		}
		fklPutHashItem(&outer_ctx->builtInPatternVar_orig,symbolSet);

		FklCodegenEnv* macroEnv=NULL;
		FklCodegenInfo* macroCodegen=macro_compile_prepare(codegen,curEnv,macroScope,symbolSet,&macroEnv,pst);
		fklDestroyHashTable(symbolSet);

		create_and_insert_to_pool(macroCodegen->pts
				,0
				,macroEnv
				,macroCodegen->globalSymTable
				,0
				,macroCodegen->fid
				,value->curline
				,pst);
		FklPtrQueue* queue=fklCreatePtrQueue();
		fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_compiler_macro_bc_process
				,createMacroQuestContext(name,pattern,macroScope,macroEnv->prototypeId)
				,createMustHasRetvalQueueNextExpression(queue)
				,1
				,macroEnv->macros
				,macroEnv
				,value->curline
				,macroCodegen
				,codegenQuestStack);
	}
	else if(name->type==FKL_NAST_VECTOR)
	{
		if(value->type!=FKL_NAST_NIL&&value->type!=FKL_NAST_SYM)
		{
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(origExp);
			return;
		}
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(update_grammer
				,createEmptyContext()
				,NULL
				,scope
				,macroScope
				,curEnv
				,origExp->curline
				,codegen
				,codegenQuestStack);
		FklSid_t group_id=value->type==FKL_NAST_NIL?0:value->sym;
		FklSymbolTable* st=pst;
		FklGrammer* g=*codegen->g;
		if(!g)
		{
			if(init_builtin_grammer(codegen,st))
				goto reader_macro_error;
			g=*codegen->g;
		}
		else
		{
			fklClearGrammer(g);
			if(add_group_ignores(g,*codegen->builtin_ignores))
				goto reader_macro_error;

			if(add_group_prods(g,codegen->builtin_prods))
				goto reader_macro_error;
		}
		if(process_add_production(group_id
					,codegen
					,name
					,errorState
					,curEnv
					,macroScope
					,codegenQuestStack
					,pst))
			return;
	}
	else
	{
reader_macro_error:
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
}

static CODEGEN_FUNC(codegen_def_reader_macros)
{
	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	FklPtrQueue prod_vector_queue;
	fklInitPtrQueue(&prod_vector_queue);
	FklNastNode* arg=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_arg0,ht);
	fklPushPtrQueue(arg,&prod_vector_queue);
	arg=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_arg1,ht);
	fklPushPtrQueue(arg,&prod_vector_queue);
	FklNastNode* rest=fklPatternMatchingHashTableRef(outer_ctx->builtInPatternVar_rest,ht);
	for(;rest->pair->cdr->type!=FKL_NAST_NIL;rest=rest->pair->cdr)
		fklPushPtrQueue(rest->pair->car,&prod_vector_queue);
	FklNastNode* group_node=rest->pair->car;
	if(group_node->type!=FKL_NAST_NIL&&group_node->type!=FKL_NAST_SYM)
	{
		fklUninitPtrQueue(&prod_vector_queue);
reader_macro_error:
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(update_grammer
			,createEmptyContext()
			,NULL
			,scope
			,macroScope
			,curEnv
			,origExp->curline
			,codegen
			,codegenQuestStack);

	FklSid_t group_id=group_node->type==FKL_NAST_SYM?group_node->sym:0;
	FklSymbolTable* st=pst;
	FklGrammer* g=*codegen->g;
	if(!g)
	{
		if(init_builtin_grammer(codegen,st))
			goto reader_macro_error;
		g=*codegen->g;
	}
	else
	{
		fklClearGrammer(g);
		if(add_group_ignores(g,*codegen->builtin_ignores))
			goto reader_macro_error;

		if(add_group_prods(g,codegen->builtin_prods))
			goto reader_macro_error;
	}
	for(FklQueueNode* first=prod_vector_queue.head;first;first=first->next)
		if(process_add_production(group_id
					,codegen
					,first->data
					,errorState
					,curEnv
					,macroScope
					,codegenQuestStack
					,pst))
		{
			fklUninitPtrQueue(&prod_vector_queue);
			return;
		}
	fklUninitPtrQueue(&prod_vector_queue);
}

typedef void (*FklCodegenFunc)(CODEGEN_ARGS);

#undef BC_PROCESS
#undef GET_STACK
#undef CODEGEN_ARGS
#undef CODEGEN_FUNC

static inline FklInstruction createPushFix(int64_t i)
{
	if(i==0)
		return create_op_ins(FKL_OP_PUSH_0);
	else if(i==1)
		return create_op_ins(FKL_OP_PUSH_1);
	else if(i>=INT8_MIN&&i<=INT8_MAX)
		return fklCreatePushI8Ins(i);
	else if(i>=INT16_MIN&&i<=INT16_MAX)
		return fklCreatePushI16Ins(i);
	else if(i>=INT32_MIN&&i<=INT32_MAX)
		return fklCreatePushI32Ins(i);
	else
		return fklCreatePushI64Ins(i);
}

FklByteCode* fklCodegenNode(const FklNastNode* node,FklCodegenInfo* codegenr)
{
	FklPtrStack stack=FKL_STACK_INIT;
	fklInitPtrStack(&stack,32,16);
	fklPushPtrStack((void*)node,&stack);
	FklByteCode* retval=fklCreateByteCode(0);
	FklSymbolTable* pst=&codegenr->outer_ctx->public_symbol_table;
	while(!fklIsPtrStackEmpty(&stack))
	{
		FklNastNode* node=fklPopPtrStack(&stack);
		FklInstruction tmp;
		switch(node->type)
		{
			case FKL_NAST_SYM:
				tmp=fklCreatePushSidIns(fklAddSymbol(fklGetSymbolWithId(node->sym,pst)->symbol
							,codegenr->globalSymTable)->id);
				break;
			case FKL_NAST_NIL:
				tmp=create_op_ins(FKL_OP_PUSH_NIL);
				break;
			case FKL_NAST_CHR:
				tmp=fklCreatePushCharIns(node->chr);
				break;
			case FKL_NAST_FIX:
				tmp=createPushFix(node->fix);
				break;
			case FKL_NAST_F64:
				tmp=fklCreatePushF64Ins(node->f64);
				break;
			case FKL_NAST_BYTEVECTOR:
				tmp=fklCreatePushBvecIns(node->bvec);
				break;
			case FKL_NAST_STR:
				tmp=fklCreatePushStrIns(node->str);
				break;
			case FKL_NAST_BIG_INT:
				tmp=fklCreatePushBigIntIns(node->bigInt);
				break;
			case FKL_NAST_PAIR:
				{
					size_t i=1;
					FklNastNode* cur=node;
					for(;cur->type==FKL_NAST_PAIR;cur=cur->pair->cdr)
					{
						fklPushPtrStack(cur->pair->car,&stack);
						i++;
					}
					fklPushPtrStack(cur,&stack);
					if(i==2)
						tmp=create_op_ins(FKL_OP_PUSH_PAIR);
					else
						tmp=create_op_imm_u64_ins(FKL_OP_PUSH_LIST,i);
				}
				break;
			case FKL_NAST_BOX:
				tmp=create_op_ins(FKL_OP_PUSH_BOX);
				fklPushPtrStack(node->box,&stack);
				break;
			case FKL_NAST_VECTOR:
				tmp=create_op_imm_u64_ins(FKL_OP_PUSH_VECTOR,node->vec->size);
				for(size_t i=0;i<node->vec->size;i++)
					fklPushPtrStack(node->vec->base[i],&stack);
				break;
			case FKL_NAST_HASHTABLE:
				tmp=create_op_imm_u64_ins(FKL_OP_PUSH_HASHTABLE_EQ+node->hash->type,node->hash->num);
				for(size_t i=0;i<node->hash->num;i++)
				{
					fklPushPtrStack(node->hash->items[i].car,&stack);
					fklPushPtrStack(node->hash->items[i].cdr,&stack);
				}
				break;
			default:
				abort();
				break;
		}
		fklByteCodeInsertFront(tmp,retval);
	}
	fklUninitPtrStack(&stack);
	return retval;
}

static int matchAndCall(FklCodegenFunc func
		,const FklNastNode* pattern
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklNastNode* exp
		,FklPtrStack* codegenQuestStack
		,FklCodegenEnv* env
		,FklCodegenInfo* codegenr
		,FklCodegenErrorState* errorState)
{
	FklHashTable* ht=fklCreatePatternMatchingHashTable();
	int r=fklPatternMatch(pattern,exp,ht);
	if(r)
	{
		fklChdir(codegenr->dir);
		func(exp
				,ht
				,codegenQuestStack
				,scope
				,macroScope
				,env
				,codegenr
				,codegenr->outer_ctx
				,errorState);
		fklChdir(codegenr->outer_ctx->cwd);
	}
	fklDestroyHashTable(ht);
	return r;
}

static struct PatternAndFunc
{
	const char* ps;
	FklCodegenFunc func;
}builtin_pattern_cstr_func[]=
{
	{"~(begin,~rest)",                                         codegen_begin,             },
	{"~(local,~rest)",                                         codegen_local,             },
	{"~(let [],~rest)",                                        codegen_let0,              },
	{"~(let [(~name ~value),~args],~rest)",                    codegen_let1,              },
	{"~(let* [],~rest)",                                       codegen_let0,              },
	{"~(let* [(~name ~value)],~rest)",                         codegen_let1,              },
	{"~(let* [(~name ~value),~args],~rest)",                   codegen_let81,             },
	{"~(let ~arg0 [],~rest)",                                  codegen_named_let0,        },
	{"~(let ~arg0 [(~name ~value),~args],~rest)",              codegen_named_let1,        },
	{"~(letrec [],~rest)",                                     codegen_let0,              },
	{"~(letrec [(~name ~value),~args],~rest)",                 codegen_letrec,            },

	{"~(do [] [~cond ~value],~rest)",                          codegen_do0,               },
	{"~(do [] [~cond],~rest)",                                 codegen_do0,               },

	{"~(do [(~name ~arg0),~args] [~cond ~value],~rest)",       codegen_do1,               },
	{"~(do [(~name ~arg0),~args] [~cond],~rest)",              codegen_do1,               },

	{"~(do [(~name ~arg0 ~arg1),~args] [~cond ~value],~rest)", codegen_do1,               },
	{"~(do [(~name ~arg0 ~arg1),~args] [~cond],~rest)",        codegen_do1,               },

	{"~(define (~name,~args),~rest)",                          codegen_defun,             },
	{"~(define ~name ~value)",                                 codegen_define,            },
	{"~(setq ~name ~value)",                                   codegen_setq,              },
	{"~(check ~value)",                                        codegen_check,             },
	{"~(quote ~value)",                                        codegen_quote,             },
	{"`(unquote `value)",                                      codegen_unquote,           },
	{"~(qsquote ~value)",                                      codegen_qsquote,           },
	{"~(lambda ~args,~rest)",                                  codegen_lambda,            },
	{"~(and,~rest)",                                           codegen_and,               },
	{"~(or,~rest)",                                            codegen_or,                },
	{"~(cond,~rest)",                                          codegen_cond,              },
	{"~(if ~value ~rest)",                                     codegen_if0,               },
	{"~(if ~value ~rest ~args)",                               codegen_if1,               },
	{"~(when ~value,~rest)",                                   codegen_when,              },
	{"~(unless ~value,~rest)",                                 codegen_unless,            },
	{"~(load ~name,~rest)",                                    codegen_load,              },
	{"~(import (prefix ~name ~rest),~args)",                   codegen_import_prefix,     },
	{"~(import (only ~name,~rest),~args)",                     codegen_import_only,       },
	{"~(import (alias ~name,~rest),~args)",                    codegen_import_alias,      },
	{"~(import (except ~name,~rest),~args)",                   codegen_import_except,     },
	{"~(import (common ~name),~args)",                         codegen_import_common,     },
	{"~(import ~name,~args)",                                  codegen_import,            },
	{"~(macroexpand ~value)",                                  codegen_macroexpand,       },
	{"~(defmacro ~name ~value)",                               codegen_defmacro,          },
	{"~(import)",                                              codegen_import_none,       },
	{"~(export ~value)",                                       codegen_export_single,     },
	{"~(export,~rest)",                                        codegen_export,            },
	{"~(defmacro ~arg0 ~arg1,~rest)",                          codegen_def_reader_macros, },
	{"~(check ~cond ~value,~rest)",                            codegen_cond_compile,      },
	{NULL,                                                     NULL,                      },
};

void fklInitCodegenOuterCtxExceptPattern(FklCodegenOuterCtx* outerCtx)
{
	FklSymbolTable* publicSymbolTable=&outerCtx->public_symbol_table;
	fklInitSymbolTable(publicSymbolTable);

	outerCtx->builtInPatternVar_orig=fklAddSymbolCstr("orig",publicSymbolTable)->id;
	outerCtx->builtInPatternVar_rest=fklAddSymbolCstr("rest",publicSymbolTable)->id;
	outerCtx->builtInPatternVar_name=fklAddSymbolCstr("name",publicSymbolTable)->id;
	outerCtx->builtInPatternVar_cond=fklAddSymbolCstr("cond",publicSymbolTable)->id;
	outerCtx->builtInPatternVar_args=fklAddSymbolCstr("args",publicSymbolTable)->id;
	outerCtx->builtInPatternVar_arg0=fklAddSymbolCstr("arg0",publicSymbolTable)->id;
	outerCtx->builtInPatternVar_arg1=fklAddSymbolCstr("arg1",publicSymbolTable)->id;
	outerCtx->builtInPatternVar_value=fklAddSymbolCstr("value",publicSymbolTable)->id;

	outerCtx->builtInPatternVar_custom=fklAddSymbolCstr("custom",publicSymbolTable)->id;
	outerCtx->builtInPatternVar_builtin=fklAddSymbolCstr("builtin",publicSymbolTable)->id;
	outerCtx->builtInPatternVar_simple=fklAddSymbolCstr("simple",publicSymbolTable)->id;

	init_builtin_prod_action_list(outerCtx->builtin_prod_action_id,publicSymbolTable);
	init_simple_prod_action_list(outerCtx->simple_prod_action_id,publicSymbolTable);
}

void fklInitCodegenOuterCtx(FklCodegenOuterCtx* outerCtx,char* main_file_real_path_dir)
{
	outerCtx->cwd=fklSysgetcwd();
	outerCtx->main_file_real_path_dir=main_file_real_path_dir?main_file_real_path_dir:fklCopyCstr(outerCtx->cwd);
	fklInitCodegenOuterCtxExceptPattern(outerCtx);
	FklSymbolTable* publicSymbolTable=&outerCtx->public_symbol_table;

	FklNastNode** builtin_pattern_node=outerCtx->builtin_pattern_node;
	for(size_t i=0;i<FKL_CODEGEN_PATTERN_NUM;i++)
	{
		FklNastNode* node=fklCreateNastNodeFromCstr(builtin_pattern_cstr_func[i].ps,publicSymbolTable);
		builtin_pattern_node[i]=fklCreatePatternFromNast(node,NULL);
		fklDestroyNastNode(node);
	}

	for(size_t i=0;i<FKL_BUILTIN_REPLACEMENT_NUM;i++)
		outerCtx->builtin_replacement_id[i]=fklAddSymbolCstr(builtInSymbolReplacement[i].s,publicSymbolTable)->id;

	FklNastNode** builtin_sub_pattern_node=outerCtx->builtin_sub_pattern_node;
	for(size_t i=0;i<FKL_CODEGEN_SUB_PATTERN_NUM;i++)
	{
		FklNastNode* node=fklCreateNastNodeFromCstr(builtInSubPattern[i],publicSymbolTable);
		builtin_sub_pattern_node[i]=fklCreatePatternFromNast(node,NULL);
		fklDestroyNastNode(node);
	}

}

void fklSetCodegenOuterCtxMainFileRealPathDir(FklCodegenOuterCtx* outer_ctx,char* dir)
{
	if(outer_ctx->main_file_real_path_dir)
		free(outer_ctx->main_file_real_path_dir);
	outer_ctx->main_file_real_path_dir=dir;
}

void fklUninitCodegenOuterCtx(FklCodegenOuterCtx* outer_ctx)
{
	fklUninitSymbolTable(&outer_ctx->public_symbol_table);
	FklNastNode** nodes=outer_ctx->builtin_pattern_node;
	for(size_t i=0;i<FKL_CODEGEN_PATTERN_NUM;i++)
		fklDestroyNastNode(nodes[i]);
	nodes=outer_ctx->builtin_sub_pattern_node;
	for(size_t i=0;i<FKL_CODEGEN_SUB_PATTERN_NUM;i++)
		fklDestroyNastNode(nodes[i]);
	free(outer_ctx->cwd);
	free(outer_ctx->main_file_real_path_dir);
}

void fklDestroyCodegenInfo(FklCodegenInfo* codegen)
{
	while(codegen&&codegen->destroyAbleMark)
	{
		codegen->refcount-=1;
		FklCodegenInfo* prev=codegen->prev;
		if(!codegen->refcount)
		{
			fklUninitCodegenInfo(codegen);
			free(codegen);
			codegen=prev;
		}
		else
			break;
	}
}

static inline int mapAllBuiltInPattern(FklNastNode* curExp
		,FklPtrStack* codegenQuestStack
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklCodegenEnv* curEnv
		,FklCodegenInfo* codegenr
		,FklCodegenErrorState* errorState)
{
	FklNastNode* const* builtin_pattern_node=codegenr->outer_ctx->builtin_pattern_node;

	if(fklIsNastNodeList(curExp))
		for(size_t i=0;i<FKL_CODEGEN_PATTERN_NUM;i++)
			if(matchAndCall(builtin_pattern_cstr_func[i].func
						,builtin_pattern_node[i]
						,scope
						,macroScope
						,curExp
						,codegenQuestStack
						,curEnv,codegenr,errorState))
				return 0;

	if(curExp->type==FKL_NAST_PAIR)
	{
		codegen_funcall(curExp
				,codegenQuestStack
				,scope
				,macroScope
				,curEnv
				,codegenr
				,errorState);
		return 0;
	}
	return 1;
}

static FklByteCodelnt* makeGetLocBc(uint32_t idx,FklSid_t fid,uint64_t curline)
{
	FklInstruction bc=create_op_imm_u32_ins(FKL_OP_GET_LOC,idx);
	return fklCreateSingleInsBclnt(bc,fid,curline);
}

static FklByteCodelnt* makeGetVarRefBc(uint32_t idx,FklSid_t fid,uint64_t curline)
{
	FklInstruction bc=create_op_imm_u32_ins(FKL_OP_GET_VAR_REF,idx);
	return fklCreateSingleInsBclnt(bc,fid,curline);
}

FklByteCodelnt* fklGenExpressionCodeWithQuest(FklCodegenQuest* initialQuest,FklCodegenInfo* codegener)
{
	FklCodegenOuterCtx* outer_ctx=codegener->outer_ctx;
	FklSymbolTable* pst=&outer_ctx->public_symbol_table;
	FklPtrStack resultStack=FKL_STACK_INIT;
	fklInitPtrStack(&resultStack,1,8);
	FklCodegenErrorState errorState={0,NULL,0};
	FklPtrStack codegenQuestStack=FKL_STACK_INIT;
	fklInitPtrStack(&codegenQuestStack,32,16);
	fklPushPtrStack(initialQuest,&codegenQuestStack);
	while(!fklIsPtrStackEmpty(&codegenQuestStack))
	{
		FklCodegenQuest* curCodegenQuest=fklTopPtrStack(&codegenQuestStack);
		FklCodegenInfo* curCodegen=curCodegenQuest->codegen;
		FklCodegenNextExpression* nextExpression=curCodegenQuest->nextExpression;
		FklCodegenEnv* curEnv=curCodegenQuest->env;
		FklCodegenQuestContext* curContext=curCodegenQuest->context;
		int r=1;
		if(nextExpression)
		{
			FklNastNode* (*get_next_expression)(void*,FklCodegenErrorState*)=nextExpression->t->getNextExpression;
			uint8_t must_has_retval=nextExpression->must_has_retval;

			for(FklNastNode* curExp=get_next_expression(nextExpression->context,&errorState)
					;curExp
					;curExp=get_next_expression(nextExpression->context,&errorState))
			{
skip:
				if(curExp->type==FKL_NAST_PAIR)
				{
					FklNastNode* orig_cur_exp=fklMakeNastNodeRef(curExp);
					curExp=fklTryExpandCodegenMacro(orig_cur_exp
							,curCodegen
							,curCodegenQuest->macroScope
							,&errorState);
					if(errorState.type)
					{
						fklDestroyNastNode(orig_cur_exp);
						break;
					}
					if(must_has_retval
							&&isNonRetvalExp(curExp
								,curCodegen->outer_ctx->builtin_pattern_node))
					{
						fklDestroyNastNode(curExp);
						errorState.type=FKL_ERR_SYNTAXERROR;
						errorState.place=orig_cur_exp;
						break;
					}
					if(must_has_retval==FIRST_MUST_HAS_RETVAL)
					{
						must_has_retval=DO_NOT_NEED_RETVAL;
						nextExpression->must_has_retval=DO_NOT_NEED_RETVAL;
					}
					fklDestroyNastNode(orig_cur_exp);
				}
				if(curExp->type==FKL_NAST_SYM)
				{
					FklNastNode* replacement=NULL;
					ReplacementFunc f=NULL;
					for(FklCodegenMacroScope* cs=curCodegenQuest->macroScope;cs&&!replacement;cs=cs->prev)
						replacement=fklGetReplacement(curExp->sym,cs->replacements);
					if(replacement)
					{
						fklDestroyNastNode(curExp);
						curExp=replacement;
						goto skip;
					}
					else if((f=findBuiltInReplacementWithId(curExp->sym,outer_ctx->builtin_replacement_id)))
					{
						FklNastNode* t=f(curExp,curEnv,curCodegen);
						fklDestroyNastNode(curExp);
						curExp=t;
						goto skip;
					}
					else
					{
						FklByteCodelnt* bcl=NULL;
						FklSymbolDef* def=fklFindSymbolDefByIdAndScope(curExp->sym,curCodegenQuest->scope,curEnv);
						if(def)
							bcl=makeGetLocBc(def->idx,curCodegen->fid,curExp->curline);
						else
						{
							uint32_t idx=fklAddCodegenRefBySid(curExp->sym,curEnv,curCodegen->fid,curExp->curline);
							bcl=makeGetVarRefBc(idx,curCodegen->fid,curExp->curline);
						}
						curContext->t->__put_bcl(curContext->data,bcl);
						fklDestroyNastNode(curExp);
						continue;
					}
				}
				r=mapAllBuiltInPattern(curExp
						,&codegenQuestStack
						,curCodegenQuest->scope
						,curCodegenQuest->macroScope
						,curEnv
						,curCodegen
						,&errorState);
				if(r)
				{
					curContext->t->__put_bcl(curContext->data
							,create_bc_lnt(fklCodegenNode(curExp,curCodegen)
								,curCodegen->fid
								,curExp->curline));
				}
				fklDestroyNastNode(curExp);
				if((!r&&fklTopPtrStack(&codegenQuestStack)!=curCodegenQuest)
						||errorState.type)
					break;
			}
		}
		if(errorState.type)
		{
print_error:
			fklPrintCodegenError(errorState.place
					,errorState.type
					,curCodegen
					,curCodegen->globalSymTable
					,errorState.line
					,pst);
			while(!fklIsPtrStackEmpty(&codegenQuestStack))
			{
				FklCodegenQuest* curCodegenQuest=fklPopPtrStack(&codegenQuestStack);
				destroyCodegenQuest(curCodegenQuest);
			}
			fklUninitPtrStack(&codegenQuestStack);
			fklUninitPtrStack(&resultStack);
			return NULL;
		}
		FklCodegenQuest* otherCodegenQuest=fklTopPtrStack(&codegenQuestStack);
		if(otherCodegenQuest==curCodegenQuest)
		{
			fklPopPtrStack(&codegenQuestStack);
			FklCodegenQuest* prevQuest=curCodegenQuest->prev?curCodegenQuest->prev:fklTopPtrStack(&codegenQuestStack);
			FklByteCodelnt* resultBcl=curCodegenQuest->processer(curCodegenQuest->codegen
					,curEnv
					,curCodegenQuest->scope
					,curCodegenQuest->macroScope
					,curContext
					,curCodegenQuest->codegen->fid
					,curCodegenQuest->curline
					,&errorState
					,outer_ctx);
			if(resultBcl)
			{
				if(fklIsPtrStackEmpty(&codegenQuestStack))
					fklPushPtrStack(resultBcl,&resultStack);
				else
				{
					FklCodegenQuestContext* prevContext=prevQuest->context;
					prevContext->t->__put_bcl(prevContext->data,resultBcl);
				}
			}
			if(errorState.type)
			{
				fklPushPtrStack(curCodegenQuest,&codegenQuestStack);
				goto print_error;
			}
			destroyCodegenQuest(curCodegenQuest);
		}
	}
	FklByteCodelnt* retval=NULL;
	if(!fklIsPtrStackEmpty(&resultStack))
		retval=fklPopPtrStack(&resultStack);
	fklUninitPtrStack(&resultStack);
	fklUninitPtrStack(&codegenQuestStack);
	if(retval)
	{
		FklInstruction ret=create_op_ins(FKL_OP_RET);
		fklBytecodeLntPushFrontIns(retval,&ret,codegener->fid,codegener->curline);
	}
	return retval;
}

FklByteCodelnt* fklGenExpressionCodeWithFp(FILE* fp
		,FklCodegenInfo* codegen)
{
	FklCodegenQuest* initialQuest=createCodegenQuest(_begin_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createFpNextExpression(fp,codegen)
			,1
			,codegen->globalEnv->macros
			,codegen->globalEnv
			,1
			,NULL
			,codegen);
	return fklGenExpressionCodeWithQuest(initialQuest,codegen);
}

FklByteCodelnt* fklGenExpressionCode(FklNastNode* exp
		,FklCodegenEnv* globalEnv
		,FklCodegenInfo* codegenr)
{
	FklPtrQueue* queue=fklCreatePtrQueue();
	fklPushPtrQueue(exp,queue);
	FklCodegenQuest* initialQuest=createCodegenQuest(_default_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createDefaultQueueNextExpression(queue)
			,1
			,globalEnv->macros
			,globalEnv
			,exp->curline
			,NULL
			,codegenr);
	return fklGenExpressionCodeWithQuest(initialQuest,codegenr);
}

static inline void init_codegen_grammer_ptr(FklCodegenInfo* codegen)
{
	codegen->self_g=NULL;
	codegen->g=&codegen->self_g;
	codegen->named_prod_groups=&codegen->self_named_prod_groups;
	codegen->unnamed_prods=&codegen->self_unnamed_prods;
	codegen->unnamed_ignores=&codegen->self_unnamed_ignores;
	codegen->builtin_prods=&codegen->self_builtin_prods;
	codegen->builtin_ignores=&codegen->self_builtin_ignores;
}

void fklInitGlobalCodegenInfo(FklCodegenInfo* codegen
		,const char* rp
		,FklSymbolTable* globalSymTable
		,int destroyAbleMark
		,FklCodegenOuterCtx* outer_ctx)
{
	codegen->globalSymTable=globalSymTable;
	codegen->outer_ctx=outer_ctx;
	if(rp!=NULL)
	{
		codegen->dir=fklGetDir(rp);
		codegen->filename=fklRelpath(outer_ctx->main_file_real_path_dir,rp);
		codegen->realpath=fklCopyCstr(rp);
		codegen->fid=fklAddSymbolCstr(codegen->filename,globalSymTable)->id;
	}
	else
	{
		codegen->dir=fklSysgetcwd();
		codegen->filename=NULL;
		codegen->realpath=NULL;
		codegen->fid=0;
	}
	codegen->libMark=1;
	codegen->macroMark=0;
	codegen->curline=1;
	codegen->prev=NULL;
	codegen->globalEnv=fklCreateCodegenEnv(NULL,0,NULL);
	fklInitGlobCodegenEnv(codegen->globalEnv,&outer_ctx->public_symbol_table);
	codegen->globalEnv->refcount+=1;
	codegen->destroyAbleMark=destroyAbleMark;
	codegen->refcount=0;


	codegen->libStack=fklCreatePtrStack(8,8);
	codegen->macroLibStack=fklCreatePtrStack(8,8);
	codegen->pts=fklCreateFuncPrototypes(0);
	codegen->macro_pts=fklCreateFuncPrototypes(0);
	codegen->builtinSymModiMark=fklGetBuiltinSymbolModifyMark(&codegen->builtinSymbolNum);
	codegen->outer_ctx=outer_ctx;

	fklInitExportSidIdxTable(&codegen->exports);
	codegen->export_replacement=fklCreateCodegenReplacementTable();
	codegen->export_named_prod_groups=fklCreateSidSet();

	init_codegen_grammer_ptr(codegen);

	create_and_insert_to_pool(codegen->pts
			,0
			,codegen->globalEnv
			,codegen->globalSymTable
			,0
			,codegen->fid
			,1
			,&outer_ctx->public_symbol_table);
}

void fklInitCodegenInfo(FklCodegenInfo* codegen
		,const char* filename
		,FklCodegenEnv* env
		,FklCodegenInfo* prev
		,FklSymbolTable* globalSymTable
		,int destroyAbleMark
		,int libMark
		,int macroMark
		,FklCodegenOuterCtx* outer_ctx)
{
	if(filename!=NULL)
	{
		char* rp=fklRealpath(filename);
		codegen->dir=fklGetDir(rp);
		codegen->filename=fklRelpath(outer_ctx->main_file_real_path_dir,rp);
		codegen->realpath=rp;
		codegen->fid=fklAddSymbolCstr(codegen->filename,globalSymTable)->id;
	}
	else
	{
		codegen->dir=fklSysgetcwd();
		codegen->filename=NULL;
		codegen->realpath=NULL;
		codegen->fid=0;
	}
	codegen->curline=1;
	codegen->prev=prev;
	prev->refcount+=1;
	codegen->globalEnv=env;
	env->refcount+=1;
	codegen->globalSymTable=globalSymTable;
	codegen->destroyAbleMark=destroyAbleMark;
	codegen->refcount=0;
	codegen->exports.t=NULL;
	codegen->libMark=libMark;
	codegen->macroMark=macroMark;

	codegen->export_macro=NULL;
	codegen->export_replacement=libMark?fklCreateCodegenReplacementTable():NULL;
	codegen->export_named_prod_groups=libMark?fklCreateSidSet():NULL;
	codegen->exports.t=NULL;
	if(libMark)
		fklInitExportSidIdxTable(&codegen->exports);

	init_codegen_grammer_ptr(codegen);
	if(prev)
	{
		codegen->libStack=prev->libStack;
		codegen->macroLibStack=prev->macroLibStack;
		codegen->pts=prev->pts;
		codegen->macro_pts=prev->macro_pts;
		codegen->builtinSymModiMark=prev->builtinSymModiMark;
		codegen->builtinSymbolNum=prev->builtinSymbolNum;
	}
	else
	{
		codegen->libStack=fklCreatePtrStack(8,8);
		codegen->macroLibStack=fklCreatePtrStack(8,8);
		codegen->pts=fklCreateFuncPrototypes(0);
		codegen->macro_pts=fklCreateFuncPrototypes(0);
		codegen->builtinSymModiMark=fklGetBuiltinSymbolModifyMark(&codegen->builtinSymbolNum);
	}
	codegen->outer_ctx=outer_ctx;
}

void fklUninitCodegenInfo(FklCodegenInfo* codegen)
{
	fklDestroyCodegenEnv(codegen->globalEnv);

	if(!codegen->destroyAbleMark||codegen->macroMark)
	{
		fklDestroyFuncPrototypes(codegen->macro_pts);
		FklPtrStack* macroLibStack=codegen->macroLibStack;
		while(!fklIsPtrStackEmpty(macroLibStack))
			fklDestroyCodegenLib(fklPopPtrStack(macroLibStack));
		fklDestroyPtrStack(macroLibStack);
		free(codegen->builtinSymModiMark);
	}

	if(!codegen->destroyAbleMark)
	{
		if(codegen->globalSymTable&&codegen->globalSymTable!=&codegen->outer_ctx->public_symbol_table)
			fklDestroySymbolTable(codegen->globalSymTable);
		while(!fklIsPtrStackEmpty(codegen->libStack))
			fklDestroyCodegenLib(fklPopPtrStack(codegen->libStack));
		fklDestroyPtrStack(codegen->libStack);

		if(codegen->pts)
			fklDestroyFuncPrototypes(codegen->pts);
	}
	free(codegen->dir);
	if(codegen->filename)
		free(codegen->filename);
	if(codegen->realpath)
		free(codegen->realpath);
	if(codegen->exports.t)
		fklUninitHashTable(&codegen->exports);
	for(FklCodegenMacro* cur=codegen->export_macro;cur;)
	{
		FklCodegenMacro* t=cur;
		cur=cur->next;
		fklDestroyCodegenMacro(t);
	}
	if(codegen->export_named_prod_groups)
		fklDestroyHashTable(codegen->export_named_prod_groups);
	if(codegen->export_replacement)
		fklDestroyHashTable(codegen->export_replacement);
	if(codegen->g==&codegen->self_g&&*codegen->g)
	{
		FklGrammer* g=*codegen->g;
		fklClearGrammer(g);
		fklUninitRegexTable(&g->regexes);
		fklUninitSymbolTable(&g->terminals);
		fklUninitSymbolTable(&g->reachable_terminals);
		fklUninitHashTable(&g->builtins);
		fklUninitHashTable(&g->firstSets);
		fklUninitHashTable(&g->productions);
		destroy_grammer_ignore_list(*codegen->builtin_ignores);
		destroy_grammer_ignore_list(*codegen->unnamed_ignores);
		fklUninitHashTable(codegen->builtin_prods);
		fklUninitHashTable(codegen->unnamed_prods);
		fklUninitHashTable(codegen->named_prod_groups);
		free(g);
		codegen->g=NULL;
	}
}

static inline void recompute_all_terminal_sid(FklGrammerProductionGroup* group
		,FklSymbolTable* target_table
		,FklSymbolTable* origin_table
		,FklRegexTable* target_rt
		,FklRegexTable* orig_rt)
{
	for(FklGrammerIgnore* igs=group->ignore;igs;igs=igs->next)
		recompute_ignores_terminal_sid_and_regex(igs,target_table,target_rt,orig_rt);
	for(FklHashTableItem* prod_hash_item_list=group->prods.first
			;prod_hash_item_list
			;prod_hash_item_list=prod_hash_item_list->next)
	{
		FklGrammerProdHashItem* prod_item=(FklGrammerProdHashItem*)prod_hash_item_list->data;
		for(FklGrammerProduction* prods=prod_item->prods;prods;prods=prods->next)
			recompute_prod_terminal_sid(prods,target_table,origin_table);
	}
}

void fklInitCodegenScriptLib(FklCodegenLib* lib
		,FklCodegenInfo* codegen
		,FklByteCodelnt* bcl
		,FklCodegenEnv* env)
{
	lib->type=FKL_CODEGEN_LIB_SCRIPT;
	lib->bcl=bcl;
	lib->named_prod_groups.t=NULL;
	lib->terminal_table.ht.t=NULL;
	lib->exports.t=NULL;
	if(codegen)
	{

		lib->rp=codegen->realpath;

		lib->head=codegen->export_macro;
		lib->replacements=codegen->export_replacement;
		if(codegen->export_named_prod_groups&&codegen->export_named_prod_groups->num)
		{
			fklInitSymbolTable(&lib->terminal_table);
			fklInitRegexTable(&lib->regexes);
			FklGrammer* g=*codegen->g;
			fklInitCodegenProdGroupTable(&lib->named_prod_groups);
			for(FklHashTableItem* sid_list=codegen->export_named_prod_groups->first
					;sid_list
					;sid_list=sid_list->next)
			{
				FklSid_t id=*(FklSid_t*)sid_list->data;
				FklGrammerProductionGroup* group=fklGetHashItem(&id,codegen->named_prod_groups);
				FklGrammerProductionGroup* target_group=add_production_group(&lib->named_prod_groups,id);
				for(FklGrammerIgnore* igs=group->ignore;igs;igs=igs->next)
				{
					FklGrammerIgnore* ig=(FklGrammerIgnore*)malloc(sizeof(FklGrammerIgnore)+igs->len*sizeof(FklGrammerIgnoreSym));
					FKL_ASSERT(ig);
					ig->len=igs->len;
					ig->next=NULL;
					memcpy(ig->ig,igs->ig,sizeof(FklGrammerIgnoreSym)*ig->len);
					fklAddIgnoreToIgnoreList(&target_group->ignore,ig);
				}
				uint32_t top=group->ignore_printing.top;
				void** base=group->ignore_printing.base;
				for(uint32_t i=0;i<top;i++)
					fklPushPtrStack(base[i],&target_group->ignore_printing);
				group->ignore_printing.top=0;

				top=group->prod_printing.top;
				base=group->prod_printing.base;
				for(uint32_t i=0;i<top;i++)
					fklPushPtrStack(base[i],&target_group->prod_printing);
				group->prod_printing.top=0;

				for(FklHashTableItem* prod_hash_item_list=group->prods.first
						;prod_hash_item_list
						;prod_hash_item_list=prod_hash_item_list->next)
				{
					FklGrammerProdHashItem* prod_item=(FklGrammerProdHashItem*)prod_hash_item_list->data;
					for(FklGrammerProduction* prods=prod_item->prods;prods;prods=prods->next)
					{
						FklGrammerProduction* prod=fklCopyUninitedGrammerProduction(prods);
						fklAddProdToProdTable(&target_group->prods,&g->builtins,prod);
					}
				}
				recompute_all_terminal_sid(target_group
						,&lib->terminal_table
						,&g->terminals
						,&lib->regexes
						,&g->regexes);
			}
		}
	}
	else
	{
		lib->rp=NULL;
		lib->head=NULL;
		lib->replacements=NULL;
	}
	if(env)
	{
		FklHashTable* exports_index=&lib->exports;
		fklInitExportSidIdxTable(exports_index);
		FklHashTable* export_sid_set=&codegen->exports;
		lib->prototypeId=env->prototypeId;
		for(const FklHashTableItem* sid_idx_list=export_sid_set->first
				;sid_idx_list
				;sid_idx_list=sid_idx_list->next)
		{
			const FklCodegenExportSidIndexHashItem* sid_idx_item=(const FklCodegenExportSidIndexHashItem*)sid_idx_list->data;
			fklGetOrPutHashItem((void*)sid_idx_item,exports_index);
		}
	}
	else
		lib->prototypeId=0;
}

FklCodegenLib* fklCreateCodegenScriptLib(FklCodegenInfo* codegen
		,FklByteCodelnt* bcl
		,FklCodegenEnv* env)
{
	FklCodegenLib* lib=(FklCodegenLib*)malloc(sizeof(FklCodegenLib));
	FKL_ASSERT(lib);
	fklInitCodegenScriptLib(lib
			,codegen
			,bcl
			,env);
	return lib;
}

FklCodegenLib* fklCreateCodegenDllLib(char* rp
		,uv_lib_t dll
		,FklSymbolTable* table
		,FklCodegenDllLibInitExportFunc init
		,FklSymbolTable* pst)
{
	FklCodegenLib* lib=(FklCodegenLib*)malloc(sizeof(FklCodegenLib));
	FKL_ASSERT(lib);
	fklInitCodegenDllLib(lib,rp,dll,table,init,pst);
	return lib;
}

void fklInitCodegenDllLib(FklCodegenLib* lib
		,char* rp
		,uv_lib_t dll
		,FklSymbolTable* table
		,FklCodegenDllLibInitExportFunc init
		,FklSymbolTable* pst)
{
	lib->rp=rp;
	lib->type=FKL_CODEGEN_LIB_DLL;
	lib->dll=dll;
	lib->head=NULL;
	lib->replacements=NULL;
	lib->exports.t=NULL;
	lib->named_prod_groups.t=NULL;

	uint32_t num=0;
	FklSid_t* exports=NULL;
	init(&num,&exports,pst);
	FklHashTable* exports_idx=&lib->exports;
	fklInitExportSidIdxTable(exports_idx);
	if(num)
	{
		for(uint32_t i=0;i<num;i++)
		{
			FklCodegenExportSidIndexHashItem* item=fklPutHashItem(&exports[i],exports_idx);
			item->idx=i;
		}
	}
	if(exports)
		free(exports);
}

void fklDestroyCodegenMacroList(FklCodegenMacro* cur)
{
	while(cur)
	{
		FklCodegenMacro* t=cur;
		cur=cur->next;
		fklDestroyCodegenMacro(t);
	}
}

void fklDestroyCodegenLibMacroScope(FklCodegenLib* lib)
{
	fklDestroyCodegenMacroList(lib->head);
	if(lib->replacements)
		fklDestroyHashTable(lib->replacements);
}

void fklUninitCodegenLibInfo(FklCodegenLib* lib)
{
	if(lib->exports.t)
		fklUninitHashTable(&lib->exports);
	free(lib->rp);
	if(lib->named_prod_groups.t)
	{
		fklUninitHashTable(&lib->named_prod_groups);
		fklUninitSymbolTable(&lib->terminal_table);
		fklUninitRegexTable(&lib->regexes);
	}
}

void fklUninitCodegenLib(FklCodegenLib* lib)
{
	switch(lib->type)
	{
		case FKL_CODEGEN_LIB_SCRIPT:
			fklDestroyByteCodelnt(lib->bcl);
			break;
		case FKL_CODEGEN_LIB_DLL:
			uv_dlclose(&lib->dll);
			break;
	}
	fklUninitCodegenLibInfo(lib);
	fklDestroyCodegenLibMacroScope(lib);
}

void fklDestroyCodegenLib(FklCodegenLib* lib)
{
	fklUninitCodegenLib(lib);
	free(lib);
}

FklCodegenMacro* fklCreateCodegenMacro(FklNastNode* pattern
		,FklNastNode* origin_exp
		,FklByteCodelnt* bcl
		,FklCodegenMacro* next
		,uint32_t prototype_id
		,int own)
{
	FklCodegenMacro* r=(FklCodegenMacro*)malloc(sizeof(FklCodegenMacro));
	FKL_ASSERT(r);
	r->pattern=pattern;
	r->origin_exp=origin_exp;
	r->bcl=bcl;
	r->next=next;
	r->prototype_id=prototype_id;
	r->own=own;
	return r;
}

void fklDestroyCodegenMacro(FklCodegenMacro* macro)
{
	uninit_codegen_macro(macro);
	free(macro);
}

FklCodegenMacroScope* fklCreateCodegenMacroScope(FklCodegenMacroScope* prev)
{
	FklCodegenMacroScope* r=(FklCodegenMacroScope*)malloc(sizeof(FklCodegenMacroScope));
	FKL_ASSERT(r);
	r->head=NULL;
	r->replacements=fklCreateCodegenReplacementTable();
	r->refcount=0;
	r->prev=make_macro_scope_ref(prev);
	return r;
}

void fklDestroyCodegenMacroScope(FklCodegenMacroScope* macros)
{
	while(macros)
	{
		macros->refcount--;
		if(macros->refcount)
			return;
		else
		{
			FklCodegenMacroScope* prev=macros->prev;
			for(FklCodegenMacro* cur=macros->head;cur;)
			{
				FklCodegenMacro* t=cur;
				cur=cur->next;
				fklDestroyCodegenMacro(t);
			}
			if(macros->replacements)
				fklDestroyHashTable(macros->replacements);
			free(macros);
			macros=prev;
		}
	}
}

static FklCodegenMacro* findMacro(FklNastNode* exp
		,FklCodegenMacroScope* macros
		,FklHashTable** pht)
{
	if(!exp)
		return NULL;
	FklCodegenMacro* r=NULL;
	for(;!r&&macros;macros=macros->prev)
	{
		for(FklCodegenMacro* cur=macros->head;cur;cur=cur->next)
		{
			FklHashTable* ht=fklCreatePatternMatchingHashTable();
			if(fklPatternMatch(cur->pattern,exp,ht))
			{
				*pht=ht;
				r=cur;
				break;
			}
			fklDestroyHashTable(ht);
		}
	}
	return r;
}

static void initVMframeFromPatternMatchTable(FklVM* exe
		,FklVMframe* frame
		,FklHashTable* ht
		,FklHashTable* lineHash
		,FklFuncPrototypes* pts
		,uint32_t prototype_id)
{
	FklFuncPrototype* mainPts=&pts->pts[prototype_id];
	FklVMCompoundFrameVarRef* lr=fklGetCompoundFrameLocRef(frame);
	FklVMproc* proc=FKL_VM_PROC(fklGetCompoundFrameProc(frame));
	uint32_t count=mainPts->lcount;
	FklVMvalue** loc=fklAllocSpaceForLocalVar(exe,count);
	uint32_t idx=0;
	for(FklHashTableItem* list=ht->first;list;list=list->next)
	{
		FklPatternMatchingHashTableItem* item=(FklPatternMatchingHashTableItem*)list->data;
		FklVMvalue* v=fklCreateVMvalueFromNastNode(exe,item->node,lineHash);
		loc[idx]=v;
		idx++;
	}
	lr->loc=loc;
	lr->lcount=count;
	lr->lref=NULL;
	lr->lrefl=NULL;
	proc->closure=lr->ref;
	proc->rcount=lr->rcount;
}

typedef struct MacroExpandCtx
{
	int done;
	FklNastNode** retval;
	FklHashTable* lineHash;
	FklSymbolTable* symbolTable;
	uint64_t curline;
}MacroExpandCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(MacroExpandCtx);

static void macro_expand_frame_atomic(void* data,FklVMgc* gc)
{
}

static void macro_expand_frame_step(void* data,FklVM* exe)
{
	MacroExpandCtx* ctx=(MacroExpandCtx*)data;
	fklVMacquireSt(exe->gc);
	*(ctx->retval)=fklCreateNastNodeFromVMvalue(fklGetTopValue(exe)
			,ctx->curline
			,ctx->lineHash
			,exe->gc);
	fklVMreleaseSt(exe->gc);
	ctx->done=1;
}

static void macro_expand_frame_finalizer(void* data)
{
}

static int macro_expand_frame_end(void* data)
{
	return ((MacroExpandCtx*)data)->done;
}

static void macro_expand_frame_backtrace(void* data,FILE* fp,FklVMgc* gc)
{
	fputs("at <macroexpand>\n",fp);
}

static const FklVMframeContextMethodTable MacroExpandMethodTable=
{
	.step=macro_expand_frame_step,
	.end=macro_expand_frame_end,
	.atomic=macro_expand_frame_atomic,
	.finalizer=macro_expand_frame_finalizer,
	.print_backtrace=macro_expand_frame_backtrace,
};

static void insert_macro_expand_frame(FklVM* exe,FklVMframe* mainframe
		,FklNastNode** ptr
		,FklHashTable* lineHash
		,FklSymbolTable* symbolTable
		,uint64_t curline)
{
	FklVMframe* f=fklCreateOtherObjVMframe(exe,&MacroExpandMethodTable,mainframe->prev);
	mainframe->prev=f;
	MacroExpandCtx* ctx=(MacroExpandCtx*)f->data;
	ctx->done=0;
	ctx->retval=ptr;
	ctx->lineHash=lineHash;
	ctx->symbolTable=symbolTable;
	ctx->curline=curline;
}

FklVM* fklInitMacroExpandVM(FklByteCodelnt* bcl
		,FklFuncPrototypes* pts
		,uint32_t prototype_id
		,FklHashTable* ht
		,FklHashTable* lineHash
		,FklPtrStack* macroLibStack
		,FklNastNode** pr
		,uint64_t curline
		,FklSymbolTable* publicSymbolTable)
{
	FklVM* anotherVM=fklCreateVM(fklCopyByteCodelnt(bcl)
			,publicSymbolTable
			,pts
			,prototype_id);
	insert_macro_expand_frame(anotherVM
			,anotherVM->top_frame
			,pr
			,lineHash
			,publicSymbolTable
			,curline);

	anotherVM->libNum=macroLibStack->top;
	anotherVM->libs=(FklVMlib*)calloc(macroLibStack->top+1,sizeof(FklVMlib));
	FKL_ASSERT(anotherVM->libs);
	for(size_t i=0;i<macroLibStack->top;i++)
	{
		FklCodegenLib* cur=macroLibStack->base[i];
		fklInitVMlibWithCodegenLib(cur,&anotherVM->libs[i+1],anotherVM,1,pts);
	}
	FklVMframe* mainframe=anotherVM->top_frame;
	fklInitGlobalVMclosure(mainframe,anotherVM);
	initVMframeFromPatternMatchTable(anotherVM,mainframe,ht,lineHash,pts,prototype_id);
	return anotherVM;
}

static inline void get_macro_pts_and_lib(FklCodegenInfo* info,FklFuncPrototypes** ppts,FklPtrStack** plib)
{
	for(;info->prev;info=info->prev);
	*ppts=info->macro_pts;
	*plib=info->macroLibStack;
}

FklNastNode* fklTryExpandCodegenMacro(FklNastNode* exp
		,FklCodegenInfo* codegen
		,FklCodegenMacroScope* macros
		,FklCodegenErrorState* errorState)
{
	FklSymbolTable* pst=&codegen->outer_ctx->public_symbol_table;
	FklNastNode* r=exp;
	FklHashTable* ht=NULL;
	uint64_t curline=exp->curline;
	for(FklCodegenMacro* macro=findMacro(r,macros,&ht)
			;!errorState->type&&macro
			;macro=findMacro(r,macros,&ht))
	{
		fklPatternMatchingHashTableSet(codegen->outer_ctx->builtInPatternVar_orig,exp,ht);
		FklNastNode* retval=NULL;
		FklHashTable lineHash;
		fklInitLineNumHashTable(&lineHash);

		FklCodegenOuterCtx* outer_ctx=codegen->outer_ctx;
		const char* cwd=outer_ctx->cwd;
		fklChdir(codegen->dir);

		FklFuncPrototypes* pts=NULL;
		FklPtrStack* macroLibStack=NULL;
		get_macro_pts_and_lib(codegen,&pts,&macroLibStack);
		FklVM* anotherVM=fklInitMacroExpandVM(macro->bcl
				,pts
				,macro->prototype_id
				,ht
				,&lineHash
				,macroLibStack
				,&retval
				,r->curline
				,pst);
		FklVMgc* gc=anotherVM->gc;
		int e=fklRunVM(anotherVM);

		fklChdir(cwd);

		anotherVM->pts=NULL;
		if(e)
		{
			errorState->type=FKL_ERR_MACROEXPANDFAILED;
			errorState->place=r;
			errorState->line=curline;
			fklDeleteCallChain(anotherVM);
			r=NULL;
		}
		else
		{
			if(retval)
			{
				fklDestroyNastNode(r);
				r=retval;
			}
			else
			{
				errorState->type=FKL_ERR_CIR_REF;
				errorState->place=NULL;
				errorState->line=curline;
			}
		}
		fklDestroyHashTable(ht);
		fklUninitHashTable(&lineHash);
		fklDestroyAllVMs(anotherVM);
		fklDestroyVMgc(gc);
	}
	return r;
}

void fklInitVMlibWithCodegenLibRefs(FklCodegenLib* clib
		,FklVMlib* vlib
		,FklVM* exe
		,FklVMvalue** refs
		,uint32_t count
		,int needCopy
		,FklFuncPrototypes* pts)
{
	FklVMvalue* val=FKL_VM_NIL;
	if(clib->type==FKL_CODEGEN_LIB_SCRIPT)
	{
		FklByteCode* bc=clib->bcl->bc;
		FklVMvalue* codeObj=fklCreateVMvalueCodeObj(exe,needCopy?fklCopyByteCodelnt(clib->bcl):clib->bcl);
		FklVMvalue* proc=fklCreateVMvalueProc(exe,bc->code,bc->len,codeObj,clib->prototypeId);
		fklInitMainProcRefs(FKL_VM_PROC(proc),refs,count);
		val=proc;
	}
	else
		val=fklCreateVMvalueStr(exe,fklCreateString(strlen(clib->rp)-strlen(FKL_DLL_FILE_TYPE),clib->rp));
	fklInitVMlib(vlib,val);
}

void fklInitVMlibWithCodegenLib(FklCodegenLib* clib
		,FklVMlib* vlib
		,FklVM* exe
		,int needCopy
		,FklFuncPrototypes* pts)
{
	FklVMvalue* val=FKL_VM_NIL;
	if(clib->type==FKL_CODEGEN_LIB_SCRIPT)
	{
		FklByteCode* bc=clib->bcl->bc;
		FklVMvalue* codeObj=fklCreateVMvalueCodeObj(exe,needCopy?fklCopyByteCodelnt(clib->bcl):clib->bcl);
		FklVMvalue* proc=fklCreateVMvalueProc(exe,bc->code,bc->len,codeObj,clib->prototypeId);
		val=proc;
	}
	else
		val=fklCreateVMvalueStr(exe,fklCreateString(strlen(clib->rp)-strlen(FKL_DLL_FILE_TYPE),clib->rp));
	fklInitVMlib(vlib,val);
}

void fklDestroyCodegenLibExceptBclAndDll(FklCodegenLib* clib)
{
	fklDestroyCodegenLibMacroScope(clib);
	fklUninitCodegenLibInfo(clib);
	free(clib);
}

void fklInitVMlibWithCodegenLibAndDestroy(FklCodegenLib* clib
		,FklVMlib* vlib
		,FklVM* exe
		,FklFuncPrototypes* pts)
{
	FklVMvalue* val=FKL_VM_NIL;
	if(clib->type==FKL_CODEGEN_LIB_SCRIPT)
	{
		FklByteCode* bc=clib->bcl->bc;
		FklVMvalue* codeObj=fklCreateVMvalueCodeObj(exe,clib->bcl);
		FklVMvalue* proc=fklCreateVMvalueProc(exe,bc->code,bc->len,codeObj,clib->prototypeId);
		val=proc;
	}
	else
	{
		val=fklCreateVMvalueStr(exe,fklCreateString(strlen(clib->rp)-strlen(FKL_DLL_FILE_TYPE),clib->rp));
		uv_dlclose(&clib->dll);
	}
	fklInitVMlib(vlib,val);

	fklDestroyCodegenLibExceptBclAndDll(clib);
}

static inline void print_nast_node_with_null_chr(const FklNastNode* node,const FklSymbolTable* st,FILE* fp)
{
	fklPrintNastNode(node,fp,st);
	fputc('\0',fp);
}

static inline void write_ignores(FklPtrStack* igs,const FklSymbolTable* st,FILE* fp)
{
	uint32_t top=igs->top;
	fwrite(&top,sizeof(top),1,fp);
	for(uint32_t i=0;i<top;i++)
	{
		FklNastNode* node=igs->base[i];
		print_nast_node_with_null_chr(node,st,fp);
	}
}

static inline void write_prods(FklPtrStack* prods,const FklSymbolTable* st,FILE* fp)
{
	uint32_t top=prods->top;
	fwrite(&top,sizeof(top),1,fp);
	for(uint32_t i=0;i<top;i++)
	{
		FklCodegenProdPrinting* printing=prods->base[i];
		fwrite(&printing->group_id,sizeof(printing->group_id),1,fp);
		fwrite(&printing->sid,sizeof(printing->sid),1,fp);
		print_nast_node_with_null_chr(printing->vec,st,fp);
		fwrite(&printing->add_extra,sizeof(printing->add_extra),1,fp);
		fwrite(&printing->type,sizeof(printing->type),1,fp);
		if(printing->type==FKL_CODEGEN_PROD_CUSTOM)
		{
			uint32_t protoId=printing->prototype_id;
			fwrite(&protoId,sizeof(protoId),1,fp);
			fklWriteByteCodelnt(printing->bcl,fp);
		}
		else
			print_nast_node_with_null_chr(printing->forth,st,fp);
	}
}

void fklWriteNamedProds(const FklHashTable* named_prod_groups
		,const FklSymbolTable* st
		,FILE* fp)
{
	uint8_t has_named_prod=named_prod_groups->t!=NULL;
	fwrite(&has_named_prod,sizeof(has_named_prod),1,fp);
	if(!has_named_prod)
		return;
	fwrite(&named_prod_groups->num,sizeof(named_prod_groups->num),1,fp);
	for(FklHashTableItem* list=named_prod_groups->first;list;list=list->next)
	{
		FklGrammerProductionGroup* group=(FklGrammerProductionGroup*)list->data;
		fwrite(&group->id,sizeof(group->id),1,fp);
		write_prods(&group->prod_printing,st,fp);
		write_ignores(&group->ignore_printing,st,fp);
	}
}

void fklWriteExportNamedProds(const FklHashTable* export_named_prod_groups
		,const FklHashTable* named_prod_groups
		,const FklSymbolTable* st
		,FILE* fp)
{
	uint8_t has_named_prod=export_named_prod_groups->num>0;
	fwrite(&has_named_prod,sizeof(has_named_prod),1,fp);
	if(!has_named_prod)
		return;
	fwrite(&export_named_prod_groups->num,sizeof(export_named_prod_groups->num),1,fp);
	for(FklHashTableItem* list=export_named_prod_groups->first;list;list=list->next)
	{
		FklSid_t id=*(FklSid_t*)list->data;
		FklGrammerProductionGroup* group=get_production_group(named_prod_groups,id);
		fwrite(&group->id,sizeof(group->id),1,fp);
		write_prods(&group->prod_printing,st,fp);
		write_ignores(&group->ignore_printing,st,fp);
	}
}

