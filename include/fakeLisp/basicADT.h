#ifndef FKL_BASIC_ADT_H
#define FKL_BASIC_ADT_H
#include<stdint.h>

typedef enum
{
	FKL_SYMUNDEFINE=1,
	FKL_SYNTAXERROR,
	FKL_INVALIDEXPR,
	FKL_INVALIDTYPEDEF,
	FKL_CIRCULARLOAD,
	FKL_INVALIDPATTERN,
	FKL_WRONGARG,
	FKL_STACKERROR,
	FKL_TOOMANYARG,
	FKL_TOOFEWARG,
	FKL_CANTCREATETHREAD,
	FKL_THREADERROR,
	FKL_MACROEXPANDFAILED,
	FKL_INVOKEERROR,
	FKL_LOADDLLFAILD,
	FKL_INVALIDSYMBOL,
	FKL_LIBUNDEFINED,
	FKL_UNEXPECTEOF,
	FKL_DIVZERROERROR,
	FKL_FILEFAILURE,
	FKL_CANTDEREFERENCE,
	FKL_CANTGETELEM,
	FKL_INVALIDMEMBER,
	FKL_NOMEMBERTYPE,
	FKL_NONSCALARTYPE,
	FKL_INVALIDASSIGN,
	FKL_INVALIDACCESS,
}FklErrorType;


typedef struct
{
	void** data;
	uint32_t num;
	long int top;
}FklPtrStack;

FklPtrStack* fklNewPtrStack(uint32_t size);
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

FklPtrQueue* fklNewPtrQueue();
void fklFreePtrQueue(FklPtrQueue*);
int32_t fklLengthPtrQueue(FklPtrQueue*);
void* fklFirstPtrQueue(FklPtrQueue*);
void* fklPopPtrQueue(FklPtrQueue*);
void fklPushPtrQueue(void*,FklPtrQueue*);
FklPtrQueue* fklCopyPtrQueue(FklPtrQueue*);

#endif
