#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/regex.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>

#include "codegen.h"

static inline FklSymDefHashMapElm *
get_def_by_id_in_scope(FklSid_t id, uint32_t scopeId,
                       FklCodegenEnvScope *scope) {
    FklSidScope key = {id, scopeId};
    return fklSymDefHashMapAt(&scope->defs, &key);
}

FklSymDefHashMapElm *fklFindSymbolDefByIdAndScope(FklSid_t id, uint32_t scope,
                                                  const FklCodegenEnv *env) {
    FklCodegenEnvScope *scopes = env->scopes;
    for (; scope; scope = scopes[scope - 1].p) {
        FklSymDefHashMapElm *r =
            get_def_by_id_in_scope(id, scope, &scopes[scope - 1]);
        if (r)
            return r;
    }
    return NULL;
}

FklSymDefHashMapElm *fklGetCodegenDefByIdInScope(FklSid_t id, uint32_t scope,
                                                 const FklCodegenEnv *env) {
    return get_def_by_id_in_scope(id, scope, &env->scopes[scope - 1]);
}

void fklPrintCodegenError(FklNastNode *obj, FklBuiltinErrorType type,
                          const FklCodegenInfo *info,
                          const FklSymbolTable *symbolTable, size_t line,
                          FklSid_t fid,
                          const FklSymbolTable *publicSymbolTable) {
    static const char *builtInErrorType[] = {
        // clang-format off
        "dummy",
        "symbol-error",
        "syntax-error",
        "read-error",
        "load-error",
        "pattern-error",
        "type-error",
        "stack-error",
        "arg-error",
        "arg-error",
        "thread-error",
        "thread-error",
        "macro-error",
        "call-error",
        "load-error",
        "symbol-error",
        "library-error",
        "eof-error",
        "div-zero-error",
        "file-error",
        "value-error",
        "access-error",
        "access-error",
        "import-error",
        "macro-error",
        "type-error",
        "type-error",
        "call-error",
        "value-error",
        "value-error",
        "value-error",
        "value-error",
        "operation-error",
        "import-error",
        "export-error",
        "import-error",
        "grammer-error",
        "grammer-error",
        "grammer-error",
        "value-error",
        "symbol-error",
        "symbol-error",
        "syntax-error",
        // clang-format on
    };

    if (type == FKL_ERR_DUMMY || type == FKL_ERR_SYMUNDEFINE)
        return;
    fklPrintRawCstr(builtInErrorType[type], "|", "|", '|', stderr);
    fputs(": ", stderr);
    switch (type) {
    case FKL_ERR_CIR_REF:
        fputs("Circular reference occur in expanding macro", stderr);
        break;
    case FKL_ERR_SYMUNDEFINE:
        break;
    case FKL_ERR_SYNTAXERROR:
        fputs("Invalid syntax ", stderr);
        if (obj != NULL)
            fklPrintNastNode(obj, stderr, publicSymbolTable);
        break;
    case FKL_ERR_EXP_HAS_NO_VALUE:
        fputs("Expression ", stderr);
        if (obj != NULL)
            fklPrintNastNode(obj, stderr, publicSymbolTable);
        fputs(" has no value", stderr);
        break;
    case FKL_ERR_INVALIDEXPR:
        fputs("Invalid expression", stderr);
        if (obj != NULL) {
            fputc(' ', stderr);
            fklPrintNastNode(obj, stderr, publicSymbolTable);
        }
        break;
    case FKL_ERR_CIRCULARLOAD:
        fputs("Circular load file ", stderr);
        if (obj != NULL)
            fklPrintNastNode(obj, stderr, publicSymbolTable);
        break;
    case FKL_ERR_INVALIDPATTERN:
        fputs("Invalid string match pattern ", stderr);
        if (obj != NULL)
            fklPrintNastNode(obj, stderr, publicSymbolTable);
        break;
    case FKL_ERR_INVALID_MACRO_PATTERN:
        fputs("Invalid macro pattern ", stderr);
        if (obj != NULL)
            fklPrintNastNode(obj, stderr, publicSymbolTable);
        break;
    case FKL_ERR_MACROEXPANDFAILED:
        fputs("Failed to expand macro in ", stderr);
        if (obj != NULL)
            fklPrintNastNode(obj, stderr, publicSymbolTable);
        break;
    case FKL_ERR_LIBUNDEFINED:
        fputs("Library ", stderr);
        if (obj != NULL)
            fklPrintNastNode(obj, stderr, publicSymbolTable);
        fputs(" undefined", stderr);
        break;
    case FKL_ERR_FILEFAILURE:
        fputs("Failed for file: ", stderr);
        fklPrintNastNode(obj, stderr, publicSymbolTable);
        break;
    case FKL_ERR_IMPORTFAILED:
        fputs("Failed for importing module: ", stderr);
        fklPrintNastNode(obj, stderr, publicSymbolTable);
        break;
    case FKL_ERR_UNEXPECTED_EOF:
        fputs("Unexpected eof", stderr);
        break;
    case FKL_ERR_IMPORT_MISSING:
        fputs("Missing import for ", stderr);
        fklPrintNastNode(obj, stderr, publicSymbolTable);
        break;
    case FKL_ERR_EXPORT_OUTER_REF_PROD_GROUP:
        fputs("Exporting production groups with reference to other group",
              stderr);
        break;
    case FKL_ERR_IMPORT_READER_MACRO_ERROR:
        fputs("Failed to import reader macro", stderr);
        break;
    case FKL_ERR_GRAMMER_CREATE_FAILED:
        fputs("Failed to create grammer", stderr);
        break;
    case FKL_ERR_REGEX_COMPILE_FAILED:
        fputs("Failed to compile regex in ", stderr);
        fklPrintNastNode(obj, stderr, publicSymbolTable);
        break;
    case FKL_ERR_ASSIGN_CONSTANT:
        fputs("attempt to assign constant ", stderr);
        fklPrintNastNode(obj, stderr, publicSymbolTable);
        break;
    case FKL_ERR_REDEFINE_VARIABLE_AS_CONSTANT:
        fputs("attempt to redefine variable ", stderr);
        fklPrintNastNode(obj, stderr, publicSymbolTable);
        fputs(" as constant", stderr);
        break;
    default:
        fputs("Unknown compiling error", stderr);
        break;
    }
    fputc('\n', stderr);
    if (obj) {
        if (fid) {
            fprintf(stderr, "at line %" FKL_PRT64U " of file %s", obj->curline,
                    fklGetSymbolWithId(fid, publicSymbolTable)->k->str);
            fputc('\n', stderr);
        } else if (info->filename) {
            fprintf(stderr, "at line %" FKL_PRT64U " of file %s", obj->curline,
                    info->filename);
            fputc('\n', stderr);
        } else
            fprintf(stderr, "at line %" FKL_PRT64U "\n", obj->curline);
    } else {
        if (fid) {
            fprintf(stderr, "at line %" FKL_PRT64U " of file %s", obj->curline,
                    fklGetSymbolWithId(fid, publicSymbolTable)->k->str);
            fputc('\n', stderr);
        } else if (info->filename) {
            fprintf(stderr, "at line %" FKL_PRT64U " of file %s", line,
                    info->filename);
            fputc('\n', stderr);
        } else
            fprintf(stderr, "at line %" FKL_PRT64U " of file ", line);
    }
    if (obj)
        fklDestroyNastNode(obj);
}

