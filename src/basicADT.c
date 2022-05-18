#include<fakeLisp/basicADT.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/bytecode.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>
#include<ctype.h>
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

FklPtrQueue* fklNewPtrQueue(void)
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

FklBigInt* fklNewBigInt(int64_t v)
{
	if(v==0)
		return fklNewBigInt0();
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	double d=v;
	FKL_ASSERT(t,__func__);
	if(v<0)
	{
		t->neg=1;
		d*=-1;
	}
	else
		t->neg=0;
	t->num=floor(log10(d))+1;
	if(t->num==0)
		t->num=1;
	t->size=t->num;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*t->num);
	FKL_ASSERT(t->digits,__func__);
	for(uint64_t i=0;i<t->num;i++)
	{
		t->digits[i]=v%10;
		if(t->neg)
			t->digits[i]*=-1;
		v/=10;
	}
	return t;
}

FklBigInt* fklNewBigInt0(void)
{
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t,__func__);
	t->neg=0;
	t->num=1;
	t->size=t->num;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*t->num);
	FKL_ASSERT(t->digits,__func__);
	t->digits[0]=0;
	return t;
}

FklBigInt* fklNewBigInt1(void)
{
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t,__func__);
	t->neg=0;
	t->num=1;
	t->size=t->num;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*t->num);
	FKL_ASSERT(t->digits,__func__);
	t->digits[0]=1;
	return t;
}

FklBigInt* fklNewBigIntFromStr(const char* v)
{
	FklBigInt* r=fklNewBigInt0();
	FKL_ASSERT(r,__func__);
	if(fklIsHexNum(v))
	{
		uint64_t num=0;
		int neg=v[0]=='-';
		v+=2+neg;
		uint64_t i=0;
		FklBigInt* base=fklNewBigInt1();
		for(;isxdigit(v[i]);i++,num++);
		for(;i>0;i--)
		{
			FklBigInt* t=fklNewBigInt(isdigit(v[i-1])
					?v[i-1]-'0'
					:isupper(v[i-1])
					?v[i-1]-'A'
					:v[i-1]-'a');
			fklMulBigInt(t,base);
			fklMulBigIntI(base,16);
			fklAddBigInt(r,t);
			fklFreeBigInt(t);
		}
		fklFreeBigInt(base);
		r->neg=neg;
	}
	else if(fklIsOctNum(v))
	{
		uint64_t num=0;
		int neg=v[0]=='-';
		v+=neg;
		uint64_t i=0;
		FklBigInt* base=fklNewBigInt1();
		for(;isdigit(v[i])&&v[i]<'9';i++,num++);
		for(;i>0;i--)
		{
			FklBigInt* t=fklNewBigInt(v[i-1]-'0');
			fklMulBigInt(t,base);
			fklMulBigIntI(base,8);
			fklAddBigInt(r,t);
			fklFreeBigInt(t);
		}
		fklFreeBigInt(base);
		r->neg=neg;
	}
	else
	{
		uint64_t num=0;
		int neg=v[0]=='-';
		v+=neg;
		uint64_t i=0;
		FklBigInt* base=fklNewBigInt1();
		for(;isdigit(v[i]);i++,num++);
		for(;i>0;i--)
		{
			FklBigInt* t=fklNewBigInt(v[i-1]-'0');
			fklMulBigInt(t,base);
			fklMulBigIntI(base,10);
			fklAddBigInt(r,t);
			fklFreeBigInt(t);
		}
		fklFreeBigInt(base);
		r->neg=neg;
	}
	return r;
}

FklBigInt* fklNewBigIntFromMem(void* mem)
{
	int neg=((uint8_t*)mem)[0];
	mem++;
	uint64_t num=fklGetU64FromByteCode(mem);
	mem+=sizeof(num);
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t,__func__);
	t->num=num;
	t->size=num;
	t->neg=neg;
	t->digits=fklCopyMemory(mem,num*sizeof(uint8_t));
	return t;
}

void fklFreeBigInt(FklBigInt* t)
{
	free(t->digits);
	free(t);
}

static void ensureBigIntDigits(FklBigInt* obj,uint64_t num)
{
	if(obj->size<num)
	{
		uint8_t* digits=obj->digits;
		obj->digits=(uint8_t*)realloc(digits,sizeof(char)*num);
		FKL_ASSERT(obj->digits,__func__);
		obj->size=num;
	}
}

void fklSetBigInt(FklBigInt* des,const FklBigInt* src)
{
	ensureBigIntDigits(des,src->num);
	memcpy(des->digits,src->digits,src->num);
	des->neg=src->neg;
	des->num=src->num;
}

