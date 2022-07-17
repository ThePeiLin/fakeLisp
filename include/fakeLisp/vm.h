#ifndef FKL_VM_H
#define FKL_VM_H

#include"basicADT.h"
#include"bytecode.h"
#include"compiler.h"
#include"builtin.h"
#include<stdio.h>
#include<stdint.h>
#include<pthread.h>
#include<setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	FKL_CC_OK,
	FKL_CC_RE,
}FklCCState;

struct FklVM;
typedef void (*FklVMdllFunc)(struct FklVM*);
typedef struct FklVMvalue* FklVMptr;
typedef enum
{
	FKL_PTR_TAG=0,
	FKL_NIL_TAG,
	FKL_I32_TAG,
	FKL_SYM_TAG,
	FKL_CHR_TAG,
}FklVMptrTag;

#ifdef _WIN32
#include<windows.h>
typedef HMODULE FklVMdllHandle;
#else
#include<dlfcn.h>
typedef void* FklVMdllHandle;
#endif

typedef struct FklVMchanl
{
	uint32_t max;
	volatile uint32_t messageNum;
	volatile uint32_t sendNum;
	volatile uint32_t recvNum;
	pthread_mutex_t lock;
	FklPtrQueue* messages;
	FklPtrQueue* recvq;
	FklPtrQueue* sendq;
}FklVMchanl;

typedef struct
{
	pthread_cond_t cond;
	struct FklVMvalue* m;
}FklVMsend;

typedef struct
{
	pthread_cond_t cond;
	struct FklVMvalue* v;
}FklVMrecv;

typedef struct FklVMpair
{
	struct FklVMvalue* car;
	struct FklVMvalue* cdr;
}FklVMpair;

typedef struct FklVMfp
{
	size_t size;
	uint8_t* prev;
	FILE* fp;
}FklVMfp;

typedef struct FklVMvec
{
	size_t size;
	struct FklVMvalue* base[];
}FklVMvec;

typedef struct FklVMudata
{
	FklSid_t type;
	struct FklVMvalue* rel;
	struct FklVMudMethodTable* t;
	void* data;
}FklVMudata;

typedef enum{
	FKL_MARK_W=0,
	FKL_MARK_G,
	FKL_MARK_B,
}FklVMvalueMark;

typedef struct FklVMvalue
{
	FklVMvalueMark mark;
	FklValueType type;
	union
	{
		struct FklVMpair* pair;
		double f64;
		int64_t i64;
		struct FklString* str;
		struct FklVMproc* proc;
		FklVMdllHandle dll;
		struct FklVMdlproc* dlproc;
		struct FklVMcontinuation* cont;
		FklVMfp* fp;
		FklVMvec* vec;
		struct FklVMenv* env;
		struct FklVMchanl* chan;
		struct FklVMerror* err;
		FklBigInt* bigInt;
		FklVMudata* ud;
		struct FklVMvalue* box;
	}u;
	struct FklVMvalue* next;
}FklVMvalue;

typedef struct FklVMenv
{
	pthread_mutex_t lock;
	struct FklVMvalue* volatile prev;
	FklHashTable* t;
}FklVMenv;

typedef struct FklVMproc
{
	uint64_t scp;
	uint64_t cpc;
	FklSid_t sid;
	FklVMvalue* prevEnv;
}FklVMproc;

typedef void (*FklVMFuncK)(struct FklVM*,FklCCState,void*);

typedef struct FklVMcCC
{
	FklVMFuncK kFunc;
	void* ctx;
	size_t size;
	struct FklVMcCC* next;
}FklVMcCC;

typedef struct FklVMrunnable
{
	unsigned int mark :1;
	FklVMvalue* localenv;
	uint64_t scp;
	uint64_t cp;
	uint64_t cpc;
	FklSid_t sid;
	FklVMcCC* ccc;
	struct FklVMrunnable* prev;
}FklVMrunnable;

typedef struct
{
	FklUintStack* bps;
	FklUintStack* tps;
	uint64_t volatile tp;
	uint64_t bp;
	size_t size;
	FklVMvalue** values;
	pthread_rwlock_t lock;
}FklVMstack;

typedef struct FklVM
{
	unsigned int mark :1;
	int32_t VMid;
	pthread_t tid;
	uint8_t* code;
	uint64_t size;
	pthread_rwlock_t rlock;
	FklVMrunnable* rhead;
	FklPtrStack* tstack;
	FklVMstack* stack;
	struct FklVMvalue* chan;
	struct FklVMheap* heap;
	struct FklLineNumberTable* lnt;
	void (*callback)(void*);
	FklVMvalue* nextCall;
	jmp_buf buf;
	int nny;
}FklVM;

