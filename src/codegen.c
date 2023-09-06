#include<fakeLisp/codegen.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/vm.h>
#include<ctype.h>
#include<string.h>
#ifdef _WIN32
#include<io.h>
#include<process.h>
#else
#include<unistd.h>
#endif

static FklSid_t builtInPatternVar_rest=0;
static FklSid_t builtInPatternVar_name=0;
static FklSid_t builtInPatternVar_value=0;
static FklSid_t builtInPatternVar_cond=0;
static FklSid_t builtInPatternVar_args=0;
static FklSid_t builtInPatternVar_arg0=0;
static FklSid_t builtInPatternVar_arg1=0;
static FklSid_t builtInPatternVar_custom=0;
static FklSid_t builtInPatternVar_builtin=0;
static FklSid_t builtInPatternVar_simple=0;

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

static FklCodegenNextExpression* createCodegenNextExpression(const FklNextExpressionMethodTable* t
		,void* context)
{
	FklCodegenNextExpression* r=(FklCodegenNextExpression*)malloc(sizeof(FklCodegenNextExpression));
	FKL_ASSERT(r);
	r->t=t;
	r->context=context;
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
			,queue);
}

typedef enum
{
	SUB_PATTERN_UNQUOTE=0,
	SUB_PATTERN_UNQTESP=1,
}SubPatternEnum;

static struct SubPattern
{
	char* ps;
	FklNastNode* pn;
}builtInSubPattern[]=
{
	{"~(unquote ~value)",NULL,},
	{"~(unqtesp ~value)",NULL,},
	{NULL,NULL,},
};

static FklByteCode* create1lenBc(FklOpcode op)
{
	FklByteCode* r=fklCreateByteCode(1);
	r->code[0]=op;
	return r;
}

static FklByteCode* create5lenBc(FklOpcode op,uint32_t imm)
{
	FklByteCode* r=fklCreateByteCode(5);
	r->code[0]=op;
	fklSetU32ToByteCode(r->code+sizeof(char),imm);
	return r;
}

static FklByteCode* create13lenBc(FklOpcode op,uint32_t im,uint64_t imm)
{
	FklByteCode* r=fklCreateByteCode(13);
	r->code[0]=op;
	fklSetU32ToByteCode(r->code+sizeof(char),im);
	fklSetU64ToByteCode(r->code+sizeof(char)+sizeof(uint32_t),imm);
	return r;
}

static FklByteCode* create9lenBc(FklOpcode op,uint64_t imm)
{
	FklByteCode* r=fklCreateByteCode(9);
	r->code[0]=op;
	fklSetU64ToByteCode(r->code+sizeof(char),imm);
	return r;
}

static FklByteCodelnt* createBclnt(FklByteCode* bc
		,FklSid_t fid
		,uint64_t line)
{
	FklByteCodelnt* r=fklCreateByteCodelnt(bc);
	r->ls=1;
	r->l=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*1);
	FKL_ASSERT(r->l);
	fklInitLineNumTabNode(&r->l[0],fid,0,bc->size,line);
	return r;
}

static FklByteCode* createDupPutLocOrVar(uint32_t idx,FklOpcode putLocOrVar)
{
	FklByteCode* r=fklCreateByteCode(sizeof(char)+sizeof(uint32_t));
	r->code[0]=putLocOrVar;
	fklSetU32ToByteCode(r->code+sizeof(char),idx);
	return r;
}

static FklByteCodelnt* makePutLoc(const FklNastNode* sym,uint32_t idx,FklCodegen* codegen)
{
	fklAddSymbol(fklGetSymbolWithIdFromPst(sym->sym)->symbol,codegen->globalSymTable);
	FklByteCodelnt* r=createBclnt(createDupPutLocOrVar(idx,FKL_OP_PUT_LOC),codegen->fid,sym->curline);
	return r;
}

static FklByteCodelnt* makePutRefLoc(const FklNastNode* sym
		,uint32_t idx
		,FklCodegen* codegen)
{
	fklAddSymbol(fklGetSymbolWithIdFromPst(sym->sym)->symbol,codegen->globalSymTable);
	FklByteCodelnt* r=createBclnt(createDupPutLocOrVar(idx,FKL_OP_PUT_VAR_REF),codegen->fid,sym->curline);
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
		,FklCodegen* codegen)
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
	fklDestroyCodegener(quest->codegen);
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

#define BC_PROCESS(NAME) static FklByteCodelnt* NAME(FklCodegen* codegen\
		,FklCodegenEnv* env\
		,uint32_t scope\
		,FklCodegenMacroScope* cms\
		,FklCodegenQuestContext* context\
		,FklSid_t fid\
		,uint64_t line\
		,FklCodegenErrorState* errorState)

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
		uint8_t opcodes[]={
			FKL_OP_DROP,
		};
		FklByteCode drop={1,&opcodes[0]};
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		size_t top=stack->top;
		for(size_t i=0;i<top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			if(cur->bc->size)
			{
				fklCodeLntCat(retval,cur);
				if(i<top-1)
					fklBclBcAppendToBcl(retval,&drop,fid,line);
			}
			fklDestroyByteCodelnt(cur);
		}
		stack->top=0;
		return retval;
	}
	else
		return createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);

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
		,FklCodegen* codegen)
{
	FklSymbolDef* ref=NULL;
	while(env)
	{
		FklHashTableItem* list=env->refs->first;
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
		return fklGetBuiltInInlineFunc(idx,argNum);
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
		if(argNum<4
				&&funcBc->code[0]==FKL_OP_GET_VAR_REF
				&&(inlFunc=is_inlinable_func_ref(fklGetU32FromByteCode(&funcBc->code[1])
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
				fklCodeLntCat(retval,cur);
				fklDestroyByteCodelnt(cur);
			}
			uint8_t opcodes[]={FKL_OP_SET_BP,FKL_OP_CALL};
			FklByteCode setBp={1,&opcodes[0],};
			FklByteCode call={1,&opcodes[1],};
			fklBcBclAppendToBcl(&setBp,retval,fid,line);
			fklCodeLntCat(retval,func);
			fklBclBcAppendToBcl(retval,&call,fid,line);
			return retval;
		}
	}
	else
		return createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
}

static int isNonRetvalExp(const FklNastNode*);

static int pushFuncallListToQueue(FklNastNode* list,FklPtrQueue* queue,FklNastNode** last)
{
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr)
		if(isNonRetvalExp(list->pair->car))
			return 1;
		else
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
		,FklCodegen* codegen
		,FklCodegenErrorState* errorState)
{
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklNastNode* last=NULL;
	int r=pushFuncallListToQueue(rest,queue,&last);
	if(r||last->type!=FKL_NAST_NIL)
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(rest);
		while(!fklIsPtrQueueEmpty(queue))
			fklDestroyNastNode(fklPopPtrQueue(queue));
		fklDestroyPtrQueue(queue);
	}
	else
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_funcall_exp_bc_process
				,createDefaultStackContext(fklCreatePtrStack(32,16))
				,createDefaultQueueNextExpression(queue)
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
		,FklCodegen* codegen\
		,FklCodegenErrorState* errorState

#define CODEGEN_FUNC(NAME) void NAME(CODEGEN_ARGS)

static CODEGEN_FUNC(codegen_begin)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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
	newScope->defs=fklCreateHashTable(&CodegenEnvHashMethodTable);
	newScope->start=0;
	newScope->end=0;
	if(p)
		newScope->start=scopes[p-1].defs->num+scopes[p-1].start;
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

static inline int has_var_be_ref(uint8_t* flags
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
	uint8_t c[9]={FKL_OP_CLOSE_REF,};
	FklByteCode bc={9,c};
	fklSetU32ToByteCode(&c[1],s);
	fklSetU32ToByteCode(&c[1+sizeof(s)],e);
	fklBclBcAppendToBcl(retval,&bc,fid,line);
}

static inline void close_ref_to_local_scope(FklByteCodelnt* retval
		,uint32_t scope
		,FklCodegenEnv* env
		,FklCodegen* codegen
		,FklSid_t fid
		,uint64_t line)
{
	FklCodegenEnvScope* cur=&env->scopes[scope-1];
	uint32_t start=cur->start;
	uint32_t end=start+1;
	if(has_var_be_ref(env->slotFlags,cur,env,codegen->pts,&start,&end))
		append_close_ref(retval,start,end,fid,line);
}

BC_PROCESS(_local_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=sequnce_exp_bc_process(stack,fid,line);
	close_ref_to_local_scope(retval,scope,env,codegen,fid,line);
	return retval;
}

static CODEGEN_FUNC(codegen_local)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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

static inline FklNastNode* cadr_nast_node(const FklNastNode* node)
{
	return node->pair->cdr->pair->car;
}

static int is_valid_let_arg(const FklNastNode* node)
{
	return node->type==FKL_NAST_PAIR
		&&fklIsNastNodeList(node)
		&&nast_list_len(node)==2
		&&node->pair->car->type==FKL_NAST_SYM
		&&!isNonRetvalExp(cadr_nast_node(node));
}

static int is_valid_let_args(const FklNastNode* sl
		,FklCodegenEnv* env
		,uint32_t scope
		,FklUintStack* stack)
{
	if(fklIsNastNodeList(sl))
	{
		for(;sl->type==FKL_NAST_PAIR;sl=sl->pair->cdr)
		{
			FklNastNode* cc=sl->pair->car;
			if(!is_valid_let_arg(cc))
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

BC_PROCESS(_let1_exp_bc_process)
{
	Let1Context* ctx=context->data;
	FklPtrStack* bcls=ctx->stack;
	FklUintStack* symbolStack=ctx->ss;
	uint8_t bpc[2]={FKL_OP_SET_BP,FKL_OP_RES_BP};
	FklByteCode setBp={1,&bpc[0]};
	FklByteCode resBp={1,&bpc[1]};

	uint8_t popac[5]={FKL_OP_POP_ARG};
	FklByteCode popArg={5,popac};

	FklByteCodelnt* retval=fklPopPtrStack(bcls);
	fklBcBclAppendToBcl(&resBp,retval,fid,line);
	while(!fklIsUintStackEmpty(symbolStack))
	{
		FklSid_t id=fklPopUintStack(symbolStack);
		uint32_t idx=fklAddCodegenDefBySid(id,scope,env);
		fklSetU32ToByteCode(&popac[1],idx);
		fklBcBclAppendToBcl(&popArg,retval,fid,line);
	}
	if(!fklIsPtrStackEmpty(bcls))
	{
		FklByteCodelnt* args=fklPopPtrStack(bcls);
		fklReCodeLntCat(args,retval);
		fklDestroyByteCodelnt(args);
	}
	fklBcBclAppendToBcl(&setBp,retval,fid,line);
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
			fklCodeLntCat(retval,cur);
			fklDestroyByteCodelnt(cur);
		}
		return retval;
	}
	else
		return NULL;
}

static CODEGEN_FUNC(codegen_let1)
{
	FklUintStack* symStack=fklCreateUintStack(8,16);
	FklNastNode* firstSymbol=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	if(firstSymbol->type!=FKL_NAST_SYM||isNonRetvalExp(value))
	{
		fklDestroyUintStack(symStack);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
		return;
	}
	FklPatternMatchingHashTableItem* item=fklGetHashItem(&builtInPatternVar_args,ht);
	FklNastNode* args=item?item->node:NULL;
	uint32_t cs=enter_new_scope(scope,curEnv);

	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

	fklAddCodegenDefBySid(firstSymbol->sym,cs,curEnv);
	fklPushUintStack(firstSymbol->sym,symStack);

	FklPtrQueue* valueQueue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(value),valueQueue);

	if(args)
	{
		if(!is_valid_let_args(args,curEnv,cs,symStack))
		{
			cms->refcount=1;
			fklDestroyNastNode(value);
			fklDestroyPtrQueue(valueQueue);
			fklDestroyCodegenMacroScope(cms);
			fklDestroyUintStack(symStack);
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(origExp);
			errorState->fid=codegen->fid;
			return;
		}
		for(FklNastNode* cur=args;cur->type==FKL_NAST_PAIR;cur=cur->pair->cdr)
			fklPushPtrQueue(fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)),valueQueue);
	}

	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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
			,createDefaultQueueNextExpression(valueQueue)
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
	FklSid_t letHeadId=fklAddSymbolCstrToPst("let")->id;
	FklNastNode* letHead=fklCreateNastNode(FKL_NAST_SYM,origExp->pair->car->curline);
	letHead->sym=letHeadId;

	FklNastNode* firstNameValue=cadr_nast_node(origExp);

	FklNastNode* args=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);

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
	FklUintStack* symStack=fklCreateUintStack(8,16);
	FklNastNode* firstSymbol=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	if(firstSymbol->type!=FKL_NAST_SYM||isNonRetvalExp(value))
	{
		fklDestroyUintStack(symStack);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
		return;
	}
	FklNastNode* args=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);
	uint32_t cs=enter_new_scope(scope,curEnv);

	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

	fklAddCodegenDefBySid(firstSymbol->sym,cs,curEnv);
	fklPushUintStack(firstSymbol->sym,symStack);

	if(!is_valid_let_args(args,curEnv,cs,symStack))
	{
		cms->refcount=1;
		fklDestroyCodegenMacroScope(cms);
		fklDestroyUintStack(symStack);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
		return;
	}

	FklPtrQueue* valueQueue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(value),valueQueue);
	for(FklNastNode* cur=args;cur->type==FKL_NAST_PAIR;cur=cur->pair->cdr)
		fklPushPtrQueue(fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)),valueQueue);

	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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
			,createDefaultQueueNextExpression(valueQueue)
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

	uint8_t popC[]={FKL_OP_DROP,};
	FklByteCode pop={1,popC};
	uint8_t jmpC[9]={FKL_OP_JMP,};
	FklByteCode jmp={9,jmpC};
	uint8_t jmpIfTrueC[9]={FKL_OP_JMP_IF_TRUE,};
	FklByteCode jmpIfTrue={9,jmpIfTrueC};

	if(rest->bc->size)
		fklBcBclAppendToBcl(&pop,rest,fid,line);

	fklSetI64ToByteCode(&jmpIfTrueC[1],rest->bc->size+jmp.size);
	fklBclBcAppendToBcl(cond,&jmpIfTrue,fid,line);

	int64_t jmpLen=rest->bc->size+cond->bc->size+jmp.size;
	fklSetI64ToByteCode(&jmpC[1],-jmpLen);
	fklBclBcAppendToBcl(rest,&jmp,fid,line);
	fklCodeLntCat(cond,rest);
	fklDestroyByteCodelnt(rest);

	if(value->bc->size)
		fklBclBcAppendToBcl(cond,&pop,fid,line);
	fklReCodeLntCat(cond,value);
	fklDestroyByteCodelnt(cond);

	return value;
}

BC_PROCESS(_do_rest_exp_bc_process)
{
	FklPtrStack* s=GET_STACK(context);
	if(s->top)
	{
		uint8_t popC[]={FKL_OP_DROP,};
		FklByteCode pop={1,popC};
		FklByteCodelnt* r=sequnce_exp_bc_process(s,fid,line);
		fklBclBcAppendToBcl(r,&pop,fid,line);
		return r;
	}
	return fklCreateByteCodelnt(fklCreateByteCode(0));
}

static CODEGEN_FUNC(codegen_do0)
{
	FklNastNode* cond=fklPatternMatchingHashTableRef(builtInPatternVar_cond,ht);

	FklPatternMatchingHashTableItem* item=fklGetHashItem(&builtInPatternVar_value,ht);
	FklNastNode* value=item?item->node:NULL;

	if(isNonRetvalExp(cond)||(value&&isNonRetvalExp(value)))
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
		return;
	}
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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
				,createDefaultQueueNextExpression(vQueue)
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
			,createDefaultQueueNextExpression(cQueue)
			,cs
			,cms
			,curEnv
			,origExp->curline
			,do0Quest
			,codegen);
	fklPushPtrStack(do0CQuest,codegenQuestStack);
}

static inline int is_valid_do_var_bind(const FklNastNode* list,FklNastNode** nextV)
{
	if(!fklIsNastNodeList(list))
		return 0;
	if(list->pair->car->type!=FKL_NAST_SYM)
		return 0;
	size_t len=nast_list_len(list);
	if(len!=2&&len!=3)
		return 0;
	const FklNastNode* initVal=cadr_nast_node(list);
	if(isNonRetvalExp(initVal))
		return 0;
	if(len==3)
	{
		FklNastNode* next=caddr_nast_node(list);
		if(isNonRetvalExp(next))
			return 0;
		*nextV=next;
	}
	return 1;
}

static inline void destroy_next_exp_queue(FklPtrQueue* q)
{
	while(!fklIsPtrQueueEmpty(q))
		fklDestroyNastNode(fklPopPtrQueue(q));
	fklDestroyPtrQueue(q);
}

static inline int is_valid_do_bind_list(const FklNastNode* sl
		,FklCodegenEnv* env
		,uint32_t scope
		,FklUintStack* stack
		,FklUintStack* nstack
		,FklPtrQueue* valueQueue
		,FklPtrQueue* nextQueue)
{
	if(fklIsNastNodeList(sl))
	{
		for(;sl->type==FKL_NAST_PAIR;sl=sl->pair->cdr)
		{
			FklNastNode* cc=sl->pair->car;
			FklNastNode* nextExp=NULL;
			if(!is_valid_do_var_bind(cc,&nextExp))
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

	uint8_t popC[1]={FKL_OP_DROP,};
	FklByteCode pop={1,popC,};

	uint8_t putLocC[5]={FKL_OP_PUT_LOC,};
	FklByteCode putLoc={5,putLocC,};

	uint64_t* idxbase=ss->base;
	FklByteCodelnt** bclBase=(FklByteCodelnt**)s->base;
	uint32_t top=s->top;
	for(uint32_t i=0;i<top;i++)
	{
		uint32_t idx=idxbase[i];
		FklByteCodelnt* curBcl=bclBase[i];
		fklSetU32ToByteCode(&putLocC[1],idx);
		fklBclBcAppendToBcl(curBcl,&putLoc,fid,line);
		fklBclBcAppendToBcl(curBcl,&pop,fid,line);
		fklCodeLntCat(ret,curBcl);
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
		uint8_t popC[1]={FKL_OP_DROP,};
		FklByteCode pop={1,popC,};

		uint8_t putLocC[5]={FKL_OP_PUT_LOC,};
		FklByteCode putLoc={5,putLocC,};

		uint64_t* idxbase=ss->base;
		FklByteCodelnt** bclBase=(FklByteCodelnt**)s->base;
		uint32_t top=s->top;
		for(uint32_t i=0;i<top;i++)
		{
			uint32_t idx=idxbase[i];
			FklByteCodelnt* curBcl=bclBase[i];
			fklSetU32ToByteCode(&putLocC[1],idx);
			fklBclBcAppendToBcl(curBcl,&putLoc,fid,line);
			fklBclBcAppendToBcl(curBcl,&pop,fid,line);
			fklCodeLntCat(ret,curBcl);
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

	uint8_t popC[]={FKL_OP_DROP,};
	FklByteCode pop={1,popC};
	uint8_t jmpC[9]={FKL_OP_JMP,};
	FklByteCode jmp={9,jmpC};
	uint8_t jmpIfTrueC[9]={FKL_OP_JMP_IF_TRUE,};
	FklByteCode jmpIfTrue={9,jmpIfTrueC};

	fklBclBcAppendToBcl(rest,&pop,fid,line);
	if(next)
	{
		fklCodeLntCat(rest,next);
		fklDestroyByteCodelnt(next);
	}

	fklSetI64ToByteCode(&jmpIfTrueC[1],rest->bc->size+jmp.size);
	fklBclBcAppendToBcl(cond,&jmpIfTrue,fid,line);

	int64_t jmpLen=rest->bc->size+cond->bc->size+jmp.size;
	fklSetI64ToByteCode(&jmpC[1],-jmpLen);
	fklBclBcAppendToBcl(rest,&jmp,fid,line);
	fklCodeLntCat(cond,rest);
	fklDestroyByteCodelnt(rest);

	if(value->bc->size)
		fklBclBcAppendToBcl(cond,&pop,fid,line);
	fklReCodeLntCat(cond,value);
	fklDestroyByteCodelnt(cond);

	fklReCodeLntCat(init,value);
	fklDestroyByteCodelnt(init);
	close_ref_to_local_scope(value,scope,env,codegen,fid,line);
	return value;
}

static CODEGEN_FUNC(codegen_do1)
{
	FklNastNode* bindlist=cadr_nast_node(origExp);

	FklNastNode* cond=fklPatternMatchingHashTableRef(builtInPatternVar_cond,ht);

	FklPatternMatchingHashTableItem* item=fklGetHashItem(&builtInPatternVar_value,ht);
	FklNastNode* value=item?item->node:NULL;

	if(isNonRetvalExp(cond)||(value&&isNonRetvalExp(value)))
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
		return;
	}

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
				,valueQueue,nextValueQueue))
	{
		cms->refcount=1;
		fklDestroyCodegenMacroScope(cms);
		fklDestroyUintStack(symStack);
		fklDestroyUintStack(nextSymStack);
		destroy_next_exp_queue(valueQueue);
		destroy_next_exp_queue(nextValueQueue);
		
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
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
			,createDefaultQueueNextExpression(nextValueQueue)
			,cs
			,cms
			,curEnv
			,origExp->curline
			,do1Quest
			,codegen);

	fklPushPtrStack(do1NextValQuest,codegenQuestStack);

	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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
				,createDefaultQueueNextExpression(vQueue)
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
			,createDefaultQueueNextExpression(cQueue)
			,cs
			,cms
			,curEnv
			,origExp->curline
			,do1Quest
			,codegen);
	fklPushPtrStack(do1CQuest,codegenQuestStack);

	FklCodegenQuest* do1InitValQuest=createCodegenQuest(_do1_init_val_bc_process
			,createLet1CodegenContext(symStack)
			,createDefaultQueueNextExpression(valueQueue)
			,scope
			,macroScope
			,curEnv
			,origExp->curline
			,do1Quest
			,codegen);
	fklPushPtrStack(do1InitValQuest,codegenQuestStack);
}

static inline FklSid_t get_sid_in_gs_with_id_in_ps(FklSid_t id,FklSymbolTable* gs)
{
	return fklAddSymbol(fklGetSymbolWithIdFromPst(id)->symbol,gs)->id;
}

static inline FklSid_t get_sid_with_idx(FklCodegenEnvScope* sc
		,uint32_t idx
		,FklSymbolTable* gs)
{
	for(FklHashTableItem* l=sc->defs->first;l;l=l->next)
	{
		FklSymbolDef* def=(FklSymbolDef*)l->data;
		if(def->idx==idx)
			return get_sid_in_gs_with_id_in_ps(def->k.id,gs);
	}
	return 0;
}

static inline FklSid_t get_sid_with_ref_idx(FklHashTable* refs
		,uint32_t idx
		,FklSymbolTable* gs)
{
	for(FklHashTableItem* l=refs->first;l;l=l->next)
	{
		FklSymbolDef* def=(FklSymbolDef*)l->data;
		if(def->idx==idx)
			return get_sid_in_gs_with_id_in_ps(def->k.id,gs);
	}
	return 0;
}

BC_PROCESS(_set_var_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* cur=fklPopPtrStack(stack);
	FklByteCodelnt* popVar=fklPopPtrStack(stack);
	if(cur&&popVar)
	{
		if(cur->bc->code[0]==FKL_OP_PUSH_PROC)
		{
			if(popVar->bc->code[0]==FKL_OP_PUT_LOC)
			{
				uint32_t prototypeId=fklGetU32FromByteCode(&cur->bc->code[1]);
				uint32_t idx=fklGetU32FromByteCode(&popVar->bc->code[1]);
				if(!codegen->pts->pts[prototypeId].sid)
					codegen->pts->pts[prototypeId].sid=get_sid_with_idx(&env->scopes[scope-1]
							,idx
							,codegen->globalSymTable);
			}
			else if(popVar->bc->code[0]==FKL_OP_PUT_VAR_REF)
			{
				uint32_t prototypeId=fklGetU32FromByteCode(&cur->bc->code[1]);
				uint32_t idx=fklGetU32FromByteCode(&popVar->bc->code[1]);
				if(!codegen->pts->pts[prototypeId].sid)
					codegen->pts->pts[prototypeId].sid=get_sid_with_ref_idx(env->refs
							,idx
							,codegen->globalSymTable);
			}
		}
		fklReCodeLntCat(cur,popVar);
		fklDestroyByteCodelnt(cur);
	}
	else
	{
		popVar=cur;
		uint8_t pushNilC[1]={FKL_OP_PUSH_NIL};
		FklByteCode pushNil={1,pushNilC};
		fklBcBclAppendToBcl(&pushNil,popVar,fid,line);
	}
	return popVar;
}

