#include<fakeLisp/ncompiler.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/pattern.h>
#include<fakeLisp/vm.h>
#include<ctype.h>
#include<string.h>

static FklSid_t builtInPatternVar_rest=0;
static FklSid_t builtInPatternVar_name=0;
static FklSid_t builtInPatternVar_value=0;
static FklSid_t builtInPatternVar_args=0;
static FklSid_t builtInPattern_head_quote=0;
static FklSid_t builtInPattern_head_qsquote=0;
static FklSid_t builtInPattern_head_unquote=0;
static FklSid_t builtInPattern_head_unqtesp=0;

static int nastNodeEqual(const FklNastNode* n0,const FklNastNode* n1)
{
	FklPtrStack* s0=fklNewPtrStack(32,16);
	FklPtrStack* s1=fklNewPtrStack(32,16);
	fklPushPtrStack((void*)n0,s0);
	fklPushPtrStack((void*)n1,s1);
	int r=1;
	while(!fklIsPtrStackEmpty(s0)&&!fklIsPtrStackEmpty(s1)&&r)
	{
		FklNastNode* root0=fklPopPtrStack(s0);
		FklNastNode* root1=fklPopPtrStack(s1);
		if(root0!=root1)
		{
			if(root0->type!=root1->type)
			{
				r=0;
				break;
			}
			else
			{
				switch(root0->type)
				{
					case FKL_TYPE_SYM:
						r=root0->u.sym==root1->u.sym;
						break;
					case FKL_TYPE_STR:
						r=!fklStringcmp(root0->u.str,root1->u.str);
						break;
					case FKL_TYPE_BYTEVECTOR:
						r=!fklBytevectorcmp(root0->u.bvec,root1->u.bvec);
						break;
					case FKL_TYPE_CHR:
						r=root0->u.chr==root1->u.chr;
						break;
					case FKL_TYPE_I32:
						r=root0->u.i32==root1->u.i32;
						break;
					case FKL_TYPE_I64:
						r=root0->u.i64==root1->u.i64;
						break;
					case FKL_TYPE_BIG_INT:
						r=fklCmpBigInt(root0->u.bigInt,root1->u.bigInt);
						break;
					case FKL_TYPE_F64:
						r=root0->u.f64==root1->u.f64;
						break;
					case FKL_TYPE_PAIR:
						fklPushPtrStack(root0->u.pair->cdr,s0);
						fklPushPtrStack(root0->u.pair->car,s0);
						fklPushPtrStack(root1->u.pair->cdr,s1);
						fklPushPtrStack(root1->u.pair->car,s1);
						break;
					case FKL_TYPE_VECTOR:
						if(root0->u.vec->size!=root1->u.vec->size)
							r=0;
						else
						{
							size_t size=root0->u.vec->size;
							for(size_t i=0;i<size;i++)
							{
								fklPushPtrStack(root0->u.vec->base[i],s0);
								fklPushPtrStack(root1->u.vec->base[i],s1);
							}
						}
						break;
					case FKL_TYPE_HASHTABLE:
						if(root0->u.hash->type!=root1->u.hash->type
								||root0->u.hash->num!=root1->u.hash->num)
							r=0;
						else
						{
							size_t num=root0->u.hash->num;
							for(size_t i=0;i<num;i++)
							{
								fklPushPtrStack(root0->u.hash->items[i].car,s0);
								fklPushPtrStack(root0->u.hash->items[i].cdr,s0);
								fklPushPtrStack(root1->u.hash->items[i].car,s1);
								fklPushPtrStack(root1->u.hash->items[i].cdr,s1);
							}
						}
						break;
					default:
						FKL_ASSERT(0);
				}
			}
		}
	}
	fklFreePtrStack(s0);
	fklFreePtrStack(s1);
	return r;
}

static FklByteCode* new1lenBc(FklOpcode op)
{
	FklByteCode* r=fklNewByteCode(1);
	r->code[0]=op;
	return r;
}

static FklByteCode* new9lenBc(FklOpcode op,uint64_t size)
{
	FklByteCode* r=fklNewByteCode(1);
	r->code[0]=op;
	fklSetU64ToByteCode(r->code+sizeof(char),size);
	return r;
}

static FklByteCode* newPushVar(FklSid_t id)
{
	FklByteCode* r=new9lenBc(FKL_OP_PUSH_VAR,0);
	fklSetSidToByteCode(r->code+sizeof(char),id);
	return r;
}

//static FklByteCode* newPopVar(uint32_t scope,FklSid_t id)
//{
//	FklByteCode* r=fklNewByteCode(sizeof(char)+sizeof(uint32_t)+sizeof(FklSid_t));
//	r->code[0]=FKL_OP_POP_VAR;
//	fklSetU32ToByteCode(r->code+sizeof(char),scope);
//	fklSetSidToByteCode(r->code+sizeof(char)+sizeof(uint32_t),id);
//	return r;
//}

static FklByteCode* newPushTopPopVar(uint32_t scope,FklSid_t id)
{
	FklByteCode* r=fklNewByteCode(sizeof(char)+sizeof(char)+sizeof(uint32_t)+sizeof(FklSid_t));
	r->code[0]=FKL_OP_PUSH_TOP;
	r->code[sizeof(char)]=FKL_OP_POP_VAR;
	fklSetU32ToByteCode(r->code+sizeof(char)+sizeof(char),scope);
	fklSetSidToByteCode(r->code+sizeof(char)+sizeof(char)+sizeof(uint32_t),id);
	return r;
}

static FklByteCodelnt* newBclnt(FklByteCode* bc
		,FklSid_t fid
		,uint64_t line)
{
	FklByteCodelnt* r=fklNewByteCodelnt(bc);
	r->ls=1;
	r->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
	FKL_ASSERT(r->l);
	r->l[0]=fklNewLineNumTabNode(fid,0,r->bc->size,line);
	return r;
}

static FklByteCodelnt* makePushTopAndPopVar(const FklNastNode* sym,uint32_t scope,FklCompiler* comp)
{
	FklSid_t sid=fklAddSymbol(fklGetGlobSymbolWithId(sym->u.sym)->symbol,comp->globTable)->id;
	FklByteCodelnt* r=newBclnt(newPushTopPopVar(scope,sid),comp->fid,sym->curline);
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
		bcl->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
		FKL_ASSERT(bcl->l);
		bcl->l[0]=fklNewLineNumTabNode(fid,0,bc->size,line);
		fklCodeCat(bcl->bc,bc);
	}
	else
	{
		fklCodeCat(bcl->bc,bc);
		bcl->l[bcl->ls-1]->cpc+=bc->size;
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
		bcl->l=(FklLineNumTabNode**)malloc(sizeof(FklLineNumTabNode*)*1);
		FKL_ASSERT(bcl->l);
		bcl->l[0]=fklNewLineNumTabNode(fid,0,bc->size,line);
		fklCodeCat(bcl->bc,bc);
	}
	else
	{
		fklReCodeCat(bc,bcl->bc);
		bcl->l[0]->cpc+=bc->size;
		FKL_INCREASE_ALL_SCP(bcl->l+1,bcl->ls-1,bc->size);
	}
}

