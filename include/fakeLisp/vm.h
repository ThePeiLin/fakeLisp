#ifndef FKL_VM_H
#define FKL_VM_H

#include"basicADT.h"
#include"bytecode.h"
#include"compiler.h"
#include<stdio.h>
#include<stdint.h>
#include<pthread.h>
#include<ffi.h>
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
}FklVMptrTag;

typedef enum
{
	FKL_MEM_RAW=0,
	FKL_MEM_ATOMIC,
}FklVMmemMode;

#ifdef _WIN32
#include<windows.h>
typedef HMODULE FklVMdllHandle;
#else
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

typedef struct FklVMbyts
{
	uint64_t size;
	uint8_t str[];
}FklVMbyts;

typedef struct FklVMfp
{
	size_t size;
	uint8_t* prev;
	FILE* fp;
}FklVMfp;

typedef struct FklVMmem
{
	FklTypeId_t type;
	FklVMmemMode mode;
	uint8_t* mem;
}FklVMmem;

typedef struct FklVMvec
{
	size_t size;
	struct FklVMvalue* base[];
}FklVMvec;

typedef struct FklVMvalue
{
	unsigned int mark :1;
	unsigned int type :6;
	union
	{
		struct FklVMpair* pair;
		double f64;
		int64_t i64;
		char* str;
		struct FklVMmem* chf;
		struct FklVMbyts* byts;
		struct FklVMproc* proc;
		FklVMdllHandle dll;
		struct FklVMdlproc* dlproc;
		struct FklVMflproc* flproc;
		struct FklVMcontinuation* cont;
		FklVMfp* fp;
		FklVMvec* vec;
		struct FklVMchanl* chan;
		struct FklVMerror* err;
	}u;
	struct FklVMvalue* prev;
	struct FklVMvalue* next;
}FklVMvalue;

typedef struct FklVMenvNode
{
	uint32_t id;
	FklVMvalue* value;
}FklVMenvNode;

typedef struct FklVMenv
{
	pthread_mutex_t mutex;
	struct FklVMenv* prev;
	uint32_t refcount;
	uint32_t num;
	FklVMenvNode** list;
}FklVMenv;

typedef struct FklVMproc
{
	uint64_t scp;
	uint64_t cpc;
	FklSid_t sid;
	FklVMenv* prevEnv;
}FklVMproc;

typedef struct FklVMrunnable
{
	unsigned int mark :1;
	FklVMenv* localenv;
	uint64_t scp;
	uint64_t cp;
	uint64_t cpc;
	FklSid_t sid;
}FklVMrunnable;

typedef struct
{
	uint32_t tp;
	uint32_t bp;
	uint32_t size;
	FklVMvalue** values;
	uint32_t tpsi;
	uint32_t tptp;
	uint32_t* tpst;
}FklVMstack;

typedef struct FklVM
{
	unsigned int mark :1;
	int32_t VMid;
	pthread_t tid;
	uint8_t* code;
	uint64_t size;
	FklPtrStack* rstack;
	FklPtrStack* tstack;
	FklVMstack* stack;
	struct FklVMvalue* chan;
	struct FklVMheap* heap;
	struct FklLineNumberTable* lnt;
	void (*callback)(void*);
	jmp_buf buf;
}FklVM;

typedef struct FklVMheap
{
	pthread_mutex_t lock;
	uint32_t num;
	uint32_t threshold;
	FklVMvalue* head;
}FklVMheap;

typedef struct
{
	uint32_t num;
	FklVM** VMs;
}FklVMlist;

typedef void (*FklVMdllFunc)(FklVM*,pthread_rwlock_t*);

typedef struct FklVMdlproc
{
	FklVMdllFunc func;
	FklVMvalue* dll;
	FklSid_t sid;
}FklVMdlproc;

typedef struct FklVMflproc
{
	void* func;
	ffi_cif cif;
	ffi_type** atypes;
	FklTypeId_t type;
	FklSid_t sid;
}FklVMflproc;

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

typedef struct FklVMptrType
{
	FklTypeId_t ptype;
}FklVMptrType;

typedef struct FklSharedObjNode
{
	FklVMdllHandle dll;
	struct FklSharedObjNode* next;
}FklSharedObjNode;

