#include <fakeLisp/base.h>
#include <fakeLisp/bigint.h>
#include <fakeLisp/code_builder.h>
#include <fakeLisp/common.h>
#include <fakeLisp/dis.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <fakeLisp/cb_helper.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdalign.h>
#include <stdio.h>
#include <string.h>

// lalr1
// =====

// 判断非终结符是否是 S'
static inline int is_Sq_nt(const FklGrammerNonterm *nt) {
    return nt->group == 0 && nt->sid == 0;
}

void fklUninitGrammerSymbols(FklGrammerSym *syms, size_t len) {
    for (size_t i = 0; i < len; i++) {
        FklGrammerSym *s = &syms[i];
        if (s->type == FKL_TERM_BUILTIN) {
            if (s->b.len) {
                s->b.len = 0;
                fklZfree(s->b.args);
                s->b.args = NULL;
            }
        }
    }
}

void fklDestroyGrammerProduction(FklGrammerProduction *h) {
    if (h == NULL)
        return;
    h->ctx_destroy(h->ctx);
    fklUninitGrammerSymbols(h->syms, h->len);
    fklZfree(h);
}

static inline void destroy_builtin_grammer_sym(FklLalrBuiltinGrammerSym *s) {
    if (s->len) {
        s->len = 0;
        fklZfree(s->args);
        s->args = NULL;
    }
}

FklGrammerProduction *fklCreateProduction(struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        const char *name,
        FklProdActionFunc func,
        void *ctx,
        void (*destroy)(void *),
        void *(*copyer)(const void *)) {
    FklGrammerProduction *r = (FklGrammerProduction *)fklZcalloc(1,
            sizeof(FklGrammerProduction) + len * sizeof(FklGrammerSym));
    FKL_ASSERT(r);
    r->left.group = group;
    r->left.sid = sid;
    r->len = len;
    r->name = name;
    r->func = func;
    r->ctx = ctx;
    r->ctx_destroy = destroy;
    r->ctx_copy = copyer;
    memcpy(r->syms, syms, len * sizeof(FklGrammerSym));
    return r;
}

FklGrammerProduction *fklCreateEmptyProduction(struct FklVMvalue *group,
        struct FklVMvalue *sid,
        size_t len,
        const char *name,
        FklProdActionFunc func,
        void *ctx,
        void (*destroy)(void *),
        void *(*copyer)(const void *)) {
    FklGrammerProduction *r = (FklGrammerProduction *)fklZcalloc(1,
            sizeof(FklGrammerProduction) + len * sizeof(FklGrammerSym));
    FKL_ASSERT(r);
    r->left.group = group;
    r->left.sid = sid;
    r->len = len;
    r->name = name;
    r->func = func;
    r->ctx = ctx;
    r->ctx_destroy = destroy;
    r->ctx_copy = copyer;
    return r;
}

#define DEFINE_DEFAULT_C_MATCH_COND(NAME)                                      \
    static void builtin_match_##NAME##_print_c_match_cond(                     \
            const FklBuiltinTerminalMatchArgs *args,                           \
            FklCodeBuilder *build) {                                           \
        CB_FMT("builtin_match_%s"                                              \
               "(start,*in+otherMatchLen+skip_ignore_len,*restLen-"            \
               "otherMatchLen-skip_ignore_len,&matchLen,ctx)",                 \
                #NAME);                                                        \
    }

void fklProdCtxDestroyDoNothing(void *c) {}

void fklProdCtxDestroyFree(void *c) { fklZfree(c); }

void *fklProdCtxCopyerDoNothing(const void *c) { return (void *)c; }

FklGrammerIgnore *fklCreateEmptyGrammerIgnore(size_t len) {
    FklGrammerIgnore *ig = (FklGrammerIgnore *)fklZcalloc(1,
            sizeof(FklGrammerIgnore) + len * sizeof(FklGrammerIgnoreSym));
    FKL_ASSERT(ig);
    ig->len = len;
    ig->next = NULL;
    return ig;
}

FklGrammerIgnore *fklGrammerSymbolsToIgnore(FklGrammerSym *syms, size_t len) {
    for (size_t i = len; i > 0; i--) {
        FklGrammerSym *sym = &syms[i - 1];
        if (sym->type == FKL_TERM_NONTERM)
            return NULL;
    }
    FklGrammerIgnore *ig = fklCreateEmptyGrammerIgnore(len);
    FklGrammerIgnoreSym *igss = ig->ig;
    for (size_t i = 0; i < len; i++) {
        FklGrammerSym *sym = &syms[i];
        FklGrammerIgnoreSym *igs = &igss[i];
        igs->term_type = sym->type;
        if (igs->term_type == FKL_TERM_BUILTIN)
            igs->b = sym->b;
        else {
            if (igs->term_type == FKL_TERM_REGEX)
                igs->re = sym->re;
            else if (igs->term_type == FKL_TERM_STRING)
                igs->str = sym->str;
            else {
                fklZfree(ig);
                return NULL;
            }
        }
    }
    return ig;
}

static inline int prod_sym_equal(const FklGrammerSym *u0,
        const FklGrammerSym *u1) {
    if (u0->type == u1->type) {
        switch (u0->type) {
        case FKL_TERM_BUILTIN:
            if (u0->b.t == u1->b.t) {
                if (u0->b.len != u1->b.len)
                    return 0;
                for (size_t i = 0; i < u0->b.len; ++i)
                    if (!fklStringEqual(u0->b.args[i], u1->b.args[i]))
                        return 0;
                return 1;
            }
            return 0;
            break;
        case FKL_TERM_REGEX:
            return u0->re == u1->re;
            break;
        case FKL_TERM_STRING:
        case FKL_TERM_KEYWORD:
            return u0->str == u1->str;
            break;
        case FKL_TERM_NONTERM:
            return u0->nt.group == u1->nt.group && u0->nt.sid == u1->nt.sid;
            break;
        case FKL_TERM_IGNORE:
            return 1;
            break;
        case FKL_TERM_EOF:
        case FKL_TERM_NONE:
            FKL_UNREACHABLE();
            break;
        }
    }
    return 0;
}

static inline int prod_equal(const FklGrammerProduction *prod0,
        const FklGrammerProduction *prod1) {
    if (prod0->len != prod1->len)
        return 0;
    size_t len = prod0->len;
    const FklGrammerSym *u0 = prod0->syms;
    const FklGrammerSym *u1 = prod1->syms;
    for (size_t i = 0; i < len; i++)
        if (!prod_sym_equal(&u0[i], &u1[i]))
            return 0;
    return 1;
}

static inline int ignore_equal(const FklGrammerIgnore *a,
        const FklGrammerIgnore *b) {
    if (a->len != b->len)
        return 0;
    for (size_t i = 0; i < a->len; ++i) {
        const FklGrammerIgnoreSym *igs0 = &a->ig[i];
        const FklGrammerIgnoreSym *igs1 = &b->ig[i];

        if (igs0->term_type != igs1->term_type)
            return 0;
        switch (igs0->term_type) {
        case FKL_TERM_BUILTIN:
            if (!fklBuiltinGrammerSymEqual(&igs0->b, &igs1->b))
                return 0;
            break;
        case FKL_TERM_STRING:
            if (igs0->str != igs1->str)
                return 0;
            break;
        case FKL_TERM_REGEX:
            if (igs0->re != igs1->re)
                return 0;
            break;

        case FKL_TERM_NONE:
        case FKL_TERM_KEYWORD:
        case FKL_TERM_IGNORE:
        case FKL_TERM_EOF:
        case FKL_TERM_NONTERM:
            FKL_UNREACHABLE();
            break;
        }
    }
    return 1;
}

static inline FklGrammerProduction *
create_extra_production(struct FklVMvalue *group, struct FklVMvalue *start) {
    FklGrammerProduction *prod = fklCreateEmptyProduction(0,
            0,
            2,
            NULL,
            NULL,
            NULL,
            fklProdCtxDestroyDoNothing,
            fklProdCtxCopyerDoNothing);
    prod->idx = 1;
    FklGrammerSym *u = &prod->syms[0];
    // epsilon
    u->type = FKL_TERM_IGNORE;

    // start symbol
    u = &prod->syms[1];
    u->type = FKL_TERM_NONTERM;
    u->nt.group = group;
    u->nt.sid = start;
    return prod;
}

int fklAddProdAndExtraToGrammer(FklGrammer *g, FklGrammerProduction *prod) {
    const FklGraSidBuiltinHashMap *builtins = &g->builtins;
    FklProdHashMap *productions = &g->productions;
    const FklGrammerNonterm *left = &prod->left;
    if (left->group == 0 && fklGetBuiltinMatch(builtins, left->sid))
        return 1;
    FklGrammerProduction **pp = fklProdHashMapGet(productions, left);
    if (pp) {
        FklGrammerProduction *cur = NULL;
        for (; *pp; pp = &((*pp)->next)) {
            if (prod_equal(*pp, prod)) {
                cur = *pp;
                break;
            }
        }
        if (cur) {
            prod->next = cur->next;
            *pp = prod;
            fklDestroyGrammerProduction(cur);
        } else {
            prod->idx = g->prodNum;
            g->prodNum++;
            prod->next = NULL;
            *pp = prod;
        }
    } else {
        if (!g->start.sid) {
            g->start = *left;
            FklGrammerProduction *extra_prod =
                    create_extra_production(left->group, left->sid);
            extra_prod->next = NULL;
            pp = fklProdHashMapAdd(productions, &extra_prod->left, NULL);
            *pp = extra_prod;
            (*pp)->idx = g->prodNum;
            g->prodNum++;
        }
        prod->next = NULL;
        pp = fklProdHashMapAdd(productions, left, NULL);
        prod->idx = g->prodNum;
        *pp = prod;
        g->prodNum++;
    }
    return 0;
}

int fklAddProdToProdTable(FklGrammer *g, FklGrammerProduction *prod) {
    const FklGraSidBuiltinHashMap *builtins = &g->builtins;
    FklProdHashMap *productions = &g->productions;
    const FklGrammerNonterm *left = &prod->left;
    if (left->group == 0 && fklGetBuiltinMatch(builtins, left->sid))
        return 1;
    FklGrammerProduction **pp = fklProdHashMapGet(productions, left);
    if (pp) {
        FklGrammerProduction *cur = NULL;
        for (; *pp; pp = &((*pp)->next)) {
            if (prod_equal(*pp, prod)) {
                cur = *pp;
                break;
            }
        }
        if (cur) {
            prod->idx = cur->idx;
            prod->next = cur->next;
            *pp = prod;
            fklDestroyGrammerProduction(cur);
        } else {
            prod->idx = g->prodNum;
            g->prodNum++;
            prod->next = NULL;
            *pp = prod;
        }
    } else {
        prod->next = NULL;
        pp = fklProdHashMapAdd(productions, left, NULL);
        prod->idx = g->prodNum;
        g->prodNum++;
        *pp = prod;
    }
    return 0;
}

int fklAddProdToProdTableNoRepeat(FklGrammer *g, FklGrammerProduction *prod) {
    const FklGraSidBuiltinHashMap *builtins = &g->builtins;
    FklProdHashMap *productions = &g->productions;
    const FklGrammerNonterm *left = &prod->left;
    if (left->group == 0 && fklGetBuiltinMatch(builtins, left->sid))
        return 1;
    FklGrammerProduction **pp = fklProdHashMapGet(productions, left);
    if (pp) {
        FklGrammerProduction *cur = NULL;
        for (; *pp; pp = &((*pp)->next)) {
            if (prod_equal(*pp, prod)) {
                cur = *pp;
                break;
            }
        }
        if (cur) {
            return 1;
        } else {
            prod->idx = g->prodNum;
            g->prodNum++;
            prod->next = NULL;
            *pp = prod;
        }
    } else {
        prod->next = NULL;
        pp = fklProdHashMapAdd(productions, left, NULL);
        prod->idx = g->prodNum;
        g->prodNum++;
        *pp = prod;
    }
    return 0;
}

static inline int builtin_grammer_sym_cmp(const FklLalrBuiltinGrammerSym *b0,
        const FklLalrBuiltinGrammerSym *b1) {
    if (b0->len > b1->len)
        return 1;
    else if (b0->len < b1->len)
        return -1;
    else {
        for (size_t i = 0; i < b0->len; ++i) {
            int r = fklStringCmp(b0->args[i], b1->args[i]);
            if (r != 0)
                return r;
        }
    }
    return 0;
}

static inline int nonterm_gt(const FklGrammerNonterm *nt0,
        const FklGrammerNonterm *nt1) {
    return ((FKL_TYPE_CAST(uintptr_t, nt0->group) << 32)
                   + FKL_TYPE_CAST(uintptr_t, nt0->sid))
         > ((FKL_TYPE_CAST(uintptr_t, nt1->group) << 32)
                 + FKL_TYPE_CAST(uintptr_t, nt1->sid));
}

static int nonterm_lt(const FklGrammerNonterm *nt0,
        const FklGrammerNonterm *nt1) {
    return ((FKL_TYPE_CAST(uintptr_t, nt0->group) << 32)
                   + FKL_TYPE_CAST(uintptr_t, nt0->sid))
         > ((FKL_TYPE_CAST(uintptr_t, nt1->group) << 32)
                 + FKL_TYPE_CAST(uintptr_t, nt1->sid));
}

static inline int grammer_sym_cmp(const FklGrammerSym *s0,
        const FklGrammerSym *s1) {
    if (s0->type < s1->type)
        return -1;
    else if (s0->type > s1->type)
        return 1;
    else {
        int r = 0;
        if (s0->type == FKL_TERM_BUILTIN) {
            if (s0->b.t < s1->b.t)
                return -1;
            else if (s0->b.t > s1->b.t)
                return 1;
            else if ((r = builtin_grammer_sym_cmp(&s0->b, &s1->b)))
                return r;
        } else if (s0->type == FKL_TERM_REGEX) {
            if (s0->re < s1->re)
                return -1;
            else if (s0->re > s1->re)
                return 1;
            else
                return 0;
        } else if (nonterm_lt(&s0->nt, &s1->nt))
            return -1;
        else if (nonterm_gt(&s0->nt, &s1->nt))
            return 1;
    }
    return 0;
}

static inline void build_string_in_hex(const FklString *stri,
        FklCodeBuilder *build) {
    size_t size = stri->size;
    const char *str = stri->str;
    for (size_t i = 0; i < size; i++)
        CB_FMT("\\x%02X", str[i]);
}

static inline int ignore_match(const FklGrammer *g,
        const FklGrammerIgnore *ig,
        const char *start,
        const char *str,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    size_t matchLen = 0;
    const FklGrammerIgnoreSym *igss = ig->ig;
    size_t len = ig->len;
    for (size_t i = 0; i < len; i++) {
        const FklGrammerIgnoreSym *ig = &igss[i];
        if (ig->term_type == FKL_TERM_BUILTIN) {
            size_t len = 0;
            FklBuiltinTerminalMatchArgs args = {
                .g = g,
                .len = ig->b.len,
                .args = ig->b.args,
            };
            if (ig->b.t->match(&args,
                        start,
                        str,
                        restLen - matchLen,
                        &len,
                        ctx,
                        is_waiting_for_more)) {
                str += len;
                matchLen += len;
            } else
                return 0;
        } else if (ig->term_type == FKL_TERM_REGEX) {
            int last_is_true = 0;
            uint32_t len =
                    fklRegexLexMatchp(ig->re, str, restLen, &last_is_true);
            if (len > restLen) {
                *is_waiting_for_more |= last_is_true;
                return 0;
            } else {
                str += len;
                matchLen += len;
            }
        } else {
            const FklString *laString = ig->str;
            if (fklStringCharBufMatch(laString, str, restLen - matchLen) >= 0) {
                str += laString->size;
                matchLen += laString->size;
            } else
                return 0;
        }
    }
    *pmatchLen = matchLen;
    return 1;
}

static inline size_t get_max_non_term_length(const FklGrammer *g,
        FklGrammerMatchCtx *ctx,
        const char *start,
        const char *cur,
        size_t restLen) {
    if (restLen) {
        if (start == ctx->start && cur == ctx->cur)
            return ctx->maxNonterminalLen;
        ctx->start = start;
        ctx->cur = cur;
        FklGrammerIgnore *ignores = g->ignores;
        const FklString **terms = g->sorted_delimiters;
        size_t num = g->sorted_delimiters_num;
        size_t len = 0;
        while (len < restLen) {
            int is_waiting_for_more = 0;
            for (FklGrammerIgnore *ig = ignores; ig; ig = ig->next) {
                size_t matchLen = 0;
                if (ignore_match(g,
                            ig,
                            start,
                            cur,
                            restLen - len,
                            &matchLen,
                            ctx,
                            &is_waiting_for_more))
                    goto break_loop;
            }
            for (size_t i = 0; i < num; i++)
                if (fklStringCharBufMatch(terms[i], cur, restLen - len) >= 0)
                    goto break_loop;
            len++;
            cur++;
        }
    break_loop:
        ctx->maxNonterminalLen = len;
        return len;
    }
    return 0;
}

static int builtin_match_dec_int_func(const FklBuiltinTerminalMatchArgs *args,
        const char *start,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    if (restLen) {
        size_t maxLen =
                get_max_non_term_length(args->g, ctx, start, cstr, restLen);
        if (maxLen && fklIsDecInt(cstr, maxLen)) {
            *pmatchLen = maxLen;
            return 1;
        }
    }
    return 0;
}

static int string_len_cmp(const void *a, const void *b) {
    const FklString *s0 = *(FklString *const *)a;
    const FklString *s1 = *(FklString *const *)b;
    if (s0->size < s1->size)
        return 1;
    if (s0->size > s1->size)
        return -1;
    return 0;
}

static inline void update_sorted_delimiters(FklGrammer *g) {
    if (g->delimiters.count != g->sorted_delimiters_num) {
        size_t num = g->delimiters.count;
        g->sorted_delimiters_num = num;
        const FklString **terms = NULL;
        if (num) {
            terms = (const FklString **)fklZrealloc(g->sorted_delimiters,
                    num * sizeof(FklString *));
            FKL_ASSERT(terms);
            size_t i = 0;
            for (const FklStrHashSetNode *cur = g->delimiters.first; cur;
                    cur = cur->next, ++i) {
                terms[i] = cur->k;
            }
            qsort(terms, num, sizeof(FklString *), string_len_cmp);
        } else {
            fklZfree(g->sorted_delimiters);
        }
        g->sorted_delimiters = terms;
    }
}

static FklBuiltinTerminalInitError builtin_match_number_create(size_t len,
        const FklString **args,
        struct FklGrammer *g) {
    if (len > 0)
        return FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS;
    return 0;
}

DEFINE_DEFAULT_C_MATCH_COND(dec_int);

static inline void build_builtin_match_print_src(FklCodeBuilder *build,
        const char *name,
        const char *func_name) {
    CB_LINE("static int builtin_match_%s(const char* start", name);
    CB_INDENT(flag) {
        CB_LINE(",const char* cstr");
        CB_LINE(",size_t restLen");
        CB_LINE(",ssize_t* pmatchLen");
        CB_LINE(",FklGrammerMatchCtx* ctx)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("if(restLen){");
        CB_INDENT(flag) {
            CB_LINE("size_t maxLen=get_max_non_term_length(ctx,start,cstr,restLen);");
            CB_LINE("if(maxLen&&%s(cstr,maxLen)){", func_name);
            CB_INDENT(flag) {
                CB_LINE("*pmatchLen=maxLen;");
                CB_LINE("return 1;");
            }
            CB_LINE("}");
        }
        CB_LINE("}");
        CB_LINE("return 0;");
    }
    CB_LINE("}");
}

static void builtin_match_dec_int_print_src(const FklGrammer *g,
        FklCodeBuilder *build) {
    build_builtin_match_print_src(build, "dec_int", "fklIsDecInt");
}

static const FklLalrBuiltinMatch builtin_match_dec_int = {
    .name = "?dint",
    .match = builtin_match_dec_int_func,
    .ctx_create = builtin_match_number_create,
    .build_src = builtin_match_dec_int_print_src,
    .build_c_match_cond = builtin_match_dec_int_print_c_match_cond,
};

static int builtin_match_hex_int_func(const FklBuiltinTerminalMatchArgs *args,
        const char *start,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    if (restLen) {
        size_t maxLen =
                get_max_non_term_length(args->g, ctx, start, cstr, restLen);
        if (maxLen && fklIsHexInt(cstr, maxLen)) {
            *pmatchLen = maxLen;
            return 1;
        }
    }
    return 0;
}

static void builtin_match_hex_int_print_src(const FklGrammer *g,
        FklCodeBuilder *build) {
    build_builtin_match_print_src(build, "hex_int", "fklIsHexInt");
}

DEFINE_DEFAULT_C_MATCH_COND(hex_int);

static const FklLalrBuiltinMatch builtin_match_hex_int = {
    .name = "?xint",
    .match = builtin_match_hex_int_func,
    .ctx_create = builtin_match_number_create,
    .build_src = builtin_match_hex_int_print_src,
    .build_c_match_cond = builtin_match_hex_int_print_c_match_cond,
};

static int builtin_match_oct_int_func(const FklBuiltinTerminalMatchArgs *args,
        const char *start,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    if (restLen) {
        size_t maxLen =
                get_max_non_term_length(args->g, ctx, start, cstr, restLen);
        if (maxLen && fklIsOctInt(cstr, maxLen)) {
            *pmatchLen = maxLen;
            return 1;
        }
    }
    return 0;
}

static void builtin_match_oct_int_print_src(const FklGrammer *g,
        FklCodeBuilder *build) {
    build_builtin_match_print_src(build, "oct_int", "fklIsOctInt");
}

DEFINE_DEFAULT_C_MATCH_COND(oct_int);

static const FklLalrBuiltinMatch builtin_match_oct_int = {
    .name = "?oint",
    .match = builtin_match_oct_int_func,
    .ctx_create = builtin_match_number_create,
    .build_src = builtin_match_oct_int_print_src,
    .build_c_match_cond = builtin_match_oct_int_print_c_match_cond,
};

static int builtin_match_dec_float_func(const FklBuiltinTerminalMatchArgs *args,
        const char *start,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    if (restLen) {
        size_t maxLen =
                get_max_non_term_length(args->g, ctx, start, cstr, restLen);
        if (maxLen && fklIsDecFloat(cstr, maxLen)) {
            *pmatchLen = maxLen;
            return 1;
        }
    }
    return 0;
}

static void builtin_match_dec_float_print_src(const FklGrammer *g,
        FklCodeBuilder *build) {
    build_builtin_match_print_src(build, "dec_float", "fklIsDecFloat");
}

DEFINE_DEFAULT_C_MATCH_COND(dec_float);

static const FklLalrBuiltinMatch builtin_match_dec_float = {
    .name = "?dfloat",
    .match = builtin_match_dec_float_func,
    .ctx_create = builtin_match_number_create,
    .build_src = builtin_match_dec_float_print_src,
    .build_c_match_cond = builtin_match_dec_float_print_c_match_cond,
};

static int builtin_match_hex_float_func(const FklBuiltinTerminalMatchArgs *args,
        const char *start,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    if (restLen) {
        size_t maxLen =
                get_max_non_term_length(args->g, ctx, start, cstr, restLen);
        if (maxLen && fklIsHexFloat(cstr, maxLen)) {
            *pmatchLen = maxLen;
            return 1;
        }
    }
    return 0;
}

static void builtin_match_hex_float_print_src(const FklGrammer *g,
        FklCodeBuilder *build) {
    build_builtin_match_print_src(build, "hex_float", "fklIsHexFloat");
}

DEFINE_DEFAULT_C_MATCH_COND(hex_float);

static const FklLalrBuiltinMatch builtin_match_hex_float = {
    .name = "?xfloat",
    .match = builtin_match_hex_float_func,
    .ctx_create = builtin_match_number_create,
    .build_src = builtin_match_hex_float_print_src,
    .build_c_match_cond = builtin_match_hex_float_print_c_match_cond,
};

static int builtin_match_identifier_func(
        const FklBuiltinTerminalMatchArgs *args,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    size_t maxLen =
            get_max_non_term_length(args->g, ctx, cstrStart, cstr, restLen);
    if (!maxLen || fklIsDecInt(cstr, maxLen) || fklIsOctInt(cstr, maxLen)
            || fklIsHexInt(cstr, maxLen) || fklIsDecFloat(cstr, maxLen)
            || fklIsHexFloat(cstr, maxLen) || fklIsAllDigit(cstr, maxLen))
        return 0;
    *pmatchLen = maxLen;
    return 1;
}

static void builtin_match_identifier_print_src(const FklGrammer *g,
        FklCodeBuilder *build) {
    CB_LINE("static int builtin_match_identifier(const char* cstrStart");
    CB_INDENT(flag) {
        CB_LINE(",const char* cstr");
        CB_LINE(",size_t restLen");
        CB_LINE(",ssize_t* pmatchLen");
        CB_LINE(",FklGrammerMatchCtx* ctx)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("size_t maxLen=get_max_non_term_length(ctx,cstrStart,cstr,restLen);");
        CB_LINE("if(!maxLen");
        CB_INDENT(flag) {
            CB_INDENT(flag) {
                CB_LINE("||fklIsDecInt(cstr,maxLen)");
                CB_LINE("||fklIsOctInt(cstr,maxLen)");
                CB_LINE("||fklIsHexInt(cstr,maxLen)");
                CB_LINE("||fklIsDecFloat(cstr,maxLen)");
                CB_LINE("||fklIsHexFloat(cstr,maxLen)");
                CB_LINE("||fklIsAllDigit(cstr,maxLen))");
            }
            CB_LINE("return 0;");
        }
        CB_LINE("*pmatchLen=maxLen;");
        CB_LINE("return 1;");
    }
    CB_LINE("}");
}

DEFINE_DEFAULT_C_MATCH_COND(identifier);

static const FklLalrBuiltinMatch builtin_match_identifier = {
    .name = "?identifier",
    .match = builtin_match_identifier_func,
    .ctx_create = builtin_match_number_create,
    .build_src = builtin_match_identifier_print_src,
    .build_c_match_cond = builtin_match_identifier_print_c_match_cond,
};

DEFINE_DEFAULT_C_MATCH_COND(nodelimiter);

static void builtin_match_nodelimiter_print_src(const FklGrammer *g,
        FklCodeBuilder *build) {
    CB_LINE("static int builtin_match_nodelimiter(const char* cstrStart");
    CB_INDENT(flag) {
        CB_LINE(",const char* cstr");
        CB_LINE(",size_t restLen");
        CB_LINE(",ssize_t* pmatchLen");
        CB_LINE(",FklGrammerMatchCtx* ctx)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("size_t maxLen=get_max_non_term_length(ctx,cstrStart,cstr,restLen);");
        CB_LINE("if(!maxLen)");
        CB_INDENT(flag) { CB_LINE("return 0;"); }
        CB_LINE("*pmatchLen=maxLen;");
        CB_LINE("return 1;");
    }
    CB_LINE("}");
}

static int builtin_match_nodelimiter_func(
        const FklBuiltinTerminalMatchArgs *args,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    size_t maxLen =
            get_max_non_term_length(args->g, ctx, cstrStart, cstr, restLen);
    if (!maxLen)
        return 0;
    *pmatchLen = maxLen;
    return 1;
}

static const FklLalrBuiltinMatch builtin_match_nodelimiter = {
    .name = "?nodelimiter",
    .match = builtin_match_nodelimiter_func,
    .ctx_create = builtin_match_number_create,
    .build_src = builtin_match_nodelimiter_print_src,
    .build_c_match_cond = builtin_match_nodelimiter_print_c_match_cond,
};

static FklBuiltinTerminalInitError
s_number_create(size_t len, const FklString **args, struct FklGrammer *g) {
    if (len > 1)
        return FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS;
    return 0;
}

#define SYMBOL_NUMBER_MATCH(F)                                                 \
    if (restLen) {                                                             \
        size_t maxLen =                                                        \
                get_max_non_term_length(args->g, ctx, start, cstr, restLen);   \
        if (maxLen                                                             \
                && (!args->len                                                 \
                        || fklStringCharBufMatch(args->args[0],                \
                                   cstr + maxLen,                              \
                                   restLen - maxLen)                           \
                                   < 0)                                        \
                && F(cstr, maxLen)) {                                          \
            *pmatchLen = maxLen;                                               \
            return 1;                                                          \
        }                                                                      \
    }                                                                          \
    return 0

static int builtin_match_s_dint_func(const FklBuiltinTerminalMatchArgs *args,
        const char *start,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsDecInt);
}

static inline void build_lisp_match_print_src(const FklGrammer *g,
        FklCodeBuilder *build,
        const char *name,
        const char *func_name) {
    CB_LINE("static int builtin_match_%s(const char* start", name);
    CB_INDENT(flag) {
        CB_LINE(",const char* cstr");
        CB_LINE(",size_t restLen");
        CB_LINE(",ssize_t* pmatchLen");
        CB_LINE(",FklGrammerMatchCtx* ctx");
        CB_LINE(",const char* term");
        CB_LINE(",size_t termLen)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("if(restLen) {");
        CB_INDENT(flag) {
            CB_LINE("size_t maxLen=get_max_non_term_length(ctx,start,cstr,restLen);");
            CB_LINE("if(maxLen&&(!term||fklCharBufMatch(term,termLen,cstr+maxLen,restLen-maxLen)<0)");
            CB_INDENT(flag) {
                CB_LINE("&&%s(cstr,maxLen)){", func_name);
                CB_LINE("*pmatchLen=maxLen;");
                CB_LINE("return 1;");
            }
            CB_LINE("}");
        }
        CB_LINE("}");
        CB_LINE("return 0;");
    }
    CB_LINE("}");
}

#define DEFINE_LISP_NUMBER_PRINT_SRC(NAME, F)                                  \
    static void builtin_match_##NAME##_print_src(const FklGrammer *g,          \
            FklCodeBuilder *build) {                                           \
        build_lisp_match_print_src(g, build, #NAME, #F);                       \
    }

#define DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(NAME)                            \
    static void builtin_match_##NAME##_print_c_match_cond(                     \
            const FklBuiltinTerminalMatchArgs *args,                           \
            FklCodeBuilder *build) {                                           \
        CB_FMT("builtin_match_%s"                                              \
               "(start,*in+otherMatchLen+skip_ignore_len,*restLen-"            \
               "otherMatchLen-skip_ignore_len,&matchLen,ctx,",                 \
                #NAME);                                                        \
        if (args->len) {                                                       \
            CB_FMT("\"");                                                      \
            build_string_in_hex(args->args[0], build);                         \
            CB_FMT("\"");                                                      \
            CB_FMT(",%" PRIu64 "", args->args[0]->size);                       \
        } else                                                                 \
            CB_FMT("NULL,0");                                                  \
        CB_FMT(")");                                                           \
    }

DEFINE_LISP_NUMBER_PRINT_SRC(s_dint, fklIsDecInt);

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_dint);

DEFINE_LISP_NUMBER_PRINT_SRC(s_xint, fklIsHexInt);

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_xint);

DEFINE_LISP_NUMBER_PRINT_SRC(s_oint, fklIsOctInt);

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_oint);

DEFINE_LISP_NUMBER_PRINT_SRC(s_dfloat, fklIsDecFloat);

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_dfloat);

DEFINE_LISP_NUMBER_PRINT_SRC(s_xfloat, fklIsHexFloat);

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_xfloat);

static int builtin_match_s_xint_func(const FklBuiltinTerminalMatchArgs *args,
        const char *start,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsHexInt);
}

