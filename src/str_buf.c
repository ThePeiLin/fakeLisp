#include <fakeLisp/str_buf.h>
#include <fakeLisp/zmalloc.h>

#include <stdarg.h>

FklStrBuf *fklCreateStrBuf(void) {
    FklStrBuf *r = (FklStrBuf *)fklZmalloc(sizeof(FklStrBuf));
    FKL_ASSERT(r);
    fklInitStrBuf(r);
    return r;
}

void fklInitStrBuf(FklStrBuf *b) {
    b->index = 0;
    b->size = 0;
    b->buf = NULL;
    fklStrBufReserve(b, 64);
    b->buf[0] = '\0';
}

void fklInitStrBufWithCapacity(FklStrBuf *b, size_t len) {
    b->index = 0;
    b->size = 0;
    b->buf = NULL;
    fklStrBufReserve(b, len);
    b->buf[0] = '\0';
}

void fklStrBufReserve(FklStrBuf *b, size_t s) {
    if ((b->size - b->index) < s) {
        b->size <<= 1;
        if ((b->size - b->index) < s)
            b->size += s;
        char *t = (char *)fklZrealloc(b->buf, b->size);
        FKL_ASSERT(t);
        b->buf = t;
    }
}

void fklStrBufShrinkTo(FklStrBuf *b, size_t s) {
    if (s < (b->index + 1))
        s = (b->index + 1);
    char *t = (char *)fklZrealloc(b->buf, s);
    FKL_ASSERT(t);
    b->buf = t;
    b->size = s;
}

static inline void stringbuffer_reserve(FklStrBuf *b, size_t s) {
    if (b->size < s) {
        b->size <<= 1;
        if (b->size < s)
            b->size = s;
        char *t = (char *)fklZrealloc(b->buf, b->size);
        FKL_ASSERT(t);
        b->buf = t;
    }
}

void fklStrBufResize(FklStrBuf *b, size_t ns, char c) {
    if (ns > b->index) {
        stringbuffer_reserve(b, ns + 1);
        memset(&b->buf[b->index], c, ns - b->index);
        b->buf[ns] = '\0';
    }
    b->index = ns;
}

void fklUninitStrBuf(FklStrBuf *b) {
    b->size = 0;
    b->index = 0;
    fklZfree(b->buf);
    b->buf = NULL;
}

void fklDestroyStrBuf(FklStrBuf *b) {
    fklUninitStrBuf(b);
    fklZfree(b);
}

void fklStrBufClear(FklStrBuf *b) {
    b->index = 0;
    b->buf[0] = '\0';
}

void fklStrBufMoveToFront(FklStrBuf *b, uint32_t idx) {
    uint32_t new_idx = b->index - idx;
    memmove(b->buf, &b->buf[idx], new_idx);
    b->buf[new_idx] = '\0';
    b->index = new_idx;
}

void fklStrBufFill(FklStrBuf *b, char c) { memset(b->buf, c, b->index); }

void fklStrBufBincpy(FklStrBuf *b, const void *p, size_t l) {
    fklStrBufReserve(b, l + 1);
    memcpy(&b->buf[b->index], p, l);
    b->index += l;
    b->buf[b->index] = '\0';
}

void fklStrBufPutc(FklStrBuf *b, char c) {
    fklStrBufReserve(b, 2);
    b->buf[b->index++] = c;
    b->buf[b->index] = '\0';
}

long fklStrBufPrintfVa(FklStrBuf *b, const char *fmt, va_list ap) {
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
            fklStrBufReserve(b, n + 1);
        else
            fklStrBufReserve(b, (b->size) * 2);
    }
    return n;
}

long fklStrBufPrintf(FklStrBuf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    long r = fklStrBufPrintfVa(b, fmt, ap);
    va_end(ap);
    return r;
}

int fklStrBufCmp(const FklStrBuf *a, const FklStrBuf *b) {
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

static int strbuf_cb_printf(void *ctx, const char *fmt, va_list ap) {
    FklStrBuf *buf = FKL_TYPE_CAST(FklStrBuf *, ctx);
    return fklStrBufPrintfVa(buf, fmt, ap);
}

static int strbuf_cb_puts(void *ctx, const char *fmt) {
    FklStrBuf *buf = FKL_TYPE_CAST(FklStrBuf *, ctx);
    fklStrBufPuts(buf, fmt);
    return 0;
}

static int strbuf_cb_putc(void *ctx, char c) {
    FklStrBuf *fp = FKL_TYPE_CAST(FklStrBuf *, ctx);
    fklStrBufPutc(fp, c);
    return (uint8_t)c;
}

static size_t strbuf_cb_write(void *ctx, size_t len, const void *s) {
    FklStrBuf *fp = FKL_TYPE_CAST(FklStrBuf *, ctx);
    fklStrBufBincpy(fp, s, len);
    return len;
}

static const FklCodeBuilderMethodTable strbuf_cb_method_table = {
    .cb_printf = strbuf_cb_printf,
    .cb_puts = strbuf_cb_puts,
    .cb_putc = strbuf_cb_putc,
    .cb_write = strbuf_cb_write,
};

void fklInitCodeBuilderStrBuf(FklCodeBuilder *b,
        FklStrBuf *fp,
        const char *indent_str) {
    fklInitCodeBuilder(b, fp, &strbuf_cb_method_table, indent_str);
}
