#ifndef FKL_BYTECODE_H
#define FKL_BYTECODE_H

#include "base.h"
#include "bigint.h"
#include "common.h"
#include "opcode.h"
#include "symbol.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct FklVMvalue;
struct FklVMgc;

// FklKi64HashMap
#define FKL_HASH_KEY_TYPE int64_t
#define FKL_HASH_VAL_TYPE uint32_t
#define FKL_HASH_ELM_NAME Ki64
#define FKL_HASH_KEY_HASH return fklHash64Shift(*pk);
#include "cont/hash.h"

// FklKf64HashMap
#define FKL_HASH_KEY_TYPE double
#define FKL_HASH_VAL_TYPE uint32_t
#define FKL_HASH_ELM_NAME Kf64
#define FKL_HASH_KEY_HASH return *FKL_TYPE_CAST(int64_t *, pk);
#include "cont/hash.h"

// FklKstrHashMap
#define FKL_HASH_KEY_INIT(K, X) *(K) = fklCopyString(*X)
#define FKL_HASH_KEY_UNINIT(K)                                                 \
    {                                                                          \
        fklZfree(*K);                                                          \
        (*K) = NULL;                                                           \
    }
#define FKL_HASH_KEY_EQUAL(A, B) fklStringEqual(*(A), *(B))
#define FKL_HASH_KEY_TYPE FklString *
#define FKL_HASH_VAL_TYPE uint32_t
#define FKL_HASH_ELM_NAME Kstr
#define FKL_HASH_KEY_HASH return fklStringHash(*pk);
#include "cont/hash.h"

// FklKbvecHashMap
#define FKL_HASH_KEY_INIT(K, X) *(K) = fklCopyBytevector(*X)
#define FKL_HASH_KEY_UNINIT(K)                                                 \
    {                                                                          \
        fklZfree(*K);                                                          \
        (*K) = NULL;                                                           \
    }
#define FKL_HASH_KEY_EQUAL(A, B) fklBytevectorEqual(*(A), *(B))
#define FKL_HASH_KEY_TYPE FklBytevector *
#define FKL_HASH_VAL_TYPE uint32_t
#define FKL_HASH_ELM_NAME Kbvec
#define FKL_HASH_KEY_HASH return fklBytevectorHash(*pk);
#include "cont/hash.h"

// FklKbiHashMap
#define FKL_HASH_KEY_INIT(K, X) fklInitBigInt(K, X)
#define FKL_HASH_KEY_UNINIT(K) fklUninitBigInt(K)
#define FKL_HASH_KEY_EQUAL(A, B) fklBigIntEqual(A, B)
#define FKL_HASH_KEY_TYPE FklBigInt
#define FKL_HASH_VAL_TYPE uint32_t
#define FKL_HASH_ELM_NAME Kbi
#define FKL_HASH_KEY_HASH return fklBigIntHash(pk);
#include "cont/hash.h"

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
} FklInstruction;

static_assert(sizeof(FklInstruction) == sizeof(uint32_t),
        "invalid instruction definition");

typedef struct {
    FklKi64HashMap ht;
    int64_t *base;
    uint32_t count;
    uint32_t size;
} FklConstI64Table;

typedef struct {
    FklKf64HashMap ht;
    double *base;
    uint32_t count;
    uint32_t size;
} FklConstF64Table;

typedef struct {
    FklKstrHashMap ht;
    FklString **base;
    uint32_t count;
    uint32_t size;
} FklConstStrTable;

typedef struct {
    FklKbvecHashMap ht;
    FklBytevector **base;
    uint32_t count;
    uint32_t size;
} FklConstBvecTable;

typedef struct {
    FklKbiHashMap ht;
    FklBigInt const **base;
    uint32_t count;
    uint32_t size;
} FklConstBiTable;

typedef struct {
    FklConstI64Table ki64t;
    FklConstF64Table kf64t;
    FklConstStrTable kstrt;
    FklConstBvecTable kbvect;
    FklConstBiTable kbit;
} FklConstTable;

#define FKL_INSTRUCTION_STATIC_INIT { .op = FKL_OP_DUMMY, .au = 0, .bu = 0 }

typedef struct {
    uint64_t len;
    FklInstruction *code;
} FklByteCode;

typedef struct {
    struct FklVMvalue *fid;
    uint64_t scp;
    uint32_t line;
    uint32_t scope;
} FklLineNumberTableItem;

typedef struct {
    FklLineNumberTableItem *l;
    FklByteCode bc;
    uint32_t ls;
} FklByteCodelnt;

// FklByteCodelntVector
#define FKL_VECTOR_ELM_TYPE FklByteCodelnt *
#define FKL_VECTOR_ELM_TYPE_NAME ByteCodelnt
#include "cont/vector.h"

void fklInitConstTable(FklConstTable *kt);
FklConstTable *fklCreateConstTable(void);

void fklUninitConstTable(FklConstTable *kt);
void fklDestroyConstTable(FklConstTable *kt);

