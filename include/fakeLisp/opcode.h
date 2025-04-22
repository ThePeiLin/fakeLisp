#ifndef FKL_OPCODE_H
#define FKL_OPCODE_H

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off
#define FKL_SUBOP_BOX_UNBOX       (-1)
#define FKL_SUBOP_BOX_NEW_NIL_BOX (0)
#define FKL_SUBOP_BOX_NEW_BOX     (1)
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
#define FKL_SUBOP_PAIR_CONS       (3)
#define FKL_SUBOP_PAIR_CAR_SET    (4)
#define FKL_SUBOP_PAIR_CDR_SET    (5)

#define FKL_SUBOP_DROP_1          (0)
#define FKL_SUBOP_DROP_ALL        (1)

#define FKL_OPCODE_X                                                      \
    X(FKL_OP_DUMMY = 0,         "dummy",             FKL_OP_MODE_I       )\
    X(FKL_OP_PUSH_NIL,          "push-nil",          FKL_OP_MODE_I       )\
    X(FKL_OP_PUSH_PAIR,         "push-pair",         FKL_OP_MODE_I       )\
    X(FKL_OP_PUSH_0,            "push-0",            FKL_OP_MODE_I       )\
    X(FKL_OP_PUSH_1,            "push-1",            FKL_OP_MODE_I       )\
    X(FKL_OP_PUSH_I8,           "push-i8",           FKL_OP_MODE_IsA     )\
    X(FKL_OP_PUSH_I16,          "push-i16",          FKL_OP_MODE_IsB     )\
    X(FKL_OP_PUSH_I24,          "push-i24",          FKL_OP_MODE_IsC     )\
    X(FKL_OP_PUSH_I64F,         "push-i64f",         FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_I64F_C,       "push-i64f-c",       FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_I64F_X,       "push-i64f-x",       FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUSH_CHAR,         "push-char",         FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_F64,          "push-f64",          FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_F64_C,        "push-f64-c",        FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_F64_X,        "push-f64-x",        FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUSH_STR,          "push-str",          FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_STR_C,        "push-str-c",        FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_STR_X,        "push-str-x",        FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUSH_SYM,          "push-sym",          FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_SYM_C,        "push-sym-c",        FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_SYM_X,        "push-sym-x",        FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUSH_PROC,         "push-proc",         FKL_OP_MODE_IuAuB   )\
    X(FKL_OP_PUSH_PROC_X,       "push-proc-x",       FKL_OP_MODE_IuCuC   )\
    X(FKL_OP_PUSH_PROC_XX,      "push-proc-xx",      FKL_OP_MODE_IuCAuBB )\
    X(FKL_OP_PUSH_PROC_XXX,     "push-proc-xxx",     FKL_OP_MODE_IuCAuBCC)\
    X(FKL_OP_DUP,               "dup",               FKL_OP_MODE_I       )\
    X(FKL_OP_DROP,              "drop",              FKL_OP_MODE_IsA     )\
    X(FKL_OP_CHECK_ARG,         "check-arg",         FKL_OP_MODE_IsAuB   )\
    X(FKL_OP_SET_BP,            "set-bp",            FKL_OP_MODE_I       )\
    X(FKL_OP_CALL,              "call",              FKL_OP_MODE_I       )\
    X(FKL_OP_TAIL_CALL,         "tail-call",         FKL_OP_MODE_I       )\
    X(FKL_OP_RET_IF_TRUE,       "ret-if-true",       FKL_OP_MODE_I       )\
    X(FKL_OP_RET_IF_FALSE,      "ret-if-false",      FKL_OP_MODE_I       )\
    X(FKL_OP_RET,               "ret",               FKL_OP_MODE_I       )\
    X(FKL_OP_JMP_IF_TRUE,       "jmp-if-true",       FKL_OP_MODE_IsB     )\
    X(FKL_OP_JMP_IF_TRUE_C,     "jmp-if-true-c",     FKL_OP_MODE_IsC     )\
    X(FKL_OP_JMP_IF_TRUE_X,     "jmp-if-true-x",     FKL_OP_MODE_IsBB    )\
    X(FKL_OP_JMP_IF_TRUE_XX,    "jmp-if-true-xx",    FKL_OP_MODE_IsCCB   )\
    X(FKL_OP_JMP_IF_FALSE,      "jmp-if-false",      FKL_OP_MODE_IsB     )\
    X(FKL_OP_JMP_IF_FALSE_C,    "jmp-if-false-c",    FKL_OP_MODE_IsC     )\
    X(FKL_OP_JMP_IF_FALSE_X,    "jmp-if-false-x",    FKL_OP_MODE_IsBB    )\
    X(FKL_OP_JMP_IF_FALSE_XX,   "jmp-if-false-xx",   FKL_OP_MODE_IsCCB   )\
    X(FKL_OP_JMP,               "jmp",               FKL_OP_MODE_IsB     )\
    X(FKL_OP_JMP_C,             "jmp-c",             FKL_OP_MODE_IsC     )\
    X(FKL_OP_JMP_X,             "jmp-x",             FKL_OP_MODE_IsBB    )\
    X(FKL_OP_JMP_XX,            "jmp-xx",            FKL_OP_MODE_IsCCB   )\
    X(FKL_OP_LIST_APPEND,       "list-append",       FKL_OP_MODE_I       )\
    X(FKL_OP_PUSH_VEC,          "push-vec",          FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_VEC_C,        "push-vec-c",        FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_VEC_X,        "push-vec-x",        FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUSH_VEC_XX,       "push-vec-xx",       FKL_OP_MODE_IuCCB   )\
    X(FKL_OP_PUSH_BI,           "push-bi",           FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_BI_C,         "push-bi-c",         FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_BI_X,         "push-bi-x",         FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUSH_BVEC,         "push-bvec",         FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_BVEC_C,       "push-bvec-c",       FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_BVEC_X,       "push-bvec-x",       FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUSH_HASHEQ,       "push-hasheq",       FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_HASHEQ_C,     "push-hasheq-c",     FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_HASHEQ_X,     "push-hasheq-x",     FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUSH_HASHEQ_XX,    "push-hasheq-xx",    FKL_OP_MODE_IuCCB   )\
    X(FKL_OP_PUSH_HASHEQV,      "push-hasheqv",      FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_HASHEQV_C,    "push-hasheqv-c",    FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_HASHEQV_X,    "push-hasheqv-x",    FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUSH_HASHEQV_XX,   "push-hasheqv-xx",   FKL_OP_MODE_IuCCB   )\
    X(FKL_OP_PUSH_HASHEQUAL,    "push-hashequal",    FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_HASHEQUAL_C,  "push-hashequal-c",  FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_HASHEQUAL_X,  "push-hashequal-x",  FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUSH_HASHEQUAL_XX, "push-hashequal-xx", FKL_OP_MODE_IuCCB   )\
    X(FKL_OP_PUSH_LIST_0,       "push-list-0",       FKL_OP_MODE_I       )\
    X(FKL_OP_PUSH_LIST,         "push-list",         FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_LIST_C,       "push-list-c",       FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_LIST_X,       "push-list-x",       FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUSH_LIST_XX,      "push-list-xx",      FKL_OP_MODE_IuCCB   )\
    X(FKL_OP_PUSH_VEC_0,        "push-vec-0",        FKL_OP_MODE_I       )\
    X(FKL_OP_LIST_PUSH,         "list-push",         FKL_OP_MODE_I       )\
    X(FKL_OP_IMPORT,            "import",            FKL_OP_MODE_IuAuB   )\
    X(FKL_OP_IMPORT_X,          "import-x",          FKL_OP_MODE_IuCuC   )\
    X(FKL_OP_IMPORT_XX,         "import-xx",         FKL_OP_MODE_IuCAuBB )\
    X(FKL_OP_PUSH_I64B,         "push-i64b",         FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUSH_I64B_C,       "push-i64b-c",       FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUSH_I64B_X,       "push-i64b-x",       FKL_OP_MODE_IuBB    )\
    X(FKL_OP_GET_LOC,           "get-loc",           FKL_OP_MODE_IuB     )\
    X(FKL_OP_GET_LOC_C,         "get-loc-c",         FKL_OP_MODE_IuC     )\
    X(FKL_OP_GET_LOC_X,         "get-loc-x",         FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUT_LOC,           "put-loc",           FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUT_LOC_C,         "put-loc-c",         FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUT_LOC_X,         "put-loc-x",         FKL_OP_MODE_IuBB    )\
    X(FKL_OP_GET_VAR_REF,       "get-var-ref",       FKL_OP_MODE_IuB     )\
    X(FKL_OP_GET_VAR_REF_C,     "get-var-ref-c",     FKL_OP_MODE_IuC     )\
    X(FKL_OP_GET_VAR_REF_X,     "get-var-ref-x",     FKL_OP_MODE_IuBB    )\
    X(FKL_OP_PUT_VAR_REF,       "put-var-ref",       FKL_OP_MODE_IuB     )\
    X(FKL_OP_PUT_VAR_REF_C,     "put-var-ref-c",     FKL_OP_MODE_IuC     )\
    X(FKL_OP_PUT_VAR_REF_X,     "put-var-ref-x",     FKL_OP_MODE_IuBB    )\
    X(FKL_OP_EXPORT,            "export",            FKL_OP_MODE_IuB     )\
    X(FKL_OP_EXPORT_C,          "export-c",          FKL_OP_MODE_IuC     )\
    X(FKL_OP_EXPORT_X,          "export-x",          FKL_OP_MODE_IuBB    )\
    X(FKL_OP_LOAD_LIB,          "load-lib",          FKL_OP_MODE_IuB     )\
    X(FKL_OP_LOAD_LIB_C,        "load-lib-c",        FKL_OP_MODE_IuC     )\
    X(FKL_OP_LOAD_LIB_X,        "load-lib-x",        FKL_OP_MODE_IuBB    )\
    X(FKL_OP_LOAD_DLL,          "load-dll",          FKL_OP_MODE_IuB     )\
    X(FKL_OP_LOAD_DLL_C,        "load-dll-c",        FKL_OP_MODE_IuC     )\
    X(FKL_OP_LOAD_DLL_X,        "load-dll-x",        FKL_OP_MODE_IuBB    )\
    X(FKL_OP_ATOM,              "atom",              FKL_OP_MODE_I       )\
    X(FKL_OP_TRUE,              "true",              FKL_OP_MODE_I       )\
    X(FKL_OP_NOT,               "not",               FKL_OP_MODE_I       )\
    X(FKL_OP_EQ,                "eq",                FKL_OP_MODE_I       )\
    X(FKL_OP_EQV,               "eqv",               FKL_OP_MODE_I       )\
    X(FKL_OP_EQUAL,             "equal",             FKL_OP_MODE_I       )\
    \
    X(FKL_OP_EQN,               "eqn",               FKL_OP_MODE_IsA     )\
    X(FKL_OP_GT,                "gt",                FKL_OP_MODE_IsA     )\
    X(FKL_OP_LT,                "lt",                FKL_OP_MODE_IsA     )\
    X(FKL_OP_GE,                "ge",                FKL_OP_MODE_IsA     )\
    X(FKL_OP_LE,                "le",                FKL_OP_MODE_IsA     )\
    \
    X(FKL_OP_ADDK,              "addk",              FKL_OP_MODE_IsA     )\
    X(FKL_OP_ADDK_LOC,          "addk-loc",          FKL_OP_MODE_IsAuB   )\
    \
    X(FKL_OP_ADD,               "add",               FKL_OP_MODE_IsA     )\
    X(FKL_OP_SUB,               "sub",               FKL_OP_MODE_IsA     )\
    X(FKL_OP_MUL,               "mul",               FKL_OP_MODE_IsA     )\
    X(FKL_OP_DIV,               "div",               FKL_OP_MODE_IsA     )\
    X(FKL_OP_IDIV,              "idiv",              FKL_OP_MODE_IsA     )\
    \
    X(FKL_OP_PAIR,              "pair",              FKL_OP_MODE_IsA     )\
    X(FKL_OP_VEC,               "vec",               FKL_OP_MODE_IsA     )\
    X(FKL_OP_STR,               "str",               FKL_OP_MODE_IsA     )\
    X(FKL_OP_BVEC,              "bvec",              FKL_OP_MODE_IsA     )\
    X(FKL_OP_BOX,               "box",               FKL_OP_MODE_IsA     )\
    X(FKL_OP_HASH,              "hash",              FKL_OP_MODE_IsA     )\
    \
    X(FKL_OP_CLOSE_REF,         "close-ref",         FKL_OP_MODE_IuAuB   )\
    X(FKL_OP_CLOSE_REF_X,       "close-ref-x",       FKL_OP_MODE_IuCuC   )\
    X(FKL_OP_CLOSE_REF_XX,      "close-ref-xx",      FKL_OP_MODE_IuCAuBB )\
    \
    X(FKL_OP_EXPORT_TO,         "export-to",         FKL_OP_MODE_IuAuB   )\
    X(FKL_OP_EXPORT_TO_X,       "export-to-x",       FKL_OP_MODE_IuCuC   )\
    X(FKL_OP_EXPORT_TO_XX,      "export-to-xx",      FKL_OP_MODE_IuCAuBB )\
    \
    X(FKL_OP_POP_LOC,           "pop-loc",           FKL_OP_MODE_IuB     )\
    X(FKL_OP_POP_LOC_C,         "pop-loc-c",         FKL_OP_MODE_IuC     )\
    X(FKL_OP_POP_LOC_X,         "pop-loc-x",         FKL_OP_MODE_IuBB    )\
    \
    X(FKL_OP_MOV_LOC,           "mov-loc",           FKL_OP_MODE_IuAuB   )\
    X(FKL_OP_MOV_VAR_REF,       "mov-var-ref",       FKL_OP_MODE_IuAuB   )\
    \
    X(FKL_OP_EXTRA_ARG,         "extra-arg",         FKL_OP_MODE_IxAxB   )
