#ifndef FKL_VM_H
#define FKL_VM_H

#include"base.h"
#include"bytecode.h"
#include"nast.h"
#include"builtin.h"
#include"pattern.h"
#include"utils.h"
#include<stdio.h>
#include<stdint.h>
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

#define FKL_VMVALUE_PTR_TYPE_NUM (FKL_TYPE_CODE_OBJ+1)

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

#define FKL_PTR_TAG_NUM (FKL_TAG_CHR+1)

typedef struct FklVMdll
{
	FklDllHandle handle;
	struct FklVMvalue* pd;
}FklVMdll;

typedef struct FklVMchanl
{
	size_t max;
	volatile size_t messageNum;
	FklPtrQueue messages;
	FklPtrQueue recvq;
	FklPtrQueue sendq;
}FklVMchanl;

typedef struct
{
	FklVM* exe;
	struct FklVMvalue** slot;
}FklVMrecv;

typedef struct FklVMpair
{
	struct FklVMvalue* car;
	struct FklVMvalue* cdr;
}FklVMpair;

typedef enum
{
	FKL_VM_FP_R=1,
	FKL_VM_FP_W=2,
	FKL_VM_FP_RW=3,
}FklVMfpRW;

typedef struct FklVMfp
{
	FILE* fp;
	FklPtrQueue next;
	FklVMfpRW rw:2;
	uint32_t mutex:1;
}FklVMfp;

typedef struct FklVMvec
{
	size_t size;
	struct FklVMvalue** base;
}FklVMvec;

#define FKL_VM_UDATA_COMMON_HEADER 	FklSid_t type;\
	struct FklVMvalue* rel;\
	const struct FklVMudMethodTable* t

typedef struct FklVMudata
{
	FKL_VM_UDATA_COMMON_HEADER;
	void* data[];
}FklVMudata;

typedef enum{
	FKL_MARK_W=0,
	FKL_MARK_G,
	FKL_MARK_B,
}FklVMvalueMark;

#define FKL_VM_VALUE_COMMON_HEADER struct FklVMvalue* next;\
	FklVMvalueMark volatile mark:32;\
	FklValueType type:32\

typedef struct FklVMvalue
{
	FKL_VM_VALUE_COMMON_HEADER;
	void* data[];
}FklVMvalue;

// builtin values
typedef struct FklVMvalueF64
{
	FKL_VM_VALUE_COMMON_HEADER;
	double f64;
}FklVMvalueF64;

typedef struct FklVMvalueBigInt
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklBigInt bi;
}FklVMvalueBigInt;

typedef struct FklVMvalueStr
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklString* str;
}FklVMvalueStr;

typedef struct FklVMvalueVec
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMvec vec;
}FklVMvalueVec;

typedef struct FklVMvaluePair
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMpair pair;
}FklVMvaluePair;

typedef struct FklVMvalueBox
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMvalue* box;
}FklVMvalueBox;

typedef struct FklVMvalueBvec
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklBytevector* bvec;
}FklVMvalueBvec;

typedef struct FklVMvalueUd
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMudata ud;
}FklVMvalueUd;

typedef struct
{
	FklVMvalue* key;
	FklVMvalue* v;
}FklVMhashTableItem;

typedef struct FklVMvarRef
{
	uint32_t refc;
	uint32_t idx;
	FklVMvalue** ref;
	FklVMvalue* v;
}FklVMvarRef;

typedef struct FklVMproc
{
	uint8_t* spc;
	uint8_t* end;
	FklSid_t sid;
	uint32_t protoId;
	uint32_t lcount;
	uint32_t rcount;
	FklVMvarRef** closure;
	FklVMvalue* codeObj;
}FklVMproc;

typedef struct FklVMvalueProc
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMproc proc;
}FklVMvalueProc;

typedef struct FklVMvalueChanl
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMchanl chanl;
}FklVMvalueChanl;

