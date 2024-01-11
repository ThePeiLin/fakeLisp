#ifndef FKL_VM_H
#define FKL_VM_H

#include"base.h"
#include"nast.h"
#include"utils.h"
#include"bytecode.h"
#include<uv.h>
#include<stdatomic.h>
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
	FKL_TYPE_CPROC,
	FKL_TYPE_ERR,
	FKL_TYPE_HASHTABLE,
	FKL_TYPE_CODE_OBJ,
	FKL_TYPE_VAR_REF,
	FKL_VM_VALUE_GC_TYPE_NUM,
}FklValueType;

struct FklVM;
struct FklVMvalue;
struct FklCprocFrameContext;
#define FKL_CPROC_ARGL struct FklVM* exe,struct FklCprocFrameContext* ctx

typedef int (*FklVMcFunc)(FKL_CPROC_ARGL);
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

typedef struct
{
	uv_lib_t dll;
	struct FklVMvalue* pd;
}FklVMdll;

typedef struct
{
	uv_mutex_t lock;
	size_t max;
	volatile size_t msg_num;
	FklPtrQueue messages;
	FklQueueNode* msg_cache;

	struct FklVMchanlRecvq
	{
		struct FklVMchanlRecv* head;
		struct FklVMchanlRecv** tail;
	}recvq;

	struct FklVMchanlSendq
	{
		struct FklVMchanlSend* head;
		struct FklVMchanlSend** tail;
	}sendq;
}FklVMchanl;

typedef struct FklVMchanlSend
{
	struct FklVMchanlSend* next;
	uv_cond_t cond;
	struct FklVMvalue* msg;
}FklVMchanlSend;

typedef struct FklVMchanlRecv
{
	struct FklVMchanlRecv* next;
	uv_cond_t cond;
	struct FklVMvalue** slot;
}FklVMchanlRecv;

typedef struct
{
	struct FklVMvalue* car;
	struct FklVMvalue* cdr;
}FklVMpair;

#define FKL_VM_FP_R_MASK (1)
#define FKL_VM_FP_W_MASK (2)

typedef enum
{
	FKL_VM_FP_R=1,
	FKL_VM_FP_W=2,
	FKL_VM_FP_RW=3,
}FklVMfpRW;

typedef struct
{
	FILE* fp;
	FklVMfpRW rw;
}FklVMfp;

typedef struct
{
	size_t size;
	struct FklVMvalue** base;
}FklVMvec;

#define FKL_VM_UDATA_COMMON_HEADER struct FklVMvalue* rel;\
	const struct FklVMudMetaTable* t

typedef struct
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
typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	double f64;
}FklVMvalueF64;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklBigInt bi;
}FklVMvalueBigInt;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklString* str;
}FklVMvalueStr;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMvec vec;
}FklVMvalueVec;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMpair pair;
}FklVMvaluePair;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMvalue* box;
}FklVMvalueBox;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklBytevector* bvec;
}FklVMvalueBvec;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMudata ud;
}FklVMvalueUd;

typedef struct
{
	FklVMvalue* key;
	FklVMvalue* v;
}FklVMhashTableItem;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	uint32_t idx;
	_Atomic(FklVMvalue**) ref;
	FklVMvalue* v;
}FklVMvalueVarRef;

typedef struct FklVMproc
{
	FklInstruction* spc;
	FklInstruction* end;
	FklSid_t sid;
	uint32_t protoId;
	uint32_t lcount;
	uint32_t rcount;
	FklVMvalue** closure;
	FklVMvalue* codeObj;
}FklVMproc;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMproc proc;
}FklVMvalueProc;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMchanl chanl;
}FklVMvalueChanl;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMfp fp;
}FklVMvalueFp;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMdll dll;
}FklVMvalueDll;

typedef enum
{
	FKL_FRAME_COMPOUND=0,
	FKL_FRAME_OTHEROBJ,
}FklFrameType;

typedef struct FklVMvarRefList
{
	FklVMvalue* ref;
	struct FklVMvarRefList* next;
}FklVMvarRefList;

typedef struct
{
	FklVMvalue** lref;
	FklVMvarRefList* lrefl;
	FklVMvalue** loc;
	FklVMvalue** ref;
	uint32_t base;
	uint32_t lcount;
	uint32_t rcount;
}FklVMCompoundFrameVarRef;

typedef struct
{
	FklSid_t sid:61;
	unsigned int tail:1;
	unsigned int mark:2;
	FklVMvalue* proc;
	FklVMCompoundFrameVarRef lr;
	FklInstruction* spc;
	FklInstruction* pc;
	FklInstruction* end;
}FklVMCompoundFrameData;

typedef enum
{
	FKL_CPROC_READY=0,
	FKL_CPROC_DONE,
}FklCprocFrameState;

typedef struct FklCprocFrameContext
{
	FklVMvalue* proc;
	FklCprocFrameState state;
	uint32_t rtp;
	uintptr_t context;
	FklVMcFunc func;
	FklVMvalue* pd;
}FklCprocFrameContext;

