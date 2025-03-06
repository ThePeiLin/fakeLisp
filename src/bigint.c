#include "fakeLisp/base.h"
#include<fakeLisp/bigint.h>
#include<fakeLisp/utils.h>

#include<string.h>
#include<ctype.h>
#include<limits.h>

#define NFKL_BIGINT_DIGIT_SHIFT (30)
#define NFKL_BIGINT_DIGIT_BASE ((NfklBigIntDigit)1<<NFKL_BIGINT_DIGIT_SHIFT)
#define NFKL_BIGINT_DIGIT_MASK ((NfklBigIntDigit)(NFKL_BIGINT_DIGIT_BASE-1))
#define NFKL_BIGINT_DECIMAL_SHIFT (9)
#define NFKL_BIGINT_DECIMAL_BASE ((NfklBigIntDigit)1000000000)

void nfklInitBigInt0(NfklBigInt* b)
{
	*b=NFKL_BIGINT_0;
}

void nfklInitBigInt1(NfklBigInt* b)
{
	*b=NFKL_BIGINT_0;
	b->num=1;
	b->size=1;
	b->digits=(NfklBigIntDigit*)malloc(b->size*sizeof(NfklBigIntDigit));
	FKL_ASSERT(b->digits);
	b->digits[0]=1;
}

void nfklInitBigIntN1(NfklBigInt* b)
{
	nfklInitBigInt1(b);
	b->num=-1;
}

static inline size_t count_digits_size(uint64_t i)
{
	if(i)
	{
		size_t count=0;
		for(;i;i>>=NFKL_BIGINT_DIGIT_SHIFT)
			++count;
		return count;
	}
	return 0;
}

static inline void set_uint64_to_digits(NfklBigIntDigit* digits,size_t size,uint64_t num)
{
	for(size_t i=0;i<size;i++)
	{
		digits[i]=num&NFKL_BIGINT_DIGIT_MASK;
		num>>=NFKL_BIGINT_DIGIT_SHIFT;
	}
}

void nfklInitBigIntU(NfklBigInt* b,uint64_t num)
{
	nfklInitBigInt0(b);
	if(num)
	{
		b->size=count_digits_size(num);
		b->num=b->size;
		b->digits=(NfklBigIntDigit*)malloc(b->size*sizeof(NfklBigIntDigit));
		FKL_ASSERT(b->digits);
		set_uint64_to_digits(b->digits,b->size,num);
	}

}

void nfklInitBigIntI(NfklBigInt* b,int64_t n)
{
	nfklInitBigIntU(b,labs(n));
	if(n<0)
		b->num=-b->num;
}

void nfklInitBigIntD(NfklBigInt* b,double d)
{
	nfklInitBigIntI(b,(int64_t)d);
}

void nfklInitBigIntWithDecCharBuf(NfklBigInt* b,const char* buf,size_t len)
{
	int neg=buf[0]=='-';
	size_t offset=neg||buf[0]=='+';
	for(size_t i=offset;i<len&&isdigit(buf[i]);i++)
	{
		nfklMulBigIntI(b,10);
		nfklAddBigIntI(b,buf[i]-'0');
	}
	if(neg)
		b->num=-b->num;
}

static void ensure_bigint_size(NfklBigInt* to,uint64_t size)
{
	if(to->size<size)
	{
		to->size=size;
		to->digits=(NfklBigIntDigit*)fklRealloc(to->digits,size*sizeof(NfklBigIntDigit));
		FKL_ASSERT(to->digits);
	}
}

static inline void bigint_normalize(NfklBigInt* a)
{
	int64_t i=labs(a->num);
	for(;i>0&&a->digits[i-1]==0;--i);
	a->num=a->num<0?-i:i;
}

#define OCT_BIT_COUNT (3)
#define DIGIT_OCT_COUNT (NFKL_BIGINT_DIGIT_SHIFT/OCT_BIT_COUNT)

