#include <fakeLisp/base.h>
#include <fakeLisp/bigint.h>
#include <fakeLisp/common.h>
#include <fakeLisp/dis.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/str_builder.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <fakeLisp/sb_helper.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdalign.h>
#include <stdio.h>
#include <string.h>

// lalr1
// =====

// 判断非终结符是否是 S'
static inline int is_Sq_nt(const FklGrammerNonterm nt) { return nt == NULL; }

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

FklGrammerProduction *fklCreateProduction(FklVMvalue *sid,
        size_t len,
        const FklGrammerSym *syms,
        const char *print_name,
        FklProdActionFunc func,
        void *ctx,
        void (*destroy)(void *),
        void *(*copyer)(const void *)) {
    size_t total_size = sizeof(FklGrammerProduction) //
                      + (len * sizeof(FklGrammerSym));
    FklGrammerProduction *r = (FklGrammerProduction *)fklZcalloc(1, total_size);
    FKL_ASSERT(r);
    r->left = sid;
    r->len = len;
    r->print_name = print_name;
    r->func = func;
    r->ctx = ctx;
    r->ctx_destroy = destroy;
    r->ctx_copy = copyer;
    if (syms != NULL) {
        memcpy(r->syms, syms, len * sizeof(FklGrammerSym));
        for (size_t i = 0; i < r->len; ++i) {
            FklGrammerSym *s = &r->syms[i];
            if (s->type == FKL_TERM_COMP) {
                s->comp.parts = s + 1;
            }
        }
    }
    return r;
}

FklGrammerProduction *fklCreateEmptyProduction(FklVMvalue *sid,
        size_t len,
        const char *print_name,
        FklProdActionFunc func,
        void *ctx,
        void (*destroy)(void *),
        void *(*copyer)(const void *)) {
    size_t total_size = sizeof(FklGrammerProduction) //
                      + len * sizeof(FklGrammerSym);
    FklGrammerProduction *r = (FklGrammerProduction *)fklZcalloc(1, total_size);
    FKL_ASSERT(r);
    r->left = sid;
    r->len = len;
    r->print_name = print_name;
    r->func = func;
    r->ctx = ctx;
    r->ctx_destroy = destroy;
    r->ctx_copy = copyer;
    return r;
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
        if (igs->term_type == FKL_TERM_BUILTIN) {
            igs->b = sym->b;
        } else if (igs->term_type == FKL_TERM_REGEX) {
            igs->re = sym->re;
        } else if (igs->term_type == FKL_TERM_STRING) {
            igs->str = sym->str;
        } else {
            fklZfree(ig);
            return NULL;
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
            return u0->nt == u1->nt;
            break;
        case FKL_TERM_IGNORE:
            return 1;
            break;
        case FKL_TERM_COMP:
            return u0->comp.len == u1->comp.len;
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
        case FKL_TERM_COMP:
            FKL_UNREACHABLE();
            break;
        }
    }
    return 1;
}

static inline FklGrammerProduction *create_extra_production(FklVMvalue *start) {
    FklGrammerProduction *prod = fklCreateEmptyProduction(NULL,
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
    u->nt = start;
    return prod;
}

int fklAddProdAndExtraToGrammer(FklGrammer *g, FklGrammerProduction *prod) {
    const FklGraSidBuiltinHashMap *builtins = &g->builtins;
    FklProdHashMap *productions = &g->prods;
    const FklGrammerNonterm left = prod->left;
    if (fklGetBuiltinMatch(builtins, left))
        return 1;
    FklGrammerProduction **pp = fklProdHashMapGet2(productions, left);
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
            prod->idx = g->prod_count;
            g->prod_count++;
            prod->next = NULL;
            *pp = prod;
        }
    } else {
        if (!g->start) {
            g->start = left;
            FklGrammerProduction *extra_prod = create_extra_production(left);
            extra_prod->next = NULL;
            pp = fklProdHashMapAdd(productions, &extra_prod->left, NULL);
            *pp = extra_prod;
            (*pp)->idx = g->prod_count;
            g->prod_count++;
        }
        prod->next = NULL;
        pp = fklProdHashMapAdd2(productions, left, NULL);
        prod->idx = g->prod_count;
        *pp = prod;
        g->prod_count++;
    }
    return 0;
}

int fklAddProdToProdTable(FklGrammer *g, FklGrammerProduction *prod) {
    const FklGraSidBuiltinHashMap *builtins = &g->builtins;
    FklProdHashMap *productions = &g->prods;
    const FklGrammerNonterm left = prod->left;
    if (fklGetBuiltinMatch(builtins, left))
        return 1;
    FklGrammerProduction **pp = fklProdHashMapGet2(productions, left);
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
            prod->idx = g->prod_count;
            g->prod_count++;
            prod->next = NULL;
            *pp = prod;
        }
    } else {
        prod->next = NULL;
        pp = fklProdHashMapAdd2(productions, left, NULL);
        prod->idx = g->prod_count;
        g->prod_count++;
        *pp = prod;
    }
    return 0;
}

int fklAddProdToProdTableNoRepeat(FklGrammer *g, FklGrammerProduction *prod) {
    const FklGraSidBuiltinHashMap *builtins = &g->builtins;
    FklProdHashMap *productions = &g->prods;
    const FklGrammerNonterm left = prod->left;
    if (fklGetBuiltinMatch(builtins, left))
        return 1;
    FklGrammerProduction **pp = fklProdHashMapGet2(productions, left);
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
            prod->idx = g->prod_count;
            g->prod_count++;
            prod->next = NULL;
            *pp = prod;
        }
    } else {
        prod->next = NULL;
        pp = fklProdHashMapAdd2(productions, left, NULL);
        prod->idx = g->prod_count;
        g->prod_count++;
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
    return FKL_TYPE_CAST(uintptr_t, *nt0) > FKL_TYPE_CAST(uintptr_t, *nt1);
}

static int nonterm_lt(const FklGrammerNonterm *nt0,
        const FklGrammerNonterm *nt1) {
    return FKL_TYPE_CAST(uintptr_t, *nt0) > FKL_TYPE_CAST(uintptr_t, *nt1);
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
        } else if (s0->type == FKL_TERM_COMP) {
            if (s0->comp.len < s1->comp.len)
                return -1;
            else if (s0->comp.len > s1->comp.len)
                return 1;
        } else if (nonterm_lt(&s0->nt, &s1->nt))
            return -1;
        else if (nonterm_gt(&s0->nt, &s1->nt))
            return 1;
    }
    return 0;
}

