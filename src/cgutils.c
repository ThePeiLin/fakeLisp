#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_lw.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/regex.h>
#include <fakeLisp/str_buf.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include "codegen.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static FklVMframe *init_macro_expand_frame(FklVM *exe,
        FklCgCtx *ctx,
        FklVMvalue *proc,
        FklPmatchHashMap *ht,
        FklVMvalueLnt *lnt,
        FklVMvalue **pr,
        uint64_t curline);

static inline FklSymDefHashMapElm *get_def_by_id_in_scope(FklVMvalue *id,
        uint32_t scopeId,
        const FklCgEnvScope *scope) {
    FklSidScope key = { id, scopeId };
    return fklSymDefHashMapAt(&scope->defs, &key);
}

FklCgEnvScope *fklCgEnvScopeGet(const FklVMvalueCgEnv *env, uint32_t scope_id) {
    FKL_ASSERT(scope_id != 0);
    return &env->scopes.base[scope_id - 1];
}

FklSymDefHashMapElm *fklFindSymbolDef1(const FklVMvalueCgEnv *env,
        uint32_t scope_id,
        FklVMvalue *id) {
    FklSymDefHashMapElm *r = NULL;
    for (; scope_id; scope_id = fklCgEnvScopeGet(env, scope_id)->p) {
        const FklCgEnvScope *scope = fklCgEnvScopeGet(env, scope_id);
        r = get_def_by_id_in_scope(id, scope_id, scope);
        if (r) {
            return r;
        }
    }
    return NULL;
}

FklSymDef *
fklUseSymbolDef(FklVMvalueCgEnv *env, uint32_t scope, FklVMvalue *id) {
    FklSymDefHashMapElm *r = fklFindSymbolDef1(env, scope, id);
    if (r == NULL) {
        return NULL;
    }

    fklSetImportedSymbolUsed(env, &r->v);
    return &r->v;
}

FklSymDefHashMapElm *fklGetCgDefByIdInScope(FklVMvalue *id,
        uint32_t scope_id,
        const FklVMvalueCgEnv *env) {
    const FklCgEnvScope *scope = fklCgEnvScopeGet(env, scope_id);
    return get_def_by_id_in_scope(id, scope_id, scope);
}

void fklPrintCgError(FklCgCtx *ctx,
        const FklVMvalueCgInfo *info,
        FklCodeBuilder *cb) {
    FklCgErrorState *error_state = ctx->error_state;
    size_t line = error_state->line;
    FklVMvalue *fid = error_state->fid;

    fklPrintErrBacktrace(error_state->error, ctx->vm, NULL);

    memset(error_state, 0, sizeof(*error_state));

    if (fid) {
        fklCodeBuilderFmt(cb,
                "at line %" PRIu64 " of file %s\n",
                line,
                FKL_VM_SYM(fid)->str);
    } else if (info->filename) {
        fklCodeBuilderFmt(cb,
                "at line %" PRIu64 " of file %s\n",
                line,
                info->filename);
    } else {
        fklCodeBuilderFmt(cb, "at line %" PRIu64 "\n", line);
    }
}

#define INIT_SYMBOL_DEF(ID, SCOPE, IDX) { { ID, SCOPE }, IDX, IDX, 0, 0 }

FklSymDefHashMapElm *fklAddCgBuiltinRefBySid(FklVMvalue *id,
        FklVMvalueCgEnv *env) {
    FklSymDefHashMap *ht = &env->refs;
    uint32_t idx = ht->count;
    return fklSymDefHashMapInsert2(ht,
            (FklSidScope){ .sid = id, .scope = env->parent_scope },
            (FklSymDef){ .idx = idx, .cidx = idx, .isLocal = 0, .isConst = 0 });
}

static inline void *use_outer_pdef_or_def(FklVMvalueCgEnv *cur,
        FklVMvalue *id,
        uint32_t scope,
        FklVMvalueCgEnv **targetEnv,
        int *is_pdef) {
    for (; cur; cur = cur->prev) {
        uint8_t *key = fklGetCgPreDefBySid(id, scope, cur);
        if (key) {
            *targetEnv = cur;
            *is_pdef = 1;
            return key;
        }
        FklSymDef *def = fklUseSymbolDef(cur, scope, id);
        if (def) {
            *targetEnv = cur;
            return def;
        }
        scope = cur->parent_scope;
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
        FklVMvalueCgEnv *cur,
        FklVMvalueCgEnv *targetEnv,
        uint8_t isConst,
        FklSymDefHashMapElm **new_ref) {
    uint32_t idx = cur->refs.count;
    FklSymDefHashMapElm *cel = fklSymDefHashMapInsert2(&cur->refs,
            (FklSidScope){ .sid = id, .scope = cur->parent_scope },
            (FklSymDef){ .idx = idx,
                .cidx = idx,
                .isConst = isConst,
                .isLocal = 0 });
    *new_ref = cel;
    FklSidScope key = { .sid = id, .scope = cur->parent_scope };
    FklSymDef def;
    for (cur = cur->prev; cur != targetEnv; cur = cur->prev) {
        uint32_t idx = cur->refs.count;
        key.scope = cur->parent_scope;
        initSymbolDef(&def, idx);
        FklSymDefHashMapElm *nel =
                fklSymDefHashMapInsert(&cur->refs, &key, &def);
        cel->v.cidx = nel->v.idx;
        cel = nel;
    }
    return cel;
}

static inline uint32_t get_child_env_prototype_id(FklVMvalueCgEnv *cur,
        FklVMvalueCgEnv *target) {
    FKL_ASSERT(cur != target);
    for (; cur->prev != target; cur = cur->prev)
        ;
    return cur->proto_id;
}

static inline FklSymDefHashMapElm *has_outer_ref(FklVMvalueCgEnv *cur,
        FklVMvalue *id,
        FklVMvalueCgEnv **targetEnv) {
    FklSymDefHashMapElm *ref = NULL;
    FklSidScope key = { id, 0 };
    for (; cur; cur = cur->prev) {
        key.scope = cur->parent_scope;
        ref = fklSymDefHashMapAt(&cur->refs, &key);
        if (ref) {
            *targetEnv = cur;
            return ref;
        }
    }
    return NULL;
}

static inline int is_ref_solved(FklSymDefHashMapElm *ref,
        FklVMvalueCgEnv *env) {
    if (env) {
        uint32_t top = env->uref.size;
        FklUnbound *refs = env->uref.base;
        for (uint32_t i = 0; i < top; i++) {
            FklUnbound *cur = &refs[i];
            if (cur->sid == ref->k.sid && cur->scope == ref->k.scope)
                return 0;
        }
    }
    return 1;
}

static inline void init_unbound(FklUnbound *r,
        FklVMvalue *id,
        uint32_t idx,
        uint32_t scope,
        FklVMvalueCgEnv *env,
        uint32_t assign,
        FklVMvalue *fid,
        uint64_t line) {
    r->sid = id;
    r->idx = idx;
    r->scope = scope;
    r->env = env;
    r->assign = assign;
    r->fid = fid;
    r->line = line;
    r->has_weak_ref = 0;
}

static inline void initPdefRef(FklPreDefRef *r,
        FklVMvalue *id,
        uint32_t scope,
        uint32_t prototypeId,
        uint32_t idx) {
    r->sid = id;
    r->scope = scope;
    r->prototypeId = prototypeId;
    r->idx = idx;
}

FklSymDef *fklGetCgRefBySid(FklVMvalue *id, FklVMvalueCgEnv *env) {
    FklSymDefHashMap *ht = &env->refs;
    return fklSymDefHashMapGet2(ht, (FklSidScope){ id, env->parent_scope });
}

static inline FklUnbound *
has_resolvable_ref(FklVMvalue *id, uint32_t scope, const FklVMvalueCgEnv *env) {
    FklUnbound *urefs = env->uref.base;
    uint32_t top = env->uref.size;
    for (uint32_t i = 0; i < top; i++) {
        FklUnbound *cur = &urefs[i];
        if (cur->sid == id && cur->scope == scope)
            return cur;
    }
    return NULL;
}

void fklAddCgPreDefBySid(FklVMvalue *id,
        uint32_t scope,
        uint8_t isConst,
        FklVMvalueCgEnv *env) {
    FklPredefHashMap *pdef = &env->pdef;
    FklSidScope key = { id, scope };
    fklPredefHashMapAdd(pdef, &key, &isConst);
}

uint8_t *
fklGetCgPreDefBySid(FklVMvalue *id, uint32_t scope, FklVMvalueCgEnv *env) {
    FklPredefHashMap *pdef = &env->pdef;
    FklSidScope key = { id, scope };
    return fklPredefHashMapGet(pdef, &key);
}

void fklAddCgRefToPreDef(FklVMvalue *id,
        uint32_t scope,
        uint32_t prototypeId,
        uint32_t idx,
        FklVMvalueCgEnv *env) {
    initPdefRef(fklPreDefRefVectorPushBack(&env->ref_pdef, NULL),
            id,
            scope,
            prototypeId,
            idx);
}

void fklResolveCgPreDef(FklVMvalue *id, uint32_t scope, FklVMvalueCgEnv *env) {
    FklPreDefRefVector *ref_pdef = &env->ref_pdef;
    const FklValueVector *child_proc_protos = &env->child_proc_protos;
    FklPreDefRefVector ref_pdef1;
    uint32_t count = ref_pdef->size;
    fklPreDefRefVectorInit(&ref_pdef1, count);
    uint8_t pdef_isconst;
    FklSidScope key = { id, scope };
    fklPredefHashMapErase(&env->pdef, &key, &pdef_isconst, NULL);
    FklSymDefHashMapElm *def = fklGetCgDefByIdInScope(id, scope, env);
    FKL_ASSERT(def);
    for (uint32_t i = 0; i < count; i++) {
        const FklPreDefRef *pdef_ref = &ref_pdef->base[i];
        if (pdef_ref->sid == id && pdef_ref->scope == scope) {
            FklVMvalue *pt_v = child_proc_protos->base[pdef_ref->prototypeId];
            FKL_ASSERT(pt_v && fklIsVMvalueProto(pt_v));

            FklVMvalueProto *cpt = fklVMvalueProto(pt_v);
            FklVarRefDef *ref = &fklVMvalueProtoVarRefs(cpt)[pdef_ref->idx];
            ref->cidx = FKL_MAKE_VM_FIX(def->v.idx);
            env->slots.base[def->v.cidx] = FKL_CODEGEN_ENV_SLOT_REF;
        } else {
            fklPreDefRefVectorPushBack(&ref_pdef1, pdef_ref);
        }
    }
    ref_pdef->size = 0;
    while (!fklPreDefRefVectorIsEmpty(&ref_pdef1))
        fklPreDefRefVectorPushBack2(ref_pdef,
                *fklPreDefRefVectorPopBack(&ref_pdef1));
    fklPreDefRefVectorUninit(&ref_pdef1);
}

void fklClearCgPreDef(FklVMvalueCgEnv *env) {
    fklPredefHashMapClear(&env->pdef);
}

FklSymDefHashMapElm *fklAddCgRefBySid(FklVMvalue *id,
        FklVMvalueCgEnv *env,
        FklVMvalue *fid,
        uint64_t line,
        uint32_t assign) {
    FklSymDefHashMap *refs = &env->refs;
    FklSymDefHashMapElm *el =
            fklSymDefHashMapAt2(refs, (FklSidScope){ id, env->parent_scope });
    if (el) {
        FklUnbound *ref = has_resolvable_ref(id,
                env->parent_scope,
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
        FklVMvalueCgEnv *prev = env->prev;
        if (prev) {
            FklVMvalueCgEnv *targetEnv = NULL;
            int is_pdef = 0;
            void *targetDef = use_outer_pdef_or_def(prev,
                    id,
                    env->parent_scope,
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
                    fklAddCgRefToPreDef(id,
                            env->parent_scope,
                            get_child_env_prototype_id(env, targetEnv),
                            cel->v.idx,
                            targetEnv);
                } else {
                    FklSymDef *def = (FklSymDef *)targetDef;
                    FklSymDefHashMapElm *cel = add_ref_to_all_penv(id,
                            env,
                            targetEnv,
                            def->isConst,
                            &ret);
                    cel->v.isLocal = 1;
                    cel->v.cidx = def->idx;
                    FklCgEnvSlot *slot_flags = targetEnv->slots.base;
                    slot_flags[def->idx] = FKL_CODEGEN_ENV_SLOT_REF;
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
                            (FklSidScope){ .sid = id,
                                .scope = env->parent_scope },
                            (FklSymDef){ .idx = idx, .cidx = idx });
                    ret->v.cidx = FKL_VAR_REF_INVALID_CIDX;

                    init_unbound(fklUnboundVectorPushBack(&prev->uref, NULL),
                            id,
                            idx,
                            env->parent_scope,
                            env,
                            assign,
                            fid,
                            line);
                }
            }
        } else {
            ret = fklSymDefHashMapInsert2(refs,
                    (FklSidScope){ .sid = id, .scope = 0 },
                    (FklSymDef){ .idx = idx, .cidx = idx });
            ret->v.cidx = FKL_VAR_REF_INVALID_CIDX;
            idx = ret->v.idx;
            init_unbound(fklUnboundVectorPushBack(&env->uref, NULL),
                    id,
                    idx,
                    0,
                    env,
                    assign,
                    fid,
                    line);
        }
        return ret;
    }
}

uint32_t fklAddCgRefBySidRetIndex(FklVMvalue *id,
        FklVMvalueCgEnv *env,
        FklVMvalue *fid,
        uint64_t line,
        uint32_t assign) {
    return fklAddCgRefBySid(id, env, fid, line, assign)->v.idx;
}

int fklIsSymbolDefined(FklVMvalue *id,
        uint32_t scope_id,
        const FklVMvalueCgEnv *env) {
    const FklCgEnvScope *scope = fklCgEnvScopeGet(env, scope_id);
    return get_def_by_id_in_scope(id, scope_id, scope) != NULL;
}

static inline uint32_t get_next_empty(uint32_t empty,
        const FklCgEnvSlotVector *flags) {
    for (; empty < flags->size
            && flags->base[empty] != FKL_CODEGEN_ENV_SLOT_NONE;
            empty++)
        ;
    return empty;
}

FklSymDef *
fklAddCgDefBySid(FklVMvalue *id, uint32_t scope_id, FklVMvalueCgEnv *env) {
    FklCgEnvScope *scope = fklCgEnvScopeGet(env, scope_id);
    FklSymDefHashMap *defs = &scope->defs;
    FklSidScope key = { id, scope_id };
    FklSymDef *el = fklSymDefHashMapGet(defs, &key);
    if (!el) {
        uint32_t idx = scope->empty;
        el = fklSymDefHashMapAdd(defs, &key, NULL);
        el->from = FKL_VAR_REF_INVALID_CIDX;
        el->from_idx = FKL_VAR_REF_INVALID_CIDX;

        if (idx < env->slots.size && has_resolvable_ref(id, scope_id, env)) {
            idx = env->slots.size;
        } else {
            scope->empty = get_next_empty(scope->empty + 1, &env->slots);
        }
        el->idx = idx;
        uint32_t end = (idx + 1) - scope->start;
        if (scope->end < end)
            scope->end = end;
        if (idx >= env->slots.size) {
            size_t new_slots_count = (idx + 1) - env->slots.size;
            for (size_t i = 0; i < new_slots_count; ++i) {
                fklCgEnvSlotVectorPushBack2(&env->slots,
                        FKL_CODEGEN_ENV_SLOT_NONE);
            }
        }
        env->slots.base[idx] = FKL_CODEGEN_ENV_SLOT_OCC;
    }
    return el;
}

void fklSetImportedSymbolUsed(const FklVMvalueCgEnv *env,
        const FklSymDef *def) {
    if (def->from == FKL_VAR_REF_INVALID_CIDX) {
        return;
    }
    FKL_ASSERT(def->from_idx != FKL_VAR_REF_INVALID_CIDX);

    FklVMvalue *v = env->imported_symbols.base[def->from];
    FKL_ASSERT(v != NULL);

    uint8_t *ptr = FKL_VM_BVEC(v)->ptr;
    ptr[def->from_idx] = FKL_IMPORT_SYMBOL_USED;
}

const uint8_t *fklGetImportedSymbolUsed(const FklVMvalueCgEnv *env,
        uint32_t from,
        uint32_t idx) {
    FKL_ASSERT(env != NULL);
    if (env->imported_symbols.size <= from)
        return NULL;
    FklVMvalue *v = env->imported_symbols.base[from];
    if (v == NULL)
        return NULL;
    const FklBytevector *bvec = FKL_VM_BVEC(v);

    if (idx < bvec->size) {
        return &FKL_VM_BVEC(v)->ptr[idx];
    } else {
        return NULL;
    }
}

void fklResolveRef(FklVMvalueCgEnv *env,
        uint32_t scope,
        const FklResolveRefArgs *args) {
    int no_refs_to_builtins = args ? args->no_refs_to_builtins : 0;
    FklResolveRefToDefCb resolve_ref_to_def_cb =
            args ? args->resolve_ref_to_def_cb : NULL;
    FklVMvalueCgEnv *top_env = args ? args->top_env : NULL;
    FklVMvalueWeakHashEq *weak_refs = args ? args->weak_refs : NULL;

    FklUnboundVector *urefs = &env->uref;
    FklUnboundVector urefs1;
    uint32_t count = urefs->size;

    fklUnboundVectorInit(&urefs1, count);
    for (uint32_t i = 0; i < count; i++) {
        FklUnbound *uref = &urefs->base[i];
        if (uref->scope < scope) {
            // 忽略来自父作用域的未解决引用
            fklUnboundVectorPushBack(&urefs1, uref);
            continue;
        }

        FklVMvalueProto *pt = uref->env->proto;
        FklVarRefDef *const ref = &fklVMvalueProtoVarRefs(pt)[uref->idx];
        const FklSymDef *def = fklUseSymbolDef(env, uref->scope, uref->sid);

        if (def) {
            env->slots.base[def->idx] = FKL_CODEGEN_ENV_SLOT_REF;
            ref->cidx = FKL_MAKE_VM_FIX(def->idx);
            ref->is_local = FKL_VM_TRUE;

            if (resolve_ref_to_def_cb) {
                resolve_ref_to_def_cb(ref,
                        def,
                        uref,
                        pt,
                        args->resolve_ref_to_def_cb_args);
            }
        } else if (fklCgEnvScopeGet(env, uref->scope)->p != 0) {
            uref->scope = fklCgEnvScopeGet(env, uref->scope)->p;
            fklUnboundVectorPushBack(&urefs1, uref);
        } else if (env->prev != top_env) {
            uint32_t cidx = fklAddCgRefBySidRetIndex(uref->sid,
                    env,
                    uref->fid,
                    uref->line,
                    uref->assign);
            ref->cidx = FKL_MAKE_VM_FIX(cidx);
        } else {
            if (!no_refs_to_builtins) {
                fklAddCgBuiltinRefBySid(uref->sid, env);
            }

            if (!uref->has_weak_ref //
                    || weak_refs == NULL
                    || fklVMvalueWeakHashEqGet(weak_refs, uref->sid) != NULL) {
                fklUnboundVectorPushBack(&urefs1, uref);
            }
        }
    }

    urefs->size = 0;

    FklValueTable id_table = { 0 };
    if (weak_refs) {
        fklInitValueTable(&id_table);
        for (const FklValueEqHashMapNode *cur = weak_refs->ht.first; cur;
                cur = cur->next) {
            fklValueTableAdd(&id_table, cur->v);
        }
    }

    while (!fklUnboundVectorIsEmpty(&urefs1)) {
        FklUnbound *uref = fklUnboundVectorPopBackNonNull(&urefs1);

        fklUnboundVectorPushBack(urefs, uref);
        if (weak_refs == NULL) {
            continue;
        }

        FklVMvalueProto *pt = uref->env->proto;

        FklVarRefDef *const ref = &fklVMvalueProtoVarRefs(pt)[uref->idx];
        FklValueEqHashMapElm *i = fklVMvalueWeakHashEqInsert(weak_refs, //
                uref->sid);
        FklVMvalue *cidx_v = NULL;
        if (i->v == NULL) {
            uint32_t cidx = (weak_refs->ht.count - 1) + env->refs.count;
            cidx_v = FKL_MAKE_VM_FIX(cidx);
            FklVMvalue *ref = fklCreateClosedVMvalueVarRef(args->vm, NULL);
            i->v = ref;
            uref->has_weak_ref = 1;
            fklValueTableAdd(&id_table, i->v);
        } else {
            uint64_t idx = fklValueTableGet(&id_table, i->v) - 1;
            cidx_v = FKL_MAKE_VM_FIX(idx + env->refs.count);
        }
        ref->cidx = cidx_v;
    }

    if (weak_refs)
        fklUninitValueTable(&id_table);
    fklUnboundVectorUninit(&urefs1);
}

