#include "fakeLisp/base.h"
#include "fakeLisp/parser.h"
#include<fakeLisp/codegen.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/lexer.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/reader.h>
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
static FklSid_t builtInHeadSymbolTable[4]={0};

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

// static void setPopArgWithFlag(uint8_t code[5],uint32_t idx,uint8_t flag)
// {
// 	code[0]=flag?FKL_OP_POP_SET_ARG:FKL_OP_POP_ARG;
// 	fklSetU32ToByteCode(&code[1],idx);
// }

static FklByteCodelnt* makePutLoc(const FklNastNode* sym,uint32_t idx,FklCodegen* codegen)
{
	fklAddSymbol(fklGetSymbolWithId(sym->u.sym,codegen->publicSymbolTable)->symbol,codegen->globalSymTable);
	FklByteCodelnt* r=createBclnt(createDupPutLocOrVar(idx,FKL_OP_PUT_LOC),codegen->fid,sym->curline);
	return r;
}

// static FklByteCodelnt* makeSetLoc(const FklNastNode* sym,uint32_t idx,FklCodegen* codegen)
// {
// 	fklAddSymbol(fklGetSymbolWithId(sym->u.sym,codegen->publicSymbolTable)->symbol,codegen->globalSymTable);
// 	FklByteCodelnt* r=createBclnt(createDupPutLocOrVar(idx,FKL_OP_SET_LOC),codegen->fid,sym->curline);
// 	return r;
// }

// static FklByteCodelnt* makePutOrSetLoc(const FklNastNode* sym,uint32_t idx,FklCodegenEnv* env,FklCodegen* codegen)
// {
// 	uint8_t* flags=env->occupiedFlags;
// 	if(flags[idx])
// 		return makeSetLoc(sym,idx,codegen);
// 	flags[idx]=1;
// 	return makePutLoc(sym,idx,codegen);
// }

static FklByteCodelnt* makePutRefLoc(const FklNastNode* sym
		,uint32_t idx
		,FklCodegen* codegen)
{
	fklAddSymbol(fklGetSymbolWithId(sym->u.sym,codegen->publicSymbolTable)->symbol,codegen->globalSymTable);
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
		,uint64_t line)

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

inline static void pushListItemToQueue(FklNastNode* list,FklPtrQueue* queue,FklNastNode** last)
{
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr)
		fklPushPtrQueue(fklMakeNastNodeRef(list->u.pair->car),queue);
	if(last)
		*last=list;
}

inline static void pushListItemToStack(FklNastNode* list,FklPtrStack* stack,FklNastNode** last)
{
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr)
		fklPushPtrStack(list->u.pair->car,stack);
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
		FklHashTableNodeList* list=env->refs->list;
		for(;list;list=list->next)
		{
			FklSymbolDef* def=(FklSymbolDef*)list->node->data;
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
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr)
		if(isNonRetvalExp(list->u.pair->car))
			return 1;
		else
			fklPushPtrQueue(fklMakeNastNodeRef(list->u.pair->car),queue);
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

static uintptr_t _codegenenv_hashFunc(void* key)
{
	FklSid_t sid=*(FklSid_t*)key;
	return sid;
}

static int _codegenenv_keyEqual(void* pkey0,void* pkey1)
{
	FklSidScope* k0=pkey0;
	FklSidScope* k1=pkey1;
	return k0->id==k1->id&&k0->scope==k1->scope;
}

static void _codegenenv_setVal(void* d0,void* d1)
{
	*(FklSymbolDef*)d0=*(FklSymbolDef*)d1;
}

static void _codegenenv_setKey(void* pkey0,void* pkey1)
{
	*(FklSidScope*)pkey0=*(FklSidScope*)pkey1;
}

static FklHashTableMetaTable CodegenEnvHashMethodTable=
{
	.size=sizeof(FklSymbolDef),
	.__setKey=_codegenenv_setKey,
	.__setVal=_codegenenv_setVal,
	.__hashFunc=_codegenenv_hashFunc,
	.__uninitItem=fklDoNothingUnintHashItem,
	.__keyEqual=_codegenenv_keyEqual,
	.__getKey=fklHashDefaultGetKey,
};

static inline uint32_t enter_new_scope(uint32_t p,FklCodegenEnv* env)
{
	uint32_t r=++env->sc;
	FklCodegenEnvScope* scopes=(FklCodegenEnvScope*)realloc(env->scopes,sizeof(FklCodegenEnvScope)*r);
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

inline static void process_unresolve_ref(FklCodegenEnv* env,FklFuncPrototypes* cp)
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
		,FklFuncPrototypes* cp)
{
	process_unresolve_ref(env,cp);
	int r=0;
	uint32_t end=sc->start+sc->end;
	uint32_t i=sc->start;
	for(;i<end;i++)
	{
		r=flags[i]==FKL_CODEGEN_ENV_SLOT_REF;
		flags[i]=0;
		if(r)
			break;
	}
	for(;i<end;i++)
		flags[i]=0;
	return r;
}

static inline void append_close_ref(FklByteCodelnt* retval
		,FklCodegenEnvScope* scope
		,FklSid_t fid
		,uint64_t line)
{
	uint32_t s=scope->start;
	uint32_t e=scope->start+scope->end;
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
	if(has_var_be_ref(env->slotFlags,cur,env,codegen->pts))
		append_close_ref(retval,cur,fid,line);
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
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr)i++;
	return i;
}

static inline FklNastNode* cadr_nast_node(const FklNastNode* node)
{
	return node->u.pair->cdr->u.pair->car;
}

static int is_valid_let_arg(const FklNastNode* node)
{
	return node->type==FKL_NAST_PAIR
		&&fklIsNastNodeList(node)
		&&nast_list_len(node)==2
		&&node->u.pair->car->type==FKL_NAST_SYM
		&&!isNonRetvalExp(cadr_nast_node(node));
}

