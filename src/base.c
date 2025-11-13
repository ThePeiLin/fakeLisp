#include "fakeLisp/code_builder.h"
#include <fakeLisp/base.h>
#include <fakeLisp/bigint.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/str_buf.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/zmalloc.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
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

void fklWriteString(const FklString *s, FILE *fp) {
    fwrite(&s->size, sizeof(s->size), 1, fp);
    fwrite(s->str, s->size * sizeof(*(s->str)), 1, fp);
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

void fklPrintStringLiteral(const FklString *str, FILE *fp) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrintStringLiteral2(str, &builder);
}

void fklPrintSymbolLiteral(const FklString *str, FILE *fp) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrintSymbolLiteral2(str, &builder);
}

void fklPrintString(const FklString *str, FILE *fp) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrintString2(str, &builder);
}

void fklPrintSymLiteral(const char *str, FILE *fp) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrintSymLiteral2(str, &builder);
}

void fklPrintStrLiteral(const char *str, FILE *fp) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrintStrLiteral2(str, &builder);
}

void fklPrintBufLiteralExt(size_t size,
        const char *str,
        const char *begin_str,
        const char *end_str,
        char se,
        FklCodeBuilder *b) {
    fklCodeBuilderPuts(b, begin_str);
    for (size_t i = 0; i < size;) {
        unsigned int l =
                fklGetByteNumOfUtf8((const uint8_t *)&str[i], size - i);
        if (l == 7) {
            fklCodeBuilderFmt(b, "\\x%02X", (uint8_t)str[i]);
            i++;
        } else if (l == 1) {
            if (str[i] == se) {
                fklCodeBuilderPutc(b, '\\');
                fklCodeBuilderPutc(b, se);
            } else if (str[i] == '\\')
                fklCodeBuilderPuts(b, "\\\\");
            else if (isgraph(str[i]))
                fklCodeBuilderPutc(b, str[i]);
            else if (fklCodeBuilderPutEscSeq(b, str[i]))
                ;
            else
                fklCodeBuilderFmt(b, "\\x%02X", (uint8_t)str[i]);
            i++;
        } else {
            fklCodeBuilderWrite(b, l, &str[i]);
            i += l;
        }
    }
    fklCodeBuilderPuts(b, end_str);
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

FklBytevector *fklStringBufferToBytevector(FklStringBuffer *b) {
    return fklCreateBytevector(b->index, (uint8_t *)b->buf);
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

void fklPrintBytesLiteral(const FklBytevector *bv, FILE *fp) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, fp, NULL);
    fklPrintBytesLiteral2(bv, &builder);
}

void fklPrintBytesLiteral2(const FklBytevector *bvec, FklCodeBuilder *b) {
#define SE ('"')
    fklCodeBuilderPuts(b, "#\"");
    const uint8_t *const end = bvec->ptr + bvec->size;
    for (const uint8_t *c = bvec->ptr; c < end; c++) {
        uint8_t ch = *c;
        if (ch == SE) {
            fklCodeBuilderPutc(b, '\\');
            fklCodeBuilderPutc(b, SE);
        } else if (ch == '\\')
            fklCodeBuilderPuts(b, "\\\\");
        else if (isgraph(ch))
            fklCodeBuilderPutc(b, ch);
        else if (fklCodeBuilderPutEscSeq(b, ch))
            ;
        else
            fklCodeBuilderFmt(b, "\\x%02X", ch);
    }

    fklCodeBuilderPutc(b, '"');
#undef SE
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

FklBytevector *fklLoadBytevector(FILE *fp) {
    uint64_t len;
    fread(&len, sizeof(len), 1, fp);
    FklBytevector *b = fklCreateBytevector(len, NULL);
    fread(b->ptr, len, 1, fp);
    return b;
}

void fklWriteBytevector(const FklBytevector *b, FILE *fp) {
    fwrite(&b->size, sizeof(b->size), 1, fp);
    fwrite(b->ptr, b->size, 1, fp);
}

static char *string_buffer_alloc(void *ptr, size_t len) {
    FklStringBuffer *buf = ptr;
    fklStringBufferReserve(buf, len + 1);
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

static int fp_cb_printf(void *ctx, const char *fmt, va_list ap) {
    FILE *fp = FKL_TYPE_CAST(FILE *, ctx);
    return vfprintf(fp, fmt, ap);
}

static int fp_cb_puts(void *ctx, const char *fmt) {
    FILE *fp = FKL_TYPE_CAST(FILE *, ctx);
    return fputs(fmt, fp);
}

static int fp_cb_putc(void *ctx, char c) {
    FILE *fp = FKL_TYPE_CAST(FILE *, ctx);
    return fputc(c, fp);
}

static size_t fp_cb_write(void *ctx, size_t len, const void *s) {
    FILE *fp = FKL_TYPE_CAST(FILE *, ctx);
    return fwrite(s, 1, len, fp);
}

static const FklCodeBuilderMethodTable fp_cb_method_table = {
    .cb_printf = fp_cb_printf,
    .cb_puts = fp_cb_puts,
    .cb_putc = fp_cb_putc,
    .cb_write = fp_cb_write,
};

void fklInitCodeBuilderFp(FklCodeBuilder *b, FILE *fp, const char *indent_str) {
    fklInitCodeBuilder(b, fp, &fp_cb_method_table, indent_str);
}
