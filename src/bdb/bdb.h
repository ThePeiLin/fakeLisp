#ifndef FKL_BDB_BDB_H
#define FKL_BDB_BDB_H

#include <fakeLisp/codegen.h>
#include <fakeLisp/vm.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { DBG_INTERRUPTED = 1, DBG_ERROR_OCCUR };

struct BdbBpInsHashMapElm;

// BdbFrameVector
#define FKL_VECTOR_TYPE_PREFIX Bdb
#define FKL_VECTOR_METHOD_PREFIX bdb
#define FKL_VECTOR_ELM_TYPE FklVMframe *
#define FKL_VECTOR_ELM_TYPE_NAME Frame
#include <fakeLisp/cont/vector.h>

// BdbThreadVector
#define FKL_VECTOR_TYPE_PREFIX Bdb
#define FKL_VECTOR_METHOD_PREFIX bdb
#define FKL_VECTOR_ELM_TYPE FklVM *
#define FKL_VECTOR_ELM_TYPE_NAME Thread
#include <fakeLisp/cont/vector.h>

typedef struct {
    const FklString *str;
    const FklString *filename;
    uint32_t line;
} BdbPos;

// 用于区分宿主的值和调试器的值
typedef struct {
    FklVMvalue *v;
} BdbWrapper;

#define BDB_NONE                                                               \
    (BdbWrapper) { .v = NULL }

static FKL_ALWAYS_INLINE BdbWrapper bdbWrap(FklVMvalue *v) {
    return (BdbWrapper){ .v = v };
}

static FKL_ALWAYS_INLINE FklVMvalue *bdbUnwrap(BdbWrapper const v) {
    return v.v;
}

static FKL_ALWAYS_INLINE const FklString *bdbSymbol(BdbWrapper const v) {
    return FKL_VM_SYM(bdbUnwrap(v));
}

static FKL_ALWAYS_INLINE uintptr_t bdbEqHashv(BdbWrapper const v) {
    return fklVMvalueEqHashv(bdbUnwrap(v));
}

static FKL_ALWAYS_INLINE int bdbEq(BdbWrapper const a, BdbWrapper const b) {
    return bdbUnwrap(a) == bdbUnwrap(b);
}

static FKL_ALWAYS_INLINE int bdbHas(BdbWrapper const v) { return v.v != NULL; }

// XXX: 现在断点释放后恢复原指令的过程是由 BdbBpTable 统一管理的
// 我们需要改进这个统一的指令恢复的实现
// 让断点不需要一个 wrapper
typedef struct BdpBp {
    struct BdpBp **pnext;
    struct BdpBp *next;
    struct BdpBp *next_del;

    atomic_uint reached_count;
    uint32_t idx;
    uint32_t count;
    uint32_t line;

    uint8_t is_errored;
    uint8_t is_temporary;
    uint8_t is_deleted;
    uint8_t is_disabled;

    struct BdbBpInsHashMapElm *item;

    BdbWrapper filename;
    BdbWrapper cond_exp;
    BdbWrapper cond_proc;
} BdbBp;

typedef struct {
    FklIns origin_ins;
    BdbBp *bp;
} BdbCodepoint;

// BdbBpInsHashMap
#define FKL_HASH_TYPE_PREFIX Bdb
#define FKL_HASH_METHOD_PREFIX bdb
#define FKL_HASH_KEY_TYPE const FklIns *
#define FKL_HASH_KEY_HASH return fklPtrHash(*pk);
#define FKL_HASH_VAL_TYPE BdbCodepoint
#define FKL_HASH_ELM_NAME BpIns
#include <fakeLisp/cont/hash.h>

// BdbBpIdxHashMap
#define FKL_HASH_TYPE_PREFIX Bdb
#define FKL_HASH_METHOD_PREFIX bdb
#define FKL_HASH_KEY_TYPE uint32_t
#define FKL_HASH_VAL_TYPE BdbBp *
#define FKL_HASH_ELM_NAME BpIdx
#include <fakeLisp/cont/hash.h>

typedef struct {
    BdbBpInsHashMap ins_ht;
    BdbBpIdxHashMap idx_ht;
    BdbBp *deleted_breakpoints;
    uint32_t next_idx;
} BdbBpTable;

