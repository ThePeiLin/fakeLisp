#ifndef FKL_VM_H
#define FKL_VM_H

#include"base.h"
#include"bytecode.h"
//#include"ast.h"
#include"lexer.h"
//#include"compiler.h"
#include"builtin.h"
//#include<stdatomic.h>
#include<stdio.h>
#include<stdint.h>
#include<pthread.h>
#include<setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	FKL_TYPE_NIL=0,
	FKL_TYPE_I32,
	FKL_TYPE_SYM,
	FKL_TYPE_CHR,
	FKL_TYPE_F64,
	FKL_TYPE_I64,
	FKL_TYPE_BIG_INT,
	FKL_TYPE_STR,
	FKL_TYPE_VECTOR,
	FKL_TYPE_PAIR,
	FKL_TYPE_BOX,
	FKL_TYPE_BYTEVECTOR,
	FKL_TYPE_USERDATA,
	FKL_TYPE_PROC,
	FKL_TYPE_CONT,
	FKL_TYPE_CHAN,
	FKL_TYPE_FP,
	FKL_TYPE_DLL,
	FKL_TYPE_DLPROC,
	FKL_TYPE_ERR,
	FKL_TYPE_ENV,
	FKL_TYPE_HASHTABLE,
	FKL_TYPE_CODE_OBJ,
}FklValueType;


typedef enum
{
	FKL_CC_OK=0,
	FKL_CC_RE,
}FklCCState;

struct FklVM;
typedef void (*FklVMdllFunc)(struct FklVM*);
typedef struct FklVMvalue* FklVMptr;
typedef enum
{
	FKL_TAG_PTR=0,
	FKL_TAG_NIL,
	FKL_TAG_I32,
	FKL_TAG_SYM,
	FKL_TAG_CHR,
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
	size_t max;
	volatile size_t messageNum;
	volatile size_t sendNum;
	volatile size_t recvNum;
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
	pthread_mutex_t lock;
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
	FklVMvalueMark volatile mark;
	FklValueType type;
	union
	{
		struct FklVMpair* pair;
		double f64;
		int64_t i64;
		struct FklString* str;
		struct FklBytevector* bvec;
		struct FklVMproc* proc;
		FklVMdllHandle dll;
		struct FklVMdlproc* dlproc;
		struct FklVMcontinuation* cont;
		FklVMfp* fp;
		FklVMvec* vec;
		struct FklVMenv* env;
		struct FklVMhashTable* hash;
		struct FklVMchanl* chan;
		struct FklVMerror* err;
		FklBigInt* bigInt;
		FklVMudata* ud;
		struct FklVMvalue* box;
		FklByteCodelnt* code;
	}u;
	struct FklVMvalue* next;
}FklVMvalue;

typedef struct FklVMenv
{
	pthread_rwlock_t lock;
	struct FklVMvalue* volatile prev;
	FklHashTable* t;
}FklVMenv;

typedef enum
{
	FKL_VM_HASH_EQ=0,
	FKL_VM_HASH_EQV,
	FKL_VM_HASH_EQUAL,
}FklVMhashTableEqType;

typedef struct
{
	FklVMvalue* key;
	FklVMvalue* v;
}FklVMhashTableItem;

typedef struct FklVMhashTable
{
	pthread_rwlock_t lock;
	FklHashTable* ht;
	FklVMhashTableEqType type;
}FklVMhashTable;

typedef struct FklVMproc
{
	uint64_t scp;
	uint64_t cpc;
	FklSid_t sid;
	FklVMvalue* prevEnv;
	FklVMvalue* codeObj;
}FklVMproc;

typedef void (*FklVMFuncK)(struct FklVM*,FklCCState,void*);

typedef struct FklVMcCC
{
	FklVMFuncK kFunc;
	void* ctx;
	size_t size;
	struct FklVMcCC* next;
}FklVMcCC;

typedef struct FklVMframe
{
	unsigned int mark :1;
	FklVMvalue* localenv;
	FklVMvalue* codeObj;
	uint8_t* code;
	uint64_t scp;
	uint64_t cp;
	uint64_t cpc;
	FklSid_t sid;
	FklVMcCC* ccc;
	struct FklVMframe* prev;
}FklVMframe;

