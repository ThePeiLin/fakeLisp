#ifndef FKL_SRC_CODEGEN_H
#define FKL_SRC_CODEGEN_H

#include <fakeLisp/codegen.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/vm.h>
#include <stdint.h>

static inline uint32_t enter_new_scope(uint32_t p, FklVMvalueCgEnv *env) {
    FklCgEnvScopeVector *scopes = &env->scopes;
    FklCgEnvScope *scope = fklCgEnvScopeVectorPushBack(scopes, NULL);
    uint32_t r = env->scopes.size;
    // uint32_t r = ++env->scope;
    // size_t total_size = r * sizeof(FklCgEnvScope);
    // FklCgEnvScope *scopes =
    //         (FklCgEnvScope *)fklZrealloc(env->scopes, total_size);
    // FKL_ASSERT(scopes);
    // env->scopes = scopes;
    // FklCgEnvScope *newScope = &scopes[r - 1];
    scope->p = p;
    fklSymDefHashMapInit(&scope->defs);
    scope->start = 0;
    scope->end = 0;
    if (p != 0) {
        scope->start = scopes->base[p - 1].start + scopes->base[p - 1].end;
    }
    scope->empty = scope->start;
    return r;
}

static inline FklGrammerProdGroupItem *add_production_group(
        FklGraProdGroupHashMap *named_prod_groups,
        FklVM *vm,
        FklVMvalue *group_id) {
    FklGrammerProdGroupItem *group =
            fklGraProdGroupHashMapAdd1(named_prod_groups, group_id);
    if ((group->flags & FKL_GRAMMER_GROUP_INITED) == 0) {
        fklInitEmptyGrammer(&group->g, vm);
        group->flags |= FKL_GRAMMER_GROUP_INITED;
    }
    return group;
}

static inline void merge_group(FklGrammerProdGroupItem *group,
        const FklGrammerProdGroupItem *other,
        const FklRecomputeGroupIdArgs *args) {
    fklMergeGrammer(&group->g, &other->g, args);
}

static inline void uninit_codegen_macro(FklCgMacro *macro) {
    macro->pattern = NULL;
    macro->proc = NULL;
}

static inline void
put_line_number(FklVMvalueLnt *ln, FklVMvalue *v, uint64_t line) {
    if (ln)
        fklVMvalueLntPut(ln, v, line);
}

static inline FklVMvalue *add_symbol_cstr(FklCgCtx *c, const char *s) {
    return fklVMaddSymbolCstr(c->vm, s);
}

static inline FklVMvalue *
add_symbol_char_buf(FklCgCtx *c, const char *s, size_t l) {
    return fklVMaddSymbolCharBuf(c->vm, s, l);
}

static inline FklVMvalue *add_symbol(FklCgCtx *c, const FklString *s) {
    return fklVMaddSymbol(c->vm, s);
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
        (*cur) = fklCreateVMvaluePair1(vm, a[i].v);
        put_line_number(ln, *cur, a[i].line);
        cur = &FKL_VM_CDR(*cur);
    }
    return r;
}

static inline uint64_t get_curline(const FklVMvalueCgInfo *info,
        const FklVMvalue *v) {
    uint64_t *r = fklVMvalueLntGet(info->lnt, v);
    if (r != NULL)
        return *r;

    return info->curline;
}

#endif
