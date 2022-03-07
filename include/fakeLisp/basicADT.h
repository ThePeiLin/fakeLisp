#ifndef FKL_BASIC_ADT_H
#define FKL_BASIC_ADT_H
#include<stdint.h>

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

FklPtrQueue* fklNewPtrQueue();
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
#endif
