#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/parser.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>

typedef struct
{
	FklVMvalue* key;
	uint64_t line;
}LineNumHashItem;

static size_t _LineNumHash_hashFunc(void* pKey)
{
	FklVMvalue* key=*(FklVMvalue**)pKey;
	return (uintptr_t)FKL_GET_PTR(key)>>FKL_UNUSEDBITNUM;
}

static int _LineNumHash_keyEqual(void* pKey0,void* pKey1)
{
	FklVMvalue* key0=*(FklVMvalue**)pKey0;
	FklVMvalue* key1=*(FklVMvalue**)pKey1;
	return key0==key1;
}

static void _LineNumHash_destroyItem(void* item)
{
	free(item);
}

static void* _LineNumHash_getKey(void* item)
{
	return &((LineNumHashItem*)item)->key;
}

static FklHashTableMethodTable LineNumHashMethTable=
{
	.__hashFunc=_LineNumHash_hashFunc,
	.__destroyItem=_LineNumHash_destroyItem,
	.__keyEqual=_LineNumHash_keyEqual,
	.__getKey=_LineNumHash_getKey,
};

FklHashTable* fklCreateLineNumHashTable(void)
{
	FklHashTable* lineHash=fklCreateHashTable(32,8,2,0.75,1,&LineNumHashMethTable);
	return lineHash;
}

static LineNumHashItem* createLineNumHashItem(FklVMvalue* value,FklVMvalue* refBy,uint64_t lineNum)
{
	LineNumHashItem* item=(LineNumHashItem*)malloc(sizeof(LineNumHashItem));
	FKL_ASSERT(item);
	item->key=value;
	item->line=lineNum;
	return item;
}

#define SENTINEL_NAST_NODE (NULL)
FklVMvalue* fklCreateVMvalueFromNastNodeAndStoreInStack(const FklNastNode* node
		,FklHashTable* lineHash
		,FklVM* vm)
{
	FklVMstack* vmStack=vm->stack;
	FklVMgc* gc=vm->gc;
	FklPtrStack nodeStack=FKL_STACK_INIT;
	fklInitPtrStack(&nodeStack,32,16);
	FklUintStack reftypeStack=FKL_STACK_INIT;
	fklInitUintStack(&reftypeStack,32,16);
	FklPtrStack valueStack=FKL_STACK_INIT;
	fklInitPtrStack(&valueStack,32,16);
	FklPtrStack stackStack=FKL_STACK_INIT;
	fklInitPtrStack(&stackStack,32,16);
	fklPushPtrStack((void*)node,&nodeStack);
	fklPushPtrStack(&valueStack,&stackStack);
	while(!fklIsPtrStackEmpty(&nodeStack))
	{
		FklNastNode* root=fklPopPtrStack(&nodeStack);
		FklPtrStack* cStack=fklTopPtrStack(&stackStack);
		if(root==SENTINEL_NAST_NODE)
		{
			fklPopPtrStack(&stackStack);
			FklPtrStack* tStack=fklTopPtrStack(&stackStack);
			FklValueType type=fklPopUintStack(&reftypeStack);
			uint64_t line=fklPopUintStack(&reftypeStack);
			FklVMvalue* v=fklCreateSaveVMvalue(type,NULL);
			fklPushPtrStack(v,tStack);
			if(lineHash)
				fklPutReplHashItem(createLineNumHashItem(v,NULL,line),lineHash);
			switch(type)
			{
				case FKL_TYPE_BOX:
					v->u.box=fklPopPtrStack(cStack);
					break;
				case FKL_TYPE_PAIR:
					{
						FklVMpair* pair=fklCreateVMpair();
						size_t top=cStack->top;
						pair->car=cStack->base[top-1];
						pair->cdr=cStack->base[top-2];
						v->u.pair=pair;
					}
					break;
				case FKL_TYPE_VECTOR:
					{
						v->u.vec=fklCreateVMvecNoInit(cStack->top);
						for(size_t i=cStack->top,j=0;i>0;i--,j++)
							v->u.vec->base[j]=cStack->base[i-1];
					}
					break;
				case FKL_TYPE_HASHTABLE:
					{
						FklVMhashTable* hash=fklCreateVMhashTable(fklPopUintStack(&reftypeStack));
						v->u.hash=hash;
						for(size_t i=cStack->top;i>0;i-=2)
						{
							FklVMvalue* key=cStack->base[i-1];
							FklVMvalue* v=cStack->base[i-2];
							fklSetVMhashTable(key,v,hash,gc);
						}
					}
					break;
				default:
					FKL_ASSERT(0);
					break;
			}
			fklAddToGCNoGC(v,vm->gc);
			fklPushVMvalue(v,vmStack);
			fklDestroyPtrStack(cStack);
			cStack=tStack;
		}
		else
		{
			switch(root->type)
			{
				case FKL_NAST_NIL:
					fklPushPtrStack(FKL_VM_NIL,cStack);
					break;
				case FKL_NAST_FIX:
					fklPushPtrStack(FKL_MAKE_VM_FIX(root->u.fix),cStack);
					break;
				case FKL_NAST_CHR:
					fklPushPtrStack(FKL_MAKE_VM_CHR(root->u.chr),cStack);
					break;
				case FKL_NAST_SYM:
					fklPushPtrStack(FKL_MAKE_VM_SYM(root->u.sym),cStack);
					break;
				case FKL_NAST_F64:
					fklPushPtrStack(fklCreateVMvalueNoGCAndToStack(FKL_TYPE_F64,&root->u.f64,gc,vmStack),cStack);
					break;
				case FKL_NAST_STR:
					fklPushPtrStack(fklCreateVMvalueNoGCAndToStack(FKL_TYPE_STR
								,fklCreateString(root->u.str->size,root->u.str->str)
								,gc
								,vmStack),cStack);
					break;
				case FKL_NAST_BYTEVECTOR:
					fklPushPtrStack(fklCreateVMvalueNoGCAndToStack(FKL_TYPE_BYTEVECTOR
								,fklCreateBytevector(root->u.bvec->size,root->u.bvec->ptr)
								,gc
								,vmStack),cStack);
					break;
				case FKL_NAST_BIG_INT:
					fklPushPtrStack(fklCreateVMvalueNoGCAndToStack(FKL_TYPE_BIG_INT,fklCopyBigInt(root->u.bigInt),gc,vmStack),cStack);
					break;
				case FKL_NAST_BOX:
					{
						FklPtrStack* bStack=fklCreatePtrStack(1,16);
						fklPushPtrStack(bStack,&stackStack);
						cStack=bStack;
						fklPushUintStack(root->curline,&reftypeStack);
						fklPushUintStack(FKL_TYPE_BOX,&reftypeStack);
						fklPushPtrStack(SENTINEL_NAST_NODE,&nodeStack);
						fklPushPtrStack(root->u.box,&nodeStack);
					}
					break;
				case FKL_NAST_VECTOR:
					{
						fklPushUintStack(root->curline,&reftypeStack);
						fklPushUintStack(FKL_TYPE_VECTOR,&reftypeStack);
						fklPushPtrStack(SENTINEL_NAST_NODE,&nodeStack);
						for(size_t i=0;i<root->u.vec->size;i++)
							fklPushPtrStack(root->u.vec->base[i],&nodeStack);
						FklPtrStack* vStack=fklCreatePtrStack(root->u.vec->size,16);
						fklPushPtrStack(vStack,&stackStack);
						cStack=vStack;
					}
					break;
				case FKL_NAST_HASHTABLE:
					{
						fklPushUintStack(root->u.hash->type,&reftypeStack);
						fklPushUintStack(root->curline,&reftypeStack);
						fklPushUintStack(FKL_TYPE_HASHTABLE,&reftypeStack);
						fklPushPtrStack(SENTINEL_NAST_NODE,&nodeStack);
						size_t num=root->u.hash->num;
						FklNastHashTable* hash=root->u.hash;
						for(size_t i=0;i<num;i++)
						{
							fklPushPtrStack(hash->items[i].car,&nodeStack);
							fklPushPtrStack(hash->items[i].cdr,&nodeStack);
						}
						FklPtrStack* hStack=fklCreatePtrStack(32,16);
						fklPushPtrStack(hStack,&stackStack);
						cStack=hStack;
					}
					break;
				case FKL_NAST_PAIR:
					{
						FklPtrStack* pStack=fklCreatePtrStack(2,16);
						fklPushPtrStack(pStack,&stackStack);
						cStack=pStack;
						fklPushUintStack(root->curline,&reftypeStack);
						fklPushUintStack(FKL_TYPE_PAIR,&reftypeStack);
						fklPushPtrStack(SENTINEL_NAST_NODE,&nodeStack);
						fklPushPtrStack(root->u.pair->car,&nodeStack);
						fklPushPtrStack(root->u.pair->cdr,&nodeStack);
					}
					break;
				default:
					FKL_ASSERT(0);
					break;
			}
		}
	}
	FklVMvalue* retval=fklTopPtrStack(&valueStack);
	fklUninitPtrStack(&stackStack);
	fklUninitPtrStack(&nodeStack);
	fklUninitPtrStack(&valueStack);
	fklUninitUintStack(&reftypeStack);
	return retval;
}

