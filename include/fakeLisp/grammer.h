#ifndef FKL_GRAMMER_H
#define FKL_GRAMMER_H

#include "base.h"
#include "code_builder.h"
#include "common.h"
#include "regex.h"
#include "string_table.h"
#include "symbol.h"

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct FklGrammerProduction;
struct FklGrammer;
struct FklGrammerSym;
struct FklAnalysisSymbol;

typedef struct {
    size_t maxNonterminalLen;
    uint64_t line;
    const char *start;
    const char *cur;
    void *ctx;
    void *(*create)(const char *s, size_t len, size_t line, void *ctx);
    void (*destroy)(void *s);
} FklGrammerMatchOuterCtx;

typedef enum FklBuiltinTerminalInitError {
    FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY = 0,
    FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS,
    FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_FEW_ARGS,
} FklBuiltinTerminalInitError;

typedef struct {
    const struct FklGrammer *g;
    size_t len;
    FklString const **args;
} FklBuiltinTerminalMatchArgs;

typedef struct {
    int (*match)(const FklBuiltinTerminalMatchArgs *args,
            const char *start,
            const char *str,
            size_t restLen,
            size_t *matchLen,
            FklGrammerMatchOuterCtx *outerCtx,
            int *is_waiting_for_more);

    FklBuiltinTerminalInitError (*ctx_create)(size_t len,
            const FklString **args,
            struct FklGrammer *g);
    const char *name;
    void (*build_src)(const struct FklGrammer *g, FklCodeBuilder *build);
    void (*build_c_match_cond)(const FklBuiltinTerminalMatchArgs *args,
            FklCodeBuilder *build);
} FklLalrBuiltinMatch;

typedef struct {
    const FklLalrBuiltinMatch *t;
    size_t len;
    FklString const **args;
} FklLalrBuiltinGrammerSym;

typedef struct {
    FklSid_t group;
    FklSid_t sid;
} FklGrammerNonterm;

// FklNontermHashSet
#define FKL_HASH_KEY_TYPE FklGrammerNonterm
#define FKL_HASH_KEY_EQUAL(A, B)                                               \
    (A)->group == (B)->group && (A)->sid == (B)->sid
#define FKL_HASH_KEY_HASH                                                      \
    return fklHashCombine(fklHash32Shift(pk->group), pk->sid)
#define FKL_HASH_ELM_NAME Nonterm
#include "cont/hash.h"

typedef enum {
    FKL_TERM_NONE = 0,
    FKL_TERM_KEYWORD,
    FKL_TERM_STRING,
    FKL_TERM_REGEX,
    FKL_TERM_BUILTIN,
    FKL_TERM_IGNORE,
    FKL_TERM_EOF,
    FKL_TERM_NONTERM,
} FklGrammerSymType;

#define FKL_LALR_MATCH_NONE_INIT                                               \
    ((FklLalrItemLookAhead){                                                   \
        .t = FKL_TERM_NONE,                                                    \
    })
#define FKL_LALR_MATCH_EOF_INIT                                                \
    ((FklLalrItemLookAhead){                                                   \
        .t = FKL_TERM_EOF,                                                     \
    })

typedef struct {
    FklGrammerSymType t;
    union {
        const FklString *s;
        FklLalrBuiltinGrammerSym b;
        const FklRegexCode *re;
    };
} FklLalrItemLookAhead;

static inline int fklBuiltinGrammerSymEqual(const FklLalrBuiltinGrammerSym *s0,
        const FklLalrBuiltinGrammerSym *s1) {
    if (s0->t != s1->t)
        return 0;
    if (s0->len != s1->len)
        return 0;
    for (size_t i = 0; i < s0->len; ++i) {
        if (!fklStringEqual(s0->args[i], s1->args[i]))
            return 0;
    }
    return 1;
}

