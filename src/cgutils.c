#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_lw.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/regex.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include "codegen.h"

#include <inttypes.h>
#include <stdio.h>

static inline FklSymDefHashMapElm *get_def_by_id_in_scope(FklVMvalue *id,
        uint32_t scopeId,
        const FklCodegenEnvScope *scope) {
    FklSidScope key = { id, scopeId };
    return fklSymDefHashMapAt(&scope->defs, &key);
}

static void fklDestroyCodegenMacro(FklCodegenMacro *macro) {
    uninit_codegen_macro(macro);
    fklZfree(macro);
}

FklSymDefHashMapElm *fklFindSymbolDefByIdAndScope(FklVMvalue *id,
        uint32_t scope,
        const FklCodegenEnvScope *scopes) {
    for (; scope; scope = scopes[scope - 1].p) {
        FklSymDefHashMapElm *r =
                get_def_by_id_in_scope(id, scope, &scopes[scope - 1]);
        if (r)
            return r;
    }
    return NULL;
}

FklSymDefHashMapElm *fklGetCodegenDefByIdInScope(FklVMvalue *id,
        uint32_t scope,
        const FklCodegenEnvScope *scopes) {
    return get_def_by_id_in_scope(id, scope, &scopes[scope - 1]);
}

void fklPrintCodegenError(FklCodegenErrorState *error_state,
        const FklVMvalueCodegenInfo *info) {
    FklVMvalue *obj = error_state->place;
    FklBuiltinErrorType type = error_state->type;
    size_t line = error_state->line;
    FklVMvalue *fid = error_state->fid;
    const char *msg = error_state->msg;
    char *msg2 = error_state->msg2;

    memset(error_state, 0, sizeof(*error_state));

    static const char *builtInErrorType[] = {
#define X(A, B) B,
        FKL_BUILTIN_ERR_MAP
#undef X
    };

    if (type == FKL_ERR_DUMMY || type == FKL_ERR_SYMUNDEFINE)
        return;
    fklPrintRawCstr(builtInErrorType[type], "|", "|", '|', stderr);
    fputs(": ", stderr);
    if (msg || msg2) {
        if (msg)
            fputs(msg, stderr);
        if (msg2)
            fputs(msg2, stderr);

        if (obj) {
            fputc(' ', stderr);
            fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        }

        goto print_line;
    }

    switch (type) {
    case FKL_ERR_UNSERIALIZABLE:
        fputs("It's unserializable to bytecode file", stderr);
        break;
    case FKL_ERR_SYMUNDEFINE:
        break;
    case FKL_ERR_SYNTAXERROR:
        fputs("Invalid syntax ", stderr);
        if (obj != NULL)
            fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        break;
    case FKL_ERR_EXP_HAS_NO_VALUE:
        fputs("Expression ", stderr);
        if (obj != NULL)
            fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        fputs(" has no value", stderr);
        break;
    case FKL_ERR_INVALIDEXPR:
        fputs("Invalid expression", stderr);
        if (obj != NULL) {
            fputc(' ', stderr);
            fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        }
        break;
    case FKL_ERR_CIRCULARLOAD:
        fputs("Circular load file ", stderr);
        if (obj != NULL)
            fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        break;
    case FKL_ERR_INVALIDPATTERN:
        fputs("Invalid string match pattern ", stderr);
        if (obj != NULL)
            fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        break;
    case FKL_ERR_INVALID_MACRO_PATTERN:
        fputs("Invalid macro pattern ", stderr);
        if (obj != NULL)
            fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        break;
    case FKL_ERR_MACROEXPANDFAILED:
        fputs("Failed to expand macro in ", stderr);
        if (obj != NULL)
            fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        break;
    case FKL_ERR_LIBUNDEFINED:
        fputs("Library ", stderr);
        if (obj != NULL)
            fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        fputs(" undefined", stderr);
        break;
    case FKL_ERR_FILEFAILURE:
        fputs("Failed for file: ", stderr);
        fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        break;
    case FKL_ERR_IMPORTFAILED:
        fputs("Failed for importing module: ", stderr);
        fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        break;
    case FKL_ERR_UNEXPECTED_EOF:
        fputs("Unexpected eof", stderr);
        break;
    case FKL_ERR_IMPORT_MISSING:
        fputs("Missing import for ", stderr);
        fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
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
        fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        break;
    case FKL_ERR_ASSIGN_CONSTANT:
        fputs("attempt to assign constant ", stderr);
        fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        break;
    case FKL_ERR_REDEFINE_VARIABLE_AS_CONSTANT:
        fputs("attempt to redefine variable ", stderr);
        fklPrin1VMvalue(obj, stderr, &info->ctx->gc->gcvm);
        fputs(" as constant", stderr);
        break;
    default:
        fputs("Unknown compiling error", stderr);
        break;
    }
print_line:
    fputc('\n', stderr);
    if (obj) {
        if (fid) {
            fprintf(stderr,
                    "at line %" PRIu64 " of file %s",
                    line,
                    FKL_VM_SYM(fid)->str);
            fputc('\n', stderr);
        } else if (info->filename) {
            fprintf(stderr,
                    "at line %" PRIu64 " of file %s",
                    line,
                    info->filename);
            fputc('\n', stderr);
        } else
            fprintf(stderr, "at line %" PRIu64 "\n", line);
    } else {
        if (fid) {
            fprintf(stderr,
                    "at line %" PRIu64 " of file %s",
                    line,
                    FKL_VM_SYM(fid)->str);
            fputc('\n', stderr);
        } else if (info->filename) {
            fprintf(stderr,
                    "at line %" PRIu64 " of file %s",
                    line,
                    info->filename);
            fputc('\n', stderr);
        } else
            fprintf(stderr, "at line %" PRIu64 " of file ", line);
    }
    error_state->place = NULL;
    if (msg2)
        fklZfree(msg2);
}

#define INIT_SYMBOL_DEF(ID, SCOPE, IDX) { { ID, SCOPE }, IDX, IDX, 0, 0 }

FklSymDefHashMapElm *fklAddCodegenBuiltinRefBySid(FklVMvalue *id,
        FklVMvalueCodegenEnv *env) {
    FklSymDefHashMap *ht = &env->refs;
    uint32_t idx = ht->count;
    return fklSymDefHashMapInsert2(ht,
            (FklSidScope){ .id = id, .scope = env->pscope },
            (FklSymDef){ .idx = idx, .cidx = idx, .isLocal = 0, .isConst = 0 });
}

