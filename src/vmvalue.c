#include<fakeLisp/vm.h>
#include<fakeLisp/parser.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>

typedef struct
{
	FklVMvalue* key;
	uint64_t line;
}LineNumHashItem;

static uintptr_t _LineNumHash_hashFunc(const void* pKey)
{
	const FklVMvalue* key=*(const FklVMvalue**)pKey;
	return (uintptr_t)FKL_GET_PTR(key)>>FKL_UNUSEDBITNUM;
}

static void _LineNumHash_setVal(void* d0,const void* d1)
{
	*(LineNumHashItem*)d0=*(const LineNumHashItem*)d1;
}

static FklHashTableMetaTable LineNumHashMetaTable=
{
	.size=sizeof(LineNumHashItem),
	.__setKey=fklPtrKeySet,
	.__setVal=_LineNumHash_setVal,
	.__hashFunc=_LineNumHash_hashFunc,
	.__uninitItem=fklDoNothingUninitHashItem,
	.__keyEqual=fklPtrKeyEqual,
	.__getKey=fklHashDefaultGetKey,
};

void fklInitLineNumHashTable(FklHashTable* ht)
{
	fklInitHashTable(ht,&LineNumHashMetaTable);
}

#define SENTINEL_NAST_NODE (NULL)
FklVMvalue* fklCreateVMvalueFromNastNode(FklVM* vm
		,const FklNastNode* node
		,FklHashTable* lineHash)
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
			FklVMvalue* v=NULL;
			switch(type)
			{
				case FKL_TYPE_BOX:
					v=fklCreateVMvalueBox(vm,fklPopPtrStack(cStack));
					break;
				case FKL_TYPE_PAIR:
					{
						size_t top=cStack->top;
						v=fklCreateVMvaluePair(vm,cStack->base[top-1],cStack->base[top-2]);
					}
					break;
				case FKL_TYPE_VECTOR:
					{
						v=fklCreateVMvalueVec(vm,cStack->top);
						FklVMvec* vec=FKL_VM_VEC(v);
						for(size_t i=cStack->top,j=0;i>0;i--,j++)
							vec->base[j]=cStack->base[i-1];
					}
					break;
				case FKL_TYPE_HASHTABLE:
					{
						v=fklCreateVMvalueHash(vm,fklPopUintStack(&reftypeStack));
						FklHashTable* hash=FKL_VM_HASH(v);
						for(size_t i=cStack->top;i>0;i-=2)
						{
							FklVMvalue* key=cStack->base[i-1];
							FklVMvalue* v=cStack->base[i-2];
							fklVMhashTableSet(key,v,hash);
						}
					}
					break;
				default:
					abort();
					break;
			}
			fklPushPtrStack(v,tStack);
			if(lineHash)
			{
				LineNumHashItem i={v,line};
				fklGetOrPutHashItem(&i,lineHash);
			}

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
					fklPushPtrStack(FKL_MAKE_VM_FIX(root->fix),cStack);
					break;
				case FKL_NAST_CHR:
					fklPushPtrStack(FKL_MAKE_VM_CHR(root->chr),cStack);
					break;
				case FKL_NAST_SYM:
					fklPushPtrStack(FKL_MAKE_VM_SYM(root->sym),cStack);
					break;
				case FKL_NAST_F64:
					fklPushPtrStack(fklCreateVMvalueF64(vm,root->f64),cStack);
					break;
				case FKL_NAST_STR:
					fklPushPtrStack(fklCreateVMvalueStr(vm
								,fklCopyString(root->str))
							,cStack);
					break;
				case FKL_NAST_BYTEVECTOR:
					fklPushPtrStack(fklCreateVMvalueBvec(vm
								,fklCreateBytevector(root->bvec->size,root->bvec->ptr))
							,cStack);
					break;
				case FKL_NAST_BIG_INT:
					fklPushPtrStack(fklCreateVMvalueBigInt(vm,root->bigInt),cStack);
					break;
				case FKL_NAST_BOX:
					{
						FklPtrStack* bStack=fklCreatePtrStack(1,16);
						fklPushPtrStack(bStack,&stackStack);
						cStack=bStack;
						fklPushUintStack(root->curline,&reftypeStack);
						fklPushUintStack(FKL_TYPE_BOX,&reftypeStack);
						fklPushPtrStack(SENTINEL_NAST_NODE,&nodeStack);
						fklPushPtrStack(root->box,&nodeStack);
					}
					break;
				case FKL_NAST_VECTOR:
					{
						fklPushUintStack(root->curline,&reftypeStack);
						fklPushUintStack(FKL_TYPE_VECTOR,&reftypeStack);
						fklPushPtrStack(SENTINEL_NAST_NODE,&nodeStack);
						for(size_t i=0;i<root->vec->size;i++)
							fklPushPtrStack(root->vec->base[i],&nodeStack);
						FklPtrStack* vStack=fklCreatePtrStack(root->vec->size,16);
						fklPushPtrStack(vStack,&stackStack);
						cStack=vStack;
					}
					break;
				case FKL_NAST_HASHTABLE:
					{
						fklPushUintStack(root->hash->type,&reftypeStack);
						fklPushUintStack(root->curline,&reftypeStack);
						fklPushUintStack(FKL_TYPE_HASHTABLE,&reftypeStack);
						fklPushPtrStack(SENTINEL_NAST_NODE,&nodeStack);
						size_t num=root->hash->num;
						FklNastHashTable* hash=root->hash;
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
						fklPushPtrStack(root->pair->car,&nodeStack);
						fklPushPtrStack(root->pair->cdr,&nodeStack);
					}
					break;
				default:
					abort();
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
	FklHashTable recValueSet;
	fklInitValueSetHashTable(&recValueSet);
	FklNastNode* retval=NULL;
	fklScanCirRef(v,&recValueSet);
	if(!recValueSet.num)
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
					cur->str=NULL;
					break;
				case FKL_TAG_SYM:
					cur->type=FKL_NAST_SYM;
					cur->sym=FKL_GET_SYM(value);
					break;
				case FKL_TAG_CHR:
					cur->type=FKL_NAST_CHR;
					cur->chr=FKL_GET_CHR(value);
					break;
				case FKL_TAG_FIX:
					cur->type=FKL_NAST_FIX;
					cur->fix=FKL_GET_FIX(value);
					break;
				case FKL_TAG_PTR:
					{
						switch(value->type)
						{
							case FKL_TYPE_F64:
								cur->type=FKL_NAST_F64;
								cur->f64=FKL_VM_F64(value);
								break;
							case FKL_TYPE_STR:
								cur->type=FKL_NAST_STR;
								cur->str=fklCopyString(FKL_VM_STR(value));
								break;
							case FKL_TYPE_BYTEVECTOR:
								cur->type=FKL_NAST_BYTEVECTOR;
								cur->bvec=fklCopyBytevector(FKL_VM_BVEC(value));
								break;
							case FKL_TYPE_BIG_INT:
								cur->type=FKL_NAST_BIG_INT;
								cur->bigInt=fklCopyBigInt(FKL_VM_BI(value));
								break;
							case FKL_TYPE_PROC:
								cur->type=FKL_NAST_SYM;
								cur->sym=fklAddSymbolCstr("#<proc>",table)->id;
								break;
							case FKL_TYPE_CPROC:
								cur->type=FKL_NAST_SYM;
								cur->sym=fklAddSymbolCstr("#<cproc>",table)->id;
								break;
							case FKL_TYPE_CHAN:
								cur->type=FKL_NAST_SYM;
								cur->sym=fklAddSymbolCstr("#<chan>",table)->id;
								break;
							case FKL_TYPE_FP:
								cur->type=FKL_NAST_SYM;
								cur->sym=fklAddSymbolCstr("#<fp>",table)->id;
								break;
							case FKL_TYPE_ERR:
								cur->type=FKL_NAST_SYM;
								cur->sym=fklAddSymbolCstr("#<err>",table)->id;
								break;
							case FKL_TYPE_CODE_OBJ:
								cur->type=FKL_NAST_SYM;
								cur->sym=fklAddSymbolCstr("#<code-obj>",table)->id;
								break;
							case FKL_TYPE_USERDATA:
								cur->type=FKL_NAST_SYM;
								cur->sym=fklAddSymbolCstr("#<userdata>",table)->id;
								break;
							case FKL_TYPE_DLL:
								cur->type=FKL_NAST_SYM;
								cur->sym=fklAddSymbolCstr("#<dll>",table)->id;
								break;
							case FKL_TYPE_BOX:
								cur->type=FKL_NAST_BOX;
								fklPushPtrStack(FKL_VM_BOX(value),&s0);
								fklPushPtrStack(&cur->box,&s1);
								fklPushUintStack(cur->curline,&lineStack);
								break;
							case FKL_TYPE_PAIR:
								cur->type=FKL_NAST_PAIR;
								cur->pair=fklCreateNastPair();
								fklPushPtrStack(FKL_VM_CAR(value),&s0);
								fklPushPtrStack(FKL_VM_CDR(value),&s0);
								fklPushPtrStack(&cur->pair->car,&s1);
								fklPushPtrStack(&cur->pair->cdr,&s1);
								fklPushUintStack(cur->curline,&lineStack);
								fklPushUintStack(cur->curline,&lineStack);
								break;
							case FKL_TYPE_VECTOR:
								{
									FklVMvec* vec=FKL_VM_VEC(value);
									cur->type=FKL_NAST_VECTOR;
									cur->vec=fklCreateNastVector(vec->size);
									for(size_t i=0;i<vec->size;i++)
										fklPushPtrStack(vec->base[i],&s0);
									for(size_t i=0;i<cur->vec->size;i++)
									{
										fklPushPtrStack(&cur->vec->base[i],&s1);
										fklPushUintStack(cur->curline,&lineStack);
									}
								}
								break;
							case FKL_TYPE_HASHTABLE:
								{
									FklHashTable* hash=FKL_VM_HASH(value);
									cur->type=FKL_NAST_HASHTABLE;
									cur->hash=fklCreateNastHash(fklGetVMhashTableType(hash),hash->num);
									for(FklHashTableItem* list=hash->first;list;list=list->next)
									{
										FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
										fklPushPtrStack(item->key,&s0);
										fklPushPtrStack(item->v,&s0);
									}
									for(size_t i=0;i<cur->hash->num;i++)
									{
										fklPushPtrStack(&cur->hash->items[i].car,&s1);
										fklPushPtrStack(&cur->hash->items[i].cdr,&s1);
										fklPushUintStack(cur->curline,&lineStack);
										fklPushUintStack(cur->curline,&lineStack);
									}
								}
								break;
							default:
								abort();
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
	fklUninitHashTable(&recValueSet);
	return retval;
}

#undef SENTINEL_NAST_NODE

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
							*root1=fklCreateVMvaluePairNil(vm);
							fklPushPtrStack(&FKL_VM_CAR(*root1),&s2);
							fklPushPtrStack(&FKL_VM_CDR(*root1),&s2);
							fklPushPtrStack(FKL_VM_CAR(root),&s1);
							fklPushPtrStack(FKL_VM_CDR(root),&s1);
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
	FklVMvalue* tmp=fklCreateVMvalueF64(vm,FKL_VM_F64(obj));
	return tmp;
}

