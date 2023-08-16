#include<fakeLisp/base.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/bytecode.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>
#include<ctype.h>
#include<stdarg.h>
int fklIsPtrStackEmpty(FklPtrStack* stack)
{
	return stack->top==0;
}

void fklInitPtrStack(FklPtrStack* r,uint32_t size,uint32_t inc)
{
	r->base=(void**)malloc(sizeof(void*)*size);
	FKL_ASSERT(r->base||!size);
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
		void** tmpData=(void**)fklRealloc(stack->base,(stack->size+stack->inc)*sizeof(void*));
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
		void** tmpData=(void**)fklRealloc(stack->base,(stack->size-stack->inc)*sizeof(void*));
		FKL_ASSERT(tmpData);
		stack->base=tmpData;
		stack->size-=stack->inc;
	}
}

inline FklQueueNode* fklCreateQueueNode(void* data)
{
	FklQueueNode* tmp=(FklQueueNode*)malloc(sizeof(FklQueueNode));
	FKL_ASSERT(tmp);
	tmp->data=data;
	tmp->next=NULL;
	return tmp;
}

inline void fklDestroyQueueNode(FklQueueNode* tmp)
{
	free(tmp);
}

inline void fklInitPtrQueue(FklPtrQueue* q)
{
	q->head=NULL;
	q->tail=&q->head;
}

inline FklPtrQueue* fklCreatePtrQueue(void)
{
	FklPtrQueue* tmp=(FklPtrQueue*)malloc(sizeof(FklPtrQueue));
	FKL_ASSERT(tmp);

	tmp->head=NULL;
	tmp->tail=&tmp->head;
	return tmp;
}

inline void fklUninitPtrQueue(FklPtrQueue* q)
{
	FklQueueNode* cur=q->head;
	while(cur)
	{
		FklQueueNode* prev=cur;
		cur=cur->next;
		fklDestroyQueueNode(prev);
	}
}

inline void fklDestroyPtrQueue(FklPtrQueue* tmp)
{
	fklUninitPtrQueue(tmp);
	free(tmp);
}

inline int fklIsPtrQueueEmpty(FklPtrQueue* queue)
{
	return queue->head==NULL;
}

inline uint64_t fklLengthPtrQueue(FklPtrQueue* tmp)
{
	FklQueueNode* cur=tmp->head;
	uint64_t i=0;
	for(;cur;cur=cur->next,i++);
	return i;
}

inline void* fklPopPtrQueue(FklPtrQueue* tmp)
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
	FKL_ASSERT(tmp->base||!size);
	tmp->size=size;
	tmp->inc=inc;
	tmp->top=0;
	return tmp;
}