BC_PROCESS(_lambda_exp_bc_process)
{
	FklByteCodelnt* retval=NULL;
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top>1)
	{
		uint8_t opcodes[]={
			FKL_OP_DROP,
		};
		FklByteCode drop={1,&opcodes[0]};
		retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		size_t top=stack->top;
		for(size_t i=1;i<top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			if(cur->bc->size)
			{
				fklCodeLntCat(retval,cur);
				if(i<top-1)
					fklBclBcAppendToBcl(retval,&drop,fid,line);
			}
			fklDestroyByteCodelnt(cur);
		}
	}
	else
		retval=createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
	fklReCodeLntCat(stack->base[0],retval);
	fklDestroyByteCodelnt(stack->base[0]);
	stack->top=0;
	fklScanAndSetTailCall(retval->bc);
	FklFuncPrototypes* pts=codegen->pts;
	fklUpdatePrototype(pts,env,codegen->globalSymTable);
	FklByteCode* pushProc=create13lenBc(FKL_OP_PUSH_PROC,env->prototypeId,retval->bc->size);
	fklBcBclAppendToBcl(pushProc,retval,fid,line);
	fklDestroyByteCode(pushProc);
	return retval;
}

static FklByteCodelnt* processArgs(const FklNastNode* args
		,FklCodegenEnv* curEnv
		,FklCodegen* codegen)
{
	FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
	uint8_t popArgCode[5]={FKL_OP_POP_ARG,};
	uint8_t popRestArgCode[5]={FKL_OP_POP_REST_ARG,};
	FklByteCode popArg={5,popArgCode};
	FklByteCode popRestArg={5,popRestArgCode};

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

		fklAddSymbol(fklGetSymbolWithIdFromPst(cur->sym)->symbol
			,codegen->globalSymTable);

		fklSetU32ToByteCode(&popArgCode[1],idx);
		fklBclBcAppendToBcl(retval,&popArg,codegen->fid,cur->curline);

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

		fklAddSymbol(fklGetSymbolWithIdFromPst(args->sym)->symbol
			,codegen->globalSymTable);

		fklSetU32ToByteCode(&popRestArgCode[1],idx);
		fklBclBcAppendToBcl(retval,&popRestArg,codegen->fid,args->curline);

	}
	uint8_t opcodes[]={FKL_OP_RES_BP,};
	FklByteCode resBp={1,&opcodes[0]};
	fklBclBcAppendToBcl(retval,&resBp,codegen->fid,args->curline);
	return retval;
}

static FklByteCodelnt* processArgsInStack(FklUintStack* stack
		,FklCodegenEnv* curEnv
		,FklCodegen* codegen
		,uint64_t curline)
{
	uint8_t popArgCode[5]={FKL_OP_POP_ARG,};
	FklByteCode popArg={5,popArgCode};
	FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
	uint32_t top=stack->top;
	uint64_t* base=stack->base;
	for(uint32_t i=0;i<top;i++)
	{
		FklSid_t curId=base[i];

		uint32_t idx=fklAddCodegenDefBySid(curId,1,curEnv);

		fklAddSymbol(fklGetSymbolWithIdFromPst(curId)->symbol
			   ,codegen->globalSymTable);

		fklSetU32ToByteCode(&popArgCode[1],idx);
		fklBclBcAppendToBcl(retval,&popArg,codegen->fid,curline);
	}
	uint8_t opcodes[]={FKL_OP_RES_BP,};
	FklByteCode resBp={1,&opcodes[0]};
	fklBclBcAppendToBcl(retval,&resBp,codegen->fid,curline);
	return retval;
}

static inline FklSymbolDef* sid_ht_to_idx_key_ht(FklHashTable* sht,FklSymbolTable* globalSymTable)
{
	FklSymbolDef* refs=(FklSymbolDef*)malloc(sizeof(FklSymbolDef)*sht->num);
	FKL_ASSERT(refs);
	for(FklHashTableItem* list=sht->first;list;list=list->next)
	{
		FklSymbolDef* sd=(FklSymbolDef*)list->data;
		FklSid_t sid=fklAddSymbol(fklGetSymbolWithIdFromPst(sd->k.id)->symbol,globalSymTable)->id;
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
		,uint64_t line)
{
	cp->count+=1;
	FklFuncPrototype* pts=(FklFuncPrototype*)fklRealloc(cp->pts,sizeof(FklFuncPrototype)*(cp->count+1));
	FKL_ASSERT(pts);
	cp->pts=pts;
	FklFuncPrototype* cpt=&pts[cp->count];
	env->prototypeId=cp->count;
	cpt->lcount=env->lcount;
	cpt->refs=sid_ht_to_idx_key_ht(env->refs,globalSymTable);
	cpt->rcount=env->refs->num;
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
		fklReCodeLntCat(cur,popVar);
		fklDestroyByteCodelnt(cur);
	}
	else
	{
		popVar=cur;
		uint8_t pushNilC[1]={FKL_OP_PUSH_NIL};
		FklByteCode pushNil={1,pushNilC};
		fklBcBclAppendToBcl(&pushNil,popVar,fid,line);
	}
	close_ref_to_local_scope(popVar,scope,env,codegen,fid,line);
	return popVar;
}

static CODEGEN_FUNC(codegen_named_let0)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_arg0,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
		return;
	}
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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

	FklPtrStack* stack=fklCreatePtrStack(2,1);
	uint32_t idx=fklAddCodegenDefBySid(name->sym,cs,curEnv);
	fklPushPtrStack(makePutLoc(name,idx,codegen),stack);

	fklAddReplacementBySid(fklAddSymbolCstrToPst("*func*")->id
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
			,get_sid_in_gs_with_id_in_ps(name->sym,codegen->globalSymTable)
			,codegen->fid
			,origExp->curline);

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
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_arg0,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
		return;
	}
	FklUintStack* symStack=fklCreateUintStack(8,16);
	FklNastNode* firstSymbol=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	if(firstSymbol->type!=FKL_NAST_SYM||isNonRetvalExp(value))
	{
		fklDestroyUintStack(symStack);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
		return;
	}
	FklNastNode* args=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);

	uint32_t cs=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

	FklCodegenEnv* lambdaCodegenEnv=fklCreateCodegenEnv(curEnv,cs,cms);
	fklAddCodegenDefBySid(firstSymbol->sym,1,lambdaCodegenEnv);
	fklPushUintStack(firstSymbol->sym,symStack);

	if(!is_valid_let_args(args,lambdaCodegenEnv,1,symStack))
	{
		lambdaCodegenEnv->refcount=1;
		fklDestroyCodegenEnv(lambdaCodegenEnv);
		fklDestroyUintStack(symStack);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
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
	fklPushPtrStack(makePutLoc(name,idx,codegen),stack);
	
	fklAddReplacementBySid(fklAddSymbolCstrToPst("*func*")->id
			,name
			,cms->replacements);
	fklPushPtrStack(createCodegenQuest(_let_arg_exp_bc_process
				,createDefaultStackContext(fklCreatePtrStack(fklLengthPtrQueue(valueQueue),16))
				,createDefaultQueueNextExpression(valueQueue)
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

	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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
	 		,get_sid_in_gs_with_id_in_ps(name->sym,codegen->globalSymTable)
			,codegen->fid
			,origExp->curline);

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
		uint8_t opcodes[]={
			FKL_OP_DROP,
		};
		uint8_t jmpOpcodes[9]={FKL_OP_JMP_IF_FALSE,};
		FklByteCode drop={1,&opcodes[0]};
		FklByteCode jmpIfFalse={9,jmpOpcodes};
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		while(!fklIsPtrStackEmpty(stack))
		{
			if(retval->bc->size)
			{
				fklBcBclAppendToBcl(&drop,retval,fid,line);
				fklSetI64ToByteCode(&jmpOpcodes[1],retval->bc->size);
				fklBcBclAppendToBcl(&jmpIfFalse,retval,fid,line);
			}
			FklByteCodelnt* cur=fklPopPtrStack(stack);
			fklReCodeLntCat(cur,retval);
			fklDestroyByteCodelnt(cur);
		}
		return retval;
	}
	else
		return createBclnt(fklCreatePushI32ByteCode(1),fid,line);
}

static CODEGEN_FUNC(codegen_and)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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
		uint8_t opcodes[]={
			FKL_OP_DROP,
		};
		uint8_t jmpOpcodes[9]={FKL_OP_JMP_IF_TRUE,};
		FklByteCode drop={1,&opcodes[0]};
		FklByteCode jmpIfTrue={9,jmpOpcodes};
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		while(!fklIsPtrStackEmpty(stack))
		{
			if(retval->bc->size)
			{
				fklBcBclAppendToBcl(&drop,retval,fid,line);
				fklSetI64ToByteCode(&jmpOpcodes[1],retval->bc->size);
				fklBcBclAppendToBcl(&jmpIfTrue,retval,fid,line);
			}
			FklByteCodelnt* cur=fklPopPtrStack(stack);
			fklReCodeLntCat(cur,retval);
			fklDestroyByteCodelnt(cur);
		}
		return retval;
	}
	else
		return createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
}

static CODEGEN_FUNC(codegen_or)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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

inline void fklUpdatePrototype(FklFuncPrototypes* cp,FklCodegenEnv* env,FklSymbolTable* globalSymTable)
{
	FklFuncPrototype* pts=&cp->pts[env->prototypeId];
	FklHashTable* eht=NULL;
	pts->lcount=env->lcount;
	process_unresolve_ref(env,cp);
	eht=env->refs;
	uint32_t count=eht->num;
	FklSymbolDef* refs=(FklSymbolDef*)fklRealloc(pts->refs,sizeof(FklSymbolDef)*count);
	FKL_ASSERT(refs||!count);
	pts->refs=refs;
	pts->rcount=count;
	for(FklHashTableItem* list=eht->first;list;list=list->next)
	{
		FklSymbolDef* sd=(FklSymbolDef*)list->data;
		FklSid_t sid=fklAddSymbol(fklGetSymbolWithIdFromPst(sd->k.id)->symbol,globalSymTable)->id;
		FklSymbolDef ref={.k.id=sid,
			.k.scope=sd->k.scope,
			.idx=sd->idx,
			.cidx=sd->cidx,
			.isLocal=sd->isLocal};
		refs[sd->idx]=ref;
	}
}

static void initSymbolDef(FklSymbolDef* def,uint32_t idx)
{
	def->idx=idx;
	def->cidx=idx;
	def->isLocal=0;
}

static FklUnReSymbolRef* createUnReSymbolRef(FklSid_t id
		,uint32_t idx
		,uint32_t scope
		,uint32_t prototypeId
		,FklSid_t fid
		,uint64_t line)
{
	FklUnReSymbolRef* r=(FklUnReSymbolRef*)malloc(sizeof(FklUnReSymbolRef));
	FKL_ASSERT(r);
	r->id=id;
	r->idx=idx;
	r->scope=scope,
	r->prototypeId=prototypeId;
	r->fid=fid;
	r->line=line;
	return r;
}

typedef struct
{
	FklSid_t id;
	FklNastNode* node;
}FklCodegenReplacement;

static void _codegen_replacement_uninitItem(void* item)
{
	FklCodegenReplacement* replace=(FklCodegenReplacement*)item;
	fklDestroyNastNode(replace->node);
}

static int _codegen_replacement_keyEqual(const void* pkey0,const void* pkey1)
{
	FklSid_t k0=*(const FklSid_t*)pkey0;
	FklSid_t k1=*(const FklSid_t*)pkey1;
	return k0==k1;
}

static void _codegen_replacement_setVal(void* d0,const void* d1)
{
	*(FklCodegenReplacement*)d0=*(const FklCodegenReplacement*)d1;
	fklMakeNastNodeRef(((FklCodegenReplacement*)d0)->node);
}

static void _codegen_replacement_setKey(void* k0,const void* k1)
{
	*(FklSid_t*)k0=*(const FklSid_t*)k1;
}

static FklHashTableMetaTable CodegenReplacementHashMetaTable=
{
	.size=sizeof(FklCodegenReplacement),
	.__setKey=_codegen_replacement_setKey,
	.__setVal=_codegen_replacement_setVal,
	.__hashFunc=_codegenenv_hashFunc,
	.__uninitItem=_codegen_replacement_uninitItem,
	.__keyEqual=_codegen_replacement_keyEqual,
	.__getKey=fklHashDefaultGetKey,
};

static FklHashTable* createCodegenReplacements(void)
{
	return fklCreateHashTable(&CodegenReplacementHashMetaTable);
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
	r->refs=fklCreateHashTable(&CodegenEnvHashMethodTable);
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
				fklDestroyHashTable(scopes[i].defs);
			free(scopes);
			free(cur->slotFlags);
			fklDestroyHashTable(cur->refs);
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

#define INIT_SYMBOL_DEF(ID,SCOPE,IDX) {{ID,SCOPE},IDX,IDX,0}

uint32_t fklAddCodegenBuiltinRefBySid(FklSid_t id,FklCodegenEnv* env)
{
	FklHashTable* ht=env->refs;
	uint32_t idx=ht->num;
	FklSymbolDef def=INIT_SYMBOL_DEF(id,env->pscope,idx);
	FklSymbolDef* el=fklGetOrPutHashItem(&def,ht);
	return el->idx;
}

static inline FklSymbolDef* has_outer_ref(FklCodegenEnv* cur
		,FklSid_t id
		,FklCodegenEnv** targetEnv)
{
	FklSymbolDef* ref=NULL;
	FklSidScope key={id,0};
	for(;cur;cur=cur->prev)
	{
		key.scope=cur->pscope;
		ref=fklGetHashItem(&key,cur->refs);
		if(ref)
		{
			*targetEnv=cur;
			return ref;
		}
	}
	return NULL;
}

static inline FklSymbolDef* has_outer_def(FklCodegenEnv* cur
		,FklSid_t id
		,uint32_t scope
		,FklCodegenEnv** targetEnv)
{
	FklSymbolDef* def=NULL;
	for(;cur;cur=cur->prev)
	{
		def=fklFindSymbolDefByIdAndScope(id,scope,cur);
		if(def)
		{
			*targetEnv=cur;
			return def;
		}
		scope=cur->pscope;
	}
	return NULL;
}

static inline FklSymbolDef* add_ref_per_penv(FklSid_t id
		,FklCodegenEnv* cur
		,FklCodegenEnv* targetEnv)
{
	uint32_t idx=cur->refs->num;
	FklSymbolDef def=INIT_SYMBOL_DEF(id,cur->pscope,idx);
	FklSymbolDef* cel=fklGetOrPutHashItem(&def,cur->refs);
	for(cur=cur->prev;cur!=targetEnv;cur=cur->prev)
	{
		uint32_t idx=cur->refs->num;
		def.k.scope=cur->pscope;
		initSymbolDef(&def,idx);
		FklSymbolDef* nel=fklGetOrPutHashItem(&def,cur->refs);
		cel->cidx=nel->idx;
		cel=nel;
	}
	return cel;
}

static inline int is_ref_solved(FklSymbolDef* ref,FklCodegenEnv* env)
{
	if(env)
	{
		uint32_t top=env->uref.top;
		FklUnReSymbolRef** refs=(FklUnReSymbolRef**)env->uref.base;
		for(uint32_t i=0;i<top;i++)
		{
			FklUnReSymbolRef* cur=refs[i];
			if(cur->id==ref->k.id&&cur->scope==ref->k.scope)
				return 0;
		}
	}
	return 1;
}

uint32_t fklAddCodegenRefBySid(FklSid_t id,FklCodegenEnv* env,FklSid_t fid,uint64_t line)
{
	FklHashTable* ht=env->refs;
	FklSidScope key={id,env->pscope};
	FklSymbolDef* el=fklGetHashItem(&key,ht);
	if(el)
		return el->idx;
	else
	{
		uint32_t idx=ht->num;
		FklCodegenEnv* cur=env->prev;
		if(cur)
		{
			FklCodegenEnv* targetEnv=NULL;
			FklSymbolDef* targetDef=has_outer_def(cur,id,env->pscope,&targetEnv);
			if(targetDef)
			{
				FklSymbolDef* cel=add_ref_per_penv(id,env,targetEnv);
				cel->isLocal=1;
				cel->cidx=targetDef->idx;
				targetEnv->slotFlags[targetDef->idx]=FKL_CODEGEN_ENV_SLOT_REF;
			}
			else
			{
				FklSymbolDef* targetRef=has_outer_ref(cur
						,id
						,&targetEnv);
				if(targetRef&&is_ref_solved(targetRef,targetEnv->prev))
					add_ref_per_penv(id,env,targetEnv->prev);
				else
				{
					FklSymbolDef def=INIT_SYMBOL_DEF(id,env->pscope,idx);
					FklSymbolDef* cel=fklGetOrPutHashItem(&def,ht);
					cel->cidx=cur->refs->num;
					FklUnReSymbolRef* unref=createUnReSymbolRef(id,idx,def.k.scope,env->prototypeId,fid,line);
					fklPushPtrStack(unref,&cur->uref);
				}
			}
		}
		else
		{
			FklSymbolDef def=INIT_SYMBOL_DEF(id,0,idx);
			FklSymbolDef* cel=fklGetOrPutHashItem(&def,ht);
			idx=cel->idx;
			FklUnReSymbolRef* unref=createUnReSymbolRef(id,idx,0,env->prototypeId,fid,line);
			fklPushPtrStack(unref,&env->uref);
		}
		return idx;
	}
}

static inline FklSymbolDef* get_def_by_id_in_scope(FklSid_t id,uint32_t scopeId,FklCodegenEnvScope* scope)
{
	FklSidScope key={id,scopeId};
	return fklGetHashItem(&key,scope->defs);
}

FklSymbolDef* fklFindSymbolDefByIdAndScope(FklSid_t id,uint32_t scope,FklCodegenEnv* env)
{
	FklCodegenEnvScope* scopes=env->scopes;
	for(;scope;scope=scopes[scope-1].p)
	{
		FklSymbolDef* r=get_def_by_id_in_scope(id,scope,&scopes[scope-1]);
		if(r)
			return r;
	}
	return NULL;
}

static inline int has_resolvable_ref(FklSid_t id,uint32_t scope,FklCodegenEnv* env)
{
	FklUnReSymbolRef** urefs=(FklUnReSymbolRef**)env->uref.base;
	uint32_t top=env->uref.top;
	for(uint32_t i=0;i<top;i++)
	{
		FklUnReSymbolRef* cur=urefs[i];
		if(cur->id==id&&cur->scope==scope)
			return 1;
	}
	return 0;
}

static inline uint32_t get_next_empty(uint32_t empty,uint8_t* flags,uint32_t lcount)
{
	for(;empty<lcount&&flags[empty];empty++);
	return empty;
}

uint32_t fklAddCodegenDefBySid(FklSid_t id,uint32_t scopeId,FklCodegenEnv* env)
{
	FklCodegenEnvScope* scope=&env->scopes[scopeId-1];
	FklHashTable* ht=scope->defs;
	FklSidScope key={id,scopeId};
	FklSymbolDef* el=fklGetHashItem(&key,ht);
	if(!el)
	{
		uint32_t idx=scope->empty;
		el=fklPutHashItem(&key,ht);
		if(idx<env->lcount&&has_resolvable_ref(id,scopeId,env))
			idx=env->lcount;
		else
			scope->empty=get_next_empty(scope->empty+1,env->slotFlags,env->lcount);
		el->idx=idx;
		uint32_t end=(idx+1)-scope->start;
		if(scope->end<end)
			scope->end=end;
		if(idx>=env->lcount)
		{
			env->lcount=idx+1;
			uint8_t* slotFlags=(uint8_t*)fklRealloc(env->slotFlags,sizeof(uint8_t)*env->lcount);
			FKL_ASSERT(slotFlags);
			env->slotFlags=slotFlags;
		}
		env->slotFlags[idx]=FKL_CODEGEN_ENV_SLOT_OCC;
	}
	return el->idx;
}

void fklAddReplacementBySid(FklSid_t id,FklNastNode* node,FklHashTable* reps)
{
	FklCodegenReplacement* rep=fklPutHashItem(&id,reps);
	fklDestroyNastNode(rep->node);
	rep->node=fklMakeNastNodeRef(node);
}

int fklIsSymbolDefined(FklSid_t id,uint32_t scope,FklCodegenEnv* env)
{
	return get_def_by_id_in_scope(id,scope,&env->scopes[scope-1])!=NULL;
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
	FklNastNode* args=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	FklCodegenEnv* lambdaCodegenEnv=fklCreateCodegenEnv(curEnv,scope,macroScope);
	FklByteCodelnt* argsBc=processArgs(args,lambdaCodegenEnv,codegen);
	if(!argsBc)
	{
		errorState->fid=codegen->fid;
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
			,origExp->curline);
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
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	if(name->type!=FKL_NAST_SYM||isNonRetvalExp(value))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklPtrStack* stack=fklCreatePtrStack(2,1);
	uint32_t idx=fklAddCodegenDefBySid(name->sym,scope,curEnv);
	fklPushPtrStack(makePutLoc(name,idx,codegen),stack);
	fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_set_var_exp_bc_process
			,createDefaultStackContext(stack)
			,createDefaultQueueNextExpression(queue)
			,scope
			,macroScope
			,curEnv
			,name->curline
			,codegen
			,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_defun)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* args=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}

	FklCodegenEnv* lambdaCodegenEnv=fklCreateCodegenEnv(curEnv,scope,macroScope);
	FklByteCodelnt* argsBc=processArgs(args,lambdaCodegenEnv,codegen);
	if(!argsBc)
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		lambdaCodegenEnv->refcount++;
		fklDestroyCodegenEnv(lambdaCodegenEnv);
		return;
	}

	FklPtrStack* setqStack=fklCreatePtrStack(2,1);
	uint32_t idx=fklAddCodegenDefBySid(name->sym,scope,curEnv);
	fklPushPtrStack(makePutLoc(name,idx,codegen),setqStack);

	FklCodegenQuest* prevQuest=createCodegenQuest(_set_var_exp_bc_process
			,createDefaultStackContext(setqStack)
			,NULL
			,scope
			,macroScope
			,curEnv
			,name->curline
			,NULL
			,codegen);
	fklPushPtrStack(prevQuest,codegenQuestStack);

	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	FklPtrStack* lStack=fklCreatePtrStack(32,16);
	fklPushPtrStack(argsBc,lStack);
	create_and_insert_to_pool(codegen->pts
			,curEnv->prototypeId
			,lambdaCodegenEnv
			,codegen->globalSymTable
			,get_sid_in_gs_with_id_in_ps(name->sym,codegen->globalSymTable)
			,codegen->fid
			,origExp->curline);
	fklAddReplacementBySid(fklAddSymbolCstrToPst("*func*")->id
			,name
			,lambdaCodegenEnv->macros->replacements);
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
		,FklCodegen* codegen)
{
	FklSymbolDef* ref=NULL;
	while(env)
	{
		FklHashTableItem* list=env->refs->first;
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
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	if(name->type!=FKL_NAST_SYM||isNonRetvalExp(value))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklPtrStack* stack=fklCreatePtrStack(2,1);
	fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
	FklSymbolDef* def=fklFindSymbolDefByIdAndScope(name->sym,scope,curEnv);
	if(def)
		fklPushPtrStack(makePutLoc(name,def->idx,codegen),stack);
	else
	{
		uint32_t idx=fklAddCodegenRefBySid(name->sym,curEnv,codegen->fid,name->curline);
		mark_modify_builtin_ref(idx,curEnv,codegen);
		fklPushPtrStack(makePutRefLoc(name,idx,codegen),stack);
	}
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_set_var_exp_bc_process
			,createDefaultStackContext(stack)
			,createDefaultQueueNextExpression(queue)
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
		,FklCodegen* codegen)
{
	FklPtrStack* stack=fklCreatePtrStack(1,1);
	fklPushPtrStack(createBclnt(fklCodegenNode(value,codegen),codegen->fid,value->curline),stack);
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
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
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
	if(macro->own)
	{
		fklDestroyByteCodelnt(macro->bcl);
		fklDestroyFuncPrototypes(macro->pts);
	}
}

static void add_compiler_macro(FklCodegenMacro** pmacro
		,FklNastNode* pattern
		,FklByteCodelnt* bcl
		,FklFuncPrototypes* pts
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
		FklCodegenMacro* macro=fklCreateCodegenMacro(pattern,bcl,*phead,pts,own);
		*phead=macro;
	}
	else if(coverState==FKL_PATTERN_EQUAL)
	{
		FklCodegenMacro* macro=*pmacro;
		uninit_codegen_macro(macro);
		macro->own=own;
		macro->pts=pts;
		macro->pattern=pattern;
		macro->bcl=bcl;
	}
	else
	{
		FklCodegenMacro* next=*pmacro;
		*pmacro=fklCreateCodegenMacro(pattern,bcl,next,pts,own);
	}
}

BC_PROCESS(_export_macro_bc_process)
{
	FklCodegenMacroScope* target_macro_scope=cms->prev;
	for(;!codegen->libMark;codegen=codegen->prev);
	for(FklCodegenMacro* cur=cms->head;cur;cur=cur->next)
	{
		cur->own=0;
		add_compiler_macro(&codegen->export_macro,fklMakeNastNodeRef(cur->pattern),cur->bcl,cur->pts,1);
		add_compiler_macro(&target_macro_scope->head,fklMakeNastNodeRef(cur->pattern),cur->bcl,cur->pts,0);
	}
	for(FklHashTableItem* cur=cms->replacements->first;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->data;
		fklAddReplacementBySid(rep->id,rep->node,codegen->export_replacement);
		fklAddReplacementBySid(rep->id,rep->node,target_macro_scope->replacements);
	}
	return NULL;
}

static int isValidExportNodeList(const FklNastNode* list);
static int isExportDefmacroExp(const FklNastNode*);
static int isExportDefReaderMacroExp(const FklNastNode*);
static int isExportDefineExp(const FklNastNode* c);
static int isExportDefunExp(const FklNastNode* c);

