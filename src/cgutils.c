#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_lw.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/regex.h>
#include <fakeLisp/str_buf.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include "codegen.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

// FklCgLibVector
#define FKL_VECTOR_ELM_TYPE FklCgLib
#define FKL_VECTOR_ELM_TYPE_NAME CgLib
#include <fakeLisp/cont/vector.h>

FKL_VM_DEF_UD_STRUCT(FklVMvalueCgLibs, { FklCgLibVector libs; });

static FklVM *init_macro_expand_vm(FklCgCtx *ctx,
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

FklSymDefHashMapElm *fklFindSymbolDefByIdAndScope(FklVMvalue *id,
        uint32_t scope,
        const FklCgEnvScope *scopes) {
    for (; scope; scope = scopes[scope - 1].p) {
        FklSymDefHashMapElm *r =
                get_def_by_id_in_scope(id, scope, &scopes[scope - 1]);
        if (r)
            return r;
    }
    return NULL;
}

FklSymDefHashMapElm *fklGetCgDefByIdInScope(FklVMvalue *id,
        uint32_t scope,
        const FklCgEnvScope *scopes) {
    return get_def_by_id_in_scope(id, scope, &scopes[scope - 1]);
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
            (FklSidScope){ .id = id, .scope = env->parent_scope },
            (FklSymDef){ .idx = idx, .cidx = idx, .isLocal = 0, .isConst = 0 });
}

static inline void *has_outer_pdef_or_def(FklVMvalueCgEnv *cur,
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
        FklSymDefHashMapElm *def =
                fklFindSymbolDefByIdAndScope(id, scope, cur->scopes);
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
            (FklSidScope){ .id = id, .scope = cur->parent_scope },
            (FklSymDef){ .idx = idx,
                .cidx = idx,
                .isConst = isConst,
                .isLocal = 0 });
    *new_ref = cel;
    FklSidScope key = { .id = id, .scope = cur->parent_scope };
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
    r->scope = scope;
    r->prototypeId = prototypeId;
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

FklSymDef *fklGetCgRefBySid(FklVMvalue *id, FklVMvalueCgEnv *env) {
    FklSymDefHashMap *ht = &env->refs;
    return fklSymDefHashMapGet2(ht, (FklSidScope){ id, env->parent_scope });
}