void fklPushIntStack(int64_t data,FklIntStack* stack)
{
	if(stack->top==stack->size)
	{
		int64_t* tmpData=(int64_t*)fklRealloc(stack->base,(stack->size+stack->inc)*sizeof(int64_t));
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
		int64_t* tmpData=(int64_t*)fklRealloc(stack->base,(stack->size-stack->inc)*sizeof(int64_t));
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
	FKL_ASSERT(r->base||!size);
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

void fklInitUintStackWithStack(FklUintStack* r,const FklUintStack* stack)
{
	fklInitUintStack(r,stack->size,stack->inc);
	r->top=stack->top;
	for(size_t i=0;i<stack->top;i++)
		r->base[i]=stack->base[i];
}

FklUintStack* fklCreateUintStackFromStack(const FklUintStack* stack)
{
	FklUintStack* r=(FklUintStack*)malloc(sizeof(FklUintStack));
	FKL_ASSERT(r);
	fklInitUintStackWithStack(r,stack);
	return r;
}

void fklPushUintStack(uint64_t data,FklUintStack* stack)
{
	if(stack->top==stack->size)
	{
		uint64_t* tmpData=(uint64_t*)fklRealloc(stack->base,(stack->size+stack->inc)*sizeof(uint64_t));
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
		uint64_t* tmpData=(uint64_t*)fklRealloc(stack->base,(stack->size-stack->inc)*sizeof(uint64_t));
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
	FKL_ASSERT(t);
	fklInitBigIntI(t,v);
	return t;
}

#include<math.h>

FklBigInt* fklCreateBigIntD(double v)
{
	v=floor(v);
	if(!islessgreater(v,0.0))
		return fklCreateBigInt0();
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	if(isless(v,0))
	{
		t->neg=1;
		v*=-1;
	}
	else
		t->neg=0;
	t->num=floor(log2(v)/log2(FKL_BIG_INT_RADIX))+1;
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
	fklInitBigIntU(t,v);
	return t;
}

void fklInitBigInt0(FklBigInt* t)
{
	t->neg=0;
	t->num=1;
	t->size=t->num;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*t->num);
	FKL_ASSERT(t->digits);
	t->digits[0]=0;
}

FklBigInt* fklCreateBigInt0(void)
{
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	fklInitBigInt0(t);
	return t;
}

void fklInitBigInt1(FklBigInt* t)
{
	t->neg=0;
	t->num=1;
	t->size=t->num;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*t->num);
	FKL_ASSERT(t->digits);
	t->digits[0]=1;
}

FklBigInt* fklCreateBigInt1(void)
{
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	fklInitBigInt1(t);
	return t;
}

FklBigInt* fklCreateBigIntFromString(const FklString* str)
{
	FklBigInt* r=fklCreateBigInt0();
	fklInitBigIntFromString(r,str);
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

void fklInitBigIntFromString(FklBigInt* r,const FklString* str)
{
	const char* buf=str->str;
	size_t len=str->size;
	if(fklIsHexNumCharBuf(buf,len))
	{
		int neg=buf[0]=='-';
		int offset=neg||buf[0]=='+';
		uint64_t i=2+offset;
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
		int offset=neg||buf[0]=='+';
		uint64_t i=1+offset;
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
		int offset=neg||buf[0]=='+';
		uint64_t i=offset;
		for(;i<len&&isdigit(buf[i]);i++)
		{
			fklMulBigIntI(r,10);
			fklAddBigIntI(r,buf[i]-'0');
		}
		r->neg=neg;
	}
}

void fklInitBigIntFromMem(FklBigInt* t,const void* mem,size_t size)
{
	uint8_t neg=((uint8_t*)mem)[0];
	mem++;
	uint64_t num=size-1;
	t->num=num;
	t->size=num;
	t->neg=neg;
	t->digits=(uint8_t*)malloc(sizeof(uint8_t)*num);
	FKL_ASSERT(t->digits||!num);
	for(uint64_t i=0;i<num;i++)
	{
		uint8_t n=((uint8_t*)mem)[i];
		t->digits[i]=n;
	}
}

void fklUninitBigInt(FklBigInt* bi)
{
	free(bi->digits);
	bi->digits=NULL;
	bi->neg=0;
	bi->num=0;
	bi->size=0;
}

FklBigInt* fklCreateBigIntFromMem(const void* mem,size_t size)
{
	if(size<2)
		return NULL;
	if(((uint8_t*)mem)[0]>1)
		return NULL;
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	fklInitBigIntFromMem(t,mem,size);
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

void fklBigIntToRadixDigitsLe(const FklBigInt* u,uint32_t radix,FklU8Stack* res)
{
	FKL_ASSERT(!FKL_IS_0_BIG_INT(u));
	const double radixLog2=log2(radix);
	const size_t radixDigits=ceil((((double)(u->num*FKL_BIG_INT_BITS))/radixLog2));
	fklInitU8Stack(res,radixDigits,16);
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
					fklPushU8Stack(r%radix,res);
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
			fklPushU8Stack(r%radix,res);
			r/=radix;
		}
	}
	uint8_t r=digits.digits[0];
	while(r!=0)
	{
		fklPushU8Stack(r%radix,res);
		r/=radix;
	}
	free(digits.digits);
}

static void toBitWiseDigitsLe(const FklBigInt* u,uint8_t bits,FklU8Stack* res)
{
	uint8_t mask=(1<<bits)-1;
	uint8_t digitsPerUint8=FKL_BIG_INT_BITS/bits;
	fklInitU8Stack(res,(u->num*FKL_BIG_INT_BITS)/bits,16);
	for(size_t i=0;i<u->num;i++)
	{
		uint8_t c=u->digits[i];
		for(size_t j=0;j<digitsPerUint8;j++)
		{
			fklPushU8Stack(c&mask,res);
			c>>=bits;
		}
	}
	while(!fklIsU8StackEmpty(res)&&fklTopU8Stack(res)==0)
		fklPopU8Stack(res);
}

static void toInexactBitWiseDigitsLe(const FklBigInt* u,uint8_t bits,FklU8Stack* res)
{
	FKL_ASSERT(!FKL_IS_0_BIG_INT(u)&&bits<=8);
	const size_t digits=ceil(((double)(u->num*FKL_BIG_INT_BITS))/bits);
	const uint8_t mask=(1<<bits)-1;
	fklInitU8Stack(res,digits,16);
	int32_t r=0;
	int32_t rbits=0;
	for(size_t i=0;i<u->num;i++)
	{
		uint8_t c=u->digits[i];
		r|=c<<rbits;
		rbits+=FKL_BIG_INT_BITS;
		while(rbits>=bits)
		{
			fklPushU8Stack(r&mask,res);
			r>>=bits;
			if(rbits>FKL_BIG_INT_BITS)
				r=c>>(FKL_BIG_INT_BITS-(rbits-bits));
			rbits-=bits;
		}
	}
	if(rbits!=0)
		fklPushU8Stack(r,res);
	while(!fklIsU8StackEmpty(res)&&fklTopU8Stack(res)==0)
		fklPopU8Stack(res);
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
		obj->digits=(uint8_t*)fklRealloc(digits,sizeof(char)*num);
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

void fklInitBigIntU(FklBigInt* t,uint64_t v)
{
	if(v==0)
		return fklInitBigInt0(t);
	t->neg=0;
	t->num=floor(log2(v)/log2(FKL_BIG_INT_RADIX))+1;
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
}

void fklInitBigIntI(FklBigInt* t,int64_t v)
{
	if(v==0)
		return fklInitBigInt0(t);
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
}

void fklSetBigIntU(FklBigInt* des,uint64_t src)
{
	des->neg=0;
	if(src)
		des->num=floor(log2(src)/log2(FKL_BIG_INT_RADIX))+1;
	else
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

void fklSetBigIntD(FklBigInt* des,double d)
{
	d=floor(d);
	if(isless(d,0))
	{
		des->neg=1;
		d*=-1;
	}
	else
		des->neg=0;
	if(islessequal(d,0.0))
		des->num=1;
	else
		des->num=floor(log2(d)/log2(FKL_BIG_INT_RADIX))+1;
	ensureBigIntDigits(des,des->num);
	for(uint64_t i=0;i<des->num;i++)
	{
		des->digits[i]=fmod(d,FKL_BIG_INT_RADIX);
		d/=FKL_BIG_INT_RADIX;
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
	FklBigInt add=FKL_BIG_INT_INIT;
	fklInitBigIntI(&add,addendI);
	fklAddBigInt(a,&add);
	fklUninitBigInt(&add);
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
	FklBigInt toSub=FKL_BIG_INT_INIT;
	fklInitBigIntI(&toSub,sub);
	fklSubBigInt(a,&toSub);
	fklUninitBigInt(&toSub);
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
	FklBigInt multipler=FKL_BIG_INT_INIT;
	fklInitBigIntI(&multipler,mul);
	fklMulBigInt(a,&multipler);
	fklUninitBigInt(&multipler);
}

int fklDivModBigIntI(FklBigInt* a,int64_t* rem,int64_t div)
{
	FKL_ASSERT(rem);
	if(div==0)
		return 1;
	else
	{
		FklBigInt divider=FKL_BIG_INT_INIT;
		fklInitBigIntI(&divider,div);
		FklBigInt trem=FKL_BIG_INT_INIT;
		int r=fklDivModBigInt(a,&trem,&divider);
		fklUninitBigInt(&divider);
		*rem=fklBigIntToI64(&trem);
		fklUninitBigInt(&trem);
		return r;
	}
}

int fklDivModBigIntU(FklBigInt* a,uint64_t* rem,uint64_t div)
{
	if(div==0)
		return 1;
	else
	{
		FklBigInt divider=FKL_BIG_INT_INIT;
		fklInitBigIntU(&divider,div);
		FklBigInt trem=FKL_BIG_INT_INIT;
		int r=fklDivModBigInt(a,&trem,&divider);
		fklUninitBigInt(&divider);
		*rem=fklBigIntToU64(&trem);
		fklUninitBigInt(&trem);
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
		FklBigInt divider=FKL_BIG_INT_INIT;
		fklInitBigIntI(&divider,div);
		int r=fklDivBigInt(a,&divider);
		fklUninitBigInt(&divider);
		return r;
	}
}

int fklDivBigIntU(FklBigInt* a,uint64_t div)
{
	if(div==0)
		return 1;
	else
	{
		FklBigInt divider=FKL_BIG_INT_INIT;
		fklInitBigIntU(&divider,div);
		int r=fklDivBigInt(a,&divider);
		fklUninitBigInt(&divider);
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
		FklBigInt divider=FKL_BIG_INT_INIT;
		fklInitBigIntU(&divider,div);
		int r=fklModBigInt(a,&divider);
		fklUninitBigInt(&divider);
		return r;
	}
}

int fklModBigIntI(FklBigInt* a,int64_t div)
{
	if(div==0)
		return 1;
	else
	{
		FklBigInt divider=FKL_BIG_INT_INIT;
		fklInitBigIntI(&divider,div);
		int r=fklModBigInt(a,&divider);
		fklUninitBigInt(&divider);
		return r;
	}
}

int fklIsDivisibleBigInt(const FklBigInt* a,const FklBigInt* b)
{
	int r=0;
	FklBigInt tbi=FKL_BIG_INT_INIT;
	fklInitBigInt(&tbi,a);
	fklModBigInt(&tbi,b);
	if(FKL_IS_0_BIG_INT(&tbi))
		r=1;
	fklUninitBigInt(&tbi);
	return r;
}

int fklIsDivisibleBigIntI(const FklBigInt* a,int64_t i)
{
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigIntI(&bi,i);
	int r=fklIsDivisibleBigInt(a,&bi);
	fklUninitBigInt(&bi);
	return r;
}

int fklIsDivisibleIBigInt(int64_t i,const FklBigInt* b)
{
	int r=0;
	FklBigInt a=FKL_BIG_INT_INIT;
	fklInitBigIntI(&a,i);
	fklModBigInt(&a,b);
	if(FKL_IS_0_BIG_INT(&a))
		r=1;
	fklUninitBigInt(&a);
	return r;
}

static const uint8_t FKL_BIG_INT_I64_MAX_BIT[8]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f,};
static const uint8_t FKL_BIG_INT_I64_MIN_BIT[8]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,};
static const uint8_t FKL_BIG_INT_U64_MAX_BIT[8]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,};

static const FklBigInt FKL_BIG_INT_I64_MAX={(uint8_t*)FKL_BIG_INT_I64_MAX_BIT,8,8,0};
static const FklBigInt FKL_BIG_INT_I64_MIN={(uint8_t*)FKL_BIG_INT_I64_MIN_BIT,8,8,1};
static const FklBigInt FKL_BIG_INT_U64_MAX={(uint8_t*)FKL_BIG_INT_U64_MAX_BIT,8,8,0};

static const uint8_t FKL_BIG_INT_FIX_MAX_BIT[8]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0f,};
static const uint8_t FKL_BIG_INT_FIX_MIN_BIT[8]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,};