static int builtin_match_s_oint_func(const FklBuiltinTerminalMatchArgs *args,
        const char *start,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsOctInt);
}

static int builtin_match_s_dfloat_func(const FklBuiltinTerminalMatchArgs *args,
        const char *start,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsDecFloat);
}

static int builtin_match_s_xfloat_func(const FklBuiltinTerminalMatchArgs *args,
        const char *start,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsHexFloat);
}

#undef SYMBOL_NUMBER_MATCH

static const FklLalrBuiltinMatch builtin_match_s_dint = {
    .name = "?s-dint",
    .match = builtin_match_s_dint_func,
    .ctx_create = s_number_create,
    .build_src = builtin_match_s_dint_print_src,
    .build_c_match_cond = builtin_match_s_dint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_xint = {
    .name = "?s-xint",
    .match = builtin_match_s_xint_func,
    .ctx_create = s_number_create,
    .build_src = builtin_match_s_xint_print_src,
    .build_c_match_cond = builtin_match_s_xint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_oint = {
    .name = "?s-oint",
    .match = builtin_match_s_oint_func,
    .ctx_create = s_number_create,
    .build_src = builtin_match_s_oint_print_src,
    .build_c_match_cond = builtin_match_s_oint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_dfloat = {
    .name = "?s-dfloat",
    .match = builtin_match_s_dfloat_func,
    .ctx_create = s_number_create,
    .build_src = builtin_match_s_dfloat_print_src,
    .build_c_match_cond = builtin_match_s_dfloat_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_xfloat = {
    .name = "?s-xfloat",
    .match = builtin_match_s_xfloat_func,
    .ctx_create = s_number_create,
    .build_src = builtin_match_s_xfloat_print_src,
    .build_c_match_cond = builtin_match_s_xfloat_print_c_match_cond,
};

static int builtin_match_s_char_func(const FklBuiltinTerminalMatchArgs *args,
        const char *start,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    const FklString *prefix = args->args[0];
    size_t minLen = prefix->size + 1;
    if (restLen < minLen)
        return 0;
    if (fklStringCharBufMatch(prefix, cstr, restLen) < 0)
        return 0;
    restLen -= prefix->size;
    cstr += prefix->size;
    size_t maxLen = get_max_non_term_length(args->g, ctx, start, cstr, restLen);
    if (!maxLen)
        *pmatchLen = prefix->size + 1;
    else
        *pmatchLen = prefix->size + maxLen;
    return 1;
}

static void builtin_match_s_char_print_src(const FklGrammer *g,
        FklCodeBuilder *build) {
    CB_LINE("static int builtin_match_s_char(const char* start");
    CB_INDENT(flag) {
        CB_LINE(",const char* cstr");
        CB_LINE(",size_t restLen");
        CB_LINE(",ssize_t* pmatchLen");
        CB_LINE(",FklGrammerMatchCtx* ctx");
        CB_LINE(",const char* prefix");
        CB_LINE(",size_t prefix_size)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("size_t minLen=prefix_size+1;");
        CB_LINE("if(restLen<minLen) return 0;");
        CB_LINE("if(fklCharBufMatch(prefix,prefix_size,cstr,restLen)<0) return 0;");
        CB_LINE("restLen-=prefix_size;");
        CB_LINE("cstr+=prefix_size;");
        CB_LINE("size_t maxLen=get_max_non_term_length(ctx,start,cstr,restLen);");
        CB_LINE("if(!maxLen) *pmatchLen=prefix_size+1;");
        CB_LINE("else *pmatchLen=prefix_size+maxLen;");
        CB_LINE("return 1;");
    }
    CB_LINE("}");
}

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_char);

static FklBuiltinTerminalInitError
s_char_create(size_t len, const FklString **args, struct FklGrammer *g) {
    if (len > 1)
        return FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS;
    else if (len < 1)
        return FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_FEW_ARGS;
    return 0;
}

static const FklLalrBuiltinMatch builtin_match_s_char = {
    .name = "?s-char",
    .match = builtin_match_s_char_func,
    .ctx_create = s_char_create,
    .build_src = builtin_match_s_char_print_src,
    .build_c_match_cond = builtin_match_s_char_print_c_match_cond,
};

static int builtin_match_symbol_func(const FklBuiltinTerminalMatchArgs *args,
        const char *cstrStart,
        const char *cstr,
        size_t restLen,
        size_t *pmatchLen,
        FklGrammerMatchCtx *ctx,
        int *is_waiting_for_more) {
    const FklString *start = args->len > 0 ? args->args[0] : NULL;
    const FklString *end = args->len > 1 ? args->args[1] : start;
    if (start) {
        size_t matchLen = 0;
        for (;;) {
            if (fklStringCharBufMatch(start, cstr, restLen - matchLen) >= 0) {
                matchLen += start->size;
                cstr += start->size;
                size_t len =
                        fklQuotedStringMatch(cstr, restLen - matchLen, end);
                if (!len) {
                    *is_waiting_for_more = 1;
                    return 0;
                }
                matchLen += len;
                cstr += len;
                continue;
            }
            size_t maxLen = get_max_non_term_length(args->g,
                    ctx,
                    cstrStart,
                    cstr,
                    restLen - matchLen);
            if ((!matchLen && !maxLen)
                    || (fklStringCharBufMatch(start,
                                cstr + maxLen,
                                restLen - maxLen - matchLen)
                                    < 0
                            && maxLen
                            && (fklIsDecInt(cstr, maxLen)
                                    || fklIsOctInt(cstr, maxLen)
                                    || fklIsHexInt(cstr, maxLen)
                                    || fklIsDecFloat(cstr, maxLen)
                                    || fklIsHexFloat(cstr, maxLen)
                                    || fklIsAllDigit(cstr, maxLen))))
                return 0;
            matchLen += maxLen;
            cstr += maxLen;
            if (fklStringCharBufMatch(start, cstr, restLen - matchLen) < 0)
                break;
        }
        *pmatchLen = matchLen;
        return matchLen != 0;
    } else {
        size_t maxLen =
                get_max_non_term_length(args->g, ctx, cstrStart, cstr, restLen);
        if (!maxLen || fklIsDecInt(cstr, maxLen) || fklIsOctInt(cstr, maxLen)
                || fklIsHexInt(cstr, maxLen) || fklIsDecFloat(cstr, maxLen)
                || fklIsHexFloat(cstr, maxLen) || fklIsAllDigit(cstr, maxLen))
            return 0;
        *pmatchLen = maxLen;
    }
    return 1;
}

static void builtin_match_symbol_print_src(const FklGrammer *g,
        FklCodeBuilder *build) {
    CB_LINE("static int builtin_match_symbol(const char* cstrStart");
    CB_INDENT(flag) {
        CB_LINE(",const char* cstr");
        CB_LINE(",size_t restLen");
        CB_LINE(",ssize_t* pmatchLen");
        CB_LINE(",FklGrammerMatchCtx* ctx");
        CB_LINE(",int* is_waiting_for_more");
        CB_LINE(",const char* start");
        CB_LINE(",size_t start_size");
        CB_LINE(",const char* end");
        CB_LINE(",size_t end_size)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("if(start){");
        CB_INDENT(flag) {
            CB_LINE("ssize_t matchLen=0;");
            CB_LINE("for(;;){");
            CB_INDENT(flag) {
                CB_LINE("if(fklCharBufMatch(start,start_size,cstr,restLen-matchLen)>=0){");
                CB_INDENT(flag) {
                    CB_LINE("matchLen+=start_size;");
                    CB_LINE("cstr+=start_size;");
                    CB_LINE("size_t len=fklQuotedCharBufMatch(cstr,restLen-matchLen,end,end_size);");
                    CB_LINE("if(!len) {");
                    CB_INDENT(flag) {
                        CB_LINE("*is_waiting_for_more=1;");
                        CB_LINE("return 0;");
                    }
                    CB_LINE("}");
                    CB_LINE("matchLen+=len;");
                    CB_LINE("cstr+=len;");
                    CB_LINE("continue;");
                }
                CB_LINE("}");
                CB_LINE("size_t maxLen=get_max_non_term_length(ctx,cstrStart,cstr,restLen-matchLen);");
                CB_LINE("if((!matchLen&&!maxLen)");
                CB_INDENT(flag) {
                    CB_LINE("||(fklCharBufMatch(start,start_size,cstr+maxLen,restLen-maxLen-matchLen)<0");
                    CB_INDENT(flag) {
                        CB_LINE("&&maxLen");
                        CB_LINE("&&(fklIsDecInt(cstr,maxLen)");
                        CB_INDENT(flag) {
                            CB_LINE("||fklIsOctInt(cstr,maxLen)");
                            CB_LINE("||fklIsHexInt(cstr,maxLen)");
                            CB_LINE("||fklIsDecFloat(cstr,maxLen)");
                            CB_LINE("||fklIsHexFloat(cstr,maxLen)");
                            CB_LINE("||fklIsAllDigit(cstr,maxLen))))");
                        }
                    }
                    CB_LINE("return 0;");
                }
                CB_LINE("matchLen+=maxLen;");
                CB_LINE("cstr+=maxLen;");
                CB_LINE("if(fklCharBufMatch(start,start_size,cstr,restLen-matchLen)<0) break;");
            }
            CB_LINE("}");
            CB_LINE("*pmatchLen=matchLen;");
            CB_LINE("return matchLen!=0;");
        }
        CB_LINE("}else{");
        CB_INDENT(flag) {
            CB_LINE("size_t maxLen=get_max_non_term_length(ctx,cstrStart,cstr,restLen);");
            CB_LINE("if(!maxLen");
            CB_INDENT(flag) {
                CB_LINE("||fklIsDecInt(cstr,maxLen)");
                CB_LINE("||fklIsOctInt(cstr,maxLen)");
                CB_LINE("||fklIsHexInt(cstr,maxLen)");
                CB_LINE("||fklIsDecFloat(cstr,maxLen)");
                CB_LINE("||fklIsHexFloat(cstr,maxLen)");
                CB_LINE("||fklIsAllDigit(cstr,maxLen))");
                CB_LINE("return 0;");
            }
            CB_LINE("*pmatchLen=maxLen;");
        }
        CB_LINE("}");
        CB_LINE("return 1;");
    }

    CB_LINE("}");
}

static void builtin_match_symbol_print_c_match_cond(
        const FklBuiltinTerminalMatchArgs *args,
        FklCodeBuilder *build) {
    const FklString *start = args->len > 0 ? args->args[0] : NULL;
    const FklString *end = args->len > 1 ? args->args[1] : start;
    CB_FMT("builtin_match_symbol(start,*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len,&matchLen,ctx,&is_waiting_for_more,");
    if (start) {
        CB_FMT("\"");
        build_string_in_hex(start, build);
        CB_FMT("\",%" PRIu64 ",\"", start->size);
    } else {
        CB_FMT("NULL,0");
    }
    if (end) {
        build_string_in_hex(end, build);
        CB_FMT("\",%" PRIu64 ")", end->size);
    } else {
        CB_FMT("NULL,0");
    }
}

static FklBuiltinTerminalInitError builtin_match_symbol_create(size_t len,
        const FklString **args,
        struct FklGrammer *g) {
    if (len > 2)
        return FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS;
    return 0;
}

static const FklLalrBuiltinMatch builtin_match_symbol = {
    .name = "?symbol",
    .match = builtin_match_symbol_func,
    .ctx_create = builtin_match_symbol_create,
    .build_src = builtin_match_symbol_print_src,
    .build_c_match_cond = builtin_match_symbol_print_c_match_cond,
};

#undef DEFINE_DEFAULT_C_MATCH_COND
#undef DEFINE_LISP_NUMBER_PRINT_SRC
#undef DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND

static const struct BuiltinGrammerSymList {
    const char *name;
    const FklLalrBuiltinMatch *t;
} builtin_grammer_sym_list[] = {
    // clang-format off
    {"?dint",        &builtin_match_dec_int     },
    {"?xint",        &builtin_match_hex_int     },
    {"?oint",        &builtin_match_oct_int     },
    {"?dfloat",      &builtin_match_dec_float   },
    {"?xfloat",      &builtin_match_hex_float   },

    {"?s-dint",      &builtin_match_s_dint      },
    {"?s-xint",      &builtin_match_s_xint      },
    {"?s-oint",      &builtin_match_s_oint      },
    {"?s-dfloat",    &builtin_match_s_dfloat    },
    {"?s-xfloat",    &builtin_match_s_xfloat    },

    {"?s-char",      &builtin_match_s_char      },
    {"?symbol",      &builtin_match_symbol      },
    {"?identifier",  &builtin_match_identifier  },
    {"?nodelimiter", &builtin_match_nodelimiter },

    {NULL,           NULL                       },
    // clang-format on
};

void fklInitBuiltinGrammerSymTable(FklGraSidBuiltinHashMap *s,
        struct FklVM *vm) {
    fklGraSidBuiltinHashMapInit(s);
    for (const struct BuiltinGrammerSymList *l = &builtin_grammer_sym_list[0];
            l->name;
            l++) {
        FklVMvalue *id = fklVMaddSymbolCstr(vm, l->name);
        fklGraSidBuiltinHashMapPut2(s, id, l->t);
    }
}

const char *fklBuiltinTerminalInitErrorToCstr(FklBuiltinTerminalInitError err) {
    switch (err) {
    case FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY:
        FKL_UNREACHABLE();
        break;
    case FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS:
        return "too many arguments";
        break;
    case FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_FEW_ARGS:
        return "too few arguments";
        break;
    }
    return NULL;
}

static inline void clear_analysis_table(FklGrammer *g, size_t last) {
    size_t end = last + 1;
    FklAnalysisState *states = g->aTable.states;
    for (size_t i = 0; i < end; i++) {
        FklAnalysisState *curState = &states[i];
        FklAnalysisStateAction *actions = curState->state.action;
        while (actions) {
            FklAnalysisStateAction *next = actions->next;
            fklZfree(actions);
            actions = next;
        }

        FklAnalysisStateGoto *gt = curState->state.gt;
        while (gt) {
            FklAnalysisStateGoto *next = gt->next;
            fklZfree(gt);
            gt = next;
        }
    }
    fklZfree(states);
    g->aTable.states = NULL;
    g->aTable.num = 0;
}

void fklDestroyIgnore(FklGrammerIgnore *ig) {
    size_t len = ig->len;
    for (size_t i = 0; i < len; i++) {
        FklGrammerIgnoreSym *igs = &ig->ig[i];
        if (igs->term_type == FKL_TERM_BUILTIN)
            destroy_builtin_grammer_sym(&igs->b);
    }
    fklZfree(ig);
}

void fklClearGrammer(FklGrammer *g) {
    g->prodNum = 0;
    g->start.group = 0;
    g->start.sid = 0;
    fklProdHashMapClear(&g->productions);
    fklFirstSetHashMapClear(&g->firstSets);
    clear_analysis_table(g, g->aTable.num - 1);
    fklZfree(g->sorted_delimiters);
    g->sorted_delimiters = NULL;
    g->sorted_delimiters_num = 0;
    FklGrammerIgnore *ig = g->ignores;
    while (ig) {
        FklGrammerIgnore *next = ig->next;
        fklZfree(ig);
        ig = next;
    }
    fklClearStringTable(&g->delimiters);

    g->ignores = NULL;
}

void fklUninitGrammer(FklGrammer *g) {
    fklProdHashMapUninit(&g->productions);
    fklGraSidBuiltinHashMapUninit(&g->builtins);
    fklFirstSetHashMapUninit(&g->firstSets);
    fklUninitStringTable(&g->terminals);
    fklUninitStringTable(&g->delimiters);
    fklUninitRegexTable(&g->regexes);
    clear_analysis_table(g, g->aTable.num - 1);
    FklGrammerIgnore *ig = g->ignores;
    while (ig) {
        FklGrammerIgnore *next = ig->next;
        fklDestroyIgnore(ig);
        ig = next;
    }
    if (g->sorted_delimiters) {
        g->sorted_delimiters_num = 0;
        fklZfree(g->sorted_delimiters);
        g->sorted_delimiters = NULL;
    }
    memset(g, 0, sizeof(*g));
}

void fklDestroyGrammer(FklGrammer *g) {
    fklUninitGrammer(g);
    fklZfree(g);
}

int fklAddIgnoreToIgnoreList(FklGrammerIgnore **pp, FklGrammerIgnore *ig) {
    for (; *pp; pp = &(*pp)->next) {
        if (ignore_equal(*pp, ig))
            return 1;
    }

    *pp = ig;
    return 0;
}

static inline uint32_t is_regex_match_epsilon(const FklRegexCode *re) {
    int last_is_true = 0;
    return fklRegexLexMatchp(re, "", 0, &last_is_true) == 0;
}

static inline int is_builtin_terminal_match_epsilon(const FklGrammer *g,
        const FklLalrBuiltinGrammerSym *b) {
    int is_waiting_for_more = 0;
    size_t matchLen = 0;
    FklGrammerMatchCtx ctx = {
        .maxNonterminalLen = 0,
        .line = 0,
        .start = NULL,
        .cur = NULL,
        .create = NULL,
        .destroy = NULL,
    };
    FklBuiltinTerminalMatchArgs args = {
        .g = g,
        .len = b->len,
        .args = b->args,
    };
    return b->t->match(&args, "", "", 0, &matchLen, &ctx, &is_waiting_for_more);
}

static inline int compute_all_first_set(FklGrammer *g) {
    FklFirstSetHashMap *firsetSets = &g->firstSets;

    const FklFirstSetItem item = { .hasEpsilon = 0 };

    for (const FklProdHashMapNode *sidl = g->productions.first; sidl;
            sidl = sidl->next) {
        if (is_Sq_nt(&sidl->k))
            continue;
        fklFirstSetHashMapPut(firsetSets, &sidl->k, &item);
    }

    int change;
    do {
        change = 0;
        for (const FklProdHashMapNode *leftProds = g->productions.first;
                leftProds;
                leftProds = leftProds->next) {
            if (is_Sq_nt(&leftProds->k))
                continue;
            FklFirstSetItem *firstItem =
                    fklFirstSetHashMapGet(firsetSets, &leftProds->k);
            const FklGrammerProduction *prods = leftProds->v;
            for (; prods; prods = prods->next) {
                size_t len = prods->len;
                if (!len) {
                    change |= firstItem->hasEpsilon != 1;
                    firstItem->hasEpsilon = 1;
                    continue;
                }
                size_t lastIdx = len - 1;
                const FklGrammerSym *syms = prods->syms;
                for (size_t i = 0; i < len; i++) {
                    const FklGrammerSym *sym = &syms[i];
                    FklLalrItemLookAhead la = { .t = sym->type };
                    switch (sym->type) {
                    case FKL_TERM_BUILTIN: {
                        int r = is_builtin_terminal_match_epsilon(g, &sym->b);
                        la.b = sym->b;
                        change |=
                                !fklLookAheadHashSetPut(&firstItem->first, &la);
                        if (r) {
                            if (i == lastIdx) {
                                change |= firstItem->hasEpsilon != 1;
                                firstItem->hasEpsilon = 1;
                            }
                        } else
                            goto break_loop;

                    } break;
                    case FKL_TERM_REGEX: {
                        uint32_t r = is_regex_match_epsilon(sym->re);
                        la.re = sym->re;
                        change |=
                                !fklLookAheadHashSetPut(&firstItem->first, &la);
                        if (r) {
                            if (i == lastIdx) {
                                change |= firstItem->hasEpsilon != 1;
                                firstItem->hasEpsilon = 1;
                            }
                        } else
                            goto break_loop;
                    } break;
                    case FKL_TERM_NONTERM: {
                        const FklFirstSetItem *curFirstItem =
                                fklFirstSetHashMapGet(firsetSets, &sym->nt);
                        if (!curFirstItem)
                            return 1;
                        for (const FklLookAheadHashSetNode *syms =
                                        curFirstItem->first.first;
                                syms;
                                syms = syms->next) {
                            change |= !fklLookAheadHashSetPut(&firstItem->first,
                                    &syms->k);
                        }
                        if (curFirstItem->hasEpsilon && i == lastIdx) {
                            change |= firstItem->hasEpsilon != 1;
                            firstItem->hasEpsilon = 1;
                        }
                        if (!curFirstItem->hasEpsilon)
                            goto break_loop;
                    } break;
                    case FKL_TERM_IGNORE: {
                        change |=
                                !fklLookAheadHashSetPut(&firstItem->first, &la);
                        if (i == lastIdx) {
                            change |= firstItem->hasEpsilon != 1;
                            firstItem->hasEpsilon = 1;
                        }
                    } break;
                    case FKL_TERM_STRING:
                    case FKL_TERM_KEYWORD: {
                        const FklString *s = sym->str;
                        if (s->size == 0) {
                            if (i == lastIdx) {
                                change |= firstItem->hasEpsilon != 1;
                                firstItem->hasEpsilon = 1;
                            }
                        } else {
                            la.s = s;
                            change |= !fklLookAheadHashSetPut(&firstItem->first,
                                    &la);
                            goto break_loop;
                        }
                    } break;
                    case FKL_TERM_NONE:
                    case FKL_TERM_EOF:
                        FKL_UNREACHABLE();
                        break;
                    }
                }
            break_loop:;
            }
        }
    } while (change);
    return 0;
}

void fklInitEmptyGrammer(FklGrammer *r, struct FklVM *vm) {
    memset(r, 0, sizeof(*r));
    fklInitStringTable(&r->terminals);
    fklInitStringTable(&r->delimiters);
    fklInitRegexTable(&r->regexes);
    fklFirstSetHashMapInit(&r->firstSets);
    fklProdHashMapInit(&r->productions);
    fklInitBuiltinGrammerSymTable(&r->builtins, vm);
}

static inline FklGrammer *create_grammer() {
    FklGrammer *r = (FklGrammer *)fklZcalloc(1, sizeof(FklGrammer));
    FKL_ASSERT(r);
    return r;
}

FklGrammer *fklCreateEmptyGrammer(struct FklVM *vm) {
    FklGrammer *r = create_grammer();
    fklInitEmptyGrammer(r, vm);
    return r;
}

int fklIsGrammerInited(const FklGrammer *g) {
    return g->productions.buckets != NULL;
}

// GraProdVector
#define FKL_VECTOR_TYPE_PREFIX Gra
#define FKL_VECTOR_METHOD_PREFIX gra
#define FKL_VECTOR_ELM_TYPE FklGrammerProduction *
#define FKL_VECTOR_ELM_TYPE_NAME Prod
#include <fakeLisp/cont/vector.h>

int fklCheckUndefinedNonterm(FklGrammer *g, FklGrammerNonterm *nt) {
    FklProdHashMap *productions = &g->productions;
    for (const FklProdHashMapNode *il = productions->first; il; il = il->next) {
        for (const FklGrammerProduction *prods = il->v; prods;
                prods = prods->next) {
            const FklGrammerSym *syms = prods->syms;
            for (size_t i = 0; i < prods->len; i++) {
                const FklGrammerSym *cur = &syms[i];
                if (cur->type == FKL_TERM_NONTERM
                        && !fklProdHashMapGet(productions, &cur->nt)) {
                    *nt = cur->nt;
                    return 1;
                }
            }
        }
    }
    return 0;
}

int fklCheckAndInitGrammerSymbols(FklGrammer *g, FklGrammerNonterm *nt) {
    int r = fklCheckUndefinedNonterm(g, nt) || compute_all_first_set(g);
    if (r)
        return r;
    update_sorted_delimiters(g);
    return 0;
}

static inline void print_unresolved_terminal(const FklGrammerNonterm *nt,
        FklCodeBuilder *fp) {
    fklCodeBuilderPuts(fp, "nonterm: ");
    fklPrintSymbolLiteral2(FKL_VM_SYM(nt->sid), fp);
    fklCodeBuilderPuts(fp, " is not defined\n");
}

int fklAddExtraProdToGrammer(FklGrammer *g) {
    FklGrammerNonterm left = g->start;
    const FklGraSidBuiltinHashMap *builtins = &g->builtins;
    if (left.group == 0 && fklGetBuiltinMatch(builtins, left.sid))
        return 1;
    FklGrammerProduction *extra_prod =
            create_extra_production(left.group, left.sid);
    extra_prod->next = NULL;
    FklGrammerProduction **item =
            fklProdHashMapAdd(&g->productions, &extra_prod->left, NULL);
    *item = extra_prod;
    (*item)->idx = g->prodNum;
    g->prodNum++;
    return 0;
}

static inline void print_as_regex(const FklString *str, FklCodeBuilder *build) {
    const char *cur = str->str;
    const char *const end = cur + str->size;
    CB_FMT("/");
    for (; cur < end; ++cur) {
        if (*cur == '/')
            CB_FMT("\\/");
        else
            CB_FMT("%c", *cur);
    }
    CB_FMT("/");
}

static inline void print_prod_sym(FklVMgc *gc,
        const FklGrammerSym *u,
        const FklRegexTable *rt,
        FklCodeBuilder *build) {
    switch (u->type) {
    case FKL_TERM_BUILTIN:
        CB_FMT("%s", u->b.t->name);
        if (u->b.len) {
            CB_FMT("[");
            size_t i = 0;
            for (; i < u->b.len - 1; ++i) {
                fklPrintSymbolLiteral2(u->b.args[i], build);
                CB_FMT(", ");
            }
            fklPrintSymbolLiteral2(u->b.args[i], build);
            CB_FMT("]");
        }
        break;
    case FKL_TERM_REGEX:
        print_as_regex(fklGetStringWithRegex(rt, u->re, NULL), build);
        break;
    case FKL_TERM_STRING:
        fklPrintStringLiteral2(u->str, build);
        break;
    case FKL_TERM_KEYWORD:
        fklPrintSymbolLiteral2(u->str, build);
        break;
    case FKL_TERM_NONTERM:
        if (u->nt.group) {
            CB_FMT("(");
            fklPrin1VMvalue2(u->nt.group, build, &gc->gcvm);
            CB_FMT(" , ");
            fklPrin1VMvalue2(u->nt.sid, build, &gc->gcvm);
            CB_FMT(")");
        } else {
            fklPrin1VMvalue2(u->nt.sid, build, &gc->gcvm);
        }
        break;
    case FKL_TERM_IGNORE:
        CB_FMT("?e");
        break;
    case FKL_TERM_NONE:
    case FKL_TERM_EOF:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void print_string_as_dot(const char *str,
        char se,
        size_t size,
        FklCodeBuilder *out) {
    uint64_t i = 0;
    while (i < size) {
        unsigned int l =
                fklGetByteNumOfUtf8((const uint8_t *)&str[i], size - i);
        if (l == 7) {
            uint8_t j = str[i];
            fklCodeBuilderFmt(out, "\\x%02X", j);
            i++;
        } else if (l == 1) {
            if (str[i] == se)
                fklCodeBuilderFmt(out, "\\%c", se);
            else if (str[i] == '"')
                fklCodeBuilderPuts(out, "\\\"");
            else if (str[i] == '\'')
                fklCodeBuilderPuts(out, "\\'");
            else if (str[i] == '\\')
                fklCodeBuilderPuts(out, "\\\\");
            else if (isgraph(str[i]))
                fklCodeBuilderPutc(out, str[i]);
            else if (fklCodeBuilderPutEscSeq(out, str[i]))
                ;
            else {
                uint8_t j = str[i];
                fklCodeBuilderFmt(out, "\\x%02X", j);
            }
            i++;
        } else {
            for (unsigned int j = 0; j < l; j++)
                fklCodeBuilderPutc(out, str[i + j]);
            i += l;
        }
    }
}
static inline void print_prod_sym_as_dot(const FklGrammerSym *u,
        const FklRegexTable *rt,
        FklCodeBuilder *fp) {
    switch (u->type) {
    case FKL_TERM_BUILTIN:
        fklCodeBuilderPutc(fp, '|');
        fklCodeBuilderPuts(fp, u->b.t->name);
        fklCodeBuilderPutc(fp, '|');
        break;
    case FKL_TERM_REGEX: {
        const FklString *str = fklGetStringWithRegex(rt, u->re, NULL);
        fklCodeBuilderPuts(fp, "\\/'");
        print_string_as_dot(str->str, '"', str->size, fp);
        fklCodeBuilderPuts(fp, "'\\/");
    } break;
    case FKL_TERM_STRING:
    case FKL_TERM_KEYWORD: {
        const FklString *str = u->str;
        fklCodeBuilderPuts(fp, "\\\'");
        print_string_as_dot(str->str, '"', str->size, fp);
        fklCodeBuilderPuts(fp, "\\\'");
    } break;
    case FKL_TERM_NONTERM: {
        const FklString *str = FKL_VM_SYM(u->nt.sid);
        fklCodeBuilderPutc(fp, '|');
        print_string_as_dot(str->str, '|', str->size, fp);
        fklCodeBuilderPutc(fp, '|');
    } break;
    case FKL_TERM_IGNORE:
        fklCodeBuilderPuts(fp, "?e");
        break;
    case FKL_TERM_NONE:
    case FKL_TERM_EOF:
        FKL_UNREACHABLE();
        break;
    }
}

static inline int is_at_delim_sym(const FklLalrItem *item) {
    FklGrammerSym *sym = &item->prod->syms[item->idx];
    if (sym->type == FKL_TERM_IGNORE) {
        if (item->idx < item->prod->len - 1)
            return 1;
        else
            return 0;
    }
    return 0;
}

static inline FklGrammerSym *get_item_next(const FklLalrItem *item) {
    if (item->idx >= item->prod->len)
        return NULL;
    FklGrammerSym *sym = &item->prod->syms[item->idx];
    if (is_at_delim_sym(item)) {
        if (item->idx < item->prod->len - 1)
            return ++sym;
        else
            return NULL;
    }
    return sym;
}

static inline FklLalrItem lalr_item_init(FklGrammerProduction *prod,
        size_t idx,
        const FklLalrItemLookAhead *la) {
    FklLalrItem item = {
        .prod = prod,
        .idx = idx,
    };
    if (la)
        item.la = *la;
    else
        item.la = FKL_LALR_MATCH_NONE_INIT;
    return item;
}

static inline FklLalrItem get_item_advance(const FklLalrItem *i) {
    FklLalrItem item = {
        .prod = i->prod,
        .idx = i->idx + 1,
        .la = i->la,
    };
    if (item.idx < i->prod->len && is_at_delim_sym(i))
        ++item.idx;
    return item;
}

static inline int lalr_lookahead_cmp(const FklLalrItemLookAhead *la0,
        const FklLalrItemLookAhead *la1) {
    if (la0->t == la1->t) {
        switch (la0->t) {
        case FKL_TERM_IGNORE:
        case FKL_TERM_NONE:
        case FKL_TERM_EOF:
            return 0;
            break;
        case FKL_TERM_BUILTIN: {
            if (la0->b.t != la1->b.t) {
                uintptr_t f0 = (uintptr_t)la0->b.t;
                uintptr_t f1 = (uintptr_t)la1->b.t;
                if (f0 > f1)
                    return 1;
                else if (f0 < f1)
                    return -1;
                else
                    return 0;
            } else
                return builtin_grammer_sym_cmp(&la0->b, &la1->b);
        } break;
        case FKL_TERM_STRING:
        case FKL_TERM_KEYWORD:
            return fklStringCmp(la0->s, la1->s);
            break;
        case FKL_TERM_REGEX:
            return ((int64_t)la0->re->totalsize)
                 - ((int64_t)la1->re->totalsize);
            break;
        case FKL_TERM_NONTERM:
            FKL_UNREACHABLE();
            break;
        }
    } else {
        int t0 = la0->t;
        int t1 = la1->t;
        return t0 > t1 ? 1 : -1;
    }
    return 0;
}

static inline int lalr_item_cmp(const FklLalrItem *i0, const FklLalrItem *i1) {
    FklGrammerProduction *p0 = i0->prod;
    FklGrammerProduction *p1 = i1->prod;
    if (p0 == p1) {
        if (i0->idx < i1->idx)
            return -1;
        else if (i0->idx > i1->idx)
            return 1;
        else
            return lalr_lookahead_cmp(&i0->la, &i1->la);
    } else if (nonterm_lt(&p0->left, &p1->left))
        return -1;
    else if (nonterm_gt(&p0->left, &p1->left))
        return 1;
    else if (p0->len > p1->len)
        return -1;
    else if (p0->len < p1->len)
        return 1;
    else {
        size_t len = p0->len;
        FklGrammerSym *syms0 = p0->syms;
        FklGrammerSym *syms1 = p1->syms;
        for (size_t i = 0; i < len; i++) {
            int r = grammer_sym_cmp(&syms0[i], &syms1[i]);
            if (r)
                return r;
        }
        return 0;
    }
}

static int lalr_item_qsort_cmp(const void *i0, const void *i1) {
    return lalr_item_cmp((const FklLalrItem *)i0, (const FklLalrItem *)i1);
}

static inline void lalr_item_set_sort(FklLalrItemHashSet *itemSet) {
    size_t num = itemSet->count;
    FklLalrItem *item_array;
    if (num == 0)
        return;
    else {
        item_array = (FklLalrItem *)fklZmalloc(num * sizeof(FklLalrItem));
        FKL_ASSERT(item_array);
    }

    size_t i = 0;
    for (FklLalrItemHashSetNode *l = itemSet->first; l; l = l->next, i++) {
        item_array[i] = l->k;
    }
    qsort(item_array, num, sizeof(FklLalrItem), lalr_item_qsort_cmp);
    fklLalrItemHashSetClear(itemSet);
    for (i = 0; i < num; i++)
        fklLalrItemHashSetPut(itemSet, &item_array[i]);
    fklZfree(item_array);
}

static inline void lr0_item_set_closure(FklLalrItemHashSet *itemSet,
        FklGrammer *g) {
    int change;
    FklNontermHashSet sidSet;
    fklNontermHashSetInit(&sidSet);
    FklNontermHashSet changeSet;
    fklNontermHashSetInit(&changeSet);
    do {
        change = 0;
        for (FklLalrItemHashSetNode *l = itemSet->first; l; l = l->next) {
            FklGrammerSym *sym = get_item_next(&l->k);
            if (sym && sym->type == FKL_TERM_NONTERM) {
                const FklGrammerNonterm *left = &sym->nt;
                if (!fklNontermHashSetPut(&sidSet, left)) {
                    change = 1;
                    fklNontermHashSetPut(&changeSet, left);
                }
            }
        }

        for (FklNontermHashSetNode *lefts = changeSet.first; lefts;
                lefts = lefts->next) {
            FklGrammerProduction *prod =
                    fklGetGrammerProductions(g, lefts->k.group, lefts->k.sid);
            for (; prod; prod = prod->next) {
                FklLalrItem item = lalr_item_init(prod, 0, NULL);
                fklLalrItemHashSetPut(itemSet, &item);
            }
        }
        fklNontermHashSetClear(&changeSet);
    } while (change);
    fklNontermHashSetUninit(&sidSet);
    fklNontermHashSetUninit(&changeSet);
}

static inline void lr0_item_set_copy_and_closure(FklLalrItemHashSet *dst,
        const FklLalrItemHashSet *itemSet,
        FklGrammer *g) {
    for (FklLalrItemHashSetNode *il = itemSet->first; il; il = il->next) {
        fklLalrItemHashSetPut(dst, &il->k);
    }
    lr0_item_set_closure(dst, g);
}

static inline void init_first_item_set(FklLalrItemHashSet *itemSet,
        FklGrammerProduction *prod) {
    FklLalrItem item = lalr_item_init(prod, 0, NULL);
    fklLalrItemHashSetInit(itemSet);
    fklLalrItemHashSetPut(itemSet, &item);
}

static inline void print_lookahead(const FklLalrItemLookAhead *la,
        const FklRegexTable *rt,
        FklCodeBuilder *build) {
    switch (la->t) {
    case FKL_TERM_STRING:
        fklPrintStringLiteral2(la->s, build);
        break;
    case FKL_TERM_KEYWORD:
        fklPrintSymbolLiteral2(la->s, build);
        break;
    case FKL_TERM_EOF:
        fklCodeBuilderPuts(build, "$$");
        break;
    case FKL_TERM_BUILTIN:
        fklCodeBuilderPuts(build, la->b.t->name);
        if (la->b.len) {
            fklCodeBuilderPutc(build, '[');
            size_t i = 0;
            for (; i < la->b.len - 1; ++i) {
                fklPrintStringLiteral2(la->b.args[i], build);
                fklCodeBuilderPuts(build, " , ");
            }
            fklPrintStringLiteral2(la->b.args[i], build);
            fklCodeBuilderPutc(build, ']');
        }
        break;
    case FKL_TERM_NONE:
        fklCodeBuilderPuts(build, "()");
        break;
    case FKL_TERM_IGNORE:
        fklCodeBuilderPuts(build, "?e");
        break;
    case FKL_TERM_REGEX:
        print_as_regex(fklGetStringWithRegex(rt, la->re, NULL), build);
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void print_item(FklVMgc *gc,
        const FklLalrItem *item,
        const FklRegexTable *rt,
        FklCodeBuilder *build) {
    size_t i = 0;
    size_t idx = item->idx;
    FklGrammerProduction *prod = item->prod;
    size_t len = prod->len;
    FklGrammerSym *syms = prod->syms;
    if (!is_Sq_nt(&prod->left))
        fklPrintString2(FKL_VM_SYM(prod->left.sid), build);
    else
        fklCodeBuilderPuts(build, "S'");
    fklCodeBuilderPuts(build, " ->");
    for (; i < idx; i++) {
        fklCodeBuilderPutc(build, ' ');
        print_prod_sym(gc, &syms[i], rt, build);
    }
    fklCodeBuilderPuts(build, " *");
    for (; i < len; i++) {
        fklCodeBuilderPutc(build, ' ');
        print_prod_sym(gc, &syms[i], rt, build);
    }
    fklCodeBuilderPuts(build, " ## ");
    print_lookahead(&item->la, rt, build);
}

void fklPrintItemSet(FklVMgc *gc,
        const FklLalrItemHashSet *itemSet,
        const FklGrammer *g,
        FklCodeBuilder *build) {
    FklLalrItem const *curItem = NULL;
    for (FklLalrItemHashSetNode *list = itemSet->first; list;
            list = list->next) {
        if (!curItem || list->k.idx != curItem->idx
                || list->k.prod != curItem->prod) {
            if (curItem)
                fklCodeBuilderPutc(build, '\n');
            curItem = &list->k;
            print_item(gc, curItem, &g->regexes, build);
        } else {
            fklCodeBuilderPuts(build, " , ");
            print_lookahead(&list->k.la, &g->regexes, build);
        }
    }
    fklCodeBuilderPutc(build, '\n');
}

typedef struct GraGetLaFirstSetCacheKey {
    const FklGrammerProduction *prod;
    uint32_t idx;
} GraGetLaFirstSetCacheKey;

typedef struct GraGetLaFirstSetCacheItem {
    FklLookAheadHashSet first;
    int has_epsilon;
} GraGetLaFirstSetCacheItem;

// GraLaFirstSetCacheHashMap
#define FKL_HASH_TYPE_PREFIX Gra
#define FKL_HASH_METHOD_PREFIX gra
#define FKL_HASH_KEY_TYPE GraGetLaFirstSetCacheKey
#define FKL_HASH_VAL_TYPE GraGetLaFirstSetCacheItem
#define FKL_HASH_VAL_INIT(A, B) FKL_UNREACHABLE()
#define FKL_HASH_VAL_UNINIT(V) fklLookAheadHashSetUninit(&(V)->first)
#define FKL_HASH_KEY_EQUAL(A, B) (A)->prod == (B)->prod && (A)->idx == (B)->idx
#define FKL_HASH_KEY_HASH                                                      \
    return fklHashCombine(FKL_TYPE_CAST(uintptr_t, pk->prod)                   \
                                  / alignof(FklGrammerProduction),             \
            pk->idx);
#define FKL_HASH_ELM_NAME LaFirstSetCache
#include <fakeLisp/cont/hash.h>

// GraItemSetQueue
#define FKL_QUEUE_TYPE_PREFIX Gra
#define FKL_QUEUE_METHOD_PREFIX gra
#define FKL_QUEUE_ELM_TYPE FklLalrItemSetHashMapElm *
#define FKL_QUEUE_ELM_TYPE_NAME ItemSet
#include <fakeLisp/cont/queue.h>

typedef struct {
    FklGrammerSym sym;
    int allow_ignore;
    int is_at_delim;
} GraLinkSym;

static void item_set_add_link(FklLalrItemSetHashMapElm *src,
        const GraLinkSym *sym,
        FklLalrItemSetHashMapElm *dst) {
    FklLalrItemSetLink *l =
            (FklLalrItemSetLink *)fklZmalloc(sizeof(FklLalrItemSetLink));
    FKL_ASSERT(l);
    l->sym = sym->sym;
    l->allow_ignore = sym->allow_ignore;
    l->dst = dst;
    l->next = src->v.links;
    src->v.links = l;
}

static inline int grammer_sym_equal(const FklGrammerSym *s0,
        const FklGrammerSym *s1) {
    if (s0->type != s1->type)
        return 0;
    switch (s0->type) {
    case FKL_TERM_BUILTIN:
        return fklBuiltinGrammerSymEqual(&s0->b, &s1->b);
        break;
    case FKL_TERM_REGEX:
        return s0->re == s1->re;
        break;
    case FKL_TERM_STRING:
    case FKL_TERM_KEYWORD:
    case FKL_TERM_NONTERM:
        return fklNontermEqual(&s0->nt, &s1->nt);
        break;
    case FKL_TERM_IGNORE:
        return 1;
        break;
    case FKL_TERM_EOF:
    case FKL_TERM_NONE:
        FKL_UNREACHABLE();
        break;
    }
    return 0;
}

static int gra_link_sym_equal(const GraLinkSym *ss0, const GraLinkSym *ss1) {
    return ss0->allow_ignore == ss1->allow_ignore
        && ss0->is_at_delim == ss1->is_at_delim
        && grammer_sym_equal(&ss0->sym, &ss1->sym);
}

static inline uintptr_t gra_link_sym_hash(const GraLinkSym *ss) {
    const FklGrammerSym *s = &ss->sym;
    uintptr_t seed =
            (s->type == FKL_TERM_BUILTIN ? fklBuiltinGrammerSymHash(&s->b)
                    : (s->type == FKL_TERM_REGEX)
                            ? fklHash64Shift(FKL_TYPE_CAST(uintptr_t, s->re)
                                             / alignof(FklRegexCode))
                            : fklNontermHash(&s->nt));
    seed = fklHashCombine(seed, ss->allow_ignore);
    return fklHashCombine(seed, ss->is_at_delim);
}

// GraSymbolHashSet
#define FKL_HASH_TYPE_PREFIX Gra
#define FKL_HASH_METHOD_PREFIX gra
#define FKL_HASH_KEY_TYPE GraLinkSym
#define FKL_HASH_KEY_EQUAL(A, B) gra_link_sym_equal(A, B)
#define FKL_HASH_KEY_HASH return gra_link_sym_hash(pk)
#define FKL_HASH_ELM_NAME Symbol
#include <fakeLisp/cont/hash.h>

static inline int is_gra_link_sym_allow_ignore(const GraSymbolHashSet *checked,
        const FklGrammerNonterm left) {
    FklGrammerSym prod_left_sym = { .type = FKL_TERM_NONTERM, .nt = left };

    return graSymbolHashSetHas2(checked,
                   (GraLinkSym){ .sym = prod_left_sym,
                       .allow_ignore = 1,
                       .is_at_delim = 1 })
        || graSymbolHashSetHas2(checked,
                (GraLinkSym){ .sym = prod_left_sym,
                    .allow_ignore = 1,
                    .is_at_delim = 0 });
}

static inline void add_gra_link_syms(GraSymbolHashSet *checked,
        const FklLalrItemHashSet *items_closure) {
    for (FklLalrItemHashSetNode *l = items_closure->first; l; l = l->next) {
        FklGrammerSym *sym = get_item_next(&l->k);
        if (sym && sym->type == FKL_TERM_NONTERM) {
            int allow_ignore =
                    is_gra_link_sym_allow_ignore(checked, l->k.prod->left);

            GraLinkSym ss = { .sym = *sym,
                .allow_ignore = allow_ignore || is_at_delim_sym(&l->k),
                .is_at_delim = is_at_delim_sym(&l->k) };
            graSymbolHashSetPut(checked, &ss);
        }
    }

    for (FklLalrItemHashSetNode *l = items_closure->first; l; l = l->next) {
        FklGrammerSym *sym = get_item_next(&l->k);
        if (sym && sym->type != FKL_TERM_NONTERM) {
            int allow_ignore =
                    l->k.idx == 0
                    && is_gra_link_sym_allow_ignore(checked, l->k.prod->left);

            int is_at_delim = is_at_delim_sym(&l->k);
            GraLinkSym ss = { .sym = *sym,
                .allow_ignore = allow_ignore || is_at_delim,
                .is_at_delim = is_at_delim };
            graSymbolHashSetPut(checked, &ss);
        }
    }
}

static inline void lr0_item_set_goto(GraSymbolHashSet *checked,
        FklLalrItemSetHashMapElm *itemset,
        FklLalrItemSetHashMap *itemsetSet,
        FklGrammer *g,
        GraItemSetQueue *pending) {
    FklLalrItemHashSet const *items = &itemset->k;
    FklLalrItemHashSet itemsClosure;
    fklLalrItemHashSetInit(&itemsClosure);
    lr0_item_set_copy_and_closure(&itemsClosure, items, g);

    add_gra_link_syms(checked, &itemsClosure);

    lalr_item_set_sort(&itemsClosure);

    for (GraSymbolHashSetNode *ll = checked->first; ll; ll = ll->next) {
        FklLalrItemHashSet newItems;
        fklLalrItemHashSetInit(&newItems);
        for (FklLalrItemHashSetNode *l = itemsClosure.first; l; l = l->next) {
            FklGrammerSym *next = get_item_next(&l->k);
            if (next == NULL || !grammer_sym_equal(&ll->k.sym, next))
                continue;
            if (ll->k.is_at_delim == is_at_delim_sym(&l->k)) {
                FklLalrItem item = get_item_advance(&l->k);
                fklLalrItemHashSetPut(&newItems, &item);
            }
        }
        FklLalrItemSetHashMapElm *itemsetptr =
                fklLalrItemSetHashMapAt(itemsetSet, &newItems);
        if (!itemsetptr) {
            itemsetptr =
                    fklLalrItemSetHashMapInsert(itemsetSet, &newItems, NULL);
            graItemSetQueuePush2(pending, itemsetptr);
        } else
            fklLalrItemHashSetUninit(&newItems);
        item_set_add_link(itemset, &ll->k, itemsetptr);
    }
    fklLalrItemHashSetUninit(&itemsClosure);
}

FklLalrItemSetHashMap *fklGenerateLr0Items(FklGrammer *grammer) {
    clear_analysis_table(grammer, grammer->aTable.num - 1);
    FklLalrItemSetHashMap *itemstate_set = fklLalrItemSetHashMapCreate();
    const FklGrammerNonterm left = {
        .group = 0,
        .sid = 0,
    };
    FklGrammerProduction *prod =
            *fklProdHashMapGet(&grammer->productions, &left);
    FklLalrItemHashSet items;
    init_first_item_set(&items, prod);
    FklLalrItemSetHashMapElm *itemsetptr =
            fklLalrItemSetHashMapInsert(itemstate_set, &items, NULL);
    GraItemSetQueue pending;
    graItemSetQueueInit(&pending);
    graItemSetQueuePush2(&pending, itemsetptr);
    GraSymbolHashSet checked;
    graSymbolHashSetInit(&checked);
    while (!graItemSetQueueIsEmpty(&pending)) {
        FklLalrItemSetHashMapElm *itemsetptr = *graItemSetQueuePop(&pending);
        lr0_item_set_goto(&checked,
                itemsetptr,
                itemstate_set,
                grammer,
                &pending);
        graSymbolHashSetClear(&checked);
    }
    graSymbolHashSetUninit(&checked);
    graItemSetQueueUninit(&pending);
    return itemstate_set;
}

static inline FklLookAheadHashSet *get_first_set_from_first_sets(
        const FklGrammer *g,
        const FklGrammerProduction *prod,
        uint32_t idx,
        GraLaFirstSetCacheHashMap *cache,
        int *pHasEpsilon) {
    size_t len = prod->len;
    if (idx >= len) {
        *pHasEpsilon = 1;
        return NULL;
    }
    GraGetLaFirstSetCacheKey key = { .prod = prod, .idx = idx };
    GraGetLaFirstSetCacheItem *item =
            graLaFirstSetCacheHashMapAdd(cache, &key, NULL);
    if (item->first.buckets) {
        *pHasEpsilon = item->has_epsilon;
        return &item->first;
    } else {
        FklLookAheadHashSet *first = &item->first;
        fklLookAheadHashSetInit(first);
        item->has_epsilon = 0;
        size_t lastIdx = len - 1;
        int hasEpsilon = 0;
        const FklFirstSetHashMap *firstSets = &g->firstSets;
        for (uint32_t i = idx; i < len; i++) {
            const FklGrammerSym *sym = &prod->syms[i];

            FklLalrItemLookAhead la = { .t = sym->type };
            switch (sym->type) {
            case FKL_TERM_BUILTIN: {
                int r = is_builtin_terminal_match_epsilon(g, &sym->b);
                la.b = sym->b;
                fklLookAheadHashSetPut(first, &la);
                if (r)
                    hasEpsilon = i == lastIdx;
                else
                    goto break_loop;
            } break;
            case FKL_TERM_REGEX: {
                uint32_t r = is_regex_match_epsilon(sym->re);
                la.re = sym->re;
                fklLookAheadHashSetPut(first, &la);
                if (r)
                    hasEpsilon = i == lastIdx;
                else
                    goto break_loop;
            } break;
            case FKL_TERM_NONTERM: {
                const FklFirstSetItem *firstSetItem =
                        fklFirstSetHashMapGet(firstSets, &sym->nt);
                for (FklLookAheadHashSetNode *symList =
                                firstSetItem->first.first;
                        symList;
                        symList = symList->next) {
                    fklLookAheadHashSetPut(first, &symList->k);
                }
                if (firstSetItem->hasEpsilon && i == lastIdx)
                    hasEpsilon = 1;
                if (!firstSetItem->hasEpsilon)
                    goto break_loop;
            } break;
            case FKL_TERM_IGNORE: {
                hasEpsilon = i == lastIdx;
                fklLookAheadHashSetPut(first, &la);
            } break;
            case FKL_TERM_STRING:
            case FKL_TERM_KEYWORD: {
                const FklString *s = sym->str;
                if (s->size == 0)
                    hasEpsilon = i == lastIdx;
                else {
                    la.s = s;
                    fklLookAheadHashSetPut(first, &la);
                    goto break_loop;
                }
            } break;
            case FKL_TERM_EOF:
            case FKL_TERM_NONE:
                FKL_UNREACHABLE();
                break;
            }
        }
    break_loop:
        *pHasEpsilon = hasEpsilon;
        item->has_epsilon = hasEpsilon;
        return first;
    }
}

static inline FklLookAheadHashSet *get_la_first_set(const FklGrammer *g,
        const FklGrammerProduction *prod,
        uint32_t beta,
        GraLaFirstSetCacheHashMap *cache,
        int *hasEpsilon) {
    return get_first_set_from_first_sets(g, prod, beta, cache, hasEpsilon);
}

static inline void lr1_item_set_closure(FklLalrItemHashSet *itemSet,
        FklGrammer *g,
        GraLaFirstSetCacheHashMap *cache) {
    FklLalrItemHashSet pendingSet;
    FklLalrItemHashSet changeSet;
    fklLalrItemHashSetInit(&pendingSet);
    fklLalrItemHashSetInit(&changeSet);

    for (FklLalrItemHashSetNode *l = itemSet->first; l; l = l->next) {
        fklLalrItemHashSetPut(&pendingSet, &l->k);
    }

    FklLalrItemHashSet *processing_set = &pendingSet;
    FklLalrItemHashSet *next_set = &changeSet;
    while (processing_set->count) {
        for (FklLalrItemHashSetNode *l = processing_set->first; l;
                l = l->next) {
            FklGrammerSym *next = get_item_next(&l->k);
            if (next && next->type == FKL_TERM_NONTERM) {
                uint32_t beta = l->k.idx + 1;
                if (is_at_delim_sym(&l->k))
                    ++beta;
                int hasEpsilon = 0;
                FklLookAheadHashSet *first = get_la_first_set(g,
                        l->k.prod,
                        beta,
                        cache,
                        &hasEpsilon);
                const FklGrammerNonterm *left = &next->nt;
                FklGrammerProduction *prods =
                        fklGetGrammerProductions(g, left->group, left->sid);
                if (first) {
                    for (FklLookAheadHashSetNode *first_list = first->first;
                            first_list;
                            first_list = first_list->next) {
                        for (FklGrammerProduction *prod = prods; prod;
                                prod = prod->next) {
                            FklLalrItem newItem = { .prod = prod,
                                .la = first_list->k,
                                .idx = 0 };
                            if (!fklLalrItemHashSetPut(itemSet, &newItem))
                                fklLalrItemHashSetPut(next_set, &newItem);
                        }
                    }
                }
                if (hasEpsilon) {
                    for (FklGrammerProduction *prod = prods; prod;
                            prod = prod->next) {
                        FklLalrItem newItem = { .prod = prod,
                            .la = l->k.la,
                            .idx = 0 };
                        if (!fklLalrItemHashSetPut(itemSet, &newItem))
                            fklLalrItemHashSetPut(next_set, &newItem);
                    }
                }
            }
        }

        fklLalrItemHashSetClear(processing_set);
        FklLalrItemHashSet *t = processing_set;
        processing_set = next_set;
        next_set = t;
    }
    fklLalrItemHashSetUninit(&changeSet);
    fklLalrItemHashSetUninit(&pendingSet);
}

static inline void add_lookahead_spread(FklLalrItemSetHashMapElm *itemset,
        const FklLalrItem *src,
        FklLalrItemHashSet *dstItems,
        const FklLalrItem *dst) {
    FklLookAheadSpreads *sp =
            (FklLookAheadSpreads *)fklZmalloc(sizeof(FklLookAheadSpreads));
    FKL_ASSERT(sp);
    sp->next = itemset->v.spreads;
    sp->dstItems = dstItems;
    sp->src = *src;
    sp->dst = *dst;
    itemset->v.spreads = sp;
}

static inline void check_lookahead_self_generated_and_spread(FklGrammer *g,
        FklLalrItemSetHashMapElm *itemset,
        GraLaFirstSetCacheHashMap *cache) {
    FklLalrItemHashSet const *items = &itemset->k;
    FklLalrItemHashSet closure;
    fklLalrItemHashSetInit(&closure);
    for (FklLalrItemHashSetNode *il = items->first; il; il = il->next) {
        if (il->k.la.t == FKL_TERM_NONE) {
            FklLalrItem item = { .prod = il->k.prod,
                .idx = il->k.idx,
                .la = FKL_LALR_MATCH_NONE_INIT };
            fklLalrItemHashSetPut(&closure, &item);
            lr1_item_set_closure(&closure, g, cache);
            for (FklLalrItemHashSetNode *cl = closure.first; cl;
                    cl = cl->next) {
                FklLalrItem i = cl->k;
                const FklGrammerSym *s = get_item_next(&i);
                int is_at_delim_v = s && is_at_delim_sym(&i);
                if (is_at_delim_v)
                    i.idx += 2;
                else
                    ++i.idx;
                for (const FklLalrItemSetLink *x = itemset->v.links; x;
                        x = x->next) {
                    if (x->dst == itemset)
                        continue;
                    const FklGrammerSym *xsym = &x->sym;
                    if (s == NULL || !grammer_sym_equal(s, xsym))
                        continue;

                    if (s->type != FKL_TERM_NONTERM
                            || x->allow_ignore == is_at_delim_v
                            || item.prod != i.prod) {
                        if (i.la.t == FKL_TERM_NONE) {
                            FklLalrItemHashSet *k =
                                    FKL_TYPE_CAST(FklLalrItemHashSet *,
                                            &x->dst->k);
                            // 这些操作可能需要重新计算哈希值
                            add_lookahead_spread(itemset, &item, k, &i);
                        } else {
                            FklLalrItemHashSet *k =
                                    FKL_TYPE_CAST(FklLalrItemHashSet *,
                                            &x->dst->k);
                            fklLalrItemHashSetPut(k, &i);
                        }
                    }
                }
            }
            fklLalrItemHashSetClear(&closure);
        }
    }
    fklLalrItemHashSetUninit(&closure);
}

static inline int lookahead_spread(FklLalrItemSetHashMapElm *itemset) {
    int change = 0;
    FklLalrItemHashSet const *items = &itemset->k;
    for (FklLookAheadSpreads *sp = itemset->v.spreads; sp; sp = sp->next) {
        FklLalrItem *srcItem = &sp->src;
        FklLalrItem *dstItem = &sp->dst;
        FklLalrItemHashSet *dstItems = sp->dstItems;
        for (FklLalrItemHashSetNode *il = items->first; il; il = il->next) {
            if (il->k.la.t != FKL_TERM_NONE && il->k.prod == srcItem->prod
                    && il->k.idx == srcItem->idx) {
                FklLalrItem newItem = *dstItem;
                newItem.la = il->k.la;
                change |= !fklLalrItemHashSetPut(dstItems, &newItem);
            }
        }
    }
    return change;
}

static inline void init_lalr_lookahead(FklLalrItemSetHashMap *lr0,
        FklGrammer *g,
        GraLaFirstSetCacheHashMap *cache) {
    for (FklLalrItemSetHashMapNode *isl = lr0->first; isl; isl = isl->next) {
        check_lookahead_self_generated_and_spread(g, &isl->elm, cache);
    }
    FklLalrItemSetHashMapNode *isl = lr0->first;
    if (isl == NULL)
        return;
    for (FklLalrItemHashSetNode *il = isl->k.first; il; il = il->next) {
        FklLalrItem item = il->k;
        item.la = FKL_LALR_MATCH_EOF_INIT;
        fklLalrItemHashSetPut(FKL_TYPE_CAST(FklLalrItemHashSet *, &isl->k),
                &item);
    }
}

static inline void add_lookahead_to_items(FklLalrItemHashSet *items,
        FklGrammer *g,
        GraLaFirstSetCacheHashMap *cache) {
    FklLalrItemHashSet add;
    fklLalrItemHashSetInit(&add);
    for (FklLalrItemHashSetNode *il = items->first; il; il = il->next) {
        if (il->k.la.t != FKL_TERM_NONE)
            fklLalrItemHashSetPut(&add, &il->k);
    }
    fklLalrItemHashSetClear(items);
    for (FklLalrItemHashSetNode *il = add.first; il; il = il->next) {
        fklLalrItemHashSetPut(items, &il->k);
    }
    fklLalrItemHashSetUninit(&add);
    lr1_item_set_closure(items, g, cache);
    lalr_item_set_sort(items);
}

static inline void add_lookahead_for_all_item_set(FklLalrItemSetHashMap *lr0,
        FklGrammer *g,
        GraLaFirstSetCacheHashMap *cache) {
    for (FklLalrItemSetHashMapNode *isl = lr0->first; isl; isl = isl->next) {
        add_lookahead_to_items(FKL_TYPE_CAST(FklLalrItemHashSet *, &isl->k),
                g,
                cache);
    }
}

void fklLr0ToLalrItems(FklLalrItemSetHashMap *lr0, FklGrammer *g) {
    GraLaFirstSetCacheHashMap cache;
    graLaFirstSetCacheHashMapInit(&cache);
    init_lalr_lookahead(lr0, g, &cache);
    int change;
    do {
        change = 0;
        for (FklLalrItemSetHashMapNode *isl = lr0->first; isl;
                isl = isl->next) {
            change |= lookahead_spread(&isl->elm);
        }
    } while (change);
    add_lookahead_for_all_item_set(lr0, g, &cache);
    graLaFirstSetCacheHashMapUninit(&cache);
}

// GraItemStateIdxHashMap
#define FKL_HASH_TYPE_PREFIX Gra
#define FKL_HASH_METHOD_PREFIX gra
#define FKL_HASH_KEY_TYPE FklLalrItemSetHashMapElm const *
#define FKL_HASH_VAL_TYPE size_t
#define FKL_HASH_ELM_NAME ItemStateIdx
#define FKL_HASH_KEY_HASH                                                      \
    return fklHash64Shift(FKL_TYPE_CAST(uintptr_t, (*pk))                      \
                          / alignof(FklLalrItemSetHashMapElm));
#include <fakeLisp/cont/hash.h>

static inline void print_lookahead_as_dot(const FklLalrItemLookAhead *la,
        const FklRegexTable *rt,
        FklCodeBuilder *fp) {
    switch (la->t) {
    case FKL_TERM_STRING:
    case FKL_TERM_KEYWORD: {
        fklCodeBuilderPuts(fp, "\\\'");
        const FklString *str = la->s;
        print_string_as_dot(str->str, '\'', str->size, fp);
        fklCodeBuilderPuts(fp, "\\\'");
    } break;
    case FKL_TERM_EOF:
        fklCodeBuilderPuts(fp, "$$");
        break;
    case FKL_TERM_BUILTIN:
        fklCodeBuilderFmt(fp, "|%s|", la->b.t->name);
        break;
    case FKL_TERM_NONE:
        fklCodeBuilderPuts(fp, "()");
        break;
    case FKL_TERM_IGNORE:
        fklCodeBuilderPuts(fp, "?e");
        break;
    case FKL_TERM_REGEX: {
        fklCodeBuilderPuts(fp, "\\/\'");
        const FklString *str = fklGetStringWithRegex(rt, la->re, NULL);
        print_string_as_dot(str->str, '\'', str->size, fp);
        fklCodeBuilderPuts(fp, "\\/\'");
    } break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void print_item_as_dot(const FklLalrItem *item,
        const FklRegexTable *rt,
        FklCodeBuilder *fp) {
    size_t i = 0;
    size_t idx = item->idx;
    FklGrammerProduction *prod = item->prod;
    size_t len = prod->len;
    FklGrammerSym *syms = prod->syms;
    if (!is_Sq_nt(&prod->left)) {
        const FklString *str = FKL_VM_SYM(prod->left.sid);
        fklCodeBuilderPutc(fp, '|');
        print_string_as_dot(str->str, '"', str->size, fp);
        fklCodeBuilderPutc(fp, '|');
    } else
        fklCodeBuilderPuts(fp, "S'");
    fklCodeBuilderPuts(fp, " ->");
    for (; i < idx; i++) {
        fklCodeBuilderPutc(fp, ' ');
        print_prod_sym_as_dot(&syms[i], rt, fp);
    }
    fklCodeBuilderPuts(fp, " *");
    for (; i < len; i++) {
        fklCodeBuilderPutc(fp, ' ');
        print_prod_sym_as_dot(&syms[i], rt, fp);
    }
    fklCodeBuilderPuts(fp, " , ");
    print_lookahead_as_dot(&item->la, rt, fp);
}

static inline void print_item_set_as_dot(const FklLalrItemHashSet *itemSet,
        const FklGrammer *g,
        FklCodeBuilder *fp) {
    FklLalrItem const *curItem = NULL;
    for (FklLalrItemHashSetNode *list = itemSet->first; list;
            list = list->next) {
        if (!curItem || list->k.idx != curItem->idx
                || list->k.prod != curItem->prod) {
            if (curItem)
                fklCodeBuilderPuts(fp, "\\l\\\n");
            curItem = &list->k;
            print_item_as_dot(curItem, &g->regexes, fp);
        } else {
            fklCodeBuilderPuts(fp, " / ");
            print_lookahead_as_dot(&list->k.la, &g->regexes, fp);
        }
    }
    fklCodeBuilderPuts(fp, "\\l\\\n");
}

static inline void print_lalr_item(FklVMgc *gc,
        const FklLalrItem *item,
        const FklStringTable *tt,
        const FklRegexTable *rt,
        FklCodeBuilder *build) {
    size_t i = 0;
    size_t idx = item->idx;
    FklGrammerProduction *prod = item->prod;
    size_t len = prod->len;
    FklGrammerSym *syms = prod->syms;
    if (!is_Sq_nt(&prod->left)) {
        if (prod->left.group) {
            fklCodeBuilderPutc(build, '(');

            fklPrintSymbolLiteral2(FKL_VM_SYM(prod->left.group), build);

            fklCodeBuilderPuts(build, " , ");

            fklPrintSymbolLiteral2(FKL_VM_SYM(prod->left.sid), build);

            fklCodeBuilderPutc(build, ')');
        } else {
            fklCodeBuilderPutc(build, '(');
            fklPrintSymbolLiteral2(FKL_VM_SYM(prod->left.sid), build);
            fklCodeBuilderPutc(build, ')');
        }
    } else
        fklCodeBuilderPuts(build, "S'");
    fklCodeBuilderPuts(build, " ->");
    for (; i < idx; i++) {
        fklCodeBuilderPutc(build, ' ');
        print_prod_sym(gc, &syms[i], rt, build);
    }
    fklCodeBuilderPuts(build, " *");
    for (; i < len; i++) {
        fklCodeBuilderPutc(build, ' ');
        print_prod_sym(gc, &syms[i], rt, build);
    }
}

void fklPrintItemStateSetAsDot(FklVMgc *gc,
        const FklLalrItemSetHashMap *i,
        const FklGrammer *g,
        FILE *fp) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrintItemStateSet2(gc, i, g, &builder);
}

void fklPrintItemStateSetAsDot2(FklVMgc *gc,
        const FklLalrItemSetHashMap *i,
        const FklGrammer *g,
        FklCodeBuilder *fp) {
    fklCodeBuilderPuts(fp, "digraph \"items-lalr\"{\n");
    fklCodeBuilderPuts(fp, "\trankdir=\"LR\"\n");
    fklCodeBuilderPuts(fp, "\tranksep=1\n");
    fklCodeBuilderPuts(fp, "\tgraph[overlap=false];\n");
    GraItemStateIdxHashMap idxTable;
    graItemStateIdxHashMapInit(&idxTable);
    size_t idx = 0;
    for (const FklLalrItemSetHashMapNode *l = i->first; l; l = l->next, idx++) {
        graItemStateIdxHashMapPut2(&idxTable, &l->elm, idx);
    }
    for (const FklLalrItemSetHashMapNode *ll = i->first; ll; ll = ll->next) {
        const FklLalrItemHashSet *i = &ll->k;
        idx = *graItemStateIdxHashMapGet2NonNull(&idxTable, &ll->elm);
        fklCodeBuilderFmt(fp,
                "\t\"I%" PRIu64
                "\"[fontname=\"Courier\" nojustify=true shape=\"box\"label =\"I%" PRIu64
                "\\l\\\n",
                idx,
                idx);
        print_item_set_as_dot(i, g, fp);
        fklCodeBuilderPuts(fp, "\"]\n");
        for (FklLalrItemSetLink *l = ll->v.links; l; l = l->next) {
            FklLalrItemSetHashMapElm *dst = l->dst;
            size_t *c = graItemStateIdxHashMapGet2NonNull(&idxTable, dst);
            fklCodeBuilderFmt(fp,
                    "\tI%" PRIu64 "->I%" PRIu64
                    "[fontname=\"Courier\" label=\"",
                    idx,
                    *c);
            print_prod_sym_as_dot(&l->sym, &g->regexes, fp);
            fklCodeBuilderPuts(fp, "\"]\n");
        }
        fklCodeBuilderPutc(fp, '\n');
    }
    graItemStateIdxHashMapUninit(&idxTable);
    fklCodeBuilderPuts(fp, "}");
}

static inline FklAnalysisStateAction *create_shift_action(
        const FklGrammerSym *sym,
        int allow_ignore,
        const FklStringTable *tt,
        FklAnalysisState *state) {
    FklAnalysisStateAction *action = (FklAnalysisStateAction *)fklZcalloc(1,
            sizeof(FklAnalysisStateAction));
    FKL_ASSERT(action);
    action->next = NULL;
    action->action = FKL_ANALYSIS_SHIFT;
    action->state = state;
    action->match.allow_ignore = allow_ignore;
    action->match.t = sym->type;
    switch (sym->type) {
    case FKL_TERM_BUILTIN:
        action->match.func = sym->b;
        break;
    case FKL_TERM_REGEX:
        action->match.re = sym->re;
        break;
    case FKL_TERM_KEYWORD:
    case FKL_TERM_STRING:
        action->match.str = sym->str;
        break;
    case FKL_TERM_IGNORE:
    case FKL_TERM_EOF:
    case FKL_TERM_NONE:
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }

    return action;
}

static inline FklAnalysisStateGoto *create_state_goto(const FklGrammerSym *sym,
        int allow_ignore,
        const FklStringTable *tt,
        FklAnalysisStateGoto *next,
        FklAnalysisState *state) {
    FklAnalysisStateGoto *gt =
            (FklAnalysisStateGoto *)fklZmalloc(sizeof(FklAnalysisStateGoto));
    FKL_ASSERT(gt);
    gt->next = next;
    gt->state = state;
    gt->nt = sym->nt;
    gt->allow_ignore = allow_ignore;
    return gt;
}

static inline int lalr_lookahead_and_action_match_equal(
        const FklAnalysisStateActionMatch *match,
        const FklLalrItemLookAhead *la) {
    if (match->t == la->t) {
        switch (match->t) {
        case FKL_TERM_STRING:
        case FKL_TERM_KEYWORD:
            return match->str == la->s;
            break;
        case FKL_TERM_BUILTIN:
            return match->func.t == la->b.t
                && fklBuiltinGrammerSymEqual(&match->func, &la->b);
            break;
        case FKL_TERM_REGEX:
            return match->re == la->re;
            break;
        case FKL_TERM_EOF:
        case FKL_TERM_NONE:
        case FKL_TERM_IGNORE:
            return 0;
            break;
        case FKL_TERM_NONTERM:
            FKL_UNREACHABLE();
            break;
        }
    }
    return 0;
}

static int check_reduce_conflict(const FklAnalysisStateAction *actions,
        const FklLalrItemLookAhead *la) {
    for (; actions; actions = actions->next)
        if (lalr_lookahead_and_action_match_equal(&actions->match, la))
            return 1;
    return 0;
}

static inline void init_action_with_lookahead(FklAnalysisStateAction *action,
        const FklLalrItemLookAhead *la) {
    action->match.t = la->t;
    switch (action->match.t) {
    case FKL_TERM_STRING:
    case FKL_TERM_KEYWORD:
        action->match.str = la->s;
        break;
    case FKL_TERM_BUILTIN:
        action->match.func = la->b;
        break;
    case FKL_TERM_REGEX:
        action->match.re = la->re;
        break;
    case FKL_TERM_NONE:
    case FKL_TERM_EOF:
    case FKL_TERM_IGNORE:
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

size_t fklComputeProdActualLen(size_t len, const FklGrammerSym *syms) {
    size_t delim_len = 0;
    for (size_t i = 0; i < len; ++i)
        if (syms[i].type == FKL_TERM_IGNORE)
            ++delim_len;

    return len - delim_len;
}

static inline int add_reduce_action(FklGrammerSymType cur_type,
        FklAnalysisState *curState,
        const FklGrammerProduction *prod,
        const FklLalrItemLookAhead *la) {
    if (check_reduce_conflict(curState->state.action, la))
        return 1;
    FklAnalysisStateAction *action = (FklAnalysisStateAction *)fklZcalloc(1,
            sizeof(FklAnalysisStateAction));
    FKL_ASSERT(action);
    init_action_with_lookahead(action, la);
    if (is_Sq_nt(&prod->left))
        action->action = FKL_ANALYSIS_ACCEPT;
    else {
        action->action = FKL_ANALYSIS_REDUCE;
        action->prod = prod;
        action->actual_len = fklComputeProdActualLen(prod->len, prod->syms);
    }
    FklAnalysisStateAction **pa = &curState->state.action;

    for (; *pa; pa = &(*pa)->next) {
        FklAnalysisStateAction *curAction = *pa;
        if (curAction->match.t == cur_type)
            break;
    }

    switch (la->t) {
    case FKL_TERM_STRING: {
        const FklString *s = action->match.str;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_TERM_STRING
                    || (curAction->match.t == FKL_TERM_STRING
                            && s->size > curAction->match.str->size))
                break;
        }
    } break;
    case FKL_TERM_KEYWORD: {
        const FklString *s = action->match.str;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_TERM_KEYWORD
                    || (curAction->match.t == FKL_TERM_KEYWORD
                            && s->size > curAction->match.str->size))
                break;
        }
    } break;
    case FKL_TERM_REGEX: {
        const FklRegexCode *re = action->match.re;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_TERM_REGEX
                    || (curAction->match.t == FKL_TERM_REGEX
                            && re->totalsize > curAction->match.re->totalsize))
                break;
        }
    } break;
    case FKL_TERM_BUILTIN:
        break;
    case FKL_TERM_EOF: {
        FklAnalysisStateAction **pa = &curState->state.action;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t == FKL_TERM_IGNORE)
                break;
        }
    } break;
    case FKL_TERM_NONE:
    case FKL_TERM_IGNORE:
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
    action->next = *pa;
    *pa = action;

    return 0;
}

static const FklGrammerSymType grammerSymPriority[] = {
    FKL_TERM_KEYWORD, //
    FKL_TERM_STRING,  //
    FKL_TERM_REGEX,   //
    FKL_TERM_BUILTIN, //
    FKL_TERM_IGNORE,  //
};

static inline void add_shift_action(FklGrammerSymType cur_type,
        FklAnalysisState *curState,
        const FklGrammerSym *sym,
        int allow_ignore,
        const FklStringTable *tt,
        FklAnalysisState *dstState) {
    FklAnalysisStateAction *action =
            create_shift_action(sym, allow_ignore, tt, dstState);
    FklAnalysisStateAction **pa = &curState->state.action;

    for (; *pa; pa = &(*pa)->next) {
        FklAnalysisStateAction *curAction = *pa;
        if (curAction->match.t == cur_type)
            break;
    }

    switch (sym->type) {
    case FKL_TERM_STRING: {
        const FklString *s = action->match.str;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_TERM_STRING
                    || allow_ignore < curAction->match.allow_ignore
                    || (curAction->match.t == FKL_TERM_STRING
                            && s->size > curAction->match.str->size))
                break;
        }
    } break;
    case FKL_TERM_KEYWORD: {
        const FklString *s = action->match.str;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_TERM_KEYWORD
                    || allow_ignore < curAction->match.allow_ignore
                    || (curAction->match.t == FKL_TERM_KEYWORD
                            && s->size > curAction->match.str->size))
                break;
        }
    } break;
    case FKL_TERM_REGEX: {
        const FklRegexCode *re = action->match.re;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_TERM_REGEX
                    || allow_ignore < curAction->match.allow_ignore
                    || (curAction->match.t == FKL_TERM_REGEX
                            && re->totalsize > curAction->match.re->totalsize))
                break;
        }
    } break;
    case FKL_TERM_BUILTIN: {
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_TERM_BUILTIN
                    || allow_ignore < curAction->match.allow_ignore)
                break;
        }
    } break;
    case FKL_TERM_EOF:
    case FKL_TERM_NONE:
    case FKL_TERM_IGNORE:
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }

    action->next = *pa;
    *pa = action;
}

