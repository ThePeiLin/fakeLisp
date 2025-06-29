#include <fakeLisp/base.h>
#include <fakeLisp/bigint.h>
#include <fakeLisp/code_builder.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/zmalloc.h>

#include <ctype.h>
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
            // if (s->b.c && s->b.t->ctx_destroy)
            //     s->b.t->ctx_destroy(s->b.c);

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
    h->ctx_destroyer(h->ctx);
    fklUninitGrammerSymbols(h->syms, h->len);
    fklZfree(h->syms);
    fklZfree(h);
}

static inline void destroy_builtin_grammer_sym(FklLalrBuiltinGrammerSym *s) {
    // if (s->t->ctx_destroy && s->c)
    //     s->t->ctx_destroy(s->c);
}

FklGrammerProduction *fklCreateProduction(FklSid_t group, FklSid_t sid,
                                          size_t len, FklGrammerSym *syms,
                                          const char *name,
                                          FklProdActionFunc func, void *ctx,
                                          void (*destroy)(void *),
                                          void *(*copyer)(const void *)) {
    FklGrammerProduction *r =
        (FklGrammerProduction *)fklZcalloc(1, sizeof(FklGrammerProduction));
    FKL_ASSERT(r);
    r->left.group = group;
    r->left.sid = sid;
    r->len = len;
    r->name = name;
    r->func = func;
    r->ctx = ctx;
    r->ctx_destroyer = destroy;
    r->ctx_copyer = copyer;
    r->syms = syms;
    return r;
}

FklGrammerProduction *fklCreateEmptyProduction(FklSid_t group, FklSid_t sid,
                                               size_t len, const char *name,
                                               FklProdActionFunc func,
                                               void *ctx,
                                               void (*destroy)(void *),
                                               void *(*copyer)(const void *)) {
    FklGrammerProduction *r =
        (FklGrammerProduction *)fklZcalloc(1, sizeof(FklGrammerProduction));
    FKL_ASSERT(r);
    r->left.group = group;
    r->left.sid = sid;
    r->len = len;
    r->name = name;
    r->func = func;
    r->ctx = ctx;
    r->ctx_destroyer = destroy;
    r->ctx_copyer = copyer;
    if (!len)
        r->syms = NULL;
    else {
        r->syms = (FklGrammerSym *)fklZcalloc(len, sizeof(FklGrammerSym));
        FKL_ASSERT(r->syms);
    }
    return r;
}

#define CB_LINE(...) fklCodeBuilderLine(build, __VA_ARGS__)
#define CB_FMT(...) fklCodeBuilderFmt(build, __VA_ARGS__)

#define CB_LINE_START(...) fklCodeBuilderLineStart(build, __VA_ARGS__)
#define CB_LINE_END(...) fklCodeBuilderLineEnd(build, __VA_ARGS__)

#define CB_INDENT(flag)                                                        \
    for (uint8_t flag = (fklCodeBuilderIndent(build), 0); flag < 1;            \
         fklCodeBuilderUnindent(build), ++flag)

#define DEFINE_DEFAULT_C_MATCH_COND(NAME)                                      \
    static void builtin_match_##NAME##_print_c_match_cond(                     \
        const FklBuiltinTerminalMatchArgs *args, FklCodeBuilder *build) {      \
        CB_FMT("builtin_match_" #NAME                                          \
               "(start,*in+otherMatchLen+skip_ignore_len,*restLen-"            \
               "otherMatchLen-skip_ignore_len,&matchLen,ctx)");                \
    }

// static void default_builtin_match_print_src(const FklGrammer *g,
//                                             FklCodeBuilder *build) {
//     CB_FMT("");
// }
//
// static inline const FklGrammerSym *sym_at_idx(const FklGrammerProduction
// *prod,
//                                               size_t i) {
//     if (i < prod->len)
//         return &prod->syms[i];
//     return NULL;
// }
//
// static inline void *init_builtin_grammer_sym(const FklLalrBuiltinMatch *m,
//                                              size_t i,
//                                              FklGrammerProduction *prod,
//                                              FklGrammer *g, int *failed) {
//     if (m->ctx_global_create)
//         return m->ctx_global_create(i, prod, g, failed);
//     if (m->ctx_create) {
//         const FklGrammerSym *sym = sym_at_idx(prod, i + 1);
//         if (sym && sym->type != FKL_TERM_STRING) {
//             *failed = 1;
//             return NULL;
//         }
//         const FklString *next =
//             sym ? fklGetSymbolWithId(sym->nt.sid, &g->terminals)->k : NULL;
//         return m->ctx_create(next, failed);
//     }
//     return NULL;
// }

void fklProdCtxDestroyDoNothing(void *c) {}

void fklProdCtxDestroyFree(void *c) { fklZfree(c); }

void *fklProdCtxCopyerDoNothing(const void *c) { return (void *)c; }

static inline void find_and_add_terminal_in_regex(const FklRegexCode *re,
                                                  FklSymbolTable *tt) {
    const FklRegexObj *objs = re->data;
    for (; objs->type != FKL_REGEX_UNUSED && objs->type != FKL_REGEX_BEGIN;
         objs++)
        ;
    if (objs->type == FKL_REGEX_BEGIN) {
        uint32_t len = 0;
        const FklRegexObj *start = objs + 1;
        for (; start[len].type == FKL_REGEX_CHAR; len++)
            ;
        if (start[len].type == FKL_REGEX_STAR
            || start[len].type == FKL_REGEX_QUESTIONMARK)
            len--;
        if (len) {
            FklString *str = fklCreateString(len, NULL);
            for (uint32_t i = 0; i < len; i++)
                str->str[i] = start[i].ch;
            fklAddSymbol(str, tt);
            fklZfree(str);
        }
    }
    const FklRegexObj *end = objs;
    for (; end->type != FKL_REGEX_UNUSED; end++)
        ;
    if (end != objs && (--end)->type == FKL_REGEX_END) {
        end--;
        uint32_t len = 0;
        for (; end >= objs && end->type == FKL_REGEX_CHAR; end--)
            len++;
        if (len) {
            end++;
            FklString *str = fklCreateString(len, NULL);
            for (uint32_t i = 0; i < len; i++)
                str->str[i] = end[i].ch;
            fklAddSymbol(str, tt);
            fklZfree(str);
        }
    }
}

static inline const FklRegexCode *
add_regex_and_get_terminal(FklRegexTable *t, const char *buf, size_t len,
                           FklSymbolTable *tt) {
    uint32_t old = t->num;
    const FklRegexCode *re = fklAddRegexCharBuf(t, buf, len);
    if (re && old != t->num)
        find_and_add_terminal_in_regex(re, tt);
    return re;
}

static inline FklGrammerProduction *create_grammer_prod_from_cstr(
    const char *str, FklGraSidBuiltinHashMap *builtins,
    FklSymbolTable *symbolTable, FklSymbolTable *termTable,
    FklRegexTable *regexTable, const char *name, FklProdActionFunc func) {
    const char *ss;
    size_t len;
    for (ss = str; isspace(*ss); ss++)
        ;
    for (len = 0; !isspace(ss[len]); len++)
        ;
    FklSid_t left = fklAddSymbolCharBuf(ss, len, symbolTable)->v;
    ss += len;
    len = 0;
    FklStringVector st;
    fklStringVectorInit(&st, 8);
    size_t joint_num = 0;
    while (*ss) {
        for (; isspace(*ss); ss++)
            ;
        for (len = 0; ss[len] && !isspace(ss[len]); len++)
            ;
        if (ss[0] == '+')
            joint_num++;
        if (len)
            fklStringVectorPushBack2(&st, fklCreateString(len, ss));
        ss += len;
    }
    size_t prod_len = st.size - joint_num;
    if (prod_len) {
        prod_len += prod_len - 1 - joint_num;
    }
    FklGrammerProduction *prod = fklCreateEmptyProduction(
        0, left, prod_len, name, func, NULL, fklProdCtxDestroyDoNothing,
        fklProdCtxCopyerDoNothing);
    size_t symIdx = 0;
    if (prod == NULL || prod_len == 0)
        goto exit;
    // if (st.size == 0 && !is_epsilon) {
    //     FklGrammerSym *u = &prod->syms[0];
    //     u->type = FKL_TERM_NONTERM;
    //     u->nt.group = 0;
    //     u->nt.sid = epsilon_id;
    //     goto exit;
    // }
    for (uint32_t i = 0; i < st.size; i++) {
        FklGrammerSym *u = &prod->syms[symIdx];
        u->type = FKL_TERM_STRING;
        FklString *s = st.base[i];
        switch (*(s->str)) {
        case '+':
            continue;
            break;
        case '#':
            u->nt.group = 0;
            u->nt.sid =
                fklAddSymbolCharBuf(&s->str[1], s->size - 1, termTable)->v;
            break;
        case ':':
            u->type = FKL_TERM_KEYWORD;
            u->nt.group = 0;
            u->nt.sid =
                fklAddSymbolCharBuf(&s->str[1], s->size - 1, termTable)->v;
            break;
        case '/': {
            u->type = FKL_TERM_REGEX;
            const FklRegexCode *re = add_regex_and_get_terminal(
                regexTable, &s->str[1], s->size - 1, termTable);
            if (!re) {
                fklDestroyGrammerProduction(prod);
                prod = NULL;
                goto exit;
            }
            u->re = re;
        } break;
        case '&': {
            FklSid_t id =
                fklAddSymbolCharBuf(&s->str[1], s->size - 1, symbolTable)->v;
            const FklLalrBuiltinMatch *builtin =
                fklGetBuiltinMatch(builtins, id);
            if (builtin) {
                u->type = FKL_TERM_BUILTIN;
                u->b.t = builtin;
                u->b.len = 0;
                u->b.args = NULL;
                // u->b.c = NULL;
            } else {
                u->type = FKL_TERM_NONTERM;
                u->nt.group = 0;
                u->nt.sid = id;
            }
        } break;
        }
        ++symIdx;
        if (symIdx < prod_len - 1 && i < st.size - 1
            && (*st.base[i + 1]->str != '+')) {
            FklGrammerSym *u = &prod->syms[symIdx];
            u->type = FKL_TERM_IGNORE;
            ++symIdx;
        }
    }
exit:
    while (!fklStringVectorIsEmpty(&st))
        fklZfree(*fklStringVectorPopBackNonNull(&st));
    fklStringVectorUninit(&st);
    return prod;
}

FklGrammerIgnore *fklCreateEmptyGrammerIgnore(size_t len) {
    FklGrammerIgnore *ig = (FklGrammerIgnore *)fklZcalloc(
        1, sizeof(FklGrammerIgnore) + len * sizeof(FklGrammerIgnoreSym));
    FKL_ASSERT(ig);
    ig->len = len;
    ig->next = NULL;
    return ig;
}

FklGrammerIgnore *fklGrammerSymbolsToIgnore(FklGrammerSym *syms, size_t len,
                                            const FklSymbolTable *tt) {
    for (size_t i = len; i > 0; i--) {
        FklGrammerSym *sym = &syms[i - 1];
        if (sym->type == FKL_TERM_NONTERM)
            return NULL;
        else if (sym->type == FKL_TERM_BUILTIN) {
            int failed = 0;
            FklLalrBuiltinGrammerSym *b = &sym->b;
            // if (b->t->ctx_global_create && !b->t->ctx_create)
            //     return NULL;
            // if (b->c)
            //     destroy_builtin_grammer_sym(b);
            // if (b->t->ctx_create) {
            //     FklGrammerSym *sym = (i < len) ? &syms[i] : NULL;
            //     if (sym && sym->type != FKL_TERM_STRING)
            //         return NULL;
            //     const FklString *next =
            //         sym ? fklGetSymbolWithId(sym->nt.sid, tt)->k : NULL;
            //     b->c = b->t->ctx_create(next, &failed);
            // }
            if (failed)
                return NULL;
        }
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
                igs->str = fklGetSymbolWithId(sym->nt.sid, tt)->k;
            else {
                fklZfree(ig);
                return NULL;
            }
        }
    }
    return ig;
}

static inline FklGrammerIgnore *create_grammer_ignore_from_cstr(
    const char *str, FklGraSidBuiltinHashMap *builtins,
    FklSymbolTable *symbolTable, FklSymbolTable *termTable, FklRegexTable *rt) {
    const char *ss;
    size_t len;
    for (ss = str; isspace(*ss); ss++)
        ;
    for (len = 0; !isspace(ss[len]); len++)
        ;
    FklSid_t left = 0;
    ss += len;
    len = 0;
    FklStringVector st;
    fklStringVectorInit(&st, 8);
    size_t joint_num = 0;
    while (*ss) {
        for (; isspace(*ss); ss++)
            ;
        for (len = 0; ss[len] && !isspace(ss[len]); len++)
            ;
        if (ss[0] == '+')
            joint_num++;
        if (len)
            fklStringVectorPushBack2(&st, fklCreateString(len, ss));
        ss += len;
    }
    size_t prod_len = st.size - joint_num;
    if (!prod_len) {
        while (!fklStringVectorIsEmpty(&st))
            fklZfree(*fklStringVectorPopBackNonNull(&st));
        fklStringVectorUninit(&st);
        return NULL;
    }
    FklGrammerProduction *prod = fklCreateEmptyProduction(
        0, left, prod_len, NULL, NULL, NULL, fklProdCtxDestroyDoNothing,
        fklProdCtxCopyerDoNothing);
    size_t symIdx = 0;
    for (uint32_t i = 0; i < st.size; i++) {
        FklGrammerSym *u = &prod->syms[symIdx];
        u->type = FKL_TERM_STRING;
        FklString *s = st.base[i];
        switch (*(s->str)) {
        case '+':
            continue;
            break;
        case '#':
            u->nt.group = 0;
            u->nt.sid =
                fklAddSymbolCharBuf(&s->str[1], s->size - 1, termTable)->v;
            break;
        case ':':
            u->type = FKL_TERM_KEYWORD;
            u->nt.group = 0;
            u->nt.sid =
                fklAddSymbolCharBuf(&s->str[1], s->size - 1, termTable)->v;
            break;
        case '/': {
            u->type = FKL_TERM_REGEX;
            const FklRegexCode *re = add_regex_and_get_terminal(
                rt, &s->str[1], s->size - 1, termTable);
            if (!re) {
                fklDestroyGrammerProduction(prod);
                goto error;
            }
            u->re = re;
        } break;
        case '&': {
            FklSid_t id =
                fklAddSymbolCharBuf(&s->str[1], s->size - 1, symbolTable)->v;
            const FklLalrBuiltinMatch *builtin =
                fklGetBuiltinMatch(builtins, id);
            if (builtin) {
                u->type = FKL_TERM_BUILTIN;
                u->b.t = builtin;
                u->b.len = 0;
                u->b.args = NULL;
                // u->b.c = NULL;
            } else {
                u->type = FKL_TERM_NONTERM;
                u->nt.group = 0;
                u->nt.sid = id;
            }
        } break;
        }
        symIdx++;
    }
    while (!fklStringVectorIsEmpty(&st))
        fklZfree(*fklStringVectorPopBackNonNull(&st));
    fklStringVectorUninit(&st);
    FklGrammerIgnore *ig =
        fklGrammerSymbolsToIgnore(prod->syms, prod->len, termTable);
    fklDestroyGrammerProduction(prod);
    return ig;
error:
    while (!fklStringVectorIsEmpty(&st))
        fklZfree(*fklStringVectorPopBackNonNull(&st));
    fklStringVectorUninit(&st);
    return NULL;
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
                // if (u0->b.t->ctx_equal)
                //     return u0->b.t->ctx_equal(u0->b.c, u1->b.c);
                return 1;
            }
            return 0;
            break;
        case FKL_TERM_REGEX:
            return u0->re == u1->re;
            break;
        case FKL_TERM_STRING:
        case FKL_TERM_KEYWORD:
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
    FklGrammerSym *u0 = prod0->syms;
    FklGrammerSym *u1 = prod1->syms;
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