static const FklBigInt FKL_BIG_INT_FIX_MAX={(uint8_t*)FKL_BIG_INT_FIX_MAX_BIT,8,8,0};
static const FklBigInt FKL_BIG_INT_FIX_MIN={(uint8_t*)FKL_BIG_INT_FIX_MIN_BIT,8,8,1};

static const uint8_t FKL_BIG_INT_FIX_SUB_1_MAX_BIT[8]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,};
static const uint8_t FKL_BIG_INT_FIX_ADD_1_MIN_BIT[8]={0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x10,};

static const FklBigInt FKL_BIG_INT_FIX_SUB_1_MAX={(uint8_t*)FKL_BIG_INT_FIX_SUB_1_MAX_BIT,8,8,0};
static const FklBigInt FKL_BIG_INT_FIX_ADD_1_MIN={(uint8_t*)FKL_BIG_INT_FIX_ADD_1_MIN_BIT,8,8,1};

inline int fklIsBigIntAdd1InFixIntRange(const FklBigInt* a)
{
	return fklCmpBigInt(a,&FKL_BIG_INT_FIX_ADD_1_MIN)==0;
}

inline int fklIsBigIntSub1InFixIntRange(const FklBigInt* a)
{
	return fklCmpBigInt(a,&FKL_BIG_INT_FIX_SUB_1_MAX)==0;
}