static FklCompileQuest* newCompileQuest(FklByteCodeProcesser f
		,FklPtrStack* stack
		,FklPtrQueue* queue
		,FklCompEnvN* env
		,uint64_t curline
		,FklCompiler* comp)
{
	FklCompileQuest* r=(FklCompileQuest*)malloc(sizeof(FklCompileQuest));
	FKL_ASSERT(r);
	r->processer=f;
	r->stack=stack;
	r->queue=queue;
	r->env=env;
	r->curline=curline;
	r->comp=comp;
	return r;
}

#define BC_PROCESS(NAME) static FklByteCodelnt* NAME(FklPtrStack* stack,FklSid_t fid,uint64_t line)

BC_PROCESS(_default_bc_process)
{
	if(!fklIsPtrStackEmpty(stack))
		return fklPopPtrStack(stack);
	else
		return NULL;
}

BC_PROCESS(_begin_exp_bc_process)
{
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
		FklByteCodelnt* retval=fklNewByteCodelnt(fklNewByteCode(0));
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
			fklFreeByteCodelnt(cur);
		}
		bcBclAppendToBcl(&setTp,retval,fid,line);
		bclBcAppendToBcl(retval,&popTp,fid,line);
		return retval;
	}
	else
		return newBclnt(new1lenBc(FKL_OP_PUSH_NIL),fid,line);
}

typedef struct
{
	FklSid_t id;
	FklNastNode* node;
}CompileHashTableItem;

static size_t _compile_hash_table_hash_func(void* key)
{
	FklSid_t sid=*(FklSid_t*)key;
	return sid;
}

static void _compile_hash_free_item(void* item)
{
	free(item);
}

static void* _compile_hash_get_key(void* item)
{
	return &((CompileHashTableItem*)item)->id;
}

static int _compile_hash_key_equal(void* pk0,void* pk1)
{
	FklSid_t k0=*(FklSid_t*)pk0;
	FklSid_t k1=*(FklSid_t*)pk1;
	return k0==k1;
}

static FklHashTableMethodTable Compile_hash_method_table=
{
	.__hashFunc=_compile_hash_table_hash_func,
	.__freeItem=_compile_hash_free_item,
	.__keyEqual=_compile_hash_key_equal,
	.__getKey=_compile_hash_get_key,
};

static FklNastNode* compileHashTableRef(FklSid_t sid,FklHashTable* ht)
{
	CompileHashTableItem* item=fklGetHashItem(&sid,ht);
	return item->node;
}

inline static CompileHashTableItem* newCompileHashTableItem(FklSid_t id,FklNastNode* node)
{
	CompileHashTableItem* r=(CompileHashTableItem*)malloc(sizeof(CompileHashTableItem*));
	FKL_ASSERT(r);
	r->id=id;
	r->node=node;
	return r;
}

static void compileHashTableSet(FklSid_t sid,FklNastNode* node,FklHashTable* ht)
{
	fklPutReplHashItem(newCompileHashTableItem(sid,node),ht);
}

static FklHashTable* newCompileHashTable(void)
{
	return fklNewHashTable(8,8,2,0.75,1,&Compile_hash_method_table);
}

inline static void pushListItemToQueue(FklNastNode* list,FklPtrQueue* queue)
{
	for(FklNastNode* cur=list;cur;cur=cur->u.pair->cdr)
		fklPushPtrQueue(cur,queue);
}

BC_PROCESS(_funcall_exp_bc_process)
{
	FklByteCodelnt* retval=fklNewByteCodelnt(fklNewByteCode(0));
	while(!fklIsPtrStackEmpty(stack))
	{
		FklByteCodelnt* cur=fklPopPtrStack(stack);
		fklCodeLntCat(retval,cur);
		fklFreeByteCodelnt(cur);
	}
	uint8_t opcodes[]={FKL_OP_SET_BP,FKL_OP_CALL};
	FklByteCode setBp={1,&opcodes[0],};
	FklByteCode call={1,&opcodes[1],};
	bcBclAppendToBcl(&setBp,retval,fid,line);
	bclBcAppendToBcl(retval,&call,fid,line);
	return retval;
}

static void compile_funcall(FklNastNode* rest
		,FklPtrStack* compileQuestStack
		,FklCompEnvN* env
		,FklCompiler* comp)
{
	FklPtrQueue* queue=fklNewPtrQueue();
	pushListItemToQueue(rest,queue);
	fklPushPtrStack(newCompileQuest(_funcall_exp_bc_process
				,fklNewPtrStack(32,16)
				,queue
				,env
				,rest->curline
				,comp)
			,compileQuestStack);
}

#define COMPILE_FUNC(NAME) void NAME(FklHashTable* ht\
		,FklPtrStack* compileQuestStack\
		,FklCompEnvN* curEnv\
		,FklCompiler* comp)