FklVMvalue* fklCreateVMvalueFromNastNodeNoGC(const FklNastNode* node
		,FklHashTable* lineHash
		,FklVMgc* gc)
{
	FklPtrStack nodeStack=FKL_STACK_INIT;
	fklInitPtrStack(&nodeStack,32,16);
	FklUintStack reftypeStack=FKL_STACK_INIT;
	fklInitUintStack(&reftypeStack,32,16);
	FklPtrStack valueStack=FKL_STACK_INIT;
	fklInitPtrStack(&valueStack,32,16);
	FklPtrStack stackStack=FKL_STACK_INIT;
	fklInitPtrStack(&stackStack,32,16);
	fklPushPtrStack((void*)node,&nodeStack);
	fklPushPtrStack(&valueStack,&stackStack);
	while(!fklIsPtrStackEmpty(&nodeStack))
	{
		FklNastNode* root=fklPopPtrStack(&nodeStack);
		FklPtrStack* cStack=fklTopPtrStack(&stackStack);
		if(root==SENTINEL_NAST_NODE)
		{
			fklPopPtrStack(&stackStack);
			FklPtrStack* tStack=fklTopPtrStack(&stackStack);
			FklValueType type=fklPopUintStack(&reftypeStack);
			uint64_t line=fklPopUintStack(&reftypeStack);
			FklVMvalue* v=fklCreateSaveVMvalue(type,NULL);
			fklPushPtrStack(v,tStack);
			if(lineHash)
				fklPutReplHashItem(createLineNumHashItem(v,NULL,line),lineHash);
			switch(type)
			{
				case FKL_TYPE_BOX:
					v->u.box=fklPopPtrStack(cStack);
					break;
				case FKL_TYPE_PAIR:
					{
						FklVMpair* pair=fklCreateVMpair();
						size_t top=cStack->top;
						pair->car=cStack->base[top-1];
						pair->cdr=cStack->base[top-2];
						v->u.pair=pair;
					}
					break;
				case FKL_TYPE_VECTOR:
					{
						v->u.vec=fklCreateVMvecNoInit(cStack->top);
						for(size_t i=cStack->top,j=0;i>0;i--,j++)
							v->u.vec->base[j]=cStack->base[i-1];
					}
					break;
				case FKL_TYPE_HASHTABLE:
					{
						FklVMhashTable* hash=fklCreateVMhashTable(fklPopUintStack(&reftypeStack));
						v->u.hash=hash;
						for(size_t i=cStack->top;i>0;i-=2)
						{
							FklVMvalue* key=cStack->base[i-1];
							FklVMvalue* v=cStack->base[i-2];
							fklSetVMhashTable(key,v,hash,gc);
						}
					}
					break;
				default:
					FKL_ASSERT(0);
					break;
			}
			fklAddToGCNoGC(v,gc);
			fklDestroyPtrStack(cStack);
			cStack=tStack;
		}
		else
		{
			switch(root->type)
			{
				case FKL_NAST_NIL:
					fklPushPtrStack(FKL_VM_NIL,cStack);
					break;
				case FKL_NAST_FIX:
					fklPushPtrStack(FKL_MAKE_VM_FIX(root->u.fix),cStack);
					break;
				case FKL_NAST_CHR:
					fklPushPtrStack(FKL_MAKE_VM_CHR(root->u.chr),cStack);
					break;
				case FKL_NAST_SYM:
					fklPushPtrStack(FKL_MAKE_VM_SYM(root->u.sym),cStack);
					break;
				case FKL_NAST_F64:
					fklPushPtrStack(fklCreateVMvalueNoGC(FKL_TYPE_F64,&root->u.f64,gc),cStack);
					break;
				case FKL_NAST_STR:
					fklPushPtrStack(fklCreateVMvalueNoGC(FKL_TYPE_STR
								,fklCreateString(root->u.str->size,root->u.str->str)
								,gc)
							,cStack);
					break;
				case FKL_NAST_BYTEVECTOR:
					fklPushPtrStack(fklCreateVMvalueNoGC(FKL_TYPE_BYTEVECTOR
								,fklCreateBytevector(root->u.bvec->size,root->u.bvec->ptr)
								,gc),cStack);
					break;
				case FKL_NAST_BIG_INT:
					fklPushPtrStack(fklCreateVMvalueNoGC(FKL_TYPE_BIG_INT,fklCopyBigInt(root->u.bigInt),gc),cStack);
					break;
				case FKL_NAST_BOX:
					{
						FklPtrStack* bStack=fklCreatePtrStack(1,16);
						fklPushPtrStack(bStack,&stackStack);
						cStack=bStack;
						fklPushUintStack(root->curline,&reftypeStack);
						fklPushUintStack(FKL_TYPE_BOX,&reftypeStack);
						fklPushPtrStack(SENTINEL_NAST_NODE,&nodeStack);
						fklPushPtrStack(root->u.box,&nodeStack);
					}
					break;
				case FKL_NAST_VECTOR:
					{
						fklPushUintStack(root->curline,&reftypeStack);
						fklPushUintStack(FKL_TYPE_VECTOR,&reftypeStack);
						fklPushPtrStack(SENTINEL_NAST_NODE,&nodeStack);
						for(size_t i=0;i<root->u.vec->size;i++)
							fklPushPtrStack(root->u.vec->base[i],&nodeStack);
						FklPtrStack* vStack=fklCreatePtrStack(root->u.vec->size,16);
						fklPushPtrStack(vStack,&stackStack);
						cStack=vStack;
					}
					break;
				case FKL_NAST_HASHTABLE:
					{
						fklPushUintStack(root->u.hash->type,&reftypeStack);
						fklPushUintStack(root->curline,&reftypeStack);
						fklPushUintStack(FKL_TYPE_HASHTABLE,&reftypeStack);
						fklPushPtrStack(SENTINEL_NAST_NODE,&nodeStack);
						size_t num=root->u.hash->num;
						FklNastHashTable* hash=root->u.hash;
						for(size_t i=0;i<num;i++)
						{
							fklPushPtrStack(hash->items[i].car,&nodeStack);
							fklPushPtrStack(hash->items[i].cdr,&nodeStack);
						}
						FklPtrStack* hStack=fklCreatePtrStack(32,16);
						fklPushPtrStack(hStack,&stackStack);
						cStack=hStack;
					}
					break;
				case FKL_NAST_PAIR:
					{
						FklPtrStack* pStack=fklCreatePtrStack(2,16);
						fklPushPtrStack(pStack,&stackStack);
						cStack=pStack;
						fklPushUintStack(root->curline,&reftypeStack);
						fklPushUintStack(FKL_TYPE_PAIR,&reftypeStack);
						fklPushPtrStack(SENTINEL_NAST_NODE,&nodeStack);
						fklPushPtrStack(root->u.pair->car,&nodeStack);
						fklPushPtrStack(root->u.pair->cdr,&nodeStack);
					}
					break;
				default:
					FKL_ASSERT(0);
					break;
			}
		}
	}
	FklVMvalue* retval=fklTopPtrStack(&valueStack);
	fklUninitPtrStack(&stackStack);
	fklUninitPtrStack(&nodeStack);
	fklUninitPtrStack(&valueStack);
	fklUninitUintStack(&reftypeStack);
	return retval;
}

