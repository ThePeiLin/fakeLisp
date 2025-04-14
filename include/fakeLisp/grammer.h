#ifndef FKL_GRAMMER_H
#define FKL_GRAMMER_H

#include "base.h"
#include "regex.h"
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

typedef struct {
    int (*match)(void *ctx, const char *start, const char *str, size_t restLen,
                 size_t *matchLen, FklGrammerMatchOuterCtx *outerCtx,
                 int *is_waiting_for_more);
    int (*ctx_cmp)(const void *c0, const void *c1);
    int (*ctx_equal)(const void *c0, const void *c1);
    uintptr_t (*ctx_hash)(const void *c);
    void *(*ctx_create)(const FklString *next, int *failed);
    void *(*ctx_global_create)(size_t, struct FklGrammerProduction *prod,
                               struct FklGrammer *g, int *failed);
    void (*ctx_destroy)(void *);
    const char *name;
    void (*print_src)(const struct FklGrammer *g, FILE *fp);
    void (*print_c_match_cond)(void *ctx, const struct FklGrammer *g, FILE *fp);
} FklLalrBuiltinMatch;

typedef struct {
    const FklLalrBuiltinMatch *t;
    void *c;
} FklLalrBuiltinGrammerSym;

typedef struct {
    FklSid_t group;
    FklSid_t sid;
} FklGrammerNonterm;

// FklNontermTable
#define FKL_TABLE_KEY_TYPE FklGrammerNonterm
#define FKL_TABLE_KEY_EQUAL(A, B)                                              \
    (A)->group == (B)->group && (A)->sid == (B)->sid
#define FKL_TABLE_KEY_HASH                                                     \
    return fklHashCombine(fklHash32Shift(pk->group), pk->sid)
#define FKL_TABLE_ELM_NAME Nonterm
#include "table.h"

typedef enum {
    FKL_LALR_MATCH_NONE,
    FKL_LALR_MATCH_EOF,
    FKL_LALR_MATCH_STRING,
    FKL_LALR_MATCH_BUILTIN,
    FKL_LALR_MATCH_STRING_END_WITH_TERMINAL,
    FKL_LALR_MATCH_REGEX,
} FklLalrMatchType;

#define FKL_LALR_MATCH_NONE_INIT                                               \
    ((FklLalrItemLookAhead){                                                   \
        .t = FKL_LALR_MATCH_NONE,                                              \
    })
#define FKL_LALR_MATCH_EOF_INIT                                                \
    ((FklLalrItemLookAhead){                                                   \
        .t = FKL_LALR_MATCH_EOF,                                               \
    })

typedef struct {
    FklLalrMatchType t;
    unsigned int delim : 1;
    unsigned int end_with_terminal : 1;
    union {
        const FklString *s;
        FklLalrBuiltinGrammerSym b;
        const FklRegexCode *re;
    };
} FklLalrItemLookAhead;

static inline int
fklBuiltinGrammerSymEqual(const FklLalrBuiltinGrammerSym *s0,
                          const FklLalrBuiltinGrammerSym *s1) {
    if (s0->t == s1->t) {
        if (s0->t->ctx_equal)
            return s0->t->ctx_equal(s0->c, s1->c);
        return 1;
    }
    return 0;
}

static inline int fklLalrItemLookAheadEqual(const FklLalrItemLookAhead *la0,
                                            const FklLalrItemLookAhead *la1) {
    if (la0->t != la1->t || la0->delim != la1->delim
        || la0->end_with_terminal != la1->end_with_terminal)
        return 0;
    switch (la0->t) {
    case FKL_LALR_MATCH_NONE:
    case FKL_LALR_MATCH_EOF:
        return 1;
        break;
    case FKL_LALR_MATCH_STRING:
    case FKL_LALR_MATCH_STRING_END_WITH_TERMINAL:
        return la0->s == la1->s;
        break;
    case FKL_LALR_MATCH_REGEX:
        return la0->re == la1->re;
        break;
    case FKL_LALR_MATCH_BUILTIN:
        return fklBuiltinGrammerSymEqual(&la0->b, &la1->b);
        break;
    }
    return 0;
}

