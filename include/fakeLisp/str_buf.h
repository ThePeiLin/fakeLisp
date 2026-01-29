#ifndef FKL_STR_BUF_H
#define FKL_STR_BUF_H

#include "code_builder.h"

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklStrBuf {
    uint32_t size;
    uint32_t index;
    char *buf;
} FklStrBuf;

#define FKL_STRING_BUFFER_INIT { 0, 0, NULL }

FklStrBuf *fklCreateStrBuf(void);
void fklInitStrBuf(FklStrBuf *);
void fklInitStrBufWithCapacity(FklStrBuf *, size_t s);
void fklStrBufReserve(FklStrBuf *, size_t s);
void fklStrBufShrinkTo(FklStrBuf *, size_t s);
void fklStrBufResize(FklStrBuf *, size_t s, char content);
void fklUninitStrBuf(FklStrBuf *);
void fklDestroyStrBuf(FklStrBuf *);
void fklStrBufClear(FklStrBuf *);
void fklStrBufMoveToFront(FklStrBuf *buf, uint32_t idx);
void fklStrBufFill(FklStrBuf *, char);
void fklStrBufBincpy(FklStrBuf *, const void *, size_t);

void fklStrBufPutc(FklStrBuf *, char);
long fklStrBufPrintfVa(FklStrBuf *b, const char *fmt, va_list ap);

FKL_FMT_ATTR(2, 3)
long fklStrBufPrintf(FklStrBuf *, const char *fmt, ...);

int fklStrBufCmp(const FklStrBuf *a, const FklStrBuf *b);

static inline uint32_t fklStrBufLen(FklStrBuf *b) { return b->index; }

static inline char *fklStrBufBody(FklStrBuf *b) { return b->buf; }

static inline void fklStrBufPuts(FklStrBuf *b, const char *s) {
    fklStrBufBincpy(b, s, strlen(s));
}

static inline void fklStrBufConcatWithCstr(FklStrBuf *b, const char *s) {
    fklStrBufBincpy(b, s, strlen(s));
}

static inline void fklStrBufConcatWithStrBuf(FklStrBuf *a, const FklStrBuf *b) {
    fklStrBufBincpy(a, b->buf, b->index);
}

void fklInitCodeBuilderStrBuf(FklCodeBuilder *b,
        FklStrBuf *buf,
        const char *indent_str);

#ifdef __cplusplus
}
#endif

#endif
