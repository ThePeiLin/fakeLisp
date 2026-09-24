#ifndef FKL_VM_H
#define FKL_VM_H

#include "base.h"
#include "bytecode.h"
#include "common.h"
#include "grammer.h"
#include "parser.h"
#include "symbol.h"
#include "vm_fwd.h"

#include <math.h>
#include <setjmp.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <string.h>

#include <uv.h>

#ifdef __cplusplus
extern "C" {
#endif

struct FklCprocFrameContext;
#define FKL_CPROC_ARGL                                                         \
    FklVM *exe, struct FklCprocFrameContext *ctx, uint32_t argc

typedef int (*FklVMcFunc)(FKL_CPROC_ARGL);
typedef struct FklVMvalue *FklVMptr;
typedef enum {
    FKL_TAG_PTR = 0,
    FKL_TAG_NIL,
    FKL_TAG_FIX,
    FKL_TAG_CHR,
} FklVMptrTag;

#define FKL_PTR_TAG_NUM (FKL_TAG_CHR + 1)

typedef struct FklVMvalue {
    FKL_VM_VALUE_COMMON_HEADER;
} FklVMvalue;

// builtin values
typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    double f64;
} FklVMvalueF64;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    int64_t num;
    FklBigIntDigit digits[1];
} FklVMvalueBigInt;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklString str;
} FklVMvalueStr;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    uint8_t interned;
    FklString str;
} FklVMvalueSym;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklString str;
} FklVMvalueKeyword;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklBytes bytes;
} FklVMvalueBytes;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklVMvalue *car;
    FklVMvalue *cdr;
} FklVMvaluePair;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    size_t size;
    struct FklVMvalue *base[FKL_FLEX_ARRAY_MEMBER];
} FklVMvalueVec;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklVMvalue *box;
} FklVMvalueBox;

// FklValueHashMap
#define FKL_HASH_KEY_TYPE FklVMvalue *
#define FKL_HASH_VAL_TYPE FklVMvalue *
#define FKL_HASH_ELM_NAME Value
#define FKL_HASH_KEY_HASH                                                      \
    {                                                                          \
        fprintf(stderr,                                                        \
                "[%s: %d] calling %s is not allowed!\n",                       \
                __FILE__,                                                      \
                __LINE__,                                                      \
                __func__);                                                     \
        abort();                                                               \
    }
#define FKL_HASH_KEY_EQUAL(A, B)                                               \
    fprintf(stderr,                                                            \
            "[%s: %d] calling %s is not allowed!\n",                           \
            __FILE__,                                                          \
            __LINE__,                                                          \
            __func__),                                                         \
            abort(), 1
#include "cont/hash.h"

typedef enum {
    FKL_HASH_EQ = 0,
    FKL_HASH_EQV,
    FKL_HASH_EQUAL,
} FklHashTableEqType;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklValueHashMap ht;
    FklHashTableEqType eq_type;
} FklVMvalueHash;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    uint32_t idx;
    _Atomic(FklVMvalue **) ref;
    FklVMvalue *v;
} FklVMvalueVarRef;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    const FklIns *spc;
    const FklIns *end;
    FklVMvalue *name;
    FklVMvalue *bcl;

    struct FklVMvalueProto *proto;

    uint32_t local_count;
    uint32_t ref_count;

    FklVMvalue *closure[FKL_FLEX_ARRAY_MEMBER];
} FklVMvalueProc;

typedef struct {
    FKL_VM_VALUE_COMMON_HEADER;
    FklVMcFunc func;
    const char *name;
    FklVMvalue *dll;
} FklVMvalueCproc;

FKL_VM_DEF_UD(FklVMvalueUd, /* no extra members */);
static_assert(sizeof(FklVMvalueUd) % sizeof(int64_t) == 0, "invalid ud size");
static_assert(offsetof(FklVMvalueUd, tp_) == sizeof(FklVMvalue),
        "invalid tp_ offset");
static_assert(alignof(FklVMvalueUd) % sizeof(int64_t) == 0, "invalid ud align");

#define FKL_VM_FP_R_MASK (1)
#define FKL_VM_FP_W_MASK (2)

typedef enum {
    FKL_VM_FP_R = 1,
    FKL_VM_FP_W = 2,
    FKL_VM_FP_RW = 3,
} FklVMfpRW;

FKL_VM_DEF_UD_STRUCT(FklVMvalueFp, {
    FILE *fp;
    FklVMfpRW rw;
});

typedef enum {
    FKL_FRAME_COMPOUND = 0,
    FKL_FRAME_OTHEROBJ,
} FklFrameType;

#define FKL_VM_COMPOUND_FRAME_MARK_RET (0)
#define FKL_VM_COMPOUND_FRAME_MARK_CALL (1)

typedef struct FklCprocFrameContext {
    FklVMvalue *proc;
    FklVMcFunc func;
    FklVMvalue *dll;
    union {
        void *ptr;
        uintptr_t uptr;
        intptr_t iptr;
        struct {
            union {
                uint32_t u32a;
                int32_t i32a;
            };
            union {
                uint32_t u32b;
                int32_t i32b;
            };
        };
    } c[3];
} FklCprocFrameContext;

typedef void (*FklBacktraceCb)(void *data, FklCodeBuilder *build, FklVM *);

typedef struct {
    int (*step)(void *data, FklVM *);
    void (*atomic)(void *data, FklVMgc *);
    void (*finalizer)(void *data);
    FklBacktraceCb print_backtrace;
} FklVMframeContextMethodTable;

struct FklVMframe;

typedef int (*FklVMerrorCallBack)(struct FklVMframe *f, //
        FklVMvalue *ev,
        FklVM *vm);

typedef int (*FklVMretCallBack)(FklVM *vm, struct FklVMframe *f);

typedef struct FklVMframe {
    FklFrameType type;
    uint32_t bp;
    FklVMerrorCallBack errorCallBack;
    FklVMretCallBack ret_cb;
    struct FklVMframe *prev;
    union {
        struct {
            FklVMvalue *proc;
            FklVMvalue *const *konsts;
            FklVMvalueLib *const *libs;
            const FklIns *spc;
            const FklIns *pc;
            const FklIns *end;
            uint32_t arg_num : 24;
            uint32_t mark : 8;
            uint32_t sp;

            FklVMvalue *lref;
            FklVMvalue *lrefl;
            FklVMvalue **ref;
            uint32_t lcount;
            uint32_t rcount;
        };
        struct {
            const FklVMframeContextMethodTable *t;
            uint8_t data[1];
        };
    };
} FklVMframe;

