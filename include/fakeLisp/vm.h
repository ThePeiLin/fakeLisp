#ifndef FKL_VM_H
#define FKL_VM_H

#include "base.h"
#include "builtin.h"
#include "bytecode.h"
#include "common.h"
#include "grammer.h"
#include "nast.h"
#include "utils.h"

#include <setjmp.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <string.h>
#include <uv.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FKL_TYPE_F64 = 0,
    FKL_TYPE_BIGINT,
    FKL_TYPE_STR,
    FKL_TYPE_VECTOR,
    FKL_TYPE_PAIR,
    FKL_TYPE_BOX,
    FKL_TYPE_BYTEVECTOR,
    FKL_TYPE_USERDATA,
    FKL_TYPE_PROC,
    FKL_TYPE_CPROC,
    FKL_TYPE_HASHTABLE,
    FKL_TYPE_VAR_REF,
    FKL_VM_VALUE_GC_TYPE_NUM,
} FklValueType;

struct FklVM;
struct FklVMvalue;
struct FklCprocFrameContext;
#define FKL_CPROC_ARGL struct FklVM *exe, struct FklCprocFrameContext *ctx
#define FKL_MAX_INDIRECT_TAIL_CALL_COUNT (4)

typedef int (*FklVMcFunc)(FKL_CPROC_ARGL);
typedef struct FklVMvalue *FklVMptr;
typedef enum {
    FKL_TAG_PTR = 0,
    FKL_TAG_NIL,
    FKL_TAG_FIX,
    FKL_TAG_SYM,
    FKL_TAG_CHR,
} FklVMptrTag;

#define FKL_PTR_TAG_NUM (FKL_TAG_CHR + 1)

typedef struct {
    uv_lib_t dll;
    struct FklVMvalue *pd;
} FklVMdll;

typedef struct {
    struct FklVMchanlRecvq {
        struct FklVMchanlRecv *head;
        struct FklVMchanlRecv **tail;
    } recvq;

    struct FklVMchanlSendq {
        struct FklVMchanlSend *head;
        struct FklVMchanlSend **tail;
    } sendq;

    uv_mutex_t lock;

    uint32_t recvx;
    uint32_t sendx;
    uint32_t count;
    uint32_t qsize;
    struct FklVMvalue *buf[];
} FklVMchanl;

typedef struct FklVMchanlSend {
    struct FklVMchanlSend *next;
    uv_cond_t cond;
    struct FklVMvalue *msg;
} FklVMchanlSend;

typedef struct FklVMchanlRecv {
    struct FklVMchanlRecv *next;
    uv_cond_t cond;
    struct FklVMvalue **slot;
} FklVMchanlRecv;

typedef struct {
    struct FklVMvalue *car;
    struct FklVMvalue *cdr;
} FklVMpair;

typedef struct {
    int64_t num;
    FklBigIntDigit digits[1];
} FklVMbigInt;

#define FKL_VM_FP_R_MASK (1)
#define FKL_VM_FP_W_MASK (2)

typedef enum {
    FKL_VM_FP_R = 1,
    FKL_VM_FP_W = 2,
    FKL_VM_FP_RW = 3,
} FklVMfpRW;

typedef struct {
    FILE *fp;
    FklVMfpRW rw;
} FklVMfp;

typedef struct {
    size_t size;
    struct FklVMvalue *base[];
} FklVMvec;

#define FKL_VM_UD_COMMON_HEADER                                                \
    struct FklVMvalue *rel;                                                    \
    const struct FklVMudMetaTable *t

typedef struct {
    FKL_VM_UD_COMMON_HEADER;
    void *data[];
} FklVMud;

typedef enum {
    FKL_MARK_W = 0,
    FKL_MARK_G,
    FKL_MARK_B,
} FklVMvalueMark;

#define FKL_VM_VALUE_COMMON_HEADER                                             \
    struct FklVMvalue *next;                                                   \
    FklVMvalueMark volatile mark : 32;                                         \
    FklValueType type : 32

typedef struct FklVMvalue {
    FKL_VM_VALUE_COMMON_HEADER;
    void *data[];
} FklVMvalue;

// builtin values
typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    double f64;
} FklVMvalueF64;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklVMbigInt bi;
} FklVMvalueBigInt;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklString str;
} FklVMvalueStr;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklVMvec vec;
} FklVMvalueVec;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklVMpair pair;
} FklVMvaluePair;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklVMvalue *box;
} FklVMvalueBox;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklBytevector bvec;
} FklVMvalueBvec;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklVMud ud;
} FklVMvalueUd;

typedef struct {
    FklVMvalue *key;
    FklVMvalue *v;
} FklVMhashTableItem;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    uint32_t idx;
    _Atomic(FklVMvalue **) ref;
    FklVMvalue *v;
} FklVMvalueVarRef;

typedef struct FklVMproc {
    FklInstruction *spc;
    FklInstruction *end;
    FklSid_t sid;
    uint32_t protoId;
    uint32_t lcount;
    uint32_t rcount;
    FklVMvalue **closure;
    FklVMvalue *codeObj;
} FklVMproc;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklVMproc proc;
} FklVMvalueProc;

typedef enum {
    FKL_FRAME_COMPOUND = 0,
    FKL_FRAME_OTHEROBJ,
} FklFrameType;

typedef struct FklVMvarRefList {
    FklVMvalue *ref;
    struct FklVMvarRefList *next;
} FklVMvarRefList;

typedef struct {
    FklVMvalue **lref;
    FklVMvarRefList *lrefl;
    FklVMvalue **loc;
    FklVMvalue **ref;
    uint32_t base;
    uint32_t lcount;
    uint32_t rcount;
} FklVMCompoundFrameVarRef;

typedef struct {
    FklSid_t sid : 61;
    unsigned int tail : 1;
    unsigned int mark : 2;
    FklVMvalue *proc;
    FklVMCompoundFrameVarRef lr;
    FklInstruction *spc;
    FklInstruction *pc;
    FklInstruction *end;
    uint32_t tp;
    uint32_t bp;
} FklVMCompoundFrameData;

typedef struct FklCprocFrameContext {
    FklVMvalue *proc;
    FklVMcFunc func;
    FklVMvalue *pd;
    union {
        void *pointer;
        uintptr_t context;
        intptr_t icontext;
        struct {
            union {
                uint32_t cau;
                int32_t cai;
            };
            union {
                uint32_t cbu;
                int32_t cbi;
            };
        };
    };
    uint32_t rtp;
} FklCprocFrameContext;

