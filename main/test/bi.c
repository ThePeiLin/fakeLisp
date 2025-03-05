#include<fakeLisp/bigint.h>
#include<assert.h>
#include<string.h>
#include<math.h>
#include<stdio.h>

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
	{
		NfklBigInt a;
		NfklBigInt b;
		NfklBigInt c;
		NfklBigInt d;
		NfklBigInt e;
		NfklBigInt f;
		nfklInitBigIntWithDecCharBuf(&a,"114514",strlen("114514"));
		nfklInitBigIntWithHexCharBuf(&c,"0x00000000114514abcd",strlen("0x00000000114514abcd"));
		nfklInitBigIntWithOctCharBuf(&e,"011451477665544",strlen("011451477665544"));
		nfklInitBigIntI(&b,114514);
		nfklInitBigIntI(&d,0x114514abcd);
		nfklInitBigIntI(&f,011451477665544);

		assert(nfklBigIntEqual(&a,&b));
		assert(nfklBigIntEqual(&c,&d));
		assert(nfklBigIntEqual(&e,&f));

		nfklUninitBigInt(&a);
		nfklUninitBigInt(&b);
		nfklUninitBigInt(&c);
		nfklUninitBigInt(&d);
		nfklUninitBigInt(&e);
		nfklUninitBigInt(&f);
	}
	{
		NfklBigInt a;
		NfklBigInt b;
		NfklBigInt c;
		NfklBigInt d;
		NfklBigInt e;
		NfklBigInt f;
		nfklInitBigIntWithCstr(&a,"114514");
		nfklInitBigIntWithCstr(&c,"0x00000000114514abcd");
		nfklInitBigIntWithCstr(&e,"011451477665544");
		nfklInitBigIntI(&b,114514);
		nfklInitBigIntI(&d,0x114514abcd);
		nfklInitBigIntI(&f,011451477665544);

		assert(nfklBigIntEqual(&a,&b));
		assert(nfklBigIntEqual(&c,&d));
		assert(nfklBigIntEqual(&e,&f));

		nfklUninitBigInt(&a);
		nfklUninitBigInt(&b);
		nfklUninitBigInt(&c);
		nfklUninitBigInt(&d);
		nfklUninitBigInt(&e);
		nfklUninitBigInt(&f);
	}
	{
		NfklBigInt c;
		NfklBigInt d;
		nfklInitBigIntWithHexCharBuf(&c,"-0x00000000114514abcd",strlen("-0x00000000114514abcd"));
		nfklInitBigIntI(&d,-0x114514abcd);

		assert(nfklBigIntEqual(&c,&d));

		nfklUninitBigInt(&c);
		nfklUninitBigInt(&d);
	}
	{
		NfklBigInt c;
		{
			nfklInitBigIntWithHexCharBuf(&c,"-0x00000000",strlen("-0x00000000"));
			assert(NFKL_BIGINT_IS_0(&c));
			nfklUninitBigInt(&c);
		}
		{
			nfklInitBigIntWithCharBuf(&c,"00000000",strlen("00000000"));
			assert(NFKL_BIGINT_IS_0(&c));
			nfklUninitBigInt(&c);
		}
	}
	{
		NfklBigInt c;
		nfklInitBigIntI(&c,1145141919);

		int64_t i=nfklBigIntToI(&c);
		uint64_t u=nfklBigIntToU(&c);
		double d=nfklBigIntToD(&c);
		fprintf(stderr,"i: %ld, u: %lu, d: %lf\n",i,u,d);
		assert(i==1145141919);
		assert(u==1145141919);
		assert(!islessgreater(d,1145141919.0));

		nfklSetBigIntI(&c,-1145141919);
		i=nfklBigIntToI(&c);
		d=nfklBigIntToD(&c);
		fprintf(stderr,"i: %ld, d: %lf\n",i,d);
		assert(i==-1145141919);
		assert(!islessgreater(d,-1145141919.0));

		nfklSetBigIntI(&c,INT64_MAX);
		i=nfklBigIntToI(&c);
		u=nfklBigIntToU(&c);
		fprintf(stderr,"i: %ld, u: %lu\n",i,u);
		assert(i==INT64_MAX);
		assert(u==INT64_MAX);

		nfklSetBigIntI(&c,INT64_MIN);
		i=nfklBigIntToI(&c);
		fprintf(stderr,"i: %ld\n",i);
		assert(i==INT64_MIN);

		nfklSubBigIntI(&c,1);
		i=nfklBigIntToI(&c);
		fprintf(stderr,"i: %ld\n",i);
		assert(i==INT64_MIN);

		nfklSetBigIntI(&c,INT64_MAX);
		nfklAddBigIntI(&c,1);
		i=nfklBigIntToI(&c);
		fprintf(stderr,"i: %ld\n",i);
		assert(i==INT64_MAX);

		nfklSetBigIntU(&c,UINT64_MAX);
		nfklAddBigIntI(&c,1);
		u=nfklBigIntToU(&c);
		fprintf(stderr,"u: %lu\n",u);
		assert(u==UINT64_MAX);

		nfklUninitBigInt(&c);
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
		assert(nfklBigIntAbsCmp(&a,&b)<0);

		assert(nfklBigIntCmp(&b,&a)>0);
		assert(nfklBigIntAbsCmp(&b,&a)>0);

		assert(nfklBigIntCmp(&c,&d)==0);
		assert(nfklBigIntAbsCmp(&c,&d)==0);

		nfklSubBigInt(&d,&b);

		assert(nfklBigIntEqual(&a,&d));
		assert(nfklBigIntCmp(&a,&d)==0);
		assert(nfklBigIntAbsCmp(&a,&d)==0);

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

		assert(nfklBigIntAbsCmp(a,b)<0);
		assert(nfklBigIntAbsCmp(b,a)>0);

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
