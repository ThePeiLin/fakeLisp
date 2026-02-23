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

#define FKL_CODEGEN_ENV_SLOT_OCC (1)
#define FKL_CODEGEN_ENV_SLOT_REF (2)
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

	// XXX: 将 slot_flags 改成一个 vector
	FKL_DEPRECATED
    uint8_t *slot_flags;
    FklCgEnvScopeVector scopes;
    // FklCgEnvScope *scopes;
    // uint32_t scope;
    uint32_t lcount;
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
    fklCreateVMvalueProto3(EXE, ENV, &(const FklResolveRefArgs){ __VA_ARGS__ })

void fklResolveRef(FklVMvalueCgEnv *env,
        uint32_t scope,
        const FklResolveRefArgs *args);

typedef struct FklCgMacro {
    struct FklCgMacro *next;
    FklVMvalue *pattern;
    FklVMvalue *proc;
} FklCgMacro;

// FklReplacementHashMap
#define FKL_HASH_KEY_TYPE FklVMvalue *
#define FKL_HASH_VAL_TYPE FklVMvalue *
#define FKL_HASH_ELM_NAME Replacement
#define FKL_HASH_KEY_HASH return fklVMvalueEqHashv(*pk);
#define FKL_HASH_VAL_INIT(V, X)                                                \
    {                                                                          \
        *(V) = *(X);                                                           \
    }
#include "cont/hash.h"

#define FKL_HASH_KEY_TYPE FklVMvalue *
#define FKL_HASH_VAL_TYPE FklCgMacro *
#define FKL_HASH_ELM_NAME Macro
#define FKL_HASH_KEY_HASH return fklVMvalueEqHashv(*pk);
#define FKL_HASH_VAL_INIT(V, X)                                                \
    {                                                                          \
        *(V) = *(X);                                                           \
    }
#define FKL_HASH_VAL_UNINIT(V)                                                 \
    {                                                                          \
        FklCgMacro *head = *(V);                                               \
        *(V) = NULL;                                                           \
        while (head) {                                                         \
            head->pattern = NULL;                                              \
            head->proc = NULL;                                                 \
            FklCgMacro *next = head->next;                                     \
            fklZfree(head);                                                    \
            head = next;                                                       \
        }                                                                      \
    }
#include "cont/hash.h"

FKL_VM_DEF_UD_STRUCT(FklVMvalueCgMacroScope, {
    struct FklVMvalueCgMacroScope *prev;
    FklReplacementHashMap *replacements;
    FklMacroHashMap *macros;
});

typedef enum {
    FKL_CODEGEN_LIB_SCRIPT = 0,
    FKL_CODEGEN_LIB_DLL = 1,
    FKL_CODEGEN_LIB_UNINIT = 2,
} FklCgLibType;

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
    FKL_CODEGEN_PROD_BUILTIN = 0,
    FKL_CODEGEN_PROD_SIMPLE,
    FKL_CODEGEN_PROD_CUSTOM,
    FKL_CODEGEN_PROD_REPLACE,
} FklCgProdActionType;

#define FKL_GRAMMER_GROUP_INITED (0x1)
#define FKL_GRAMMER_GROUP_HAS_OUTER_REF (0x2)
typedef struct {
    FklGrammer g;
    int flags;
} FklGrammerProdGroupItem;

// FklGraProdGroupHashMap
#define FKL_HASH_KEY_TYPE FklVMvalue *
#define FKL_HASH_VAL_TYPE FklGrammerProdGroupItem
#define FKL_HASH_ELM_NAME GraProdGroup
#define FKL_HASH_KEY_HASH return fklVMvalueEqHashv(*pk);
#define FKL_HASH_VAL_UNINIT(V)                                                 \
    {                                                                          \
        fklUninitGrammer(&(V)->g);                                             \
    }
#include "cont/hash.h"

typedef struct FklCgLib {
    FklCgExportSidIdxHashMap exports;
    FklMacroHashMap *macros;
    FklReplacementHashMap *replacements;
    FklGraProdGroupHashMap *named_prod_groups;

    FklVMvalueLib *lib;
    FklCgLibType type;
} FklCgLib;

typedef struct FklVMvalueCgLibs FklVMvalueCgLibs;

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
    char *main_file_real_path_dir;
    const char *cur_file_dir;

    FklValueVector *bcl_vector;

    FklVMvalueCgLibs *libraries;

    FklVMvalueCgLibs *macro_libraries;

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

FKL_VM_DEF_UD_STRUCT(FklVMvalueCgInfo, {
    FklVMvalueLnt *lnt;
    struct FklVMvalueCgInfo *prev;
    char *filename;
    char *realpath;
    char *dir;
    uint64_t curline;
    FklVMvalue *fid;
    struct {
        FklGrammer *self_g;
        FklGraProdGroupHashMap self_prod_groups;
    };
    FklGrammer **g;
    FklGraProdGroupHashMap *prod_groups;
    FklVMvalueCgEnv *global_env;
    uint64_t epc;
    FklCgExportSidIdxHashMap exports;

    FklMacroHashMap *export_macros;
    FklReplacementHashMap *export_replacement;
    FklGraProdGroupHashMap *export_prod_groups;

    FklVMvalueCgLibs *libraries;
    unsigned int is_lib : 1;
    unsigned int is_macro : 1;
});

