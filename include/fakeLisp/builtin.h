#ifndef FKL_BUILTIN_H
#define FKL_BUILTIN_H

#include "bytecode.h"
#include "symbol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FKL_BUILTIN_SYMBOL_NUM (198)

typedef FklByteCodelnt *(*FklBuiltinInlineFunc)(FklByteCodelnt *[], FklSid_t,
                                                uint32_t line, uint32_t scope);
FklBuiltinInlineFunc fklGetBuiltinInlineFunc(uint32_t idx, uint32_t argNum);

struct FklCodegenEnv;
struct FklVM;
struct FklVMframe;
struct FklVMproc;

void fklInitGlobCodegenEnv(struct FklCodegenEnv *, FklSymbolTable *pst);
void fklInitGlobalVMclosureForGC(struct FklVM *);

#define FKL_VM_STDIN_IDX (0)
#define FKL_VM_STDOUT_IDX (1)
#define FKL_VM_STDERR_IDX (2)

#ifdef __cplusplus
}
#endif

#endif
