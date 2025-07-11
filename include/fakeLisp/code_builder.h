#ifndef FKL_CODE_BUILDER
#define FKL_CODE_BUILDER

#include "common.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long (
        *FklCodeBuilderLinePrintf)(void *ctx, const char *fmt, va_list va);
typedef void (*FklCodeBuilderPuts)(void *ctx, const char *s);

typedef struct FklCodeBuilder {
    void *ctx;

    unsigned int indents;
    const char *indent_str;

    FklCodeBuilderLinePrintf printf;
    FklCodeBuilderPuts puts;

    int line_start;
} FklCodeBuilder;

static inline void fklCodeBuilderIndent(FklCodeBuilder *b) { ++b->indents; }

static inline void fklCodeBuilderUnindent(FklCodeBuilder *b) {
    FKL_ASSERT(b->indents);
    --b->indents;
};

static inline long
fklCodeBuilderFmtVa(const FklCodeBuilder *b, const char *fmt, va_list ap) {
    return b->printf(b->ctx, fmt, ap);
}

static inline long
fklCodeBuilderFmt(const FklCodeBuilder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    long r = fklCodeBuilderFmtVa(b, fmt, ap);
    va_end(ap);
    return r;
}

static inline int
fklCodeBuilderLineStartVa(FklCodeBuilder *b, const char *fmt, va_list ap) {
    FKL_ASSERT(!b->line_start);
    for (unsigned int i = 0; i < b->indents; ++i)
        b->puts(b->ctx, b->indent_str);
    long r = fklCodeBuilderFmtVa(b, fmt, ap);
    b->line_start = 1;
    return r;
}

static inline int
fklCodeBuilderLineStart(FklCodeBuilder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    long r = fklCodeBuilderLineStartVa(b, fmt, ap);
    va_end(ap);
    return r;
}

static inline long
fklCodeBuilderLineEndVa(FklCodeBuilder *b, const char *fmt, va_list ap) {
    FKL_ASSERT(b->line_start);
    long r = fklCodeBuilderFmtVa(b, fmt, ap);
    b->puts(b->ctx, "\n");
    b->line_start = 0;
    return r;
}

static inline long
fklCodeBuilderLineEnd(FklCodeBuilder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    long r = fklCodeBuilderLineEndVa(b, fmt, ap);
    va_end(ap);
    return r;
}

static inline int
fklCodeBuilderLineVa(const FklCodeBuilder *b, const char *fmt, va_list ap) {
    if (*fmt && !b->line_start) {
        for (unsigned int i = 0; i < b->indents; ++i)
            b->puts(b->ctx, b->indent_str);
    }

    long r = fklCodeBuilderFmtVa(b, fmt, ap);

    if (!b->line_start)
        b->puts(b->ctx, "\n");
    return r;
}

static inline int
fklCodeBuilderLine(const FklCodeBuilder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    long r = fklCodeBuilderLineVa(b, fmt, ap);
    va_end(ap);
    return r;
}

static inline void fklInitCodeBuilder(FklCodeBuilder *b,
        void *ctx,
        FklCodeBuilderLinePrintf line,
        FklCodeBuilderPuts puts,
        const char *indent_str) {
    FKL_ASSERT(line && puts);
    b->printf = line;
    b->puts = puts;
    b->indents = 0;
    b->ctx = ctx;
    b->line_start = 0;
    b->indent_str = indent_str == NULL ? "    " : indent_str;
}

static inline long
fklCodeBuilderFpPrintf(void *ctx, const char *fmt, va_list ap) {
    FILE *fp = FKL_TYPE_CAST(FILE *, ctx);
    return vfprintf(fp, fmt, ap);
}

static inline void fklCodeBuilderFpPuts(void *ctx, const char *fmt) {
    FILE *fp = FKL_TYPE_CAST(FILE *, ctx);
    fputs(fmt, fp);
}

static inline void
fklInitCodeBuilderFp(FklCodeBuilder *b, FILE *fp, const char *indent_str) {
    fklInitCodeBuilder(b,
            fp,
            fklCodeBuilderFpPrintf,
            fklCodeBuilderFpPuts,
            indent_str);
}

#ifdef __cplusplus
}
#endif

#endif
