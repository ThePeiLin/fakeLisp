#ifndef FKL_CODE_BUILDER
#define FKL_CODE_BUILDER

#include "common.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*FklStrBuilderPrintf)(void *ctx, const char *fmt, va_list va);
typedef int (*FklStrBuilderPuts)(void *ctx, const char *s);
typedef int (*FklStrBuilderPutc)(void *ctx, int c);
typedef size_t (*FklStrBuilderWrite)(void *ctx, size_t len, const void *s);

typedef struct {
    FklStrBuilderPrintf const cb_printf;
    FklStrBuilderPuts const cb_puts;
    FklStrBuilderPutc const cb_putc;
    FklStrBuilderWrite const cb_write;
} FklStrBuilderMethodTable;

typedef struct FklStrBuilder {
    const FklStrBuilderMethodTable *t;
    void *ctx;

    unsigned int indents;
    const char *indent_str;

    int line_start;
} FklStrBuilder;

static inline void fklStrBuilderIndent(FklStrBuilder *b) { ++b->indents; }

static inline void fklStrBuilderUnindent(FklStrBuilder *b) {
    FKL_ASSERT(b->indents);
    --b->indents;
};

static inline int
fklStrBuilderFmtVa(const FklStrBuilder *b, const char *fmt, va_list ap) {
    return b->t->cb_printf(b->ctx, fmt, ap);
}

FKL_FMT_ATTR(2, 3)
static inline int
fklStrBuilderFmt(const FklStrBuilder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = fklStrBuilderFmtVa(b, fmt, ap);
    va_end(ap);
    return r;
}

static inline int fklStrBuilderPuts(const FklStrBuilder *b, const char *s) {
    return b->t->cb_puts(b->ctx, s);
}

static inline int fklStrBuilderPutc(const FklStrBuilder *b, int c) {
    return b->t->cb_putc(b->ctx, c);
}

static inline size_t
fklStrBuilderWrite(const FklStrBuilder *b, size_t c, const void *s) {
    return b->t->cb_write(b->ctx, c, s);
}

static inline int fklStrBuilderPutEscSeq(const FklStrBuilder *b, int ch) {
    int r = 0;
    if ((r = ch == '\n'))
        fklStrBuilderPuts(b, "\\n");
    else if ((r = ch == '\t'))
        fklStrBuilderPuts(b, "\\t");
    else if ((r = ch == '\v'))
        fklStrBuilderPuts(b, "\\v");
    else if ((r = ch == '\a'))
        fklStrBuilderPuts(b, "\\a");
    else if ((r = ch == '\b'))
        fklStrBuilderPuts(b, "\\b");
    else if ((r = ch == '\f'))
        fklStrBuilderPuts(b, "\\f");
    else if ((r = ch == '\r'))
        fklStrBuilderPuts(b, "\\r");
    else if ((r = ch == '\x20'))
        fklStrBuilderPutc(b, ' ');
    return r;
}

static inline int
fklStrBuilderLineStartVa(FklStrBuilder *b, const char *fmt, va_list ap) {
    FKL_ASSERT(!b->line_start);
    for (unsigned int i = 0; i < b->indents; ++i)
        b->t->cb_puts(b->ctx, b->indent_str);
    int r = fklStrBuilderFmtVa(b, fmt, ap);
    b->line_start = 1;
    return r;
}

FKL_FMT_ATTR(2, 3)
static inline int
fklStrBuilderLineStart(FklStrBuilder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    long r = fklStrBuilderLineStartVa(b, fmt, ap);
    va_end(ap);
    return r;
}

static inline int
fklStrBuilderLineEndVa(FklStrBuilder *b, const char *fmt, va_list ap) {
    FKL_ASSERT(b->line_start);
    int r = fklStrBuilderFmtVa(b, fmt, ap);
    b->t->cb_puts(b->ctx, "\n");
    b->line_start = 0;
    return r;
}

FKL_FMT_ATTR(2, 3)
static inline int fklStrBuilderLineEnd(FklStrBuilder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = fklStrBuilderLineEndVa(b, fmt, ap);
    va_end(ap);
    return r;
}

static inline int
fklStrBuilderLineVa(const FklStrBuilder *b, const char *fmt, va_list ap) {
    if (*fmt && !b->line_start) {
        for (unsigned int i = 0; i < b->indents; ++i)
            b->t->cb_puts(b->ctx, b->indent_str);
    }

    long r = fklStrBuilderFmtVa(b, fmt, ap);

    if (!b->line_start)
        b->t->cb_puts(b->ctx, "\n");
    return r;
}

FKL_FMT_ATTR(2, 3)
static inline int
fklStrBuilderLine(const FklStrBuilder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    long r = fklStrBuilderLineVa(b, fmt, ap);
    va_end(ap);
    return r;
}

static inline void fklInitStrBuilder(FklStrBuilder *b,
        void *ctx,
        const FklStrBuilderMethodTable *t,
        const char *indent_str) {
    FKL_ASSERT(t);
    memset(b, 0, sizeof(*b));
    b->t = t;
    b->indents = 0;
    b->ctx = ctx;
    b->line_start = 0;
    b->indent_str = indent_str == NULL ? "    " : indent_str;
}

#ifdef __cplusplus
}
#endif

#endif