#define FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(TYPE) static_assert(sizeof(TYPE)<=(sizeof(FklVMCompoundFrameData)-sizeof(void*))\
		,#TYPE" is too big")

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(FklCprocFrameContext);

struct FklVMgc;
typedef struct
{
	int (*end)(void* data);
	void (*step)(void* data,struct FklVM*);
	void (*print_backtrace)(void* data,FILE* fp,struct FklVMgc*);
	void (*atomic)(void* data,struct FklVMgc*);
	void (*finalizer)(void* data);
	void (*copy)(void* dst,const void* src,struct FklVM*);
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
			uint8_t data[1];
		};
	};
}FklVMframe;

void fklDoPrintBacktrace(FklVMframe* f,FILE* fp,struct FklVMgc* table);
void fklCallObj(struct FklVM* exe,FklVMvalue*);
void fklTailCallObj(struct FklVM* exe,FklVMvalue*);
void fklDoAtomicFrame(FklVMframe* f,struct FklVMgc*);
void fklDoCopyObjFrameContext(FklVMframe*,FklVMframe*,struct FklVM* exe);
void* fklGetFrameData(FklVMframe* f);
int fklIsCallableObjFrameReachEnd(FklVMframe* f);
void fklDoCallableObjFrameStep(FklVMframe* f,struct FklVM* exe);
void fklDoFinalizeObjFrame(struct FklVM*,FklVMframe* f);
void fklDoFinalizeCompoundFrame(struct FklVM* exe,FklVMframe* frame);

typedef struct FklVMlib
{
	FklVMvalue* proc;
	FklVMvalue** loc;
	uint32_t count;
	uint8_t imported;
	uint8_t belong;
}FklVMlib;

#define FKL_VM_ERR_RAISE (1)

typedef enum
{
	FKL_VM_EXIT,
	FKL_VM_READY,
	FKL_VM_RUNNING,
	FKL_VM_WAITING,
}FklVMstate;

#define FKL_VM_LOCV_INC_NUM (32)
#define FKL_VM_STACK_INC_NUM (128)
#define FKL_VM_STACK_INC_SIZE (sizeof(FklVMvalue*)*128)

typedef struct
{
	uv_mutex_t pre_running_lock;
	FklPtrQueue pre_running_q;
	atomic_size_t running_count;
	FklPtrQueue running_q;
}FklVMqueue;

typedef struct FklVMlocvList
{
	struct FklVMlocvList* next;
	uint32_t llast;
	FklVMvalue** locv;
}FklVMlocvList;

#define FKL_VM_INS_FUNC_ARGL struct FklVM* exe,FklInstruction* ins
typedef void (*FklVMinsFunc)(FKL_VM_INS_FUNC_ARGL);

#define FKL_VM_GC_LOCV_CACHE_NUM (8)
#define FKL_VM_GC_LOCV_CACHE_LAST_IDX (FKL_VM_GC_LOCV_CACHE_NUM-1)

typedef struct FklVM
{
	uv_thread_t tid;
	uv_mutex_t lock;
	FklVMvalue* obj_head;
	FklVMvalue* obj_tail;

	int16_t trapping;
	int16_t is_single_thread;
	uint32_t ltp;
	uint32_t llast;
	uint32_t old_locv_count;
	FklVMvalue** locv;
	FklVMlocvList old_locv_cache[FKL_VM_GC_LOCV_CACHE_NUM];
	FklVMlocvList* old_locv_list;
	//op stack
	
	size_t size;
	uint32_t last;
	uint32_t tp;
	uint32_t bp;
	FklVMvalue** base;

	FklVMframe static_frame;
	FklVMframe* top_frame;

	FklVMframe* frame_cache_head;
	FklVMframe** frame_cache_tail;

	struct FklVMvalue* chan;
	struct FklVMgc* gc;
	uint64_t libNum;
	FklVMlib* libs;
	struct FklVM* prev;
	struct FklVM* next;
	jmp_buf buf;

	FklFuncPrototypes* pts;
	FklVMlib* importingLib;

	FklVMstate volatile state;

	atomic_int notice_lock;
	_Atomic(FklVMinsFunc) ins_table[FKL_OP_LAST_OPCODE];

	uint64_t rand_state[4];
}FklVM;