static inline FklUnReSymbolRef *
has_resolvable_ref(FklVMvalue *id, uint32_t scope, const FklVMvalueCgEnv *env) {
    FklUnReSymbolRef *urefs = env->uref.base;
    uint32_t top = env->uref.size;
    for (uint32_t i = 0; i < top; i++) {
        FklUnReSymbolRef *cur = &urefs[i];
        if (cur->id == id && cur->scope == scope)
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
    fklPredefHashMapEarase(&env->pdef, &key, &pdef_isconst, NULL);
    FklSymDefHashMapElm *def = fklGetCgDefByIdInScope(id, scope, env->scopes);
    FKL_ASSERT(def);
    for (uint32_t i = 0; i < count; i++) {
        const FklPreDefRef *pdef_ref = &ref_pdef->base[i];
        if (pdef_ref->id == id && pdef_ref->scope == scope) {
            FklVMvalue *pt_v = child_proc_protos->base[pdef_ref->prototypeId];
            FKL_ASSERT(pt_v && fklIsVMvalueProto(pt_v));

            FklVMvalueProto *cpt = fklVMvalueProto(pt_v);
            FklVarRefDef *ref = &fklVMvalueProtoVarRefs(cpt)[pdef_ref->idx];
            ref->cidx = FKL_MAKE_VM_FIX(def->v.idx);
            env->slot_flags[def->v.idx] = FKL_CODEGEN_ENV_SLOT_REF;
        } else
            fklPreDefRefVectorPushBack(&ref_pdef1, pdef_ref);
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
        FklUnReSymbolRef *ref = has_resolvable_ref(id,
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
            void *targetDef = has_outer_pdef_or_def(prev,
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
                    FklSymDefHashMapElm *def =
                            FKL_TYPE_CAST(FklSymDefHashMapElm *, targetDef);
                    FklSymDefHashMapElm *cel = add_ref_to_all_penv(id,
                            env,
                            targetEnv,
                            def->v.isConst,
                            &ret);
                    cel->v.isLocal = 1;
                    cel->v.cidx = def->v.idx;
                    uint8_t *slot_flags = targetEnv->slot_flags;
                    slot_flags[def->v.idx] = FKL_CODEGEN_ENV_SLOT_REF;
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
                            (FklSidScope){ .id = id,
                                .scope = env->parent_scope },
                            (FklSymDef){ .idx = idx, .cidx = idx });
                    ret->v.cidx = FKL_VAR_REF_INVALID_CIDX;

                    initUnReSymbolRef(
                            fklUnReSymbolRefVectorPushBack(&prev->uref, NULL),
                            id,
                            idx,
                            env->parent_scope,
                            env->proto_id,
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
                    env->proto_id,
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

int fklIsSymbolDefined(FklVMvalue *id, uint32_t scope, FklVMvalueCgEnv *env) {
    return get_def_by_id_in_scope(id, scope, &env->scopes[scope - 1]) != NULL;
}

static inline uint32_t
get_next_empty(uint32_t empty, uint8_t *flags, uint32_t lcount) {
    for (; empty < lcount && flags[empty]; empty++)
        ;
    return empty;
}

FklSymDef *
fklAddCgDefBySid(FklVMvalue *id, uint32_t scopeId, FklVMvalueCgEnv *env) {
    FklCgEnvScope *scope = &env->scopes[scopeId - 1];
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
                    env->slot_flags,
                    env->lcount);
        el->idx = idx;
        uint32_t end = (idx + 1) - scope->start;
        if (scope->end < end)
            scope->end = end;
        if (idx >= env->lcount) {
            env->lcount = idx + 1;
            uint8_t *slotFlags = (uint8_t *)fklZrealloc(env->slot_flags,
                    env->lcount * sizeof(uint8_t));
            FKL_ASSERT(slotFlags);
            env->slot_flags = slotFlags;
        }
        env->slot_flags[idx] = FKL_CODEGEN_ENV_SLOT_OCC;
    }
    return el;
}

void fklResolveRef(FklVMvalueCgEnv *env,
        uint32_t scope,
        const FklResolveRefArgs *args) {
    int no_refs_to_builtins = args ? args->no_refs_to_builtins : 0;
    FklResolveRefToDefCb resolve_ref_to_def_cb =
            args ? args->resolve_ref_to_def_cb : NULL;
    FklVMvalueCgEnv *top_env = args ? args->top_env : NULL;

    FklUnReSymbolRefVector *urefs = &env->uref;
    const FklValueVector *child_proc_protos = &env->child_proc_protos;
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

        FklVMvalue *pt_v = child_proc_protos->base[uref->prototypeId];
        FKL_ASSERT(pt_v && fklIsVMvalueProto(pt_v));
        FklVMvalueProto *pt = fklVMvalueProto(pt_v);

        FklVarRefDef *const ref = &fklVMvalueProtoVarRefs(pt)[uref->idx];
        const FklSymDefHashMapElm *def = fklFindSymbolDefByIdAndScope(uref->id,
                uref->scope,
                env->scopes);

        if (def) {
            env->slot_flags[def->v.idx] = FKL_CODEGEN_ENV_SLOT_REF;
            ref->cidx = FKL_MAKE_VM_FIX(def->v.idx);
            ref->is_local = FKL_VM_TRUE;

            if (resolve_ref_to_def_cb) {
                resolve_ref_to_def_cb(ref,
                        def,
                        uref,
                        pt,
                        args->resolve_ref_to_def_cb_args);
            }
        } else if (env->scopes[uref->scope - 1].p) {
            uref->scope = env->scopes[uref->scope - 1].p;
            fklUnReSymbolRefVectorPushBack(&urefs1, uref);
        } else if (env->prev != top_env) {
            uint32_t cidx = fklAddCgRefBySidRetIndex(uref->id,
                    env,
                    uref->fid,
                    uref->line,
                    uref->assign);
            ref->cidx = FKL_MAKE_VM_FIX(cidx);
        } else {
            if (!no_refs_to_builtins) {
                fklAddCgBuiltinRefBySid(uref->id, env);
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

void fklInitVMlibWithCgLib(const FklCgLib *clib, FklVMlib *vlib, FklVM *exe) {
    FklVMvalue *val = FKL_VM_NIL;
    if (clib->type == FKL_CODEGEN_LIB_SCRIPT) {
        val = clib->proc;
    } else
        val = fklCreateVMvalueStr2(exe,
                strlen(clib->rp) - strlen(FKL_DLL_FILE_TYPE),
                clib->rp);
    fklInitVMlib(vlib, val, clib->epc);
}

static inline void uninit_codegen_lib_exports(FklCgLib *lib) {
    if (lib->exports.buckets)
        fklCgExportSidIdxHashMapUninit(&lib->exports);
    fklZfree(lib->rp);
    lib->rp = NULL;
}

static void uninit_cg_lib(FklCgLib *lib) {
    if (lib == NULL)
        return;
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        lib->proc = NULL;
        lib->epc = 0;
        break;
    case FKL_CODEGEN_LIB_DLL:
        uv_dlclose(&lib->dll);
        break;
    case FKL_CODEGEN_LIB_UNINIT:
        return;
        break;
    }
    uninit_codegen_lib_exports(lib);
    fklClearCgLibMacros(lib);
}

void fklInitCgDllLib(const FklCgCtx *ctx,
        FklCgLib *lib,
        char *rp,
        uv_lib_t dll,
        FklCgDllLibInitExportCb init) {
    memset(lib, 0, sizeof(*lib));
    lib->rp = rp;
    lib->type = FKL_CODEGEN_LIB_DLL;
    lib->dll = dll;

    uint32_t num = 0;
    FklVMvalue **exports = init(ctx->vm, &num);
    FklCgExportSidIdxHashMap *exports_idx = &lib->exports;
    fklCgExportSidIdxHashMapInit(exports_idx);
    if (num) {
        for (uint32_t i = 0; i < num; i++) {
            fklCgExportSidIdxHashMapAdd(exports_idx,
                    &exports[i],
                    &(FklCgExportIdx){ .idx = i });
        }
    }
    if (exports)
        fklZfree(exports);
}

void fklInitCgScriptLib(FklCgLib *lib,
        FklVMvalueCgInfo *info,
        FklVMvalue *proc,
        uint64_t epc) {
    memset(lib, 0, sizeof(*lib));

    lib->type = FKL_CODEGEN_LIB_SCRIPT;
    lib->proc = proc;
    lib->epc = epc;

    if (info == NULL) {
        lib->rp = NULL;
        lib->macros = NULL;
        lib->replacements = NULL;
        return;
    }
    lib->rp = info->realpath;
    lib->macros = info->export_macros;
    lib->replacements = info->export_replacement;
    lib->named_prod_groups = info->export_prod_groups;

    info->realpath = NULL;
    info->export_macros = NULL;
    info->export_replacement = NULL;
    info->export_prod_groups = NULL;

    FklCgExportSidIdxHashMap *exports_index = &lib->exports;
    fklCgExportSidIdxHashMapInit(exports_index);
    FklCgExportSidIdxHashMap *export_sid_set = &info->exports;
    for (const FklCgExportSidIdxHashMapNode *sid_idx_list =
                    export_sid_set->first;
            sid_idx_list;
            sid_idx_list = sid_idx_list->next) {
        fklCgExportSidIdxHashMapPut(exports_index,
                &sid_idx_list->k,
                &sid_idx_list->v);
    }
}

static const FklCgMacro *find_macro(FklVMvalue *exp,
        const FklVMvalueCgMacroScope *macro_scope,
        FklPmatchHashMap *pht) {
    if (exp == NULL || !FKL_IS_PAIR(exp))
        return NULL;

    for (; macro_scope; macro_scope = macro_scope->prev) {
        FklMacroHashMap *macros = macro_scope->macros;

        FklVMvalue *header = FKL_VM_CAR(exp);
        FklCgMacro *const *const pm = fklMacroHashMapGet2(macros, header);

        if (pm == NULL)
            continue;

        for (const FklCgMacro *cur = *pm; cur; cur = cur->next) {
            if (pht->buckets == NULL)
                fklPmatchHashMapInit(pht);
            if (fklPatternMatch(cur->pattern, exp, pht))
                return cur;

            fklPmatchHashMapClear(pht);
        }
    }
    return NULL;
}

void fklClearCgLibMacros(FklCgLib *lib) {
    if (lib->macros) {
        fklMacroHashMapDestroy(lib->macros);
        lib->macros = NULL;
    }
    if (lib->replacements) {
        fklReplacementHashMapDestroy(lib->replacements);
        lib->replacements = NULL;
    }
    if (lib->named_prod_groups) {
        fklGraProdGroupHashMapDestroy(lib->named_prod_groups);
        lib->named_prod_groups = NULL;
    }
}

void fklClearCgLibMacros2(const FklCgCtx *ctx) {
    FklCgLib *cur = ctx->libraries->libs.base;
    FklCgLib *end = &cur[ctx->libraries->libs.size];
    for (; cur < end; ++cur) {
        fklClearCgLibMacros(cur);
    }

    cur = ctx->macro_libraries->libs.base;
    end = &cur[ctx->macro_libraries->libs.size];
    for (; cur < end; ++cur) {
        fklClearCgLibMacros(cur);
    }
}

FklCgMacro *
fklCreateCgMacro(FklVMvalue *pattern, FklVMvalue *proc, FklCgMacro *next) {
    FKL_ASSERT(proc);
    FklCgMacro *r = (FklCgMacro *)fklZmalloc(sizeof(FklCgMacro));
    FKL_ASSERT(r);
    r->proc = NULL;
    r->pattern = pattern;
    r->proc = proc;
    r->next = next;
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

FklVMvalue *fklTryExpandCgMacroOnce(FklCgCtx *ctx,
        const FklPmatchRes *exp,
        const FklVMvalueCgInfo *codegen,
        const FklVMvalueCgMacroScope *macros) {
    FklCgErrorState *error_state = ctx->error_state;
    FklVMvalue *r = exp->value;
    if (!FKL_IS_PAIR(r))
        return r;
    FklPmatchHashMap ht = { .buckets = NULL };
    uint64_t curline = CURLINE(exp->container);
    for (const FklCgMacro *macro = find_macro(r, macros, &ht);
            !error_state->error && macro;
            macro = find_macro(r, macros, &ht)) {
        if (expand_all_macro_arg(ctx, &ht, codegen, macros))
            return NULL;

        fklPmatchHashMapAdd2(&ht,
                ctx->builtin_sym_orig,
                (FklPmatchRes){
                    .value = r,
                    .container = exp->container,
                });
        FklVMvalue *retval = NULL;

        const char *cwd = ctx->cwd;
        fklChdir(codegen->dir);

        FklVM *exe = init_macro_expand_vm(ctx,
                macro->proc,
                &ht,
                ctx->lnt,
                &retval,
                curline);
        FklVMgc *gc = exe->gc;
        int e = fklRunVMidleLoop(exe);
        fklMoveThreadObjectsToGc(exe, gc);

        fklChdir(cwd);

        if (e) {
            error_state->error = make_macroexpand_error(ctx->vm, r);
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
        break;
    }
    if (ht.buckets)
        fklPmatchHashMapUninit(&ht);
    return r;
}

FklVMvalue *fklTryExpandCgMacro(FklCgCtx *ctx,
        const FklPmatchRes *exp,
        const FklVMvalueCgInfo *codegen,
        const FklVMvalueCgMacroScope *macros) {
    FklCgErrorState *error_state = ctx->error_state;
    FklVMvalue *r = exp->value;
    if (!FKL_IS_PAIR(r))
        return r;
    FklPmatchHashMap ht = { .buckets = NULL };
    uint64_t curline = CURLINE(exp->container);
    for (const FklCgMacro *macro = find_macro(r, macros, &ht);
            !error_state->error && macro;
            macro = find_macro(r, macros, &ht)) {
        if (expand_all_macro_arg(ctx, &ht, codegen, macros))
            return NULL;

        fklPmatchHashMapAdd2(&ht,
                ctx->builtin_sym_orig,
                (FklPmatchRes){
                    .value = r,
                    .container = exp->container,
                });
        FklVMvalue *retval = NULL;

        const char *cwd = ctx->cwd;
        fklChdir(codegen->dir);

        FklVM *exe = init_macro_expand_vm(ctx,
                macro->proc,
                &ht,
                ctx->lnt,
                &retval,
                curline);
        FklVMgc *gc = exe->gc;
        int e = fklRunVMidleLoop(exe);
        fklMoveThreadObjectsToGc(exe, gc);

        fklChdir(cwd);

        if (e) {
            error_state->error = make_macroexpand_error(ctx->vm, r);
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
        FklVMframe *frame,
        FklPmatchHashMap *ht,
        FklVMvalueLnt *lnt,
        const FklVMvalueProto *pt) {
    FklVMCompoundFrameVarRef *lr = fklGetCompoundFrameLocRef(frame);
    FklVMvalueProc *proc = FKL_VM_PROC(fklGetCompoundFrameProc(frame));
    uint32_t count = pt->local_count;
    uint32_t idx = 0;
    for (FklPmatchHashMapNode *list = ht->first; list; list = list->next) {
        FklVMvalue *v = list->v.value;
        FKL_VM_GET_ARG(exe, frame, idx) = v;
        idx++;
    }
    lr->lcount = count;
    lr->lref = NULL;
    lr->lrefl = NULL;
    proc->ref_count = lr->rcount;
}

static inline void update_new_codegen_to_new_vm_lib(FklVM *vm,
        const FklCgLibVector *clibs,
        FklVMvalueLibs *libs) {
    if (libs->count != clibs->size) {
        uint64_t old_count = libs->count;
        fklVMvalueLibsReserve(libs, clibs->size);
        for (size_t i = old_count; i < clibs->size; ++i) {
            const FklCgLib *cur = &clibs->base[i];
            fklInitVMlibWithCgLib(cur, &libs->libs[i + 1], vm);
        }
    }
}

void fklUpdateVMlibsWithCgLibVector(FklVM *vm,
        FklVMvalueLibs *libs,
        const FklVMvalueCgLibs *clibs) {
    update_new_codegen_to_new_vm_lib(vm, &clibs->libs, libs);
}

static inline FklVM *init_macro_expand_vm(FklCgCtx *ctx,
        FklVMvalue *proc,
        FklPmatchHashMap *ht,
        FklVMvalueLnt *lnt,
        FklVMvalue **pr,
        uint64_t curline) {
    FKL_ASSERT(FKL_IS_PROC(proc));
    FklCgErrorState *error_state = ctx->error_state;

    FklVMvalueLibs *libs = ctx->macro_vm_libs;
    if (libs == NULL) {
        libs = fklCreateVMvalueLibs(ctx->vm);
        ctx->macro_vm_libs = libs;
    }

    FklVM *exe = fklCreateVM(NULL, ctx->vm->gc, libs);

    push_macro_expand_frame(exe, pr, lnt, curline, error_state);

    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, proc);
    fklCallObj(exe, proc);
    init_macro_match_local_variable(exe,
            exe->top_frame,
            ht,
            lnt,
            FKL_VM_PROC(proc)->proto);

    update_new_codegen_to_new_vm_lib(exe, &ctx->macro_libraries->libs, libs);
    return exe;
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
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ms->prev), gc);
    for (const FklMacroHashMapNode *cur = ms->macros->first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->k, gc);
        for (const FklCgMacro *m = cur->v; m; m = m->next) {
            fklVMgcToGray(m->pattern, gc);
            fklVMgcToGray(m->proc, gc);
        }
    }

    for (const FklReplacementHashMapNode *cur = ms->replacements->first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->k, gc);
        fklVMgcToGray(cur->v, gc);
    }
}

static int macro_scope_finalize(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgMacroScope *ms = as_macro_scope(ud);
    if (ms->macros) {
        fklMacroHashMapDestroy(ms->macros);
        ms->macros = NULL;
    }

    if (ms->replacements) {
        fklReplacementHashMapDestroy(ms->replacements);
        ms->replacements = NULL;
    }
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

    r->macros = fklMacroHashMapCreate();
    r->replacements = fklReplacementHashMapCreate();
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

static void env_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgEnv *e = as_env(ud);
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

    for (size_t i = 0; i < e->child_proc_protos.size; ++i) {
        fklVMgcToGray(e->child_proc_protos.base[i], gc);
    }
}

static int env_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgEnv *cur = as_env(ud);
    uint32_t sc = cur->scope;
    FklCgEnvScope *scopes = cur->scopes;
    for (uint32_t i = 0; i < sc; i++)
        fklSymDefHashMapUninit(&scopes[i].defs);
    fklZfree(scopes);
    fklZfree(cur->slot_flags);
    cur->scopes = NULL;
    cur->slot_flags = NULL;

    fklSymDefHashMapUninit(&cur->refs);
    FklUnReSymbolRefVector *unref = &cur->uref;
    fklUnReSymbolRefVectorUninit(unref);

    fklPredefHashMapUninit(&cur->pdef);
    fklPreDefRefVectorUninit(&cur->ref_pdef);

    fklUninitValueTable(&cur->konsts);
    fklValueVectorUninit(&cur->child_proc_protos);
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
    if (env->env_work_cb)
        env->env_work_cb(env, env->work_ctx);

    FklVMvalueCgEnv *parent_env = env->prev;
    if (parent_env == NULL)
        return;

    FklValueVector *child_proc_protos = &parent_env->child_proc_protos;

    env->proto_id = child_proc_protos->size;
    fklValueVectorPushBack2(child_proc_protos, FKL_VM_NIL);
}

FklVMvalueCgEnv *fklCreateVMvalueCgEnv(const FklCgCtx *c,
        const FklCgEnvCreateArgs *args) {
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
    r->scope = 0;
    r->scopes = NULL;
    enter_new_scope(0, r);
    r->proto_id = UINT32_MAX;
    r->prev = prev_env;
    r->lcount = 0;
    r->slot_flags = NULL;
    r->is_debugging = 0;
    fklSymDefHashMapInit(&r->refs);
    fklUnReSymbolRefVectorInit(&r->uref, 8);
    fklPredefHashMapInit(&r->pdef);
    fklPreDefRefVectorInit(&r->ref_pdef, 8);
    r->macros = fklCreateVMvalueCgMacroScope(c, prev_ms);
    fklInitValueTable(&r->konsts);
    fklValueVectorInit(&r->child_proc_protos, 4);

    if (prev_env) {
        r->work_ctx = prev_env->work_ctx;
        r->env_work_cb = prev_env->env_work_cb;
    }

    insert_proto_to_parent(r);
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
int fklIsVMvalueCgInfo(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &InfoUserDataMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueCgInfo *as_info(const FklVMvalue *r) {
    FKL_ASSERT(fklIsVMvalueCgInfo(r));
    return FKL_TYPE_CAST(FklVMvalueCgInfo *, r);
}

static void info_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgInfo *e = as_info(ud);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->lnt), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->prev), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, e->global_env), gc);

    fklVMgcToGray(e->fid, gc);

    if (e->export_replacement) {
        for (const FklReplacementHashMapNode *cur =
                        e->export_replacement->first;
                cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            fklVMgcToGray(cur->v, gc);
        }
    }

    if (e->export_macros) {
        for (const FklMacroHashMapNode *cur = e->export_macros->first; cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            for (const FklCgMacro *m = cur->v; m; m = m->next) {
                fklVMgcToGray(m->pattern, gc);
                fklVMgcToGray(m->proc, gc);
            }
        }
    }

    if (e->export_prod_groups) {
        for (FklGraProdGroupHashMapNode *cur = e->export_prod_groups->first;
                cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            fklVMgcMarkGrammer(gc, &cur->v.g, NULL);
        }
    }

    if (e->prod_groups == &e->self_prod_groups) {
        for (FklGraProdGroupHashMapNode *cur = e->prod_groups->first; cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            fklVMgcMarkGrammer(gc, &cur->v.g, NULL);
        }
    }
}

