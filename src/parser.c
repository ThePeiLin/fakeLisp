#include <fakeLisp/base.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <string.h>

FklVMvalue *
fklCreateNastNodeFromCstr(FklVM *vm, const char *cStr, FklVMvalueLnt *ln) {
    FklParseStateVector stateStack;
    FklAnalysisSymbolVector symbolStack;
    FklUintVector lineStack;
    fklParseStateVectorInit(&stateStack, 16);
    fklAnalysisSymbolVectorInit(&symbolStack, 16);
    fklUintVectorInit(&lineStack, 16);
    fklVMvaluePushState0ToStack(&stateStack);

    FklParseError err = 0;
    size_t errLine = 0;
    FklGrammerMatchCtx ctx = FKL_VMVALUE_PARSE_CTX_INIT(vm, ln);
    FklVMvalue *node = fklDefaultParseForCstr(cStr,
            &ctx,
            &err,
            &errLine,
            &symbolStack,
            &lineStack,
            &stateStack);

    fklAnalysisSymbolVectorUninit(&symbolStack);
    fklParseStateVectorUninit(&stateStack);
    fklUintVectorUninit(&lineStack);

    return node;
}

char *fklReadWithBuiltinParser(FILE *fp,
        size_t *psize,
        size_t line,
        size_t *pline,
        FklVM *vm,
        int *unexpectEOF,
        size_t *errLine,
        FklVMvalue **output,
        FklAnalysisSymbolVector *symbolStack,
        FklUintVector *lineStack,
        FklParseStateVector *stateStack,
        FklVMvalueLnt *ln) {
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    *unexpectEOF = 0;
    FklVMvalue *ast = NULL;
    FklGrammerMatchCtx ctx = FKL_VMVALUE_PARSE_CTX_INIT(vm, ln);
    ctx.line = line;
    size_t offset = 0;
    for (;;) {
        size_t restLen = buf.index - offset;
        FklParseError err = 0;
        ast = fklDefaultParseForCharBuf(buf.buf + offset,
                restLen,
                &restLen,
                &ctx,
                &err,
                errLine,
                symbolStack,
                lineStack,
                stateStack);
        if (err == FKL_PARSE_WAITING_FOR_MORE && feof(fp)) {
            *errLine = ctx.line;
            *unexpectEOF = FKL_PARSE_TERMINAL_MATCH_FAILED;
            buf.index = 0;
            break;
        } else if (err == FKL_PARSE_TERMINAL_MATCH_FAILED) {
            if (restLen) {
                *errLine = fklUintVectorIsEmpty(lineStack)
                                 ? ctx.line
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
    *pline = ctx.line;
    *psize = buf.index;
    char *tmp = buf.index ? fklZstrdup(buf.buf) : NULL;
    fklUninitStringBuffer(&buf);
    return tmp;
}

char *fklReadWithAnalysisTable(const FklGrammer *g,
        FILE *fp,
        size_t *psize,
        size_t line,
        size_t *pline,
        FklVM *vm,
        int *unexpectEOF,
        size_t *errLine,
        FklVMvalue **output,
        FklAnalysisSymbolVector *symbolStack,
        FklUintVector *lineStack,
        FklParseStateVector *stateStack,
        FklVMvalueLnt *ln) {
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    *unexpectEOF = 0;
    FklVMvalue *ast = NULL;
    FklGrammerMatchCtx ctx = FKL_VMVALUE_PARSE_CTX_INIT(vm, ln);
    ctx.line = line;
    size_t offset = 0;
    for (;;) {
        size_t restLen = buf.index - offset;
        FklParseError err = 0;
        ast = fklParseWithTableForCharBuf2(g,
                buf.buf + offset,
                restLen,
                &restLen,
                &ctx,
                &err,
                errLine,
                symbolStack,
                lineStack,
                stateStack);
        if (err == FKL_PARSE_WAITING_FOR_MORE && feof(fp)) {
            *errLine = ctx.line;
            *unexpectEOF = FKL_PARSE_TERMINAL_MATCH_FAILED;
            buf.index = 0;
            break;
        } else if (err == FKL_PARSE_TERMINAL_MATCH_FAILED) {
            if (restLen) {
                *errLine = fklUintVectorIsEmpty(lineStack)
                                 ? ctx.line
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
    *pline = ctx.line;
    *psize = buf.index;
    char *tmp = buf.index ? fklZstrdup(buf.buf) : NULL;
    fklUninitStringBuffer(&buf);
    return tmp;
}

void *fklParseWithTableForCstr(const FklGrammer *g,
        const char *cstr,
        FklGrammerMatchCtx *ctx,
        FklParseError *err) {
    return fklParseWithTableForCharBuf(g, cstr, strlen(cstr), ctx, err);
}

void *fklParseWithTableForCharBuf(const FklGrammer *g,
        const char *cstr,
        size_t restLen,
        FklGrammerMatchCtx *ctx,
        FklParseError *err) {
    const FklAnalysisTable *t = &g->aTable;
    size_t errLine = 0;
    FklAnalysisSymbolVector symbolStack;
    FklParseStateVector stateStack;
    FklUintVector lineStack;
    fklParseStateVectorInit(&stateStack, 16);
    fklAnalysisSymbolVectorInit(&symbolStack, 16);
    fklUintVectorInit(&lineStack, 16);
    fklParseStateVectorPushBack2(&stateStack,
            (FklParseState){ .state = &t->states[0] });

    void *ast = fklParseWithTableForCharBuf2(g,
            cstr,
            restLen,
            &restLen,
            ctx,
            err,
            &errLine,
            &symbolStack,
            &lineStack,
            &stateStack);

    fklUintVectorUninit(&lineStack);
    fklParseStateVectorUninit(&stateStack);
    fklAnalysisSymbolVectorUninit(&symbolStack);
    return ast;
}

static inline int do_reduce_action(FklParseStateVector *stateStack,
        FklAnalysisSymbolVector *symbolStack,
        FklUintVector *lineStack,
        const FklGrammerProduction *prod,
        size_t len,
        FklGrammerMatchCtx *ctx,
        size_t *errLine) {
    stateStack->size -= len;
    symbolStack->size -= len;
    FklAnalysisSymbol *base = &symbolStack->base[symbolStack->size];
    FklAnalysisStateGoto *gt =
            fklParseStateVectorBackNonNull(stateStack)->state->state.gt;
    const FklAnalysisState *state = NULL;
    FklGrammerNonterm left = prod->left;
    for (; gt; gt = gt->next) {
        if ((gt->allow_ignore || !len || !base[0].start_with_ignore)
                && fklNontermEqual(&gt->nt, &left)) {
            state = gt->state;
            break;
        }
    }
    if (!state)
        return 1;
    size_t line = fklGetFirstNthLine(lineStack, len, ctx->line);
    lineStack->size -= len;
    void *ast = prod->func(prod->ctx, ctx->ctx, base, len, line);
    for (size_t i = 0; i < len; i++) {
        ctx->destroy(base[i].ast);
        base[i].ast = NULL;
    }
    if (!ast) {
        *errLine = line;
        return FKL_PARSE_REDUCE_FAILED;
    }

    fklInitNontermAnalysisSymbol(
            fklAnalysisSymbolVectorPushBack(symbolStack, NULL),
            left.group,
            left.sid,
            ast,
            len && base[0].start_with_ignore);
    fklUintVectorPushBack2(lineStack, line);
    fklParseStateVectorPushBack2(stateStack, (FklParseState){ .state = state });
    return 0;
}

void *fklDefaultParseForCstr(const char *cstr,
        FklGrammerMatchCtx *ctx,
        FklParseError *err,
        uint64_t *errLine,
        FklAnalysisSymbolVector *symbolStack,
        FklUintVector *lineStack,
        FklParseStateVector *stateStack) {
    size_t restLen = strlen(cstr);
    return fklDefaultParseForCharBuf(cstr,
            restLen,
            &restLen,
            ctx,
            err,
            errLine,
            symbolStack,
            lineStack,
            stateStack);
}

void *fklDefaultParseForCharBuf(const char *cstr,
        size_t len,
        size_t *restLen,
        FklGrammerMatchCtx *ctx,
        FklParseError *err,
        uint64_t *errLine,
        FklAnalysisSymbolVector *symbolStack,
        FklUintVector *lineStack,
        FklParseStateVector *stateStack) {
    const char *start = cstr;
    *restLen = len;
    void *ast = NULL;
    for (;;) {
        int accept = 0;
        FklStateFuncPtr state =
                fklParseStateVectorBackNonNull(stateStack)->func;
        *err = state(stateStack,
                symbolStack,
                lineStack,
                1,
                0,
                0,
                NULL,
                start,
                &cstr,
                restLen,
                ctx,
                &accept,
                errLine);
        if (*err)
            break;
        if (accept) {
            ast = fklAnalysisSymbolVectorPopBackNonNull(symbolStack)->ast;
            break;
        }
    }
    return ast;
}

// FklLineNumHashMap
#define FKL_HASH_KEY_TYPE FklVMvalue const *
#define FKL_HASH_VAL_TYPE uint64_t
#define FKL_HASH_ELM_NAME LineNum
#define FKL_HASH_KEY_HASH                                                      \
    return fklHash64Shift(FKL_TYPE_CAST(uintptr_t, (*pk)));
#include <fakeLisp/cont/hash.h>

// value line number table

static FklVMudMetaTable const LntUserDataMetaTable;
int fklIsVMvalueLnt(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &LntUserDataMetaTable;
}

FKL_VM_DEF_UD_STRUCT(FklVMvalueLnt, { FklLineNumHashMap ht; });

FKL_VM_USER_DATA_DEFAULT_PRINT(lnt_ud_print, "ln-table")

static FKL_ALWAYS_INLINE FklVMvalueLnt *as_lnt(const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueLnt(v));
    return FKL_TYPE_CAST(FklVMvalueLnt *, v);
}

static int lnt_ud_finalize(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(ht, FklLineNumHashMap, ud);
    // fklLineNumHashMapUninit(ht);
    fklLineNumHashMapUninit(&as_lnt(ud)->ht);
    return FKL_VM_UD_FINALIZE_NOW;
}

static void lnt_ud_update_weak_ref(const FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(ht, FklLineNumHashMap, ud);
    FklLineNumHashMap *ht = &as_lnt(ud)->ht;
    const FklLineNumHashMapNode *cur = ht->first;
    while (cur) {
        const FklLineNumHashMapNode *next = cur->next;
        if (cur->k->mark_ == FKL_MARK_W) {
            fklLineNumHashMapDel2(ht, cur->k);
        }
        cur = next;
    }
}

static FklVMudMetaTable const LntUserDataMetaTable = {
    // .size = sizeof(FklVMvalueIdHashMap),
    .size = sizeof(FklVMvalueLnt),
    .prin1 = lnt_ud_print,
    .princ = lnt_ud_print,
    .finalize = lnt_ud_finalize,
    .update_weak_ref = lnt_ud_update_weak_ref,
};

FklVMvalueLnt *fklCreateVMvalueLnt(FklVM *vm) {
    FklVMvalueLnt *r = (FklVMvalueLnt *)fklCreateVMvalueUd(vm,
            &LntUserDataMetaTable,
            NULL);

    fklLineNumHashMapInit(&r->ht);
    return r;
}

void fklVMvalueLntPut(FklVMvalueLnt *ht, const FklVMvalue *v, uint64_t line) {
    fklLineNumHashMapPut2(&ht->ht, v, line);
}

uint64_t *fklVMvalueLntGet(FklVMvalueLnt *ht, const FklVMvalue *v) {
    return fklLineNumHashMapGet2(&ht->ht, v);
}

#define PARSE_INCLUDED
#include "parse.h"
#undef PARSE_INCLUDED