static inline void update_parent_env_proto(const FklVMvalueCgEnv *env,
        FklVMvalue *proto) {
    FKL_ASSERT(fklIsVMvalueProto(proto));
    FklVMvalueCgEnv *parent_env = env->prev;
    FKL_ASSERT(parent_env);
    const FklValueVector *child_proc_protos = &parent_env->child_proc_protos;
    FKL_ASSERT(env->proto_id < child_proc_protos->size);

    child_proc_protos->base[env->proto_id] = FKL_VM_VAL(proto);
}

void fklPrintUndefinedRef(const FklVMvalueCgEnv *env, FklCodeBuilder *cb) {
    const FklUnboundVector *urefs = &env->uref;
    for (uint32_t i = urefs->size; i > 0; i--) {
        FklUnbound *ref = &urefs->base[i - 1];
        fklCodeBuilderPuts(cb, "warning: Symbol ");
        fklPrintSymbolLiteral2(FKL_VM_SYM(ref->sid), cb);
        fklCodeBuilderFmt(cb, " is undefined at line %" PRIu64, ref->line);
        if (ref->fid) {
            fklCodeBuilderPuts(cb, " of ");
            fklPrintString2(FKL_VM_SYM(ref->fid), cb);
        }
        fklCodeBuilderPutc(cb, '\n');
    }
}

FklVMvalueVec *fklCreateCgNamesVec(FklVM *vm,
        const FklCgExportSidIdxHashMap *map) {
    FklVMvalue *vv = fklCreateVMvalueVec(vm, map->count);
    FklVMvalueVec *names = FKL_VM_VEC(vv);

    const FklCgExportSidIdxHashMapNode *cur = NULL;
    for (cur = map->first; cur; cur = cur->next) {
        names->base[cur->v.idx] = cur->k;
    }

    return names;
}

static FklVMvalueLib *create_script_lib(FklVM *vm,
        FklVMvalue *name,
        FklVMvalueCgLib *clib,
        const FklVMvalueProc *proc) {
    FKL_ASSERT(FKL_IS_SYM(name));
    if (clib->lib) {
        FKL_ASSERT(FKL_IS_PROC(clib->lib->proc));
        return clib->lib;
    }

    FklVMvalueVec *names = fklCreateCgNamesVec(vm, &clib->exports);
    FklVMvalueLib *l = fklCreateVMvalueLib(vm, name, names);
    l->proc = FKL_VM_VAL(proc);
    clib->lib = l;
    return l;
}

static FklVMvalueLib *create_dll_lib(FklVM *vm,
        FklVMvalue *name,
        FklVMvalueCgLib *clib,
        FklVMvalue *rp) {
    FKL_ASSERT(FKL_IS_SYM(rp));
    if (clib->lib) {
        FklVMvalue *proc = clib->lib->proc;
        (void)proc;
        FKL_ASSERT(fklIsVMvalueDll(proc) || FKL_IS_STR(proc));
        return clib->lib;
    }

    FklVMvalueVec *names = fklCreateCgNamesVec(vm, &clib->exports);
    FklVMvalueLib *l = fklCreateVMvalueLib(vm, name, names);
    l->proc = rp;
    clib->lib = l;
    return l;
}

void fklInitCgDllLib(const FklCgCtx *ctx,
        FklVMvalue *name,
        FklVMvalueCgLib *lib,
        FklVMvalue *rp,
        uv_lib_t dll,
        FklCgDllLibInitExportCb init) {
    uint32_t num = 0;
    FklVMvalue **exports = init(ctx->vm, &num);
    FklCgExportSidIdxHashMap *exports_idx = &lib->exports;
    fklCgExportSidIdxHashMapInit(exports_idx);
    if (num) {
        for (uint32_t i = 0; i < num; i++) {
            FklCgExportIdx const idx = { .idx = i };
            fklCgExportSidIdxHashMapAdd(exports_idx, &exports[i], &idx);
        }
    }

    if (exports) {
        fklZfree(exports);
    }

    lib->lib = create_dll_lib(ctx->vm, name, lib, rp);

    lib->macros = ctx->hash_singleton;
    lib->replacements = ctx->hash_singleton;
    lib->rmacros = ctx->hash_singleton;

    lib->re_exports = FKL_VM_NIL;
}

void fklInitCgScriptLib(const FklCgCtx *ctx,
        FklVMvalueCgLib *lib,
        FklVMvalue *mod_name,
        const FklVMvalueCgInfo *info,
        const FklVMvalueProc *proc) {
    FKL_ASSERT(info != NULL);

    FklCgExportSidIdxHashMap *exports_index = &lib->exports;
    fklCgExportSidIdxHashMapInit(exports_index);
    const FklCgExportSidIdxHashMap *export_sid_set = &info->exports;
    for (const FklCgExportSidIdxHashMapNode *sid_idx_list =
                    export_sid_set->first;
            sid_idx_list;
            sid_idx_list = sid_idx_list->next) {
        fklCgExportSidIdxHashMapPut(exports_index,
                &sid_idx_list->k,
                &sid_idx_list->v);
    }

    lib->lib = create_script_lib(ctx->vm, mod_name, lib, proc);

    lib->macros = info->export_macros;
    lib->replacements = info->export_replacement;
    lib->rmacros = info->export_rmacros;

    lib->re_exports = info->re_exports;
}

static FKL_ALWAYS_INLINE FklVMvalueCgMacro *as_macro(const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCgMacro(r));
    return FKL_TYPE_CAST(FklVMvalueCgMacro *, r);
}

static const FklVMvalueCgMacro *find_macro(FklVMvalue *exp,
        const FklVMvalueCgMacroScope *macro_scope,
        FklPmatchHashMap *pht) {
    if (exp == NULL || !FKL_IS_PAIR(exp))
        return NULL;

    for (; macro_scope; macro_scope = macro_scope->prev) {
        FklVMvalueCgMacroHashMap *macros = macro_scope->macros;

        FklVMvalue *header = FKL_VM_CAR(exp);
        FklValueHashMapElm *pm = fklCgMacroHashMapGet(macros, header);

        if (pm == NULL)
            continue;

        for (FklVMvalue *p = pm->v; FKL_IS_PAIR(p); p = FKL_VM_CDR(p)) {
            FklVMvalueCgMacro *cur = as_macro(FKL_VM_CAR(p));
            FklVMvalue *pattern_cdr = cur->pattern;
            FklVMvalue *exp_cdr = FKL_VM_CDR(exp);
            if (fklPatternMatch1(header, pattern_cdr, exp_cdr, pht))
                return cur;

            fklPmatchHashMapClear(pht);
        }
    }
    return NULL;
}

static inline FklVMvalue *make_macroexpand_error(FklVM *exe,
        FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_MACROEXPANDFAILED,
            exe,
            "Failed to expand macro in %S",
            place);
}

static inline FklVMvalue *expand_macro_arg(FklCgCtx *ctx,
        FklPmatchExpandType e,
        const FklPmatchRes *exp,
        const FklVMvalueCgInfo *codegen,
        const FklVMvalueCgMacroScope *macros) {
    switch (e) {
    case FKL_PMATCH_EXPAND_NONE:
        return exp->value;
        break;
    case FKL_PMATCH_EXPAND_ONCE:
        return fklTryExpandCgMacroOnce(ctx, exp, codegen, macros);
        break;
    case FKL_PMATCH_EXPAND_ALL:
        return fklTryExpandCgMacro(ctx, exp, codegen, macros);
        break;
    }

    return NULL;
}

static inline int expand_all_macro_arg(FklCgCtx *ctx,
        const FklPmatchHashMap *ht,
        const FklVMvalueCgInfo *info,
        const FklVMvalueCgMacroScope *macro_scope) {
    for (FklPmatchHashMapNode *cur = ht->first; cur; cur = cur->next) {
        FklPmatchRes *r = &cur->v;
        FklVMvalue *rr = expand_macro_arg(ctx, r->expand, r, info, macro_scope);
        if (rr == NULL)
            return 1;
        r->value = rr;
    }

    return 0;
}

FKL_NODISCARD
static int execute_macro_expand_procedure(FklCgCtx *ctx,
        const char *file_dir,
        FklVMvalue *macro_proc,
        FklPmatchHashMap *ht,
        uint64_t curline,
        FklVMvalue **pretval) {
    const char *cwd = ctx->cwd;
    fklChdir(file_dir);

    FklVMgc *gc = ctx->vm->gc;

    int is_repl = ctx->vm != &gc->gcvm;

    FklVM *exe = is_repl ? ctx->vm : fklCreateVM(NULL, gc);

    uint32_t bottom_tp = exe->tp;
    FklVMframe *exit_frame = init_macro_expand_frame(exe,
            ctx,
            macro_proc,
            ht,
            ctx->lnt,
            pretval,
            curline);

    int e = is_repl ? fklRunVM(exe, exit_frame) : fklRunVMidleLoop(exe);
    fklMoveThreadObjectsToGc(exe, gc);

    fklChdir(cwd);

    fklPopVMframe2(exe, exit_frame);

    if (!is_repl) {
        fklDestroyAllVMs(exe);
    } else
        exe->tp = bottom_tp;

    return e;
}

FklVMvalue *fklTryExpandCgMacroOnce(FklCgCtx *ctx,
        const FklPmatchRes *exp,
        const FklVMvalueCgInfo *info,
        const FklVMvalueCgMacroScope *macros) {
    FklCgErrorState *error_state = ctx->error_state;
    FklVMvalue *r = exp->value;
    if (!FKL_IS_PAIR(r))
        return r;
    FklPmatchHashMap ht = { 0 };
    fklPmatchHashMapInit(&ht);

    FklPmatchStorage storage = { .ht = &ht };
    fklPushCgPmatchStorage(ctx, &storage);

    uint64_t curline = CURLINE(exp->container);
    for (const FklVMvalueCgMacro *macro = find_macro(r, macros, &ht);
            !error_state->error && macro;
            macro = find_macro(r, macros, &ht)) {
        if (expand_all_macro_arg(ctx, &ht, info, macros))
            return NULL;

        fklPmatchHashMapAdd2(&ht,
                ctx->builtin_sym_orig,
                (FklPmatchRes){
                    .value = r,
                    .container = exp->container,
                });
        FklVMvalue *retval = NULL;

        int e = execute_macro_expand_procedure(ctx,
                info->dir,
                macro->proc,
                &ht,
                curline,
                &retval);

        if (e) {
            error_state->error = make_macroexpand_error(ctx->vm, r);
            error_state->line = curline;
            r = NULL;
        } else if (retval) {
            r = retval;
        } else {
            error_state->line = curline;
        }
        fklPmatchHashMapClear(&ht);
        break;
    }

    fklPopCgPmatchStorage(ctx, &storage);
    fklPmatchHashMapUninit(&ht);
    return r;
}

FklVMvalue *fklTryExpandCgMacro(FklCgCtx *ctx,
        const FklPmatchRes *exp,
        const FklVMvalueCgInfo *info,
        const FklVMvalueCgMacroScope *macros) {
    FklCgErrorState *error_state = ctx->error_state;
    FklVMvalue *r = exp->value;
    if (!FKL_IS_PAIR(r))
        return r;

    FklPmatchHashMap ht = { 0 };
    fklPmatchHashMapInit(&ht);

    FklPmatchStorage storage = { .ht = &ht };
    fklPushCgPmatchStorage(ctx, &storage);

    uint64_t curline = CURLINE(exp->container);
    for (const FklVMvalueCgMacro *macro = find_macro(r, macros, &ht);
            !error_state->error && macro;
            macro = find_macro(r, macros, &ht)) {
        if (expand_all_macro_arg(ctx, &ht, info, macros))
            return NULL;

        fklPmatchHashMapAdd2(&ht,
                ctx->builtin_sym_orig,
                (FklPmatchRes){
                    .value = r,
                    .container = exp->container,
                });
        FklVMvalue *retval = NULL;

        int e = execute_macro_expand_procedure(ctx,
                info->dir,
                macro->proc,
                &ht,
                curline,
                &retval);

        if (e) {
            error_state->error = make_macroexpand_error(ctx->vm, r);
            error_state->line = curline;
            r = NULL;
        } else if (retval) {
            r = retval;
        } else {
            error_state->line = curline;
        }
        fklPmatchHashMapClear(&ht);
    }

    fklPopCgPmatchStorage(ctx, &storage);
    fklPmatchHashMapUninit(&ht);
    return r;
}

typedef struct MacroExpandCtx {
    FklVMvalue **retval;
    FklVMvalueLnt *lnt;
    uint64_t curline;
    FklCgErrorState *error_state;
} MacroExpandCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(MacroExpandCtx);

static inline FklVMvalue *
check_macro_expand_result(FklVMvalue *r, FklVMvalueLnt *lnt, uint64_t line) {
    if (fklIsSerializableToByteCodeFile(r, lnt, line))
        return r;
    return NULL;
}

static inline FklVMvalue *make_serializable_error(FklVM *exe, FklVMvalue *r) {
    return FKL_MAKE_VM_ERR(FKL_ERR_UNSERIALIZABLE,
            exe,
            "Unserializable to bytecode file value %S",
            r);
}

static int macro_expand_frame_step(void *data, FklVM *exe) {
    MacroExpandCtx *ctx = (MacroExpandCtx *)data;
    FKL_ASSERT(ctx->retval);

    FklVMvalue *r = FKL_VM_GET_TOP_VALUE(exe);
    *(ctx->retval) = check_macro_expand_result(r, ctx->lnt, ctx->curline);
    if (*(ctx->retval) == NULL)
        ctx->error_state->error = make_serializable_error(exe, r);

    return 0;
}

static void macro_expand_frame_atomic(void *data, FklVMgc *gc) {
    MacroExpandCtx *ctx = (MacroExpandCtx *)data;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->lnt), gc);
    if (*ctx->retval) {
        fklVMgcToGray(*ctx->retval, gc);
    }
}

static void
macro_expand_frame_backtrace(void *data, FklCodeBuilder *build, FklVM *vm) {
    fklCodeBuilderPuts(build, "<macroexpand>");
}

static const FklVMframeContextMethodTable MacroExpandMethodTable = {
    .step = macro_expand_frame_step,
    .finalizer = NULL,
    .print_backtrace = macro_expand_frame_backtrace,
    .atomic = macro_expand_frame_atomic,
};

static void push_macro_expand_frame(FklVM *exe,
        FklVMvalue **ptr,
        FklVMvalueLnt *lnt,
        uint64_t curline,
        FklCgErrorState *error_state) {
    FklVMframe *f = fklCreateOtherObjVMframe(exe, &MacroExpandMethodTable);
    MacroExpandCtx *ctx = (MacroExpandCtx *)f->data;
    ctx->retval = ptr;
    ctx->lnt = lnt;
    ctx->curline = curline;
    ctx->error_state = error_state;
    fklPushVMframe(f, exe);
}

static void init_macro_match_local_variable(FklVM *exe,
        FklVMframe *f,
        FklPmatchHashMap *ht,
        FklVMvalueLnt *lnt,
        const FklVMvalueProto *pt) {
    FklVMvalueProc *proc = FKL_VM_PROC(f->proc);
    uint32_t count = pt->local_count;
    uint32_t idx = 0;
    for (FklPmatchHashMapNode *list = ht->first; list; list = list->next) {
        FklVMvalue *v = list->v.value;
        FKL_VM_GET_ARG(exe, f, idx) = v;
        idx++;
    }
    f->lcount = count;
    f->lref = FKL_VM_NIL;
    f->lrefl = FKL_VM_NIL;
    proc->ref_count = f->rcount;
}

static inline FklVMframe *init_macro_expand_frame(FklVM *exe,
        FklCgCtx *ctx,
        FklVMvalue *proc,
        FklPmatchHashMap *ht,
        FklVMvalueLnt *lnt,
        FklVMvalue **pr,
        uint64_t curline) {
    FKL_ASSERT(FKL_IS_PROC(proc));
    FklCgErrorState *error_state = ctx->error_state;

    FklVMframe *exit_frame = exe->top_frame;

    push_macro_expand_frame(exe, pr, lnt, curline, error_state);

    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, proc);
    fklCallObj(exe, proc);
    init_macro_match_local_variable(exe,
            exe->top_frame,
            ht,
            lnt,
            FKL_VM_PROC(proc)->proto);
    return exit_frame;
}

static FklVMudMetaTable const MacroScopeUserDataMetaTable;
int fklIsVMvalueCgMacroScope(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->mt_ == &MacroScopeUserDataMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueCgMacroScope *as_macro_scope(
        const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCgMacroScope(r));
    return FKL_TYPE_CAST(FklVMvalueCgMacroScope *, r);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(macro_scope_print, "macro-scope");

static void macro_scope_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgMacroScope *ms = as_macro_scope(ud);
    fklVMgcToGray(FKL_VM_VAL(ms->prev), gc);
    fklVMgcToGray(FKL_VM_VAL(ms->macros), gc);
    fklVMgcToGray(FKL_VM_VAL(ms->replacements), gc);
}