#define PRINT_C_REGEX_PREFIX "R_"

static inline void ignore_print_c_match_cond(uint64_t number,
        const FklGrammerIgnore *ig,
        const FklGrammer *g,
        FklCodeBuilder *build) {
    CB_FMT("match_ignore_%" PRIu64
           "(start,*in+otherMatchLen,*restLen-otherMatchLen,&matchLen,&is_waiting_for_more,ctx)",
            number);
    return;
}

static inline FklAnalysisStateAction *create_ignore_action(FklGrammer *g) {
    FklAnalysisStateAction *action = (FklAnalysisStateAction *)fklZcalloc(1,
            sizeof(FklAnalysisStateAction));
    FKL_ASSERT(action);
    action->next = NULL;
    action->action = FKL_ANALYSIS_IGNORE;
    action->state = NULL;
    action->match.t = FKL_TERM_IGNORE;

    return action;
}

static inline void add_ignore_action(FklGrammer *g,
        FklAnalysisState *curState) {
    FklAnalysisStateAction *action = create_ignore_action(g);
    FklAnalysisStateAction **pa = &curState->state.action;
    for (; *pa; pa = &(*pa)->next)
        if ((*pa)->match.t == FKL_TERM_IGNORE)
            break;
    action->next = *pa;
    *pa = action;
}

