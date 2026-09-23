#ifndef FKL_BYTECODE_H
#define FKL_BYTECODE_H

#include "common.h"
#include "opcode.h"
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

FKL_API void fklInitByteCode(FklByteCode *, size_t len);
FKL_API FklByteCode *fklCreateByteCode(size_t);
FKL_API void fklByteCodeRealloc(FklByteCode *b, size_t len);

FKL_API void fklCodeConcat(FklByteCode *, const FklByteCode *);
FKL_API void fklCodeReverseConcat(const FklByteCode *, FklByteCode *);

FKL_API void fklMoveByteCode(FklByteCode *to, FklByteCode *from);
FKL_API void fklSetByteCode(FklByteCode *to, const FklByteCode *from);
FKL_API FklByteCode *fklCopyByteCode(const FklByteCode *);
FKL_API FklByteCodelnt *fklCopyByteCodelnt(const FklByteCodelnt *);
FKL_API void fklSetByteCodelnt(FklByteCodelnt *, const FklByteCodelnt *);
FKL_API void fklMoveByteCodelnt(FklByteCodelnt *to, FklByteCodelnt *from);

FKL_API void fklUninitByteCode(FklByteCode *);
FKL_API void fklDestroyByteCode(FklByteCode *);

FKL_API void fklInitByteCodelnt(FklByteCodelnt *t, size_t len);
FKL_API FklByteCodelnt *fklCreateByteCodelnt(size_t len);

FKL_API FklByteCodelnt *fklCreateSingleInsBclnt(FklIns ins,
        FklVMvalue *fid,
        size_t line,
        uint32_t scope);

FKL_API void fklInitSingleInsBcl(FklByteCodelnt *bcl,
        FklIns ins,
        FklVMvalue *fid,
        size_t line,
        uint32_t scope);

FKL_API void fklUninitByteCodelnt(FklByteCodelnt *);
FKL_API void fklDestroyByteCodelnt(FklByteCodelnt *);
FKL_API void fklIncreaseScpOfByteCodelnt(FklByteCodelnt *, uint64_t);
FKL_API void fklCodeLntConcat(FklByteCodelnt *, const FklByteCodelnt *);
FKL_API void fklCodeLntReverseConcat(const FklByteCodelnt *, FklByteCodelnt *);

FKL_API void fklByteCodeLntPushBackIns(FklByteCodelnt *bcl,
        const FklIns ins,
        FklVMvalue *fid,
        size_t line,
        uint32_t scope);

FKL_API void fklByteCodeLntInsertFrontIns(const FklIns ins,
        FklByteCodelnt *bcl,
        FklVMvalue *fid,
        size_t line,
        uint32_t scope);

FKL_API void fklByteCodePushBack(FklByteCode *bc, FklIns ins);
FKL_API void fklByteCodeInsertFront(FklIns, FklByteCode *bc);

FKL_API void
fklByteCodeLntInsertInsAt(FklByteCodelnt *bcl, FklIns ins, uint64_t idx);
FKL_API FklIns fklByteCodeLntRemoveInsAt(FklByteCodelnt *bcl, uint64_t idx);

FKL_API void fklInitLineNumTabNode(FklLntItem *,
        FklVMvalue *fid,
        uint64_t scp,
        size_t line,
        uint32_t scope);

FKL_API const FklLntItem *
fklFindLntItem(uint64_t cp, size_t ls, const FklLntItem *l);

static FKL_ALWAYS_INLINE const FklLntItem *
fklGetLntItem(const FklByteCodelnt *code, const FklIns *cp) {
    return fklFindLntItem(cp - code->bc.code, code->ls, code->l);
}

typedef struct {
    int64_t ix;
    uint64_t ux;
    uint64_t uy;
} FklInsArg;

FKL_API int fklGetInsOpArg(const FklIns *ins, FklInsArg *arg);
FKL_API int
fklGetInsOpArgWithOp(FklOpcode op, const FklIns *ins, FklInsArg *arg);

FKL_API int fklGetNextIns(const FklIns *cur_ins, const FklIns *ins[2]);

FKL_API
FKL_NODISCARD
int fklMakeIns(FklIns *ins, FklOpcode op, const FklInsArg *);

FKL_API void fklScanAndSetTailCall(FklByteCode *bc);

#define FKL_MAKE_INS(INS, OP, ...)                                             \
    (fklMakeIns((INS), (OP), &(const FklInsArg){ __VA_ARGS__ }));

static FKL_ALWAYS_INLINE int fklIsLoadProto(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_LOAD_PROTO;
}

static inline int fklIsJmpIns(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_JMP;
}

static inline int fklIsCondJmpIns(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_JMP_IF_TRUE
        || FKL_INS_OP(ins) == FKL_OP_JMP_IF_FALSE;
}

static inline int fklIsJmpIfTrueIns(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_JMP_IF_TRUE;
}

static inline int fklIsJmpIfFalseIns(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_JMP_IF_FALSE;
}

static inline int fklIsPutLocIns(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_PUT_LOC;
}

static inline int fklIsMakeProcIns(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_MAKE_PROC;
}

static inline int fklIsPutVarRefIns(const FklIns ins) {
    return FKL_INS_OP(ins) == FKL_OP_PUT_VAR_REF;
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

#define FKL_INS_uX(ins0, ins1)                                                 \
    (FKL_INS_uB(ins0) | (((uint32_t)FKL_INS_uB(ins1)) << FKL_I16_WIDTH))

#define FKL_INS_sX(ins0, ins1) ((int32_t)FKL_INS_uX(ins0, ins1))

#define FKL_INS_uXX(ins0, ins1, ins2)                                          \
    (FKL_INS_uC(ins0) | (((uint64_t)FKL_INS_uC(ins1)) << FKL_I24_WIDTH)        \
            | (((uint64_t)FKL_INS_uB(ins2)) << (FKL_I24_WIDTH * 2)))

#define FKL_INS_sXX(ins0, ins1, ins2) ((int64_t)FKL_INS_uXX(ins0, ins1, ins2))

#ifdef __cplusplus
}
#endif

#endif