static uintptr_t fklBuiltinGrammerSymHash(const FklLalrBuiltinGrammerSym *s) {
    if (s->t->ctx_hash)
        return s->t->ctx_hash(s->c);
    return 0;
}

static uintptr_t fklLalrItemLookAheadHash(const FklLalrItemLookAhead *pk) {
    uintptr_t rest = pk->t == FKL_LALR_MATCH_STRING
                       ? FKL_TYPE_CAST(uintptr_t, pk->s)
                       : (pk->t == FKL_LALR_MATCH_BUILTIN
                              ? fklBuiltinGrammerSymHash(&pk->b)
                              : 0);
    rest = fklHashCombine(rest, FKL_TYPE_CAST(uintptr_t, pk->t));
    rest = fklHashCombine(rest, pk->delim);
    return fklHashCombine(rest, pk->end_with_terminal);
}

// FklLookAheadTable
#define FKL_TABLE_KEY_TYPE FklLalrItemLookAhead
#define FKL_TABLE_KEY_EQUAL(A, B) fklLalrItemLookAheadEqual(A, B)
#define FKL_TABLE_KEY_HASH return fklLalrItemLookAheadHash(pk)
#define FKL_TABLE_ELM_NAME LookAhead
#include "table.h"

typedef struct {
    FklLookAheadTable first;
    int hasEpsilon;
} FklFirstSetItem;

static inline int fklNontermEqual(const FklGrammerNonterm *nt0,
                                  const FklGrammerNonterm *nt1) {
    return nt0->group == nt1->group && nt0->sid == nt1->sid;
}

static inline uintptr_t fklNontermHash(const FklGrammerNonterm *pk) {
    return fklHashCombine(fklHash32Shift(pk->group), pk->sid);
}

// FklFirstSetTable
#define FKL_TABLE_KEY_TYPE FklGrammerNonterm
#define FKL_TABLE_KEY_EQUAL(A, B) fklNontermEqual(A, B)
#define FKL_TABLE_KEY_HASH return fklNontermHash(pk)
#define FKL_TABLE_VAL_TYPE FklFirstSetItem
#define FKL_TABLE_VAL_UNINIT(V) fklLookAheadTableUninit(&(V)->first)
#define FKL_TABLE_ELM_NAME FirstSet
#include "table.h"

enum FklGrammerTermType {
    FKL_TERM_STRING = 0,
    FKL_TERM_BUILTIN,
    FKL_TERM_REGEX,
};

typedef struct FklGrammerSym {
    uint16_t delim;
    uint16_t isterm;
    uint16_t term_type;
    uint16_t end_with_terminal;
    union {
        FklGrammerNonterm nt;
        FklLalrBuiltinGrammerSym b;
        const FklRegexCode *re;
    };
} FklGrammerSym;

struct FklGrammerProduction;

uint64_t fklGetFirstNthLine(FklUintVector *lineStack, size_t num, size_t line);

typedef void *(*FklProdActionFunc)(void *ctx, void *outerCtx, void *asts[],
                                   size_t num, size_t line);

typedef struct FklGrammerProduction {
    FklGrammerNonterm left;
    size_t len;
    size_t idx;
    FklGrammerSym *syms;
    struct FklGrammerProduction *next;
    const char *name;
    void *ctx;
    FklProdActionFunc func;
    void *(*ctx_copyer)(const void *);
    void (*ctx_destroyer)(void *);
} FklGrammerProduction;

void fklDestroyGrammerProduction(FklGrammerProduction *h);

// FklProdTable
#define FKL_TABLE_KEY_TYPE FklGrammerNonterm
#define FKL_TABLE_VAL_TYPE FklGrammerProduction *
#define FKL_TABLE_KEY_EQUAL(A, B)                                              \
    (A)->group == (B)->group && (A)->sid == (B)->sid
#define FKL_TABLE_KEY_HASH                                                     \
    return fklHashCombine(fklHash32Shift(pk->group), pk->sid)
