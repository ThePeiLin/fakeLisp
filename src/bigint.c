#include <fakeLisp/base.h>
#include <fakeLisp/bigint.h>
#include <fakeLisp/common.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/zmalloc.h>

#include <ctype.h>
#include <limits.h>
#include <string.h>

// steal from cpython: https://github.com/python/cpython
#define FKL_BIGINT_DECIMAL_SHIFT (9)
#define FKL_BIGINT_DECIMAL_BASE ((FklBigIntDigit)1000000000)

void fklInitBigInt0(FklBigInt *b) { *b = FKL_BIGINT_0; }

void fklInitBigInt1(FklBigInt *b) {
    *b = FKL_BIGINT_0;
    b->num = 1;
    b->size = 1;
    b->digits = (FklBigIntDigit *)fklZmalloc(b->size * sizeof(FklBigIntDigit));
    FKL_ASSERT(b->digits);
    b->digits[0] = 1;
}

void fklInitBigIntN1(FklBigInt *b) {
    fklInitBigInt1(b);
    b->num = -1;
}

void fklInitBigInt(FklBigInt *a, const FklBigInt *b) {
    *a = FKL_BIGINT_0;
    fklSetBigInt(a, b);
}

static inline size_t count_digits_size(uint64_t i) {
    if (i) {
        size_t count = 0;
        for (; i; i >>= FKL_BIGINT_DIGIT_SHIFT)
            ++count;
        return count;
    }
    return 0;
}

static inline void set_uint64_to_digits(FklBigIntDigit *digits, size_t size,
                                        uint64_t num) {
    for (size_t i = 0; i < size; i++) {
        digits[i] = num & FKL_BIGINT_DIGIT_MASK;
        num >>= FKL_BIGINT_DIGIT_SHIFT;
    }
}

void fklInitBigIntU(FklBigInt *b, uint64_t num) {
    *b = FKL_BIGINT_0;
    if (num) {
        b->size = count_digits_size(num);
        b->num = b->size;
        b->digits =
            (FklBigIntDigit *)fklZmalloc(b->size * sizeof(FklBigIntDigit));
        FKL_ASSERT(b->digits);
        set_uint64_to_digits(b->digits, b->size, num);
    }
}

void fklInitBigIntI(FklBigInt *b, int64_t n) {
    fklInitBigIntU(b, fklAbs(n));
    if (n < 0)
        b->num = -b->num;
}

void fklInitBigIntD(FklBigInt *b, double d) { fklInitBigIntI(b, (int64_t)d); }

void fklInitBigIntFromMem(FklBigInt *t, int64_t num, FklBigIntDigit *mem) {
    t->num = num;
    t->size = fklAbs(num);
    t->digits = mem;
}

// also steal from cpython: cpython: https://github.com/python/cpython
unsigned char DigitValue[256] = {
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 0,  1,  2,  3,  4,  5,  6,  7,  8,
    9,  37, 37, 37, 37, 37, 37, 37, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 37, 37, 37,
    37, 37, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
    27, 28, 29, 30, 31, 32, 33, 34, 35, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
    37, 37, 37, 37, 37, 37, 37, 37, 37,
};

void fklInitBigIntWithDecCharBuf(FklBigInt *b, const char *buf, size_t len) {
    *b = FKL_BIGINT_0;
    int neg = buf[0] == '-';
    size_t offset = neg || buf[0] == '+';
    for (size_t i = offset; i < len && isdigit(buf[i]); i++) {
        fklMulBigIntI(b, 10);
        fklAddBigIntI(b, DigitValue[(unsigned char)buf[i]]);
    }
    if (neg)
        b->num = -b->num;
}

static void ensure_bigint_size(FklBigInt *to, uint64_t size) {
    if (to->size < size) {
        FKL_ASSERT(!to->const_size);
        to->size = size;
        to->digits = (FklBigIntDigit *)fklZrealloc(
            to->digits, size * sizeof(FklBigIntDigit));
        FKL_ASSERT(to->digits);
    }
}

static inline void bigint_normalize(FklBigInt *a) {
    int64_t i = fklAbs(a->num);
    for (; i > 0 && a->digits[i - 1] == 0; --i)
        ;
    a->num = a->num < 0 ? -i : i;
}

#define OCT_BIT_COUNT (3)
#define DIGIT_OCT_COUNT (FKL_BIGINT_DIGIT_SHIFT / OCT_BIT_COUNT)

void fklInitBigIntWithOctCharBuf(FklBigInt *b, const char *buf, size_t len) {
    *b = FKL_BIGINT_0;
    int neg = buf[0] == '-';
    size_t offset = (neg || buf[0] == '+') + 1;
    for (; offset < len && buf[offset] == '0'; offset++)
        ;
    size_t actual_len = len - offset;
    size_t high_len = actual_len % DIGIT_OCT_COUNT;
    size_t total_len = actual_len / DIGIT_OCT_COUNT + (high_len > 0);
    ensure_bigint_size(b, total_len);
    b->num = total_len;
    if (total_len == 0)
        return;
    FklBigIntDigit *pdigits = b->digits;
    size_t i = offset;
    uint32_t carry = 0;
    for (; i < offset + high_len; i++) {
        carry <<= OCT_BIT_COUNT;
        carry |= DigitValue[(unsigned char)buf[i]];
    }
    pdigits[--total_len] = carry;
    for (; i < len; i += DIGIT_OCT_COUNT) {
        carry = 0;
        for (size_t j = 0; j < DIGIT_OCT_COUNT; j++) {
            carry <<= OCT_BIT_COUNT;
            carry |= DigitValue[(unsigned char)buf[i + j]];
        }
        pdigits[--total_len] = carry;
    }
    bigint_normalize(b);
    if (neg)
        b->num = -b->num;
}

#define HEX_BIT_COUNT (4)
#define U32_HEX_COUNT ((sizeof(uint32_t) * CHAR_BIT) / HEX_BIT_COUNT)

