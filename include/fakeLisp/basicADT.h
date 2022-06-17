#ifndef FKL_BASIC_ADT_H
#define FKL_BASIC_ADT_H
#include<stdio.h>
#include<stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef struct
{
	void** table;
	size_t size;
	size_t inc;
}FklPtrHashTable;

FklPtrHashTable* fklNewPtrHashTable(size_t size,size_t inc);
void* fklGetHashPtr(uint64_t key);
void** fklGetHashSlot(uint64_t key);
uint64_t fklPtrHashFunc(uint64_t id);
void fklFreePtrHashTable(FklPtrHashTable*);

typedef struct
{
	void** base;
	uint32_t size;
	uint32_t top;
	uint32_t inc;
}FklPtrStack;

FklPtrStack* fklNewPtrStack(uint32_t size,uint32_t inc);
void fklPushPtrStack(void* data,FklPtrStack*);
void* fklPopPtrStack(FklPtrStack*);
void* fklTopPtrStack(FklPtrStack*);
void fklFreePtrStack(FklPtrStack*);
void fklRecyclePtrStack(FklPtrStack*);
int fklIsPtrStackEmpty(FklPtrStack*);

typedef struct FklQueueNode
{
	void* data;
	struct FklQueueNode* next;
}FklQueueNode;

typedef struct
{
	FklQueueNode* head;
	FklQueueNode* tail;
}FklPtrQueue;

FklQueueNode* fklNewQueueNode(void*);
void fklFreeQueueNode(FklQueueNode*);

FklPtrQueue* fklNewPtrQueue(void);
void fklFreePtrQueue(FklPtrQueue*);
int32_t fklLengthPtrQueue(FklPtrQueue*);
void* fklFirstPtrQueue(FklPtrQueue*);
void* fklPopPtrQueue(FklPtrQueue*);
void fklPushPtrQueue(void*,FklPtrQueue*);
FklPtrQueue* fklCopyPtrQueue(FklPtrQueue*);

typedef struct
{
	int64_t* base;
	uint32_t size;
	uint32_t top;
	uint32_t inc;
}FklIntStack;

FklIntStack* fklNewIntStack(uint32_t size,uint32_t inc);
void fklPushIntStack(int64_t e,FklIntStack*);
int64_t fklPopIntStack(FklIntStack*);
int64_t fklTopIntStack(FklIntStack*);
void fklFreeIntStack(FklIntStack*);
void fklRecycleIntStack(FklIntStack*);
int fklIsIntStackEmpty(FklIntStack*);

typedef struct
{
	uint64_t* base;
	uint32_t size;
	uint32_t top;
	uint32_t inc;
}FklUintStack;

FklUintStack* fklNewUintStack(uint32_t size,uint32_t inc);
void fklPushUintStack(uint64_t e,FklUintStack*);
uint64_t fklPopUintStack(FklUintStack*);
uint64_t fklTopUintStack(FklUintStack*);
void fklFreeUintStack(FklUintStack*);
void fklRecycleUintStack(FklUintStack*);
int fklIsUintStackEmpty(FklUintStack*);

typedef struct FklBigInt{
	uint8_t* digits;
	uint64_t num;
	uint64_t size;
	int neg;
}FklBigInt;

#define FKL_IS_0_BIG_INT(I) ((I)->num==1&&(I)->digits[0]==0)
#define FKL_IS_1_BIG_INT(I) ((I)->num==1&&(I)->digits[0]==1)
FklBigInt* fklNewBigInt(int64_t v);
FklBigInt* fklNewBigIntFromStr(const char* str);
FklBigInt* fklNewBigIntFromMem(void* mem,size_t size);
FklBigInt* fklNewBigInt0(void);
FklBigInt* fklNewBigInt1(void);
void fklInitBigInt(FklBigInt*,const FklBigInt*);
void fklInitBigIntI(FklBigInt*,int64_t v);
void fklSetBigInt(FklBigInt*,const FklBigInt* src);
void fklSetBigIntI(FklBigInt*,int64_t src);
void fklAddBigInt(FklBigInt*,const FklBigInt* addend);
void fklAddBigIntI(FklBigInt*,int64_t addend);
void fklSubBigInt(FklBigInt*,const FklBigInt* toSub);
void fklSubBigIntI(FklBigInt*,int64_t toSub);
void fklMulBigInt(FklBigInt*,const FklBigInt* multiplier);
void fklMulBigIntI(FklBigInt*,int64_t multiplier);
int fklIsGtLtI64BigInt(const FklBigInt* a);
int fklIsGtI64MaxBigInt(const FklBigInt* a);
int fklIsLtI64MinBigInt(const FklBigInt* a);
int fklIsGeLeI64BigInt(const FklBigInt* a);
int fklDivBigInt(FklBigInt*,const FklBigInt* divider);
int fklDivBigIntI(FklBigInt*,int64_t divider);
int fklRemBigInt(FklBigInt*,const FklBigInt* divider);
int fklRemBigIntI(FklBigInt*,int64_t divider);
int fklCmpBigInt(const FklBigInt*,const FklBigInt*);
int fklCmpBigIntI(const FklBigInt*,int64_t i);
int fklCmpIBigInt(int64_t i,const FklBigInt*);
int fklIsDivisibleBigInt(const FklBigInt* a,const FklBigInt* b);
int fklIsDivisibleBigIntI(const FklBigInt* a,int64_t);
int fklIsDivisibleIBigInt(int64_t a,const FklBigInt* b);
int64_t fklBigIntToI64(const FklBigInt*);
double fklBigIntToDouble(const FklBigInt*);
void fklFreeBigInt(FklBigInt*);
void fklPrintBigInt(FklBigInt*,FILE*);
void fklSprintBigInt(FklBigInt*,size_t size,char* buf);

typedef struct FklString
{
	uint64_t size;
	char str[];
}FklString;

FklString* fklNewString(size_t,const char*);

FklString* fklCopyString(const FklString*);
FklString* fklNewEmptyString();
void fklStringCat(FklString**,const FklString*);
int fklStringcmp(const FklString*,const FklString*);
char* fklStringToCstr(FklString* str);
void fklPrintRawString(FklString* str,FILE* fp);
#ifdef __cplusplus
}
#endif
#endif
