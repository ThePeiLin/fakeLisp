#include<fakeLisp/vmutils.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/fakeFFI.h>
#include<fakeLisp/opcode.h>
#include<string.h>
#include<stdio.h>
#include<math.h>
#include<pthread.h>
#include<ffi.h>
#ifndef _WIN32
#include<dlfcn.h>
#else
#include<tchar.h>
#endif

typedef struct Cirular_Ref_List
{
	FklVMpair* pair;
	int32_t count;
	struct Cirular_Ref_List* next;
}CRL;


/*print mem ref func list*/
#define PRINT_MEM_REF(FMT,TYPE,MEM,FP) fprintf(FP,FMT,*(TYPE*)(MEM))
#define ARGL uint8_t* mem,FILE* fp
static void printShortMem (ARGL){PRINT_MEM_REF("%d",short,mem,fp);}
static void printIntMem   (ARGL){PRINT_MEM_REF("%d",int,mem,fp);}
static void printUShortMem(ARGL){PRINT_MEM_REF("%u",unsigned short,mem,fp);}
static void printUIntMem  (ARGL){PRINT_MEM_REF("%u",unsigned int,mem,fp);}
static void printLongMem  (ARGL){PRINT_MEM_REF("%ld",long,mem,fp);}
static void printULongMem (ARGL){PRINT_MEM_REF("%lu",unsigned long,mem,fp);}
static void printLLongMem (ARGL){PRINT_MEM_REF("%lld",long long,mem,fp);}
static void printULLongMem(ARGL){PRINT_MEM_REF("%llu",unsigned long long,mem,fp);}
static void printPtrdiff_t(ARGL){PRINT_MEM_REF("%ld",ptrdiff_t,mem,fp);}
static void printSize_t   (ARGL){PRINT_MEM_REF("%zu",size_t,mem,fp);};
static void printSsize_t  (ARGL){PRINT_MEM_REF("%zd",ssize_t,mem,fp);}
static void printChar     (ARGL){PRINT_MEM_REF("%c",char,mem,fp);}
static void printWchar_t  (ARGL){PRINT_MEM_REF("%C",wchar_t,mem,fp);}
static void printFloat    (ARGL){PRINT_MEM_REF("%f",float,mem,fp);}
static void printDouble   (ARGL){PRINT_MEM_REF("%lf",double,mem,fp);}
static void printInt8_t   (ARGL){PRINT_MEM_REF("%d",int8_t,mem,fp);}
static void printUint8_t  (ARGL){PRINT_MEM_REF("%u",uint8_t,mem,fp);}
static void printInt16_t  (ARGL){PRINT_MEM_REF("%d",int16_t,mem,fp);}
static void printUint16_t (ARGL){PRINT_MEM_REF("%u",uint16_t,mem,fp);}
static void printInt32_t  (ARGL){PRINT_MEM_REF("%d",int32_t,mem,fp);}
static void printUint32_t (ARGL){PRINT_MEM_REF("%u",uint32_t,mem,fp);}
static void printInt64_t  (ARGL){PRINT_MEM_REF("%ld",int64_t,mem,fp);}
static void printUint64_t (ARGL){PRINT_MEM_REF("%lu",uint64_t,mem,fp);}
static void printIptr     (ARGL){PRINT_MEM_REF("%ld",intptr_t,mem,fp);}
static void printUptr     (ARGL){PRINT_MEM_REF("%lu",uintptr_t,mem,fp);}
static void printVPtr     (ARGL){PRINT_MEM_REF("%p",void*,mem,fp);}
#undef ARGL
#undef PRINT_MEM_REF
static void (*PrintMemoryRefFuncList[])(uint8_t*,FILE*)=
{
	printShortMem ,
	printIntMem   ,
	printUShortMem,
	printUIntMem  ,
	printLongMem  ,
	printULongMem ,
	printLLongMem ,
	printULLongMem,
	printPtrdiff_t,
	printSize_t   ,
	printSsize_t  ,
	printChar     ,
	printWchar_t  ,
	printFloat    ,
	printDouble   ,
	printInt8_t   ,
	printUint8_t  ,
	printInt16_t  ,
    printUint16_t ,
    printInt32_t  ,
    printUint32_t ,
    printInt64_t  ,
    printUint64_t ,
    printIptr     ,
    printUptr     ,
    printVPtr     ,
};
/*-----------------------*/

/*memory caster list*/
#define CAST_TO_I32(TYPE) return MAKE_VM_I32(*(TYPE*)mem);
#define CAST_TO_I64(TYPE) int64_t t=*(TYPE*)mem;return fklNewVMvalue(FKL_I64,&t,heap);
#define ARGL uint8_t* mem,VMheap* heap
static FklVMvalue* castShortMem (ARGL){CAST_TO_I32(short)}
static FklVMvalue* castIntMem   (ARGL){CAST_TO_I32(int)}
static FklVMvalue* castUShortMem(ARGL){CAST_TO_I32(unsigned short)}
static FklVMvalue* castUintMem  (ARGL){CAST_TO_I64(unsigned int)}
static FklVMvalue* castLongMem  (ARGL){CAST_TO_I64(long)}
static FklVMvalue* castULongMem (ARGL){CAST_TO_I64(unsigned long)}
static FklVMvalue* castLLongMem (ARGL){CAST_TO_I64(long long)}
static FklVMvalue* castULLongMem(ARGL){CAST_TO_I64(unsigned long long)}
static FklVMvalue* castPtrdiff_t(ARGL){CAST_TO_I64(ptrdiff_t)}
static FklVMvalue* castSize_t   (ARGL){CAST_TO_I64(size_t)}
static FklVMvalue* castSsize_t  (ARGL){CAST_TO_I64(ssize_t)}
static FklVMvalue* castChar     (ARGL){return MAKE_VM_CHR(*(char*)mem);}
static FklVMvalue* castWchar_t  (ARGL){return MAKE_VM_I32(*(wchar_t*)mem);}
static FklVMvalue* castFloat    (ARGL){double t=*(float*)mem;return fklNewVMvalue(FKL_DBL,&t,heap);}
static FklVMvalue* castDouble   (ARGL){double t=*(double*)mem;return fklNewVMvalue(FKL_DBL,&t,heap);}
static FklVMvalue* castInt8_t   (ARGL){CAST_TO_I32(int8_t)}
static FklVMvalue* castUint8_t  (ARGL){CAST_TO_I32(uint8_t)}
static FklVMvalue* castInt16_t  (ARGL){CAST_TO_I32(int16_t)}
static FklVMvalue* castUint16_t (ARGL){CAST_TO_I32(uint16_t)}
static FklVMvalue* castInt32_t  (ARGL){CAST_TO_I32(int32_t)}
static FklVMvalue* castUint32_t (ARGL){CAST_TO_I32(uint32_t)}
static FklVMvalue* castInt64_t  (ARGL){CAST_TO_I64(int64_t)}
static FklVMvalue* castUint64_t (ARGL){CAST_TO_I64(uint64_t)}
static FklVMvalue* castIptr     (ARGL){CAST_TO_I64(intptr_t)}
static FklVMvalue* castUptr     (ARGL){CAST_TO_I64(uintptr_t)}
static FklVMvalue* castVPtr     (ARGL){return fklNewVMvalue(FKL_I64,mem,heap);}
#undef ARGL
#undef CAST_TO_I32
#undef CAST_TO_I64
static FklVMvalue*(*MemoryCasterList[])(uint8_t*,VMheap*)=
{
	castShortMem ,
	castIntMem   ,
	castUShortMem,
	castUintMem  ,
	castLongMem  ,
	castULongMem ,
	castLLongMem ,
	castULLongMem,
	castPtrdiff_t,
	castSize_t   ,
	castSsize_t  ,
	castChar     ,
	castWchar_t  ,
	castFloat    ,
	castDouble   ,
	castInt8_t   ,
	castUint8_t  ,
	castInt16_t  ,
    castUint16_t ,
    castInt32_t  ,
    castUint32_t ,
    castInt64_t  ,
    castUint64_t ,
    castIptr     ,
    castUptr     ,
    castVPtr     ,
};
/*------------------*/