// GraProdHashSet
#define FKL_HASH_TYPE_PREFIX Gra
#define FKL_HASH_METHOD_PREFIX gra
#define FKL_HASH_KEY_TYPE FklGrammerProduction *
#define FKL_HASH_ELM_NAME Prod
#define FKL_HASH_KEY_HASH                                                      \
    return fklHash64Shift(                                                     \
            FKL_TYPE_CAST(uintptr_t, (*pk)) / alignof(FklGrammerProduction));
#include <fakeLisp/cont/hash.h>

static inline int is_only_single_way_to_reduce(
        const FklLalrItemSetHashMapElm *set) {
    for (FklLalrItemSetLink *l = set->v.links; l; l = l->next)
        if (l->sym.type != FKL_TERM_NONTERM)
            return 0;
    GraProdHashSet prodSet;
    int hasEof = 0;
    graProdHashSetInit(&prodSet);
    for (const FklLalrItemHashSetNode *l = set->k.first; l; l = l->next) {
        graProdHashSetPut2(&prodSet, l->k.prod);
        if (l->k.la.t == FKL_TERM_EOF)
            hasEof = 1;
    }
    size_t num = prodSet.count;
    graProdHashSetUninit(&prodSet);
    if (num != 1)
        return 0;
    return num == 1 && hasEof;
}

