#ifndef FKL_CODEGEN_H
#define FKL_CODEGEN_H

#include "bytecode.h"
#include "grammer.h"
#include "pattern.h"
#include "symbol.h"
#include "vm.h"

#include <uv.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklCodegenEnvScope {
    uint32_t p;
    uint32_t start;
    uint32_t empty;
    uint32_t end;
    FklSymDefHashMap defs;
} FklCodegenEnvScope;

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

#define FKL_CODEGEN_ENV_MEMBERS                                                \
    FklUnReSymbolRefVector uref;                                               \
    uint8_t *slotFlags;                                                        \
    FklCodegenEnvScope *scopes;                                                \
    uint32_t lcount;                                                           \
    uint32_t sc;                                                               \
    uint32_t pscope;                                                           \
    uint32_t prototypeId;                                                      \
    struct FklVMvalueCodegenEnv *prev;                                         \
    struct FklVMvalueCodegenMacroScope *macros;                                \
    FklSymDefHashMap refs;                                                     \
    FklPredefHashMap pdef;                                                     \
    FklValueTable konsts;                                                      \
    struct FklPreDefRefVector ref_pdef;                                        \
    int is_debugging

struct FklCodegenEnv {
    FKL_CODEGEN_ENV_MEMBERS;
};

FKL_VM_DEF_UD_STRUCT(FklVMvalueCodegenEnv, //
        {                                  //
            union {
                struct FklCodegenEnv _env;
                alignas(struct FklCodegenEnv) struct {
                    FKL_CODEGEN_ENV_MEMBERS;
                };
            };
        });

typedef struct {
    FklVMvalueCodegenEnv *top_env;
    int no_refs_to_builtins;
    void (*resolve_ref_to_def_cb)(const FklSymDefHashMapMutElm *ref,
            const FklSymDefHashMapElm *def,
            const FklUnReSymbolRef *uref,
            FklFuncPrototype *,
            void *args);
    void *resolve_ref_to_def_cb_args;
} FklResolveRefArgs;

void fklResolveRef(FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklFuncPrototypes *cp,
        const FklResolveRefArgs *args);

void fklUpdatePrototype(FklFuncPrototypes *cp, FklVMvalueCodegenEnv *env);
void fklUpdatePrototypeRef(FklFuncPrototypes *cp,
        const FklVMvalueCodegenEnv *env);
void fklUpdatePrototypeConst(FklFuncPrototypes *cp,
        const FklVMvalueCodegenEnv *env);

typedef struct FklCodegenMacro {
    FklVMvalue *pattern;
    uint32_t prototype_id;
    FklByteCodelnt *bcl;
    struct FklCodegenMacro *next;
} FklCodegenMacro;

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

#define FKL_CODEGEN_MACRO_SCOPE_MEMBERS                                        \
    struct FklVMvalueCodegenMacroScope *prev;                                  \
    FklReplacementHashMap *replacements;                                       \
    FklCodegenMacro *head

struct FklCodegenMacroScope {
    FKL_CODEGEN_MACRO_SCOPE_MEMBERS;
};

FKL_VM_DEF_UD_STRUCT(FklVMvalueCodegenMacroScope,
        { //
            union {
                struct FklCodegenMacroScope _ms;
                alignas(struct FklCodegenMacroScope) struct {
                    FKL_CODEGEN_MACRO_SCOPE_MEMBERS;
                };
            };
        });

typedef enum FklCodegenLibType {
    FKL_CODEGEN_LIB_SCRIPT = 0,
    FKL_CODEGEN_LIB_DLL = 1,
    FKL_CODEGEN_LIB_UNINIT = 2,
} FklCodegenLibType;

typedef struct {
    uint32_t idx;
    uint32_t oidx;
} FklCodegenExportIdx;

// FklCgExportSidIdxHashMap
#define FKL_HASH_KEY_TYPE FklVMvalue *
#define FKL_HASH_VAL_TYPE FklCodegenExportIdx
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
} FklCodegenProdActionType;

#define FKL_GRAMMER_GROUP_INITED (0x1)
#define FKL_GRAMMER_GROUP_HAS_OUTER_REF (0x2)
typedef struct {
    FklStringTable delimiters;
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
        fklUninitStringTable(&(V)->delimiters);                                \
        fklUninitGrammer(&(V)->g);                                             \
    }