static int is_valid_let_args(const FklNastNode* sl
		,FklCodegenEnv* env
		,uint32_t scope
		,FklUintStack* stack)
{
	if(fklIsNastNodeList(sl))
	{
		for(;sl->type==FKL_NAST_PAIR;sl=sl->u.pair->cdr)
		{
			FklNastNode* cc=sl->u.pair->car;
			if(!is_valid_let_arg(cc))
				return 0;
			FklSid_t id=cc->u.pair->car->u.sym;
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
	return node->u.pair->cdr->u.pair->cdr->u.pair->car;
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
	// uint8_t popsetac[5]={FKL_OP_POP_SET_ARG};
	// FklByteCode popSetArg={5,popsetac};

	FklByteCodelnt* retval=fklPopPtrStack(bcls);
	fklBcBclAppendToBcl(&resBp,retval,fid,line);
	while(!fklIsUintStackEmpty(symbolStack))
	{
		FklSid_t id=fklPopUintStack(symbolStack);
		uint32_t idx=fklAddCodegenDefBySid(id,scope,env);
		// if(flag)
		// {
		// 	fklSetU32ToByteCode(&popsetac[1],idx);
		// 	fklBcBclAppendToBcl(&popSetArg,retval,fid,line);
		// }
		// else
		//{
		fklSetU32ToByteCode(&popac[1],idx);
		fklBcBclAppendToBcl(&popArg,retval,fid,line);
		//}
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

	fklAddCodegenDefBySid(firstSymbol->u.sym,cs,curEnv);
	fklPushUintStack(firstSymbol->u.sym,symStack);

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
		for(FklNastNode* cur=args;cur->type==FKL_NAST_PAIR;cur=cur->u.pair->cdr)
			fklPushPtrQueue(fklMakeNastNodeRef(cadr_nast_node(cur->u.pair->car)),valueQueue);
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
		(*cur)->u.pair=fklCreateNastPair();
		(*cur)->u.pair->car=a[i];
		cur=&(*cur)->u.pair->cdr;
	}
	(*cur)=fklCreateNastNode(FKL_NAST_NIL,line);
	return r;
}

static CODEGEN_FUNC(codegen_let81)
{
	FklSid_t letHeadId=fklAddSymbolCstr("let",codegen->publicSymbolTable)->id;
	FklNastNode* letHead=fklCreateNastNode(FKL_NAST_SYM,origExp->u.pair->car->curline);
	letHead->u.sym=letHeadId;

	FklNastNode* firstNameValue=cadr_nast_node(origExp);

	FklNastNode* args=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);

	FklNastNode* restLet8=fklCreateNastNode(FKL_NAST_PAIR,args->curline);
	restLet8->u.pair=fklCreateNastPair();
	restLet8->u.pair->car=args;
	restLet8->u.pair->cdr=fklMakeNastNodeRef(rest);
	FklNastNode* old=origExp->u.pair->cdr;
	origExp->u.pair->cdr=restLet8;

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
	firstNameValue->u.pair->cdr=fklCreateNastNode(FKL_NAST_NIL,firstNameValue->u.pair->cdr->curline);

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

	fklAddCodegenDefBySid(firstSymbol->u.sym,cs,curEnv);
	fklPushUintStack(firstSymbol->u.sym,symStack);

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
	for(FklNastNode* cur=args;cur->type==FKL_NAST_PAIR;cur=cur->u.pair->cdr)
		fklPushPtrQueue(fklMakeNastNodeRef(cadr_nast_node(cur->u.pair->car)),valueQueue);

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
	if(list->u.pair->car->type!=FKL_NAST_SYM)
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
		for(;sl->type==FKL_NAST_PAIR;sl=sl->u.pair->cdr)
		{
			FklNastNode* cc=sl->u.pair->car;
			FklNastNode* nextExp=NULL;
			if(!is_valid_do_var_bind(cc,&nextExp))
				return 0;
			FklSid_t id=cc->u.pair->car->u.sym;
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
	// FklU8Stack* fs=ctx->fs;

	uint8_t popC[1]={FKL_OP_DROP,};
	FklByteCode pop={1,popC,};

	uint8_t putLocC[5]={FKL_OP_PUT_LOC,};
	FklByteCode putLoc={5,putLocC,};

	// uint8_t setLocC[5]={FKL_OP_SET_LOC,};
	// FklByteCode setLoc={5,setLocC,};


	uint64_t* idxbase=ss->base;
	// uint8_t* flags=fs->base;
	FklByteCodelnt** bclBase=(FklByteCodelnt**)s->base;
	uint32_t top=s->top;
	for(uint32_t i=0;i<top;i++)
	{
		uint32_t idx=idxbase[i];
		// uint8_t flag=flags[i];
		FklByteCodelnt* curBcl=bclBase[i];
		// if(flag)
		// {
		// 	fklSetU32ToByteCode(&setLocC[1],idx);
		// 	fklBclBcAppendToBcl(curBcl,&setLoc,fid,line);
		// }
		// else
		//{
			fklSetU32ToByteCode(&putLocC[1],idx);
			fklBclBcAppendToBcl(curBcl,&putLoc,fid,line);
		//}
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

BC_PROCESS(_set_var_exp_bc_process)
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
	fklUpdatePrototype(pts,env,codegen->globalSymTable,codegen->publicSymbolTable);
	FklByteCode* pushProc=create13lenBc(FKL_OP_PUSH_PROC,env->prototypeId,retval->bc->size);
	fklBcBclAppendToBcl(pushProc,retval,fid,line);
	fklDestroyByteCode(pushProc);
	return retval;
}

// static FklByteCodelnt* makePopArg(FklOpcode code,const FklNastNode* sym,uint32_t idx,FklCodegen* codegen)
// {
// 	fklAddSymbol(fklGetSymbolWithId(sym->u.sym,codegen->publicSymbolTable)->symbol
// 			,codegen->globalSymTable);
// 	FklByteCode* bc=create5lenBc(code,idx);
// 	return createBclnt(bc,codegen->fid,sym->curline);
// }

static FklByteCodelnt* processArgs(const FklNastNode* args
		,FklCodegenEnv* curEnv
		,FklCodegen* codegen)
{
	FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
	uint8_t popArgCode[5]={FKL_OP_POP_ARG,};
	uint8_t popRestArgCode[5]={FKL_OP_POP_REST_ARG,};
	FklByteCode popArg={5,popArgCode};
	FklByteCode popRestArg={5,popRestArgCode};

	for(;args->type==FKL_NAST_PAIR;args=args->u.pair->cdr)
	{
		FklNastNode* cur=args->u.pair->car;
		if(cur->type!=FKL_NAST_SYM)
		{
			fklDestroyByteCodelnt(retval);
			return NULL;
		}
		if(fklIsSymbolDefined(cur->u.sym,1,curEnv))
		{
			fklDestroyByteCodelnt(retval);
			return NULL;
		}
		uint32_t idx=fklAddCodegenDefBySid(cur->u.sym,1,curEnv);

		fklAddSymbol(fklGetSymbolWithId(cur->u.sym,codegen->publicSymbolTable)->symbol
			,codegen->globalSymTable);

		fklSetU32ToByteCode(&popArgCode[1],idx);
		fklBclBcAppendToBcl(retval,&popArg,codegen->fid,cur->curline);

		//FklByteCodelnt* tmp=makePopArg(FKL_OP_POP_ARG,cur,idx,codegen);
		//fklCodeLntCat(retval,tmp);
		//fklDestroyByteCodelnt(tmp);
	}
	if(args->type!=FKL_NAST_NIL&&args->type!=FKL_NAST_SYM)
	{
		fklDestroyByteCodelnt(retval);
		return NULL;
	}
	if(args->type==FKL_NAST_SYM)
	{
		if(fklIsSymbolDefined(args->u.sym,1,curEnv))
		{
			fklDestroyByteCodelnt(retval);
			return NULL;
		}
		uint32_t idx=fklAddCodegenDefBySid(args->u.sym,1,curEnv);

		fklAddSymbol(fklGetSymbolWithId(args->u.sym,codegen->publicSymbolTable)->symbol
			,codegen->globalSymTable);

		fklSetU32ToByteCode(&popRestArgCode[1],idx);
		fklBclBcAppendToBcl(retval,&popRestArg,codegen->fid,args->curline);

		//FklByteCodelnt* tmp=makePopArg(FKL_OP_POP_REST_ARG,args,idx,codegen);
		//fklCodeLntCat(retval,tmp);
		//fklDestroyByteCodelnt(tmp);
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

		fklAddSymbol(fklGetSymbolWithId(curId,codegen->publicSymbolTable)->symbol
			   ,codegen->globalSymTable);

		fklSetU32ToByteCode(&popArgCode[1],idx);
		fklBclBcAppendToBcl(retval,&popArg,codegen->fid,curline);
	}
	uint8_t opcodes[]={FKL_OP_RES_BP,};
	FklByteCode resBp={1,&opcodes[0]};
	fklBclBcAppendToBcl(retval,&resBp,codegen->fid,curline);
	return retval;
}

static inline FklSymbolDef* psid_to_gsid_ht(FklCodegenEnv* env,FklSymbolTable* globalSymTable,FklSymbolTable* publicSymbolTable)
{
	FklCodegenEnvScope* scopes=env->scopes;
	uint32_t sc=env->sc;
	FklSymbolDef* loc=(FklSymbolDef*)malloc(sizeof(FklSymbolDef)*env->lcount);
	FKL_ASSERT(loc||!env->lcount);
	for(uint32_t i=0;i<sc;i++)
	{
		FklHashTable* sht=scopes[i].defs;
		for(FklHashTableNodeList* list=sht->list;list;list=list->next)
		{
			FklSymbolDef* sd=(FklSymbolDef*)list->node->data;
			FklSid_t sid=fklAddSymbol(fklGetSymbolWithId(sd->k.id,publicSymbolTable)->symbol,globalSymTable)->id;
			uint32_t idx=sd->idx;
			FklSymbolDef def={.k.id=sid,.k.scope=sd->k.scope,.idx=sd->idx,.cidx=sd->cidx,.isLocal=sd->isLocal};
			loc[idx]=def;
		}
	}
	return loc;
}

static inline FklSymbolDef* sid_ht_to_idx_key_ht(FklHashTable* sht,FklSymbolTable* globalSymTable,FklSymbolTable* publicSymbolTable)
{
	FklSymbolDef* refs=(FklSymbolDef*)malloc(sizeof(FklSymbolDef)*sht->num);
	FKL_ASSERT(refs);
	for(FklHashTableNodeList* list=sht->list;list;list=list->next)
	{
		FklSymbolDef* sd=(FklSymbolDef*)list->node->data;
		FklSid_t sid=fklAddSymbol(fklGetSymbolWithId(sd->k.id,publicSymbolTable)->symbol,globalSymTable)->id;
		FklSymbolDef ref={{sid,0},sd->idx,sd->cidx,sd->isLocal};
		refs[sd->idx]=ref;
	}
	return refs;
}

static inline void create_and_insert_to_pool(FklFuncPrototypes* cp,uint32_t p,FklCodegenEnv* env,FklSymbolTable* globalSymTable,FklSymbolTable* publicSymbolTable)
{
	cp->count+=1;
	FklFuncPrototype* pts=(FklFuncPrototype*)realloc(cp->pts,sizeof(FklFuncPrototype)*(cp->count+1));
	FKL_ASSERT(pts);
	cp->pts=pts;
	FklFuncPrototype* cpt=&pts[cp->count];
	env->prototypeId=cp->count;
	cpt->loc=psid_to_gsid_ht(env,globalSymTable,publicSymbolTable);
	cpt->lcount=env->lcount;
	cpt->refs=sid_ht_to_idx_key_ht(env->refs,globalSymTable,publicSymbolTable);
	cpt->rcount=env->refs->num;
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
	uint32_t idx=fklAddCodegenDefBySid(name->u.sym,cs,curEnv);
	fklPushPtrStack(makePutLoc(name,idx,codegen),stack);

	fklAddReplacementBySid(fklAddSymbolCstr("*func*",codegen->publicSymbolTable)->id
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
			,codegen->publicSymbolTable);

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
	fklAddCodegenDefBySid(firstSymbol->u.sym,1,lambdaCodegenEnv);
	fklPushUintStack(firstSymbol->u.sym,symStack);

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

	create_and_insert_to_pool(codegen->pts
			,curEnv->prototypeId
			,lambdaCodegenEnv
			,codegen->globalSymTable
			,codegen->publicSymbolTable);

	FklPtrQueue* valueQueue=fklCreatePtrQueue();

	fklPushPtrQueue(fklMakeNastNodeRef(value),valueQueue);
	for(FklNastNode* cur=args;cur->type==FKL_NAST_PAIR;cur=cur->u.pair->cdr)
		fklPushPtrQueue(fklMakeNastNodeRef(cadr_nast_node(cur->u.pair->car)),valueQueue);

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
	uint32_t idx=fklAddCodegenDefBySid(name->u.sym,cs,curEnv);
	fklPushPtrStack(makePutLoc(name,idx,codegen),stack);

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
			,codegen->publicSymbolTable);

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

inline void fklUpdatePrototype(FklFuncPrototypes* cp,FklCodegenEnv* env,FklSymbolTable* globalSymTable,FklSymbolTable* publicSymbolTable)
{
	FklFuncPrototype* pts=&cp->pts[env->prototypeId];
	FklCodegenEnvScope* scopes=env->scopes;
	size_t size=sizeof(FklSymbolDef)*env->lcount;
	FklSymbolDef* loc=(FklSymbolDef*)realloc(pts->loc,size);
	FKL_ASSERT(loc||!env->lcount);
	memset(loc,0,size);
	FklHashTable* eht=NULL;
	eht=scopes[0].defs;
	for(FklHashTableNodeList* list=eht->list;list;list=list->next)
	{
		FklSymbolDef* sd=(FklSymbolDef*)list->node->data;
		FklSid_t sid=fklAddSymbol(fklGetSymbolWithId(sd->k.id,publicSymbolTable)->symbol,globalSymTable)->id;
		uint32_t idx=sd->idx;
		FklSymbolDef def={.k.id=sid,.k.scope=sd->k.scope,.idx=sd->idx,.cidx=sd->cidx,.isLocal=sd->isLocal};
		loc[idx]=def;
	}
	pts->loc=loc;
	pts->lcount=env->lcount;
	process_unresolve_ref(env,cp);
	eht=env->refs;
	uint32_t count=eht->num;
	FklSymbolDef* refs=(FklSymbolDef*)realloc(pts->refs,sizeof(FklSymbolDef)*count);
	FKL_ASSERT(refs||!count);
	pts->refs=refs;
	pts->rcount=count;
	for(FklHashTableNodeList* list=eht->list;list;list=list->next)
	{
		FklSymbolDef* sd=(FklSymbolDef*)list->node->data;
		FklSid_t sid=fklAddSymbol(fklGetSymbolWithId(sd->k.id,publicSymbolTable)->symbol,globalSymTable)->id;
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

static int _codegen_replacement_keyEqual(void* pkey0,void* pkey1)
{
	FklSid_t k0=*(FklSid_t*)pkey0;
	FklSid_t k1=*(FklSid_t*)pkey1;
	return k0==k1;
}

static void _codegen_replacement_setVal(void* d0,void* d1)
{
	*(FklCodegenReplacement*)d0=*(FklCodegenReplacement*)d1;
	fklMakeNastNodeRef(((FklCodegenReplacement*)d0)->node);
}

static void _codegen_replacement_setKey(void* k0,void* k1)
{
	*(FklSid_t*)k0=*(FklSid_t*)k1;
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
	// r->defs=fklCreateHashTable(&CodegenEnvHashMethodTable);
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

// int fklIsSymbolRefed(FklSid_t id,FklCodegenEnv* env)
// {
// 	return fklGetHashItem(&id,env->defs)!=NULL||fklGetHashItem(&id,env->refs)!=NULL;
// }

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
				if(targetRef)
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
			uint8_t* slotFlags=(uint8_t*)realloc(env->slotFlags,sizeof(uint8_t)*env->lcount);
			FKL_ASSERT(slotFlags);
			env->slotFlags=slotFlags;
		}
		env->slotFlags[idx]=FKL_CODEGEN_ENV_SLOT_OCC;
		// env->occupiedFlags[el->idx]=1;
	}
	// *flag=env->occupiedFlags[el->idx];
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
			,codegen->publicSymbolTable);
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
	uint32_t idx=fklAddCodegenDefBySid(name->u.sym,scope,curEnv);
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
	uint32_t idx=fklAddCodegenDefBySid(name->u.sym,scope,curEnv);
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
			,codegen->publicSymbolTable);
	fklAddReplacementBySid(fklAddSymbolCstr("*func*",codegen->publicSymbolTable)->id
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
		FklHashTableNodeList* list=env->refs->list;
		for(;list;list=list->next)
		{
			FklSymbolDef* def=(FklSymbolDef*)list->node->data;
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
	FklSymbolDef* def=fklFindSymbolDefByIdAndScope(name->u.sym,scope,curEnv);
	if(def)
		fklPushPtrStack(makePutLoc(name,def->idx,codegen),stack);
	else
	{
		uint32_t idx=fklAddCodegenRefBySid(name->u.sym,curEnv,codegen->fid,name->curline);
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

inline static void push_default_codegen_quest(FklNastNode* value
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
	int coverState=0;
	FklCodegenMacro** phead=pmacro;
	for(FklCodegenMacro* cur=*pmacro;cur;pmacro=&cur->next,cur=cur->next)
	{
		coverState=fklPatternCoverState(cur->pattern,pattern);
		if(coverState&&coverState!=FKL_PATTERN_BE_COVER)
			break;
	}
	if(!coverState)
	{
		FklCodegenMacro* macro=fklCreateCodegenMacro(pattern,bcl,*phead,pts,own);
		*phead=macro;
	}
	else
	{
		if(coverState==FKL_PATTERN_EQUAL)
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
}

BC_PROCESS(_export_macro_bc_process)
{
	FklCodegenMacroScope* oms=cms->prev;
	FklCodegen* origCodegen=codegen;
	for(;!codegen->libMark;codegen=codegen->prev);
	for(FklCodegenMacro* cur=cms->head;cur;cur=cur->next)
	{
		cur->own=0;
		add_compiler_macro(&codegen->exportM,fklMakeNastNodeRef(cur->pattern),cur->bcl,cur->pts,1);
		add_compiler_macro(&oms->head,fklMakeNastNodeRef(cur->pattern),cur->bcl,cur->pts,0);
	}
	for(FklHashTableNodeList* cur=cms->replacements->list;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->node->data;
		fklAddReplacementBySid(rep->id,rep->node,codegen->exportR);
		fklAddReplacementBySid(rep->id,rep->node,oms->replacements);
	}
	origCodegen->phead=&codegen->head;
	for(FklStringMatchPattern* head=cms->patterns;head;head=head->next)
	{
		head->own=0;
		fklAddStringMatchPattern(head->parts,head->u.proc,&codegen->exportRM,head->pts,1);
		fklAddStringMatchPattern(head->parts,head->u.proc,origCodegen->phead,head->pts,0);
	}
	return NULL;
}

static int isValidExportNodeList(const FklNastNode* list);
static int isExportDefmacroExp(const FklNastNode*);

static inline void add_export_macro_exp(FklNastNode* list
		,FklPtrQueue* exportQueue)
{
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr)
	{
		FklNastNode* cur=list->u.pair->car;
		if(cur->type!=FKL_NAST_SYM)
			fklPushPtrQueue(fklMakeNastNodeRef(cur),exportQueue);
	}
}

static int isExportImportExp(FklNastNode*);

static inline void add_export_symbol(FklCodegen* libCodegen
		,FklNastNode* origExp
		,FklNastNode* rest
		,FklPtrQueue* exportQueue
		,FklSymbolTable* publicSymbolTable
		,FklSymbolTable* globalSymTable)
{
	FklNastPair* prevPair=origExp->u.pair;
	FklNastNode* exportHead=origExp->u.pair->car;
	for(;rest->type==FKL_NAST_PAIR;rest=rest->u.pair->cdr)
	{
		FklNastNode* restExp=fklNastCons(fklMakeNastNodeRef(exportHead)
				,rest
				,rest->curline);
		fklPushPtrQueue(restExp,exportQueue);
		prevPair->cdr=fklCreateNastNode(FKL_NAST_NIL,rest->curline);
		prevPair=rest->u.pair;
	}
}

inline static void push_single_bcl_codegen_quest(FklByteCodelnt* bcl
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
	if(fklFindSymbolDefByIdAndScope(name->u.sym,scope,curEnv))
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

inline static void unquoteHelperFunc(FklNastNode* value
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
					FklNastNode* cur=node->u.pair->car;
					if(fklPatternMatch(builtInSubPattern[SUB_PATTERN_UNQTESP].pn,cur,unquoteHt))
					{
						FklNastNode* unqtespValue=fklPatternMatchingHashTableRef(builtInPatternVar_value,unquoteHt);
						if(node->u.pair->cdr->type!=FKL_NAST_NIL)
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
							fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,node->u.pair->cdr,appendQuest),&valueStack);
						}
						else
							fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_UNQTESP_CAR,unqtespValue,curQuest),&valueStack);
						break;
					}
					else
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,cur,curQuest),&valueStack);
					node=node->u.pair->cdr;
					if(node->type!=FKL_NAST_PAIR)
					{
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,node,curQuest),&valueStack);
						break;
					}
				}
			}
			else if(curValue->type==FKL_NAST_VECTOR)
			{
				size_t vecSize=curValue->u.vec->size;
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
					if(fklPatternMatch(builtInSubPattern[SUB_PATTERN_UNQTESP].pn,curValue->u.vec->base[i],unquoteHt))
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_UNQTESP_VEC
									,fklPatternMatchingHashTableRef(builtInPatternVar_value,unquoteHt)
									,curQuest)
								,&valueStack);
					else
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,curValue->u.vec->base[i],curQuest),&valueStack);
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
				fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,curValue->u.box,curQuest),&valueStack);
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
		if(lastExp->u.pair->cdr->type==FKL_NAST_PAIR&&is_true_exp(lastExp->u.pair->car))
		{
			pushListItemToQueue(lastExp->u.pair->cdr,lastQueue,&last);
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
			,prev->publicSymbolTable
			,1
			,libMark
			,macroMark);
	return r;
}

