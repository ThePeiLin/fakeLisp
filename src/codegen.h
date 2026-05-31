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
    for (; v != FKL_VM_NIL; v = FKL_VM_CDR(v)) {
        if (!FKL_IS_PAIR(v) || !FKL_IS_SYM(FKL_VM_CAR(v)))
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

static inline FklVMvalue *create_list(ListElm *a,
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

static inline FklVMvalue *make_syntax_error(FklVM *exe, FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_SYNTAXERROR,
            exe,
            "Invalid syntax %S",
            place);
}

static inline FklVMvalue *
make_grammer_create_error2(FklVM *exe, const char *s, FklVMvalue *place) {
    if (place == NULL) {
        return FKL_MAKE_VM_ERR(FKL_ERR_GRAMMER_CREATE_FAILED,
                exe,
                "%s",
                fklCreateVMvalueStr1(exe, s));
    } else {
        return FKL_MAKE_VM_ERR(FKL_ERR_GRAMMER_CREATE_FAILED,
                exe,
                "%s %S",
                fklCreateVMvalueStr1(exe, s),
                place);
    }
}

#define CURLINE(V) get_curline(info, V)

static inline FklVMvalueCgInfo *macro_compile_prepare(FklCgCtx *ctx,
        FklVMvalueCgInfo *info,
        FklVMvalueCgMacroScope *macro_scope,
        FklValueHashSet *symbol_set,
        FklVMvalueCgEnv **penv,
        uint64_t line) {
    FklVMvalueCgInfo *macro_info = fklCreateVMvalueCgInfo(ctx,
            info,
            NULL,
            &(FklCgInfoArgs){
                .is_macro = 1,
                .macro_scope = macro_scope,
            });

    FklVMvalueCgEnv *macro_main_env = fklCreateVMvalueCgEnv(ctx, //
            &(const FklCgEnvCreateArgs){
                .prev_env = macro_info->global_env,
                .prev_ms = macro_scope,
                .parent_scope = 1,
                .filename = info->fid,
                .name = FKL_VM_NIL,
                .line = line,
            });

    *penv = macro_main_env;
    if (symbol_set == NULL)
        return macro_info;

    for (FklValueHashSetNode *list = symbol_set->first; list;
            list = list->next) {
        FklVMvalue *id = FKL_TYPE_CAST(FklVMvalue *, list->k);
        fklAddCgDefBySid(id, 1, macro_main_env);
    }

    return macro_info;
}

// FKL_DEPRECATED start
typedef FklCgExpQueue CgExpQueue;
typedef FklCgExpQueueNode CgExpQueueNode;

#define cgExpQueuePop fklCgExpQueuePop
#define cgExpQueuePush fklCgExpQueuePush
#define cgExpQueuePush2 fklCgExpQueuePush2

#define cgExpQueueCreate fklCgExpQueueCreate
#define cgExpQueueDestroy fklCgExpQueueDestroy
#define cgExpQueueInit fklCgExpQueueInit
#define cgExpQueueUninit fklCgExpQueueUninit
// FKL_DEPRECATED end

#define DO_NOT_NEED_RETVAL (0)
#define ALL_MUST_HAS_RETVAL (1)
#define FIRST_MUST_HAS_RETVAL (2)

static inline FklCgNextExp *createCgNextExp(
        const FklNextExpressionMethodTable *t,
        void *context,
        uint8_t must_has_retval) {
    FklCgNextExp *r = (FklCgNextExp *)fklZmalloc(sizeof(FklCgNextExp));
    FKL_ASSERT(r);
    r->t = t;
    r->context = context;
    r->must_has_retval = must_has_retval;
    return r;
}

static int _default_codegen_get_next_expression(FklCgCtx *ctx,
        void *context,
        FklPmatchRes *out) {
    FklPmatchRes *head = cgExpQueuePop(FKL_TYPE_CAST(CgExpQueue *, context));
    if (head == NULL)
        return 0;
    *out = *head;
    return 1;
}

static void _default_codegen_next_expression_finalizer(void *context) {
    CgExpQueue *q = FKL_TYPE_CAST(CgExpQueue *, context);
    cgExpQueueDestroy(q);
}

static void _default_codegen_next_expression_atomic(FklVMgc *gc, void *ctx) {
    CgExpQueue *q = FKL_TYPE_CAST(CgExpQueue *, ctx);
    for (const CgExpQueueNode *c = q->head; c; c = c->next) {
        fklVMgcToGray(c->data.value, gc);
        fklVMgcToGray(c->data.container, gc);
    }
}

static const FklNextExpressionMethodTable
        _default_codegen_next_expression_method_table = {
            .get_next_exp = _default_codegen_get_next_expression,
            .finalize = _default_codegen_next_expression_finalizer,
            .atomic = _default_codegen_next_expression_atomic,
        };

static inline FklCgNextExp *createDefaultQueueNextExpression(
        CgExpQueue *queue) {
    return createCgNextExp(&_default_codegen_next_expression_method_table,
            queue,
            DO_NOT_NEED_RETVAL);
}

static inline FklCgNextExp *createMustHasRetvalQueueNextExpression(
        CgExpQueue *queue) {
    return createCgNextExp(&_default_codegen_next_expression_method_table,
            queue,
            ALL_MUST_HAS_RETVAL);
}

static inline FklCgNextExp *createFirstHasRetvalQueueNextExpression(
        CgExpQueue *queue) {
    return createCgNextExp(&_default_codegen_next_expression_method_table,
            queue,
            FIRST_MUST_HAS_RETVAL);
}

static inline FklCgActCtx *createCgActCtx(const FklCgActCtxMethodTable *t) {
    FklCgActCtx *r = NULL;
    r = (FklCgActCtx *)fklZcalloc(1, sizeof(FklCgActCtx) + t->size);
    FKL_ASSERT(r);
    r->t = t;
    return r;
}

static inline FklCgAct *make_cg_act(FklCgActCb f,
        FklCgActCtx *context,
        FklCgNextExp *nextExpression,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalueCgEnv *env,
        uint64_t curline,
        FklCgAct *prev,
        FklVMvalueCgInfo *info) {
    FklCgAct *r = (FklCgAct *)fklZmalloc(sizeof(FklCgAct));
    FKL_ASSERT(r);
    r->scope = scope;
    r->macros = macro_scope;
    r->cb = f;
    r->ctx = context;
    r->exps = nextExpression;
    r->env = env;
    r->curline = curline;
    r->info = info;
    r->prev = prev;
    fklValueVectorInit(&r->bcl_vector, 0);
    return r;
}

#endif
