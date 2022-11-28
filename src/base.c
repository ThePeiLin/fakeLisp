#include<fakeLisp/base.h>
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

void fklInitPtrStack(FklPtrStack* r,uint32_t size,uint32_t inc)
{
	r->base=(void**)malloc(sizeof(void*)*size);
	FKL_ASSERT(r->base);
	r->size=size;
	r->inc=inc;
	r->top=0;
}

FklPtrStack* fklCreatePtrStack(uint32_t size,uint32_t inc)
{
	FklPtrStack* r=(FklPtrStack*)malloc(sizeof(FklPtrStack));
	FKL_ASSERT(r);
	fklInitPtrStack(r,size,inc);
	return r;
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
	return tmp;
}

void* fklTopPtrStack(FklPtrStack* stack)
{
	if(fklIsPtrStackEmpty(stack))
		return NULL;
	return stack->base[stack->top-1];
}

void fklUninitPtrStack(FklPtrStack* s)
{
	free(s->base);
}

void fklDestroyPtrStack(FklPtrStack* stack)
{
	fklUninitPtrStack(stack);
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

FklQueueNode* fklCreateQueueNode(void* data)
{
	FklQueueNode* tmp=(FklQueueNode*)malloc(sizeof(FklQueueNode));
	FKL_ASSERT(tmp);
	tmp->data=data;
	tmp->next=NULL;
	return tmp;
}

void fklDestroyQueueNode(FklQueueNode* tmp)
{
	free(tmp);
}

FklPtrQueue* fklCreatePtrQueue(void)
{
	FklPtrQueue* tmp=(FklPtrQueue*)malloc(sizeof(FklPtrQueue));
	FKL_ASSERT(tmp);
	tmp->head=NULL;
	tmp->tail=&tmp->head;
	return tmp;
}

void fklDestroyPtrQueue(FklPtrQueue* tmp)
{
	FklQueueNode* cur=tmp->head;
	while(cur)
	{
		FklQueueNode* prev=cur;
		cur=cur->next;
		fklDestroyQueueNode(prev);
	}
	free(tmp);
}

int fklIsPtrQueueEmpty(FklPtrQueue* queue)
{
	return queue->head==NULL;
}

uint64_t fklLengthPtrQueue(FklPtrQueue* tmp)
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
		tmp->tail=&tmp->head;
	fklDestroyQueueNode(head);
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
	FklQueueNode* tmpNode=fklCreateQueueNode(data);
	*(tmp->tail)=tmpNode;
	tmp->tail=&tmpNode->next;
}

void fklPushPtrQueueToFront(void* data,FklPtrQueue* tmp)
{
	FklQueueNode* tmpNode=fklCreateQueueNode(data);
	tmpNode->next=tmp->head;
	tmp->head=tmpNode;
	if(!tmp->head)
		tmp->tail=&tmpNode->next;
}

FklPtrQueue* fklCopyPtrQueue(FklPtrQueue* q)
{
	FklQueueNode* head=q->head;
	FklPtrQueue* tmp=fklCreatePtrQueue();
	for(;head;head=head->next)
		fklPushPtrQueue(head->data,tmp);
	return tmp;
}

int fklIsIntStackEmpty(FklIntStack* stack)
{
	return stack->top==0;
}

FklIntStack* fklCreateIntStack(uint32_t size,uint32_t inc)
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
	return tmp;
}

int64_t fklTopIntStack(FklIntStack* stack)
{
	if(fklIsIntStackEmpty(stack))
		return 0;
	return stack->base[stack->top-1];
}

void fklDestroyIntStack(FklIntStack* stack)
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

void fklInitUintStack(FklUintStack* r,uint32_t size,uint32_t inc)
{
	r->base=(uint64_t*)malloc(sizeof(uint64_t)*size);
	FKL_ASSERT(r->base);
	r->size=size;
	r->inc=inc;
	r->top=0;
}

FklUintStack* fklCreateUintStack(uint32_t size,uint32_t inc)
{
	FklUintStack* tmp=(FklUintStack*)malloc(sizeof(FklUintStack));
	FKL_ASSERT(tmp);
	fklInitUintStack(tmp,size,inc);
	return tmp;
}

void fklUninitUintStack(FklUintStack* r)
{
	free(r->base);
}

