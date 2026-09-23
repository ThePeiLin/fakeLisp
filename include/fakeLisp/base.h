#ifndef FKL_BASE_H
#define FKL_BASE_H

#include "bigint.h"
#include "code_builder.h"
#include "common.h"
#include "str_buf.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t len;
    const char *str;
} FklStrView;

typedef struct FklString {
    uint64_t size;
    char str[1];
} FklString;

FKL_API FklString *fklCreateString(size_t, const char *);
FKL_API FklString *fklStringRealloc(FklString *s, size_t new_size);
FKL_API FklString *fklCopyString(const FklString *);
FKL_API FklString *fklCreateEmptyString(void);
FKL_API FklString *fklCreateStringFromCstr(const char *);
FKL_API size_t fklCountCharInString(FklString *s, char c);
FKL_API void fklStringCat(FklString **, const FklString *);
FKL_API void fklStringCstrCat(FklString **, const char *);
FKL_API void fklStringCharBufCat(FklString **, const char *buf, size_t s);
FKL_API char *fklCstrStringCat(char *, const FklString *);
FKL_API int fklStringCmp(const FklString *, const FklString *);
FKL_API int fklStringEqual(const FklString *fir, const FklString *sec);

FKL_API int fklStringCharBufCmp(const FklString *, size_t len, const char *buf);
FKL_API int fklStringCstrCmp(const FklString *, const char *);

FKL_API
ssize_t fklStringCharBufMatch(const FklString *, const char *, size_t len);

FKL_API
ssize_t fklCharBufMatch(const char *s0, size_t l0, const char *s1, size_t l1);

FKL_API
int fklCharBufEqual(const char *s0, size_t l0, const char *s1, size_t l1);

FKL_API
size_t
fklQuotedStringMatch(const char *cstr, size_t restLen, const FklString *end);

FKL_API
size_t fklQuotedCharBufMatch(const char *cstr,
        size_t restLen,
        const char *end,
        size_t end_size);

FKL_API void fklWriteString(const FklString *s, FILE *fp);
FKL_API FklString *fklLoadString(FILE *fp);

FKL_API char *fklStringToCstr(const FklString *str);

static inline uintptr_t fklCharBufHash(const char *str, size_t len) {
    uintptr_t h = 0;
    for (size_t i = 0; i < len; i++)
        h = 31 * h + str[i];
    return h;
}

static inline uintptr_t fklStringHash(const FklString *s) {
    return fklCharBufHash(s->str, s->size);
}

static inline void fklStrBufConcatWithString(FklStrBuf *b, const FklString *s) {
    fklStrBufBincpy(b, s->str, s->size);
}

static inline FklString *fklStrBufToString(FklStrBuf *b) {
    return fklCreateString(b->index, b->buf);
}

FKL_API FklString *fklStringAppend(const FklString *, const FklString *);
FKL_API void fklWriteStringToCstr(char *, const FklString *);

FKL_API void fklPrintString(const FklString *str, FILE *fp);

FKL_API void fklPrintStringLiteral(const FklString *str, FILE *fp);

FKL_API void fklPrintSymbolLiteral(const FklString *str, FILE *fp);

FKL_API void fklPrintStrLiteral(const char *str, FILE *fp);

FKL_API void fklPrintSymLiteral(const char *str, FILE *fp);

FKL_API
void fklPrintBufLiteralExt(size_t len,
        const char *fstr,
        const char *begin_str,
        const char *end_str,
        char se,
        FklCodeBuilder *build);

static inline void fklPrintString2(const FklString *str, FklCodeBuilder *b) {
    fklCodeBuilderPuts(b, str->str);
}

static inline void fklPrintStrLiteralExt(const char *str,
        const char *begin_str,
        const char *end_str,
        char se,
        FklCodeBuilder *build) {
    fklPrintBufLiteralExt(strlen(str), str, begin_str, end_str, se, build);
}

static inline void fklPrintStringLiteralExt(const FklString *fstr,
        const char *begin_str,
        const char *end_str,
        char se,
        FklCodeBuilder *build) {
    fklPrintBufLiteralExt(fstr->size, fstr->str, begin_str, end_str, se, build);
}

static inline void fklPrintSymbolLiteral2(const FklString *fstr,
        FklCodeBuilder *build) {
    fklPrintBufLiteralExt(fstr->size, fstr->str, "|", "|", '|', build);
}