#include "cont/hash.h"

typedef struct {
    FklCodegenLibType type;
    union {
        struct {
            FklByteCodelnt *bcl;
            uint64_t epc;
            uint32_t prototypeId;
            FklCodegenMacro *head;
            FklReplacementHashMap *replacements;
            FklGraProdGroupHashMap named_prod_groups;
        };
        uv_lib_t dll;
    };
    char *rp;
    FklCgExportSidIdxHashMap exports;

} FklCodegenLib;

// FklCodegenLibVector
#define FKL_VECTOR_ELM_TYPE FklCodegenLib
#define FKL_VECTOR_ELM_TYPE_NAME CodegenLib
#include "cont/vector.h"

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
    FKL_CODEGEN_PATTERN_MACROEXPAND,
    FKL_CODEGEN_PATTERN_DEFMACRO,
    FKL_CODEGEN_PATTERN_IMPORT_NONE,
    FKL_CODEGEN_PATTERN_EXPORT_SINGLE,
    FKL_CODEGEN_PATTERN_EXPORT_NONE,
    FKL_CODEGEN_PATTERN_EXPORT,
    FKL_CODEGEN_PATTERN_DEF_READER_MACROS,
    FKL_CODEGEN_PATTERN_COND_COMPILE,
    FKL_CODEGEN_PATTERN_NUM,
} FklCodegenPatternEnum;

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
} FklCodegenSubPatternEnum;

#define FKL_BUILTIN_REPLACEMENT_NUM (7)
#define FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM (17)
#define FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM (11)

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

typedef struct FklCodegenCtx {
    struct FklCodegenActionVector *action_vector;
    struct FklVMvalueCodegenEnv *global_env;
    struct FklVMvalueCodegenInfo *global_info;
    struct FklCodegenErrorState *error_state;

    char *cwd;
    char *main_file_real_path_dir;
    const char *cur_file_dir;

    FklCodegenLibVector libraries;

    FklCodegenLibVector macro_libraries;

    FklVMvalueLibs *macro_vm_libs;

    FklVMvalueProtos *pts;
    FklVMvalueProtos *macro_pts;

    FklVMgc *gc;

    FklGrammer builtin_g;

#define XX(A) FklVMvalue *builtInPatternVar_##A;
    FKL_CODEGEN_SYMBOL_MAP
#undef XX

    FklVMvalue *builtin_replacement_id[FKL_BUILTIN_REPLACEMENT_NUM];

    FklVMvalue *builtin_pattern_node[FKL_CODEGEN_PATTERN_NUM];
    FklVMvalue *builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_NUM];

    FklVMvalue *builtin_prod_action_id[FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM];
    FklVMvalue *simple_prod_action_id[FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM];

} FklCodegenCtx;

typedef void (
        *FklCodegenInfoWorkCb)(struct FklVMvalueCodegenInfo *self, void *);
typedef void (*FklCodegenInfoEnvWorkCb)(struct FklVMvalueCodegenInfo *self,
        FklVMvalueCodegenEnv *,
        void *);

#define FKL_CODEGEN_INFO_MEMBERS                                               \
    FklCodegenCtx *ctx;                                                        \
    struct FklVMvalueCodegenInfo *prev;                                        \
    char *filename;                                                            \
    char *realpath;                                                            \
    char *dir;                                                                 \
    uint64_t curline;                                                          \
    FklVMvalue *fid;                                                           \
    struct {                                                                   \
        FklGrammer *self_g;                                                    \
        FklGrammer self_unnamed_g;                                             \
        FklGraProdGroupHashMap self_named_prod_groups;                         \
    };                                                                         \
    FklGrammer **g;                                                            \
    FklGrammer *unnamed_g;                                                     \
    FklGraProdGroupHashMap *named_prod_groups;                                 \
    FklVMvalueCodegenEnv *global_env;                                          \
    uint64_t epc;                                                              \
    FklCgExportSidIdxHashMap exports;                                          \
    FklCodegenMacro *export_macro;                                             \
    FklReplacementHashMap *export_replacement;                                 \
    FklVMvalueHashSet *export_named_prod_groups;                               \
    FklCodegenLibVector *libraries;                                            \
    FklVMvalueProtos *pts;                                                     \
    unsigned int is_lib : 1;                                                   \
    unsigned int is_macro : 1;                                                 \
    struct {                                                                   \
        void *work_ctx;                                                        \
        FklCodegenInfoWorkCb work_cb;                                          \
        FklCodegenInfoEnvWorkCb env_work_cb;                                   \
    }

