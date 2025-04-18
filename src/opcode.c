#include <fakeLisp/opcode.h>

#include <string.h>

static struct {
    char *name;
    FklOpcodeMode mode;
} opcodeInfo[FKL_OPCODE_NUM] = {
#define X(A, B, C) {B, C},
    FKL_OPCODE_X
#undef X
};

const char *fklGetOpcodeName(FklOpcode opcode) {
    return opcodeInfo[opcode].name;
}

FklOpcodeMode fklGetOpcodeMode(FklOpcode opcode) {
    return opcodeInfo[opcode].mode;
}

FklOpcode fklFindOpcode(const char *str) {
    for (FklOpcode i = 0; i < FKL_OPCODE_NUM; i++)
        if (!strcmp(opcodeInfo[i].name, str))
            return i;
    return 0;
}

int fklGetOpcodeModeLen(FklOpcode op) {
    FklOpcodeMode mode = opcodeInfo[op].mode;
    switch (mode) {
    case FKL_OP_MODE_I:
    case FKL_OP_MODE_IsA:
    case FKL_OP_MODE_IuB:
    case FKL_OP_MODE_IsB:
    case FKL_OP_MODE_IuC:
    case FKL_OP_MODE_IsC:
    case FKL_OP_MODE_IsAuB:
    case FKL_OP_MODE_IuAuB:
    case FKL_OP_MODE_IxAxB:
        return 1;
        break;

    case FKL_OP_MODE_IuBB:
    case FKL_OP_MODE_IsBB:
    case FKL_OP_MODE_IuCuC:
        return 2;
        break;

    case FKL_OP_MODE_IuCCB:
    case FKL_OP_MODE_IsCCB:
    case FKL_OP_MODE_IuCAuBB:
        return 3;
        break;

    case FKL_OP_MODE_IuCAuBCC:
        return 4;
        break;
    }
    return 0;
}
