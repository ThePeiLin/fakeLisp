#ifndef FKL_BASIC_ADT_H
#define FKL_BASIC_ADT_H
#include<stdio.h>
#include<stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

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

typedef struct FklHashTableNode
{
	void* item;
	struct FklHashTableNode* next;
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
	size_t num;
	size_t size;
	struct FklHashTableMethodTable* t;
	double threshold;
	int thresholdInc;
	size_t linkNum;
	int linkNumInc;
}FklHashTable;

typedef struct FklHashTableMethodTable
{
	size_t (*__hashFunc)(void*);
	void (*__destroyItem)(void*);
	int (*__keyEqual)(void*,void*);
	void* (*__getKey)(void*);
}FklHashTableMethodTable;

FklHashTable* fklCreateHashTable(size_t size
		,size_t linkNum
		,int linkNumInc
		,double threshold
		,int thresholdInc
		,FklHashTableMethodTable*);
void* fklGetHashItem(void* key,FklHashTable*);
void* fklPutInReverseOrder(void* item,FklHashTable* table);
void* fklPutReplHashItem(void* item,FklHashTable*); //put and replace;
void* fklPutNoRpHashItem(void* item,FklHashTable* table); //put and no replace
void fklDestroyHashTable(FklHashTable*);
void fklRehashTable(FklHashTable*,unsigned int);

typedef struct
{
	void** base;
	uint32_t size;
	uint32_t top;
	uint32_t inc;
}FklPtrStack;

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
	size_t size;
	uint32_t top;
	uint32_t inc;
}FklUintStack;

void fklInitUintStack(FklUintStack*,uint32_t size,uint32_t inc);
void fklUninitUintStack(FklUintStack*);
FklUintStack* fklCreateUintStack(uint32_t size,uint32_t inc);
FklUintStack* fklCreateUintStackFromStack(FklUintStack*);
void fklPushUintStack(uint64_t e,FklUintStack*);
uint64_t fklPopUintStack(FklUintStack*);
uint64_t fklTopUintStack(FklUintStack*);
void fklDestroyUintStack(FklUintStack*);
void fklRecycleUintStack(FklUintStack*);
int fklIsUintStackEmpty(FklUintStack*);

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
void fklSetBigInt(FklBigInt*,const FklBigInt* src);
void fklSetBigIntI(FklBigInt*,int64_t src);
void fklSetBigIntU(FklBigInt*,uint64_t src);
void fklAddBigInt(FklBigInt*,const FklBigInt* addend);
void fklAddBigIntI(FklBigInt*,int64_t addend);
void fklSubBigInt(FklBigInt*,const FklBigInt* toSub);
void fklSubBigIntI(FklBigInt*,int64_t toSub);
void fklMulBigInt(FklBigInt*,const FklBigInt* multiplier);
void fklMulBigIntI(FklBigInt*,int64_t multiplier);
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

#ifdef __cplusplus
}
#endif
#endif
