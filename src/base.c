#include <fakeLisp/base.h>
#include <fakeLisp/bigint.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/zmalloc.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

FklString *fklCreateString(size_t size, const char *str) {
    FklString *tmp =
            (FklString *)fklZmalloc(sizeof(FklString) + size * sizeof(uint8_t));
    FKL_ASSERT(tmp);
    tmp->size = size;
    if (str)
        memcpy(tmp->str, str, size);
    tmp->str[size] = '\0';
    return tmp;
}

FklString *fklStringRealloc(FklString *str, size_t new_size) {
    FklString *new = (FklString *)fklZrealloc(str,
            sizeof(FklString) + new_size * sizeof(uint8_t));
    FKL_ASSERT(new);
    new->str[new_size] = '\0';
    return new;
}

int fklStringEqual(const FklString *fir, const FklString *sec) {
    if (fir->size == sec->size)
        return !memcmp(fir->str, sec->str, fir->size);
    return 0;
}

int fklStringCmp(const FklString *fir, const FklString *sec) {
    return strcmp(fir->str, sec->str);
}

int fklStringCstrCmp(const FklString *fir, const char *sec) {
    return strcmp(fir->str, sec);
}

int fklStringCharBufCmp(const FklString *fir, size_t len, const char *buf) {
    size_t size = fir->size < len ? fir->size : len;
    int r = memcmp(fir->str, buf, size);
    if (r)
        return r;
    else if (fir->size < len)
        return -1;
    else if (fir->size > len)
        return 1;
    return r;
}

ssize_t
fklStringCharBufMatch(const FklString *a, const char *b, size_t b_size) {
    return fklCharBufMatch(a->str, a->size, b, b_size);
}

ssize_t
fklCharBufMatch(const char *a, size_t a_size, const char *b, size_t b_size) {
    if (b_size < a_size)
        return -1;
    if (!memcmp(a, b, a_size))
        return a_size;
    return -1;
}

size_t
fklQuotedStringMatch(const char *cstr, size_t restLen, const FklString *end) {
    if (restLen < end->size)
        return 0;
    size_t matchLen = 0;
    size_t len = 0;
    for (; len < restLen; len++) {
        if (cstr[len] == '\\') {
            len++;
            continue;
        }
        if (fklStringCharBufMatch(end, &cstr[len], restLen - len) >= 0) {
            matchLen += len + end->size;
            return matchLen;
        }
    }
    return 0;
}

size_t fklQuotedCharBufMatch(const char *cstr,
        size_t restLen,
        const char *end,
        size_t end_size) {
    if (restLen < end_size)
        return 0;
    size_t matchLen = 0;
    size_t len = 0;
    for (; len < restLen; len++) {
        if (cstr[len] == '\\') {
            len++;
            continue;
        }
        if (fklCharBufMatch(end, end_size, &cstr[len], restLen - len) >= 0) {
            matchLen += len + end_size;
            return matchLen;
        }
    }
    return 0;
}

FklString *fklLoadString(FILE *fp) {
    uint64_t str_len;
    fread(&str_len, sizeof(str_len), 1, fp);
    FklString *str = fklCreateString(str_len, NULL);
    fread(str->str, str_len, 1, fp);
    return str;
}

FklString *fklCopyString(const FklString *obj) {
    return fklCreateString(obj->size, obj->str);
}

char *fklStringToCstr(const FklString *str) {
    return fklCharBufToCstr(str->str, str->size);
}

FklString *fklCreateStringFromCstr(const char *cStr) {
    return fklCreateString(strlen(cStr), cStr);
}

void fklStringCharBufCat(FklString **a, const char *buf, size_t s) {
    size_t aSize = (*a)->size;
    FklString *prev = *a;
    prev = (FklString *)fklZrealloc(prev,
            sizeof(FklString) + (aSize + s) * sizeof(char));
    FKL_ASSERT(prev);
    *a = prev;
    prev->size = aSize + s;
    memcpy(prev->str + aSize, buf, s);
    prev->str[prev->size] = '\0';
}

void fklStringCat(FklString **fir, const FklString *sec) {
    fklStringCharBufCat(fir, sec->str, sec->size);
}

void fklStringCstrCat(FklString **pfir, const char *sec) {
    fklStringCharBufCat(pfir, sec, strlen(sec));
}

FklString *fklCreateEmptyString() {
    FklString *tmp = (FklString *)fklZcalloc(1, sizeof(FklString));
    FKL_ASSERT(tmp);
    return tmp;
}

void fklPrintRawString(const FklString *str, FILE *fp) {
    fklPrintRawCharBuf((const uint8_t *)str->str,
            str->size,
            "\"",
            "\"",
            '"',
            fp);
}