static inline int fklLalrItemLookAheadEqual(const FklLalrItemLookAhead *la0,
        const FklLalrItemLookAhead *la1) {
    if (la0->t != la1->t)
        return 0;
    switch (la0->t) {
    case FKL_TERM_NONE:
    case FKL_TERM_EOF:
        return 1;
        break;
    case FKL_TERM_STRING:
    case FKL_TERM_KEYWORD:
        return la0->s == la1->s;
        break;
    case FKL_TERM_REGEX:
        return la0->re == la1->re;
        break;
    case FKL_TERM_BUILTIN:
        return fklBuiltinGrammerSymEqual(&la0->b, &la1->b);
        break;
    case FKL_TERM_IGNORE:
        return 1;
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }
    return 0;
}

static uintptr_t fklBuiltinGrammerSymHash(const FklLalrBuiltinGrammerSym *s) {
    uintptr_t seed = s->len;
    for (size_t i = 0; i < s->len; ++i) {
        seed = fklHashCombine(seed, fklStringHash(s->args[i]));
    }
    return fklHashCombine(FKL_TYPE_CAST(uintptr_t, s->t), seed);
}

static uintptr_t fklLalrItemLookAheadHash(const FklLalrItemLookAhead *pk) {
    uintptr_t rest = 0;
    switch (pk->t) {
    case FKL_TERM_KEYWORD:
    case FKL_TERM_STRING:
        rest = FKL_TYPE_CAST(uintptr_t, pk->s);
        break;
    case FKL_TERM_REGEX:
        rest = FKL_TYPE_CAST(uintptr_t, pk->re);
        break;
    case FKL_TERM_BUILTIN:
        rest = fklBuiltinGrammerSymHash(&pk->b);
        break;
    case FKL_TERM_EOF:
    case FKL_TERM_NONE:
    case FKL_TERM_IGNORE:
        rest = 0;
        break;
    case FKL_TERM_NONTERM:
        FKL_UNREACHABLE();
        break;
    }

    return fklHashCombine(rest, FKL_TYPE_CAST(uintptr_t, pk->t));
}

// FklLookAheadHashSet
#define FKL_HASH_KEY_TYPE FklLalrItemLookAhead
#define FKL_HASH_KEY_EQUAL(A, B) fklLalrItemLookAheadEqual(A, B)
#define FKL_HASH_KEY_HASH return fklLalrItemLookAheadHash(pk)
#define FKL_HASH_ELM_NAME LookAhead
#include "cont/hash.h"

typedef struct {
    FklLookAheadHashSet first;
    int hasEpsilon;
} FklFirstSetItem;

static inline int fklNontermEqual(const FklGrammerNonterm *nt0,
        const FklGrammerNonterm *nt1) {
    return nt0->group == nt1->group && nt0->sid == nt1->sid;
}

static inline uintptr_t fklNontermHash(const FklGrammerNonterm *pk) {
    return fklHashCombine(fklHash32Shift(pk->group), pk->sid);
}

// FklFirstSetHashMap
#define FKL_HASH_KEY_TYPE FklGrammerNonterm
#define FKL_HASH_KEY_EQUAL(A, B) fklNontermEqual(A, B)
#define FKL_HASH_KEY_HASH return fklNontermHash(pk)
#define FKL_HASH_VAL_TYPE FklFirstSetItem
#define FKL_HASH_VAL_INIT(V, X)                                                \
    {                                                                          \
        fklLookAheadHashSetInit(&(V)->first);                                  \
        (V)->hasEpsilon = 0;                                                   \
    }
#define FKL_HASH_VAL_UNINIT(V) fklLookAheadHashSetUninit(&(V)->first)
#define FKL_HASH_ELM_NAME FirstSet
#include "cont/hash.h"

typedef struct FklGrammerSym {
    FklGrammerSymType type;
    union {
        FklGrammerNonterm nt;
        FklLalrBuiltinGrammerSym b;
        const FklRegexCode *re;
        const FklString *str;
    };
} FklGrammerSym;

// FklGraSymVector
#define FKL_VECTOR_ELM_TYPE FklGrammerSym
#define FKL_VECTOR_ELM_TYPE_NAME GraSym
#include "cont/vector.h"

struct FklGrammerProduction;

uint64_t fklGetFirstNthLine(FklUintVector *lineStack, size_t num, size_t line);

typedef void *(*FklProdActionFunc)(void *args,
        void *ctx,
        void *asts[],
        size_t num,
        size_t line);

