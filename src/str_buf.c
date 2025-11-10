#include <fakeLisp/str_buf.h>
#include <fakeLisp/zmalloc.h>

#include <stdarg.h>
#include <stdio.h>

FklStringBuffer *fklCreateStringBuffer(void) {
    FklStringBuffer *r = (FklStringBuffer *)fklZmalloc(sizeof(FklStringBuffer));
    FKL_ASSERT(r);
    fklInitStringBuffer(r);
    return r;
}

void fklInitStringBuffer(FklStringBuffer *b) {
    b->index = 0;
    b->size = 0;
    b->buf = NULL;
    fklStringBufferReserve(b, 64);
    b->buf[0] = '\0';
}

void fklInitStringBufferWithCapacity(FklStringBuffer *b, size_t len) {
    b->index = 0;
    b->size = 0;
    b->buf = NULL;
    fklStringBufferReserve(b, len);
    b->buf[0] = '\0';
}

void fklStringBufferReserve(FklStringBuffer *b, size_t s) {
    if ((b->size - b->index) < s) {
        b->size <<= 1;
        if ((b->size - b->index) < s)
            b->size += s;
        char *t = (char *)fklZrealloc(b->buf, b->size);
        FKL_ASSERT(t);
        b->buf = t;
    }
}

void fklStringBufferShrinkTo(FklStringBuffer *b, size_t s) {
    char *t = (char *)fklZrealloc(b->buf, s);
    FKL_ASSERT(t);
    t[s - 1] = '\0';
    b->buf = t;
    b->size = s;
}

static inline void stringbuffer_reserve(FklStringBuffer *b, size_t s) {
    if (b->size < s) {
        b->size <<= 1;
        if (b->size < s)
            b->size = s;
        char *t = (char *)fklZrealloc(b->buf, b->size);
        FKL_ASSERT(t);
        b->buf = t;
    }
}

void fklStringBufferResize(FklStringBuffer *b, size_t ns, char c) {
    if (ns > b->index) {
        stringbuffer_reserve(b, ns + 1);
        memset(&b->buf[b->index], c, ns - b->index);
        b->buf[ns] = '\0';
    }
    b->index = ns;
}

void fklUninitStringBuffer(FklStringBuffer *b) {
    b->size = 0;
    b->index = 0;
    fklZfree(b->buf);
    b->buf = NULL;
}

void fklDestroyStringBuffer(FklStringBuffer *b) {
    fklUninitStringBuffer(b);
    fklZfree(b);
}

void fklStringBufferClear(FklStringBuffer *b) {
    b->index = 0;
    b->buf[0] = '\0';
}

void fklStringBufferMoveToFront(FklStringBuffer *b, uint32_t idx) {
    uint32_t new_idx = b->index - idx;
    memmove(b->buf, &b->buf[idx], new_idx);
    b->buf[new_idx] = '\0';
    b->index = new_idx;
}

void fklStringBufferFill(FklStringBuffer *b, char c) {
    memset(b->buf, c, b->index);
}

void fklStringBufferBincpy(FklStringBuffer *b, const void *p, size_t l) {
    fklStringBufferReserve(b, l + 1);
    memcpy(&b->buf[b->index], p, l);
    b->index += l;
    b->buf[b->index] = '\0';
}

void fklStringBufferPutc(FklStringBuffer *b, char c) {
    fklStringBufferReserve(b, 2);
    b->buf[b->index++] = c;
    b->buf[b->index] = '\0';
}

long fklStringBufferPrintfVa(FklStringBuffer *b, const char *fmt, va_list ap) {
    long n;
    va_list cp;
    for (;;) {
#ifdef _WIN32
        cp = ap;
#else
        va_copy(cp, ap);
#endif
        n = vsnprintf(&b->buf[b->index], b->size - b->index, fmt, cp);
        va_end(cp);
        if ((n > -1) && n < (b->size - b->index)) {
            b->index += n;
            return n;
        }
        if (n > -1)
            fklStringBufferReserve(b, n + 1);
        else
            fklStringBufferReserve(b, (b->size) * 2);
    }
    return n;
}

long fklStringBufferPrintf(FklStringBuffer *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    long r = fklStringBufferPrintfVa(b, fmt, ap);
    va_end(ap);
    return r;
}

int fklStringBufferPutEscSeq(FklStringBuffer *s, char ch) {
    int r = 0;
    if ((r = ch == '\n'))
        fklStringBufferConcatWithCstr(s, "\\n");
    else if ((r = ch == '\t'))
        fklStringBufferConcatWithCstr(s, "\\t");
    else if ((r = ch == '\v'))
        fklStringBufferConcatWithCstr(s, "\\v");
    else if ((r = ch == '\a'))
        fklStringBufferConcatWithCstr(s, "\\a");
    else if ((r = ch == '\b'))
        fklStringBufferConcatWithCstr(s, "\\b");
    else if ((r = ch == '\f'))
        fklStringBufferConcatWithCstr(s, "\\f");
    else if ((r = ch == '\r'))
        fklStringBufferConcatWithCstr(s, "\\r");
    else if ((r = ch == '\x20'))
        fklStringBufferPutc(s, ' ');
    return r;
}

int fklStringBufferCmp(const FklStringBuffer *a, const FklStringBuffer *b) {
    size_t size = a->index < b->index ? a->index : b->index;
    int r = memcmp(a->buf, b->buf, size);
    if (r)
        return r;
    else if (a->index < b->index)
        return -1;
    else if (a->index > b->index)
        return 1;
    return r;
}
