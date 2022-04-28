#ifndef FKL_VM_H
#define FKL_VM_H

#include"basicADT.h"
#include"bytecode.h"
#include"compiler.h"
#include<stdio.h>
#include<stdint.h>
#include<pthread.h>
#include<setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklVMvalue* FklVMptr;
typedef enum
{
	FKL_PTR_TAG=0,
	FKL_NIL_TAG,
	FKL_I32_TAG,
	FKL_SYM_TAG,
	FKL_CHR_TAG,
	FKL_REF_TAG,
	FKL_MREF_TAG,
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
	pthread_mutex_t lock;
	FklPtrQueue* messages;
	FklPtrQueue* sendq;
	FklPtrQueue* recvq;
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

//typedef struct FklVMbyts
//{
//	uint64_t size;
//	uint8_t str[];
//}FklVMbyts;

typedef struct FklVMstr
{
	uint64_t size;
	char str[];
}FklVMstr;

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

typedef enum{
	FKL_MARK_W=0,
	FKL_MARK_G,
	FKL_MARK_B,
}FklVMvalueMark;
typedef struct FklVMvalue
{
	volatile FklVMvalueMark mark;
	FklValueType type;
	union
	{
		struct FklVMpair* pair;
		double f64;
		int64_t i64;
		struct FklVMstr* str;
		struct FklVMproc* proc;
		FklVMdllHandle dll;
		struct FklVMdlproc* dlproc;
		struct FklVMcontinuation* cont;
		FklVMfp* fp;
		FklVMvec* vec;
		struct FklVMenv* env;
		struct FklVMchanl* chan;
		struct FklVMerror* err;
		void* p;
	}u;
	volatile struct FklVMvalue* next;
}FklVMvalue;

typedef struct FklVMenvNode
{
	uint32_t id;
	FklVMvalue* value;
}FklVMenvNode;

typedef struct FklVMenv
{
	pthread_mutex_t mutex;
	struct FklVMvalue* prev;
	volatile uint32_t num;
	FklVMenvNode** list;
}FklVMenv;

typedef struct FklVMproc
{
	uint64_t scp;
	uint64_t cpc;
	FklSid_t sid;
	FklVMvalue* prevEnv;
}FklVMproc;

typedef struct FklVMrunnable
{
	unsigned int mark :1;
	FklVMvalue* localenv;
	uint64_t scp;
	uint64_t cp;
	uint64_t cpc;
	FklSid_t sid;
}FklVMrunnable;

typedef struct
{
	volatile uint32_t tp;
	uint32_t bp;
	uint32_t size;
	FklVMvalue** values;
	uint32_t tpsi;
	uint32_t tptp;
	uint32_t* tpst;
	pthread_mutex_t lock;
}FklVMstack;

typedef struct FklVM
{
	unsigned int mark :1;
	int32_t VMid;
	pthread_t tid;
	uint8_t* code;
	uint64_t size;
	pthread_mutex_t rlock;
	FklPtrStack* rstack;
	FklPtrStack* tstack;
	FklVMstack* stack;
	struct FklVMvalue* chan;
	struct FklVMheap* heap;
	struct FklLineNumberTable* lnt;
	void (*callback)(void*);
	jmp_buf buf;
}FklVM;

typedef enum
{
	FKL_GC_RUNNING=1,
	FKL_GC_DONE,
}FklGCState;

typedef struct FklVMheap
{
	volatile FklGCState running;
	pthread_mutex_t lock;
	volatile uint32_t num;
	uint32_t threshold;
	volatile FklVMvalue* head;
	pthread_mutex_t glock;
	volatile struct Graylink* gray;
	volatile FklVMvalue* white;
}FklVMheap;

typedef struct
{
	uint32_t num;
	FklVM** VMs;
}FklVMlist;

typedef void (*FklVMdllFunc)(FklVM*);

typedef struct FklVMdlproc
{
	FklVMdllFunc func;
	FklVMvalue* dll;
	FklSid_t sid;
}FklVMdlproc;

typedef struct FklVMerror
{
	FklSid_t type;
	char* who;
	char* message;
}FklVMerror;

typedef struct FklVMcontinuation
{
	uint32_t num;
	uint32_t tnum;
	FklVMstack* stack;
	FklVMrunnable* state;
	struct FklVMtryBlock* tb;
}VMcontinuation;

typedef struct FklVMtryBlock
{
	FklSid_t sid;
	FklPtrStack* hstack;
	long int rtp;
	uint32_t tp;
}FklVMtryBlock;

typedef struct FklVMerrorHandler
{
	FklSid_t type;
	FklVMproc proc;
}FklVMerrorHandler;

//vmrun

int fklRunVM(FklVM*);
FklVM* fklNewVM(FklByteCode*);
FklVM* fklNewTmpVM(FklByteCode*);
FklVM* fklNewThreadVM(FklVMproc*,FklVMheap*);
FklVM* fklNewThreadDlprocVM(FklVMrunnable* r,FklVMheap* heap);
void fklInitGlobEnv(FklVMenv*,FklVMheap*);

void fklFreeVMvalue(volatile FklVMvalue*);
FklVMstack* fklNewVMstack(int32_t);
void fklFreeVMstack(FklVMstack*);
void fklStackRecycle(FklVM*);
int fklCreateNewThread(FklVM*);
FklVMlist* fklNewThreadStack(int32_t);
FklVMrunnable* fklHasSameProc(uint32_t,FklPtrStack*);
int fklIsTheLastExpress(const FklVMrunnable*,const FklVMrunnable*,const FklVM* exe);
FklVMheap* fklNewVMheap();
void fklCreateCallChainWithContinuation(FklVM*,VMcontinuation*);
void fklFreeVMheap(FklVMheap*);
void fklFreeAllVMs();
void fklDeleteCallChain(FklVM*);
void fklJoinAllThread();
void fklCancelAllThread();
void* fklGC_threadFunc(void*);
void fklGC_mark(FklVM*);
void fklGC_markValue(FklVMvalue*);
void fklGC_markValueInStack(FklVMstack*);
void fklGC_markValueInEnv(FklVMenv*);
void fklGC_markValueInCallChain(FklPtrStack*);
void fklGC_markMessage(FklQueueNode*);
void fklGC_markSendT(FklQueueNode*);
void fklGC_sweep(FklVMheap*);
void fklGC_compact(FklVMheap*);
void fklGC_toGray(FklVMvalue*,FklVMheap*);
void fklGC_tryRun(FklVM* exe);
void fklGC_tryJoin(FklVMheap* exe);
void fklGC_wait(FklVMheap* h);
void fklGC_joinGCthread(FklVMheap* h);

void fklDBG_printVMenv(FklVMenv*,FILE*);
void fklDBG_printVMvalue(FklVMvalue*,FILE*);
void fklDBG_printVMstack(FklVMstack*,FILE*,int);

FklVMvalue* fklGET_VAL(FklVMvalue* P,FklVMheap*);
int fklSET_REF(FklVMvalue* P,FklVMvalue* V);
void fklPrintVMvalue(FklVMvalue* value,FILE* fp,void(*atomPrinter)(FklVMvalue* v,FILE* fp));
void fklPrin1VMvalue(FklVMvalue*,FILE*);
void fklPrincVMvalue(FklVMvalue*,FILE*);

//

//vmutils

FklVMvalue* fklMakeVMint(int64_t r64,FklVMstack*,FklVMheap* heap);
int fklIsInt(FklVMvalue* p);
int64_t fklGetInt(FklVMvalue* p);
void fklInitVMRunningResource(FklVM*,FklVMvalue*,FklVMheap* heap,FklByteCodelnt*,uint32_t,uint32_t);
void fklUninitVMRunningResource(FklVM*);

FklVMvalue* fklGetTopValue(FklVMstack* stack);
FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place);
FklVMvalue* fklCastPreEnvToVMenv(FklPreEnv*,FklVMvalue*,FklVMheap*);
FklAstCptr* fklCastVMvalueToCptr(FklVMvalue* value,int32_t curline);

