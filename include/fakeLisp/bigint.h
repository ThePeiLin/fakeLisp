#ifndef FKL_BIGINT_H
#define FKL_BIGINT_H

#include "code_builder.h"

#include <stdint.h>
#include <stdio.h>

// steal from cpython: https://github.com/python/cpython

typedef uint32_t FklBigIntDigit;
typedef int32_t FklBigIntSDigit;
typedef uint64_t FklBigIntTwoDigit;
typedef int64_t FklBigIntSTwoDigit;
#define FKL_BIGINT_DIGIT_SHIFT (30)
#define FKL_BIGINT_DIGIT_BASE ((FklBigIntDigit)1 << FKL_BIGINT_DIGIT_SHIFT)
#define FKL_BIGINT_DIGIT_MASK ((FklBigIntDigit)(FKL_BIGINT_DIGIT_BASE - 1))
#define FKL_MAX_INT64_DIGITS_COUNT (3)
#define FKL_MAX_UINT64_DIGITS_COUNT (3)

typedef struct FklBigInt {
    FklBigIntDigit *digits;
    int64_t num;
    uint64_t size : 63;
    uint64_t const_size : 1;
} FklBigInt;

typedef enum FklBigIntFmtFlags {
    FKL_BIGINT_FMT_FLAG_NONE = 0,
    FKL_BIGINT_FMT_FLAG_ALTERNATE = 1 << 0,
    FKL_BIGINT_FMT_FLAG_CAPITALS = 1 << 1,
} FklBigIntFmtFlags;

// 替换labs，防止一些机器上long只有32位长
static FKL_ALWAYS_INLINE int64_t fklAbs(int64_t a) { return a > 0 ? a : -a; }

#define FKL_BIGINT_0                                                           \
    ((FklBigInt){                                                              \
        .digits = NULL,                                                        \
        .num = 0,                                                              \
        .size = 0,                                                             \
        .const_size = 0,                                                       \
    })
#define FKL_BIGINT_IS_0(I)                                                     \
    (((I)->num == 0) || (fklAbs((I)->num) == 1 && (I)->digits[0] == 0))
#define FKL_BIGINT_IS_1(I) (((I)->num == 1) && ((I)->digits[0] == 1))
#define FKL_BIGINT_IS_N1(I) (((I)->num == -1) && ((I)->digits[0] == 1))
#define FKL_BIGINT_IS_ABS1(I) ((fklAbs((I)->num) == 1) && ((I)->digits[0] == 1))

FKL_API FklBigInt *fklCreateBigInt0(void);
FKL_API FklBigInt *fklCreateBigInt1(void);
FKL_API FklBigInt *fklCreateBigIntN1(void);
FKL_API FklBigInt *fklCreateBigIntI(int64_t v);
FKL_API FklBigInt *fklCreateBigIntD(double v);
FKL_API FklBigInt *fklCreateBigIntU(uint64_t v);

FKL_API
FklBigInt *fklCreateBigIntFromMemCopy(int64_t num, const FklBigIntDigit *mem);
FKL_API
FklBigInt *fklCreateBigIntFromMem(int64_t num, const FklBigIntDigit *mem);

FKL_API FklBigInt *fklCreateBigIntWithCharBuf(const char *buf, size_t size);
FKL_API FklBigInt *fklCreateBigIntWithDecCharBuf(const char *buf, size_t);
FKL_API FklBigInt *fklCreateBigIntWithHexCharBuf(const char *buf, size_t);
FKL_API FklBigInt *fklCreateBigIntWithOctCharBuf(const char *buf, size_t);
FKL_API FklBigInt *fklCreateBigIntWithCstr(const char *str);

FKL_API void fklDestroyBigInt(FklBigInt *);

FKL_API FklBigInt *fklCopyBigInt(const FklBigInt *);

FKL_API void fklInitBigInt(FklBigInt *, const FklBigInt *);
FKL_API void fklInitBigInt0(FklBigInt *);
FKL_API void fklInitBigInt1(FklBigInt *);
FKL_API void fklInitBigIntN1(FklBigInt *);
FKL_API void fklInitBigIntI(FklBigInt *, int64_t);
FKL_API void fklInitBigIntU(FklBigInt *, uint64_t);
FKL_API void fklInitBigIntD(FklBigInt *, double);

FKL_API
void fklInitBigIntFromMem(FklBigInt *t, int64_t num, FklBigIntDigit *mem);

FKL_API void fklInitBigIntWithCharBuf(FklBigInt *t, const char *, size_t);
FKL_API void fklInitBigIntWithDecCharBuf(FklBigInt *t, const char *, size_t);
FKL_API void fklInitBigIntWithHexCharBuf(FklBigInt *t, const char *, size_t);
FKL_API void fklInitBigIntWithOctCharBuf(FklBigInt *t, const char *, size_t);
FKL_API void fklInitBigIntWithCstr(FklBigInt *t, const char *);

