#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
//#include<fakeLisp/ast.h>
//#include<fakeLisp/compiler.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<float.h>
#include<pthread.h>
#ifndef _WIN32
#include<dlfcn.h>
#else
#include<tchar.h>
#endif

FklVMvalue* fklMakeVMf64(double f,FklVMstack* s,FklVMgc* gc)
{
	return fklCreateVMvalueToStack(FKL_TYPE_F64,&f,s,gc);
}

FklVMvalue* fklMakeVMint(int64_t r64,FklVMstack* s,FklVMgc* gc)
{
	if(r64>INT32_MAX||r64<INT32_MIN)
		return fklCreateVMvalueToStack(FKL_TYPE_I64,&r64,s,gc);
	else
		return FKL_MAKE_VM_I32(r64);
}

FklVMvalue* fklMakeVMuint(uint64_t r64,FklVMstack* s,FklVMgc* gc)
{
	if(r64>INT64_MAX)
	{
		FklBigInt* bi=fklCreateBigIntU(r64);
		return fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,s,gc);
	}
	else if(r64>INT32_MAX)
		return fklCreateVMvalueToStack(FKL_TYPE_I64,&r64,s,gc);
	else
		return FKL_MAKE_VM_I32(r64);
}

FklVMvalue* fklMakeVMintD(double r64,FklVMstack* s,FklVMgc* gc)
{
	if(r64-INT32_MAX>DBL_EPSILON||r64-INT32_MIN<-DBL_EPSILON)
		return fklCreateVMvalueToStack(FKL_TYPE_I64,&r64,s,gc);
	else
		return FKL_MAKE_VM_I32(r64);
}

inline int fklIsFixint(const FklVMvalue* p)
{
	return FKL_IS_I32(p)||FKL_IS_I64(p);
}

inline int fklIsInt(const FklVMvalue* p)
{
	return FKL_IS_I32(p)||FKL_IS_I64(p)||FKL_IS_BIG_INT(p);
}

inline int fklIsVMnumber(const FklVMvalue* p)
{
	return fklIsInt(p)||FKL_IS_F64(p);
}

int fklIsList(const FklVMvalue* p)
{
	for(;p!=FKL_VM_NIL;p=p->u.pair->cdr)
		if(!FKL_IS_PAIR(p))
			return 0;
	return 1;
}

inline int64_t fklGetInt(const FklVMvalue* p)
{
	return FKL_IS_I32(p)
		?FKL_GET_I32(p)
		:(FKL_IS_I64(p))?p->u.i64
		:fklBigIntToI64(p->u.bigInt);
}

inline uint64_t fklGetUint(const FklVMvalue* p)
{
	return FKL_IS_I32(p)
		?FKL_GET_I32(p)
		:(FKL_IS_I64(p))?p->u.i64
		:fklBigIntToU64(p->u.bigInt);
}

inline int fklVMnumberLt0(const FklVMvalue* p)
{
	return fklIsFixint(p)
		?fklGetInt(p)<0
		:FKL_IS_F64(p)
		?0.0-p->u.f64>DBL_EPSILON
		:!FKL_IS_0_BIG_INT(p->u.bigInt)&&p->u.bigInt->neg;
}

inline double fklGetDouble(const FklVMvalue* p)
{
	return FKL_IS_I32(p)?FKL_GET_I32(p)
		:(FKL_IS_I64(p))?p->u.i64
		:(FKL_IS_BIG_INT(p))?fklBigIntToDouble(p->u.bigInt)
		:p->u.f64;
}

FklVMvalue* fklPopVMstack(FklVMstack* stack)
{
	if(!(stack->tp>stack->bp))
		return NULL;
	FklVMvalue* tmp=fklGetTopValue(stack);
	stack->tp-=1;
	return tmp;
}

FklVMvalue* fklTopGet(FklVMstack* stack)
{
//	pthread_rwlock_wrlock(&stack->lock);
	FklVMvalue* r=NULL;
	if(!(stack->tp>stack->bp))
		r=NULL;
	else
	{
		FklVMvalue* tmp=fklGetTopValue(stack);
		r=tmp;
	}
//	pthread_rwlock_unlock(&stack->lock);
	return r;
}

void fklDecTop(FklVMstack* stack)
{
//	pthread_rwlock_wrlock(&stack->lock);
	stack->tp--;
//	pthread_rwlock_unlock(&stack->lock);
}

//typedef struct
//{
//	FklVMvalue* key;
//	uint64_t line;
//}LineNumHashItem;
//
//static size_t _LineNumHash_hashFunc(void* pKey)
//{
//	FklVMvalue* key=*(FklVMvalue**)pKey;
//	return (uintptr_t)FKL_GET_PTR(key)>>FKL_UNUSEDBITNUM;
//}
//
//static int _LineNumHash_keyEqual(void* pKey0,void* pKey1)
//{
//	FklVMvalue* key0=*(FklVMvalue**)pKey0;
//	FklVMvalue* key1=*(FklVMvalue**)pKey1;
//	return key0==key1;
//}
//
//static void _LineNumHash_freeItem(void* item)
//{
//	free(item);
//}
//
//static void* _LineNumHash_getKey(void* item)
//{
//	return &((LineNumHashItem*)item)->key;
//}
//
//static FklHashTableMethodTable LineNumHashMethTable=
//{
//	.__hashFunc=_LineNumHash_hashFunc,
//	.__freeItem=_LineNumHash_freeItem,
//	.__keyEqual=_LineNumHash_keyEqual,
//	.__getKey=_LineNumHash_getKey,
//};
//
//FklHashTable* fklCreateLineNumHashTable(void)
//{
//	FklHashTable* lineHash=fklCreateHashTable(32,8,2,0.75,1,&LineNumHashMethTable);
//	return lineHash;
//}
//
//static LineNumHashItem* createLineNumHashItem(FklVMvalue* value,FklVMvalue* refBy,uint64_t lineNum)
//{
//	LineNumHashItem* item=(LineNumHashItem*)malloc(sizeof(LineNumHashItem));
//	FKL_ASSERT(item);
//	item->key=value;
//	item->line=lineNum;
//	return item;
//}