inline int fklIsGtLtFixBigInt(const FklBigInt* a)
{
	return (fklCmpBigInt(a,&FKL_BIG_INT_FIX_MAX)>0||fklCmpBigInt(a,&FKL_BIG_INT_FIX_MIN)<0);
}

inline int fklIsGeLeFixBigInt(const FklBigInt* a)
{
	return (fklCmpBigInt(a,&FKL_BIG_INT_FIX_MAX)>=0||fklCmpBigInt(a,&FKL_BIG_INT_FIX_MIN)<=0);
}

inline int fklIsGeLeI64BigInt(const FklBigInt* a)
{
	return (fklCmpBigInt(a,&FKL_BIG_INT_I64_MAX)>=0||fklCmpBigInt(a,&FKL_BIG_INT_I64_MIN)<=0);
}

inline int fklIsGtLtI64BigInt(const FklBigInt* a)
{
	return (fklCmpBigInt(a,&FKL_BIG_INT_I64_MAX)>0||fklCmpBigInt(a,&FKL_BIG_INT_I64_MIN)<0);
}

inline int fklIsGtU64MaxBigInt(const FklBigInt* a)
{
	return fklCmpBigInt(a,&FKL_BIG_INT_U64_MAX)>0;
}

inline int fklIsGtI64MaxBigInt(const FklBigInt* a)
{
	return fklCmpBigInt(a,&FKL_BIG_INT_I64_MAX)>0;
}

inline int fklIsLtI64MinBigInt(const FklBigInt* a)
{
	return fklCmpBigInt(a,&FKL_BIG_INT_I64_MIN)<0;
}

inline int fklIsBigIntOdd(const FklBigInt* a)
{
	return a->digits[0]%2;
}

inline int fklIsBigIntEven(const FklBigInt* a)
{
	return !fklIsBigIntOdd(a);
}

inline int fklIsBigIntLt0(const FklBigInt *a)
{
	return !FKL_IS_0_BIG_INT(a)&&a->neg;
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
		FklU8Stack res=FKL_STACK_INIT;
		fklBigIntToRadixDigitsLe(a,10,&res);
		for(size_t i=res.top;i>0;i--)
		{
			uint8_t c=res.base[i-1];
			fputc('0'+c,fp);
		}
		fklUninitU8Stack(&res);
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
		FklU8Stack res=FKL_STACK_INIT;
		if(radix==10)
			fklBigIntToRadixDigitsLe(a,radix,&res);
		else if(radix==16)
		{
			toBitWiseDigitsLe(a,4,&res);
			len+=2;
			j+=2;
		}
		else if(radix==8)
		{
			len++;
			j++;
			toInexactBitWiseDigitsLe(a,3,&res);
		}
		len+=res.top;
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
		for(size_t i=res.top;i>0;i--,j++)
		{
			uint64_t c=res.base[i-1];
			buf[j]=c<10?c+'0':c-10+'A';
		}
		fklUninitU8Stack(&res);
		return s;
	}
}

int fklCmpBigIntI(const FklBigInt* bi,int64_t i)
{
	FklBigInt tbi=FKL_BIG_INT_INIT;
	fklInitBigIntI(&tbi,i);
	int r=fklCmpBigInt(bi,&tbi);
	fklUninitBigInt(&tbi);
	return r;
}

int fklCmpIBigInt(int64_t i,const FklBigInt* bi)
{
	return -fklCmpBigIntI(bi,i);
}

