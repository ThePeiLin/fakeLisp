#ifndef FKL_PARSER_H
#define FKL_PARSER_H

#include "grammer.h"
#include "vm_fwd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklVMvalueLnt FklVMvalueLnt;

typedef struct {
    FklVM *exe;
    FklVMvalueLnt *ln;
    void *opaque;
} FklVMparseCtx;

#define FKL_VMVALUE_PARSE_CTX_INIT(EXE, LN)                                    \
    (FklGrammerMatchCtx) {                                                     \
        .maxNonterminalLen = 0, .line = 1, .start = NULL, .cur = NULL,         \
        .create = fklVMvalueTerminalCreate,                                    \
        .destroy = fklVMvalueTerminalDestroy,                                  \
        .ctx = (void *)&(FklVMparseCtx){                                       \
            .exe = (EXE),                                                      \
            .ln = (LN),                                                        \
            .opaque = NULL,                                                    \
        },                                                                     \
    }

#define FKL_VMVALUE_PARSE_CTX_INIT1(EXE, LN, OPA)                              \
    (FklGrammerMatchCtx) {                                                     \
        .maxNonterminalLen = 0, .line = 1, .start = NULL, .cur = NULL,         \
        .create = fklVMvalueTerminalCreate,                                    \
        .destroy = fklVMvalueTerminalDestroy,                                  \
        .ctx = (void *)&(FklVMparseCtx){                                       \
            .exe = (EXE),                                                      \
            .ln = (LN),                                                        \
            .opaque = (OPA),                                                   \
        },                                                                     \
    }

FklVMvalue *fklCreateNastNodeFromCstr(FklVM *, const char *, FklVMvalueLnt *ln);

int fklIsVMvalueLnt(const FklVMvalue *v);
FklVMvalueLnt *fklCreateVMvalueLnt(FklVM *v);
void fklVMvalueLntPut(FklVMvalueLnt *ht, const FklVMvalue *v, uint64_t line);
uint64_t *fklVMvalueLntGet(FklVMvalueLnt *ht, const FklVMvalue *v);

typedef struct {
    // in
    size_t const line;
    FklAnalysisSymbolVector *const symbols;
    FklParseStateVector *const states;

    FklVM *const vm;
    FklVMvalueLnt *const ln;
    void *const opa;

    // out
    size_t output_size;
    int unexpect_eof;
    size_t output_line;
    FklVMvalue *output;
} FklReadArgs;

char *fklReadWithBuiltinParser(FILE *fp, FklReadArgs *args);

char *fklReadWithAnalysisTable(const FklGrammer *g, //
        FILE *fp,
        FklReadArgs *args);

void *fklParseWithTableForCstr(const FklGrammer *,
        const char *str,
        FklGrammerMatchCtx *,
        FklParseError *err);

void *fklParseWithTableForCharBuf(const FklGrammer *,
        const char *str,
        size_t len,
        FklGrammerMatchCtx *,
        FklParseError *err);

void *fklParseWithTableForCharBuf2(const FklGrammer *,
        const char *str,
        size_t len,
        size_t *restLen,
        FklGrammerMatchCtx *,
        FklParseError *err,
        size_t *output_line,
        struct FklAnalysisSymbolVector *symbols,
        FklParseStateVector *states);

void *fklDefaultParseForCstr(const char *str,
        FklGrammerMatchCtx *,
        FklParseError *err,
        size_t *output_line,
        FklAnalysisSymbolVector *symbols,
        FklParseStateVector *states);

void *fklDefaultParseForCharBuf(const char *str,
        size_t len,
        size_t *restLen,
        FklGrammerMatchCtx *,
        FklParseError *err,
        size_t *output_line,
        FklAnalysisSymbolVector *symbols,
        FklParseStateVector *states);

#ifdef __cplusplus
}
#endif
#endif