static int macro_scope_finalize(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgMacroScope *ms = as_macro_scope(ud);
    ms->macros = NULL;
    ms->replacements = NULL;

    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const MacroScopeUserDataMetaTable = {
    .size = sizeof(FklVMvalueCgMacroScope),
    .princ = macro_scope_print,
    .prin1 = macro_scope_print,
    .atomic = macro_scope_atomic,
    .finalize = macro_scope_finalize,
};

FklVMvalueCgMacroScope *fklCreateVMvalueCgMacroScope(const FklCgCtx *c,
        FklVMvalueCgMacroScope *prev) {
    FKL_ASSERT(prev == NULL //
               || fklIsVMvalueCgMacroScope((FklVMvalue *)prev));
    FklVMvalueCgMacroScope *r =
            (FklVMvalueCgMacroScope *)fklCreateVMvalueUd(c->vm,
                    &MacroScopeUserDataMetaTable,
                    NULL);

    r->macros = fklCreateVMvalueCgMacroHashMap(c->vm);
    r->replacements = fklCreateVMvalueCgRplHashMap(c->vm);
    r->prev = prev;
    return r;
}

static FklVMudMetaTable const EnvUserDataMetaTable;

int fklIsVMvalueCgEnv(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &EnvUserDataMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueCgEnv *as_env(const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCgEnv(r));
    return FKL_TYPE_CAST(FklVMvalueCgEnv *, r);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(env_print, "env");

static FKL_ALWAYS_INLINE void mark_sym_def_hash_map(const FklSymDefHashMap *map,
        FklVMgc *gc) {
    for (const FklSymDefHashMapNode *cur = map->first; cur; cur = cur->next) {
        fklVMgcToGray(cur->k.sid, gc);
    }
}

static FKL_ALWAYS_INLINE void
mark_export_sid_map(const FklCgExportSidIdxHashMap *map, FklVMgc *gc) {
    for (const FklCgExportSidIdxHashMapNode *cur = map->first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->k, gc);
    }
}

static void env_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgEnv *e = as_env(ud);
    for (size_t i = 0; i < e->uref.size; ++i) {
        fklVMgcToGray(e->uref.base[i].sid, gc);
        fklVMgcToGray(e->uref.base[i].fid, gc);
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->uref.base[i].env), gc);
    }

    for (size_t i = 0; i < e->scopes.size; ++i) {
        const FklCgEnvScope *scope = &e->scopes.base[i];
        mark_sym_def_hash_map(&scope->defs, gc);
    }

    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->prev), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->macros), gc);

    fklVMgcToGray(e->filename, gc);
    fklVMgcToGray(e->name, gc);

    mark_sym_def_hash_map(&e->refs, gc);

    for (const FklPredefHashMapNode *cur = e->pdef.first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->k.sid, gc);
    }

    for (size_t i = 0; i < e->uref.size; ++i) {
        fklVMgcToGray(e->uref.base[i].fid, gc);
        fklVMgcToGray(e->uref.base[i].sid, gc);
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->uref.base[i].env), gc);
    }

    for (const FklValueIdHashMapNode *cur = e->konsts.ht.first; cur;
            cur = cur->next) {
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, cur->k), gc);
    }

    for (size_t i = 0; i < e->ref_pdef.size; ++i) {
        fklVMgcToGray(e->ref_pdef.base[i].sid, gc);
    }

    for (size_t i = 0; i < e->child_proc_protos.size; ++i) {
        fklVMgcToGray(e->child_proc_protos.base[i], gc);
    }

    for (const FklLibIdHashMapNode *cur = e->used_libraries.first; cur;
            cur = cur->next) {
        fklVMgcToGray(FKL_VM_VAL(cur->v.lib), gc);
    }

    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->proto), gc);
    fklVMgcToGray(FKL_VM_VAL(e->proto_env_map), gc);

    for (size_t i = 0; i < e->imported_symbols.size; ++i) {
        fklVMgcToGray(e->imported_symbols.base[i], gc);
    }
}

static int env_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgEnv *cur = as_env(ud);

    for (size_t i = 0; i < cur->scopes.size; ++i) {
        fklSymDefHashMapUninit(&cur->scopes.base[i].defs);
    }

    fklCgEnvScopeVectorUninit(&cur->scopes);
    fklCgEnvSlotVectorUninit(&cur->slots);

    fklSymDefHashMapUninit(&cur->refs);
    FklUnboundVector *unref = &cur->uref;
    fklUnboundVectorUninit(unref);

    fklPredefHashMapUninit(&cur->pdef);
    fklPreDefRefVectorUninit(&cur->ref_pdef);

    fklLibIdHashMapUninit(&cur->used_libraries);
    fklUninitValueTable(&cur->konsts);
    fklValueVectorUninit(&cur->child_proc_protos);
    fklValueVectorUninit(&cur->imported_symbols);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const EnvUserDataMetaTable = {
    .size = sizeof(FklVMvalueCgEnv),
    .princ = env_print,
    .prin1 = env_print,
    .atomic = env_atomic,
    .finalize = env_finalizer,
};

static inline void insert_proto_to_parent(FklVMvalueCgEnv *env) {
    FklVMvalueCgEnv *parent_env = env->prev;
    if (parent_env == NULL)
        return;

    FklValueVector *child_proc_protos = &parent_env->child_proc_protos;

    env->proto_id = child_proc_protos->size;
    fklValueVectorPushBack2(child_proc_protos, FKL_VM_NIL);
}

FklVMvalueCgEnv *fklCreateVMvalueCgEnv(const FklCgCtx *c,
        const FklCgEnvCreateArgs *args) {
    FKL_ASSERT(args);
    FklVMvalueCgEnv *prev_env = args->prev_env;
    FklVMvalueCgMacroScope *prev_ms = args->prev_ms;
    FKL_ASSERT((prev_env == NULL || fklIsVMvalueCgEnv((FklVMvalue *)prev_env)));
    FKL_ASSERT((prev_ms == NULL
                || fklIsVMvalueCgMacroScope((FklVMvalue *)prev_ms)));

    FklVMvalueCgEnv *r = (FklVMvalueCgEnv *)fklCreateVMvalueUd(c->vm,
            &EnvUserDataMetaTable,
            NULL);

    r->filename = args->filename;
    r->name = args->name;
    r->line = args->line;
    r->parent_scope = args->parent_scope;

    fklCgEnvScopeVectorInit(&r->scopes, 8);
    fklCgEnvSlotVectorInit(&r->slots, 8);
    enter_new_scope(0, r);
    r->proto_id = FKL_TOP_ENV_PROTO_ID;
    r->prev = prev_env;
    r->is_debugging = prev_env ? prev_env->is_debugging : 0;
    fklSymDefHashMapInit(&r->refs);
    fklUnboundVectorInit(&r->uref, 8);
    fklPredefHashMapInit(&r->pdef);
    fklPreDefRefVectorInit(&r->ref_pdef, 8);
    r->macros = fklCreateVMvalueCgMacroScope(c, prev_ms);
    fklInitValueTable(&r->konsts);
    fklValueVectorInit(&r->child_proc_protos, 4);
    fklLibIdHashMapInit(&r->used_libraries);

    r->proto_env_map = c->proto_env_map;
    insert_proto_to_parent(r);

    fklValueVectorInit(&r->imported_symbols, 0);
    return r;
}

FklLibId *fklVMvalueCgEnvAddUsedLib(FklVMvalueCgEnv *env,
        const char *rp,
        FklVMvalueLib *lib) {
    FKL_ASSERT(lib != NULL);
    FklLibId *id = &fklLibIdHashMapInsert(&env->used_libraries, &rp, NULL)->v;
    if (id->lib == NULL) {
        id->id = env->used_libraries.count - 1;
        id->lib = lib;
    }

    return id;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(info_print, "info");

static void *custom_action(FklProdActionArgs *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line);

static void *simple_action(FklProdActionArgs *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line);

static void *replace_action(FklProdActionArgs *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line);

static FklVMudMetaTable const InfoUserDataMetaTable;
int fklIsVMvalueCgInfo(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &InfoUserDataMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueCgInfo *as_info(const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCgInfo(r));
    return FKL_TYPE_CAST(FklVMvalueCgInfo *, r);
}

static void info_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgInfo *e = as_info(ud);
    fklVMgcToGray(FKL_VM_VAL(e->lnt), gc);
    fklVMgcToGray(FKL_VM_VAL(e->prev), gc);
    fklVMgcToGray(FKL_VM_VAL(e->global_env), gc);
    fklVMgcToGray(FKL_VM_VAL(e->libraries), gc);
    fklVMgcToGray(FKL_VM_VAL(e->g), gc);

    fklVMgcToGray(e->fid, gc);

    fklVMgcToGray(FKL_VM_VAL(e->export_replacement), gc);
    fklVMgcToGray(FKL_VM_VAL(e->export_macros), gc);
    fklVMgcToGray(FKL_VM_VAL(e->export_rmacros), gc);

    fklVMgcToGray(FKL_VM_VAL(e->rmacros), gc);

    mark_export_sid_map(&e->exports, gc);

    fklVMgcToGray(e->user_data, gc);

    fklVMgcToGray(e->re_exports, gc);
    fklVMgcToGray(e->last_re_export, gc);
}

static int info_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgInfo *i = as_info(ud);

    fklZfree(i->dir);
    if (i->filename)
        fklZfree(i->filename);
    if (i->realpath)
        fklZfree(i->realpath);

    fklCgExportSidIdxHashMapUninit(&i->exports);
    i->export_macros = NULL;
    i->export_replacement = NULL;

    i->export_rmacros = NULL;
    i->g = NULL;

    memset(i, 0, sizeof(FklVMvalueCgInfo));
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const InfoUserDataMetaTable = {
    .size = sizeof(FklVMvalueCgInfo),
    .princ = info_print,
    .prin1 = info_print,
    .atomic = info_atomic,
    .finalize = info_finalizer,
};

FklVMvalueCgInfo *fklCreateVMvalueCgInfo(FklCgCtx *ctx,
        FklVMvalueCgInfo *prev,
        const char *filename,
        const FklCgInfoArgs *args) {
    int is_lib = args == NULL ? 0 : args->is_lib;
    int is_macro = args == NULL ? 0 : args->is_macro;
    int is_main = args == NULL ? 0 : args->is_main;
    int is_debugging = args == NULL ? 0 : args->is_debugging;
    int is_precompile = args == NULL ? 0 : args->is_precompile;

    FklVMvalue *user_data = args == NULL ? NULL : args->user_data;

    FKL_ASSERT(prev == NULL || fklIsVMvalueCgInfo((FklVMvalue *)prev));

    if (((is_main && is_debugging) || is_precompile)
            && FKL_IS_NIL(ctx->proto_env_map))
        ctx->proto_env_map = fklCreateVMvalueCgEnvWeakMap(ctx->vm);

    FklVMvalueCgInfo *r = (FklVMvalueCgInfo *)fklCreateVMvalueUd(ctx->vm,
            &InfoUserDataMetaTable,
            NULL);

    FklVMvalueCgLibs *libs = args && args->libraries ? args->libraries
                           : is_macro                ? ctx->macro_libraries
                           : prev                    ? prev->libraries
                                                     : ctx->libraries;

    char *rp = filename ? fklRealpath(filename)
             : prev     ? fklZstrdup(prev->realpath)
                        : NULL;

    filename = filename ? filename       //
             : prev     ? prev->filename //
                        : NULL;

    r->user_data = user_data;
    r->re_exports = FKL_VM_NIL;
    r->last_re_export = FKL_VM_NIL;

    if (filename != NULL) {
        r->dir = fklDupDir(rp);
        r->filename = fklRelpath(ctx->main_file_real_path_dir, rp);
        r->realpath = rp;
        r->fid = add_symbol_cstr(ctx, r->filename);
    } else {
        r->dir = fklSysgetcwd();
        r->filename = NULL;
        r->realpath = NULL;
        r->fid = FKL_VM_NIL;
    }

    r->prev = prev;

    r->curline = filename == NULL && prev ? prev->curline : 1;

    r->lnt = ctx->lnt;

    r->exports.buckets = NULL;
    r->is_lib = is_lib;
    r->is_macro = is_macro;

    r->is_precompile = is_precompile;
    r->is_precompile |= prev && !prev->is_macro && prev->is_precompile;

    FklVM *vm = ctx->vm;
    r->export_macros = is_lib ? fklCreateVMvalueCgMacroHashMap(vm) : NULL;
    r->export_replacement = is_lib ? fklCreateVMvalueCgRplHashMap(vm) : NULL;
    r->export_rmacros = is_lib ? fklCreateVMvalueCgRmacroHashMap(vm) : NULL;
    if (is_lib) {
        fklCgExportSidIdxHashMapInit(&r->exports);
    } else {
        r->exports.buckets = NULL;
    }

    r->libraries = libs;

    if (args && args->inherit_grammer && prev) {
        r->g = prev->g;
        r->rmacros = prev->rmacros;
    } else {
        r->g = NULL;
        r->rmacros = NULL;
    }

    if (prev && !is_macro) {
        r->global_env = prev->global_env;
    } else {
        r->global_env = fklCreateVMvalueCgEnv(ctx,
                &(FklCgEnvCreateArgs){
                    .prev_env = NULL,
                    .prev_ms = is_macro ? args->macro_scope : NULL,
                    .parent_scope = 0,
                    .filename = r->fid,
                    .name = FKL_VM_NIL,
                    .line = r->curline,
                });
        r->global_env->is_debugging = is_debugging;
        fklInitGlobCgEnv(r->global_env, ctx->vm, is_precompile);
    }

    FklVMvalueCgEnv *main_env = NULL;
    if (is_main) {
        FklVMvalueCgMacroScope *macros = r->global_env->macros;

        main_env = fklCreateVMvalueCgEnv(ctx,
                &(FklCgEnvCreateArgs){
                    .prev_env = r->global_env,
                    .prev_ms = macros,
                    .parent_scope = 1,
                    .filename = r->fid,
                    .name = FKL_VM_NIL,
                    .line = r->curline,
                });
        ctx->main_env = main_env;
        ctx->main_info = r;
    }

    return r;
}

static const FklVMudMetaTable CustomActionCtxUdMetaTable;

static FKL_ALWAYS_INLINE FKL_UNUSED int is_custom_ctx(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->mt_ == &CustomActionCtxUdMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueCustomActCtx *as_custom_ctx(
        const FklVMvalue *v) {
    FKL_ASSERT(is_custom_ctx(v));
    return FKL_TYPE_CAST(FklVMvalueCustomActCtx *, v);
}

int fklIsVMvalueCustomActCtx(const FklVMvalue *v) { return is_custom_ctx(v); }

FKL_VM_USER_DATA_DEFAULT_PRINT(custom_action_ctx_ud_as_print,
        "custom-action-ctx")

static void custom_action_ctx_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCustomActCtx *c = as_custom_ctx(ud);
    if (c->proc == NULL)
        return;

    fklVMgcToGray(c->proc, gc);
    fklVMgcToGray(c->doller_s, gc);
    fklVMgcToGray(c->line_s, gc);
    for (size_t i = 0; i < c->actual_len; ++i) {
        fklVMgcToGray(c->dollers[i], gc);
    }
}

static const FklVMudMetaTable CustomActionCtxUdMetaTable = {
    .size = sizeof(FklVMvalueCustomActCtx),
    .atomic = custom_action_ctx_ud_atomic,
    .prin1 = custom_action_ctx_ud_as_print,
    .princ = custom_action_ctx_ud_as_print,
};

static void *custom_action(FklProdActionArgs *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMparseCtx *pctx = FKL_TYPE_CAST(FklVMparseCtx *, ctx);
    FklCgCtx *cg_ctx = pctx->opaque;
    FKL_ASSERT(cg_ctx);
    FklVMvalueCustomActCtx *action_ctx = (FklVMvalueCustomActCtx *)c;
    FklVMvalue *nodes_vector = fklCreateVMvalueVec(cg_ctx->vm, line);
    for (size_t i = 0; i < num; i++)
        FKL_VM_VEC(nodes_vector)->base[i] = nodes[i].ast;

    FklPmatchHashMap ht;
    fklPmatchHashMapInit(&ht);
    FklPmatchStorage storage = { .ht = &ht };
    fklPushCgPmatchStorage(cg_ctx, &storage);

    FklVMvalue *line_node = fklMakeVMintU(cg_ctx->vm, line);

    put_line_number(pctx->ln, nodes_vector, line);
    for (size_t i = 0; i < num; ++i) {
        fklPmatchHashMapAdd2(&ht,
                action_ctx->dollers[i],
                (FklPmatchRes){
                    .value = nodes[i].ast,
                    .container = nodes_vector,
                });
    }

    fklPmatchHashMapAdd2(&ht,
            action_ctx->doller_s,
            (FklPmatchRes){
                .value = nodes_vector,
                .container = nodes_vector,
            });

    fklPmatchHashMapAdd2(&ht,
            action_ctx->line_s,
            (FklPmatchRes){
                .value = line_node,
                .container = nodes_vector,
            });

    FklVMvalue *r = NULL;

    int e = execute_macro_expand_procedure(cg_ctx,
            cg_ctx->cur_file_dir,
            action_ctx->proc,
            &ht,
            line,
            &r);

    (void)e;

    fklPopCgPmatchStorage(cg_ctx, &storage);
    fklPmatchHashMapUninit(&ht);
    return r;
}

static inline size_t compute_prod_actual_len(
        const FklVMvalueCgRmacroProd *prod) {
    size_t delim_len = 0;
    for (size_t i = 0; i < prod->len; ++i)
        if (prod->syms[i].type == FKL_TERM_IGNORE)
            ++delim_len;
    return prod->len - delim_len;
}

FklVMvalueCustomActCtx *fklCreateVMvalueCustomActCtx(FklVM *vm, size_t len) {
    FklVMvalueCustomActCtx *v;
    FklVMvalue *vv = fklCreateVMvalueUd2(vm,
            &CustomActionCtxUdMetaTable,
            len * sizeof(v->dollers[0]),
            NULL);

    v = fklVMvalueCustomActCtx(vv);
    return v;
}

FklVMvalueCustomActCtx *fklCreateCgRmacroCustomAction(FklCgCtx *cg_ctx,
        FklVMvalueCgRmacroProd *prod) {
    size_t actual_len = compute_prod_actual_len(prod);

    FklVMvalueCustomActCtx *v;
    v = fklCreateVMvalueCustomActCtx(cg_ctx->vm, actual_len);

    v->actual_len = actual_len;

    v->doller_s = add_symbol_cstr(cg_ctx, "$$");
    v->line_s = add_symbol_cstr(cg_ctx, "$$");

    FklStrBuf buf = { 0 };
    fklInitStrBuf(&buf);
    for (size_t i = 0; i < actual_len; ++i) {
        fklStrBufPrintf(&buf, "$%zu", i);
        v->dollers[i] = add_symbol_char_buf(cg_ctx, buf.buf, buf.index);
        fklStrBufClear(&buf);
    }
    fklUninitStrBuf(&buf);

    return v;
}

static int simple_action_nth_check(FklVMvalue *rest[], size_t rest_len) {
    if (rest_len != 1 || !FKL_IS_FIX(rest[0]) || FKL_GET_FIX(rest[0]) < 0) {
        return 1;
    }

    return 0;
}

static inline uint64_t get_nth(FklVMvalue *vec) {
    FKL_ASSERT(FKL_IS_VECTOR(vec) && FKL_VM_VEC(vec)->size == 2);
    FKL_ASSERT(FKL_IS_FIX(FKL_VM_VEC(vec)->base[1]));
    return FKL_GET_FIX(FKL_VM_VEC(vec)->base[1]);
}

static void *simple_action_nth(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth((FklVMvalue *)action_ctx);
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

static inline void get_car_cdr(FklVMvalue *vec, uint64_t *car, uint64_t *cdr) {
    FKL_ASSERT(FKL_IS_VECTOR(vec) && FKL_VM_VEC(vec)->size == 3);
    FKL_ASSERT(FKL_IS_FIX(FKL_VM_VEC(vec)->base[1]));
    FKL_ASSERT(FKL_IS_FIX(FKL_VM_VEC(vec)->base[2]));
    *car = FKL_GET_FIX(FKL_VM_VEC(vec)->base[1]);
    *cdr = FKL_GET_FIX(FKL_VM_VEC(vec)->base[2]);
}

static void *simple_action_cons(FklProdActionArgs *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t car = 0;
    uint64_t cdr = 0;
    get_car_cdr((FklVMvalue *)c, &car, &cdr);

    if (car >= num || cdr >= num)
        return NULL;
    FklVMparseCtx *ct = ctx;
    FklVMvalue *retval =
            fklCreateVMvaluePair(ct->exe, nodes[car].ast, nodes[cdr].ast);
    put_line_number(ct->ln, retval, line);
    return retval;
}

static void *simple_action_head(FklProdActionArgs *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMvalue *vec = FKL_TYPE_CAST(FklVMvalue *, c);
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
        *pr = fklCreateVMvaluePair1(ct->exe, s->ast);
        put_line_number(ct->ln, *pr, s->line);
        pr = &FKL_VM_CDR(*pr);
    }

    r = fklCreateVMvaluePair(ct->exe, head, r);
    put_line_number(ct->ln, r, line);
    return r;
}

