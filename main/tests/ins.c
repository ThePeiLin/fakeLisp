#include <fakeLisp/bytecode.h>
#include <fakeLisp/common.h>
#include <fakeLisp/utils.h>

static FklInstruction ins[4] = {FKL_INSTRUCTION_STATIC_INIT};

#define I24_L8_MASK (0xFF)
#define I32_L16_MASK (0xFFFF)
#define I64_L24_MASK (0xFFFFFF)

static inline void set_ins_uc(FklInstruction *ins, uint32_t k) {
    ins->au = k & I24_L8_MASK;
    ins->bu = k >> FKL_BYTE_WIDTH;
}

static inline void set_ins_uxx(FklInstruction *ins, uint64_t k) {
    set_ins_uc(&ins[0], k & I64_L24_MASK);
    ins[1].op = FKL_OP_EXTRA_ARG;
    set_ins_uc(&ins[1], (k >> FKL_I24_WIDTH) & I64_L24_MASK);
    ins[2].op = FKL_OP_EXTRA_ARG;
    ins[2].bu = (k >> (FKL_I24_WIDTH * 2));
}

static inline void set_ins_ux(FklInstruction *ins, uint32_t k) {
    ins[0].bu = k & I32_L16_MASK;
    ins[1].op = FKL_OP_EXTRA_ARG;
    ins[1].bu = k >> FKL_I16_WIDTH;
}