typedef struct FklGrammerProduction {
    struct FklGrammerProduction *next;
    const char *name;
    void *ctx;
    FklProdActionFunc func;
    void *(*ctx_copyer)(const void *);
    void (*ctx_destroyer)(void *);
    FklGrammerNonterm left;
    size_t len;
    size_t idx;
    FklGrammerSym syms[];
} FklGrammerProduction;

void fklDestroyGrammerProduction(FklGrammerProduction *h);

// FklProdHashMap
#define FKL_HASH_KEY_TYPE FklGrammerNonterm
#define FKL_HASH_VAL_TYPE FklGrammerProduction *
#define FKL_HASH_KEY_EQUAL(A, B)                                               \
    (A)->group == (B)->group && (A)->sid == (B)->sid
#define FKL_HASH_KEY_HASH                                                      \
    return fklHashCombine(fklHash32Shift(pk->group), pk->sid)
#define FKL_HASH_ELM_NAME Prod
#define FKL_HASH_VAL_UNINIT(V)                                                 \
    {                                                                          \
        FklGrammerProduction *h = *(V);                                        \
        while (h) {                                                            \
            FklGrammerProduction *next = h->next;                              \
            fklDestroyGrammerProduction(h);                                    \
            h = next;                                                          \
        }                                                                      \
        *(V) = NULL;                                                           \
    }
#include "cont/hash.h"

// FklGraSidBuiltinHashMap
#define FKL_HASH_KEY_TYPE FklSid_t
#define FKL_HASH_VAL_TYPE FklLalrBuiltinMatch const *
#define FKL_HASH_ELM_NAME GraSidBuiltin
#include "cont/hash.h"

typedef struct {
    FklGrammerProduction *prod;
    uint32_t idx;
    FklLalrItemLookAhead la;
} FklLalrItem;

static inline uintptr_t fklLalrItemEqual(const FklLalrItem *a,
        const FklLalrItem *b) {
    return a->prod == b->prod && a->idx == b->idx
        && fklLalrItemLookAheadEqual(&a->la, &b->la);
}

static inline uintptr_t fklLalrItemHash(const FklLalrItem *pk) {
    return fklHashCombine(
            fklHashCombine(FKL_TYPE_CAST(uintptr_t, pk->prod), pk->idx),
            fklLalrItemLookAheadHash(&pk->la));
}

// FklLalrItemHashSet
#define FKL_HASH_KEY_TYPE FklLalrItem
#define FKL_HASH_KEY_EQUAL(A, B) fklLalrItemEqual((A), (B))
#define FKL_HASH_KEY_HASH return fklLalrItemHash(pk)
#define FKL_HASH_ELM_NAME LalrItem
#include "cont/hash.h"

typedef struct FklLalrItemSetLink {
    FklGrammerSym sym;
    int allow_ignore;
    struct FklLalrItemSetHashMapElm *dst;
    struct FklLalrItemSetLink *next;
} FklLalrItemSetLink;

typedef struct FklLookAheadSpreads {
    FklLalrItem src;
    FklLalrItemHashSet *dstItems;
    FklLalrItem dst;
    struct FklLookAheadSpreads *next;
} FklLookAheadSpreads;

static inline int fklLalrItemHashSetEqual(const FklLalrItemHashSet *s0,
        const FklLalrItemHashSet *s1) {
    if (s0->count != s1->count)
        return 0;
    const FklLalrItemHashSetNode *l0 = s0->first;
    const FklLalrItemHashSetNode *l1 = s1->first;
    for (; l0; l0 = l0->next, l1 = l1->next)
        if (!fklLalrItemEqual(&l0->k, &l1->k))
            return 0;
    return 1;
}

static inline uintptr_t fklLalrItemHashSetHash(const FklLalrItemHashSet *i) {
    uintptr_t v = 0;
    for (FklLalrItemHashSetNode *l = i->first; l; l = l->next) {
        v = fklHashCombine(v, fklLalrItemHash(&l->k));
    }
    return v;
}

typedef struct FklLalrItemSetHashMapItem {
    FklLookAheadSpreads *spreads;
    FklLalrItemSetLink *links;
} FklLalrItemSetHashMapItem;