typedef struct FklVMudMetaTable
{
	size_t size;
	void (*__princ)(const FklVMudata*,FILE*,struct FklVMgc* table);
	void (*__prin1)(const FklVMudata*,FILE*,struct FklVMgc* table);
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

	FklBytevector* (*__to_bvec)(const FklVMudata*);

	void (*__as_princ)(const FklVMudata*,FklStringBuffer* buf,struct FklVMgc* gc);
	void (*__as_prin1)(const FklVMudata*,FklStringBuffer* buf,struct FklVMgc* gc);
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

#define FKL_VM_GC_LOCV_CACHE_LEVEL_NUM (5)
#define FKL_VM_GC_THRESHOLD_SIZE (2048)

typedef enum
{
	FKL_ERR_DUMMY=0,
	FKL_ERR_SYMUNDEFINE,
	FKL_ERR_SYNTAXERROR,
	FKL_ERR_INVALIDEXPR,
	FKL_ERR_CIRCULARLOAD,
	FKL_ERR_INVALIDPATTERN,
	FKL_ERR_INCORRECT_TYPE_VALUE,
	FKL_ERR_STACKERROR,
	FKL_ERR_TOOMANYARG,
	FKL_ERR_TOOFEWARG,
	FKL_ERR_CANTCREATETHREAD,
	FKL_ERR_THREADERROR,
	FKL_ERR_MACROEXPANDFAILED,
	FKL_ERR_CALL_ERROR,
	FKL_ERR_LOADDLLFAILD,
	FKL_ERR_INVALIDSYMBOL,
	FKL_ERR_LIBUNDEFINED,
	FKL_ERR_UNEXPECTED_EOF,
	FKL_ERR_DIVZEROERROR,
	FKL_ERR_FILEFAILURE,
	FKL_ERR_INVALID_VALUE,
	FKL_ERR_INVALIDASSIGN,
	FKL_ERR_INVALIDACCESS,
	FKL_ERR_IMPORTFAILED,
	FKL_ERR_INVALID_MACRO_PATTERN,
	FKL_ERR_FAILD_TO_CREATE_BIG_INT_FROM_MEM,
	FKL_ERR_LIST_DIFFER_IN_LENGTH,
	FKL_ERR_CROSS_C_CALL_CONTINUATION,
	FKL_ERR_INVALIDRADIX_FOR_INTEGER,
	FKL_ERR_NO_VALUE_FOR_KEY,
	FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,
	FKL_ERR_CIR_REF,
	FKL_ERR_UNSUPPORTED_OP,
	FKL_ERR_IMPORT_MISSING,
	FKL_ERR_EXPORT_OUTER_REF_PROD_GROUP,
	FKL_ERR_IMPORT_READER_MACRO_ERROR,
	FKL_ERR_ANALYSIS_TABLE_GENERATE_FAILED,
	FKL_ERR_REGEX_COMPILE_FAILED,
	FKL_ERR_GRAMMER_CREATE_FAILED,
	FKL_ERR_INVALIDRADIX_FOR_FLOAT,
	FKL_BUILTIN_ERR_NUM,
}FklBuiltinErrorType;

typedef int (*FklVMinterruptHandler)(struct FklVMgc*,FklVM* exe,FklVMvalue* ev,void*);

typedef void (*FklVMextraMarkFunc)(struct FklVMgc*,void* arg);

typedef struct FklVMgc
{
	FklGCstate volatile running;
	atomic_size_t num;
	size_t threshold;
	FklVMvalue* head;

	int argc;
	char** argv;

	struct FklVMgcGrayList
	{
		struct FklVMgcGrayList* next;
		FklVMvalue* v;
	}* gray_list;

	struct FklVMgcGrayList* gray_list_cache;

	struct FklVMgcGrayList* unused_gray_list_cache;

	FklVMqueue q;

	FklVM* main_thread;
	int exit_code;

	struct FklLocvCacheLevel
	{
		uv_mutex_t lock;
		uint32_t num;
		struct FklLocvCache
		{
			uint32_t llast;
			FklVMvalue** locv;
		}locv[FKL_VM_GC_LOCV_CACHE_NUM];
	}locv_cache[FKL_VM_GC_LOCV_CACHE_LEVEL_NUM];

	uv_rwlock_t st_lock;
	FklSymbolTable* st;

	FklSid_t builtinErrorTypeId[FKL_BUILTIN_ERR_NUM];

	uv_mutex_t workq_lock;
	struct FklVMworkq
	{
		struct FklVMidleWork
		{
			struct FklVMidleWork* next;
			uv_cond_t cond;
			FklVM* vm;
			void* arg;
			void (*cb)(FklVM*,void*);
		}* head;
		struct FklVMidleWork** tail;
	}workq;

	atomic_uint work_num;

	struct FklVMinterruptHandleList
	{
		struct FklVMinterruptHandleList* next;
		FklVMinterruptHandler int_handler;
		void* int_handle_arg;
	}* int_list;

	struct FklVMextraMarkObjList
	{
		struct FklVMextraMarkObjList* next;
		FklVMextraMarkFunc func;
		void* arg;
	}* extra_mark_list;
}FklVMgc;

typedef struct
{
	FklVMcFunc func;
	FklVMvalue* dll;
	FklSid_t sid;
	FklVMvalue* pd;
}FklVMcproc;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMcproc cproc;
}FklVMvalueCproc;

typedef struct
{
	FklSid_t type;
	FklString* where;
	FklString* message;
}FklVMerror;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklVMerror err;
}FklVMvalueErr;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklHashTable hash;
}FklVMvalueHash;

typedef struct
{
	FKL_VM_VALUE_COMMON_HEADER;
	FklByteCodelnt bcl;
}FklVMvalueCodeObj;

typedef struct
{
	FklSid_t* typeIds;
	FklVMproc proc;
	uint32_t num;
}FklVMerrorHandler;

