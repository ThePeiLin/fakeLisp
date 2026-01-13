#ifndef FKL_DIS_H
#define FKL_DIS_H

#include "code_builder.h"
#include "value_table.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

void fklPrintObarray(FklVM *vm, const FklVMobarray *array, FklCodeBuilder *fp);
void fklDisassembleProc(FklVM *vm,
        const FklVMvalueProc *proc,
        FklCodeBuilder *fp,
        const FklLibTable *lib_table);
void fklDisassembleByteCodelnt(FklVM *vm,
        const FklByteCodelnt *bcl,
        const FklVMvalueProto *proto,
        FklCodeBuilder *build,
        const FklLibTable *lib_table);

#ifdef __cplusplus
}
#endif

#endif