void fklInitBigIntWithHexCharBuf(FklBigInt *b, const char *buf, size_t len) {
    *b = FKL_BIGINT_0;
    int neg = buf[0] == '-';
    size_t offset = (neg || buf[0] == '+') + 2;
    for (; offset < len && buf[offset] == '0'; offset++)
        ;
    const char *start = buf + offset;
    int64_t digits_count =
        ((len - offset) * HEX_BIT_COUNT + FKL_BIGINT_DIGIT_SHIFT - 1)
        / FKL_BIGINT_DIGIT_SHIFT;
    ensure_bigint_size(b, digits_count);

    int bits_in_accum = 0;
    FklBigIntTwoDigit accum = 0;
    FklBigIntDigit *pdigits = b->digits;
    const char *p = buf + len;
    while (--p >= start) {
        accum |= (FklBigIntTwoDigit)DigitValue[(unsigned char)*p]
              << bits_in_accum;
        bits_in_accum += HEX_BIT_COUNT;
        if (bits_in_accum >= FKL_BIGINT_DIGIT_SHIFT) {
            *pdigits++ = (FklBigIntDigit)(accum & FKL_BIGINT_DIGIT_MASK);
            accum >>= FKL_BIGINT_DIGIT_SHIFT;
            bits_in_accum -= FKL_BIGINT_DIGIT_SHIFT;
        }
    }
    if (bits_in_accum)
        *pdigits++ = (FklBigIntDigit)accum;
    while (pdigits - b->digits < digits_count)
        *pdigits++ = 0;
    b->num = digits_count;
    bigint_normalize(b);
    if (neg)
        b->num = -b->num;
}

void fklInitBigIntWithCharBuf(FklBigInt *b, const char *buf, size_t len) {
    if (fklIsHexInt(buf, len))
        fklInitBigIntWithHexCharBuf(b, buf, len);
    else if (fklIsOctInt(buf, len))
        fklInitBigIntWithOctCharBuf(b, buf, len);
    else
        fklInitBigIntWithDecCharBuf(b, buf, len);
}

void fklInitBigIntWithCstr(FklBigInt *b, const char *cstr) {
    fklInitBigIntWithCharBuf(b, cstr, strlen(cstr));
}

static inline void bigint_normalize2(int64_t *num, FklBigIntDigit *digits) {
    int64_t i = fklAbs(*num);
    for (; i > 0 && digits[i - 1] == 0; --i)
        ;
    *num = *num < 0 ? -i : i;
}

void fklInitBigIntWithCharBuf2(void *ctx,
                               const FklBigIntInitWithCharBufMethodTable *table,
                               const char *buf, size_t len) {
    if (fklIsHexInt(buf, len))
        fklInitBigIntWithHexCharBuf2(ctx, table, buf, len);
    else if (fklIsOctInt(buf, len))
        fklInitBigIntWithOctCharBuf2(ctx, table, buf, len);
    else
        fklInitBigIntWithDecCharBuf2(ctx, table, buf, len);
}

void fklInitBigIntWithOctCharBuf2(
    void *ctx, const FklBigIntInitWithCharBufMethodTable *table,
    const char *buf, size_t len) {
    int neg = buf[0] == '-';
    size_t offset = (neg || buf[0] == '+') + 1;
    for (; offset < len && buf[offset] == '0'; offset++)
        ;
    size_t actual_len = len - offset;
    size_t high_len = actual_len % DIGIT_OCT_COUNT;
    size_t total_len = actual_len / DIGIT_OCT_COUNT + (high_len > 0);
    FklBigIntDigit *bdigits = table->alloc(ctx, total_len);
    int64_t *bnum = table->num(ctx);
    *bnum = total_len;
    if (total_len == 0)
        return;
    FklBigIntDigit *pdigits = bdigits;
    size_t i = offset;
    uint32_t carry = 0;
    for (; i < offset + high_len; i++) {
        carry <<= OCT_BIT_COUNT;
        carry |= DigitValue[(unsigned char)buf[i]];
    }
    pdigits[--total_len] = carry;
    for (; i < len; i += DIGIT_OCT_COUNT) {
        carry = 0;
        for (size_t j = 0; j < DIGIT_OCT_COUNT; j++) {
            carry <<= OCT_BIT_COUNT;
            carry |= DigitValue[(unsigned char)buf[i + j]];
        }
        pdigits[--total_len] = carry;
    }
    bigint_normalize2(bnum, bdigits);
    if (neg)
        *bnum = -*bnum;
}

void fklInitBigIntWithHexCharBuf2(
    void *ctx, const FklBigIntInitWithCharBufMethodTable *table,
    const char *buf, size_t len) {
    int neg = buf[0] == '-';
    size_t offset = (neg || buf[0] == '+') + 2;
    for (; offset < len && buf[offset] == '0'; offset++)
        ;
    const char *start = buf + offset;
    int64_t digits_count =
        ((len - offset) * HEX_BIT_COUNT + FKL_BIGINT_DIGIT_SHIFT - 1)
        / FKL_BIGINT_DIGIT_SHIFT;
    FklBigIntDigit *bdigits = table->alloc(ctx, digits_count);

    int bits_in_accum = 0;
    FklBigIntTwoDigit accum = 0;
    FklBigIntDigit *pdigits = bdigits;
    const char *p = buf + len;
    while (--p >= start) {
        accum |= (FklBigIntTwoDigit)DigitValue[(unsigned char)*p]
              << bits_in_accum;
        bits_in_accum += HEX_BIT_COUNT;
        if (bits_in_accum >= FKL_BIGINT_DIGIT_SHIFT) {
            *pdigits++ = (FklBigIntDigit)(accum & FKL_BIGINT_DIGIT_MASK);
            accum >>= FKL_BIGINT_DIGIT_SHIFT;
            bits_in_accum -= FKL_BIGINT_DIGIT_SHIFT;
        }
    }
    if (bits_in_accum)
        *pdigits++ = (FklBigIntDigit)accum;
    while (pdigits - bdigits < digits_count)
        *pdigits++ = 0;
    int64_t *bnum = table->num(ctx);
    *bnum = digits_count;
    bigint_normalize2(bnum, bdigits);
    if (neg)
        *bnum = -*bnum;
}

