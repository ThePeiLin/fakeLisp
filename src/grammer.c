#include <fakeLisp/base.h>
#include <fakeLisp/bigint.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/utils.h>

#include <ctype.h>
#include <stdalign.h>
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
        if (s->term_type == FKL_TERM_BUILTIN && s->b.c && s->b.t->ctx_destroy)
            s->b.t->ctx_destroy(s->b.c);
    }
}

void fklDestroyGrammerProduction(FklGrammerProduction *h) {
    if (h == NULL)
        return;
    h->ctx_destroyer(h->ctx);
    fklUninitGrammerSymbols(h->syms, h->len);
    free(h->syms);
    free(h);
}

static inline void destroy_builtin_grammer_sym(FklLalrBuiltinGrammerSym *s) {
    if (s->t->ctx_destroy && s->c)
        s->t->ctx_destroy(s->c);
}

FklGrammerProduction *fklCreateProduction(FklSid_t group, FklSid_t sid,
                                          size_t len, FklGrammerSym *syms,
                                          const char *name,
                                          FklProdActionFunc func, void *ctx,
                                          void (*destroy)(void *),
                                          void *(*copyer)(const void *)) {
    FklGrammerProduction *r =
        (FklGrammerProduction *)calloc(1, sizeof(FklGrammerProduction));
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
    ;
    return r;
}

FklGrammerProduction *fklCreateEmptyProduction(FklSid_t group, FklSid_t sid,
                                               size_t len, const char *name,
                                               FklProdActionFunc func,
                                               void *ctx,
                                               void (*destroy)(void *),
                                               void *(*copyer)(const void *)) {
    FklGrammerProduction *r =
        (FklGrammerProduction *)calloc(1, sizeof(FklGrammerProduction));
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
        r->syms = (FklGrammerSym *)calloc(len, sizeof(FklGrammerSym));
        FKL_ASSERT(r->syms);
    }
    return r;
}

#define DEFINE_DEFAULT_C_MATCH_COND(NAME)                                      \
    static void builtin_match_##NAME##_print_c_match_cond(                     \
        void *ctx, const FklGrammer *g, FILE *fp) {                            \
        fputs("builtin_match_" #NAME "(start,*in+otherMatchLen,*restLen-"      \
              "otherMatchLen,&matchLen,outerCtx)",                             \
              fp);                                                             \
    }

static void default_builtin_match_print_src(const FklGrammer *g, FILE *fp) {
    fputs("", fp);
}

static inline const FklGrammerSym *sym_at_idx(const FklGrammerProduction *prod,
                                              size_t i) {
    if (i < prod->len)
        return &prod->syms[i];
    return NULL;
}

static inline void *init_builtin_grammer_sym(const FklLalrBuiltinMatch *m,
                                             size_t i,
                                             FklGrammerProduction *prod,
                                             FklGrammer *g, int *failed) {
    if (m->ctx_global_create)
        return m->ctx_global_create(i, prod, g, failed);
    if (m->ctx_create) {
        const FklGrammerSym *sym = sym_at_idx(prod, i + 1);
        if (sym && (!sym->isterm || sym->term_type != FKL_TERM_STRING)) {
            *failed = 1;
            return NULL;
        }
        const FklString *next =
            sym ? fklGetSymbolWithId(sym->nt.sid, &g->terminals)->k : NULL;
        return m->ctx_create(next, failed);
    }
    return NULL;
}

void fklProdCtxDestroyDoNothing(void *c) {}

void fklProdCtxDestroyFree(void *c) { free(c); }

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
            free(str);
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
            free(str);
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
    const char *str, FklGraSidBuiltinTable *builtins,
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
        if (ss[0] == '+' || ss[0] == '-')
            joint_num++;
        if (len)
            fklStringVectorPushBack2(&st, fklCreateString(len, ss));
        ss += len;
    }
    size_t prod_len = st.size - joint_num;
    FklGrammerProduction *prod = fklCreateEmptyProduction(
        0, left, prod_len, name, func, NULL, fklProdCtxDestroyDoNothing,
        fklProdCtxCopyerDoNothing);
    int next_delim = 1;
    int next_end_with_term = 0;
    size_t symIdx = 0;
    if (prod == NULL || prod_len == 0)
        goto exit;
    for (uint32_t i = 0; i < st.size; i++) {
        FklGrammerSym *u = &prod->syms[symIdx];
        u->term_type = FKL_TERM_STRING;
        FklString *s = st.base[i];
        switch (*(s->str)) {
        case '+':
            next_delim = 0;
            continue;
            break;
        case '-':
            next_end_with_term = 1;
            continue;
            break;
        case '#':
            u->isterm = 1;
            u->nt.group = 0;
            u->nt.sid =
                fklAddSymbolCharBuf(&s->str[1], s->size - 1, termTable)->v;
            break;
        case '/': {
            u->isterm = 1;
            u->term_type = FKL_TERM_REGEX;
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
                u->isterm = 1;
                u->term_type = FKL_TERM_BUILTIN;
                u->b.t = builtin;
                u->b.c = NULL;
            } else {
                u->isterm = 0;
                u->nt.group = 0;
                u->nt.sid = id;
            }
        } break;
        }
        u->delim = next_delim;
        u->end_with_terminal = next_end_with_term;
        next_end_with_term = 0;
        next_delim = 1;
        symIdx++;
    }
exit:
    while (!fklStringVectorIsEmpty(&st))
        free(*fklStringVectorPopBack(&st));
    fklStringVectorUninit(&st);
    return prod;
}

FklGrammerIgnore *fklGrammerSymbolsToIgnore(FklGrammerSym *syms, size_t len,
                                            const FklSymbolTable *tt) {
    for (size_t i = len; i > 0; i--) {
        FklGrammerSym *sym = &syms[i - 1];
        if (!sym->isterm)
            return NULL;
        if (sym->term_type == FKL_TERM_BUILTIN) {
            int failed = 0;
            FklLalrBuiltinGrammerSym *b = &sym->b;
            if (b->t->ctx_global_create && !b->t->ctx_create)
                return NULL;
            if (b->c)
                destroy_builtin_grammer_sym(b);
            if (b->t->ctx_create) {
                FklGrammerSym *sym = (i < len) ? &syms[i] : NULL;
                if (sym && (!sym->isterm || sym->term_type != FKL_TERM_STRING))
                    return NULL;
                const FklString *next =
                    sym ? fklGetSymbolWithId(sym->nt.sid, tt)->k : NULL;
                b->c = b->t->ctx_create(next, &failed);
            }
            if (failed)
                return NULL;
        }
    }
    FklGrammerIgnore *ig = (FklGrammerIgnore *)malloc(
        sizeof(FklGrammerIgnore) + len * sizeof(FklGrammerIgnoreSym));
    FKL_ASSERT(ig);
    ig->len = len;
    ig->next = NULL;
    FklGrammerIgnoreSym *igss = ig->ig;
    for (size_t i = 0; i < len; i++) {
        FklGrammerSym *sym = &syms[i];
        FklGrammerIgnoreSym *igs = &igss[i];
        igs->term_type = sym->term_type;
        if (igs->term_type == FKL_TERM_BUILTIN)
            igs->b = sym->b;
        else {
            if (sym->end_with_terminal) {
                free(ig);
                return NULL;
            }
            if (igs->term_type == FKL_TERM_REGEX)
                igs->re = sym->re;
            else
                igs->str = fklGetSymbolWithId(sym->nt.sid, tt)->k;
        }
    }
    return ig;
}

static inline FklGrammerIgnore *create_grammer_ignore_from_cstr(
    const char *str, FklGraSidBuiltinTable *builtins,
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
            free(*fklStringVectorPopBack(&st));
        fklStringVectorUninit(&st);
        return NULL;
    }
    FklGrammerProduction *prod = fklCreateEmptyProduction(
        0, left, prod_len, NULL, NULL, NULL, fklProdCtxDestroyDoNothing,
        fklProdCtxCopyerDoNothing);
    int next_delim = 1;
    size_t symIdx = 0;
    for (uint32_t i = 0; i < st.size; i++) {
        FklGrammerSym *u = &prod->syms[symIdx];
        u->term_type = FKL_TERM_STRING;
        u->delim = next_delim;
        next_delim = 1;
        u->end_with_terminal = 0;
        FklString *s = st.base[i];
        switch (*(s->str)) {
        case '+':
            next_delim = 0;
            continue;
            break;
        case '#':
            u->isterm = 1;
            u->nt.group = 0;
            u->nt.sid =
                fklAddSymbolCharBuf(&s->str[1], s->size - 1, termTable)->v;
            break;
        case '/': {
            u->isterm = 1;
            u->term_type = FKL_TERM_REGEX;
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
                u->isterm = 1;
                u->term_type = FKL_TERM_BUILTIN;
                u->b.t = builtin;
                u->b.c = NULL;
            } else {
                u->isterm = 0;
                u->nt.group = 0;
                u->nt.sid = id;
            }
        } break;
        }
        symIdx++;
    }
    while (!fklStringVectorIsEmpty(&st))
        free(*fklStringVectorPopBack(&st));
    fklStringVectorUninit(&st);
    FklGrammerIgnore *ig =
        fklGrammerSymbolsToIgnore(prod->syms, prod->len, termTable);
    fklDestroyGrammerProduction(prod);
    return ig;
error:
    while (!fklStringVectorIsEmpty(&st))
        free(*fklStringVectorPopBack(&st));
    fklStringVectorUninit(&st);
    return NULL;
}

static inline int prod_sym_equal(const FklGrammerSym *u0,
                                 const FklGrammerSym *u1) {
    if (u0->term_type == u1->term_type && u0->delim == u1->delim
        && u0->end_with_terminal == u1->end_with_terminal
        && u0->isterm == u1->isterm) {
        if (u0->term_type == FKL_TERM_BUILTIN) {
            if (u0->b.t == u1->b.t) {
                if (u0->b.t->ctx_equal)
                    return u0->b.t->ctx_equal(u0->b.c, u1->b.c);
                return 1;
            }
            return 0;
        } else if (u0->term_type == FKL_TERM_REGEX)
            return u0->re == u1->re;
        else
            return u0->nt.group == u1->nt.group && u0->nt.sid == u1->nt.sid;
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

static inline FklGrammerProduction *create_extra_production(FklSid_t group,
                                                            FklSid_t start) {
    FklGrammerProduction *prod = fklCreateEmptyProduction(
        0, 0, 1, NULL, NULL, NULL, fklProdCtxDestroyDoNothing,
        fklProdCtxCopyerDoNothing);
    prod->idx = 0;
    FklGrammerSym *u = &prod->syms[0];
    u->delim = 1;
    u->end_with_terminal = 0;
    u->term_type = FKL_TERM_STRING;
    u->isterm = 0;
    u->nt.group = group;
    u->nt.sid = start;
    return prod;
}

FklGrammerProduction *
fklCopyUninitedGrammerProduction(FklGrammerProduction *prod) {
    FklGrammerProduction *r = fklCreateEmptyProduction(
        prod->left.group, prod->left.sid, prod->len, prod->name, prod->func,
        prod->ctx, prod->ctx_destroyer, prod->ctx_copyer);
    FklGrammerSym *ss = r->syms;
    FklGrammerSym *oss = prod->syms;
    memcpy(ss, oss, prod->len * sizeof(FklGrammerSym));
    r->ctx = r->ctx_copyer(prod->ctx);
    r->next = NULL;
    return r;
}

int fklAddProdAndExtraToGrammer(FklGrammer *g, FklGrammerProduction *prod) {
    const FklGraSidBuiltinTable *builtins = &g->builtins;
    FklProdTable *productions = &g->productions;
    const FklGrammerNonterm *left = &prod->left;
    if (left->group == 0 && fklGetBuiltinMatch(builtins, left->sid))
        return 1;
    FklGrammerProduction **pp = fklProdTableGet(productions, left);
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
            pp = fklProdTableAdd(productions, &extra_prod->left, NULL);
            *pp = extra_prod;
            (*pp)->idx = g->prodNum;
            g->prodNum++;
        }
        prod->next = NULL;
        pp = fklProdTableAdd(productions, left, NULL);
        prod->idx = g->prodNum;
        *pp = prod;
        g->prodNum++;
    }
    return 0;
}

int fklAddProdToProdTable(FklProdTable *productions,
                          FklGraSidBuiltinTable *builtins,
                          FklGrammerProduction *prod) {
    const FklGrammerNonterm *left = &prod->left;
    if (left->group == 0 && fklGetBuiltinMatch(builtins, left->sid))
        return 1;
    FklGrammerProduction **pp = fklProdTableGet(productions, left);
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
            prod->idx = 0;
            prod->next = NULL;
            *pp = prod;
        }
    } else {
        prod->next = NULL;
        pp = fklProdTableAdd(productions, left, NULL);
        prod->idx = 0;
        *pp = prod;
    }
    return 0;
}

int fklAddProdToProdTableNoRepeat(FklProdTable *productions,
                                  FklGraSidBuiltinTable *builtins,
                                  FklGrammerProduction *prod) {
    const FklGrammerNonterm *left = &prod->left;
    if (left->group == 0 && fklGetBuiltinMatch(builtins, left->sid))
        return 1;
    FklGrammerProduction **pp = fklProdTableGet(productions, left);
    if (pp) {
        FklGrammerProduction *cur = NULL;
        for (; *pp; pp = &((*pp)->next))
            if (prod_equal(*pp, prod))
                return 1;
        if (cur) {
            prod->next = cur->next;
            *pp = prod;
            fklDestroyGrammerProduction(cur);
        } else {
            prod->idx = 0;
            prod->next = NULL;
            *pp = prod;
        }
    } else {
        prod->next = NULL;
        pp = fklProdTableAdd(productions, left, NULL);
        prod->idx = 0;
        *pp = prod;
    }
    return 0;
}