static int info_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgInfo *i = as_info(ud);

    fklZfree(i->dir);
    if (i->filename)
        fklZfree(i->filename);
    if (i->realpath)
        fklZfree(i->realpath);

    fklCgExportSidIdxHashMapUninit(&i->exports);
    if (i->export_macros)
        fklMacroHashMapDestroy(i->export_macros);
    if (i->export_prod_groups)
        fklGraProdGroupHashMapDestroy(i->export_prod_groups);

    if (i->export_replacement)
        fklReplacementHashMapDestroy(i->export_replacement);

    if (i->g == &i->self_g && *i->g) {
        FklGrammer *g = *i->g;
        fklUninitGrammer(g);
        fklGraProdGroupHashMapUninit(i->prod_groups);
        fklZfree(g);
    }

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
    int is_global = args == NULL ? 0 : args->is_global;

    FklCgInfoWorkCb work_cb = args ? args->work_cb
                            : prev ? prev->work_cb
                                   : NULL;

    FklCgInfoEnvWorkCb env_work_cb = args ? args->env_work_cb
                                   : prev ? prev->global_env->env_work_cb
                                          : NULL;
    void *work_ctx = args ? args->work_ctx //
                   : prev ? prev->work_ctx
                          : NULL;

    FKL_ASSERT(prev == NULL || fklIsVMvalueCgInfo((FklVMvalue *)prev));

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

    if (filename != NULL) {
        r->dir = fklGetDir(rp);
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

    r->export_macros = is_lib ? fklMacroHashMapCreate() : NULL;
    r->export_replacement = is_lib ? fklReplacementHashMapCreate() : NULL;
    r->export_prod_groups = is_lib ? fklGraProdGroupHashMapCreate() : NULL;
    if (is_lib)
        fklCgExportSidIdxHashMapInit(&r->exports);
    else
        r->exports.buckets = NULL;

    r->work_cb = work_cb;
    r->work_ctx = work_ctx;

    r->libraries = libs;

    if (args->inherit_grammer && prev) {
        r->g = &prev->self_g;
        r->prod_groups = &prev->self_prod_groups;
    } else {
        r->g = &r->self_g;
        r->prod_groups = &r->self_prod_groups;
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
                    .name = NULL,
                    .line = r->curline,
                });
        fklInitGlobCgEnv(r->global_env, ctx->vm, args->is_precompile);
    }

    r->global_env->work_ctx = work_ctx;
    r->global_env->env_work_cb = env_work_cb;

    if (r->work_cb)
        r->work_cb(r, r->work_ctx);

    FklVMvalueCgEnv *main_env = NULL;
    if (is_global) {
        FklVMvalueCgMacroScope *macros = r->global_env->macros;

        main_env = fklCreateVMvalueCgEnv(ctx,
                &(FklCgEnvCreateArgs){
                    .prev_env = r->global_env,
                    .prev_ms = macros,
                    .parent_scope = 1,
                    .filename = r->fid,
                    .name = NULL,
                    .line = r->curline,
                });
        ctx->global_env = main_env;
        ctx->global_info = r;
    }

    return r;
}