#define INIT_SYMBOL_DEF(ID, SCOPE, IDX) {{ID, SCOPE}, IDX, IDX, 0, 0}

FklSymDefHashMapElm *fklAddCodegenBuiltinRefBySid(FklSid_t id,
                                                  FklCodegenEnv *env) {
    FklSymDefHashMap *ht = &env->refs;
    uint32_t idx = ht->count;
    return fklSymDefHashMapInsert2(
        ht, (FklSidScope){.id = id, .scope = env->pscope},
        (FklSymDef){.idx = idx, .cidx = idx, .isLocal = 0, .isConst = 0});
}

static inline void *has_outer_pdef_or_def(FklCodegenEnv *cur, FklSid_t id,
                                          uint32_t scope,
                                          FklCodegenEnv **targetEnv,
                                          int *is_pdef) {
    for (; cur; cur = cur->prev) {
        uint8_t *key = fklGetCodegenPreDefBySid(id, scope, cur);
        if (key) {
            *targetEnv = cur;
            *is_pdef = 1;
            return key;
        }
        FklSymDefHashMapElm *def = fklFindSymbolDefByIdAndScope(id, scope, cur);
        if (def) {
            *targetEnv = cur;
            return def;
        }
        scope = cur->pscope;
    }
    return NULL;
}

static inline void initSymbolDef(FklSymDef *def, uint32_t idx) {
    def->idx = idx;
    def->cidx = idx;
    def->isLocal = 0;
    def->isConst = 0;
}

static inline FklSymDefHashMapElm *
add_ref_to_all_penv(FklSid_t id, FklCodegenEnv *cur, FklCodegenEnv *targetEnv,
                    uint8_t isConst, FklSymDefHashMapElm **new_ref) {
    uint32_t idx = cur->refs.count;
    FklSymDefHashMapElm *cel = fklSymDefHashMapInsert2(
        &cur->refs, (FklSidScope){.id = id, .scope = cur->pscope},
        (FklSymDef){.idx = idx, .cidx = idx, .isConst = isConst, .isLocal = 0});
    *new_ref = cel;
    FklSidScope key = {.id = id, .scope = cur->pscope};
    FklSymDef def;
    for (cur = cur->prev; cur != targetEnv; cur = cur->prev) {
        uint32_t idx = cur->refs.count;
        key.scope = cur->pscope;
        initSymbolDef(&def, idx);
        FklSymDefHashMapElm *nel =
            fklSymDefHashMapInsert(&cur->refs, &key, &def);
        cel->v.cidx = nel->v.idx;
        cel = nel;
    }
    return cel;
}

static inline uint32_t get_child_env_prototype_id(FklCodegenEnv *cur,
                                                  FklCodegenEnv *target) {
    FKL_ASSERT(cur != target);
    for (; cur->prev != target; cur = cur->prev)
        ;
    return cur->prototypeId;
}

static inline FklSymDefHashMapElm *
has_outer_ref(FklCodegenEnv *cur, FklSid_t id, FklCodegenEnv **targetEnv) {
    FklSymDefHashMapElm *ref = NULL;
    FklSidScope key = {id, 0};
    for (; cur; cur = cur->prev) {
        key.scope = cur->pscope;
        ref = fklSymDefHashMapAt(&cur->refs, &key);
        if (ref) {
            *targetEnv = cur;
            return ref;
        }
    }
    return NULL;
}

static inline int is_ref_solved(FklSymDefHashMapElm *ref, FklCodegenEnv *env) {
    if (env) {
        uint32_t top = env->uref.size;
        FklUnReSymbolRef *refs = env->uref.base;
        for (uint32_t i = 0; i < top; i++) {
            FklUnReSymbolRef *cur = &refs[i];
            if (cur->id == ref->k.id && cur->scope == ref->k.scope)
                return 0;
        }
    }
    return 1;
}

static inline void initUnReSymbolRef(FklUnReSymbolRef *r, FklSid_t id,
                                     uint32_t idx, uint32_t scope,
                                     uint32_t prototypeId, uint32_t assign,
                                     FklSid_t fid, uint64_t line) {
    r->id = id;
    r->idx = idx;
    r->scope = scope, r->prototypeId = prototypeId;
    r->assign = assign;
    r->fid = fid;
    r->line = line;
}

static inline void initPdefRef(FklPreDefRef *r, FklSid_t id, uint32_t scope,
                               uint32_t prototypeId, uint32_t idx) {
    r->id = id;
    r->scope = scope;
    r->prototypeId = prototypeId;
    r->idx = idx;
}

FklSymDef *fklGetCodegenRefBySid(FklSid_t id, FklCodegenEnv *env) {
    FklSymDefHashMap *ht = &env->refs;
    return fklSymDefHashMapGet2(ht, (FklSidScope){id, env->pscope});
}

static inline FklUnReSymbolRef *has_resolvable_ref(FklSid_t id, uint32_t scope,
                                                   const FklCodegenEnv *env) {
    FklUnReSymbolRef *urefs = env->uref.base;
    uint32_t top = env->uref.size;
    for (uint32_t i = 0; i < top; i++) {
        FklUnReSymbolRef *cur = &urefs[i];
        if (cur->id == id && cur->scope == scope)
            return cur;
    }
    return NULL;
}

void fklAddCodegenPreDefBySid(FklSid_t id, uint32_t scope, uint8_t isConst,
                              FklCodegenEnv *env) {
    FklPredefHashMap *pdef = &env->pdef;
    FklSidScope key = {id, scope};
    fklPredefHashMapAdd(pdef, &key, &isConst);
}

uint8_t *fklGetCodegenPreDefBySid(FklSid_t id, uint32_t scope,
                                  FklCodegenEnv *env) {
    FklPredefHashMap *pdef = &env->pdef;
    FklSidScope key = {id, scope};
    return fklPredefHashMapGet(pdef, &key);
}

void fklAddCodegenRefToPreDef(FklSid_t id, uint32_t scope, uint32_t prototypeId,
                              uint32_t idx, FklCodegenEnv *env) {
    initPdefRef(fklPreDefRefVectorPushBack(&env->ref_pdef, NULL), id, scope,
                prototypeId, idx);
}