typedef struct
{
	FklUintStack* bps;
	FklUintStack* tps;
	uint64_t volatile tp;
	uint64_t bp;
	size_t size;
	FklVMvalue** values;
//	pthread_rwlock_t lock;
}FklVMstack;

typedef struct FklVMlib
{
	size_t exportNum;
	FklSid_t* exports;
	FklVMvalue* proc;
	FklVMvalue* libEnv;
}FklVMlib;

typedef struct FklVM
{
	uint32_t mark;
	pthread_t tid;
	pthread_rwlock_t rlock;
	FklVMframe* frames;
	FklPtrStack* tstack;
	FklVMstack* stack;
	FklVMvalue* codeObj;
	struct FklVMvalue* chan;
	struct FklVMgc* gc;
	void (*callback)(void*);
	FklVMvalue* volatile nextCall;
	FklVMvalue* volatile nextCallBackUp;
	size_t libNum;
	FklVMlib* libs;
	pthread_mutex_t prev_next_lock;
	struct FklVM* prev;
	struct FklVM* next;
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
	void (*__atomic)(void*,struct FklVMgc*);
	size_t (*__length)(void*);
	void (*__append)(void**,void*);
	void* (*__copy)(void*);
	size_t (*__hash)(void*,FklPtrStack*);
	FklString* (*__to_string)(void*);
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

typedef struct FklVMgc
{
	FklGCstate volatile running;
	pthread_rwlock_t lock;
	size_t volatile num;
	uint32_t threshold;
	FklVMvalue* head;
	pthread_rwlock_t greylock;
	struct Greylink* volatile grey;
	size_t volatile greyNum;
}FklVMgc;

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
	FklVMframe* curr;
	struct FklVMtryBlock* tb;
	FklVMvalue* nextCall;
	FklVMvalue* codeObj;
}FklVMcontinuation;

typedef struct FklVMtryBlock
{
	FklSid_t sid;
	FklPtrStack* hstack;
	FklVMframe* curFrame;
	uint32_t tp;
}FklVMtryBlock;

typedef struct FklVMerrorHandler
{
	FklSid_t* typeIds;
	uint32_t num;
	FklVMproc proc;
}FklVMerrorHandler;

//typedef struct FklVMnode
//{
//	FklVM* vm;
//	struct FklVMnode* next;
//}FklVMnode;

//vmrun

int fklRunVM(FklVM*);
//FklVMlist* fklGetGlobVMs(void);
//void fklSetGlobVMs(FklVMlist*);
FklVM* fklCreateVM(FklByteCodelnt*,FklVM* prev,FklVM* next);
//FklVM* fklCreateTmpVM(FklByteCode*,FklVMgc*,FklVM* prev,FklVM* next);
FklVM* fklCreateThreadVM(FklVMproc*,FklVMgc*,FklVM* prev,FklVM* next,size_t libNum,FklVMlib* libs);
FklVM* fklCreateThreadCallableObjVM(FklVMframe* frame,FklVMgc* gc,FklVMvalue*,FklVM* prev,FklVM* next,size_t libNum,FklVMlib* libs);

void fklDestroyVMvalue(FklVMvalue*);
FklVMstack* fklCreateVMstack(int32_t);
void fklDestroyVMstack(FklVMstack*);
void fklStackRecycle(FklVMstack*);
int fklCreateCreateThread(FklVM*);
//FklVMlist* fklCreateThreadStack(int32_t);
FklVMframe* fklHasSameProc(uint32_t,FklVMframe*);
int fklIsTheLastExpress(const FklVMframe*,const FklVMframe*,const FklVM* exe);
FklVMgc* fklCreateVMgc();
void fklCreateCallChainWithContinuation(FklVM*,FklVMcontinuation*);
void fklDestroyVMgc(FklVMgc*);
void fklDestroyAllVMs(FklVM* cur);
void fklDeleteCallChain(FklVM*);
void fklJoinAllThread(FklVM* cur);
void fklCancelAllThread();
void fklChangeGCstate(FklGCstate,FklVMgc*);
FklGCstate fklGetGCstate(FklVMgc*);
void fklGetGCstateAndGCNum(FklVMgc*,FklGCstate* s,int* num);
void* fklGC_threadFunc(void*);
void* fklGC_sweepThreadFunc(void*);
void fklGC_mark(FklVM*);
void fklGC_markValue(FklVMvalue*);
void fklGC_markValueInStack(FklVMstack*);
void fklGC_markValueInEnv(FklVMenv*);
void fklGC_markValueInCallChain(FklPtrStack*);
void fklGC_markMessage(FklQueueNode*);
void fklGC_markSendT(FklQueueNode*);
void fklGC_toGrey(FklVMvalue*,FklVMgc*);
void fklGC_step(FklVM* exe);
void fklGC_joinGCthread(FklVMgc* gc);