static FklVMvalue* __fkl_bigint_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCreateVMvalueBigInt(vm,FKL_VM_BI(obj));
}

static FklVMvalue* __fkl_vector_copyer(FklVMvalue* obj,FklVM* vm)
{
	FklVMvec* vec=FKL_VM_VEC(obj);
	return fklCreateVMvalueVecWithPtr(vm,vec->size,vec->base);
}

static FklVMvalue* __fkl_str_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCreateVMvalueStr(vm,fklCopyString(FKL_VM_STR(obj)));
}

static FklVMvalue* __fkl_bytevector_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCreateVMvalueBvec(vm,fklCopyBytevector(FKL_VM_BVEC(obj)));
}

static FklVMvalue* __fkl_pair_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCopyVMlistOrAtom(obj,vm);
}

static FklVMvalue* __fkl_box_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCreateVMvalueBox(vm,FKL_VM_BOX(obj));
}

static FklVMvalue* __fkl_userdata_copyer(FklVMvalue* obj,FklVM* vm)
{
	FklVMudata* src=FKL_VM_UD(obj);
	FklVMvalue* dst=fklCreateVMvalueUdata(vm
			,src->t
			,src->rel);
	if(fklCopyVMudata(src,FKL_VM_UD(dst)))
		return NULL;
	return dst;
}

static FklVMvalue* __fkl_hashtable_copyer(FklVMvalue* obj,FklVM* vm)
{
	FklHashTable* ht=FKL_VM_HASH(obj);
	FklVMvalue* r=fklCreateVMvalueHashEq(vm);
	FklHashTable* nht=FKL_VM_HASH(r);
	nht->t=ht->t;
	for(FklHashTableItem* list=ht->first;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
		fklVMhashTableSet(item->key,item->v,nht);
	}
	return r;
}

static FklVMvalue* (*const valueCopyers[FKL_VM_VALUE_GC_TYPE_NUM])(FklVMvalue* obj,FklVM* vm)=
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
			abort();
			break;
	}
	return tmp;
}

FklVMvalue* fklCreateTrueValue()
{
	return FKL_MAKE_VM_FIX(1);
}