FklNastNode* fklCreateNastNodeFromVMvalue(FklVMvalue* v
		,uint64_t curline
		,FklHashTable* lineHash
		,FklSymbolTable* table)
{
	FklHashTable* recValueSet=fklCreateValueSetHashtable();
	FklNastNode* retval=NULL;
	fklScanCirRef(v,recValueSet);
	if(!recValueSet->num)
	{
		FklPtrStack s0=FKL_STACK_INIT;
		fklInitPtrStack(&s0,32,16);
		FklPtrStack s1=FKL_STACK_INIT;
		fklInitPtrStack(&s1,32,16);
		FklUintStack lineStack=FKL_STACK_INIT;
		fklInitUintStack(&lineStack,32,16);
		fklPushPtrStack(v,&s0);
		fklPushPtrStack(&retval,&s1);
		fklPushUintStack(curline,&lineStack);
		while(!fklIsPtrStackEmpty(&s0))
		{
			FklVMvalue* value=fklPopPtrStack(&s0);
			FklNastNode** pcur=fklPopPtrStack(&s1);
			LineNumHashItem* item=fklGetHashItem(&value,lineHash);
			uint64_t sline=fklPopUintStack(&lineStack);
			uint64_t line=item?item->line:sline;
			FklNastNode* cur=fklCreateNastNode(FKL_NAST_NIL,line);
			*pcur=cur;
			switch(FKL_GET_TAG(value))
			{
				case FKL_TAG_NIL:
					cur->u.str=NULL;
					break;
				case FKL_TAG_SYM:
					cur->type=FKL_NAST_SYM;
					cur->u.sym=FKL_GET_SYM(value);
					break;
				case FKL_TAG_CHR:
					cur->type=FKL_NAST_CHR;
					cur->u.chr=FKL_GET_CHR(value);
					break;
				case FKL_TAG_FIX:
					cur->type=FKL_NAST_FIX;
					cur->u.fix=FKL_GET_FIX(value);
					break;
				case FKL_TAG_PTR:
					{
						switch(value->type)
						{
							case FKL_TYPE_F64:
								cur->type=FKL_NAST_F64;
								cur->u.f64=value->u.f64;
								break;
							case FKL_TYPE_STR:
								cur->type=FKL_NAST_STR;
								cur->u.str=fklCopyString(value->u.str);
								break;
							case FKL_TYPE_BYTEVECTOR:
								cur->type=FKL_NAST_BYTEVECTOR;
								cur->u.bvec=fklCopyBytevector(value->u.bvec);
								break;
							case FKL_TYPE_BIG_INT:
								cur->type=FKL_NAST_BIG_INT;
								cur->u.bigInt=fklCopyBigInt(value->u.bigInt);
								break;
							case FKL_TYPE_PROC:
								cur->type=FKL_NAST_SYM;
								cur->u.sym=fklAddSymbolCstr("#<proc>",table)->id;
								break;
							case FKL_TYPE_DLPROC:
								cur->type=FKL_NAST_SYM;
								cur->u.sym=fklAddSymbolCstr("#<dlproc>",table)->id;
								break;
							case FKL_TYPE_CHAN:
								cur->type=FKL_NAST_SYM;
								cur->u.sym=fklAddSymbolCstr("#<chan>",table)->id;
								break;
							case FKL_TYPE_FP:
								cur->type=FKL_NAST_SYM;
								cur->u.sym=fklAddSymbolCstr("#<fp>",table)->id;
								break;
							case FKL_TYPE_ERR:
								cur->type=FKL_NAST_SYM;
								cur->u.sym=fklAddSymbolCstr("#<err>",table)->id;
								break;
							case FKL_TYPE_CODE_OBJ:
								cur->type=FKL_NAST_SYM;
								cur->u.sym=fklAddSymbolCstr("#<code-obj>",table)->id;
								break;
							case FKL_TYPE_USERDATA:
								cur->type=FKL_NAST_SYM;
								cur->u.sym=fklAddSymbolCstr("#<userdata>",table)->id;
								break;
							case FKL_TYPE_DLL:
								cur->type=FKL_NAST_SYM;
								cur->u.sym=fklAddSymbolCstr("#<dll>",table)->id;
								break;
							case FKL_TYPE_BOX:
								cur->type=FKL_NAST_BOX;
								fklPushPtrStack(value->u.box,&s0);
								fklPushPtrStack(&cur->u.box,&s1);
								fklPushUintStack(cur->curline,&lineStack);
								break;
							case FKL_TYPE_PAIR:
								cur->type=FKL_NAST_PAIR;
								cur->u.pair=fklCreateNastPair();
								fklPushPtrStack(value->u.pair->car,&s0);
								fklPushPtrStack(value->u.pair->cdr,&s0);
								fklPushPtrStack(&cur->u.pair->car,&s1);
								fklPushPtrStack(&cur->u.pair->cdr,&s1);
								fklPushUintStack(cur->curline,&lineStack);
								fklPushUintStack(cur->curline,&lineStack);
								break;
							case FKL_TYPE_VECTOR:
								cur->type=FKL_NAST_VECTOR;
								cur->u.vec=fklCreateNastVector(value->u.vec->size);
								for(size_t i=0;i<value->u.vec->size;i++)
									fklPushPtrStack(value->u.vec->base[i],&s0);
								for(size_t i=0;i<cur->u.vec->size;i++)
								{
									fklPushPtrStack(&cur->u.vec->base[i],&s1);
									fklPushUintStack(cur->curline,&lineStack);
								}
								break;
							case FKL_TYPE_HASHTABLE:
								cur->type=FKL_NAST_HASHTABLE;
								cur->u.hash=fklCreateNastHash(value->u.hash->type,value->u.hash->ht->num);
								for(FklHashTableNodeList* list=value->u.hash->ht->list;list;list=list->next)
								{
									FklVMhashTableItem* item=list->node->item;
									fklPushPtrStack(item->key,&s0);
									fklPushPtrStack(item->v,&s0);
								}
								for(size_t i=0;i<cur->u.hash->num;i++)
								{
									fklPushPtrStack(&cur->u.hash->items[i].car,&s1);
									fklPushPtrStack(&cur->u.hash->items[i].cdr,&s1);
									fklPushUintStack(cur->curline,&lineStack);
									fklPushUintStack(cur->curline,&lineStack);
								}
								break;
						}
					}
					break;
			}
		}
		fklUninitPtrStack(&s0);
		fklUninitPtrStack(&s1);
		fklUninitUintStack(&lineStack);
	}
	fklDestroyHashTable(recValueSet);
	return retval;
}

#undef SENTINEL_NAST_NODE

FklVMproc* fklCreateVMprocWithWholeCodeObj(FklVMvalue* codeObj,FklVMgc* gc)
{
	FklByteCode* bc=codeObj->u.code->bc;
	FklVMproc* tmp=(FklVMproc*)malloc(sizeof(FklVMproc));
	FKL_ASSERT(tmp);
	//tmp->prevEnv=FKL_VM_NIL;
	tmp->spc=bc->code;
	tmp->end=bc->code+bc->size;
	tmp->sid=0;
	tmp->closure=NULL;
	tmp->count=0;
	tmp->by=NULL;
	tmp->protoId=1;
	tmp->unresolveRef=0;
	fklSetRef(&tmp->codeObj,codeObj,gc);
	return tmp;
}

FklVMproc* fklCreateVMproc(uint8_t* spc,uint64_t cpc,FklVMvalue* codeObj,FklVMgc* gc)
{
	FklVMproc* tmp=(FklVMproc*)malloc(sizeof(FklVMproc));
	FKL_ASSERT(tmp);
	//tmp->prevEnv=FKL_VM_NIL;
	tmp->spc=spc;
	tmp->end=spc+cpc;
	tmp->sid=0;
	tmp->closure=NULL;
	tmp->count=0;
	tmp->protoId=0;
	tmp->by=NULL;
	tmp->unresolveRef=0;
	fklSetRef(&tmp->codeObj,codeObj,gc);
	return tmp;
}

FklVMvalue* fklCopyVMlistOrAtom(FklVMvalue* obj,FklVM* vm)
{
	FklPtrStack s1=FKL_STACK_INIT;
	fklInitPtrStack(&s1,32,16);
	FklPtrStack s2=FKL_STACK_INIT;
	fklInitPtrStack(&s2,32,16);
	FklVMvalue* tmp=FKL_VM_NIL;
	fklPushPtrStack(obj,&s1);
	fklPushPtrStack(&tmp,&s2);
	while(!fklIsPtrStackEmpty(&s1))
	{
		FklVMvalue* root=fklPopPtrStack(&s1);
		FklVMptrTag tag=FKL_GET_TAG(root);
		FklVMvalue** root1=fklPopPtrStack(&s2);
		switch(tag)
		{
			case FKL_TAG_NIL:
			case FKL_TAG_FIX:
			case FKL_TAG_SYM:
			case FKL_TAG_CHR:
				*root1=root;
				break;
			case FKL_TAG_PTR:
				{
					FklValueType type=root->type;
					switch(type)
					{
						case FKL_TYPE_PAIR:
							*root1=fklCreateVMvalueToStack(FKL_TYPE_PAIR,fklCreateVMpair(),vm);
							fklPushPtrStack(&(*root1)->u.pair->car,&s2);
							fklPushPtrStack(&(*root1)->u.pair->cdr,&s2);
							fklPushPtrStack(root->u.pair->car,&s1);
							fklPushPtrStack(root->u.pair->cdr,&s1);
							break;
						default:
							*root1=root;
							break;
					}
					*root1=FKL_MAKE_VM_PTR(*root1);
				}
				break;
			default:
				return NULL;
				break;
		}
	}
	fklUninitPtrStack(&s1);
	fklUninitPtrStack(&s2);
	return tmp;
}

static FklVMvalue* __fkl_f64_copyer(FklVMvalue* obj,FklVM* vm)
{
	FklVMvalue* tmp=fklCreateVMvalueToStack(FKL_TYPE_F64,NULL,vm);
	tmp->u.f64=obj->u.f64;
	return tmp;
}

static FklVMvalue* __fkl_bigint_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,fklCopyBigInt(obj->u.bigInt),vm);
}

static FklVMvalue* __fkl_vector_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCreateVMvecV(obj->u.vec->size,obj->u.vec->base,vm);
}

static FklVMvalue* __fkl_str_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCreateVMvalueToStack(FKL_TYPE_STR,fklCopyString(obj->u.str),vm); }

static FklVMvalue* __fkl_bytevector_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCopyBytevector(obj->u.bvec),vm);
}

static FklVMvalue* __fkl_pair_copyer(FklVMvalue* obj,FklVM* vm)
{
//	return fklCreateVMpairV(obj->u.pair->car,obj->u.pair->cdr,s,gc);
	return fklCopyVMlistOrAtom(obj,vm);
}

static FklVMvalue* __fkl_box_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCreateVMvalueToStack(FKL_TYPE_BOX,obj->u.box,vm);
}

static FklVMvalue* __fkl_userdata_copyer(FklVMvalue* obj,FklVM* vm)
{
	if(obj->u.ud->t->__copy==NULL)
		return NULL;
	else
		return fklCreateVMvalueToStack(FKL_TYPE_USERDATA
				,fklCreateVMudata(obj->u.ud->type
					,obj->u.ud->t
					,obj->u.ud->t->__copy(obj->u.ud->data)
					,obj->u.ud->rel
					,obj->u.ud->pd)
				,vm);
}