void fklInitBigIntWithDecCharBuf2(
    void *ctx, const FklBigIntInitWithCharBufMethodTable *table,
    const char *buf, size_t len) {
    FklBigInt b;
    fklInitBigIntWithDecCharBuf(&b, buf, len);
    FklBigIntDigit *digits = table->alloc(ctx, b.size);
    FklBigInt to = {
        .digits = digits,
        .num = 0,
        .size = b.size,
        .const_size = 1,
    };
    fklSetBigInt(&to, &b);
    *(table->num(ctx)) = to.num;
    fklUninitBigInt(&b);
}

void fklUninitBigInt(FklBigInt *b) {
    fklZfree(b->digits);
    *b = FKL_BIGINT_0;
}

FklBigInt *fklCreateBigInt0(void) {
    FklBigInt *b = (FklBigInt *)fklZmalloc(sizeof(FklBigInt));
    FKL_ASSERT(b);
    *b = FKL_BIGINT_0;
    return b;
}

FklBigInt *fklCreateBigInt1(void) {
    FklBigInt *b = fklCreateBigInt0();
    fklInitBigInt1(b);
    return b;
}

FklBigInt *fklCreateBigIntN1(void) {
    FklBigInt *b = fklCreateBigInt0();
    fklInitBigIntN1(b);
    return b;
}

FklBigInt *fklCreateBigIntI(int64_t v) {
    FklBigInt *b = fklCreateBigInt0();
    fklInitBigIntI(b, v);
    return b;
}

FklBigInt *fklCreateBigIntD(double v) { return fklCreateBigIntI((int64_t)v); }

FklBigInt *fklCreateBigIntU(uint64_t v) {
    FklBigInt *b = fklCreateBigInt0();
    fklInitBigIntU(b, v);
    return b;
}

FklBigInt *fklCreateBigIntWithCharBuf(const char *buf, size_t len) {
    FklBigInt *b = fklCreateBigInt0();
    fklInitBigIntWithCharBuf(b, buf, len);
    return b;
}

FklBigInt *fklCreateBigIntWithDecCharBuf(const char *buf, size_t len) {
    FklBigInt *b = fklCreateBigInt0();
    fklInitBigIntWithDecCharBuf(b, buf, len);
    return b;
}

FklBigInt *fklCreateBigIntWithHexCharBuf(const char *buf, size_t len) {
    FklBigInt *b = fklCreateBigInt0();
    fklInitBigIntWithHexCharBuf(b, buf, len);
    return b;
}

FklBigInt *fklCreateBigIntWithOctCharBuf(const char *buf, size_t len) {
    FklBigInt *b = fklCreateBigInt0();
    fklInitBigIntWithOctCharBuf(b, buf, len);
    return b;
}

FklBigInt *fklCreateBigIntWithCstr(const char *cstr) {
    FklBigInt *b = fklCreateBigInt0();
    fklInitBigIntWithCstr(b, cstr);
    return b;
}

void fklDestroyBigInt(FklBigInt *b) {
    fklUninitBigInt(b);
    fklZfree(b);
}

FklBigInt *fklCopyBigInt(const FklBigInt *b) {
    FklBigInt *r = fklCreateBigInt0();
    fklSetBigInt(r, b);
    return r;
}

int fklBigIntEqual(const FklBigInt *a, const FklBigInt *b) {
    if (a->num == b->num)
        return memcmp(a->digits, b->digits,
                      fklAbs(a->num) * sizeof(FklBigIntDigit))
            == 0;
    else
        return 0;
}

static inline int x_cmp(const FklBigInt *a, const FklBigInt *b) {
    int64_t i = fklAbs(a->num);
    while (--i >= 0 && a->digits[i] == b->digits[i])
        ;
    return i < 0
             ? 0
             : (FklBigIntSDigit)a->digits[i] - (FklBigIntSDigit)b->digits[i];
}

int fklBigIntCmp(const FklBigInt *a, const FklBigInt *b) {
    int sign;
    if (a->num != b->num)
        sign = a->num - b->num;
    else {
        sign = x_cmp(a, b);
        if (a->num < 0)
            sign = -sign;
    }

    return sign < 0 ? -1 : sign > 0 ? 1 : 0;
}

static inline void set_bigint_i64_without_ensure(FklBigInt *to,
                                                 int64_t const v) {
    uint64_t uv = fklAbs(v);
    if (uv) {
        size_t c = count_digits_size(uv);
        to->num = c;
        set_uint64_to_digits(to->digits, c, uv);
    } else
        to->num = 0;
    if (v < 0)
        to->num = -to->num;
}

int fklBigIntCmpI(const FklBigInt *a, int64_t b) {
    FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
    FklBigInt bi = FKL_BIGINT_0;
    bi.size = FKL_MAX_INT64_DIGITS_COUNT;
    bi.digits = digits;
    bi.const_size = 1;
    set_bigint_i64_without_ensure(&bi, b);
    return fklBigIntCmp(a, &bi);
}

int fklBigIntAbsCmp(const FklBigInt *a, const FklBigInt *b) {
    int64_t num_a = fklAbs(a->num);
    int64_t num_b = fklAbs(b->num);
    int sign = num_a != num_b ? num_a - num_b : x_cmp(a, b);

    return sign < 0 ? -1 : sign > 0 ? 1 : 0;
}

int fklIsBigIntOdd(const FklBigInt *b) {
    return b->num != 0 && b->digits[0] % 2;
}

int fklIsBigIntEven(const FklBigInt *b) { return !fklIsBigIntOdd(b); }

int fklIsBigIntLe0(const FklBigInt *b) { return b->num <= 0; }

int fklIsBigIntLt0(const FklBigInt *b) { return b->num < 0; }

int fklIsDivisibleBigInt(const FklBigInt *a, const FklBigInt *d) {
    int r = 0;
    FklBigInt rem = FKL_BIGINT_0;
    fklSetBigInt(&rem, a);
    fklRemBigInt(&rem, d);
    if (rem.num == 0)
        r = 1;
    fklUninitBigInt(&rem);
    return r;
}