FklVMvalue* fklCreateNilValue()
{
	return FKL_VM_NIL;
}

int fklVMvalueEqv(const FklVMvalue* fir,const FklVMvalue* sec)
{
	if(FKL_IS_BIG_INT(fir)&&FKL_IS_BIG_INT(sec))
		return fklCmpBigInt(FKL_VM_BI(fir),FKL_VM_BI(sec))==0;
	else
		return fir==sec;
}

int fklVMvalueEq(const FklVMvalue* fir,const FklVMvalue* sec)
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
		if(FKL_GET_TAG(root1)!=FKL_GET_TAG(root2))
			r=0;
		else if(!FKL_IS_PTR(root1)&&!FKL_IS_PTR(root2)&&root1!=root2)
			r=0;
		else if(FKL_IS_PTR(root1)&&FKL_IS_PTR(root2))
		{
			if(root1->type!=root2->type)
				r=0;
			else
				switch(root1->type)
				{
					case FKL_TYPE_F64:
						r=FKL_VM_F64(root1)==FKL_VM_F64(root2);
						break;
					case FKL_TYPE_STR:
						r=!fklStringCmp(FKL_VM_STR(root1),FKL_VM_STR(root2));
						break;
					case FKL_TYPE_BYTEVECTOR:
						r=!fklBytevectorCmp(FKL_VM_BVEC(root1),FKL_VM_BVEC(root2));
						break;
					case FKL_TYPE_PAIR:
						r=1;
						fklPushPtrStack(FKL_VM_CAR(root1),&s1);
						fklPushPtrStack(FKL_VM_CDR(root1),&s1);
						fklPushPtrStack(FKL_VM_CAR(root2),&s2);
						fklPushPtrStack(FKL_VM_CDR(root2),&s2);
						break;
					case FKL_TYPE_BOX:
						r=1;
						fklPushPtrStack(FKL_VM_BOX(root1),&s1);
						fklPushPtrStack(FKL_VM_BOX(root2),&s2);
						break;
					case FKL_TYPE_VECTOR:
						{
							FklVMvec* vec1=FKL_VM_VEC(root1);
							FklVMvec* vec2=FKL_VM_VEC(root2);
							if(vec1->size!=vec2->size)
								r=0;
							else
							{
								r=1;
								size_t size=vec1->size;
								for(size_t i=0;i<size;i++)
									fklPushPtrStack(vec1->base[i],&s1);
								for(size_t i=0;i<size;i++)
									fklPushPtrStack(vec2->base[i],&s2);
							}
						}
						break;
					case FKL_TYPE_BIG_INT:
						r=!fklCmpBigInt(FKL_VM_BI(root1),FKL_VM_BI(root2));
						break;
					case FKL_TYPE_USERDATA:
						{
							FklVMudata* ud1=FKL_VM_UD(root1);
							FklVMudata* ud2=FKL_VM_UD(root2);
							if(ud1->t!=ud2->t||!ud1->t->__equal)
								r=0;
							else
								r=ud1->t->__equal(ud1,ud2);
						}
						break;
					case FKL_TYPE_HASHTABLE:
						{
							FklHashTable* h1=FKL_VM_HASH(root1);
							FklHashTable* h2=FKL_VM_HASH(root2);
							if(h1->t!=h2->t||h1->num!=h2->num)
								r=0;
							else
							{
								for(FklHashTableItem* list=h1->first;list;list=list->next)
								{
									FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
									fklPushPtrStack(item->key,&s1);
									fklPushPtrStack(item->v,&s1);
								}
								for(FklHashTableItem* list=h2->first;list;list=list->next)
								{
									FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
									fklPushPtrStack(item->key,&s2);
									fklPushPtrStack(item->v,&s2);
								}
							}
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

int fklVMvalueCmp(FklVMvalue* a,FklVMvalue* b,int* err)
{
	int r=0;
	*err=0;
	if((FKL_IS_F64(a)&&fklIsVMnumber(b))
			||(FKL_IS_F64(b)&&fklIsVMnumber(a)))
	{
		double af=fklGetDouble(a);
		double bf=fklGetDouble(b);
		r=isgreater(af,bf)?1:
			(isless(af,bf)?-1
			 :0);
	}
	else if(FKL_IS_FIX(a)&&FKL_IS_FIX(b))
		r=FKL_GET_FIX(a)-FKL_GET_FIX(b);
	else if(FKL_IS_BIG_INT(a)&&FKL_IS_BIG_INT(b))
		r=fklCmpBigInt(FKL_VM_BI(a),FKL_VM_BI(b));
	else if(FKL_IS_BIG_INT(a)&&FKL_IS_FIX(b))
		r=fklCmpBigIntI(FKL_VM_BI(a),FKL_GET_FIX(b));
	else if(FKL_IS_FIX(a)&&FKL_IS_BIG_INT(b))
		r=-1*(fklCmpBigIntI(FKL_VM_BI(b),FKL_GET_FIX(a)));
	else if(FKL_IS_STR(a)&&FKL_IS_STR(b))
		r=fklStringCmp(FKL_VM_STR(a),FKL_VM_STR(b));
	else if(FKL_IS_BYTEVECTOR(a)&&FKL_IS_BYTEVECTOR(a))
		r=fklBytevectorCmp(FKL_VM_BVEC(a),FKL_VM_BVEC(b));
	else if(FKL_IS_CHR(a)&&FKL_IS_CHR(b))
		r=FKL_GET_CHR(a)-FKL_GET_CHR(b);
	else if(FKL_IS_USERDATA(a)&&fklIsCmpableUd(FKL_VM_UD(a)))
		r=fklCmpVMudata(FKL_VM_UD(a),b,err);
	else if(FKL_IS_USERDATA(b)&&fklIsCmpableUd(FKL_VM_UD(b)))
		r=fklCmpVMudata(FKL_VM_UD(b),a,err)*-1;
	else
		*err=1;
	return r;
}

FklVMfpRW fklGetVMfpRwFromCstr(const char* mode)
{
	int hasPlus=0;
	int hasW=0;
	switch(*(mode++))
	{
		case 'w':
		case 'a':
			hasW=1;
			break;
		case 'r':
			break;
		default:
			break;
	}

	while(*mode)
		switch(*(mode++))
		{
			case '+':
				hasPlus=1;
				goto ret;
				break;
			case 't':
			case 'b':
			case 'c':
			case 'n':
				break;
			default:
				goto ret;
				break;
		}
ret:
	if(hasPlus)
		return FKL_VM_FP_RW;
	else if(hasW)
		return FKL_VM_FP_W;
	return FKL_VM_FP_R;
}

int fklVMfpEof(FklVMfp* vfp)
{
	return feof(vfp->fp);
}

int fklVMfpRewind(FklVMfp* vfp,FklStringBuffer* b,size_t j)
{
	return fklRewindStream(vfp->fp,b->buf+j,b->index-j);
}

int fklUninitVMfp(FklVMfp* vfp)
{
	int r=0;
	FILE* fp=vfp->fp;
	if(!(fp!=NULL&&fp!=stdin&&fp!=stdout&&fp!=stderr&&fclose(fp)!=EOF))
		r=1;
	return r;
}

void fklInitVMdll(FklVMvalue* rel,FklVM* exe)
{
	FklVMdll* dll=FKL_VM_DLL(rel);
	void (*init)(FklVMdll* dll,FklVM* exe)=fklGetAddress("_fklInit",&dll->dll);
	if(init)
		init(dll,exe);
}

static inline void uninit_nothing_value(FklVMvalue* v)
{
}

static inline void uninit_big_int_value(FklVMvalue* v)
{
	fklUninitBigInt(FKL_VM_BI(v));
}

static inline void uninit_string_value(FklVMvalue* v)
{
	free(FKL_VM_STR(v));
}

static inline void uninit_bvec_value(FklVMvalue* v)
{
	free(FKL_VM_BVEC(v));
}

static inline void uninit_vector_value(FklVMvalue* v)
{
	free(FKL_VM_VEC(v)->base);
}

static inline void uninit_ud_value(FklVMvalue* v)
{
	fklFinalizeVMudata(FKL_VM_UD(v));
}

static inline void uninit_proc_value(FklVMvalue* v)
{
	FklVMproc* proc=FKL_VM_PROC(v);
	free(proc->closure);
}

static inline void destroy_all_queue_node(FklQueueNode* head)
{
	while(head)
	{
		FklQueueNode* prev=head;
		head=head->next;
		fklDestroyQueueNode(prev);
	}
}

static inline void uninit_chanl_value(FklVMvalue* v)
{
	FklVMchanl* ch=FKL_VM_CHANL(v);
	fklUninitPtrQueue(&ch->messages);
	destroy_all_queue_node(ch->msg_cache);
}

static inline void uninit_fp_value(FklVMvalue* v)
{
	fklUninitVMfp(FKL_VM_FP(v));
}

static inline void uninit_dll_value(FklVMvalue* v)
{
	FklVMdll* dll=FKL_VM_DLL(v);
	void (*uninit)(void)=fklGetAddress("_fklUninit",&dll->dll);
	if(uninit)
		uninit();
	uv_dlclose(&dll->dll);
}

static inline void uninit_err_value(FklVMvalue* v)
{
	FklVMerror* err=FKL_VM_ERR(v);
	free(err->where);
	free(err->message);
}

static inline void uninit_hash_value(FklVMvalue* v)
{
	fklUninitHashTable(FKL_VM_HASH(v));
}

static inline void uninit_code_obj_value(FklVMvalue* v)
{
	FklByteCodelnt* t=FKL_VM_CO(v);
	fklDestroyByteCode(t->bc);
	if(t->l)
		free(t->l);
}

void fklDestroyVMvalue(FklVMvalue* cur)
{
	static void (*fkl_value_uniniters[FKL_VM_VALUE_GC_TYPE_NUM])(FklVMvalue* v)=
	{
		uninit_nothing_value,  //f64
		uninit_big_int_value,  //big-int
		uninit_string_value,   //string
		uninit_vector_value,   //vector
		uninit_nothing_value,  //pair
		uninit_nothing_value,  //box
		uninit_bvec_value,     //bvec
		uninit_ud_value,       //ud
		uninit_proc_value,     //proc
		uninit_chanl_value,    //chanl
		uninit_fp_value,       //fp
		uninit_dll_value,      //dll
		uninit_nothing_value,  //cproc
		uninit_err_value,      //error
		uninit_hash_value,     //hash
		uninit_code_obj_value, //code-obj
		uninit_nothing_value,  //var-ref
	};

	fkl_value_uniniters[cur->type](cur);
	free((void*)cur);
}

static inline void chanl_push_recv(FklVMchanl* ch,FklVMchanlRecv* recv)
{
	*(ch->recvq.tail)=recv;
	ch->recvq.tail=&recv->next;
}

static inline FklVMchanlRecv* chanl_pop_recv(FklVMchanl* ch)
{
	FklVMchanlRecv* r=ch->recvq.head;
	if(r)
	{
		ch->recvq.head=r->next;
		if(r->next==NULL)
			ch->recvq.tail=&ch->recvq.head;
	}
	return r;
}

static inline void chanl_push_send(FklVMchanl* ch,FklVMchanlSend* send)
{
	*(ch->sendq.tail)=send;
	ch->sendq.tail=&send->next;
}

static inline FklVMchanlSend* chanl_pop_send(FklVMchanl* ch)
{
	FklVMchanlSend* s=ch->sendq.head;
	if(s)
	{
		ch->sendq.head=s->next;
		if(s->next==NULL)
			ch->sendq.tail=&ch->sendq.head;
	}
	return s;
}

static inline void chanl_push_msg(FklVMchanl* ch,FklVMvalue* msg)
{
	FklQueueNode* n=NULL;
	if(ch->msg_cache)
	{
		n=ch->msg_cache;
		ch->msg_cache=n->next;
	}
	else
		n=fklCreateQueueNode(msg);
	fklPushPtrQueueNode(&ch->messages,n);
	ch->msg_num++;
}

static inline FklVMvalue* chanl_pop_msg(FklVMchanl* ch)
{
	FklQueueNode* n=fklPopPtrQueueNode(&ch->messages);
	n->next=ch->msg_cache;
	ch->msg_cache=n;
	ch->msg_num--;
	return n->data;
}

int fklChanlRecvOk(FklVMchanl* ch,FklVMvalue** slot)
{
	uv_mutex_lock(&ch->lock);
	FklVMchanlSend* send=chanl_pop_send(ch);
	if(send)
	{
		if(ch->msg_num)
		{
			*slot=chanl_pop_msg(ch);
			chanl_push_msg(ch,send->msg);
		}
		else
			*slot=send->msg;
		uv_cond_signal(&send->cond);
		uv_mutex_unlock(&ch->lock);
		return 1;
	}
	else if(ch->msg_num)
	{
		*slot=chanl_pop_msg(ch);
		uv_mutex_unlock(&ch->lock);
		return 1;
	}
	else
	{
		uv_mutex_unlock(&ch->lock);
		return 0;
	}
}

void fklChanlRecv(FklVMchanl* ch,FklVMvalue** slot,FklVM* exe)
{
	uv_mutex_lock(&ch->lock);
	FklVMchanlSend* send=chanl_pop_send(ch);
	if(send)
	{
		if(ch->msg_num)
		{
			*slot=chanl_pop_msg(ch);
			chanl_push_msg(ch,send->msg);
		}
		else
			*slot=send->msg;
		uv_cond_signal(&send->cond);
		uv_mutex_unlock(&ch->lock);
		return;
	}
	else if(ch->msg_num)
	{
		*slot=chanl_pop_msg(ch);
		uv_mutex_unlock(&ch->lock);
		return;
	}
	else
	{
		FklVMchanlRecv r={.slot=slot,};
		if(uv_cond_init(&r.cond))
			abort();
		chanl_push_recv(ch,&r);
		fklUnlockThread(exe);
		uv_cond_wait(&r.cond,&ch->lock);
		uv_mutex_unlock(&ch->lock);
		fklLockThread(exe);
		uv_cond_destroy(&r.cond);
		return;
	}
}

void fklChanlSend(FklVMchanl* ch,FklVMvalue* msg,FklVM* exe)
{
	uv_mutex_lock(&ch->lock);
	FklVMchanlRecv* recv=chanl_pop_recv(ch);
	if(recv)
	{
		*(recv->slot)=msg;
		uv_cond_signal(&recv->cond);
		uv_mutex_unlock(&ch->lock);
		return;
	}
	else if(ch->msg_num<ch->max)
	{
		chanl_push_msg(ch,msg);
		uv_mutex_unlock(&ch->lock);
		return;
	}
	else
	{
		FklVMchanlSend s={.msg=msg,};
		if(uv_cond_init(&s.cond))
			abort();
		chanl_push_send(ch,&s);
		fklUnlockThread(exe);
		uv_cond_wait(&s.cond,&ch->lock);
		uv_mutex_unlock(&ch->lock);
		fklLockThread(exe);
		uv_cond_destroy(&s.cond);
		return;
	}
}

uint64_t fklVMchanlRecvqLen(FklVMchanl* ch)
{
	uint64_t r=0;
	uv_mutex_lock(&ch->lock);
	for(FklVMchanlRecv* r=ch->recvq.head;r;r=r->next)
		r++;
	uv_mutex_unlock(&ch->lock);
	return r;
}

uint64_t fklVMchanlSendqLen(FklVMchanl* ch)
{
	uint64_t r=0;
	uv_mutex_lock(&ch->lock);
	for(FklVMchanlSend* r=ch->sendq.head;r;r=r->next)
		r++;
	uv_mutex_unlock(&ch->lock);
	return r;
}

uint64_t fklVMchanlMessageNum(FklVMchanl* ch)
{
	uint64_t r=0;
	uv_mutex_lock(&ch->lock);
	r=ch->msg_num;
	uv_mutex_unlock(&ch->lock);
	return r;
}

int fklVMchanlFull(FklVMchanl* ch)
{
	int r=0;
	uv_mutex_lock(&ch->lock);
	r=ch->msg_num>=ch->max;
	uv_mutex_unlock(&ch->lock);
	return r;
}

uintptr_t fklVMhashEqHashv(const FklVMvalue* key)
{
	return ((uintptr_t)(key))>>FKL_UNUSEDBITNUM;
}

static uintptr_t _vmhashtableEq_hashFunc(const void* key)
{
	return fklVMhashEqHashv(*(const void**)key);
}

static size_t integerHashFunc(const FklVMvalue* v)
{
	if(FKL_IS_FIX(v))
		return FKL_GET_FIX(v);
	else
	{
		size_t sum=0;
		FklBigInt* bi=FKL_VM_BI(v);
		for(size_t i=0;i<bi->num;i++)
			sum+=bi->digits[i];
		return sum;
	}
}

uintptr_t fklVMhashEqvHashv(const FklVMvalue* v)
{
	if(fklIsVMint(v))
		return integerHashFunc(v);
	else
		return fklVMhashEqHashv(v);
}

static uintptr_t _vmhashtableEqv_hashFunc(const void* key)
{
	const FklVMvalue* v=*(const FklVMvalue**)key;
	return fklVMhashEqvHashv(v);
}

static size_t _f64_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	union
	{
		double f;
		uint64_t i;
	}t=
	{
		.f=FKL_VM_F64(v),
	};
	return t.i;
}

static size_t _big_int_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	const FklBigInt* bi=FKL_VM_BI(v);
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
	const FklString* str=FKL_VM_STR(v);
	return fklStringHash(str);
}

static size_t _bytevector_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	const FklBytevector* bvec=FKL_VM_BVEC(v);
	return fklBytevectorHash(bvec);
}