void fklResolveCodegenPreDef(FklSid_t id, uint32_t scope, FklCodegenEnv *env,
                             FklFuncPrototypes *pts) {
    FklPreDefRefVector *ref_pdef = &env->ref_pdef;
    FklFuncPrototype *pa = pts->pa;
    FklPreDefRefVector ref_pdef1;
    uint32_t count = ref_pdef->size;
    fklPreDefRefVectorInit(&ref_pdef1, count);
    uint8_t pdef_isconst;
    FklSidScope key = {id, scope};
    fklPredefHashMapEarase(&env->pdef, &key, &pdef_isconst, NULL);
    FklSymDefHashMapElm *def = fklGetCodegenDefByIdInScope(id, scope, env);
    FKL_ASSERT(def);
    for (uint32_t i = 0; i < count; i++) {
        const FklPreDefRef *pdef_ref = &ref_pdef->base[i];
        if (pdef_ref->id == id && pdef_ref->scope == scope) {
            FklFuncPrototype *cpt = &pa[pdef_ref->prototypeId];
            FklSymDefHashMapMutElm *ref = &cpt->refs[pdef_ref->idx];
            ref->v.cidx = def->v.idx;
            env->slotFlags[def->v.idx] = FKL_CODEGEN_ENV_SLOT_REF;
        } else
            fklPreDefRefVectorPushBack(&ref_pdef1, pdef_ref);
    }
    ref_pdef->size = 0;
    while (!fklPreDefRefVectorIsEmpty(&ref_pdef1))
        fklPreDefRefVectorPushBack2(ref_pdef,
                                    *fklPreDefRefVectorPopBack(&ref_pdef1));
    fklPreDefRefVectorUninit(&ref_pdef1);
}

void fklClearCodegenPreDef(FklCodegenEnv *env) {
    fklPredefHashMapClear(&env->pdef);
}

FklSymDefHashMapElm *fklAddCodegenRefBySid(FklSid_t id, FklCodegenEnv *env,
                                           FklSid_t fid, uint64_t line,
                                           uint32_t assign) {
    FklSymDefHashMap *refs = &env->refs;
    FklSymDefHashMapElm *el =
        fklSymDefHashMapAt2(refs, (FklSidScope){id, env->pscope});
    if (el) {
        FklUnReSymbolRef *ref =
            has_resolvable_ref(id, env->pscope, env->prev ? env->prev : env);
        if (assign && ref && !ref->assign) {
            ref->assign = 1;
            ref->fid = fid;
            ref->line = line;
        }
        return el;
    } else {
        FklSymDefHashMapElm *ret = NULL;
        uint32_t idx = refs->count;
        FklCodegenEnv *prev = env->prev;
        if (prev) {
            FklCodegenEnv *targetEnv = NULL;
            int is_pdef = 0;
            void *targetDef = has_outer_pdef_or_def(prev, id, env->pscope,
                                                    &targetEnv, &is_pdef);
            if (targetDef) {
                if (is_pdef) {
                    uint8_t *pdef = FKL_TYPE_CAST(uint8_t *, targetDef);
                    FklSymDefHashMapElm *cel =
                        add_ref_to_all_penv(id, env, targetEnv, *pdef, &ret);
                    cel->v.isLocal = 1;
                    cel->v.cidx = FKL_VAR_REF_INVALID_CIDX;
                    fklAddCodegenRefToPreDef(
                        id, env->pscope,
                        get_child_env_prototype_id(env, targetEnv), cel->v.idx,
                        targetEnv);
                } else {
                    FklSymDefHashMapElm *def =
                        FKL_TYPE_CAST(FklSymDefHashMapElm *, targetDef);
                    FklSymDefHashMapElm *cel = add_ref_to_all_penv(
                        id, env, targetEnv, def->v.isConst, &ret);
                    cel->v.isLocal = 1;
                    cel->v.cidx = def->v.idx;
                    targetEnv->slotFlags[def->v.idx] = FKL_CODEGEN_ENV_SLOT_REF;
                }
            } else {
                FklSymDefHashMapElm *targetRef =
                    has_outer_ref(prev, id, &targetEnv);
                if (targetRef && is_ref_solved(targetRef, targetEnv->prev))
                    add_ref_to_all_penv(id, env, targetEnv->prev,
                                        targetRef->v.isConst, &ret);
                else {
                    ret = fklSymDefHashMapInsert2(
                        refs, (FklSidScope){.id = id, .scope = env->pscope},
                        (FklSymDef){.idx = idx, .cidx = idx});
                    ret->v.cidx = FKL_VAR_REF_INVALID_CIDX;

                    initUnReSymbolRef(
                        fklUnReSymbolRefVectorPushBack(&prev->uref, NULL), id,
                        idx, env->pscope, env->prototypeId, assign, fid, line);
                }
            }
        } else {
            ret = fklSymDefHashMapInsert2(refs,
                                          (FklSidScope){.id = id, .scope = 0},
                                          (FklSymDef){.idx = idx, .cidx = idx});
            idx = ret->v.idx;
            initUnReSymbolRef(fklUnReSymbolRefVectorPushBack(&env->uref, NULL),
                              id, idx, 0, env->prototypeId, assign, fid, line);
        }
        return ret;
    }
}

uint32_t fklAddCodegenRefBySidRetIndex(FklSid_t id, FklCodegenEnv *env,
                                       FklSid_t fid, uint64_t line,
                                       uint32_t assign) {
    return fklAddCodegenRefBySid(id, env, fid, line, assign)->v.idx;
}

int fklIsSymbolDefined(FklSid_t id, uint32_t scope, FklCodegenEnv *env) {
    return get_def_by_id_in_scope(id, scope, &env->scopes[scope - 1]) != NULL;
}

static inline uint32_t get_next_empty(uint32_t empty, uint8_t *flags,
                                      uint32_t lcount) {
    for (; empty < lcount && flags[empty]; empty++)
        ;
    return empty;
}

FklSymDef *fklAddCodegenDefBySid(FklSid_t id, uint32_t scopeId,
                                 FklCodegenEnv *env) {
    FklCodegenEnvScope *scope = &env->scopes[scopeId - 1];
    FklSymDefHashMap *defs = &scope->defs;
    FklSidScope key = {id, scopeId};
    FklSymDef *el = fklSymDefHashMapGet(defs, &key);
    if (!el) {
        uint32_t idx = scope->empty;
        el = fklSymDefHashMapAdd(defs, &key, NULL);
        if (idx < env->lcount && has_resolvable_ref(id, scopeId, env))
            idx = env->lcount;
        else
            scope->empty =
                get_next_empty(scope->empty + 1, env->slotFlags, env->lcount);
        el->idx = idx;
        uint32_t end = (idx + 1) - scope->start;
        if (scope->end < end)
            scope->end = end;
        if (idx >= env->lcount) {
            env->lcount = idx + 1;
            uint8_t *slotFlags = (uint8_t *)fklZrealloc(
                env->slotFlags, env->lcount * sizeof(uint8_t));
            FKL_ASSERT(slotFlags);
            env->slotFlags = slotFlags;
        }
        env->slotFlags[idx] = FKL_CODEGEN_ENV_SLOT_OCC;
    }
    return el;
}

struct RecomputeSidAndConstIdCtx {
    const FklSymbolTable *origin_st;
    FklSymbolTable *target_st;

    const FklConstTable *origin_kt;
    FklConstTable *target_kt;
};

#define IS_PUSH_SYM_OP(OP)                                                     \
    ((OP) >= FKL_OP_PUSH_SYM && (OP) <= FKL_OP_PUSH_SYM_X)
