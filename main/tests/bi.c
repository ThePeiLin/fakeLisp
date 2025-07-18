#include <fakeLisp/base.h>
#include <fakeLisp/bigint.h>
#include <fakeLisp/common.h>
#include <fakeLisp/zmalloc.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define A_BIG_NUM ((int64_t)4294967295)

static void sub_test0(void) {
    {
        FklBigInt a;
        FklBigInt b;
        FklBigInt c;

        fklInitBigIntI(&a, -114514);
        fklInitBigIntD(&b, -114514.1919810);
        FKL_ASSERT(fklBigIntEqual(&a, &b));
        fklInitBigIntU(&c, 1919810);
        fklSetBigIntI(&a, 1919810);
        FKL_ASSERT(fklBigIntEqual(&a, &c));

        fklUninitBigInt(&a);
        fklUninitBigInt(&b);
        fklUninitBigInt(&c);
    }
    {
        FklBigInt *a = fklCreateBigIntI(-114514);
        FklBigInt *b = fklCreateBigIntD(-114514.1919810);
        FklBigInt *c = fklCreateBigIntU(1919810);

        FKL_ASSERT(fklBigIntEqual(a, b));
        fklSetBigIntI(a, 1919810);
        FKL_ASSERT(fklBigIntEqual(a, c));

        fklDestroyBigInt(a);
        fklDestroyBigInt(b);
        fklDestroyBigInt(c);
    }
    {
        FklBigInt a;
        FklBigInt b;
        FklBigInt c;
        FklBigInt d;
        FklBigInt e;
        FklBigInt f;
        fklInitBigIntWithDecCharBuf(&a, "114514", strlen("114514"));
        fklInitBigIntWithHexCharBuf(&c,
                "0x00000000114514abcd",
                strlen("0x00000000114514abcd"));
        fklInitBigIntWithOctCharBuf(&e,
                "011451477665544",
                strlen("011451477665544"));
        fklInitBigIntI(&b, 114514);
        fklInitBigIntI(&d, 0x114514abcd);
        fklInitBigIntI(&f, 011451477665544);

        FKL_ASSERT(fklBigIntEqual(&a, &b));
        FKL_ASSERT(fklBigIntEqual(&c, &d));
        FKL_ASSERT(fklBigIntEqual(&e, &f));

        fklUninitBigInt(&a);
        fklUninitBigInt(&b);
        fklUninitBigInt(&c);
        fklUninitBigInt(&d);
        fklUninitBigInt(&e);
        fklUninitBigInt(&f);
    }
    {
        FklBigInt a;
        FklBigInt b;
        FklBigInt c;
        FklBigInt d;
        FklBigInt e;
        FklBigInt f;
        fklInitBigIntWithCstr(&a, "114514");
        fklInitBigIntWithCstr(&c, "0x00000000114514abcd");
        fklInitBigIntWithCstr(&e, "011451477665544");
        fklInitBigIntI(&b, 114514);
        fklInitBigIntI(&d, 0x114514abcd);
        fklInitBigIntI(&f, 011451477665544);

        FKL_ASSERT(fklBigIntEqual(&a, &b));
        FKL_ASSERT(fklBigIntEqual(&c, &d));
        FKL_ASSERT(fklBigIntEqual(&e, &f));

        fklUninitBigInt(&a);
        fklUninitBigInt(&b);
        fklUninitBigInt(&c);
        fklUninitBigInt(&d);
        fklUninitBigInt(&e);
        fklUninitBigInt(&f);
    }
    {
        FklBigInt c;
        FklBigInt d;
        fklInitBigIntWithHexCharBuf(&c,
                "-0x00000000114514abcd",
                strlen("-0x00000000114514abcd"));
        fklInitBigIntI(&d, -0x114514abcd);

        FKL_ASSERT(fklBigIntEqual(&c, &d));

        fklUninitBigInt(&c);
        fklUninitBigInt(&d);
    }
    {
        FklBigInt c;
        {
            fklInitBigIntWithHexCharBuf(&c,
                    "-0x00000000",
                    strlen("-0x00000000"));
            FKL_ASSERT(FKL_BIGINT_IS_0(&c));
            fklUninitBigInt(&c);
        }
        {
            fklInitBigIntWithCharBuf(&c, "00000000", strlen("00000000"));
            FKL_ASSERT(FKL_BIGINT_IS_0(&c));
            fklUninitBigInt(&c);
        }
    }
    {
        FklBigInt c;
        fklInitBigIntI(&c, 1145141919);

        int64_t i = fklBigIntToI(&c);
        uint64_t u = fklBigIntToU(&c);
        double d = fklBigIntToD(&c);
        fprintf(stderr, "i: %ld, u: %lu, d: %lf\n", i, u, d);
        FKL_ASSERT(i == 1145141919);
        FKL_ASSERT(u == 1145141919);
        FKL_ASSERT(!islessgreater(d, 1145141919.0));

        fklSetBigIntI(&c, -1145141919);
        i = fklBigIntToI(&c);
        d = fklBigIntToD(&c);
        fprintf(stderr, "i: %ld, d: %lf\n", i, d);
        FKL_ASSERT(i == -1145141919);
        FKL_ASSERT(!islessgreater(d, -1145141919.0));

        fklSetBigIntI(&c, INT64_MAX);
        i = fklBigIntToI(&c);
        u = fklBigIntToU(&c);
        fprintf(stderr, "i: %ld, u: %lu\n", i, u);
        FKL_ASSERT(i == INT64_MAX);
        FKL_ASSERT(u == INT64_MAX);

        fklSetBigIntI(&c, INT64_MIN);
        i = fklBigIntToI(&c);
        fprintf(stderr, "i: %ld\n", i);
        FKL_ASSERT(i == INT64_MIN);

        fklSubBigIntI(&c, 1);
        i = fklBigIntToI(&c);
        fprintf(stderr, "i: %ld\n", i);
        FKL_ASSERT(i == INT64_MIN);

        fklSetBigIntI(&c, INT64_MAX);
        fklAddBigIntI(&c, 1);
        i = fklBigIntToI(&c);
        fprintf(stderr, "i: %ld\n", i);
        FKL_ASSERT(i == INT64_MAX);

        fklSetBigIntU(&c, UINT64_MAX);
        fklAddBigIntI(&c, 1);
        u = fklBigIntToU(&c);
        fprintf(stderr, "u: %lu\n", u);
        FKL_ASSERT(u == UINT64_MAX);

        fklUninitBigInt(&c);
    }
}