static int simple_action_head_check(FklVMvalue *rest[], size_t rest_len) {
    if (rest_len < 2) {
        return 1;
    }

    for (size_t i = 1; i < rest_len; i++)
        if (!FKL_IS_FIX(rest[i]) || FKL_GET_FIX(rest[i]) < 0)
            return 1;

    return 0;
}

static void *simple_action_list(FklProdActionArgs *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMvalue *vec = FKL_TYPE_CAST(FklVMvalue *, c);
    FKL_ASSERT(FKL_IS_VECTOR(vec));

    for (size_t i = 1; i < FKL_VM_VEC(vec)->size; i++) {
        FklVMvalue *c = FKL_VM_VEC(vec)->base[i];
        FKL_ASSERT(FKL_IS_FIX(c) && FKL_GET_FIX(c) >= 0);
        uint64_t idx = FKL_GET_FIX(c);
        if (idx >= num)
            return NULL;
    }
    FklVMparseCtx *ct = ctx;
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **pr = &r;
    for (size_t i = 1; i < FKL_VM_VEC(vec)->size; ++i) {
        FklVMvalue *c = FKL_VM_VEC(vec)->base[i];
        uint64_t idx = FKL_GET_FIX(c);

        const FklAnalysisSymbol *s = &nodes[idx];
        *pr = fklCreateVMvaluePair1(ct->exe, s->ast);
        put_line_number(ct->ln, *pr, s->line);
        pr = &FKL_VM_CDR(*pr);
    }

    put_line_number(ct->ln, r, line);
    return r;
}

static int simple_action_list_check(FklVMvalue *rest[], size_t rest_len) {
    if (rest_len < 1) {
        return 1;
    }

    for (size_t i = 0; i < rest_len; i++)
        if (!FKL_IS_FIX(rest[i]) || FKL_GET_FIX(rest[i]) < 0)
            return 1;

    return 0;
}

static inline void *simple_action_box(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth((FklVMvalue *)action_ctx);

    if (nth >= num)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *box = fklCreateVMvalueBox(c->exe, nodes[nth].ast);
    put_line_number(c->ln, box, line);
    return box;
}

static inline void *simple_action_vector(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth((FklVMvalue *)action_ctx);

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
}

static inline void *simple_action_hasheq(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth((FklVMvalue *)action_ctx);

    if (nth >= num)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *list = nodes[nth].ast;
    if (!is_pair_list(list))
        return NULL;
    return codegen_create_hash(c, FKL_HASH_EQ, list, line);
}

static inline void *simple_action_hasheqv(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth((FklVMvalue *)action_ctx);

    if (nth >= num)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *list = nodes[nth].ast;
    if (!is_pair_list(list))
        return NULL;
    return codegen_create_hash(c, FKL_HASH_EQV, list, line);
}

static inline void *simple_action_hashequal(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = get_nth((FklVMvalue *)action_ctx);

    if (nth >= num)
        return NULL;
    FklVMparseCtx *c = ctx;
    FklVMvalue *list = nodes[nth].ast;
    if (!is_pair_list(list))
        return NULL;
    return codegen_create_hash(c, FKL_HASH_EQUAL, list, line);
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

static inline void *simple_action_bytes(FklProdActionArgs *actx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = 0;
    const FklString *start_str = NULL;
    const FklString *end_str = NULL;
    get_nth_start_end((FklVMvalue *)actx, &start_str, &end_str, &nth);

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

static void *simple_action_symbol(FklProdActionArgs *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = 0;
    const FklString *start_str = NULL;
    const FklString *end_str = NULL;
    get_nth_start_end((FklVMvalue *)c, &start_str, &end_str, &nth);
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

        FklStrBuf buffer;
        fklInitStrBuf(&buffer);
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
                fklStrBufBincpy(&buffer, s, size);
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
            fklStrBufBincpy(&buffer, cstr, len);
            cstr += len;
            cstr_size -= len;
        }
        sym = fklVMaddSymbolCharBuf(ct->exe, buffer.buf, buffer.index);
        fklUninitStrBuf(&buffer);
    } else {
        sym = fklVMaddSymbol(ct->exe, FKL_VM_STR(node));
    }
    return sym;
}

static inline void *simple_action_string(FklProdActionArgs *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uint64_t nth = 0;
    const FklString *start_str = NULL;
    const FklString *end_str = NULL;
    get_nth_start_end((FklVMvalue *)c, &start_str, &end_str, &nth);
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
}

static struct FklSimpleProdAction
        CgProdCreatorActions[FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM] = {
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
                "list",
                .func = simple_action_list,
                .check = simple_action_list_check,
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
                .func = simple_action_bytes,
                .check = simple_action_symbol_check,
            },
        };

static void *simple_action(FklProdActionArgs *actx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMvalueSimpleActCtx *cc = (FklVMvalueSimpleActCtx *)actx;
    return cc->mt->func((void *)cc->vec, ctx, nodes, num, line);
}

static inline void init_simple_prod_action_list(FklCgCtx *ctx) {
    FklVMvalue **const simple_prod_action_id = ctx->simple_prod_action_id;
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++)
        simple_prod_action_id[i] =
                add_symbol_cstr(ctx, CgProdCreatorActions[i].name);
}

static void *replace_action(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMvalue *v = (FklVMvalue *)action_ctx;
    return v;
}

static void *builtin_prod_action_nil(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {

    return FKL_VM_NIL;
}

static void *builtin_prod_action_first(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 1)
        return NULL;
    return nodes[0].ast;
}

static void *builtin_prod_action_symbol(FklProdActionArgs *action_ctx,
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
}

static void *builtin_prod_action_second(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;

    return nodes[1].ast;
}

static void *builtin_prod_action_third(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 3)
        return NULL;

    return nodes[2].ast;
}

static inline void *builtin_prod_action_pair(FklProdActionArgs *action_ctx,
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
}

static inline void *builtin_prod_action_cons(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMparseCtx *c = ctx;
    if (num == 1) {
        FklVMvalue *car = nodes[0].ast;
        FklVMvalue *pair = fklCreateVMvaluePair1(c->exe, car);
        put_line_number(c->ln, pair, line);
        return pair;
    } else if (num == 2) {
        FklVMvalue *car = nodes[0].ast;
        FklVMvalue *cdr = nodes[1].ast;
        FklVMvalue *pair = fklCreateVMvaluePair(c->exe, car, cdr);
        put_line_number(c->ln, pair, line);
        return pair;
    } else
        return NULL;
}

static inline void *builtin_prod_action_box(FklProdActionArgs *action_ctx,
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
}

static inline void *builtin_prod_action_vector(FklProdActionArgs *action_ctx,
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
    return create_list(s_exps, 2, line, c->exe, c->ln);
}

static inline void *builtin_prod_action_quote(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMparseCtx *c = ctx;
    return add_header(c, nodes, "quote", line);
}

static inline void *builtin_prod_action_unquote(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMparseCtx *c = ctx;
    return add_header(c, nodes, "unquote", line);
}

static inline void *builtin_prod_action_qsquote(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMparseCtx *c = ctx;
    return add_header(c, nodes, "qsquote", line);
}

static inline void *builtin_prod_action_unqtesp(FklProdActionArgs *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklVMparseCtx *c = ctx;
    return add_header(c, nodes, "unqtesp", line);
}

static inline void *builtin_prod_action_hasheq(FklProdActionArgs *action_ctx,
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
}

static inline void *builtin_prod_action_hasheqv(FklProdActionArgs *action_ctx,
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
}

static inline void *builtin_prod_action_hashequal(FklProdActionArgs *action_ctx,
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
}

static inline void *builtin_prod_action_bytes(FklProdActionArgs *action_ctx,
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
    {"bytes",     builtin_prod_action_bytes },
    // clang-format on
};

static inline void init_builtin_prod_action_list(FklCgCtx *ctx) {
    FklVMvalue **const builtin_prod_action_id = ctx->builtin_prod_action_id;
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++)
        builtin_prod_action_id[i] =
                add_symbol_cstr(ctx, BuiltinProdActions[i].name);
}

void fklInitProdActionList(FklCgCtx *ctx) {
    init_builtin_prod_action_list(ctx);
    init_simple_prod_action_list(ctx);
}

static inline FklProdActionFunc find_builtin_prod_action(const FklCgCtx *ctx,
        const FklVMvalue *id) {
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++) {
        if (ctx->builtin_prod_action_id[i] == id)
            return BuiltinProdActions[i].func;
    }
    return NULL;
}

int fklIsCgRmacroBuiltinActionValid(const FklCgCtx *ctx, const FklVMvalue *id) {
    FklProdActionFunc act = find_builtin_prod_action(ctx, id);
    return act != NULL;
}

static inline const struct FklSimpleProdAction *find_simple_prod_action(
        FklVMvalue *id,
        FklVMvalue *const simple_prod_action_id[]) {
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++) {
        if (simple_prod_action_id[i] == id)
            return &CgProdCreatorActions[i];
    }
    return NULL;
}

static const FklVMudMetaTable SimpleActionCtxUdMetaTable;
static FKL_ALWAYS_INLINE FKL_UNUSED int is_simple_ctx(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->mt_ == &SimpleActionCtxUdMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueSimpleActCtx *as_simple_ctx(
        const FklVMvalue *v) {
    FKL_ASSERT(is_simple_ctx(v));
    return FKL_TYPE_CAST(FklVMvalueSimpleActCtx *, v);
}

int fklIsVMvalueSimpleActCtx(const FklVMvalue *v) { return is_simple_ctx(v); }

static void simple_action_ctx_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueSimpleActCtx *c = as_simple_ctx(ud);
    fklVMgcToGray(c->vec, gc);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(simple_action_ctx_ud_as_print,
        "simple-action-ctx")

static const FklVMudMetaTable SimpleActionCtxUdMetaTable = {
    .size = sizeof(FklVMvalueSimpleActCtx),
    .atomic = simple_action_ctx_ud_atomic,
    .prin1 = simple_action_ctx_ud_as_print,
    .princ = simple_action_ctx_ud_as_print,
};

FklVMvalueSimpleActCtx *fklCreateVMvalueSimpleActCtx(FklVM *vm,
        FklVMvalue *act) {
    FKL_ASSERT(FKL_IS_VECTOR(act));
    FklVMvalue *vv = fklCreateVMvalueUd(vm, &SimpleActionCtxUdMetaTable, NULL);

    FklVMvalueSimpleActCtx *v = (FklVMvalueSimpleActCtx *)vv;
    v->vec = act;
    return v;
}

FklVMvalueSimpleActCtx *fklCreateVMvalueSimpleActCtx1(const FklCgCtx *cg_ctx,
        FklVMvalue *act) {
    FklVM *vm = cg_ctx->vm;
    FklVMvalue *sym = FKL_VM_VEC(act)->base[0];
    FklVMvalue *const *ids = cg_ctx->simple_prod_action_id;
    const FklSimpleProdAction *mt = find_simple_prod_action(sym, ids);
    if (mt == NULL)
        return NULL;

    FklVMvalue **rest = &FKL_VM_VEC(act)->base[1];
    size_t rest_len = FKL_VM_VEC(act)->size - 1;

    FklVMvalueSimpleActCtx *v = fklCreateVMvalueSimpleActCtx(vm, act);

    int r = mt->check(rest, rest_len);
    if (r != 0) {
        return NULL;
    }
    v->mt = mt;

    return v;
}

static FklGrammerProduction *create_extra_start_prod(const FklCgCtx *ctx,
        FklVMvalue *sid) {
    FklGrammerProduction *prod;
    prod = fklCreateEmptyProduction(ctx->builtin_g.start,
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
    u->nt = sid;
    return prod;
}

static FklVMudMetaTable const CgLibsUserDataMetaTable;

int fklIsVMvalueCgLibs(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &CgLibsUserDataMetaTable;
}

FklVMvalueCgLibs *fklCreateVMvalueCgLibs(FklVM *vm) {
    FklVMvalue *r = fklCreateVMvalueHashEq(vm);
    return FKL_TYPE_CAST(FklVMvalueCgLibs *, r);
}

FklVMvalueCgLib *fklVMvalueCgLibsGet(const FklCgCtx *c,
        const FklVMvalueCgLibs *libs,
        const char *rp) {
    FklVMvalue *rp_s = fklVMaddSymbolCstr(c->vm, rp);
    return fklVMvalueCgLibsGet1(libs, rp_s);
}

FklVMvalueCgLib *fklVMvalueCgLibsGet1(const FklVMvalueCgLibs *libs,
        FklVMvalue *rp_s) {
    FklValueHashMapElm *e = fklVMhashTableGet(libs, rp_s);
    return e == NULL ? NULL : fklVMvalueCgLib(e->v);
}

FklVMvalueCgLib *
fklVMvalueCgLibsAdd1(FklVM *vm, FklVMvalueCgLibs *libs, FklVMvalue *rp_s) {
    FklValueHashMapElm *e = fklVMhashTableRef1(libs, rp_s, NULL);
    if (e->v == NULL) {
        FklVMvalueCgLib *l = fklCreateVMvalueCgLib(vm, rp_s);
        e->v = FKL_VM_VAL(l);
    }

    return fklVMvalueCgLib(e->v);
}

FklVMvalueCgLib *fklVMvalueCgLibsAdd2(FklVMvalueCgLibs *libs,
        FklVMvalue *rp_s,
        FklVMvalueCgLib *l) {
    fklVMhashTableSet(libs, rp_s, FKL_VM_VAL(l));
    return l;
}

FklVMvalueCgLib *
fklVMvalueCgLibsAdd(FklCgCtx *c, FklVMvalueCgLibs *libs, const char *rp) {
    FklVMvalue *rp_s = fklVMaddSymbolCstr(c->vm, rp);
    return fklVMvalueCgLibsAdd1(c->vm, libs, rp_s);
}

void fklVMvalueCgLibsRemove(FklCgCtx *c,
        FklVMvalueCgLibs *libs,
        const char *rp) {
    FklVMvalue *rp_s = fklVMaddSymbolCstr(c->vm, rp);

    fklVMhashTableDel(libs, rp_s, NULL, NULL);
}

const char *fklCgLibRp(const FklVMvalueCgLib *c) {
    if (c == NULL)
        return NULL;
    FKL_ASSERT(c->rp);
    return FKL_VM_SYM(c->rp)->str;
}

FklVMvalueProto *fklCreateVMvalueProto3(FklVM *exe,
        FklVMvalueCgEnv *env,
        const FklResolveRefArgs *args) {
    FKL_ASSERT(env->pdef.count == 0);
    fklResolveRef(env, 1, args);

    FklVMvalueWeakHashEq *tmp_var_refs = args ? args->weak_refs : NULL;
    uint32_t ref_count = (tmp_var_refs == NULL ? 0 : tmp_var_refs->ht.count)
                       + env->refs.count;

    uint32_t ref_offset = 0; // 固定等于 0

    uint32_t local_count = env->slots.size;

    uint32_t konsts_count = env->konsts.ht.count;

    uint32_t konsts_offset = (ref_count * FKL_VAR_REF_DEF_MEMBER_COUNT) //
                           + ref_offset;

    uint32_t child_proto_count = env->child_proc_protos.size;

    uint32_t child_proto_offset = konsts_offset + konsts_count;

    uint32_t used_libraries_count = env->used_libraries.count;

    uint32_t used_libraries_offset = child_proto_offset + child_proto_count;

    uint32_t total_val_count = (ref_count * FKL_VAR_REF_DEF_MEMBER_COUNT) //
                             + konsts_count                               //
                             + child_proto_count                          //
                             + used_libraries_count;

    FklVMvalueProto *proto = fklCreateVMvalueProto(exe, total_val_count);

    proto->name = env->name;
    proto->file = env->filename;
    proto->line = env->line;
    proto->local_count = local_count;

    proto->ref_count = ref_count;
    proto->ref_offset = ref_offset;

    proto->konsts_count = konsts_count;
    proto->konsts_offset = konsts_offset;

    proto->child_proto_count = child_proto_count;
    proto->child_proto_offset = child_proto_offset;

    proto->used_libraries_count = used_libraries_count;
    proto->used_libraries_offset = used_libraries_offset;

    FklVMvalue **const vals = proto->vals;

    FklVarRefDef *const refs = (FklVarRefDef *)&vals[proto->ref_offset];

    for (const FklSymDefHashMapNode *l = env->refs.first; l; l = l->next) {
        FklVMvalue *sid = NULL;
        if (env->is_debugging || l->v.cidx == FKL_VAR_REF_INVALID_CIDX) {
            sid = l->k.sid;
        }

        FklVarRefDef *cur = &refs[l->v.idx];
        cur->sid = sid;
        cur->cidx = FKL_MAKE_VM_FIX(l->v.cidx);
        cur->is_local = l->v.isLocal ? FKL_VM_TRUE : FKL_VM_NIL;
    }

    FklVMvalue **const konsts = &vals[konsts_offset];

    for (const FklValueIdHashMapNode *cur = env->konsts.ht.first; cur;
            cur = cur->next) {
        konsts[cur->v - 1] = FKL_TYPE_CAST(FklVMvalue *, cur->k);
    }

    FklVMvalue **const protos = &vals[child_proto_offset];

    for (size_t i = 0; i < proto->child_proto_count; ++i) {
        protos[i] = env->child_proc_protos.base[i];
    }

    FklVMvalue **const libs = &vals[used_libraries_offset];

    for (const FklLibIdHashMapNode *cur = env->used_libraries.first; cur;
            cur = cur->next) {
        FKL_ASSERT(cur->v.lib);
        libs[cur->v.id] = FKL_VM_VAL(cur->v.lib);
    }

    update_parent_env_proto(env, FKL_VM_VAL(proto));

    env->proto = proto;

    if (!FKL_IS_NIL(env->proto_env_map))
        fklVMvalueCgEnvWeakMapInsert(env->proto_env_map, proto, env);

    if (tmp_var_refs == NULL)
        return proto;

    uint32_t i = env->refs.count;
    for (const FklValueEqHashMapNode *cur = tmp_var_refs->ht.first; cur;
            cur = cur->next) {
        FklVarRefDef *c_ref = &refs[i];
        c_ref->sid = cur->k;
        c_ref->cidx = FKL_MAKE_VM_FIX(FKL_VAR_REF_INVALID_CIDX);
        c_ref->is_local = cur->v;
        ++i;
    }

    return proto;
}

FklVMvalueProto *fklCreateVMvalueProto2(FklVM *exe, FklVMvalueCgEnv *env) {
    return fklCreateVMvalueProto3(exe, env, NULL);
}

FklVMvalueCgEnvWeakMap *fklCreateVMvalueCgEnvWeakMap(FklVM *vm) {
    FklVMvalueWeakHashEq *m = fklCreateVMvalueWeakHashEq2(vm, FKL_WEAK_MAP_K);
    return (FklVMvalueCgEnvWeakMap *)m;
}

static FKL_ALWAYS_INLINE FklVMvalueWeakHashEq *as_weak_map(
        const FklVMvalueCgEnvWeakMap *hp) {
    FKL_ASSERT(fklIsVMvalueWeakHashEq(FKL_VM_VAL(hp)));
    return (FklVMvalueWeakHashEq *)hp;
}

FklVMvalueCgEnv *fklVMvalueCgEnvWeakMapGet(const FklVMvalueCgEnvWeakMap *hp,
        const FklVMvalueProto *p) {
    FklVMvalue **pv = fklVMvalueWeakHashEqGet(as_weak_map(hp), (FklVMvalue *)p);
    if (pv == NULL)
        return NULL;
    return as_env(*pv);
}

void fklVMvalueCgEnvWeakMapInsert(FklVMvalueCgEnvWeakMap *hp,
        const FklVMvalueProto *k,
        const FklVMvalueCgEnv *v) {
    FklValueEqHashMapElm *elm = fklVMvalueWeakHashEqInsert(as_weak_map(hp), //
            (FklVMvalue *)k);
    elm->v = (FklVMvalue *)v;
}

static inline char *get_suffix_pos(char *path) {
    char *r = NULL;

    r = fklStrEndWith(path, FKL_PATH_SEPARATOR_STR "main.fklp");
    if (r != NULL)
        return r;

    r = fklStrEndWith(path, FKL_PATH_SEPARATOR_STR "main.fkl");
    if (r != NULL)
        return r;

    r = fklStrEndWith(path, FKL_SCRIPT_FILE_EXTENSION);
    if (r != NULL)
        return r;

    r = fklStrEndWith(path, FKL_DLL_FILE_EXTENSION);
    if (r != NULL)
        return r;

    return NULL;
}

static inline char *path_to_module_name(char *path) {
    FKL_ASSERT(path != NULL);
    char *pos = get_suffix_pos(path);
    if (pos != NULL) {
        *pos = '\0';
    }

#ifdef _WIN32
    for (char *cur = path; cur < pos; ++cur) {
        if (*cur == '\\')
            *cur = '/';
    }
#endif

    return path;
}

static inline FklVMvalue *
realpath_to_module_name(FklVM *vm, const char *main_dir, const char *rp) {
    char *mod_path_cstr = fklRelpath(main_dir, rp);
    if (mod_path_cstr == NULL)
        mod_path_cstr = fklZstrdup(rp);
    path_to_module_name(mod_path_cstr);
    FklVMvalue *module_name = fklVMaddSymbolCstr(vm, mod_path_cstr);
    fklZfree(mod_path_cstr);
    return module_name;
}

FklVMvalue *fklCgRealpathToModuleName(FklCgCtx *ctx, const char *rp) {
    return realpath_to_module_name(ctx->vm, ctx->main_file_real_path_dir, rp);
}

static FklVMudMetaTable const MacroUserDataMetaTable;
int fklIsVMvalueCgMacro(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &MacroUserDataMetaTable;
}

FklVMvalueCgMacro *fklVMvalueCgMacro(const FklVMvalue *r) {
    return as_macro(r);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(macro_print, "macro");

static void macro_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgMacro *m = as_macro(ud);
    fklVMgcToGray(m->pattern, gc);
    fklVMgcToGray(m->proc, gc);
}

static FklVMudMetaTable const MacroUserDataMetaTable = {
    .size = sizeof(FklVMvalueCgMacro),
    .princ = macro_print,
    .prin1 = macro_print,
    .atomic = macro_atomic,
};

FklVMvalueCgMacro *fklCreateVMvalueCgMacro(const FklCgCtx *c,
        FklVMvalue *pattern,
        FklVMvalue *proc) {
    FKL_ASSERT(FKL_IS_PROC(proc));
    FklVMvalueCgMacro *r = (FklVMvalueCgMacro *)fklCreateVMvalueUd(c->vm,
            &MacroUserDataMetaTable,
            NULL);
    r->pattern = pattern;
    r->proc = proc;
    return r;
}

FklVMvalueCgMacroHashMap *fklCreateVMvalueCgMacroHashMap(FklVM *vm) {
    return FKL_VM_HASH(fklCreateVMvalueHashEq(vm));
}

FklValueHashMapElm *fklCgMacroHashMapGet(const FklVMvalueCgMacroHashMap *map,
        const FklVMvalue *s) {
    return fklVMhashTableGet(map, FKL_VM_VAL(s));
}

FklValueHashMapElm *fklCgMacroHashMapRef1(FklVMvalueCgMacroHashMap *map,
        const FklVMvalue *s) {
    return fklVMhashTableRef1(map, FKL_VM_VAL(s), FKL_VM_NIL);
}

void fklCgMacroHashMapDel(FklVMvalueCgMacroHashMap *map, FklVMvalue *s) {
    FklVMvalue *v = NULL;
    fklVMhashTableDel(map, s, &v, NULL);
}

static FklVMudMetaTable const RplUserDataMetaTable;
int fklIsVMvalueCgRpl(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &RplUserDataMetaTable;
}

static inline FklVMvalueCgRpl *as_rpl(const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCgRpl(r));
    return FKL_TYPE_CAST(FklVMvalueCgRpl *, r);
}

FklVMvalueCgRpl *fklVMvalueCgRpl(const FklVMvalue *r) { return as_rpl(r); }

FKL_VM_USER_DATA_DEFAULT_PRINT(rpl_print, "rpl");

static void rpl_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgRpl *m = as_rpl(ud);
    fklVMgcToGray(m->value, gc);
}

