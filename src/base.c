#include<fakeLisp/base.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/bytecode.h>
#include<fakeLisp/common.h>
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
	r->base=(void**)malloc(size*sizeof(void*));
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

void fklPushFrontPtrStack(void* data,FklPtrStack* stack)
{
	if(stack->top==stack->size)
	{
		void** tmpData=(void**)fklRealloc(stack->base,(stack->size+stack->inc)*sizeof(void*));
		FKL_ASSERT(tmpData);
		stack->base=tmpData;
		stack->size+=stack->inc;
	}
	for(uint32_t top=stack->top;top>0;top--)
		stack->base[top]=stack->base[top-1];
	stack->base[0]=data;
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

void fklInitPtrQueue(FklPtrQueue* q)
{
	q->head=NULL;
	q->tail=&q->head;
}

FklPtrQueue* fklCreatePtrQueue(void)
{
	FklPtrQueue* tmp=(FklPtrQueue*)malloc(sizeof(FklPtrQueue));
	FKL_ASSERT(tmp);

	tmp->head=NULL;
	tmp->tail=&tmp->head;
	return tmp;
}

void fklUninitPtrQueue(FklPtrQueue* q)
{
	FklQueueNode* cur=q->head;
	while(cur)
	{
		FklQueueNode* prev=cur;
		cur=cur->next;
		fklDestroyQueueNode(prev);
	}
}

void fklDestroyPtrQueue(FklPtrQueue* tmp)
{
	fklUninitPtrQueue(tmp);
	free(tmp);
}

int fklIsPtrQueueEmpty(FklPtrQueue* queue)
{
	return queue->head==NULL;
}

uint64_t fklLengthPtrQueue(FklPtrQueue* tmp)
{
	FklQueueNode* cur=tmp->head;
	uint64_t i=0;
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

FklQueueNode* fklPopPtrQueueNode(FklPtrQueue* q)
{
	FklQueueNode* head=q->head;
	if(!head)
		return NULL;
	q->head=head->next;
	if(!q->head)
		q->tail=&q->head;
	return head;
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

void fklPushPtrQueueNodeToFront(FklPtrQueue* q,FklQueueNode* n)
{
	n->next=q->head;
	if(!q->head)
		q->tail=&n->next;
	q->head=n;
}

void fklPushPtrQueueNode(FklPtrQueue* q,FklQueueNode* n)
{
	n->next=NULL;
	*(q->tail)=n;
	q->tail=&n->next;
}

void fklPushPtrQueueToFront(void* data,FklPtrQueue* tmp)
{
	FklQueueNode* tmpNode=fklCreateQueueNode(data);
	tmpNode->next=tmp->head;
	if(!tmp->head)
		tmp->tail=&tmpNode->next;
	tmp->head=tmpNode;
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
	tmp->base=(int64_t*)malloc(size*sizeof(int64_t));
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
	r->base=(uint64_t*)malloc(size*sizeof(uint64_t));
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
		uint32_t size=stack->size-stack->inc;
		uint64_t* tmpData=(uint64_t*)fklRealloc(stack->base,size*sizeof(uint64_t));
		FKL_ASSERT(tmpData||!size);
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
	t->digits=(uint8_t*)malloc(t->num*sizeof(uint8_t));
	FKL_ASSERT(t->digits||!t->num);
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
	t->digits=(uint8_t*)malloc(t->num*sizeof(uint8_t));
	FKL_ASSERT(t->digits||!t->num);
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
	t->digits=(uint8_t*)malloc(t->num*sizeof(uint8_t));
	FKL_ASSERT(t->digits||!t->num);
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
	if(fklIsHexInt(v,len))
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
	else if(fklIsOctInt(v,len))
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
	t->digits=(uint8_t*)malloc(bigint->num*sizeof(uint8_t));
	FKL_ASSERT(t->digits||!bigint->num);
	memcpy(t->digits,bigint->digits,t->num);
	return t;
}

void fklInitBigIntFromDecString(FklBigInt* r,const FklString* str)
{
	const char* buf=str->str;
	size_t len=str->size;
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

void fklInitBigIntFromOctString(FklBigInt* r,const FklString* str)
{
	const char* buf=str->str;
	size_t len=str->size;
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

static inline void ensureBigIntDigits(FklBigInt* obj,uint64_t num)
{
	if(obj->size<num)
	{
		uint8_t* digits=obj->digits;
		obj->digits=(uint8_t*)fklRealloc(digits,num*sizeof(char));
		FKL_ASSERT(obj->digits||!num);
		obj->size=num;
	}
}

void fklInitBigIntFromHexString(FklBigInt* r,const FklString* str)
{
	const char* buf=str->str;
	size_t len=str->size;
	int neg=buf[0]=='-';
	int offset=neg||buf[0]=='+';
	uint64_t i=2+offset;
	for(;i<len&&buf[i]=='0';i++);
	buf=&buf[i];
	len-=i;
	for(i=0;i<len&&isxdigit(buf[i]);i++);
	len=i;
	uint64_t num=len==0?1:len/2+len%2;
	ensureBigIntDigits(r,num);
	r->num=num;
	if(len==0)
	{
		r->digits[0]=0;
		return;
	}

	uint64_t k=0;
	uint64_t char_idx=len-1;
	for(i=0;i<len;i++,char_idx--)
	{
		uint8_t d=0;
		char c=buf[char_idx];
		if(isdigit(c))
			d=c-'0';
		else
			d=toupper(c)-'A'+10;

		if(i%2)
		{
			r->digits[k]|=d<<(FKL_BYTE_WIDTH/2);
			k++;
		}
		else
			r->digits[k]=d;
	}
	r->neg=neg;
}

void fklInitBigIntFromString(FklBigInt* r,const FklString* str)
{
	const char* buf=str->str;
	size_t len=str->size;
	if(fklIsHexInt(buf,len))
		fklInitBigIntFromHexString(r,str);
	else if(fklIsOctInt(buf,len))
		fklInitBigIntFromOctString(r,str);
	else
		fklInitBigIntFromDecString(r,str);
}

void fklInitBigIntFromMemCopy(FklBigInt* t,uint8_t neg,const uint8_t* memptr,size_t num)
{
	t->num=num;
	t->size=num;
	t->neg=neg;
	t->digits=(uint8_t*)malloc(num*sizeof(uint8_t));
	FKL_ASSERT(t->digits||!num);
	memcpy(t->digits,memptr,num);
}

void fklUninitBigInt(FklBigInt* bi)
{
	free(bi->digits);
	bi->digits=NULL;
	bi->neg=0;
	bi->num=0;
	bi->size=0;
}

FklBigInt* fklCreateBigIntFromMemCopy(uint8_t neg,const uint8_t* mem,size_t size)
{
	if(size<1)
		return NULL;
	if(neg>1)
		return NULL;
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	fklInitBigIntFromMemCopy(t,neg,mem,size);
	return t;
}

void fklInitBigIntFromMem(FklBigInt* t,uint8_t neg,uint8_t* memptr,size_t num)
{
	t->num=num;
	t->size=num;
	t->neg=neg;
	t->digits=memptr;
}

FklBigInt* fklCreateBigIntFromMem(uint8_t neg,uint8_t* mem,size_t size)
{
	if(size<1)
		return NULL;
	if(neg>1)
		return NULL;
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	fklInitBigIntFromMem(t,neg,mem,size);
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

static inline void divRemBigIntU8(FklBigInt* a,uint64_t* prem,uint8_t b)
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
		uint8_t* t=(uint8_t*)malloc(bigBase.num);
		FKL_ASSERT(t||!bigBase.num);
		while(bigBase.num<targetLen)
		{
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
		free(t);
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

static inline void toBitWiseDigitsLe(const FklBigInt* u,uint8_t bits,FklU8Stack* res)
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

static inline void toInexactBitWiseDigitsLe(const FklBigInt* u,uint8_t bits,FklU8Stack* res)
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

uintptr_t fklBigIntHash(const FklBigInt* bi)
{
	uintptr_t r=0;
	for(size_t i=0;i<bi->size;i++)
	{
		uint64_t c=bi->digits[i];
		r+=c<<(i%8);
	}
	if(bi->neg)
		r*=-1;
	return r;
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
	t->digits=(uint8_t*)malloc(t->num*sizeof(uint8_t));
	FKL_ASSERT(t->digits||!t->num);
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
	t->digits=(uint8_t*)malloc(t->num*sizeof(uint8_t));
	FKL_ASSERT(t->digits||!t->num);
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

static inline int cmpDigits(const FklBigInt* a,const FklBigInt* b)
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

int fklBigIntEqual(const FklBigInt* a,const FklBigInt* b)
{
	if((FKL_IS_0_BIG_INT(a)&&FKL_IS_0_BIG_INT(b))
			||(a->num==b->num
				&&a->neg==b->neg
				&&!memcmp(a->digits,b->digits,a->num)))
		return 1;
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

static inline void addDigits(FklBigInt* a,const FklBigInt* addend)
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

static inline void subDigits(FklBigInt* a,const FklBigInt* sub)
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
		uint8_t* res=(uint8_t*)malloc(a->num);
		FKL_ASSERT(res||!a->num);
		uint64_t num=0;
		FklBigInt s=FKL_BIG_INT_INIT;
		uint8_t* sdigits=(uint8_t*)malloc(a->num+1);
		FKL_ASSERT(sdigits);
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
		free(res);
		free(sdigits);
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
		uint8_t* res=(uint8_t*)malloc(a->num);
		FKL_ASSERT(res||!a->num);
		uint64_t num=0;
		FklBigInt s=FKL_BIG_INT_INIT;
		uint8_t* sdigits=(uint8_t*)malloc(a->num+1);
		FKL_ASSERT(sdigits);
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
		free(res);
		free(sdigits);
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
		uint8_t* sdigits=(uint8_t*)malloc(a->num+1);
		FKL_ASSERT(sdigits);
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
		free(sdigits);
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

int fklIsBigIntAdd1InFixIntRange(const FklBigInt* a)
{
	return fklCmpBigInt(a,&FKL_BIG_INT_FIX_ADD_1_MIN)==0;
}

int fklIsBigIntSub1InFixIntRange(const FklBigInt* a)
{
	return fklCmpBigInt(a,&FKL_BIG_INT_FIX_SUB_1_MAX)==0;
}

int fklIsGtLtFixBigInt(const FklBigInt* a)
{
	return (fklCmpBigInt(a,&FKL_BIG_INT_FIX_MAX)>0||fklCmpBigInt(a,&FKL_BIG_INT_FIX_MIN)<0);
}

int fklIsGeLeFixBigInt(const FklBigInt* a)
{
	return (fklCmpBigInt(a,&FKL_BIG_INT_FIX_MAX)>=0||fklCmpBigInt(a,&FKL_BIG_INT_FIX_MIN)<=0);
}

int fklIsGeLeI64BigInt(const FklBigInt* a)
{
	return (fklCmpBigInt(a,&FKL_BIG_INT_I64_MAX)>=0||fklCmpBigInt(a,&FKL_BIG_INT_I64_MIN)<=0);
}

int fklIsGtLtI64BigInt(const FklBigInt* a)
{
	return (fklCmpBigInt(a,&FKL_BIG_INT_I64_MAX)>0||fklCmpBigInt(a,&FKL_BIG_INT_I64_MIN)<0);
}

int fklIsGtU64MaxBigInt(const FklBigInt* a)
{
	return fklCmpBigInt(a,&FKL_BIG_INT_U64_MAX)>0;
}

int fklIsGtI64MaxBigInt(const FklBigInt* a)
{
	return fklCmpBigInt(a,&FKL_BIG_INT_I64_MAX)>0;
}

int fklIsLtI64MinBigInt(const FklBigInt* a)
{
	return fklCmpBigInt(a,&FKL_BIG_INT_I64_MIN)<0;
}

int fklIsBigIntOdd(const FklBigInt* a)
{
	return a->digits[0]%2;
}

int fklIsBigIntEven(const FklBigInt* a)
{
	return !fklIsBigIntOdd(a);
}

int fklIsBigIntLt0(const FklBigInt *a)
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

FklString* fklBigIntToString(const FklBigInt* a
		,int radix
		,int need_prefix
		,int capitals)
{
	if(FKL_IS_0_BIG_INT(a))
		return fklCreateStringFromCstr("0");
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
			if(need_prefix)
			{
				len+=2;
				j+=2;
			}
		}
		else if(radix==8)
		{
			toInexactBitWiseDigitsLe(a,3,&res);

			if(need_prefix)
			{
				len++;
				j++;
			}
		}
		len+=res.top;
		FklString* s=fklCreateString(sizeof(char)*len,NULL);
		char* buf=s->str;
		if(a->neg)
			buf[0]='-';
		if(need_prefix)
		{
			if(radix==16)
			{
				buf[a->neg]='0';
				buf[a->neg+1]=capitals?'X':'x';
			}
			else if(radix==8)
				buf[a->neg]='0';

		}
		if(capitals)
		{
			for(size_t i=res.top;i>0;i--,j++)
			{
				uint64_t c=res.base[i-1];
				buf[j]=c<10?c+'0':c-10+'A';
			}
		}
		else
		{
			for(size_t i=res.top;i>0;i--,j++)
			{
				uint64_t c=res.base[i-1];
				buf[j]=c<10?c+'0':c-10+'a';
			}
		}
		fklUninitU8Stack(&res);
		return s;
	}
}

void fklBigIntToStringBuffer(FklStringBuffer* buf
		,const FklBigInt* a
		,int radix
		,int need_prefix
		,int capitals)
{
	if(FKL_IS_0_BIG_INT(a))
	{
		fklStringBufferPutc(buf,'0');
		return;
	}
	else
	{
		FklU8Stack res=FKL_STACK_INIT;
		if(radix==10)
			fklBigIntToRadixDigitsLe(a,radix,&res);
		else if(radix==16)
			toBitWiseDigitsLe(a,4,&res);
		else if(radix==8)
			toInexactBitWiseDigitsLe(a,3,&res);
		if(a->neg)
			fklStringBufferPutc(buf,'-');
		if(need_prefix)
		{
			if(radix==16)
			{
				fklStringBufferPutc(buf,'0');
				fklStringBufferPutc(buf,capitals?'X':'x');
			}
			else if(radix==8)
				fklStringBufferPutc(buf,'0');

		}
		if(capitals)
		{
			for(size_t i=res.top;i>0;i--)
			{
				uint64_t c=res.base[i-1];
				fklStringBufferPutc(buf,c<10?c+'0':c-10+'A');
			}
		}
		else
		{
			for(size_t i=res.top;i>0;i--)
			{
				uint64_t c=res.base[i-1];
				fklStringBufferPutc(buf,c<10?c+'0':c-10+'a');
			}
		}
		fklUninitU8Stack(&res);
		return;
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

FklString* fklCreateString(size_t size,const char* str)
{
	FklString* tmp=(FklString*)malloc(sizeof(FklString)+size*sizeof(uint8_t));
	FKL_ASSERT(tmp);
	tmp->size=size;
	if(str)
		memcpy(tmp->str,str,size);
	tmp->str[size]='\0';
	return tmp;
}

FklString* fklStringRealloc(FklString* str,size_t new_size)
{
	FklString* new=(FklString*)fklRealloc(str,sizeof(FklString)+new_size*sizeof(uint8_t));
	FKL_ASSERT(new);
	new->str[new_size]='\0';
	return new;
}

int fklStringEqual(const FklString* fir,const FklString* sec)
{
	if(fir->size==sec->size)
		return !memcmp(fir->str,sec->str,fir->size);
	return 0;
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

size_t fklStringCharBufMatch(const FklString* a,const char* b,size_t b_size)
{
	return fklCharBufMatch(a->str,a->size,b,b_size);
}

size_t fklCharBufMatch(const char* a,size_t a_size,const char* b,size_t b_size)
{
	if(b_size<a_size)
		return 0;
	if(!memcmp(a,b,a_size))
		return a_size;
	return 0;
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

void fklStringCharBufCat(FklString** a,const char* buf,size_t s)
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

int fklIsSpecialCharAndPrintToStringBuffer(FklStringBuffer* s,char ch)
{
	int r=0;
	if((r=ch=='\n'))
		fklStringBufferConcatWithCstr(s,"\\n");
	else if((r=ch=='\t'))
		fklStringBufferConcatWithCstr(s,"\\t");
	else if((r=ch=='\v'))
		fklStringBufferConcatWithCstr(s,"\\v");
	else if((r=ch=='\a'))
		fklStringBufferConcatWithCstr(s,"\\a");
	else if((r=ch=='\b'))
		fklStringBufferConcatWithCstr(s,"\\b");
	else if((r=ch=='\f'))
		fklStringBufferConcatWithCstr(s,"\\f");
	else if((r=ch=='\r'))
		fklStringBufferConcatWithCstr(s,"\\r");
	else if((r=ch=='\x20'))
		fklStringBufferPutc(s,' ');
	return r;
}

void fklPrintRawCstrToStringBuffer(FklStringBuffer* s,const char* str,char se)
{
	char buf[7]={0};
	fklStringBufferPutc(s,se);
	size_t size=strlen(str);
	for(size_t i=0;i<size;)
	{
		unsigned int l=fklGetByteNumOfUtf8((uint8_t*)&str[i],size-i);
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
				fklStringBufferConcatWithCstr(s,"\\\\");
			else if(isgraph(str[i]))
				fklStringBufferPutc(s,str[i]);
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
			fklStringBufferConcatWithCstr(s,buf);
			i+=l;
		}
	}
	fklStringBufferPutc(s,se);
}

void fklPrintRawStringToStringBuffer(FklStringBuffer* s,const FklString* fstr,char se)
{
	char buf[7]={0};
	fklStringBufferPutc(s,se);
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
				fklStringBufferConcatWithCstr(s,"\\\\");
			else if(isgraph(str[i]))
				fklStringBufferPutc(s,str[i]);
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
			fklStringBufferConcatWithCstr(s,buf);
			i+=l;
		}
	}
	fklStringBufferPutc(s,se);
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
	fir=(char*)fklRealloc(fir,len*sizeof(char)+sec->size+1);
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

uintptr_t fklCharBufHash(const char* str,size_t len)
{
	uintptr_t h=0;
	for(size_t i=0;i<len;i++)
		h=31*h+str[i];
	return h;
}

uintptr_t fklStringHash(const FklString* s)
{
	return fklCharBufHash(s->str,s->size);
}

size_t fklCountCharInString(FklString* s,char c)
{
	return fklCountCharInBuf(s->str,s->size,c);
}

void fklInitStringBuffer(FklStringBuffer* b)
{
	b->index=0;
	b->size=0;
	b->buf=NULL;
	fklStringBufferReverse(b,64);
}

void fklInitStringBufferWithCapacity(FklStringBuffer* b,size_t len)
{
	b->index=0;
	b->size=0;
	b->buf=NULL;
	fklStringBufferReverse(b,len);
}

uint32_t fklStringBufferLen(FklStringBuffer* b)
{
	return b->index;
}

char* fklStringBufferBody(FklStringBuffer* b)
{
	return b->buf;
}

void fklStringBufferPutc(FklStringBuffer* b,char c)
{
	fklStringBufferReverse(b,2);
	b->buf[b->index++]=c;
	b->buf[b->index]='\0';
}

void fklStringBufferBincpy(FklStringBuffer* b,const void* p,size_t l)
{
	fklStringBufferReverse(b,l+1);
	memcpy(&b->buf[b->index],p,l);
	b->index+=l;
	b->buf[b->index]='\0';
}

FklString* fklStringBufferToString(FklStringBuffer* b)
{
	return fklCreateString(b->index,b->buf);
}

FklBytevector* fklStringBufferToBytevector(FklStringBuffer* b)
{
	return fklCreateBytevector(b->index,(uint8_t*)b->buf);
}

FklStringBuffer* fklCreateStringBuffer(void)
{
	FklStringBuffer* r=(FklStringBuffer*)malloc(sizeof(FklStringBuffer));
	FKL_ASSERT(r);
	fklInitStringBuffer(r);
	return r;
}

void fklUninitStringBuffer(FklStringBuffer* b)
{
	b->size=0;
	b->index=0;
	free(b->buf);
	b->buf=NULL;
}

void fklDestroyStringBuffer(FklStringBuffer* b)
{
	fklUninitStringBuffer(b);
	free(b);
}

void fklStringBufferClear(FklStringBuffer* b)
{
	b->index=0;
}

void fklStringBufferFill(FklStringBuffer* b,char c)
{
	memset(b->buf,c,b->index);
}

void fklStringBufferReverse(FklStringBuffer* b,size_t s)
{
	if((b->size-b->index)<s)
	{
		b->size+=s;
		char* t=(char*)fklRealloc(b->buf,b->size);
		FKL_ASSERT(t);
		b->buf=t;
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
		n=vsnprintf(&b->buf[b->index],b->size-b->index,fmt,cp);
		va_end(cp);
		if((n>-1)&&n<(b->size-b->index))
		{
			b->index+=n;
			return;
		}
		if(n>-1)
			fklStringBufferReverse(b,n+1);
		else
			fklStringBufferReverse(b,(b->size)*2);
	}
}

void fklStringBufferPrintf(FklStringBuffer* b,const char* fmt,...)
{
	va_list ap;
	va_start(ap,fmt);
	string_buffer_printf_va(b,fmt,ap);
	va_end(ap);
}

void fklStringBufferConcatWithCstr(FklStringBuffer* b,const char* s)
{
	fklStringBufferBincpy(b,s,strlen(s));
}

void fklStringBufferConcatWithString(FklStringBuffer* b,const FklString* s)
{
	fklStringBufferBincpy(b,s->str,s->size);
}

void fklStringBufferConcatWithStringBuffer(FklStringBuffer* a,const FklStringBuffer* b)
{
	fklStringBufferBincpy(a,b->buf,b->index);
}

#define DEFAULT_HASH_TABLE_SIZE (4)

void fklInitHashTable(FklHashTable* r,const FklHashTableMetaTable* t)
{
	FklHashTableItem** base=(FklHashTableItem**)calloc(DEFAULT_HASH_TABLE_SIZE,sizeof(FklHashTableItem*));
	FKL_ASSERT(base);
	r->base=base;
	r->first=NULL;
	r->last=NULL;
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

void fklPtrKeySet(void* k0,const void* k1)
{
	*(void**)k0=*(void* const*)k1;
}

int fklPtrKeyEqual(const void* k0,const void* k1)
{
	return *(void* const*)k0==*(void* const*)k1;
}

uintptr_t fklPtrKeyHashFunc(const void* k)
{
	return (uintptr_t)*(void* const*)k;
}

static const FklHashTableMetaTable PtrSetMetaTable=
{
	.size=sizeof(void*),
	.__setKey=fklPtrKeySet,
	.__setVal=fklPtrKeySet,
	.__hashFunc=fklPtrKeyHashFunc,
	.__keyEqual=fklPtrKeyEqual,
	.__getKey=fklHashDefaultGetKey,
	.__uninitItem=fklDoNothingUninitHashItem,
};

void fklInitPtrSet(FklHashTable* r)
{
	fklInitHashTable(r,&PtrSetMetaTable);
}

FklHashTable* fklCreatePtrSet(void)
{
	return fklCreateHashTable(&PtrSetMetaTable);
}

#define HASH_FUNC_HEADER() uintptr_t (*hashv)(const void*)=ht->t->__hashFunc;\
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

void* fklGetHashItem(const void* pkey,const FklHashTable* ht)
{
	HASH_FUNC_HEADER();

	for(FklHashTableItem* p=ht->base[hash32shift(hashv(pkey),ht->mask)];p;p=p->ni)
		if(keq(pkey,key(p->data)))
			return p->data;
	return NULL;
}

FklHashTableItem** fklGetHashItemSlot(FklHashTable* ht,uintptr_t hashv)
{
	return &ht->base[hash32shift(hashv,ht->mask)];
}

#define REHASH() if(isgreater((double)ht->num/ht->size,FKL_DEFAULT_HASH_LOAD_FACTOR))\
	expandHashTable(ht);

static inline FklHashTableItem* createHashTableItem(size_t size,FklHashTableItem* ni)
{
	FklHashTableItem* node=(FklHashTableItem*)calloc(1,sizeof(FklHashTableItem)+size);
	FKL_ASSERT(node);
	node->ni=ni;
	return node;
}

static inline void putHashNode(FklHashTableItem* node,FklHashTable* ht)
{
	void* pkey=ht->t->__getKey(node->data);
	FklHashTableItem** pp=&ht->base[hash32shift(ht->t->__hashFunc(pkey),ht->mask)];
	node->ni=*pp;
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

void fklClearHashTable(FklHashTable* ht)
{
	void (*uninitFunc)(void*)=ht->t->__uninitItem;
	for(FklHashTableItem* list=ht->first;list;)
	{
		FklHashTableItem* cur=list;
		uninitFunc(cur->data);
		list=list->next;
		free(cur);
	}
	memset(ht->base,0,sizeof(FklHashTableItem*)*ht->size);
	ht->num=0;
	ht->first=NULL;
	ht->last=NULL;
	ht->size=DEFAULT_HASH_TABLE_SIZE;
	ht->mask=DEFAULT_HASH_TABLE_SIZE-1;
}

#undef DEFAULT_HASH_TABLE_SIZE

static inline void expandHashTable(FklHashTable* table)
{
	table->size=next_pow2(table->num);
	table->mask=table->size-1;
	FklHashTableItem* list=table->first;
	free(table->base);
	FklHashTableItem** nbase=(FklHashTableItem**)calloc(table->size,sizeof(FklHashTableItem*));
	FKL_ASSERT(nbase);
	table->base=nbase;
	for(;list;list=list->next)
		putHashNode(list,table);
}

void fklRehashTable(FklHashTable* table)
{
	FklHashTableItem* list=table->first;
	memset(table->base,0,sizeof(FklHashTableItem*)*table->size);
	for(;list;list=list->next)
		putHashNode(list,table);
}

int fklDelHashItem(void* pkey,FklHashTable* ht,void* deleted)
{
	HASH_FUNC_HEADER();
	FklHashTableItem** p=&ht->base[hash32shift(hashv(pkey),ht->mask)];
	for(;*p;p=&(*p)->ni)
		if(keq(pkey,key((*p)->data)))
			break;

	FklHashTableItem* item=*p;
	if(item)
	{
		*p=item->ni;
		if(item->prev)
			item->prev->next=item->next;
		if(item->next)
			item->next->prev=item->prev;
		if(ht->last==item)
			ht->last=item->prev;
		if(ht->first==item)
			ht->first=item->next;

		ht->num--;
		if(deleted)
			ht->t->__setVal(deleted,item->data);
		else
		{
			void (*uninitFunc)(void*)=ht->t->__uninitItem;
			uninitFunc(item->data);
		}
		free(item);
		return 1;
	}
	return 0;
}

void fklRemoveHashItem(FklHashTable* ht,FklHashTableItem* hi)
{
	uintptr_t (*hashv)(const void*)=ht->t->__hashFunc;
	void* (*key)(void*)=ht->t->__getKey;
	void* data=hi->data;

	void* pkey=key(data);
	FklHashTableItem** p=&ht->base[hash32shift(hashv(pkey),ht->mask)];
	for(;*p;p=&(*p)->ni)
		if(*p==hi)
			break;

	if(*p)
	{
		*p=hi->ni;
		if(hi->prev)
			hi->prev->next=hi->next;
		if(hi->next)
			hi->next->prev=hi->prev;
		if(ht->last==hi)
			ht->last=hi->prev;
		if(ht->first==hi)
			ht->first=hi->next;
		ht->num--;
	}
}

void fklRemoveHashItemWithSlot(FklHashTable* ht,FklHashTableItem** p)
{
	FklHashTableItem* item=*p;
	if(item)
	{
		*p=item->ni;
		if(item->prev)
			item->prev->next=item->next;
		if(item->next)
			item->next->prev=item->prev;
		if(ht->last==item)
			ht->last=item->prev;
		if(ht->first==item)
			ht->first=item->next;

		ht->num--;
		void (*uninitFunc)(void*)=ht->t->__uninitItem;
		uninitFunc(item->data);
		free(item);
	}
}

void* fklPutHashItemInSlot(FklHashTable* ht,FklHashTableItem** pp)
{
	FklHashTableItem* node=createHashTableItem(ht->t->size,*pp);
	*pp=node;
	if(ht->first)
	{
		node->prev=ht->last;
		ht->last->next=node;
	}
	else
		ht->first=node;
	ht->last=node;
	ht->num++;
	REHASH();
	return node->data;
}

void* fklPutHashItem(const void* pkey,FklHashTable* ht)
{
	HASH_FUNC_HEADER();

	FklHashTableItem** pp=&ht->base[hash32shift(hashv(pkey),ht->mask)];
	void* d1=NULL;
	for(FklHashTableItem* pn=*pp;pn;pn=pn->ni)
		if(keq(key(pn->data),pkey))
			d1=pn->data;
	if(!d1)
	{
		FklHashTableItem* node=createHashTableItem(ht->t->size,*pp);
		ht->t->__setKey(key(node->data),pkey);
		*pp=node;
		if(ht->first)
		{
			node->prev=ht->last;
			ht->last->next=node;
		}
		else
			ht->first=node;
		ht->last=node;
		ht->num++;
		d1=node->data;
		REHASH();
	}
	return d1;
}

void* fklGetOrPutWithOtherKey(void* pkey
		,uintptr_t (*hashv)(const void* key)
		,int (*keq)(const void* k0,const void* k1)
		,void (*setVal)(void* d0,const void* d1)
		,FklHashTable* ht)
{
	void* (*key)(void*)=ht->t->__getKey;
	FklHashTableItem** pp=&ht->base[hash32shift(hashv(pkey),ht->mask)];
	void* d1=NULL;
	for(FklHashTableItem* pn=*pp;pn;pn=pn->ni)
		if(keq(key(pn->data),pkey))
		{
			d1=pn->data;
			break;
		}
	if(!d1)
	{
		FklHashTableItem* node=createHashTableItem(ht->t->size,*pp);
		setVal(node->data,pkey);
		d1=node->data;
		*pp=node;
		if(ht->first)
		{
			node->prev=ht->last;
			ht->last->next=node;
		}
		else
			ht->first=node;
		ht->last=node;
		ht->num++;
		REHASH();
	}
	return d1;
}

void* fklGetOrPutHashItem(void* data,FklHashTable* ht)
{
	HASH_FUNC_HEADER();
	void* pkey=key(data);
	FklHashTableItem** pp=&ht->base[hash32shift(hashv(pkey),ht->mask)];
	void* d1=NULL;
	for(FklHashTableItem* pn=*pp;pn;pn=pn->ni)
		if(keq(key(pn->data),pkey))
		{
			d1=pn->data;
			break;
		}
	if(!d1)
	{
		FklHashTableItem* node=createHashTableItem(ht->t->size,*pp);
		ht->t->__setVal(node->data,data);
		d1=node->data;
		*pp=node;
		if(ht->first)
		{
			node->prev=ht->last;
			ht->last->next=node;
		}
		else
			ht->first=node;
		ht->last=node;
		ht->num++;
		REHASH();
	}
	return d1;
}

int fklPutHashItem2(FklHashTable* ht,const void* pkey,void** pitem)
{
	HASH_FUNC_HEADER();
	FklHashTableItem** pp=&ht->base[hash32shift(hashv(pkey),ht->mask)];
	void* d1=NULL;
	for(FklHashTableItem* pn=*pp;pn;pn=pn->ni)
		if(keq(key(pn->data),pkey))
		{
			d1=pn->data;
			break;
		}
	if(!d1)
	{
		FklHashTableItem* node=createHashTableItem(ht->t->size,*pp);
		ht->t->__setKey(node->data,pkey);
		d1=node->data;
		*pp=node;
		if(ht->first)
		{
			node->prev=ht->last;
			ht->last->next=node;
		}
		else
			ht->first=node;
		ht->last=node;
		ht->num++;
		REHASH();
		if(pitem)
			*pitem=d1;
		return 0;
	}
	else
	{
		if(pitem)
			*pitem=d1;
		return 1;
	}
}

void* fklHashDefaultGetKey(void* i)
{
	return i;
}

#undef REHASH
#undef HASH_FUNC_HEADER

void fklUninitHashTable(FklHashTable* table)
{
	void (*uninitFunc)(void*)=table->t->__uninitItem;
	FklHashTableItem* list=table->first;
	while(list)
	{
		FklHashTableItem* cur=list;
		uninitFunc(cur->data);
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

void fklDoNothingUninitHashItem(void* i)
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

FklBytevector* fklBytevectorRealloc(FklBytevector* bvec,size_t new_size)
{
	FklBytevector* new=(FklBytevector*)fklRealloc(bvec,sizeof(FklBytevector)+new_size*sizeof(uint8_t));
	FKL_ASSERT(new);
	return new;
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

int fklBytevectorEqual(const FklBytevector* fir,const FklBytevector* sec)
{
	if(fir->size==sec->size)
		return !memcmp(fir->ptr,sec->ptr,fir->size);
	return 0;
}

void fklPrintRawBytevector(const FklBytevector* bv,FILE* fp)
{
	fklPrintRawByteBuf(bv->ptr,bv->size,fp);
}

void fklPrintBytevectorToStringBuffer(FklStringBuffer* s,const FklBytevector* bvec)
{
	size_t size=bvec->size;
	const uint8_t* ptr=bvec->ptr;
	fklStringBufferConcatWithCstr(s,"#vu8(");
	for(size_t i=0;i<size;i++)
	{
		fklStringBufferPrintf(s,"0x%X",ptr[i]);
		if(i<size-1)
			fklStringBufferPutc(s,' ');
	}
	fklStringBufferPutc(s,')');
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

uintptr_t fklBytevectorHash(const FklBytevector* bv)
{
	uintptr_t h=0;
	const uint8_t* val=bv->ptr;
	size_t size=bv->size;
	for(size_t i=0;i<size;i++)
		h=31*h+val[i];
	return h;
}

void fklInitU8Stack(FklU8Stack* r,size_t size,uint32_t inc)
{
	r->base=(uint8_t*)malloc(size*sizeof(uint8_t));
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

void fklUninitU8Stack(FklU8Stack* r)
{
	free(r->base);
}

void fklDestroyU8Stack(FklU8Stack* r)
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

uint8_t fklPopU8Stack(FklU8Stack* s)
{
	if(fklIsU8StackEmpty(s))
		return 0;
	return s->base[--s->top];
}

uint8_t fklTopU8Stack(FklU8Stack* s)
{
	if(fklIsU8StackEmpty(s))
		return 0;
	return s->base[s->top-1];

}

int fklIsU8StackEmpty(FklU8Stack* s)
{
	return s->top==0;
}

void fklRecycleU8Stack(FklU8Stack* s)
{
	if(s->size-s->top>s->inc)
	{
		uint32_t size=s->size-s->inc;
		uint8_t* tmpData=(uint8_t*)fklRealloc(s->base,size*sizeof(uint8_t));
		FKL_ASSERT(tmpData||!size);
		s->base=tmpData;
		s->size-=s->inc;
	}
}

