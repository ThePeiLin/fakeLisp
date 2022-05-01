#ifndef FKL_SRC_UTILS_H
#define FKL_SRC_UTILS_H
#include<stdint.h>
#include<stddef.h>
#include<stdio.h>
#include<stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FKL_THRESHOLD_SIZE 256
#define FKL_MAX_STRING_SIZE 64
#define FKL_STATIC_SYMBOL_INIT {0,NULL,NULL}
#define FKL_UNUSEDBITNUM 3
#define FKL_PTR_MASK ((intptr_t)0xFFFFFFFFFFFFFFF8)
#define FKL_TAG_MASK ((intptr_t)0x7)

#define FKL_INCREASE_ALL_SCP(l,ls,s) {int32_t i=0;\
	for(;i<(ls);i++)\
	(l)[i]->scp+=(s);\
}

#define FKL_MIN(a,b) (((a)<(b))?(a):(b))

#define FKL_ASSERT(exp,str) \
{ \
	if(!(exp)) \
	{\
		fprintf(stderr,"In file \"%s\" line %d\n",__FILE__,__LINE__);\
		perror((str));\
		exit(1);\
	}\
}

#define FKL_VM_NIL ((FklVMptr)0x1)
#define FKL_VM_TRUE (FKL_MAKE_VM_I32(1))
#define FKL_VM_EOF ((FklVMptr)0x7fffffffa)
#define FKL_MAKE_VM_I32(I) ((FklVMptr)((((uintptr_t)(I))<<FKL_UNUSEDBITNUM)|FKL_I32_TAG))
#define FKL_MAKE_VM_CHR(C) ((FklVMptr)((((uintptr_t)(C))<<FKL_UNUSEDBITNUM)|FKL_CHR_TAG))
#define FKL_MAKE_VM_SYM(S) ((FklVMptr)((((uintptr_t)(S))<<FKL_UNUSEDBITNUM)|FKL_SYM_TAG))
#define FKL_MAKE_VM_PTR(P) ((FklVMptr)(((uintptr_t)(P))|FKL_PTR_TAG))
#define FKL_MAKE_VM_REF(P) ((FklVMptr)(((uintptr_t)(P))|FKL_REF_TAG))
#define FKL_MAKE_VM_MREF(S) ((FklVMptr)((((uintptr_t)(S))<<FKL_UNUSEDBITNUM)|FKL_MREF_TAG))
#define FKL_GET_MREF_SIZE(P) ((size_t)((uintptr_t)(P)>>FKL_UNUSEDBITNUM))
#define FKL_GET_TAG(P) ((FklVMptrTag)(((uintptr_t)(P))&FKL_TAG_MASK))
#define FKL_GET_PTR(P) ((FklVMptr)(((uintptr_t)(P))&FKL_PTR_MASK))
#define FKL_GET_I32(P) ((int32_t)((uintptr_t)(P)>>FKL_UNUSEDBITNUM))
#define FKL_GET_CHR(P) ((char)((uintptr_t)(P)>>FKL_UNUSEDBITNUM))
#define FKL_GET_SYM(P) ((FklSid_t)((uintptr_t)(P)>>FKL_UNUSEDBITNUM))
#define FKL_IS_PTR(P) (FKL_GET_TAG(P)==FKL_PTR_TAG)
#define FKL_IS_PAIR(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_PAIR)
#define FKL_IS_F64(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_F64)
#define FKL_IS_STR(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_STR)
#define FKL_IS_CHAN(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_CHAN)
#define FKL_IS_FP(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_FP)
#define FKL_IS_DLL(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_DLL)
#define FKL_IS_PROC(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_PROC)
#define FKL_IS_DLPROC(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_DLPROC)
#define FKL_IS_VECTOR(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_VECTOR)
#define FKL_IS_ERR(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_ERR)
#define FKL_IS_CONT(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_CONT)
#define FKL_IS_I32(P) (FKL_GET_TAG(P)==FKL_I32_TAG)
#define FKL_IS_CHR(P) (FKL_GET_TAG(P)==FKL_CHR_TAG)
#define FKL_IS_SYM(P) (FKL_GET_TAG(P)==FKL_SYM_TAG)
#define FKL_IS_REF(P) (FKL_GET_TAG(P)==FKL_REF_TAG)
#define FKL_IS_MREF(P) (FKL_GET_TAG(P)==FKL_MREF_TAG)
#define FKL_IS_I64(P) (FKL_GET_TAG(P)==FKL_PTR_TAG&&(P)->type==FKL_I64)
#define FKL_FREE_CHF(P) (free(FKL_GET_PTR(P)))

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
void fklPrintRawString(const char*,FILE*);
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
char* fklCopyStr(const char*);
void* fklCopyMemory(const void*,size_t);
int fklIsSymbolShouldBeExport(const char* str,const char** pStr,uint32_t n);
void fklPrintByteStr(size_t size,const uint8_t* str,FILE*,int);
void fklPrintAsByteStr(const uint8_t*,int32_t,FILE*);
void fklChangeWorkPath(const char*);
char* fklGetDir(const char*);
char* fklGetStringFromFile(FILE*);
char** fklSplit(char*,char*,int*);
char* fklRealpath(const char*);
char* fklRelpath(char*,char*);

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