static FklVMudMetaTable const RplUserDataMetaTable = {
    .size = sizeof(FklVMvalueCgRpl),
    .princ = rpl_print,
    .prin1 = rpl_print,
    .atomic = rpl_atomic,
};

FklVMvalueCgRpl *fklCreateVMvalueCgRpl(const FklCgCtx *c, FklVMvalue *value) {
    FklVMvalueCgRpl *r = (FklVMvalueCgRpl *)fklCreateVMvalueUd(c->vm,
            &RplUserDataMetaTable,
            NULL);
    r->value = value;
    return r;
}

FklVMvalueCgRplHashMap *fklCreateVMvalueCgRplHashMap(FklVM *vm) {
    return FKL_VM_HASH(fklCreateVMvalueHashEq(vm));
}

FklVMvalueCgRpl *fklCgRplHashMapGet(const FklVMvalueCgRplHashMap *map,
        const FklVMvalue *s) {
    FklValueHashMapElm *elm = fklVMhashTableGet(map, FKL_VM_VAL(s));
    return elm ? fklVMvalueCgRpl(elm->v) : NULL;
}

FklVMvalueCgRpl *fklCgRplHashMapDel(FklVMvalueCgRplHashMap *map,
        FklVMvalue *s) {
    FklVMvalue *v = NULL;
    fklVMhashTableDel(map, s, &v, NULL);
    return fklVMvalueCgRpl(v);
}

void fklCgRplHashMapSet(FklVMvalueCgRplHashMap *map,
        const FklVMvalue *sym,
        FklVMvalueCgRpl *rep) {
    fklVMhashTableSet(map, FKL_VM_VAL(sym), FKL_VM_VAL(rep));
}

static FklVMudMetaTable const RmacroUserDataMetaTable;
int fklIsVMvalueCgRmacro(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &RmacroUserDataMetaTable;
}

static inline FklVMvalueCgRmacro *as_reader_macro(const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCgRmacro(r));
    return FKL_TYPE_CAST(FklVMvalueCgRmacro *, r);
}

FklVMvalueCgRmacro *fklVMvalueCgRmacro(const FklVMvalue *r) {
    return as_reader_macro(r);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(reader_macro_print, "reader-macro");

static void reader_macro_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgRmacro *m = as_reader_macro(ud);
    for (uint64_t i = 0; i < m->len; ++i) {
        FklVMvalue *v = m->cmds[i].args;
        fklVMgcToGray(v, gc);
    }
}

static FklVMudMetaTable const RmacroUserDataMetaTable = {
    .size = sizeof(FklVMvalueCgRmacro),
    .princ = reader_macro_print,
    .prin1 = reader_macro_print,
    .atomic = reader_macro_atomic,
};

FklVMvalueCgRmacro *fklCreateVMvalueCgRmacro(FklVM *vm, uint64_t len) {
    size_t cmds_size = len * sizeof(FklCgRmacroCmd);
    FklVMvalue *v = NULL;
    v = fklCreateVMvalueUd2(vm, &RmacroUserDataMetaTable, cmds_size, NULL);
    FklVMvalueCgRmacro *r = as_reader_macro(v);
    r->len = len;
    return r;
}

static FklVMudMetaTable const GrammerUserDataMetaTable;
int fklIsVMvalueCgGrammer(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &GrammerUserDataMetaTable;
}

static inline FklVMvalueCgGrammer *as_grammer(const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCgGrammer(r));
    return FKL_TYPE_CAST(FklVMvalueCgGrammer *, r);
}

FklVMvalueCgGrammer *fklVMvalueCgGrammer(const FklVMvalue *r) {
    return as_grammer(r);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(grammer_print, "grammer");

static void grammer_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgGrammer *m = as_grammer(ud);
    fklVMgcMarkGrammer(gc, &m->g, NULL);
}

static int grammer_finanlize(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgGrammer *g = as_grammer(ud);
    fklUninitGrammer(&g->g);

    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const GrammerUserDataMetaTable = {
    .size = sizeof(FklVMvalueCgGrammer),
    .princ = grammer_print,
    .prin1 = grammer_print,
    .atomic = grammer_atomic,
    .finalize = grammer_finanlize,
};

FklVMvalueCgGrammer *fklCreateVMvalueCgGrammer(const FklCgCtx *c) {
    FklVMvalueCgGrammer *r = (FklVMvalueCgGrammer *)fklCreateVMvalueUd(c->vm,
            &GrammerUserDataMetaTable,
            NULL);
    fklInitEmptyGrammer(&r->g, c->vm);
    return r;
}

FklVMvalueCgRmacroHashMap *fklCreateVMvalueCgRmacroHashMap(FklVM *vm) {
    return FKL_VM_HASH(fklCreateVMvalueHashEq(vm));
}

FklValueHashMapElm *fklCgRmacroHashMapGet(const FklVMvalueCgRmacroHashMap *map,
        const FklVMvalue *s) {
    return fklVMhashTableGet(map, FKL_VM_VAL(s));
}

FklValueHashMapElm *fklCgRmacroHashMapRef1(FklVMvalueCgRmacroHashMap *map,
        const FklVMvalue *s) {
    return fklVMhashTableRef1(map, FKL_VM_VAL(s), NULL);
}

FklVMvalueCgRmacro *fklCgRmacroHashMapDel(FklVMvalueCgRmacroHashMap *map,
        const FklVMvalue *s) {
    FklVMvalue *macro = NULL;
    int has = fklVMhashTableDel(map, FKL_VM_VAL(s), &macro, NULL);
    return has ? fklVMvalueCgRmacro(macro) : NULL;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(prod_print, "prod");

static FklVMudMetaTable const RmacroProdMt;
int fklIsVMvalueCgRmacroProd(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &RmacroProdMt;
}

static inline FklVMvalueCgRmacroProd *as_prod(const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCgRmacroProd(r));
    return FKL_TYPE_CAST(FklVMvalueCgRmacroProd *, r);
}

FklVMvalueCgRmacroProd *fklVMvalueCgRmacroProd(const FklVMvalue *r) {
    return as_prod(r);
}

static void prod_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgRmacroProd *m = as_prod(ud);
    fklVMgcToGray(m->left, gc);
    fklVMgcToGray(m->action_type, gc);
    fklVMgcToGray(m->action, gc);

    for (uint32_t i = 0; i < m->len; ++i) {
        FklVMvalue *v = m->syms[i].v;
        fklVMgcToGray(v, gc);
    }
}

static FklVMudMetaTable const RmacroProdMt = {
    .size = sizeof(FklVMvalueCgRmacroProd),
    .princ = prod_print,
    .prin1 = prod_print,
    .atomic = prod_atomic,
};

FklVMvalueCgRmacroProd *fklCreateVMvalueCgRmacroProd(FklVM *vm,
        FklVMvalue *left,
        FklVMvalue *action_type,
        FklVMvalue *action,
        int add_extra,
        uint32_t len) {
    size_t syms_size = len * sizeof(FklCgRmacroGraSym);
    FklVMvalue *v = fklCreateVMvalueUd2(vm, &RmacroProdMt, syms_size, NULL);
    FklVMvalueCgRmacroProd *r = as_prod(v);
    FKL_ASSERT(action != NULL);

    r->left = left;
    r->action_type = action_type;
    r->action = action;
    r->add_extra = add_extra;
    r->len = len;
    return r;
}

const char *fklGetCgRmacroOpName(FklCgRmacroOpcode op) {
    static const char *names[] = {
        [FKL_CG_RMACRO_NONE] = "none",
        [FKL_CG_RMACRO_ADD_PROD] = "add-prod",
        [FKL_CG_RMACRO_ADD_IGNORE] = "add-ignore",
        [FKL_CG_RMACRO_ADD_DELIM] = "add-delim",
    };

    return names[op];
}

typedef FklBuiltinTerminalInitError BtError;

static inline BtError builtin_terminal_init(const FklCgCtx *ctx,
        FklVMvalue *v,
        FklLalrBuiltinGrammerSym *out,
        FklGrammer *g) {
    BtError err = FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY;
    const FklLalrBuiltinMatch *bt = NULL;

    out->args = NULL;
    out->len = 0;

    if (FKL_IS_SYM(v)) {
        bt = fklGetBuiltinMatch(&g->builtins, v);
        out->t = bt;
        if (out->t->args_check) {
            err = out->t->args_check(out->len, out->args, g);
        }
    } else if (FKL_IS_VECTOR(v)) {
        FklVMvalue *first = FKL_VM_VEC(v)->base[0];
        bt = fklGetBuiltinMatch(&g->builtins, first);

        size_t arg_count = FKL_VM_VEC(v)->size - 1;
        size_t total_size = arg_count * sizeof(FklString *);
        FklString const **args = fklZmalloc(total_size);

        for (size_t i = 1; i < FKL_VM_VEC(v)->size; ++i) {
            const FklString *str_v = FKL_VM_STR(FKL_VM_VEC(v)->base[i]);
            args[i - 1] = fklAddString(&g->terminals, str_v);
        }

        out->t = bt;
        if (out->t->args_check)
            err = out->t->args_check(arg_count, args, g);
        if (err) {
            fklZfree(args);
            goto done;
        }

        out->len = arg_count;
        out->args = args;
    }

done:
    return err;
}

static inline FklVMvalue *
make_qualified_sym(FklVM *vm, FklVMvalue *a, FklVMvalue *b) {
    FklStrBuf buf = { 0 };
    fklInitStrBuf(&buf);
    fklStrBufConcatWithString(&buf, FKL_VM_SYM(a));
    fklStrBufConcatWithCstr(&buf, FKL_QUALIFIED_NONTERM_SEP_STR);
    fklStrBufConcatWithString(&buf, FKL_VM_SYM(b));

    FklVMvalue *v = fklVMaddSymbolCharBuf(vm, buf.buf, buf.index);
    fklUninitStrBuf(&buf);
    return v;
}

// return the pos of qualified seperator or start of symbol
static inline const char *is_valid_nonterm_sym(const FklVMvalue *sym) {
    FKL_ASSERT(FKL_IS_SYM(sym));
    FklString const *const s = FKL_VM_SYM(sym);
    const char *const start = s->str;
    const char *const end = &s->str[s->size];
    size_t len = s->size;

    const char *r = start;
    const char *pos = memchr(start, FKL_QUALIFIED_NONTERM_SEP, len);
    if (pos == NULL)
        return r;

    r = pos;
    if (pos == start)
        return NULL;

    if (pos == end)
        return NULL;

    ++pos;

    size_t offset = pos - start;
    const char *other = memchr(pos, FKL_QUALIFIED_NONTERM_SEP, len - offset);
    if (other != NULL)
        return NULL;

    return r;
}

static FKL_ALWAYS_INLINE int is_qualified_sym(const FklVMvalue *s) {
    const char *pos = is_valid_nonterm_sym(s);
    FKL_ASSERT(pos != NULL);
    return *pos == FKL_QUALIFIED_NONTERM_SEP;
}

static inline int rmacro_prod_sym_to_gra_sym(const FklCgCtx *ctx,
        FklVMvalue *name,
        const FklCgRmacroGraSym *in,
        FklGrammerSym *out,
        FklGrammer *g) {
    int err = 0;
    const FklGrammer *builtin_g = &ctx->builtin_g;
    switch (in->type) {
    case FKL_TERM_NONE:
    case FKL_TERM_EOF:
        FKL_UNREACHABLE();
        break;

    case FKL_TERM_BUILTIN: {
        FklVMvalue *v = in->v;
        FKL_ASSERT(FKL_IS_SYM(v) || FKL_IS_VECTOR(v));

        *out = (FklGrammerSym){ .type = FKL_TERM_BUILTIN };
        err = builtin_terminal_init(ctx, v, &out->b, g);
        FKL_ASSERT(err == FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY);
        break;
    }

    case FKL_TERM_KEYWORD: {
        FklVMvalue *v = in->v;
        FKL_ASSERT(FKL_IS_STR(v));

        const FklString *str = fklAddString(&g->terminals, FKL_VM_STR(v));
        *out = (FklGrammerSym){ .type = FKL_TERM_STRING, .str = str };
        break;
    }

    case FKL_TERM_REGEX: {
        FklVMvalue *v = in->v;
        FKL_ASSERT(FKL_IS_STR(v));

        const FklString *re_str = FKL_VM_STR(v);
        const FklRegexCode *re = fklAddRegexStr(&g->regexes, re_str);

        *out = (FklGrammerSym){ .type = FKL_TERM_REGEX, .re = re };
        break;
    }

    case FKL_TERM_STRING: {
        FklVMvalue *v = in->v;
        FKL_ASSERT(FKL_IS_STR(v));

        const FklString *str = fklAddString(&g->terminals, FKL_VM_STR(v));
        *out = (FklGrammerSym){ .type = FKL_TERM_STRING, .str = str };
        fklAddString(&g->delimiters, FKL_VM_STR(v));
        break;
    }

    case FKL_TERM_NONTERM: {
        FklVMvalue *v = in->v;
        if (FKL_IS_SYM(v)) {
            FklVMvalue *left;

            if (is_qualified_sym(v)) {
                left = v;
            } else if (fklIsNonterminalExist(builtin_g, v)) {
                left = v;
            } else {
                left = make_qualified_sym(ctx->vm, name, v);
            }

            *out = (FklGrammerSym){
                .type = FKL_TERM_NONTERM,
                .nt = left,
            };

        } else if (FKL_IS_PAIR(v)) {
            FKL_ASSERT(FKL_IS_SYM(FKL_VM_CAR(v)));
            FKL_ASSERT(FKL_IS_SYM(FKL_VM_CDR(v)));

            *out = (FklGrammerSym){
                .type = FKL_TERM_NONTERM,
                .nt = make_qualified_sym(ctx->vm, FKL_VM_CAR(v), FKL_VM_CDR(v)),
            };
        } else {
            FKL_UNREACHABLE();
        }
        break;
    }

    case FKL_TERM_IGNORE:
        FKL_ASSERT(in->v == NULL);
        *out = (FklGrammerSym){ .type = FKL_TERM_IGNORE };
        break;
    }

    return err;
}

enum ActionType {
    ACTION_TYPE_NONE = 0,
    ACTION_TYPE_BUILTIN,
    ACTION_TYPE_SIMPLE,
    ACTION_TYPE_CUSTOM,
    ACTION_TYPE_REPLACE,
};

typedef FklGrammerNonterm Nt;
static inline Nt get_rmacro_prod_left(FklCgCtx *ctx,
        FklVMvalue *name,
        FklVMvalueCgRmacroProd *in) {
    FKL_ASSERT(in->left != NULL);
    if (in->left == FKL_VM_NIL) {
        return ctx->builtin_g.start;
    }

    FklVMvalue *left = make_qualified_sym(ctx->vm, name, in->left);
    FKL_ASSERT(!fklIsNonterminalExist(&ctx->builtin_g, left));

    return left;
}