static void sub_test1(void) {
    {
        FklBigInt a;
        FklBigInt b;
        FklBigInt c;
        FklBigInt d = FKL_BIGINT_0;
        fklInitBigIntI(&a, 114514);
        fklInitBigIntI(&b, 1919810);
        fklInitBigIntI(&c, 114514 + 1919810);

        fklAddBigInt(&d, &a);
        fklAddBigInt(&d, &b);

        FKL_ASSERT(fklBigIntEqual(&c, &d));

        FKL_ASSERT(fklBigIntCmp(&a, &b) < 0);
        FKL_ASSERT(fklBigIntAbsCmp(&a, &b) < 0);

        FKL_ASSERT(fklBigIntCmp(&b, &a) > 0);
        FKL_ASSERT(fklBigIntAbsCmp(&b, &a) > 0);

        FKL_ASSERT(fklBigIntCmp(&c, &d) == 0);
        FKL_ASSERT(fklBigIntAbsCmp(&c, &d) == 0);

        fklSubBigInt(&d, &b);

        FKL_ASSERT(fklBigIntEqual(&a, &d));
        FKL_ASSERT(fklBigIntCmp(&a, &d) == 0);
        FKL_ASSERT(fklBigIntAbsCmp(&a, &d) == 0);

        fklUninitBigInt(&a);
        fklUninitBigInt(&b);
        fklUninitBigInt(&c);
        fklUninitBigInt(&d);
    }
    {
        FklBigInt *a = fklCreateBigIntI(114514 + A_BIG_NUM);
        FklBigInt *b = fklCreateBigIntI(1919810 + A_BIG_NUM);
        FklBigInt *c =
                fklCreateBigIntI(114514 + 1919810 + A_BIG_NUM + A_BIG_NUM);
        FklBigInt *d = fklCreateBigInt0();

        fklAddBigInt(d, a);
        fklAddBigInt(d, b);

        FKL_ASSERT(fklBigIntEqual(c, d));
        FKL_ASSERT(fklBigIntCmp(a, b) < 0);
        FKL_ASSERT(fklBigIntCmp(b, a) > 0);
        FKL_ASSERT(fklBigIntCmp(c, d) == 0);

        fklSubBigInt(d, b);

        FKL_ASSERT(fklBigIntEqual(a, d));
        FKL_ASSERT(fklBigIntCmp(a, d) == 0);

        fklDestroyBigInt(a);
        fklDestroyBigInt(b);
        fklDestroyBigInt(c);
        fklDestroyBigInt(d);
    }
}