#define FKL_TABLE_ELM_NAME Prod
#define FKL_TABLE_VAL_UNINIT(V)                                                \
    {                                                                          \
        FklGrammerProduction *h = *(V);                                        \
        while (h) {                                                            \
            FklGrammerProduction *next = h->next;                              \
            fklDestroyGrammerProduction(h);                                    \
            h = next;                                                          \
        }                                                                      \
        *(V) = NULL;                                                           \
    }
#include "table.h"

// FklGraSidBuiltinTable
#define FKL_TABLE_KEY_TYPE FklSid_t
#define FKL_TABLE_VAL_TYPE FklLalrBuiltinMatch const *
#define FKL_TABLE_ELM_NAME GraSidBuiltin
#include "table.h"

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

// FklLalrItemTable
#define FKL_TABLE_KEY_TYPE FklLalrItem
#define FKL_TABLE_KEY_EQUAL(A, B) fklLalrItemEqual((A), (B))
#define FKL_TABLE_KEY_HASH return fklLalrItemHash(pk)
#define FKL_TABLE_ELM_NAME LalrItem
#include <fakeLisp/table.h>

FklGrammerProduction *
fklCopyUninitedGrammerProduction(FklGrammerProduction *prod);

typedef struct FklLalrItemSetLink {
    FklGrammerSym sym;
    struct FklLalrItemSet *dst;
    struct FklLalrItemSetLink *next;
} FklLalrItemSetLink;

typedef struct FklLookAheadSpreads {
    FklLalrItem src;
    FklLalrItemTable *dstItems;
    FklLalrItem dst;
    struct FklLookAheadSpreads *next;
} FklLookAheadSpreads;

typedef struct FklLalrItemSet {
    FklLalrItemTable items;
    FklLookAheadSpreads *spreads;
    FklLalrItemSetLink *links;
} FklLalrItemSet;

typedef struct FklGetLaFirstSetCacheKey {
    const FklGrammerProduction *prod;
    uint32_t idx;
} FklGetLaFirstSetCacheKey;

typedef struct FklGetLaFirstSetCacheItem {
    FklGetLaFirstSetCacheKey key;
    FklLookAheadTable first;
    int has_epsilon;
} FklGetLaFirstSetCacheItem;

typedef enum {
    FKL_ANALYSIS_SHIFT,
    FKL_ANALYSIS_ACCEPT,
    FKL_ANALYSIS_REDUCE,
    FKL_ANALYSIS_IGNORE,
} FklAnalysisStateActionEnum;

typedef struct FklAnalysisStateGoto {
    FklGrammerNonterm nt;
    struct FklAnalysisState *state;
    struct FklAnalysisStateGoto *next;
} FklAnalysisStateGoto;

typedef struct {
    FklLalrMatchType t;
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
        const FklGrammerProduction *prod;
    };

    struct FklAnalysisStateAction *next;
} FklAnalysisStateAction;

typedef struct FklAnalysisState {
    unsigned int builtin : 1;
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
    uint32_t term_type;
} FklGrammerIgnoreSym;

typedef struct FklGrammerIgnore {
    struct FklGrammerIgnore *next;
    size_t len;
    FklGrammerIgnoreSym ig[];
} FklGrammerIgnore;

typedef struct FklGrammer {
    FklSymbolTable terminals;

    FklSymbolTable reachable_terminals;

    FklRegexTable regexes;

    size_t sortedTerminalsNum;
    const FklString **sortedTerminals;

    FklGrammerNonterm start;

    size_t prodNum;
    FklProdTable productions;

    FklGraSidBuiltinTable builtins;

    FklFirstSetTable firstSets;

    FklGrammerIgnore *ignores;

    FklAnalysisTable aTable;
} FklGrammer;

typedef struct {
    const char *cstr;
    const char *action_name;
    FklProdActionFunc func;
} FklGrammerCstrAction;

typedef struct FklAnalysisSymbol {
    FklGrammerNonterm nt;
    void *ast;
} FklAnalysisSymbol;

int fklCheckAndInitGrammerSymbols(FklGrammer *g);