struct FklCodegenInfo {
    FKL_CODEGEN_INFO_MEMBERS;
};

FKL_VM_DEF_UD_STRUCT(FklVMvalueCodegenInfo, //
        {                                   //
            union {
                struct FklCodegenInfo _info;
                alignas(struct FklCodegenInfo) struct {
                    FKL_CODEGEN_INFO_MEMBERS;
                };
            };
        });

typedef struct {
    size_t size;
    void (*__finalizer)(void *);
    void (*__atomic)(FklVMgc *, void *);
} FklCodegenActionContextMethodTable;

typedef struct FklCodegenActionContext {
    const FklCodegenActionContextMethodTable *t;
    alignas(void *) uint8_t d[];
} FklCodegenActionContext;

typedef struct FklCodegenErrorState {
    FklBuiltinErrorType type;
    struct {
        FklVMvalue *place;
        size_t line;
    };
    FklVMvalue *fid;
    const char *msg;
    char *msg2;
} FklCodegenErrorState;

typedef FklByteCodelnt *(*FklByteCodeProcesser)(FklVMvalueCodegenInfo *,
        FklVMvalueCodegenEnv *,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *,
        void *data,
        FklByteCodelntVector *bcl_vec,
        FklVMvalue *,
        uint64_t,
        FklCodegenErrorState *errorState,
        FklCodegenCtx *ctx);

typedef struct {
    FklVMvalue *(*getNextExpression)(void *, FklCodegenErrorState *);
    void (*finalizer)(void *);
    void (*atomic)(FklVMgc *, void *);
} FklNextExpressionMethodTable;

typedef struct {
    const FklNextExpressionMethodTable *t;
    void *context;
    uint8_t must_has_retval;
} FklCodegenNextExpression;

typedef struct FklCodegenAction {
    struct FklCodegenAction *prev;
    FklByteCodeProcesser processer;
    FklCodegenActionContext *context;
    FklVMvalueCodegenEnv *env;
    FklVMvalueCodegenMacroScope *macros;
    FklVMvalueCodegenInfo *codegen;
    FklCodegenNextExpression *expressions;

    uint32_t scope;
    uint64_t curline;

    FklByteCodelntVector bcl_vector;
} FklCodegenAction;

// FklCodegenActionVector
#define FKL_VECTOR_ELM_TYPE FklCodegenAction *
#define FKL_VECTOR_ELM_TYPE_NAME CodegenAction
#include "cont/vector.h"

void fklInitProdActionList(FklCodegenCtx *ctx);

void fklInitCodegenCtx(FklCodegenCtx *ctx,
        char *main_file_real_path_dir,
        FklVMvalueProtos *pts,
        FklVMgc *gc);
void fklInitCodegenCtxExceptPattern(FklCodegenCtx *ctx,
        FklVMvalueProtos *pts,
        FklVMgc *gc);

FKL_DEPRECATED
void fklSetCodegenCtxMainFileRealPathDir(FklCodegenCtx *ctx,
        char *main_file_real_path_dir);

void fklUninitCodegenCtx(FklCodegenCtx *ctx);

typedef struct {
    FklVMvalueProtos *pts;
    FklCodegenLibVector *libraries;
    FklVMvalueCodegenMacroScope *macro_scope;

    void *work_ctx;
    FklCodegenInfoWorkCb work_cb;
    FklCodegenInfoEnvWorkCb env_work_cb;

    int8_t is_lib;
    int8_t is_macro;
    int8_t is_global;
    int8_t inherit_grammer;
} FklCodegenInfoArgs;

struct FklCustomActionCtx {
    FklCodegenCtx *ctx;
    FklByteCodelnt *bcl;
    uint32_t prototype_id;
};

typedef struct FklSimpleActionCtx {
    const FklSimpleProdAction *mt;
    struct FklVMvalue *vec;
} FklSimpleActionCtx;