int fklIsDivisibleBigIntI(const FklBigInt *a, int64_t d) {
    FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
    FklBigInt bi = FKL_BIGINT_0;
    bi.size = FKL_MAX_INT64_DIGITS_COUNT;
    bi.digits = digits;
    bi.const_size = 1;
    set_bigint_i64_without_ensure(&bi, d);
    return fklIsDivisibleBigInt(a, &bi);
}

void fklSetBigInt(FklBigInt *to, const FklBigInt *from) {
    ensure_bigint_size(to, fklAbs(from->num));
    to->num = from->num;
    memcpy(to->digits, from->digits, fklAbs(to->num) * sizeof(FklBigIntDigit));
}

void fklSetBigIntU(FklBigInt *to, uint64_t v) {
    if (v) {
        size_t c = count_digits_size(v);
        to->num = c;
        ensure_bigint_size(to, c);
        set_uint64_to_digits(to->digits, c, v);
    } else
        to->num = 0;
}

void fklSetBigIntI(FklBigInt *to, int64_t v) {
    fklSetBigIntU(to, fklAbs(v));
    if (v < 0)
        to->num = -to->num;
}

void fklSetBigIntD(FklBigInt *to, double from) {
    fklSetBigIntI(to, (int64_t)from);
}

static inline void x_add(FklBigInt *a, const FklBigInt *b) {
    uint64_t num_a = fklAbs(a->num);
    uint64_t num_b = fklAbs(b->num);
    size_t i = 0;
    FklBigIntDigit carry = 0;
    const FklBigIntDigit *digits_a;
    const FklBigIntDigit *digits_b;
    if (num_a < num_b) {
        uint64_t num_tmp = num_a;
        num_a = num_b;
        num_b = num_tmp;
        ensure_bigint_size(a, num_a + 1);
        digits_a = a->digits;
        digits_b = b->digits;

        const FklBigIntDigit *digits_tmp = digits_a;
        digits_a = digits_b;
        digits_b = digits_tmp;
    } else {
        ensure_bigint_size(a, num_a + 1);
        digits_a = a->digits;
        digits_b = b->digits;
    }
    for (; i < num_b; ++i) {
        carry += digits_a[i] + digits_b[i];
        a->digits[i] = carry & FKL_BIGINT_DIGIT_MASK;
        carry >>= FKL_BIGINT_DIGIT_SHIFT;
    }
    for (; i < num_a; ++i) {
        carry += digits_a[i];
        a->digits[i] = carry & FKL_BIGINT_DIGIT_MASK;
        carry >>= FKL_BIGINT_DIGIT_SHIFT;
    }

    a->num = num_a + 1;
    a->digits[i] = carry & FKL_BIGINT_DIGIT_MASK;
    bigint_normalize(a);
}

static inline void x_sub(FklBigInt *a, const FklBigInt *b) {
    int64_t num_a = fklAbs(a->num);
    int64_t num_b = fklAbs(b->num);
    int sign = 1;
    ssize_t i = 0;
    FklBigIntDigit borrow = 0;
    const FklBigIntDigit *digits_a;
    const FklBigIntDigit *digits_b;
    if (num_a < num_b) {
        sign = -1;
        uint64_t num_tmp = num_a;
        num_a = num_b;
        num_b = num_tmp;
        ensure_bigint_size(a, num_a + 1);
        digits_a = a->digits;
        digits_b = b->digits;

        const FklBigIntDigit *digits_tmp = digits_a;
        digits_a = digits_b;
        digits_b = digits_tmp;
    } else {
        ensure_bigint_size(a, num_a + 1);
        digits_a = a->digits;
        digits_b = b->digits;
        if (num_a == num_b) {
            i = num_a;
            while (--i >= 0 && a->digits[i] == b->digits[i])
                ;
            if (i < 0) {
                a->num = 0;
                return;
            }
            if (a->digits[i] < b->digits[i]) {
                sign = -1;
                const FklBigIntDigit *digits_tmp = digits_a;
                digits_a = digits_b;
                digits_b = digits_tmp;
            }
            num_a = num_b = i + 1;
        }
    }

    for (i = 0; i < num_b; ++i) {
        borrow = digits_a[i] - digits_b[i] - borrow;
        a->digits[i] = borrow & FKL_BIGINT_DIGIT_MASK;
        borrow >>= FKL_BIGINT_DIGIT_SHIFT;
        borrow &= 1;
    }
    for (; i < num_a; ++i) {
        borrow = digits_a[i] - borrow;
        a->digits[i] = borrow & FKL_BIGINT_DIGIT_MASK;
        borrow >>= FKL_BIGINT_DIGIT_SHIFT;
        borrow &= 1;
    }
    FKL_ASSERT(borrow == 0);
    if (sign < 0)
        a->num = a->num < 0 ? num_a : -num_a;
    bigint_normalize(a);
}

static inline void x_sub2(const FklBigInt *a, FklBigInt *b) {
    int64_t num_a = fklAbs(a->num);
    int64_t num_b = fklAbs(b->num);
    int sign = 1;
    ssize_t i = 0;
    FklBigIntDigit borrow = 0;
    const FklBigIntDigit *digits_a;
    const FklBigIntDigit *digits_b;
    if (num_a < num_b) {
        sign = -1;
        uint64_t num_tmp = num_a;
        num_a = num_b;
        num_b = num_tmp;
        ensure_bigint_size(b, num_a + 1);
        digits_a = a->digits;
        digits_b = b->digits;

        const FklBigIntDigit *digits_tmp = digits_a;
        digits_a = digits_b;
        digits_b = digits_tmp;
    } else {
        ensure_bigint_size(b, num_a + 1);
        digits_a = a->digits;
        digits_b = b->digits;
        if (num_a == num_b) {
            i = num_a;
            while (--i >= 0 && a->digits[i] == b->digits[i])
                ;
            if (i < 0) {
                b->num = 0;
                return;
            }
            if (a->digits[i] < b->digits[i]) {
                sign = -1;
                const FklBigIntDigit *digits_tmp = digits_a;
                digits_a = digits_b;
                digits_b = digits_tmp;
            }
            num_a = num_b = i + 1;
        }
    }

    for (i = 0; i < num_b; ++i) {
        borrow = digits_a[i] - digits_b[i] - borrow;
        b->digits[i] = borrow & FKL_BIGINT_DIGIT_MASK;
        borrow >>= FKL_BIGINT_DIGIT_SHIFT;
        borrow &= 1;
    }
    for (; i < num_a; ++i) {
        borrow = digits_a[i] - borrow;
        b->digits[i] = borrow & FKL_BIGINT_DIGIT_MASK;
        borrow >>= FKL_BIGINT_DIGIT_SHIFT;
        borrow &= 1;
    }
    FKL_ASSERT(borrow == 0);
    b->num = num_a * sign;
    bigint_normalize(b);
}