void fklPopVMframe(FklVM*);
int fklRunVM(FklVM* volatile);
void fklVMidleLoop(FklVMgc* gc);
void fklVMtrappingIdleLoop(FklVMgc* gc);

void fklVMinterrupt(FklVM*,FklVMvalue*);
void fklVMpushInterruptHandler(FklVMgc*,FklVMinterruptHandler,void*);

void fklVMgcExtraMark(FklVMgc*);
void fklVMpushExtraMarkFunc(FklVMgc*,FklVMextraMarkFunc,void*);

void fklRunVMinSingleThread(FklVM* exe);
void fklVMthreadStart(FklVM*,FklVMqueue* q);
FklVM* fklCreateVM(FklByteCodelnt*,FklSymbolTable*,FklFuncPrototypes*,uint32_t);
FklVM* fklCreateThreadVM(FklVMvalue*
		,FklVM* prev
		,FklVM* next
		,size_t libNum
		,FklVMlib* libs);

void fklDestroyVMvalue(FklVMvalue*);
void fklInitVMstack(FklVM*);
void fklUninitVMstack(FklVM*);
void fklAllocMoreStack(FklVM*);
void fklShrinkStack(FklVM*);
int fklCreateCreateThread(FklVM*);
FklVMframe* fklHasSameProc(FklVMvalue* proc,FklVMframe*);
FklVMgc* fklCreateVMgc(FklSymbolTable* st);
FklVMvalue** fklAllocLocalVarSpaceFromGC(FklVMgc*,uint32_t llast,uint32_t* pllast);
FklVMvalue** fklAllocLocalVarSpaceFromGCwithoutLock(FklVMgc*,uint32_t llast,uint32_t* pllast);

void fklVMacquireSt(FklVMgc*);
void fklVMreleaseSt(FklVMgc*);
FklSymbolHashItem* fklVMaddSymbol(FklVMgc*,const FklString* str);
FklSymbolHashItem* fklVMaddSymbolCstr(FklVMgc*,const char* str);
FklSymbolHashItem* fklVMaddSymbolCharBuf(FklVMgc*,const char* str,size_t);
FklSymbolHashItem* fklVMgetSymbolWithId(FklVMgc*,FklSid_t id);

void fklVMgcMarkAllRootToGray(FklVM* curVM);
int fklVMgcPropagate(FklVMgc* gc);
void fklVMgcCollect(FklVMgc* gc,FklVMvalue** pw);
void fklVMgcSweep(FklVMvalue*);
void fklVMgcRemoveUnusedGrayCache(FklVMgc* gc);

void fklDestroyVMgc(FklVMgc*);

void fklDestroyAllVMs(FklVM* cur);
void fklDeleteCallChain(FklVM*);

FklGCstate fklGetGCstate(FklVMgc*);
void fklVMgcToGray(FklVMvalue*,FklVMgc*);

void fklDestroyAllValues(FklVMgc*);

void fklDBG_printVMvalue(FklVMvalue*,FILE*,FklVMgc* gc);
void fklDBG_printVMstack(FklVM*,FILE*,int,FklVMgc* gc);

FklString* fklVMstringify(FklVMvalue*,FklVMgc*);
FklString* fklVMstringifyAsPrinc(FklVMvalue*,FklVMgc*);
void fklPrintVMvalue(FklVMvalue* value
		,FILE* fp
		,void(*atomPrinter)(FklVMvalue* v,FILE* fp,FklVMgc* gc)
		,FklVMgc* gc);
void fklPrin1VMvalue(FklVMvalue*,FILE*,FklVMgc* gc);
void fklPrincVMvalue(FklVMvalue*,FILE*,FklVMgc* gc);
FklBuiltinErrorType fklVMprintf(FklVM*
		,FILE* fp
		,const FklString* fmt
		,uint64_t* plen);

FklBuiltinErrorType fklVMformat(FklVM*
		,FklStringBuffer* buf
		,const FklString* fmt
		,uint64_t* plen);

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
uint32_t fklResBpIn(FklVM*,uint32_t);

#define FKL_CHECK_TYPE(V,P,ERR_INFO,EXE) if(!P(V))FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_INCORRECT_TYPE_VALUE,EXE)
#define FKL_CHECK_REST_ARG(EXE,ERR_INFO) if(fklResBp((EXE)))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_TOOMANYARG,EXE)

FklVMvalue* fklMakeVMint(int64_t r64,FklVM*);
FklVMvalue* fklMakeVMuint(uint64_t r64,FklVM*);
FklVMvalue* fklMakeVMintD(double r64,FklVM*);
int fklIsVMnumber(const FklVMvalue* p);
int fklIsVMint(const FklVMvalue* p);
int fklIsList(const FklVMvalue* p);
int fklIsSymbolList(const FklVMvalue* p);
int64_t fklGetInt(const FklVMvalue* p);
uint64_t fklVMintToHashv(const FklVMvalue* p);
double fklGetDouble(const FklVMvalue* p);