void fklSetBigIntI(FklBigInt* des,int64_t src)
{
	if(src<0)
	{
		des->neg=1;
		src*=-1;
	}
	else
		des->neg=0;
	des->num=floor(log10(src))+1;
	if(des->num==0)
		des->num=1;
	ensureBigIntDigits(des,des->num);
	for(uint64_t i=0;i<des->num;i++)
	{
		des->digits[i]=src%10;
		src/=10;
	}
}

static int cmpDigits(const FklBigInt* a,const FklBigInt* b)
{
	if(a->num>b->num)
		return 1;
	else if(a->num<b->num)
		return -1;
	for(uint64_t i=a->num;i>0;i--)
		if(a->digits[i-1]>b->digits[i-1])
			return 1;
		else if(a->digits[i-1]<b->digits[i-1])
			return -1;
	return 0;
}

int fklCmpBigInt(const FklBigInt* a,const FklBigInt* b)
{
	if(a->num>0||a->digits[0]>0||b->num>0||b->digits[0]>0)
	{
		if(a->neg&&!b->neg)
			return -1;
		else if(!a->neg&&b->neg)
			return 1;
	}
	return a->neg?cmpDigits(b,a):cmpDigits(a,b);
}

static void addDigits(FklBigInt* a,const FklBigInt* addend)
{
	uint64_t nDigit =FKL_MAX(a->num,addend->num)+1;
	ensureBigIntDigits(a,nDigit);
	int carry=0;
	for(uint64_t i=0;i<addend->num||carry>0;i++)
	{
		if(i==a->num)
		{
			a->num++;
			a->digits[i]=0;
		}
		uint8_t addendDigit=i<addend->num?addend->digits[i]:0;
		uint8_t total=a->digits[i]+addendDigit+carry;
		a->digits[i]=total%10;
		carry=(total>=10)?1:0;
	}
}

static void subDigits(FklBigInt* a,const FklBigInt* sub)
{

	uint64_t nDigit = FKL_MAX(a->num, sub->num) + 1;
	ensureBigIntDigits(a,nDigit);
	uint8_t* greaterIntDigits=NULL;
	uint8_t* smallerIntDigits=NULL;
	uint8_t greaterIntNum=0;
	uint8_t smallerIntNum=0;
	if(cmpDigits(a,sub) > 0)
	{
		greaterIntDigits=a->digits;
		greaterIntNum=a->num;
		smallerIntDigits=sub->digits;
		smallerIntNum=sub->num;
	}
	else
	{
		greaterIntDigits=sub->digits;
		greaterIntNum=sub->num;
		smallerIntDigits=a->digits;
		smallerIntNum=a->num;
	}
	int carry=0;
	a->num=1;
	for(uint64_t i=0;i<greaterIntNum;i++)
	{
		int tmp=0;
		if(i<smallerIntNum)
			tmp=(int)greaterIntDigits[i]-(int)smallerIntDigits[i]+carry;
		else
			tmp=(int)greaterIntDigits[i]+carry;
		if(tmp<0)
		{
			carry=-1;
			tmp+=10;
		}
		else
			carry=0;
		FKL_ASSERT(tmp>=0,__func__);
		a->digits[i]=tmp;
		if(tmp!=0)
			a->num=i+1;
	}
	FKL_ASSERT(carry==0,__func__);
}

void fklAddBigInt(FklBigInt* a,const FklBigInt* addend)
{
	if(a->neg==addend->neg)
		addDigits(a,addend);
	else
	{
		int neg=cmpDigits(a,addend)>0?a->neg:addend->neg;
		subDigits(a,addend);
		a->neg=neg;
	}
}

void fklAddBigIntI(FklBigInt* a,int64_t addendI)
{
	FklBigInt* addend=fklNewBigInt(addendI);
	fklAddBigInt(a,addend);
	fklFreeBigInt(addend);
}

void fklSubBigInt(FklBigInt* a,const FklBigInt* sub)
{
	int neg=fklCmpBigInt(a,sub)<0;
	if(a->neg==sub->neg)
		subDigits(a,sub);
	else
		addDigits(a, sub);
	a->neg=neg;
}

void fklSubBigIntI(FklBigInt* a,int64_t sub)
{
	FklBigInt* toSub=fklNewBigInt(sub);
	fklSubBigInt(a,toSub);
	fklFreeBigInt(toSub);
}