FklGrammer *fklCreateGrammerFromCstr(const char *str[], FklSymbolTable *st);
FklGrammer *
fklCreateGrammerFromCstrAction(const FklGrammerCstrAction prodAction[],
                               FklSymbolTable *st);

int fklAddProdAndExtraToGrammer(FklGrammer *g, FklGrammerProduction *prod);
int fklAddProdToProdTable(FklProdTable *productions,
                          FklGraSidBuiltinTable *builtins,
                          FklGrammerProduction *prod);

int fklAddProdToProdTableNoRepeat(FklProdTable *productions,
                                  FklGraSidBuiltinTable *builtins,
                                  FklGrammerProduction *prod);

void fklUninitGrammer(FklGrammer *);
void fklDestroyGrammer(FklGrammer *);
void fklClearGrammer(FklGrammer *);

FklHashTable *fklGenerateLr0Items(FklGrammer *grammer);

int fklIsStateActionMatch(const FklAnalysisStateActionMatch *match,
                          const char *start, const char *cstr, size_t restLen,
                          size_t *matchLen, FklGrammerMatchOuterCtx *outerCtx,
                          int *is_waiting_for_more, const FklGrammer *g);

static inline void
fklInitTerminalAnalysisSymbol(FklAnalysisSymbol *sym, const char *s, size_t len,
                              FklGrammerMatchOuterCtx *outerCtx) {
    void *ast = outerCtx->create(s, len, outerCtx->line, outerCtx->ctx);
    sym->nt.group = 0;
    sym->nt.sid = 0;
    sym->ast = ast;
}

int fklGenerateLalrAnalyzeTable(FklGrammer *grammer, FklHashTable *states);
void fklPrintAnalysisTable(const FklGrammer *grammer, const FklSymbolTable *st,
                           FILE *fp);
void fklPrintAnalysisTableForGraphEasy(const FklGrammer *grammer,
                                       const FklSymbolTable *st, FILE *fp);
void fklPrintAnalysisTableAsCfunc(const FklGrammer *grammer,
                                  const FklSymbolTable *st, FILE *action_src_fp,
                                  const char *ast_creator_name,
                                  const char *ast_destroy_name,
                                  const char *state_0_push_func_name, FILE *fp);

void fklLr0ToLalrItems(FklHashTable *, FklGrammer *grammer);

void fklPrintItemSet(const FklLalrItemTable *itemSet, const FklGrammer *g,
                     const FklSymbolTable *st, FILE *fp);
void fklPrintItemStateSet(const FklHashTable *i, const FklGrammer *g,
                          const FklSymbolTable *st, FILE *fp);

int fklAddExtraProdToGrammer(FklGrammer *grammer);
void fklPrintItemStateSetAsDot(const FklHashTable *i, const FklGrammer *g,
                               const FklSymbolTable *st, FILE *fp);

FklGrammerProduction *fklCreateEmptyProduction(FklSid_t group, FklSid_t sid,
                                               size_t len, const char *name,
                                               FklProdActionFunc func,
                                               void *ctx,
                                               void (*destroy)(void *),
                                               void *(*copyer)(const void *));

FklGrammerProduction *fklCreateProduction(FklSid_t group, FklSid_t sid,
                                          size_t len, FklGrammerSym *syms,
                                          const char *name,
                                          FklProdActionFunc func, void *ctx,
                                          void (*destroy)(void *),
                                          void *(*copyer)(const void *));

void fklProdCtxDestroyFree(void *c);
void fklProdCtxDestroyDoNothing(void *c);
void *fklProdCtxCopyerDoNothing(const void *c);

void fklUninitGrammerSymbols(FklGrammerSym *syms, size_t len);
FklGrammerIgnore *fklGrammerSymbolsToIgnore(FklGrammerSym *syms, size_t len,
                                            const FklSymbolTable *tt);
const FklLalrBuiltinMatch *fklGetBuiltinMatch(const FklGraSidBuiltinTable *ht,
                                              FklSid_t id);

int fklIsNonterminalExist(FklProdTable *prods, FklSid_t group, FklSid_t id);