inline FklString* fklCreateString(size_t size,const char* str)
{
	FklString* tmp=(FklString*)malloc(sizeof(FklString)+size*sizeof(uint8_t));
	FKL_ASSERT(tmp);
	tmp->size=size;
	if(str)
		memcpy(tmp->str,str,size);
	tmp->str[size]='\0';
	return tmp;
}

int fklStringCmp(const FklString* fir,const FklString* sec)
{
	return strcmp(fir->str,sec->str);
}

int fklStringCstrCmp(const FklString* fir,const char* sec)
{
	return strcmp(fir->str,sec);
}

int fklStringCharBufCmp(const FklString* fir,size_t len,const char* buf)
{
	size_t size=fir->size<len?fir->size:len;
	int r=memcmp(fir->str,buf,size);
	if(r)
		return r;
	else if(fir->size<len)
		return -1;
	else if(fir->size>len)
		return 1;
	return r;
}

int fklStringCstrMatch(const FklString* a,const char* b)
{
	return !strncmp(a->str,b,a->size);
}

FklString* fklCopyString(const FklString* obj)
{
	return fklCreateString(obj->size,obj->str);
}

char* fklStringToCstr(const FklString* str)
{
	return fklCharBufToCstr(str->str,str->size);
}

FklString* fklCreateStringFromCstr(const char* cStr)
{
	return fklCreateString(strlen(cStr),cStr);
}

inline void fklStringCharBufCat(FklString** a,const char* buf,size_t s)
{
	size_t aSize=(*a)->size;
	FklString* prev=*a;
	prev=(FklString*)fklRealloc(prev,sizeof(FklString)+(aSize+s)*sizeof(char));
	FKL_ASSERT(prev);
	*a=prev;
	prev->size=aSize+s;
	memcpy(prev->str+aSize,buf,s);
	prev->str[prev->size]='\0';
}

void fklStringCat(FklString** fir,const FklString* sec)
{
	fklStringCharBufCat(fir,sec->str,sec->size);
}

void fklStringCstrCat(FklString** pfir,const char* sec)
{
	fklStringCharBufCat(pfir,sec,strlen(sec));
}

FklString* fklCreateEmptyString()
{
	FklString* tmp=(FklString*)calloc(1,sizeof(FklString));
	FKL_ASSERT(tmp);
	return tmp;
}

void fklPrintRawString(const FklString* str,FILE* fp)
{
	fklPrintRawCharBuf((const uint8_t*)str->str,'"',str->size,fp);
}

void fklPrintRawSymbol(const FklString* str,FILE* fp)
{
	fklPrintRawCharBuf((const uint8_t*)str->str,'|',str->size,fp);
}

void fklPrintString(const FklString* str,FILE* fp)
{
	fwrite(str->str,str->size,1,fp);
}

inline int fklIsSpecialCharAndPrintToStringBuffer(FklStringBuffer* s,char ch)
{
	int r=0;
	if((r=ch=='\n'))
		fklStringBufferPrintf(s,"\\n");
	else if((r=ch=='\t'))
		fklStringBufferPrintf(s,"\\t");
	else if((r=ch=='\v'))
		fklStringBufferPrintf(s,"\\v");
	else if((r=ch=='\a'))
		fklStringBufferPrintf(s,"\\a");
	else if((r=ch=='\b'))
		fklStringBufferPrintf(s,"\\b");
	else if((r=ch=='\f'))
		fklStringBufferPrintf(s,"\\f");
	else if((r=ch=='\r'))
		fklStringBufferPrintf(s,"\\r");
	else if((r=ch=='\x20'))
		fklStringBufferPrintf(s," ");
	return r;
}

inline void fklPrintRawStringToStringBuffer(FklStringBuffer* s,const FklString* fstr,char se)
{
	char buf[7]={0};
	fklStringBufferPrintf(s,"%c",se);
	size_t size=fstr->size;
	uint8_t* str=(uint8_t*)fstr->str;
	for(size_t i=0;i<size;)
	{
		unsigned int l=fklGetByteNumOfUtf8(&str[i],size-i);
		if(l==7)
		{
			uint8_t j=str[i];
			uint8_t h=j/16;
			uint8_t l=j%16;
			fklStringBufferPrintf(s,"\\x%c%c"
					,h<10?'0'+h:'A'+(h-10)
					,l<10?'0'+l:'A'+(l-10));
			i++;
		}
		else if(l==1)
		{
			if(str[i]==se)
				fklStringBufferPrintf(s,"\\%c",se);
			else if(str[i]=='\\')
				fklStringBufferPrintf(s,"\\\\");
			else if(isgraph(str[i]))
				fklStringBufferPrintf(s,"%c",str[i]);
			else if(fklIsSpecialCharAndPrintToStringBuffer(s,str[i]));
			else
			{
				uint8_t j=str[i];
				uint8_t h=j/16;
				uint8_t l=j%16;
				fklStringBufferPrintf(s,"\\x%c%c"
						,h<10?'0'+h:'A'+(h-10)
						,l<10?'0'+l:'A'+(l-10));
			}
			i++;
		}
		else
		{
			strncpy(buf,(char*)&str[i],l);
			buf[l]='\0';
			fklStringBufferPrintf(s,"%s",buf);
			i+=l;
		}
	}
	fklStringBufferPrintf(s,"%c",se);
}