#define IS_PUSH_F64_OP(OP)                                                     \
    ((OP) >= FKL_OP_PUSH_F64 && (OP) <= FKL_OP_PUSH_F64_X)
#define IS_PUSH_I64F_OP(OP)                                                    \
    ((OP) >= FKL_OP_PUSH_I64F && (OP) <= FKL_OP_PUSH_I64F_X)
#define IS_PUSH_I64B_OP(OP)                                                    \
    ((OP) >= FKL_OP_PUSH_I64B && (OP) <= FKL_OP_PUSH_I64B_X)
#define IS_PUSH_STR_OP(OP)                                                     \
    ((OP) >= FKL_OP_PUSH_STR && (OP) <= FKL_OP_PUSH_STR_X)
#define IS_PUSH_BVEC_OP(OP)                                                    \
    ((OP) >= FKL_OP_PUSH_BVEC && (OP) <= FKL_OP_PUSH_BVEC_X)
#define IS_PUSH_BI_OP(OP) ((OP) >= FKL_OP_PUSH_BI && (OP) <= FKL_OP_PUSH_BI_X)

static int recompute_sid_and_const_id_predicate(FklOpcode op) {
    return IS_PUSH_SYM_OP(op) || IS_PUSH_I64F_OP(op) || IS_PUSH_I64B_OP(op)
        || IS_PUSH_F64_OP(op) || IS_PUSH_STR_OP(op) || IS_PUSH_BVEC_OP(op)
        || IS_PUSH_BI_OP(op);
}

static int recompute_sid_and_const_id_func(void *cctx, FklOpcode *popcode,
                                           FklOpcodeMode *pmode,
                                           FklInstructionArg *ins_arg) {
    struct RecomputeSidAndConstIdCtx *ctx = cctx;
    FklOpcode op = *popcode;
    *pmode = FKL_OP_MODE_IuB;
    if (IS_PUSH_SYM_OP(op)) {
        replace_sid(&ins_arg->ux, ctx->origin_st, ctx->target_st);
        *popcode = FKL_OP_PUSH_SYM;
    } else if (IS_PUSH_I64F_OP(op)) {
        ins_arg->ux = fklAddI64Const(
            ctx->target_kt, fklGetI64ConstWithIdx(ctx->origin_kt, ins_arg->ux));
        *popcode = FKL_OP_PUSH_I64F;
    } else if (IS_PUSH_I64B_OP(op)) {
        ins_arg->ux = fklAddI64Const(
            ctx->target_kt, fklGetI64ConstWithIdx(ctx->origin_kt, ins_arg->ux));
        *popcode = FKL_OP_PUSH_I64B;
    } else if (IS_PUSH_F64_OP(op)) {
        ins_arg->ux = fklAddF64Const(
            ctx->target_kt, fklGetF64ConstWithIdx(ctx->origin_kt, ins_arg->ux));
        *popcode = FKL_OP_PUSH_F64;
    } else if (IS_PUSH_STR_OP(op)) {
        ins_arg->ux = fklAddStrConst(
            ctx->target_kt, fklGetStrConstWithIdx(ctx->origin_kt, ins_arg->ux));
        *popcode = FKL_OP_PUSH_STR;
    } else if (IS_PUSH_BVEC_OP(op)) {
        ins_arg->ux = fklAddBvecConst(
            ctx->target_kt,
            fklGetBvecConstWithIdx(ctx->origin_kt, ins_arg->ux));
        *popcode = FKL_OP_PUSH_BVEC;
    } else if (IS_PUSH_BI_OP(op)) {
        ins_arg->ux = fklAddBigIntConst(
            ctx->target_kt, fklGetBiConstWithIdx(ctx->origin_kt, ins_arg->ux));
        *popcode = FKL_OP_PUSH_BI;
    }
    return 0;
}

#undef IS_PUSH_SYM_OP
#undef IS_PUSH_F64_OP
#undef IS_PUSH_I64F_OP
#undef IS_PUSH_I64B_OP
#undef IS_PUSH_STR_OP
#undef IS_PUSH_BVEC_OP
#undef IS_PUSH_BI_OP

void fklRecomputeSidAndConstIdForBcl(FklByteCodelnt *bcl,
                                     const FklSymbolTable *origin_st,
                                     FklSymbolTable *target_st,
                                     const FklConstTable *origin_kt,
                                     FklConstTable *target_kt) {
    struct RecomputeSidAndConstIdCtx ctx = {
        .origin_st = origin_st,
        .target_st = target_st,
        .origin_kt = origin_kt,
        .target_kt = target_kt,
    };
    fklRecomputeInsImm(bcl, &ctx, recompute_sid_and_const_id_predicate,
                       recompute_sid_and_const_id_func);

    FklLineNumberTableItem *lnt_start = bcl->l;
    FklLineNumberTableItem *lnt_end = lnt_start + bcl->ls;
    for (; lnt_start < lnt_end; lnt_start++)
        replace_sid(&lnt_start->fid, origin_st, target_st);
}

void fklRecomputeSidForNastNode(FklNastNode *node,
                                const FklSymbolTable *origin_table,
                                FklSymbolTable *target_table,
                                FklCodegenRecomputeNastSidOption option) {
    FklNastNodeVector pending;
    fklNastNodeVectorInit(&pending, 16);
    fklNastNodeVectorPushBack2(&pending, node);
    while (!fklNastNodeVectorIsEmpty(&pending)) {
        FklNastNode *top = *fklNastNodeVectorPopBack(&pending);
        switch (top->type) {
        case FKL_NAST_SLOT:
            abort();
            break;
        case FKL_NAST_SYM:
            replace_sid(&top->sym, origin_table, target_table);
            if (option == FKL_CODEGEN_SID_RECOMPUTE_MARK_SYM_AS_RC_SYM)
                top->type = FKL_NAST_RC_SYM;
            break;
        case FKL_NAST_BOX:
            fklNastNodeVectorPushBack2(&pending, top->box);
            break;
        case FKL_NAST_PAIR:
            fklNastNodeVectorPushBack2(&pending, top->pair->car);
            fklNastNodeVectorPushBack2(&pending, top->pair->cdr);
            break;
        case FKL_NAST_VECTOR:
            for (size_t i = 0; i < top->vec->size; i++)
                fklNastNodeVectorPushBack2(&pending, top->vec->base[i]);
            break;
        case FKL_NAST_HASHTABLE:
            for (size_t i = 0; i < top->hash->num; i++) {
                fklNastNodeVectorPushBack2(&pending, top->hash->items[i].car);
                fklNastNodeVectorPushBack2(&pending, top->hash->items[i].cdr);
            }
            break;
        default:
            break;
        }
    }

    fklNastNodeVectorUninit(&pending);
}

static inline void
recompute_sid_for_prototypes(FklFuncPrototypes *pts,
                             const FklSymbolTable *origin_table,
                             FklSymbolTable *target_table) {
    uint32_t end = pts->count + 1;

    for (uint32_t i = 1; i < end; i++) {
        FklFuncPrototype *cur = &pts->pa[i];
        replace_sid(&cur->fid, origin_table, target_table);
        if (cur->sid)
            replace_sid(&cur->sid, origin_table, target_table);

        for (uint32_t i = 0; i < cur->rcount; i++)
            replace_sid(FKL_REMOVE_CONST(FklSid_t, &cur->refs[i].k.id),
                        origin_table, target_table);
    }
}