// clang-format on

typedef enum {
#define X(A, B, C) A,
    FKL_OPCODE_X
#undef X
} FklOpcode;

#define FKL_OPCODE_NUM (FKL_OP_EXTRA_ARG + 1)

#define FKL_MAX_OPCODE_NAME_LEN (sizeof("push-hashequal-xx"))

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
    FKL_OP_MODE_IuCAuBCC,

    FKL_OP_MODE_IxAxB,
} FklOpcodeMode;

#define FKL_GET_INS_UC(ins)                                                    \
    (((((uint32_t)(ins)->bu) << FKL_BYTE_WIDTH)) | (ins)->au)
#define FKL_GET_INS_IC(ins) (((int32_t)FKL_GET_INS_UC(ins)) - FKL_I24_OFFSET)
#define FKL_GET_INS_UX(ins)                                                    \
    ((ins)->bu | (((uint32_t)(ins)[1].bu) << FKL_I16_WIDTH))
#define FKL_GET_INS_IX(ins) ((int32_t)FKL_GET_INS_UX(ins))
#define FKL_GET_INS_UXX(ins)                                                   \
    (FKL_GET_INS_UC(ins)                                                       \
     | (((uint64_t)FKL_GET_INS_UC((ins) + 1)) << FKL_I24_WIDTH)                \
     | (((uint64_t)ins[2].bu) << (FKL_I24_WIDTH * 2)))

#define FKL_GET_INS_IXX(ins) ((int64_t)FKL_GET_INS_UXX(ins))

const char *fklGetOpcodeName(FklOpcode);
FklOpcodeMode fklGetOpcodeMode(FklOpcode);
FklOpcode fklFindOpcode(const char *);
int fklGetOpcodeModeLen(FklOpcode);

#ifdef __cplusplus
}
#endif

#endif