void nfklInitBigIntWithOctCharBuf(NfklBigInt* b,const char* buf,size_t len)
{
	int neg=buf[0]=='-';
	size_t offset=(neg||buf[0]=='+')+1;
	for(;offset<len&&buf[offset]=='0';offset++);
	size_t actual_len=len-offset;
	size_t high_len=actual_len%DIGIT_OCT_COUNT;
	size_t total_len=actual_len/DIGIT_OCT_COUNT+(high_len>0);
	ensure_bigint_size(b,total_len);
	b->num=total_len;
	if(total_len==0)
		return;
	NfklBigIntDigit* pdigits=b->digits;
	size_t i=offset;
	uint32_t carry=0;
	for(;i<offset+high_len;i++)
	{
		carry<<=OCT_BIT_COUNT;
		carry|=buf[i]-'0';
	}
	pdigits[--total_len]=carry;
	for(;i<len;i+=DIGIT_OCT_COUNT)
	{
		carry=0;
		for(size_t j=0;j<DIGIT_OCT_COUNT;j++)
		{
			carry<<=OCT_BIT_COUNT;
			carry|=buf[i+j]-'0';
		}
		pdigits[--total_len]=carry;
	}
	bigint_normalize(b);
	if(neg)
		b->num=-b->num;
}

#define HEX_BIT_COUNT (4)
#define U32_HEX_COUNT ((sizeof(uint32_t)*CHAR_BIT)/HEX_BIT_COUNT)

void nfklInitBigIntWithHexCharBuf(NfklBigInt* b,const char* buf,size_t len)
{
	int neg=buf[0]=='-';
	size_t offset=(neg||buf[0]=='+')+2;
	for(;offset<len&&buf[offset]=='0';offset++);
	const char* start=buf+offset;
	int64_t digits_count=((len-offset)*HEX_BIT_COUNT+NFKL_BIGINT_DIGIT_SHIFT-1)/NFKL_BIGINT_DIGIT_SHIFT;
	ensure_bigint_size(b,digits_count);

	int bits_in_accum=0;
	NfklBigIntTwoDigit accum=0;
	NfklBigIntDigit* pdigits=b->digits;
	const char* p=buf+len;
	while(--p>=start)
	{
		char c=*p;
		int k=isdigit(c)
			?c-'0'
			:toupper(c)-'A'+10;
		accum|=(NfklBigIntTwoDigit)k<<bits_in_accum;
		bits_in_accum+=HEX_BIT_COUNT;
		if(bits_in_accum>=NFKL_BIGINT_DIGIT_SHIFT)
		{
			*pdigits++=(NfklBigIntDigit)(accum&NFKL_BIGINT_DIGIT_MASK);
			accum>>=NFKL_BIGINT_DIGIT_SHIFT;
			bits_in_accum-=NFKL_BIGINT_DIGIT_SHIFT;
		}
	}
	if(bits_in_accum)
		*pdigits++=(NfklBigIntDigit)accum;
	while(pdigits-b->digits<digits_count)
		*pdigits++=0;
	b->num=digits_count;
	bigint_normalize(b);
	if(neg)
		b->num=-b->num;
}

void nfklInitBigIntWithCharBuf(NfklBigInt* b,const char* buf,size_t len)
{
	if(fklIsHexInt(buf,len))
		nfklInitBigIntWithHexCharBuf(b,buf,len);
	else if(fklIsOctInt(buf,len))
		nfklInitBigIntWithOctCharBuf(b,buf,len);
	else
		nfklInitBigIntWithDecCharBuf(b,buf,len);
}

void nfklInitBigIntWithCstr(NfklBigInt* b,const char* cstr)
{
	nfklInitBigIntWithCharBuf(b,cstr,strlen(cstr));
}

void nfklUninitBigInt(NfklBigInt* b)
{
	free(b->digits);
	*b=NFKL_BIGINT_0;
}

NfklBigInt* nfklCreateBigInt0(void)
{
	NfklBigInt* b=(NfklBigInt*)malloc(sizeof(NfklBigInt));
	FKL_ASSERT(b);
	nfklInitBigInt0(b);
	return b;
}


NfklBigInt* nfklCreateBigInt1(void)
{
	NfklBigInt* b=nfklCreateBigInt0();
	nfklInitBigInt1(b);
	return b;
}