#define MEDIUM_VALUE(I)                                                        \
    ((int64_t)((I)->num == 0  ? 0                                              \
               : (I)->num < 0 ? -((FklBigIntSDigit)(I)->digits[0])             \
                              : (FklBigIntSDigit)(I)->digits[0]))

void fklAddBigInt(FklBigInt *a, const FklBigInt *b) {
    if (fklAbs(a->num) <= 1 && fklAbs(b->num) <= 1) {
        fklSetBigIntI(a, MEDIUM_VALUE(a) + MEDIUM_VALUE(b));
        return;
    }
    if (a->num < 0) {
        if (b->num < 0) {
            x_add(a, b);
            a->num = -a->num;
        } else
            x_sub2(b, a);
    } else if (b->num < 0)
        x_sub(a, b);
    else
        x_add(a, b);
}

void fklAddBigIntI(FklBigInt *b, int64_t v) {
    FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
    FklBigInt bi = FKL_BIGINT_0;
    bi.size = FKL_MAX_INT64_DIGITS_COUNT;
    bi.digits = digits;
    bi.const_size = 1;
    set_bigint_i64_without_ensure(&bi, v);
    fklAddBigInt(b, &bi);
}

void fklSubBigInt(FklBigInt *a, const FklBigInt *b) {
    if (fklAbs(a->num) <= 1 && fklAbs(b->num) <= 1) {
        fklSetBigIntI(a, MEDIUM_VALUE(a) - MEDIUM_VALUE(b));
        return;
    }
    if (a->num < 0) {
        if (b->num < 0)
            x_sub(a, b);
        else {
            x_add(a, b);
            a->num = -a->num;
        }
    } else if (b->num < 0)
        x_add(a, b);
    else
        x_sub(a, b);
}

void fklSubBigIntI(FklBigInt *a, int64_t v) {
    FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
    FklBigInt bi = FKL_BIGINT_0;
    bi.size = FKL_MAX_INT64_DIGITS_COUNT;
    bi.digits = digits;
    bi.const_size = 1;
    set_bigint_i64_without_ensure(&bi, v);
    fklSubBigInt(a, &bi);
}

#define SAME_SIGN(a, b)                                                        \
    (((a)->num >= 0 && (b)->num >= 0) || ((a)->num < 0 && (b)->num < 0))

// XXX: 这个乘法的实现不是很高效
void fklMulBigInt(FklBigInt *a, const FklBigInt *b) {
    if (FKL_BIGINT_IS_0(a) || FKL_BIGINT_IS_1(b))
        return;
    else if (FKL_BIGINT_IS_0(b))
        a->num = 0;
    else if (FKL_BIGINT_IS_N1(b))
        a->num = -a->num;
    else if (FKL_BIGINT_IS_1(a))
        fklSetBigInt(a, b);
    else if (FKL_BIGINT_IS_N1(a)) {
        fklSetBigInt(a, b);
        a->num = -a->num;
    } else {
        int64_t num_a = fklAbs(a->num);
        int64_t num_b = fklAbs(b->num);
        int64_t total = num_a + num_b;
        FklBigIntDigit *result =
            (FklBigIntDigit *)fklZcalloc(total, sizeof(FklBigIntDigit));
        FKL_ASSERT(result);
        for (int64_t i = 0; i < num_a; i++) {
            FklBigIntTwoDigit na = a->digits[i];
            FklBigIntTwoDigit carry = 0;
            for (int64_t j = 0; j < num_b; j++) {
                FklBigIntTwoDigit nb = b->digits[j];
                carry += result[i + j] + na * nb;
                result[i + j] = carry & FKL_BIGINT_DIGIT_MASK;
                carry >>= FKL_BIGINT_DIGIT_SHIFT;
            }
            result[i + num_b] += carry;
        }
        fklZfree(a->digits);
        a->digits = result;
        a->size = total;
        a->num = SAME_SIGN(a, b) ? total : -total;
        bigint_normalize(a);
    }
}

void fklMulBigIntI(FklBigInt *b, int64_t v) {
    FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
    FklBigInt bi = FKL_BIGINT_0;
    bi.size = FKL_MAX_INT64_DIGITS_COUNT;
    bi.digits = digits;
    bi.const_size = 1;
    set_bigint_i64_without_ensure(&bi, v);
    fklMulBigInt(b, &bi);
}

int fklDivRemBigIntI(FklBigInt *b, int64_t divider, FklBigInt *rem) {
    FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
    FklBigInt bi = FKL_BIGINT_0;
    bi.size = FKL_MAX_INT64_DIGITS_COUNT;
    bi.digits = digits;
    bi.const_size = 1;
    set_bigint_i64_without_ensure(&bi, divider);
    return fklDivRemBigInt(b, &bi, rem);
}