// FklLalrItemSetHashSet
#define FKL_HASH_KEY_TYPE FklLalrItemHashSet
#define FKL_HASH_KEY_EQUAL(A, B) fklLalrItemHashSetEqual(A, B)
#define FKL_HASH_KEY_HASH return fklLalrItemHashSetHash(pk)
#define FKL_HASH_KEY_UNINIT(K) fklLalrItemHashSetUninit(K)
#define FKL_HASH_VAL_INIT(A, B) abort()
#define FKL_HASH_VAL_TYPE FklLalrItemSetHashMapItem
#define FKL_HASH_VAL_UNINIT(V)                                                 \
    {                                                                          \
        FklLalrItemSetLink *l = (V)->links;                                    \
        while (l) {                                                            \
            FklLalrItemSetLink *next = l->next;                                \
            fklZfree(l);                                                       \
            l = next;                                                          \
        }                                                                      \
        FklLookAheadSpreads *sp = (V)->spreads;                                \
        while (sp) {                                                           \
            FklLookAheadSpreads *next = sp->next;                              \
            fklZfree(sp);                                                      \
            sp = next;                                                         \
        }                                                                      \
    }
#define FKL_HASH_ELM_NAME LalrItemSet
#include "cont/hash.h"

typedef enum {
    FKL_ANALYSIS_SHIFT,
    FKL_ANALYSIS_ACCEPT,
    FKL_ANALYSIS_REDUCE,
    FKL_ANALYSIS_IGNORE,
} FklAnalysisStateActionEnum;

typedef struct FklAnalysisStateGoto {
    int allow_ignore;
    FklGrammerNonterm nt;
    struct FklAnalysisState *state;
    struct FklAnalysisStateGoto *next;
} FklAnalysisStateGoto;

typedef struct {
    FklGrammerSymType t;
    int allow_ignore;
    union {
        const FklString *str;
        FklLalrBuiltinGrammerSym func;
        const FklRegexCode *re;
    };
} FklAnalysisStateActionMatch;

typedef struct FklAnalysisStateAction {
    FklAnalysisStateActionMatch match;

    FklAnalysisStateActionEnum action;
    union {
        const struct FklAnalysisState *state;
        struct {
            const FklGrammerProduction *prod;
            size_t actual_len;
        };
    };

    struct FklAnalysisStateAction *next;
} FklAnalysisStateAction;

typedef struct FklAnalysisState {
    union {
        void *func;
        struct {
            FklAnalysisStateAction *action;
            FklAnalysisStateGoto *gt;
        } state;
    };
} FklAnalysisState;

typedef struct FklAnalysisTable {
    size_t num;
    FklAnalysisState *states;
} FklAnalysisTable;

typedef struct {
    union {
        const FklString *str;
        FklLalrBuiltinGrammerSym b;
        const FklRegexCode *re;
    };
    FklGrammerSymType term_type;
} FklGrammerIgnoreSym;

typedef struct FklGrammerIgnore {
    struct FklGrammerIgnore *next;
    size_t len;
    FklGrammerIgnoreSym ig[];
} FklGrammerIgnore;

typedef struct FklGrammer {
    FklSymbolTable *st;
    FklStringTable terminals;

    FklStringTable delimiters;

    FklRegexTable regexes;

    size_t sorted_delimiters_num;
    const FklString **sorted_delimiters;

    FklGrammerNonterm start;

    size_t prodNum;
    FklProdHashMap productions;

    FklGraSidBuiltinHashMap builtins;

    FklFirstSetHashMap firstSets;

    FklGrammerIgnore *ignores;

    FklAnalysisTable aTable;
} FklGrammer;

typedef struct {
    FklSid_t old_group_id;
    FklSid_t new_group_id;
} FklRecomputeGroupIdArgs;

typedef struct {
    const char *name;
    FklProdActionFunc func;
} FklGrammerBuiltinAction;

typedef struct FklAnalysisSymbol {
    FklGrammerNonterm nt;
    void *ast;
} FklAnalysisSymbol;

typedef struct FklStateActionMatchArgs {
    size_t matchLen;
    size_t skip_ignore_len;
    ssize_t ignore_len;
} FklStateActionMatchArgs;

