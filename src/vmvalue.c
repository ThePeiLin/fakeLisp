#include<fakeLisp/vm.h>
#include<fakeLisp/parser.h>
#include<stdalign.h>
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
					fklPushPtrStack(fklCreateVMvalueStr(vm,root->str),cStack);
					break;
				case FKL_NAST_BYTEVECTOR:
					fklPushPtrStack(fklCreateVMvalueBvec2(vm,root->bvec->size,root->bvec->ptr)
							,cStack);
					break;
				case FKL_NAST_BIG_INT:
					fklPushPtrStack(fklCreateVMvalueBigInt2(vm,root->bigInt),cStack);
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
		,FklVMgc* gc)
{
	FklNastNode* retval=NULL;
	if(!fklHasCircleRef(v))
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
			LineNumHashItem* item=lineHash?fklGetHashItem(&value,lineHash):NULL;
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
								cur->bigInt=fklCreateBigIntWithVMbigInt(FKL_VM_BI(value));
								break;
							case FKL_TYPE_PROC:
								cur->type=FKL_NAST_SYM;
								cur->sym=fklVMaddSymbolCstr(gc,"#<proc>")->id;
								break;
							case FKL_TYPE_CPROC:
								cur->type=FKL_NAST_SYM;
								cur->sym=fklVMaddSymbolCstr(gc,"#<cproc>")->id;
								break;
							case FKL_TYPE_USERDATA:
								cur->type=FKL_NAST_SYM;
								cur->sym=fklVMaddSymbolCstr(gc,"#<userdata>")->id;
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
	return fklCreateVMvalueBigIntWithOther(vm,FKL_VM_BI(obj));
}

static FklVMvalue* __fkl_vector_copyer(FklVMvalue* obj,FklVM* vm)
{
	FklVMvec* vec=FKL_VM_VEC(obj);
	return fklCreateVMvalueVecWithPtr(vm,vec->size,vec->base);
}

static FklVMvalue* __fkl_str_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCreateVMvalueStr(vm,FKL_VM_STR(obj));
}