static inline void divrem1(FklBigIntDigit *pin, int64_t size, FklBigIntDigit d,
                           FklBigInt *rem) {
    FKL_ASSERT(size > 0 && d <= FKL_BIGINT_DIGIT_MASK);
    FklBigIntDigit rem1 = 0;
    while (--size >= 0) {
        FklBigIntTwoDigit dividend =
            ((FklBigIntTwoDigit)rem1 << FKL_BIGINT_DIGIT_SHIFT) | pin[size];
        FklBigIntDigit quotient = (FklBigIntDigit)(dividend / d);
        rem1 = dividend % d;
        pin[size] = quotient;
    }
    if (rem)
        fklSetBigIntI(rem, rem1);
}

static inline uint8_t bit_length_digit(uint64_t x) {
    static const int BIT_LENGTH_TABLE[32] = {
        0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    };
    int msb = 0;
    while (x >= 32) {
        msb += 6;
        x >>= 6;
    }
    msb += BIT_LENGTH_TABLE[x];
    return msb;
}

static inline FklBigIntDigit
v_lshift(FklBigIntDigit *z, const FklBigIntDigit *a, int64_t m, int d) {
    FklBigIntDigit carry = 0;
    FKL_ASSERT(0 <= d && d < FKL_BIGINT_DIGIT_SHIFT);
    for (int64_t i = 0; i < m; i++) {
        FklBigIntTwoDigit acc = (FklBigIntTwoDigit)a[i] << d | carry;
        z[i] = (FklBigIntDigit)acc & FKL_BIGINT_DIGIT_MASK;
        carry = (FklBigIntDigit)(acc >> FKL_BIGINT_DIGIT_SHIFT);
    }
    return carry;
}

static inline FklBigIntDigit
v_rshift(FklBigIntDigit *z, const FklBigIntDigit *a, int64_t m, int d) {
    FklBigIntDigit carry = 0;
    FklBigIntDigit mask = ((FklBigIntDigit)1 << d) - 1U;
    FKL_ASSERT(0 <= d && d < FKL_BIGINT_DIGIT_SHIFT);
    for (int64_t i = m; i-- > 0;) {
        FklBigIntTwoDigit acc =
            (FklBigIntTwoDigit)carry << FKL_BIGINT_DIGIT_SHIFT | a[i];
        carry = (FklBigIntDigit)acc & mask;
        z[i] = (FklBigIntDigit)(acc >> d);
    }
    return carry;
}

static inline void x_divrem(FklBigInt *v1, const FklBigInt *w1,
                            FklBigInt *rem) {
    FklBigInt w = FKL_BIGINT_0;
    FklBigInt a = FKL_BIGINT_0;
    int64_t size_v = fklAbs(v1->num);
    int64_t size_w = fklAbs(w1->num);
    FKL_ASSERT(size_v >= size_w && size_w >= 2);
    ensure_bigint_size(v1, size_v + 1);
    v1->num = size_v + 1;
    ensure_bigint_size(&w, size_w);
    w.num = size_w;
    int d = FKL_BIGINT_DIGIT_SHIFT - bit_length_digit(w1->digits[size_w - 1]);
    FKL_ASSERT(w.digits);
    FklBigIntDigit carry = v_lshift(w.digits, w1->digits, size_w, d);
    FKL_ASSERT(carry == 0 && v1->digits);
    carry = v_lshift(v1->digits, v1->digits, size_v, d);
    if (carry != 0 || v1->digits[size_v - 1] >= w.digits[size_w - 1]) {
        v1->digits[size_v] = carry;
        size_v++;
    }
    int64_t k = size_v - size_w;
    FKL_ASSERT(k >= 0);
    ensure_bigint_size(&a, k);
    a.num = k;
    FklBigIntDigit *v0 = v1->digits;
    FklBigIntDigit *w0 = w.digits;
    FklBigIntDigit wm1 = w0[size_w - 1];
    FklBigIntDigit wm2 = w0[size_w - 2];
    for (FklBigIntDigit *vk = v0 + k, *ak = a.digits + k; vk-- > v0;) {
        FklBigIntDigit vtop = vk[size_w];
        FKL_ASSERT(vtop <= wm1);
        FklBigIntTwoDigit vv =
            ((FklBigIntTwoDigit)vtop << FKL_BIGINT_DIGIT_SHIFT)
            | vk[size_w - 1];
        FklBigIntDigit q = (FklBigIntDigit)(vv / wm1);
        FklBigIntDigit r = (FklBigIntDigit)(vv % wm1);
        while ((FklBigIntTwoDigit)wm2 * q
               > (((FklBigIntTwoDigit)r << FKL_BIGINT_DIGIT_SHIFT)
                  | vk[size_w - 2])) {
            --q;
            r += wm1;
            if (r >= FKL_BIGINT_DIGIT_BASE)
                break;
        }
        FKL_ASSERT(q <= FKL_BIGINT_DIGIT_BASE);
        FklBigIntSDigit zhi = 0;
        for (int64_t i = 0; i < size_w; ++i) {
            FklBigIntSTwoDigit z =
                (FklBigIntSDigit)vk[i] + zhi
                - (FklBigIntSTwoDigit)q * (FklBigIntSTwoDigit)w0[i];
            vk[i] = (FklBigIntDigit)z & FKL_BIGINT_DIGIT_MASK;
            zhi = (FklBigIntSDigit)(z >> FKL_BIGINT_DIGIT_SHIFT);
        }
        FKL_ASSERT((FklBigIntSDigit)vtop + zhi == -1
                   || (FklBigIntSDigit)vtop + zhi == 0);
        if ((FklBigIntSDigit)vtop + zhi < 0) {
            carry = 0;
            for (int64_t i = 0; i < size_w; ++i) {
                carry += vk[i] + w0[i];
                vk[i] = carry & FKL_BIGINT_DIGIT_MASK;
                carry >>= FKL_BIGINT_DIGIT_SHIFT;
            }
            --q;
        }
        FKL_ASSERT(q < FKL_BIGINT_DIGIT_BASE);
        *--ak = q;
    }
    carry = v_rshift(w0, v0, size_w, d);
    FKL_ASSERT(carry == 0);
    if (rem) {
        bigint_normalize(&w);
        fklSetBigInt(rem, &w);
    }
    if (v1 != rem) {
        bigint_normalize(&a);
        fklSetBigInt(v1, &a);
    }

    fklUninitBigInt(&a);
    fklUninitBigInt(&w);
}