static inline void *has_outer_pdef_or_def(FklVMvalueCodegenEnv *cur,
        FklVMvalue *id,
        uint32_t scope,
        FklVMvalueCodegenEnv **targetEnv,
        int *is_pdef) {
    for (; cur; cur = cur->prev) {
        uint8_t *key = fklGetCodegenPreDefBySid(id, scope, cur);
        if (key) {
            *targetEnv = cur;
            *is_pdef = 1;
            return key;
        }
        FklSymDefHashMapElm *def =
                fklFindSymbolDefByIdAndScope(id, scope, cur->scopes);
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

static inline FklSymDefHashMapElm *add_ref_to_all_penv(FklVMvalue *id,
        FklVMvalueCodegenEnv *cur,
        FklVMvalueCodegenEnv *targetEnv,
        uint8_t isConst,
        FklSymDefHashMapElm **new_ref) {
    uint32_t idx = cur->refs.count;
    FklSymDefHashMapElm *cel = fklSymDefHashMapInsert2(&cur->refs,
            (FklSidScope){ .id = id, .scope = cur->pscope },
            (FklSymDef){ .idx = idx,
                .cidx = idx,
                .isConst = isConst,
                .isLocal = 0 });
    *new_ref = cel;
    FklSidScope key = { .id = id, .scope = cur->pscope };
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

static inline uint32_t get_child_env_prototype_id(FklVMvalueCodegenEnv *cur,
        FklVMvalueCodegenEnv *target) {
    FKL_ASSERT(cur != target);
    for (; cur->prev != target; cur = cur->prev)
        ;
    return cur->prototypeId;
}

static inline FklSymDefHashMapElm *has_outer_ref(FklVMvalueCodegenEnv *cur,
        FklVMvalue *id,
        FklVMvalueCodegenEnv **targetEnv) {
    FklSymDefHashMapElm *ref = NULL;
    FklSidScope key = { id, 0 };
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

static inline int is_ref_solved(FklSymDefHashMapElm *ref,
        FklVMvalueCodegenEnv *env) {
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

static inline void initUnReSymbolRef(FklUnReSymbolRef *r,
        FklVMvalue *id,
        uint32_t idx,
        uint32_t scope,
        uint32_t prototypeId,
        uint32_t assign,
        FklVMvalue *fid,
        uint64_t line) {
    r->id = id;
    r->idx = idx;
    r->scope = scope, r->prototypeId = prototypeId;
    r->assign = assign;
    r->fid = fid;
    r->line = line;
}

static inline void initPdefRef(FklPreDefRef *r,
        FklVMvalue *id,
        uint32_t scope,
        uint32_t prototypeId,
        uint32_t idx) {
    r->id = id;
    r->scope = scope;
    r->prototypeId = prototypeId;
    r->idx = idx;
}

FklSymDef *fklGetCodegenRefBySid(FklVMvalue *id, FklVMvalueCodegenEnv *env) {
    FklSymDefHashMap *ht = &env->refs;
    return fklSymDefHashMapGet2(ht, (FklSidScope){ id, env->pscope });
}

static inline FklUnReSymbolRef *has_resolvable_ref(FklVMvalue *id,
        uint32_t scope,
        const FklVMvalueCodegenEnv *env) {
    FklUnReSymbolRef *urefs = env->uref.base;
    uint32_t top = env->uref.size;
    for (uint32_t i = 0; i < top; i++) {
        FklUnReSymbolRef *cur = &urefs[i];
        if (cur->id == id && cur->scope == scope)
            return cur;
    }
    return NULL;
}

void fklAddCodegenPreDefBySid(FklVMvalue *id,
        uint32_t scope,
        uint8_t isConst,
        FklVMvalueCodegenEnv *env) {
    FklPredefHashMap *pdef = &env->pdef;
    FklSidScope key = { id, scope };
    fklPredefHashMapAdd(pdef, &key, &isConst);
}

uint8_t *fklGetCodegenPreDefBySid(FklVMvalue *id,
        uint32_t scope,
        FklVMvalueCodegenEnv *env) {
    FklPredefHashMap *pdef = &env->pdef;
    FklSidScope key = { id, scope };
    return fklPredefHashMapGet(pdef, &key);
}

void fklAddCodegenRefToPreDef(FklVMvalue *id,
        uint32_t scope,
        uint32_t prototypeId,
        uint32_t idx,
        FklVMvalueCodegenEnv *env) {
    initPdefRef(fklPreDefRefVectorPushBack(&env->ref_pdef, NULL),
            id,
            scope,
            prototypeId,
            idx);
}

void fklResolveCodegenPreDef(FklVMvalue *id,
        uint32_t scope,
        FklVMvalueCodegenEnv *env,
        FklFuncPrototypes *pts) {
    FklPreDefRefVector *ref_pdef = &env->ref_pdef;
    FklFuncPrototype *pa = pts->pa;
    FklPreDefRefVector ref_pdef1;
    uint32_t count = ref_pdef->size;
    fklPreDefRefVectorInit(&ref_pdef1, count);
    uint8_t pdef_isconst;
    FklSidScope key = { id, scope };
    fklPredefHashMapEarase(&env->pdef, &key, &pdef_isconst, NULL);
    FklSymDefHashMapElm *def =
            fklGetCodegenDefByIdInScope(id, scope, env->scopes);
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

void fklClearCodegenPreDef(FklVMvalueCodegenEnv *env) {
    fklPredefHashMapClear(&env->pdef);
}

FklSymDefHashMapElm *fklAddCodegenRefBySid(FklVMvalue *id,
        FklVMvalueCodegenEnv *env,
        FklVMvalue *fid,
        uint64_t line,
        uint32_t assign) {
    FklSymDefHashMap *refs = &env->refs;
    FklSymDefHashMapElm *el =
            fklSymDefHashMapAt2(refs, (FklSidScope){ id, env->pscope });
    if (el) {
        FklUnReSymbolRef *ref = has_resolvable_ref(id,
                env->pscope,
                env->prev ? env->prev : env);
        if (assign && ref && !ref->assign) {
            ref->assign = 1;
            ref->fid = fid;
            ref->line = line;
        }
        return el;
    } else {
        FklSymDefHashMapElm *ret = NULL;
        uint32_t idx = refs->count;
        FklVMvalueCodegenEnv *prev = env->prev;
        if (prev) {
            FklVMvalueCodegenEnv *targetEnv = NULL;
            int is_pdef = 0;
            void *targetDef = has_outer_pdef_or_def(prev,
                    id,
                    env->pscope,
                    &targetEnv,
                    &is_pdef);
            if (targetDef) {
                if (is_pdef) {
                    uint8_t *pdef = FKL_TYPE_CAST(uint8_t *, targetDef);
                    FklSymDefHashMapElm *cel = add_ref_to_all_penv(id,
                            env,
                            targetEnv,
                            *pdef,
                            &ret);
                    cel->v.isLocal = 1;
                    cel->v.cidx = FKL_VAR_REF_INVALID_CIDX;
                    fklAddCodegenRefToPreDef(id,
                            env->pscope,
                            get_child_env_prototype_id(env, targetEnv),
                            cel->v.idx,
                            targetEnv);
                } else {
                    FklSymDefHashMapElm *def =
                            FKL_TYPE_CAST(FklSymDefHashMapElm *, targetDef);
                    FklSymDefHashMapElm *cel = add_ref_to_all_penv(id,
                            env,
                            targetEnv,
                            def->v.isConst,
                            &ret);
                    cel->v.isLocal = 1;
                    cel->v.cidx = def->v.idx;
                    targetEnv->slotFlags[def->v.idx] = FKL_CODEGEN_ENV_SLOT_REF;
                }
            } else {
                FklSymDefHashMapElm *targetRef =
                        has_outer_ref(prev, id, &targetEnv);
                if (targetRef && is_ref_solved(targetRef, targetEnv->prev))
                    add_ref_to_all_penv(id,
                            env,
                            targetEnv->prev,
                            targetRef->v.isConst,
                            &ret);
                else {
                    ret = fklSymDefHashMapInsert2(refs,
                            (FklSidScope){ .id = id, .scope = env->pscope },
                            (FklSymDef){ .idx = idx, .cidx = idx });
                    ret->v.cidx = FKL_VAR_REF_INVALID_CIDX;

                    initUnReSymbolRef(
                            fklUnReSymbolRefVectorPushBack(&prev->uref, NULL),
                            id,
                            idx,
                            env->pscope,
                            env->prototypeId,
                            assign,
                            fid,
                            line);
                }
            }
        } else {
            ret = fklSymDefHashMapInsert2(refs,
                    (FklSidScope){ .id = id, .scope = 0 },
                    (FklSymDef){ .idx = idx, .cidx = idx });
            ret->v.cidx = FKL_VAR_REF_INVALID_CIDX;
            idx = ret->v.idx;
            initUnReSymbolRef(fklUnReSymbolRefVectorPushBack(&env->uref, NULL),
                    id,
                    idx,
                    0,
                    env->prototypeId,
                    assign,
                    fid,
                    line);
        }
        return ret;
    }
}

uint32_t fklAddCodegenRefBySidRetIndex(FklVMvalue *id,
        FklVMvalueCodegenEnv *env,
        FklVMvalue *fid,
        uint64_t line,
        uint32_t assign) {
    return fklAddCodegenRefBySid(id, env, fid, line, assign)->v.idx;
}

int fklIsSymbolDefined(FklVMvalue *id,
        uint32_t scope,
        FklVMvalueCodegenEnv *env) {
    return get_def_by_id_in_scope(id, scope, &env->scopes[scope - 1]) != NULL;
}

static inline uint32_t
get_next_empty(uint32_t empty, uint8_t *flags, uint32_t lcount) {
    for (; empty < lcount && flags[empty]; empty++)
        ;
    return empty;
}

FklSymDef *fklAddCodegenDefBySid(FklVMvalue *id,
        uint32_t scopeId,
        FklVMvalueCodegenEnv *env) {
    FklCodegenEnvScope *scope = &env->scopes[scopeId - 1];
    FklSymDefHashMap *defs = &scope->defs;
    FklSidScope key = { id, scopeId };
    FklSymDef *el = fklSymDefHashMapGet(defs, &key);
    if (!el) {
        uint32_t idx = scope->empty;
        el = fklSymDefHashMapAdd(defs, &key, NULL);
        if (idx < env->lcount && has_resolvable_ref(id, scopeId, env))
            idx = env->lcount;
        else
            scope->empty = get_next_empty(scope->empty + 1,
                    env->slotFlags,
                    env->lcount);
        el->idx = idx;
        uint32_t end = (idx + 1) - scope->start;
        if (scope->end < end)
            scope->end = end;
        if (idx >= env->lcount) {
            env->lcount = idx + 1;
            uint8_t *slotFlags = (uint8_t *)fklZrealloc(env->slotFlags,
                    env->lcount * sizeof(uint8_t));
            FKL_ASSERT(slotFlags);
            env->slotFlags = slotFlags;
        }
        env->slotFlags[idx] = FKL_CODEGEN_ENV_SLOT_OCC;
    }
    return el;
}

// struct RecomputeSidAndConstIdCtx {
//     const FklSymbolTable *origin_st;
//     FklSymbolTable *target_st;
//
//     const FklConstTable *origin_kt;
//     FklConstTable *target_kt;
// };
//
// #define IS_PUSH_SYM_OP(OP)
//     ((OP) >= FKL_OP_PUSH_SYM && (OP) <= FKL_OP_PUSH_SYM_X)
// #define IS_PUSH_F64_OP(OP)
//     ((OP) >= FKL_OP_PUSH_F64 && (OP) <= FKL_OP_PUSH_F64_X)
// #define IS_PUSH_I64F_OP(OP)
//     ((OP) >= FKL_OP_PUSH_I64F && (OP) <= FKL_OP_PUSH_I64F_X)
// #define IS_PUSH_I64B_OP(OP)
//     ((OP) >= FKL_OP_PUSH_I64B && (OP) <= FKL_OP_PUSH_I64B_X)
// #define IS_PUSH_STR_OP(OP)
//     ((OP) >= FKL_OP_PUSH_STR && (OP) <= FKL_OP_PUSH_STR_X)
// #define IS_PUSH_BVEC_OP(OP)
//     ((OP) >= FKL_OP_PUSH_BVEC && (OP) <= FKL_OP_PUSH_BVEC_X)
// #define IS_PUSH_BI_OP(OP) ((OP) >= FKL_OP_PUSH_BI && (OP) <=
// FKL_OP_PUSH_BI_X)
//
// static int recompute_sid_and_const_id_predicate(FklOpcode op) {
//     return IS_PUSH_SYM_OP(op) || IS_PUSH_I64F_OP(op) || IS_PUSH_I64B_OP(op)
//         || IS_PUSH_F64_OP(op) || IS_PUSH_STR_OP(op) || IS_PUSH_BVEC_OP(op)
//         || IS_PUSH_BI_OP(op);
// }
//
// static int recompute_sid_and_const_id_func(void *cctx,
//         FklOpcode *popcode,
//         FklOpcodeMode *pmode,
//         FklInstructionArg *ins_arg) {
//     struct RecomputeSidAndConstIdCtx *ctx = cctx;
//     FklOpcode op = *popcode;
//     *pmode = FKL_OP_MODE_IuB;
//     if (IS_PUSH_SYM_OP(op)) {
//         replace_sid(&ins_arg->ux, ctx->origin_st, ctx->target_st);
//         *popcode = FKL_OP_PUSH_SYM;
//     } else if (IS_PUSH_I64F_OP(op)) {
//         ins_arg->ux = fklAddI64Const(ctx->target_kt,
//                 fklGetI64ConstWithIdx(ctx->origin_kt, ins_arg->ux));
//         *popcode = FKL_OP_PUSH_I64F;
//     } else if (IS_PUSH_I64B_OP(op)) {
//         ins_arg->ux = fklAddI64Const(ctx->target_kt,
//                 fklGetI64ConstWithIdx(ctx->origin_kt, ins_arg->ux));
//         *popcode = FKL_OP_PUSH_I64B;
//     } else if (IS_PUSH_F64_OP(op)) {
//         ins_arg->ux = fklAddF64Const(ctx->target_kt,
//                 fklGetF64ConstWithIdx(ctx->origin_kt, ins_arg->ux));
//         *popcode = FKL_OP_PUSH_F64;
//     } else if (IS_PUSH_STR_OP(op)) {
//         ins_arg->ux = fklAddStrConst(ctx->target_kt,
//                 fklGetStrConstWithIdx(ctx->origin_kt, ins_arg->ux));
//         *popcode = FKL_OP_PUSH_STR;
//     } else if (IS_PUSH_BVEC_OP(op)) {
//         ins_arg->ux = fklAddBvecConst(ctx->target_kt,
//                 fklGetBvecConstWithIdx(ctx->origin_kt, ins_arg->ux));
//         *popcode = FKL_OP_PUSH_BVEC;
//     } else if (IS_PUSH_BI_OP(op)) {
//         ins_arg->ux = fklAddBigIntConst(ctx->target_kt,
//                 fklGetBiConstWithIdx(ctx->origin_kt, ins_arg->ux));
//         *popcode = FKL_OP_PUSH_BI;
//     }
//     return 0;
// }
//
// #undef IS_PUSH_SYM_OP
// #undef IS_PUSH_F64_OP
// #undef IS_PUSH_I64F_OP
// #undef IS_PUSH_I64B_OP
// #undef IS_PUSH_STR_OP
// #undef IS_PUSH_BVEC_OP
// #undef IS_PUSH_BI_OP
//
// void fklRecomputeSidAndConstIdForBcl(FklByteCodelnt *bcl,
//         const FklSymbolTable *origin_st,
//         FklSymbolTable *target_st,
//         const FklConstTable *origin_kt,
//         FklConstTable *target_kt) {
//     struct RecomputeSidAndConstIdCtx ctx = {
//         .origin_st = origin_st,
//         .target_st = target_st,
//         .origin_kt = origin_kt,
//         .target_kt = target_kt,
//     };
//     fklRecomputeInsImm(bcl,
//             &ctx,
//             recompute_sid_and_const_id_predicate,
//             recompute_sid_and_const_id_func);
//
//     FklLineNumberTableItem *lnt_start = bcl->l;
//     FklLineNumberTableItem *lnt_end = lnt_start + bcl->ls;
//     for (; lnt_start < lnt_end; lnt_start++)
//         replace_sid(&lnt_start->fid, origin_st, target_st);
// }
//
// void fklRecomputeSidForNastNode(FklNastNode *node,
//         const FklSymbolTable *origin_table,
//         FklSymbolTable *target_table,
//         FklCodegenRecomputeNastSidOption option) {
//     FklNastNodeVector pending;
//     fklNastNodeVectorInit(&pending, 16);
//     fklNastNodeVectorPushBack2(&pending, node);
//     while (!fklNastNodeVectorIsEmpty(&pending)) {
//         FklNastNode *top = *fklNastNodeVectorPopBack(&pending);
//         switch (top->type) {
//         case FKL_NAST_SLOT:
//             FKL_UNREACHABLE();
//             break;
//         case FKL_NAST_SYM:
//             replace_sid(&top->sym, origin_table, target_table);
//             if (option == FKL_CODEGEN_SID_RECOMPUTE_MARK_SYM_AS_RC_SYM)
//                 top->type = FKL_NAST_RC_SYM;
//             break;
//         case FKL_NAST_BOX:
//             fklNastNodeVectorPushBack2(&pending, top->box);
//             break;
//         case FKL_NAST_PAIR:
//             fklNastNodeVectorPushBack2(&pending, top->pair->car);
//             fklNastNodeVectorPushBack2(&pending, top->pair->cdr);
//             break;
//         case FKL_NAST_VECTOR:
//             for (size_t i = 0; i < top->vec->size; i++)
//                 fklNastNodeVectorPushBack2(&pending, top->vec->base[i]);
//             break;
//         case FKL_NAST_HASHTABLE:
//             for (size_t i = 0; i < top->hash->num; i++) {
//                 fklNastNodeVectorPushBack2(&pending,
//                 top->hash->items[i].car);
//                 fklNastNodeVectorPushBack2(&pending,
//                 top->hash->items[i].cdr);
//             }
//             break;
//         default:
//             break;
//         }
//     }
//
//     fklNastNodeVectorUninit(&pending);
// }
//
// static inline void recompute_sid_for_prototypes(FklFuncPrototypes *pts,
//         const FklSymbolTable *origin_table,
//         FklSymbolTable *target_table) {
//     uint32_t end = pts->count + 1;
//
//     for (uint32_t i = 1; i < end; i++) {
//         FklFuncPrototype *cur = &pts->pa[i];
//         replace_sid(&cur->fid, origin_table, target_table);
//         if (cur->sid)
//             replace_sid(&cur->sid, origin_table, target_table);
//
//         for (uint32_t i = 0; i < cur->rcount; i++) {
//             if (cur->refs[i].k.id) {
//                 replace_sid(FKL_TYPE_CAST(FklSid_t *, &cur->refs[i].k.id),
//                         origin_table,
//                         target_table);
//             }
//         }
//     }
// }
//
// static inline void recompute_sid_for_export_sid_index(
//         FklCgExportSidIdxHashMap *t,
//         const FklSymbolTable *origin_table,
//         FklSymbolTable *target_table) {
//     for (FklCgExportSidIdxHashMapNode *sid_idx_list = t->first; sid_idx_list;
//             sid_idx_list = sid_idx_list->next) {
//         replace_sid(FKL_TYPE_CAST(FklSid_t *, &sid_idx_list->k),
//                 origin_table,
//                 target_table);
//     }
// }
//
// static inline void recompute_sid_for_compiler_macros(FklCodegenMacro *m,
//         const FklSymbolTable *origin_st,
//         FklSymbolTable *target_st,
//         const FklConstTable *origin_kt,
//         FklConstTable *target_kt,
//         FklCodegenRecomputeNastSidOption option) {
//     for (; m; m = m->next) {
//         fklRecomputeSidForNastNode(m->origin_exp, origin_st, target_st,
//         option); fklRecomputeSidAndConstIdForBcl(m->bcl,
//                 origin_st,
//                 target_st,
//                 origin_kt,
//                 target_kt);
//     }
// }
//
// static inline void recompute_sid_for_replacements(FklReplacementHashMap *t,
//         const FklSymbolTable *origin_table,
//         FklSymbolTable *target_table,
//         FklCodegenRecomputeNastSidOption option) {
//     if (t == NULL)
//         return;
//     for (FklReplacementHashMapNode *rep_list = t->first; rep_list;
//             rep_list = rep_list->next) {
//         replace_sid(FKL_TYPE_CAST(FklSid_t *, &rep_list->k),
//                 origin_table,
//                 target_table);
//         fklRecomputeSidForNastNode(rep_list->v,
//                 origin_table,
//                 target_table,
//                 option);
//     }
// }
//
// static inline void recompute_sid_for_script_lib(FklCodegenLib *lib,
//         const FklSymbolTable *origin_st,
//         FklSymbolTable *target_st,
//         const FklConstTable *origin_kt,
//         FklConstTable *target_kt,
//         FklCodegenRecomputeNastSidOption option) {
//     fklRecomputeSidAndConstIdForBcl(lib->bcl,
//             origin_st,
//             target_st,
//             origin_kt,
//             target_kt);
//     recompute_sid_for_export_sid_index(&lib->exports, origin_st, target_st);
//     recompute_sid_for_compiler_macros(lib->head,
//             origin_st,
//             target_st,
//             origin_kt,
//             target_kt,
//             option);
//     recompute_sid_for_replacements(lib->replacements,
//             origin_st,
//             target_st,
//             option);
//     fklRecomputeSidForNamedProdGroups(&lib->named_prod_groups,
//             origin_st,
//             target_st,
//             origin_kt,
//             target_kt,
//             option);
// }
//
// static inline void recompute_sid_for_dll_lib(FklCodegenLib *lib,
//         const FklSymbolTable *origin_table,
//         FklSymbolTable *target_table) {
//     recompute_sid_for_export_sid_index(&lib->exports,
//             origin_table,
//             target_table);
// }
//
// static inline void recompute_sid_for_lib_stack(
//         FklCodegenLibVector *loaded_libraries,
//         const FklSymbolTable *origin_st,
//         FklSymbolTable *target_st,
//         const FklConstTable *origin_kt,
//         FklConstTable *target_kt,
//         FklCodegenRecomputeNastSidOption option) {
//     for (size_t i = 0; i < loaded_libraries->size; i++) {
//         FklCodegenLib *lib = &loaded_libraries->base[i];
//         switch (lib->type) {
//         case FKL_CODEGEN_LIB_SCRIPT:
//             recompute_sid_for_script_lib(lib,
//                     origin_st,
//                     target_st,
//                     origin_kt,
//                     target_kt,
//                     option);
//             break;
//         case FKL_CODEGEN_LIB_DLL:
//             recompute_sid_for_dll_lib(lib, origin_st, target_st);
//             break;
//         case FKL_CODEGEN_LIB_UNINIT:
//             FKL_UNREACHABLE();
//             break;
//         }
//     }
// }
//
// static inline void recompute_sid_for_double_st_script_lib(FklCodegenLib *lib,
//         const FklSymbolTable *origin_st,
//         FklSymbolTable *runtime_st,
//         FklSymbolTable *public_st,
//         const FklConstTable *origin_kt,
//         FklConstTable *runtime_kt,
//         FklConstTable *public_kt,
//         FklCodegenRecomputeNastSidOption option) {
//     fklRecomputeSidAndConstIdForBcl(lib->bcl,
//             origin_st,
//             runtime_st,
//             origin_kt,
//             runtime_kt);
//     recompute_sid_for_export_sid_index(&lib->exports, origin_st, public_st);
//     recompute_sid_for_compiler_macros(lib->head,
//             origin_st,
//             public_st,
//             origin_kt,
//             public_kt,
//             option);
//     recompute_sid_for_replacements(lib->replacements,
//             origin_st,
//             public_st,
//             option);
//     fklRecomputeSidForNamedProdGroups(&lib->named_prod_groups,
//             origin_st,
//             public_st,
//             origin_kt,
//             public_kt,
//             option);
// }
//
// static inline void recompute_sid_for_double_st_lib_stack(
//         FklCodegenLibVector *loaded_libraries,
//         const FklSymbolTable *origin_st,
//         FklSymbolTable *runtime_st,
//         FklSymbolTable *public_st,
//         const FklConstTable *origin_kt,
//         FklConstTable *runtime_kt,
//         FklConstTable *public_kt,
//         FklCodegenRecomputeNastSidOption option) {
//     for (size_t i = 0; i < loaded_libraries->size; i++) {
//         FklCodegenLib *lib = &loaded_libraries->base[i];
//         switch (lib->type) {
//         case FKL_CODEGEN_LIB_SCRIPT:
//             recompute_sid_for_double_st_script_lib(lib,
//                     origin_st,
//                     runtime_st,
//                     public_st,
//                     origin_kt,
//                     runtime_kt,
//                     public_kt,
//                     option);
//             break;
//         case FKL_CODEGEN_LIB_DLL:
//             recompute_sid_for_dll_lib(lib, origin_st, public_st);
//             break;
//         case FKL_CODEGEN_LIB_UNINIT:
//             FKL_UNREACHABLE();
//             break;
//         }
//     }
// }
//
// static inline void recompute_sid_for_sid_set(FklSidHashSet *ht,
//         const FklSymbolTable *ost,
//         FklSymbolTable *tst) {
//     for (FklSidHashSetNode *l = ht->first; l; l = l->next) {
//         FklSid_t *id = FKL_TYPE_CAST(FklSid_t *, &l->k);
//         replace_sid(id, ost, tst);
//         *FKL_TYPE_CAST(uintptr_t *, &l->hashv) =
//                 fklGraProdGroupHashMap__hashv(id);
//     }
//     fklSidHashSetRehash(ht);
// }
//
// static inline void recompute_sid_for_main_file(FklVMvalueCodegenInfo
// *codegen,
//         FklByteCodelnt *bcl,
//         const FklSymbolTable *origin_st,
//         FklSymbolTable *target_st,
//         const FklConstTable *origin_kt,
//         FklConstTable *target_kt,
//         FklCodegenRecomputeNastSidOption option) {
//     fklRecomputeSidAndConstIdForBcl(bcl,
//             origin_st,
//             target_st,
//             origin_kt,
//             target_kt);
//     recompute_sid_for_export_sid_index(&codegen->exports, origin_st,
//     target_st); recompute_sid_for_compiler_macros(codegen->export_macro,
//             origin_st,
//             target_st,
//             origin_kt,
//             target_kt,
//             option);
//     recompute_sid_for_replacements(codegen->export_replacement,
//             origin_st,
//             target_st,
//             option);
//     recompute_sid_for_sid_set(codegen->export_named_prod_groups,
//             origin_st,
//             target_st);
//     fklRecomputeSidForNamedProdGroups(codegen->named_prod_groups,
//             origin_st,
//             target_st,
//             origin_kt,
//             target_kt,
//             option);
// }
//
// void fklRecomputeSidForSingleTableInfo(FklVMvalueCodegenInfo *codegen,
//         FklByteCodelnt *bcl,
//         const FklSymbolTable *origin_st,
//         FklSymbolTable *target_st,
//         const FklConstTable *origin_kt,
//         FklConstTable *target_kt,
//         FklCodegenRecomputeNastSidOption option) {
//     FklCodegenCtx *ctx = codegen->ctx;
//     FklFuncPrototypes *pts = ctx->pts;
//     FklFuncPrototypes *macro_pts = ctx->macro_pts;
//     FklCodegenLibVector *libs = &ctx->libraries;
//     FklCodegenLibVector *macro_libs = &ctx->macro_libraries;
//
//     recompute_sid_for_prototypes(pts, origin_st, target_st);
//     recompute_sid_for_prototypes(macro_pts, origin_st, target_st);
//     recompute_sid_for_main_file(codegen,
//             bcl,
//             origin_st,
//             target_st,
//             origin_kt,
//             target_kt,
//             option);
//     recompute_sid_for_lib_stack(libs,
//             origin_st,
//             target_st,
//             origin_kt,
//             target_kt,
//             option);
//     recompute_sid_for_lib_stack(macro_libs,
//             origin_st,
//             target_st,
//             origin_kt,
//             target_kt,
//             option);
// }

#include <fakeLisp/parser.h>
#include <string.h>

typedef void (*FklResolveRefToDefCb)(const FklSymDefHashMapMutElm *ref,
        const FklSymDefHashMapElm *def,
        const FklUnReSymbolRef *uref,
        FklFuncPrototype *,
        void *args);

void fklResolveRef(FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklFuncPrototypes *cp,
        const FklResolveRefArgs *args) {

    int no_refs_to_builtins = args ? args->no_refs_to_builtins : 0;
    FklResolveRefToDefCb resolve_ref_to_def_cb =
            args ? args->resolve_ref_to_def_cb : NULL;
    FklVMvalueCodegenEnv *top_env = args ? args->top_env : NULL;

    FklUnReSymbolRefVector *urefs = &env->uref;
    FklFuncPrototype *pa = cp->pa;
    FklUnReSymbolRefVector urefs1;
    uint32_t count = urefs->size;

    fklUnReSymbolRefVectorInit(&urefs1, count);
    for (uint32_t i = 0; i < count; i++) {
        FklUnReSymbolRef *uref = &urefs->base[i];
        if (uref->scope < scope) {
            // 忽略来自父作用域的未解决引用
            fklUnReSymbolRefVectorPushBack(&urefs1, uref);
            continue;
        }

        FklFuncPrototype *cpt = &pa[uref->prototypeId];
        FklSymDefHashMapMutElm *ref = &cpt->refs[uref->idx];
        const FklSymDefHashMapElm *def = fklFindSymbolDefByIdAndScope(uref->id,
                uref->scope,
                env->scopes);

        if (def) {
            env->slotFlags[def->v.idx] = FKL_CODEGEN_ENV_SLOT_REF;
            ref->v.cidx = def->v.idx;
            ref->v.isLocal = 1;

            if (resolve_ref_to_def_cb) {
                resolve_ref_to_def_cb(ref,
                        def,
                        uref,
                        cpt,
                        args->resolve_ref_to_def_cb_args);
            }
        } else if (env->scopes[uref->scope - 1].p) {
            uref->scope = env->scopes[uref->scope - 1].p;
            fklUnReSymbolRefVectorPushBack(&urefs1, uref);
        } else if (env->prev != top_env) {
            ref->v.cidx = fklAddCodegenRefBySidRetIndex(uref->id,
                    env,
                    uref->fid,
                    uref->line,
                    uref->assign);
        } else {
            if (!no_refs_to_builtins) {
                fklAddCodegenBuiltinRefBySid(uref->id, env);
            }
            fklUnReSymbolRefVectorPushBack(&urefs1, uref);
        }
    }
    urefs->size = 0;
    while (!fklUnReSymbolRefVectorIsEmpty(&urefs1))
        fklUnReSymbolRefVectorPushBack(urefs,
                fklUnReSymbolRefVectorPopBackNonNull(&urefs1));
    fklUnReSymbolRefVectorUninit(&urefs1);
}

void fklUpdatePrototypeConst(FklFuncPrototypes *cp,
        const FklVMvalueCodegenEnv *env) {
    FklFuncPrototype *pt = &cp->pa[env->prototypeId];
    uint32_t count = env->konsts.next_id - 1;
    if (count == 0) {
        fklZfree(pt->konsts);
        pt->konsts = NULL;
    } else if (pt->konsts) {
        FklVMvalue **karray = (FklVMvalue **)fklZrealloc(pt->konsts,
                count * sizeof(FklVMvalue *));
        FKL_ASSERT(karray);
        pt->konsts = karray;
    } else {
        FklVMvalue **karray =
                (FklVMvalue **)fklZmalloc(count * sizeof(FklVMvalue *));
        FKL_ASSERT(karray);
        pt->konsts = karray;
    }

    for (const FklVMvalueIdHashMapNode *cur = env->konsts.ht.first; cur;
            cur = cur->next) {
        pt->konsts[cur->v - 1] = FKL_TYPE_CAST(FklVMvalue *, cur->k);
    }

    pt->konsts_count = count;
}

void fklUpdatePrototypeRef(FklFuncPrototypes *cp,
        const FklVMvalueCodegenEnv *env) {
    FKL_ASSERT(env->pdef.count == 0);
    FklFuncPrototype *pt = &cp->pa[env->prototypeId];
    const FklSymDefHashMap *eht = &env->refs;
    uint32_t count = eht->count;

    FklSymDefHashMapMutElm *refs;
    if (!count)
        refs = NULL;
    else {
        refs = (FklSymDefHashMapMutElm *)fklZrealloc(pt->refs,
                count * sizeof(FklSymDefHashMapMutElm));
        FKL_ASSERT(refs);
    }
    pt->refs = refs;
    pt->rcount = count;
    for (FklSymDefHashMapNode *list = eht->first; list; list = list->next) {

        FklVMvalue *sid = NULL;
        if (env->is_debugging || list->v.cidx == FKL_VAR_REF_INVALID_CIDX) {
            sid = list->k.id;
        }

        refs[list->v.idx] = (FklSymDefHashMapMutElm){
            .k = { .id = sid, .scope = list->k.scope },
            .v = { .idx = list->v.idx,
                .cidx = list->v.cidx,
                .isLocal = list->v.isLocal,
                .isConst = 0 },
        };
    }
}

void fklUpdatePrototype(FklFuncPrototypes *cp, FklVMvalueCodegenEnv *env) {
    FKL_ASSERT(env->pdef.count == 0);
    FklFuncPrototype *pts = &cp->pa[env->prototypeId];
    pts->lcount = env->lcount;
    fklResolveRef(env, 1, cp, NULL);
    fklUpdatePrototypeRef(cp, env);
    fklUpdatePrototypeConst(cp, env);
}

void fklPrintUndefinedRef(const FklVMvalueCodegenEnv *env) {
    const FklUnReSymbolRefVector *urefs = &env->uref;
    for (uint32_t i = urefs->size; i > 0; i--) {
        FklUnReSymbolRef *ref = &urefs->base[i - 1];
        fprintf(stderr, "warning: Symbol ");
        fklPrintRawSymbol(FKL_VM_SYM(ref->id), stderr);
        fprintf(stderr, " is undefined at line %" PRIu64, ref->line);
        if (ref->fid) {
            fputs(" of ", stderr);
            fklPrintString(FKL_VM_SYM(ref->fid), stderr);
        }
        fputc('\n', stderr);
    }
}

void fklInitFuncPrototypeWithEnv(FklFuncPrototype *cpt,
        FklVMvalueCodegenInfo *info,
        FklVMvalueCodegenEnv *env,
        FklVMvalue *sid,
        uint32_t line) {
    FKL_TODO();
    // cpt->lcount = env->lcount;
    // cpt->rcount = 0;
    // cpt->refs = NULL;
    // FKL_ASSERT(cpt == &info->pts->pa[env->prototypeId]);
    // fklUpdatePrototypeRef(info->pts, env, info->st, pst);
    // cpt->sid = sid;
    // cpt->fid = info->fid;
    // cpt->line = line;
}

void fklInitVMlibWithCodegenLib(const FklCodegenLib *clib,
        FklVMlib *vlib,
        FklVM *exe,
        const FklVMvalueProtos *pts) {
    FklVMvalue *val = FKL_VM_NIL;
    if (clib->type == FKL_CODEGEN_LIB_SCRIPT) {
        FklVMvalue *codeObj = fklCreateVMvalueCodeObj(exe, clib->bcl);
        FklVMvalue *proc = fklCreateVMvalueProc(exe,
                codeObj,
                clib->prototypeId,
                FKL_TYPE_CAST(FklVMvalueProtos *, pts));
        fklInitMainProcRefs(exe, proc);
        val = proc;
    } else
        val = fklCreateVMvalueStr2(exe,
                strlen(clib->rp) - strlen(FKL_DLL_FILE_TYPE),
                clib->rp);
    fklInitVMlib(vlib, val, clib->epc);
}

static inline void uninit_codegen_lib_info(FklCodegenLib *lib) {
    if (lib->exports.buckets)
        fklCgExportSidIdxHashMapUninit(&lib->exports);
    fklZfree(lib->rp);
}

void fklUninitCodegenLib(FklCodegenLib *lib) {
    if (lib == NULL)
        return;
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        if (lib->bcl != NULL) {
            fklDestroyByteCodelnt(lib->bcl);
            lib->bcl = NULL;
        }
        break;
    case FKL_CODEGEN_LIB_DLL:
        uv_dlclose(&lib->dll);
        break;
    case FKL_CODEGEN_LIB_UNINIT:
        return;
        break;
    }
    uninit_codegen_lib_info(lib);
    fklClearCodegenLibMacros(lib);
}

void fklInitCodegenDllLib(FklCodegenCtx *ctx,
        FklCodegenLib *lib,
        char *rp,
        uv_lib_t dll,
        FklCodegenDllLibInitExportFunc init) {
    memset(lib, 0, sizeof(*lib));
    lib->rp = rp;
    lib->type = FKL_CODEGEN_LIB_DLL;
    lib->dll = dll;
    lib->head = NULL;
    lib->replacements = NULL;
    lib->exports.buckets = NULL;
    lib->named_prod_groups.buckets = NULL;

    uint32_t num = 0;
    FklVMvalue **exports = init(ctx->gc, &num);
    FklCgExportSidIdxHashMap *exports_idx = &lib->exports;
    fklCgExportSidIdxHashMapInit(exports_idx);
    if (num) {
        for (uint32_t i = 0; i < num; i++) {
            fklCgExportSidIdxHashMapAdd(exports_idx,
                    &exports[i],
                    &(FklCodegenExportIdx){ .idx = i });
        }
    }
    if (exports)
        fklZfree(exports);

    return;
    FKL_TODO();
}

void fklInitCodegenScriptLib(FklCodegenLib *lib,
        FklVMvalueCodegenInfo *info,
        FklByteCodelnt *bcl,
        uint64_t epc,
        FklVMvalueCodegenEnv *env) {
    memset(lib, 0, sizeof(*lib));
    lib->type = FKL_CODEGEN_LIB_SCRIPT;
    lib->bcl = bcl;
    lib->epc = epc;
    lib->named_prod_groups.buckets = NULL;
    lib->exports.buckets = NULL;
    if (info) {
        lib->rp = info->realpath;

        lib->head = info->export_macro;
        lib->replacements = info->export_replacement;
        if (info->export_named_prod_groups
                && info->export_named_prod_groups->count) {
            fklGraProdGroupHashMapInit(&lib->named_prod_groups);
            for (FklVMvalueHashSetNode *sid_list =
                            info->export_named_prod_groups->first;
                    sid_list;
                    sid_list = sid_list->next) {
                FklVMvalue *id = FKL_TYPE_CAST(FklVMvalue *, sid_list->k);
                FklGrammerProdGroupItem *group =
                        fklGraProdGroupHashMapGet2(info->named_prod_groups, id);
                FKL_ASSERT(group);
                FklGrammerProdGroupItem *target_group =
                        add_production_group(&lib->named_prod_groups,
                                info->ctx->gc,
                                id);
                merge_group(target_group, group, NULL);

                for (const FklStrHashSetNode *cur = group->delimiters.first;
                        cur;
                        cur = cur->next) {
                    fklAddString(&target_group->delimiters, cur->k);
                }
            }
        }

        if (env) {
            FklCgExportSidIdxHashMap *exports_index = &lib->exports;
            fklCgExportSidIdxHashMapInit(exports_index);
            FklCgExportSidIdxHashMap *export_sid_set = &info->exports;
            lib->prototypeId = env->prototypeId;
            for (const FklCgExportSidIdxHashMapNode *sid_idx_list =
                            export_sid_set->first;
                    sid_idx_list;
                    sid_idx_list = sid_idx_list->next) {
                fklCgExportSidIdxHashMapPut(exports_index,
                        &sid_idx_list->k,
                        &sid_idx_list->v);
            }
        } else
            lib->prototypeId = 0;
    } else {
        lib->rp = NULL;
        lib->head = NULL;
        lib->replacements = NULL;
    }
}

static FklCodegenMacro *find_macro(FklVMvalue *exp,
        FklVMvalueCodegenMacroScope *macros,
        FklPmatchHashMap *pht) {
    if (!exp)
        return NULL;
    FklCodegenMacro *r = NULL;
    for (; !r && macros; macros = macros->prev) {
        for (FklCodegenMacro *cur = macros->head; cur; cur = cur->next) {
            if (pht->buckets == NULL)
                fklPmatchHashMapInit(pht);
            if (fklPatternMatch(cur->pattern, exp, pht)) {
                r = cur;
                break;
            }
            fklPmatchHashMapClear(pht);
        }
    }
    return r;
}

static void fklDestroyCodegenMacroList(FklCodegenMacro *cur) {
    while (cur) {
        FklCodegenMacro *t = cur;
        cur = cur->next;
        fklDestroyCodegenMacro(t);
    }
}

void fklClearCodegenLibMacros(FklCodegenLib *lib) {
    if (lib->head) {
        fklDestroyCodegenMacroList(lib->head);
        lib->head = NULL;
    }
    if (lib->replacements) {
        fklReplacementHashMapDestroy(lib->replacements);
        lib->replacements = NULL;
    }
    if (lib->named_prod_groups.buckets) {
        fklGraProdGroupHashMapUninit(&lib->named_prod_groups);
    }
}

FklCodegenMacro *fklCreateCodegenMacro(FklVMvalue *pattern,
        const FklByteCodelnt *bcl,
        FklCodegenMacro *next,
        uint32_t prototype_id) {
    FklCodegenMacro *r = (FklCodegenMacro *)fklZmalloc(sizeof(FklCodegenMacro));
    FKL_ASSERT(r);
    r->pattern = pattern;
    r->bcl = fklCopyByteCodelnt(bcl);
    r->next = next;
    r->prototype_id = prototype_id;
    return r;
}

static inline uint64_t get_curline(const FklVMvalueCodegenInfo *info,
        const FklVMvalue *v) {
    return info->curline;
    FKL_TODO();
}

#define CURLINE(V) get_curline(codegen, V)

FklVMvalue *fklTryExpandCodegenMacro(FklVMvalue *exp,
        FklVMvalueCodegenInfo *codegen,
        FklVMvalueCodegenMacroScope *macros,
        FklCodegenErrorState *errorState) {
    FklVMvalue *r = exp;
    FklPmatchHashMap ht = { .buckets = NULL };
    uint64_t curline = CURLINE(exp);
    for (FklCodegenMacro *macro = find_macro(r, macros, &ht);
            !errorState->type && macro;
            macro = find_macro(r, macros, &ht)) {
        fklPmatchHashMapAdd2(&ht, codegen->ctx->builtInPatternVar_orig, exp);
        FklVMvalue *retval = NULL;
        FklLineNumHashMap lineHash;
        fklLineNumHashMapInit(&lineHash);

        FklCodegenCtx *ctx = codegen->ctx;
        const char *cwd = ctx->cwd;
        fklChdir(codegen->dir);

        FklVM *exe = fklInitMacroExpandVM(ctx,
                macro->bcl,
                macro->prototype_id,
                &ht,
                &lineHash,
                &retval,
                CURLINE(r));
        FklVMgc *gc = ctx->gc;
        int e = fklRunVMidleLoop(exe);
        fklMoveThreadObjectsToGc(exe, gc);

        fklChdir(cwd);

        if (e) {
            errorState->type = FKL_ERR_MACROEXPANDFAILED;
            errorState->place = r;
            errorState->line = curline;
            fklDeleteCallChain(exe);
            r = NULL;
        } else {
            if (retval) {
                r = retval;
            } else {
                errorState->type = FKL_ERR_UNSERIALIZABLE;
                errorState->place = NULL;
                errorState->line = curline;
            }
        }
        fklLineNumHashMapUninit(&lineHash);
        fklDestroyAllVMs(exe);
    }
    if (ht.buckets)
        fklPmatchHashMapUninit(&ht);
    return r;
}

typedef struct MacroExpandCtx {
    FklVMvalue **retval;
    FklLineNumHashMap *lineHash;
    uint64_t curline;
} MacroExpandCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(MacroExpandCtx);

static int macro_expand_frame_step(void *data, FklVM *exe) {
    MacroExpandCtx *ctx = (MacroExpandCtx *)data;

    FklVMvalue *r = FKL_VM_GET_TOP_VALUE(exe);
    *(ctx->retval) = check_macro_expand_result(r);

    return 0;
}

static void macro_expand_frame_backtrace(void *data, FILE *fp, FklVMgc *gc) {
    fputs("at <macroexpand>\n", fp);
}

static const FklVMframeContextMethodTable MacroExpandMethodTable = {
    .step = macro_expand_frame_step,
    .finalizer = NULL,
    .print_backtrace = macro_expand_frame_backtrace,
};

static void push_macro_expand_frame(FklVM *exe,
        FklVMvalue **ptr,
        FklLineNumHashMap *lineHash,
        uint64_t curline) {
    FklVMframe *f = fklCreateOtherObjVMframe(exe, &MacroExpandMethodTable);
    MacroExpandCtx *ctx = (MacroExpandCtx *)f->data;
    ctx->retval = ptr;
    ctx->lineHash = lineHash;
    ctx->curline = curline;
    fklPushVMframe(f, exe);
}

static void init_macro_match_local_variable(FklVM *exe,
        FklVMframe *frame,
        FklPmatchHashMap *ht,
        FklLineNumHashMap *lineHash,
        FklFuncPrototypes *pts,
        uint32_t prototype_id) {
    FklFuncPrototype *mainPts = &pts->pa[prototype_id];
    FklVMCompoundFrameVarRef *lr = fklGetCompoundFrameLocRef(frame);
    FklVMproc *proc = FKL_VM_PROC(fklGetCompoundFrameProc(frame));
    uint32_t count = mainPts->lcount;
    uint32_t idx = 0;
    for (FklPmatchHashMapNode *list = ht->first; list; list = list->next) {
        FklVMvalue *v = list->v;
        FKL_VM_GET_ARG(exe, frame, idx) = v;
        idx++;
    }
    lr->lcount = count;
    lr->lref = NULL;
    lr->lrefl = NULL;
    proc->closure = lr->ref;
    proc->rcount = lr->rcount;
}

static inline void update_new_codegen_to_new_vm_lib(FklVM *vm,
        const FklCodegenLibVector *clibs,
        const FklVMvalueProtos *pts,
        FklVMvalueLibs *libs) {
    if (libs->count != clibs->size) {
        uint64_t old_count = libs->count;
        fklVMvalueLibsReserve(libs, clibs->size);
        for (size_t i = old_count; i < clibs->size; ++i) {
            const FklCodegenLib *cur = &clibs->base[i];
            fklInitVMlibWithCodegenLib(cur, &libs->libs[i + 1], vm, pts);
        }
    }
}

void fklUpdateVMlibsWithCodegenLibVector(FklVM *vm,
        FklVMvalueLibs *libs,
        const FklCodegenLibVector *clibs,
        const FklVMvalueProtos *pts) {
    update_new_codegen_to_new_vm_lib(vm, clibs, pts, libs);
}

FklVM *fklInitMacroExpandVM(FklCodegenCtx *ctx,
        FklByteCodelnt *bcl,
        uint32_t prototype_id,
        FklPmatchHashMap *ht,
        FklLineNumHashMap *lineHash,
        FklVMvalue **pr,
        uint64_t curline) {
    FklVMgc *gc = ctx->gc;
    FklVMvalueProtos *pts = ctx->macro_pts;

    FklVMvalueLibs *libs = ctx->macro_vm_libs;
    if (libs == NULL) {
        libs = fklCreateVMvalueLibs(&gc->gcvm);
        ctx->macro_vm_libs = libs;
    }

    FklVM *exe = fklCreateVM(NULL, gc, pts, libs);
    FklVMvalue *code_obj = fklCreateVMvalueCodeObj(exe, bcl);
    FklVMvalue *proc = fklCreateVMvalueProc(exe, code_obj, prototype_id, pts);

    push_macro_expand_frame(exe, pr, lineHash, curline);

    fklInitMainProcRefs(exe, proc);
    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, proc);
    fklCallObj(exe, proc);
    init_macro_match_local_variable(exe,
            exe->top_frame,
            ht,
            lineHash,
            &pts->p,
            prototype_id);

    update_new_codegen_to_new_vm_lib(exe, &ctx->macro_libraries, pts, libs);
    return exe;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(macro_scope_as_print, "macro-scope")

static void macro_scope_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(ms, struct FklCodegenMacroScope, ud);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ms->prev), gc);
    for (const FklCodegenMacro *cur = ms->head; cur; cur = cur->next) {
        fklVMgcToGray(cur->pattern, gc);
        fklVMgcMarkCodeObject(gc, cur->bcl);
    }
    for (const FklReplacementHashMapNode *cur = ms->replacements->first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->k, gc);
        fklVMgcToGray(cur->v, gc);
    }

    return;
    FKL_TODO();
}

static int macro_scope_finalizer(FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(ms, struct FklCodegenMacroScope, ud);
    for (FklCodegenMacro *cur = ms->head; cur;) {
        FklCodegenMacro *t = cur;
        cur = cur->next;
        fklDestroyCodegenMacro(t);
    }

    ms->head = NULL;
    if (ms->replacements) {
        fklReplacementHashMapDestroy(ms->replacements);
        ms->replacements = NULL;
    }
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable MacroScopeUserDataMetaTable = {
    .size = sizeof(struct FklCodegenMacroScope),
    .__as_princ = macro_scope_as_print,
    .__as_prin1 = macro_scope_as_print,
    .__atomic = macro_scope_atomic,
    .__finalizer = macro_scope_finalizer,
};

int fklIsVMvalueCodegenMacroScope(FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->t == &MacroScopeUserDataMetaTable;
}

FklVMvalueCodegenMacroScope *fklCreateVMvalueCodegenMacroScope(FklCodegenCtx *c,
        FklVMvalueCodegenMacroScope *prev) {
    FKL_ASSERT(
            prev == NULL || fklIsVMvalueCodegenMacroScope((FklVMvalue *)prev));
    FklVMvalueCodegenMacroScope *r =
            (FklVMvalueCodegenMacroScope *)fklCreateVMvalueUd(&c->gc->gcvm,
                    &MacroScopeUserDataMetaTable,
                    NULL);

    r->head = NULL;
    r->replacements = fklReplacementHashMapCreate();
    r->prev = prev;
    return r;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(env_as_print, "env");

static void env_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(e, struct FklCodegenEnv, ud);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->prev), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->macros), gc);

    for (size_t i = 0; i < e->uref.size; ++i) {
        fklVMgcToGray(e->uref.base[i].fid, gc);
        fklVMgcToGray(e->uref.base[i].id, gc);
    }

    for (const FklVMvalueIdHashMapNode *cur = e->konsts.ht.first; cur;
            cur = cur->next) {
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, cur->k), gc);
    }
}

