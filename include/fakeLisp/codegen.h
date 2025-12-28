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

#define FKL_CODEGEN_ENV_SLOT_OCC (1)
#define FKL_CODEGEN_ENV_SLOT_REF (2)

typedef struct {
    FklVMvalue *id;
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
    return fklHashCombine(fklVMvalueEqHashv((pk)->id), (pk)->scope);
#define FKL_HASH_KEY_EQUAL(A, B) (A)->id == (B)->id && (A)->scope == (B)->scope
#include "cont/hash.h"

FKL_VM_DEF_UD_STRUCT(FklVMvalueCgEnv, {
    FklUnReSymbolRefVector uref;
    uint8_t *slotFlags;
    FklCgEnvScope *scopes;
    uint32_t lcount;
    uint32_t sc;
    uint32_t pscope;
    uint32_t prototypeId;
    struct FklVMvalueCgEnv *prev;
    struct FklVMvalueCgMacroScope *macros;
    FklSymDefHashMap refs;
    FklPredefHashMap pdef;
    FklValueTable konsts;
    struct FklPreDefRefVector ref_pdef;
    int is_debugging;
});

typedef struct {
    FklVMvalueCgEnv *top_env;
    int no_refs_to_builtins;
    void (*resolve_ref_to_def_cb)(const FklSymDefHashMapMutElm *ref,
            const FklSymDefHashMapElm *def,
            const FklUnReSymbolRef *uref,
            FklFuncPrototype *,
            void *args);
    void *resolve_ref_to_def_cb_args;
} FklResolveRefArgs;

void fklResolveRef(FklVMvalueCgEnv *env,
        uint32_t scope,
        FklFuncPrototypes *cp,
        const FklResolveRefArgs *args);

void fklUpdatePrototype(FklFuncPrototypes *cp, FklVMvalueCgEnv *env);
void fklUpdatePrototypeRef(FklFuncPrototypes *cp, const FklVMvalueCgEnv *env);
void fklUpdatePrototypeConst(FklFuncPrototypes *cp, const FklVMvalueCgEnv *env);

typedef struct FklCgMacro {
    FklVMvalue *pattern;
    uint32_t prototype_id;
    FklVMvalue *bcl;
    struct FklCgMacro *next;
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
            head->bcl = NULL;                                                  \
            head->prototype_id = 0;                                            \
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

typedef struct {
    FklCgLibType type;
    union {
        struct {
            FklVMvalue *bcl;
            uint64_t epc;
            uint32_t prototypeId;
            FklMacroHashMap *macros;
            FklReplacementHashMap *replacements;
            FklGraProdGroupHashMap *named_prod_groups;
        };
        uv_lib_t dll;
    };
    char *rp;
    FklCgExportSidIdxHashMap exports;

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

typedef struct FklCgCtx {
    struct FklCgActVector *action_vector;
    FklValueVector *bcl_vector;
    struct FklVMvalueCgEnv *global_env;
    struct FklVMvalueCgInfo *global_info;
    struct FklCgErrorState *error_state;

    char *cwd;
    char *main_file_real_path_dir;
    const char *cur_file_dir;

    FklVMvalueCgLibs *libraries;

    FklVMvalueCgLibs *macro_libraries;

    FklVMvalueLibs *macro_vm_libs;

    FklVMvalueProtos *pts;
    FklVMvalueProtos *macro_pts;

    FklVM *vm;

    FklGrammer builtin_g;

#define XX(A) FklVMvalue *builtin_sym_##A;
    FKL_CODEGEN_SYMBOL_MAP
#undef XX

    FklVMvalueLnt *lnt;

    FklVMvalue *builtin_replacement_id[FKL_BUILTIN_REPLACEMENT_NUM];

    FklVMvalue *builtin_pattern_node[FKL_CODEGEN_PATTERN_NUM];
    FklVMvalue *builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_NUM];

    FklVMvalue *builtin_prod_action_id[FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM];
    FklVMvalue *simple_prod_action_id[FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM];

} FklCgCtx;

typedef void (*FklCgInfoWorkCb)(struct FklVMvalueCgInfo *self, void *);
typedef void (*FklCgInfoEnvWorkCb)(struct FklVMvalueCgInfo *self,
        FklVMvalueCgEnv *,
        void *);

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
    FklVMvalueProtos *pts;
    unsigned int is_lib : 1;
    unsigned int is_macro : 1;
    struct {
        void *work_ctx;
        FklCgInfoWorkCb work_cb;
        FklCgInfoEnvWorkCb env_work_cb;
    };
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

void fklInitCgCtx(FklCgCtx *ctx,
        char *main_file_real_path_dir,
        FklVMvalueProtos *pts,
        FklVM *vm);
void fklInitCgCtxExceptPattern(FklCgCtx *ctx, FklVMvalueProtos *pts, FklVM *vm);

void fklUninitCgCtx(FklCgCtx *ctx);

typedef struct {
    FklVMvalueProtos *pts;
    FklVMvalueCgLibs *libraries;
    FklVMvalueCgMacroScope *macro_scope;

    void *work_ctx;
    FklCgInfoWorkCb work_cb;
    FklCgInfoEnvWorkCb env_work_cb;

    int8_t is_lib;
    int8_t is_macro;
    int8_t is_global;
    int8_t inherit_grammer;
} FklCgInfoArgs;

FKL_VM_DEF_UD_STRUCT(FklVMvalueSimpleActCtx, {
    const FklSimpleProdAction *mt;
    FklVMvalue *vec;
});

FKL_VM_DEF_UD_STRUCT(FklVMvalueCustomActCtx, {
    FklVMvalue *bcl;
    uint32_t prototype_id;

    FklVMvalue *doller_s;
    FklVMvalue *line_s;
    size_t actual_len;
    FklVMvalue *dollers[];
});

int fklIsVMvalueCgLibs(const FklVMvalue *v);
FklVMvalueCgLibs *fklCreateVMvalueCgLibs(FklVM *vm);
FklVMvalueCgLibs *fklCreateVMvalueCgLibs1(FklVM *vm, size_t num);
FklCgLib *fklVMvalueCgLibsLast(const FklVMvalueCgLibs *v);
size_t fklVMvalueCgLibsLastId(const FklVMvalueCgLibs *v);
size_t fklVMvalueCgLibsNextId(const FklVMvalueCgLibs *v);
size_t fklVMvalueCgLibsFind(const FklVMvalueCgLibs *v, const char *realpath);
FklCgLib *fklVMvalueCgLibsPushBack(FklVMvalueCgLibs *v);
FklCgLib *fklVMvalueCgLibsEmplaceBack(FklVMvalueCgLibs *v, FklCgLib *libs);
FklCgLib *fklVMvalueCgLibsGet(FklVMvalueCgLibs *v, size_t id);

FklCgLib *fklVMvalueCgLibs(FklVMvalueCgLibs *v);
size_t fklVMvalueCgLibsCount(FklVMvalueCgLibs *v);

void fklVMvalueCgLibsMerge(FklVMvalueCgLibs *out, FklVMvalueCgLibs *in);

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

FklSymDefHashMapElm *fklFindSymbolDefByIdAndScope(FklVMvalue *id,
        uint32_t scope,
        const FklCgEnvScope *scopes);
FklSymDefHashMapElm *fklGetCgDefByIdInScope(FklVMvalue *id,
        uint32_t scope,
        const FklCgEnvScope *scopes);

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
void fklResolveCgPreDef(FklVMvalue *,
        uint32_t scope,
        FklVMvalueCgEnv *env,
        FklFuncPrototypes *pts);
void fklClearCgPreDef(FklVMvalueCgEnv *env);

int fklIsSymbolDefined(FklVMvalue *sid, uint32_t scope, FklVMvalueCgEnv *);

int fklIsReplacementDefined(FklVMvalue *sid, FklVMvalueCgEnv *);

FklVMvalue *fklGetReplacement(FklVMvalue *sid, const FklReplacementHashMap *);

int fklIsVMvalueCgEnv(const FklVMvalue *);
FklVMvalueCgEnv *fklCreateVMvalueCgEnv(const FklCgCtx *ctx,
        FklVMvalueCgEnv *prev,
        uint32_t pscope,
        FklVMvalueCgMacroScope *);

void fklCreateFuncPrototypeAndInsertToPool(FklVMvalueCgInfo *info,
        uint32_t p,
        FklVMvalueCgEnv *env,
        FklVMvalue *sid,
        uint32_t line);
void fklInitFuncPrototypeWithEnv(FklFuncPrototype *cpt,
        FklVMvalueCgInfo *info,
        FklVMvalueCgEnv *env,
        FklVMvalue *sid,
        uint32_t line);

void fklInitCgScriptLib(FklCgLib *lib,
        FklVMvalueCgInfo *codegen,
        FklVMvalue *bcl,
        uint64_t epc,
        FklVMvalueCgEnv *env);

FklCgDllLibInitExportCb fklGetCgInitExportFunc(uv_lib_t *dll);

void fklInitCgDllLib(const FklCgCtx *ctx,
        FklCgLib *lib,
        char *rp,
        uv_lib_t dll,
        FklCgDllLibInitExportCb init);

void fklClearCgLibMacros(FklCgLib *lib);
void fklClearCgLibMacros2(const FklCgCtx *ctx);

FklCgMacro *fklCreateCgMacro(FklVMvalue *pattern,
        FklVMvalue *bcl,
        FklCgMacro *next,
        uint32_t prototype_id);

int fklIsVMvalueCgMacroScope(const FklVMvalue *v);
FklVMvalueCgMacroScope *fklCreateVMvalueCgMacroScope(const FklCgCtx *c,
        FklVMvalueCgMacroScope *prev);

FklVMvalue *fklTryExpandCgMacroOnce(FklCgCtx *ctx,
        const FklPmatchRes *exp,
        FklVMvalueCgInfo *,
        FklVMvalueCgMacroScope *macros);

FklVMvalue *fklTryExpandCgMacro(FklCgCtx *ctx,
        const FklPmatchRes *exp,
        FklVMvalueCgInfo *,
        FklVMvalueCgMacroScope *macros);

void fklInitVMlibWithCgLib(const FklCgLib *clib,
        FklVMlib *vlib,
        FklVM *exe,
        const FklVMvalueProtos *);

void fklUpdateVMlibsWithCgLibVector(FklVM *vm,
        FklVMvalueLibs *libs,
        const FklVMvalueCgLibs *clibs,
        const FklVMvalueProtos *pts);

FklGrammerProduction *fklCreateCustomActionProd(FklCgCtx *cg_ctx,
        struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        uint32_t prototypeId);
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