static COMPILE_FUNC(compile_begin)
{
	FklNastNode* rest=compileHashTableRef(builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklNewPtrQueue();
	pushListItemToQueue(rest,queue);
	fklPushPtrStack(newCompileQuest(_begin_exp_bc_process
				,fklNewPtrStack(32,16)
				,queue
				,curEnv
				,rest->curline
				,comp)
			,compileQuestStack);
}

BC_PROCESS(_and_exp_bc_process)
{
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
		FklByteCodelnt* retval=fklNewByteCodelnt(fklNewByteCode(0));
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
			fklFreeByteCodelnt(cur);
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
		return newBclnt(fklNewPushI32ByteCode(1),fid,line);
}

static COMPILE_FUNC(compile_and)
{
	FklNastNode* rest=compileHashTableRef(builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklNewPtrQueue();
	pushListItemToQueue(rest,queue);
	FklCompEnvN* andEnv=fklNewCompEnvN(curEnv);
	fklPushPtrStack(newCompileQuest(_and_exp_bc_process
				,fklNewPtrStack(32,16)
				,queue
				,andEnv
				,rest->curline
				,comp)
			,compileQuestStack);
}

BC_PROCESS(_or_exp_bc_process)
{
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
		FklByteCodelnt* retval=fklNewByteCodelnt(fklNewByteCode(0));
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
			fklFreeByteCodelnt(cur);
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
		return newBclnt(new1lenBc(FKL_OP_PUSH_NIL),fid,line);
}

static COMPILE_FUNC(compile_or)
{
	FklNastNode* rest=compileHashTableRef(builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklNewPtrQueue();
	pushListItemToQueue(rest,queue);
	FklCompEnvN* orEnv=fklNewCompEnvN(curEnv);
	fklPushPtrStack(newCompileQuest(_or_exp_bc_process
				,fklNewPtrStack(32,16)
				,queue
				,orEnv
				,rest->curline
				,comp)
			,compileQuestStack);
}


BC_PROCESS(_lambda_exp_bc_process)
{
	FklByteCodelnt* retval=NULL;
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
		retval=fklNewByteCodelnt(fklNewByteCode(0));
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
			fklFreeByteCodelnt(cur);
		}
		bcBclAppendToBcl(&setTp,retval,fid,line);
		bclBcAppendToBcl(retval,&popTp,fid,line);
	}
	else
		retval=newBclnt(new1lenBc(FKL_OP_PUSH_NIL),fid,line);
	fklReCodeLntCat(stack->base[0],retval);
	fklFreeByteCodelnt(stack->base[0]);
	return retval;
}

static FklByteCodelnt* makePopArg(FklOpcode code,const FklNastNode* sym,FklCompiler* comp)
{
	FklByteCode* bc=new9lenBc(code,0);
	FklSid_t sid=fklAddSymbol(fklGetGlobSymbolWithId(sym->u.sym)->symbol,comp->globTable)->id;
	fklSetSidToByteCode(bc->code+sizeof(char),sid);
	return newBclnt(bc,comp->fid,sym->curline);
}

static FklByteCodelnt* processArgs(const FklNastNode* args,FklCompEnvN* curEnv,FklCompiler* comp)
{
	FklByteCodelnt* retval=fklNewByteCodelnt(fklNewByteCode(0));
	for(;args->type==FKL_TYPE_PAIR;args=args->u.pair->cdr)
	{
		FklNastNode* cur=args->u.pair->car;
		fklAddCompDefBySid(cur->u.sym,curEnv);
		FklByteCodelnt* tmp=makePopArg(FKL_OP_POP_ARG,cur,comp);
		fklCodeLntCat(retval,tmp);
		fklFreeByteCodelnt(tmp);
	}
	if(args)
	{
		FklByteCodelnt* tmp=makePopArg(FKL_OP_POP_REST_ARG,args,comp);
		fklCodeLntCat(retval,tmp);
		fklFreeByteCodelnt(tmp);
	}
	uint8_t opcodes[]={FKL_OP_RES_BP,};
	FklByteCode resBp={1,&opcodes[0]};
	bclBcAppendToBcl(retval,&resBp,comp->fid,args->curline);
	return retval;
}

static COMPILE_FUNC(compile_lambda)
{
	FklNastNode* args=compileHashTableRef(builtInPatternVar_args,ht);
	FklNastNode* rest=compileHashTableRef(builtInPatternVar_rest,ht);
	FklPtrQueue* queue=fklNewPtrQueue();
	pushListItemToQueue(rest,queue);
	FklPtrStack* stack=fklNewPtrStack(32,16);
	FklCompEnvN* lambdaCompEnv=fklNewCompEnvN(curEnv);
	fklPushPtrStack(processArgs(args,lambdaCompEnv,comp),stack);
	fklPushPtrStack(newCompileQuest(_lambda_exp_bc_process
				,stack
				,queue
				,lambdaCompEnv
				,rest->curline
				,comp)
			,compileQuestStack);
}

BC_PROCESS(_define_exp_bc_process)
{
	FklByteCodelnt* cur=fklPopPtrStack(stack);
	FklByteCodelnt* popVar=fklPopPtrStack(stack);
	fklCodeLntCat(popVar,cur);
	fklFreeByteCodelnt(cur);
	return popVar;
}

static COMPILE_FUNC(compile_define)
{
	FklNastNode* name=compileHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* value=compileHashTableRef(builtInPatternVar_value,ht);
	FklPtrQueue* queue=fklNewPtrQueue();
	FklPtrStack* stack=fklNewPtrStack(2,1);
	fklAddCompDefBySid(name->u.sym,curEnv);
	fklPushPtrStack(makePushTopAndPopVar(name,1,comp),stack);
	fklPushPtrQueue(value,queue);
	fklPushPtrStack(newCompileQuest(_define_exp_bc_process
				,stack
				,queue
				,curEnv
				,name->curline
				,comp)
			,compileQuestStack);
}

BC_PROCESS(_setq_exp_bc_process)
{
	FklByteCodelnt* cur=fklPopPtrStack(stack);
	FklByteCodelnt* popVar=fklPopPtrStack(stack);
	fklCodeLntCat(popVar,cur);
	fklFreeByteCodelnt(cur);
	return popVar;
}

static COMPILE_FUNC(compile_setq)
{
	FklNastNode* name=compileHashTableRef(builtInPatternVar_name,ht);
	FklNastNode* value=compileHashTableRef(builtInPatternVar_value,ht);
	FklPtrQueue* queue=fklNewPtrQueue();
	FklPtrStack* stack=fklNewPtrStack(2,1);
	int isDefined=0;
	uint32_t scope=1;
	for(FklCompEnvN* cEnv=curEnv;cEnv&&!isDefined;cEnv=cEnv->prev,scope++)
		isDefined=fklIsSymbolDefined(name->u.sym,cEnv);
	if(!isDefined)
		scope=0;
	fklPushPtrStack(makePushTopAndPopVar(name,scope,comp),stack);
	fklPushPtrQueue(value,queue);
	fklPushPtrStack(newCompileQuest(_setq_exp_bc_process
				,stack
				,queue
				,curEnv
				,name->curline
				,comp)
			,compileQuestStack);
}

BC_PROCESS(_quote_exp_bc_process)
{
	return fklPopPtrStack(stack);
}

static COMPILE_FUNC(compile_quote)
{
	FklNastNode* value=compileHashTableRef(builtInPatternVar_value,ht);
	FklPtrStack* stack=fklNewPtrStack(1,1);
	fklPushPtrStack(newBclnt(fklCompileNode(value,comp),comp->fid,value->curline),stack);
	fklPushPtrStack(newCompileQuest(_quote_exp_bc_process
				,stack
				,NULL
				,curEnv
				,value->curline
				,comp)
			,compileQuestStack);
}

BC_PROCESS(_unquote_exp_bc_process)
{
	return fklPopPtrStack(stack);
}

static COMPILE_FUNC(compile_unquote)
{
	FklNastNode* value=compileHashTableRef(builtInPatternVar_value,ht);
	FklPtrQueue* queue=fklNewPtrQueue();
	fklPushPtrQueue(value,queue);
	fklPushPtrStack(newCompileQuest(_unquote_exp_bc_process
				,fklNewPtrStack(1,1)
				,queue
				,curEnv
				,value->curline
				,comp)
			,compileQuestStack);
}

//BC_PROCESS(_cond_exp_bc_process)
//{
//}
//
//BC_PROCESS(_try_exp_bc_process)
//{
//}

#undef BC_PROCESS

int fklPatternMatch(const FklNastNode* pattern,FklNastNode* exp,FklHashTable* ht)
{
	if(exp->type!=FKL_TYPE_PAIR)
		return 0;
	if(exp->u.pair->car->type==FKL_TYPE_SYM
			&&pattern->u.pair->car->u.sym!=exp->u.pair->car->u.sym)
		return 0;
	FklPtrStack* s0=fklNewPtrStack(32,16);
	FklPtrStack* s1=fklNewPtrStack(32,16);
	fklPushPtrStack(pattern->u.pair->cdr,s0);
	fklPushPtrStack(exp->u.pair->cdr,s1);
	while(!fklIsPtrStackEmpty(s0)&&!fklIsPtrStackEmpty(s1))
	{
		FklNastNode* n0=fklPopPtrStack(s0);
		FklNastNode* n1=fklPopPtrStack(s1);
		if(n0->type==FKL_TYPE_SYM)
			compileHashTableSet(n0->u.sym,n1,ht);
		else if(n0->type==FKL_TYPE_PAIR&&n1->type==FKL_TYPE_PAIR)
		{
			fklPushPtrStack(n0->u.pair->cdr,s0);
			fklPushPtrStack(n0->u.pair->car,s0);
			fklPushPtrStack(n1->u.pair->cdr,s1);
			fklPushPtrStack(n1->u.pair->car,s1);
		}
		else if(!nastNodeEqual(n0,n1))
		{
			fklFreePtrStack(s0);
			fklFreePtrStack(s1);
			return 0;
		}
	}
	return 1;
}

FklByteCode* fklCompileNode(const FklNastNode* node,FklCompiler* compiler)
{
	FklPtrStack* stack=fklNewPtrStack(32,16);
	fklPushPtrStack((void*)node,stack);
	FklByteCode* retval=fklNewByteCode(0);
	while(!fklIsPtrStackEmpty(stack))
	{
		FklNastNode* node=fklPopPtrStack(stack);
		FklByteCode* tmp=NULL;
		switch(node->type)
		{
			case FKL_TYPE_SYM:
				tmp=fklNewPushSidByteCode(fklAddSymbol(fklGetGlobSymbolWithId(node->u.sym)->symbol,compiler->globTable)->id);
				break;
			case FKL_TYPE_NIL:
				tmp=new1lenBc(FKL_OP_PUSH_NIL);
				break;
			case FKL_TYPE_CHR:
				tmp=fklNewPushCharByteCode(node->u.chr);
				break;
			case FKL_TYPE_I32:
				tmp=fklNewPushI32ByteCode(node->u.i32);
				break;
			case FKL_TYPE_I64:
				tmp=fklNewPushI64ByteCode(node->u.i64);
				break;
			case FKL_TYPE_F64:
				tmp=fklNewPushF64ByteCode(node->u.f64);
				break;
			case FKL_TYPE_BYTEVECTOR:
				tmp=fklNewPushBvecByteCode(node->u.bvec);
				break;
			case FKL_TYPE_STR:
				tmp=fklNewPushStrByteCode(node->u.str);
				break;
			case FKL_TYPE_BIG_INT:
				tmp=fklNewPushBigIntByteCode(node->u.bigInt);
				break;
			case FKL_TYPE_PAIR:
				tmp=new1lenBc(FKL_OP_PUSH_PAIR);
				fklPushPtrStack(node->u.pair->car,stack);
				fklPushPtrStack(node->u.pair->cdr,stack);
				break;
			case FKL_TYPE_BOX:
				tmp=new1lenBc(FKL_OP_PUSH_BOX);
				fklPushPtrStack(node->u.box,stack);
				break;
			case FKL_TYPE_VECTOR:
				tmp=new9lenBc(FKL_OP_PUSH_VECTOR,node->u.vec->size);
				for(size_t i=0;i<node->u.vec->size;i++)
					fklPushPtrStack(node->u.vec->base[i],stack);
				break;
			case FKL_TYPE_HASHTABLE:
				tmp=new9lenBc(FKL_OP_PUSH_HASHTABLE_EQ+node->u.hash->type,node->u.hash->num);
				for(size_t i=0;i<node->u.hash->num;i++)
				{
					fklPushPtrStack(node->u.hash->items->car,stack);
					fklPushPtrStack(node->u.hash->items->cdr,stack);
				}
				break;
			default:
				FKL_ASSERT(0);
				break;
		}
		fklReCodeCat(tmp,retval);
	}
	return retval;
}

static int matchAndCall(FklFormCompilerFunc func
		,const FklNastNode* pattern
		,FklNastNode* exp
		,FklPtrStack* compileQuestStack
		,FklCompEnvN* env
		,FklCompiler* compiler)
{
	FklHashTable* ht=newCompileHashTable();
	int r=fklPatternMatch(pattern,exp,ht);
	if(r)
		func(ht,compileQuestStack,env,compiler);
	fklFreeHashTable(ht);
	return r;
}

static struct PatternAndFunc
{
	char* ps;
	FklNastNode* pn;
	FklFormCompilerFunc func;
}buildIntPattern[]=
{
	{"(begin,rest)",        NULL, compile_begin,   },
	{"(define name value)", NULL, compile_define,  },
	{"(setq name value)",   NULL, compile_setq,    },
	{"(quote value)",       NULL, compile_quote,   },
	{"(unquote value)",     NULL, compile_unquote, },
	{"(lambda args,rest)",  NULL, compile_lambda,  },
	{"(and,rest)",          NULL, compile_and,     },
	{"(or,rest)",           NULL, compile_or,      },
	{NULL,                  NULL, NULL,            }
};

void fklInitBuiltInPattern(void)
{
	builtInPatternVar_rest=fklAddSymbolToGlobCstr("rest")->id;
	builtInPatternVar_name=fklAddSymbolToGlobCstr("name")->id;
	builtInPatternVar_args=fklAddSymbolToGlobCstr("args")->id;
	builtInPatternVar_value=fklAddSymbolToGlobCstr("value")->id;
	builtInPattern_head_quote=fklAddSymbolToGlobCstr("quote")->id;
	builtInPattern_head_qsquote=fklAddSymbolToGlobCstr("qsquote")->id;
	builtInPattern_head_unquote=fklAddSymbolToGlobCstr("unquote")->id;
	builtInPattern_head_unqtesp=fklAddSymbolToGlobCstr("unqtesp")->id;

	for(struct PatternAndFunc* cur=&buildIntPattern[0];cur->ps!=NULL;cur++)
		cur->pn=fklNewNastNodeFromCstr(cur->ps);
}

void fklUninitBuiltInPattern(void)
{
	for(struct PatternAndFunc* cur=&buildIntPattern[0];cur->ps!=NULL;cur++)
	{
		fklFreeNastNode(cur->pn);
		cur->pn=NULL;
	}
}

FklNastNode* fklNewNastNodeFromCstr(const char* cStr)
{
	FklPtrStack* tokenStack=fklNewPtrStack(8,16);
	size_t size=strlen(cStr);
	uint32_t line=0;
	uint32_t i,j=0;
	FklPtrStack* matchStateStack=fklNewPtrStack(8,16);
	fklSplitStringPartsIntoToken(&cStr
			,&size
			,0
			,&line
			,tokenStack
			,matchStateStack
			,&i
			,&j);
	FklNastNode* retval=fklNewNastNodeFromTokenStack(tokenStack);
	while(!fklIsPtrStackEmpty(tokenStack))
		fklFreeToken(fklPopPtrStack(tokenStack));
	while(!fklIsPtrStackEmpty(matchStateStack))
		free(fklPopPtrStack(matchStateStack));
	return retval;
}

static FklNastNode* newNastNode(FklValueType type,uint64_t line)
{
	FklNastNode* r=(FklNastNode*)malloc(sizeof(FklNastNode));
	FKL_ASSERT(r);
	r->curline=line;
	r->type=type;
	return r;
}

static FklNastPair* newNastPair(void)
{
	FklNastPair* pair=(FklNastPair*)malloc(sizeof(FklNastPair));
	FKL_ASSERT(pair);
	pair->car=NULL;
	pair->cdr=NULL;
	return pair;
}

static FklNastVector* newNastVector(size_t size)
{
	FklNastVector* vec=(FklNastVector*)malloc(sizeof(FklNastVector));
	FKL_ASSERT(vec);
	vec->size=size;
	vec->base=(FklNastNode**)calloc(size,sizeof(FklNastNode*));
	FKL_ASSERT(vec->base);
	return vec;
}

static FklNastNode* newNastList(FklNastNode** a,size_t num,uint64_t line)
{
	FklNastNode* r=NULL;
	FklNastNode** cur=&r;
	for(size_t i=0;i<num;i++)
	{
		(*cur)=newNastNode(FKL_TYPE_PAIR,line);
		(*cur)->u.pair=newNastPair();
		(*cur)->u.pair->car=a[i];
		cur=&(*cur)->u.pair->cdr;
	}
	(*cur)->u.pair->cdr=newNastNode(FKL_TYPE_NIL,line);
	return r;
}

static int fklIsValidCharStr(const char* str,size_t len)
{
	if(len==0)
		return 0;
	if(isalpha(str[0])&&len>1)
		return 0;
	if(str[0]=='\\')
	{
		if(len<2)
			return 0;
		if(toupper(str[1])=='X')
		{
			if(len<3||len>4)
				return 0;
			for(size_t i=2;i<len;i++)
				if(!isxdigit(str[i]))
					return 0;
		}
		else if(str[1]=='0')
		{
			if(len>5)
				return 0;
			if(len>2)
			{
				for(size_t i=2;i<len;i++)
					if(!isdigit(str[i])||str[i]>'7')
						return 0;
			}
		}
		else if(isdigit(str[1]))
		{
			if(len>4)
				return 0;
			for(size_t i=1;i<len;i++)
				if(!isdigit(str[i]))
					return 0;
		}
	}
	return 1;
}

static FklNastNode* createChar(const FklString* oStr,uint64_t line)
{
	if(!fklIsValidCharStr(oStr->str+2,oStr->size-2))
		return NULL;
	FklNastNode* r=newNastNode(FKL_TYPE_CHR,line);
	r->u.chr=fklCharBufToChar(oStr->str+2,oStr->size-2);
	return r;
}

static FklNastNode* createNum(const FklString* oStr,uint64_t line)
{
	FklNastNode* r=NULL;
	if(fklIsDoubleString(oStr))
	{
		r=newNastNode(FKL_TYPE_F64,line);
		r->u.f64=fklStringToDouble(oStr);
	}
	else
	{
		r=newNastNode(FKL_TYPE_I32,line);
		FklBigInt* bInt=fklNewBigIntFromString(oStr);
		if(fklIsGtLtI64BigInt(bInt))
			r->u.bigInt=bInt;
		else
		{
			int64_t num=fklBigIntToI64(bInt);
			if(num>INT32_MAX||num<INT32_MIN)
			{
				r->type=FKL_TYPE_I64;
				r->u.i64=num;
			}
			else
				r->u.i32=num;
			fklFreeBigInt(bInt);
		}
	}
	return r;
}

static FklNastNode* createString(const FklString* oStr,uint64_t line)
{
	size_t size=0;
	char* str=fklCastEscapeCharBuf(oStr->str+1,'\"',&size);
	FklNastNode* r=newNastNode(FKL_TYPE_STR,line);
	r->u.str=fklNewString(size,str);
	free(str);
	return r;
}

static FklNastNode* createSymbol(const FklString* oStr,uint64_t line)
{
	FklNastNode* r=newNastNode(FKL_TYPE_SYM,line);
	r->u.sym=fklAddSymbolToGlob(oStr)->id;
	return r;
}

static FklNastNode* (*literalNodeCreator[])(const FklString*,uint64_t)=
{
	createChar,
	createNum,
	createString,
	createSymbol,
};

typedef enum
{
	NAST_CAR,
	NAST_CDR,
	NAST_BOX,
}NastPlace;

typedef struct
{
	NastPlace place;
	FklNastNode* node;
}NastElem;

static NastElem* newNastElem(NastPlace place,FklNastNode* node)
{
	NastElem* r=(NastElem*)malloc(sizeof(NastElem));
	FKL_ASSERT(r);
	r->node=node;
	r->place=place;
	return r;
}

typedef struct
{
	FklStringMatchPattern* pattern;
	uint32_t line;
	uint32_t cindex;
}MatchState;

static void freeMatchState(MatchState* state)
{
	free(state);
}

#define PARENTHESE_0 ((void*)0)
#define PARENTHESE_1 ((void*)1)
#define QUOTE ((void*)2)
#define QSQUOTE ((void*)3)
#define UNQUOTE ((void*)4)
#define UNQTESP ((void*)5)
#define DOTTED ((void*)6)
#define VECTOR_0 ((void*)7)
#define VECTOR_1 ((void*)8)
#define BOX ((void*)9)
#define BVECTOR_0 ((void*)10)
#define BVECTOR_1 ((void*)11)
#define HASH_EQ_0 ((void*)12)
#define HASH_EQ_1 ((void*)13)
#define HASH_EQV_0 ((void*)14)
#define HASH_EQV_1 ((void*)15)
#define HASH_EQUAL_0 ((void*)16)
#define HASH_EQUAL_1 ((void*)17)

static int isBuiltInSingleStrPattern(FklStringMatchPattern* pattern)
{
	return pattern==QUOTE
		||pattern==QSQUOTE
		||pattern==UNQUOTE
		||pattern==UNQTESP
		||pattern==DOTTED
		||pattern==BOX
		;
}

static int isBuiltInHashTable(FklStringMatchPattern* pattern)
{
	return pattern==HASH_EQ_0
		||pattern==HASH_EQ_1
		||pattern==HASH_EQV_0
		||pattern==HASH_EQV_1
		||pattern==HASH_EQUAL_0
		||pattern==HASH_EQUAL_1
		;
}

static int isBuiltInParenthese(FklStringMatchPattern* pattern)
{
	return pattern==PARENTHESE_0
		||pattern==PARENTHESE_1
		;
}

static int isBuiltInVector(FklStringMatchPattern* pattern)
{
	return pattern==VECTOR_0
		||pattern==VECTOR_1
		;
}

static int isBuiltInBvector(FklStringMatchPattern* pattern)
{
	return pattern==BVECTOR_0
		||pattern==BVECTOR_1
		;
}

static int isLeftParenthese(const FklString* str)
{
	return str->size==1&&(str->str[0]=='('||str->str[0]=='[');
}

static int isVectorStart(const FklString* str)
{
	return str->size==2&&str->str[0]=='#'&&(str->str[1]=='('||str->str[1]=='[');
}

static int isBytevectorStart(const FklString* str)
{
	return str->size==5&&!strncmp(str->str,"#vu8",4)&&(str->str[4]=='('||str->str[4]=='[');
}

static int isRightParenthese(const FklString* str)
{
	return str->size==1&&(str->str[0]==')'||str->str[0]==']');
}

static FklStringMatchPattern* getHashPattern(const FklString* str)
{
	if(str->size==6&&!strncmp(str->str,"#hash",5))
	{
		if(str->str[5]=='(')
			return HASH_EQ_0;
		if(str->str[5]=='[')
			return HASH_EQ_1;
		return NULL;
	}
	else if(str->size==9&&!strncmp(str->str,"#hasheqv",8))
	{
		if(str->str[8]=='(')
			return HASH_EQV_0;
		if(str->str[8]=='[')
			return HASH_EQV_1;
		return NULL;
	}
	else if(str->size==11&&!strncmp(str->str,"#hashequal",10))
	{
		if(str->str[10]=='(')
			return HASH_EQUAL_0;
		if(str->str[10]=='[')
			return HASH_EQUAL_1;
		return NULL;
	}
	return NULL;
}

static int isHashTableStart(const FklString* str)
{
	return (str->size==6&&!strncmp(str->str,"#hash",5)&&(str->str[5]=='('||str->str[5]=='['))
		||(str->size==9&&!strncmp(str->str,"#hasheqv",8)&&(str->str[8]=='('||str->str[8]=='['))
		||(str->size==11&&!strncmp(str->str,"#hashequal",10)&&(str->str[10]=='('||str->str[10]=='['))
		;
}

static int isBuilInPatternStart(const FklString* str)
{
	return isLeftParenthese(str)
		||isVectorStart(str)
		||isBytevectorStart(str)
		||isHashTableStart(str)
		;
}

static int isBuiltInSingleStr(const FklString* str)
{
	const char* buf=str->str;
	return (str->size==1&&(buf[0]=='\''
				||buf[0]=='`'
				||buf[0]=='~'
				||buf[0]==','))
		||!fklStringCstrCmp(str,"~@")
		||!fklStringCstrCmp(str,"#&");
}

static int isBuiltInPattern(FklStringMatchPattern* pattern)
{
	return isBuiltInSingleStrPattern(pattern)
		||isBuiltInParenthese(pattern)
		||isBuiltInVector(pattern)
		||isBuiltInBvector(pattern)
		||isBuiltInHashTable(pattern);
}

static MatchState* searchReverseStringCharMatchState(const FklString* str,FklPtrStack* matchStateStack)
{
	MatchState* top=fklTopPtrStack(matchStateStack);
	uint32_t i=0;
	for(;top&&!isBuiltInPattern(top->pattern)
			&&top->cindex+i<top->pattern->num;)
	{
		const FklString* cpart=fklGetNthPartOfStringMatchPattern(top->pattern,top->cindex+i);
		if(!fklIsVar(cpart)&&!fklStringcmp(str,cpart))
		{
			top->cindex+=i;
			return top;
		}
		i++;
	}
	return NULL;
}


static MatchState* newMatchState(FklStringMatchPattern* pattern,uint32_t line,uint32_t cindex)
{
	MatchState* state=(MatchState*)malloc(sizeof(MatchState));
	FKL_ASSERT(state);
	state->pattern=pattern;
	state->line=line;
	state->cindex=cindex;
	return state;
}

#define BUILTIN_PATTERN_START_PROCESS(PATTERN) FklStringMatchPattern* pattern=(PATTERN);\
						MatchState* state=newMatchState(pattern,token->line,0);\
						fklPushPtrStack(state,matchStateStack);\
						cStack=fklNewPtrStack(32,16);\
						fklPushPtrStack(cStack,stackStack)\

static void freeNastPair(FklNastPair* pair)
{
	free(pair);
}

static void freeNastVector(FklNastVector* vec)
{
	free(vec->base);
	free(vec);
}

static void freeNastHash(FklNastHashTable* hash)
{
	free(hash->items);
	free(hash);
}

void fklFreeNastNode(FklNastNode* node)
{
	FklPtrStack* stack=fklNewPtrStack(32,16);
	fklPushPtrStack(node,stack);
	while(!fklIsPtrStackEmpty(stack))
	{
		FklNastNode* cur=fklPopPtrStack(stack);
		if(cur)
		{
			switch(cur->type)
			{
				case FKL_TYPE_I64:
				case FKL_TYPE_I32:
				case FKL_TYPE_SYM:
				case FKL_TYPE_NIL:
				case FKL_TYPE_CHR:
				case FKL_TYPE_F64:
					break;
				case FKL_TYPE_STR:
					free(cur->u.str);
					break;
				case FKL_TYPE_BYTEVECTOR:
					free(cur->u.bvec);
					break;
				case FKL_TYPE_BIG_INT:
					fklFreeBigInt(cur->u.bigInt);
					break;
				case FKL_TYPE_PAIR:
					fklPushPtrStack(cur->u.pair->car,stack);
					fklPushPtrStack(cur->u.pair->cdr,stack);
					freeNastPair(cur->u.pair);
					break;
				case FKL_TYPE_BOX:
					fklPushPtrStack(cur->u.box,stack);
					break;
				case FKL_TYPE_VECTOR:
					for(size_t i=0;i<cur->u.vec->size;i++)
						fklPushPtrStack(cur->u.vec->base[i],stack);
					freeNastVector(cur->u.vec);
					break;
				case FKL_TYPE_HASHTABLE:
					for(size_t i=0;i<cur->u.hash->num;i++)
					{
						fklPushPtrStack(cur->u.hash->items[i].car,stack);
						fklPushPtrStack(cur->u.hash->items[i].cdr,stack);
					}
					freeNastHash(cur->u.hash);
					break;
				default:
					FKL_ASSERT(0);
					break;
			}
			free(cur);
		}
	}
}

static FklNastNode* listProcesser(FklPtrStack* nodeStack,uint64_t line)
{
	FklNastNode* retval=NULL;
	FklNastNode** cur=&retval;
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR)
		{
			(*cur)=newNastNode(FKL_TYPE_PAIR,node->curline);
			(*cur)->u.pair=newNastPair();
			(*cur)->u.pair->car=node;
			cur=&(*cur)->u.pair->cdr;
		}
		else if((*cur)==NULL)
			(*cur)=node;
		else
		{
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[i];
				fklFreeNastNode(elem->node);
				free(elem);
			}
			fklFreeNastNode(retval);
			retval=NULL;
			break;
		}
		free(elem);
	}
	return retval;
}