static int env_finalizer(FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(cur, struct FklCodegenEnv, ud);
    uint32_t sc = cur->sc;
    FklCodegenEnvScope *scopes = cur->scopes;
    for (uint32_t i = 0; i < sc; i++)
        fklSymDefHashMapUninit(&scopes[i].defs);
    fklZfree(scopes);
    fklZfree(cur->slotFlags);
    cur->scopes = NULL;
    cur->slotFlags = NULL;

    fklSymDefHashMapUninit(&cur->refs);
    FklUnReSymbolRefVector *unref = &cur->uref;
    fklUnReSymbolRefVectorUninit(unref);

    fklPredefHashMapUninit(&cur->pdef);
    fklPreDefRefVectorUninit(&cur->ref_pdef);

    fklUninitValueTable(&cur->konsts);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable EnvUserDataMetaTable = {
    .size = sizeof(struct FklCodegenEnv),
    .__as_princ = env_as_print,
    .__as_prin1 = env_as_print,
    .__atomic = env_atomic,
    .__finalizer = env_finalizer,
};

int fklIsVMvalueCodegenEnv(FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->t == &EnvUserDataMetaTable;
}

FklVMvalueCodegenEnv *fklCreateVMvalueCodegenEnv(FklCodegenCtx *c,
        FklVMvalueCodegenEnv *prev,
        uint32_t pscope,
        FklVMvalueCodegenMacroScope *prev_ms) {
    FKL_ASSERT(
            (prev == NULL || fklIsVMvalueCodegenEnv((FklVMvalue *)prev))
            && (prev_ms == NULL
                    || fklIsVMvalueCodegenMacroScope((FklVMvalue *)prev_ms)));

    FklVMvalueCodegenEnv *r =
            (FklVMvalueCodegenEnv *)fklCreateVMvalueUd(&c->gc->gcvm,
                    &EnvUserDataMetaTable,
                    NULL);

    r->pscope = pscope;
    r->sc = 0;
    r->scopes = NULL;
    enter_new_scope(0, r);
    r->prototypeId = 1;
    r->prev = prev;
    r->lcount = 0;
    r->slotFlags = NULL;
    r->is_debugging = 0;
    fklSymDefHashMapInit(&r->refs);
    fklUnReSymbolRefVectorInit(&r->uref, 8);
    fklPredefHashMapInit(&r->pdef);
    fklPreDefRefVectorInit(&r->ref_pdef, 8);
    r->macros = fklCreateVMvalueCodegenMacroScope(c, prev_ms);
    fklInitValueTable(&r->konsts);
    return r;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(info_as_print, "info");

static void *custom_action(void *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line);

static void *simple_action(void *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line);

static void *replace_action(void *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line);

static void info_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(e, struct FklCodegenInfo, ud);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->prev), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->global_env), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->pts), gc);
    fklVMgcToGray(e->fid, gc);

    if (e->named_prod_groups == &e->self_named_prod_groups) {
        for (FklGraProdGroupHashMapNode *cur = e->named_prod_groups->first; cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            fklVMgcMarkGrammer(gc, &cur->v.g, NULL);
        }
    }
    if (e->unnamed_g == &e->self_unnamed_g) {
        fklVMgcMarkGrammer(gc, e->unnamed_g, NULL);
    }
}

