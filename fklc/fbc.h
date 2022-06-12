#ifndef FKL_FKLC_BC
#define FKL_FKLC_BC
#include<fakeLisp/opcode.h>
#include<fakeLisp/bytecode.h>
#include<fakeLisp/vm.h>

#ifdef __cplusplus
extern "C"{
#endif

int fklcIsFbc(FklVMvalue*);
void fklcInit(FklVMvalue* rel);
FklVMudata* fklcNewFbcUd(FklByteCode* code);
FklVMvalue* fklcGetRel(void);
FklByteCode* fklcNewPushStrByteCode(const FklVMstr* str);
#ifdef __cplusplus
}
#endif
#endif