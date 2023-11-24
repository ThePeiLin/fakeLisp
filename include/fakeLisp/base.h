#ifndef FKL_BASIC_ADT_H
#define FKL_BASIC_ADT_H

#include<stdio.h>
#include<stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

#define FKL_FIX_INT_MAX (1152921504606846975)
#define FKL_FIX_INT_MIN (-1152921504606846975-1)

typedef struct FklString
{
	uint64_t size;
	char str[1];
}FklString;

FklString* fklCreateString(size_t,const char*);
FklString* fklCopyString(const FklString*);
FklString* fklCreateEmptyString(void);
FklString* fklCreateStringFromCstr(const char*);
size_t fklCountCharInString(FklString* s,char c);
void fklStringCat(FklString**,const FklString*);
void fklStringCstrCat(FklString**,const char*);
void fklStringCharBufCat(FklString**,const char* buf,size_t s);
char* fklCstrStringCat(char*,const FklString*);
int fklStringCmp(const FklString*,const FklString*);
int fklStringEqual(const FklString* fir,const FklString* sec);

int fklStringCharBufCmp(const FklString*,size_t len,const char* buf);
int fklStringCstrCmp(const FklString*,const char*);
size_t fklStringCharBufMatch(const FklString*,const char*,size_t len);

size_t fklCharBufMatch(const char* s0,size_t l0,const char* s1,size_t l1);

char* fklStringToCstr(const FklString* str);
void fklPrintRawString(const FklString* str,FILE* fp);
void fklPrintString(const FklString* str,FILE* fp);
void fklPrintRawSymbol(const FklString* str,FILE* fp);
FklString* fklStringAppend(const FklString*,const FklString*);
void fklWriteStringToCstr(char*,const FklString*);

uintptr_t fklStringHash(const FklString* s);

uintptr_t fklCharBufHash(const char* str,size_t len);

struct FklStringBuffer;
void fklPrintRawCstrToStringBuffer(struct FklStringBuffer* s,const char* str,char se);
void fklPrintRawStringToStringBuffer(struct FklStringBuffer* s,const FklString* fstr,char se);
FklString* fklStringToRawString(const FklString* str);
FklString* fklStringToRawSymbol(const FklString* str);

typedef struct FklStringBuffer
{
	uint32_t size;
	uint32_t index;
	char* buf;
}FklStringBuffer;

#define FKL_STRING_BUFFER_INIT {0,0,NULL}

FklStringBuffer* fklCreateStringBuffer(void);
char* fklStringBufferBody(FklStringBuffer*);
uint32_t fklStringBufferLen(FklStringBuffer*);
void fklInitStringBuffer(FklStringBuffer*);
void fklInitStringBufferWithCapacity(FklStringBuffer*,size_t s);
void fklStringBufferReverse(FklStringBuffer*,size_t s);
void fklUninitStringBuffer(FklStringBuffer*);
void fklDestroyStringBuffer(FklStringBuffer*);
void fklStringBufferClear(FklStringBuffer*);
void fklStringBufferFill(FklStringBuffer*,char);
void fklStringBufferBincpy(FklStringBuffer*,const void*,size_t);
void fklStringBufferConcatWithCstr(FklStringBuffer*,const char*);
void fklStringBufferConcatWithString(FklStringBuffer*,const FklString*);
void fklStringBufferConcatWithStringBuffer(FklStringBuffer*,const FklStringBuffer*);
void fklStringBufferPutc(FklStringBuffer*,char);
void fklStringBufferPrintf(FklStringBuffer*,const char* fmt,...);
FklString* fklStringBufferToString(FklStringBuffer*);
int fklIsSpecialCharAndPrintToStringBuffer(FklStringBuffer* s,char ch);

typedef struct FklBytevector
{
	uint64_t size;
	uint8_t ptr[];
}FklBytevector;
FklBytevector* fklStringBufferToBytevector(FklStringBuffer*);