#define FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(TYPE)                                 \
    static_assert(                                                             \
            sizeof(TYPE) <= (sizeof(FklVMframe) - offsetof(FklVMframe, data)), \
            #TYPE " is too big")

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(FklCprocFrameContext);

#define FKL_VM_FRAME_OF(ptr) FKL_CONTAINER_OF(ptr, FklVMframe, data)
#define FKL_VM_VALUE_OF(ptr) FKL_CONTAINER_OF(ptr, FklVMvalue, data)
#define FKL_VM_UDATA_OF(ptr) FKL_CONTAINER_OF(ptr, FklVMud, data)

FKL_API
void fklCallObj(FklVM *exe, FklVMvalue *);

FKL_API
void fklTailCallObj(FklVM *exe, FklVMvalue *);

typedef struct {
    uint32_t bp;
    uint32_t tp;
    FklVMframe *exit_frame;
} FklVMrecoverArgs;

typedef struct {
    int err;
    FklVMvalue *v;
} FklVMcallResult;

FKL_API
void fklCloseVMvalueVarRef(FklVMvalue *ref);

FKL_API
int fklIsClosedVMvalueVarRef(FklVMvalue *ref);

#define FKL_VM_LIB_NONE (0)
#define FKL_VM_LIB_IMPORTING (1)
#define FKL_VM_LIB_IMPORTED (2)
#define FKL_VM_LIB_ERROR (3)

FKL_VM_DEF_UD_STRUCT(FklVMvalueLib, {
    uv_mutex_t lock;

    atomic_int import_state;

    // 大小是 values 实际长度的一半
    uint32_t count;

    uint64_t name_offset;

    FklVMvalue *name;
    FklVMvalue *proc;

    // 前半部分用来存值，后半部分存名字
    FklVMvalue *values[FKL_FLEX_ARRAY_MEMBER];
});

#define FKL_VM_ERR_RAISE (1)

typedef enum {
    FKL_VM_EXIT,
    FKL_VM_READY,
    FKL_VM_RUNNING,
    FKL_VM_WAITING,
} FklVMstate;

#define FKL_VM_STACK_INC_NUM (128)

// FklThreadQueue
#define FKL_QUEUE_ELM_TYPE FklVM *
#define FKL_QUEUE_ELM_TYPE_NAME Thread
#include "cont/queue.h"

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

typedef void (*FklVMinsFunc)(FklVM *exe, const FklIns *ins);

#define FKL_VM_GC_LOCV_CACHE_NUM (8)
#define FKL_VM_GC_LOCV_CACHE_LAST_IDX (FKL_VM_GC_LOCV_CACHE_NUM - 1)

typedef void (*FklVMatExitFunc)(FklVM *, void *);
typedef void (*FklVMatExitMarkFunc)(void *, FklVMgc *);

// FklStrValueHashMap
#define FKL_HASH_KEY_TYPE FklString *
#define FKL_HASH_VAL_TYPE FklVMvalue *
#define FKL_HASH_ELM_NAME StrValue
#define FKL_HASH_KEY_HASH return fklStringHash(*pk);
#define FKL_HASH_KEY_EQUAL(A, B) fklStringEqual(*(A), *(B))
#define FKL_HASH_KEY_UNINIT(K)                                                 \
    {                                                                          \
        fklZfree(*K);                                                          \
        (*K) = NULL;                                                           \
    }
#include "cont/hash.h"

typedef struct FklVM {
    uv_thread_t tid;
    uv_mutex_t lock;
    FklVMvalue *obj_head;
    FklVMvalue *obj_tail;

    int8_t is_single_thread;
    atomic_schar notice_lock;
    uint32_t old_locv_count;
    FklVMlocvList old_locv_cache[FKL_VM_GC_LOCV_CACHE_NUM];
    FklVMlocvList *old_locv_list;

    FklVMstate volatile state;

    // op stack
    uint32_t last;
    uint32_t tp;
    uint32_t bp;
    FklVMvalue **base;

    FklVMframe inplace_frame;
    FklVMframe *top_frame;

    FklVMframe *frame_cache_head;
    FklVMframe **frame_cache_tail;

    struct FklVMvalue *chan;
    FklVMgc *gc;
    struct FklVM *prev;
    struct FklVM *next;
    jmp_buf *buf;

    FklVMinsFunc dummy_ins_func;

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

static FKL_ALWAYS_INLINE uintptr_t fklVMvalueEqHashv(const FklVMvalue *key) {
    return fklHash64Shift(FKL_TYPE_CAST(uintptr_t, key));
}

// FklValueVector
#define FKL_VECTOR_ELM_TYPE FklVMvalue *
#define FKL_VECTOR_ELM_TYPE_NAME Value
#include "cont/vector.h"

// FklValueQueue
#define FKL_QUEUE_ELM_TYPE FklVMvalue *
#define FKL_QUEUE_ELM_TYPE_NAME Value
#include "cont/queue.h"

typedef struct {
    FklVMvalue *car;
    FklVMvalue *cdr;
} FklPair;

// FklPairVector
#define FKL_VECTOR_ELM_TYPE FklPair
#define FKL_VECTOR_ELM_TYPE_NAME Pair
#include "cont/vector.h"

// FklValueHashSet
#define FKL_HASH_KEY_TYPE FklVMvalue const *
#define FKL_HASH_ELM_NAME Value
#define FKL_HASH_KEY_HASH return fklVMvalueEqHashv(*pk);
#include "cont/hash.h"

// FklValueEqHashMap
#define FKL_HASH_KEY_TYPE FklVMvalue *
#define FKL_HASH_VAL_TYPE FklVMvalue *
#define FKL_HASH_ELM_NAME ValueEq
#define FKL_HASH_KEY_HASH return fklVMvalueEqHashv(*pk);
#include "cont/hash.h"

typedef uint32_t FklValueId;

// FKlValueIdHashMap
#define FKL_HASH_KEY_TYPE FklVMvalue const *
#define FKL_HASH_VAL_TYPE FklValueId
#define FKL_HASH_ELM_NAME ValueId
#define FKL_HASH_KEY_HASH return fklVMvalueEqHashv((*pk));
#include "cont/hash.h"

typedef struct {
    FklValueIdHashMap ht;
    FklValueId next_id;
} FklValueTable;

typedef FklVMvalue *(*FklVMudCopyAppendCb)(FklVM *exe,
        const FklVMvalue *ud,
        uint32_t argc,
        FklVMvalue *const *base);

typedef FklVMvalue *(*FklVMudAppendCb)(FklVM *vm,
        FklVMvalue *,
        uint32_t argc,
        FklVMvalue *const *base);

typedef void (*FklVMudPrintCb)(const FklVMvalue *, FklCodeBuilder *, FklVM *);
typedef int (*FklVMudEqualCb)(const FklVMvalue *, const FklVMvalue *);

typedef enum {
    FKL_VM_UD_FINALIZE_NOW = 0,
    FKL_VM_UD_FINALIZE_DELAY
} FklVMudFinalizeResult;

typedef FklVMudFinalizeResult (*FklVMudFinalizer)(FklVMvalue *, FklVMgc *gc);
typedef void (*FklVMudAtomicCb)(const FklVMvalue *, FklVMgc *);

typedef struct FklVMudMetaTable {
    const char *name;
    size_t size;
    FklVMudPrintCb princ;
    FklVMudPrintCb prin1;
    FklVMudFinalizer finalize;
    FklVMudEqualCb equal;
    void (*call)(FklVMvalue *, FklVM *);
    int (*cmp)(const FklVMvalue *, const FklVMvalue *, int *);
    void (*write)(const FklVMvalue *, FklCodeBuilder *);
    FklVMudAtomicCb atomic;
    size_t (*length)(const FklVMvalue *);
    void (*update_weak_ref)(const FklVMvalue *ud, FklVMgc *gc);
    FklVMudCopyAppendCb copy_append;
    FklVMudAppendCb append;
    uintptr_t (*hash)(const FklVMvalue *);
} FklVMudMetaTable;

FKL_VM_DEF_UD_STRUCT(FklVMvalueType, {
    FklVMvalue *dll;
    const void *token;
    FklVMudMetaTable mt;
});

FKL_API
void fklVMtypeCall(FklVMvalue *tp, FklVM *exe);

FKL_API
void fklVMtypePrint(const FklVMvalue *, FklCodeBuilder *, FklVM *);

#ifdef FKL_USING_WIN32
#define FKL_VM_TYPE_TYPE_ATTR alignas(8)
#else
#define FKL_VM_TYPE_TYPE_ATTR alignas(8) const
#endif

extern FKL_API FKL_VM_TYPE_TYPE_ATTR FklVMvalueType FklVMtypeType;

typedef enum {
    FKL_GC_NONE = 0,
    FKL_GC_STW,
    FKL_GC_MARK_ROOT,
    FKL_GC_PROPAGATE,
    FKL_GC_SWEEP,
    FKL_GC_COLLECT,
    FKL_GC_SWEEPING,
    FKL_GC_DONE,
} FklGCstate;

#define FKL_VM_GC_LOCV_CACHE_LEVEL_NUM (5)
#define FKL_VM_GC_THRESHOLD_SIZE (0x4000)

#define FKL_BUILTIN_ERR_MAP                                                    \
    X(FKL_ERR_DUMMY = 0, "dummy")                                              \
    X(FKL_ERR_SYMUNDEFINE, "symbol-error")                                     \
    X(FKL_ERR_SYNTAXERROR, "syntax-error")                                     \
    X(FKL_ERR_INVALIDEXPR, "read-error")                                       \
    X(FKL_ERR_CIRCULARLOAD, "load-error")                                      \
    X(FKL_ERR_INVALIDPATTERN, "pattern-error")                                 \
    X(FKL_ERR_INCORRECT_TYPE_VALUE, "type-error")                              \
    X(FKL_ERR_STACKERROR, "stack-error")                                       \
    X(FKL_ERR_TOOMANYARG, "arg-error")                                         \
    X(FKL_ERR_TOOFEWARG, "arg-error")                                          \
    X(FKL_ERR_CANTCREATETHREAD, "thread-error")                                \
    X(FKL_ERR_THREADERROR, "thread-error")                                     \
    X(FKL_ERR_MACROEXPANDFAILED, "macro-error")                                \
    X(FKL_ERR_CALL_ERROR, "call-error")                                        \
    X(FKL_ERR_LOADDLLFAILD, "load-error")                                      \
    X(FKL_ERR_INVALIDSYMBOL, "symbol-error")                                   \
    X(FKL_ERR_LIBUNDEFINED, "library-error")                                   \
    X(FKL_ERR_UNEXPECTED_EOF, "eof-error")                                     \
    X(FKL_ERR_DIVZEROERROR, "div-zero-error")                                  \
    X(FKL_ERR_FILEFAILURE, "file-error")                                       \
    X(FKL_ERR_INVALID_VALUE, "value-error")                                    \
    X(FKL_ERR_INVALIDASSIGN, "access-error")                                   \
    X(FKL_ERR_INVALIDACCESS, "access-error")                                   \
    X(FKL_ERR_IMPORTFAILED, "import-error")                                    \
    X(FKL_ERR_INVALID_MACRO_PATTERN, "macro-error")                            \
    X(FKL_ERR_FAILD_TO_CREATE_BIGINT_FROM_MEM, "type-error")                   \
    X(FKL_ERR_LIST_DIFFER_IN_LENGTH, "type-error")                             \
    X(FKL_ERR_CROSS_C_CALL_CONTINUATION, "call-error")                         \
    X(FKL_ERR_INVALIDRADIX_FOR_INTEGER, "value-error")                         \
    X(FKL_ERR_NO_VALUE_FOR_KEY, "value-error")                                 \
    X(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, "value-error")                        \
    X(FKL_ERR_UNSERIALIZABLE, "value-error")                                   \
    X(FKL_ERR_UNSUPPORTED_OP, "operation-error")                               \
    X(FKL_ERR_IMPORT_MISSING, "import-error")                                  \
    X(FKL_ERR_EXPORT_ERROR, "export-error")                                    \
    X(FKL_ERR_IMPORT_READER_MACRO_ERROR, "import-error")                       \
    X(FKL_ERR_ANALYSIS_TABLE_GENERATE_FAILED, "grammer-error")                 \
    X(FKL_ERR_REGEX_COMPILE_FAILED, "grammer-error")                           \
    X(FKL_ERR_GRAMMER_CREATE_FAILED, "grammer-error")                          \
    X(FKL_ERR_INVALIDRADIX_FOR_FLOAT, "value-error")                           \
    X(FKL_ERR_ASSIGN_CONSTANT, "symbol-error")                                 \
    X(FKL_ERR_REDEFINE_VARIABLE_AS_CONSTANT, "symbol-error")                   \
    X(FKL_ERR_EXP_HAS_NO_VALUE, "syntax-error")                                \
    X(FKL_ERR_UNRESOLVED_NOMTERM, "grammer-error")                             \
    X(FKL_ERR_REDUCE_CONFLICT, "grammer-error")

typedef enum {
#define X(A, B) A,
    FKL_BUILTIN_ERR_MAP
#undef X
            FKL_BUILTIN_ERR_NUM,
} FklBuiltinErrorType;

typedef enum {
    FKL_INT_DONE = 0,
    FKL_INT_NEXT,
} FklVMinterruptResult;

typedef struct FklVMextraMarkArgs FklVMextraMarkArgs;

typedef FklVMinterruptResult (*FklVMinterruptHandler)(FklVM *exe,
        FklVMvalue *value,
        FklVMvalue **pvalue,
        void *);

typedef void (*FklVMextraMarkFunc)(FklVMgc *, FklVMextraMarkArgs *arg);

typedef struct {
    FklVMextraMarkFunc func;
    void (*finalizer)(FklVMextraMarkArgs *);
} FklVMextraMarkItem;

// FklVMextraMarkHashMap
#define FKL_HASH_KEY_TYPE FklVMextraMarkArgs *
#define FKL_HASH_VAL_TYPE FklVMextraMarkItem
#define FKL_HASH_ELM_NAME VMextraMark
#define FKL_HASH_KEY_HASH return fklPtrHash(*pk);
#include "cont/hash.h"

typedef struct FklVMgc {
    _Atomic(FklGCstate) volatile running;
    atomic_size_t alloced_size;
    size_t threshold;
    FklVMvalue *head;

    int argc;
    char **argv;

    FklVMvalue *gray_list;
    FklVMvalue *weak_refs;

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

    struct FklVMvalueObarray *obarray;

    struct FklVMvalueObarray *keywords;

    FklVMvalue *builtinErrorTypeId[FKL_BUILTIN_ERR_NUM];

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
        void (*finalizer)(FklVMextraMarkArgs *);
        FklVMextraMarkArgs *int_handle_arg;
    } *int_list;

    uv_mutex_t extra_mark_lock;

    FklVMextraMarkHashMap extra_marks;

    FklVMvalue **builtin_refs;

    FklCodeBuilder err_out;
    uv_mutex_t print_backtrace_lock;

    FklVMvalue *seek_set;
    FklVMvalue *seek_cur;
    FklVMvalue *seek_end;

    FklVMvalue *sym_quote;
    FklVMvalue *sym_unquote;
    FklVMvalue *sym_qsquote;
    FklVMvalue *sym_unqtesp;

    FklVMvalue *concat_s;
    FklVMvalue *keyword_k;
    FklVMvalue *regex_k;
    FklVMvalue *ignore_k;
    FklVMvalue *delim_k;

    FklVMvalue *path_vec;

    // only for create objects before idle loop start
    FklVM gcvm;
} FklVMgc;

FKL_VM_DEF_UD_STRUCT(FklVMvalueError, {
    FklVMvalue *type;
    FklVMvalue *message;
});

typedef struct FklVMchanlSend {
    struct FklVMchanlSend *next;
    uv_cond_t cond;
    struct FklVMvalue *msg;
} FklVMchanlSend;

typedef struct FklVMchanlRecv {
    struct FklVMchanlRecv *next;
    uv_cond_t cond;
    FklVM *exe;
    uint32_t slot;
} FklVMchanlRecv;

FKL_VM_DEF_UD_STRUCT(FklVMvalueChanl, {
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
    struct FklVMvalue *buf[FKL_FLEX_ARRAY_MEMBER];
});

FKL_VM_DEF_UD_STRUCT(FklVMvalueCodeObj, { FklByteCodelnt bcl; });

typedef struct {
    size_t size;
    FklVMudAtomicCb atomic;
    FklVMudFinalizer finalizer;
} FklDllStateDesc;

#define FKL_VM_DEF_DLL(NAME, ...)                                              \
    FKL_VM_DEF_UD_STRUCT(NAME, {                                               \
        alignas(8) const FklDllStateDesc *desc;                                \
        uv_lib_t dll;                                                          \
        __VA_ARGS__                                                            \
    })

#define FKL_VM_DEF_DLL_STRUCT(NAME, ...)                                       \
    FKL_VM_DEF_DLL(NAME, struct __VA_ARGS__;)

FKL_VM_DEF_DLL(FklVMvalueDll, /* no extra members */);
static_assert(sizeof(FklVMvalueDll) % sizeof(int64_t) == 0, "invalid dll size");
static_assert(alignof(FklVMvalueDll) % sizeof(int64_t) == 0,
        "invalid dll align");
static_assert(offsetof(FklVMvalueDll, desc) == sizeof(FklVMvalueUd),
        "invalid dll desc offset");

typedef enum {
    FKL_WEAK_MAP_V = 1,
    FKL_WEAK_MAP_K = 2,

    FKL_WEAK_MAP_KV = (FKL_WEAK_MAP_K | FKL_WEAK_MAP_V),
} FklWeakMapMode;

FKL_VM_DEF_UD_STRUCT(FklVMvalueWeakHashEq, {
    FklValueEqHashMap ht;
    FklWeakMapMode weak_mode;
});

FKL_VM_DEF_UD_STRUCT(FklVMvalueObarray, {
    uv_mutex_t lock;
    FklStrValueHashMap map;
});

FKL_API void fklPopVMframe(FklVM *);
FKL_API void fklPopVMframe2(FklVM *, FklVMframe *const bottom_frame);

FKL_API int fklRunVM(FklVM *exe, FklVMframe *const exit_frame);

// same as fklRunVM, but in single thread
FKL_API int fklRunVM2(FklVM *exe, FklVMframe *const exit_frame);

typedef int (*FklRunVMcb)(FklVM *exe, FklVMframe *const exit_frame);
typedef void (*FklVMcallbackValueCreator)(FklVM *exe, void *arg);

FKL_API
FklVMcallResult fklVMcall(FklVM *exe,
        FklVMrecoverArgs *re,
        FklVMvalue *proc,
        size_t count,
        FklVMvalue *values[]);

// 类似fklVMcall，但是被调用函数和参数已经入栈
FKL_API
FklVMcallResult fklVMcall0(FklRunVMcb, FklVM *exe, FklVMrecoverArgs *re);

FKL_API
FklVMcallResult fklVMcall2(FklRunVMcb,
        FklVM *exe,
        FklVMrecoverArgs *re,
        FklVMvalue *proc,
        size_t count,
        FklVMvalue *values[]);

FKL_API
FklVMcallResult fklVMcall3(FklRunVMcb,
        FklVM *exe,
        FklVMrecoverArgs *re,
        FklVMvalue *proc,
        FklVMcallbackValueCreator creator,
        void *args);

FKL_API void fklVMsetRecover(FklVM *exe, FklVMrecoverArgs *args);
FKL_API void fklVMrecover(FklVM *exe, const FklVMrecoverArgs *args);

FKL_API int fklRunVMidleLoop(FklVM *volatile);

FKL_API
void fklVMatExit(FklVM *vm,
        FklVMatExitFunc func,
        FklVMatExitMarkFunc mark,
        void (*finalizer)(void *),
        void *arg);

FKL_API void fklVMidleLoop(FklVMgc *gc);

FKL_API
FklVMinterruptResult fklVMinterrupt(FklVM *, FklVMvalue *v, FklVMvalue **pv);

FKL_API
void fklDestroyVMinterruptHandlerList(struct FklVMinterruptHandleList *l);

FKL_API
void fklVMpushInterruptHandler(FklVMgc *,
        FklVMinterruptHandler,
        FklVMextraMarkFunc,
        void (*finalizer)(FklVMextraMarkArgs *),
        FklVMextraMarkArgs *);

FKL_API
void fklVMpushInterruptHandlerLocal(FklVM *,
        FklVMinterruptHandler,
        FklVMextraMarkFunc,
        void (*finalizer)(FklVMextraMarkArgs *),
        FklVMextraMarkArgs *);

FKL_API
void fklVMregisterExtraMarkFunc(FklVMgc *,
        FklVMextraMarkArgs *,
        FklVMextraMarkFunc,
        void (*finalizer)(FklVMextraMarkArgs *));

FKL_API void fklVMunregisterExtraMarkFunc(FklVMgc *, FklVMextraMarkArgs *);

FKL_API void fklVMclearExtraMarkFunc(FklVMgc *gc);

FKL_API
void fklVMexecuteInstruction(FklVM *exe,
        FklOpcode op,
        const FklIns *ins,
        FklVMframe *frame);

// check and gc in single thread

FKL_API void fklVMgcCheck(FklVM *exe, int forced);

static FKL_ALWAYS_INLINE void fklVMgcStateSet(FklVMgc *gc, FklGCstate state) {
    atomic_store(&gc->running, state);
}

static FKL_ALWAYS_INLINE FklGCstate fklVMgcStateGet(FklVMgc *gc) {
    return atomic_load(&gc->running);
}

static FKL_ALWAYS_INLINE void fklVMgcCheckPoint(FklVM *exe, int forced) {
    if (exe == &exe->gc->gcvm || exe->is_single_thread) {
        fklVMgcCheck(exe, forced);
        return;
    }

    if (!atomic_load(&exe->notice_lock))
        return;

    uv_mutex_unlock(&exe->lock);

    while (fklVMgcStateGet(exe->gc) == FKL_GC_STW)
        uv_sleep(0);

    uv_mutex_lock(&exe->lock);
}

FKL_API void fklVMthreadStart(FklVM *, FklVMqueue *q);

FKL_API
FklVM *fklCreateVMwithByteCode(FklVMvalue *,
        FklVMgc *gc,
        FklVMvalueProto *pt,
        uint64_t spc);

FKL_API
FklVM *fklCreateVMwithByteCode2(FklVMvalue *,
        FklVMgc *gc,
        FklVMvalueProto *proto,
        uint64_t spc);

FKL_API FklVM *fklCreateVM(FklVMvalue *proc, FklVMgc *gc);

FklVM *fklCreateThreadVM(FklVMvalue *,
        uint32_t arg_num,
        FklVMvalue *const *args,
        FklVM *prev,
        FklVM *next);

FKL_API void fklMoveThreadObjectsToGc(FklVM *vm, FklVMgc *gc);

FKL_API void fklVMstackShrink(FklVM *);
FKL_API int fklCreateCreateThread(FklVM *);

static inline uint32_t fklVMgcComputeLocvLevelIdx(uint32_t llast) {
    uint32_t l = (llast / FKL_VM_STACK_INC_NUM) - 1;
    if (l >= 8)
        return 4;
    else if (l & 0x4)
        return 3;
    else if (l & 0x2)
        return 2;
    else if (l & 0x1)
        return 1;
    else
        return 0;
}

FKL_API FklVMvalueObarray *fklCreateVMvalueObarray(FklVM *);

FKL_API void fklInitVMgc(FklVMgc *);
FKL_API FklVMgc *fklCreateVMgc(void);
FKL_API FklVMvalue *fklSetVMgcPath(FklVMgc *, FklVMvalue *path_vec);

FKL_API
FklVMvalue **
fklAllocLocalVarSpaceFromGC(FklVMgc *, uint32_t llast, uint32_t *pllast);

FKL_API
FklVMvalue **fklAllocLocalVarSpaceFromGCwithoutLock(FklVMgc *,
        uint32_t llast,
        uint32_t *pllast);

FKL_API FklVMvalue *fklVMaddSymbol(FklVM *, const FklString *str);
FKL_API FklVMvalue *fklVMaddSymbolCstr(FklVM *, const char *str);
FKL_API FklVMvalue *fklVMaddSymbolCharBuf(FklVM *, const char *str, size_t);
FKL_API FklVMvalue *fklVMaddSymbolValue(FklVM *, FklVMvalue *s);

FKL_API FklVMvalue *fklVMaddKeyword(FklVM *, const FklString *str);
FKL_API FklVMvalue *fklVMaddKeywordCstr(FklVM *, const char *str);
FKL_API FklVMvalue *fklVMaddKeywordCharBuf(FklVM *, const char *str, size_t);

FKL_API int fklVMhasSymbol(FklVM *v, const FklString *str);
FKL_API int fklVMhasSymbol1(FklVM *v, const char *str);
FKL_API int fklVMhasSymbol2(FklVM *v, const char *str, size_t);

FKL_API void fklInitValueTable(FklValueTable *t);
FKL_API void fklUninitValueTable(FklValueTable *t);
FKL_API FklValueId fklValueTableAdd(FklValueTable *t, const FklVMvalue *v);

FKL_API
FklValueId fklValueTableGet(const FklValueTable *t, const FklVMvalue *v);

FKL_API void fklValueTableClear(FklValueTable *t);

FKL_API
void fklTraverseSerializableValue(FklValueTable *t, const FklVMvalue *v);

FKL_API
void fklVMgcAddLocvCache(FklVMgc *gc, uint32_t llast, FklVMvalue **locv);

FKL_API void fklVMgcMoveLocvCache(FklVM *vm, FklVMgc *gc);
FKL_API void fklVMgcMarkAllRootToGray(FklVM *curVM);
FKL_API void fklVMgcUpdateWeakRefs(FklVMgc *gc);
FKL_API int fklVMgcPropagate(FklVMgc *gc);
FKL_API void fklVMgcCollect(FklVMgc *gc, FklVMvalue **pw);
FKL_API void fklVMgcSweep(FklVMgc *gc, FklVMvalue *);
FKL_API void fklVMgcUpdateThreshold(FklVMgc *);

FKL_API void fklVMgcMarkCodeObject(FklVMgc *, const FklByteCodelnt *bc);

typedef void (*FklVMgrammerProdMarker)(FklVMgc *gc, void *ctx);
#define FKL_VM_GRAMMER_CTX_MARKER_NONE ((FklVMgrammerProdMarker)UINTPTR_MAX)

FKL_API
void fklVMgcMarkGrammerProd(FklVMgc *gc,
        const FklGrammerProduction *prod,
        FklVMgrammerProdMarker ctx_atomic);

FKL_API
void fklVMgcMarkGrammer(FklVMgc *,
        const FklGrammer *g,
        FklVMgrammerProdMarker ctx_atomic);

FKL_API void fklUninitVMgc(FklVMgc *);
FKL_API void fklDestroyVMgc(FklVMgc *);

FKL_API void fklDestroyAllVMs(FklVM *cur);
FKL_API void fklDeleteCallChain(FklVM *);

FKL_API
void fklDBG_printVMstack(FklVM *,
        uint32_t c,
        FklCodeBuilder *,
        int,
        FklVM *exe);

FKL_API
void fklDBG_printLinkBacktrace(FklVMframe *t, FklCodeBuilder *, FklVM *exe);

FKL_API FklVMvalue *fklVMstringify(FklVMvalue *, FklVM *, char mode);

FKL_API void fklPrin1VMvalue(FklVMvalue *, FILE *, FklVM *vm);
FKL_API void fklPrin1VMvalue2(FklVMvalue *, FklCodeBuilder *, FklVM *vm);

FKL_API void fklPrincVMvalue(FklVMvalue *, FILE *, FklVM *vm);
FKL_API void fklPrincVMvalue2(FklVMvalue *, FklCodeBuilder *, FklVM *vm);

FKL_API
FklBuiltinErrorType fklVMformat(FklVM *,
        FklCodeBuilder *buf,
        const char *fmt,
        uint64_t *plen,
        size_t value_count,
        FklVMvalue *const values[]);

FKL_API
FklBuiltinErrorType fklVMformat2(FklVM *,
        FklCodeBuilder *buf,
        const FklString *fmt,
        uint64_t *plen,
        size_t value_count,
        FklVMvalue *const values[]);

FKL_API
FklBuiltinErrorType fklVMformat3(FklVM *,
        FklCodeBuilder *result,
        size_t fmt_len,
        const char *fmt,
        uint64_t *plen,
        size_t value_count,
        FklVMvalue *const values[]);

FKL_API
FklVMvalue *fklVMformatToString(FklVM *exe,
        const char *fmt,
        size_t len,
        FklVMvalue *const base[]);

FKL_API FklVMvalue *fklProcessVMnumAddk(FklVM *, FklVMvalue *, int8_t);

FKL_API
int fklProcessVMnumAdd(FklVMvalue *cur,
        int64_t *pr64,
        double *pf64,
        FklBigInt *bi);

FKL_API
int fklProcessVMnumMul(FklVMvalue *cur,
        int64_t *pr64,
        double *pf64,
        FklBigInt *bi);

FKL_API
int fklProcessVMintMul(FklVMvalue *cur, int64_t *pr64, FklBigInt *bi);

FKL_API FklVMvalue *fklProcessVMnumNeg(FklVM *exe, FklVMvalue *prev);
FKL_API FklVMvalue *fklProcessVMnumRec(FklVM *exe, FklVMvalue *prev);

FKL_API
FklVMvalue *fklProcessVMnumMod(FklVM *, FklVMvalue *fir, FklVMvalue *sec);

FKL_API
FklVMvalue *
fklProcessVMnumAddResult(FklVM *, int64_t r64, double rd, FklBigInt *bi);

FKL_API
FklVMvalue *
fklProcessVMnumMulResult(FklVM *, int64_t r64, double rd, FklBigInt *bi);

FKL_API
FklVMvalue *fklProcessVMnumSubResult(FklVM *,
        FklVMvalue *,
        int64_t r64,
        double rd,
        FklBigInt *bi);

FKL_API
FklVMvalue *fklProcessVMnumDivResult(FklVM *,
        FklVMvalue *,
        int64_t r64,
        double rd,
        FklBigInt *bi);

FKL_API
FklVMvalue *fklProcessVMnumIdivResult(FklVM *exe,
        FklVMvalue *prev,
        int64_t r64,
        FklBigInt *bi);

#define FKL_CHECK_TYPE(V, P, EXE)                                              \
    do {                                                                       \
        if (!P(V))                                                             \
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, EXE);        \
    } while (0)