static size_t _vector_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	const FklVMvec* vec=FKL_VM_VEC(v);
	for(size_t i=0;i<vec->size;i++)
		fklPushPtrStack(vec->base[i],s);
	return vec->size;
}

static size_t _pair_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	fklPushPtrStack(FKL_VM_CAR(v),s);
	fklPushPtrStack(FKL_VM_CDR(v),s);
	return 2;
}

static size_t _box_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	fklPushPtrStack(FKL_VM_BOX(v),s);
	return 1;
}

static size_t _userdata_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	return fklHashvVMudata(FKL_VM_UD(v));
}

static size_t _hashTable_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	FklHashTable* hash=FKL_VM_HASH(v);
	for(FklHashTableItem* list=hash->first;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
		fklPushPtrStack(item->key,s);
		fklPushPtrStack(item->v,s);
	}
	return hash->num+fklGetVMhashTableType(hash);
}

static size_t (*const valueHashFuncTable[FKL_VM_VALUE_GC_TYPE_NUM])(const FklVMvalue*,FklPtrStack* s)=
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
		if(fklIsVMint(root))
			sum+=integerHashFunc(root);
		else if(FKL_IS_PTR(root)&&(valueHashFunc=valueHashFuncTable[v->type]))
			sum+=valueHashFunc(root,&stack);
		else
			sum+=((uintptr_t)root>>FKL_UNUSEDBITNUM);
	}
	fklUninitPtrStack(&stack);
	return sum;
}

