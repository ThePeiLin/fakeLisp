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
    size_t size;
    size_t index;
    char *buf;
} FklStrBuf;

#define FKL_STRING_BUFFER_INIT { 0, 0, NULL }

FKL_API FklStrBuf *fklCreateStrBuf(void);
FKL_API void fklInitStrBuf(FklStrBuf *);
FKL_API void fklInitStrBufWithCapacity(FklStrBuf *, size_t s);
FKL_API void fklStrBufReserve(FklStrBuf *, size_t s);
FKL_API void fklStrBufShrinkTo(FklStrBuf *, size_t s);
FKL_API void fklStrBufResize(FklStrBuf *, size_t s, char content);
FKL_API void fklUninitStrBuf(FklStrBuf *);
FKL_API void fklDestroyStrBuf(FklStrBuf *);
FKL_API void fklStrBufClear(FklStrBuf *);
FKL_API void fklStrBufMoveToFront(FklStrBuf *buf, size_t idx);
FKL_API void fklStrBufFill(FklStrBuf *, char);
FKL_API void fklStrBufBincpy(FklStrBuf *, const void *, size_t);

FKL_API void fklStrBufPutc(FklStrBuf *, int);
FKL_API long fklStrBufPrintfVa(FklStrBuf *b, const char *fmt, va_list ap);

FKL_API
FKL_FMT_ATTR(2, 3)
long fklStrBufPrintf(FklStrBuf *, const char *fmt, ...);

FKL_API
int fklStrBufCmp(const FklStrBuf *a, const FklStrBuf *b);

FKL_API
void fklInitCodeBuilderStrBuf(FklCodeBuilder *b,
        FklStrBuf *buf,
        const char *indent_str);

static inline size_t fklStrBufLen(const FklStrBuf *b) { return b->index; }

static inline char *fklStrBufBody(const FklStrBuf *b) { return b->buf; }

static inline void fklStrBufPuts(FklStrBuf *b, const char *s) {
    fklStrBufBincpy(b, s, strlen(s));
}

static inline void fklStrBufConcatWithCstr(FklStrBuf *b, const char *s) {
    fklStrBufBincpy(b, s, strlen(s));
}

static inline void fklStrBufConcatWithStrBuf(FklStrBuf *a, const FklStrBuf *b) {
    fklStrBufBincpy(a, b->buf, b->index);
}

#ifdef __cplusplus
}
#endif

#endif
