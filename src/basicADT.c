#include<fakeLisp/basicADT.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/bytecode.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>
#include<ctype.h>
#include<float.h>
int fklIsPtrStackEmpty(FklPtrStack* stack)
{
	return stack->top==0;
}

FklPtrStack* fklNewPtrStack(uint32_t size,uint32_t inc)
{
	FklPtrStack* tmp=(FklPtrStack*)malloc(sizeof(FklPtrStack));
	FKL_ASSERT(tmp);
	tmp->base=(void**)malloc(sizeof(void*)*size);
	FKL_ASSERT(tmp->base);
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
		FKL_ASSERT(tmpData);
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
		FKL_ASSERT(tmpData);
		stack->base=tmpData;
		stack->size-=stack->inc;
	}
}

FklQueueNode* fklNewQueueNode(void* data)
{
	FklQueueNode* tmp=(FklQueueNode*)malloc(sizeof(FklQueueNode));
	FKL_ASSERT(tmp);
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
	FKL_ASSERT(tmp);
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
	FKL_ASSERT(tmp);
	tmp->base=(int64_t*)malloc(sizeof(int64_t)*size);
	FKL_ASSERT(tmp->base);
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
		FKL_ASSERT(tmpData);
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
		FKL_ASSERT(tmpData);
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
	FKL_ASSERT(tmp);
	tmp->base=(uint64_t*)malloc(sizeof(uint64_t)*size);
	FKL_ASSERT(tmp->base);
	tmp->size=size;
	tmp->inc=inc;
	tmp->top=0;
	return tmp;
}

FklUintStack* fklNewUintStackFromStack(FklUintStack* stack)
{
	FklUintStack* r=fklNewUintStack(stack->size,stack->inc);
	r->top=stack->top;
	for(size_t i=0;i<stack->top;i++)
		r->base[i]=stack->base[i];
	return r;
}

void fklPushUintStack(uint64_t data,FklUintStack* stack)
{
	if(stack->top==stack->size)
	{
		uint64_t* tmpData=(uint64_t*)realloc(stack->base,(stack->size+stack->inc)*sizeof(uint64_t));
		FKL_ASSERT(tmpData);
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
		FKL_ASSERT(tmpData);
		stack->base=tmpData;
		stack->size-=stack->inc;
	}
}

#define FKL_BIG_INT_RADIX (10)

FklBigInt* fklNewBigInt(int64_t v)
{
	if(v==0)
		return fklNewBigInt0();
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	double d=v;
	FKL_ASSERT(t);
	if(v<0)
	{
		t->neg=1;
		d*=-1;
	}
	else
		t->neg=0;
	t->num=floor(log2(d)/log2(FKL_BIG_INT_RADIX))+1;
	if(t->num==0)
		t->num=1;
	t->size=t->num;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*t->num);
	FKL_ASSERT(t->digits);
	for(uint64_t i=0;i<t->num;i++)
	{
		t->digits[i]=v%FKL_BIG_INT_RADIX;
		if(t->neg)
			t->digits[i]*=-1;
		v/=FKL_BIG_INT_RADIX;
	}
	return t;
}

FklBigInt* fklNewBigIntD(double v)
{
	v=floor(v);
	if(fabs(v-0)<DBL_EPSILON)
		return fklNewBigInt0();
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	if(v-0<DBL_EPSILON)
	{
		t->neg=1;
		v*=-1;
	}
	else
		t->neg=0;
	t->num=floor(log2(v)/log2(FKL_BIG_INT_RADIX))+1;
	if(t->num==0)
		t->num=1;
	t->size=t->num;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*t->num);
	FKL_ASSERT(t->digits);
	for(uint64_t i=0;i<t->num;i++)
	{
		t->digits[i]=fmod(v,FKL_BIG_INT_RADIX);
		v/=FKL_BIG_INT_RADIX;
	}
	return t;
}

FklBigInt* fklNewBigIntU(uint64_t v)
{
	if(v==0)
		return fklNewBigInt0();
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	t->neg=0;
	t->num=floor(log2(v)/log2(FKL_BIG_INT_RADIX))+1;
	if(t->num==0)
		t->num=1;
	t->size=t->num;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*t->num);
	FKL_ASSERT(t->digits);
	for(uint64_t i=0;i<t->num;i++)
	{
		t->digits[i]=v%FKL_BIG_INT_RADIX;
		if(t->neg)
			t->digits[i]*=-1;
		v/=FKL_BIG_INT_RADIX;
	}
	return t;
}

