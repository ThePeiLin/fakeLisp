#ifndef FKL_BUILTIN_H
#define FKL_BUILTIN_H

#include "codegen.h"
#include "vm_fwd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FKL_BUILTIN_SYMBOL_NUM (206)

typedef FklVMvalue *(*FklBuiltinInlineFunc)(FklVM *exe,
        FklVMvalue *[],
        FklVMvalue *,
        size_t line,
        uint32_t scope);

FKL_API
FklBuiltinInlineFunc fklGetBuiltinInlineFunc(uint32_t idx, uint32_t argNum);

FKL_API
void fklInitGlobCgEnv(FklVMvalueCgEnv *, FklVM *gc, int is_precompile);

FKL_API
void fklInitGlobalVMclosureForGC(FklVMgc *);

#define FKL_VM_STDIN_IDX (0)
#define FKL_VM_STDOUT_IDX (1)
#define FKL_VM_STDERR_IDX (2)

#ifdef __cplusplus
}
#endif

#endif