/*Memory setting function list*/
#define BODY(EXPRESSION,TYPE,VALUE) if(EXPRESSION)return 1;*(TYPE*)mem=(VALUE);return 0;
#define SET_NUM(TYPE) BODY(!IS_I32(v)&&!IS_I64(v),TYPE,IS_I32(v)?GET_I32(v):v->u.i64)
#define ARGL uint8_t* mem,FklVMvalue* v
static int setShortMem (ARGL){SET_NUM(short)}
static int setIntMem   (ARGL){SET_NUM(int)}
static int setUShortMem(ARGL){SET_NUM(unsigned short)}
static int setUintMem  (ARGL){SET_NUM(unsigned int)}
static int setLongMem  (ARGL){SET_NUM(long)}
static int setULongMem (ARGL){SET_NUM(unsigned long)}
static int setLLongMem (ARGL){SET_NUM(long long)}
static int setULLongMem(ARGL){SET_NUM(unsigned long long)}
static int setPtrdiff_t(ARGL){SET_NUM(ptrdiff_t)}
static int setSize_t   (ARGL){SET_NUM(size_t)}
static int setSsize_t  (ARGL){SET_NUM(ssize_t)}
static int setChar     (ARGL){BODY(!IS_CHR(v),char,GET_CHR(v))}
static int setWchar_t  (ARGL){BODY(!IS_CHR(v)&&!IS_I32(v),wchar_t,IS_I32(v)?GET_I32(v):GET_CHR(v))}
static int setFloat    (ARGL){BODY(!IS_DBL(v)&&!IS_I32(v)&&IS_I64(v),float,IS_DBL(v)?v->u.dbl:(IS_I32(v)?GET_I32(v):v->u.i64))}
static int setDouble   (ARGL){BODY(!IS_DBL(v)&&!IS_I32(v)&&IS_I64(v),double,IS_DBL(v)?v->u.dbl:(IS_I32(v)?GET_I32(v):v->u.i64))}
static int setInt8_t   (ARGL){SET_NUM(int8_t)}
static int setUint8_t  (ARGL){SET_NUM(uint8_t)}
static int setInt16_t  (ARGL){SET_NUM(int16_t)}
static int setUint16_t (ARGL){SET_NUM(uint16_t)}
static int setInt32_t  (ARGL){SET_NUM(int32_t)}
static int setUint32_t (ARGL){SET_NUM(uint32_t)}
static int setInt64_t  (ARGL){SET_NUM(int64_t)}
static int setUint64_t (ARGL){SET_NUM(uint64_t)}
static int setIptr     (ARGL){SET_NUM(intptr_t)}
static int setUptr     (ARGL){SET_NUM(uintptr_t)}
static int setVPtr     (ARGL){SET_NUM(uintptr_t)}
#undef SET_NUM
#undef BODY
static int (*MemorySeterList[])(ARGL)=
{
	setShortMem ,
	setIntMem   ,
	setUShortMem,
	setUintMem  ,
	setLongMem  ,
	setULongMem ,
	setLLongMem ,
	setULLongMem,
	setPtrdiff_t,
	setSize_t   ,
	setSsize_t  ,
	setChar     ,
	setWchar_t  ,
	setFloat    ,
	setDouble   ,
	setInt8_t   ,
	setUint8_t  ,
	setInt16_t  ,
    setUint16_t ,
    setInt32_t  ,
    setUint32_t ,
    setInt64_t  ,
    setUint64_t ,
    setIptr     ,
    setUptr     ,
    setVPtr     ,
};
#undef ARGL
/*------------------*/
extern FklSymbolTable GlobSymbolTable;
static CRL* newCRL(FklVMpair* pair,int32_t count)
{
	CRL* tmp=(CRL*)malloc(sizeof(CRL));
	FKL_ASSERT(tmp,"newCRL",__FILE__,__LINE__);
	tmp->pair=pair;
	tmp->count=count;
	tmp->next=NULL;
	return tmp;
}

static int32_t findCRLcount(FklVMpair* pair,CRL* h)
{
	for(;h;h=h->next)
	{
		if(h->pair==pair)
			return h->count;
	}
	return -1;
}

static FklVMpair* hasSameVMpair(FklVMpair* begin,FklVMpair* other,CRL* h)
{
	FklVMpair* tmpPair=NULL;
	if(findCRLcount(begin,h)!=-1||findCRLcount(other,h)!=-1)
		return NULL;
	if(begin==other)
		return begin;

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->car))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->car->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->cdr))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->car->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->car))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->car->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->car)&&IS_PAIR(other->car->u.pair->cdr))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->car->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->car))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->cdr->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->cdr))&&IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin->car->u.pair,other->cdr->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->car))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->cdr->u.pair->car->u.pair,h);
	if(tmpPair)
		return tmpPair;

	if((IS_PAIR(other->cdr)&&IS_PAIR(other->cdr->u.pair->cdr))&&IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin->cdr->u.pair,other->cdr->u.pair->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;
	return NULL;
}

FklVMpair* isCircularReference(FklVMpair* begin,CRL* h)
{
	FklVMpair* tmpPair=NULL;
	if(IS_PAIR(begin->car))
		tmpPair=hasSameVMpair(begin,begin->car->u.pair,h);
	if(tmpPair)
		return tmpPair;
	if(IS_PAIR(begin->cdr))
		tmpPair=hasSameVMpair(begin,begin->cdr->u.pair,h);
	if(tmpPair)
		return tmpPair;
	return NULL;
}

int8_t isInTheCircle(FklVMpair* obj,FklVMpair* begin,FklVMpair* curPair)
{
	if(obj==curPair)
		return 1;
	if((IS_PAIR(curPair->car)&&begin==curPair->car->u.pair)||(IS_PAIR(curPair->cdr)&&begin==curPair->cdr->u.pair))
		return 0;
	return ((IS_PAIR(curPair->car))&&isInTheCircle(obj,begin,curPair->car->u.pair))||((IS_PAIR(curPair->cdr))&&isInTheCircle(obj,begin,curPair->cdr->u.pair));
}



pthread_mutex_t FklVMenvGlobalRefcountLock=PTHREAD_MUTEX_INITIALIZER;