static inline void
recompute_sid_for_export_sid_index(FklCgExportSidIdxHashMap *t,
                                   const FklSymbolTable *origin_table,
                                   FklSymbolTable *target_table) {
    for (FklCgExportSidIdxHashMapNode *sid_idx_list = t->first; sid_idx_list;
         sid_idx_list = sid_idx_list->next) {
        replace_sid(FKL_REMOVE_CONST(FklSid_t, &sid_idx_list->k), origin_table,
                    target_table);
    }
}

static inline void recompute_sid_for_compiler_macros(
    FklCodegenMacro *m, const FklSymbolTable *origin_st,
    FklSymbolTable *target_st, const FklConstTable *origin_kt,
    FklConstTable *target_kt, FklCodegenRecomputeNastSidOption option) {
    for (; m; m = m->next) {
        fklRecomputeSidForNastNode(m->origin_exp, origin_st, target_st, option);
        if (m->own)
            fklRecomputeSidAndConstIdForBcl(m->bcl, origin_st, target_st,
                                            origin_kt, target_kt);
    }
}

static inline void recompute_sid_for_replacements(
    FklReplacementHashMap *t, const FklSymbolTable *origin_table,
    FklSymbolTable *target_table, FklCodegenRecomputeNastSidOption option) {
    for (FklReplacementHashMapNode *rep_list = t->first; rep_list;
         rep_list = rep_list->next) {
        replace_sid(FKL_REMOVE_CONST(FklSid_t, &rep_list->k), origin_table,
                    target_table);
        fklRecomputeSidForNastNode(rep_list->v, origin_table, target_table,
                                   option);
    }
}

static inline void recompute_sid_for_script_lib(
    FklCodegenLib *lib, const FklSymbolTable *origin_st,
    FklSymbolTable *target_st, const FklConstTable *origin_kt,
    FklConstTable *target_kt, FklCodegenRecomputeNastSidOption option) {
    fklRecomputeSidAndConstIdForBcl(lib->bcl, origin_st, target_st, origin_kt,
                                    target_kt);
    recompute_sid_for_export_sid_index(&lib->exports, origin_st, target_st);
    recompute_sid_for_compiler_macros(lib->head, origin_st, target_st,
                                      origin_kt, target_kt, option);
    recompute_sid_for_replacements(lib->replacements, origin_st, target_st,
                                   option);
    fklRecomputeSidForNamedProdGroups(&lib->named_prod_groups, origin_st,
                                      target_st, origin_kt, target_kt, option);
}

static inline void recompute_sid_for_dll_lib(FklCodegenLib *lib,
                                             const FklSymbolTable *origin_table,
                                             FklSymbolTable *target_table) {
    recompute_sid_for_export_sid_index(&lib->exports, origin_table,
                                       target_table);
}

static inline void recompute_sid_for_lib_stack(
    FklCodegenLibVector *loadedLibStack, const FklSymbolTable *origin_st,
    FklSymbolTable *target_st, const FklConstTable *origin_kt,
    FklConstTable *target_kt, FklCodegenRecomputeNastSidOption option) {
    for (size_t i = 0; i < loadedLibStack->size; i++) {
        FklCodegenLib *lib = &loadedLibStack->base[i];
        switch (lib->type) {
        case FKL_CODEGEN_LIB_SCRIPT:
            recompute_sid_for_script_lib(lib, origin_st, target_st, origin_kt,
                                         target_kt, option);
            break;
        case FKL_CODEGEN_LIB_DLL:
            recompute_sid_for_dll_lib(lib, origin_st, target_st);
            break;
        }
    }
}

static inline void recompute_sid_for_double_st_script_lib(
    FklCodegenLib *lib, const FklSymbolTable *origin_st,
    FklSymbolTable *runtime_st, FklSymbolTable *public_st,
    const FklConstTable *origin_kt, FklConstTable *runtime_kt,
    FklConstTable *public_kt, FklCodegenRecomputeNastSidOption option) {
    fklRecomputeSidAndConstIdForBcl(lib->bcl, origin_st, runtime_st, origin_kt,
                                    runtime_kt);
    recompute_sid_for_export_sid_index(&lib->exports, origin_st, public_st);
    recompute_sid_for_compiler_macros(lib->head, origin_st, public_st,
                                      origin_kt, public_kt, option);
    recompute_sid_for_replacements(lib->replacements, origin_st, public_st,
                                   option);
    fklRecomputeSidForNamedProdGroups(&lib->named_prod_groups, origin_st,
                                      public_st, origin_kt, public_kt, option);
}

static inline void recompute_sid_for_double_st_lib_stack(
    FklCodegenLibVector *loadedLibStack, const FklSymbolTable *origin_st,
    FklSymbolTable *runtime_st, FklSymbolTable *public_st,
    const FklConstTable *origin_kt, FklConstTable *runtime_kt,
    FklConstTable *public_kt, FklCodegenRecomputeNastSidOption option) {
    for (size_t i = 0; i < loadedLibStack->size; i++) {
        FklCodegenLib *lib = &loadedLibStack->base[i];
        switch (lib->type) {
        case FKL_CODEGEN_LIB_SCRIPT:
            recompute_sid_for_double_st_script_lib(
                lib, origin_st, runtime_st, public_st, origin_kt, runtime_kt,
                public_kt, option);
            break;
        case FKL_CODEGEN_LIB_DLL:
            recompute_sid_for_dll_lib(lib, origin_st, public_st);
            break;
        }
    }
}

static inline void recompute_sid_for_sid_set(FklSidHashSet *ht,
                                             const FklSymbolTable *ost,
                                             FklSymbolTable *tst) {
    for (FklSidHashSetNode *l = ht->first; l; l = l->next) {
        FklSid_t *id = FKL_REMOVE_CONST(FklSid_t, &l->k);
        replace_sid(id, ost, tst);
        *FKL_REMOVE_CONST(uintptr_t, &l->hashv) =
            fklGraProdGroupHashMap__hashv(id);
    }
    fklSidHashSetRehash(ht);
}

static inline void recompute_sid_for_main_file(
    FklCodegenInfo *codegen, FklByteCodelnt *bcl,
    const FklSymbolTable *origin_st, FklSymbolTable *target_st,
    const FklConstTable *origin_kt, FklConstTable *target_kt,
    FklCodegenRecomputeNastSidOption option) {
    fklRecomputeSidAndConstIdForBcl(bcl, origin_st, target_st, origin_kt,
                                    target_kt);
    recompute_sid_for_export_sid_index(&codegen->exports, origin_st, target_st);
    recompute_sid_for_compiler_macros(codegen->export_macro, origin_st,
                                      target_st, origin_kt, target_kt, option);
    recompute_sid_for_replacements(codegen->export_replacement, origin_st,
                                   target_st, option);
    recompute_sid_for_sid_set(codegen->export_named_prod_groups, origin_st,
                              target_st);
    fklRecomputeSidForNamedProdGroups(codegen->named_prod_groups, origin_st,
                                      target_st, origin_kt, target_kt, option);
}