void fklWaitGC(FklVMgc* gc);
void fklDestroyAllValues(FklVMgc*);
void fklGC_sweep(FklVMvalue*);

void fklDBG_printVMenv(FklVMenv*,FILE*);
void fklDBG_printVMvalue(FklVMvalue*,FILE*);
void fklDBG_printVMstack(FklVMstack*,FILE*,int);

void fklPrintVMvalue(FklVMvalue* value,FILE* fp,void(*atomPrinter)(FklVMvalue* v,FILE* fp));
void fklPrin1VMvalue(FklVMvalue*,FILE*);
void fklPrincVMvalue(FklVMvalue*,FILE*);

//vmutils

FklVMvalue* fklMakeVMf64(double f,FklVM*);
FklVMvalue* fklMakeVMint(int64_t r64,FklVM*);
FklVMvalue* fklMakeVMuint(uint64_t r64,FklVM*);
FklVMvalue* fklMakeVMintD(double r64,FklVM*);
int fklIsVMnumber(const FklVMvalue* p);
int fklIsFixint(const FklVMvalue* p);
int fklIsInt(const FklVMvalue* p);
int fklIsList(const FklVMvalue* p);
int64_t fklGetInt(const FklVMvalue* p);
double fklGetDouble(const FklVMvalue* p);
//void fklInitVMRunningResource(FklVM*,FklVMvalue*,FklVMgc* gc,FklByteCodelnt*,uint32_t,uint32_t);
//void fklUninitVMRunningResource(FklVM*);

typedef struct FklPreEnv FklPreEnv;
FklHashTable* fklCreateLineNumHashTable(void);
FklVMvalue* fklGetTopValue(FklVMstack* stack);
FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place);
//FklVMvalue* fklCastPreEnvToVMenv(FklPreEnv*,FklVMvalue*,FklHashTable*,FklVMgc*);
//FklAstCptr* fklCastVMvalueToCptr(FklVMvalue* value,uint64_t curline,FklHashTable*);

FklVMstack* fklCopyStack(FklVMstack*);
FklVMvalue* fklPopVMstack(FklVMstack*);
FklVMtryBlock* fklCreateVMtryBlock(FklSid_t,uint32_t tp,FklVMframe* frame);
void fklDestroyVMtryBlock(FklVMtryBlock* b);

FklVMerrorHandler* fklCreateVMerrorHandler(FklSid_t* typeIds,uint32_t,uint64_t scp,uint64_t cpc);
void fklDestroyVMerrorHandler(FklVMerrorHandler*);
int fklRaiseVMerror(FklVMvalue* err,FklVM*);
FklVMframe* fklCreateVMframeWithCodeObj(FklVMvalue* codeObj,FklVMframe* prev,FklVMgc* gc);
FklVMframe* fklCreateVMframeWithProc(FklVMproc*,FklVMframe*);
void fklDestroyVMframe(FklVMframe*);
char* fklGenErrorMessage(FklBuiltInErrorType type,FklVM* exe);
char* fklGenInvalidSymbolErrorMessage(char* str,int _free,FklBuiltInErrorType);
int32_t fklGetSymbolIdInByteCode(const uint8_t*);

FklVMcCC* fklCreateVMcCC(FklVMFuncK kFunc,void* ctx,size_t,FklVMcCC* next);
FklVMcCC* fklCopyVMcCC(FklVMcCC*);
void fklDestroyVMcCC(FklVMcCC*);

FklVMcontinuation* fklCreateVMcontinuation(uint32_t ap,FklVM*);
void fklDestroyVMcontinuation(FklVMcontinuation* cont);