#define INCREASE_REFCOUNT(TYPE,PV) {\
	if((PV))\
	{\
		pthread_mutex_lock(&(TYPE##GlobalRefcountLock));\
		((TYPE*)(PV))->refcount+=1;\
		pthread_mutex_unlock(&(TYPE##GlobalRefcountLock));\
	}\
}

#define DECREASE_REFCOUNT(TYPE,PV) {\
	if((PV))\
	{\
		pthread_mutex_lock(&(TYPE##GlobalRefcountLock));\
		((TYPE*)(PV))->refcount-=1;\
		pthread_mutex_unlock(&(TYPE##GlobalRefcountLock));\
	}\
}

FklVMproc* fklNewVMproc(uint32_t scp,uint32_t cpc)
{
	FklVMproc* tmp=(FklVMproc*)malloc(sizeof(FklVMproc));
	FKL_ASSERT(tmp,"fklNewVMproc",__FILE__,__LINE__);
	tmp->prevEnv=NULL;
	tmp->scp=scp;
	tmp->cpc=cpc;
	tmp->sid=0;
	return tmp;
}

FklVMvalue* fklCopyVMvalue(FklVMvalue* obj,VMheap* heap)
{
	FklPtrStack* s1=fklNewPtrStack(32);
	FklPtrStack* s2=fklNewPtrStack(32);
	FklVMvalue* tmp=VM_NIL;
	fklPushPtrStack(obj,s1);
	fklPushPtrStack(&tmp,s2);
	while(!fklIsPtrStackEmpty(s1))
	{
		FklVMvalue* root=fklPopPtrStack(s1);
		FklVMptrTag tag=GET_TAG(root);
		FklVMvalue** root1=fklPopPtrStack(s2);
		switch(tag)
		{
			case FKL_NIL_TAG:
			case FKL_I32_TAG:
			case FKL_SYM_TAG:
			case FKL_CHR_TAG:
				*root1=root;
				break;
			case FKL_PTR_TAG:
				{
					FklValueType type=root->type;
					switch(type)
					{
						case FKL_DBL:
							*root1=fklNewVMvalue(FKL_DBL,&root->u.dbl,heap);
							break;
						case FKL_BYTS:
							*root1=fklNewVMvalue(FKL_BYTS,fklNewVMByts(root->u.byts->size,root->u.byts->str),heap);
							break;
						case FKL_STR:
							*root1=fklNewVMvalue(FKL_STR,fklCopyStr(root->u.str),heap);
							break;
						case FKL_CONT:
						case FKL_PRC:
						case FKL_FP:
						case FKL_DLL:
						case FKL_DLPROC:
						case FKL_ERR:
							*root1=root;
							break;
						case FKL_CHAN:
							{
								FklVMChanl* objCh=root->u.chan;
								FklVMChanl* tmpCh=fklNewVMChanl(objCh->max);
								FklQueueNode* cur=objCh->messages->head;
								for(;cur;cur=cur->next)
								{
									void* tmp=VM_NIL;
									fklPushPtrQueue(tmp,tmpCh->messages);
									fklPushPtrStack(cur->data,s1);
									fklPushPtrStack(tmp,s2);
								}
								*root1=fklNewVMvalue(FKL_CHAN,tmpCh,heap);
							}
							break;
						case FKL_PAIR:
							*root1=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),heap);
							fklPushPtrStack(&(*root1)->u.pair->car,s2);
							fklPushPtrStack(&(*root1)->u.pair->cdr,s2);
							fklPushPtrStack(root->u.pair->car,s1);
							fklPushPtrStack(root->u.pair->cdr,s1);
							break;
						default:
							break;
					}
					*root1=MAKE_VM_PTR(*root1);
				}
				break;
			default:
				return NULL;
				break;
		}
	}
	fklFreePtrStack(s1);
	fklFreePtrStack(s2);
	return tmp;
}

FklVMvalue* fklNewVMvalue(FklValueType type,void* pValue,VMheap* heap)
{
	switch((int)type)
	{
		case FKL_NIL:
			return VM_NIL;
			break;
		case FKL_CHR:
			return MAKE_VM_CHR(pValue);
			break;
		case FKL_I32:
			return MAKE_VM_I32(pValue);
			break;
		case FKL_SYM:
			return MAKE_VM_SYM(pValue);
			break;
		default:
			{
				FklVMvalue* tmp=(FklVMvalue*)malloc(sizeof(FklVMvalue));
				FKL_ASSERT(tmp,"fklNewVMvalue",__FILE__,__LINE__);
				tmp->type=type;
				tmp->mark=0;
				tmp->next=heap->head;
				tmp->prev=NULL;
				pthread_mutex_lock(&heap->lock);
				if(heap->head!=NULL)heap->head->prev=tmp;
				heap->head=tmp;
				heap->num+=1;
				pthread_mutex_unlock(&heap->lock);
				switch(type)
				{
					case FKL_DBL:
						if(pValue)
							tmp->u.dbl=*(double*)pValue;
						break;
					case FKL_I64:
						if(pValue)
							tmp->u.i64=*(int64_t*)pValue;
						break;
					case FKL_STR:
						tmp->u.str=pValue;break;
					case FKL_PAIR:
						tmp->u.pair=pValue;break;
					case FKL_PRC:
						tmp->u.prc=pValue;break;
					case FKL_BYTS:
						tmp->u.byts=pValue;break;
					case FKL_CONT:
						tmp->u.cont=pValue;break;
					case FKL_CHAN:
						tmp->u.chan=pValue;break;
					case FKL_FP:
						tmp->u.fp=pValue;break;
					case FKL_DLL:
						tmp->u.dll=pValue;break;
					case FKL_DLPROC:
						tmp->u.dlproc=pValue;break;
					case FKL_FLPROC:
						tmp->u.flproc=pValue;break;
					case FKL_ERR:
						tmp->u.err=pValue;break;
					case FKL_CHF:
						tmp->u.chf=pValue;break;
					case FKL_MEM:
						tmp->u.chf=pValue;break;
					default:
						return NULL;
						break;
				}
				return MAKE_VM_PTR(tmp);
			}
			break;
	}
}

FklVMvalue* fklNewTrueValue(VMheap* heap)
{
	FklVMvalue* tmp=fklNewVMvalue(FKL_I32,(FklVMptr)1,heap);
	return tmp;
}

FklVMvalue* fklNewNilValue(VMheap* heap)
{
	FklVMvalue* tmp=fklNewVMvalue(FKL_NIL,NULL,heap);
	return tmp;
}

FklVMvalue* fklGetTopValue(FklVMstack* stack)
{
	return stack->values[stack->tp-1];
}

FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place)
{
	return stack->values[place];
}

FklVMvalue* fklGetVMpairCar(FklVMvalue* obj)
{
	return obj->u.pair->car;
}

FklVMvalue* fklGetVMpairCdr(FklVMvalue* obj)
{
	return obj->u.pair->cdr;
}

int fklVMvaluecmp(FklVMvalue* fir,FklVMvalue* sec)
{
	FklPtrStack* s1=fklNewPtrStack(32);
	FklPtrStack* s2=fklNewPtrStack(32);
	fklPushPtrStack(fir,s1);
	fklPushPtrStack(sec,s2);
	int r=1;
	while(!fklIsPtrStackEmpty(s1))
	{
		FklVMvalue* root1=fklPopPtrStack(s1);
		FklVMvalue* root2=fklPopPtrStack(s2);
		if(!IS_PTR(root1)&&!IS_PTR(root2)&&root1!=root2)
			r=0;
		else if(GET_TAG(root1)!=GET_TAG(root2))
			r=0;
		else if(IS_PTR(root1)&&IS_PTR(root2))
		{
			if(root1->type!=root2->type)
				r=0;
			switch(root1->type)
			{
				case FKL_DBL:
					r=(fabs(root1->u.dbl-root2->u.dbl)==0);
					break;
				case FKL_I64:
					r=(root1->u.i64-root2->u.i64)==0;
				case FKL_STR:
					r=!strcmp(root1->u.str,root2->u.str);
					break;
				case FKL_BYTS:
					r=fklEqVMByts(root1->u.byts,root2->u.byts);
					break;
				case FKL_PAIR:
					r=1;
					fklPushPtrStack(root1->u.pair->car,s1);
					fklPushPtrStack(root1->u.pair->cdr,s1);
					fklPushPtrStack(root2->u.pair->car,s2);
					fklPushPtrStack(root2->u.pair->cdr,s2);
					break;
				default:
					r=(root1==root2);
					break;
			}
		}
		if(!r)
			break;
	}
	fklFreePtrStack(s1);
	fklFreePtrStack(s2);
	return r;
}

int subfklVMvaluecmp(FklVMvalue* fir,FklVMvalue* sec)
{
	return fir==sec;
}

int fklNumcmp(FklVMvalue* fir,FklVMvalue* sec)
{
	if(GET_TAG(fir)==GET_TAG(sec)&&GET_TAG(fir)==FKL_I32_TAG)
		return fir==sec;
	else
	{
		double first=(GET_TAG(fir)==FKL_I32_TAG)?GET_I32(fir)
			:((IS_I64(fir))?fir->u.i64:fir->u.dbl);
		double second=(GET_TAG(sec)==FKL_I32_TAG)?GET_I32(sec)
			:((IS_I64(sec))?sec->u.i64:sec->u.dbl);
		return fabs(first-second)==0;
	}
}

FklVMenv* fklNewVMenv(FklVMenv* prev)
{
	FklVMenv* tmp=(FklVMenv*)malloc(sizeof(FklVMenv));
	FKL_ASSERT(tmp,"fklNewVMenv",__FILE__,__LINE__);
	tmp->num=0;
	tmp->list=NULL;
	tmp->prev=prev;
	fklIncreaseVMenvRefcount(prev);
	tmp->refcount=0;
	return tmp;
}

void fklIncreaseVMenvRefcount(FklVMenv* env)
{
	INCREASE_REFCOUNT(FklVMenv,env);
}

void fklDecreaseVMenvRefcount(FklVMenv* env)
{
	DECREASE_REFCOUNT(FklVMenv,env);
}

FklVMpair* fklNewVMpair(void)
{
	FklVMpair* tmp=(FklVMpair*)malloc(sizeof(FklVMpair));
	FKL_ASSERT(tmp,"fklNewVMpair",__FILE__,__LINE__);
	tmp->car=VM_NIL;
	tmp->cdr=VM_NIL;
	return tmp;
}

FklVMvalue* fklCastCptrVMvalue(FklAstCptr* objCptr,VMheap* heap)
{
	FklPtrStack* s1=fklNewPtrStack(32);
	FklPtrStack* s2=fklNewPtrStack(32);
	FklVMvalue* tmp=VM_NIL;
	fklPushPtrStack(objCptr,s1);
	fklPushPtrStack(&tmp,s2);
	while(!fklIsPtrStackEmpty(s1))
	{
		FklAstCptr* root=fklPopPtrStack(s1);
		FklVMvalue** root1=fklPopPtrStack(s2);
		if(root->type==FKL_ATM)
		{
			FklAstAtom* tmpAtm=root->u.atom;
			switch(tmpAtm->type)
			{
				case FKL_I32:
					*root1=MAKE_VM_I32(tmpAtm->value.i32);
					break;
				case FKL_CHR:
					*root1=MAKE_VM_CHR(tmpAtm->value.chr);
					break;
				case FKL_SYM:
					*root1=MAKE_VM_SYM(fklAddSymbolToGlob(tmpAtm->value.str)->id);
					break;
				case FKL_DBL:
					*root1=fklNewVMvalue(FKL_DBL,&tmpAtm->value.dbl,heap);
					break;
				case FKL_BYTS:
					*root1=fklNewVMvalue(FKL_BYTS,fklNewVMByts(tmpAtm->value.byts.size,tmpAtm->value.byts.str),heap);
					break;
				case FKL_STR:
					*root1=fklNewVMvalue(FKL_STR,fklCopyStr(tmpAtm->value.str),heap);
					break;
				default:
					return NULL;
					break;
			}
		}
		else if(root->type==FKL_PAIR)
		{
			FklAstPair* objPair=root->u.pair;
			FklVMpair* tmpPair=fklNewVMpair();
			*root1=fklNewVMvalue(FKL_PAIR,tmpPair,heap);
			fklPushPtrStack(&objPair->car,s1);
			fklPushPtrStack(&objPair->cdr,s1);
			fklPushPtrStack(&tmpPair->car,s2);
			fklPushPtrStack(&tmpPair->cdr,s2);
			*root1=MAKE_VM_PTR(*root1);
		}
	}
	fklFreePtrStack(s1);
	fklFreePtrStack(s2);
	return tmp;
}

FklVMByts* fklNewVMByts(size_t size,uint8_t* str)
{
	FklVMByts* tmp=(FklVMByts*)malloc(sizeof(FklVMByts)+size);
	FKL_ASSERT(tmp,"fklNewVMByts",__FILE__,__LINE__);
	tmp->size=size;
	memcpy(tmp->str,str,size);
	return tmp;
}

FklVMByts* fklCopyVMByts(const FklVMByts* obj)
{
	if(obj==NULL)return NULL;
	FklVMByts* tmp=(FklVMByts*)malloc(sizeof(FklVMByts)+obj->size);
	FKL_ASSERT(tmp,"fklCopyVMByts",__FILE__,__LINE__);
	memcpy(tmp->str,obj->str,obj->size);
	tmp->size=obj->size;
	return tmp;
}

void fklVMBytsCat(FklVMByts** fir,const FklVMByts* sec)
{
	size_t firSize=(*fir)->size;
	size_t secSize=sec->size;
	*fir=(FklVMByts*)realloc(*fir,sizeof(FklVMByts)+(firSize+secSize)*sizeof(uint8_t));
	FKL_ASSERT(*fir,"fklVMBytsCat",__FILE__,__LINE__);
	(*fir)->size=firSize+secSize;
	memcpy((*fir)->str+firSize,sec->str,secSize);
}
FklVMByts* fklNewEmptyVMByts()
{
	FklVMByts* tmp=(FklVMByts*)malloc(sizeof(FklVMByts));
	FKL_ASSERT(tmp,"fklNewEmptyVMByts",__FILE__,__LINE__);
	tmp->size=0;
	return tmp;
}

uint8_t* fklCopyArry(size_t size,uint8_t* str)
{
	uint8_t* tmp=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(tmp,"fklCopyArry",__FILE__,__LINE__);
	memcpy(tmp,str,size);
	return tmp;
}

FklVMproc* fklCopyVMproc(FklVMproc* obj,VMheap* heap)
{
	FklVMproc* tmp=(FklVMproc*)malloc(sizeof(FklVMproc));
	FKL_ASSERT(tmp,"fklCopyVMproc",__FILE__,__LINE__);
	tmp->scp=obj->scp;
	tmp->cpc=obj->cpc;
	tmp->prevEnv=fklCopyVMenv(obj->prevEnv,heap);
	return tmp;
}

FklVMenv* fklCopyVMenv(FklVMenv* objEnv,VMheap* heap)
{
	FklVMenv* tmp=fklNewVMenv(NULL);
	tmp->list=(FklVMenvNode**)malloc(sizeof(FklVMenvNode*)*objEnv->num);
	FKL_ASSERT(tmp->list,"fklCopyVMenv",__FILE__,__LINE__);
	int i=0;
	for(;i<objEnv->num;i++)
	{
		FklVMenvNode* node=objEnv->list[i];
		FklVMvalue* v=fklCopyVMvalue(node->value,heap);
		FklVMenvNode* tmp=fklNewVMenvNode(v,node->id);
		objEnv->list[i]=tmp;
	}
	tmp->prev=(objEnv->prev->refcount==0)?fklCopyVMenv(objEnv->prev,heap):objEnv->prev;
	return tmp;
}

void fklFreeVMproc(FklVMproc* proc)
{
	if(proc->prevEnv)
		fklFreeVMenv(proc->prevEnv);
	free(proc);
}

void fklFreeVMenv(FklVMenv* obj)
{
	while(obj!=NULL)
	{
		if(!obj->refcount)
		{
			FklVMenv* prev=obj;
			obj=obj->prev;
			int32_t i=0;
			for(;i<prev->num;i++)
				fklFreeVMenvNode(prev->list[i]);
			free(prev->list);
			free(prev);
		}
		else
		{
			fklDecreaseVMenvRefcount(obj);
			break;
		}
	}
}

void fklReleaseSource(pthread_rwlock_t* pGClock)
{
	pthread_rwlock_unlock(pGClock);
}

void fklLockSource(pthread_rwlock_t* pGClock)
{
	pthread_rwlock_rdlock(pGClock);
}

FklVMvalue* fklPopVMstack(FklVMstack* stack)
{
	if(!(stack->tp>stack->bp))
		return NULL;
	FklVMvalue* tmp=fklGetTopValue(stack);
	stack->tp-=1;
	return tmp;
}

FklVMenv* fklCastPreEnvToVMenv(FklPreEnv* pe,FklVMenv* prev,VMheap* heap)
{
	int32_t size=0;
	FklPreDef* tmpDef=pe->symbols;
	while(tmpDef)
	{
		size++;
		tmpDef=tmpDef->next;
	}
	FklVMenv* tmp=fklNewVMenv(prev);
	for(tmpDef=pe->symbols;tmpDef;tmpDef=tmpDef->next)
	{
		FklVMvalue* v=fklCastCptrVMvalue(&tmpDef->obj,heap);
		FklVMenvNode* node=fklNewVMenvNode(v,fklAddSymbolToGlob(tmpDef->symbol)->id);
		fklAddVMenvNode(node,tmp);
	}
	return tmp;
}

VMcontinuation* fklNewVMcontinuation(FklVMstack* stack,FklPtrStack* rstack,FklPtrStack* tstack)
{
	int32_t size=rstack->top;
	int32_t i=0;
	VMcontinuation* tmp=(VMcontinuation*)malloc(sizeof(VMcontinuation));
	FKL_ASSERT(tmp,"fklNewVMcontinuation",__FILE__,__LINE__);
	FklVMrunnable* state=(FklVMrunnable*)malloc(sizeof(FklVMrunnable)*size);
	FKL_ASSERT(state,"fklNewVMcontinuation",__FILE__,__LINE__);
	int32_t tbnum=tstack->top;
	FklVMTryBlock* tb=(FklVMTryBlock*)malloc(sizeof(FklVMTryBlock)*tbnum);
	FKL_ASSERT(tb,"fklNewVMcontinuation",__FILE__,__LINE__);
	tmp->stack=fklCopyStack(stack);
	tmp->num=size;
	for(;i<size;i++)
	{
		FklVMrunnable* cur=rstack->data[i];
		state[i].cp=cur->cp;
		fklIncreaseVMenvRefcount(cur->localenv);
		state[i].localenv=cur->localenv;
		state[i].cpc=cur->cpc;
		state[i].scp=cur->scp;
		state[i].sid=cur->sid;
		state[i].mark=cur->mark;
	}
	tmp->tnum=tbnum;
	for(i=0;i<tbnum;i++)
	{
		FklVMTryBlock* cur=tstack->data[i];
		tb[i].sid=cur->sid;
		FklPtrStack* hstack=cur->hstack;
		int32_t handlerNum=hstack->top;
		FklPtrStack* curHstack=fklNewPtrStack(handlerNum);
		int32_t j=0;
		for(;j<handlerNum;j++)
		{
			FklVMerrorHandler* curH=hstack->data[i];
			FklVMerrorHandler* h=fklNewVMerrorHandler(curH->type,curH->proc.scp,curH->proc.cpc);
			fklPushPtrStack(h,curHstack);
		}
		tb[i].hstack=curHstack;
		tb[i].rtp=cur->rtp;
		tb[i].tp=cur->tp;
	}
	tmp->state=state;
	tmp->tb=tb;
	return tmp;
}

void fklFreeVMcontinuation(VMcontinuation* cont)
{
	int32_t i=0;
	int32_t size=cont->num;
	int32_t tbsize=cont->tnum;
	FklVMstack* stack=cont->stack;
	FklVMrunnable* state=cont->state;
	FklVMTryBlock* tb=cont->tb;
	free(stack->tpst);
	free(stack->values);
	free(stack);
	for(;i<size;i++)
		fklFreeVMenv(state[i].localenv);
	for(i=0;i<tbsize;i++)
	{
		FklPtrStack* hstack=tb[i].hstack;
		while(!fklIsPtrStackEmpty(hstack))
		{
			FklVMerrorHandler* h=fklPopPtrStack(hstack);
			fklFreeVMerrorHandler(h);
		}
		fklFreePtrStack(hstack);
	}
	free(tb);
	free(state);
	free(cont);
}

FklVMstack* fklCopyStack(FklVMstack* stack)
{
	int32_t i=0;
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp,"fklCopyStack",__FILE__,__LINE__);
	tmp->size=stack->size;
	tmp->tp=stack->tp;
	tmp->bp=stack->bp;
	tmp->values=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*(tmp->size));
	FKL_ASSERT(tmp->values,"fklCopyStack",__FILE__,__LINE__);
	for(;i<stack->tp;i++)
		tmp->values[i]=stack->values[i];
	tmp->tptp=stack->tptp;
	tmp->tpst=NULL;
	tmp->tpsi=stack->tpsi;
	if(tmp->tpsi)
	{
		tmp->tpst=(uint32_t*)malloc(sizeof(uint32_t)*tmp->tpsi);
		FKL_ASSERT(tmp->tpst,"fklCopyStack",__FILE__,__LINE__);
		if(tmp->tptp)memcpy(tmp->tpst,stack->tpst,sizeof(int32_t)*(tmp->tptp));
	}
	return tmp;
}

FklVMenvNode* fklNewVMenvNode(FklVMvalue* value,int32_t id)
{
	FklVMenvNode* tmp=(FklVMenvNode*)malloc(sizeof(FklVMenvNode));
	FKL_ASSERT(tmp,"fklNewVMenvNode",__FILE__,__LINE__);
	tmp->id=id;
	tmp->value=value;
	return tmp;
}

FklVMenvNode* fklAddVMenvNode(FklVMenvNode* node,FklVMenv* env)
{
	if(!env->list)
	{
		env->num=1;
		env->list=(FklVMenvNode**)malloc(sizeof(FklVMenvNode*)*1);
		FKL_ASSERT(env->list,"fklAddVMenvNode",__FILE__,__LINE__);
		env->list[0]=node;
	}
	else
	{
		int32_t l=0;
		int32_t h=env->num-1;
		int32_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			if(env->list[mid]->id>=node->id)
				h=mid-1;
			else
				l=mid+1;
		}
		if(env->list[mid]->id<=node->id)
			mid++;
		env->num+=1;
		int32_t i=env->num-1;
		env->list=(FklVMenvNode**)realloc(env->list,sizeof(FklVMenvNode*)*env->num);
		FKL_ASSERT(env->list,"fklAddVMenvNode",__FILE__,__LINE__);
		for(;i>mid;i--)
			env->list[i]=env->list[i-1];
		env->list[mid]=node;
	}
	return node;
}