void fklRecomputeSidForSingleTableInfo(
    FklCodegenInfo *codegen, FklByteCodelnt *bcl,
    const FklSymbolTable *origin_st, FklSymbolTable *target_st,
    const FklConstTable *origin_kt, FklConstTable *target_kt,
    FklCodegenRecomputeNastSidOption option) {
    recompute_sid_for_prototypes(codegen->pts, origin_st, target_st);
    recompute_sid_for_prototypes(codegen->macro_pts, origin_st, target_st);
    recompute_sid_for_main_file(codegen, bcl, origin_st, target_st, origin_kt,
                                target_kt, option);
    recompute_sid_for_lib_stack(codegen->libStack, origin_st, target_st,
                                origin_kt, target_kt, option);
    recompute_sid_for_lib_stack(codegen->macroLibStack, origin_st, target_st,
                                origin_kt, target_kt, option);
}

#include <fakeLisp/parser.h>
#include <string.h>

static inline char *load_script_lib_path(const char *main_dir, FILE *fp) {
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    fklStringBufferConcatWithCstr(&buf, main_dir);
    int ch = fgetc(fp);
    for (;;) {
        while (ch) {
            fklStringBufferPutc(&buf, ch);
            ch = fgetc(fp);
        }
        ch = fgetc(fp);
        if (!ch)
            break;
        fklStringBufferPutc(&buf, FKL_PATH_SEPARATOR);
    }

    fklStringBufferPutc(&buf, FKL_PRE_COMPILE_FKL_SUFFIX);

    char *path = fklZstrdup(buf.buf);
    fklUninitStringBuffer(&buf);
    return path;
}

static inline void load_export_sid_idx_table(FklCgExportSidIdxHashMap *t,
                                             FILE *fp) {
    fklCgExportSidIdxHashMapInit(t);
    fread(&t->count, sizeof(t->count), 1, fp);
    uint32_t num = t->count;
    t->count = 0;
    for (uint32_t i = 0; i < num; i++) {
        FklSid_t sid;
        FklCodegenExportIdx idxs;
        fread(&sid, sizeof(sid), 1, fp);
        fread(&idxs.idx, sizeof(idxs.idx), 1, fp);
        fread(&idxs.oidx, sizeof(idxs.oidx), 1, fp);
        fklCgExportSidIdxHashMapPut(t, &sid, &idxs);
    }
}

static inline FklCodegenMacro *load_compiler_macros(FklSymbolTable *st,
                                                    FILE *fp) {
    uint64_t count = 0;
    fread(&count, sizeof(count), 1, fp);
    FklCodegenMacro *r = NULL;
    FklCodegenMacro **pr = &r;
    for (uint64_t i = 0; i < count; i++) {
        FklNastNode *node = load_nast_node_with_null_chr(st, fp);
        uint32_t prototype_id = 0;
        fread(&prototype_id, sizeof(prototype_id), 1, fp);
        FklByteCodelnt *bcl = fklLoadByteCodelnt(fp);
        FklNastNode *pattern = fklCreatePatternFromNast(node, NULL);
        FklCodegenMacro *cur =
            fklCreateCodegenMacro(pattern, node, bcl, NULL, prototype_id, 1);
        *pr = cur;
        pr = &cur->next;
    }
    return r;
}

static inline FklReplacementHashMap *load_replacements(FklSymbolTable *st,
                                                       FILE *fp) {
    FklReplacementHashMap *ht = fklReplacementHashMapCreate();
    fread(&ht->count, sizeof(ht->count), 1, fp);
    uint32_t num = ht->count;
    ht->count = 0;
    for (uint32_t i = 0; i < num; i++) {
        FklSid_t id = 0;
        fread(&id, sizeof(id), 1, fp);
        FklNastNode *node = load_nast_node_with_null_chr(st, fp);
        *fklReplacementHashMapAdd1(ht, id) = node;
    }
    return ht;
}

static inline void
load_script_lib_from_pre_compile(FklCodegenLib *lib, FklSymbolTable *st,
                                 FklCodegenOuterCtx *outer_ctx,
                                 const char *main_dir, FILE *fp) {
    lib->rp = load_script_lib_path(main_dir, fp);
    fread(&lib->prototypeId, sizeof(lib->prototypeId), 1, fp);
    lib->bcl = fklLoadByteCodelnt(fp);
    load_export_sid_idx_table(&lib->exports, fp);
    lib->head = load_compiler_macros(st, fp);
    lib->replacements = load_replacements(st, fp);
    lib->named_prod_groups.buckets = NULL;
    // lib->terminal_table.ht.buckets = NULL;
    fklLoadNamedProds(&lib->named_prod_groups, st, outer_ctx, fp);
}

static inline char *load_dll_lib_path(const char *main_dir, FILE *fp) {
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    fklStringBufferConcatWithCstr(&buf, main_dir);
    int ch = fgetc(fp);
    for (;;) {
        while (ch) {
            fklStringBufferPutc(&buf, ch);
            ch = fgetc(fp);
        }
        ch = fgetc(fp);
        if (!ch)
            break;
        fklStringBufferPutc(&buf, FKL_PATH_SEPARATOR);
    }

    fklStringBufferConcatWithCstr(&buf, FKL_DLL_FILE_TYPE);

    char *path = fklZstrdup(buf.buf);
    fklUninitStringBuffer(&buf);
    char *rp = fklRealpath(path);
    fklZfree(path);
    return rp;
}

static inline int load_dll_lib_from_pre_compile(FklCodegenLib *lib,
                                                FklSymbolTable *st,
                                                const char *main_dir, FILE *fp,
                                                char **errorStr) {
    lib->rp = load_dll_lib_path(main_dir, fp);
    if (!lib->rp || !fklIsAccessibleRegFile(lib->rp)) {
        fklZfree(lib->rp);
        return 1;
    }

    if (uv_dlopen(lib->rp, &lib->dll)) {
        *errorStr = fklZstrdup(uv_dlerror(&lib->dll));
        uv_dlclose(&lib->dll);
        fklZfree(lib->rp);
        return 1;
    }
    load_export_sid_idx_table(&lib->exports, fp);
    return 0;
}

static inline int load_lib_from_pre_compile(FklCodegenLib *lib,
                                            FklSymbolTable *st,
                                            FklCodegenOuterCtx *outer_ctx,
                                            const char *main_dir, FILE *fp,
                                            char **errorStr) {
    uint8_t type = 0;
    fread(&type, sizeof(type), 1, fp);
    lib->type = type;
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        load_script_lib_from_pre_compile(lib, st, outer_ctx, main_dir, fp);
        break;
    case FKL_CODEGEN_LIB_DLL:
        if (load_dll_lib_from_pre_compile(lib, st, main_dir, fp, errorStr))
            return 1;
        break;
    }
    return 0;
}

