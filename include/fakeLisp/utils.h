#ifndef FKL_SRC_UTILS_H
#define FKL_SRC_UTILS_H
#include"base.h"
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
#define FKL_DLL_FILE_TYPE (".dll")
#include<windows.h>
typedef HMODULE FklDllHandle;
#else
#define FKL_PATH_SEPARATOR ('/')
#define FKL_PATH_SEPARATOR_STR ("/")
#define FKL_DLL_FILE_TYPE (".so")
#include<dlfcn.h>
typedef void* FklDllHandle;
#endif

#define FKL_THRESHOLD_SIZE (512)
#define FKL_MAX_STRING_SIZE (64)
#define FKL_STATIC_SYMBOL_INIT {0,NULL,NULL}
#define FKL_MIN(a,b) (((a)<(b))?(a):(b))
#define FKL_MAX(a,b) (((a)>(b))?(a):(b))
#define FKL_ASSERT(exp) assert(exp)

int fklIsHexNumString(const FklString*);
int fklIsOctNumString(const FklString*);
int fklIsDoubleString(const FklString*);
int fklIsNumberString(const FklString*);

int fklIsHexNumCstr(const char*);
int fklIsOctNumCstr(const char*);
int fklIsDoubleCstr(const char*);
int fklIsNumberCstr(const char*);

int fklIsHexNumCharBuf(const char*,size_t);
int fklIsOctNumCharBuf(const char*,size_t);
int fklIsDoubleCharBuf(const char*,size_t);
int fklIsNumberCharBuf(const char*,size_t);

char* fklGetStringFromList(const char*);
char* fklGetStringAfterBackslash(const char*);
char* fklGetStringAfterBackslashInStr(const char* str);
int fklPower(int,int);
//char* fklCastEscapeCharater(const char*,char,size_t*);
void fklPrintRawCharBuf(const uint8_t* str,char se,size_t size,FILE* out);
void fklPrintRawCstring(const char*,char se,FILE*);
void fklPrintRawChar(char,FILE*);
void fklPrintRawByteBuf(const uint8_t* ptr,size_t size,FILE* out);

char* fklDoubleToCstr(double);
FklString* fklDoubleToString(double);
double fklCstrToDouble(const char*);
double fklStringToDouble(const FklString*);

unsigned int fklGetByteNumOfUtf8(const uint8_t* byte,size_t max);
int fklWriteCharAsCstr(char,char*,size_t);
char* fklIntToCstr(int64_t);
FklString* fklIntToString(int64_t);
//int64_t fklCstrToInt(const char*);
//int64_t fklStringToInt(const FklString*);

int32_t fklCountChar(const char*,char,int32_t);
size_t fklCountCharInBuf(const char*,size_t s,char);
int fklStringToChar(const FklString*);
int fklCharBufToChar(const char*,size_t);

char* fklCastEscapeCharBuf(const char* str,size_t size,size_t* psize);

uint8_t fklCastCharInt(char);
int fklIsscript(const char*);
int fklIscode(const char*);
int fklIsAllSpace(const char*);
char* fklCopyCstr(const char*);
void* fklCopyMemory(const void*,size_t);
int fklIsSymbolShouldBeExport(const FklString* str,const FklString** pStr,uint32_t n);
int fklChangeWorkPath(const char*);
char* fklGetDir(const char*);
char* fklGetStringFromFile(FILE*);
char** fklSplit(char*,char*,int*);
char* fklRealpath(const char*);
char* fklRelpath(const char*,const char*);

int fklIsI64AddOverflow(int64_t a,int64_t b);
int fklIsI64MulOverflow(int64_t a,int64_t b);
void mergeSort(void* base
		,size_t num
		,size_t size
		,int (*cmpf)(const void*,const void*));

char* fklStrCat(char*,const char*);
uint8_t* fklCreateByteArry(int32_t);

char* fklCharBufToCstr(const char* buf,size_t size);

const char* fklGetMainFileRealPath(void);
const char* fklGetCwd(void);
void fklSetCwd(const char*);
void fklDestroyCwd(void);
void fklSetMainFileRealPath(const char* path);
void fklDestroyMainFileRealPath(void);
void fklSetMainFileRealPathWithCwd(void);

int fklIsRegFile(const char* s);
int fklIsDirectory(const char* s);

int fklIsAccessableRegFile(const char* s);
int fklIsAccessableDirectory(const char* s);

FklDllHandle fklLoadDll(const char* path);
void* fklGetAddress(const char*,FklDllHandle);
void fklDestroyDll(FklDllHandle);
#ifdef __cplusplus
}
#endif

#endif
