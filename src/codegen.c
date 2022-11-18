#include<fakeLisp/codegen.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/utils.h>
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
static FklSid_t builtInPatternVar_args=0;
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
	SUB_PATTERN_LIBRARY=2,
}SubPatternEnum;

static struct SubPattern
{
	char* ps;
	FklNastNode* pn;
}builtInSubPattern[]=
{
	{"(unquote value)",NULL,},
	{"(unqtesp value)",NULL,},
	{"(module name args,rest)",NULL,},
	{NULL,NULL,},
};

static FklByteCode* create1lenBc(FklOpcode op)
{
	FklByteCode* r=fklCreateByteCode(1);
	r->code[0]=op;
	return r;
}

static FklByteCode* create9lenBc(FklOpcode op,uint64_t size)
{
	FklByteCode* r=fklCreateByteCode(9);
	r->code[0]=op;
	fklSetU64ToByteCode(r->code+sizeof(char),size);
	return r;
}

static FklByteCode* createPushVar(FklSid_t id)
{
	FklByteCode* r=create9lenBc(FKL_OP_PUSH_VAR,0);
	fklSetSidToByteCode(r->code+sizeof(char),id);
	return r;
}

//static FklByteCode* createPopVar(uint32_t scope,FklSid_t id)
//{
//	FklByteCode* r=fklCreateByteCode(sizeof(char)+sizeof(uint32_t)+sizeof(FklSid_t));
//	r->code[0]=FKL_OP_POP_VAR;
//	fklSetU32ToByteCode(r->code+sizeof(char),scope);
//	fklSetSidToByteCode(r->code+sizeof(char)+sizeof(uint32_t),id);
//	return r;
//}

static FklByteCode* createPushTopPopVar(uint32_t scope,FklSid_t id)
{
	FklByteCode* r=fklCreateByteCode(sizeof(char)+sizeof(char)+sizeof(uint32_t)+sizeof(FklSid_t));
	r->code[0]=FKL_OP_PUSH_TOP;
	r->code[sizeof(char)]=FKL_OP_POP_VAR;
	fklSetU32ToByteCode(r->code+sizeof(char)+sizeof(char),scope);
	fklSetSidToByteCode(r->code+sizeof(char)+sizeof(char)+sizeof(uint32_t),id);
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

static FklByteCodelnt* makePushTopAndPopVar(const FklNastNode* sym,uint32_t scope,FklCodegen* codegen)
{
	FklSid_t sid=fklAddSymbol(fklGetGlobSymbolWithId(sym->u.sym)->symbol,codegen->globalSymTable)->id;
	FklByteCodelnt* r=createBclnt(createPushTopPopVar(scope,sid),codegen->fid,sym->curline);
	return r;
}

static void bclBcAppendToBcl(FklByteCodelnt* bcl
		,const FklByteCode* bc
		,FklSid_t fid
		,uint64_t line)
{
	if(!bcl->l)
	{
		bcl->ls=1;
		bcl->l=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*1);
		FKL_ASSERT(bcl->l);
		fklInitLineNumTabNode(&bcl->l[0],fid,0,bc->size,line);
		fklCodeCat(bcl->bc,bc);
	}
	else
	{
		fklCodeCat(bcl->bc,bc);
		bcl->l[bcl->ls-1].cpc+=bc->size;
	}
}

static void bcBclAppendToBcl(const FklByteCode* bc
		,FklByteCodelnt* bcl
		,FklSid_t fid
		,uint64_t line)
{
	if(!bcl->l)
	{
		bcl->ls=1;
		bcl->l=(FklLineNumTabNode*)malloc(sizeof(FklLineNumTabNode)*1);
		FKL_ASSERT(bcl->l);
		fklInitLineNumTabNode(&bcl->l[0],fid,0,bc->size,line);
		fklCodeCat(bcl->bc,bc);
	}
	else
	{
		fklReCodeCat(bc,bcl->bc);
		bcl->l[0].cpc+=bc->size;
		FKL_INCREASE_ALL_SCP(bcl->l+1,bcl->ls-1,bc->size);
	}
}

static FklCodegenQuest* createCodegenQuest(FklByteCodeProcesser f
		,FklCodegenQuestContext* context
		,FklCodegenNextExpression* nextExpression
		,FklCodegenEnv* env
		,uint64_t curline
		,FklCodegenQuest* prev
		,FklCodegen* codegen)
{
	FklCodegenQuest* r=(FklCodegenQuest*)malloc(sizeof(FklCodegenQuest));
	FKL_ASSERT(r);
	r->processer=f;
	r->context=context;
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
	quest->context->t->__finalizer(quest->context->data);
	free(quest->context);
	fklDestroyCodegener(quest->codegen);
	free(quest);
}

#define FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(F,STACK,NEXT_EXPRESSIONS,ENV,LINE,CODEGEN,CODEGEN_CONTEXT) fklPushPtrStack(createCodegenQuest((F),(STACK),(NEXT_EXPRESSIONS),(ENV),(LINE),NULL,(CODEGEN)),(CODEGEN_CONTEXT))
#define BC_PROCESS(NAME) static FklByteCodelnt* NAME(FklCodegen* codegen,FklCodegenQuestContext* context,FklSid_t fid,uint64_t line)

#define GET_STACK(CONTEXT) ((CONTEXT)->t->__get_bcl_stack((CONTEXT)->data))
BC_PROCESS(_default_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	if(!fklIsPtrStackEmpty(stack))
		return fklPopPtrStack(stack);
	else
		return NULL;
}

BC_PROCESS(_begin_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top)
	{
		uint8_t opcodes[]={
			FKL_OP_SET_TP,
			FKL_OP_RES_TP,
			FKL_OP_POP_TP,
		};
		FklByteCode setTp={1,&opcodes[0]};
		FklByteCode resTp={1,&opcodes[1]};
		FklByteCode popTp={1,&opcodes[2]};
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		size_t top=stack->top;
		for(size_t i=0;i<top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			if(cur->bc->size)
			{
				fklCodeLntCat(retval,cur);
				if(i<top-1)
					bclBcAppendToBcl(retval,&resTp,fid,line);
			}
			fklDestroyByteCodelnt(cur);
		}
		stack->top=0;
		bcBclAppendToBcl(&setTp,retval,fid,line);
		bclBcAppendToBcl(retval,&popTp,fid,line);
		return retval;
	}
	else
		return createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
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

BC_PROCESS(_funcall_exp_bc_process)
{
	FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
	FklPtrStack* stack=GET_STACK(context);
	while(!fklIsPtrStackEmpty(stack))
	{
		FklByteCodelnt* cur=fklPopPtrStack(stack);
		fklCodeLntCat(retval,cur);
		fklDestroyByteCodelnt(cur);
	}
	uint8_t opcodes[]={FKL_OP_SET_BP,FKL_OP_CALL};
	FklByteCode setBp={1,&opcodes[0],};
	FklByteCode call={1,&opcodes[1],};
	bcBclAppendToBcl(&setBp,retval,fid,line);
	bclBcAppendToBcl(retval,&call,fid,line);
	return retval;
}

static void codegen_funcall(FklNastNode* rest
		,FklPtrStack* codegenQuestStack
		,FklCodegenEnv* env
		,FklCodegen* codegen
		,FklCodegenErrorState* errorState)
{
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklNastNode* last=NULL;
	pushListItemToQueue(rest,queue,&last);
	if(last->type!=FKL_NAST_NIL)
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
				,env
				,rest->curline
				,codegen
				,codegenQuestStack);
}

#define CODEGEN_ARGS FklNastNode* origExp\
		,FklHashTable* ht\
		,FklPtrStack* codegenQuestStack\
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
			,curEnv
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
			FKL_OP_SET_TP,
			FKL_OP_RES_TP,
			FKL_OP_POP_TP,
			FKL_OP_PUSH_R_ENV,
			FKL_OP_POP_R_ENV,
		};
		uint8_t jmpOpcodes[9]={FKL_OP_JMP_IF_FALSE,};
		FklByteCode setTp={1,&opcodes[0]};
		FklByteCode resTp={1,&opcodes[1]};
		FklByteCode popTp={1,&opcodes[2]};
		FklByteCode pushREnv={1,&opcodes[3]};
		FklByteCode popREnv={1,&opcodes[4]};
		FklByteCode jmpIfFalse={9,jmpOpcodes};
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		while(!fklIsPtrStackEmpty(stack))
		{
			if(retval->bc->size)
			{
				bcBclAppendToBcl(&resTp,retval,fid,line);
				fklSetI64ToByteCode(&jmpOpcodes[1],retval->bc->size);
				bcBclAppendToBcl(&jmpIfFalse,retval,fid,line);
			}
			FklByteCodelnt* cur=fklPopPtrStack(stack);
			fklReCodeLntCat(cur,retval);
			fklDestroyByteCodelnt(cur);
		}
		if(retval->bc->size)
		{
			bcBclAppendToBcl(&pushREnv,retval,fid,line);
			bclBcAppendToBcl(retval,&popREnv,fid,line);
		}
		bcBclAppendToBcl(&setTp,retval,fid,line);
		bclBcAppendToBcl(retval,&popTp,fid,line);
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
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_and_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createDefaultQueueNextExpression(queue)
			,fklCreateCodegenEnv(curEnv)
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
			FKL_OP_SET_TP,
			FKL_OP_RES_TP,
			FKL_OP_POP_TP,
			FKL_OP_PUSH_R_ENV,
			FKL_OP_POP_R_ENV,
		};
		uint8_t jmpOpcodes[9]={FKL_OP_JMP_IF_TRUE,};
		FklByteCode setTp={1,&opcodes[0]};
		FklByteCode resTp={1,&opcodes[1]};
		FklByteCode popTp={1,&opcodes[2]};
		FklByteCode pushREnv={1,&opcodes[3]};
		FklByteCode popREnv={1,&opcodes[4]};
		FklByteCode jmpIfTrue={9,jmpOpcodes};
		FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		while(!fklIsPtrStackEmpty(stack))
		{
			if(retval->bc->size)
			{
				bcBclAppendToBcl(&resTp,retval,fid,line);
				fklSetI64ToByteCode(&jmpOpcodes[1],retval->bc->size);
				bcBclAppendToBcl(&jmpIfTrue,retval,fid,line);
			}
			FklByteCodelnt* cur=fklPopPtrStack(stack);
			fklReCodeLntCat(cur,retval);
			fklDestroyByteCodelnt(cur);
		}
		if(retval->bc->size)
		{
			bcBclAppendToBcl(&pushREnv,retval,fid,line);
			bclBcAppendToBcl(retval,&popREnv,fid,line);
		}
		bcBclAppendToBcl(&setTp,retval,fid,line);
		bclBcAppendToBcl(retval,&popTp,fid,line);
		return retval;
	}
	else
		return createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
}

static CODEGEN_FUNC(codegen_or)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_or_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createDefaultQueueNextExpression(queue)
			,curEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}


BC_PROCESS(_lambda_exp_bc_process)
{
	FklByteCodelnt* retval=NULL;
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top>1)
	{
		uint8_t opcodes[]={
			FKL_OP_SET_TP,
			FKL_OP_RES_TP,
			FKL_OP_POP_TP,
		};
		FklByteCode setTp={1,&opcodes[0]};
		FklByteCode resTp={1,&opcodes[1]};
		FklByteCode popTp={1,&opcodes[2]};
		retval=fklCreateByteCodelnt(fklCreateByteCode(0));
		size_t top=stack->top;
		for(size_t i=1;i<top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			if(cur->bc->size)
			{
				fklCodeLntCat(retval,cur);
				if(i<top-1)
					bclBcAppendToBcl(retval,&resTp,fid,line);
			}
			fklDestroyByteCodelnt(cur);
		}
		bcBclAppendToBcl(&setTp,retval,fid,line);
		bclBcAppendToBcl(retval,&popTp,fid,line);
	}
	else
		retval=createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
	fklReCodeLntCat(stack->base[0],retval);
	fklDestroyByteCodelnt(stack->base[0]);
	stack->top=0;
	FklByteCode* pushProc=create9lenBc(FKL_OP_PUSH_PROC,retval->bc->size);
	bcBclAppendToBcl(pushProc,retval,fid,line);
	fklDestroyByteCode(pushProc);
	return retval;
}

static FklByteCodelnt* makePopArg(FklOpcode code,const FklNastNode* sym,FklCodegen* codegen)
{
	FklByteCode* bc=create9lenBc(code,0);
	FklSid_t sid=fklAddSymbol(fklGetGlobSymbolWithId(sym->u.sym)->symbol,codegen->globalSymTable)->id;
	fklSetSidToByteCode(bc->code+sizeof(char),sid);
	return createBclnt(bc,codegen->fid,sym->curline);
}

typedef struct
{
	FklSid_t id;
}FklCodegenEnvHashItem;