FklVMenvNode* fklFindVMenvNode(FklSid_t id,FklVMenv* env)
{
	if(!env->list)
		return NULL;
	int32_t l=0;
	int32_t h=env->num-1;
	int32_t mid;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		if(env->list[mid]->id>id)
			h=mid-1;
		else if(env->list[mid]->id<id)
			l=mid+1;
		else
			return env->list[mid];
	}
	return NULL;
}

void fklFreeVMenvNode(FklVMenvNode* node)
{
	free(node);
}


FklVMChanl* fklNewVMChanl(int32_t maxSize)
{
	FklVMChanl* tmp=(FklVMChanl*)malloc(sizeof(FklVMChanl));
	FKL_ASSERT(tmp,"fklNewVMChanl",__FILE__,__LINE__);
	pthread_mutex_init(&tmp->lock,NULL);
	tmp->max=maxSize;
	tmp->messages=fklNewPtrQueue();
	tmp->sendq=fklNewPtrQueue();
	tmp->recvq=fklNewPtrQueue();
	return tmp;
}

int32_t fklGetNumVMChanl(FklVMChanl* ch)
{
	pthread_rwlock_rdlock((pthread_rwlock_t*)&ch->lock);
	uint32_t i=0;
	i=fklLengthPtrQueue(ch->messages);
	pthread_rwlock_unlock((pthread_rwlock_t*)&ch->lock);
	return i;
}