NfklBigInt* nfklCreateBigIntN1(void)
{
	NfklBigInt* b=nfklCreateBigInt0();
	nfklInitBigIntN1(b);
	return b;
}

NfklBigInt* nfklCreateBigIntI(int64_t v)
{
	NfklBigInt* b=nfklCreateBigInt0();
	nfklInitBigIntI(b,v);
	return b;
}

NfklBigInt* nfklCreateBigIntD(double v)
{
	return nfklCreateBigIntI((int64_t)v);
}

NfklBigInt* nfklCreateBigIntU(uint64_t v)
{
	NfklBigInt* b=nfklCreateBigInt0();
	nfklInitBigIntU(b,v);
	return b;
}

NfklBigInt* nfklCreateBigIntWithCharBuf(const char* buf,size_t len)
{
	NfklBigInt* b=nfklCreateBigInt0();
	nfklInitBigIntWithCharBuf(b,buf,len);
	return b;
}

NfklBigInt* nfklCreateBigIntWithDecCharBuf(const char* buf,size_t len)
{
	NfklBigInt* b=nfklCreateBigInt0();
	nfklInitBigIntWithDecCharBuf(b,buf,len);
	return b;
}

NfklBigInt* nfklCreateBigIntWithHexCharBuf(const char* buf,size_t len)
{
	NfklBigInt* b=nfklCreateBigInt0();
	nfklInitBigIntWithHexCharBuf(b,buf,len);
	return b;
}

NfklBigInt* nfklCreateBigIntWithOctCharBuf(const char* buf,size_t len)
{
	NfklBigInt* b=nfklCreateBigInt0();
	nfklInitBigIntWithOctCharBuf(b,buf,len);
	return b;
}

NfklBigInt* nfklCreateBigIntWithCstr(const char* cstr)
{
	NfklBigInt* b=nfklCreateBigInt0();
	nfklInitBigIntWithCstr(b,cstr);
	return b;
}

void nfklDestroyBigInt(NfklBigInt* b)
{
	nfklUninitBigInt(b);
	free(b);
}

NfklBigInt* nfklCopyBigInt(const NfklBigInt* b)
{
	NfklBigInt* r=nfklCreateBigInt0();
	nfklSetBigInt(r,b);
	return r;
}

int nfklBigIntEqual(const NfklBigInt* a,const NfklBigInt* b)
{
	if(a->num==b->num)
		return memcmp(a->digits,b->digits,labs(a->num)*sizeof(NfklBigIntDigit))==0;
	else
		return 0;
}

static inline int x_cmp(const NfklBigInt* a,const NfklBigInt* b)
{
	int64_t i=labs(a->num);
	while(--i>=0&&a->digits[i]==b->digits[i]);
	return i<0
		?0
		:(NfklBigIntSDigit)a->digits[i]-(NfklBigIntSDigit)b->digits[i];
}

int nfklBigIntCmp(const NfklBigInt* a,const NfklBigInt* b)
{
	int sign;
	if(a->num!=b->num)
		sign=a->num-b->num;
	else
	{
		sign=x_cmp(a,b);
		if(a->num<0)
			sign=-sign;
	}

	return sign<0
		?-1
		:sign>0
		?1:0;
}

int nfklBigIntAbsCmp(const NfklBigInt* a,const NfklBigInt* b)
{
	int64_t num_a=labs(a->num);
	int64_t num_b=labs(b->num);
	int sign=num_a!=num_b?num_a-num_b:x_cmp(a,b);

	return sign<0
		?-1
		:sign>0
		?1:0;
}

int nfklIsBigIntOdd(const NfklBigInt* b)
{
	return b->num!=0&&b->digits[0]%2;
}

int nfklIsBigIntEven(const NfklBigInt* b)
{
	return !nfklIsBigIntOdd(b);
}

int nfklIsBigIntLe0(const NfklBigInt* b)
{
	return b->num<=0;
}

int nfklIsBigIntLt0(const NfklBigInt* b)
{
	return b->num<0;
}