static inline FklGrammerProduction *create_builtin_act_prod(FklCgCtx *ctx,
        const Nt left,
        size_t len,
        FklVMvalue *id) {
    FklProdActionFunc action = id == NULL ? builtin_prod_action_first
                                          : find_builtin_prod_action(ctx, id);
    if (action == NULL) {
        return NULL;
    }

    return fklCreateProduction(left,
            len,
            NULL,
            NULL,
            action,
            id,
            fklProdCtxDestroyDoNothing,
            fklProdCtxCopyerDoNothing);
}

static inline FklGrammerProduction *create_simple_act_prod(FklCgCtx *ctx,
        const Nt left,
        size_t len,
        FklVMvalue *action) {
    return fklCreateProduction(left,
            len,
            NULL,
            NULL,
            simple_action,
            action,
            fklProdCtxDestroyDoNothing,
            fklProdCtxCopyerDoNothing);
}

static inline FklGrammerProduction *create_custom_act_prod(FklCgCtx *ctx,
        const Nt left,
        size_t len,
        FklVMvalue *action) {
    return fklCreateProduction(left,
            len,
            NULL,
            NULL,
            custom_action,
            action,
            fklProdCtxDestroyDoNothing,
            fklProdCtxCopyerDoNothing);
}

static inline FklGrammerProduction *create_replace_act_prod(FklCgCtx *ctx,
        const Nt left,
        size_t len,
        FklVMvalue *action) {
    return fklCreateProduction(left,
            len,
            NULL,
            NULL,
            replace_action,
            action,
            fklProdCtxDestroyDoNothing,
            fklProdCtxCopyerDoNothing);
}
static inline FklGrammerProduction *rmacro_prod_to_prod(FklCgCtx *ctx,
        FklGrammer *g,
        FklVMvalue *name,
        FklVMvalueCgRmacroProd *in) {
    FKL_ASSERT(in != NULL);
    FKL_ASSERT(in->action_type != NULL);
    FKL_ASSERT(in->action_type != FKL_VM_NIL);

    enum ActionType action_type = ACTION_TYPE_NONE;
    if (in->action_type == ctx->builtin_sym_builtin) {
        action_type = ACTION_TYPE_BUILTIN;
    } else if (in->action_type == ctx->builtin_sym_simple) {
        action_type = ACTION_TYPE_SIMPLE;
    } else if (in->action_type == ctx->builtin_sym_custom) {
        action_type = ACTION_TYPE_CUSTOM;
    } else if (in->action_type == ctx->builtin_sym_replace) {
        action_type = ACTION_TYPE_REPLACE;
    } else {
        FKL_UNREACHABLE();
        return NULL;
    }

    Nt left = get_rmacro_prod_left(ctx, name, in);

    FklGrammerProduction *prod = NULL;
    switch (action_type) {
    case ACTION_TYPE_NONE:
        return NULL;
        break;
    case ACTION_TYPE_BUILTIN:
        prod = create_builtin_act_prod(ctx, left, in->len, in->action);
        FKL_ASSERT(prod);
        break;
    case ACTION_TYPE_SIMPLE:
        prod = create_simple_act_prod(ctx, left, in->len, in->action);
        break;
    case ACTION_TYPE_CUSTOM:
        prod = create_custom_act_prod(ctx, left, in->len, in->action);
        break;
    case ACTION_TYPE_REPLACE:
        prod = create_replace_act_prod(ctx, left, in->len, in->action);
        break;
    }

    for (size_t i = 0; i < in->len; ++i) {
        const FklCgRmacroGraSym *from = &in->syms[i];
        FklGrammerSym *to = &prod->syms[i];
        int err = rmacro_prod_sym_to_gra_sym(ctx, name, from, to, g);

        if (err) {
            fklDestroyGrammerProduction(prod);
            return NULL;
        }
    }

    return prod;
}

static inline FklVMvalue *make_dup_prod_rule_error(FklVM *vm,
        const FklGrammer *g,
        const FklGrammerProduction *prod) {
    FKL_ASSERT(prod != NULL);

    FklStrBuf buf = { 0 };
    fklInitStrBuf(&buf);
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderStrBuf(&builder, &buf, NULL);
    fklPrintGrammerProduction(vm, prod, &g->regexes, &builder);

    FklVMvalue *str_v = fklCreateVMvalueStr2(vm, buf.index, buf.buf);

    FklVMvalue *err = FKL_MAKE_VM_ERR(FKL_ERR_GRAMMER_CREATE_FAILED, //
            vm,
            "duplicate production %s",
            str_v);

    fklUninitStrBuf(&buf);

    return err;
}

static inline int do_rmacro_add_prod(FklCgCtx *ctx,
        FklVMvalueCgInfo *info,
        FklVMvalue *name,
        FklVMvalue *args) {
    FKL_ASSERT(info->g != NULL);
    FklGrammer *g = &info->g->g;
    FklVM *vm = ctx->vm;
    FklCgErrorState *errors = ctx->error_state;
    FklVMvalueCgRmacroProd *prod = fklVMvalueCgRmacroProd(args);
    FklGrammerProduction *out_prod = rmacro_prod_to_prod(ctx, g, name, prod);
    if (out_prod == NULL) {
        // should not happen
        FKL_UNREACHABLE();
        abort();
    }

    if (fklAddProdToProdTableNoRepeat(g, out_prod)) {
        errors->error = make_dup_prod_rule_error(vm, g, out_prod);
        fklDestroyGrammerProduction(out_prod);

        errors->fid = info->fid;
        errors->line = info->curline;
        return 1;
    }

    if (prod->add_extra) {
        FklVMvalue *left = make_qualified_sym(vm, name, prod->left);
        out_prod = create_extra_start_prod(ctx, left);
        int r = fklAddProdToProdTable(g, out_prod);
        FKL_ASSERT(r == 0);
        (void)r;
    }
    return 0;
}

typedef FklGrammerIgnoreSym IgSym;

static inline int rmacro_prod_sym_to_ig_sym(const FklCgCtx *ctx,
        const FklCgRmacroGraSym *in,
        IgSym *out,
        FklGrammer *g) {
    int err = 0;
    switch (in->type) {
    case FKL_TERM_NONTERM:
    case FKL_TERM_NONE:
    case FKL_TERM_EOF:
    case FKL_TERM_KEYWORD:
    case FKL_TERM_IGNORE:
        FKL_UNREACHABLE();
        break;

    case FKL_TERM_BUILTIN: {
        FklVMvalue *v = in->v;
        FKL_ASSERT(FKL_IS_SYM(v) || FKL_IS_VECTOR(v));

        *out = (IgSym){ .term_type = FKL_TERM_BUILTIN };
        err = builtin_terminal_init(ctx, v, &out->b, g);
        FKL_ASSERT(err == FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY);
        break;
    }

    case FKL_TERM_REGEX: {
        FklVMvalue *v = in->v;
        FKL_ASSERT(FKL_IS_STR(v));

        const FklString *re_str = FKL_VM_STR(v);
        const FklRegexCode *re = fklAddRegexStr(&g->regexes, re_str);

        *out = (IgSym){ .term_type = FKL_TERM_REGEX, .re = re };
        break;
    }

    case FKL_TERM_STRING: {
        FklVMvalue *v = in->v;
        FKL_ASSERT(FKL_IS_STR(v));

        const FklString *str = fklAddString(&g->terminals, FKL_VM_STR(v));
        *out = (IgSym){ .term_type = FKL_TERM_STRING, .str = str };
        fklAddString(&g->delimiters, FKL_VM_STR(v));
        break;
    }
    }

    return err;
}

static inline FklGrammerIgnore *
rmacro_prod_to_ig(FklCgCtx *ctx, FklGrammer *g, FklVMvalueCgRmacroProd *in) {
    FklGrammerIgnore *ig = fklCreateEmptyGrammerIgnore(in->len);
    for (size_t i = 0; i < in->len; ++i) {
        const FklCgRmacroGraSym *from = &in->syms[i];
        IgSym *to = &ig->ig[i];
        int err = rmacro_prod_sym_to_ig_sym(ctx, from, to, g);

        if (err) {
            fklDestroyIgnore(ig);
            return NULL;
        }
    }

    return ig;
}

static inline int do_rmacro_add_ignore(FklCgCtx *ctx,
        FklGrammer *g,
        FklVMvalue *name,
        FklVMvalue *args) {
    FklVMvalueCgRmacroProd *prod = fklVMvalueCgRmacroProd(args);
    FklGrammerIgnore *ig = rmacro_prod_to_ig(ctx, g, prod);
    if (ig == NULL) {
        // should not happen
        FKL_UNREACHABLE();
        return 1;
    }

    if (fklAddIgnoreToIgnoreList(&g->ignores, ig)) {
        fklDestroyIgnore(ig);
    }

    return 0;
}

int fklExecuteCgRmacro(FklCgCtx *ctx,
        FklVMvalueCgInfo *info,
        FklVMvalue *name,
        FklVMvalueCgRmacro *r) {
    FKL_ASSERT(info->g != NULL);
    FklGrammer *g = &info->g->g;

    for (uint32_t i = 0; i < r->len; ++i) {
        int err = 0;
        const FklCgRmacroCmd *cmd = &r->cmds[i];
        switch (cmd->op) {
        case FKL_CG_RMACRO_ADD_DELIM: {
            FklVMvalue *s = cmd->args;
            FKL_ASSERT(FKL_IS_STR(s));
            fklAddString(&g->terminals, FKL_VM_STR(s));
            fklAddString(&g->delimiters, FKL_VM_STR(s));
        } break;

        case FKL_CG_RMACRO_ADD_PROD:
            err = do_rmacro_add_prod(ctx, info, name, cmd->args);
            break;

        case FKL_CG_RMACRO_ADD_IGNORE:
            err = do_rmacro_add_ignore(ctx, g, name, cmd->args);
            break;

        case FKL_CG_RMACRO_NONE:
        default:
            FKL_UNREACHABLE();
            break;
        }

        if (err != 0)
            return err;
    }
    return 0;
}

typedef enum {
    VAL_TO_GRAMMER_SYM_ERR_DUMMY = 0,
    VAL_TO_GRAMMER_SYM_ERR_INVALID,
    VAL_TO_GRAMMER_SYM_ERR_REGEX_COMPILE_FAILED,
    VAL_TO_GRAMMER_SYM_ERR_UNRESOLVED_BUILTIN,
    VAL_TO_GRAMMER_SYM_ERR_BUILTIN_TERMINAL_INIT_FAILED,
    VAL_TO_GRAMMER_SYM_ERR_INVALID_ACTION_TYPE,
    VAL_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST,
} ValToGrammerSymErr;

static inline const char *get_val_to_gra_sym_err_msg(ValToGrammerSymErr err) {
    FklBuiltinTerminalInitError builtin_terminal_err = err >> CHAR_BIT;
    err &= 0xff;

    switch (err) {
    case VAL_TO_GRAMMER_SYM_ERR_DUMMY:
        FKL_UNREACHABLE();
        break;
    case VAL_TO_GRAMMER_SYM_ERR_INVALID:
        return "invalid syntax";
        break;
    case VAL_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST:
        return "invalid action syntax";
        break;
    case VAL_TO_GRAMMER_SYM_ERR_INVALID_ACTION_TYPE:
        return "invalid action type";
        break;
    case VAL_TO_GRAMMER_SYM_ERR_BUILTIN_TERMINAL_INIT_FAILED:
        switch (builtin_terminal_err) {
        case FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY:
            FKL_UNREACHABLE();
            break;
        case FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS:
            return "init builtin terminal with too many arguments";
            break;
        case FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_FEW_ARGS:
            return "init builtin terminal with too few arguments";
            break;
        }
        return fklBuiltinTerminalInitErrorToCstr(builtin_terminal_err);
        break;
    case VAL_TO_GRAMMER_SYM_ERR_UNRESOLVED_BUILTIN:
        return "unresolved builtin terminal";
        break;
    case VAL_TO_GRAMMER_SYM_ERR_REGEX_COMPILE_FAILED:
        return "failed to compile regex";
        break;
    }

    return NULL;
}

static inline int is_builtin_gra_sym(const FklVMvalue *sym) {
    const FklString *s = FKL_VM_SYM(sym);
    return fklCharBufMatch(FKL_PG_SPECIAL_PREFIX,
                   sizeof(FKL_PG_SPECIAL_PREFIX) - 1,
                   s->str,
                   s->size)
        == sizeof(FKL_PG_SPECIAL_PREFIX) - 1;
}

typedef FklLalrBuiltinMatch BtS;

static FKL_ALWAYS_INLINE const BtS *is_valid_builtin_term(const FklGrammer *g,
        const FklVMvalue *sym) {
    const BtS *builtin_terminal = fklGetBuiltinMatch(&g->builtins, sym);
    return builtin_terminal;
}

static inline int is_valid_production_rule_node(const FklVMvalue *n) {
    return FKL_IS_VECTOR(n)                                //
        || FKL_IS_STR(n)                                   //
        || (FKL_IS_BOX(n) && FKL_IS_VECTOR(FKL_VM_BOX(n))) //
        || is_string_list(n);
}

typedef FklBuiltinTerminalInitError BtError;

static FKL_ALWAYS_INLINE BtError do_check_bs_args(const BtS *bt,
        const FklGrammer *g,
        FklVMvalueVec *args_vec) {
    size_t arg_count = 0;
    FklString const **args = NULL;
    BtError err = FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY;

    if (args_vec != NULL) {
        FKL_ASSERT(FKL_IS_VECTOR(FKL_VM_VAL(args_vec)));
        FKL_ASSERT(args_vec->size > 0);

        arg_count = args_vec->size - 1;
        size_t total_size = arg_count * sizeof(FklString *);
        args = fklZmalloc(total_size);

        for (size_t i = 1; i < args_vec->size; ++i) {
            const FklString *str_v = FKL_VM_STR(args_vec->base[i]);
            args[i - 1] = str_v;
        }
    }

    if (bt->args_check != NULL) {
        err = bt->args_check(arg_count, args, g);
    }

    if (args != NULL) {
        fklZfree(args);
    }

    arg_count = 0;

    return err;
}

static inline FklVMvalue *
make_BtS_args_check(FklVM *vm, BtError err, FklVMvalue *sym) {
    FklVMvalue *err_v = NULL;
    switch (err) {
    case FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY:
        FKL_UNREACHABLE();
        break;
    case FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS:
        err_v = FKL_MAKE_VM_ERR(FKL_ERR_GRAMMER_CREATE_FAILED,
                vm,
                "Too many args passed to builtin terminal %S",
                sym);
        break;
    case FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_FEW_ARGS:
        err_v = FKL_MAKE_VM_ERR(FKL_ERR_GRAMMER_CREATE_FAILED,
                vm,
                "Too few args passed to builtin terminal %S",
                sym);
        break;
    }

    FKL_ASSERT(err_v != NULL);
    return err_v;
}

FKL_NODISCARD
static inline ValToGrammerSymErr vec_to_builtin_terminal(const FklCgCtx *ctx,
        FklVMvalue *vec,
        FklCgRmacroGraSym *s,
        const FklGrammer *g) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *errors = ctx->error_state;
    if (FKL_VM_VEC(vec)->size == 0)
        return VAL_TO_GRAMMER_SYM_ERR_INVALID;

    FklVMvalue *first = FKL_VM_VEC(vec)->base[0];
    if (!FKL_IS_SYM(first))
        return VAL_TO_GRAMMER_SYM_ERR_INVALID;

    if (is_builtin_gra_sym(first)) {
        const BtS *bs = is_valid_builtin_term(g, first);
        if (bs == NULL)
            return VAL_TO_GRAMMER_SYM_ERR_UNRESOLVED_BUILTIN;

        for (size_t i = 1; i < FKL_VM_VEC(vec)->size; ++i) {
            if (!FKL_IS_STR(FKL_VM_VEC(vec)->base[i]))
                return VAL_TO_GRAMMER_SYM_ERR_INVALID;
        }

        BtError err = do_check_bs_args(bs, g, FKL_VM_VEC(vec));
        if (err != FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY) {
            errors->error = make_BtS_args_check(vm, err, first);
            return VAL_TO_GRAMMER_SYM_ERR_INVALID;
        }

        s->type = FKL_TERM_BUILTIN;
        s->v = vec;
        return VAL_TO_GRAMMER_SYM_ERR_DUMMY;
    } else {
        return VAL_TO_GRAMMER_SYM_ERR_INVALID;
    }
}

static inline int is_regex_str_valid(const FklString *s) {
    FklRegexCode *re = fklRegexCompileCharBuf(s->str, s->size);
    if (re == NULL)
        return 1;
    fklRegexFree(re);
    return 1;
}

static inline ValToGrammerSymErr val_to_grammer_sym(const FklCgCtx *cg_ctx,
        FklVMvalue *node,
        FklCgRmacroGraSym *s) {
    const FklGrammer *g = &cg_ctx->builtin_g;
    FklVM *vm = cg_ctx->vm;
    FklCgErrorState *errors = cg_ctx->error_state;
    if (FKL_IS_VECTOR(node)) {
        return vec_to_builtin_terminal(cg_ctx, node, s, g);
    } else if (FKL_IS_BYTEVECTOR(node)) {
        FklBytevector *bytes = FKL_VM_BVEC(node);
        s->type = FKL_TERM_KEYWORD;
        s->v = fklCreateVMvalueStr2(vm, bytes->size, (const char *)bytes->ptr);
    } else if (FKL_IS_BOX(node)) {
        FklVMvalue *v = FKL_VM_BOX(node);
        if (!FKL_IS_STR(v))
            return VAL_TO_GRAMMER_SYM_ERR_INVALID;
        s->type = FKL_TERM_REGEX;
        s->v = v;
        if (!is_regex_str_valid(FKL_VM_STR(v))) {
            return VAL_TO_GRAMMER_SYM_ERR_REGEX_COMPILE_FAILED;
        }
    } else if (FKL_IS_STR(node)) {
        s->type = FKL_TERM_STRING;
        s->v = node;
    } else if (FKL_IS_PAIR(node)) {
        FklVMvalue *car = FKL_VM_CAR(node);
        FklVMvalue *cdr = FKL_VM_CDR(node);
        if (!FKL_IS_SYM(car) || !FKL_IS_SYM(cdr))
            return VAL_TO_GRAMMER_SYM_ERR_INVALID;
        s->type = FKL_TERM_NONTERM;
        s->v = node;
    } else if (FKL_IS_SYM(node)) {
        if (!is_valid_nonterm_sym(node)) {
            return VAL_TO_GRAMMER_SYM_ERR_INVALID;
        }

        if (!is_builtin_gra_sym(node)) {
            s->type = FKL_TERM_NONTERM;
            s->v = node;
            return VAL_TO_GRAMMER_SYM_ERR_DUMMY;
        }

        const BtS *bt = is_valid_builtin_term(g, node);
        if (bt == NULL)
            return VAL_TO_GRAMMER_SYM_ERR_UNRESOLVED_BUILTIN;

        BtError err = do_check_bs_args(bt, g, NULL);
        if (err != FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY) {
            errors->error = make_BtS_args_check(vm, err, node);
            return VAL_TO_GRAMMER_SYM_ERR_INVALID;
        }

        s->type = FKL_TERM_BUILTIN;
        s->v = node;

    } else {
        return VAL_TO_GRAMMER_SYM_ERR_INVALID;
    }

    return VAL_TO_GRAMMER_SYM_ERR_DUMMY;
}