FklString* fklStringToRawString(const FklString* str)
{
	FklStringBuffer buf=FKL_STRING_BUFFER_INIT;
	fklInitStringBuffer(&buf);
	fklPrintRawStringToStringBuffer(&buf,str,'\"');
	FklString* retval=fklStringBufferToString(&buf);
	fklUninitStringBuffer(&buf);
	return retval;
}

FklString* fklStringToRawSymbol(const FklString* str)
{
	FklStringBuffer buf=FKL_STRING_BUFFER_INIT;
	fklInitStringBuffer(&buf);
	fklPrintRawStringToStringBuffer(&buf,str,'|');
	FklString* retval=fklStringBufferToString(&buf);
	fklUninitStringBuffer(&buf);
	return retval;
}

FklString* fklStringAppend(const FklString* a,const FklString* b)
{
	FklString* r=fklCopyString(a);
	fklStringCat(&r,b);
	return r;
}

char* fklCstrStringCat(char* fir,const FklString* sec)
{
	if(!fir)
		return fklStringToCstr(sec);
	size_t len=strlen(fir);
	fir=(char*)fklRealloc(fir,sizeof(char)*len+sec->size+1);
	FKL_ASSERT(fir);
	fklWriteStringToCstr(&fir[len],sec);
	return fir;
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

inline void fklInitStringBuffer(FklStringBuffer* b)
{
	b->i=0;
	b->s=0;
	b->b=NULL;
	fklStringBufferReverse(b,64);
}

inline uint32_t fklStringBufferLen(FklStringBuffer* b)
{
	return b->i;
}

inline char* fklStringBufferBody(FklStringBuffer* b)
{
	return b->b;
}

inline void fklStringBufferPutc(FklStringBuffer* b,char c)
{
	fklStringBufferReverse(b,1);
	b->b[b->i++]=c;
}

inline void fklStringBufferBincpy(FklStringBuffer* b,const void* p,size_t l)
{
	fklStringBufferReverse(b,l);
	memcpy(&b->b[b->i],p,l);
	b->i+=l;
}

inline FklString* fklStringBufferToString(FklStringBuffer* b)
{
	return fklCreateString(b->i,b->b);
}

inline FklBytevector* fklStringBufferToBytevector(FklStringBuffer* b)
{
	return fklCreateBytevector(b->i,(uint8_t*)b->b);
}

inline FklStringBuffer* fklCreateStringBuffer(void)
{
	FklStringBuffer* r=(FklStringBuffer*)malloc(sizeof(FklStringBuffer));
	FKL_ASSERT(r);
	fklInitStringBuffer(r);
	return r;
}

inline void fklUninitStringBuffer(FklStringBuffer* b)
{
	b->s=0;
	b->i=0;
	free(b->b);
	b->b=NULL;
}

inline void fklDestroyStringBuffer(FklStringBuffer* b)
{
	fklUninitStringBuffer(b);
	free(b);
}

inline void fklStringBufferClear(FklStringBuffer* b)
{
	b->i=0;
}

inline void fklStringBufferFill(FklStringBuffer* b,char c)
{
	memset(b->b,c,b->i);
}

inline void fklStringBufferReverse(FklStringBuffer* b,size_t s)
{
	if((b->s-b->i)<s)
	{
		b->s+=s;
		char* t=(char*)fklRealloc(b->b,b->s);
		FKL_ASSERT(t);
		b->b=t;
	}
}

static inline void string_buffer_printf_va(FklStringBuffer* b,const char* fmt,va_list ap)
{
	long n;
	va_list cp;
	for(;;)
	{
#ifdef _WIN32
		cp = ap;
#else
		va_copy(cp,ap);
#endif
		n=vsnprintf(&b->b[b->i],b->s-b->i,fmt,cp);
		va_end(cp);
		if((n>-1)&&n<(b->s-b->i))
		{
			b->i+=n;
			return;
		}
		if(n>-1)
			fklStringBufferReverse(b,n+1);
		else
			fklStringBufferReverse(b,(b->s)*2);
	}
}

inline void fklStringBufferPrintf(FklStringBuffer* b,const char* fmt,...)
{
	va_list ap;
	va_start(ap,fmt);
	string_buffer_printf_va(b,fmt,ap);
	va_end(ap);
}

inline void fklStringBufferConcatWithCstr(FklStringBuffer* b,const char* s)
{
	fklStringBufferBincpy(b,s,strlen(s));
}

inline void fklStringBufferConcatWithString(FklStringBuffer* b,const FklString* s)
{
	fklStringBufferBincpy(b,s->str,s->size);
}

inline void fklStringBufferConcatWithStringBuffer(FklStringBuffer* a,const FklStringBuffer* b)
{
	fklStringBufferBincpy(a,b->b,b->i);
}

#define DEFAULT_HASH_TABLE_SIZE (4)

inline void fklInitHashTable(FklHashTable* r,const FklHashTableMetaTable* t)
{
	FklHashTableNode** base=(FklHashTableNode**)calloc(DEFAULT_HASH_TABLE_SIZE,sizeof(FklHashTableNode*));
	FKL_ASSERT(base);
	r->base=base;
	r->list=NULL;
	r->tail=&r->list;
	r->num=0;
	r->size=DEFAULT_HASH_TABLE_SIZE;
	r->mask=DEFAULT_HASH_TABLE_SIZE-1;
	r->t=t;
}

FklHashTable* fklCreateHashTable(const FklHashTableMetaTable* t)
{
	FklHashTable* r=(FklHashTable*)malloc(sizeof(FklHashTable));
	FKL_ASSERT(r);
	fklInitHashTable(r,t);
	return r;
}

#define HASH_FUNC_HEADER uintptr_t (*hashv)(const void*)=ht->t->__hashFunc;\
	void* (*key)(void*)=ht->t->__getKey;\
	int (*keq)(const void*,const void*)=ht->t->__keyEqual;\

static inline uint32_t hash32shift(uint32_t k,uint32_t mask)
{
	k=~k+(k<<15);
	k=k^(k>>12);
	k=k+(k<<2);
	k=k^(k>>4);
	k=(k+(k<<3))+(k<<11);
	k=k^(k>>16);
	return k&mask;
}

void* fklGetHashItem(void* pkey,const FklHashTable* ht)
{
	HASH_FUNC_HEADER;

	for(FklHashTableNode* p=ht->base[hash32shift(hashv(pkey),ht->mask)];p;p=p->next)
		if(keq(pkey,key(p->data)))
			return p->data;
	return NULL;
}

static FklHashTableNodeList* createHashTableNodeList(FklHashTableNode* node)
{
	FklHashTableNodeList* list=(FklHashTableNodeList*)malloc(sizeof(FklHashTableNodeList));
	FKL_ASSERT(node);
	list->node=node;
	list->next=NULL;
	return list;
}

#define REHASH() if(isgreater((double)ht->num/ht->size,FKL_DEFAULT_HASH_LOAD_FACTOR))\
	expandHashTable(ht);

static FklHashTableNode* createHashTableNode(size_t size,FklHashTableNode* next)
{
	FklHashTableNode* node=(FklHashTableNode*)calloc(1,sizeof(FklHashTableNode)+size);
	FKL_ASSERT(node);
	node->next=next;
	return node;
}

static inline void putHashNode(FklHashTableNode* node,FklHashTable* ht)
{
	void* pkey=ht->t->__getKey(node->data);
	FklHashTableNode** pp=&ht->base[hash32shift(ht->t->__hashFunc(pkey),ht->mask)];
	node->next=*pp;
	*pp=node;
}

static inline uint32_t next_pow2(uint32_t n)
{
	n--;
	n|=n>>1;
	n|=n>>2;
	n|=n>>4;
	n|=n>>8;
	n|=n>>16;
	return n+1;
}

inline void fklClearHashTable(FklHashTable* ht)
{
	for(FklHashTableNodeList* list=ht->list;list;)
	{
		FklHashTableNodeList* cur=list;
		free(list->node);
		list=list->next;
		free(cur);
	}
	ht->num=0;
	ht->list=NULL;
	ht->tail=&ht->list;
	ht->size=DEFAULT_HASH_TABLE_SIZE;
	ht->mask=DEFAULT_HASH_TABLE_SIZE-1;
	FklHashTableNode** base=(FklHashTableNode**)calloc(ht->size,sizeof(FklHashTableNode*));
	FKL_ASSERT(base);
	free(ht->base);
	ht->base=base;
}

#undef DEFAULT_HASH_TABLE_SIZE

static inline void expandHashTable(FklHashTable* table)
{
	table->size=next_pow2(table->num);
	table->mask=table->size-1;
	FklHashTableNodeList* list=table->list;
	free(table->base);
	FklHashTableNode** nbase=(FklHashTableNode**)calloc(table->size,sizeof(FklHashTableNode*));
	FKL_ASSERT(nbase);
	table->base=nbase;
	for(;list;list=list->next)
		putHashNode(list->node,table);
}

void* fklPutHashItem(void* pkey,FklHashTable* ht)
{
	HASH_FUNC_HEADER;

	FklHashTableNode** pp=&ht->base[hash32shift(hashv(pkey),ht->mask)];
	int i=0;
	void* d1=NULL;
	for(FklHashTableNode* pn=*pp;pn;pn=pn->next,i++)
		if(keq(key(pn->data),pkey))
			d1=pn->data;
	if(!d1)
	{
		FklHashTableNode* node=createHashTableNode(ht->t->size,*pp);
		ht->t->__setKey(key(node->data),pkey);
		*pp=node;
		*ht->tail=createHashTableNodeList(node);
		ht->tail=&(*ht->tail)->next;
		ht->num++;
		d1=node->data;
		REHASH();
	}
	return d1;
}

void* fklGetOrPutHashItem(void* data,FklHashTable* ht)
{
	HASH_FUNC_HEADER;
	void* pkey=key(data);
	FklHashTableNode** pp=&ht->base[hash32shift(hashv(pkey),ht->mask)];
	int i=0;
	void* d1=NULL;
	for(FklHashTableNode* pn=*pp;pn;pn=pn->next,i++)
		if(keq(key(pn->data),pkey))
			d1=pn->data;
	if(!d1)
	{
		FklHashTableNode* node=createHashTableNode(ht->t->size,*pp);
		ht->t->__setVal(node->data,data);
		d1=node->data;
		*pp=node;
		*ht->tail=createHashTableNodeList(node);
		ht->tail=&(*ht->tail)->next;
		ht->num++;
		REHASH();
	}
	return d1;
}

void* fklHashDefaultGetKey(void* i)
{
	return i;
}

int fklHashPtrKeyEqual(const void* a,const void* b)
{
	return *(const void**)a==*(const void**)b;
}

void fklHashDefaultSetPtrKey(void* k0,const void* k1)
{
	*(void**)k0=*(void* const*)k1;
}

#undef REHASH
#undef HASH_FUNC_HEADER

inline void fklUninitHashTable(FklHashTable* table)
{
	void (*uninitFunc)(void*)=table->t->__uninitItem;
	FklHashTableNodeList* list=table->list;
	while(list)
	{
		FklHashTableNodeList* cur=list;
		uninitFunc(cur->node->data);
		free(cur->node);
		list=list->next;
		free(cur);
	}
	free(table->base);
}

void fklDestroyHashTable(FklHashTable* table)
{
	fklUninitHashTable(table);
	free(table);
}

void fklDoNothingUnintHashItem(void* i)
{
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
	prev=(FklBytevector*)fklRealloc(prev,sizeof(FklBytevector)+(aSize+b->size)*sizeof(char));
	FKL_ASSERT(prev);
	*a=prev;
	prev->size=aSize+b->size;
	memcpy(prev->ptr+aSize,b->ptr,b->size);
}

int fklBytevectorCmp(const FklBytevector* fir,const FklBytevector* sec)
{
	size_t size=fir->size<sec->size?fir->size:sec->size;
	int r=memcmp(fir->ptr,sec->ptr,size);
	if(r)
		return r;
	else if(fir->size<sec->size)
		return -1;
	else if(fir->size>sec->size)
		return 1;
	return r;
}

void fklPrintRawBytevector(const FklBytevector* bv,FILE* fp)
{
	fklPrintRawByteBuf(bv->ptr,bv->size,fp);
}

inline void fklPrintBytevectorToStringBuffer(FklStringBuffer* s,const FklBytevector* bvec)
{
	size_t size=bvec->size;
	const uint8_t* ptr=bvec->ptr;
	fklStringBufferPrintf(s,"#vu8(");
	for(size_t i=0;i<size;i++)
	{
		fklStringBufferPrintf(s,"0x%X",ptr[i]);
		if(i<size-1)
			fklStringBufferPrintf(s," ");
	}
	fklStringBufferPrintf(s,")");
}

FklString* fklBytevectorToString(const FklBytevector* bv)
{
	FklStringBuffer buf=FKL_STRING_BUFFER_INIT;
	fklInitStringBuffer(&buf);
	fklPrintBytevectorToStringBuffer(&buf,bv);
	FklString* r=fklStringBufferToString(&buf);
	fklUninitStringBuffer(&buf);
	return r;
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

void fklInitU8Stack(FklU8Stack* r,size_t size,uint32_t inc)
{
	r->base=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(r->base||!size);
	r->size=size;
	r->inc=inc;
	r->top=0;
}

FklU8Stack* fklCreateU8Stack(size_t size,uint32_t inc)
{
	FklU8Stack* tmp=(FklU8Stack*)malloc(sizeof(FklU8Stack));
	FKL_ASSERT(tmp);
	fklInitU8Stack(tmp,size,inc);
	return tmp;
}

inline void fklUninitU8Stack(FklU8Stack* r)
{
	free(r->base);
}

inline void fklDestroyU8Stack(FklU8Stack* r)
{
	fklUninitU8Stack(r);
	free(r);
}

void fklPushU8Stack(uint8_t data,FklU8Stack* stack)
{
	if(stack->top==stack->size)
	{
		uint8_t* tmpData=(uint8_t*)fklRealloc(stack->base,(stack->size+stack->inc)*sizeof(uint8_t));
		FKL_ASSERT(tmpData);
		stack->base=tmpData;
		stack->size+=stack->inc;
	}
	stack->base[stack->top]=data;
	stack->top+=1;
}

inline uint8_t fklPopU8Stack(FklU8Stack* s)
{
	if(fklIsU8StackEmpty(s))
		return 0;
	return s->base[s->top--];
}

inline uint8_t fklTopU8Stack(FklU8Stack* s)
{
	if(fklIsU8StackEmpty(s))
		return 0;
	return s->base[s->top-1];

}

inline int fklIsU8StackEmpty(FklU8Stack* s)
{
	return s->top==0;
}

void fklRecycleU8Stack(FklU8Stack* s)
{
	if(s->size-s->top>s->inc)
	{
		uint8_t* tmpData=(uint8_t*)fklRealloc(s->base,(s->size-s->inc)*sizeof(uint8_t));
		FKL_ASSERT(tmpData);
		s->base=tmpData;
		s->size-=s->inc;
	}
}