static FklVMvalue* __fkl_hashtable_copyer(FklVMvalue* obj,FklVM* vm)
{
	FklVMhashTable* ht=obj->u.hash;
	FklVMhashTable* nht=fklCreateVMhashTable(obj->u.hash->type);
	for(FklHashTableNodeList* list=ht->ht->list;list;list=list->next)
	{
		FklVMhashTableItem* item=list->node->item;
		fklSetVMhashTable(item->key,item->v,nht,vm->gc);
	}
	return fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE,nht,vm);
}

static FklVMvalue* (*const valueCopyers[FKL_TYPE_CODE_OBJ+1])(FklVMvalue* obj,FklVM* vm)=
{
	__fkl_f64_copyer,
	__fkl_bigint_copyer,
	__fkl_str_copyer,
	__fkl_vector_copyer,
	__fkl_pair_copyer,
	__fkl_box_copyer,
	__fkl_bytevector_copyer,
	__fkl_userdata_copyer,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	__fkl_hashtable_copyer,
	NULL,
};

FklVMvalue* fklCopyVMvalue(FklVMvalue* obj,FklVM* vm)
{
	FklVMvalue* tmp=FKL_VM_NIL;
	FklVMptrTag tag=FKL_GET_TAG(obj);
	switch(tag)
	{
		case FKL_TAG_NIL:
		case FKL_TAG_FIX:
		case FKL_TAG_SYM:
		case FKL_TAG_CHR:
			tmp=obj;
			break;
		case FKL_TAG_PTR:
			{
				FklValueType type=obj->type;
				FKL_ASSERT(type<=FKL_TYPE_HASHTABLE);
				FklVMvalue* (*valueCopyer)(FklVMvalue* obj,FklVM* vm)=valueCopyers[type];
				if(!valueCopyer)
					return NULL;
				else
					return valueCopyer(obj,vm);
			}
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
	return tmp;
}

static inline double getF64FromByteCode(uint8_t* code)
{
	double i=0;
	((uint8_t*)&i)[0]=code[0];
	((uint8_t*)&i)[1]=code[1];
	((uint8_t*)&i)[2]=code[2];
	((uint8_t*)&i)[3]=code[3];
	((uint8_t*)&i)[4]=code[4];
	((uint8_t*)&i)[5]=code[5];
	((uint8_t*)&i)[6]=code[6];
	((uint8_t*)&i)[7]=code[7];
	return i;
}

//static inline int64_t getI64FromByteCode(uint8_t* code)
//{
//	int64_t i=0;
//	((uint8_t*)&i)[0]=code[0];
//	((uint8_t*)&i)[1]=code[1];
//	((uint8_t*)&i)[2]=code[2];
//	((uint8_t*)&i)[3]=code[3];
//	((uint8_t*)&i)[4]=code[4];
//	((uint8_t*)&i)[5]=code[5];
//	((uint8_t*)&i)[6]=code[6];
//	((uint8_t*)&i)[7]=code[7];
//	return i;
//}

FklVMvalue* fklCreateVMvalue(FklValueType type,void* pValue,FklVM* vm)
{
	FklVMvalue* r=fklCreateSaveVMvalue(type,pValue);
	fklAddToGC(r,vm);
	return r;
}

FklVMvalue* fklCreateVMvalueToStack(FklValueType type
		,void* pValue
		,FklVM* vm)
{
	FklVMstack* stack=vm->stack;
	FklVMvalue* r=fklCreateSaveVMvalue(type,pValue);
	//	pthread_rwlock_wrlock(&stack->lock);
	fklPushVMvalue(r,stack);
	//	pthread_rwlock_unlock(&stack->lock);
	fklAddToGC(r,vm);
	return stack->values[stack->tp-1];
}

FklVMvalue* fklCreateSaveVMvalue(FklValueType type,void* pValue)
{
	FklVMvalue* tmp=(FklVMvalue*)malloc(sizeof(FklVMvalue));
	FKL_ASSERT(tmp);
	tmp->type=type;
	tmp->mark=FKL_MARK_W;
	switch(type)
	{
		case FKL_TYPE_F64:
			if(pValue)
				tmp->u.f64=getF64FromByteCode(pValue);
			break;
			//case FKL_TYPE_I64:
			//	if(pValue)
			//		tmp->u.i64=getI64FromByteCode(pValue);
			//	break;
		case FKL_TYPE_BYTEVECTOR:
		case FKL_TYPE_STR:
		case FKL_TYPE_PAIR:
		case FKL_TYPE_PROC:
		case FKL_TYPE_CHAN:
		case FKL_TYPE_FP:
		case FKL_TYPE_DLL:
		case FKL_TYPE_DLPROC:
		case FKL_TYPE_ERR:
		case FKL_TYPE_VECTOR:
		case FKL_TYPE_USERDATA:
		case FKL_TYPE_BIG_INT:
		case FKL_TYPE_BOX:
		case FKL_TYPE_HASHTABLE:
		case FKL_TYPE_CODE_OBJ:
			tmp->u.box=pValue;
			break;
		default:
			return NULL;
			break;
	}
	return FKL_MAKE_VM_PTR(tmp);
		//switch(type)
		//{
		//	//case FKL_TYPE_NIL:
		//	//	return FKL_VM_NIL;
		//	//	break;
		//	//case FKL_TYPE_CHR:
		//	//	return FKL_MAKE_VM_CHR(pValue);
		//	//	break;
		//	//case FKL_TYPE_I32:
		//	//	return FKL_MAKE_VM_I32(pValue);
		//	//	break;
		//	//case FKL_TYPE_SYM:
		//	//	return FKL_MAKE_VM_SYM(pValue);
		//	//	break;
		//	default:
		//		{
		//			FklVMvalue* tmp=(FklVMvalue*)malloc(sizeof(FklVMvalue));
		//			FKL_ASSERT(tmp);
		//			tmp->type=type;
		//			tmp->mark=FKL_MARK_W;
		//			switch(type)
		//			{
		//				case FKL_TYPE_F64:
		//					if(pValue)
		//						tmp->u.f64=getF64FromByteCode(pValue);
		//					break;
		//					//case FKL_TYPE_I64:
		//					//	if(pValue)
		//					//		tmp->u.i64=getI64FromByteCode(pValue);
		//					//	break;
		//				case FKL_TYPE_BYTEVECTOR:
		//				case FKL_TYPE_STR:
		//				case FKL_TYPE_PAIR:
		//				case FKL_TYPE_PROC:
		//				case FKL_TYPE_CHAN:
		//				case FKL_TYPE_FP:
		//				case FKL_TYPE_DLL:
		//				case FKL_TYPE_DLPROC:
		//				case FKL_TYPE_ERR:
		//				case FKL_TYPE_VECTOR:
		//				case FKL_TYPE_USERDATA:
		//				case FKL_TYPE_ENV:
		//				case FKL_TYPE_BIG_INT:
		//				case FKL_TYPE_BOX:
		//				case FKL_TYPE_HASHTABLE:
		//				case FKL_TYPE_CODE_OBJ:
		//					tmp->u.box=pValue;
		//					break;
		//				default:
		//					return NULL;
		//					break;
		//			}
		//			return FKL_MAKE_VM_PTR(tmp);
		//		}
		//		break;
		//}
}

void fklAddToGCNoGC(FklVMvalue* v,FklVMgc* gc)
{
	if(FKL_IS_PTR(v))
	{
		FklGCstate running=fklGetGCstate(gc);
		if(running>FKL_GC_NONE&&running<FKL_GC_SWEEPING)
			fklGC_toGrey(v,gc);
		v->mark=FKL_MARK_W;
		v->next=gc->head;
		gc->head=v;
		gc->num+=1;
	}
}

static void tryGC(FklVM* vm)
{
	FklVMgc* gc=vm->gc;
	if(gc->num>gc->threshold)
		fklGC_threadFunc(vm);
}

FklVMvalue* fklCreateVMvalueNoGC(FklValueType type,void* pValue,FklVMgc* gc)
{
	FklVMvalue* r=fklCreateSaveVMvalue(type,pValue);
	fklAddToGCNoGC(r,gc);
	return r;
}

FklVMvalue* fklCreateVMvalueNoGCAndToStack(FklValueType type,void* pValue,FklVMgc* gc,FklVMstack* s)
{
	FklVMvalue* r=fklCreateSaveVMvalue(type,pValue);
	fklPushVMvalue(r,s);
	fklAddToGCNoGC(r,gc);
	return r;
}

//void fklAddToGCBeforeGC(FklVMvalue* v,FklVM* vm)
//{
//	FklVMgc* gc=vm->gc;
//	if(FKL_IS_PTR(v))
//	{
//		FklGCstate running=fklGetGCstate(gc);
//		if(running>FKL_GC_NONE&&running<FKL_GC_SWEEPING)
//			fklGC_toGrey(v,gc);
//		pthread_rwlock_wrlock(&gc->lock);
//		gc->num+=1;
//		v->next=gc->head;
//		gc->head=v;
//		tryGC(vm);
//		v->mark=FKL_MARK_W;
//		pthread_rwlock_unlock(&gc->lock);
//	}
//}

void fklAddToGC(FklVMvalue* v,FklVM* vm)
{
	FklVMgc* gc=vm->gc;
	if(FKL_IS_PTR(v))
	{
		FklGCstate running=fklGetGCstate(gc);
		if(running>FKL_GC_NONE&&running<FKL_GC_SWEEPING)
			fklGC_toGrey(v,gc);
		gc->num+=1;
		tryGC(vm);
		v->next=gc->head;
		v->mark=FKL_MARK_W;
		gc->head=v;
	}
}

inline FklVMvalue* fklCreateTrueValue()
{
	return FKL_MAKE_VM_FIX(1);
}

inline FklVMvalue* fklCreateNilValue()
{
	return FKL_VM_NIL;
}

FklVMvalue* fklGetVMpairCar(FklVMvalue* obj)
{
	return obj->u.pair->car;
}

FklVMvalue* fklGetVMpairCdr(FklVMvalue* obj)
{
	return obj->u.pair->cdr;
}

int fklVMvalueEqv(const FklVMvalue* fir,const FklVMvalue* sec)
{
	if(fklIsInt(fir)&&fklIsInt(sec))
	{
		if(FKL_IS_BIG_INT(fir)&&FKL_IS_BIG_INT(sec))
			return fklCmpBigInt(fir->u.bigInt,sec->u.bigInt)==0;
		else if(FKL_IS_BIG_INT(fir))
			return fklCmpBigIntI(fir->u.bigInt,FKL_GET_FIX(sec))==0;
		else if(FKL_IS_BIG_INT(fir))
			return fklCmpBigIntI(fir->u.bigInt,FKL_GET_FIX(sec))==0;
		else
			return FKL_GET_FIX(fir)==FKL_GET_FIX(sec);
	}
	else
		return fir==sec;
}

inline int fklVMvalueEq(const FklVMvalue* fir,const FklVMvalue* sec)
{
	return fir==sec;
}

int fklVMvalueEqual(const FklVMvalue* fir,const FklVMvalue* sec)
{
	FklPtrStack s1=FKL_STACK_INIT;
	fklInitPtrStack(&s1,32,16);
	FklPtrStack s2=FKL_STACK_INIT;
	fklInitPtrStack(&s2,32,16);
	fklPushPtrStack((void*)fir,&s1);
	fklPushPtrStack((void*)sec,&s2);
	int r=1;
	while(!fklIsPtrStackEmpty(&s1))
	{
		FklVMvalue* root1=fklPopPtrStack(&s1);
		FklVMvalue* root2=fklPopPtrStack(&s2);
		if(fklIsInt(root1)&&fklIsInt(root2))
			r=fklVMvalueEqv(root1,root2);
		else if(!FKL_IS_PTR(root1)&&!FKL_IS_PTR(root2)&&root1!=root2)
			r=0;
		else if(FKL_GET_TAG(root1)!=FKL_GET_TAG(root2))
			r=0;
		else if(FKL_IS_PTR(root1)&&FKL_IS_PTR(root2))
		{
			if(root1->type!=root2->type)
				r=0;
			switch(root1->type)
			{
				case FKL_TYPE_F64:
					r=root1->u.f64==root2->u.f64;
					break;
				case FKL_TYPE_STR:
					r=!fklStringcmp(root1->u.str,root2->u.str);
					break;
				case FKL_TYPE_BYTEVECTOR:
					r=!fklBytevectorcmp(root1->u.bvec,root2->u.bvec);
					break;
				case FKL_TYPE_PAIR:
					r=1;
					fklPushPtrStack(root1->u.pair->car,&s1);
					fklPushPtrStack(root1->u.pair->cdr,&s1);
					fklPushPtrStack(root2->u.pair->car,&s2);
					fklPushPtrStack(root2->u.pair->cdr,&s2);
					break;
				case FKL_TYPE_BOX:
					r=1;
					fklPushPtrStack(root1->u.box,&s1);
					fklPushPtrStack(root2->u.box,&s2);
					break;
				case FKL_TYPE_VECTOR:
					if(root1->u.vec->size!=root2->u.vec->size)
						r=0;
					else
					{
						r=1;
						size_t size=root1->u.vec->size;
						for(size_t i=0;i<size;i++)
							fklPushPtrStack(root1->u.vec->base[i],&s1);
						for(size_t i=0;i<size;i++)
							fklPushPtrStack(root2->u.vec->base[i],&s2);
					}
					break;
				case FKL_TYPE_BIG_INT:
					r=!fklCmpBigInt(root1->u.bigInt,root2->u.bigInt);
					break;
				case FKL_TYPE_USERDATA:
					if(root1->u.ud->type!=root2->u.ud->type||!root1->u.ud->t->__equal)
						r=0;
					else
						r=root1->u.ud->t->__equal(root1->u.ud,root2->u.ud);
					break;
				case FKL_TYPE_HASHTABLE:
					r=1;
					for(FklHashTableNodeList* list=root1->u.hash->ht->list;list;list=list->next)
					{
						FklVMhashTableItem* item=list->node->item;
						fklPushPtrStack(item->key,&s1);
						fklPushPtrStack(item->v,&s1);
					}
					for(FklHashTableNodeList* list=root2->u.hash->ht->list;list;list=list->next)
					{
						FklVMhashTableItem* item=list->node->item;
						fklPushPtrStack(item->key,&s2);
						fklPushPtrStack(item->v,&s2);
					}
					break;
				default:
					r=(root1==root2);
					break;
			}
		}
		if(!r)
			break;
	}
	fklUninitPtrStack(&s1);
	fklUninitPtrStack(&s2);
	return r;
}

int fklNumcmp(FklVMvalue* fir,FklVMvalue* sec)
{
	if(FKL_GET_TAG(fir)==FKL_GET_TAG(sec)&&FKL_GET_TAG(fir)==FKL_TAG_FIX)
		return fir==sec;
	else
	{
		double first=(FKL_GET_TAG(fir)==FKL_TAG_FIX)?FKL_GET_FIX(fir):fir->u.f64;
		double second=(FKL_GET_TAG(sec)==FKL_TAG_FIX)?FKL_GET_FIX(sec):sec->u.f64;
		return fabs(first-second)==0;
	}
}

FklVMpair* fklCreateVMpair(void)
{
	FklVMpair* tmp=(FklVMpair*)malloc(sizeof(FklVMpair));
	FKL_ASSERT(tmp);
	tmp->car=FKL_VM_NIL;
	tmp->cdr=FKL_VM_NIL;
	return tmp;
}

FklVMvalue* fklCreateVMpairV(FklVMvalue* car,FklVMvalue* cdr,FklVM* vm)
{
	FklVMgc* gc=vm->gc;
	FklVMvalue* pair=fklCreateVMvalueToStack(FKL_TYPE_PAIR,fklCreateVMpair(),vm);
	fklSetRef(&pair->u.pair->car,car,gc);
	fklSetRef(&pair->u.pair->cdr,cdr,gc);
	return pair;
}

FklVMvalue* fklCreateVMvecV(size_t size,FklVMvalue** base,FklVM* vm)
{
	FklVMgc* gc=vm->gc;
	FklVMvalue* vec=fklCreateVMvalueToStack(FKL_TYPE_VECTOR,fklCreateVMvec(size),vm);
	if(base)
		for(size_t i=0;i<size;i++)
			fklSetRef(&vec->u.vec->base[i],base[i],gc);
	return vec;
}

FklVMchanl* fklCreateVMchanl(int32_t maxSize)
{
	FklVMchanl* tmp=(FklVMchanl*)malloc(sizeof(FklVMchanl));
	FKL_ASSERT(tmp);
	tmp->max=maxSize;
	tmp->messageNum=0;
	tmp->sendNum=0;
	tmp->recvNum=0;
	tmp->messages=fklCreatePtrQueue();
	tmp->sendq=fklCreatePtrQueue();
	tmp->recvq=fklCreatePtrQueue();
	return tmp;
}

//int32_t fklGetNumVMchanl(FklVMchanl* ch)
//{
//	pthread_rwlock_rdlock((pthread_rwlock_t*)&ch->lock);
//	uint32_t i=0;
//	i=fklLengthPtrQueue(ch->messages);
//	pthread_rwlock_unlock((pthread_rwlock_t*)&ch->lock);
//	return i;
//}

void fklDestroyVMchanl(FklVMchanl* ch)
{
	fklDestroyPtrQueue(ch->messages);
	FklQueueNode* head=ch->sendq->head;
	for(;head;head=head->next)
		fklDestroyVMsend(head->data);
	for(head=ch->recvq->head;head;head=head->next)
		fklDestroyVMrecv(head->data);
	fklDestroyPtrQueue(ch->sendq);
	fklDestroyPtrQueue(ch->recvq);
	free(ch);
}

void fklDestroyVMproc(FklVMproc* proc)
{
	free(proc->closure);
	free(proc);
}

FklVMfp* fklCreateVMfp(FILE* fp)
{
	FklVMfp* vfp=(FklVMfp*)malloc(sizeof(FklVMfp));
	FKL_ASSERT(vfp);
	vfp->fp=fp;
	vfp->size=0;
	vfp->prev=NULL;
	return vfp;
}

int fklDestroyVMfp(FklVMfp* vfp)
{
	int r=0;
	if(vfp)
	{
		if(vfp->size)
			free(vfp->prev);
		FILE* fp=vfp->fp;
		if(!(fp!=NULL&&fp!=stdin&&fp!=stdout&&fp!=stderr&&fclose(fp)!=EOF))
			r=1;
		free(vfp);
	}
	return r;
}

FklVMdll* fklCreateVMdll(const char* dllName)
{
	size_t len=strlen(dllName)+strlen(FKL_DLL_FILE_TYPE)+1;
	char* realDllName=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(realDllName);
	strcpy(realDllName,dllName);
	strcat(realDllName,FKL_DLL_FILE_TYPE);
	char* rpath=fklRealpath(realDllName);
	if(!rpath)
	{
		free(realDllName);
		return NULL;
	}
	FklDllHandle handle=fklLoadDll(rpath);
	if(!handle)
	{
		free(rpath);
		free(realDllName);
		return NULL;
	}
	free(realDllName);
	free(rpath);
	FklVMdll* r=(FklVMdll*)malloc(sizeof(FklVMdll));
	FKL_ASSERT(r);
	r->handle=handle;
	r->pd=FKL_VM_NIL;
	return r;
}

void fklInitVMdll(FklVMvalue* rel,FklVM* exe)
{
	FklDllHandle handle=rel->u.dll->handle;
	void (*init)(FklVMdll* dll,FklVM* exe)=fklGetAddress("_fklInit",handle);
	if(init)
		init(rel->u.dll,exe);
}

void fklDestroyVMvalue(FklVMvalue* cur)
{
	switch(cur->type)
	{
		case FKL_TYPE_STR:
			free(cur->u.str);
			break;
		case FKL_TYPE_BYTEVECTOR:
			free(cur->u.bvec);
			break;
		case FKL_TYPE_PAIR:
			free(cur->u.pair);
			break;
		case FKL_TYPE_PROC:
			fklDestroyVMproc(cur->u.proc);
			break;
		case FKL_TYPE_CHAN:
			fklDestroyVMchanl(cur->u.chan);
			break;
		case FKL_TYPE_FP:
			fklDestroyVMfp(cur->u.fp);
			break;
		case FKL_TYPE_DLL:
			fklDestroyVMdll(cur->u.dll);
			break;
		case FKL_TYPE_DLPROC:
			fklDestroyVMdlproc(cur->u.dlproc);
			break;
		case FKL_TYPE_ERR:
			fklDestroyVMerror(cur->u.err);
			break;
		case FKL_TYPE_VECTOR:
			fklDestroyVMvec(cur->u.vec);
			break;
		case FKL_TYPE_USERDATA:
			if(cur->u.ud->t->__finalizer)
				cur->u.ud->t->__finalizer(cur->u.ud->data);
			fklDestroyVMudata(cur->u.ud);
			break;
		case FKL_TYPE_F64:
		case FKL_TYPE_BOX:
			break;
		case FKL_TYPE_HASHTABLE:
			fklDestroyVMhashTable(cur->u.hash);
			break;
		case FKL_TYPE_BIG_INT:
			fklDestroyBigInt(cur->u.bigInt);
			break;
		case FKL_TYPE_CODE_OBJ:
			fklDestroyByteCodelnt(cur->u.code);
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
	free((void*)cur);
}

void fklDestroyVMdll(FklVMdll* dll)
{
	if(dll->handle)
	{
		void (*uninit)(void)=fklGetAddress("_fklUninit",dll->handle);
		if(uninit)
			uninit();
		fklDestroyDll(dll->handle);
	}
	free(dll);
}

FklVMdlproc* fklCreateVMdlproc(FklVMdllFunc address
		,FklVMvalue* dll
		,FklVMvalue* pd)
{
	FklVMdlproc* tmp=(FklVMdlproc*)malloc(sizeof(FklVMdlproc));
	FKL_ASSERT(tmp);
	tmp->func=address;
	tmp->dll=dll;
	tmp->pd=pd;
	tmp->sid=0;
	return tmp;
}

void fklDestroyVMdlproc(FklVMdlproc* dlproc)
{
	free(dlproc);
}

FklVMerror* fklCreateVMerror(const FklString* who,FklSid_t type,FklString* message)
{
	FklVMerror* t=(FklVMerror*)malloc(sizeof(FklVMerror));
	FKL_ASSERT(t);
	t->who=fklCopyString(who);
	t->type=type;
	t->message=message;
	return t;
}

FklVMerror* fklCreateVMerrorCstr(const char* who,FklSid_t type,FklString* message)
{
	FklVMerror* t=(FklVMerror*)malloc(sizeof(FklVMerror));
	FKL_ASSERT(t);
	t->who=fklCreateStringFromCstr(who);
	t->type=type;
	t->message=message;
	return t;
}

void fklDestroyVMerror(FklVMerror* err)
{
	free(err->who);
	free(err->message);
	free(err);
}

FklVMrecv* fklCreateVMrecv(void)
{
	FklVMrecv* tmp=(FklVMrecv*)malloc(sizeof(FklVMrecv));
	FKL_ASSERT(tmp);
	tmp->v=NULL;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void fklDestroyVMrecv(FklVMrecv* r)
{
	pthread_cond_destroy(&r->cond);
	free(r);
}

FklVMsend* fklCreateVMsend(FklVMvalue* m)
{
	FklVMsend* tmp=(FklVMsend*)malloc(sizeof(FklVMsend));
	FKL_ASSERT(tmp);
	tmp->m=m;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void fklDestroyVMsend(FklVMsend* s)
{
	pthread_cond_destroy(&s->cond);
	free(s);
}

inline void fklResumeThread(pthread_cond_t* cond)
{
	pthread_cond_signal(cond);
}

void fklChanlRecvOk(FklVMchanl* ch,FklVMvalue** r,int* ok)
{
	if(ch->messageNum)
	{
		*r=fklPopPtrQueue(ch->messages);
		ch->messageNum--;
		if(ch->messageNum<ch->max)
		{
			FklVMsend* s=fklPopPtrQueue(ch->sendq);
			ch->sendNum--;
			if(s)
				fklResumeThread(&s->cond);
		}
		*ok=1;
	}
	else
		*ok=0;
}

inline void fklSuspendThread(pthread_cond_t* cond,FklVMgc* gc)
{
	pthread_cond_wait(cond,&gc->tcMutex);
}

void fklChanlRecv(FklVMrecv* r,FklVMchanl* ch,FklVMgc* gc)
{
	if(!ch->messageNum)
	{
		fklPushPtrQueue(r,ch->recvq);
		ch->recvNum++;
		fklSuspendThread(&r->cond,gc);
	}
	r->v=fklPopPtrQueue(ch->messages);
	ch->messageNum--;
	if(ch->messageNum<ch->max)
	{
		FklVMsend* s=fklPopPtrQueue(ch->sendq);
		ch->sendNum--;
		if(s)
			fklResumeThread(&s->cond);
	}
}

void fklChanlSend(FklVMsend* s,FklVMchanl* ch,FklVMgc* gc)
{
	if(ch->recvNum)
	{
		FklVMrecv* r=fklPopPtrQueue(ch->recvq);
		ch->recvNum--;
		if(r)
			fklResumeThread(&r->cond);
	}
	if(!ch->max||ch->messageNum<ch->max-1)
	{
		fklPushPtrQueue(s->m,ch->messages);
		ch->messageNum++;
	}
	else
	{
		if(ch->messageNum>=ch->max-1)
		{
			fklPushPtrQueue(s,ch->sendq);
			ch->sendNum++;
			if(ch->messageNum==ch->max-1)
			{
				fklPushPtrQueue(s->m,ch->messages);
				ch->messageNum++;
			}
			fklSuspendThread(&s->cond,gc);
		}
	}
	fklDestroyVMsend(s);
}

//typedef struct
//{
//	FklSid_t key;
//	FklVMvalue* volatile v;
//}VMenvHashItem;
//
//static VMenvHashItem* createVMenvHashItme(FklSid_t id,FklVMvalue* v)
//{
//	VMenvHashItem* r=(VMenvHashItem*)malloc(sizeof(VMenvHashItem));
//	FKL_ASSERT(r);
//	r->key=id;
//	r->v=v;
//	return r;
//}
//
//static size_t _vmenv_hashFunc(void* key)
//{
//	FklSid_t sid=*(FklSid_t*)key;
//	return sid;
//}
//
//static void _vmenv_destroyItem(void* item)
//{
//	free(item);
//}
//
//static int _vmenv_keyEqual(void* pkey0,void* pkey1)
//{
//	FklSid_t k0=*(FklSid_t*)pkey0;
//	FklSid_t k1=*(FklSid_t*)pkey1;
//	return k0==k1;
//}
//
//static void* _vmenv_getKey(void* item)
//{
//	return &((VMenvHashItem*)item)->key;
//}
//
//static FklHashTableMethodTable VMenvHashMethTable=
//{
//	.__hashFunc=_vmenv_hashFunc,
//	.__destroyItem=_vmenv_destroyItem,
//	.__keyEqual=_vmenv_keyEqual,
//	.__getKey=_vmenv_getKey,
//};
//
//FklVMenv* fklCreateGlobVMenv(FklVMvalue* prev
//		,FklVMgc* gc
//		,FklSymbolTable* table)
//{
//	FklVMenv* tmp=(FklVMenv*)malloc(sizeof(FklVMenv));
//	FKL_ASSERT(tmp);
//	tmp->prev=prev;
//	tmp->t=fklCreateHashTable(512,4,2,0.75,1,&VMenvHashMethTable);
//	fklSetRef(&tmp->prev,prev,gc);
//	fklInitGlobEnv(tmp,gc,table);
//	return tmp;
//}
//
//FklVMenv* fklCreateVMenv(FklVMvalue* prev,FklVMgc* gc)
//{
//	FklVMenv* tmp=(FklVMenv*)malloc(sizeof(FklVMenv));
//	FKL_ASSERT(tmp);
//	tmp->prev=prev;
//	tmp->t=fklCreateHashTable(8,4,2,0.75,1,&VMenvHashMethTable);
//	fklSetRef(&tmp->prev,prev,gc);
//	return tmp;
//}

static FklVMhashTableItem* createVMhashTableItem(FklVMvalue* key,FklVMvalue* v,FklVMgc* gc)
{
	FklVMhashTableItem* tmp=(FklVMhashTableItem*)malloc(sizeof(FklVMhashTableItem));
	FKL_ASSERT(tmp);
	fklSetRef(&tmp->key,key,gc);
	fklSetRef(&tmp->v,v,gc);
	return tmp;
}

static size_t _vmhashtableEq_hashFunc(void* key)
{
	return (uintptr_t)(*(void**)key)>>FKL_UNUSEDBITNUM;
}

static size_t integerHashFunc(const FklVMvalue* v)
{
	if(FKL_IS_FIX(v))
		return FKL_GET_FIX(v);
	else
	{
		size_t sum=0;
		FklBigInt* bi=v->u.bigInt;
		for(size_t i=0;i<bi->num;i++)
			sum+=bi->digits[i];
		return sum;
	}
}

static size_t _vmhashtableEqv_hashFunc(void* key)
{
	FklVMvalue* v=*(FklVMvalue**)key;
	if(fklIsInt(v))
		return integerHashFunc(v);
	else
		return (uintptr_t)(*(void**)key)>>FKL_UNUSEDBITNUM;
}

static size_t _f64_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	union
	{
		double f;
		uint64_t i;
	}t=
	{
		.f=v->u.f64,
	};
	return t.i;
}

static size_t _big_int_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	const FklBigInt* bi=v->u.bigInt;
	size_t r=0;
	for(size_t i=0;i<bi->size;i++)
	{
		uint64_t c=bi->digits[i];
		r+=c<<(i%8);
	}
	if(bi->neg)
		r*=-1;
	return r;
}

static size_t _str_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	FklString* str=v->u.str;
	size_t sum=str->size;
	for(size_t i=0;i<str->size;i++)
		sum+=str->str[i];
	return sum;
}

static size_t _bytevector_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	FklBytevector* bvec=v->u.bvec;
	size_t sum=bvec->size;
	for(size_t i=0;i<bvec->size;i++)
		sum+=bvec->ptr[i];
	return sum;
}