static inline int is_concat_sym(const FklString *str) {
    return fklStringCstrCmp(str, FKL_PG_TERM_CONCAT) == 0;
}

typedef struct {
    // in
    const FklCgCtx *ctx;

    // out
    FklVMvalue *err_node;
    FklVMvalueCgRmacroProd *prod;
    int adding_ignore;
} VecToGrammerSymArgs;

// CgRmacroGraSymVector
#define FKL_VECTOR_TYPE_PREFIX Cg
#define FKL_VECTOR_METHOD_PREFIX cg
#define FKL_VECTOR_ELM_TYPE FklCgRmacroGraSym
#define FKL_VECTOR_ELM_TYPE_NAME RmacroGraSym
#include <fakeLisp/cont/vector.h>

static inline FklVMvalueCgRmacroProd *create_prod(FklVM *vm, uint64_t len) {
    return fklCreateVMvalueCgRmacroProd(vm, NULL, NULL, FKL_VM_NIL, 0, len);
}

static inline ValToGrammerSymErr
vec_to_prod_right_part(VecToGrammerSymArgs *args, const FklVMvalue *vec) {
    const FklCgCtx *ctx = args->ctx;
    if (FKL_VM_VEC(vec)->size == 0) {
        args->prod = create_prod(ctx->vm, 0);
        return VAL_TO_GRAMMER_SYM_ERR_DUMMY;
    }

    ValToGrammerSymErr err = VAL_TO_GRAMMER_SYM_ERR_DUMMY;
    CgRmacroGraSymVector gsym_vector;
    cgRmacroGraSymVectorInit(&gsym_vector, 2);

    int has_ignore = 0;
    for (size_t i = 0; i < FKL_VM_VEC(vec)->size; ++i) {
        FklCgRmacroGraSym s = { .type = FKL_TERM_NONE };
        FklVMvalue *cur = FKL_VM_VEC(vec)->base[i];
        if (FKL_IS_SYM(cur) && is_concat_sym(FKL_VM_SYM(cur))) {
            if (!has_ignore) {
                args->err_node = cur;
                err = VAL_TO_GRAMMER_SYM_ERR_INVALID;
                goto error_happened;
            } else {
                has_ignore = 0;
            }
            continue;
        } else {
            err = val_to_grammer_sym(ctx, cur, &s);
            if (err) {
                args->err_node = cur;
                goto error_happened;
            }
        }

        if (has_ignore) {
            FklCgRmacroGraSym s = { .type = FKL_TERM_IGNORE };
            cgRmacroGraSymVectorPushBack(&gsym_vector, &s);
        }

        if (args->adding_ignore && s.type == FKL_TERM_NONTERM) {
            args->err_node = cur;
            err = VAL_TO_GRAMMER_SYM_ERR_INVALID;
            goto error_happened;
        }

        cgRmacroGraSymVectorPushBack(&gsym_vector, &s);
        has_ignore = !args->adding_ignore;
    }

    args->prod = create_prod(ctx->vm, gsym_vector.size);

    for (size_t i = 0; i < args->prod->len; ++i) {
        args->prod->syms[i] = gsym_vector.base[i];
    }

    cgRmacroGraSymVectorUninit(&gsym_vector);

    return VAL_TO_GRAMMER_SYM_ERR_DUMMY;

error_happened:
    cgRmacroGraSymVectorUninit(&gsym_vector);
    return err;
}

static inline FklVMvalueCgRmacroProd *vec_to_ignore(const FklVMvalue *vec,
        VecToGrammerSymArgs *args,
        ValToGrammerSymErr *perr) {

    args->adding_ignore = 1;
    ValToGrammerSymErr err = vec_to_prod_right_part(args, vec);
    *perr = err;
    if (err)
        return NULL;
    return args->prod;
}

typedef struct {
    // in
    uint32_t line;
    uint8_t add_extra;
    FklVMvalue *left_sid;
    FklVMvalue *action_type;
    FklVMvalue *action_ast;
    FklVMvalueCgInfo *info;
    FklVMvalueCgMacroScope *macro_scope;
    FklCgActVector *actions;
    FklCgCtx *ctx;

    // out
    FklVMvalueCgRmacro *const g;
    FklVMvalue *err_node;
    FklVMvalueCgRmacroProd *prod;
} NastToProductionArgs;

struct RmacroCtx {
    FklVMvalueCustomActCtx *action_ctx;
};

static inline void init_reader_macro_context(struct RmacroCtx *r,
        FklVMvalueCustomActCtx *ctx) {
    r->action_ctx = ctx;
}

static void _reader_macro_stack_context_atomic(FklVMgc *gc, void *data) {
    struct RmacroCtx *d = (struct RmacroCtx *)data;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->action_ctx), gc);
}

static const FklCgActCtxMt RmacroStackContextMethodTable = {
    .size = sizeof(struct RmacroCtx),
    .atomic = _reader_macro_stack_context_atomic,
};

static inline FklCgActCtx *createRmacroActionContext(
        FklVMvalueCustomActCtx *ctx) {
    FklCgActCtx *r = createCgActCtx(&RmacroStackContextMethodTable);

    init_reader_macro_context(FKL_TYPE_CAST(struct RmacroCtx *, r->d), ctx);

    return r;
}

static FklVMvalue *_reader_macro_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    struct RmacroCtx *d = FKL_TYPE_CAST(struct RmacroCtx *, data);
    FklVMvalueCustomActCtx *custom_ctx = d->action_ctx;
    d->action_ctx = NULL;

    FklVMvalueProto *pt = fklCreateVMvalueProto2(ctx->vm, env);
    fklPrintUndefinedRef(env->prev, &ctx->vm->gc->err_out);

    FklVMvalue *macro_bcl = *fklValueVectorPopBackNonNull(bcl_vec);
    FklIns ret = FKL_MAKE_INS_I(FKL_OP_RET);
    fklByteCodeLntPushBackIns(FKL_VM_CO(macro_bcl), ret, fid, line, scope);

    fklPeepholeOptimize(FKL_VM_CO(macro_bcl));

    FklVMvalue *proc = fklCreateVMvalueProc(ctx->vm, macro_bcl, pt);
    fklInitMainProcRefs(ctx->vm, proc);

    custom_ctx->proc = proc;
    return NULL;
}

static inline ValToGrammerSymErr vec_to_prod(const FklVMvalue *vec,
        NastToProductionArgs *args) {
    FklCgCtx *ctx = args->ctx;

    VecToGrammerSymArgs other_args = { .ctx = ctx };
    ValToGrammerSymErr err = vec_to_prod_right_part(&other_args, vec);
    if (err) {
        args->err_node = other_args.err_node;
        return err;
    }

    FKL_ASSERT(other_args.prod != NULL);
    FklVMvalue *action_type = args->action_type;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalue *action_ast = args->action_ast;
    FklVMvalue *left_sid = args->left_sid;

    FklVMvalueCgRmacroProd *prod = other_args.prod;

    args->prod = prod;
    prod->left = left_sid;
    prod->add_extra = args->add_extra;
    prod->action_type = action_type;

    if (action_type == ctx->builtin_sym_builtin) {
        if (!FKL_IS_SYM(action_ast)
                || !fklIsCgRmacroBuiltinActionValid(ctx, action_ast)) {
            args->err_node = action_ast;
            err = VAL_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }

        prod->action = action_ast;
    } else if (action_type == ctx->builtin_sym_simple) {
        if (!FKL_IS_VECTOR(action_ast)               //
                || FKL_VM_VEC(action_ast)->size == 0 //
                || !FKL_IS_SYM(FKL_VM_VEC(action_ast)->base[0])) {
            args->err_node = action_ast;
            err = VAL_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }

        FklVMvalueSimpleActCtx *action = NULL;
        action = fklCreateVMvalueSimpleActCtx1(ctx, action_ast);
        if (action == NULL) {
            args->err_node = action_ast;
            err = VAL_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }

        prod->action = FKL_VM_VAL(action);
    } else if (action_type == ctx->builtin_sym_custom) {
        FklVMvalueCgEnv *macro_env = NULL;
        FklVMvalueCgInfo *macro_info = macro_compile_prepare(ctx,
                info,
                args->macro_scope,
                NULL,
                &macro_env,
                CURLINE(action_ast));

        CgExpQueue *queue = cgExpQueueCreate();
        int failed = 0;
        if (failed) {
            args->err_node = action_ast;
            err = VAL_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }

        FklVMvalueCustomActCtx *act_ctx = NULL;

        act_ctx = fklCreateCgRmacroCustomAction(ctx, prod);

        prod->action = FKL_VM_VAL(act_ctx);

        for (size_t i = 0; i < act_ctx->actual_len; ++i) {
            fklAddCgDefBySid(act_ctx->dollers[i], 1, macro_env);
        }
        fklAddCgDefBySid(act_ctx->doller_s, 1, macro_env);
        fklAddCgDefBySid(act_ctx->line_s, 1, macro_env);

        cgExpQueuePush2(queue,
                (FklPmatchRes){
                    .value = action_ast,
                    .container = action_ast,
                });

        FklCgAct *new_act = make_cg_act(_reader_macro_bc_process,
                createRmacroActionContext(act_ctx),
                createMustHasRetvalQueueNextExpression(queue),
                1,
                macro_env->macros,
                macro_env,
                CURLINE(action_ast),
                NULL,
                macro_info);
        fklCgActVectorPushBack2(args->actions, new_act);
    } else if (action_type == ctx->builtin_sym_replace) {
        prod->action = action_ast;
    } else {
        args->err_node = NULL;
        err = VAL_TO_GRAMMER_SYM_ERR_INVALID_ACTION_TYPE;
    error_happened:
        return err;
    }

    return VAL_TO_GRAMMER_SYM_ERR_DUMMY;
}

static inline FklVMvalueCgRmacroProd *
make_ignore(FklCgCtx *ctx, FklVMvalueCgInfo *info, FklVMvalue *vector_node) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *errors = ctx->error_state;
    FklVMvalue *ignore_obj = FKL_VM_BOX(vector_node);
    FKL_ASSERT(FKL_IS_VECTOR(ignore_obj));

    VecToGrammerSymArgs args = { .ctx = ctx };

    ValToGrammerSymErr err = 0;
    FklVMvalueCgRmacroProd *prod = vec_to_ignore(ignore_obj, &args, &err);

    if (err) {
        if (errors->error != NULL) {
            errors->line = CURLINE(args.err_node);
        } else {
            const char *msg = get_val_to_gra_sym_err_msg(err);
            errors->error = make_grammer_create_error2(vm, msg, args.err_node);
            errors->line = CURLINE(vector_node);
        }
        return NULL;
    }

    return prod;
}

// CgRmacroCmdVector
#define FKL_VECTOR_TYPE_PREFIX Cg
#define FKL_VECTOR_METHOD_PREFIX cg
#define FKL_VECTOR_ELM_TYPE FklCgRmacroCmd
#define FKL_VECTOR_ELM_TYPE_NAME RmacroCmd
#include <fakeLisp/cont/vector.h>

static int add_delimiters(FklCgCtx *ctx,
        FklVMvalueCgInfo *info,
        FklVMvalue *vector_node,
        CgRmacroCmdVector *cmds) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *errors = ctx->error_state;
    if (FKL_IS_STR(vector_node)) {
        FklCgRmacroCmd cmd = {
            .op = FKL_CG_RMACRO_ADD_DELIM,
            .args = vector_node,
        };
        cgRmacroCmdVectorPushBack(cmds, &cmd);
        return 0;
    }

    if (FKL_IS_PAIR(vector_node)) {
        const FklVMvalue *cur = vector_node;
        for (; FKL_IS_PAIR(cur); cur = FKL_VM_CDR(cur)) {
            FKL_ASSERT(FKL_IS_STR(FKL_VM_CAR(cur)));
            FklCgRmacroCmd cmd = {
                .op = FKL_CG_RMACRO_ADD_DELIM,
                .args = FKL_VM_CAR(cur),
            };
            cgRmacroCmdVectorPushBack(cmds, &cmd);
        }

        if (cur != FKL_VM_NIL) {
            errors->error = make_syntax_error(vm, vector_node);
            errors->line = CURLINE(vector_node);
            return 1;
        }

        return 0;
    }

    if (FKL_IS_BOX(vector_node)) {
        FklVMvalueCgRmacroProd *ignore = make_ignore(ctx, info, vector_node);
        if (ignore == NULL)
            return 1;

        FklCgRmacroCmd cmd = {
            .op = FKL_CG_RMACRO_ADD_IGNORE,
            .args = FKL_VM_VAL(ignore),
        };

        cgRmacroCmdVectorPushBack(cmds, &cmd);
        return 0;
    }

    errors->error = make_syntax_error(vm, vector_node);
    errors->line = CURLINE(vector_node);

    return 1;
}

static inline int parse_rmacro_cmds(FklCgCtx *ctx,
        CgRmacroCmdVector *cmds,
        FklVMvalueCgInfo *info,
        FklVMvalue *vector_node,
        FklVMvalueCgMacroScope *macro_scope,
        FklCgActVector *actions) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *errors = ctx->error_state;

    if (!FKL_IS_VECTOR(vector_node)) {
        if (add_delimiters(ctx, info, vector_node, cmds))
            return 1;

        return 0;
    }

    FklVMvalue *error_node = NULL;
    FklVMvalue *vec = vector_node;

    if (FKL_VM_VEC(vec)->size != 4) {
        error_node = vector_node;
    reader_macro_syntax_error:
        errors->error = make_syntax_error(vm, error_node);
        errors->line = CURLINE(vec);
        return 1;
    }

    FklVMvalue **base = FKL_VM_VEC(vec)->base;

    if (!FKL_IS_SYM(base[2])) {
        error_node = base[2];
        goto reader_macro_syntax_error;
    }

    FklVMvalue *vect = NULL;
    NastToProductionArgs args = {
        .line = CURLINE(vec),
        .add_extra = 1,
        .action_type = base[2],
        .action_ast = base[3],
        .info = info,
        .macro_scope = macro_scope,
        .actions = actions,
        .ctx = ctx,
    };

    args.left_sid = FKL_VM_NIL;

    if (base[0] == FKL_VM_NIL && FKL_IS_VECTOR(base[1])) {
        vect = base[1];
        args.add_extra = 0;
    } else if (FKL_IS_SYM(base[0]) && FKL_IS_VECTOR(base[1])) {
        vect = base[1];
        FklVMvalue *sid = base[0];

        args.left_sid = sid;
    } else if (FKL_IS_VECTOR(base[0]) && FKL_IS_SYM(base[1])) {
        vect = base[0];
        FklVMvalue *sid = base[1];

        args.left_sid = sid;
        args.add_extra = 0;
    } else {
        error_node = vector_node;
        goto reader_macro_syntax_error;
    }

    if (fklIsNonterminalExist(&ctx->builtin_g, args.left_sid)) {
        errors->error = FKL_MAKE_VM_ERR(FKL_ERR_GRAMMER_CREATE_FAILED,
                vm,
                "cannot redefine builtin non-terminal %S",
                args.left_sid);

        errors->line = CURLINE(vect);
        return 1;
    }

    if (is_valid_builtin_term(&ctx->builtin_g, args.left_sid)) {
        errors->error = FKL_MAKE_VM_ERR(FKL_ERR_GRAMMER_CREATE_FAILED,
                vm,
                "cannot redefine builtin special terminal %S",
                args.left_sid);
        errors->line = CURLINE(vect);
        return 1;
    }

    ValToGrammerSymErr err = vec_to_prod(vect, &args);
    if (err == VAL_TO_GRAMMER_SYM_ERR_DUMMY) {
        FklCgRmacroCmd cmd = {
            .op = FKL_CG_RMACRO_ADD_PROD,
            .args = FKL_VM_VAL(args.prod),
        };
        cgRmacroCmdVectorPushBack(cmds, &cmd);
        return 0;
    }

    FklVMvalue *err_val = args.err_node == NULL ? base[2] : args.err_node;
    if (errors->error != NULL) {
        errors->line = CURLINE(args.err_node);
    } else {
        const char *msg = get_val_to_gra_sym_err_msg(err);
        errors->error = make_grammer_create_error2(vm, msg, err_val);
        errors->line = CURLINE(vect);
    }
    return 1;
}

static inline FklVMvalueCgRmacro *make_rmacro(FklCgCtx *ctx,
        CgRmacroCmdVector *cmds) {
    FklVM *vm = ctx->vm;
    uint32_t len = cmds->size;
    FklVMvalueCgRmacro *rmacro = fklCreateVMvalueCgRmacro(vm, len);
    for (uint32_t i = 0; i < len; ++i) {
        rmacro->cmds[i] = cmds->base[i];
    }

    return rmacro;
}

FklVMvalueCgRmacro *fklCgParseReaderMacroDefine(FklCgCtx *ctx,
        FklCgActVector *actions,
        FklVMvalue *rest,
        FklVMvalueCgInfo *info,
        FklVMvalueCgMacroScope *ms) {
    FklVM *vm = ctx->vm;

    FklCgErrorState *errors = ctx->error_state;
    FKL_ASSERT(errors);

    FklVMvalue *rv = rest;

    for (; FKL_IS_PAIR(rv); rv = FKL_VM_CDR(rv)) {
        if (!is_valid_production_rule_node(FKL_VM_CAR(rv))) {
            errors->error = make_syntax_error(vm, FKL_VM_CAR(rv));
            errors->line = get_curline(info, rv);
            return NULL;
        }
    }

    if (!FKL_IS_NIL(rv)) {
        errors->error = make_syntax_error(vm, FKL_VM_CAR(rest));
        errors->line = get_curline(info, rest);
    }

    CgRmacroCmdVector cmd_vec = { 0 };
    cgRmacroCmdVectorInit(&cmd_vec, 8);

    for (rv = rest; FKL_IS_PAIR(rv); rv = FKL_VM_CDR(rv)) {
        FklVMvalue *cur = FKL_VM_CAR(rv);
        if (parse_rmacro_cmds(ctx, &cmd_vec, info, cur, ms, actions)) {
            cgRmacroCmdVectorUninit(&cmd_vec);
            return NULL;
        }
    }

    FklVMvalueCgRmacro *rmacro = make_rmacro(ctx, &cmd_vec);

    cgRmacroCmdVectorUninit(&cmd_vec);

    return rmacro;
}

static void cg_lib_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgLib *l = fklVMvalueCgLib(ud);

    fklVMgcToGray(FKL_VM_VAL(l->lib), gc);

    mark_export_sid_map(&l->exports, gc);

    fklVMgcToGray(FKL_VM_VAL(l->macros), gc);
    fklVMgcToGray(FKL_VM_VAL(l->replacements), gc);
    fklVMgcToGray(FKL_VM_VAL(l->rmacros), gc);
    fklVMgcToGray(l->rp, gc);
    fklVMgcToGray(l->re_exports, gc);
}

static void
cg_lib_print(const FklVMvalue *ud, FklCodeBuilder *build, FklVM *exe) {
    const FklVMvalueCgLib *lib = fklVMvalueCgLib(ud);
    FKL_ASSERT(FKL_IS_SYM(lib->rp));

    FklVMvalue *const values[] = { lib->rp };
    fklVMformat(exe, build, "#<cg-lib %S>", NULL, 1, values);
}

