#include<fakeLisp/basicADT.h>
#include<fakeLisp/utils.h>
#include<stdlib.h>
int fklIsPtrStackEmpty(FklPtrStack* stack)
{
	return stack->top==0;
}

FklPtrStack* fklNewPtrStack(uint32_t size,uint32_t inc)
{
	FklPtrStack* tmp=(FklPtrStack*)malloc(sizeof(FklPtrStack));
	FKL_ASSERT(tmp,__func__);
	tmp->base=(void**)malloc(sizeof(void*)*size);
	FKL_ASSERT(tmp->base,__func__);
	tmp->size=size;
	tmp->inc=inc;
	tmp->top=0;
	return tmp;
}

void fklPushPtrStack(void* data,FklPtrStack* stack)
{
	if(stack->top==stack->size)
	{
		void** tmpData=(void**)realloc(stack->base,(stack->size+stack->inc)*sizeof(void*));
		FKL_ASSERT(tmpData,__func__);
		stack->base=tmpData;
		stack->size+=stack->inc;
	}
	stack->base[stack->top]=data;
	stack->top+=1;
}

void* fklPopPtrStack(FklPtrStack* stack)
{
	if(fklIsPtrStackEmpty(stack))
		return NULL;
	stack->top-=1;
	void* tmp=stack->base[stack->top];
	fklRecyclePtrStack(stack);
	return tmp;
}

void* fklTopPtrStack(FklPtrStack* stack)
{
	if(fklIsPtrStackEmpty(stack))
		return NULL;
	return stack->base[stack->top-1];
}

void fklFreePtrStack(FklPtrStack* stack)
{
	free(stack->base);
	free(stack);
}

void fklRecyclePtrStack(FklPtrStack* stack)
{
	if(stack->size-stack->top>stack->inc)
	{
		void** tmpData=(void**)realloc(stack->base,(stack->size-stack->inc)*sizeof(void*));
		FKL_ASSERT(tmpData,__func__);
		stack->base=tmpData;
		stack->size-=stack->inc;
	}
}

FklQueueNode* fklNewQueueNode(void* data)
{
	FklQueueNode* tmp=(FklQueueNode*)malloc(sizeof(FklQueueNode));
	FKL_ASSERT(tmp,__func__);
	tmp->data=data;
	tmp->next=NULL;
	return tmp;
}

void fklFreeQueueNode(FklQueueNode* tmp)
{
	free(tmp);
}

FklPtrQueue* fklNewPtrQueue()
{
	FklPtrQueue* tmp=(FklPtrQueue*)malloc(sizeof(FklPtrQueue));
	FKL_ASSERT(tmp,__func__);
	tmp->head=NULL;
	tmp->tail=NULL;
	return tmp;
}

void fklFreePtrQueue(FklPtrQueue* tmp)
{
	FklQueueNode* cur=tmp->head;
	while(cur)
	{
		FklQueueNode* prev=cur;
		cur=cur->next;
		fklFreeQueueNode(prev);
	}
	free(tmp);
}

int32_t fklLengthPtrQueue(FklPtrQueue* tmp)
{
	FklQueueNode* cur=tmp->head;
	int32_t i=0;
	for(;cur;cur=cur->next,i++);
	return i;
}

void* fklPopPtrQueue(FklPtrQueue* tmp)
{
	FklQueueNode* head=tmp->head;
	if(!head)
		return NULL;
	void* retval=head->data;
	tmp->head=head->next;
	if(!tmp->head)
		tmp->tail=NULL;
	fklFreeQueueNode(head);
	return retval;
}

void* fklFirstPtrQueue(FklPtrQueue* tmp)
{
	FklQueueNode* head=tmp->head;
	if(!head)
		return NULL;
	return head->data;
}

void fklPushPtrQueue(void* data,FklPtrQueue* tmp)
{
	FklQueueNode* tmpNode=fklNewQueueNode(data);
	if(!tmp->head)
	{
		tmp->head=tmpNode;
		tmp->tail=tmpNode;
	}
	else
	{
		tmp->tail->next=tmpNode;
		tmp->tail=tmpNode;
	}
}