static size_t _vector_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	FklVMvec* vec=v->u.vec;
	for(size_t i=0;i<vec->size;i++)
		fklPushPtrStack(vec->base[i],s);
	return vec->size;
}

static size_t _pair_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	fklPushPtrStack(v->u.pair->car,s);
	fklPushPtrStack(v->u.pair->cdr,s);
	return 2;
}

static size_t _box_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	fklPushPtrStack(v->u.box,s);
	return 1;
}

static size_t _userdata_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	size_t (*udHashFunc)(void*,FklPtrStack* s)=v->u.ud->t->__hash;
	if(udHashFunc)
		return udHashFunc(v->u.ud->data,s);
	else
		return ((uintptr_t)v>>FKL_UNUSEDBITNUM);
}

static size_t _hashTable_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	for(FklHashTableNodeList* list=v->u.hash->ht->list;list;list=list->next)
	{
		FklVMhashTableItem* item=list->node->item;
		fklPushPtrStack(item->key,s);
		fklPushPtrStack(item->v,s);
	}
	return v->u.hash->ht->num+v->u.hash->type;
}

static size_t (*const valueHashFuncTable[FKL_TYPE_CODE_OBJ+1])(const FklVMvalue*,FklPtrStack* s)=
{
	_f64_hashFunc,
	_big_int_hashFunc,
	_str_hashFunc,
	_vector_hashFunc,
	_pair_hashFunc,
	_box_hashFunc,
	_bytevector_hashFunc,
	_userdata_hashFunc,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	_hashTable_hashFunc,
	NULL,
};