FklVMstack* fklCopyStack(FklVMstack*);
//void fklReleaseGC(pthread_rwlock_t*);
//void fklLockGC(pthread_rwlock_t*);
FklVMvalue* fklPopVMstack(FklVMstack*);
FklVMtryBlock* fklNewVMtryBlock(FklSid_t,uint32_t tp,long int rtp);
void fklFreeVMtryBlock(FklVMtryBlock* b);

FklVMerrorHandler* fklNewVMerrorHandler(FklSid_t type,uint64_t scp,uint64_t cpc);
void fklFreeVMerrorHandler(FklVMerrorHandler*);
int fklRaiseVMerror(FklVMvalue* err,FklVM*);
FklVMrunnable* fklNewVMrunnable(FklVMproc*);
char* fklGenErrorMessage(FklErrorType type,FklVMrunnable* r,FklVM* exe);
char* fklGenInvalidSymbolErrorMessage(char* str,int _free,FklErrorType,FklVMrunnable* r,FklVM* exe);
int32_t fklGetSymbolIdInByteCode(const uint8_t*);

VMcontinuation* fklNewVMcontinuation(uint32_t ap,FklVMstack* stack,FklPtrStack* rstack,FklPtrStack* tstack);
void fklFreeVMcontinuation(VMcontinuation* cont);