static FklVMvalue* __fkl_bytevector_copyer(FklVMvalue* obj,FklVM* vm)
{
	return fklCreateVMvalueBvec(vm,FKL_VM_BVEC(obj));
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
	const FklVMud* ud=FKL_VM_UD(obj);
	FklVMudCopyAppender copyer=ud->t->__copy_append;
	return copyer(vm,ud,0,NULL);
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
	[FKL_TYPE_F64]        = __fkl_f64_copyer,
	[FKL_TYPE_BIG_INT]    = __fkl_bigint_copyer,
	[FKL_TYPE_STR]        = __fkl_str_copyer,
	[FKL_TYPE_VECTOR]     = __fkl_vector_copyer,
	[FKL_TYPE_PAIR]       = __fkl_pair_copyer,
	[FKL_TYPE_BOX]        = __fkl_box_copyer,
	[FKL_TYPE_BYTEVECTOR] = __fkl_bytevector_copyer,
	[FKL_TYPE_USERDATA]   = __fkl_userdata_copyer,
	[FKL_TYPE_HASHTABLE]  = __fkl_hashtable_copyer,
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
		return fklVMbigIntEqual(FKL_VM_BI(fir),FKL_VM_BI(sec));
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
						r=fklStringEqual(FKL_VM_STR(root1),FKL_VM_STR(root2));
						break;
					case FKL_TYPE_BYTEVECTOR:
						r=fklBytevectorEqual(FKL_VM_BVEC(root1),FKL_VM_BVEC(root2));
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
						r=fklVMbigIntEqual(FKL_VM_BI(root1),FKL_VM_BI(root2));
						break;
					case FKL_TYPE_USERDATA:
						{
							FklVMud* ud1=FKL_VM_UD(root1);
							FklVMud* ud2=FKL_VM_UD(root2);
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
		double af=fklVMgetDouble(a);
		double bf=fklVMgetDouble(b);
		r=isgreater(af,bf)?1:
			(isless(af,bf)?-1
			 :0);
	}
	else if(FKL_IS_FIX(a)&&FKL_IS_FIX(b))
		r=FKL_GET_FIX(a)-FKL_GET_FIX(b);
	else if(FKL_IS_BIG_INT(a)&&FKL_IS_BIG_INT(b))
		r=fklVMbigIntCmp(FKL_VM_BI(a),FKL_VM_BI(b));
	else if(FKL_IS_BIG_INT(a)&&FKL_IS_FIX(b))
		r=fklVMbigIntCmpI(FKL_VM_BI(a),FKL_GET_FIX(b));
	else if(FKL_IS_FIX(a)&&FKL_IS_BIG_INT(b))
		r=-1*(fklVMbigIntCmpI(FKL_VM_BI(b),FKL_GET_FIX(a)));
	else if(FKL_IS_STR(a)&&FKL_IS_STR(b))
		r=fklStringCmp(FKL_VM_STR(a),FKL_VM_STR(b));
	else if(FKL_IS_BYTEVECTOR(a)&&FKL_IS_BYTEVECTOR(a))
		r=fklBytevectorCmp(FKL_VM_BVEC(a),FKL_VM_BVEC(b));
	else if(FKL_IS_CHR(a)&&FKL_IS_CHR(b))
		r=FKL_GET_CHR(a)-FKL_GET_CHR(b);
	else if(FKL_IS_USERDATA(a)&&fklIsCmpableUd(FKL_VM_UD(a)))
		r=fklCmpVMud(FKL_VM_UD(a),b,err);
	else if(FKL_IS_USERDATA(b)&&fklIsCmpableUd(FKL_VM_UD(b)))
		r=-fklCmpVMud(FKL_VM_UD(b),a,err);
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

int fklVMfpFileno(FklVMfp* vfp)
{
#ifdef WIN32
	return _fileno(vfp->fp);
#else
	return fileno(vfp->fp);
#endif
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

static inline void uninit_ud_value(FklVMvalue* v)
{
	fklFinalizeVMud(FKL_VM_UD(v));
}

static inline void uninit_proc_value(FklVMvalue* v)
{
	FklVMproc* proc=FKL_VM_PROC(v);
	free(proc->closure);
}

static inline void uninit_hash_value(FklVMvalue* v)
{
	fklUninitHashTable(FKL_VM_HASH(v));
}

void fklDestroyVMvalue(FklVMvalue* cur)
{
	static void (*fkl_value_uniniters[FKL_VM_VALUE_GC_TYPE_NUM])(FklVMvalue* v)=
	{
		[FKL_TYPE_F64]        = uninit_nothing_value,  //f64
		[FKL_TYPE_BIG_INT]    = uninit_nothing_value,  //big-int
		[FKL_TYPE_STR]        = uninit_nothing_value,  //string
		[FKL_TYPE_VECTOR]     = uninit_nothing_value,  //vector
		[FKL_TYPE_PAIR]       = uninit_nothing_value,  //pair
		[FKL_TYPE_BOX]        = uninit_nothing_value,  //box
		[FKL_TYPE_BYTEVECTOR] = uninit_nothing_value,  //bvec
		[FKL_TYPE_USERDATA]   = uninit_ud_value,       //ud
		[FKL_TYPE_PROC]       = uninit_proc_value,     //proc
		[FKL_TYPE_CPROC]      = uninit_nothing_value,  //cproc
		[FKL_TYPE_HASHTABLE]  = uninit_hash_value,     //hash
		[FKL_TYPE_VAR_REF]    = uninit_nothing_value,  //var-ref
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

static inline void chanl_push_msg(FklVMchanl* c,FklVMvalue* msg)
{
	c->buf[c->sendx]=msg;
	c->sendx=(c->sendx+1)%c->qsize;
	c->count++;
}

static inline FklVMvalue* chanl_pop_msg(FklVMchanl* c)
{
	FklVMvalue* r=c->buf[c->recvx];
	c->recvx=(c->recvx+1)%c->qsize;
	c->count--;
	return r;
}

int fklChanlRecvOk(FklVMchanl* ch,FklVMvalue** slot)
{
	uv_mutex_lock(&ch->lock);
	FklVMchanlSend* send=chanl_pop_send(ch);
	if(send)
	{
		if(ch->count)
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
	else if(ch->count)
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
		if(ch->count)
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
	else if(ch->count)
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
	else if(ch->count<ch->qsize)
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
	r=ch->count;
	uv_mutex_unlock(&ch->lock);
	return r;
}

int fklVMchanlFull(FklVMchanl* ch)
{
	int r=0;
	uv_mutex_lock(&ch->lock);
	r=ch->count>=ch->qsize;
	uv_mutex_unlock(&ch->lock);
	return r;
}

int fklVMchanlEmpty(FklVMchanl* ch)
{
	int r=0;
	uv_mutex_lock(&ch->lock);
	r=ch->count==0;
	uv_mutex_unlock(&ch->lock);
	return r;
}

uintptr_t fklVMvalueEqHashv(const FklVMvalue* key)
{
	return ((uintptr_t)(key))>>FKL_UNUSEDBITNUM;
}

static uintptr_t _vmhashtableEq_hashFunc(const void* key)
{
	return fklVMvalueEqHashv(*(const void**)key);
}

static size_t integerHashFunc(const FklVMvalue* v)
{
	if(FKL_IS_FIX(v))
		return FKL_GET_FIX(v);
	else
		return fklVMbigIntHash(FKL_VM_BI(v));
}

uintptr_t fklVMvalueEqvHashv(const FklVMvalue* v)
{
	if(fklIsVMint(v))
		return integerHashFunc(v);
	else
		return fklVMvalueEqHashv(v);
}

static uintptr_t _vmhashtableEqv_hashFunc(const void* key)
{
	const FklVMvalue* v=*(const FklVMvalue**)key;
	return fklVMvalueEqvHashv(v);
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
	return fklVMbigIntHash(FKL_VM_BI(v));
}

static size_t _str_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	return fklStringHash(FKL_VM_STR(v));
}

static size_t _bytevector_hashFunc(const FklVMvalue* v,FklPtrStack* s)
{
	return fklBytevectorHash(FKL_VM_BVEC(v));
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
	return fklHashvVMud(FKL_VM_UD(v),s);
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
	[FKL_TYPE_F64]        = _f64_hashFunc,
	[FKL_TYPE_BIG_INT]    = _big_int_hashFunc,
	[FKL_TYPE_STR]        = _str_hashFunc,
	[FKL_TYPE_VECTOR]     = _vector_hashFunc,
	[FKL_TYPE_PAIR]       = _pair_hashFunc,
	[FKL_TYPE_BOX]        = _box_hashFunc,
	[FKL_TYPE_BYTEVECTOR] = _bytevector_hashFunc,
	[FKL_TYPE_USERDATA]   = _userdata_hashFunc,
	[FKL_TYPE_HASHTABLE]  = _hashTable_hashFunc,
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
	FklVMvalue* r;
	FklVMhashTableItem* item=fklGetHashItem(&key,ht);
	if(item)
	{
		*ok=1;
		r=item->v;
	}
	else
	{
		*ok=0;
		r=NULL;
	}
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
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueVec)+size*sizeof(FklVMvalue*));
	FKL_ASSERT(r);
	r->type=FKL_TYPE_VECTOR;
	FklVMvec* v=FKL_VM_VEC(r);
	v->size=size;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueVecWithPtr(FklVM* exe,size_t size,FklVMvalue* const* ptr)
{
	size_t ss=size*sizeof(FklVMvalue*);
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueVec)+ss);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_VECTOR;
	FklVMvec* v=FKL_VM_VEC(r);
	memcpy(v->base,ptr,ss);
	v->size=size;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueVec3(FklVM* exe
		,FklVMvalue* a
		,FklVMvalue* b
		,FklVMvalue* c)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueVec)+3*sizeof(FklVMvalue*));
	FKL_ASSERT(r);
	r->type=FKL_TYPE_VECTOR;
	FklVMvec* v=FKL_VM_VEC(r);
	v->base[0]=a;
	v->base[1]=b;
	v->base[2]=c;
	v->size=3;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueVec4(FklVM* exe
		,FklVMvalue* a
		,FklVMvalue* b
		,FklVMvalue* c
		,FklVMvalue* d)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueVec)+4*sizeof(FklVMvalue*));
	FKL_ASSERT(r);
	r->type=FKL_TYPE_VECTOR;
	FklVMvec* v=FKL_VM_VEC(r);
	v->base[0]=a;
	v->base[1]=b;
	v->base[2]=c;
	v->base[3]=d;
	v->size=4;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueVec5(FklVM* exe
		,FklVMvalue* a
		,FklVMvalue* b
		,FklVMvalue* c
		,FklVMvalue* d
		,FklVMvalue* f)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueVec)+5*sizeof(FklVMvalue*));
	FKL_ASSERT(r);
	r->type=FKL_TYPE_VECTOR;
	FklVMvec* v=FKL_VM_VEC(r);
	v->base[0]=a;
	v->base[1]=b;
	v->base[2]=c;
	v->base[3]=d;
	v->base[4]=f;
	v->size=5;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueVec6(FklVM* exe
		,FklVMvalue* a
		,FklVMvalue* b
		,FklVMvalue* c
		,FklVMvalue* d
		,FklVMvalue* f
		,FklVMvalue* e)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueVec)+6*sizeof(FklVMvalue*));
	FKL_ASSERT(r);
	r->type=FKL_TYPE_VECTOR;
	FklVMvec* v=FKL_VM_VEC(r);
	v->base[0]=a;
	v->base[1]=b;
	v->base[2]=c;
	v->base[3]=d;
	v->base[4]=f;
	v->base[5]=e;
	v->size=6;
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