static size_t VMvalueHashFunc(const FklVMvalue* v)
{
	size_t sum=0;
	FklPtrStack stack=FKL_STACK_INIT;
	fklInitPtrStack(&stack,32,16);
	fklPushPtrStack((void*)v,&stack);
	while(!fklIsPtrStackEmpty(&stack))
	{
		const FklVMvalue* root=fklPopPtrStack(&stack);
		size_t (*valueHashFunc)(const FklVMvalue*,FklPtrStack*)=NULL;
		if(fklIsInt(root))
			sum+=integerHashFunc(root);
		else if(FKL_IS_PTR(root)&&(valueHashFunc=valueHashFuncTable[v->type]))
			sum+=valueHashFunc(root,&stack);
		else
			sum+=((uintptr_t)root>>FKL_UNUSEDBITNUM);
	}
	fklUninitPtrStack(&stack);
	return sum;
}

static size_t _vmhashtable_hashFunc(void* key)
{
	FklVMvalue* v=*(FklVMvalue**)key;
	if(fklIsInt(v))
		return integerHashFunc(v);
	else
		return VMvalueHashFunc(v);
}

static void _vmhashtable_destroyItem(void* item)
{
	free(item);
}

static int _vmhashtableEq_keyEqual(void* pkey0,void* pkey1)
{
	FklVMvalue* k0=*(FklVMvalue**)pkey0;
	FklVMvalue* k1=*(FklVMvalue**)pkey1;
	return fklVMvalueEq(k0,k1);
}

