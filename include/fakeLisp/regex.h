#ifndef FKL_REGEX_H
#define FKL_REGEX_H

#include "base.h"
#include "common.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FKL_REGEX_UNUSED = 0,
    FKL_REGEX_BEGIN,
    FKL_REGEX_END,
    FKL_REGEX_DOT,
    FKL_REGEX_CHAR,
    FKL_REGEX_CHAR_CLASS,
    FKL_REGEX_INV_CHAR_CLASS,
    FKL_REGEX_DIGITS,
    FKL_REGEX_NOT_DIGITS,
    FKL_REGEX_ALPHA,
    FKL_REGEX_NOT_ALPHA,
    FKL_REGEX_WHITESPACE,
    FKL_REGEX_NOT_WHITESPACE,
    FKL_REGEX_STAR,
    FKL_REGEX_PLUS,
    FKL_REGEX_QUESTIONMARK,
    FKL_REGEX_BRANCH,
    FKL_REGEX_GROUPSTART,
    FKL_REGEX_GROUPEND,
};

enum {
    FKL_REGEX_CHAR_CLASS_END = 0,
    FKL_REGEX_CHAR_CLASS_CHAR,
    FKL_REGEX_CHAR_CLASS_RANGE,
};

typedef struct {
    uint32_t type;
    uint32_t trueoffset;
    uint32_t falseoffset;
    union {
        uint8_t ch;
        uint32_t ccl;
    };
} FklRegexObj;

typedef struct {
    uint32_t totalsize;
    uint32_t pstsize;
    FklRegexObj data[];
} FklRegexCode;

// FklRegexStrHashMap
#define FKL_HASH_KEY_TYPE FklRegexCode const *
#define FKL_HASH_VAL_TYPE FklString *
#define FKL_HASH_ELM_NAME RegexStr
#define FKL_HASH_KEY_HASH                                                      \
    return fklHash64Shift(                                                     \
        FKL_TYPE_CAST(uintptr_t, (*pk) / alignof(FklRegexCode)));
#include "hash.h"

typedef struct {
    uint64_t num;
    FklRegexCode *re;
} FklRegexItem;

void fklRegexFree(FklRegexCode *);

// FklStrRegexHashMap
#define FKL_HASH_KEY_TYPE FklString *
#define FKL_HASH_VAL_TYPE FklRegexItem
#define FKL_HASH_ELM_NAME StrRegex
#define FKL_HASH_KEY_HASH return fklStringHash(*pk);
#define FKL_HASH_KEY_EQUAL(A, B) fklStringEqual(*(A), *(B))
#define FKL_HASH_KEY_UNINIT(K) free(*(K))
#define FKL_HASH_VAL_UNINIT(V) fklRegexFree((V)->re)
#include "hash.h"

typedef struct {
    FklStrRegexHashMap str_re;
    FklRegexStrHashMap re_str;
    uint32_t num;
} FklRegexTable;

FklRegexCode *fklRegexCompileCstr(const char *pattern);
FklRegexCode *fklRegexCompileCharBuf(const char *pattern, size_t len);

uint32_t fklRegexMatchpInCstr(const FklRegexCode *pattern, const char *text,
                              uint32_t *ppos);
uint32_t fklRegexMatchpInCharBuf(const FklRegexCode *pattern, const char *text,
                                 uint32_t len, uint32_t *ppos);
uint32_t fklRegexLexMatchp(const FklRegexCode *re, const char *str,
                           uint32_t len, int *last_is_true);

void fklRegexPrint(const FklRegexCode *, FILE *fp);

void fklRegexPrintAsC(const FklRegexCode *, const char *prefix,
                      const char *pattern, uint32_t pattern_len, FILE *fp);

void fklRegexPrintAsCwithNum(const FklRegexCode *, const char *prefix,
                             uint64_t num, FILE *fp);

FklRegexTable *fklCreateRegexTable(void);
void fklInitRegexTable(FklRegexTable *);
void fklUninitRegexTable(FklRegexTable *);

const FklRegexCode *fklAddRegexStr(FklRegexTable *table, const FklString *str);

const FklRegexCode *fklAddRegexCharBuf(FklRegexTable *table, const char *buf,
                                       size_t len);

const FklRegexCode *fklAddRegexCstr(FklRegexTable *table, const char *str);

const FklString *fklGetStringWithRegex(const FklRegexTable *t,
                                       const FklRegexCode *, uint64_t *num);

#ifdef __cplusplus
}
#endif

#endif