FKL_VM_DEF_UD_STRUCT(FklVMvalueSimpleActionCtx, { FklSimpleActionCtx c; });
FKL_VM_DEF_UD_STRUCT(FklVMvalueCustomActionCtx,
        { struct FklCustomActionCtx c; });

int fklIsVMvalueCodegenInfo(FklVMvalue *v);
FklVMvalueCodegenInfo *fklCreateVMvalueCodegenInfo(FklCodegenCtx *ctx,
        FklVMvalueCodegenInfo *prev,
        const char *filename,
        const FklCodegenInfoArgs *args);

FklByteCodelnt *fklGenExpressionCode(FklVMvalue *exp,
        FklVMvalueCodegenEnv *cur_env,
        FklVMvalueCodegenInfo *codegen);
FklByteCodelnt *fklGenExpressionCodeWithAction(FklCodegenAction *,
        FklVMvalueCodegenInfo *codegen);
FklByteCodelnt *fklGenExpressionCodeWithFp(FILE *,
        FklVMvalueCodegenInfo *codegen,
        FklVMvalueCodegenEnv *cur_env);

FklByteCodelnt *fklGenExpressionCodeWithQueue(FklVMvalueQueue *,
        FklVMvalueCodegenInfo *codegen,
        FklVMvalueCodegenEnv *cur_env);

FklByteCodelnt *fklGenExpressionCodeWithFpForPrecompile(FILE *fp,
        FklVMvalueCodegenInfo *codegen,
        FklVMvalueCodegenEnv *cur_env);

FklSymDefHashMapElm *fklFindSymbolDefByIdAndScope(FklVMvalue *id,
        uint32_t scope,
        const FklCodegenEnvScope *scopes);
FklSymDefHashMapElm *fklGetCodegenDefByIdInScope(FklVMvalue *id,
        uint32_t scope,
        const FklCodegenEnvScope *scopes);

void fklPrintCodegenError(FklCodegenErrorState *error_state,
        const FklVMvalueCodegenInfo *info);

void fklPrintUndefinedRef(const FklVMvalueCodegenEnv *env);

FklSymDefHashMapElm *fklAddCodegenBuiltinRefBySid(FklVMvalue *id,
        FklVMvalueCodegenEnv *env);
uint32_t fklAddCodegenRefBySidRetIndex(FklVMvalue *id,
        FklVMvalueCodegenEnv *env,
        FklVMvalue *fid,
        uint64_t line,
        uint32_t assign);
FklSymDefHashMapElm *fklAddCodegenRefBySid(FklVMvalue *id,
        FklVMvalueCodegenEnv *env,
        FklVMvalue *fid,
        uint64_t line,
        uint32_t assign);
FklSymDef *fklGetCodegenRefBySid(FklVMvalue *id, FklVMvalueCodegenEnv *env);

FklSymDef *fklAddCodegenDefBySid(FklVMvalue *id,
        uint32_t scope,
        FklVMvalueCodegenEnv *env);

void fklAddCodegenPreDefBySid(FklVMvalue *id,
        uint32_t scope,
        uint8_t isConst,
        FklVMvalueCodegenEnv *env);

uint8_t *fklGetCodegenPreDefBySid(FklVMvalue *id,
        uint32_t scope,
        FklVMvalueCodegenEnv *env);
void fklAddCodegenRefToPreDef(FklVMvalue *id,
        uint32_t scope,
        uint32_t prototypeId,
        uint32_t idx,
        FklVMvalueCodegenEnv *env);
void fklResolveCodegenPreDef(FklVMvalue *,
        uint32_t scope,
        FklVMvalueCodegenEnv *env,
        FklFuncPrototypes *pts);
void fklClearCodegenPreDef(FklVMvalueCodegenEnv *env);

int fklIsSymbolDefined(FklVMvalue *sid, uint32_t scope, FklVMvalueCodegenEnv *);

int fklIsReplacementDefined(FklVMvalue *sid, FklVMvalueCodegenEnv *);

FklVMvalue *fklGetReplacement(FklVMvalue *sid, const FklReplacementHashMap *);

int fklIsVMvalueCodegenEnv(FklVMvalue *);
FklVMvalueCodegenEnv *fklCreateVMvalueCodegenEnv(FklCodegenCtx *ctx,
        FklVMvalueCodegenEnv *prev,
        uint32_t pscope,
        FklVMvalueCodegenMacroScope *);

