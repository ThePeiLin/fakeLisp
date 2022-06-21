#ifndef FKL_SRC_UTILS_H
#define FKL_SRC_UTILS_H
#include<stdint.h>
#include<stddef.h>
#include<stdio.h>
#include<stdlib.h>
#include<fakeLisp/basicADT.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define FKL_PATH_SEPARATOR '\\'
#else
#define FKL_PATH_SEPARATOR '/'
#endif

#ifdef _WIN32
#define FKL_PATH_SEPARATOR_STR "\\"
#else
#define FKL_PATH_SEPARATOR_STR "/"
#endif

#define FKL_THRESHOLD_SIZE (512)
#define FKL_MAX_STRING_SIZE (64)
#define FKL_STATIC_SYMBOL_INIT {0,NULL,NULL}
#define FKL_INCREASE_ALL_SCP(l,ls,s) {int32_t i=0;\
	for(;i<(ls);i++)\
	(l)[i]->scp+=(s);\
}

#define FKL_MIN(a,b) (((a)<(b))?(a):(b))
#define FKL_MAX(a,b) (((a)>(b))?(a):(b))
#define FKL_ASSERT(exp,str) \
{ \
	if(!(exp)) \
	{\
		fprintf(stderr,"In file \"%s\" line %d\n",__FILE__,__LINE__);\
		perror((str));\
		exit(1);\
	}\
}

int fklIsHexNum(const char*);
int fklIsOctNum(const char*);
int fklIsDouble(const char*);
int fklIsNum(const char*);
char* fklGetStringFromList(const char*);
char* fklGetStringAfterBackslash(const char*);
char* fklGetStringAfterBackslashInStr(const char* str);
int fklPower(int,int);
char* fklCastEscapeCharater(const char*,char,size_t*);
void fklPrintRawCharBuf(const char* str,size_t size,FILE* out);
void fklPrintRawCstring(const char*,FILE*);
void fklPrintRawChar(char,FILE*);
char* fklDoubleToString(double);
double fklStringToDouble(const char*);
char* fklIntToString(long);
int64_t fklStringToInt(const char*);
int32_t fklCountChar(const char*,char,int32_t);
int fklStringToChar(const char*);
uint8_t* fklCastStrByteStr(const char*);
uint8_t fklCastCharInt(char);
int fklIsscript(const char*);
int fklIscode(const char*);
int fklIsAllSpace(const char*);
char* fklCopyCStr(const char*);
void* fklCopyMemory(const void*,size_t);
int fklIsSymbolShouldBeExport(const FklString* str,const FklString** pStr,uint32_t n);
void fklPrintByteStr(size_t size,const uint8_t* str,FILE*,int);
void fklPrintAsByteStr(const uint8_t*,int32_t,FILE*);
void fklChangeWorkPath(const char*);
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

char* fklCharBufToStr(const char* buf,size_t size);
#ifdef __cplusplus
}
#endif

#endif