static inline void add_export_macro_exp(FklNastNode* list
		,FklPtrQueue* exportQueue)
{
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr)
	{
		FklNastNode* cur=list->pair->car;
		if(cur->type!=FKL_NAST_SYM)
			fklPushPtrQueue(fklMakeNastNodeRef(cur),exportQueue);
	}
}

static int isExportImportExp(FklNastNode*);

static inline void add_export_symbol(FklCodegen* libCodegen
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
		,FklCodegen* codegen
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

static CODEGEN_FUNC(codegen_check)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklByteCodelnt* bcl=NULL;
	FklOpcode op=FKL_OP_PUSH_NIL;
	if(fklFindSymbolDefByIdAndScope(name->sym,scope,curEnv))
		op=FKL_OP_PUSH_1;
	bcl=createBclnt(create1lenBc(op),codegen->fid,origExp->curline);
	push_single_bcl_codegen_quest(bcl
			,codegenQuestStack
			,scope
			,macroScope
			,curEnv
			,NULL
			,codegen
			,origExp->curline);
}

static CODEGEN_FUNC(codegen_quote)
{
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
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
		,FklCodegen* codegen)
{
	FklPtrQueue* queue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
	FklCodegenQuest* quest=createCodegenQuest(func
			,createDefaultStackContext(fklCreatePtrStack(1,1))
			,createDefaultQueueNextExpression(queue)
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
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	if(isNonRetvalExp(value))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
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
	FklByteCode* pushBox=create1lenBc(FKL_OP_PUSH_BOX);
	fklBclBcAppendToBcl(retval,pushBox,fid,line);
	fklDestroyByteCode(pushBox);
	return retval;
}

BC_PROCESS(_qsquote_vec_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=stack->base[0];
	for(size_t i=1;i<stack->top;i++)
	{
		FklByteCodelnt* cur=stack->base[i];
		fklCodeLntCat(retval,cur);
		fklDestroyByteCodelnt(cur);
	}
	stack->top=0;
	FklByteCode* pushVec=create1lenBc(FKL_OP_PUSH_VECTOR_0);
	FklByteCode* setBp=create1lenBc(FKL_OP_SET_BP);
	fklBcBclAppendToBcl(setBp,retval,fid,line);
	fklBclBcAppendToBcl(retval,pushVec,fid,line);
	fklDestroyByteCode(setBp);
	fklDestroyByteCode(pushVec);
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
			fklCodeLntCat(other,cur);
			fklDestroyByteCodelnt(cur);
		}
		fklReCodeLntCat(other,retval);
		fklDestroyByteCodelnt(other);
	}
	FklByteCode* listPush=create1lenBc(FKL_OP_LIST_PUSH);
	fklBclBcAppendToBcl(retval,listPush,fid,line);
	fklDestroyByteCode(listPush);
	return retval;
}

BC_PROCESS(_qsquote_pair_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=stack->base[0];
	for(size_t i=1;i<stack->top;i++)
	{
		FklByteCodelnt* cur=stack->base[i];
		fklCodeLntCat(retval,cur);
		fklDestroyByteCodelnt(cur);
	}
	stack->top=0;
	FklByteCode* pushPair=create1lenBc(FKL_OP_PUSH_LIST_0);
	FklByteCode* setBp=create1lenBc(FKL_OP_SET_BP);
	fklBcBclAppendToBcl(setBp,retval,fid,line);
	fklBclBcAppendToBcl(retval,pushPair,fid,line);
	fklDestroyByteCode(setBp);
	fklDestroyByteCode(pushPair);
	return retval;
}

BC_PROCESS(_qsquote_list_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=stack->base[0];
	for(size_t i=1;i<stack->top;i++)
	{
		FklByteCodelnt* cur=stack->base[i];
		fklCodeLntCat(retval,cur);
		fklDestroyByteCodelnt(cur);
	}
	stack->top=0;
	FklByteCode* pushPair=create1lenBc(FKL_OP_LIST_APPEND);
	fklBclBcAppendToBcl(retval,pushPair,fid,line);
	fklDestroyByteCode(pushPair);
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
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	FklPtrStack valueStack=FKL_STACK_INIT;
	fklInitPtrStack(&valueStack,8,16);
	fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,value,NULL),&valueStack);
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
			if(fklPatternMatch(builtInSubPattern[SUB_PATTERN_UNQUOTE].pn,curValue,unquoteHt))
			{
				FklNastNode* unquoteValue=fklPatternMatchingHashTableRef(builtInPatternVar_value,unquoteHt);
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
					if(fklPatternMatch(builtInSubPattern[SUB_PATTERN_UNQUOTE].pn,node,unquoteHt))
					{
						FklNastNode* unquoteValue=fklPatternMatchingHashTableRef(builtInPatternVar_value,unquoteHt);
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
					if(fklPatternMatch(builtInSubPattern[SUB_PATTERN_UNQTESP].pn,cur,unquoteHt))
					{
						FklNastNode* unqtespValue=fklPatternMatchingHashTableRef(builtInPatternVar_value,unquoteHt);
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
					if(fklPatternMatch(builtInSubPattern[SUB_PATTERN_UNQTESP].pn,curValue->vec->base[i],unquoteHt))
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_UNQTESP_VEC
									,fklPatternMatchingHashTableRef(builtInPatternVar_value,unquoteHt)
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
		retval=createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
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

		uint8_t dropC[]={FKL_OP_DROP};
		FklByteCode drop={1,&dropC[0]};

		fklBcBclAppendToBcl(&drop,prev,fid,line);

		uint8_t jmpC[9]={FKL_OP_JMP};
		fklSetI64ToByteCode(&jmpC[1],prev->bc->size);
		FklByteCode jmp={9,jmpC};

		for(size_t i=2;i<stack->top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			fklBclBcAppendToBcl(retval,&drop,fid,line);
			fklCodeLntCat(retval,cur);
			fklDestroyByteCodelnt(cur);
		}

		close_ref_to_local_scope(retval,scope,env,codegen,fid,line);

		fklBclBcAppendToBcl(retval,&jmp,fid,line);
		uint8_t jmpIfFalseC[9]={FKL_OP_JMP_IF_FALSE};
		fklSetI64ToByteCode(&jmpIfFalseC[1],retval->bc->size);
		FklByteCode jmpIfFalse={9,jmpIfFalseC};

		fklBcBclAppendToBcl(&jmpIfFalse,retval,fid,line);
		fklCodeLntCat(retval,prev);
		fklDestroyByteCodelnt(prev);
		fklReCodeLntCat(first,retval);
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
	uint8_t dropC[]={FKL_OP_DROP};
	FklByteCode drop={1,&dropC[0]};

	FklByteCodelnt* retval=NULL;
	if(stack->top)
	{
		retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		FklByteCodelnt* first=stack->base[0];
		for(size_t i=1;i<stack->top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			fklBclBcAppendToBcl(retval,&drop,fid,line);
			fklCodeLntCat(retval,cur);
			fklDestroyByteCodelnt(cur);
		}
		if(retval->bc->size)
		{
			uint8_t jmpIfFalseC[9]={FKL_OP_JMP_IF_FALSE};
			fklSetI64ToByteCode(&jmpIfFalseC[1],retval->bc->size);
			FklByteCode jmpIfFalse={9,jmpIfFalseC};
			fklBcBclAppendToBcl(&jmpIfFalse,retval,fid,line);
		}
		fklReCodeLntCat(first,retval);
		fklDestroyByteCodelnt(first);

		if(!retval->bc->size)
		{
			fklDestroyByteCodelnt(retval);
			retval=createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
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
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
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
				errorState->fid=codegen->fid;
				errorState->type=FKL_ERR_SYNTAXERROR;
				errorState->place=fklMakeNastNodeRef(origExp);
				fklUninitPtrStack(&tmpStack);
				return;
			}
			FklNastNode* last=NULL;
			FklPtrQueue* curQueue=fklCreatePtrQueue();
			pushListItemToQueue(curExp,curQueue,&last);
			if(last->type!=FKL_NAST_NIL||isNonRetvalExp(fklFirstPtrQueue(curQueue)))
			{
				errorState->fid=codegen->fid;
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
					,createDefaultQueueNextExpression(curQueue)
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
			errorState->fid=codegen->fid;
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
		if(last->type!=FKL_NAST_NIL
				||(cond_exp_bc_process!=_local_exp_bc_process
					&&isNonRetvalExp(fklFirstPtrQueue(lastQueue))))
		{
			errorState->fid=codegen->fid;
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
					,createDefaultQueueNextExpression(lastQueue)
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
	uint8_t jmpIfFalseCode[9]={FKL_OP_JMP_IF_FALSE,0};
	FklByteCode jmpIfFalse={9,jmpIfFalseCode};
	uint8_t dropCode[]={FKL_OP_DROP};
	FklByteCode drop={1,dropCode};

	FklByteCodelnt* exp=fklPopPtrStack(stack);
	FklByteCodelnt* cond=fklPopPtrStack(stack);
	if(exp&&cond)
	{
		fklBcBclAppendToBcl(&drop,exp,fid,line);
		fklSetI64ToByteCode(&jmpIfFalseCode[1],exp->bc->size);
		fklBclBcAppendToBcl(cond,&jmpIfFalse,fid,line);
		fklCodeLntCat(cond,exp);
		fklDestroyByteCodelnt(exp);
		return cond;
	}
	else if(exp)
		return exp;
	else
		return createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
}

static CODEGEN_FUNC(codegen_if0)
{
	FklNastNode* cond=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	FklNastNode* exp=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	if(isNonRetvalExp(cond)||isNonRetvalExp(exp))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklPtrQueue* nextQueue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(cond),nextQueue);
	fklPushPtrQueue(fklMakeNastNodeRef(exp),nextQueue);

	uint32_t curScope=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);
	fklPushPtrStack(createCodegenQuest(_if_exp_bc_process_0
				,createDefaultStackContext(fklCreatePtrStack(2,2))
				,createDefaultQueueNextExpression(nextQueue)
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

	uint8_t jmpCode[9]={FKL_OP_JMP,0};
	FklByteCode jmp={9,jmpCode};
	uint8_t jmpIfFalseCode[9]={FKL_OP_JMP_IF_FALSE,0};
	FklByteCode jmpIfFalse={9,jmpIfFalseCode};

	uint8_t dropCode[]={FKL_OP_DROP};
	FklByteCode drop={1,dropCode};

	if(exp0&&cond&&exp1)
	{

		fklBcBclAppendToBcl(&drop,exp0,fid,line);
		fklBcBclAppendToBcl(&drop,exp1,fid,line);
		fklSetI64ToByteCode(&jmpCode[1],exp1->bc->size);
		fklBclBcAppendToBcl(exp0,&jmp,fid,line);
		fklSetI64ToByteCode(&jmpIfFalseCode[1],exp0->bc->size);
		fklBclBcAppendToBcl(cond,&jmpIfFalse,fid,line);
		fklCodeLntCat(cond,exp0);
		fklCodeLntCat(cond,exp1);
		fklDestroyByteCodelnt(exp0);
		fklDestroyByteCodelnt(exp1);
		return cond;
	}
	else if(exp0&&cond)
	{
		fklBcBclAppendToBcl(&drop,exp0,fid,line);
		fklSetI64ToByteCode(&jmpIfFalseCode[1],exp0->bc->size);
		fklBclBcAppendToBcl(cond,&jmpIfFalse,fid,line);
		fklCodeLntCat(cond,exp0);
		fklDestroyByteCodelnt(exp0);
		return cond;
	}
	else if(exp0)
		return exp0;
	else
		return createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
}

static CODEGEN_FUNC(codegen_if1)
{
	FklNastNode* cond=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	FklNastNode* exp0=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	FklNastNode* exp1=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);

	if(isNonRetvalExp(cond)||isNonRetvalExp(exp0)||isNonRetvalExp(exp1))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklPtrQueue* exp0Queue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(cond),exp0Queue);
	fklPushPtrQueue(fklMakeNastNodeRef(exp0),exp0Queue);

	FklPtrQueue* exp1Queue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(exp1),exp1Queue);

	uint32_t curScope=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);
	FklCodegenQuest* prev=createCodegenQuest(_if_exp_bc_process_1
			,createDefaultStackContext(fklCreatePtrStack(2,2))
			,createDefaultQueueNextExpression(exp0Queue)
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
				,createDefaultQueueNextExpression(exp1Queue)
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

		uint8_t singleOp[]={FKL_OP_DROP};
		FklByteCode drop={1,&singleOp[0]};
		for(size_t i=1;i<stack->top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			fklBclBcAppendToBcl(retval,&drop,fid,line);
			fklCodeLntCat(retval,cur);
			fklDestroyByteCodelnt(cur);
		}
		stack->top=0;
		uint8_t jmpIfFalseCode[9]={FKL_OP_JMP_IF_FALSE,0};
		FklByteCode jmpIfFalse={9,jmpIfFalseCode};
		fklSetI64ToByteCode(&jmpIfFalseCode[1],retval->bc->size);
		if(retval->bc->size)
			fklBcBclAppendToBcl(&jmpIfFalse,retval,fid,line);
		fklReCodeLntCat(cond,retval);
		fklDestroyByteCodelnt(cond);
		close_ref_to_local_scope(retval,scope,env,codegen,fid,line);
		return retval;
	}
	else
		return createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
}

BC_PROCESS(_unless_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top)
	{
		FklByteCodelnt* cond=stack->base[0];
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));

		uint8_t singleOp[]={FKL_OP_DROP};
		FklByteCode drop={1,&singleOp[0]};
		for(size_t i=1;i<stack->top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			fklBclBcAppendToBcl(retval,&drop,fid,line);
			fklCodeLntCat(retval,cur);
			fklDestroyByteCodelnt(cur);
		}
		stack->top=0;
		uint8_t jmpIfFalseCode[9]={FKL_OP_JMP_IF_TRUE,0};
		FklByteCode jmpIfFalse={9,jmpIfFalseCode};
		fklSetI64ToByteCode(&jmpIfFalseCode[1],retval->bc->size);
		if(retval->bc->size)
			fklBcBclAppendToBcl(&jmpIfFalse,retval,fid,line);
		fklReCodeLntCat(cond,retval);
		fklDestroyByteCodelnt(cond);
		close_ref_to_local_scope(retval,scope,env,codegen,fid,line);
		return retval;
	}
	else
		return createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
}

static inline void codegen_when_unless(FklHashTable* ht
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklCodegenEnv* curEnv
		,FklCodegen* codegen
		,FklPtrStack* codegenQuestStack
		,FklByteCodeProcesser func
		,FklCodegenErrorState* errorState
		,FklNastNode* origExp)
{
	FklNastNode* cond=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	if(isNonRetvalExp(cond))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(cond),queue);
	pushListItemToQueue(rest,queue,NULL);

	uint32_t curScope=enter_new_scope(scope,curEnv);
	FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(func
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createDefaultQueueNextExpression(queue)
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

static FklCodegen* createCodegen(FklCodegen* prev
		,const char* filename
		,FklCodegenEnv* env
		,int libMark
		,int macroMark)
{
	FklCodegen* r=(FklCodegen*)malloc(sizeof(FklCodegen));
	FKL_ASSERT(r);
	fklInitCodegener(r
			,filename
			,env
			,prev
			,prev->globalSymTable
			,1
			,libMark
			,macroMark);
	return r;
}

typedef struct
{
	FILE* fp;
	FklCodegen* codegen;
	FklPtrStack symbolStack;
	FklPtrStack stateStack;
}CodegenLoadContext;

#include<fakeLisp/grammer.h>

static CodegenLoadContext* createCodegenLoadContext(FILE* fp,FklCodegen* codegen)
{
	CodegenLoadContext* r=(CodegenLoadContext*)malloc(sizeof(CodegenLoadContext));
	FKL_ASSERT(r);
	r->codegen=codegen;
	r->fp=fp;
	fklInitPtrStack(&r->stateStack,16,16);
	fklInitPtrStack(&r->symbolStack,16,16);
	return r;
}

static void _codegen_load_finalizer(void* pcontext)
{
	CodegenLoadContext* context=pcontext;
	FklPtrStack* symbolStack=&context->symbolStack;
	while(!fklIsPtrStackEmpty(symbolStack))
		fklDestroyAnayzingSymbol(fklPopPtrStack(symbolStack));
	fklUninitPtrStack(symbolStack);
	fklUninitPtrStack(&context->stateStack);
	fclose(context->fp);
	free(context);
}

static inline FklNastNode* getExpressionFromFile(FklCodegen* codegen
		,FILE* fp
		,int* unexpectEOF
		,size_t* errorLine
		,FklCodegenErrorState* errorState
		,FklPtrStack* symbolStack
		,FklPtrStack* stateStack)
{
	size_t size=0;
	FklNastNode* begin=NULL;
	char* list=NULL;
	stateStack->top=0;
	if(codegen->g)
	{
		fklPushPtrStack(&codegen->g->aTable.states[0],stateStack);
		list=fklReadWithAnalysisTable(codegen->g
				,fp
				,&size
				,codegen->curline
				,&codegen->curline
				,fklGetPubSymTab()
				,unexpectEOF
				,errorLine
				,&begin
				,symbolStack
				,stateStack);
	}
	else
	{
		fklPushState0ToStack(stateStack);
		list=fklReadWithBuiltinParser(fp
				,&size
				,codegen->curline
				,&codegen->curline
				,fklGetPubSymTab()
				,unexpectEOF
				,errorLine
				,&begin
				,symbolStack
				,stateStack);
	}
	if(list)
		free(list);
	if(*unexpectEOF)
		begin=NULL;
	while(!fklIsPtrStackEmpty(symbolStack))
		fklDestroyAnayzingSymbol(fklPopPtrStack(symbolStack));
	return begin;
}

#include<fakeLisp/grammer.h>

static FklNastNode* _codegen_load_get_next_expression(void* pcontext,FklCodegenErrorState* errorState)
{
	CodegenLoadContext* context=pcontext;
	FklCodegen* codegen=context->codegen;
	FklPtrStack* stateStack=&context->stateStack;
	FklPtrStack* symbolStack=&context->symbolStack;
	FILE* fp=context->fp;
	int unexpectEOF=0;
	size_t errorLine=0;
	FklNastNode* begin=getExpressionFromFile(codegen
			,fp
			,&unexpectEOF
			,&errorLine
			,errorState
			,symbolStack
			,stateStack);
	if(unexpectEOF)
	{
		errorState->fid=codegen->fid;
		errorState->place=NULL;
		errorState->line=errorLine;
		errorState->type=unexpectEOF==FKL_PARSE_TERMINAL_MATCH_FAILED?FKL_ERR_UNEXPECTED_EOF:FKL_ERR_INVALIDEXPR;
		return NULL;
	}
	return begin;
}

static int hasLoadSameFile(const char* rpath,const FklCodegen* codegen)
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

static FklCodegenNextExpression* createFpNextExpression(FILE* fp,FklCodegen* codegen)
{
	CodegenLoadContext* context=createCodegenLoadContext(fp,codegen);
	return createCodegenNextExpression(&_codegen_load_get_next_expression_method_table
			,context);
}

static CODEGEN_FUNC(codegen_load)
{
	FklNastNode* filename=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	if(filename->type!=FKL_NAST_STR)
	{
		errorState->fid=codegen->fid;
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
	if(!fklIsAccessableRegFile(filenameStr->str))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_FILEFAILURE;
		errorState->place=fklMakeNastNodeRef(filename);
		return;
	}
	FklCodegen* nextCodegen=createCodegen(codegen,filenameStr->str,curEnv,0,0);
	if(hasLoadSameFile(nextCodegen->realpath,codegen))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_CIRCULARLOAD;
		errorState->place=fklMakeNastNodeRef(filename);
		nextCodegen->refcount=1;
		fklDestroyCodegener(nextCodegen);
		return;
	}
	FILE* fp=fopen(filenameStr->str,"r");
	if(!fp)
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_FILEFAILURE;
		errorState->place=fklMakeNastNodeRef(filename);
		nextCodegen->refcount=1;
		fklDestroyCodegener(nextCodegen);
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

static inline char* combineFileNameFromList(FklNastNode* list)
{
	char* r=NULL;
	for(FklNastNode* curPair=list;curPair->type==FKL_NAST_PAIR;curPair=curPair->pair->cdr)
	{
		FklNastNode* cur=curPair->pair->car;
		r=fklCstrStringCat(r,fklGetSymbolWithIdFromPst(cur->sym)->symbol);
		if(curPair->pair->cdr->type!=FKL_NAST_NIL)
			r=fklStrCat(r,FKL_PATH_SEPARATOR_STR);
	}
	return r;
}

static void add_symbol_to_local_env_in_array(FklCodegenEnv* env
		,FklSymbolTable* symbolTable
		,size_t num
		,FklSid_t* exports
		,uint32_t* exportIndex
		,FklByteCodelnt* bcl
		,FklSid_t fid
		,uint64_t line
		,uint32_t scope)
{
	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};
	if(exportIndex)
	{
		for(size_t i=0;i<num;i++)
		{
			FklSid_t sym=fklAddSymbolToPst(fklGetSymbolWithId(exports[i],symbolTable)->symbol)->id;
			uint32_t idx=fklAddCodegenDefBySid(sym,scope,env);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],exportIndex[i]);
			fklBclBcAppendToBcl(bcl,&bc,fid,line);
		}
	}
	else
	{
		for(size_t i=0;i<num;i++)
		{
			FklSid_t sym=fklAddSymbolToPst(fklGetSymbolWithId(exports[i],symbolTable)->symbol)->id;
			uint32_t idx=fklAddCodegenDefBySid(sym,scope,env);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],i);
			fklBclBcAppendToBcl(bcl,&bc,fid,line);
		}
	}
}

static void add_symbol_with_prefix_to_local_env_in_array(FklCodegenEnv* env
		,FklSymbolTable* symbolTable
		,FklString* prefix
		,size_t num
		,FklSid_t* exports
		,uint32_t* exportIndex
		,FklByteCodelnt* bcl
		,FklSid_t fid
		,uint64_t line
		,uint32_t scope)
{
	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};
	if(exportIndex)
	{
		for(size_t i=0;i<num;i++)
		{
			FklString* origSymbol=fklGetSymbolWithId(exports[i],symbolTable)->symbol;
			FklString* symbolWithPrefix=fklStringAppend(prefix,origSymbol);
			FklSid_t sym=fklAddSymbolToPst(symbolWithPrefix)->id;
			uint32_t idx=fklAddCodegenDefBySid(sym,scope,env);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],exportIndex[i]);
			fklBclBcAppendToBcl(bcl,&bc,fid,line);
			free(symbolWithPrefix);
		}
	}
	else
	{
		for(size_t i=0;i<num;i++)
		{
			FklString* origSymbol=fklGetSymbolWithId(exports[i],symbolTable)->symbol;
			FklString* symbolWithPrefix=fklStringAppend(prefix,origSymbol);
			FklSid_t sym=fklAddSymbolToPst(symbolWithPrefix)->id;
			uint32_t idx=fklAddCodegenDefBySid(sym,scope,env);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],i);
			fklBclBcAppendToBcl(bcl,&bc,fid,line);
			free(symbolWithPrefix);
		}
	}
}

// static inline void process_import_reader_macro(FklStringMatchPattern** phead,FklStringMatchPattern* head,int own)
// {
// 	for(FklStringMatchPattern* cur=head;cur&&cur->type!=FKL_STRING_PATTERN_BUILTIN;cur=cur->next)
// 	{
// 		if(own)
// 			cur->own=0;
// 		fklAddStringMatchPattern(cur->parts,cur->proc,phead,cur->pts,own);
// 	}
// }

static FklNastNode* createPatternWithPrefixFromOrig(FklNastNode* orig
		,FklString* prefix)
{
	FklString* head=fklGetSymbolWithIdFromPst(orig->pair->car->sym)->symbol;
	FklString* symbolWithPrefix=fklStringAppend(prefix,head);
	FklNastNode* patternWithPrefix=fklNastConsWithSym(fklAddSymbolToPst(symbolWithPrefix)->id
			,fklMakeNastNodeRef(orig->pair->cdr)
			,orig->curline
			,orig->pair->car->curline);
	free(symbolWithPrefix);
	return patternWithPrefix;
}

static inline void export_replacement_with_prefix(FklHashTable* replacements
		,FklCodegenMacroScope* macros
		,FklString* prefix)
{
	for(FklHashTableItem* cur=replacements->first;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->data;
		FklString* origSymbol=fklGetSymbolWithIdFromPst(rep->id)->symbol;
		FklString* symbolWithPrefix=fklStringAppend(prefix,origSymbol);
		FklSid_t id=fklAddSymbolToPst(symbolWithPrefix)->id;
		fklAddReplacementBySid(id,rep->node,macros->replacements);
		free(symbolWithPrefix);
	}
}

static inline void print_undefined_symbol(const FklPtrStack* urefs
		,FklSymbolTable* globalSymTable)
{
	for(uint32_t i=0;i<urefs->top;i++)
	{
		FklUnReSymbolRef* ref=urefs->base[i];
		fprintf(stderr,"warning in compiling: Symbol \"");
		fklPrintString(fklGetSymbolWithIdFromPst(ref->id)->symbol,stderr);
		fprintf(stderr,"\" is undefined at line %lu of ",ref->line);
		fklPrintString(fklGetSymbolWithId(ref->fid,globalSymTable)->symbol,stderr);
		fputc('\n',stderr);
	}
}