typedef struct FklVMudMethodTable
{
	void (*__princ)(void*,FILE*);
	void (*__prin1)(void*,FILE*);
	void (*__finalizer)(void*);
	int  (*__equal)(const FklVMudata*,const FklVMudata*);
	void (*__call)(void*,FklVM*);
	int (*__cmp)(FklVMvalue*,FklVMvalue*,int*);
	void (*__write)(void*,FILE*);
	void (*__atomic)(void*,struct FklVMheap*);
	size_t (*__length)(void*);
//	FklString* (*__as_str)(void*);
//	FklString* (*__to_str)(void*);
}FklVMudMethodTable;

typedef enum
{
	FKL_GC_NONE=0,
	FKL_GC_MARK_ROOT,
	FKL_GC_PROPAGATE,
	FKL_GC_SWEEP,
	FKL_GC_COLLECT,
	FKL_GC_SWEEPING,
	FKL_GC_DONE,
}FklGCstate;

typedef struct FklVMheap
{
	FklGCstate volatile running;
	pthread_rwlock_t lock;
	size_t volatile num;
	uint32_t threshold;
	FklVMvalue* head;
	pthread_rwlock_t glock;
	struct Graylink* volatile gray;
	size_t volatile grayNum;
	FklVMvalue* white;
}FklVMheap;

typedef struct
{
	uint32_t num;
	FklVM** VMs;
}FklVMlist;

typedef struct FklVMdlproc
{
	FklVMdllFunc func;
	FklVMvalue* dll;
	FklSid_t sid;
}FklVMdlproc;

typedef struct FklVMerror
{
	FklSid_t type;
	FklString* who;
	FklString* message;
}FklVMerror;

typedef struct FklVMcontinuation
{
	uint32_t tnum;
	FklVMstack* stack;
	FklVMrunnable* curr;
	struct FklVMtryBlock* tb;
	FklVMvalue* nextCall;
}FklVMcontinuation;

typedef struct FklVMtryBlock
{
	FklSid_t sid;
	FklPtrStack* hstack;
	FklVMrunnable* curr;
	uint32_t tp;
}FklVMtryBlock;

typedef struct FklVMerrorHandler
{
	FklSid_t* typeIds;
	uint32_t num;
	FklVMproc proc;
}FklVMerrorHandler;

//vmrun

int fklRunVM(FklVM*);
FklVM* fklNewVM(FklByteCode*);
FklVM* fklNewTmpVM(FklByteCode*);
FklVM* fklNewThreadVM(FklVMproc*,FklVMheap*);
FklVM* fklNewThreadCallableObjVM(FklVMrunnable* r,FklVMheap* heap,FklVMvalue*);

void fklFreeVMvalue(FklVMvalue*);
FklVMstack* fklNewVMstack(int32_t);
void fklFreeVMstack(FklVMstack*);
void fklStackRecycle(FklVMstack*);
int fklCreateNewThread(FklVM*);
FklVMlist* fklNewThreadStack(int32_t);
FklVMrunnable* fklHasSameProc(uint32_t,FklVMrunnable*);
int fklIsTheLastExpress(const FklVMrunnable*,const FklVMrunnable*,const FklVM* exe);
FklVMheap* fklNewVMheap();
void fklCreateCallChainWithContinuation(FklVM*,FklVMcontinuation*);
void fklFreeVMheap(FklVMheap*);
void fklFreeAllVMs();
void fklDeleteCallChain(FklVM*);
void fklJoinAllThread();
void fklCancelAllThread();
void fklChangeGCstate(FklGCstate,FklVMheap*);
FklGCstate fklGetGCstate(FklVMheap*);
void fklGetGCstateAndHeapNum(FklVMheap*,FklGCstate* s,int* num);
void* fklGC_threadFunc(void*);
void* fklGC_sweepThreadFunc(void*);
void fklGC_mark(FklVM*);
void fklGC_markValue(FklVMvalue*);
void fklGC_markValueInStack(FklVMstack*);
void fklGC_markValueInEnv(FklVMenv*);
void fklGC_markValueInCallChain(FklPtrStack*);
void fklGC_markMessage(FklQueueNode*);
void fklGC_markSendT(FklQueueNode*);
void fklGC_toGray(FklVMvalue*,FklVMheap*);
void fklGC_reGray(FklVMvalue*,FklVMheap*);
void fklGC_step(FklVM* exe);
void fklGC_joinGCthread(FklVMheap* h);

