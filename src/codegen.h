#ifndef FKL_SRC_CODEGEN_H
#define FKL_SRC_CODEGEN_H

#include <fakeLisp/codegen.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/vm.h>
#include <stdint.h>

static inline uint32_t enter_new_scope(uint32_t p, FklVMvalueCodegenEnv *env) {
    uint32_t r = ++env->sc;
    FklCodegenEnvScope *scopes = (FklCodegenEnvScope *)fklZrealloc(env->scopes,
            r * sizeof(FklCodegenEnvScope));
    FKL_ASSERT(scopes);
    env->scopes = scopes;
    FklCodegenEnvScope *newScope = &scopes[r - 1];
    newScope->p = p;
    fklSymDefHashMapInit(&newScope->defs);
    newScope->start = 0;
    newScope->end = 0;
    if (p)
        newScope->start = scopes[p - 1].start + scopes[p - 1].end;
    newScope->empty = newScope->start;
    return r;
}

static inline FklGrammerProdGroupItem *add_production_group(
        FklGraProdGroupHashMap *named_prod_groups,
        FklVMgc *gc,
        FklVMvalue *group_id) {
    FklGrammerProdGroupItem *group =
            fklGraProdGroupHashMapAdd1(named_prod_groups, group_id);
    if ((group->flags & FKL_GRAMMER_GROUP_INITED) == 0) {
        fklInitEmptyGrammer(&group->g, &gc->gcvm);
        fklInitStringTable(&group->delimiters);
        group->flags |= FKL_GRAMMER_GROUP_INITED;
    }
    return group;
}

static inline void merge_group(FklGrammerProdGroupItem *group,
        const FklGrammerProdGroupItem *other,
        const FklRecomputeGroupIdArgs *args) {
    for (const FklStrHashSetNode *cur = other->delimiters.first; cur;
            cur = cur->next)
        fklAddString(&group->delimiters, cur->k);
    fklMergeGrammer(&group->g, &other->g, args);
}

static inline void uninit_codegen_macro(FklCodegenMacro *macro) {
    macro->pattern = NULL;
    fklDestroyByteCodelnt(macro->bcl);
    macro->bcl = NULL;
}

static inline void create_and_insert_to_pool(FklVMvalueCodegenInfo *info,
        uint32_t p,
        FklVMvalueCodegenEnv *env,
        FklVMvalue *sid,
        uint32_t line) {
    FklVMvalue *fid = info->fid;
    FklFuncPrototypes *cp = &info->pts->p;
    cp->count += 1;
    FklFuncPrototype *pts = (FklFuncPrototype *)fklZrealloc(cp->pa,
            (cp->count + 1) * sizeof(FklFuncPrototype));
    FKL_ASSERT(pts);
    cp->pa = pts;
    FklFuncPrototype *cpt = &pts[cp->count];
    memset(cpt, 0, sizeof(FklFuncPrototype));
    env->prototypeId = cp->count;
    cpt->lcount = env->lcount;

    FKL_ASSERT(cpt == &cp->pa[env->prototypeId]);
    fklUpdatePrototypeRef(cp, env);

    cpt->name = sid;
    cpt->file = fid;
    cpt->line = line;

    if (info->env_work_cb)
        info->env_work_cb(info, env, info->work_ctx);
}

static inline void
put_line_number(FklVMvalueLnt *ln, FklVMvalue *v, uint64_t line) {
    if (ln)
        fklVMvalueLntPut(ln, v, line);
}

static inline FklVMvalue *add_symbol_cstr(FklCodegenCtx *c, const char *s) {
    return fklVMaddSymbolCstr(&c->gc->gcvm, s);
}

static inline FklVMvalue *
add_symbol_char_buf(FklCodegenCtx *c, const char *s, size_t l) {
    return fklVMaddSymbolCharBuf(&c->gc->gcvm, s, l);
}

static inline FklVMvalue *add_symbol(FklCodegenCtx *c, const FklString *s) {
    return fklVMaddSymbol(&c->gc->gcvm, s);
}

static inline int is_symbol_list(const FklVMvalue *v) {
    for (; FKL_IS_PAIR(v); v = FKL_VM_CDR(v)) {
        if (!FKL_IS_SYM(FKL_VM_CAR(v)))
            return 0;
    }
    return 1;
}

static inline int is_string_list(const FklVMvalue *v) {
    for (; FKL_IS_PAIR(v); v = FKL_VM_CDR(v)) {
        if (!FKL_IS_STR(FKL_VM_CAR(v)))
            return 0;
    }
    return 1;
}

static inline int is_pair_list(const FklVMvalue *v) {
    for (; FKL_IS_PAIR(v); v = FKL_VM_CDR(v)) {
        if (!FKL_IS_PAIR(FKL_VM_CAR(v)))
            return 0;
    }
    return 1;
}

static inline FklVMvalue *
check_macro_expand_result(FklVMvalue *r, FklVMvalueLnt *lnt, uint64_t line) {
    if (fklIsSerializableToByteCodeFile(r, lnt, line))
        return r;
    return NULL;
}

static inline FklVMvalue *codegen_create_hash(FklVMparseCtx *c,
        FklHashTableEqType eq_type,
        FklVMvalue *list,
        size_t line) {
    FklVMvalue *r = NULL;
    switch (eq_type) {
    case FKL_HASH_EQ:
        r = fklCreateVMvalueHashEq(c->exe);
        break;
    case FKL_HASH_EQV:
        r = fklCreateVMvalueHashEqv(c->exe);
        break;
    case FKL_HASH_EQUAL:
        r = fklCreateVMvalueHashEqual(c->exe);
        break;
    }
    put_line_number(c->ln, r, line);
    for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list)) {
        FklVMvalue *p = FKL_VM_CAR(list);
        FklVMvalue *car = FKL_VM_CAR(p);
        FklVMvalue *cdr = FKL_VM_CDR(p);
        fklVMhashTableSet(FKL_VM_HASH(r), car, cdr);
    }
    return r;
}

typedef struct ListElm {
    FklVMvalue *v;
    uint64_t line;
} ListElm;

static inline FklVMvalue *create_nast_list(ListElm *a,
        size_t num,
        uint32_t line,
        FklVM *vm,
        FklVMvalueLnt *ln) {
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **cur = &r;
    for (size_t i = 0; i < num; i++) {
        (*cur) = fklCreateVMvaluePairWithCar(vm, a[i].v);
        put_line_number(ln, *cur, a[i].line);
        cur = &FKL_VM_CDR(*cur);
        // (*cur) = fklCreateNastNode(FKL_NAST_PAIR, a[i]->curline);
        // (*cur)->pair = fklCreateNastPair();
        // (*cur)->pair->car = a[i];
        // cur = &(*cur)->pair->cdr;
    }
    // (*cur) = fklCreateNastNode(FKL_NAST_NIL, line);
    return r;
}

static inline uint64_t get_curline(const FklVMvalueCodegenInfo *info,
        const FklVMvalue *v) {
    // for (const FklVMvalueCodegenInfo *cur = info; cur; cur = cur->prev) {
    //     uint64_t *r = fklVMvalueCodegenLntGet(cur->lnt, v);
    //     if (r != NULL)
    //         return *r;
    // }

    uint64_t *r = fklVMvalueLntGet(info->ctx->lnt, v);
    if (r != NULL)
        return *r;

    return info->curline;
    FKL_TODO();
}

#define PLACE(P, L) ((FklCodegenErrorPlace){ .place = (P), .line = (L) })

#endif