typedef struct
{
	FILE* fp;
	FklCodegen* codegen;
	FklPtrStack tokenStack;
}CodegenLoadContext;

static CodegenLoadContext* createCodegenLoadContext(FILE* fp,FklCodegen* codegen)
{
	CodegenLoadContext* r=(CodegenLoadContext*)malloc(sizeof(CodegenLoadContext));
	FKL_ASSERT(r);
	r->codegen=codegen;
	r->fp=fp;
	fklInitPtrStack(&r->tokenStack,32,16);
	return r;
}

static void _codegen_load_finalizer(void* pcontext)
{
	CodegenLoadContext* context=pcontext;
	FklPtrStack* tokenStack=&context->tokenStack;
	while(!fklIsPtrStackEmpty(tokenStack))
		fklDestroyToken(fklPopPtrStack(tokenStack));
	fklUninitPtrStack(tokenStack);
	fclose(context->fp);
	free(context);
}

inline static FklNastNode* getExpressionFromFile(FklCodegen* codegen
		,FILE* fp
		,int* unexpectEOF
		,FklPtrStack* tokenStack
		,size_t* errorLine
		,int* hasError
		,FklCodegenErrorState* errorState)
{
	size_t size=0;
	FklNastNode* begin=NULL;
	FklStringMatchRouteNode* route=NULL;
	char* list=fklReadInStringPattern(fp
			,&size
			,codegen->curline
			,&codegen->curline
			,unexpectEOF
			,tokenStack
			,*(codegen->phead)
			,&route);
	if(*unexpectEOF)
		begin=NULL;
	else
	{
		free(list);
		if(fklIsAllComment(tokenStack))
		{
			begin=NULL;
			*hasError=0;
		}
		else
		{
			begin=fklCreateNastNodeFromTokenStackAndMatchRoute(tokenStack
					,route
					,errorLine
					,builtInHeadSymbolTable
					,codegen
					,codegen->publicSymbolTable);
			*hasError=(begin==NULL);
		}
	}
	while(!fklIsPtrStackEmpty(tokenStack))
		fklDestroyToken(fklPopPtrStack(tokenStack));
	fklDestroyStringMatchRoute(route);
	return begin;
}

static FklNastNode* _codegen_load_get_next_expression(void* pcontext,FklCodegenErrorState* errorState)
{
	CodegenLoadContext* context=pcontext;
	FklCodegen* codegen=context->codegen;
	FklPtrStack* tokenStack=&context->tokenStack;
	FILE* fp=context->fp;
	int unexpectEOF=0;
	size_t errorLine=0;
	int hasError=0;
	FklNastNode* begin=getExpressionFromFile(codegen
			,fp
			,&unexpectEOF
			,tokenStack
			,&errorLine
			,&hasError
			,errorState);
	if(unexpectEOF)
	{
		errorState->line=codegen->curline;
		errorState->fid=codegen->fid;
		errorState->place=NULL;
		switch(unexpectEOF)
		{
			case 1:
				errorState->type=FKL_ERR_UNEXPECTEOF;
				break;
			case 2:
				errorState->type=FKL_ERR_INVALIDEXPR;
				break;
		}
		return NULL;
	}
	if(hasError)
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_INVALIDEXPR;
		errorState->line=errorLine?errorLine:codegen->curline;
		errorState->place=NULL;
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

		FklNastPair* prevPair=origExp->u.pair->cdr->u.pair;

		FklNastNode* loadHead=origExp->u.pair->car;

		for(;rest->type==FKL_NAST_PAIR;rest=rest->u.pair->cdr)
		{
			FklNastNode* restExp=fklNastCons(fklMakeNastNodeRef(loadHead),rest,rest->curline);

			prevPair->cdr=fklCreateNastNode(FKL_NAST_NIL,rest->curline);

			fklPushPtrQueue(restExp,queue);

			prevPair=rest->u.pair;
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

	char* filenameCstr=fklStringToCstr(filename->u.str);
	if(!fklIsAccessableRegFile(filenameCstr))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_FILEFAILURE;
		errorState->place=fklMakeNastNodeRef(filename);
		free(filenameCstr);
		return;
	}
	FklCodegen* nextCodegen=createCodegen(codegen,filenameCstr,curEnv,0,0);
	if(hasLoadSameFile(nextCodegen->realpath,codegen))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_CIRCULARLOAD;
		errorState->place=fklMakeNastNodeRef(filename);
		nextCodegen->refcount=1;
		fklDestroyCodegener(nextCodegen);
		return;
	}
	FILE* fp=fopen(filenameCstr,"r");
	free(filenameCstr);
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

inline static char* combineFileNameFromList(FklNastNode* list
		,FklSymbolTable* publicSymbolTable)
{
	char* r=NULL;
	for(FklNastNode* curPair=list;curPair->type==FKL_NAST_PAIR;curPair=curPair->u.pair->cdr)
	{
		FklNastNode* cur=curPair->u.pair->car;
		r=fklCstrStringCat(r,fklGetSymbolWithId(cur->u.sym,publicSymbolTable)->symbol);
		if(curPair->u.pair->cdr->type!=FKL_NAST_NIL)
			r=fklStrCat(r,FKL_PATH_SEPARATOR_STR);
	}
	return r;
}

static void add_symbol_to_local_env_in_array(FklCodegenEnv* env
		,FklSymbolTable* symbolTable
		,FklSymbolTable* publicSymbolTable
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
			FklSid_t sym=fklAddSymbol(fklGetSymbolWithId(exports[i],symbolTable)->symbol
					,publicSymbolTable)->id;
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
			FklSid_t sym=fklAddSymbol(fklGetSymbolWithId(exports[i],symbolTable)->symbol
					,publicSymbolTable)->id;
			uint32_t idx=fklAddCodegenDefBySid(sym,scope,env);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],i);
			fklBclBcAppendToBcl(bcl,&bc,fid,line);
		}
	}
}

static void add_symbol_with_prefix_to_local_env_in_array(FklCodegenEnv* env
		,FklSymbolTable* symbolTable
		,FklSymbolTable* publicSymbolTable
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
			FklSid_t sym=fklAddSymbol(symbolWithPrefix,publicSymbolTable)->id;
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
			FklSid_t sym=fklAddSymbol(symbolWithPrefix,publicSymbolTable)->id;
			uint32_t idx=fklAddCodegenDefBySid(sym,scope,env);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],i);
			fklBclBcAppendToBcl(bcl,&bc,fid,line);
			free(symbolWithPrefix);
		}
	}
}

static inline void process_import_reader_macro(FklStringMatchPattern** phead,FklStringMatchPattern* head,int own)
{
	for(FklStringMatchPattern* cur=head;cur&&cur->type!=FKL_STRING_PATTERN_BUILTIN;cur=cur->next)
	{
		if(own)
			cur->own=0;
		fklAddStringMatchPattern(cur->parts,cur->u.proc,phead,cur->pts,own);
	}
}

static FklNastNode* createPatternWithPrefixFromOrig(FklNastNode* orig
		,FklString* prefix
		,FklSymbolTable* publicSymbolTable)
{
	FklString* head=fklGetSymbolWithId(orig->u.pair->car->u.sym,publicSymbolTable)->symbol;
	FklString* symbolWithPrefix=fklStringAppend(prefix,head);
	FklNastNode* patternWithPrefix=fklNastConsWithSym(fklAddSymbol(symbolWithPrefix,publicSymbolTable)->id
			,fklMakeNastNodeRef(orig->u.pair->cdr)
			,orig->curline
			,orig->u.pair->car->curline);
	free(symbolWithPrefix);
	return patternWithPrefix;
}