FklHashTable* fklCreateValueSetHashtable(void);
void fklInitValueSetHashTable(FklHashTable* ht);
int fklHasCircleRef(FklVMvalue* first_value);
void fklScanCirRef(FklVMvalue* s,FklHashTable* circle_head_set);

void fklInitLineNumHashTable(FklHashTable* ht);

FklVMerrorHandler* fklCreateVMerrorHandler(FklSid_t* typeIds,uint32_t,FklInstruction* spc,uint64_t cpc);
void fklDestroyVMerrorHandler(FklVMerrorHandler*);
int fklRaiseVMerror(FklVMvalue* err,FklVM*);

void fklPrintErrBacktrace(FklVMvalue*,FklVM*,FILE* fp);
void fklPrintFrame(FklVMframe* cur,FklVM* exe,FILE* fp);
void fklPrintBacktrace(FklVM*,FILE* fp);

void fklInitMainProcRefs(FklVMproc* mainProc,FklVMvalue** closure,uint32_t count);

void fklInitMainVMframeWithProc(FklVM*
		,FklVMframe* tmp
		,FklVMproc* code
		,FklVMframe* prev
		,FklFuncPrototypes* pts);

void fklShrinkLocv(FklVM*);
FklVMvalue** fklAllocSpaceForLocalVar(FklVM*,uint32_t);
FklVMvalue** fklAllocMoreSpaceForMainFrame(FklVM*,uint32_t);
void fklUpdateAllVarRef(FklVMframe*,FklVMvalue**);

FklVMframe* fklCreateVMframeWithCodeObj(FklVM* exe,FklVMvalue* codeObj,uint32_t pid,FklVMframe*);
FklVMframe* fklCreateVMframeWithProcValue(FklVM* exe,FklVMvalue*,FklVMframe*);

FklVMvalue* fklCreateVMvalueVarRef(FklVM* exe,FklVMvalue** loc,uint32_t idx);
FklVMvalue* fklCreateClosedVMvalueVarRef(FklVM* exe,FklVMvalue* v);

void fklDestroyVMframe(FklVMframe*,FklVM* exe);
FklString* fklGenErrorMessage(FklBuiltinErrorType type);
FklString* fklGenInvalidSymbolErrorMessage(char* str,int _free,FklBuiltinErrorType);

int fklIsVMhashEq(FklHashTable*);
int fklIsVMhashEqv(FklHashTable*);
int fklIsVMhashEqual(FklHashTable*);
uintptr_t fklGetVMhashTableType(FklHashTable*);
const char* fklGetVMhashTablePrefix(FklHashTable*);

uintptr_t fklVMvalueEqHashv(const FklVMvalue*);
uintptr_t fklVMvalueEqvHashv(const FklVMvalue*);
uintptr_t fklVMvalueEqualHashv(const FklVMvalue* v);

void fklVMhashTableSet(FklVMvalue* key,FklVMvalue* v,FklHashTable* ht);
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
void fklAtomicVMcproc(FklVMvalue*,FklVMgc*);
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
FklVMvalue* fklCreateVMvalueVec3(FklVM* vm,FklVMvalue* a,FklVMvalue* b,FklVMvalue* c);

FklVMvalue* fklCreateVMvalueF64(FklVM*,double f64);

FklVMvalue* fklCreateVMvalueProc(FklVM*
		,FklInstruction* spc
		,uint64_t cpc
		,FklVMvalue* codeObj
		,uint32_t pid);
FklVMvalue* fklCreateVMvalueProcWithWholeCodeObj(FklVM*,FklVMvalue* codeObj,uint32_t pid);
FklVMvalue* fklCreateVMvalueProcWithFrame(FklVM*,FklVMframe* f,size_t,uint32_t pid);
void fklCreateVMvalueClosureFrom(FklVM*,FklVMvalue** closure,FklVMframe* f,uint32_t from,FklFuncPrototype* pt);

#ifdef WIN32
#define FKL_DLL_EXPORT __declspec( dllexport )
#else
#define FKL_DLL_EXPORT
#endif

FklVMvalue* fklCreateVMvalueDll(FklVM*,const char*,char**);
void* fklGetAddress(const char* funcname,uv_lib_t* dll);

FklVMvalue* fklCreateVMvalueCproc(FklVM*
		,FklVMcFunc
		,FklVMvalue* dll
		,FklVMvalue* pd
		,FklSid_t sid);

void fklDoPrintCprocBacktrace(FklSid_t,FILE* fp,FklVMgc* gc);

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

FklVMvalue* fklCreateVMvalueBigIntWithString(FklVM* exe,const FklString* str,int base);

FklVMvalue* fklCreateVMvalueBigIntWithDecString(FklVM* exe,const FklString* str);

FklVMvalue* fklCreateVMvalueBigIntWithOctString(FklVM* exe,const FklString* str);

FklVMvalue* fklCreateVMvalueBigIntWithHexString(FklVM* exe,const FklString* str);

FklVMvalue* fklCreateVMvalueBigIntWithI64(FklVM*,int64_t);

FklVMvalue* fklCreateVMvalueBigIntWithU64(FklVM*,uint64_t);

