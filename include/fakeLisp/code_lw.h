// bytecode file loader and writer

#ifndef FKL_CODE_LW
#define FKL_CODE_LW

#include "bytecode.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FklWriteCodePass {
    FKL_WRITE_CODE_PASS_FIRST = 0,
    FKL_WRITE_CODE_PASS_SECOND,
} FklWriteCodePass;

// write and load value table

typedef struct {
    // in
    FklVM *const vm;

    // out
    FklValueId count;
    FklVMvalue **values;
} FklLoadValueArgs;

FKL_NODISCARD
int fklLoadValueTable(FILE *fp, FklLoadValueArgs *args);

typedef struct {
    // in
    FklVM *const vm;

    // out
    FklValueId count;
    FklVMvalueProto **protos;
} FklLoadProtoArgs;

FKL_NODISCARD
int fklLoadProtoTable(FILE *fp,
        const FklLoadValueArgs *values,
        FklLoadProtoArgs *args);

// write and load bytecodes

void fklWriteLineNumberTable(const FklLineNumberTableItem *,
        uint32_t count,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *);

void fklLoadLineNumberTable(FILE *fp,
        const FklLoadValueArgs *values,
        FklLineNumberTableItem **plist,
        uint32_t *pnum);

void fklWriteByteCode(const FklByteCode *bc, FILE *fp);
void fklLoadByteCode(FklByteCode *bc, FILE *fp);

void fklWriteProc(const FklVMvalueProc *proc,
        FklValueTable *vt,
        FklValueTable *proto_table,
        FklWriteCodePass pass,
        FILE *fp);
FklVMvalueProc *fklLoadProc(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos);

void fklWriteByteCodelnt(const FklByteCodelnt *bcl,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *fp);

FKL_NODISCARD
int fklLoadByteCodelnt(FILE *fp,
        const FklLoadValueArgs *values,
        FklByteCodelnt *bcl);

// write and load vm libs
void fklWriteVMlibs(const FklVMvalueLibs *l,
        FklValueTable *vt,
        FklValueTable *proto_table,
        FklWriteCodePass pass,
        FILE *fp);

void fklLoadVMlibs(FILE *fp,
        FklVM *vm,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos,
        FklVMvalueLibs *l);

typedef struct {
    const FklVMvalueProc *proc;
    const FklVMvalueLibs *libs;
} FklWriteCodeFileArgs;

typedef struct {
    // in
    FklVM *const vm;
    FklVMvalueLibs *const libs;

    // out
    FklVMvalueProc *main_func;
} FklLoadCodeFileArgs;

// write and load byte code files
void fklWriteCodeFile(FILE *fp, const FklWriteCodeFileArgs *const args);

FKL_NODISCARD
int fklLoadCodeFile(FILE *fp, FklLoadCodeFileArgs *const args);

struct FklCgCtx;
struct FklGraProdGroupHashMap;

typedef struct {
    const struct FklCgCtx *const ctx;
    const struct FklVMvalueCgInfo *const main_info;

    const FklVMvalueProc *main_proc;
} FklWritePreCompileArgs;

void fklWritePreCompile(FILE *fp,
        const char *target_dir,
        const FklWritePreCompileArgs *const args);

typedef struct {
    // in
    struct FklCgCtx *const ctx;
    struct FklVMvalueCgLibs *const libraries;
    struct FklVMvalueCgLibs *const macro_libraries;

    // out
    char *error;
} FklLoadPreCompileArgs;

FKL_NODISCARD
int fklLoadPreCompile(FILE *fp,
        const char *rp,
        FklLoadPreCompileArgs *const args);

#ifdef __cplusplus
}
#endif

#endif