//#define SENTINEL_CPTR (NULL)
//FklVMvalue* fklCastCptrVMvalue(FklAstCptr* objCptr,FklHashTable* lineHash,FklVMgc* gc)
//{
//	FklPtrStack* cptrStack=fklCreatePtrStack(32,16);
//	FklUintStack* reftypeStack=fklCreateUintStack(32,16);
//	FklPtrStack* valueStack=fklCreatePtrStack(32,16);
//	FklPtrStack* stackStack=fklCreatePtrStack(32,16);
//	fklPushPtrStack(objCptr,cptrStack);
//	fklPushPtrStack(valueStack,stackStack);
//	while(!fklIsPtrStackEmpty(cptrStack))
//	{
//		FklAstCptr* root=fklPopPtrStack(cptrStack);
//		FklPtrStack* cStack=fklTopPtrStack(stackStack);
//		if(root==SENTINEL_CPTR)
//		{
//			fklPopPtrStack(stackStack);
//			FklPtrStack* tStack=fklTopPtrStack(stackStack);
//			FklValueType type=fklPopUintStack(reftypeStack);
//			uint64_t line=fklPopUintStack(reftypeStack);
//			FklVMvalue* v=fklCreateVMvalueNoGC(type,NULL,gc);
//			fklPushPtrStack(v,tStack);
//			if(lineHash)
//				fklPutReplHashItem(createLineNumHashItem(v,NULL,line),lineHash);
//			switch(type)
//			{
//				case FKL_TYPE_BOX:
//					v->u.box=fklPopPtrStack(cStack);
//					break;
//				case FKL_TYPE_PAIR:
//					{
//						FklVMpair* pair=fklCreateVMpair();
//						size_t top=cStack->top;
//						pair->car=cStack->base[top-1];
//						pair->cdr=cStack->base[top-2];
//						v->u.pair=pair;
//					}
//					break;
//				case FKL_TYPE_VECTOR:
//					{
//						v->u.vec=fklCreateVMvecNoInit(cStack->top);
//						for(size_t i=cStack->top,j=0;i>0;i--,j++)
//							v->u.vec->base[j]=cStack->base[i-1];
//					}
//					break;
//				case FKL_TYPE_HASHTABLE:
//					{
//						FklVMhashTable* hash=fklCreateVMhashTable(fklPopUintStack(reftypeStack));
//						v->u.hash=hash;
//						for(size_t i=cStack->top;i>0;i-=2)
//						{
//							FklVMvalue* key=cStack->base[i-1];
//							FklVMvalue* v=cStack->base[i-2];
//							fklSetVMhashTable(key,v,hash,gc);
//						}
//					}
//					break;
//				default:
//					FKL_ASSERT(0);
//					break;
//			}
//			fklFreePtrStack(cStack);
//			cStack=tStack;
//		}
//		else
//		{
//			if(root->type==FKL_TYPE_ATM)
//			{
//				FklAstAtom* tmpAtm=root->u.atom;
//				switch(tmpAtm->type)
//				{
//					case FKL_TYPE_I32:
//						fklPushPtrStack(FKL_MAKE_VM_I32(tmpAtm->value.i32),cStack);
//						break;
//					case FKL_TYPE_CHR:
//						fklPushPtrStack(FKL_MAKE_VM_CHR(tmpAtm->value.chr),cStack);
//						break;
//					case FKL_TYPE_SYM:
//						fklPushPtrStack(FKL_MAKE_VM_SYM(fklAddSymbolToGlob(tmpAtm->value.str)->id),cStack);
//						break;
//					case FKL_TYPE_F64:
//						fklPushPtrStack(fklCreateVMvalueNoGC(FKL_TYPE_F64,&tmpAtm->value.f64,gc),cStack);
//						break;
//					case FKL_TYPE_STR:
//						fklPushPtrStack(fklCreateVMvalueNoGC(FKL_TYPE_STR
//									,fklCreateString(tmpAtm->value.str->size,tmpAtm->value.str->str)
//									,gc),cStack);
//						break;
//					case FKL_TYPE_BYTEVECTOR:
//						fklPushPtrStack(fklCreateVMvalueNoGC(FKL_TYPE_BYTEVECTOR
//									,fklCreateBytevector(tmpAtm->value.bvec->size,tmpAtm->value.bvec->ptr)
//									,gc),cStack);
//						break;
//					case FKL_TYPE_BIG_INT:
//						fklPushPtrStack(fklCreateVMvalueNoGC(FKL_TYPE_BIG_INT,fklCopyBigInt(&tmpAtm->value.bigInt),gc),cStack);
//						break;
//					case FKL_TYPE_BOX:
//						{
//							FklPtrStack* bStack=fklCreatePtrStack(1,16);
//							fklPushPtrStack(bStack,stackStack);
//							cStack=bStack;
//							fklPushUintStack(root->curline,reftypeStack);
//							fklPushUintStack(FKL_TYPE_BOX,reftypeStack);
//							fklPushPtrStack(SENTINEL_CPTR,cptrStack);
//							fklPushPtrStack(&tmpAtm->value.box,cptrStack);
//						}
//						break;
//					case FKL_TYPE_VECTOR:
//						{
//							fklPushUintStack(root->curline,reftypeStack);
//							fklPushUintStack(FKL_TYPE_VECTOR,reftypeStack);
//							fklPushPtrStack(SENTINEL_CPTR,cptrStack);
//							for(size_t i=0;i<tmpAtm->value.vec.size;i++)
//								fklPushPtrStack(&tmpAtm->value.vec.base[i],cptrStack);
//							FklPtrStack* vStack=fklCreatePtrStack(tmpAtm->value.vec.size,16);
//							fklPushPtrStack(vStack,stackStack);
//							cStack=vStack;
//						}
//						break;
//					case FKL_TYPE_HASHTABLE:
//						{
//							fklPushUintStack(root->u.atom->value.hash.type,reftypeStack);
//							fklPushUintStack(root->curline,reftypeStack);
//							fklPushUintStack(FKL_TYPE_HASHTABLE,reftypeStack);
//							fklPushPtrStack(SENTINEL_CPTR,cptrStack);
//							for(FklAstCptr* cptr=fklGetFirstCptr(&tmpAtm->value.hash.items);cptr;cptr=fklNextCptr(cptr))
//							{
//								fklPushPtrStack(fklGetASTPairCar(cptr),cptrStack);
//								fklPushPtrStack(fklGetASTPairCdr(cptr),cptrStack);
//							}
//							FklPtrStack* hStack=fklCreatePtrStack(32,16);
//							fklPushPtrStack(hStack,stackStack);
//							cStack=hStack;
//						}
//						break;
//					default:
//						return NULL;
//						break;
//				}
//			}
//			else if(root->type==FKL_TYPE_PAIR)
//			{
//				FklPtrStack* pStack=fklCreatePtrStack(2,16);
//				fklPushPtrStack(pStack,stackStack);
//				cStack=pStack;
//				fklPushUintStack(root->curline,reftypeStack);
//				fklPushUintStack(FKL_TYPE_PAIR,reftypeStack);
//				fklPushPtrStack(SENTINEL_CPTR,cptrStack);
//				fklPushPtrStack(fklGetASTPairCar(root),cptrStack);
//				fklPushPtrStack(fklGetASTPairCdr(root),cptrStack);
//			}
//			else
//				fklPushPtrStack(FKL_VM_NIL,cStack);
//		}
//	}
//	FklVMvalue* retval=fklTopPtrStack(valueStack);
//	fklFreePtrStack(stackStack);
//	fklFreePtrStack(cptrStack);
//	fklFreePtrStack(valueStack);
//	fklFreeUintStack(reftypeStack);
//	return retval;
//}