#define FKL_CHECK_RANGE_I(V, EXE, FROM, TO)                                    \
    do {                                                                       \
        if (!fklVMintegerInRangeI(V, FROM, TO)) {                              \
            FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_INVALID_VALUE,                 \
                    (EXE),                                                     \
                    "Expect value in [" #FROM ", " #TO "], but got %S",        \
                    (V));                                                      \
        }                                                                      \
    } while (0)

#define FKL_CHECK_RANGE_U(V, EXE, FROM, TO)                                    \
    do {                                                                       \
        if (!fklVMintegerInRangeU(V, FROM, TO)) {                              \
            FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_INVALID_VALUE,                 \
                    (EXE),                                                     \
                    "Expect value in [" #FROM ", " #TO "], but got %S",        \
                    (V));                                                      \
        }                                                                      \
    } while (0)

#define FKL_CHECK_BYTE_RANGE(V, EXE) FKL_CHECK_RANGE_I(V, EXE, -128, 255)

#define FKL_CHECK_BYTE_RANGE_AT(V, AT, EXE)                                    \
    do {                                                                       \
        if (!fklVMintegerInRangeI(V, -128, 255)) {                             \
            FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_INVALID_VALUE,                 \
                    (EXE),                                                     \
                    "Expect value in [-128, 255], but got %S at %S",           \
                    (V),                                                       \
                    fklMakeVMintU((EXE), (AT)));                               \
        }                                                                      \
    } while (0)