#define FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(TYPE)                                 \
    static_assert(sizeof(TYPE)                                                 \
                      <= (sizeof(FklVMCompoundFrameData) - sizeof(void *)),    \
                  #TYPE " is too big")

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(FklCprocFrameContext);

struct FklVMgc;
typedef struct {
    int (*step)(void *data, struct FklVM *);
    void (*print_backtrace)(void *data, FILE *fp, struct FklVMgc *);
    void (*atomic)(void *data, struct FklVMgc *);
    void (*finalizer)(void *data);
    void (*copy)(void *dst, const void *src, struct FklVM *);
} FklVMframeContextMethodTable;

struct FklVMframe;

#define FKL_VM_ERROR_CALLBACK_ARGL                                             \
    struct FklVMframe *f, FklVMvalue *ev, struct FklVM *vm
typedef int (*FklVMerrorCallBack)(FKL_VM_ERROR_CALLBACK_ARGL);

typedef struct FklVMframe {
    FklFrameType type;
    FklVMerrorCallBack errorCallBack;
    struct FklVMframe *prev;
    union {
        FklVMCompoundFrameData c;
        struct {
            const FklVMframeContextMethodTable *t;
            uint8_t data[1];
        };
    };
} FklVMframe;

void fklDoPrintBacktrace(FklVMframe *f, FILE *fp, struct FklVMgc *table);
void fklCallObj(struct FklVM *exe, FklVMvalue *);
void fklTailCallObj(struct FklVM *exe, FklVMvalue *);
void fklDoAtomicFrame(FklVMframe *f, struct FklVMgc *);
void fklDoCopyObjFrameContext(FklVMframe *, FklVMframe *, struct FklVM *exe);
void *fklGetFrameData(FklVMframe *f);

static inline int fklDoCallableObjFrameStep(FklVMframe *f, struct FklVM *exe) {
    return f->t->step(fklGetFrameData(f), exe);
}

void fklDoFinalizeObjFrame(struct FklVM *, FklVMframe *f);
void fklDoFinalizeCompoundFrame(struct FklVM *exe, FklVMframe *frame);
void fklCloseVMvalueVarRef(FklVMvalue *ref);
int fklIsClosedVMvalueVarRef(FklVMvalue *ref);

typedef struct FklVMlib {
    FklVMvalue *proc;
    FklVMvalue **loc;
    uint32_t count;
    uint8_t imported;
    uint8_t belong;
} FklVMlib;

#define FKL_VM_ERR_RAISE (1)

typedef enum {
    FKL_VM_EXIT,
    FKL_VM_READY,
    FKL_VM_RUNNING,
    FKL_VM_WAITING,
} FklVMstate;

#define FKL_VM_LOCV_INC_NUM (32)
#define FKL_VM_STACK_INC_NUM (128)
#define FKL_VM_STACK_INC_SIZE (sizeof(FklVMvalue *) * 128)

// FklThreadQueue
#define FKL_QUEUE_ELM_TYPE struct FklVM *
#define FKL_QUEUE_ELM_TYPE_NAME Thread
#include "queue.h"

typedef struct {
    uv_mutex_t pre_running_lock;
    FklThreadQueue pre_running_q;
    atomic_size_t running_count;
    FklThreadQueue running_q;
} FklVMqueue;

typedef struct FklVMlocvList {
    struct FklVMlocvList *next;
    uint32_t llast;
    FklVMvalue **locv;
} FklVMlocvList;

#define FKL_VM_INS_FUNC_ARGL struct FklVM *exe, FklInstruction *ins
typedef void (*FklVMinsFunc)(FKL_VM_INS_FUNC_ARGL);

#define FKL_VM_GC_LOCV_CACHE_NUM (8)
#define FKL_VM_GC_LOCV_CACHE_LAST_IDX (FKL_VM_GC_LOCV_CACHE_NUM - 1)

typedef void (*FklVMatExitFunc)(struct FklVM *, void *);
typedef void (*FklVMatExitMarkFunc)(void *, struct FklVMgc *);
typedef int (*FklVMrunCb)(struct FklVM *, FklVMframe *const exit_frame);

typedef struct FklVM {
    FklVMrunCb thread_run_cb;
    FklVMrunCb run_cb;

    uv_thread_t tid;
    uv_mutex_t lock;
    FklVMvalue *obj_head;
    FklVMvalue *obj_tail;

    int16_t trapping;
    int16_t is_single_thread;
    uint32_t ltp;
    uint32_t llast;
    uint32_t old_locv_count;
    FklVMvalue **locv;
    FklVMlocvList old_locv_cache[FKL_VM_GC_LOCV_CACHE_NUM];
    FklVMlocvList *old_locv_list;
    // op stack

    size_t size;
    uint32_t last;
    uint32_t tp;
    uint32_t bp;
    FklVMvalue **base;

    FklVMframe inplace_frame;
    FklVMframe *top_frame;

    FklVMframe *frame_cache_head;
    FklVMframe **frame_cache_tail;

    struct FklVMvalue *chan;
    struct FklVMgc *gc;
    uint64_t libNum;
    FklVMlib *libs;
    struct FklVM *prev;
    struct FklVM *next;
    jmp_buf buf;

    FklFuncPrototypes *pts;
    FklVMlib *importingLib;

    FklVMstate volatile state;

    atomic_int notice_lock;

    FklVMinsFunc dummy_ins_func;
    void *debug_ctx;

    uint64_t rand_state[4];

    struct FklVMatExit {
        struct FklVMatExit *next;
        FklVMatExitFunc func;
        FklVMatExitMarkFunc mark;
        void (*finalizer)(void *);
        void *arg;
    } *atexit;

    struct FklVMinterruptHandleList *int_list;
} FklVM;

// FklVMvalueVector
#define FKL_VECTOR_ELM_TYPE FklVMvalue *
#define FKL_VECTOR_ELM_TYPE_NAME VMvalue
#include "vector.h"

// FklVMpairVector
#define FKL_VECTOR_ELM_TYPE FklVMpair
#define FKL_VECTOR_ELM_TYPE_NAME VMpair
#include "vector.h"

// FklVMobjTable
#define FKL_TABLE_KEY_TYPE FklVMvalue const *
#define FKL_TABLE_ELM_NAME VMobj
#define FKL_TABLE_KEY_HASH                                                     \
    return fklHash64Shift(                                                     \
        FKL_TYPE_CAST(uintptr_t, (*pk) / alignof(FklVMvalue)));
#include "table.h"

// FklLineNumTable
#define FKL_TABLE_KEY_TYPE FklVMvalue const *
#define FKL_TABLE_VAL_TYPE uint64_t
#define FKL_TABLE_ELM_NAME LineNum
#define FKL_TABLE_KEY_HASH                                                     \
    return fklHash64Shift(                                                     \
        FKL_TYPE_CAST(uintptr_t, (*pk) / alignof(FklVMvalue)));
#include "table.h"

typedef FklVMvalue *(*FklVMudCopyAppender)(FklVM *exe, const FklVMud *ud,
                                           uint32_t argc,
                                           FklVMvalue *const *top);
typedef int (*FklVMudAppender)(FklVMud *, uint32_t argc,
                               FklVMvalue *const *top);

typedef struct FklVMudMetaTable {
    size_t size;
    void (*__as_princ)(const FklVMud *, FklStringBuffer *buf,
                       struct FklVMgc *gc);
    void (*__as_prin1)(const FklVMud *, FklStringBuffer *buf,
                       struct FklVMgc *gc);
    void (*__finalizer)(FklVMud *);
    int (*__equal)(const FklVMud *, const FklVMud *);
    void (*__call)(FklVMvalue *, FklVM *);
    int (*__cmp)(const FklVMud *, const FklVMvalue *, int *);
    void (*__write)(const FklVMud *, FILE *);
    void (*__atomic)(const FklVMud *, struct FklVMgc *);
    size_t (*__length)(const FklVMud *);
    FklVMudCopyAppender __copy_append;
    FklVMudAppender __append;
    size_t (*__hash)(const FklVMud *, FklVMvalueVector *s);
} FklVMudMetaTable;

typedef enum {
    FKL_GC_NONE = 0,
    FKL_GC_MARK_ROOT,
    FKL_GC_PROPAGATE,
    FKL_GC_SWEEP,
    FKL_GC_COLLECT,
    FKL_GC_SWEEPING,
    FKL_GC_DONE,
} FklGCstate;

#define FKL_VM_GC_LOCV_CACHE_LEVEL_NUM (5)
#define FKL_VM_GC_THRESHOLD_SIZE (2048)

typedef enum {
    FKL_ERR_DUMMY = 0,
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
    FKL_ERR_FAILD_TO_CREATE_BIGINT_FROM_MEM,
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
    FKL_ERR_ASSIGN_CONSTANT,
    FKL_ERR_REDEFINE_VARIABLE_AS_CONSTANT,
    FKL_ERR_EXP_HAS_NO_VALUE,
    FKL_BUILTIN_ERR_NUM,
} FklBuiltinErrorType;

typedef enum {
    FKL_INT_DONE = 0,
    FKL_INT_NEXT,
} FklVMinterruptResult;

typedef FklVMinterruptResult (*FklVMinterruptHandler)(FklVM *exe,
                                                      FklVMvalue *value,
                                                      FklVMvalue **pvalue,
                                                      void *);

typedef void (*FklVMextraMarkFunc)(struct FklVMgc *, void *arg);

typedef struct FklVMgc {
    FklGCstate volatile running;
    atomic_size_t num;
    size_t threshold;
    FklVMvalue *head;

    int argc;
    char **argv;

    struct FklVMgcGrayList {
        struct FklVMgcGrayList *next;
        FklVMvalue *v;
    } *gray_list;

    struct FklVMgcGrayList *gray_list_cache;

    struct FklVMgcGrayList *unused_gray_list_cache;

    FklVMqueue q;

    FklVM *main_thread;
    int exit_code;

    struct FklLocvCacheLevel {
        uv_mutex_t lock;
        uint32_t num;
        struct FklLocvCache {
            uint32_t llast;
            FklVMvalue **locv;
        } locv[FKL_VM_GC_LOCV_CACHE_NUM];
    } locv_cache[FKL_VM_GC_LOCV_CACHE_LEVEL_NUM];

    uv_rwlock_t st_lock;
    FklSymbolTable *st;
    FklFuncPrototypes *pts;

    FklSid_t builtinErrorTypeId[FKL_BUILTIN_ERR_NUM];

    uv_mutex_t workq_lock;
    struct FklVMworkq {
        struct FklVMidleWork {
            struct FklVMidleWork *next;
            uv_cond_t cond;
            FklVM *vm;
            void *arg;
            void (*cb)(FklVM *, void *);
        } *head;
        struct FklVMidleWork **tail;
    } workq;

    atomic_uint work_num;

    struct FklVMinterruptHandleList {
        struct FklVMinterruptHandleList *next;
        FklVMinterruptHandler int_handler;
        FklVMextraMarkFunc mark;
        void (*finalizer)(void *);
        void *int_handle_arg;
    } *int_list;

    uv_mutex_t extra_mark_lock;
    struct FklVMextraMarkObjList {
        struct FklVMextraMarkObjList *next;
        FklVMextraMarkFunc func;
        void (*finalizer)(void *);
        void *arg;
    } *extra_mark_list;

    FklVMvalue *builtin_refs[FKL_BUILTIN_SYMBOL_NUM];

    FklConstTable *kt;
    double *kf64;
    int64_t *ki64;
    FklString **kstr;
    FklBigInt **kbi;
    FklBytevector **kbvec;
} FklVMgc;

typedef struct {
    FklVMcFunc func;
    FklVMvalue *dll;
    FklSid_t sid;
    FklVMvalue *pd;
} FklVMcproc;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklVMcproc cproc;
} FklVMvalueCproc;