static FklCodegenEnvHashItem* createCodegenEnvHashItem(FklSid_t key)
{
	FklCodegenEnvHashItem* r=(FklCodegenEnvHashItem*)malloc(sizeof(FklCodegenEnvHashItem));
	FKL_ASSERT(r);
	r->id=key;
	return r;
}

static size_t _codegenenv_hashFunc(void* key)
{
	FklSid_t sid=*(FklSid_t*)key;
	return sid;
}

static void _codegenenv_destroyItem(void* item)
{
	free(item);
}

static int _codegenenv_keyEqual(void* pkey0,void* pkey1)
{
	FklSid_t k0=*(FklSid_t*)pkey0;
	FklSid_t k1=*(FklSid_t*)pkey1;
	return k0==k1;
}

static void* _codegenenv_getKey(void* item)
{
	return &((FklCodegenEnvHashItem*)item)->id;
}

static FklHashTableMethodTable CodegenEnvHashMethodTable=
{
	.__hashFunc=_codegenenv_hashFunc,
	.__destroyItem=_codegenenv_destroyItem,
	.__keyEqual=_codegenenv_keyEqual,
	.__getKey=_codegenenv_getKey,
};

typedef struct
{
	FklSid_t id;
	FklNastNode* node;
}FklCodegenReplacement;

static FklCodegenReplacement* createCodegenReplacement(FklSid_t key,FklNastNode* node)
{
	FklCodegenReplacement* r=(FklCodegenReplacement*)malloc(sizeof(FklCodegenReplacement));
	FKL_ASSERT(r);
	r->id=key;
	r->node=fklMakeNastNodeRef(node);
	return r;
}

static void _codegen_replacement_destroyItem(void* item)
{
	FklCodegenReplacement* replace=(FklCodegenReplacement*)item;
	fklDestroyNastNode(replace->node);
	free(replace);
}

static void* _codegen_replacement_getKey(void* item)
{
	return &((FklCodegenReplacement*)item)->id;
}

static FklHashTableMethodTable CodegenReplacementHashMethodTable=
{
	.__hashFunc=_codegenenv_hashFunc,
	.__destroyItem=_codegen_replacement_destroyItem,
	.__keyEqual=_codegenenv_keyEqual,
	.__getKey=_codegen_replacement_getKey,
};

FklCodegenEnv* fklCreateCodegenEnv(FklCodegenEnv* prev)
{
	FklCodegenEnv* r=(FklCodegenEnv*)malloc(sizeof(FklCodegenEnv));
	FKL_ASSERT(r);
	r->prev=prev;
	r->refcount=0;
	r->defs=fklCreateHashTable(8,4,2,0.75,1,&CodegenEnvHashMethodTable);
	if(prev)
	{
		r->replacements=fklCreateHashTable(8,4,2,0.75,1,&CodegenReplacementHashMethodTable);
		prev->refcount+=1;
		r->macros=fklCreateCodegenMacroScope(prev->macros);
	}
	else
	{
		r->replacements=NULL;
		r->macros=NULL;
	}
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
			fklDestroyHashTable(cur->defs);
			if(cur->prev)
			{
				fklDestroyHashTable(cur->replacements);
				fklDestroyCodegenMacroScope(cur->macros);
			}
			free(cur);
		}
		else
			break;
	}
}


FklCodegenEnv* fklCreateGlobCodegenEnv(void)
{
	FklCodegenEnv* r=fklCreateCodegenEnv(NULL);
	fklInitGlobCodegenEnv(r);
	return r;
}

void fklAddCodegenDefBySid(FklSid_t id,FklCodegenEnv* env)
{
	fklPutNoRpHashItem(createCodegenEnvHashItem(id),env->defs);
}

void fklAddReplacementBySid(FklSid_t id,FklNastNode* node,FklCodegenEnv* env)
{
	fklPutNoRpHashItem(createCodegenReplacement(id,node),env->replacements);
}

int fklIsSymbolDefined(FklSid_t id,FklCodegenEnv* env)
{
	return fklGetHashItem(&id,env->defs)!=NULL;
}

int fklIsReplacementDefined(FklSid_t id,FklCodegenEnv* env)
{
	return fklGetHashItem(&id,env->replacements)!=NULL;
}

FklNastNode* fklGetReplacement(FklSid_t id,FklCodegenEnv* env)
{
	FklCodegenReplacement* replace=fklGetHashItem(&id,env->replacements);
	if(replace)
		return fklCopyNastNode(replace->node);
	return NULL;
}

static FklByteCodelnt* processArgs(const FklNastNode* args,FklCodegenEnv* curEnv,FklCodegen* codegen)
{
	FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
	for(;args->type==FKL_NAST_PAIR;args=args->u.pair->cdr)
	{
		FklNastNode* cur=args->u.pair->car;
		if(cur->type!=FKL_NAST_SYM)
		{
			fklDestroyByteCodelnt(retval);
			return NULL;
		}
		fklAddCodegenDefBySid(cur->u.sym,curEnv);
		FklByteCodelnt* tmp=makePopArg(FKL_OP_POP_ARG,cur,codegen);
		fklCodeLntCat(retval,tmp);
		fklDestroyByteCodelnt(tmp);
	}
	if(args->type!=FKL_NAST_NIL&&args->type!=FKL_NAST_SYM)
	{
		fklDestroyByteCodelnt(retval);
		return NULL;
	}
	if(args->type==FKL_NAST_SYM)
	{
		FklByteCodelnt* tmp=makePopArg(FKL_OP_POP_REST_ARG,args,codegen);
		fklCodeLntCat(retval,tmp);
		fklDestroyByteCodelnt(tmp);
	}
	uint8_t opcodes[]={FKL_OP_RES_BP,};
	FklByteCode resBp={1,&opcodes[0]};
	bclBcAppendToBcl(retval,&resBp,codegen->fid,args->curline);
	return retval;
}

static CODEGEN_FUNC(codegen_lambda)
{
	FklNastNode* args=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	FklCodegenEnv* lambdaCodegenEnv=fklCreateCodegenEnv(curEnv);
	FklByteCodelnt* argsBc=processArgs(args,lambdaCodegenEnv,codegen);
	if(!argsBc)
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		fklDestroyCodegenEnv(lambdaCodegenEnv);
		return;
	}
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	FklPtrStack* stack=fklCreatePtrStack(32,16);
	fklPushPtrStack(argsBc,stack);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_lambda_exp_bc_process
			,createDefaultStackContext(stack)
			,createDefaultQueueNextExpression(queue)
			,lambdaCodegenEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}

BC_PROCESS(_set_var_exp_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* cur=fklPopPtrStack(stack);
	FklByteCodelnt* popVar=fklPopPtrStack(stack);
	fklReCodeLntCat(cur,popVar);
	fklDestroyByteCodelnt(cur);
	return popVar;
}

static CODEGEN_FUNC(codegen_define)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklPtrStack* stack=fklCreatePtrStack(2,1);
	fklAddCodegenDefBySid(name->u.sym,curEnv);
	fklPushPtrStack(makePushTopAndPopVar(name,1,codegen),stack);
	fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_set_var_exp_bc_process
			,createDefaultStackContext(stack)
			,createDefaultQueueNextExpression(queue)
			,curEnv
			,name->curline
			,codegen
			,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_setq)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklPtrStack* stack=fklCreatePtrStack(2,1);
	int isDefined=0;
	uint32_t scope=0;
	for(FklCodegenEnv* cEnv=curEnv;cEnv&&!isDefined;cEnv=cEnv->prev,scope++)
		isDefined=fklIsSymbolDefined(name->u.sym,cEnv);
	if(!isDefined)
		scope=0;
	fklPushPtrStack(makePushTopAndPopVar(name,scope,codegen),stack);
	fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_set_var_exp_bc_process
			,createDefaultStackContext(stack)
			,createDefaultQueueNextExpression(queue)
			,curEnv
			,name->curline
			,codegen
			,codegenQuestStack);
}

inline static void push_default_codegen_quest(FklNastNode* value
		,FklPtrStack* codegenQuestStack
		,FklCodegenEnv* curEnv
		,FklCodegenQuest* prev
		,FklCodegen* codegen)
{
	FklPtrStack* stack=fklCreatePtrStack(1,1);
	fklPushPtrStack(createBclnt(fklCodegenNode(value,codegen),codegen->fid,value->curline),stack);
	FklCodegenQuest* quest=createCodegenQuest(_default_bc_process
			,createDefaultStackContext(stack)
			,NULL
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
			,curEnv
			,NULL
			,codegen);
	fklDestroyNastNode(value);
}

static CODEGEN_FUNC(codegen_quote)
{
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	push_default_codegen_quest(value
			,codegenQuestStack
			,curEnv
			,NULL
			,codegen);
}

inline static void unquoteHelperFunc(FklNastNode* value
		,FklPtrStack* codegenQuestStack
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
			,curEnv
			,value->curline
			,prev
			,codegen);
	fklPushPtrStack(quest,codegenQuestStack);
}

static CODEGEN_FUNC(codegen_unquote)
{
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	unquoteHelperFunc(value,codegenQuestStack,curEnv,_default_bc_process,NULL,codegen);
}

BC_PROCESS(_qsquote_box_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* retval=fklPopPtrStack(stack);
	FklByteCode* pushBox=create1lenBc(FKL_OP_PUSH_BOX);
	bclBcAppendToBcl(retval,pushBox,fid,line);
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
	bcBclAppendToBcl(setBp,retval,fid,line);
	bclBcAppendToBcl(retval,pushVec,fid,line);
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
	bclBcAppendToBcl(retval,listPush,fid,line);
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
	bcBclAppendToBcl(setBp,retval,fid,line);
	bclBcAppendToBcl(retval,pushPair,fid,line);
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
	bclBcAppendToBcl(retval,pushPair,fid,line);
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
	FklPtrStack* valueStack=fklCreatePtrStack(8,16);
	fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,value,NULL),valueStack);
	while(!fklIsPtrStackEmpty(valueStack))
	{
		QsquoteHelperStruct* curNode=fklPopPtrStack(valueStack);
		QsquoteHelperEnum e=curNode->e;
		FklCodegenQuest* prevQuest=curNode->prev;
		FklNastNode* curValue=curNode->node;
		free(curNode);
		if(e==QSQUOTE_UNQTESP_CAR)
			unquoteHelperFunc(curValue,codegenQuestStack,curEnv,_default_bc_process,prevQuest,codegen);
		else if(e==QSQUOTE_UNQTESP_VEC)
			unquoteHelperFunc(curValue,codegenQuestStack,curEnv,_unqtesp_vec_bc_process,prevQuest,codegen);
		else
		{
			FklHashTable* unquoteHt=fklCreatePatternMatchingHashTable();
			if(fklPatternMatch(builtInSubPattern[SUB_PATTERN_UNQUOTE].pn,curValue,unquoteHt))
			{
				FklNastNode* unquoteValue=fklPatternMatchingHashTableRef(builtInPatternVar_value,unquoteHt);
				unquoteHelperFunc(unquoteValue
						,codegenQuestStack
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
									,curEnv
									,curValue->curline
									,curQuest
									,codegen);
							fklPushPtrStack(appendQuest,codegenQuestStack);
							fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_UNQTESP_CAR,unqtespValue,appendQuest),valueStack);
							fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,node->u.pair->cdr,appendQuest),valueStack);
						}
						else
							fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_UNQTESP_CAR,unqtespValue,curQuest),valueStack);
						break;
					}
					else
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,cur,curQuest),valueStack);
					node=node->u.pair->cdr;
					if(node->type!=FKL_NAST_PAIR)
					{
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,node,curQuest),valueStack);
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
								,valueStack);
					else
						fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,curValue->u.vec->base[i],curQuest),valueStack);
				}
			}
			else if(curValue->type==FKL_NAST_BOX)
			{
				FklCodegenQuest* curQuest=createCodegenQuest(_qsquote_box_bc_process
						,createDefaultStackContext(fklCreatePtrStack(1,1))
						,NULL
						,curEnv
						,curValue->curline
						,prevQuest
						,codegen);
				fklPushPtrStack(curQuest,codegenQuestStack);
				fklPushPtrStack(createQsquoteHelperStruct(QSQUOTE_NONE,curValue->u.box,curQuest),valueStack);
			}
			else
				push_default_codegen_quest(curValue,codegenQuestStack,curEnv,prevQuest,codegen);
			fklDestroyHashTable(unquoteHt);
		}
	}
	fklDestroyPtrStack(valueStack);
}