#define FKL_CPROC_GET_ARG_NUM(S, CTX) ((S)->tp - FKL_VM_FRAME_OF(CTX)->bp - 1)

#define FKL_CPROC_CHECK_ARG_NUM(EXE, NUM, N)                                   \
    if (NUM > (N)) {                                                           \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, EXE);                      \
    } else if ((FKL_TYPE_CAST(int64_t, (N)) - FKL_TYPE_CAST(int64_t, NUM))     \
               > 0) {                                                          \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, EXE);                       \
    }

#define FKL_CPROC_CHECK_ARG_NUM2(EXE, NUM, MIN, MAX)                           \
    if ((NUM) > (MAX)) {                                                       \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, (EXE));                    \
    } else if (((NUM) + 1) < ((MIN) + 1)) {                                    \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, (EXE));                     \
    }

#define FKL_CPROC_GET_ARG(EXE, CTX, I)                                         \
    FKL_VM_GET_ARG((EXE), FKL_VM_FRAME_OF(CTX), (I))

#define FKL_CPROC_RETURN(EXE, CTX, V)                                          \
    do {                                                                       \
        (EXE)->bp =                                                            \
                (uint32_t)FKL_GET_FIX(FKL_CPROC_GET_ARG((EXE), (CTX), -2));    \
        (EXE)->tp = FKL_VM_FRAME_OF(CTX)->bp;                                  \
        FKL_VM_GET_TOP_VALUE((EXE)) = (V);                                     \
    } while (0)

FKL_API int fklIsList(const FklVMvalue *p);
FKL_API int fklIsList2(const FklVMvalue *p, size_t *len);
FKL_API uint64_t fklVMintToHashv(const FklVMvalue *p);
FKL_API double fklVMgetDouble(const FklVMvalue *p);

FKL_API int fklHasCircleRef(const FklVMvalue *first_value);

FKL_API
int fklIsSerializableToByteCodeFile(const FklVMvalue *first_value,
        FklVMvalueLnt *lnt,
        uint64_t line);

FKL_API
noreturn void fklRaiseVMerror(FklVMvalue *err, FklVM *);

FKL_API void fklPrintErrBacktrace(FklVMvalue *, FklVM *, FklCodeBuilder *fp);
FKL_API void fklPrintIntruptInfo(FklVMvalue *, FklVM *, FklCodeBuilder *fp);

FKL_API
void fklPrintFrame(const FklVMframe *cur, FklVM *exe, FklCodeBuilder *fp);

FKL_API void fklPrintBacktrace(FklVM *, FklCodeBuilder *fp);

FKL_API void fklInitMainProcRefs(FklVM *exe, FklVMvalue *proc_obj);

FKL_API FklVMframe *fklCreateVMframeWithProc(FklVM *exe, FklVMvalue *);

FKL_API
void fklInitClosedVMvalueVarRef(FklVMvalueVarRef *ref, FklVMvalue *v);

FKL_API
FklVMvalue *fklCreateVMvalueVarRef(FklVM *exe, FklVMframe *f, uint32_t idx);

FKL_API
FklVMvalue *fklCreateClosedVMvalueVarRef(FklVM *exe, FklVMvalue *v);

FKL_API void fklDestroyVMframe(FklVMframe *, FklVM *exe);
FKL_API FklVMvalue *fklGenErrorMessage(FklBuiltinErrorType type, FklVM *exe);

FKL_API const char *fklGetVMhashTablePrefix(const FklVMvalueHash *);

FKL_API
int fklVMhashTableDel(FklVMvalueHash *ht,
        FklVMvalue *key,
        FklVMvalue **pv,
        FklVMvalue **pk);

FKL_API
FklValueHashMapElm *
fklVMhashTableSet(FklVMvalueHash *ht, FklVMvalue *key, FklVMvalue *v);

FKL_API
FklValueHashMapElm *
fklVMhashTableRef1(FklVMvalueHash *ht, FklVMvalue *key, FklVMvalue *v);

FKL_API
FklValueHashMapElm *fklVMhashTableGet(const FklVMvalueHash *, FklVMvalue *key);

FKL_API void fklAtomicVMhashTable(const FklVMvalue *pht, FklVMgc *gc);
FKL_API void fklAtomicVMuserdata(const FklVMvalue *, FklVMgc *);
FKL_API void fklAtomicVMpair(const FklVMvalue *, FklVMgc *);
FKL_API void fklAtomicVMproc(const FklVMvalue *, FklVMgc *);
FKL_API void fklAtomicVMvec(const FklVMvalue *, FklVMgc *);
FKL_API void fklAtomicVMbox(const FklVMvalue *, FklVMgc *);
FKL_API void fklAtomicVMcproc(const FklVMvalue *, FklVMgc *);
FKL_API void fklAtomicVMtype(const FklVMvalue *ud, FklVMgc *gc);

FKL_API FklVMvalue *fklCloneVMlist(FklVM *vm, const FklVMvalue *v);
FKL_API FklVMvalue *fklCopyVMlist(FklVM *vm, const FklVMvalue *);
FKL_API FklVMvalue **fklCopyVMlist1(FklVM *vm, FklVMvalue **);
FKL_API FklVMvalue *fklCopyVMvalue(FklVM *vm, const FklVMvalue *);

FKL_API
FklVMvalue *fklAppendVMvalue(FklVM *vm,
        const FklVMvalue *v,
        uint32_t argc,
        FklVMvalue *const *base);

FKL_API
FklVMvalue *fklAppendVMvalue1(FklVM *vm,
        FklVMvalue *v,
        uint32_t argc,
        FklVMvalue *const *base);

// value creator

FKL_API
FklVMvalue *fklCreateVMvaluePair(FklVM *, FklVMvalue *car, FklVMvalue *cdr);

FKL_API FklVMvalue *fklCreateVMvaluePair1(FklVM *, FklVMvalue *car);
FKL_API FklVMvalue *fklCreateVMvaluePairNil(FklVM *);