FklPtrQueue* fklCopyPtrQueue(FklPtrQueue* q)
{
	FklQueueNode* head=q->head;
	FklPtrQueue* tmp=fklNewPtrQueue();
	for(;head;head=head->next)
		fklPushPtrQueue(head->data,tmp);
	return tmp;
}

int fklIsIntStackEmpty(FklIntStack* stack)
{
	return stack->top==0;
}

FklIntStack* fklNewIntStack(uint32_t size,uint32_t inc)
{
	FklIntStack* tmp=(FklIntStack*)malloc(sizeof(FklIntStack));
	FKL_ASSERT(tmp,__func__);
	tmp->base=(int64_t*)malloc(sizeof(int64_t)*size);
	FKL_ASSERT(tmp->base,__func__);
	tmp->size=size;
	tmp->inc=inc;
	tmp->top=0;
	return tmp;
}

void fklPushIntStack(int64_t data,FklIntStack* stack)
{
	if(stack->top==stack->size)
	{
		int64_t* tmpData=(int64_t*)realloc(stack->base,(stack->size+stack->inc)*sizeof(int64_t));
		FKL_ASSERT(tmpData,__func__);
		stack->base=tmpData;
		stack->size+=stack->inc;
	}
	stack->base[stack->top]=data;
	stack->top+=1;
}

int64_t fklPopIntStack(FklIntStack* stack)
{
	if(fklIsIntStackEmpty(stack))
		return 0;
	stack->top-=1;
	int64_t tmp=stack->base[stack->top];
	fklRecycleIntStack(stack);
	return tmp;
}

int64_t fklTopIntStack(FklIntStack* stack)
{
	if(fklIsIntStackEmpty(stack))
		return 0;
	return stack->base[stack->top-1];
}

void fklFreeIntStack(FklIntStack* stack)
{
	free(stack->base);
	free(stack);
}

void fklRecycleIntStack(FklIntStack* stack)
{
	if(stack->size-stack->top>stack->inc)
	{
		int64_t* tmpData=(int64_t*)realloc(stack->base,(stack->size-stack->inc)*sizeof(int64_t));
		FKL_ASSERT(tmpData,__func__);
		stack->base=tmpData;
		stack->size-=stack->inc;
	}
}

int fklIsUintStackEmpty(FklUintStack* stack)
{
	return stack->top==0;
}

FklUintStack* fklNewUintStack(uint32_t size,uint32_t inc)
{
	FklUintStack* tmp=(FklUintStack*)malloc(sizeof(FklUintStack));
	FKL_ASSERT(tmp,__func__);
	tmp->base=(uint64_t*)malloc(sizeof(uint64_t)*size);
	FKL_ASSERT(tmp->base,__func__);
	tmp->size=size;
	tmp->inc=inc;
	tmp->top=0;
	return tmp;
}

void fklPushUintStack(uint64_t data,FklUintStack* stack)
{
	if(stack->top==stack->size)
	{
		uint64_t* tmpData=(uint64_t*)realloc(stack->base,(stack->size+stack->inc)*sizeof(uint64_t));
		FKL_ASSERT(tmpData,__func__);
		stack->base=tmpData;
		stack->size+=stack->inc;
	}
	stack->base[stack->top]=data;
	stack->top+=1;
}

uint64_t fklPopUintStack(FklUintStack* stack)
{
	if(fklIsUintStackEmpty(stack))
		return 0;
	stack->top-=1;
	uint64_t tmp=stack->base[stack->top];
	fklRecycleUintStack(stack);
	return tmp;
}

uint64_t fklTopUintStack(FklUintStack* stack)
{
	if(fklIsUintStackEmpty(stack))
		return 0;
	return stack->base[stack->top-1];
}

void fklFreeUintStack(FklUintStack* stack)
{
	free(stack->base);
	free(stack);
}

void fklRecycleUintStack(FklUintStack* stack)
{
	if(stack->size-stack->top>stack->inc)
	{
		uint64_t* tmpData=(uint64_t*)realloc(stack->base,(stack->size-stack->inc)*sizeof(uint64_t));
		FKL_ASSERT(tmpData,__func__);
		stack->base=tmpData;
		stack->size-=stack->inc;
	}
}