FklVMhashTable* fklCreateVMhashTable(FklVMhashTableEqType);
void fklClearVMhashTable(FklVMhashTable* ht,FklVMgc*);
void fklSetVMhashTableInReverseOrder(FklVMvalue* key,FklVMvalue* v,FklVMhashTable* ht,FklVMgc* gc);
void fklSetVMhashTable(FklVMvalue* key,FklVMvalue* v,FklVMhashTable* ht,FklVMgc* gc);
FklVMhashTableItem* fklRefVMhashTable1(FklVMvalue* key,FklVMvalue* toSet,FklVMhashTable* ht,FklVMgc*);
FklVMhashTableItem* fklRefVMhashTable(FklVMvalue* key,FklVMhashTable* ht);
FklVMvalue* fklGetVMhashTable(FklVMvalue* key,FklVMhashTable* ht,int* ok);
void fklDestroyVMhashTable(FklVMhashTable*);

FklVMenv* fklCreateGlobVMenv(FklVMvalue*,FklVMgc*);
FklVMenv* fklCreateVMenv(FklVMvalue*,FklVMgc*);
FklVMvalue* volatile* fklFindVar(FklSid_t id,FklVMenv*);
FklVMvalue* volatile* fklFindOrAddVar(FklSid_t id,FklVMenv* env);
FklVMvalue* volatile* fklFindOrAddVarWithValue(FklSid_t id,FklVMvalue*,FklVMenv* env);
void fklDestroyVMenv(FklVMenv*);

FklVMproc* fklCreateVMproc(uint64_t scp,uint64_t cpc,FklVMvalue* codeObj,FklVMgc* gc);

void fklAtomicVMhashTable(FklVMvalue* pht,FklVMgc* gc);
void fklAtomicVMenv(FklVMvalue* penv,FklVMgc*);
void fklAtomicVMuserdata(FklVMvalue*,FklVMgc*);
void fklAtomicVMpair(FklVMvalue*,FklVMgc*);
void fklAtomicVMproc(FklVMvalue*,FklVMgc*);
void fklAtomicVMvec(FklVMvalue*,FklVMgc*);
void fklAtomicVMbox(FklVMvalue*,FklVMgc*);
void fklAtomicVMdlproc(FklVMvalue*,FklVMgc*);
void fklAtomicVMcontinuation(FklVMvalue*,FklVMgc*);
void fklAtomicVMchan(FklVMvalue*,FklVMgc*);


FklVMvalue* fklCopyVMlistOrAtom(FklVMvalue*,FklVM*);
FklVMvalue* fklCopyVMvalue(FklVMvalue*,FklVM*);
FklVMvalue* fklCreateVMvalue(FklValueType,void*,FklVM*);
FklVMvalue* fklCreateVMvalueNoGC(FklValueType,void*,FklVMgc*);
FklVMvalue* fklCreateVMvalueNoGCAndToStack(FklValueType,void*,FklVMgc*,FklVMstack* s);
FklVMvalue* fklCreateSaveVMvalue(FklValueType,void*);
FklVMvalue* fklCreateVMvalueFromNastNodeAndStoreInStack(const FklNastNode* node
		,FklHashTable* lineHash
		,FklVM* vm);

void fklAddToGC(FklVMvalue*,FklVM*);
void fklAddToGCNoGC(FklVMvalue*,FklVMgc*);
void fklAddToGCBeforeGC(FklVMvalue*,FklVM*);
FklVMvalue* fklCreateTrueValue(void);
FklVMvalue* fklCreateNilValue(void);
FklVMvalue* fklGetTopValue(FklVMstack*);
FklVMvalue* fklGetValue(FklVMstack*,int32_t);
FklVMvalue* fklGetVMpairCar(FklVMvalue*);
FklVMvalue* fklGetVMpairCdr(FklVMvalue*);
int fklVMvalueEqual(const FklVMvalue*,const FklVMvalue*);
int fklVMvalueEqv(const FklVMvalue*,const FklVMvalue*);
int fklVMvalueEq(const FklVMvalue*,const FklVMvalue*);
int fklNumcmp(FklVMvalue*,FklVMvalue*);

FklVMpair* fklCreateVMpair(void);
FklVMvalue* fklCreateVMpairV(FklVMvalue* car,FklVMvalue* cdr,FklVM*);

FklVMchanl* fklCreateVMchanl(int32_t size);

void fklDestroyVMchanl(FklVMchanl*);
//int32_t fklGetNumVMchanl(FklVMchanl*);

void fklDestroyVMproc(FklVMproc*);