//FklVMvalue* fklCastPreEnvToVMenv(FklPreEnv* pe
//		,FklVMvalue* prev
//		,FklHashTable* lineNumberHash
//		,FklVMgc* gc)
//{
//	int32_t size=0;
//	FklPreDef* tmpDef=pe->symbols;
//	while(tmpDef)
//	{
//		size++;
//		tmpDef=tmpDef->next;
//	}
//	FklVMenv* tmp=fklCreateVMenv(prev,gc);
//	for(tmpDef=pe->symbols;tmpDef;tmpDef=tmpDef->next)
//		fklSetRef(fklFindOrAddVar(fklAddSymbolToGlob(tmpDef->symbol)->id,tmp),fklCastCptrVMvalue(&tmpDef->obj,lineNumberHash,gc),gc);
//	return fklCreateVMvalueNoGC(FKL_TYPE_ENV,tmp,gc);
//}

FklVMstack* fklCopyStack(FklVMstack* stack)
{
	int32_t i=0;
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp);
//	pthread_rwlock_rdlock(&stack->lock);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
//	pthread_rwlock_init(&tmp->lock,NULL);
	tmp->values=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*(tmp->size));
	FKL_ASSERT(tmp->values);
	for(;i<stack->tp;i++)
		tmp->values[i]=stack->values[i];
	tmp->tps=fklCreateUintStackFromStack(stack->tps);
	tmp->bps=fklCreateUintStackFromStack(stack->bps);
//	pthread_rwlock_unlock(&stack->lock);
	return tmp;
}

FklVMtryBlock* fklCreateVMtryBlock(FklSid_t sid,uint32_t tp,FklVMframe* frame)
{
	FklVMtryBlock* t=(FklVMtryBlock*)malloc(sizeof(FklVMtryBlock));
	FKL_ASSERT(t);
	t->sid=sid;
	t->hstack=fklCreatePtrStack(32,16);
	t->tp=tp;
	t->curFrame=frame;
	return t;
}

void fklFreeVMtryBlock(FklVMtryBlock* b)
{
	FklPtrStack* hstack=b->hstack;
	while(!fklIsPtrStackEmpty(hstack))
	{
		FklVMerrorHandler* h=fklPopPtrStack(hstack);
		fklFreeVMerrorHandler(h);
	}
	fklFreePtrStack(b->hstack);
	free(b);
}

FklVMerrorHandler* fklCreateVMerrorHandler(FklSid_t* typeIds,uint32_t errTypeNum,uint64_t scp,uint64_t cpc)
{
	FklVMerrorHandler* t=(FklVMerrorHandler*)malloc(sizeof(FklVMerrorHandler));
	FKL_ASSERT(t);
	t->typeIds=typeIds;
	t->num=errTypeNum;
	t->proc.prevEnv=NULL;
	t->proc.scp=scp;
	t->proc.cpc=cpc;
	t->proc.sid=0;
	return t;
}

void fklFreeVMerrorHandler(FklVMerrorHandler* h)
{
	free(h->typeIds);
	free(h);
}

static int isShouldBeHandle(FklSid_t* typeIds,uint32_t num,FklSid_t type)
{
	if(num==0)
		return 1;
	for(uint32_t i=0;i<num;i++)
		if(typeIds[i]==type)
			return 1;
	return 0;
}

int fklRaiseVMerror(FklVMvalue* ev,FklVM* exe)
{
	FklVMerror* err=ev->u.err;
	while(!fklIsPtrStackEmpty(exe->tstack))
	{
		FklVMtryBlock* tb=fklTopPtrStack(exe->tstack);
		FklVMstack* stack=exe->stack;
		stack->tp=tb->tp;
		while(!fklIsPtrStackEmpty(tb->hstack))
		{
			FklVMerrorHandler* h=fklPopPtrStack(tb->hstack);
			if(isShouldBeHandle(h->typeIds,h->num,err->type))
			{
				FklVMframe* curr=tb->curFrame;
				for(FklVMframe* other=exe->frames;other!=curr;)
				{
					FklVMframe* frame=other;
					other=other->prev;
					free(frame);
				}
				exe->frames=curr;
				FklVMframe* prevFrame=exe->frames;
				FklVMframe* frame=fklCreateVMframe(&h->proc,prevFrame);
				frame->localenv=fklCreateSaveVMvalue(FKL_TYPE_ENV,fklCreateVMenv(prevFrame->localenv,exe->gc));
				fklAddToGC(frame->localenv,exe->gc);
				FklVMvalue* curEnv=frame->localenv;
				FklSid_t idOfError=tb->sid;
				fklSetRef(fklFindOrAddVar(idOfError,curEnv->u.env),ev,exe->gc);
				fklFreeVMerrorHandler(h);
				exe->frames=frame;
				return 1;
			}
			fklFreeVMerrorHandler(h);
		}
		fklFreeVMtryBlock(fklPopPtrStack(exe->tstack));
	}
	fprintf(stderr,"error of ");
	fklPrintString(err->who,stderr);
	fprintf(stderr," :");
	fklPrintString(err->message,stderr);
	fprintf(stderr,"\n");
	for(FklVMframe* cur=exe->frames;cur;cur=cur->prev)
	{
		if(cur->sid!=0)
		{
			fprintf(stderr,"at proc:");
			fklPrintString(fklGetGlobSymbolWithId(cur->sid)->symbol,stderr);
		}
		else
		{
			if(cur->prev)
				fprintf(stderr,"at <lambda>");
			else
				fprintf(stderr,"at <top>");
		}
		FklLineNumTabNode* node=fklFindLineNumTabNode(cur->cp,exe->lnt);
		if(node->fid)
		{
			fprintf(stderr,"(%u:",node->line);
			fklPrintString(fklGetGlobSymbolWithId(node->fid)->symbol,stderr);
			fprintf(stderr,")\n");
		}
		else
			fprintf(stderr,"(%u)\n",node->line);
	}
	void* i[3]={exe,(void*)255,(void*)1};
	exe->callback(i);
	return 255;
}

FklVMframe* fklCreateVMframe(FklVMproc* code,FklVMframe* prev)
{
	FklVMframe* tmp=(FklVMframe*)malloc(sizeof(FklVMframe));
	FKL_ASSERT(tmp);
	tmp->sid=0;
	tmp->cp=0;
	tmp->scp=0;
	tmp->cpc=0;
	tmp->prev=prev;
	tmp->ccc=NULL;
	if(code)
	{
		tmp->cp=code->scp;
		tmp->scp=code->scp;
		tmp->cpc=code->cpc;
		tmp->sid=code->sid;
	}
	tmp->mark=0;
	return tmp;
}