FKL_API FklVMvalue *fklCreateVMvalueStr(FklVM *, const FklString *str);
FKL_API FklVMvalue *fklCreateVMvalueStr2(FklVM *, size_t size, const char *str);

static inline FklVMvalue *fklCreateVMvalueStr1(FklVM *exe, const char *str) {
    return fklCreateVMvalueStr2(exe, strlen(str), str);
}

FKL_API FklVMvalue *fklCreateVMvalueSym(FklVM *, const FklString *str);
FKL_API
FklVMvalue *fklCreateVMvalueSym2(FklVM *, size_t size, const char *str);

// TODO: rename it from FromCstr to Cstr
static inline FklVMvalue *fklCreateVMvalueSymFromCstr(FklVM *exe,
        const char *str) {
    return fklCreateVMvalueSym2(exe, strlen(str), str);
}

FKL_API
FklVMvalue *fklCreateVMvalueKeyword(FklVM *, size_t size, const char *str);

FKL_API FklVMvalue *fklCreateVMvalueBytes(FklVM *, const FklBytes *bytes);

FKL_API
FklVMvalue *fklCreateVMvalueBytes2(FklVM *, size_t size, const uint8_t *);

FKL_API FklVMvalue *fklCreateVMvalueVec(FklVM *, size_t);
FKL_API FklVMvalue *fklCreateVMvalueVec2(FklVM *, size_t, FklVMvalue *const *);
FKL_API FklVMvalue *fklCreateVMvalueVecExt(FklVM *, size_t, ...);

#define FKL_VM_F64_STATIC_INIT(F64)                                            \
    ((FklVMvalueF64){                                                          \
        .next_ = NULL,                                                         \
        .gray_next_ = NULL,                                                    \
        .mark_ = FKL_MARK_B,                                                   \
        .type_ = FKL_TYPE_F64,                                                 \
        .f64 = (F64),                                                          \
    })

FKL_API FklVMvalue *fklCreateVMvalueF64(FklVM *, double f64);

FKL_API
uint32_t fklVMfetchVarRef(FklVM *exe, FklVMvalueProc *proc, FklVMframe *f);

FKL_API
FklVMvalue *
fklCreateVMvalueProc(FklVM *, FklVMvalue *codeObj, FklVMvalueProto *pt);

FKL_API
FklVMvalue *fklCreateVMvalueProc2(FklVM *,
        const FklIns *spc,
        uint64_t cpc,
        FklVMvalue *codeObj,
        FklVMvalueProto *pt);

FKL_API
FklVMvalue *fklCreateVMvalueProc3(FklVM *,
        FklVMframe *f,
        size_t,
        FklVMvalueProto *child_proto);

FKL_API
FklVMvalue *
fklCreateVMvalueDll(FklVM *vm, FklVMvalue *realpath, FklVMvalue **error_msg);

FKL_API int fklIsVMvalueDll(const FklVMvalue *v);
FKL_API void *fklGetAddress(const char *funcname, uv_lib_t *dll);

#define FKL_VM_CPROC_STATIC_INIT(NAME, FUNC)                                   \
    ((FklVMvalueCproc){                                                        \
        .next_ = NULL,                                                         \
        .gray_next_ = NULL,                                                    \
        .mark_ = FKL_MARK_B,                                                   \
        .type_ = FKL_TYPE_CPROC,                                               \
        .func = (FUNC),                                                        \
        .name = (NAME),                                                        \
        .dll = NULL,                                                           \
    })

FKL_API
FklVMvalue *
fklCreateVMvalueCproc(FklVM *, FklVMcFunc, FklVMvalue *dll, const char *name);

FKL_API
void fklPrintCprocBacktrace(const char *name, FklCodeBuilder *build);

FKL_API void fklInitVMvalueFp(FklVMvalueFp *vfp, FILE *fp, FklVMfpRW rw);
FKL_API FklVMvalue *fklCreateVMvalueFp(FklVM *, FILE *, FklVMfpRW);
FKL_API int fklIsVMvalueFp(const FklVMvalue *v);

FKL_API FklVMvalue *fklCreateVMvalueHash(FklVM *, FklHashTableEqType);

FKL_API FklVMvalue *fklCreateVMvalueHashEq(FklVM *);

FKL_API FklVMvalue *fklCreateVMvalueHashEqv(FklVM *);

FKL_API FklVMvalue *fklCreateVMvalueHashEqual(FklVM *);

FKL_API int fklIsVMvalueWeakHashEq(const FklVMvalue *v);

static FKL_ALWAYS_INLINE FklVMvalueWeakHashEq *fklVMvalueWeakHashEq(
        const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueWeakHashEq(v));
    return FKL_TYPE_CAST(FklVMvalueWeakHashEq *, v);
}

FKL_API FklVMvalueWeakHashEq *fklCreateVMvalueWeakHashEq(FklVM *exe);

FKL_API
FklVMvalueWeakHashEq *fklCreateVMvalueWeakHashEq2(FklVM *exe,
        FklWeakMapMode mode);

FKL_API
FklVMvalue **fklVMvalueWeakHashEqGet(FklVMvalueWeakHashEq *h, FklVMvalue *k);

FKL_API
FklValueEqHashMapElm *fklVMvalueWeakHashEqInsert(FklVMvalueWeakHashEq *h,
        FklVMvalue *k);

FKL_API FklVMvalue *fklCreateVMvalueChanl(FklVM *, uint32_t);
FKL_API int fklIsVMvalueChanl(const FklVMvalue *v);

FKL_API FklVMvalue *fklCreateVMvalueBox(FklVM *, FklVMvalue *);

FKL_API FklVMvalue *fklCreateVMvalueBoxNil(FklVM *);

FKL_API
FklVMvalue *
fklCreateVMvalueError(FklVM *, FklVMvalue *type, FklVMvalue *message);

FKL_API
FklVMvalue *fklCreateVMvalueError2(FklVM *exe,
        FklVMvalue *type,
        const char *fmt,
        size_t count,
        FklVMvalue *values[]);

FKL_API int fklIsVMvalueError(const FklVMvalue *v);

FKL_API FklVMvalue *fklCreateVMvalueBigInt(FklVM *, size_t num);

FKL_API FklVMvalue *fklCreateVMvalueBigInt2(FklVM *, const FklBigInt *);

FKL_API
FklVMvalue *fklCreateVMvalueBigInt3(FklVM *, const FklBigInt *, size_t size);

FKL_API
FklVMvalue *
fklVMbigIntAdd(FklVM *, const FklVMvalueBigInt *a, const FklVMvalueBigInt *b);

FKL_API FklVMvalue *fklVMbigIntAddI(FklVM *, const FklVMvalueBigInt *, int64_t);

FKL_API
FklVMvalue *
fklVMbigIntSub(FklVM *, const FklVMvalueBigInt *a, const FklVMvalueBigInt *b);

FKL_API FklVMvalue *fklVMbigIntSubI(FklVM *, const FklVMvalueBigInt *, int64_t);

FKL_API
FklVMvalue *
fklCreateVMvalueBigIntWithString(FklVM *exe, const FklString *str, int base);

FKL_API
FklVMvalue *fklCreateVMvalueBigIntWithDecString(FklVM *exe,
        const FklString *str);

FKL_API
FklVMvalue *fklCreateVMvalueBigIntWithOctString(FklVM *exe,
        const FklString *str);

FKL_API
FklVMvalue *fklCreateVMvalueBigIntWithHexString(FklVM *exe,
        const FklString *str);

FKL_API FklVMvalue *fklCreateVMvalueBigIntWithI64(FklVM *, int64_t);

FKL_API
FklVMvalue *fklCreateVMvalueBigIntWithU64(FklVM *, uint64_t);

FKL_API
FklVMvalue *fklCreateVMvalueBigIntWithF64(FklVM *, double);

FKL_API
FklVMvalue *fklCreateVMvalueUd(FklVM *, const FklVMvalueType *t);

FKL_API
FklVMvalue *
fklCreateVMvalueUd2(FklVM *, const FklVMvalueType *t, size_t extra_size);

FKL_API
FklVMvalue *
fklCreateVMvalueUdSized(FklVM *, const FklVMvalueType *t, size_t actual_size);

FKL_API
FklVMvalue *fklCreateVMvalueCodeObj1(FklVM *);

FKL_API
FklVMvalue *fklCreateVMvalueCodeObjExt(FklVM *exe,
        FklIns ins,
        FklVMvalue *fid,
        size_t line,
        uint32_t scope);

FKL_API int fklIsVMvalueCodeObj(const FklVMvalue *v);

FKL_API FklVMvalue *fklVMvalueEof(void);

#define FKL_VM_EOF (fklVMvalueEof())

FKL_API FklVMvalue *fklVMvalueUndefined(void);

#define FKL_VM_UNDEFINED (fklVMvalueUndefined())

// value getters

#define FKL_UNUSEDBITNUM (3)
#define FKL_PTR_MASK ((intptr_t)0xFFFFFFFFFFFFFFF8)
#define FKL_TAG_MASK ((intptr_t)0x7)

#define FKL_GET_TAG(P) ((FklVMptrTag)(((uintptr_t)(P)) & FKL_TAG_MASK))
#define FKL_GET_PTR(P) ((FklVMptr)(((uintptr_t)(P)) & FKL_PTR_MASK))

#define FKL_GET_CHR(P) ((uint8_t)((uintptr_t)(P) >> FKL_UNUSEDBITNUM))
#define FKL_GET_SYM(P) (P)

#define FKL_IS_FIX(P) (FKL_GET_TAG(P) == FKL_TAG_FIX)
#define FKL_IS_CHR(P) (FKL_GET_TAG(P) == FKL_TAG_CHR)
#define FKL_IS_PTR(P) (FKL_GET_TAG(P) == FKL_TAG_PTR)

#define X(A, B)                                                                \
    static FKL_ALWAYS_INLINE int FKL_IS_##B(const FklVMvalue *p) {             \
        return FKL_IS_PTR(p) && (p)->type_ == FKL_TYPE_##B;                    \
    }
FKL_VM_TYPE_X
#undef X

static FKL_ALWAYS_INLINE FklVMvalue *FKL_VM_VAL(const void *c) {
    return FKL_TYPE_CAST(FklVMvalue *, c);
}

static FKL_ALWAYS_INLINE FklVMvalue **FKL_VM_CAR(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_PAIR(V));
    return &FKL_TYPE_CAST(FklVMvaluePair *, V)->car;
}

static FKL_ALWAYS_INLINE FklVMvalue **FKL_VM_CDR(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_PAIR(V));
    return &FKL_TYPE_CAST(FklVMvaluePair *, V)->cdr;
}

static FKL_ALWAYS_INLINE FklVMvaluePair *FKL_VM_PAIR(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_PAIR(V));
    return FKL_TYPE_CAST(FklVMvaluePair *, V);
}

#define FKL_VM_CAR(V) (*FKL_VM_CAR(V))
#define FKL_VM_CDR(V) (*FKL_VM_CDR(V))

static FKL_ALWAYS_INLINE FklString *FKL_VM_STR(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_STR(V));
    return (&((FklVMvalueStr *)(V))->str);
}