FklVMvalue* fklCreateVMvalueStr(FklVM* exe,const FklString* s)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueStr)+s->size*sizeof(s->str[0]));
	FKL_ASSERT(r);
	r->type=FKL_TYPE_STR;
	FklString* rs=FKL_VM_STR(r);
	rs->size=s->size;
	memcpy(rs->str,s->str,rs->size*sizeof(rs->str[0]));
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueStr2(FklVM* exe,size_t size,const char* str)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueStr)+size*sizeof(str[0]));
	FKL_ASSERT(r);
	r->type=FKL_TYPE_STR;
	FklString* rs=FKL_VM_STR(r);
	rs->size=size;
	if(str)
		memcpy(rs->str,str,rs->size*sizeof(rs->str[0]));
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBvec(FklVM* exe,const FklBytevector* b)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueBvec)+b->size*sizeof(b->ptr[0]));
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BYTEVECTOR;
	FklBytevector* bvec=FKL_VM_BVEC(r);
	bvec->size=b->size;
	memcpy(bvec->ptr,b->ptr,bvec->size*sizeof(bvec->ptr[0]));
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBvec2(FklVM* exe,size_t size,const uint8_t* ptr)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueBvec)+size*sizeof(ptr[0]));
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BYTEVECTOR;
	FklBytevector* bvec=FKL_VM_BVEC(r);
	bvec->size=size;
	if(ptr)
		memcpy(bvec->ptr,ptr,bvec->size*sizeof(bvec->ptr[0]));
	fklAddToGC(r,exe);
	return r;
}