typedef struct FklVMvalueFp
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMfp fp;
}FklVMvalueFp;

typedef struct FklVMvalueDll
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMdll dll;
}FklVMvalueDll;

typedef void (*FklVMFuncK)(struct FklVM*,FklCCState,void*);

typedef enum
{
	FKL_FRAME_COMPOUND=0,
	FKL_FRAME_OTHEROBJ,
}FklFrameType;

typedef struct FklVMvarRefList
{
	FklVMvarRef* ref;
	struct FklVMvarRefList* next;
}FklVMvarRefList;

typedef struct FklVMCompoundFrameVarRef
{
	FklVMvarRef** lref;
	FklVMvarRefList* lrefl;
	FklVMvalue** loc;
	FklVMvarRef** ref;
	uint32_t base;
	uint32_t lcount;
	uint32_t rcount;
}FklVMCompoundFrameVarRef;

typedef struct FklVMCompoundFrameData
{
	FklSid_t sid:61;
	unsigned int tail:1;
	unsigned int mark:2;
	FklVMvalue* proc;
	uint8_t* spc;
	uint8_t* pc;
	uint8_t* end;
	FklVMCompoundFrameVarRef lr;
}FklVMCompoundFrameData;

typedef void* FklCallObjData[10];

typedef enum
{
	FKL_DLPROC_READY=0,
	FKL_DLPROC_CCC,
	FKL_DLPROC_DONE,
}FklDlprocFrameState;

typedef struct
{
	FklVMvalue* proc;
	FklDlprocFrameState state:32;
	uint32_t btp;
	FklVMFuncK kFunc;
	size_t size;
	void* ctx;
	uint32_t rtp;
}FklDlprocFrameContext;

#define FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(TYPE) static_assert(sizeof(TYPE)<=(sizeof(FklVMCompoundFrameData)-sizeof(void*))\
		,#TYPE" is too big")

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(FklDlprocFrameContext);

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

typedef struct FklVMframe
{
	FklFrameType type;
	int (*errorCallBack)(struct FklVMframe*,FklVMvalue*,struct FklVM*);
	struct FklVMframe* prev;
	union
	{
		FklVMCompoundFrameData c;
		struct
		{
			const FklVMframeContextMethodTable* t;
			FklCallObjData data;
		};
	};
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

void fklDoUninitCompoundFrame(FklVMframe* frame,FklVM* exe);
void fklDoFinalizeCompoundFrame(FklVMframe* frame,FklVM* exe);

typedef struct FklVMlib
{
	FklVMvalue* proc;
	FklVMvalue** loc;
	uint32_t count;
	uint8_t imported;
	uint8_t belong;
}FklVMlib;

typedef enum
{
	FKL_VM_EXIT,
	FKL_VM_READY,
	FKL_VM_RUNNING,
	FKL_VM_WAITING,
	FKL_VM_SLEEPING,
}FklVMstate;

#define FKL_VM_STACK_INC_NUM (128)
#define FKL_VM_STACK_INC_SIZE (sizeof(FklVMvalue*)*128)

typedef struct FklVM
{

	uint32_t ltp;
	uint32_t llast;
	FklVMvalue** locv;
	//op stack
	
	size_t size;
	uint32_t last;
	uint32_t tp;
	uint32_t bp;
	FklVMvalue** base;

	//static stack frame,only for dlproc and callable obj;
	//如果这个栈帧不会再进行调用，那么就会直接使用这个
	FklVMframe sf;

	FklVMframe* frames;

	struct FklVMvalue* chan;
	struct FklVMgc* gc;
	uint64_t libNum;
	FklVMlib* libs;
	struct FklVM* prev;
	struct FklVM* next;
	jmp_buf buf;
	FklSymbolTable* symbolTable;
	FklSid_t* builtinErrorTypeId;
	FklFuncPrototypes* pts;
	FklVMlib* importingLib;

	int64_t alarmtime;
	FklVMstate state;
}FklVM;

//typedef struct
//{
//	FklVM* run;
//	FklVM* io;
//	FklVM* sleep;
//}FklVMscheduler;

typedef struct FklVMudMethodTable
{
	size_t size;
	void (*__princ)(const FklVMudata*,FILE*,FklSymbolTable* table);
	void (*__prin1)(const FklVMudata*,FILE*,FklSymbolTable* table);
	void (*__finalizer)(FklVMudata*);
	int  (*__equal)(const FklVMudata*,const FklVMudata*);
	void (*__call)(FklVMvalue*,FklVM*);
	int (*__cmp)(const FklVMudata*,const FklVMvalue*,int*);
	void (*__write)(const FklVMudata*,FILE*);
	void (*__atomic)(const FklVMudata*,struct FklVMgc*);
	size_t (*__length)(const FklVMudata*);
	void (*__append)(FklVMudata*,const FklVMudata*);
	void (*__copy)(const FklVMudata* src,FklVMudata* dst);
	size_t (*__hash)(const FklVMudata*);
	FklString* (*__to_string)(const FklVMudata*);
	FklBytevector* (*__to_bvec)(const FklVMudata*);
}FklVMudMetaTable;

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
	size_t volatile num;
	uint32_t threshold;
	FklVMvalue* head;
	FklVM GcCo;
}FklVMgc;

