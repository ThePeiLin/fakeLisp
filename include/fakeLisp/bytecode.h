#ifndef FKL_BYTECODE_H
#define FKL_BYTECODE_H

#include "common.h"
#include "opcode.h"
#include "symbol.h"
#include "vm_fwd.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t FklIns;

#define FKL_INS_MSK(S) (FKL_MASK1(FklIns, S, 0))

static_assert(sizeof(FklIns) == sizeof(uint32_t),
        "invalid instruction definition");

#define FKL_INS_STATIC_INIT ((FklIns)(0))

#define FKL_INS_OP_SIZE ((sizeof(uint8_t)) * FKL_BYTE_WIDTH)
#define FKL_INS_OP_POS (0)

#define FKL_INS_A_POS (FKL_INS_OP_SIZE)
#define FKL_INS_A_SIZE ((sizeof(uint8_t)) * FKL_BYTE_WIDTH)

#define FKL_INS_B_POS (FKL_INS_A_SIZE + FKL_INS_OP_SIZE)
#define FKL_INS_B_SIZE ((sizeof(uint16_t)) * FKL_BYTE_WIDTH)

#define FKL_INS_C_POS (FKL_INS_OP_SIZE)
#define FKL_INS_C_SIZE (FKL_INS_B_SIZE + FKL_INS_A_SIZE)

static_assert(FKL_INS_C_SIZE == FKL_I24_WIDTH, "what the fuck?");

#define FKL_INS_GET(type, i, pos, size)                                        \
    ((type)((((FklIns)(i)) >> (pos)) & FKL_INS_MSK(size)))

#define FKL_INS_SET(type, i, v, pos, size)                                     \
    (((i) & FKL_MASK0(FklIns, size, pos))                                      \
            | ((((FklIns)(type)(v)) & FKL_INS_MSK(size)) << (pos)))

#define FKL_INS_OP(I)                                                          \
    (FKL_INS_GET(FklOpcode, (I), FKL_INS_OP_POS, FKL_INS_OP_SIZE))
#define FKL_INS_SET_OP(I, OP)                                                  \
    FKL_INS_SET(FklOpcode, I, OP, FKL_INS_OP_POS, FKL_INS_OP_SIZE)

#define FKL_INS_sA(I) (FKL_INS_GET(int8_t, (I), FKL_INS_A_POS, FKL_INS_A_SIZE))
#define FKL_INS_uA(I) (FKL_INS_GET(uint8_t, (I), FKL_INS_A_POS, FKL_INS_A_SIZE))
#define FKL_INS_SET_uA(I, A)                                                   \
    (FKL_INS_SET(uint8_t, (I), (A), FKL_INS_A_POS, FKL_INS_A_SIZE))
#define FKL_INS_SET_sA(I, A) (FKL_INS_SET_uA(I, A))

#define FKL_INS_sB(I) (FKL_INS_GET(int16_t, (I), FKL_INS_B_POS, FKL_INS_B_SIZE))
#define FKL_INS_uB(I)                                                          \
    (FKL_INS_GET(uint16_t, (I), FKL_INS_B_POS, FKL_INS_B_SIZE))
#define FKL_INS_SET_uB(I, B)                                                   \
    (FKL_INS_SET(uint16_t, (I), (B), FKL_INS_B_POS, FKL_INS_B_SIZE))
#define FKL_INS_SET_sB(I, B) (FKL_INS_SET_uB(I, B))

#define FKL_INS_sC(I)                                                          \
    (FKL_INS_GET(int32_t, (I), FKL_INS_C_POS, FKL_INS_C_SIZE) - FKL_I24_OFFSET)
#define FKL_INS_uC(I)                                                          \
    (FKL_INS_GET(uint32_t, (I), FKL_INS_C_POS, FKL_INS_C_SIZE))
#define FKL_INS_SET_sC(I, sC)                                                  \
    (FKL_INS_SET(uint32_t,                                                     \
            (I),                                                               \
            ((uint32_t)(int32_t)(sC)) + FKL_I24_OFFSET,                        \
            FKL_INS_C_POS,                                                     \
            FKL_INS_C_SIZE))
