#ifndef FKL_BYTECODE_H
#define FKL_BYTECODE_H

#include "common.h"
#include "opcode.h"
#include "symbol.h"
#include "vm_fwd.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
    struct {
        uint8_t op;
        union {
            uint8_t au;
            int8_t ai;
        };
        union {
            uint16_t bu;
            int16_t bi;
        };
    };
    struct {
        uint32_t op1 : 8;
        uint32_t ci : 24;
    };
    struct {
        uint32_t op2 : 8;
        uint32_t cu : 24;
    };
} FklIns;

static_assert(sizeof(FklIns) == sizeof(uint32_t),
        "invalid instruction definition");

#define FKL_INSTRUCTION_STATIC_INIT { .op = FKL_OP_DUMMY, .au = 0, .bu = 0 }

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
        const FklIns *ins,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope);

void fklByteCodeLntInsertFrontIns(const FklIns *ins,
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

static inline int fklIsJmpIns(const FklIns *ins) {
    return ins->op >= FKL_OP_JMP && ins->op <= FKL_OP_JMP_XX;
}

static inline int fklIsCondJmpIns(const FklIns *ins) {
    return ins->op >= FKL_OP_JMP_IF_TRUE && ins->op <= FKL_OP_JMP_IF_FALSE_XX;
}

static inline int fklIsPutLocIns(const FklIns *ins) {
    return ins->op >= FKL_OP_PUT_LOC && ins->op <= FKL_OP_PUT_LOC_X;
}

static inline int fklIsPushProcIns(const FklIns *ins) {
    return ins->op >= FKL_OP_PUSH_PROC && ins->op <= FKL_OP_PUSH_PROC_XXX;
}

static inline int fklIsPutVarRefIns(const FklIns *ins) {
    return ins->op >= FKL_OP_PUT_VAR_REF && ins->op <= FKL_OP_PUT_VAR_REF_X;
}

static inline int fklIsCallIns(const FklIns *ins) {
    return ins->op == FKL_OP_CALL || ins->op == FKL_OP_TAIL_CALL;
}

static inline int fklIsRetIns(const FklIns *ins) {
    return ins->op >= FKL_OP_RET_IF_TRUE && ins->op <= FKL_OP_RET;
}

static inline int fklIsLoadLibIns(const FklIns *ins) {
    return ins->op >= FKL_OP_LOAD_LIB && ins->op <= FKL_OP_LOAD_LIB_X;
}

void fklScanAndSetTailCall(FklByteCode *bc);

#define FKL_GET_INS_UC(ins)                                                    \
    (((((uint32_t)(ins)->bu) << FKL_BYTE_WIDTH)) | (ins)->au)
#define FKL_GET_INS_IC(ins) (((int32_t)FKL_GET_INS_UC(ins)) - FKL_I24_OFFSET)
#define FKL_GET_INS_UX(ins)                                                    \
    ((ins)->bu | (((uint32_t)(ins)[1].bu) << FKL_I16_WIDTH))
#define FKL_GET_INS_IX(ins) ((int32_t)FKL_GET_INS_UX(ins))
#define FKL_GET_INS_UXX(ins)                                                   \
    (FKL_GET_INS_UC(ins)                                                       \
            | (((uint64_t)FKL_GET_INS_UC((ins) + 1)) << FKL_I24_WIDTH)         \
            | (((uint64_t)ins[2].bu) << (FKL_I24_WIDTH * 2)))

#define FKL_GET_INS_IXX(ins) ((int64_t)FKL_GET_INS_UXX(ins))

#ifdef __cplusplus
}
#endif

#endif