static FklNastNode* vectorProcesser(FklPtrStack* nodeStack,uint64_t line)
{
	FklNastNode* retval=newNastNode(FKL_TYPE_VECTOR,line);
	retval->u.vec=newNastVector(nodeStack->top);
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR)
			retval->u.vec->base[i]=node;
		else
		{
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[i];
				fklFreeNastNode(elem->node);
				free(elem);
			}
			fklFreeNastNode(retval);
			retval=NULL;
			break;
		}
		free(elem);
	}
	return retval;
}

static FklNastNode* bytevectorProcesser(FklPtrStack* nodeStack,uint64_t line)
{
	FklNastNode* retval=newNastNode(FKL_TYPE_BYTEVECTOR,line);
	retval->u.bvec=fklNewBytevector(nodeStack->top,NULL);
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR
				&&(node->type==FKL_TYPE_I32
					||node->type==FKL_TYPE_I64
					||node->type==FKL_TYPE_BIG_INT))
		{
			retval->u.bvec->ptr[i]=node->type==FKL_TYPE_I32
				?node->u.i32
				:node->type==FKL_TYPE_I64
				?node->u.i64
				:fklBigIntToI64(node->u.bigInt);
			fklFreeNastNode(node);
		}
		else
		{
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[i];
				fklFreeNastNode(elem->node);
				free(elem);
			}
			fklFreeNastNode(retval);
			retval=NULL;
			break;
		}
		free(elem);
	}
	return retval;
}