typedef struct {
    size_t size;
    void (*finalize)(void *);
    void (*atomic)(FklVMgc *, void *);
} FklCgActCtxMethodTable;

typedef struct FklCgActCtx {
    const FklCgActCtxMethodTable *t;
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

void fklInitProdActionList(FklCgCtx *ctx);

void fklInitCgCtx(FklCgCtx *ctx, char *main_file_real_path_dir, FklVM *vm);

void fklRegisterCgCtx(FklCgCtx *ctx);
void fklInitCgCtxExceptPattern(FklCgCtx *ctx, FklVM *vm);

void fklUnregisterCgCtx(FklCgCtx *ctx);
void fklUninitCgCtx(FklCgCtx *ctx);

typedef struct {
    FklVMvalueCgLibs *libraries;
    FklVMvalueCgMacroScope *macro_scope;

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
    size_t actual_len;
    FklVMvalue *dollers[];
});

int fklIsVMvalueCgLibs(const FklVMvalue *v);
FklVMvalueCgLibs *fklCreateVMvalueCgLibs(FklVM *vm);
FklCgLib *fklVMvalueCgLibsGet(const FklVMvalueCgLibs *, const char *rp);
FklCgLib *fklVMvalueCgLibsAdd(FklVMvalueCgLibs *, const char *rp);
void fklVMvalueCgLibsRemove(FklVMvalueCgLibs *, const char *rp);

size_t fklVMvalueCgLibsCount(FklVMvalueCgLibs *v);
FklCgLib *fklVMvalueCgLibsIter(const FklVMvalueCgLibs *v);

FklCgLib *fklCgLibNext(const FklCgLib *c);
const char *fklCgLibRp(const FklCgLib *c);

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
        FklVMvalueCgEnv *cur_env,
        FklVMvalueCgInfo *codegen);
FklVMvalue *fklGenExpressionCodeWithAction(FklCgCtx *ctx,
        FklCgAct *,
        FklVMvalueCgInfo *codegen);
FklVMvalue *fklGenExpressionCodeWithFp(FklCgCtx *ctx,
        FILE *,
        FklVMvalueCgInfo *codegen,
        FklVMvalueCgEnv *cur_env);

FklVMvalue *fklGenExpressionCodeWithFpForPrecompile(FklCgCtx *ctx,
        FILE *fp,
        FklVMvalueCgInfo *codegen,
        FklVMvalueCgEnv *cur_env);

FklSymDefHashMapElm *
fklFindSymbolDef(FklVMvalue *id, uint32_t scope, const FklVMvalueCgEnv *env);
FklSymDefHashMapElm *fklGetCgDefByIdInScope(FklVMvalue *id,
        uint32_t scope,
        const FklVMvalueCgEnv *env);

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

int fklIsReplacementDefined(FklVMvalue *sid, FklVMvalueCgEnv *);

FklVMvalue *fklGetReplacement(FklVMvalue *sid, const FklReplacementHashMap *);

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
        FklCgLib *lib,
        FklVMvalueCgInfo *codegen,
        FklVMvalue *proc,
        uint64_t epc);

FklCgDllLibInitExportCb fklGetCgInitExportFunc(uv_lib_t *dll);

void fklInitCgDllLib(const FklCgCtx *ctx,
        FklCgLib *lib,
        uv_lib_t dll,
        FklCgDllLibInitExportCb init);

void fklClearCgLibMacros(FklCgLib *lib);
void fklClearCgLibMacros2(const FklCgCtx *ctx);

FklCgMacro *
fklCreateCgMacro(FklVMvalue *pattern, FklVMvalue *proc, FklCgMacro *next);

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

FklGrammerProduction *fklCreateCustomActionProd(FklCgCtx *cg_ctx,
        struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms);
int fklIsCustomActionProd(const FklGrammerProduction *p);

FklGrammerProduction *fklCreateSimpleActionProd(FklCgCtx *cg_ctx,
        struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        struct FklVMvalue *action);
int fklIsSimpleActionProd(const FklGrammerProduction *p);

FklGrammerProduction *fklCreateReplaceActionProd(struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        FklVMvalue *ast);
int fklIsReplaceActionProd(const FklGrammerProduction *p);

FklGrammerProduction *fklCreateBuiltinActionProd(FklCgCtx *ctx,
        struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        FklVMvalue *id);

FklGrammerProduction *fklCreateExtraStartProduction(const FklCgCtx *ctx,
        FklVMvalue *group,
        FklVMvalue *sid);

#ifdef __cplusplus
}
#endif
#endif