void fklPrintRawSymbol(const FklString *str, FILE *fp) {
    fklPrintRawCharBuf((const uint8_t *)str->str, str->size, "|", "|", '|', fp);
}

void fklPrintString(const FklString *str, FILE *fp) {
    fwrite(str->str, str->size, 1, fp);
}

int fklIsSpecialCharAndPrintToStringBuffer(FklStringBuffer *s, char ch) {
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

void fklPrintRawCharBufToStringBuffer(struct FklStringBuffer *s,
        size_t size,
        const char *fstr,
        const char *begin_str,
        const char *end_str,
        char se) {
    fklStringBufferConcatWithCstr(s, begin_str);
    uint8_t *str = (uint8_t *)fstr;
    for (size_t i = 0; i < size;) {
        unsigned int l = fklGetByteNumOfUtf8(&str[i], size - i);
        if (l == 7) {
            fklStringBufferPrintf(s, "\\x%02X", (uint8_t)str[i]);
            i++;
        } else if (l == 1) {
            if (str[i] == se) {
                fklStringBufferPutc(s, '\\');
                fklStringBufferPutc(s, se);
            } else if (str[i] == '\\')
                fklStringBufferConcatWithCstr(s, "\\\\");
            else if (isgraph(str[i]))
                fklStringBufferPutc(s, str[i]);
            else if (fklIsSpecialCharAndPrintToStringBuffer(s, str[i]))
                ;
            else
                fklStringBufferPrintf(s, "\\x%02X", (uint8_t)str[i]);
            i++;
        } else {
            fklStringBufferBincpy(s, &str[i], l);
            i += l;
        }
    }
    fklStringBufferConcatWithCstr(s, end_str);
}

FklString *fklStringToRawString(const FklString *str) {
    FklStringBuffer buf = FKL_STRING_BUFFER_INIT;
    fklInitStringBuffer(&buf);
    fklPrintRawStringToStringBuffer(&buf, str, "\"", "\"", '"');
    FklString *retval = fklStringBufferToString(&buf);
    fklUninitStringBuffer(&buf);
    return retval;
}

FklString *fklStringToRawSymbol(const FklString *str) {
    FklStringBuffer buf = FKL_STRING_BUFFER_INIT;
    fklInitStringBuffer(&buf);
    fklPrintRawStringToStringBuffer(&buf, str, "|", "|", '|');
    FklString *retval = fklStringBufferToString(&buf);
    fklUninitStringBuffer(&buf);
    return retval;
}

FklString *fklStringAppend(const FklString *a, const FklString *b) {
    FklString *r = fklCopyString(a);
    fklStringCat(&r, b);
    return r;
}

char *fklCstrStringCat(char *fir, const FklString *sec) {
    if (!fir)
        return fklStringToCstr(sec);
    size_t len = strlen(fir);
    fir = (char *)fklZrealloc(fir, len * sizeof(char) + sec->size + 1);
    FKL_ASSERT(fir);
    fklWriteStringToCstr(&fir[len], sec);
    return fir;
}

void fklWriteStringToCstr(char *c_str, const FklString *str) {
    size_t len = 0;
    const char *buf = str->str;
    size_t size = str->size;
    for (size_t i = 0; i < size; i++) {
        if (!buf[i])
            continue;
        c_str[len] = buf[i];
        len++;
    }
    c_str[len] = '\0';
}

size_t fklCountCharInString(FklString *s, char c) {
    return fklCountCharInBuf(s->str, s->size, c);
}

void fklInitStringBuffer(FklStringBuffer *b) {
    b->index = 0;
    b->size = 0;
    b->buf = NULL;
    fklStringBufferReverse(b, 64);
}

void fklInitStringBufferWithCapacity(FklStringBuffer *b, size_t len) {
    b->index = 0;
    b->size = 0;
    b->buf = NULL;
    fklStringBufferReverse(b, len);
}

void fklStringBufferPutc(FklStringBuffer *b, char c) {
    fklStringBufferReverse(b, 2);
    b->buf[b->index++] = c;
    b->buf[b->index] = '\0';
}

void fklStringBufferBincpy(FklStringBuffer *b, const void *p, size_t l) {
    fklStringBufferReverse(b, l + 1);
    memcpy(&b->buf[b->index], p, l);
    b->index += l;
    b->buf[b->index] = '\0';
}

FklString *fklStringBufferToString(FklStringBuffer *b) {
    return fklCreateString(b->index, b->buf);
}

FklBytevector *fklStringBufferToBytevector(FklStringBuffer *b) {
    return fklCreateBytevector(b->index, (uint8_t *)b->buf);
}

FklStringBuffer *fklCreateStringBuffer(void) {
    FklStringBuffer *r = (FklStringBuffer *)fklZmalloc(sizeof(FklStringBuffer));
    FKL_ASSERT(r);
    fklInitStringBuffer(r);
    return r;
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

void fklStringBufferClear(FklStringBuffer *b) { b->index = 0; }

void fklStringBufferFill(FklStringBuffer *b, char c) {
    memset(b->buf, c, b->index);
}

void fklStringBufferReverse(FklStringBuffer *b, size_t s) {
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
            fklStringBufferReverse(b, n + 1);
        else
            fklStringBufferReverse(b, (b->size) * 2);
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

void fklStringBufferConcatWithCstr(FklStringBuffer *b, const char *s) {
    fklStringBufferBincpy(b, s, strlen(s));
}

void fklStringBufferConcatWithString(FklStringBuffer *b, const FklString *s) {
    fklStringBufferBincpy(b, s->str, s->size);
}

void fklStringBufferConcatWithStringBuffer(FklStringBuffer *a,
        const FklStringBuffer *b) {
    fklStringBufferBincpy(a, b->buf, b->index);
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

FklBytevector *fklCreateBytevector(size_t size, const uint8_t *ptr) {
    FklBytevector *tmp = (FklBytevector *)fklZmalloc(
            sizeof(FklBytevector) + size * sizeof(uint8_t));
    FKL_ASSERT(tmp);
    tmp->size = size;
    if (ptr)
        memcpy(tmp->ptr, ptr, size);
    return tmp;
}

FklBytevector *fklBytevectorRealloc(FklBytevector *bvec, size_t new_size) {
    FklBytevector *new = (FklBytevector *)fklZrealloc(bvec,
            sizeof(FklBytevector) + new_size * sizeof(uint8_t));
    FKL_ASSERT(new);
    return new;
}

void fklBytevectorCat(FklBytevector **a, const FklBytevector *b) {
    size_t aSize = (*a)->size;
    FklBytevector *prev = *a;
    prev = (FklBytevector *)fklZrealloc(prev,
            sizeof(FklBytevector) + (aSize + b->size) * sizeof(char));
    FKL_ASSERT(prev);
    *a = prev;
    prev->size = aSize + b->size;
    memcpy(prev->ptr + aSize, b->ptr, b->size);
}

int fklBytevectorCmp(const FklBytevector *fir, const FklBytevector *sec) {
    size_t size = fir->size < sec->size ? fir->size : sec->size;
    int r = memcmp(fir->ptr, sec->ptr, size);
    if (r)
        return r;
    else if (fir->size < sec->size)
        return -1;
    else if (fir->size > sec->size)
        return 1;
    return r;
}

int fklBytevectorEqual(const FklBytevector *fir, const FklBytevector *sec) {
    if (fir->size == sec->size)
        return !memcmp(fir->ptr, sec->ptr, fir->size);
    return 0;
}

void fklPrintRawBytevector(const FklBytevector *bv, FILE *fp) {
#define SE ('"')
    fputs("#\"", fp);
    const uint8_t *const end = bv->ptr + bv->size;
    for (const uint8_t *c = bv->ptr; c < end; c++) {
        uint8_t ch = *c;
        if (ch == SE) {
            putc('\\', fp);
            putc(SE, fp);
        } else if (ch == '\\')
            fputs("\\\\", fp);
        else if (isgraph(ch))
            putc(ch, fp);
        else if (fklIsSpecialCharAndPrint(ch, fp))
            ;
        else
            fprintf(fp, "\\x%02X", ch);
    }

    fputc('"', fp);
}

void fklPrintBytevectorToStringBuffer(FklStringBuffer *s,
        const FklBytevector *bvec) {
    fklStringBufferConcatWithCstr(s, "#\"");
    const uint8_t *const end = bvec->ptr + bvec->size;
    for (const uint8_t *c = bvec->ptr; c < end; c++) {
        uint8_t ch = *c;
        if (ch == SE) {
            fklStringBufferPutc(s, '\\');
            fklStringBufferPutc(s, SE);
        } else if (ch == '\\')
            fklStringBufferConcatWithCstr(s, "\\\\");
        else if (isgraph(ch))
            fklStringBufferPutc(s, ch);
        else if (fklIsSpecialCharAndPrintToStringBuffer(s, ch))
            ;
        else
            fklStringBufferPrintf(s, "\\x%02X", ch);
    }

    fklStringBufferPutc(s, '"');
#undef SE
}

FklString *fklBytevectorToString(const FklBytevector *bv) {
    FklStringBuffer buf = FKL_STRING_BUFFER_INIT;
    fklInitStringBuffer(&buf);
    fklPrintBytevectorToStringBuffer(&buf, bv);
    FklString *r = fklStringBufferToString(&buf);
    fklUninitStringBuffer(&buf);
    return r;
}

FklBytevector *fklCopyBytevector(const FklBytevector *obj) {
    if (obj == NULL)
        return NULL;
    FklBytevector *tmp =
            (FklBytevector *)fklZmalloc(sizeof(FklBytevector) + obj->size);
    FKL_ASSERT(tmp);
    memcpy(tmp->ptr, obj->ptr, obj->size);
    tmp->size = obj->size;
    return tmp;
}

uintptr_t fklBytevectorHash(const FklBytevector *bv) {
    uintptr_t h = 0;
    const uint8_t *val = bv->ptr;
    size_t size = bv->size;
    for (size_t i = 0; i < size; i++)
        h = 31 * h + val[i];
    return h;
}

static char *string_buffer_alloc(void *ptr, size_t len) {
    FklStringBuffer *buf = ptr;
    fklStringBufferReverse(buf, len + 1);
    buf->index = len;
    char *body = fklStringBufferBody(buf);
    body[len] = '\0';
    return body;
}

size_t fklBigIntToStringBuffer(const FklBigInt *a,
        FklStringBuffer *buf,
        uint8_t radix,
        FklBigIntFmtFlags flags) {
    return fklBigIntToStr(a, string_buffer_alloc, buf, radix, flags);
}

static char *string_alloc_callback(void *ptr, size_t len) {
    FklString **pstr = ptr;
    FklString *str = fklCreateString(len, NULL);
    *pstr = str;
    return str->str;
}

FklString *
fklBigIntToString(const FklBigInt *a, uint8_t radix, FklBigIntFmtFlags flags) {
    FklString *str = NULL;
    fklBigIntToStr(a, string_alloc_callback, &str, radix, flags);
    return str;
}

static const FklBigIntDigit FKL_BIGINT_FIX_MAX_DIGITS[2] = {
    0xffffffff & FKL_BIGINT_DIGIT_MASK,
    0xffffffff & FKL_BIGINT_DIGIT_MASK,
};

static const FklBigIntDigit FKL_BIGINT_FIX_MIN_DIGITS[3] = {
    0x0,
    0x0,
    0x1,
};

static const FklBigInt FKL_BIGINT_FIX_MAX = {
    .digits = (FklBigIntDigit *)FKL_BIGINT_FIX_MAX_DIGITS,
    .num = 2,
    .size = 2,
    .const_size = 1,
};
static const FklBigInt FKL_BIGINT_FIX_MIN = {
    .digits = (FklBigIntDigit *)FKL_BIGINT_FIX_MIN_DIGITS,
    .num = -3,
    .size = 3,
    .const_size = 1,
};

static const FklBigInt FKL_BIGINT_SUB_1_MAX = {
    .digits = (FklBigIntDigit *)FKL_BIGINT_FIX_MIN_DIGITS,
    .num = 3,
    .size = 3,
    .const_size = 1,
};

static const FklBigIntDigit FKL_BIGINT_ADD_1_MIN_DIGITS[3] = {
    0x1,
    0x0,
    0x1,
};

static const FklBigInt FKL_BIGINT_ADD_1_MIN = {
    .digits = (FklBigIntDigit *)FKL_BIGINT_ADD_1_MIN_DIGITS,
    .num = -3,
    .size = 3,
    .const_size = 1,
};

int fklIsBigIntGtLtFix(const FklBigInt *a) {
    return fklBigIntCmp(a, &FKL_BIGINT_FIX_MAX) > 0
        || fklBigIntCmp(a, &FKL_BIGINT_FIX_MIN) < 0;
}

int fklIsBigIntAddkInFixIntRange(const FklBigInt *a, int8_t k) {
    if (k == 1)
        return fklBigIntEqual(a, &FKL_BIGINT_ADD_1_MIN);
    else if (k == -1)
        return fklBigIntEqual(a, &FKL_BIGINT_SUB_1_MAX);
    else if (!fklIsBigIntGtLtI64(a)) {
        int64_t i = fklBigIntToI(a);
        if (i > 0 && k <= 0)
            return i + k <= FKL_FIX_INT_MAX;
        else if (i < 0 && k >= 0)
            return i + k >= FKL_FIX_INT_MIN;
        else if (i < 0)
            return i >= FKL_FIX_INT_MIN && i + k >= FKL_FIX_INT_MIN;
        else if (i > 0)
            return i <= FKL_FIX_INT_MAX && i + k <= FKL_FIX_INT_MAX;
        else
            return 1;
    } else
        return 0;
}