BC_PROCESS(_cond_exp_bc_process_0)
{
    FklByteCodelnt* retval=NULL;
	FklPtrStack* stack=GET_STACK(context);
    if(stack->top)
    {
        retval=fklPopPtrStack(stack);
        FklByteCode* setTp=create1lenBc(FKL_OP_SET_TP);
        FklByteCode* popTp=create1lenBc(FKL_OP_POP_TP);
        bcBclAppendToBcl(setTp,retval,fid,line);
        bclBcAppendToBcl(retval,popTp,fid,line);
        fklDestroyByteCode(setTp);
        fklDestroyByteCode(popTp);
    }
    else
        retval=createBclnt(create1lenBc(FKL_OP_PUSH_NIL),fid,line);
    return retval;
}

BC_PROCESS(_cond_exp_bc_process_1)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* prev=stack->base[0];
	FklByteCodelnt* first=stack->base[1];
	FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
	FklByteCode* resTp=create1lenBc(FKL_OP_RES_TP);
	FklByteCode* pushREnv=create1lenBc(FKL_OP_PUSH_R_ENV);
	FklByteCode* popREnv=create1lenBc(FKL_OP_POP_R_ENV);
	bcBclAppendToBcl(resTp,prev,fid,line);
	FklByteCode* jmp=create9lenBc(FKL_OP_JMP,prev->bc->size);
	for(size_t i=2;i<stack->top;i++)
	{
		FklByteCodelnt* cur=stack->base[i];
		bclBcAppendToBcl(retval,resTp,fid,line);
		fklCodeLntCat(retval,cur);
		fklDestroyByteCodelnt(cur);
	}
	stack->top=0;
	bclBcAppendToBcl(retval,jmp,fid,line);
	FklByteCode* jmpIfFalse=create9lenBc(FKL_OP_JMP_IF_FALSE,retval->bc->size);
	bcBclAppendToBcl(jmpIfFalse,retval,fid,line);
	bclBcAppendToBcl(retval,popREnv,fid,line);
	fklCodeLntCat(retval,prev);
	fklDestroyByteCodelnt(prev);
	fklReCodeLntCat(first,retval);
	fklDestroyByteCodelnt(first);
	bcBclAppendToBcl(pushREnv,retval,fid,line);
	fklDestroyByteCode(jmp);
	fklDestroyByteCode(jmpIfFalse);
	fklDestroyByteCode(pushREnv);
	fklDestroyByteCode(popREnv);
	fklDestroyByteCode(resTp);
	return retval;
}

BC_PROCESS(_cond_exp_bc_process_2)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* first=stack->base[0];
	FklByteCodelnt* retval=fklCreateByteCodelnt(fklCreateByteCode(0));
	FklByteCode* resTp=create1lenBc(FKL_OP_RES_TP);
	FklByteCode* pushREnv=create1lenBc(FKL_OP_PUSH_R_ENV);
	FklByteCode* popREnv=create1lenBc(FKL_OP_POP_R_ENV);
	for(size_t i=1;i<stack->top;i++)
	{
		FklByteCodelnt* cur=stack->base[i];
		bclBcAppendToBcl(retval,resTp,fid,line);
		fklCodeLntCat(retval,cur);
		fklDestroyByteCodelnt(cur);
	}
	stack->top=0;
	if(retval->bc->size)
	{
		FklByteCode* jmpIfFalse=create9lenBc(FKL_OP_JMP_IF_FALSE,retval->bc->size);
		bcBclAppendToBcl(jmpIfFalse,retval,fid,line);
		fklDestroyByteCode(jmpIfFalse);
	}
	fklReCodeLntCat(first,retval);
	fklDestroyByteCodelnt(first);
	bclBcAppendToBcl(retval,popREnv,fid,line);
	bcBclAppendToBcl(pushREnv,retval,fid,line);
	fklDestroyByteCode(pushREnv);
	fklDestroyByteCode(popREnv);
	fklDestroyByteCode(resTp);
	return retval;
}

static CODEGEN_FUNC(codegen_cond)
{
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	FklCodegenQuest* quest=createCodegenQuest(_cond_exp_bc_process_0
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,NULL
			,curEnv
			,rest->curline
			,NULL
			,codegen);
	fklPushPtrStack(quest,codegenQuestStack);
	if(rest->type!=FKL_NAST_NIL)
	{
		FklPtrStack* tmpStack=fklCreatePtrStack(32,16);
		pushListItemToStack(rest,tmpStack,NULL);
		FklNastNode* lastExp=fklPopPtrStack(tmpStack);
		FklCodegenQuest* prevQuest=quest;
		for(size_t i=0;i<tmpStack->top;i++)
		{
			FklNastNode* curExp=tmpStack->base[i];
			if(curExp->type!=FKL_NAST_PAIR)
			{
				errorState->fid=codegen->fid;
				errorState->type=FKL_ERR_SYNTAXERROR;
				errorState->place=fklMakeNastNodeRef(origExp);
				fklDestroyPtrStack(tmpStack);
				return;
			}
			FklNastNode* last=NULL;
			FklPtrQueue* curQueue=fklCreatePtrQueue();
			pushListItemToQueue(curExp,curQueue,&last);
			if(last->type!=FKL_NAST_NIL)
			{
				errorState->fid=codegen->fid;
				errorState->type=FKL_ERR_SYNTAXERROR;
				errorState->place=fklMakeNastNodeRef(origExp);
				while(!fklIsPtrQueueEmpty(curQueue))
					fklDestroyNastNode(fklPopPtrQueue(curQueue));
				fklDestroyPtrQueue(curQueue);
				fklDestroyPtrStack(tmpStack);
				return;
			}
			FklCodegenQuest* curQuest=createCodegenQuest(_cond_exp_bc_process_1
					,createDefaultStackContext(fklCreatePtrStack(32,16))
					,createDefaultQueueNextExpression(curQueue)
					,fklCreateCodegenEnv(curEnv)
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
			fklDestroyPtrStack(tmpStack);
			return;
		}
		FklPtrQueue* lastQueue=fklCreatePtrQueue();
		pushListItemToQueue(lastExp,lastQueue,&last);
		if(last->type!=FKL_NAST_NIL)
		{
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_SYNTAXERROR;
			errorState->place=fklMakeNastNodeRef(origExp);
			while(!fklIsPtrQueueEmpty(lastQueue))
				fklDestroyNastNode(fklPopPtrQueue(lastQueue));
			fklDestroyPtrQueue(lastQueue);
			fklDestroyPtrStack(tmpStack);
			return;
		}
		fklPushPtrStack(createCodegenQuest(_cond_exp_bc_process_2
					,createDefaultStackContext(fklCreatePtrStack(32,16))
					,createDefaultQueueNextExpression(lastQueue)
					,fklCreateCodegenEnv(curEnv)
					,lastExp->curline
					,prevQuest
					,codegen)
				,codegenQuestStack);
		fklDestroyPtrStack(tmpStack);
	}
}

//static FklPtrQueue* readExpressionIntoQueue(FklCodegen* codegen,FILE* fp,FklCodegenErrorState* errorState)
//{
//	FklPtrQueue* queue=fklCreatePtrQueue();
//	FklPtrStack* tokenStack=fklCreatePtrStack(32,16);
//	size_t size=0;
//	char* prev=NULL;
//	size_t prevSize=0;
//	for(;;)
//	{
//		int unexpectEOF=0;
//		FklNastNode* begin=NULL;
//		char* list=fklReadInStringPattern(fp,&prev,&size,&prevSize,codegen->curline,&unexpectEOF,tokenStack,NULL);
//		if(unexpectEOF)
//		{
//			errorState->line=codegen->curline;
//			switch(unexpectEOF)
//			{
//				case 1:
//					errorState->type=FKL_ERR_UNEXPECTEOF;
//					break;
//				case 2:
//					errorState->type=FKL_ERR_INVALIDEXPR;
//					break;
//			}
//			while(!fklIsPtrQueueEmpty(queue))
//				fklDestroyNastNode(fklPopPtrQueue(queue));
//			fklDestroyPtrStack(tokenStack);
//			fklDestroyPtrQueue(queue);
//			return NULL;
//		}
//		size_t errorLine=0;
//		codegen->curline+=fklCountChar(list,'\n',size);
//		begin=fklCreateNastNodeFromTokenStack(tokenStack,&errorLine);
//		free(list);
//		if(fklIsPtrStackEmpty(tokenStack))
//			break;
//		if(!begin)
//		{
//			if(!fklIsAllComment(tokenStack))
//			{
//				errorState->type=FKL_ERR_INVALIDEXPR;
//				errorState->line=errorLine;
//				while(!fklIsPtrQueueEmpty(queue))
//					fklDestroyNastNode(fklPopPtrQueue(queue));
//				fklDestroyPtrQueue(queue);
//				fklDestroyPtrStack(tokenStack);
//				return NULL;
//			}
//		}
//		else
//			fklPushPtrQueue(begin,queue);
//		while(!fklIsPtrStackEmpty(tokenStack))
//			fklDestroyToken(fklPopPtrStack(tokenStack));
//	}
//	fklDestroyPtrStack(tokenStack);
//	return queue;
//}

static FklCodegen* createCodegen(FklCodegen* prev
		,const char* filename
		,FklCodegenEnv* env)
{
	FklCodegen* r=(FklCodegen*)malloc(sizeof(FklCodegen));
	FKL_ASSERT(r);
	fklInitCodegener(r,filename,env,prev,prev->globalSymTable,1);
	return r;
}

typedef struct
{
	char* prev;
	size_t prevSize;
	FILE* fp;
	FklCodegen* codegen;
	FklPtrStack* tokenStack;
}CodegenLoadContext;

static CodegenLoadContext* createCodegenLoadContext(FILE* fp,FklCodegen* codegen)
{
	CodegenLoadContext* r=(CodegenLoadContext*)malloc(sizeof(CodegenLoadContext));
	FKL_ASSERT(r);
	r->codegen=codegen;
	r->fp=fp;
	r->tokenStack=fklCreatePtrStack(32,16);
	r->prev=NULL;
	r->prevSize=0;
	return r;
}

static void _codegen_load_finalizer(void* pcontext)
{
	CodegenLoadContext* context=pcontext;
	FklPtrStack* tokenStack=context->tokenStack;
	while(!fklIsPtrStackEmpty(tokenStack))
		fklDestroyToken(fklPopPtrStack(tokenStack));
	fklDestroyPtrStack(tokenStack);
	fclose(context->fp);
	free(context->prev);
	free(context);
}

inline static FklNastNode* getExpressionFromFile(FklCodegen* codegen
		,FILE* fp
		,char** prev
		,size_t* prevSize
		,int* unexpectEOF
		,FklPtrStack* tokenStack
		,size_t* errorLine
		,int* hasError)
{
	size_t size=0;
	FklNastNode* begin=NULL;
	FklStringMatchRouteNode* route=NULL;
	char* list=fklReadInStringPattern(fp
			,prev
			,&size
			,prevSize
			,codegen->curline
			,&codegen->curline
			,unexpectEOF
			,tokenStack,NULL
			,codegen->patterns
			,&route);
	if(*unexpectEOF)
		return NULL;
	free(list);
	if(fklIsAllComment(tokenStack))
	{
		begin=NULL;
		*hasError=0;
	}
	else
	{
		begin=fklCreateNastNodeFromTokenStackAndMatchRoute(tokenStack,route,errorLine,builtInHeadSymbolTable);
		*hasError=(begin==NULL);
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
	FklPtrStack* tokenStack=context->tokenStack;
	FILE* fp=context->fp;
	int unexpectEOF=0;
	size_t errorLine=0;
	int hasError=0;
	FklNastNode* begin=getExpressionFromFile(codegen
			,fp
			,&context->prev
			,&context->prevSize
			,&unexpectEOF
			,tokenStack
			,&errorLine
			,&hasError);
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
		errorState->line=errorLine;
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
	if(filename->type!=FKL_NAST_STR)
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	char* filenameCstr=fklStringToCstr(filename->u.str);
	if(!fklIsAccessableScriptFile(filenameCstr))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_FILEFAILURE;
		errorState->place=fklMakeNastNodeRef(filename);
		free(filenameCstr);
		return;
	}
	FklCodegen* nextCodegen=createCodegen(codegen,filenameCstr,curEnv);
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
			,curEnv
			,origExp->curline
			,nextCodegen
			,codegenQuestStack);
}

inline static char* combineFileNameFromListAndGetLastNode(FklNastNode* list,FklNastNode** last)
{
	char* r=NULL;
	for(FklNastNode* curPair=list;curPair->type==FKL_NAST_PAIR;curPair=curPair->u.pair->cdr)
	{
		FklNastNode* cur=curPair->u.pair->car;
		r=fklCstrStringCat(r,fklGetGlobSymbolWithId(cur->u.sym)->symbol);
		if(curPair->u.pair->cdr->type==FKL_NAST_NIL)
			*last=cur;
		else
			r=fklStrCat(r,FKL_PATH_SEPARATOR_STR);
	}
	return r;
}

