#ifndef FKL_DIS_H
#define FKL_DIS_H

#include "code_builder.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

void fklPrintObarray(FklVMgc *gc,
        const FklVMobarray *array,
        FklCodeBuilder *fp);
void fklDisassembleProc(FklVMgc *gc,
        const FklVMvalue *proc,
        FklCodeBuilder *fp);
void fklDisassembleByteCodelnt(FklVMgc *gc,
        const FklByteCodelnt *bcl,
        uint32_t proto_id,
        const FklVMvalueProtos *protos,
        FklCodeBuilder *build);

#ifdef __cplusplus
}
#endif

#endif