uintptr_t fklVMvalueEqualHashv(const FklVMvalue* v)
{
	if(fklIsVMint(v))
		return integerHashFunc(v);
	else
		return VMvalueHashFunc(v);
}

static uintptr_t _vmhashtable_hashFunc(const void* key)
{
	const FklVMvalue* v=*(const FklVMvalue**)key;
	return fklVMvalueEqualHashv(v);
}

static void _vmhashtable_setVal(void* d0,const void* d1)
{
	*(FklVMhashTableItem*)d0=*(const FklVMhashTableItem*)d1;
}

static int _vmhashtableEqv_keyEqual(const void* pkey0,const void* pkey1)
{
	const FklVMvalue* k0=*(const FklVMvalue**)pkey0;
	const FklVMvalue* k1=*(const FklVMvalue**)pkey1;
	return fklVMvalueEqv(k0,k1);
}

static int _vmhashtableEqual_keyEqual(const void* pkey0,const void* pkey1)
{
	const FklVMvalue* k0=*(const FklVMvalue**)pkey0;
	const FklVMvalue* k1=*(const FklVMvalue**)pkey1;
	return fklVMvalueEqual(k0,k1);
}

static const FklHashTableMetaTable VMhashTableMetaTableTable[]=
{
	//eq
	{
		.size=sizeof(FklVMhashTableItem),
		.__setKey=fklPtrKeySet,
		.__setVal=_vmhashtable_setVal,
		.__hashFunc=_vmhashtableEq_hashFunc,
		.__uninitItem=fklDoNothingUninitHashItem,
		.__keyEqual=fklPtrKeyEqual,
		.__getKey=fklHashDefaultGetKey,
	},
	//eqv
	{
		.size=sizeof(FklVMhashTableItem),
		.__setKey=fklPtrKeySet,
		.__setVal=_vmhashtable_setVal,
		.__hashFunc=_vmhashtableEqv_hashFunc,
		.__uninitItem=fklDoNothingUninitHashItem,
		.__keyEqual=_vmhashtableEqv_keyEqual,
		.__getKey=fklHashDefaultGetKey,
	},
	//equal
	{
		.size=sizeof(FklVMhashTableItem),
		.__setKey=fklPtrKeySet,
		.__setVal=_vmhashtable_setVal,
		.__hashFunc=_vmhashtable_hashFunc,
		.__uninitItem=fklDoNothingUninitHashItem,
		.__keyEqual=_vmhashtableEqual_keyEqual,
		.__getKey=fklHashDefaultGetKey,
	},
};