static inline void export_replacement_with_prefix(FklHashTable* replacements
		,FklCodegenMacroScope* macros
		,FklSymbolTable* publicSymbolTable
		,FklString* prefix)
{
	for(FklHashTableNodeList* cur=replacements->list;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->node->data;
		FklString* origSymbol=fklGetSymbolWithId(rep->id,publicSymbolTable)->symbol;
		FklString* symbolWithPrefix=fklStringAppend(prefix,origSymbol);
		FklSid_t id=fklAddSymbol(symbolWithPrefix,publicSymbolTable)->id;
		fklAddReplacementBySid(id,rep->node,macros->replacements);
		free(symbolWithPrefix);
	}
}

inline static void print_undefined_symbol(const FklPtrStack* urefs
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* publicSymTable)
{
	for(uint32_t i=0;i<urefs->top;i++)
	{
		FklUnReSymbolRef* ref=urefs->base[i];
		fprintf(stderr,"warning in compiling: Symbol \"");
		fklPrintString(fklGetSymbolWithId(ref->id,publicSymTable)->symbol,stderr);
		fprintf(stderr,"\" is undefined at line %lu of ",ref->line);
		fklPrintString(fklGetSymbolWithId(ref->fid,globalSymTable)->symbol,stderr);
		fputc('\n',stderr);
	}
}

void fklPrintUndefinedRef(const FklCodegenEnv* env
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* publicSymTable)
{
	print_undefined_symbol(&env->uref,globalSymTable,publicSymTable);
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

static inline FklByteCodelnt* process_import_imported_lib_common(uint32_t libId
		,FklCodegen* codegen
		,FklCodegenLib* lib
		,FklCodegenEnv* curEnv
		,uint32_t scope
		,FklCodegenMacroScope* macroScope
		,uint64_t curline)
{
	for(FklCodegenMacro* cur=lib->head;cur;cur=cur->next)
		add_compiler_macro(&macroScope->head
				,fklMakeNastNodeRef(cur->pattern)
				,cur->bcl
				,cur->pts
				,0);

	for(FklHashTableNodeList* cur=lib->replacements->list;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->node->data;
		fklAddReplacementBySid(rep->id,rep->node,macroScope->replacements);
	}

	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);
	add_symbol_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,codegen->publicSymbolTable
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
		,FklNastNode* prefixNode)
{
	FklString* prefix=fklGetSymbolWithId(prefixNode->u.sym,codegen->publicSymbolTable)->symbol;
	for(FklCodegenMacro* cur=lib->head;cur;cur=cur->next)
		add_compiler_macro(&macroScope->head
				,createPatternWithPrefixFromOrig(cur->pattern
					,prefix
					,codegen->publicSymbolTable)
				,cur->bcl
				,cur->pts
				,0);

	export_replacement_with_prefix(lib->replacements
			,macroScope
			,codegen->publicSymbolTable
			,prefix);

	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);
	add_symbol_with_prefix_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,codegen->publicSymbolTable
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
		,FklNastNode* only)
{
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	uint32_t exportNum=lib->exportNum;
	uint32_t* exportIndex=lib->exportIndex;
	FklSid_t* exportsP=lib->exportsP;
	FklHashTable* replace=lib->replacements;
	FklCodegenMacro* head=lib->head;

	FklFuncPrototype* pt=&codegen->pts->pts[lib->prototypeId];

	for(;only->type==FKL_NAST_PAIR;only=only->u.pair->cdr)
	{
		FklSid_t cur=only->u.pair->car->u.sym;
		int r=0;
		for(FklCodegenMacro* macro=head;macro;macro=macro->next)
		{
			FklNastNode* patternHead=macro->pattern->u.pair->car;
			if(patternHead->u.sym==cur)
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
			uint32_t idx=fklAddCodegenDefBySid(cur,scope,curEnv);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],pt->lcount);
			fklBclBcAppendToBcl(importBc,&bc,codegen->fid,only->curline);
		}
	}

	return importBc;
}

static inline int is_in_except_list(const FklNastNode* list,FklSid_t id)
{
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr)
		if(list->u.pair->car->u.sym==id)
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
		,FklNastNode* except)
{
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	uint32_t exportNum=lib->exportNum;
	uint32_t* exportIndex=lib->exportIndex;

	FklSid_t* exportsP=lib->exportsP;

	FklHashTable* replace=lib->replacements;
	FklCodegenMacro* head=lib->head;

	for(FklCodegenMacro* macro=head;macro;macro=macro->next)
	{
		FklNastNode* patternHead=macro->pattern->u.pair->car;
		if(!is_in_except_list(except,patternHead->u.sym))
		{
			add_compiler_macro(&macroScope->head
					,fklMakeNastNodeRef(macro->pattern)
					,macro->bcl
					,macro->pts
					,0);
		}
	}

	for(FklHashTableNodeList* reps=replace->list;reps;reps=reps->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)reps->node->data;
		if(!is_in_except_list(except,rep->id))
			fklAddReplacementBySid(rep->id,rep->node,macroScope->replacements);
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
		,FklNastNode* alias)
{
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_LIB,libId),codegen->fid,curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	FklSymbolTable* publicSymbolTable=codegen->publicSymbolTable;
	FklSymbolTable* globalSymTable=codegen->globalSymTable;

	uint32_t exportNum=lib->exportNum;
	uint32_t* exportIndex=lib->exportIndex;
	FklSid_t* exports=lib->exports;
	FklHashTable* replace=lib->replacements;
	FklCodegenMacro* head=lib->head;

	FklFuncPrototype* pt=&codegen->pts->pts[lib->prototypeId];

	for(;alias->type==FKL_NAST_PAIR;alias=alias->u.pair->cdr)
	{
		FklNastNode* curNode=alias->u.pair->car;
		FklSid_t cur=curNode->u.pair->car->u.sym;
		FklSid_t cur0=curNode->u.pair->cdr->u.pair->car->u.sym;
		int r=0;
		for(FklCodegenMacro* macro=head;macro;macro=macro->next)
		{
			FklNastNode* patternHead=macro->pattern->u.pair->car;
			if(patternHead->u.sym==cur)
			{
				r=1;

				FklNastNode* orig=macro->pattern;
				FklNastNode* pattern=fklNastConsWithSym(cur0
						,fklMakeNastNodeRef(orig->u.pair->cdr)
						,orig->curline
						,orig->u.pair->car->curline);

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
		FklSid_t targetId=fklAddSymbol(fklGetSymbolWithId(cur,publicSymbolTable)->symbol,globalSymTable)->id;
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
			uint32_t idx=fklAddCodegenDefBySid(cur0,scope,curEnv);
			fklSetU32ToByteCode(&importCode[1],idx);
			fklSetU32ToByteCode(&importCode[5],pt->lcount);
			fklBclBcAppendToBcl(importBc,&bc,codegen->fid,alias->curline);
		}
	}

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
		,FklNastNode* except)
{
	process_import_reader_macro(cms->phead,lib->patterns,0);
	if(prefix)
		return process_import_imported_lib_prefix(libId
				,codegen
				,lib
				,env
				,scope
				,cms
				,line
				,prefix);
	if(only)
		return process_import_imported_lib_only(libId
				,codegen
				,lib
				,env
				,scope
				,cms
				,line
				,only);
	if(alias)
		return process_import_imported_lib_alias(libId
				,codegen
				,lib
				,env
				,scope
				,cms
				,line
				,alias);
	if(except)
		return process_import_imported_lib_except(libId
				,codegen
				,lib
				,env
				,scope
				,cms
				,line
				,except);
	return process_import_imported_lib_common(libId
			,codegen
			,lib
			,env
			,scope
			,cms
			,line);
}

BC_PROCESS(_library_bc_process)
{
	ExportContextData* data=context->data;
	FklByteCodelnt* libBc=sequnce_exp_bc_process(data->stack,fid,line);

	uint8_t exportOpBcCode[5]={FKL_OP_EXPORT,0};
	fklSetU32ToByteCode(&exportOpBcCode[1],codegen->loadedLibStack->top+1);
	FklByteCode exportOpBc={5,exportOpBcCode};
	fklBclBcAppendToBcl(libBc,&exportOpBc,fid,line);

	FklCodegenLib* lib=fklCreateCodegenScriptLib(codegen->realpath
			,libBc
			,codegen->exportNum
			,codegen->exports
			,codegen->exportsP
			,codegen->exportM
			,codegen->exportR
			,codegen->exportRM
			,env);

	fklUpdatePrototype(codegen->pts,env,codegen->globalSymTable,codegen->publicSymbolTable);
	print_undefined_symbol(&env->uref,codegen->globalSymTable,codegen->publicSymbolTable);

	fklPushPtrStack(lib,codegen->loadedLibStack);

	codegen->realpath=NULL;
	codegen->exports=NULL;
	codegen->exportsP=NULL;
	codegen->exportM=NULL;
	codegen->exportR=NULL;
	codegen->exportRM=NULL;
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
				,data->except);
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

	FklSymbolTable* publicSymbolTable=codegen->publicSymbolTable;
	FklSymbolTable* globalSymTable=codegen->globalSymTable;

	for(FklHashTableNodeList* list=defs->list;list;list=list->next)
	{
		FklSymbolDef* def=(FklSymbolDef*)(list->node->data);
		FklSid_t idp=def->k.id;
		fklAddCodegenDefBySid(idp,1,targetEnv);
		FklSid_t id=fklAddSymbol(fklGetSymbolWithId(idp,publicSymbolTable)->symbol
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

		FklSid_t* exports=(FklSid_t*)realloc(codegen->exports,num*sizeof(FklSid_t));
		FKL_ASSERT(exports);

		FklSid_t* exportsP=(FklSid_t*)realloc(codegen->exportsP,num*sizeof(FklSid_t));
		FKL_ASSERT(exportsP);

		FklSid_t* nes=&exports[codegen->exportNum];
		FklSid_t* nesp=&exportsP[codegen->exportNum];
		for(uint32_t i=0;i<top;i++)
		{
			nes[i]=idBase[i];
			nesp[i]=idPbase[i];
		}

		codegen->exportsP=exportsP;
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

		add_compiler_macro(&codegen->exportM
				,fklMakeNastNodeRef(head->pattern)
				,head->bcl
				,head->pts,0);
	}

	for(FklHashTableNodeList* cur=env->macros->replacements->list;cur;cur=cur->next)
	{
		FklCodegenReplacement* rep=(FklCodegenReplacement*)cur->node->data;
		fklAddReplacementBySid(rep->id,rep->node,codegen->exportR);
		fklAddReplacementBySid(rep->id,rep->node,macros->replacements);
	}

	process_import_reader_macro(macros->phead,cms->patterns,0);
	process_import_reader_macro(&codegen->exportRM,cms->patterns,0);
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
				,exportQueue
				,codegen->publicSymbolTable
				,codegen->globalSymTable);

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