typedef struct {
    FklBigIntDigit *(*alloc)(void *ctx, size_t);
    int64_t *(*num)(void *ctx);
} FklBigIntInitWithCharBufMethodTable;

FKL_API
void fklInitBigIntWithCharBuf2(void *ctx,
        const FklBigIntInitWithCharBufMethodTable *table,
        const char *,
        size_t);

FKL_API
void fklInitBigIntWithDecCharBuf2(void *ctx,
        const FklBigIntInitWithCharBufMethodTable *table,
        const char *,
        size_t);

FKL_API
void fklInitBigIntWithOctCharBuf2(void *ctx,
        const FklBigIntInitWithCharBufMethodTable *table,
        const char *,
        size_t);

FKL_API
void fklInitBigIntWithHexCharBuf2(void *ctx,
        const FklBigIntInitWithCharBufMethodTable *table,
        const char *,
        size_t);

FKL_API void fklInitBigIntWithCstr2(FklBigInt *t, const char *);

FKL_API void fklUninitBigInt(FklBigInt *);

FKL_API uintptr_t fklBigIntHash(const FklBigInt *bi);
FKL_API void fklSetBigInt(FklBigInt *to, const FklBigInt *from);
FKL_API void fklSetBigIntI(FklBigInt *to, int64_t from);
FKL_API void fklSetBigIntU(FklBigInt *to, uint64_t from);
FKL_API void fklSetBigIntD(FklBigInt *to, double from);

FKL_API void fklAddBigInt(FklBigInt *, const FklBigInt *addend);
FKL_API void fklAddBigIntI(FklBigInt *, int64_t addend);
FKL_API void fklSubBigInt(FklBigInt *, const FklBigInt *sub);
FKL_API void fklSubBigIntI(FklBigInt *, int64_t sub);
FKL_API void fklMulBigInt(FklBigInt *, const FklBigInt *mul);
FKL_API void fklMulBigIntI(FklBigInt *, int64_t mul);
FKL_API int fklDivBigInt(FklBigInt *a, const FklBigInt *d);
FKL_API int fklDivBigIntI(FklBigInt *a, int64_t d);
FKL_API int fklRemBigInt(FklBigInt *a, const FklBigInt *d);
FKL_API int fklRemBigIntI(FklBigInt *a, int64_t d);

FKL_API
int fklDivRemBigInt(FklBigInt *a, const FklBigInt *divider, FklBigInt *rem);
FKL_API int fklDivRemBigIntI(FklBigInt *a, int64_t divider, FklBigInt *rem);

FKL_API int fklIsBigIntOdd(const FklBigInt *);
FKL_API int fklIsBigIntEven(const FklBigInt *);
FKL_API int fklIsBigIntLt0(const FklBigInt *);
FKL_API int fklIsBigIntLe0(const FklBigInt *);
FKL_API int fklIsBigIntGtLtI64(const FklBigInt *);
FKL_API int fklIsDivisibleBigInt(const FklBigInt *a, const FklBigInt *b);
FKL_API int fklIsDivisibleBigIntI(const FklBigInt *a, int64_t b);
FKL_API int fklIsDivisibleIBigInt(int64_t a, const FklBigInt *b);
FKL_API int fklBigIntEqual(const FklBigInt *a, const FklBigInt *b);
FKL_API int fklBigIntCmp(const FklBigInt *a, const FklBigInt *b);
FKL_API int fklBigIntCmpI(const FklBigInt *a, int64_t b);
FKL_API int fklBigIntCmpU(const FklBigInt *a, uint64_t b);
FKL_API int fklBigIntAbsCmp(const FklBigInt *a, const FklBigInt *b);

FKL_API int64_t fklBigIntToI(const FklBigInt *a);
FKL_API uint64_t fklBigIntToU(const FklBigInt *a);
FKL_API double fklBigIntToD(const FklBigInt *a);

typedef char *(*FklBigIntToStrAllocCb)(void *ctx, size_t);

FKL_API size_t fklBigIntToStr(const FklBigInt *a,
        FklBigIntToStrAllocCb alloc_cb,
        void *ctx,
        uint8_t radix,
        FklBigIntFmtFlags flags);

FKL_API void fklPrintBigInt(const FklBigInt *a, FILE *fp);
FKL_API void fklPrintBigInt2(const FklBigInt *a, FklCodeBuilder *fp);

FKL_API char *
fklBigIntToCstr(const FklBigInt *a, uint8_t radix, FklBigIntFmtFlags flags);
#endif
