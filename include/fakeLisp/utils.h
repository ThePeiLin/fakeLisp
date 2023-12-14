#ifndef FKL_SRC_UTILS_H
#define FKL_SRC_UTILS_H

#include"base.h"
#include<uv.h>
#include<stdint.h>
#include<stddef.h>
#include<stdio.h>
#include<stdlib.h>
#include<assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define FKL_PATH_SEPARATOR ('\\')
#define FKL_PATH_SEPARATOR_STR ("\\")
#define FKL_PATH_SEPARATOR_STR_LEN (1)

#define FKL_DLL_FILE_TYPE (".dll")
#define FKL_DLL_FILE_TYPE_STR_LEN (4)

#include <BaseTsd.h>
typedef SSIZE_T ssize_t;

#else
#define FKL_PATH_SEPARATOR ('/')
#define FKL_PATH_SEPARATOR_STR ("/")
#define FKL_PATH_SEPARATOR_STR_LEN (1)
#define FKL_DLL_FILE_TYPE (".so")
#define FKL_DLL_FILE_TYPE_STR_LEN (3)
#endif

#define FKL_PACKAGE_MAIN_FILE ("main.fkl")
#define FKL_PRE_COMPILE_PACKAGE_MAIN_FILE ("main.fklp")

#define FKL_SCRIPT_FILE_EXTENSION (".fkl")
#define FKL_BYTECODE_FILE_EXTENSION (".fklc")
#define FKL_PRE_COMPILE_FILE_EXTENSION (".fklp")

#define FKL_BYTECODE_FKL_SUFFIX_STR ("c")
#define FKL_PRE_COMPILE_FKL_SUFFIX_STR ("p")

#define FKL_BYTECODE_FKL_SUFFIX ('c')
#define FKL_PRE_COMPILE_FKL_SUFFIX ('p')

#define FKL_DEFAULT_INC (32)
#define FKL_MAX_STRING_SIZE (64)
#define FKL_STATIC_SYMBOL_INIT {0,NULL,NULL}
#define FKL_MIN(a,b) (((a)<(b))?(a):(b))
#define FKL_MAX(a,b) (((a)>(b))?(a):(b))
#define FKL_ASSERT(exp) assert(exp)

#define FKL_ESCAPE_CHARS ("ABTNVFRS")
#define FKL_ESCAPE_CHARS_TO ("\a\b\t\n\v\f\r\x20")

int fklIsDecInt(const char* cstr,size_t maxLen);
int fklIsOctInt(const char* cstr,size_t maxLen);
int fklIsHexInt(const char* cstr,size_t maxLen);
int fklIsDecFloat(const char* cstr,size_t maxLen);
int fklIsHexFloat(const char* cstr,size_t maxLen);
int fklIsFloat(const char* cstr,size_t maxLen);
int fklIsAllDigit(const char* cstr,size_t maxLen);

int64_t fklStringToInt(const char* cstr,size_t maxLen,int* base);

int fklIsNumberString(const FklString*);
int fklIsNumberCstr(const char*);
int fklIsNumberCharBuf(const char*,size_t);

char* fklGetStringFromList(const char*);
char* fklGetStringAfterBackslash(const char*);
char* fklGetStringAfterBackslashInStr(const char* str);
int fklPower(int,int);

int fklIsSpecialCharAndPrint(uint8_t ch,FILE* out);
void fklPrintRawCharBuf(const uint8_t* str,char se,size_t size,FILE* out);
void fklPrintRawCstring(const char*,char se,FILE*);
void fklPrintRawChar(char,FILE*);
void fklPrintRawByteBuf(const uint8_t* ptr,size_t size,FILE* out);
void fklPrintCharBufInHex(const char* buf,uint32_t len,FILE* fp);

double fklStringToDouble(const FklString*);
size_t fklWriteDoubleToBuf(char* buf,size_t max,double f64);

unsigned int fklGetByteNumOfUtf8(const uint8_t* byte,size_t max);
int fklWriteCharAsCstr(char,char*,size_t);
char* fklIntToCstr(int64_t);
FklString* fklIntToString(int64_t);

size_t fklCountCharInBuf(const char*,size_t s,char);

int fklIsValidCharBuf(const char* str,size_t len);
int fklCharBufToChar(const char*,size_t);

char* fklCastEscapeCharBuf(const char* str,size_t size,size_t* psize);

int fklIsScriptFile(const char*);
int fklIsByteCodeFile(const char*);
int fklIsPrecompileFile(const char* filename);

int fklGetDelim(FILE* fp,FklStringBuffer* b,char d);

char* fklCopyCstr(const char*);
void* fklCopyMemory(const void*,size_t);
int fklIsSymbolShouldBeExport(const FklString* str,const FklString** pStr,uint32_t n);
char* fklGetDir(const char*);
char* fklGetStringFromFile(FILE*);
char** fklSplit(char* str,const char* divider,size_t*);
char* fklRealpath(const char*);
char* fklRelpath(const char* start,const char* path);

int fklIsI64AddOverflow(int64_t a,int64_t b);
int fklIsI64MulOverflow(int64_t a,int64_t b);

int fklIsFixAddOverflow(int64_t a,int64_t b);
int fklIsFixMulOverflow(int64_t a,int64_t b);

char* fklStrCat(char*,const char*);
uint8_t* fklCreateByteArry(int32_t);

char* fklCharBufToCstr(const char* buf,size_t size);

int fklChdir(const char*);
char* fklSysgetcwd(void);

int fklIsRegFile(const char* s);
int fklIsDirectory(const char* s);

int fklMkdir(const char* dir);
int fklIsAccessibleRegFile(const char* s);
int fklIsAccessibleDirectory(const char* s);

int fklRewindStream(FILE* fp,const char* buf,ssize_t len);

void* fklRealloc(void* ptr,size_t nsize);

#ifdef __cplusplus
}
#endif

#endif