static void _error_userdata_as_princ(const FklVMud* ud,FklStringBuffer* buf,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(err,FklVMerror,ud);
	fklStringBufferConcatWithString(buf,err->message);
}

static inline void print_raw_symbol_to_string_buffer(FklStringBuffer* s,FklString* f)
{
	fklPrintRawStringToStringBuffer(s,f,"|","|",'|');
}

static inline void print_raw_string_to_string_buffer(FklStringBuffer* s,FklString* f)
{
	fklPrintRawStringToStringBuffer(s,f,"\"","\"",'"');
}

static void _error_userdata_as_prin1(const FklVMud* ud,FklStringBuffer* buf,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(err,FklVMerror,ud);
	fklStringBufferConcatWithCstr(buf,"#<err t: ");
	print_raw_symbol_to_string_buffer(buf,fklVMgetSymbolWithId(gc,err->type)->symbol);
	fklStringBufferConcatWithCstr(buf," m: ");
	print_raw_string_to_string_buffer(buf,err->message);
	fklStringBufferPutc(buf,'>');
}

static void _error_userdata_finalizer(FklVMud* v)
{
	FKL_DECL_UD_DATA(err,FklVMerror,v);
	free(err->message);
}

static FklVMudMetaTable ErrorUserDataMetaTable=
{
	.size=sizeof(FklVMdll),
	.__as_princ=_error_userdata_as_princ,
	.__as_prin1=_error_userdata_as_prin1,
	.__finalizer=_error_userdata_finalizer,
};

FklVMvalue* fklCreateVMvalueError(FklVM* exe
		,FklSid_t type
		,FklString* message)
{
	FklVMvalue* r=fklCreateVMvalueUd(exe,&ErrorUserDataMetaTable,NULL);
	FklVMerror* err=FKL_VM_ERR(r);
	err->type=type;
	err->message=message;
	return r;
}

