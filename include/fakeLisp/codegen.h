#ifndef FKL_CODEGEN_H
#define FKL_CODEGEN_H

#include "grammer.h"
#include "parser.h"
#include "pattern.h"
#include "symbol.h"
#include "vm.h"

#include <uv.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklCgEnvScope {
    uint32_t p;
    uint32_t start;
    uint32_t empty;
    uint32_t end;
    FklSymDefHashMap defs;
} FklCgEnvScope;

// FklCgEnvScopeVector
#define FKL_VECTOR_ELM_TYPE FklCgEnvScope
#define FKL_VECTOR_ELM_TYPE_NAME CgEnvScope
#include "cont/vector.h"

typedef enum {
    FKL_CODEGEN_ENV_SLOT_NONE = 0,
    FKL_CODEGEN_ENV_SLOT_OCC,
    FKL_CODEGEN_ENV_SLOT_REF,
} FklCgEnvSlot;

// FklCgEnvSlotVector
#define FKL_VECTOR_ELM_TYPE FklCgEnvSlot
#define FKL_VECTOR_ELM_TYPE_NAME CgEnvSlot
#include "cont/vector.h"

#define FKL_TOP_ENV_PROTO_ID (UINT32_MAX)
#define FKL_PRE_COMPILE_TOP_ENV_PROTO_ID (INT32_MAX)
#define FKL_INVALID_LIB_ID (UINT32_MAX)

typedef struct {
    FklVMvalue *sid;
    uint32_t scope;
    uint32_t prototypeId;
    uint32_t idx;
} FklPreDefRef;

// FklPreDefRefVector
#define FKL_VECTOR_ELM_TYPE FklPreDefRef
#define FKL_VECTOR_ELM_TYPE_NAME PreDefRef
#include "cont/vector.h"

// FklPredefHashMap
#define FKL_HASH_KEY_TYPE FklSidScope
#define FKL_HASH_VAL_TYPE uint8_t
#define FKL_HASH_ELM_NAME Predef
#define FKL_HASH_KEY_HASH                                                      \
    return fklHashCombine(fklVMvalueEqHashv((pk)->sid), (pk)->scope);
#define FKL_HASH_KEY_EQUAL(A, B)                                               \
    (A)->sid == (B)->sid && (A)->scope == (B)->scope
#include "cont/hash.h"

typedef struct {
    FklVMvalueLib *lib;
    uint32_t id;
} FklLibId;

// FklLibIdHashMap
#define FKL_HASH_KEY_TYPE const char *
#define FKL_HASH_VAL_TYPE FklLibId
#define FKL_HASH_ELM_NAME LibId
#define FKL_HASH_KEY_HASH return fklCharBufHash(*pk, strlen(*pk));
#define FKL_HASH_KEY_EQUAL(A, B) (!(strcmp(*(A), *(B))))
#define FKL_HASH_KEY_INIT(X, K) *(X) = fklZstrdup(*(K))
#define FKL_HASH_KEY_UNINIT(K) fklZfree((void *)*(K));
#include "cont/hash.h"

struct FklVMvalueCgEnv;

// 实际上是 FklVMvalueWeakHashEq 的别名
typedef struct FklVMvalueCgEnvWeakMap FklVMvalueCgEnvWeakMap;

FKL_VM_DEF_UD_STRUCT(FklVMvalueCgEnv, {
    FklUnboundVector uref;

    FklCgEnvSlotVector slots;
    FklCgEnvScopeVector scopes;

    uint32_t parent_scope;
    uint32_t proto_id;

    FklVMvalue *filename;
    FklVMvalue *name;
    uint64_t line;

    struct FklVMvalueCgEnv *prev;
    struct FklVMvalueCgMacroScope *macros;
    FklSymDefHashMap refs;
    FklPredefHashMap pdef;
    FklValueTable konsts;
    struct FklPreDefRefVector ref_pdef;
    FklValueVector child_proc_protos;
    FklLibIdHashMap used_libraries;
    int is_debugging;

    FklVMvalueProto *proto;
    FklVMvalueCgEnvWeakMap *proto_env_map;
});