FklBytevector* fklCreateBytevector(size_t,const uint8_t*);
FklBytevector* fklCopyBytevector(const FklBytevector*);
void fklBytevectorCat(FklBytevector**,const FklBytevector*);
int fklBytevectorCmp(const FklBytevector*,const FklBytevector*);
void fklPrintRawBytevector(const FklBytevector* str,FILE* fp);
FklString* fklBytevectorToString(const FklBytevector*);
void fklPrintBytevectorToStringBuffer(FklStringBuffer*,const FklBytevector* bvec);

uintptr_t fklBytevectorHash(const FklBytevector* bv);

typedef struct FklHashTableItem
{
	struct FklHashTableItem* prev;
	struct FklHashTableItem* next;
	struct FklHashTableItem* ni;
	uint8_t data[];
}FklHashTableItem;

typedef struct FklHashTable
{
	const struct FklHashTableMetaTable* t;
	FklHashTableItem** base;
	FklHashTableItem* first;
	FklHashTableItem* last;
	uint32_t num;
	uint32_t size;
	uint32_t mask;
}FklHashTable;

typedef struct FklHashTableMetaTable
{
	uint8_t size;
	void (*__setKey)(void*,const void*);
	void (*__setVal)(void*,const void*);
	uintptr_t (*__hashFunc)(const void*);
	void (*__uninitItem)(void*);
	int (*__keyEqual)(const void*,const void*);
	void* (*__getKey)(void*);
}FklHashTableMetaTable;

#define FKL_HASH_GET_KEY(name,type,field) void* name(void* item){return &((type*)item)->field;}
#define FKL_DEFAULT_HASH_LOAD_FACTOR (2.0)

void fklInitHashTable(FklHashTable*,const FklHashTableMetaTable*);
FklHashTable* fklCreateHashTable(const FklHashTableMetaTable*);

void fklPtrKeySet(void* k0,const void* k1);
uintptr_t fklPtrKeyHashFunc(const void* k);
int fklPtrKeyEqual(const void* k0,const void* k1);

void fklInitPtrSet(FklHashTable*);
FklHashTable* fklCreatPtrSet(void);

void* fklGetOrPutWithOtherKey(void* pkey
	,uintptr_t (*hashv)(const void* key)
	,int (*keq)(const void* k0,const void* k1)
	,void (*setVal)(void* d0,const void* d1)
	,FklHashTable* ht);

void* fklGetHashItem(const void* key,const FklHashTable*);
void* fklPutHashItem(const void* key,FklHashTable*);
void* fklGetOrPutHashItem(void* data,FklHashTable*);
int fklDelHashItem(void* key,FklHashTable*,void* v);
void fklRehashTable(FklHashTable* table);
void fklClearHashTable(FklHashTable* ht);
void fklUninitHashTable(FklHashTable*);
void fklDestroyHashTable(FklHashTable*);

void fklDoNothingUninitHashItem(void*);
void* fklHashDefaultGetKey(void* i);

#define FKL_STACK_INIT {NULL,0,0,0}
typedef struct
{
	void** base;
	uint32_t size;
	uint32_t top;
	uint32_t inc;
}FklPtrStack;

void fklInitPtrStack(FklPtrStack*,uint32_t size,uint32_t inc);
void fklUninitPtrStack(FklPtrStack*);
FklPtrStack* fklCreatePtrStack(uint32_t size,uint32_t inc);
void fklPushPtrStack(void* data,FklPtrStack*);
void fklPushFrontPtrStack(void* data,FklPtrStack*);

void* fklPopPtrStack(FklPtrStack*);
void* fklTopPtrStack(FklPtrStack*);
void fklDestroyPtrStack(FklPtrStack*);
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
	FklQueueNode** tail;
}FklPtrQueue;

FklQueueNode* fklCreateQueueNode(void*);
void fklDestroyQueueNode(FklQueueNode*);

void fklInitPtrQueue(FklPtrQueue* q);
void fklUninitPtrQueue(FklPtrQueue* q);
FklPtrQueue* fklCreatePtrQueue(void);
void fklDestroyPtrQueue(FklPtrQueue*);
int fklIsPtrQueueEmpty(FklPtrQueue*);
uint64_t fklLengthPtrQueue(FklPtrQueue*);
void* fklFirstPtrQueue(FklPtrQueue*);
void* fklPopPtrQueue(FklPtrQueue*);
void fklPushPtrQueue(void*,FklPtrQueue*);
void fklPushPtrQueueToFront(void*,FklPtrQueue*);
FklPtrQueue* fklCopyPtrQueue(FklPtrQueue*);