static inline FklGrammerProduction *create_extra_production(FklSid_t group,
                                                            FklSid_t start) {
    FklGrammerProduction *prod = fklCreateEmptyProduction(
        0, 0, 2, NULL, NULL, NULL, fklProdCtxDestroyDoNothing,
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

// FklGrammerProduction *
// fklCopyUninitedGrammerProduction(FklGrammerProduction *prod) {
//     FklGrammerProduction *r = fklCreateEmptyProduction(
//         prod->left.group, prod->left.sid, prod->len, prod->name, prod->func,
//         prod->ctx, prod->ctx_destroyer, prod->ctx_copyer);
//     FklGrammerSym *ss = r->syms;
//     FklGrammerSym *oss = prod->syms;
//     memcpy(ss, oss, prod->len * sizeof(FklGrammerSym));
//     r->ctx = r->ctx_copyer(prod->ctx);
//     r->next = NULL;
//     return r;
// }

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
    // if (b0->t->ctx_cmp)
    //     return b0->t->ctx_cmp(b0->c, b1->c);
    return 0;
}

static inline int nonterm_gt(const FklGrammerNonterm *nt0,
                             const FklGrammerNonterm *nt1) {
    return ((nt0->group << 32) + nt0->sid) > ((nt1->group << 32) + nt1->sid);
}

static int nonterm_lt(const FklGrammerNonterm *nt0,
                      const FklGrammerNonterm *nt1) {
    return ((nt0->group << 32) + nt0->sid) > ((nt1->group << 32) + nt1->sid);
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

static inline int ignore_match(const FklGrammer *g, const FklGrammerIgnore *ig,
                               const char *start, const char *str,
                               size_t restLen, size_t *pmatchLen,
                               FklGrammerMatchOuterCtx *ctx,
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
            if (ig->b.t->match(&args, start, str, restLen - matchLen, &len, ctx,
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
                                             FklGrammerMatchOuterCtx *ctx,
                                             const char *start, const char *cur,
                                             size_t restLen) {
    if (restLen) {
        if (start == ctx->start && cur == ctx->cur)
            return ctx->maxNonterminalLen;
        ctx->start = start;
        ctx->cur = cur;
        FklGrammerIgnore *ignores = g->ignores;
        const FklString **terms = g->sortedTerminals;
        size_t num = g->sortedTerminalsNum;
        size_t len = 0;
        while (len < restLen) {
            int is_waiting_for_more = 0;
            for (FklGrammerIgnore *ig = ignores; ig; ig = ig->next) {
                size_t matchLen = 0;
                if (ignore_match(g, ig, start, cur, restLen - len, &matchLen,
                                 ctx, &is_waiting_for_more))
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
                                      const char *start, const char *cstr,
                                      size_t restLen, size_t *pmatchLen,
                                      FklGrammerMatchOuterCtx *ctx,
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

static inline void sort_reachable_terminals(FklGrammer *g) {
    size_t num = g->reachable_terminals.num;
    g->sortedTerminalsNum = num;
    const FklString **terms = NULL;
    if (num) {
        terms = (const FklString **)fklZmalloc(num * sizeof(FklString *));
        FKL_ASSERT(terms);
        FklStrIdHashMapElm **symList = g->reachable_terminals.idl;
        for (size_t i = 0; i < num; i++)
            terms[i] = symList[i]->k;
        qsort(terms, num, sizeof(FklString *), string_len_cmp);
    }
    g->sortedTerminals = terms;
}

// static void *builtin_match_number_global_create(size_t idx,
//                                                 FklGrammerProduction *prod,
//                                                 FklGrammer *g, int *failed) {
//     if (!g->sortedTerminals)
//         sort_reachable_terminals(g);
//     return g;
// }
//
DEFINE_DEFAULT_C_MATCH_COND(dec_int);

static inline void build_builtin_match_print_src(FklCodeBuilder *build,
                                                 const char *name,
                                                 const char *func_name) {
    CB_LINE("static int builtin_match_%s(const char* start", name);
    CB_INDENT(flag) {
        CB_LINE(",const char* cstr");
        CB_LINE(",size_t restLen");
        CB_LINE(",ssize_t* pmatchLen");
        CB_LINE(",FklGrammerMatchOuterCtx* ctx)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("if(restLen){");
        CB_INDENT(flag) {
            CB_LINE(
                "size_t maxLen=get_max_non_term_length(ctx,start,cstr,restLen);");
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
    // .ctx_global_create = builtin_match_number_global_create,
    .build_src = builtin_match_dec_int_print_src,
    .build_c_match_cond = builtin_match_dec_int_print_c_match_cond,
};

static int builtin_match_hex_int_func(const FklBuiltinTerminalMatchArgs *args,
                                      const char *start, const char *cstr,
                                      size_t restLen, size_t *pmatchLen,
                                      FklGrammerMatchOuterCtx *ctx,
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
    // .ctx_global_create = builtin_match_number_global_create,
    .build_src = builtin_match_hex_int_print_src,
    .build_c_match_cond = builtin_match_hex_int_print_c_match_cond,
};

static int builtin_match_oct_int_func(const FklBuiltinTerminalMatchArgs *args,
                                      const char *start, const char *cstr,
                                      size_t restLen, size_t *pmatchLen,
                                      FklGrammerMatchOuterCtx *ctx,
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
    // .ctx_global_create = builtin_match_number_global_create,
    .build_src = builtin_match_oct_int_print_src,
    .build_c_match_cond = builtin_match_oct_int_print_c_match_cond,
};

static int builtin_match_dec_float_func(const FklBuiltinTerminalMatchArgs *args,
                                        const char *start, const char *cstr,
                                        size_t restLen, size_t *pmatchLen,
                                        FklGrammerMatchOuterCtx *ctx,
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
    // .ctx_global_create = builtin_match_number_global_create,
    .build_src = builtin_match_dec_float_print_src,
    .build_c_match_cond = builtin_match_dec_float_print_c_match_cond,
};

static int builtin_match_hex_float_func(const FklBuiltinTerminalMatchArgs *args,
                                        const char *start, const char *cstr,
                                        size_t restLen, size_t *pmatchLen,
                                        FklGrammerMatchOuterCtx *ctx,
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
    // .ctx_global_create = builtin_match_number_global_create,
    .build_src = builtin_match_hex_float_print_src,
    .build_c_match_cond = builtin_match_hex_float_print_c_match_cond,
};

static int builtin_match_identifier_func(
    const FklBuiltinTerminalMatchArgs *args, const char *cstrStart,
    const char *cstr, size_t restLen, size_t *pmatchLen,
    FklGrammerMatchOuterCtx *ctx, int *is_waiting_for_more) {
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
        CB_LINE(",FklGrammerMatchOuterCtx* ctx)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE(
            "size_t maxLen=get_max_non_term_length(ctx,cstrStart,cstr,restLen);");
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
    // .ctx_global_create = builtin_match_number_global_create,
    .build_src = builtin_match_identifier_print_src,
    .build_c_match_cond = builtin_match_identifier_print_c_match_cond,
};

DEFINE_DEFAULT_C_MATCH_COND(noterm);

static void builtin_match_noterm_print_src(const FklGrammer *g,
                                           FklCodeBuilder *build) {
    CB_LINE("static int builtin_match_nonterm(const char* cstrStart");
    CB_INDENT(flag) {
        CB_LINE(",const char* cstr");
        CB_LINE(",size_t restLen");
        CB_LINE(",ssize_t* pmatchLen");
        CB_LINE(",FklGrammerMatchOuterCtx* ctx)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE(
            "size_t maxLen=get_max_non_term_length(ctx,cstrStart,cstr,restLen);");
        CB_LINE("if(!maxLen)");
        CB_INDENT(flag) { CB_LINE("return 0;"); }
        CB_LINE("*pmatchLen=maxLen;");
        CB_LINE("return 1;");
    }
    CB_LINE("}");
}

static int builtin_match_noterm_func(const FklBuiltinTerminalMatchArgs *args,
                                     const char *cstrStart, const char *cstr,
                                     size_t restLen, size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *ctx,
                                     int *is_waiting_for_more) {
    size_t maxLen =
        get_max_non_term_length(args->g, ctx, cstrStart, cstr, restLen);
    if (!maxLen)
        return 0;
    *pmatchLen = maxLen;
    return 1;
}

static const FklLalrBuiltinMatch builtin_match_noterm = {
    .name = "?noterm",
    .match = builtin_match_noterm_func,
    // .ctx_global_create = builtin_match_number_global_create,
    .build_src = builtin_match_noterm_print_src,
    .build_c_match_cond = builtin_match_noterm_print_c_match_cond,
};

// typedef struct {
//     const FklGrammer *g;
//     const FklString *start;
// } SymbolNumberCtx;
//
// static void *s_number_ctx_global_create(size_t idx, FklGrammerProduction
// *prod,
//                                         FklGrammer *g, int *failed) {
//     if (prod->len > 2 || idx != 0) {
//         *failed = 1;
//         return NULL;
//     }
//     const FklString *start = NULL;
//     if (prod->len == 2) {
//         if (prod->syms[1].type != FKL_TERM_STRING) {
//             *failed = 1;
//             return NULL;
//         }
//         start = fklGetSymbolWithId(prod->syms[1].nt.sid, &g->terminals)->k;
//     }
//     prod->len = 1;
//     SymbolNumberCtx *ctx =
//         (SymbolNumberCtx *)fklZmalloc(sizeof(SymbolNumberCtx));
//     FKL_ASSERT(ctx);
//     ctx->start = start;
//     ctx->g = g;
//     if (!g->sortedTerminals)
//         sort_reachable_terminals(g);
//     return ctx;
// }

static FklBuiltinTerminalInitError
s_number_create_new(size_t len, const FklString **args, struct FklGrammer *g) {
    if (len > 1)
        return FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS;
    return 0;
}

// static int s_number_ctx_cmp(const void *c0, const void *c1) {
//     const SymbolNumberCtx *ctx0 = c0;
//     const SymbolNumberCtx *ctx1 = c1;
//     if (ctx0->start == ctx1->start)
//         return 0;
//     else if (ctx0 == NULL)
//         return -1;
//     else if (ctx1 == NULL)
//         return 1;
//     else
//         return fklStringCmp(ctx0->start, ctx1->start);
// }
//
// static int s_number_ctx_equal(const void *c0, const void *c1) {
//     const SymbolNumberCtx *ctx0 = c0;
//     const SymbolNumberCtx *ctx1 = c1;
//     if (ctx0->start == ctx1->start)
//         return 1;
//     else if (ctx0 == NULL || ctx1 == NULL)
//         return 0;
//     else
//         return !fklStringCmp(ctx0->start, ctx1->start);
// }
//
// static uintptr_t s_number_ctx_hash(const void *c0) {
//     const SymbolNumberCtx *ctx0 = c0;
//     return (uintptr_t)ctx0->start;
// }
//
// static void s_number_ctx_destroy(void *c) { fklZfree(c); }

#define SYMBOL_NUMBER_MATCH(F)                                                 \
    if (restLen) {                                                             \
        size_t maxLen =                                                        \
            get_max_non_term_length(args->g, ctx, start, cstr, restLen);       \
        if (maxLen                                                             \
            && (!args->len                                                     \
                || fklStringCharBufMatch(args->args[0], cstr + maxLen,         \
                                         restLen - maxLen)                     \
                       < 0)                                                    \
            && F(cstr, maxLen)) {                                              \
            *pmatchLen = maxLen;                                               \
            return 1;                                                          \
        }                                                                      \
    }                                                                          \
    return 0

static int builtin_match_s_dint_func(const FklBuiltinTerminalMatchArgs *args,
                                     const char *start, const char *cstr,
                                     size_t restLen, size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *ctx,
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
        CB_LINE(",FklGrammerMatchOuterCtx* ctx");
        CB_LINE(",const char* term");
        CB_LINE(",size_t termLen)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("if(restLen) {");
        CB_INDENT(flag) {
            CB_LINE(
                "size_t maxLen=get_max_non_term_length(ctx,start,cstr,restLen);");
            CB_LINE(
                "if(maxLen&&(!term||fklCharBufMatch(term,termLen,cstr+maxLen,restLen-maxLen)<0)");
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
                                                 FklCodeBuilder *build) {      \
        build_lisp_match_print_src(g, build, #NAME, #F);                       \
    }

#define DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(NAME)                            \
    static void builtin_match_##NAME##_print_c_match_cond(                     \
        const FklBuiltinTerminalMatchArgs *args, FklCodeBuilder *build) {      \
        CB_FMT("builtin_match_" #NAME                                          \
               "(start,*in+otherMatchLen+skip_ignore_len,*restLen-"            \
               "otherMatchLen-skip_ignore_len,&matchLen,ctx,");                \
        if (args->len) {                                                       \
            CB_FMT("\"");                                                      \
            build_string_in_hex(args->args[0], build);                         \
            CB_FMT("\"");                                                      \
            CB_FMT(",%" FKL_PRT64U "", args->args[0]->size);                   \
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
                                     const char *start, const char *cstr,
                                     size_t restLen, size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *ctx,
                                     int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsHexInt);
}

static int builtin_match_s_oint_func(const FklBuiltinTerminalMatchArgs *args,
                                     const char *start, const char *cstr,
                                     size_t restLen, size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *ctx,
                                     int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsOctInt);
}

static int builtin_match_s_dfloat_func(const FklBuiltinTerminalMatchArgs *args,
                                       const char *start, const char *cstr,
                                       size_t restLen, size_t *pmatchLen,
                                       FklGrammerMatchOuterCtx *ctx,
                                       int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsDecFloat);
}

static int builtin_match_s_xfloat_func(const FklBuiltinTerminalMatchArgs *args,
                                       const char *start, const char *cstr,
                                       size_t restLen, size_t *pmatchLen,
                                       FklGrammerMatchOuterCtx *ctx,
                                       int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsHexFloat);
}

#undef SYMBOL_NUMBER_MATCH

static const FklLalrBuiltinMatch builtin_match_s_dint = {
    .name = "?s-dint",
    .match = builtin_match_s_dint_func,
    // .ctx_global_create = s_number_ctx_global_create,
    .ctx_create = s_number_create_new,
    // .ctx_cmp = s_number_ctx_cmp,
    // .ctx_equal = s_number_ctx_equal,
    // .ctx_hash = s_number_ctx_hash,
    // .ctx_destroy = s_number_ctx_destroy,
    .build_src = builtin_match_s_dint_print_src,
    .build_c_match_cond = builtin_match_s_dint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_xint = {
    .name = "?s-xint",
    .match = builtin_match_s_xint_func,
    // .ctx_global_create = s_number_ctx_global_create,
    .ctx_create = s_number_create_new,
    // .ctx_cmp = s_number_ctx_cmp,
    // .ctx_equal = s_number_ctx_equal,
    // .ctx_hash = s_number_ctx_hash,
    // .ctx_destroy = s_number_ctx_destroy,
    .build_src = builtin_match_s_xint_print_src,
    .build_c_match_cond = builtin_match_s_xint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_oint = {
    .name = "?s-oint",
    .match = builtin_match_s_oint_func,
    // .ctx_global_create = s_number_ctx_global_create,
    .ctx_create = s_number_create_new,
    // .ctx_cmp = s_number_ctx_cmp,
    // .ctx_equal = s_number_ctx_equal,
    // .ctx_hash = s_number_ctx_hash,
    // .ctx_destroy = s_number_ctx_destroy,
    .build_src = builtin_match_s_oint_print_src,
    .build_c_match_cond = builtin_match_s_oint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_dfloat = {
    .name = "?s-dfloat",
    .match = builtin_match_s_dfloat_func,
    // .ctx_global_create = s_number_ctx_global_create,
    .ctx_create = s_number_create_new,
    // .ctx_cmp = s_number_ctx_cmp,
    // .ctx_equal = s_number_ctx_equal,
    // .ctx_hash = s_number_ctx_hash,
    // .ctx_destroy = s_number_ctx_destroy,
    .build_src = builtin_match_s_dfloat_print_src,
    .build_c_match_cond = builtin_match_s_dfloat_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_xfloat = {
    .name = "?s-xfloat",
    .match = builtin_match_s_xfloat_func,
    // .ctx_global_create = s_number_ctx_global_create,
    .ctx_create = s_number_create_new,
    // .ctx_cmp = s_number_ctx_cmp,
    // .ctx_equal = s_number_ctx_equal,
    // .ctx_hash = s_number_ctx_hash,
    // .ctx_destroy = s_number_ctx_destroy,
    .build_src = builtin_match_s_xfloat_print_src,
    .build_c_match_cond = builtin_match_s_xfloat_print_c_match_cond,
};

static int builtin_match_s_char_func(const FklBuiltinTerminalMatchArgs *args,
                                     const char *start, const char *cstr,
                                     size_t restLen, size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *ctx,
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
        CB_LINE(",FklGrammerMatchOuterCtx* ctx");
        CB_LINE(",const char* prefix");
        CB_LINE(",size_t prefix_size)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("size_t minLen=prefix_size+1;");
        CB_LINE("if(restLen<minLen) return 0;");
        CB_LINE(
            "if(fklCharBufMatch(prefix,prefix_size,cstr,restLen)<0) return 0;");
        CB_LINE("restLen-=prefix_size;");
        CB_LINE("cstr+=prefix_size;");
        CB_LINE(
            "size_t maxLen=get_max_non_term_length(ctx,start,cstr,restLen);");
        CB_LINE("if(!maxLen) *pmatchLen=prefix_size+1;");
        CB_LINE("else *pmatchLen=prefix_size+maxLen;");
        CB_LINE("return 1;");
    }
    CB_LINE("}");
}

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_char);

static FklBuiltinTerminalInitError
s_char_create_new(size_t len, const FklString **args, struct FklGrammer *g) {
    if (len > 1)
        return FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS;
    else if (len < 1)
        return FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_FEW_ARGS;
    return 0;
}

// static void *s_char_ctx_global_create(size_t idx, FklGrammerProduction *prod,
//                                       FklGrammer *g, int *failed) {
//     if (prod->len < 2 || prod->len > 2 || idx != 0) {
//         *failed = 1;
//         return NULL;
//     }
//     const FklString *start = NULL;
//     if (prod->syms[1].type != FKL_TERM_STRING) {
//         *failed = 1;
//         return NULL;
//     }
//     start = fklGetSymbolWithId(prod->syms[1].nt.sid, &g->terminals)->k;
//     prod->len = 1;
//     SymbolNumberCtx *ctx =
//         (SymbolNumberCtx *)fklZmalloc(sizeof(SymbolNumberCtx));
//     FKL_ASSERT(ctx);
//     ctx->start = start;
//     ctx->g = g;
//     if (!g->sortedTerminals)
//         sort_reachable_terminals(g);
//     return ctx;
// }

static const FklLalrBuiltinMatch builtin_match_s_char = {
    .name = "?s-char",
    .match = builtin_match_s_char_func,
    // .ctx_global_create = s_char_ctx_global_create,
    .ctx_create = s_char_create_new,
    // .ctx_cmp = s_number_ctx_cmp,
    // .ctx_equal = s_number_ctx_equal,
    // .ctx_hash = s_number_ctx_hash,
    // .ctx_destroy = s_number_ctx_destroy,
    .build_src = builtin_match_s_char_print_src,
    .build_c_match_cond = builtin_match_s_char_print_c_match_cond,
};

// typedef struct {
//     const FklGrammer *g;
//     const FklString *start;
//     const FklString *end;
// } MatchSymbolCtx;

static int builtin_match_symbol_func(const FklBuiltinTerminalMatchArgs *args,
                                     const char *cstrStart, const char *cstr,
                                     size_t restLen, size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *ctx,
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
            size_t maxLen = get_max_non_term_length(args->g, ctx, cstrStart,
                                                    cstr, restLen - matchLen);
            if ((!matchLen && !maxLen)
                || (fklStringCharBufMatch(start, cstr + maxLen,
                                          restLen - maxLen - matchLen)
                        < 0
                    && maxLen
                    && (fklIsDecInt(cstr, maxLen) || fklIsOctInt(cstr, maxLen)
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
        CB_LINE(",FklGrammerMatchOuterCtx* ctx");
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
                CB_LINE(
                    "if(fklCharBufMatch(start,start_size,cstr,restLen-matchLen)>=0){");
                CB_INDENT(flag) {
                    CB_LINE("matchLen+=start_size;");
                    CB_LINE("cstr+=start_size;");
                    CB_LINE(
                        "size_t len=fklQuotedCharBufMatch(cstr,restLen-matchLen,end,end_size);");
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
                CB_LINE(
                    "size_t maxLen=get_max_non_term_length(ctx,cstrStart,cstr,restLen-matchLen);");
                CB_LINE("if((!matchLen&&!maxLen)");
                CB_INDENT(flag) {
                    CB_LINE(
                        "||(fklCharBufMatch(start,start_size,cstr+maxLen,restLen-maxLen-matchLen)<0");
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
                CB_LINE(
                    "if(fklCharBufMatch(start,start_size,cstr,restLen-matchLen)<0) break;");
            }
            CB_LINE("}");
            CB_LINE("*pmatchLen=matchLen;");
            CB_LINE("return matchLen!=0;");
        }
        CB_LINE("}else{");
        CB_INDENT(flag) {
            CB_LINE(
                "size_t maxLen=get_max_non_term_length(ctx,cstrStart,cstr,restLen);");
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

static void
builtin_match_symbol_print_c_match_cond(const FklBuiltinTerminalMatchArgs *args,
                                        FklCodeBuilder *build) {
    // MatchSymbolCtx *ctx = c;
    const FklString *start =
        args->len > 0 ? args->args[0] : NULL;                     // ctx->start;
    const FklString *end = args->len > 1 ? args->args[1] : start; // ctx->end;
    CB_FMT(
        "builtin_match_symbol(start,*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len,&matchLen,ctx,&is_waiting_for_more,");
    CB_FMT("\"");
    build_string_in_hex(start, build);
    CB_FMT("\",%" FKL_PRT64U ",\"", start->size);
    build_string_in_hex(end, build);
    CB_FMT("\",%" FKL_PRT64U ")", end->size);
}

// static int builtin_match_symbol_cmp(const void *c0, const void *c1) {
//     const MatchSymbolCtx *ctx0 = c0;
//     const MatchSymbolCtx *ctx1 = c1;
//     if (ctx0->start == ctx1->start && ctx0->end == ctx1->end)
//         return 0;
//     if (ctx0->start == NULL && ctx1->start)
//         return -1;
//     else if (ctx1->start == NULL)
//         return 1;
//     else {
//         int r = fklStringCmp(ctx0->start, ctx1->start);
//         if (r)
//             return r;
//         return fklStringCmp(ctx0->end, ctx1->end);
//     }
// }
//
// static int builtin_match_symbol_equal(const void *c0, const void *c1) {
//     const MatchSymbolCtx *ctx0 = c0;
//     const MatchSymbolCtx *ctx1 = c1;
//     if (ctx0->start == ctx1->start && ctx0->end == ctx1->end)
//         return 1;
//     else if (ctx0->start || ctx1->start)
//         return !fklStringCmp(ctx0->start, ctx1->start)
//             && !fklStringCmp(ctx0->end, ctx1->end);
//     return 0;
// }
//
// static void builtin_match_symbol_destroy(void *c) { fklZfree(c); }

// static void *builtin_match_symbol_global_create(size_t idx,
//                                                 FklGrammerProduction *prod,
//                                                 FklGrammer *g, int *failed) {
//     if (prod->len > 3 || idx != 0) {
//         *failed = 1;
//         return NULL;
//     }
//     const FklString *start = NULL;
//     const FklString *end = NULL;
//     if (prod->len == 2) {
//         if (prod->syms[1].type != FKL_TERM_STRING) {
//             *failed = 1;
//             return NULL;
//         }
//         start = fklGetSymbolWithId(prod->syms[1].nt.sid, &g->terminals)->k;
//         end = start;
//     } else if (prod->len == 3) {
//         if (prod->syms[1].type != FKL_TERM_STRING
//             || prod->syms[2].type != FKL_TERM_STRING) {
//             *failed = 1;
//             return NULL;
//         }
//         start = fklGetSymbolWithId(prod->syms[1].nt.sid, &g->terminals)->k;
//         end = fklGetSymbolWithId(prod->syms[2].nt.sid, &g->terminals)->k;
//     }
//     prod->len = 1;
//     MatchSymbolCtx *ctx = (MatchSymbolCtx
//     *)fklZmalloc(sizeof(MatchSymbolCtx)); FKL_ASSERT(ctx); ctx->start =
//     start; ctx->end = end; ctx->g = g; if (!g->sortedTerminals)
//         sort_reachable_terminals(g);
//     return ctx;
// }

static FklBuiltinTerminalInitError
builtin_match_symbol_create_new(size_t len, const FklString **args,
                                struct FklGrammer *g) {
    if (len > 2)
        return FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS;
    return 0;
}

// static uintptr_t builtin_match_symbol_hash(const void *c) {
//     const MatchSymbolCtx *ctx = c;
//     return (uintptr_t)ctx->start + (uintptr_t)ctx->end;
// }
//
static const FklLalrBuiltinMatch builtin_match_symbol = {
    .name = "?symbol",
    .match = builtin_match_symbol_func,
    // .ctx_cmp = builtin_match_symbol_cmp,
    // .ctx_equal = builtin_match_symbol_equal,
    // .ctx_hash = builtin_match_symbol_hash,
    // .ctx_global_create = builtin_match_symbol_global_create,
    .ctx_create = builtin_match_symbol_create_new,
    // .ctx_destroy = builtin_match_symbol_destroy,
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
    {"?dint",       &builtin_match_dec_int    },
    {"?xint",       &builtin_match_hex_int    },
    {"?oint",       &builtin_match_oct_int    },
    {"?dfloat",     &builtin_match_dec_float  },
    {"?xfloat",     &builtin_match_hex_float  },

    {"?s-dint",     &builtin_match_s_dint     },
    {"?s-xint",     &builtin_match_s_xint     },
    {"?s-oint",     &builtin_match_s_oint     },
    {"?s-dfloat",   &builtin_match_s_dfloat   },
    {"?s-xfloat",   &builtin_match_s_xfloat   },

    {"?s-char",     &builtin_match_s_char     },
    {"?symbol",     &builtin_match_symbol     },
    {"?identifier", &builtin_match_identifier },
    {"?noterm",     &builtin_match_noterm     },

    {NULL,          NULL                      },
    // clang-format on
};

void fklInitBuiltinGrammerSymTable(FklGraSidBuiltinHashMap *s,
                                   FklSymbolTable *st) {
    fklGraSidBuiltinHashMapInit(s);
    for (const struct BuiltinGrammerSymList *l = &builtin_grammer_sym_list[0];
         l->name; l++) {
        FklSid_t id = fklAddSymbolCstr(l->name, st)->v;
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
    fklZfree(g->sortedTerminals);
    g->sortedTerminals = NULL;
    g->sortedTerminalsNum = 0;
    FklGrammerIgnore *ig = g->ignores;
    while (ig) {
        FklGrammerIgnore *next = ig->next;
        fklZfree(ig);
        ig = next;
    }
    fklUninitSymbolTable(&g->reachable_terminals);
    fklInitSymbolTable(&g->reachable_terminals);

    g->ignores = NULL;
}

void fklUninitGrammer(FklGrammer *g) {
    fklProdHashMapUninit(&g->productions);
    fklGraSidBuiltinHashMapUninit(&g->builtins);
    fklFirstSetHashMapUninit(&g->firstSets);
    fklUninitSymbolTable(&g->terminals);
    fklUninitSymbolTable(&g->reachable_terminals);
    fklUninitRegexTable(&g->regexes);
    clear_analysis_table(g, g->aTable.num - 1);
    FklGrammerIgnore *ig = g->ignores;
    while (ig) {
        FklGrammerIgnore *next = ig->next;
        fklDestroyIgnore(ig);
        ig = next;
    }
    if (g->sortedTerminals) {
        g->sortedTerminalsNum = 0;
        fklZfree(g->sortedTerminals);
        g->sortedTerminals = NULL;
    }
    memset(g, 0, sizeof(*g));
}

void fklDestroyGrammer(FklGrammer *g) {
    fklUninitGrammer(g);
    fklZfree(g);
}

static inline int init_all_builtin_grammer_sym(FklGrammer *g) {
    int failed = 0;
    if (g->sortedTerminalsNum) {
        fklZfree(g->sortedTerminals);
        g->sortedTerminals = NULL;
        g->sortedTerminalsNum = 0;
    }
    for (FklProdHashMapNode *pl = g->productions.first; pl; pl = pl->next) {
        for (FklGrammerProduction *prod = pl->v; prod; prod = prod->next) {
            for (size_t i = prod->len; i > 0; i--) {
                size_t idx = i - 1;
                FklGrammerSym *u = &prod->syms[idx];
                if (u->type == FKL_TERM_BUILTIN) {
                    // if (!u->b.c)
                    //     u->b.c = init_builtin_grammer_sym(u->b.t, idx, prod,
                    //     g,
                    //                                       &failed);
                    // else if (u->b.t->ctx_global_create) {
                    //     destroy_builtin_grammer_sym(&u->b);
                    //     u->b.c = init_builtin_grammer_sym(u->b.t, idx, prod,
                    //     g,
                    //                                       &failed);
                    // }
                } else if (u->type == FKL_TERM_KEYWORD
                           && !g->sortedTerminalsNum)
                    sort_reachable_terminals(g);
                if (failed)
                    break;
            }
        }
    }
    return failed;
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

static inline int
is_builtin_terminal_match_epsilon(const FklGrammer *g,
                                  const FklLalrBuiltinGrammerSym *b) {
    int is_waiting_for_more = 0;
    size_t matchLen = 0;
    FklGrammerMatchOuterCtx ctx = {
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
    const FklSymbolTable *tt = &g->terminals;

    const FklFirstSetItem item = {.hasEpsilon = 0};

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
             leftProds; leftProds = leftProds->next) {
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
                    FklLalrItemLookAhead la = {.t = sym->type};
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
                             syms; syms = syms->next) {
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
                        const FklString *s =
                            fklGetSymbolWithId(sym->nt.sid, tt)->k;
                        if (s->size == 0) {
                            if (i == lastIdx) {
                                change |= firstItem->hasEpsilon != 1;
                                firstItem->hasEpsilon = 1;
                            }
                        } else {
                            la.s = s;
                            change |=
                                !fklLookAheadHashSetPut(&firstItem->first, &la);
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

void fklInitEmptyGrammer(FklGrammer *r, FklSymbolTable *st) {
    memset(r, 0, sizeof(*r));
    fklInitSymbolTable(&r->terminals);
    fklInitSymbolTable(&r->reachable_terminals);
    fklInitRegexTable(&r->regexes);
    r->st = st;
    fklFirstSetHashMapInit(&r->firstSets);
    fklProdHashMapInit(&r->productions);
    fklInitBuiltinGrammerSymTable(&r->builtins, st);
}

static inline FklGrammer *create_grammer() {
    FklGrammer *r = (FklGrammer *)fklZcalloc(1, sizeof(FklGrammer));
    FKL_ASSERT(r);
    return r;
}

FklGrammer *fklCreateEmptyGrammer(FklSymbolTable *st) {
    FklGrammer *r = create_grammer();
    fklInitEmptyGrammer(r, st);
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
#include <fakeLisp/vector.h>

static inline int add_reachable_terminal(FklGrammer *g) {
    FklProdHashMap *productions = &g->productions;
    GraProdVector prod_stack;
    graProdVectorInit(&prod_stack, g->prodNum);
    FklGrammerProduction *first = NULL;
    for (const FklProdHashMapNode *first_node = g->productions.first;
         first_node; first_node = first_node->next) {
        if (is_Sq_nt(&first_node->v->left))
            first = first_node->v;
    }
    graProdVectorPushBack2(&prod_stack, first);
    FklNontermHashSet nonterm_set;
    fklNontermHashSetInit(&nonterm_set);
    while (!graProdVectorIsEmpty(&prod_stack)) {
        FklGrammerProduction *prods = *graProdVectorPopBackNonNull(&prod_stack);
        for (; prods; prods = prods->next) {
            const FklGrammerSym *syms = prods->syms;
            for (size_t i = 0; i < prods->len; i++) {
                const FklGrammerSym *cur = &syms[i];
                if (cur->type == FKL_TERM_STRING)
                    fklAddSymbol(
                        fklGetSymbolWithId(cur->nt.sid, &g->terminals)->k,
                        &g->reachable_terminals);
                else if (cur->type == FKL_TERM_REGEX)
                    find_and_add_terminal_in_regex(cur->re,
                                                   &g->reachable_terminals);
                else if (cur->type == FKL_TERM_BUILTIN) {
                    for (size_t i = 0; i < cur->b.len; i++)
                        fklAddSymbol(cur->b.args[i], &g->reachable_terminals);
                } else if (cur->type == FKL_TERM_NONTERM
                           && !fklNontermHashSetPut(&nonterm_set, &cur->nt)) {
                    graProdVectorPushBack2(&prod_stack,
                                           fklGetProductions(productions,
                                                             cur->nt.group,
                                                             cur->nt.sid));
                }
            }
        }
    }
    fklNontermHashSetUninit(&nonterm_set);
    graProdVectorUninit(&prod_stack);
    for (FklGrammerIgnore *igs = g->ignores; igs; igs = igs->next) {
        FklGrammerIgnoreSym *igss = igs->ig;
        for (size_t i = 0; i < igs->len; i++) {
            FklGrammerIgnoreSym *cur = &igss[i];
            if (cur->term_type == FKL_TERM_STRING)
                fklAddSymbol(cur->str, &g->reachable_terminals);
            else
                find_and_add_terminal_in_regex(cur->re,
                                               &g->reachable_terminals);
        }
    }
    return 0;
}

static inline int check_undefined_nonterm(FklGrammer *g) {
    FklProdHashMap *productions = &g->productions;
    for (const FklProdHashMapNode *il = productions->first; il; il = il->next) {
        for (const FklGrammerProduction *prods = il->v; prods;
             prods = prods->next) {
            const FklGrammerSym *syms = prods->syms;
            for (size_t i = 0; i < prods->len; i++) {
                const FklGrammerSym *cur = &syms[i];
                if (cur->type == FKL_TERM_NONTERM
                    && !fklProdHashMapGet(productions, &cur->nt)) {
                    fputs("nonterm: ", stderr);
                    fklPrintRawSymbol(fklGetSymbolWithId(cur->nt.sid, g->st)->k,
                                      stderr);
                    fputs(" is not defined\n", stderr);
                    return 1;
                }
            }
        }
    }
    return 0;
}

int fklCheckAndInitGrammerSymbols(FklGrammer *g) {
    int r = check_undefined_nonterm(g) || compute_all_first_set(g);
    if (r)
        return r;
    if (!g->sortedTerminals)
        sort_reachable_terminals(g);
    return 0;
    // return check_undefined_nonterm(g)      //
    //     || add_reachable_terminal(g)       //
    //     || init_all_builtin_grammer_sym(g) //
    //     || compute_all_first_set(g);
}

FklGrammer *fklCreateGrammerFromCstr(const char *str[], FklSymbolTable *st) {
    FklGrammer *grammer = fklCreateEmptyGrammer(st);
    FklSymbolTable *tt = &grammer->terminals;
    FklRegexTable *rt = &grammer->regexes;
    FklGraSidBuiltinHashMap *builtins = &grammer->builtins;
    for (; *str; str++) {
        if (**str == '+') {
            FklGrammerIgnore *ignore =
                create_grammer_ignore_from_cstr(*str, builtins, st, tt, rt);
            if (!ignore) {
                fklDestroyGrammer(grammer);
                return NULL;
            }
            if (fklAddIgnoreToIgnoreList(&grammer->ignores, ignore)) {
                fklDestroyIgnore(ignore);
                fklDestroyGrammer(grammer);
                return NULL;
            }
        } else {
            FklGrammerProduction *prod = create_grammer_prod_from_cstr(
                *str, builtins, st, tt, rt, NULL, NULL);
            if (prod == NULL || fklAddProdAndExtraToGrammer(grammer, prod)) {
                fklDestroyGrammerProduction(prod);
                fklDestroyGrammer(grammer);
                return NULL;
            }
        }
    }
    if (fklCheckAndInitGrammerSymbols(grammer)) {
        fklDestroyGrammer(grammer);
        return NULL;
    }
    return grammer;
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

FklGrammer *fklCreateGrammerFromCstrAction(const FklGrammerCstrAction pa[],
                                           FklSymbolTable *st) {
    FklGrammer *grammer = fklCreateEmptyGrammer(st);
    FklSymbolTable *tt = &grammer->terminals;
    FklRegexTable *rt = &grammer->regexes;
    FklGraSidBuiltinHashMap *builtins = &grammer->builtins;
    for (; pa->cstr; pa++) {
        const char *str = pa->cstr;
        if (*str == '+') {
            FklGrammerIgnore *ignore =
                create_grammer_ignore_from_cstr(str, builtins, st, tt, rt);
            if (!ignore) {
                fklDestroyGrammer(grammer);
                return NULL;
            }
            if (fklAddIgnoreToIgnoreList(&grammer->ignores, ignore)) {
                fklDestroyIgnore(ignore);
                fklDestroyGrammer(grammer);
                return NULL;
            }
        } else {
            FklGrammerProduction *prod = create_grammer_prod_from_cstr(
                str, builtins, st, tt, rt, pa->action_name, pa->func);
            if (prod == NULL || fklAddProdAndExtraToGrammer(grammer, prod)) {
                fklDestroyGrammerProduction(prod);
                fklDestroyGrammer(grammer);
                return NULL;
            }
        }
    }
    if (fklCheckAndInitGrammerSymbols(grammer)) {
        fklDestroyGrammer(grammer);
        return NULL;
    }
    return grammer;
}

static inline void print_prod_sym(FILE *fp, const FklGrammerSym *u,
                                  const FklSymbolTable *st,
                                  const FklSymbolTable *tt,
                                  const FklRegexTable *rt) {
    switch (u->type) {
    case FKL_TERM_BUILTIN:
        // putc('?', fp);
        fputs(u->b.t->name, fp);
        if (u->b.len) {
            fputc('[', fp);
            size_t i = 0;
            for (; i < u->b.len - 1; ++i) {
                fklPrintRawString(u->b.args[i], fp);
                fputs(" , ", fp);
            }
            fklPrintRawString(u->b.args[i], fp);
            fputc(']', fp);
        }
        break;
    case FKL_TERM_REGEX:
        fprintf(fp, "/%s/", fklGetStringWithRegex(rt, u->re, NULL)->str);
        // putc('/', fp);
        // fklPrintString(fklGetStringWithRegex(rt, u->re, NULL), fp);
        break;
    case FKL_TERM_STRING:
        fklPrintRawString(fklGetSymbolWithId(u->nt.sid, tt)->k, fp);
        // putc('#', fp);
        // fklPrintString(fklGetSymbolWithId(u->nt.sid, tt)->k, fp);
        break;
    case FKL_TERM_KEYWORD:
        fklPrintRawSymbol(fklGetSymbolWithId(u->nt.sid, tt)->k, fp);
        // putc(':', fp);
        // fklPrintString(fklGetSymbolWithId(u->nt.sid, tt)->k, fp);
        break;
    case FKL_TERM_NONTERM:
        // putc('&', fp);
        fklPrintString(fklGetSymbolWithId(u->nt.sid, st)->k, fp);
        break;
    case FKL_TERM_IGNORE:
        fputs("?e", fp);
        break;
    case FKL_TERM_NONE:
    case FKL_TERM_EOF:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void print_string_as_dot(const uint8_t *str, char se, size_t size,
                                       FILE *out) {
    uint64_t i = 0;
    while (i < size) {
        unsigned int l = fklGetByteNumOfUtf8(&str[i], size - i);
        if (l == 7) {
            uint8_t j = str[i];
            fprintf(out, "\\x");
            fprintf(out, "%02X", j);
            i++;
        } else if (l == 1) {
            if (str[i] == se)
                fprintf(out, "\\%c", se);
            else if (str[i] == '"')
                fputs("\\\"", out);
            else if (str[i] == '\'')
                fputs("\\'", out);
            else if (str[i] == '\\')
                fputs("\\\\", out);
            else if (isgraph(str[i]))
                putc(str[i], out);
            else if (fklIsSpecialCharAndPrint(str[i], out))
                ;
            else {
                uint8_t j = str[i];
                fprintf(out, "\\x");
                fprintf(out, "%02X", j);
            }
            i++;
        } else {
            for (unsigned int j = 0; j < l; j++)
                putc(str[i + j], out);
            i += l;
        }
    }
}
static inline void print_prod_sym_as_dot(FILE *fp, const FklGrammerSym *u,
                                         const FklSymbolTable *st,
                                         const FklSymbolTable *tt,
                                         const FklRegexTable *rt) {
    switch (u->type) {
    case FKL_TERM_BUILTIN:
        fputc('|', fp);
        fputs(u->b.t->name, fp);
        fputc('|', fp);
        break;
    case FKL_TERM_REGEX: {
        const FklString *str = fklGetStringWithRegex(rt, u->re, NULL);
        fputs("\\/'", fp);
        print_string_as_dot((const uint8_t *)str->str, '"', str->size, fp);
        fputs("'\\/", fp);
    } break;
    case FKL_TERM_STRING:
    case FKL_TERM_KEYWORD: {
        const FklString *str = fklGetSymbolWithId(u->nt.sid, tt)->k;
        fputs("\\\'", fp);
        print_string_as_dot((const uint8_t *)str->str, '"', str->size, fp);
        fputs("\\\'", fp);
    } break;
    case FKL_TERM_NONTERM: {
        const FklString *str = fklGetSymbolWithId(u->nt.sid, st)->k;
        fputc('|', fp);
        print_string_as_dot((const uint8_t *)str->str, '|', str->size, fp);
        fputc('|', fp);
    } break;
    case FKL_TERM_IGNORE:
        fputs("?e", fp);
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

static inline int is_delim_sym2(const FklGrammerProduction *prod,
                                uint32_t idx) {
    FklGrammerSym *sym = &prod->syms[idx];
    if (sym->type == FKL_TERM_IGNORE) {
        return 1;
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

static inline FklLalrItem lalr_item_init(FklGrammerProduction *prod, size_t idx,
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

static inline int lalr_look_ahead_cmp(const FklLalrItemLookAhead *la0,
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
            return lalr_look_ahead_cmp(&i0->la, &i1->la);
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

static inline void lr0_item_set_copy_and_closure(
    FklLalrItemHashSet *dst, const FklLalrItemHashSet *itemSet, FklGrammer *g) {
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

static inline void print_look_ahead(FILE *fp, const FklLalrItemLookAhead *la,
                                    const FklRegexTable *rt) {
    switch (la->t) {
    case FKL_TERM_STRING:
        fklPrintRawString(la->s, fp);
        // putc('#', fp);
        // fklPrintString(la->s, fp);
        break;
    case FKL_TERM_KEYWORD:
        fklPrintRawSymbol(la->s, fp);
        // putc(':', fp);
        // fklPrintString(la->s, fp);
        break;
    case FKL_TERM_EOF:
        fputs("$$", fp);
        break;
    case FKL_TERM_BUILTIN:
        fputs(la->b.t->name, fp);
        if (la->b.len) {
            fputc('[', fp);
            size_t i = 0;
            for (; i < la->b.len - 1; ++i) {
                fklPrintRawString(la->b.args[i], fp);
                fputs(" , ", fp);
            }
            fklPrintRawString(la->b.args[i], fp);
            fputc(']', fp);
        }
        break;
        // fputs("&?", fp);
        // fputs(la->b.t->name, fp);
        // break;
    case FKL_TERM_NONE:
        fputs("()", fp);
        break;
    case FKL_TERM_IGNORE:
        fputs("?e", fp);
        break;
    case FKL_TERM_REGEX:
        fprintf(fp, "/%s/", fklGetStringWithRegex(rt, la->re, NULL)->str);
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void print_item(FILE *fp, const FklLalrItem *item,
                              const FklSymbolTable *st,
                              const FklSymbolTable *tt,
                              const FklRegexTable *rt) {
    size_t i = 0;
    size_t idx = item->idx;
    FklGrammerProduction *prod = item->prod;
    size_t len = prod->len;
    FklGrammerSym *syms = prod->syms;
    if (!is_Sq_nt(&prod->left))
        fklPrintString(fklGetSymbolWithId(prod->left.sid, st)->k, fp);
    else
        fputs("S'", fp);
    fputs(" ->", fp);
    for (; i < idx; i++) {
        putc(' ', fp);
        print_prod_sym(fp, &syms[i], st, tt, rt);
    }
    fputs(" *", fp);
    for (; i < len; i++) {
        putc(' ', fp);
        print_prod_sym(fp, &syms[i], st, tt, rt);
    }
    fputs(" # ", fp);
    print_look_ahead(fp, &item->la, rt);
}

void fklPrintItemSet(const FklLalrItemHashSet *itemSet, const FklGrammer *g,
                     const FklSymbolTable *st, FILE *fp) {
    const FklSymbolTable *tt = &g->terminals;
    FklLalrItem const *curItem = NULL;
    for (FklLalrItemHashSetNode *list = itemSet->first; list;
         list = list->next) {
        if (!curItem || list->k.idx != curItem->idx
            || list->k.prod != curItem->prod) {
            if (curItem)
                putc('\n', fp);
            curItem = &list->k;
            print_item(fp, curItem, st, tt, &g->regexes);
        } else {
            fputs(" , ", fp);
            print_look_ahead(fp, &list->k.la, &g->regexes);
        }
    }
    putc('\n', fp);
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
#define FKL_HASH_VAL_INIT(A, B) abort()
#define FKL_HASH_VAL_UNINIT(V) fklLookAheadHashSetUninit(&(V)->first)
#define FKL_HASH_KEY_EQUAL(A, B) (A)->prod == (B)->prod && (A)->idx == (B)->idx
#define FKL_HASH_KEY_HASH                                                      \
    return fklHashCombine(FKL_TYPE_CAST(uintptr_t, pk->prod)                   \
                              / alignof(FklGrammerProduction),                 \
                          pk->idx);
#define FKL_HASH_ELM_NAME LaFirstSetCache
#include <fakeLisp/hash.h>

// GraItemSetQueue
#define FKL_QUEUE_TYPE_PREFIX Gra
#define FKL_QUEUE_METHOD_PREFIX gra
#define FKL_QUEUE_ELM_TYPE FklLalrItemSetHashMapElm *
#define FKL_QUEUE_ELM_TYPE_NAME ItemSet
#include <fakeLisp/queue.h>

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
#include <fakeLisp/hash.h>

static inline int is_gra_link_sym_allow_ignore(const GraSymbolHashSet *checked,
                                               const FklGrammerNonterm left) {
    FklGrammerSym prod_left_sym = {.type = FKL_TERM_NONTERM, .nt = left};

    return graSymbolHashSetHas2(checked, (GraLinkSym){.sym = prod_left_sym,
                                                      .allow_ignore = 1,
                                                      .is_at_delim = 1})
        || graSymbolHashSetHas2(checked, (GraLinkSym){.sym = prod_left_sym,
                                                      .allow_ignore = 1,
                                                      .is_at_delim = 0});
}

static inline void add_gra_link_syms(GraSymbolHashSet *checked,
                                     const FklLalrItemHashSet *items_closure) {
    for (FklLalrItemHashSetNode *l = items_closure->first; l; l = l->next) {
        FklGrammerSym *sym = get_item_next(&l->k);
        if (sym && sym->type == FKL_TERM_NONTERM) {
            int allow_ignore =
                is_gra_link_sym_allow_ignore(checked, l->k.prod->left);

            // int is_at_delim = is_at_delim_sym(&l->k);
            GraLinkSym ss = {.sym = *sym,
                             .allow_ignore =
                                 allow_ignore || is_at_delim_sym(&l->k),
                             .is_at_delim = 0};
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
            GraLinkSym ss = {.sym = *sym,
                             .allow_ignore = allow_ignore || is_at_delim,
                             .is_at_delim = is_at_delim};
            graSymbolHashSetPut(checked, &ss);
        }
    }
}

static inline void lr0_item_set_goto(GraSymbolHashSet *checked,
                                     FklLalrItemSetHashMapElm *itemset,
                                     FklLalrItemSetHashMap *itemsetSet,
                                     FklGrammer *g, GraItemSetQueue *pending) {
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
            if (next && grammer_sym_equal(&ll->k.sym, next)
                && (ll->k.is_at_delim == is_at_delim_sym(&l->k)
                    || ll->k.sym.type == FKL_TERM_NONTERM)) {
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
        lr0_item_set_goto(&checked, itemsetptr, itemstate_set, grammer,
                          &pending);
        graSymbolHashSetClear(&checked);
    }
    graSymbolHashSetUninit(&checked);
    graItemSetQueueUninit(&pending);
    return itemstate_set;
}

static inline FklLookAheadHashSet *get_first_set_from_first_sets(
    const FklGrammer *g, const FklGrammerProduction *prod, uint32_t idx,
    GraLaFirstSetCacheHashMap *cache, int *pHasEpsilon) {
    size_t len = prod->len;
    if (idx >= len) {
        *pHasEpsilon = 1;
        return NULL;
    }
    GraGetLaFirstSetCacheKey key = {.prod = prod, .idx = idx};
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
        const FklSymbolTable *tt = &g->terminals;
        int hasEpsilon = 0;
        const FklFirstSetHashMap *firstSets = &g->firstSets;
        for (uint32_t i = idx; i < len; i++) {
            FklGrammerSym *sym = &prod->syms[i];

            FklLalrItemLookAhead la = {.t = sym->type};
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
                     symList; symList = symList->next) {
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
                FklString *s = fklGetSymbolWithId(sym->nt.sid, tt)->k;
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

static inline FklLookAheadHashSet *
get_la_first_set(const FklGrammer *g, const FklGrammerProduction *prod,
                 uint32_t beta, GraLaFirstSetCacheHashMap *cache,
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
                FklLookAheadHashSet *first =
                    get_la_first_set(g, l->k.prod, beta, cache, &hasEpsilon);
                const FklGrammerNonterm *left = &next->nt;
                FklGrammerProduction *prods =
                    fklGetGrammerProductions(g, left->group, left->sid);
                if (first) {
                    for (FklLookAheadHashSetNode *first_list = first->first;
                         first_list; first_list = first_list->next) {
                        for (FklGrammerProduction *prod = prods; prod;
                             prod = prod->next) {
                            FklLalrItem newItem = {
                                .prod = prod, .la = first_list->k, .idx = 0};
                            if (!fklLalrItemHashSetPut(itemSet, &newItem))
                                fklLalrItemHashSetPut(next_set, &newItem);
                        }
                    }
                }
                if (hasEpsilon) {
                    for (FklGrammerProduction *prod = prods; prod;
                         prod = prod->next) {
                        FklLalrItem newItem = {
                            .prod = prod, .la = l->k.la, .idx = 0};
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

static inline void
check_lookahead_self_generated_and_spread(FklGrammer *g,
                                          FklLalrItemSetHashMapElm *itemset,
                                          GraLaFirstSetCacheHashMap *cache) {
    FklLalrItemHashSet const *items = &itemset->k;
    FklLalrItemHashSet closure;
    fklLalrItemHashSetInit(&closure);
    for (FklLalrItemHashSetNode *il = items->first; il; il = il->next) {
        if (il->k.la.t == FKL_TERM_NONE) {
            FklLalrItem item = {.prod = il->k.prod,
                                .idx = il->k.idx,
                                .la = FKL_LALR_MATCH_NONE_INIT};
            fklLalrItemHashSetPut(&closure, &item);
            lr1_item_set_closure(&closure, g, cache);
            for (FklLalrItemHashSetNode *cl = closure.first; cl;
                 cl = cl->next) {
                FklLalrItem i = cl->k;
                const FklGrammerSym *s = get_item_next(&i);
                if (s && is_at_delim_sym(&i))
                    i.idx += 2;
                else
                    ++i.idx;
                for (const FklLalrItemSetLink *x = itemset->v.links; x;
                     x = x->next) {
                    if (x->dst == itemset)
                        continue;
                    const FklGrammerSym *xsym = &x->sym;
                    if (s && grammer_sym_equal(s, xsym)) {
                        if (i.la.t == FKL_TERM_NONE)
                            add_lookahead_spread(
                                itemset, &item,
                                // 这些操作可能需要重新计算哈希值
                                FKL_REMOVE_CONST(FklLalrItemHashSet,
                                                 &x->dst->k),
                                &i);
                        else
                            fklLalrItemHashSetPut(
                                FKL_REMOVE_CONST(FklLalrItemHashSet,
                                                 &x->dst->k),
                                &i);
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

static inline void init_lalr_look_ahead(FklLalrItemSetHashMap *lr0,
                                        FklGrammer *g,
                                        GraLaFirstSetCacheHashMap *cache) {
    for (FklLalrItemSetHashMapNode *isl = lr0->first; isl; isl = isl->next) {
        check_lookahead_self_generated_and_spread(g, &isl->elm, cache);
    }
    FklLalrItemSetHashMapNode *isl = lr0->first;
    for (FklLalrItemHashSetNode *il = isl->k.first; il; il = il->next) {
        FklLalrItem item = il->k;
        item.la = FKL_LALR_MATCH_EOF_INIT;
        fklLalrItemHashSetPut(FKL_REMOVE_CONST(FklLalrItemHashSet, &isl->k),
                              &item);
    }
}

static inline void add_look_ahead_to_items(FklLalrItemHashSet *items,
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

static inline void
add_look_ahead_for_all_item_set(FklLalrItemSetHashMap *lr0, FklGrammer *g,
                                GraLaFirstSetCacheHashMap *cache) {
    for (FklLalrItemSetHashMapNode *isl = lr0->first; isl; isl = isl->next) {
        add_look_ahead_to_items(FKL_REMOVE_CONST(FklLalrItemHashSet, &isl->k),
                                g, cache);
    }
}

void fklLr0ToLalrItems(FklLalrItemSetHashMap *lr0, FklGrammer *g) {
    GraLaFirstSetCacheHashMap cache;
    graLaFirstSetCacheHashMapInit(&cache);
    init_lalr_look_ahead(lr0, g, &cache);
    int change;
    do {
        change = 0;
        for (FklLalrItemSetHashMapNode *isl = lr0->first; isl;
             isl = isl->next) {
            change |= lookahead_spread(&isl->elm);
        }
    } while (change);
    add_look_ahead_for_all_item_set(lr0, g, &cache);
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
#include <fakeLisp/hash.h>

static inline void print_look_ahead_as_dot(FILE *fp,
                                           const FklLalrItemLookAhead *la,
                                           const FklRegexTable *rt) {
    switch (la->t) {
    case FKL_TERM_STRING:
    case FKL_TERM_KEYWORD: {
        fputs("\\\'", fp);
        const FklString *str = la->s;
        print_string_as_dot((const uint8_t *)str->str, '\'', str->size, fp);
        fputs("\\\'", fp);
    } break;
    case FKL_TERM_EOF:
        fputs("$$", fp);
        break;
    case FKL_TERM_BUILTIN:
        fputc('|', fp);
        fputs(la->b.t->name, fp);
        fputc('|', fp);
        break;
    case FKL_TERM_NONE:
        fputs("()", fp);
        break;
    case FKL_TERM_IGNORE:
        fputs("?e", fp);
        break;
    case FKL_TERM_REGEX: {
        fputs("\\/\'", fp);
        const FklString *str = fklGetStringWithRegex(rt, la->re, NULL);
        print_string_as_dot((const uint8_t *)str->str, '\'', str->size, fp);
        fputs("\\/\'", fp);
    } break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void print_item_as_dot(FILE *fp, const FklLalrItem *item,
                                     const FklSymbolTable *st,
                                     const FklSymbolTable *tt,
                                     const FklRegexTable *rt) {
    size_t i = 0;
    size_t idx = item->idx;
    FklGrammerProduction *prod = item->prod;
    size_t len = prod->len;
    FklGrammerSym *syms = prod->syms;
    if (!is_Sq_nt(&prod->left)) {
        const FklString *str = fklGetSymbolWithId(prod->left.sid, st)->k;
        fputc('|', fp);
        print_string_as_dot((const uint8_t *)str->str, '"', str->size, fp);
        fputc('|', fp);
    } else
        fputs("S'", fp);
    fputs(" ->", fp);
    for (; i < idx; i++) {
        putc(' ', fp);
        print_prod_sym_as_dot(fp, &syms[i], st, tt, rt);
    }
    fputs(" *", fp);
    for (; i < len; i++) {
        putc(' ', fp);
        print_prod_sym_as_dot(fp, &syms[i], st, tt, rt);
    }
    fputs(" , ", fp);
    print_look_ahead_as_dot(fp, &item->la, rt);
}

static inline void print_item_set_as_dot(const FklLalrItemHashSet *itemSet,
                                         const FklGrammer *g,
                                         const FklSymbolTable *st, FILE *fp) {
    const FklSymbolTable *tt = &g->terminals;
    FklLalrItem const *curItem = NULL;
    for (FklLalrItemHashSetNode *list = itemSet->first; list;
         list = list->next) {
        if (!curItem || list->k.idx != curItem->idx
            || list->k.prod != curItem->prod) {
            if (curItem)
                fputs("\\l\\\n", fp);
            curItem = &list->k;
            print_item_as_dot(fp, curItem, st, tt, &g->regexes);
        } else {
            fputs(" / ", fp);
            print_look_ahead_as_dot(fp, &list->k.la, &g->regexes);
        }
    }
    fputs("\\l\\\n", fp);
}

void fklPrintItemStateSetAsDot(const FklLalrItemSetHashMap *i,
                               const FklGrammer *g, const FklSymbolTable *st,
                               FILE *fp) {
    fputs("digraph \"items-lalr\"{\n", fp);
    fputs("\trankdir=\"LR\"\n", fp);
    fputs("\tranksep=1\n", fp);
    fputs("\tgraph[overlap=false];\n", fp);
    GraItemStateIdxHashMap idxTable;
    graItemStateIdxHashMapInit(&idxTable);
    size_t idx = 0;
    for (const FklLalrItemSetHashMapNode *l = i->first; l; l = l->next, idx++) {
        graItemStateIdxHashMapPut2(&idxTable, &l->elm, idx);
    }
    for (const FklLalrItemSetHashMapNode *ll = i->first; ll; ll = ll->next) {
        const FklLalrItemHashSet *i = &ll->k;
        idx = *graItemStateIdxHashMapGet2NonNull(&idxTable, &ll->elm);
        fprintf(
            fp,
            "\t\"I%" FKL_PRT64U
            "\"[fontname=\"Courier\" nojustify=true shape=\"box\" label=\"I%" FKL_PRT64U
            "\\l\\\n",
            idx, idx);
        print_item_set_as_dot(i, g, st, fp);
        fputs("\"]\n", fp);
        for (FklLalrItemSetLink *l = ll->v.links; l; l = l->next) {
            FklLalrItemSetHashMapElm *dst = l->dst;
            size_t *c = graItemStateIdxHashMapGet2NonNull(&idxTable, dst);
            fprintf(fp,
                    "\tI%" FKL_PRT64U "->I%" FKL_PRT64U
                    "[fontname=\"Courier\" label=\"",
                    idx, *c);
            print_prod_sym_as_dot(fp, &l->sym, st, &g->terminals, &g->regexes);
            fputs("\"]\n", fp);
        }
        putc('\n', fp);
    }
    graItemStateIdxHashMapUninit(&idxTable);
    fputs("}", fp);
}

static inline FklAnalysisStateAction *
create_shift_action(const FklGrammerSym *sym, int allow_ignore,
                    const FklSymbolTable *tt, FklAnalysisState *state) {
    FklAnalysisStateAction *action =
        (FklAnalysisStateAction *)fklZcalloc(1, sizeof(FklAnalysisStateAction));
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
    case FKL_TERM_STRING: {
        const FklString *s = fklGetSymbolWithId(sym->nt.sid, tt)->k;
        action->match.str = s;
    } break;
    case FKL_TERM_IGNORE:
    case FKL_TERM_EOF:
    case FKL_TERM_NONE:
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }

    return action;
}

static inline FklAnalysisStateGoto *
create_state_goto(const FklGrammerSym *sym, int allow_ignore,
                  const FklSymbolTable *tt, FklAnalysisStateGoto *next,
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

static inline int
lalr_look_ahead_and_action_match_equal(const FklAnalysisStateActionMatch *match,
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
        if (lalr_look_ahead_and_action_match_equal(&actions->match, la))
            return 1;
    return 0;
}

static inline void init_action_with_look_ahead(FklAnalysisStateAction *action,
                                               const FklLalrItemLookAhead *la) {
    action->match.t = la->t;
    switch (action->match.t) {
    case FKL_TERM_STRING:
    case FKL_TERM_KEYWORD:
        action->match.str = la->s;
        break;
    case FKL_TERM_BUILTIN:
        action->match.func = la->b;
        // action->match.func.t = la->b.t;
        // action->match.func.c = la->b.c;
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

static inline int add_reduce_action(FklGrammerSymType cur_type,
                                    FklAnalysisState *curState,
                                    const FklGrammerProduction *prod,
                                    const FklLalrItemLookAhead *la) {
    if (check_reduce_conflict(curState->state.action, la))
        return 1;
    FklAnalysisStateAction *action =
        (FklAnalysisStateAction *)fklZcalloc(1, sizeof(FklAnalysisStateAction));
    FKL_ASSERT(action);
    init_action_with_look_ahead(action, la);
    if (is_Sq_nt(&prod->left))
        action->action = FKL_ANALYSIS_ACCEPT;
    else {
        action->action = FKL_ANALYSIS_REDUCE;
        action->prod = prod;

        size_t delim_len = 0;
        for (size_t i = 0; i < prod->len; ++i)
            if (is_delim_sym2(prod, i))
                ++delim_len;

        action->actual_len = prod->len - delim_len;
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
                                    const FklGrammerSym *sym, int allow_ignore,
                                    const FklSymbolTable *tt,
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

// static int builtin_match_ignore_func(const FklBuiltinTerminalMatchArgs *args,
//                                      const char *start, const char *str,
//                                      size_t restLen, size_t *pmatchLen,
//                                      FklGrammerMatchOuterCtx *ctx,
//                                      int *is_waiting_for_more) {
//     FklGrammerIgnore *ig = args->g->ignores;
//     return ignore_match(args->g, ig, start, str, restLen, pmatchLen,
//     ctx,
//                         is_waiting_for_more);
// }

#define PRINT_C_REGEX_PREFIX "R_"

static inline void ignore_print_c_match_cond(const FklGrammerIgnore *ig,
                                             const FklGrammer *g,
                                             FklCodeBuilder *build) {
    CB_FMT("(");
    size_t len = ig->len;
    const FklGrammerIgnoreSym *igss = ig->ig;

    const FklGrammerIgnoreSym *igs = &igss[0];
    switch (igs->term_type) {
    case FKL_TERM_BUILTIN: {
        FklBuiltinTerminalMatchArgs args = {
            .g = g,
            .len = igs->b.len,
            .args = igs->b.args,
        };
        igs->b.t->build_c_match_cond(&args, build);
    } break;
    case FKL_TERM_REGEX: {
        uint64_t num = 0;
        fklGetStringWithRegex(&g->regexes, igs->re, &num);
        CB_FMT("regex_lex_match_for_parser_in_c((const FklRegexCode*)&");
        CB_FMT(PRINT_C_REGEX_PREFIX "%" FKL_PRT64X, num);
        CB_FMT(
            ",*in+otherMatchLen,*restLen-otherMatchLen,&matchLen,&is_waiting_for_more)");
    } break;
    case FKL_TERM_STRING: {
        CB_FMT("(matchLen=fklCharBufMatch(\"");
        build_string_in_hex(igs->str, build);
        CB_FMT("\",%" FKL_PRT64U
               ",*in+otherMatchLen,*restLen-otherMatchLen))>=0",
               igs->str->size);
    } break;
    case FKL_TERM_EOF:
    case FKL_TERM_NONE:
    case FKL_TERM_IGNORE:
    case FKL_TERM_NONTERM:
    case FKL_TERM_KEYWORD:
        FKL_UNREACHABLE();
        break;
    }

    CB_FMT("&&((otherMatchLen+=matchLen)||1)");

    for (size_t i = 1; i < len; i++) {
        CB_FMT("&&");
        const FklGrammerIgnoreSym *igs = &igss[i];
        switch (igs->term_type) {
        case FKL_TERM_BUILTIN: {
            FklBuiltinTerminalMatchArgs args = {
                .g = g,
                .len = igs->b.len,
                .args = igs->b.args,
            };
            igs->b.t->build_c_match_cond(&args, build);
        } break;
        case FKL_TERM_REGEX: {
            uint64_t num = 0;
            fklGetStringWithRegex(&g->regexes, igs->re, &num);
            CB_FMT("regex_lex_match_for_parser_in_c((const FklRegexCode*)&");
            CB_FMT(PRINT_C_REGEX_PREFIX "%" FKL_PRT64X, num);
            CB_FMT(
                ",*in+otherMatchLen,*restLen-otherMatchLen,&matchLen,&is_waiting_for_more)");
        } break;
        case FKL_TERM_STRING: {
            CB_FMT("(matchLen+=fklCharBufMatch(\"");
            build_string_in_hex(igs->str, build);
            CB_FMT("\",%" FKL_PRT64U
                   ",*in+otherMatchLen,*restLen-otherMatchLen))>=0",
                   igs->str->size);
        } break;
        case FKL_TERM_EOF:
        case FKL_TERM_NONE:
        case FKL_TERM_IGNORE:
        case FKL_TERM_NONTERM:
        case FKL_TERM_KEYWORD:
            FKL_UNREACHABLE();
            break;
        }

        CB_FMT("&&((otherMatchLen+=matchLen)||1)");
    }
    CB_FMT("&&((matchLen=otherMatchLen)||1)");
    CB_FMT(")");
}

// static void
// builtin_match_ignore_print_c_match_cond(const FklBuiltinTerminalMatchArgs
// *args,
//                                         const FklGrammer *g,
//                                         FklCodeBuilder *build) {
//     ignore_print_c_match_cond(c, g, build);
// }
//
// static const FklLalrBuiltinMatch builtin_match_ignore = {
//     .name = NULL,
//     .match = builtin_match_ignore_func,
//     .ctx_create = NULL,
//     .ctx_destroy = NULL,
//     .ctx_global_create = NULL,
//     .ctx_cmp = NULL,
//     .ctx_hash = NULL,
//     .build_src = default_builtin_match_print_src,
//     .build_c_match_cond = builtin_match_ignore_print_c_match_cond,
// };

static inline FklAnalysisStateAction *
create_builtin_ignore_action(FklGrammer *g, FklGrammerIgnore *ig) {
    FklAnalysisStateAction *action =
        (FklAnalysisStateAction *)fklZcalloc(1, sizeof(FklAnalysisStateAction));
    FKL_ASSERT(action);
    action->next = NULL;
    action->action = FKL_ANALYSIS_IGNORE;
    action->state = NULL;
    if (ig->len == 1) {
        FklGrammerIgnoreSym *igs = &ig->ig[0];
        action->match.t = igs->term_type;
        switch (igs->term_type) {
        case FKL_TERM_BUILTIN:
            action->match.func = igs->b;
            break;
        case FKL_TERM_REGEX:
            action->match.re = igs->re;
            break;
        case FKL_TERM_STRING:
        case FKL_TERM_KEYWORD:
            action->match.str = igs->str;
            break;

        case FKL_TERM_EOF:
        case FKL_TERM_NONE:
        case FKL_TERM_IGNORE:
        case FKL_TERM_NONTERM:
            FKL_UNREACHABLE();
            break;
        }
    } else {
#warning INCOMPLETE
        FKL_UNREACHABLE();
        // action->match.t = FKL_TERM_BUILTIN;
        // action->match.func.t = &builtin_match_ignore;
        // action->match.func.c = ig;
    }
    return action;
}

static inline void add_ignore_action(FklGrammer *g,
                                     FklAnalysisState *curState) {
    for (FklGrammerIgnore *ig = g->ignores; ig; ig = ig->next) {
        FklAnalysisStateAction *action = create_builtin_ignore_action(g, ig);
        FklAnalysisStateAction **pa = &curState->state.action;
        for (; *pa; pa = &(*pa)->next)
            if ((*pa)->match.t == FKL_TERM_IGNORE)
                break;
        action->next = *pa;
        *pa = action;
    }
}

// GraProdHashSet
#define FKL_HASH_TYPE_PREFIX Gra
#define FKL_HASH_METHOD_PREFIX gra
#define FKL_HASH_KEY_TYPE FklGrammerProduction *
#define FKL_HASH_ELM_NAME Prod
#define FKL_HASH_KEY_HASH                                                      \
    return fklHash64Shift(FKL_TYPE_CAST(uintptr_t, (*pk))                      \
                          / alignof(FklGrammerProduction));
#include <fakeLisp/hash.h>

static inline int
is_only_single_way_to_reduce(const FklLalrItemSetHashMapElm *set) {
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

int fklGenerateLalrAnalyzeTable(FklGrammer *grammer,
                                FklLalrItemSetHashMap *states) {
    int hasConflict = 0;
    grammer->aTable.num = states->count;
    FklSymbolTable *tt = &grammer->terminals;
    FklAnalysisState *astates;
    if (!states->count)
        astates = NULL;
    else {
        astates = (FklAnalysisState *)fklZmalloc(states->count
                                                 * sizeof(FklAnalysisState));
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
                        add_shift_action(*priority, curState, sym,
                                         ll->allow_ignore, tt, dstState);
                } else if (*priority == FKL_TERM_STRING && ll->allow_ignore) {
                    curState->state.gt =
                        create_state_goto(sym, ll->allow_ignore, tt,
                                          curState->state.gt, dstState);
                } else if (*priority == FKL_TERM_KEYWORD && !ll->allow_ignore) {
                    curState->state.gt =
                        create_state_goto(sym, ll->allow_ignore, tt,
                                          curState->state.gt, dstState);
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
                        hasConflict = add_reduce_action(*priority, curState,
                                                        il->k.prod, &il->k.la);
                    }
                    if (hasConflict) {
                        clear_analysis_table(grammer, idx);
                        fprintf(stderr,
                                "[%s: %d] conflict idx: %lu, la: ", __FILE__,
                                __LINE__, idx);
                        print_look_ahead(stderr, &il->k.la, &grammer->regexes);
                        fputs(" prod: ", stderr);
                        fklPrintGrammerProduction(
                            stderr, il->k.prod, grammer->st,
                            &grammer->terminals, &grammer->regexes);
                        fprintf(stderr, "idx: %u\n", il->k.idx);
                        goto break_loop;
                    }
                }
            }
        }

        if (is_single_way) {
            FklLalrItem const *item = &items->first->k;
            FklLalrItemLookAhead eofla = FKL_LALR_MATCH_EOF_INIT;
            add_reduce_action(FKL_TERM_EOF, curState, item->prod, &eofla);
        }
        if (idx == 0 && !ignore_added)
            add_ignore_action(grammer, curState);
    }

break_loop:
    graItemStateIdxHashMapUninit(&idxTable);
    return hasConflict;
}

static inline void
print_look_ahead_of_analysis_table(FILE *fp,
                                   const FklAnalysisStateActionMatch *match) {
    switch (match->t) {
    case FKL_TERM_STRING: {
        fputc('\'', fp);
        const FklString *str = match->str;
        print_string_as_dot((const uint8_t *)str->str, '\'', str->size, fp);
        fputc('\'', fp);
    } break;
    case FKL_TERM_KEYWORD: {
        fputc('\'', fp);
        const FklString *str = match->str;
        print_string_as_dot((const uint8_t *)str->str, '\'', str->size, fp);
        fputs("\'$", fp);
    } break;
    case FKL_TERM_EOF:
        fputs("$$", fp);
        break;
    case FKL_TERM_BUILTIN:
        fputs(match->func.t->name, fp);
        break;
    case FKL_TERM_NONE:
        fputs("()", fp);
        break;
    case FKL_TERM_REGEX:
        fprintf(fp, "/%p/", match->re);
        break;
    case FKL_TERM_IGNORE:
        fputs("?e", fp);
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

void fklPrintAnalysisTable(const FklGrammer *grammer, const FklSymbolTable *st,
                           FILE *fp) {
    size_t num = grammer->aTable.num;
    FklAnalysisState *states = grammer->aTable.states;

    for (size_t i = 0; i < num; i++) {
        fprintf(fp, "%" FKL_PRT64U ": ", i);
        FklAnalysisState *curState = &states[i];
        for (FklAnalysisStateAction *actions = curState->state.action; actions;
             actions = actions->next) {
            switch (actions->action) {
            case FKL_ANALYSIS_SHIFT:
                fputs("S(", fp);
                print_look_ahead_of_analysis_table(fp, &actions->match);
                {
                    uintptr_t idx = actions->state - states;
                    fprintf(fp, " , %" FKL_PRT64U " )", idx);
                }
                break;
            case FKL_ANALYSIS_REDUCE:
                fputs("R(", fp);
                print_look_ahead_of_analysis_table(fp, &actions->match);
                fprintf(fp, " , %" FKL_PRT64U " )", actions->prod->idx);
                break;
            case FKL_ANALYSIS_ACCEPT:
                fputs("acc(", fp);
                print_look_ahead_of_analysis_table(fp, &actions->match);
                fputc(')', fp);
                break;
            case FKL_ANALYSIS_IGNORE:
                break;
            }
            fputc('\t', fp);
        }
        fputs("|\t", fp);
        for (FklAnalysisStateGoto *gt = curState->state.gt; gt; gt = gt->next) {
            uintptr_t idx = gt->state - states;
            fputc('(', fp);
            fklPrintRawSymbol(fklGetSymbolWithId(gt->nt.sid, st)->k, fp);
            fprintf(fp, " , %" FKL_PRT64U ")", idx);
            fputc('\t', fp);
        }
        fputc('\n', fp);
    }
}

static uintptr_t
action_match_hash_func(const FklAnalysisStateActionMatch *match) {
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
#include <fakeLisp/hash.h>

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

static inline void print_symbol_for_grapheasy(const FklString *stri, FILE *fp) {
    size_t size = stri->size;
    const uint8_t *str = (uint8_t *)stri->str;
    size_t i = 0;
    while (i < size) {
        unsigned int l = fklGetByteNumOfUtf8(&str[i], size - i);
        if (l == 7) {
            uint8_t j = str[i];
            fprintf(fp, "\\x");
            fprintf(fp, "%02X", j);
            i++;
        } else if (l == 1) {
            if (str[i] == '\\')
                fputs("\\\\", fp);
            else if (str[i] == '|')
                fputs("\\\\|", fp);
            else if (str[i] == ']')
                fputs("\\]", fp);
            else if (isgraph(str[i]))
                putc(str[i], fp);
            else if (fklIsSpecialCharAndPrint(str[i], fp))
                ;
            else {
                uint8_t j = str[i];
                fprintf(fp, "\\x");
                fprintf(fp, "%02X", j);
            }
            i++;
        } else {
            for (unsigned int j = 0; j < l; j++)
                putc(str[i + j], fp);
            i += l;
        }
    }
}

static inline void print_string_for_grapheasy(const FklString *stri, FILE *fp) {
    size_t size = stri->size;
    const uint8_t *str = (uint8_t *)stri->str;
    size_t i = 0;
    while (i < size) {
        unsigned int l = fklGetByteNumOfUtf8(&str[i], size - i);
        if (l == 7) {
            uint8_t j = str[i];
            fprintf(fp, "\\x");
            fprintf(fp, "%02X", j);
            i++;
        } else if (l == 1) {
            if (str[i] == '\\')
                fputs("\\\\", fp);
            else if (str[i] == '\'')
                fputs("\\\\'", fp);
            else if (str[i] == '#')
                fputs("\\#", fp);
            else if (str[i] == '|')
                fputs("\\|", fp);
            else if (str[i] == ']')
                fputs("\\]", fp);
            else if (isgraph(str[i]))
                putc(str[i], fp);
            else if (fklIsSpecialCharAndPrint(str[i], fp))
                ;
            else {
                uint8_t j = str[i];
                fprintf(fp, "\\x");
                fprintf(fp, "%02X", j);
            }
            i++;
        } else {
            for (unsigned int j = 0; j < l; j++)
                putc(str[i + j], fp);
            i += l;
        }
    }
}

static inline void
print_look_ahead_for_grapheasy(const FklAnalysisStateActionMatch *la,
                               FILE *fp) {
    switch (la->t) {
    case FKL_TERM_STRING: {
        fputc('\'', fp);
        print_string_for_grapheasy(la->str, fp);
        fputc('\'', fp);
    } break;
    case FKL_TERM_KEYWORD: {
        fputc('\'', fp);
        print_string_for_grapheasy(la->str, fp);
        fputs("\'$", fp);
    } break;
    case FKL_TERM_EOF:
        fputc('$', fp);
        break;
    case FKL_TERM_IGNORE:
        fputs("?e", fp);
        break;
    case FKL_TERM_BUILTIN:
        fputs("\\|", fp);
        fputs(la->func.t->name, fp);
        fputs("\\|", fp);
        break;
    case FKL_TERM_NONE:
        fputs("()", fp);
        break;
    case FKL_TERM_REGEX:
        fprintf(fp, "/%p/", la->re);
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void
print_table_header_for_grapheasy(const GraActionMatchHashSet *la,
                                 const FklNontermHashSet *sid,
                                 const FklSymbolTable *st, FILE *fp) {
    fputs("\\n|", fp);
    for (GraActionMatchHashSetNode *al = la->first; al; al = al->next) {
        print_look_ahead_for_grapheasy(&al->k, fp);
        fputc('|', fp);
    }
    fputs("\\n|\\n", fp);
    for (FklNontermHashSetNode *sl = sid->first; sl; sl = sl->next) {
        fputc('|', fp);
        fputs("\\|", fp);
        print_symbol_for_grapheasy(fklGetSymbolWithId(sl->k.sid, st)->k, fp);
        fputs("\\|", fp);
    }
    fputs("||\n", fp);
}

static inline FklAnalysisStateAction *
find_action(FklAnalysisStateAction *action,
            const FklAnalysisStateActionMatch *match) {
    for (; action; action = action->next) {
        if (action_match_equal(match, &action->match))
            return action;
    }
    return NULL;
}

static inline FklAnalysisStateGoto *find_gt(FklAnalysisStateGoto *gt,
                                            FklSid_t group, FklSid_t id) {
    for (; gt; gt = gt->next) {
        if (gt->nt.sid == id && gt->nt.group == group)
            return gt;
    }
    return NULL;
}

void fklPrintAnalysisTableForGraphEasy(const FklGrammer *g,
                                       const FklSymbolTable *st, FILE *fp) {
    size_t num = g->aTable.num;
    FklAnalysisState *states = g->aTable.states;

    fputs("graph{title:state-table;}", fp);
    fputs("[\n", fp);

    GraActionMatchHashSet laTable;
    FklNontermHashSet sidSet;
    init_analysis_table_header(&laTable, &sidSet, states, num);

    print_table_header_for_grapheasy(&laTable, &sidSet, st, fp);

    GraActionMatchHashSetNode *laList = laTable.first;
    FklNontermHashSetNode *sidList = sidSet.first;
    for (size_t i = 0; i < num; i++) {
        const FklAnalysisState *curState = &states[i];
        fprintf(fp, "%" FKL_PRT64U ": |", i);
        for (GraActionMatchHashSetNode *al = laList; al; al = al->next) {
            FklAnalysisStateAction *action =
                find_action(curState->state.action, &al->k);
            if (action) {
                switch (action->action) {
                case FKL_ANALYSIS_SHIFT: {
                    uintptr_t idx = action->state - states;
                    fprintf(fp, "s%" FKL_PRT64U "", idx);
                } break;
                case FKL_ANALYSIS_REDUCE:
                    fprintf(fp, "r%" FKL_PRT64U "", action->prod->idx);
                    break;
                case FKL_ANALYSIS_ACCEPT:
                    fputs("acc", fp);
                    break;
                case FKL_ANALYSIS_IGNORE:
                    break;
                }
            } else
                fputs("\\n", fp);
            fputc('|', fp);
        }
        fputs("\\n|\\n", fp);
        for (FklNontermHashSetNode *sl = sidList; sl; sl = sl->next) {
            fputc('|', fp);
            FklAnalysisStateGoto *gt =
                find_gt(curState->state.gt, sl->k.group, sl->k.sid);
            if (gt) {
                uintptr_t idx = gt->state - states;
                fprintf(fp, "%" FKL_PRT64U "", idx);
            } else
                fputs("\\n", fp);
        }
        fputs("||\n", fp);
    }
    fputc(']', fp);
    graActionMatchHashSetUninit(&laTable);
    fklNontermHashSetUninit(&sidSet);
}

static inline void
build_get_max_non_term_length_prototype_to_c_file(FklCodeBuilder *build) {

    CB_LINE("static inline size_t");
    CB_LINE("get_max_non_term_length(FklGrammerMatchOuterCtx*");

    CB_INDENT(flag) {
        CB_LINE(",const char*");
        CB_LINE(",const char*");
        CB_LINE(",size_t);");
    }
}

static inline void
build_match_ignore_prototype_to_c_file(FklCodeBuilder *build) {
    CB_LINE("static inline size_t match_ignore(const char*,size_t,int* );");
}

static inline void build_match_ignore_to_c_file(const FklGrammer *g,
                                                FklCodeBuilder *build) {
    CB_LINE(
        "static inline size_t match_ignore(const char *start, size_t rest_len, int* p_is_waiting_for_more) {\n");

    CB_INDENT(flag) {
        const FklGrammerIgnore *ig = g->ignores;
        if (ig) {
            CB_LINE("ssize_t matchLen=0;");
            CB_LINE("size_t otherMatchLen=0;");
            CB_LINE("size_t ret_len=0;");
            CB_LINE("const char** in=&start;");
            CB_LINE("size_t* restLen=&rest_len;");
            CB_LINE("int is_waiting_for_more=0;");
            CB_LINE("(void)is_waiting_for_more;");
            CB_LINE("(void)restLen;");

            CB_LINE("for(;rest_len>ret_len;){");
            CB_INDENT(flag) {
                CB_LINE_START("if(");
                ignore_print_c_match_cond(ig, g, build);
                CB_INDENT(flag) {
                    for (ig = ig->next; ig; ig = ig->next) {
                        CB_LINE_END("");
                        CB_LINE_START("||");
                        ignore_print_c_match_cond(ig, g, build);
                    }
                }
                CB_LINE_END(") ret_len=otherMatchLen;");

                CB_LINE("else break;");
            }
            CB_LINE("}");

            CB_LINE("*p_is_waiting_for_more=is_waiting_for_more;");
            CB_LINE("return ret_len;");
        } else {
            CB_LINE("return 0;");
        }
    }

    CB_LINE("}");
}

static inline void
build_get_max_non_term_length_to_c_file(const FklGrammer *g,
                                        FklCodeBuilder *build) {
    CB_LINE("static inline size_t");
    CB_LINE("get_max_non_term_length(FklGrammerMatchOuterCtx* ctx");
    CB_INDENT(flag) {
        CB_LINE(",const char* start");
        CB_LINE(",const char* cur");
        CB_LINE(",size_t rLen) {");
    }
    CB_INDENT(flag) {
        CB_LINE("if(rLen) {");
        CB_INDENT(flag) {
            CB_LINE(
                "if(start==ctx->start&&cur==ctx->cur) return ctx->maxNonterminalLen;");
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
                    ignore_print_c_match_cond(igns, g, build);
                    igns = igns->next;
                    for (; igns; igns = igns->next) {
                        CB_LINE_END("");
                        CB_LINE_START("||");
                        ignore_print_c_match_cond(igns, g, build);
                    }
                }
                if (g->ignores && g->sortedTerminalsNum) {
                    CB_LINE_END("");
                    CB_LINE_START("||");
                }
                if (g->sortedTerminalsNum) {
                    size_t num = g->sortedTerminalsNum;
                    const FklString **terminals = g->sortedTerminals;
                    const FklString *cur = terminals[0];
                    CB_LINE("(matchLen=fklCharBufMatch(\"");
                    build_string_in_hex(cur, build);
                    CB_LINE("\",%" FKL_PRT64U
                            ",*in+otherMatchLen,*restLen-otherMatchLen))>=0",
                            cur->size);
                    for (size_t i = 1; i < num; i++) {
                        CB_LINE_END("");
                        CB_LINE_START("||");
                        const FklString *cur = terminals[i];
                        CB_LINE("(matchLen=fklCharBufMatch(\"");
                        build_string_in_hex(cur, build);
                        CB_LINE(
                            "\",%" FKL_PRT64U
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
        CB_LINE("FklGrammerMatchOuterCtx* ctx,");
        CB_LINE("const char* start);");
    }
}

static inline void
build_match_char_buf_end_with_terminal_to_c_file(FklCodeBuilder *build) {
    CB_LINE("static inline size_t");
    CB_LINE("match_char_buf_end_with_terminal(const char* pattern");
    CB_INDENT(flag) {
        CB_LINE(",size_t pattern_size");
        CB_LINE(",const char* cstr");
        CB_LINE(",size_t restLen");
        CB_LINE(",FklGrammerMatchOuterCtx* ctx");
        CB_LINE(",const char* start)");
    }

    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE(
            "size_t maxNonterminalLen=get_max_non_term_length(ctx,start,cstr,restLen);");
        CB_LINE(
            "ssize_t matchLen=fklCharBufMatch(pattern,pattern_size,cstr,restLen);");
        CB_LINE("return matchLen>=0 && maxNonterminalLen==(size_t)matchLen;");
    }
    CB_LINE("}");
}

static inline void
build_state_action_match_to_c_file(const FklAnalysisStateAction *ac,
                                   const FklGrammer *g, FklCodeBuilder *build) {
    switch (ac->match.t) {
    case FKL_TERM_KEYWORD:
        CB_FMT("(matchLen=match_char_buf_end_with_terminal(\"");
        build_string_in_hex(ac->match.str, build);
        CB_FMT(
            "\",%" FKL_PRT64U
            ",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len,ctx,start))",
            ac->match.str->size);
        break;
    case FKL_TERM_STRING:
        CB_FMT("(matchLen=fklCharBufMatch(\"");
        build_string_in_hex(ac->match.str, build);
        CB_FMT(
            "\",%" FKL_PRT64U
            ",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len))>=0",
            ac->match.str->size);
        break;
    case FKL_TERM_REGEX: {
        uint64_t num = 0;
        fklGetStringWithRegex(&g->regexes, ac->match.re, &num);
        CB_FMT("regex_lex_match_for_parser_in_c((const FklRegexCode*)&");
        CB_FMT(PRINT_C_REGEX_PREFIX "%" FKL_PRT64X, num);
        CB_FMT(
            ",*in+otherMatchLen+skip_ignore_len,*restLen-otherMatchLen-skip_ignore_len,&matchLen,&is_waiting_for_more)");
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
        CB_FMT("(matchLen=1)", build);
        break;
    case FKL_TERM_NONE:
    case FKL_TERM_IGNORE:
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void build_state_action_to_c_file(
    const FklAnalysisStateAction *ac, const FklAnalysisState *states,
    const char *ast_destroyer_name, FklCodeBuilder *build) {
    CB_LINE("{");
    CB_INDENT(flag) {
        switch (ac->action) {
        case FKL_ANALYSIS_SHIFT:
            CB_LINE(
                "fklParseStateVectorPushBack2(stateStack,(FklParseState){.func=state_%" FKL_PRT64U
                "});",
                ac->state - states);
            CB_LINE(
                "init_term_analyzing_symbol(fklAnalysisSymbolVectorPushBack(symbolStack,NULL),*in+skip_ignore_len,matchLen,ctx->line,ctx->ctx);");
            CB_LINE("fklUintVectorPushBack2(lineStack,ctx->line);");
            CB_LINE(
                "ctx->line+=fklCountCharInBuf(*in,matchLen+skip_ignore_len,'\\n');");
            CB_LINE("*in+=matchLen+skip_ignore_len;");
            CB_LINE("*restLen-=matchLen+skip_ignore_len;");
            break;
        case FKL_ANALYSIS_ACCEPT:
            CB_LINE("*accept=1;");
            break;
        case FKL_ANALYSIS_REDUCE: {

            size_t delim_len = 0;
            for (size_t i = 0; i < ac->prod->len; ++i)
                if (is_delim_sym2(ac->prod, i))
                    ++delim_len;
            size_t actual_len = ac->prod->len - delim_len;

            CB_LINE("stateStack->size-=%" FKL_PRT64U ";", actual_len);
            CB_LINE("symbolStack->size-=%" FKL_PRT64U ";", actual_len);
            CB_LINE(
                "FklStateFuncPtr func=fklParseStateVectorBackNonNull(stateStack)->func;");
            CB_LINE("FklParseState nextState={.func=NULL};");
            CB_LINE("func(NULL,NULL,NULL,0,%" FKL_PRT64U
                    ",&nextState,NULL,NULL,NULL,NULL,NULL,NULL);",
                    ac->prod->left.sid);
            CB_LINE(
                "if(nextState.func == NULL) return FKL_PARSE_REDUCE_FAILED;");
            CB_LINE("fklParseStateVectorPushBack(stateStack,&nextState);");

            if (actual_len) {
                CB_LINE("void** nodes=(void**)fklZmalloc(%" FKL_PRT64U
                        "*sizeof(void*));",
                        actual_len);
                CB_LINE("FKL_ASSERT(nodes||!%" FKL_PRT64U ");", actual_len);
                CB_LINE(
                    "const FklAnalysisSymbol* base=&symbolStack->base[symbolStack->size];");
            } else
                CB_LINE("void** nodes=NULL;");

            if (actual_len)
                CB_LINE("for(size_t i=0;i<%" FKL_PRT64U
                        ";i++) nodes[i]=base[i].ast;",
                        actual_len);

            if (actual_len) {
                CB_LINE("size_t line=fklGetFirstNthLine(lineStack,%" FKL_PRT64U
                        ",ctx->line);",
                        actual_len);
                CB_LINE("lineStack->size-=%" FKL_PRT64U ";", actual_len);
            } else
                CB_LINE("size_t line=ctx->line;");

            CB_LINE("void* ast=prod_action_%s(ctx->ctx,nodes,%" FKL_PRT64U
                    ",line);\n",
                    ac->prod->name, actual_len);

            if (actual_len) {
                CB_LINE("for(size_t i=0;i<%" FKL_PRT64U ";i++) %s(nodes[i]);",
                        actual_len, ast_destroyer_name);
                CB_LINE("fklZfree(nodes);");
            }
            CB_LINE("if(!ast) {");
            CB_INDENT(flag) {
                CB_LINE("*errLine=line;");
                CB_LINE("return FKL_PARSE_REDUCE_FAILED;");
            }
            CB_LINE("}");

            CB_LINE(
                "init_nonterm_analyzing_symbol(fklAnalysisSymbolVectorPushBack(symbolStack,NULL),%" FKL_PRT64U
                ",ast);",
                ac->prod->left.sid);
            CB_LINE("fklUintVectorPushBack2(lineStack,line);");
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

static inline void
build_state_prototype_to_c_file(const FklAnalysisState *states, size_t idx,
                                FklCodeBuilder *build) {
    CB_LINE_START("static int state_%" FKL_PRT64U "(FklParseStateVector*", idx);
    CB_LINE(",FklAnalysisSymbolVector*");
    CB_LINE(",FklUintVector*");
    CB_LINE(",int");
    CB_LINE(",FklSid_t");
    CB_LINE(",FklParseState* pfunc");
    CB_LINE(",const char*");
    CB_LINE(",const char**");
    CB_LINE(",size_t*");
    CB_LINE(",FklGrammerMatchOuterCtx*");
    CB_LINE(",int*");
    CB_LINE_END(",size_t*);");
}

static inline void build_state_to_c_file(const FklAnalysisState *states,
                                         size_t idx, const FklGrammer *g,
                                         const char *ast_destroyer_name,
                                         FklCodeBuilder *build) {
    const FklAnalysisState *state = &states[idx];
    CB_LINE("static int state_%" FKL_PRT64U "(FklParseStateVector* stateStack",
            idx);
    CB_INDENT(flag) {
        CB_LINE(",FklAnalysisSymbolVector* symbolStack");
        CB_LINE(",FklUintVector* lineStack");
        CB_LINE(",int is_action");
        CB_LINE(",FklSid_t left");
        CB_LINE(",FklParseState* pfunc");
        CB_LINE(",const char* start");
        CB_LINE(",const char** in");
        CB_LINE(",size_t* restLen");
        CB_LINE(",FklGrammerMatchOuterCtx* ctx");
        CB_LINE(",int* accept");
        CB_LINE(",size_t* errLine) {");
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
                    build_state_action_to_c_file(ac, states, ast_destroyer_name,
                                                 build);
                    if (ac->match.allow_ignore
                        && ac->action != FKL_ANALYSIS_IGNORE) {
                        CB_LINE(
                            "else if(!has_tried_match_ignore && ((ignore_len==-1 ");
                        CB_INDENT(flag) {
                            CB_LINE(
                                "&& (ignore_len=match_ignore(*in+otherMatchLen,*restLen-otherMatchLen,&is_waiting_for_more))>0)");
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
                CB_LINE(
                    "return (is_waiting_for_more||(*restLen && *restLen==skip_ignore_len))?FKL_PARSE_WAITING_FOR_MORE:FKL_PARSE_TERMINAL_MATCH_FAILED;");
            } else
                CB_LINE("return FKL_PARSE_TERMINAL_MATCH_FAILED;");
            CB_LINE("(void)otherMatchLen;");
        }
        CB_LINE("}else{");
        CB_INDENT(flag) {
            const FklAnalysisStateGoto *gt = state->state.gt;
            if (gt) {
                if (gt->allow_ignore)
                    CB_LINE("if(1 && left==%" FKL_PRT64U "){", gt->nt.sid);
                else
                    CB_LINE("if(left==%" FKL_PRT64U "){", gt->nt.sid);
                CB_INDENT(flag) {
                    CB_LINE("pfunc->func=state_%" FKL_PRT64U ";",
                            gt->state - states);
                    CB_LINE("return 0;");
                }
                CB_LINE("}");

                gt = gt->next;
                for (; gt; gt = gt->next) {
                    if (gt->allow_ignore)
                        CB_LINE("else if(1 && left==%" FKL_PRT64U "){",
                                gt->nt.sid);
                    else
                        CB_LINE("else if(left==%" FKL_PRT64U "){", gt->nt.sid);
                    CB_INDENT(flag) {
                        CB_LINE("pfunc->func=state_%" FKL_PRT64U ";",
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
    return fklHash64Shift(FKL_TYPE_CAST(uintptr_t, *pk)                        \
                          / alignof(FklLalrBuiltinMatch));
#include <fakeLisp/hash.h>

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

static inline void
build_regex_lex_match_for_parser_in_c_to_c_file(FklCodeBuilder *build) {
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
        CB_LINE(
            "uint32_t len=fklRegexLexMatchp(re,cstr,restLen,&last_is_true);");
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
        CB_LINE("void* ctx)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("void* ast=%s(s,len,line,ctx);", name);
        CB_LINE("sym->nt.group=0;");
        CB_LINE("sym->nt.sid=0;");
        CB_LINE("sym->ast=ast;");
    }
    CB_LINE("}");
    CB_LINE("");
}

static inline void
build_init_nonterm_analyzing_symbol_src(FklCodeBuilder *build) {
    CB_LINE("static inline void");
    CB_LINE("init_nonterm_analyzing_symbol(FklAnalysisSymbol* sym,");
    CB_INDENT(flag) {
        CB_LINE("FklSid_t id,");
        CB_LINE("void* ast)");
    }
    CB_LINE("{");
    CB_INDENT(flag) {
        CB_LINE("sym->nt.group=0;");
        CB_LINE("sym->nt.sid=id;");
        CB_LINE("sym->ast=ast;");
    }
    CB_LINE("}");
    CB_LINE("");
}

void fklPrintAnalysisTableAsCfunc(const FklGrammer *g, const FklSymbolTable *st,
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

    build_init_nonterm_analyzing_symbol_src(build);

    build_match_ignore_prototype_to_c_file(build);

    CB_LINE("");

    if (g->sortedTerminals || g->sortedTerminalsNum != g->terminals.num) {
        build_get_max_non_term_length_prototype_to_c_file(build);
        CB_LINE("");
    }

    if (g->regexes.num) {
        build_all_regex(&g->regexes, build);
        build_regex_lex_match_for_parser_in_c_to_c_file(build);
        CB_LINE("");
    }

    if (g->sortedTerminalsNum != g->terminals.num) {
        build_match_char_buf_end_with_terminal_prototype_to_c_file(build);
        CB_LINE("");
    }

    build_all_builtin_match_func(g, build);
    size_t stateNum = g->aTable.num;
    const FklAnalysisState *states = g->aTable.states;
    if (g->sortedTerminals || g->sortedTerminalsNum != g->terminals.num) {
        build_get_max_non_term_length_to_c_file(g, build);
        CB_LINE("");
    }

    if (g->sortedTerminalsNum != g->terminals.num) {
        build_match_char_buf_end_with_terminal_to_c_file(build);
        CB_LINE("");
    }

    build_match_ignore_to_c_file(g, build);
    CB_LINE("");

    for (size_t i = 0; i < stateNum; i++)
        build_state_prototype_to_c_file(states, i, build);
    CB_LINE("");

    for (size_t i = 0; i < stateNum; i++)
        build_state_to_c_file(states, i, g, ast_destroyer_name, build);

    CB_LINE("void %s(FklParseStateVector* "
            "stateStack){",
            state_0_push_func_name);
    CB_INDENT(flag) {
        CB_LINE(
            "fklParseStateVectorPushBack2(stateStack,(FklParseState){.func=state_0});");
    }
    CB_LINE("}");
}

void fklPrintItemStateSet(const FklLalrItemSetHashMap *i, const FklGrammer *g,
                          const FklSymbolTable *st, FILE *fp) {
    GraItemStateIdxHashMap idxTable;
    graItemStateIdxHashMapInit(&idxTable);
    size_t idx = 0;
    for (const FklLalrItemSetHashMapNode *l = i->first; l; l = l->next, idx++) {
        graItemStateIdxHashMapPut2(&idxTable, &l->elm, idx);
    }
    for (const FklLalrItemSetHashMapNode *l = i->first; l; l = l->next) {
        const FklLalrItemHashSet *i = &l->k;
        idx = *graItemStateIdxHashMapGet2NonNull(&idxTable, &l->elm);
        fprintf(fp, "===\nI%" FKL_PRT64U ": ", idx);
        putc('\n', fp);
        fklPrintItemSet(i, g, st, fp);
        putc('\n', fp);
        for (FklLalrItemSetLink *ll = l->v.links; ll; ll = ll->next) {
            FklLalrItemSetHashMapElm *dst = ll->dst;
            size_t *c = graItemStateIdxHashMapGet2NonNull(&idxTable, dst);
            fprintf(fp, "I%" FKL_PRT64U "--{ ", idx);
            if (ll->allow_ignore)
                fputs("?e ", fp);
            print_prod_sym(fp, &ll->sym, st, &g->terminals, &g->regexes);
            fprintf(fp, " }-->I%" FKL_PRT64U "\n", *c);
        }
        putc('\n', fp);
    }
    graItemStateIdxHashMapUninit(&idxTable);
}

void fklPrintGrammerProduction(FILE *fp, const FklGrammerProduction *prod,
                               const FklSymbolTable *st,
                               const FklSymbolTable *tt,
                               const FklRegexTable *rt) {
    if (!is_Sq_nt(&prod->left))
        fklPrintString(fklGetSymbolWithId(prod->left.sid, st)->k, fp);
    else
        fputs("S'", fp);
    fputs(" ->", fp);
    size_t len = prod->len;
    FklGrammerSym *syms = prod->syms;
    for (size_t i = 0; i < len;) {
        putc(' ', fp);
        print_prod_sym(fp, &syms[i], st, tt, rt);
        ++i;
        if (i < len && syms[i].type != FKL_TERM_IGNORE)
            fputs(" ..", fp);
        else
            ++i;
    }
    putc('\n', fp);
}

const FklLalrBuiltinMatch *fklGetBuiltinMatch(const FklGraSidBuiltinHashMap *ht,
                                              FklSid_t id) {
    FklLalrBuiltinMatch const **i = fklGraSidBuiltinHashMapGet2(ht, id);
    if (i)
        return *i;
    return NULL;
}

int fklIsNonterminalExist(FklProdHashMap *prods, FklSid_t group_id,
                          FklSid_t sid) {
    return fklProdHashMapGet2(
               prods, (FklGrammerNonterm){.group = group_id, .sid = sid})
        != NULL;
}

FklGrammerProduction *fklGetGrammerProductions(const FklGrammer *g,
                                               FklSid_t group, FklSid_t sid) {
    return fklGetProductions(&g->productions, group, sid);
}

FklGrammerProduction *fklGetProductions(const FklProdHashMap *prods,
                                        FklSid_t group, FklSid_t sid) {
    FklGrammerProduction **pp = fklProdHashMapGet2(
        prods, (FklGrammerNonterm){.group = group, .sid = sid});
    return pp ? *pp : NULL;
}

static inline void print_ignores(const FklGrammerIgnore *ig,
                                 const FklRegexTable *rt, FILE *fp) {
    for (; ig; ig = ig->next) {
        for (size_t i = 0; i < ig->len; i++) {
            const FklGrammerIgnoreSym *u = &ig->ig[i];
            switch (u->term_type) {
            case FKL_TERM_BUILTIN: {
                fputs(u->b.t->name, fp);
                if (u->b.len) {
                    fputc('[', fp);
                    size_t i = 0;
                    for (; i < u->b.len - 1; ++i) {
                        fklPrintRawString(u->b.args[i], fp);
                        fputs(" , ", fp);
                    }
                    fklPrintRawString(u->b.args[i], fp);
                    fputc(']', fp);
                }
            } break;

            case FKL_TERM_REGEX:
                fprintf(fp, "/%s/",
                        fklGetStringWithRegex(rt, u->re, NULL)->str);
                break;
            case FKL_TERM_STRING: {
                fklPrintRawString(u->str, fp);
            } break;
            default:
                FKL_UNREACHABLE();
                break;
            }
            // if (igs->term_type == FKL_TERM_BUILTIN) {
            //     fputs(u->b.t->name, fp);
            //     if (u->b.len) {
            //         fputc('[', fp);
            //         size_t i = 0;
            //         for (; i < u->b.len - 1; ++i) {
            //             fklPrintRawString(u->b.args[i], fp);
            //             fputs(" , ", fp);
            //         }
            //         fklPrintRawString(u->b.args[i], fp);
            //         fputc(']', fp);
            //     }
            // } else if (igs->term_type == FKL_TERM_REGEX) {
            //     const FklString *str = fklGetStringWithRegex(rt, u->re,
            //     NULL); fklPrintRawCharBuf((const uint8_t *)str->str,
            //     str->size, "/",
            //                        "/", '/', fp);
            //     // fputs("/'", fp);
            //     // print_string_for_grapheasy(
            //     //     fklGetStringWithRegex(rt, igs->re, NULL), fp);
            //     // fputs("'/", fp);
            // } else {
            //     fputc('\'', fp);
            //     print_string_for_grapheasy(igs->str, fp);
            //     fputc('\'', fp);
            // }
            fputc(' ', fp);
        }
        fputc('\n', fp);
    }
}

void fklPrintGrammer(FILE *fp, const FklGrammer *grammer, FklSymbolTable *st) {
    const FklSymbolTable *tt = &grammer->terminals;
    const FklRegexTable *rt = &grammer->regexes;
    for (FklProdHashMapNode *list = grammer->productions.first; list;
         list = list->next) {
        FklGrammerProduction *prods = list->v;
        for (; prods; prods = prods->next) {
            fprintf(fp, "(%" FKL_PRT64U ") ", prods->idx);
            fklPrintGrammerProduction(fp, prods, st, tt, rt);
        }
    }
    fputs("\nignore:\n", fp);
    print_ignores(grammer->ignores, &grammer->regexes, fp);
}

void *fklDefaultParseForCstr(const char *cstr, FklGrammerMatchOuterCtx *ctx,
                             int *err, uint64_t *errLine,
                             FklAnalysisSymbolVector *symbolStack,
                             FklUintVector *lineStack,
                             FklParseStateVector *stateStack) {
    size_t restLen = strlen(cstr);
    return fklDefaultParseForCharBuf(cstr, restLen, &restLen, ctx, err, errLine,
                                     symbolStack, lineStack, stateStack);
}

void *fklDefaultParseForCharBuf(const char *cstr, size_t len, size_t *restLen,
                                FklGrammerMatchOuterCtx *ctx, int *err,
                                uint64_t *errLine,
                                FklAnalysisSymbolVector *symbolStack,
                                FklUintVector *lineStack,
                                FklParseStateVector *stateStack) {
    const char *start = cstr;
    *restLen = len;
    void *ast = NULL;
    for (;;) {
        int accept = 0;
        FklStateFuncPtr state =
            fklParseStateVectorBackNonNull(stateStack)->func;
        *err = state(stateStack, symbolStack, lineStack, 1, 0, NULL, start,
                     &cstr, restLen, ctx, &accept, errLine);
        if (*err)
            break;
        if (accept) {
            ast = fklAnalysisSymbolVectorPopBackNonNull(symbolStack)->ast;
            break;
        }
    }
    return ast;
}

static inline int match_char_buf_end_with_terminal(
    const FklString *laString, const char *cstr, size_t restLen,
    const FklGrammer *g, FklGrammerMatchOuterCtx *outerctx, const char *start) {
    size_t maxNonterminalLen =
        get_max_non_term_length(g, outerctx, start, cstr, restLen);
    return maxNonterminalLen == laString->size
        && fklStringCharBufMatch(laString, cstr, restLen) >= 0;
}

static inline size_t match_ignore(const FklGrammer *g,
                                  FklGrammerMatchOuterCtx *outerctx,
                                  const char *start, const char *cstr,
                                  size_t restLen, int *is_waiting_for_more) {
    size_t ret_len = 0;
    size_t matchLen = 0;
    for (; restLen > ret_len;) {
        int has_matched = 0;
        for (const FklGrammerIgnore *ig = g->ignores; ig; ig = ig->next) {
            if (ignore_match(g, ig, cstr, cstr + ret_len, restLen - ret_len,
                             &matchLen, outerctx, is_waiting_for_more)) {
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
                          const FklGrammer *g, FklGrammerMatchOuterCtx *ctx,
                          const char *start, const char *cstr, size_t restLen,
                          int *is_waiting_for_more,
                          FklStateActionMatchArgs *args) {
    args->skip_ignore_len = 0;
    int has_tried_match_ignore = 0;
match_start:
    switch (match->t) {
    case FKL_TERM_STRING: {
        const FklString *laString = match->str;
        if (fklStringCharBufMatch(laString, cstr + args->skip_ignore_len,
                                  restLen - args->skip_ignore_len)
            >= 0) {
            args->matchLen = laString->size;
            return 1;
        }
    } break;
    case FKL_TERM_KEYWORD: {
        const FklString *laString = match->str;
        if (match_char_buf_end_with_terminal(
                laString, cstr + args->skip_ignore_len,
                restLen - args->skip_ignore_len, g, ctx, start)) {
            args->matchLen = laString->size;
            return 1;
        }
    } break;

    case FKL_TERM_EOF:
        args->matchLen = 1;
        return 1;
        break;
    case FKL_TERM_BUILTIN: {
        FklBuiltinTerminalMatchArgs match_args = {
            .g = g, .len = match->func.len, .args = match->func.args};
        if (match->func.t->match(&match_args, start,
                                 cstr + args->skip_ignore_len,
                                 restLen - args->skip_ignore_len,
                                 &args->matchLen, ctx, is_waiting_for_more)) {
            return 1;
        }
    } break;
    case FKL_TERM_REGEX: {
        int last_is_true = 0;
        uint32_t len =
            fklRegexLexMatchp(match->re, cstr + args->skip_ignore_len,
                              restLen - args->skip_ignore_len, &last_is_true);
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
             && (args->ignore_len = match_ignore(g, ctx, start, cstr, restLen,
                                                 is_waiting_for_more))
                    > 0)
            || args->ignore_len > 0)) {
        has_tried_match_ignore = 1;
        args->skip_ignore_len = (size_t)args->ignore_len;
        goto match_start;
    }

    return 0;
}

static inline void init_nonterm_analyzing_symbol(FklAnalysisSymbol *sym,
                                                 FklSid_t group, FklSid_t id,
                                                 FklNastNode *ast) {
    sym->nt.group = group;
    sym->nt.sid = id;
    sym->ast = ast;
}

static inline int do_reduce_action(FklParseStateVector *stateStack,
                                   FklAnalysisSymbolVector *symbolStack,
                                   FklUintVector *lineStack,
                                   const FklGrammerProduction *prod, size_t len,
                                   FklGrammerMatchOuterCtx *ctx,
                                   FklSymbolTable *st, size_t *errLine) {
    stateStack->size -= len;
    FklAnalysisStateGoto *gt =
        fklParseStateVectorBackNonNull(stateStack)->state->state.gt;
    const FklAnalysisState *state = NULL;
    FklGrammerNonterm left = prod->left;
    for (; gt; gt = gt->next)
        if (fklNontermEqual(&gt->nt, &left))
            state = gt->state;
    if (!state)
        return 1;
    symbolStack->size -= len;
    void **nodes = NULL;
    if (len) {
        nodes = (void **)fklZmalloc(len * sizeof(void *));
        FKL_ASSERT(nodes);
        const FklAnalysisSymbol *base = &symbolStack->base[symbolStack->size];
        for (size_t i = 0; i < len; i++) {
            nodes[i] = base[i].ast;
        }
    }
    size_t line = fklGetFirstNthLine(lineStack, len, ctx->line);
    lineStack->size -= len;
    void *ast = prod->func(prod->ctx, ctx->ctx, nodes, len, line);
    if (len) {
        for (size_t i = 0; i < len; i++)
            ctx->destroy(nodes[i]);
        fklZfree(nodes);
    }
    if (!ast) {
        *errLine = line;
        return FKL_PARSE_REDUCE_FAILED;
    }
    init_nonterm_analyzing_symbol(
        fklAnalysisSymbolVectorPushBack(symbolStack, NULL), left.group,
        left.sid, ast);
    fklUintVectorPushBack2(lineStack, line);
    fklParseStateVectorPushBack2(stateStack, (FklParseState){.state = state});
    return 0;
}

void *fklParseWithTableForCstr(const FklGrammer *g, const char *cstr,
                               FklGrammerMatchOuterCtx *ctx, FklSymbolTable *st,
                               int *err) {
    return fklParseWithTableForCharBuf(g, cstr, strlen(cstr), ctx, st, err);
}

void *fklParseWithTableForCharBuf(const FklGrammer *g, const char *cstr,
                                  size_t restLen, FklGrammerMatchOuterCtx *ctx,
                                  FklSymbolTable *st, int *err) {
    const FklAnalysisTable *t = &g->aTable;
    size_t errLine = 0;
    FklAnalysisSymbolVector symbolStack;
    FklParseStateVector stateStack;
    FklUintVector lineStack;
    fklParseStateVectorInit(&stateStack, 16);
    fklAnalysisSymbolVectorInit(&symbolStack, 16);
    fklUintVectorInit(&lineStack, 16);
    fklParseStateVectorPushBack2(&stateStack,
                                 (FklParseState){.state = &t->states[0]});

    void *ast = fklParseWithTableForCharBuf2(g, cstr, restLen, &restLen, ctx,
                                             st, err, &errLine, &symbolStack,
                                             &lineStack, &stateStack);

    fklUintVectorUninit(&lineStack);
    fklParseStateVectorUninit(&stateStack);
    while (!fklAnalysisSymbolVectorIsEmpty(&symbolStack))
        fklDestroyNastNode(
            fklAnalysisSymbolVectorPopBackNonNull(&symbolStack)->ast);
    fklAnalysisSymbolVectorUninit(&symbolStack);
    return ast;
}

#define PARSE_INCLUDED
#include "parse.h"
#undef PARSE_INCLUDED

uint64_t fklGetFirstNthLine(FklUintVector *lineStack, size_t num, size_t line) {
    if (num)
        return lineStack->base[lineStack->size - num];
    else
        return line;
}

#include "grammer/action.h"

static const char builtin_grammer_rules[] = {
#include "lisp.g.h"
    '\0',
};
// clang-format off
// static const FklGrammerCstrAction builtin_grammer_and_action[] = {
//     {"*s-exp* &*list*",                                       "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*vector*",                                     "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*bytevector*",                                 "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*box*",                                        "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*hasheq*",                                     "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*hasheqv*",                                    "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*hashequal*",                                  "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*string*",                                     "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*symbol*",                                     "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*integer*",                                    "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*float*",                                      "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*char*",                                       "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*quote*",                                      "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*qsquote*",                                    "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*unquote*",                                    "prod_action_return_first",  prod_action_return_first  },
//     {"*s-exp* &*unqtesp*",                                    "prod_action_return_first",  prod_action_return_first  },
//
//     {"*quote* #' &*s-exp*",                                   "prod_action_quote",         prod_action_quote         },
//     {"*qsquote* #` &*s-exp*",                                 "prod_action_qsquote",       prod_action_qsquote       },
//     {"*unquote* #~ &*s-exp*",                                 "prod_action_unquote",       prod_action_unquote       },
//     {"*unqtesp* #~@ &*s-exp*",                                "prod_action_unqtesp",       prod_action_unqtesp       },
//
//     {"*symbol* &?symbol + #|",                                "prod_action_symbol",        prod_action_symbol        },
//
//     {"*string* /\"\"|^\"(\\\\.|.)*\"$",                       "prod_action_string",        prod_action_string        },
//
//     {"*bytevector* /#\"\"|^#\"(\\\\.|.)*\"$",                 "prod_action_bytevector",    prod_action_bytevector    },
//
//     // s-dint can accepet the next terminator as its arguement
//     {"*integer* &?s-dint + #|",                               "prod_action_dec_integer",   prod_action_dec_integer   },
//     {"*integer* &?s-xint + #|",                               "prod_action_hex_integer",   prod_action_hex_integer   },
//     {"*integer* &?s-oint + #|",                               "prod_action_oct_integer",   prod_action_oct_integer   },
//
//     {"*float* &?s-dfloat + #|",                               "prod_action_float",         prod_action_float         },
//     {"*float* &?s-xfloat + #|",                               "prod_action_float",         prod_action_float         },
//
//     {"*char* &?s-char + ##\\",                                "prod_action_char",          prod_action_char          },
//
//     {"*box* ##& &*s-exp*",                                    "prod_action_box",           prod_action_box           },
//
//     {"*list* #( &*list-items* #)",                            "prod_action_return_second", prod_action_return_second },
//     {"*list* #[ &*list-items* #]",                            "prod_action_return_second", prod_action_return_second },
//
//     {"*list-items* ",                                         "prod_action_nil",           prod_action_nil           },
//     {"*list-items* &*s-exp* &*list-items*",                   "prod_action_list",          prod_action_list          },
//     {"*list-items* &*s-exp* #, &*s-exp*",                     "prod_action_pair",          prod_action_pair          },
//
//     {"*vector* ##( &*vector-items* #)",                       "prod_action_vector",        prod_action_vector        },
//     {"*vector* ##[ &*vector-items* #]",                       "prod_action_vector",        prod_action_vector        },
//
//     {"*vector-items* ",                                       "prod_action_nil",           prod_action_nil           },
//     {"*vector-items* &*s-exp* &*vector-items*",               "prod_action_list",          prod_action_list          },
//
//     {"*hasheq* ##hash( &*hash-items* #)",                     "prod_action_hasheq",        prod_action_hasheq        },
//     {"*hasheq* ##hash[ &*hash-items* #]",                     "prod_action_hasheq",        prod_action_hasheq        },
//
// 	// {"*hasheq* ##hash + &*hasheq-items*",                     "prod_action_return_second", prod_action_return_second},
// 	// {"*hasheq-items* #( &*hash-items* #)",                     "prod_action_hasheq", prod_action_hasheq},
// 	// {"*hasheq-items* #[ &*hash-items* #]",                     "prod_action_hasheq", prod_action_hasheq},
//
//     {"*hasheqv* ##hasheqv( &*hash-items* #)",                 "prod_action_hasheqv",       prod_action_hasheqv       },
//     {"*hasheqv* ##hasheqv[ &*hash-items* #]",                 "prod_action_hasheqv",       prod_action_hasheqv       },
//
//     {"*hashequal* ##hashequal( &*hash-items* #)",             "prod_action_hashequal",     prod_action_hashequal     },
//     {"*hashequal* ##hashequal[ &*hash-items* #]",             "prod_action_hashequal",     prod_action_hashequal     },
//
//     {"*hash-items* ",                                         "prod_action_nil",           prod_action_nil           },
//     {"*hash-items* #( &*s-exp* #, &*s-exp* #) &*hash-items*", "prod_action_kv_list",       prod_action_kv_list       },
//     {"*hash-items* #[ &*s-exp* #, &*s-exp* #] &*hash-items*", "prod_action_kv_list",       prod_action_kv_list       },
//
//     {"+ /\\s+",                                               NULL,                        NULL                      },
//     {"+ /^;.*\\n?",                                           NULL,                        NULL                      },
//     {"+ /^#!.*\\n?",                                          NULL,                        NULL                      },
//     {NULL,                                                    NULL,                        NULL                      },
// };
// clang-format on

void fklInitBuiltinGrammer(FklGrammer *g, FklSymbolTable *st) {
    FklParserGrammerParseArg args;
    fklInitEmptyGrammer(g, st);

    fklInitParserGrammerParseArg(&args, g, 1, builtin_prod_action_resolver,
                                 NULL);
    int err = fklParseProductionRuleWithCstr(&args, builtin_grammer_rules);
    if (err) {
        fklPrintParserGrammerParseError(err, &args, stderr);
        fklDestroyGrammer(g);
        FKL_UNREACHABLE();
        return;
    }
    fklUninitParserGrammerParseArg(&args);
    if (fklCheckAndInitGrammerSymbols(g)) {
        fklDestroyGrammer(g);
        FKL_UNREACHABLE();
        return;
    }
}

FklGrammer *fklCreateBuiltinGrammer(FklSymbolTable *st) {
    FklGrammer *g = create_grammer();
    fklInitBuiltinGrammer(g, st);
    // return fklCreateGrammerFromCstrAction(builtin_grammer_and_action, st);
    return g;
}

FklGrammerIgnore *fklInitBuiltinProductionSet(FklGrammer *g) {
    FklParserGrammerParseArg args;
    fklInitParserGrammerParseArg(&args, g, 1, builtin_prod_action_resolver,
                                 NULL);
    int err = fklParseProductionRuleWithCstr(&args, builtin_grammer_rules);
    if (err) {
        fklPrintParserGrammerParseError(err, &args, stderr);
        fklDestroyGrammer(g);
        FKL_UNREACHABLE();
        return NULL;
    }
    fklUninitParserGrammerParseArg(&args);
    return g->ignores;
    // FklGrammerIgnore *ignores = NULL;
    // const FklGrammerCstrAction *pa = builtin_grammer_and_action;
    // for (; pa->cstr; pa++) {
    //     const char *str = pa->cstr;
    //     if (*str == '+') {
    //         FklGrammerIgnore *ignore =
    //             create_grammer_ignore_from_cstr(str, builtins, st, tt, rt);
    //         if (!ignore)
    //             return NULL;
    //         if (fklAddIgnoreToIgnoreList(&ignores, ignore)) {
    //             fklDestroyIgnore(ignore);
    //             return NULL;
    //         }
    //     } else {
    //         FklGrammerProduction *prod = create_grammer_prod_from_cstr(
    //             str, builtins, st, tt, rt, pa->action_name, pa->func);
    //         if (prod == NULL || fklAddProdToProdTable(ht, builtins, prod)) {
    //             fklDestroyGrammerProduction(prod);
    //             return NULL;
    //         }
    //     }
    // }
    // return ignores;
}

static inline void replace_group_id(FklGrammerProduction *prod,
                                    FklSid_t new_id) {
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

int fklMergeGrammer(FklGrammer *g, const FklGrammer *other,
                    const FklRecomputeGroupIdArgs *args) {
    for (const FklGrammerIgnore *ig = other->ignores; ig; ig = ig->next) {
        FklGrammerIgnore *new_ig = fklCreateEmptyGrammerIgnore(ig->len);
        for (size_t i = 0; i < ig->len; ++i) {
            const FklGrammerIgnoreSym *from = &ig->ig[i];
            FklGrammerIgnoreSym *to = &new_ig->ig[i];

            to->term_type = from->term_type;
            switch (from->term_type) {
            case FKL_TERM_STRING:
                to->str = fklAddSymbol(from->str, &g->terminals)->k;
                fklAddSymbol(from->str, &g->reachable_terminals);
                break;
            case FKL_TERM_REGEX: {
                const FklString *regex_str =
                    fklGetStringWithRegex(&other->regexes, from->re, NULL);
                to->re = fklAddRegexStr(&g->regexes, regex_str);
            } break;
            case FKL_TERM_BUILTIN: {
                to->b.t = from->b.t;
                to->b.len = from->b.len;
                FklString const **args =
                    fklZmalloc(to->b.len * sizeof(FklString *));
                FKL_ASSERT(args);
                for (size_t i = 0; i < from->b.len; ++i) {
                    args[i] = fklAddSymbol(from->b.args[i], &g->terminals)->k;
                    fklAddSymbol(args[i], &g->reachable_terminals);
                }
                to->b.args = args;
                if (to->b.t->ctx_create
                    && to->b.t->ctx_create(to->b.len, to->b.args, g)) {
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

            if (fklAddIgnoreToIgnoreList(&g->ignores, new_ig))
                fklDestroyIgnore(new_ig);
        }
    }

    for (const FklProdHashMapNode *prods = other->productions.first; prods;
         prods = prods->next) {
        for (const FklGrammerProduction *prod = prods->v; prod;
             prod = prod->next) {

            FklGrammerProduction *new_prod = fklCreateEmptyProduction(
                prod->left.group, prod->left.sid, prod->len, prod->name,
                prod->func, prod->ctx_copyer(prod->ctx), prod->ctx_destroyer,
                prod->ctx_copyer);

            FklGrammerSym *syms = new_prod->syms;
            for (size_t i = 0; i < prod->len; ++i) {
                const FklGrammerSym *from = &prod->syms[i];
                FklGrammerSym *to = &syms[i];

                to->type = from->type;
                switch (from->type) {

                case FKL_TERM_NONTERM:
                    to->nt.group = from->nt.group;
                    to->nt.sid = from->nt.sid;
                    break;
                case FKL_TERM_REGEX: {
                    const FklString *regex_str =
                        fklGetStringWithRegex(&other->regexes, from->re, NULL);
                    to->re = fklAddRegexStr(&g->regexes, regex_str);
                } break;

                case FKL_TERM_STRING: {
                    const FklString *from_str =
                        fklGetSymbolWithId(from->nt.sid, &other->terminals)->k;
                    to->nt.group = 0;
                    to->nt.sid = fklAddSymbol(from_str, &g->terminals)->v;
                    fklAddSymbol(from_str, &g->reachable_terminals);
                } break;

                case FKL_TERM_KEYWORD: {
                    const FklString *from_str =
                        fklGetSymbolWithId(from->nt.sid, &other->terminals)->k;
                    to->nt.group = 0;
                    to->nt.sid = fklAddSymbol(from_str, &g->terminals)->v;
                } break;

                case FKL_TERM_BUILTIN: {
                    to->b.t = from->b.t;
                    to->b.len = from->b.len;
                    FklString const **args =
                        fklZmalloc(to->b.len * sizeof(FklString *));
                    FKL_ASSERT(args);
                    for (size_t i = 0; i < from->b.len; ++i) {
                        args[i] =
                            fklAddSymbol(from->b.args[i], &g->terminals)->k;
                        fklAddSymbol(args[i], &g->reachable_terminals);
                    }
                    to->b.args = args;
                    if (to->b.t->ctx_create
                        && to->b.t->ctx_create(to->b.len, to->b.args, g)) {
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
            if (fklAddProdToProdTableNoRepeat(g, new_prod))
                fklDestroyGrammerProduction(new_prod);
        }
    }

    for (size_t i = 0; i < other->reachable_terminals.num; ++i)
        fklAddSymbol(other->reachable_terminals.idl[i]->k,
                     &g->reachable_terminals);
    return 0;
}