FklBigInt* fklNewBigInt0(void)
{
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	t->neg=0;
	t->num=1;
	t->size=t->num;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*t->num);
	FKL_ASSERT(t->digits);
	t->digits[0]=0;
	return t;
}

FklBigInt* fklNewBigInt1(void)
{
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	t->neg=0;
	t->num=1;
	t->size=t->num;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*t->num);
	FKL_ASSERT(t->digits);
	t->digits[0]=1;
	return t;
}

FklBigInt* fklNewBigIntFromString(const FklString* str)
{
	const char* buf=str->str;
	FklBigInt* r=fklNewBigInt0();
	size_t len=str->size;
	FKL_ASSERT(r);
	if(fklIsHexNumCharBuf(buf,len))
	{
		int neg=buf[0]=='-';
		uint64_t i=2+neg;
		for(;i<len&&isxdigit(buf[i]);i++)
		{
			fklMulBigIntI(r,16);
			fklAddBigIntI(r,isdigit(buf[i])?buf[i-1]-'0':toupper(buf[i])-'A'+10);
		}
		r->neg=neg;
	}
	else if(fklIsOctNumCharBuf(buf,len))
	{
		int neg=buf[0]=='-';
		uint64_t i=1+neg;
		for(;i<len&&isdigit(buf[i])&&buf[i]<'9';i++)
		{
			fklMulBigIntI(r,8);
			fklAddBigIntI(r,buf[i]-'0');
		}
		r->neg=neg;
	}
	else
	{
		int neg=buf[0]=='-';
		uint64_t i=neg;
		for(;i<len&&isdigit(buf[i]);i++)
		{
			fklMulBigIntI(r,10);
			fklAddBigIntI(r,buf[i]-'0');
		}
		r->neg=neg;
	}
	return r;
}

FklBigInt* fklNewBigIntFromCstr(const char* v)
{
	FklBigInt* r=fklNewBigInt0();
	size_t len=strlen(v);
	FKL_ASSERT(r);
	if(fklIsHexNumCharBuf(v,len))
	{
		int neg=v[0]=='-';
		v+=2+neg;
		for(uint64_t i=0;isxdigit(v[i]);i++)
		{
			fklMulBigIntI(r,16);
			fklAddBigIntI(r,isdigit(v[i])?v[i-1]-'0':toupper(v[i])-'A'+10);
		}
		r->neg=neg;
	}
	else if(fklIsOctNumCharBuf(v,len))
	{
		int neg=v[0]=='-';
		v+=neg+1;
		for(uint64_t i=0;isdigit(v[i])&&v[i]<'9';i++)
		{
			fklMulBigIntI(r,8);
			fklAddBigIntI(r,v[i]-'0');
		}
		r->neg=neg;
	}
	else
	{
		int neg=v[0]=='-';
		v+=neg;
		uint64_t i=0;
		for(;isdigit(v[i]);i++)
		{
			fklMulBigIntI(r,10);
			fklAddBigIntI(r,v[i]-'0');
		}
		r->neg=neg;
	}
	return r;
}

FklBigInt* fklNewBigIntFromMem(const void* mem,size_t size)
{
	if(size<2)
		return NULL;
	uint8_t neg=((uint8_t*)mem)[0];
	if(neg>1)
		return NULL;
	mem++;
	uint64_t num=size-1;
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	t->num=num;
	t->size=num;
	t->neg=neg;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*num);
	FKL_ASSERT(t->digits);
	for(uint64_t i=0;i<num;i++)
	{
		uint8_t n=((uint8_t*)mem)[i];
		if(n>9)
		{
			free(t->digits);
			free(t);
			return NULL;
		}
		t->digits[i]=n;
	}
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
		FKL_ASSERT(obj->digits);
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

void fklInitBigInt(FklBigInt* a,const FklBigInt* src)
{
	a->digits=NULL;
	a->neg=0;
	a->num=0;
	a->size=0;
	fklSetBigInt(a,src);
}

