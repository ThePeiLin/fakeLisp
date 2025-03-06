#ifndef FKL_BIGINT_H
#define FKL_BIGINT_H

#include"base.h"
#include<stdint.h>
#include<stdlib.h>

typedef uint32_t NfklBigIntDigit;
typedef int32_t NfklBigIntSDigit;
typedef uint64_t NfklBigIntTwoDigit;

typedef struct NfklBigInt
{
	NfklBigIntDigit* digits;
	int64_t num;
	uint64_t size;
}NfklBigInt;

#define NFKL_BIGINT_0 ((NfklBigInt){.digits=NULL,.num=0,.size=0})
#define NFKL_BIGINT_IS_0(I) (((I)->num==0)||(labs((I)->num)==1&&(I)->digits[0]==0))
#define NFKL_BIGINT_IS_1(I) (((I)->num==1)&&((I)->digits[0]==1))
#define NFKL_BIGINT_IS_N1(I) (((I)->num==-1)&&((I)->digits[0]==1))
#define NFKL_BIGINT_IS_ABS1(I) ((labs((I)->num)==1)&&((I)->digits[0]==1))

NfklBigInt* nfklCreateBigInt0(void);
NfklBigInt* nfklCreateBigInt1(void);
NfklBigInt* nfklCreateBigIntN1(void);
NfklBigInt* nfklCreateBigIntI(int64_t v);
NfklBigInt* nfklCreateBigIntD(double v);
NfklBigInt* nfklCreateBigIntU(uint64_t v);
NfklBigInt* nfklCreateBigIntFromMemCopy(int64_t num,const NfklBigIntDigit* mem);
NfklBigInt* nfklCreateBigIntFromMem(int64_t num,const NfklBigIntDigit* mem);
NfklBigInt* nfklCreateBigIntWithCharBuf(const char* buf,size_t size);
NfklBigInt* nfklCreateBigIntWithDecCharBuf(const char* buf,size_t);
NfklBigInt* nfklCreateBigIntWithHexCharBuf(const char* buf,size_t);
NfklBigInt* nfklCreateBigIntWithOctCharBuf(const char* buf,size_t);
NfklBigInt* nfklCreateBigIntWithCstr(const char* str);

void nfklDestroyBigInt(NfklBigInt*);

NfklBigInt* nfklCopyBigInt(const NfklBigInt*);

void nfklInitBigInt0(NfklBigInt*);
void nfklInitBigInt1(NfklBigInt*);
void nfklInitBigIntN1(NfklBigInt*);
void nfklInitBigIntWithOther(NfklBigInt*,const NfklBigInt*);
void nfklInitBigIntI(NfklBigInt*,int64_t);
void nfklInitBigIntU(NfklBigInt*,uint64_t);
void nfklInitBigIntD(NfklBigInt*,double);
void nfklInitBigIntFromMemCopy(NfklBigInt* t,int64_t num,const uint8_t* mem);
void nfklInitBigIntFromMem(NfklBigInt* t,int64_t num,uint8_t* mem);
void nfklInitBigIntWithCharBuf(NfklBigInt* t,const char*,size_t);
void nfklInitBigIntWithDecCharBuf(NfklBigInt* t,const char*,size_t);
void nfklInitBigIntWithHexCharBuf(NfklBigInt* t,const char*,size_t);
void nfklInitBigIntWithOctCharBuf(NfklBigInt* t,const char*,size_t);
void nfklInitBigIntWithCstr(NfklBigInt* t,const char*);

void nfklUninitBigInt(NfklBigInt*);

uintptr_t nfklBigIntHash(const NfklBigInt* bi);
void nfklSetBigInt(NfklBigInt* to,const NfklBigInt* from);
void nfklSetBigIntI(NfklBigInt* to,int64_t from);
void nfklSetBigIntU(NfklBigInt* to,uint64_t from);
void nfklSetBigIntD(NfklBigInt* to,double from);

void nfklAddBigInt(NfklBigInt* ,const NfklBigInt* addend);
void nfklAddBigIntI(NfklBigInt* ,int64_t addend);
void nfklSubBigInt(NfklBigInt*,const NfklBigInt* sub);
void nfklSubBigIntI(NfklBigInt*,int64_t sub);
void nfklMulBigInt(NfklBigInt*,const NfklBigInt* mul);
void nfklMulBigIntI(NfklBigInt*,int64_t mul);

int nfklIsBigIntOdd(const NfklBigInt*);
int nfklIsBigIntEven(const NfklBigInt*);
int nfklIsBigIntLt0(const NfklBigInt*);
int nfklIsBigIntLe0(const NfklBigInt*);
int nfklBigIntEqual(const NfklBigInt* a,const NfklBigInt* b);
int nfklBigIntCmp(const NfklBigInt* a,const NfklBigInt* b);
int nfklBigIntAbsCmp(const NfklBigInt* a,const NfklBigInt* b);

int64_t nfklBigIntToI(const NfklBigInt* a);
uint64_t nfklBigIntToU(const NfklBigInt* a);
double nfklBigIntToD(const NfklBigInt* a);

typedef char* (*NfklBigIntToStrAllocCb)(void* ctx,size_t);

size_t nfklBigIntToStr(const NfklBigInt* a
		,NfklBigIntToStrAllocCb alloc_cb
		,void* ctx
		,uint8_t radix
		,int with_prefix
		,int capitals);

size_t nfklBigIntToStringBuffer(const NfklBigInt* a
		,FklStringBuffer* string_buffer
		,uint8_t radix
		,int with_prefix
		,int capitals);

#endif
