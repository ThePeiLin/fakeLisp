// bytecode file loader and writer

#ifndef FKL_CODE_LW
#define FKL_CODE_LW

#include "bytecode.h"
#include "codegen.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FklWriteCodePass {
    FKL_WRITE_CODE_PASS_FIRST = 0,
    FKL_WRITE_CODE_PASS_SECOND,
} FklWriteCodePass;

// for type-safe
typedef struct {
    FklValueTable vt;
} FklProtoTable;

static inline void fklInitProtoTable(FklProtoTable *t) {
    fklInitValueTable(&t->vt);
}

static inline void fklUninitProtoTable(FklProtoTable *t) {
    fklUninitValueTable(&t->vt);
}

static inline FklValueId fklProtoTableAdd(FklProtoTable *t,
        const FklVMvalueProto *v) {
    return fklValueTableAdd(&t->vt, FKL_VM_VAL(v));
}

static inline FklValueId fklProtoTableGet(const FklProtoTable *t,
        const FklVMvalueProto *v) {
    return fklValueTableGet(&t->vt, FKL_VM_VAL(v));
}

// for type-safe
typedef struct {
    FklValueTable vt;
} FklLibTable;

static inline void fklInitLibTable(FklLibTable *t) {
    fklInitValueTable(&t->vt);
}

static inline void fklUninitLibTable(FklLibTable *t) {
    fklUninitValueTable(&t->vt);
}

static inline FklValueId fklLibTableAdd(FklLibTable *t,
        const FklVMvalueLib *v) {
    return fklValueTableAdd(&t->vt, FKL_VM_VAL(v));
}

static inline FklValueId fklLibTableGet(const FklLibTable *t,
        const FklVMvalueLib *v) {
    return fklValueTableGet(&t->vt, FKL_VM_VAL(v));
}

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

typedef struct {
    // in
    FklVM *const vm;

    // out
    FklValueId count;
    FklVMvalueLib **libs;
} FklLoadLibArgs;

FKL_NODISCARD
int fklLoadLibTable(FILE *fp,
        const FklLoadValueArgs *values,
        const FklLoadProtoArgs *protos,
        FklLoadLibArgs *args);

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
        FklProtoTable *proto_table,
        FklLibTable *lib_table,
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
// void fklWriteVMlibs(const FklVMvalueVec *l,
//         FklValueTable *vt,
//         FklProtoTable *proto_table,
//         FklLibTable *lib_table,
//         FklWriteCodePass pass,
//         FILE *fp);
//
// FklVMvalueVec *fklLoadVMlibs(FILE *fp,
//         FklVM *vm,
//         const FklLoadValueArgs *values,
//         const FklLoadProtoArgs *protos);

typedef struct {
    const FklVMvalueProc *proc;
} FklWriteCodeFileArgs;

typedef struct {
    // in
    FklVM *const vm;

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
    FklCgCtx *const ctx;
    FklVMvalueCgLibs *const libraries;

    // out
    char *error;
} FklLoadPreCompileArgs;

FKL_NODISCARD
const FklCgLib *
fklLoadPreCompile(FILE *fp, const char *rp, FklLoadPreCompileArgs *const args);

#ifdef __cplusplus
}
#endif

#endif