typedef void (*FklResolveRefToDefCb)(const FklVarRefDef *ref,
        const FklSymDefHashMapElm *def,
        const FklUnbound *uref,
        FklVMvalueProto *,
        void *args);

typedef struct {
    FklVM *vm;
    FklVMvalueCgEnv *top_env;
    int no_refs_to_builtins;
    FklResolveRefToDefCb resolve_ref_to_def_cb;
    void *resolve_ref_to_def_cb_args;
    FklVMvalueWeakHashEq *weak_refs;
} FklResolveRefArgs;

FKL_NODISCARD
FklVMvalueProto *fklCreateVMvalueProto2(FklVM *exe, FklVMvalueCgEnv *env);

FKL_NODISCARD
FklVMvalueProto *fklCreateVMvalueProto3(FklVM *exe,
        FklVMvalueCgEnv *env,
        const FklResolveRefArgs *args);

#define FKL_CREATE_VMVALUE_PROTO(EXE, ENV, ...)                                \
    (fklCreateVMvalueProto3(EXE,                                               \
            ENV,                                                               \
            &(const FklResolveRefArgs){ __VA_ARGS__ }))

void fklResolveRef(FklVMvalueCgEnv *env,
        uint32_t scope,
        const FklResolveRefArgs *args);

FKL_VM_DEF_UD_STRUCT(FklVMvalueCgMacro, {
    FklVMvalue *pattern;
    FklVMvalue *proc;
});

typedef FklVMvalueHash FklVMvalueCgMacroHashMap;

// 我们需要一种只保存用于替换的值的对象
// 使得其能够是唯一的对象
// 不使用 BOX 是为了解释器编写过程中的类型安全
FKL_VM_DEF_UD_STRUCT(FklVMvalueCgRpl, { FklVMvalue *value; });

typedef FklVMvalueHash FklVMvalueCgRplHashMap;

FKL_VM_DEF_UD_STRUCT(FklVMvalueCgMacroScope, {
    struct FklVMvalueCgMacroScope *prev;
    FklVMvalueCgRplHashMap *replacements;
    FklVMvalueCgMacroHashMap *macros;
});

typedef struct {
    uint32_t idx;
    uint32_t oidx;
} FklCgExportIdx;

// FklCgExportSidIdxHashMap
#define FKL_HASH_KEY_TYPE FklVMvalue *
#define FKL_HASH_VAL_TYPE FklCgExportIdx
#define FKL_HASH_ELM_NAME CgExportSidIdx
#define FKL_HASH_KEY_HASH return fklVMvalueEqHashv(*pk);
#include "cont/hash.h"

typedef struct FklSimpleProdAction {
    const char *const name;
    FklProdActionFunc const func;
    int (*const check)(FklVMvalue *rest[], size_t rest_len);
} FklSimpleProdAction;

typedef enum {
    FKL_CG_PROD_ACT_CTX_TYPE_OTHER = 0,
    FKL_CG_PROD_ACT_CTX_TYPE_SIMPLE,
    FKL_CG_PROD_ACT_CTX_TYPE_CUSTOM,
} FklCgProdActCtxType;

typedef enum {
    FKL_CG_RMACRO_NONE = 0,
    FKL_CG_RMACRO_ADD_PROD,
    FKL_CG_RMACRO_ADD_IGNORE,
    FKL_CG_RMACRO_ADD_DELIM,
} FklCgRmacroOpcode;

typedef struct {
    FklGrammerSymType type;
    FklVMvalue *v;
} FklCgRmacroGraSym;

FKL_VM_DEF_UD_STRUCT(FklVMvalueCgRmacroProd, {
    FklVMvalue *left;
    FklVMvalue *action_type;
    FklVMvalue *action;
    uint8_t add_extra;
    uint32_t len;
    FklCgRmacroGraSym syms[];
});

typedef struct {
    FklCgRmacroOpcode op;
    FklVMvalue *args;
} FklCgRmacroCmd;

// 读取器宏的本质是一系列对语法的修改的指令
FKL_VM_DEF_UD_STRUCT(FklVMvalueCgRmacro, {
    uint64_t len;
    FklCgRmacroCmd cmds[];
});

typedef FklVMvalueHash FklVMvalueCgRmacroHashMap;

