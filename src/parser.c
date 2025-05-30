#include <fakeLisp/codegen.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <string.h>

FklNastNode *fklCreateNastNodeFromCstr(const char *cStr,
                                       FklSymbolTable *publicSymbolTable) {
    FklParseStateVector stateStack;
    FklAnalysisSymbolVector symbolStack;
    FklUintVector lineStack;
    fklParseStateVectorInit(&stateStack, 16);
    fklAnalysisSymbolVectorInit(&symbolStack, 16);
    fklUintVectorInit(&lineStack, 16);
    fklNastPushState0ToStack(&stateStack);

    int err = 0;
    size_t errLine = 0;
    FklGrammerMatchOuterCtx outerCtx =
        FKL_NAST_PARSE_OUTER_CTX_INIT(publicSymbolTable);
    FklNastNode *node = fklDefaultParseForCstr(
        cStr, &outerCtx, &err, &errLine, &symbolStack, &lineStack, &stateStack);

    while (!fklAnalysisSymbolVectorIsEmpty(&symbolStack))
        fklDestroyNastNode(fklAnalysisSymbolVectorPopBack(&symbolStack)->ast);
    fklAnalysisSymbolVectorUninit(&symbolStack);
    fklParseStateVectorUninit(&stateStack);
    fklUintVectorUninit(&lineStack);

    return node;
}

char *fklReadWithBuiltinParser(FILE *fp, size_t *psize, size_t line,
                               size_t *pline, FklSymbolTable *st,
                               int *unexpectEOF, size_t *errLine,
                               FklNastNode **output,
                               FklAnalysisSymbolVector *symbolStack,
                               FklUintVector *lineStack,
                               FklParseStateVector *stateStack) {
    size_t size = 0;
    FklStringBuffer next_buf;
    fklInitStringBuffer(&next_buf);
    char *tmp = NULL;
    *unexpectEOF = 0;
    FklNastNode *ast = NULL;
    FklGrammerMatchOuterCtx outerCtx = FKL_NAST_PARSE_OUTER_CTX_INIT(st);
    outerCtx.line = line;
    size_t offset = 0;
    for (;;) {
        size_t restLen = size - offset;
        int err = 0;
        ast = fklDefaultParseForCharBuf(tmp + offset, restLen, &restLen,
                                        &outerCtx, &err, errLine, symbolStack,
                                        lineStack, stateStack);
        if (err == FKL_PARSE_WAITING_FOR_MORE && feof(fp)) {
            *errLine = outerCtx.line;
            *unexpectEOF = FKL_PARSE_TERMINAL_MATCH_FAILED;
            fklZfree(tmp);
            tmp = NULL;
            break;
        } else if (err == FKL_PARSE_TERMINAL_MATCH_FAILED) {
            if (restLen) {
                *errLine = fklUintVectorIsEmpty(lineStack)
                             ? outerCtx.line
                             : *fklUintVectorBack(lineStack);
                *unexpectEOF = FKL_PARSE_REDUCE_FAILED;
                fklZfree(tmp);
                tmp = NULL;
                break;
            } else if (feof(fp)) {
                if (!fklAnalysisSymbolVectorIsEmpty(symbolStack)) {
                    *errLine = lineStack->base[0];
                    *unexpectEOF = FKL_PARSE_TERMINAL_MATCH_FAILED;
                    fklZfree(tmp);
                    tmp = NULL;
                }
                break;
            }
        } else if (err == FKL_PARSE_REDUCE_FAILED) {
            *unexpectEOF = err;
            fklZfree(tmp);
            tmp = NULL;
            break;
        } else if (ast) {
            if (restLen) {
                fklRewindStream(fp, tmp + size - restLen, restLen);
                size -= restLen;
            }
            *output = ast;
            break;
        }
        fklGetDelim(fp, &next_buf, '\n');
        size_t nextSize = next_buf.index;
        offset = size - restLen;
        if (nextSize == 0)
            continue;
        tmp = (char *)fklZrealloc(tmp, (size + nextSize) * sizeof(char));
        FKL_ASSERT(tmp);
        memcpy(&tmp[size], next_buf.buf, nextSize);
        size += nextSize;
        next_buf.index = 0;
    }
    *pline = outerCtx.line;
    *psize = size;
    fklUninitStringBuffer(&next_buf);
    return tmp;
}

char *fklReadWithAnalysisTable(const FklGrammer *g, FILE *fp, size_t *psize,
                               size_t line, size_t *pline, FklSymbolTable *st,
                               int *unexpectEOF, size_t *errLine,
                               FklNastNode **output,
                               FklAnalysisSymbolVector *symbolStack,
                               FklUintVector *lineStack,
                               FklParseStateVector *stateStack) {
    size_t size = 0;
    FklStringBuffer next_buf;
    fklInitStringBuffer(&next_buf);
    char *tmp = NULL;
    *unexpectEOF = 0;
    FklNastNode *ast = NULL;
    FklGrammerMatchOuterCtx outerCtx = FKL_NAST_PARSE_OUTER_CTX_INIT(st);
    outerCtx.line = line;
    size_t offset = 0;
    for (;;) {
        size_t restLen = size - offset;
        int err = 0;
        ast = fklParseWithTableForCharBuf(g, tmp + offset, restLen, &restLen,
                                          &outerCtx, st, &err, errLine,
                                          symbolStack, lineStack, stateStack);
        if (err == FKL_PARSE_WAITING_FOR_MORE && feof(fp)) {
            *errLine = outerCtx.line;
            *unexpectEOF = FKL_PARSE_TERMINAL_MATCH_FAILED;
            fklZfree(tmp);
            tmp = NULL;
            break;
        } else if (err == FKL_PARSE_TERMINAL_MATCH_FAILED) {
            if (restLen) {
                *errLine = fklUintVectorIsEmpty(lineStack)
                             ? outerCtx.line
                             : *fklUintVectorBack(lineStack);
                *unexpectEOF = FKL_PARSE_REDUCE_FAILED;
                fklZfree(tmp);
                tmp = NULL;
                break;
            } else if (feof(fp)) {
                if (!fklAnalysisSymbolVectorIsEmpty(symbolStack)) {
                    *errLine = lineStack->base[0];
                    *unexpectEOF = FKL_PARSE_TERMINAL_MATCH_FAILED;
                    fklZfree(tmp);
                    tmp = NULL;
                }
                break;
            }
        } else if (err == FKL_PARSE_REDUCE_FAILED) {
            *unexpectEOF = err;
            fklZfree(tmp);
            tmp = NULL;
            break;
        } else if (ast) {
            if (restLen) {
                fklRewindStream(fp, tmp + size - restLen, restLen);
                size -= restLen;
            }
            *output = ast;
            break;
        }
        fklGetDelim(fp, &next_buf, '\n');
        size_t nextSize = next_buf.index;
        offset = size - restLen;
        if (nextSize == 0)
            continue;
        tmp = (char *)fklZrealloc(tmp, (size + nextSize) * sizeof(char));
        FKL_ASSERT(tmp);
        memcpy(&tmp[size], next_buf.buf, nextSize);
        size += nextSize;
        next_buf.index = 0;
    }
    *pline = outerCtx.line;
    *psize = size;
    fklUninitStringBuffer(&next_buf);
    return tmp;
}

void *fklNastTerminalCreate(const char *s, size_t len, size_t line, void *ctx) {
    FklNastNode *ast = fklCreateNastNode(FKL_NAST_STR, line);
    ast->str = fklCreateString(len, s);
    return ast;
}