void nfklSetBigInt(NfklBigInt* to,const NfklBigInt* from)
{
	ensure_bigint_size(to,labs(from->num));
	to->num=from->num;
	memcpy(to->digits,from->digits,labs(from->num)*sizeof(NfklBigIntDigit));
}

void nfklSetBigIntU(NfklBigInt* to,uint64_t v)
{
	if(v)
	{
		size_t c=count_digits_size(v);
		to->num=c;
		ensure_bigint_size(to,c);
		set_uint64_to_digits(to->digits,c,v);
	}
	else
		to->num=0;
}

void nfklSetBigIntI(NfklBigInt* to,int64_t v)
{
	nfklSetBigIntU(to,labs(v));
	if(v<0)
		to->num=-to->num;
}

void nfklSetBigIntD(NfklBigInt* to,double from)
{
	nfklSetBigIntI(to,(int64_t)from);
}

static inline void x_add(NfklBigInt* a,const NfklBigInt* b)
{
	uint64_t num_a=labs(a->num);
	uint64_t num_b=labs(b->num);
	size_t i=0;
	NfklBigIntDigit carry=0;
	const NfklBigIntDigit* digits_a;
	const NfklBigIntDigit* digits_b;
	if(num_a<num_b)
	{
		uint64_t num_tmp=num_a;
		num_a=num_b;
		num_b=num_tmp;
		ensure_bigint_size(a,num_a+1);
		digits_a=a->digits;
		digits_b=b->digits;

		const NfklBigIntDigit* digits_tmp=digits_a;
		digits_a=digits_b;
		digits_b=digits_tmp;
	}
	else
	{
		ensure_bigint_size(a,num_a+1);
		digits_a=a->digits;
		digits_b=b->digits;
	}
	for(;i<num_b;++i)
	{
		carry+=digits_a[i]+digits_b[i];
		a->digits[i]=carry&NFKL_BIGINT_DIGIT_MASK;
		carry>>=NFKL_BIGINT_DIGIT_SHIFT;
	}
	for(;i<num_a;++i)
	{
		carry+=digits_a[i];
		a->digits[i]=carry;
		carry>>=NFKL_BIGINT_DIGIT_SHIFT;
	}

	a->num=num_a+1;
	a->digits[i]=carry;
	bigint_normalize(a);
}

static inline void x_sub(NfklBigInt* a,const NfklBigInt* b)
{
	int64_t num_a=labs(a->num);
	int64_t num_b=labs(b->num);
	int sign=1;
	ssize_t i=0;
	NfklBigIntDigit borrow=0;
	const NfklBigIntDigit* digits_a;
	const NfklBigIntDigit* digits_b;
	if(num_a<num_b)
	{
		sign=-1;
		uint64_t num_tmp=num_a;
		num_a=num_b;
		num_b=num_tmp;
		ensure_bigint_size(a,num_a+1);
		digits_a=a->digits;
		digits_b=b->digits;

		const NfklBigIntDigit* digits_tmp=digits_a;
		digits_a=digits_b;
		digits_b=digits_tmp;
	}
	else
	{
		ensure_bigint_size(a,num_a+1);
		digits_a=a->digits;
		digits_b=b->digits;
		if(num_a==num_b)
		{
			i=num_a;
			while(--i>=0&&a->digits[i]==b->digits[i]);
			if(i<0)
			{
				a->num=0;
				return;
			}
			if(a->digits[i]<b->digits[i])
			{
				sign=-1;
				const NfklBigIntDigit* digits_tmp=digits_a;
				digits_a=digits_b;
				digits_b=digits_tmp;
			}
			num_a=num_b=i+1;
		}
	}

	for(i=0;i<num_b;++i)
	{
		borrow=digits_a[i]-digits_b[i]-borrow;
		a->digits[i]=borrow&NFKL_BIGINT_DIGIT_MASK;
		borrow>>=NFKL_BIGINT_DIGIT_SHIFT;
		borrow&=1;
	}
	for(;i<num_a;++i)
	{
		borrow=digits_a[i]-borrow;
		a->digits[i]=borrow&NFKL_BIGINT_DIGIT_MASK;
		borrow>>=NFKL_BIGINT_DIGIT_SHIFT;
		borrow&=1;
	}
	FKL_ASSERT(borrow==0);
	if(sign<0)
		a->num=a->num<0?num_a:-num_a;
	bigint_normalize(a);
}