FKL_VM_DEF_UD_STRUCT(FklVMvalueCgLib, {
    FklCgExportSidIdxHashMap exports;
    FklVMvalueCgMacroHashMap *macros;
    FklVMvalueCgRplHashMap *replacements;
    FklVMvalueCgRmacroHashMap *rmacros;

    FklVMvalue *rp;
    FklVMvalueLib *lib;
});

typedef FklVMvalueHash FklVMvalueCgLibs;

typedef enum {
    FKL_CODEGEN_PATTERN_BEGIN = 0,
    FKL_CODEGEN_PATTERN_LOCAL,
    FKL_CODEGEN_PATTERN_LET0,
    FKL_CODEGEN_PATTERN_LET1,
    FKL_CODEGEN_PATTERN_LET80,
    FKL_CODEGEN_PATTERN_LET81,
    FKL_CODEGEN_PATTERN_LET82,
    FKL_CODEGEN_PATTERN_NAMED_LET0,
    FKL_CODEGEN_PATTERN_NAMED_LET1,
    FKL_CODEGEN_PATTERN_LETREC0,
    FKL_CODEGEN_PATTERN_LETREC1,

    FKL_CODEGEN_PATTERN_DO0,
    FKL_CODEGEN_PATTERN_DO0N,
    FKL_CODEGEN_PATTERN_DO1,
    FKL_CODEGEN_PATTERN_DO1N,
    FKL_CODEGEN_PATTERN_DO11,
    FKL_CODEGEN_PATTERN_DO11N,

    FKL_CODEGEN_PATTERN_DEFUN,
    FKL_CODEGEN_PATTERN_DEFINE,

    FKL_CODEGEN_PATTERN_DEFUN_CONST,
    FKL_CODEGEN_PATTERN_DEFCONST,

    FKL_CODEGEN_PATTERN_SETQ,
    FKL_CODEGEN_PATTERN_CHECK,
    FKL_CODEGEN_PATTERN_QUOTE,
    FKL_CODEGEN_PATTERN_UNQUOTE,
    FKL_CODEGEN_PATTERN_QSQUOTE,
    FKL_CODEGEN_PATTERN_LAMBDA,
    FKL_CODEGEN_PATTERN_AND,
    FKL_CODEGEN_PATTERN_OR,
    FKL_CODEGEN_PATTERN_COND,
    FKL_CODEGEN_PATTERN_IF0,
    FKL_CODEGEN_PATTERN_IF1,
    FKL_CODEGEN_PATTERN_WHEN,
    FKL_CODEGEN_PATTERN_UNLESS,
    FKL_CODEGEN_PATTERN_LOAD,
    FKL_CODEGEN_PATTERN_IMPORT_PREFIX,
    FKL_CODEGEN_PATTERN_IMPORT_ONLY,
    FKL_CODEGEN_PATTERN_IMPORT_ALIAS,
    FKL_CODEGEN_PATTERN_IMPORT_EXCEPT,
    FKL_CODEGEN_PATTERN_IMPORT_COMMON,
    FKL_CODEGEN_PATTERN_IMPORT,
    FKL_CODEGEN_PATTERN_IMPORT_NONE,
    FKL_CODEGEN_PATTERN_EXPORT_SINGLE,
    FKL_CODEGEN_PATTERN_EXPORT_NONE,
    FKL_CODEGEN_PATTERN_EXPORT,
    FKL_CODEGEN_PATTERN_DEFMACRO,
    FKL_CODEGEN_PATTERN_DEF_READER_MACROS,
    FKL_CODEGEN_PATTERN_COND_COMPILE,
    FKL_CODEGEN_PATTERN_NUM,
} FklCgPatternEnum;

typedef enum {
    FKL_CODEGEN_SUB_PATTERN_UNQUOTE = 0,
    FKL_CODEGEN_SUB_PATTERN_UNQTESP,
    FKL_CODEGEN_SUB_PATTERN_DEFINE,
    FKL_CODEGEN_SUB_PATTERN_DEFMACRO,
    FKL_CODEGEN_SUB_PATTERN_IMPORT,
    FKL_CODEGEN_SUB_PATTERN_AND,
    FKL_CODEGEN_SUB_PATTERN_OR,
    FKL_CODEGEN_SUB_PATTERN_NOT,
    FKL_CODEGEN_SUB_PATTERN_EQ,
    FKL_CODEGEN_SUB_PATTERN_MATCH,
    FKL_CODEGEN_SUB_PATTERN_NUM,
} FklCgSubPatternEnum;

