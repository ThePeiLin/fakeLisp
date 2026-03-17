#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/utils.h>

#include <inttypes.h>

static FklIns ins[4] = { FKL_INS_STATIC_INIT };

#define I24_L8_MASK (0xFF)
#define I32_L16_MASK (0xFFFF)
#define I64_L24_MASK (0xFFFFFF)

int main() {
    uint64_t ux = 0x123456;
    int64_t s = -0x123456;
    FklInsArg arg;
    // IuC
    ins[0] = FKL_MAKE_INS_IuC(FKL_OP_LOAD_PROTO, ux);
    arg.ux = FKL_INS_uC(ins[0]);
    fputs("IuC\n", stderr);
    fprintf(stderr,
            "[%s: %d] ux= %" PRIu64 ", arg.ux= %" PRIu64 "\n",
            __FILE__,
            __LINE__,
            ux,
            arg.ux);
    FKL_ASSERT(arg.ux == ux);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr,
            "[%s: %d] ux= %" PRIu64 ", arg.ux= %" PRIu64 "\n",
            __FILE__,
            __LINE__,
            ux,
            arg.ux);
    FKL_ASSERT(arg.ux == ux);
    fputc('\n', stderr);

    // IsC
    ins[0] = FKL_MAKE_INS_IsC(FKL_OP_JMP_IF_TRUE_C, s);
    arg.ix = FKL_INS_sC(ins[0]);
    fputs("IsC\n", stderr);
    fprintf(stderr,
            "[%s: %d] i= %" PRId64 ", arg.ix= %" PRId64 "\n",
            __FILE__,
            __LINE__,
            s,
            arg.ix);
    FKL_ASSERT(arg.ix == s);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr,
            "[%s: %d] i= %" PRId64 ", arg.ix= %" PRId64 "\n",
            __FILE__,
            __LINE__,
            s,
            arg.ix);
    FKL_ASSERT(arg.ix == s);
    fputc('\n', stderr);

    // IuBB
    ux = 0x12345678;
    int r = FKL_MAKE_INS(ins, FKL_OP_GET_LOC_X, .ux = ux);
    FKL_ASSERT(r == 2);
	(void)r;
    arg.ux = FKL_INS_uX(ins);
    fputs("IuBB\n", stderr);
    fprintf(stderr,
            "[%s: %d] ux= %" PRIu64 ", arg.ux= %" PRIu64 "\n",
            __FILE__,
            __LINE__,
            ux,
            arg.ux);
    FKL_ASSERT(arg.ux == ux);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr,
            "[%s: %d] ux= %" PRIu64 ", arg.ux= %" PRIu64 "\n",
            __FILE__,
            __LINE__,
            ux,
            arg.ux);
    FKL_ASSERT(arg.ux == ux);
    fputc('\n', stderr);

    // IsBB
    s = -0x12345678;
    r = FKL_MAKE_INS(ins, FKL_OP_JMP_IF_TRUE_X, .ix = s);
    FKL_ASSERT(r == 2);
    arg.ix = FKL_INS_sX(ins);
    fputs("IsBB\n", stderr);
    fprintf(stderr,
            "[%s: %d] i= %" PRId64 ", arg.ix= %" PRId64 "\n",
            __FILE__,
            __LINE__,
            s,
            arg.ix);
    FKL_ASSERT(arg.ix == s);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr,
            "[%s: %d] i= %" PRId64 ", arg.ix= %" PRId64 "\n",
            __FILE__,
            __LINE__,
            s,
            arg.ix);
    FKL_ASSERT(arg.ix == s);
    fputc('\n', stderr);

    // IsCCB
    s = -0x123456789ABCDEF0;
    r = FKL_MAKE_INS(ins, FKL_OP_JMP_IF_TRUE_XX, .ix = s);
    FKL_ASSERT(r == 3);
    arg.ix = FKL_INS_sXX(ins);
    fputs("IsCCB\n", stderr);
    fprintf(stderr,
            "[%s: %d] i= %" PRId64 ", arg.ix= %" PRId64 "\n",
            __FILE__,
            __LINE__,
            s,
            arg.ix);
    FKL_ASSERT(arg.ix == s);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr,
            "[%s: %d] i= %" PRId64 ", arg.ix= %" PRId64 "\n",
            __FILE__,
            __LINE__,
            s,
            arg.ix);
    FKL_ASSERT(arg.ix == s);
    fputc('\n', stderr);
}
