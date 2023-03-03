#ifndef FKL_VM_H
#define FKL_VM_H

#include"base.h"
#include"bytecode.h"
#include"parser.h"
#include"builtin.h"
#include"pattern.h"
#include"utils.h"
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
	FKL_TYPE_F64=0,
	FKL_TYPE_BIG_INT,
	FKL_TYPE_STR,
	FKL_TYPE_VECTOR,
	FKL_TYPE_PAIR,
	FKL_TYPE_BOX,
	FKL_TYPE_BYTEVECTOR,
	FKL_TYPE_USERDATA,
	FKL_TYPE_PROC,
	FKL_TYPE_CHAN,
	FKL_TYPE_FP,
	FKL_TYPE_DLL,
	FKL_TYPE_DLPROC,
	FKL_TYPE_ERR,
	FKL_TYPE_HASHTABLE,
	FKL_TYPE_CODE_OBJ,
}FklValueType;

typedef enum
{
	FKL_CC_OK=0,
	FKL_CC_RE,
}FklCCState;

struct FklVM;
struct FklVMvalue;
typedef void (*FklVMdllFunc)(struct FklVM*,struct FklVMvalue* dll,struct FklVMvalue* pd);
typedef struct FklVMvalue* FklVMptr;
typedef enum
{
	FKL_TAG_PTR=0,
	FKL_TAG_NIL,
	FKL_TAG_FIX,
	FKL_TAG_SYM,
	FKL_TAG_CHR,
}FklVMptrTag;

typedef struct FklVMdll
{
	FklDllHandle handle;
	struct FklVMvalue* pd;
}FklVMdll;

typedef struct FklVMchanl
{
	size_t max;
	volatile size_t messageNum;
	volatile size_t sendNum;
	volatile size_t recvNum;
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
	struct FklVMvalue* pd;
	const struct FklVMudMethodTable* t;
	void* data;
}FklVMudata;

typedef enum{
	FKL_MARK_W=0,
	FKL_MARK_G,
	FKL_MARK_B,
}FklVMvalueMark;