static const FklHashTableMetaTable* EqHashTableT=&VMhashTableMetaTableTable[0];
static const FklHashTableMetaTable* EqvHashTableT=&VMhashTableMetaTableTable[1];
static const FklHashTableMetaTable* EqualHashTableT=&VMhashTableMetaTableTable[2];

void fklAtomicVMhashTable(FklVMvalue* pht,FklVMgc* gc)
{
	FklHashTable* table=FKL_VM_HASH(pht);
	for(FklHashTableItem* list=table->first;list;list=list->next)
	{
		FklVMhashTableItem* item=(FklVMhashTableItem*)list->data;
		fklVMgcToGray(item->key,gc);
		fklVMgcToGray(item->v,gc);
	}
}

int fklIsVMhashEq(FklHashTable* ht)
{
	return ht->t==EqHashTableT;
}

int fklIsVMhashEqv(FklHashTable* ht)
{
	return ht->t==EqvHashTableT;
}

int fklIsVMhashEqual(FklHashTable* ht)
{
	return ht->t==EqualHashTableT;
}

uintptr_t fklGetVMhashTableType(FklHashTable* ht)
{
	return ht->t-EqHashTableT;
}

const char* fklGetVMhashTablePrefix(FklHashTable* ht)
{
	static const char* prefix[]=
	{
		"#hash(",
		"#hasheqv(",
		"#hashequal(",
	};
	return prefix[fklGetVMhashTableType(ht)];
}

FklVMhashTableItem* fklVMhashTableRef1(FklVMvalue* key,FklVMvalue* toSet,FklHashTable* ht,FklVMgc* gc)
{
	FklVMhashTableItem i={key,toSet};
	return fklGetOrPutHashItem(&i,ht);
}

FklVMhashTableItem* fklVMhashTableRef(FklVMvalue* key,FklHashTable* ht)
{
	FklVMhashTableItem* item=fklGetHashItem(&key,ht);
	return item;
}

FklVMhashTableItem* fklVMhashTableGetItem(FklVMvalue* key,FklHashTable* ht)
{
	return (FklVMhashTableItem*)fklGetHashItem(&key,ht);
}

FklVMvalue* fklVMhashTableGet(FklVMvalue* key,FklHashTable* ht,int* ok)
{
	FklVMvalue* r=NULL;
	FklVMhashTableItem* item=fklGetHashItem(&key,ht);
	if(item)
	{
		*ok=1;
		r=item->v;
	}
	else
		*ok=0;
	return r;
}

void fklVMhashTableSet(FklVMvalue* key,FklVMvalue* v,FklHashTable* ht)
{
	FklVMhashTableItem* i=fklPutHashItem(&key,ht);
	i->v=v;
}

#define NEW_OBJ(TYPE) (FklVMvalue*)calloc(1,sizeof(TYPE));

FklVMvalue* fklCreateVMvaluePair(FklVM* exe,FklVMvalue* car,FklVMvalue* cdr)
{
	FklVMvalue* r=NEW_OBJ(FklVMvaluePair);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_PAIR;
	FKL_VM_CAR(r)=car;
	FKL_VM_CDR(r)=cdr;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvaluePairWithCar(FklVM* exe,FklVMvalue* car)
{
	FklVMvalue* r=NEW_OBJ(FklVMvaluePair);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_PAIR;
	FKL_VM_CAR(r)=car;
	FKL_VM_CDR(r)=FKL_VM_NIL;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvaluePairNil(FklVM* exe)
{
	FklVMvalue* r=NEW_OBJ(FklVMvaluePair);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_PAIR;
	FKL_VM_CAR(r)=FKL_VM_NIL;
	FKL_VM_CDR(r)=FKL_VM_NIL;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueVec(FklVM* exe,size_t size)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueVec);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_VECTOR;
	FklVMvec* v=FKL_VM_VEC(r);
	FklVMvalue** base=(FklVMvalue**)calloc(size,sizeof(FklVMvalue*));
	FKL_ASSERT(base);
	v->base=base;
	v->size=size;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueVecWithPtr(FklVM* exe,size_t size,FklVMvalue* const* ptr)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueVec);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_VECTOR;
	FklVMvec* v=FKL_VM_VEC(r);
	size_t ss=size*sizeof(FklVMvalue*);
	FklVMvalue** base=(FklVMvalue**)malloc(ss);
	FKL_ASSERT(base);
	memcpy(base,ptr,ss);
	v->base=base;
	v->size=size;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBox(FklVM* exe,FklVMvalue* b)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueBox);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BOX;
	FKL_VM_BOX(r)=b;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBoxNil(FklVM* exe)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueBox);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BOX;
	FKL_VM_BOX(r)=FKL_VM_NIL;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueF64(FklVM* exe,double d)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueF64);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_F64;
	FKL_VM_F64(r)=d;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueStr(FklVM* exe,FklString* s)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueStr);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_STR;
	FKL_VM_STR(r)=s;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBvec(FklVM* exe,FklBytevector* b)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueStr);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BYTEVECTOR;
	FKL_VM_BVEC(r)=b;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueError(FklVM* exe
		,FklSid_t type
		,const FklString* where
		,FklString* message)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueErr);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_ERR;
	FklVMerror* err=FKL_VM_ERR(r);
	err->type=type;
	err->where=fklCopyString(where);
	err->message=message;
	fklAddToGC(r,exe);
	return r;
}

