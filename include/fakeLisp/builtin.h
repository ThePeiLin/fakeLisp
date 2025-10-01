#ifndef FKL_BUILTIN_H
#define FKL_BUILTIN_H

#include "bytecode.h"
#include "symbol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FKL_BUILTIN_SYMBOL_NUM (201)

struct FklVMvalueCodegenEnv;
struct FklVM;
struct FklVMgc;
struct FklVMframe;
struct FklVMproc;
struct FklVMvalue;

typedef FklByteCodelnt *(*FklBuiltinInlineFunc)(FklByteCodelnt *[],
        struct FklVMvalue *,
        uint32_t line,
        uint32_t scope);
FklBuiltinInlineFunc fklGetBuiltinInlineFunc(uint32_t idx, uint32_t argNum);

void fklInitGlobCodegenEnv(struct FklVMvalueCodegenEnv *, struct FklVM *gc);
void fklInitGlobalVMclosureForGC(struct FklVMgc *);

#define FKL_VM_STDIN_IDX (0)
#define FKL_VM_STDOUT_IDX (1)
#define FKL_VM_STDERR_IDX (2)

#ifdef __cplusplus
}
#endif

#endif
