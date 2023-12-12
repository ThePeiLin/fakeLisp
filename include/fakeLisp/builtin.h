#ifndef FKL_BUILTIN_H
#define FKL_BUILTIN_H

#include"symbol.h"
#include"bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef FklByteCodelnt* (*FklBuiltinInlineFunc)(FklByteCodelnt*[],FklSid_t,uint64_t);
FklBuiltinInlineFunc fklGetBuiltinInlineFunc(uint32_t idx,uint32_t argNum);

uint8_t* fklGetBuiltinSymbolModifyMark(uint32_t*);
struct FklCodegenEnv;
struct FklVM;
struct FklVMframe;
struct FklVMproc;

uint32_t fklGetBuiltinSymbolNum(void);
void fklInitGlobCodegenEnv(struct FklCodegenEnv*,FklSymbolTable* pst);
void fklInitSymbolTableWithBuiltinSymbol(FklSymbolTable* symbolTable);
void fklInitGlobalVMclosure(struct FklVMframe* frame,struct FklVM*);
void fklInitGlobalVMclosureForProc(struct FklVMproc*,struct FklVM*);

#define FKL_VM_STDIN_IDX (0)
#define FKL_VM_STDOUT_IDX (1)
#define FKL_VM_STDERR_IDX (2)

#ifdef __cplusplus
}
#endif

#endif