void fklWaitGC(FklVMheap* h);
void fklFreeAllValues(FklVMheap*);
void fklGC_sweep(FklVMheap*);

void fklDBG_printVMenv(FklVMenv*,FILE*);
void fklDBG_printVMvalue(FklVMvalue*,FILE*);
void fklDBG_printVMstack(FklVMstack*,FILE*,int);

void fklPrintVMvalue(FklVMvalue* value,FILE* fp,void(*atomPrinter)(FklVMvalue* v,FILE* fp));
void fklPrin1VMvalue(FklVMvalue*,FILE*);
void fklPrincVMvalue(FklVMvalue*,FILE*);

//vmutils

FklVMvalue* fklMakeVMint(int64_t r64,FklVMstack*,FklVMheap* heap);
int fklIsVMnumber(FklVMvalue* p);
int fklIsFixint(FklVMvalue* p);
int fklIsInt(FklVMvalue* p);
int fklIsList(FklVMvalue* p);
int64_t fklGetInt(FklVMvalue* p);
double fklGetDouble(FklVMvalue* p);
void fklInitVMRunningResource(FklVM*,FklVMvalue*,FklVMheap* heap,FklByteCodelnt*,uint32_t,uint32_t);
void fklUninitVMRunningResource(FklVM*);

FklVMvalue* fklGetTopValue(FklVMstack* stack);
FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place);
FklVMvalue* fklCastPreEnvToVMenv(FklPreEnv*,FklVMvalue*,FklVMheap*);
FklAstCptr* fklCastVMvalueToCptr(FklVMvalue* value,int32_t curline);

FklVMstack* fklCopyStack(FklVMstack*);
FklVMvalue* fklPopVMstack(FklVMstack*);
FklVMtryBlock* fklNewVMtryBlock(FklSid_t,uint32_t tp,FklVMrunnable* r);
void fklFreeVMtryBlock(FklVMtryBlock* b);

FklVMerrorHandler* fklNewVMerrorHandler(FklSid_t* typeIds,uint32_t,uint64_t scp,uint64_t cpc);
void fklFreeVMerrorHandler(FklVMerrorHandler*);
int fklRaiseVMerror(FklVMvalue* err,FklVM*);
FklVMrunnable* fklNewVMrunnable(FklVMproc*,FklVMrunnable*);
void fklFreeVMrunnable(FklVMrunnable*);
char* fklGenErrorMessage(FklBuiltInErrorType type,FklVMrunnable* r,FklVM* exe);
char* fklGenInvalidSymbolErrorMessage(char* str,int _free,FklBuiltInErrorType);
int32_t fklGetSymbolIdInByteCode(const uint8_t*);

FklVMcCC* fklNewVMcCC(FklVMFuncK kFunc,void* ctx,size_t,FklVMcCC* next);
FklVMcCC* fklCopyVMcCC(FklVMcCC*);
void fklFreeVMcCC(FklVMcCC*);

FklVMcontinuation* fklNewVMcontinuation(uint32_t ap,FklVM*);
void fklFreeVMcontinuation(FklVMcontinuation* cont);

FklVMenv* fklNewGlobVMenv(FklVMvalue*,FklVMheap*);
FklVMenv* fklNewVMenv(FklVMvalue*,FklVMheap*);
FklVMvalue* volatile* fklFindVar(FklSid_t id,FklVMenv*);
FklVMvalue* volatile* fklFindOrAddVar(FklSid_t id,FklVMenv* env);
FklVMvalue* volatile* fklFindOrAddVarWithValue(FklSid_t id,FklVMvalue*,FklVMenv* env);
void fklAtomicVMenv(FklVMenv*,FklVMheap*);
void fklFreeVMenv(FklVMenv*);

FklVMproc* fklNewVMproc(uint64_t scp,uint64_t cpc);

FklVMvalue* fklCopyVMvalue(FklVMvalue*,FklVMstack*,FklVMheap*);
FklVMvalue* fklNewVMvalue(FklValueType,void*,FklVMheap*);
FklVMvalue* fklNewSaveVMvalue(FklValueType,void*);
void fklAddToHeap(FklVMvalue*,FklVMheap*);
FklVMvalue* fklNewTrueValue(FklVMheap*);
FklVMvalue* fklNewNilValue();
FklVMvalue* fklGetTopValue(FklVMstack*);
FklVMvalue* fklGetValue(FklVMstack*,int32_t);
FklVMvalue* fklGetVMpairCar(FklVMvalue*);
FklVMvalue* fklGetVMpairCdr(FklVMvalue*);
int fklVMvaluecmp(FklVMvalue*,FklVMvalue*);
int subfklVMvaluecmp(FklVMvalue*,FklVMvalue*);
int fklNumcmp(FklVMvalue*,FklVMvalue*);