void fklFreeVMframe(FklVMframe* frame)
{
	FklVMcCC* curCCC=frame->ccc;
	while(curCCC)
	{
		FklVMcCC* cur=curCCC;
		curCCC=cur->next;
		fklFreeVMcCC(cur);
	}
	free(frame);
}

FklVMcCC* fklCreateVMcCC(FklVMFuncK kFunc,void* ctx,size_t size,FklVMcCC* next)
{
	FklVMcCC* r=(FklVMcCC*)malloc(sizeof(FklVMcCC));
	FKL_ASSERT(r);
	r->kFunc=kFunc;
	r->ctx=ctx;
	r->size=size;
	r->next=next;
	return r;
}

FklVMcCC* fklCopyVMcCC(FklVMcCC* ccc)
{
	FklVMcCC* r=NULL;
	FklVMcCC** pr=&r;
	for(FklVMcCC* curCCC=ccc;curCCC;curCCC=curCCC->next,pr=&(*pr)->next)
		*pr=fklCreateVMcCC(curCCC->kFunc,fklCopyMemory(curCCC->ctx,curCCC->size),curCCC->size,NULL);
	return r;
}

void fklFreeVMcCC(FklVMcCC* cc)
{
	free(cc->ctx);
	free(cc);
}

char* fklGenInvalidSymbolErrorMessage(char* str,int _free,FklBuiltInErrorType type)
{
	char* t=fklCopyCstr("");
	switch(type)
	{
		case FKL_ERR_LOADDLLFAILD:
			t=fklStrCat(t,"Faild to load dll \"");
			t=fklStrCat(t,str);
			t=fklStrCat(t,"\"");
			break;
		case FKL_ERR_INVALIDSYMBOL:
			t=fklStrCat(t,"Invalid symbol ");
			t=fklStrCat(t,str);
			break;
		case FKL_ERR_FILEFAILURE:
			t=fklStrCat(t,"Failed for file:\"");
			t=fklStrCat(t,str);
			t=fklStrCat(t,"\"");
		case FKL_ERR_SYMUNDEFINE:
			t=fklStrCat(t,"Symbol ");
			t=fklStrCat(t,str);
			t=fklStrCat(t," is undefined");
			break;
		default:
			break;
	}
	if(_free)
		free(str);
	return t;
}

char* fklGenErrorMessage(FklBuiltInErrorType type,FklVMframe* frame,FklVM* exe)
{
	char* t=fklCopyCstr("");
	switch(type)
	{
		case FKL_ERR_INCORRECT_TYPE_VALUE:
			t=fklStrCat(t,"Incorrect type of values");
			break;
		case FKL_ERR_STACKERROR:
			t=fklStrCat(t,"Stack error");
			break;
		case FKL_ERR_TOOMANYARG:
			t=fklStrCat(t,"Too many arguements");
			break;
		case FKL_ERR_TOOFEWARG:
			t=fklStrCat(t,"Too few arguements");
			break;
		case FKL_ERR_CANTCREATETHREAD:
			t=fklStrCat(t,"Can't create thread");
			break;
		case FKL_ERR_SYMUNDEFINE:
			t=fklStrCat(t,"Symbol ");
			t=fklCstrStringCat(t,fklGetGlobSymbolWithId(fklGetSymbolIdInByteCode(exe->code+frame->cp))->symbol);
			t=fklStrCat(t," is undefined");
			break;
		case FKL_ERR_CALL_ERROR:
			t=fklStrCat(t,"Try to call an object that can't be call");
			break;
		case FKL_ERR_CROSS_C_CALL_CONTINUATION:
			t=fklStrCat(t,"attempt to get a continuation cross C-call boundary");
			break;
		case FKL_ERR_LOADDLLFAILD:
			t=fklStrCat(t,"Faild to load dll \"");
			{
				FklVMvalue* v=exe->stack->values[exe->stack->tp-1];
				char* str=fklStringToCstr(v->u.str);
				t=fklStrCat(t,str);
				free(str);
			}
			t=fklStrCat(t,"\"");
			break;
		case FKL_ERR_INVALIDSYMBOL:
			t=fklStrCat(t,"Invalid symbol ");
			{
				FklVMvalue* v=exe->stack->values[exe->stack->tp-1];
				char* str=fklStringToCstr(v->u.str);
				t=fklStrCat(t,str);
				free(str);
			}
			break;
		case FKL_ERR_DIVZEROERROR:
			t=fklStrCat(t,"Divided by zero");
			break;
		case FKL_ERR_FILEFAILURE:
			t=fklStrCat(t,"Failed for file:\"");
			{
				FklVMvalue* v=exe->stack->values[exe->stack->tp-1];
				char* str=fklStringToCstr(v->u.str);
				t=fklStrCat(t,str);
				free(str);
			}
			t=fklStrCat(t,"\"");
			break;
		case FKL_ERR_NO_VALUE_FOR_KEY:
			t=fklStrCat(t,"No value for key");
			break;
		case FKL_ERR_INVALIDASSIGN:
			t=fklStrCat(t,"Invalid assign");
			break;
		case FKL_ERR_INVALIDACCESS:
			t=fklStrCat(t,"Invalid access");
			break;
		case FKL_ERR_UNEXPECTEOF:
			t=fklStrCat(t,"Unexpected eof");
			break;
		case FKL_ERR_FAILD_TO_CREATE_BIG_INT_FROM_MEM:
			t=fklStrCat(t,"Failed to create big-int from mem");
			break;
		case FKL_ERR_LIST_DIFFER_IN_LENGTH:
			t=fklStrCat(t,"List differ in length");
			break;
		case FKL_ERR_INVALIDRADIX:
			t=fklStrCat(t,"Radix should be 8,10 or 16");
			break;
		case FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0:
			t=fklStrCat(t,"Number should not be less than 0");
			break;
		case FKL_ERR_INVALID_VALUE:
			t=fklStrCat(t,"Invalid value");
			break;
		default:
			break;
	}
	return t;
}

int32_t fklGetSymbolIdInByteCode(const uint8_t* code)
{
	char op=*code;
	switch(op)
	{
		case FKL_OP_PUSH_VAR:
			return *(int32_t*)(code+sizeof(char));
			break;
		case FKL_OP_POP_VAR:
		case FKL_OP_POP_ARG:
		case FKL_OP_POP_REST_ARG:
			return *(int32_t*)(code+sizeof(char)+sizeof(int32_t));
			break;
	}
	return -1;
}