typedef struct FklVMdlproc
{
	FklVMdllFunc func;
	FklVMvalue* dll;
	FklSid_t sid;
	FklVMvalue* pd;
}FklVMdlproc;

typedef struct FklVMvalueDlproc
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMdlproc dlproc;
}FklVMvalueDlproc;

typedef struct FklVMerror
{
	FklSid_t type;
	FklString* who;
	FklString* message;
}FklVMerror;

typedef struct FklVMvalueErr
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMerror err;
}FklVMvalueErr;

typedef struct FklVMvalueHash
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklHashTable hash;
}FklVMvalueHash;

typedef struct FklVMvalueCodeObj
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklByteCodelnt bcl;
}FklVMvalueCodeObj;

typedef struct FklVMerrorHandler
{
	FklSid_t* typeIds;
	FklVMproc proc;
	uint32_t num;
}FklVMerrorHandler;

void fklPopVMframe(FklVM*);
int fklRunVM(FklVM* volatile);
FklVM* fklCreateVM(FklByteCodelnt*,FklSymbolTable*,FklFuncPrototypes*);
FklVM* fklCreateThreadVM(FklVMvalue*
		,FklVM* prev
		,FklVM* next
		,size_t libNum
		,FklVMlib* libs
		,FklSymbolTable*
		,FklSid_t* builtinErrorTypeId);

//FklVM* fklGetNextRunningVM(FklVMscheduler* sc);

void fklDestroyVMvalue(FklVMvalue*);
void fklInitVMstack(FklVM*);
void fklUninitVMstack(FklVM*);
void fklAllocMoreStack(FklVM*);
void fklShrinkStack(FklVM*);
int fklCreateCreateThread(FklVM*);
FklVMframe* fklHasSameProc(FklVMvalue* proc,FklVMframe*);
FklVMgc* fklCreateVMgc();
void fklDestroyVMgc(FklVMgc*);

void fklDestroyAllVMs(FklVM* cur);
void fklDeleteCallChain(FklVM*);

FklGCstate fklGetGCstate(FklVMgc*);
void fklTryGC(FklVM*);
void fklGC_toGrey(FklVMvalue*,FklVMgc*);

void fklDestroyAllValues(FklVMgc*);
void fklGC_sweep(FklVMvalue*);

void fklDBG_printVMvalue(FklVMvalue*,FILE*,FklSymbolTable* table);
void fklDBG_printVMstack(FklVM*,FILE*,int,FklSymbolTable* table);