typedef struct
{
	int64_t* base;
	uint32_t size;
	uint32_t top;
	uint32_t inc;
}FklIntStack;

FklIntStack* fklCreateIntStack(uint32_t size,uint32_t inc);
void fklPushIntStack(int64_t e,FklIntStack*);
int64_t fklPopIntStack(FklIntStack*);
int64_t fklTopIntStack(FklIntStack*);
void fklDestroyIntStack(FklIntStack*);
void fklRecycleIntStack(FklIntStack*);
int fklIsIntStackEmpty(FklIntStack*);

typedef struct
{
	uint64_t* base;
	uint32_t size;
	uint32_t top;
	uint32_t inc;
}FklUintStack;

void fklInitUintStack(FklUintStack*,uint32_t size,uint32_t inc);
void fklUninitUintStack(FklUintStack*);
FklUintStack* fklCreateUintStack(uint32_t size,uint32_t inc);
void fklInitUintStackWithStack(FklUintStack*,const FklUintStack*);
FklUintStack* fklCreateUintStackFromStack(const FklUintStack*);
void fklPushUintStack(uint64_t e,FklUintStack*);
uint64_t fklPopUintStack(FklUintStack*);
uint64_t fklTopUintStack(FklUintStack*);
void fklDestroyUintStack(FklUintStack*);
void fklRecycleUintStack(FklUintStack*);
int fklIsUintStackEmpty(FklUintStack*);

typedef struct
{
	uint8_t* base;
	size_t size;
	size_t top;
	uint32_t inc;
}FklU8Stack;

void fklInitU8Stack(FklU8Stack*,size_t size,uint32_t inc);
void fklUninitU8Stack(FklU8Stack*);
FklU8Stack* fklCreateU8Stack(size_t size,uint32_t inc);
void fklPushU8Stack(uint8_t e,FklU8Stack*);
uint8_t fklPopU8Stack(FklU8Stack*);
uint8_t fklTopU8Stack(FklU8Stack*);
void fklDestroyU8Stack(FklU8Stack*);
void fklRecycleU8Stack(FklU8Stack*);
int fklIsU8StackEmpty(FklU8Stack*);

typedef struct FklBigInt
{
	uint8_t* digits;
	uint64_t num;
	uint64_t size;
	uint8_t neg;
}FklBigInt;

#define FKL_BIG_INT_INIT {NULL,0,0,0}
#define FKL_IS_0_BIG_INT(I) ((I)->num==1&&(I)->digits[0]==0)
#define FKL_IS_1_BIG_INT(I) ((I)->num==1&&(I)->neg==0&&(I)->digits[0]==1)
#define FKL_IS_N_1_BIG_INT(I) ((I)->num==1&&(I)->neg==1&&(I)->digits[0]==1)
#define FKL_IS_1_N_1_BIG_INT(I) ((I)->num==1&&(I)->digits[0]==1)

FklBigInt* fklCreateBigInt(int64_t v);
FklBigInt* fklCreateBigIntD(double v);
FklBigInt* fklCreateBigIntU(uint64_t v);
FklBigInt* fklCreateBigIntFromCstr(const char* str);
FklBigInt* fklCreateBigIntFromMemCopy(uint8_t,const uint8_t* mem,size_t size);
FklBigInt* fklCreateBigIntFromMem(uint8_t neg,uint8_t* mem,size_t size);

FklBigInt* fklCreateBigIntFromString(const FklString*);
FklBigInt* fklCreateBigInt0(void);
FklBigInt* fklCreateBigInt1(void);
FklBigInt* fklCopyBigInt(const FklBigInt*);

void fklInitBigInt(FklBigInt*,const FklBigInt*);
void fklInitBigIntI(FklBigInt*,int64_t v);
void fklInitBigInt0(FklBigInt*);
void fklInitBigInt1(FklBigInt*);
void fklInitBigIntU(FklBigInt*,uint64_t v);