typedef struct {
    FklSid_t type;
    FklString *message;
} FklVMerror;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklHashTable hash;
} FklVMvalueHash;

typedef struct {
    FklSid_t *typeIds;
    FklVMproc proc;
    uint32_t num;
} FklVMerrorHandler;

void fklPopVMframe(FklVM *);
int fklRunVM(FklVM *volatile);
void fklVMatExit(FklVM *vm, FklVMatExitFunc func, FklVMatExitMarkFunc mark,
                 void (*finalizer)(void *), void *arg);
void fklVMidleLoop(FklVMgc *gc);
void fklVMtrappingIdleLoop(FklVMgc *gc);

FklVMinterruptResult fklVMinterrupt(FklVM *, FklVMvalue *v, FklVMvalue **pv);
void fklDestroyVMinterruptHandlerList(struct FklVMinterruptHandleList *l);
void fklVMpushInterruptHandler(FklVMgc *, FklVMinterruptHandler,
                               FklVMextraMarkFunc, void (*finalizer)(void *),
                               void *);
void fklVMpushInterruptHandlerLocal(FklVM *, FklVMinterruptHandler,
                                    FklVMextraMarkFunc,
                                    void (*finalizer)(void *), void *);

void fklVMpushExtraMarkFunc(FklVMgc *, FklVMextraMarkFunc,
                            void (*finalizer)(void *), void *);

void fklSetVMsingleThread(FklVM *exe);
void fklUnsetVMsingleThread(FklVM *exe);
void fklVMexecuteInstruction(FklVM *exe, FklOpcode op, FklInstruction *ins,
                             FklVMframe *frame);
int fklRunVMinSingleThread(FklVM *exe, FklVMframe *const exit_frame);
void fklCheckAndGCinSingleThread(FklVM *exe);
void fklVMthreadStart(FklVM *, FklVMqueue *q);
FklVM *fklCreateVMwithByteCode(FklByteCodelnt *, FklSymbolTable *,
                               FklConstTable *, FklFuncPrototypes *, uint32_t);
FklVM *fklCreateVM(FklVMvalue *proc, FklVMgc *gc, uint64_t lib_num,
                   FklVMlib *libs);
FklVM *fklCreateThreadVM(FklVMvalue *, FklVM *prev, FklVM *next, size_t libNum,
                         FklVMlib *libs);

void fklDestroyVMvalue(FklVMvalue *);
void fklInitVMstack(FklVM *);
void fklUninitVMstack(FklVM *);
void fklAllocMoreStack(FklVM *);
void fklShrinkStack(FklVM *);
int fklCreateCreateThread(FklVM *);
FklVMframe *fklHasSameProc(FklVMvalue *proc, FklVMframe *);

FklVMgc *fklCreateVMgc(FklSymbolTable *st, FklConstTable *kt,
                       FklFuncPrototypes *pts);
FklVMvalue **fklAllocLocalVarSpaceFromGC(FklVMgc *, uint32_t llast,
                                         uint32_t *pllast);
FklVMvalue **fklAllocLocalVarSpaceFromGCwithoutLock(FklVMgc *, uint32_t llast,
                                                    uint32_t *pllast);

void fklVMacquireSt(FklVMgc *);
void fklVMreleaseSt(FklVMgc *);
FklStrIdTableElm *fklVMaddSymbol(FklVMgc *, const FklString *str);
FklStrIdTableElm *fklVMaddSymbolCstr(FklVMgc *, const char *str);
FklStrIdTableElm *fklVMaddSymbolCharBuf(FklVMgc *, const char *str, size_t);
FklStrIdTableElm *fklVMgetSymbolWithId(FklVMgc *, FklSid_t id);

void fklVMgcUpdateConstArray(FklVMgc *gc, FklConstTable *kt);
void fklVMgcMarkAllRootToGray(FklVM *curVM);
int fklVMgcPropagate(FklVMgc *gc);
void fklVMgcCollect(FklVMgc *gc, FklVMvalue **pw);
void fklVMgcSweep(FklVMvalue *);
void fklVMgcRemoveUnusedGrayCache(FklVMgc *gc);
void fklVMgcUpdateThreshold(FklVMgc *);

void fklDestroyVMgc(FklVMgc *);

void fklDestroyAllVMs(FklVM *cur);
void fklDeleteCallChain(FklVM *);

FklGCstate fklGetGCstate(FklVMgc *);
void fklVMgcToGray(FklVMvalue *, FklVMgc *);

void fklDestroyAllValues(FklVMgc *);

void fklDBG_printVMvalue(FklVMvalue *, FILE *, FklVMgc *gc);
void fklDBG_printVMstack(FklVM *, FILE *, int, FklVMgc *gc);

FklVMvalue *fklVMstringify(FklVMvalue *, FklVM *);
FklVMvalue *fklVMstringifyAsPrinc(FklVMvalue *, FklVM *);
void fklPrin1VMvalue(FklVMvalue *, FILE *, FklVMgc *gc);
void fklPrincVMvalue(FklVMvalue *, FILE *, FklVMgc *gc);
FklBuiltinErrorType fklVMprintf(FklVM *, FILE *fp, const FklString *fmt,
                                uint64_t *plen);

