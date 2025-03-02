#include<fakeLisp/bigint.h>
#include<assert.h>

#define A_BIG_NUM ((int64_t)4294967295)

static void sub_test0(void)
{
	{
		NfklBigInt a;
		NfklBigInt b;
		NfklBigInt c;

		nfklInitBigIntI(&a,-114514);
		nfklInitBigIntD(&b,-114514.1919810);
		assert(nfklBigIntEqual(&a,&b));
		nfklInitBigIntU(&c,1919810);
		nfklSetBigIntI(&a,1919810);
		assert(nfklBigIntEqual(&a,&c));

		nfklUninitBigInt(&a);
		nfklUninitBigInt(&b);
		nfklUninitBigInt(&c);
	}
	{
		NfklBigInt* a=nfklCreateBigIntI(-114514);
		NfklBigInt* b=nfklCreateBigIntD(-114514.1919810);
		NfklBigInt* c=nfklCreateBigIntU(1919810);

		assert(nfklBigIntEqual(a,b));
		nfklSetBigIntI(a,1919810);
		assert(nfklBigIntEqual(a,c));

		nfklDestroyBigInt(a);
		nfklDestroyBigInt(b);
		nfklDestroyBigInt(c);
	}
}

static void sub_test1(void)
{
	{
		NfklBigInt a;
		NfklBigInt b;
		NfklBigInt c;
		NfklBigInt d=NFKL_BIGINT_0;
		nfklInitBigIntI(&a,114514);
		nfklInitBigIntI(&b,1919810);
		nfklInitBigIntI(&c,114514+1919810);

		nfklAddBigInt(&d,&a);
		nfklAddBigInt(&d,&b);

		assert(nfklBigIntEqual(&c,&d));

		assert(nfklBigIntCmp(&a,&b)<0);

		assert(nfklBigIntCmp(&b,&a)>0);

		assert(nfklBigIntCmp(&c,&d)==0);

		nfklSubBigInt(&d,&b);

		assert(nfklBigIntEqual(&a,&d));
		assert(nfklBigIntCmp(&a,&d)==0);

		nfklUninitBigInt(&a);
		nfklUninitBigInt(&b);
		nfklUninitBigInt(&c);
		nfklUninitBigInt(&d);
	}
	{
		NfklBigInt* a=nfklCreateBigIntI(114514+A_BIG_NUM);
		NfklBigInt* b=nfklCreateBigIntI(1919810+A_BIG_NUM);
		NfklBigInt* c=nfklCreateBigIntI(114514+1919810+A_BIG_NUM+A_BIG_NUM);
		NfklBigInt* d=nfklCreateBigInt0();

		nfklAddBigInt(d,a);
		nfklAddBigInt(d,b);

		assert(nfklBigIntEqual(c,d));
		assert(nfklBigIntCmp(a,b)<0);
		assert(nfklBigIntCmp(b,a)>0);
		assert(nfklBigIntCmp(c,d)==0);

		nfklSubBigInt(d,b);

		assert(nfklBigIntEqual(a,d));
		assert(nfklBigIntCmp(a,d)==0);

		nfklDestroyBigInt(a);
		nfklDestroyBigInt(b);
		nfklDestroyBigInt(c);
		nfklDestroyBigInt(d);
	}
}