static inline void init_chanl_sendq(struct FklVMchanlSendq* q)
{
	q->head=NULL;
	q->tail=&q->head;
}

static inline void init_chanl_recvq(struct FklVMchanlRecvq* q)
{
	q->head=NULL;
	q->tail=&q->head;
}

FklVMvalue* fklCreateVMvalueChanl(FklVM* exe,size_t max)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueChanl);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_CHAN;
	FklVMchanl* ch=FKL_VM_CHANL(r);
	ch->max=max;
	ch->msg_num=0;
	fklInitPtrQueue(&ch->messages);
	ch->msg_cache=NULL;
	uv_mutex_init(&ch->lock);
	init_chanl_sendq(&ch->sendq);
	init_chanl_recvq(&ch->recvq);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueFp(FklVM* exe,FILE* fp,FklVMfpRW rw)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueFp);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_FP;
	FklVMfp* vfp=FKL_VM_FP(r);
	vfp->fp=fp;
	vfp->rw=rw;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueErrorWithCstr(FklVM* exe
		,FklSid_t type
		,const char* where
		,FklString* message)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueErr);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_ERR;
	FklVMerror* err=FKL_VM_ERR(r);
	err->type=type;
	err->where=fklCreateStringFromCstr(where);
	err->message=message;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBigIntWithString(FklVM* exe,const FklString* str,int base)
{
	if(base==10)
		return fklCreateVMvalueBigIntWithDecString(exe,str);
	else if(base==16)
		return fklCreateVMvalueBigIntWithHexString(exe,str);
	else
		return fklCreateVMvalueBigIntWithOctString(exe,str);
}

FklVMvalue* fklCreateVMvalueBigIntWithDecString(FklVM* exe,const FklString* str)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueBigInt);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BIG_INT;
	fklInitBigIntFromDecString(FKL_VM_BI(r),str);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBigIntWithHexString(FklVM* exe,const FklString* str)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueBigInt);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BIG_INT;
	fklInitBigIntFromHexString(FKL_VM_BI(r),str);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBigIntWithOctString(FklVM* exe,const FklString* str)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueBigInt);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BIG_INT;
	fklInitBigIntFromOctString(FKL_VM_BI(r),str);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBigInt(FklVM* exe
		,const FklBigInt* bi)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueBigInt);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BIG_INT;
	fklSetBigInt(FKL_VM_BI(r),bi);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBigIntWithI64(FklVM* exe
		,int64_t i)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueBigInt);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BIG_INT;
	fklSetBigIntI(FKL_VM_BI(r),i);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBigIntWithU64(FklVM* exe
		,uint64_t u)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueBigInt);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BIG_INT;
	fklSetBigIntU(FKL_VM_BI(r),u);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBigIntWithF64(FklVM* exe
		,double d)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueBigInt);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BIG_INT;
	fklSetBigIntD(FKL_VM_BI(r),d);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueProc(FklVM* exe
		,FklInstruction* spc
		,uint64_t cpc
		,FklVMvalue* codeObj
		,uint32_t pid)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueProc);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_PROC;
	FklVMproc* proc=FKL_VM_PROC(r);

	FklFuncPrototype* pt=&exe->pts->pts[pid];

	proc->spc=spc;
	proc->end=spc+cpc;
	proc->sid=pt->sid;
	proc->protoId=pid;
	proc->lcount=pt->lcount;
	proc->codeObj=codeObj;

	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueProcWithWholeCodeObj(FklVM* exe
		,FklVMvalue* codeObj
		,uint32_t pid)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueProc);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_PROC;
	FklVMproc* proc=FKL_VM_PROC(r);

	FklFuncPrototype* pt=&exe->pts->pts[pid];

 	FklByteCode* bc=FKL_VM_CO(codeObj)->bc;
	proc->spc=bc->code;
	proc->end=bc->code+bc->len;
	proc->sid=pt->sid;
	proc->protoId=pid;
	proc->lcount=pt->lcount;
	proc->codeObj=codeObj;

	fklAddToGC(r,exe);
	return r;
}


FklVMvalue* fklCreateVMvalueHash(FklVM* exe,FklHashTableEqType type)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueHash);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_HASHTABLE;
	fklInitHashTable(FKL_VM_HASH(r),&VMhashTableMetaTableTable[type]);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueHashEq(FklVM* exe)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueHash);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_HASHTABLE;
	fklInitHashTable(FKL_VM_HASH(r),EqHashTableT);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueHashEqv(FklVM* exe)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueHash);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_HASHTABLE;
	fklInitHashTable(FKL_VM_HASH(r),EqvHashTableT);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueHashEqual(FklVM* exe)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueHash);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_HASHTABLE;
	fklInitHashTable(FKL_VM_HASH(r),EqualHashTableT);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueCodeObj(FklVM* exe,FklByteCodelnt* bcl)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueCodeObj);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_CODE_OBJ;
	*FKL_VM_CO(r)=*bcl;
	free(bcl);
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueDll(FklVM* exe,const char* dllName,char** errorStr)
{
	size_t len=strlen(dllName)+strlen(FKL_DLL_FILE_TYPE)+1;
	char* realDllName=(char*)malloc(len);
	FKL_ASSERT(realDllName);
	strcpy(realDllName,dllName);
	strcat(realDllName,FKL_DLL_FILE_TYPE);
	char* rpath=fklRealpath(realDllName);
	if(!rpath)
		goto err;
	uv_lib_t lib;
	if(uv_dlopen(rpath,&lib))
	{
		*errorStr=fklCopyCstr(uv_dlerror(&lib));
		uv_dlclose(&lib);
		free(rpath);
		goto err;
	}
	free(rpath);
	FklVMvalue* r=NEW_OBJ(FklVMvalueDll);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_DLL;
	FklVMdll* dll=FKL_VM_DLL(r);
	dll->dll=lib;
	dll->pd=FKL_VM_NIL;
	fklAddToGC(r,exe);
	free(realDllName);
	return r;
err:
	free(realDllName);
	return NULL;
}

FklVMvalue* fklCreateVMvalueCproc(FklVM* exe
		,FklVMcFunc func
		,FklVMvalue* dll
		,FklVMvalue* pd
		,FklSid_t sid)
{
	FklVMvalue* r=NEW_OBJ(FklVMvalueCproc);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_CPROC;
	FklVMcproc* dlp=FKL_VM_CPROC(r);
	dlp->func=func;
	dlp->dll=dll;
	dlp->pd=pd;
	dlp->sid=sid;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueUdata(FklVM* exe
		,const FklVMudMetaTable* t
		,FklVMvalue* rel)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueUd)+t->size);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_USERDATA;
	FklVMudata* ud=FKL_VM_UD(r);
	ud->t=t;
	ud->rel=rel;
	fklAddToGC(r,exe);
	return r;
}

