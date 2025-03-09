#include "fakeLisp/base.h"
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
	{
		NfklBigInt a;
		NfklBigInt b;
		NfklBigInt c;
		nfklInitBigInt0(&a);
		nfklInitBigIntI(&b,114514);
		nfklInitBigIntI(&c,1919);

		assert(nfklIsBigIntEven(&a));
		assert(nfklIsBigIntEven(&b));
		assert(nfklIsBigIntOdd(&c));

		assert(!nfklIsBigIntOdd(&a));
		assert(!nfklIsBigIntOdd(&b));
		assert(!nfklIsBigIntEven(&c));

		nfklSetBigIntI(&b,-1);
		assert(nfklIsBigIntLe0(&a));
		assert(nfklIsBigIntLe0(&b));
		assert(nfklIsBigIntLt0(&b));
		assert(!nfklIsBigIntLt0(&a));

		nfklUninitBigInt(&a);
		nfklUninitBigInt(&b);
		nfklUninitBigInt(&c);
	}
	{
		FklStringBuffer buf;
		NfklBigInt a;
		FklString* str=NULL;
		nfklInitBigIntI(&a,1145141919);
		fklInitStringBuffer(&buf);

		// pos
		nfklBigIntToStringBuffer(&a,&buf,10,NFKL_BIGINT_FMT_FLAG_NONE);
		str=nfklBigIntToString(&a,10,NFKL_BIGINT_FMT_FLAG_NONE);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"1145141919"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"1145141919"));

		// neg
		free(str);
		fklUninitStringBuffer(&buf);

		nfklSetBigIntI(&a,-1145141919);
		nfklBigIntToStringBuffer(&a,&buf,10,NFKL_BIGINT_FMT_FLAG_NONE);
		str=nfklBigIntToString(&a,10,NFKL_BIGINT_FMT_FLAG_NONE);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"-1145141919"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"-1145141919"));

		nfklUninitBigInt(&a);
		fklUninitStringBuffer(&buf);
		free(str);
	}
	{
		// oct
		FklStringBuffer buf;
		NfklBigInt a;
		FklString* str=NULL;
		nfklInitBigIntI(&a,1145141919);
		fklInitStringBuffer(&buf);

		nfklSetBigIntI(&a,1145141919);
		nfklBigIntToStringBuffer(&a,&buf,8,NFKL_BIGINT_FMT_FLAG_NONE);
		str=nfklBigIntToString(&a,8,NFKL_BIGINT_FMT_FLAG_NONE);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"10420275237"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"10420275237"));

		fklUninitStringBuffer(&buf);
		free(str);

		// oct alternate

		nfklBigIntToStringBuffer(&a,&buf,8,NFKL_BIGINT_FMT_FLAG_ALTERNATE);
		str=nfklBigIntToString(&a,8,NFKL_BIGINT_FMT_FLAG_ALTERNATE);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"010420275237"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"010420275237"));
		fklUninitStringBuffer(&buf);
		free(str);

		// oct neg

		nfklSetBigIntI(&a,-1145141919);
		nfklBigIntToStringBuffer(&a,&buf,8,NFKL_BIGINT_FMT_FLAG_NONE);
		str=nfklBigIntToString(&a,8,NFKL_BIGINT_FMT_FLAG_NONE);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"-10420275237"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"-10420275237"));
		fklUninitStringBuffer(&buf);
		free(str);

		// oct neg alternate

		nfklBigIntToStringBuffer(&a,&buf,8,NFKL_BIGINT_FMT_FLAG_ALTERNATE);
		str=nfklBigIntToString(&a,8,NFKL_BIGINT_FMT_FLAG_ALTERNATE);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"-010420275237"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"-010420275237"));
		fklUninitStringBuffer(&buf);
		free(str);
		nfklUninitBigInt(&a);
	}
	{
		FklStringBuffer buf;
		NfklBigInt a;
		FklString* str=NULL;
		nfklSetBigIntI(&a,1145141919);

		// hex

		nfklBigIntToStringBuffer(&a,&buf,16,NFKL_BIGINT_FMT_FLAG_NONE);
		str=nfklBigIntToString(&a,16,NFKL_BIGINT_FMT_FLAG_NONE);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"44417a9f"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"44417a9f"));
		fklUninitStringBuffer(&buf);
		free(str);

		// hex alternate

		nfklBigIntToStringBuffer(&a,&buf,16,NFKL_BIGINT_FMT_FLAG_ALTERNATE);
		str=nfklBigIntToString(&a,16,NFKL_BIGINT_FMT_FLAG_ALTERNATE);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"0x44417a9f"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"0x44417a9f"));
		fklUninitStringBuffer(&buf);
		free(str);

		// hex capitals

		nfklBigIntToStringBuffer(&a,&buf,16,NFKL_BIGINT_FMT_FLAG_CAPITALS);
		str=nfklBigIntToString(&a,16,NFKL_BIGINT_FMT_FLAG_CAPITALS);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"44417A9F"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"44417A9F"));
		fklUninitStringBuffer(&buf);
		free(str);

		// hex alternate capitals

		nfklBigIntToStringBuffer(&a,&buf,16,NFKL_BIGINT_FMT_FLAG_ALTERNATE|NFKL_BIGINT_FMT_FLAG_CAPITALS);
		str=nfklBigIntToString(&a,16,NFKL_BIGINT_FMT_FLAG_ALTERNATE|NFKL_BIGINT_FMT_FLAG_CAPITALS);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"0X44417A9F"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"0X44417A9F"));
		fklUninitStringBuffer(&buf);
		free(str);

		// hex neg
		nfklSetBigIntI(&a,-1145141919);

		nfklBigIntToStringBuffer(&a,&buf,16,NFKL_BIGINT_FMT_FLAG_NONE);
		str=nfklBigIntToString(&a,16,NFKL_BIGINT_FMT_FLAG_NONE);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"-44417a9f"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"-44417a9f"));
		fklUninitStringBuffer(&buf);
		free(str);

		// hex neg alternate
		nfklBigIntToStringBuffer(&a,&buf,16,NFKL_BIGINT_FMT_FLAG_ALTERNATE);
		str=nfklBigIntToString(&a,16,NFKL_BIGINT_FMT_FLAG_ALTERNATE);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"-0x44417a9f"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"-0x44417a9f"));
		fklUninitStringBuffer(&buf);
		free(str);

		// hex neg capitals
		nfklBigIntToStringBuffer(&a,&buf,16,NFKL_BIGINT_FMT_FLAG_CAPITALS);
		str=nfklBigIntToString(&a,16,NFKL_BIGINT_FMT_FLAG_CAPITALS);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"-44417A9F"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"-44417A9F"));
		fklUninitStringBuffer(&buf);
		free(str);

		// hex neg alternate capitals
		nfklBigIntToStringBuffer(&a,&buf,16,NFKL_BIGINT_FMT_FLAG_CAPITALS|NFKL_BIGINT_FMT_FLAG_ALTERNATE);
		str=nfklBigIntToString(&a,16,NFKL_BIGINT_FMT_FLAG_CAPITALS|NFKL_BIGINT_FMT_FLAG_ALTERNATE);

		fprintf(stderr,"buf: %s, len: %u\n",fklStringBufferBody(&buf),fklStringBufferLen(&buf));
		assert(!strcmp(fklStringBufferBody(&buf),"-0X44417A9F"));
		fprintf(stderr,"str: %s, len: %lu\n",str->str,str->size);
		assert(!strcmp(str->str,"-0X44417A9F"));
		fklUninitStringBuffer(&buf);
		free(str);

		nfklUninitBigInt(&a);
	}
}