static FklNastHashTable* newNastHash(FklVMhashTableEqType type,size_t num)
{
	FklNastHashTable* r=(FklNastHashTable*)malloc(sizeof(FklNastHashTable));
	FKL_ASSERT(r);
	r->items=(FklNastPair*)calloc(sizeof(FklNastPair),num);
	FKL_ASSERT(r->items);
	r->num=num;
	r->type=type;
	return r;
}

static FklNastNode* hashEqProcesser(FklPtrStack* nodeStack,uint64_t line)
{
	FklNastNode* retval=newNastNode(FKL_TYPE_HASHTABLE,line);
	retval->u.hash=newNastHash(FKL_VM_HASH_EQ,nodeStack->top);
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR&&node->type==FKL_TYPE_PAIR)
		{
			retval->u.hash->items[i].car=node->u.pair->car;
			retval->u.hash->items[i].cdr=node->u.pair->cdr;
			node->u.pair->car=NULL;
			node->u.pair->cdr=NULL;
			fklFreeNastNode(node);
		}
		else
		{
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[i];
				fklFreeNastNode(elem->node);
				free(elem);
			}
			fklFreeNastNode(retval);
			retval=NULL;
			break;
		}
		free(elem);
	}
	return retval;
}

static FklNastNode* hashEqvProcesser(FklPtrStack* nodeStack,uint64_t line)
{
	FklNastNode* retval=newNastNode(FKL_TYPE_HASHTABLE,line);
	retval->u.hash=newNastHash(FKL_VM_HASH_EQV,nodeStack->top);
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR&&node->type==FKL_TYPE_PAIR)
		{
			retval->u.hash->items[i].car=node->u.pair->car;
			retval->u.hash->items[i].cdr=node->u.pair->cdr;
			node->u.pair->car=NULL;
			node->u.pair->cdr=NULL;
			fklFreeNastNode(node);
		}
		else
		{
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[i];
				fklFreeNastNode(elem->node);
				free(elem);
			}
			fklFreeNastNode(retval);
			retval=NULL;
			break;
		}
		free(elem);
	}
	return retval;
}