int fklIsVMvalueError(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)&&FKL_VM_UD(v)->t==&ErrorUserDataMetaTable;
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

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(_chanl_userdata_as_print,chanl);

static void _chanl_userdata_atomic(const FklVMud* root,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(ch,FklVMchanl,root);

	if(ch->recvx<ch->sendx)
	{

		FklVMvalue** const end=&ch->buf[ch->sendx];
		for(FklVMvalue** buf=&ch->buf[ch->recvx];buf<end;buf++)
			fklVMgcToGray(*buf,gc);
	}
	else
	{
		FklVMvalue** end=&ch->buf[ch->qsize];
		FklVMvalue** buf=&ch->buf[ch->recvx];
		for(;buf<end;buf++)
			fklVMgcToGray(*buf,gc);

		buf=ch->buf;
		end=&ch->buf[ch->sendx];
		for(;buf<end;buf++)
			fklVMgcToGray(*buf,gc);
	}
	for(FklVMchanlSend* s=ch->sendq.head;s;s=s->next)
		fklVMgcToGray(s->msg,gc);
}

static FklVMudMetaTable ChanlUserDataMetaTable=
{
	.size=sizeof(FklVMchanl),
	.__as_princ=_chanl_userdata_as_print,
	.__as_prin1=_chanl_userdata_as_print,
	.__atomic=_chanl_userdata_atomic,
};

FklVMvalue* fklCreateVMvalueChanl(FklVM* exe,uint32_t qsize)
{
	FklVMvalue* r=fklCreateVMvalueUd2(exe,&ChanlUserDataMetaTable,sizeof(FklVMvalue*)*qsize,NULL);
	FklVMchanl* ch=FKL_VM_CHANL(r);
	ch->qsize=qsize;
	uv_mutex_init(&ch->lock);
	init_chanl_sendq(&ch->sendq);
	init_chanl_recvq(&ch->recvq);
	return r;
}

int fklIsVMvalueChanl(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)&&FKL_VM_UD(v)->t==&ChanlUserDataMetaTable;
}

static void _fp_userdata_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(fp,FklVMfp,ud);
	fklUninitVMfp(fp);
}

static void _fp_userdata_as_print(const FklVMud* ud,FklStringBuffer* buf,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(vfp,FklVMfp,ud);
	if(vfp->fp==stdin)
		fklStringBufferConcatWithCstr(buf,"#<fp stdin>");
	else if(vfp->fp==stdout)
		fklStringBufferConcatWithCstr(buf,"#<fp stdout>");
	else if(vfp->fp==stderr)
		fklStringBufferConcatWithCstr(buf,"#<fp stderr>");
	else
		fklStringBufferPrintf(buf,"#<fp %p>",vfp);
}

static FklVMudMetaTable FpUserDataMetaTable=
{
	.size=sizeof(FklVMfp),
	.__as_princ=_fp_userdata_as_print,
	.__as_prin1=_fp_userdata_as_print,
	.__finalizer=_fp_userdata_finalizer,
};

FklVMvalue* fklCreateVMvalueFp(FklVM* exe,FILE* fp,FklVMfpRW rw)
{
	FklVMvalue* r=fklCreateVMvalueUd(exe,&FpUserDataMetaTable,NULL);
	FklVMfp* vfp=FKL_VM_FP(r);
	vfp->fp=fp;
	vfp->rw=rw;
	return r;
}

int fklIsVMvalueFp(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)&&FKL_VM_UD(v)->t==&FpUserDataMetaTable;
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

typedef struct
{
	FklVM* exe;
	FklVMvalue* bi;
}VMbigIntInitCtx;

static FklBigIntDigit* vmbigint_alloc_cb(void* ptr,size_t size)
{
	VMbigIntInitCtx* ctx=(VMbigIntInitCtx*)ptr;
	ctx->bi=fklCreateVMvalueBigInt(ctx->exe,size);
	return FKL_VM_BI(ctx->bi)->digits;
}

static int64_t* vmbigint_num_cb(void* ptr)
{
	VMbigIntInitCtx* ctx=(VMbigIntInitCtx*)ptr;
	return &FKL_VM_BI(ctx->bi)->num;
}

static const FklBigIntInitWithCharBufMethodTable VMbigIntInitWithCharBufMethodTable=
{
	.alloc=vmbigint_alloc_cb,
	.num=vmbigint_num_cb,
};