static inline void x_sub2(const NfklBigInt* a,NfklBigInt* b)
{
	int64_t num_a=labs(a->num);
	int64_t num_b=labs(b->num);
	int sign=1;
	ssize_t i=0;
	NfklBigIntDigit borrow=0;
	const NfklBigIntDigit* digits_a;
	const NfklBigIntDigit* digits_b;
	if(num_a<num_b)
	{
		sign=-1;
		uint64_t num_tmp=num_a;
		num_a=num_b;
		num_b=num_tmp;
		ensure_bigint_size(b,num_a+1);
		digits_a=a->digits;
		digits_b=b->digits;

		const NfklBigIntDigit* digits_tmp=digits_a;
		digits_a=digits_b;
		digits_b=digits_tmp;
	}
	else
	{
		ensure_bigint_size(b,num_a+1);
		digits_a=a->digits;
		digits_b=b->digits;
		if(num_a==num_b)
		{
			i=num_a;
			while(--i>=0&&a->digits[i]==b->digits[i]);
			if(i<0)
			{
				b->num=0;
				return;
			}
			if(a->digits[i]<b->digits[i])
			{
				sign=-1;
				const NfklBigIntDigit* digits_tmp=digits_a;
				digits_a=digits_b;
				digits_b=digits_tmp;
			}
			num_a=num_b=i+1;
		}
	}

	for(i=0;i<num_b;++i)
	{
		borrow=digits_a[i]-digits_b[i]-borrow;
		b->digits[i]=borrow&NFKL_BIGINT_DIGIT_MASK;
		borrow>>=NFKL_BIGINT_DIGIT_SHIFT;
		borrow&=1;
	}
	for(;i<num_a;++i)
	{
		borrow=digits_a[i]-borrow;
		b->digits[i]=borrow&NFKL_BIGINT_DIGIT_MASK;
		borrow>>=NFKL_BIGINT_DIGIT_SHIFT;
		borrow&=1;
	}
	FKL_ASSERT(borrow==0);
	b->num=num_a*sign;
	bigint_normalize(b);
}

#define MAX_INT64_DIGITS_COUNT (3)
#define MEDIUM_VALUE(I) ((int64_t)((I)->num==0?0:(I)->num<0?-((NfklBigIntSDigit)(I)->digits[0]):(NfklBigIntSDigit)(I)->digits[0]))

void nfklAddBigInt(NfklBigInt* a, const NfklBigInt* b)
{
	if(labs(a->num)<=1&&labs(b->num)<=1)
	{
		nfklSetBigIntI(a,MEDIUM_VALUE(a)+MEDIUM_VALUE(b));
		return;
	}
	if(a->num<0)
	{
		if(b->num<0)
		{
			x_add(a,b);
			a->num=-a->num;
		}
		else
			x_sub2(b,a);
	}
	else if(b->num<0)
		x_sub(a,b);
	else
		x_add(a,b);
}

void nfklAddBigIntI(NfklBigInt* b,int64_t v)
{
	NfklBigIntDigit digits[MAX_INT64_DIGITS_COUNT];
	NfklBigInt bi=NFKL_BIGINT_0;
	bi.size=MAX_INT64_DIGITS_COUNT;
	bi.digits=digits;
	nfklSetBigIntI(&bi,v);
	nfklAddBigInt(b,&bi);
}

void nfklSubBigInt(NfklBigInt* a,const NfklBigInt* b)
{
	if(labs(a->num)<=1&&labs(b->num)<=1)
	{
		nfklSetBigIntI(a,MEDIUM_VALUE(a)-MEDIUM_VALUE(b));
		return;
	}
	if(a->num<0)
	{
		if(b->num<0)
			x_sub(a,b);
		else
		{
			x_add(a,b);
			a->num=-a->num;
		}
	}
	else if(b->num<0)
		x_add(a,b);
	else
		x_sub(a,b);
}