void fklInitBigIntFromMemCopy(FklBigInt*,uint8_t,const uint8_t* mem,size_t size);
void fklInitBigIntFromMem(FklBigInt* t,uint8_t neg,uint8_t* memptr,size_t num);

void fklInitBigIntFromString(FklBigInt*,const FklString*);
void fklInitBigIntFromDecString(FklBigInt*,const FklString*);
void fklInitBigIntFromHexString(FklBigInt*,const FklString*);
void fklInitBigIntFromOctString(FklBigInt*,const FklString*);

void fklUninitBigInt(FklBigInt*);

void fklSetBigInt(FklBigInt*,const FklBigInt* src);
void fklSetBigIntI(FklBigInt*,int64_t src);
void fklSetBigIntU(FklBigInt*,uint64_t src);
void fklSetBigIntD(FklBigInt*,double);

void fklAddBigInt(FklBigInt*,const FklBigInt* addend);
void fklAddBigIntI(FklBigInt*,int64_t addend);
void fklSubBigInt(FklBigInt*,const FklBigInt* toSub);
void fklSubBigIntI(FklBigInt*,int64_t toSub);
void fklMulBigInt(FklBigInt*,const FklBigInt* multiplier);
void fklMulBigIntI(FklBigInt*,int64_t multiplier);

int fklIsGtLtFixBigInt(const FklBigInt* a);
int fklIsGeLeFixBigInt(const FklBigInt* a);

int fklIsGtLtI64BigInt(const FklBigInt* a);
int fklIsGtI64MaxBigInt(const FklBigInt* a);
int fklIsGtU64MaxBigInt(const FklBigInt* a);
int fklIsLtI64MinBigInt(const FklBigInt* a);
int fklIsGeLeI64BigInt(const FklBigInt* a);

int fklIsBigIntOdd(const FklBigInt* a);
int fklIsBigIntEven(const FklBigInt* a);
int fklIsBigIntLt0(const FklBigInt* a);
int fklIsBigIntAdd1InFixIntRange(const FklBigInt* a);
int fklIsBigIntSub1InFixIntRange(const FklBigInt* a);

int fklDivModBigInt(FklBigInt*,FklBigInt*,const FklBigInt* divider);
int fklDivModBigIntI(FklBigInt*,int64_t*,int64_t divider);
int fklDivModBigIntU(FklBigInt*,uint64_t*,uint64_t divider);
int fklDivBigInt(FklBigInt*,const FklBigInt* divider);
int fklDivBigIntI(FklBigInt*,int64_t divider);
int fklDivBigIntU(FklBigInt*,uint64_t divider);
int fklModBigInt(FklBigInt*,const FklBigInt* divider);
int fklModBigIntI(FklBigInt*,int64_t divider);
int fklModBigIntU(FklBigInt*,uint64_t divider);
int fklCmpBigInt(const FklBigInt*,const FklBigInt*);
int fklCmpBigIntI(const FklBigInt*,int64_t i);
int fklCmpBigIntU(const FklBigInt*,uint64_t i);
int fklCmpIBigInt(int64_t i,const FklBigInt*);
int fklCmpUBigInt(uint64_t i,const FklBigInt*);
int fklIsDivisibleBigInt(const FklBigInt* a,const FklBigInt* b);
int fklIsDivisibleBigIntI(const FklBigInt* a,int64_t);
int fklIsDivisibleIBigInt(int64_t a,const FklBigInt* b);
int64_t fklBigIntToI64(const FklBigInt*);
uint64_t fklBigIntToU64(const FklBigInt*);
double fklBigIntToDouble(const FklBigInt*);
void fklDestroyBigInt(FklBigInt*);
void fklPrintBigInt(const FklBigInt*,FILE*);
FklString* fklBigIntToString(const FklBigInt*,int radix);
void fklBigIntToRadixDigitsLe(const FklBigInt* u,uint32_t radix,FklU8Stack* res);

#ifdef __cplusplus
}
#endif
#endif