void fklInitBigIntI(FklBigInt* a,int64_t v)
{
	FklBigInt* bi=fklNewBigInt(v);
	fklInitBigInt(a,bi);
	fklFreeBigInt(bi);
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
	des->num=floor(log2(src)/log2(FKL_BIG_INT_RADIX))+1;
	if(des->num==0)
		des->num=1;
	ensureBigIntDigits(des,des->num);
	for(uint64_t i=0;i<des->num;i++)
	{
		des->digits[i]=src%FKL_BIG_INT_RADIX;
		src/=FKL_BIG_INT_RADIX;
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
		a->digits[i]=total%FKL_BIG_INT_RADIX;
		carry=(total>=FKL_BIG_INT_RADIX)?1:0;
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
			tmp+=FKL_BIG_INT_RADIX;
		}
		else
			carry=0;
		FKL_ASSERT(tmp>=0);
		a->digits[i]=tmp;
		if(tmp!=0)
			a->num=i+1;
	}
	FKL_ASSERT(carry==0);
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
	else if(multipler->num==1&&multipler->digits[0]==1)
	{
		a->neg^=multipler->neg;
		return;
	}
	else
	{
		uint64_t totalNum=a->num+multipler->num;
		uint8_t* res=(uint8_t*)calloc(totalNum,sizeof(uint8_t));
		FKL_ASSERT(res);
		for(uint64_t i=0;i<a->num;i++)
		{
			int n0=a->digits[i];
			for(uint64_t j=0;j<multipler->num;j++)
			{
				int n1=multipler->digits[j];
				int sum=(res[i+j]+n0*n1);
				res[i+j]=sum%FKL_BIG_INT_RADIX;
				res[i+j+1]+=sum/FKL_BIG_INT_RADIX;
			}
		}
		free(a->digits);
		a->digits=res;
		a->num=totalNum-(res[totalNum-1]==0);
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
		a->neg^=divider->neg;
	}
	else
	{
		uint8_t* res=(uint8_t*)malloc(sizeof(uint8_t)*a->num);
		FKL_ASSERT(res);
		uint64_t num=0;
		FklBigInt s={NULL,0,a->num,0};
		s.digits=(uint8_t*)malloc(sizeof(uint8_t)*(divider->num+1));
		FKL_ASSERT(s.digits);
		for(uint64_t i=0;i<a->num;i++)
		{
			memcpy(s.digits,&a->digits[a->num-i-1],(i+1)*sizeof(uint8_t));
			s.num++;
			int count=0;
			for(;cmpDigits(&s,divider)>=0;subDigits(&s,divider),count++);
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
		FKL_ASSERT(s.digits);
		for(uint64_t i=0;i<a->num;i++)
		{
			memcpy(s.digits,&a->digits[a->num-i-1],(i+1)*sizeof(uint8_t));
			s.num++;
			for(;cmpDigits(&s,divider)>=0;subDigits(&s,divider));
		}
		free(a->digits);
		a->digits=s.digits;
		a->num=s.num;
		a->size=s.size;
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

int fklIsDivisibleBigInt(const FklBigInt* a,const FklBigInt* b)
{
	int r=0;
	FklBigInt tbi={NULL,0,0,0};
	fklInitBigInt(&tbi,a);
	fklRemBigInt(&tbi,b);
	if(tbi.num==1&&tbi.digits[0]==0)
		r=1;
	free(tbi.digits);
	return r;
}

int fklIsDivisibleBigIntI(const FklBigInt* a,int64_t i)
{
	FklBigInt* bi=fklNewBigInt(i);
	int r=fklIsDivisibleBigInt(a,bi);
	fklFreeBigInt(bi);
	return r;
}

int fklIsDivisibleIBigInt(int64_t i,const FklBigInt* b)
{
	int r=0;
	FklBigInt* a=fklNewBigInt(i);
	fklRemBigInt(a,b);
	if(a->num==1&&a->digits[0]==0)
		r=1;
	fklFreeBigInt(a);
	return r;
}

int fklIsGeLeI64BigInt(const FklBigInt* a)
{
	FklBigInt* bI64Max=fklNewBigInt(INT64_MAX);
	FklBigInt* bI64Min=fklNewBigInt(INT64_MIN);
	int r=(fklCmpBigInt(a,bI64Max)>=0||fklCmpBigInt(a,bI64Min)<=0);
	fklFreeBigInt(bI64Max);
	fklFreeBigInt(bI64Min);
	return r;
}

int fklIsGtLtI64BigInt(const FklBigInt* a)
{
	FklBigInt* bI64Max=fklNewBigInt(INT64_MAX);
	FklBigInt* bI64Min=fklNewBigInt(INT64_MIN);
	int r=(fklCmpBigInt(a,bI64Max)>0||fklCmpBigInt(a,bI64Min)<0);
	fklFreeBigInt(bI64Max);
	fklFreeBigInt(bI64Min);
	return r;
}

int fklIsGtI64MaxBigInt(const FklBigInt* a)
{
	FklBigInt* bI64Max=fklNewBigInt(INT64_MAX);
	int r=fklCmpBigInt(a,bI64Max)>0;
	fklFreeBigInt(bI64Max);
	return r;
}

int fklIsLtI64MinBigInt(const FklBigInt* a)
{
	FklBigInt* bI64Min=fklNewBigInt(INT64_MIN);
	int r=fklCmpBigInt(a,bI64Min)<0;
	fklFreeBigInt(bI64Min);
	return r;
}

int64_t fklBigIntToI64(const FklBigInt* a)
{
	if(fklIsGtLtI64BigInt(a))
	{
		if(a->neg)
			return INT64_MIN;
		return INT64_MAX;
	}
	int64_t r=0;
	uint64_t base=1;
	for(uint64_t i=0;i<a->num;i++)
	{
		r+=a->digits[i]*base;
		base*=FKL_BIG_INT_RADIX;
	}
	if(a->neg)
		r*=-1;
	return r;
}

double fklBigIntToDouble(const FklBigInt* a)
{
	double r=0;
	double base=1;
	for(uint64_t i=0;i<a->num;i++)
	{
		r+=a->digits[i]*base;
		base*=FKL_BIG_INT_RADIX;
	}
	if(a->neg)
		r*=-1;
	return r;
}

void fklPrintBigInt(FklBigInt* a,FILE* fp)
{
	if(!FKL_IS_0_BIG_INT(a)&&a->neg)
		fputc('-',fp);
	for(uint64_t i=a->num;i>0;i--)
		fprintf(fp,"%u",a->digits[i-1]);
}

void fklSprintBigInt(FklBigInt* bi,size_t size,char* buf)
{
	if(bi->neg)
	{
		(buf++)[0]='-';
		size--;
	}
	for(uint64_t i=0;i<size;i++)
		buf[i]=bi->digits[i]+'0';
}

int fklCmpBigIntI(const FklBigInt* bi,int64_t i)
{
	FklBigInt* tbi=fklNewBigInt(i);
	int r=fklCmpBigInt(bi,tbi);
	fklFreeBigInt(tbi);
	return r;
}

int fklCmpIBigInt(int64_t i,const FklBigInt* bi)
{
	return -fklCmpBigIntI(bi,i);
}

FklString* fklNewString(size_t size,const char* str)
{
	FklString* tmp=(FklString*)malloc(sizeof(FklString)+size*sizeof(uint8_t));
	FKL_ASSERT(tmp);
	tmp->size=size;
	if(str)
		memcpy(tmp->str,str,size);
	return tmp;
}

int fklStringcmp(const FklString* fir,const FklString* sec)
{
	size_t size=fir->size<sec->size?fir->size:sec->size;
	int r=memcmp(fir->str,sec->str,size);
	if(!r)
		return (int64_t)fir->size-(int64_t)sec->size;
	return r;
}

int fklStringCstrCmp(const FklString* fir,const char* sec)
{
	size_t seclen=strlen(sec);
	size_t size=fir->size<seclen?fir->size:seclen;
	int r=memcmp(fir->str,sec,size);
	if(!r)
		return (int64_t)fir->size-(int64_t)seclen;
	return r;
}

FklString* fklCopyString(const FklString* obj)
{
	if(obj==NULL)return NULL;
	FklString* tmp=(FklString*)malloc(sizeof(FklString)+obj->size);
	FKL_ASSERT(tmp);
	memcpy(tmp->str,obj->str,obj->size);
	tmp->size=obj->size;
	return tmp;
}

char* fklStringToCstr(const FklString* str)
{
	return fklCharBufToCstr(str->str,str->size);
}

FklString* fklNewStringFromCstr(const char* cStr)
{
	return fklNewString(strlen(cStr),cStr);
}

void fklStringCharBufCat(FklString** a,const char* buf,size_t s)
{
	size_t aSize=(*a)->size;
	FklString* prev=*a;
	prev=(FklString*)realloc(prev,sizeof(FklString)+(aSize+s)*sizeof(char));
	FKL_ASSERT(prev);
	*a=prev;
	prev->size=aSize+s;
	memcpy(prev->str+aSize,buf,s);
}

void fklStringCat(FklString** fir,const FklString* sec)
{
	fklStringCharBufCat(fir,sec->str,sec->size);
}

FklString* fklNewEmptyString()
{
	FklString* tmp=(FklString*)malloc(sizeof(FklString));
	FKL_ASSERT(tmp);
	tmp->size=0;
	return tmp;
}

void fklPrintRawString(const FklString* str,FILE* fp)
{
	fklPrintRawCharBuf(str->str,'"',str->size,fp);
}

void fklPrintRawSymbol(const FklString* str,FILE* fp)
{
	fklPrintRawCharBuf(str->str,'|',str->size,fp);
}

void fklPrintString(const FklString* str,FILE* fp)
{
	fwrite(str->str,str->size,1,fp);
}

void fklFreeStringArray(FklString** ss,uint32_t num)
{
	for(uint32_t i=0;i<num;i++)
		free(ss[i]);
	free(ss);
}

FklString* fklStringAppend(const FklString* a,const FklString* b)
{
	FklString* r=fklNewString(a->size+b->size,NULL);
	memcpy(&r->str[0],a->str,a->size);
	memcpy(&r->str[a->size],b->str,b->size);
	return r;
}

char* fklCstrStringCat(char* fir,const FklString* sec)
{
	size_t len=strlen(fir);
	fir=(char*)realloc(fir,sizeof(char)*len+sec->size+1);
	FKL_ASSERT(fir);
	fklWriteStringToCstr(&fir[len],sec);
	return fir;
}

void fklStringCstrCat(FklString** pfir,const char* sec)
{
	size_t seclen=strlen(sec);
	FklString* prev=*pfir;
	prev=(FklString*)realloc(prev,sizeof(FklString)+(prev->size+seclen)*sizeof(char));
	FKL_ASSERT(prev);
	*pfir=prev;
	memcpy(&prev->str[prev->size],sec,seclen);
	prev->size+=seclen;
}

void fklWriteStringToCstr(char* c_str,const FklString* str)
{
	size_t len=0;
	const char* buf=str->str;
	size_t size=str->size;
	for(size_t i=0;i<size;i++)
	{
		if(!buf[i])
			continue;
		c_str[len]=buf[i];
		len++;
	}
	c_str[len]='\0';
}

FklHashTable* fklNewHashTable(size_t size
		,double threshold
		,FklHashTableMethodTable* t)
{
	FKL_ASSERT(size);
	FklHashTable* r=(FklHashTable*)malloc(sizeof(FklHashTable));
	FKL_ASSERT(r);
	FklHashTableNode** base=(FklHashTableNode**)calloc(size,sizeof(FklHashTableNode*));
	FKL_ASSERT(base);
	FklHashTableNode** list=(FklHashTableNode**)malloc(size*sizeof(FklHashTableNode*));
	FKL_ASSERT(list);
	r->base=base;
	r->list=list;
	r->num=0;
	r->size=size;
	r->threshold=threshold;
	r->t=t;
	return r;
}

void* fklGetHashItem(void* key,FklHashTable* table)
{
	size_t (*__hashFunc)(void*,FklHashTable*)=table->t->__hashFunc;
	void* (*__getKey)(void*)=table->t->__getKey;
	int (*__keyEqual)(void*,void*)=table->t->__keyEqual;
	for(FklHashTableNode* p=table->base[__hashFunc(key,table)];p;p=p->next)
		if(__keyEqual(key,__getKey(p->item)))
			return p->item;
	return NULL;
}

static FklHashTableNode* newHashTableNode(void* item,FklHashTableNode* next)
{
	FklHashTableNode* node=(FklHashTableNode*)malloc(sizeof(FklHashTableNode));
	FKL_ASSERT(node);
	node->item=item;
	node->next=next;
	return node;
}

void* fklInsertHashItem(void* item,FklHashTable* table)
{
	size_t (*__hashFunc)(void*,FklHashTable*)=table->t->__hashFunc;
	void* (*__getKey)(void*)=table->t->__getKey;
	int (*__keyEqual)(void*,void*)=table->t->__keyEqual;
	FklHashTableNode** pp=&table->base[__hashFunc(__getKey(item),table)];
	int i=0;
	for(;*pp;pp=&(*pp)->next,i++)
		if(__keyEqual(__getKey((*pp)->item),__getKey(item)))
			return (*pp)->item;
	*pp=newHashTableNode(item,NULL);
	table->list[table->num]=*pp;
	table->num++;
	if(((double)table->num/table->size)>table->threshold)
		fklRehashTable(table,1);
	else if(i>4)
		fklRehashTable(table,2);
	return item;
}

void* fklInsNrptHashItem(void* item,FklHashTable* table)
{
	size_t (*__hashFunc)(void*,FklHashTable*)=table->t->__hashFunc;
	void* (*__getKey)(void*)=table->t->__getKey;
	int (*__keyEqual)(void*,void*)=table->t->__keyEqual;
	FklHashTableNode** pp=&table->base[__hashFunc(__getKey(item),table)];
	int i=0;
	for(;*pp;pp=&(*pp)->next,i++)
		if(__keyEqual(__getKey((*pp)->item),__getKey(item)))
		{
			table->t->__freeItem(item);
			return (*pp)->item;
		}
	*pp=newHashTableNode(item,NULL);
	table->list[table->num]=*pp;
	table->num++;
	if(((double)table->num/table->size)>table->threshold)
		fklRehashTable(table,1);
	else if(i>4)
		fklRehashTable(table,2);
	return item;
}

void fklRehashTable(FklHashTable* table,unsigned int inc)
{
	table->size+=table->size/inc;
	size_t num=table->num;
	FklHashTableNode** list=table->list;
	FklHashTableNode** base=table->base;
	FklHashTableNode** nlist=(FklHashTableNode**)realloc(base,table->size*sizeof(FklHashTableNode*));
	FKL_ASSERT(nlist);
	table->list=nlist;
	FklHashTableNode** nbase=(FklHashTableNode**)calloc(table->size,sizeof(FklHashTableNode*));
	table->base=nbase;
	FKL_ASSERT(nbase);
	table->num=0;
	for(size_t i=0;i<num;i++)
	{
		fklInsertHashItem(list[i]->item,table);
		free(list[i]);
	}
	free(list);
}

void fklFreeHashTable(FklHashTable* table)
{
	void (*__freeItem)(void*)=table->t->__freeItem;
	for(size_t i=0;i<table->num;i++)
	{
		__freeItem(table->list[i]->item);
		free(table->list[i]);
	}
	free(table->list);
	free(table->base);
	free(table);
}

FklBytevector* fklNewBytevector(size_t size,const uint8_t* ptr)
{
	FklBytevector* tmp=(FklBytevector*)malloc(sizeof(FklBytevector)+size*sizeof(uint8_t));
	FKL_ASSERT(tmp);
	tmp->size=size;
	if(ptr)
		memcpy(tmp->ptr,ptr,size);
	return tmp;
}

void fklBytevectorCat(FklBytevector** a,const FklBytevector* b)
{
	size_t aSize=(*a)->size;
	FklBytevector* prev=*a;
	prev=(FklBytevector*)realloc(prev,sizeof(FklBytevector)+(aSize+b->size)*sizeof(char));
	FKL_ASSERT(prev);
	*a=prev;
	prev->size=aSize+b->size;
	memcpy(prev->ptr+aSize,b->ptr,b->size);
}

int fklBytevectorcmp(const FklBytevector* fir,const FklBytevector* sec)
{
	size_t size=fir->size<sec->size?fir->size:sec->size;
	int r=memcmp(fir->ptr,sec->ptr,size);
	if(!r)
		return (int64_t)fir->size-(int64_t)sec->size;
	return r;
}

void fklPrintRawBytevector(const FklBytevector* bv,FILE* fp)
{
	fklPrintRawByteBuf(bv->ptr,bv->size,fp);
}

FklBytevector* fklCopyBytevector(const FklBytevector* obj)
{
	if(obj==NULL)return NULL;
	FklBytevector* tmp=(FklBytevector*)malloc(sizeof(FklBytevector)+obj->size);
	FKL_ASSERT(tmp);
	memcpy(tmp->ptr,obj->ptr,obj->size);
	tmp->size=obj->size;
	return tmp;
}