static void* _vmhashtable_getKey(void* item)
{
	return &((FklVMhashTableItem*)item)->key;
}

static FklHashTableMethodTable VMhashTableEqMethTable=
{
	.__hashFunc=_vmhashtableEq_hashFunc,
	.__destroyItem=_vmhashtable_destroyItem,
	.__keyEqual=_vmhashtableEq_keyEqual,
	.__getKey=_vmhashtable_getKey,
};

static int _vmhashtableEqv_keyEqual(void* pkey0,void* pkey1)
{
	FklVMvalue* k0=*(FklVMvalue**)pkey0;
	FklVMvalue* k1=*(FklVMvalue**)pkey1;
	return fklVMvalueEqv(k0,k1);
}

static FklHashTableMethodTable VMhashTableEqvMethTable=
{
	.__hashFunc=_vmhashtableEqv_hashFunc,
	.__destroyItem=_vmhashtable_destroyItem,
	.__keyEqual=_vmhashtableEqv_keyEqual,
	.__getKey=_vmhashtable_getKey,
};

static int _vmhashtableEqual_keyEqual(void* pkey0,void* pkey1)
{
	FklVMvalue* k0=*(FklVMvalue**)pkey0;
	FklVMvalue* k1=*(FklVMvalue**)pkey1;
	return fklVMvalueEqual(k0,k1);
}

static FklHashTableMethodTable VMhashTableEqualMethTable=
{
	.__hashFunc=_vmhashtable_hashFunc,
	.__destroyItem=_vmhashtable_destroyItem,
	.__keyEqual=_vmhashtableEqual_keyEqual,
	.__getKey=_vmhashtable_getKey,
};

static FklHashTableMethodTable* const VMhashTableMethTableTable[]=
{
	&VMhashTableEqMethTable,
	&VMhashTableEqvMethTable,
	&VMhashTableEqualMethTable,
};

void fklAtomicVMhashTable(FklVMvalue* pht,FklVMgc* gc)
{
	FklVMhashTable* ht=pht->u.hash;
	FklHashTable* table=ht->ht;
	for(FklHashTableNodeList* list=table->list;list;list=list->next)
	{
		FklVMhashTableItem* item=list->node->item;
		fklGC_toGrey(item->key,gc);
		fklGC_toGrey(item->v,gc);
	}
}

FklVMhashTable* fklCreateVMhashTable(FklVMhashTableEqType type)
{
	FklVMhashTable* tmp=(FklVMhashTable*)malloc(sizeof(FklVMhashTable));
	FKL_ASSERT(tmp);
	tmp->type=type;
	tmp->ht=fklCreateHashTable(8,8,2,1.0,1,VMhashTableMethTableTable[type]);
	return tmp;
}

void fklDestroyVMhashTable(FklVMhashTable* ht)
{
	fklDestroyHashTable(ht->ht);
	free(ht);
}

FklVMhashTableItem* fklRefVMhashTable1(FklVMvalue* key,FklVMvalue* toSet,FklVMhashTable* ht,FklVMgc* gc)
{
	FklVMhashTableItem* item=fklRefVMhashTable(key,ht);
	if(!item)
	{
		item=fklPutNoRpHashItem(createVMhashTableItem(key,toSet,gc),ht->ht);
	}
	return item;
}

