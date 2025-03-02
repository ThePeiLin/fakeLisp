#include<fakeLisp/bigint.h>
#include<fakeLisp/utils.h>

#include<string.h>

#define NFKL_BIGINT_DIGIT_SHIFT (30)
#define NFKL_BIGINT_DIGIT_BASE ((NfklBigIntDigit)1<<NFKL_BIGINT_DIGIT_SHIFT)
#define NFKL_BIGINT_DIGIT_MASK ((NfklBigIntDigit)(NFKL_BIGINT_DIGIT_BASE-1))

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

void nfklDestroyBigInt(NfklBigInt* b)
{
	nfklUninitBigInt(b);
	free(b);
}

int nfklBigIntEqual(const NfklBigInt* a,const NfklBigInt* b)
{
	if(a->num==b->num)
		return memcmp(a->digits,b->digits,labs(a->num)*sizeof(NfklBigIntDigit))==0;
	else
		return 0;
}

int nfklBigIntCmp(const NfklBigInt* a,const NfklBigInt* b)
{
	int sign;
	if(a->num!=b->num)
		sign=a->num-b->num;
	else
	{
		int64_t i=labs(a->num);
		while(--i>=0&&a->digits[i]==b->digits[i]);
		if(i<0)
			sign=0;
		else
		{
			sign=(NfklBigIntSDigit)a->digits[i]-(NfklBigIntSDigit)b->digits[i];
			if(a->num<0)
				sign=-sign;
		}
	}

	return sign<0
		?-1
		:sign>0
		?1:0;
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

static inline void normalize(NfklBigInt* a)
{
	int64_t i=labs(a->num);
	for(;i>0&&a->digits[i-1]==0;--i);
	a->num=a->num<0?-i:i;
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
	normalize(a);
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
	normalize(a);
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
	normalize(b);
}

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
			x_add(a,b);
	}
	else if(b->num<0)
		x_add(a,b);
	else
		x_sub(a,b);
}

#define SAME_SIGN(a,b) (((a)->num>=0&&(b)->num>=0)||((a)->num<0&&(b)->num<0))

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
			uint64_t na=a->digits[i];
			uint64_t carry=0;
			for(int64_t j=0;j<num_b;j++)
			{
				uint64_t nb=b->digits[j];
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
		normalize(a);
	}

}

