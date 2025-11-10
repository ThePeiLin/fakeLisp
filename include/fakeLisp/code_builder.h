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

typedef int (*FklCodeBuilderPrintf)(void *ctx, const char *fmt, va_list va);
typedef int (*FklCodeBuilderPuts)(void *ctx, const char *s);
typedef int (*FklCodeBuilderPutc)(void *ctx, char c);
typedef size_t (*FklCodeBuilderWrite)(void *ctx, size_t len, const char *s);

typedef struct {
    FklCodeBuilderPrintf const cb_printf;
    FklCodeBuilderPuts const cb_puts;
    FklCodeBuilderPutc const cb_putc;
    FklCodeBuilderWrite const cb_write;
} FklCodeBuilderMethodTable;

typedef struct FklCodeBuilder {
    const FklCodeBuilderMethodTable *t;
    void *ctx;

    unsigned int indents;
    const char *indent_str;

    int line_start;
} FklCodeBuilder;

static inline void fklCodeBuilderIndent(FklCodeBuilder *b) { ++b->indents; }

static inline void fklCodeBuilderUnindent(FklCodeBuilder *b) {
    FKL_ASSERT(b->indents);
    --b->indents;
};

static inline int
fklCodeBuilderFmtVa(const FklCodeBuilder *b, const char *fmt, va_list ap) {
    return b->t->cb_printf(b->ctx, fmt, ap);
}

FKL_FMT_ATTR(2, 3)
static inline int
fklCodeBuilderFmt(const FklCodeBuilder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = fklCodeBuilderFmtVa(b, fmt, ap);
    va_end(ap);
    return r;
}

static inline int fklCodeBuilderPuts(const FklCodeBuilder *b, const char *s) {
    return b->t->cb_puts(b->ctx, s);
}

static inline int fklCodeBuilderPutc(const FklCodeBuilder *b, char c) {
    return b->t->cb_putc(b->ctx, c);
}

static inline size_t
fklCodeBuilderWrite(const FklCodeBuilder *b, size_t c, const char *s) {
    return b->t->cb_write(b->ctx, c, s);
}

static inline int fklCodeBuilderPutEscSeq(const FklCodeBuilder *b, char ch) {
    int r = 0;
    if ((r = ch == '\n'))
        fklCodeBuilderPuts(b, "\\n");
    else if ((r = ch == '\t'))
        fklCodeBuilderPuts(b, "\\t");
    else if ((r = ch == '\v'))
        fklCodeBuilderPuts(b, "\\v");
    else if ((r = ch == '\a'))
        fklCodeBuilderPuts(b, "\\a");
    else if ((r = ch == '\b'))
        fklCodeBuilderPuts(b, "\\b");
    else if ((r = ch == '\f'))
        fklCodeBuilderPuts(b, "\\f");
    else if ((r = ch == '\r'))
        fklCodeBuilderPuts(b, "\\r");
    else if ((r = ch == '\x20'))
        fklCodeBuilderPutc(b, ' ');
    return r;
}

static inline int
fklCodeBuilderLineStartVa(FklCodeBuilder *b, const char *fmt, va_list ap) {
    FKL_ASSERT(!b->line_start);
    for (unsigned int i = 0; i < b->indents; ++i)
        b->t->cb_puts(b->ctx, b->indent_str);
    int r = fklCodeBuilderFmtVa(b, fmt, ap);
    b->line_start = 1;
    return r;
}

FKL_FMT_ATTR(2, 3)
static inline int
fklCodeBuilderLineStart(FklCodeBuilder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    long r = fklCodeBuilderLineStartVa(b, fmt, ap);
    va_end(ap);
    return r;
}

static inline int
fklCodeBuilderLineEndVa(FklCodeBuilder *b, const char *fmt, va_list ap) {
    FKL_ASSERT(b->line_start);
    int r = fklCodeBuilderFmtVa(b, fmt, ap);
    b->t->cb_puts(b->ctx, "\n");
    b->line_start = 0;
    return r;
}

FKL_FMT_ATTR(2, 3)
static inline int
fklCodeBuilderLineEnd(FklCodeBuilder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = fklCodeBuilderLineEndVa(b, fmt, ap);
    va_end(ap);
    return r;
}

static inline int
fklCodeBuilderLineVa(const FklCodeBuilder *b, const char *fmt, va_list ap) {
    if (*fmt && !b->line_start) {
        for (unsigned int i = 0; i < b->indents; ++i)
            b->t->cb_puts(b->ctx, b->indent_str);
    }

    long r = fklCodeBuilderFmtVa(b, fmt, ap);

    if (!b->line_start)
        b->t->cb_puts(b->ctx, "\n");
    return r;
}

FKL_FMT_ATTR(2, 3)
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
        const FklCodeBuilderMethodTable *t,
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