FklVMvalue* fklCreateVMvalueBigIntWithDecString(FklVM* exe,const FklString* str)
{
	VMbigIntInitCtx ctx=
	{
		.exe=exe,
		.bi=NULL,
	};
	fklInitBigIntWithDecCharBuf2(&ctx,&VMbigIntInitWithCharBufMethodTable,str->str,str->size);
	return ctx.bi;
}

FklVMvalue* fklCreateVMvalueBigIntWithHexString(FklVM* exe,const FklString* str)
{
	VMbigIntInitCtx ctx=
	{
		.exe=exe,
		.bi=NULL,
	};
	fklInitBigIntWithHexCharBuf2(&ctx,&VMbigIntInitWithCharBufMethodTable,str->str,str->size);
	return ctx.bi;
}

FklVMvalue* fklCreateVMvalueBigIntWithOctString(FklVM* exe,const FklString* str)
{
	VMbigIntInitCtx ctx=
	{
		.exe=exe,
		.bi=NULL,
	};
	fklInitBigIntWithOctCharBuf2(&ctx,&VMbigIntInitWithCharBufMethodTable,str->str,str->size);
	return ctx.bi;
}

FklVMvalue* fklCreateVMvalueBigInt(FklVM* exe,size_t size)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueBigInt)+size*sizeof(FklBigIntDigit));
	FKL_ASSERT(r);
	r->type=FKL_TYPE_BIG_INT;
	FklVMbigInt* b=FKL_VM_BI(r);
	b->num=size+1;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueBigInt2(FklVM* exe
		,const FklBigInt* bi)
{
	FklVMvalue* r=fklCreateVMvalueBigInt(exe,fklAbs(bi->num));
	FklVMbigInt* b=FKL_VM_BI(r);
	FklBigInt t=
	{
		.digits=b->digits,
		.num=0,
		.size=fklAbs(b->num),
		.const_size=1,
	};
	fklSetBigInt(&t,bi);
	b->num=t.num;
	return r;
}

FklVMvalue* fklCreateVMvalueBigInt3(FklVM* exe
		,const FklBigInt* bi
		,size_t size)
{
	FKL_ASSERT(size>=(size_t)fklAbs(bi->num));
	FklVMvalue* r=fklCreateVMvalueBigInt(exe,fklAbs(bi->num));
	FklVMbigInt* b=FKL_VM_BI(r);
	FklBigInt t=
	{
		.digits=b->digits,
		.num=0,
		.size=size,
		.const_size=1,
	};
	fklSetBigInt(&t,bi);
	b->num=t.num;
	return r;
}


FklVMvalue* fklCreateVMvalueBigIntWithI64(FklVM* exe
		,int64_t i)
{
	FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
	FklBigInt bi=
	{
		.digits=digits,
		.num=0,
		.size=FKL_MAX_INT64_DIGITS_COUNT,
		.const_size=1,
	};
	fklSetBigIntI(&bi,i);
	return fklCreateVMvalueBigInt2(exe,&bi);
}

FklVMvalue* fklCreateVMvalueBigIntWithU64(FklVM* exe
		,uint64_t u)
{
	FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
	FklBigInt bi=
	{
		.digits=digits,
		.num=0,
		.size=FKL_MAX_INT64_DIGITS_COUNT,
		.const_size=1,
	};
	fklSetBigIntU(&bi,u);
	return fklCreateVMvalueBigInt2(exe,&bi);
}

FklVMvalue* fklCreateVMvalueBigIntWithF64(FklVM* exe
		,double d)
{
	FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
	FklBigInt bi=
	{
		.digits=digits,
		.num=0,
		.size=FKL_MAX_INT64_DIGITS_COUNT,
		.const_size=1,
	};
	fklSetBigIntD(&bi,d);
	return fklCreateVMvalueBigInt2(exe,&bi);
}

FklVMvalue* fklVMbigIntAddI(FklVM* exe,const FklVMbigInt* a,int64_t b)
{
	FklVMvalue* r=fklCreateVMvalueBigIntWithOther2(exe,a,fklAbs(a->num)+1);
	FklVMbigInt* a0=FKL_VM_BI(r);
	FklBigInt a1=
	{
		.digits=a0->digits,
		.num=0,
		.size=fklAbs(a0->num),
		.const_size=1,
	};
	fklSetBigIntWithVMbigInt(&a1,a);
	fklAddBigIntI(&a1,b);
	a0->num=a1.num;
	return r;
}