static void sub_test2(void)
{
	{
		NfklBigInt a;
		NfklBigInt b;
		NfklBigInt c;
		nfklInitBigInt0(&a);
		nfklInitBigInt1(&b);
		nfklInitBigIntN1(&c);

		assert(NFKL_BIGINT_IS_0(&a));
		assert(NFKL_BIGINT_IS_1(&b));
		assert(NFKL_BIGINT_IS_N1(&c));
		assert(NFKL_BIGINT_IS_ABS1(&c)&&NFKL_BIGINT_IS_ABS1(&b));

		nfklUninitBigInt(&a);
		nfklUninitBigInt(&b);
		nfklUninitBigInt(&c);
	}
	{
		NfklBigInt* a=nfklCreateBigIntI(114514+A_BIG_NUM);
		NfklBigInt* b=nfklCreateBigIntI(-1919810-A_BIG_NUM);
		NfklBigInt* c=nfklCreateBigIntI(114514-1919810+A_BIG_NUM-A_BIG_NUM);
		NfklBigInt* d=nfklCreateBigInt0();

		nfklAddBigInt(d,a);
		nfklAddBigInt(d,b);

		assert(nfklBigIntEqual(c,d));
		assert(nfklBigIntCmp(a,b)>0);
		assert(nfklBigIntCmp(b,a)<0);
		assert(nfklBigIntCmp(c,d)==0);

		nfklSubBigInt(d,b);

		assert(nfklBigIntEqual(a,d));
		assert(nfklBigIntCmp(a,d)==0);

		nfklDestroyBigInt(a);
		nfklDestroyBigInt(b);
		nfklDestroyBigInt(c);
		nfklDestroyBigInt(d);
	}
	{
		NfklBigInt* a=nfklCreateBigIntI(-114514-A_BIG_NUM);
		NfklBigInt* b=nfklCreateBigIntI(-1919810-A_BIG_NUM);
		NfklBigInt* c=nfklCreateBigIntI(-114514-1919810-A_BIG_NUM-A_BIG_NUM);
		NfklBigInt* d=nfklCreateBigInt0();

		nfklAddBigInt(d,a);
		nfklAddBigInt(d,b);

		assert(nfklBigIntEqual(c,d));
		assert(nfklBigIntCmp(a,b)>0);
		assert(nfklBigIntCmp(b,a)<0);
		assert(nfklBigIntCmp(c,d)==0);

		nfklSubBigInt(d,b);

		assert(nfklBigIntEqual(a,d));
		assert(nfklBigIntCmp(a,d)==0);

		nfklDestroyBigInt(a);
		nfklDestroyBigInt(b);
		nfklDestroyBigInt(c);
		nfklDestroyBigInt(d);
	}
	{
		NfklBigInt* a=nfklCreateBigIntI(-114514-A_BIG_NUM);
		NfklBigInt* b=nfklCreateBigIntI(1919810+A_BIG_NUM);
		NfklBigInt* c=nfklCreateBigIntI(-114514+1919810-A_BIG_NUM+A_BIG_NUM);
		NfklBigInt* d=nfklCreateBigInt0();

		nfklAddBigInt(d,a);
		nfklAddBigInt(d,b);

		assert(nfklBigIntEqual(c,d));
		assert(nfklBigIntCmp(a,b)<0);
		assert(nfklBigIntCmp(b,a)>0);
		assert(nfklBigIntCmp(c,d)==0);

		nfklSubBigInt(d,b);

		assert(nfklBigIntEqual(a,d));
		assert(nfklBigIntCmp(a,d)==0);

		nfklDestroyBigInt(a);
		nfklDestroyBigInt(b);
		nfklDestroyBigInt(c);
		nfklDestroyBigInt(d);
	}
}

static void sub_test3(void)
{
	{
		NfklBigInt* a=nfklCreateBigIntI(114514);
		NfklBigInt* b=nfklCreateBigIntI(1919810+A_BIG_NUM);
		NfklBigInt* c=nfklCreateBigIntI(114514*(1919810+A_BIG_NUM));
		NfklBigInt* d=nfklCreateBigInt1();

		nfklMulBigInt(d,a);
		nfklMulBigInt(d,b);

		assert(nfklBigIntEqual(c,d));

		nfklDestroyBigInt(a);
		nfklDestroyBigInt(b);
		nfklDestroyBigInt(c);
		nfklDestroyBigInt(d);
	}
	{
		NfklBigInt* a=nfklCreateBigIntI(-114514);
		NfklBigInt* b=nfklCreateBigIntI(1919810+A_BIG_NUM);
		NfklBigInt* c=nfklCreateBigIntI(-114514*(1919810+A_BIG_NUM));
		NfklBigInt* d=nfklCreateBigInt1();

		nfklMulBigInt(d,a);
		nfklMulBigInt(d,b);

		assert(nfklBigIntEqual(c,d));

		nfklDestroyBigInt(a);
		nfklDestroyBigInt(b);
		nfklDestroyBigInt(c);
		nfklDestroyBigInt(d);
	}
}

int main()
{
	sub_test0();
	sub_test1();
	sub_test2();
	sub_test3();
	return 0;
}