static int info_finalizer(FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(i, struct FklCodegenInfo, ud);

    fklZfree(i->dir);
    if (i->filename)
        fklZfree(i->filename);
    if (i->realpath)
        fklZfree(i->realpath);

    fklCgExportSidIdxHashMapUninit(&i->exports);
    for (FklCodegenMacro *cur = i->export_macro; cur;) {
        FklCodegenMacro *t = cur;
        cur = cur->next;
        fklDestroyCodegenMacro(t);
    }
    if (i->export_named_prod_groups)
        fklVMvalueHashSetDestroy(i->export_named_prod_groups);
    if (i->export_replacement)
        fklReplacementHashMapDestroy(i->export_replacement);
    if (i->g == &i->self_g && *i->g) {
        FklGrammer *g = *i->g;
        fklUninitGrammer(g);
        fklUninitGrammer(i->unnamed_g);
        fklGraProdGroupHashMapUninit(i->named_prod_groups);
        fklZfree(g);
    }

    memset(i, 0, sizeof(struct FklCodegenInfo));
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable InfoUserDataMetaTable = {
    .size = sizeof(struct FklCodegenInfo),
    .__as_princ = info_as_print,
    .__as_prin1 = info_as_print,
    .__atomic = info_atomic,
    .__finalizer = info_finalizer,
};

int fklIsVMvalueCodegenInfo(FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->t == &InfoUserDataMetaTable;
}

FklVMvalueCodegenInfo *fklCreateVMvalueCodegenInfo(FklCodegenCtx *ctx,
        FklVMvalueCodegenInfo *prev,
        const char *filename,
        const FklCodegenInfoArgs *args) {
    int is_lib = args == NULL ? 0 : args->is_lib;
    int is_macro = args == NULL ? 0 : args->is_macro;
    int is_global = args == NULL ? 0 : args->is_global;

    FklCodegenInfoWorkCb work_cb = args ? args->work_cb
                                 : prev ? prev->work_cb
                                        : NULL;

    FklCodegenInfoEnvWorkCb env_work_cb = args ? args->env_work_cb
                                        : prev ? prev->env_work_cb
                                               : NULL;
    void *work_ctx = args ? args->work_ctx //
                   : prev ? prev->work_ctx
                          : NULL;

    FKL_ASSERT(prev == NULL || fklIsVMvalueCodegenInfo((FklVMvalue *)prev));

    FklVMvalueCodegenInfo *r =
            (FklVMvalueCodegenInfo *)fklCreateVMvalueUd(&ctx->gc->gcvm,
                    &InfoUserDataMetaTable,
                    NULL);

    FklCodegenLibVector *libs = args && args->libraries ? args->libraries
                              : is_macro                ? &ctx->macro_libraries
                              : prev                    ? prev->libraries
                                                        : &ctx->libraries;

    FklVMvalueProtos *pts = args && args->pts ? args->pts
                          : is_macro          ? ctx->macro_pts
                          : prev              ? prev->pts
                                              : ctx->pts;

    char *rp = filename ? fklRealpath(filename)
             : prev     ? fklZstrdup(prev->realpath)
                        : NULL;

    filename = filename ? filename       //
             : prev     ? prev->filename //
                        : NULL;

    if (filename != NULL) {
        r->dir = fklGetDir(rp);
        r->filename = fklRelpath(ctx->main_file_real_path_dir, rp);
        r->realpath = rp;
        r->fid = add_symbol_cstr(ctx, r->filename);
        // r->fid = fklVMaddSymbolCstr(st, r->filename);
    } else {
        r->dir = fklSysgetcwd();
        r->filename = NULL;
        r->realpath = NULL;
        r->fid = FKL_VM_NIL;
    }

    r->prev = prev;

    r->curline = prev ? prev->curline : 1;

    r->ctx = ctx;

    r->exports.buckets = NULL;
    r->is_lib = is_lib;
    r->is_macro = is_macro;

    r->export_macro = NULL;
    r->export_replacement = is_lib ? fklReplacementHashMapCreate() : NULL;
    r->export_named_prod_groups = is_lib ? fklVMvalueHashSetCreate() : NULL;
    r->exports.buckets = NULL;

    r->work_cb = work_cb;
    r->env_work_cb = env_work_cb;
    r->work_ctx = work_ctx;

    r->libraries = libs;
    r->pts = pts;

    if (is_lib)
        fklCgExportSidIdxHashMapInit(&r->exports);

    if (args->inherit_grammer && prev) {
        r->g = &prev->self_g;
        r->named_prod_groups = &prev->self_named_prod_groups;
        r->unnamed_g = &prev->self_unnamed_g;
    } else {
        r->g = &r->self_g;
        r->named_prod_groups = &r->self_named_prod_groups;
        r->unnamed_g = &r->self_unnamed_g;
    }

    if (prev && !is_macro) {
        r->global_env = prev->global_env;
    } else {
        r->global_env = fklCreateVMvalueCodegenEnv(ctx,
                NULL,
                0,
                is_macro ? args->macro_scope : NULL);
        fklInitGlobCodegenEnv(r->global_env, &ctx->gc->gcvm);
    }

    if (r->work_cb)
        r->work_cb(r, r->work_ctx);

    FklVMvalueCodegenEnv *main_env = NULL;
    if (is_global) {
        FklVMvalueCodegenMacroScope *macros = r->global_env->macros;
        main_env = fklCreateVMvalueCodegenEnv(r->ctx, r->global_env, 1, macros);
        create_and_insert_to_pool(r, 0, main_env, 0, 1);
        ctx->global_env = main_env;
        ctx->global_info = r;
    }

    return r;
}

void fklCreateFuncPrototypeAndInsertToPool(FklVMvalueCodegenInfo *info,
        uint32_t p,
        FklVMvalueCodegenEnv *env,
        FklVMvalue *sid,
        uint32_t line) {
    create_and_insert_to_pool(info, p, env, sid, line);
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(custom_action_ctx_ud_as_print,
        "custom-action-ctx")

static void custom_action_ctx_ud_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(c, struct FklCustomActionCtx, ud);
    if (c->bcl == NULL)
        return;
    fklVMgcMarkCodeObject(gc, c->bcl);
}

static int custom_action_ctx_ud_finalizer(FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(c, struct FklCustomActionCtx, ud);

    if (c->bcl)
        fklDestroyByteCodelnt(c->bcl);

    return FKL_VM_UD_FINALIZE_NOW;
}

static const FklVMudMetaTable CustomActionCtxUdMetaTable = {
    .size = sizeof(struct FklCustomActionCtx),
    .__atomic = custom_action_ctx_ud_atomic,
    .__as_prin1 = custom_action_ctx_ud_as_print,
    .__as_princ = custom_action_ctx_ud_as_print,
    .__finalizer = custom_action_ctx_ud_finalizer,
};

static void *custom_action(void *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMvalueCustomActionCtx *action_ctx = c;
    FklCodegenCtx *cg_ctx = action_ctx->c.ctx;
    FklVMvalue *nodes_vector = fklCreateVMvalueVec(&cg_ctx->gc->gcvm, line);
    // nodes_vector->vec = fklCreateNastVector(num);
    for (size_t i = 0; i < num; i++)
        FKL_VM_VEC(nodes_vector)->base[i] = nodes[i].ast;
    FklLineNumHashMap lineHash;
    fklLineNumHashMapInit(&lineHash);
    FklPmatchHashMap ht;
    fklPmatchHashMapInit(&ht);
    FklVMvalue *line_node = NULL;
    if (line > FKL_FIX_INT_MAX) {
        line_node = fklCreateVMvalueBigIntWithU64(&cg_ctx->gc->gcvm, line);
        // line_node->bigInt = fklCreateBigIntU(line);
    } else {
        line_node = FKL_MAKE_VM_FIX(line);
        // fklCreateNastNode(FKL_NAST_FIX, line);
        //       line_node->fix = line;
    }

    fklPmatchHashMapAdd2(&ht,
            add_symbol_cstr(cg_ctx, "$$"),
            // fklAddSymbolCstr("$$", action_ctx->pst),
            nodes_vector);
    fklPmatchHashMapAdd2(&ht,
            add_symbol_cstr(cg_ctx, "line"),
            // fklAddSymbolCstr("line", action_ctx->pst),
            line_node);

    FklVMvalue *r = NULL;
    const char *cwd = cg_ctx->cwd;
    const char *file_dir = cg_ctx->cur_file_dir;
    fklChdir(file_dir);

    FklVM *exe = fklInitMacroExpandVM(cg_ctx,
            action_ctx->c.bcl,
            action_ctx->c.prototype_id,
            &ht,
            &lineHash,
            &r,
            line);
    FklVMgc *gc = exe->gc;

    int e = fklRunVMidleLoop(exe);
    fklMoveThreadObjectsToGc(exe, gc);

    fklChdir(cwd);

    if (e)
        fklDeleteCallChain(exe);

    // fklDestroyNastNode(nodes_vector);
    // fklDestroyNastNode(line_node);

    fklPmatchHashMapUninit(&ht);
    fklLineNumHashMapUninit(&lineHash);
    fklDestroyAllVMs(exe);
    return r;
}

static inline FklVMvalue *create_custom_prod_action_ctx(FklCodegenCtx *cg_ctx,
        uint32_t prototypeId) {
    FklVMvalueCustomActionCtx *v =
            (FklVMvalueCustomActionCtx *)fklCreateVMvalueUd(&cg_ctx->gc->gcvm,
                    &CustomActionCtxUdMetaTable,
                    NULL);

    v->c.ctx = cg_ctx;
    v->c.prototype_id = prototypeId;

    return FKL_TYPE_CAST(FklVMvalue *, v);
}

FklGrammerProduction *fklCreateCustomActionProd(FklCodegenCtx *cg_ctx,
        struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        uint32_t prototypeId) {
    FklVMvalue *action_ctx = create_custom_prod_action_ctx(cg_ctx, prototypeId);

    FklGrammerProduction *prod = fklCreateProduction(group,
            sid,
            len,
            syms,
            NULL,
            custom_action,
            action_ctx,
            fklProdCtxDestroyDoNothing,
            fklProdCtxCopyerDoNothing);

    return prod;
}

int fklIsCustomActionProd(const FklGrammerProduction *p) {
    return p->func == custom_action;
}

static int simple_action_nth_check(FklVMvalue *rest[], size_t rest_len) {
    if (rest_len != 1 || !FKL_IS_FIX(rest[0]) || FKL_GET_FIX(rest[0]) < 0) {
        return 1;
    }

    return 0;
}

static void simple_action_nth_write(void *ctx,
        FklWriteCodePass pass,
        FklValueTable *vt,
        FILE *fp) {
    FKL_TODO();
    // if (pass == FKL_WRITE_CODE_PASS_FIRST)
    //     return;
    // struct SimpleActionNthCtx *c = ctx;
    // fwrite(&c->nth, sizeof(c->nth), 1, fp);
}

static void simple_action_nth_print(void *ctx, FILE *fp) {
    FKL_TODO();
    // struct SimpleActionNthCtx *c = ctx;
    // fprintf(fp, "%" PRIu64, c->nth);
}

static void *simple_action_nth_read(FklVM *vm, FILE *fp) {
    FKL_TODO();
    // struct SimpleActionNthCtx *c = (struct SimpleActionNthCtx *)fklZmalloc(
    //         sizeof(struct SimpleActionNthCtx));
    // FKL_ASSERT(c);
    // fread(&c->nth, sizeof(c->nth), 1, fp);

    // return c;
}

static inline uint64_t get_nth(FklVMvalue *vec) {
    FKL_ASSERT(FKL_IS_VECTOR(vec) && FKL_VM_VEC(vec)->size == 2);
    FKL_ASSERT(FKL_IS_FIX(FKL_VM_VEC(vec)->base[1]));
    return FKL_GET_FIX(FKL_VM_VEC(vec)->base[1]);
}

static void *simple_action_nth(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth(action_ctx);
    if (nth >= num)
        return NULL;
    return nodes[nth].ast;
}

static int simple_action_cons_check(FklVMvalue *rest[], size_t rest_len) {
    if (rest_len != 2                   //
            || !FKL_IS_FIX(rest[0])     //
            || FKL_GET_FIX(rest[0]) < 0 //
            || !FKL_IS_FIX(rest[1])     //
            || FKL_GET_FIX(rest[1]) < 0) {
        return 1;
    }
    return 0;
}

static void *simple_action_cons_copy(const void *c) {
    FKL_TODO();
    // struct SimpleActionConsCtx *ctx = (struct SimpleActionConsCtx
    // *)fklZmalloc(
    //         sizeof(struct SimpleActionConsCtx));
    // FKL_ASSERT(ctx);
    // *ctx = *(struct SimpleActionConsCtx *)c;
    // return ctx;
}

static inline void get_car_cdr(FklVMvalue *vec, uint64_t *car, uint64_t *cdr) {
    FKL_ASSERT(FKL_IS_VECTOR(vec) && FKL_VM_VEC(vec)->size == 3);
    FKL_ASSERT(FKL_IS_FIX(FKL_VM_VEC(vec)->base[1]));
    FKL_ASSERT(FKL_IS_FIX(FKL_VM_VEC(vec)->base[2]));
    *car = FKL_GET_FIX(FKL_VM_VEC(vec)->base[1]);
    *cdr = FKL_GET_FIX(FKL_VM_VEC(vec)->base[2]);
}

static void *simple_action_cons(void *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t car = 0;
    uint64_t cdr = 0;
    get_car_cdr(c, &car, &cdr);

    if (car >= num || cdr >= num)
        return NULL;
    FklVMparseCtx *ct = ctx;
    FklVMvalue *retval =
            fklCreateVMvaluePair(ct->exe, nodes[car].ast, nodes[cdr].ast);
    put_line_number(ct->ln, retval, line);
    return retval;

    FKL_TODO();
    // FklNastNode *retval = fklCreateNastNode(FKL_NAST_PAIR, line);
    // retval->pair = fklCreateNastPair();
    // retval->pair->car = fklMakeNastNodeRef(nodes[cc->car].ast);
    // retval->pair->cdr = fklMakeNastNodeRef(nodes[cc->cdr].ast);
    // return retval;
}

static void simple_action_cons_write(void *c,
        FklWriteCodePass pass,
        FklValueTable *vt,
        FILE *fp) {
    FKL_TODO();
    // if (pass == FKL_WRITE_CODE_PASS_FIRST)
    //     return;
    // struct SimpleActionConsCtx *ctx = c;
    // fwrite(&ctx->car, sizeof(ctx->car), 1, fp);
    // fwrite(&ctx->cdr, sizeof(ctx->cdr), 1, fp);
}

static void simple_action_cons_print(void *c, FILE *fp) {
    FKL_TODO();
    // struct SimpleActionConsCtx *ctx = c;
    // fprintf(fp, "%" PRIu64 ", %" PRIu64, ctx->car, ctx->cdr);
}

static void *simple_action_cons_read(FklVM *pst, FILE *fp) {
    FKL_TODO();
    // struct SimpleActionConsCtx *ctx = (struct SimpleActionConsCtx
    // *)fklZmalloc(
    //         sizeof(struct SimpleActionConsCtx));
    // FKL_ASSERT(ctx);
    // fread(&ctx->car, sizeof(ctx->car), 1, fp);
    // fread(&ctx->cdr, sizeof(ctx->cdr), 1, fp);
    // return ctx;
}

static void *simple_action_head(void *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMvalue *vec = c;
    FKL_ASSERT(FKL_IS_VECTOR(vec));

    for (size_t i = 2; i < FKL_VM_VEC(vec)->size; i++) {
        FklVMvalue *c = FKL_VM_VEC(vec)->base[i];
        FKL_ASSERT(FKL_IS_FIX(c) && FKL_GET_FIX(c) >= 0);
        uint64_t idx = FKL_GET_FIX(c);
        if (idx >= num)
            return NULL;
    }
    FklVMparseCtx *ct = ctx;
    FklVMvalue *head = FKL_VM_VEC(vec)->base[1];
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **pr = &r;
    for (size_t i = 2; i < FKL_VM_VEC(vec)->size; ++i) {
        FklVMvalue *c = FKL_VM_VEC(vec)->base[i];
        uint64_t idx = FKL_GET_FIX(c);

        const FklAnalysisSymbol *s = &nodes[idx];
        *pr = fklCreateVMvaluePairWithCar(ct->exe, s->ast);
        put_line_number(ct->ln, *pr, s->line);
        pr = &FKL_VM_CDR(*pr);
    }

    r = fklCreateVMvaluePair(ct->exe, head, r);
    put_line_number(ct->ln, r, line);
    return r;
    FKL_TODO();
    // FklNastNode *head = fklMakeNastNodeRef(cc->head);
    // FklNastNode **exps = (FklNastNode **)fklZmalloc(
    //         (1 + cc->idx_num) * sizeof(FklNastNode *));
    // FKL_ASSERT(exps);
    // exps[0] = head;
    // for (size_t i = 0; i < cc->idx_num; i++)
    //     exps[i + 1] = fklMakeNastNodeRef(nodes[cc->idx[i]].ast);
    // FklNastNode *retval = create_nast_list(exps, 1 + cc->idx_num, line);
    // fklZfree(exps);
    // return retval;
}

static size_t simple_action_head_get_size(FklVMvalue *rest[], size_t rest_len) {
    FKL_TODO();
    // return sizeof(struct SimpleActionHeadCtx)
    //      + ((rest_len - 1) * sizeof(uint64_t));
}

static int simple_action_head_check(FklVMvalue *rest[], size_t rest_len) {
    if (rest_len < 2 ||          //
            !FKL_IS_FIX(rest[1]) //
            || FKL_GET_FIX(rest[1]) < 0) {
        return 1;
    }
    for (size_t i = 1; i < rest_len; i++)
        if (!FKL_IS_FIX(rest[i]) || FKL_GET_FIX(rest[i]) < 0)
            return 1;

    return 0;
    FKL_TODO();
    // for (size_t i = 1; i < rest_len; i++)
    //     if (rest[i]->type != FKL_NAST_FIX || rest[i]->fix < 0)
    //         return NULL;
    // struct SimpleActionHeadCtx *ctx = (struct SimpleActionHeadCtx
    // *)fklZmalloc(
    //         sizeof(struct SimpleActionHeadCtx)
    //         + ((rest_len - 1) * sizeof(uint64_t)));
    // FKL_ASSERT(ctx);
    // ctx->head = fklMakeNastNodeRef(rest[0]);
    // ctx->idx_num = rest_len - 1;
    // for (size_t i = 1; i < rest_len; i++)
    //     ctx->idx[i - 1] = rest[i]->fix;
    // return ctx;
}

static void simple_action_head_atomic(void *c, FklVMgc *gc) {
    FKL_TODO();
    // struct SimpleActionHeadCtx *cc = c;
    // fklVMgcToGray(cc->head, gc);
}

static void simple_action_head_write(void *c,
        FklWriteCodePass pass,
        FklValueTable *vt,
        FILE *fp) {
    FKL_TODO();
    // struct SimpleActionHeadCtx *ctx = c;
    // if (pass == FKL_WRITE_CODE_PASS_FIRST) {
    //     fklTraverseSerializableValue(vt, ctx->head);
    //     return;
    // }
    //
    // fklWriteValueId(vt, ctx->head, fp);
    // fwrite(&ctx->idx_num, sizeof(ctx->idx_num), 1, fp);
    // fwrite(&ctx->idx[0], sizeof(ctx->idx[0]), ctx->idx_num, fp);
}

static void simple_action_head_print(void *c, FILE *fp) {
    FKL_TODO();
    // struct SimpleActionHeadCtx *ctx = c;
    // fklPrintNastNode(ctx->head, fp, pst);
    // for (size_t i = 0; i < ctx->idx_num; ++i) {
    //     fprintf(fp, ", %" PRIu64, ctx->idx[i]);
    // }
}

static void *simple_action_head_read(FklVM *pst, FILE *fp) {
    FKL_TODO();
    // FklNastNode *node = load_nast_node_with_null_chr(pst, fp);
    // uint64_t idx_num = 0;
    // fread(&idx_num, sizeof(idx_num), 1, fp);
    // struct SimpleActionHeadCtx *ctx = (struct SimpleActionHeadCtx
    // *)fklZmalloc(
    //         sizeof(struct SimpleActionHeadCtx) + (idx_num *
    //         sizeof(uint64_t)));
    // FKL_ASSERT(ctx);
    // ctx->head = node;
    // ctx->idx_num = idx_num;
    // fread(&ctx->idx[0], sizeof(ctx->idx[0]), ctx->idx_num, fp);
    //
    // return ctx;
}

static void *simple_action_head_copy(const void *c) {
    FKL_TODO();
    // struct SimpleActionHeadCtx *cc = (struct SimpleActionHeadCtx *)c;
    // size_t size = sizeof(struct SimpleActionHeadCtx)
    //             + (sizeof(uint64_t) * cc->idx_num);
    // struct SimpleActionHeadCtx *ctx =
    //         (struct SimpleActionHeadCtx *)fklZmalloc(size);
    // FKL_ASSERT(ctx);
    // memcpy(ctx, cc, size);
    // fklMakeNastNodeRef(ctx->head);
    // return ctx;
}

static void simple_action_head_destroy(void *cc) {
    struct SimpleActionHeadCtx *ctx = (struct SimpleActionHeadCtx *)cc;
    // fklDestroyNastNode(ctx->head);
    fklZfree(ctx);
}

static inline void *simple_action_box(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth(action_ctx);

    if (nth >= num)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *box = fklCreateVMvalueBox(c->exe, nodes[nth].ast);
    put_line_number(c->ln, box, line);
    return box;
    FKL_TODO();
    // FklNastNode *box = fklCreateNastNode(FKL_NAST_BOX, line);
    // box->box = fklMakeNastNodeRef(nodes[nth].ast);
    // return box;
}

static inline void *simple_action_vector(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth(action_ctx);

    if (nth >= num)
        return NULL;
    FklVMvalue *list = nodes[nth].ast;
    if (!fklIsList(list))
        return NULL;
    FklVMparseCtx *c = ctx;
    size_t len = fklVMlistLength(list);
    FklVMvalue *r = fklCreateVMvalueVec(c->exe, len);
    for (size_t i = 0; FKL_IS_PAIR(list); list = FKL_VM_CDR(list), ++i)
        FKL_VM_VEC(r)->base[i] = FKL_VM_CAR(list);
    return r;
    FKL_TODO();
    // FklNastNode *list = nodes[nth].ast;
    // FklNastNode *r = fklCreateNastNode(FKL_NAST_VECTOR, line);
    // size_t len = fklNastListLength(list);
    // FklNastVector *vec = fklCreateNastVector(len);
    // r->vec = vec;
    // size_t i = 0;
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++)
    //     vec->base[i] = fklMakeNastNodeRef(list->pair->car);
    // return r;
}

