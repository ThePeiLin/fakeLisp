#ifndef FKL_FKLC_BC
#define FKL_FKLC_BC
#include<fakeLisp/opcode.h>
#include<fakeLisp/bytecode.h>
#include<fakeLisp/vm.h>

#ifdef __cplusplus
extern "C"{
#endif

int fklcIsFbc(FklVMvalue*);
void fklcInit(FklVMdll* rel);
FklVMudata* fklcCreateFbcUd(FklByteCode* code,FklVMvalue* dll,FklVMvalue* pd);
FklByteCode* fklcCreatePushSidByteCode(FklSid_t,FklSymbolTable*,FklVMvalue* pd);
FklByteCode* fklcCreatePushObjByteCode(FklVMvalue*,FklSymbolTable*,FklVMvalue*);
FklByteCode* fklcCreatePushPairByteCode(FklVMvalue*);
FklByteCode* fklcCreatePushBoxByteCode(FklVMvalue*);
FklByteCode* fklcCreatePushVectorByteCode(FklVMvalue*);
void fklcCodeAppend(FklByteCode**,const FklByteCode*);
#ifdef __cplusplus
}
#endif
#endif