void nfklSubBigIntI(NfklBigInt* a,int64_t v)
{
	NfklBigIntDigit digits[MAX_INT64_DIGITS_COUNT];
	NfklBigInt bi=NFKL_BIGINT_0;
	bi.size=MAX_INT64_DIGITS_COUNT;
	bi.digits=digits;
	nfklSetBigIntI(&bi,v);
	nfklSubBigInt(a,&bi);
}

#define SAME_SIGN(a,b) (((a)->num>=0&&(b)->num>=0)||((a)->num<0&&(b)->num<0))

// XXX: 这个乘法的实现不是很高效
void nfklMulBigInt(NfklBigInt* a,const NfklBigInt* b)
{
	if(NFKL_BIGINT_IS_0(a)||NFKL_BIGINT_IS_1(b))
		return;
	else if(NFKL_BIGINT_IS_0(b))
		a->num=0;
	else if(NFKL_BIGINT_IS_N1(b))
		a->num=-a->num;
	else if(NFKL_BIGINT_IS_1(a))
		nfklSetBigInt(a,b);
	else if(NFKL_BIGINT_IS_N1(a))
	{
		nfklSetBigInt(a,b);
		a->num=-a->num;
	}
	else
	{
		int64_t num_a=labs(a->num);
		int64_t num_b=labs(b->num);
		int64_t total=num_a+num_b;
		NfklBigIntDigit* result=(NfklBigIntDigit*)calloc(total,sizeof(NfklBigIntDigit));
		FKL_ASSERT(result);
		for(int64_t i=0;i<num_a;i++)
		{
			NfklBigIntTwoDigit na=a->digits[i];
			NfklBigIntTwoDigit carry=0;
			for(int64_t j=0;j<num_b;j++)
			{
				NfklBigIntTwoDigit nb=b->digits[j];
				carry+=result[i+j]+na*nb;
				result[i+j]=carry&NFKL_BIGINT_DIGIT_MASK;
				carry>>=NFKL_BIGINT_DIGIT_SHIFT;
			}
			result[i+num_b]+=carry;
		}
		free(a->digits);
		a->digits=result;
		a->size=total;
		a->num=SAME_SIGN(a,b)?total:-total;
		bigint_normalize(a);
	}

}

void nfklMulBigIntI(NfklBigInt* b,int64_t v)
{
	NfklBigIntDigit digits[MAX_INT64_DIGITS_COUNT];
	NfklBigInt bi=NFKL_BIGINT_0;
	bi.size=MAX_INT64_DIGITS_COUNT;
	bi.digits=digits;
	nfklSetBigIntI(&bi,v);
	nfklMulBigInt(b,&bi);
}

uintptr_t nfklBigIntHash(const NfklBigInt* bi)
{
	const int64_t len=labs(bi->num);
	uintptr_t r=(uintptr_t)bi->num;
	for(int64_t i=0;i<len;i++)
		r=fklHashCombine(r,bi->digits[i]);
	return r;
}

double nfklBigIntToD(const NfklBigInt* bi)
{
	double r=0;
	double base=1;
	const int64_t num=labs(bi->num);
	for(int64_t i=0;i<num;i++)
	{
		r+=((double)bi->digits[i])*base;
		base*=NFKL_BIGINT_DIGIT_BASE;
	}
	if(bi->num<0)
		r=-r;
	return r;
}

static const NfklBigIntDigit U64_MAX_DIGITS[3]=
{
	0xffffffff&NFKL_BIGINT_DIGIT_MASK,
	0xffffffff&NFKL_BIGINT_DIGIT_MASK,
	0x0000000f&NFKL_BIGINT_DIGIT_MASK,
};

static const NfklBigInt U64_MAX_BIGINT=
{
	.digits=(NfklBigIntDigit*)&U64_MAX_DIGITS[0],
	.num=3,
	.size=3,
};

static const NfklBigIntDigit I64_MIN_DIGITS[3]=
{
	0x0,
	0x0,
	0x8,
};

static const NfklBigInt I64_MIN_BIGINT=
{
	.digits=(NfklBigIntDigit*)&I64_MIN_DIGITS[0],
	.num=-3,
	.size=3,
};