static CODEGEN_FUNC(codegen_export_single)
{
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	FklSymbolTable* publicSymbolTable=codegen->publicSymbolTable;
	FklSymbolTable* globalSymTable=codegen->globalSymTable;
	FklCodegen* libCodegen=codegen;
	for(;libCodegen&&!libCodegen->libMark;libCodegen=libCodegen->prev);

	int isInLib=libCodegen
		&&curEnv==codegen->globalEnv
		&&macroScope==codegen->globalEnv->macros;

	if(value->type==FKL_NAST_SYM)
	{
		if(isInLib)
		{
			FklSid_t idp=value->u.sym;
			FklSid_t id=fklAddSymbol(fklGetSymbolWithId(idp,publicSymbolTable)->symbol
					,globalSymTable)->id;

			uint32_t num=libCodegen->exportNum+1;

			FklSid_t* exports=(FklSid_t*)realloc(libCodegen->exports,num*sizeof(FklSid_t));
			FKL_ASSERT(exports);

			FklSid_t* exportsP=(FklSid_t*)realloc(libCodegen->exportsP,num*sizeof(FklSid_t));
			FKL_ASSERT(exportsP);

			exports[libCodegen->exportNum]=id;
			exportsP[libCodegen->exportNum]=idp;

			libCodegen->exportsP=exportsP;
			libCodegen->exports=exports;
			libCodegen->exportNum=num;
		}
	}
	else
	{
		FklPtrQueue* queue=fklCreatePtrQueue();
		fklPushPtrQueue(fklMakeNastNodeRef(value),queue);

		if(isExportDefmacroExp(value))
		{
			if(isInLib)
			{
				FklCodegenMacroScope* cms=fklCreateCodegenMacroScope(macroScope);

				cms->phead=&cms->patterns;

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
		else if(isExportImportExp(value))
		{
			if(isInLib)
			{
				FklCodegenEnv* exportEnv=fklCreateCodegenEnv(NULL,0,macroScope);
				FklCodegenMacroScope* cms=exportEnv->macros;
				cms->phead=&cms->patterns;
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
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(origExp);
			return;
		}
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
	nextCodegen->head=fklInitBuiltInStringPattern(nextCodegen->publicSymbolTable);
	nextCodegen->phead=&nextCodegen->head;
	libEnv->macros->phead=nextCodegen->phead;

	fklInitGlobCodegenEnv(libEnv,nextCodegen->publicSymbolTable);

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
		create_and_insert_to_pool(nextCodegen->pts,0,libEnv,nextCodegen->globalSymTable,nextCodegen->publicSymbolTable);

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
		fklDestroyHashTable(nextCodegen->exportR);
		nextCodegen->exportR=NULL;
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
				,except);
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
				,codegen->publicSymbolTable
				,initExport);
		fklPushPtrStack(lib,codegen->loadedLibStack);
		libId=codegen->loadedLibStack->top;
	}
	else
	{
		lib=codegen->loadedLibStack->base[libId-1];
		dll=lib->u.dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	FklSymbolTable* publicSymbolTable=codegen->publicSymbolTable;
	FklSymbolTable* globalSymTable=codegen->globalSymTable;

	uint32_t exportNum=lib->exportNum;
	FklSid_t* exports=lib->exports;

	for(;only->type==FKL_NAST_PAIR;only=only->u.pair->cdr)
	{
		FklSid_t cur=only->u.pair->car->u.sym;
		FklSid_t targetId=fklAddSymbol(fklGetSymbolWithId(cur,publicSymbolTable)->symbol,globalSymTable)->id;
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
				,codegen->publicSymbolTable
				,initExport);
		fklPushPtrStack(lib,codegen->loadedLibStack);
		libId=codegen->loadedLibStack->top;
	}
	else
	{
		lib=codegen->loadedLibStack->base[libId-1];
		dll=lib->u.dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	uint32_t exportNum=lib->exportNum;
	FklSid_t* exportsP=lib->exportsP;

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
				,codegen->publicSymbolTable
				,initExport);
		fklPushPtrStack(lib,codegen->loadedLibStack);
		libId=codegen->loadedLibStack->top;
	}
	else
	{
		lib=codegen->loadedLibStack->base[libId-1];
		dll=lib->u.dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);
	add_symbol_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,codegen->publicSymbolTable
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
				,codegen->publicSymbolTable
				,initExport);
		fklPushPtrStack(lib,codegen->loadedLibStack);
		libId=codegen->loadedLibStack->top;
	}
	else
	{
		lib=codegen->loadedLibStack->base[libId-1];
		dll=lib->u.dll;
		free(realpath);
	}

	FklString* prefix=fklGetSymbolWithId(prefixNode->u.sym
                ,codegen->publicSymbolTable)->symbol;

	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);
	add_symbol_with_prefix_to_local_env_in_array(curEnv
			,codegen->globalSymTable
			,codegen->publicSymbolTable
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
				,codegen->publicSymbolTable
				,initExport);
		fklPushPtrStack(lib,codegen->loadedLibStack);
		libId=codegen->loadedLibStack->top;
	}
	else
	{
		lib=codegen->loadedLibStack->base[libId-1];
		dll=lib->u.dll;
		free(realpath);
	}
	FklByteCodelnt* importBc=createBclnt(create5lenBc(FKL_OP_LOAD_DLL,libId),codegen->fid,origExp->curline);

	uint8_t importCode[9]={FKL_OP_IMPORT,0};
	FklByteCode bc={9,importCode};

	FklSymbolTable* publicSymbolTable=codegen->publicSymbolTable;
	FklSymbolTable* globalSymTable=codegen->globalSymTable;

	uint32_t exportNum=lib->exportNum;
	FklSid_t* exports=lib->exports;

	for(;alias->type==FKL_NAST_PAIR;alias=alias->u.pair->cdr)
	{
		FklNastNode* curNode=alias->u.pair->car;
		FklSid_t cur=curNode->u.pair->car->u.sym;
		FklSid_t cur0=curNode->u.pair->cdr->u.pair->car->u.sym;
		FklSid_t targetId=fklAddSymbol(fklGetSymbolWithId(cur,publicSymbolTable)->symbol,globalSymTable)->id;
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
	for(;alias->type==FKL_NAST_PAIR;alias=alias->u.pair->cdr)
	{
		FklNastNode* cur=alias->u.pair->car;
		if(cur->type!=FKL_NAST_PAIR
				||cur->u.pair->car->type!=FKL_NAST_SYM
				||cur->u.pair->cdr->type!=FKL_NAST_PAIR
				||cur->u.pair->cdr->u.pair->car->type!=FKL_NAST_SYM
				||cur->u.pair->cdr->u.pair->cdr->type!=FKL_NAST_NIL)
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

		FklNastPair* prevPair=origExp->u.pair->cdr->u.pair;

		FklNastNode* importHead=origExp->u.pair->car;

		for(;rest->type==FKL_NAST_PAIR;rest=rest->u.pair->cdr)
		{
			FklNastNode* restExp=fklNastCons(fklMakeNastNodeRef(importHead),rest,rest->curline);

			prevPair->cdr=fklCreateNastNode(FKL_NAST_NIL,rest->curline);

			fklPushPtrQueue(restExp,queue);

			prevPair=rest->u.pair;
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

    char* filename=combineFileNameFromList(name,codegen->publicSymbolTable);

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

	fklUpdatePrototype(codegen->pts,env,codegen->globalSymTable,codegen->publicSymbolTable);
	print_undefined_symbol(&env->uref,codegen->globalSymTable,codegen->publicSymbolTable);
	add_compiler_macro(&macros->head
			,fklMakeNastNodeRef(pattern)
			,macroBcl
			,pts
			,1);
	return NULL;
}

BC_PROCESS(_reader_macro_bc_process)
{
	MacroContext* d=(MacroContext*)(context->data);
	FklPtrStack* stack=d->stack;
	FklByteCodelnt* macroBcl=fklPopPtrStack(stack);
	FklNastNode* pattern=d->pattern;
	FklCodegenMacroScope* macroScope=d->macroScope;
	FklFuncPrototypes* pts=d->pts;
	d->pts=NULL;

	fklUpdatePrototype(codegen->pts,env,codegen->globalSymTable,codegen->publicSymbolTable);
	print_undefined_symbol(&env->uref,codegen->globalSymTable,codegen->publicSymbolTable);
	fklAddStringMatchPattern(fklMakeNastNodeRef(pattern)
			,macroBcl
			,macroScope->phead
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

typedef FklNastNode* (*ReplacementFunc)(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen);

static FklNastNode* _nil_replacement(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen)
{
	return fklCreateNastNode(FKL_NAST_NIL,orig->curline);
}

static FklNastNode* _is_main_replacement(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen)
{
	FklNastNode* n=fklCreateNastNode(FKL_NAST_FIX,orig->curline);
	if(env->prototypeId==1)
		n->u.fix=1;
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
	r->u.str=s;
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
		r->u.str=s;
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
		r->u.fix=line;
	}
	else
	{
		r=fklCreateNastNode(FKL_NAST_BIG_INT,orig->curline);
		r->u.bigInt=fklCreateBigInt(line);
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
				replacement=fklGetReplacement(value->u.sym,cs->replacements);
			if(replacement)
				value=replacement;
			else
			{
				ReplacementFunc f=findBuiltInReplacementWithId(value->u.sym);
				if(f)
				{
					FklNastNode* t=f(value,curEnv,codegen);
					value=t;
				}
			}
		}
		fklAddReplacementBySid(name->u.sym,value,macroScope->replacements);
	}
	else
	{
		if(name->type==FKL_NAST_PAIR)
		{
			if(isNonRetvalExp(value))
			{
				errorState->type=FKL_ERR_SYNTAXERROR;
				errorState->place=fklMakeNastNodeRef(origExp);
				errorState->fid=codegen->fid;
				return;
			}
			FklHashTable* symbolTable=NULL;
			FklNastNode* pattern=fklCreatePatternFromNast(name,&symbolTable);
			if(!pattern)
			{
				errorState->fid=codegen->fid;
				errorState->type=FKL_ERR_INVALID_MACRO_PATTERN;
				errorState->place=fklMakeNastNodeRef(name);
				return;
			}
			FklCodegenEnv* macroEnv=fklCreateCodegenEnv(curEnv,0,macroScope);
			for(FklHashTableNodeList* list=symbolTable->list
					;list
					;list=list->next)
			{
				FklSid_t* id=(FklSid_t*)list->node->data;
				fklAddCodegenDefBySid(*id,1,macroEnv);
			}
			fklDestroyHashTable(symbolTable);
			FklCodegen* macroCodegen=createCodegen(codegen
					,NULL
					,macroEnv
					,0
					,1);
			macroCodegen->builtinSymModiMark=fklGetBultinSymbolModifyMark(&macroCodegen->builtinSymbolNum);
			fklDestroyCodegenEnv(macroEnv->prev);
			macroEnv->prev=NULL;
			free(macroCodegen->curDir);
			macroCodegen->curDir=fklCopyCstr(codegen->curDir);
			macroCodegen->filename=fklCopyCstr(codegen->filename);
			macroCodegen->realpath=fklCopyCstr(codegen->realpath);
			macroCodegen->pts=fklCreateFuncPrototypes(0);

			macroCodegen->globalSymTable=codegen->publicSymbolTable;
			macroCodegen->publicSymbolTable=codegen->publicSymbolTable;
			macroCodegen->fid=macroCodegen->filename?fklAddSymbolCstr(macroCodegen->filename,macroCodegen->publicSymbolTable)->id:0;
			macroCodegen->loadedLibStack=macroCodegen->macroLibStack;
			fklInitGlobCodegenEnv(macroEnv,macroCodegen->publicSymbolTable);

			create_and_insert_to_pool(macroCodegen->pts,0,macroEnv,macroCodegen->globalSymTable,macroCodegen->publicSymbolTable);
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
			if(isNonRetvalExp(value))
			{
				errorState->type=FKL_ERR_SYNTAXERROR;
				errorState->place=fklMakeNastNodeRef(origExp);
				errorState->fid=codegen->fid;
				return;
			}
			FklHashTable* symbolTable=NULL;
			if(!fklIsValidStringPattern(name,&symbolTable))
			{
				errorState->type=FKL_ERR_INVALID_MACRO_PATTERN;
				errorState->place=fklMakeNastNodeRef(name);
				errorState->fid=codegen->fid;
				return;
			}
			FklCodegenEnv* macroEnv=fklCreateCodegenEnv(NULL,0,macroScope);
			for(FklHashTableNodeList* list=symbolTable->list
					;list
					;list=list->next)
			{
				FklSid_t* item=(FklSid_t*)list->node->data;
				fklAddCodegenDefBySid(*item,1,macroEnv);
			}
			fklDestroyHashTable(symbolTable);

			FklCodegen* macroCodegen=createCodegen(codegen
					,NULL
					,macroEnv
					,0
					,1);
			macroCodegen->builtinSymModiMark=fklGetBultinSymbolModifyMark(&macroCodegen->builtinSymbolNum);
			fklDestroyCodegenEnv(macroEnv->prev);
			macroEnv->prev=NULL;
			free(macroCodegen->curDir);
			macroCodegen->curDir=fklCopyCstr(codegen->curDir);
			macroCodegen->filename=fklCopyCstr(codegen->filename);
			macroCodegen->realpath=fklCopyCstr(codegen->realpath);
			macroCodegen->pts=fklCreateFuncPrototypes(0);

			macroCodegen->globalSymTable=codegen->publicSymbolTable;
			macroCodegen->publicSymbolTable=codegen->publicSymbolTable;
			macroCodegen->loadedLibStack=macroCodegen->macroLibStack;
			macroCodegen->fid=macroCodegen->filename?fklAddSymbolCstr(macroCodegen->filename,macroCodegen->publicSymbolTable)->id:0;
			fklInitGlobCodegenEnv(macroEnv,macroCodegen->publicSymbolTable);

			create_and_insert_to_pool(macroCodegen->pts,0,macroEnv,macroCodegen->globalSymTable,macroCodegen->publicSymbolTable);
			FklPtrQueue* queue=fklCreatePtrQueue();
			fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
			FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_reader_macro_bc_process
					,createMacroQuestContext(name,macroScope,macroCodegen->pts)
					,createDefaultQueueNextExpression(queue)
					,1
					,macroEnv->macros
					,macroEnv
					,value->curline
					,macroCodegen
					,codegenQuestStack);
		}
		else
		{
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(origExp);
			errorState->fid=codegen->fid;
			return;
		}
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
				tmp=fklCreatePushSidByteCode(fklAddSymbol(fklGetSymbolWithId(node->u.sym
								,codegenr->publicSymbolTable)->symbol
							,codegenr->globalSymTable)->id);
				break;
			case FKL_NAST_NIL:
				tmp=create1lenBc(FKL_OP_PUSH_NIL);
				break;
			case FKL_NAST_CHR:
				tmp=fklCreatePushCharByteCode(node->u.chr);
				break;
			case FKL_NAST_FIX:
				tmp=createPushFix(node->u.fix);
				break;
			case FKL_NAST_F64:
				tmp=fklCreatePushF64ByteCode(node->u.f64);
				break;
			case FKL_NAST_BYTEVECTOR:
				tmp=fklCreatePushBvecByteCode(node->u.bvec);
				break;
			case FKL_NAST_STR:
				tmp=fklCreatePushStrByteCode(node->u.str);
				break;
			case FKL_NAST_BIG_INT:
				tmp=fklCreatePushBigIntByteCode(node->u.bigInt);
				break;
			case FKL_NAST_PAIR:
				{
					size_t i=1;
					FklNastNode* cur=node;
					for(;cur->type==FKL_NAST_PAIR;cur=cur->u.pair->cdr)
					{
						fklPushPtrStack(cur->u.pair->car,&stack);
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
				fklPushPtrStack(node->u.box,&stack);
				break;
			case FKL_NAST_VECTOR:
				tmp=create9lenBc(FKL_OP_PUSH_VECTOR,node->u.vec->size);
				for(size_t i=0;i<node->u.vec->size;i++)
					fklPushPtrStack(node->u.vec->base[i],&stack);
				break;
			case FKL_NAST_HASHTABLE:
				tmp=create9lenBc(FKL_OP_PUSH_HASHTABLE_EQ+node->u.hash->type,node->u.hash->num);
				for(size_t i=0;i<node->u.hash->num;i++)
				{
					fklPushPtrStack(node->u.hash->items[i].car,&stack);
					fklPushPtrStack(node->u.hash->items[i].cdr,&stack);
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
	PATTERN_IMPORT_NONE,
	PATTERN_MACROEXPAND,
	PATTERN_DEFMACRO,
	PATTERN_EXPORT_SINGLE,
	PATTERN_EXPORT,
}PatternEnum;

static struct PatternAndFunc
{
	const char* ps;
	FklNastNode* pn;
	FklCodegenFunc func;
}builtInPattern[]=
{
	{"~(begin,~rest)",                                         NULL, codegen_begin,         },
	{"~(local,~rest)",                                         NULL, codegen_local,         },
	{"~(let [],~rest)",                                        NULL, codegen_let0,          },
	{"~(let [(~name ~value),~args],~rest)",                    NULL, codegen_let1,          },
	{"~(let* [],~rest)",                                       NULL, codegen_let0,          },
	{"~(let* [(~name ~value)],~rest)",                         NULL, codegen_let1,          },
	{"~(let* [(~name ~value),~args],~rest)",                   NULL, codegen_let81,         },
	{"~(let ~arg0 [],~rest)",                                  NULL, codegen_named_let0,    },
	{"~(let ~arg0 [(~name ~value),~args],~rest)",              NULL, codegen_named_let1,    },
	{"~(letrec [],~rest)",                                     NULL, codegen_let0,          },
	{"~(letrec [(~name ~value),~args],~rest)",                 NULL, codegen_letrec,        },

	{"~(do [] [~cond ~value],~rest)",                          NULL, codegen_do0,           },
	{"~(do [] [~cond],~rest)",                                 NULL, codegen_do0,           },

	{"~(do [(~name ~arg0),~args] [~cond ~value],~rest)",       NULL, codegen_do1,           },
	{"~(do [(~name ~arg0),~args] [~cond],~rest)",              NULL, codegen_do1,           },

	{"~(do [(~name ~arg0 ~arg1),~args] [~cond ~value],~rest)", NULL, codegen_do1,           },
	{"~(do [(~name ~arg0 ~arg1),~args] [~cond],~rest)",        NULL, codegen_do1,           },

	{"~(define (~name,~args),~rest)",                          NULL, codegen_defun,         },
	{"~(define ~name ~value)",                                 NULL, codegen_define,        },
	{"~(setq ~name ~value)",                                   NULL, codegen_setq,          },
	{"~(check ~name)",                                         NULL, codegen_check,         },
	{"~(quote ~value)",                                        NULL, codegen_quote,         },
	{"`(unquote `value)",                                      NULL, codegen_unquote,       },
	{"~(qsquote ~value)",                                      NULL, codegen_qsquote,       },
	{"~(lambda ~args,~rest)",                                  NULL, codegen_lambda,        },
	{"~(and,~rest)",                                           NULL, codegen_and,           },
	{"~(or,~rest)",                                            NULL, codegen_or,            },
	{"~(cond,~rest)",                                          NULL, codegen_cond,          },
	{"~(if ~value ~rest)",                                     NULL, codegen_if0,           },
	{"~(if ~value ~rest ~args)",                               NULL, codegen_if1,           },
	{"~(when ~value,~rest)",                                   NULL, codegen_when,          },
	{"~(unless ~value,~rest)",                                 NULL, codegen_unless,        },
	{"~(load ~name,~rest)",                                    NULL, codegen_load,          },
	{"~(import (prefix ~name ~rest),~args)",                   NULL, codegen_import_prefix, },
	{"~(import (only ~name,~rest),~args)",                     NULL, codegen_import_only,   },
	{"~(import (alias ~name,~rest),~args)",                    NULL, codegen_import_alias,  },
	{"~(import (except ~name,~rest),~args)",                   NULL, codegen_import_except, },
	{"~(import ~name,~args)",                                  NULL, codegen_import,        },
	{"~(import)",                                              NULL, codegen_import_none,   },
	{"~(macroexpand ~value)",                                  NULL, codegen_macroexpand,   },
	{"~(defmacro ~name ~value)",                               NULL, codegen_defmacro,      },
	{"~(export ~value)",                                       NULL, codegen_export_single, },
	{"~(export,~rest)",                                        NULL, codegen_export,        },
	{NULL,                                                     NULL, NULL,                  },
};

static inline int isExportDefmacroExp(const FklNastNode* c)
{
	return fklPatternMatch(builtInPattern[PATTERN_DEFMACRO].pn,c,NULL);
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
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr)
		if(list->u.pair->car->type!=FKL_NAST_SYM
				&&!isExportDefmacroExp(list->u.pair->car)
				&&!isExportImportExp(list->u.pair->car))
			return 0;
	return list->type==FKL_NAST_NIL;
}

const FklSid_t* fklInitCodegen(FklSymbolTable* publicSymbolTable)
{
	static const char* builtInHeadSymbolTableCstr[4]=
	{
		"quote",
		"qsquote",
		"unquote",
		"unqtesp",
	};
	for(int i=0;i<4;i++)
		builtInHeadSymbolTable[i]=fklAddSymbolCstr(builtInHeadSymbolTableCstr[i],publicSymbolTable)->id;
	builtInPatternVar_rest=fklAddSymbolCstr("rest",publicSymbolTable)->id;
	builtInPatternVar_name=fklAddSymbolCstr("name",publicSymbolTable)->id;
	builtInPatternVar_cond=fklAddSymbolCstr("cond",publicSymbolTable)->id;
	builtInPatternVar_args=fklAddSymbolCstr("args",publicSymbolTable)->id;
	builtInPatternVar_arg0=fklAddSymbolCstr("arg0",publicSymbolTable)->id;
	builtInPatternVar_arg1=fklAddSymbolCstr("arg1",publicSymbolTable)->id;
	builtInPatternVar_value=fklAddSymbolCstr("value",publicSymbolTable)->id;

	FklStringMatchPattern* builtinStringPatterns=fklInitBuiltInStringPattern(publicSymbolTable);
	for(struct PatternAndFunc* cur=&builtInPattern[0];cur->ps!=NULL;cur++)
	{
		FklNastNode* node=fklCreateNastNodeFromCstr(cur->ps
				,builtInHeadSymbolTable
				,builtinStringPatterns
				,publicSymbolTable);
		cur->pn=fklMakeNastNodeRef(fklCreatePatternFromNast(node,NULL));
		fklDestroyNastNode(node);
	}

	for(struct SymbolReplacement* cur=&builtInSymbolReplacement[0];cur->s!=NULL;cur++)
		cur->sid=fklAddSymbolCstr(cur->s,publicSymbolTable)->id;

	for(struct SubPattern* cur=&builtInSubPattern[0];cur->ps!=NULL;cur++)
	{
		FklNastNode* node=fklCreateNastNodeFromCstr(cur->ps
				,builtInHeadSymbolTable
				,builtinStringPatterns
				,publicSymbolTable);
		cur->pn=fklMakeNastNodeRef(fklCreatePatternFromNast(node,NULL));
		fklDestroyNastNode(node);
	}

	fklDestroyAllStringPattern(builtinStringPatterns);
	return builtInHeadSymbolTable;
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
		,FklSymbolTable* publicSymbolTable
		,size_t line)
{
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
		case FKL_ERR_UNEXPECTEOF:
			fprintf(stderr,"Unexpect EOF");
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
	FklCodegenErrorState errorState={0,0,NULL};
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
						replacement=fklGetReplacement(curExp->u.sym,cs->replacements);
					if(replacement)
					{
						fklDestroyNastNode(curExp);
						curExp=replacement;
						goto skip;
					}
					else
					{
						ReplacementFunc f=findBuiltInReplacementWithId(curExp->u.sym);
						if(f)
						{
							FklNastNode* t=f(curExp,curEnv,curCodegen);
							fklDestroyNastNode(curExp);
							curExp=t;
						}
						else
						{
							FklByteCodelnt* bcl=NULL;
							FklSymbolDef* def=fklFindSymbolDefByIdAndScope(curExp->u.sym,curCodegenQuest->scope,curEnv);
							if(def)
								bcl=makeGetLocBc(def->idx,curCodegen->fid,curExp->curline);
							else
							{
								uint32_t idx=fklAddCodegenRefBySid(curExp->u.sym,curEnv,curCodegen->fid,curExp->curline);
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
			printCodegenError(errorState.place
					,errorState.type
					,errorState.fid
					,curCodegen->globalSymTable
					,curCodegen->publicSymbolTable
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
					,curCodegenQuest->curline);
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
		,FklSymbolTable* publicSymTable
		,int destroyAbleMark)
{
	fklInitSymbolTableWithBuiltinSymbol(globalSymTable);
	codegen->globalSymTable=globalSymTable;
	codegen->publicSymbolTable=publicSymTable;
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
	codegen->curline=1;
	codegen->prev=NULL;
	codegen->globalEnv=fklCreateCodegenEnv(NULL,0,NULL);
	fklInitGlobCodegenEnv(codegen->globalEnv,publicSymTable);
	codegen->globalEnv->macros->phead=&codegen->head;
	codegen->globalEnv->refcount+=1;
	codegen->destroyAbleMark=destroyAbleMark;
	codegen->refcount=0;
	codegen->exportNum=0;
	codegen->exports=NULL;
	codegen->head=fklInitBuiltInStringPattern(publicSymTable);
	codegen->phead=&codegen->head;
	codegen->loadedLibStack=fklCreatePtrStack(8,8);
	codegen->macroLibStack=fklCreatePtrStack(8,8);
	codegen->pts=fklCreateFuncPrototypes(0);
	codegen->builtinSymModiMark=fklGetBultinSymbolModifyMark(&codegen->builtinSymbolNum);
	create_and_insert_to_pool(codegen->pts
			,0
			,codegen->globalEnv
			,codegen->globalSymTable
			,codegen->publicSymbolTable);
}

void fklInitCodegener(FklCodegen* codegen
		,const char* filename
		,FklCodegenEnv* env
		,FklCodegen* prev
		,FklSymbolTable* globalSymTable
		,FklSymbolTable* publicSymTable
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
	codegen->curline=1;
	codegen->prev=prev;
	prev->refcount+=1;
	codegen->globalEnv=env;
	env->refcount+=1;
	codegen->globalSymTable=globalSymTable;
	codegen->publicSymbolTable=publicSymTable;
	codegen->destroyAbleMark=destroyAbleMark;
	codegen->refcount=0;
	codegen->exportNum=0;
	codegen->exports=NULL;
	codegen->exportsP=NULL;
	codegen->head=NULL;
	codegen->libMark=libMark;
	codegen->macroMark=macroMark;
	codegen->exportM=NULL;
	codegen->exportR=libMark?createCodegenReplacements():NULL;
	codegen->exportRM=NULL;
	if(prev)
	{
		codegen->phead=prev->phead;
		codegen->loadedLibStack=prev->loadedLibStack;
		codegen->macroLibStack=prev->macroLibStack;
		codegen->pts=prev->pts;
		codegen->builtinSymModiMark=prev->builtinSymModiMark;
		codegen->builtinSymbolNum=prev->builtinSymbolNum;
	}
	else
	{
		codegen->head=fklInitBuiltInStringPattern(publicSymTable);
		codegen->phead=&codegen->head;
		codegen->loadedLibStack=fklCreatePtrStack(8,8);
		codegen->macroLibStack=fklCreatePtrStack(8,8);
		codegen->pts=fklCreateFuncPrototypes(0);
		codegen->builtinSymModiMark=fklGetBultinSymbolModifyMark(&codegen->builtinSymbolNum);
	}
}

void fklUninitCodegener(FklCodegen* codegen)
{
	fklDestroyCodegenEnv(codegen->globalEnv);
	if(codegen->phead==&codegen->head)
	{
		fklDestroyAllStringPattern(codegen->head);
		codegen->head=NULL;
	}
	if(!codegen->destroyAbleMark||codegen->macroMark)
		free(codegen->builtinSymModiMark);
	if(!codegen->destroyAbleMark)
	{
		if(codegen->globalSymTable&&codegen->globalSymTable!=codegen->publicSymbolTable)
			fklDestroySymbolTable(codegen->globalSymTable);
		fklDestroySymbolTable(codegen->publicSymbolTable);
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
	if(codegen->exportsP)
		free(codegen->exportsP);
	for(FklCodegenMacro* cur=codegen->exportM;cur;)
	{
		FklCodegenMacro* t=cur;
		cur=cur->next;
		fklDestroyCodegenMacro(t);
	}
	if(codegen->exportR)
		fklDestroyHashTable(codegen->exportR);
	fklDestroyAllStringPattern(codegen->exportRM);
}

void fklInitCodegenScriptLib(FklCodegenLib* lib
		,char* rp
		,FklByteCodelnt* bcl
		,size_t exportNum
		,FklSid_t* exports
		,FklSid_t* exportsP
		,FklCodegenMacro* head
		,FklHashTable* replacements
		,FklStringMatchPattern* patterns
		,FklCodegenEnv* env)
{
	lib->rp=rp;
	lib->type=FKL_CODEGEN_LIB_SCRIPT;
	lib->u.bcl=bcl;
	lib->exportNum=exportNum;
	lib->exports=exports;
	lib->head=head;
	lib->replacements=replacements;
	lib->patterns=patterns;
	lib->exportsP=exportsP;
	if(env)
	{
		lib->prototypeId=env->prototypeId;
		uint32_t* idxes=(uint32_t*)malloc(sizeof(uint32_t)*exportNum);
		FKL_ASSERT(idxes||!exportNum);
		for(uint32_t i=0;i<exportNum;i++)
			idxes[i]=fklAddCodegenDefBySid(exportsP[i],1,env);
		lib->exportIndex=idxes;
	}
	else
	{
		lib->prototypeId=0;
		lib->exportIndex=NULL;
	}
}

FklCodegenLib* fklCreateCodegenScriptLib(char* rp
		,FklByteCodelnt* bcl
		,size_t exportNum
		,FklSid_t* exports
		,FklSid_t* exportsP
		,FklCodegenMacro* head
		,FklHashTable* replacements
		,FklStringMatchPattern* patterns
		,FklCodegenEnv* env)
{
	FklCodegenLib* lib=(FklCodegenLib*)malloc(sizeof(FklCodegenLib));
	FKL_ASSERT(lib);
	fklInitCodegenScriptLib(lib
			,rp
			,bcl
			,exportNum
			,exports
			,exportsP
			,head
			,replacements
			,patterns
			,env);
	return lib;
}

FklCodegenLib* fklCreateCodegenDllLib(char* rp
		,FklDllHandle dll
		,FklSymbolTable* table
		,FklSymbolTable* ptable
		,FklCodegenDllLibInitExportFunc init)
{
	FklCodegenLib* lib=(FklCodegenLib*)malloc(sizeof(FklCodegenLib));
	FKL_ASSERT(lib);
	fklInitCodegenDllLib(lib,rp,dll,table,ptable,init);
	return lib;
}

void fklInitCodegenDllLib(FklCodegenLib* lib
		,char* rp
		,FklDllHandle dll
		,FklSymbolTable* table
		,FklSymbolTable* ptable
		,FklCodegenDllLibInitExportFunc init)
{
	lib->rp=rp;
	lib->type=FKL_CODEGEN_LIB_DLL;
	lib->u.dll=dll;
	lib->head=NULL;
	lib->replacements=NULL;
	lib->patterns=NULL;
	lib->exportIndex=NULL;
	lib->exportNum=0;
	size_t num=0;
	FklSid_t* exports=NULL;
	init(&num,&exports,table);
	FklSid_t* exportsP=NULL;
	if(ptable)
	{
		exportsP=(FklSid_t*)malloc(sizeof(FklSid_t)*num);
		FKL_ASSERT(exportsP);
		for(size_t i=0;i<num;i++)
		{
			FklSid_t idp=fklAddSymbol(fklGetSymbolWithId(exports[i],table)->symbol,ptable)->id;
			exportsP[i]=idp;
		}
	}
	lib->exportsP=exportsP;
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
	fklDestroyAllStringPattern(lib->patterns);
}

inline void fklUninitCodegenLibInfo(FklCodegenLib* lib)
{
	free(lib->rp);
	free(lib->exports);
	free(lib->exportsP);
	free(lib->exportIndex);
	lib->exportNum=0;
}

inline void fklUninitCodegenLib(FklCodegenLib* lib)
{
	switch(lib->type)
	{
		case FKL_CODEGEN_LIB_SCRIPT:
			fklDestroyByteCodelnt(lib->u.bcl);
			break;
		case FKL_CODEGEN_LIB_DLL:
			if(lib->u.dll)
				fklDestroyDll(lib->u.dll);
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
	r->patterns=NULL;
	r->replacements=createCodegenReplacements();
	r->refcount=0;
	r->prev=make_macro_scope_ref(prev);
	r->phead=prev?prev->phead:&r->patterns;
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
			fklDestroyAllStringPattern(macros->patterns);
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
		,FklVMgc* gc
		,FklFuncPrototypes* pts)
{
	FklFuncPrototype* mainPts=&pts->pts[1];
	FklVMCompoundFrameVarRef* lr=fklGetCompoundFrameLocRef(frame);
	FklVMproc* proc=fklGetCompoundFrameProc(frame)->u.proc;
	uint32_t count=mainPts->lcount;
	FklVMvalue** loc=fklAllocSpaceForLocalVar(exe,count);
	uint32_t idx=0;
	for(FklHashTableNodeList* list=ht->list;list;list=list->next)
	{
		FklPatternMatchingHashTableItem* item=(FklPatternMatchingHashTableItem*)list->node->data;
		FklVMvalue* v=fklCreateVMvalueFromNastNodeNoGC(item->node,lineHash,gc);
		loc[idx]=v;
		idx++;
	}
	lr->loc=loc;
	lr->lcount=count;
	lr->lref=NULL;
	lr->lrefl=NULL;
	proc->closure=lr->ref;
	proc->count=lr->rcount;
}

FklVM* fklInitMacroExpandVM(FklByteCodelnt* bcl
		,FklFuncPrototypes* pts
		,FklHashTable* ht
		,FklHashTable* lineHash
		,FklCodegen* codegen)
{
	FklVM* anotherVM=fklCreateVM(fklCopyByteCodelnt(bcl)
			,codegen->publicSymbolTable);
	FklPtrStack* macroLibStack=codegen->macroLibStack;
	anotherVM->libNum=macroLibStack->top;;
	anotherVM->libs=(FklVMlib*)malloc(sizeof(FklVMlib)*macroLibStack->top);
	FKL_ASSERT(anotherVM->libs);
	for(size_t i=0;i<macroLibStack->top;i++)
	{
		FklCodegenLib* cur=macroLibStack->base[i];
		fklInitVMlibWithCodgenLib(cur,&anotherVM->libs[i],anotherVM->gc,1,pts);
	}
	FklVMframe* mainframe=anotherVM->frames;
	fklInitGlobalVMclosure(mainframe,anotherVM);
	initVMframeFromPatternMatchTable(anotherVM,mainframe,ht,lineHash,anotherVM->gc,pts);
	anotherVM->pts=pts;
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
		FklHashTable* lineHash=fklCreateLineNumHashTable();
		FklVM* anotherVM=fklInitMacroExpandVM(macro->bcl,macro->pts,ht,lineHash,codegen);
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
			uint64_t curline=r->curline;
			fklDestroyNastNode(r);
			r=fklCreateNastNodeFromVMvalue(fklGetTopValue(anotherVM)
					,curline
					,lineHash
					,codegen->publicSymbolTable);
			if(!r)
			{
				errorState->type=FKL_ERR_CIR_REF;
				errorState->place=NULL;
				errorState->fid=codegen->fid;
				errorState->line=curline;
			}
		}
		fklDestroyHashTable(ht);
		fklDestroyHashTable(lineHash);
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
	FklVMgc* gc=exe->gc;
	if(clib->type==FKL_CODEGEN_LIB_SCRIPT)
	{
		FklByteCode* bc=clib->u.bcl->bc;
		FklVMvalue* codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,needCopy?fklCopyByteCodelnt(clib->u.bcl):clib->u.bcl,gc);
		FklVMproc* prc=fklCreateVMproc(bc->code,bc->size,codeObj,gc,clib->prototypeId);
		prc->lcount=pts->pts[prc->protoId].lcount;
		FklVMvalue* proc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,prc,gc);
		fklInitMainProcRefs(proc->u.proc,refs,count);
		val=proc;
	}
	else
		val=fklCreateVMvalueNoGC(FKL_TYPE_STR,fklCreateString(strlen(clib->rp)-strlen(FKL_DLL_FILE_TYPE),clib->rp),gc);
	fklInitVMlib(vlib,val);
}

inline void fklInitVMlibWithCodgenLib(FklCodegenLib* clib
		,FklVMlib* vlib
		,FklVMgc* gc
		,int needCopy
		,FklFuncPrototypes* pts)
{
	FklVMvalue* val=FKL_VM_NIL;
	if(clib->type==FKL_CODEGEN_LIB_SCRIPT)
	{
		FklByteCode* bc=clib->u.bcl->bc;
		FklVMvalue* codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,needCopy?fklCopyByteCodelnt(clib->u.bcl):clib->u.bcl,gc);
		FklVMproc* prc=fklCreateVMproc(bc->code,bc->size,codeObj,gc,clib->prototypeId);
		prc->lcount=pts->pts[prc->protoId].lcount;
		FklVMvalue* proc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,prc,gc);
		val=proc;
	}
	else
		val=fklCreateVMvalueNoGC(FKL_TYPE_STR,fklCreateString(strlen(clib->rp)-strlen(FKL_DLL_FILE_TYPE),clib->rp),gc);
	fklInitVMlib(vlib,val);
}

inline void fklInitVMlibWithCodgenLibAndDestroy(FklCodegenLib* clib
		,FklVMlib* vlib
		,FklVMgc* gc
		,FklFuncPrototypes* pts)
{
	FklVMvalue* val=FKL_VM_NIL;
	if(clib->type==FKL_CODEGEN_LIB_SCRIPT)
	{
		FklByteCode* bc=clib->u.bcl->bc;
		FklVMvalue* codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,clib->u.bcl,gc);
		FklVMproc* prc=fklCreateVMproc(bc->code,bc->size,codeObj,gc,clib->prototypeId);
		prc->lcount=pts->pts[prc->protoId].lcount;
		FklVMvalue* proc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,prc,gc);
		val=proc;
	}
	else
	{
		val=fklCreateVMvalueNoGC(FKL_TYPE_STR,fklCreateString(strlen(clib->rp)-strlen(FKL_DLL_FILE_TYPE),clib->rp),gc);
		fklDestroyDll(clib->u.dll);
	}
	fklInitVMlib(vlib,val);

	fklDestroyCodegenLibMacroScope(clib);
	fklUninitCodegenLibInfo(clib);
	free(clib);
}

#include"utstring.h"

typedef struct
{
	size_t j;
	FklPtrStack* tokenStack;
	FklStringMatchSet* matchSet;
	FklStringMatchRouteNode* root;
	FklStringMatchRouteNode* route;
}NastCreatCtx;

typedef struct
{
	FklCodegen* codegen;
	NastCreatCtx* cc;
	FklVM* exe;
	FklVMvalue* stdinVal;
	FklVMvalue* mainProc;
	UT_string* buf;
	const FklSid_t* headSymbol;
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

#include<fcntl.h>

static inline void init_with_main_proc(FklVMproc* d,const FklVMproc* s)
{
	uint32_t count=s->count;
	FklVMvarRef** refs=fklCopyMemory(s->closure,count*sizeof(FklVMvarRef*));
	for(uint32_t i=0;i<count;i++)
		fklMakeVMvarRefRef(refs[i]);
	d->closure=refs;
	d->count=count;
	d->protoId=1;
	d->lcount=s->lcount;
}

static void repl_frame_step(FklCallObjData data,FklVM* exe)
{
	ReplCtx* ctx=(ReplCtx*)data;

	if(ctx->state==READY)
	{
		ctx->state=WAITING;
		fklLockVMfp(ctx->stdinVal,exe);
		return;
	}
	NastCreatCtx* cc=ctx->cc;
	if(ctx->state==WAITING)
	{
		exe->ltp=ctx->lcount;
		ctx->state=READING;
		if(exe->tp!=0)
		{
			printf(";=>");
			fklDBG_printVMstack(exe,stdout,0,exe->symbolTable);
		}
		exe->tp=0;

		cc->matchSet=FKL_STRING_PATTERN_UNIVERSAL_SET;
		cc->j=0;
		cc->root=fklCreateStringMatchRouteNode(NULL
				,0,0
				,NULL
				,NULL
				,NULL);
		cc->route=cc->root;
		printf(">>>");
	}

	FklCodegen* codegen=ctx->codegen;
	FklVMfp* vfp=ctx->stdinVal->u.fp;
	FILE* fp=vfp->fp;
	int fd=fileno(fp);
	int attr=fcntl(fd,F_GETFL);
	fcntl(fd,F_SETFL,attr|O_NONBLOCK);
	UT_string* s=ctx->buf;
	int ch;
	while((ch=fgetc(fp))>0)
	{
		char c=ch;
		utstring_bincpy(s,&c,sizeof(c));
		if(ch=='\n')
			break;
	}
	fcntl(fd,F_SETFL,attr);
	FklPtrStack* tokenStack=cc->tokenStack;
	if(feof(fp)||ch=='\n')
	{
		size_t line=1;
		int err=0;
		cc->matchSet=fklSplitStringIntoTokenWithPattern(utstring_body(s)
				,utstring_len(s)
				,line
				,&line
				,cc->j
				,&cc->j
				,cc->tokenStack
				,cc->matchSet
				,*(codegen->phead)
				,cc->route
				,&cc->route
				,&err);
		if(fklIsPtrStackEmpty(tokenStack)&&ch==-1)
		{
			fklUnLockVMfp(ctx->stdinVal);
			ctx->state=DONE;
		}
		if(cc->matchSet==NULL)
		{
			size_t j=cc->j;
			size_t len=utstring_len(s);
			size_t errorLine=0;
			if(len-j)
				fklRewindStream(fp,utstring_body(s)+j,len-j);
			FklNastNode* node=fklCreateNastNodeFromTokenStackAndMatchRoute(tokenStack
					,cc->root
					,&errorLine
					,ctx->headSymbol
					,codegen
					,exe->symbolTable);

			while(!fklIsPtrStackEmpty(tokenStack))
				fklDestroyToken(fklPopPtrStack(tokenStack));
			fklDestroyStringMatchRoute(cc->root);
			utstring_clear(s);

			if(node==NULL)
				FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
			fklMakeNastNodeRef(node);

			size_t libNum=codegen->loadedLibStack->top;
			FklByteCodelnt* mainCode=fklGenExpressionCode(node,codegen->globalEnv,codegen);
			fklDestroyNastNode(node);
			if(mainCode)
			{
				fklUnLockVMfp(ctx->stdinVal);
				ctx->state=READY;
				fklUpdatePrototype(codegen->pts
						,codegen->globalEnv
						,codegen->globalSymTable
						,codegen->publicSymbolTable);
				size_t unloadlibNum=codegen->loadedLibStack->top-libNum;

				FklVMproc* proc=fklCreateVMproc(NULL,0,FKL_VM_NIL,exe->gc,1);
				init_with_main_proc(proc,ctx->mainProc->u.proc);
				FklVMvalue* mainProc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,proc,exe->gc);
				ctx->mainProc=mainProc;
				if(unloadlibNum)
				{
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
								,proc->count
								,0
								,exe->pts);
					}
					FklVMlib* prev=exe->libs;
					exe->libs=nlibs;
					exe->libNum=libNum;
					free(prev);
				}
				FklVMvalue* mainCodeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,mainCode,exe->gc);
				ctx->lcount=codegen->pts->pts[1].lcount;

				proc->lcount=ctx->lcount;
				proc->codeObj=mainCodeObj;
				proc->spc=mainCode->bc->code;
				proc->end=proc->spc+mainCode->bc->size;

				FklVMframe* mainframe=fklCreateVMframeWithProcValue(ctx->mainProc,exe->frames);

				FklVMCompoundFrameVarRef* f=&mainframe->u.c.lr;
				f->base=0;
				f->loc=fklAllocMoreSpaceForMainFrame(exe,proc->lcount);
				f->lcount=proc->lcount;

				exe->frames=mainframe;
			}
			else
				ctx->state=WAITING;
		}
		else if(err||(feof(fp)&&cc->matchSet!=FKL_STRING_PATTERN_UNIVERSAL_SET))
		{
			fklUnLockVMfp(ctx->stdinVal);
			fklDestroyStringMatchSet(cc->matchSet);
			fklDestroyStringMatchRoute(cc->root);
			while(!fklIsPtrStackEmpty(tokenStack))
				fklDestroyToken(fklPopPtrStack(tokenStack));
			cc->root=NULL;
			cc->matchSet=FKL_STRING_PATTERN_UNIVERSAL_SET;
			utstring_clear(s);
			if(err)
				FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_INVALIDEXPR,exe);
			else
				FKL_RAISE_BUILTIN_ERROR_CSTR("reading",FKL_ERR_UNEXPECTEOF,exe);
		}
	}
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
	FklPtrStack* tokenStack=cc->tokenStack;
	while(!tokenStack)
		fklDestroyToken(fklPopPtrStack(tokenStack));
	fklDestroyPtrStack(tokenStack);
	fklDestroyStringMatchRoute(cc->root);
	fklDestroyStringMatchSet(cc->matchSet);
	free(cc);
}