//vmrun

int fklRunVM(FklVM*);
FklVM* fklNewVM(FklByteCode*);
FklVM* fklNewTmpVM(FklByteCode*);
FklVM* fklNewThreadVM(FklVMproc*,FklVMheap*);
FklVM* fklNewThreadDlprocVM(FklVMrunnable* r,FklVMheap* heap);
void fklInitGlobEnv(FklVMenv*,FklVMheap*);

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
void fklGC_mark(FklVM*);
void fklGC_markValue(FklVMvalue*);
void fklGC_markValueInStack(FklVMstack*);
void fklGC_markValueInEnv(FklVMenv*);
void fklGC_markValueInCallChain(FklPtrStack*);
void fklGC_markMessage(FklQueueNode*);
void fklGC_markSendT(FklQueueNode*);
void fklGC_sweep(FklVMheap*);
void fklGC_compact(FklVMheap*);

void fklDBG_printVMenv(FklVMenv*,FILE*);
void fklDBG_printVMvalue(FklVMvalue*,FILE*);
void fklDBG_printVMstack(FklVMstack*,FILE*,int);
void fklFreeAllSharedObj(void);

FklVMvalue* fklGET_VAL(FklVMvalue* P,FklVMheap*);
int fklSET_REF(FklVMvalue* P,FklVMvalue* V);
void fklPrintVMvalue(FklVMvalue* value,FILE* fp,void(*atomPrinter)(FklVMvalue* v,FILE* fp));
void fklPrin1VMvalue(FklVMvalue*,FILE*);
void fklPrincVMvalue(FklVMvalue*,FILE*);

//

//vmtype

void fklApplyFF(void* func,int argc,ffi_type* rtype,ffi_type** atypes,void* rvalue,void** avalue);

ffi_type* fklGetFfiType(FklTypeId_t typeId);
void fklPrepFFIcif(ffi_cif* cif,int argc,ffi_type** atypes,ffi_type* rtype);
void fklApplyFlproc(FklVMflproc* f,void* rvalue,void** avalue);
int fklCastValueToVptr(FklTypeId_t,FklVMvalue* v,void** p);

FklVMmem* fklNewVMmem(FklTypeId_t typeId,FklVMmemMode mode,uint8_t* mem);
void fklPrintMemoryRef(FklTypeId_t,FklVMmem*,FILE*);
FklVMvalue* fklMemoryCast(FklTypeId_t,FklVMmem*,FklVMheap*);
int fklMemorySet(FklTypeId_t id,FklVMmem*,FklVMvalue* v);


//


//vmutils

void fklInitVMRunningResource(FklVM*,FklVMenv*,FklVMheap* heap,FklByteCodelnt*,uint32_t,uint32_t);
void fklUninitVMRunningResource(FklVM*);

FklVMvalue* fklGetTopValue(FklVMstack* stack);
FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place);
FklVMenv* fklCastPreEnvToVMenv(FklPreEnv*,FklVMenv*,FklVMheap*);
FklAstCptr* fklCastVMvalueToCptr(FklVMvalue* value,int32_t curline);

FklVMstack* fklCopyStack(FklVMstack*);
void fklReleaseGC(pthread_rwlock_t*);
void fklLockGC(pthread_rwlock_t*);
FklVMvalue* fklPopVMstack(FklVMstack*);
FklVMtryBlock* fklNewVMtryBlock(FklSid_t,uint32_t tp,long int rtp);
void fklFreeVMtryBlock(FklVMtryBlock* b);

FklVMerrorHandler* fklNewVMerrorHandler(FklSid_t type,uint64_t scp,uint64_t cpc);
void fklFreeVMerrorHandler(FklVMerrorHandler*);
int fklRaiseVMerror(FklVMvalue* err,FklVM*);
FklVMrunnable* fklNewVMrunnable(FklVMproc*);
char* fklGenErrorMessage(unsigned int type,FklVMrunnable* r,FklVM* exe);
char* fklGenInvalidSymbolErrorMessage(const char* str,FklVMrunnable* r,FklVM* exe);
int32_t fklGetSymbolIdInByteCode(const uint8_t*);
int fklResBp(FklVMstack*);