static FKL_ALWAYS_INLINE FklString *FKL_VM_SYM(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_SYM(V));
    return (&((FklVMvalueSym *)(V))->str);
}

static FKL_ALWAYS_INLINE uint8_t *FKL_VM_SYM_INTERNED(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_SYM(V));
    return &(((FklVMvalueSym *)(V))->interned);
}

static FKL_ALWAYS_INLINE FklString *FKL_VM_KEYWORD(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_KEYWORD(V));
    return (&((FklVMvalueKeyword *)(V))->str);
}

#define FKL_VM_SYM_INTERNED(V) (*FKL_VM_SYM_INTERNED(V))

static FKL_ALWAYS_INLINE FklBytes *FKL_VM_BYTES(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_BYTES(V));
    return (&((FklVMvalueBytes *)(V))->bytes);
}

static FKL_ALWAYS_INLINE FklVMvalueVec *FKL_VM_VEC(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_VEC(V));
    return FKL_TYPE_CAST(FklVMvalueVec *, V);
}

#define FKL_VM_VEC_CAS(V, I, O, N)                                             \
    (atomic_compare_exchange_strong(FKL_TYPE_CAST(_Atomic(FklVMvalue *) *,     \
                                            &((FKL_VM_VEC(V))->base[(I)])),    \
            (O),                                                               \
            (N)))

static FKL_ALWAYS_INLINE double *FKL_VM_F64(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_F64(V));
    return &(((FklVMvalueF64 *)(V))->f64);
}

#define FKL_VM_F64(V) (*(FKL_VM_F64(V)))

static FKL_ALWAYS_INLINE FklVMvalueProc *FKL_VM_PROC(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_PROC(V));
    return FKL_TYPE_CAST(FklVMvalueProc *, V);
}

static FKL_ALWAYS_INLINE FklVMvalueCproc *FKL_VM_CPROC(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_CPROC(V));
    return FKL_TYPE_CAST(FklVMvalueCproc *, (V));
}

static FKL_ALWAYS_INLINE FklVMvalueHash *FKL_VM_HASH(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_HASHTABLE(V));
    return FKL_TYPE_CAST(FklVMvalueHash *, (V));
}

static FKL_ALWAYS_INLINE FklVMvalueBigInt *FKL_VM_BI(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_BIGINT(V));
    return FKL_TYPE_CAST(FklVMvalueBigInt *, V);
}

static FKL_ALWAYS_INLINE FklVMvalueUd *FKL_VM_UD(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_USERDATA(V));
    return FKL_TYPE_CAST(FklVMvalueUd *, V);
}

static FKL_ALWAYS_INLINE FklVMvalue **FKL_VM_BOX(const FklVMvalue *V) {
    return &(((FklVMvalueBox *)(V))->box);
}

#define FKL_VM_BOX(V) (*(FKL_VM_BOX(V)))

#define FKL_VM_BOX_CAS(V, O, N)                                                \
    (atomic_compare_exchange_strong(                                           \
            FKL_TYPE_CAST(_Atomic(FklVMvalue *) *, &(FKL_VM_BOX((V)))),        \
            (O),                                                               \
            (N)))

static FKL_ALWAYS_INLINE FklVMvalueVarRef *FKL_VM_VAR_REF(const FklVMvalue *V) {
    FKL_ASSERT(FKL_IS_VAR_REF(V));
    return FKL_TYPE_CAST(FklVMvalueVarRef *, V);
}

static FKL_ALWAYS_INLINE FklVMvalueError *FKL_VM_ERR(const FklVMvalue *V) {
    FKL_ASSERT(fklIsVMvalueError(V));
    return FKL_TYPE_CAST(FklVMvalueError *, V);
}

static FKL_ALWAYS_INLINE FklVMvalueChanl *FKL_VM_CHANL(const FklVMvalue *V) {
    FKL_ASSERT(fklIsVMvalueChanl(V));
    return FKL_TYPE_CAST(FklVMvalueChanl *, V);
}

static FKL_ALWAYS_INLINE FklVMvalueDll *FKL_VM_DLL(const FklVMvalue *V) {
    FKL_ASSERT(fklIsVMvalueDll(V));
    return FKL_TYPE_CAST(FklVMvalueDll *, V);
}

static FKL_ALWAYS_INLINE FklVMvalueFp *FKL_VM_FP(const FklVMvalue *V) {
    FKL_ASSERT(fklIsVMvalueFp(V));
    return FKL_TYPE_CAST(FklVMvalueFp *, V);
}

static FKL_ALWAYS_INLINE FklByteCodelnt *FKL_VM_CO(const FklVMvalue *V) {
    FKL_ASSERT(fklIsVMvalueCodeObj(V));
    return &FKL_TYPE_CAST(FklVMvalueCodeObj *, V)->bcl;
}

#define FKL_VM_VAR_REF_GET(V) ((atomic_load(&(FKL_VM_VAR_REF(V)->ref))))

// vmparser

FKL_API
void fklVMvaluePushState0ToStack(FklParseStateVector *stateStack);

FKL_API
void *
fklVMvalueTerminalCreate(const char *s, size_t len, size_t line, void *ctx);

FKL_API void fklAddToGC(FklVMvalue *, FklVM *);
FKL_API FklVMvalue *fklCreateTrueValue(void);
FKL_API FklVMvalue *fklCreateNilValue(void);

// return the callee if I is -1
#define FKL_VM_GET_ARG(S, F, I) ((S)->base[(F)->bp + 1 + (I)])

#define FKL_VM_GET_TOP_VALUE(S) ((S)->base[(S)->tp - 1])

#define FKL_VM_POP_TOP_VALUE(S) ((S)->base[--(S)->tp])

#define FKL_VM_PUSH_VALUE(S, V) fklPushVMvalue((S), (V))

static inline void fklUpdateAllVarRef(FklVM *exe, FklVMframe *f) {
    for (; f; f = f->prev)
        if (f->type == FKL_FRAME_COMPOUND) {
            FklVMvalue **loc = &FKL_VM_GET_ARG(exe, f, 0);
            for (FklVMvalue *ll = f->lrefl; FKL_IS_PAIR(ll);
                    ll = FKL_VM_CDR(ll)) {
                FklVMvalueVarRef *ref = FKL_VM_VAR_REF(FKL_VM_CAR(ll));
                if (ref->ref != &ref->v)
                    ref->ref = &loc[ref->idx];
            }
        }
}

FKL_API void fklVMstackReserve(FklVM *exe, uint32_t s);

static inline void fklPushVMvalue(FklVM *s, FklVMvalue *v) {
    if (s->tp >= s->last)
        fklVMstackReserve(s, s->tp + 1);
    s->base[s->tp++] = v;
}

static FKL_ALWAYS_INLINE FklVMvalue *fklGetTopValue(FklVM *exe) {
    return exe->base[exe->tp - 1];
}
static FKL_ALWAYS_INLINE FklVMvalue *fklPopTopValue(FklVM *s) {
    return s->base[--s->tp];
}
static FKL_ALWAYS_INLINE FklVMvalue *fklGetValue(FklVM *exe, uint32_t i) {
    return exe->base[exe->tp - i];
}
static FKL_ALWAYS_INLINE FklVMvalue **fklGetStackSlot(FklVM *exe, uint32_t i) {
    return &exe->base[exe->tp - i];
}

FKL_API int fklVMvalueEqual(const FklVMvalue *, const FklVMvalue *);
FKL_API int fklVMvalueCmp(FklVMvalue *, FklVMvalue *, int *);

FKL_API
FklVMfpRW fklGetVMfpRwFromCstr(const char *mode);

FKL_API int fklVMfpRewind(FklVMvalueFp *vfp, FklStrBuf *, size_t j);
FKL_API int fklVMfpEof(FklVMvalueFp *);
FKL_API int fklVMfpFileno(FklVMvalueFp *);
FKL_API int fklVMfpClose(FklVMvalueFp *);

typedef FklVMvalue **(*FklCgDllLibInitExportCb)(FklVM *vm, uint32_t *num);

#define FKL_IMPORT_DLL_INIT_FUNC_ARGS                                          \
    FklVM *exe, FklVMvalue *dll, uint32_t count, FklVMvalue *values[]

typedef int (*FklImportDllInitFunc)(FKL_IMPORT_DLL_INIT_FUNC_ARGS);
typedef void (*FklDllInitFunc)(FklVMvalueDll *dll, FklVM *exe);
typedef void (*FklDllUninitFunc)(void);
typedef const FklDllStateDesc *(*FklDllStateDescGet)(void);

FKL_API
FklDllUninitFunc fklVMdllGetUninitCb(FklVMvalueDll *dll);

#define FKL_CHECK_IMPORT_DLL_INIT_FUNC()                                       \
    static_assert(                                                             \
            _Generic(&_fklImportInit, FklImportDllInitFunc: 1, default: 0),    \
            "invalid import dll init func")

#define FKL_CHECK_EXPORT_DLL_INIT_FUNC()                                       \
    static_assert(_Generic(&_fklExportSymbolInit,                              \
                    FklCgDllLibInitExportCb: 1,                                \
                    default: 0),                                               \
            "invalid export dll init func")

#define FKL_CHECK_DLL_DESC_GET_FUNC()                                          \
    static_assert(                                                             \
            _Generic(&_fklDllStateDescGet, FklDllStateDescGet: 1, default: 0), \
            "invalid dll desc get func")

FKL_API uint64_t fklVMchanlRecvqLen(FklVMvalueChanl *ch);
FKL_API uint64_t fklVMchanlSendqLen(FklVMvalueChanl *ch);
FKL_API uint64_t fklVMchanlMessageNum(FklVMvalueChanl *ch);
FKL_API int fklVMchanlFull(FklVMvalueChanl *ch);
FKL_API int fklVMchanlEmpty(FklVMvalueChanl *ch);

FKL_API void fklVMsleep(FklVM *, uint64_t ms);

FKL_API void fklVMread(FklVM *, FILE *fp, FklStrBuf *buf, uint64_t len, int d);

FKL_API void fklVMacquireWq(FklVMgc *);
FKL_API void fklVMreleaseWq(FklVMgc *);

FKL_API
void fklQueueWorkInIdleThread(FklVM *vm,
        void (*cb)(FklVM *, void *),
        void *arg);

static FKL_ALWAYS_INLINE void fklVMyield(FklVM *exe) {
    if (exe->is_single_thread || !atomic_load(&(exe)->notice_lock)) {
        uv_sleep(0);
        return;
    }

    uv_mutex_unlock(&exe->lock);

    while (fklVMgcStateGet(exe->gc) == FKL_GC_STW)
        uv_sleep(0);

    uv_mutex_lock(&exe->lock);
}

FKL_API void fklUnlockThread(FklVM *);
FKL_API void fklLockThread(FklVM *);

#define FKL_VM_LOCK_BLOCK(exe, flag)                                           \
    for (uint8_t flag = (fklLockThread(exe), 0); flag < 1;                     \
            fklUnlockThread(exe), ++flag)

#define FKL_VM_UNLOCK_BLOCK(exe, flag)                                         \
    for (uint8_t flag = (fklUnlockThread(exe), 0); flag < 1;                   \
            fklLockThread(exe), ++flag)