FklVMvalue* fklCreateVMvalueBigIntWithF64(FklVM*,double);

FklVMvalue* fklCreateVMvalueUdata(FklVM*
		,const FklVMudMetaTable* t
		,FklVMvalue* rel);

FklVMvalue* fklCreateVMvalueCodeObj(FklVM*,FklByteCodelnt* bcl);

FklVMvalue* fklCreateVMvalueFromNastNode(FklVM* vm
		,const FklNastNode* node
		,FklHashTable* lineHash);

FklNastNode* fklCreateNastNodeFromVMvalue(FklVMvalue* v
		,uint64_t curline
		,FklHashTable*
		,FklVMgc* gc);

FklVMvalue* fklCreateVMvalueEof(FklVM* exe);
int fklIsVMeofUd(FklVMvalue* v);
// value getters

#define FKL_VM_CAR(V) (((FklVMvaluePair*)(V))->pair.car)
#define FKL_VM_CDR(V) (((FklVMvaluePair*)(V))->pair.cdr)

#define FKL_VM_STR(V) (((FklVMvalueStr*)(V))->str)

#define FKL_VM_BVEC(V) (((FklVMvalueBvec*)(V))->bvec)

#define FKL_VM_VEC(V) (&(((FklVMvalueVec*)(V))->vec))

#define FKL_VM_F64(V) (((FklVMvalueF64*)(V))->f64)

#define FKL_VM_PROC(V) (&(((FklVMvalueProc*)(V))->proc))

#define FKL_VM_DLL(V) (&(((FklVMvalueDll*)(V))->dll))

#define FKL_VM_CPROC(V) (&(((FklVMvalueCproc*)(V))->cproc))

#define FKL_VM_FP(V) (&(((FklVMvalueFp*)(V))->fp))

#define FKL_VM_HASH(V) (&(((FklVMvalueHash*)(V))->hash))

#define FKL_VM_CHANL(V) (&(((FklVMvalueChanl*)(V))->chanl))

#define FKL_VM_ERR(V) (&(((FklVMvalueErr*)(V))->err))

#define FKL_VM_BI(V) (&(((FklVMvalueBigInt*)(V))->bi))

#define FKL_VM_UD(V) (&(((FklVMvalueUd*)(V))->ud))

#define FKL_VM_BOX(V) (((FklVMvalueBox*)(V))->box)

#define FKL_VM_CO(V) (&(((FklVMvalueCodeObj*)(V))->bcl))

#define FKL_VM_VAR_REF(V) ((FklVMvalueVarRef*)(V))

// vmparser

void fklVMvaluePushState0ToStack(FklPtrStack* stateStack);
#define FKL_VMVALUE_PARSE_OUTER_CTX_INIT(EXE) {.maxNonterminalLen=0,.line=1,.start=NULL,.cur=NULL,.create=fklVMvalueTerminalCreate,.destroy=fklVMvalueTerminalDestroy,.ctx=(void*)(EXE)}

void* fklVMvalueTerminalCreate(const char* s,size_t len,size_t line,void* ctx);
void fklVMvalueTerminalDestroy(void*);

void fklAddToGC(FklVMvalue*,FklVM*);
FklVMvalue* fklCreateTrueValue(void);
FklVMvalue* fklCreateNilValue(void);

void fklDropTop(FklVM* s);

#define FKL_VM_GET_ARG_NUM(S) ((S)->tp-(S)->bp)

#define FKL_VM_POP_ARG(S) (((S)->tp>(S)->bp)?(S)->base[--(S)->tp]:NULL)

#define FKL_VM_GET_TOP_VALUE(S) ((S)->base[(S)->tp-1])

#define FKL_VM_POP_TOP_VALUE(S) ((S)->base[--(S)->tp])

#define FKL_VM_GET_VALUE(S,N) ((S)->base[(S)->tp-(N)])

#define FKL_VM_PUSH_VALUE(S,V) (((S)->tp>=(S)->last?\
		((S)->last+=FKL_VM_STACK_INC_NUM,\
		 (S)->size+=FKL_VM_STACK_INC_SIZE,\
		 (S)->base=(FklVMvalue**)fklRealloc((S)->base,(S)->size),\
		 (FKL_ASSERT((S)->base)),NULL):NULL),(S)->base[(S)->tp++]=(V))

#define FKL_VM_SET_TP_AND_PUSH_VALUE(S,T,V) (((S)->tp=(T)+1),((S)->base[(T)]=(V)))

#define FKL_DECL_AND_CHECK_ARG(a,exe,Pname) \
	FklVMvalue* a=FKL_VM_POP_ARG(exe);\
	if(!a)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);

#define FKL_DECL_AND_CHECK_ARG2(a,b,exe,Pname) \
	FklVMvalue* a=FKL_VM_POP_ARG(exe);\
	FklVMvalue* b=FKL_VM_POP_ARG(exe);\
	if(!b||!a)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);

#define FKL_DECL_AND_CHECK_ARG3(a,b,c,exe,Pname) \
	FklVMvalue* a=FKL_VM_POP_ARG(exe);\
	FklVMvalue* b=FKL_VM_POP_ARG(exe);\
	FklVMvalue* c=FKL_VM_POP_ARG(exe);\
	if(!c||!b||!a)\
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);