FklVMfp* fklCreateVMfp(FILE*);
int fklDestroyVMfp(FklVMfp*);

FklVMdllHandle fklCreateVMdll(const char*);
void fklInitVMdll(FklVMvalue* rel);
void* fklGetAddress(const char*,FklVMdllHandle);
void fklDestroyVMdll(FklVMdllHandle);

FklVMdlproc* fklCreateVMdlproc(FklVMdllFunc,FklVMvalue*);
void fklDestroyVMdlproc(FklVMdlproc*);

FklVMerror* fklCreateVMerror(const FklString* who,FklSid_t type,const FklString* message);
FklVMerror* fklCreateVMerrorCstr(const char* who,FklSid_t type,const char* message);
FklVMerror* fklCreateVMerrorMCstr(const FklString* who,FklSid_t type,const char* message);
void fklDestroyVMerror(FklVMerror*);


FklVMrecv* fklCreateVMrecv(void);
void fklDestroyVMrecv(FklVMrecv*);

FklVMsend* fklCreateVMsend(FklVMvalue*);
void fklDestroyVMsend(FklVMsend*);

void fklChanlSend(FklVMsend*,FklVMchanl*);
void fklChanlRecvOk(FklVMchanl*,FklVMvalue**,int*);
void fklChanlRecv(FklVMrecv*,FklVMchanl*);

//FklVMvalue* fklCastCptrVMvalue(FklAstCptr*,FklHashTable* lineNumberHash,FklVMgc*);

FklVMvec* fklCreateVMvecNoInit(size_t size);
FklVMvec* fklCreateVMvec(size_t size);
FklVMvalue* fklCreateVMvecV(size_t size,FklVMvalue** base,FklVM*);
FklVMvalue* fklCreateVMvecVFromStack(size_t size,FklVMvalue** base,FklVM*);
void fklDestroyVMvec(FklVMvec*);
void fklVMvecCat(FklVMvec**,const FklVMvec*);

FklVMudata* fklCreateVMudata(FklSid_t type,FklVMudMethodTable* t,void* mem,FklVMvalue* rel);
int fklIsCallableUd(FklVMvalue*);
void fklDestroyVMudata(FklVMudata*);

int fklIsCallable(FklVMvalue*);
int fklIsAppendable(FklVMvalue*);
void fklInitVMargs(int argc,char** argv);
int fklGetVMargc(void);
char** fklGetVMargv(void);

FklVMvalue* fklPopGetAndMark(FklVMstack* stack,FklVMgc*);
FklVMvalue* fklPopGetAndMarkWithoutLock(FklVMstack* stack,FklVMgc* gc);
FklVMvalue* fklTopGet(FklVMstack* stack);
void fklDecTop(FklVMstack* s);
FklVMvalue* fklCreateVMvalueToStack(FklValueType
		,void* p
		,FklVM*);
FklVMvalue* fklSetRef(FklVMvalue* volatile*
		,FklVMvalue* v
		,FklVMgc*);

int fklVMnumberLt0(const FklVMvalue*);
uint64_t fklGetUint(const FklVMvalue*);
FklVMdllHandle fklLoadDll(const char* path);

void fklPushVMvalue(FklVMvalue* v,FklVMstack* s);

int fklVMcallInDlproc(FklVMvalue*
		,size_t argNum
		,FklVMvalue*[]
		,FklVMframe*
		,FklVM*
		,FklVMFuncK
		,void*
		,size_t);

size_t fklVMlistLength(FklVMvalue*);
void fklDestroyVMframes(FklVMframe* h);

FklVMlib* fklCreateVMlib(size_t exportNum,FklSid_t* exports,FklVMvalue* codeObj);
void fklDestroyVMlib(FklVMlib* lib);
void fklInitVMlib(FklVMlib*,size_t exportNum,FklSid_t* exports,FklVMvalue* codeObj);
void fklUninitVMlib(FklVMlib*);
void fklCallNativeProcdure(FklVM* exe,FklVMproc* tmpProc,FklVMframe* frame);

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