static inline void *simple_action_hasheq(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth(action_ctx);

    if (nth >= num)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *list = nodes[nth].ast;
    if (!is_pair_list(list))
        return NULL;
    return codegen_create_hash(c, FKL_HASH_EQ, list, line);
    FKL_TODO();
    // FklNastNode *list = nodes[nth].ast;
    // if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
    //     return NULL;
    // FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    // size_t len = fklNastListLength(list);
    // FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQ, len);
    // r->hash = ht;
    // size_t i = 0;
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
    //     ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
    //     ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    // }
    // return r;
}

static inline void *simple_action_hasheqv(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth(action_ctx);

    if (nth >= num)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *list = nodes[nth].ast;
    if (!is_pair_list(list))
        return NULL;
    return codegen_create_hash(c, FKL_HASH_EQV, list, line);
    FKL_TODO();
    // FklNastNode *list = nodes[nth].ast;
    // if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
    //     return NULL;
    // FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    // size_t len = fklNastListLength(list);
    // FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQV, len);
    // r->hash = ht;
    // size_t i = 0;
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
    //     ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
    //     ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    // }
    // return r;
}

static inline void *simple_action_hashequal(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth(action_ctx);

    if (nth >= num)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *list = nodes[nth].ast;
    if (!is_pair_list(list))
        return NULL;
    return codegen_create_hash(c, FKL_HASH_EQUAL, list, line);
    FKL_TODO();
    // FklNastNode *list = nodes[nth].ast;
    // if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
    //     return NULL;
    // FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    // size_t len = fklNastListLength(list);
    // FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQUAL, len);
    // r->hash = ht;
    // size_t i = 0;
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
    //     ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
    //     ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    // }
    // return r;
}