int main() {
    uint64_t ux = 0x123456;
    int64_t s = -0x123456;
    FklInstructionArg arg;
    // IuC
    ins[0].op = FKL_OP_PUSH_I64F_C;
    set_ins_uc(ins, ux);
    arg.ux = FKL_GET_INS_UC(ins);
    fputs("IuC\n", stderr);
    fprintf(stderr, "[%s: %d] ux= %" FKL_PRT64U ", arg.ux= %" FKL_PRT64U "\n",
            __FILE__, __LINE__, ux, arg.ux);
    FKL_ASSERT(arg.ux == ux);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr, "[%s: %d] ux= %" FKL_PRT64U ", arg.ux= %" FKL_PRT64U "\n",
            __FILE__, __LINE__, ux, arg.ux);
    FKL_ASSERT(arg.ux == ux);
    fputc('\n', stderr);

    // IsC
    ins[0].op = FKL_OP_JMP_IF_TRUE_C;
    set_ins_uc(ins, s + FKL_I24_OFFSET);
    arg.ix = FKL_GET_INS_IC(ins);
    fputs("IsC\n", stderr);
    fprintf(stderr, "[%s: %d] i= %" FKL_PRT64D ", arg.ix= %" FKL_PRT64D "\n",
            __FILE__, __LINE__, s, arg.ix);
    FKL_ASSERT(arg.ix == s);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr, "[%s: %d] i= %" FKL_PRT64D ", arg.ix= %" FKL_PRT64D "\n",
            __FILE__, __LINE__, s, arg.ix);
    FKL_ASSERT(arg.ix == s);
    fputc('\n', stderr);

    // IuBB
    ux = 0x12345678;
    ins[0].op = FKL_OP_PUSH_I64F_X;
    set_ins_ux(ins, ux);
    arg.ux = FKL_GET_INS_UX(ins);
    fputs("IuBB\n", stderr);
    fprintf(stderr, "[%s: %d] ux= %" FKL_PRT64U ", arg.ux= %" FKL_PRT64U "\n",
            __FILE__, __LINE__, ux, arg.ux);
    FKL_ASSERT(arg.ux == ux);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr, "[%s: %d] ux= %" FKL_PRT64U ", arg.ux= %" FKL_PRT64U "\n",
            __FILE__, __LINE__, ux, arg.ux);
    FKL_ASSERT(arg.ux == ux);
    fputc('\n', stderr);

    // IsBB
    s = -0x12345678;
    ins[0].op = FKL_OP_JMP_IF_TRUE_X;
    set_ins_ux(ins, s);
    arg.ix = FKL_GET_INS_IX(ins);
    fputs("IsBB\n", stderr);
    fprintf(stderr, "[%s: %d] i= %" FKL_PRT64D ", arg.ix= %" FKL_PRT64D "\n",
            __FILE__, __LINE__, s, arg.ix);
    FKL_ASSERT(arg.ix == s);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr, "[%s: %d] i= %" FKL_PRT64D ", arg.ix= %" FKL_PRT64D "\n",
            __FILE__, __LINE__, s, arg.ix);
    FKL_ASSERT(arg.ix == s);
    fputc('\n', stderr);

    // IsCCB
    s = -0x123456789ABCDEF0;
    ins[0].op = FKL_OP_JMP_IF_TRUE_XX;
    set_ins_uxx(ins, s);
    arg.ix = FKL_GET_INS_IXX(ins);
    fputs("IsCCB\n", stderr);
    fprintf(stderr, "[%s: %d] i= %" FKL_PRT64D ", arg.ix= %" FKL_PRT64D "\n",
            __FILE__, __LINE__, s, arg.ix);
    FKL_ASSERT(arg.ix == s);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr, "[%s: %d] i= %" FKL_PRT64D ", arg.ix= %" FKL_PRT64D "\n",
            __FILE__, __LINE__, s, arg.ix);
    FKL_ASSERT(arg.ix == s);
    fputc('\n', stderr);

    // IuCuC
    ux = 0x123456;
    uint64_t uy = 0x789ABC;
    ins[0].op = FKL_OP_CLOSE_REF_X;
    ins[1].op = FKL_OP_EXTRA_ARG;
    set_ins_uc(&ins[0], ux);
    set_ins_uc(&ins[1], uy);
    arg.ux = FKL_GET_INS_UC(ins);
    arg.uy = FKL_GET_INS_UC(&ins[1]);
    fputs("IuCuC\n", stderr);
    fprintf(stderr,
            "[%s: %d] ux= %" FKL_PRT64U ", uy= %" FKL_PRT64U
            ", arg.ux= %" FKL_PRT64U ", arg.uy= %" FKL_PRT64U "\n",
            __FILE__, __LINE__, ux, uy, arg.ux, arg.uy);
    FKL_ASSERT(arg.ux == ux && arg.uy == uy);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr,
            "[%s: %d] ux= %" FKL_PRT64U ", uy= %" FKL_PRT64U
            ", arg.ux= %" FKL_PRT64U ", arg.uy= %" FKL_PRT64U "\n",
            __FILE__, __LINE__, ux, uy, arg.ux, arg.uy);
    FKL_ASSERT(arg.ux == ux && arg.uy == uy);
    fputc('\n', stderr);

    // IuCAuBB
    ux = 0x12345678;
    uy = 0x9ABCDEF0;
    ins[0].op = FKL_OP_CLOSE_REF_XX;
    set_ins_uc(&ins[0], ux & I64_L24_MASK);
    ins[1].op = FKL_OP_EXTRA_ARG;
    ins[1].au = ux >> FKL_I24_WIDTH;
    ins[1].bu = uy & I32_L16_MASK;
    ins[2].op = FKL_OP_EXTRA_ARG;
    ins[2].bu = uy >> FKL_I16_WIDTH;
    arg.ux = FKL_GET_INS_UC(ins) | (((uint32_t)ins[1].au) << FKL_I24_WIDTH);
    arg.uy = ins[1].bu | (((uint64_t)ins[2].bu) << FKL_I16_WIDTH);
    fputs("IuCAuBB\n", stderr);
    fprintf(stderr,
            "[%s: %d] ux= %" FKL_PRT64U ", uy= %" FKL_PRT64U
            ", arg.ux= %" FKL_PRT64U ", arg.uy= %" FKL_PRT64U "\n",
            __FILE__, __LINE__, ux, uy, arg.ux, arg.uy);
    FKL_ASSERT(arg.ux == ux && arg.uy == uy);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr,
            "[%s: %d] ux= %" FKL_PRT64U ", uy= %" FKL_PRT64U
            ", arg.ux= %" FKL_PRT64U ", arg.uy= %" FKL_PRT64U "\n",
            __FILE__, __LINE__, ux, uy, arg.ux, arg.uy);
    FKL_ASSERT(arg.ux == ux && arg.uy == uy);
    fputc('\n', stderr);

    // IuCAuBCC
    ux = 0x12345678;
    uy = 0x9ABCDEF012345678;
    ins[0].op = FKL_OP_PUSH_PROC_XXX;
    set_ins_uc(&ins[0], ux & I64_L24_MASK);
    ins[1].op = FKL_OP_EXTRA_ARG;
    ins[1].au = ux >> FKL_I24_WIDTH;
    ins[1].bu = uy & I32_L16_MASK;

    ins[2].op = FKL_OP_EXTRA_ARG;
    set_ins_uc(&ins[2], (uy >> FKL_I16_WIDTH) & I64_L24_MASK);

    ins[3].op = FKL_OP_EXTRA_ARG;
    set_ins_uc(&ins[3], (uy >> (FKL_I16_WIDTH + FKL_I24_WIDTH)) & I64_L24_MASK);

    arg.ux = FKL_GET_INS_UC(ins) | (((uint32_t)ins[1].au) << FKL_I24_WIDTH);
    arg.uy =
        ins[1].bu | (((uint64_t)ins[2].au) << FKL_I16_WIDTH)
        | (((uint64_t)ins[2].bu) << FKL_I24_WIDTH)
        | (((uint64_t)ins[3].au) << (FKL_I16_WIDTH * 2 + FKL_BYTE_WIDTH))
        | (((uint64_t)ins[3].bu) << (FKL_I16_WIDTH * 2 + FKL_BYTE_WIDTH * 2));

    fputs("IuCAuBCC\n", stderr);
    fprintf(stderr,
            "[%s: %d] ux= %" FKL_PRT64U ", uy= %" FKL_PRT64U
            ", arg.ux= %" FKL_PRT64U ", arg.uy= %" FKL_PRT64U "\n",
            __FILE__, __LINE__, ux, uy, arg.ux, arg.uy);
    FKL_ASSERT(arg.ux == ux && arg.uy == uy);
    fklGetInsOpArg(ins, &arg);
    fprintf(stderr,
            "[%s: %d] ux= %" FKL_PRT64U ", uy= %" FKL_PRT64U
            ", arg.ux= %" FKL_PRT64U ", arg.uy= %" FKL_PRT64U "\n",
            __FILE__, __LINE__, ux, uy, arg.ux, arg.uy);
    FKL_ASSERT(arg.ux == ux && arg.uy == uy);
    fputc('\n', stderr);
}
