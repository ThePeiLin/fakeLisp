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

typedef struct {
    uint8_t op;
    union {
        uint8_t au;
        int8_t ai;
    };
    union {
        uint16_t bu;
        int16_t bi;
    };
} FklInstruction;

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
    FklSid_t fid;
    uint64_t scp;
    uint32_t line;
    uint32_t scope;
} FklLineNumberTableItem;

typedef struct {
    FklLineNumberTableItem *l;
    FklByteCode *bc;
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

FklByteCode *fklCreateByteCode(size_t);

void fklCodeConcat(FklByteCode *, const FklByteCode *);
void fklCodeReverseConcat(const FklByteCode *, FklByteCode *);

FklByteCode *fklCopyByteCode(const FklByteCode *);
FklByteCodelnt *fklCopyByteCodelnt(const FklByteCodelnt *);

void fklDestroyByteCode(FklByteCode *);
void fklPrintByteCode(const FklByteCode *,
        FILE *,
        FklSymbolTable *,
        FklConstTable *);

FklByteCodelnt *fklCreateByteCodelnt(FklByteCode *bc);

FklByteCodelnt *fklCreateSingleInsBclnt(FklInstruction ins,
        FklSid_t fid,
        uint32_t line,
        uint32_t scope);

void fklPrintByteCodelnt(const FklByteCodelnt *obj,
        FILE *fp,
        const FklSymbolTable *,
        const FklConstTable *);
void fklDestroyByteCodelnt(FklByteCodelnt *);
void fklIncreaseScpOfByteCodelnt(FklByteCodelnt *, uint64_t);
void fklCodeLntConcat(FklByteCodelnt *, const FklByteCodelnt *);
void fklCodeLntReverseConcat(const FklByteCodelnt *, FklByteCodelnt *);

void fklByteCodeLntPushBackIns(FklByteCodelnt *bcl,
        const FklInstruction *ins,
        FklSid_t fid,
        uint32_t line,
        uint32_t scope);

void fklByteCodeLntInsertFrontIns(const FklInstruction *ins,
        FklByteCodelnt *bcl,
        FklSid_t fid,
        uint32_t line,
        uint32_t scope);

void fklByteCodePushBack(FklByteCode *bc, FklInstruction ins);
void fklByteCodeInsertFront(FklInstruction, FklByteCode *bc);

void fklByteCodeLntInsertInsAt(FklByteCodelnt *bcl,
        FklInstruction ins,
        uint64_t idx);
FklInstruction fklByteCodeLntRemoveInsAt(FklByteCodelnt *bcl, uint64_t idx);

void fklInitLineNumTabNode(FklLineNumberTableItem *,
        FklSid_t fid,
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

int fklIsJmpIns(const FklInstruction *ins);
int fklIsCondJmpIns(const FklInstruction *ins);
int fklIsPutLocIns(const FklInstruction *ins);
int fklIsPushProcIns(const FklInstruction *ins);
int fklIsPutVarRefIns(const FklInstruction *ins);

void fklScanAndSetTailCall(FklByteCode *bc);

void fklWriteConstTable(const FklConstTable *kt, FILE *fp);
void fklLoadConstTable(FILE *fp, FklConstTable *kt);

void fklWriteLineNumberTable(const FklLineNumberTableItem *,
        uint32_t num,
        FILE *);
void fklLoadLineNumberTable(FILE *fp,
        FklLineNumberTableItem **plist,
        uint32_t *pnum);

void fklWriteByteCode(const FklByteCode *bc, FILE *fp);
FklByteCode *fklLoadByteCode(FILE *fp);

void fklWriteByteCodelnt(const FklByteCodelnt *bcl, FILE *fp);
FklByteCodelnt *fklLoadByteCodelnt(FILE *fp);

#ifdef __cplusplus
}
#endif

#endif