static inline void get_nth_start_end(FklVMvalue *v,
        FklString const **pstart,
        FklString const **pend,
        uint64_t *nth) {

    FKL_ASSERT(FKL_IS_VECTOR(v) && FKL_VM_VEC(v)->size > 1);

    FklVMvalue **rest = &FKL_VM_VEC(v)->base[1];
    size_t rest_len = FKL_VM_VEC(v)->size - 1;

    FKL_ASSERT(FKL_IS_FIX(rest[0]) && FKL_GET_FIX(rest[0]) >= 0);
    *nth = FKL_GET_FIX(rest[0]);

    if (rest_len == 3) {
        FKL_ASSERT(FKL_IS_STR(rest[1])              //
                   && FKL_VM_STR(rest[1])->size > 0 //
                   && FKL_IS_STR(rest[2])           //
                   && FKL_VM_STR(rest[2])->size > 0);

        *pstart = FKL_VM_STR(rest[1]);
        *pend = FKL_VM_STR(rest[2]);
    } else if (rest_len == 2) {
        FKL_ASSERT(FKL_IS_STR(rest[1]) && FKL_VM_STR(rest[1])->size > 0);
        *pstart = FKL_VM_STR(rest[1]);
        *pend = *pstart;
    } else {
        *pstart = NULL;
        *pend = NULL;
    }
}

static inline void *simple_action_bytevector(void *actx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = 0;
    const FklString *start_str = NULL;
    const FklString *end_str = NULL;
    get_nth_start_end(actx, &start_str, &end_str, &nth);

    if (nth >= num)
        return NULL;

    FklVMparseCtx *ct = ctx;
    FklVMvalue *node = nodes[nth].ast;
    if (!FKL_IS_STR(node))
        return NULL;
    if (start_str) {
        size_t start_size = start_str->size;
        size_t end_size = end_str->size;

        const FklString *str = FKL_VM_STR(node);
        const char *cstr = str->str;

        size_t size = 0;
        char *s = fklCastEscapeCharBuf(&cstr[start_size],
                str->size - end_size - start_size,
                &size);
        FklVMvalue *retval = fklCreateVMvalueBvec2(ct->exe, size, (uint8_t *)s);
        fklZfree(s);
        return retval;
    } else {
        FklVMvalue *v = fklCreateVMvalueBvec2(ct->exe,
                FKL_VM_STR(node)->size,
                (uint8_t *)FKL_VM_STR(node)->str);
        return v;
    }

    FKL_TODO();
    // FklNastNode *list = nodes[nth].ast;
    // FklNastNode *r = fklCreateNastNode(FKL_NAST_BYTEVECTOR, line);
    // size_t len = fklNastListLength(list);
    // FklBytevector *bv = fklCreateBytevector(len, NULL);
    // r->bvec = bv;
    // size_t i = 0;
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
    //     FklNastNode *cur = list->pair->car;
    //     if (cur->type == FKL_NAST_FIX)
    //         bv->ptr[i] = cur->fix > UINT8_MAX ? UINT8_MAX
    //                                           : (cur->fix < 0 ? 0 :
    //                                           cur->fix);
    //     else
    //         bv->ptr[i] = cur->bigInt->num < 0 ? 0 : UINT8_MAX;
    // }
    // return r;
}

static int simple_action_symbol_check(FklVMvalue *rest[], size_t rest_len) {
    if (rest_len < 1)
        return 1;

    if (!FKL_IS_FIX(rest[0]) //
            || FKL_GET_FIX(rest[0]) < 0) {
        return 1;
    }

    if (rest_len == 3) {
        if (!FKL_IS_STR(rest[1])                  //
                || FKL_VM_STR(rest[1])->size == 0 //
                || !FKL_IS_STR(rest[2])           //
                || FKL_VM_STR(rest[2])->size == 0) {
            return 1;
        }
    } else if (rest_len == 2) {
        if (!FKL_IS_STR(rest[1]) || FKL_VM_STR(rest[1])->size == 0) {
            return 1;
        }
    }

    return 0;
}

static void simple_action_symbol_atomic(void *c, FklVMgc *gc) {
    FKL_TODO();
    // struct SimpleActionSymbolCtx *cc = c;
    // fklVMgcToGray(cc->start, gc);
    // fklVMgcToGray(cc->end, gc);
}

static void simple_action_symbol_write(void *c,
        FklWriteCodePass pass,
        FklValueTable *vt,
        FILE *fp) {
    FKL_TODO();
    // struct SimpleActionSymbolCtx *ctx = c;
    // if (pass == FKL_WRITE_CODE_PASS_FIRST) {
    //     fklTraverseSerializableValue(vt, ctx->start);
    //     fklTraverseSerializableValue(vt, ctx->end);
    //     return;
    // }
    // fwrite(&ctx->nth, sizeof(ctx->nth), 1, fp);
    // fklWriteValueId(vt, ctx->start, fp);
    // fklWriteValueId(vt, ctx->end, fp);
    // if (ctx->start) {
    //     fwrite(ctx->start, sizeof(ctx->start->size) + ctx->start->size, 1,
    //     fp);
    // } else {
    //     uint64_t len = 0;
    //     fwrite(&len, sizeof(len), 1, fp);
    // }
    // if (ctx->end) {
    //     fwrite(ctx->end, sizeof(ctx->end->size) + ctx->end->size, 1, fp);
    // } else {
    //     uint64_t len = 0;
    //     fwrite(&len, sizeof(len), 1, fp);
    // }
}

static void simple_action_symbol_print(void *c, FILE *fp) {
    FKL_TODO();
    // struct SimpleActionSymbolCtx *ctx = c;
    // fprintf(fp, "%" PRIu64, ctx->nth);
    // if (ctx->start) {
    //     fputs(", ", fp);
    //     fklPrintRawString(FKL_VM_STR(ctx->start), fp);
    // }
    // if (ctx->end) {
    //     fputs(", ", fp);
    //     fklPrintRawString(FKL_VM_STR(ctx->end), fp);
    // }
}
static void *simple_action_symbol_read(FklVM *pst, FILE *fp) {
    FKL_TODO();
    // struct SimpleActionSymbolCtx *ctx =
    //         (struct SimpleActionSymbolCtx *)fklZmalloc(
    //                 sizeof(struct SimpleActionSymbolCtx));
    // FKL_ASSERT(ctx);
    // fread(&ctx->nth, sizeof(ctx->nth), 1, fp);
    //
    // {
    //     uint64_t len = 0;
    //     fread(&len, sizeof(len), 1, fp);
    //     if (len == 0)
    //         ctx->start = NULL;
    //     else {
    //         ctx->start = fklCreateString(len, NULL);
    //         fread(ctx->start->str, len, 1, fp);
    //     }
    // }
    // {
    //     uint64_t len = 0;
    //     fread(&len, sizeof(len), 1, fp);
    //     if (len == 0)
    //         ctx->end = NULL;
    //     else {
    //         ctx->end = fklCreateString(len, NULL);
    //         fread(ctx->end->str, len, 1, fp);
    //     }
    // }
    //
    // return ctx;
}

static void simple_action_symbol_destroy(void *cc) {
    FKL_TODO();
    // struct SimpleActionSymbolCtx *ctx = (struct SimpleActionSymbolCtx *)cc;
    // ctx->end = NULL;
    // ctx->start = NULL;
}