static inline void fklPrintKeywordLiteral2(const FklString *fstr,
        FklCodeBuilder *build) {
    fklPrintBufLiteralExt(fstr->size, fstr->str, ":|", "|", '|', build);
}

static inline void fklPrintStringLiteral2(const FklString *fstr,
        FklCodeBuilder *build) {
    fklPrintBufLiteralExt(fstr->size, fstr->str, "\"", "\"", '"', build);
}

static inline void fklPrintStrLiteral2(const char *fstr,
        FklCodeBuilder *build) {
    fklPrintStrLiteralExt(fstr, "\"", "\"", '"', build);
}

static inline void fklPrintSymLiteral2(const char *fstr,
        FklCodeBuilder *build) {
    fklPrintStrLiteralExt(fstr, "|", "|", '|', build);
}

typedef struct FklBytes {
    uint64_t size;
    uint8_t ptr[FKL_FLEX_ARRAY_MEMBER];
} FklBytes;

FKL_API FklBytes *fklStrBufToBytes(FklStrBuf *);

FKL_API FklBytes *fklCreateBytes(size_t, const uint8_t *);
FKL_API FklBytes *fklBytesRealloc(FklBytes *b, size_t new_size);
FKL_API FklBytes *fklCopyBytes(const FklBytes *);
FKL_API void fklBytesCat(FklBytes **, const FklBytes *);
FKL_API int fklBytesCmp(const FklBytes *, const FklBytes *);
FKL_API int fklBytesEqual(const FklBytes *fir, const FklBytes *sec);

FKL_API void fklPrintBytesLiteral(const FklBytes *str, FILE *fp);

FKL_API
void fklPrintBytesLiteral2(const FklBytes *bytes, FklCodeBuilder *build);

FKL_API void fklWriteBytes(const FklBytes *b, FILE *fp);
FKL_API FklBytes *fklLoadBytes(FILE *fp);

FKL_API uintptr_t fklBytesHash(const FklBytes *bv);

// FklUintVector
#define FKL_VECTOR_ELM_TYPE uintmax_t
#define FKL_VECTOR_ELM_TYPE_NAME Uint
#include "cont/vector.h"

// FklStringVector
#define FKL_VECTOR_ELM_TYPE FklString *
#define FKL_VECTOR_ELM_TYPE_NAME String
#include "cont/vector.h"

// FklStrViewVector
#define FKL_VECTOR_ELM_TYPE FklStrView
#define FKL_VECTOR_ELM_TYPE_NAME StrView
#include "cont/vector.h"

FKL_API
size_t fklBigIntToStrBuf(const FklBigInt *a,
        FklStrBuf *string_buffer,
        uint8_t radix,
        FklBigIntFmtFlags flags);

FKL_API
FklString *
fklBigIntToString(const FklBigInt *a, uint8_t radix, FklBigIntFmtFlags flags);

FKL_API int fklIsBigIntGtLtFix(const FklBigInt *a);
FKL_API int fklIsBigIntAddkInFixIntRange(const FklBigInt *a, int8_t k);

static FKL_ALWAYS_INLINE uintptr_t fklHashCombine(uintptr_t seed,
        uintptr_t hash) {
    return seed ^ (hash + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

static FKL_ALWAYS_INLINE uint32_t fklNextPow2(uint32_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

// Thomas Wang's 32 Bit / 64 Bit Mix Function
static FKL_ALWAYS_INLINE uintptr_t fklHash32Shift(uint32_t k) {
    k = ~k + (k << 15);
    k = k ^ (k >> 12);
    k = k + (k << 2);
    k = k ^ (k >> 4);
    k = (k + (k << 3)) + (k << 11);
    k = k ^ (k >> 16);
    return k;
}

static FKL_ALWAYS_INLINE uintptr_t fklHash64Shift(uint64_t key) {
    key = (~key) + (key << 21); // key = (key << 21) - key - 1;
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8); // key * 265
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4); // key * 21
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return key;
}

static FKL_ALWAYS_INLINE uintptr_t fklPtrHash(const void *key) {
    return fklHash64Shift(FKL_TYPE_CAST(uintptr_t, key));
}

FKL_API
void fklInitCodeBuilderFp(FklCodeBuilder *b, FILE *fp, const char *indent_str);

#ifdef __cplusplus
}
#endif
#endif
