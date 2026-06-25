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

typedef enum {
    FKL_RE_EXPORT_OP_PUSH_LIB = 0,
    FKL_RE_EXPORT_OP_POP,
    FKL_RE_EXPORT_OP_IMPORT,
    FKL_RE_EXPORT_OP_PUSH_CTX,
    FKL_RE_EXPORT_OP_POP_CTX,
} FklReExportOp;

typedef struct {
    FklReExportOp op;
    FklCgImportType type;
    FklVMvalue *arg0;
    FklVMvalue *arg1;
} FklReExportCmd;

// FklReExportCmdVector
#define FKL_VECTOR_ELM_TYPE FklReExportCmd
#define FKL_VECTOR_ELM_TYPE_NAME ReExportCmd
#include "cont/vector.h"

FKL_VM_DEF_UD_STRUCT(FklVMvalueReExportCmds, {
    uint64_t count;
    FklReExportCmd cmds[];
});

FklVMvalueReExportCmds *fklCreateVMvalueReExportCmds(FklVM *vm, uint64_t count);

int fklIsVMvalueReExportCmds(const FklVMvalue *);
static FKL_ALWAYS_INLINE FklVMvalueReExportCmds *fklVMvalueReExportCmds(
        const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueReExportCmds(v));
    return FKL_TYPE_CAST(FklVMvalueReExportCmds *, v);
}

typedef struct {
    FklVMvalueCgLib *lib;
    FklPcDepVector pendings;
    FklValueVector protos;
} FklPreCompileFixup;

void fklPreCompileFixupInit(FklPreCompileFixup *);
void fklPreCompileFixupUninit(FklPreCompileFixup *);
int fklPreCompileFixup(const FklPreCompileFixup *fixup, const FklCgCtx *cg_ctx);

FKL_VM_DEF_UD_STRUCT(FklVMvaluePcFixup, { FklPreCompileFixup f; });
FklVMvaluePcFixup *fklCreateVMvaluePcFixup(FklVM *vm);

int fklIsVMvaluePcFixup(const FklVMvalue *v);
static FKL_ALWAYS_INLINE FklVMvaluePcFixup *fklVMvaluePcFixup(
        const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvaluePcFixup(v));
    return FKL_TYPE_CAST(FklVMvaluePcFixup *, v);
}

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
FklVMvalueCgLib *
fklLoadPreCompile(FILE *fp, const char *rp, FklLoadPreCompileArgs *const args);

#ifdef __cplusplus
}
#endif

#endif