static const NfklBigIntDigit I64_MAX_DIGITS[3]=
{
	0xffffffff&NFKL_BIGINT_DIGIT_MASK,
	0xffffffff&NFKL_BIGINT_DIGIT_MASK,
	0x00000007&NFKL_BIGINT_DIGIT_MASK,
};

static const NfklBigInt I64_MAX_BIGINT=
{
	.digits=(NfklBigIntDigit*)&I64_MAX_DIGITS[0],
	.num=3,
	.size=3,
};

uint64_t nfklBigIntToU(const NfklBigInt* bi)
{
	if(bi->num<=0)
		return 0;
	if(bi->num>3)
		return UINT64_MAX;
	else if(bi->num>2&&nfklBigIntCmp(bi,&U64_MAX_BIGINT)>0)
		return UINT64_MAX;
	else
	{
		uint64_t r=0;
		for(int64_t i=0;i<bi->num;i++)
			r|=((uint64_t)bi->digits[i])<<(NFKL_BIGINT_DIGIT_SHIFT*i);
		return r;
	}
}

int64_t nfklBigIntToI(const NfklBigInt* bi)
{
	if(bi->num<-3)
		return INT64_MIN;
	else if(bi->num<-2&&nfklBigIntCmp(bi,&I64_MIN_BIGINT)<0)
		return INT64_MIN;
	else if(bi->num==0)
		return 0;
	else if(bi->num>3)
		return INT64_MAX;
	else if(bi->num>2&&nfklBigIntCmp(bi,&I64_MAX_BIGINT)>0)
		return INT64_MAX;
	else
	{
		int64_t r=0;
		const int64_t num=labs(bi->num);
		for(int64_t i=0;i<num;i++)
			r|=((uint64_t)bi->digits[i])<<(NFKL_BIGINT_DIGIT_SHIFT*i);
		if(bi->num<0)
			r=-r;
		return r;
	}
}

static inline size_t bigint_to_dec_string_buffer(const NfklBigInt* a
		,NfklBigIntToStrAllocCb alloc_cb
		,void* ctx)
{
	FKL_ASSERT(a->num);
	int64_t size_a=labs(a->num);
	int64_t d=(33*NFKL_BIGINT_DECIMAL_SHIFT)/(10*NFKL_BIGINT_DIGIT_SHIFT-33*NFKL_BIGINT_DECIMAL_SHIFT);
	int64_t size=1+size_a+size_a/d;
	const NfklBigIntDigit* pin=a->digits;
	NfklBigIntDigit* pout=(NfklBigIntDigit*)malloc(size*sizeof(NfklBigIntDigit));
	FKL_ASSERT(pout);
	size=0;
	for(int64_t i=size_a;--i>=0;)
	{
		NfklBigIntDigit hi=pin[i];
		for(int64_t j=0;j<size;j++)
		{
			NfklBigIntTwoDigit z=(NfklBigIntTwoDigit)pout[j]<<NFKL_BIGINT_DIGIT_SHIFT|hi;
			hi=(NfklBigIntDigit)(z/NFKL_BIGINT_DECIMAL_BASE);
			pout[j]=(NfklBigIntDigit)(z-(NfklBigIntTwoDigit)hi*NFKL_BIGINT_DECIMAL_BASE);
		}
		while(hi)
		{
			pout[size++]=hi%NFKL_BIGINT_DECIMAL_BASE;
			hi/=NFKL_BIGINT_DECIMAL_BASE;
		}
	}
	if(size==0)
		pout[size++]=0;
	size_t neg=a->num<0;
	size_t len=neg+1+(size-1)*NFKL_BIGINT_DECIMAL_SHIFT;
	NfklBigIntDigit tenpow=10;
	NfklBigIntDigit rem=pout[size-1];
	while(rem>=tenpow)
	{
		tenpow*=10;
		len++;
	}
	char* p_str=alloc_cb(ctx,len)+len;
	int64_t i;
	for(i=0;i<size-1;i++)
	{
		rem=pout[i];
		for(int64_t j=0;j<NFKL_BIGINT_DECIMAL_SHIFT;j++)
		{
			*--p_str='0'+rem%10;
			rem/=10;
		}
	}
	rem=pout[i];
	do
	{
		*--p_str='0'+rem%10;
		rem/=10;
	}while(rem!=0);
	if(neg)
		*--p_str='-';
	free(pout);
	return len;
}

