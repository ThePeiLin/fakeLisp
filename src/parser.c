#include <fakeLisp/base.h>
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

    FklAnalysisSymbol *cur_sym = symbolStack.base;
    FklAnalysisSymbol *const sym_end = cur_sym + symbolStack.size;
    for (; cur_sym < sym_end; ++cur_sym)
        fklDestroyNastNode(cur_sym->ast);
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
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    *unexpectEOF = 0;
    FklNastNode *ast = NULL;
    FklGrammerMatchOuterCtx outerCtx = FKL_NAST_PARSE_OUTER_CTX_INIT(st);
    outerCtx.line = line;
    size_t offset = 0;
    for (;;) {
        size_t restLen = buf.index - offset;
        int err = 0;
        ast = fklDefaultParseForCharBuf(buf.buf + offset, restLen, &restLen,
                                        &outerCtx, &err, errLine, symbolStack,
                                        lineStack, stateStack);
        if (err == FKL_PARSE_WAITING_FOR_MORE && feof(fp)) {
            *errLine = outerCtx.line;
            *unexpectEOF = FKL_PARSE_TERMINAL_MATCH_FAILED;
            buf.index = 0;
            break;
        } else if (err == FKL_PARSE_TERMINAL_MATCH_FAILED) {
            if (restLen) {
                *errLine = fklUintVectorIsEmpty(lineStack)
                             ? outerCtx.line
                             : *fklUintVectorBack(lineStack);
                *unexpectEOF = FKL_PARSE_REDUCE_FAILED;
                fklStringBufferClear(&buf);
                break;
            } else if (feof(fp)) {
                if (!fklAnalysisSymbolVectorIsEmpty(symbolStack)) {
                    *errLine = lineStack->base[0];
                    *unexpectEOF = FKL_PARSE_TERMINAL_MATCH_FAILED;
                    fklStringBufferClear(&buf);
                }
                break;
            }
        } else if (err == FKL_PARSE_REDUCE_FAILED) {
            *unexpectEOF = err;
            fklStringBufferClear(&buf);
            break;
        } else if (ast) {
            if (restLen) {
                fklRewindStream(fp, buf.buf + buf.index - restLen, restLen);
                buf.index -= restLen;
                buf.buf[buf.index] = '\0';
            }
            *output = ast;
            break;
        }
        offset = buf.index - restLen;
        fklGetDelim(fp, &buf, '\n');
    }
    *pline = outerCtx.line;
    *psize = buf.index;
    char *tmp = buf.index ? fklZstrdup(buf.buf) : NULL;
    fklUninitStringBuffer(&buf);
    return tmp;
}

char *fklReadWithAnalysisTable(const FklGrammer *g, FILE *fp, size_t *psize,
                               size_t line, size_t *pline, FklSymbolTable *st,
                               int *unexpectEOF, size_t *errLine,
                               FklNastNode **output,
                               FklAnalysisSymbolVector *symbolStack,
                               FklUintVector *lineStack,
                               FklParseStateVector *stateStack) {
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    *unexpectEOF = 0;
    FklNastNode *ast = NULL;
    FklGrammerMatchOuterCtx outerCtx = FKL_NAST_PARSE_OUTER_CTX_INIT(st);
    outerCtx.line = line;
    size_t offset = 0;
    for (;;) {
        size_t restLen = buf.index - offset;
        int err = 0;
        ast = fklParseWithTableForCharBuf2(
            g, buf.buf + offset, restLen, &restLen, &outerCtx, st, &err,
            errLine, symbolStack, lineStack, stateStack);
        if (err == FKL_PARSE_WAITING_FOR_MORE && feof(fp)) {
            *errLine = outerCtx.line;
            *unexpectEOF = FKL_PARSE_TERMINAL_MATCH_FAILED;
            buf.index = 0;
            break;
        } else if (err == FKL_PARSE_TERMINAL_MATCH_FAILED) {
            if (restLen) {
                *errLine = fklUintVectorIsEmpty(lineStack)
                             ? outerCtx.line
                             : *fklUintVectorBack(lineStack);
                *unexpectEOF = FKL_PARSE_REDUCE_FAILED;
                buf.index = 0;
                break;
            } else if (feof(fp)) {
                if (!fklAnalysisSymbolVectorIsEmpty(symbolStack)) {
                    *errLine = lineStack->base[0];
                    *unexpectEOF = FKL_PARSE_TERMINAL_MATCH_FAILED;
                    buf.index = 0;
                }
                break;
            }
        } else if (err == FKL_PARSE_REDUCE_FAILED) {
            *unexpectEOF = err;
            buf.index = 0;
            break;
        } else if (ast) {
            if (restLen) {
                fklRewindStream(fp, buf.buf + buf.index - restLen, restLen);
                buf.index -= restLen;
                buf.buf[buf.index] = '\0';
            }
            *output = ast;
            break;
        }
        offset = buf.index - restLen;
        fklGetDelim(fp, &buf, '\n');
    }
    *pline = outerCtx.line;
    *psize = buf.index;
    char *tmp = buf.index ? fklZstrdup(buf.buf) : NULL;
    fklUninitStringBuffer(&buf);
    return tmp;
}

void *fklNastTerminalCreate(const char *s, size_t len, size_t line, void *ctx) {
    FklNastNode *ast = fklCreateNastNode(FKL_NAST_STR, line);
    ast->str = fklCreateString(len, s);
    return ast;
}