FklString* fklStringify(FklVMvalue*,FklSymbolTable*);
void fklPrintVMvalue(FklVMvalue* value
		,FILE* fp
		,void(*atomPrinter)(FklVMvalue* v,FILE* fp,FklSymbolTable* table)
		,FklSymbolTable* table);
void fklPrin1VMvalue(FklVMvalue*,FILE*,FklSymbolTable* table);
void fklPrincVMvalue(FklVMvalue*,FILE*,FklSymbolTable* table);

FklVMvalue* fklProcessVMnumInc(FklVM*,FklVMvalue*);
FklVMvalue* fklProcessVMnumDec(FklVM*,FklVMvalue*);

int fklProcessVMnumAdd(FklVMvalue* cur,int64_t* pr64,double* pf64,FklBigInt* bi);
int fklProcessVMnumMul(FklVMvalue* cur,int64_t* pr64,double* pf64,FklBigInt* bi);
int fklProcessVMintMul(FklVMvalue* cur,int64_t* pr64,FklBigInt* bi);

FklVMvalue* fklProcessVMnumNeg(FklVM* exe,FklVMvalue* prev);
FklVMvalue* fklProcessVMnumRec(FklVM* exe,FklVMvalue* prev);

FklVMvalue* fklProcessVMnumMod(FklVM*,FklVMvalue* fir,FklVMvalue* sec);

FklVMvalue* fklProcessVMnumAddResult(FklVM*,int64_t r64,double rd,FklBigInt* bi);
FklVMvalue* fklProcessVMnumMulResult(FklVM*,int64_t r64,double rd,FklBigInt* bi);
FklVMvalue* fklProcessVMnumSubResult(FklVM*,FklVMvalue*,int64_t r64,double rd,FklBigInt* bi);
FklVMvalue* fklProcessVMnumDivResult(FklVM*,FklVMvalue*,int64_t r64,double rd,FklBigInt* bi);

FklVMvalue* fklProcessVMnumIdivResult(FklVM* exe,FklVMvalue* prev,int64_t r64,FklBigInt* bi);

void fklSetBp(FklVM* s);
int fklResBp(FklVM*);
// FklVMvalue* fklDlprocGetArg(uint32_t* ap,FklVM*);
// FklVMvalue** fklDlprocReturn(FklVMvalue*,uint32_t* ap,FklVM*);
// void fklDlprocSetBpWithTp(FklVM* s);
// uint32_t fklDlprocSetBp(uint32_t nbp,FklVM* s);
// int fklDlprocResBp(uint32_t* ap,FklVM*);
// void fklDlprocEnd(uint32_t* ap,FklVM*);
// void fklDlprocBegin(uint32_t* ap,FklVM*);

#define FKL_CHECK_TYPE(V,P,ERR_INFO,EXE) if(!P(V))FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_INCORRECT_TYPE_VALUE,EXE)
#define FKL_CHECK_REST_ARG(STACK,ERR_INFO,EXE) if(fklResBp((STACK)))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_TOOMANYARG,EXE)

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

void fklInitLineNumHashTable(FklHashTable* ht);
FklVMvalue* fklGetTopValue(FklVM*);

FklVMerrorHandler* fklCreateVMerrorHandler(FklSid_t* typeIds,uint32_t,uint8_t* spc,uint64_t cpc);
void fklDestroyVMerrorHandler(FklVMerrorHandler*);
int fklRaiseVMerror(FklVMvalue* err,FklVM*);
void fklPrintErrBacktrace(FklVMvalue*,FklVM*);

void fklInitMainProcRefs(FklVMproc* mainProc,FklVMvarRef** closure,uint32_t count);

void fklInitMainVMframeWithProc(FklVM*
		,FklVMframe* tmp
		,FklVMproc* code
		,FklVMframe* prev
		,FklFuncPrototypes* pts);