static void add_symbol_with_prefix_to_locale_env_in_list(const FklNastNode* list
		,FklSid_t prefixId
		,FklCodegenEnv* env
		,FklSymbolTable* globalSymTable
		,FklUintStack* idStack
		,FklUintStack* idWithPrefixStack)
{
	FklString* prefix=fklGetGlobSymbolWithId(prefixId)->symbol;
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr)
	{
		FklNastNode* cur=list->u.pair->car;
		FklString* origSymbol=fklGetGlobSymbolWithId(cur->u.sym)->symbol;
		FklString* symbolWithPrefix=fklStringAppend(prefix,origSymbol);
		FklSid_t sym=fklAddSymbolToGlob(symbolWithPrefix)->id;
		fklAddCodegenDefBySid(sym,env);
		fklPushUintStack(fklAddSymbol(origSymbol,globalSymTable)->id,idStack);
		fklPushUintStack(fklAddSymbol(symbolWithPrefix,globalSymTable)->id,idWithPrefixStack);
		free(symbolWithPrefix);
	}
}

static void add_symbol_to_locale_env_in_list(const FklNastNode* list,FklCodegenEnv* env,FklSymbolTable* globalSymTable,FklUintStack* idStack)
{
	for(;list->type==FKL_NAST_PAIR;list=list->u.pair->cdr)
	{
		FklNastNode* cur=list->u.pair->car;
		fklAddCodegenDefBySid(cur->u.sym,env);
		fklPushUintStack(fklAddSymbol(fklGetGlobSymbolWithId(cur->u.sym)->symbol,globalSymTable)->id,idStack);
	}
}

static void add_symbol_to_locale_env_in_array(FklCodegenEnv* env,FklSymbolTable* symbolTable,size_t num,FklSid_t* exports)
{
	for(size_t i=0;i<num;i++)
	{
		FklSid_t sym=fklAddSymbolToGlob(fklGetSymbolWithId(exports[i],symbolTable)->symbol)->id;
		fklAddCodegenDefBySid(sym,env);
	}
}

static void add_symbol_with_prefix_to_locale_env_in_array(FklCodegenEnv* env
		,FklSymbolTable* symbolTable
		,FklSid_t prefixId
		,size_t num
		,FklSid_t* exports
		,FklSid_t* exportsWithPrefix)
{
	FklString* prefix=fklGetGlobSymbolWithId(prefixId)->symbol;
	for(size_t i=0;i<num;i++)
	{
		FklString* origSymbol=fklGetSymbolWithId(exports[i],symbolTable)->symbol;
		FklString* symbolWithPrefix=fklStringAppend(prefix,origSymbol);
		FklSid_t sym=fklAddSymbolToGlob(symbolWithPrefix)->id;
		fklAddCodegenDefBySid(sym,env);
		exportsWithPrefix[i]=fklAddSymbol(symbolWithPrefix,symbolTable)->id;
		free(symbolWithPrefix);
	}
}

static void add_compiler_macro(FklCodegenMacroScope* macros,FklNastNode* pattern,FklByteCodelnt* bcl)
{
	int coverState=0;
	FklCodegenMacro** pmacro=&macros->head;
	for(;*pmacro;pmacro=&(*pmacro)->next)
	{
		FklCodegenMacro* cur=*pmacro;
		coverState=fklPatternCoverState(cur->pattern,pattern);
		if(coverState)
			break;
	}
	if(!coverState)
	{
		FklCodegenMacro* macro=fklCreateCodegenMacro(pattern,bcl,macros->head);
		macros->head=macro;
	}
	else
	{
		if(coverState==FKL_PATTERN_COVER)
		{
			FklCodegenMacro* next=*pmacro;
			*pmacro=fklCreateCodegenMacro(pattern,bcl,next);
		}
		else if(coverState==FKL_PATTERN_BE_COVER)
		{
			FklCodegenMacro* cur=(*pmacro);
			FklCodegenMacro* next=cur->next;
			cur->next=fklCreateCodegenMacro(pattern,bcl,next);
		}
		else
		{
			FklCodegenMacro* macro=*pmacro;
			fklDestroyNastNode(macro->pattern);
			fklDestroyByteCodelnt(macro->bcl);
			macro->pattern=fklMakeNastNodeRef(pattern);
			macro->bcl=bcl;
		}
	}
	fklDestroyNastNode(pattern);
}

BC_PROCESS(_library_bc_process)
{
	FklByteCodelnt* libBc=NULL;
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top>2)
	{
		uint8_t opcodes[]={
			FKL_OP_SET_TP,
			FKL_OP_RES_TP,
			FKL_OP_POP_TP,
		};
		FklByteCode setTp={1,&opcodes[0]};
		FklByteCode resTp={1,&opcodes[1]};
		FklByteCode popTp={1,&opcodes[2]};
		FklByteCodelnt* r=fklCreateByteCodelnt(fklCreateByteCode(0));
		size_t top=stack->top;
		for(size_t i=2;i<top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			if(cur->bc->size)
			{
				fklCodeLntCat(r,cur);
				bclBcAppendToBcl(r,&resTp,fid,line);
			}
			fklDestroyByteCodelnt(cur);
		}
		bcBclAppendToBcl(&setTp,r,fid,line);
		bclBcAppendToBcl(r,&popTp,fid,line);
		libBc=r;
	}
	else
		libBc=createBclnt(fklCreateByteCode(0),fid,line);
	fklPushPtrStack(fklCreateCodegenLib(codegen->realpath
				,libBc
				,codegen->exportNum
				,codegen->exports
				,codegen->globalEnv->macros->head),codegen->loadedLibStack);
	FklCodegenEnv* exportTargetEnv=stack->base[0];
	for(FklCodegenMacro* cur=codegen->globalEnv->macros->head;cur;cur=cur->next)
		add_compiler_macro(exportTargetEnv->macros,fklMakeNastNodeRef(cur->pattern),fklCopyByteCodelnt(cur->bcl));
	fklDestroyCodegenEnv(exportTargetEnv);
	codegen->globalEnv->macros->head=NULL;
	FklByteCodelnt* retval=stack->base[1];
	stack->top=0;
	fklSetU64ToByteCode(retval->bc->code+sizeof(char),codegen->loadedLibStack->top);
	codegen->exportNum=0;
	codegen->exports=NULL;
	codegen->realpath=NULL;
	return retval;
}

static FklNastNode* createPatternWithPrefixFromOrig(FklNastNode* orig,FklString* prefix)
{
	FklString* head=fklGetGlobSymbolWithId(orig->u.pair->car->u.sym)->symbol;
	FklString* symbolWithPrefix=fklStringAppend(prefix,head);
	FklNastNode* patternWithPrefix=fklCreateNastNode(FKL_NAST_PAIR,orig->curline);
	FklNastNode* newHead=fklCreateNastNode(FKL_NAST_SYM,orig->u.pair->car->curline);
	newHead->u.sym=fklAddSymbolToGlob(symbolWithPrefix)->id;
	patternWithPrefix->u.pair=fklCreateNastPair();
	patternWithPrefix->u.pair->car=fklMakeNastNodeRef(newHead);
	patternWithPrefix->u.pair->cdr=fklMakeNastNodeRef(orig->u.pair->cdr);
	free(symbolWithPrefix);
	return patternWithPrefix;
}

BC_PROCESS(_library_be_imported_with_prefix_bc_process)
{
	FklByteCodelnt* libBc=NULL;
	FklPtrStack* stack=GET_STACK(context);
	if(stack->top>3)
	{
		uint8_t opcodes[]={
			FKL_OP_SET_TP,
			FKL_OP_RES_TP,
			FKL_OP_POP_TP,
		};
		FklByteCode setTp={1,&opcodes[0]};
		FklByteCode resTp={1,&opcodes[1]};
		FklByteCode popTp={1,&opcodes[2]};
		FklByteCodelnt* r=fklCreateByteCodelnt(fklCreateByteCode(0));
		size_t top=stack->top;
		for(size_t i=3;i<top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			if(cur->bc->size)
			{
				fklCodeLntCat(r,cur);
				bclBcAppendToBcl(r,&resTp,fid,line);
			}
			fklDestroyByteCodelnt(cur);
		}
		bcBclAppendToBcl(&setTp,r,fid,line);
		bclBcAppendToBcl(r,&popTp,fid,line);
		libBc=r;
	}
	else
		libBc=createBclnt(fklCreateByteCode(0),fid,line);
	fklPushPtrStack(fklCreateCodegenLib(codegen->realpath
				,libBc
				,codegen->exportNum
				,codegen->exports
				,codegen->globalEnv->macros->head),codegen->loadedLibStack);
	FklCodegenEnv* exportTargetEnv=stack->base[0];
	FklString* prefix=stack->base[1];
	for(FklCodegenMacro* cur=codegen->globalEnv->macros->head;cur;cur=cur->next)
		add_compiler_macro(exportTargetEnv->macros
				,fklMakeNastNodeRef(createPatternWithPrefixFromOrig(cur->pattern,prefix))
				,fklCopyByteCodelnt(cur->bcl));
	fklDestroyCodegenEnv(exportTargetEnv);
	codegen->globalEnv->macros->head=NULL;
	FklByteCodelnt* retval=stack->base[2];
	stack->top=0;
	fklSetU64ToByteCode(retval->bc->code+sizeof(char),codegen->loadedLibStack->top);
	codegen->exportNum=0;
	codegen->exports=NULL;
	codegen->realpath=NULL;
	return retval;
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

static void _import_macro_stack_context_finalizer(void* data)
{
	FklPtrStack* s=data;
	while(s->top>1)
		fklDestroyByteCodelnt(fklPopPtrStack(s));
	fklDestroyCodegenEnv(fklPopPtrStack(s));
	fklDestroyPtrStack(s);
}

static const FklCodegenQuestContextMethodTable ImportMacroStackContextMethodTable=
{
	.__get_bcl_stack=_default_stack_context_get_bcl_stack,
	.__put_bcl=_default_stack_context_put_bcl,
	.__finalizer=_import_macro_stack_context_finalizer,
};

static CODEGEN_FUNC(codegen_import)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	if(name->type==FKL_NAST_NIL||!fklIsNastNodeListAndHasSameType(name,FKL_NAST_SYM))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklNastNode* importLibraryName=NULL;
	char* filename=combineFileNameFromListAndGetLastNode(name,&importLibraryName);
	filename=fklStrCat(filename,".fkl");
	if(!fklIsAccessableScriptFile(filename))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_IMPORTFAILED;
		errorState->place=fklMakeNastNodeRef(name);
		free(filename);
		return;
	}
	FklCodegenEnv* globalEnv=curEnv;
	while(globalEnv->prev)globalEnv=globalEnv->prev;
	FklCodegenEnv* libEnv=fklCreateCodegenEnv(globalEnv);
	FklCodegen* nextCodegen=createCodegen(codegen,filename,libEnv);
	if(hasLoadSameFile(nextCodegen->realpath,codegen))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_CIRCULARLOAD;
		errorState->place=fklMakeNastNodeRef(name);
		nextCodegen->refcount=1;
		fklDestroyCodegener(nextCodegen);
		free(filename);
		return;
	}
	size_t libId=check_loaded_lib(nextCodegen->realpath,codegen->loadedLibStack);
	if(!libId)
	{
		FILE* fp=fopen(filename,"r");
		free(filename);
		if(!fp)
		{
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			nextCodegen->refcount=1;
			fklDestroyCodegener(nextCodegen);
			return;
		}
		int unexpectEOF=0;
		int hasError=0;
		char* prev=NULL;
		size_t prevSize=0;
		size_t errorLine=0;
		FklPtrStack* tokenStack=fklCreatePtrStack(32,16);
		FklNastNode* libraryExpression=NULL;
		FklHashTable* patternMatchTable=fklCreatePatternMatchingHashTable();
		for(;;)
		{
			libraryExpression=getExpressionFromFile(nextCodegen
					,fp
					,&prev
					,&prevSize
					,&unexpectEOF
					,tokenStack
					,&errorLine
					,&hasError);
			if(!libraryExpression)
				break;
			if(unexpectEOF)
			{
				errorState->line=nextCodegen->curline;
				errorState->fid=nextCodegen->fid;
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
				nextCodegen->refcount=1;
				fklDestroyCodegener(nextCodegen);
				fclose(fp);
				fklDestroyPtrStack(tokenStack);
				return;
			}
			if(hasError)
			{
				errorState->fid=nextCodegen->fid;
				errorState->type=FKL_ERR_INVALIDEXPR;
				errorState->line=errorLine;
				errorState->place=NULL;
				nextCodegen->refcount=1;
				fklDestroyCodegener(nextCodegen);
				fclose(fp);
				fklDestroyPtrStack(tokenStack);
				return;
			}
			if(fklIsNastNodeList(libraryExpression)
					&&fklPatternMatch(builtInSubPattern[SUB_PATTERN_LIBRARY].pn
						,libraryExpression
						,patternMatchTable))
			{
				FklNastNode* libraryName=fklPatternMatchingHashTableRef(builtInPatternVar_name
						,patternMatchTable);
				FklNastNode* export=fklPatternMatchingHashTableRef(builtInPatternVar_args
						,patternMatchTable);
				if(libraryName->type!=FKL_NAST_SYM)
				{
					errorState->fid=nextCodegen->fid;
					errorState->type=FKL_ERR_SYNTAXERROR;
					errorState->place=fklMakeNastNodeRef(libraryExpression);
					nextCodegen->refcount=1;
					fklDestroyCodegener(nextCodegen);
					fclose(fp);
					fklDestroyNastNode(libraryExpression);
					return;
				}
				if(libraryName->u.sym==importLibraryName->u.sym)
				{
					if(!fklIsNastNodeListAndHasSameType(export,FKL_NAST_SYM))
					{
						errorState->fid=nextCodegen->fid;
						errorState->type=FKL_ERR_SYNTAXERROR;
						errorState->place=fklMakeNastNodeRef(libraryExpression);
						nextCodegen->refcount=1;
						fklDestroyCodegener(nextCodegen);
						fclose(fp);
						fklDestroyNastNode(libraryExpression);
						fklDestroyPtrStack(tokenStack);
						return;
					}
					FklUintStack* idStack=fklCreateUintStack(32,16);
					add_symbol_to_locale_env_in_list(export,curEnv,codegen->globalSymTable,idStack);
					nextCodegen->exportNum=idStack->top;
					nextCodegen->exports=fklCopyMemory(idStack->base,sizeof(FklSid_t)*idStack->top);
					fklDestroyUintStack(idStack);
					FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,patternMatchTable);
					FklPtrQueue* libraryRestExpressionQueue=fklCreatePtrQueue();
					pushListItemToQueue(rest,libraryRestExpressionQueue,NULL);
					FklPtrStack* bcStack=fklCreatePtrStack(32,16);
					FklByteCodelnt* importBc=createBclnt(create9lenBc(FKL_OP_IMPORT,0),codegen->fid,origExp->curline);
					fklPushPtrStack(curEnv,bcStack);
					curEnv->refcount+=1;
					fklPushPtrStack(importBc,bcStack);
					FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_library_bc_process
							,createCodegenQuestContext(bcStack,&ImportMacroStackContextMethodTable)
							,createDefaultQueueNextExpression(libraryRestExpressionQueue)
							,nextCodegen->globalEnv
							,rest->curline
							,nextCodegen
							,codegenQuestStack);
					fklDestroyNastNode(libraryExpression);
					break;
				}
			}
			else
				fklDestroyNastNode(libraryExpression);
		}
		fclose(fp);
		if(prev)
			free(prev);
		fklDestroyHashTable(patternMatchTable);
		fklDestroyPtrStack(tokenStack);
		if(!libraryExpression)
		{
			errorState->fid=nextCodegen->fid;
			errorState->type=FKL_ERR_LIBUNDEFINED;
			errorState->place=fklMakeNastNodeRef(name);
			nextCodegen->refcount=1;
			fklDestroyCodegener(nextCodegen);
			return;
		}
	}
	else
	{
		free(filename);
		FklCodegenLib* lib=codegen->loadedLibStack->base[libId-1];
		for(FklCodegenMacro* cur=lib->head;cur;cur=cur->next)
			add_compiler_macro(curEnv->macros,fklMakeNastNodeRef(cur->pattern),fklCopyByteCodelnt(cur->bcl));
		add_symbol_to_locale_env_in_array(curEnv,codegen->globalSymTable,lib->exportNum,lib->exports);
		FklByteCodelnt* importBc=createBclnt(create9lenBc(FKL_OP_IMPORT,libId),codegen->fid,origExp->curline);
		FklPtrStack* bcStack=fklCreatePtrStack(1,1);
		fklPushPtrStack(importBc,bcStack);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process
				,createDefaultStackContext(bcStack)
				,NULL
				,nextCodegen->globalEnv
				,origExp->curline
				,nextCodegen
				,codegenQuestStack);
	}
}