FklBuiltinErrorType fklVMformat(FklVM *, FklStringBuffer *buf,
                                const FklString *fmt, uint64_t *plen);

FklBuiltinErrorType fklVMformat2(FklVM *exe, FklStringBuffer *result,
                                 const char *fmt, const char *end,
                                 uint64_t *plen);

void fklVMformatToBuf(FklVM *exe, FklStringBuffer *buf, const char *fmt,
                      FklVMvalue **base, size_t len);

FklString *fklVMformatToString(FklVM *exe, const char *fmt, FklVMvalue **base,
                               size_t len);

FklVMvalue *fklProcessVMnumInc(FklVM *, FklVMvalue *);
FklVMvalue *fklProcessVMnumDec(FklVM *, FklVMvalue *);

int fklProcessVMnumAdd(FklVMvalue *cur, int64_t *pr64, double *pf64,
                       FklBigInt *bi);
int fklProcessVMnumMul(FklVMvalue *cur, int64_t *pr64, double *pf64,
                       FklBigInt *bi);
int fklProcessVMintMul(FklVMvalue *cur, int64_t *pr64, FklBigInt *bi);

FklVMvalue *fklProcessVMnumNeg(FklVM *exe, FklVMvalue *prev);
FklVMvalue *fklProcessVMnumRec(FklVM *exe, FklVMvalue *prev);

FklVMvalue *fklProcessVMnumMod(FklVM *, FklVMvalue *fir, FklVMvalue *sec);

FklVMvalue *fklProcessVMnumAddResult(FklVM *, int64_t r64, double rd,
                                     FklBigInt *bi);
FklVMvalue *fklProcessVMnumMulResult(FklVM *, int64_t r64, double rd,
                                     FklBigInt *bi);
FklVMvalue *fklProcessVMnumSubResult(FklVM *, FklVMvalue *, int64_t r64,
                                     double rd, FklBigInt *bi);
FklVMvalue *fklProcessVMnumDivResult(FklVM *, FklVMvalue *, int64_t r64,
                                     double rd, FklBigInt *bi);

FklVMvalue *fklProcessVMnumIdivResult(FklVM *exe, FklVMvalue *prev, int64_t r64,
                                      FklBigInt *bi);

#define FKL_CHECK_TYPE(V, P, EXE)                                              \
    if (!P(V))                                                                 \
    FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, EXE)
#define FKL_CHECK_REST_ARG(EXE)                                                \
    if (fklResBp((EXE)))                                                       \
    FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, EXE)

FklVMvalue *fklMakeVMint(int64_t r64, FklVM *);
FklVMvalue *fklMakeVMuint(uint64_t r64, FklVM *);
FklVMvalue *fklMakeVMintD(double r64, FklVM *);
int fklIsVMnumber(const FklVMvalue *p);
int fklIsVMint(const FklVMvalue *p);
int fklIsList(const FklVMvalue *p);
int64_t fklVMgetInt(const FklVMvalue *p);
uint64_t fklVMintToHashv(const FklVMvalue *p);
double fklVMgetDouble(const FklVMvalue *p);

int fklHasCircleRef(const FklVMvalue *first_value);

FklVMerrorHandler *fklCreateVMerrorHandler(FklSid_t *typeIds, uint32_t,
                                           FklInstruction *spc, uint64_t cpc);
void fklDestroyVMerrorHandler(FklVMerrorHandler *);
noreturn void fklRaiseVMerror(FklVMvalue *err, FklVM *);

void fklPrintErrBacktrace(FklVMvalue *, FklVM *, FILE *fp);
void fklPrintFrame(FklVMframe *cur, FklVM *exe, FILE *fp);
void fklPrintBacktrace(FklVM *, FILE *fp);

void fklInitMainProcRefs(FklVM *exe, FklVMvalue *proc_obj);

void fklShrinkLocv(FklVM *);
FklVMvalue **fklAllocSpaceForLocalVar(FklVM *, uint32_t);
FklVMvalue **fklAllocMoreSpaceForMainFrame(FklVM *, uint32_t);
void fklUpdateAllVarRef(FklVMframe *, FklVMvalue **);

FklVMframe *fklCreateVMframeWithProcValue(FklVM *exe, FklVMvalue *,
                                          FklVMframe *);

FklVMvalue *fklCreateVMvalueVarRef(FklVM *exe, FklVMvalue **loc, uint32_t idx);
FklVMvalue *fklCreateClosedVMvalueVarRef(FklVM *exe, FklVMvalue *v);

void fklDestroyVMframe(FklVMframe *, FklVM *exe);
FklString *fklGenErrorMessage(FklBuiltinErrorType type);

int fklIsVMhashEq(const FklHashTable *);
int fklIsVMhashEqv(const FklHashTable *);
int fklIsVMhashEqual(const FklHashTable *);
uintptr_t fklGetVMhashTableType(const FklHashTable *);
const char *fklGetVMhashTablePrefix(const FklHashTable *);

uintptr_t fklVMvalueEqHashv(const FklVMvalue *);
uintptr_t fklVMvalueEqvHashv(const FklVMvalue *);
uintptr_t fklVMvalueEqualHashv(const FklVMvalue *v);

FklVMhashTableItem *fklVMhashTableSet(FklVMvalue *key, FklVMvalue *v,
                                      FklHashTable *ht);
FklVMhashTableItem *fklVMhashTableRef1(FklVMvalue *key, FklVMvalue *toSet,
                                       FklHashTable *ht, FklVMgc *);
FklVMhashTableItem *fklVMhashTableRef(FklVMvalue *key, FklHashTable *ht);
FklVMhashTableItem *fklVMhashTableGetItem(FklVMvalue *key, FklHashTable *ht);
FklVMvalue *fklVMhashTableGet(FklVMvalue *key, FklHashTable *ht, int *ok);

void fklAtomicVMhashTable(FklVMvalue *pht, FklVMgc *gc);
void fklAtomicVMuserdata(FklVMvalue *, FklVMgc *);
void fklAtomicVMpair(FklVMvalue *, FklVMgc *);
void fklAtomicVMproc(FklVMvalue *, FklVMgc *);
void fklAtomicVMvec(FklVMvalue *, FklVMgc *);
void fklAtomicVMbox(FklVMvalue *, FklVMgc *);
void fklAtomicVMcproc(FklVMvalue *, FklVMgc *);

FklVMvalue *fklCopyVMlistOrAtom(const FklVMvalue *, FklVM *);
FklVMvalue *fklCopyVMvalue(const FklVMvalue *, FklVM *);

// value creator

FklVMvalue *fklCreateVMvaluePair(FklVM *, FklVMvalue *car, FklVMvalue *cdr);
FklVMvalue *fklCreateVMvaluePairWithCar(FklVM *, FklVMvalue *car);
FklVMvalue *fklCreateVMvaluePairNil(FklVM *);

FklVMvalue *fklCreateVMvalueStr(FklVM *, const FklString *str);
FklVMvalue *fklCreateVMvalueStr2(FklVM *, size_t size, const char *str);
static inline FklVMvalue *fklCreateVMvalueStrFromCstr(FklVM *exe,
                                                      const char *str) {
    return fklCreateVMvalueStr2(exe, strlen(str), str);
}

FklVMvalue *fklCreateVMvalueBvec(FklVM *, const FklBytevector *bvec);
FklVMvalue *fklCreateVMvalueBvec2(FklVM *, size_t size, const uint8_t *);

