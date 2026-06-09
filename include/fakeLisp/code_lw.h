// bytecode file loader and writer

#ifndef FKL_CODE_LW_H
#define FKL_CODE_LW_H

#include "codegen.h"
#include "value_table.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

// module reference type
typedef enum {
    FKL_LIB_REF_SCRIPT_EMBEDDED = 0,
    FKL_LIB_REF_DLL_INTERNAL,
    FKL_LIB_REF_DLL_ABSOLUTE,
    FKL_LIB_REF_EXTERNAL,
} FklLibRefType;

// write and load byte code files
void fklWriteCodeFile(FILE *fp, const FklVMvalueProc *const proc);

FKL_NODISCARD
FklVMvalueProc *fklLoadCodeFile(FILE *fp,
        FklVM *vm,
        const char *main_dir,
        FklLibTable *lib_table);

typedef struct {
    const FklCgCtx *const ctx;
    const FklVMvalueCgInfo *const main_info;

    const FklVMvalueProc *main_proc;
} FklWritePreCompileArgs;

void fklWritePreCompile(FILE *fp,
        const char *target_dir,
        const FklWritePreCompileArgs *const args);

// pre-compile dependence
typedef struct {
    FklVMvalue *name;
    FklVMvalue *rp;
	FklFileType ft;

    int is_imported_by_macro;
} FklPcDep;

// FklPcDepVector
#define FKL_VECTOR_ELM_TYPE FklPcDep
#define FKL_VECTOR_ELM_TYPE_NAME PcDep
#include "cont/vector.h"

typedef struct {
    FklPcDepVector pendings;
    FklValueVector protos;

    // 实际值
    FklValueVector libs;
} FklPreCompileFixup;

void fklPreCompileFixupInit(FklPreCompileFixup *);
void fklPreCompileFixupUninit(FklPreCompileFixup *);
int fklPreCompileFixup(const FklPreCompileFixup *fixup);

typedef struct {
    // in
    FklCgCtx *const ctx;
    FklVMvalueCgLibs *const libraries;

    // out
    FklLibTable *lib_table;
    FklVMvalueCgLib *cg_lib;

    FklPreCompileFixup *fixup;

    const char *error_fmt;
    FklVMvalue *error_obj;
} FklLoadPreCompileArgs;

FKL_NODISCARD
const FklVMvalueCgLib *
fklLoadPreCompile(FILE *fp, const char *rp, FklLoadPreCompileArgs *const args);

#ifdef __cplusplus
}
#endif

#endif
