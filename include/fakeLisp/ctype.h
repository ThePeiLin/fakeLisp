#ifndef FKL_CTYPE_H
#define FKL_CTYPE_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * steal from musl
 * [x] int isalnum(int c);
 * [x] int isalpha(int c);
 * [x] int isblank(int c);
 * [x] int iscntrl(int c);
 * [x] int isdigit(int c);
 * [x] int isgraph(int c);
 * [x] int islower(int c);
 * [x] int isprint(int c);
 * [x] int ispunct(int c);
 * [x] int isspace(int c);
 * [x] int isupper(int c);
 * [x] int isxdigit(int c);
 * [x] int tolower(int c);
 * [x] int toupper(int c);
 */

static FKL_ALWAYS_INLINE int fklIsAlpha(int a) {
    return (((unsigned)(a) | 32) - 'a') < 26;
}

static FKL_ALWAYS_INLINE int fklIsDigit(int a) {
    return ((unsigned)(a) - '0') < 10;
}

static FKL_ALWAYS_INLINE int fklIsLower(int a) {
    return ((unsigned)(a) - 'a') < 26;
}

static FKL_ALWAYS_INLINE int fklIsUpper(int a) {
    return ((unsigned)(a) - 'A') < 26;
}

static FKL_ALWAYS_INLINE int fklIsPrint(int a) {
    return ((unsigned)(a)-0x20) < 0x5f;
}

static FKL_ALWAYS_INLINE int fklIsGraph(int a) {
    return ((unsigned)(a)-0x21) < 0x5e;
}

static FKL_ALWAYS_INLINE int fklIsSpace(int _c) {
    return _c == ' ' || (unsigned)_c - '\t' < 5;
}

static FKL_ALWAYS_INLINE int fklIsBlank(int c) {
    return (c == ' ' || c == '\t');
}

static FKL_ALWAYS_INLINE int fklIsCntrl(int c) {
    return (unsigned)c < 0x20 || c == 0x7f;
}

static FKL_ALWAYS_INLINE int fklIsXDigit(int c) {
    return fklIsDigit(c) || ((unsigned)c | 32) - 'a' < 6;
}

static FKL_ALWAYS_INLINE int fklIsAlnum(int c) {
    return fklIsAlpha(c) || fklIsDigit(c);
}

static FKL_ALWAYS_INLINE int fklIsPunct(int c) {
    return fklIsGraph(c) && !fklIsAlnum(c);
}

static FKL_ALWAYS_INLINE int fklToLower(int c) {
    if (fklIsUpper(c))
        return c | 32;
    return c;
}

static FKL_ALWAYS_INLINE int fklToUpper(int c) {
    if (fklIsLower(c))
        return c & 0x5f;
    return c;
}

#ifdef __cplusplus
}
#endif

#endif