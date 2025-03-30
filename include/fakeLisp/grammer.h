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

typedef struct {
    FklGrammerNonterm left;
    FklGrammerProduction *prods;
} FklGrammerProdHashItem;

FklGrammerProduction *
fklCopyUninitedGrammerProduction(FklGrammerProduction *prod);
void fklInitGrammerProductionTable(FklHashTable *);
FklHashTable *fklCreateGrammerProductionTable(void);

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

typedef struct {
    FklGrammerProduction *prod;
    uint32_t idx;
    FklLalrItemLookAhead la;
} FklLalrItem;

typedef struct FklLalrItemSetLink {
    FklGrammerSym sym;
    struct FklLalrItemSet *dst;
    struct FklLalrItemSetLink *next;
} FklLalrItemSetLink;

typedef struct FklLookAheadSpreads {
    FklLalrItem src;
    FklHashTable *dstItems;
    FklLalrItem dst;
    struct FklLookAheadSpreads *next;
} FklLookAheadSpreads;

typedef struct FklLalrItemSet {
    FklHashTable items;
    FklLookAheadSpreads *spreads;
    FklLalrItemSetLink *links;
} FklLalrItemSet;

typedef struct FklGetLaFirstSetCacheKey {
    const FklGrammerProduction *prod;
    uint32_t idx;
} FklGetLaFirstSetCacheKey;

typedef struct FklGetLaFirstSetCacheItem {
    FklGetLaFirstSetCacheKey key;
    FklHashTable first;
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
    FklHashTable productions;

    FklHashTable builtins;

    FklHashTable firstSets;

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
void fklDestroyAnaysisSymbol(FklAnalysisSymbol *);
FklHashTable *fklCreateTerminalIndexSet(FklString *const *terminals,
                                        size_t num);

FklGrammer *fklCreateGrammerFromCstr(const char *str[], FklSymbolTable *st);
FklGrammer *
fklCreateGrammerFromCstrAction(const FklGrammerCstrAction prodAction[],
                               FklSymbolTable *st);

int fklAddProdAndExtraToGrammer(FklGrammer *g, FklGrammerProduction *prod);
int fklAddProdToProdTable(FklHashTable *productions, FklHashTable *builtins,
                          FklGrammerProduction *prod);

int fklAddProdToProdTableNoRepeat(FklHashTable *productions,
                                  FklHashTable *builtins,
                                  FklGrammerProduction *prod);

void fklUninitGrammer(FklGrammer *);
void fklDestroyGrammer(FklGrammer *);
void fklClearGrammer(FklGrammer *);

FklHashTable *fklGenerateLr0Items(FklGrammer *grammer);

int fklIsStateActionMatch(const FklAnalysisStateActionMatch *match,
                          const char *start, const char *cstr, size_t restLen,
                          size_t *matchLen, FklGrammerMatchOuterCtx *outerCtx,
                          int *is_waiting_for_more, const FklGrammer *g);

FklAnalysisSymbol *
fklCreateTerminalAnalysisSymbol(const char *s, size_t len,
                                FklGrammerMatchOuterCtx *outerCtx);
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

void fklPrintItemSet(const FklHashTable *itemSet, const FklGrammer *g,
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

void fklDestroyGrammerProduction(FklGrammerProduction *h);
void fklUninitGrammerSymbols(FklGrammerSym *syms, size_t len);
FklGrammerIgnore *fklGrammerSymbolsToIgnore(FklGrammerSym *syms, size_t len,
                                            const FklSymbolTable *tt);
const FklLalrBuiltinMatch *fklGetBuiltinMatch(const FklHashTable *ht,
                                              FklSid_t id);

int fklIsNonterminalExist(FklHashTable *prods, FklSid_t group, FklSid_t id);

FklGrammerProduction *fklGetProductions(const FklHashTable *prod,
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

void *fklParseWithTableForCharBuf(const FklGrammer *, const char *str,
                                  size_t len, size_t *restLen,
                                  FklGrammerMatchOuterCtx *, FklSymbolTable *st,
                                  int *err, size_t *errLine,
                                  FklPtrStack *symbols, FklUintVector *lines,
                                  FklPtrStack *states);

#define FKL_PARSE_TERMINAL_MATCH_FAILED (1)
#define FKL_PARSE_REDUCE_FAILED (2)
#define FKL_PARSE_WAITING_FOR_MORE (3)

typedef int (*FklStateFuncPtr)(FklPtrStack *, FklPtrStack *, FklUintVector *,
                               int, FklSid_t, void **, const char *,
                               const char **, size_t *,
                               FklGrammerMatchOuterCtx *, int *,
                               size_t *errLine);

void *fklDefaultParseForCstr(const char *str, FklGrammerMatchOuterCtx *,
                             int *err, size_t *errLine, FklPtrStack *symbols,
                             FklUintVector *lines, FklPtrStack *states);

void *fklDefaultParseForCharBuf(const char *str, size_t len, size_t *restLen,
                                FklGrammerMatchOuterCtx *, int *err,
                                size_t *errLine, FklPtrStack *symbols,
                                FklUintVector *lines, FklPtrStack *states);

FklGrammerIgnore *fklInitBuiltinProductionSet(FklHashTable *ht,
                                              FklSymbolTable *st,
                                              FklSymbolTable *tt,
                                              FklRegexTable *rt,
                                              FklHashTable *builtins);

void fklInitBuiltinGrammerSymTable(FklHashTable *s, FklSymbolTable *st);
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