static inline void build_string_in_hex(const FklString *stri,
        FklStrBuilder *build) {
    size_t size = stri->size;
    const char *str = stri->str;
    for (size_t i = 0; i < size; i++)
        SB_FMT("\\x%02X", str[i]);
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
        switch (ig->term_type) {
        case FKL_TERM_BUILTIN: {
            size_t len = 0;
            FklBuiltinTerminalMatchArgs args = {
                .len = ig->b.len,
                .args = ig->b.args,
            };
            if (ig->b.t->match(&args,
                        g,
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
        } break;

        case FKL_TERM_REGEX: {
            int last_is_true = 0;
            size_t len = fklRegexLexMatchp(ig->re, str, restLen, &last_is_true);
            if (len > restLen) {
                *is_waiting_for_more |= last_is_true;
                return 0;
            } else {
                str += len;
                matchLen += len;
            }
        } break;

        case FKL_TERM_STRING: {
            const FklString *laString = ig->str;
            if (fklStringCharBufMatch(laString, str, restLen - matchLen) >= 0) {
                str += laString->size;
                matchLen += laString->size;
            } else
                return 0;
        } break;

        case FKL_TERM_NONE:
        case FKL_TERM_KEYWORD:
        case FKL_TERM_IGNORE:
        case FKL_TERM_EOF:
        case FKL_TERM_NONTERM:
        case FKL_TERM_COMP:
            FKL_UNREACHABLE();
            break;
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
    if (restLen == 0)
        return 0;

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

#define INCLUDED_BUILTIN_H
#include "grammer/builtin.c"
#undef INCLUDED_BUILTIN_H

static const FklLalrBuiltinMatch builtin_match_dec_int = {
    .name = "?dint",
    .key = "builtin_match_dec_int",
    .match = builtin_match_dec_int_func,

    .min_args = 0,
    .max_args = 0,
};

static const FklLalrBuiltinMatch builtin_match_hex_int = {
    .name = "?xint",
    .key = "builtin_match_hex_int",
    .match = builtin_match_hex_int_func,

    .min_args = 0,
    .max_args = 0,
};

static const FklLalrBuiltinMatch builtin_match_oct_int = {
    .name = "?oint",
    .key = "builtin_match_oct_int",
    .match = builtin_match_oct_int_func,

    .min_args = 0,
    .max_args = 0,
};

static const FklLalrBuiltinMatch builtin_match_dec_float = {
    .name = "?dfloat",
    .key = "builtin_match_dec_float",
    .match = builtin_match_dec_float_func,

    .min_args = 0,
    .max_args = 0,
};

static const FklLalrBuiltinMatch builtin_match_hex_float = {
    .name = "?xfloat",
    .key = "builtin_match_hex_float",
    .match = builtin_match_hex_float_func,

    .min_args = 0,
    .max_args = 0,
};

static const FklLalrBuiltinMatch builtin_match_identifier = {
    .name = "?identifier",
    .key = "builtin_match_identifier",
    .match = builtin_match_identifier_func,

    .min_args = 0,
    .max_args = 0,
};

static const FklLalrBuiltinMatch builtin_match_nodelimiter = {
    .name = "?nodelimiter",
    .key = "builtin_match_nodelimiter",
    .match = builtin_match_nodelimiter_func,

    .min_args = 0,
    .max_args = 0,
};

static const FklLalrBuiltinMatch builtin_match_s_dint = {
    .name = "?s-dint",
    .key = "builtin_match_s_dint",
    .match = builtin_match_s_dint_func,

    .min_args = 0,
    .max_args = 1,
};

static const FklLalrBuiltinMatch builtin_match_s_xint = {
    .name = "?s-xint",
    .key = "builtin_match_s_xint",
    .match = builtin_match_s_xint_func,

    .min_args = 0,
    .max_args = 1,
};

static const FklLalrBuiltinMatch builtin_match_s_oint = {
    .name = "?s-oint",
    .key = "builtin_match_s_oint",
    .match = builtin_match_s_oint_func,

    .min_args = 0,
    .max_args = 1,
};

static const FklLalrBuiltinMatch builtin_match_s_dfloat = {
    .name = "?s-dfloat",
    .key = "builtin_match_s_dfloat",
    .match = builtin_match_s_dfloat_func,

    .min_args = 0,
    .max_args = 1,
};

static const FklLalrBuiltinMatch builtin_match_s_xfloat = {
    .name = "?s-xfloat",
    .key = "builtin_match_s_xfloat",
    .match = builtin_match_s_xfloat_func,

    .min_args = 0,
    .max_args = 1,
};

static const FklLalrBuiltinMatch builtin_match_s_char = {
    .name = "?s-char",
    .key = "builtin_match_s_char",
    .match = builtin_match_s_char_func,

    .min_args = 1,
    .max_args = 1,
};

static const FklLalrBuiltinMatch builtin_match_never = {
    .name = "?never",
    .key = "builtin_match_never",
    .match = builtin_match_never_func,

    .min_args = 0,
    .max_args = 0,
};

static const FklLalrBuiltinMatch builtin_match_symbol = {
    .name = "?symbol",
    .key = "builtin_match_symbol",
    .match = builtin_match_symbol_func,

    .min_args = 0,
    .max_args = 2,
};

static const FklLalrBuiltinMatch builtin_match_raw_string = {
    .name = "?raw-string",
    .key = "builtin_match_raw_string",
    .match = builtin_match_raw_string_func,

    .min_args = 1,
    .max_args = 2,
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

    {"?raw-string",  &builtin_match_raw_string  },
    {"?never",       &builtin_match_never       },

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
    g->prod_count = 0;
    g->start = NULL;
    fklProdHashMapClear(&g->prods);
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
    fklProdHashMapUninit(&g->prods);
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
        .len = b->len,
        .args = b->args,
    };
    FklBuiltinTermMatchFunc match = b->t->match;
    FKL_ASSERT(match != NULL);
    return match(&args, g, "", "", 0, &matchLen, &ctx, &is_waiting_for_more);
}

static inline int is_comp_terminal_match_epsilon(const FklGrammer *g,
        const FklCompositeSym *comp) {
    FKL_ASSERT(comp->parts);
    for (size_t i = 0; i < comp->len; ++i) {
        const FklGrammerSym *p = &comp->parts[i];
        int eps = 0;
        switch (p->type) {
        default: // should not happen, let it crash
            FKL_UNREACHABLE();
            break;

        case FKL_TERM_BUILTIN:
            eps = is_builtin_terminal_match_epsilon(g, &p->b);
            break;
        case FKL_TERM_REGEX:
            eps = is_regex_match_epsilon(p->re);
            break;
        case FKL_TERM_STRING:
            eps = p->str->size == 0;
            break;
        }

        if (!eps) {
            return 0;
        }
    }

    return 1;
}

static inline int get_first_from_syms(FklGrammer *g,
        FklFirstSetItem *first,
        const FklGrammerSym *syms,
        size_t len,
        int *done_or_error) {
    size_t lastIdx = len - 1;
    FklFirstSetHashMap *firsts = &g->firstSets;
    int change = 0;
    const FklFirstSetItem *cur_first;
    for (size_t i = 0; i < len; i++) {
        const FklGrammerSym *sym = &syms[i];
        FklLalrItemLookAhead la = { .t = sym->type };

        switch (sym->type) {
        case FKL_TERM_BUILTIN: {
            int r = is_builtin_terminal_match_epsilon(g, &sym->b);
            la.b = sym->b;
            change |= !fklLookAheadHashSetPut(&first->first, &la);
            if (r) {
                if (i == lastIdx) {
                    change |= first->hasEpsilon != 1;
                    first->hasEpsilon = 1;
                }
            } else {
                *done_or_error = 1;
                return change;
            }
        } break;

        case FKL_TERM_COMP: {
            la.comp = sym->comp;
            change |= !fklLookAheadHashSetPut(&first->first, &la);

            int all_epsilon = is_comp_terminal_match_epsilon(g, &sym->comp);
            i += sym->comp.len;
            if (all_epsilon) {
                if (i == lastIdx) {
                    change |= first->hasEpsilon != 1;
                    first->hasEpsilon = 1;
                }
            } else {
                *done_or_error = 1;
                return change;
            }
        } break;

        case FKL_TERM_REGEX: {
            uint32_t r = is_regex_match_epsilon(sym->re);
            la.re = sym->re;
            change |= !fklLookAheadHashSetPut(&first->first, &la);
            if (r) {
                if (i == lastIdx) {
                    change |= first->hasEpsilon != 1;
                    first->hasEpsilon = 1;
                }
            } else {
                *done_or_error = 1;
                return change;
            }
        } break;
        case FKL_TERM_NONTERM: {
            cur_first = fklFirstSetHashMapGet(firsts, &sym->nt);
            if (!cur_first) {
                // error occur
                *done_or_error = -1;
                return change;
            }

            for (const FklLookAheadHashSetNode *syms = cur_first->first.first;
                    syms;
                    syms = syms->next) {
                change |= !fklLookAheadHashSetPut(&first->first, &syms->k);
            }
            if (cur_first->hasEpsilon && i == lastIdx) {
                change |= first->hasEpsilon != 1;
                first->hasEpsilon = 1;
            }
            if (!cur_first->hasEpsilon) {
                *done_or_error = 1;
                return change;
            }
        } break;
        case FKL_TERM_IGNORE: {
            change |= !fklLookAheadHashSetPut(&first->first, &la);
            if (i == lastIdx) {
                change |= first->hasEpsilon != 1;
                first->hasEpsilon = 1;
            }
        } break;

        case FKL_TERM_STRING:
        case FKL_TERM_KEYWORD: {
            const FklString *s = sym->str;
            if (s->size == 0) {
                if (i == lastIdx) {
                    change |= first->hasEpsilon != 1;
                    first->hasEpsilon = 1;
                }
            } else {
                la.s = s;
                change |= !fklLookAheadHashSetPut(&first->first, &la);
                *done_or_error = 1;
                return change;
            }
        } break;
        case FKL_TERM_NONE:
        case FKL_TERM_EOF:
            FKL_UNREACHABLE();
            break;
        }
    }

    return change;
}

static inline int compute_all_first_set(FklGrammer *g) {
    FklFirstSetHashMap *firsts = &g->firstSets;

    const FklFirstSetItem item = { .hasEpsilon = 0 };

    for (const FklProdHashMapNode *sidl = g->prods.first; sidl;
            sidl = sidl->next) {
        if (is_Sq_nt(sidl->k))
            continue;
        fklFirstSetHashMapPut(firsts, &sidl->k, &item);
    }

    int change;
    FklFirstSetItem *first;

    do {
        change = 0;
        for (const FklProdHashMapNode *leftProds = g->prods.first; leftProds;
                leftProds = leftProds->next) {
            if (is_Sq_nt(leftProds->k))
                continue;
            first = fklFirstSetHashMapGet(firsts, &leftProds->k);
            const FklGrammerProduction *prods = leftProds->v;
            for (; prods; prods = prods->next) {
                size_t len = prods->len;
                if (!len) {
                    change |= first->hasEpsilon != 1;
                    first->hasEpsilon = 1;
                    continue;
                }
                int done = 0;
                const FklGrammerSym *syms = prods->syms;
                change |= get_first_from_syms(g, first, syms, len, &done);
                if (done > 0)
                    break;
                if (done < 0)
                    return 1;
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
    fklProdHashMapInit(&r->prods);
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

int fklIsGrammerInited(const FklGrammer *g) { return g->prods.buckets != NULL; }

// GraProdVector
#define FKL_VECTOR_TYPE_PREFIX Gra
#define FKL_VECTOR_METHOD_PREFIX gra
#define FKL_VECTOR_ELM_TYPE FklGrammerProduction *
#define FKL_VECTOR_ELM_TYPE_NAME Prod
#include <fakeLisp/cont/vector.h>

int fklCheckUndefinedNonterm(FklGrammer *g, FklGrammerNonterm *nt) {
    FklProdHashMap *productions = &g->prods;
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

static inline void print_unresolved_terminal(const FklGrammerNonterm nt,
        FklStrBuilder *fp) {
    fklStrBuilderPuts(fp, "nonterm: ");
    fklPrintSymbolLiteral2(FKL_VM_SYM(nt), fp);
    fklStrBuilderPuts(fp, " is not defined\n");
}

int fklAddExtraProdToGrammer(FklGrammer *g) {
    FklGrammerNonterm left = g->start;
    const FklGraSidBuiltinHashMap *builtins = &g->builtins;
    if (fklGetBuiltinMatch(builtins, left))
        return 1;
    FklGrammerProduction *extra_prod = create_extra_production(left);
    extra_prod->next = NULL;
    FklGrammerProduction **item =
            fklProdHashMapAdd(&g->prods, &extra_prod->left, NULL);
    *item = extra_prod;
    (*item)->idx = g->prod_count;
    g->prod_count++;
    return 0;
}

static inline void print_as_regex(const FklString *str, FklStrBuilder *build) {
    const char *cur = str->str;
    const char *const end = cur + str->size;
    SB_FMT("/");
    for (; cur < end; ++cur) {
        if (*cur == '/')
            SB_FMT("\\/");
        else
            SB_FMT("%c", *cur);
    }
    SB_FMT("/");
}

static inline void print_prod_sym(FklVM *vm,
        const FklGrammerSym *u,
        const FklRegexTable *rt,
        FklStrBuilder *build) {
    switch (u->type) {
    case FKL_TERM_BUILTIN:
        SB_FMT("%s", u->b.t->name);
        if (u->b.len) {
            SB_FMT("[");
            size_t i = 0;
            for (; i < u->b.len - 1; ++i) {
                fklPrintSymbolLiteral2(u->b.args[i], build);
                SB_FMT(", ");
            }
            fklPrintSymbolLiteral2(u->b.args[i], build);
            SB_FMT("]");
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

    case FKL_TERM_COMP:
        for (size_t i = 0; i < u->comp.len; i++) {
            if (i)
                SB_FMT("..");
            print_prod_sym(vm, &u->comp.parts[i], rt, build);
        }
        break;

    case FKL_TERM_NONTERM:
        fklPrin1VMvalue2(u->nt, build, vm);
        break;
    case FKL_TERM_IGNORE:
        SB_FMT("?e");
        break;
    case FKL_TERM_NONE:
    case FKL_TERM_EOF:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void
print_string_as_dot(const char *str, char se, size_t size, FklStrBuilder *out) {
    uint64_t i = 0;
    while (i < size) {
        unsigned int l =
                fklGetByteNumOfUtf8((const uint8_t *)&str[i], size - i);
        if (l == 7) {
            uint8_t j = str[i];
            fklStrBuilderFmt(out, "\\x%02X", j);
            i++;
        } else if (l == 1) {
            if (str[i] == se)
                fklStrBuilderFmt(out, "\\%c", se);
            else if (str[i] == '"')
                fklStrBuilderPuts(out, "\\\"");
            else if (str[i] == '\'')
                fklStrBuilderPuts(out, "\\'");
            else if (str[i] == '\\')
                fklStrBuilderPuts(out, "\\\\");
            else if (isgraph(str[i]))
                fklStrBuilderPutc(out, str[i]);
            else if (fklStrBuilderPutEscSeq(out, str[i]))
                ;
            else {
                uint8_t j = str[i];
                fklStrBuilderFmt(out, "\\x%02X", j);
            }
            i++;
        } else {
            for (unsigned int j = 0; j < l; j++)
                fklStrBuilderPutc(out, str[i + j]);
            i += l;
        }
    }
}
static inline void print_prod_sym_as_dot(const FklGrammerSym *u,
        const FklRegexTable *rt,
        FklStrBuilder *fp) {
    switch (u->type) {
    case FKL_TERM_BUILTIN:
        fklStrBuilderPutc(fp, '|');
        fklStrBuilderPuts(fp, u->b.t->name);
        fklStrBuilderPutc(fp, '|');
        break;
    case FKL_TERM_REGEX: {
        const FklString *str = fklGetStringWithRegex(rt, u->re, NULL);
        fklStrBuilderPuts(fp, "\\/'");
        print_string_as_dot(str->str, '"', str->size, fp);
        fklStrBuilderPuts(fp, "'\\/");
    } break;
    case FKL_TERM_STRING:
    case FKL_TERM_KEYWORD: {
        const FklString *str = u->str;
        fklStrBuilderPuts(fp, "\\\'");
        print_string_as_dot(str->str, '"', str->size, fp);
        fklStrBuilderPuts(fp, "\\\'");
    } break;
    case FKL_TERM_NONTERM: {
        const FklString *str = FKL_VM_SYM(u->nt);
        fklStrBuilderPutc(fp, '|');
        print_string_as_dot(str->str, '|', str->size, fp);
        fklStrBuilderPutc(fp, '|');
    } break;
    case FKL_TERM_IGNORE:
        fklStrBuilderPuts(fp, "?e");
        break;

    case FKL_TERM_COMP:
        for (size_t i = 0; i < u->comp.len; i++) {
            if (i)
                fklStrBuilderPuts(fp, "..");
            print_prod_sym_as_dot(&u->comp.parts[i], rt, fp);
        }
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
        uint32_t idx,
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
        .idx = i->idx,
        .la = i->la,
    };

    const FklGrammerSym *s = get_item_next(&item);
    int is_at_delim_v = s && is_at_delim_sym(&item);
    uint32_t advance = is_at_delim_v ? 2 : 1;

    if (s && s->type == FKL_TERM_COMP)
        advance += s->comp.len;
    item.idx += advance;

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

        case FKL_TERM_COMP: {
            if (la0->comp.len != la1->comp.len)
                return la0->comp.len > la1->comp.len ? 1 : -1;

            for (size_t i = 0; i < la0->comp.len; i++) {
                int r = grammer_sym_cmp(&la0->comp.parts[i],
                        &la1->comp.parts[i]);
                if (r)
                    return r;
            }
            return 0;
        } break;

        case FKL_TERM_STRING:
        case FKL_TERM_KEYWORD:
            return fklStringCmp(la0->s, la1->s);
            break;
        case FKL_TERM_REGEX: {
            int64_t r = ((int64_t)la0->re->totalsize)
                      - ((int64_t)la1->re->totalsize);

            return r > 0 ? 1 //
                 : r < 0 ? -1
                         : 0;
        } break;
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
                const FklGrammerNonterm left = sym->nt;
                if (!fklNontermHashSetPut2(&sidSet, left)) {
                    change = 1;
                    fklNontermHashSetPut2(&changeSet, left);
                }
            }
        }

        FklGrammerProduction *prod = NULL;
        for (FklNontermHashSetNode *lefts = changeSet.first; lefts;
                lefts = lefts->next) {
            prod = fklGetProductions(g, lefts->k);
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

static void print_lookahead(const FklLalrItemLookAhead *la,
        const FklRegexTable *rt,
        FklStrBuilder *build);

static inline void print_lookahead_comp(const FklCompositeSym *comp,
        const FklRegexTable *rt,
        FklStrBuilder *build) {
    for (size_t i = 0; i < comp->len; i++) {
        if (i) {
            fklStrBuilderPuts(build, "..");
        }
        const FklGrammerSym *p = &comp->parts[i];
        FklLalrItemLookAhead pla;
        switch (p->type) {
        default:
            FKL_UNREACHABLE();
            break;

        case FKL_TERM_BUILTIN:
            pla = (FklLalrItemLookAhead){ .t = FKL_TERM_BUILTIN, .b = p->b };
            break;
        case FKL_TERM_REGEX:
            pla = (FklLalrItemLookAhead){ .t = FKL_TERM_REGEX, .re = p->re };
            break;
        case FKL_TERM_STRING:
            pla = (FklLalrItemLookAhead){ .t = FKL_TERM_STRING, .s = p->str };
            break;
        }
        print_lookahead(&pla, rt, build);
    }
}

static void print_lookahead(const FklLalrItemLookAhead *la,
        const FklRegexTable *rt,
        FklStrBuilder *build) {
    switch (la->t) {
    case FKL_TERM_STRING:
        fklPrintStringLiteral2(la->s, build);
        break;
    case FKL_TERM_KEYWORD:
        fklPrintSymbolLiteral2(la->s, build);
        break;
    case FKL_TERM_EOF:
        fklStrBuilderPuts(build, "$$");
        break;
    case FKL_TERM_BUILTIN:
        fklStrBuilderPuts(build, la->b.t->name);
        if (la->b.len) {
            fklStrBuilderPutc(build, '[');
            size_t i = 0;
            for (; i < la->b.len - 1; ++i) {
                fklPrintStringLiteral2(la->b.args[i], build);
                fklStrBuilderPuts(build, " , ");
            }
            fklPrintStringLiteral2(la->b.args[i], build);
            fklStrBuilderPutc(build, ']');
        }
        break;
    case FKL_TERM_NONE:
        fklStrBuilderPuts(build, "()");
        break;
    case FKL_TERM_IGNORE:
        fklStrBuilderPuts(build, "?e");
        break;
    case FKL_TERM_COMP:
        print_lookahead_comp(&la->comp, rt, build);
        break;
    case FKL_TERM_REGEX:
        print_as_regex(fklGetStringWithRegex(rt, la->re, NULL), build);
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void print_item(FklVM *vm,
        const FklLalrItem *item,
        const FklRegexTable *rt,
        FklStrBuilder *build) {
    size_t i = 0;
    size_t idx = item->idx;
    FklGrammerProduction *prod = item->prod;
    size_t len = prod->len;
    FklGrammerSym *syms = prod->syms;
    if (!is_Sq_nt(prod->left))
        fklPrintString2(FKL_VM_SYM(prod->left), build);
    else
        fklStrBuilderPuts(build, "S'");
    fklStrBuilderPuts(build, " ->");
    for (; i < idx; i++) {
        fklStrBuilderPutc(build, ' ');
        print_prod_sym(vm, &syms[i], rt, build);
        if (syms[i].type == FKL_TERM_COMP) {
            i += syms[i].comp.len;
            continue;
        }
    }
    fklStrBuilderPuts(build, " *");
    for (; i < len; i++) {
        fklStrBuilderPutc(build, ' ');
        print_prod_sym(vm, &syms[i], rt, build);
        if (syms[i].type == FKL_TERM_COMP) {
            i += syms[i].comp.len;
            continue;
        }
    }
    fklStrBuilderPuts(build, " ## ");
    print_lookahead(&item->la, rt, build);
}

void fklPrintItemSet(FklVM *vm,
        const FklLalrItemHashSet *itemSet,
        const FklGrammer *g,
        FklStrBuilder *build) {
    FklLalrItem const *curItem = NULL;
    for (FklLalrItemHashSetNode *list = itemSet->first; list;
            list = list->next) {
        if (!curItem || list->k.idx != curItem->idx
                || list->k.prod != curItem->prod) {
            if (curItem)
                fklStrBuilderPutc(build, '\n');
            curItem = &list->k;
            print_item(vm, curItem, &g->regexes, build);
        } else {
            fklStrBuilderPuts(build, " , ");
            print_lookahead(&list->k.la, &g->regexes, build);
        }
    }
    fklStrBuilderPutc(build, '\n');
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
        return s0->str == s1->str;
        break;
    case FKL_TERM_NONTERM:
        return fklNontermEqual(&s0->nt, &s1->nt);
        break;
    case FKL_TERM_COMP:
        return fklCompositeGrammerSymEqual(&s0->comp, &s1->comp);
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
    uintptr_t seed = 0;
    switch (s->type) {
    default:
        FKL_UNREACHABLE();
        break;

    case FKL_TERM_KEYWORD:
    case FKL_TERM_STRING:
        seed = fklHash64Shift(FKL_TYPE_CAST(uintptr_t, s->str) >> 3);
        break;

    case FKL_TERM_BUILTIN:
        seed = fklBuiltinGrammerSymHash(&s->b);
        break;
    case FKL_TERM_REGEX:
        seed = fklHash64Shift(FKL_TYPE_CAST(uintptr_t, s->re) >> 3);
        break;
    case FKL_TERM_NONTERM:
        seed = fklNontermHash(&s->nt);
        break;

    case FKL_TERM_COMP:
        seed = fklCompositeGrammerSymHash(&s->comp);
        break;
    }

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

            FklGrammerSym sym_copy = *sym;
            // parts relocation
            if (sym_copy.type == FKL_TERM_COMP)
                sym_copy.comp.parts = sym + 1;

            GraLinkSym ss = {
                .sym = sym_copy,
                .allow_ignore = allow_ignore || is_at_delim,
                .is_at_delim = is_at_delim,
            };
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
    const FklGrammerNonterm left = NULL;

    FklGrammerProduction *prod = *fklProdHashMapGet(&grammer->prods, &left);
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
        for (size_t i = idx; i < len; i++) {
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

            case FKL_TERM_COMP: {
                la.comp = sym->comp;
                fklLookAheadHashSetPut(first, &la);

                int all_epsilon = is_comp_terminal_match_epsilon(g, &sym->comp);
                i += sym->comp.len;

                if (all_epsilon) {
                    hasEpsilon = i == lastIdx;
                } else {
                    goto break_loop;
                }
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
                const FklGrammerNonterm left = next->nt;
                FklGrammerProduction *prods = fklGetProductions(g, left);
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
                uint32_t advance = is_at_delim_v ? 2 : 1;

                if (s && s->type == FKL_TERM_COMP)
                    advance += s->comp.len;
                i.idx += advance;

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

static void print_lookahead_as_dot(const FklLalrItemLookAhead *la,
        const FklRegexTable *rt,
        FklStrBuilder *fp);

static void print_lookahead_comp_as_dot(const FklCompositeSym *comp,
        const FklRegexTable *rt,
        FklStrBuilder *fp) {
    for (size_t i = 0; i < comp->len; i++) {
        if (i)
            fklStrBuilderPuts(fp, "..");
        const FklGrammerSym *p = &comp->parts[i];
        FklLalrItemLookAhead pla;
        switch (p->type) {
        default:
            FKL_UNREACHABLE();
            break;

        case FKL_TERM_BUILTIN:
            pla = (FklLalrItemLookAhead){ .t = FKL_TERM_BUILTIN, .b = p->b };
            break;
        case FKL_TERM_REGEX:
            pla = (FklLalrItemLookAhead){ .t = FKL_TERM_REGEX, .re = p->re };
            break;
        case FKL_TERM_STRING:
            pla = (FklLalrItemLookAhead){ .t = FKL_TERM_STRING, .s = p->str };
            break;
        }
        print_lookahead_as_dot(&pla, rt, fp);
    }
}

static void print_lookahead_as_dot(const FklLalrItemLookAhead *la,
        const FklRegexTable *rt,
        FklStrBuilder *fp) {
    switch (la->t) {
    case FKL_TERM_STRING:
    case FKL_TERM_KEYWORD: {
        fklStrBuilderPuts(fp, "\\\'");
        const FklString *str = la->s;
        print_string_as_dot(str->str, '\'', str->size, fp);
        fklStrBuilderPuts(fp, "\\\'");
    } break;
    case FKL_TERM_EOF:
        fklStrBuilderPuts(fp, "$$");
        break;
    case FKL_TERM_BUILTIN:
        fklStrBuilderFmt(fp, "|%s|", la->b.t->name);
        break;
    case FKL_TERM_NONE:
        fklStrBuilderPuts(fp, "()");
        break;
    case FKL_TERM_IGNORE:
        fklStrBuilderPuts(fp, "?e");
        break;
    case FKL_TERM_REGEX: {
        fklStrBuilderPuts(fp, "\\/\'");
        const FklString *str = fklGetStringWithRegex(rt, la->re, NULL);
        print_string_as_dot(str->str, '\'', str->size, fp);
        fklStrBuilderPuts(fp, "\\/\'");
    } break;

    case FKL_TERM_COMP:
        print_lookahead_comp_as_dot(&la->comp, rt, fp);
        break;

    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void print_item_as_dot(const FklLalrItem *item,
        const FklRegexTable *rt,
        FklStrBuilder *fp) {
    size_t i = 0;
    size_t idx = item->idx;
    FklGrammerProduction *prod = item->prod;
    size_t len = prod->len;
    FklGrammerSym *syms = prod->syms;
    if (!is_Sq_nt(prod->left)) {
        const FklString *str = FKL_VM_SYM(prod->left);
        fklStrBuilderPutc(fp, '|');
        print_string_as_dot(str->str, '"', str->size, fp);
        fklStrBuilderPutc(fp, '|');
    } else
        fklStrBuilderPuts(fp, "S'");
    fklStrBuilderPuts(fp, " ->");
    for (; i < idx; i++) {
        fklStrBuilderPutc(fp, ' ');
        print_prod_sym_as_dot(&syms[i], rt, fp);
        if (syms[i].type == FKL_TERM_COMP) {
            i += syms[i].comp.len;
            continue;
        }
    }
    fklStrBuilderPuts(fp, " *");
    for (; i < len; i++) {
        fklStrBuilderPutc(fp, ' ');
        print_prod_sym_as_dot(&syms[i], rt, fp);
        if (syms[i].type == FKL_TERM_COMP) {
            i += syms[i].comp.len;
            continue;
        }
    }
    fklStrBuilderPuts(fp, " , ");
    print_lookahead_as_dot(&item->la, rt, fp);
}

static inline void print_item_set_as_dot(const FklLalrItemHashSet *itemSet,
        const FklGrammer *g,
        FklStrBuilder *fp) {
    FklLalrItem const *curItem = NULL;
    for (FklLalrItemHashSetNode *list = itemSet->first; list;
            list = list->next) {
        if (!curItem || list->k.idx != curItem->idx
                || list->k.prod != curItem->prod) {
            if (curItem)
                fklStrBuilderPuts(fp, "\\l\\\n");
            curItem = &list->k;
            print_item_as_dot(curItem, &g->regexes, fp);
        } else {
            fklStrBuilderPuts(fp, " / ");
            print_lookahead_as_dot(&list->k.la, &g->regexes, fp);
        }
    }
    fklStrBuilderPuts(fp, "\\l\\\n");
}

static inline void print_lalr_item(FklVM *vm,
        const FklLalrItem *item,
        const FklStringTable *tt,
        const FklRegexTable *rt,
        FklStrBuilder *build) {
    size_t i = 0;
    size_t idx = item->idx;
    FklGrammerProduction *prod = item->prod;
    size_t len = prod->len;
    FklGrammerSym *syms = prod->syms;
    if (!is_Sq_nt(prod->left)) {
        fklStrBuilderPutc(build, '(');
        fklPrintSymbolLiteral2(FKL_VM_SYM(prod->left), build);
        fklStrBuilderPutc(build, ')');
    } else {
        fklStrBuilderPuts(build, "S'");
    }

    fklStrBuilderPuts(build, " ->");
    for (; i < idx; i++) {
        fklStrBuilderPutc(build, ' ');
        print_prod_sym(vm, &syms[i], rt, build);
        if (syms[i].type == FKL_TERM_COMP) {
            i += syms[i].comp.len;
            continue;
        }
    }
    fklStrBuilderPuts(build, " *");
    for (; i < len; i++) {
        fklStrBuilderPutc(build, ' ');
        print_prod_sym(vm, &syms[i], rt, build);
        if (syms[i].type == FKL_TERM_COMP) {
            i += syms[i].comp.len;
            continue;
        }
    }
}

void fklPrintItemStateSetAsDot(FklVM *vm,
        const FklLalrItemSetHashMap *i,
        const FklGrammer *g,
        FILE *fp) {
    FklStrBuilder builder = { 0 };
    fklInitStrBuilderFp(&builder, fp, NULL);
    fklPrintItemStateSet2(vm, i, g, &builder);
}

void fklPrintItemStateSetAsDot2(FklVM *vm,
        const FklLalrItemSetHashMap *i,
        const FklGrammer *g,
        FklStrBuilder *fp) {
    fklStrBuilderPuts(fp, "digraph \"items-lalr\"{\n");
    fklStrBuilderPuts(fp, "\trankdir=\"LR\"\n");
    fklStrBuilderPuts(fp, "\tranksep=1\n");
    fklStrBuilderPuts(fp, "\tgraph[overlap=false];\n");
    GraItemStateIdxHashMap idxTable;
    graItemStateIdxHashMapInit(&idxTable);
    size_t idx = 0;
    for (const FklLalrItemSetHashMapNode *l = i->first; l; l = l->next, idx++) {
        graItemStateIdxHashMapPut2(&idxTable, &l->elm, idx);
    }
    for (const FklLalrItemSetHashMapNode *ll = i->first; ll; ll = ll->next) {
        const FklLalrItemHashSet *i = &ll->k;
        idx = *graItemStateIdxHashMapGet2NonNull(&idxTable, &ll->elm);
        fklStrBuilderFmt(fp,
                "\t\"I%" PRIu64
                "\"[fontname=\"Courier\" nojustify=true shape=\"box\"label =\"I%" PRIu64
                "\\l\\\n",
                idx,
                idx);
        print_item_set_as_dot(i, g, fp);
        fklStrBuilderPuts(fp, "\"]\n");
        for (FklLalrItemSetLink *l = ll->v.links; l; l = l->next) {
            FklLalrItemSetHashMapElm *dst = l->dst;
            size_t *c = graItemStateIdxHashMapGet2NonNull(&idxTable, dst);
            fklStrBuilderFmt(fp,
                    "\tI%" PRIu64 "->I%" PRIu64
                    "[fontname=\"Courier\" label=\"",
                    idx,
                    *c);
            print_prod_sym_as_dot(&l->sym, &g->regexes, fp);
            fklStrBuilderPuts(fp, "\"]\n");
        }
        fklStrBuilderPutc(fp, '\n');
    }
    graItemStateIdxHashMapUninit(&idxTable);
    fklStrBuilderPuts(fp, "}");
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

    case FKL_TERM_COMP:
        action->match.comp = sym->comp;
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
        case FKL_TERM_COMP:
            return fklCompositeGrammerSymEqual(&match->comp, &la->comp);
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

    case FKL_TERM_COMP:
        action->match.comp = la->comp;
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
    for (size_t i = 0; i < len; ++i) {
        const FklGrammerSym *sym = &syms[i];
        if (sym->type == FKL_TERM_IGNORE || sym->type == FKL_TERM_COMP) {
            ++delim_len;
        }
    }

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
    if (is_Sq_nt(prod->left))
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
    case FKL_TERM_COMP:
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
    FKL_TERM_COMP,
    FKL_TERM_KEYWORD,
    FKL_TERM_STRING,
    FKL_TERM_REGEX,
    FKL_TERM_BUILTIN,
    FKL_TERM_IGNORE,
};

static inline size_t composite_fixed_size(const FklCompositeSym *c) {
    size_t total = 0;
    for (size_t i = 0; i < c->len; ++i) {
        const FklGrammerSym *p = &c->parts[i];
        if (p->type == FKL_TERM_STRING || p->type == FKL_TERM_KEYWORD)
            total += p->str->size;
        else if (p->type == FKL_TERM_REGEX)
            total += p->re->totalsize;
    }
    return total;
}

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

    case FKL_TERM_COMP: {
        size_t total = composite_fixed_size(&action->match.comp);
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_TERM_COMP
                    || allow_ignore < curAction->match.allow_ignore
                    || (curAction->match.t == FKL_TERM_COMP
                            && total > composite_fixed_size(
                                       &curAction->match.comp)))
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
        FklStrBuilder *build) {
    SB_FMT("match_ignore_%" PRIu64
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

int fklGenerateLalrAnalyzeTable(FklVM *vm,
        FklGrammer *grammer,
        FklLalrItemSetHashMap *states,
        FklStrBuf *error_msg) {
    FklStrBuilder err = { 0 };
    fklInitStrBuilderStrBuf(&err, error_msg, NULL);

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
                        fklStrBuilderFmt(&err,
                                "conflict at state %lu with [[  ",
                                idx);
                        print_lalr_item(vm,
                                &il->k,
                                &grammer->terminals,
                                &grammer->regexes,
                                &err);
                        fklStrBuilderFmt(&err, " ## ");
                        print_lookahead(&il->k.la, &grammer->regexes, &err);
                        fklStrBuilderFmt(&err, "  ]]");
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
        FklStrBuilder *fp) {
    switch (match->t) {
    case FKL_TERM_STRING: {
        fklStrBuilderPutc(fp, '\'');
        const FklString *str = match->str;
        print_string_as_dot(str->str, '\'', str->size, fp);
        fklStrBuilderPutc(fp, '\'');
    } break;
    case FKL_TERM_KEYWORD: {
        fklStrBuilderPutc(fp, '\'');
        const FklString *str = match->str;
        print_string_as_dot(str->str, '\'', str->size, fp);
        fklStrBuilderPuts(fp, "\'$");
    } break;
    case FKL_TERM_EOF:
        fklStrBuilderPuts(fp, "$$");
        break;
    case FKL_TERM_BUILTIN:
        fklStrBuilderPuts(fp, match->func.t->name);
        break;
    case FKL_TERM_NONE:
        fklStrBuilderPuts(fp, "()");
        break;
    case FKL_TERM_REGEX:
        print_as_regex(fklGetStringWithRegex(rt, match->re, NULL), fp);
        break;
    case FKL_TERM_IGNORE:
        fklStrBuilderPuts(fp, "?e");
        break;
    case FKL_TERM_COMP:
        print_lookahead_comp(&match->comp, rt, fp);
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

void fklPrintAnalysisTable(const FklGrammer *grammer, FILE *fp) {
    FklStrBuilder builder = { 0 };
    fklInitStrBuilderFp(&builder, fp, NULL);
    fklPrintAnalysisTable2(grammer, &builder);
}

void fklPrintAnalysisTable2(const FklGrammer *grammer, FklStrBuilder *fp) {
    size_t num = grammer->aTable.num;
    FklAnalysisState *states = grammer->aTable.states;

    for (size_t i = 0; i < num; i++) {
        fklStrBuilderFmt(fp, "%" PRIu64 ": ", i);
        FklAnalysisState *curState = &states[i];
        for (FklAnalysisStateAction *actions = curState->state.action; actions;
                actions = actions->next) {
            switch (actions->action) {
            case FKL_ANALYSIS_SHIFT:
                fklStrBuilderPuts(fp, "S(");
                print_lookahead_of_analysis_table(&grammer->regexes,
                        &actions->match,
                        fp);
                {
                    uintptr_t idx = actions->state - states;
                    fklStrBuilderFmt(fp, " , %" PRIu64 " )", idx);
                }
                break;
            case FKL_ANALYSIS_REDUCE:
                fklStrBuilderPuts(fp, "R(");
                print_lookahead_of_analysis_table(&grammer->regexes,
                        &actions->match,
                        fp);
                fklStrBuilderFmt(fp, " , %" PRIu64 " )", actions->prod->idx);
                break;
            case FKL_ANALYSIS_ACCEPT:
                fklStrBuilderPuts(fp, "acc(");
                print_lookahead_of_analysis_table(&grammer->regexes,
                        &actions->match,
                        fp);
                fklStrBuilderPutc(fp, ')');
                break;
            case FKL_ANALYSIS_IGNORE:
                break;
            }
            fklStrBuilderPutc(fp, '\t');
        }
        fklStrBuilderPuts(fp, "|\t");
        for (FklAnalysisStateGoto *gt = curState->state.gt; gt; gt = gt->next) {
            uintptr_t idx = gt->state - states;
            fklStrBuilderPutc(fp, '(');
            fklPrintSymbolLiteral2(FKL_VM_SYM(gt->nt), fp);
            fklStrBuilderFmt(fp, " , %" PRIu64 ")", idx);
            fklStrBuilderPutc(fp, '\t');
        }
        fklStrBuilderPutc(fp, '\n');
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
    case FKL_TERM_COMP:
        return fklCompositeGrammerSymHash(&match->comp);
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
        case FKL_TERM_COMP:
            return fklCompositeGrammerSymEqual(&m0->comp, &m1->comp);
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
        FklStrBuilder *fp) {
    size_t size = stri->size;
    const uint8_t *str = (uint8_t *)stri->str;
    size_t i = 0;
    while (i < size) {
        unsigned int l = fklGetByteNumOfUtf8(&str[i], size - i);
        if (l == 7) {
            uint8_t j = str[i];
            fklStrBuilderFmt(fp, "\\x%02X", j);
            i++;
        } else if (l == 1) {
            if (str[i] == '\\')
                fklStrBuilderPuts(fp, "\\\\");
            else if (str[i] == '|')
                fklStrBuilderPuts(fp, "\\|");
            else if (str[i] == ']')
                fklStrBuilderPuts(fp, "\\]");
            else if (isgraph(str[i]))
                fklStrBuilderPutc(fp, str[i]);
            else if (fklStrBuilderPutEscSeq(fp, str[i]))
                ;
            else {
                uint8_t j = str[i];
                fklStrBuilderFmt(fp, "\\x%02X", j);
            }
            i++;
        } else {
            for (unsigned int j = 0; j < l; j++)
                fklStrBuilderPutc(fp, str[i + j]);
            i += l;
        }
    }
}

static inline void print_string_for_grapheasy(const FklString *stri,
        FklStrBuilder *fp) {
    size_t size = stri->size;
    const uint8_t *str = (uint8_t *)stri->str;
    size_t i = 0;
    while (i < size) {
        unsigned int l = fklGetByteNumOfUtf8(&str[i], size - i);
        if (l == 7) {
            uint8_t j = str[i];
            fklStrBuilderFmt(fp, "\\x%02X", j);
            i++;
        } else if (l == 1) {
            if (str[i] == '\\')
                fklStrBuilderPuts(fp, "\\\\");
            else if (str[i] == '\'')
                fklStrBuilderPuts(fp, "\\\\'");
            else if (str[i] == '#')
                fklStrBuilderPuts(fp, "\\#");
            else if (str[i] == '|')
                fklStrBuilderPuts(fp, "\\|");
            else if (str[i] == ']')
                fklStrBuilderPuts(fp, "\\]");
            else if (isgraph(str[i]))
                fklStrBuilderPutc(fp, str[i]);
            else if (fklStrBuilderPutEscSeq(fp, str[i]))
                ;
            else {
                uint8_t j = str[i];
                fklStrBuilderFmt(fp, "\\x%02X", j);
            }
            i++;
        } else {
            for (unsigned int j = 0; j < l; j++)
                fklStrBuilderPutc(fp, str[i + j]);
            i += l;
        }
    }
}

static void print_lookahead_for_grapheasy(const FklAnalysisStateActionMatch *la,
        const FklRegexTable *rt,
        FklStrBuilder *fp);

static inline void print_lookahead_comp_for_grapheasy(
        const FklCompositeSym *comp,
        const FklRegexTable *rt,
        FklStrBuilder *fp) {
    for (size_t i = 0; i < comp->len; i++) {
        if (i) {
            fklStrBuilderPuts(fp, "..");
        }
        const FklGrammerSym *p = &comp->parts[i];
        FklAnalysisStateActionMatch pla;
        switch (p->type) {
        default:
            FKL_UNREACHABLE();
            break;
        case FKL_TERM_BUILTIN:
            pla = (FklAnalysisStateActionMatch){ .t = FKL_TERM_BUILTIN,
                .func = p->b };
            break;
        case FKL_TERM_REGEX:
            pla = (FklAnalysisStateActionMatch){ .t = FKL_TERM_REGEX,
                .re = p->re };
            break;
        case FKL_TERM_STRING:
            pla = (FklAnalysisStateActionMatch){ .t = FKL_TERM_STRING,
                .str = p->str };
            break;
        }
        print_lookahead_for_grapheasy(&pla, rt, fp);
    }
}

static void print_lookahead_for_grapheasy(const FklAnalysisStateActionMatch *la,
        const FklRegexTable *rt,
        FklStrBuilder *fp) {
    switch (la->t) {
    case FKL_TERM_STRING: {
        fklStrBuilderPutc(fp, '\'');
        print_string_for_grapheasy(la->str, fp);
        fklStrBuilderPutc(fp, '\'');
    } break;
    case FKL_TERM_KEYWORD: {
        fklStrBuilderPutc(fp, '\'');
        print_string_for_grapheasy(la->str, fp);
        fklStrBuilderPuts(fp, "\'$");
    } break;
    case FKL_TERM_EOF:
        fklStrBuilderPutc(fp, '$');
        break;
    case FKL_TERM_IGNORE:
        fklStrBuilderPuts(fp, "?e");
        break;
    case FKL_TERM_BUILTIN:
        fklStrBuilderFmt(fp, "\\|%s\\|", la->func.t->name);
        break;
    case FKL_TERM_NONE:
        fklStrBuilderPuts(fp, "()");
        break;
    case FKL_TERM_REGEX:
        fklStrBuilderPuts(fp, "\\/\'");
        const FklString *str = fklGetStringWithRegex(rt, la->re, NULL);
        print_string_for_grapheasy(str, fp);
        fklStrBuilderPuts(fp, "\\/\'");
        break;
    case FKL_TERM_COMP:
        print_lookahead_comp_for_grapheasy(&la->comp, rt, fp);
        break;

    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void print_table_header_for_grapheasy(const FklGrammer *g,
        const GraActionMatchHashSet *la,
        const FklNontermHashSet *sid,
        FklStrBuilder *fp) {
    fklStrBuilderPuts(fp, "\\n|");
    for (GraActionMatchHashSetNode *al = la->first; al; al = al->next) {
        print_lookahead_for_grapheasy(&al->k, &g->regexes, fp);
        fklStrBuilderPutc(fp, '|');
    }
    fklStrBuilderPuts(fp, "\\n|\\n");
    for (FklNontermHashSetNode *sl = sid->first; sl; sl = sl->next) {
        fklStrBuilderPutc(fp, '|');
        fklStrBuilderPuts(fp, "\\|");
        print_symbol_for_grapheasy(FKL_VM_SYM(sl->k), fp);
        fklStrBuilderPuts(fp, "\\|");
    }
    fklStrBuilderPuts(fp, "||\n");
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
        struct FklVMvalue *id) {
    for (; gt; gt = gt->next) {
        if (gt->nt == id)
            return gt;
    }
    return NULL;
}

void fklPrintAnalysisTableForGraphEasy(const FklGrammer *grammer, FILE *fp) {
    FklStrBuilder builder = { 0 };
    fklInitStrBuilderFp(&builder, fp, NULL);
    fklPrintAnalysisTableForGraphEasy2(grammer, &builder);
}

void fklPrintAnalysisTableForGraphEasy2(const FklGrammer *g,
        FklStrBuilder *fp) {
    size_t num = g->aTable.num;
    FklAnalysisState *states = g->aTable.states;

    fklStrBuilderPuts(fp, "graph{title:state-table;}[\n");

    GraActionMatchHashSet laTable;
    FklNontermHashSet sidSet;
    init_analysis_table_header(&laTable, &sidSet, states, num);

    print_table_header_for_grapheasy(g, &laTable, &sidSet, fp);

    GraActionMatchHashSetNode *laList = laTable.first;
    FklNontermHashSetNode *sidList = sidSet.first;
    for (size_t i = 0; i < num; i++) {
        const FklAnalysisState *curState = &states[i];
        fklStrBuilderFmt(fp, "%" PRIu64 ": |", i);
        for (GraActionMatchHashSetNode *al = laList; al; al = al->next) {
            FklAnalysisStateAction *action =
                    find_action(curState->state.action, &al->k);
            if (action) {
                switch (action->action) {
                case FKL_ANALYSIS_SHIFT: {
                    uintptr_t idx = action->state - states;
                    fklStrBuilderFmt(fp, "s%" PRIu64 "", idx);
                } break;
                case FKL_ANALYSIS_REDUCE:
                    fklStrBuilderFmt(fp, "r%" PRIu64 "", action->prod->idx);
                    break;
                case FKL_ANALYSIS_ACCEPT:
                    fklStrBuilderPuts(fp, "acc");
                    break;
                case FKL_ANALYSIS_IGNORE:
                    break;
                }
            } else
                fklStrBuilderPuts(fp, "\\n");
            fklStrBuilderPutc(fp, '|');
        }
        fklStrBuilderPuts(fp, "\\n|\\n");
        for (FklNontermHashSetNode *sl = sidList; sl; sl = sl->next) {
            fklStrBuilderPutc(fp, '|');
            FklAnalysisStateGoto *gt = find_gt(curState->state.gt, sl->k);
            if (gt) {
                uintptr_t idx = gt->state - states;
                fklStrBuilderFmt(fp, "%" PRIu64 "", idx);
            } else {
                fklStrBuilderPuts(fp, "\\n");
            }
        }
        fklStrBuilderPuts(fp, "||\n");
    }
    fklStrBuilderPutc(fp, ']');
    graActionMatchHashSetUninit(&laTable);
    fklNontermHashSetUninit(&sidSet);
}

static inline void build_get_max_non_term_length_prototype_to_c_file(
        FklStrBuilder *build) {

    SB_LINE("static inline size_t");
    SB_LINE("get_max_non_term_length(const FklGrammer*");

    SB_INDENT(flag) {
        SB_LINE(",FklGrammerMatchCtx*");
        SB_LINE(",const char*");
        SB_LINE(",const char*");
        SB_LINE(",size_t);");
    }
}

static inline void build_match_ignore_prototype_to_c_file(
        FklStrBuilder *build) {
    SB_LINE("static inline size_t match_ignore(FklGrammerMatchCtx*,const char*,size_t,int* );");
}

static inline void build_match_ignore_to_c_file(const FklGrammer *g,
        FklStrBuilder *build) {
    SB_LINE("static inline size_t match_ignore(FklGrammerMatchCtx* ctx,const char *start, size_t rest_len, int* p_is_waiting_for_more) {\n");

    SB_INDENT(flag) {
        const FklGrammerIgnore *ig = g->ignores;
        if (ig) {
            SB_LINE("ssize_t matchLen=0;");
            SB_LINE("size_t otherMatchLen=0;");
            SB_LINE("const char** in=&start;");
            SB_LINE("size_t* restLen=&rest_len;");
            SB_LINE("int is_waiting_for_more=0;");
            SB_LINE("(void)is_waiting_for_more;");
            SB_LINE("(void)restLen;");

            SB_LINE("for(;rest_len>otherMatchLen;){");
            SB_INDENT(flag) {
                SB_LINE_START("if(");
                uint64_t number = 0;
                ignore_print_c_match_cond(number, ig, g, build);
                SB_INDENT(flag) {
                    ++number;
                    for (ig = ig->next; ig; ig = ig->next, ++number) {
                        SB_LINE_END("");
                        SB_LINE_START("||");
                        ignore_print_c_match_cond(number, ig, g, build);
                    }
                }
                SB_LINE_END(")");
                SB_LINE("{");
                SB_INDENT(flag) { SB_LINE("otherMatchLen+=matchLen;"); }
                SB_LINE("}");

                SB_LINE("else");
                SB_INDENT(flag) { SB_LINE("break;"); }
            }
            SB_LINE("}");

            SB_LINE("*p_is_waiting_for_more|=is_waiting_for_more;");
            SB_LINE("return otherMatchLen;");
        } else {
            SB_LINE("return 0;");
        }
    }

    SB_LINE("}");
}

static inline void build_get_max_non_term_length_to_c_file(const FklGrammer *g,
        FklStrBuilder *build) {
    SB_LINE("static inline size_t");
    SB_LINE("get_max_non_term_length(const FklGrammer* g");
    SB_INDENT(flag) {
        SB_LINE(",FklGrammerMatchCtx* ctx");
        SB_LINE(",const char* start");
        SB_LINE(",const char* cur");
        SB_LINE(",size_t rLen) {");
    }
    SB_INDENT(flag) {
        SB_LINE("if(rLen) {");
        SB_INDENT(flag) {
            SB_LINE("if(start==ctx->start&&cur==ctx->cur) return ctx->maxNonterminalLen;");
            SB_LINE("ctx->start=start;");
            SB_LINE("ctx->cur=cur;");
            SB_LINE("size_t len=0;");
            SB_LINE("ssize_t matchLen=0;");
            SB_LINE("size_t otherMatchLen=0;");
            SB_LINE("size_t* restLen=&rLen;");
            SB_LINE("const char** in=&cur;");
            SB_LINE("int is_waiting_for_more=0;");
            SB_LINE("(void)is_waiting_for_more;");
            SB_LINE("(void)otherMatchLen;");
            SB_LINE("(void)restLen;");
            SB_LINE("(void)in;");
            SB_LINE("while(rLen) {");
            SB_INDENT(flag) {
                SB_LINE_START("if(");
                if (g->ignores) {
                    const FklGrammerIgnore *igns = g->ignores;
                    uint64_t number = 0;
                    ignore_print_c_match_cond(number, igns, g, build);
                    igns = igns->next;
                    for (++number; igns; igns = igns->next, ++number) {
                        SB_LINE_END("");
                        SB_LINE_START("||");
                        ignore_print_c_match_cond(number, igns, g, build);
                    }
                }
                if (g->ignores && g->sorted_delimiters_num) {
                    SB_LINE_END("");
                    SB_LINE_START("||");
                }
                if (g->sorted_delimiters_num) {
                    size_t num = g->sorted_delimiters_num;
                    const FklString **terminals = g->sorted_delimiters;
                    const FklString *cur = terminals[0];
                    SB_LINE("(matchLen=fklCharBufMatch(\"");
                    build_string_in_hex(cur, build);
                    SB_LINE("\",%" PRIu64
                            ",*in+otherMatchLen,*restLen-otherMatchLen))>=0",
                            cur->size);
                    for (size_t i = 1; i < num; i++) {
                        SB_LINE_END("");
                        SB_LINE_START("||");
                        const FklString *cur = terminals[i];
                        SB_LINE("(matchLen=fklCharBufMatch(\"");
                        build_string_in_hex(cur, build);
                        SB_LINE("\",%" PRIu64
                                ",*in+otherMatchLen,*restLen-otherMatchLen))>=0",
                                cur->size);
                    }
                }
                SB_LINE_END(") break;");
                SB_LINE("len++;");
                SB_LINE("rLen--;");
                SB_LINE("cur++;");
            }
            SB_LINE("}");
            SB_LINE("ctx->maxNonterminalLen=len;");
            SB_LINE("return len;");
        }
        SB_LINE("}");
        SB_LINE("return 0;");
    }
    SB_LINE("}");
}

static inline void build_match_char_buf_end_with_terminal_prototype_to_c_file(
        FklStrBuilder *build) {
    SB_LINE("static inline size_t");
    SB_LINE("match_char_buf_end_with_terminal(const char*,");
    SB_INDENT(flag) {
        SB_LINE("size_t,");
        SB_LINE("const char*,");
        SB_LINE("size_t,");
        SB_LINE("FklGrammerMatchCtx* ctx,");
        SB_LINE("const char* start);");
    }
}

static inline void build_match_char_buf_end_with_terminal_to_c_file(
        FklStrBuilder *build) {
    SB_LINE("static inline size_t");
    SB_LINE("match_char_buf_end_with_terminal(const char* pattern");
    SB_INDENT(flag) {
        SB_LINE(",size_t pattern_size");
        SB_LINE(",const char* cstr");
        SB_LINE(",size_t restLen");
        SB_LINE(",FklGrammerMatchCtx* ctx");
        SB_LINE(",const char* start)");
    }

    SB_LINE("{");
    SB_INDENT(flag) {
        SB_LINE("size_t maxNonterminalLen=get_max_non_term_length(NULL,ctx,start,cstr,restLen);");
        SB_LINE("ssize_t matchLen=fklCharBufMatch(pattern,pattern_size,cstr,restLen);");
        SB_LINE("return matchLen>=0 && maxNonterminalLen==(size_t)matchLen;");
    }
    SB_LINE("}");
}

// composite-terminal unordered set
// GraCompHashMap
#define FKL_HASH_TYPE_PREFIX Gra
#define FKL_HASH_METHOD_PREFIX gra
#define FKL_HASH_KEY_TYPE FklCompositeSym
#define FKL_HASH_VAL_TYPE uint64_t
#define FKL_HASH_ELM_NAME Comp
#define FKL_HASH_KEY_EQUAL(A, B) fklCompositeGrammerSymEqual(A, B)
#define FKL_HASH_KEY_HASH return fklCompositeGrammerSymHash(pk);
#include <fakeLisp/cont/hash.h>

static inline uint64_t get_or_add_composite_entry(GraCompHashMap *comps,
        const FklCompositeSym *c) {
    GraCompHashMapElm *id = graCompHashMapInsert2(comps, *c, comps->count + 1);
    return id->v;
}

static inline uint64_t get_composite_entry(const GraCompHashMap *comps,
        const FklCompositeSym *c) {
    uint64_t *id = graCompHashMapGet(comps, c);
    if (id == NULL)
        return 0;
    return *id;
}

static inline void build_builtin_term_match_cond(
        const FklLalrBuiltinGrammerSym *b,
        const FklGrammer *g,
        FklStrBuilder *build) {
    const FklLalrBuiltinMatch *t = b->t;
    FKL_ASSERT(t->key != NULL);
    FKL_ASSERT(t->max_args >= 0);

    SB_FMT("%s(NULL,start,*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len,&matchLen,ctx,&is_waiting_for_more",
            t->key);

    int last_args_idx = -1;
    for (int i = 0; i < t->max_args; ++i) {
        if ((unsigned)i < b->len) {
            last_args_idx = i;
        }

        SB_INDENT(flags) {
            if (last_args_idx == -1) {
                SB_FMT(",NULL,0");
            } else {
                const FklString *cur = b->args[last_args_idx];
                SB_FMT(",\"");
                build_string_in_hex(cur, build);
                SB_FMT("\"");
                SB_FMT(",%" PRIu64 "", cur->size);
            }
        }
    }

    SB_FMT(")");
}

static inline void build_state_action_match_to_c_file(const FklGrammer *g,
        const FklAnalysisStateAction *ac,
        GraCompHashMap *comps,
        FklStrBuilder *build) {
    switch (ac->match.t) {
    case FKL_TERM_KEYWORD:
        SB_FMT("(matchLen=match_char_buf_end_with_terminal(\"");
        build_string_in_hex(ac->match.str, build);
        SB_FMT("\",%" PRIu64
               ",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len,ctx,start))",
                ac->match.str->size);
        break;
    case FKL_TERM_STRING:
        SB_FMT("(matchLen=fklCharBufMatch(\"");
        build_string_in_hex(ac->match.str, build);
        SB_FMT("\",%" PRIu64
               ",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len))>=0",
                ac->match.str->size);
        break;
    case FKL_TERM_REGEX: {
        uint64_t num = 0;
        fklGetStringWithRegex(&g->regexes, ac->match.re, &num);
        SB_FMT("regex_lex_match_for_parser_in_c((const FklRegexCode*)&");
        SB_FMT(PRINT_C_REGEX_PREFIX "%" PRIX64, num);
        SB_FMT(",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len,&matchLen,&is_waiting_for_more)");
    } break;
    case FKL_TERM_BUILTIN:
        build_builtin_term_match_cond(&ac->match.func, g, build);
        break;
    case FKL_TERM_EOF:
        SB_FMT("(matchLen=1)");
        break;
    case FKL_TERM_IGNORE:
        SB_FMT("(!is_waiting_for_more&&(matchLen=match_ignore(ctx,*in+otherMatchLen,*restLen-otherMatchLen,&is_waiting_for_more)))");
        break;
    case FKL_TERM_COMP: {
        uint64_t id = get_composite_entry(comps, &ac->match.comp);
        FKL_ASSERT(id > 0);
        SB_FMT("(matchLen=match_composite_%" PRIu64
               "(start,*in+otherMatchLen+skip_ignore_len,*restLen-"
               "otherMatchLen-skip_ignore_len,ctx,&is_waiting_for_more,NULL,0))>=0",
                id);
    } break;
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
        const GraCompHashMap *comps,
        FklStrBuilder *build) {
    SB_LINE("{");
    SB_INDENT(flag) {
        SB_LINE("int is_waiting_for_more = 0;");
        SB_LINE("(void)is_waiting_for_more;");
        switch (ac->action) {
        case FKL_ANALYSIS_SHIFT:
            if (ac->match.t != FKL_TERM_COMP) {
                SB_LINE("fklParseStateVectorPushBack2(stateStack,(FklParseState){.func=state_%" PRIu64
                        "});",
                        ac->state - states);
                SB_LINE("init_term_analyzing_symbol(fklAnalysisSymbolVectorPushBack(symbols,NULL)");
                SB_INDENT(flag) {
                    SB_LINE(",*in+skip_ignore_len");
                    SB_LINE(",matchLen");
                    SB_LINE(",ctx->line");
                    SB_LINE(",skip_ignore_len>0");
                    SB_LINE(",ctx->ctx);");
                }
            } else {
                for (size_t i = 0; i < ac->match.comp.len; ++i) {
                    SB_LINE("fklParseStateVectorPushBack2(stateStack,(FklParseState){.func=state_%" PRIu64
                            "});",
                            ac->state - states);
                }

                uint64_t id = get_composite_entry(comps, &ac->match.comp);
                SB_LINE("match_composite_%" PRIu64 "(start", id);
                SB_INDENT(flag) {
                    SB_LINE(",*in+skip_ignore_len");
                    SB_LINE(",matchLen");
                    SB_LINE(",ctx");
                    SB_LINE(",&is_waiting_for_more");
                    SB_LINE(",symbols");
                    SB_LINE(",skip_ignore_len>0);");
                }
            }

            SB_LINE("ctx->line+=fklCountCharInBuf(*in,matchLen+skip_ignore_len,'\\n');");
            SB_LINE("*in+=matchLen+skip_ignore_len;");
            SB_LINE("*restLen-=matchLen+skip_ignore_len;");
            break;
        case FKL_ANALYSIS_ACCEPT:
            SB_LINE("*accept=1;");
            break;
        case FKL_ANALYSIS_REDUCE: {

            size_t actual_len =
                    fklComputeProdActualLen(ac->prod->len, ac->prod->syms);

            if (actual_len) {
                SB_LINE("size_t line=fklGetFirstNthLine(symbols,%" PRIu64
                        ",ctx->line);",
                        actual_len);
            } else {
                SB_LINE("size_t line=ctx->line;");
            }

            SB_LINE("stateStack->size-=%" PRIu64 ";", actual_len);
            SB_LINE("symbols->size-=%" PRIu64 ";", actual_len);
            SB_LINE("FklAnalysisSymbol* base=&symbols->base[symbols->size];");

            SB_LINE("FklStateFuncPtr func=fklParseStateVectorBackNonNull(stateStack)->func;");
            SB_LINE("FklParseState nextState={.func=NULL};");
            SB_LINE("func(NULL,NULL,0,%s,FKL_MAKE_VM_FIX(%" PRIu32
                    "),&nextState,NULL,NULL,NULL,NULL,NULL,NULL);",
                    actual_len ? "base[0].start_with_ignore" : "0",
                    fklValueTableAdd(t, ac->prod->left));
            SB_LINE("if(nextState.func == NULL) return FKL_PARSE_REDUCE_FAILED;");
            SB_LINE("fklParseStateVectorPushBack(stateStack,&nextState);");

            SB_LINE("void* ast=prod_action_%s(NULL,ctx->ctx,base,%" PRIu64
                    ",line);\n",
                    ac->prod->print_name,
                    actual_len);

            if (actual_len) {
                SB_LINE("for(size_t i=0;i<%" PRIu64 ";i++) %s(base[i].ast);",
                        actual_len,
                        ast_destroyer_name);
            }
            SB_LINE("if(!ast) {");
            SB_INDENT(flag) {
                SB_LINE("*output_line=line;");
                SB_LINE("return FKL_PARSE_REDUCE_FAILED;");
            }
            SB_LINE("}");

            SB_LINE("fklInitNontermAnalysisSymbol(fklAnalysisSymbolVectorPushBack(symbols,NULL),FKL_MAKE_VM_FIX(%" PRIu32
                    "),ast,%s,line);",
                    fklValueTableAdd(t, ac->prod->left),
                    actual_len ? "base[0].start_with_ignore" : "0");
        } break;
        case FKL_ANALYSIS_IGNORE:
            SB_LINE("ctx->line+=fklCountCharInBuf(*in,matchLen,'\\n');");
            SB_LINE("*in+=matchLen;");
            SB_LINE("*restLen-=matchLen;");
            SB_LINE("goto action_match_start;");
            break;
        }
        SB_LINE("return 0;");
    }
    SB_LINE("}");
}

static inline void build_state_prototype_to_c_file(
        const FklAnalysisState *states,
        size_t idx,
        FklStrBuilder *build) {
    SB_LINE_START("static int state_%" PRIu64 "(FklParseStateVector*", idx);
    SB_LINE(",FklAnalysisSymbolVector*");
    SB_LINE(",int");
    SB_LINE(",uint8_t");
    SB_LINE(",struct FklVMvalue*");
    SB_LINE(",FklParseState* pfunc");
    SB_LINE(",const char*");
    SB_LINE(",const char**");
    SB_LINE(",size_t*");
    SB_LINE(",FklGrammerMatchCtx*");
    SB_LINE(",int*");
    SB_LINE_END(",size_t*);");
}

static inline void build_state_to_c_file(FklValueTable *t,
        GraCompHashMap *comps,
        const FklAnalysisState *states,
        size_t idx,
        const FklGrammer *g,
        const char *ast_destroyer_name,
        FklStrBuilder *build) {
    const FklAnalysisState *state = &states[idx];
    SB_LINE("static int state_%" PRIu64 "(FklParseStateVector* stateStack",
            idx);
    SB_INDENT(flag) {
        SB_LINE(",FklAnalysisSymbolVector* symbols");
        SB_LINE(",int is_action");
        SB_LINE(",uint8_t start_with_ignore");
        SB_LINE(",struct FklVMvalue* left");
        SB_LINE(",FklParseState* pfunc");
        SB_LINE(",const char* start");
        SB_LINE(",const char** in");
        SB_LINE(",size_t* restLen");
        SB_LINE(",FklGrammerMatchCtx* ctx");
        SB_LINE(",int* accept");
        SB_LINE(",size_t* output_line) {");
    }

    SB_INDENT(flag) {
        SB_LINE("if(is_action){");
        SB_INDENT(flag) {
            SB_LINE("int is_waiting_for_more=0;");
            SB_LINE("(void)is_waiting_for_more;");
            for (const FklAnalysisStateAction *ac = state->state.action; ac;
                    ac = ac->next)
                if (ac->action == FKL_ANALYSIS_IGNORE) {
                    SB_FMT("action_match_start:;\n");
                    break;
                }
            SB_LINE("int has_tried_match_ignore;");
            SB_LINE("ssize_t matchLen=0;");
            SB_LINE("ssize_t ignore_len=-1;");
            SB_LINE("size_t skip_ignore_len;");
            SB_LINE("size_t otherMatchLen=0;");
            SB_LINE("(void)ignore_len;");
            SB_LINE("(void)has_tried_match_ignore;");
            SB_LINE("(void)skip_ignore_len;");
            SB_LINE("");
            const FklAnalysisStateAction *ac = state->state.action;
            if (ac) {
                uint32_t allow_ignore_label_count = 0;
                for (; ac; ac = ac->next) {
                    SB_LINE("skip_ignore_len=0;");
                    SB_LINE("has_tried_match_ignore=0;");
                    uint32_t cur_allow_ignore_label_num = 0;
                    if (ac->match.allow_ignore
                            && ac->action != FKL_ANALYSIS_IGNORE) {
                        cur_allow_ignore_label_num = allow_ignore_label_count++;
                        SB_FMT("allow_ignore_label%u:\n",
                                cur_allow_ignore_label_num);
                    }
                    SB_LINE_START("if(");
                    build_state_action_match_to_c_file(g, ac, comps, build);
                    SB_LINE_END(")");
                    build_state_action_to_c_file(t,
                            ac,
                            states,
                            ast_destroyer_name,
                            comps,
                            build);
                    SB_LINE("else if(is_waiting_for_more)");
                    SB_INDENT(flags) {
                        SB_LINE("goto return_is_waiting_for_more;");
                    }
                    if (ac->match.allow_ignore
                            && ac->action != FKL_ANALYSIS_IGNORE) {
                        SB_LINE("else if(!has_tried_match_ignore && ((ignore_len==-1 ");
                        SB_INDENT(flag) {
                            SB_LINE("&& (ignore_len=match_ignore(ctx,*in+otherMatchLen,*restLen-otherMatchLen,&is_waiting_for_more))>0)");
                            SB_LINE_START("|| ignore_len>0)");
                        }
                        SB_LINE_END(") {");
                        SB_INDENT(flag) {
                            SB_LINE("has_tried_match_ignore=1;");
                            SB_LINE("skip_ignore_len=(size_t)ignore_len;");
                            SB_LINE("goto allow_ignore_label%u;",
                                    cur_allow_ignore_label_num);
                        }
                        SB_LINE("}");
                        SB_LINE("");
                    }
                }
                SB_FMT("return_is_waiting_for_more:\n");
                SB_LINE("return (is_waiting_for_more||(*restLen && *restLen==skip_ignore_len))?FKL_PARSE_WAITING_FOR_MORE:FKL_PARSE_TERMINAL_MATCH_FAILED;");
            } else
                SB_LINE("return FKL_PARSE_TERMINAL_MATCH_FAILED;");
            SB_LINE("(void)otherMatchLen;");
        }
        SB_LINE("}else{");
        SB_INDENT(flag) {
            const FklAnalysisStateGoto *gt = state->state.gt;
            if (gt) {
                SB_LINE("if(0){}");
                for (; gt; gt = gt->next) {
                    if (!gt->allow_ignore) {
                        SB_LINE("else if(!start_with_ignore&&left==FKL_MAKE_VM_FIX(%" PRIu32
                                ")/* %s */){",
                                fklValueTableAdd(t, gt->nt),
                                FKL_VM_SYM(gt->nt)->str);
                    } else {
                        SB_LINE("else if(left==FKL_MAKE_VM_FIX(%" PRIu32
                                ")/* %s */){",
                                fklValueTableAdd(t, gt->nt),
                                FKL_VM_SYM(gt->nt)->str);
                    }
                    SB_INDENT(flag) {
                        SB_LINE("pfunc->func=state_%" PRIu64 ";",
                                gt->state - states);
                        SB_LINE("return 0;");
                    }
                    SB_LINE("}");
                }
                SB_LINE("else return FKL_PARSE_REDUCE_FAILED;");
            } else
                SB_LINE("return FKL_PARSE_REDUCE_FAILED;");
        }
        SB_LINE("}");
        SB_LINE("return 0;");
    }
    SB_LINE("}");
    SB_LINE("");
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
                ac = ac->next) {
            if (ac->match.t == FKL_TERM_BUILTIN) {
                graBtmHashSetPut2(ptrSet, ac->match.func.t);
            } else if (ac->match.t == FKL_TERM_COMP) {
                const FklCompositeSym *c = &ac->match.comp;
                for (size_t i = 0; i < c->len; ++i) {
                    const FklGrammerSym *p = &c->parts[i];
                    if (p->type == FKL_TERM_BUILTIN)
                        graBtmHashSetPut2(ptrSet, p->b.t);
                }
            }
        }
    }
}

FKL_NODISCARD
static inline int build_builtin_term_args(const FklStringVector *lines,
        FklStrBuilder *build) {
    for (size_t i = 0; i < lines->size; ++i) {
        const FklString *cur = lines->base[i];
        const char *pos = fklStrstr(cur->str, "FKL_BUILTIN_TERMINAL_ARG(");
        if (pos == NULL)
            continue;

        size_t name_len = 0;
        const char *name_pos = pos + strlen("FKL_BUILTIN_TERMINAL_ARG(");
        const char *name_cur = name_pos;
        for (; *name_cur != '\0' && *name_cur != ')'; ++name_cur) {
            name_len++;
        }

        if (name_len == 0) {
            return -1;
        }

        SB_LINE_START(",const char* ");
        fklStrBuilderWrite(build, name_len, name_pos);
        SB_LINE_END("");
        SB_LINE_START(",size_t ");
        fklStrBuilderWrite(build, name_len, name_pos);
        SB_LINE_END("_size");
    }

    return 0;
}

FKL_NODISCARD
static inline int build_builtin_term_lines(const FklStrView *name,
        const FklStringVector *lines,
        FklStrBuilder *build) {
    SB_LINE_START("static int ");
    fklStrBuilderWrite(build, name->len, name->str);
    SB_LINE_END("(const FklGrammer* g");

    SB_INDENT(flags) {
        SB_LINE(",const char* cstrStart");
        SB_LINE(",const char* cstr");
        SB_LINE(",size_t restLen");
        SB_LINE(",ssize_t* pmatchLen");
        SB_LINE(",FklGrammerMatchCtx* ctx");
        SB_LINE(",int* is_waiting_for_more");
        int r = build_builtin_term_args(lines, build);
        SB_LINE(")");
        if (r < 0) {
            return -1;
        }
    }

    SB_LINE("{");
    SB_INDENT(flags) { SB_LINE("FKL_ASSERT(g == NULL);"); }

    for (size_t i = 0; i < lines->size; ++i) {
        const FklString *cur = lines->base[i];
        fklStrBuilderWrite(build, cur->size, cur->str);
    }

    SB_LINE("}");

    return 0;
}

FKL_NODISCARD
static inline int build_all_builtin_match_func(const FklGrammer *g,
        const FklBuiltinTermSrcHashMap *maps,
        FklStrBuilder *build,
        FklStrView *err) {
    int r = 0;
    GraBtmHashSet builtin_match_method_table_set;
    graBtmHashSetInit(&builtin_match_method_table_set);
    get_all_match_method_table(g, &builtin_match_method_table_set);
    for (GraBtmHashSetNode *il = builtin_match_method_table_set.first; il;
            il = il->next) {
        const FklLalrBuiltinMatch *t = il->k;
        if (t->key == NULL) {
            err->str = t->name;
            err->len = strlen(t->name);
            r = -1;
            break;
        }

        FklStrView key = {
            .str = t->key,
            .len = strlen(t->key),
        };

        const FklStringVector *lines = fklBuiltinTermSrcHashMapGet(maps, &key);
        if (lines == NULL) {
            *err = key;
            r = -1;
            break;
        }

        r = build_builtin_term_lines(&key, lines, build);
        if (r < 0) {
            *err = key;
            break;
        }
        SB_LINE("");
    }

    graBtmHashSetUninit(&builtin_match_method_table_set);

    return r;
}

static void build_composite(const FklGrammer *g,
        uint64_t id,
        const FklCompositeSym *c,
        FklStrBuilder *build) {
    const FklGrammerSym *parts = c->parts;
    size_t len = c->len;
    SB_LINE("static ssize_t match_composite_%" PRIu64 "(const char* start", id);
    SB_INDENT(flag) {
        SB_LINE(",const char* cstr");
        SB_LINE(",size_t rest_len");
        SB_LINE(",FklGrammerMatchCtx* ctx");
        SB_LINE(",int* p_is_waiting_for_more");
        SB_LINE(",FklAnalysisSymbolVector *symbols");
        SB_LINE(",uint8_t start_with_ignore)");
    }
    SB_LINE("{");
    SB_INDENT(flag) {
        SB_LINE("size_t total=0;");
        SB_LINE("const char** in=&cstr;");
        SB_LINE("size_t* restLen=&rest_len;");
        SB_LINE("size_t otherMatchLen=0;");
        SB_LINE("ssize_t matchLen=0;");
        SB_LINE("int is_waiting_for_more=0;");
        SB_LINE("size_t const skip_ignore_len = 0;");
        SB_LINE("size_t line = ctx->line;");
        SB_LINE("(void)skip_ignore_len;");
        SB_LINE("(void)line;");
        for (size_t k = 0; k < len; k++) {
            const FklGrammerSym *p = &parts[k];
            switch (p->type) {
            case FKL_TERM_STRING:
                SB_LINE_START("if((matchLen=fklCharBufMatch(\"");
                build_string_in_hex(p->str, build);
                SB_LINE_END("\",%" PRIu64 ",*in+otherMatchLen,*restLen-"
                            "otherMatchLen))<0) goto fail;",
                        p->str->size);
                break;
            case FKL_TERM_REGEX: {
                uint64_t renum = 0;
                fklGetStringWithRegex(&g->regexes, p->re, &renum);
                SB_LINE("if(!regex_lex_match_for_parser_in_c((const "
                        "FklRegexCode*)&" PRINT_C_REGEX_PREFIX "%" PRIX64
                        ",*in+otherMatchLen,*restLen-"
                        "otherMatchLen,&matchLen,"
                        "&is_waiting_for_more)) goto fail;",
                        renum);
            } break;
            case FKL_TERM_BUILTIN: {
                SB_LINE_START("if(!");
                build_builtin_term_match_cond(&p->b, g, build);
                SB_LINE_END(") goto fail;");
            } break;
            default:
                FKL_UNREACHABLE();
                break;
            }

            SB_LINE("if(symbols == NULL)");
            SB_LINE("{");
            SB_INDENT(flags) { SB_LINE("total+=matchLen;"); }
            SB_LINE("} else {");

            SB_INDENT(flags) {
                SB_LINE("init_term_analyzing_symbol(fklAnalysisSymbolVectorPushBack(symbols,NULL)");
                SB_INDENT(flag) {
                    SB_LINE(",cstr+otherMatchLen");
                    SB_LINE(",matchLen");
                    SB_LINE(",line");
                    if (k > 0) {
                        SB_LINE(",0");
                    } else {
                        SB_LINE(",start_with_ignore");
                    }
                    SB_LINE(",ctx->ctx);");
                    SB_LINE("line += fklCountCharInBuf(cstr+otherMatchLen, matchLen, '\\n');");
                }
            }

            SB_LINE("}");

            SB_LINE("otherMatchLen+=matchLen;");
        }
        SB_LINE("*p_is_waiting_for_more|=is_waiting_for_more;");
        SB_LINE("return (ssize_t)total;");
        fklStrBuilderUnindent(build);
        SB_LINE("fail:");
        fklStrBuilderIndent(build);
        SB_LINE("if(symbols != NULL)");
        SB_INDENT(flag) { SB_LINE("FKL_UNREACHABLE();"); }

        SB_LINE("*p_is_waiting_for_more|=is_waiting_for_more;");
        SB_LINE("return -1;");
    }
    SB_LINE("}");
    SB_LINE("");
}

static void build_all_composites(const FklGrammer *g,
        GraCompHashMap *comps,
        FklStrBuilder *build) {
    const FklAnalysisState *states = g->aTable.states;
    size_t num = g->aTable.num;
    for (size_t i = 0; i < num; ++i) {
        for (const FklAnalysisStateAction *ac = states[i].state.action; ac;
                ac = ac->next) {
            if (ac->match.t == FKL_TERM_COMP) {
                get_or_add_composite_entry(comps, &ac->match.comp);
            }
        }
    }

    for (const GraCompHashMapNode *cur = comps->first; cur; cur = cur->next) {
        build_composite(g, cur->v, &cur->k, build);
    }
}

static inline void build_all_regex(const FklRegexTable *rt,
        FklStrBuilder *build) {
    for (const FklStrRegexHashMapNode *l = rt->str_re.first; l; l = l->next) {
        SB_LINE("static const ");
        fklRegexBuildAsCwithNum(l->v.re, PRINT_C_REGEX_PREFIX, l->v.num, build);
        SB_LINE("");
    }
}

static inline void build_regex_lex_match_for_parser_in_c_to_c_file(
        FklStrBuilder *build) {
    SB_LINE("static inline int");
    SB_LINE("regex_lex_match_for_parser_in_c(const FklRegexCode* re,");
    SB_INDENT(flag) {
        SB_LINE("const char* cstr,");
        SB_LINE("size_t restLen,");
        SB_LINE("ssize_t* matchLen,");
        SB_LINE("int* is_waiting_for_more)");
    }
    SB_LINE("{");
    SB_INDENT(flag) {
        SB_LINE("int last_is_true=0;");
        SB_LINE("size_t len=fklRegexLexMatchp(re,cstr,restLen,&last_is_true);");
        SB_LINE("if(len>restLen) {");
        SB_INDENT(flag) {
            SB_LINE("*is_waiting_for_more|=last_is_true;");
            SB_LINE("return 0;");
        }
        SB_LINE("}");
        SB_LINE("*matchLen=len;");
        SB_LINE("return 1;");
    }
    SB_LINE("}");
}

static inline void build_init_term_analyzing_symbol_src(FklStrBuilder *build,
        const char *name) {
    SB_LINE("static inline void");
    SB_LINE("init_term_analyzing_symbol(FklAnalysisSymbol* sym,");
    SB_INDENT(flag) {
        SB_LINE("const char* s,");
        SB_LINE("size_t len,");
        SB_LINE("size_t line,");
        SB_LINE("uint8_t start_with_ignore,");
        SB_LINE("void* ctx)");
    }
    SB_LINE("{");
    SB_INDENT(flag) {
        SB_LINE("void* ast=%s(s,len,line,ctx);", name);
        SB_LINE("sym->nt=NULL;");
        SB_LINE("sym->ast=ast;");
        SB_LINE("sym->start_with_ignore=start_with_ignore;");
        SB_LINE("sym->line=line;");
    }
    SB_LINE("}");
    SB_LINE("");
}

static inline void build_ignore_sym_match_to_c_file(
        const FklGrammerIgnoreSym *sym,
        const FklGrammer *g,
        FklStrBuilder *build) {
    switch (sym->term_type) {
    case FKL_TERM_STRING:
        SB_FMT("(matchLen=fklCharBufMatch(\"");
        build_string_in_hex(sym->str, build);
        SB_FMT("\",%" PRIu64
               ",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len))>=0",
                sym->str->size);
        break;
    case FKL_TERM_REGEX: {
        uint64_t num = 0;
        fklGetStringWithRegex(&g->regexes, sym->re, &num);
        SB_FMT("regex_lex_match_for_parser_in_c((const FklRegexCode*)&");
        SB_FMT(PRINT_C_REGEX_PREFIX "%" PRIX64, num);
        SB_FMT(",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len,&matchLen,&is_waiting_for_more)");
    } break;
    case FKL_TERM_BUILTIN:
        build_builtin_term_match_cond(&sym->b, g, build);
        break;

    case FKL_TERM_KEYWORD:
    case FKL_TERM_EOF:
    case FKL_TERM_NONE:
    case FKL_TERM_IGNORE:
    case FKL_TERM_NONTERM:
    case FKL_TERM_COMP:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void build_ignore(uint64_t number,
        const FklGrammerIgnore *ig,
        const FklGrammer *g,
        FklStrBuilder *build) {
    SB_LINE("static inline int match_ignore_%" PRIu64 "(const char* start",
            number);
    SB_INDENT(flag) {
        SB_LINE(",const char* cstr");
        SB_LINE(",size_t restLen_");
        SB_LINE(",ssize_t* pmatchLen");
        SB_LINE(",int* pis_waiting_for_more");
        SB_LINE(",FklGrammerMatchCtx* ctx)");
    }
    SB_LINE("{");
    SB_INDENT(flag) {
        SB_LINE("int is_waiting_for_more=0;");
        SB_LINE("if(restLen_) {");
        if (ig->len == 0) {
            SB_LINE("*pmatchLen=0;");
            SB_LINE("*pis_waiting_for_more=is_waiting_for_more;");
            SB_LINE("return 1;");
        }

        SB_INDENT(flag) {
            SB_LINE("size_t otherMatchLen=0;");
            SB_LINE("const char** in=&cstr;");
            SB_LINE("ssize_t matchLen=0;");
            SB_LINE("size_t skip_ignore_len=0;");
            SB_LINE("size_t* restLen=&restLen_;");
            for (size_t i = 0; i < ig->len; ++i) {
                SB_LINE_START("if(");
                build_ignore_sym_match_to_c_file(&ig->ig[i], g, build);
                SB_LINE_END(") {");
                SB_INDENT(flag) { SB_LINE("otherMatchLen+=matchLen;"); }
                SB_LINE("} else {");
                SB_INDENT(flag) {
                    SB_LINE("*pis_waiting_for_more=is_waiting_for_more;");
                    SB_LINE("return 0;");
                }
                SB_LINE("}");
                SB_LINE("");
            }

            SB_LINE("*pmatchLen=otherMatchLen;");
            SB_LINE("*pis_waiting_for_more=is_waiting_for_more;");
            SB_LINE("return 1;");
        }
        SB_LINE("}");
        SB_LINE("*pis_waiting_for_more=is_waiting_for_more;");
        SB_LINE("return 0;");
    }
    SB_LINE("}");
}

static inline void build_all_ignores(const FklGrammer *g,
        FklStrBuilder *build) {
    uint64_t number = 0;
    for (const FklGrammerIgnore *ig = g->ignores; ig; ig = ig->next, ++number) {
        build_ignore(number, ig, g, build);
        SB_LINE("");
    }
}

int fklPrintAnalysisTableAsCfunc(const FklGrammer *g,
        FILE *action_src_fp,
        const char *ast_creator_name,
        const char *ast_destroyer_name,
        const char *state_0_push_func_name,
        const FklBuiltinTermSrcHashMap *maps,
        FILE *fp) {
    FklStrBuilder builder;
    fklInitStrBuilderFp(&builder, fp, NULL);

    FklStrBuilder *const build = &builder;

    SB_LINE("// Do not edit!");
    SB_LINE("");
    SB_LINE("#include <fakeLisp/grammer.h>");
    SB_LINE("#include <fakeLisp/utils.h>");

#define BUFFER_SIZE (512)
    char buffer[BUFFER_SIZE];
    size_t size = 0;
    while ((size = fread(buffer, 1, BUFFER_SIZE, action_src_fp)))
        fwrite(buffer, size, 1, fp);
#undef BUFFER_SIZE
    SB_LINE("");

    SB_LINE("");
    SB_LINE("");
    build_init_term_analyzing_symbol_src(build, ast_creator_name);

    build_match_ignore_prototype_to_c_file(build);

    SB_LINE("");

    if (g->sorted_delimiters
            || g->sorted_delimiters_num != g->terminals.count) {
        build_get_max_non_term_length_prototype_to_c_file(build);
        SB_LINE("");
    }

    if (g->regexes.num) {
        build_all_regex(&g->regexes, build);
        build_regex_lex_match_for_parser_in_c_to_c_file(build);
        SB_LINE("");
    }

    if (g->sorted_delimiters_num != g->terminals.count) {
        build_match_char_buf_end_with_terminal_prototype_to_c_file(build);
        SB_LINE("");
    }

    FklStrView err = { 0 };
    int r = build_all_builtin_match_func(g, maps, build, &err);
    if (r < 0) {
        fprintf(stderr,
                "%s: failed to build builtin terminal lines\n",
                err.str);
        return -1;
    }

    build_all_ignores(g, build);

    size_t stateNum = g->aTable.num;
    const FklAnalysisState *states = g->aTable.states;
    if (g->sorted_delimiters
            || g->sorted_delimiters_num != g->terminals.count) {
        build_get_max_non_term_length_to_c_file(g, build);
        SB_LINE("");
    }

    if (g->sorted_delimiters_num != g->terminals.count) {
        build_match_char_buf_end_with_terminal_to_c_file(build);
        SB_LINE("");
    }

    build_match_ignore_to_c_file(g, build);

    GraCompHashMap comps;
    graCompHashMapInit(&comps);

    build_all_composites(g, &comps, build);
    SB_LINE("");

    for (size_t i = 0; i < stateNum; i++)
        build_state_prototype_to_c_file(states, i, build);
    SB_LINE("");

    FklValueTable t;
    fklInitValueTable(&t);

    for (size_t i = 0; i < stateNum; i++)
        build_state_to_c_file(&t,
                &comps,
                states,
                i,
                g,
                ast_destroyer_name,
                build);
    fklUninitValueTable(&t);
    graCompHashMapUninit(&comps);

    SB_LINE("void %s(FklParseStateVector* "
            "stateStack){",
            state_0_push_func_name);
    SB_INDENT(flag) {
        SB_LINE("fklParseStateVectorPushBack2(stateStack,(FklParseState){.func=state_0});");
    }
    SB_LINE("}");

    return 0;
}

void fklPrintItemStateSet(FklVM *vm,
        const FklLalrItemSetHashMap *i,
        const FklGrammer *g,
        FILE *fp) {
    FklStrBuilder builder = { 0 };
    fklInitStrBuilderFp(&builder, fp, NULL);
    fklPrintItemStateSet2(vm, i, g, &builder);
}

void fklPrintItemStateSet2(FklVM *vm,
        const FklLalrItemSetHashMap *i,
        const FklGrammer *g,
        FklStrBuilder *fp) {
    GraItemStateIdxHashMap idxTable;
    graItemStateIdxHashMapInit(&idxTable);
    size_t idx = 0;
    for (const FklLalrItemSetHashMapNode *l = i->first; l; l = l->next, idx++) {
        graItemStateIdxHashMapPut2(&idxTable, &l->elm, idx);
    }
    for (const FklLalrItemSetHashMapNode *l = i->first; l; l = l->next) {
        const FklLalrItemHashSet *i = &l->k;
        idx = *graItemStateIdxHashMapGet2NonNull(&idxTable, &l->elm);
        fklStrBuilderFmt(fp, "===\nI%" PRIu64 ": \n", idx);
        fklPrintItemSet(vm, i, g, fp);
        fklStrBuilderPutc(fp, '\n');
        for (FklLalrItemSetLink *ll = l->v.links; ll; ll = ll->next) {
            FklLalrItemSetHashMapElm *dst = ll->dst;
            size_t *c = graItemStateIdxHashMapGet2NonNull(&idxTable, dst);
            fklStrBuilderFmt(fp, "I%" PRIu64 "--{ ", idx);
            if (ll->allow_ignore)
                fklStrBuilderPuts(fp, "?e ");
            print_prod_sym(vm, &ll->sym, &g->regexes, fp);
            fklStrBuilderFmt(fp, " }-->I%" PRIu64 "\n", *c);
        }
        fklStrBuilderPutc(fp, '\n');
    }
    graItemStateIdxHashMapUninit(&idxTable);
}

const FklLalrBuiltinMatch *fklGetBuiltinMatch(const FklGraSidBuiltinHashMap *ht,
        const FklVMvalue *id) {
    FklLalrBuiltinMatch const **i = NULL;
    i = fklGraSidBuiltinHashMapGet2(ht, (FklVMvalue *)id);
    if (i)
        return *i;
    return NULL;
}

int fklIsNonterminalExist(const FklGrammer *g, FklVMvalue *sid) {
    return fklIsNonterminalExist1(&g->prods, sid);
}

int fklIsNonterminalExist1(const FklProdHashMap *prods, FklVMvalue *sid) {
    return fklProdHashMapGet2(prods, sid) != NULL;
}

FklGrammerProduction *fklGetProductions(const FklGrammer *g, FklVMvalue *sid) {
    return fklGetProductions1(&g->prods, sid);
}

FklGrammerProduction *fklGetProductions1(const FklProdHashMap *prods,
        FklVMvalue *sid) {
    FklGrammerProduction **pp = fklProdHashMapGet2(prods, sid);
    return pp ? *pp : NULL;
}

void fklPrintGrammerIgnores(const FklGrammer *g,
        const FklRegexTable *rt,
        FklStrBuilder *build) {
    const FklGrammerIgnore *ig = g->ignores;
    for (; ig; ig = ig->next) {
        SB_LINE_START("");
        for (size_t i = 0; i < ig->len; i++) {
            const FklGrammerIgnoreSym *u = &ig->ig[i];
            switch (u->term_type) {
            case FKL_TERM_BUILTIN: {
                SB_FMT("%s", u->b.t->name);
                if (u->b.len) {
                    SB_FMT("[");
                    size_t i = 0;
                    for (; i < u->b.len - 1; ++i) {
                        fklPrintStringLiteral2(u->b.args[i], build);
                        SB_FMT(" , ");
                    }
                    fklPrintSymbolLiteral2(u->b.args[i], build);
                    SB_FMT("]");
                }
            } break;

            case FKL_TERM_REGEX:
                print_as_regex(fklGetStringWithRegex(rt, u->re, NULL), build);
                break;
            case FKL_TERM_STRING: {
                fklPrintStringLiteral2(u->str, build);
            } break;
            default:
                FKL_UNREACHABLE();
                break;
            }
            SB_FMT(" ");
        }
        SB_LINE_END("");
    }
}

void fklPrintGrammerProduction(FklVM *vm,
        const FklGrammerProduction *prod,
        const FklRegexTable *rt,
        FklStrBuilder *build) {
    if (!is_Sq_nt(prod->left)) {
        fklPrin1VMvalue2(prod->left, build, vm);
    } else {
        SB_FMT("S'");
    }
    SB_FMT(" -> ");
    size_t len = prod->len;
    const FklGrammerSym *syms = prod->syms;
    for (size_t i = 0; i < len;) {
        SB_FMT(" ");
        print_prod_sym(vm, &syms[i], rt, build);
        if (syms[i].type == FKL_TERM_COMP) {
            i += 1 + syms[i].comp.len;
            if (i < len && syms[i].type == FKL_TERM_IGNORE)
                i++;
            else if (i < len)
                SB_FMT(" .. ");
            continue;
        }

        ++i;
        if (i < len && syms[i].type != FKL_TERM_IGNORE) {
            SB_FMT(" .. ");
        } else {
            ++i;
        }
    }
}

void fklPrintGrammer(FklVM *vm, const FklGrammer *grammer, FILE *fp) {
    FklStrBuilder builder = { 0 };
    fklInitStrBuilderFp(&builder, fp, NULL);
    fklPrintGrammer2(vm, grammer, &builder);
}

void fklPrintGrammer2(FklVM *vm, const FklGrammer *grammer, FklStrBuilder *fp) {
    const FklRegexTable *rt = &grammer->regexes;
    for (FklProdHashMapNode *list = grammer->prods.first; list;
            list = list->next) {
        FklGrammerProduction *prods = list->v;
        for (; prods; prods = prods->next) {
            fklStrBuilderFmt(fp, "(%" PRIu64 ") ", prods->idx);
            fklPrintGrammerProduction(vm, prods, rt, fp);
            fklStrBuilderPutc(fp, '\n');
        }
    }
    fklStrBuilderPuts(fp, "\nignore:\n");
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

static inline ssize_t match_comp(const FklGrammer *g,
        FklGrammerMatchCtx *ctx,
        const FklCompositeSym *comp,
        const char *start,
        const char *cur,
        size_t rest,
        int *is_waiting_for_more) {
    ssize_t total = 0;
    for (size_t i = 0; i < comp->len; i++) {
        const FklGrammerSym *p = &comp->parts[i];
        size_t partLen = 0;
        switch (p->type) {
        default:
            FKL_UNREACHABLE();
            break;
        case FKL_TERM_STRING:
            if (fklStringCharBufMatch(p->str, cur, rest) < 0)
                return -1;
            partLen = p->str->size;
            break;
        case FKL_TERM_REGEX: {
            int last_is_true = 0;
            size_t len = fklRegexLexMatchp(p->re, cur, rest, &last_is_true);
            if (len > rest) {
                *is_waiting_for_more |= last_is_true;
                return -1;
            }
            partLen = len;
        } break;
        case FKL_TERM_BUILTIN: {
            FklBuiltinTerminalMatchArgs part_args = { .len = p->b.len,
                .args = p->b.args };
            if (!p->b.t->match(&part_args,
                        g,
                        start,
                        cur,
                        rest,
                        &partLen,
                        ctx,
                        is_waiting_for_more))
                return -1;
        } break;
        }
        total += partLen;
        cur += partLen;
        rest -= partLen;
    }
    return total;
}

int fklIsStateActionMatch(const FklAnalysisStateActionMatch *match,
        const FklGrammer *g,
        FklGrammerMatchCtx *ctx,
        const char *start,
        const char *cstr,
        size_t restLen,
        int *p_is_waiting_for_more,
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
        FklBuiltinTerminalMatchArgs match_args = { .len = match->func.len,
            .args = match->func.args };
        if (match->func.t->match(&match_args,
                    g,
                    start,
                    cstr + args->skip_ignore_len,
                    restLen - args->skip_ignore_len,
                    &args->matchLen,
                    ctx,
                    p_is_waiting_for_more)) {
            return 1;
        }
    } break;
    case FKL_TERM_REGEX: {
        int last_is_true = 0;
        size_t len = fklRegexLexMatchp(match->re,
                cstr + args->skip_ignore_len,
                restLen - args->skip_ignore_len,
                &last_is_true);
        if (len > (restLen - args->skip_ignore_len))
            *p_is_waiting_for_more |= last_is_true;
        else {
            args->matchLen = len;
            return 1;
        }
    } break;
    case FKL_TERM_IGNORE: {
        size_t match_len = match_ignore(g,
                ctx,
                start,
                cstr,
                restLen,
                p_is_waiting_for_more);
        if (match_len > 0) {
            args->matchLen = match_len;
            return 1;
        }
    } break;

    case FKL_TERM_COMP: {
        ssize_t match_len = match_comp(g,
                ctx,
                &match->comp,
                start,
                cstr + args->skip_ignore_len,
                restLen - args->skip_ignore_len,
                p_is_waiting_for_more);

        if (match_len >= 0) {
            args->matchLen = match_len;
            return 1;
        }
    } break;

    case FKL_TERM_NONE:
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }

    if (*p_is_waiting_for_more)
        return 0;

    if (match->allow_ignore && !has_tried_match_ignore
            && ((args->ignore_len == -2
                        && (args->ignore_len = match_ignore(g,
                                    ctx,
                                    start,
                                    cstr,
                                    restLen,
                                    p_is_waiting_for_more))
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
            vm,
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
    FklGrammerNonterm nonterm = NULL;
    fklUninitParserGrammerParseArg(&args);
    if (fklCheckAndInitGrammerSymbols(g, &nonterm)) {
        print_unresolved_terminal(nonterm, &vm->gc->err_out);
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

FklGrammerIgnore *fklInitBuiltinProductionSet(FklGrammer *g, FklVM *vm) {
    FklParserGrammerParseArg args;
    fklInitParserGrammerParseArg(&args,
            g,
            vm,
            1,
            builtin_prod_action_resolver,
            NULL);
    int err = fklParseProductionRuleWithCstr(&args, builtin_grammer_rules);
    if (err) {
        fklPrintParserGrammerParseError(err, &args, &vm->gc->err_out);
        fklDestroyGrammer(g);
        FKL_UNREACHABLE();
        return NULL;
    }
    fklUninitParserGrammerParseArg(&args);
    return g->ignores;
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
            if (fklBuiltinTermArgsCheck(to_s->b.t, to_s->b.len)) {
                FKL_UNREACHABLE();
            }

        } break;

        case FKL_TERM_NONE:
        case FKL_TERM_KEYWORD:
        case FKL_TERM_IGNORE:
        case FKL_TERM_EOF:
        case FKL_TERM_NONTERM:
        case FKL_TERM_COMP:
            FKL_UNREACHABLE();
            break;
        }
    }
    if (fklAddIgnoreToIgnoreList(&to->ignores, new_ig))
        fklDestroyIgnore(new_ig);
}

void fklMergeGrammerProd(FklGrammer *to,
        const FklGrammerProduction *prod,
        const FklGrammer *from) {
    FklGrammerProduction *new_prod = fklCreateEmptyProduction(prod->left,
            prod->len,
            prod->print_name,
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
            to_s->nt = from_s->nt;
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
                if (fklBuiltinTermArgsCheck(to_s->b.t, to_s->b.len)) {
                    FKL_UNREACHABLE();
                }
                break;
            }

            size_t total_size = to_s->b.len * sizeof(FklString *);
            FklString const **args = fklZmalloc(total_size);
            FKL_ASSERT(args);
            for (size_t i = 0; i < from_s->b.len; ++i) {
                args[i] = fklAddString(&to->terminals, from_s->b.args[i]);
                fklAddString(&to->delimiters, args[i]);
            }
            to_s->b.args = args;
            if (fklBuiltinTermArgsCheck(to_s->b.t, to_s->b.len)) {
                FKL_UNREACHABLE();
            }
        } break;

        case FKL_TERM_COMP: {
            to_s->comp.len = from_s->comp.len;
            to_s->comp.parts = &syms[i + 1];
        } break;

        case FKL_TERM_IGNORE:
        case FKL_TERM_EOF:
            break;
        case FKL_TERM_NONE:
            FKL_UNREACHABLE();
            break;
        }
    }

    if (fklAddProdToProdTableNoRepeat(to, new_prod))
        fklDestroyGrammerProduction(new_prod);
}

int fklMergeGrammer(FklGrammer *g, const FklGrammer *other) {

    for (const FklGrammerIgnore *ig = other->ignores; ig; ig = ig->next) {
        fklMergeGrammerIgnore(g, ig, other);
    }

    for (const FklProdHashMapNode *prods = other->prods.first; prods;
            prods = prods->next) {
        for (const FklGrammerProduction *prod = prods->v; prod;
                prod = prod->next) {
            fklMergeGrammerProd(g, prod, other);
        }
    }

    for (const FklStrHashSetNode *cur = other->delimiters.first; cur;
            cur = cur->next)
        fklAddString(&g->delimiters, cur->k);
    return 0;
}

void fklEmplaceAnalysisSymbol(const FklGrammer *g,
        FklAnalysisSymbolVector *symbols,
        const FklAnalysisStateActionMatch *match,
        const char *start,
        const char *cur,
        size_t rest,
        FklGrammerMatchCtx *ctx,
        uint8_t start_with_ignore,
        uint64_t line) {
    if (match->t != FKL_TERM_COMP) {
        fklInitTerminalAnalysisSymbol(
                fklAnalysisSymbolVectorPushBack(symbols, NULL),
                cur,
                rest,
                ctx,
                start_with_ignore,
                line);
        return;
    }

    int is_waiting_for_more = 0;
    for (size_t i = 0; i < match->comp.len; ++i) {
        const FklGrammerSym *p = &match->comp.parts[i];
        size_t partLen = 0;
        switch (p->type) {
        default:
            FKL_UNREACHABLE();
            break;
        case FKL_TERM_STRING:
            if (fklStringCharBufMatch(p->str, cur, rest) < 0)
                FKL_UNREACHABLE();
            partLen = p->str->size;

            break;
        case FKL_TERM_REGEX: {
            int last_is_true = 0;
            size_t len = fklRegexLexMatchp(p->re, cur, rest, &last_is_true);
            if (len > rest) {
                FKL_UNREACHABLE();
            }
            partLen = len;
        } break;
        case FKL_TERM_BUILTIN: {
            FklBuiltinTerminalMatchArgs part_args = { .len = p->b.len,
                .args = p->b.args };
            if (!p->b.t->match(&part_args,
                        g,
                        start,
                        cur,
                        rest,
                        &partLen,
                        ctx,
                        &is_waiting_for_more))
                FKL_UNREACHABLE();
        } break;
        }

        fklInitTerminalAnalysisSymbol(
                fklAnalysisSymbolVectorPushBack(symbols, NULL),
                cur,
                partLen,
                ctx,
                i == 0 && start_with_ignore,
                line);
        line += fklCountCharInBuf(cur, partLen, '\n');
        cur += partLen;
        rest -= partLen;
    }
}

typedef enum {
    PARSE_BUILTIN_TERM_STATE_EXPECT_START = 0,
    PARSE_BUILTIN_TERM_STATE_EXPECT_END,
} ParseBuiltinTermState;

int fklParseBuiltinTermSrc(FklBuiltinTermSrcHashMap *maps,
        const FklStringVector *lines) {
    ParseBuiltinTermState state = PARSE_BUILTIN_TERM_STATE_EXPECT_START;
    FklStringVector *lines_of_builtin_term = NULL;

    for (size_t i = 0; i < lines->size; ++i) {
        FklString *cur = lines->base[i];

        switch (state) {
        case PARSE_BUILTIN_TERM_STATE_EXPECT_START: {
            const char *pos =
                    fklStrstr(cur->str, "FKL_BUILTIN_TERMINAL_START(");
            if (pos == NULL)
                break;
            size_t name_len = 0;
            const char *name_pos = pos + strlen("FKL_BUILTIN_TERMINAL_START(");
            const char *name_cur = name_pos;

            // skip space
            for (; *name_cur != '\0' && isspace(*name_cur); ++name_cur)
                ;
            for (; *name_cur != '\0' && *name_cur != ')' && !isspace(*name_cur);
                    ++name_cur) {
                name_len++;
            }

            if (name_len == 0) {
                fprintf(stderr, "name len is 0 in line %zu\n", i);
                return -1;
            }

            FklStrView key = {
                .len = name_len,
                .str = name_pos,
            };

            lines_of_builtin_term = fklBuiltinTermSrcHashMapGet(maps, &key);
            if (lines_of_builtin_term != NULL) {
                fprintf(stderr, "duplicate define builtin terminal block\n");
                return -1;
            }

            lines_of_builtin_term = fklBuiltinTermSrcHashMapAdd1(maps, key);
            FKL_ASSERT(lines_of_builtin_term != NULL);

            state = PARSE_BUILTIN_TERM_STATE_EXPECT_END;
        } break;

        case PARSE_BUILTIN_TERM_STATE_EXPECT_END: {
            FKL_ASSERT(lines_of_builtin_term != NULL);
            const char *pos = fklStrstr(cur->str, "FKL_BUILTIN_TERMINAL_END()");
            if (pos != NULL) {
                state = PARSE_BUILTIN_TERM_STATE_EXPECT_START;
                lines_of_builtin_term = NULL;
                break;
            }

            fklStringVectorPushBack2(lines_of_builtin_term, cur);
        } break;
        }
    }

    if (state == PARSE_BUILTIN_TERM_STATE_EXPECT_END) {
        fprintf(stderr, "expecting builtin terminal block end\n");
        return -1;
    }
    return 0;
}
