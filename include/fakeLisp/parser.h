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
} FklVMparseCtx;

#define FKL_VMVALUE_PARSE_CTX_INIT(EXE, LN)                                    \
    {                                                                          \
        .maxNonterminalLen = 0,                                                \
        .line = 1,                                                             \
        .start = NULL,                                                         \
        .cur = NULL,                                                           \
        .create = fklVMvalueTerminalCreate,                                    \
        .destroy = fklVMvalueTerminalDestroy,                                  \
        .ctx = (void *)&(FklVMparseCtx){ .exe = (EXE), .ln = (LN) },           \
    }

FklVMvalue *fklCreateNastNodeFromCstr(FklVM *, const char *, FklVMvalueLnt *ln);

int fklIsVMvalueLnt(const FklVMvalue *v);
FklVMvalueLnt *fklCreateVMvalueLnt(FklVM *v);
void fklVMvalueLntPut(FklVMvalueLnt *ht, const FklVMvalue *v, uint64_t line);
uint64_t *fklVMvalueLntGet(FklVMvalueLnt *ht, const FklVMvalue *v);

char *fklReadWithBuiltinParser(FILE *fp,
        size_t *psize,
        size_t line,
        size_t *pline,
        FklVM *st,
        int *unexpectEOF,
        size_t *output_line,
        FklVMvalue **output,
        FklAnalysisSymbolVector *symbolStack,
        FklParseStateVector *stateStack,
        FklVMvalueLnt *ln);

char *fklReadWithAnalysisTable(const FklGrammer *g,
        FILE *fp,
        size_t *psize,
        size_t line,
        size_t *pline,
        FklVM *st,
        int *unexpectEOF,
        size_t *output_line,
        FklVMvalue **output,
        FklAnalysisSymbolVector *symbolStack,
        FklParseStateVector *stateStack,
        FklVMvalueLnt *ln);

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