#define FKL_BUILTIN_REPLACEMENT_NUM (7)
#define FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM (17)
#define FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM (12)

#define FKL_CODEGEN_SYMBOL_MAP                                                 \
    XX(orig)                                                                   \
    XX(rest)                                                                   \
    XX(name)                                                                   \
    XX(value)                                                                  \
    XX(cond)                                                                   \
    XX(args)                                                                   \
    XX(arg0)                                                                   \
    XX(arg1)                                                                   \
    XX(custom)                                                                 \
    XX(builtin)                                                                \
    XX(simple)                                                                 \
    XX(replace)

typedef struct FklPmatchStorage {
    struct FklPmatchStorage *next;
    const FklPmatchHashMap *ht;
} FklPmatchStorage;

typedef struct FklCgCtx {
    struct FklCgActVector *action_vector;
    struct FklVMvalueCgEnv *main_env;
    struct FklVMvalueCgInfo *main_info;
    struct FklCgErrorState *error_state;
    FklPmatchStorage *ht_storage;
    FklPmatchRes cur_exp;

    char *cwd;

    // TODO: rename to main_dir
    char *main_file_real_path_dir;
    const char *cur_file_dir;

    FklValueVector *bcl_vector;

    FklVMvalueCgLibs *libraries;

    FklVMvalueCgLibs *macro_libraries;

    // for dll lib
    FklVMvalueHash *hash_singleton;

    FklVM *vm;

    FklGrammer builtin_g;

#define XX(A) FklVMvalue *builtin_sym_##A;
    FKL_CODEGEN_SYMBOL_MAP
#undef XX

    FklVMvalueLnt *lnt;

    FklVMvalueCgEnvWeakMap *proto_env_map;

    FklVMvalue *builtin_replacement_id[FKL_BUILTIN_REPLACEMENT_NUM];

    FklVMvalue *builtin_pattern_node[FKL_CODEGEN_PATTERN_NUM];
    FklVMvalue *builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_NUM];

    FklVMvalue *builtin_prod_action_id[FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM];
    FklVMvalue *simple_prod_action_id[FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM];
} FklCgCtx;

FKL_VM_DEF_UD_STRUCT(FklVMvalueCgGrammer, { FklGrammer g; });

typedef enum {
    FKL_FILE_NONE,
    FKL_FILE_SCRIPT,
    FKL_FILE_PACKAGE,
    FKL_FILE_PRECOMPILE,
    FKL_FILE_DLL,
} FklFileType;

FKL_VM_DEF_UD_STRUCT(FklVMvalueCgInfo, {
    FklVMvalueLnt *lnt;
    struct FklVMvalueCgInfo *prev;
    char *filename;
    char *realpath;
    char *dir;
    uint64_t curline;
    FklVMvalue *fid;

    FklVMvalueCgGrammer *g;
    FklVMvalueCgRmacroHashMap *rmacros;

    FklVMvalueCgEnv *global_env;
    FklCgExportSidIdxHashMap exports;

    FklVMvalueCgMacroHashMap *export_macros;
    FklVMvalueCgRplHashMap *export_replacement;
    FklVMvalueCgRmacroHashMap *export_rmacros;

    FklVMvalueCgLibs *libraries;
    FklVMvalue *user_data;

    unsigned int is_lib : 1;
    unsigned int is_macro : 1;
});

typedef struct {
    size_t size;
    void (*finalize)(void *);
    void (*atomic)(FklVMgc *, void *);
} FklCgActCtxMt;

typedef struct FklCgActCtx {
    const FklCgActCtxMt *t;
    alignas(void *) uint8_t d[];
} FklCgActCtx;

typedef struct FklCgErrorState {
    FklVMvalue *error;
    size_t line;
    FklVMvalue *fid;
} FklCgErrorState;

