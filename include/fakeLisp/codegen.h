#ifndef FKL_CODEGEN_H
#define FKL_CODEGEN_H

#include "bytecode.h"
#include "grammer.h"
#include "nast.h"
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
    FklSid_t id;
    uint32_t scope;
    uint32_t prototypeId;
    uint32_t idx;
} FklPreDefRef;

// FklPreDefRefVector
#define FKL_VECTOR_ELM_TYPE FklPreDefRef
#define FKL_VECTOR_ELM_TYPE_NAME PreDefRef
#include "vector.h"

// FklPredefHashMap
#define FKL_HASH_KEY_TYPE FklSidScope
#define FKL_HASH_VAL_TYPE uint8_t
#define FKL_HASH_ELM_NAME Predef
#define FKL_HASH_KEY_HASH                                                      \
    return fklHashCombine(fklHash32Shift((pk)->id), (pk)->scope);
#define FKL_HASH_KEY_EQUAL(A, B) (A)->id == (B)->id && (A)->scope == (B)->scope
#include "hash.h"

typedef struct FklCodegenEnv {
    size_t refcount;
    FklUnReSymbolRefVector uref;

    uint8_t *slotFlags;
    FklCodegenEnvScope *scopes;
    uint32_t lcount;
    uint32_t sc;
    uint32_t pscope;

    uint32_t prototypeId;

    struct FklCodegenEnv *prev;
    FklSymDefHashMap refs;
    struct FklCodegenMacroScope *macros;

    FklPredefHashMap pdef;
    struct FklPreDefRefVector ref_pdef;
} FklCodegenEnv;

void fklUpdatePrototype(FklFuncPrototypes *cp, FklCodegenEnv *env,
                        FklSymbolTable *runtime_st, FklSymbolTable *pst);

void fklUpdatePrototypeRef(FklFuncPrototypes *cp, FklCodegenEnv *env,
                           FklSymbolTable *globalSymTable, FklSymbolTable *pst);

typedef struct FklCodegenMacro {
    FklNastNode *origin_exp;
    FklNastNode *pattern;
    uint32_t prototype_id;
    FklByteCodelnt *bcl;
    struct FklCodegenMacro *next;
    uint8_t own;
} FklCodegenMacro;

// FklReplacementHashMap
#define FKL_HASH_KEY_TYPE FklSid_t
#define FKL_HASH_VAL_TYPE FklNastNode *
#define FKL_HASH_ELM_NAME Replacement
#define FKL_HASH_VAL_INIT(V, X)                                                \
    {                                                                          \
        fklDestroyNastNode(*(V));                                              \
        *(V) = fklMakeNastNodeRef(*(X));                                       \
    }
#define FKL_HASH_VAL_UNINIT(V) fklDestroyNastNode(*(V))
#include "hash.h"

typedef struct FklCodegenMacroScope {
    uint32_t refcount;
    struct FklCodegenMacroScope *prev;
    FklReplacementHashMap *replacements;
    FklCodegenMacro *head;
} FklCodegenMacroScope;

typedef enum FklCodegenLibType {
    FKL_CODEGEN_LIB_SCRIPT = 0,
    FKL_CODEGEN_LIB_DLL = 1,
} FklCodegenLibType;

typedef struct {
    uint32_t idx;
    uint32_t oidx;
} FklCodegenExportIdx;

// FklCgExportSidIdxHashMap
#define FKL_HASH_KEY_TYPE FklSid_t
#define FKL_HASH_VAL_TYPE FklCodegenExportIdx
#define FKL_HASH_ELM_NAME CgExportSidIdx
#include "hash.h"

typedef struct {
    FklSid_t group_id;
    FklSid_t sid;
    FklNastNode *vec;
    uint8_t add_extra;
    uint8_t type;
    union {
        FklNastNode *forth;
        struct {
            uint32_t prototype_id;
            FklByteCodelnt *bcl;
        };
    };
} FklCodegenProdPrinting;

