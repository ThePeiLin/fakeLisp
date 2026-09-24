#ifndef FKL_OPTIMIZER_H
#define FKL_OPTIMIZER_H

#include "bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FklIns ins;
    uint32_t line;
    struct FklVMvalue *fid;
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
        FklInsArg *ins_arg);

FKL_API
void fklRecomputeInsImm(FklByteCodelnt *bcl,
        void *ctx,
        FklRecomputeInsImmPredicate p,
        FklRecomputeInsImmFunc func);

FKL_API
void fklInitByteCodeBuffer(FklByteCodeBuffer *buf, size_t capacity);

FKL_API
FklByteCodeBuffer *fklCreateByteCodeBuffer(size_t capacity);

FKL_API
void fklSetByteCodeBuffer(FklByteCodeBuffer *buf, const FklByteCodelnt *);

FKL_API
void fklSetByteCodelntWithBuf(FklByteCodelnt *, const FklByteCodeBuffer *buf);

FKL_API
uint32_t fklByteCodeBufferScanAndSetBasicBlock(FklByteCodeBuffer *buf);

FKL_API
FklByteCodelnt *fklCreateByteCodelntFromBuf(const FklByteCodeBuffer *);

FKL_API
void fklByteCodeBufferPush(FklByteCodeBuffer *buf,
        const FklIns *ins,
        uint32_t line,
        uint32_t scope,
        struct FklVMvalue *fid);

FKL_API
void fklInitByteCodeBufferWith(FklByteCodeBuffer *buf, const FklByteCodelnt *);

FKL_API
FklByteCodeBuffer *fklCreateByteCodeBufferWith(const FklByteCodelnt *);

FKL_API void fklUninitByteCodeBuffer(FklByteCodeBuffer *buf);

FKL_API void fklDestroyByteCodeBuffer(FklByteCodeBuffer *buf);

FKL_API void fklPeepholeOptimize(FklByteCodelnt *bcl);

#ifdef __cplusplus
}
#endif

#endif
