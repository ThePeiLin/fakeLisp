#ifndef FKL_BIGINT_H
#define FKL_BIGINT_H

#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>

// steal from cpython: https://github.com/python/cpython

typedef uint32_t FklBigIntDigit;
typedef int32_t FklBigIntSDigit;
typedef uint64_t FklBigIntTwoDigit;
typedef int64_t FklBigIntSTwoDigit;
#define FKL_BIGINT_DIGIT_SHIFT (30)
#define FKL_BIGINT_DIGIT_BASE ((FklBigIntDigit)1<<FKL_BIGINT_DIGIT_SHIFT)
#define FKL_BIGINT_DIGIT_MASK ((FklBigIntDigit)(FKL_BIGINT_DIGIT_BASE-1))
#define FKL_MAX_INT64_DIGITS_COUNT (3)


typedef struct FklBigInt
{
	FklBigIntDigit* digits;
	int64_t num;
	uint64_t size;
}FklBigInt;

typedef enum FklBigIntFmtFlags
{
	FKL_BIGINT_FMT_FLAG_NONE=0,
	FKL_BIGINT_FMT_FLAG_ALTERNATE=1<<0,
	FKL_BIGINT_FMT_FLAG_CAPITALS=1<<1,
}FklBigIntFmtFlags;

#define FKL_BIGINT_0 ((FklBigInt){.digits=NULL,.num=0,.size=0})
#define FKL_BIGINT_IS_0(I) (((I)->num==0)||(labs((I)->num)==1&&(I)->digits[0]==0))
#define FKL_BIGINT_IS_1(I) (((I)->num==1)&&((I)->digits[0]==1))
#define FKL_BIGINT_IS_N1(I) (((I)->num==-1)&&((I)->digits[0]==1))
#define FKL_BIGINT_IS_ABS1(I) ((labs((I)->num)==1)&&((I)->digits[0]==1))

FklBigInt* fklCreateBigInt0(void);
FklBigInt* fklCreateBigInt1(void);
FklBigInt* fklCreateBigIntN1(void);
FklBigInt* fklCreateBigIntI(int64_t v);
FklBigInt* fklCreateBigIntD(double v);
FklBigInt* fklCreateBigIntU(uint64_t v);
FklBigInt* fklCreateBigIntFromMemCopy(int64_t num,const FklBigIntDigit* mem);
FklBigInt* fklCreateBigIntFromMem(int64_t num,const FklBigIntDigit* mem);
FklBigInt* fklCreateBigIntWithCharBuf(const char* buf,size_t size);
FklBigInt* fklCreateBigIntWithDecCharBuf(const char* buf,size_t);
FklBigInt* fklCreateBigIntWithHexCharBuf(const char* buf,size_t);
FklBigInt* fklCreateBigIntWithOctCharBuf(const char* buf,size_t);
FklBigInt* fklCreateBigIntWithCstr(const char* str);

void fklDestroyBigInt(FklBigInt*);

FklBigInt* fklCopyBigInt(const FklBigInt*);

void fklInitBigInt(FklBigInt*,const FklBigInt*);
void fklInitBigInt0(FklBigInt*);
void fklInitBigInt1(FklBigInt*);
void fklInitBigIntN1(FklBigInt*);
void fklInitBigIntI(FklBigInt*,int64_t);
void fklInitBigIntU(FklBigInt*,uint64_t);
void fklInitBigIntD(FklBigInt*,double);
void fklInitBigIntFromMem(FklBigInt* t,int64_t num,FklBigIntDigit* mem);
void fklInitBigIntWithCharBuf(FklBigInt* t,const char*,size_t);
void fklInitBigIntWithDecCharBuf(FklBigInt* t,const char*,size_t);
void fklInitBigIntWithHexCharBuf(FklBigInt* t,const char*,size_t);
void fklInitBigIntWithOctCharBuf(FklBigInt* t,const char*,size_t);
void fklInitBigIntWithCstr(FklBigInt* t,const char*);