typedef struct PrtElem
{
	enum PrintingState
	{
		PRT_CAR,
		PRT_CDR,
		PRT_REC_CAR,
		PRT_REC_CDR,
		PRT_BOX,
		PRT_REC_BOX,
		PRT_HASH_ITEM,
	}state;
	FklVMvalue* v;
}PrtElem;

static PrtElem* createPrtElem(enum PrintingState state,FklVMvalue* v)
{
	PrtElem* r=(PrtElem*)malloc(sizeof(PrtElem));
	FKL_ASSERT(r);
	r->state=state;
	r->v=v;
	return r;
}

static int isInStack(FklVMvalue* v,FklPtrStack* stack,size_t* w)
{
	for(size_t i=0;i<stack->top;i++)
		if(stack->base[i]==v)
		{
			if(w)
				*w=i;
			return 1;
		}
	return 0;
}

static void scanCirRef(FklVMvalue* s,FklPtrStack* recStack)
{
	FklPtrStack* beAccessed=fklCreatePtrStack(32,16);
	FklPtrStack* totalAccessed=fklCreatePtrStack(32,16);
	fklPushPtrStack(s,beAccessed);
	while(!fklIsPtrStackEmpty(beAccessed))
	{
		FklVMvalue* v=fklPopPtrStack(beAccessed);
		if(FKL_IS_PAIR(v)||FKL_IS_VECTOR(v)||FKL_IS_BOX(v)||FKL_IS_HASHTABLE(v))
		{
			if(isInStack(v,totalAccessed,NULL))
			{
				fklPushPtrStack(v,recStack);
				continue;
			}
			if(isInStack(v,recStack,NULL))
				continue;
			fklPushPtrStack(v,totalAccessed);
			if(FKL_IS_PAIR(v))
			{
				if(v->u.pair->cdr==v||v->u.pair->car==v)
					fklPushPtrStack(v,recStack);
				else
				{
					fklPushPtrStack(v->u.pair->cdr,beAccessed);
					fklPushPtrStack(v->u.pair->car,beAccessed);
				}
			}
			else if(FKL_IS_VECTOR(v))
			{
				FklVMvec* vec=v->u.vec;
				for(size_t i=vec->size;i>0;i--)
				{
					if(vec->base[i-1]==v)
						fklPushPtrStack(v,recStack);
					else
						fklPushPtrStack(vec->base[i-1],beAccessed);
				}
			}
			else if(FKL_IS_HASHTABLE(v))
			{
				FklVMhashTable* ht=v->u.hash;
				FklPtrStack* stack=fklCreatePtrStack(32,16);
				for(FklHashTableNodeList* list=ht->ht->list;list;list=list->next)
					fklPushPtrStack(list->node->item,stack);
				while(!fklIsPtrStackEmpty(stack))
				{
					FklVMhashTableItem* item=fklPopPtrStack(stack);
					if(item->v==v||item->key==v)
						fklPushPtrStack(v,recStack);
					else
					{
						fklPushPtrStack(item->v,beAccessed);
						fklPushPtrStack(item->key,beAccessed);
					}
				}
				fklFreePtrStack(stack);
			}
			else
			{
				if(v->u.box==v)
					fklPushPtrStack(v,recStack);
				else
					fklPushPtrStack(v->u.box,beAccessed);
			}
		}
	}
	fklFreePtrStack(totalAccessed);
	fklFreePtrStack(beAccessed);
}