FklUintStack* fklCreateUintStackFromStack(FklUintStack* stack)
{
	FklUintStack* r=fklCreateUintStack(stack->size,stack->inc);
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
	return tmp;
}

uint64_t fklTopUintStack(FklUintStack* stack)
{
	if(fklIsUintStackEmpty(stack))
		return 0;
	return stack->base[stack->top-1];
}

void fklDestroyUintStack(FklUintStack* stack)
{
	fklUninitUintStack(stack);
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

#define FKL_BIG_INT_RADIX (256)
#define FKL_BIG_INT_BITS (8)

FklBigInt* fklCreateBigInt(int64_t v)
{
	if(v==0)
		return fklCreateBigInt0();
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
		int c=v%FKL_BIG_INT_RADIX;
		if(c<0)
			c*=-1;
		t->digits[i]=c;
		v/=FKL_BIG_INT_RADIX;
	}
	return t;
}

FklBigInt* fklCreateBigIntD(double v)
{
	v=floor(v);
	if(fabs(v-0)<DBL_EPSILON)
		return fklCreateBigInt0();
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

FklBigInt* fklCreateBigIntU(uint64_t v)
{
	if(v==0)
		return fklCreateBigInt0();
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

FklBigInt* fklCreateBigInt0(void)
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

FklBigInt* fklCreateBigInt1(void)
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

FklBigInt* fklCreateBigIntFromString(const FklString* str)
{
	const char* buf=str->str;
	FklBigInt* r=fklCreateBigInt0();
	size_t len=str->size;
	FKL_ASSERT(r);
	if(fklIsHexNumCharBuf(buf,len))
	{
		int neg=buf[0]=='-';
		uint64_t i=2+neg;
		for(;i<len&&isxdigit(buf[i]);i++)
		{
			fklMulBigIntI(r,16);
			fklAddBigIntI(r,isdigit(buf[i])?buf[i]-'0':toupper(buf[i])-'A'+10);
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

FklBigInt* fklCreateBigIntFromCstr(const char* v)
{
	FklBigInt* r=fklCreateBigInt0();
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

FklBigInt* fklCopyBigInt(const FklBigInt* bigint)
{
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	t->num=bigint->num;
	t->size=bigint->num;
	t->neg=bigint->neg;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*bigint->num);
	FKL_ASSERT(t->digits);
	memcpy(t->digits,bigint->digits,t->num);
	return t;
}

FklBigInt* fklCreateBigIntFromMem(const void* mem,size_t size)
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
		t->digits[i]=n;
	}
	return t;
}

static const struct
{
	uint64_t base;
	size_t pow;
}BASE8[]=
{
	{0,   0, },
	{0,   0, },
	{0,   0, },
	{0,   0, },
	{0,   0, },
	{0,   0, },
	{0,   0, },
	{64,  2, },
	{0,   0, },
	{0,   0, },
	{100, 2, },
	{0,   0, },
	{0,   0, },
	{0,   0, },
	{0,   0, },
	{0,   0, },
	{256, 2, },
};

static void divRemBigIntU8(FklBigInt* a,uint64_t* prem,uint8_t b)
{
	uint64_t rem=0;
	for(uint64_t i=a->num;i>0;i--)
	{
		uint64_t t=(rem<<FKL_BIG_INT_BITS)|a->digits[i-1];
		uint64_t q=t/b;
		uint64_t r=t%b;
		if(q==0&&i==a->num)
			a->num--;
		else
			a->digits[i-1]=q;
		rem=r;
	}
	*prem=rem;
}

static FklUintStack* toRadixDigitsLe(const FklBigInt* u,uint32_t radix)
{
	FKL_ASSERT(!FKL_IS_0_BIG_INT(u));
	const double radixLog2=log2(radix);
	const size_t radixDigits=ceil((((double)(u->num*FKL_BIG_INT_BITS))/radixLog2));
	FklUintStack* res=fklCreateUintStack(radixDigits,16);
	uint8_t base=BASE8[radix].base;
	size_t pow=BASE8[radix].pow;
	FklBigInt digits=FKL_BIG_INT_INIT;
	fklSetBigInt(&digits,u);
	digits.neg=0;
	if(digits.num>=64)
	{
		FklBigInt bigBase=FKL_BIG_INT_INIT;
		fklSetBigIntU(&bigBase,base*base);
		size_t bigPow=2;
		size_t targetLen=sqrt(digits.num);
		while(bigBase.num<targetLen)
		{
			uint8_t t[bigBase.num];
			FklBigInt tmp=
			{
				.num=0,
				.neg=0,
				.digits=t,
				.size=bigBase.num,
			};
			fklSetBigInt(&tmp,&bigBase);
			fklMulBigInt(&bigBase,&tmp);
			bigPow*=2;
		}
		while(fklCmpBigInt(&digits,&bigBase)>0)
		{
			FklBigInt bigR=FKL_BIG_INT_INIT;
			fklDivModBigInt(&digits,&bigR,&bigBase);
			for(size_t i=0;i<bigPow;i++)
			{
				uint64_t r=0;
				divRemBigIntU8(&bigR,&r,base);
				for(size_t j=0;j<pow;j++)
				{
					fklPushUintStack(r%radix,res);
					r/=radix;
				}
			}
			free(bigR.digits);
		}
		free(bigBase.digits);
	}
	while(digits.num>1)
	{
		uint64_t r=0;
		divRemBigIntU8(&digits,&r,base);
		for(size_t i=0;i<pow;i++)
		{
			fklPushUintStack(r%radix,res);
			r/=radix;
		}
	}
	uint8_t r=digits.digits[0];
	while(r!=0)
	{
		fklPushUintStack(r%radix,res);
		r/=radix;
	}
	free(digits.digits);
	return res;
}

static FklUintStack* toBitWiseDigitsLe(const FklBigInt* u,uint8_t bits)
{
	uint8_t mask=(1<<bits)-1;
	uint8_t digitsPerUint8=FKL_BIG_INT_BITS/bits;
	FklUintStack* res=fklCreateUintStack((u->num*FKL_BIG_INT_BITS)/bits,16);
	for(size_t i=0;i<u->num;i++)
	{
		uint8_t c=u->digits[i];
		for(size_t j=0;j<digitsPerUint8;j++)
		{
			fklPushUintStack(c&mask,res);
			c>>=bits;
		}
	}
	while(!fklIsUintStackEmpty(res)&&fklTopUintStack(res)==0)
		fklPopUintStack(res);
	return res;
}

static FklUintStack* toInexactBitWiseDigitsLe(const FklBigInt* u,uint8_t bits)
{
	FKL_ASSERT(!FKL_IS_0_BIG_INT(u)&&bits<=8);
	const size_t digits=ceil(((double)(u->num*FKL_BIG_INT_BITS))/bits);
	const uint8_t mask=(1<<bits)-1;
	FklUintStack* res=fklCreateUintStack(digits,16);
	int32_t r=0;
	int32_t rbits=0;
	for(size_t i=0;i<u->num;i++)
	{
		uint8_t c=u->digits[i];
		r|=c<<rbits;
		rbits+=FKL_BIG_INT_BITS;
		while(rbits>=bits)
		{
			fklPushUintStack(r&mask,res);
			r>>=bits;
			if(rbits>FKL_BIG_INT_BITS)
				r=c>>(FKL_BIG_INT_BITS-(rbits-bits));
			rbits-=bits;
		}
	}
	if(rbits!=0)
		fklPushUintStack(r,res);
	while(!fklIsUintStackEmpty(res)&&fklTopUintStack(res)==0)
		fklPopUintStack(res);
	return res;
}

void fklDestroyBigInt(FklBigInt* t)
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

void fklInitBigIntU(FklBigInt* a,uint64_t v)
{
	FklBigInt* bi=fklCreateBigIntU(v);
	fklInitBigInt(a,bi);
	fklDestroyBigInt(bi);
}

void fklInitBigIntI(FklBigInt* a,int64_t v)
{
	FklBigInt* bi=fklCreateBigInt(v);
	fklInitBigInt(a,bi);
	fklDestroyBigInt(bi);
}

void fklSetBigIntU(FklBigInt* des,uint64_t src)
{
	des->neg=0;
	if(src)
		des->num=floor(log2(src)/log2(FKL_BIG_INT_RADIX))+1;
	else
		des->num=1;
	if(des->num==0)
		des->num=1;
	ensureBigIntDigits(des,des->num);
	for(uint64_t i=0;i<des->num;i++)
	{
		des->digits[i]=src%FKL_BIG_INT_RADIX;
		src/=FKL_BIG_INT_RADIX;
	}
}

void fklSetBigIntI(FklBigInt* des,int64_t src)
{
	double d=src;
	if(src<0)
	{
		des->neg=1;
		d*=-1;
	}
	else
		des->neg=0;
	if(src!=0)
		des->num=floor(log2(d)/log2(FKL_BIG_INT_RADIX))+1;
	else
		des->num=1;
	if(des->num==0)
		des->num=1;
	ensureBigIntDigits(des,des->num);
	for(uint64_t i=0;i<des->num;i++)
	{
		int c=src%FKL_BIG_INT_RADIX;
		if(c<0)
			c*=-1;
		des->digits[i]=c;
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
		uint16_t total=a->digits[i]+addendDigit+carry;
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
	FklBigInt* addend=fklCreateBigInt(addendI);
	fklAddBigInt(a,addend);
	fklDestroyBigInt(addend);
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
	FklBigInt* toSub=fklCreateBigInt(sub);
	fklSubBigInt(a,toSub);
	fklDestroyBigInt(toSub);
}

void fklMulBigInt(FklBigInt* a,const FklBigInt* multipler)
{
	if(FKL_IS_0_BIG_INT(a))
		return;
	else if(FKL_IS_0_BIG_INT(multipler))
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
			uint8_t n0=a->digits[i];
			uint32_t carry=0;
			for(uint64_t j=0;j<multipler->num;j++)
			{
				uint8_t n1=multipler->digits[j];
				uint32_t sum=(res[i+j]+n0*n1+carry);
				res[i+j]=sum%FKL_BIG_INT_RADIX;
				carry=sum/FKL_BIG_INT_RADIX;
			}
			res[i+multipler->num]+=carry;
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
	FklBigInt* multipler=fklCreateBigInt(mul);
	fklMulBigInt(a,multipler);
	fklDestroyBigInt(multipler);
}

int fklDivModBigIntI(FklBigInt* a,int64_t* rem,int64_t div)
{
	FKL_ASSERT(rem);
	if(div==0)
		return 1;
	else
	{
		FklBigInt* divider=fklCreateBigInt(div);
		FklBigInt trem=FKL_BIG_INT_INIT;
		int r=fklDivModBigInt(a,&trem,divider);
		fklDestroyBigInt(divider);
		*rem=fklBigIntToI64(&trem);
		free(trem.digits);
		return r;
	}
}

int fklDivModBigIntU(FklBigInt* a,uint64_t* rem,uint64_t div)
{
	if(div==0)
		return 1;
	else
	{
		FklBigInt* divider=fklCreateBigIntU(div);
		FklBigInt trem=FKL_BIG_INT_INIT;
		int r=fklDivModBigInt(a,&trem,divider);
		fklDestroyBigInt(divider);
		*rem=fklBigIntToU64(&trem);
		free(trem.digits);
		return r;
	}
}

int fklDivModBigInt(FklBigInt* a,FklBigInt* rem,const FklBigInt* divider)
{
	if(divider->num==1&&divider->digits[0]==0)
		return 1;
	int cmpR=cmpDigits(a,divider);
	if(cmpR<0)
	{
		fklSetBigInt(rem,a);
		a->num=1;
		a->digits[0]=0;
	}
	else if(!cmpR)
	{
		a->num=1;
		a->digits[0]=1;
		a->neg^=divider->neg;
		fklSetBigIntI(rem,0);
	}
	else
	{
		uint8_t res[a->num];
		uint64_t num=0;
		FklBigInt s=FKL_BIG_INT_INIT;
		uint8_t sdigits[a->num+1];
		s.digits=&sdigits[a->num+1];
		s.size=a->num+1;
		for(uint64_t i=0;i<a->num;i++)
		{
			int count=0;
			if(!FKL_IS_0_BIG_INT(&s))
				s.num++;
			s.digits--;
			s.digits[0]=a->digits[a->num-i-1];
			for(;cmpDigits(&s,divider)>=0
					;subDigits(&s,divider))
				count++;
			if(count!=0||num!=0)
			{
				res[num]=count;
				num++;
			}
		}
		fklSetBigInt(rem,&s);
		rem->neg=a->neg;
		for(size_t i=0;i<num;i++)
			a->digits[i]=res[num-i-1];
		a->num=num;
		a->neg^=divider->neg;
	}
	return 0;
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
		uint8_t res[a->num];
		uint64_t num=0;
		FklBigInt s=FKL_BIG_INT_INIT;
		uint8_t sdigits[a->num+1];
		s.digits=&sdigits[a->num+1];
		s.size=a->num+1;
		for(uint64_t i=0;i<a->num;i++)
		{
			int count=0;
			if(!FKL_IS_0_BIG_INT(&s))
				s.num++;
			s.digits--;
			s.digits[0]=a->digits[a->num-i-1];
			for(;cmpDigits(&s,divider)>=0;subDigits(&s,divider))
				count++;
			if(count!=0||num!=0)
			{
				res[num]=count;
				num++;
			}
		}
		for(size_t i=0;i<num;i++)
			a->digits[i]=res[num-i-1];
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
		FklBigInt* divider=fklCreateBigInt(div);
		int r=fklDivBigInt(a,divider);
		fklDestroyBigInt(divider);
		return r;
	}
}

int fklDivBigIntU(FklBigInt* a,uint64_t div)
{
	if(div==0)
		return 1;
	else
	{
		FklBigInt* divider=fklCreateBigIntU(div);
		int r=fklDivBigInt(a,divider);
		fklDestroyBigInt(divider);
		return r;
	}
}

int fklModBigInt(FklBigInt* a,const FklBigInt* divider)
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
		int neg=a->neg;
		FklBigInt s=FKL_BIG_INT_INIT;
		uint8_t sdigits[a->num+1];
		s.digits=&sdigits[a->num+1];
		s.size=a->num+1;
		for(uint64_t i=0;i<a->num;i++)
		{
			if(!FKL_IS_0_BIG_INT(&s))
				s.num++;
			s.digits--;
			s.digits[0]=a->digits[a->num-i-1];
			for(;cmpDigits(&s,divider)>=0;subDigits(&s,divider));
		}
		fklSetBigInt(a,&s);
		a->neg=neg;
	}
	return 0;
}

int fklModBigIntU(FklBigInt* a,uint64_t div)
{
	if(div==0)
		return 1;
	else
	{
		FklBigInt* divider=fklCreateBigIntU(div);
		int r=fklModBigInt(a,divider);
		fklDestroyBigInt(divider);
		return r;
	}
}

int fklModBigIntI(FklBigInt* a,int64_t div)
{
	if(div==0)
		return 1;
	else
	{
		FklBigInt* divider=fklCreateBigInt(div);
		int r=fklModBigInt(a,divider);
		fklDestroyBigInt(divider);
		return r;
	}
}

int fklIsDivisibleBigInt(const FklBigInt* a,const FklBigInt* b)
{
	int r=0;
	FklBigInt tbi={NULL,0,0,0};
	fklInitBigInt(&tbi,a);
	fklModBigInt(&tbi,b);
	if(FKL_IS_0_BIG_INT(&tbi))
		r=1;
	free(tbi.digits);
	return r;
}

int fklIsDivisibleBigIntI(const FklBigInt* a,int64_t i)
{
	FklBigInt* bi=fklCreateBigInt(i);
	int r=fklIsDivisibleBigInt(a,bi);
	fklDestroyBigInt(bi);
	return r;
}

int fklIsDivisibleIBigInt(int64_t i,const FklBigInt* b)
{
	int r=0;
	FklBigInt* a=fklCreateBigInt(i);
	fklModBigInt(a,b);
	if(a->num==1&&a->digits[0]==0)
		r=1;
	fklDestroyBigInt(a);
	return r;
}

int fklIsGeLeI64BigInt(const FklBigInt* a)
{
	FklBigInt* bI64Max=fklCreateBigInt(INT64_MAX);
	FklBigInt* bI64Min=fklCreateBigInt(INT64_MIN);
	int r=(fklCmpBigInt(a,bI64Max)>=0||fklCmpBigInt(a,bI64Min)<=0);
	fklDestroyBigInt(bI64Max);
	fklDestroyBigInt(bI64Min);
	return r;
}

int fklIsGtLtI64BigInt(const FklBigInt* a)
{
	FklBigInt* bI64Max=fklCreateBigInt(INT64_MAX);
	FklBigInt* bI64Min=fklCreateBigInt(INT64_MIN);
	int r=(fklCmpBigInt(a,bI64Max)>0||fklCmpBigInt(a,bI64Min)<0);
	fklDestroyBigInt(bI64Max);
	fklDestroyBigInt(bI64Min);
	return r;
}

int fklIsGtU64MaxBigInt(const FklBigInt* a)
{
	FklBigInt* bI64Max=fklCreateBigIntU(UINT64_MAX);
	int r=fklCmpBigInt(a,bI64Max)>0;
	fklDestroyBigInt(bI64Max);
	return r;
}

int fklIsGtI64MaxBigInt(const FklBigInt* a)
{
	FklBigInt* bI64Max=fklCreateBigInt(INT64_MAX);
	int r=fklCmpBigInt(a,bI64Max)>0;
	fklDestroyBigInt(bI64Max);
	return r;
}

int fklIsLtI64MinBigInt(const FklBigInt* a)
{
	FklBigInt* bI64Min=fklCreateBigInt(INT64_MIN);
	int r=fklCmpBigInt(a,bI64Min)<0;
	fklDestroyBigInt(bI64Min);
	return r;
}

uint64_t fklBigIntToU64(const FklBigInt* a)
{
	if(fklIsGtU64MaxBigInt(a))
		return UINT64_MAX;
	uint64_t r=0;
	uint64_t base=1;
	for(uint64_t i=0;i<a->num;i++)
	{
		r+=a->digits[i]*base;
		base*=FKL_BIG_INT_RADIX;
	}
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

void fklPrintBigInt(const FklBigInt* a,FILE* fp)
{
	if(!FKL_IS_0_BIG_INT(a)&&a->neg)
		fputc('-',fp);
	if(FKL_IS_0_BIG_INT(a))
		fputc('0',fp);
	else
	{
		FklUintStack* res=toRadixDigitsLe(a,10);
		for(size_t i=res->top;i>0;i--)
		{
			uint8_t c=res->base[i-1];
			fputc('0'+c,fp);
		}
		fklDestroyUintStack(res);
	}
}

FklString* fklBigIntToString(const FklBigInt* a,int radix)
{
	if(FKL_IS_0_BIG_INT(a))
	{
		FklString* s=fklCreateString(sizeof(char)*1,NULL);
		s->str[0]='0';
		return s;
	}
	else
	{
		size_t len=0;
		size_t j=0;
		if(a->neg)
		{
			j++;
			len++;
		}
		FklUintStack* res=NULL;
		if(radix==10)
			res=toRadixDigitsLe(a,radix);
		else if(radix==16)
		{
			res=toBitWiseDigitsLe(a,4);
			len+=2;
			j+=2;
		}
		else if(radix==8)
		{
			len++;
			j++;
			res=toInexactBitWiseDigitsLe(a,3);
		}
		len+=res->top;
		FklString* s=fklCreateString(sizeof(char)*len,NULL);
		char* buf=s->str;
		if(a->neg)
			buf[0]='-';
		if(radix==16)
		{
			buf[a->neg]='0';
			buf[a->neg+1]='x';
		}
		else if(radix==8)
			buf[a->neg]='0';
		for(size_t i=res->top;i>0;i--,j++)
		{
			uint64_t c=res->base[i-1];
			buf[j]=c<10?c+'0':c-10+'A';
		}
		fklDestroyUintStack(res);
		return s;
	}
}

void fklSprintBigInt(const FklBigInt* bi,size_t size,char* buf)
{
	if(bi->neg)
	{
		(buf++)[0]='-';
		size--;
	}
	if(FKL_IS_0_BIG_INT(bi))
		buf[0]='0';
	else
	{
		FklUintStack* res=toRadixDigitsLe(bi,10);
		for(size_t i=res->top;i>0;i--)
			buf[i]=res->base[i-1]+'0';
		fklDestroyUintStack(res);
	}
}

int fklCmpBigIntI(const FklBigInt* bi,int64_t i)
{
	FklBigInt* tbi=fklCreateBigInt(i);
	int r=fklCmpBigInt(bi,tbi);
	fklDestroyBigInt(tbi);
	return r;
}

int fklCmpIBigInt(int64_t i,const FklBigInt* bi)
{
	return -fklCmpBigIntI(bi,i);
}

FklString* fklCreateString(size_t size,const char* str)
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

FklString* fklCreateStringFromCstr(const char* cStr)
{
	return fklCreateString(strlen(cStr),cStr);
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

FklString* fklCreateEmptyString()
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

FklString* fklStringToRawString(const FklString* str)
{
	FklString* retval=fklCreateStringFromCstr("\"");
	size_t size=str->size;
	const char* buf=str->str;
	char c_str[FKL_MAX_STRING_SIZE]={0};
	for(size_t i=0;i<size;i++)
	{
		char cur=buf[i];
		fklWriteCharAsCstr(cur,c_str,FKL_MAX_STRING_SIZE);
		fklStringCstrCat(&retval,&c_str[2]);
	}
	fklStringCstrCat(&retval,"\"");
	return retval;
}

FklString* fklStringToRawSymbol(const FklString* str)
{
	FklString* retval=fklCreateStringFromCstr("|");
	size_t size=str->size;
	const char* buf=str->str;
	char c_str[FKL_MAX_STRING_SIZE]={0};
	for(size_t i=0;i<size;i++)
	{
		char cur=buf[i];
		fklWriteCharAsCstr(cur,c_str,FKL_MAX_STRING_SIZE);
		fklStringCstrCat(&retval,&c_str[2]);
	}
	fklStringCstrCat(&retval,"|");
	return retval;
}

void fklDestroyStringArray(FklString** ss,uint32_t num)
{
	for(uint32_t i=0;i<num;i++)
		free(ss[i]);
	free(ss);
}

FklString* fklStringAppend(const FklString* a,const FklString* b)
{
	FklString* r=fklCreateString(a->size+b->size,NULL);
	memcpy(&r->str[0],a->str,a->size);
	memcpy(&r->str[a->size],b->str,b->size);
	return r;
}

char* fklCstrStringCat(char* fir,const FklString* sec)
{
	if(!fir)
		return fklStringToCstr(sec);
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

size_t fklCountCharInString(FklString* s,char c)
{
	return fklCountCharInBuf(s->str,s->size,c);
}

FklHashTable* fklCreateHashTable(size_t size
		,size_t linkNum
		,int linkNumInc
		,double threshold
		,int thresholdInc
		,FklHashTableMethodTable* t)
{
	FKL_ASSERT(size&&linkNum&&linkNumInc&&thresholdInc&&threshold!=0.0);
	FklHashTable* r=(FklHashTable*)malloc(sizeof(FklHashTable));
	FKL_ASSERT(r);
	FklHashTableNode** base=(FklHashTableNode**)calloc(size,sizeof(FklHashTableNode*));
	FKL_ASSERT(base);
	r->base=base;
	r->list=NULL;
	r->tail=&r->list;
	r->num=0;
	r->size=size;
	r->linkNum=linkNum;
	r->linkNumInc=linkNumInc;
	r->threshold=threshold;
	r->thresholdInc=thresholdInc;
	r->t=t;
	return r;
}

void* fklGetHashItem(void* key,FklHashTable* table)
{
	size_t (*__hashFunc)(void*)=table->t->__hashFunc;
	void* (*__getKey)(void*)=table->t->__getKey;
	int (*__keyEqual)(void*,void*)=table->t->__keyEqual;
	for(FklHashTableNode* p=table->base[__hashFunc(key)%table->size];p;p=p->next)
		if(__keyEqual(key,__getKey(p->item)))
			return p->item;
	return NULL;
}

static FklHashTableNode* createHashTableNode(void* item,FklHashTableNode* next)
{
	FklHashTableNode* node=(FklHashTableNode*)malloc(sizeof(FklHashTableNode));
	FKL_ASSERT(node);
	node->item=item;
	node->next=next;
	return node;
}

static FklHashTableNodeList* createHashTableNodeList(FklHashTableNode* node,FklHashTableNodeList* next)
{
	FklHashTableNodeList* list=(FklHashTableNodeList*)malloc(sizeof(FklHashTableNodeList));
	FKL_ASSERT(node);
	list->node=node;
	list->next=next;
	return list;
}

#define REHASH() if(((double)table->num/table->size)>table->threshold)\
	fklRehashTable(table,table->thresholdInc);\
	else if(table->linkNum&&i>table->linkNum)\
	fklRehashTable(table,table->linkNumInc)

void* fklPutReplHashItem(void* item,FklHashTable* table)
{
	size_t (*__hashFunc)(void*)=table->t->__hashFunc;
	void* (*__getKey)(void*)=table->t->__getKey;
	int (*__keyEqual)(void*,void*)=table->t->__keyEqual;
	FklHashTableNode** pp=&table->base[__hashFunc(__getKey(item))%table->size];
	int i=0;
	for(;*pp;pp=&(*pp)->next,i++)
		if(__keyEqual(__getKey((*pp)->item),__getKey(item)))
		{
			table->t->__destroyItem((*pp)->item);
			(*pp)->item=item;
			return item;
		}
	*pp=createHashTableNode(item,NULL);
	*table->tail=createHashTableNodeList(*pp,NULL);
	table->tail=&(*table->tail)->next;
	table->num++;
	REHASH();
	return item;
}

void* fklPutInReverseOrder(void* item,FklHashTable* table)
{
	size_t (*__hashFunc)(void*)=table->t->__hashFunc;
	void* (*__getKey)(void*)=table->t->__getKey;
	int (*__keyEqual)(void*,void*)=table->t->__keyEqual;
	FklHashTableNode** pp=&table->base[__hashFunc(__getKey(item))%table->size];
	int i=0;
	for(;*pp;pp=&(*pp)->next,i++)
		if(__keyEqual(__getKey((*pp)->item),__getKey(item)))
		{
			table->t->__destroyItem(item);
			return (*pp)->item;
		}
	*pp=createHashTableNode(item,NULL);
	table->list=createHashTableNodeList(*pp,table->list);
	if(table->tail==&table->list)
		table->tail=&table->list->next;
	table->num++;
	REHASH();
	return item;
}

void* fklPutNoRpHashItem(void* item,FklHashTable* table)
{
	size_t (*__hashFunc)(void*)=table->t->__hashFunc;
	void* (*__getKey)(void*)=table->t->__getKey;
	int (*__keyEqual)(void*,void*)=table->t->__keyEqual;
	FklHashTableNode** pp=&table->base[__hashFunc(__getKey(item))%table->size];
	int i=0;
	for(;*pp;pp=&(*pp)->next,i++)
		if(__keyEqual(__getKey((*pp)->item),__getKey(item)))
		{
			table->t->__destroyItem(item);
			return (*pp)->item;
		}
	*pp=createHashTableNode(item,NULL);
	*table->tail=createHashTableNodeList(*pp,NULL);
	table->tail=&(*table->tail)->next;
	table->num++;
	REHASH();
	return item;
}

void fklRehashTable(FklHashTable* table,unsigned int inc)
{
	table->size+=table->size/inc;
	FklHashTableNodeList* list=table->list;
	table->list=NULL;
	table->tail=&table->list;
	free(table->base);
	FklHashTableNode** nbase=(FklHashTableNode**)calloc(table->size,sizeof(FklHashTableNode*));
	FKL_ASSERT(nbase);
	table->base=nbase;
	table->num=0;
	while(list)
	{
		FklHashTableNodeList* cur=list;
		fklPutReplHashItem(list->node->item,table);
		list=list->next;
		free(cur->node);
		free(cur);
	}
}

void fklDestroyHashTable(FklHashTable* table)
{
	void (*__destroyItem)(void*)=table->t->__destroyItem;
	FklHashTableNodeList* list=table->list;
	while(list)
	{
		FklHashTableNodeList* cur=list;
		__destroyItem(cur->node->item);
		free(cur->node);
		list=list->next;
		free(cur);
	}
	free(table->base);
	free(table);
}

FklBytevector* fklCreateBytevector(size_t size,const uint8_t* ptr)
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

FklString* fklBytevectorToString(const FklBytevector* bv)
{
	FklString* retval=fklCreateStringFromCstr("#vu8(");
	const uint8_t* ptr=bv->ptr;
	size_t size=bv->size;
	char c_str[FKL_MAX_STRING_SIZE]={0};
	for(size_t i=0;i<size;i++)
	{
		snprintf(c_str,FKL_MAX_STRING_SIZE,"0x%X",ptr[i]);
		if(i<size-1)
			fklStringCstrCat(&retval," ");
	}
	fklStringCstrCat(&retval,")");
	return retval;
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