// BdbSourceCodeHashMap
#define FKL_HASH_TYPE_PREFIX Bdb
#define FKL_HASH_METHOD_PREFIX bdb
#define FKL_HASH_KEY_TYPE BdbWrapper
#define FKL_HASH_KEY_HASH return bdbEqHashv(*pk);
#define FKL_HASH_KEY_EQUAL(A, B) bdbEq(*(A), *(B))
#define FKL_HASH_VAL_TYPE FklStringVector
#define FKL_HASH_VAL_UNINIT(V)                                                 \
    {                                                                          \
        FklString **c = (V)->base;                                             \
        FklString **const end = c + (V)->size;                                 \
        for (; c < end; ++c)                                                   \
            fklZfree(*c);                                                      \
        fklStringVectorUninit(V);                                              \
    }
#define FKL_HASH_ELM_NAME SourceCode
#include <fakeLisp/cont/hash.h>

#define BDB_STEPPING_TARGET_INS_COUNT (2)

typedef struct {
    FklStrBuf buf;
    FklAnalysisSymbolVector symbols;
    FklParseStateVector states;
} BdbCmdReadCtx;

FKL_VM_DEF_UD_STRUCT(FklVMvalueDebugCtx, {
    FklVMvalue *backtrace_list;
    FklValueHashSet file_sid_set;
    BdbCmdReadCtx read_ctx;

    int8_t inited;
    int8_t exit;
    int8_t running;
    int8_t done;

    uint32_t curlist_line;
    uint32_t curline;

    uint64_t cur_ins_pc;

    const FklString *curline_str;
    const BdbSourceCodeHashMapElm *curfile_lines;

    BdbSourceCodeHashMap source_code_table;

    jmp_buf *jmpb;
    FklVM *reached_thread;

    BdbFrameVector reached_thread_frames;
    uint32_t curframe_idx;

    BdbThreadVector threads;
    uint32_t curthread_idx;

    const BdbBp *reached_breakpoint;

    struct SteppingCtx {
        FklVM *vm;
        const FklLntItem *ln;

        FklIns ins[BDB_STEPPING_TARGET_INS_COUNT];
        const FklIns *target_ins[BDB_STEPPING_TARGET_INS_COUNT];
    } stepping_ctx;

    // 下面的成员由 `DbgCtx` 的GC标记
    // 所以下面的对象引用成员不使用 `BdbWrapper`
    FklVMgc gc;
    BdbBpTable bt;
    FklCgCtx cg_ctx;
    FklVMvalueCgEnv *glob_env;

    FklVMvalueCgEnv *eval_env;
    FklVMvalueCgInfo *eval_info;

    FklVMvalueProc *cur_proc;
    FklVMvalueProc *main_proc;
    FklValueVector code_objs;

    BdbWrapper error;
});

FKL_VM_DEF_UD_STRUCT(FklVMvalueBpWrapper, { BdbBp *bp; });

typedef FklVMvalueDebugCtx DebugCtx;

typedef enum {
    BDB_PUT_BP_AT_END_OF_FILE = 1,
    BDB_PUT_BP_FILE_INVALID,
    BDB_PUT_BP_NOT_A_PROC,
} BdbPutBpErrorType;

typedef struct {
    BdbBp *bp;
    const FklLntItem *ln;
    DebugCtx *ctx;
    BdbWrapper error;
} BdbIntArg;

int bdbInitDbgCtx(DebugCtx *,
        FklVM *exe,
        const char *filename,
        FklVMvalue *argv);
void bdbExitDbgCtx(DebugCtx *);
void bdbUninitDbgCtx(DebugCtx *);

void bdbInitBpTable(BdbBpTable *);
void bdbUninitBpTable(BdbBpTable *);

const FklStringVector *bdbGetSource(DebugCtx *dctx, const FklString *filename);

BdbWrapper bdbAddSymbol(DebugCtx *dctx, const FklString *s);
BdbWrapper bdbAddSymbol1(DebugCtx *dctx, const char *s);
BdbWrapper bdbParse(DebugCtx *dctx, const FklString *s);

BdbCodepoint *bdbGetCodepoint(DebugCtx *, const FklIns *ins);

const FklIns *bdbGetIns(DebugCtx *ctx,
        const FklString *filename,
        uint32_t line,
        BdbPutBpErrorType *);

BdbBp *bdbPutBp(DebugCtx *ctx,
        const FklString *filename,
        uint32_t line,
        BdbPutBpErrorType *);

BdbBp *bdbPutBp1(DebugCtx *ctx, const FklString *func_name);

FklVMvalueBpWrapper *bdbCreateBpWrapper(FklVM *exe, BdbBp *bp);
int bdbIsBpWrapper(const FklVMvalue *v);
BdbBp *bdbGetBp(const FklVMvalue *v);