static void sub_test2(void) {
    {
        FklBigInt a;
        FklBigInt b;
        FklBigInt c;
        fklInitBigInt0(&a);
        fklInitBigInt1(&b);
        fklInitBigIntN1(&c);

        FKL_ASSERT(FKL_BIGINT_IS_0(&a));
        FKL_ASSERT(FKL_BIGINT_IS_1(&b));
        FKL_ASSERT(FKL_BIGINT_IS_N1(&c));
        FKL_ASSERT(FKL_BIGINT_IS_ABS1(&c) && FKL_BIGINT_IS_ABS1(&b));

        fklUninitBigInt(&a);
        fklUninitBigInt(&b);
        fklUninitBigInt(&c);
    }
    {
        FklBigInt *a = fklCreateBigIntI(114514 + A_BIG_NUM);
        FklBigInt *b = fklCreateBigIntI(-1919810 - A_BIG_NUM);
        FklBigInt *c =
                fklCreateBigIntI(114514 - 1919810 + A_BIG_NUM - A_BIG_NUM);
        FklBigInt *d = fklCreateBigInt0();

        fklAddBigInt(d, a);
        fklAddBigInt(d, b);

        FKL_ASSERT(fklBigIntEqual(c, d));
        FKL_ASSERT(fklBigIntCmp(a, b) > 0);
        FKL_ASSERT(fklBigIntCmp(b, a) < 0);
        FKL_ASSERT(fklBigIntCmp(c, d) == 0);

        FKL_ASSERT(fklBigIntAbsCmp(a, b) < 0);
        FKL_ASSERT(fklBigIntAbsCmp(b, a) > 0);

        fklSubBigInt(d, b);

        FKL_ASSERT(fklBigIntEqual(a, d));
        FKL_ASSERT(fklBigIntCmp(a, d) == 0);

        fklDestroyBigInt(a);
        fklDestroyBigInt(b);
        fklDestroyBigInt(c);
        fklDestroyBigInt(d);
    }
    {
        FklBigInt *a = fklCreateBigIntI(-114514 - A_BIG_NUM);
        FklBigInt *b = fklCreateBigIntI(-1919810 - A_BIG_NUM);
        FklBigInt *c =
                fklCreateBigIntI(-114514 - 1919810 - A_BIG_NUM - A_BIG_NUM);
        FklBigInt *d = fklCreateBigInt0();

        fklAddBigInt(d, a);
        fklAddBigInt(d, b);

        FKL_ASSERT(fklBigIntEqual(c, d));
        FKL_ASSERT(fklBigIntCmp(a, b) > 0);
        FKL_ASSERT(fklBigIntCmp(b, a) < 0);
        FKL_ASSERT(fklBigIntCmp(c, d) == 0);

        fklSubBigInt(d, b);

        FKL_ASSERT(fklBigIntEqual(a, d));
        FKL_ASSERT(fklBigIntCmp(a, d) == 0);

        fklDestroyBigInt(a);
        fklDestroyBigInt(b);
        fklDestroyBigInt(c);
        fklDestroyBigInt(d);
    }
    {
        FklBigInt *a = fklCreateBigIntI(-114514 - A_BIG_NUM);
        FklBigInt *b = fklCreateBigIntI(1919810 + A_BIG_NUM);
        FklBigInt *c =
                fklCreateBigIntI(-114514 + 1919810 - A_BIG_NUM + A_BIG_NUM);
        FklBigInt *d = fklCreateBigInt0();

        fklAddBigInt(d, a);
        fklAddBigInt(d, b);

        FKL_ASSERT(fklBigIntEqual(c, d));
        FKL_ASSERT(fklBigIntCmp(a, b) < 0);
        FKL_ASSERT(fklBigIntCmp(b, a) > 0);
        FKL_ASSERT(fklBigIntCmp(c, d) == 0);

        fklSubBigInt(d, b);

        FKL_ASSERT(fklBigIntEqual(a, d));
        FKL_ASSERT(fklBigIntCmp(a, d) == 0);

        fklDestroyBigInt(a);
        fklDestroyBigInt(b);
        fklDestroyBigInt(c);
        fklDestroyBigInt(d);
    }
}