static void *simple_action_symbol(void *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = 0;
    const FklString *start_str = NULL;
    const FklString *end_str = NULL;
    get_nth_start_end(c, &start_str, &end_str, &nth);
    if (nth >= num)
        return NULL;
    FklVMvalue *node = nodes[nth].ast;
    if (!FKL_IS_STR(node))
        return NULL;
    FklVMvalue *sym = NULL;
    FklVMparseCtx *ct = ctx;
    if (start_str) {
        const char *start = start_str->str;
        size_t start_size = start_str->size;
        const char *end = end_str->str;
        size_t end_size = end_str->size;

        const FklString *str = FKL_VM_STR(node);
        const char *cstr = str->str;
        size_t cstr_size = str->size;

        FklStringBuffer buffer;
        fklInitStringBuffer(&buffer);
        const char *end_cstr = cstr + str->size;
        while (cstr < end_cstr) {
            if (fklCharBufMatch(start, start_size, cstr, cstr_size) >= 0) {
                cstr += start_size;
                cstr_size -= start_size;
                size_t len =
                        fklQuotedCharBufMatch(cstr, cstr_size, end, end_size);
                if (!len)
                    return 0;
                size_t size = 0;
                char *s = fklCastEscapeCharBuf(cstr, len - end_size, &size);
                fklStringBufferBincpy(&buffer, s, size);
                fklZfree(s);
                cstr += len;
                cstr_size -= len;
                continue;
            }
            size_t len = 0;
            for (; (cstr + len) < end_cstr; len++)
                if (fklCharBufMatch(start,
                            start_size,
                            cstr + len,
                            cstr_size - len)
                        >= 0)
                    break;
            fklStringBufferBincpy(&buffer, cstr, len);
            cstr += len;
            cstr_size -= len;
        }
        sym = fklVMaddSymbolCharBuf(ct->exe, buffer.buf, buffer.index);
        fklUninitStringBuffer(&buffer);
    } else {
        sym = fklVMaddSymbol(ct->exe, FKL_VM_STR(node));
    }
    return sym;
    FKL_TODO();
    // FklNastNode *node = nodes[action_ctx->nth].ast;
    // if (node->type != FKL_NAST_STR)
    //     return NULL;
    // FklNastNode *sym = NULL;
    // if (action_ctx->start) {
    //     const char *start = action_ctx->start->str;
    //     size_t start_size = action_ctx->start->size;
    //     const char *end = action_ctx->end->str;
    //     size_t end_size = action_ctx->end->size;
    //
    //     const FklString *str = node->str;
    //     const char *cstr = str->str;
    //     size_t cstr_size = str->size;
    //
    //     FklStringBuffer buffer;
    //     fklInitStringBuffer(&buffer);
    //     const char *end_cstr = cstr + str->size;
    //     while (cstr < end_cstr) {
    //         if (fklCharBufMatch(start, start_size, cstr, cstr_size) >= 0) {
    //             cstr += start_size;
    //             cstr_size -= start_size;
    //             size_t len =
    //                     fklQuotedCharBufMatch(cstr, cstr_size, end,
    //                     end_size);
    //             if (!len)
    //                 return 0;
    //             size_t size = 0;
    //             char *s = fklCastEscapeCharBuf(cstr, len - end_size, &size);
    //             fklStringBufferBincpy(&buffer, s, size);
    //             fklZfree(s);
    //             cstr += len;
    //             cstr_size -= len;
    //             continue;
    //         }
    //         size_t len = 0;
    //         for (; (cstr + len) < end_cstr; len++)
    //             if (fklCharBufMatch(start,
    //                         start_size,
    //                         cstr + len,
    //                         cstr_size - len)
    //                     >= 0)
    //                 break;
    //         fklStringBufferBincpy(&buffer, cstr, len);
    //         cstr += len;
    //         cstr_size -= len;
    //     }
    //     FklSid_t id = fklAddSymbolCharBuf(buffer.buf, buffer.index, ctx);
    //     sym = fklCreateNastNode(FKL_NAST_SYM, node->curline);
    //     sym->sym = id;
    //     fklUninitStringBuffer(&buffer);
    // } else {
    //     FklNastNode *sym = fklCreateNastNode(FKL_NAST_SYM, node->curline);
    //     sym->sym = fklAddSymbol(node->str, ctx);
    // }
    // return sym;
}

static inline void *simple_action_string(void *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = 0;
    const FklString *start_str = NULL;
    const FklString *end_str = NULL;
    get_nth_start_end(c, &start_str, &end_str, &nth);
    if (nth >= num)
        return NULL;
    FklVMparseCtx *ct = ctx;
    FklVMvalue *node = nodes[nth].ast;
    if (!FKL_IS_STR(node))
        return NULL;
    if (start_str) {
        size_t start_size = start_str->size;
        size_t end_size = end_str->size;

        const FklString *str = FKL_VM_STR(node);
        const char *cstr = str->str;

        size_t size = 0;
        char *s = fklCastEscapeCharBuf(&cstr[start_size],
                str->size - end_size - start_size,
                &size);
        FklVMvalue *retval = fklCreateVMvalueStr2(ct->exe, size, s);
        fklZfree(s);
        return retval;
    } else
        return node;
    FKL_TODO();
    // FklNastNode *node = nodes[action_ctx->nth].ast;
    // if (node->type != FKL_NAST_STR)
    //     return NULL;
    // if (action_ctx->start) {
    //     size_t start_size = action_ctx->start->size;
    //     size_t end_size = action_ctx->end->size;
    //
    //     const FklString *str = node->str;
    //     const char *cstr = str->str;
    //
    //     size_t size = 0;
    //     char *s = fklCastEscapeCharBuf(&cstr[start_size],
    //             str->size - end_size - start_size,
    //             &size);
    //     FklNastNode *retval = fklCreateNastNode(FKL_NAST_STR, node->curline);
    //     retval->str = fklCreateString(size, s);
    //     fklZfree(s);
    //     return retval;
    // } else
    //     return fklMakeNastNodeRef(node);
}

static struct FklSimpleProdAction
        CodegenProdCreatorActions[FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM] = {
            {
                "nth",
                .func = simple_action_nth,
                .check = simple_action_nth_check,
            },
            {
                "cons",
                .func = simple_action_cons,
                .check = simple_action_cons_check,
            },
            {
                "head",
                .func = simple_action_head,
                .check = simple_action_head_check,
            },
            {
                "symbol",
                .func = simple_action_symbol,
                .check = simple_action_symbol_check,
            },
            {
                "string",
                .func = simple_action_string,
                .check = simple_action_symbol_check,
            },
            {
                "box",
                .func = simple_action_box,
                .check = simple_action_nth_check,
            },
            {
                "vector",
                .func = simple_action_vector,
                .check = simple_action_nth_check,
            },
            {
                "hasheq",
                .func = simple_action_hasheq,
                .check = simple_action_nth_check,
            },
            {
                "hasheqv",
                .func = simple_action_hasheqv,
                .check = simple_action_nth_check,
            },
            {
                "hashequal",
                .func = simple_action_hashequal,
                .check = simple_action_nth_check,
            },
            {
                "bytes",
                .func = simple_action_bytevector,
                .check = simple_action_symbol_check,
            },
        };

static void *simple_action(void *actx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMvalueSimpleActionCtx *cc = actx;
    return cc->c.mt->func((void *)cc->c.vec, ctx, nodes, num, line);
}

const FklSimpleProdAction *fklGetSimpleProdActions(void) {
    return CodegenProdCreatorActions;
}

static inline void init_simple_prod_action_list(FklCodegenCtx *ctx) {
    FklVMvalue **const simple_prod_action_id = ctx->simple_prod_action_id;
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++)
        simple_prod_action_id[i] =
                add_symbol_cstr(ctx, CodegenProdCreatorActions[i].name);
    // fklAddSymbolCstr(CodegenProdCreatorActions[i].name, pst);
}

static void *replace_action(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMvalue *v = action_ctx;
    return v;
}

FklGrammerProduction *fklCreateReplaceActionProd(struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        FklVMvalue *ast) {
    FklGrammerProduction *prod = fklCreateProduction(group,
            sid,
            len,
            syms,
            NULL,
            replace_action,
            ast,
            fklProdCtxDestroyDoNothing,
            fklProdCtxCopyerDoNothing);

    return prod;
}

int fklIsReplaceActionProd(const FklGrammerProduction *p) {
    return p->func == replace_action;
}

static void *builtin_prod_action_nil(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    // return fklCreateNastNode(FKL_NAST_NIL, line);

    return FKL_VM_NIL;
}

static void *builtin_prod_action_first(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 1)
        return NULL;
    // return fklMakeNastNodeRef(nodes[0].ast);
    return nodes[0].ast;
}

static void *builtin_prod_action_symbol(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 1)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *node = nodes[0].ast;
    if (!FKL_IS_STR(node))
        return NULL;
    return fklVMaddSymbol(c->exe, FKL_VM_STR(node));
    FKL_TODO();
    // FklNastNode *node = nodes[0].ast;
    // if (node->type != FKL_NAST_STR)
    //     return NULL;
    // FklNastNode *sym = fklCreateNastNode(FKL_NAST_SYM, node->curline);
    // sym->sym = fklAddSymbol(node->str, ctx);
    // return sym;
}

static void *builtin_prod_action_second(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    // return fklMakeNastNodeRef(nodes[1].ast);

    return nodes[1].ast;
}

static void *builtin_prod_action_third(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 3)
        return NULL;
    // return fklMakeNastNodeRef(nodes[2].ast);

    return nodes[2].ast;
}

static inline void *builtin_prod_action_pair(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 3)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *car = nodes[0].ast;
    FklVMvalue *cdr = nodes[2].ast;
    FklVMvalue *pair = fklCreateVMvaluePair(c->exe, car, cdr);
    put_line_number(c->ln, pair, line);
    return pair;
    FKL_TODO();
    // FklNastNode *car = nodes[0].ast;
    // FklNastNode *cdr = nodes[2].ast;
    // FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    // pair->pair = fklCreateNastPair();
    // pair->pair->car = fklMakeNastNodeRef(car);
    // pair->pair->cdr = fklMakeNastNodeRef(cdr);
    // return pair;
}

static inline void *builtin_prod_action_cons(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMparseCtx *c = ctx;
    if (num == 1) {
        FklVMvalue *car = nodes[0].ast;
        FklVMvalue *pair = fklCreateVMvaluePairWithCar(c->exe, car);
        put_line_number(c->ln, pair, line);
        return pair;
        FKL_TODO();
        // FklNastNode *car = nodes[0].ast;
        // FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
        // pair->pair = fklCreateNastPair();
        // pair->pair->car = fklMakeNastNodeRef(car);
        // pair->pair->cdr = fklCreateNastNode(FKL_NAST_NIL, line);
        // return pair;
    } else if (num == 2) {
        FklVMvalue *car = nodes[0].ast;
        FklVMvalue *cdr = nodes[1].ast;
        FklVMvalue *pair = fklCreateVMvaluePair(c->exe, car, cdr);
        put_line_number(c->ln, pair, line);
        return pair;
        FKL_TODO();
        // FklNastNode *car = nodes[0].ast;
        // FklNastNode *cdr = nodes[1].ast;
        // FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
        // pair->pair = fklCreateNastPair();
        // pair->pair->car = fklMakeNastNodeRef(car);
        // pair->pair->cdr = fklMakeNastNodeRef(cdr);
        // return pair;
    } else
        return NULL;
}

static inline void *builtin_prod_action_box(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *box = fklCreateVMvalueBox(c->exe, nodes[1].ast);
    put_line_number(c->ln, box, line);
    return box;
    FKL_TODO();
    // FklNastNode *box = fklCreateNastNode(FKL_NAST_BOX, line);
    // box->box = fklMakeNastNodeRef(nodes[1].ast);
    // return box;
}

static inline void *builtin_prod_action_vector(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMvalue *list = nodes[1].ast;
    if (!fklIsList(list))
        return NULL;
    FklVMparseCtx *c = ctx;
    size_t len = fklVMlistLength(list);
    FklVMvalue *r = fklCreateVMvalueVec(c->exe, len);
    for (size_t i = 0; FKL_IS_PAIR(list); list = FKL_VM_CDR(list), ++i)
        FKL_VM_VEC(r)->base[i] = FKL_VM_CAR(list);
    return r;
    FKL_TODO();
    // FklNastNode *list = nodes[1].ast;
    // FklNastNode *r = fklCreateNastNode(FKL_NAST_VECTOR, line);
    // size_t len = fklNastListLength(list);
    // FklNastVector *vec = fklCreateNastVector(len);
    // r->vec = vec;
    // size_t i = 0;
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++)
    //     vec->base[i] = fklMakeNastNodeRef(list->pair->car);
    // return r;
}

static inline FklVMvalue *add_header(FklVMparseCtx *c,
        const FklAnalysisSymbol nodes[],
        const char *header_str,
        size_t line) {
    FklVMvalue *head = fklVMaddSymbolCstr(c->exe, header_str);
    FklVMvalue *s_exp = nodes[1].ast;
    ListElm s_exps[] = {
        { .v = head, .line = nodes[0].line },
        { .v = s_exp, .line = nodes[1].line },
    };
    return create_nast_list(s_exps, 2, line, c->exe, c->ln);
}

static inline void *builtin_prod_action_quote(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMparseCtx *c = ctx;
    return add_header(c, nodes, "quote", line);
    FKL_TODO();
    // FklSid_t id = fklAddSymbolCstr("quote", ctx);
    // FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    // FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    // head->sym = id;
    // FklNastNode *s_exps[] = { head, s_exp };
    // return create_nast_list(s_exps, 2, line);
}

static inline void *builtin_prod_action_unquote(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMparseCtx *c = ctx;
    return add_header(c, nodes, "unquote", line);
    FKL_TODO();
    // FklSid_t id = fklAddSymbolCstr("unquote", ctx);
    // FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    // FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    // head->sym = id;
    // FklNastNode *s_exps[] = { head, s_exp };
    // return create_nast_list(s_exps, 2, line);
}

static inline void *builtin_prod_action_qsquote(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMparseCtx *c = ctx;
    return add_header(c, nodes, "qsquote", line);
    FKL_TODO();
    // FklSid_t id = fklAddSymbolCstr("qsquote", ctx);
    // FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    // FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    // head->sym = id;
    // FklNastNode *s_exps[] = { head, s_exp };
    // return create_nast_list(s_exps, 2, line);
}

static inline void *builtin_prod_action_unqtesp(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMparseCtx *c = ctx;
    return add_header(c, nodes, "unqtesp", line);
    FKL_TODO();
    // FklSid_t id = fklAddSymbolCstr("unqtesp", tx);
    // FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    // FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    // head->sym = id;
    // FklNastNode *s_exps[] = { head, s_exp };
    // return create_nast_list(s_exps, 2, line);
}

static inline void *builtin_prod_action_hasheq(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMvalue *list = nodes[1].ast;
    if (!is_pair_list(list))
        return NULL;
    FklVMparseCtx *c = ctx;
    return codegen_create_hash(c, FKL_HASH_EQ, list, line);
    FKL_TODO();
    // FklNastNode *list = nodes[1].ast;
    // if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
    //     return NULL;
    // FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    // size_t len = fklNastListLength(list);
    // FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQ, len);
    // r->hash = ht;
    // size_t i = 0;
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
    //     ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
    //     ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    // }
    // return r;
}