FklVMvalue* fklPopArg(FklVM*);
FklVMvalue* fklGetTopValue(FklVM*);
FklVMvalue* fklPopTopValue(FklVM*);
FklVMvalue* fklGetValue(FklVM*,uint32_t);
FklVMvalue** fklGetStackSlot(FklVM*,uint32_t);

int fklVMvalueEqual(const FklVMvalue*,const FklVMvalue*);
int fklVMvalueEqv(const FklVMvalue*,const FklVMvalue*);
int fklVMvalueEq(const FklVMvalue*,const FklVMvalue*);
int fklVMvalueCmp(FklVMvalue*,FklVMvalue*,int*);

FklVMfpRW fklGetVMfpRwFromCstr(const char* mode);

int fklVMfpRewind(FklVMfp* vfp,FklStringBuffer*,size_t j);
int fklVMfpEof(FklVMfp*);

int fklUninitVMfp(FklVMfp*);

#define FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS uint32_t* num,FklSid_t** exports,FklSymbolTable* st
typedef void (*FklCodegenDllLibInitExportFunc)(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS);

#define FKL_IMPORT_DLL_INIT_FUNC_ARGS FklVM* exe,FklVMvalue* dll,uint32_t* count
typedef FklVMvalue** (*FklImportDllInitFunc)(FKL_IMPORT_DLL_INIT_FUNC_ARGS);

void fklInitVMdll(FklVMvalue* rel,FklVM*);

uint64_t fklVMchanlRecvqLen(FklVMchanl* ch);
uint64_t fklVMchanlSendqLen(FklVMchanl* ch);
uint64_t fklVMchanlMessageNum(FklVMchanl* ch);
int fklVMchanlFull(FklVMchanl* ch);

void fklVMsleep(FklVM*,uint64_t ms);

void fklVMread(FklVM*
		,FILE* fp
		,FklStringBuffer* buf
		,uint64_t len
		,int d);

void fklVMacquireWq(FklVMgc*);
void fklVMreleaseWq(FklVMgc*);
void fklQueueWorkInIdleThread(FklVM* vm
		,void (*cb)(FklVM*,void*)
		,void* arg);
void fklResumeQueueWorkThread(FklVM*,uv_cond_t*);
void fklResumeQueueIdleThread(FklVM*,uv_cond_t*);

void fklNoticeThreadLock(FklVM*);
void fklDontNoticeThreadLock(FklVM*);
void fklUnlockThread(FklVM*);
void fklLockThread(FklVM*);
void fklVMstopTheWorld(FklVMgc*);
void fklVMcontinueTheWorld(FklVMgc*);

void fklChanlSend(FklVMchanl*,FklVMvalue* msg,FklVM*);
void fklChanlRecv(FklVMchanl*,FklVMvalue**,FklVM*);
int fklChanlRecvOk(FklVMchanl*,FklVMvalue**);

void fklVMvecConcat(FklVMvec*,const FklVMvec*);

#define FKL_GET_UD_DATA(TYPE,UD) ((TYPE*)(UD)->data)
#define FKL_DECL_UD_DATA(NAME,TYPE,UD) TYPE* NAME=FKL_GET_UD_DATA(TYPE,UD)
#define FKL_DECL_VM_UD_DATA(NAME,TYPE,UD) TYPE* NAME=FKL_GET_UD_DATA(TYPE,FKL_VM_UD(UD))

int fklIsCallableUd(const FklVMudata*);
int fklIsCmpableUd(const FklVMudata*);
int fklIsWritableUd(const FklVMudata*);
int fklIsAbleToStringUd(const FklVMudata*);
int fklIsAbleAsPrincUd(const FklVMudata*);
int fklUdHasLength(const FklVMudata*);

void fklPrincVMudata(const FklVMudata*,FILE*,FklVMgc*);
void fklPrin1VMudata(const FklVMudata*,FILE*,FklVMgc*);
void fklFinalizeVMudata(FklVMudata*);
int fklEqualVMudata(const FklVMudata*,const FklVMudata*);
void fklCallVMudata(const FklVMudata*,const FklVMudata*);
int fklCmpVMudata(const FklVMudata*,const FklVMvalue*,int*);
void fklWriteVMudata(const FklVMudata*,FILE* fp);
size_t fklLengthVMudata(const FklVMudata*);
int fklAppendVMudata(FklVMudata*,const FklVMudata*);
int fklCopyVMudata(const FklVMudata*,FklVMudata* dst);
size_t fklHashvVMudata(const FklVMudata*);
void fklUdToString(const FklVMudata*,FklStringBuffer*,FklVMgc*);
void fklUdAsPrinc(const FklVMudata*,FklStringBuffer*,FklVMgc*);
FklBytevector* fklUdToBytevector(const FklVMudata*);

int fklIsCallable(FklVMvalue*);
int fklIsAppendable(FklVMvalue*);
void fklInitVMargs(FklVMgc* gc,int argc,const char* const* argv);