FklVMvalue* fklVMbigIntSubI(FklVM* exe,const FklVMbigInt* a,int64_t b)
{
	FklVMvalue* r=fklCreateVMvalueBigIntWithOther2(exe,a,fklAbs(a->num)+1);
	FklVMbigInt* a0=FKL_VM_BI(r);
	FklBigInt a1=
	{
		.digits=a0->digits,
		.num=0,
		.size=fklAbs(a0->num),
		.const_size=1,
	};
	fklSetBigIntWithVMbigInt(&a1,a);
	fklSubBigIntI(&a1,b);
	a0->num=a1.num;
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

	FklFuncPrototype* pt=&exe->pts->pa[pid];

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

	FklFuncPrototype* pt=&exe->pts->pa[pid];

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

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(_code_obj_userdata_as_print,code-obj);

static void _code_obj_userdata_finalizer(FklVMud* v)
{
	FKL_DECL_UD_DATA(t,FklByteCodelnt,v);
	fklDestroyByteCode(t->bc);
	if(t->l)
		free(t->l);
}

static FklVMudMetaTable CodeObjUserDataMetaTable=
{
	.size=sizeof(FklByteCodelnt),
	.__as_princ=_code_obj_userdata_as_print,
	.__as_prin1=_code_obj_userdata_as_print,
	.__finalizer=_code_obj_userdata_finalizer,
};

FklVMvalue* fklCreateVMvalueCodeObj(FklVM* exe,FklByteCodelnt* bcl)
{
	FklVMvalue* r=fklCreateVMvalueUd(exe,&CodeObjUserDataMetaTable,NULL);
	*FKL_VM_CO(r)=*bcl;
	free(bcl);
	return r;
}

int fklIsVMvalueCodeObj(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)&&FKL_VM_UD(v)->t==&CodeObjUserDataMetaTable;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(_dll_userdata_as_print,dll);

static void _dll_userdata_atomic(const FklVMud* root,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(dll,FklVMdll,root);
	fklVMgcToGray(dll->pd,gc);
}

static void _dll_userdata_finalizer(FklVMud* v)
{
	FKL_DECL_UD_DATA(dll,FklVMdll,v);
	void (*uninit)(void)=fklGetAddress("_fklUninit",&dll->dll);
	if(uninit)
		uninit();
	uv_dlclose(&dll->dll);
}

static FklVMudMetaTable DllUserDataMetaTable=
{
	.size=sizeof(FklVMdll),
	.__as_princ=_dll_userdata_as_print,
	.__as_prin1=_dll_userdata_as_print,
	.__atomic=_dll_userdata_atomic,
	.__finalizer=_dll_userdata_finalizer,
};

FklVMvalue* fklCreateVMvalueDll(FklVM* exe,const char* dllName,FklVMvalue** errorStr)
{
	size_t len=strlen(dllName)+strlen(FKL_DLL_FILE_TYPE)+1;
	char* realDllName=(char*)malloc(len);
	FKL_ASSERT(realDllName);
	strcpy(realDllName,dllName);
	strcat(realDllName,FKL_DLL_FILE_TYPE);
	char* rpath=fklRealpath(realDllName);
	if(rpath)
	{
		free(realDllName);
		realDllName=rpath;
	}
	uv_lib_t lib;
	if(uv_dlopen(realDllName,&lib))
	{
		*errorStr=fklCreateVMvalueStr(exe,fklCreateStringFromCstr(uv_dlerror(&lib)));
		uv_dlclose(&lib);
		goto err;
	}
	FklVMvalue* r=fklCreateVMvalueUd(exe,&DllUserDataMetaTable,NULL);
	FklVMdll* dll=FKL_VM_DLL(r);
	dll->dll=lib;
	dll->pd=FKL_VM_NIL;
	free(realDllName);
	return r;
err:
	free(realDllName);
	return NULL;
}

int fklIsVMvalueDll(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)&&FKL_VM_UD(v)->t==&DllUserDataMetaTable;
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

FklVMvalue* fklCreateVMvalueUd(FklVM* exe
		,const FklVMudMetaTable* t
		,FklVMvalue* rel)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueUd)+t->size);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_USERDATA;
	FklVMud* ud=FKL_VM_UD(r);
	ud->t=t;
	ud->rel=rel;
	fklAddToGC(r,exe);
	return r;
}