static inline int builtin_grammer_sym_cmp(const FklLalrBuiltinGrammerSym *b0,
                                          const FklLalrBuiltinGrammerSym *b1) {
    if (b0->t->ctx_cmp)
        return b0->t->ctx_cmp(b0->c, b1->c);
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
    if (s0->isterm > s1->isterm)
        return -1;
    else if (s0->isterm < s1->isterm)
        return 1;
    else if (s0->term_type < s1->term_type)
        return -1;
    else if (s0->term_type > s1->term_type)
        return 1;
    else {
        int r = 0;
        if (s0->term_type == FKL_TERM_BUILTIN) {
            if (s0->b.t < s1->b.t)
                return -1;
            else if (s0->b.t > s1->b.t)
                return 1;
            else if ((r = builtin_grammer_sym_cmp(&s0->b, &s1->b)))
                return r;
        } else if (s0->term_type == FKL_TERM_REGEX) {
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
        else {
            unsigned int f0 = (s0->delim << 1);
            unsigned int f1 = (s1->delim << 1);
            if (f0 < f1)
                return -1;
            else if (f0 > f1)
                return 1;
        }
    }
    return 0;
}

static inline void print_string_in_hex(const FklString *stri, FILE *fp) {
    size_t size = stri->size;
    const char *str = stri->str;
    for (size_t i = 0; i < size; i++)
        fprintf(fp, "\\x%02X", str[i]);
}

static inline int ignore_match(FklGrammerIgnore *ig, const char *start,
                               const char *str, size_t restLen,
                               size_t *pmatchLen,
                               FklGrammerMatchOuterCtx *outerCtx,
                               int *is_waiting_for_more) {
    size_t matchLen = 0;
    FklGrammerIgnoreSym *igss = ig->ig;
    size_t len = ig->len;
    for (size_t i = 0; i < len; i++) {
        FklGrammerIgnoreSym *ig = &igss[i];
        if (ig->term_type == FKL_TERM_BUILTIN) {
            size_t len = 0;
            if (ig->b.t->match(ig->b.c, start, str, restLen - matchLen, &len,
                               outerCtx, is_waiting_for_more)) {
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
            if (fklStringCharBufMatch(laString, str, restLen - matchLen)) {
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
                if (ignore_match(ig, start, cur, restLen - len, &matchLen, ctx,
                                 &is_waiting_for_more))
                    goto break_loop;
            }
            for (size_t i = 0; i < num; i++)
                if (fklStringCharBufMatch(terms[i], cur, restLen - len))
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

static int builtin_match_dec_int_func(void *ctx, const char *start,
                                      const char *cstr, size_t restLen,
                                      size_t *pmatchLen,
                                      FklGrammerMatchOuterCtx *outerCtx,
                                      int *is_waiting_for_more) {
    if (restLen) {
        size_t maxLen =
            get_max_non_term_length(ctx, outerCtx, start, cstr, restLen);
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
        terms = (const FklString **)malloc(num * sizeof(FklString *));
        FKL_ASSERT(terms);
        FklStrIdTableElm **symList = g->reachable_terminals.idl;
        for (size_t i = 0; i < num; i++)
            terms[i] = symList[i]->k;
        qsort(terms, num, sizeof(FklString *), string_len_cmp);
    }
    g->sortedTerminals = terms;
}

static void *builtin_match_number_global_create(size_t idx,
                                                FklGrammerProduction *prod,
                                                FklGrammer *g, int *failed) {
    if (!g->sortedTerminals)
        sort_reachable_terminals(g);
    return g;
}

DEFINE_DEFAULT_C_MATCH_COND(dec_int);

static void builtin_match_dec_int_print_src(const FklGrammer *g, FILE *fp) {
    fputs("static int builtin_match_dec_int(const char* start\n"
          "\t\t,const char* cstr\n"
          "\t\t,size_t restLen\n"
          "\t\t,size_t* pmatchLen\n"
          "\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
          "{\n"
          "\tif(restLen)\n"
          "\t{\n"
          "\t\tsize_t "
          "maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"
          "\t\tif(maxLen&&fklIsDecInt(cstr,maxLen))\n"
          "\t\t{\n"
          "\t\t\t*pmatchLen=maxLen;\n"
          "\t\t\treturn 1;\n"
          "\t\t}\n"
          "\t}\n"
          "\treturn 0;\n"
          "}\n",
          fp);
}

static const FklLalrBuiltinMatch builtin_match_dec_int = {
    .name = "?dint",
    .match = builtin_match_dec_int_func,
    .ctx_global_create = builtin_match_number_global_create,
    .print_src = builtin_match_dec_int_print_src,
    .print_c_match_cond = builtin_match_dec_int_print_c_match_cond,
};

static int builtin_match_hex_int_func(void *ctx, const char *start,
                                      const char *cstr, size_t restLen,
                                      size_t *pmatchLen,
                                      FklGrammerMatchOuterCtx *outerCtx,
                                      int *is_waiting_for_more) {
    if (restLen) {
        size_t maxLen =
            get_max_non_term_length(ctx, outerCtx, start, cstr, restLen);
        if (maxLen && fklIsHexInt(cstr, maxLen)) {
            *pmatchLen = maxLen;
            return 1;
        }
    }
    return 0;
}

static void builtin_match_hex_int_print_src(const FklGrammer *g, FILE *fp) {
    fputs("static int builtin_match_hex_int(const char* start\n"
          "\t\t,const char* cstr\n"
          "\t\t,size_t restLen\n"
          "\t\t,size_t* pmatchLen\n"
          "\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
          "{\n"
          "\tif(restLen)\n"
          "\t{\n"
          "\t\tsize_t "
          "maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"
          "\t\tif(maxLen&&fklIsHexInt(cstr,maxLen))\n"
          "\t\t{\n"
          "\t\t\t*pmatchLen=maxLen;\n"
          "\t\t\treturn 1;\n"
          "\t\t}\n"
          "\t}\n"
          "\treturn 0;\n"
          "}\n",
          fp);
}

DEFINE_DEFAULT_C_MATCH_COND(hex_int);

static const FklLalrBuiltinMatch builtin_match_hex_int = {
    .name = "?xint",
    .match = builtin_match_hex_int_func,
    .ctx_global_create = builtin_match_number_global_create,
    .print_src = builtin_match_hex_int_print_src,
    .print_c_match_cond = builtin_match_hex_int_print_c_match_cond,
};

static int builtin_match_oct_int_func(void *ctx, const char *start,
                                      const char *cstr, size_t restLen,
                                      size_t *pmatchLen,
                                      FklGrammerMatchOuterCtx *outerCtx,
                                      int *is_waiting_for_more) {
    if (restLen) {
        size_t maxLen =
            get_max_non_term_length(ctx, outerCtx, start, cstr, restLen);
        if (maxLen && fklIsOctInt(cstr, maxLen)) {
            *pmatchLen = maxLen;
            return 1;
        }
    }
    return 0;
}

static void builtin_match_oct_int_print_src(const FklGrammer *g, FILE *fp) {
    fputs("static int builtin_match_oct_int(const char* start\n"
          "\t\t,const char* cstr\n"
          "\t\t,size_t restLen\n"
          "\t\t,size_t* pmatchLen\n"
          "\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
          "{\n"
          "\tif(restLen)\n"
          "\t{\n"
          "\t\tsize_t "
          "maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"
          "\t\tif(maxLen&&fklIsOctInt(cstr,maxLen))\n"
          "\t\t{\n"
          "\t\t\t*pmatchLen=maxLen;\n"
          "\t\t\treturn 1;\n"
          "\t\t}\n"
          "\t}\n"
          "\treturn 0;\n"
          "}\n",
          fp);
}

DEFINE_DEFAULT_C_MATCH_COND(oct_int);

static const FklLalrBuiltinMatch builtin_match_oct_int = {
    .name = "?oint",
    .match = builtin_match_oct_int_func,
    .ctx_global_create = builtin_match_number_global_create,
    .print_src = builtin_match_oct_int_print_src,
    .print_c_match_cond = builtin_match_oct_int_print_c_match_cond,
};

static int builtin_match_dec_float_func(void *ctx, const char *start,
                                        const char *cstr, size_t restLen,
                                        size_t *pmatchLen,
                                        FklGrammerMatchOuterCtx *outerCtx,
                                        int *is_waiting_for_more) {
    if (restLen) {
        size_t maxLen =
            get_max_non_term_length(ctx, outerCtx, start, cstr, restLen);
        if (maxLen && fklIsDecFloat(cstr, maxLen)) {
            *pmatchLen = maxLen;
            return 1;
        }
    }
    return 0;
}

static void builtin_match_dec_float_print_src(const FklGrammer *g, FILE *fp) {
    fputs("static int builtin_match_dec_float(const char* start\n"
          "\t\t,const char* cstr\n"
          "\t\t,size_t restLen\n"
          "\t\t,size_t* pmatchLen\n"
          "\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
          "{\n"
          "\tif(restLen)\n"
          "\t{\n"
          "\t\tsize_t "
          "maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"
          "\t\tif(maxLen&&fklIsDecFloat(cstr,maxLen))\n"
          "\t\t{\n"
          "\t\t\t*pmatchLen=maxLen;\n"
          "\t\t\treturn 1;\n"
          "\t\t}\n"
          "\t}\n"
          "\treturn 0;\n"
          "}\n",
          fp);
}

DEFINE_DEFAULT_C_MATCH_COND(dec_float);

static const FklLalrBuiltinMatch builtin_match_dec_float = {
    .name = "?dfloat",
    .match = builtin_match_dec_float_func,
    .ctx_global_create = builtin_match_number_global_create,
    .print_src = builtin_match_dec_float_print_src,
    .print_c_match_cond = builtin_match_dec_float_print_c_match_cond,
};

static int builtin_match_hex_float_func(void *ctx, const char *start,
                                        const char *cstr, size_t restLen,
                                        size_t *pmatchLen,
                                        FklGrammerMatchOuterCtx *outerCtx,
                                        int *is_waiting_for_more) {
    if (restLen) {
        size_t maxLen =
            get_max_non_term_length(ctx, outerCtx, start, cstr, restLen);
        if (maxLen && fklIsHexFloat(cstr, maxLen)) {
            *pmatchLen = maxLen;
            return 1;
        }
    }
    return 0;
}

static void builtin_match_hex_float_print_src(const FklGrammer *g, FILE *fp) {
    fputs("static int builtin_match_hex_float(const char* start\n"
          "\t\t,const char* cstr\n"
          "\t\t,size_t restLen\n"
          "\t\t,size_t* pmatchLen\n"
          "\t\t,FklGrammerMatchOuterCtx* outerCtx)\n"
          "{\n"
          "\tif(restLen)\n"
          "\t{\n"
          "\t\tsize_t "
          "maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"
          "\t\tif(maxLen&&fklIsHexFloat(cstr,maxLen))\n"
          "\t\t{\n"
          "\t\t\t*pmatchLen=maxLen;\n"
          "\t\t\treturn 1;\n"
          "\t\t}\n"
          "\t}\n"
          "\treturn 0;\n"
          "}\n",
          fp);
}

DEFINE_DEFAULT_C_MATCH_COND(hex_float);

static const FklLalrBuiltinMatch builtin_match_hex_float = {
    .name = "?xfloat",
    .match = builtin_match_hex_float_func,
    .ctx_global_create = builtin_match_number_global_create,
    .print_src = builtin_match_hex_float_print_src,
    .print_c_match_cond = builtin_match_hex_float_print_c_match_cond,
};

static int builtin_match_identifier_func(void *c, const char *cstrStart,
                                         const char *cstr, size_t restLen,
                                         size_t *pmatchLen,
                                         FklGrammerMatchOuterCtx *outerCtx,
                                         int *is_waiting_for_more) {
    size_t maxLen =
        get_max_non_term_length(c, outerCtx, cstrStart, cstr, restLen);
    if (!maxLen || fklIsDecInt(cstr, maxLen) || fklIsOctInt(cstr, maxLen)
        || fklIsHexInt(cstr, maxLen) || fklIsDecFloat(cstr, maxLen)
        || fklIsHexFloat(cstr, maxLen) || fklIsAllDigit(cstr, maxLen))
        return 0;
    *pmatchLen = maxLen;
    return 1;
}

static void builtin_match_identifier_print_src(const FklGrammer *g, FILE *fp) {
    fputs("static int builtin_match_identifier(const char* cstrStart\n"
          "\t,const char* cstr\n"
          "\t,size_t restLen\n"
          "\t,size_t* pmatchLen\n"
          "\t,FklGrammerMatchOuterCtx* outerCtx)\n"
          "{\n"
          "\tsize_t "
          "maxLen=get_max_non_term_length(outerCtx,cstrStart,cstr,restLen);\n"
          "\tif(!maxLen\n"
          "\t\t\t||fklIsDecInt(cstr,maxLen)\n"
          "\t\t\t||fklIsOctInt(cstr,maxLen)\n"
          "\t\t\t||fklIsHexInt(cstr,maxLen)\n"
          "\t\t\t||fklIsDecFloat(cstr,maxLen)\n"
          "\t\t\t||fklIsHexFloat(cstr,maxLen)\n"
          "\t\t\t||fklIsAllDigit(cstr,maxLen))\n"
          "\t\treturn 0;\n"
          "\t*pmatchLen=maxLen;\n"
          "\treturn 1;\n"
          "}\n",
          fp);
}

DEFINE_DEFAULT_C_MATCH_COND(identifier);

static const FklLalrBuiltinMatch builtin_match_identifier = {
    .name = "?identifier",
    .match = builtin_match_identifier_func,
    .ctx_global_create = builtin_match_number_global_create,
    .print_src = builtin_match_identifier_print_src,
    .print_c_match_cond = builtin_match_identifier_print_c_match_cond,
};

DEFINE_DEFAULT_C_MATCH_COND(noterm);

static void builtin_match_noterm_print_src(const FklGrammer *g, FILE *fp) {
    fputs("static int builtin_match_noterm(const char* cstrStart\n"
          "\t,const char* cstr\n"
          "\t,size_t restLen\n"
          "\t,size_t* pmatchLen\n"
          "\t,FklGrammerMatchOuterCtx* outerCtx)\n"
          "{\n"
          "\tsize_t "
          "maxLen=get_max_non_term_length(outerCtx,cstrStart,cstr,restLen);\n"
          "\tif(!maxLen)\n"
          "\t\treturn 0;\n"
          "\t*pmatchLen=maxLen;\n"
          "\treturn 1;\n"
          "}\n",
          fp);
}

static int builtin_match_noterm_func(void *c, const char *cstrStart,
                                     const char *cstr, size_t restLen,
                                     size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *outerCtx,
                                     int *is_waiting_for_more) {
    size_t maxLen =
        get_max_non_term_length(c, outerCtx, cstrStart, cstr, restLen);
    if (!maxLen)
        return 0;
    *pmatchLen = maxLen;
    return 1;
}

static const FklLalrBuiltinMatch builtin_match_noterm = {
    .name = "?noterm",
    .match = builtin_match_noterm_func,
    .ctx_global_create = builtin_match_number_global_create,
    .print_src = builtin_match_noterm_print_src,
    .print_c_match_cond = builtin_match_noterm_print_c_match_cond,
};

typedef struct {
    const FklGrammer *g;
    const FklString *start;
} SymbolNumberCtx;

static void *s_number_ctx_global_create(size_t idx, FklGrammerProduction *prod,
                                        FklGrammer *g, int *failed) {
    if (prod->len > 2 || idx != 0) {
        *failed = 1;
        return NULL;
    }
    const FklString *start = NULL;
    if (prod->len == 2) {
        if (!prod->syms[1].isterm || prod->syms[1].term_type != FKL_TERM_STRING
            || prod->syms[1].end_with_terminal) {
            *failed = 1;
            return NULL;
        }
        start = fklGetSymbolWithId(prod->syms[1].nt.sid, &g->terminals)->k;
    }
    prod->len = 1;
    SymbolNumberCtx *ctx = (SymbolNumberCtx *)malloc(sizeof(SymbolNumberCtx));
    FKL_ASSERT(ctx);
    ctx->start = start;
    ctx->g = g;
    if (!g->sortedTerminals)
        sort_reachable_terminals(g);
    return ctx;
}

static int s_number_ctx_cmp(const void *c0, const void *c1) {
    const SymbolNumberCtx *ctx0 = c0;
    const SymbolNumberCtx *ctx1 = c1;
    if (ctx0->start == ctx1->start)
        return 0;
    else if (ctx0 == NULL)
        return -1;
    else if (ctx1 == NULL)
        return 1;
    else
        return fklStringCmp(ctx0->start, ctx1->start);
}

static int s_number_ctx_equal(const void *c0, const void *c1) {
    const SymbolNumberCtx *ctx0 = c0;
    const SymbolNumberCtx *ctx1 = c1;
    if (ctx0->start == ctx1->start)
        return 1;
    else if (ctx0 == NULL || ctx1 == NULL)
        return 0;
    else
        return !fklStringCmp(ctx0->start, ctx1->start);
}

static uintptr_t s_number_ctx_hash(const void *c0) {
    const SymbolNumberCtx *ctx0 = c0;
    return (uintptr_t)ctx0->start;
}

static void s_number_ctx_destroy(void *c) { free(c); }

#define SYMBOL_NUMBER_MATCH(F)                                                 \
    SymbolNumberCtx *ctx = c;                                                  \
    if (restLen) {                                                             \
        size_t maxLen =                                                        \
            get_max_non_term_length(ctx->g, outerCtx, start, cstr, restLen);   \
        if (maxLen                                                             \
            && (!ctx->start                                                    \
                || !fklStringCharBufMatch(ctx->start, cstr + maxLen,           \
                                          restLen - maxLen))                   \
            && F(cstr, maxLen)) {                                              \
            *pmatchLen = maxLen;                                               \
            return 1;                                                          \
        }                                                                      \
    }                                                                          \
    return 0

static int builtin_match_s_dint_func(void *c, const char *start,
                                     const char *cstr, size_t restLen,
                                     size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *outerCtx,
                                     int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsDecInt);
}

#define DEFINE_LISP_NUMBER_PRINT_SRC(NAME, F)                                  \
    static void builtin_match_##NAME##_print_src(const FklGrammer *g,          \
                                                 FILE *fp) {                   \
        fputs("static int builtin_match_" #NAME "(const char* start\n"         \
              "\t\t,const char* cstr\n"                                        \
              "\t\t,size_t restLen\n"                                          \
              "\t\t,size_t* pmatchLen\n"                                       \
              "\t\t,FklGrammerMatchOuterCtx* outerCtx\n"                       \
              "\t\t,const char* term\n"                                        \
              "\t\t,size_t termLen)\n"                                         \
              "{\n"                                                            \
              "\tif(restLen)\n"                                                \
              "\t{\n"                                                          \
              "\t\tsize_t "                                                    \
              "maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n" \
              "\t\tif(maxLen&&(!term||!fklCharBufMatch(term,termLen,cstr+"     \
              "maxLen,restLen-maxLen))\n"                                      \
              "\t\t\t\t&&" #F "(cstr,maxLen))\n"                               \
              "\t\t{\n"                                                        \
              "\t\t\t*pmatchLen=maxLen;\n"                                     \
              "\t\t\treturn 1;\n"                                              \
              "\t\t}\n"                                                        \
              "\t}\n"                                                          \
              "\treturn 0;\n"                                                  \
              "}\n",                                                           \
              fp);                                                             \
    }

#define DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(NAME)                            \
    static void builtin_match_##NAME##_print_c_match_cond(                     \
        void *c, const FklGrammer *g, FILE *fp) {                              \
        fputs("builtin_match_" #NAME "(start,*in+otherMatchLen,*restLen-"      \
              "otherMatchLen,&matchLen,outerCtx,",                             \
              fp);                                                             \
        SymbolNumberCtx *ctx = c;                                              \
        if (ctx->start) {                                                      \
            fputc('\"', fp);                                                   \
            print_string_in_hex(ctx->start, fp);                               \
            fputc('\"', fp);                                                   \
            fprintf(fp, ",%" FKL_PRT64U "", ctx->start->size);                 \
        } else                                                                 \
            fputs("NULL,0", fp);                                               \
        fputc(')', fp);                                                        \
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

static int builtin_match_s_xint_func(void *c, const char *start,
                                     const char *cstr, size_t restLen,
                                     size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *outerCtx,
                                     int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsHexInt);
}

static int builtin_match_s_oint_func(void *c, const char *start,
                                     const char *cstr, size_t restLen,
                                     size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *outerCtx,
                                     int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsOctInt);
}

static int builtin_match_s_dfloat_func(void *c, const char *start,
                                       const char *cstr, size_t restLen,
                                       size_t *pmatchLen,
                                       FklGrammerMatchOuterCtx *outerCtx,
                                       int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsDecFloat);
}

static int builtin_match_s_xfloat_func(void *c, const char *start,
                                       const char *cstr, size_t restLen,
                                       size_t *pmatchLen,
                                       FklGrammerMatchOuterCtx *outerCtx,
                                       int *is_waiting_for_more) {
    SYMBOL_NUMBER_MATCH(fklIsHexFloat);
}

#undef SYMBOL_NUMBER_MATCH

