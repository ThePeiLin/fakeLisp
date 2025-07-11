#ifndef FKL_OPTIMIZER_H
#define FKL_OPTIMIZER_H

#include "bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FklInstruction ins;
    uint32_t line;
    FklSid_t fid;
    uint32_t scope;
    uint32_t block_id;
    uint32_t jmp_to;
} FklInsLn;

typedef struct {
    size_t size;
    size_t capacity;
    FklInsLn *base;
} FklByteCodeBuffer;

typedef int (*FklRecomputeInsImmPredicate)(FklOpcode op);
typedef int (*FklRecomputeInsImmFunc)(void *ctx,
        FklOpcode *opcode,
        FklOpcodeMode *pmode,
        FklInstructionArg *ins_arg);

void fklRecomputeInsImm(FklByteCodelnt *bcl,
        void *ctx,
        FklRecomputeInsImmPredicate p,
        FklRecomputeInsImmFunc func);

void fklInitByteCodeBuffer(FklByteCodeBuffer *buf, size_t capacity);
FklByteCodeBuffer *fklCreateByteCodeBuffer(size_t capacity);

void fklSetByteCodeBuffer(FklByteCodeBuffer *buf, const FklByteCodelnt *);
void fklSetByteCodelntWithBuf(FklByteCodelnt *, const FklByteCodeBuffer *buf);

uint32_t fklByteCodeBufferScanAndSetBasicBlock(FklByteCodeBuffer *buf);

FklByteCodelnt *fklCreateByteCodelntFromBuf(const FklByteCodeBuffer *);

void fklByteCodeBufferPush(FklByteCodeBuffer *buf,
        const FklInstruction *ins,
        uint32_t line,
        uint32_t scope,
        FklSid_t fid);

void fklInitByteCodeBufferWith(FklByteCodeBuffer *buf, const FklByteCodelnt *);
FklByteCodeBuffer *fklCreateByteCodeBufferWith(const FklByteCodelnt *);

void fklUninitByteCodeBuffer(FklByteCodeBuffer *buf);
void fklDestroyByteCodeBuffer(FklByteCodeBuffer *buf);

void fklPeepholeOptimize(FklByteCodelnt *bcl);

#ifdef __cplusplus
}
#endif

#endif