int fklDivRemBigInt(FklBigInt *a, const FklBigInt *b, FklBigInt *rem) {
    const int neg_a = a->num < 0;
    const int neg_b = b->num < 0;
    const int64_t num_a = fklAbs(a->num);
    const int64_t num_b = fklAbs(b->num);
    if (num_b == 0)
        return -1;
    else if (num_a < num_b) {
        if (rem != a) {
            if (rem)
                fklSetBigInt(rem, a);
            a->num = 0;
        }
        return 0;
    } else if (num_b == 1)
        divrem1(a->digits, num_a, b->digits[0], rem);
    else
        x_divrem(a, b, rem);
    if (neg_a && rem && !FKL_BIGINT_IS_0(rem))
        rem->num = -rem->num;
    if (a != rem) {
        if ((neg_a != neg_b && a->num > 0) || (neg_a == neg_b && a->num < 0))
            a->num = -a->num;
        bigint_normalize(a);
    }
    return 0;
}

int fklDivBigInt(FklBigInt *a, const FklBigInt *d) {
    return fklDivRemBigInt(a, d, NULL);
}

int fklDivBigIntI(FklBigInt *a, int64_t d) {
    return fklDivRemBigIntI(a, d, NULL);
}

int fklRemBigIntI(FklBigInt *a, int64_t div) {
    FklBigIntDigit digits[FKL_MAX_INT64_DIGITS_COUNT];
    FklBigInt bi = FKL_BIGINT_0;
    bi.size = FKL_MAX_INT64_DIGITS_COUNT;
    bi.digits = digits;
    bi.const_size = 1;
    set_bigint_i64_without_ensure(&bi, div);
    return fklRemBigInt(a, &bi);
}

int fklRemBigInt(FklBigInt *a, const FklBigInt *d) {
    return fklDivRemBigInt(a, d, a);
}

uintptr_t fklBigIntHash(const FklBigInt *bi) {
    const int64_t len = fklAbs(bi->num);
    uintptr_t r = (uintptr_t)bi->num;
    for (int64_t i = 0; i < len; i++)
        r = fklHashCombine(r, bi->digits[i]);
    return r;
}

double fklBigIntToD(const FklBigInt *bi) {
    double r = 0;
    double base = 1;
    const int64_t num = fklAbs(bi->num);
    for (int64_t i = 0; i < num; i++) {
        r += ((double)bi->digits[i]) * base;
        base *= FKL_BIGINT_DIGIT_BASE;
    }
    if (bi->num < 0)
        r = -r;
    return r;
}

static const FklBigIntDigit U64_MAX_DIGITS[3] = {
    0xffffffff & FKL_BIGINT_DIGIT_MASK,
    0xffffffff & FKL_BIGINT_DIGIT_MASK,
    0x0000000f & FKL_BIGINT_DIGIT_MASK,
};

static const FklBigInt U64_MAX_BIGINT = {
    .digits = (FklBigIntDigit *)&U64_MAX_DIGITS[0],
    .num = 3,
    .size = 3,
    .const_size = 1,
};

static const FklBigIntDigit I64_MIN_DIGITS[3] = {
    0x0,
    0x0,
    0x8,
};

static const FklBigInt I64_MIN_BIGINT = {
    .digits = (FklBigIntDigit *)&I64_MIN_DIGITS[0],
    .num = -3,
    .size = 3,
    .const_size = 1,
};

static const FklBigIntDigit I64_MAX_DIGITS[3] = {
    0xffffffff & FKL_BIGINT_DIGIT_MASK,
    0xffffffff & FKL_BIGINT_DIGIT_MASK,
    0x00000007 & FKL_BIGINT_DIGIT_MASK,
};

static const FklBigInt I64_MAX_BIGINT = {
    .digits = (FklBigIntDigit *)&I64_MAX_DIGITS[0],
    .num = 3,
    .size = 3,
    .const_size = 1,
};

int fklIsBigIntGtLtI64(const FklBigInt *a) {
    return fklBigIntCmp(a, &I64_MAX_BIGINT) > 0
        || fklBigIntCmp(a, &I64_MIN_BIGINT) < 0;
}

uint64_t fklBigIntToU(const FklBigInt *bi) {
    if (bi->num <= 0)
        return 0;
    if (bi->num > 3)
        return UINT64_MAX;
    else if (bi->num > 2 && fklBigIntCmp(bi, &U64_MAX_BIGINT) > 0)
        return UINT64_MAX;
    else {
        uint64_t r = 0;
        for (int64_t i = 0; i < bi->num; i++)
            r |= ((uint64_t)bi->digits[i]) << (FKL_BIGINT_DIGIT_SHIFT * i);
        return r;
    }
}

int64_t fklBigIntToI(const FklBigInt *bi) {
    if (bi->num < -3)
        return INT64_MIN;
    else if (bi->num < -2 && fklBigIntCmp(bi, &I64_MIN_BIGINT) < 0)
        return INT64_MIN;
    else if (bi->num == 0)
        return 0;
    else if (bi->num > 3)
        return INT64_MAX;
    else if (bi->num > 2 && fklBigIntCmp(bi, &I64_MAX_BIGINT) > 0)
        return INT64_MAX;
    else {
        int64_t r = 0;
        const int64_t num = fklAbs(bi->num);
        for (int64_t i = 0; i < num; i++)
            r |= ((uint64_t)bi->digits[i]) << (FKL_BIGINT_DIGIT_SHIFT * i);
        if (bi->num < 0)
            r = -r;
        return r;
    }
}