#define FKL_INS_SET_uC(I, uC)                                                  \
    (FKL_INS_SET(uint32_t,                                                     \
            (I),                                                               \
            ((uint32_t)(uC)),                                                  \
            FKL_INS_C_POS,                                                     \
            FKL_INS_C_SIZE))

#define FKL_MAKE_INS_I(OP) (FKL_INS_SET_OP(FKL_INS_STATIC_INIT, OP))
#define FKL_MAKE_INS_IsA(OP, sA) (FKL_INS_SET_sA(FKL_MAKE_INS_I(OP), sA))
#define FKL_MAKE_INS_IuA(OP, uA) (FKL_INS_SET_uA(FKL_MAKE_INS_I(OP), uA))

#define FKL_MAKE_INS_IuC(OP, uC) (FKL_INS_SET_uC(FKL_MAKE_INS_I(OP), uC))
#define FKL_MAKE_INS_IsC(OP, sC) (FKL_INS_SET_sC(FKL_MAKE_INS_I(OP), sC))

#define FKL_MAKE_INS_IuB(OP, uB) (FKL_INS_SET_uB(FKL_MAKE_INS_I(OP), uB))
#define FKL_MAKE_INS_IsB(OP, sB) (FKL_INS_SET_uB(FKL_MAKE_INS_I(OP), sB))

typedef struct {
    uint64_t len;
    FklIns *code;
} FklByteCode;

typedef struct {
    FklVMvalue *fid;
    uint64_t scp;
    uint32_t line;
    uint32_t scope;
} FklLntItem;

typedef struct {
    FklLntItem *l;
    FklByteCode bc;
    uint32_t ls;
} FklByteCodelnt;

// FklByteCodelntVector
#define FKL_VECTOR_ELM_TYPE FklByteCodelnt *
#define FKL_VECTOR_ELM_TYPE_NAME ByteCodelnt
#include "cont/vector.h"

void fklInitByteCode(FklByteCode *, size_t len);
FklByteCode *fklCreateByteCode(size_t);
void fklByteCodeRealloc(FklByteCode *b, size_t len);

void fklCodeConcat(FklByteCode *, const FklByteCode *);
void fklCodeReverseConcat(const FklByteCode *, FklByteCode *);

void fklMoveByteCode(FklByteCode *to, FklByteCode *from);
void fklSetByteCode(FklByteCode *to, const FklByteCode *from);
FklByteCode *fklCopyByteCode(const FklByteCode *);
FklByteCodelnt *fklCopyByteCodelnt(const FklByteCodelnt *);
void fklSetByteCodelnt(FklByteCodelnt *, const FklByteCodelnt *);
void fklMoveByteCodelnt(FklByteCodelnt *to, FklByteCodelnt *from);

void fklUninitByteCode(FklByteCode *);
void fklDestroyByteCode(FklByteCode *);

void fklInitByteCodelnt(FklByteCodelnt *t, size_t len);
FklByteCodelnt *fklCreateByteCodelnt(size_t len);

FklByteCodelnt *fklCreateSingleInsBclnt(FklIns ins,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope);

void fklInitSingleInsBcl(FklByteCodelnt *bcl,
        FklIns ins,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope);

void fklUninitByteCodelnt(FklByteCodelnt *);
void fklDestroyByteCodelnt(FklByteCodelnt *);
void fklIncreaseScpOfByteCodelnt(FklByteCodelnt *, uint64_t);
void fklCodeLntConcat(FklByteCodelnt *, const FklByteCodelnt *);
void fklCodeLntReverseConcat(const FklByteCodelnt *, FklByteCodelnt *);

void fklByteCodeLntPushBackIns(FklByteCodelnt *bcl,
        const FklIns ins,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope);

void fklByteCodeLntInsertFrontIns(const FklIns ins,
        FklByteCodelnt *bcl,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope);

void fklByteCodePushBack(FklByteCode *bc, FklIns ins);
void fklByteCodeInsertFront(FklIns, FklByteCode *bc);