FKL_API void fklSetThreadReadyToExit(FklVM *);
FKL_API void fklVMstopTheWorld(FklVMgc *);
FKL_API void fklVMcontinueTheWorld(FklVMgc *);

FKL_API void fklChanlSend(FklVMvalueChanl *, FklVMvalue *msg, FklVM *);
FKL_API void fklChanlRecv(FklVMvalueChanl *, uint32_t, FklVM *);
FKL_API int fklChanlRecvOk(FklVMvalueChanl *, FklVMvalue **);

FKL_API int fklWriteVMvalue(const FklVMvalue *v, FklCodeBuilder *fp);
FKL_API int fklVMvalueLength(const FklVMvalue *v, size_t *len);

FKL_API int fklIsCallable(FklVMvalue *);

FKL_API void fklInitVMargs(FklVMgc *gc, int argc, const char *const *argv);

FKL_API int fklIsVMnumberLt0(const FklVMvalue *);

FKL_API
void fklVMsetTpAndPushValue(FklVM *exe, uint32_t rtp, FklVMvalue *retval);

FKL_API size_t fklVMlistLength(const FklVMvalue *);

FKL_API void fklPushVMraiseErrorFrame(FklVM *exe, FklVMvalue *err);

FKL_API void fklPushVMframe(FklVMframe *, FklVM *exe);

FKL_API
FklVMframe *fklCreateOtherObjVMframe(FklVM *exe,
        const FklVMframeContextMethodTable *t);

FKL_API
FklVMframe *fklCreateNewOtherObjVMframe(const FklVMframeContextMethodTable *t);

FKL_API void fklVMcompoundFrameReturn(FklVM *exe);
FKL_API void fklDestroyVMframes(FklVMframe *h);

FKL_API void fklLockVMlib(FklVMvalueLib *lib);
FKL_API void fklUnlockVMlib(FklVMvalueLib *lib);

FKL_API int fklIsVMvalueLib(const FklVMvalue *);

FKL_API
FklVMvalueLib *fklCreateVMvalueLib(FklVM *vm, //
        FklVMvalue *name,
        const FklVMvalueVec *names);

static FKL_ALWAYS_INLINE FklVMvalueLib *fklVMvalueLib(const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueLib(v));
    return FKL_TYPE_CAST(FklVMvalueLib *, v);
}

FKL_ALWAYS_INLINE
static FklVMvalue *const *fklVMvalueLibNames(const FklVMvalueLib *l) {
    return &l->values[l->count];
}

FKL_API
void fklInitBuiltinErrorType(FklVMvalue *errorTypeId[FKL_BUILTIN_ERR_NUM],
        FklVMgc *);

FKL_API
uintptr_t fklVMvalueEqualHashv(const FklVMvalue *v);

noreturn static FKL_ALWAYS_INLINE void
FKL_RAISE_BUILTIN_ERROR(FklBuiltinErrorType error_type, FklVM *exe) {
    FklVMvalue *errorMessage = fklGenErrorMessage(error_type, exe);
    FklVMvalue *err = fklCreateVMvalueError(exe,
            exe->gc->builtinErrorTypeId[error_type],
            errorMessage);
    fklRaiseVMerror(err, exe);
}

noreturn static FKL_ALWAYS_INLINE void fklRaiseBuiltinErrorFmtArr(
        FklBuiltinErrorType error_type,
        FklVM *exe,
        const char *fmt,
        size_t value_count,
        FklVMvalue *values[]) {
    FklVMvalue *errorMessage = fklVMformatToString(exe, //
            fmt,
            value_count,
            values);
    FklVMvalue *err = fklCreateVMvalueError(exe,
            exe->gc->builtinErrorTypeId[error_type],
            errorMessage);
    fklRaiseVMerror(err, exe);
}

static FKL_ALWAYS_INLINE FklVMvalue *fklCreateVMvalueErrorFmt(FklVM *exe,
        FklBuiltinErrorType error_type,
        const char *fmt,
        FklVMvalue *values[]) {
    size_t count = 0;
    for (; values[count]; ++count)
        ;
    return fklCreateVMvalueError2(exe,
            exe->gc->builtinErrorTypeId[error_type],
            fmt,
            count,
            values);
}

noreturn static FKL_ALWAYS_INLINE void fklRaiseBuiltinErrorFmtV(
        FklBuiltinErrorType error_type,
        FklVM *exe,
        const char *fmt,
        FklVMvalue *values[]) {
    size_t count = 0;
    for (; values[count]; ++count)
        ;
    fklRaiseBuiltinErrorFmtArr(error_type, exe, fmt, count, values);
}