static void sub_test4(void)
{
	const int64_t h=114514+A_BIG_NUM;
	const int64_t i=1919810;
	{
		NfklBigInt a;
		NfklBigInt rem=NFKL_BIGINT_0;
		FklStringBuffer buf;
		fklInitStringBuffer(&buf);
		nfklInitBigIntI(&a,h);
		nfklMulBigIntI(&a,i);

		nfklDivRemBigIntI(&a,i,&rem);

		int64_t d=nfklBigIntToI(&a);
		int64_t r=nfklBigIntToI(&rem);
		fprintf(stderr,"%17ld / %-17ld = %17ld ...... %-17ld\n",h*i,i,d,r);
		assert(d==114514+A_BIG_NUM);
		assert(r==0);

		fklUninitStringBuffer(&buf);
		nfklUninitBigInt(&a);
		nfklUninitBigInt(&rem);
	}
	{
		NfklBigInt a;
		NfklBigInt rem=NFKL_BIGINT_0;
		FklStringBuffer buf;
		fklInitStringBuffer(&buf);
		nfklInitBigIntI(&a,h);
		nfklMulBigIntI(&a,-i);

		nfklDivRemBigIntI(&a,i,&rem);

		int64_t d=nfklBigIntToI(&a);
		int64_t r=nfklBigIntToI(&rem);
		fprintf(stderr,"%17ld / %-17ld = %17ld ...... %-17ld\n",h*-i,i,d,r);
		assert(d==-114514-A_BIG_NUM);
		assert(r==0);

		fklUninitStringBuffer(&buf);
		nfklUninitBigInt(&a);
	}
	{
		int64_t d;
		int64_t r;
		NfklBigInt a;
		NfklBigInt rem;
		FklStringBuffer buf;
		const int64_t h=114514;
		const int64_t i=19198;
		fklInitStringBuffer(&buf);
		nfklInitBigIntI(&a,-h);
		nfklInitBigInt0(&rem);

		nfklDivRemBigIntI(&a,-i,&rem);

		d=nfklBigIntToI(&a);
		r=nfklBigIntToI(&rem);
		fprintf(stderr,"%17ld / %-17ld = %17ld ...... %-17ld\n",-h,-i,d,r);
		assert(d==(-h/-i));
		assert(r==(-h%-i));

		nfklSetBigIntI(&a,h);
		nfklDivRemBigIntI(&a,i,&rem);
		d=nfklBigIntToI(&a);
		r=nfklBigIntToI(&rem);
		fprintf(stderr,"%17ld / %-17ld = %17ld ...... %-17ld\n",h,i,d,r);
		assert(d==(h/i));
		assert(r==(h%i));

		nfklSetBigIntI(&a,-h);
		nfklDivRemBigIntI(&a,i,&rem);
		d=nfklBigIntToI(&a);
		r=nfklBigIntToI(&rem);
		fprintf(stderr,"%17ld / %-17ld = %17ld ...... %-17ld\n",-h,i,d,r);
		assert(d==(-h/i));
		assert(r==(-h%i));

		nfklSetBigIntI(&a,h);
		nfklDivRemBigIntI(&a,-i,&rem);
		d=nfklBigIntToI(&a);
		r=nfklBigIntToI(&rem);
		fprintf(stderr,"%17ld / %-17ld = %17ld ...... %-17ld\n",h,-i,d,r);
		assert(d==(h/-i));
		assert(r==(h%-i));

		fklUninitStringBuffer(&buf);
		nfklUninitBigInt(&a);
		nfklUninitBigInt(&rem);
	}
	{
		int64_t d;
		int64_t r;
		NfklBigInt a;
		NfklBigInt rem;
		FklStringBuffer buf;
		const int64_t h=114514+A_BIG_NUM*2;
		const int64_t i=19198+A_BIG_NUM;
		fklInitStringBuffer(&buf);
		nfklInitBigIntI(&a,-h);
		nfklInitBigInt0(&rem);

		nfklDivRemBigIntI(&a,-i,&rem);

		d=nfklBigIntToI(&a);
		r=nfklBigIntToI(&rem);
		fprintf(stderr,"%17ld / %-17ld = %17ld ...... %-17ld\n",-h,-i,d,r);
		assert(d==(-h/-i));
		assert(r==(-h%-i));

		nfklSetBigIntI(&a,h);
		nfklDivRemBigIntI(&a,i,&rem);
		d=nfklBigIntToI(&a);
		r=nfklBigIntToI(&rem);
		fprintf(stderr,"%17ld / %-17ld = %17ld ...... %-17ld\n",h,i,d,r);
		assert(d==(h/i));
		assert(r==(h%i));

		nfklSetBigIntI(&a,-h);
		nfklDivRemBigIntI(&a,i,&rem);
		d=nfklBigIntToI(&a);
		r=nfklBigIntToI(&rem);
		fprintf(stderr,"%17ld / %-17ld = %17ld ...... %-17ld\n",-h,i,d,r);
		assert(d==(-h/i));
		assert(r==(-h%i));

		nfklSetBigIntI(&a,h);
		nfklDivRemBigIntI(&a,-i,&rem);
		d=nfklBigIntToI(&a);
		r=nfklBigIntToI(&rem);
		fprintf(stderr,"%17ld / %-17ld = %17ld ...... %-17ld\n",h,-i,d,r);
		assert(d==(h/-i));
		assert(r==(h%-i));

		fklUninitStringBuffer(&buf);
		nfklUninitBigInt(&a);
		nfklUninitBigInt(&rem);
	}
	{
		int64_t r;
		NfklBigInt a;
		FklStringBuffer buf;
		const int64_t h=114514+A_BIG_NUM*2;
		const int64_t i=19198+A_BIG_NUM;
		fklInitStringBuffer(&buf);
		nfklInitBigIntI(&a,-h);

		nfklRemBigIntI(&a,-i);

		r=nfklBigIntToI(&a);
		fprintf(stderr,"%17ld %% %-17ld = %-17ld\n",-h,-i,r);
		assert(r==(-h%-i));

		nfklSetBigIntI(&a,h);
		nfklRemBigIntI(&a,i);
		r=nfklBigIntToI(&a);
		fprintf(stderr,"%17ld %% %-17ld = %-17ld\n",h,i,r);
		assert(r==(h%i));

		nfklSetBigIntI(&a,-h);
		nfklRemBigIntI(&a,i);
		r=nfklBigIntToI(&a);
		fprintf(stderr,"%17ld %% %-17ld = %-17ld\n",-h,i,r);
		assert(r==(-h%i));

		nfklSetBigIntI(&a,h);
		nfklRemBigIntI(&a,-i);
		r=nfklBigIntToI(&a);
		fprintf(stderr,"%17ld %% %-17ld = %-17ld\n",h,-i,r);
		assert(r==(h%-i));

		fklUninitStringBuffer(&buf);
		nfklUninitBigInt(&a);
	}
}

int main()
{
	sub_test0();
	sub_test1();
	sub_test2();
	sub_test3();
	sub_test4();
	return 0;
}