const char *bdbGetPutBpErrorMsg(BdbPutBpErrorType t);

BdbBp *bdbDisableBp(DebugCtx *ctx, uint32_t idx);
BdbBp *bdbEnableBp(DebugCtx *ctx, uint32_t idx);
BdbBp *bdbDeleteBp(DebugCtx *ctx, uint32_t idx);
void bdbClearDeletedBp(DebugCtx *dctx);

BdbPos bdbBpPos(const BdbBp *bp);

int bdbGetCurLine(DebugCtx *dctx, BdbPos *line);

BdbWrapper bdbUpdateCurProc(DebugCtx *ctx, uint64_t *ppc);

static FKL_ALWAYS_INLINE const FklByteCodelnt *bdbBcl(BdbWrapper const proc) {
    return FKL_VM_CO(FKL_VM_PROC(bdbUnwrap(proc))->bcl);
}

static FKL_ALWAYS_INLINE int bdbIsError(BdbWrapper const v) {
    return fklIsVMvalueError(bdbUnwrap(v));
}

static FKL_ALWAYS_INLINE int bdbIsBox(BdbWrapper const v) {
    return FKL_IS_BOX(bdbUnwrap(v));
}

void bdbUnsetStepping(DebugCtx *);

void bdbSetStepOut(DebugCtx *);

void bdbSetStepUntil(DebugCtx *, uint32_t line);

typedef enum {
    BDB_STEP_INS_NEXT = 0,
    BDB_STEP_INS_CUR,
} BdbStepTargetPlacingType;

typedef enum { BDB_STEP_TYPE_INS = 0, BDB_STEP_TYPE_LINE } BdbSteppingType;

typedef enum {
    BDB_STEP_MODE_INTO,
    BDB_STEP_MODE_OVER,
} BdbSteppingMode;

typedef enum {
    BDB_INT3_STEPPING = 0x1,
    BDB_INT3_STEP_AT_BP = 0x2,
    BDB_INT3_GET_NEXT_INS = 0x4,
    BDB_INT3_STEP_LINE = 0x8,
} BdbInt3Flags;

void bdbSetStepIns(DebugCtx *,
        FklVM *exe,
        BdbStepTargetPlacingType,
        BdbSteppingMode,
        BdbSteppingType type);

void bdbSetAllThreadReadyToExit(FklVM *head);
void bdbWaitAllThreadExit(FklVM *head);
void bdbRestartDebugging(DebugCtx *ctx);

typedef enum {
    BDB_EVAL_ERR_UNABLE = 1,
    BDB_EVAL_ERR_IMPORT,
    BDB_EVAL_ERR_INVALID_EXP,
} BdbEvalErr;

BdbWrapper bdbCompileEvalExpression(DebugCtx *ctx,
        FklVM *,
        const FklString *exp,
        FklVMframe *frame,
        BdbEvalErr *is_complie_unabled);

BdbWrapper bdbCompileEvalExpression1(DebugCtx *ctx,
        FklVM *,
        BdbWrapper exp,
        FklVMframe *frame,
        BdbEvalErr *is_complie_unabled);

BdbWrapper bdbCallEvalProc(DebugCtx *ctx,
        FklVM *host_vm,
        FklVM *reached_thread,
        BdbWrapper proc,
        FklVMframe *frame);

void bdbSetReachedThread(DebugCtx *ctx, FklVM *);

FklVMvalue *bdbGetCurBacktrace(DebugCtx *ctx, FklVM *host_vm);
FklVMvalue *bdbErrInfo(DebugCtx *ctx, FklVM *host_Vm);

FklVMframe *bdbGetCurrentFrame(DebugCtx *ctx);

FklVM *bdbGetCurThread(DebugCtx *ctx);
void bdbSwitchCurThread(DebugCtx *ctx, uint32_t idx);
FklVMvalue *bdbListThreads(DebugCtx *ctx, FklVM *host_vm);

int bdbHasSymbol(DebugCtx *ctx, const FklString *s);

FklVMvalue *bdbCreateInsVec(FklVM *exe,
        DebugCtx *dctx,
        int is_cur_pc,
        uint64_t ins_pc,
        BdbWrapper proc);

FklVMvalue *bdbBoxStringify(FklVM *exe, BdbWrapper const v);

FklVMvalue *bdbCreateBpVec(FklVM *exe, DebugCtx *dctx, const BdbBp *bp);

#ifdef __cplusplus
}
#endif
#endif