FklVMvalue** fklAllocSpaceForLocalVar(FklVM*,uint32_t);
FklVMvalue** fklAllocMoreSpaceForMainFrame(FklVM*,uint32_t);
void fklUpdateAllVarRef(FklVMframe*,FklVMvalue**);

void fklInitVMframeWithProc(FklVMframe* tmp,FklVMproc* code,FklVMframe* prev);

FklVMframe* fklCreateVMframeWithCodeObj(FklVMvalue* codeObj,FklVM*);
FklVMframe* fklCreateVMframeWithProcValue(FklVMvalue*,FklVMframe*);

FklVMvarRef* fklMakeVMvarRefRef(FklVMvarRef* ref);
FklVMvarRef* fklCreateVMvarRef(FklVMvalue** loc,uint32_t idx);
FklVMvarRef* fklCreateClosedVMvarRef(FklVMvalue* v);
void fklDestroyVMvarRef(FklVMvarRef*);

void fklDestroyVMframe(FklVMframe*,FklVM* exe);
FklString* fklGenErrorMessage(FklBuiltInErrorType type);
FklString* fklGenInvalidSymbolErrorMessage(char* str,int _free,FklBuiltInErrorType);

int fklIsVMhashEq(FklHashTable*);
int fklIsVMhashEqv(FklHashTable*);
int fklIsVMhashEqual(FklHashTable*);
uintptr_t fklGetVMhashTableType(FklHashTable*);
const char* fklGetVMhashTablePrefix(FklHashTable*);

void fklVMhashTableSet(FklVMvalue* key,FklVMvalue* v,FklHashTable* ht,FklVMgc* gc);
FklVMhashTableItem* fklVMhashTableRef1(FklVMvalue* key,FklVMvalue* toSet,FklHashTable* ht,FklVMgc*);
FklVMhashTableItem* fklVMhashTableRef(FklVMvalue* key,FklHashTable* ht);
FklVMhashTableItem* fklVMhashTableGetItem(FklVMvalue* key,FklHashTable* ht);
FklVMvalue* fklVMhashTableGet(FklVMvalue* key,FklHashTable* ht,int* ok);

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

// value creator

FklVMvalue* fklCreateVMvaluePair(FklVM*,FklVMvalue* car,FklVMvalue* cdr);
FklVMvalue* fklCreateVMvaluePairWithCar(FklVM*,FklVMvalue* car);
FklVMvalue* fklCreateVMvaluePairNil(FklVM*);

FklVMvalue* fklCreateVMvalueStr(FklVM*,FklString* str);

FklVMvalue* fklCreateVMvalueBvec(FklVM*,FklBytevector* bvec);

FklVMvalue* fklCreateVMvalueVec(FklVM*,size_t);
FklVMvalue* fklCreateVMvalueVecWithPtr(FklVM*,size_t,FklVMvalue* const*);

FklVMvalue* fklCreateVMvalueF64(FklVM*,double f64);

FklVMvalue* fklCreateVMvalueProc(FklVM*
		,uint8_t* spc
		,uint64_t cpc
		,FklVMvalue* codeObj
		,uint32_t pid);
FklVMvalue* fklCreateVMvalueProcWithWholeCodeObj(FklVM*,FklVMvalue* codeObj,uint32_t pid);
FklVMvalue* fklCreateVMvalueProcWithFrame(FklVM*,FklVMframe* f,size_t,uint32_t pid);

FklVMvalue* fklCreateVMvalueDll(FklVM*,const char*);

FklVMvalue* fklCreateVMvalueDlproc(FklVM*
		,FklVMdllFunc
		,FklVMvalue* dll
		,FklVMvalue* pd
		,FklSid_t sid);

FklVMvalue* fklCreateVMvalueFp(FklVM*,FILE*,FklVMfpRW);

FklVMvalue* fklCreateVMvalueHash(FklVM*,FklHashTableEqType);

FklVMvalue* fklCreateVMvalueHashEq(FklVM*);

FklVMvalue* fklCreateVMvalueHashEqv(FklVM*);