typedef struct {
    void *data;
    FklCgCtx *ctx;
    FklVMvalueCgInfo *info;
    FklVMvalueCgEnv *env;
    uint32_t scope;
    FklVMvalueCgMacroScope *cms;
    FklValueVector *bcl_vec;
    FklVMvalue *fid;
    uint64_t line;
    struct FklCgAct *prev;
} FklCgActCbArgs;

typedef FklVMvalue *(*FklCgActCb)(const FklCgActCbArgs *args);

typedef int (*FklCgGetNextExpCb)(FklCgCtx *ctx, void *, FklPmatchRes *res);

typedef struct {
    FklCgGetNextExpCb get_next_exp;
    void (*finalize)(void *);
    void (*atomic)(FklVMgc *, void *);
} FklNextExpressionMethodTable;

typedef struct {
    const FklNextExpressionMethodTable *t;
    void *context;
    uint8_t must_has_retval;
} FklCgNextExp;

typedef struct FklCgAct {
    struct FklCgAct *prev;
    FklCgActCb cb;
    FklCgActCtx *ctx;
    FklVMvalueCgEnv *env;
    FklVMvalueCgMacroScope *macros;
    FklVMvalueCgInfo *info;
    FklCgNextExp *exps;

    uint32_t scope;
    uint64_t curline;

    FklValueVector bcl_vector;
} FklCgAct;

// FklCgActVector
#define FKL_VECTOR_ELM_TYPE FklCgAct *
#define FKL_VECTOR_ELM_TYPE_NAME CgAct
#include "cont/vector.h"

// FklCgExpQueue
#define FKL_QUEUE_ELM_TYPE FklPmatchRes
#define FKL_QUEUE_ELM_TYPE_NAME CgExp
#include "cont/queue.h"

void fklInitProdActionList(FklCgCtx *ctx);

void fklInitCgCtx(FklCgCtx *ctx, char *main_file_real_path_dir, FklVM *vm);

void fklRegisterCgCtx(FklCgCtx *ctx);
void fklInitCgCtxExceptPattern(FklCgCtx *ctx, FklVM *vm);

void fklUnregisterCgCtx(FklCgCtx *ctx);
void fklUninitCgCtx(FklCgCtx *ctx);

typedef struct {
    FklVMvalueCgLibs *libraries;
    FklVMvalueCgMacroScope *macro_scope;
    FklVMvalue *user_data;

    int8_t is_debugging;
    int8_t is_lib;
    int8_t is_macro;
    int8_t is_main;
    int8_t is_precompile;
    int8_t inherit_grammer;
} FklCgInfoArgs;

FKL_VM_DEF_UD_STRUCT(FklVMvalueSimpleActCtx, {
    const FklSimpleProdAction *mt;
    FklVMvalue *vec;
});

FKL_VM_DEF_UD_STRUCT(FklVMvalueCustomActCtx, {
    FklVMvalue *proc;

    FklVMvalue *doller_s;
    FklVMvalue *line_s;
    uint64_t actual_len;
    FklVMvalue *dollers[];
});

FklVMvalueVec *fklCreateCgNamesVec(FklVM *vm,
        const FklCgExportSidIdxHashMap *map);

FklVMvalue *fklResolveLibPath(FklVM *vm,
        const char *main_dir,
        FklVMvalue *name,
        FklFileType *ft);

int fklIsVMvalueCgLibs(const FklVMvalue *v);
FklVMvalueCgLibs *fklCreateVMvalueCgLibs(FklVM *vm);

FklVMvalueCgLib *fklCreateVMvalueCgLib(FklVM *vm, FklVMvalue *rp_s);
int fklIsVMvalueCgLib(const FklVMvalue *v);
static FKL_ALWAYS_INLINE FklVMvalueCgLib *fklVMvalueCgLib(const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueCgLib(v));
    return FKL_TYPE_CAST(FklVMvalueCgLib *, v);
}

FklVMvalueCgLib *fklVMvalueCgLibsGet(const FklCgCtx *c,
        const FklVMvalueCgLibs *,
        const char *rp);

