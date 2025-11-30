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
#include "fakeLisp/str_buf.h"

#include <inttypes.h>
#include <stdio.h>

static FklVM *init_macro_expand_vm(FklCodegenCtx *ctx,
        FklVMvalue *bcl,
        uint32_t prototype_id,
        FklPmatchHashMap *ht,
        FklVMvalueLnt *lnt,
        FklVMvalue **pr,
        uint64_t curline,
        FklCodegenErrorState *error_state);

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
        const FklVMvalueCodegenInfo *info,
        FklCodeBuilder *cb) {
    size_t line = error_state->line;
    FklVMvalue *fid = error_state->fid;

    fklPrintErrBacktrace(error_state->error, &info->ctx->gc->gcvm, NULL);

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

    for (const FklValueIdHashMapNode *cur = env->konsts.ht.first; cur;
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

void fklPrintUndefinedRef(const FklVMvalueCodegenEnv *env, FklCodeBuilder *cb) {
    const FklUnReSymbolRefVector *urefs = &env->uref;
    for (uint32_t i = urefs->size; i > 0; i--) {
        FklUnReSymbolRef *ref = &urefs->base[i - 1];
        fklCodeBuilderPuts(cb, "warning: Symbol ");
        fklPrintSymbolLiteral2(FKL_VM_SYM(ref->id), cb);
        fklCodeBuilderFmt(cb, " is undefined at line %" PRIu64, ref->line);
        if (ref->fid) {
            fklCodeBuilderPuts(cb, " of ");
            fklPrintString2(FKL_VM_SYM(ref->fid), cb);
        }
        fklCodeBuilderPutc(cb, '\n');
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
        FklVMvalue *proc = fklCreateVMvalueProc(exe,
                clib->bcl,
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
        FklCgDllLibInitExportCb init) {
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
}

void fklInitCodegenScriptLib(FklCodegenLib *lib,
        FklVMvalueCodegenInfo *info,
        FklVMvalue *bcl,
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
            for (FklValueHashSetNode *sid_list =
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

void fklClearCodegenLibMacros2(FklCodegenCtx *ctx) {

    FklCodegenLib *cur = ctx->libraries.base;
    FklCodegenLib *end = &cur[ctx->libraries.size];
    for (; cur < end; ++cur) {
        fklClearCodegenLibMacros(cur);
    }

    cur = ctx->macro_libraries.base;
    end = &cur[ctx->macro_libraries.size];
    for (; cur < end; ++cur) {
        fklClearCodegenLibMacros(cur);
    }
}

FklCodegenMacro *fklCreateCodegenMacro(FklVMvalue *pattern,
        FklVMvalue *bcl,
        FklCodegenMacro *next,
        uint32_t prototype_id) {
    FklCodegenMacro *r = (FklCodegenMacro *)fklZmalloc(sizeof(FklCodegenMacro));
    FKL_ASSERT(r);
    r->pattern = pattern;
    r->bcl = bcl;
    r->next = next;
    r->prototype_id = prototype_id;
    return r;
}

#define CURLINE(V) get_curline(codegen, V)

static inline FklVMvalue *make_macroexpand_error(FklVM *exe,
        FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_MACROEXPANDFAILED,
            exe,
            "Failed to expand macro in %S",
            place);
}

FklVMvalue *fklTryExpandCodegenMacro(const FklPmatchRes *exp,
        FklVMvalueCodegenInfo *codegen,
        FklVMvalueCodegenMacroScope *macros,
        FklCodegenErrorState *error_state) {
    FklVMvalue *r = exp->value;
    FklPmatchHashMap ht = { .buckets = NULL };
    uint64_t curline = CURLINE(exp->container);
    for (FklCodegenMacro *macro = find_macro(r, macros, &ht);
            !error_state->error && macro;
            macro = find_macro(r, macros, &ht)) {
        fklPmatchHashMapAdd2(&ht,
                codegen->ctx->builtInPatternVar_orig,
                (FklPmatchRes){ .value = r, .container = exp->container });
        FklVMvalue *retval = NULL;

        FklCodegenCtx *ctx = codegen->ctx;
        const char *cwd = ctx->cwd;
        fklChdir(codegen->dir);

        FklVM *exe = init_macro_expand_vm(ctx,
                macro->bcl,
                macro->prototype_id,
                &ht,
                ctx->lnt,
                &retval,
                curline,
                error_state);
        FklVMgc *gc = ctx->gc;
        int e = fklRunVMidleLoop(exe);
        fklMoveThreadObjectsToGc(exe, gc);

        fklChdir(cwd);

        if (e) {
            error_state->error = make_macroexpand_error(&gc->gcvm, r);
            error_state->line = curline;
            fklDeleteCallChain(exe);
            r = NULL;
        } else if (retval) {
            r = retval;
        } else {
            error_state->line = curline;
        }
        fklPmatchHashMapClear(&ht);
        fklDestroyAllVMs(exe);
    }
    if (ht.buckets)
        fklPmatchHashMapUninit(&ht);
    return r;
}

typedef struct MacroExpandCtx {
    FklVMvalue **retval;
    FklVMvalueLnt *lnt;
    uint64_t curline;
    FklCodegenErrorState *error_state;
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
macro_expand_frame_backtrace(void *data, FklCodeBuilder *build, FklVMgc *gc) {
    fklCodeBuilderPuts(build, "at <macroexpand>\n");
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
        FklCodegenErrorState *error_state) {
    FklVMframe *f = fklCreateOtherObjVMframe(exe, &MacroExpandMethodTable);
    MacroExpandCtx *ctx = (MacroExpandCtx *)f->data;
    ctx->retval = ptr;
    ctx->lnt = lnt;
    ctx->curline = curline;
    ctx->error_state = error_state;
    fklPushVMframe(f, exe);
}

static void init_macro_match_local_variable(FklVM *exe,
        FklVMframe *frame,
        FklPmatchHashMap *ht,
        FklVMvalueLnt *lnt,
        FklFuncPrototypes *pts,
        uint32_t prototype_id) {
    FklFuncPrototype *mainPts = &pts->pa[prototype_id];
    FklVMCompoundFrameVarRef *lr = fklGetCompoundFrameLocRef(frame);
    FklVMvalueProc *proc = FKL_VM_PROC(fklGetCompoundFrameProc(frame));
    uint32_t count = mainPts->lcount;
    uint32_t idx = 0;
    for (FklPmatchHashMapNode *list = ht->first; list; list = list->next) {
        FklVMvalue *v = list->v.value;
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

static inline FklVM *init_macro_expand_vm(FklCodegenCtx *ctx,
        FklVMvalue *bcl,
        uint32_t prototype_id,
        FklPmatchHashMap *ht,
        FklVMvalueLnt *lnt,
        FklVMvalue **pr,
        uint64_t curline,
        FklCodegenErrorState *error_state) {
    FklVMgc *gc = ctx->gc;
    FklVMvalueProtos *pts = ctx->macro_pts;

    FklVMvalueLibs *libs = ctx->macro_vm_libs;
    if (libs == NULL) {
        libs = fklCreateVMvalueLibs(&gc->gcvm);
        ctx->macro_vm_libs = libs;
    }

    FklVM *exe = fklCreateVM(NULL, gc, pts, libs);
    FklVMvalue *proc = fklCreateVMvalueProc(exe, bcl, prototype_id, pts);

    push_macro_expand_frame(exe, pr, lnt, curline, error_state);

    fklInitMainProcRefs(exe, proc);
    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, proc);
    fklCallObj(exe, proc);
    init_macro_match_local_variable(exe,
            exe->top_frame,
            ht,
            lnt,
            &pts->p,
            prototype_id);

    update_new_codegen_to_new_vm_lib(exe, &ctx->macro_libraries, pts, libs);
    return exe;
}

static FklVMudMetaTable const MacroScopeUserDataMetaTable;
int fklIsVMvalueCodegenMacroScope(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->mt_ == &MacroScopeUserDataMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueCodegenMacroScope *as_macro_scope(
        const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCodegenMacroScope(r));
    return FKL_TYPE_CAST(FklVMvalueCodegenMacroScope *, r);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(macro_scope_print, "macro-scope")

static void macro_scope_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCodegenMacroScope *ms = as_macro_scope(ud);
    // FKL_DECL_UD_DATA(ms, struct FklCodegenMacroScope, ud);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ms->prev), gc);
    for (const FklCodegenMacro *cur = ms->head; cur; cur = cur->next) {
        fklVMgcToGray(cur->pattern, gc);
        fklVMgcToGray(cur->bcl, gc);
    }
    for (const FklReplacementHashMapNode *cur = ms->replacements->first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->k, gc);
        fklVMgcToGray(cur->v, gc);
    }
}

static int macro_scope_finalize(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(ms, struct FklCodegenMacroScope, ud);
    FklVMvalueCodegenMacroScope *ms = as_macro_scope(ud);
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

static FklVMudMetaTable const MacroScopeUserDataMetaTable = {
    // .size = sizeof(struct FklCodegenMacroScope),
    .size = sizeof(FklVMvalueCodegenMacroScope),
    .princ = macro_scope_print,
    .prin1 = macro_scope_print,
    .atomic = macro_scope_atomic,
    .finalize = macro_scope_finalize,
};

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

static FklVMudMetaTable const EnvUserDataMetaTable;

int fklIsVMvalueCodegenEnv(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &EnvUserDataMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueCodegenEnv *as_env(const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCodegenEnv(r));
    return FKL_TYPE_CAST(FklVMvalueCodegenEnv *, r);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(env_print, "env");

static void env_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCodegenEnv *e = as_env(ud);
    // FKL_DECL_UD_DATA(e, struct FklCodegenEnv, ud);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->prev), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->macros), gc);

    for (size_t i = 0; i < e->uref.size; ++i) {
        fklVMgcToGray(e->uref.base[i].fid, gc);
        fklVMgcToGray(e->uref.base[i].id, gc);
    }

    for (const FklValueIdHashMapNode *cur = e->konsts.ht.first; cur;
            cur = cur->next) {
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, cur->k), gc);
    }
}

static int env_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCodegenEnv *cur = as_env(ud);
    // FKL_DECL_UD_DATA(cur, struct FklCodegenEnv, ud);
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

static FklVMudMetaTable const EnvUserDataMetaTable = {
    // .size = sizeof(struct FklCodegenEnv),
    .size = sizeof(FklVMvalueCodegenEnv),
    .princ = env_print,
    .prin1 = env_print,
    .atomic = env_atomic,
    .finalize = env_finalizer,
};

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

FKL_VM_USER_DATA_DEFAULT_PRINT(info_print, "info");

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

static FklVMudMetaTable const InfoUserDataMetaTable;
int fklIsVMvalueCodegenInfo(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &InfoUserDataMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueCodegenInfo *as_info(const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCodegenInfo(r));
    return FKL_TYPE_CAST(FklVMvalueCodegenInfo *, r);
}

static void info_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCodegenInfo *e = as_info(ud);
    // FKL_DECL_UD_DATA(e, struct FklCodegenInfo, ud);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->prev), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->global_env), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->pts), gc);
    // fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->lnt), gc);

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

static int info_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCodegenInfo *i = as_info(ud);
    // FKL_DECL_UD_DATA(i, struct FklCodegenInfo, ud);

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
        fklValueHashSetDestroy(i->export_named_prod_groups);
    if (i->export_replacement)
        fklReplacementHashMapDestroy(i->export_replacement);
    if (i->g == &i->self_g && *i->g) {
        FklGrammer *g = *i->g;
        fklUninitGrammer(g);
        fklUninitGrammer(i->unnamed_g);
        fklGraProdGroupHashMapUninit(i->named_prod_groups);
        fklZfree(g);
    }

    memset(i, 0, sizeof(FklVMvalueCodegenInfo));
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const InfoUserDataMetaTable = {
    // .size = sizeof(struct FklCodegenInfo),
    .size = sizeof(FklVMvalueCodegenInfo),
    .princ = info_print,
    .prin1 = info_print,
    .atomic = info_atomic,
    .finalize = info_finalizer,
};

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

    r->curline = filename == NULL && prev ? prev->curline : 1;

    r->ctx = ctx;

    r->exports.buckets = NULL;
    r->is_lib = is_lib;
    r->is_macro = is_macro;

    r->export_macro = NULL;
    r->export_replacement = is_lib ? fklReplacementHashMapCreate() : NULL;
    r->export_named_prod_groups = is_lib ? fklValueHashSetCreate() : NULL;
    r->exports.buckets = NULL;

    r->work_cb = work_cb;
    r->env_work_cb = env_work_cb;
    r->work_ctx = work_ctx;

    r->libraries = libs;
    r->pts = pts;
    // r->lnt = fklCreateVMvalueCodegenLnt(&ctx->gc->gcvm);

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

static const FklVMudMetaTable CustomActionCtxUdMetaTable;

static FKL_ALWAYS_INLINE int is_custom_ctx(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->mt_ == &CustomActionCtxUdMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueCustomActionCtx *as_custom_ctx(
        const FklVMvalue *v) {
    FKL_ASSERT(is_custom_ctx(v));
    return FKL_TYPE_CAST(FklVMvalueCustomActionCtx *, v);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(custom_action_ctx_ud_as_print,
        "custom-action-ctx")

static void custom_action_ctx_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCustomActionCtx *c = as_custom_ctx(ud);
    // FKL_DECL_UD_DATA(c, struct FklCustomActionCtx, ud);
    if (c->bcl == NULL)
        return;
    fklVMgcToGray(c->bcl, gc);
}

static int custom_action_ctx_ud_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCustomActionCtx *c = as_custom_ctx(ud);
    // FKL_DECL_UD_DATA(c, struct FklCustomActionCtx, ud);

    c->bcl = NULL;

    return FKL_VM_UD_FINALIZE_NOW;
}

static const FklVMudMetaTable CustomActionCtxUdMetaTable = {
    // .size = sizeof(struct FklCustomActionCtx),
    .size = sizeof(FklVMvalueCustomActionCtx),
    .atomic = custom_action_ctx_ud_atomic,
    .prin1 = custom_action_ctx_ud_as_print,
    .princ = custom_action_ctx_ud_as_print,
    .finalize = custom_action_ctx_ud_finalizer,
};

static void *custom_action(void *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMparseCtx *pctx = FKL_TYPE_CAST(FklVMparseCtx *, ctx);
    FklVMvalueCustomActionCtx *action_ctx = c;
    FklCodegenCtx *cg_ctx = action_ctx->ctx;
    FklVMvalue *nodes_vector = fklCreateVMvalueVec(&cg_ctx->gc->gcvm, line);
    // nodes_vector->vec = fklCreateNastVector(num);
    for (size_t i = 0; i < num; i++)
        FKL_VM_VEC(nodes_vector)->base[i] = nodes[i].ast;
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

    put_line_number(pctx->ln, nodes_vector, line);
    {
        FklStringBuffer buf = { 0 };
        fklInitStringBuffer(&buf);
        for (size_t i = 0; i < num; ++i) {
            fklStringBufferPrintf(&buf, "$%zu", i);
            FklVMvalue *s = add_symbol_char_buf(cg_ctx, buf.buf, buf.index);
            fklPmatchHashMapAdd2(&ht,
                    s,
                    (FklPmatchRes){ .value = nodes[i].ast,
                        .container = nodes_vector });
            fklStringBufferClear(&buf);
        }
        fklUninitStringBuffer(&buf);
    }
    fklPmatchHashMapAdd2(&ht,
            add_symbol_cstr(cg_ctx, "$$"),
            // fklAddSymbolCstr("$$", action_ctx->pst),
            (FklPmatchRes){ .value = nodes_vector, .container = nodes_vector });
    fklPmatchHashMapAdd2(&ht,
            add_symbol_cstr(cg_ctx, "line"),
            // fklAddSymbolCstr("line", action_ctx->pst),
            (FklPmatchRes){ .value = line_node, .container = nodes_vector });

    FklVMvalue *r = NULL;
    const char *cwd = cg_ctx->cwd;
    const char *file_dir = cg_ctx->cur_file_dir;
    fklChdir(file_dir);

    FklVM *exe = init_macro_expand_vm(cg_ctx,
            action_ctx->bcl,
            action_ctx->prototype_id,
            &ht,
            pctx->ln,
            &r,
            line,
            cg_ctx->error_state);
    FklVMgc *gc = exe->gc;

    int e = fklRunVMidleLoop(exe);
    fklMoveThreadObjectsToGc(exe, gc);

    fklChdir(cwd);

    if (e)
        fklDeleteCallChain(exe);

    // fklDestroyNastNode(nodes_vector);
    // fklDestroyNastNode(line_node);

    fklPmatchHashMapUninit(&ht);
    fklDestroyAllVMs(exe);
    return r;
}

static inline FklVMvalue *create_custom_prod_action_ctx(FklCodegenCtx *cg_ctx,
        uint32_t prototypeId) {
    FklVMvalueCustomActionCtx *v =
            (FklVMvalueCustomActionCtx *)fklCreateVMvalueUd(&cg_ctx->gc->gcvm,
                    &CustomActionCtxUdMetaTable,
                    NULL);

    v->ctx = cg_ctx;
    v->prototype_id = prototypeId;

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

static void *simple_action_list(void *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMvalue *vec = c;
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
        *pr = fklCreateVMvaluePairWithCar(ct->exe, s->ast);
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
    return cc->mt->func((void *)cc->vec, ctx, nodes, num, line);
}

static inline void init_simple_prod_action_list(FklCodegenCtx *ctx) {
    FklVMvalue **const simple_prod_action_id = ctx->simple_prod_action_id;
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++)
        simple_prod_action_id[i] =
                add_symbol_cstr(ctx, CodegenProdCreatorActions[i].name);
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
    } else if (num == 2) {
        FklVMvalue *car = nodes[0].ast;
        FklVMvalue *cdr = nodes[1].ast;
        FklVMvalue *pair = fklCreateVMvaluePair(c->exe, car, cdr);
        put_line_number(c->ln, pair, line);
        return pair;
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

static inline const struct FklSimpleProdAction *
find_simple_prod_action(FklVMvalue *id, FklVMvalue *simple_prod_action_id[]) {
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++) {
        if (simple_prod_action_id[i] == id)
            return &CodegenProdCreatorActions[i];
    }
    return NULL;
}

static const FklVMudMetaTable SimpleActionCtxUdMetaTable;
static FKL_ALWAYS_INLINE int is_simple_ctx(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->mt_ == &SimpleActionCtxUdMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueSimpleActionCtx *as_simple_ctx(
        const FklVMvalue *v) {
    FKL_ASSERT(is_simple_ctx(v));
    return FKL_TYPE_CAST(FklVMvalueSimpleActionCtx *, v);
}

static void simple_action_ctx_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(c, struct FklSimpleActionCtx, ud);
    FklVMvalueSimpleActionCtx *c = as_simple_ctx(ud);
    fklVMgcToGray(c->vec, gc);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(simple_action_ctx_ud_as_print,
        "simple-action-ctx")

static int simple_action_ctx_ud_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueSimpleActionCtx *c = as_simple_ctx(ud);
    // FKL_DECL_UD_DATA(c, struct FklSimpleActionCtx, ud);
    // if (c->mt == NULL)
    //     return FKL_VM_UD_FINALIZE_NOW;

    c->mt = NULL;
    c->vec = NULL;
    return FKL_VM_UD_FINALIZE_NOW;
}

static const FklVMudMetaTable SimpleActionCtxUdMetaTable = {
    // .size = sizeof(FklSimpleActionCtx),
    .size = sizeof(FklVMvalueSimpleActionCtx),
    .atomic = simple_action_ctx_ud_atomic,
    .prin1 = simple_action_ctx_ud_as_print,
    .princ = simple_action_ctx_ud_as_print,
    .finalize = simple_action_ctx_ud_finalizer,
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
    v->mt = mt;
    v->vec = action_ast;

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
