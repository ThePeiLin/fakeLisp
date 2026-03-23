#ifndef FKL_OPCODE_H
#define FKL_OPCODE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
#define FKL_SUBOP_BOX_UNBOX       (1)
#define FKL_SUBOP_BOX_SET         (2)

#define FKL_SUBOP_HASH_REF_2      (1)
#define FKL_SUBOP_HASH_REF_3      (2)
#define FKL_SUBOP_HASH_SET        (3)

#define FKL_SUBOP_VEC_LAST        (-1)
#define FKL_SUBOP_VEC_FIRST       (0)
#define FKL_SUBOP_VEC_REF         (1)
#define FKL_SUBOP_VEC_SET         (2)

#define FKL_SUBOP_STR_REF         (0)
#define FKL_SUBOP_STR_SET         (1)

#define FKL_SUBOP_BVEC_REF        (0)
#define FKL_SUBOP_BVEC_SET        (1)

#define FKL_SUBOP_PAIR_CAR        (0)
#define FKL_SUBOP_PAIR_CDR        (1)
#define FKL_SUBOP_PAIR_NTH        (2)
#define FKL_SUBOP_PAIR_CAR_SET    (4)
#define FKL_SUBOP_PAIR_CDR_SET    (5)
#define FKL_SUBOP_PAIR_APPEND     (6)
#define FKL_SUBOP_PAIR_UNPACK     (7)

#define FKL_SUBOP_DROP_1          (0)
#define FKL_SUBOP_DROP_ALL        (1)

#define FKL_OPCODE_X                                                      \
    X(FKL_OP_DUMMY = 0,        "dummy",            FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_NIL,         "push-nil",         FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_PAIR,        "push-pair",        FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_0,           "push-0",           FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_1,           "push-1",           FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_I8,          "push-i8",          FKL_OP_MODE_IsA,      FKL_OP_PUSH_I16       )\
    X(FKL_OP_PUSH_I16,         "push-i16",         FKL_OP_MODE_IsB,      FKL_OP_PUSH_I24       )\
    X(FKL_OP_PUSH_I24,         "push-i24",         FKL_OP_MODE_IsC,      FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_CHAR,        "push-char",        FKL_OP_MODE_IuB,      FKL_OP_DUMMY          )\
	X(FKL_OP_PUSH_CONST,       "push-const",       FKL_OP_MODE_IuC,      FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_BOX,         "push-box",         FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_LIST_0,      "push-list-0",      FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_VEC_0,       "push-vec-0",       FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_HASHEQ_0,    "push-hasheq-0",    FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_HASHEQV_0,   "push-hasheqv-0",   FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_PUSH_HASHEQUAL_0, "push-hashequal-0", FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_LOAD_PROTO,       "load-proto",       FKL_OP_MODE_IuC,      FKL_OP_DUMMY          )\
    X(FKL_OP_MAKE_PROC,        "make-proc",        FKL_OP_MODE_IuC,      FKL_OP_DUMMY          )\
    X(FKL_OP_DUP,              "dup",              FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_DROP,             "drop",             FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_CHECK_ARG,        "check-arg",        FKL_OP_MODE_IsAuB,    FKL_OP_DUMMY          )\
    X(FKL_OP_SET_BP,           "set-bp",           FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_CALL,             "call",             FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_TAIL_CALL,        "tail-call",        FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_RET_IF_TRUE,      "ret-if-true",      FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_RET_IF_FALSE,     "ret-if-false",     FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_RET,              "ret",              FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_JMP_IF_TRUE,      "jmp-if-true",      FKL_OP_MODE_IsB,      FKL_OP_JMP_IF_TRUE_C  )\
    X(FKL_OP_JMP_IF_TRUE_C,    "jmp-if-true-c",    FKL_OP_MODE_IsC,      FKL_OP_JMP_IF_TRUE_X  )\
    X(FKL_OP_JMP_IF_TRUE_X,    "jmp-if-true-x",    FKL_OP_MODE_IsBB,     FKL_OP_JMP_IF_TRUE_XX )\
    X(FKL_OP_JMP_IF_TRUE_XX,   "jmp-if-true-xx",   FKL_OP_MODE_IsCCB,    FKL_OP_DUMMY          )\
    X(FKL_OP_JMP_IF_FALSE,     "jmp-if-false",     FKL_OP_MODE_IsB,      FKL_OP_JMP_IF_FALSE_C )\
    X(FKL_OP_JMP_IF_FALSE_C,   "jmp-if-false-c",   FKL_OP_MODE_IsC,      FKL_OP_JMP_IF_FALSE_X )\
    X(FKL_OP_JMP_IF_FALSE_X,   "jmp-if-false-x",   FKL_OP_MODE_IsBB,     FKL_OP_JMP_IF_FALSE_XX)\
    X(FKL_OP_JMP_IF_FALSE_XX,  "jmp-if-false-xx",  FKL_OP_MODE_IsCCB,    FKL_OP_DUMMY          )\
    X(FKL_OP_JMP,              "jmp",              FKL_OP_MODE_IsB,      FKL_OP_JMP_C          )\
    X(FKL_OP_JMP_C,            "jmp-c",            FKL_OP_MODE_IsC,      FKL_OP_JMP_X          )\
    X(FKL_OP_JMP_X,            "jmp-x",            FKL_OP_MODE_IsBB,     FKL_OP_JMP_XX         )\
    X(FKL_OP_JMP_XX,           "jmp-xx",           FKL_OP_MODE_IsCCB,    FKL_OP_DUMMY          )\
    X(FKL_OP_IMPORT,           "import",           FKL_OP_MODE_IuC,      FKL_OP_DUMMY          )\
    X(FKL_OP_GET_LOC,          "get-loc",          FKL_OP_MODE_IuC,      FKL_OP_DUMMY          )\
    X(FKL_OP_PUT_LOC,          "put-loc",          FKL_OP_MODE_IuC,      FKL_OP_DUMMY          )\
    X(FKL_OP_GET_VAR_REF,      "get-var-ref",      FKL_OP_MODE_IuC,      FKL_OP_DUMMY          )\
    X(FKL_OP_PUT_VAR_REF,      "put-var-ref",      FKL_OP_MODE_IuC,      FKL_OP_DUMMY          )\
    X(FKL_OP_EXPORT,           "export",           FKL_OP_MODE_IuC,      FKL_OP_DUMMY          )\
    X(FKL_OP_LOAD_LIB,         "load-lib",         FKL_OP_MODE_IuC,      FKL_OP_DUMMY          )\
    X(FKL_OP_ATOM,             "atom",             FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_TRUE,             "true",             FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_NOT,              "not",              FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_EQ,               "eq",               FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_EQV,              "eqv",              FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_EQUAL,            "equal",            FKL_OP_MODE_I,        FKL_OP_DUMMY          )\
    X(FKL_OP_EQN,              "eqn",              FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_GT,               "gt",               FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_LT,               "lt",               FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_GE,               "ge",               FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_LE,               "le",               FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_ADDK,             "addk",             FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_ADDK_LOC,         "addk-loc",         FKL_OP_MODE_IsAuB,    FKL_OP_DUMMY          )\
    X(FKL_OP_ADD,              "add",              FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_SUB,              "sub",              FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_MUL,              "mul",              FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_DIV,              "div",              FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_IDIV,             "idiv",             FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_PAIR,             "pair",             FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_VEC,              "vec",              FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_STR,              "str",              FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_BVEC,             "bvec",             FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_BOX,              "box",              FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_HASH,             "hash",             FKL_OP_MODE_IsA,      FKL_OP_DUMMY          )\
    X(FKL_OP_CLOSE_REF,        "close-ref",        FKL_OP_MODE_IuC,      FKL_OP_DUMMY          )\
    X(FKL_OP_POP_LOC,          "pop-loc",          FKL_OP_MODE_IuB,      FKL_OP_DUMMY          )\
    X(FKL_OP_MOV_LOC,          "mov-loc",          FKL_OP_MODE_IuAuB,    FKL_OP_DUMMY          )\
    X(FKL_OP_MOV_VAR_REF,      "mov-var-ref",      FKL_OP_MODE_IuAuB,    FKL_OP_DUMMY          )\
    X(FKL_OP_EXTRA_ARG,        "extra-arg",        FKL_OP_MODE_IxAxB,    FKL_OP_DUMMY          )