static inline int load_imported_lib_stack(FklCodegenLibVector *libStack,
                                          FklSymbolTable *st,
                                          FklCodegenOuterCtx *outer_ctx,
                                          const char *main_dir, FILE *fp,
                                          char **errorStr) {
    uint64_t num = 0;
    fread(&num, sizeof(num), 1, fp);
    fklCodegenLibVectorInit(libStack, num + 1);
    for (size_t i = 0; i < num; i++) {
        FklCodegenLib lib = {0};
        if (load_lib_from_pre_compile(&lib, st, outer_ctx, main_dir, fp,
                                      errorStr))
            return 1;
        fklCodegenLibVectorPushBack(libStack, &lib);
    }
    FklCodegenLib main_lib = {0};
    main_lib.named_prod_groups.buckets = NULL;
    main_lib.type = FKL_CODEGEN_LIB_SCRIPT;
    main_lib.rp = load_script_lib_path(main_dir, fp);
    main_lib.bcl = fklLoadByteCodelnt(fp);
    main_lib.prototypeId = 1;
    load_export_sid_idx_table(&main_lib.exports, fp);
    main_lib.head = load_compiler_macros(st, fp);
    main_lib.replacements = load_replacements(st, fp);
    fklLoadNamedProds(&main_lib.named_prod_groups, st, outer_ctx, fp);
    fklCodegenLibVectorPushBack(libStack, &main_lib);
    return 0;
}

static inline int load_macro_lib_stack(FklCodegenLibVector *libStack,
                                       FklSymbolTable *st,
                                       FklCodegenOuterCtx *outer_ctx,
                                       const char *main_dir, FILE *fp,
                                       char **errorStr) {
    uint64_t num = 0;
    fread(&num, sizeof(num), 1, fp);
    fklCodegenLibVectorInit(libStack, num);
    for (size_t i = 0; i < num; i++) {
        FklCodegenLib lib = {0};
        if (load_lib_from_pre_compile(&lib, st, outer_ctx, main_dir, fp,
                                      errorStr))
            return 1;
        fklCodegenLibVectorPushBack(libStack, &lib);
    }
    return 0;
}

#define IS_LOAD_DLL_OP(OP)                                                     \
    ((OP) >= FKL_OP_LOAD_DLL && (OP) <= FKL_OP_LOAD_DLL_X)
#define IS_LOAD_LIB_OP(OP)                                                     \
    ((OP) >= FKL_OP_LOAD_LIB && (OP) <= FKL_OP_LOAD_LIB_X)
#define IS_PUSH_PROC_OP(OP)                                                    \
    ((OP) >= FKL_OP_PUSH_PROC && (OP) <= FKL_OP_PUSH_PROC_XXX)
#define IS_EXPORT_TO_OP(OP)                                                    \
    ((OP) >= FKL_OP_EXPORT_TO && (OP) <= FKL_OP_EXPORT_TO_XX)

struct IncreaseBclLibPrototypeCtx {
    uint32_t lib_count;
    uint32_t pts_count;
};

static int increase_bcl_lib_prototype_id_predicate(FklOpcode op) {
    return IS_LOAD_LIB_OP(op) || IS_LOAD_DLL_OP(op) || IS_PUSH_PROC_OP(op)
        || IS_EXPORT_TO_OP(op);
}

static int increase_bcl_lib_prototype_id_func(void *cctx, FklOpcode *popcode,
                                              FklOpcodeMode *pmode,
                                              FklInstructionArg *ins_arg) {
    struct IncreaseBclLibPrototypeCtx *ctx = cctx;
    FklOpcode op = *popcode;
    if (IS_LOAD_DLL_OP(op)) {
        ins_arg->ux += ctx->lib_count;
        *popcode = FKL_OP_LOAD_DLL;
        *pmode = FKL_OP_MODE_IuB;
    } else if (IS_LOAD_LIB_OP(op)) {
        ins_arg->ux += ctx->lib_count;
        *popcode = FKL_OP_LOAD_LIB;
        *pmode = FKL_OP_MODE_IuB;
    } else if (IS_EXPORT_TO_OP(op)) {
        ins_arg->ux += ctx->lib_count;
        *popcode = FKL_OP_EXPORT_TO;
        *pmode = FKL_OP_MODE_IuAuB;
    } else if (IS_PUSH_PROC_OP(op)) {
        ins_arg->ux += ctx->pts_count;
        *popcode = FKL_OP_PUSH_PROC;
        *pmode = FKL_OP_MODE_IuAuB;
    }
    return 0;
}

static inline void increase_bcl_lib_prototype_id(FklByteCodelnt *bcl,
                                                 uint32_t lib_count,
                                                 uint32_t pts_count) {
    struct IncreaseBclLibPrototypeCtx ctx = {
        .lib_count = lib_count,
        .pts_count = pts_count,
    };
    fklRecomputeInsImm(bcl, &ctx, increase_bcl_lib_prototype_id_predicate,
                       increase_bcl_lib_prototype_id_func);
}

static inline void increase_compiler_macros_lib_prototype_id(
    FklCodegenMacro *head, uint32_t macro_lib_count, uint32_t macro_pts_count) {
    for (; head; head = head->next) {
        head->prototype_id += macro_pts_count;
        increase_bcl_lib_prototype_id(head->bcl, macro_lib_count,
                                      macro_pts_count);
    }
}

static inline void increase_reader_macro_lib_prototype_id(
    FklGraProdGroupHashMap *named_prod_groups, uint32_t lib_count,
    uint32_t count) {
    if (named_prod_groups->buckets) {
        for (FklGraProdGroupHashMapNode *list = named_prod_groups->first; list;
             list = list->next) {
            uint32_t top = list->v.prod_printing.size;
            for (uint32_t i = 0; i < top; i++) {
                FklCodegenProdPrinting *p = &list->v.prod_printing.base[i];
                if (p->type == FKL_CODEGEN_PROD_CUSTOM) {
                    p->prototype_id += count;
                    increase_bcl_lib_prototype_id(p->bcl, lib_count, count);
                }
            }
        }
    }
}

static inline void increase_lib_id_and_prototype_id(FklCodegenLib *lib,
                                                    uint32_t lib_count,
                                                    uint32_t macro_lib_count,
                                                    uint32_t pts_count,
                                                    uint32_t macro_pts_count) {
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        lib->prototypeId += pts_count;
        increase_bcl_lib_prototype_id(lib->bcl, lib_count, pts_count);
        increase_compiler_macros_lib_prototype_id(lib->head, macro_lib_count,
                                                  macro_pts_count);
        increase_reader_macro_lib_prototype_id(
            &lib->named_prod_groups, macro_lib_count, macro_pts_count);
        break;
    case FKL_CODEGEN_LIB_DLL:
        break;
    }
}