static FklNastNode* hashEqualProcesser(FklPtrStack* nodeStack,uint64_t line)
{
	FklNastNode* retval=newNastNode(FKL_TYPE_HASHTABLE,line);
	retval->u.hash=newNastHash(FKL_VM_HASH_EQUAL,nodeStack->top);
	for(size_t i=0;i<nodeStack->top;i++)
	{
		NastElem* elem=nodeStack->base[i];
		FklNastNode* node=elem->node;
		NastPlace place=elem->place;
		if(place==NAST_CAR&&node->type==FKL_TYPE_PAIR)
		{
			retval->u.hash->items[i].car=node->u.pair->car;
			retval->u.hash->items[i].cdr=node->u.pair->cdr;
			node->u.pair->car=NULL;
			node->u.pair->cdr=NULL;
			fklFreeNastNode(node);
		}
		else
		{
			for(size_t j=i;j<nodeStack->top;j++)
			{
				NastElem* elem=nodeStack->base[i];
				fklFreeNastNode(elem->node);
				free(elem);
			}
			fklFreeNastNode(retval);
			retval=NULL;
			break;
		}
		free(elem);
	}
	return retval;
}


static FklNastNode* (*nastStackProcessers[])(FklPtrStack*,uint64_t)=
{
	listProcesser,
	listProcesser,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	vectorProcesser,
	vectorProcesser,
	NULL,
	bytevectorProcesser,
	bytevectorProcesser,
	hashEqProcesser,
	hashEqProcesser,
	hashEqvProcesser,
	hashEqvProcesser,
	hashEqualProcesser,
	hashEqualProcesser,
};