void fklPrintUndefinedRef(const FklCodegenEnv* env
		,FklSymbolTable* globalSymTable)
{
	print_undefined_symbol(&env->uref,globalSymTable);
}

typedef struct
{
	FklCodegen* codegen;
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
	fklDestroyCodegener(d->codegen);
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

static inline void recompute_ignores_terminal_sid(FklGrammerIgnore* igs,FklSymbolTable* target_table)
{
	FklGrammerIgnoreSym* syms=igs->ig;
	size_t len=igs->len;
	for(size_t i=len;i>0;i--)
	{
		size_t idx=i-1;
		FklGrammerIgnoreSym* cur=&syms[idx];
		if(cur->isbuiltin)
		{
			int failed=0;
			cur->b.c=cur->b.t->ctx_create(idx+1<len?syms[idx+1].str:NULL,&failed);
		}
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
		if(cur->isterm&&!cur->isbuiltin)
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

static inline int init_builtin_grammer(FklCodegen* codegen,FklSymbolTable* st);
static inline int add_group_prods(FklGrammer* g,const FklHashTable* prods);
static inline int add_group_ignores(FklGrammer* g,FklGrammerIgnore* igs);
static inline FklGrammerProductionGroup* add_production_group(FklHashTable* named_prod_groups,FklSid_t group_id);
static inline int add_all_group_to_grammer(FklCodegen* codegen);

static inline FklByteCodelnt* process_import_imported_lib_common(uint32_t libId
		,FklCodegen* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,uint64_t curline
		,FklCodegenErrorState* errorState)
{
	for(FklCodegenMacro* cur=lib->head;cur;cur=cur->next)
		add_compiler_macro(&macroScope->head
				,fklMakeNastNodeRef(cur->pattern)
				,cur->bcl
				,cur->pts
				,0);

	for(FklHashTableItem* cur=lib->replacements->first;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->data;
		fklAddReplacementBySid(rep->id,rep->node,macroScope->replacements);
	}

	if(lib->named_prod_groups.t)
	{
		if(!codegen->g)
		{
			if(init_builtin_grammer(codegen,fklGetPubSymTab()))
			{
reader_macro_error:
				errorState->line=curline;
				errorState->fid=codegen->fid;
				errorState->type=FKL_ERR_IMPORT_READER_MACRO_ERROR;
				errorState->place=NULL;
				return NULL;
			}
		}
		else
		{
			fklClearGrammer(codegen->g);
			if(add_group_ignores(codegen->g,codegen->builtin_ignores))
				goto reader_macro_error;

			if(add_group_prods(codegen->g,&codegen->builtin_prods))
				goto reader_macro_error;
		}

		FklGrammer* g=codegen->g;
		FklSymbolTable* origin_table=lib->terminal_table;
		for(FklHashTableItem* prod_group_item=lib->named_prod_groups.first;prod_group_item;prod_group_item=prod_group_item->next)
		{
			FklGrammerProductionGroup* group=(FklGrammerProductionGroup*)prod_group_item->data;
			FklGrammerProductionGroup* target_group=add_production_group(&codegen->named_prod_groups,group->id);

			for(FklGrammerIgnore* igs=group->ignore;igs;igs=igs->next)
			{
				FklGrammerIgnore* ig=(FklGrammerIgnore*)malloc(sizeof(FklGrammerIgnore)+igs->len*sizeof(FklGrammerIgnoreSym));
				FKL_ASSERT(ig);
				ig->len=igs->len;
				ig->next=NULL;
				memcpy(ig->ig,igs->ig,sizeof(FklGrammerIgnoreSym)*ig->len);

				recompute_ignores_terminal_sid(ig,g->terminals);
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
					recompute_prod_terminal_sid(prod,g->terminals,origin_table);
					if(fklAddProdToProdTable(&target_group->prods,&g->builtins,prod))
					{
						fklDestroyGrammerProduction(prod);
						goto reader_macro_error;
					}
				}
			}
		}
		if(add_all_group_to_grammer(codegen))
			goto reader_macro_error;
	}

	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);
	add_symbol_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,lib->exportNum
			,lib->exports
			,lib->exportIndex
			,importBc
			,codegen->fid
			,curline
			,scope);

	return importBc;
}

static inline FklByteCodelnt* process_import_imported_lib_prefix(uint32_t libId
		,FklCodegen* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,uint64_t curline
		,FklNastNode* prefixNode
		,FklCodegenErrorState* errorState)
{
	FklString* prefix=fklGetSymbolWithIdFromPst(prefixNode->sym)->symbol;
	for(FklCodegenMacro* cur=lib->head;cur;cur=cur->next)
		add_compiler_macro(&macroScope->head
				,createPatternWithPrefixFromOrig(cur->pattern
					,prefix)
				,cur->bcl
				,cur->pts
				,0);

	export_replacement_with_prefix(lib->replacements
			,macroScope
			,prefix);
	if(lib->named_prod_groups.t)
	{
		if(!codegen->g)
		{
			if(init_builtin_grammer(codegen,fklGetPubSymTab()))
			{
reader_macro_error:
				errorState->line=curline;
				errorState->fid=codegen->fid;
				errorState->type=FKL_ERR_IMPORT_READER_MACRO_ERROR;
				errorState->place=NULL;
				return NULL;
			}
		}
		else
		{
			fklClearGrammer(codegen->g);
			if(add_group_ignores(codegen->g,codegen->builtin_ignores))
				goto reader_macro_error;

			if(add_group_prods(codegen->g,&codegen->builtin_prods))
				goto reader_macro_error;
		}

		FklGrammer* g=codegen->g;
		FklSymbolTable* origin_table=lib->terminal_table;
		FklStringBuffer buffer;
		fklInitStringBuffer(&buffer);
		for(FklHashTableItem* prod_group_item=lib->named_prod_groups.first;prod_group_item;prod_group_item=prod_group_item->next)
		{
			FklGrammerProductionGroup* group=(FklGrammerProductionGroup*)prod_group_item->data;
			fklStringBufferConcatWithString(&buffer,prefix);
			fklStringBufferConcatWithString(&buffer,fklGetSymbolWithIdFromPst(group->id)->symbol);

			FklSid_t group_id_with_prefix=fklAddSymbolCharBufToPst(buffer.b,buffer.i)->id;

			FklGrammerProductionGroup* target_group=add_production_group(&codegen->named_prod_groups,group_id_with_prefix);

			for(FklGrammerIgnore* igs=group->ignore;igs;igs=igs->next)
			{
				FklGrammerIgnore* ig=(FklGrammerIgnore*)malloc(sizeof(FklGrammerIgnore)+igs->len*sizeof(FklGrammerIgnoreSym));
				FKL_ASSERT(ig);
				ig->len=igs->len;
				ig->next=NULL;
				memcpy(ig->ig,igs->ig,sizeof(FklGrammerIgnoreSym)*ig->len);

				recompute_ignores_terminal_sid(ig,g->terminals);
				if(fklAddIgnoreToIgnoreList(&target_group->ignore,ig))
				{
					fklUninitStringBuffer(&buffer);
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
					recompute_prod_terminal_sid(prod,g->terminals,origin_table);
					replace_group_id(prod,group_id_with_prefix);
					if(fklAddProdToProdTable(&target_group->prods,&g->builtins,prod))
					{
						fklUninitStringBuffer(&buffer);
						fklDestroyGrammerProduction(prod);
						goto reader_macro_error;
					}
				}
			}
			fklStringBufferClear(&buffer);
		}
		fklUninitStringBuffer(&buffer);
		if(add_all_group_to_grammer(codegen))
			goto reader_macro_error;
	}
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);
	add_symbol_with_prefix_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,prefix
			,lib->exportNum
			,lib->exports
			,lib->exportIndex
			,importBc
			,codegen->fid
			,curline
			,scope);

	return importBc;
}

static inline FklByteCodelnt* process_import_imported_lib_only(uint32_t libId
		,FklCodegen* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,uint64_t curline
		,FklNastNode* only
		,FklCodegenErrorState* errorState)
{
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	uint32_t exportNum=lib->exportNum;
	uint32_t* exportIndex=lib->exportIndex;
	FklSid_t* exportsP=lib->exports_in_pst;
	FklHashTable* replace=lib->replacements;
	FklCodegenMacro* head=lib->head;

	int is_inited_grammer=0;
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
						,macro->bcl
						,macro->pts
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
			if(!is_inited_grammer)
			{
				if(!codegen->g)
				{
					if(init_builtin_grammer(codegen,fklGetPubSymTab()))
					{
reader_macro_error:
						errorState->line=curline;
						errorState->fid=codegen->fid;
						errorState->type=FKL_ERR_IMPORT_READER_MACRO_ERROR;
						errorState->place=NULL;
						return NULL;
					}
				}
				else
				{
					fklClearGrammer(codegen->g);
					if(add_group_ignores(codegen->g,codegen->builtin_ignores))
						goto reader_macro_error;

					if(add_group_prods(codegen->g,&codegen->builtin_prods))
						goto reader_macro_error;
				}
				is_inited_grammer=1;
			}
			FklGrammer* g=codegen->g;
			FklSymbolTable* origin_table=lib->terminal_table;
			FklGrammerProductionGroup* group=fklGetHashItem(&cur,&lib->named_prod_groups);
			FklGrammerProductionGroup* target_group=add_production_group(&codegen->named_prod_groups,cur);

			for(FklGrammerIgnore* igs=group->ignore;igs;igs=igs->next)
			{
				FklGrammerIgnore* ig=(FklGrammerIgnore*)malloc(sizeof(FklGrammerIgnore)+igs->len*sizeof(FklGrammerIgnoreSym));
				FKL_ASSERT(ig);
				ig->len=igs->len;
				ig->next=NULL;
				memcpy(ig->ig,igs->ig,sizeof(FklGrammerIgnoreSym)*ig->len);

				recompute_ignores_terminal_sid(ig,g->terminals);
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
					recompute_prod_terminal_sid(prod,g->terminals,origin_table);
					if(fklAddProdToProdTable(&target_group->prods,&g->builtins,prod))
					{
						fklDestroyGrammerProduction(prod);
						goto reader_macro_error;
					}
				}
			}
		}

		uint32_t idl=0;
		for(;idl<exportNum&&exportsP[idl]!=cur;idl++);
		if(idl!=exportNum)
		{
			uint32_t idx=fklAddCodegenDefBySid(cur,scope,curEnv);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],exportIndex[idl]);
			fklBclBcAppendToBcl(importBc,&bc,codegen->fid,only->curline);
		}
		else if(!r)
		{
			errorState->type=FKL_ERR_IMPORT_MISSING;
			errorState->place=fklMakeNastNodeRef(only->pair->car);
			errorState->fid=codegen->fid;
			errorState->line=only->curline;
			break;
		}
	}

	if(is_inited_grammer&&add_all_group_to_grammer(codegen))
		goto reader_macro_error;
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
		,FklCodegen* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,uint64_t curline
		,FklNastNode* except
		,FklCodegenErrorState* errorState)
{
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	uint32_t exportNum=lib->exportNum;
	uint32_t* exportIndex=lib->exportIndex;

	FklSid_t* exportsP=lib->exports_in_pst;

	FklHashTable* replace=lib->replacements;
	FklCodegenMacro* head=lib->head;

	for(FklCodegenMacro* macro=head;macro;macro=macro->next)
	{
		FklNastNode* patternHead=macro->pattern->pair->car;
		if(!is_in_except_list(except,patternHead->sym))
		{
			add_compiler_macro(&macroScope->head
					,fklMakeNastNodeRef(macro->pattern)
					,macro->bcl
					,macro->pts
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
		if(!codegen->g)
		{
			if(init_builtin_grammer(codegen,fklGetPubSymTab()))
			{
reader_macro_error:
				errorState->line=curline;
				errorState->fid=codegen->fid;
				errorState->type=FKL_ERR_IMPORT_READER_MACRO_ERROR;
				errorState->place=NULL;
				return NULL;
			}
		}
		else
		{
			fklClearGrammer(codegen->g);
			if(add_group_ignores(codegen->g,codegen->builtin_ignores))
				goto reader_macro_error;

			if(add_group_prods(codegen->g,&codegen->builtin_prods))
				goto reader_macro_error;
		}

		FklGrammer* g=codegen->g;
		FklSymbolTable* origin_table=lib->terminal_table;
		for(FklHashTableItem* prod_group_item=lib->named_prod_groups.first;prod_group_item;prod_group_item=prod_group_item->next)
		{
			FklGrammerProductionGroup* group=(FklGrammerProductionGroup*)prod_group_item->data;
			if(!is_in_except_list(except,group->id))
			{
				FklGrammerProductionGroup* target_group=add_production_group(&codegen->named_prod_groups,group->id);

				for(FklGrammerIgnore* igs=group->ignore;igs;igs=igs->next)
				{
					FklGrammerIgnore* ig=(FklGrammerIgnore*)malloc(sizeof(FklGrammerIgnore)+igs->len*sizeof(FklGrammerIgnoreSym));
					FKL_ASSERT(ig);
					ig->len=igs->len;
					ig->next=NULL;
					memcpy(ig->ig,igs->ig,sizeof(FklGrammerIgnoreSym)*ig->len);

					recompute_ignores_terminal_sid(ig,g->terminals);
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
						recompute_prod_terminal_sid(prod,g->terminals,origin_table);
						if(fklAddProdToProdTable(&target_group->prods,&g->builtins,prod))
						{
							fklDestroyGrammerProduction(prod);
							goto reader_macro_error;
						}
					}
				}
			}
		}
		if(add_all_group_to_grammer(codegen))
			goto reader_macro_error;
	}
	for(uint32_t i=0;i<exportNum;i++)
	{
		if(!is_in_except_list(except,exportsP[i]))
		{
			uint32_t idx=fklAddCodegenDefBySid(exportsP[i],scope,curEnv);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],exportIndex[i]);
			fklBclBcAppendToBcl(importBc,&bc,codegen->fid,except->curline);
		}
	}
	return importBc;
}

static inline FklByteCodelnt* process_import_imported_lib_alias(uint32_t libId
		,FklCodegen* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,uint64_t curline
		,FklNastNode* alias
		,FklCodegenErrorState* errorState)
{
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	FklSymbolTable* globalSymTable=codegen->globalSymTable;

	uint32_t exportNum=lib->exportNum;
	uint32_t* exportIndex=lib->exportIndex;
	FklSid_t* exports=lib->exports;
	FklHashTable* replace=lib->replacements;
	FklCodegenMacro* head=lib->head;

	int is_inited_grammer=0;
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
						,macro->bcl
						,macro->pts
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
			if(!is_inited_grammer)
			{
				if(!codegen->g)
				{
					if(init_builtin_grammer(codegen,fklGetPubSymTab()))
					{
reader_macro_error:
						errorState->line=curline;
						errorState->fid=codegen->fid;
						errorState->type=FKL_ERR_IMPORT_READER_MACRO_ERROR;
						errorState->place=NULL;
						return NULL;
					}
				}
				else
				{
					fklClearGrammer(codegen->g);
					if(add_group_ignores(codegen->g,codegen->builtin_ignores))
						goto reader_macro_error;

					if(add_group_prods(codegen->g,&codegen->builtin_prods))
						goto reader_macro_error;
				}
				is_inited_grammer=1;
			}
			FklGrammer* g=codegen->g;
			FklSymbolTable* origin_table=lib->terminal_table;
			FklGrammerProductionGroup* group=fklGetHashItem(&cur,&lib->named_prod_groups);

			FklGrammerProductionGroup* target_group=add_production_group(&codegen->named_prod_groups,cur0);

			for(FklGrammerIgnore* igs=group->ignore;igs;igs=igs->next)
			{
				FklGrammerIgnore* ig=(FklGrammerIgnore*)malloc(sizeof(FklGrammerIgnore)+igs->len*sizeof(FklGrammerIgnoreSym));
				FKL_ASSERT(ig);
				ig->len=igs->len;
				ig->next=NULL;
				memcpy(ig->ig,igs->ig,sizeof(FklGrammerIgnoreSym)*ig->len);

				recompute_ignores_terminal_sid(ig,g->terminals);
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
					recompute_prod_terminal_sid(prod,g->terminals,origin_table);
					replace_group_id(prod,cur0);
					if(fklAddProdToProdTable(&target_group->prods,&g->builtins,prod))
					{
						fklDestroyGrammerProduction(prod);
						goto reader_macro_error;
					}
				}
			}
		}

		FklSid_t targetId=fklAddSymbol(fklGetSymbolWithIdFromPst(cur)->symbol,globalSymTable)->id;
		uint32_t idl=0;
		for(;idl<exportNum&&exports[idl]!=targetId;idl++);
		if(idl!=exportNum)
		{
			uint32_t idx=fklAddCodegenDefBySid(cur0,scope,curEnv);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],exportIndex[idl]);
			fklBclBcAppendToBcl(importBc,&bc,codegen->fid,alias->curline);
		}
		else if(!r)
		{
			errorState->type=FKL_ERR_IMPORT_MISSING;
			errorState->place=fklMakeNastNodeRef(curNode->pair->car);
			errorState->fid=codegen->fid;
			errorState->line=alias->curline;
			break;
		}
	}
	
	if(is_inited_grammer&&add_all_group_to_grammer(codegen))
		goto reader_macro_error;

	return importBc;
}

static inline FklByteCodelnt* process_import_imported_lib(uint32_t libId
		,FklCodegen* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* env
		,uint32_t scope
		,FklCodegenMacroScope* cms
		,uint64_t line
		,FklNastNode* prefix
		,FklNastNode* only
		,FklNastNode* alias
		,FklNastNode* except
		,FklCodegenErrorState* errorState)
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
				,errorState);
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
				,errorState);
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
			,errorState);
}

static inline int is_exporting_outer_ref_group(FklCodegen* codegen)
{
	for(FklHashTableItem* sid_list=codegen->export_named_prod_groups->first
			;sid_list
			;sid_list=sid_list->next)
	{
		FklSid_t id=*(FklSid_t*)sid_list->data;
		FklGrammerProductionGroup* group=fklGetHashItem(&id,&codegen->named_prod_groups);
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
		errorState->fid=codegen->fid;
		errorState->line=line;
		errorState->place=NULL;
		return NULL;
	}
	ExportContextData* data=context->data;
	FklByteCodelnt* libBc=sequnce_exp_bc_process(data->stack,fid,line);

	uint8_t exportOpBcCode[5]={FKL_OP_EXPORT,0};
	fklSetU32ToByteCode(&exportOpBcCode[1],codegen->loadedLibStack->top+1);
	FklByteCode exportOpBc={5,exportOpBcCode};
	fklBclBcAppendToBcl(libBc,&exportOpBc,fid,line);

	FklCodegenLib* lib=fklCreateCodegenScriptLib(codegen
			,libBc
			,env);

	fklUpdatePrototype(codegen->pts,env,codegen->globalSymTable);
	print_undefined_symbol(&env->uref,codegen->globalSymTable);

	fklPushPtrStack(lib,codegen->loadedLibStack);

	codegen->realpath=NULL;
	codegen->exports=NULL;
	codegen->exports_in_pst=NULL;
	codegen->export_macro=NULL;
	codegen->export_replacement=NULL;
	return process_import_imported_lib(codegen->loadedLibStack->top
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
				,errorState);
}

static FklCodegenQuestContext* createExportContext(FklCodegen* codegen
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

	FklHashTable* defs=env->scopes[0].defs;

	FklUintStack idStack=FKL_STACK_INIT;
	fklInitUintStack(&idStack,defs->num,16);

	FklUintStack idPstack=FKL_STACK_INIT;
	fklInitUintStack(&idPstack,defs->num,16);

	FklSymbolTable* globalSymTable=codegen->globalSymTable;

	for(FklHashTableItem* list=defs->first;list;list=list->next)
	{
		FklSymbolDef* def=(FklSymbolDef*)(list->data);
		FklSid_t idp=def->k.id;
		fklAddCodegenDefBySid(idp,1,targetEnv);
		FklSid_t id=fklAddSymbol(fklGetSymbolWithIdFromPst(idp)->symbol
					,globalSymTable)->id;
		fklPushUintStack(id,&idStack);
		fklPushUintStack(idp,&idPstack);
	}

	if(idStack.top)
	{
		uint32_t top=idStack.top;
		uint32_t num=codegen->exportNum+top;
		uint64_t* idBase=idStack.base;
		uint64_t* idPbase=idPstack.base;

		FklSid_t* exports=(FklSid_t*)fklRealloc(codegen->exports,num*sizeof(FklSid_t));
		FKL_ASSERT(exports);

		FklSid_t* exportsP=(FklSid_t*)fklRealloc(codegen->exports_in_pst,num*sizeof(FklSid_t));
		FKL_ASSERT(exportsP);

		FklSid_t* nes=&exports[codegen->exportNum];
		FklSid_t* nesp=&exportsP[codegen->exportNum];
		for(uint32_t i=0;i<top;i++)
		{
			nes[i]=idBase[i];
			nesp[i]=idPbase[i];
		}

		codegen->exports_in_pst=exportsP;
		codegen->exports=exports;
		codegen->exportNum=num;
	}

	fklUninitUintStack(&idStack);
	fklUninitUintStack(&idPstack);

	FklCodegenMacroScope* macros=targetEnv->macros;

	for(FklCodegenMacro* head=env->macros->head;head;head=head->next)
	{
		add_compiler_macro(&macros->head
				,fklMakeNastNodeRef(head->pattern)
				,head->bcl
				,head->pts,0);

		add_compiler_macro(&codegen->export_macro
				,fklMakeNastNodeRef(head->pattern)
				,head->bcl
				,head->pts,0);
	}

	for(FklHashTableItem* cur=env->macros->replacements->first;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->data;
		fklAddReplacementBySid(rep->id,rep->node,codegen->export_replacement);
		fklAddReplacementBySid(rep->id,rep->node,macros->replacements);
	}

	return bcl;
}

static CODEGEN_FUNC(codegen_export)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	if(!isValidExportNodeList(rest))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklPtrQueue* exportQueue=fklCreatePtrQueue();
	FklCodegen* libCodegen=codegen;
	for(;libCodegen&&!libCodegen->libMark;libCodegen=libCodegen->prev);
	if(libCodegen
			&&curEnv==codegen->globalEnv
			&&macroScope==codegen->globalEnv->macros)
	{
		FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
		add_export_symbol(libCodegen
				,origExp
				,rest
				,exportQueue);

		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_begin_exp_bc_process
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
		add_export_macro_exp(rest,exportQueue);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_begin_exp_bc_process
				,createDefaultStackContext(fklCreatePtrStack(2,1))
				,createDefaultQueueNextExpression(exportQueue)
				,scope
				,macroScope
				,curEnv
				,origExp->curline
				,codegen
				,codegenQuestStack);
	}
}