FklVMvalueCgLib *fklVMvalueCgLibsGet1(const FklVMvalueCgLibs *, FklVMvalue *rp);

FklVMvalueCgLib *
fklVMvalueCgLibsAdd(FklCgCtx *c, FklVMvalueCgLibs *, const char *rp);
FklVMvalueCgLib *
fklVMvalueCgLibsAdd1(FklVM *vm, FklVMvalueCgLibs *libs, FklVMvalue *rp_s);

void fklVMvalueCgLibsRemove(FklCgCtx *c, FklVMvalueCgLibs *, const char *rp);

const char *fklCgLibRp(const FklVMvalueCgLib *c);

FklVMvalue *fklCgRealpathToModuleName(FklCgCtx *ctx, const char *rp);

FklVMvalueCgEnvWeakMap *fklCreateVMvalueCgEnvWeakMap(FklVM *vm);
FklVMvalueCgEnv *fklVMvalueCgEnvWeakMapGet(const FklVMvalueCgEnvWeakMap *,
        const FklVMvalueProto *p);
void fklVMvalueCgEnvWeakMapInsert(FklVMvalueCgEnvWeakMap *,
        const FklVMvalueProto *,
        const FklVMvalueCgEnv *env);

int fklIsVMvalueCgInfo(const FklVMvalue *v);
FklVMvalueCgInfo *fklCreateVMvalueCgInfo(FklCgCtx *ctx,
        FklVMvalueCgInfo *prev,
        const char *filename,
        const FklCgInfoArgs *args);

FklVMvalue *fklGenExpressionCode(FklCgCtx *ctx,
        FklVMvalue *exp,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info);
FklVMvalue *fklGenExpressionCodeWithAction(FklCgCtx *ctx, FklCgAct *);
FklVMvalue *fklGenExpressionCodeWithFp(FklCgCtx *ctx,
        FILE *,
        FklVMvalueCgInfo *info,
        FklVMvalueCgEnv *env);

FklVMvalue *fklGenExpressionCodeExt(FklCgCtx *, size_t, FklCgAct *const *);

FklCgAct *fklMakeImportAct(FklCgCtx *,
        FklVMvalue *name,
        FklFileType,
        FklVMvalue *rp,
        FklVMvalueCgInfo *info,
        FklCgAct *);
FklCgAct *fklMakeCollectAct(FklCgCtx *, FklVMvalueCgInfo *info, FklCgAct *prev);

FklSymDefHashMapElm *
fklFindSymbolDef(FklVMvalue *id, uint32_t scope, const FklVMvalueCgEnv *env);
FklSymDefHashMapElm *fklGetCgDefByIdInScope(FklVMvalue *id,
        uint32_t scope,
        const FklVMvalueCgEnv *env);

FklCgEnvScope *fklCgEnvScopeGet(const FklVMvalueCgEnv *env, uint32_t scope_id);

void fklPrintCgError(FklCgCtx *ctx,
        const FklVMvalueCgInfo *info,
        FklCodeBuilder *cb);

void fklPrintUndefinedRef(const FklVMvalueCgEnv *env, FklCodeBuilder *cb);

FklSymDefHashMapElm *fklAddCgBuiltinRefBySid(FklVMvalue *id,
        FklVMvalueCgEnv *env);
uint32_t fklAddCgRefBySidRetIndex(FklVMvalue *id,
        FklVMvalueCgEnv *env,
        FklVMvalue *fid,
        uint64_t line,
        uint32_t assign);
FklSymDefHashMapElm *fklAddCgRefBySid(FklVMvalue *id,
        FklVMvalueCgEnv *env,
        FklVMvalue *fid,
        uint64_t line,
        uint32_t assign);
FklSymDef *fklGetCgRefBySid(FklVMvalue *id, FklVMvalueCgEnv *env);

FklSymDef *
fklAddCgDefBySid(FklVMvalue *id, uint32_t scope, FklVMvalueCgEnv *env);

void fklAddCgPreDefBySid(FklVMvalue *id,
        uint32_t scope,
        uint8_t isConst,
        FklVMvalueCgEnv *env);