static inline void *builtin_prod_action_hasheqv(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMvalue *list = nodes[1].ast;
    if (!is_pair_list(list))
        return NULL;
    FklVMparseCtx *c = ctx;
    return codegen_create_hash(c, FKL_HASH_EQV, list, line);
    FKL_TODO();
    // FklNastNode *list = nodes[1].ast;
    // if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
    //     return NULL;
    // FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    // size_t len = fklNastListLength(list);
    // FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQV, len);
    // r->hash = ht;
    // size_t i = 0;
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
    //     ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
    //     ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    // }
    // return r;
}

static inline void *builtin_prod_action_hashequal(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return 0;
    FklVMvalue *list = nodes[1].ast;
    if (!is_pair_list(list))
        return NULL;
    FklVMparseCtx *c = ctx;
    return codegen_create_hash(c, FKL_HASH_EQUAL, list, line);
    FKL_TODO();
    // FklNastNode *list = nodes[1].ast;
    // if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
    //     return NULL;
    // FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    // size_t len = fklNastListLength(list);
    // FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQUAL, len);
    // r->hash = ht;
    // size_t i = 0;
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
    //     ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
    //     ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    // }
    // return r;
}

static inline void *builtin_prod_action_bytevector(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 1)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *node = nodes[0].ast;
    if (!FKL_IS_STR(node))
        return NULL;
    return fklCreateVMvalueBvec2(c->exe,
            FKL_VM_STR(node)->size,
            (const uint8_t *)FKL_VM_STR(node)->str);
    FKL_TODO();
    // FklNastNode *list = nodes[1].ast;
    // FklNastNode *r = fklCreateNastNode(FKL_NAST_BYTEVECTOR, line);
    // size_t len = fklNastListLength(list);
    // FklBytevector *bv = fklCreateBytevector(len, NULL);
    // r->bvec = bv;
    // size_t i = 0;
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
    //     FklNastNode *cur = list->pair->car;
    //     if (cur->type == FKL_NAST_FIX)
    //         bv->ptr[i] = cur->fix > UINT8_MAX ? UINT8_MAX
    //                                           : (cur->fix < 0 ? 0 :
    //                                           cur->fix);
    //     else
    //         bv->ptr[i] = cur->bigInt->num < 0 ? 0 : UINT8_MAX;
    // }
    // return r;
}

static struct CstrIdProdAction {
    const char *name;
    FklProdActionFunc func;
} BuiltinProdActions[FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM] = {
    // clang-format off
    {"nil",       builtin_prod_action_nil        },
    {"symbol",    builtin_prod_action_symbol     },
    {"first",     builtin_prod_action_first      },
    {"second",    builtin_prod_action_second     },
    {"third",     builtin_prod_action_third      },
    {"pair",      builtin_prod_action_pair       },
    {"cons",      builtin_prod_action_cons       },
    {"box",       builtin_prod_action_box        },
    {"vector",    builtin_prod_action_vector     },
    {"quote",     builtin_prod_action_quote      },
    {"unquote",   builtin_prod_action_unquote    },
    {"qsquote",   builtin_prod_action_qsquote    },
    {"unqtesp",   builtin_prod_action_unqtesp    },
    {"hasheq",    builtin_prod_action_hasheq     },
    {"hasheqv",   builtin_prod_action_hasheqv    },
    {"hashequal", builtin_prod_action_hashequal  },
    {"bytes",     builtin_prod_action_bytevector },
    // clang-format on
};

static inline void init_builtin_prod_action_list(FklCodegenCtx *ctx) {
    FklVMvalue **const builtin_prod_action_id = ctx->builtin_prod_action_id;
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++)
        builtin_prod_action_id[i] =
                add_symbol_cstr(ctx, BuiltinProdActions[i].name);
    // fklAddSymbolCstr(CodegenProdActions[i].name, pst);
}

void fklInitProdActionList(FklCodegenCtx *ctx) {
    init_builtin_prod_action_list(ctx);
    init_simple_prod_action_list(ctx);
}

static inline FklProdActionFunc find_builtin_prod_action(FklCodegenCtx *ctx,
        FklVMvalue *id) {
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++) {
        if (ctx->builtin_prod_action_id[i] == id)
            return BuiltinProdActions[i].func;
    }
    return NULL;
}

FklGrammerProduction *fklCreateBuiltinActionProd(FklCodegenCtx *ctx,
        struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        FklVMvalue *id) {
    FklProdActionFunc action = id == NULL ? builtin_prod_action_first
                                          : find_builtin_prod_action(ctx, id);
    if (action == NULL) {
        return NULL;
    }

    return fklCreateProduction(group,
            sid,
            len,
            syms,
            NULL,
            action,
            id,
            fklProdCtxDestroyDoNothing,
            fklProdCtxCopyerDoNothing);
}

static inline const char *find_builtin_prod_action_name(
        FklProdActionFunc func) {
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++) {
        if (BuiltinProdActions[i].func == func)
            return BuiltinProdActions[i].name;
    }
    return NULL;
}

// static inline int is_simple_action(FklProdActionFunc func) {
//     for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++) {
//         if (fklGetSimpleProdActions()[i].func == func)
//             return 1;
//     }
//     return 0;
// }

// static inline const struct FklSimpleProdAction *find_simple_prod_action_name(
//         FklProdActionFunc func) {
//     for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++) {
//         if (fklGetSimpleProdActions()[i].func == func)
//             return &fklGetSimpleProdActions()[i];
//     }
//     return NULL;
// }

static inline const struct FklSimpleProdAction *
find_simple_prod_action(FklVMvalue *id, FklVMvalue *simple_prod_action_id[]) {
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++) {
        if (simple_prod_action_id[i] == id)
            return &CodegenProdCreatorActions[i];
    }
    return NULL;
}

static void simple_action_ctx_ud_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(c, struct FklSimpleActionCtx, ud);
    fklVMgcToGray(c->vec, gc);
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(simple_action_ctx_ud_as_print,
        "simple-action-ctx")

static int simple_action_ctx_ud_finalizer(FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(c, struct FklSimpleActionCtx, ud);
    if (c->mt == NULL)
        return FKL_VM_UD_FINALIZE_NOW;

    c->mt = NULL;
    c->vec = NULL;
    return FKL_VM_UD_FINALIZE_NOW;
}

static const FklVMudMetaTable SimpleActionCtxUdMetaTable = {
    .size = sizeof(FklSimpleActionCtx),
    .__atomic = simple_action_ctx_ud_atomic,
    .__as_prin1 = simple_action_ctx_ud_as_print,
    .__as_princ = simple_action_ctx_ud_as_print,
    .__finalizer = simple_action_ctx_ud_finalizer,
};

static inline FklVMvalue *create_simple_prod_action_ctx(FklCodegenCtx *cg_ctx,
        FklVMvalue *action_ast) {
    const FklSimpleProdAction *mt =
            find_simple_prod_action(FKL_VM_VEC(action_ast)->base[0],
                    cg_ctx->simple_prod_action_id);
    if (mt == NULL)
        return NULL;

    FklVMvalue **rest = &FKL_VM_VEC(action_ast)->base[1];
    size_t rest_len = FKL_VM_VEC(action_ast)->size - 1;

    FklVMvalueSimpleActionCtx *v =
            (FklVMvalueSimpleActionCtx *)fklCreateVMvalueUd(&cg_ctx->gc->gcvm,
                    &SimpleActionCtxUdMetaTable,
                    NULL);

    int r = mt->check(rest, rest_len);
    if (r != 0) {
        return NULL;
    }
    v->c.mt = mt;
    v->c.vec = action_ast;

    return FKL_TYPE_CAST(FklVMvalue *, v);
}

FklGrammerProduction *fklCreateSimpleActionProd(FklCodegenCtx *cg_ctx,
        struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        struct FklVMvalue *action) {
    FklVMvalue *action_ctx = create_simple_prod_action_ctx(cg_ctx, action);
    if (action_ctx == NULL)
        return NULL;
    return fklCreateProduction(group,
            sid,
            len,
            syms,
            NULL,
            simple_action,
            action_ctx,
            fklProdCtxDestroyDoNothing,
            fklProdCtxCopyerDoNothing);
}

int fklIsSimpleActionProd(const FklGrammerProduction *p) {
    return p->func == simple_action;
}

FklProdActionFunc fklFindBuiltinProdActionByName(const char *str) {
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++) {
        if (strcmp(BuiltinProdActions[i].name, str) == 0)
            return BuiltinProdActions[i].func;
    }
    return NULL;
}

FklGrammerProduction *fklCreateExtraStartProduction(FklCodegenCtx *ctx,
        FklVMvalue *group,
        FklVMvalue *sid) {
    FklGrammerProduction *prod =
            fklCreateEmptyProduction(ctx->builtin_g.start.group,
                    ctx->builtin_g.start.sid,
                    1,
                    NULL,
                    NULL,
                    NULL,
                    fklProdCtxDestroyDoNothing,
                    fklProdCtxCopyerDoNothing);
    prod->func = builtin_prod_action_first;
    prod->idx = 0;

    FklGrammerSym *u = &prod->syms[0];
    u->type = FKL_TERM_NONTERM;
    u->nt.group = group;
    u->nt.sid = sid;
    return prod;
}

// void fklRecomputeSidForNamedProdGroups(FklGraProdGroupHashMap *ht,
//         const FklSymbolTable *origin_st,
//         FklSymbolTable *target_st,
//         const FklConstTable *origin_kt,
//         FklConstTable *target_kt,
//         FklCodegenRecomputeNastSidOption option) {
//     if (ht && ht->buckets) {
//         for (FklGraProdGroupHashMapNode *list = ht->first; list;
//                 list = list->next) {
//             replace_sid(FKL_TYPE_CAST(FklSid_t *, &list->k),
//                     origin_st,
//                     target_st);
//             *FKL_TYPE_CAST(uintptr_t *, &list->hashv) =
//                     fklGraProdGroupHashMap__hashv(&list->k);
//
//             for (FklProdHashMapNode *cur = list->v.g.productions.first; cur;
//                     cur = cur->next) {
//                 for (FklGrammerProduction *prod = cur->v; prod;
//                         prod = prod->next) {
//                     if (prod->func == custom_action) {
//                         struct CustomActionCtx *ctx = prod->ctx;
//                         if (!ctx->is_recomputed) {
//                             fklRecomputeSidAndConstIdForBcl(ctx->bcl,
//                                     origin_st,
//                                     target_st,
//                                     origin_kt,
//                                     target_kt);
//                             ctx->is_recomputed = 1;
//                         }
//                     }
//                 }
//             }
//         }
//         fklGraProdGroupHashMapRehash(ht);
//     }
// }

void fklInitPreLibReaderMacros(FklCodegenLibVector *libraries,
        FklVMgc *st,
        FklCodegenCtx *ctx,
        FklFuncPrototypes *pts,
        FklCodegenLibVector *macro_libraries) {
    FKL_TODO();
    // uint32_t top = libraries->size;
    // for (uint32_t i = 0; i < top; i++) {
    //     FklCodegenLib *lib = &libraries->base[i];
    //     if (lib->named_prod_groups.buckets) {
    //         for (FklGraProdGroupHashMapNode *l =
    //         lib->named_prod_groups.first;
    //                 l;
    //                 l = l->next) {
    //
    //             for (const FklStrHashSetNode *cur = l->v.delimiters.first;
    //             cur;
    //                     cur = cur->next)
    //                 fklAddString(&l->v.g.delimiters, cur->k);
    //
    //             for (FklProdHashMapNode *cur = l->v.g.productions.first; cur;
    //                     cur = cur->next) {
    //                 for (FklGrammerProduction *prod = cur->v; prod;
    //                         prod = prod->next) {
    //                     if (prod->func == custom_action) {
    //                         FklCustomActionCtx *action_ctx = prod->ctx;
    //                         action_ctx->ctx = ctx;
    //                         action_ctx->macro_libraries = macro_libraries;
    //                         action_ctx->pts = pts;
    //                         action_ctx->is_recomputed = 0;
    //                         prod->func = custom_action;
    //                         prod->ctx = action_ctx;
    //                         prod->ctx_destroyer = custom_action_ctx_destroy;
    //                         prod->ctx_copyer = custom_action_ctx_copy;
    //                     }
    //                 }
    //             }
    //         }
    //     }
    // }
}

void fklPrintReaderMacroAction(FILE *fp, const FklGrammerProduction *prod) {
    FKL_TODO();
    // if (prod->func == custom_action) {
    //     struct CustomActionCtx *ctx = prod->ctx;
    //     fputs("custom\n", fp);
    //     fklPrintByteCodelnt(ctx->bcl, fp, pst, pkt);
    //     fputc('\n', fp);
    // } else if (is_simple_action(prod->func)) {
    //     struct FklSimpleProdAction *a =
    //             find_simple_prod_action_name(prod->func);
    //     FKL_ASSERT(a);
    //     fprintf(fp, "simple %s(", a->name);
    //     a->print(prod->ctx, pst, fp);
    //     fputc(')', fp);
    // } else {
    //     const char *name = find_builtin_prod_action_name(prod->func);
    //     FKL_ASSERT(name);
    //     fprintf(fp, "builtin %s", name);
    // }
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

static int increase_bcl_lib_prototype_id_func(void *cctx,
        FklOpcode *popcode,
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
    fklRecomputeInsImm(bcl,
            &ctx,
            increase_bcl_lib_prototype_id_predicate,
            increase_bcl_lib_prototype_id_func);
}

static inline void increase_compiler_macros_lib_prototype_id(
        FklCodegenMacro *head,
        uint32_t macro_lib_count,
        uint32_t macro_pts_count) {
    for (; head; head = head->next) {
        head->prototype_id += macro_pts_count;
        increase_bcl_lib_prototype_id(head->bcl,
                macro_lib_count,
                macro_pts_count);
    }
}

static inline void increase_reader_macro_lib_prototype_id(
        FklGraProdGroupHashMap *named_prod_groups,
        uint32_t lib_count,
        uint32_t count) {
    FKL_TODO();
    // if (named_prod_groups->buckets) {
    //     for (FklGraProdGroupHashMapNode *list = named_prod_groups->first;
    //     list;
    //             list = list->next) {
    //
    //         for (const FklProdHashMapNode *cur = list->v.g.productions.first;
    //                 cur;
    //                 cur = cur->next) {
    //             for (const FklGrammerProduction *prod = cur->v; prod;
    //                     prod = prod->next) {
    //                 if (prod->func == custom_action) {
    //                     FklCustomActionCtx *ctx = prod->ctx;
    //                     ctx->prototype_id += count;
    //                     increase_bcl_lib_prototype_id(ctx->bcl,
    //                             lib_count,
    //                             count);
    //                 }
    //             }
    //         }
    //     }
    // }
}

void fklIncreaseLibIdAndPrototypeId(FklCodegenLib *lib,
        uint32_t lib_count,
        uint32_t macro_lib_count,
        uint32_t pts_count,
        uint32_t macro_pts_count) {
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        lib->prototypeId += pts_count;
        increase_bcl_lib_prototype_id(lib->bcl, lib_count, pts_count);
        increase_compiler_macros_lib_prototype_id(lib->head,
                macro_lib_count,
                macro_pts_count);
        increase_reader_macro_lib_prototype_id(&lib->named_prod_groups,
                macro_lib_count,
                macro_pts_count);
        break;
    case FKL_CODEGEN_LIB_DLL:
        break;
    case FKL_CODEGEN_LIB_UNINIT:
        FKL_UNREACHABLE();
        break;
    }
}