FklVMvalue* fklCreateVMvalueHashEqual(FklVM*);

FklVMvalue* fklCreateVMvalueChanl(FklVM*,uint64_t);

FklVMvalue* fklCreateVMvalueBox(FklVM*,FklVMvalue*);

FklVMvalue* fklCreateVMvalueBoxNil(FklVM*);

FklVMvalue* fklCreateVMvalueError(FklVM*,FklSid_t type,const FklString*,FklString* message);

FklVMvalue* fklCreateVMvalueErrorWithCstr(FklVM*,FklSid_t type,const char* who,FklString* message);

FklVMvalue* fklCreateVMvalueBigInt(FklVM*,const FklBigInt*);

FklVMvalue* fklCreateVMvalueBigIntWithI64(FklVM*,int64_t);

FklVMvalue* fklCreateVMvalueBigIntWithU64(FklVM*,uint64_t);

FklVMvalue* fklCreateVMvalueBigIntWithF64(FklVM*,double);

FklVMvalue* fklCreateVMvalueUdata(FklVM*
		,FklSid_t type
		,const FklVMudMetaTable* t
		,FklVMvalue* rel);

FklVMvalue* fklCreateVMvalueCodeObj(FklVM*,FklByteCodelnt* bcl);

FklVMvalue* fklCreateVMvalueFromNastNode(FklVM* vm
		,const FklNastNode* node
		,FklHashTable* lineHash);

FklNastNode* fklCreateNastNodeFromVMvalue(FklVMvalue* v
		,uint64_t curline
		,FklHashTable*
		,FklSymbolTable* symbolTable);

// value getters

#define FKL_VM_CAR(V) (((FklVMvaluePair*)(V))->pair.car)
#define FKL_VM_CDR(V) (((FklVMvaluePair*)(V))->pair.cdr)

#define FKL_VM_STR(V) (((FklVMvalueStr*)(V))->str)

#define FKL_VM_BVEC(V) (((FklVMvalueBvec*)(V))->bvec)

#define FKL_VM_VEC(V) (&(((FklVMvalueVec*)(V))->vec))

#define FKL_VM_F64(V) (((FklVMvalueF64*)(V))->f64)

#define FKL_VM_PROC(V) (&(((FklVMvalueProc*)(V))->proc))

#define FKL_VM_DLL(V) (&(((FklVMvalueDll*)(V))->dll))

#define FKL_VM_DLPROC(V) (&(((FklVMvalueDlproc*)(V))->dlproc))

#define FKL_VM_FP(V) (&(((FklVMvalueFp*)(V))->fp))

#define FKL_VM_HASH(V) (&(((FklVMvalueHash*)(V))->hash))

#define FKL_VM_CHANL(V) (&(((FklVMvalueChanl*)(V))->chanl))

#define FKL_VM_ERR(V) (&(((FklVMvalueErr*)(V))->err))

#define FKL_VM_BI(V) (&(((FklVMvalueBigInt*)(V))->bi))

#define FKL_VM_UD(V) (&(((FklVMvalueUd*)(V))->ud))

#define FKL_VM_BOX(V) (((FklVMvalueBox*)(V))->box)

#define FKL_VM_CO(V) (&(((FklVMvalueCodeObj*)(V))->bcl))

void fklAddToGC(FklVMvalue*,FklVM*);
FklVMvalue* fklCreateTrueValue(void);
FklVMvalue* fklCreateNilValue(void);

void fklDropTop(FklVM* s);
FklVMvalue* fklPopArg(FklVM*);
FklVMvalue* fklGetTopValue(FklVM*);
FklVMvalue* fklPopTopValue(FklVM*);
FklVMvalue* fklGetValue(FklVM*,uint32_t);

int fklVMvalueEqual(const FklVMvalue*,const FklVMvalue*);
int fklVMvalueEqv(const FklVMvalue*,const FklVMvalue*);
int fklVMvalueEq(const FklVMvalue*,const FklVMvalue*);
int fklVMvalueCmp(FklVMvalue*,FklVMvalue*,int*);