uint8_t *
fklGetCgPreDefBySid(FklVMvalue *id, uint32_t scope, FklVMvalueCgEnv *env);
void fklAddCgRefToPreDef(FklVMvalue *id,
        uint32_t scope,
        uint32_t prototypeId,
        uint32_t idx,
        FklVMvalueCgEnv *env);
void fklResolveCgPreDef(FklVMvalue *, uint32_t scope, FklVMvalueCgEnv *env);
void fklClearCgPreDef(FklVMvalueCgEnv *env);

int fklIsSymbolDefined(FklVMvalue *sid,
        uint32_t scope,
        const FklVMvalueCgEnv *);

int fklIsRplDefined(FklVMvalue *sid, FklVMvalueCgEnv *);

int fklIsVMvalueCgEnv(const FklVMvalue *);

typedef struct {
    FklVMvalueCgEnv *prev_env;
    FklVMvalueCgMacroScope *prev_ms;
    uint32_t parent_scope;
    FklVMvalue *filename;
    FklVMvalue *name;
    uint64_t line;
} FklCgEnvCreateArgs;

FklVMvalueCgEnv *fklCreateVMvalueCgEnv(const FklCgCtx *ctx,
        const FklCgEnvCreateArgs *args);
FklLibId *fklVMvalueCgEnvAddUsedLib(FklVMvalueCgEnv *env,
        const char *rp,
        FklVMvalueLib *lib);

void fklInitCgScriptLib(const FklCgCtx *ctx,
        FklVMvalueCgLib *lib,
        FklVMvalue *mod_name,
        FklVMvalueCgInfo *codegen,
        FklVMvalue *proc);

FklCgDllLibInitExportCb fklGetCgInitExportFunc(uv_lib_t *dll);

void fklInitCgDllLib(const FklCgCtx *ctx,
        FklVMvalue *name,
        FklVMvalueCgLib *lib,
        FklVMvalue *rp,
        uv_lib_t dll,
        FklCgDllLibInitExportCb init);

FKL_DEPRECATED
void fklClearCgLibMacros(FklVMvalueCgLib *lib);
FKL_DEPRECATED
void fklClearCgLibMacros2(const FklCgCtx *ctx);

FklVMvalueCgMacro *fklCreateVMvalueCgMacro(const FklCgCtx *c,
        FklVMvalue *pattern,
        FklVMvalue *proc);
int fklIsVMvalueCgMacro(const FklVMvalue *v);
FklVMvalueCgMacro *fklVMvalueCgMacro(const FklVMvalue *r);

FklVMvalueCgMacroHashMap *fklCreateVMvalueCgMacroHashMap(const FklCgCtx *c);
FklValueHashMapElm *fklCgMacroHashMapGet(const FklVMvalueCgMacroHashMap *,
        const FklVMvalue *s);
FklValueHashMapElm *fklCgMacroHashMapRef1(FklVMvalueCgMacroHashMap *,
        const FklVMvalue *s);

FklVMvalueCgRpl *fklCreateVMvalueCgRpl(const FklCgCtx *c, FklVMvalue *value);
int fklIsVMvalueCgRpl(const FklVMvalue *v);
FklVMvalueCgRpl *fklVMvalueCgRpl(const FklVMvalue *r);

FklVMvalueCgRplHashMap *fklCreateVMvalueCgRplHashMap(const FklCgCtx *c);
FklVMvalueCgRpl *fklCgRplHashMapGet(const FklVMvalueCgRplHashMap *,
        const FklVMvalue *sym);
void fklCgRplHashMapSet(FklVMvalueCgRplHashMap *,
        const FklVMvalue *sym,
        FklVMvalueCgRpl *rep);

FklVMvalueCgRmacroProd *fklCreateVMvalueCgRmacroProd(FklVM *c,
        FklVMvalue *left,
        FklVMvalue *action_type,
        FklVMvalue *action,
        int add_extra,
        uint32_t len);

int fklIsVMvalueCgRmacroProd(const FklVMvalue *v);
FklVMvalueCgRmacroProd *fklVMvalueCgRmacroProd(const FklVMvalue *r);

FklVMvalueCgRmacro *fklCreateVMvalueCgRmacro(FklVM *c, uint64_t len);
int fklIsVMvalueCgRmacro(const FklVMvalue *v);
FklVMvalueCgRmacro *fklVMvalueCgRmacro(const FklVMvalue *r);