static void princVMatom(FklVMvalue* v,FILE* fp)
{
	FklVMptrTag tag=FKL_GET_TAG(v);
	switch(tag)
	{
		case FKL_TAG_NIL:
			fprintf(fp,"()");
			break;
		case FKL_TAG_I32:
			fprintf(fp,"%d",FKL_GET_I32(v));
			break;
		case FKL_TAG_CHR:
			putc(FKL_GET_CHR(v),fp);
			break;
		case FKL_TAG_SYM:
			fklPrintString(fklGetGlobSymbolWithId(FKL_GET_SYM(v))->symbol,fp);
			break;
		case FKL_TAG_PTR:
			{
				switch(v->type)
				{
					case FKL_TYPE_F64:
						fprintf(fp,"%lf",v->u.f64);
						break;
					case FKL_TYPE_I64:
						fprintf(fp,"%ld",v->u.i64);
						break;
					case FKL_TYPE_STR:
						fwrite(v->u.str->str,v->u.str->size,1,fp);
						break;
					case FKL_TYPE_BYTEVECTOR:
						fklPrintRawBytevector(v->u.bvec,fp);
						break;
					case FKL_TYPE_PROC:
						if(v->u.proc->sid)
						{
							fprintf(fp,"<#proc: ");
							fklPrintString(fklGetGlobSymbolWithId(v->u.proc->sid)->symbol,fp);
							fputc('>',fp);
						}
						else
							fprintf(fp,"#<proc>");
						break;
					case FKL_TYPE_CONT:
						fprintf(fp,"#<cont>");
						break;
					case FKL_TYPE_CHAN:
						fprintf(fp,"#<chan>");
						break;
					case FKL_TYPE_FP:
						fprintf(fp,"#<fp>");
						break;
					case FKL_TYPE_DLL:
						fprintf(fp,"<#dll>");
						break;
					case FKL_TYPE_ENV:
						fprintf(fp,"#<env>");
						break;
					case FKL_TYPE_DLPROC:
						if(v->u.dlproc->sid)
						{
							fprintf(fp,"<#dlproc: ");
							fklPrintString(fklGetGlobSymbolWithId(v->u.dlproc->sid)->symbol,fp);
							fputc('>',fp);
						}
						else
							fprintf(fp,"<#dlproc>");
						break;
					case FKL_TYPE_ERR:
						fklPrintString(v->u.err->message,fp);
						break;
					case FKL_TYPE_BIG_INT:
						fklPrintBigInt(v->u.bigInt,fp);
						break;
					case FKL_TYPE_USERDATA:
						if(v->u.ud->t->__princ)
							v->u.ud->t->__princ(v->u.ud->data,fp);
						else
						{
							fprintf(fp,"#<");
							fklPrintString(fklGetGlobSymbolWithId(v->u.ud->type)->symbol,fp);
							fprintf(fp,":%p>",v->u.ud);
						}
						break;
					default:
						fputs("#<unknown>",fp);
						break;
				}
			}
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

static void prin1VMatom(FklVMvalue* v,FILE* fp)
{
	FklVMptrTag tag=FKL_GET_TAG(v);
	switch(tag)
	{
		case FKL_TAG_NIL:
			fputs("()",fp);
			break;
		case FKL_TAG_I32:
			fprintf(fp,"%d",FKL_GET_I32(v));
			break;
		case FKL_TAG_CHR:
			fklPrintRawChar(FKL_GET_CHR(v),fp);
			break;
		case FKL_TAG_SYM:
			fklPrintRawSymbol(fklGetGlobSymbolWithId(FKL_GET_SYM(v))->symbol,fp);
			break;
		case FKL_TAG_PTR:
			{
				switch(v->type)
				{
					case FKL_TYPE_F64:
						fprintf(fp,"%lf",v->u.f64);
						break;
					case FKL_TYPE_I64:
						fprintf(fp,"%ld",v->u.i64);
						break;
					case FKL_TYPE_STR:
						fklPrintRawString(v->u.str,fp);
						break;
					case FKL_TYPE_BYTEVECTOR:
						fklPrintRawBytevector(v->u.bvec,fp);
						break;
					case FKL_TYPE_PROC:
						if(v->u.proc->sid)
						{
							fprintf(fp,"<#proc: ");
							fklPrintString(fklGetGlobSymbolWithId(v->u.proc->sid)->symbol,fp);
							fputc('>',fp);
						}					else
							fputs("#<proc>",fp);
						break;
					case FKL_TYPE_CONT:
						fputs("#<continuation>",fp);
						break;
					case FKL_TYPE_CHAN:
						fputs("#<chanl>",fp);
						break;
					case FKL_TYPE_FP:
						fputs("#<fp>",fp);
						break;
					case FKL_TYPE_DLL:
						fputs("#<dll>",fp);
						break;
					case FKL_TYPE_DLPROC:
						if(v->u.dlproc->sid){
							fprintf(fp,"<#dlproc: ");
							fklPrintString(fklGetGlobSymbolWithId(v->u.dlproc->sid)->symbol,fp);
							fputc('>',fp);
						}
						else
							fputs("#<dlproc>",fp);
						break;
					case FKL_TYPE_ENV:
						fprintf(fp,"#<env>");
						break;
					case FKL_TYPE_ERR:
						fprintf(fp,"#<err w:");
						fklPrintString(v->u.err->who,fp);
						fprintf(fp," t:");
						fklPrintString(fklGetGlobSymbolWithId(v->u.err->type)->symbol,fp);
						fprintf(fp," m:");
						fklPrintString(v->u.err->message,fp);
						fprintf(fp,">");
						break;
					case FKL_TYPE_BIG_INT:
						fklPrintBigInt(v->u.bigInt,fp);
						break;
					case FKL_TYPE_USERDATA:
						if(v->u.ud->t->__prin1)
							v->u.ud->t->__prin1(v->u.ud->data,fp);
						else
						{
							fprintf(fp,"#<");
							fklPrintString(fklGetGlobSymbolWithId(v->u.ud->type)->symbol,fp);
							fprintf(fp,":%p>",v->u.ud);
						}
						break;
					default:
						fputs("#<unknown>",fp);
						break;
				}
			}
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

void fklPrin1VMvalue(FklVMvalue* value,FILE* fp)
{
	fklPrintVMvalue(value,fp,prin1VMatom);
}

void fklPrincVMvalue(FklVMvalue* value,FILE* fp)
{
	fklPrintVMvalue(value,fp,princVMatom);
}

typedef struct
{
	PrtElem* key;
	PrtElem* v;
}HashPrtElem;

static HashPrtElem* createHashPrtElem(PrtElem* key,PrtElem* v)
{
	HashPrtElem* r=(HashPrtElem*)malloc(sizeof(HashPrtElem));
	FKL_ASSERT(r);
	r->key=key;
	r->v=v;
	return r;
}

void fklPrintVMvalue(FklVMvalue* value,FILE* fp,void(*atomPrinter)(FklVMvalue* v,FILE* fp))
{
	FklPtrStack* recStack=fklCreatePtrStack(32,16);
	FklPtrStack* hasPrintRecStack=fklCreatePtrStack(32,16);
	scanCirRef(value,recStack);
	FklPtrQueue* queue=fklCreatePtrQueue();
	FklPtrStack* queueStack=fklCreatePtrStack(32,16);
	fklPushPtrQueue(createPrtElem(PRT_CAR,value),queue);
	fklPushPtrStack(queue,queueStack);
	while(!fklIsPtrStackEmpty(queueStack))
	{
		FklPtrQueue* cQueue=fklTopPtrStack(queueStack);
		while(fklLengthPtrQueue(cQueue))
		{
			PrtElem* e=fklPopPtrQueue(cQueue);
			FklVMvalue* v=e->v;
			if(e->state==PRT_CDR||e->state==PRT_REC_CDR)
				fputc(',',fp);
			if(e->state==PRT_REC_CAR||e->state==PRT_REC_CDR||e->state==PRT_REC_BOX)
			{
				fprintf(fp,"#%lu#",(uintptr_t)e->v);
				free(e);
			}
			else if(e->state==PRT_HASH_ITEM)
			{
				fputc('(',fp);
				FklPtrQueue* iQueue=fklCreatePtrQueue();
				HashPrtElem* elem=(void*)e->v;
				fklPushPtrQueue(elem->key,iQueue);
				fklPushPtrQueue(elem->v,iQueue);
				fklPushPtrStack(iQueue,queueStack);
				cQueue=iQueue;
				free(elem);
				free(e);
				continue;
			}
			else
			{
				free(e);
				if(!FKL_IS_VECTOR(v)&&!FKL_IS_PAIR(v)&&!FKL_IS_BOX(v)&&!FKL_IS_HASHTABLE(v))
					atomPrinter(v,fp);
				else
				{
					for(uint32_t i=0;i<recStack->top;i++)
						if(recStack->base[i]==v)
						{
							fprintf(fp,"#%u=",i);
							break;
						}
					if(FKL_IS_VECTOR(v))
					{
						fputs("#(",fp);
						FklPtrQueue* vQueue=fklCreatePtrQueue();
						for(size_t i=0;i<v->u.vec->size;i++)
						{
							size_t w=0;
							if((isInStack(v->u.vec->base[i],recStack,&w)&&isInStack(v->u.vec->base[i],hasPrintRecStack,NULL))||v->u.vec->base[i]==v)
								fklPushPtrQueue(createPrtElem(PRT_REC_CAR,(void*)w)
										,vQueue);
							else
							{
								fklPushPtrQueue(createPrtElem(PRT_CAR,v->u.vec->base[i])
										,vQueue);
								fklPushPtrStack(v->u.vec->base[i],hasPrintRecStack);
							}
						}
						fklPushPtrStack(vQueue,queueStack);
						cQueue=vQueue;
						continue;
					}
					else if(FKL_IS_BOX(v))
					{
						fputs("#&",fp);
						size_t w=0;
						if((isInStack(v->u.box,recStack,&w)&&isInStack(v->u.box,hasPrintRecStack,NULL))||v->u.box==v)
							fklPushPtrQueueToFront(createPrtElem(PRT_REC_BOX,(void*)w),cQueue);
						else
						{
							fklPushPtrQueueToFront(createPrtElem(PRT_BOX,v->u.box),cQueue);
							fklPushPtrStack(v->u.box,hasPrintRecStack);
						}
						continue;
					}
					else if(FKL_IS_HASHTABLE(v))
					{
						const static char* tmp[]=
						{
							"#hash",
							"#hasheqv",
							"#hashequal",
						};
						fputs(tmp[v->u.hash->type],fp);
						FklPtrQueue* hQueue=fklCreatePtrQueue();
						for(FklHashTableNodeList* list=v->u.hash->ht->list;list;list=list->next)
						{
							FklVMhashTableItem* item=list->node->item;
							PrtElem* keyElem=NULL;
							PrtElem* vElem=NULL;
							size_t w=0;
							if((isInStack(item->key,recStack,&w)&&isInStack(item->key,hasPrintRecStack,NULL))||item->key==v)
								keyElem=createPrtElem(PRT_REC_CAR,(void*)w);
							else
							{
								keyElem=createPrtElem(PRT_CAR,item->key);
								fklPushPtrStack(item->key,hasPrintRecStack);
							}
							if((isInStack(item->v,recStack,&w)&&isInStack(item->key,hasPrintRecStack,NULL))||item->v==v)
								vElem=createPrtElem(PRT_REC_CDR,(void*)w);
							else
							{
								vElem=createPrtElem(PRT_CDR,item->v);
								fklPushPtrStack(item->v,hasPrintRecStack);
							}
							fklPushPtrQueue(createPrtElem(PRT_HASH_ITEM,(void*)createHashPrtElem(keyElem,vElem)),hQueue);
						}
						fklPushPtrStack(hQueue,queueStack);
						cQueue=hQueue;
						continue;
					}
					else
					{
						fputc('(',fp);
						FklPtrQueue* lQueue=fklCreatePtrQueue();
						FklVMpair* p=v->u.pair;
						for(;;)
						{
							PrtElem* ce=NULL;
							size_t w=0;
							if(isInStack(p->car,recStack,&w)&&(isInStack(p->car,hasPrintRecStack,NULL)||p->car==v))
								ce=createPrtElem(PRT_REC_CAR,(void*)w);
							else
							{
								ce=createPrtElem(PRT_CAR,p->car);
								fklPushPtrStack(p->car,hasPrintRecStack);
							}
							fklPushPtrQueue(ce,lQueue);
							if(isInStack(p->cdr,recStack,&w))
							{
								PrtElem* cdre=NULL;
								if(p->cdr!=v&&!isInStack(p->cdr,hasPrintRecStack,NULL))
								{
									cdre=createPrtElem(PRT_CDR,p->cdr);
									fklPushPtrStack(p->cdr,hasPrintRecStack);
								}
								else
								{
									cdre=createPrtElem(PRT_REC_CDR,(void*)w);
								}
								fklPushPtrQueue(cdre,lQueue);
								break;
							}
							FklVMpair* next=FKL_IS_PAIR(p->cdr)?p->cdr->u.pair:NULL;
							if(!next)
							{
								FklVMvalue* cdr=p->cdr;
								if(cdr!=FKL_VM_NIL)
									fklPushPtrQueue(createPrtElem(PRT_CDR,cdr),lQueue);
								break;
							}
							p=next;
						}
						fklPushPtrStack(lQueue,queueStack);
						cQueue=lQueue;
						continue;
					}
				}
				
			}
			if(fklLengthPtrQueue(cQueue)
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_BOX
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_BOX)
				fputc(' ',fp);
		}
		fklPopPtrStack(queueStack);
		fklFreePtrQueue(cQueue);
		if(!fklIsPtrStackEmpty(queueStack))
		{
			fputc(')',fp);
			cQueue=fklTopPtrStack(queueStack);
			if(fklLengthPtrQueue(cQueue)
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_CDR
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_BOX
					&&((PrtElem*)fklFirstPtrQueue(cQueue))->state!=PRT_REC_BOX)
				fputc(' ',fp);
		}
	}
	fklFreePtrStack(queueStack);
	fklFreePtrStack(recStack);
	fklFreePtrStack(hasPrintRecStack);
}

FklVMvalue* fklSetRef(FklVMvalue* volatile* pref,FklVMvalue* v,FklVMgc* gc)
{
	FklVMvalue* ref=*pref;
	*pref=v;
	FklGCstate running=fklGetGCstate(gc);
	if(running==FKL_GC_PROPAGATE||running==FKL_GC_COLLECT)
	{
		fklGC_toGrey(ref,gc);
		fklGC_toGrey(v,gc);
	}
	return ref;
}

FklVMvalue* fklGetTopValue(FklVMstack* stack)
{
	return stack->values[stack->tp-1];
}

FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place)
{
	return stack->values[place];
}

//static void addToList(FklAstCptr* fir,FklAstCptr* sec)
//{
//	while(fir->type==FKL_TYPE_PAIR)fir=&fir->u.pair->cdr;
//	fir->type=FKL_TYPE_PAIR;
//	fir->u.pair=fklCreatePair(sec->curline,fir->outer);
//	fir->u.pair->car.curline=sec->curline;
//	fir->u.pair->car.outer=fir->outer;
//	fir->u.pair->car.type=sec->type;
//	fir->u.pair->car.u.pair=sec->u.pair;
//	fir->u.pair->car.u.pair->prev=fir->u.pair;
//}

//FklAstCptr* fklCastVMvalueToCptr(FklVMvalue* value,uint64_t curline,FklHashTable* lineHash)
//{
//	FklPtrStack* recStack=fklCreatePtrStack(32,16);
//	scanCirRef(value,recStack);
//	FklAstCptr* tmp=NULL;
//	if(!recStack->top)
//	{
//		tmp=fklCreateCptr(curline,NULL);
//		FklPtrStack* s1=fklCreatePtrStack(32,16);
//		FklPtrStack* s2=fklCreatePtrStack(32,16);
//		fklPushPtrStack(value,s1);
//		fklPushPtrStack(tmp,s2);
//		while(!fklIsPtrStackEmpty(s1))
//		{
//			FklVMvalue* root=fklPopPtrStack(s1);
//			LineNumHashItem* item=fklGetHashItem(&root,lineHash);
//			FklAstCptr* root1=fklPopPtrStack(s2);
//			if(item)
//				root1->curline=item->line;
//			FklValueType cptrType=0;
//			if(root==FKL_VM_NIL)
//				cptrType=FKL_TYPE_NIL;
//			else if(FKL_IS_PAIR(root))
//				cptrType=FKL_TYPE_PAIR;
//			else
//				cptrType=FKL_TYPE_ATM;
//			root1->type=cptrType;
//			if(cptrType==FKL_TYPE_ATM)
//			{
//				FklAstAtom* tmpAtm=fklCreateAtom(FKL_TYPE_SYM,root1->outer);
//				FklVMptrTag tag=FKL_GET_TAG(root);
//				switch(tag)
//				{
//					case FKL_TAG_SYM:
//						tmpAtm->type=FKL_TYPE_SYM;
//						tmpAtm->value.str=fklCopyString(fklGetGlobSymbolWithId(FKL_GET_SYM(root))->symbol);
//						break;
//					case FKL_TAG_I32:
//						tmpAtm->type=FKL_TYPE_I32;
//						tmpAtm->value.i32=FKL_GET_I32(root);
//						break;
//					case FKL_TAG_CHR:
//						tmpAtm->type=FKL_TYPE_CHR;
//						tmpAtm->value.chr=FKL_GET_CHR(root);
//						break;
//					case FKL_TAG_PTR:
//						{
//							tmpAtm->type=root->type;
//							switch(root->type)
//							{
//								case FKL_TYPE_F64:
//									tmpAtm->value.f64=root->u.f64;
//									break;
//								case FKL_TYPE_I64:
//									tmpAtm->value.i64=root->u.i64;
//									break;
//								case FKL_TYPE_STR:
//									tmpAtm->value.str=fklCopyString(root->u.str);
//									break;
//								case FKL_TYPE_BYTEVECTOR:
//									tmpAtm->value.bvec=fklCopyBytevector(root->u.bvec);
//									break;
//								case FKL_TYPE_PROC:
//									tmpAtm->type=FKL_TYPE_SYM;
//									tmpAtm->value.str=fklCreateStringFromCstr("#<proc>");
//									break;
//								case FKL_TYPE_DLPROC:
//									tmpAtm->type=FKL_TYPE_SYM;
//									tmpAtm->value.str=fklCreateStringFromCstr("#<dlproc>");
//									break;
//								case FKL_TYPE_CONT:
//									tmpAtm->type=FKL_TYPE_SYM;
//									tmpAtm->value.str=fklCreateStringFromCstr("#<proc>");
//									break;
//								case FKL_TYPE_CHAN:
//									tmpAtm->type=FKL_TYPE_SYM;
//									tmpAtm->value.str=fklCreateStringFromCstr("#<chan>");
//									break;
//								case FKL_TYPE_FP:
//									tmpAtm->type=FKL_TYPE_SYM;
//									tmpAtm->value.str=fklCreateStringFromCstr("#<fp>");
//									break;
//								case FKL_TYPE_ERR:
//									tmpAtm->type=FKL_TYPE_SYM;
//									tmpAtm->value.str=fklCreateStringFromCstr("#<err>");
//									break;
//								case FKL_TYPE_BOX:
//									tmpAtm->type=FKL_TYPE_BOX;
//									fklPushPtrStack(root->u.box,s1);
//									fklPushPtrStack(&tmpAtm->value.box,s2);
//									break;
//								case FKL_TYPE_VECTOR:
//									tmpAtm->type=FKL_TYPE_VECTOR;
//									fklMakeAstVector(&tmpAtm->value.vec,root->u.vec->size,NULL);
//									for(size_t i=0;i<root->u.vec->size;i++)
//										fklPushPtrStack(root->u.vec->base[i],s1);
//									for(size_t i=0;i<tmpAtm->value.vec.size;i++)
//										fklPushPtrStack(&tmpAtm->value.vec.base[i],s2);
//									break;
//								case FKL_TYPE_HASHTABLE:
//									{
//										tmpAtm->type=FKL_TYPE_HASHTABLE;
//										uint64_t num=root->u.hash->ht->num;
//										fklMakeAstHashTable(&tmpAtm->value.hash,root->u.hash->type,root->u.hash->ht->num);
//										for(FklHashTableNodeList* list=root->u.hash->ht->list;list;list=list->next)
//										{
//											FklVMhashTableItem* item=list->node->item;
//											fklPushPtrStack(item->key,s1);
//											fklPushPtrStack(item->v,s1);
//										}
//										for(size_t i=0;i<num;i++)
//										{
//											FklAstCptr pair={NULL,0,FKL_TYPE_NIL,{NULL}};
//											pair.type=FKL_TYPE_PAIR;
//											pair.u.pair=fklCreatePair(item?item->line:curline,pair.outer);
//											addToList(&tmpAtm->value.hash.items,&pair);
//											fklPushPtrStack(&pair.u.pair->car,s2);
//											fklPushPtrStack(&pair.u.pair->cdr,s2);
//										}
//									}
//									break;
//								case FKL_TYPE_BIG_INT:
//									tmpAtm->type=FKL_TYPE_BIG_INT;
//									fklSetBigInt(&tmpAtm->value.bigInt,root->u.bigInt);
//									break;
//								default:
//									return NULL;
//									break;
//							}
//						}
//						break;
//					default:
//						return NULL;
//						break;
//				}
//				root1->u.atom=tmpAtm;
//			}
//			else if(cptrType==FKL_TYPE_PAIR)
//			{
//				fklPushPtrStack(root->u.pair->car,s1);
//				fklPushPtrStack(root->u.pair->cdr,s1);
//				FklAstPair* tmpPair=fklCreatePair(item?item->line:curline,root1->outer);
//				root1->u.pair=tmpPair;
//				tmpPair->car.outer=tmpPair;
//				tmpPair->cdr.outer=tmpPair;
//				fklPushPtrStack(&tmpPair->car,s2);
//				fklPushPtrStack(&tmpPair->cdr,s2);
//			}
//		}
//		fklFreePtrStack(s1);
//		fklFreePtrStack(s2);
//	}
//	fklFreePtrStack(recStack);
//	return tmp;
//}

void fklInitVMRunningResource(FklVM* vm,FklVMvalue* vEnv,FklVMgc* gc,FklByteCodelnt* code,uint32_t start,uint32_t size)
{
	FklVMproc proc={
		.scp=start,
		.cpc=size,
		.sid=0,
		.prevEnv=NULL,
	};
	FklVMframe* mainframe=fklCreateVMframe(&proc,NULL);
	mainframe->localenv=vEnv;
	vm->code=code->bc->code;
	vm->size=code->bc->size;
	vm->frames=mainframe;
	vm->lnt=fklCreateLineNumTable();
	vm->lnt->num=code->ls;
	vm->lnt->list=code->l;
	if(vm->gc!=gc)
	{
		fklFreeVMgc(vm->gc);
		vm->gc=gc;
	}
}

void fklUninitVMRunningResource(FklVM* vm)
{
	fklWaitGC(vm->gc);
	free(vm->lnt);
	fklFreeAllVMs(vm);
}

size_t fklVMlistLength(FklVMvalue* v)
{
	size_t len=0;
	for(;FKL_IS_PAIR(v);v=fklGetVMpairCdr(v))len++;
	return len;
}