typedef struct FklVMvalue
{
	FklVMvalueMark volatile mark:32;
	FklValueType type:32;
	union
	{
		struct FklVMpair* pair;
		double f64;
		struct FklString* str;
		struct FklBytevector* bvec;
		struct FklVMproc* proc;
		FklVMdll* dll;
		struct FklVMdlproc* dlproc;
		struct FklVMcontinuation* cont;
		FklVMfp* fp;
		FklVMvec* vec;
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

typedef struct
{
	FklVMvalue* key;
	FklVMvalue* v;
}FklVMhashTableItem;

typedef struct FklVMhashTable
{
	FklHashTable* ht;
	FklVMhashTableEqType type;
}FklVMhashTable;

typedef struct FklVMproc
{
	uint8_t* spc;
	uint8_t* end;
	FklSid_t sid;
	uint32_t protoId;
	FklVMvalue** closure;
	uint32_t count;
	FklVMvalue* codeObj;
}FklVMproc;

typedef void (*FklVMFuncK)(struct FklVM*,FklCCState,void*);

typedef enum
{
	FKL_FRAME_COMPOUND=0,
	FKL_FRAME_OTHEROBJ,
}FklFrameType;

typedef void* FklCallObjData[6];

struct FklVMgc;
typedef struct
{
	int (*end)(FklCallObjData data);
	void (*step)(FklCallObjData data,struct FklVM*);
	void (*print_backtrace)(FklCallObjData data,FILE* fp,FklSymbolTable*);
	void (*atomic)(FklCallObjData data,struct FklVMgc*);
	void (*finalizer)(FklCallObjData data);
	void (*copy)(FklCallObjData dst,const FklCallObjData src,struct FklVM*);
}FklVMframeContextMethodTable;

typedef struct FklVMCompoundFrameVarRef
{
	FklVMvalue** loc;
	FklVMvalue** ref;
	uint32_t lcount;
	uint32_t rcount;
}FklVMCompoundFrameVarRef;

typedef struct FklVMCompoundFrameData
{
	unsigned int tail:1;
	unsigned int mark:2;
	FklSid_t sid:61;
	FklVMCompoundFrameVarRef lr;
	FklVMvalue* proc;
	uint8_t* spc;
	uint8_t* pc;
	uint8_t* end;
}FklVMCompoundFrameData;

typedef struct FklVMframe
{
	FklFrameType type;
	union
	{
		FklVMCompoundFrameData c;
		struct
		{
			const FklVMframeContextMethodTable* t;
			FklCallObjData data;
		}o;
	}u;
	int (*errorCallBack)(struct FklVMframe*,FklVMvalue*,struct FklVM*,void*);
	struct FklVMframe* prev;
}FklVMframe;

void fklDoPrintBacktrace(FklVMframe* f,FILE* fp,FklSymbolTable* table);
void fklCallObj(FklVMvalue*,FklVMframe*,struct FklVM* exe);
void fklTailCallObj(FklVMvalue*,FklVMframe*,struct FklVM* exe);
void fklDoAtomicFrame(FklVMframe* f,struct FklVMgc*);
void fklDoCopyObjFrameContext(FklVMframe*,FklVMframe*,struct FklVM* exe);
void** fklGetFrameData(FklVMframe* f);
int fklIsCallableObjFrameReachEnd(FklVMframe* f);
void fklDoCallableObjFrameStep(FklVMframe* f,struct FklVM* exe);
void fklDoFinalizeObjFrame(FklVMframe* f,FklVMframe* sf);
void fklDoFinalizeCompoundFrame(FklVMframe* frame);

typedef struct
{
	FklUintStack bps;
	FklUintStack tps;
	uint64_t volatile tp;
	uint64_t bp;
	size_t size;
	FklVMvalue** values;
}FklVMstack;

typedef struct FklVMlib
{
	size_t exportNum;
	FklSid_t* exports;
	uint32_t* idxes;
	FklVMvalue* proc;
	FklVMvalue** loc;
	uint32_t count;
	uint8_t imported;
}FklVMlib;

typedef struct FklVM
{
	uint32_t mark;
	pthread_t tid;
	FklVMframe* frames;
	//static stack frame,only for dlproc and callable obj;
	//如果这个栈帧不会再进行调用，那么就会直接使用这个
	FklVMframe sf;
	FklVMstack* stack;
	FklVMvalue* codeObj;
	struct FklVMvalue* chan;
	struct FklVMgc* gc;
	size_t libNum;
	FklVMlib* libs;
	struct FklVM* prev;
	struct FklVM* next;
	jmp_buf buf;
	FklSymbolTable* symbolTable;
	FklSid_t* builtinErrorTypeId;
	FklPrototypePool* ptpool;
	uint32_t importingLibId;
}FklVM;

typedef struct FklVMudMethodTable
{
	void (*__princ)(void*,FILE*,FklSymbolTable* table,FklVMvalue* pd);
	void (*__prin1)(void*,FILE*,FklSymbolTable* table,FklVMvalue* pd);
	void (*__finalizer)(void*);
	int  (*__equal)(const FklVMudata*,const FklVMudata*);
	void (*__call)(FklVMvalue*,FklVM*);
	int (*__cmp)(FklVMvalue*,FklVMvalue*,int*);
	void (*__write)(void*,FILE*);
	void (*__atomic)(void*,struct FklVMgc*);
	size_t (*__length)(void*);
	void (*__append)(void**,void*);
	void* (*__copy)(void*);
	size_t (*__hash)(void*,FklPtrStack*);
	void (*__setq_hook)(void*,uint32_t idx,FklVMframe* f,FklVM* exe);
	FklString* (*__to_string)(void*);
	FklBytevector* (*__to_bvec)(void*);
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
	pthread_mutex_t tcMutex;
	size_t volatile num;
	uint32_t threshold;
	FklVMvalue* head;
	struct Greylink* volatile grey;
	size_t volatile greyNum;
}FklVMgc;

typedef struct FklVMdlproc
{
	FklVMdllFunc func;
	FklVMvalue* dll;
	FklSid_t sid;
	FklVMvalue* pd;
}FklVMdlproc;

typedef struct FklVMerror
{
	FklSid_t type;
	FklString* who;
	FklString* message;
}FklVMerror;

typedef struct FklVMerrorHandler
{
	FklSid_t* typeIds;
	uint32_t num;
	FklVMproc proc;
}FklVMerrorHandler;

//vmrun

int fklRunVM(FklVM*);
int fklRunReplVM(FklVM*);
FklVM* fklCreateVM(FklByteCodelnt*,FklSymbolTable*,FklVM* prev,FklVM* next);
FklVM* fklCreateThreadVM(FklVMgc* gc
		,FklVMvalue*
		,FklVM* prev
		,FklVM* next
		,size_t libNum
		,FklVMlib* libs
		,FklSymbolTable*
		,FklSid_t* builtinErrorTypeId);

void fklDestroyVMvalue(FklVMvalue*);
FklVMstack* fklCreateVMstack(uint32_t);
void fklDestroyVMstack(FklVMstack*);
void fklStackRecycle(FklVMstack*);
int fklCreateCreateThread(FklVM*);
FklVMframe* fklHasSameProc(FklVMvalue* proc,FklVMframe*);
FklVMgc* fklCreateVMgc();
void fklDestroyVMgc(FklVMgc*);

void fklTcMutexAcquire(FklVMgc*);
int fklTcMutexTryAcquire(FklVMgc*);
void fklTcMutexRelease(FklVMgc*);

void fklDestroyAllVMs(FklVM* cur);
void fklDeleteCallChain(FklVM*);
void fklJoinAllThread(FklVM* cur);

void fklChangeGCstate(FklGCstate,FklVMgc*);
FklGCstate fklGetGCstate(FklVMgc*);
void fklGetGCstateAndGCNum(FklVMgc*,FklGCstate* s,int* num);
void* fklGC_threadFunc(void*);
void* fklGC_sweepThreadFunc(void*);
void fklGC_mark(FklVM*);
void fklGC_markValue(FklVMvalue*);
void fklGC_markValueInStack(FklVMstack*);
void fklGC_markValueInCallChain(FklPtrStack*);
void fklGC_markMessage(FklQueueNode*);
void fklGC_markSendT(FklQueueNode*);
void fklGC_toGrey(FklVMvalue*,FklVMgc*);
void fklGC_step(FklVM* exe);
void fklGC_joinGCthread(FklVMgc* gc);

void fklWaitGC(FklVMgc* gc);
void fklDestroyAllValues(FklVMgc*);
void fklGC_sweep(FklVMvalue*);

void fklDBG_printVMvalue(FklVMvalue*,FILE*,FklSymbolTable* table);
void fklDBG_printVMstack(FklVMstack*,FILE*,int,FklSymbolTable* table);

FklString* fklStringify(FklVMvalue*,FklSymbolTable*);
void fklPrintVMvalue(FklVMvalue* value
		,FILE* fp
		,void(*atomPrinter)(FklVMvalue* v,FILE* fp,FklSymbolTable* table)
		,FklSymbolTable* table);
void fklPrin1VMvalue(FklVMvalue*,FILE*,FklSymbolTable* table);
void fklPrincVMvalue(FklVMvalue*,FILE*,FklSymbolTable* table);

//vmutils

FklVMvalue* fklMakeVMf64(double f,FklVM*);
FklVMvalue* fklMakeVMint(int64_t r64,FklVM*);
FklVMvalue* fklMakeVMuint(uint64_t r64,FklVM*);
FklVMvalue* fklMakeVMintD(double r64,FklVM*);
int fklIsVMnumber(const FklVMvalue* p);
int fklIsInt(const FklVMvalue* p);
int fklIsList(const FklVMvalue* p);
int fklIsSymbolList(const FklVMvalue* p);
int64_t fklGetInt(const FklVMvalue* p);
double fklGetDouble(const FklVMvalue* p);

FklHashTable* fklCreateValueSetHashtable(void);
void fklScanCirRef(FklVMvalue* s,FklHashTable* recValueSet);

typedef struct FklPreEnv FklPreEnv;
FklHashTable* fklCreateLineNumHashTable(void);
FklVMvalue* fklGetTopValue(FklVMstack* stack);
FklVMvalue* fklGetValue(FklVMstack* stack,int32_t place);

FklVMstack* fklCopyStack(FklVMstack*);
FklVMvalue* fklPopVMstack(FklVMstack*);

FklVMerrorHandler* fklCreateVMerrorHandler(FklSid_t* typeIds,uint32_t,uint8_t* spc,uint64_t cpc);
void fklDestroyVMerrorHandler(FklVMerrorHandler*);
int fklRaiseVMerror(FklVMvalue* err,FklVM*);

void fklInitMainProcRefs(FklVMproc* mainProc,FklVMvalue** closure,uint32_t count);
void fklInitMainVMframeWithProc(FklVMframe* tmp,FklVMproc* code,FklVMframe* prev);
void fklInitVMframeWithProc(FklVMframe* tmp,FklVMproc* code,FklVMframe* prev);

FklVMframe* fklCreateVMframeWithCodeObj(FklVMvalue* codeObj,FklVMframe* prev,FklVMgc* gc);
FklVMframe* fklCreateVMframeWithProcValue(FklVMvalue*,FklVMframe*);
FklVMframe* fklCreateVMframeWithProc(FklVMproc*,FklVMframe*);

void fklDestroyVMframe(FklVMframe*,FklVMframe* sf);
FklString* fklGenErrorMessage(FklBuiltInErrorType type);
FklString* fklGenInvalidSymbolErrorMessage(char* str,int _free,FklBuiltInErrorType);

FklVMhashTable* fklCreateVMhashTable(FklVMhashTableEqType);
void fklClearVMhashTable(FklVMhashTable* ht,FklVMgc*);
void fklSetVMhashTableInReverseOrder(FklVMvalue* key,FklVMvalue* v,FklVMhashTable* ht,FklVMgc* gc);
void fklSetVMhashTable(FklVMvalue* key,FklVMvalue* v,FklVMhashTable* ht,FklVMgc* gc);
FklVMhashTableItem* fklRefVMhashTable1(FklVMvalue* key,FklVMvalue* toSet,FklVMhashTable* ht,FklVMgc*);
FklVMhashTableItem* fklRefVMhashTable(FklVMvalue* key,FklVMhashTable* ht);
FklVMvalue* fklGetVMhashTable(FklVMvalue* key,FklVMhashTable* ht,int* ok);
void fklDestroyVMhashTable(FklVMhashTable*);

FklVMproc* fklCreateVMproc(uint8_t* spc,uint64_t cpc,FklVMvalue* codeObj,FklVMgc* gc);
FklVMproc* fklCreateVMprocWithWholeCodeObj(FklVMvalue* codeObj,FklVMgc* gc);

void fklAtomicVMhashTable(FklVMvalue* pht,FklVMgc* gc);
void fklAtomicVMuserdata(FklVMvalue*,FklVMgc*);
void fklAtomicVMpair(FklVMvalue*,FklVMgc*);
void fklAtomicVMproc(FklVMvalue*,FklVMgc*);
void fklAtomicVMvec(FklVMvalue*,FklVMgc*);
void fklAtomicVMbox(FklVMvalue*,FklVMgc*);
void fklAtomicVMdll(FklVMvalue*,FklVMgc*);
void fklAtomicVMdlproc(FklVMvalue*,FklVMgc*);
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
FklVMvalue* fklCreateVMvalueFromNastNodeNoGC(const FklNastNode* node,FklHashTable*,FklVMgc* gc);
FklNastNode* fklCreateNastNodeFromVMvalue(FklVMvalue* v
		,uint64_t curline
		,FklHashTable*
		,FklSymbolTable* symbolTable);

void fklAddToGC(FklVMvalue*,FklVM*);
void fklAddToGCNoGC(FklVMvalue*,FklVMgc*);
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

void fklDestroyVMproc(FklVMproc*);

FklVMfp* fklCreateVMfp(FILE*);
int fklDestroyVMfp(FklVMfp*);

typedef FklVMvalue** (*FklImportDllInitFunc)(FklVM* exe,FklVMvalue* dll,uint32_t* count);

FklVMdll* fklCreateVMdll(const char*);
void fklInitVMdll(FklVMvalue* rel,FklVM*);
void fklDestroyVMdll(FklVMdll*);

FklVMdlproc* fklCreateVMdlproc(FklVMdllFunc
		,FklVMvalue*
		,FklVMvalue*);
void fklDestroyVMdlproc(FklVMdlproc*);

FklVMerror* fklCreateVMerror(const FklString* who,FklSid_t type,FklString* message);
FklVMerror* fklCreateVMerrorCstr(const char* who,FklSid_t type,FklString* message);
void fklDestroyVMerror(FklVMerror*);


FklVMrecv* fklCreateVMrecv(void);
void fklDestroyVMrecv(FklVMrecv*);

FklVMsend* fklCreateVMsend(FklVMvalue*);
void fklDestroyVMsend(FklVMsend*);

void fklSuspendThread(pthread_cond_t* cond,FklVMgc* gc);
void fklResumeThread(pthread_cond_t* cond);
void fklChanlSend(FklVMsend*,FklVMchanl*,FklVMgc*);
void fklChanlRecvOk(FklVMchanl*,FklVMvalue**,int*);
void fklChanlRecv(FklVMrecv*,FklVMchanl*,FklVMgc*);

FklVMvalue* fklCreateVMboxNoGC(FklVMgc*,FklVMvalue*);

FklVMvec* fklCreateVMvecNoInit(size_t size);
FklVMvec* fklCreateVMvec(size_t size);
FklVMvalue* fklCreateVMvecV(size_t size,FklVMvalue** base,FklVM*);
FklVMvalue* fklCreateVMvecVFromStack(size_t size,FklVMvalue** base,FklVM*);
void fklDestroyVMvec(FklVMvec*);
void fklVMvecCat(FklVMvec**,const FklVMvec*);

FklVMudata* fklCreateVMudata(FklSid_t type,const FklVMudMethodTable* t,void* mem,FklVMvalue* rel,FklVMvalue* pd);
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

void fklPushVMvalue(FklVMvalue* v,FklVMstack* s);

void fklCallFuncK(FklVMFuncK funck,FklVM*,void* ctx);

void fklCallInDlproc(FklVMvalue*
		,size_t argNum
		,FklVMvalue*[]
		,FklVMframe*
		,FklVM*
		,FklVMFuncK
		,void*
		,size_t);

size_t fklVMlistLength(FklVMvalue*);

void fklPushVMframe(FklVMframe*,FklVM* exe);
FklVMframe* fklCreateOtherObjVMframe(const FklVMframeContextMethodTable* t,FklVMframe* prev);

void fklSwapCompoundFrame(FklVMframe*,FklVMframe*);
unsigned int fklGetCompoundFrameMark(const FklVMframe*);
unsigned int fklSetCompoundFrameMark(FklVMframe*,unsigned int);

FklVMCompoundFrameVarRef* fklGetCompoundFrameLocRef(FklVMframe* f);

FklVMvalue* fklGetCompoundFrameProc(const FklVMframe*);
FklPrototype* fklGetCompoundFrameProcPrototype(const FklVMframe*,FklVM* exe);

uint8_t* fklGetCompoundFrameCode(const FklVMframe*);

uint8_t* fklGetCompoundFrameEnd(const FklVMframe*);

uint8_t* fklGetCompoundFrameCodeAndAdd(FklVMframe*,size_t a);

uint8_t fklGetCompoundFrameOpAndInc(FklVMframe* frame);

void fklAddCompoundFrameCp(FklVMframe*,int64_t a);

uint64_t fklGetCompoundFrameCpc(const FklVMframe*);
FklSid_t fklGetCompoundFrameSid(const FklVMframe*);
int fklIsCompoundFrameReachEnd(FklVMframe*);
void fklDoCompoundFrameStep(FklVMframe* curframe,FklVM* exe);

FklVMframe* fklCreateVMframeWithCompoundFrame(const FklVMframe*,FklVMframe* prev,FklVMgc*);
FklVMframe* fklCopyVMframe(FklVMframe*,FklVMframe* prev,FklVM*);
void fklDestroyVMframes(FklVMframe* h);

void fklDestroyVMlib(FklVMlib* lib);

void fklInitVMlib(FklVMlib*
		,size_t exportNum
		,FklSid_t* exports
		,uint32_t*
		,FklVMvalue* proc);

void fklInitVMlibWithCodeObj(FklVMlib*
		,size_t exportNum
		,FklSid_t* exports
		,uint32_t* idxes
		,FklVMvalue* codeObj
		,FklVMgc* gc);

void fklUninitVMlib(FklVMlib*);

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
	FklString* errorMessage=fklGenErrorMessage((ERRORTYPE));\
	FklVMvalue* err=fklCreateVMvalueToStack(FKL_TYPE_ERR,fklCreateVMerror((WHO),fklGetBuiltInErrorType(ERRORTYPE,(EXE)->builtinErrorTypeId),errorMessage),(EXE));\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,ERRORTYPE,EXE) do{\
	FklString* errorMessage=fklGenErrorMessage((ERRORTYPE));\
	FklVMvalue* err=fklCreateVMvalueToStack(FKL_TYPE_ERR,fklCreateVMerrorCstr((WHO),fklGetBuiltInErrorType(ERRORTYPE,(EXE)->builtinErrorTypeId),errorMessage),(EXE));\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(WHO,STR,FREE,ERRORTYPE,EXE) do{\
	FklString* errorMessage=fklGenInvalidSymbolErrorMessage((STR),(FREE),(ERRORTYPE));\
	FklVMvalue* err=fklCreateVMvalueToStack(FKL_TYPE_ERR,fklCreateVMerrorCstr((WHO),fklGetBuiltInErrorType(ERRORTYPE,(EXE)->builtinErrorTypeId),errorMessage),(EXE));\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define FKL_UNUSEDBITNUM (3)
#define FKL_PTR_MASK ((intptr_t)0xFFFFFFFFFFFFFFF8)
#define FKL_TAG_MASK ((intptr_t)0x7)

#define FKL_VM_NIL ((FklVMptr)0x1)
#define FKL_VM_TRUE (FKL_MAKE_VM_FIX(1))
#define FKL_VM_EOF ((FklVMptr)0x7fffffffa)
#define FKL_MAKE_VM_FIX(I) ((FklVMptr)((((uintptr_t)(I))<<FKL_UNUSEDBITNUM)|FKL_TAG_FIX))
#define FKL_MAKE_VM_CHR(C) ((FklVMptr)((((uintptr_t)(C))<<FKL_UNUSEDBITNUM)|FKL_TAG_CHR))
#define FKL_MAKE_VM_SYM(S) ((FklVMptr)((((uintptr_t)(S))<<FKL_UNUSEDBITNUM)|FKL_TAG_SYM))
#define FKL_MAKE_VM_PTR(P) ((FklVMptr)(((uintptr_t)(P))|FKL_TAG_PTR))
#define FKL_GET_TAG(P) ((FklVMptrTag)(((uintptr_t)(P))&FKL_TAG_MASK))
#define FKL_GET_PTR(P) ((FklVMptr)(((uintptr_t)(P))&FKL_PTR_MASK))
#define FKL_GET_FIX(P) ((int64_t)((intptr_t)(P)>>FKL_UNUSEDBITNUM))
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
#define FKL_IS_FIX(P) (FKL_GET_TAG(P)==FKL_TAG_FIX)
#define FKL_IS_CHR(P) (FKL_GET_TAG(P)==FKL_TAG_CHR)
#define FKL_IS_SYM(P) (FKL_GET_TAG(P)==FKL_TAG_SYM)
#define FKL_IS_USERDATA(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_USERDATA)
#define FKL_IS_BIG_INT(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_BIG_INT)
#define FKL_IS_BOX(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_BOX)
#define FKL_IS_HASHTABLE(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_HASHTABLE)
#define FKL_IS_HASHTABLE_EQ(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_HASHTABLE&&(P)->u.hash->type==FKL_VM_HASH_EQ)
#define FKL_IS_HASHTABLE_EQV(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_HASHTABLE&&(P)->u.hash->type==FKL_VM_HASH_EQV)
#define FKL_IS_HASHTABLE_EQUAL(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_HASHTABLE&&(P)->u.hash->type==FKL_VM_HASH_EQUAL)

#define FKL_DL_PROC_ARGL FklVM* exe,FklVMvalue* rel,FklVMvalue* pd

#ifdef __cplusplus
}
#endif

#endif