FklGrammerProduction *fklGetProductions(const FklProdTable *prod,
                                        FklSid_t group, FklSid_t id);
FklGrammerProduction *fklGetGrammerProductions(const FklGrammer *g,
                                               FklSid_t group, FklSid_t id);

void fklPrintGrammerProduction(FILE *fp, const FklGrammerProduction *prod,
                               const FklSymbolTable *st,
                               const FklSymbolTable *tt,
                               const FklRegexTable *rt);
void fklPrintGrammer(FILE *fp, const FklGrammer *grammer, FklSymbolTable *st);

void *fklParseWithTableForCstr(const FklGrammer *, const char *str,
                               FklGrammerMatchOuterCtx *, FklSymbolTable *st,
                               int *err);

void *fklParseWithTableForCstrDbg(const FklGrammer *, const char *str,
                                  FklGrammerMatchOuterCtx *, FklSymbolTable *st,
                                  int *err);

struct FklParseStateVector;

// FklAnalysisSymbolVector
#define FKL_VECTOR_ELM_TYPE FklAnalysisSymbol
#define FKL_VECTOR_ELM_TYPE_NAME AnalysisSymbol
#include "vector.h"

typedef int (*FklStateFuncPtr)(struct FklParseStateVector *,
                               FklAnalysisSymbolVector *, FklUintVector *, int,
                               FklSid_t, void **, const char *, const char **,
                               size_t *, FklGrammerMatchOuterCtx *, int *,
                               size_t *errLine);

typedef union FklParseState {
    FklStateFuncPtr func;
    FklAnalysisState const *state;
} FklParseState;

// FklParseStateVector
#define FKL_VECTOR_ELM_TYPE FklParseState
#define FKL_VECTOR_ELM_TYPE_NAME ParseState
#include "vector.h"

void *fklParseWithTableForCharBuf(const FklGrammer *, const char *str,
                                  size_t len, size_t *restLen,
                                  FklGrammerMatchOuterCtx *, FklSymbolTable *st,
                                  int *err, size_t *errLine,
                                  struct FklAnalysisSymbolVector *symbols,
                                  FklUintVector *lines,
                                  FklParseStateVector *states);

#define FKL_PARSE_TERMINAL_MATCH_FAILED (1)
#define FKL_PARSE_REDUCE_FAILED (2)
#define FKL_PARSE_WAITING_FOR_MORE (3)

void *fklDefaultParseForCstr(const char *str, FklGrammerMatchOuterCtx *,
                             int *err, size_t *errLine,
                             FklAnalysisSymbolVector *symbols,
                             FklUintVector *lines, FklParseStateVector *states);

void *fklDefaultParseForCharBuf(const char *str, size_t len, size_t *restLen,
                                FklGrammerMatchOuterCtx *, int *err,
                                size_t *errLine,
                                FklAnalysisSymbolVector *symbols,
                                FklUintVector *lines,
                                FklParseStateVector *states);

FklGrammerIgnore *fklInitBuiltinProductionSet(FklProdTable *ht,
                                              FklSymbolTable *st,
                                              FklSymbolTable *tt,
                                              FklRegexTable *rt,
                                              FklGraSidBuiltinTable *builtins);

void fklInitBuiltinGrammerSymTable(FklGraSidBuiltinTable *s,
                                   FklSymbolTable *st);
void fklInitEmptyGrammer(FklGrammer *g, FklSymbolTable *st);
FklGrammer *fklCreateEmptyGrammer(FklSymbolTable *st);
FklGrammer *fklCreateBuiltinGrammer(FklSymbolTable *st);

int fklAddIgnoreToIgnoreList(FklGrammerIgnore **pp, FklGrammerIgnore *ig);
void fklDestroyIgnore(FklGrammerIgnore *ig);

size_t fklQuotedStringMatch(const char *cstr, size_t restLen,
                            const FklString *end);
size_t fklQuotedCharBufMatch(const char *cstr, size_t restLen, const char *end,
                             size_t end_size);

#ifdef __cplusplus
}
#endif

#endif