typedef struct FklSimpleProdAction {
    const char *name;
    FklProdActionFunc func;
    void *(*creator)(FklNastNode *rest[], size_t rest_len, int *failed);
    void *(*ctx_copyer)(const void *);
    void (*ctx_destroy)(void *);
    void (*write)(void *, const FklSymbolTable *pst, FILE *fp);
    void *(*read)(FklSymbolTable *pst, FILE *fp);
    void (*print)(void *ctx, const FklSymbolTable *pst, FILE *fp);
} FklSimpleProdAction;

// to be delete
// FklProdPrintingVector
#define FKL_VECTOR_ELM_TYPE FklCodegenProdPrinting
#define FKL_VECTOR_ELM_TYPE_NAME ProdPrinting
#include "vector.h"

typedef enum {
    FKL_CODEGEN_PROD_BUILTIN = 0,
    FKL_CODEGEN_PROD_SIMPLE,
    FKL_CODEGEN_PROD_CUSTOM,
} FklCodegenProdActionType;

typedef struct {
    int is_ref_outer;
    FklSymbolTable reachable_terminals;
    FklGrammer g;
    // FklProdHashMap prods;
    // FklGrammerIgnore *ignore;

    // to be delete
    FklNastNodeVector ignore_printing;
    FklProdPrintingVector prod_printing;
} FklGrammerProdGroupItem;

// FklGraProdGroupHashMap
#define FKL_HASH_KEY_TYPE FklSid_t
#define FKL_HASH_VAL_TYPE FklGrammerProdGroupItem
#define FKL_HASH_ELM_NAME GraProdGroup
#define FKL_HASH_VAL_UNINIT(V)                                                 \
    {                                                                          \
        fklUninitSymbolTable(&(V)->reachable_terminals);                       \
        fklUninitGrammer(&(V)->g);                                             \
        while (!fklNastNodeVectorIsEmpty(&(V)->ignore_printing))               \
            fklDestroyNastNode(                                                \
                *fklNastNodeVectorPopBackNonNull(&(V)->ignore_printing));      \
        fklNastNodeVectorUninit(&(V)->ignore_printing);                        \
        while (!fklProdPrintingVectorIsEmpty(&(V)->prod_printing)) {           \
            FklCodegenProdPrinting *p =                                        \
                fklProdPrintingVectorPopBackNonNull(&(V)->prod_printing);      \
            fklDestroyNastNode(p->vec);                                        \
            if (p->type == FKL_CODEGEN_PROD_CUSTOM)                            \
                fklDestroyByteCodelnt(p->bcl);                                 \
            else                                                               \
                fklDestroyNastNode(p->forth);                                  \
        }                                                                      \
        fklProdPrintingVectorUninit(&(V)->prod_printing);                      \
    }
#include "hash.h"

typedef struct {
    FklCodegenLibType type;
    union {
        FklByteCodelnt *bcl;
        uv_lib_t dll;
    };
    char *rp;
    FklCgExportSidIdxHashMap exports;

    FklCodegenMacro *head;
    FklReplacementHashMap *replacements;

    FklGraProdGroupHashMap named_prod_groups;
    // FklSymbolTable terminal_table;
    // FklRegexTable regexes;

    uint32_t prototypeId;
} FklCodegenLib;

// FklCodegenLibVector
#define FKL_VECTOR_ELM_TYPE FklCodegenLib
#define FKL_VECTOR_ELM_TYPE_NAME CodegenLib
#include "vector.h"

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

typedef enum {
    FKL_CODEGEN_SID_RECOMPUTE_NONE = 0,
    FKL_CODEGEN_SID_RECOMPUTE_MARK_SYM_AS_RC_SYM,
} FklCodegenRecomputeNastSidOption;

#define FKL_BUILTIN_REPLACEMENT_NUM (7)
#define FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM (17)
#define FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM (11)

