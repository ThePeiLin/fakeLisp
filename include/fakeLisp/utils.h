#ifndef FKL_UTILS_H
#define FKL_UTILS_H

#include "base.h"
#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

int fklIsDecInt(const char *cstr, size_t maxLen);
int fklIsOctInt(const char *cstr, size_t maxLen);
int fklIsHexInt(const char *cstr, size_t maxLen);
int fklIsDecFloat(const char *cstr, size_t maxLen);
int fklIsHexFloat(const char *cstr, size_t maxLen);
int fklIsFloat(const char *cstr, size_t maxLen);
int fklIsAllDigit(const char *cstr, size_t maxLen);

int64_t fklStringToInt(const char *cstr, size_t maxLen, int *base);

int fklIsNumberString(const FklString *);
int fklIsNumberCstr(const char *);
int fklIsNumberCharBuf(const char *, size_t);

int fklPower(int, int);

void fklPrintCharLiteral(char, FILE *);
void fklPrintCharLiteral2(char, FklCodeBuilder *);

double fklStringToDouble(const FklString *);
size_t fklWriteDoubleToBuf(char *buf, size_t max, double f64);
size_t fklPrintDouble(double k, FILE *fp);

unsigned int fklGetByteNumOfUtf8(const uint8_t *byte, size_t max);

char *fklIntToCstr(int64_t);
FklString *fklIntToString(int64_t);

size_t fklCountCharInBuf(const char *, size_t s, char);

int fklIsValidCharBuf(const char *str, size_t len);
int fklCharBufToChar(const char *, size_t);

char *fklCastEscapeCharBuf(const char *str, size_t size, size_t *psize);

int fklIsScriptFile(const char *);
int fklIsByteCodeFile(const char *);
int fklIsPrecompileFile(const char *filename);

int fklGetDelim(FILE *fp, FklStrBuf *b, char d);

void *fklCopyMemory(const void *, size_t);

char *fklGetDir(const char *);
char **fklSplit(char *str, const char *divider, size_t *);
char *fklStrTok(char *str, const char *divstr, char **context);
char *fklTrim(char *str);
char *fklRealpath(const char *);
char *fklRelpath(const char *start, const char *path);

int fklIsI64AddOverflow(int64_t a, int64_t b);
int fklIsI64MulOverflow(int64_t a, int64_t b);

int fklIsFixAddOverflow(int64_t a, int64_t b);
int fklIsFixMulOverflow(int64_t a, int64_t b);

char *fklStrCat(char *, const char *);

char *fklCharBufToCstr(const char *buf, size_t size);

int fklChdir(const char *);
char *fklSysgetcwd(void);

int fklIsRegFile(const char *s);
int fklIsDirectory(const char *s);

int fklMkdir(const char *dir);
int fklIsAccessibleRegFile(const char *s);
int fklIsAccessibleDirectory(const char *s);

int fklRewindStream(FILE *fp, const char *buf, ssize_t len);
void fklStoreHistoryInStrBuf(FklStrBuf *buf, size_t offset);

#ifdef __cplusplus
}
#endif

#endif