#define FKL_RAISE_BUILTIN_ERROR(WHO,ERRORTYPE,EXE) do{\
	char* errorMessage=fklGenErrorMessage((ERRORTYPE),(EXE));\
	FklVMvalue* err=fklCreateVMvalueToStack(FKL_TYPE_ERR,fklCreateVMerrorMCstr((WHO),fklGetBuiltInErrorType(ERRORTYPE),errorMessage),(EXE));\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,ERRORTYPE,EXE) do{\
	char* errorMessage=fklGenErrorMessage((ERRORTYPE),(EXE));\
	FklVMvalue* err=fklCreateVMvalueToStack(FKL_TYPE_ERR,fklCreateVMerrorCstr((WHO),fklGetBuiltInErrorType(ERRORTYPE),errorMessage),(EXE));\
	free(errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(WHO,STR,FREE,ERRORTYPE,EXE) do{\
	char* errorMessage=fklGenInvalidSymbolErrorMessage((STR),(FREE),(ERRORTYPE));\
	FklVMvalue* err=fklCreateVMvalueToStack(FKL_TYPE_ERR,fklCreateVMerrorCstr((WHO),fklGetBuiltInErrorType(ERRORTYPE),errorMessage),(EXE));\
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
#define FKL_MAKE_VM_I32(I) ((FklVMptr)((((uintptr_t)(I))<<FKL_UNUSEDBITNUM)|FKL_TAG_I32))
#define FKL_MAKE_VM_CHR(C) ((FklVMptr)((((uintptr_t)(C))<<FKL_UNUSEDBITNUM)|FKL_TAG_CHR))
#define FKL_MAKE_VM_SYM(S) ((FklVMptr)((((uintptr_t)(S))<<FKL_UNUSEDBITNUM)|FKL_TAG_SYM))
#define FKL_MAKE_VM_PTR(P) ((FklVMptr)(((uintptr_t)(P))|FKL_TAG_PTR))
#define FKL_GET_TAG(P) ((FklVMptrTag)(((uintptr_t)(P))&FKL_TAG_MASK))
#define FKL_GET_PTR(P) ((FklVMptr)(((uintptr_t)(P))&FKL_PTR_MASK))
#define FKL_GET_I32(P) ((int32_t)((uintptr_t)(P)>>FKL_UNUSEDBITNUM))
#define FKL_GET_CHR(P) ((char)((uintptr_t)(P)>>FKL_UNUSEDBITNUM))
#define FKL_GET_SYM(P) ((FklSid_t)((uintptr_t)(P)>>FKL_UNUSEDBITNUM))
#define FKL_IS_NIL(P) ((P)==FKL_VM_NIL)
#define FKL_IS_PTR(P) (FKL_GET_TAG(P)==FKL_TAG_PTR)
#define FKL_IS_PAIR(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_PAIR)
#define FKL_IS_F64(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_F64)
#define FKL_IS_STR(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_STR)
#define FKL_IS_CHAN(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_CHAN)
#define FKL_IS_FP(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_FP)
#define FKL_IS_DLL(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_DLL)
#define FKL_IS_PROC(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_PROC)
#define FKL_IS_DLPROC(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_DLPROC)
#define FKL_IS_VECTOR(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_VECTOR)
#define FKL_IS_BYTEVECTOR(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_BYTEVECTOR)
#define FKL_IS_ERR(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_ERR)
#define FKL_IS_CONT(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_CONT)
#define FKL_IS_I32(P) (FKL_GET_TAG(P)==FKL_TAG_I32)
#define FKL_IS_CHR(P) (FKL_GET_TAG(P)==FKL_TAG_CHR)
#define FKL_IS_SYM(P) (FKL_GET_TAG(P)==FKL_TAG_SYM)
#define FKL_IS_I64(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_I64)
#define FKL_IS_USERDATA(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_USERDATA)
#define FKL_IS_BIG_INT(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_BIG_INT)
#define FKL_IS_BOX(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_BOX)
#define FKL_IS_HASHTABLE(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_HASHTABLE)
#define FKL_IS_HASHTABLE_EQ(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_HASHTABLE&&(P)->u.hash->type==FKL_VM_HASH_EQ)
#define FKL_IS_HASHTABLE_EQV(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_HASHTABLE&&(P)->u.hash->type==FKL_VM_HASH_EQV)
#define FKL_IS_HASHTABLE_EQUAL(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_HASHTABLE&&(P)->u.hash->type==FKL_VM_HASH_EQUAL)

#ifdef __cplusplus
}
#endif

#endif