typedef struct {
    char *cwd;
    char *main_file_real_path_dir;
    const char *cur_file_dir;

    FklSymbolTable public_symbol_table;
    FklConstTable public_kt;

    FklGrammer builtin_g;

    FklSid_t builtInPatternVar_orig;
    FklSid_t builtInPatternVar_rest;
    FklSid_t builtInPatternVar_name;
    FklSid_t builtInPatternVar_value;
    FklSid_t builtInPatternVar_cond;
    FklSid_t builtInPatternVar_args;
    FklSid_t builtInPatternVar_arg0;
    FklSid_t builtInPatternVar_arg1;
    FklSid_t builtInPatternVar_custom;
    FklSid_t builtInPatternVar_builtin;
    FklSid_t builtInPatternVar_simple;

    FklSid_t builtin_replacement_id[FKL_BUILTIN_REPLACEMENT_NUM];

    FklNastNode *builtin_pattern_node[FKL_CODEGEN_PATTERN_NUM];
    FklNastNode *builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_NUM];

    FklSid_t builtin_prod_action_id[FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM];
    FklSid_t simple_prod_action_id[FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM];

} FklCodegenOuterCtx;

struct FklCodegenInfo;
typedef void (*FklCodegenInfoWorkCb)(struct FklCodegenInfo *self, void *);
typedef void (*FklCodegenInfoEnvWorkCb)(struct FklCodegenInfo *self,
                                        FklCodegenEnv *, void *);

typedef struct FklCodegenInfo {
    FklCodegenOuterCtx *outer_ctx;

    struct FklCodegenInfo *prev;

    char *filename;
    char *realpath;
    char *dir;
    uint64_t curline;
    FklSid_t fid;

    struct {
        FklGrammer *self_g;
        // FklProdHashMap self_builtin_prods;
        // FklGrammerIgnore *self_builtin_ignores;
        FklGrammer self_unnamed_g;
        // FklProdHashMap self_unnamed_prods;
        // FklGrammerIgnore *self_unnamed_ignores;
        FklGraProdGroupHashMap self_named_prod_groups;
    };

    FklGrammer **g;
    // FklProdHashMap *builtin_prods;
    // FklGrammerIgnore **builtin_ignores;
    FklGrammer *unnamed_g;
    // FklProdHashMap *unnamed_prods;
    // FklGrammerIgnore **unnamed_ignores;
    FklGraProdGroupHashMap *named_prod_groups;

    FklCodegenEnv *global_env;
    FklSymbolTable *runtime_symbol_table;
    FklConstTable *runtime_kt;

    FklCgExportSidIdxHashMap exports;

    FklCodegenMacro *export_macro;
    FklReplacementHashMap *export_replacement;

    FklSidHashSet *export_named_prod_groups;

    FklCodegenLibVector *libStack;

    FklCodegenLibVector *macroLibStack;

    FklFuncPrototypes *pts;
    FklFuncPrototypes *macro_pts;

    unsigned int is_destroyable : 1;
    unsigned int is_lib : 1;
    unsigned int is_macro : 1;
    uint64_t refcount : 61;

    struct {
        void *work_ctx;
        FklCodegenInfoWorkCb work_cb;
        FklCodegenInfoEnvWorkCb env_work_cb;
    } work;
} FklCodegenInfo;

typedef struct {
    void (*__put_bcl)(void *, FklByteCodelnt *bcl);
    void (*__finalizer)(void *);
    FklByteCodelntVector *(*__get_bcl_stack)(void *);
} FklCodegenQuestContextMethodTable;

typedef struct FklCodegenQuestContext {
    void *data;
    const FklCodegenQuestContextMethodTable *t;
} FklCodegenQuestContext;

typedef struct FklCodegenErrorState {
    FklBuiltinErrorType type;
    FklNastNode *place;
    size_t line;
    FklSid_t fid;
    const char *msg;
} FklCodegenErrorState;

typedef FklByteCodelnt *(*FklByteCodeProcesser)(
    FklCodegenInfo *, FklCodegenEnv *, uint32_t scope, FklCodegenMacroScope *,
    FklCodegenQuestContext *context, FklSid_t, uint64_t,
    FklCodegenErrorState *errorState, FklCodegenOuterCtx *outer_ctx);

typedef struct {
    FklNastNode *(*getNextExpression)(void *, FklCodegenErrorState *);
    void (*finalizer)(void *);
} FklNextExpressionMethodTable;

typedef struct {
    const FklNextExpressionMethodTable *t;
    void *context;
    uint8_t must_has_retval;
} FklCodegenNextExpression;