static void _import_macro_with_prefix_stack_context_finalizer(void* data)
{
	FklPtrStack* s=data;
	while(s->top>2)
		fklDestroyByteCodelnt(fklPopPtrStack(s));
	fklPopPtrStack(s);
	fklDestroyCodegenEnv(fklPopPtrStack(s));
	fklDestroyPtrStack(s);
}

static const FklCodegenQuestContextMethodTable ImportMacroWithPrefixStackContextMethodTable=
{
	.__get_bcl_stack=_default_stack_context_get_bcl_stack,
	.__put_bcl=_default_stack_context_put_bcl,
	.__finalizer=_import_macro_with_prefix_stack_context_finalizer,
};

static CODEGEN_FUNC(codegen_import_with_prefix)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* prefixNode=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	if(prefixNode->type!=FKL_NAST_SYM)
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	if(name->type==FKL_NAST_NIL||!fklIsNastNodeListAndHasSameType(name,FKL_NAST_SYM))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklNastNode* importLibraryName=NULL;
	char* filename=combineFileNameFromListAndGetLastNode(name,&importLibraryName);
	filename=fklStrCat(filename,".fkl");
	if(!fklIsAccessableScriptFile(filename))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_IMPORTFAILED;
		errorState->place=fklMakeNastNodeRef(name);
		free(filename);
		return;
	}
	FklCodegenEnv* globalEnv=curEnv;
	while(globalEnv->prev)globalEnv=globalEnv->prev;
	FklCodegenEnv* libEnv=fklCreateCodegenEnv(globalEnv);
	FklCodegen* nextCodegen=createCodegen(codegen,filename,libEnv);
	if(hasLoadSameFile(nextCodegen->realpath,codegen))
	{
		errorState->fid=codegen->fid;
		errorState->type=FKL_ERR_CIRCULARLOAD;
		errorState->place=fklMakeNastNodeRef(name);
		nextCodegen->refcount=1;
		fklDestroyCodegener(nextCodegen);
		free(filename);
		return;
	}
	size_t libId=check_loaded_lib(nextCodegen->realpath,codegen->loadedLibStack);
	if(!libId)
	{
		FILE* fp=fopen(filename,"r");
		free(filename);
		if(!fp)
		{
			errorState->fid=codegen->fid;
			errorState->type=FKL_ERR_FILEFAILURE;
			errorState->place=fklMakeNastNodeRef(name);
			nextCodegen->refcount=1;
			fklDestroyCodegener(nextCodegen);
			return;
		}
		int unexpectEOF=0;
		int hasError=0;
		char* prev=NULL;
		size_t prevSize=0;
		size_t errorLine=0;
		FklPtrStack* tokenStack=fklCreatePtrStack(32,16);
		FklNastNode* libraryExpression=NULL;
		FklHashTable* patternMatchTable=fklCreatePatternMatchingHashTable();
		for(;;)
		{
			libraryExpression=getExpressionFromFile(nextCodegen
					,fp
					,&prev
					,&prevSize
					,&unexpectEOF
					,tokenStack
					,&errorLine
					,&hasError);
			if(!libraryExpression)
				break;
			if(unexpectEOF)
			{
				errorState->line=nextCodegen->curline;
				errorState->fid=nextCodegen->fid;
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
				nextCodegen->refcount=1;
				fklDestroyCodegener(nextCodegen);
				fclose(fp);
				fklDestroyPtrStack(tokenStack);
				return;
			}
			if(hasError)
			{
				errorState->fid=nextCodegen->fid;
				errorState->type=FKL_ERR_INVALIDEXPR;
				errorState->line=errorLine;
				errorState->place=NULL;
				nextCodegen->refcount=1;
				fklDestroyCodegener(nextCodegen);
				fclose(fp);
				fklDestroyPtrStack(tokenStack);
				return;
			}
			if(fklIsNastNodeList(libraryExpression)
					&&fklPatternMatch(builtInSubPattern[SUB_PATTERN_LIBRARY].pn
						,libraryExpression
						,patternMatchTable))
			{
				FklNastNode* libraryName=fklPatternMatchingHashTableRef(builtInPatternVar_name
						,patternMatchTable);
				FklNastNode* export=fklPatternMatchingHashTableRef(builtInPatternVar_args
						,patternMatchTable);
				if(libraryName->type!=FKL_NAST_SYM)
				{
					errorState->fid=nextCodegen->fid;
					errorState->type=FKL_ERR_SYNTAXERROR;
					errorState->place=fklMakeNastNodeRef(libraryExpression);
					nextCodegen->refcount=1;
					fklDestroyCodegener(nextCodegen);
					fclose(fp);
					fklDestroyNastNode(libraryExpression);
					return;
				}
				if(libraryName->u.sym==importLibraryName->u.sym)
				{
					if(!fklIsNastNodeListAndHasSameType(export,FKL_NAST_SYM))
					{
						errorState->fid=nextCodegen->fid;
						errorState->type=FKL_ERR_SYNTAXERROR;
						errorState->place=fklMakeNastNodeRef(libraryExpression);
						nextCodegen->refcount=1;
						fklDestroyCodegener(nextCodegen);
						fclose(fp);
						fklDestroyNastNode(libraryExpression);
						fklDestroyPtrStack(tokenStack);
						return;
					}
					FklUintStack* idStack=fklCreateUintStack(32,16);
					FklUintStack* idWithPrefixStack=fklCreateUintStack(32,16);
					add_symbol_with_prefix_to_locale_env_in_list(export
							,prefixNode->u.sym
							,curEnv
							,codegen->globalSymTable
							,idStack
							,idWithPrefixStack);
					nextCodegen->exportNum=idStack->top;
					nextCodegen->exports=fklCopyMemory(idStack->base,sizeof(FklSid_t)*idStack->top);
					fklDestroyUintStack(idStack);
					FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,patternMatchTable);
					FklPtrQueue* libraryRestExpressionQueue=fklCreatePtrQueue();
					pushListItemToQueue(rest,libraryRestExpressionQueue,NULL);
					FklPtrStack* bcStack=fklCreatePtrStack(32,16);
					FklByteCode* bc=fklCreateByteCode(sizeof(char)+sizeof(uint64_t)*2+sizeof(FklSid_t)*idWithPrefixStack->top);
					bc->code[0]=FKL_OP_IMPORT_WITH_SYMBOLS;
					fklSetU64ToByteCode(bc->code+sizeof(char),0);
					fklSetU64ToByteCode(bc->code+sizeof(char)+sizeof(uint64_t),idWithPrefixStack->top);
					for(size_t i=0;i<idWithPrefixStack->top;i++)
					{
						FklSid_t id=idWithPrefixStack->base[i];
						fklSetSidToByteCode(bc->code
								+sizeof(char)
								+sizeof(uint64_t)*2
								+i*sizeof(FklSid_t)
								,id);
					}
					fklDestroyUintStack(idWithPrefixStack);
					FklByteCodelnt* importBc=createBclnt(bc,codegen->fid,origExp->curline);
					fklPushPtrStack(curEnv,bcStack);
					curEnv->refcount+=1;
					fklPushPtrStack(fklGetGlobSymbolWithId(prefixNode->u.sym)->symbol,bcStack);
					fklPushPtrStack(importBc,bcStack);
					FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_library_be_imported_with_prefix_bc_process
							,createCodegenQuestContext(bcStack,&ImportMacroWithPrefixStackContextMethodTable)
							,createDefaultQueueNextExpression(libraryRestExpressionQueue)
							,nextCodegen->globalEnv
							,rest->curline
							,nextCodegen
							,codegenQuestStack);
					fklDestroyNastNode(libraryExpression);
					break;
				}
			}
			else
				fklDestroyNastNode(libraryExpression);
		}
		if(prev)
			free(prev);
		fclose(fp);
		fklDestroyHashTable(patternMatchTable);
		fklDestroyPtrStack(tokenStack);
		if(!libraryExpression)
		{
			errorState->fid=nextCodegen->fid;
			errorState->type=FKL_ERR_LIBUNDEFINED;
			errorState->place=fklMakeNastNodeRef(name);
			nextCodegen->refcount=1;
			fklDestroyCodegener(nextCodegen);
			return;
		}
	}
	else
	{
		free(filename);
		FklCodegenLib* lib=codegen->loadedLibStack->base[libId-1];
		FklString* prefix=fklGetGlobSymbolWithId(prefixNode->u.sym)->symbol;
		for(FklCodegenMacro* cur=lib->head;cur;cur=cur->next)
			add_compiler_macro(curEnv->macros
					,fklMakeNastNodeRef(createPatternWithPrefixFromOrig(cur->pattern,prefix))
					,fklCopyByteCodelnt(cur->bcl));
		FklSid_t* exportsWithPrefix=(FklSid_t*)malloc(sizeof(FklSid_t)*lib->exportNum);
		FKL_ASSERT(exportsWithPrefix);
		add_symbol_with_prefix_to_locale_env_in_array(curEnv
				,codegen->globalSymTable
				,prefixNode->u.sym
				,lib->exportNum
				,lib->exports
				,exportsWithPrefix);
		FklByteCode* bc=fklCreateByteCode(sizeof(char)+sizeof(uint64_t)*2+sizeof(FklSid_t)*lib->exportNum);
		bc->code[0]=FKL_OP_IMPORT_WITH_SYMBOLS;
		fklSetU64ToByteCode(bc->code+sizeof(char),libId);
		fklSetU64ToByteCode(bc->code+sizeof(char)+sizeof(uint64_t),lib->exportNum);
		memcpy(bc->code+sizeof(char)+sizeof(uint64_t)*2,exportsWithPrefix,sizeof(FklSid_t)*lib->exportNum);
		free(exportsWithPrefix);
		FklByteCodelnt* importBc=createBclnt(bc,codegen->fid,origExp->curline);
		FklPtrStack* bcStack=fklCreatePtrStack(1,1);
		fklPushPtrStack(importBc,bcStack);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process
				,createDefaultStackContext(bcStack)
				,NULL
				,nextCodegen->globalEnv
				,origExp->curline
				,nextCodegen
				,codegenQuestStack);
	}
}