static const FklLalrBuiltinMatch builtin_match_s_dint = {
    .name = "?s-dint",
    .match = builtin_match_s_dint_func,
    .ctx_global_create = s_number_ctx_global_create,
    .ctx_cmp = s_number_ctx_cmp,
    .ctx_equal = s_number_ctx_equal,
    .ctx_hash = s_number_ctx_hash,
    .ctx_destroy = s_number_ctx_destroy,
    .print_src = builtin_match_s_dint_print_src,
    .print_c_match_cond = builtin_match_s_dint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_xint = {
    .name = "?s-xint",
    .match = builtin_match_s_xint_func,
    .ctx_global_create = s_number_ctx_global_create,
    .ctx_cmp = s_number_ctx_cmp,
    .ctx_equal = s_number_ctx_equal,
    .ctx_hash = s_number_ctx_hash,
    .ctx_destroy = s_number_ctx_destroy,
    .print_src = builtin_match_s_xint_print_src,
    .print_c_match_cond = builtin_match_s_xint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_oint = {
    .name = "?s-oint",
    .match = builtin_match_s_oint_func,
    .ctx_global_create = s_number_ctx_global_create,
    .ctx_cmp = s_number_ctx_cmp,
    .ctx_equal = s_number_ctx_equal,
    .ctx_hash = s_number_ctx_hash,
    .ctx_destroy = s_number_ctx_destroy,
    .print_src = builtin_match_s_oint_print_src,
    .print_c_match_cond = builtin_match_s_oint_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_dfloat = {
    .name = "?s-dfloat",
    .match = builtin_match_s_dfloat_func,
    .ctx_global_create = s_number_ctx_global_create,
    .ctx_cmp = s_number_ctx_cmp,
    .ctx_equal = s_number_ctx_equal,
    .ctx_hash = s_number_ctx_hash,
    .ctx_destroy = s_number_ctx_destroy,
    .print_src = builtin_match_s_dfloat_print_src,
    .print_c_match_cond = builtin_match_s_dfloat_print_c_match_cond,
};

static const FklLalrBuiltinMatch builtin_match_s_xfloat = {
    .name = "?s-xfloat",
    .match = builtin_match_s_xfloat_func,
    .ctx_global_create = s_number_ctx_global_create,
    .ctx_cmp = s_number_ctx_cmp,
    .ctx_equal = s_number_ctx_equal,
    .ctx_hash = s_number_ctx_hash,
    .ctx_destroy = s_number_ctx_destroy,
    .print_src = builtin_match_s_xfloat_print_src,
    .print_c_match_cond = builtin_match_s_xfloat_print_c_match_cond,
};

static int builtin_match_s_char_func(void *c, const char *start,
                                     const char *cstr, size_t restLen,
                                     size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *outerCtx,
                                     int *is_waiting_for_more) {
    const SymbolNumberCtx *ctx = c;
    const FklString *prefix = ctx->start;
    size_t minLen = prefix->size + 1;
    if (restLen < minLen)
        return 0;
    if (!fklStringCharBufMatch(prefix, cstr, restLen))
        return 0;
    restLen -= prefix->size;
    cstr += prefix->size;
    size_t maxLen =
        get_max_non_term_length(ctx->g, outerCtx, start, cstr, restLen);
    if (!maxLen)
        *pmatchLen = prefix->size + 1;
    else
        *pmatchLen = prefix->size + maxLen;
    return 1;
}

static void builtin_match_s_char_print_src(const FklGrammer *g, FILE *fp) {
    fputs("static int builtin_match_s_char(const char* start\n"
          "\t\t,const char* cstr\n"
          "\t\t,size_t restLen\n"
          "\t\t,size_t* pmatchLen\n"
          "\t\t,FklGrammerMatchOuterCtx* outerCtx\n"
          "\t\t,const char* prefix\n"
          "\t\t,size_t prefix_size)\n"
          "{\n"
          "\tsize_t minLen=prefix_size+1;\n"
          "\tif(restLen<minLen)\n"
          "\t\treturn 0;\n"
          "\tif(!fklCharBufMatch(prefix,prefix_size,cstr,restLen))\n"
          "\t\treturn 0;\n"
          "\trestLen-=prefix_size;\n"
          "\tcstr+=prefix_size;\n"
          "\tsize_t "
          "maxLen=get_max_non_term_length(outerCtx,start,cstr,restLen);\n"
          "\tif(!maxLen)\n"
          "\t\t*pmatchLen=prefix_size+1;\n"
          "\telse\n"
          "\t\t*pmatchLen=prefix_size+maxLen;\n"
          "\treturn 1;\n"
          "}\n",
          fp);
}

DEFINE_LISP_NUMBER_PRINT_C_MATCH_COND(s_char);

static void *s_char_ctx_global_create(size_t idx, FklGrammerProduction *prod,
                                      FklGrammer *g, int *failed) {
    if (prod->len < 2 || prod->len > 2 || idx != 0) {
        *failed = 1;
        return NULL;
    }
    const FklString *start = NULL;
    if (!prod->syms[1].isterm || prod->syms[1].term_type != FKL_TERM_STRING
        || prod->syms[1].end_with_terminal) {
        *failed = 1;
        return NULL;
    }
    start = fklGetSymbolWithId(prod->syms[1].nt.sid, &g->terminals)->k;
    prod->len = 1;
    SymbolNumberCtx *ctx = (SymbolNumberCtx *)malloc(sizeof(SymbolNumberCtx));
    FKL_ASSERT(ctx);
    ctx->start = start;
    ctx->g = g;
    if (!g->sortedTerminals)
        sort_reachable_terminals(g);
    return ctx;
}

static const FklLalrBuiltinMatch builtin_match_s_char = {
    .name = "?char",
    .match = builtin_match_s_char_func,
    .ctx_global_create = s_char_ctx_global_create,
    .ctx_cmp = s_number_ctx_cmp,
    .ctx_equal = s_number_ctx_equal,
    .ctx_hash = s_number_ctx_hash,
    .ctx_destroy = s_number_ctx_destroy,
    .print_src = builtin_match_s_char_print_src,
    .print_c_match_cond = builtin_match_s_char_print_c_match_cond,
};

typedef struct {
    const FklGrammer *g;
    const FklString *start;
    const FklString *end;
} MatchSymbolCtx;

size_t fklQuotedStringMatch(const char *cstr, size_t restLen,
                            const FklString *end) {
    if (restLen < end->size)
        return 0;
    size_t matchLen = 0;
    size_t len = 0;
    for (; len < restLen; len++) {
        if (cstr[len] == '\\') {
            len++;
            continue;
        }
        if (fklStringCharBufMatch(end, &cstr[len], restLen - len)) {
            matchLen += len + end->size;
            return matchLen;
        }
    }
    return 0;
}

size_t fklQuotedCharBufMatch(const char *cstr, size_t restLen, const char *end,
                             size_t end_size) {
    if (restLen < end_size)
        return 0;
    size_t matchLen = 0;
    size_t len = 0;
    for (; len < restLen; len++) {
        if (cstr[len] == '\\') {
            len++;
            continue;
        }
        if (fklCharBufMatch(end, end_size, &cstr[len], restLen - len)) {
            matchLen += len + end_size;
            return matchLen;
        }
    }
    return 0;
}

static int builtin_match_symbol_func(void *c, const char *cstrStart,
                                     const char *cstr, size_t restLen,
                                     size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *outerCtx,
                                     int *is_waiting_for_more) {
    MatchSymbolCtx *ctx = c;
    const FklString *start = ctx->start;
    const FklString *end = ctx->end;
    if (start) {
        size_t matchLen = 0;
        for (;;) {
            if (fklStringCharBufMatch(start, cstr, restLen - matchLen)) {
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
            size_t maxLen = get_max_non_term_length(ctx->g, outerCtx, cstrStart,
                                                    cstr, restLen - matchLen);
            if ((!matchLen && !maxLen)
                || (!fklStringCharBufMatch(start, cstr + maxLen,
                                           restLen - maxLen - matchLen)
                    && maxLen
                    && (fklIsDecInt(cstr, maxLen) || fklIsOctInt(cstr, maxLen)
                        || fklIsHexInt(cstr, maxLen)
                        || fklIsDecFloat(cstr, maxLen)
                        || fklIsHexFloat(cstr, maxLen)
                        || fklIsAllDigit(cstr, maxLen))))
                return 0;
            matchLen += maxLen;
            cstr += maxLen;
            if (!fklStringCharBufMatch(start, cstr, restLen - matchLen))
                break;
        }
        *pmatchLen = matchLen;
        return matchLen != 0;
    } else {
        size_t maxLen =
            get_max_non_term_length(ctx->g, outerCtx, cstrStart, cstr, restLen);
        if (!maxLen || fklIsDecInt(cstr, maxLen) || fklIsOctInt(cstr, maxLen)
            || fklIsHexInt(cstr, maxLen) || fklIsDecFloat(cstr, maxLen)
            || fklIsHexFloat(cstr, maxLen) || fklIsAllDigit(cstr, maxLen))
            return 0;
        *pmatchLen = maxLen;
    }
    return 1;
}

static void builtin_match_symbol_print_src(const FklGrammer *g, FILE *fp) {
    fputs("static int builtin_match_symbol(const char* cstrStart\n"
          "\t\t,const char* cstr\n"
          "\t\t,size_t restLen\n"
          "\t\t,size_t* pmatchLen\n"
          "\t\t,FklGrammerMatchOuterCtx* outerCtx\n"
          "\t\t,int* is_waiting_for_more\n"
          "\t\t,const char* start\n"
          "\t\t,size_t start_size\n"
          "\t\t,const char* end\n"
          "\t\t,size_t end_size)\n"
          "{\n"
          "\tif(start)\n"
          "\t{\n"
          "\t\tsize_t matchLen=0;\n"
          "\t\tfor(;;)\n"
          "\t\t{\n"
          "\t\t\tif(fklCharBufMatch(start,start_size,cstr,restLen-matchLen))\n"
          "\t\t\t{\n"
          "\t\t\t\tmatchLen+=start_size;\n"
          "\t\t\t\tcstr+=start_size;\n"
          "\t\t\t\tsize_t "
          "len=fklQuotedCharBufMatch(cstr,restLen-matchLen,end,end_size);\n"
          "\t\t\t\tif(!len)\n"
          "\t\t\t\t{\n"
          "\t\t\t\t\t*is_waiting_for_more=1;\n"
          "\t\t\t\t\treturn 0;\n"
          "\t\t\t\t}\n"
          "\t\t\t\tmatchLen+=len;\n"
          "\t\t\t\tcstr+=len;\n"
          "\t\t\t\tcontinue;\n"
          "\t\t\t}\n"
          "\t\t\tsize_t "
          "maxLen=get_max_non_term_length(outerCtx,cstrStart,cstr,restLen-"
          "matchLen);\n"
          "\t\t\tif((!matchLen&&!maxLen)\n"
          "\t\t\t\t\t||(!fklCharBufMatch(start,start_size,cstr+maxLen,restLen-"
          "maxLen-matchLen)\n"
          "\t\t\t\t\t\t&&maxLen\n"
          "\t\t\t\t\t\t&&(fklIsDecInt(cstr,maxLen)\n"
          "\t\t\t\t\t\t\t||fklIsOctInt(cstr,maxLen)\n"
          "\t\t\t\t\t\t\t||fklIsHexInt(cstr,maxLen)\n"
          "\t\t\t\t\t\t\t||fklIsDecFloat(cstr,maxLen)\n"
          "\t\t\t\t\t\t\t||fklIsHexFloat(cstr,maxLen)\n"
          "\t\t\t\t\t\t\t||fklIsAllDigit(cstr,maxLen))))\n"
          "\t\t\t\treturn 0;\n"
          "\t\t\tmatchLen+=maxLen;\n"
          "\t\t\tcstr+=maxLen;\n"
          "\t\t\tif(!fklCharBufMatch(start,start_size,cstr,restLen-matchLen))\n"
          "\t\t\t\tbreak;\n"
          "\t\t}\n"
          "\t\t*pmatchLen=matchLen;\n"
          "\t\treturn matchLen!=0;\n"
          "\t}\n"
          "\telse\n"
          "\t{\n"
          "\t\tsize_t "
          "maxLen=get_max_non_term_length(outerCtx,cstrStart,cstr,restLen);\n"
          "\t\tif(!maxLen\n"
          "\t\t\t\t||fklIsDecInt(cstr,maxLen)\n"
          "\t\t\t\t||fklIsOctInt(cstr,maxLen)\n"
          "\t\t\t\t||fklIsHexInt(cstr,maxLen)\n"
          "\t\t\t\t||fklIsDecFloat(cstr,maxLen)\n"
          "\t\t\t\t||fklIsHexFloat(cstr,maxLen)\n"
          "\t\t\t\t||fklIsAllDigit(cstr,maxLen))\n"
          "\t\t\treturn 0;\n"
          "\t\t*pmatchLen=maxLen;\n"
          "\t}\n"
          "\treturn 1;\n"
          "}\n",
          fp);
}

static void builtin_match_symbol_print_c_match_cond(void *c,
                                                    const FklGrammer *g,
                                                    FILE *fp) {
    MatchSymbolCtx *ctx = c;
    const FklString *start = ctx->start;
    const FklString *end = ctx->end;
    fputs("builtin_match_symbol(start,*in+otherMatchLen,*restLen-otherMatchLen,"
          "&matchLen,outerCtx,&is_waiting_for_more,",
          fp);
    fputc('\"', fp);
    print_string_in_hex(start, fp);
    fprintf(fp, "\",%" FKL_PRT64U ",\"", start->size);
    print_string_in_hex(end, fp);
    fprintf(fp, "\",%" FKL_PRT64U ")", end->size);
}

static int builtin_match_symbol_cmp(const void *c0, const void *c1) {
    const MatchSymbolCtx *ctx0 = c0;
    const MatchSymbolCtx *ctx1 = c1;
    if (ctx0->start == ctx1->start && ctx0->end == ctx1->end)
        return 0;
    if (ctx0->start == NULL && ctx1->start)
        return -1;
    else if (ctx1->start == NULL)
        return 1;
    else {
        int r = fklStringCmp(ctx0->start, ctx1->start);
        if (r)
            return r;
        return fklStringCmp(ctx0->end, ctx1->end);
    }
}

static int builtin_match_symbol_equal(const void *c0, const void *c1) {
    const MatchSymbolCtx *ctx0 = c0;
    const MatchSymbolCtx *ctx1 = c1;
    if (ctx0->start == ctx1->start && ctx0->end == ctx1->end)
        return 1;
    else if (ctx0->start || ctx1->start)
        return !fklStringCmp(ctx0->start, ctx1->start)
            && !fklStringCmp(ctx0->end, ctx1->end);
    return 0;
}

static void builtin_match_symbol_destroy(void *c) { free(c); }

static void *builtin_match_symbol_global_create(size_t idx,
                                                FklGrammerProduction *prod,
                                                FklGrammer *g, int *failed) {
    if (prod->len > 3 || idx != 0) {
        *failed = 1;
        return NULL;
    }
    const FklString *start = NULL;
    const FklString *end = NULL;
    if (prod->len == 2) {
        if (!prod->syms[1].isterm || prod->syms[1].term_type != FKL_TERM_STRING
            || prod->syms[1].end_with_terminal) {
            *failed = 1;
            return NULL;
        }
        start = fklGetSymbolWithId(prod->syms[1].nt.sid, &g->terminals)->k;
        end = start;
    } else if (prod->len == 3) {
        if (!prod->syms[1].isterm || prod->syms[1].term_type != FKL_TERM_STRING
            || prod->syms[1].end_with_terminal || !prod->syms[2].isterm
            || prod->syms[2].term_type != FKL_TERM_STRING
            || prod->syms[2].end_with_terminal) {
            *failed = 1;
            return NULL;
        }
        start = fklGetSymbolWithId(prod->syms[1].nt.sid, &g->terminals)->k;
        end = fklGetSymbolWithId(prod->syms[2].nt.sid, &g->terminals)->k;
    }
    prod->len = 1;
    MatchSymbolCtx *ctx = (MatchSymbolCtx *)malloc(sizeof(MatchSymbolCtx));
    FKL_ASSERT(ctx);
    ctx->start = start;
    ctx->end = end;
    ctx->g = g;
    if (!g->sortedTerminals)
        sort_reachable_terminals(g);
    return ctx;
}

static uintptr_t builtin_match_symbol_hash(const void *c) {
    const MatchSymbolCtx *ctx = c;
    return (uintptr_t)ctx->start + (uintptr_t)ctx->end;
}

static const FklLalrBuiltinMatch builtin_match_symbol = {
    .name = "?symbol",
    .match = builtin_match_symbol_func,
    .ctx_cmp = builtin_match_symbol_cmp,
    .ctx_equal = builtin_match_symbol_equal,
    .ctx_hash = builtin_match_symbol_hash,
    .ctx_global_create = builtin_match_symbol_global_create,
    .ctx_destroy = builtin_match_symbol_destroy,
    .print_src = builtin_match_symbol_print_src,
    .print_c_match_cond = builtin_match_symbol_print_c_match_cond,
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

    {"?symbol",     &builtin_match_symbol     },
    {"?char",       &builtin_match_s_char     },
    {"?identifier", &builtin_match_identifier },
    {"?noterm",     &builtin_match_noterm     },

    {NULL,          NULL                      },
    // clang-format on
};

void fklInitBuiltinGrammerSymTable(FklGraSidBuiltinTable *s,
                                   FklSymbolTable *st) {
    fklGraSidBuiltinTableInit(s);
    for (const struct BuiltinGrammerSymList *l = &builtin_grammer_sym_list[0];
         l->name; l++) {
        FklSid_t id = fklAddSymbolCstr(l->name, st)->v;
        fklGraSidBuiltinTablePut2(s, id, l->t);
    }
}

static inline void clear_analysis_table(FklGrammer *g, size_t last) {
    size_t end = last + 1;
    FklAnalysisState *states = g->aTable.states;
    for (size_t i = 0; i < end; i++) {
        FklAnalysisState *curState = &states[i];
        FklAnalysisStateAction *actions = curState->state.action;
        while (actions) {
            FklAnalysisStateAction *next = actions->next;
            free(actions);
            actions = next;
        }

        FklAnalysisStateGoto *gt = curState->state.gt;
        while (gt) {
            FklAnalysisStateGoto *next = gt->next;
            free(gt);
            gt = next;
        }
    }
    free(states);
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
    free(ig);
}

void fklClearGrammer(FklGrammer *g) {
    g->prodNum = 0;
    g->start.group = 0;
    g->start.sid = 0;
    fklProdTableClear(&g->productions);
    fklFirstSetTableClear(&g->firstSets);
    clear_analysis_table(g, g->aTable.num - 1);
    free(g->sortedTerminals);
    g->sortedTerminals = NULL;
    g->sortedTerminalsNum = 0;
    FklGrammerIgnore *ig = g->ignores;
    while (ig) {
        FklGrammerIgnore *next = ig->next;
        free(ig);
        ig = next;
    }
    g->ignores = NULL;
}

void fklUninitGrammer(FklGrammer *g) {
    fklProdTableUninit(&g->productions);
    fklGraSidBuiltinTableUninit(&g->builtins);
    fklFirstSetTableUninit(&g->firstSets);
    fklUninitSymbolTable(&g->terminals);
    fklUninitSymbolTable(&g->reachable_terminals);
    fklUninitRegexTable(&g->regexes);
    clear_analysis_table(g, g->aTable.num - 1);
    free(g->sortedTerminals);
    FklGrammerIgnore *ig = g->ignores;
    while (ig) {
        FklGrammerIgnore *next = ig->next;
        fklDestroyIgnore(ig);
        ig = next;
    }
}

void fklDestroyGrammer(FklGrammer *g) {
    fklUninitGrammer(g);
    free(g);
}

static inline int init_all_builtin_grammer_sym(FklGrammer *g) {
    int failed = 0;
    if (g->sortedTerminalsNum) {
        free(g->sortedTerminals);
        g->sortedTerminals = NULL;
        g->sortedTerminalsNum = 0;
    }
    for (FklProdTableNode *pl = g->productions.first; pl; pl = pl->next) {
        for (FklGrammerProduction *prod = pl->v; prod; prod = prod->next) {
            for (size_t i = prod->len; i > 0; i--) {
                size_t idx = i - 1;
                FklGrammerSym *u = &prod->syms[idx];
                if (u->term_type == FKL_TERM_BUILTIN) {
                    if (!u->b.c)
                        u->b.c = init_builtin_grammer_sym(u->b.t, idx, prod, g,
                                                          &failed);
                    else if (u->b.t->ctx_global_create) {
                        destroy_builtin_grammer_sym(&u->b);
                        u->b.c = init_builtin_grammer_sym(u->b.t, idx, prod, g,
                                                          &failed);
                    }
                } else if (u->isterm && u->end_with_terminal
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
        FklGrammerIgnore *cur = *pp;
        FklGrammerIgnoreSym *igss0 = cur->ig;
        FklGrammerIgnoreSym *igss1 = ig->ig;
        size_t len = cur->len < ig->len ? cur->len : ig->len;
        for (size_t i = 0; i < len; i++) {
            FklGrammerIgnoreSym *igs0 = &igss0[i];
            FklGrammerIgnoreSym *igs1 = &igss1[i];
            if (igs0->term_type == igs1->term_type) {
                if (igs0->term_type == FKL_TERM_BUILTIN
                    && fklBuiltinGrammerSymEqual(&igs0->b, &igs1->b))
                    return 1;
                else if (igs0->term_type == FKL_TERM_REGEX
                         && igs0->re == igs1->re)
                    return 1;
                else if (igs0->str == igs1->str)
                    return 1;
            }
            break;
        }
    }
    *pp = ig;
    return 0;
}

static inline uint32_t is_regex_match_epsilon(const FklRegexCode *re) {
    int last_is_true = 0;
    return fklRegexLexMatchp(re, "", 0, &last_is_true) == 0;
}

static inline int
is_builtin_terminal_match_epsilon(const FklLalrBuiltinGrammerSym *b) {
    int is_waiting_for_more = 0;
    size_t matchLen = 0;
    FklGrammerMatchOuterCtx outerCtx = {
        .maxNonterminalLen = 0,
        .line = 0,
        .start = NULL,
        .cur = NULL,
        .create = NULL,
        .destroy = NULL,
    };
    return b->t->match(b->c, "", "", 0, &matchLen, &outerCtx,
                       &is_waiting_for_more);
}

static inline int compute_all_first_set(FklGrammer *g) {
    FklFirstSetTable *firsetSets = &g->firstSets;
    const FklSymbolTable *tt = &g->terminals;

    for (const FklProdTableNode *sidl = g->productions.first; sidl;
         sidl = sidl->next) {
        if (is_Sq_nt(&sidl->k))
            continue;
        FklFirstSetItem *item = fklFirstSetTableAdd(firsetSets, &sidl->k, NULL);
        item->hasEpsilon = 0;
        fklLookAheadTableInit(&item->first);
    }

    int change;
    do {
        change = 0;
        for (const FklProdTableNode *leftProds = g->productions.first;
             leftProds; leftProds = leftProds->next) {
            if (is_Sq_nt(&leftProds->k))
                continue;
            FklFirstSetItem *firstItem =
                fklFirstSetTableGet(firsetSets, &leftProds->k);
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
                    if (sym->isterm) {
                        if (sym->term_type == FKL_TERM_BUILTIN) {
                            int r = is_builtin_terminal_match_epsilon(&sym->b);
                            FklLalrItemLookAhead la = {
                                .t = FKL_LALR_MATCH_BUILTIN,
                                .delim = sym->delim,
                                .b = sym->b,
                            };
                            change |=
                                !fklLookAheadTablePut(&firstItem->first, &la);
                            if (r) {
                                if (i == lastIdx) {
                                    change |= firstItem->hasEpsilon != 1;
                                    firstItem->hasEpsilon = 1;
                                }
                            } else
                                break;

                        } else if (sym->term_type == FKL_TERM_REGEX) {
                            uint32_t r = is_regex_match_epsilon(sym->re);
                            FklLalrItemLookAhead la = {
                                .t = FKL_LALR_MATCH_REGEX,
                                .delim = sym->delim,
                                .end_with_terminal = sym->end_with_terminal,
                                .re = sym->re,
                            };

                            change |=
                                !fklLookAheadTablePut(&firstItem->first, &la);
                            if (r) {
                                if (i == lastIdx) {
                                    change |= firstItem->hasEpsilon != 1;
                                    firstItem->hasEpsilon = 1;
                                }
                            } else
                                break;
                        } else {
                            const FklString *s =
                                fklGetSymbolWithId(sym->nt.sid, tt)->k;
                            if (s->size == 0) {
                                if (i == lastIdx) {
                                    change |= firstItem->hasEpsilon != 1;
                                    firstItem->hasEpsilon = 1;
                                }
                            } else {
                                FklLalrItemLookAhead la = {
                                    .t = FKL_LALR_MATCH_STRING,
                                    .delim = sym->delim,
                                    .end_with_terminal = sym->end_with_terminal,
                                    .s = s,
                                };
                                change |= !fklLookAheadTablePut(
                                    &firstItem->first, &la);
                                break;
                            }
                        }
                    } else {
                        const FklFirstSetItem *curFirstItem =
                            fklFirstSetTableGet(firsetSets, &sym->nt);
                        if (!curFirstItem)
                            return 1;
                        for (const FklLookAheadTableNode *syms =
                                 curFirstItem->first.first;
                             syms; syms = syms->next) {
                            change |= !fklLookAheadTablePut(&firstItem->first,
                                                            &syms->k);
                        }
                        if (curFirstItem->hasEpsilon && i == lastIdx) {
                            change |= firstItem->hasEpsilon != 1;
                            firstItem->hasEpsilon = 1;
                        }
                        if (!curFirstItem->hasEpsilon)
                            break;
                    }
                }
            }
        }
    } while (change);
    return 0;
}

void fklInitEmptyGrammer(FklGrammer *r, FklSymbolTable *st) {
    fklInitSymbolTable(&r->terminals);
    fklInitSymbolTable(&r->reachable_terminals);
    fklInitRegexTable(&r->regexes);
    r->ignores = NULL;
    r->sortedTerminals = NULL;
    r->sortedTerminalsNum = 0;
    fklFirstSetTableInit(&r->firstSets);
    fklProdTableInit(&r->productions);
    fklInitBuiltinGrammerSymTable(&r->builtins, st);
}

FklGrammer *fklCreateEmptyGrammer(FklSymbolTable *st) {
    FklGrammer *r = (FklGrammer *)calloc(1, sizeof(FklGrammer));
    FKL_ASSERT(r);
    fklInitEmptyGrammer(r, st);
    return r;
}

// GraProdVector
#define FKL_VECTOR_TYPE_PREFIX Gra
#define FKL_VECTOR_METHOD_PREFIX gra
#define FKL_VECTOR_ELM_TYPE FklGrammerProduction *
#define FKL_VECTOR_ELM_TYPE_NAME Prod
#include <fakeLisp/vector.h>

static inline int add_reachable_terminal(FklGrammer *g) {
    FklProdTable *productions = &g->productions;
    GraProdVector prod_stack;
    graProdVectorInit(&prod_stack, g->prodNum);
    FklProdTableElm *first = &g->productions.first->elm;
    graProdVectorPushBack2(&prod_stack, first->v);
    FklNontermTable nonterm_set;
    fklNontermTableInit(&nonterm_set);
    while (!graProdVectorIsEmpty(&prod_stack)) {
        FklGrammerProduction *prods = *graProdVectorPopBack(&prod_stack);
        for (; prods; prods = prods->next) {
            const FklGrammerSym *syms = prods->syms;
            for (size_t i = 0; i < prods->len; i++) {
                const FklGrammerSym *cur = &syms[i];
                if (cur->isterm) {
                    if (cur->term_type == FKL_TERM_STRING
                        && !cur->end_with_terminal)
                        fklAddSymbol(
                            fklGetSymbolWithId(cur->nt.sid, &g->terminals)->k,
                            &g->reachable_terminals);
                    else if (cur->term_type == FKL_TERM_REGEX)
                        find_and_add_terminal_in_regex(cur->re,
                                                       &g->reachable_terminals);
                }
                if (!cur->isterm && !fklNontermTablePut(&nonterm_set, &cur->nt))
                    graProdVectorPushBack2(&prod_stack,
                                           fklGetProductions(productions,
                                                             cur->nt.group,
                                                             cur->nt.sid));
            }
        }
    }
    fklNontermTableUninit(&nonterm_set);
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
    FklProdTable *productions = &g->productions;
    for (const FklProdTableNode *il = productions->first; il; il = il->next) {
        for (const FklGrammerProduction *prods = il->v; prods;
             prods = prods->next) {
            const FklGrammerSym *syms = prods->syms;
            for (size_t i = 0; i < prods->len; i++) {
                const FklGrammerSym *cur = &syms[i];
                if (!cur->isterm && !fklProdTableGet(productions, &cur->nt))
                    return 1;
            }
        }
    }
    return 0;
}

int fklCheckAndInitGrammerSymbols(FklGrammer *g) {
    return check_undefined_nonterm(g) || add_reachable_terminal(g)
        || init_all_builtin_grammer_sym(g) || compute_all_first_set(g);
}

FklGrammer *fklCreateGrammerFromCstr(const char *str[], FklSymbolTable *st) {
    FklGrammer *grammer = fklCreateEmptyGrammer(st);
    FklSymbolTable *tt = &grammer->terminals;
    FklRegexTable *rt = &grammer->regexes;
    FklGraSidBuiltinTable *builtins = &grammer->builtins;
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
    const FklGraSidBuiltinTable *builtins = &g->builtins;
    if (left.group == 0 && fklGetBuiltinMatch(builtins, left.sid))
        return 1;
    FklGrammerProduction *extra_prod =
        create_extra_production(left.group, left.sid);
    extra_prod->next = NULL;
    FklGrammerProduction **item =
        fklProdTableAdd(&g->productions, &extra_prod->left, NULL);
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
    FklGraSidBuiltinTable *builtins = &grammer->builtins;
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
    if (u->isterm) {
        if (u->term_type == FKL_TERM_BUILTIN) {
            putc('?', fp);
            fputs(u->b.t->name, fp);
        } else if (u->term_type == FKL_TERM_REGEX) {
            putc('/', fp);
            fklPrintString(fklGetStringWithRegex(rt, u->re, NULL), fp);
        } else {
            putc('#', fp);
            fklPrintString(fklGetSymbolWithId(u->nt.sid, tt)->k, fp);
        }
    } else {
        putc('&', fp);
        fklPrintString(fklGetSymbolWithId(u->nt.sid, st)->k, fp);
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
    if (u->isterm) {
        if (u->term_type == FKL_TERM_BUILTIN) {
            fputc('|', fp);
            fputs(u->b.t->name, fp);
            fputc('|', fp);
        } else if (u->term_type == FKL_TERM_REGEX) {
            const FklString *str = fklGetStringWithRegex(rt, u->re, NULL);
            fputs("\\/'", fp);
            print_string_as_dot((const uint8_t *)str->str, '"', str->size, fp);
            fputs("'\\/", fp);
        } else {
            const FklString *str = fklGetSymbolWithId(u->nt.sid, tt)->k;
            fputs("\\\'", fp);
            print_string_as_dot((const uint8_t *)str->str, '"', str->size, fp);
            fputs("\\\'", fp);
        }
    } else {
        const FklString *str = fklGetSymbolWithId(u->nt.sid, st)->k;
        fputc('|', fp);
        print_string_as_dot((const uint8_t *)str->str, '|', str->size, fp);
        fputc('|', fp);
    }
}

static inline FklGrammerSym *get_item_next(const FklLalrItem *item) {
    if (item->idx >= item->prod->len)
        return NULL;
    return &item->prod->syms[item->idx];
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
    return item;
}

static inline int lalr_look_ahead_cmp(const FklLalrItemLookAhead *la0,
                                      const FklLalrItemLookAhead *la1) {
    if (la0->t == la1->t) {
        switch (la0->t) {
        case FKL_LALR_MATCH_NONE:
        case FKL_LALR_MATCH_EOF:
            return 0;
            break;
        case FKL_LALR_MATCH_BUILTIN: {
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
        case FKL_LALR_MATCH_STRING:
        case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL:
            return fklStringCmp(la0->s, la1->s);
            break;
        case FKL_LALR_MATCH_REGEX:
            return ((int64_t)la0->re->totalsize)
                 - ((int64_t)la1->re->totalsize);
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

static inline void lalr_item_set_sort(FklLalrItemTable *itemSet) {
    size_t num = itemSet->count;
    FklLalrItem *item_array;
    if (num == 0)
        return;
    else {
        item_array = (FklLalrItem *)malloc(num * sizeof(FklLalrItem));
        FKL_ASSERT(item_array);
    }

    size_t i = 0;
    for (FklLalrItemTableNode *l = itemSet->first; l; l = l->next, i++) {
        item_array[i] = l->k;
    }
    qsort(item_array, num, sizeof(FklLalrItem), lalr_item_qsort_cmp);
    fklLalrItemTableClear(itemSet);
    for (i = 0; i < num; i++)
        fklLalrItemTablePut(itemSet, &item_array[i]);
    free(item_array);
}

static inline void lr0_item_set_closure(FklLalrItemTable *itemSet,
                                        FklGrammer *g) {
    int change;
    FklNontermTable sidSet;
    fklNontermTableInit(&sidSet);
    FklNontermTable changeSet;
    fklNontermTableInit(&changeSet);
    do {
        change = 0;
        for (FklLalrItemTableNode *l = itemSet->first; l; l = l->next) {
            FklGrammerSym *sym = get_item_next(&l->k);
            if (sym && !sym->isterm) {
                const FklGrammerNonterm *left = &sym->nt;
                if (!fklNontermTablePut(&sidSet, left)) {
                    change = 1;
                    fklNontermTablePut(&changeSet, left);
                }
            }
        }

        for (FklNontermTableNode *lefts = changeSet.first; lefts;
             lefts = lefts->next) {
            FklGrammerProduction *prod =
                fklGetGrammerProductions(g, lefts->k.group, lefts->k.sid);
            for (; prod; prod = prod->next) {
                FklLalrItem item = lalr_item_init(prod, 0, NULL);
                fklLalrItemTablePut(itemSet, &item);
            }
        }
        fklNontermTableClear(&changeSet);
    } while (change);
    fklNontermTableUninit(&sidSet);
    fklNontermTableUninit(&changeSet);
}

static inline void
lr0_item_set_copy_and_closure(FklLalrItemTable *dst,
                              const FklLalrItemTable *itemSet, FklGrammer *g) {
    for (FklLalrItemTableNode *il = itemSet->first; il; il = il->next) {
        fklLalrItemTablePut(dst, &il->k);
    }
    lr0_item_set_closure(dst, g);
}

static inline void init_first_item_set(FklLalrItemTable *itemSet,
                                       FklGrammerProduction *prod) {
    FklLalrItem item = lalr_item_init(prod, 0, NULL);
    fklLalrItemTableInit(itemSet);
    fklLalrItemTablePut(itemSet, &item);
}

static inline void print_look_ahead(FILE *fp, const FklLalrItemLookAhead *la,
                                    const FklRegexTable *rt) {
    switch (la->t) {
    case FKL_LALR_MATCH_STRING:
    case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL:
        putc('#', fp);
        fklPrintString(la->s, fp);
        break;
    case FKL_LALR_MATCH_EOF:
        fputs("$$", fp);
        break;
    case FKL_LALR_MATCH_BUILTIN:
        fputs("&?", fp);
        fputs(la->b.t->name, fp);
        break;
    case FKL_LALR_MATCH_NONE:
        fputs("()", fp);
        break;
    case FKL_LALR_MATCH_REGEX:
        fprintf(fp, "/%s/", fklGetStringWithRegex(rt, la->re, NULL)->str);
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
    fputs(" , ", fp);
    print_look_ahead(fp, &item->la, rt);
}

void fklPrintItemSet(const FklLalrItemTable *itemSet, const FklGrammer *g,
                     const FklSymbolTable *st, FILE *fp) {
    const FklSymbolTable *tt = &g->terminals;
    FklLalrItem const *curItem = NULL;
    for (FklLalrItemTableNode *list = itemSet->first; list; list = list->next) {
        if (!curItem || list->k.idx != curItem->idx
            || list->k.prod != curItem->prod) {
            if (curItem)
                putc('\n', fp);
            curItem = &list->k;
            print_item(fp, curItem, st, tt, &g->regexes);
        } else {
            fputs(" / ", fp);
            print_look_ahead(fp, &list->k.la, &g->regexes);
        }
    }
    putc('\n', fp);
}

static void item_set_set_val(void *d0, const void *d1) {
    FklLalrItemSet *s = (FklLalrItemSet *)d0;
    *s = *(const FklLalrItemSet *)d1;
    s->links = NULL;
    s->spreads = NULL;
}

static uintptr_t item_set_hash_func(const void *d) {
    const FklLalrItemTable *i = FKL_TYPE_CAST(const FklLalrItemTable *, d);
    uintptr_t v = 0;
    for (FklLalrItemTableNode *l = i->first; l; l = l->next) {
        v = fklHashCombine(v, fklLalrItemHash(&l->k));
    }
    return v;
}

static int item_set_equal(const void *d0, const void *d1) {
    const FklLalrItemTable *s0 = FKL_TYPE_CAST(const FklLalrItemTable *, d0);
    const FklLalrItemTable *s1 = FKL_TYPE_CAST(const FklLalrItemTable *, d1);
    if (s0->count != s1->count)
        return 0;
    const FklLalrItemTableNode *l0 = s0->first;
    const FklLalrItemTableNode *l1 = s1->first;
    for (; l0; l0 = l0->next, l1 = l1->next)
        if (!fklLalrItemEqual(&l0->k, &l1->k))
            return 0;
    return 1;
}

static void item_set_uninit(void *d) {
    FklLalrItemSet *s = (FklLalrItemSet *)d;
    fklLalrItemTableUninit(&s->items);
    FklLalrItemSetLink *l = s->links;
    while (l) {
        FklLalrItemSetLink *next = l->next;
        free(l);
        l = next;
    }
    FklLookAheadSpreads *sp = s->spreads;
    while (sp) {
        FklLookAheadSpreads *next = sp->next;
        free(sp);
        sp = next;
    }
}

static void item_set_add_link(FklLalrItemSet *src, const FklGrammerSym *sym,
                              FklLalrItemSet *dst) {
    FklLalrItemSetLink *l =
        (FklLalrItemSetLink *)malloc(sizeof(FklLalrItemSetLink));
    FKL_ASSERT(l);
    l->sym = *sym;
    l->dst = dst;
    l->next = src->links;
    src->links = l;
}

static void item_set_set_key(void *k0, const void *k1) {
    *(FklLalrItemTable *)k0 = *(FklLalrItemTable *)k1;
}

static const FklHashTableMetaTable ItemStateSetHashMetaTable = {
    .size = sizeof(FklLalrItemSet),
    .__getKey = fklHashDefaultGetKey,
    .__setKey = item_set_set_key,
    .__setVal = item_set_set_val,
    .__hashFunc = item_set_hash_func,
    .__keyEqual = item_set_equal,
    .__uninitItem = item_set_uninit,
};

static inline FklHashTable *create_item_state_set(void) {
    return fklCreateHashTable(&ItemStateSetHashMetaTable);
}

typedef struct GraGetLaFirstSetCacheKey {
    const FklGrammerProduction *prod;
    uint32_t idx;
} GraGetLaFirstSetCacheKey;

typedef struct GraGetLaFirstSetCacheItem {
    FklLookAheadTable first;
    int has_epsilon;
} GraGetLaFirstSetCacheItem;

// GraLaFirstSetCacheTable
#define FKL_TABLE_TYPE_PREFIX Gra
#define FKL_TABLE_METHOD_PREFIX gra
#define FKL_TABLE_KEY_TYPE GraGetLaFirstSetCacheKey
#define FKL_TABLE_VAL_TYPE GraGetLaFirstSetCacheItem
#define FKL_TABLE_VAL_INIT(A, B) abort()
#define FKL_TABLE_VAL_UNINIT(V) fklLookAheadTableUninit(&(V)->first)
#define FKL_TABLE_KEY_EQUAL(A, B) (A)->prod == (B)->prod && (A)->idx == (B)->idx
#define FKL_TABLE_KEY_HASH                                                     \
    return fklHashCombine(                                                     \
        FKL_TYPE_CAST(uintptr_t, (pk->prod) / alignof(FklGrammerProduction)),  \
        pk->idx);
#define FKL_TABLE_ELM_NAME LaFirstSetCache
#include <fakeLisp/table.h>

// GraItemSetQueue
#define FKL_QUEUE_TYPE_PREFIX Gra
#define FKL_QUEUE_METHOD_PREFIX gra
#define FKL_QUEUE_ELM_TYPE FklLalrItemSet *
#define FKL_QUEUE_ELM_TYPE_NAME ItemSet
#include <fakeLisp/queue.h>

static inline int grammer_sym_equal(const FklGrammerSym *s0,
                                    const FklGrammerSym *s1) {
    return s0->isterm == s1->isterm && s0->term_type == s1->term_type
        && (s0->term_type == FKL_TERM_BUILTIN
                ? fklBuiltinGrammerSymEqual(&s0->b, &s1->b)
            : (s0->term_type == FKL_TERM_REGEX)
                ? s0->re == s1->re
                : fklNontermEqual(&s0->nt, &s1->nt))
        && s0->delim == s1->delim
        && s0->end_with_terminal == s1->end_with_terminal;
}

static inline uintptr_t grammer_sym_hash(const FklGrammerSym *s) {
    uintptr_t seed =
        (s->term_type == FKL_TERM_BUILTIN ? fklBuiltinGrammerSymHash(&s->b)
         : (s->term_type == FKL_TERM_REGEX)
             ? fklHash64Shift(FKL_TYPE_CAST(uintptr_t, s->re / alignof(*s->re)))
             : fklNontermHash(&s->nt));
    seed = fklHashCombine(seed, s->isterm);
    seed = fklHashCombine(seed, s->delim);
    seed = fklHashCombine(seed, s->end_with_terminal);
    return seed;
}

// GraSymbolTable
#define FKL_TABLE_TYPE_PREFIX Gra
#define FKL_TABLE_METHOD_PREFIX gra
#define FKL_TABLE_KEY_TYPE FklGrammerSym
#define FKL_TABLE_KEY_EQUAL(A, B) grammer_sym_equal(A, B)
#define FKL_TABLE_KEY_HASH return grammer_sym_hash(pk)
#define FKL_TABLE_ELM_NAME Symbol
#include <fakeLisp/table.h>

static inline void lr0_item_set_goto(FklLalrItemSet *itemset,
                                     FklHashTable *itemsetSet, FklGrammer *g,
                                     GraItemSetQueue *pending) {
    GraSymbolTable checked;
    graSymbolTableInit(&checked);
    FklLalrItemTable *items = &itemset->items;
    FklLalrItemTable itemsClosure;
    fklLalrItemTableInit(&itemsClosure);
    lr0_item_set_copy_and_closure(&itemsClosure, items, g);
    lalr_item_set_sort(&itemsClosure);
    for (FklLalrItemTableNode *l = itemsClosure.first; l; l = l->next) {
        FklGrammerSym *sym = get_item_next(&l->k);
        if (sym)
            graSymbolTablePut(&checked, sym);
    }
    for (GraSymbolTableNode *ll = checked.first; ll; ll = ll->next) {
        FklLalrItemTable newItems;
        fklLalrItemTableInit(&newItems);
        for (FklLalrItemTableNode *l = itemsClosure.first; l; l = l->next) {
            FklGrammerSym *next = get_item_next(&l->k);
            if (next && grammer_sym_equal(&ll->k, next)) {
                FklLalrItem item = get_item_advance(&l->k);
                fklLalrItemTablePut(&newItems, &item);
            }
        }
        FklLalrItemSet *itemsetptr = fklGetHashItem(&newItems, itemsetSet);
        if (!itemsetptr) {
            FklLalrItemSet itemset = {.items = newItems, .links = NULL};
            itemsetptr = fklGetOrPutHashItem(&itemset, itemsetSet);
            graItemSetQueuePush2(pending, itemsetptr);
        } else
            fklLalrItemTableUninit(&newItems);
        item_set_add_link(itemset, &ll->k, itemsetptr);
    }
    fklLalrItemTableUninit(&itemsClosure);
    graSymbolTableUninit(&checked);
}

FklHashTable *fklGenerateLr0Items(FklGrammer *grammer) {
    FklHashTable *itemstate_set = create_item_state_set();
    const FklGrammerNonterm left = {
        .group = 0,
        .sid = 0,
    };
    FklGrammerProduction *prod = *fklProdTableGet(&grammer->productions, &left);
    FklLalrItemTable items;
    init_first_item_set(&items, prod);
    FklLalrItemSet itemset = {
        .items = items,
    };
    FklLalrItemSet *itemsetptr = fklGetOrPutHashItem(&itemset, itemstate_set);
    GraItemSetQueue pending;
    graItemSetQueueInit(&pending);
    graItemSetQueuePush2(&pending, itemsetptr);
    while (!graItemSetQueueIsEmpty(&pending)) {
        FklLalrItemSet *itemsetptr = *graItemSetQueuePop(&pending);
        lr0_item_set_goto(itemsetptr, itemstate_set, grammer, &pending);
    }
    graItemSetQueueUninit(&pending);
    return itemstate_set;
}

static inline FklLookAheadTable *get_first_set_from_first_sets(
    const FklGrammer *g, const FklGrammerProduction *prod, uint32_t idx,
    GraLaFirstSetCacheTable *cache, int *pHasEpsilon) {
    size_t len = prod->len;
    if (idx >= len) {
        *pHasEpsilon = 1;
        return NULL;
    }
    GraGetLaFirstSetCacheKey key = {.prod = prod, .idx = idx};
    GraGetLaFirstSetCacheItem *item =
        graLaFirstSetCacheTableAdd(cache, &key, NULL);
    if (item->first.buckets) {
        *pHasEpsilon = item->has_epsilon;
        return &item->first;
    } else {
        FklLookAheadTable *first = &item->first;
        fklLookAheadTableInit(first);
        item->has_epsilon = 0;
        size_t lastIdx = len - 1;
        const FklSymbolTable *tt = &g->terminals;
        int hasEpsilon = 0;
        const FklFirstSetTable *firstSets = &g->firstSets;
        for (uint32_t i = idx; i < len; i++) {
            FklGrammerSym *sym = &prod->syms[i];
            if (sym->isterm) {
                if (sym->term_type == FKL_TERM_BUILTIN) {
                    int r = is_builtin_terminal_match_epsilon(&sym->b);
                    FklLalrItemLookAhead la = {
                        .t = FKL_LALR_MATCH_BUILTIN,
                        .delim = sym->delim,
                        .b = sym->b,
                    };
                    fklLookAheadTablePut(first, &la);
                    if (r)
                        hasEpsilon = i == lastIdx;
                    else
                        break;
                } else if (sym->term_type == FKL_TERM_REGEX) {
                    uint32_t r = is_regex_match_epsilon(sym->re);
                    FklLalrItemLookAhead la = {
                        .t = FKL_LALR_MATCH_REGEX,
                        .delim = sym->delim,
                        .end_with_terminal = sym->end_with_terminal,
                        .re = sym->re,
                    };
                    fklLookAheadTablePut(first, &la);
                    if (r)
                        hasEpsilon = i == lastIdx;
                    else
                        break;
                } else {
                    FklString *s = fklGetSymbolWithId(sym->nt.sid, tt)->k;
                    if (s->size == 0)
                        hasEpsilon = i == lastIdx;
                    else {
                        FklLalrItemLookAhead la = {
                            .t = FKL_LALR_MATCH_STRING,
                            .delim = sym->delim,
                            .end_with_terminal = sym->end_with_terminal,
                            .s = s,
                        };
                        fklLookAheadTablePut(first, &la);
                        break;
                    }
                }
            } else {
                const FklFirstSetItem *firstSetItem =
                    fklFirstSetTableGet(firstSets, &sym->nt);
                for (FklLookAheadTableNode *symList = firstSetItem->first.first;
                     symList; symList = symList->next) {
                    fklLookAheadTablePut(first, &symList->k);
                }
                if (firstSetItem->hasEpsilon && i == lastIdx)
                    hasEpsilon = 1;
                if (!firstSetItem->hasEpsilon)
                    break;
            }
        }
        *pHasEpsilon = hasEpsilon;
        item->has_epsilon = hasEpsilon;
        return first;
    }
}

static inline FklLookAheadTable *
get_la_first_set(const FklGrammer *g, const FklGrammerProduction *prod,
                 uint32_t beta, GraLaFirstSetCacheTable *cache,
                 int *hasEpsilon) {
    return get_first_set_from_first_sets(g, prod, beta, cache, hasEpsilon);
}

static inline void lr1_item_set_closure(FklLalrItemTable *itemSet,
                                        FklGrammer *g,
                                        GraLaFirstSetCacheTable *cache) {
    FklLalrItemTable pendingSet;
    FklLalrItemTable changeSet;
    fklLalrItemTableInit(&pendingSet);
    fklLalrItemTableInit(&changeSet);

    for (FklLalrItemTableNode *l = itemSet->first; l; l = l->next) {
        fklLalrItemTablePut(&pendingSet, &l->k);
    }

    FklLalrItemTable *processing_set = &pendingSet;
    FklLalrItemTable *next_set = &changeSet;
    while (processing_set->count) {
        for (FklLalrItemTableNode *l = processing_set->first; l; l = l->next) {
            FklGrammerSym *next = get_item_next(&l->k);
            if (next && !next->isterm) {
                uint32_t beta = l->k.idx + 1;
                int hasEpsilon = 0;
                FklLookAheadTable *first =
                    get_la_first_set(g, l->k.prod, beta, cache, &hasEpsilon);
                const FklGrammerNonterm *left = &next->nt;
                FklGrammerProduction *prods =
                    fklGetGrammerProductions(g, left->group, left->sid);
                if (first) {
                    for (FklLookAheadTableNode *first_list = first->first;
                         first_list; first_list = first_list->next) {
                        for (FklGrammerProduction *prod = prods; prod;
                             prod = prod->next) {
                            FklLalrItem newItem = {
                                .prod = prod, .la = first_list->k, .idx = 0};
                            if (!fklLalrItemTablePut(itemSet, &newItem))
                                fklLalrItemTablePut(next_set, &newItem);
                        }
                    }
                }
                if (hasEpsilon) {
                    for (FklGrammerProduction *prod = prods; prod;
                         prod = prod->next) {
                        FklLalrItem newItem = {
                            .prod = prod, .la = l->k.la, .idx = 0};
                        if (!fklLalrItemTablePut(itemSet, &newItem))
                            fklLalrItemTablePut(next_set, &newItem);
                    }
                }
            }
        }

        fklLalrItemTableClear(processing_set);
        FklLalrItemTable *t = processing_set;
        processing_set = next_set;
        next_set = t;
    }
    fklLalrItemTableUninit(&changeSet);
    fklLalrItemTableUninit(&pendingSet);
}

static inline void add_lookahead_spread(FklLalrItemSet *itemset,
                                        const FklLalrItem *src,
                                        FklLalrItemTable *dstItems,
                                        const FklLalrItem *dst) {
    FklLookAheadSpreads *sp =
        (FklLookAheadSpreads *)malloc(sizeof(FklLookAheadSpreads));
    FKL_ASSERT(sp);
    sp->next = itemset->spreads;
    sp->dstItems = dstItems;
    sp->src = *src;
    sp->dst = *dst;
    itemset->spreads = sp;
}

static inline void check_lookahead_self_generated_and_spread(
    FklGrammer *g, FklLalrItemSet *itemset, GraLaFirstSetCacheTable *cache) {
    FklLalrItemTable *items = &itemset->items;
    FklLalrItemTable closure;
    fklLalrItemTableInit(&closure);
    for (FklLalrItemTableNode *il = items->first; il; il = il->next) {
        if (il->k.la.t == FKL_LALR_MATCH_NONE) {
            FklLalrItem item = {.prod = il->k.prod,
                                .idx = il->k.idx,
                                .la = FKL_LALR_MATCH_NONE_INIT};
            fklLalrItemTablePut(&closure, &item);
            lr1_item_set_closure(&closure, g, cache);
            for (FklLalrItemTableNode *cl = closure.first; cl; cl = cl->next) {
                FklLalrItem i = cl->k;
                const FklGrammerSym *s = get_item_next(&i);
                ++i.idx;
                for (const FklLalrItemSetLink *x = itemset->links; x;
                     x = x->next) {
                    if (x->dst == itemset)
                        continue;
                    const FklGrammerSym *xsym = &x->sym;
                    if (s && grammer_sym_equal(s, xsym)) {
                        if (i.la.t == FKL_LALR_MATCH_NONE)
                            add_lookahead_spread(itemset, &item, &x->dst->items,
                                                 &i);
                        else
                            fklLalrItemTablePut(&x->dst->items, &i);
                    }
                }
            }
            fklLalrItemTableClear(&closure);
        }
    }
    fklLalrItemTableUninit(&closure);
}

static inline int lookahead_spread(FklLalrItemSet *itemset) {
    int change = 0;
    FklLalrItemTable *items = &itemset->items;
    for (FklLookAheadSpreads *sp = itemset->spreads; sp; sp = sp->next) {
        FklLalrItem *srcItem = &sp->src;
        FklLalrItem *dstItem = &sp->dst;
        FklLalrItemTable *dstItems = sp->dstItems;
        for (FklLalrItemTableNode *il = items->first; il; il = il->next) {
            if (il->k.la.t != FKL_LALR_MATCH_NONE && il->k.prod == srcItem->prod
                && il->k.idx == srcItem->idx) {
                FklLalrItem newItem = *dstItem;
                newItem.la = il->k.la;
                change |= !fklLalrItemTablePut(dstItems, &newItem);
            }
        }
    }
    return change;
}

static inline void init_lalr_look_ahead(FklHashTable *lr0, FklGrammer *g,
                                        GraLaFirstSetCacheTable *cache) {
    for (FklHashTableItem *isl = lr0->first; isl; isl = isl->next) {
        FklLalrItemSet *s = (FklLalrItemSet *)isl->data;
        check_lookahead_self_generated_and_spread(g, s, cache);
    }
    FklHashTableItem *isl = lr0->first;
    FklLalrItemSet *s = FKL_TYPE_CAST(FklLalrItemSet *, isl->data);
    for (FklLalrItemTableNode *il = s->items.first; il; il = il->next) {
        FklLalrItem item = il->k;
        item.la = FKL_LALR_MATCH_EOF_INIT;
        fklLalrItemTablePut(&s->items, &item);
    }
}

static inline void add_look_ahead_to_items(FklLalrItemTable *items,
                                           FklGrammer *g,
                                           GraLaFirstSetCacheTable *cache) {
    FklLalrItemTable add;
    fklLalrItemTableInit(&add);
    for (FklLalrItemTableNode *il = items->first; il; il = il->next) {
        if (il->k.la.t != FKL_LALR_MATCH_NONE)
            fklLalrItemTablePut(&add, &il->k);
    }
    fklLalrItemTableClear(items);
    for (FklLalrItemTableNode *il = add.first; il; il = il->next) {
        fklLalrItemTablePut(items, &il->k);
    }
    fklLalrItemTableUninit(&add);
    lr1_item_set_closure(items, g, cache);
    lalr_item_set_sort(items);
}

static inline void
add_look_ahead_for_all_item_set(FklHashTable *lr0, FklGrammer *g,
                                GraLaFirstSetCacheTable *cache) {
    for (FklHashTableItem *isl = lr0->first; isl; isl = isl->next) {
        FklLalrItemSet *itemSet = (FklLalrItemSet *)isl->data;
        add_look_ahead_to_items(&itemSet->items, g, cache);
    }
}

void fklLr0ToLalrItems(FklHashTable *lr0, FklGrammer *g) {
    GraLaFirstSetCacheTable cache;
    graLaFirstSetCacheTableInit(&cache);
    init_lalr_look_ahead(lr0, g, &cache);
    int change;
    do {
        change = 0;
        for (FklHashTableItem *isl = lr0->first; isl; isl = isl->next) {
            FklLalrItemSet *s = (FklLalrItemSet *)isl->data;
            change |= lookahead_spread(s);
        }
    } while (change);
    add_look_ahead_for_all_item_set(lr0, g, &cache);
    graLaFirstSetCacheTableUninit(&cache);
}

// GraItemStateIdxTable
#define FKL_TABLE_TYPE_PREFIX Gra
#define FKL_TABLE_METHOD_PREFIX gra
#define FKL_TABLE_KEY_TYPE FklLalrItemSet const *
#define FKL_TABLE_VAL_TYPE size_t
#define FKL_TABLE_ELM_NAME ItemStateIdx
#define FKL_TABLE_KEY_HASH                                                     \
    return fklHash64Shift(                                                     \
        FKL_TYPE_CAST(uintptr_t, (*pk) / alignof(FklLalrItemSet)));
#include <fakeLisp/table.h>

static inline void print_look_ahead_as_dot(FILE *fp,
                                           const FklLalrItemLookAhead *la,
                                           const FklRegexTable *rt) {
    switch (la->t) {
    case FKL_LALR_MATCH_STRING:
    case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL: {
        fputs("\\\'", fp);
        const FklString *str = la->s;
        print_string_as_dot((const uint8_t *)str->str, '\'', str->size, fp);
        fputs("\\\'", fp);
    } break;
    case FKL_LALR_MATCH_EOF:
        fputs("$$", fp);
        break;
    case FKL_LALR_MATCH_BUILTIN:
        fputc('|', fp);
        fputs(la->b.t->name, fp);
        fputc('|', fp);
        break;
    case FKL_LALR_MATCH_NONE:
        fputs("()", fp);
        break;
    case FKL_LALR_MATCH_REGEX: {
        fputs("\\/\'", fp);
        const FklString *str = fklGetStringWithRegex(rt, la->re, NULL);
        print_string_as_dot((const uint8_t *)str->str, '\'', str->size, fp);
        fputs("\\/\'", fp);
    } break;
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

static inline void print_item_set_as_dot(const FklLalrItemTable *itemSet,
                                         const FklGrammer *g,
                                         const FklSymbolTable *st, FILE *fp) {
    const FklSymbolTable *tt = &g->terminals;
    FklLalrItem const *curItem = NULL;
    for (FklLalrItemTableNode *list = itemSet->first; list; list = list->next) {
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

void fklPrintItemStateSetAsDot(const FklHashTable *i, const FklGrammer *g,
                               const FklSymbolTable *st, FILE *fp) {
    fputs("digraph \"items-lalr\"{\n", fp);
    fputs("\trankdir=\"LR\"\n", fp);
    fputs("\tranksep=1\n", fp);
    fputs("\tgraph[overlap=false];\n", fp);
    GraItemStateIdxTable idxTable;
    graItemStateIdxTableInit(&idxTable);
    size_t idx = 0;
    for (const FklHashTableItem *l = i->first; l; l = l->next, idx++) {
        const FklLalrItemSet *s = (const FklLalrItemSet *)l->data;
        graItemStateIdxTablePut2(&idxTable, s, idx);
    }
    for (const FklHashTableItem *l = i->first; l; l = l->next) {
        const FklLalrItemSet *s = (const FklLalrItemSet *)l->data;
        const FklLalrItemTable *i = &s->items;
        idx = *graItemStateIdxTableGet2(&idxTable, s);
        fprintf(fp,
                "\t\"I%" FKL_PRT64U "\"[fontname=\"Courier\" nojustify=true "
                "shape=\"box\" label=\"I%" FKL_PRT64U "\\l\\\n",
                idx, idx);
        print_item_set_as_dot(i, g, st, fp);
        fputs("\"]\n", fp);
        for (FklLalrItemSetLink *l = s->links; l; l = l->next) {
            FklLalrItemSet *dst = l->dst;
            size_t *c = graItemStateIdxTableGet2(&idxTable, dst);
            fprintf(fp,
                    "\tI%" FKL_PRT64U "->I%" FKL_PRT64U
                    "[fontname=\"Courier\" label=\"",
                    idx, *c);
            print_prod_sym_as_dot(fp, &l->sym, st, &g->terminals, &g->regexes);
            fputs("\"]\n", fp);
        }
        putc('\n', fp);
    }
    graItemStateIdxTableUninit(&idxTable);
    fputs("}", fp);
}

static inline FklAnalysisStateAction *
create_shift_action(const FklGrammerSym *sym, const FklSymbolTable *tt,
                    FklAnalysisState *state) {
    FklAnalysisStateAction *action =
        (FklAnalysisStateAction *)malloc(sizeof(FklAnalysisStateAction));
    FKL_ASSERT(action);
    action->next = NULL;
    action->action = FKL_ANALYSIS_SHIFT;
    action->state = state;
    if (sym->term_type == FKL_TERM_BUILTIN) {
        action->match.t = FKL_LALR_MATCH_BUILTIN;
        action->match.func = sym->b;
    } else if (sym->term_type == FKL_TERM_REGEX) {
        action->match.t = FKL_LALR_MATCH_REGEX;
        action->match.re = sym->re;
    } else {
        const FklString *s = fklGetSymbolWithId(sym->nt.sid, tt)->k;
        action->match.t = sym->end_with_terminal
                            ? FKL_LALR_MATCH_STRING_END_WITH_TERMINAL
                            : FKL_LALR_MATCH_STRING;
        action->match.str = s;
    }
    return action;
}

static inline FklAnalysisStateGoto *
create_state_goto(const FklGrammerSym *sym, const FklSymbolTable *tt,
                  FklAnalysisStateGoto *next, FklAnalysisState *state) {
    FklAnalysisStateGoto *gt =
        (FklAnalysisStateGoto *)malloc(sizeof(FklAnalysisStateGoto));
    FKL_ASSERT(gt);
    gt->next = next;
    gt->state = state;
    gt->nt = sym->nt;
    return gt;
}

static inline int
lalr_look_ahead_and_action_match_equal(const FklAnalysisStateActionMatch *match,
                                       const FklLalrItemLookAhead *la) {
    if (match->t == la->t) {
        switch (match->t) {
        case FKL_LALR_MATCH_STRING:
        case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL:
            return match->str == la->s;
            break;
        case FKL_LALR_MATCH_BUILTIN:
            return match->func.t == la->b.t
                && fklBuiltinGrammerSymEqual(&match->func, &la->b);
            break;
        case FKL_LALR_MATCH_REGEX:
            return match->re == la->re;
            break;
        case FKL_LALR_MATCH_EOF:
        case FKL_LALR_MATCH_NONE:
            return 0;
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
    if (la->end_with_terminal && action->match.t == FKL_LALR_MATCH_STRING)
        action->match.t = FKL_LALR_MATCH_STRING_END_WITH_TERMINAL;
    switch (action->match.t) {
    case FKL_LALR_MATCH_STRING:
    case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL:
        action->match.str = la->s;
        break;
    case FKL_LALR_MATCH_BUILTIN:
        action->match.func.t = la->b.t;
        action->match.func.c = la->b.c;
        break;
    case FKL_LALR_MATCH_REGEX:
        action->match.re = la->re;
        break;
    case FKL_LALR_MATCH_NONE:
    case FKL_LALR_MATCH_EOF:
        break;
    }
}

static inline int add_reduce_action(FklAnalysisState *curState,
                                    const FklGrammerProduction *prod,
                                    const FklLalrItemLookAhead *la) {
    if (check_reduce_conflict(curState->state.action, la))
        return 1;
    FklAnalysisStateAction *action =
        (FklAnalysisStateAction *)malloc(sizeof(FklAnalysisStateAction));
    FKL_ASSERT(action);
    init_action_with_look_ahead(action, la);
    if (is_Sq_nt(&prod->left))
        action->action = FKL_ANALYSIS_ACCEPT;
    else {
        action->action = FKL_ANALYSIS_REDUCE;
        action->prod = prod;
    }
    FklAnalysisStateAction **pa = &curState->state.action;
    if (la->t == FKL_LALR_MATCH_STRING) {
        const FklString *s = action->match.str;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_LALR_MATCH_STRING_END_WITH_TERMINAL)
                break;
        }
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_LALR_MATCH_STRING
                || (curAction->match.t == FKL_LALR_MATCH_STRING
                    && s->size > curAction->match.str->size))
                break;
        }
        action->next = *pa;
        *pa = action;
    } else if (action->match.t == FKL_LALR_MATCH_STRING_END_WITH_TERMINAL) {
        const FklString *s = action->match.str;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_LALR_MATCH_STRING_END_WITH_TERMINAL
                || (curAction->match.t
                        == FKL_LALR_MATCH_STRING_END_WITH_TERMINAL
                    && s->size > curAction->match.str->size))
                break;
        }
        action->next = *pa;
        *pa = action;
    } else if (la->t == FKL_LALR_MATCH_EOF) {
        FklAnalysisStateAction **pa = &curState->state.action;
        for (; *pa; pa = &(*pa)->next)
            ;
        action->next = *pa;
        *pa = action;
    } else {
        FklAnalysisStateAction **pa = &curState->state.action;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_LALR_MATCH_STRING)
                break;
        }
        action->next = *pa;
        *pa = action;
    }
    return 0;
}

static inline void add_shift_action(FklAnalysisState *curState,
                                    const FklGrammerSym *sym,
                                    const FklSymbolTable *tt,
                                    FklAnalysisState *dstState) {
    FklAnalysisStateAction *action = create_shift_action(sym, tt, dstState);
    FklAnalysisStateAction **pa = &curState->state.action;
    if (action->match.t == FKL_LALR_MATCH_STRING) {
        const FklString *s = action->match.str;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_LALR_MATCH_STRING_END_WITH_TERMINAL)
                break;
        }
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_LALR_MATCH_STRING
                || (curAction->match.t == FKL_LALR_MATCH_STRING
                    && s->size > curAction->match.str->size))
                break;
        }
    } else if (action->match.t == FKL_LALR_MATCH_STRING_END_WITH_TERMINAL) {
        const FklString *s = action->match.str;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_LALR_MATCH_STRING_END_WITH_TERMINAL
                || (curAction->match.t
                        == FKL_LALR_MATCH_STRING_END_WITH_TERMINAL
                    && s->size > curAction->match.str->size))
                break;
        }
    } else if (action->match.t == FKL_LALR_MATCH_REGEX) {
        const FklRegexCode *re = action->match.re;
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_LALR_MATCH_STRING_END_WITH_TERMINAL
                && curAction->match.t != FKL_LALR_MATCH_STRING)
                break;
        }
        for (; *pa; pa = &(*pa)->next) {
            FklAnalysisStateAction *curAction = *pa;
            if (curAction->match.t != FKL_LALR_MATCH_REGEX
                || (curAction->match.t == FKL_LALR_MATCH_REGEX
                    && re->totalsize > curAction->match.re->totalsize))
                break;
        }
    } else
        for (; *pa; pa = &(*pa)->next)
            if ((*pa)->match.t == FKL_LALR_MATCH_EOF)
                break;
    action->next = *pa;
    *pa = action;
}

static int builtin_match_ignore_func(void *ctx, const char *start,
                                     const char *str, size_t restLen,
                                     size_t *pmatchLen,
                                     FklGrammerMatchOuterCtx *outerCtx,
                                     int *is_waiting_for_more) {
    FklGrammerIgnore *ig = (FklGrammerIgnore *)ctx;
    return ignore_match(ig, start, str, restLen, pmatchLen, outerCtx,
                        is_waiting_for_more);
}

#define PRINT_C_REGEX_PREFIX "R_"

static inline void ignore_print_c_match_cond(const FklGrammerIgnore *ig,
                                             const FklGrammer *g, FILE *fp) {
    fputc('(', fp);
    size_t len = ig->len;
    const FklGrammerIgnoreSym *igss = ig->ig;

    const FklGrammerIgnoreSym *igs = &igss[0];
    if (igs->term_type == FKL_TERM_BUILTIN)
        igs->b.t->print_c_match_cond(igs->b.c, g, fp);
    else if (igs->term_type == FKL_TERM_REGEX) {
        uint64_t num = 0;
        fklGetStringWithRegex(&g->regexes, igs->re, &num);
        fputs("regex_lex_match_for_parser_in_c((const FklRegexCode*)&", fp);
        fprintf(fp, PRINT_C_REGEX_PREFIX "%" FKL_PRT64X, num);
        fputs(",*in+otherMatchLen,*restLen-otherMatchLen,&matchLen,&is_waiting_"
              "for_more)",
              fp);
    } else {
        fputs("(matchLen=fklCharBufMatch(\"", fp);
        print_string_in_hex(igs->str, fp);
        fprintf(fp,
                "\",%" FKL_PRT64U ",*in+otherMatchLen,*restLen-otherMatchLen))",
                igs->str->size);
    }
    fputs("&&((otherMatchLen+=matchLen)||1)", fp);

    for (size_t i = 1; i < len; i++) {
        fputs("&&", fp);
        const FklGrammerIgnoreSym *igs = &igss[i];
        if (igs->term_type == FKL_TERM_BUILTIN)
            igs->b.t->print_c_match_cond(igs->b.c, g, fp);
        else if (igs->term_type == FKL_TERM_REGEX) {
            uint64_t num = 0;
            fklGetStringWithRegex(&g->regexes, igs->re, &num);
            fputs("regex_lex_match_for_parser_in_c((const FklRegexCode*)&", fp);
            fprintf(fp, PRINT_C_REGEX_PREFIX "%" FKL_PRT64X, num);
            fputs(",*in+otherMatchLen,*restLen-otherMatchLen,&matchLen,&is_"
                  "waiting_for_more)",
                  fp);
        } else {
            fputs("(matchLen+=fklCharBufMatch(\"", fp);
            print_string_in_hex(igs->str, fp);
            fprintf(fp,
                    "\",%" FKL_PRT64U
                    ",*in+otherMatchLen,*restLen-otherMatchLen))",
                    igs->str->size);
        }
        fputs("&&((otherMatchLen+=matchLen)||1)", fp);
    }
    fputs("&&((matchLen=otherMatchLen)||1)", fp);
    fputc(')', fp);
}

static void builtin_match_ignore_print_c_match_cond(void *c,
                                                    const FklGrammer *g,
                                                    FILE *fp) {
    ignore_print_c_match_cond(c, g, fp);
}

static const FklLalrBuiltinMatch builtin_match_ignore = {
    .name = NULL,
    .match = builtin_match_ignore_func,
    .ctx_create = NULL,
    .ctx_destroy = NULL,
    .ctx_global_create = NULL,
    .ctx_cmp = NULL,
    .ctx_hash = NULL,
    .print_src = default_builtin_match_print_src,
    .print_c_match_cond = builtin_match_ignore_print_c_match_cond,
};

static inline FklAnalysisStateAction *
create_builtin_ignore_action(FklGrammer *g, FklGrammerIgnore *ig) {
    FklAnalysisStateAction *action =
        (FklAnalysisStateAction *)malloc(sizeof(FklAnalysisStateAction));
    FKL_ASSERT(action);
    action->next = NULL;
    action->action = FKL_ANALYSIS_IGNORE;
    action->state = NULL;
    if (ig->len == 1) {
        FklGrammerIgnoreSym *igs = &ig->ig[0];
        action->match.t =
            igs->term_type == FKL_TERM_BUILTIN ? FKL_LALR_MATCH_BUILTIN
            : igs->term_type == FKL_TERM_REGEX ? FKL_LALR_MATCH_REGEX
                                               : FKL_LALR_MATCH_STRING;
        if (igs->term_type == FKL_TERM_BUILTIN)
            action->match.func = igs->b;
        else if (igs->term_type == FKL_TERM_REGEX)
            action->match.re = igs->re;
        else
            action->match.str = igs->str;
    } else {
        action->match.t = FKL_LALR_MATCH_BUILTIN;
        action->match.func.t = &builtin_match_ignore;
        action->match.func.c = ig;
    }
    return action;
}

static inline void add_ignore_action(FklGrammer *g,
                                     FklAnalysisState *curState) {
    for (FklGrammerIgnore *ig = g->ignores; ig; ig = ig->next) {
        FklAnalysisStateAction *action = create_builtin_ignore_action(g, ig);
        FklAnalysisStateAction **pa = &curState->state.action;
        for (; *pa; pa = &(*pa)->next)
            if ((*pa)->match.t == FKL_LALR_MATCH_EOF)
                break;
        action->next = *pa;
        *pa = action;
    }
}

// GraProdTable
#define FKL_TABLE_TYPE_PREFIX Gra
#define FKL_TABLE_METHOD_PREFIX gra
#define FKL_TABLE_KEY_TYPE FklGrammerProduction *
#define FKL_TABLE_ELM_NAME Prod
#define FKL_TABLE_KEY_HASH                                                     \
    return fklHash64Shift(                                                     \
        FKL_TYPE_CAST(uintptr_t, (*pk) / alignof(FklGrammerProduction)));
#include <fakeLisp/table.h>

static inline int is_only_single_way_to_reduce(const FklLalrItemSet *set) {
    for (FklLalrItemSetLink *l = set->links; l; l = l->next)
        if (l->sym.isterm)
            return 0;
    GraProdTable prodSet;
    int hasEof = 0;
    int delim = 0;
    graProdTableInit(&prodSet);
    for (const FklLalrItemTableNode *l = set->items.first; l; l = l->next) {
        graProdTablePut2(&prodSet, l->k.prod);
        if (l->k.la.t == FKL_LALR_MATCH_EOF)
            hasEof = 1;
        if (l->k.la.delim)
            delim = 1;
    }
    size_t num = prodSet.count;
    graProdTableUninit(&prodSet);
    if (num != 1)
        return 0;
    return num == 1 && hasEof && delim;
}

int fklGenerateLalrAnalyzeTable(FklGrammer *grammer, FklHashTable *states) {
    int hasConflict = 0;
    grammer->aTable.num = states->num;
    FklSymbolTable *tt = &grammer->terminals;
    FklAnalysisState *astates;
    if (!states->num)
        astates = NULL;
    else {
        astates =
            (FklAnalysisState *)malloc(states->num * sizeof(FklAnalysisState));
        FKL_ASSERT(astates);
    }
    grammer->aTable.states = astates;
    GraItemStateIdxTable idxTable;
    graItemStateIdxTableInit(&idxTable);
    size_t idx = 0;
    for (const FklHashTableItem *l = states->first; l; l = l->next, idx++) {
        const FklLalrItemSet *s = (const FklLalrItemSet *)l->data;
        graItemStateIdxTablePut2(&idxTable, s, idx);
    }
    idx = 0;
    if (astates == NULL)
        goto break_loop;
    for (const FklHashTableItem *l = states->first; l; l = l->next, idx++) {
        int skip_space = 0;
        const FklLalrItemSet *s = (const FklLalrItemSet *)l->data;
        FklAnalysisState *curState = &astates[idx];
        curState->builtin = 0;
        curState->func = NULL;
        curState->state.action = NULL;
        curState->state.gt = NULL;
        const FklLalrItemTable *items = &s->items;
        for (FklLalrItemSetLink *ll = s->links; ll; ll = ll->next) {
            const FklGrammerSym *sym = &ll->sym;
            size_t dstIdx = *graItemStateIdxTableGet2(&idxTable, ll->dst);
            FklAnalysisState *dstState = &astates[dstIdx];
            if (sym->isterm)
                add_shift_action(curState, sym, tt, dstState);
            else
                curState->state.gt =
                    create_state_goto(sym, tt, curState->state.gt, dstState);
        }
        if (is_only_single_way_to_reduce(s)) {
            FklLalrItem const *item = &items->first->k;
            FklLalrItemLookAhead eofla = FKL_LALR_MATCH_EOF_INIT;
            add_reduce_action(curState, item->prod, &eofla);
            skip_space = 1;
        } else {
            for (const FklLalrItemTableNode *il = items->first; il;
                 il = il->next) {
                FklGrammerSym *sym = get_item_next(&il->k);
                if (sym)
                    skip_space = sym->delim;
                else {
                    hasConflict =
                        add_reduce_action(curState, il->k.prod, &il->k.la);
                    if (hasConflict) {
                        clear_analysis_table(grammer, idx);
                        goto break_loop;
                    }
                    if (il->k.la.delim)
                        skip_space = 1;
                }
            }
        }
        if (skip_space)
            add_ignore_action(grammer, curState);
    }

break_loop:
    graItemStateIdxTableUninit(&idxTable);
    return hasConflict;
}

static inline void
print_look_ahead_of_analysis_table(FILE *fp,
                                   const FklAnalysisStateActionMatch *match) {
    switch (match->t) {
    case FKL_LALR_MATCH_STRING: {
        fputc('\'', fp);
        const FklString *str = match->str;
        print_string_as_dot((const uint8_t *)str->str, '\'', str->size, fp);
        fputc('\'', fp);
    } break;
    case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL: {
        fputc('\'', fp);
        const FklString *str = match->str;
        print_string_as_dot((const uint8_t *)str->str, '\'', str->size, fp);
        fputs("\'$", fp);
    } break;
    case FKL_LALR_MATCH_EOF:
        fputs("$$", fp);
        break;
    case FKL_LALR_MATCH_BUILTIN:
        fputs(match->func.t->name, fp);
        break;
    case FKL_LALR_MATCH_NONE:
        fputs("()", fp);
        break;
    case FKL_LALR_MATCH_REGEX:
        fprintf(fp, "/%p/", match->re);
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
    case FKL_LALR_MATCH_NONE:
        return 0;
        break;
    case FKL_LALR_MATCH_EOF:
        return 1;
        break;
    case FKL_LALR_MATCH_STRING:
    case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL:
        return (uintptr_t)match->str;
        break;
    case FKL_LALR_MATCH_REGEX:
        return (uintptr_t)match->re;
        break;
    case FKL_LALR_MATCH_BUILTIN:
        return (uintptr_t)match->func.t + (uintptr_t)match->func.c;
        break;
    }
    return 0;
}

static inline int action_match_equal(const FklAnalysisStateActionMatch *m0,
                                     const FklAnalysisStateActionMatch *m1) {
    if (m0->t == m1->t) {
        switch (m0->t) {
        case FKL_LALR_MATCH_NONE:
        case FKL_LALR_MATCH_EOF:
            return 1;
            break;
        case FKL_LALR_MATCH_STRING:
        case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL:
            return m0->str == m1->str;
            break;
        case FKL_LALR_MATCH_REGEX:
            return m0->re == m1->re;
            break;
        case FKL_LALR_MATCH_BUILTIN:
            return m0->func.t == m1->func.t && m0->func.c == m1->func.c;
        }
    }
    return 0;
}

// GraActionMatchTable
#define FKL_TABLE_TYPE_PREFIX Gra
#define FKL_TABLE_METHOD_PREFIX gra
#define FKL_TABLE_KEY_TYPE FklAnalysisStateActionMatch
#define FKL_TABLE_KEY_EQUAL(A, B) action_match_equal(A, B)
#define FKL_TABLE_KEY_HASH return action_match_hash_func(pk)
#define FKL_TABLE_ELM_NAME ActionMatch
#include <fakeLisp/table.h>

static inline void init_analysis_table_header(GraActionMatchTable *la,
                                              FklNontermTable *nt,
                                              FklAnalysisState *states,
                                              size_t stateNum) {
    graActionMatchTableInit(la);
    fklNontermTableInit(nt);

    for (size_t i = 0; i < stateNum; i++) {
        FklAnalysisState *curState = &states[i];
        for (FklAnalysisStateAction *action = curState->state.action; action;
             action = action->next)
            if (action->action != FKL_ANALYSIS_IGNORE)
                graActionMatchTablePut(la, &action->match);
        for (FklAnalysisStateGoto *gt = curState->state.gt; gt; gt = gt->next)
            fklNontermTablePut(nt, &gt->nt);
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
    case FKL_LALR_MATCH_STRING: {
        fputc('\'', fp);
        print_string_for_grapheasy(la->str, fp);
        fputc('\'', fp);
    } break;
    case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL: {
        fputc('\'', fp);
        print_string_for_grapheasy(la->str, fp);
        fputs("\'$", fp);
    } break;
    case FKL_LALR_MATCH_EOF:
        fputc('$', fp);
        break;
    case FKL_LALR_MATCH_BUILTIN:
        fputs("\\|", fp);
        fputs(la->func.t->name, fp);
        fputs("\\|", fp);
        break;
    case FKL_LALR_MATCH_NONE:
        fputs("()", fp);
        break;
    case FKL_LALR_MATCH_REGEX:
        fprintf(fp, "/%p/", la->re);
        break;
    }
}

static inline void
print_table_header_for_grapheasy(const GraActionMatchTable *la,
                                 const FklNontermTable *sid,
                                 const FklSymbolTable *st, FILE *fp) {
    fputs("\\n|", fp);
    for (GraActionMatchTableNode *al = la->first; al; al = al->next) {
        print_look_ahead_for_grapheasy(&al->k, fp);
        fputc('|', fp);
    }
    fputs("\\n|\\n", fp);
    for (FklNontermTableNode *sl = sid->first; sl; sl = sl->next) {
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

    GraActionMatchTable laTable;
    FklNontermTable sidSet;
    init_analysis_table_header(&laTable, &sidSet, states, num);

    print_table_header_for_grapheasy(&laTable, &sidSet, st, fp);

    GraActionMatchTableNode *laList = laTable.first;
    FklNontermTableNode *sidList = sidSet.first;
    for (size_t i = 0; i < num; i++) {
        const FklAnalysisState *curState = &states[i];
        fprintf(fp, "%" FKL_PRT64U ": |", i);
        for (GraActionMatchTableNode *al = laList; al; al = al->next) {
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
        for (FklNontermTableNode *sl = sidList; sl; sl = sl->next) {
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
    graActionMatchTableUninit(&laTable);
    fklNontermTableUninit(&sidSet);
}

static inline void print_get_max_non_term_length_prototype_to_c_file(FILE *fp) {
    fputs(
        "static inline size_t get_max_non_term_length(FklGrammerMatchOuterCtx*"
        ",const char*"
        ",const char*"
        ",size_t);\n",
        fp);
}

static inline void print_get_max_non_term_length_to_c_file(const FklGrammer *g,
                                                           FILE *fp) {
    fputs("static inline size_t "
          "get_max_non_term_length(FklGrammerMatchOuterCtx* outerCtx\n"
          "\t\t,const char* start\n"
          "\t\t,const char* cur\n"
          "\t\t,size_t rLen)\n{\n"
          "\tif(rLen)\n\t{\n"
          "\t\tif(start==outerCtx->start&&cur==outerCtx->cur)\n\t\t\treturn "
          "outerCtx->maxNonterminalLen;\n"
          "\t\touterCtx->start=start;\n\t\touterCtx->cur=cur;\n"
          "\t\tsize_t len=0;\n"
          "\t\tsize_t matchLen=0;\n"
          "\t\tsize_t otherMatchLen=0;\n"
          "\t\tsize_t* restLen=&rLen;\n"
          "\t\tconst char** in=&cur;\n"
          "\t\tint is_waiting_for_more=0;\n"
          "\t\t(void)is_waiting_for_more;\n"
          "\t\t(void)otherMatchLen;\n"
          "\t\t(void)restLen;\n"
          "\t\t(void)in;\n"
          "\t\twhile(rLen)\n\t\t{\n"
          "\t\t\tif(",
          fp);
    if (g->ignores) {
        const FklGrammerIgnore *igns = g->ignores;
        ignore_print_c_match_cond(igns, g, fp);
        igns = igns->next;
        for (; igns; igns = igns->next) {
            fputs("\n\t\t\t\t\t||", fp);
            ignore_print_c_match_cond(igns, g, fp);
        }
    }
    if (g->ignores && g->sortedTerminalsNum)
        fputs("\n\t\t\t\t\t||", fp);
    if (g->sortedTerminalsNum) {
        size_t num = g->sortedTerminalsNum;
        const FklString **terminals = g->sortedTerminals;
        const FklString *cur = terminals[0];
        fputs("(matchLen=fklCharBufMatch(\"", fp);
        print_string_in_hex(cur, fp);
        fprintf(fp,
                "\",%" FKL_PRT64U ",*in+otherMatchLen,*restLen-otherMatchLen))",
                cur->size);
        for (size_t i = 1; i < num; i++) {
            fputs("\n\t\t\t\t\t||", fp);
            const FklString *cur = terminals[i];
            fputs("(matchLen=fklCharBufMatch(\"", fp);
            print_string_in_hex(cur, fp);
            fprintf(fp,
                    "\",%" FKL_PRT64U
                    ",*in+otherMatchLen,*restLen-otherMatchLen))",
                    cur->size);
        }
    }
    fputs(")\n", fp);
    fputs("\t\t\t\tbreak;\n"
          "\t\t\tlen++;\n"
          "\t\t\trLen--;\n"
          "\t\t\tcur++;\n"
          "\t\t}\n"
          "\t\touterCtx->maxNonterminalLen=len;\n"
          "\t\treturn len;\n"
          "\t}\n\treturn 0;\n}\n",
          fp);
}

static inline void
print_match_char_buf_end_with_terminal_prototype_to_c_file(FILE *fp) {
    fputs("static inline size_t match_char_buf_end_with_terminal(const char*"
          ",size_t"
          ",const char*"
          ",size_t"
          ",FklGrammerMatchOuterCtx* outerCtx"
          ",const char* start);\n",
          fp);
}

static inline void
print_match_char_buf_end_with_terminal_to_c_file(const FklGrammer *g,
                                                 FILE *fp) {
    fputs("static inline size_t match_char_buf_end_with_terminal(const char* "
          "pattern\n"
          "\t\t,size_t pattern_size\n"
          "\t\t,const char* cstr\n"
          "\t\t,size_t restLen\n"
          "\t\t,FklGrammerMatchOuterCtx* outerCtx\n"
          "\t\t,const char* start)\n{"
          "\tsize_t "
          "maxNonterminalLen=get_max_non_term_length(outerCtx,start,cstr,"
          "restLen);\n"
          "\treturn "
          "maxNonterminalLen==pattern_size?fklCharBufMatch(pattern,pattern_"
          "size,cstr,restLen):0;\n"
          "}\n",
          fp);
}

static inline void
print_state_action_match_to_c_file(const FklAnalysisStateAction *ac,
                                   const FklGrammer *g, FILE *fp) {
    switch (ac->match.t) {
    case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL:
        fputs("(matchLen=match_char_buf_end_with_terminal(\"", fp);
        print_string_in_hex(ac->match.str, fp);
        fprintf(fp,
                "\",%" FKL_PRT64U
                ",*in+otherMatchLen,*restLen-otherMatchLen,outerCtx,start))",
                ac->match.str->size);
        break;
    case FKL_LALR_MATCH_STRING:
        fputs("(matchLen=fklCharBufMatch(\"", fp);
        print_string_in_hex(ac->match.str, fp);
        fprintf(fp,
                "\",%" FKL_PRT64U ",*in+otherMatchLen,*restLen-otherMatchLen))",
                ac->match.str->size);
        break;
    case FKL_LALR_MATCH_REGEX: {
        uint64_t num = 0;
        fklGetStringWithRegex(&g->regexes, ac->match.re, &num);
        fputs("regex_lex_match_for_parser_in_c((const FklRegexCode*)&", fp);
        fprintf(fp, PRINT_C_REGEX_PREFIX "%" FKL_PRT64X, num);
        fputs(",*in+otherMatchLen,*restLen-otherMatchLen,&matchLen,&is_waiting_"
              "for_more)",
              fp);
    } break;
    case FKL_LALR_MATCH_BUILTIN:
        ac->match.func.t->print_c_match_cond(ac->match.func.c, g, fp);
        break;
    case FKL_LALR_MATCH_EOF:
        fputs("(matchLen=1)", fp);
        break;
    case FKL_LALR_MATCH_NONE:
        abort();
        break;
    }
}

static inline void
print_state_action_to_c_file(const FklAnalysisStateAction *ac,
                             const FklAnalysisState *states,
                             const char *ast_destroyer_name, FILE *fp) {
    fputs("\t\t{\n", fp);
    switch (ac->action) {
    case FKL_ANALYSIS_SHIFT:
        fprintf(fp,
                "\t\t\tfklParseStateVectorPushBack2(stateStack,(FklParseState){"
                ".func=state_"
                "%" FKL_PRT64U "});\n",
                ac->state - states);
        fputs("\t\t\tinit_term_"
              "analyzing_symbol(fklAnalysisSymbolVectorPushBack(symbolStack,"
              "NULL),*in,matchLen,"
              "outerCtx->line,outerCtx->ctx);\n"
              "\t\t\tfklUintVectorPushBack2(lineStack,outerCtx->line);\n"
              "\t\t\touterCtx->line+=fklCountCharInBuf(*in,matchLen,'\\n');\n"
              "\t\t\t*in+=matchLen;\n"
              "\t\t\t*restLen-=matchLen;\n",
              fp);
        break;
    case FKL_ANALYSIS_ACCEPT:
        fputs("\t\t\t*accept=1;\n", fp);
        break;
    case FKL_ANALYSIS_REDUCE:
        fprintf(fp, "\t\t\tstateStack->size-=%" FKL_PRT64U ";\n",
                ac->prod->len);
        fprintf(fp, "\t\t\tsymbolStack->size-=%" FKL_PRT64U ";\n",
                ac->prod->len);
        fputs("\t\t\tFklStateFuncPtr "
              "func=fklParseStateVectorBack(stateStack)->func;\n",
              fp);
        fputs("\t\t\tFklStateFuncPtr nextState=NULL;\n", fp);
        fprintf(fp,
                "\t\t\tfunc(NULL,NULL,NULL,0,%" FKL_PRT64U
                ",(void**)&nextState,NULL,NULL,NULL,NULL,NULL,NULL);\n",
                ac->prod->left.sid);
        fputs("\t\t\tif(!nextState)\n\t\t\t\treturn FKL_PARSE_REDUCE_FAILED;\n",
              fp);
        fputs("\t\t\tfklParseStateVectorPushBack2(stateStack,(FklParseState){."
              "func=nextState});\n",
              fp);

        if (ac->prod->len)
            fprintf(fp,
                    "\t\t\tvoid** nodes=(void**)malloc(%" FKL_PRT64U
                    "*sizeof(void*));\n"
                    "\t\t\tFKL_ASSERT(nodes||!%" FKL_PRT64U ");\n"
                    "\t\t\tconst FklAnalysisSymbol* "
                    "base=&symbolStack->base[symbolStack->"
                    "size];\n",
                    ac->prod->len, ac->prod->len);
        else
            fputs("\t\t\tvoid** nodes=NULL;\n", fp);

        if (ac->prod->len)
            fprintf(fp,
                    "\t\t\tfor(size_t i=0;i<%" FKL_PRT64U ";i++)\n"
                    "\t\t\t\tnodes[i]=base[i].ast;\n",
                    ac->prod->len);

        if (ac->prod->len) {
            fprintf(
                fp,
                "\t\t\tsize_t line=fklGetFirstNthLine(lineStack,%" FKL_PRT64U
                ",outerCtx->line);\n",
                ac->prod->len);
            fprintf(fp, "\t\t\tlineStack->size-=%" FKL_PRT64U ";\n",
                    ac->prod->len);
        } else
            fputs("\t\t\tsize_t line=outerCtx->line;\n", fp);

        fprintf(fp,
                "\t\t\tvoid* ast=%s(outerCtx->ctx,nodes,%" FKL_PRT64U
                ",line);\n",
                ac->prod->name, ac->prod->len);

        if (ac->prod->len)
            fprintf(fp,
                    "\t\t\tfor(size_t i=0;i<%" FKL_PRT64U ";i++)\n"
                    "\t\t\t\t%s(nodes[i]);\n"
                    "\t\t\tfree(nodes);\n",
                    ac->prod->len, ast_destroyer_name);
        fputs("\t\t\tif(!ast)\n\t\t\t{\n\t\t\t\t*errLine=line;\n\t\t\t\treturn "
              "FKL_PARSE_REDUCE_FAILED;\n\t\t\t}\n",
              fp);
        fprintf(fp,
                "\t\t\tinit_"
                "nonterm_analyzing_symbol(fklAnalysisSymbolVectorPushBack("
                "symbolStack,NULL),"
                "%" FKL_PRT64U ",ast);\n",
                ac->prod->left.sid);
        fputs("\t\t\tfklUintVectorPushBack2(lineStack,line);\n", fp);
        break;
    case FKL_ANALYSIS_IGNORE:
        fputs("\t\t\touterCtx->line+=fklCountCharInBuf(*in,matchLen,'\\n');\n"
              "\t\t\t*in+=matchLen;\n"
              "\t\t\t*restLen-=matchLen;\n"
              "\t\t\tgoto action_match_start;\n",
              fp);
        break;
    }
    fputs("\t\t}\n", fp);
}

static inline void
print_state_prototype_to_c_file(const FklAnalysisState *states, size_t idx,
                                FILE *fp) {
    fprintf(fp,
            "static int state_%" FKL_PRT64U "(FklParseStateVector*"
            ",FklAnalysisSymbolVector*"
            ",FklUintVector*"
            ",int"
            ",FklSid_t"
            ",void** pfunc"
            ",const char*"
            ",const char**"
            ",size_t*"
            ",FklGrammerMatchOuterCtx*"
            ",int*"
            ",size_t*);\n",
            idx);
}

static inline void print_state_to_c_file(const FklAnalysisState *states,
                                         size_t idx, const FklGrammer *g,
                                         const char *ast_destroyer_name,
                                         FILE *fp) {
    const FklAnalysisState *state = &states[idx];
    fprintf(fp,
            "static int state_%" FKL_PRT64U "(FklParseStateVector* stateStack\n"
            "\t\t,FklAnalysisSymbolVector* symbolStack\n"
            "\t\t,FklUintVector* lineStack\n"
            "\t\t,int is_action\n"
            "\t\t,FklSid_t left\n"
            "\t\t,void** pfunc\n"
            "\t\t,const char* start\n"
            "\t\t,const char** in\n"
            "\t\t,size_t* restLen\n"
            "\t\t,FklGrammerMatchOuterCtx* outerCtx\n"
            "\t\t,int* accept\n"
            "\t\t,size_t* errLine)\n{\n",
            idx);
    fputs("\tif(is_action)\n\t{\n", fp);
    fputs("\t\tint is_waiting_for_more=0;\n", fp);
    fputs("\t\t(void)is_waiting_for_more;\n", fp);
    for (const FklAnalysisStateAction *ac = state->state.action; ac;
         ac = ac->next)
        if (ac->action == FKL_ANALYSIS_IGNORE) {
            fputs("action_match_start:\n\t\t;\n", fp);
            break;
        }
    fputs("\t\tsize_t matchLen=0;\n", fp);
    fputs("\t\tsize_t otherMatchLen=0;\n", fp);
    const FklAnalysisStateAction *ac = state->state.action;
    if (ac) {
        fputs("\t\tif(", fp);
        print_state_action_match_to_c_file(ac, g, fp);
        fputs(")\n", fp);
        print_state_action_to_c_file(ac, states, ast_destroyer_name, fp);
        ac = ac->next;
        for (; ac; ac = ac->next) {
            fputs("\t\telse if(", fp);
            print_state_action_match_to_c_file(ac, g, fp);
            fputs(")\n", fp);
            print_state_action_to_c_file(ac, states, ast_destroyer_name, fp);
        }
        fputs("\t\telse\n\t\t\treturn "
              "is_waiting_for_more?FKL_PARSE_WAITING_FOR_MORE:FKL_PARSE_"
              "TERMINAL_MATCH_FAILED;\n",
              fp);
    } else
        fputs("\t\treturn FKL_PARSE_TERMINAL_MATCH_FAILED;\n", fp);
    fputs("\t\t(void)otherMatchLen;\n", fp);
    fputs("\t}\n\telse\n\t{\n", fp);
    const FklAnalysisStateGoto *gt = state->state.gt;
    if (gt) {
        fprintf(fp, "\t\tif(left==%" FKL_PRT64U ")\n\t\t{\n", gt->nt.sid);
        fprintf(fp,
                "\t\t\t*pfunc=(void*)state_%" FKL_PRT64U
                ";\n\t\t\treturn 0;\n\t\t}\n",
                gt->state - states);
        gt = gt->next;
        for (; gt; gt = gt->next) {
            fprintf(fp, "\t\telse if(left==%" FKL_PRT64U ")\n\t\t{\n",
                    gt->nt.sid);
            fprintf(fp,
                    "\t\t\t*pfunc=(void*)state_%" FKL_PRT64U
                    ";\n\t\t\treturn 0;\n\t\t}\n",
                    gt->state - states);
        }
        fputs("\t\telse\n\t\t\treturn FKL_PARSE_REDUCE_FAILED;\n", fp);
    } else
        fputs("\t\treturn FKL_PARSE_REDUCE_FAILED;\n", fp);
    fputs("\t}\n", fp);
    fputs("\treturn 0;\n}\n\n", fp);
}

// builtin match method unordered set
// GraBtmTable
#define FKL_TABLE_TYPE_PREFIX Gra
#define FKL_TABLE_METHOD_PREFIX gra
#define FKL_TABLE_KEY_TYPE FklLalrBuiltinMatch const *
#define FKL_TABLE_ELM_NAME Btm
#define FKL_TABLE_KEY_HASH                                                     \
    return fklHash64Shift(                                                     \
        FKL_TYPE_CAST(uintptr_t, (*pk) / alignof(FklLalrBuiltinMatch)));
#include <fakeLisp/table.h>

static inline void get_all_match_method_table(const FklGrammer *g,
                                              GraBtmTable *ptrSet) {
    for (const FklGrammerIgnore *ig = g->ignores; ig; ig = ig->next) {
        size_t len = ig->len;
        for (size_t i = 0; i < len; i++)
            if (ig->ig[i].term_type == FKL_TERM_BUILTIN)
                graBtmTablePut2(ptrSet, ig->ig[i].b.t);
    }

    const FklAnalysisState *states = g->aTable.states;
    size_t num = g->aTable.num;
    for (size_t i = 0; i < num; i++) {
        for (const FklAnalysisStateAction *ac = states[i].state.action; ac;
             ac = ac->next)
            if (ac->match.t == FKL_LALR_MATCH_BUILTIN)
                graBtmTablePut2(ptrSet, ac->match.func.t);
    }
}

static inline void print_all_builtin_match_func(const FklGrammer *g, FILE *fp) {
    GraBtmTable builtin_match_method_table_set;
    graBtmTableInit(&builtin_match_method_table_set);
    get_all_match_method_table(g, &builtin_match_method_table_set);
    for (GraBtmTableNode *il = builtin_match_method_table_set.first; il;
         il = il->next) {
        const FklLalrBuiltinMatch *t = il->k;
        t->print_src(g, fp);
        fputc('\n', fp);
    }
    graBtmTableUninit(&builtin_match_method_table_set);
}

static inline void print_all_regex(const FklRegexTable *rt, FILE *fp) {
    for (const FklStrRegexTableNode *l = rt->str_re.first; l; l = l->next) {
        fputs("static const ", fp);
        fklRegexPrintAsCwithNum(l->v.re, PRINT_C_REGEX_PREFIX, l->v.num, fp);
        fputc('\n', fp);
    }
}

static inline void print_regex_lex_match_for_parser_in_c_to_c_file(FILE *fp) {
    fputs("static inline int regex_lex_match_for_parser_in_c(const "
          "FklRegexCode* re,const char* cstr,size_t restLen,size_t* "
          "matchLen,int* is_waiting_for_more)\n"
          "{\n"
          "\tint last_is_true=0;\n"
          "\tuint32_t len=fklRegexLexMatchp(re,cstr,restLen,&last_is_true);\n"
          "\tif(len>restLen)\n"
          "\t{\n"
          "\t\t*is_waiting_for_more|=last_is_true;\n"
          "\t\treturn 0;\n"
          "\t}\n"
          "\t*matchLen=len;\n"
          "\treturn 1;\n"
          "}\n",
          fp);
}

void fklPrintAnalysisTableAsCfunc(const FklGrammer *g, const FklSymbolTable *st,
                                  FILE *action_src_fp,
                                  const char *ast_creator_name,
                                  const char *ast_destroyer_name,
                                  const char *state_0_push_func_name,
                                  FILE *fp) {
    static const char *init_term_analyzing_symbol_src =
        "static inline void init_term_analyzing_symbol(FklAnalysisSymbol* "
        "sym,const "
        "char* s,size_t len,size_t line,void* ctx)"
        "{\n"
        "\tvoid* ast=%s(s,len,line,ctx);\n"
        "\tsym->nt.group=0;\n"
        "\tsym->nt.sid=0;\n"
        "\tsym->ast=ast;\n"
        "}\n\n";

    static const char *init_nonterm_analyzing_symbol_src =
        "static inline void "
        "init_nonterm_analyzing_symbol(FklAnalysisSymbol* sym,FklSid_t "
        "id,void* ast)\n"
        "{\n"
        "\tsym->nt.group=0;\n"
        "\tsym->nt.sid=id;\n"
        "\tsym->ast=ast;\n"
        "}\n\n";

    fputs("// Do not edit!\n\n", fp);
    fputs("#include<fakeLisp/grammer.h>\n", fp);
    fputs("#include<fakeLisp/utils.h>\n", fp);
    fputs("#include<ctype.h>\n", fp);

#define BUFFER_SIZE (512)
    char buffer[BUFFER_SIZE];
    size_t size = 0;
    while ((size = fread(buffer, 1, BUFFER_SIZE, action_src_fp)))
        fwrite(buffer, size, 1, fp);
#undef BUFFER_SIZE
    fputc('\n', fp);

    fputc('\n', fp);
    fputc('\n', fp);
    fprintf(fp, init_term_analyzing_symbol_src, ast_creator_name);

    fputs(init_nonterm_analyzing_symbol_src, fp);

    if (g->sortedTerminals || g->sortedTerminalsNum != g->terminals.num) {
        print_get_max_non_term_length_prototype_to_c_file(fp);
        fputc('\n', fp);
    }

    if (g->regexes.num) {
        print_all_regex(&g->regexes, fp);
        print_regex_lex_match_for_parser_in_c_to_c_file(fp);
        fputc('\n', fp);
    }

    if (g->sortedTerminalsNum != g->terminals.num) {
        print_match_char_buf_end_with_terminal_prototype_to_c_file(fp);
        fputc('\n', fp);
    }

    print_all_builtin_match_func(g, fp);
    size_t stateNum = g->aTable.num;
    const FklAnalysisState *states = g->aTable.states;
    if (g->sortedTerminals || g->sortedTerminalsNum != g->terminals.num) {
        print_get_max_non_term_length_to_c_file(g, fp);
        fputc('\n', fp);
    }

    if (g->sortedTerminalsNum != g->terminals.num) {
        print_match_char_buf_end_with_terminal_to_c_file(g, fp);
        fputc('\n', fp);
    }

    for (size_t i = 0; i < stateNum; i++)
        print_state_prototype_to_c_file(states, i, fp);
    fputc('\n', fp);

    for (size_t i = 0; i < stateNum; i++)
        print_state_to_c_file(states, i, g, ast_destroyer_name, fp);
    fprintf(fp,
            "void %s(FklParseStateVector* "
            "stateStack){fklParseStateVectorPushBack2(stateStack,("
            "FklParseState){.func=state_0});}"
            "\n\n",
            state_0_push_func_name);
}

void fklPrintItemStateSet(const FklHashTable *i, const FklGrammer *g,
                          const FklSymbolTable *st, FILE *fp) {
    GraItemStateIdxTable idxTable;
    graItemStateIdxTableInit(&idxTable);
    size_t idx = 0;
    for (const FklHashTableItem *l = i->first; l; l = l->next, idx++) {
        const FklLalrItemSet *s = (const FklLalrItemSet *)l->data;
        graItemStateIdxTablePut2(&idxTable, s, idx);
    }
    for (const FklHashTableItem *l = i->first; l; l = l->next) {
        const FklLalrItemSet *s = (const FklLalrItemSet *)l->data;
        const FklLalrItemTable *i = &s->items;
        idx = *graItemStateIdxTableGet2(&idxTable, s);
        fprintf(fp, "===\nI%" FKL_PRT64U ": ", idx);
        putc('\n', fp);
        fklPrintItemSet(i, g, st, fp);
        putc('\n', fp);
        for (FklLalrItemSetLink *l = s->links; l; l = l->next) {
            FklLalrItemSet *dst = l->dst;
            size_t *c = graItemStateIdxTableGet2(&idxTable, dst);
            fprintf(fp, "I%" FKL_PRT64U "--{ ", idx);
            print_prod_sym(fp, &l->sym, st, &g->terminals, &g->regexes);
            fprintf(fp, " }-->I%" FKL_PRT64U "\n", *c);
        }
        putc('\n', fp);
    }
    graItemStateIdxTableUninit(&idxTable);
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
    for (size_t i = 0; i < len; i++) {
        putc(' ', fp);
        print_prod_sym(fp, &syms[i], st, tt, rt);
    }
    putc('\n', fp);
}

const FklLalrBuiltinMatch *fklGetBuiltinMatch(const FklGraSidBuiltinTable *ht,
                                              FklSid_t id) {
    FklLalrBuiltinMatch const **i = fklGraSidBuiltinTableGet2(ht, id);
    if (i)
        return *i;
    return NULL;
}

int fklIsNonterminalExist(FklProdTable *prods, FklSid_t group_id,
                          FklSid_t sid) {
    return fklProdTableGet2(prods,
                            (FklGrammerNonterm){.group = group_id, .sid = sid})
        != NULL;
}

FklGrammerProduction *fklGetGrammerProductions(const FklGrammer *g,
                                               FklSid_t group, FklSid_t sid) {
    return fklGetProductions(&g->productions, group, sid);
}

FklGrammerProduction *fklGetProductions(const FklProdTable *prods,
                                        FklSid_t group, FklSid_t sid) {
    FklGrammerProduction **pp = fklProdTableGet2(
        prods, (FklGrammerNonterm){.group = group, .sid = sid});
    return pp ? *pp : NULL;
}

static inline void print_ignores(const FklGrammerIgnore *ig,
                                 const FklRegexTable *rt, FILE *fp) {
    for (; ig; ig = ig->next) {
        for (size_t i = 0; i < ig->len; i++) {
            const FklGrammerIgnoreSym *igs = &ig->ig[i];
            if (igs->term_type == FKL_TERM_BUILTIN)
                fputs(igs->b.t->name, fp);
            else if (igs->term_type == FKL_TERM_REGEX) {
                fputs("/'", fp);
                print_string_for_grapheasy(
                    fklGetStringWithRegex(rt, igs->re, NULL), fp);
                fputs("'/", fp);
            } else {
                fputc('\'', fp);
                print_string_for_grapheasy(igs->str, fp);
                fputc('\'', fp);
            }
            fputc(' ', fp);
        }
        fputc('\n', fp);
    }
}

void fklPrintGrammer(FILE *fp, const FklGrammer *grammer, FklSymbolTable *st) {
    const FklSymbolTable *tt = &grammer->terminals;
    const FklRegexTable *rt = &grammer->regexes;
    for (FklProdTableNode *list = grammer->productions.first; list;
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

void *fklDefaultParseForCstr(const char *cstr,
                             FklGrammerMatchOuterCtx *outerCtx, int *err,
                             uint64_t *errLine,
                             FklAnalysisSymbolVector *symbolStack,
                             FklUintVector *lineStack,
                             FklParseStateVector *stateStack) {
    const char *start = cstr;
    size_t restLen = strlen(start);
    void *ast = NULL;
    for (;;) {
        int accept = 0;
        FklStateFuncPtr state = fklParseStateVectorBack(stateStack)->func;
        *err = state(stateStack, symbolStack, lineStack, 1, 0, NULL, start,
                     &cstr, &restLen, outerCtx, &accept, errLine);
        if (*err)
            break;
        if (accept) {
            ast = fklAnalysisSymbolVectorPopBack(symbolStack)->ast;
            break;
        }
    }
    return ast;
}

void *fklDefaultParseForCharBuf(const char *cstr, size_t len, size_t *restLen,
                                FklGrammerMatchOuterCtx *outerCtx, int *err,
                                uint64_t *errLine,
                                FklAnalysisSymbolVector *symbolStack,
                                FklUintVector *lineStack,
                                FklParseStateVector *stateStack) {
    const char *start = cstr;
    *restLen = len;
    void *ast = NULL;
    for (;;) {
        int accept = 0;
        FklStateFuncPtr state = fklParseStateVectorBack(stateStack)->func;
        *err = state(stateStack, symbolStack, lineStack, 1, 0, NULL, start,
                     &cstr, restLen, outerCtx, &accept, errLine);
        if (*err)
            break;
        if (accept) {
            ast = fklAnalysisSymbolVectorPopBack(symbolStack)->ast;
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
        && fklStringCharBufMatch(laString, cstr, restLen);
}

int fklIsStateActionMatch(const FklAnalysisStateActionMatch *match,
                          const char *start, const char *cstr, size_t restLen,
                          size_t *matchLen, FklGrammerMatchOuterCtx *outerCtx,
                          int *is_waiting_for_more, const FklGrammer *g) {
    switch (match->t) {
    case FKL_LALR_MATCH_STRING: {
        const FklString *laString = match->str;
        if (fklStringCharBufMatch(laString, cstr, restLen)) {
            *matchLen = laString->size;
            return 1;
        }
    } break;
    case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL: {
        const FklString *laString = match->str;
        if (match_char_buf_end_with_terminal(laString, cstr, restLen, g,
                                             outerCtx, start)) {
            *matchLen = laString->size;
            return 1;
        }
    } break;

    case FKL_LALR_MATCH_EOF:
        *matchLen = 1;
        return 1;
        break;
    case FKL_LALR_MATCH_BUILTIN:
        if (match->func.t->match(match->func.c, start, cstr, restLen, matchLen,
                                 outerCtx, is_waiting_for_more))
            return 1;
        break;
    case FKL_LALR_MATCH_REGEX: {
        int last_is_true = 0;
        uint32_t len =
            fklRegexLexMatchp(match->re, cstr, restLen, &last_is_true);
        if (len > restLen)
            *is_waiting_for_more |= last_is_true;
        else {
            *matchLen = len;
            return 1;
        }
    } break;
    case FKL_LALR_MATCH_NONE:
        abort();
        break;
    }
    return 0;
}

static inline void dbg_print_state_stack(FklParseStateVector *stateStack,
                                         FklAnalysisState *states) {
    for (size_t i = 0; i < stateStack->size; i++) {
        const FklAnalysisState *curState = stateStack->base[i].state;
        fprintf(stderr, "%" FKL_PRT64U " ", curState - states);
    }
    fputc('\n', stderr);
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
                                   const FklGrammerProduction *prod,
                                   FklGrammerMatchOuterCtx *outerCtx,
                                   FklSymbolTable *st, size_t *errLine) {
    size_t len = prod->len;
    stateStack->size -= len;
    FklAnalysisStateGoto *gt =
        fklParseStateVectorBack(stateStack)->state->state.gt;
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
        nodes = (void **)malloc(len * sizeof(void *));
        FKL_ASSERT(nodes);
        const FklAnalysisSymbol *base = &symbolStack->base[symbolStack->size];
        for (size_t i = 0; i < len; i++) {
            nodes[i] = base[i].ast;
        }
    }
    size_t line = fklGetFirstNthLine(lineStack, len, outerCtx->line);
    lineStack->size -= len;
    void *ast = prod->func(prod->ctx, outerCtx->ctx, nodes, len, line);
    if (len) {
        for (size_t i = 0; i < len; i++)
            outerCtx->destroy(nodes[i]);
        free(nodes);
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

void *fklParseWithTableForCstrDbg(const FklGrammer *g, const char *cstr,
                                  FklGrammerMatchOuterCtx *outerCtx,
                                  FklSymbolTable *st, int *err) {
    const FklAnalysisTable *t = &g->aTable;
    size_t errLine = 0;
    void *ast = NULL;
    const char *start = cstr;
    size_t restLen = strlen(start);
    FklAnalysisSymbolVector symbolStack;
    FklParseStateVector stateStack;
    FklUintVector lineStack;
    fklParseStateVectorInit(&stateStack, 16);
    fklAnalysisSymbolVectorInit(&symbolStack, 16);
    fklUintVectorInit(&lineStack, 16);
    fklParseStateVectorPushBack2(&stateStack,
                                 (FklParseState){.state = &t->states[0]});
    size_t matchLen = 0;
    int waiting_for_more_err = 0;
    for (;;) {
        int is_waiting_for_more = 0;
        const FklAnalysisState *state =
            fklParseStateVectorBack(&stateStack)->state;
        const FklAnalysisStateAction *action = state->state.action;
        dbg_print_state_stack(&stateStack, t->states);
        for (; action; action = action->next)
            if (fklIsStateActionMatch(&action->match, start, cstr, restLen,
                                      &matchLen, outerCtx, &is_waiting_for_more,
                                      g))
                break;
        waiting_for_more_err |= is_waiting_for_more;
        if (action) {
            FKL_ASSERT(action->match.t != FKL_LALR_MATCH_EOF
                       || action->next == NULL);
            switch (action->action) {
            case FKL_ANALYSIS_IGNORE:
                outerCtx->line += fklCountCharInBuf(cstr, matchLen, '\n');
                cstr += matchLen;
                restLen -= matchLen;
                continue;
                break;
            case FKL_ANALYSIS_SHIFT: {
                fklParseStateVectorPushBack2(
                    &stateStack, (FklParseState){.state = action->state});
                fklInitTerminalAnalysisSymbol(
                    fklAnalysisSymbolVectorPushBack(&symbolStack, NULL), cstr,
                    matchLen, outerCtx);
                fklUintVectorPushBack2(&lineStack, outerCtx->line);
                outerCtx->line += fklCountCharInBuf(cstr, matchLen, '\n');
                cstr += matchLen;
                restLen -= matchLen;
            } break;
            case FKL_ANALYSIS_ACCEPT: {
                ast = fklAnalysisSymbolVectorPopBack(&symbolStack)->ast;
            }
                goto break_for;
                break;
            case FKL_ANALYSIS_REDUCE:
                if (do_reduce_action(&stateStack, &symbolStack, &lineStack,
                                     action->prod, outerCtx, st, &errLine)) {
                    *err = FKL_PARSE_REDUCE_FAILED;
                    goto break_for;
                }
                break;
            }
        } else {
            *err = waiting_for_more_err ? FKL_PARSE_WAITING_FOR_MORE
                                        : FKL_PARSE_TERMINAL_MATCH_FAILED;
            break;
        }
    }
break_for:
    fklUintVectorUninit(&lineStack);
    fklParseStateVectorUninit(&stateStack);
    while (!fklAnalysisSymbolVectorIsEmpty(&symbolStack))
        fklDestroyNastNode(fklAnalysisSymbolVectorPopBack(&symbolStack)->ast);
    fklAnalysisSymbolVectorUninit(&symbolStack);
    return ast;
}

void *fklParseWithTableForCstr(const FklGrammer *g, const char *cstr,
                               FklGrammerMatchOuterCtx *outerCtx,
                               FklSymbolTable *st, int *err) {
    const FklAnalysisTable *t = &g->aTable;
    size_t errLine = 0;
    void *ast = NULL;
    const char *start = cstr;
    size_t restLen = strlen(start);
    FklAnalysisSymbolVector symbolStack;
    FklParseStateVector stateStack;
    FklUintVector lineStack;
    fklParseStateVectorInit(&stateStack, 16);
    fklAnalysisSymbolVectorInit(&symbolStack, 16);
    fklUintVectorInit(&lineStack, 16);
    fklParseStateVectorPushBack2(&stateStack,
                                 (FklParseState){.state = &t->states[0]});
    size_t matchLen = 0;
    int waiting_for_more_err = 0;
    for (;;) {
        int is_waiting_for_more = 0;
        const FklAnalysisState *state =
            fklParseStateVectorBack(&stateStack)->state;
        const FklAnalysisStateAction *action = state->state.action;
        for (; action; action = action->next)
            if (fklIsStateActionMatch(&action->match, start, cstr, restLen,
                                      &matchLen, outerCtx, &is_waiting_for_more,
                                      g))
                break;
        waiting_for_more_err |= is_waiting_for_more;
        if (action) {
            switch (action->action) {
            case FKL_ANALYSIS_IGNORE:
                outerCtx->line += fklCountCharInBuf(cstr, matchLen, '\n');
                cstr += matchLen;
                restLen -= matchLen;
                continue;
                break;
            case FKL_ANALYSIS_SHIFT: {
                fklParseStateVectorPushBack2(
                    &stateStack, (FklParseState){.state = action->state});
                fklInitTerminalAnalysisSymbol(
                    fklAnalysisSymbolVectorPushBack(&symbolStack, NULL), cstr,
                    matchLen, outerCtx);
                fklUintVectorPushBack2(&lineStack, outerCtx->line);
                outerCtx->line += fklCountCharInBuf(cstr, matchLen, '\n');
                cstr += matchLen;
                restLen -= matchLen;
            } break;
            case FKL_ANALYSIS_ACCEPT: {
                ast = fklAnalysisSymbolVectorPopBack(&symbolStack)->ast;
            }
                goto break_for;
                break;
            case FKL_ANALYSIS_REDUCE:
                if (do_reduce_action(&stateStack, &symbolStack, &lineStack,
                                     action->prod, outerCtx, st, &errLine)) {
                    *err = FKL_PARSE_REDUCE_FAILED;
                    goto break_for;
                }
                break;
            }
        } else {
            *err = waiting_for_more_err ? FKL_PARSE_WAITING_FOR_MORE
                                        : FKL_PARSE_TERMINAL_MATCH_FAILED;
            break;
        }
    }
break_for:
    fklUintVectorUninit(&lineStack);
    fklParseStateVectorUninit(&stateStack);
    while (!fklAnalysisSymbolVectorIsEmpty(&symbolStack))
        fklDestroyNastNode(fklAnalysisSymbolVectorPopBack(&symbolStack)->ast);
    fklAnalysisSymbolVectorUninit(&symbolStack);
    return ast;
}

void *fklParseWithTableForCharBuf(const FklGrammer *g, const char *cstr,
                                  size_t len, size_t *restLen,
                                  FklGrammerMatchOuterCtx *outerCtx,
                                  FklSymbolTable *st, int *err, size_t *errLine,
                                  FklAnalysisSymbolVector *symbolStack,
                                  FklUintVector *lineStack,
                                  FklParseStateVector *stateStack) {
    *restLen = len;
    void *ast = NULL;
    const char *start = cstr;
    size_t matchLen = 0;
    int waiting_for_more_err = 0;
    for (;;) {
        int is_waiting_for_more = 0;
        const FklAnalysisState *state =
            fklParseStateVectorBack(stateStack)->state;
        const FklAnalysisStateAction *action = state->state.action;
        for (; action; action = action->next)
            if (fklIsStateActionMatch(&action->match, start, cstr, *restLen,
                                      &matchLen, outerCtx, &is_waiting_for_more,
                                      g))
                break;
        waiting_for_more_err |= is_waiting_for_more;
        if (action) {
            switch (action->action) {
            case FKL_ANALYSIS_IGNORE:
                outerCtx->line += fklCountCharInBuf(cstr, matchLen, '\n');
                cstr += matchLen;
                (*restLen) -= matchLen;
                continue;
                break;
            case FKL_ANALYSIS_SHIFT: {
                fklParseStateVectorPushBack2(
                    stateStack, (FklParseState){.state = action->state});
                fklInitTerminalAnalysisSymbol(
                    fklAnalysisSymbolVectorPushBack(symbolStack, NULL), cstr,
                    matchLen, outerCtx);
                fklUintVectorPushBack2(lineStack, outerCtx->line);
                outerCtx->line += fklCountCharInBuf(cstr, matchLen, '\n');
                cstr += matchLen;
                (*restLen) -= matchLen;
            } break;
            case FKL_ANALYSIS_ACCEPT: {
                ast = fklAnalysisSymbolVectorPopBack(symbolStack)->ast;
                return ast;
            } break;
            case FKL_ANALYSIS_REDUCE:
                if (do_reduce_action(stateStack, symbolStack, lineStack,
                                     action->prod, outerCtx, st, errLine)) {
                    *err = FKL_PARSE_REDUCE_FAILED;
                    return NULL;
                }
                break;
            }
        } else {
            *err = waiting_for_more_err ? FKL_PARSE_WAITING_FOR_MORE
                                        : FKL_PARSE_TERMINAL_MATCH_FAILED;
            break;
        }
    }
    return ast;
}

uint64_t fklGetFirstNthLine(FklUintVector *lineStack, size_t num, size_t line) {
    if (num)
        return lineStack->base[lineStack->size - num];
    else
        return line;
}

static inline void *prod_action_symbol(void *ctx, void *outerCtx, void *ast[],
                                       size_t num, size_t line) {
    FklNastNode **nodes = (FklNastNode **)ast;
    const char *start = "|";
    size_t start_size = 1;
    const char *end = "|";
    size_t end_size = 1;

    const FklString *str = nodes[0]->str;
    const char *cstr = str->str;
    size_t cstr_size = str->size;

    FklStringBuffer buffer;
    fklInitStringBuffer(&buffer);
    const char *end_cstr = cstr + str->size;
    while (cstr < end_cstr) {
        if (fklCharBufMatch(start, start_size, cstr, cstr_size)) {
            cstr += start_size;
            cstr_size -= start_size;
            size_t len = fklQuotedCharBufMatch(cstr, cstr_size, end, end_size);
            if (!len)
                return 0;
            size_t size = 0;
            char *s = fklCastEscapeCharBuf(cstr, len - end_size, &size);
            fklStringBufferBincpy(&buffer, s, size);
            free(s);
            cstr += len;
            cstr_size -= len;
            continue;
        }
        size_t len = 0;
        for (; (cstr + len) < end_cstr; len++)
            if (fklCharBufMatch(start, start_size, cstr + len, cstr_size - len))
                break;
        fklStringBufferBincpy(&buffer, cstr, len);
        cstr += len;
        cstr_size -= len;
    }
    FklSid_t id = fklAddSymbolCharBuf(buffer.buf, buffer.index, outerCtx)->v;
    FklNastNode *node = fklCreateNastNode(FKL_NAST_SYM, nodes[0]->curline);
    node->sym = id;
    fklUninitStringBuffer(&buffer);
    return node;
}

static inline void *prod_action_return_first(void *ctx, void *outerCtx,
                                             void *nodes[], size_t num,
                                             size_t line) {
    return fklMakeNastNodeRef(nodes[0]);
}

static inline void *prod_action_return_second(void *ctx, void *outerCtx,
                                              void *nodes[], size_t num,
                                              size_t line) {
    return fklMakeNastNodeRef(nodes[1]);
}

static inline void *prod_action_string(void *ctx, void *outerCtx, void *ast[],
                                       size_t num, size_t line) {
    FklNastNode **nodes = (FklNastNode **)ast;
    const size_t start_size = 1;
    const size_t end_size = 1;

    const FklString *str = nodes[0]->str;
    const char *cstr = str->str;

    size_t size = 0;
    char *s = fklCastEscapeCharBuf(&cstr[start_size],
                                   str->size - end_size - start_size, &size);
    FklNastNode *node = fklCreateNastNode(FKL_NAST_STR, nodes[0]->curline);
    node->str = fklCreateString(size, s);
    free(s);
    return node;
}

static inline void *prod_action_nil(void *ctx, void *outerCtx, void *nodes[],
                                    size_t num, size_t line) {
    return fklCreateNastNode(FKL_NAST_NIL, line);
}

static inline void *prod_action_pair(void *ctx, void *outerCtx, void *nodes[],
                                     size_t num, size_t line) {
    FklNastNode *car = nodes[0];
    FklNastNode *cdr = nodes[2];
    FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    pair->pair = fklCreateNastPair();
    pair->pair->car = fklMakeNastNodeRef(car);
    pair->pair->cdr = fklMakeNastNodeRef(cdr);
    return pair;
}

static inline void *prod_action_list(void *ctx, void *outerCtx, void *nodes[],
                                     size_t num, size_t line) {
    FklNastNode *car = nodes[0];
    FklNastNode *cdr = nodes[1];
    FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    pair->pair = fklCreateNastPair();
    pair->pair->car = fklMakeNastNodeRef(car);
    pair->pair->cdr = fklMakeNastNodeRef(cdr);
    return pair;
}

static inline void *prod_action_dec_integer(void *ctx, void *outerCtx,
                                            void *nodes[], size_t num,
                                            size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0])->str;
    int64_t i = strtoll(str->str, NULL, 10);
    FklNastNode *r = fklCreateNastNode(FKL_NAST_FIX, line);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN) {
        FklBigInt bInt = FKL_BIGINT_0;
        fklInitBigIntWithDecCharBuf(&bInt, str->str, str->size);
        FklBigInt *bi = (FklBigInt *)malloc(sizeof(FklBigInt));
        FKL_ASSERT(bi);
        *bi = bInt;
        r->bigInt = bi;
        r->type = FKL_NAST_BIGINT;
    } else {
        r->type = FKL_NAST_FIX;
        r->fix = i;
    }
    return r;
}

static inline void *prod_action_hex_integer(void *ctx, void *outerCtx,
                                            void *nodes[], size_t num,
                                            size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0])->str;
    int64_t i = strtoll(str->str, NULL, 16);
    FklNastNode *r = fklCreateNastNode(FKL_NAST_FIX, line);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN) {
        FklBigInt bInt = FKL_BIGINT_0;
        fklInitBigIntWithHexCharBuf(&bInt, str->str, str->size);
        FklBigInt *bi = (FklBigInt *)malloc(sizeof(FklBigInt));
        FKL_ASSERT(bi);
        *bi = bInt;
        r->bigInt = bi;
        r->type = FKL_NAST_BIGINT;
    } else {
        r->type = FKL_NAST_FIX;
        r->fix = i;
    }
    return r;
}

static inline void *prod_action_oct_integer(void *ctx, void *outerCtx,
                                            void *nodes[], size_t num,
                                            size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0])->str;
    int64_t i = strtoll(str->str, NULL, 8);
    FklNastNode *r = fklCreateNastNode(FKL_NAST_FIX, line);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN) {
        FklBigInt bInt = FKL_BIGINT_0;
        fklInitBigIntWithOctCharBuf(&bInt, str->str, str->size);
        FklBigInt *bi = (FklBigInt *)malloc(sizeof(FklBigInt));
        FKL_ASSERT(bi);
        *bi = bInt;
        r->bigInt = bi;
        r->type = FKL_NAST_BIGINT;
    } else {
        r->type = FKL_NAST_FIX;
        r->fix = i;
    }
    return r;
}

static inline void *prod_action_float(void *ctx, void *outerCtx, void *nodes[],
                                      size_t num, size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0])->str;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_F64, line);
    double i = strtod(str->str, NULL);
    r->f64 = i;
    return r;
}