typedef struct FklCodegenQuest {
    FklByteCodeProcesser processer;
    FklCodegenQuestContext *context;
    FklCodegenEnv *env;
    uint32_t scope;
    FklCodegenMacroScope *macroScope;
    uint64_t curline;
    FklCodegenInfo *codegen;
    struct FklCodegenQuest *prev;
    FklCodegenNextExpression *nextExpression;
} FklCodegenQuest;

// FklCodegenQuestVector
#define FKL_VECTOR_ELM_TYPE FklCodegenQuest *
#define FKL_VECTOR_ELM_TYPE_NAME CodegenQuest
#include "vector.h"

FklCodegenEnv *
fklInitGlobalCodegenInfo(FklCodegenInfo *codegen, const char *rp,
                         FklSymbolTable *runtime_st, FklConstTable *runtime_kt,
                         int destroyAbleMark, FklCodegenOuterCtx *outer_ctx,
                         FklCodegenInfoWorkCb work_cb,
                         FklCodegenInfoEnvWorkCb evn_work_cb, void *ctx);

FklCodegenQuest *
fklCreateCodegenQuest(FklByteCodeProcesser f, FklCodegenQuestContext *context,
                      FklCodegenNextExpression *nextExpression, uint32_t scope,
                      FklCodegenMacroScope *macroScope, FklCodegenEnv *env,
                      uint64_t curline, FklCodegenQuest *prev,
                      FklCodegenInfo *codegen);

void fklInitCodegenInfo(FklCodegenInfo *codegen, const char *filename,
                        FklCodegenInfo *prev, FklSymbolTable *runtime_st,
                        FklConstTable *runtime_kt, int destroyAbleMark,
                        int libMark, int macroMark,
                        FklCodegenOuterCtx *outer_ctx);

void fklUninitCodegenInfo(FklCodegenInfo *codegen);
void fklDestroyCodegenInfo(FklCodegenInfo *codegen);

void fklInitCodegenOuterCtx(FklCodegenOuterCtx *ctx,
                            char *main_file_real_path_dir);
void fklInitCodegenOuterCtxExceptPattern(FklCodegenOuterCtx *outerCtx);

void fklSetCodegenOuterCtxMainFileRealPathDir(FklCodegenOuterCtx *ctx,
                                              char *main_file_real_path_dir);

void fklUninitCodegenOuterCtx(FklCodegenOuterCtx *ctx);
FklByteCode *fklCodegenNode(const FklNastNode *, FklCodegenInfo *codegen);
FklByteCodelnt *fklGenExpressionCode(FklNastNode *exp, FklCodegenEnv *cur_env,
                                     FklCodegenInfo *codegen);
FklByteCodelnt *fklGenExpressionCodeWithQuest(FklCodegenQuest *,
                                              FklCodegenInfo *codegen);
FklByteCodelnt *fklGenExpressionCodeWithFp(FILE *, FklCodegenInfo *codegen,
                                           FklCodegenEnv *cur_env);

FklByteCodelnt *fklGenExpressionCodeWithQueue(FklNastNodeQueue *,
                                              FklCodegenInfo *codegen,
                                              FklCodegenEnv *cur_env);

FklByteCodelnt *fklGenExpressionCodeWithFpForPrecompile(FILE *fp,
                                                        FklCodegenInfo *codegen,
                                                        FklCodegenEnv *cur_env);

FklSymDefHashMapElm *fklFindSymbolDefByIdAndScope(FklSid_t id, uint32_t scope,
                                                  const FklCodegenEnv *env);
FklSymDefHashMapElm *fklGetCodegenDefByIdInScope(FklSid_t id, uint32_t scope,
                                                 const FklCodegenEnv *env);

void fklPrintCodegenError(FklNastNode *obj, FklBuiltinErrorType type,
                          const FklCodegenInfo *info,
                          const FklSymbolTable *symbolTable, size_t line,
                          FklSid_t fid, const FklSymbolTable *pst,
                          const char *msg);

void fklPrintUndefinedRef(const FklCodegenEnv *env, FklSymbolTable *runtime_st,
                          FklSymbolTable *pst);

FklSymDefHashMapElm *fklAddCodegenBuiltinRefBySid(FklSid_t id,
                                                  FklCodegenEnv *env);