static inline size_t bigint_to_dec_string_buffer(const FklBigInt *a,
                                                 FklBigIntToStrAllocCb alloc_cb,
                                                 void *ctx) {
    FKL_ASSERT(a->num);
    int64_t size_a = fklAbs(a->num);
    int64_t d = (33 * FKL_BIGINT_DECIMAL_SHIFT)
              / (10 * FKL_BIGINT_DIGIT_SHIFT - 33 * FKL_BIGINT_DECIMAL_SHIFT);
    int64_t size = 1 + size_a + size_a / d;
    const FklBigIntDigit *pin = a->digits;
    FklBigIntDigit *pout =
        (FklBigIntDigit *)fklZmalloc(size * sizeof(FklBigIntDigit));
    FKL_ASSERT(pout);
    size = 0;
    for (int64_t i = size_a; --i >= 0;) {
        FklBigIntDigit hi = pin[i];
        for (int64_t j = 0; j < size; j++) {
            FklBigIntTwoDigit z =
                (FklBigIntTwoDigit)pout[j] << FKL_BIGINT_DIGIT_SHIFT | hi;
            hi = (FklBigIntDigit)(z / FKL_BIGINT_DECIMAL_BASE);
            pout[j] = (FklBigIntDigit)(z
                                       - (FklBigIntTwoDigit)hi
                                             * FKL_BIGINT_DECIMAL_BASE);
        }
        while (hi) {
            pout[size++] = hi % FKL_BIGINT_DECIMAL_BASE;
            hi /= FKL_BIGINT_DECIMAL_BASE;
        }
    }
    if (size == 0)
        pout[size++] = 0;
    size_t neg = a->num < 0;
    size_t len = neg + 1 + (size - 1) * FKL_BIGINT_DECIMAL_SHIFT;
    FklBigIntDigit tenpow = 10;
    FklBigIntDigit rem = pout[size - 1];
    while (rem >= tenpow) {
        tenpow *= 10;
        len++;
    }
    char *p_str = alloc_cb(ctx, len) + len;
    int64_t i;
    for (i = 0; i < size - 1; i++) {
        rem = pout[i];
        for (int64_t j = 0; j < FKL_BIGINT_DECIMAL_SHIFT; j++) {
            *--p_str = '0' + rem % 10;
            rem /= 10;
        }
    }
    rem = pout[i];
    do {
        *--p_str = '0' + rem % 10;
        rem /= 10;
    } while (rem != 0);
    if (neg)
        *--p_str = '-';
    fklZfree(pout);
    return len;
}

static inline size_t bigint_to_bin_string_buffer(const FklBigInt *a,
                                                 FklBigIntToStrAllocCb alloc_cb,
                                                 void *ctx, uint8_t base,
                                                 FklBigIntFmtFlags flags,
                                                 const char *alt_str) {
    FKL_ASSERT(a->num);
    int bits = 0;
    int alternate = (flags & FKL_BIGINT_FMT_FLAG_ALTERNATE) != 0;
    int capitals = (flags & FKL_BIGINT_FMT_FLAG_CAPITALS) != 0;
    switch (base) {
    case 16:
        bits = HEX_BIT_COUNT;
        break;
    case 8:
        bits = OCT_BIT_COUNT;
        break;
    default:
        abort();
    }
    int64_t size_a = fklAbs(a->num);
    int neg = a->num < 0;
    int64_t size_a_in_bits = (size_a - 1) * FKL_BIGINT_DIGIT_SHIFT
                           + bit_length_digit(a->digits[size_a - 1]);
    int64_t sz = neg + (size_a_in_bits + (bits - 1)) / bits;
    size_t alt_str_len = strlen(alt_str);
    if (alternate)
        sz += alt_str_len;
    char *p_str = alloc_cb(ctx, sz) + sz;
    FklBigIntTwoDigit accum = 0;
    int accumbits = 0;
    int64_t i;
    for (i = 0; i < size_a; ++i) {
        accum |= (FklBigIntTwoDigit)a->digits[i] << accumbits;
        accumbits += FKL_BIGINT_DIGIT_SHIFT;
        FKL_ASSERT(accumbits >= bits);
        do {
            char cdigit;
            cdigit = (char)(accum & (base - 1));
            cdigit += (cdigit < 10) ? '0' : (capitals ? 'A' : 'a') - 10;
            *--p_str = cdigit;
            accumbits -= bits;
            accum >>= bits;
        } while (i < size_a - 1 ? accumbits >= bits : accum > 0);
    }
    if (alternate) {
        memcpy(p_str - alt_str_len, alt_str, alt_str_len);
        p_str -= alt_str_len;
    }
    if (neg)
        *--p_str = '-';
    return sz;
}

size_t fklBigIntToStr(const FklBigInt *a, FklBigIntToStrAllocCb alloc_cb,
                      void *ctx, uint8_t radix, FklBigIntFmtFlags flags) {
    if (FKL_BIGINT_IS_0(a)) {
        char *ptr = alloc_cb(ctx, 1);
        ptr[0] = '0';
        return 1;
    } else if (radix == 10)
        return bigint_to_dec_string_buffer(a, alloc_cb, ctx);
    else if (radix == 8)
        return bigint_to_bin_string_buffer(a, alloc_cb, ctx, 8, flags, "0");
    else if (radix == 16)
        return bigint_to_bin_string_buffer(
            a, alloc_cb, ctx, 16, flags,
            flags & FKL_BIGINT_FMT_FLAG_CAPITALS ? "0X" : "0x");
    else
        abort();
}

static char *print_bigint_alloc(void *ptr, size_t len) {
    char **pstr = ptr;
    char *str = (char *)fklZmalloc((len + 1) * sizeof(char));
    FKL_ASSERT(str);
    str[len] = '\0';
    *pstr = str;
    return str;
}

void fklPrintBigInt(const FklBigInt *a, FILE *fp) {
    if (a->num == 0)
        fputc('0', fp);
    else {
        char *str = NULL;
        bigint_to_dec_string_buffer(a, print_bigint_alloc, &str);
        fputs(str, fp);
        fklZfree(str);
    }
}

char *fklBigIntToCstr(const FklBigInt *a, uint8_t radix,
                      FklBigIntFmtFlags flags) {
    char *str = NULL;
    fklBigIntToStr(a, print_bigint_alloc, &str, radix, flags);
    return str;
}