FklVMenvNode* fklNewVMenvNode(FklVMvalue*,int32_t);
FklVMenvNode* fklAddVMenvNode(FklVMenvNode*,FklVMenv*);
FklVMenvNode* fklFindVMenvNode(FklSid_t,FklVMenv*);
void fklFreeVMenvNode(FklVMenvNode*);


FklVMenv* fklNewVMenv(FklVMvalue*);
void fklFreeVMenv(FklVMenv*);
//FklVMenv* fklCopyVMenv(FklVMenv*,FklVMheap*);

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

FklVMstr* fklNewVMstr(size_t,char*);

FklVMstr* fklCopyVMstr(const FklVMstr*);
FklVMstr* fklNewEmptyVMstr();
void fklVMstrCat(FklVMstr**,const FklVMstr*);
int fklVMstrcmp(const FklVMstr*,const FklVMstr*);
char* fklVMstrToCstr(FklVMstr* str);
FklVMchanl* fklNewVMchanl(int32_t size);

void fklFreeVMchanl(FklVMchanl*);
//FklVMchanl* fklCopyVMchanl(FklVMchanl*,FklVMheap*);
int32_t fklGetNumVMchanl(FklVMchanl*);

//FklVMproc* fklCopyVMproc(FklVMproc*,FklVMheap*);
void fklFreeVMproc(FklVMproc*);

FklVMfp* fklNewVMfp(FILE*);
int fklFreeVMfp(FklVMfp*);

FklVMdllHandle fklNewVMdll(const char*);
void* fklGetAddress(const char*,FklVMdllHandle);
void fklFreeVMdll(FklVMdllHandle);

FklVMdlproc* fklNewVMdlproc(FklVMdllFunc,FklVMvalue*);
void fklFreeVMdlproc(FklVMdlproc*);

FklVMerror* fklNewVMerror(const char* who,const char* type,const char* message);
FklVMerror* fklNewVMerrorWithSid(const char* who,FklSid_t type,const char* message);
void fklFreeVMerror(FklVMerror*);


FklVMrecv* fklNewVMrecv(void);
void fklFreeVMrecv(FklVMrecv*);

FklVMsend* fklNewVMsend(FklVMvalue*);
void fklFreeVMsend(FklVMsend*);

void fklChanlSend(FklVMsend*,FklVMchanl*);
void fklChanlRecv(FklVMrecv*,FklVMchanl*);

FklVMvalue* fklCastCptrVMvalue(FklAstCptr*,FklVMheap*);

//char* fklVMbytsToCstr(FklVMbyts*);
FklVMvalue* fklGetVMstdin(void);
FklVMvalue* fklGetVMstdout(void);
FklVMvalue* fklGetVMstderr(void);

FklVMvec* fklNewVMvec(size_t size,FklVMvalue** base);
void fklFreeVMvec(FklVMvec*);
void fklVMvecCat(FklVMvec**,const FklVMvec*);

void fklInitVMargs(int argc,char** argv);
int fklGetVMargc(void);
char** fklGetVMargv(void);

//FklVMvalue* fklPopAndGetVMstack(FklVMstack* stack);
FklVMvalue* fklPopGetAndMark(FklVMstack* stack,FklVMheap*);
FklVMvalue* fklPopGetAndMarkWithoutLock(FklVMstack* stack,FklVMheap* heap);
FklVMvalue* fklTopGet(FklVMstack* stack);
void fklDecTop(FklVMstack* s);
FklVMvalue* fklNewVMvalueToStack(FklValueType
		,void* p
		,FklVMstack*
		,FklVMheap* heap);
FklVMvalue* fklNewVMvalueToStackWithoutLock(FklValueType
		,void* p
		,FklVMstack*
		,FklVMheap* heap);
void fklSetAndPop(FklVMvalue* by,FklVMvalue**,FklVMstack* s,FklVMheap*);

FklVMdllHandle fklLoadDll(const char* path);

void fklPushVMvalue(FklVMvalue* v,FklVMstack* s);

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
	FklVMvalue* err=fklNiNewVMvalue(FKL_ERR,fklNewVMerror((WHO),fklGetBuiltInErrorType(ERRORTYPE),errorMessage),(EXE)->stack,(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR(WHO,STR,FREE,ERRORTYPE,RUNNABLE,EXE) do{\
	char* errorMessage=fklGenInvalidSymbolErrorMessage((STR),(FREE),(ERRORTYPE),(RUNNABLE),(EXE));\
	FklVMvalue* err=fklNiNewVMvalue(FKL_ERR,fklNewVMerror((WHO),fklGetBuiltInErrorType(ERRORTYPE),errorMessage),(EXE)->stack,(EXE)->heap);\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#ifdef __cplusplus
}
#endif

#endif