static void sub_test3(void) {
    {
        FklBigInt *a = fklCreateBigIntI(114514);
        FklBigInt *b = fklCreateBigIntI(1919810 + A_BIG_NUM);
        FklBigInt *c = fklCreateBigIntI(114514 * (1919810 + A_BIG_NUM));
        FklBigInt *d = fklCreateBigInt1();

        fklMulBigInt(d, a);
        fklMulBigInt(d, b);

        FKL_ASSERT(fklBigIntEqual(c, d));

        fklDestroyBigInt(a);
        fklDestroyBigInt(b);
        fklDestroyBigInt(c);
        fklDestroyBigInt(d);
    }
    {
        FklBigInt *a = fklCreateBigIntI(-114514);
        FklBigInt *b = fklCreateBigIntI(1919810 + A_BIG_NUM);
        FklBigInt *c = fklCreateBigIntI(-114514 * (1919810 + A_BIG_NUM));
        FklBigInt *d = fklCreateBigInt1();

        fklMulBigInt(d, a);
        fklMulBigInt(d, b);

        FKL_ASSERT(fklBigIntEqual(c, d));

        fklDestroyBigInt(a);
        fklDestroyBigInt(b);
        fklDestroyBigInt(c);
        fklDestroyBigInt(d);
    }
    {
        FklBigInt a;
        FklBigInt b;
        FklBigInt c;
        fklInitBigInt0(&a);
        fklInitBigIntI(&b, 114514);
        fklInitBigIntI(&c, 1919);

        FKL_ASSERT(fklIsBigIntEven(&a));
        FKL_ASSERT(fklIsBigIntEven(&b));
        FKL_ASSERT(fklIsBigIntOdd(&c));

        FKL_ASSERT(!fklIsBigIntOdd(&a));
        FKL_ASSERT(!fklIsBigIntOdd(&b));
        FKL_ASSERT(!fklIsBigIntEven(&c));

        fklSetBigIntI(&b, -1);
        FKL_ASSERT(fklIsBigIntLe0(&a));
        FKL_ASSERT(fklIsBigIntLe0(&b));
        FKL_ASSERT(fklIsBigIntLt0(&b));
        FKL_ASSERT(!fklIsBigIntLt0(&a));

        fklUninitBigInt(&a);
        fklUninitBigInt(&b);
        fklUninitBigInt(&c);
    }
    {
        FklStringBuffer buf;
        FklBigInt a;
        FklString *str = NULL;
        fklInitBigIntI(&a, 1145141919);
        fklInitStringBuffer(&buf);

        // pos
        fklBigIntToStringBuffer(&a, &buf, 10, FKL_BIGINT_FMT_FLAG_NONE);
        str = fklBigIntToString(&a, 10, FKL_BIGINT_FMT_FLAG_NONE);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "1145141919"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "1145141919"));

        // neg
        fklZfree(str);
        fklUninitStringBuffer(&buf);

        fklSetBigIntI(&a, -1145141919);
        fklBigIntToStringBuffer(&a, &buf, 10, FKL_BIGINT_FMT_FLAG_NONE);
        str = fklBigIntToString(&a, 10, FKL_BIGINT_FMT_FLAG_NONE);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "-1145141919"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "-1145141919"));

        fklUninitBigInt(&a);
        fklUninitStringBuffer(&buf);
        fklZfree(str);
    }
    {
        // oct
        FklStringBuffer buf;
        FklBigInt a;
        FklString *str = NULL;
        fklInitBigIntI(&a, 1145141919);
        fklInitStringBuffer(&buf);

        fklSetBigIntI(&a, 1145141919);
        fklBigIntToStringBuffer(&a, &buf, 8, FKL_BIGINT_FMT_FLAG_NONE);
        str = fklBigIntToString(&a, 8, FKL_BIGINT_FMT_FLAG_NONE);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "10420275237"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "10420275237"));

        fklUninitStringBuffer(&buf);
        fklZfree(str);

        // oct alternate

        fklBigIntToStringBuffer(&a, &buf, 8, FKL_BIGINT_FMT_FLAG_ALTERNATE);
        str = fklBigIntToString(&a, 8, FKL_BIGINT_FMT_FLAG_ALTERNATE);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "010420275237"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "010420275237"));
        fklUninitStringBuffer(&buf);
        fklZfree(str);

        // oct neg

        fklSetBigIntI(&a, -1145141919);
        fklBigIntToStringBuffer(&a, &buf, 8, FKL_BIGINT_FMT_FLAG_NONE);
        str = fklBigIntToString(&a, 8, FKL_BIGINT_FMT_FLAG_NONE);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "-10420275237"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "-10420275237"));
        fklUninitStringBuffer(&buf);
        fklZfree(str);

        // oct neg alternate

        fklBigIntToStringBuffer(&a, &buf, 8, FKL_BIGINT_FMT_FLAG_ALTERNATE);
        str = fklBigIntToString(&a, 8, FKL_BIGINT_FMT_FLAG_ALTERNATE);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "-010420275237"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "-010420275237"));
        fklUninitStringBuffer(&buf);
        fklZfree(str);
        fklUninitBigInt(&a);
    }
    {
        FklStringBuffer buf;
        FklBigInt a;
        FklString *str = NULL;
        fklSetBigIntI(&a, 1145141919);

        // hex

        fklBigIntToStringBuffer(&a, &buf, 16, FKL_BIGINT_FMT_FLAG_NONE);
        str = fklBigIntToString(&a, 16, FKL_BIGINT_FMT_FLAG_NONE);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "44417a9f"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "44417a9f"));
        fklUninitStringBuffer(&buf);
        fklZfree(str);

        // hex alternate

        fklBigIntToStringBuffer(&a, &buf, 16, FKL_BIGINT_FMT_FLAG_ALTERNATE);
        str = fklBigIntToString(&a, 16, FKL_BIGINT_FMT_FLAG_ALTERNATE);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "0x44417a9f"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "0x44417a9f"));
        fklUninitStringBuffer(&buf);
        fklZfree(str);

        // hex capitals

        fklBigIntToStringBuffer(&a, &buf, 16, FKL_BIGINT_FMT_FLAG_CAPITALS);
        str = fklBigIntToString(&a, 16, FKL_BIGINT_FMT_FLAG_CAPITALS);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "44417A9F"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "44417A9F"));
        fklUninitStringBuffer(&buf);
        fklZfree(str);

        // hex alternate capitals

        fklBigIntToStringBuffer(&a,
                &buf,
                16,
                FKL_BIGINT_FMT_FLAG_ALTERNATE | FKL_BIGINT_FMT_FLAG_CAPITALS);
        str = fklBigIntToString(&a,
                16,
                FKL_BIGINT_FMT_FLAG_ALTERNATE | FKL_BIGINT_FMT_FLAG_CAPITALS);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "0X44417A9F"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "0X44417A9F"));
        fklUninitStringBuffer(&buf);
        fklZfree(str);

        // hex neg
        fklSetBigIntI(&a, -1145141919);

        fklBigIntToStringBuffer(&a, &buf, 16, FKL_BIGINT_FMT_FLAG_NONE);
        str = fklBigIntToString(&a, 16, FKL_BIGINT_FMT_FLAG_NONE);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "-44417a9f"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "-44417a9f"));
        fklUninitStringBuffer(&buf);
        fklZfree(str);

        // hex neg alternate
        fklBigIntToStringBuffer(&a, &buf, 16, FKL_BIGINT_FMT_FLAG_ALTERNATE);
        str = fklBigIntToString(&a, 16, FKL_BIGINT_FMT_FLAG_ALTERNATE);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "-0x44417a9f"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "-0x44417a9f"));
        fklUninitStringBuffer(&buf);
        fklZfree(str);

        // hex neg capitals
        fklBigIntToStringBuffer(&a, &buf, 16, FKL_BIGINT_FMT_FLAG_CAPITALS);
        str = fklBigIntToString(&a, 16, FKL_BIGINT_FMT_FLAG_CAPITALS);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "-44417A9F"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "-44417A9F"));
        fklUninitStringBuffer(&buf);
        fklZfree(str);

        // hex neg alternate capitals
        fklBigIntToStringBuffer(&a,
                &buf,
                16,
                FKL_BIGINT_FMT_FLAG_CAPITALS | FKL_BIGINT_FMT_FLAG_ALTERNATE);
        str = fklBigIntToString(&a,
                16,
                FKL_BIGINT_FMT_FLAG_CAPITALS | FKL_BIGINT_FMT_FLAG_ALTERNATE);

        fprintf(stderr,
                "buf: %s, len: %u\n",
                fklStringBufferBody(&buf),
                fklStringBufferLen(&buf));
        FKL_ASSERT(!strcmp(fklStringBufferBody(&buf), "-0X44417A9F"));
        fprintf(stderr, "str: %s, len: %lu\n", str->str, str->size);
        FKL_ASSERT(!strcmp(str->str, "-0X44417A9F"));
        fklUninitStringBuffer(&buf);
        fklZfree(str);

        fklUninitBigInt(&a);
    }
}

