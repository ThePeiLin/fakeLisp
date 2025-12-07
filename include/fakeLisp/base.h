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

#define FKL_FIX_INT_MAX (1152921504606846975)
#define FKL_FIX_INT_MIN (-1152921504606846975 - 1)

typedef struct FklString {
    uint64_t size;
    char str[1];
} FklString;

FklString *fklCreateString(size_t, const char *);
FklString *fklStringRealloc(FklString *s, size_t new_size);
FklString *fklCopyString(const FklString *);
FklString *fklCreateEmptyString(void);
FklString *fklCreateStringFromCstr(const char *);
size_t fklCountCharInString(FklString *s, char c);
void fklStringCat(FklString **, const FklString *);
void fklStringCstrCat(FklString **, const char *);
void fklStringCharBufCat(FklString **, const char *buf, size_t s);
char *fklCstrStringCat(char *, const FklString *);
int fklStringCmp(const FklString *, const FklString *);
int fklStringEqual(const FklString *fir, const FklString *sec);

int fklStringCharBufCmp(const FklString *, size_t len, const char *buf);
int fklStringCstrCmp(const FklString *, const char *);

ssize_t fklStringCharBufMatch(const FklString *, const char *, size_t len);

ssize_t fklCharBufMatch(const char *s0, size_t l0, const char *s1, size_t l1);

size_t
fklQuotedStringMatch(const char *cstr, size_t restLen, const FklString *end);
size_t fklQuotedCharBufMatch(const char *cstr,
        size_t restLen,
        const char *end,
        size_t end_size);

void fklWriteString(const FklString *s, FILE *fp);
FklString *fklLoadString(FILE *fp);

char *fklStringToCstr(const FklString *str);

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

FklString *fklStringAppend(const FklString *, const FklString *);
void fklWriteStringToCstr(char *, const FklString *);

void fklPrintString(const FklString *str, FILE *fp);
static inline void fklPrintString2(const FklString *str, FklCodeBuilder *build);

void fklPrintStringLiteral(const FklString *str, FILE *fp);
static inline void fklPrintStringLiteral2(const FklString *str,
        FklCodeBuilder *build);

void fklPrintSymbolLiteral(const FklString *str, FILE *fp);
static inline void fklPrintSymbolLiteral2(const FklString *str,
        FklCodeBuilder *build);

void fklPrintStrLiteral(const char *str, FILE *fp);
static inline void fklPrintStrLiteral2(const char *str, FklCodeBuilder *fp);

void fklPrintSymLiteral(const char *str, FILE *fp);
static inline void fklPrintSymLiteral2(const char *str, FklCodeBuilder *fp);

void fklPrintBufLiteralExt(size_t len,
        const char *fstr,
        const char *begin_str,
        const char *end_str,
        char se,
        FklCodeBuilder *build);

static inline void fklPrintStringLiteralExt(const FklString *fstr,
        const char *begin_str,
        const char *end_str,
        char se,
        FklCodeBuilder *build);

static inline void fklPrintStrLiteralExt(const char *str,
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

typedef struct FklBytevector {
    uint64_t size;
    uint8_t ptr[];
} FklBytevector;
FklBytevector *fklStrBufToBytevector(FklStrBuf *);

FklBytevector *fklCreateBytevector(size_t, const uint8_t *);
FklBytevector *fklBytevectorRealloc(FklBytevector *b, size_t new_size);
FklBytevector *fklCopyBytevector(const FklBytevector *);
void fklBytevectorCat(FklBytevector **, const FklBytevector *);
int fklBytevectorCmp(const FklBytevector *, const FklBytevector *);
int fklBytevectorEqual(const FklBytevector *fir, const FklBytevector *sec);

void fklPrintBytesLiteral(const FklBytevector *str, FILE *fp);
void fklPrintBytesLiteral2(const FklBytevector *bvec, FklCodeBuilder *build);

void fklWriteBytevector(const FklBytevector *b, FILE *fp);
FklBytevector *fklLoadBytevector(FILE *fp);

uintptr_t fklBytevectorHash(const FklBytevector *bv);

// FklUintVector
#define FKL_VECTOR_ELM_TYPE uintmax_t
#define FKL_VECTOR_ELM_TYPE_NAME Uint
#include "cont/vector.h"

// FklstringVector
#define FKL_VECTOR_ELM_TYPE FklString *
#define FKL_VECTOR_ELM_TYPE_NAME String
#include "cont/vector.h"

size_t fklBigIntToStrBuf(const FklBigInt *a,
        FklStrBuf *string_buffer,
        uint8_t radix,
        FklBigIntFmtFlags flags);

FklString *
fklBigIntToString(const FklBigInt *a, uint8_t radix, FklBigIntFmtFlags flags);

int fklIsBigIntGtLtFix(const FklBigInt *a);
int fklIsBigIntAddkInFixIntRange(const FklBigInt *a, int8_t k);

static inline uintptr_t fklHashCombine(uintptr_t seed, uintptr_t hash) {
    return seed ^ (hash + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

static inline uint32_t fklNextPow2(uint32_t n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

// Thomas Wang's 32 Bit / 64 Bit Mix Function
static inline uintptr_t fklHash32Shift(uint32_t k) {
    k = ~k + (k << 15);
    k = k ^ (k >> 12);
    k = k + (k << 2);
    k = k ^ (k >> 4);
    k = (k + (k << 3)) + (k << 11);
    k = k ^ (k >> 16);
    return k;
}

static inline uintptr_t fklHash64Shift(uint64_t key) {
    key = (~key) + (key << 21); // key = (key << 21) - key - 1;
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8); // key * 265
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4); // key * 21
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return key;
}

void fklInitCodeBuilderFp(FklCodeBuilder *b, FILE *fp, const char *indent_str);

#ifdef __cplusplus
}
#endif
#endif