static const FklVMudMetaTable CustomActionCtxUdMetaTable;

static FKL_ALWAYS_INLINE int is_custom_ctx(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->mt_ == &CustomActionCtxUdMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueCustomActCtx *as_custom_ctx(
        const FklVMvalue *v) {
    FKL_ASSERT(is_custom_ctx(v));
    return FKL_TYPE_CAST(FklVMvalueCustomActCtx *, v);
}

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

static void *custom_action(void *c,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMparseCtx *pctx = FKL_TYPE_CAST(FklVMparseCtx *, ctx);
    FklCgCtx *cg_ctx = pctx->opaque;
    FKL_ASSERT(cg_ctx);
    FklVMvalueCustomActCtx *action_ctx = c;
    FklVMvalue *nodes_vector = fklCreateVMvalueVec(cg_ctx->vm, line);
    for (size_t i = 0; i < num; i++)
        FKL_VM_VEC(nodes_vector)->base[i] = nodes[i].ast;
    FklPmatchHashMap ht;
    fklPmatchHashMapInit(&ht);
    FklVMvalue *line_node = fklMakeVMuint(line, cg_ctx->vm);

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
    const char *cwd = cg_ctx->cwd;
    const char *file_dir = cg_ctx->cur_file_dir;
    fklChdir(file_dir);

    FklVM *exe = init_macro_expand_vm(cg_ctx,
            action_ctx->proc,
            &ht,
            pctx->ln,
            &r,
            line);
    FklVMgc *gc = exe->gc;

    int e = fklRunVMidleLoop(exe);
    fklMoveThreadObjectsToGc(exe, gc);

    fklChdir(cwd);

    if (e)
        fklDeleteCallChain(exe);

    fklPmatchHashMapUninit(&ht);
    fklDestroyAllVMs(exe);
    return r;
}

static inline FklVMvalue *create_custom_prod_action_ctx(FklCgCtx *cg_ctx,
        size_t actual_len) {
    FklVMvalueCustomActCtx *v =
            (FklVMvalueCustomActCtx *)fklCreateVMvalueUd2(cg_ctx->vm,
                    &CustomActionCtxUdMetaTable,
                    actual_len * sizeof(v->dollers[0]),
                    NULL);

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

    return FKL_TYPE_CAST(FklVMvalue *, v);
}

FklGrammerProduction *fklCreateCustomActionProd(FklCgCtx *cg_ctx,
        struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms) {
    FklVMvalue *action_ctx = create_custom_prod_action_ctx(cg_ctx,
            fklComputeProdActualLen(len, syms));

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
                .func = simple_action_bytevector,
                .check = simple_action_symbol_check,
            },
        };

