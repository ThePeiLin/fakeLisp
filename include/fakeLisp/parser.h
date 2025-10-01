#ifndef FKL_PARSER_H
#define FKL_PARSER_H

#include "grammer.h"
#include "symbol.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct FklVMvalue *
fklCreateNastNodeFromCstr(FklVM *, const char *, FklLineNumHashMap *ln);

char *fklReadWithBuiltinParser(FILE *fp,
        size_t *psize,
        size_t line,
        size_t *pline,
        FklVM *st,
        int *unexpectEOF,
        size_t *errLine,
        struct FklVMvalue **output,
        FklAnalysisSymbolVector *symbolStack,
        FklUintVector *lineStack,
        FklParseStateVector *stateStack,
        FklLineNumHashMap *ln);

char *fklReadWithAnalysisTable(const FklGrammer *g,
        FILE *fp,
        size_t *psize,
        size_t line,
        size_t *pline,
        FklVM *st,
        int *unexpectEOF,
        size_t *errLine,
        struct FklVMvalue **output,
        FklAnalysisSymbolVector *symbolStack,
        FklUintVector *lineStack,
        FklParseStateVector *stateStack,
        FklLineNumHashMap *ln);

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
        size_t *errLine,
        struct FklAnalysisSymbolVector *symbols,
        FklUintVector *lines,
        FklParseStateVector *states);

void *fklDefaultParseForCstr(const char *str,
        FklGrammerMatchCtx *,
        FklParseError *err,
        size_t *errLine,
        FklAnalysisSymbolVector *symbols,
        FklUintVector *lines,
        FklParseStateVector *states);

void *fklDefaultParseForCharBuf(const char *str,
        size_t len,
        size_t *restLen,
        FklGrammerMatchCtx *,
        FklParseError *err,
        size_t *errLine,
        FklAnalysisSymbolVector *symbols,
        FklUintVector *lines,
        FklParseStateVector *states);

#ifdef __cplusplus
}
#endif
#endif