int fklIsVMnumberLt0(const FklVMvalue*);
uint64_t fklGetUint(const FklVMvalue*);

FklVMvalue** fklPushVMvalue(FklVM* s,FklVMvalue* v);

void fklSetTpAndPushValue(FklVM* exe,uint32_t rtp,FklVMvalue* retval);

size_t fklVMlistLength(FklVMvalue*);

void fklPushVMframe(FklVMframe*,FklVM* exe);
FklVMframe* fklCreateOtherObjVMframe(FklVM* exe,const FklVMframeContextMethodTable* t,FklVMframe* prev);
FklVMframe* fklCreateNewOtherObjVMframe(const FklVMframeContextMethodTable* t,FklVMframe* prev);

void fklSwapCompoundFrame(FklVMframe*,FklVMframe*);
unsigned int fklGetCompoundFrameMark(const FklVMframe*);
unsigned int fklSetCompoundFrameMark(FklVMframe*,unsigned int);

FklVMCompoundFrameVarRef* fklGetCompoundFrameLocRef(FklVMframe* f);

FklVMvalue* fklGetCompoundFrameProc(const FklVMframe*);
FklFuncPrototype* fklGetCompoundFrameProcPrototype(const FklVMframe*,FklVM* exe);

FklInstruction* fklGetCompoundFrameCode(const FklVMframe*);

FklInstruction* fklGetCompoundFrameEnd(const FklVMframe*);

FklOpcode fklGetCompoundFrameOp(FklVMframe* frame);

void fklAddCompoundFrameCp(FklVMframe*,int64_t a);

uint64_t fklGetCompoundFrameCpc(const FklVMframe*);
FklSid_t fklGetCompoundFrameSid(const FklVMframe*);

FklVMframe* fklCreateVMframeWithCompoundFrame(const FklVMframe*,FklVMframe* prev);
FklVMframe* fklCopyVMframe(FklVM*,FklVMframe*,FklVMframe* prev);
void fklDestroyVMframes(FklVMframe* h);

void fklDestroyVMlib(FklVMlib* lib);

void fklInitVMlib(FklVMlib*,FklVMvalue* proc);

void fklInitVMlibWithCodeObj(FklVMlib*,FklVMvalue* codeObj,FklVM* exe,uint32_t protoId);

void fklUninitVMlib(FklVMlib*);

void fklInitBuiltinErrorType(FklSid_t errorTypeId[FKL_BUILTIN_ERR_NUM],FklSymbolTable* table);
FklSid_t fklGetBuiltinErrorType(FklBuiltinErrorType type,FklSid_t errorTypeId[FKL_ERR_INCORRECT_TYPE_VALUE]);

#define FKL_RAISE_BUILTIN_ERROR(WHO,ERRORTYPE,EXE) do{\
	FklString* errorMessage=fklGenErrorMessage((ERRORTYPE));\
	FklVMvalue* err=fklCreateVMvalueError((EXE)\
			,fklGetBuiltinErrorType(ERRORTYPE,(EXE)->gc->builtinErrorTypeId)\
			,(WHO)\
			,errorMessage);\
	fklRaiseVMerror(err,(EXE));\
}while(0)

#define FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,ERRORTYPE,EXE) do{\
	FklString* errorMessage=fklGenErrorMessage((ERRORTYPE));\
	FklVMvalue* err=fklCreateVMvalueErrorWithCstr((EXE)\
			,fklGetBuiltinErrorType(ERRORTYPE,(EXE)->gc->builtinErrorTypeId)\
			,(WHO)\
			,errorMessage);\
	fklRaiseVMerror(err,(EXE));\
}while(0)

#define FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(WHO,STR,FREE,ERRORTYPE,EXE) do{\
	FklString* errorMessage=fklGenInvalidSymbolErrorMessage((STR),(FREE),(ERRORTYPE));\
	FklVMvalue* err=fklCreateVMvalueErrorWithCstr((EXE)\
			,fklGetBuiltinErrorType(ERRORTYPE,(EXE)->gc->builtinErrorTypeId)\
			,(WHO)\
			,errorMessage);\
	fklRaiseVMerror(err,(EXE));\
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
#define FKL_IS_CPROC(P) (FKL_GET_TAG(P)==FKL_TAG_PTR&&(P)->type==FKL_TYPE_CPROC)
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

#define FKL_VM_USER_DATA_DEFAULT_PRINT(NAME,DATA_TYPE_NAME) static void NAME(const FklVMudata* ud,FILE* fp,FklVMgc* gc) {\
	fprintf(fp,"#<"#DATA_TYPE_NAME" %p>",ud);\
}

#define FKL_VM_USER_DATA_DEFAULT_AS_PRINT(NAME,DATA_TYPE_NAME) static void NAME(const FklVMudata* ud,FklStringBuffer* buf,FklVMgc* gc) {\
	fklStringBufferPrintf(buf,"#<"#DATA_TYPE_NAME" %p>",ud);\
}

#ifdef __cplusplus
}
#endif

#endif