#undef NEW_OBJ

void fklAtomicVMvec(FklVMvalue* pVec,FklVMgc* gc)
{
	FklVMvec* vec=FKL_VM_VEC(pVec);
	for(size_t i=0;i<vec->size;i++)
		fklVMgcToGray(vec->base[i],gc);
}

void fklAtomicVMpair(FklVMvalue* root,FklVMgc* gc)
{
	fklVMgcToGray(FKL_VM_CAR(root),gc);
	fklVMgcToGray(FKL_VM_CDR(root),gc);
}

void fklAtomicVMproc(FklVMvalue* root,FklVMgc* gc)
{
	FklVMproc* proc=FKL_VM_PROC(root);
	fklVMgcToGray(proc->codeObj,gc);
	uint32_t count=proc->rcount;
	FklVMvalue** ref=proc->closure;
	for(uint32_t i=0;i<count;i++)
		fklVMgcToGray(ref[i],gc);
}

void fklAtomicVMchan(FklVMvalue* root,FklVMgc* gc)
{
	FklVMchanl* ch=FKL_VM_CHANL(root);
	for(FklQueueNode* head=ch->messages.head
		;head
		;head=head->next)
		fklVMgcToGray(head->data,gc);
	for(FklVMchanlSend* s=ch->sendq.head;s;s=s->next)
		fklVMgcToGray(s->msg,gc);
}

void fklAtomicVMdll(FklVMvalue* root,FklVMgc* gc)
{
	fklVMgcToGray(FKL_VM_DLL(root)->pd,gc);
}

void fklAtomicVMcproc(FklVMvalue* root,FklVMgc* gc)
{
	FklVMcproc* cproc=FKL_VM_CPROC(root);
	fklVMgcToGray(cproc->dll,gc);
	fklVMgcToGray(cproc->pd,gc);
}

void fklAtomicVMbox(FklVMvalue* root,FklVMgc* gc)
{
	fklVMgcToGray(FKL_VM_BOX(root),gc);
}

void fklAtomicVMuserdata(FklVMvalue* root,FklVMgc* gc)
{
	FklVMudata* ud=FKL_VM_UD(root);
	fklVMgcToGray(ud->rel,gc);
	if(ud->t->__atomic)
		ud->t->__atomic(ud,gc);
}

void fklVMvecConcat(FklVMvec* fir,const FklVMvec* sec)
{
	size_t firSize=fir->size;
	size_t secSize=sec->size;
	FklVMvalue** nbase=(FklVMvalue**)fklRealloc(fir->base,(firSize+secSize)*sizeof(FklVMvalue*));
	FKL_ASSERT(nbase);
	fir->base=nbase;
	fir->size=firSize+secSize;
	for(size_t i=0;i<secSize;i++)
		nbase[firSize+i]=sec->base[i];
}

int fklIsCallableUd(const FklVMudata* ud)
{
	return ud->t->__call!=NULL;
}

int fklIsCallable(FklVMvalue* v)
{
	return FKL_IS_PROC(v)||FKL_IS_CPROC(v)||(FKL_IS_USERDATA(v)&&fklIsCallableUd(FKL_VM_UD(v)));
}

static inline int is_appendable_userdata(FklVMudata* ud)
{
	return ud->t->__append!=NULL;
}

int fklIsAppendable(FklVMvalue* v)
{
	return FKL_IS_STR(v)
		||FKL_IS_BYTEVECTOR(v)
		||FKL_IS_VECTOR(v)
		||(FKL_IS_USERDATA(v)&&is_appendable_userdata(FKL_VM_UD(v)));
}

int fklIsCmpableUd(const FklVMudata* u)
{
	return u->t->__cmp!=NULL;
}

int fklIsWritableUd(const FklVMudata* u)
{
	return u->t->__write!=NULL;
}

int fklIsAbleToStringUd(const FklVMudata* u)
{
	return u->t->__to_string!=NULL;
}

int fklUdHasLength(const FklVMudata* u)
{
	return u->t->__length!=NULL;
}

void fklPrincVMudata(const FklVMudata* u,FILE* fp,FklSymbolTable* table)
{
	void (*princ)(const FklVMudata*,FILE*,FklSymbolTable*)=u->t->__princ;
	if(princ)
		princ(u,fp,table);
	else
		fprintf(fp,"#<userdata %p>",u);
}

void fklPrin1VMudata(const FklVMudata* u,FILE* fp,FklSymbolTable* table)
{
	void (*prin1)(const FklVMudata*,FILE*,FklSymbolTable*)=u->t->__prin1;
	if(prin1)
		prin1(u,fp,table);
	else
		fprintf(fp,"#<userdata %p>",u);
}

int fklAppendVMudata(FklVMudata* a,const FklVMudata* b)
{
	void (*append)(FklVMudata*,const FklVMudata*)=a->t->__append;
	if(append)
	{
		append(a,b);
		return 0;
	}
	return 1;
}

int fklCmpVMudata(const FklVMudata* a,const FklVMvalue* b,int* r)
{
	return a->t->__cmp(a,b,r);
}

size_t fklLengthVMudata(const FklVMudata* a)
{
	return a->t->__length(a);
}

FklString* fklUdToString(const FklVMudata* a)
{
	return a->t->__to_string(a);
}

void fklWriteVMudata(const FklVMudata* a,FILE* fp)
{
	a->t->__write(a,fp);
}

size_t fklHashvVMudata(const FklVMudata* a)
{
	size_t (*hashv)(const FklVMudata*)=a->t->__hash;
	if(hashv)
		return hashv(a);
	else
		return ((uintptr_t)a->data>>FKL_UNUSEDBITNUM);
}

void fklFinalizeVMudata(FklVMudata* a)
{
	void (*finalize)(FklVMudata*)=a->t->__finalizer;
	if(finalize)
		finalize(a);
}

int fklCopyVMudata(const FklVMudata* a,FklVMudata* dst)
{
	void (*copyer)(const FklVMudata* a,FklVMudata* dst)=a->t->__copy;
	if(copyer)
	{
		copyer(a,dst);
		return 0;
	}
	return 1;
}

void* fklVMvalueTerminalCreate(const char* s,size_t len,size_t line,void* ctx)
{
	FklVM* exe=(FklVM*)ctx;
	FklString* str=fklCreateString(len,s);
	return fklCreateVMvalueStr(exe,str);
}

void fklVMvalueTerminalDestroy(void* ast)
{
}

