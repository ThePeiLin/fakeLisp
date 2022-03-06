#include<fakeLisp/basicADT.h>
#include<fakeLisp/utils.h>
#include<stdlib.h>
int fklIsPtrStackEmpty(FklPtrStack* stack)
{
	return stack->top==0;
}

FklPtrStack* fklNewPtrStack(uint32_t num)
{
	FklPtrStack* tmp=(FklPtrStack*)malloc(sizeof(FklPtrStack));
	FKL_ASSERT(tmp,"fklNewPtrStack",__FILE__,__LINE__);
	tmp->data=(void**)malloc(sizeof(void*)*num);
	FKL_ASSERT(tmp->data,"fklNewPtrStack",__FILE__,__LINE__);
	tmp->num=num;
	tmp->top=0;
	return tmp;
}

void fklPushPtrStack(void* data,FklPtrStack* stack)
{
	if(stack->top==stack->num)
	{
		void** tmpData=(void**)realloc(stack->data,(stack->num+32)*sizeof(void*));
		FKL_ASSERT(tmpData,"fklPushPtrStack",__FILE__,__LINE__);
		stack->data=tmpData;
		stack->num+=32;
	}
	stack->data[stack->top]=data;
	stack->top+=1;
}

void* fklPopPtrStack(FklPtrStack* stack)
{
	if(fklIsPtrStackEmpty(stack))
		return NULL;
	stack->top-=1;
	void* tmp=stack->data[stack->top];
	fklRecyclePtrStack(stack);
	return tmp;
}

void* fklTopPtrStack(FklPtrStack* stack)
{
	if(fklIsPtrStackEmpty(stack))
		return NULL;
	return stack->data[stack->top-1];
}

void fklFreePtrStack(FklPtrStack* stack)
{
	free(stack->data);
	free(stack);
}

void fklRecyclePtrStack(FklPtrStack* stack)
{
	if(stack->num-stack->top>32)
	{
		void** tmpData=(void**)realloc(stack->data,(stack->num-32)*sizeof(void*));
		FKL_ASSERT(tmpData,"fklRecyclePtrStack",__FILE__,__LINE__);
		stack->data=tmpData;
		stack->num-=32;
	}
}

FklQueueNode* fklNewQueueNode(void* data)
{
	FklQueueNode* tmp=(FklQueueNode*)malloc(sizeof(FklQueueNode));
	FKL_ASSERT(tmp,"fklNewQueueNode",__FILE__,__LINE__);
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
	FKL_ASSERT(tmp,"fklNewPtrQueue",__FILE__,__LINE__);
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


