#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/utils.h>

#include <inttypes.h>

static FklIns ins[4] = { FKL_INSTRUCTION_STATIC_INIT };

#define I24_L8_MASK (0xFF)
#define I32_L16_MASK (0xFFFF)
#define I64_L24_MASK (0xFFFFFF)

static inline void set_ins_uc(FklIns *ins, uint32_t k) {
    ins->au = k & I24_L8_MASK;
    ins->bu = k >> FKL_BYTE_WIDTH;
}

static inline void set_ins_uxx(FklIns *ins, uint64_t k) {
    set_ins_uc(&ins[0], k & I64_L24_MASK);
    ins[1].op = FKL_OP_EXTRA_ARG;
    set_ins_uc(&ins[1], (k >> FKL_I24_WIDTH) & I64_L24_MASK);
    ins[2].op = FKL_OP_EXTRA_ARG;
    ins[2].bu = (k >> (FKL_I24_WIDTH * 2));
}

static inline void set_ins_ux(FklIns *ins, uint32_t k) {
    ins[0].bu = k & I32_L16_MASK;
    ins[1].op = FKL_OP_EXTRA_ARG;
    ins[1].bu = k >> FKL_I16_WIDTH;
}

int main() {
    uint64_t ux = 0x123456;
    int64_t s = -0x123456;
    FklInsArg arg;
    // IuC
    ins[0].op = FKL_OP_PUSH_I24;
    set_ins_uc(ins, ux);
    arg.ux = FKL_GET_INS_UC(ins);
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
    ins[0].op = FKL_OP_JMP_IF_TRUE_C;
    set_ins_uc(ins, s + FKL_I24_OFFSET);
    arg.ix = FKL_GET_INS_IC(ins);
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
    ins[0].op = FKL_OP_GET_LOC_X;
    set_ins_ux(ins, ux);
    arg.ux = FKL_GET_INS_UX(ins);
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
    ins[0].op = FKL_OP_JMP_IF_TRUE_X;
    set_ins_ux(ins, s);
    arg.ix = FKL_GET_INS_IX(ins);
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
    ins[0].op = FKL_OP_JMP_IF_TRUE_XX;
    set_ins_uxx(ins, s);
    arg.ix = FKL_GET_INS_IXX(ins);
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