void fklFreeVMChanl(FklVMChanl* ch)
{
	pthread_mutex_destroy(&ch->lock);
	fklFreePtrQueue(ch->messages);
	FklQueueNode* head=ch->sendq->head;
	for(;head;head=head->next)
		fklFreeSendT(head->data);
	for(head=ch->recvq->head;head;head=head->next)
		fklFreeRecvT(head->data);
	fklFreePtrQueue(ch->sendq);
	fklFreePtrQueue(ch->recvq);
	free(ch);
}

FklVMChanl* fklCopyVMChanl(FklVMChanl* ch,VMheap* heap)
{
	FklVMChanl* tmpCh=fklNewVMChanl(ch->max);
	FklQueueNode* cur=ch->messages->head;
	FklPtrQueue* tmp=fklNewPtrQueue();
	for(;cur;cur=cur->next)
	{
		void* message=fklCopyVMvalue(cur->data,heap);
		fklPushPtrQueue(message,tmp);
	}
	return tmpCh;
}

void fklFreeVMfp(FILE* fp)
{
	if(fp!=stdin&&fp!=stdout&&fp!=stderr)
		fclose(fp);
}

FklDllHandle* fklNewVMDll(const char* dllName)
{
#ifdef _WIN32
	char filetype[]=".dll";
#else
	char filetype[]=".so";
#endif
	size_t len=strlen(dllName)+strlen(filetype)+1;
	char* realDllName=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(realDllName,"fklNewVMDll",__FILE__,__LINE__);
	sprintf(realDllName,"%s%s",dllName,filetype);
#ifdef _WIN32
	char* rpath=_fullpath(NULL,realDllName,0);
#else
	char* rpath=realpath(realDllName,0);
#endif
	if(!rpath)
	{
		perror(dllName);
		free(realDllName);
		return NULL;
	}
#ifdef _WIN32
	FklDllHandle handle=LoadLibrary(rpath);
	if(!handle)
	{
		TCHAR szBuf[128];
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage (
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				dw,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
				(LPTSTR) &lpMsgBuf,
				0, NULL );
		wsprintf(szBuf,
				_T("%s error Message (error code=%d): %s"),
				_T("CreateDirectory"), dw, lpMsgBuf);
		LocalFree(lpMsgBuf);
		fprintf(stderr,"%s\n",szBuf);
		free(rpath);
		free(realDllName);
		return NULL;
	}
#else
	FklDllHandle handle=dlopen(rpath,RTLD_LAZY);
	if(!handle)
	{
		perror(dlerror());
		putc('\n',stderr);
		free(rpath);
		free(realDllName);
		return NULL;
	}
#endif
	free(realDllName);
	free(rpath);
	return handle;
}

void fklFreeVMDll(FklDllHandle* dll)
{
	if(dll)
	{
#ifdef _WIN32
		FreeLibrary(dll);
#else
		dlclose(dll);
#endif
	}
}

void* fklGetAddress(const char* funcname,FklDllHandle dlhandle)
{
	void* pfunc=NULL;
#ifdef _WIN32
		pfunc=GetProcAddress(dlhandle,funcname);
#else
		pfunc=dlsym(dlhandle,funcname);
#endif
	return pfunc;
}

FklVMDlproc* fklNewVMDlproc(FklDllFunc address,FklVMvalue* dll)
{
	FklVMDlproc* tmp=(FklVMDlproc*)malloc(sizeof(FklVMDlproc));
	FKL_ASSERT(tmp,"fklNewVMDlproc",__FILE__,__LINE__);
	tmp->func=address;
	tmp->dll=dll;
	tmp->sid=0;
	return tmp;
}

void fklFreeVMDlproc(FklVMDlproc* dlproc)
{
	free(dlproc);
}