FKL_NODISCARD
int fklExecuteCgRmacro(FklCgCtx *ctx,
        FklVMvalueCgInfo *info,
        FklVMvalue *name,
        FklVMvalueCgRmacro *r);
const char *fklGetCgRmacroOpName(FklCgRmacroOpcode op);

FklVMvalueCgRmacroHashMap *fklCreateVMvalueCgRmacroHashMap(const FklCgCtx *c);

FklValueHashMapElm *fklCgRmacroHashMapGet(const FklVMvalueCgRmacroHashMap *,
        const FklVMvalue *s);
FklValueHashMapElm *fklCgRmacroHashMapRef1(FklVMvalueCgRmacroHashMap *,
        const FklVMvalue *s);
FklVMvalueCgRmacro *fklCgRmacroHashMapDel(FklVMvalueCgRmacroHashMap *,
        const FklVMvalue *s);

FklVMvalueCgGrammer *fklCreateVMvalueCgGrammer(const FklCgCtx *c);
int fklIsVMvalueCgGrammer(const FklVMvalue *v);
FklVMvalueCgGrammer *fklVMvalueCgGrammer(const FklVMvalue *r);

int fklIsVMvalueCgMacroScope(const FklVMvalue *v);
FklVMvalueCgMacroScope *fklCreateVMvalueCgMacroScope(const FklCgCtx *c,
        FklVMvalueCgMacroScope *prev);

static inline void fklPushCgPmatchStorage(FklCgCtx *ctx, FklPmatchStorage *s) {
    s->next = ctx->ht_storage;
    ctx->ht_storage = s;
}

static inline void fklPopCgPmatchStorage(FklCgCtx *ctx,
        const FklPmatchStorage *s) {
    FKL_ASSERT(ctx->ht_storage == s);
    FklPmatchStorage *ss = ctx->ht_storage;
    ctx->ht_storage = ss->next;
    ss->next = NULL;
}

FklVMvalue *fklTryExpandCgMacroOnce(FklCgCtx *ctx,
        const FklPmatchRes *exp,
        const FklVMvalueCgInfo *,
        const FklVMvalueCgMacroScope *macros);

FklVMvalue *fklTryExpandCgMacro(FklCgCtx *ctx,
        const FklPmatchRes *exp,
        const FklVMvalueCgInfo *,
        const FklVMvalueCgMacroScope *macros);

int fklIsCgRmacroBuiltinActionValid(const FklCgCtx *ctx, const FklVMvalue *id);

FklVMvalueSimpleActCtx *fklCreateVMvalueSimpleActCtx(FklVM *v, FklVMvalue *act);
FklVMvalueSimpleActCtx *fklCreateVMvalueSimpleActCtx1(const FklCgCtx *cg_ctx,
        FklVMvalue *action_ast);

FklVMvalueCustomActCtx *fklCreateCgRmacroCustomAction(FklCgCtx *cg_ctx,
        FklVMvalueCgRmacroProd *prod);

FklVMvalueCustomActCtx *fklCreateVMvalueCustomActCtx(FklVM *vm,
        size_t actual_len);

int fklIsVMvalueCustomActCtx(const FklVMvalue *v);

static FKL_ALWAYS_INLINE FklVMvalueCustomActCtx *fklVMvalueCustomActCtx(
        const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueCustomActCtx(v));
    return (FklVMvalueCustomActCtx *)v;
}

FklVMvalueCgRmacro *fklCgParseReaderMacroDefine(FklCgCtx *ctx,
        FklCgActVector *actions,
        FklVMvalue *rest,
        FklVMvalueCgInfo *info,
        FklVMvalueCgMacroScope *ms);

int fklIsVMvalueSimpleActCtx(const FklVMvalue *v);

static FKL_ALWAYS_INLINE FklVMvalueSimpleActCtx *fklVMvalueSimpleActCtx(
        const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueSimpleActCtx(v));
    return (FklVMvalueSimpleActCtx *)v;
}

#ifdef __cplusplus
}
#endif
#endif