static inline int is_symbolId_in(FklSid_t* ida,uint32_t num,FklSid_t id)
{
	FklSid_t* end=ida+num;
	for(;ida<end;ida++)
		if(*ida==id)
			return 1;
	return 0;
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
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	FklSymbolTable* globalSymTable=codegen->globalSymTable;
	FklCodegen* libCodegen=codegen;
	for(;libCodegen&&!libCodegen->libMark;libCodegen=libCodegen->prev);

	int isInLib=libCodegen
		&&curEnv==codegen->globalEnv
		&&macroScope==codegen->globalEnv->macros;

	FklPtrQueue* queue=fklCreatePtrQueue();
	fklPushPtrQueue(fklMakeNastNodeRef(value),queue);

	FklNastNode* name=NULL;
	if(isExportDefunExp(value))
	{
		if(isInLib)
		{
			name=cadr_nast_node(value)->pair->car;
			goto process_def_in_lib;
		}
		goto process_as_begin;
	}
	else if(isExportDefineExp(value))
	{
		if(isInLib)
		{
			name=cadr_nast_node(value);
process_def_in_lib:
			if(name->type!=FKL_NAST_SYM)
				goto error;
			FklSid_t idp=name->sym;
			if(!is_symbolId_in(libCodegen->exports_in_pst,libCodegen->exportNum,idp))
			{
				FklSid_t id=fklAddSymbol(fklGetSymbolWithIdFromPst(idp)->symbol
						,globalSymTable)->id;

				uint32_t num=libCodegen->exportNum+1;

				FklSid_t* exports=(FklSid_t*)fklRealloc(libCodegen->exports,num*sizeof(FklSid_t));
				FKL_ASSERT(exports);

				FklSid_t* exportsP=(FklSid_t*)fklRealloc(libCodegen->exports_in_pst,num*sizeof(FklSid_t));
				FKL_ASSERT(exportsP);

				exports[libCodegen->exportNum]=id;
				exportsP[libCodegen->exportNum]=idp;

				libCodegen->exports_in_pst=exportsP;
				libCodegen->exports=exports;
				libCodegen->exportNum=num;
			}
		}
		goto process_as_begin;
	}
	else if(isExportDefmacroExp(value))
	{
		if(isInLib)
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
		else
		{
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
	}
	else if(isExportDefReaderMacroExp(value))
	{
		FklSid_t group_id=get_reader_macro_group_id(value);
		if(!group_id)
			goto error;
		if(isInLib)
			fklPutHashItem(&group_id,codegen->export_named_prod_groups);
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
	else if(isExportImportExp(value))
	{
		if(isInLib)
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
process_as_begin:
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
	}
	else
	{
error:
		fklDestroyNastNode(value);
		fklDestroyPtrQueue(queue);
		errorState->fid=codegen->fid;
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
		,FklCodegen* codegen
		,FklCodegenErrorState* errorState
		,FklPtrStack* codegenQuestStack
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklNastNode* prefix
		,FklNastNode* only
		,FklNastNode* alias
		,FklNastNode* except)
{
	FklCodegenEnv* libEnv=fklCreateCodegenEnv(NULL,0,NULL);
	FklCodegen* nextCodegen=createCodegen(codegen,filename,libEnv,1,0);

	fklInitGlobCodegenEnv(libEnv);

	if(hasLoadSameFile(nextCodegen->realpath,codegen))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_CIRCULARLOAD;
		errorState->place=fklMakeNastNodeRef(name);
		nextCodegen->refcount=1;
		fklDestroyCodegener(nextCodegen);
		return;
	}
	size_t libId=check_loaded_lib(nextCodegen->realpath,codegen->loadedLibStack);
	if(!libId)
	{
		FILE* fp=fopen(filename,"r");
		if(!fp)
		{
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			nextCodegen->refcount=1;
			fklDestroyCodegener(nextCodegen);
			return;
		}
		create_and_insert_to_pool(nextCodegen->pts
				,0
				,libEnv
				,nextCodegen->globalSymTable
				,0
				,nextCodegen->fid
				,1);

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
		FklCodegenLib* lib=codegen->loadedLibStack->base[libId-1];

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
				,errorState);
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

inline FklCodegenDllLibInitExportFunc fklGetCodegenInitExportFunc(FklDllHandle dll)
{
	return fklGetAddress("_fklExportSymbolInit",dll);
}

static inline FklByteCodelnt* process_import_from_dll_only(FklNastNode* origExp
		,FklNastNode* name
		,FklNastNode* only
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegen* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope)
{
	char* realpath=fklRealpath(filename);
	size_t libId=check_loaded_lib(realpath,codegen->loadedLibStack);
	FklDllHandle dll;
	FklCodegenLib* lib=NULL;
	if(!libId)
	{
		dll=fklLoadDll(realpath);
		if(!dll)
		{
			fprintf(stderr,"%s\n",dlerror());
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		FklCodegenDllLibInitExportFunc initExport=fklGetCodegenInitExportFunc(dll);
		if(!initExport)
		{
			fklDestroyDll(dll);
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_IMPORTFAILED;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		lib=fklCreateCodegenDllLib(realpath
				,dll
				,codegen->globalSymTable
				,initExport);
		fklPushPtrStack(lib,codegen->loadedLibStack);
		libId=codegen->loadedLibStack->top;
	}
	else
	{
		lib=codegen->loadedLibStack->base[libId-1];
		dll=lib->dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	FklSymbolTable* globalSymTable=codegen->globalSymTable;

	uint32_t exportNum=lib->exportNum;
	FklSid_t* exports=lib->exports;

	for(;only->type==FKL_NAST_PAIR;only=only->pair->cdr)
	{
		FklSid_t cur=only->pair->car->sym;
		FklSid_t targetId=fklAddSymbol(fklGetSymbolWithIdFromPst(cur)->symbol,globalSymTable)->id;
		uint32_t idl=0;
		for(;idl<exportNum&&exports[idl]!=targetId;idl++);
		uint32_t idx=fklAddCodegenDefBySid(cur,scope,curEnv);
		fklSetU32ToByteCode(&importCode[1],idx);
		fklSetU32ToByteCode(&importCode[5],idl);
		fklBclBcAppendToBcl(importBc,&bc,codegen->fid,only->curline);
	}

	return importBc;
}

static inline FklByteCodelnt* process_import_from_dll_except(FklNastNode* origExp
		,FklNastNode* name
		,FklNastNode* except
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegen* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope)
{
	char* realpath=fklRealpath(filename);
	size_t libId=check_loaded_lib(realpath,codegen->loadedLibStack);
	FklDllHandle dll;
	FklCodegenLib* lib=NULL;
	if(!libId)
	{
		dll=fklLoadDll(realpath);
		if(!dll)
		{
			fprintf(stderr,"%s\n",dlerror());
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		FklCodegenDllLibInitExportFunc initExport=fklGetCodegenInitExportFunc(dll);
		if(!initExport)
		{
			fklDestroyDll(dll);
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_IMPORTFAILED;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		lib=fklCreateCodegenDllLib(realpath
				,dll
				,codegen->globalSymTable
				,initExport);
		fklPushPtrStack(lib,codegen->loadedLibStack);
		libId=codegen->loadedLibStack->top;
	}
	else
	{
		lib=codegen->loadedLibStack->base[libId-1];
		dll=lib->dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	uint32_t exportNum=lib->exportNum;
	FklSid_t* exportsP=lib->exports_in_pst;

	for(uint32_t i=0;i<exportNum;i++)
	{
		if(!is_in_except_list(except,exportsP[i]))
		{
			uint32_t idx=fklAddCodegenDefBySid(exportsP[i],scope,curEnv);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],i);
			fklBclBcAppendToBcl(importBc,&bc,codegen->fid,except->curline);
		}
	}

	return importBc;
}

static inline FklByteCodelnt* process_import_from_dll_common(FklNastNode* origExp
		,FklNastNode* name
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegen* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope)
{
	char* realpath=fklRealpath(filename);
	size_t libId=check_loaded_lib(realpath,codegen->loadedLibStack);
	FklDllHandle dll;
	FklCodegenLib* lib=NULL;
	if(!libId)
	{
		dll=fklLoadDll(realpath);
		if(!dll)
		{
			fprintf(stderr,"%s\n",dlerror());
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		FklCodegenDllLibInitExportFunc initExport=fklGetCodegenInitExportFunc(dll);
		if(!initExport)
		{
			fklDestroyDll(dll);
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_IMPORTFAILED;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		lib=fklCreateCodegenDllLib(realpath
				,dll
				,codegen->globalSymTable
				,initExport);
		fklPushPtrStack(lib,codegen->loadedLibStack);
		libId=codegen->loadedLibStack->top;
	}
	else
	{
		lib=codegen->loadedLibStack->base[libId-1];
		dll=lib->dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);
	add_symbol_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,lib->exportNum
			,lib->exports
			,lib->exportIndex
			,importBc
			,codegen->fid
			,origExp->curline
			,scope);
	return importBc;
}

static inline FklByteCodelnt* process_import_from_dll_prefix(FklNastNode* origExp
		,FklNastNode* name
		,FklNastNode* prefixNode
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegen* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope)
{
	char* realpath=fklRealpath(filename);
	size_t libId=check_loaded_lib(realpath,codegen->loadedLibStack);
	FklDllHandle dll;
	FklCodegenLib* lib=NULL;
	if(!libId)
	{
		dll=fklLoadDll(realpath);
		if(!dll)
		{
			fprintf(stderr,"%s\n",dlerror());
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		FklCodegenDllLibInitExportFunc initExport=fklGetCodegenInitExportFunc(dll);
		if(!initExport)
		{
			fklDestroyDll(dll);
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_IMPORTFAILED;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		lib=fklCreateCodegenDllLib(realpath
				,dll
				,codegen->globalSymTable
				,initExport);
		fklPushPtrStack(lib,codegen->loadedLibStack);
		libId=codegen->loadedLibStack->top;
	}
	else
	{
		lib=codegen->loadedLibStack->base[libId-1];
		dll=lib->dll;
		free(realpath);
	}

	FklString* prefix=fklGetSymbolWithIdFromPst(prefixNode->sym)->symbol;

	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);
	add_symbol_with_prefix_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,prefix
			,lib->exportNum
			,lib->exports
			,lib->exportIndex
			,importBc
			,codegen->fid
			,origExp->curline
			,scope);

	return importBc;
}

static inline FklByteCodelnt* process_import_from_dll_alias(FklNastNode* origExp
		,FklNastNode* name
		,FklNastNode* alias
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegen* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope)
{
	char* realpath=fklRealpath(filename);
	size_t libId=check_loaded_lib(realpath,codegen->loadedLibStack);
	FklDllHandle dll;
	FklCodegenLib* lib=NULL;
	if(!libId)
	{
		dll=fklLoadDll(realpath);
		if(!dll)
		{
			fprintf(stderr,"%s\n",dlerror());
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		FklCodegenDllLibInitExportFunc initExport=fklGetCodegenInitExportFunc(dll);
		if(!initExport)
		{
			fklDestroyDll(dll);
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_IMPORTFAILED;
			errorState->place=fklMakeNastNodeRef(name);
			free(realpath);
			return NULL;
		}
		lib=fklCreateCodegenDllLib(realpath
				,dll
				,codegen->globalSymTable
				,initExport);
		fklPushPtrStack(lib,codegen->loadedLibStack);
		libId=codegen->loadedLibStack->top;
	}
	else
	{
		lib=codegen->loadedLibStack->base[libId-1];
		dll=lib->dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	FklSymbolTable* globalSymTable=codegen->globalSymTable;

	uint32_t exportNum=lib->exportNum;
	FklSid_t* exports=lib->exports;

	for(;alias->type==FKL_NAST_PAIR;alias=alias->pair->cdr)
	{
		FklNastNode* curNode=alias->pair->car;
		FklSid_t cur=curNode->pair->car->sym;
		FklSid_t cur0=curNode->pair->cdr->pair->car->sym;
		FklSid_t targetId=fklAddSymbol(fklGetSymbolWithIdFromPst(cur)->symbol,globalSymTable)->id;
		uint32_t idl=0;
		for(;idl<exportNum&&exports[idl]!=targetId;idl++);
		uint32_t idx=fklAddCodegenDefBySid(cur0,scope,curEnv);
		fklSetU32ToByteCode(&importCode[1],idx);
		fklSetU32ToByteCode(&importCode[5],idl);
		fklBclBcAppendToBcl(importBc,&bc,codegen->fid,alias->curline);
	}

	return importBc;
}

static inline FklByteCodelnt* process_import_from_dll(FklNastNode* origExp
		,FklNastNode* name
		,const char* filename
		,FklCodegenEnv* curEnv
		,FklCodegen* codegen
		,FklCodegenErrorState* errorState
		,uint32_t scope
		,FklNastNode* prefix
		,FklNastNode* only
		,FklNastNode* alias
		,FklNastNode* except)
{
	if(prefix)
		return process_import_from_dll_prefix(origExp
				,name
				,prefix
				,filename
				,curEnv
				,codegen
				,errorState
				,scope);
	if(only)
		return process_import_from_dll_only(origExp
				,name
				,only
				,filename
				,curEnv
				,codegen
				,errorState
				,scope);
	if(alias)
		return process_import_from_dll_alias(origExp
				,name
				,alias
				,filename
				,curEnv
				,codegen
				,errorState
				,scope);
	if(except)
		return process_import_from_dll_except(origExp
				,name
				,except
				,filename
				,curEnv
				,codegen
				,errorState
				,scope);
	return process_import_from_dll_common(origExp
			,name
			,filename
			,curEnv
			,codegen
			,errorState
			,scope);
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
		,FklCodegen* codegen
		,FklCodegenErrorState* errorState
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,FklPtrStack* codegenQuestStack
		,FklNastNode* prefix
		,FklNastNode* only
		,FklNastNode* alias
		,FklNastNode* except)
{
    if(name->type==FKL_NAST_NIL||!fklIsNastNodeListAndHasSameType(name,FKL_NAST_SYM)
            ||(prefix&&prefix->type!=FKL_NAST_SYM)
            ||(only&&!fklIsNastNodeListAndHasSameType(only,FKL_NAST_SYM))
            ||(except&&!fklIsNastNodeListAndHasSameType(except,FKL_NAST_SYM))
            ||(alias&&!is_valid_alias_sym_list(alias)))
    {
        errorState->fid=codegen->fid;
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

    char* filename=combineFileNameFromList(name);

    char* packageMainFileName=fklStrCat(fklCopyCstr(filename),FKL_PATH_SEPARATOR_STR);
    packageMainFileName=fklStrCat(packageMainFileName,"main.fkl");

    char* scriptFileName=fklStrCat(fklCopyCstr(filename),".fkl");

    char* dllFileName=fklStrCat(fklCopyCstr(filename),FKL_DLL_FILE_TYPE);

    if(fklIsAccessableRegFile(packageMainFileName))
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
				,except);
    else if(fklIsAccessableRegFile(scriptFileName))
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
				,except);
    else if(fklIsAccessableRegFile(dllFileName))
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
				,except);
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
        errorState->fid=codegen->fid;
        errorState->type=FKL_ERR_IMPORTFAILED;
        errorState->place=fklMakeNastNodeRef(name);
    }
    free(filename);
    free(scriptFileName);
    free(dllFileName);
    free(packageMainFileName);
}

static CODEGEN_FUNC(codegen_import)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);

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
			,NULL);
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
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* prefix=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);

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
			,NULL);
}

static CODEGEN_FUNC(codegen_import_only)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* only=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);

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
			,NULL);
}

static CODEGEN_FUNC(codegen_import_alias)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* alias=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);

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
			,NULL);
}

static CODEGEN_FUNC(codegen_import_except)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* except=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);

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
			,except);
}

typedef struct
{
	FklPtrStack* stack;
	FklNastNode* pattern;
	FklCodegenMacroScope* macroScope;
	FklFuncPrototypes* pts;
}MacroContext;

static inline MacroContext* createMacroContext(FklNastNode* pattern
		,FklCodegenMacroScope* macroScope
		,FklFuncPrototypes* pts)
{
	MacroContext* r=(MacroContext*)malloc(sizeof(MacroContext));
	FKL_ASSERT(r);
	r->pattern=fklMakeNastNodeRef(pattern);
	r->macroScope=macroScope?make_macro_scope_ref(macroScope):NULL;
	r->stack=fklCreatePtrStack(1,1);
	r->pts=pts;
	return r;
}

BC_PROCESS(_compiler_macro_bc_process)
{
	MacroContext* d=(MacroContext*)(context->data);
	FklPtrStack* stack=d->stack;
	FklByteCodelnt* macroBcl=fklPopPtrStack(stack);
	FklNastNode* pattern=d->pattern;
	FklCodegenMacroScope* macros=d->macroScope;
	FklFuncPrototypes* pts=d->pts;
	d->pts=NULL;

	fklUpdatePrototype(codegen->pts,env,codegen->globalSymTable);
	print_undefined_symbol(&env->uref,codegen->globalSymTable);
	add_compiler_macro(&macros->head
			,fklMakeNastNodeRef(pattern)
			,macroBcl
			,pts
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
	if(d->macroScope)
		fklDestroyCodegenMacroScope(d->macroScope);
	if(d->pts)
		fklDestroyFuncPrototypes(d->pts);
	free(d);
}

static const FklCodegenQuestContextMethodTable MacroStackContextMethodTable=
{
	.__get_bcl_stack=_macro_stack_context_get_bcl_stack,
	.__put_bcl=_macro_stack_context_put_bcl,
	.__finalizer=_macro_stack_context_finalizer,
};

static inline FklCodegenQuestContext* createMacroQuestContext(FklNastNode* pattern
		,FklCodegenMacroScope* macroScope
		,FklFuncPrototypes* pts)
{
	return createCodegenQuestContext(createMacroContext(pattern,macroScope,pts),&MacroStackContextMethodTable);
}

struct CustomActionCtx
{
	uint64_t refcount;
	FklByteCodelnt* bcl;
	FklFuncPrototypes* pts;
	FklPtrStack* macroLibStack;
};

struct ReaderMacroCtx
{
	FklPtrStack* stack;
	FklFuncPrototypes* pts;
	struct CustomActionCtx* action_ctx;
};

static FklNastNode* custom_action(void* c
		,FklNastNode* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	FklNastNode* nodes_vector=fklCreateNastNode(FKL_NAST_VECTOR,line);
	nodes_vector->vec=fklCreateNastVector(num);
	for(size_t i=0;i<num;i++)
		nodes_vector->vec->base[i]=fklMakeNastNodeRef(nodes[i]);
	FklHashTable lineHash;
	fklInitLineNumHashTable(&lineHash);
	FklHashTable ht;
	fklInitPatternMatchHashTable(&ht);
	fklPatternMatchingHashTableSet(fklAddSymbolCstrToPst("$$")->id,nodes_vector,&ht);
	struct CustomActionCtx* ctx=(struct CustomActionCtx*)c;

	FklNastNode* r=NULL;
	FklVM* anotherVM=fklInitMacroExpandVM(ctx->bcl,ctx->pts,&ht,&lineHash,ctx->macroLibStack,&r,line);
	FklVMgc* gc=anotherVM->gc;

	int e=fklRunVM(anotherVM);
	anotherVM->pts=NULL;
	if(e)
		fklDeleteCallChain(anotherVM);

	fklDestroyNastNode(nodes_vector);
	fklUninitHashTable(&ht);
	fklUninitHashTable(&lineHash);
	fklDestroyVMgc(gc);
	fklDestroyAllVMs(anotherVM);
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
		if(ctx->pts)
			fklDestroyFuncPrototypes(ctx->pts);
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
	FklPtrStack* stack=d->stack;
	FklByteCodelnt* macroBcl=fklPopPtrStack(stack);
	FklFuncPrototypes* pts=d->pts;
	struct CustomActionCtx* custom_ctx=d->action_ctx;
	d->pts=NULL;
	d->action_ctx=NULL;

	custom_action_ctx_destroy(custom_ctx);
	fklUpdatePrototype(codegen->pts,env,codegen->globalSymTable);
	print_undefined_symbol(&env->uref,codegen->globalSymTable);

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
	if(d->pts)
		fklDestroyFuncPrototypes(d->pts);
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

typedef FklNastNode* (*ReplacementFunc)(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen);

static FklNastNode* _nil_replacement(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen)
{
	return fklCreateNastNode(FKL_NAST_NIL,orig->curline);
}

static FklNastNode* _is_main_replacement(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen)
{
	FklNastNode* n=fklCreateNastNode(FKL_NAST_FIX,orig->curline);
	if(env->prototypeId==1)
		n->fix=1;
	else
		n->type=FKL_NAST_NIL;
	return n;
}

static FklNastNode* _file_dir_replacement(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen)
{
	FklString* s=NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_STR,orig->curline);
	if(codegen->filename==NULL)
		s=fklCreateStringFromCstr(fklGetCwd());
	else
		s=fklCreateStringFromCstr(codegen->curDir);
	fklStringCstrCat(&s,FKL_PATH_SEPARATOR_STR);
	r->str=s;
	return r;
}

static FklNastNode* _file_replacement(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen)
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

static FklNastNode* _line_replacement(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen)
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
	FklSid_t sid;
	ReplacementFunc func;
}builtInSymbolReplacement[]=
{
	{"nil",        0, _nil_replacement,      },
	{"*line*",     0, _line_replacement,     },
	{"*file*",     0, _file_replacement,     },
	{"*file-dir*", 0, _file_dir_replacement, },
	{"*main?*",    0, _is_main_replacement,  },
	{NULL,         0, NULL,                  },
};

static ReplacementFunc findBuiltInReplacementWithId(FklSid_t id)
{
	for(struct SymbolReplacement* cur=&builtInSymbolReplacement[0];cur->s!=NULL;cur++)
		if(cur->sid==id)
			return cur->func;
	return NULL;
}

static FklNastNode* codegen_prod_action_nil(void* ctx
		,FklNastNode* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	return fklCreateNastNode(FKL_NAST_NIL,line);
}

static FklNastNode* codegen_prod_action_first(void* ctx
		,FklNastNode* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	if(num<1)
		return NULL;
	return fklMakeNastNodeRef(nodes[0]);
}

static FklNastNode* codegen_prod_action_symbol(void* ctx
		,FklNastNode* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	if(num<1)
		return NULL;
    FklNastNode* node=nodes[0];
    if(node->type!=FKL_NAST_STR)
        return NULL;
    FklNastNode* sym=fklCreateNastNode(FKL_NAST_SYM,node->curline);
    sym->sym=fklAddSymbol(node->str,st)->id;
	return sym;
}

static FklNastNode* codegen_prod_action_second(void* ctx
		,FklNastNode* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	if(num<2)
		return NULL;
	return fklMakeNastNodeRef(nodes[1]);
}

static FklNastNode* codegen_prod_action_third(void* ctx
		,FklNastNode* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	if(num<3)
		return NULL;
	return fklMakeNastNodeRef(nodes[2]);
}

