// bytecode file loader and writer

#ifndef FKL_CODE_LW_H
#define FKL_CODE_LW_H

#include "codegen.h"
#include "value_table.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FklWriteCodePass {
    FKL_WRITE_CODE_PASS_FIRST = 0,
    FKL_WRITE_CODE_PASS_SECOND,
} FklWriteCodePass;

// write and load byte code files
void fklWriteCodeFile(FILE *fp, const FklVMvalueProc *const proc);

FKL_NODISCARD
FklVMvalueProc *fklLoadCodeFile(FILE *fp, FklVM *vm, FklLibTable *lib_table);

typedef struct {
    const FklCgCtx *const ctx;
    const FklVMvalueCgInfo *const main_info;

    const FklVMvalueProc *main_proc;
} FklWritePreCompileArgs;

void fklWritePreCompile(FILE *fp,
        const char *target_dir,
        const FklWritePreCompileArgs *const args);

typedef struct {
    // in
    FklCgCtx *const ctx;
    FklVMvalueCgLibs *const libraries;

    // out
    FklLibTable *lib_table;
    FklCgLib *cg_lib;

    const char *error_fmt;
    FklVMvalue *error_obj;
} FklLoadPreCompileArgs;

FKL_NODISCARD
const FklCgLib *
fklLoadPreCompile(FILE *fp, const char *rp, FklLoadPreCompileArgs *const args);

#ifdef __cplusplus
}
#endif

#endif
