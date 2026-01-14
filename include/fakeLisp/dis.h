#ifndef FKL_DIS_H
#define FKL_DIS_H

#include "code_builder.h"
#include "value_table.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

void fklPrintObarray(FklVM *vm, const FklVMobarray *array, FklCodeBuilder *fp);

typedef struct {
    const char *indent_str;
    const FklLibTable *lib_table;
    int indents;
} FklDisArgs;

void fklDisassembleProc(FklVM *vm,
        const FklVMvalueProc *proc,
        FklCodeBuilder *fp,
        const FklDisArgs *args);

#define FKL_DIS_PROC(VM, PROC, BUILD, ...)                                     \
    fklDisassembleProc((VM), (PROC), (BUILD), &(FklDisArgs){ __VA_ARGS__ })

void fklDisassembleByteCodelnt(FklVM *vm,
        const FklByteCodelnt *bcl,
        const FklVMvalueProto *proto,
        FklCodeBuilder *build,
        const FklDisArgs *args);

#define FKL_DIS_BCL(VM, BCL, PROTO, BUILD, ...)                                \
    fklDisassembleByteCodelnt((VM),                                            \
            (BCL),                                                             \
            (PROTO),                                                           \
            (BUILD),                                                           \
            &(FklDisArgs){ __VA_ARGS__ })

#ifdef __cplusplus
}
#endif

#endif