static void sub_test4(void) {
    const int64_t h = 114514 + A_BIG_NUM;
    const int64_t i = 1919810;
    {
        FklBigInt a;
        FklBigInt rem = FKL_BIGINT_0;
        FklStringBuffer buf;
        fklInitStringBuffer(&buf);
        fklInitBigIntI(&a, h);
        fklMulBigIntI(&a, i);

        fklDivRemBigIntI(&a, i, &rem);

        int64_t d = fklBigIntToI(&a);
        int64_t r = fklBigIntToI(&rem);
        fprintf(stderr,
                "%17ld / %-17ld = %17ld ...... %-17ld\n",
                h * i,
                i,
                d,
                r);
        FKL_ASSERT(d == 114514 + A_BIG_NUM);
        FKL_ASSERT(r == 0);

        fklUninitStringBuffer(&buf);
        fklUninitBigInt(&a);
        fklUninitBigInt(&rem);
    }
    {
        FklBigInt a;
        FklBigInt rem = FKL_BIGINT_0;
        FklStringBuffer buf;
        fklInitStringBuffer(&buf);
        fklInitBigIntI(&a, h);
        fklMulBigIntI(&a, -i);

        fklDivRemBigIntI(&a, i, &rem);

        int64_t d = fklBigIntToI(&a);
        int64_t r = fklBigIntToI(&rem);
        fprintf(stderr,
                "%17ld / %-17ld = %17ld ...... %-17ld\n",
                h * -i,
                i,
                d,
                r);
        FKL_ASSERT(d == -114514 - A_BIG_NUM);
        FKL_ASSERT(r == 0);

        fklUninitStringBuffer(&buf);
        fklUninitBigInt(&a);
    }
    {
        int64_t d;
        int64_t r;
        FklBigInt a;
        FklBigInt rem;
        FklStringBuffer buf;
        const int64_t h = 114514;
        const int64_t i = 19198;
        fklInitStringBuffer(&buf);
        fklInitBigIntI(&a, -h);
        fklInitBigInt0(&rem);

        fklDivRemBigIntI(&a, -i, &rem);

        d = fklBigIntToI(&a);
        r = fklBigIntToI(&rem);
        fprintf(stderr, "%17ld / %-17ld = %17ld ...... %-17ld\n", -h, -i, d, r);
        FKL_ASSERT(d == (-h / -i));
        FKL_ASSERT(r == (-h % -i));

        fklSetBigIntI(&a, h);
        fklDivRemBigIntI(&a, i, &rem);
        d = fklBigIntToI(&a);
        r = fklBigIntToI(&rem);
        fprintf(stderr, "%17ld / %-17ld = %17ld ...... %-17ld\n", h, i, d, r);
        FKL_ASSERT(d == (h / i));
        FKL_ASSERT(r == (h % i));

        fklSetBigIntI(&a, -h);
        fklDivRemBigIntI(&a, i, &rem);
        d = fklBigIntToI(&a);
        r = fklBigIntToI(&rem);
        fprintf(stderr, "%17ld / %-17ld = %17ld ...... %-17ld\n", -h, i, d, r);
        FKL_ASSERT(d == (-h / i));
        FKL_ASSERT(r == (-h % i));

        fklSetBigIntI(&a, h);
        fklDivRemBigIntI(&a, -i, &rem);
        d = fklBigIntToI(&a);
        r = fklBigIntToI(&rem);
        fprintf(stderr, "%17ld / %-17ld = %17ld ...... %-17ld\n", h, -i, d, r);
        FKL_ASSERT(d == (h / -i));
        FKL_ASSERT(r == (h % -i));

        fklUninitStringBuffer(&buf);
        fklUninitBigInt(&a);
        fklUninitBigInt(&rem);
    }
    {
        int64_t d;
        int64_t r;
        FklBigInt a;
        FklBigInt rem;
        FklStringBuffer buf;
        const int64_t h = 114514 + A_BIG_NUM * 2;
        const int64_t i = 19198 + A_BIG_NUM;
        fklInitStringBuffer(&buf);
        fklInitBigIntI(&a, -h);
        fklInitBigInt0(&rem);

        fklDivRemBigIntI(&a, -i, &rem);

        d = fklBigIntToI(&a);
        r = fklBigIntToI(&rem);
        fprintf(stderr, "%17ld / %-17ld = %17ld ...... %-17ld\n", -h, -i, d, r);
        FKL_ASSERT(d == (-h / -i));
        FKL_ASSERT(r == (-h % -i));

        fklSetBigIntI(&a, h);
        fklDivRemBigIntI(&a, i, &rem);
        d = fklBigIntToI(&a);
        r = fklBigIntToI(&rem);
        fprintf(stderr, "%17ld / %-17ld = %17ld ...... %-17ld\n", h, i, d, r);
        FKL_ASSERT(d == (h / i));
        FKL_ASSERT(r == (h % i));

        fklSetBigIntI(&a, -h);
        fklDivRemBigIntI(&a, i, &rem);
        d = fklBigIntToI(&a);
        r = fklBigIntToI(&rem);
        fprintf(stderr, "%17ld / %-17ld = %17ld ...... %-17ld\n", -h, i, d, r);
        FKL_ASSERT(d == (-h / i));
        FKL_ASSERT(r == (-h % i));

        fklSetBigIntI(&a, h);
        fklDivRemBigIntI(&a, -i, &rem);
        d = fklBigIntToI(&a);
        r = fklBigIntToI(&rem);
        fprintf(stderr, "%17ld / %-17ld = %17ld ...... %-17ld\n", h, -i, d, r);
        FKL_ASSERT(d == (h / -i));
        FKL_ASSERT(r == (h % -i));

        fklUninitStringBuffer(&buf);
        fklUninitBigInt(&a);
        fklUninitBigInt(&rem);
    }
    {
        int64_t r;
        FklBigInt a;
        FklStringBuffer buf;
        const int64_t h = 114514 + A_BIG_NUM * 2;
        const int64_t i = 19198 + A_BIG_NUM;
        fklInitStringBuffer(&buf);
        fklInitBigIntI(&a, -h);

        fklRemBigIntI(&a, -i);

        r = fklBigIntToI(&a);
        fprintf(stderr, "%17ld %% %-17ld = %-17ld\n", -h, -i, r);
        FKL_ASSERT(r == (-h % -i));

        fklSetBigIntI(&a, h);
        fklRemBigIntI(&a, i);
        r = fklBigIntToI(&a);
        fprintf(stderr, "%17ld %% %-17ld = %-17ld\n", h, i, r);
        FKL_ASSERT(r == (h % i));

        fklSetBigIntI(&a, -h);
        fklRemBigIntI(&a, i);
        r = fklBigIntToI(&a);
        fprintf(stderr, "%17ld %% %-17ld = %-17ld\n", -h, i, r);
        FKL_ASSERT(r == (-h % i));

        fklSetBigIntI(&a, h);
        fklRemBigIntI(&a, -i);
        r = fklBigIntToI(&a);
        fprintf(stderr, "%17ld %% %-17ld = %-17ld\n", h, -i, r);
        FKL_ASSERT(r == (h % -i));

        fklUninitStringBuffer(&buf);
        fklUninitBigInt(&a);
    }
}