static void repl_frame_finalizer(FklCallObjData data)
{
	ReplCtx* ctx=(ReplCtx*)data;
	utstring_free(ctx->buf);
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
	cc->j=0;
	cc->tokenStack=fklCreatePtrStack(32,16);
	cc->matchSet=FKL_STRING_PATTERN_UNIVERSAL_SET;
	cc->root=NULL;
	cc->route=NULL;
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
	ReplCtx* ctx=(ReplCtx*)exe->frames->u.o.data;
	ctx->state=READY;
	return 1;
}

inline void fklInitFrameToReplFrame(FklVM* exe
		,FklCodegen* codegen
		,const FklSid_t* builtInHeadSymbolTable)
{
	FklVMframe* replFrame=(FklVMframe*)calloc(1,sizeof(FklVMframe));
	FKL_ASSERT(replFrame);
	replFrame->prev=NULL;
	replFrame->errorCallBack=replErrorCallBack;
	exe->frames=replFrame;
	replFrame->type=FKL_FRAME_OTHEROBJ;
	replFrame->u.o.t=&ReplContextMethodTable;
	ReplCtx* ctx=(ReplCtx*)replFrame->u.o.data;

	FklVMproc* proc=fklCreateVMproc(NULL,0,FKL_VM_NIL,exe->gc,1);
	fklInitGlobalVMclosureForProc(proc,exe);
	ctx->exe=exe;
	ctx->mainProc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,proc,exe->gc);
	ctx->lcount=0;

	FklVMvalue* stdinVal=proc->closure[FKL_VM_STDIN_IDX]->v;
	ctx->stdinVal=stdinVal;
	ctx->codegen=codegen;
	NastCreatCtx* cc=createNastCreatCtx();
	ctx->cc=cc;
	ctx->state=READY;
	ctx->headSymbol=builtInHeadSymbolTable;
	utstring_new(ctx->buf);
}