static inline void *prod_action_char(void *ctx, void *outerCtx, void *nodes[],
                                     size_t num, size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0])->str;
    if (!fklIsValidCharBuf(str->str + 2, str->size - 2))
        return NULL;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_CHR, line);
    r->chr = fklCharBufToChar(str->str + 2, str->size - 2);
    return r;
}

static inline void *prod_action_box(void *ctx, void *outerCtx, void *nodes[],
                                    size_t num, size_t line) {
    FklNastNode *box = fklCreateNastNode(FKL_NAST_BOX, line);
    box->box = fklMakeNastNodeRef(nodes[1]);
    return box;
}

static inline void *prod_action_vector(void *ctx, void *outerCtx, void *nodes[],
                                       size_t num, size_t line) {
    FklNastNode *list = nodes[1];
    FklNastNode *r = fklCreateNastNode(FKL_NAST_VECTOR, line);
    size_t len = fklNastListLength(list);
    FklNastVector *vec = fklCreateNastVector(len);
    r->vec = vec;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++)
        vec->base[i] = fklMakeNastNodeRef(list->pair->car);
    return r;
}

static inline FklNastNode *create_nast_list(FklNastNode *a[], size_t num,
                                            uint64_t line) {
    FklNastNode *r = NULL;
    FklNastNode **cur = &r;
    for (size_t i = 0; i < num; i++) {
        (*cur) = fklCreateNastNode(FKL_NAST_PAIR, a[i]->curline);
        (*cur)->pair = fklCreateNastPair();
        (*cur)->pair->car = a[i];
        cur = &(*cur)->pair->cdr;
    }
    (*cur) = fklCreateNastNode(FKL_NAST_NIL, line);
    return r;
}