static inline void
increase_prototype_and_lib_id(uint32_t pts_count, uint32_t macro_pts_count,
                              uint32_t lib_count, uint32_t macro_lib_count,
                              FklCodegenLibVector *libStack,
                              FklCodegenLibVector *macroLibStack) {
    uint32_t top = libStack->size;
    FklCodegenLib *base = libStack->base;
    for (uint32_t i = 0; i < top; i++) {
        increase_lib_id_and_prototype_id(&base[i], lib_count, macro_lib_count,
                                         pts_count, macro_pts_count);
    }

    top = macroLibStack->size;
    base = macroLibStack->base;
    for (uint32_t i = 0; i < top; i++) {
        increase_lib_id_and_prototype_id(&base[i], macro_lib_count,
                                         macro_lib_count, pts_count,
                                         macro_pts_count);
    }
}

static inline void merge_prototypes(FklFuncPrototypes *o,
                                    const FklFuncPrototypes *pts) {
    uint32_t o_count = o->count;
    o->count += pts->count;
    FklFuncPrototype *pa = (FklFuncPrototype *)fklZrealloc(
        o->pa, (o->count + 1) * sizeof(FklFuncPrototype));
    FKL_ASSERT(pa);
    o->pa = pa;
    uint32_t i = o_count + 1;
    memcpy(&pa[i], &pts->pa[1], sizeof(FklFuncPrototype) * pts->count);
    uint32_t end = o->count + 1;
    for (; i < end; i++) {
        FklFuncPrototype *cur = &pa[i];
        cur->refs = fklCopyMemory(cur->refs, cur->rcount * sizeof(*cur->refs));
    }
}

// static inline void load_printing_ignores(FklNastNodeVector *stack,
//                                          FklSymbolTable *st, FILE *fp) {
//     uint32_t count = 0;
//     fread(&count, sizeof(count), 1, fp);
//     for (; count > 0; count--) {
//         FklNastNode *node = load_nast_node_with_null_chr(st, fp);
//         fklNastNodeVectorPushBack2(stack, node);
//     }
// }
//
// static inline void load_printing_prods(FklProdPrintingVector *stack,
//                                        FklSymbolTable *st, FILE *fp) {
//     uint32_t count = 0;
//     fread(&count, sizeof(count), 1, fp);
//     for (; count > 0; count--) {
//         FklCodegenProdPrinting p = {0};
//         fread(&p.group_id, sizeof(p.group_id), 1, fp);
//         fread(&p.sid, sizeof(p.sid), 1, fp);
//         p.vec = load_nast_node_with_null_chr(st, fp);
//         fread(&p.add_extra, sizeof(p.add_extra), 1, fp);
//         fread(&p.type, sizeof(p.type), 1, fp);
//         if (p.type == FKL_CODEGEN_PROD_CUSTOM) {
//             uint32_t prototype_id = 0;
//             fread(&prototype_id, sizeof(prototype_id), 1, fp);
//             p.prototype_id = prototype_id;
//             p.bcl = fklLoadByteCodelnt(fp);
//         } else
//             p.forth = load_nast_node_with_null_chr(st, fp);
//         fklProdPrintingVectorPushBack(stack, &p);
//     }
// }

int fklLoadPreCompile(FklFuncPrototypes *info_pts,
                      FklFuncPrototypes *info_macro_pts,
                      FklCodegenLibVector *info_scriptLibStack,
                      FklCodegenLibVector *info_macroScriptLibStack,
                      FklSymbolTable *runtime_st, FklConstTable *runtime_kt,
                      FklCodegenOuterCtx *outer_ctx, const char *rp, FILE *fp,
                      char **errorStr) {
    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklConstTable *pkt = &outer_ctx->public_kt;

    int need_open = fp == NULL;
    if (need_open) {
        fp = fopen(rp, "rb");
        if (fp == NULL)
            return 1;
    }
    int err = 0;
    FklSymbolTable ost;
    FklConstTable okt;
    char *main_dir = fklGetDir(rp);
    main_dir = fklStrCat(main_dir, FKL_PATH_SEPARATOR_STR);

    FklCodegenLibVector scriptLibStack;
    FklCodegenLibVector macroScriptLibStack;

    fklInitSymbolTable(&ost);
    fklLoadSymbolTable(fp, &ost);

    fklInitConstTable(&okt);
    fklLoadConstTable(fp, &okt);

    FklFuncPrototypes *pts = NULL;
    FklFuncPrototypes *macro_pts = NULL;

    pts = fklLoadFuncPrototypes(fp);

    if (load_imported_lib_stack(&scriptLibStack, &ost, outer_ctx, main_dir, fp,
                                errorStr))
        goto error;

    macro_pts = fklLoadFuncPrototypes(fp);
    if (load_macro_lib_stack(&macroScriptLibStack, &ost, outer_ctx, main_dir,
                             fp, errorStr)) {
        while (!fklCodegenLibVectorIsEmpty(&macroScriptLibStack))
            fklUninitCodegenLib(
                fklCodegenLibVectorPopBack(&macroScriptLibStack));
        fklCodegenLibVectorUninit(&macroScriptLibStack);
    error:
        while (!fklCodegenLibVectorIsEmpty(&scriptLibStack))
            fklUninitCodegenLib(fklCodegenLibVectorPopBack(&scriptLibStack));
        err = 1;
        goto exit;
    }

    recompute_sid_for_prototypes(pts, &ost, runtime_st);
    recompute_sid_for_prototypes(macro_pts, &ost, pst);

    recompute_sid_for_double_st_lib_stack(&scriptLibStack, &ost, runtime_st,
                                          pst, &okt, runtime_kt, pkt,
                                          FKL_CODEGEN_SID_RECOMPUTE_NONE);
    recompute_sid_for_lib_stack(&macroScriptLibStack, &ost, pst, &okt, pkt,
                                FKL_CODEGEN_SID_RECOMPUTE_NONE);

    increase_prototype_and_lib_id(
        info_pts->count, info_macro_pts->count, info_scriptLibStack->size,
        info_macroScriptLibStack->size, &scriptLibStack, &macroScriptLibStack);

    merge_prototypes(info_pts, pts);
    merge_prototypes(info_macro_pts, macro_pts);

    fklInitPreLibReaderMacros(&scriptLibStack, pst, outer_ctx, info_macro_pts,
                              info_macroScriptLibStack);
    fklInitPreLibReaderMacros(&macroScriptLibStack, pst, outer_ctx,
                              info_macro_pts, info_macroScriptLibStack);

    for (uint32_t i = 0; i < scriptLibStack.size; i++)
        fklCodegenLibVectorPushBack(info_scriptLibStack,
                                    &scriptLibStack.base[i]);

    for (uint32_t i = 0; i < macroScriptLibStack.size; i++)
        fklCodegenLibVectorPushBack(info_macroScriptLibStack,
                                    &macroScriptLibStack.base[i]);

    fklCodegenLibVectorUninit(&macroScriptLibStack);
exit:
    if (need_open)
        fclose(fp);
    fklZfree(main_dir);
    fklDestroyFuncPrototypes(pts);
    if (macro_pts)
        fklDestroyFuncPrototypes(macro_pts);
    fklUninitSymbolTable(&ost);
    fklUninitConstTable(&okt);
    fklCodegenLibVectorUninit(&scriptLibStack);
    return err;
}