void fklCreateFuncPrototypeAndInsertToPool(FklVMvalueCodegenInfo *info,
        uint32_t p,
        FklVMvalueCodegenEnv *env,
        FklVMvalue *sid,
        uint32_t line);
void fklInitFuncPrototypeWithEnv(FklFuncPrototype *cpt,
        FklVMvalueCodegenInfo *info,
        FklVMvalueCodegenEnv *env,
        FklVMvalue *sid,
        uint32_t line);

void fklInitCodegenScriptLib(FklCodegenLib *lib,
        FklVMvalueCodegenInfo *codegen,
        FklByteCodelnt *bcl,
        uint64_t epc,
        FklVMvalueCodegenEnv *env);

FklCodegenDllLibInitExportFunc fklGetCodegenInitExportFunc(uv_lib_t *dll);

void fklInitCodegenDllLib(FklCodegenCtx *ctx,
        FklCodegenLib *lib,
        char *rp,
        uv_lib_t dll,
        FklCodegenDllLibInitExportFunc init);

void fklClearCodegenLibMacros(FklCodegenLib *lib);

void fklUninitCodegenLib(FklCodegenLib *);

void fklUninitCodegenLib(FklCodegenLib *);

FklCodegenMacro *fklCreateCodegenMacro(FklVMvalue *pattern,
        const FklByteCodelnt *bcl,
        FklCodegenMacro *next,
        uint32_t prototype_id);

int fklIsVMvalueCodegenMacroScope(FklVMvalue *v);
FklVMvalueCodegenMacroScope *fklCreateVMvalueCodegenMacroScope(FklCodegenCtx *c,
        FklVMvalueCodegenMacroScope *prev);

FklVMvalue *fklTryExpandCodegenMacro(FklVMvalue *exp,
        FklVMvalueCodegenInfo *,
        FklVMvalueCodegenMacroScope *macros,
        FklCodegenErrorState *);

FklVM *fklInitMacroExpandVM(FklCodegenCtx *ctx,
        FklByteCodelnt *bcl,
        uint32_t prototype_id,
        FklPmatchHashMap *ht,
        FklLineNumHashMap *lineHash,
        FklVMvalue **pr,
        uint64_t curline);

void fklInitVMlibWithCodegenLib(const FklCodegenLib *clib,
        FklVMlib *vlib,
        FklVM *exe,
        const FklVMvalueProtos *);

void fklUpdateVMlibsWithCodegenLibVector(FklVM *vm,
        FklVMvalueLibs *libs,
        const FklCodegenLibVector *clibs,
        const FklVMvalueProtos *pts);

FKL_DEPRECATED
void fklInitPreLibReaderMacros(FklCodegenLibVector *libs,
        FklVMgc *gc,
        FklCodegenCtx *ctx,
        FklFuncPrototypes *pts,
        FklCodegenLibVector *macroLibs);

FKL_DEPRECATED
FklProdActionFunc fklFindBuiltinProdActionByName(const char *str);
FKL_DEPRECATED
const FklSimpleProdAction *fklFindSimpleProdActionByName(const char *str);

FklGrammerProduction *fklCreateCustomActionProd(FklCodegenCtx *cg_ctx,
        struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        uint32_t prototypeId);
int fklIsCustomActionProd(const FklGrammerProduction *p);

FklGrammerProduction *fklCreateSimpleActionProd(FklCodegenCtx *cg_ctx,
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

FklGrammerProduction *fklCreateBuiltinActionProd(FklCodegenCtx *ctx,
        struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        FklVMvalue *id);

FKL_DEPRECATED
const FklSimpleProdAction *fklGetSimpleProdActions(void);

FKL_DEPRECATED
void fklPrintReaderMacroAction(FILE *fp, const FklGrammerProduction *prod);

FKL_DEPRECATED
void fklIncreaseLibIdAndPrototypeId(FklCodegenLib *lib,
        uint32_t lib_count,
        uint32_t macro_lib_count,
        uint32_t pts_count,
        uint32_t macro_pts_count);

FklGrammerProduction *fklCreateExtraStartProduction(FklCodegenCtx *ctx,
        FklVMvalue *group,
        FklVMvalue *sid);

#ifdef __cplusplus
}
#endif
#endif
