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
	char str[];
}FklString;

FklString* fklCreateString(size_t,const char*);
FklString* fklCopyString(const FklString*);
FklString* fklCreateEmptyString();
FklString* fklCreateStringFromCstr(const char*);
size_t fklCountCharInString(FklString* s,char c);
void fklStringCat(FklString**,const FklString*);
void fklStringCstrCat(FklString**,const char*);
void fklStringCharBufCat(FklString**,const char* buf,size_t s);
char* fklCstrStringCat(char*,const FklString*);
int fklStringcmp(const FklString*,const FklString*);
int fklStringCstrCmp(const FklString*,const char*);
char* fklStringToCstr(const FklString* str);
void fklPrintRawString(const FklString* str,FILE* fp);
void fklPrintString(const FklString* str,FILE* fp);
void fklPrintRawSymbol(const FklString* str,FILE* fp);
FklString* fklStringAppend(const FklString*,const FklString*);
void fklDestroyStringArray(FklString**,uint32_t num);
void fklWriteStringToCstr(char*,const FklString*);

FklString* fklStringToRawString(const FklString* str);
FklString* fklStringToRawSymbol(const FklString* str);

typedef struct FklBytevector
{
	uint64_t size;
	uint8_t ptr[];
}FklBytevector;

FklBytevector* fklCreateBytevector(size_t,const uint8_t*);
FklBytevector* fklCopyBytevector(const FklBytevector*);
void fklBytevectorCat(FklBytevector**,const FklBytevector*);
int fklBytevectorcmp(const FklBytevector*,const FklBytevector*);
void fklPrintRawBytevector(const FklBytevector* str,FILE* fp);
FklString* fklBytevectorToString(const FklBytevector*);

typedef struct FklHashTableNode
{
	struct FklHashTableNode* next;
	uint8_t data[];
}FklHashTableNode;

typedef struct FklHashTableNodeList
{
	FklHashTableNode* node;
	struct FklHashTableNodeList* next;
}FklHashTableNodeList;

typedef struct FklHashTable
{
	FklHashTableNode** base;
	FklHashTableNodeList* list;
	FklHashTableNodeList** tail;
	uint32_t num;
	uint32_t size;
	uint32_t mask;
	const struct FklHashTableMetaTable* t;
	//double threshold;
	//size_t linkNum;
	//int thresholdInc;  //size+=size*(1.0/thresholdInc)
	//int linkNumInc;   //size+=size*(1.0/linkNumInc)
}FklHashTable;

typedef struct FklHashTableMetaTable
{
	uint8_t size;
	void (*__setKey)(void*,void*);
	void (*__setVal)(void*,void*);
	uintptr_t (*__hashFunc)(void*);
	void (*__uninitItem)(void*);
	int (*__keyEqual)(void*,void*);
	void* (*__getKey)(void*);
}FklHashTableMetaTable;

#define FKL_HASH_GET_KEY(name,type,field) void* name(void* item){return &((type*)item)->field;}

FklHashTable* fklCreateHashTable(
		//size_t size
		//,size_t linkNum
		//,int linkNumInc
		//,double threshold
		//,int thresholdInc
		//,
		const FklHashTableMetaTable*);

//FklHashTable* fklCreateDefaultHashTable(size_t size,FklHashTableMetaTable* t);

void* fklGetHashItem(void* key,FklHashTable*);
void* fklPutHashItem(void* key,FklHashTable*);
void* fklGetOrPutHashItem(void* data,FklHashTable*);
void fklDestroyHashTable(FklHashTable*);

void fklDoNothingUnintHashItem(void*);
void* fklHashDefaultGetKey(void* i);
int fklHashPtrKeyEqual(void* a,void* b);
void fklHashDefaultSetPtrKey(void* k0,void* k1);

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
	int neg;
}FklBigInt;

#define FKL_BIG_INT_INIT {NULL,0,0,0}
#define FKL_IS_0_BIG_INT(I) ((I)->num==1&&(I)->digits[0]==0)
#define FKL_IS_1_BIG_INT(I) ((I)->num==1&&(I)->neg==0&&(I)->digits[0]==1)
#define FKL_IS_N_1_BIG_INT(I) ((I)->num==1&&(I)->neg==0&&(I)->digits[0]==1)
//#define FKL_IS_1_N_1_BIG_INT(I) ((I)->num==1&&(I)->digits[0]==1)

FklBigInt* fklCreateBigInt(int64_t v);
FklBigInt* fklCreateBigIntD(double v);
FklBigInt* fklCreateBigIntU(uint64_t v);
FklBigInt* fklCreateBigIntFromCstr(const char* str);
FklBigInt* fklCreateBigIntFromMem(const void* mem,size_t size);
FklBigInt* fklCreateBigIntFromString(const FklString*);
FklBigInt* fklCreateBigInt0(void);
FklBigInt* fklCreateBigInt1(void);
FklBigInt* fklCopyBigInt(const FklBigInt*);

void fklInitBigInt(FklBigInt*,const FklBigInt*);
void fklInitBigIntI(FklBigInt*,int64_t v);
void fklInitBigInt0(FklBigInt*);
void fklInitBigInt1(FklBigInt*);
void fklInitBigIntU(FklBigInt*,uint64_t v);
void fklInitBigIntFromMem(FklBigInt*,const void*mem,size_t size);
void fklInitBigIntFromString(FklBigInt*,const FklString*);
void fklUninitBigInt(FklBigInt*);

void fklSetBigInt(FklBigInt*,const FklBigInt* src);
void fklSetBigIntI(FklBigInt*,int64_t src);
void fklSetBigIntU(FklBigInt*,uint64_t src);

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
void fklSprintBigInt(const FklBigInt*,size_t size,char* buf);
FklString* fklBigIntToString(const FklBigInt*,int radix);
void fklBigIntToRadixDigitsLe(const FklBigInt* u,uint32_t radix,FklU8Stack* res);

#ifdef __cplusplus
}
#endif
#endif