FklVMFlproc* fklNewVMFlproc(FklTypeId_t type,void* func)
{
	FklVMFlproc* tmp=(FklVMFlproc*)malloc(sizeof(FklVMFlproc));
	FKL_ASSERT(tmp,"fklNewVMDlproc",__FILE__,__LINE__);
	tmp->type=type;
	tmp->func=func;
	FklVMFuncType* ft=(FklVMFuncType*)GET_TYPES_PTR(fklGetVMTypeUnion(type).all);
	uint32_t anum=ft->anum;
	FklTypeId_t* atypes=ft->atypes;
	ffi_type** ffiAtypes=(ffi_type**)malloc(sizeof(ffi_type*)*anum);
	FKL_ASSERT(ffiAtypes,"invokeFlproc",__FILE__,__LINE__);
	uint32_t i=0;
	for(;i<anum;i++)
	{
		if(atypes[i]>LastNativeTypeId)
			ffiAtypes[i]=fklGetFfiType(LastNativeTypeId);
		else
			ffiAtypes[i]=fklGetFfiType(atypes[i]);
	}
	FklTypeId_t rtype=ft->rtype;
	ffi_type* ffiRtype=NULL;
	if(rtype>LastNativeTypeId)
		ffiRtype=fklGetFfiType(LastNativeTypeId);
	else
		ffiRtype=fklGetFfiType(rtype);
	fklPrepFFIcif(&tmp->cif,anum,ffiAtypes,ffiRtype);
	tmp->atypes=ffiAtypes;
	tmp->sid=0;
	return tmp;
}

void fklFreeVMFlproc(FklVMFlproc* t)
{
	free(t->atypes);
	free(t);
}

FklVMerror* fklNewVMerror(const char* who,const char* type,const char* message)
{
	FklVMerror* t=(FklVMerror*)malloc(sizeof(FklVMerror));
	FKL_ASSERT(t,"fklNewVMerror",__FILE__,__LINE__);
	t->who=fklCopyStr(who);
	t->type=fklAddSymbolToGlob(type)->id;
	t->message=fklCopyStr(message);
	return t;
}

FklVMerror* fklNewVMerrorWithSid(const char* who,FklSid_t type,const char* message)
{
	FklVMerror* t=(FklVMerror*)malloc(sizeof(FklVMerror));
	FKL_ASSERT(t,"fklNewVMerror",__FILE__,__LINE__);
	t->who=fklCopyStr(who);
	t->type=type;
	t->message=fklCopyStr(message);
	return t;
}

void fklFreeVMerror(FklVMerror* err)
{
	free(err->who);
	free(err->message);
	free(err);
}