int fklGenerateLalrAnalyzeTable(FklVMgc *gc,
        FklGrammer *grammer,
        FklLalrItemSetHashMap *states,
        FklStrBuf *error_msg) {
    FklCodeBuilder err = { 0 };
    fklInitCodeBuilderStrBuf(&err, error_msg, NULL);

    int hasConflict = 0;
    grammer->aTable.num = states->count;
    FklStringTable *tt = &grammer->terminals;
    FklAnalysisState *astates;
    if (!states->count)
        astates = NULL;
    else {
        astates = (FklAnalysisState *)fklZmalloc(
                states->count * sizeof(FklAnalysisState));
        FKL_ASSERT(astates);
    }
    grammer->aTable.states = astates;
    GraItemStateIdxHashMap idxTable;
    graItemStateIdxHashMapInit(&idxTable);
    size_t idx = 0;
    for (const FklLalrItemSetHashMapNode *l = states->first; l;
            l = l->next, idx++) {
        graItemStateIdxHashMapPut2(&idxTable, &l->elm, idx);
    }
    idx = 0;
    if (astates == NULL)
        goto break_loop;
    for (const FklLalrItemSetHashMapNode *l = states->first; l;
            l = l->next, idx++) {
        FklAnalysisState *curState = &astates[idx];
        curState->func = NULL;
        curState->state.action = NULL;
        curState->state.gt = NULL;
        const FklLalrItemHashSet *items = &l->k;
        int ignore_added = 0;
        int is_single_way = is_only_single_way_to_reduce(&l->elm);
        for (const FklGrammerSymType *priority = &grammerSymPriority[0];
                priority < &grammerSymPriority[sizeof(grammerSymPriority)
                                               / sizeof(*priority)];
                ++priority) {
            for (FklLalrItemSetLink *ll = l->v.links; ll; ll = ll->next) {
                const FklGrammerSym *sym = &ll->sym;
                size_t dstIdx =
                        *graItemStateIdxHashMapGet2NonNull(&idxTable, ll->dst);
                FklAnalysisState *dstState = &astates[dstIdx];
                if (sym->type != FKL_TERM_NONTERM) {
                    if (sym->type == *priority)
                        add_shift_action(*priority,
                                curState,
                                sym,
                                ll->allow_ignore,
                                tt,
                                dstState);
                } else if (*priority == FKL_TERM_KEYWORD && ll->allow_ignore) {
                    curState->state.gt = create_state_goto(sym,
                            ll->allow_ignore,
                            tt,
                            curState->state.gt,
                            dstState);
                } else if (*priority == FKL_TERM_STRING && !ll->allow_ignore) {
                    curState->state.gt = create_state_goto(sym,
                            ll->allow_ignore,
                            tt,
                            curState->state.gt,
                            dstState);
                }
            }
            if (is_single_way)
                continue;
            for (const FklLalrItemHashSetNode *il = items->first; il;
                    il = il->next) {
                FklGrammerSym *sym = get_item_next(&il->k);
                if (!sym && il->k.la.t == *priority) {
                    if (il->k.la.t == FKL_TERM_IGNORE) {
                        ignore_added = 1;
                        add_ignore_action(grammer, curState);
                    } else {
                        hasConflict = add_reduce_action(*priority,
                                curState,
                                il->k.prod,
                                &il->k.la);
                    }
                    if (hasConflict) {
                        clear_analysis_table(grammer, idx);
                        fklCodeBuilderFmt(&err,
                                "conflict at state %lu with [[  ",
                                idx);
                        print_lalr_item(gc,
                                &il->k,
                                &grammer->terminals,
                                &grammer->regexes,
                                &err);
                        fklCodeBuilderFmt(&err, " ## ");
                        print_lookahead(&il->k.la, &grammer->regexes, &err);
                        fklCodeBuilderFmt(&err, "  ]]");
                        goto break_loop;
                    }
                }
            }
        }

        if (is_single_way) {
            FklLalrItem const *item = &items->first->k;
            FklLalrItemLookAhead eofla = FKL_LALR_MATCH_EOF_INIT;
            int r = add_reduce_action(FKL_TERM_EOF,
                    curState,
                    item->prod,
                    &eofla);
            FKL_ASSERT(r == 0);
            (void)r;
        }
        if (idx == 0 && !ignore_added)
            add_ignore_action(grammer, curState);
    }

break_loop:
    graItemStateIdxHashMapUninit(&idxTable);
    return hasConflict;
}

