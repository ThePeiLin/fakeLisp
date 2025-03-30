#ifndef FKL_PARSER_H
#define FKL_PARSER_H

#include "grammer.h"
#include "nast.h"
#include "symbol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FKL_NAST_PARSE_OUTER_CTX_INIT(CTX)                                     \
    {.maxNonterminalLen = 0,                                                   \
     .line = 1,                                                                \
     .start = NULL,                                                            \
     .cur = NULL,                                                              \
     .create = fklNastTerminalCreate,                                          \
     .destroy = (void (*)(void *))fklDestroyNastNode,                          \
     .ctx = (CTX)}

FklNastNode *fklCreateNastNodeFromCstr(const char *,
                                       FklSymbolTable *publicSymbolTable);

void fklNastPushState0ToStack(FklPtrStack *stateStack);

void *fklNastTerminalCreate(const char *s, size_t len, size_t line, void *ctx);

char *fklReadWithBuiltinParser(FILE *fp, size_t *psize, size_t line,
                               size_t *pline, FklSymbolTable *st,
                               int *unexpectEOF, size_t *errLine,
                               FklNastNode **output, FklPtrStack *symbolStack,
                               FklUintStack *lineStack,
                               FklPtrStack *stateStack);

char *fklReadWithAnalysisTable(const FklGrammer *g, FILE *fp, size_t *psize,
                               size_t line, size_t *pline, FklSymbolTable *st,
                               int *unexpectEOF, size_t *errLine,
                               FklNastNode **output, FklPtrStack *symbolStack,
                               FklUintStack *lineStack,
                               FklPtrStack *stateStack);

#ifdef __cplusplus
}
#endif
#endif