typedef struct {
    int64_t num;
    FklBigIntDigit digits[1];
} OtherBi;

static FklBigIntDigit *other_bi_alloc_cb(void *ctx, size_t size) {
    OtherBi **pbi = (OtherBi **)ctx;
    *pbi = (OtherBi *)fklZmalloc(
            sizeof(OtherBi)
            + (size == 0 ? size - 1 : 0) * sizeof(FklBigIntDigit));
    FKL_ASSERT(*pbi);
    return (*pbi)->digits;
}

static int64_t *other_bi_num_cb(void *ctx) {
    return &(*((OtherBi **)ctx))->num;
}

static const FklBigIntInitWithCharBufMethodTable method_table = {
    .alloc = other_bi_alloc_cb,
    .num = other_bi_num_cb,
};

static void sub_test5(void) {
    {
        OtherBi *a0 = NULL;
        FklBigInt a1;
        fklInitBigIntWithDecCharBuf2(&a0,
                &method_table,
                "114514",
                strlen("114514"));
        fklInitBigIntWithDecCharBuf(&a1, "114514", strlen("114514"));
        FklBigInt tmp = {
            .digits = a0->digits,
            .num = a0->num,
            .size = fklAbs(a0->num),
            .const_size = 1,
        };
        (void)tmp;

        FKL_ASSERT(fklBigIntEqual(&a1, &tmp));

        fklUninitBigInt(&a1);
        fklZfree(a0);
        a0 = NULL;

        fklInitBigIntWithHexCharBuf2(&a0,
                &method_table,
                "0x00000000114514abcd",
                strlen("0x00000000114514abcd"));
        fklInitBigIntWithHexCharBuf(&a1,
                "0x00000000114514abcd",
                strlen("0x00000000114514abcd"));
        tmp.digits = a0->digits;
        tmp.num = a0->num;
        tmp.size = fklAbs(a0->num);

        FKL_ASSERT(fklBigIntEqual(&a1, &tmp));
        fklUninitBigInt(&a1);
        fklZfree(a0);
        a0 = NULL;

        fklInitBigIntWithOctCharBuf2(&a0,
                &method_table,
                "011451477665544",
                strlen("011451477665544"));
        fklInitBigIntWithOctCharBuf(&a1,
                "011451477665544",
                strlen("011451477665544"));
        tmp.digits = a0->digits;
        tmp.num = a0->num;
        tmp.size = fklAbs(a0->num);

        FKL_ASSERT(fklBigIntEqual(&a1, &tmp));
        fklUninitBigInt(&a1);
        fklZfree(a0);
        a0 = NULL;
    }
    {
        OtherBi *a0 = NULL;
        FklBigInt a1;
        fklInitBigIntWithDecCharBuf2(&a0,
                &method_table,
                "-114514",
                strlen("114514"));
        fklInitBigIntWithDecCharBuf(&a1, "-114514", strlen("114514"));
        FklBigInt tmp = {
            .digits = a0->digits,
            .num = a0->num,
            .size = fklAbs(a0->num),
            .const_size = 1,
        };
        (void)tmp;

        FKL_ASSERT(fklBigIntEqual(&a1, &tmp));

        fklUninitBigInt(&a1);
        fklZfree(a0);
        a0 = NULL;

        fklInitBigIntWithHexCharBuf2(&a0,
                &method_table,
                "-0x00000000114514abcd",
                strlen("0x00000000114514abcd"));
        fklInitBigIntWithHexCharBuf(&a1,
                "-0x00000000114514abcd",
                strlen("0x00000000114514abcd"));
        tmp.digits = a0->digits;
        tmp.num = a0->num;
        tmp.size = fklAbs(a0->num);

        FKL_ASSERT(fklBigIntEqual(&a1, &tmp));
        fklUninitBigInt(&a1);
        fklZfree(a0);
        a0 = NULL;

        fklInitBigIntWithOctCharBuf2(&a0,
                &method_table,
                "-011451477665544",
                strlen("011451477665544"));
        fklInitBigIntWithOctCharBuf(&a1,
                "-011451477665544",
                strlen("011451477665544"));
        tmp.digits = a0->digits;
        tmp.num = a0->num;
        tmp.size = fklAbs(a0->num);

        FKL_ASSERT(fklBigIntEqual(&a1, &tmp));
        fklUninitBigInt(&a1);
        fklZfree(a0);
        a0 = NULL;
    }
}

int main() {
    fprintf(stderr, "sizeof(FklBigInt) == %lu\n", sizeof(FklBigInt));
    sub_test0();
    sub_test1();
    sub_test2();
    sub_test3();
    sub_test4();
    sub_test5();
    return 0;
}
