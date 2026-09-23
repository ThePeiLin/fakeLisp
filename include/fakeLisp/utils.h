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

FKL_API int fklIsDecInt(const char *cstr, size_t maxLen);
FKL_API int fklIsOctInt(const char *cstr, size_t maxLen);
FKL_API int fklIsHexInt(const char *cstr, size_t maxLen);
FKL_API int fklIsDecFloat(const char *cstr, size_t maxLen);
FKL_API int fklIsHexFloat(const char *cstr, size_t maxLen);
FKL_API int fklIsFloat(const char *cstr, size_t maxLen);
FKL_API int fklIsAllDigit(const char *cstr, size_t maxLen);

FKL_API int64_t fklStringToInt(const char *cstr, size_t maxLen, int *base);

FKL_API int fklIsNumberString(const FklString *);
FKL_API int fklIsNumberCstr(const char *);
FKL_API int fklIsNumberCharBuf(const char *, size_t);

FKL_API int fklPower(int, int);

FKL_API void fklPrintCharLiteral(int, FILE *);
FKL_API void fklPrintCharLiteral2(int, FklCodeBuilder *);

FKL_API double fklStringToDouble(const FklString *);
FKL_API size_t fklWriteDoubleToBuf(char *buf, size_t max, double f64);
FKL_API size_t fklPrintDouble(double k, FILE *fp);

FKL_API unsigned int fklGetByteNumOfUtf8(const uint8_t *byte, size_t max);

FKL_API char *fklIntToCstr(int64_t);
FKL_API FklString *fklIntToString(int64_t);

FKL_API size_t fklCountCharInBuf(const char *, size_t s, char);

FKL_API int fklIsValidCharBuf(const char *str, size_t len);
FKL_API int fklCharBufToChar(const char *, size_t);

FKL_API char *fklCastEscapeCharBuf(const char *str, size_t size, size_t *psize);

FKL_API int fklIsScriptFile(const char *);
FKL_API int fklIsByteCodeFile(const char *);
FKL_API int fklIsPrecompileFile(const char *filename);

FKL_API int fklGetDelim(FILE *fp, FklStrBuf *b, int d);

FKL_API void *fklCopyMemory(const void *, size_t);

FKL_API char *fklTruncDir(char *path);
FKL_API char *fklDupDir(const char *);
FKL_API char **fklSplit(char *str, const char *divider, size_t *);
FKL_API char *fklStrTok(char *str, const char *divstr, char **context);
FKL_API char *fklTrim(char *str);

FKL_API char *fklRealpath(const char *);
FKL_API char *fklAbspath(const char *);

FKL_API char *fklRelpath(const char *start, const char *path);

FKL_API int fklIsI64AddOverflow(int64_t a, int64_t b);
FKL_API int fklIsI64MulOverflow(int64_t a, int64_t b);

FKL_API int fklIsFixAddOverflow(int64_t a, int64_t b);
FKL_API int fklIsFixMulOverflow(int64_t a, int64_t b);

FKL_API char *fklStrCat(char *, const char *);

FKL_API char *fklStrstr(const char *haystack, const char *needle);
// steal from glib: https://gitlab.gnome.org/GNOME/glib
FKL_API char *fklStrrstr(const char *haystack, const char *needle);
FKL_API char *fklStrEndWith(const char *str, const char *sub);
FKL_API int fklStrStartWith(const char *haystack, const char *needle);

FKL_API char *fklCharBufToCstr(const char *buf, size_t size);

FKL_API int fklChdir(const char *);
FKL_API char *fklSysgetcwd(void);

FKL_API int fklIsRegFile(const char *s);
FKL_API int fklIsDirectory(const char *s);

FKL_API int fklMkdir(const char *dir);
FKL_API int fklIsAccessibleRegFile(const char *s);
FKL_API int fklIsAccessibleDirectory(const char *s);

FKL_API int fklRewindStream(FILE *fp, const char *buf, ssize_t len);
FKL_API void fklStoreHistoryInStrBuf(FklStrBuf *buf, size_t offset);

FKL_API int fklLoadLines(FklStringVector *lines, FILE *in);

FKL_API const char *fklSysGetEnv(const char *name);
FKL_API int fklSysSetEnv(const char *name, const char *value, int overwrite);
FKL_API int fklSysUnsetEnv(const char *name);

FKL_ALWAYS_INLINE
static int fklComputeDigitsCount(uint64_t len) {
    if (len == 0)
        return 1;
    int sum = 0;
    while (len > 0) {
        ++sum;
        len /= 10;
    }

    return sum;
}

#ifdef __cplusplus
}
#endif

#endif
