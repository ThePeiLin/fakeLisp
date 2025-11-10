#ifndef FKL_STR_BUF_H
#define FKL_STR_BUF_H

#include "common.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklStringBuffer {
    uint32_t size;
    uint32_t index;
    char *buf;
} FklStringBuffer;

#define FKL_STRING_BUFFER_INIT { 0, 0, NULL }

FklStringBuffer *fklCreateStringBuffer(void);
void fklInitStringBuffer(FklStringBuffer *);
void fklInitStringBufferWithCapacity(FklStringBuffer *, size_t s);
void fklStringBufferReserve(FklStringBuffer *, size_t s);
void fklStringBufferShrinkTo(FklStringBuffer *, size_t s);
void fklStringBufferResize(FklStringBuffer *, size_t s, char content);
void fklUninitStringBuffer(FklStringBuffer *);
void fklDestroyStringBuffer(FklStringBuffer *);
void fklStringBufferClear(FklStringBuffer *);
void fklStringBufferMoveToFront(FklStringBuffer *buf, uint32_t idx);
void fklStringBufferFill(FklStringBuffer *, char);
void fklStringBufferBincpy(FklStringBuffer *, const void *, size_t);

void fklStringBufferPutc(FklStringBuffer *, char);
long fklStringBufferPrintfVa(FklStringBuffer *b, const char *fmt, va_list ap);
long fklStringBufferPrintf(FklStringBuffer *, const char *fmt, ...);
int fklStringBufferPutEscSeq(FklStringBuffer *s, char ch);
int fklStringBufferCmp(const FklStringBuffer *a, const FklStringBuffer *b);

static inline uint32_t fklStringBufferLen(FklStringBuffer *b) {
    return b->index;
}

static inline char *fklStringBufferBody(FklStringBuffer *b) { return b->buf; }

static inline void fklStringBufferConcatWithCstr(FklStringBuffer *b,
        const char *s) {
    fklStringBufferBincpy(b, s, strlen(s));
}

static inline void fklStringBufferConcatWithStringBuffer(FklStringBuffer *a,
        const FklStringBuffer *b) {
    fklStringBufferBincpy(a, b->buf, b->index);
}

#ifdef __cplusplus
}
#endif

#endif