#define FKL_MAKE_VM_ERR(ERRORTYPE, EXE, FMT, ...)                              \
    fklCreateVMvalueErrorFmt((EXE),                                            \
            (ERRORTYPE),                                                       \
            (FMT),                                                             \
            &((FklVMvalue *[]){ NULL, ##__VA_ARGS__, NULL })[1])

#define FKL_RAISE_BUILTIN_ERROR_FMT(ERRORTYPE, EXE, FMT, ...)                  \
    fklRaiseBuiltinErrorFmtV((ERRORTYPE),                                      \
            (EXE),                                                             \
            (FMT),                                                             \
            &((FklVMvalue *[]){ NULL, ##__VA_ARGS__, NULL })[1]);

#define FKL_VM_NIL ((FklVMptr)0x1)
#define FKL_VM_TRUE (FKL_MAKE_VM_FIX(1))

#define FKL_MAKE_VM_CHR(C)                                                     \
    ((FklVMptr)((((uintptr_t)(C)) << FKL_UNUSEDBITNUM) | FKL_TAG_CHR))

#define HASH_P(T)                                                              \
    static FKL_ALWAYS_INLINE int FKL_IS_HASHTABLE_##T(const FklVMvalue *p) {   \
        return FKL_IS_HASHTABLE(p) && FKL_VM_HASH(p)->eq_type == FKL_HASH_##T; \
    }

HASH_P(EQ);
HASH_P(EQV);
HASH_P(EQUAL);

#undef HASH_P

#if FKL_IS_ARITHMETIC_RIGHT_SHIFT

#define FKL_GET_FIX(P) ((int64_t)((intptr_t)(P) >> FKL_UNUSEDBITNUM))
#define FKL_MAKE_VM_FIX(I)                                                     \
    ((FklVMptr)((((uintptr_t)(I)) << FKL_UNUSEDBITNUM) | FKL_TAG_FIX))

#else

#define FKL_GET_FIX(P)                                                         \
    (((int64_t)((uintptr_t)(P) >> FKL_UNUSEDBITNUM)) - FKL_FIX_INT_OFFSET)

#define FKL_MAKE_VM_FIX(I)                                                     \
    ((FklVMptr)((((uintptr_t)(((int64_t)(I)) + FKL_FIX_INT_OFFSET))            \
                        << FKL_UNUSEDBITNUM)                                   \
                | FKL_TAG_FIX))
#endif

static FKL_ALWAYS_INLINE int FKL_IS_TRUE(const void *P) {
    FKL_ASSERT(P != NULL);
    return (P) != FKL_VM_NIL;
}

static FKL_ALWAYS_INLINE int FKL_IS_NIL(const void *P) {
    FKL_ASSERT(P != NULL);
    return (P) == FKL_VM_NIL;
}

#define FKL_VM_USER_DATA_DEFAULT_PRINT(NAME, DATA_TYPE_NAME)                   \
    static void NAME(const FklVMvalue *ud,                                     \
            FklCodeBuilder *build,                                             \
            FklVM *exe) {                                                      \
        fklCodeBuilderFmt(build, "#<%s %p>", DATA_TYPE_NAME, ud);              \
    }

// inlines

static FKL_ALWAYS_INLINE FklVMvalue *fklMakeVMint(FklVM *vm, int64_t r64) {
    if (r64 > FKL_FIX_INT_MAX || r64 < FKL_FIX_INT_MIN)
        return fklCreateVMvalueBigIntWithI64(vm, r64);
    else
        return FKL_MAKE_VM_FIX(r64);
}

static FKL_ALWAYS_INLINE FklVMvalue *fklMakeVMintU(FklVM *vm, uint64_t r64) {
    if (r64 > FKL_FIX_INT_MAX)
        return fklCreateVMvalueBigIntWithU64(vm, r64);
    else
        return FKL_MAKE_VM_FIX(r64);
}

static FKL_ALWAYS_INLINE FklVMvalue *fklMakeVMintD(FklVM *vm, double r64) {
    if (isgreater(r64, (double)FKL_FIX_INT_MAX) || isless(r64, FKL_FIX_INT_MIN))
        return fklCreateVMvalueBigIntWithF64(vm, r64);
    else
        return FKL_MAKE_VM_FIX(r64);
}

static FKL_ALWAYS_INLINE void fklVMgcToGray(const FklVMvalue *v_, FklVMgc *gc) {
    FklVMvalue *v = FKL_TYPE_CAST(FklVMvalue *, v_);
    if (v && FKL_IS_PTR(v) && v->mark_ < FKL_MARK_G) {
        v->mark_ = FKL_MARK_G;
        v->gray_next_ = gc->gray_list;
        gc->gray_list = v;
    }
}

static inline FklBigInt fklVMbigIntToBigInt(const FklVMvalueBigInt *b) {
    const FklBigInt bi = {
        .digits = (FklBigIntDigit *)b->digits,
        .num = b->num,
        .size = fklAbs(b->num),
        .const_size = 1,
    };
    return bi;
}

#define FKL_VM_BIGINT_CALL_1(NAME, FUNC)                                       \
    static inline void NAME(const FklVMvalueBigInt *b) {                       \
        const FklBigInt bi = fklVMbigIntToBigInt(b);                           \
        FUNC(&bi);                                                             \
    }
#define FKL_VM_BIGINT_CALL_1R(NAME, RET, FUNC)                                 \
    static inline RET NAME(const FklVMvalueBigInt *b) {                        \
        const FklBigInt bi = fklVMbigIntToBigInt(b);                           \
        return FUNC(&bi);                                                      \
    }
#define FKL_VM_BIGINT_CALL_2(NAME, ARG2, FUNC)                                 \
    static inline void NAME(const FklVMvalueBigInt *b, ARG2 arg2) {            \
        const FklBigInt bi = fklVMbigIntToBigInt(b);                           \
        FUNC(&bi, arg2);                                                       \
    }
#define FKL_VM_BIGINT_CALL_2R(NAME, RET, ARG2, FUNC)                           \
    static inline RET NAME(const FklVMvalueBigInt *b, ARG2 arg2) {             \
        const FklBigInt bi = fklVMbigIntToBigInt(b);                           \
        return FUNC(&bi, arg2);                                                \
    }
#define FKL_VM_CALL_WITH_2_BIR(NAME, RET, FUNC)                                \
    static inline RET NAME(const FklVMvalueBigInt *a,                          \
            const FklVMvalueBigInt *b) {                                       \
        const FklBigInt a1 = fklVMbigIntToBigInt(a);                           \
        const FklBigInt b1 = fklVMbigIntToBigInt(b);                           \
        return FUNC(&a1, &b1);                                                 \
    }

#define FKL_VM_CALL_WITH_1_VB_1BI(NAME, FUNC)                                  \
    static inline void NAME(FklBigInt *a, const FklVMvalueBigInt *b) {         \
        const FklBigInt b1 = fklVMbigIntToBigInt(b);                           \
        FUNC(a, &b1);                                                          \
    }

FKL_VM_BIGINT_CALL_1R(fklVMbigIntToI, int64_t, fklBigIntToI);
FKL_VM_BIGINT_CALL_1R(fklVMbigIntToD, double, fklBigIntToD);
FKL_VM_BIGINT_CALL_1R(fklVMbigIntToU, uint64_t, fklBigIntToU);
FKL_VM_BIGINT_CALL_1R(fklVMbigIntHash, uintptr_t, fklBigIntHash);
FKL_VM_BIGINT_CALL_1R(fklIsVMbigIntLt0, int, fklIsBigIntLt0);
FKL_VM_BIGINT_CALL_1R(fklIsVMbigIntEven, int, fklIsBigIntEven);
FKL_VM_BIGINT_CALL_1R(fklIsVMbigIntOdd, int, fklIsBigIntOdd);
FKL_VM_BIGINT_CALL_2R(fklIsVMbigIntAddkInFixIntRange,
        int,
        int8_t,
        fklIsBigIntAddkInFixIntRange);
FKL_VM_BIGINT_CALL_1R(fklCreateBigIntWithVMbigInt, FklBigInt *, fklCopyBigInt);
FKL_VM_BIGINT_CALL_2(fklPrintVMbigInt, FklCodeBuilder *, fklPrintBigInt2);
FKL_VM_CALL_WITH_2_BIR(fklVMbigIntEqual, int, fklBigIntEqual);
FKL_VM_CALL_WITH_2_BIR(fklVMbigIntCmp, int, fklBigIntCmp);
FKL_VM_BIGINT_CALL_2R(fklVMbigIntCmpI, int, int64_t, fklBigIntCmpI);
FKL_VM_BIGINT_CALL_2R(fklVMbigIntCmpU, int, uint64_t, fklBigIntCmpU);
FKL_VM_CALL_WITH_1_VB_1BI(fklAddVMbigInt, fklAddBigInt);
FKL_VM_CALL_WITH_1_VB_1BI(fklSubVMbigInt, fklSubBigInt);
FKL_VM_CALL_WITH_1_VB_1BI(fklMulVMbigInt, fklMulBigInt);

#undef FKL_VM_BIGINT_CALL_1R
#undef FKL_VM_BIGINT_CALL_1
#undef FKL_VM_BIGINT_CALL_2
#undef FKL_VM_CALL_WITH_2_BI
#undef FKL_VM_CALL_WITH_1_VB_1BI

static FKL_ALWAYS_INLINE int64_t fklVMgetInt(const FklVMvalue *p) {
    return FKL_IS_FIX(p) ? FKL_GET_FIX(p) : fklVMbigIntToI(FKL_VM_BI(p));
}

static FKL_ALWAYS_INLINE uint64_t fklVMgetUint(const FklVMvalue *p) {
    return FKL_IS_FIX(p) ? (uint64_t)FKL_GET_FIX(p)
                         : fklVMbigIntToU(FKL_VM_BI(p));
}

static FKL_ALWAYS_INLINE int fklVMintegerCmpI(FklVMvalue *p, int64_t i) {
    if (FKL_IS_BIGINT(p)) {
        return fklVMbigIntCmpI(FKL_VM_BI(p), i);
    } else if (FKL_IS_FIX(p)) {
        int64_t v = FKL_GET_FIX(p);
        if (v > i) {
            return 1;
        } else if (v < i) {
            return -1;
        } else
            return 0;
    } else {
        FKL_UNREACHABLE();
    }
    FKL_ASSERT(0);
    return 0;
}

static FKL_ALWAYS_INLINE int fklVMintegerCmpU(FklVMvalue *p, uint64_t u) {
    if (FKL_IS_BIGINT(p)) {
        return fklVMbigIntCmpU(FKL_VM_BI(p), u);
    } else if (FKL_IS_FIX(p)) {
        int64_t vv = FKL_GET_FIX(p);
        if (vv < 0)
            return -1;
        uint64_t v = (uint64_t)vv;
        if (v > u) {
            return 1;
        } else if (v < u) {
            return -1;
        } else
            return 0;
    } else {
        FKL_UNREACHABLE();
    }
    FKL_ASSERT(0);
    return 0;
}

static inline void fklSetBigIntWithVMbigInt(FklBigInt *a,
        const FklVMvalueBigInt *b) {
    const FklBigInt bi = fklVMbigIntToBigInt(b);
    fklSetBigInt(a, &bi);
}

static inline int fklIsVMbigIntDivisible(const FklVMvalueBigInt *a,
        const FklBigInt *b) {
    const FklBigInt a0 = fklVMbigIntToBigInt(a);
    return fklIsDivisibleBigInt(&a0, b);
}

static inline int fklIsVMbigIntDivisibleI(const FklVMvalueBigInt *a,
        int64_t b) {
    const FklBigInt a0 = fklVMbigIntToBigInt(a);
    return fklIsDivisibleBigIntI(&a0, b);
}

static inline FklVMvalue *fklCreateVMvalueBigIntWithOther(FklVM *exe,
        const FklVMvalueBigInt *b) {
    const FklBigInt bi = fklVMbigIntToBigInt(b);
    return fklCreateVMvalueBigInt2(exe, &bi);
}

static inline FklVMvalue *fklCreateVMvalueBigIntWithOther2(FklVM *exe,
        const FklVMvalueBigInt *b,
        size_t size) {
    const FklBigInt bi = fklVMbigIntToBigInt(b);
    return fklCreateVMvalueBigInt3(exe, &bi, size);
}

static inline void
fklVMframeSetSp(FklVM *exe, FklVMframe *frame, uint32_t lcount) {
    frame->arg_num = exe->tp - frame->bp - 1;
    frame->sp = frame->bp + 1 + lcount;
    fklVMstackReserve(exe, frame->sp + 1);
    if (frame->sp > exe->tp) {
        memset(&exe->base[exe->tp],
                0,
                (frame->sp - exe->tp) * sizeof(FklVMvalue *));
        exe->tp = frame->sp;
    }
}

static inline void
fklVMframeSetBp(FklVM *exe, FklVMframe *frame, uint32_t lcount) {
    frame->bp = exe->bp;
    fklVMframeSetSp(exe, frame, lcount);
}

static FKL_ALWAYS_INLINE void fklSetBp(FklVM *s) {
    FKL_VM_PUSH_VALUE(s, FKL_MAKE_VM_FIX(s->bp));
    s->bp = s->tp;
}

static FKL_ALWAYS_INLINE int fklIsVMint(const FklVMvalue *p) {
    return FKL_IS_FIX(p) || FKL_IS_BIGINT(p);
}

static FKL_ALWAYS_INLINE int fklIsVMnumber(const FklVMvalue *p) {
    return FKL_IS_FIX(p) || FKL_IS_BIGINT(p) || FKL_IS_F64(p);
}

static FKL_ALWAYS_INLINE uintptr_t fklVMintegerHashv(const FklVMvalue *v) {
    if (FKL_IS_FIX(v))
        return FKL_GET_FIX(v);
    else
        return fklVMbigIntHash(FKL_VM_BI(v));
}

static FKL_ALWAYS_INLINE uintptr_t fklVMvalueEqvHashv(const FklVMvalue *v) {
    if (fklIsVMint(v))
        return fklVMintegerHashv(v);
    else
        return fklVMvalueEqHashv(v);
}

static FKL_ALWAYS_INLINE int fklVMvalueEq(const FklVMvalue *fir,
        const FklVMvalue *sec) {
    return fir == sec;
}

static FKL_ALWAYS_INLINE int fklVMvalueEqv(const FklVMvalue *fir,
        const FklVMvalue *sec) {
    if (FKL_IS_BIGINT(fir) && FKL_IS_BIGINT(sec))
        return fklVMbigIntEqual(FKL_VM_BI(fir), FKL_VM_BI(sec));
    else
        return fir == sec;
}

static FKL_ALWAYS_INLINE int fklVMgcIsMarked(const FklVMvalue *v) {
    return v != NULL && FKL_IS_PTR(v) && v->mark_ != FKL_MARK_W;
}

static FKL_ALWAYS_INLINE void fklVMvalueTerminalDestroy(void *v) {}

static FKL_ALWAYS_INLINE int FKL_IS_TYPE(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->tp_->token == &FklVMtypeType.mt;
}

static FKL_ALWAYS_INLINE FklVMvalueType *FKL_VM_TYPE(const FklVMvalue *v) {
    FKL_ASSERT(FKL_IS_TYPE(v));
    return (FklVMvalueType *)v;
}

static FKL_ALWAYS_INLINE const char *fklVMstr(const FklVMvalue *v) {
    const char *r = FKL_IS_SYM(v)     ? FKL_VM_SYM(v)->str
                  : FKL_IS_KEYWORD(v) ? FKL_VM_KEYWORD(v)->str
                  : FKL_IS_STR(v)     ? FKL_VM_STR(v)->str
                                      : NULL;
    return r;
}

/// 闭区间 [from, to]
static FKL_ALWAYS_INLINE int
fklVMintegerInRangeI(FklVMvalue *v, int64_t from, int64_t to) {
    FKL_ASSERT(fklIsVMint(v));
    return fklVMintegerCmpI(v, from) >= 0 && fklVMintegerCmpI(v, to) <= 0;
}

/// 闭区间 [from, to]
static FKL_ALWAYS_INLINE int
fklVMintegerInRangeU(FklVMvalue *v, uint64_t from, uint64_t to) {
    FKL_ASSERT(fklIsVMint(v));
    return fklVMintegerCmpU(v, from) >= 0 && fklVMintegerCmpU(v, to) <= 0;
}

#ifdef FKL_USING_DLL
#define FKL_VM_TYPE_TP_INIT NULL
#else
#define FKL_VM_TYPE_TP_INIT &FklVMtypeType
#endif

#define FKL_VM_TYPE_STATIC_INIT(NAME, ...)                                     \
    {                                                                          \
        .next_ = NULL,                                                         \
        .gray_next_ = NULL,                                                    \
        .mark_ = FKL_MARK_B,                                                   \
        .type_ = FKL_TYPE_USERDATA,                                            \
        .tp_ = FKL_VM_TYPE_TP_INIT,                                            \
        .dll = NULL,                                                           \
        .token = &NAME.mt,                                                     \
        .mt = __VA_ARGS__,                                                     \
    }

#if defined(FKL_USING_DLL) || defined(FKL_USING_WIN32)
#define FKL_VM_TYPE_ATTR alignas(8) static
#else
#define FKL_VM_TYPE_ATTR alignas(8) static const
#endif

FKL_API
FklVMvalueType *fklCreateVMvalueType(FklVM *,
        FklVMvalue *dll,
        const void *token,
        const FklVMudMetaTable *mt);

FKL_API
FklVMvalue *fklVMpathVecToString(FklVM *vm, FklVMvalue *path_vec);

FKL_API
FklVMvalue *fklVMpathStrToVec(FklVM *vm, const char *p);

#ifdef __cplusplus
}
#endif

#endif