FklNastNode* fklNewNastNodeFromTokenStack(FklPtrStack* tokenStack)
{
	FklNastNode* retval=NULL;
	if(!fklIsPtrStackEmpty(tokenStack))
	{
		FklPtrStack* matchStateStack=fklNewPtrStack(32,16);
		FklPtrStack* stackStack=fklNewPtrStack(32,16);
		FklPtrStack* elemStack=fklNewPtrStack(32,16);
		fklPushPtrStack(elemStack,stackStack);
		for(uint32_t i=0;i<tokenStack->top;i++)
		{
			FklToken* token=tokenStack->base[i];
			FklPtrStack* cStack=fklTopPtrStack(stackStack);
			if(token->type>FKL_TOKEN_RESERVE_STR&&token->type<FKL_TOKEN_COMMENT)
			{
				FklNastNode* singleLiteral=literalNodeCreator[token->type-FKL_TOKEN_CHAR](token->value,token->line);
				NastElem* elem=newNastElem(NAST_CAR,singleLiteral);
				fklPushPtrStack(elem,cStack);
			}
			else if(token->type==FKL_TOKEN_RESERVE_STR)
			{
				//MatchState* state=searchReverseStringCharMatchState(token->value
				//		,matchStateStack);
				FklStringMatchPattern* pattern=fklFindStringPattern(token->value);
				if(pattern)
				{
					MatchState* state=newMatchState(pattern,token->line,1);
					fklPushPtrStack(state,matchStateStack);
					cStack=fklNewPtrStack(32,16);
					fklPushPtrStack(cStack,stackStack);
					const FklString* part=fklGetNthPartOfStringMatchPattern(pattern,state->cindex);
					if(fklIsVar(part)&&fklIsMustList(part))
					{
						cStack=fklNewPtrStack(32,16);
						fklPushPtrStack(cStack,stackStack);
					}
					continue;
				}
				else if(isBuilInPatternStart(token->value))
				{
					if(isLeftParenthese(token->value))
					{
						BUILTIN_PATTERN_START_PROCESS(token->value->str[0]=='('
								?PARENTHESE_0
								:PARENTHESE_1);
					}
					else if(isVectorStart(token->value))
					{
						BUILTIN_PATTERN_START_PROCESS(token->value->str[1]=='('
								?VECTOR_0
								:VECTOR_1);
					}
					else if(isBytevectorStart(token->value))
					{
						BUILTIN_PATTERN_START_PROCESS(token->value->str[4]=='('
								?BVECTOR_0
								:BVECTOR_1);
					}
					else if(isHashTableStart(token->value))
					{
						BUILTIN_PATTERN_START_PROCESS(getHashPattern(token->value));
					}
					continue;
				}
				else if(isBuiltInSingleStr(token->value))
				{
					const char* buf=token->value->str;
					FklStringMatchPattern* pattern=buf[0]=='\''?
						QUOTE:
						buf[0]=='`'?
						QSQUOTE:
						buf[0]==','?
						DOTTED:
						buf[0]=='~'?
						(token->value->size==1?
						 UNQUOTE:
						 buf[1]=='@'?
						 UNQTESP:NULL):
						(token->value->size==2&&buf[0]=='#'&&buf[1]=='&')?
						BOX:
						NULL;
					MatchState* state=newMatchState(pattern,token->line,0);
					fklPushPtrStack(state,matchStateStack);
					cStack=fklNewPtrStack(1,1);
					fklPushPtrStack(cStack,stackStack);
					continue;
				}
				else if(isRightParenthese(token->value))
				{
					fklPopPtrStack(stackStack);
					MatchState* cState=fklPopPtrStack(matchStateStack);
					FklNastNode* n=nastStackProcessers[(uintptr_t)cState->pattern](cStack,token->line);
					NastElem* v=newNastElem(NAST_CAR,n);
					fklFreePtrStack(cStack);
					cStack=fklTopPtrStack(stackStack);
					freeMatchState(cState);
					fklPushPtrStack(v,cStack);
				}
			}
			for(MatchState* state=fklTopPtrStack(matchStateStack);
					state&&(isBuiltInSingleStrPattern(state->pattern)||(!isBuiltInParenthese(state->pattern)
							&&!isBuiltInVector(state->pattern)
							&&!isBuiltInBvector(state->pattern)
							&&!isBuiltInHashTable(state->pattern)));
					state=fklTopPtrStack(matchStateStack))
			{
				if(isBuiltInSingleStrPattern(state->pattern))
				{
					fklPopPtrStack(stackStack);
					MatchState* cState=fklPopPtrStack(matchStateStack);
					NastElem* postfix=fklPopPtrStack(cStack);
					fklFreePtrStack(cStack);
					cStack=fklTopPtrStack(stackStack);
					if(state->pattern==DOTTED)
					{
						postfix->place=NAST_CDR;
						fklPushPtrStack(postfix,cStack);
					}
					else if(state->pattern==BOX)
					{
						FklNastNode* box=newNastNode(FKL_TYPE_BOX,token->line);
						box->u.box=postfix->node;
						NastElem* v=newNastElem(NAST_CAR,box);
						fklPushPtrStack(v,cStack);
						free(postfix);
					}
					else
					{
						FklNastNode* prefix=newNastNode(FKL_TYPE_SYM,token->line);
						FklStringMatchPattern* pattern=cState->pattern;
						FklSid_t prefixValue=pattern==QUOTE?
							builtInPattern_head_quote:
							pattern==QSQUOTE?
							builtInPattern_head_qsquote:
							pattern==UNQUOTE?
							builtInPattern_head_unquote:
							builtInPattern_head_unqtesp;
						prefix->u.sym=prefixValue;
						FklNastNode* tmp[]={prefix,postfix->node,};
						FklNastNode* list=newNastList(tmp,2,prefix->curline);
						free(postfix);
						NastElem* v=newNastElem(NAST_CAR,list);
						fklPushPtrStack(v,cStack);
					}
					freeMatchState(cState);
				}
			}
		}
	}
	return retval;
}