static inline uint8_t bit_length_digit(uint64_t x)
{
    static const int BIT_LENGTH_TABLE[32]=
	{
        0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    };
    int msb=0;
    while (x>=32) {
        msb+=6;
        x>>=6;
    }
    msb+=BIT_LENGTH_TABLE[x];
    return msb;
}

static inline size_t bigint_to_bin_string_buffer(const NfklBigInt* a
		,NfklBigIntToStrAllocCb alloc_cb
		,void* ctx
		,uint8_t base
		,NfklBigIntFmtFlags flags
		,const char* alt_str)
{
	FKL_ASSERT(a->num);
	int bits=0;
	int alternate=(flags&NFKL_BIGINT_FMT_FLAG_ALTERNATE)!=0;
	int capitals=(flags&NFKL_BIGINT_FMT_FLAG_CAPITALS)!=0;
	switch(base)
	{
		case 16:
			bits=HEX_BIT_COUNT;
			break;
		case 8:
			bits=OCT_BIT_COUNT;
			break;
		default:
			abort();
	}
	int64_t size_a=labs(a->num);
	int neg=a->num<0;
	int64_t size_a_in_bits=(size_a-1)*NFKL_BIGINT_DIGIT_SHIFT+bit_length_digit(a->digits[size_a-1]);
	int64_t sz=neg+(size_a_in_bits+(bits-1))/bits;
	size_t alt_str_len=strlen(alt_str);
	if(alternate)
		sz+=alt_str_len;
	char* p_str=alloc_cb(ctx,sz)+sz;
	NfklBigIntTwoDigit accum=0;
	int accumbits=0;
	int64_t i;
	for(i=0;i<size_a;++i)
	{
		accum|=(NfklBigIntTwoDigit)a->digits[i]<<accumbits;
		accumbits+=NFKL_BIGINT_DIGIT_SHIFT;
		FKL_ASSERT(accumbits>=bits);
		do
		{
			char cdigit;
			cdigit=(char)(accum&(base-1));
			cdigit+=(cdigit<10)?'0':(capitals?'A':'a')-10;
			*--p_str=cdigit;
			accumbits-=bits;
			accum>>=bits;
		}while(i<size_a-1?accumbits>=bits:accum>0);
	}
	if(alternate)
	{
		memcpy(p_str-alt_str_len,alt_str,alt_str_len);
		p_str-=alt_str_len;
	}
	if(neg)
		*--p_str='-';
	return sz;
}

size_t nfklBigIntToStr(const NfklBigInt *a
		,NfklBigIntToStrAllocCb alloc_cb
		,void* ctx
		,uint8_t radix
		,NfklBigIntFmtFlags flags)
{
	if(NFKL_BIGINT_IS_0(a))
	{
		char* ptr=alloc_cb(ctx,1);
		ptr[0]='0';
		return 1;
	}
	else if(radix==10)
		return bigint_to_dec_string_buffer(a,alloc_cb,ctx);
	else if(radix==8)
		return bigint_to_bin_string_buffer(a,alloc_cb,ctx,8,flags,"0");
	else if(radix==16)
		return bigint_to_bin_string_buffer(a,alloc_cb,ctx,16,flags,flags&NFKL_BIGINT_FMT_FLAG_CAPITALS?"0X":"0x");
	else
		abort();
}

static char* string_buffer_alloc(void* ptr,size_t len)
{
	FklStringBuffer* buf=ptr;
	fklStringBufferReverse(buf,len+1);
	buf->index=len;
	char* body=fklStringBufferBody(buf);
	body[len]='\0';
	return body;
}

size_t nfklBigIntToStringBuffer(const NfklBigInt* a
		,FklStringBuffer* buf
		,uint8_t radix
		,NfklBigIntFmtFlags flags)
{
	return nfklBigIntToStr(a,string_buffer_alloc,buf,radix,flags);
}