static CODEGEN_FUNC(codegen_module)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	if(name->type!=FKL_NAST_SYM)
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklNastNode* exportExpression=fklPatternMatchingHashTableRef(builtInPatternVar_args,ht);
	if(!fklIsNastNodeListAndHasSameType(exportExpression,FKL_NAST_SYM))
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
	FklNastNode* rest=fklPatternMatchingHashTableRef(builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklCreatePtrQueue();
	pushListItemToQueue(rest,queue,NULL);
	FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_begin_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createDefaultQueueNextExpression(queue)
			,curEnv
			,rest->curline
			,codegen
			,codegenQuestStack);
}

BC_PROCESS(_compiler_macro_bc_process)
{
	FklPtrStack* stack=GET_STACK(context);
	FklByteCodelnt* macroBcl=fklPopPtrStack(stack);
	FklNastNode* pattern=fklPopPtrStack(stack);
	FklCodegenMacroScope* macros=fklPopPtrStack(stack);
	add_compiler_macro(macros,pattern,macroBcl);
	return fklCreateByteCodelnt(fklCreateByteCode(0));
}

static void _macro_stack_context_finalizer(void* data)
{
	FklPtrStack* s=data;
	while(s->top>2)
		fklDestroyByteCodelnt(fklPopPtrStack(s));
	fklPopPtrStack(s);
	fklDestroyNastNode(fklPopPtrStack(s));
	fklDestroyPtrStack(s);
}

static const FklCodegenQuestContextMethodTable MacroStackContextMethodTable=
{
	.__get_bcl_stack=_default_stack_context_get_bcl_stack,
	.__put_bcl=_default_stack_context_put_bcl,
	.__finalizer=_macro_stack_context_finalizer,
};

static CODEGEN_FUNC(codegen_defmacro)
{
	FklNastNode* name=fklPatternMatchingHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* value=fklPatternMatchingHashTableRef(builtInPatternVar_value,ht);
	if(name->type==FKL_NAST_SYM)
		fklAddReplacementBySid(name->u.sym,value,curEnv);
	else if(name->type==FKL_NAST_PAIR)
	{
		if(!fklIsValidSyntaxPattern(name))
		{
			errorState->type=FKL_ERR_INVALID_MACRO_PATTERN;
			errorState->place=fklMakeNastNodeRef(name);
			return;
		}
		FklHashTable* psht=fklCreatePatternMatchingHashTable();
		fklPatternMatch(name,name,psht);
		FklCodegenEnv* globalEnv=curEnv;
		while(globalEnv->prev)globalEnv=globalEnv->prev;
		FklCodegenEnv* macroEnv=fklCreateCodegenEnv(globalEnv);
		macroEnv->macros->prev=curEnv->macros;
		for(FklHashTableNodeList* list=psht->list;list;list=list->next)
		{
			FklPatternMatchingHashTableItem* item=list->node->item;
			fklAddCodegenDefBySid(item->id,macroEnv);
		}
		fklDestroyHashTable(psht);
		FklCodegen* macroCodegen=createCodegen(codegen
				,NULL
				,macroEnv);
		free(macroCodegen->curDir);
		macroCodegen->curDir=fklCopyCstr(codegen->curDir);
		macroCodegen->filename=fklCopyCstr(codegen->filename);
		macroCodegen->realpath=fklCopyCstr(codegen->realpath);

		macroCodegen->globalSymTable=fklGetGlobSymbolTable();
		macroCodegen->loadedLibStack=macroCodegen->macroLibStack;
		FklPtrStack* bcStack=fklCreatePtrStack(16,16);
		fklPushPtrStack(curEnv->macros,bcStack);
		fklPushPtrStack(fklMakeNastNodeRef(name),bcStack);
		FklPtrQueue* queue=fklCreatePtrQueue();
		fklPushPtrQueue(fklMakeNastNodeRef(value),queue);
		FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_compiler_macro_bc_process
				,createCodegenQuestContext(bcStack,&MacroStackContextMethodTable)
				,createDefaultQueueNextExpression(queue)
				,macroEnv
				,value->curline
				,macroCodegen
				,codegenQuestStack);
	}
	else if(name->type==FKL_NAST_VECTOR)
	{
#pragma message "Todo:defmacro for reader macro"
	}
	else
	{
		errorState->type=FKL_ERR_SYNTAXERROR;
		errorState->place=fklMakeNastNodeRef(origExp);
		return;
	}
}

typedef void (*FklCodegenFunc)(CODEGEN_ARGS);

#undef BC_PROCESS
#undef GET_STACK
#undef CODEGEN_ARGS
#undef CODEGEN_FUNC

FklByteCode* fklCodegenNode(const FklNastNode* node,FklCodegen* codegenr)
{
	FklPtrStack* stack=fklCreatePtrStack(32,16);
	fklPushPtrStack((void*)node,stack);
	FklByteCode* retval=fklCreateByteCode(0);
	while(!fklIsPtrStackEmpty(stack))
	{
		FklNastNode* node=fklPopPtrStack(stack);
		FklByteCode* tmp=NULL;
		switch(node->type)
		{
			case FKL_NAST_SYM:
				tmp=fklCreatePushSidByteCode(fklAddSymbol(fklGetGlobSymbolWithId(node->u.sym)->symbol,codegenr->globalSymTable)->id);
				break;
			case FKL_NAST_NIL:
				tmp=create1lenBc(FKL_OP_PUSH_NIL);
				break;
			case FKL_NAST_CHR:
				tmp=fklCreatePushCharByteCode(node->u.chr);
				break;
			case FKL_NAST_I32:
				tmp=fklCreatePushI32ByteCode(node->u.i32);
				break;
			case FKL_NAST_I64:
				tmp=fklCreatePushI64ByteCode(node->u.i64);
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
						fklPushPtrStack(cur->u.pair->car,stack);
						i++;
					}
					fklPushPtrStack(cur,stack);
					if(i==2)
						tmp=create1lenBc(FKL_OP_PUSH_PAIR);
					else
						tmp=create9lenBc(FKL_OP_PUSH_LIST,i);
				}
				break;
			case FKL_NAST_BOX:
				tmp=create1lenBc(FKL_OP_PUSH_BOX);
				fklPushPtrStack(node->u.box,stack);
				break;
			case FKL_NAST_VECTOR:
				tmp=create9lenBc(FKL_OP_PUSH_VECTOR,node->u.vec->size);
				for(size_t i=0;i<node->u.vec->size;i++)
					fklPushPtrStack(node->u.vec->base[i],stack);
				break;
			case FKL_NAST_HASHTABLE:
				tmp=create9lenBc(FKL_OP_PUSH_HASHTABLE_EQ+node->u.hash->type,node->u.hash->num);
				for(size_t i=0;i<node->u.hash->num;i++)
				{
					fklPushPtrStack(node->u.hash->items[i].car,stack);
					fklPushPtrStack(node->u.hash->items[i].cdr,stack);
				}
				break;
			default:
				FKL_ASSERT(0);
				break;
		}
		fklReCodeCat(tmp,retval);
		fklDestroyByteCode(tmp);
	}
	fklDestroyPtrStack(stack);
	return retval;
}

static int matchAndCall(FklCodegenFunc func
		,const FklNastNode* pattern
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
		func(exp,ht,codegenQuestStack,env,codegenr,errorState);
	}
	fklDestroyHashTable(ht);
	return r;
}

static FklNastNode* _nil_replacement(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen)
{
	return fklMakeNastNodeRef(fklCreateNastNode(FKL_NAST_NIL,orig->curline));
}

static FklNastNode* _file_dir_replacement(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen)
{
	FklString* s=NULL;
	FklNastNode* r=fklCreateNastNode(FKL_NAST_STR,orig->curline);
	if(codegen->filename==NULL)
		s=fklCreateStringFromCstr(fklGetCwd());
	else
		s=fklCreateStringFromCstr(codegen->curDir);
	r->u.str=s;
	return fklMakeNastNodeRef(r);
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
	return fklMakeNastNodeRef(r);
}

static FklNastNode* _line_replacement(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen)
{
	uint64_t line=orig->curline;
	FklNastNode* r=NULL;
	if(line<INT32_MAX)
	{
		r=fklCreateNastNode(FKL_NAST_I32,orig->curline);
		r->u.i32=line;
	}
	else
	{
		r=fklCreateNastNode(FKL_NAST_I64,orig->curline);
		r->u.i64=line;
	}
	return fklMakeNastNodeRef(r);
}

typedef FklNastNode* (*RelpacementFunc)(const FklNastNode* orig,FklCodegenEnv* env,FklCodegen* codegen);
static struct SymbolReplacement
{
	const char* s;
	FklSid_t sid;
	RelpacementFunc func;
}builtInSymbolReplacement[]=
{
	{"nil",        0, _nil_replacement,      },
	{"*line*",     0, _line_replacement,     },
	{"*file*",     0, _file_replacement,     },
	{"*file-dir*", 0, _file_dir_replacement, },
	{NULL,         0, NULL,                  },
};

static struct PatternAndFunc
{
	const char* ps;
	FklNastNode* pn;
	FklCodegenFunc func;
}builtInPattern[]=
{
	{"(begin,rest)",            NULL, codegen_begin,              },
	{"(define name value)",     NULL, codegen_define,             },
	{"(setq name value)",       NULL, codegen_setq,               },
	{"(quote value)",           NULL, codegen_quote,              },
	{"(unquote value)",         NULL, codegen_unquote,            },
	{"(qsquote value)",         NULL, codegen_qsquote,            },
	{"(lambda args,rest)",      NULL, codegen_lambda,             },
	{"(and,rest)",              NULL, codegen_and,                },
	{"(or,rest)",               NULL, codegen_or,                 },
	{"(cond,rest)",             NULL, codegen_cond,               },
	{"(load name)",             NULL, codegen_load,               },
	{"(import name)",           NULL, codegen_import,             },
	{"(import name rest)",      NULL, codegen_import_with_prefix, },
	{"(module name args,rest)", NULL, codegen_module,             },
	{"(defmacro name value)",   NULL, codegen_defmacro,           },
	{"(macroexpand value)",     NULL, codegen_macroexpand,        },
	{NULL,                      NULL, NULL,                       },
};