#define FKL_STATE_ACTION_MATCH_ARGS_INIT { .ignore_len = -2 }

int fklCheckAndInitGrammerSymbols(FklGrammer *g,
        FklGrammerNonterm *unresolved_nonterm);

int fklAddProdAndExtraToGrammer(FklGrammer *g, FklGrammerProduction *prod);
int fklAddProdToProdTable(FklGrammer *g, FklGrammerProduction *prod);

int fklAddProdToProdTableNoRepeat(FklGrammer *g, FklGrammerProduction *prod);

void fklUninitGrammer(FklGrammer *);
void fklDestroyGrammer(FklGrammer *);
void fklClearGrammer(FklGrammer *);

FklLalrItemSetHashMap *fklGenerateLr0Items(FklGrammer *grammer);

int fklIsStateActionMatch(const FklAnalysisStateActionMatch *match,
        const FklGrammer *g,
        FklGrammerMatchOuterCtx *outerCtx,
        const char *start,
        const char *cstr,
        size_t restLen,
        int *is_waiting_for_more,
        FklStateActionMatchArgs *args);

static inline void fklInitTerminalAnalysisSymbol(FklAnalysisSymbol *sym,
        const char *s,
        size_t len,
        FklGrammerMatchOuterCtx *outerCtx) {
    void *ast = outerCtx->create(s, len, outerCtx->line, outerCtx->ctx);
    sym->nt.group = 0;
    sym->nt.sid = 0;
    sym->ast = ast;
}

int fklGenerateLalrAnalyzeTable(FklGrammer *grammer,
        FklLalrItemSetHashMap *states,
        FklStringBuffer *error_msg);
void fklPrintAnalysisTable(const FklGrammer *grammer,
        const FklSymbolTable *st,
        FILE *fp);
void fklPrintAnalysisTableForGraphEasy(const FklGrammer *grammer,
        const FklSymbolTable *st,
        FILE *fp);
void fklPrintAnalysisTableAsCfunc(const FklGrammer *grammer,
        const FklSymbolTable *st,
        FILE *action_src_fp,
        const char *ast_creator_name,
        const char *ast_destroy_name,
        const char *state_0_push_func_name,
        FILE *fp);

void fklLr0ToLalrItems(FklLalrItemSetHashMap *, FklGrammer *grammer);

void fklPrintItemSet(const FklLalrItemHashSet *itemSet,
        const FklGrammer *g,
        const FklSymbolTable *st,
        FILE *fp);
void fklPrintItemStateSet(const FklLalrItemSetHashMap *i,
        const FklGrammer *g,
        const FklSymbolTable *st,
        FILE *fp);

int fklAddExtraProdToGrammer(FklGrammer *grammer);
void fklPrintItemStateSetAsDot(const FklLalrItemSetHashMap *i,
        const FklGrammer *g,
        const FklSymbolTable *st,
        FILE *fp);

FklGrammerProduction *fklCreateEmptyProduction(FklSid_t group,
        FklSid_t sid,
        size_t len,
        const char *name,
        FklProdActionFunc func,
        void *ctx,
        void (*destroy)(void *),
        void *(*copyer)(const void *));
FklGrammerIgnore *fklCreateEmptyGrammerIgnore(size_t len);

FklGrammerProduction *fklCreateProduction(FklSid_t group,
        FklSid_t sid,
        size_t len,
        FklGrammerSym *syms,
        const char *name,
        FklProdActionFunc func,
        void *ctx,
        void (*destroy)(void *),
        void *(*copyer)(const void *));

void fklProdCtxDestroyFree(void *c);
void fklProdCtxDestroyDoNothing(void *c);
void *fklProdCtxCopyerDoNothing(const void *c);

void fklUninitGrammerSymbols(FklGrammerSym *syms, size_t len);
FklGrammerIgnore *fklGrammerSymbolsToIgnore(FklGrammerSym *syms, size_t len);
const FklLalrBuiltinMatch *fklGetBuiltinMatch(const FklGraSidBuiltinHashMap *ht,
        FklSid_t id);

int fklIsNonterminalExist(FklProdHashMap *prods, FklSid_t group, FklSid_t id);