FklVMfpRW fklGetVMfpRwFromCstr(const char* mode);

int fklVMfpRewind(FklVMfp* vfp,FklStringBuffer*,size_t j);
int fklVMfpEof(FklVMfp*);
int fklVMfpNonBlockGetc(FklVMfp* fp);
size_t fklVMfpNonBlockGets(FklVMfp* fp,FklStringBuffer*,size_t len);
int fklVMfpNonBlockGetline(FklVMfp* fp,FklStringBuffer*);
int fklVMfpNonBlockGetdelim(FklVMfp* fp,FklStringBuffer*,char ch);

int fklUninitVMfp(FklVMfp*);

void fklLockVMfp(FklVMvalue* fpv,FklVM*);
void fklUnLockVMfp(FklVMvalue* vfp);

typedef FklVMvalue** (*FklImportDllInitFunc)(FklVM* exe,FklVMvalue* dll,uint32_t* count);

void fklInitVMdll(FklVMvalue* rel,FklVM*);

FklVMrecv* fklCreateVMrecv(FklVMvalue**,FklVM* exe);
void fklDestroyVMrecv(FklVMrecv*);

void fklSleepThread(FklVM*,uint64_t sec);
void fklSuspendThread(FklVM*);
void fklResumeThread(FklVM*);
void fklChanlSend(FklVMvalue* msg,FklVMchanl*,FklVM*);
int fklChanlRecvOk(FklVMchanl*,FklVMvalue**);
void fklChanlRecv(FklVMvalue**,FklVMchanl*,FklVM*);

void fklVMvecConcat(FklVMvec*,const FklVMvec*);

#define FKL_GET_UD_DATA(TYPE,UD) ((TYPE*)(UD)->data)
#define FKL_DECL_UD_DATA(NAME,TYPE,UD) TYPE* NAME=FKL_GET_UD_DATA(TYPE,UD)

int fklIsCallableUd(const FklVMudata*);
int fklIsCmpableUd(const FklVMudata*);
int fklIsWritableUd(const FklVMudata*);
int fklIsAbleToStringUd(const FklVMudata*);
int fklUdHasLength(const FklVMudata*);

void fklPrincVMudata(const FklVMudata*,FILE*,FklSymbolTable*);
void fklPrin1VMudata(const FklVMudata*,FILE*,FklSymbolTable*);
void fklFinalizeVMudata(FklVMudata*);
int fklEqualVMudata(const FklVMudata*,const FklVMudata*);
void fklCallVMudata(const FklVMudata*,const FklVMudata*);
int fklCmpVMudata(const FklVMudata*,const FklVMvalue*,int*);
void fklWriteVMudata(const FklVMudata*,FILE* fp);
size_t fklLengthVMudata(const FklVMudata*);
int fklAppendVMudata(FklVMudata*,const FklVMudata*);
int fklCopyVMudata(const FklVMudata*,FklVMudata* dst);
size_t fklHashvVMudata(const FklVMudata*);
FklString* fklUdToString(const FklVMudata*);
FklBytevector* fklUdToBytevector(const FklVMudata*);

int fklIsCallable(FklVMvalue*);
int fklIsAppendable(FklVMvalue*);
void fklInitVMargs(int argc,char** argv);
int fklGetVMargc(void);
char** fklGetVMargv(void);

FklVMvalue* fklSetRef(FklVMvalue* volatile*
		,FklVMvalue* v
		,FklVMgc*);

int fklVMnumberLt0(const FklVMvalue*);
uint64_t fklGetUint(const FklVMvalue*);

FklVMvalue** fklPushVMvalue(FklVM* s,FklVMvalue* v);

void fklCallFuncK(FklVMFuncK funck
		,FklVM*
		,void* ctx
		,uint32_t rtp);