FklVMpair* fklNewVMpair(void);
FklVMvalue* fklNewVMpairV(FklVMvalue* car,FklVMvalue* cdr,FklVMstack*,FklVMheap*);

FklVMchanl* fklNewVMchanl(int32_t size);

void fklFreeVMchanl(FklVMchanl*);
int32_t fklGetNumVMchanl(FklVMchanl*);

void fklFreeVMproc(FklVMproc*);

FklVMfp* fklNewVMfp(FILE*);
int fklFreeVMfp(FklVMfp*);

FklVMdllHandle fklNewVMdll(const char*);
void fklInitVMdll(FklVMvalue* rel);
void* fklGetAddress(const char*,FklVMdllHandle);
void fklFreeVMdll(FklVMdllHandle);

FklVMdlproc* fklNewVMdlproc(FklVMdllFunc,FklVMvalue*);
void fklFreeVMdlproc(FklVMdlproc*);

FklVMerror* fklNewVMerror(const FklString* who,FklSid_t type,const FklString* message);
FklVMerror* fklNewVMerrorCstr(const char* who,FklSid_t type,const char* message);
FklVMerror* fklNewVMerrorMCstr(const FklString* who,FklSid_t type,const char* message);
void fklFreeVMerror(FklVMerror*);


FklVMrecv* fklNewVMrecv(void);
void fklFreeVMrecv(FklVMrecv*);

FklVMsend* fklNewVMsend(FklVMvalue*);
void fklFreeVMsend(FklVMsend*);

void fklChanlSend(FklVMsend*,FklVMchanl*);
void fklChanlRecvOk(FklVMchanl*,FklVMvalue**,int*);
void fklChanlRecv(FklVMrecv*,FklVMchanl*);

FklVMvalue* fklCastCptrVMvalue(FklAstCptr*,FklVMheap*);

FklVMvec* fklNewVMvec(size_t size);
FklVMvalue* fklNewVMvecV(size_t size,FklVMvalue** base,FklVMstack*,FklVMheap*);
FklVMvalue* fklNewVMvecVFromStack(size_t size,FklVMvalue** base,FklVMstack*,FklVMheap*);
void fklFreeVMvec(FklVMvec*);
void fklVMvecCat(FklVMvec**,const FklVMvec*);

FklVMudata* fklNewVMudata(FklSid_t type,FklVMudMethodTable* t,void* mem,FklVMvalue* rel);
int fklIsCallableUd(FklVMvalue*);
void fklFreeVMudata(FklVMudata*);

int fklIsCallable(FklVMvalue*);
void fklInitVMargs(int argc,char** argv);
int fklGetVMargc(void);
char** fklGetVMargv(void);

FklVMvalue* fklPopGetAndMark(FklVMstack* stack,FklVMheap*);
FklVMvalue* fklPopGetAndMarkWithoutLock(FklVMstack* stack,FklVMheap* heap);
FklVMvalue* fklTopGet(FklVMstack* stack);
void fklDecTop(FklVMstack* s);
FklVMvalue* fklNewVMvalueToStack(FklValueType
		,void* p
		,FklVMstack*
		,FklVMheap* heap);
FklVMvalue* fklSetRef(FklVMvalue* volatile*
		,FklVMvalue* v
		,FklVMheap*);

FklVMdllHandle fklLoadDll(const char* path);

void fklPushVMvalue(FklVMvalue* v,FklVMstack* s);

int fklVMcallInDlproc(FklVMvalue*
		,size_t argNum
		,FklVMvalue*[]
		,FklVMrunnable*
		,FklVM*
		,FklVMFuncK
		,void*
		,size_t);

size_t fklVMlistLength(FklVMvalue*);
void fklFreeRunnables(FklVMrunnable* h);
#define FKL_SET_RETURN(fn,v,stack) do{\
	if((stack)->tp>=(stack)->size)\
	{\
		(stack)->values=(FklVMvalue**)realloc((stack)->values,sizeof(FklVMvalue*)*((stack)->size+64));\
		FKL_ASSERT((stack)->values,fn);\
		if((stack)->values==NULL)\
		{\
			fprintf(stderr,"In file \"%s\" line %d\n",__FILE__,__LINE__);\
			perror((fn));\
			exit(1);\
		}\
		(stack)->size+=64;\
	}\
	(stack)->values[(stack)->tp]=(v);\
	(stack)->tp+=1;\
}while(0)