static inline void *prod_action_quote(void *ctx, void *outerCtx, void *nodes[],
                                      size_t num, size_t line) {
    FklSid_t id = fklAddSymbolCstr("quote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static inline void *prod_action_unquote(void *ctx, void *outerCtx,
                                        void *nodes[], size_t num,
                                        size_t line) {
    FklSid_t id = fklAddSymbolCstr("unquote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static inline void *prod_action_qsquote(void *ctx, void *outerCtx,
                                        void *nodes[], size_t num,
                                        size_t line) {
    FklSid_t id = fklAddSymbolCstr("qsquote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static inline void *prod_action_unqtesp(void *ctx, void *outerCtx,
                                        void *nodes[], size_t num,
                                        size_t line) {
    FklSid_t id = fklAddSymbolCstr("unqtesp", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static inline void *prod_action_kv_list(void *ctx, void *outerCtx,
                                        void *nodes[], size_t num,
                                        size_t line) {
    FklNastNode *car = nodes[1];
    FklNastNode *cdr = nodes[3];
    FklNastNode *rest = nodes[5];

    FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    pair->pair = fklCreateNastPair();
    pair->pair->car = fklMakeNastNodeRef(car);
    pair->pair->cdr = fklMakeNastNodeRef(cdr);

    FklNastNode *r = fklCreateNastNode(FKL_NAST_PAIR, line);
    r->pair = fklCreateNastPair();
    r->pair->car = pair;
    r->pair->cdr = fklMakeNastNodeRef(rest);
    return r;
}

static inline void *prod_action_hasheq(void *ctx, void *outerCtx, void *nodes[],
                                       size_t num, size_t line) {
    FklNastNode *list = nodes[1];
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQ, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *prod_action_hasheqv(void *ctx, void *outerCtx,
                                        void *nodes[], size_t num,
                                        size_t line) {
    FklNastNode *list = nodes[1];
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQV, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *prod_action_hashequal(void *ctx, void *outerCtx,
                                          void *nodes[], size_t num,
                                          size_t line) {
    FklNastNode *list = nodes[1];
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQUAL, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *prod_action_bytevector(void *ctx, void *outerCtx,
                                           void *ast[], size_t num,
                                           size_t line) {
    FklNastNode **nodes = (FklNastNode **)ast;
    const size_t start_size = 2;
    const size_t end_size = 1;

    const FklString *str = nodes[0]->str;
    const char *cstr = str->str;

    size_t size = 0;
    char *s = fklCastEscapeCharBuf(&cstr[start_size],
                                   str->size - end_size - start_size, &size);
    FklNastNode *node =
        fklCreateNastNode(FKL_NAST_BYTEVECTOR, nodes[0]->curline);
    node->bvec = fklCreateBytevector(size, (uint8_t *)s);
    free(s);
    return node;
}

static const FklGrammerCstrAction builtin_grammer_and_action[] = {
    // clang-format off
    {"*s-exp* &*list*",                                       "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*vector*",                                     "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*bytevector*",                                 "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*box*",                                        "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*hasheq*",                                     "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*hasheqv*",                                    "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*hashequal*",                                  "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*string*",                                     "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*symbol*",                                     "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*integer*",                                    "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*float*",                                      "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*char*",                                       "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*quote*",                                      "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*qsquote*",                                    "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*unquote*",                                    "prod_action_return_first",  prod_action_return_first  },
    {"*s-exp* &*unqtesp*",                                    "prod_action_return_first",  prod_action_return_first  },

    {"*quote* #' &*s-exp*",                                   "prod_action_quote",         prod_action_quote         },
    {"*qsquote* #` &*s-exp*",                                 "prod_action_qsquote",       prod_action_qsquote       },
    {"*unquote* #~ &*s-exp*",                                 "prod_action_unquote",       prod_action_unquote       },
    {"*unqtesp* #~@ &*s-exp*",                                "prod_action_unqtesp",       prod_action_unqtesp       },

    {"*symbol* &?symbol + #|",                                "prod_action_symbol",        prod_action_symbol        },

    {"*string* /\"\"|^\"(\\\\.|.)*\"$",                       "prod_action_string",        prod_action_string        },

    {"*bytevector* /#\"\"|^#\"(\\\\.|.)*\"$",                 "prod_action_bytevector",    prod_action_bytevector    },

    // s-dint can accepet the next terminator as its arguement
    {"*integer* &?s-dint + #|",                               "prod_action_dec_integer",   prod_action_dec_integer   },
    {"*integer* &?s-xint + #|",                               "prod_action_hex_integer",   prod_action_hex_integer   },
    {"*integer* &?s-oint + #|",                               "prod_action_oct_integer",   prod_action_oct_integer   },

    {"*float* &?s-dfloat + #|",                               "prod_action_float",         prod_action_float         },
    {"*float* &?s-xfloat + #|",                               "prod_action_float",         prod_action_float         },

    {"*char* &?char + ##\\",                                  "prod_action_char",          prod_action_char          },

    {"*box* ##& &*s-exp*",                                    "prod_action_box",           prod_action_box           },

    {"*list* #( &*list-items* #)",                            "prod_action_return_second", prod_action_return_second },
    {"*list* #[ &*list-items* #]",                            "prod_action_return_second", prod_action_return_second },

    {"*list-items* ",                                         "prod_action_nil",           prod_action_nil           },
    {"*list-items* &*s-exp* &*list-items*",                   "prod_action_list",          prod_action_list          },
    {"*list-items* &*s-exp* #, &*s-exp*",                     "prod_action_pair",          prod_action_pair          },

    {"*vector* ##( &*vector-items* #)",                       "prod_action_vector",        prod_action_vector        },
    {"*vector* ##[ &*vector-items* #]",                       "prod_action_vector",        prod_action_vector        },

    {"*vector-items* ",                                       "prod_action_nil",           prod_action_nil           },
    {"*vector-items* &*s-exp* &*vector-items*",               "prod_action_list",          prod_action_list          },

    {"*hasheq* ##hash( &*hash-items* #)",                     "prod_action_hasheq",        prod_action_hasheq        },
    {"*hasheq* ##hash[ &*hash-items* #]",                     "prod_action_hasheq",        prod_action_hasheq        },

    {"*hasheqv* ##hasheqv( &*hash-items* #)",                 "prod_action_hasheqv",       prod_action_hasheqv       },
    {"*hasheqv* ##hasheqv[ &*hash-items* #]",                 "prod_action_hasheqv",       prod_action_hasheqv       },

    {"*hashequal* ##hashequal( &*hash-items* #)",             "prod_action_hashequal",     prod_action_hashequal     },
    {"*hashequal* ##hashequal[ &*hash-items* #]",             "prod_action_hashequal",     prod_action_hashequal     },

    {"*hash-items* ",                                         "prod_action_nil",           prod_action_nil           },
    {"*hash-items* #( &*s-exp* #, &*s-exp* #) &*hash-items*", "prod_action_kv_list",       prod_action_kv_list       },
    {"*hash-items* #[ &*s-exp* #, &*s-exp* #] &*hash-items*", "prod_action_kv_list",       prod_action_kv_list       },

    {"+ /\\s+",                                               NULL,                        NULL                      },
    {"+ /^;.*\\n?",                                           NULL,                        NULL                      },
    {"+ /^#!.*\\n?",                                          NULL,                        NULL                      },
    {NULL,                                                    NULL,                        NULL                      },
    // clang-format on
};

FklGrammer *fklCreateBuiltinGrammer(FklSymbolTable *st) {
    return fklCreateGrammerFromCstrAction(builtin_grammer_and_action, st);
}

FklGrammerIgnore *fklInitBuiltinProductionSet(FklProdTable *ht,
                                              FklSymbolTable *st,
                                              FklSymbolTable *tt,
                                              FklRegexTable *rt,
                                              FklGraSidBuiltinTable *builtins) {
    FklGrammerIgnore *ignores = NULL;
    const FklGrammerCstrAction *pa = builtin_grammer_and_action;
    for (; pa->cstr; pa++) {
        const char *str = pa->cstr;
        if (*str == '+') {
            FklGrammerIgnore *ignore =
                create_grammer_ignore_from_cstr(str, builtins, st, tt, rt);
            if (!ignore)
                return NULL;
            if (fklAddIgnoreToIgnoreList(&ignores, ignore)) {
                fklDestroyIgnore(ignore);
                return NULL;
            }
        } else {
            FklGrammerProduction *prod = create_grammer_prod_from_cstr(
                str, builtins, st, tt, rt, pa->action_name, pa->func);
            if (prod == NULL || fklAddProdToProdTable(ht, builtins, prod)) {
                fklDestroyGrammerProduction(prod);
                return NULL;
            }
        }
    }
    return ignores;
}