void fklCallFuncK1(FklVMFuncK funck
		,FklVM*
		,void* ctx
		,uint32_t rtp
		,FklVMvalue* resultBox);

void fklCallFuncK2(FklVMFuncK funck
		,FklVM*
		,void* ctx
		,uint32_t rtp
		,FklVMvalue* a
		,FklVMvalue* b);

void fklCallFuncK3(FklVMFuncK funck
		,FklVM*
		,void* ctx
		,uint32_t rtp
		,FklVMvalue* a
		,FklVMvalue* b
		,FklVMvalue* c);

void fklContinueFuncK(FklVMframe*,FklVMFuncK,void*,size_t);

void fklCallInFuncK(FklVMvalue*
		,uint32_t argNum
		,FklVMvalue*[]
		,FklVMframe*
		,FklVM*
		,FklVMFuncK
		,void*
		,size_t);

#define FKL_GET_DLPROC_RTP(EXE) (((FklDlprocFrameContext*)((EXE)->frames->data))->rtp)
void fklFuncKReturn(FklVM* exe,uint32_t rtp,FklVMvalue* retval);

size_t fklVMlistLength(FklVMvalue*);

void fklPushVMframe(FklVMframe*,FklVM* exe);
FklVMframe* fklCreateOtherObjVMframe(const FklVMframeContextMethodTable* t,FklVMframe* prev);

void fklSwapCompoundFrame(FklVMframe*,FklVMframe*);
unsigned int fklGetCompoundFrameMark(const FklVMframe*);
unsigned int fklSetCompoundFrameMark(FklVMframe*,unsigned int);

FklVMCompoundFrameVarRef* fklGetCompoundFrameLocRef(FklVMframe* f);

FklVMvalue* fklGetCompoundFrameProc(const FklVMframe*);
FklFuncPrototype* fklGetCompoundFrameProcPrototype(const FklVMframe*,FklVM* exe);

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

void fklInitVMlib(FklVMlib*,FklVMvalue* proc);

void fklInitVMlibWithCodeObj(FklVMlib*,FklVMvalue* codeObj,FklVM* exe,uint32_t protoId);

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
	FklVMvalue* err=fklCreateVMvalueError((EXE)\
			,fklGetBuiltInErrorType(ERRORTYPE,(EXE)->builtinErrorTypeId)\
			,(WHO)\
			,errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,ERRORTYPE,EXE) do{\
	FklString* errorMessage=fklGenErrorMessage((ERRORTYPE));\
	FklVMvalue* err=fklCreateVMvalueErrorWithCstr((EXE)\
			,fklGetBuiltInErrorType(ERRORTYPE,(EXE)->builtinErrorTypeId)\
			,(WHO)\
			,errorMessage);\
	fklRaiseVMerror(err,(EXE));\
	return;\
}while(0)

#define FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(WHO,STR,FREE,ERRORTYPE,EXE) do{\
	FklString* errorMessage=fklGenInvalidSymbolErrorMessage((STR),(FREE),(ERRORTYPE));\
	FklVMvalue* err=fklCreateVMvalueErrorWithCstr((EXE)\
			,fklGetBuiltInErrorType(ERRORTYPE,(EXE)->builtinErrorTypeId)\
			,(WHO)\
			,errorMessage);\
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
#define FKL_IS_HASHTABLE_EQ(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_HASHTABLE&&fklIsVMhashEq(FKL_VM_HASH(P)))
#define FKL_IS_HASHTABLE_EQV(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_HASHTABLE&&fklIsVMhashEqv(FKL_VM_HASH(P)))
#define FKL_IS_HASHTABLE_EQUAL(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_HASHTABLE&&fklIsVMhashEqual(FKL_VM_HASH(P)))

#define FKL_DL_PROC_ARGL FklVM* exe,FklVMvalue* rel,FklVMvalue* pd

#ifdef __cplusplus
}
#endif

#endif