FklRecvT* fklNewRecvT(FklVM* v)
{
	FklRecvT* tmp=(FklRecvT*)malloc(sizeof(FklRecvT));
	FKL_ASSERT(tmp,"fklNewRecvT",__FILE__,__LINE__);
	tmp->v=v;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void fklFreeRecvT(FklRecvT* r)
{
	pthread_cond_destroy(&r->cond);
	free(r);
}

FklSendT* fklNewSendT(FklVMvalue* m)
{
	FklSendT* tmp=(FklSendT*)malloc(sizeof(FklSendT));
	FKL_ASSERT(tmp,"fklNewSendT",__FILE__,__LINE__);
	tmp->m=m;
	pthread_cond_init(&tmp->cond,NULL);
	return tmp;
}

void fklFreeSendT(FklSendT* s)
{
	pthread_cond_destroy(&s->cond);
	free(s);
}

void fklChanlRecv(FklRecvT* r,FklVMChanl* ch)
{
	pthread_mutex_lock(&ch->lock);
	if(!fklLengthPtrQueue(ch->messages))
	{
		fklPushPtrQueue(r,ch->recvq);
		pthread_cond_wait(&r->cond,&ch->lock);
	}
	SET_RETURN("fklChanlRecv",fklPopPtrQueue(ch->messages),r->v->stack);
	if(fklLengthPtrQueue(ch->messages)<ch->max)
	{
		FklSendT* s=fklPopPtrQueue(ch->sendq);
		if(s)
			pthread_cond_signal(&s->cond);
	}
	fklFreeRecvT(r);
	pthread_mutex_unlock(&ch->lock);
}

void fklChanlSend(FklSendT*s,FklVMChanl* ch)
{
	pthread_mutex_lock(&ch->lock);
	if(fklLengthPtrQueue(ch->recvq))
	{
		FklRecvT* r=fklPopPtrQueue(ch->recvq);
		if(r)
			pthread_cond_signal(&r->cond);
	}
	if(!ch->max||fklLengthPtrQueue(ch->messages)<ch->max-1)
		fklPushPtrQueue(s->m,ch->messages);
	else
	{
		if(fklLengthPtrQueue(ch->messages)>=ch->max-1)
		{
			fklPushPtrQueue(s,ch->sendq);
			if(fklLengthPtrQueue(ch->messages)==ch->max-1)
				fklPushPtrQueue(s->m,ch->messages);
			pthread_cond_wait(&s->cond,&ch->lock);
		}
	}
	fklFreeSendT(s);
	pthread_mutex_unlock(&ch->lock);
}

FklVMTryBlock* fklNewVMTryBlock(FklSid_t sid,uint32_t tp,long int rtp)
{
	FklVMTryBlock* t=(FklVMTryBlock*)malloc(sizeof(FklVMTryBlock));
	FKL_ASSERT(t,"fklNewVMTryBlock",__FILE__,__LINE__);
	t->sid=sid;
	t->hstack=fklNewPtrStack(32);
	t->tp=tp;
	t->rtp=rtp;
	return t;
}

void fklFreeVMTryBlock(FklVMTryBlock* b)
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

FklVMerrorHandler* fklNewVMerrorHandler(FklSid_t type,uint32_t scp,uint32_t cpc)
{
	FklVMerrorHandler* t=(FklVMerrorHandler*)malloc(sizeof(FklVMerrorHandler));
	FKL_ASSERT(t,"fklNewVMerrorHandler",__FILE__,__LINE__);
	t->type=type;
	t->proc.prevEnv=NULL;
	t->proc.scp=scp;
	t->proc.cpc=cpc;
	return t;
}

void fklFreeVMerrorHandler(FklVMerrorHandler* h)
{
	fklFreeVMenv(h->proc.prevEnv);
	free(h);
}

extern const char*  builtInErrorType[NUM_OF_BUILT_IN_ERROR_TYPE];

int fklRaiseVMerror(FklVMvalue* ev,FklVM* exe)
{
	FklVMerror* err=ev->u.err;
	while(!fklIsPtrStackEmpty(exe->tstack))
	{
		FklVMTryBlock* tb=fklTopPtrStack(exe->tstack);
		FklVMstack* stack=exe->stack;
		stack->tp=tb->tp;
		while(!fklIsPtrStackEmpty(tb->hstack))
		{
			FklVMerrorHandler* h=fklPopPtrStack(tb->hstack);
			if(h->type==err->type)
			{
				long int increment=exe->rstack->top-tb->rtp;
				unsigned int i=0;
				for(;i<increment;i++)
				{
					FklVMrunnable* runnable=fklPopPtrStack(exe->rstack);
					fklFreeVMenv(runnable->localenv);
					free(runnable);
				}
				FklVMrunnable* prevRunnable=fklTopPtrStack(exe->rstack);
				FklVMrunnable* r=fklNewVMrunnable(&h->proc);
				r->localenv=fklNewVMenv(prevRunnable->localenv);
				FklVMenv* curEnv=r->localenv;
				FklSid_t idOfError=tb->sid;
				FklVMenvNode* errorNode=fklFindVMenvNode(idOfError,curEnv);
				if(!errorNode)
					errorNode=fklAddVMenvNode(fklNewVMenvNode(NULL,idOfError),curEnv);
				errorNode->value=ev;
				fklPushPtrStack(r,exe->rstack);
				fklFreeVMerrorHandler(h);
				return 1;
			}
			fklFreeVMerrorHandler(h);
		}
		fklFreeVMTryBlock(fklPopPtrStack(exe->tstack));
	}
	fprintf(stderr,"error of %s :%s",err->who,err->message);
	void* i[3]={exe,(void*)255,(void*)1};
	exe->callback(i);
	return 255;
}

FklVMrunnable* fklNewVMrunnable(FklVMproc* code)
{
	FklVMrunnable* tmp=(FklVMrunnable*)malloc(sizeof(FklVMrunnable));
	FKL_ASSERT(tmp,"fklNewVMrunnable",__FILE__,__LINE__);
	tmp->sid=0;
	tmp->cp=0;
	tmp->scp=0;
	tmp->cpc=0;
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

char* fklGenInvalidSymbolErrorMessage(const char* str,FklVMrunnable* r,FklVM* exe)
{
	int32_t cp=r->cp;
	LineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
	char* line=fklIntToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(lineNumber,"fklGenInvalidSymbolErrorMessag",__FILE__,__LINE__);
	sprintf(lineNumber,"at line %s of %s\n",line,filename);
	free(line);
	char* t=fklCopyStr("");
	t=fklStrCat(t,"Invalid symbol ");
	t=fklStrCat(t,str);
	t=fklStrCat(t," ");
	t=fklStrCat(t,lineNumber);
	free(lineNumber);
	for(uint32_t i=exe->rstack->top;i;i--)
	{
		FklVMrunnable* r=exe->rstack->data[i-1];
		if(r->sid!=0)
		{
			t=fklStrCat(t,"at proc:");
			t=fklStrCat(t,fklGetGlobSymbolWithId(r->sid)->symbol);
		}
		else
		{
			if(i>1)
				t=fklStrCat(t,"at <lambda>");
			else
				t=fklStrCat(t,"at <top>");
		}
		LineNumTabNode* node=fklFindLineNumTabNode(r->cp,exe->lnt);
		char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
		char* line=fklIntToString(node->line);
		size_t len=strlen("(:)\n")+strlen(filename)+strlen(line)+1;
		char* lineNumber=(char*)malloc(sizeof(char)*len);
		FKL_ASSERT(lineNumber,"fklGenErrorMessage",__FILE__,__LINE__);
		sprintf(lineNumber,"(%s:%s)\n",line,filename);
		free(line);
		t=fklStrCat(t,lineNumber);
		free(lineNumber);
	}

	return t;
}

char* fklGenErrorMessage(unsigned int type,FklVMrunnable* r,FklVM* exe)
{
	int32_t cp=r->cp;
	LineNumTabNode* node=fklFindLineNumTabNode(cp,exe->lnt);
	char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
	char* line=fklIntToString(node->line);
	size_t len=strlen("at line  of \n")+strlen(filename)+strlen(line)+1;
	char* lineNumber=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(lineNumber,"fklGenErrorMessage",__FILE__,__LINE__);
	sprintf(lineNumber,"at line %s of %s\n",line,filename);
	free(line);
	char* t=fklCopyStr("");
	switch(type)
	{
		case FKL_WRONGARG:
			t=fklStrCat(t,"Wrong arguement ");
			break;
		case FKL_STACKERROR:
			t=fklStrCat(t,"Stack error ");
			break;
		case FKL_TOOMANYARG:
			t=fklStrCat(t,"Too many arguements ");
			break;
		case FKL_TOOFEWARG:
			t=fklStrCat(t,"Too few arguements ");
			break;
		case FKL_CANTCREATETHREAD:
			t=fklStrCat(t,"Can't create thread ");
			break;
		case FKL_SYMUNDEFINE:
			t=fklStrCat(t,"Symbol ");
			t=fklStrCat(t,fklGetGlobSymbolWithId(fklGetSymbolIdInByteCode(exe->code+r->cp))->symbol);
			t=fklStrCat(t," is undefined ");
			break;
		case FKL_INVOKEERROR:
			t=fklStrCat(t,"Try to invoke an object that isn't procedure,continuation or native procedure ");
			break;
		case FKL_LOADDLLFAILD:
			t=fklStrCat(t,"Faild to load dll \"");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=fklStrCat(t,"\" ");
			break;
		case FKL_INVALIDSYMBOL:
			t=fklStrCat(t,"Invalid symbol ");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=fklStrCat(t," ");
			break;
		case FKL_DIVZERROERROR:
			t=fklStrCat(t,"Divided by zero ");
			break;
		case FKL_FILEFAILURE:
			t=fklStrCat(t,"Failed for file:\"");
			t=fklStrCat(t,exe->stack->values[exe->stack->tp-1]->u.str);
			t=fklStrCat(t,"\" ");
			break;
		case FKL_INVALIDASSIGN:
			t=fklStrCat(t,"Invalid assign ");
			break;
		case FKL_INVALIDACCESS:
			t=fklStrCat(t,"Invalid access ");
			break;
	}
	t=fklStrCat(t,lineNumber);
	free(lineNumber);
	for(uint32_t i=exe->rstack->top;i;i--)
	{
		FklVMrunnable* r=exe->rstack->data[i-1];
		if(r->sid!=0)
		{
			t=fklStrCat(t,"at proc:");
			t=fklStrCat(t,fklGetGlobSymbolWithId(r->sid)->symbol);
		}
		else
		{
			if(i>1)
				t=fklStrCat(t,"at <lambda>");
			else
				t=fklStrCat(t,"at <top>");
		}
		LineNumTabNode* node=fklFindLineNumTabNode(r->cp,exe->lnt);
		char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
		char* line=fklIntToString(node->line);
		size_t len=strlen("(:)\n")+strlen(filename)+strlen(line)+1;
		char* lineNumber=(char*)malloc(sizeof(char)*len);
		FKL_ASSERT(lineNumber,"fklGenErrorMessage",__FILE__,__LINE__);
		sprintf(lineNumber,"(%s:%s)\n",line,filename);
		free(line);
		t=fklStrCat(t,lineNumber);
		free(lineNumber);
	}
	return t;
}

int32_t fklGetSymbolIdInByteCode(const uint8_t* code)
{
	char op=*code;
	switch(op)
	{
		case FKL_PUSH_VAR:
			return *(int32_t*)(code+sizeof(char));
			break;
		case FKL_POP_VAR:
		case FKL_POP_ARG:
		case FKL_POP_REST_ARG:
			return *(int32_t*)(code+sizeof(char)+sizeof(int32_t));
			break;
	}
	return -1;
}

int fklResBp(FklVMstack* stack)
{
	if(stack->tp>stack->bp)
		return FKL_TOOMANYARG;
	FklVMvalue* prevBp=fklGetTopValue(stack);
	stack->bp=GET_I32(prevBp);
	stack->tp-=1;
	return 0;
}

void fklPrincVMvalue(FklVMvalue* objValue,FILE* fp,CRL** h)
{
	int access=!h;
	CRL* head=NULL;
	if(!h)h=&head;
	FklVMpair* cirPair=NULL;
	int32_t CRLcount=-1;
	int8_t isInCir=0;
	FklVMptrTag tag=GET_TAG(objValue);
	switch(tag)
	{
		case FKL_NIL_TAG:
			fprintf(fp,"()");
			break;
		case FKL_I32_TAG:
			fprintf(fp,"%d",GET_I32(objValue));
			break;
		case FKL_CHR_TAG:
			putc(GET_CHR(objValue),fp);
			break;
		case FKL_SYM_TAG:
			fprintf(fp,"%s",fklGetGlobSymbolWithId(GET_SYM(objValue))->symbol);
			break;
		case FKL_PTR_TAG:
			{
				switch(objValue->type)
				{
					case FKL_DBL:
						fprintf(fp,"%lf",objValue->u.dbl);
						break;
					case FKL_I64:
						fprintf(fp,"%ld",objValue->u.i64);
						break;
					case FKL_STR:
						fprintf(fp,"%s",objValue->u.str);
						break;
					case FKL_MEM:
					case FKL_CHF:
						{
							FklVMMem* mem=objValue->u.chf;
							FklTypeId_t type=mem->type;
							if(type>0&&type<=LastNativeTypeId)
								PrintMemoryRefFuncList[type-1](mem->mem,fp);
							else if(IS_CHF(objValue))
								fprintf(fp,"<#memref at %p>",mem->mem);
							else
								fprintf(fp,"<#mem at %p>",mem->mem);
						}
						break;
					case FKL_PRC:
						if(objValue->u.prc->sid)
							fprintf(fp,"<#proc: %s>",fklGetGlobSymbolWithId(objValue->u.prc->sid)->symbol);
						else
							fprintf(fp,"#<proc>");
					case FKL_PAIR:
						cirPair=isCircularReference(objValue->u.pair,*h);
						if(cirPair)
							isInCir=isInTheCircle(objValue->u.pair,cirPair,cirPair);
						if(cirPair&&isInCir)
						{
							CRL* crl=newCRL(objValue->u.pair,(*h)?(*h)->count+1:0);
							crl->next=*h;
							*h=crl;
							fprintf(fp,"#%d=(",crl->count);
						}
						else
							putc('(',fp);
						for(;IS_PAIR(objValue);objValue=fklGetVMpairCdr(objValue))
						{
							FklVMvalue* tmpValue=fklGetVMpairCar(objValue);
							if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
								fprintf(fp,"#%d#",CRLcount);
							else
								fklPrincVMvalue(tmpValue,fp,h);
							tmpValue=fklGetVMpairCdr(objValue);
							if(tmpValue!=VM_NIL&&!IS_PAIR(tmpValue))
							{
								putc(',',fp);
								fklPrincVMvalue(tmpValue,fp,h);
							}
							else if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
							{
								fprintf(fp,",#%d#",CRLcount);
								break;
							}
							else if(tmpValue!=VM_NIL)
								putc(' ',fp);
						}
						putc(')',fp);
						break;
					case FKL_BYTS:
						fklPrintByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,0);
						break;
					case FKL_CONT:
						fprintf(fp,"#<cont>");
						break;
					case FKL_CHAN:
						fprintf(fp,"#<chan>");
						break;
					case FKL_FP:
						fprintf(fp,"#<fp>");
						break;
					case FKL_DLL:
						fprintf(fp,"<#dll>");
						break;
					case FKL_DLPROC:
						if(objValue->u.dlproc->sid)
							fprintf(fp,"<#dlproc: %s>",fklGetGlobSymbolWithId(objValue->u.dlproc->sid)->symbol);
						else
							fprintf(fp,"<#dlproc>");
						break;
					case FKL_FLPROC:
						if(objValue->u.flproc->sid)
							fprintf(fp,"<#flproc: %s>",fklGetGlobSymbolWithId(objValue->u.flproc->sid)->symbol);
						else
							fprintf(fp,"<#flproc>");
						break;
					case FKL_ERR:
						fprintf(fp,"%s",objValue->u.err->message);
						break;
					default:
						fprintf(fp,"Bad value!");break;
				}
			}
		default:
			break;
	}
	if(access)
	{
		head=*h;
		while(head)
		{
			CRL* prev=head;
			head=head->next;
			free(prev);
		}
	}
}

void fklPrin1VMvalue(FklVMvalue* objValue,FILE* fp,CRL** h)
{
	int access=!h;
	CRL* head=NULL;
	if(!h)h=&head;
	FklVMpair* cirPair=NULL;
	int8_t isInCir=0;
	int32_t CRLcount=-1;
	FklVMptrTag tag=GET_TAG(objValue);
	switch(tag)
	{
		case FKL_NIL_TAG:
			fprintf(fp,"()");
			break;
		case FKL_I32_TAG:
			fprintf(fp,"%d",GET_I32(objValue));
			break;
		case FKL_CHR_TAG:
			fklPrintRawChar(GET_CHR(objValue),fp);
			break;
		case FKL_SYM_TAG:
			fprintf(fp,"%s",fklGetGlobSymbolWithId(GET_SYM(objValue))->symbol);
			break;
		case FKL_PTR_TAG:
			{
				switch(objValue->type)
				{
					case FKL_DBL:
						fprintf(fp,"%lf",objValue->u.dbl);
						break;
					case FKL_I64:
						fprintf(fp,"%ld",objValue->u.i64);
						break;
					case FKL_STR:
						fklPrintRawString(objValue->u.str,fp);
						break;
					case FKL_MEM:
					case FKL_CHF:
						{
							FklVMMem* mem=objValue->u.chf;
							FklTypeId_t type=mem->type;
							if(type>0&&type<=LastNativeTypeId)
								PrintMemoryRefFuncList[type-1](mem->mem,fp);
							else if(IS_CHF(objValue))
								fprintf(fp,"<#memref at %p>",mem->mem);
							else
								fprintf(fp,"<#mem at %p>",mem->mem);
						}
						break;
					case FKL_PRC:
						if(objValue->u.prc->sid)
							fprintf(fp,"<#proc: %s>",fklGetGlobSymbolWithId(objValue->u.prc->sid)->symbol);
						else
							fprintf(fp,"#<proc>");
						break;
					case FKL_PAIR:
						cirPair=isCircularReference(objValue->u.pair,*h);
						if(cirPair)
							isInCir=isInTheCircle(objValue->u.pair,cirPair,cirPair);
						if(cirPair&&isInCir)
						{
							CRL* crl=newCRL(objValue->u.pair,(*h)?(*h)->count+1:0);
							crl->next=*h;
							*h=crl;
							fprintf(fp,"#%d=(",crl->count);
						}
						else
							putc('(',fp);
						for(;IS_PAIR(objValue);objValue=fklGetVMpairCdr(objValue))
						{
							FklVMvalue* tmpValue=fklGetVMpairCar(objValue);
							if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
								fprintf(fp,"#%d#",CRLcount);
							else
								fklPrin1VMvalue(tmpValue,fp,h);
							tmpValue=fklGetVMpairCdr(objValue);
							if(tmpValue!=VM_NIL&&!IS_PAIR(tmpValue))
							{
								putc(',',fp);
								fklPrin1VMvalue(tmpValue,fp,h);
							}
							else if(IS_PAIR(tmpValue)&&(CRLcount=findCRLcount(tmpValue->u.pair,*h))!=-1)
							{
								fprintf(fp,",#%d#",CRLcount);
								break;
							}
							else if(tmpValue!=VM_NIL)
								putc(' ',fp);
						}
						putc(')',fp);
						break;
					case FKL_BYTS:
						fklPrintByteStr(objValue->u.byts->size,objValue->u.byts->str,fp,1);
						break;
					case FKL_CONT:
						fprintf(fp,"#<cont>");
						break;
					case FKL_CHAN:
						fprintf(fp,"#<chan>");
						break;
					case FKL_FP:
						fprintf(fp,"#<fp>");
						break;
					case FKL_DLL:
						fprintf(fp,"<#dll>");
						break;
					case FKL_DLPROC:
						if(objValue->u.dlproc->sid)
							fprintf(fp,"<#dlproc: %s>",fklGetGlobSymbolWithId(objValue->u.dlproc->sid)->symbol);
						else
							fprintf(fp,"<#dlproc>");
						break;
					case FKL_FLPROC:
						if(objValue->u.flproc->sid)
							fprintf(fp,"<#flproc: %s>",fklGetGlobSymbolWithId(objValue->u.flproc->sid)->symbol);
						else
							fprintf(fp,"<#flproc>");
						break;
					case FKL_ERR:
						fprintf(fp,"<#err w:%s t:%s m:%s>",objValue->u.err->who,fklGetGlobSymbolWithId(objValue->u.err->type)->symbol,objValue->u.err->message);
						break;
					default:fprintf(fp,"Bad value!");break;
				}
			}
		default:
			break;
	}
	if(access)
	{
		head=*h;
		while(head)
		{
			CRL* prev=head;
			head=head->next;
			free(prev);
		}
	}
}

int fklEqVMByts(const FklVMByts* fir,const FklVMByts* sec)
{
	if(fir->size!=sec->size)
		return 0;
	return !memcmp(fir->str,sec->str,sec->size);
}

FklVMvalue* fklGET_VAL(FklVMvalue* P,VMheap* heap)
{
	if(P)
	{
		if(IS_REF(P))
			return *(FklVMvalue**)(GET_PTR(P));
		else if(IS_CHF(P))
		{
			FklVMvalue* t=NULL;
			FklVMMem* mem=P->u.chf;
			if(mem->type>0&&mem->type<=LastNativeTypeId)
			{
				t=MemoryCasterList[mem->type-1](mem->mem,heap);
			}
			else
			{
				t=P;
			}
			return t;
		}
		return P;
	}
	return P;
}

int fklSET_REF(FklVMvalue* P,FklVMvalue* V)
{
	if(IS_MEM(P)||IS_CHF(P))
	{
		FklVMMem* mem=P->u.chf;
		if(mem->type<=0)
			return 1;
		else if(mem->type>LastNativeTypeId)
		{
			if(!IS_I32(V)&&!IS_I64(V))
				return 1;
			mem->mem=(uint8_t*)(IS_I32(V)?GET_I32(V):V->u.i64);
		}
		else if(MemorySeterList[mem->type-1](mem->mem,V))
			return 1;
		return 0;
	}
	else if(IS_REF(P))
	{
		*(FklVMvalue**)GET_PTR(P)=V;
		return 0;
	}
	else
		return 1;
}

FklVMMem* fklNewVMMem(FklTypeId_t type,uint8_t* mem)
{
	FklVMMem* tmp=(FklVMMem*)malloc(sizeof(FklVMMem));
	FKL_ASSERT(tmp,"fklNewVMMem",__FILE__,__LINE__);
	tmp->type=type;
	tmp->mem=mem;
	return tmp;
}