VMcontinuation* fklNewVMcontinuation(FklVMstack* stack,FklPtrStack* rstack,FklPtrStack* tstack);
void fklFreeVMcontinuation(VMcontinuation* cont);


FklVMenvNode* fklNewVMenvNode(FklVMvalue*,int32_t);
FklVMenvNode* fklAddVMenvNode(FklVMenvNode*,FklVMenv*);
FklVMenvNode* fklFindVMenvNode(FklSid_t,FklVMenv*);
void fklFreeVMenvNode(FklVMenvNode*);


FklVMenv* fklNewVMenv(FklVMenv*);
void fklIncreaseVMenvRefcount(FklVMenv*);
void fklDecreaseVMenvRefcount(FklVMenv*);
int fklFreeVMenv(FklVMenv*);
FklVMenv* fklCopyVMenv(FklVMenv*,FklVMheap*);

FklVMproc* fklNewVMproc(uint64_t scp,uint64_t cpc);

FklVMvalue* fklCopyVMvalue(FklVMvalue*,FklVMheap*);
FklVMvalue* fklNewVMvalue(FklValueType,void*,FklVMheap*);
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

FklVMbyts* fklNewVMbyts(size_t,uint8_t*);
//void fklIncreaseVMbyts(FklVMbyts*);
//void fklDecreaseVMbyts(FklVMbyts*);

FklVMbyts* fklCopyVMbyts(const FklVMbyts*);
FklVMbyts* fklNewEmptyVMbyts();
void fklVMbytsCat(FklVMbyts**,const FklVMbyts*);
int fklEqVMbyts(const FklVMbyts*,const FklVMbyts*);
FklVMchanl* fklNewVMchanl(int32_t size);

void fklFreeVMchanl(FklVMchanl*);
FklVMchanl* fklCopyVMchanl(FklVMchanl*,FklVMheap*);
int32_t fklGetNumVMchanl(FklVMchanl*);

FklVMproc* fklCopyVMproc(FklVMproc*,FklVMheap*);
void fklFreeVMproc(FklVMproc*);

VMcontinuation* fklNewVMcontinuation(FklVMstack*,FklPtrStack*,FklPtrStack*);
void fklFreeVMcontinuation(VMcontinuation*);

FklVMfp* fklNewVMfp(FILE*);
int fklFreeVMfp(FklVMfp*);

FklVMdllHandle* fklNewVMdll(const char*);
void* fklGetAddress(const char*,FklVMdllHandle);
void fklFreeVMdll(FklVMdllHandle*);

FklVMdlproc* fklNewVMdlproc(FklVMdllFunc,FklVMvalue*);
void fklFreeVMdlproc(FklVMdlproc*);

FklVMflproc* fklNewVMflproc(FklTypeId_t type,void* func);
void fklFreeVMflproc(FklVMflproc*);

FklVMerror* fklNewVMerror(const char* who,const char* type,const char* message);
FklVMerror* fklNewVMerrorWithSid(const char* who,FklSid_t type,const char* message);
void fklFreeVMerror(FklVMerror*);


FklVMrecv* fklNewVMrecv(void);
void fklFreeVMrecv(FklVMrecv*);

FklVMsend* fklNewVMsend(FklVMvalue*);
void fklFreeVMsend(FklVMsend*);

void fklChanlSend(FklVMsend*,FklVMchanl*,pthread_rwlock_t* pGClock);
void fklChanlRecv(FklVMrecv*,FklVMchanl*,pthread_rwlock_t* pGClock);

FklVMvalue* fklCastCptrVMvalue(FklAstCptr*,FklVMheap*);

char* fklVMbytsToCstr(FklVMbyts*);
FklVMvalue* fklGetVMstdin(void);
FklVMvalue* fklGetVMstdout(void);
FklVMvalue* fklGetVMstderr(void);

FklVMvec* fklNewVMvec(size_t size,FklVMvalue** base);
void fklFreeVMvec(FklVMvec*);
void fklVMvecCat(FklVMvec**,const FklVMvec*);

void fklInitVMargs(int argc,char** argv);
int fklGetVMargc(void);
char** fklGetVMargv(void);
#ifdef __cplusplus
}
#endif

#endif