void fklMulBigInt(FklBigInt* a,const FklBigInt* multipler)
{
	if(a->num==1&&a->digits[0]==0)
		return;
	else if(multipler->num==1&&multipler->digits[0]==0)
	{
		a->num=1;
		a->digits[0]=0;
		return;
	}
	else
	{
		uint64_t totalNum=a->num+multipler->num;
		uint64_t num=0;
		uint8_t* res=(uint8_t*)calloc(totalNum,sizeof(uint8_t));
		FKL_ASSERT(res,__func__);
		for(uint64_t i=0;i<a->num;i++)
		{
			int n0=a->digits[i];
			for(uint64_t j=0;j<multipler->num;j++)
			{
				int n1=multipler->digits[j];
				int sum=(res[i+j]+n0*n1);
				res[i+j]=sum%10;
				res[i+j+1]=sum/10;
				num++;
				if(sum/10)
					num++;
			}
		}
		free(a->digits);
		a->digits=res;
		a->num=num;
		a->size=totalNum;
		a->neg^=multipler->neg;
	}
}

void fklMulBigIntI(FklBigInt* a,int64_t mul)
{
	FklBigInt* multipler=fklNewBigInt(mul);
	fklMulBigInt(a,multipler);
	fklFreeBigInt(multipler);
}

int fklDivBigInt(FklBigInt* a,const FklBigInt* divider)
{
	if(divider->num==1&&divider->digits[0]==0)
		return 1;
	int cmpR=cmpDigits(a,divider);
	if(cmpR<0)
	{
		a->num=1;
		a->digits[0]=0;
	}
	else if(!cmpR)
	{
		a->num=1;
		a->digits[0]=1;
	}
	else
	{
		uint8_t* res=(uint8_t*)malloc(sizeof(uint8_t)*a->num);
		FKL_ASSERT(res,__func__);
		uint64_t num=0;
		FklBigInt s={NULL,0,a->num,0};
		s.digits=(uint8_t*)malloc(sizeof(uint8_t)*(divider->num+1));
		FKL_ASSERT(s.digits,__func__);
		for(uint64_t i=0;i<a->num;i++)
		{
			s.digits[i]=a->digits[i];
			s.num++;
			int count=0;
			for(;cmpDigits(&s,divider)>0;fklSubBigInt(&s,divider),count++);
			if(count!=0)
			{
				res[num]=count;
				num++;
			}
		}
		free(s.digits);
		free(a->digits);
		a->digits=res;
		a->size=a->num;
		a->num=num;
		a->neg^=divider->neg;
	}
	return 0;
}

int fklDivBigIntI(FklBigInt* a,int64_t div)
{
	if(div==0)
		return 1;
	else
	{
		FklBigInt* divider=fklNewBigInt(div);
		int r=fklDivBigInt(a,divider);
		fklFreeBigInt(divider);
		return r;
	}
}

int fklRemBigInt(FklBigInt* a,const FklBigInt* divider)
{
	if(divider->num==1&&divider->digits[0]==0)
		return 1;
	int cmpR=cmpDigits(a,divider);
	if(!cmpR)
	{
		a->num=1;
		a->digits[0]=0;
	}
	else if(cmpR>0)
	{
		FklBigInt s={NULL,0,a->num,0};
		s.digits=(uint8_t*)malloc(sizeof(uint8_t)*(divider->num+1));
		FKL_ASSERT(s.digits,__func__);
		for(uint64_t i=0;i<a->num;i++)
		{
			s.digits[i]=a->digits[i];
			s.num++;
			for(;cmpDigits(&s,divider)>0;fklSubBigInt(&s,divider));
		}
		free(s.digits);
		free(a->digits);
		a->digits=s.digits;
		a->size=s.num;
		a->num=s.size;
	}
	return 0;
}

int fklRemBigIntI(FklBigInt* a,int64_t div)
{
	if(div==0)
		return 1;
	else
	{
		FklBigInt* divider=fklNewBigInt(div);
		int r=fklRemBigInt(a,divider);
		fklFreeBigInt(divider);
		return r;
	}
}

int64_t fklBigIntToI64(const FklBigInt* a)
{
	int64_t r=0;
	int base=1;
	for(uint64_t i=0;i<a->num;i++)
	{
		r=a->digits[i]*base;
		base*=10;
	}
	if(a->neg)
		r*=-1;
	return r;
}

double fklBigIntToDouble(const FklBigInt* a)
{
	double r=0;
	int base=1;
	for(uint64_t i=0;i<a->num;i++)
	{
		r=a->digits[i]*base;
		base*=10;
	}
	if(a->neg)
		r*=-1;
	return r;
}

void fklPrintBigInt(FklBigInt* a,FILE* fp)
{
	if(!FKL_IS_ZERO_BIG_INT(a)&&a->neg)
		fputc('-',fp);
	for(uint64_t i=a->num;i>0;i--)
		fprintf(fp,"%u",a->digits[i-1]);
}