// clang-format on

typedef enum {
#define X(A, B, C, D) A,
    FKL_OPCODE_X
#undef X
} FklOpcode;

#define FKL_OPCODE_NUM (FKL_OP_EXTRA_ARG + 1)

#define FKL_MAX_OPCODE_NAME_LEN (sizeof("push-hashequal-0"))

// Op(8)  |  A(8)  |  B(16)
// Op(8)  |      C(24)

#define FKL_MAX_INS_LEN (4)
typedef enum {
    FKL_OP_MODE_I = 0,
    FKL_OP_MODE_IsA,
    FKL_OP_MODE_IuB,
    FKL_OP_MODE_IsB,
    FKL_OP_MODE_IuC,
    FKL_OP_MODE_IsC,
    FKL_OP_MODE_IuBB,
    FKL_OP_MODE_IsBB,
    FKL_OP_MODE_IuCCB,
    FKL_OP_MODE_IsCCB,

    FKL_OP_MODE_IsAuB,

    FKL_OP_MODE_IuAuB,
    FKL_OP_MODE_IuCuC,
    FKL_OP_MODE_IuCAuBB,

    FKL_OP_MODE_IxAxB,
} FklOpcodeMode;

const char *fklGetOpcodeName(FklOpcode);
FklOpcodeMode fklGetOpcodeMode(FklOpcode);
FklOpcode fklGetOpcodeNext(FklOpcode opcode);

FklOpcode fklFindOpcode(const char *);
int fklGetOpcodeModeLen(FklOpcode);
const char *fklGetSubOpcodeName(FklOpcode op, int8_t subop);

#ifdef __cplusplus
}
#endif

#endif