FklVMhashTableItem* fklRefVMhashTable(FklVMvalue* key,FklVMhashTable* ht)
{
	FklVMhashTableItem* item=fklGetHashItem(&key,ht->ht);
	return item;
}

FklVMvalue* fklGetVMhashTable(FklVMvalue* key,FklVMhashTable* ht,int* ok)
{
	FklVMvalue* r=NULL;
	FklVMhashTableItem* item=fklGetHashItem(&key,ht->ht);
	if(item)
	{
		*ok=1;
		r=item->v;
	}
	else
		*ok=0;
	return r;
}

void fklClearVMhashTable(FklVMhashTable* ht,FklVMgc* gc)
{
	FklHashTable* hash=ht->ht;
	for(FklHashTableNodeList* list=hash->list;list;)
	{
		FklVMhashTableItem* item=list->node->item;
		fklSetRef(&item->key,FKL_VM_NIL,gc);
		fklSetRef(&item->v,FKL_VM_NIL,gc);
		free(item);
		free(list->node);
		FklHashTableNodeList* cur=list;
		list=list->next;
		free(cur);
	}
	hash->num=0;
	hash->list=NULL;
	hash->tail=&hash->list;
}

void fklSetVMhashTableInReverseOrder(FklVMvalue* key,FklVMvalue* v,FklVMhashTable* ht,FklVMgc* gc)
{
	FklVMhashTableItem* item=fklRefVMhashTable(key,ht);
	if(item)
		fklSetRef(&item->v,v,gc);
	else
	{
		fklPutInReverseOrder(createVMhashTableItem(key,v,gc),ht->ht);
	}
}

void fklSetVMhashTable(FklVMvalue* key,FklVMvalue* v,FklVMhashTable* ht,FklVMgc* gc)
{
	FklVMhashTableItem* item=fklRefVMhashTable(key,ht);
	if(item)
		fklSetRef(&item->v,v,gc);
	else
	{
		fklPutReplHashItem(createVMhashTableItem(key,v,gc),ht->ht);
	}
}

void fklAtomicVMvec(FklVMvalue* pVec,FklVMgc* gc)
{
	FklVMvec* vec=pVec->u.vec;
	for(size_t i=0;i<vec->size;i++)
		fklGC_toGrey(vec->base[i],gc);
}

void fklAtomicVMpair(FklVMvalue* root,FklVMgc* gc)
{
	fklGC_toGrey(root->u.pair->car,gc);
	fklGC_toGrey(root->u.pair->cdr,gc);
}

void fklAtomicVMproc(FklVMvalue* root,FklVMgc* gc)
{
	FklVMproc* proc=root->u.proc;
	//if(proc->prevEnv)
	//	fklGC_toGrey(proc->prevEnv,gc);
	fklGC_toGrey(proc->codeObj,gc);
	uint32_t count=proc->count;
	FklVMvalue** ref=proc->closure;
	for(uint32_t i=0;i<count;i++)
		fklGC_toGrey(ref[i],gc);
}

void fklAtomicVMchan(FklVMvalue* root,FklVMgc* gc)
{
	FklQueueNode* head=root->u.chan->messages->head;
	for(;head;head=head->next)
		fklGC_toGrey(head->data,gc);
	for(head=root->u.chan->sendq->head;head;head=head->next)
		fklGC_toGrey(((FklVMsend*)head->data)->m,gc);
}

void fklAtomicVMdll(FklVMvalue* root,FklVMgc* gc)
{
	if(root->u.dll->pd)
		fklGC_toGrey(root->u.dll->pd,gc);
}

void fklAtomicVMdlproc(FklVMvalue* root,FklVMgc* gc)
{
	if(root->u.dlproc->dll)
		fklGC_toGrey(root->u.dlproc->dll,gc);
	if(root->u.dlproc->pd)
		fklGC_toGrey(root->u.dlproc->pd,gc);
}

void fklAtomicVMbox(FklVMvalue* root,FklVMgc* gc)
{
	fklGC_toGrey(root->u.box,gc);
}

void fklAtomicVMuserdata(FklVMvalue* root,FklVMgc* gc)
{
	if(root->u.ud->rel)
		fklGC_toGrey(root->u.ud->rel,gc);
	if(root->u.ud->pd)
		fklGC_toGrey(root->u.ud->rel,gc);
	if(root->u.ud->t->__atomic)
		root->u.ud->t->__atomic(root->u.ud->data,gc);
}

void fklAtomicVMenv(FklVMvalue* penv,FklVMgc* gc)
{
	//FklVMenv* env=penv->u.env;
	//FklHashTable* table=env->t;
	//if(env->prev)
	//	fklGC_toGrey(env->prev,gc);
	//for(FklHashTableNodeList* list=table->list;list;list=list->next)
	//{
	//	VMenvHashItem* item=list->node->item;
	//	fklGC_toGrey(item->v,gc);
	//}
}

//FklVMvalue* volatile* fklFindVar(FklSid_t id,FklVMenv* env)
//{
//	FklVMvalue* volatile* r=NULL;
//	VMenvHashItem* item=fklGetHashItem(&id,env->t);
//	if(item)
//		r=&item->v;
//	return r;
//}
//
//FklVMvalue* volatile* fklFindOrAddVar(FklSid_t id,FklVMenv* env)
//{
//	FklVMvalue* volatile* r=NULL;
//	r=fklFindVar(id,env);
//	if(!r)
//	{
//		VMenvHashItem* ritem=fklPutNoRpHashItem(createVMenvHashItme(id,FKL_VM_NIL),env->t);
//		r=&ritem->v;
//	}
//	return r;
//}
//
//FklVMvalue* volatile* fklFindOrAddVarWithValue(FklSid_t id,FklVMvalue* v,FklVMenv* env)
//{
//	FklVMvalue* volatile* r=NULL;
//	r=fklFindVar(id,env);
//	if(!r)
//	{
//		VMenvHashItem* ritem=fklPutNoRpHashItem(createVMenvHashItme(id,FKL_VM_NIL),env->t);
//		r=&ritem->v;
//	}
//	*r=v;
//	return r;
//}
//
//void fklDBG_printVMenv(FklVMenv* curEnv,FILE* fp,FklSymbolTable* table)
//{
//	if(curEnv->t->num==0)
//		fprintf(fp,"This ENV is empty!");
//	else
//	{
//		fprintf(fp,"ENV:");
//		for(FklHashTableNodeList* list=curEnv->t->list;list;list=list->next)
//		{
//			VMenvHashItem* item=list->node->item;
//			FklVMvalue* tmp=item->v;
//			fklPrin1VMvalue(tmp,fp,table);
//			putc(' ',fp);
//		}
//	}
//}

//void fklDestroyVMenv(FklVMenv* obj)
//{
//	fklDestroyHashTable(obj->t);
//	free(obj);
//}

inline FklVMvalue* fklCreateVMboxNoGC(FklVMgc* gc,FklVMvalue* v)
{
	return fklCreateVMvalueNoGC(FKL_TYPE_BOX,v,gc);
}

FklVMvec* fklCreateVMvecNoInit(size_t size)
{
	FklVMvec* r=(FklVMvec*)malloc(sizeof(FklVMvec)+sizeof(FklVMvalue*)*size);
	FKL_ASSERT(r);
	r->size=size;
	return r;
}

FklVMvec* fklCreateVMvec(size_t size)
{
	FklVMvec* r=(FklVMvec*)malloc(sizeof(FklVMvec)+sizeof(FklVMvalue*)*size);
	FKL_ASSERT(r);
	r->size=size;
	for(size_t i=0;i<size;i++)
		r->base[i]=FKL_VM_NIL;
	return r;
}

void fklDestroyVMvec(FklVMvec* vec)
{
	free(vec);
}

void fklVMvecCat(FklVMvec** fir,const FklVMvec* sec)
{
	size_t firSize=(*fir)->size;
	size_t secSize=sec->size;
	*fir=(FklVMvec*)realloc(*fir,sizeof(FklVMvec)+(firSize+secSize)*sizeof(FklVMvalue*));
	FKL_ASSERT(*fir);
	(*fir)->size=firSize+secSize;
	for(size_t i=0;i<secSize;i++)
		(*fir)->base[firSize+i]=sec->base[i];
}

FklVMudata* fklCreateVMudata(FklSid_t type,const FklVMudMethodTable* t,void* mem,FklVMvalue* rel,FklVMvalue* pd)
{
	FklVMudata* r=(FklVMudata*)malloc(sizeof(FklVMudata));
	FKL_ASSERT(r);
	r->type=type;
	r->t=t;
	r->rel=rel;
	r->pd=pd;
	r->data=mem;
	return r;
}

int fklIsCallableUd(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)&&v->u.ud->t->__call;
}

int fklIsCallable(FklVMvalue* v)
{
	return FKL_IS_PROC(v)||FKL_IS_DLPROC(v)||fklIsCallableUd(v);
}

int fklIsAppendable(FklVMvalue* v)
{
	return FKL_IS_STR(v)
		||FKL_IS_BYTEVECTOR(v)
		||FKL_IS_VECTOR(v)
		||(FKL_IS_USERDATA(v)&&v->u.ud->t->__copy&&v->u.ud->t->__append);
}

void fklDestroyVMudata(FklVMudata* u)
{
	free(u);
}