#undef BUILTIN_PATTERN_START_PROCESS
FklByteCodelnt* fklMakePushVar(const FklNastNode* exp,FklCompiler* comp)
{
	FklSid_t sid=fklAddSymbol(fklGetGlobSymbolWithId(exp->u.sym)->symbol,comp->globTable)->id;
	FklByteCodelnt* retval=newBclnt(newPushVar(sid),comp->fid,exp->curline);
	return retval;
}

static inline int mapAllBuiltInPattern(FklNastNode* curExp,FklPtrStack* compileQuestStack,FklCompEnvN* curEnv,FklCompiler* compiler)
{
	for(struct PatternAndFunc* cur=&buildIntPattern[0];cur->ps!=NULL;cur++)
		if(matchAndCall(cur->func,cur->pn,curExp,compileQuestStack,curEnv,compiler))
			return 0;
	return 1;
}

FklByteCodelnt* fklCompileExpression(const FklNastNode* exp
		,FklCompEnvN* globalEnv
		,FklCompiler* compiler)
{
	FklPtrStack* compileQuestStack=fklNewPtrStack(32,16);
	FklPtrQueue* queue=fklNewPtrQueue();
	FklPtrStack* resultStack=fklNewPtrStack(8,8);
	fklPushPtrQueue((void*)exp,queue);
	fklPushPtrStack(newCompileQuest(_default_bc_process
				,fklNewPtrStack(32,16)
				,queue
				,globalEnv
				,exp->curline
				,compiler)
			,compileQuestStack);
	while(!fklIsPtrStackEmpty(compileQuestStack))
	{
		FklCompileQuest* curCompiling=fklTopPtrStack(compileQuestStack);
		FklPtrQueue* queue=curCompiling->queue;
		FklCompEnvN* curEnv=curCompiling->env;
		FklPtrStack* curBcStack=curCompiling->stack;
		while(!fklIsPtrQueueEmpty(queue))
		{
			FklNastNode* curExp=fklPopPtrQueue(queue);
			if(mapAllBuiltInPattern(curExp,compileQuestStack,curEnv,compiler))
			{
				if(curExp->type==FKL_TYPE_PAIR)
				{
					compile_funcall(curExp,compileQuestStack,curEnv,compiler);
					break;
				}
				else if(curExp->type==FKL_TYPE_SYM)
					fklPushPtrStack(fklMakePushVar(curExp,compiler),curBcStack);
				else
					fklPushPtrStack(newBclnt(fklCompileNode(curExp,compiler)
								,compiler->fid
								,curExp->curline)
							,curBcStack);
			}
		}
		FklCompileQuest* otherCompiling=fklTopPtrStack(compileQuestStack);
		if(otherCompiling==curCompiling)
		{
			fklPopPtrStack(compileQuestStack);
			fklPushPtrStack(curCompiling->processer(curBcStack
						,compiler->fid
						,curCompiling->curline)
					,fklIsPtrStackEmpty(compileQuestStack)
					?resultStack
					:((FklCompileQuest*)fklTopPtrStack(compileQuestStack))->stack);
		}
	}
	FklByteCodelnt* retval=NULL;
	if(!fklIsPtrStackEmpty(resultStack))
		retval=fklPopPtrStack(resultStack);
	fklFreePtrStack(resultStack);
	return retval;
}