FklVMvalue *fklCreateVMvalueVec(FklVM *, size_t);
FklVMvalue *fklCreateVMvalueVecWithPtr(FklVM *, size_t, FklVMvalue *const *);
FklVMvalue *fklCreateVMvalueVec3(FklVM *vm, FklVMvalue *a, FklVMvalue *b,
                                 FklVMvalue *c);
FklVMvalue *fklCreateVMvalueVec4(FklVM *vm, FklVMvalue *a, FklVMvalue *b,
                                 FklVMvalue *c, FklVMvalue *d);
FklVMvalue *fklCreateVMvalueVec5(FklVM *vm, FklVMvalue *a, FklVMvalue *b,
                                 FklVMvalue *c, FklVMvalue *d, FklVMvalue *f);
FklVMvalue *fklCreateVMvalueVec6(FklVM *vm, FklVMvalue *a, FklVMvalue *b,
                                 FklVMvalue *c, FklVMvalue *d, FklVMvalue *f,
                                 FklVMvalue *e);

FklVMvalue *fklCreateVMvalueF64(FklVM *, double f64);

FklVMvalue *fklCreateVMvalueProc(FklVM *, FklInstruction *spc, uint64_t cpc,
                                 FklVMvalue *codeObj, uint32_t pid);
FklVMvalue *fklCreateVMvalueProcWithWholeCodeObj(FklVM *, FklVMvalue *codeObj,
                                                 uint32_t pid);
FklVMvalue *fklCreateVMvalueProcWithFrame(FklVM *, FklVMframe *f, size_t,
                                          uint32_t pid);
void fklCreateVMvalueClosureFrom(FklVM *, FklVMvalue **closure, FklVMframe *f,
                                 uint32_t from, FklFuncPrototype *pt);

#ifdef WIN32
#define FKL_DLL_EXPORT __declspec(dllexport)
#else
#define FKL_DLL_EXPORT
#endif

FklVMvalue *fklCreateVMvalueDll(FklVM *, const char *, FklVMvalue **);
int fklIsVMvalueDll(FklVMvalue *v);
void *fklGetAddress(const char *funcname, uv_lib_t *dll);

FklVMvalue *fklCreateVMvalueCproc(FklVM *, FklVMcFunc, FklVMvalue *dll,
                                  FklVMvalue *pd, FklSid_t sid);

void fklDoPrintCprocBacktrace(FklSid_t, FILE *fp, FklVMgc *gc);

FklVMvalue *fklCreateVMvalueFp(FklVM *, FILE *, FklVMfpRW);
int fklIsVMvalueFp(FklVMvalue *v);

FklVMvalue *fklCreateVMvalueHash(FklVM *, FklHashTableEqType);

FklVMvalue *fklCreateVMvalueHashEq(FklVM *);

FklVMvalue *fklCreateVMvalueHashEqv(FklVM *);

FklVMvalue *fklCreateVMvalueHashEqual(FklVM *);

FklVMvalue *fklCreateVMvalueChanl(FklVM *, uint32_t);
int fklIsVMvalueChanl(FklVMvalue *v);

FklVMvalue *fklCreateVMvalueBox(FklVM *, FklVMvalue *);

FklVMvalue *fklCreateVMvalueBoxNil(FklVM *);

FklVMvalue *fklCreateVMvalueError(FklVM *, FklSid_t type, FklString *message);
int fklIsVMvalueError(FklVMvalue *v);

FklVMvalue *fklCreateVMvalueBigInt(FklVM *, size_t num);

FklVMvalue *fklCreateVMvalueBigInt2(FklVM *, const FklBigInt *);
FklVMvalue *fklCreateVMvalueBigInt3(FklVM *, const FklBigInt *, size_t size);
FklVMvalue *fklVMbigIntAdd(FklVM *, const FklVMbigInt *a, const FklVMbigInt *b);
FklVMvalue *fklVMbigIntAddI(FklVM *, const FklVMbigInt *, int64_t);
FklVMvalue *fklVMbigIntSub(FklVM *, const FklVMbigInt *a, const FklVMbigInt *b);
FklVMvalue *fklVMbigIntSubI(FklVM *, const FklVMbigInt *, int64_t);

FklVMvalue *fklCreateVMvalueBigIntWithString(FklVM *exe, const FklString *str,
                                             int base);

FklVMvalue *fklCreateVMvalueBigIntWithDecString(FklVM *exe,
                                                const FklString *str);

FklVMvalue *fklCreateVMvalueBigIntWithOctString(FklVM *exe,
                                                const FklString *str);

FklVMvalue *fklCreateVMvalueBigIntWithHexString(FklVM *exe,
                                                const FklString *str);

FklVMvalue *fklCreateVMvalueBigIntWithI64(FklVM *, int64_t);

FklVMvalue *fklCreateVMvalueBigIntWithU64(FklVM *, uint64_t);

FklVMvalue *fklCreateVMvalueBigIntWithF64(FklVM *, double);

FklVMvalue *fklCreateVMvalueUd(FklVM *, const FklVMudMetaTable *t,
                               FklVMvalue *rel);
FklVMvalue *fklCreateVMvalueUd2(FklVM *, const FklVMudMetaTable *t,
                                size_t extra_size, FklVMvalue *rel);

FklVMvalue *fklCreateVMvalueCodeObj(FklVM *, FklByteCodelnt *bcl);
int fklIsVMvalueCodeObj(FklVMvalue *v);

FklVMvalue *fklCreateVMvalueFromNastNode(FklVM *vm, const FklNastNode *node,
                                         FklLineNumTable *lineHash);

FklNastNode *fklCreateNastNodeFromVMvalue(const FklVMvalue *v, uint64_t curline,
                                          FklLineNumTable *, FklVMgc *gc);

FklVMvalue *fklGetVMvalueEof(void);
int fklIsVMeofUd(FklVMvalue *v);
// value getters

#define FKL_VM_CAR(V) (((FklVMvaluePair *)(V))->pair.car)
#define FKL_VM_CDR(V) (((FklVMvaluePair *)(V))->pair.cdr)

#define FKL_VM_STR(V) (&((FklVMvalueStr *)(V))->str)

#define FKL_VM_BVEC(V) (&((FklVMvalueBvec *)(V))->bvec)

#define FKL_VM_VEC(V) (&(((FklVMvalueVec *)(V))->vec))

#define FKL_VM_VEC_CAS(V, I, O, N)                                             \
    (atomic_compare_exchange_strong(                                           \
        ((atomic_uintptr_t *)&(((FklVMvalueVec *)(V))->vec.base[(I)])),        \
        (uintptr_t *)(O), (uintptr_t)(N)))

#define FKL_VM_F64(V) (((FklVMvalueF64 *)(V))->f64)

#define FKL_VM_PROC(V) (&(((FklVMvalueProc *)(V))->proc))

#define FKL_VM_CPROC(V) (&(((FklVMvalueCproc *)(V))->cproc))

#define FKL_VM_HASH(V) (&(((FklVMvalueHash *)(V))->hash))

#define FKL_VM_BI(V) (&(((FklVMvalueBigInt *)(V))->bi))

#define FKL_VM_UD(V) (&(((FklVMvalueUd *)(V))->ud))

#define FKL_VM_BOX(V) (((FklVMvalueBox *)(V))->box)

#define FKL_VM_BOX_CAS(V, O, N)                                                \
    (atomic_compare_exchange_strong(                                           \
        ((atomic_uintptr_t *)&(((FklVMvalueBox *)(V))->box)),                  \
        (uintptr_t *)(O), (uintptr_t)(N)))

#define FKL_VM_VAR_REF(V) ((FklVMvalueVarRef *)(V))

#define FKL_VM_ERR(V) FKL_GET_UD_DATA(FklVMerror, FKL_VM_UD(V))

#define FKL_VM_CHANL(V) FKL_GET_UD_DATA(FklVMchanl, FKL_VM_UD(V))