static inline FklNastNode* codegen_prod_action_pair(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static inline FklNastNode* codegen_prod_action_cons(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static inline FklNastNode* codegen_prod_action_box(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	if(num<2)
		return NULL;
	FklNastNode* box=fklCreateNastNode(FKL_NAST_BOX,line);
	box->box=fklMakeNastNodeRef(nodes[1]);
	return box;
}

static inline FklNastNode* codegen_prod_action_vector(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static inline FklNastNode* codegen_prod_action_quote(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	if(num<2)
		return NULL;
	FklSid_t id=fklAddSymbolCstr("quote",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline FklNastNode* codegen_prod_action_unquote(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	if(num<2)
		return NULL;
	FklSid_t id=fklAddSymbolCstr("unquote",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline FklNastNode* codegen_prod_action_qsquote(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	if(num<2)
		return NULL;
	FklSid_t id=fklAddSymbolCstr("qsquote",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline FklNastNode* codegen_prod_action_unqtesp(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	if(num<2)
		return NULL;
	FklSid_t id=fklAddSymbolCstr("unqtesp",st)->id;
	FklNastNode* s_exp=fklMakeNastNodeRef(nodes[1]);
	FklNastNode* head=fklCreateNastNode(FKL_NAST_SYM,line);
	head->sym=id;
	FklNastNode* s_exps[]={head,s_exp};
	return create_nast_list(s_exps,2,line);
}

static inline FklNastNode* codegen_prod_action_hasheq(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static inline FklNastNode* codegen_prod_action_hasheqv(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static inline FklNastNode* codegen_prod_action_hashequal(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static inline FklNastNode* codegen_prod_action_bytevector(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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
	FklSid_t id;
	FklBuiltinProdAction func;
}CodegenProdActions[]=
{
	{"nil",       0, codegen_prod_action_nil,       },
	{"symbol",    0, codegen_prod_action_symbol,    },
	{"first",     0, codegen_prod_action_first,     },
	{"second",    0, codegen_prod_action_second,    },
	{"third",     0, codegen_prod_action_third,     },
	{"pair",      0, codegen_prod_action_pair,      },
	{"cons",      0, codegen_prod_action_cons,      },
	{"box",       0, codegen_prod_action_box,       },
	{"vector",    0, codegen_prod_action_vector,    },
	{"quote",     0, codegen_prod_action_quote,     },
	{"unquote",   0, codegen_prod_action_unquote,   },
	{"qsquote",   0, codegen_prod_action_qsquote,   },
	{"unqtesp",   0, codegen_prod_action_unqtesp,   },
	{"hasheq",    0, codegen_prod_action_hasheq,    },
	{"hasheqv",   0, codegen_prod_action_hasheqv,   },
	{"hashequal", 0, codegen_prod_action_hashequal, },
	{"bytevector",0, codegen_prod_action_bytevector, },
	{NULL,        0, NULL,                          },
};

static inline void init_builtin_prod_action_list(void)
{
	for(struct CstrIdProdAction* l=CodegenProdActions;l->name;l++)
		l->id=fklAddSymbolCstrToPst(l->name)->id;
}

static inline FklBuiltinProdAction find_builtin_prod_action(FklSid_t id)
{
	for(struct CstrIdProdAction* l=CodegenProdActions;l->name;l++)
		if(l->id==id)
			return l->func;
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

static FklNastNode* simple_action_nth(void* ctx
		,FklNastNode* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static FklNastNode* simple_action_cons(void* c
		,FklNastNode* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static FklNastNode* simple_action_head(void* c
		,FklNastNode* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static inline FklNastNode* simple_action_box(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
{
	uintptr_t nth=(uintptr_t)ctx;
	if(nth>=num)
		return NULL;
	FklNastNode* box=fklCreateNastNode(FKL_NAST_BOX,line);
	box->box=fklMakeNastNodeRef(nodes[nth]);
	return box;
}

static inline FklNastNode* simple_action_vector(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static inline FklNastNode* simple_action_hasheq(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static inline FklNastNode* simple_action_hasheqv(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static inline FklNastNode* simple_action_hashequal(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static inline FklNastNode* simple_action_bytevector(void* ctx
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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

static FklNastNode* simple_action_symbol(void* c
		,FklNastNode* nodes[]
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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
		FklSid_t id=fklAddSymbolCharBuf(buffer.b,buffer.i,st)->id;
		sym=fklCreateNastNode(FKL_NAST_SYM,node->curline);
		sym->sym=id;
		fklUninitStringBuffer(&buffer);
	}
	else
	{
		FklNastNode* sym=fklCreateNastNode(FKL_NAST_SYM,node->curline);
		sym->sym=fklAddSymbol(node->str,st)->id;
	}
	return sym;
}

static inline FklNastNode* simple_action_string(void* c
		,FklNastNode** nodes
		,size_t num
		,size_t line
		,FklSymbolTable* st)
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
	FklSid_t id;
	FklBuiltinProdAction func;
	void* (*creator)(FklNastNode* rest[],size_t rest_len,int* failed);
	void* (*ctx_copyer)(const void*);
	void (*ctx_destroy)(void*);
}CodegenProdCreatorActions[]=
{
	{"nth",        0, simple_action_nth,        simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"cons",       0, simple_action_cons,       simple_action_cons_create,   simple_action_cons_copy,   fklProdCtxDestroyFree,        },
	{"head",       0, simple_action_head,       simple_action_head_create,   simple_action_head_copy,   simple_action_head_destroy,   },
	{"symbol",     0, simple_action_symbol,     simple_action_symbol_create, simple_action_symbol_copy, simple_action_symbol_destroy, },
	{"string",     0, simple_action_string,     simple_action_symbol_create, simple_action_symbol_copy, simple_action_symbol_destroy, },
	{"box",        0, simple_action_box,        simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"vector",     0, simple_action_vector,     simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"hasheq",     0, simple_action_hasheq,     simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"hasheqv",    0, simple_action_hasheqv,    simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"hashequal",  0, simple_action_hashequal,  simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{"bytevector", 0, simple_action_bytevector, simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing,   },
	{NULL,         0, NULL,                     NULL,                        NULL,                      NULL,                         },
};

static inline void init_simple_prod_action_list(void)
{
	for(struct CstrIdCreatorProdAction* l=CodegenProdCreatorActions;l->name;l++)
		l->id=fklAddSymbolCstrToPst(l->name)->id;
}

static inline struct CstrIdCreatorProdAction* find_simple_prod_action(FklSid_t id)
{
	for(struct CstrIdCreatorProdAction* l=CodegenProdCreatorActions;l->name;l++)
		if(l->id==id)
			return l;
	return NULL;
}

static inline FklGrammerSym* nast_vector_to_production_right_part(const FklNastVector* vec
		,FklHashTable* builtin_term
		,FklSymbolTable* tt
		,size_t* num
		,int* failed)
{
	FklGrammerSym* retval=NULL;

	FklPtrStack valid_items;
	fklInitPtrStack(&valid_items,vec->size,8);

	uint8_t* delim=(uint8_t*)malloc(sizeof(uint8_t)*vec->size);
	FKL_ASSERT(delim);
	memset(delim,1,sizeof(uint8_t)*vec->size);

	uint8_t* end_with_terminal=(uint8_t*)malloc(sizeof(uint8_t)*vec->size);
	memset(end_with_terminal,0,sizeof(uint8_t)*vec->size);

	for(size_t i=0;i<vec->size;i++)
	{
		const FklNastNode* cur=vec->base[i];
		if(cur->type==FKL_NAST_STR)
			fklPushPtrStack((void*)cur,&valid_items);
		else if(cur->type==FKL_NAST_SYM)
			fklPushPtrStack((void*)cur,&valid_items);
		else if(cur->type==FKL_NAST_NIL)
		{
			delim[valid_items.top]=0;
			continue;
		}
		else if(cur->type==FKL_NAST_FIX&&cur->fix==1)
		{
			end_with_terminal[valid_items.top]=1;
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
				ss->isbuiltin=0;
				ss->nt.group=0;
				ss->nt.sid=fklAddSymbol(cur->str,tt)->id;
				ss->end_with_terminal=end_with_terminal[i];
			}
			else if(cur->type==FKL_NAST_PAIR)
			{
				ss->isterm=0;
				ss->isbuiltin=0;
				ss->nt.group=cur->pair->car->sym;
				ss->nt.sid=cur->pair->cdr->sym;
			}
			else
			{
				if(fklGetBuiltinMatch(builtin_term,cur->sym))
				{
					ss->isterm=1;
					ss->isbuiltin=1;
					ss->b.t=fklGetBuiltinMatch(builtin_term,cur->sym);
					ss->b.c=NULL;
				}
				else
				{
					ss->isterm=0;
					ss->isbuiltin=0;
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
	free(end_with_terminal);
	return retval;
}

static inline FklGrammerIgnore* nast_vector_to_ignore(const FklNastVector* vec
		,FklHashTable* builtin_term
		,FklSymbolTable* tt)
{
	size_t num=0;
	int failed=0;
	FklGrammerSym* syms=nast_vector_to_production_right_part(vec
			,builtin_term
			,tt
			,&num
			,&failed);
	if(failed)
		return NULL;
	FklGrammerIgnore* ig=fklGrammerSymbolsToIgnore(syms,num,tt);
	fklUninitGrammerSymbols(syms,num);
	free(syms);
	return ig;
}

static inline FklCodegen* macro_compile_prepare(FklCodegen* codegen
		,FklCodegenEnv* curEnv
		,FklCodegenMacroScope* macroScope
		,FklHashTable* symbolSet
		,FklCodegenEnv** pmacroEnv)
{
	FklCodegenEnv* macroEnv=fklCreateCodegenEnv(curEnv,0,macroScope);

	for(FklHashTableItem* list=symbolSet->first
			;list
			;list=list->next)
	{
		FklSid_t* id=(FklSid_t*)list->data;
		fklAddCodegenDefBySid(*id,1,macroEnv);
	}
	FklCodegen* macroCodegen=createCodegen(codegen
			,NULL
			,macroEnv
			,0
			,1);
	macroCodegen->builtinSymModiMark=fklGetBuiltinSymbolModifyMark(&macroCodegen->builtinSymbolNum);
	fklDestroyCodegenEnv(macroEnv->prev);
	macroEnv->prev=NULL;
	free(macroCodegen->curDir);
	macroCodegen->curDir=fklCopyCstr(codegen->curDir);
	macroCodegen->filename=fklCopyCstr(codegen->filename);
	macroCodegen->realpath=fklCopyCstr(codegen->realpath);
	macroCodegen->pts=fklCreateFuncPrototypes(0);

	macroCodegen->globalSymTable=fklGetPubSymTab();
	macroCodegen->fid=macroCodegen->filename?fklAddSymbolCstrToPst(macroCodegen->filename)->id:0;
	macroCodegen->loadedLibStack=macroCodegen->macroLibStack;
	fklInitGlobCodegenEnv(macroEnv);

	*pmacroEnv=macroEnv;
	return macroCodegen;
}

static inline FklGrammerProduction* nast_vector_to_production(const FklNastVector* vec
		,FklHashTable* builtin_term
		,FklSymbolTable* tt
		,FklSid_t left_group
		,FklSid_t sid
		,FklSid_t action_type
		,FklNastNode* action_ast
		,FklSid_t group_id
		,FklCodegen* codegen
		,FklCodegenEnv* curEnv
		,FklCodegenMacroScope* macroScope
		,FklPtrStack* codegenQuestStack)
{
	int failed=0;
	size_t len=0;
	FklGrammerSym* syms=nast_vector_to_production_right_part(vec
			,builtin_term
			,tt
			,&len
			,&failed);
	if(failed)
		return NULL;
	if(action_type==builtInPatternVar_builtin)
	{
		if(action_ast->type!=FKL_NAST_SYM)
			return NULL;
		FklBuiltinProdAction action=find_builtin_prod_action(action_ast->sym);
		if(!action)
			return NULL;
		return fklCreateProduction(left_group
				,sid
				,len
				,syms
				,NULL
				,action
				,NULL
				,fklProdCtxDestroyDoNothing
				,fklProdCtxCopyerDoNothing);
	}
	else if(action_type==builtInPatternVar_simple)
	{
		if(action_ast->type!=FKL_NAST_VECTOR
				||!action_ast->vec->size
				||action_ast->vec->base[0]->type!=FKL_NAST_SYM)
			return NULL;
		struct CstrIdCreatorProdAction* action=find_simple_prod_action(action_ast->vec->base[0]->sym);
		if(!action)
			return NULL;
		int failed=0;
		void* ctx=action->creator(&action_ast->vec->base[1],action_ast->vec->size-1,&failed);
		if(failed)
			return NULL;
		return fklCreateProduction(left_group
				,sid
				,len
				,syms
				,NULL
				,action->func
				,ctx
				,action->ctx_destroy
				,action->ctx_copyer);
	}
	else if(action_type==builtInPatternVar_custom)
	{
		if(isNonRetvalExp(action_ast))
			return NULL;
		FklHashTable symbolSet;
		fklInitSidSet(&symbolSet);
		fklPutHashItem(&fklAddSymbolCstrToPst("$$")->id,&symbolSet);
		FklCodegenEnv* macroEnv=NULL;
		FklCodegen* macroCodegen=macro_compile_prepare(codegen
				,curEnv
				,macroScope
				,&symbolSet
				,&macroEnv);
		fklUninitHashTable(&symbolSet);

		create_and_insert_to_pool(macroCodegen->pts
				,0
				,macroEnv
				,macroCodegen->globalSymTable
				,0
				,macroCodegen->fid
				,action_ast->curline);
		FklPtrQueue* queue=fklCreatePtrQueue();
		struct CustomActionCtx* ctx=(struct CustomActionCtx*)calloc(1,sizeof(struct CustomActionCtx));
		FKL_ASSERT(ctx);
		ctx->macroLibStack=codegen->macroLibStack;

		fklPushPtrQueue(fklMakeNastNodeRef(action_ast),queue);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_reader_macro_bc_process
				,createReaderMacroQuestContext(ctx,macroCodegen->pts)
				,createDefaultQueueNextExpression(queue)
				,1
				,macroEnv->macros
				,macroEnv
				,action_ast->curline
				,macroCodegen
				,codegenQuestStack);
		return fklCreateProduction(left_group
				,sid
				,len
				,syms
				,NULL
				,custom_action
				,ctx
				,custom_action_ctx_destroy
				,custom_action_ctx_copy);
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

static inline int add_all_ignores_to_grammer(FklCodegen* codegen)
{
	FklGrammer* g=codegen->g;

	if(add_group_ignores(g,codegen->unnamed_ignores))
		return 1;

	for(FklHashTableItem* group_items=codegen->named_prod_groups.first
			;group_items
			;group_items=group_items->next)
	{
		FklGrammerProductionGroup* group=(FklGrammerProductionGroup*)group_items->data;
		if(add_group_ignores(g,group->ignore))
			return 1;
	}
	return 0;
}

static inline int add_all_start_prods_to_grammer(FklCodegen* codegen)
{
	FklGrammer* g=codegen->g;
	if(add_start_prods_to_grammer(g,&codegen->unnamed_prods))
		return 1;

	for(FklHashTableItem* group_items=codegen->named_prod_groups.first
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

static inline int scan_and_add_reachable_productions(FklCodegen* codegen)
{
	FklGrammer* g=codegen->g;
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
						FklGrammerProductionGroup* group=fklGetHashItem(&cur->nt.group,&codegen->named_prod_groups);
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
						FklGrammerProduction* prods=fklGetProductions(&codegen->unnamed_prods,cur->nt.group,cur->nt.sid);
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

static inline int add_all_group_to_grammer(FklCodegen* codegen)
{
	if(add_all_ignores_to_grammer(codegen))
		return 1;

	if(add_all_start_prods_to_grammer(codegen))
		return 1;

	if(scan_and_add_reachable_productions(codegen))
		return 1;

	FklGrammer* g=codegen->g;

	if(fklInitGrammer(g))
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

static inline FklGrammerProductionGroup* add_production_group(FklHashTable* named_prod_groups,FklSid_t group_id)
{
	FklGrammerProductionGroup group_init={.id=group_id,.ignore=NULL,.is_ref_outer=0,.prods.t=NULL};
	FklGrammerProductionGroup* group=fklGetOrPutHashItem(&group_init,named_prod_groups);
	if(!group->prods.t)
		fklInitGrammerProductionTable(&group->prods);
	return group;
}

static inline int init_builtin_grammer(FklCodegen* codegen,FklSymbolTable* st)
{
	codegen->g=fklCreateEmptyGrammer(st);
	fklInitGrammerProductionTable(&codegen->builtin_prods);
	fklInitGrammerProductionTable(&codegen->unnamed_prods);
	fklInitHashTable(&codegen->named_prod_groups,&GrammerProductionMetaTable);
	codegen->unnamed_ignores=NULL;
	codegen->builtin_ignores=fklInitBuiltinProductionSet(&codegen->builtin_prods
			,st
			,codegen->g->terminals
			,&codegen->g->builtins);
	FklGrammer* g=codegen->g;
	g->ignores=NULL;

	fklClearGrammer(g);
	if(add_group_ignores(g,codegen->builtin_ignores))
		return 1;

	if(add_group_prods(g,&codegen->builtin_prods))
		return 1;
	return 0;
}

static inline int check_group_outer_ref(FklCodegen* codegen
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
						else if(fklGetHashItem(&left,&codegen->unnamed_prods))
							is_ref_outer=1;
					}
				}
			}
		}
	}
	return is_ref_outer;
}

static inline FklGrammerProduction* create_extra_start_production(FklSid_t group,FklSid_t sid)
{
	FklGrammerProduction* prod=fklCreateEmptyProduction(0,0,1,NULL,NULL,NULL
			,fklProdCtxDestroyDoNothing
			,fklProdCtxCopyerDoNothing);
	prod->func=codegen_prod_action_first;
	prod->idx=0;
	FklGrammerSym* u=&prod->syms[0];
	u->delim=1;
	u->isbuiltin=0;
	u->isterm=0;
	u->nt.group=group;
	u->nt.sid=sid;
	return prod;
}

static inline int process_add_production(FklSid_t group_id
		,FklCodegen* codegen
		,FklNastNode* vector_node
		,FklCodegenErrorState* errorState
		,FklCodegenEnv* curEnv
		,FklCodegenMacroScope* macroScope
		,FklPtrStack* codegenQuestStack)
{
	FklSymbolTable* st=fklGetPubSymTab();
	if(!codegen->g)
	{
		if(init_builtin_grammer(codegen,st))
			goto reader_macro_error;
	}
	else
	{
		fklClearGrammer(codegen->g);
		if(add_group_ignores(codegen->g,codegen->builtin_ignores))
			goto reader_macro_error;

		if(add_group_prods(codegen->g,&codegen->builtin_prods))
			goto reader_macro_error;
	}
	FklNastVector* vec=vector_node->vec;
	if(vec->size==1)
	{
		if(vec->base[0]->type!=FKL_NAST_VECTOR||vec->base[0]->vec->size==0)
			goto reader_macro_error;
		FklGrammerIgnore* ignore=nast_vector_to_ignore(vec->base[0]->vec
				,&codegen->g->builtins
				,codegen->g->terminals);
		if(!ignore)
		{
reader_macro_error:
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(vector_node);
			errorState->fid=codegen->fid;
			return 1;
		}
		if(group_id==0)
		{
			if(fklAddIgnoreToIgnoreList(&codegen->unnamed_ignores,ignore))
			{
				fklDestroyIgnore(ignore);
				goto reader_macro_error;
			}
		}
		else
		{
			FklGrammerProductionGroup* group=add_production_group(&codegen->named_prod_groups,group_id);
			if(fklAddIgnoreToIgnoreList(&group->ignore,ignore))
			{
				fklDestroyIgnore(ignore);
				goto reader_macro_error;
			}
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
			FklGrammerProduction* prod=nast_vector_to_production(base[1]->vec
					,&codegen->g->builtins
					,codegen->g->terminals
					,0
					,0
					,base[2]->sym
					,base[3]
					,group_id
					,codegen
					,curEnv
					,macroScope
					,codegenQuestStack);
			if(!prod)
				goto reader_macro_error;
			if(group_id==0)
			{
				if(fklAddProdToProdTable(&codegen->unnamed_prods,&codegen->g->builtins,prod))
				{
					fklDestroyGrammerProduction(prod);
					goto reader_macro_error;
				}
			}
			else
			{
				FklGrammerProductionGroup* group=add_production_group(&codegen->named_prod_groups,group_id);
				if(fklAddProdToProdTable(&group->prods,&codegen->g->builtins,prod))
				{
					fklDestroyGrammerProduction(prod);
					goto reader_macro_error;
				}
				group->is_ref_outer=check_group_outer_ref(codegen,&group->prods,group_id);
			}
		}
		else if(base[0]->type==FKL_NAST_SYM
				&&base[1]->type==FKL_NAST_VECTOR)
		{
			FklNastVector* vect=base[1]->vec;
			FklSid_t sid=base[0]->sym;
			if(group_id==0&&(fklGetHashItem(&sid,&codegen->g->builtins)
						||fklIsNonterminalExist(&codegen->builtin_prods,group_id,sid)))
				goto reader_macro_error;
			FklGrammerProduction* prod=nast_vector_to_production(vect
					,&codegen->g->builtins
					,codegen->g->terminals
					,group_id
					,sid
					,base[2]->sym
					,base[3]
					,group_id
					,codegen
					,curEnv
					,macroScope
					,codegenQuestStack);
			FklGrammerProduction* extra_prod=create_extra_start_production(group_id,sid);
			if(!prod)
				goto reader_macro_error;
			if(group_id==0)
			{
				if(fklAddProdToProdTable(&codegen->unnamed_prods,&codegen->g->builtins,prod))
				{
					fklDestroyGrammerProduction(prod);
					goto reader_macro_error;
				}
				if(fklAddProdToProdTable(&codegen->unnamed_prods,&codegen->g->builtins,extra_prod))
				{
					fklDestroyGrammerProduction(extra_prod);
					goto reader_macro_error;
				}
			}
			else
			{
				FklGrammerProductionGroup* group=add_production_group(&codegen->named_prod_groups,group_id);
				if(fklAddProdToProdTable(&group->prods,&codegen->g->builtins,prod))
				{
					fklDestroyGrammerProduction(prod);
					goto reader_macro_error;
				}
				if(fklAddProdToProdTable(&group->prods,&codegen->g->builtins,extra_prod))
				{
					fklDestroyGrammerProduction(extra_prod);
					goto reader_macro_error;
				}
				group->is_ref_outer=check_group_outer_ref(codegen,&group->prods,group_id);
			}
		}
		else if(base[0]->type==FKL_NAST_VECTOR
				&&base[1]->type==FKL_NAST_SYM)
		{
			FklNastVector* vect=base[0]->vec;
			FklSid_t sid=base[1]->sym;
			if(group_id==0&&(fklGetHashItem(&sid,&codegen->g->builtins)
						||fklIsNonterminalExist(&codegen->builtin_prods,group_id,sid)))
				goto reader_macro_error;
			FklGrammerProduction* prod=nast_vector_to_production(vect
					,&codegen->g->builtins
					,codegen->g->terminals
					,group_id
					,sid
					,base[2]->sym
					,base[3]
					,group_id
					,codegen
					,curEnv
					,macroScope
					,codegenQuestStack);
			if(!prod)
				goto reader_macro_error;
			if(group_id==0)
			{
				if(fklAddProdToProdTable(&codegen->unnamed_prods,&codegen->g->builtins,prod))
				{
					fklDestroyGrammerProduction(prod);
					goto reader_macro_error;
				}
			}
			else
			{
				FklGrammerProductionGroup* group=add_production_group(&codegen->named_prod_groups,group_id);
				if(fklAddProdToProdTable(&group->prods,&codegen->g->builtins,prod))
				{
					fklDestroyGrammerProduction(prod);
					goto reader_macro_error;
				}
				group->is_ref_outer=check_group_outer_ref(codegen,&group->prods,group_id);
			}
		}
		else
			goto reader_macro_error;
	}
	else
		goto reader_macro_error;
	return 0;
}

static CODEGEN_FUNC(codegen_defmacro)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	if(name->type==FKL_NAST_SYM)
	{
		if(value->type==FKL_NAST_SYM)
		{
			FklNastNode* replacement=NULL;
			for(FklCodegenMacroScope* cs=macroScope;cs&&!replacement;cs=cs->prev)
				replacement=fklGetReplacement(value->sym,cs->replacements);
			if(replacement)
				value=replacement;
			else
			{
				ReplacementFunc f=findBuiltInReplacementWithId(value->sym);
				if(f)
				{
					FklNastNode* t=f(value,curEnv,codegen);
					value=t;
				}
			}
		}
		fklAddReplacementBySid(name->sym,value,macroScope->replacements);
	}
	else if(name->type==FKL_NAST_PAIR)
	{
		if(isNonRetvalExp(value))
		{
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(origExp);
			errorState->fid=codegen->fid;
			return;
		}
		FklHashTable* symbolSet=NULL;
		FklNastNode* pattern=fklCreatePatternFromNast(name,&symbolSet);
		if(!pattern)
		{
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_INVALID_MACRO_PATTERN;
			errorState->place=fklMakeNastNodeRef(name);
			return;
		}
		FklCodegenEnv* macroEnv=NULL;
		FklCodegen* macroCodegen=macro_compile_prepare(codegen,curEnv,macroScope,symbolSet,&macroEnv);
		fklDestroyHashTable(symbolSet);

		create_and_insert_to_pool(macroCodegen->pts
				,0
				,macroEnv
				,macroCodegen->globalSymTable
				,0
				,macroCodegen->fid
				,value->curline);
		FklPtrQueue* queue=fklCreatePtrQueue();
		fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_compiler_macro_bc_process
				,createMacroQuestContext(pattern,macroScope,macroCodegen->pts)
				,createDefaultQueueNextExpression(queue)
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
			errorState->fid=codegen->fid;
			return;
		}
		FklSid_t group_id=value->type==FKL_NAST_NIL?0:value->sym;
		if(process_add_production(group_id
					,codegen
					,name
					,errorState
					,curEnv
					,macroScope
					,codegenQuestStack))
			return;
		if(add_all_group_to_grammer(codegen))
		{
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(origExp);
			errorState->fid=codegen->fid;
			return;
		}
	}
	else
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
		return;
	}
}

static CODEGEN_FUNC(codegen_def_reader_macros)
{
	FklPtrQueue prod_vector_queue;
	fklInitPtrQueue(&prod_vector_queue);
	FklNastNode* arg=fklPatternMatchingHashTableRef(builtInPatternVar_arg0,ht);
	fklPushPtrQueue(arg,&prod_vector_queue);
	arg=fklPatternMatchingHashTableRef(builtInPatternVar_arg1,ht);
	fklPushPtrQueue(arg,&prod_vector_queue);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	for(;rest->pair->cdr->type!=FKL_NAST_NIL;rest=rest->pair->cdr)
		fklPushPtrQueue(rest->pair->car,&prod_vector_queue);
	FklNastNode* group_node=rest->pair->car;
	if(group_node->type!=FKL_NAST_NIL&&group_node->type!=FKL_NAST_SYM)
	{
		fklUninitPtrQueue(&prod_vector_queue);
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
		return;
	}
	FklSid_t group_id=group_node->type==FKL_NAST_SYM?group_node->sym:0;
	for(FklQueueNode* first=prod_vector_queue.head;first;first=first->next)
		if(process_add_production(group_id
					,codegen
					,first->data
					,errorState
					,curEnv
					,macroScope
					,codegenQuestStack))
		{
			fklUninitPtrQueue(&prod_vector_queue);
			return;
		}
	fklUninitPtrQueue(&prod_vector_queue);
	if(add_all_group_to_grammer(codegen))
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		errorState->fid=codegen->fid;
		return;
	}
}

typedef void (*FklCodegenFunc)(CODEGEN_ARGS);

#undef BC_PROCESS
#undef GET_STACK
#undef CODEGEN_ARGS
#undef CODEGEN_FUNC

static FklByteCode* createPushFix(int64_t i)
{
	if(i==0)
		return create1lenBc(FKL_OP_PUSH_0);
	else if(i==1)
		return create1lenBc(FKL_OP_PUSH_1);
	else if(i>=INT8_MIN&&i<=INT8_MAX)
		return fklCreatePushI8ByteCode(i);
	else if(i>=INT16_MIN&&i<=INT16_MAX)
		return fklCreatePushI16ByteCode(i);
	else if(i>=INT32_MIN&&i<=INT32_MAX)
		return fklCreatePushI32ByteCode(i);
	else
		return fklCreatePushI64ByteCode(i);
}

FklByteCode* fklCodegenNode(const FklNastNode* node,FklCodegen* codegenr)
{
	FklPtrStack stack=FKL_STACK_INIT;
	fklInitPtrStack(&stack,32,16);
	fklPushPtrStack((void*)node,&stack);
	FklByteCode* retval=fklCreateByteCode(0);
	while(!fklIsPtrStackEmpty(&stack))
	{
		FklNastNode* node=fklPopPtrStack(&stack);
		FklByteCode* tmp=NULL;
		switch(node->type)
		{
			case FKL_NAST_SYM:
				tmp=fklCreatePushSidByteCode(fklAddSymbol(fklGetSymbolWithIdFromPst(node->sym)->symbol
							,codegenr->globalSymTable)->id);
				break;
			case FKL_NAST_NIL:
				tmp=create1lenBc(FKL_OP_PUSH_NIL);
				break;
			case FKL_NAST_CHR:
				tmp=fklCreatePushCharByteCode(node->chr);
				break;
			case FKL_NAST_FIX:
				tmp=createPushFix(node->fix);
				break;
			case FKL_NAST_F64:
				tmp=fklCreatePushF64ByteCode(node->f64);
				break;
			case FKL_NAST_BYTEVECTOR:
				tmp=fklCreatePushBvecByteCode(node->bvec);
				break;
			case FKL_NAST_STR:
				tmp=fklCreatePushStrByteCode(node->str);
				break;
			case FKL_NAST_BIG_INT:
				tmp=fklCreatePushBigIntByteCode(node->bigInt);
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
						tmp=create1lenBc(FKL_OP_PUSH_PAIR);
					else
						tmp=create9lenBc(FKL_OP_PUSH_LIST,i);
				}
				break;
			case FKL_NAST_BOX:
				tmp=create1lenBc(FKL_OP_PUSH_BOX);
				fklPushPtrStack(node->box,&stack);
				break;
			case FKL_NAST_VECTOR:
				tmp=create9lenBc(FKL_OP_PUSH_VECTOR,node->vec->size);
				for(size_t i=0;i<node->vec->size;i++)
					fklPushPtrStack(node->vec->base[i],&stack);
				break;
			case FKL_NAST_HASHTABLE:
				tmp=create9lenBc(FKL_OP_PUSH_HASHTABLE_EQ+node->hash->type,node->hash->num);
				for(size_t i=0;i<node->hash->num;i++)
				{
					fklPushPtrStack(node->hash->items[i].car,&stack);
					fklPushPtrStack(node->hash->items[i].cdr,&stack);
				}
				break;
			default:
				FKL_ASSERT(0);
				break;
		}
		fklReCodeCat(tmp,retval);
		fklDestroyByteCode(tmp);
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
		,FklCodegen* codegenr
		,FklCodegenErrorState* errorState)
{
	FklHashTable* ht=fklCreatePatternMatchingHashTable();
	int r=fklPatternMatch(pattern,exp,ht);
	if(r)
	{
		chdir(codegenr->curDir);
		func(exp
				,ht
				,codegenQuestStack
				,scope
				,macroScope
				,env
				,codegenr
				,errorState);
	}
	fklDestroyHashTable(ht);
	return r;
}

typedef enum
{
	PATTERN_BEGIN=0,
	PATTERN_LOCAL,
	PATTERN_LET0,
	PATTERN_LET1,
	PATTERN_LET80,
	PATTERN_LET81,
	PATTERN_LET82,
	PATTERN_NAMED_LET0,
	PATTERN_NAMED_LET1,
	PATTERN_LETREC0,
	PATTERN_LETREC1,

	PATTERN_DO0,
	PATTERN_DO0N,
	PATTERN_DO1,
	PATTERN_DO1N,
	PATTERN_DO11,
	PATTERN_DO11N,

	PATTERN_DEFUN,
	PATTERN_DEFINE,
	PATTERN_SETQ,
	PATTERN_CHECK,
	PATTERN_QUOTE,
	PATTERN_UNQUOTE,
	PATTERN_QSQUOTE,
	PATTERN_LAMBDA,
	PATTERN_AND,
	PATTERN_OR,
	PATTERN_COND,
	PATTERN_IF0,
	PATTERN_IF1,
	PATTERN_WHEN,
	PATTERN_UNLESS,
	PATTERN_LOAD,
	PATTERN_IMPORT_PREFIX,
	PATTERN_IMPORT_ONLY,
	PATTERN_IMPORT_ALIAS,
	PATTERN_IMPORT_EXCEPT,
	PATTERN_IMPORT,
	PATTERN_MACROEXPAND,
	PATTERN_DEFMACRO,
	PATTERN_IMPORT_NONE,
	PATTERN_EXPORT_SINGLE,
	PATTERN_EXPORT,
	PATTERN_DEF_READER_MACROS,
}PatternEnum;

static struct PatternAndFunc
{
	const char* ps;
	FklNastNode* pn;
	FklCodegenFunc func;
}builtInPattern[]=
{
	{"~(begin,~rest)",                                         NULL, codegen_begin,               },
	{"~(local,~rest)",                                         NULL, codegen_local,               },
	{"~(let [],~rest)",                                        NULL, codegen_let0,                },
	{"~(let [(~name ~value),~args],~rest)",                    NULL, codegen_let1,                },
	{"~(let* [],~rest)",                                       NULL, codegen_let0,                },
	{"~(let* [(~name ~value)],~rest)",                         NULL, codegen_let1,                },
	{"~(let* [(~name ~value),~args],~rest)",                   NULL, codegen_let81,               },
	{"~(let ~arg0 [],~rest)",                                  NULL, codegen_named_let0,          },
	{"~(let ~arg0 [(~name ~value),~args],~rest)",              NULL, codegen_named_let1,          },
	{"~(letrec [],~rest)",                                     NULL, codegen_let0,                },
	{"~(letrec [(~name ~value),~args],~rest)",                 NULL, codegen_letrec,              },

	{"~(do [] [~cond ~value],~rest)",                          NULL, codegen_do0,                 },
	{"~(do [] [~cond],~rest)",                                 NULL, codegen_do0,                 },

	{"~(do [(~name ~arg0),~args] [~cond ~value],~rest)",       NULL, codegen_do1,                 },
	{"~(do [(~name ~arg0),~args] [~cond],~rest)",              NULL, codegen_do1,                 },

	{"~(do [(~name ~arg0 ~arg1),~args] [~cond ~value],~rest)", NULL, codegen_do1,                 },
	{"~(do [(~name ~arg0 ~arg1),~args] [~cond],~rest)",        NULL, codegen_do1,                 },

	{"~(define (~name,~args),~rest)",                          NULL, codegen_defun,               },
	{"~(define ~name ~value)",                                 NULL, codegen_define,              },
	{"~(setq ~name ~value)",                                   NULL, codegen_setq,                },
	{"~(check ~name)",                                         NULL, codegen_check,               },
	{"~(quote ~value)",                                        NULL, codegen_quote,               },
	{"`(unquote `value)",                                      NULL, codegen_unquote,             },
	{"~(qsquote ~value)",                                      NULL, codegen_qsquote,             },
	{"~(lambda ~args,~rest)",                                  NULL, codegen_lambda,              },
	{"~(and,~rest)",                                           NULL, codegen_and,                 },
	{"~(or,~rest)",                                            NULL, codegen_or,                  },
	{"~(cond,~rest)",                                          NULL, codegen_cond,                },
	{"~(if ~value ~rest)",                                     NULL, codegen_if0,                 },
	{"~(if ~value ~rest ~args)",                               NULL, codegen_if1,                 },
	{"~(when ~value,~rest)",                                   NULL, codegen_when,                },
	{"~(unless ~value,~rest)",                                 NULL, codegen_unless,              },
	{"~(load ~name,~rest)",                                    NULL, codegen_load,                },
	{"~(import (prefix ~name ~rest),~args)",                   NULL, codegen_import_prefix,       },
	{"~(import (only ~name,~rest),~args)",                     NULL, codegen_import_only,         },
	{"~(import (alias ~name,~rest),~args)",                    NULL, codegen_import_alias,        },
	{"~(import (except ~name,~rest),~args)",                   NULL, codegen_import_except,       },
	{"~(import ~name,~args)",                                  NULL, codegen_import,              },
	{"~(macroexpand ~value)",                                  NULL, codegen_macroexpand,         },
	{"~(defmacro ~name ~value)",                               NULL, codegen_defmacro,            },
	{"~(import)",                                              NULL, codegen_import_none,         },
	{"~(export ~value)",                                       NULL, codegen_export_single,       },
	{"~(export,~rest)",                                        NULL, codegen_export,              },
	{"~(defmacro ~arg0 ~arg1,~rest)",                          NULL, codegen_def_reader_macros,   },
	{NULL,                                                     NULL, NULL,                        },
};

static inline int isExportDefmacroExp(const FklNastNode* c)
{
	return fklPatternMatch(builtInPattern[PATTERN_DEFMACRO].pn,c,NULL)
		&&cadr_nast_node(c)->type!=FKL_NAST_VECTOR;
}

static inline int isExportDefReaderMacroExp(const FklNastNode* c)
{
	return (fklPatternMatch(builtInPattern[PATTERN_DEFMACRO].pn,c,NULL)
			&&cadr_nast_node(c)->type==FKL_NAST_VECTOR)
		||fklPatternMatch(builtInPattern[PATTERN_DEF_READER_MACROS].pn,c,NULL);
}

static inline int isExportDefineExp(const FklNastNode* c)
{
	return fklPatternMatch(builtInPattern[PATTERN_DEFINE].pn,c,NULL);
}

static inline int isExportDefunExp(const FklNastNode* c)
{
	return fklPatternMatch(builtInPattern[PATTERN_DEFUN].pn,c,NULL);
}

static inline int isNonRetvalExp(const FklNastNode* c)
{
	static const size_t PatternNum=sizeof(builtInPattern)/sizeof(struct PatternAndFunc)-1;
	for(size_t i=PATTERN_DEFMACRO;i<PatternNum;i++)
		if(fklPatternMatch(builtInPattern[i].pn,c,NULL))
			return 1;
	return 0;
}

static inline int isExportImportExp(FklNastNode* c)
{
	return fklPatternMatch(builtInPattern[PATTERN_IMPORT_PREFIX].pn,c,NULL)
		||fklPatternMatch(builtInPattern[PATTERN_IMPORT_ONLY].pn,c,NULL)
		||fklPatternMatch(builtInPattern[PATTERN_IMPORT_ALIAS].pn,c,NULL)
		||fklPatternMatch(builtInPattern[PATTERN_IMPORT_EXCEPT].pn,c,NULL)
		||fklPatternMatch(builtInPattern[PATTERN_IMPORT].pn,c,NULL)
		||fklPatternMatch(builtInPattern[PATTERN_IMPORT_NONE].pn,c,NULL);
}

static inline int isValidExportNodeList(const FklNastNode* list)
{
	for(;list->type==FKL_NAST_PAIR;list=list->pair->cdr)
		if(list->pair->car->type!=FKL_NAST_PAIR
				&&!isExportDefmacroExp(list->pair->car)
				&&!isExportImportExp(list->pair->car)
				&&!isExportDefunExp(list->pair->car)
				&&!isExportDefineExp(list->pair->car))
			return 0;
	return list->type==FKL_NAST_NIL;
}

void fklInitCodegen(void)
{
	FklSymbolTable* publicSymbolTable=fklGetPubSymTab();
	builtInPatternVar_rest=fklAddSymbolCstr("rest",publicSymbolTable)->id;
	builtInPatternVar_name=fklAddSymbolCstr("name",publicSymbolTable)->id;
	builtInPatternVar_cond=fklAddSymbolCstr("cond",publicSymbolTable)->id;
	builtInPatternVar_args=fklAddSymbolCstr("args",publicSymbolTable)->id;
	builtInPatternVar_arg0=fklAddSymbolCstr("arg0",publicSymbolTable)->id;
	builtInPatternVar_arg1=fklAddSymbolCstr("arg1",publicSymbolTable)->id;
	builtInPatternVar_value=fklAddSymbolCstr("value",publicSymbolTable)->id;

	builtInPatternVar_custom=fklAddSymbolCstr("custom",publicSymbolTable)->id;
	builtInPatternVar_builtin=fklAddSymbolCstr("builtin",publicSymbolTable)->id;
	builtInPatternVar_simple=fklAddSymbolCstr("simple",publicSymbolTable)->id;

	for(struct PatternAndFunc* cur=&builtInPattern[0];cur->ps!=NULL;cur++)
	{
		FklNastNode* node=fklCreateNastNodeFromCstr(cur->ps,publicSymbolTable);
		cur->pn=fklMakeNastNodeRef(fklCreatePatternFromNast(node,NULL));
		fklDestroyNastNode(node);
	}

	for(struct SymbolReplacement* cur=&builtInSymbolReplacement[0];cur->s!=NULL;cur++)
		cur->sid=fklAddSymbolCstr(cur->s,publicSymbolTable)->id;

	for(struct SubPattern* cur=&builtInSubPattern[0];cur->ps!=NULL;cur++)
	{
		FklNastNode* node=fklCreateNastNodeFromCstr(cur->ps,publicSymbolTable);
		cur->pn=fklMakeNastNodeRef(fklCreatePatternFromNast(node,NULL));
		fklDestroyNastNode(node);
	}

	init_builtin_prod_action_list();
	init_simple_prod_action_list();
}

void fklUninitCodegen(void)
{
	for(struct PatternAndFunc* cur=&builtInPattern[0];cur->ps!=NULL;cur++)
	{
		fklDestroyNastNode(cur->pn);
		cur->pn=NULL;
	}
	for(struct SubPattern* cur=&builtInSubPattern[0];cur->ps!=NULL;cur++)
	{
		fklDestroyNastNode(cur->pn);
		cur->pn=NULL;
	}
}

void fklDestroyCodegener(FklCodegen* codegen)
{
	while(codegen&&codegen->destroyAbleMark)
	{
		codegen->refcount-=1;
		FklCodegen* prev=codegen->prev;
		if(!codegen->refcount)
		{
			fklUninitCodegener(codegen);
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
		,FklCodegen* codegenr
		,FklCodegenErrorState* errorState)
{
	if(fklIsNastNodeList(curExp))
		for(struct PatternAndFunc* cur=&builtInPattern[0];cur->ps!=NULL;cur++)
			if(matchAndCall(cur->func
						,cur->pn
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
	FklByteCode* bc=create5lenBc(FKL_OP_GET_LOC,idx);
	return createBclnt(bc,fid,curline);
}

static FklByteCodelnt* makeGetVarRefBc(uint32_t idx,FklSid_t fid,uint64_t curline)
{
	FklByteCode* bc=create5lenBc(FKL_OP_GET_VAR_REF,idx);
	return createBclnt(bc,fid,curline);
}

static void printCodegenError(FklNastNode* obj
		,FklBuiltInErrorType type
		,FklSid_t sid
		,FklSymbolTable* symbolTable
		,size_t line)
{
	FklSymbolTable* publicSymbolTable=fklGetPubSymTab();
	if(type==FKL_ERR_DUMMY)
		return;
	fprintf(stderr,"error in compiling: ");
	switch(type)
	{
		case FKL_ERR_CIR_REF:
			fprintf(stderr,"Circular reference occur in expanding macro");
			break;
		case FKL_ERR_SYMUNDEFINE:
			fprintf(stderr,"Symbol ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			fprintf(stderr," is undefined");
			break;
		case FKL_ERR_SYNTAXERROR:
			fprintf(stderr,"Invalid syntax ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_INVALIDEXPR:
			fprintf(stderr,"Invalid expression");
			if(obj!=NULL)
			{
				fputc(' ',stderr);
				fklPrintNastNode(obj,stderr,publicSymbolTable);
			}
			break;
		case FKL_ERR_CIRCULARLOAD:
			fprintf(stderr,"Circular load file ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_INVALIDPATTERN:
			fprintf(stderr,"Invalid string match pattern ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_INVALID_MACRO_PATTERN:
			fprintf(stderr,"Invalid macro pattern ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_MACROEXPANDFAILED:
			fprintf(stderr,"Failed to expand macro in ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_LIBUNDEFINED:
			fprintf(stderr,"Library ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr,publicSymbolTable);
			fprintf(stderr," undefined");
			break;
		case FKL_ERR_FILEFAILURE:
			fprintf(stderr,"Failed for file:");
			fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_IMPORTFAILED:
			fprintf(stderr,"Failed for importing module:");
			fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_UNEXPECTED_EOF:
			fprintf(stderr,"Unexpected eof");
			break;
		case FKL_ERR_IMPORT_MISSING:
			fprintf(stderr,"Missing import for ");
			fklPrintNastNode(obj,stderr,publicSymbolTable);
			break;
		case FKL_ERR_EXPORT_OUTER_REF_PROD_GROUP:
			fprintf(stderr,"Exporting production groups with reference to other group");
			break;
		default:
			fprintf(stderr,"Unknown compiling error.");
			break;
	}
	if(obj)
	{
		if(sid)
		{
			fprintf(stderr," at line %lu of file ",obj->curline);
			fklPrintString(fklGetSymbolWithId(sid,symbolTable)->symbol,stderr);
			fputc('\n',stderr);
		}
		else
			fprintf(stderr," at line %lu\n",obj->curline);
	}
	else
	{
		fprintf(stderr," at line %lu of file ",line);
		fklPrintString(fklGetSymbolWithId(sid,symbolTable)->symbol,stderr);
		fputc('\n',stderr);
	}
	if(obj)
		fklDestroyNastNode(obj);
}

FklByteCodelnt* fklGenExpressionCodeWithQuest(FklCodegenQuest* initialQuest,FklCodegen* codegener)
{
	FklPtrStack resultStack=FKL_STACK_INIT;
	fklInitPtrStack(&resultStack,1,8);
	FklCodegenErrorState errorState={0,0,NULL,0};
	FklPtrStack codegenQuestStack=FKL_STACK_INIT;
	fklInitPtrStack(&codegenQuestStack,32,16);
	fklPushPtrStack(initialQuest,&codegenQuestStack);
	while(!fklIsPtrStackEmpty(&codegenQuestStack))
	{
		FklCodegenQuest* curCodegenQuest=fklTopPtrStack(&codegenQuestStack);
		FklCodegen* curCodegen=curCodegenQuest->codegen;
		FklCodegenNextExpression* nextExpression=curCodegenQuest->nextExpression;
		FklCodegenEnv* curEnv=curCodegenQuest->env;
		FklCodegenQuestContext* curContext=curCodegenQuest->context;
		int r=1;
		if(nextExpression)
		{
			for(FklNastNode* curExp=nextExpression->t->getNextExpression(nextExpression->context,&errorState)
					;curExp
					;curExp=nextExpression->t->getNextExpression(nextExpression->context,&errorState))
			{
				if(curExp->type==FKL_NAST_PAIR)
				{
					curExp=fklTryExpandCodegenMacro(curExp
							,curCodegen
							,curCodegenQuest->macroScope
							,&errorState);
					if(errorState.type)
						break;
				}
				if(curExp->type==FKL_NAST_SYM)
				{
					FklNastNode* replacement=NULL;
					for(FklCodegenMacroScope* cs=curCodegenQuest->macroScope;cs&&!replacement;cs=cs->prev)
						replacement=fklGetReplacement(curExp->sym,cs->replacements);
					if(replacement)
					{
						fklDestroyNastNode(curExp);
						curExp=replacement;
						goto skip;
					}
					else
					{
						ReplacementFunc f=findBuiltInReplacementWithId(curExp->sym);
						if(f)
						{
							FklNastNode* t=f(curExp,curEnv,curCodegen);
							fklDestroyNastNode(curExp);
							curExp=t;
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
skip:
					curContext->t->__put_bcl(curContext->data
							,createBclnt(fklCodegenNode(curExp,curCodegen)
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
			printCodegenError(errorState.place
					,errorState.type
					,errorState.fid
					,curCodegen->globalSymTable
					,errorState.line);
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
					,&errorState);
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
	return retval;
}

FklByteCodelnt* fklGenExpressionCodeWithFp(FILE* fp
		,FklCodegen* codegen)
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
		,FklCodegen* codegenr)
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

void fklInitGlobalCodegener(FklCodegen* codegen
		,const char* rp
		,FklSymbolTable* globalSymTable
		,int destroyAbleMark)
{
	fklInitSymbolTableWithBuiltinSymbol(globalSymTable);
	codegen->globalSymTable=globalSymTable;
	if(rp!=NULL)
	{
		codegen->curDir=fklGetDir(rp);
		codegen->filename=fklRelpath(fklGetMainFileRealPath(),rp);
		codegen->realpath=fklCopyCstr(rp);
		codegen->fid=fklAddSymbolCstr(codegen->filename,globalSymTable)->id;
	}
	else
	{
		codegen->curDir=getcwd(NULL,0);
		codegen->filename=NULL;
		codegen->realpath=NULL;
		codegen->fid=0;
	}
	codegen->g=NULL;
	codegen->curline=1;
	codegen->prev=NULL;
	codegen->globalEnv=fklCreateCodegenEnv(NULL,0,NULL);
	fklInitGlobCodegenEnv(codegen->globalEnv);
	codegen->globalEnv->refcount+=1;
	codegen->destroyAbleMark=destroyAbleMark;
	codegen->refcount=0;
	codegen->exportNum=0;
	codegen->exports=NULL;
	codegen->loadedLibStack=fklCreatePtrStack(8,8);
	codegen->macroLibStack=fklCreatePtrStack(8,8);
	codegen->pts=fklCreateFuncPrototypes(0);
	codegen->builtinSymModiMark=fklGetBuiltinSymbolModifyMark(&codegen->builtinSymbolNum);
	create_and_insert_to_pool(codegen->pts
			,0
			,codegen->globalEnv
			,codegen->globalSymTable
			,0
			,codegen->fid
			,1);
}

void fklInitCodegener(FklCodegen* codegen
		,const char* filename
		,FklCodegenEnv* env
		,FklCodegen* prev
		,FklSymbolTable* globalSymTable
		,int destroyAbleMark
		,int libMark
		,int macroMark)
{
	if(filename!=NULL)
	{
		char* rp=fklRealpath(filename);
		codegen->curDir=fklGetDir(rp);
		codegen->filename=fklRelpath(fklGetMainFileRealPath(),rp);
		codegen->realpath=rp;
		codegen->fid=fklAddSymbolCstr(codegen->filename,globalSymTable)->id;
	}
	else
	{
		codegen->curDir=getcwd(NULL,0);
		codegen->filename=NULL;
		codegen->realpath=NULL;
		codegen->fid=0;
	}
	codegen->g=NULL;
	codegen->curline=1;
	codegen->prev=prev;
	prev->refcount+=1;
	codegen->globalEnv=env;
	env->refcount+=1;
	codegen->globalSymTable=globalSymTable;
	codegen->destroyAbleMark=destroyAbleMark;
	codegen->refcount=0;
	codegen->exportNum=0;
	codegen->exports=NULL;
	codegen->exports_in_pst=NULL;
	codegen->libMark=libMark;
	codegen->macroMark=macroMark;
	codegen->export_macro=NULL;
	codegen->export_replacement=libMark?createCodegenReplacements():NULL;
	codegen->export_named_prod_groups=libMark?fklCreateSidSet():NULL;
	if(prev)
	{
		codegen->loadedLibStack=prev->loadedLibStack;
		codegen->macroLibStack=prev->macroLibStack;
		codegen->pts=prev->pts;
		codegen->builtinSymModiMark=prev->builtinSymModiMark;
		codegen->builtinSymbolNum=prev->builtinSymbolNum;
	}
	else
	{
		codegen->loadedLibStack=fklCreatePtrStack(8,8);
		codegen->macroLibStack=fklCreatePtrStack(8,8);
		codegen->pts=fklCreateFuncPrototypes(0);
		codegen->builtinSymModiMark=fklGetBuiltinSymbolModifyMark(&codegen->builtinSymbolNum);
	}
}

void fklUninitCodegener(FklCodegen* codegen)
{
	fklDestroyCodegenEnv(codegen->globalEnv);

	if(!codegen->destroyAbleMark||codegen->macroMark)
		free(codegen->builtinSymModiMark);
	if(!codegen->destroyAbleMark)
	{
		if(codegen->globalSymTable&&codegen->globalSymTable!=fklGetPubSymTab())
			fklDestroySymbolTable(codegen->globalSymTable);
		while(!fklIsPtrStackEmpty(codegen->loadedLibStack))
			fklDestroyCodegenLib(fklPopPtrStack(codegen->loadedLibStack));
		fklDestroyPtrStack(codegen->loadedLibStack);
		FklPtrStack* macroLibStack=codegen->macroLibStack;
		while(!fklIsPtrStackEmpty(macroLibStack))
		{
			FklCodegenLib* cur=fklPopPtrStack(macroLibStack);
			fklUninitCodegenLib(cur);
			free(cur);
		}
		fklDestroyPtrStack(macroLibStack);
	}
	free(codegen->curDir);
	if(codegen->filename)
		free(codegen->filename);
	if(codegen->realpath)
		free(codegen->realpath);
	if(codegen->exports)
		free(codegen->exports);
	if(codegen->exports_in_pst)
		free(codegen->exports_in_pst);
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
	if(codegen->g)
	{
		FklGrammer* g=codegen->g;
		fklClearGrammer(g);
		fklDestroySymbolTable(g->terminals);
		fklDestroySymbolTable(g->reachable_terminals);
		fklUninitHashTable(&g->builtins);
		fklUninitHashTable(&g->firstSets);
		fklUninitHashTable(&g->productions);
		destroy_grammer_ignore_list(codegen->builtin_ignores);
		destroy_grammer_ignore_list(codegen->unnamed_ignores);
		fklUninitHashTable(&codegen->builtin_prods);
		fklUninitHashTable(&codegen->unnamed_prods);
		fklUninitHashTable(&codegen->named_prod_groups);
		free(g);
		codegen->g=NULL;
	}
}

static inline void recompute_all_terminal_sid(FklGrammerProductionGroup* group
		,FklSymbolTable* target_table
		,FklSymbolTable* origin_table)
{
	for(FklGrammerIgnore* igs=group->ignore;igs;igs=igs->next)
		recompute_ignores_terminal_sid(igs,target_table);
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
		,FklCodegen* codegen
		,FklByteCodelnt* bcl
		,FklCodegenEnv* env)
{
	size_t exportNum=0;
	FklSid_t* exports_in_pst=NULL;
	lib->type=FKL_CODEGEN_LIB_SCRIPT;
	lib->bcl=bcl;
	lib->named_prod_groups.t=NULL;
	lib->terminal_table=NULL;
	if(codegen)
	{
		exportNum=codegen->exportNum;
		exports_in_pst=codegen->exports_in_pst;
		lib->rp=codegen->realpath;
		lib->exportNum=exportNum;
		lib->exports=codegen->exports;
		lib->head=codegen->export_macro;
		lib->replacements=codegen->export_replacement;
		lib->exports_in_pst=exports_in_pst;
		if(codegen->export_named_prod_groups&&codegen->export_named_prod_groups->num)
		{
			lib->terminal_table=fklCreateSymbolTable();
			FklGrammer* g=codegen->g;
			fklInitHashTable(&lib->named_prod_groups,&GrammerProductionMetaTable);
			for(FklHashTableItem* sid_list=codegen->export_named_prod_groups->first
					;sid_list
					;sid_list=sid_list->next)
			{
				FklSid_t id=*(FklSid_t*)sid_list->data;
				FklGrammerProductionGroup* group=fklGetHashItem(&id,&codegen->named_prod_groups);
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
				recompute_all_terminal_sid(target_group,lib->terminal_table,g->terminals);
			}
		}
	}
	else
	{
		lib->rp=NULL;
		lib->exports=NULL;
		lib->head=NULL;
		lib->replacements=NULL;
		lib->exportNum=exportNum;
		lib->exports_in_pst=exports_in_pst;
	}
	if(env)
	{
		lib->prototypeId=env->prototypeId;
		uint32_t* idxes=(uint32_t*)malloc(sizeof(uint32_t)*exportNum);
		FKL_ASSERT(idxes||!exportNum);
		for(uint32_t i=0;i<exportNum;i++)
			idxes[i]=fklAddCodegenDefBySid(exports_in_pst[i],1,env);
		lib->exportIndex=idxes;
	}
	else
	{
		lib->prototypeId=0;
		lib->exportIndex=NULL;
	}
}

FklCodegenLib* fklCreateCodegenScriptLib(FklCodegen* codegen
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
		,FklDllHandle dll
		,FklSymbolTable* table
		,FklCodegenDllLibInitExportFunc init)
{
	FklCodegenLib* lib=(FklCodegenLib*)malloc(sizeof(FklCodegenLib));
	FKL_ASSERT(lib);
	fklInitCodegenDllLib(lib,rp,dll,table,init);
	return lib;
}

void fklInitCodegenDllLib(FklCodegenLib* lib
		,char* rp
		,FklDllHandle dll
		,FklSymbolTable* table
		,FklCodegenDllLibInitExportFunc init)
{
	lib->rp=rp;
	lib->type=FKL_CODEGEN_LIB_DLL;
	lib->dll=dll;
	lib->head=NULL;
	lib->replacements=NULL;
	lib->exportIndex=NULL;
	lib->exportNum=0;
	size_t num=0;
	FklSid_t* exports=NULL;
	init(&num,&exports,table);
	FklSid_t* exportsP=NULL;
	exportsP=(FklSid_t*)malloc(sizeof(FklSid_t)*num);
	FKL_ASSERT(exportsP);
	for(size_t i=0;i<num;i++)
	{
		FklSid_t idp=fklAddSymbolToPst(fklGetSymbolWithId(exports[i],table)->symbol)->id;
		exportsP[i]=idp;
	}
	lib->exports_in_pst=exportsP;
	lib->exports=exports;
	lib->exportNum=num;
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

inline void fklUninitCodegenLibInfo(FklCodegenLib* lib)
{
	free(lib->rp);
	free(lib->exports);
	free(lib->exports_in_pst);
	free(lib->exportIndex);
	if(lib->named_prod_groups.t)
	{
		fklUninitHashTable(&lib->named_prod_groups);
		fklDestroySymbolTable(lib->terminal_table);
	}
	lib->exportNum=0;
}

inline void fklUninitCodegenLib(FklCodegenLib* lib)
{
	switch(lib->type)
	{
		case FKL_CODEGEN_LIB_SCRIPT:
			fklDestroyByteCodelnt(lib->bcl);
			break;
		case FKL_CODEGEN_LIB_DLL:
			if(lib->dll)
				fklDestroyDll(lib->dll);
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
		,FklByteCodelnt* bcl
		,FklCodegenMacro* next
		,FklFuncPrototypes* pts
		,int own)
{
	FklCodegenMacro* r=(FklCodegenMacro*)malloc(sizeof(FklCodegenMacro));
	FKL_ASSERT(r);
	r->pattern=pattern;
	r->bcl=bcl;
	r->next=next;
	r->pts=pts;
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
	r->replacements=createCodegenReplacements();
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
		,FklFuncPrototypes* pts)
{
	FklFuncPrototype* mainPts=&pts->pts[1];
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

static void macro_expand_frame_atomic(FklCallObjData data,FklVMgc* gc)
{
}

static void macro_expand_frame_step(FklCallObjData data,FklVM* exe)
{
	MacroExpandCtx* ctx=(MacroExpandCtx*)data;
	*(ctx->retval)=fklCreateNastNodeFromVMvalue(fklGetTopValue(exe)
			,ctx->curline
			,ctx->lineHash
			,ctx->symbolTable);
	ctx->done=1;
}

static void macro_expand_frame_finalizer(FklCallObjData data)
{
}

static int macro_expand_frame_end(FklCallObjData data)
{
	return ((MacroExpandCtx*)data)->done;
}

static const FklVMframeContextMethodTable MacroExpandMethodTable=
{
	.step=macro_expand_frame_step,
	.end=macro_expand_frame_end,
	.atomic=macro_expand_frame_atomic,
	.finalizer=macro_expand_frame_finalizer
};

static void insert_macro_expand_frame(FklVMframe* mainframe
		,FklNastNode** ptr
		,FklHashTable* lineHash
		,FklSymbolTable* symbolTable
		,uint64_t curline)
{
	FklVMframe* f=fklCreateOtherObjVMframe(&MacroExpandMethodTable,mainframe->prev);
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
		,FklHashTable* ht
		,FklHashTable* lineHash
		,FklPtrStack* macroLibStack
		,FklNastNode** pr
		,uint64_t curline)
{
	FklSymbolTable* publicSymbolTable=fklGetPubSymTab();
	FklVM* anotherVM=fklCreateVM(fklCopyByteCodelnt(bcl)
			,publicSymbolTable
			,pts);
	insert_macro_expand_frame(anotherVM->frames
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
		fklInitVMlibWithCodgenLib(cur,&anotherVM->libs[i+1],anotherVM,1,pts);
	}
	FklVMframe* mainframe=anotherVM->frames;
	fklInitGlobalVMclosure(mainframe,anotherVM);
	initVMframeFromPatternMatchTable(anotherVM,mainframe,ht,lineHash,pts);
	return anotherVM;
}

FklNastNode* fklTryExpandCodegenMacro(FklNastNode* exp
		,FklCodegen* codegen
		,FklCodegenMacroScope* macros
		,FklCodegenErrorState* errorState)
{
	FklNastNode* r=exp;
	FklHashTable* ht=NULL;
	uint64_t curline=exp->curline;
	for(FklCodegenMacro* macro=findMacro(r,macros,&ht)
			;macro
			;macro=findMacro(r,macros,&ht))
	{
		FklNastNode* retval=NULL;
		FklHashTable lineHash;
		fklInitLineNumHashTable(&lineHash);
		FklVM* anotherVM=fklInitMacroExpandVM(macro->bcl
				,macro->pts
				,ht
				,&lineHash
				,codegen->macroLibStack
				,&retval
				,r->curline);
		FklVMgc* gc=anotherVM->gc;
		int e=fklRunVM(anotherVM);
		anotherVM->pts=NULL;
		if(e)
		{
			errorState->type=FKL_ERR_MACROEXPANDFAILED;
			errorState->place=r;
			errorState->fid=codegen->fid;
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
				errorState->fid=codegen->fid;
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

inline void fklInitVMlibWithCodegenLibRefs(FklCodegenLib* clib
		,FklVMlib* vlib
		,FklVM* exe
		,FklVMvarRef** refs
		,uint32_t count
		,int needCopy
		,FklFuncPrototypes* pts)
{
	FklVMvalue* val=FKL_VM_NIL;
	if(clib->type==FKL_CODEGEN_LIB_SCRIPT)
	{
		FklByteCode* bc=clib->bcl->bc;
		FklVMvalue* codeObj=fklCreateVMvalueCodeObj(exe,needCopy?fklCopyByteCodelnt(clib->bcl):clib->bcl);
		FklVMvalue* proc=fklCreateVMvalueProc(exe,bc->code,bc->size,codeObj,clib->prototypeId);
		fklInitMainProcRefs(FKL_VM_PROC(proc),refs,count);
		val=proc;
	}
	else
		val=fklCreateVMvalueStr(exe,fklCreateString(strlen(clib->rp)-strlen(FKL_DLL_FILE_TYPE),clib->rp));
	fklInitVMlib(vlib,val);
}

inline void fklInitVMlibWithCodgenLib(FklCodegenLib* clib
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
		FklVMvalue* proc=fklCreateVMvalueProc(exe,bc->code,bc->size,codeObj,clib->prototypeId);
		val=proc;
	}
	else
		val=fklCreateVMvalueStr(exe,fklCreateString(strlen(clib->rp)-strlen(FKL_DLL_FILE_TYPE),clib->rp));
	fklInitVMlib(vlib,val);
}

inline void fklInitVMlibWithCodgenLibAndDestroy(FklCodegenLib* clib
		,FklVMlib* vlib
		,FklVM* exe
		,FklFuncPrototypes* pts)
{
	FklVMvalue* val=FKL_VM_NIL;
	if(clib->type==FKL_CODEGEN_LIB_SCRIPT)
	{
		FklByteCode* bc=clib->bcl->bc;
		FklVMvalue* codeObj=fklCreateVMvalueCodeObj(exe,clib->bcl);
		FklVMvalue* proc=fklCreateVMvalueProc(exe,bc->code,bc->size,codeObj,clib->prototypeId);
		val=proc;
	}
	else
	{
		val=fklCreateVMvalueStr(exe,fklCreateString(strlen(clib->rp)-strlen(FKL_DLL_FILE_TYPE),clib->rp));
		fklDestroyDll(clib->dll);
	}
	fklInitVMlib(vlib,val);

	fklDestroyCodegenLibMacroScope(clib);
	fklUninitCodegenLibInfo(clib);
	free(clib);
}

typedef struct
{
	size_t offset;
	FklPtrStack stateStack;
	FklPtrStack symbolStack;
}NastCreatCtx;

typedef struct
{
	FklCodegen* codegen;
	NastCreatCtx* cc;
	FklVM* exe;
	FklVMvalue* stdinVal;
	FklVMvalue* mainProc;
	FklStringBuffer* buf;
	enum
	{
		READY,
		WAITING,
		READING,
		DONE,
	}state:8;
	uint32_t lcount;
}ReplCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(ReplCtx);

static inline void init_with_main_proc(FklVMproc* d,const FklVMproc* s)
{
	uint32_t count=s->rcount;
	FklVMvarRef** refs=fklCopyMemory(s->closure,count*sizeof(FklVMvarRef*));
	for(uint32_t i=0;i<count;i++)
		fklMakeVMvarRefRef(refs[i]);
	d->closure=refs;
	d->rcount=count;
	d->protoId=1;
	d->lcount=s->lcount;
}

static inline void inc_lref(FklVMCompoundFrameVarRef* lr,uint32_t lcount)
{
	if(!lr->lref)
	{
		lr->lref=(FklVMvarRef**)calloc(lcount,sizeof(FklVMvarRef*));
		FKL_ASSERT(lr->lref);
	}
}

static inline FklVMvarRef* insert_local_ref(FklVMCompoundFrameVarRef* lr
		,FklVMvarRef* ref
		,uint32_t cidx)
{
	FklVMvarRefList* rl=(FklVMvarRefList*)malloc(sizeof(FklVMvarRefList));
	FKL_ASSERT(rl);
	rl->ref=fklMakeVMvarRefRef(ref);
	rl->next=lr->lrefl;
	lr->lrefl=rl;
	lr->lref[cidx]=ref;
	return ref;
}

static inline void process_unresolve_ref_for_repl(FklCodegenEnv* env
		,FklFuncPrototypes* cp
		,FklVMgc* gc
		,FklVMvalue** loc
		,FklVMframe* mainframe)
{
	FklVMCompoundFrameVarRef* lr=fklGetCompoundFrameLocRef(mainframe);
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
			uint32_t prototypeId=uref->prototypeId;
			uint32_t idx=ref->idx;
			inc_lref(lr,lr->lcount);
			for(FklVMvalue* v=gc->head;v;v=v->next)
				if(FKL_IS_PROC(v)&&FKL_VM_PROC(v)->protoId==prototypeId)
				{
					FklVMproc* proc=FKL_VM_PROC(v);
					FklVMvarRef* ref=proc->closure[idx];
					if(lr->lref[def->idx])
					{
						proc->closure[idx]=fklMakeVMvarRefRef(lr->lref[def->idx]);
						fklDestroyVMvarRef(ref);
					}
					else
					{
						ref->idx=def->idx;
						ref->ref=&loc[def->idx];
						insert_local_ref(lr,ref,def->idx);
					}
				}
			free(uref);
		}
		else if(env->prev)
		{
			ref->cidx=fklAddCodegenRefBySid(uref->id,env,uref->fid,uref->line);
			free(uref);
		}
		else
			fklPushPtrStack(uref,&urefs1);
	}
	urefs->top=0;
	while(!fklIsPtrStackEmpty(&urefs1))
		fklPushPtrStack(fklPopPtrStack(&urefs1),urefs);
	fklUninitPtrStack(&urefs1);
}

static inline void update_prototype_lcount(FklFuncPrototypes* cp
		,FklCodegenEnv* env)
{
	FklFuncPrototype* pts=&cp->pts[env->prototypeId];
	pts->lcount=env->lcount;
}

static inline void update_prototype_ref(FklFuncPrototypes* cp
		,FklCodegenEnv* env
		,FklSymbolTable* globalSymTable)
{
	FklFuncPrototype* pts=&cp->pts[env->prototypeId];
	FklHashTable* eht=env->refs;
	uint32_t count=eht->num;
	FklSymbolDef* refs=(FklSymbolDef*)fklRealloc(pts->refs,sizeof(FklSymbolDef)*count);
	FKL_ASSERT(refs||!count);
	pts->refs=refs;
	pts->rcount=count;
	for(FklHashTableItem* list=eht->first;list;list=list->next)
	{
		FklSymbolDef* sd=(FklSymbolDef*)list->data;
		FklSid_t sid=fklAddSymbol(fklGetSymbolWithIdFromPst(sd->k.id)->symbol,globalSymTable)->id;
		FklSymbolDef ref={.k.id=sid,
			.k.scope=sd->k.scope,
			.idx=sd->idx,
			.cidx=sd->cidx,
			.isLocal=sd->isLocal};
		refs[sd->idx]=ref;
	}
}

static inline void alloc_more_space_for_var_ref(FklVMCompoundFrameVarRef* lr
		,uint32_t i
		,uint32_t n)
{
	if(lr->lref&&i<n)
	{
		FklVMvarRef** lref=(FklVMvarRef**)fklRealloc(lr->lref,sizeof(FklVMvarRef*)*n);
		FKL_ASSERT(lref);
		for(;i<n;i++)
			lref[i]=NULL;
		lr->lref=lref;
	}
}

#include<fakeLisp/grammer.h>

static inline void repl_nast_ctx_and_buf_reset(NastCreatCtx* cc,FklStringBuffer* s)
{
	cc->offset=0;
	fklStringBufferClear(s);
	FklPtrStack* ss=&cc->symbolStack;
	while(!fklIsPtrStackEmpty(ss))
	{
		FklAnalyzingSymbol* s=fklPopPtrStack(ss);
		fklDestroyNastNode(s->ast);
		free(s);
	}
	cc->stateStack.top=0;
	fklPushState0ToStack(&cc->stateStack);
}

static void repl_frame_step(FklCallObjData data,FklVM* exe)
{
	ReplCtx* ctx=(ReplCtx*)data;
	NastCreatCtx* cc=ctx->cc;

	if(ctx->state==READY)
	{
		ctx->state=WAITING;
		fklLockVMfp(ctx->stdinVal,exe);
		return;
	}
	else if(ctx->state==WAITING)
	{
		exe->ltp=ctx->lcount;
		ctx->state=READING;
		if(exe->tp!=0)
		{
			printf(";=>");
			fklDBG_printVMstack(exe,stdout,0,exe->symbolTable);
		}
		exe->tp=0;

		printf(">>>");
	}

	FklCodegen* codegen=ctx->codegen;
	FklVMfp* vfp=FKL_VM_FP(ctx->stdinVal);
	FklStringBuffer* s=ctx->buf;
	int ch=fklVMfpNonBlockGetline(vfp,s);
	FklNastNode* ast=NULL;
	FklGrammerMatchOuterCtx outerCtx=FKL_GRAMMER_MATCH_OUTER_CTX_INIT;
	outerCtx.line=codegen->curline;
	size_t restLen=fklStringBufferLen(s)-cc->offset;
	if(fklVMfpEof(vfp)||ch=='\n')
	{
		int err=0;
		size_t errLine=0;
		ast=fklDefaultParseForCharBuf(fklStringBufferBody(s)+cc->offset
				,restLen
				,&restLen
				,&outerCtx
				,exe->symbolTable
				,&err
				,&errLine
				,&cc->symbolStack
				,&cc->stateStack);
		cc->offset=fklStringBufferLen(s)-restLen;
		codegen->curline=outerCtx.line;
		fklUnLockVMfp(ctx->stdinVal);
		if(!restLen&&cc->symbolStack.top==0&&ch==-1)
			ctx->state=DONE;
		else
		{
			if(err==FKL_PARSE_WAITING_FOR_MORE&&fklVMfpEof(vfp))
			{
				repl_nast_ctx_and_buf_reset(cc,s);
				FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_UNEXPECTED_EOF,exe);
			}
			else if(err==FKL_PARSE_TERMINAL_MATCH_FAILED&&restLen)
			{
				repl_nast_ctx_and_buf_reset(cc,s);
				FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
			}
			else if(err==FKL_PARSE_REDUCE_FAILED)
			{
				repl_nast_ctx_and_buf_reset(cc,s);
				FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
			}
			else if(ast)
			{
				if(restLen)
					fklVMfpRewind(vfp,s,fklStringBufferLen(s)-restLen);
				ctx->state=DONE;
				repl_nast_ctx_and_buf_reset(cc,s);

				fklMakeNastNodeRef(ast);
				size_t libNum=codegen->loadedLibStack->top;
				FklByteCodelnt* mainCode=fklGenExpressionCode(ast,codegen->globalEnv,codegen);
				fklDestroyNastNode(ast);
				size_t unloadlibNum=codegen->loadedLibStack->top-libNum;
				if(unloadlibNum)
				{
					FklVMproc* proc=FKL_VM_PROC(ctx->mainProc);
					libNum+=unloadlibNum;
					FklVMlib* nlibs=(FklVMlib*)calloc((libNum+1),sizeof(FklVMlib));
					FKL_ASSERT(nlibs);
					memcpy(nlibs,exe->libs,sizeof(FklVMlib)*(exe->libNum+1));
					for(size_t i=exe->libNum;i<libNum;i++)
					{
						FklVMlib* curVMlib=&nlibs[i+1];
						FklCodegenLib* curCGlib=codegen->loadedLibStack->base[i];
						fklInitVMlibWithCodegenLibRefs(curCGlib
								,curVMlib
								,exe
								,proc->closure
								,proc->rcount
								,0
								,exe->pts);
					}
					FklVMlib* prev=exe->libs;
					exe->libs=nlibs;
					exe->libNum=libNum;
					free(prev);
				}
				if(mainCode)
				{
					uint32_t o_lcount=ctx->lcount;
					fklUnLockVMfp(ctx->stdinVal);
					ctx->state=READY;

					update_prototype_lcount(codegen->pts,codegen->globalEnv);

					update_prototype_ref(codegen->pts
							,codegen->globalEnv
							,codegen->globalSymTable);
					FklVMvalue* mainProc=fklCreateVMvalueProc(exe,NULL,0,FKL_VM_NIL,1);
					FklVMproc* proc=FKL_VM_PROC(mainProc);
					init_with_main_proc(proc,FKL_VM_PROC(ctx->mainProc));
					ctx->mainProc=mainProc;

					FklVMvalue* mainCodeObj=fklCreateVMvalueCodeObj(exe,mainCode);
					ctx->lcount=codegen->pts->pts[1].lcount;

					mainCode=FKL_VM_CO(mainCodeObj);
					proc->lcount=ctx->lcount;
					proc->codeObj=mainCodeObj;
					proc->spc=mainCode->bc->code;
					proc->end=proc->spc+mainCode->bc->size;

					FklVMframe* mainframe=fklCreateVMframeWithProcValue(ctx->mainProc,exe->frames);
					FklVMCompoundFrameVarRef* f=&mainframe->c.lr;
					f->base=0;
					f->loc=fklAllocMoreSpaceForMainFrame(exe,proc->lcount);
					f->lcount=proc->lcount;
					alloc_more_space_for_var_ref(f,o_lcount,f->lcount);

					process_unresolve_ref_for_repl(codegen->globalEnv,codegen->pts,exe->gc,exe->locv,mainframe);

					exe->frames=mainframe;
				}
				else
					ctx->state=WAITING;
			}
		}
	}
	fklUnLockVMfp(ctx->stdinVal);
}

static int repl_frame_end(FklCallObjData data)
{
	ReplCtx* ctx=(ReplCtx*)data;
	return ctx->state==DONE;
}

static void repl_frame_print_backtrace(FklCallObjData data,FILE* fp,FklSymbolTable* table)
{
}

static void repl_frame_atomic(FklCallObjData data,FklVMgc* gc)
{
	ReplCtx* ctx=(ReplCtx*)data;
	fklGC_toGrey(ctx->stdinVal,gc);
	fklGC_toGrey(ctx->mainProc,gc);
	FklVMvalue** locv=ctx->exe->locv;
	uint32_t lcount=ctx->lcount;
	for(uint32_t i=0;i<lcount;i++)
		fklGC_toGrey(locv[i],gc);
}

static inline void destroyNastCreatCtx(NastCreatCtx* cc)
{
	fklUninitPtrStack(&cc->stateStack);
	while(!fklIsPtrStackEmpty(&cc->symbolStack))
	{
		FklAnalyzingSymbol* s=fklPopPtrStack(&cc->symbolStack);
		fklDestroyNastNode(s->ast);
		free(s);
	}
	fklUninitPtrStack(&cc->symbolStack);
	free(cc);
}

static void repl_frame_finalizer(FklCallObjData data)
{
	ReplCtx* ctx=(ReplCtx*)data;
	fklDestroyStringBuffer(ctx->buf);
	destroyNastCreatCtx(ctx->cc);
}

static const FklVMframeContextMethodTable ReplContextMethodTable=
{
	.atomic=repl_frame_atomic,
	.finalizer=repl_frame_finalizer,
	.copy=NULL,
	.print_backtrace=repl_frame_print_backtrace,
	.step=repl_frame_step,
	.end=repl_frame_end,
};

static inline NastCreatCtx* createNastCreatCtx(void)
{
	NastCreatCtx* cc=(NastCreatCtx*)malloc(sizeof(NastCreatCtx));
	FKL_ASSERT(cc);
	cc->offset=0;
	fklInitPtrStack(&cc->symbolStack,16,16);
	fklInitPtrStack(&cc->stateStack,16,16);
	fklPushState0ToStack(&cc->stateStack);
	return cc;
}

static int replErrorCallBack(FklVMframe* f,FklVMvalue* errValue,FklVM* exe)
{
	exe->tp=0;
	exe->bp=0;
	fklPrintErrBacktrace(errValue,exe);
	while(exe->frames->prev)
	{
		FklVMframe* cur=exe->frames;
		exe->frames=cur->prev;
		fklDestroyVMframe(cur,exe);
	}
	ReplCtx* ctx=(ReplCtx*)exe->frames->data;
	ctx->state=READY;
	return 1;
}

inline void fklInitFrameToReplFrame(FklVM* exe,FklCodegen* codegen)
{
	FklVMframe* replFrame=(FklVMframe*)calloc(1,sizeof(FklVMframe));
	FKL_ASSERT(replFrame);
	replFrame->prev=NULL;
	replFrame->errorCallBack=replErrorCallBack;
	exe->frames=replFrame;
	replFrame->type=FKL_FRAME_OTHEROBJ;
	replFrame->t=&ReplContextMethodTable;
	ReplCtx* ctx=(ReplCtx*)replFrame->data;

	FklVMvalue* mainProc=fklCreateVMvalueProc(exe,NULL,0,FKL_VM_NIL,1);
	FklVMproc* proc=FKL_VM_PROC(mainProc);
	fklInitGlobalVMclosureForProc(proc,exe);
	ctx->exe=exe;
	ctx->mainProc=mainProc;
	ctx->lcount=0;

	FklVMvalue* stdinVal=proc->closure[FKL_VM_STDIN_IDX]->v;
	ctx->stdinVal=stdinVal;
	ctx->codegen=codegen;
	NastCreatCtx* cc=createNastCreatCtx();
	ctx->cc=cc;
	ctx->state=READY;
	ctx->buf=fklCreateStringBuffer();
}

