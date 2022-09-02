#include<fakeLisp/ncompiler.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/utils.h>

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
						r=!fklStringcmp(&root0->u.str,&root1->u.str);
						break;
					case FKL_TYPE_BYTEVECTOR:
						r=!fklBytevectorcmp(&root0->u.bvec,&root1->u.bvec);
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
						r=fklCmpBigInt(&root0->u.bigInt,&root1->u.bigInt);
						break;
					case FKL_TYPE_F64:
						r=root0->u.f64==root1->u.f64;
						break;
					case FKL_TYPE_PAIR:
						fklPushPtrStack(root0->u.pair.cdr,s0);
						fklPushPtrStack(root0->u.pair.car,s0);
						fklPushPtrStack(root1->u.pair.cdr,s1);
						fklPushPtrStack(root1->u.pair.car,s1);
						break;
					case FKL_TYPE_VECTOR:
						if(root0->u.vec.size!=root1->u.vec.size)
							r=0;
						else
						{
							size_t size=root0->u.vec.size;
							for(size_t i=0;i<size;i++)
							{
								fklPushPtrStack(root0->u.vec.base[i],s0);
								fklPushPtrStack(root1->u.vec.base[i],s1);
							}
						}
						break;
					case FKL_TYPE_HASHTABLE:
						if(root0->u.hash.type!=root1->u.hash.type
								||root0->u.hash.num!=root1->u.hash.num)
							r=0;
						else
						{
							size_t num=root0->u.hash.num;
							for(size_t i=0;i<num;i++)
							{
								fklPushPtrStack(root0->u.hash.items[i].key,s0);
								fklPushPtrStack(root0->u.hash.items[i].value,s0);
								fklPushPtrStack(root1->u.hash.items[i].key,s1);
								fklPushPtrStack(root1->u.hash.items[i].value,s1);
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

static FklNextCompile* newNextCompile(FklByteCodeProcesser f
		,FklPtrStack* stack
		,FklPtrQueue* queue
		,FklCompEnvN* env
		,uint64_t curline
		,FklCompiler* comp)
{
	FklNextCompile* r=(FklNextCompile*)malloc(sizeof(FklNextCompile));
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

BC_PROCESS(_begin_exp_bc_process)
{
	if(stack->top)
	{
		FklByteCode* setTp=new1lenBc(FKL_OP_SET_TP);
		FklByteCode* resTp=new1lenBc(FKL_OP_RES_TP);
		FklByteCode* popTp=new1lenBc(FKL_OP_POP_TP);
		FklByteCodelnt* retval=fklNewByteCodelnt(fklNewByteCode(0));
		size_t top=stack->top;
		for(size_t i=0;i<top;i++)
		{
			FklByteCodelnt* cur=stack->base[i];
			if(cur->bc->size)
			{
				fklCodeLntCat(retval,cur);
				if(i<top-1)
					bclBcAppendToBcl(retval,resTp,fid,line);
			}
			fklFreeByteCodelnt(cur);
		}
		bcBclAppendToBcl(setTp,retval,fid,line);
		bclBcAppendToBcl(retval,popTp,fid,line);
		fklFreeByteCode(setTp);
		fklFreeByteCode(resTp);
		fklFreeByteCode(popTp);
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
	for(FklNastNode* cur=list;cur;cur=cur->u.pair.cdr)
		fklPushPtrQueue(cur,queue);
}

static void compile_begin(FklHashTable* ht
		,FklPtrStack* nextCompileStack
		,FklCompEnvN* cur
		,FklCompiler* comp)
{
	FklNastNode* rest=compileHashTableRef(fklAddSymbolToGlobCstr("rest")->id,ht);
	FklPtrQueue* queue=fklNewPtrQueue();
	pushListItemToQueue(rest,queue);
	fklPushPtrStack(newNextCompile(_begin_exp_bc_process
				,fklNewPtrStack(32,16)
				,queue
				,cur
				,rest->curline
				,comp)
			,nextCompileStack);
}

//BC_PROCESS(_funcall_exp_bc_process)
//{
//}
//
//BC_PROCESS(_define_exp_bc_process)
//{
//}
//
//BC_PROCESS(_setq_exp_bc_process)
//{
//}
//
//BC_PROCESS(_cond_exp_bc_process)
//{
//}
//
//BC_PROCESS(_lambda_exp_bc_process)
//{
//}
//
//BC_PROCESS(_and_exp_bc_process)
//{
//}
//
//BC_PROCESS(_or_exp_bc_process)
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
	if(exp->u.pair.car->type==FKL_TYPE_SYM
			&&pattern->u.pair.car->u.sym!=exp->u.pair.car->u.sym)
		return 0;
	FklPtrStack* s0=fklNewPtrStack(32,16);
	FklPtrStack* s1=fklNewPtrStack(32,16);
	fklPushPtrStack(pattern->u.pair.cdr,s0);
	fklPushPtrStack(exp->u.pair.cdr,s1);
	while(!fklIsPtrStackEmpty(s0)&&!fklIsPtrStackEmpty(s1))
	{
		FklNastNode* n0=fklPopPtrStack(s0);
		FklNastNode* n1=fklPopPtrStack(s1);
		if(n0->type==FKL_TYPE_SYM)
			compileHashTableSet(n0->u.sym,n1,ht);
		else if(n0->type==FKL_TYPE_PAIR&&n1->type==FKL_TYPE_PAIR)
		{
			fklPushPtrStack(n0->u.pair.cdr,s0);
			fklPushPtrStack(n0->u.pair.car,s0);
			fklPushPtrStack(n1->u.pair.cdr,s1);
			fklPushPtrStack(n1->u.pair.car,s1);
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

FklByteCode* fklCompileNode(const FklNastNode* node)
{
}

static int matchAndCall(FklFormCompilerFunc func
		,const FklNastNode* pattern
		,FklNastNode* exp
		,FklPtrStack* nextCompileStack
		,FklCompEnvN* env
		,FklCompiler* compiler)
{
	FklHashTable* ht=newCompileHashTable();
	int r=fklPatternMatch(pattern,exp,ht);
	if(r)
		func(ht,env,nextCompileStack,compiler);
	fklFreeHashTable(ht);
	return r;
}

FklByteCodelnt* fklCompileExpression(const FklNastNode* exp
		,FklCompEnvN* globalEnv
		,FklCompiler* compiler)
{
	FklPtrStack* nextCompileStack=fklNewPtrStack(32,16);
}