#define FKL_VM_DLL(V) FKL_GET_UD_DATA(FklVMdll, FKL_VM_UD(V))

#define FKL_VM_FP(V) FKL_GET_UD_DATA(FklVMfp, FKL_VM_UD(V))

#define FKL_VM_CO(V) FKL_GET_UD_DATA(FklByteCodelnt, FKL_VM_UD(V))

#define FKL_VM_VAR_REF_GET(V) ((atomic_load(&(FKL_VM_VAR_REF(V)->ref))))

// vmparser

void fklVMvaluePushState0ToStack(FklParseStateVector *stateStack);
#define FKL_VMVALUE_PARSE_OUTER_CTX_INIT(EXE)                                  \
    {.maxNonterminalLen = 0,                                                   \
     .line = 1,                                                                \
     .start = NULL,                                                            \
     .cur = NULL,                                                              \
     .create = fklVMvalueTerminalCreate,                                       \
     .destroy = fklVMvalueTerminalDestroy,                                     \
     .ctx = (void *)(EXE)}

void *fklVMvalueTerminalCreate(const char *s, size_t len, size_t line,
                               void *ctx);
void fklVMvalueTerminalDestroy(void *);

void fklAddToGC(FklVMvalue *, FklVM *);
FklVMvalue *fklCreateTrueValue(void);
FklVMvalue *fklCreateNilValue(void);

void fklDropTop(FklVM *s);

#define FKL_VM_GET_ARG_NUM(S) ((S)->tp - (S)->bp)

#define FKL_VM_POP_ARG(S) (((S)->tp > (S)->bp) ? (S)->base[--(S)->tp] : NULL)

#define FKL_VM_GET_TOP_VALUE(S) ((S)->base[(S)->tp - 1])

#define FKL_VM_POP_TOP_VALUE(S) ((S)->base[--(S)->tp])

#define FKL_VM_GET_VALUE(S, N) ((S)->base[(S)->tp - (N)])

#define FKL_VM_PUSH_VALUE(S, V)                                                \
    (((S)->tp >= (S)->last                                                     \
          ? ((S)->last += FKL_VM_STACK_INC_NUM,                                \
             (S)->size += FKL_VM_STACK_INC_SIZE,                               \
             (S)->base = (FklVMvalue **)fklRealloc((S)->base, (S)->size),      \
             (FKL_ASSERT((S)->base)), NULL)                                    \
          : NULL),                                                             \
     (S)->base[(S)->tp++] = (V))

#define FKL_VM_SET_TP_AND_PUSH_VALUE(S, T, V)                                  \
    (((S)->tp = (T) + 1), ((S)->base[(T)] = (V)))

#define FKL_DECL_AND_CHECK_ARG(a, exe)                                         \
    FklVMvalue *a = FKL_VM_POP_ARG(exe);                                       \
    if (!a)                                                                    \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);

#define FKL_DECL_AND_CHECK_ARG2(a, b, exe)                                     \
    FklVMvalue *a = FKL_VM_POP_ARG(exe);                                       \
    FklVMvalue *b = FKL_VM_POP_ARG(exe);                                       \
    if (!b || !a)                                                              \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);

#define FKL_DECL_AND_CHECK_ARG3(a, b, c, exe)                                  \
    FklVMvalue *a = FKL_VM_POP_ARG(exe);                                       \
    FklVMvalue *b = FKL_VM_POP_ARG(exe);                                       \
    FklVMvalue *c = FKL_VM_POP_ARG(exe);                                       \
    if (!c || !b || !a)                                                        \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);

#define FKL_DECL_AND_CHECK_ARG4(a, b, c, d, exe)                               \
    FklVMvalue *a = FKL_VM_POP_ARG(exe);                                       \
    FklVMvalue *b = FKL_VM_POP_ARG(exe);                                       \
    FklVMvalue *c = FKL_VM_POP_ARG(exe);                                       \
    FklVMvalue *d = FKL_VM_POP_ARG(exe);                                       \
    if (!d || !c || !b || !a)                                                  \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);

FklVMvalue *fklPopArg(FklVM *);
FklVMvalue *fklGetTopValue(FklVM *);
FklVMvalue *fklPopTopValue(FklVM *);
FklVMvalue *fklGetValue(FklVM *, uint32_t);
FklVMvalue **fklGetStackSlot(FklVM *, uint32_t);

int fklVMvalueEqual(const FklVMvalue *, const FklVMvalue *);
int fklVMvalueEqv(const FklVMvalue *, const FklVMvalue *);
int fklVMvalueEq(const FklVMvalue *, const FklVMvalue *);
int fklVMvalueCmp(FklVMvalue *, FklVMvalue *, int *);

FklVMfpRW fklGetVMfpRwFromCstr(const char *mode);

int fklVMfpRewind(FklVMfp *vfp, FklStringBuffer *, size_t j);
int fklVMfpEof(FklVMfp *);
int fklVMfpFileno(FklVMfp *);

int fklUninitVMfp(FklVMfp *);

#define FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS                              \
    FklSymbolTable *st, uint32_t *num
typedef FklSid_t *(*FklCodegenDllLibInitExportFunc)(
    FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS);

#define FKL_IMPORT_DLL_INIT_FUNC_ARGS                                          \
    FklVM *exe, FklVMvalue *dll, uint32_t *count
typedef FklVMvalue **(*FklImportDllInitFunc)(FKL_IMPORT_DLL_INIT_FUNC_ARGS);

void fklInitVMdll(FklVMvalue *rel, FklVM *);

uint64_t fklVMchanlRecvqLen(FklVMchanl *ch);
uint64_t fklVMchanlSendqLen(FklVMchanl *ch);
uint64_t fklVMchanlMessageNum(FklVMchanl *ch);
int fklVMchanlFull(FklVMchanl *ch);
int fklVMchanlEmpty(FklVMchanl *ch);

void fklVMsleep(FklVM *, uint64_t ms);

void fklVMread(FklVM *, FILE *fp, FklStringBuffer *buf, uint64_t len, int d);

void fklVMacquireWq(FklVMgc *);
void fklVMreleaseWq(FklVMgc *);
void fklQueueWorkInIdleThread(FklVM *vm, void (*cb)(FklVM *, void *),
                              void *arg);
void fklResumeQueueWorkThread(FklVM *, uv_cond_t *);
void fklResumeQueueIdleThread(FklVM *, uv_cond_t *);

void fklNoticeThreadLock(FklVM *);
void fklDontNoticeThreadLock(FklVM *);
void fklUnlockThread(FklVM *);
void fklLockThread(FklVM *);
void fklSetThreadReadyToExit(FklVM *);
void fklVMstopTheWorld(FklVMgc *);
void fklVMcontinueTheWorld(FklVMgc *);

void fklChanlSend(FklVMchanl *, FklVMvalue *msg, FklVM *);
void fklChanlRecv(FklVMchanl *, FklVMvalue **, FklVM *);
int fklChanlRecvOk(FklVMchanl *, FklVMvalue **);

#define FKL_GET_UD_DATA(TYPE, UD) ((TYPE *)(UD)->data)
#define FKL_DECL_UD_DATA(NAME, TYPE, UD) TYPE *NAME = FKL_GET_UD_DATA(TYPE, UD)
#define FKL_DECL_VM_UD_DATA(NAME, TYPE, UD)                                    \
    TYPE *NAME = FKL_GET_UD_DATA(TYPE, FKL_VM_UD(UD))

int fklIsCallableUd(const FklVMud *);
int fklIsCmpableUd(const FklVMud *);
int fklIsWritableUd(const FklVMud *);
int fklIsAbleToStringUd(const FklVMud *);
int fklIsAbleAsPrincUd(const FklVMud *);
int fklUdHasLength(const FklVMud *);

