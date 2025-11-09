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
    FklVMgc *const gc;

    // out
    uint32_t count;
    FklVMvalue **values;
} FklLoadValueArgs;

int fklLoadValueTable(FILE *fp, FklLoadValueArgs *args);
FklVMvalue *fklLoadValueId(FILE *fp, const FklLoadValueArgs *values);

// write and load prototypes

void fklWriteFuncPrototypes(const FklVMvalueProtos *pts,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *fp);

int fklLoadFuncPrototypes(FILE *fp,
        const FklLoadValueArgs *values,
        FklVMvalueProtos *pts);

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

void fklWriteByteCodelnt(const FklByteCodelnt *bcl,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *fp);
int fklLoadByteCodelnt(FILE *fp,
        const FklLoadValueArgs *values,
        FklByteCodelnt *bcl);

// write and load vm libs
void fklWriteVMlibs(const FklVMvalueLibs *l,
        FklValueTable *vt,
        FklWriteCodePass pass,
        FILE *fp);

void fklLoadVMlibs(FILE *fp,
        FklVM *vm,
        const FklVMvalueProtos *pts,
        const FklLoadValueArgs *values,
        FklVMvalueLibs *l);

typedef struct {
    const FklVMvalueProtos *pts;
    const FklByteCodelnt *main_func;
    const FklVMvalueLibs *libs;
} FklWriteCodeFileArgs;

typedef struct {
    // in
    FklVMgc *const gc;
    FklVMvalueProtos *const pts;
    FklVMvalueLibs *const libs;

    // out
    FklVMvalue *main_func;
} FklLoadCodeFileArgs;

// write and load byte code files
void fklWriteCodeFile(FILE *fp, const FklWriteCodeFileArgs *const args);
int fklLoadCodeFile(FILE *fp, FklLoadCodeFileArgs *const args);

struct FklCodegenCtx;
struct FklVMvalueCodegenInfo;
struct FklVMvalueProtos;
struct FklCodegenLibVector;
struct FklGraProdGroupHashMap;

typedef struct {
    const struct FklCodegenCtx *const ctx;
    const struct FklVMvalueCodegenInfo *const main_info;
    const FklByteCodelnt *const main_bcl;
} FklWritePreCompileArgs;

void fklWritePreCompile(FILE *fp,
        const char *target_dir,
        const FklWritePreCompileArgs *const args);

typedef struct {
    // in
    struct FklCodegenCtx *const ctx;
    struct FklCodegenLibVector *const libraries;
    struct FklCodegenLibVector *const macro_libraries;
    struct FklVMvalueProtos *const pts;
    struct FklVMvalueProtos *const macro_pts;

    // out
    char *error;
} FklLoadPreCompileArgs;

int fklLoadPreCompile(FILE *fp,
        const char *rp,
        FklLoadPreCompileArgs *const args);

#ifdef __cplusplus
}
#endif

#endif