static void *simple_action(void *actx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklVMvalueSimpleActCtx *cc = actx;
    return cc->mt->func((void *)cc->vec, ctx, nodes, num, line);
}

static inline void init_simple_prod_action_list(FklCgCtx *ctx) {
    FklVMvalue **const simple_prod_action_id = ctx->simple_prod_action_id;
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++)
        simple_prod_action_id[i] =
                add_symbol_cstr(ctx, CgProdCreatorActions[i].name);
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

    return FKL_VM_NIL;
}

static void *builtin_prod_action_first(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 1)
        return NULL;
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

    return nodes[1].ast;
}

static void *builtin_prod_action_third(void *action_ctx,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 3)
        return NULL;

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

static inline FklProdActionFunc find_builtin_prod_action(FklCgCtx *ctx,
        FklVMvalue *id) {
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++) {
        if (ctx->builtin_prod_action_id[i] == id)
            return BuiltinProdActions[i].func;
    }
    return NULL;
}

FklGrammerProduction *fklCreateBuiltinActionProd(FklCgCtx *ctx,
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
            return &CgProdCreatorActions[i];
    }
    return NULL;
}

static const FklVMudMetaTable SimpleActionCtxUdMetaTable;
static FKL_ALWAYS_INLINE int is_simple_ctx(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->mt_ == &SimpleActionCtxUdMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueSimpleActCtx *as_simple_ctx(
        const FklVMvalue *v) {
    FKL_ASSERT(is_simple_ctx(v));
    return FKL_TYPE_CAST(FklVMvalueSimpleActCtx *, v);
}

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

static inline FklVMvalue *create_simple_prod_action_ctx(FklCgCtx *cg_ctx,
        FklVMvalue *action_ast) {
    const FklSimpleProdAction *mt =
            find_simple_prod_action(FKL_VM_VEC(action_ast)->base[0],
                    cg_ctx->simple_prod_action_id);
    if (mt == NULL)
        return NULL;

    FklVMvalue **rest = &FKL_VM_VEC(action_ast)->base[1];
    size_t rest_len = FKL_VM_VEC(action_ast)->size - 1;

    FklVMvalueSimpleActCtx *v =
            (FklVMvalueSimpleActCtx *)fklCreateVMvalueUd(cg_ctx->vm,
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

FklGrammerProduction *fklCreateSimpleActionProd(FklCgCtx *cg_ctx,
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

FklGrammerProduction *fklCreateExtraStartProduction(const FklCgCtx *ctx,
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

static FklVMudMetaTable const CgLibsUserDataMetaTable;

int fklIsVMvalueCgLibs(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &CgLibsUserDataMetaTable;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(cg_libs_print, "cg-libs");

static inline void mark_codegen_lib(FklVMgc *gc, const FklCgLib *lib) {
    for (const FklCgExportSidIdxHashMapNode *cur = lib->exports.first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->k, gc);
    }
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        fklVMgcToGray(lib->proc, gc);

        for (const FklMacroHashMapNode *cur = lib->macros->first; cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            for (const FklCgMacro *m = cur->v; m; m = m->next) {
                fklVMgcToGray(m->pattern, gc);
                fklVMgcToGray(m->proc, gc);
            }
        }

        for (const FklReplacementHashMapNode *cur = lib->replacements->first;
                cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            fklVMgcToGray(cur->v, gc);
        }

        for (const FklGraProdGroupHashMapNode *cur =
                        lib->named_prod_groups->first;
                cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            fklVMgcMarkGrammer(gc, &cur->v.g, NULL);
        }

        break;
    case FKL_CODEGEN_LIB_DLL:
        break;
    case FKL_CODEGEN_LIB_UNINIT:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void mark_codegen_lib_vector(FklVMgc *gc,
        const FklCgLibVector *libs) {
    for (size_t i = 0; i < libs->size; ++i) {
        mark_codegen_lib(gc, &libs->base[i]);
    }
}

static FklVMvalueCgLibs *as_cg_libs(const FklVMvalue *ud) {
    FKL_ASSERT(fklIsVMvalueCgLibs(ud));
    return FKL_TYPE_CAST(FklVMvalueCgLibs *, ud);
}

static void cg_libs_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgLibs *libs = as_cg_libs(ud);
    mark_codegen_lib_vector(gc, &libs->libs);
}

static int cg_libs_finalize(FklVMvalue *ud, FklVMgc *gc) {
    FklVMvalueCgLibs *libs = as_cg_libs(ud);
    FklCgLibVector *l = &libs->libs;
    while (!fklCgLibVectorIsEmpty(l))
        uninit_cg_lib(fklCgLibVectorPopBackNonNull(l));

    fklCgLibVectorUninit(l);

    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const CgLibsUserDataMetaTable = {
    .size = sizeof(FklVMvalueCgLibs),
    .princ = cg_libs_print,
    .prin1 = cg_libs_print,
    .atomic = cg_libs_atomic,
    .finalize = cg_libs_finalize,
};

FklVMvalueCgLibs *fklCreateVMvalueCgLibs(FklVM *vm) {
    FklVMvalueCgLibs *r = (FklVMvalueCgLibs *)fklCreateVMvalueUd(vm,
            &CgLibsUserDataMetaTable,
            NULL);

    fklCgLibVectorInit(&r->libs, 8);
    return r;
}

FklVMvalueCgLibs *fklCreateVMvalueCgLibs1(FklVM *vm, size_t num) {
    FklVMvalueCgLibs *r = (FklVMvalueCgLibs *)fklCreateVMvalueUd(vm,
            &CgLibsUserDataMetaTable,
            NULL);

    fklCgLibVectorInit(&r->libs, num);
    return r;
}

size_t fklVMvalueCgLibsFind(const FklVMvalueCgLibs *libs,
        const char *realpath) {
    FKL_ASSERT(fklIsVMvalueCgLibs(FKL_TYPE_CAST(FklVMvalue *, libs)));
    const FklCgLibVector *l = &libs->libs;
    for (size_t i = 0; i < l->size; i++) {
        const FklCgLib *cur = &l->base[i];
        if (!strcmp(realpath, cur->rp))
            return i + 1;
    }
    return 0;
}

size_t fklVMvalueCgLibsLastId(const FklVMvalueCgLibs *libs) {
    FKL_ASSERT(fklIsVMvalueCgLibs(FKL_TYPE_CAST(FklVMvalue *, libs)));
    return libs->libs.size;
}

size_t fklVMvalueCgLibsNextId(const FklVMvalueCgLibs *libs) {
    FKL_ASSERT(fklIsVMvalueCgLibs(FKL_TYPE_CAST(FklVMvalue *, libs)));
    return libs->libs.size + 1;
}

FklCgLib *fklVMvalueCgLibsLast(const FklVMvalueCgLibs *libs) {
    FKL_ASSERT(fklIsVMvalueCgLibs(FKL_TYPE_CAST(FklVMvalue *, libs)));
    return &libs->libs.base[libs->libs.size - 1];
}

FklCgLib *fklVMvalueCgLibsPushBack(FklVMvalueCgLibs *v) {
    FKL_ASSERT(fklIsVMvalueCgLibs(FKL_TYPE_CAST(FklVMvalue *, v)));
    return fklCgLibVectorPushBack(&v->libs, NULL);
}

FklCgLib *fklVMvalueCgLibsEmplaceBack(FklVMvalueCgLibs *v, FklCgLib *l) {
    FKL_ASSERT(fklIsVMvalueCgLibs(FKL_TYPE_CAST(FklVMvalue *, v)));
    return fklCgLibVectorPushBack(&v->libs, l);
}

FklCgLib *fklVMvalueCgLibsGet(FklVMvalueCgLibs *v, size_t id) {
    FKL_ASSERT(fklIsVMvalueCgLibs(FKL_TYPE_CAST(FklVMvalue *, v)));
    FKL_ASSERT(id > 0 && id <= v->libs.size);
    return &v->libs.base[id - 1];
}

FklCgLib *fklVMvalueCgLibs(FklVMvalueCgLibs *v) {
    FKL_ASSERT(fklIsVMvalueCgLibs(FKL_TYPE_CAST(FklVMvalue *, v)));
    return v->libs.base;
}

size_t fklVMvalueCgLibsCount(FklVMvalueCgLibs *v) {
    FKL_ASSERT(fklIsVMvalueCgLibs(FKL_TYPE_CAST(FklVMvalue *, v)));
    return v->libs.size;
}

void fklVMvalueCgLibsMerge(FklVMvalueCgLibs *out_v, FklVMvalueCgLibs *in_v) {
    FKL_ASSERT(fklIsVMvalueCgLibs(FKL_TYPE_CAST(FklVMvalue *, out_v)));
    FKL_ASSERT(fklIsVMvalueCgLibs(FKL_TYPE_CAST(FklVMvalue *, in_v)));

    FklCgLibVector *in = &in_v->libs;
    FklCgLibVector *out = &out_v->libs;

    for (uint32_t i = 0; i < in->size; i++)
        fklCgLibVectorPushBack(out, &in->base[i]);

    in->size = 0;
}

FklVMvalueProto *fklCreateVMvalueProto2(FklVM *exe, FklVMvalueCgEnv *env) {
    FKL_ASSERT(env->pdef.count == 0);
    fklResolveRef(env, 1, NULL);

    uint32_t ref_count = env->refs.count;
    uint32_t ref_offset = 0; // 固定等于 0

    uint32_t local_count = env->lcount;

    uint32_t konsts_count = env->konsts.ht.count;

    uint32_t konsts_offset = (ref_count * FKL_VAR_REF_DEF_MEMBER_COUNT) //
                           + ref_offset;

    uint32_t child_proto_count = env->child_proc_protos.size;

    uint32_t child_proto_offset = konsts_offset + konsts_count;

    uint32_t total_val_count = (ref_count * FKL_VAR_REF_DEF_MEMBER_COUNT) //
                             + konsts_count                               //
                             + child_proto_count;

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

    FklVMvalue **const vals = proto->vals;

    FklVarRefDef *const refs = (FklVarRefDef *)&vals[proto->ref_offset];

    for (const FklSymDefHashMapNode *l = env->refs.first; l; l = l->next) {
        FklVMvalue *sid = NULL;
        if (env->is_debugging || l->v.cidx == FKL_VAR_REF_INVALID_CIDX) {
            sid = l->k.id;
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

    update_parent_env_proto(env, FKL_VM_VAL(proto));
    return proto;
}