void fklFinalizeVMud(FklVMud *);
int fklEqualVMud(const FklVMud *, const FklVMud *);
void fklCallVMud(const FklVMud *, const FklVMud *);
int fklCmpVMud(const FklVMud *, const FklVMvalue *, int *);
void fklWriteVMud(const FklVMud *, FILE *fp);
size_t fklLengthVMud(const FklVMud *);
size_t fklHashvVMud(const FklVMud *, FklVMvalueVector *s);
void fklUdAsPrin1(const FklVMud *, FklStringBuffer *, FklVMgc *);
void fklUdAsPrinc(const FklVMud *, FklStringBuffer *, FklVMgc *);
FklBytevector *fklUdToBytevector(const FklVMud *);

int fklIsCallable(FklVMvalue *);
void fklInitVMargs(FklVMgc *gc, int argc, const char *const *argv);

int fklIsVMnumberLt0(const FklVMvalue *);
uint64_t fklVMgetUint(const FklVMvalue *);

FklVMvalue **fklPushVMvalue(FklVM *s, FklVMvalue *v);

void fklVMsetTpAndPushValue(FklVM *exe, uint32_t rtp, FklVMvalue *retval);

size_t fklVMlistLength(FklVMvalue *);

void fklPushVMframe(FklVMframe *, FklVM *exe);
FklVMframe *fklMoveVMframeToTop(FklVM *exe, FklVMframe *f);
void fklInsertTopVMframeAsPrev(FklVM *exe, FklVMframe *f);
FklVMframe *fklCreateOtherObjVMframe(FklVM *exe,
                                     const FklVMframeContextMethodTable *t,
                                     FklVMframe *prev);
FklVMframe *fklCreateNewOtherObjVMframe(const FklVMframeContextMethodTable *t,
                                        FklVMframe *prev);

void fklSwapCompoundFrame(FklVMframe *, FklVMframe *);
unsigned int fklGetCompoundFrameMark(const FklVMframe *);
unsigned int fklSetCompoundFrameMark(FklVMframe *, unsigned int);

FklVMCompoundFrameVarRef *fklGetCompoundFrameLocRef(FklVMframe *f);

FklVMvalue *fklGetCompoundFrameProc(const FklVMframe *);
FklFuncPrototype *fklGetCompoundFrameProcPrototype(const FklVMframe *,
                                                   FklVM *exe);

FklInstruction *fklGetCompoundFrameCode(const FklVMframe *);

FklOpcode fklGetCompoundFrameOp(FklVMframe *frame);

FklVMframe *fklCreateVMframeWithCompoundFrame(const FklVMframe *,
                                              FklVMframe *prev);
FklVMframe *fklCopyVMframe(FklVM *, FklVMframe *, FklVMframe *prev);
void fklDestroyVMframes(FklVMframe *h);

void fklDestroyVMlib(FklVMlib *lib);

void fklInitVMlib(FklVMlib *, FklVMvalue *proc);

void fklInitVMlibWithCodeObj(FklVMlib *, FklVMvalue *codeObj, FklVM *exe,
                             uint32_t protoId);

void fklUninitVMlib(FklVMlib *);

void fklInitBuiltinErrorType(FklSid_t errorTypeId[FKL_BUILTIN_ERR_NUM],
                             FklSymbolTable *table);

#define FKL_RAISE_BUILTIN_ERROR(ERRORTYPE, EXE)                                \
    do {                                                                       \
        FklString *errorMessage = fklGenErrorMessage((ERRORTYPE));             \
        FklVMvalue *err = fklCreateVMvalueError(                               \
            (EXE), (EXE)->gc->builtinErrorTypeId[(ERRORTYPE)], errorMessage);  \
        fklRaiseVMerror(err, (EXE));                                           \
    } while (0)

#define FKL_RAISE_BUILTIN_ERROR_FMT(ERRORTYPE, EXE, FMT, ...)                  \
    {                                                                          \
        FklVMvalue *values[] = {NULL, __VA_ARGS__};                            \
        FklString *errorMessage =                                              \
            fklVMformatToString((EXE), (FMT), &values[1],                      \
                                (sizeof(values) / sizeof(FklVMvalue *)) - 1);  \
        FklVMvalue *err = fklCreateVMvalueError(                               \
            (EXE), (EXE)->gc->builtinErrorTypeId[(ERRORTYPE)], errorMessage);  \
        fklRaiseVMerror(err, (EXE));                                           \
    }

#define FKL_UNUSEDBITNUM (3)
#define FKL_PTR_MASK ((intptr_t)0xFFFFFFFFFFFFFFF8)
#define FKL_TAG_MASK ((intptr_t)0x7)

#define FKL_VM_NIL ((FklVMptr)0x1)
#define FKL_VM_TRUE (FKL_MAKE_VM_FIX(1))
#define FKL_VM_EOF ((FklVMptr)0x7fffffffa)
#define FKL_MAKE_VM_FIX(I)                                                     \
    ((FklVMptr)((((uintptr_t)(I)) << FKL_UNUSEDBITNUM) | FKL_TAG_FIX))
#define FKL_MAKE_VM_CHR(C)                                                     \
    ((FklVMptr)((((uintptr_t)(C)) << FKL_UNUSEDBITNUM) | FKL_TAG_CHR))
#define FKL_MAKE_VM_SYM(S)                                                     \
    ((FklVMptr)((((uintptr_t)(S)) << FKL_UNUSEDBITNUM) | FKL_TAG_SYM))
#define FKL_MAKE_VM_PTR(P) ((FklVMptr)(((uintptr_t)(P)) | FKL_TAG_PTR))
#define FKL_GET_TAG(P) ((FklVMptrTag)(((uintptr_t)(P)) & FKL_TAG_MASK))
#define FKL_GET_PTR(P) ((FklVMptr)(((uintptr_t)(P)) & FKL_PTR_MASK))
#define FKL_GET_FIX(P) ((int64_t)((intptr_t)(P) >> FKL_UNUSEDBITNUM))
#define FKL_GET_CHR(P) ((char)((uintptr_t)(P) >> FKL_UNUSEDBITNUM))
#define FKL_GET_SYM(P) ((FklSid_t)((uintptr_t)(P) >> FKL_UNUSEDBITNUM))
#define FKL_IS_NIL(P) ((P) == FKL_VM_NIL)
#define FKL_IS_PTR(P) (FKL_GET_TAG(P) == FKL_TAG_PTR)
#define FKL_IS_PAIR(P)                                                         \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_PAIR)
#define FKL_IS_F64(P)                                                          \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_F64)
#define FKL_IS_STR(P)                                                          \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_STR)
#define FKL_IS_PROC(P)                                                         \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_PROC)
#define FKL_IS_CPROC(P)                                                        \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_CPROC)
#define FKL_IS_VECTOR(P)                                                       \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_VECTOR)
#define FKL_IS_BYTEVECTOR(P)                                                   \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_BYTEVECTOR)
#define FKL_IS_FIX(P) (FKL_GET_TAG(P) == FKL_TAG_FIX)
#define FKL_IS_CHR(P) (FKL_GET_TAG(P) == FKL_TAG_CHR)
#define FKL_IS_SYM(P) (FKL_GET_TAG(P) == FKL_TAG_SYM)
#define FKL_IS_USERDATA(P)                                                     \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_USERDATA)
#define FKL_IS_BIGINT(P)                                                       \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_BIGINT)
#define FKL_IS_BOX(P)                                                          \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_BOX)
#define FKL_IS_HASHTABLE(P)                                                    \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_HASHTABLE)
#define FKL_IS_HASHTABLE_EQ(P)                                                 \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_HASHTABLE          \
     && fklIsVMhashEq(FKL_VM_HASH(P)))