const FklSid_t* fklInitCodegen(void)
{
	static const char* builtInHeadSymbolTableCstr[4]=
	{
		"quote",
		"qsquote",
		"unquote",
		"unqtesp",
	};
	for(int i=0;i<4;i++)
		builtInHeadSymbolTable[i]=fklAddSymbolToGlobCstr(builtInHeadSymbolTableCstr[i])->id;
	builtInPatternVar_rest=fklAddSymbolToGlobCstr("rest")->id;
	builtInPatternVar_name=fklAddSymbolToGlobCstr("name")->id;
	builtInPatternVar_args=fklAddSymbolToGlobCstr("args")->id;
	builtInPatternVar_value=fklAddSymbolToGlobCstr("value")->id;

	FklStringMatchPattern* builtinStringPatterns=fklInitBuiltInStringPattern();
	for(struct PatternAndFunc* cur=&builtInPattern[0];cur->ps!=NULL;cur++)
		cur->pn=fklCreateNastNodeFromCstr(cur->ps
				,builtInHeadSymbolTable
				,builtinStringPatterns);

	for(struct SymbolReplacement* cur=&builtInSymbolReplacement[0];cur->s!=NULL;cur++)
		cur->sid=fklAddSymbolToGlobCstr(cur->s)->id;

	for(struct SubPattern* cur=&builtInSubPattern[0];cur->ps!=NULL;cur++)
		cur->pn=fklCreateNastNodeFromCstr(cur->ps
				,builtInHeadSymbolTable
				,builtinStringPatterns);

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

FklByteCodelnt* fklMakePushVar(const FklNastNode* exp,FklCodegen* codegen)
{
	FklSid_t sid=fklAddSymbol(fklGetGlobSymbolWithId(exp->u.sym)->symbol,codegen->globalSymTable)->id;
	FklByteCodelnt* retval=createBclnt(createPushVar(sid),codegen->fid,exp->curline);
	return retval;
}

static inline int mapAllBuiltInPattern(FklNastNode* curExp
		,FklPtrStack* codegenQuestStack
		,FklCodegenEnv* curEnv
		,FklCodegen* codegenr
		,FklCodegenErrorState* errorState)
{
	if(fklIsNastNodeList(curExp))
		for(struct PatternAndFunc* cur=&builtInPattern[0];cur->ps!=NULL;cur++)
			if(matchAndCall(cur->func,cur->pn,curExp,codegenQuestStack,curEnv,codegenr,errorState))
				return 0;
	if(curExp->type==FKL_NAST_PAIR)
	{
		codegen_funcall(curExp,codegenQuestStack,curEnv,codegenr,errorState);
		return 0;
	}
	return 1;
}

static void printCodegenError(FklNastNode* obj,FklBuiltInErrorType type,FklSid_t sid,FklSymbolTable* symbolTable,size_t line)
{
	if(type==FKL_ERR_DUMMY)
		return;
	fprintf(stderr,"error of compiling: ");
	switch(type)
	{
		case FKL_ERR_CIR_REF:
			fprintf(stderr,"Circular reference occur in expanding macro");
			break;
		case FKL_ERR_SYMUNDEFINE:
			fprintf(stderr,"Symbol ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr);
			fprintf(stderr," is undefined");
			break;
		case FKL_ERR_SYNTAXERROR:
			fprintf(stderr,"Invalid syntax ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr);
			break;
		case FKL_ERR_INVALIDEXPR:
			fprintf(stderr,"Invalid expression ");
			if(obj!=NULL)
				fklPrintNastNode(obj,stderr);
			break;
		case FKL_ERR_CIRCULARLOAD:
			fprintf(stderr,"Circular load file ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr);
			break;
		case FKL_ERR_INVALIDPATTERN:
			fprintf(stderr,"Invalid string match pattern ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr);
			break;
		case FKL_ERR_INVALID_MACRO_PATTERN:
			fprintf(stderr,"Invalid macro pattern ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr);
			break;
		case FKL_ERR_MACROEXPANDFAILED:
			fprintf(stderr,"Failed to expand macro in ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr);
			break;
		case FKL_ERR_LIBUNDEFINED:
			fprintf(stderr,"Library ");
			if(obj!=NULL)fklPrintNastNode(obj,stderr);
			fprintf(stderr," undefined");
			break;
		case FKL_ERR_FILEFAILURE:
			fprintf(stderr,"Failed for file:");
			fklPrintNastNode(obj,stderr);
			break;
		case FKL_ERR_IMPORTFAILED:
			fprintf(stderr,"Failed for importing module:");
			fklPrintNastNode(obj,stderr);
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

static RelpacementFunc findBuiltInReplacementWithId(FklSid_t id)
{
	for(struct SymbolReplacement* cur=&builtInSymbolReplacement[0];cur->s!=NULL;cur++)
	{
		if(cur->sid==id)
			return cur->func;
	}
	return NULL;
}

FklByteCodelnt* fklGenExpressionCodeWithQuest(FklCodegenQuest* initialQuest,FklCodegen* codegener)
{
	FklPtrStack* resultStack=fklCreatePtrStack(1,8);
	FklCodegenErrorState errorState={0,0,NULL};
	FklPtrStack* codegenQuestStack=fklCreatePtrStack(32,16);
	fklPushPtrStack(initialQuest,codegenQuestStack);
	while(!fklIsPtrStackEmpty(codegenQuestStack))
	{
		FklCodegenQuest* curCodegenQuest=fklTopPtrStack(codegenQuestStack);
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
					curExp=fklTryExpandCodegenMacro(curExp,curCodegen,curEnv->macros,&errorState);
					if(errorState.type)
						break;
				}
				if(curExp->type==FKL_NAST_SYM)
				{
					FklNastNode* replacement=NULL;
					for(FklCodegenEnv* env=curEnv;env->prev&&!replacement;env=env->prev)
						replacement=fklGetReplacement(curExp->u.sym,env);
					if(replacement)
					{
						fklDestroyNastNode(curExp);
						curExp=replacement;
					}
					else
					{
						RelpacementFunc f=findBuiltInReplacementWithId(curExp->u.sym);
						if(f)
						{
							FklNastNode* t=f(curExp,curEnv,curCodegen);
							fklDestroyNastNode(curExp);
							curExp=t;
						}
						else
						{
							curContext->t->__put_bcl(curContext->data,fklMakePushVar(curExp,curCodegen));
							fklDestroyNastNode(curExp);
							continue;
						}
					}
				}
				r=mapAllBuiltInPattern(curExp,codegenQuestStack,curEnv,curCodegen,&errorState);
				if(r)
				{
					curContext->t->__put_bcl(curContext->data
							,createBclnt(fklCodegenNode(curExp,curCodegen)
								,curCodegen->fid
								,curExp->curline));
				}
				fklDestroyNastNode(curExp);
				if(!r&&fklTopPtrStack(codegenQuestStack)!=curCodegenQuest)
					break;
			}
		}
		if(errorState.type)
		{
			printCodegenError(errorState.place,errorState.type,errorState.fid,codegener->globalSymTable,errorState.line);
			while(!fklIsPtrStackEmpty(codegenQuestStack))
			{
				FklCodegenQuest* curCodegenQuest=fklPopPtrStack(codegenQuestStack);
				destroyCodegenQuest(curCodegenQuest);
			}
			fklDestroyPtrStack(codegenQuestStack);
			fklDestroyPtrStack(resultStack);
			return NULL;
		}
		FklCodegenQuest* otherCodegenQuest=fklTopPtrStack(codegenQuestStack);
		if(otherCodegenQuest==curCodegenQuest)
		{
			fklPopPtrStack(codegenQuestStack);
			FklCodegenQuest* prevQuest=curCodegenQuest->prev?curCodegenQuest->prev:fklTopPtrStack(codegenQuestStack);
			FklByteCodelnt* resultBcl=curCodegenQuest->processer(curCodegenQuest->codegen
					,curContext
					,curCodegenQuest->codegen->fid
					,curCodegenQuest->curline);
			if(fklIsPtrStackEmpty(codegenQuestStack))
				fklPushPtrStack(resultBcl,resultStack);
			else
			{
				FklCodegenQuestContext* prevContext=prevQuest->context;
				prevContext->t->__put_bcl(prevContext->data,resultBcl);
			}
			destroyCodegenQuest(curCodegenQuest);
		}
	}
	FklByteCodelnt* retval=NULL;
	if(!fklIsPtrStackEmpty(resultStack))
		retval=fklPopPtrStack(resultStack);
	fklDestroyPtrStack(resultStack);
	fklDestroyPtrStack(codegenQuestStack);
	return retval;

}

FklByteCodelnt* fklGenExpressionCodeWithFp(FILE* fp
		,FklCodegen* codegen)
{
	FklCodegenEnv* mainEnv=fklCreateCodegenEnv(codegen->globalEnv);
	FklCodegenQuest* initialQuest=createCodegenQuest(_begin_exp_bc_process
			,createDefaultStackContext(fklCreatePtrStack(32,16))
			,createFpNextExpression(fp,codegen)
			,mainEnv
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
			,globalEnv
			,exp->curline
			,NULL
			,codegenr);
	return fklGenExpressionCodeWithQuest(initialQuest,codegenr);
}

void fklInitGlobalCodegener(FklCodegen* codegen
		,const char* rp
		,FklCodegenEnv* globalEnv
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
	codegen->curline=1;
	codegen->prev=NULL;
	if(globalEnv)
		codegen->globalEnv=globalEnv;
	else
	{
		codegen->globalEnv=fklCreateCodegenEnv(NULL);
		fklInitGlobCodegenEnv(codegen->globalEnv);
	}
	codegen->globalEnv->refcount+=1;
	codegen->destroyAbleMark=destroyAbleMark;
	codegen->refcount=0;
	codegen->exportNum=0;
	codegen->exports=NULL;
	codegen->patterns=fklInitBuiltInStringPattern();
	codegen->loadedLibStack=fklCreatePtrStack(8,8);
	codegen->macroLibStack=fklCreatePtrStack(8,8);
}

void fklInitCodegener(FklCodegen* codegen
		,const char* filename
		,FklCodegenEnv* env
		,FklCodegen* prev
		,FklSymbolTable* globalSymTable
		,int destroyAbleMark)
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
	codegen->destroyAbleMark=destroyAbleMark;
	codegen->refcount=0;
	codegen->exportNum=0;
	codegen->exports=NULL;
	codegen->patterns=prev?prev->patterns:fklInitBuiltInStringPattern();
	codegen->loadedLibStack=prev?prev->loadedLibStack:fklCreatePtrStack(8,8);
	codegen->macroLibStack=prev?prev->macroLibStack:fklCreatePtrStack(8,8);
}

void fklUninitCodegener(FklCodegen* codegen)
{
	fklDestroyCodegenEnv(codegen->globalEnv);
	if(!codegen->destroyAbleMark)
	{
		if(codegen->globalSymTable&&codegen->globalSymTable!=fklGetGlobSymbolTable())
			fklDestroySymbolTable(codegen->globalSymTable);
		while(!fklIsPtrStackEmpty(codegen->loadedLibStack))
			fklDestroyCodegenLib(fklPopPtrStack(codegen->loadedLibStack));
		fklDestroyAllStringPattern(codegen->patterns);
		fklDestroyPtrStack(codegen->loadedLibStack);
		FklPtrStack* macroLibStack=codegen->macroLibStack;
		while(!fklIsPtrStackEmpty(macroLibStack))
		{
			FklCodegenLib* cur=fklPopPtrStack(macroLibStack);
			fklDestroyByteCodelnt(cur->bcl);
			free(cur->exports);
			free(cur->rp);
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
}

typedef struct
{
	FklCodegenEnv* env;
	size_t cp;
	FklSid_t sid;
}MayUndefine;

static MayUndefine* createMayUndefine(FklCodegenEnv* env,size_t cp,FklSid_t sid)
{
	MayUndefine* r=(MayUndefine*)malloc(sizeof(MayUndefine));
	FKL_ASSERT(r);
	env->refcount+=1;
	r->env=env;
	r->cp=cp;
	r->sid=sid;
	return r;
}

static void destroyMayUndefine(MayUndefine* t)
{
	fklDestroyCodegenEnv(t->env);
	free(t);
}

void fklCodegenPrintUndefinedSymbol(FklByteCodelnt* code,FklCodegenLib** libs,FklSymbolTable* symbolTable,size_t exportNum,FklSid_t* exports)
{
	FklUintStack* cpcstack=fklCreateUintStack(32,16);
	FklUintStack* scpstack=fklCreateUintStack(32,16);
	FklPtrStack* envstack=fklCreatePtrStack(32,16);
	FklPtrStack* mayUndefined=fklCreatePtrStack(32,16);
	FklByteCode* bc=code->bc;
	fklPushUintStack(0,cpcstack);
	fklPushUintStack(bc->size,scpstack);
	FklCodegenEnv* globEnv=fklCreateCodegenEnv(NULL);
	FklCodegenEnv* mainEnv=fklCreateCodegenEnv(globEnv);
	globEnv->refcount=1;
	mainEnv->refcount=2;
	fklInitGlobCodegenEnvWithSymbolTable(globEnv,symbolTable);
	fklPushPtrStack(mainEnv,envstack);
	while((!fklIsUintStackEmpty(cpcstack))&&(!fklIsUintStackEmpty(scpstack)))
	{
		uint64_t i=fklPopUintStack(cpcstack);
		uint64_t end=i+fklPopUintStack(scpstack);
		FklCodegenEnv* curEnv=fklPopPtrStack(envstack);
		while(i<end)
		{
			FklOpcode opcode=(FklOpcode)(bc->code[i]);
			switch(fklGetOpcodeArgLen(opcode))
			{
				case -1:
					{
						switch(opcode)
						{
							case FKL_OP_PUSH_TRY:
								{
									i+=sizeof(FklSid_t)+sizeof(char);
									uint32_t handlerNum=fklGetU32FromByteCode(bc->code+i);
									i+=sizeof(uint32_t);
									int j=0;
									for(;j<handlerNum;j++)
									{
										uint32_t num=fklGetU32FromByteCode(bc->code+i);
										i+=sizeof(num);
										i+=sizeof(FklSid_t)*num;
										uint32_t pCpc=fklGetU64FromByteCode(bc->code+i);
										i+=sizeof(uint64_t);
										i+=pCpc;
									}
								}
								break;
							case FKL_OP_POP_VAR:
								{
									uint32_t scope=fklGetU32FromByteCode(bc->code+i+sizeof(char));
									FklSid_t id=fklGetSidFromByteCode(bc->code+i+sizeof(char)+sizeof(scope));
									if(scope)
									{
										FklCodegenEnv* env=curEnv;
										for(uint32_t i=1;i<scope;i++)
											env=curEnv->prev;
										fklAddCodegenDefBySid(id,env);
									}
									else
									{
										int r=0;
										for(FklCodegenEnv* e=curEnv;e;e=e->prev)
										{
											r=fklIsSymbolDefined(id,e);
											if(r)
												break;
										}
										if(!r)
											fklPushPtrStack(createMayUndefine(curEnv,i,id),mayUndefined);
									}
								}
								i+=sizeof(char)+sizeof(uint32_t)+sizeof(FklSid_t);
								break;
							case FKL_OP_PUSH_PROC:
								fklPushUintStack(i+sizeof(char)+sizeof(uint64_t),cpcstack);
								{
									fklPushUintStack(fklGetU64FromByteCode(bc->code+i+sizeof(char)),scpstack);
									FklCodegenEnv* nextEnv=fklCreateCodegenEnv(curEnv);
									nextEnv->refcount=1;
									fklPushPtrStack(nextEnv,envstack);
								}
								i+=sizeof(char)+sizeof(uint64_t)+fklGetU64FromByteCode(bc->code+i+sizeof(char));
								break;
							case FKL_OP_PUSH_STR:
							case FKL_OP_PUSH_BIG_INT:
							case FKL_OP_PUSH_BYTEVECTOR:
								i+=sizeof(char)
									+sizeof(uint64_t)
									+fklGetU64FromByteCode(bc->code+i+sizeof(char));
								break;
							case FKL_OP_IMPORT_WITH_SYMBOLS:
								{
									uint64_t exportsNum=fklGetU64FromByteCode(bc->code+i+sizeof(char)+sizeof(uint64_t));
									FklSid_t* exports=fklCopyMemory(bc->code+i+sizeof(char)+sizeof(uint64_t)*2
											,sizeof(FklSid_t)*exportsNum);
									for(size_t i=0;i<exportsNum;i++)
										fklAddCodegenDefBySid(exports[i],curEnv);
									free(exports);
									i+=sizeof(char)+sizeof(uint64_t)*2+exportsNum*sizeof(FklSid_t);
								}
								break;
							default:
								FKL_ASSERT(0);
								break;
						}
					}
					break;
				case 0:
					if(opcode==FKL_OP_PUSH_R_ENV)
					{
						curEnv=fklCreateCodegenEnv(curEnv);
						curEnv->refcount=1;
					}
					else if(opcode==FKL_OP_POP_R_ENV)
					{
						FklCodegenEnv* p=curEnv->prev;
						fklDestroyCodegenEnv(curEnv);
						curEnv=p;
					}
					i+=sizeof(char);
					break;
				case 1:
					i+=sizeof(char)+sizeof(char);
					break;
				case 4:
					i+=sizeof(char)+sizeof(int32_t);
					break;
				case 8:
					switch(opcode)
					{
						case FKL_OP_POP_ARG:
						case FKL_OP_POP_REST_ARG:
						case FKL_OP_PUSH_VAR:
							{
								FklSid_t id=fklGetSidFromByteCode(bc->code+i+sizeof(char));
								if(opcode==FKL_OP_POP_ARG||opcode==FKL_OP_POP_REST_ARG)
									fklAddCodegenDefBySid(id,curEnv);
								else if(opcode==FKL_OP_PUSH_VAR)
								{
									int r=0;
									for(FklCodegenEnv* e=curEnv;e;e=e->prev)
									{
										r=fklIsSymbolDefined(id,e);
										if(r)
											break;
									}
									if(!r)
										fklPushPtrStack(createMayUndefine(curEnv,i,id),mayUndefined);
								}
							}
							break;
						case FKL_OP_IMPORT:
							{
								uint64_t libCount=fklGetU64FromByteCode(bc->code+i+sizeof(char));
								FklCodegenLib* lib=libs[libCount-1];
								for(size_t i=0;i<lib->exportNum;i++)
									fklAddCodegenDefBySid(lib->exports[i],curEnv);
							}
							break;
						default:
							break;
					}
					i+=sizeof(char)+sizeof(int64_t);
					break;
			}
		}
		fklDestroyCodegenEnv(curEnv);
	}
	for(size_t i=0;i<exportNum;i++)
	{
		if(!fklIsSymbolDefined(exports[i],mainEnv))
			fklPushPtrStack(createMayUndefine(mainEnv,0,exports[i]),mayUndefined);
	}
	fklDestroyCodegenEnv(mainEnv);
	fklDestroyUintStack(cpcstack);
	fklDestroyUintStack(scpstack);
	fklDestroyPtrStack(envstack);
	for(uint32_t i=0;i<mayUndefined->top;i++)
	{
		MayUndefine* cur=mayUndefined->base[i];
		FklCodegenEnv* curEnv=cur->env;
		int r=0;
		for(FklCodegenEnv* e=curEnv;e;e=e->prev)
		{
			r=fklIsSymbolDefined(cur->sid,e);
			if(r)
				break;
		}
		if(!r)
		{
			FklLineNumTabNode* node=fklFindLineNumTabNode(cur->cp,code->ls,code->l);
			fprintf(stderr,"warning of compiling: Symbol \"");
			fklPrintString(fklGetSymbolWithId(cur->sid,symbolTable)->symbol,stderr);
			fprintf(stderr,"\" is undefined at line %d of ",node->line);
			fklPrintString(fklGetSymbolWithId(node->fid,symbolTable)->symbol,stderr);
			fputc('\n',stderr);
		}
		destroyMayUndefine(cur);
	}
	fklDestroyPtrStack(mayUndefined);
}

void fklInitCodegenLib(FklCodegenLib* lib
		,char* rp
		,FklByteCodelnt* bcl
		,size_t exportNum
		,FklSid_t* exports
		,FklCodegenMacro* head)
{
	lib->rp=rp;
	lib->bcl=bcl;
	lib->exportNum=exportNum;
	lib->exports=exports;
	lib->head=head;
}

FklCodegenLib* fklCreateCodegenLib(char* rp
		,FklByteCodelnt* bcl
		,size_t exportNum
		,FklSid_t* exports
		,FklCodegenMacro* head)
{
	FklCodegenLib* lib=(FklCodegenLib*)malloc(sizeof(FklCodegenLib));
	FKL_ASSERT(lib);
	fklInitCodegenLib(lib,rp,bcl,exportNum,exports,head);
	return lib;
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

void fklDestroyCodegenLib(FklCodegenLib* lib)
{
	free(lib->rp);
	fklDestroyByteCodelnt(lib->bcl);
	free(lib->exports);
	fklDestroyCodegenMacroList(lib->head);
	lib->exportNum=0;
	free(lib);
}

FklCodegenMacro* fklCreateCodegenMacro(FklNastNode* pattern,FklByteCodelnt* bcl,FklCodegenMacro* next)
{
	FklCodegenMacro* r=(FklCodegenMacro*)malloc(sizeof(FklCodegenMacro));
	FKL_ASSERT(r);
	r->pattern=fklMakeNastNodeRef(pattern);
	r->bcl=bcl;
	r->next=next;
	return r;
}

void fklDestroyCodegenMacro(FklCodegenMacro* macro)
{
	fklDestroyNastNode(macro->pattern);
	fklDestroyByteCodelnt(macro->bcl);
	free(macro);
}

FklCodegenMacroScope* fklCreateCodegenMacroScope(FklCodegenMacroScope* prev)
{
	FklCodegenMacroScope* r=(FklCodegenMacroScope*)malloc(sizeof(FklCodegenMacroScope));
	FKL_ASSERT(r);
	r->head=NULL;
	r->prev=prev;
	return r;
}

void fklDestroyCodegenMacroScope(FklCodegenMacroScope* macros)
{
	for(FklCodegenMacro* cur=macros->head;cur;)
	{
		FklCodegenMacro* t=cur;
		cur=cur->next;
		fklDestroyCodegenMacro(t);
	}
	free(macros);
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

#include<setjmp.h>
static jmp_buf buf;
static void errorCallBack(void* a)
{
	int* i=(int*)a;
	longjmp(buf,i[(sizeof(void*)*2)/sizeof(int)]);
}

static FklVMenv* createVMenvFromPatternMatchTable(FklVMvalue* prev
		,FklHashTable* ht
		,FklHashTable* lineHash
		,FklVMgc* gc)
{
	FklVMenv* env=fklCreateVMenv(prev,gc);
	for(FklHashTableNodeList* list=ht->list;list;list=list->next)
	{
		FklPatternMatchingHashTableItem* item=list->node->item;
		FklVMvalue* v=fklCreateVMvalueFromNastNodeNoGC(item->node,lineHash,gc);
		FklVMvalue* volatile* pv=fklFindOrAddVar(item->id,env);
		*pv=v;
	}
	return env;
}

static FklVM* initMacroExpandVM(FklByteCodelnt* bcl
		,FklHashTable* ht
		,FklHashTable* lineHash
		,FklCodegen* codegen)
{
	FklVM* anotherVM=fklCreateVM(fklCopyByteCodelnt(bcl),NULL,NULL);
	FklVMvalue* globEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,fklCreateGlobVMenv(FKL_VM_NIL,anotherVM->gc),anotherVM->gc);
	FklPtrStack* macroLibStack=codegen->macroLibStack;
	anotherVM->libNum=macroLibStack->top;;
	anotherVM->libs=(FklVMlib*)malloc(sizeof(FklVMlib)*macroLibStack->top);
	FKL_ASSERT(anotherVM->libs);
	for(size_t i=0;i<macroLibStack->top;i++)
	{
		FklCodegenLib* cur=macroLibStack->base[i];
		FklVMvalue* codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,fklCopyByteCodelnt(cur->bcl),anotherVM->gc);
		FklVMvalue* proc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,fklCreateVMproc(0,cur->bcl->bc->size,codeObj,anotherVM->gc),anotherVM->gc);
		fklSetRef(&proc->u.proc->prevEnv,globEnv,anotherVM->gc);
		fklInitVMlib(&anotherVM->libs[i]
				,cur->exportNum
				,fklCopyMemory(cur->exports,cur->exportNum*sizeof(FklSid_t))
				,proc);
	}
	FklVMvalue* mainEnv=fklCreateVMvalueNoGC(FKL_TYPE_ENV,createVMenvFromPatternMatchTable(globEnv,ht,lineHash,anotherVM->gc),anotherVM->gc);
	FklVMframe* mainframe=anotherVM->frames;
	mainframe->localenv=mainEnv;
	anotherVM->callback=errorCallBack;
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
	for(FklCodegenMacro* macro=findMacro(r,macros,&ht);macro;macro=findMacro(r,macros,&ht))
	{
		FklHashTable* lineHash=fklCreateLineNumHashTable();
		FklVM* anotherVM=initMacroExpandVM(macro->bcl,ht,lineHash,codegen);
		FklVMgc* gc=anotherVM->gc;
		if(!setjmp(buf))
		{
			fklRunVM(anotherVM);
			fklJoinAllThread(anotherVM);
			uint64_t curline=r->curline;
			fklDestroyNastNode(r);
			r=fklCreateNastNodeFromVMvalue(fklGetTopValue(anotherVM->stack),curline,lineHash);
			if(!r)
			{
				errorState->type=FKL_ERR_CIR_REF;
				errorState->place=NULL;
				errorState->fid=codegen->fid;
				errorState->line=curline;
			}
			fklDestroyHashTable(ht);
			fklDestroyHashTable(lineHash);
			fklDestroyVMgc(gc);
			fklDestroyAllVMs(anotherVM);
		}
		else
		{
			errorState->type=FKL_ERR_MACROEXPANDFAILED;
			errorState->place=r;
			errorState->fid=codegen->fid;
			errorState->line=curline;
			fklDestroyHashTable(ht);
			fklDestroyHashTable(lineHash);
			fklDeleteCallChain(anotherVM);
			fklCancelAllThread();
			fklJoinAllThread(anotherVM);
			fklDestroyAllVMs(anotherVM);
			fklDestroyVMgc(gc);
			r=NULL;
		}
	}
	return r;
}
