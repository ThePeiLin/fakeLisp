#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    char *name;
    FklOpcodeMode mode;
} opcodeInfo[FKL_OPCODE_NUM] = {
#define X(A, B, C) { B, C },
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

static inline const char *get_box_subop(int8_t subop) {
    switch (subop) {
    case FKL_SUBOP_BOX_UNBOX:
        return "unbox";
        break;
    case FKL_SUBOP_BOX_SET:
        return "set";
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    return NULL;
}

static inline const char *get_drop_subop(int8_t subop) {
    switch (subop) {
    case FKL_SUBOP_DROP_1:
        return "1";
        break;
    case FKL_SUBOP_DROP_ALL:
        return "all";
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    return NULL;
}

static inline const char *get_pair_subop(int8_t subop) {
    switch (subop) {
    case FKL_SUBOP_PAIR_CAR:
        return "car";
        break;
    case FKL_SUBOP_PAIR_CDR:
        return "cdr";
        break;
    case FKL_SUBOP_PAIR_NTH:
        return "nth";
        break;
    case FKL_SUBOP_PAIR_CAR_SET:
        return "car-set";
        break;
    case FKL_SUBOP_PAIR_CDR_SET:
        return "cdr-set";
        break;
    case FKL_SUBOP_PAIR_APPEND:
        return "append";
        break;
    case FKL_SUBOP_PAIR_UNPACK:
        return "unpack";
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    return NULL;
}

static inline const char *get_vec_subop(int8_t subop) {
    switch (subop) {
    case FKL_SUBOP_VEC_LAST:
        return "last";
        break;
    case FKL_SUBOP_VEC_FIRST:
        return "first";
        break;
    case FKL_SUBOP_VEC_REF:
        return "ref";
        break;
    case FKL_SUBOP_VEC_SET:
        return "set";
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    return NULL;
}

static inline const char *get_str_subop(int8_t subop) {
    switch (subop) {
    case FKL_SUBOP_STR_REF:
        return "ref";
        break;
    case FKL_SUBOP_STR_SET:
        return "set";
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    return NULL;
}

static inline const char *get_bvec_subop(int8_t subop) {
    switch (subop) {
    case FKL_SUBOP_BVEC_REF:
        return "ref";
        break;
    case FKL_SUBOP_BVEC_SET:
        return "set";
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    return NULL;
}

static inline const char *get_hash_subop(int8_t subop) {
    switch (subop) {
    case FKL_SUBOP_HASH_REF_2:
        return "ref2";
        break;
    case FKL_SUBOP_HASH_REF_3:
        return "ref3";
        break;
    case FKL_SUBOP_HASH_SET:
        return "set";
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    return NULL;
}

const char *fklGetSubOpcodeName(FklOpcode op, int8_t subop) {
    switch (op) {
    case FKL_OP_DROP:
        return get_drop_subop(subop);
        break;
    case FKL_OP_PAIR:
        return get_pair_subop(subop);
        break;
    case FKL_OP_VEC:
        return get_vec_subop(subop);
        break;
    case FKL_OP_STR:
        return get_str_subop(subop);
        break;
    case FKL_OP_BVEC:
        return get_bvec_subop(subop);
        break;
    case FKL_OP_BOX:
        return get_box_subop(subop);
        break;
    case FKL_OP_HASH:
        return get_hash_subop(subop);
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    return NULL;
}