#define FKL_IS_HASHTABLE_EQV(P)                                                \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_HASHTABLE          \
     && fklIsVMhashEqv(FKL_VM_HASH(P)))
#define FKL_IS_HASHTABLE_EQUAL(P)                                              \
    (FKL_GET_TAG(P) == FKL_TAG_PTR && (P)->type == FKL_TYPE_HASHTABLE          \
     && fklIsVMhashEqual(FKL_VM_HASH(P)))

#define FKL_VM_USER_DATA_DEFAULT_AS_PRINT(NAME, DATA_TYPE_NAME)                \
    static void NAME(const FklVMud *ud, FklStringBuffer *buf, FklVMgc *gc) {   \
        fklStringBufferPrintf(buf, "#<" #DATA_TYPE_NAME " %p>", ud);           \
    }

// inlines
static inline FklBigInt fklVMbigIntToBigInt(const FklVMbigInt *b) {
    const FklBigInt bi = {
        .digits = (FklBigIntDigit *)b->digits,
        .num = b->num,
        .size = labs(b->num),
        .const_size = 1,
    };
    return bi;
}

#define FKL_VM_BIGINT_CALL_1(NAME, FUNC)                                       \
    static inline void NAME(const FklVMbigInt *b) {                            \
        const FklBigInt bi = fklVMbigIntToBigInt(b);                           \
        FUNC(&bi);                                                             \
    }
#define FKL_VM_BIGINT_CALL_1R(NAME, RET, FUNC)                                 \
    static inline RET NAME(const FklVMbigInt *b) {                             \
        const FklBigInt bi = fklVMbigIntToBigInt(b);                           \
        return FUNC(&bi);                                                      \
    }
#define FKL_VM_BIGINT_CALL_2(NAME, ARG2, FUNC)                                 \
    static inline void NAME(const FklVMbigInt *b, ARG2 arg2) {                 \
        const FklBigInt bi = fklVMbigIntToBigInt(b);                           \
        FUNC(&bi, arg2);                                                       \
    }
#define FKL_VM_BIGINT_CALL_2R(NAME, RET, ARG2, FUNC)                           \
    static inline RET NAME(const FklVMbigInt *b, ARG2 arg2) {                  \
        const FklBigInt bi = fklVMbigIntToBigInt(b);                           \
        return FUNC(&bi, arg2);                                                \
    }
#define FKL_VM_CALL_WITH_2_BIR(NAME, RET, FUNC)                                \
    static inline RET NAME(const FklVMbigInt *a, const FklVMbigInt *b) {       \
        const FklBigInt a1 = fklVMbigIntToBigInt(a);                           \
        const FklBigInt b1 = fklVMbigIntToBigInt(b);                           \
        return FUNC(&a1, &b1);                                                 \
    }

#define FKL_VM_CALL_WITH_1_VB_1BI(NAME, FUNC)                                  \
    static inline void NAME(FklBigInt *a, const FklVMbigInt *b) {              \
        const FklBigInt b1 = fklVMbigIntToBigInt(b);                           \
        return FUNC(a, &b1);                                                   \
    }

FKL_VM_BIGINT_CALL_1R(fklVMbigIntToI, int64_t, fklBigIntToI);
FKL_VM_BIGINT_CALL_1R(fklVMbigIntToD, double, fklBigIntToD);
FKL_VM_BIGINT_CALL_1R(fklVMbigIntToU, uint64_t, fklBigIntToU);
FKL_VM_BIGINT_CALL_1R(fklVMbigIntHash, uintptr_t, fklBigIntHash);
FKL_VM_BIGINT_CALL_1R(fklIsVMbigIntLt0, int, fklIsBigIntLt0);
FKL_VM_BIGINT_CALL_1R(fklIsVMbigIntEven, int, fklIsBigIntEven);
FKL_VM_BIGINT_CALL_1R(fklIsVMbigIntOdd, int, fklIsBigIntOdd);
FKL_VM_BIGINT_CALL_1R(fklIsVMbigIntAdd1InFixIntRange, int,
                      fklIsBigIntAdd1InFixIntRange);
FKL_VM_BIGINT_CALL_1R(fklIsVMbigIntSub1InFixIntRange, int,
                      fklIsBigIntSub1InFixIntRange);
FKL_VM_BIGINT_CALL_1R(fklCreateBigIntWithVMbigInt, FklBigInt *, fklCopyBigInt);
FKL_VM_BIGINT_CALL_2(fklPrintVMbigInt, FILE *, fklPrintBigInt);
FKL_VM_CALL_WITH_2_BIR(fklVMbigIntEqual, int, fklBigIntEqual);
FKL_VM_CALL_WITH_2_BIR(fklVMbigIntCmp, int, fklBigIntCmp);
FKL_VM_BIGINT_CALL_2R(fklVMbigIntCmpI, int, int64_t, fklBigIntCmpI);
FKL_VM_CALL_WITH_1_VB_1BI(fklAddVMbigInt, fklAddBigInt);
FKL_VM_CALL_WITH_1_VB_1BI(fklSubVMbigInt, fklSubBigInt);
FKL_VM_CALL_WITH_1_VB_1BI(fklMulVMbigInt, fklMulBigInt);

#undef FKL_VM_BIGINT_CALL_1R
#undef FKL_VM_BIGINT_CALL_1
#undef FKL_VM_BIGINT_CALL_2
#undef FKL_VM_CALL_WITH_2_BI
#undef FKL_VM_CALL_WITH_1_VB_1BI

static inline void fklSetBigIntWithVMbigInt(FklBigInt *a,
                                            const FklVMbigInt *b) {
    const FklBigInt bi = fklVMbigIntToBigInt(b);
    fklSetBigInt(a, &bi);
}

static inline int fklIsVMbigIntDivisible(const FklVMbigInt *a,
                                         const FklBigInt *b) {
    const FklBigInt a0 = fklVMbigIntToBigInt(a);
    return fklIsDivisibleBigInt(&a0, b);
}

static inline int fklIsVMbigIntDivisibleI(const FklVMbigInt *a, int64_t b) {
    const FklBigInt a0 = fklVMbigIntToBigInt(a);
    return fklIsDivisibleBigIntI(&a0, b);
}

static inline FklVMvalue *
fklCreateVMvalueBigIntWithOther(FklVM *exe, const FklVMbigInt *b) {
    const FklBigInt bi = fklVMbigIntToBigInt(b);
    return fklCreateVMvalueBigInt2(exe, &bi);
}

static inline FklVMvalue *fklCreateVMvalueBigIntWithOther2(FklVM *exe,
                                                           const FklVMbigInt *b,
                                                           size_t size) {
    const FklBigInt bi = fklVMbigIntToBigInt(b);
    return fklCreateVMvalueBigInt3(exe, &bi, size);
}

static inline void fklSetBp(FklVM *s) {
    FKL_VM_PUSH_VALUE(s, FKL_MAKE_VM_FIX(s->bp));
    s->bp = s->tp;
}

static inline int fklResBp(FklVM *exe) {
    if (exe->tp > exe->bp)
        return 1;
    exe->bp = FKL_GET_FIX(exe->base[--exe->tp]);
    return 0;
}

static inline uint32_t fklResBpIn(FklVM *exe, uint32_t n) {
    uint32_t rtp = exe->tp - n - 1;
    exe->bp = FKL_GET_FIX(exe->base[rtp]);
    return rtp;
}

static inline void fklAddCompoundFrameCp(FklVMframe *f, int64_t a) {
    f->c.pc += a;
}

static inline FklInstruction *fklGetCompoundFrameEnd(const FklVMframe *f) {
    return f->c.end;
}

static inline FklSid_t fklGetCompoundFrameSid(const FklVMframe *f) {
    return f->c.sid;
}

#ifdef __cplusplus
}
#endif

#endif