#define FKL_RAISE_BUILTIN_ERROR(WHO,ERRORTYPE,RUNNABLE,EXE) do{\
	char* errorMessage=fklGenErrorMessage((ERRORTYPE),(RUNNABLE),(EXE));\
	FklVMvalue* err=fklNewVMvalueToStack(FKL_ERR,fklNewVMerrorMCstr((WHO),fklGetBuiltInErrorType(ERRORTYPE),errorMessage),(EXE)->stack,(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,ERRORTYPE,RUNNABLE,EXE) do{\
	char* errorMessage=fklGenErrorMessage((ERRORTYPE),(RUNNABLE),(EXE));\
	FklVMvalue* err=fklNewVMvalueToStack(FKL_ERR,fklNewVMerrorCstr((WHO),fklGetBuiltInErrorType(ERRORTYPE),errorMessage),(EXE)->stack,(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(WHO,STR,FREE,ERRORTYPE,EXE) do{\
	char* errorMessage=fklGenInvalidSymbolErrorMessage((STR),(FREE),(ERRORTYPE));\
	FklVMvalue* err=fklNewVMvalueToStack(FKL_ERR,fklNewVMerrorCstr((WHO),fklGetBuiltInErrorType(ERRORTYPE),errorMessage),(EXE)->stack,(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define FKL_UNUSEDBITNUM (3)
#define FKL_PTR_MASK ((intptr_t)0xFFFFFFFFFFFFFFF8)
#define FKL_TAG_MASK ((intptr_t)0x7)

#define FKL_VM_NIL ((FklVMptr)0x1)
#define FKL_VM_TRUE (FKL_MAKE_VM_I32(1))
#define FKL_VM_EOF ((FklVMptr)0x7fffffffa)
#define FKL_MAKE_VM_I32(I) ((FklVMptr)((((uintptr_t)(I))<<FKL_UNUSEDBITNUM)|FKL_I32_TAG))
#define FKL_MAKE_VM_CHR(C) ((FklVMptr)((((uintptr_t)(C))<<FKL_UNUSEDBITNUM)|FKL_CHR_TAG))
#define FKL_MAKE_VM_SYM(S) ((FklVMptr)((((uintptr_t)(S))<<FKL_UNUSEDBITNUM)|FKL_SYM_TAG))
#define FKL_MAKE_VM_PTR(P) ((FklVMptr)(((uintptr_t)(P))|FKL_PTR_TAG))
#define FKL_GET_TAG(P) ((FklVMptrTag)(((uintptr_t)(P))&FKL_TAG_MASK))
#define FKL_GET_PTR(P) ((FklVMptr)(((uintptr_t)(P))&FKL_PTR_MASK))
#define FKL_GET_I32(P) ((int32_t)((uintptr_t)(P)>>FKL_UNUSEDBITNUM))
#define FKL_GET_CHR(P) ((char)((uintptr_t)(P)>>FKL_UNUSEDBITNUM))
#define FKL_GET_SYM(P) ((FklSid_t)((uintptr_t)(P)>>FKL_UNUSEDBITNUM))
#define FKL_IS_NIL(P) ((P)==FKL_VM_NIL)
#define FKL_IS_PTR(P) (FKL_GET_TAG(P)==FKL_PTR_TAG)
#define FKL_IS_PAIR(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_PAIR)
#define FKL_IS_F64(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_F64)
#define FKL_IS_STR(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_STR)
#define FKL_IS_CHAN(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_CHAN)
#define FKL_IS_FP(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_FP)
#define FKL_IS_DLL(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_DLL)
#define FKL_IS_PROC(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_PROC)
#define FKL_IS_DLPROC(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_DLPROC)
#define FKL_IS_VECTOR(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_VECTOR)
#define FKL_IS_ERR(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_ERR)
#define FKL_IS_CONT(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_CONT)
#define FKL_IS_I32(P) (FKL_GET_TAG(P)==FKL_I32_TAG)
#define FKL_IS_CHR(P) (FKL_GET_TAG(P)==FKL_CHR_TAG)
#define FKL_IS_SYM(P) (FKL_GET_TAG(P)==FKL_SYM_TAG)
#define FKL_IS_I64(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_I64)
#define FKL_IS_USERDATA(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_USERDATA)
#define FKL_IS_BIG_INT(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_BIG_INT)
#define FKL_IS_BOX(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_BOX)

#ifdef __cplusplus
}
#endif

#endif