static int cg_lib_finalize(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgLib *l = fklVMvalueCgLib(ud);
    fklCgExportSidIdxHashMapUninit(&l->exports);
    l->macros = NULL;
    l->replacements = NULL;
    l->rmacros = NULL;

    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const CgLibMt = {
    .size = sizeof(FklVMvalueCgLib),
    .princ = cg_lib_print,
    .prin1 = cg_lib_print,
    .atomic = cg_lib_atomic,
    .finalize = cg_lib_finalize,
};

int fklIsVMvalueCgLib(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &CgLibMt;
}

FklVMvalueCgLib *fklCreateVMvalueCgLib(FklVM *vm, FklVMvalue *rp_s) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &CgLibMt, NULL);
    FklVMvalueCgLib *l = fklVMvalueCgLib(v);

    l->rp = rp_s;
    l->re_exports = FKL_VM_NIL;
    return l;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(cg_re_export_print, "cg-re-export");

static void cg_re_export_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgReExport *l = fklVMvalueCgReExport(ud);

    fklVMgcToGray(FKL_VM_VAL(l->lib), gc);
    fklVMgcToGray(l->args, gc);
}

static FklVMudMetaTable const CgReExportMt = {
    .size = sizeof(FklVMvalueCgReExport),
    .princ = cg_re_export_print,
    .prin1 = cg_re_export_print,
    .atomic = cg_re_export_atomic,
};

int fklIsVMvalueCgReExport(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &CgReExportMt;
}

FklVMvalueCgReExport *fklCreateVMvalueCgReExport(FklVM *vm,
        FklVMvalueCgLib *lib,
        FklCgImportType type,
        FklVMvalue *args) {
    FKL_ASSERT(lib != NULL);
    FklVMvalue *v = fklCreateVMvalueUd(vm, &CgReExportMt, NULL);
    FklVMvalueCgReExport *r = fklVMvalueCgReExport(v);

    r->lib = lib;
    r->type = type;
    r->args = args;

    return r;
}

FklVMvalue *fklCgAppendReExport(FklVM *vm,
        FklVMvalueCgInfo *info,
        const FklVMvalueCgReExport *re_export) {
    FklVMvalue *new_pair = fklCreateVMvaluePair1(vm, FKL_VM_VAL(re_export));

    if (info->re_exports == FKL_VM_NIL) {
        info->re_exports = new_pair;
        info->last_re_export = new_pair;
    } else {
        FklVMvalue *last_pair = info->last_re_export;
        FKL_VM_CDR(last_pair) = new_pair;
        info->last_re_export = new_pair;
    }

    return new_pair;
}

static void add_compiler_macro(FklVM *vm,
        FklVMvalueCgMacroHashMap *macros,
        int no_replace,
        FklVMvalue *head,
        FklVMvalueCgMacro *macro) {
    FKL_ASSERT(FKL_IS_SYM(head));
    int cover = FKL_PATTERN_NOT_EQUAL;
    FklValueHashMapElm *pmacro = fklCgMacroHashMapRef1(macros, head);
    FklVMvalue *const pattern = macro->pattern;
    FklVMvalue **phead = &pmacro->v;

    for (FklVMvalue *p = *phead; FKL_IS_PAIR(p); p = *phead) {
        FklVMvalueCgMacro *cur = fklVMvalueCgMacro(FKL_VM_CAR(p));
        cover = fklPatternCoverState(cur->pattern, pattern);
        if (cover == FKL_PATTERN_EQUAL || cover == FKL_PATTERN_COVER)
            break;
        phead = &FKL_VM_CDR(p);
    }

    switch (cover) {
    case FKL_PATTERN_NOT_EQUAL:
        pmacro->v = fklCreateVMvaluePair(vm, FKL_VM_VAL(macro), pmacro->v);
        break;
    case FKL_PATTERN_EQUAL:
        if (no_replace)
            break;
        FKL_VM_CAR(*phead) = FKL_VM_VAL(macro);
        break;

    case FKL_PATTERN_BE_COVER:
    case FKL_PATTERN_COVER:
        *phead = fklCreateVMvaluePair(vm, FKL_VM_VAL(macro), *phead);
        break;
    }
}

static void import_macro_list(FklVM *vm,
        FklVMvalue *sym,
        FklVMvalue *list,
        const FklCgImportArgs *to) {
    FklVMvalue *cur_pair = list;
    for (; FKL_IS_PAIR(cur_pair); cur_pair = FKL_VM_CDR(cur_pair)) {
        FklVMvalueCgMacro *macro = fklVMvalueCgMacro(FKL_VM_CAR(cur_pair));
        size_t const count = sizeof(to->macros) / sizeof(to->macros[0]);
        for (size_t i = 0; i < count; ++i) {
            FklVMvalueCgMacroHashMap *macros = to->macros[i];
            if (macros == NULL)
                continue;
            add_compiler_macro(vm, macros, to->no_replace, sym, macro);
        }
    }

    FKL_ASSERT(cur_pair == FKL_VM_NIL);
}

static void import_replacement(FklVM *vm,
        FklVMvalue *sym,
        FklVMvalueCgRpl *rpl,
        const FklCgImportArgs *to) {
    size_t const count = sizeof(to->replacements) / sizeof(to->replacements[0]);
    for (size_t i = 0; i < count; ++i) {
        FklVMvalueCgRplHashMap *rpls = to->replacements[i];
        if (rpls == NULL)
            continue;
        FklVMvalueCgRpl *old = fklCgRplHashMapGet(rpls, sym);
        if (to->no_replace && old != NULL) {
            continue;
        }

        fklCgRplHashMapSet(rpls, sym, rpl);
    }
}

static inline int do_add_rmacro(FklVM *vm,
        int no_replace,
        FklVMvalueCgRmacroHashMap *map,
        FklVMvalue *new_id,
        FklVMvalueCgRmacro *rmacro) {
    FklValueHashMapElm *old = fklCgRmacroHashMapGet(map, new_id);
    if (no_replace && old != NULL)
        return 0;

    fklCgRmacroHashMapDel(map, new_id);

    FklValueHashMapElm *e = NULL;
    e = fklCgRmacroHashMapRef1(map, new_id);
    e->v = FKL_VM_VAL(rmacro);

    return old != NULL;
}

static void import_reader_macro(FklVM *vm,
        FklVMvalue *sym,
        FklVMvalueCgRmacro *rmacro,
        FklCgImportArgs *to) {
    size_t const count = sizeof(to->rmacros) / sizeof(to->rmacros[0]);
    for (size_t i = 0; i < count; ++i) {
        FklVMvalueCgRmacroHashMap *rmacros = to->rmacros[i];
        if (rmacros == NULL)
            continue;

        int r = do_add_rmacro(vm, to->no_replace, rmacros, sym, rmacro);
        to->need_rebuild[i] |= r;
    }

    if (to->no_replace || to->rmacro_vec == NULL)
        return;

    FKL_ASSERT(to->rmacro_vec != NULL);
    FklPair pair = {
        .car = sym,
        .cdr = FKL_VM_VAL(rmacro),
    };

    fklPairVectorPushBack(to->rmacro_vec, &pair);
}

FklCgExportIdx *fklCgExportAdd(FklCgExportSidIdxHashMap *exports,
        FklVMvalue *s,
        uint8_t not_owned) {
    const FklCgExportIdx item = {
        .idx = exports->count,
        .not_owned = not_owned,
    };

    FklCgExportSidIdxHashMapElm *r =
            fklCgExportSidIdxHashMapInsert(exports, &s, &item);

    r->v.not_owned &= not_owned;
    return &r->v;
}

static void import_symbol(FklVM *vm,
        FklVMvalue *new_head,
        FklVMvalue *old,
        const FklVMvalueCgLib *from,
        uint8_t not_owned,
        FklCgImportArgs *to) {
    if (to->exports != NULL) {
        if (to->cg_ctx != NULL) {
            const char *rp = FKL_VM_SYM(from->rp)->str;
            // 不是内部模块，我们把 not_owned 设置为 true
            not_owned |= !fklIsInternalModule(to->cg_ctx, rp);
        }

        fklCgExportAdd(to->exports, new_head, not_owned);
    }

    if (to->env != NULL) {
        fklAddCgDefBySid(new_head, to->scope, to->env);
    }

    if (to->import_cache != NULL) {
        FklPair p = { .car = new_head, .cdr = old };
        fklPairVectorPushBack(to->import_cache, &p);
    }
}

static int do_import_macros_common(FklVM *vm,
        const FklVMvalueCgLib *from,
        FklCgImportArgs *to) {
    const FklVMvalueCgMacroHashMap *macros = from->macros;
    for (const FklValueHashMapNode *cur = macros->ht.first; cur;
            cur = cur->next) {
        FklVMvalue *head = cur->k;
        FklVMvalue *list = cur->v;

        import_macro_list(vm, head, list, to);
    }

    const FklVMvalueCgRplHashMap *rpls = from->replacements;
    for (FklValueHashMapNode *cur = rpls->ht.first; cur; cur = cur->next) {
        import_replacement(vm, cur->k, fklVMvalueCgRpl(cur->v), to);
    }

    const FklVMvalueCgRmacroHashMap *rmacros = from->rmacros;
    for (FklValueHashMapNode *cur = rmacros->ht.first; cur; cur = cur->next) {
        import_reader_macro(vm, cur->k, fklVMvalueCgRmacro(cur->v), to);
    }

    const FklCgExportSidIdxHashMap *exports = &from->exports;
    for (const FklCgExportSidIdxHashMapNode *cur = exports->first; cur;
            cur = cur->next) {
        import_symbol(vm, cur->k, cur->k, from, cur->v.not_owned, to);
    }

    return 0;
}

static inline FklVMvalue *append_symbol_prefix(FklVM *vm,
        const FklString *prefix,
        FklVMvalue *sym,
        FklStrBuf *buf) {
    fklStrBufClear(buf);
    const FklString *head = FKL_VM_SYM(sym);
    fklStrBufConcatWithString(buf, prefix);
    fklStrBufConcatWithString(buf, head);
    FklVMvalue *r = fklVMaddSymbolCharBuf(vm, buf->buf, buf->index);
    return r;
}

static int do_import_macros_prefix(FklVM *vm,
        const FklVMvalueCgLib *from,
        FklCgImportArgs *to) {
    FklStrBuf buf = { 0 };
    fklInitStrBuf(&buf);

    const FklString *prefix = FKL_VM_SYM(to->args);

    const FklVMvalueCgMacroHashMap *macros = from->macros;
    for (const FklValueHashMapNode *cur = macros->ht.first; cur;
            cur = cur->next) {
        FklVMvalue *head = cur->k;
        FklVMvalue *list = cur->v;

        FklVMvalue *new_head = append_symbol_prefix(vm, prefix, head, &buf);
        import_macro_list(vm, new_head, list, to);
    }

    const FklVMvalueCgRplHashMap *rpls = from->replacements;
    for (FklValueHashMapNode *cur = rpls->ht.first; cur; cur = cur->next) {

        FklVMvalue *head = cur->k;
        FklVMvalue *new_head = append_symbol_prefix(vm, prefix, head, &buf);
        import_replacement(vm, new_head, fklVMvalueCgRpl(cur->v), to);
    }

    const FklVMvalueCgRmacroHashMap *rmacros = from->rmacros;
    for (FklValueHashMapNode *cur = rmacros->ht.first; cur; cur = cur->next) {
        FklVMvalue *head = cur->k;
        FklVMvalue *new_head = append_symbol_prefix(vm, prefix, head, &buf);

        import_reader_macro(vm, new_head, fklVMvalueCgRmacro(cur->v), to);
    }

    const FklCgExportSidIdxHashMap *exports = &from->exports;
    for (const FklCgExportSidIdxHashMapNode *cur = exports->first; cur;
            cur = cur->next) {
        FklVMvalue *head = cur->k;
        FklVMvalue *new_head = append_symbol_prefix(vm, prefix, head, &buf);

        import_symbol(vm, new_head, head, from, cur->v.not_owned, to);
    }

    fklUninitStrBuf(&buf);
    return 0;
}

static int do_import_macros_only(FklVM *vm,
        const FklVMvalueCgLib *from,
        FklCgImportArgs *to) {
    FklVMvalue *sym_list = to->args;
    FklVMvalue *cur = sym_list;

    const FklVMvalueCgMacroHashMap *macros = from->macros;
    const FklVMvalueCgRplHashMap *rpls = from->replacements;
    const FklVMvalueCgRmacroHashMap *rmacros = from->rmacros;
    const FklCgExportSidIdxHashMap *exports = &from->exports;

    int import_missing = 0;
    for (; FKL_IS_PAIR(cur); cur = FKL_VM_CDR(cur)) {
        int has_entity = 0;
        FklVMvalue *sym = FKL_VM_CAR(cur);
        FklValueHashMapElm *kv = fklCgMacroHashMapGet(macros, sym);
        if (kv != NULL) {
            has_entity = 1;
            import_macro_list(vm, sym, kv->v, to);
        }

        FklVMvalueCgRpl *rpl = fklCgRplHashMapGet(rpls, sym);
        if (rpl != NULL) {
            has_entity = 1;
            import_replacement(vm, sym, rpl, to);
        }

        kv = fklCgRmacroHashMapGet(rmacros, sym);
        if (kv != NULL) {
            has_entity = 1;
            import_reader_macro(vm, sym, fklVMvalueCgRmacro(kv->v), to);
        }

        const FklCgExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, sym);
        if (item != NULL) {
            has_entity = 1;
            import_symbol(vm, sym, sym, from, item->not_owned, to);
        }

        if (!has_entity && to->missing_syms != NULL) {
            import_missing = 1;
            fklValueVectorPushBack2(to->missing_syms, sym);
        }
    }

    FKL_ASSERT(cur == FKL_VM_NIL);

    return import_missing ? -1 : 0;
}

static int do_import_macros_alias(FklVM *vm,
        const FklVMvalueCgLib *from,
        FklCgImportArgs *to) {
    FklVMvalue *sym_list = to->args;
    FklVMvalue *cur = sym_list;

    const FklVMvalueCgMacroHashMap *macros = from->macros;
    const FklVMvalueCgRplHashMap *rpls = from->replacements;
    const FklVMvalueCgRmacroHashMap *rmacros = from->rmacros;
    const FklCgExportSidIdxHashMap *exports = &from->exports;

    int import_missing = 0;
    for (; FKL_IS_PAIR(cur); cur = FKL_VM_CDR(cur)) {
        FklVMvalue *cur_list = FKL_VM_CAR(cur);
        FKL_ASSERT(FKL_IS_PAIR(cur_list));

        int has_entity = 0;
        FklVMvalue *sym = FKL_VM_CAR(cur_list);
        FklVMvalue *alias = FKL_VM_CAR(FKL_VM_CDR(cur_list));
        FKL_ASSERT(FKL_VM_CDR(FKL_VM_CDR(cur_list)) == FKL_VM_NIL);

        FklValueHashMapElm *kv = fklCgMacroHashMapGet(macros, sym);
        if (kv != NULL) {
            has_entity = 1;
            import_macro_list(vm, alias, kv->v, to);
        }

        FklVMvalueCgRpl *rpl = fklCgRplHashMapGet(rpls, sym);
        if (rpl != NULL) {
            has_entity = 1;
            import_replacement(vm, alias, rpl, to);
        }

        kv = fklCgRmacroHashMapGet(rmacros, sym);
        if (kv != NULL) {
            has_entity = 1;
            import_reader_macro(vm, alias, fklVMvalueCgRmacro(kv->v), to);
        }

        const FklCgExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, sym);
        if (item != NULL) {
            has_entity = 1;
            import_symbol(vm, alias, sym, from, item->not_owned, to);
        }

        if (!has_entity && to->missing_syms != NULL) {
            import_missing = 1;
            fklValueVectorPushBack2(to->missing_syms, sym);
        }
    }

    FKL_ASSERT(cur == FKL_VM_NIL);

    return import_missing ? -1 : 0;
}

static int do_import_macros_except_impl(FklVM *vm,
        const FklVMvalueCgLib *const from,
        const FklValueHashSet *const excepts,
        FklCgImportArgs *const to) {
    const FklVMvalueCgMacroHashMap *macros = from->macros;
    for (const FklValueHashMapNode *cur = macros->ht.first; cur;
            cur = cur->next) {
        if (fklValueHashSetHas2(excepts, cur->k))
            continue;

        FklVMvalue *head = cur->k;
        FklVMvalue *list = cur->v;
        import_macro_list(vm, head, list, to);
    }

    const FklVMvalueCgRplHashMap *rpls = from->replacements;
    for (FklValueHashMapNode *cur = rpls->ht.first; cur; cur = cur->next) {
        if (fklValueHashSetHas2(excepts, cur->k))
            continue;

        import_replacement(vm, cur->k, fklVMvalueCgRpl(cur->v), to);
    }

    const FklVMvalueCgRmacroHashMap *rmacros = from->rmacros;
    for (FklValueHashMapNode *cur = rmacros->ht.first; cur; cur = cur->next) {
        if (fklValueHashSetHas2(excepts, cur->k))
            continue;

        import_reader_macro(vm, cur->k, fklVMvalueCgRmacro(cur->v), to);
    }

    const FklCgExportSidIdxHashMap *exports = &from->exports;
    for (const FklCgExportSidIdxHashMapNode *cur = exports->first; cur;
            cur = cur->next) {
        if (fklValueHashSetHas2(excepts, cur->k))
            continue;

        import_symbol(vm, cur->k, cur->k, from, cur->v.not_owned, to);
    }

    return 0;
}

#ifndef NDEBUG

static FKL_ALWAYS_INLINE int except_cache_verify(const FklValueHashSet *excepts,
        FklVMvalue *except) {
    if (excepts->count != fklVMlistLength(except))
        return 0;
    FklVMvalue *a = except;
    const FklValueHashSetNode *b = excepts->first;
    for (; FKL_IS_PAIR(a) && b != NULL; a = FKL_VM_CDR(a), b = b->next) {
        if (FKL_VM_CAR(a) != b->k)
            return 0;
    }

    return 1;
}

#endif

static int do_import_macros_except(FklVM *vm,
        const FklVMvalueCgLib *from,
        FklCgImportArgs *to) {
    if (to->excepts == NULL) {
        FklValueHashSet excepts;
        fklValueHashSetInit(&excepts);

        FklVMvalue *except = to->args;
        FklVMvalue *cur = except;
        for (; FKL_IS_PAIR(cur); cur = FKL_VM_CDR(cur))
            fklValueHashSetPut2(&excepts, FKL_VM_CAR(cur));
        FKL_ASSERT(cur == FKL_VM_NIL);

        do_import_macros_except_impl(vm, from, &excepts, to);

        fklValueHashSetUninit(&excepts);
    } else {
        FKL_ASSERT(except_cache_verify(to->excepts, to->args));
        do_import_macros_except_impl(vm, from, to->excepts, to);
    }

    return 0;
}

int fklCgImport(FklVM *vm, const FklVMvalueCgLib *from, FklCgImportArgs *to) {
    FKL_ASSERT(from->rmacros);
    FKL_ASSERT(from->replacements);
    FKL_ASSERT(from->macros);
    FKL_ASSERT(!to->no_replace || !to->rmacro_vec);

    switch (to->type) {
    case FKL_CG_IMPORT_COMMON:
        return do_import_macros_common(vm, from, to);
        break;
    case FKL_CG_IMPORT_PREFIX:
        return do_import_macros_prefix(vm, from, to);
        break;
    case FKL_CG_IMPORT_ONLY:
        return do_import_macros_only(vm, from, to);
        break;
    case FKL_CG_IMPORT_ALIAS:
        return do_import_macros_alias(vm, from, to);
        break;
    case FKL_CG_IMPORT_EXCEPT:
        return do_import_macros_except(vm, from, to);
        break;

    default:
        FKL_UNREACHABLE();
        break;
    }

    return -1;
}