typedef struct
{
	FklBigIntDigit* (*alloc)(void* ctx,size_t);
	int64_t* (*num)(void* ctx);
}FklBigIntInitWithCharBufMethodTable;

void fklInitBigIntWithCharBuf2(void* ctx
		,const FklBigIntInitWithCharBufMethodTable* table
		,const char*
		,size_t);
void fklInitBigIntWithDecCharBuf2(void* ctx
		,const FklBigIntInitWithCharBufMethodTable* table
		,const char*
		,size_t);
void fklInitBigIntWithOctCharBuf2(void* ctx
		,const FklBigIntInitWithCharBufMethodTable* table
		,const char*
		,size_t);
void fklInitBigIntWithHexCharBuf2(void* ctx
		,const FklBigIntInitWithCharBufMethodTable* table
		,const char*
		,size_t);
void fklInitBigIntWithCstr2(FklBigInt* t,const char*);

void fklUninitBigInt(FklBigInt*);

uintptr_t fklBigIntHash(const FklBigInt* bi);
void fklSetBigInt(FklBigInt* to,const FklBigInt* from);
void fklSetBigIntI(FklBigInt* to,int64_t from);
void fklSetBigIntU(FklBigInt* to,uint64_t from);
void fklSetBigIntD(FklBigInt* to,double from);

void fklAddBigInt(FklBigInt* ,const FklBigInt* addend);
void fklAddBigIntI(FklBigInt* ,int64_t addend);
void fklSubBigInt(FklBigInt*,const FklBigInt* sub);
void fklSubBigIntI(FklBigInt*,int64_t sub);
void fklMulBigInt(FklBigInt*,const FklBigInt* mul);
void fklMulBigIntI(FklBigInt*,int64_t mul);
int fklDivBigInt(FklBigInt* a,const FklBigInt* d);
int fklDivBigIntI(FklBigInt* a,int64_t d);
int fklRemBigInt(FklBigInt* a,const FklBigInt* d);
int fklRemBigIntI(FklBigInt* a,int64_t d);
int fklDivRemBigInt(FklBigInt* a,const FklBigInt* divider,FklBigInt* rem);
int fklDivRemBigIntI(FklBigInt* a,int64_t divider,FklBigInt* rem);

int fklIsBigIntOdd(const FklBigInt*);
int fklIsBigIntEven(const FklBigInt*);
int fklIsBigIntLt0(const FklBigInt*);
int fklIsBigIntLe0(const FklBigInt*);
int fklIsBigIntGtLtI64(const FklBigInt*);
int fklIsDivisibleBigInt(const FklBigInt* a,const FklBigInt* b);
int fklIsDivisibleBigIntI(const FklBigInt* a,int64_t b);
int fklIsDivisibleIBigInt(int64_t a,const FklBigInt* b);
int fklBigIntEqual(const FklBigInt* a,const FklBigInt* b);
int fklBigIntCmp(const FklBigInt* a,const FklBigInt* b);
int fklBigIntCmpI(const FklBigInt* a,int64_t b);
int fklBigIntAbsCmp(const FklBigInt* a,const FklBigInt* b);

int64_t fklBigIntToI(const FklBigInt* a);
uint64_t fklBigIntToU(const FklBigInt* a);
double fklBigIntToD(const FklBigInt* a);

typedef char* (*FklBigIntToStrAllocCb)(void* ctx,size_t);

size_t fklBigIntToStr(const FklBigInt* a
		,FklBigIntToStrAllocCb alloc_cb
		,void* ctx
		,uint8_t radix
		,FklBigIntFmtFlags flags);

void fklPrintBigInt(const FklBigInt* a,FILE* fp);
char* fklBigIntToCstr(const FklBigInt* a,uint8_t radix,FklBigIntFmtFlags flags);
#endif