uint32_t fklAddCodegenRefBySidRetIndex(FklSid_t id, FklCodegenEnv *env,
                                       FklSid_t fid, uint64_t line,
                                       uint32_t assign);
FklSymDefHashMapElm *fklAddCodegenRefBySid(FklSid_t id, FklCodegenEnv *env,
                                           FklSid_t fid, uint64_t line,
                                           uint32_t assign);
FklSymDef *fklGetCodegenRefBySid(FklSid_t id, FklCodegenEnv *env);

FklSymDef *fklAddCodegenDefBySid(FklSid_t id, uint32_t scope,
                                 FklCodegenEnv *env);

void fklAddCodegenPreDefBySid(FklSid_t id, uint32_t scope, uint8_t isConst,
                              FklCodegenEnv *env);
uint8_t *fklGetCodegenPreDefBySid(FklSid_t id, uint32_t scope,
                                  FklCodegenEnv *env);
void fklAddCodegenRefToPreDef(FklSid_t id, uint32_t scope, uint32_t prototypeId,
                              uint32_t idx, FklCodegenEnv *env);
void fklResolveCodegenPreDef(FklSid_t, uint32_t scope, FklCodegenEnv *env,
                             FklFuncPrototypes *pts);
void fklClearCodegenPreDef(FklCodegenEnv *env);

int fklIsSymbolDefined(FklSid_t sid, uint32_t scope, FklCodegenEnv *);

int fklIsReplacementDefined(FklSid_t sid, FklCodegenEnv *);

FklNastNode *fklGetReplacement(FklSid_t sid, const FklReplacementHashMap *);

FklCodegenEnv *fklCreateCodegenEnv(FklCodegenEnv *prev, uint32_t pscope,
                                   FklCodegenMacroScope *);
void fklCreateFuncPrototypeAndInsertToPool(FklCodegenInfo *info, uint32_t p,
                                           FklCodegenEnv *env, FklSid_t sid,
                                           uint32_t line, FklSymbolTable *pst);
void fklInitFuncPrototypeWithEnv(FklFuncPrototype *cpt, FklCodegenInfo *info,
                                 FklCodegenEnv *env, FklSid_t sid,
                                 uint32_t line, FklSymbolTable *pst);
void fklDestroyCodegenEnv(FklCodegenEnv *env);

void fklInitCodegenScriptLib(FklCodegenLib *lib, FklCodegenInfo *codegen,
                             FklByteCodelnt *bcl, FklCodegenEnv *env);

FklCodegenDllLibInitExportFunc fklGetCodegenInitExportFunc(uv_lib_t *dll);

void fklInitCodegenDllLib(FklCodegenLib *lib, char *rp, uv_lib_t dll,
                          FklSymbolTable *table,
                          FklCodegenDllLibInitExportFunc init,
                          FklSymbolTable *pst);

void fklDestroyCodegenLibMacroScope(FklCodegenLib *lib);
void fklUninitCodegenLib(FklCodegenLib *);
void fklUninitCodegenLibInfo(FklCodegenLib *);

void fklUninitCodegenLibExceptBclAndDll(FklCodegenLib *clib);
void fklUninitCodegenLib(FklCodegenLib *);

FklCodegenMacro *fklCreateCodegenMacro(FklNastNode *pattern,
                                       FklNastNode *origin_exp,
                                       FklByteCodelnt *bcl,
                                       FklCodegenMacro *next,
                                       uint32_t prototype_id, int own);
void fklDestroyCodegenMacro(FklCodegenMacro *macro);
void fklDestroyCodegenMacroList(FklCodegenMacro *macro);

FklCodegenMacroScope *fklCreateCodegenMacroScope(FklCodegenMacroScope *prev);
void fklDestroyCodegenMacroScope(FklCodegenMacroScope *c);

FklNastNode *fklTryExpandCodegenMacro(FklNastNode *exp, FklCodegenInfo *,
                                      FklCodegenMacroScope *macros,
                                      FklCodegenErrorState *);