static inline void print_lookahead_of_analysis_table(const FklRegexTable *rt,
        const FklAnalysisStateActionMatch *match,
        FklCodeBuilder *fp) {
    switch (match->t) {
    case FKL_TERM_STRING: {
        fklCodeBuilderPutc(fp, '\'');
        const FklString *str = match->str;
        print_string_as_dot(str->str, '\'', str->size, fp);
        fklCodeBuilderPutc(fp, '\'');
    } break;
    case FKL_TERM_KEYWORD: {
        fklCodeBuilderPutc(fp, '\'');
        const FklString *str = match->str;
        print_string_as_dot(str->str, '\'', str->size, fp);
        fklCodeBuilderPuts(fp, "\'$");
    } break;
    case FKL_TERM_EOF:
        fklCodeBuilderPuts(fp, "$$");
        break;
    case FKL_TERM_BUILTIN:
        fklCodeBuilderPuts(fp, match->func.t->name);
        break;
    case FKL_TERM_NONE:
        fklCodeBuilderPuts(fp, "()");
        break;
    case FKL_TERM_REGEX:
        print_as_regex(fklGetStringWithRegex(rt, match->re, NULL), fp);
        break;
    case FKL_TERM_IGNORE:
        fklCodeBuilderPuts(fp, "?e");
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

void fklPrintAnalysisTable(const FklGrammer *grammer, FILE *fp) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrintAnalysisTable2(grammer, &builder);
}

void fklPrintAnalysisTable2(const FklGrammer *grammer, FklCodeBuilder *fp) {
    size_t num = grammer->aTable.num;
    FklAnalysisState *states = grammer->aTable.states;

    for (size_t i = 0; i < num; i++) {
        fklCodeBuilderFmt(fp, "%" PRIu64 ": ", i);
        FklAnalysisState *curState = &states[i];
        for (FklAnalysisStateAction *actions = curState->state.action; actions;
                actions = actions->next) {
            switch (actions->action) {
            case FKL_ANALYSIS_SHIFT:
                fklCodeBuilderPuts(fp, "S(");
                print_lookahead_of_analysis_table(&grammer->regexes,
                        &actions->match,
                        fp);
                {
                    uintptr_t idx = actions->state - states;
                    fklCodeBuilderFmt(fp, " , %" PRIu64 " )", idx);
                }
                break;
            case FKL_ANALYSIS_REDUCE:
                fklCodeBuilderPuts(fp, "R(");
                print_lookahead_of_analysis_table(&grammer->regexes,
                        &actions->match,
                        fp);
                fklCodeBuilderFmt(fp, " , %" PRIu64 " )", actions->prod->idx);
                break;
            case FKL_ANALYSIS_ACCEPT:
                fklCodeBuilderPuts(fp, "acc(");
                print_lookahead_of_analysis_table(&grammer->regexes,
                        &actions->match,
                        fp);
                fklCodeBuilderPutc(fp, ')');
                break;
            case FKL_ANALYSIS_IGNORE:
                break;
            }
            fklCodeBuilderPutc(fp, '\t');
        }
        fklCodeBuilderPuts(fp, "|\t");
        for (FklAnalysisStateGoto *gt = curState->state.gt; gt; gt = gt->next) {
            uintptr_t idx = gt->state - states;
            fklCodeBuilderPutc(fp, '(');
            fklPrintSymbolLiteral2(FKL_VM_SYM(gt->nt.sid), fp);
            fklCodeBuilderFmt(fp, " , %" PRIu64 ")", idx);
            fklCodeBuilderPutc(fp, '\t');
        }
        fklCodeBuilderPutc(fp, '\n');
    }
}

static uintptr_t action_match_hash_func(
        const FklAnalysisStateActionMatch *match) {
    switch (match->t) {
    case FKL_TERM_NONE:
        return 0;
        break;
    case FKL_TERM_EOF:
        return 1;
        break;
    case FKL_TERM_IGNORE:
        return 2;
        break;
    case FKL_TERM_STRING:
    case FKL_TERM_KEYWORD:
        return (uintptr_t)match->str;
        break;
    case FKL_TERM_REGEX:
        return (uintptr_t)match->re;
        break;
    case FKL_TERM_BUILTIN:
        return fklBuiltinGrammerSymHash(&match->func);
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
    return 0;
}

static inline int action_match_equal(const FklAnalysisStateActionMatch *m0,
        const FklAnalysisStateActionMatch *m1) {
    if (m0->t == m1->t) {
        switch (m0->t) {
        case FKL_TERM_NONE:
        case FKL_TERM_EOF:
        case FKL_TERM_IGNORE:
            return 1;
            break;
        case FKL_TERM_STRING:
        case FKL_TERM_KEYWORD:
            return m0->str == m1->str;
            break;
        case FKL_TERM_REGEX:
            return m0->re == m1->re;
            break;
        case FKL_TERM_BUILTIN:
            return fklBuiltinGrammerSymEqual(&m0->func, &m1->func);
            break;
        case FKL_TERM_NONTERM:
            FKL_UNREACHABLE();
            break;
        }
    }
    return 0;
}

// GraActionMatchHashSet
#define FKL_HASH_TYPE_PREFIX Gra
#define FKL_HASH_METHOD_PREFIX gra
#define FKL_HASH_KEY_TYPE FklAnalysisStateActionMatch
#define FKL_HASH_KEY_EQUAL(A, B) action_match_equal(A, B)
#define FKL_HASH_KEY_HASH return action_match_hash_func(pk)
#define FKL_HASH_ELM_NAME ActionMatch
#include <fakeLisp/cont/hash.h>

static inline void init_analysis_table_header(GraActionMatchHashSet *la,
        FklNontermHashSet *nt,
        FklAnalysisState *states,
        size_t stateNum) {
    graActionMatchHashSetInit(la);
    fklNontermHashSetInit(nt);

    for (size_t i = 0; i < stateNum; i++) {
        FklAnalysisState *curState = &states[i];
        for (FklAnalysisStateAction *action = curState->state.action; action;
                action = action->next)
            if (action->action != FKL_ANALYSIS_IGNORE)
                graActionMatchHashSetPut(la, &action->match);
        for (FklAnalysisStateGoto *gt = curState->state.gt; gt; gt = gt->next)
            fklNontermHashSetPut(nt, &gt->nt);
    }
}

static inline void print_symbol_for_grapheasy(const FklString *stri,
        FklCodeBuilder *fp) {
    size_t size = stri->size;
    const uint8_t *str = (uint8_t *)stri->str;
    size_t i = 0;
    while (i < size) {
        unsigned int l = fklGetByteNumOfUtf8(&str[i], size - i);
        if (l == 7) {
            uint8_t j = str[i];
            fklCodeBuilderFmt(fp, "\\x%02X", j);
            i++;
        } else if (l == 1) {
            if (str[i] == '\\')
                fklCodeBuilderPuts(fp, "\\\\");
            else if (str[i] == '|')
                fklCodeBuilderPuts(fp, "\\|");
            else if (str[i] == ']')
                fklCodeBuilderPuts(fp, "\\]");
            else if (isgraph(str[i]))
                fklCodeBuilderPutc(fp, str[i]);
            else if (fklCodeBuilderPutEscSeq(fp, str[i]))
                ;
            else {
                uint8_t j = str[i];
                fklCodeBuilderFmt(fp, "\\x%02X", j);
            }
            i++;
        } else {
            for (unsigned int j = 0; j < l; j++)
                fklCodeBuilderPutc(fp, str[i + j]);
            i += l;
        }
    }
}

static inline void print_string_for_grapheasy(const FklString *stri,
        FklCodeBuilder *fp) {
    size_t size = stri->size;
    const uint8_t *str = (uint8_t *)stri->str;
    size_t i = 0;
    while (i < size) {
        unsigned int l = fklGetByteNumOfUtf8(&str[i], size - i);
        if (l == 7) {
            uint8_t j = str[i];
            fklCodeBuilderFmt(fp, "\\x%02X", j);
            i++;
        } else if (l == 1) {
            if (str[i] == '\\')
                fklCodeBuilderPuts(fp, "\\\\");
            else if (str[i] == '\'')
                fklCodeBuilderPuts(fp, "\\\\'");
            else if (str[i] == '#')
                fklCodeBuilderPuts(fp, "\\#");
            else if (str[i] == '|')
                fklCodeBuilderPuts(fp, "\\|");
            else if (str[i] == ']')
                fklCodeBuilderPuts(fp, "\\]");
            else if (isgraph(str[i]))
                fklCodeBuilderPutc(fp, str[i]);
            else if (fklCodeBuilderPutEscSeq(fp, str[i]))
                ;
            else {
                uint8_t j = str[i];
                fklCodeBuilderFmt(fp, "\\x%02X", j);
            }
            i++;
        } else {
            for (unsigned int j = 0; j < l; j++)
                fklCodeBuilderPutc(fp, str[i + j]);
            i += l;
        }
    }
}

static inline void print_lookahead_for_grapheasy(
        const FklAnalysisStateActionMatch *la,
        FklCodeBuilder *fp) {
    switch (la->t) {
    case FKL_TERM_STRING: {
        fklCodeBuilderPutc(fp, '\'');
        print_string_for_grapheasy(la->str, fp);
        fklCodeBuilderPutc(fp, '\'');
    } break;
    case FKL_TERM_KEYWORD: {
        fklCodeBuilderPutc(fp, '\'');
        print_string_for_grapheasy(la->str, fp);
        fklCodeBuilderPuts(fp, "\'$");
    } break;
    case FKL_TERM_EOF:
        fklCodeBuilderPutc(fp, '$');
        break;
    case FKL_TERM_IGNORE:
        fklCodeBuilderPuts(fp, "?e");
        break;
    case FKL_TERM_BUILTIN:
        fklCodeBuilderFmt(fp, "\\|%s\\|", la->func.t->name);
        break;
    case FKL_TERM_NONE:
        fklCodeBuilderPuts(fp, "()");
        break;
    case FKL_TERM_REGEX:
        fklCodeBuilderFmt(fp, "/%p/", la->re);
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void print_table_header_for_grapheasy(
        const GraActionMatchHashSet *la,
        const FklNontermHashSet *sid,
        FklCodeBuilder *fp) {
    fklCodeBuilderPuts(fp, "\\n|");
    for (GraActionMatchHashSetNode *al = la->first; al; al = al->next) {
        print_lookahead_for_grapheasy(&al->k, fp);
        fklCodeBuilderPutc(fp, '|');
    }
    fklCodeBuilderPuts(fp, "\\n|\\n");
    for (FklNontermHashSetNode *sl = sid->first; sl; sl = sl->next) {
        fklCodeBuilderPutc(fp, '|');
        fklCodeBuilderPuts(fp, "\\|");
        print_symbol_for_grapheasy(FKL_VM_SYM(sl->k.sid), fp);
        fklCodeBuilderPuts(fp, "\\|");
    }
    fklCodeBuilderPuts(fp, "||\n");
}

static inline FklAnalysisStateAction *find_action(
        FklAnalysisStateAction *action,
        const FklAnalysisStateActionMatch *match) {
    for (; action; action = action->next) {
        if (action_match_equal(match, &action->match))
            return action;
    }
    return NULL;
}

static inline FklAnalysisStateGoto *find_gt(FklAnalysisStateGoto *gt,
        struct FklVMvalue *group,
        struct FklVMvalue *id) {
    for (; gt; gt = gt->next) {
        if (gt->nt.sid == id && gt->nt.group == group)
            return gt;
    }
    return NULL;
}

void fklPrintAnalysisTableForGraphEasy(const FklGrammer *grammer, FILE *fp) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrintAnalysisTableForGraphEasy2(grammer, &builder);
}

void fklPrintAnalysisTableForGraphEasy2(const FklGrammer *g,
        FklCodeBuilder *fp) {
    size_t num = g->aTable.num;
    FklAnalysisState *states = g->aTable.states;

    fklCodeBuilderPuts(fp, "graph{title:state-table;}[\n");

    GraActionMatchHashSet laTable;
    FklNontermHashSet sidSet;
    init_analysis_table_header(&laTable, &sidSet, states, num);

    print_table_header_for_grapheasy(&laTable, &sidSet, fp);

    GraActionMatchHashSetNode *laList = laTable.first;
    FklNontermHashSetNode *sidList = sidSet.first;
    for (size_t i = 0; i < num; i++) {
        const FklAnalysisState *curState = &states[i];
        fklCodeBuilderFmt(fp, "%" PRIu64 ": |", i);
        for (GraActionMatchHashSetNode *al = laList; al; al = al->next) {
            FklAnalysisStateAction *action =
                    find_action(curState->state.action, &al->k);
            if (action) {
                switch (action->action) {
                case FKL_ANALYSIS_SHIFT: {
                    uintptr_t idx = action->state - states;
                    fklCodeBuilderFmt(fp, "s%" PRIu64 "", idx);
                } break;
                case FKL_ANALYSIS_REDUCE:
                    fklCodeBuilderFmt(fp, "r%" PRIu64 "", action->prod->idx);
                    break;
                case FKL_ANALYSIS_ACCEPT:
                    fklCodeBuilderPuts(fp, "acc");
                    break;
                case FKL_ANALYSIS_IGNORE:
                    break;
                }
            } else
                fklCodeBuilderPuts(fp, "\\n");
            fklCodeBuilderPutc(fp, '|');
        }
        fklCodeBuilderPuts(fp, "\\n|\\n");
        for (FklNontermHashSetNode *sl = sidList; sl; sl = sl->next) {
            fklCodeBuilderPutc(fp, '|');
            FklAnalysisStateGoto *gt =
                    find_gt(curState->state.gt, sl->k.group, sl->k.sid);
            if (gt) {
                uintptr_t idx = gt->state - states;
                fklCodeBuilderFmt(fp, "%" PRIu64 "", idx);
            } else
                fklCodeBuilderPuts(fp, "\\n");
        }
        fklCodeBuilderPuts(fp, "||\n");
    }
    fklCodeBuilderPutc(fp, ']');
    graActionMatchHashSetUninit(&laTable);
    fklNontermHashSetUninit(&sidSet);
}

static inline void build_get_max_non_term_length_prototype_to_c_file(
        FklCodeBuilder *build) {

    CB_LINE("static inline size_t");
    CB_LINE("get_max_non_term_length(FklGrammerMatchCtx*");

    CB_INDENT(flag) {
        CB_LINE(",const char*");
        CB_LINE(",const char*");
        CB_LINE(",size_t);");
    }
}

static inline void build_match_ignore_prototype_to_c_file(
        FklCodeBuilder *build) {
    CB_LINE("static inline size_t match_ignore(FklGrammerMatchCtx*,const char*,size_t,int* );");
}

static inline void build_match_ignore_to_c_file(const FklGrammer *g,
        FklCodeBuilder *build) {
    CB_LINE("static inline size_t match_ignore(FklGrammerMatchCtx* ctx,const char *start, size_t rest_len, int* p_is_waiting_for_more) {\n");

    CB_INDENT(flag) {
        const FklGrammerIgnore *ig = g->ignores;
        if (ig) {
            CB_LINE("ssize_t matchLen=0;");
            CB_LINE("size_t otherMatchLen=0;");
            CB_LINE("const char** in=&start;");
            CB_LINE("size_t* restLen=&rest_len;");
            CB_LINE("int is_waiting_for_more=0;");
            CB_LINE("(void)is_waiting_for_more;");
            CB_LINE("(void)restLen;");

            CB_LINE("for(;rest_len>otherMatchLen;){");
            CB_INDENT(flag) {
                CB_LINE_START("if(");
                uint64_t number = 0;
                ignore_print_c_match_cond(number, ig, g, build);
                CB_INDENT(flag) {
                    ++number;
                    for (ig = ig->next; ig; ig = ig->next, ++number) {
                        CB_LINE_END("");
                        CB_LINE_START("||");
                        ignore_print_c_match_cond(number, ig, g, build);
                    }
                }
                CB_LINE_END(")");
                CB_LINE("{");
                CB_INDENT(flag) { CB_LINE("otherMatchLen+=matchLen;"); }
                CB_LINE("}");

                CB_LINE("else");
                CB_INDENT(flag) { CB_LINE("break;"); }
            }
            CB_LINE("}");

            CB_LINE("*p_is_waiting_for_more=is_waiting_for_more;");
            CB_LINE("return otherMatchLen;");
        } else {
            CB_LINE("return 0;");
        }
    }

    CB_LINE("}");
}

static inline void build_get_max_non_term_length_to_c_file(const FklGrammer *g,
        FklCodeBuilder *build) {
    CB_LINE("static inline size_t");
    CB_LINE("get_max_non_term_length(FklGrammerMatchCtx* ctx");
    CB_INDENT(flag) {
        CB_LINE(",const char* start");
        CB_LINE(",const char* cur");
        CB_LINE(",size_t rLen) {");
    }
    CB_INDENT(flag) {
        CB_LINE("if(rLen) {");
        CB_INDENT(flag) {
            CB_LINE("if(start==ctx->start&&cur==ctx->cur) return ctx->maxNonterminalLen;");
            CB_LINE("ctx->start=start;");
            CB_LINE("ctx->cur=cur;");
            CB_LINE("size_t len=0;");
            CB_LINE("ssize_t matchLen=0;");
            CB_LINE("size_t otherMatchLen=0;");
            CB_LINE("size_t* restLen=&rLen;");
            CB_LINE("const char** in=&cur;");
            CB_LINE("int is_waiting_for_more=0;");
            CB_LINE("(void)is_waiting_for_more;");
            CB_LINE("(void)otherMatchLen;");
            CB_LINE("(void)restLen;");
            CB_LINE("(void)in;");
            CB_LINE("while(rLen) {");
            CB_INDENT(flag) {
                CB_LINE_START("if(");
                if (g->ignores) {
                    const FklGrammerIgnore *igns = g->ignores;
                    uint64_t number = 0;
                    ignore_print_c_match_cond(number, igns, g, build);
                    igns = igns->next;
                    for (++number; igns; igns = igns->next, ++number) {
                        CB_LINE_END("");
                        CB_LINE_START("||");
                        ignore_print_c_match_cond(number, igns, g, build);
                    }
                }
                if (g->ignores && g->sorted_delimiters_num) {
                    CB_LINE_END("");
                    CB_LINE_START("||");
                }
                if (g->sorted_delimiters_num) {
                    size_t num = g->sorted_delimiters_num;
                    const FklString **terminals = g->sorted_delimiters;
                    const FklString *cur = terminals[0];
                    CB_LINE("(matchLen=fklCharBufMatch(\"");
                    build_string_in_hex(cur, build);
                    CB_LINE("\",%" PRIu64
                            ",*in+otherMatchLen,*restLen-otherMatchLen))>=0",
                            cur->size);
                    for (size_t i = 1; i < num; i++) {
                        CB_LINE_END("");
                        CB_LINE_START("||");
                        const FklString *cur = terminals[i];
                        CB_LINE("(matchLen=fklCharBufMatch(\"");
                        build_string_in_hex(cur, build);
                        CB_LINE("\",%" PRIu64
                                ",*in+otherMatchLen,*restLen-otherMatchLen))>=0",
                                cur->size);
                    }
                }
                CB_LINE_END(") break;");
                CB_LINE("len++;");
                CB_LINE("rLen--;");
                CB_LINE("cur++;");
            }
            CB_LINE("}");
            CB_LINE("ctx->maxNonterminalLen=len;");
            CB_LINE("return len;");
        }
        CB_LINE("}");
        CB_LINE("return 0;");
    }
    CB_LINE("}");
}

static inline void build_match_char_buf_end_with_terminal_prototype_to_c_file(
        FklCodeBuilder *build) {
    CB_LINE("static inline size_t");
    CB_LINE("match_char_buf_end_with_terminal(const char*,");
    CB_INDENT(flag) {
        CB_LINE("size_t,");
        CB_LINE("const char*,");
        CB_LINE("size_t,");
        CB_LINE("FklGrammerMatchCtx* ctx,");
        CB_LINE("const char* start);");
    }
}

static inline void build_match_char_buf_end_with_terminal_to_c_file(
        FklCodeBuilder *build) {
    CB_LINE("static inline size_t");
    CB_LINE("match_char_buf_end_with_terminal(const char* pattern");
    CB_INDENT(flag) {
        CB_LINE(",size_t pattern_size");
        CB_LINE(",const char* cstr");
        CB_LINE(",size_t restLen");
        CB_LINE(",FklGrammerMatchCtx* ctx");
        CB_LINE(",const char* start)");
    }

    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("size_t maxNonterminalLen=get_max_non_term_length(ctx,start,cstr,restLen);");
        CB_LINE("ssize_t matchLen=fklCharBufMatch(pattern,pattern_size,cstr,restLen);");
        CB_LINE("return matchLen>=0 && maxNonterminalLen==(size_t)matchLen;");
    }
    CB_LINE("}");
}

static inline void build_state_action_match_to_c_file(
        const FklAnalysisStateAction *ac,
        const FklGrammer *g,
        FklCodeBuilder *build) {
    switch (ac->match.t) {
    case FKL_TERM_KEYWORD:
        CB_FMT("(matchLen=match_char_buf_end_with_terminal(\"");
        build_string_in_hex(ac->match.str, build);
        CB_FMT("\",%" PRIu64
               ",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len,ctx,start))",
                ac->match.str->size);
        break;
    case FKL_TERM_STRING:
        CB_FMT("(matchLen=fklCharBufMatch(\"");
        build_string_in_hex(ac->match.str, build);
        CB_FMT("\",%" PRIu64
               ",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len))>=0",
                ac->match.str->size);
        break;
    case FKL_TERM_REGEX: {
        uint64_t num = 0;
        fklGetStringWithRegex(&g->regexes, ac->match.re, &num);
        CB_FMT("regex_lex_match_for_parser_in_c((const FklRegexCode*)&");
        CB_FMT(PRINT_C_REGEX_PREFIX "%" PRIX64, num);
        CB_FMT(",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len,&matchLen,&is_waiting_for_more)");
    } break;
    case FKL_TERM_BUILTIN: {
        FklBuiltinTerminalMatchArgs args = {
            .g = g,
            .len = ac->match.func.len,
            .args = ac->match.func.args,
        };
        ac->match.func.t->build_c_match_cond(&args, build);
    } break;
    case FKL_TERM_EOF:
        CB_FMT("(matchLen=1)");
        break;
    case FKL_TERM_IGNORE:
        CB_FMT("(matchLen=match_ignore(ctx,*in+otherMatchLen,*restLen-otherMatchLen,&is_waiting_for_more))");
        break;
    case FKL_TERM_NONE:
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void build_state_action_to_c_file(FklValueTable *t,
        const FklAnalysisStateAction *ac,
        const FklAnalysisState *states,
        const char *ast_destroyer_name,
        FklCodeBuilder *build) {
    CB_LINE("{");
    CB_INDENT(flag) {
        switch (ac->action) {
        case FKL_ANALYSIS_SHIFT:
            CB_LINE("fklParseStateVectorPushBack2(stateStack,(FklParseState){.func=state_%" PRIu64
                    "});",
                    ac->state - states);
            CB_LINE("init_term_analyzing_symbol(fklAnalysisSymbolVectorPushBack(symbolStack,NULL)");
            CB_INDENT(flag) {
                CB_LINE(",*in+skip_ignore_len");
                CB_LINE(",matchLen");
                CB_LINE(",ctx->line");
                CB_LINE(",skip_ignore_len>0");
                CB_LINE(",ctx->ctx);");
            }
            CB_LINE("ctx->line+=fklCountCharInBuf(*in,matchLen+skip_ignore_len,'\\n');");
            CB_LINE("*in+=matchLen+skip_ignore_len;");
            CB_LINE("*restLen-=matchLen+skip_ignore_len;");
            break;
        case FKL_ANALYSIS_ACCEPT:
            CB_LINE("*accept=1;");
            break;
        case FKL_ANALYSIS_REDUCE: {

            size_t actual_len =
                    fklComputeProdActualLen(ac->prod->len, ac->prod->syms);

            if (actual_len) {
                CB_LINE("size_t line=fklGetFirstNthLine(symbolStack,%" PRIu64
                        ",ctx->line);",
                        actual_len);
            } else {
                CB_LINE("size_t line=ctx->line;");
            }

            CB_LINE("stateStack->size-=%" PRIu64 ";", actual_len);
            CB_LINE("symbolStack->size-=%" PRIu64 ";", actual_len);
            CB_LINE("FklAnalysisSymbol* base=&symbolStack->base[symbolStack->size];");

            CB_LINE("FklStateFuncPtr func=fklParseStateVectorBackNonNull(stateStack)->func;");
            CB_LINE("FklParseState nextState={.func=NULL};");
            CB_LINE("func(NULL,NULL,0,%s,FKL_MAKE_VM_FIX(%" PRIu64
                    "),&nextState,NULL,NULL,NULL,NULL,NULL,NULL);",
                    actual_len ? "base[0].start_with_ignore" : "0",
                    fklValueTableAdd(t, ac->prod->left.sid));
            CB_LINE("if(nextState.func == NULL) return FKL_PARSE_REDUCE_FAILED;");
            CB_LINE("fklParseStateVectorPushBack(stateStack,&nextState);");

            CB_LINE("void* ast=prod_action_%s(ctx->ctx,base,%" PRIu64
                    ",line);\n",
                    ac->prod->name,
                    actual_len);

            if (actual_len) {
                CB_LINE("for(size_t i=0;i<%" PRIu64 ";i++) %s(base[i].ast);",
                        actual_len,
                        ast_destroyer_name);
            }
            CB_LINE("if(!ast) {");
            CB_INDENT(flag) {
                CB_LINE("*output_line=line;");
                CB_LINE("return FKL_PARSE_REDUCE_FAILED;");
            }
            CB_LINE("}");

            CB_LINE("fklInitNontermAnalysisSymbol(fklAnalysisSymbolVectorPushBack(symbolStack,NULL),0,FKL_MAKE_VM_FIX(%" PRIu64
                    "),ast,%s,line);",
                    fklValueTableAdd(t, ac->prod->left.sid),
                    actual_len ? "base[0].start_with_ignore" : "0");
        } break;
        case FKL_ANALYSIS_IGNORE:
            CB_LINE("ctx->line+=fklCountCharInBuf(*in,matchLen,'\\n');");
            CB_LINE("*in+=matchLen;");
            CB_LINE("*restLen-=matchLen;");
            CB_LINE("goto action_match_start;");
            break;
        }
        CB_LINE("return 0;");
    }
    CB_LINE("}");
}

static inline void build_state_prototype_to_c_file(
        const FklAnalysisState *states,
        size_t idx,
        FklCodeBuilder *build) {
    CB_LINE_START("static int state_%" PRIu64 "(FklParseStateVector*", idx);
    CB_LINE(",FklAnalysisSymbolVector*");
    CB_LINE(",int");
    CB_LINE(",uint8_t");
    CB_LINE(",struct FklVMvalue*");
    CB_LINE(",FklParseState* pfunc");
    CB_LINE(",const char*");
    CB_LINE(",const char**");
    CB_LINE(",size_t*");
    CB_LINE(",FklGrammerMatchCtx*");
    CB_LINE(",int*");
    CB_LINE_END(",size_t*);");
}

static inline void build_state_to_c_file(FklValueTable *t,
        const FklAnalysisState *states,
        size_t idx,
        const FklGrammer *g,
        const char *ast_destroyer_name,
        FklCodeBuilder *build) {
    const FklAnalysisState *state = &states[idx];
    CB_LINE("static int state_%" PRIu64 "(FklParseStateVector* stateStack",
            idx);
    CB_INDENT(flag) {
        CB_LINE(",FklAnalysisSymbolVector* symbolStack");
        CB_LINE(",int is_action");
        CB_LINE(",uint8_t start_with_ignore");
        CB_LINE(",struct FklVMvalue* left");
        CB_LINE(",FklParseState* pfunc");
        CB_LINE(",const char* start");
        CB_LINE(",const char** in");
        CB_LINE(",size_t* restLen");
        CB_LINE(",FklGrammerMatchCtx* ctx");
        CB_LINE(",int* accept");
        CB_LINE(",size_t* output_line) {");
    }

    CB_INDENT(flag) {
        CB_LINE("if(is_action){");
        CB_INDENT(flag) {
            CB_LINE("int is_waiting_for_more=0;");
            CB_LINE("(void)is_waiting_for_more;");
            for (const FklAnalysisStateAction *ac = state->state.action; ac;
                    ac = ac->next)
                if (ac->action == FKL_ANALYSIS_IGNORE) {
                    CB_FMT("action_match_start:;\n");
                    break;
                }
            CB_LINE("int has_tried_match_ignore;");
            CB_LINE("ssize_t matchLen=0;");
            CB_LINE("ssize_t ignore_len=-1;");
            CB_LINE("size_t skip_ignore_len;");
            CB_LINE("size_t otherMatchLen=0;");
            CB_LINE("(void)ignore_len;");
            CB_LINE("(void)has_tried_match_ignore;");
            CB_LINE("(void)skip_ignore_len;");
            CB_LINE("");
            const FklAnalysisStateAction *ac = state->state.action;
            if (ac) {
                uint32_t allow_ignore_label_count = 0;
                for (; ac; ac = ac->next) {
                    CB_LINE("skip_ignore_len=0;");
                    CB_LINE("has_tried_match_ignore=0;");
                    uint32_t cur_allow_ignore_label_num = 0;
                    if (ac->match.allow_ignore
                            && ac->action != FKL_ANALYSIS_IGNORE) {
                        cur_allow_ignore_label_num = allow_ignore_label_count++;
                        CB_FMT("allow_ignore_label%u:\n",
                                cur_allow_ignore_label_num);
                    }
                    CB_LINE_START("if(");
                    build_state_action_match_to_c_file(ac, g, build);
                    CB_LINE_END(")");
                    build_state_action_to_c_file(t,
                            ac,
                            states,
                            ast_destroyer_name,
                            build);
                    if (ac->match.allow_ignore
                            && ac->action != FKL_ANALYSIS_IGNORE) {
                        CB_LINE("else if(!has_tried_match_ignore && ((ignore_len==-1 ");
                        CB_INDENT(flag) {
                            CB_LINE("&& (ignore_len=match_ignore(ctx,*in+otherMatchLen,*restLen-otherMatchLen,&is_waiting_for_more))>0)");
                            CB_LINE_START("|| ignore_len>0)");
                        }
                        CB_LINE_END(") {");
                        CB_INDENT(flag) {
                            CB_LINE("has_tried_match_ignore=1;");
                            CB_LINE("skip_ignore_len=(size_t)ignore_len;");
                            CB_LINE("goto allow_ignore_label%u;",
                                    cur_allow_ignore_label_num);
                        }
                        CB_LINE("}");
                        CB_LINE("");
                    }
                }
                CB_LINE("return (is_waiting_for_more||(*restLen && *restLen==skip_ignore_len))?FKL_PARSE_WAITING_FOR_MORE:FKL_PARSE_TERMINAL_MATCH_FAILED;");
            } else
                CB_LINE("return FKL_PARSE_TERMINAL_MATCH_FAILED;");
            CB_LINE("(void)otherMatchLen;");
        }
        CB_LINE("}else{");
        CB_INDENT(flag) {
            const FklAnalysisStateGoto *gt = state->state.gt;
            if (gt) {
                CB_LINE("if(0){}");
                for (; gt; gt = gt->next) {
                    if (!gt->allow_ignore) {
                        CB_LINE("else if(!start_with_ignore&&left==FKL_MAKE_VM_FIX(%" PRIu64
                                ")/* %s */){",
                                fklValueTableAdd(t, gt->nt.sid),
                                FKL_VM_SYM(gt->nt.sid)->str);
                    } else {
                        CB_LINE("else if(left==FKL_MAKE_VM_FIX(%" PRIu64
                                ")/* %s */){",
                                fklValueTableAdd(t, gt->nt.sid),
                                FKL_VM_SYM(gt->nt.sid)->str);
                    }
                    CB_INDENT(flag) {
                        CB_LINE("pfunc->func=state_%" PRIu64 ";",
                                gt->state - states);
                        CB_LINE("return 0;");
                    }
                    CB_LINE("}");
                }
                CB_LINE("else return FKL_PARSE_REDUCE_FAILED;");
            } else
                CB_LINE("return FKL_PARSE_REDUCE_FAILED;");
        }
        CB_LINE("}");
        CB_LINE("return 0;");
    }
    CB_LINE("}");
    CB_LINE("");
}

// builtin match method unordered set
// GraBtmHashSet
#define FKL_HASH_TYPE_PREFIX Gra
#define FKL_HASH_METHOD_PREFIX gra
#define FKL_HASH_KEY_TYPE FklLalrBuiltinMatch const *
#define FKL_HASH_ELM_NAME Btm
#define FKL_HASH_KEY_HASH                                                      \
    return fklHash64Shift(                                                     \
            FKL_TYPE_CAST(uintptr_t, *pk) / alignof(FklLalrBuiltinMatch));
#include <fakeLisp/cont/hash.h>

static inline void get_all_match_method_table(const FklGrammer *g,
        GraBtmHashSet *ptrSet) {
    for (const FklGrammerIgnore *ig = g->ignores; ig; ig = ig->next) {
        size_t len = ig->len;
        for (size_t i = 0; i < len; i++)
            if (ig->ig[i].term_type == FKL_TERM_BUILTIN)
                graBtmHashSetPut2(ptrSet, ig->ig[i].b.t);
    }

    const FklAnalysisState *states = g->aTable.states;
    size_t num = g->aTable.num;
    for (size_t i = 0; i < num; i++) {
        for (const FklAnalysisStateAction *ac = states[i].state.action; ac;
                ac = ac->next)
            if (ac->match.t == FKL_TERM_BUILTIN)
                graBtmHashSetPut2(ptrSet, ac->match.func.t);
    }
}

static inline void build_all_builtin_match_func(const FklGrammer *g,
        FklCodeBuilder *build) {
    GraBtmHashSet builtin_match_method_table_set;
    graBtmHashSetInit(&builtin_match_method_table_set);
    get_all_match_method_table(g, &builtin_match_method_table_set);
    for (GraBtmHashSetNode *il = builtin_match_method_table_set.first; il;
            il = il->next) {
        const FklLalrBuiltinMatch *t = il->k;
        t->build_src(g, build);
        CB_LINE("");
    }
    graBtmHashSetUninit(&builtin_match_method_table_set);
}

static inline void build_all_regex(const FklRegexTable *rt,
        FklCodeBuilder *build) {
    for (const FklStrRegexHashMapNode *l = rt->str_re.first; l; l = l->next) {
        CB_LINE("static const ");
        fklRegexBuildAsCwithNum(l->v.re, PRINT_C_REGEX_PREFIX, l->v.num, build);
        CB_LINE("");
    }
}

static inline void build_regex_lex_match_for_parser_in_c_to_c_file(
        FklCodeBuilder *build) {
    CB_LINE("static inline int");
    CB_LINE("regex_lex_match_for_parser_in_c(const FklRegexCode* re,");
    CB_INDENT(flag) {
        CB_LINE("const char* cstr,");
        CB_LINE("size_t restLen,");
        CB_LINE("ssize_t* matchLen,");
        CB_LINE("int* is_waiting_for_more)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("int last_is_true=0;");
        CB_LINE("size_t len=fklRegexLexMatchp(re,cstr,restLen,&last_is_true);");
        CB_LINE("if(len>restLen) {");
        CB_INDENT(flag) {
            CB_LINE("*is_waiting_for_more|=last_is_true;");
            CB_LINE("return 0;");
        }
        CB_LINE("}");
        CB_LINE("*matchLen=len;");
        CB_LINE("return 1;");
    }
    CB_LINE("}");
}

static inline void build_init_term_analyzing_symbol_src(FklCodeBuilder *build,
        const char *name) {
    CB_LINE("static inline void");
    CB_LINE("init_term_analyzing_symbol(FklAnalysisSymbol* sym,");
    CB_INDENT(flag) {
        CB_LINE("const char* s,");
        CB_LINE("size_t len,");
        CB_LINE("size_t line,");
        CB_LINE("uint8_t start_with_ignore,");
        CB_LINE("void* ctx)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("void* ast=%s(s,len,line,ctx);", name);
        CB_LINE("sym->nt.group=0;");
        CB_LINE("sym->nt.sid=0;");
        CB_LINE("sym->ast=ast;");
        CB_LINE("sym->start_with_ignore=start_with_ignore;");
        CB_LINE("sym->line=line;");
    }
    CB_LINE("}");
    CB_LINE("");
}

static inline void build_ignore_sym_match_to_c_file(
        const FklGrammerIgnoreSym *sym,
        const FklGrammer *g,
        FklCodeBuilder *build) {
    switch (sym->term_type) {
    case FKL_TERM_STRING:
        CB_FMT("(matchLen=fklCharBufMatch(\"");
        build_string_in_hex(sym->str, build);
        CB_FMT("\",%" PRIu64
               ",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len))>=0",
                sym->str->size);
        break;
    case FKL_TERM_REGEX: {
        uint64_t num = 0;
        fklGetStringWithRegex(&g->regexes, sym->re, &num);
        CB_FMT("regex_lex_match_for_parser_in_c((const FklRegexCode*)&");
        CB_FMT(PRINT_C_REGEX_PREFIX "%" PRIX64, num);
        CB_FMT(",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len,&matchLen,&is_waiting_for_more)");
    } break;
    case FKL_TERM_BUILTIN: {
        FklBuiltinTerminalMatchArgs args = {
            .g = g,
            .len = sym->b.len,
            .args = sym->b.args,
        };
        sym->b.t->build_c_match_cond(&args, build);
    } break;

    case FKL_TERM_KEYWORD:
    case FKL_TERM_EOF:
    case FKL_TERM_NONE:
    case FKL_TERM_IGNORE:
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void build_ignore(uint64_t number,
        const FklGrammerIgnore *ig,
        const FklGrammer *g,
        FklCodeBuilder *build) {
    CB_LINE("static inline int match_ignore_%" PRIu64 "(const char* start",
            number);
    CB_INDENT(flag) {
        CB_LINE(",const char* cstr");
        CB_LINE(",size_t restLen_");
        CB_LINE(",ssize_t* pmatchLen");
        CB_LINE(",int* pis_waiting_for_more");
        CB_LINE(",FklGrammerMatchCtx* ctx)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("int is_waiting_for_more=0;");
        CB_LINE("if(restLen_) {");
        if (ig->len == 0) {
            CB_LINE("*pmatchLen=0;");
            CB_LINE("*pis_waiting_for_more=is_waiting_for_more;");
            CB_LINE("return 1;");
        }

        CB_INDENT(flag) {
            CB_LINE("size_t otherMatchLen=0;");
            CB_LINE("const char** in=&cstr;");
            CB_LINE("ssize_t matchLen=0;");
            CB_LINE("size_t skip_ignore_len=0;");
            CB_LINE("size_t* restLen=&restLen_;");
            for (size_t i = 0; i < ig->len; ++i) {
                CB_LINE_START("if(");
                build_ignore_sym_match_to_c_file(&ig->ig[i], g, build);
                CB_LINE_END(") {");
                CB_INDENT(flag) { CB_LINE("otherMatchLen+=matchLen;"); }
                CB_LINE("} else {");
                CB_INDENT(flag) {
                    CB_LINE("*pis_waiting_for_more=is_waiting_for_more;");
                    CB_LINE("return 0;");
                }
                CB_LINE("}");
                CB_LINE("");
            }

            CB_LINE("*pmatchLen=otherMatchLen;");
            CB_LINE("*pis_waiting_for_more=is_waiting_for_more;");
            CB_LINE("return 1;");
        }
        CB_LINE("}");
        CB_LINE("*pis_waiting_for_more=is_waiting_for_more;");
        CB_LINE("return 0;");
    }
    CB_LINE("}");
}

static inline void build_all_ignores(const FklGrammer *g,
        FklCodeBuilder *build) {
    uint64_t number = 0;
    for (const FklGrammerIgnore *ig = g->ignores; ig; ig = ig->next, ++number) {
        build_ignore(number, ig, g, build);
        CB_LINE("");
    }
}

void fklPrintAnalysisTableAsCfunc(const FklGrammer *g,
        FILE *action_src_fp,
        const char *ast_creator_name,
        const char *ast_destroyer_name,
        const char *state_0_push_func_name,
        FILE *fp) {
    FklCodeBuilder builder;
    fklInitCodeBuilderFp(&builder, fp, NULL);

    FklCodeBuilder *const build = &builder;

    CB_LINE("// Do not edit!");
    CB_LINE("");
    CB_LINE("#include <fakeLisp/grammer.h>");
    CB_LINE("#include <fakeLisp/utils.h>");

#define BUFFER_SIZE (512)
    char buffer[BUFFER_SIZE];
    size_t size = 0;
    while ((size = fread(buffer, 1, BUFFER_SIZE, action_src_fp)))
        fwrite(buffer, size, 1, fp);
#undef BUFFER_SIZE
    CB_LINE("");

    CB_LINE("");
    CB_LINE("");
    build_init_term_analyzing_symbol_src(build, ast_creator_name);

    build_match_ignore_prototype_to_c_file(build);

    CB_LINE("");

    if (g->sorted_delimiters
            || g->sorted_delimiters_num != g->terminals.count) {
        build_get_max_non_term_length_prototype_to_c_file(build);
        CB_LINE("");
    }

    if (g->regexes.num) {
        build_all_regex(&g->regexes, build);
        build_regex_lex_match_for_parser_in_c_to_c_file(build);
        CB_LINE("");
    }

    if (g->sorted_delimiters_num != g->terminals.count) {
        build_match_char_buf_end_with_terminal_prototype_to_c_file(build);
        CB_LINE("");
    }

    build_all_builtin_match_func(g, build);

    build_all_ignores(g, build);

    size_t stateNum = g->aTable.num;
    const FklAnalysisState *states = g->aTable.states;
    if (g->sorted_delimiters
            || g->sorted_delimiters_num != g->terminals.count) {
        build_get_max_non_term_length_to_c_file(g, build);
        CB_LINE("");
    }

    if (g->sorted_delimiters_num != g->terminals.count) {
        build_match_char_buf_end_with_terminal_to_c_file(build);
        CB_LINE("");
    }

    build_match_ignore_to_c_file(g, build);
    CB_LINE("");

    for (size_t i = 0; i < stateNum; i++)
        build_state_prototype_to_c_file(states, i, build);
    CB_LINE("");

    FklValueTable t;
    fklInitValueTable(&t);
    for (size_t i = 0; i < stateNum; i++)
        build_state_to_c_file(&t, states, i, g, ast_destroyer_name, build);
    fklUninitValueTable(&t);

    CB_LINE("void %s(FklParseStateVector* "
            "stateStack){",
            state_0_push_func_name);
    CB_INDENT(flag) {
        CB_LINE("fklParseStateVectorPushBack2(stateStack,(FklParseState){.func=state_0});");
    }
    CB_LINE("}");
}

void fklPrintItemStateSet(FklVMgc *gc,
        const FklLalrItemSetHashMap *i,
        const FklGrammer *g,
        FILE *fp) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrintItemStateSet2(gc, i, g, &builder);
}

void fklPrintItemStateSet2(FklVMgc *gc,
        const FklLalrItemSetHashMap *i,
        const FklGrammer *g,
        FklCodeBuilder *fp) {
    GraItemStateIdxHashMap idxTable;
    graItemStateIdxHashMapInit(&idxTable);
    size_t idx = 0;
    for (const FklLalrItemSetHashMapNode *l = i->first; l; l = l->next, idx++) {
        graItemStateIdxHashMapPut2(&idxTable, &l->elm, idx);
    }
    for (const FklLalrItemSetHashMapNode *l = i->first; l; l = l->next) {
        const FklLalrItemHashSet *i = &l->k;
        idx = *graItemStateIdxHashMapGet2NonNull(&idxTable, &l->elm);
        fklCodeBuilderFmt(fp, "===\nI%" PRIu64 ": \n", idx);
        fklPrintItemSet(gc, i, g, fp);
        fklCodeBuilderPutc(fp, '\n');
        for (FklLalrItemSetLink *ll = l->v.links; ll; ll = ll->next) {
            FklLalrItemSetHashMapElm *dst = ll->dst;
            size_t *c = graItemStateIdxHashMapGet2NonNull(&idxTable, dst);
            fklCodeBuilderFmt(fp, "I%" PRIu64 "--{ ", idx);
            if (ll->allow_ignore)
                fklCodeBuilderPuts(fp, "?e ");
            print_prod_sym(gc, &ll->sym, &g->regexes, fp);
            fklCodeBuilderFmt(fp, " }-->I%" PRIu64 "\n", *c);
        }
        fklCodeBuilderPutc(fp, '\n');
    }
    graItemStateIdxHashMapUninit(&idxTable);
}

const FklLalrBuiltinMatch *fklGetBuiltinMatch(const FklGraSidBuiltinHashMap *ht,
        struct FklVMvalue *id) {
    FklLalrBuiltinMatch const **i = fklGraSidBuiltinHashMapGet2(ht, id);
    if (i)
        return *i;
    return NULL;
}

int fklIsNonterminalExist(const FklProdHashMap *prods,
        FklVMvalue *group_id,
        FklVMvalue *sid) {
    return fklProdHashMapGet2(prods,
                   (FklGrammerNonterm){ .group = group_id, .sid = sid })
        != NULL;
}

FklGrammerProduction *fklGetGrammerProductions(const FklGrammer *g,
        FklVMvalue *group,
        FklVMvalue *sid) {
    return fklGetProductions(&g->productions, group, sid);
}

FklGrammerProduction *fklGetProductions(const FklProdHashMap *prods,
        FklVMvalue *group,
        FklVMvalue *sid) {
    FklGrammerProduction **pp = fklProdHashMapGet2(prods,
            (FklGrammerNonterm){ .group = group, .sid = sid });
    return pp ? *pp : NULL;
}

void fklPrintGrammerIgnores(const FklGrammer *g,
        const FklRegexTable *rt,
        FklCodeBuilder *build) {
    const FklGrammerIgnore *ig = g->ignores;
    for (; ig; ig = ig->next) {
        CB_LINE_START("");
        for (size_t i = 0; i < ig->len; i++) {
            const FklGrammerIgnoreSym *u = &ig->ig[i];
            switch (u->term_type) {
            case FKL_TERM_BUILTIN: {
                CB_FMT("%s", u->b.t->name);
                if (u->b.len) {
                    CB_FMT("[");
                    size_t i = 0;
                    for (; i < u->b.len - 1; ++i) {
                        fklPrintStringLiteral2(u->b.args[i], build);
                        CB_FMT(" , ");
                    }
                    fklPrintSymbolLiteral2(u->b.args[i], build);
                    CB_FMT("]");
                }
            } break;

            case FKL_TERM_REGEX:
                print_as_regex(fklGetStringWithRegex(rt, u->re, NULL), build);
                break;
            case FKL_TERM_STRING: {
                fklPrintStringLiteral2(u->b.args[i], build);
            } break;
            default:
                FKL_UNREACHABLE();
                break;
            }
            CB_FMT(" ");
        }
        CB_LINE_END("");
    }
}

void fklPrintGrammerProduction(FklVMgc *gc,
        const FklGrammerProduction *prod,
        const FklRegexTable *rt,
        FklCodeBuilder *build) {
    if (!is_Sq_nt(&prod->left)) {
        if (prod->left.group) {
            CB_FMT("(");
            fklPrin1VMvalue2(prod->left.group, build, &gc->gcvm);
            CB_FMT(" , ");
            fklPrin1VMvalue2(prod->left.sid, build, &gc->gcvm);
            CB_FMT(")");
        } else {
            fklPrin1VMvalue2(prod->left.sid, build, &gc->gcvm);
        }
    } else
        CB_FMT("S'");
    CB_FMT(" -> ");
    size_t len = prod->len;
    const FklGrammerSym *syms = prod->syms;
    for (size_t i = 0; i < len;) {
        CB_FMT(" ");
        print_prod_sym(gc, &syms[i], rt, build);
        ++i;
        if (i < len && syms[i].type != FKL_TERM_IGNORE)
            CB_FMT(" .. ");
        else
            ++i;
    }
}

void fklPrintGrammer(FklVMgc *gc, const FklGrammer *grammer, FILE *fp) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrintGrammer2(gc, grammer, &builder);
}

void fklPrintGrammer2(FklVMgc *gc,
        const FklGrammer *grammer,
        FklCodeBuilder *fp) {
    const FklRegexTable *rt = &grammer->regexes;
    for (FklProdHashMapNode *list = grammer->productions.first; list;
            list = list->next) {
        FklGrammerProduction *prods = list->v;
        for (; prods; prods = prods->next) {
            fklCodeBuilderFmt(fp, "(%" PRIu64 ") ", prods->idx);
            fklPrintGrammerProduction(gc, prods, rt, fp);
            fklCodeBuilderPutc(fp, '\n');
        }
    }
    fklCodeBuilderPuts(fp, "\nignore:\n");
    fklPrintGrammerIgnores(grammer, &grammer->regexes, fp);
}

static inline int match_char_buf_end_with_terminal(const FklString *laString,
        const char *cstr,
        size_t restLen,
        const FklGrammer *g,
        FklGrammerMatchCtx *ctx,
        const char *start) {
    size_t maxNonterminalLen =
            get_max_non_term_length(g, ctx, start, cstr, restLen);
    return maxNonterminalLen == laString->size
        && fklStringCharBufMatch(laString, cstr, restLen) >= 0;
}

static inline size_t match_ignore(const FklGrammer *g,
        FklGrammerMatchCtx *ctx,
        const char *start,
        const char *cstr,
        size_t restLen,
        int *is_waiting_for_more) {
    size_t ret_len = 0;
    size_t matchLen = 0;
    for (; restLen > ret_len;) {
        int has_matched = 0;
        for (const FklGrammerIgnore *ig = g->ignores; ig; ig = ig->next) {
            if (ignore_match(g,
                        ig,
                        cstr,
                        cstr + ret_len,
                        restLen - ret_len,
                        &matchLen,
                        ctx,
                        is_waiting_for_more)) {
                ret_len += matchLen;
                has_matched = 1;
                break;
            }
        }
        if (!has_matched)
            break;
    }
    return ret_len;
}

int fklIsStateActionMatch(const FklAnalysisStateActionMatch *match,
        const FklGrammer *g,
        FklGrammerMatchCtx *ctx,
        const char *start,
        const char *cstr,
        size_t restLen,
        int *is_waiting_for_more,
        FklStateActionMatchArgs *args) {
    args->skip_ignore_len = 0;
    int has_tried_match_ignore = 0;
match_start:
    switch (match->t) {
    case FKL_TERM_STRING: {
        const FklString *laString = match->str;
        if (fklStringCharBufMatch(laString,
                    cstr + args->skip_ignore_len,
                    restLen - args->skip_ignore_len)
                >= 0) {
            args->matchLen = laString->size;
            return 1;
        }
    } break;
    case FKL_TERM_KEYWORD: {
        const FklString *laString = match->str;
        if (match_char_buf_end_with_terminal(laString,
                    cstr + args->skip_ignore_len,
                    restLen - args->skip_ignore_len,
                    g,
                    ctx,
                    start)) {
            args->matchLen = laString->size;
            return 1;
        }
    } break;

    case FKL_TERM_EOF:
        args->matchLen = 1;
        return 1;
        break;
    case FKL_TERM_BUILTIN: {
        FklBuiltinTerminalMatchArgs match_args = { .g = g,
            .len = match->func.len,
            .args = match->func.args };
        if (match->func.t->match(&match_args,
                    start,
                    cstr + args->skip_ignore_len,
                    restLen - args->skip_ignore_len,
                    &args->matchLen,
                    ctx,
                    is_waiting_for_more)) {
            return 1;
        }
    } break;
    case FKL_TERM_REGEX: {
        int last_is_true = 0;
        uint32_t len = fklRegexLexMatchp(match->re,
                cstr + args->skip_ignore_len,
                restLen - args->skip_ignore_len,
                &last_is_true);
        if (len > restLen - args->skip_ignore_len)
            *is_waiting_for_more |= last_is_true;
        else {
            args->matchLen = len;
            return 1;
        }
    } break;
    case FKL_TERM_IGNORE: {
        size_t match_len =
                match_ignore(g, ctx, start, cstr, restLen, is_waiting_for_more);
        if (match_len > 0) {
            args->matchLen = match_len;
            return 1;
        }
    } break;
    case FKL_TERM_NONE:
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
    if (match->allow_ignore && !has_tried_match_ignore
            && ((args->ignore_len == -2
                        && (args->ignore_len = match_ignore(g,
                                    ctx,
                                    start,
                                    cstr,
                                    restLen,
                                    is_waiting_for_more))
                                   > 0)
                    || args->ignore_len > 0)) {
        has_tried_match_ignore = 1;
        args->skip_ignore_len = (size_t)args->ignore_len;
        goto match_start;
    }

    return 0;
}

uint64_t
fklGetFirstNthLine(FklAnalysisSymbolVector *symbols, size_t num, size_t line) {
    if (num)
        return symbols->base[symbols->size - num].line;
    else
        return line;
}

#include "grammer/action.h"

static const char builtin_grammer_rules[] = {
#include "lisp.g.h"
    '\0',
};

void fklInitBuiltinGrammer(FklGrammer *g, FklVM *vm) {
    FklParserGrammerParseArg args;
    fklInitEmptyGrammer(g, vm);

    fklInitParserGrammerParseArg(&args,
            g,
            vm->gc,
            1,
            builtin_prod_action_resolver,
            NULL);
    int err = fklParseProductionRuleWithCstr(&args, builtin_grammer_rules);
    if (err) {
        fklPrintParserGrammerParseError(err, &args, &vm->gc->err_out);
        fklDestroyGrammer(g);
        FKL_UNREACHABLE();
        return;
    }
    FklGrammerNonterm nonterm = { 0, 0 };
    fklUninitParserGrammerParseArg(&args);
    if (fklCheckAndInitGrammerSymbols(g, &nonterm)) {
        print_unresolved_terminal(&nonterm, &vm->gc->err_out);
        fklDestroyGrammer(g);
        FKL_UNREACHABLE();
        return;
    }
}

FklGrammer *fklCreateBuiltinGrammer(FklVM *vm) {
    FklGrammer *g = create_grammer();
    fklInitBuiltinGrammer(g, vm);
    return g;
}

FklGrammerIgnore *fklInitBuiltinProductionSet(FklGrammer *g, FklVMgc *gc) {
    FklParserGrammerParseArg args;
    fklInitParserGrammerParseArg(&args,
            g,
            gc,
            1,
            builtin_prod_action_resolver,
            NULL);
    int err = fklParseProductionRuleWithCstr(&args, builtin_grammer_rules);
    if (err) {
        fklPrintParserGrammerParseError(err, &args, &gc->err_out);
        fklDestroyGrammer(g);
        FKL_UNREACHABLE();
        return NULL;
    }
    fklUninitParserGrammerParseArg(&args);
    return g->ignores;
}

static inline void replace_group_id(FklGrammerProduction *prod,
        FklVMvalue *new_id) {
    size_t len = prod->len;
    if (prod->left.group)
        prod->left.group = new_id;
    FklGrammerSym *sym = prod->syms;
    for (size_t i = 0; i < len; i++) {
        FklGrammerSym *cur = &sym[i];
        if (cur->type == FKL_TERM_NONTERM && cur->nt.group)
            cur->nt.group = new_id;
    }
}

void fklMergeGrammerIgnore(FklGrammer *to,
        const FklGrammerIgnore *ig,
        const FklGrammer *from) {
    FklGrammerIgnore *new_ig = fklCreateEmptyGrammerIgnore(ig->len);
    for (size_t i = 0; i < ig->len; ++i) {
        const FklGrammerIgnoreSym *from_s = &ig->ig[i];
        FklGrammerIgnoreSym *to_s = &new_ig->ig[i];

        to_s->term_type = from_s->term_type;
        switch (from_s->term_type) {
        case FKL_TERM_STRING:
            to_s->str = fklAddString(&to->terminals, from_s->str);
            fklAddString(&to->delimiters, from_s->str);
            break;
        case FKL_TERM_REGEX: {
            const FklString *regex_str =
                    fklGetStringWithRegex(&from->regexes, from_s->re, NULL);
            to_s->re = fklAddRegexStr(&to->regexes, regex_str);
        } break;
        case FKL_TERM_BUILTIN: {
            to_s->b.t = from_s->b.t;
            to_s->b.len = from_s->b.len;
            FklString const **args =
                    fklZmalloc(to_s->b.len * sizeof(FklString *));
            FKL_ASSERT(args);
            for (size_t i = 0; i < from_s->b.len; ++i) {
                args[i] = fklAddString(&to->terminals, from_s->b.args[i]);
                fklAddString(&to->delimiters, args[i]);
            }
            to_s->b.args = args;
            if (to_s->b.t->ctx_create
                    && to_s->b.t->ctx_create(to_s->b.len, to_s->b.args, to)) {
                FKL_UNREACHABLE();
            }

        } break;

        case FKL_TERM_NONE:
        case FKL_TERM_KEYWORD:
        case FKL_TERM_IGNORE:
        case FKL_TERM_EOF:
        case FKL_TERM_NONTERM:
            FKL_UNREACHABLE();
            break;
        }
    }
    if (fklAddIgnoreToIgnoreList(&to->ignores, new_ig))
        fklDestroyIgnore(new_ig);
}

void fklMergeGrammerProd(FklGrammer *to,
        const FklGrammerProduction *prod,
        const FklGrammer *from,
        const FklRecomputeGroupIdArgs *args) {
    FklGrammerProduction *new_prod = fklCreateEmptyProduction(prod->left.group,
            prod->left.sid,
            prod->len,
            prod->name,
            prod->func,
            prod->ctx_copy(prod->ctx),
            prod->ctx_destroy,
            prod->ctx_copy);

    FklGrammerSym *syms = new_prod->syms;
    for (size_t i = 0; i < prod->len; ++i) {
        const FklGrammerSym *from_s = &prod->syms[i];
        FklGrammerSym *to_s = &syms[i];

        to_s->type = from_s->type;
        switch (from_s->type) {

        case FKL_TERM_NONTERM:
            to_s->nt.group = from_s->nt.group;
            to_s->nt.sid = from_s->nt.sid;
            break;
        case FKL_TERM_REGEX: {
            const FklString *regex_str =
                    fklGetStringWithRegex(&from->regexes, from_s->re, NULL);
            to_s->re = fklAddRegexStr(&to->regexes, regex_str);
        } break;

        case FKL_TERM_STRING: {
            to_s->str = fklAddString(&to->terminals, from_s->str);
            fklAddString(&to->delimiters, from_s->str);
        } break;

        case FKL_TERM_KEYWORD: {
            to_s->str = fklAddString(&to->terminals, from_s->str);
        } break;

        case FKL_TERM_BUILTIN: {
            to_s->b.t = from_s->b.t;
            to_s->b.len = from_s->b.len;
            if (to_s->b.len == 0) {
                to_s->b.args = NULL;
                if (to_s->b.t->ctx_create
                        && to_s->b.t->ctx_create(to_s->b.len,
                                to_s->b.args,
                                to)) {
                    FKL_UNREACHABLE();
                }
                break;
            }
            FklString const **args =
                    fklZmalloc(to_s->b.len * sizeof(FklString *));
            FKL_ASSERT(args);
            for (size_t i = 0; i < from_s->b.len; ++i) {
                args[i] = fklAddString(&to->terminals, from_s->b.args[i]);
                fklAddString(&to->delimiters, args[i]);
            }
            to_s->b.args = args;
            if (to_s->b.t->ctx_create
                    && to_s->b.t->ctx_create(to_s->b.len, to_s->b.args, to)) {
                FKL_UNREACHABLE();
            }
        } break;

        case FKL_TERM_IGNORE:
        case FKL_TERM_EOF:
            break;
        case FKL_TERM_NONE:
            FKL_UNREACHABLE();
            break;
        }
    }

    if (args && args->old_group_id != args->new_group_id)
        replace_group_id(new_prod, args->new_group_id);
    if (fklAddProdToProdTableNoRepeat(to, new_prod))
        fklDestroyGrammerProduction(new_prod);
}

int fklMergeGrammer(FklGrammer *g,
        const FklGrammer *other,
        const FklRecomputeGroupIdArgs *args) {

    for (const FklGrammerIgnore *ig = other->ignores; ig; ig = ig->next) {
        fklMergeGrammerIgnore(g, ig, other);
    }

    for (const FklProdHashMapNode *prods = other->productions.first; prods;
            prods = prods->next) {
        for (const FklGrammerProduction *prod = prods->v; prod;
                prod = prod->next) {
            fklMergeGrammerProd(g, prod, other, args);
        }
    }

    for (const FklStrHashSetNode *cur = other->delimiters.first; cur;
            cur = cur->next)
        fklAddString(&g->delimiters, cur->k);
    return 0;
}