uint32_t fklAddI64Const(FklConstTable *kt, int64_t k);
uint32_t fklAddF64Const(FklConstTable *kt, double k);
uint32_t fklAddStrConst(FklConstTable *kt, const FklString *k);
uint32_t fklAddBvecConst(FklConstTable *kt, const FklBytevector *k);
uint32_t fklAddBigIntConst(FklConstTable *kt, const FklBigInt *k);

int64_t fklGetI64ConstWithIdx(const FklConstTable *kt, uint32_t i);
double fklGetF64ConstWithIdx(const FklConstTable *kt, uint32_t i);
const FklString *fklGetStrConstWithIdx(const FklConstTable *kt, uint32_t i);
const FklBytevector *fklGetBvecConstWithIdx(const FklConstTable *kt,
        uint32_t i);
const FklBigInt *fklGetBiConstWithIdx(const FklConstTable *kt, uint32_t i);

void fklPrintConstTable(const FklConstTable *kt, FILE *fp);

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

FKL_DEPRECATED
void fklPrintByteCode(const FklByteCode *,
        uint32_t prototype_id,
        FILE *,
        struct FklVMgc *,
        FklConstTable *);

void fklInitByteCodelnt(FklByteCodelnt *t, size_t len);
FklByteCodelnt *fklCreateByteCodelnt(size_t len);

FklByteCodelnt *fklCreateSingleInsBclnt(FklInstruction ins,
        struct FklVMvalue *fid,
        uint32_t line,
        uint32_t scope);

FKL_DEPRECATED
void fklPrintByteCodelnt(const FklByteCodelnt *obj,
        uint32_t prototype_id,
        FILE *fp,
        struct FklVMgc *,
        const FklConstTable *);

void fklUninitByteCodelnt(FklByteCodelnt *);
void fklDestroyByteCodelnt(FklByteCodelnt *);
void fklIncreaseScpOfByteCodelnt(FklByteCodelnt *, uint64_t);
void fklCodeLntConcat(FklByteCodelnt *, const FklByteCodelnt *);
void fklCodeLntReverseConcat(const FklByteCodelnt *, FklByteCodelnt *);

void fklByteCodeLntPushBackIns(FklByteCodelnt *bcl,
        const FklInstruction *ins,
        struct FklVMvalue *fid,
        uint32_t line,
        uint32_t scope);

void fklByteCodeLntInsertFrontIns(const FklInstruction *ins,
        FklByteCodelnt *bcl,
        struct FklVMvalue *fid,
        uint32_t line,
        uint32_t scope);

void fklByteCodePushBack(FklByteCode *bc, FklInstruction ins);
void fklByteCodeInsertFront(FklInstruction, FklByteCode *bc);

void fklByteCodeLntInsertInsAt(FklByteCodelnt *bcl,
        FklInstruction ins,
        uint64_t idx);
FklInstruction fklByteCodeLntRemoveInsAt(FklByteCodelnt *bcl, uint64_t idx);

void fklInitLineNumTabNode(FklLineNumberTableItem *,
        struct FklVMvalue *fid,
        uint64_t scp,
        uint32_t line,
        uint32_t scope);

const FklLineNumberTableItem *
fklFindLineNumTabNode(uint64_t cp, size_t ls, const FklLineNumberTableItem *l);

typedef struct {
    int64_t ix;
    uint64_t ux;
    uint64_t uy;
} FklInstructionArg;

int fklGetInsOpArg(const FklInstruction *ins, FklInstructionArg *arg);
int fklGetInsOpArgWithOp(FklOpcode op,
        const FklInstruction *ins,
        FklInstructionArg *arg);

int fklGetNextIns(const FklInstruction *cur_ins, const FklInstruction *ins[2]);
int fklGetNextIns2(FklInstruction *cur_ins, FklInstruction *ins[2]);

static inline int fklIsJmpIns(const FklInstruction *ins) {
    return ins->op >= FKL_OP_JMP && ins->op <= FKL_OP_JMP_XX;
}

static inline int fklIsCondJmpIns(const FklInstruction *ins) {
    return ins->op >= FKL_OP_JMP_IF_TRUE && ins->op <= FKL_OP_JMP_IF_FALSE_XX;
}

static inline int fklIsPutLocIns(const FklInstruction *ins) {
    return ins->op >= FKL_OP_PUT_LOC && ins->op <= FKL_OP_PUT_LOC_X;
}

static inline int fklIsPushProcIns(const FklInstruction *ins) {
    return ins->op >= FKL_OP_PUSH_PROC && ins->op <= FKL_OP_PUSH_PROC_XXX;
}

static inline int fklIsPutVarRefIns(const FklInstruction *ins) {
    return ins->op >= FKL_OP_PUT_VAR_REF && ins->op <= FKL_OP_PUT_VAR_REF_X;
}

static inline int fklIsCallIns(const FklInstruction *ins) {
    return ins->op == FKL_OP_CALL || ins->op == FKL_OP_TAIL_CALL;
}

static inline int fklIsRetIns(const FklInstruction *ins) {
    return ins->op >= FKL_OP_RET_IF_TRUE && ins->op <= FKL_OP_RET;
}

static inline int fklIsLoadLibIns(const FklInstruction *ins) {
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