FklVM *fklInitMacroExpandVM(FklByteCodelnt *bcl, FklFuncPrototypes *pts,
                            uint32_t prototype_id, FklPmatchHashMap *ht,
                            FklLineNumHashMap *lineHash,
                            FklCodegenLibVector *macroLibStack,
                            FklNastNode **pr, uint64_t curline,
                            FklSymbolTable *pst, FklConstTable *pkt);

void fklInitVMlibWithCodegenLibRefs(FklCodegenLib *clib, FklVMlib *vlib,
                                    FklVM *exe, FklVMvalue **refs,
                                    uint32_t count, int needCopy,
                                    FklFuncPrototypes *);

void fklInitVMlibWithCodegenLib(const FklCodegenLib *clib, FklVMlib *vlib,
                                FklVM *vm, int needCopy, FklFuncPrototypes *);

void fklInitVMlibWithCodegenLibAndDestroy(FklCodegenLib *clib, FklVMlib *vlib,
                                          FklVM *vm, FklFuncPrototypes *);

void fklRecomputeSidForSingleTableInfo(FklCodegenInfo *codegen,
                                       FklByteCodelnt *bcl,
                                       const FklSymbolTable *origin_st,
                                       FklSymbolTable *target_st,
                                       const FklConstTable *origin_kt,
                                       FklConstTable *target_kt,
                                       FklCodegenRecomputeNastSidOption option);

void fklRecomputeSidForNastNode(FklNastNode *node,
                                const FklSymbolTable *origin_table,
                                FklSymbolTable *target_table,
                                FklCodegenRecomputeNastSidOption option);

void fklRecomputeSidAndConstIdForBcl(FklByteCodelnt *bcl,
                                     const FklSymbolTable *origin_st,
                                     FklSymbolTable *target_st,
                                     const FklConstTable *origin_kt,
                                     FklConstTable *target_kt);

void fklRecomputeSidForNamedProdGroups(FklGraProdGroupHashMap *ht,
                                       const FklSymbolTable *origin_st,
                                       FklSymbolTable *target_st,
                                       const FklConstTable *origin_kt,
                                       FklConstTable *target_kt,
                                       FklCodegenRecomputeNastSidOption option);

void fklInitPreLibReaderMacros(FklCodegenLibVector *libStack,
                               FklSymbolTable *st,
                               FklCodegenOuterCtx *outer_ctx,
                               FklFuncPrototypes *pts,
                               FklCodegenLibVector *macroLibStack);

int fklLoadPreCompile(FklFuncPrototypes *info_pts,
                      FklFuncPrototypes *info_macro_pts,
                      FklCodegenLibVector *info_scriptLibStack,
                      FklCodegenLibVector *info_macroScriptLibStack,
                      FklSymbolTable *runtime_st, FklConstTable *runtime_kt,
                      FklCodegenOuterCtx *outer_ctx, const char *rp, FILE *fp,
                      char **errorStr);

FklProdActionFunc fklFindBuiltinProdActionByName(const char *str);
const FklSimpleProdAction *fklFindSimpleProdActionByName(const char *str);

void fklWriteNamedProds(const FklGraProdGroupHashMap *named_prod_groups,
                        const FklSymbolTable *st, FILE *fp);

void fklPrintReaderMacroAction(FILE *fp, const FklGrammerProduction *prod,
                               const FklSymbolTable *pst,
                               const FklConstTable *pkt);

void fklLoadNamedProds(FklGraProdGroupHashMap *ht, FklSymbolTable *st,
                       FklCodegenOuterCtx *outer_ctx, FILE *fp);

FklGrammerProduction *
fklCreateExtraStartProduction(FklCodegenOuterCtx *outer_ctx, FklSid_t group,
                              FklSid_t sid);

FklGrammerProduction *
fklCodegenProdPrintingToProduction(const FklCodegenProdPrinting *p,
                                   FklGrammer *g, FklCodegenOuterCtx *outer_ctx,
                                   FklFuncPrototypes *pts,
                                   FklCodegenLibVector *macroLibStack);

void fklWriteExportNamedProds(const FklSidHashSet *export_named_prod_groups,
                              const FklGraProdGroupHashMap *named_prod_groups,
                              const FklSymbolTable *st, FILE *fp);

#ifdef __cplusplus
}
#endif
#endif