void fklByteCodeLntInsertInsAt(FklByteCodelnt *bcl, FklIns ins, uint64_t idx);
FklIns fklByteCodeLntRemoveInsAt(FklByteCodelnt *bcl, uint64_t idx);

void fklInitLineNumTabNode(FklLntItem *,
        FklVMvalue *fid,
        uint64_t scp,
        uint32_t line,
        uint32_t scope);

const FklLntItem *fklFindLntItem(uint64_t cp, size_t ls, const FklLntItem *l);

static FKL_ALWAYS_INLINE const FklLntItem *
fklGetLntItem(const FklByteCodelnt *code, const FklIns *cp) {
    return fklFindLntItem(cp - code->bc.code, code->ls, code->l);
}

typedef struct {
    int64_t ix;
    uint64_t ux;
    uint64_t uy;
} FklInsArg;

int fklGetInsOpArg(const FklIns *ins, FklInsArg *arg);
int fklGetInsOpArgWithOp(FklOpcode op, const FklIns *ins, FklInsArg *arg);

int fklGetNextIns(const FklIns *cur_ins, const FklIns *ins[2]);

FKL_NODISCARD
int fklMakeIns(FklIns *ins, FklOpcode op, const FklInsArg *);

#define FKL_MAKE_INS(INS, OP, ...)                                             \
    (fklMakeIns((INS), (OP), &(const FklInsArg){ __VA_ARGS__ }));

static FKL_ALWAYS_INLINE int fklIsLoadProto(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_LOAD_PROTO;
}

static inline int fklIsJmpIns(const FklIns ins) {
    return FKL_INS_OP(ins) >= FKL_OP_JMP && FKL_INS_OP(ins) <= FKL_OP_JMP_XX;
}

static inline int fklIsCondJmpIns(const FklIns ins) {
    return FKL_INS_OP(ins) >= FKL_OP_JMP_IF_TRUE
        && FKL_INS_OP(ins) <= FKL_OP_JMP_IF_FALSE_XX;
}

static inline int fklIsJmpIfTrueIns(const FklIns ins) {
    return FKL_INS_OP(ins) >= FKL_OP_JMP_IF_TRUE
        && FKL_INS_OP(ins) <= FKL_OP_JMP_IF_TRUE_XX;
}

static inline int fklIsJmpIfFalseIns(const FklIns ins) {
    return FKL_INS_OP(ins) >= FKL_OP_JMP_IF_FALSE
        && FKL_INS_OP(ins) <= FKL_OP_JMP_IF_FALSE_XX;
}

static inline int fklIsPutLocIns(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_PUT_LOC_C;
}

static inline int fklIsMakeProcIns(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_MAKE_PROC;
}

static inline int fklIsPutVarRefIns(const FklIns ins) {
    return FKL_INS_OP(ins) >= FKL_OP_PUT_VAR_REF
        && FKL_INS_OP(ins) <= FKL_OP_PUT_VAR_REF_X;
}

static inline int fklIsCallIns(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_CALL
        || FKL_INS_OP(ins) == FKL_OP_TAIL_CALL;
}

static inline int fklIsRetIns(const FklIns ins) {
    return FKL_INS_OP(ins) >= FKL_OP_RET_IF_TRUE
        && FKL_INS_OP(ins) <= FKL_OP_RET;
}

static inline int fklIsLoadLibIns(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_LOAD_LIB;
}

void fklScanAndSetTailCall(FklByteCode *bc);

#define FKL_INS_uX(ins)                                                        \
    (FKL_INS_uB(*(ins)) | (((uint32_t)FKL_INS_uB((ins)[1])) << FKL_I16_WIDTH))
#define FKL_INS_sX(ins) ((int32_t)FKL_INS_uX(ins))
#define FKL_INS_uXX(ins)                                                       \
    (FKL_INS_uC(*(ins)) | (((uint64_t)FKL_INS_uC((ins)[1])) << FKL_I24_WIDTH)  \
            | (((uint64_t)FKL_INS_uB(ins[2])) << (FKL_I24_WIDTH * 2)))

#define FKL_INS_sXX(ins) ((int64_t)FKL_INS_uXX(ins))

#ifdef __cplusplus
}
#endif

#endif