FklGrammerProduction *
fklGetProductions(const FklProdHashMap *prod, FklSid_t group, FklSid_t id);
FklGrammerProduction *
fklGetGrammerProductions(const FklGrammer *g, FklSid_t group, FklSid_t id);

void fklPrintGrammerIgnores(const FklGrammer *g,
        const FklRegexTable *rt,
        FILE *fp);
void fklPrintGrammerProduction(FILE *fp,
        const FklGrammerProduction *prod,
        const FklSymbolTable *st,
        const FklRegexTable *rt);
void fklPrintGrammer(FILE *fp, const FklGrammer *grammer, FklSymbolTable *st);

void *fklParseWithTableForCstr(const FklGrammer *,
        const char *str,
        FklGrammerMatchOuterCtx *,
        FklSymbolTable *st,
        int *err);

void *fklParseWithTableForCharBuf(const FklGrammer *,
        const char *str,
        size_t len,
        FklGrammerMatchOuterCtx *,
        FklSymbolTable *st,
        int *err);
struct FklParseStateVector;

// FklAnalysisSymbolVector
#define FKL_VECTOR_ELM_TYPE FklAnalysisSymbol
#define FKL_VECTOR_ELM_TYPE_NAME AnalysisSymbol
#include "cont/vector.h"

typedef union FklParseState FklParseState;

typedef int (*FklStateFuncPtr)(struct FklParseStateVector *,
        FklAnalysisSymbolVector *,
        FklUintVector *,
        int,
        FklSid_t,
        FklParseState *func,
        const char *,
        const char **,
        size_t *,
        FklGrammerMatchOuterCtx *,
        int *,
        size_t *errLine);

typedef union FklParseState {
    FklStateFuncPtr func;
    FklAnalysisState const *state;
} FklParseState;

// FklParseStateVector
#define FKL_VECTOR_ELM_TYPE FklParseState
#define FKL_VECTOR_ELM_TYPE_NAME ParseState
#include "cont/vector.h"

void *fklParseWithTableForCharBuf2(const FklGrammer *,
        const char *str,
        size_t len,
        size_t *restLen,
        FklGrammerMatchOuterCtx *,
        FklSymbolTable *st,
        int *err,
        size_t *errLine,
        struct FklAnalysisSymbolVector *symbols,
        FklUintVector *lines,
        FklParseStateVector *states);

#define FKL_PARSE_TERMINAL_MATCH_FAILED (1)
#define FKL_PARSE_REDUCE_FAILED (2)
#define FKL_PARSE_WAITING_FOR_MORE (3)

void *fklDefaultParseForCstr(const char *str,
        FklGrammerMatchOuterCtx *,
        int *err,
        size_t *errLine,
        FklAnalysisSymbolVector *symbols,
        FklUintVector *lines,
        FklParseStateVector *states);

void *fklDefaultParseForCharBuf(const char *str,
        size_t len,
        size_t *restLen,
        FklGrammerMatchOuterCtx *,
        int *err,
        size_t *errLine,
        FklAnalysisSymbolVector *symbols,
        FklUintVector *lines,
        FklParseStateVector *states);

FklGrammerIgnore *fklInitBuiltinProductionSet(FklGrammer *g);

void fklInitBuiltinGrammerSymTable(FklGraSidBuiltinHashMap *s,
        FklSymbolTable *st);

const char *fklBuiltinTerminalInitErrorToCstr(FklBuiltinTerminalInitError err);

int fklMergeGrammer(FklGrammer *g,
        const FklGrammer *other,
        const FklRecomputeGroupIdArgs *);

void fklInitEmptyGrammer(FklGrammer *g, FklSymbolTable *st);
FklGrammer *fklCreateEmptyGrammer(FklSymbolTable *st);

int fklIsGrammerInited(const FklGrammer *g);

void fklInitBuiltinGrammer(FklGrammer *g, FklSymbolTable *st);
FklGrammer *fklCreateBuiltinGrammer(FklSymbolTable *st);

int fklAddIgnoreToIgnoreList(FklGrammerIgnore **pp, FklGrammerIgnore *ig);
void fklDestroyIgnore(FklGrammerIgnore *ig);

#ifdef __cplusplus
}
#endif

#endif