FklVMvalue* fklCreateVMvalueUd2(FklVM* exe
		,const FklVMudMetaTable* t
		,size_t extra_size
		,FklVMvalue* rel)
{
	FklVMvalue* r=(FklVMvalue*)calloc(1,sizeof(FklVMvalueUd)+t->size+extra_size);
	FKL_ASSERT(r);
	r->type=FKL_TYPE_USERDATA;
	FklVMud* ud=FKL_VM_UD(r);
	ud->t=t;
	ud->rel=rel;
	fklAddToGC(r,exe);
	return r;
}

#undef NEW_OBJ

static void _eof_userdata_as_print(const FklVMud* ud,FklStringBuffer* buf,FklVMgc* gc)
{
	fklStringBufferConcatWithCstr(buf,"#<eof>");
}

static FklVMudMetaTable EofUserDataMetaTable=
{
	.size=0,
	.__as_princ=_eof_userdata_as_print,
	.__as_prin1=_eof_userdata_as_print,
};

static const alignas(8) FklVMvalueUd EofUserDataValue=
{
	.mark=FKL_MARK_B,
	.next=NULL,
	.type=FKL_TYPE_USERDATA,
	.ud=
	{
		.t=&EofUserDataMetaTable,
		.rel=NULL,
	},
};

FklVMvalue* fklGetVMvalueEof(void)
{
	return (FklVMvalue*)&EofUserDataValue;
}

int fklIsVMeofUd(FklVMvalue* v)
{
	return v==(FklVMvalue*)&EofUserDataValue;
}

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
	FklVMud* ud=FKL_VM_UD(root);
	fklVMgcToGray(ud->rel,gc);
	if(ud->t->__atomic)
		ud->t->__atomic(ud,gc);
}

int fklIsCallableUd(const FklVMud* ud)
{
	return ud->t->__call!=NULL;
}

int fklIsCallable(FklVMvalue* v)
{
	return FKL_IS_PROC(v)||FKL_IS_CPROC(v)||(FKL_IS_USERDATA(v)&&fklIsCallableUd(FKL_VM_UD(v)));
}

int fklIsCmpableUd(const FklVMud* u)
{
	return u->t->__cmp!=NULL;
}

int fklIsWritableUd(const FklVMud* u)
{
	return u->t->__write!=NULL;
}

int fklIsAbleToStringUd(const FklVMud* u)
{
	return u->t->__as_prin1!=NULL;
}

int fklIsAbleAsPrincUd(const FklVMud* u)
{
	return u->t->__as_princ!=NULL;
}

int fklUdHasLength(const FklVMud* u)
{
	return u->t->__length!=NULL;
}

int fklCmpVMud(const FklVMud* a,const FklVMvalue* b,int* err)
{
	return a->t->__cmp(a,b,err);
}

size_t fklLengthVMud(const FklVMud* a)
{
	return a->t->__length(a);
}

void fklUdAsPrin1(const FklVMud* a,FklStringBuffer* buf,FklVMgc* gc)
{
	a->t->__as_prin1(a,buf,gc);
}

void fklUdAsPrinc(const FklVMud* a,FklStringBuffer* buf,FklVMgc* gc)
{
	a->t->__as_princ(a,buf,gc);
}

void fklWriteVMud(const FklVMud* a,FILE* fp)
{
	a->t->__write(a,fp);
}

size_t fklHashvVMud(const FklVMud* a,FklPtrStack* s)
{
	size_t (*hashv)(const FklVMud*,FklPtrStack*)=a->t->__hash;
	if(hashv)
		return hashv(a,s);
	else
		return ((uintptr_t)a->data>>FKL_UNUSEDBITNUM);
}

void fklFinalizeVMud(FklVMud* a)
{
	void (*finalize)(FklVMud*)=a->t->__finalizer;
	if(finalize)
		finalize(a);
}

void* fklVMvalueTerminalCreate(const char* s,size_t len,size_t line,void* ctx)
{
	FklVM* exe=(FklVM*)ctx;
	return fklCreateVMvalueStr2(exe,len,s);
}

void fklVMvalueTerminalDestroy(void* ast)
{
}

