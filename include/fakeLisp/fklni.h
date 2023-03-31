#ifndef FKL_NI_H
#define FKL_NI_H
#include<fakeLisp/vm.h>

#ifdef __cplusplus
extern "C"{
#endif

FklVMvalue* fklNiGetArg(uint32_t* ap,FklVMstack*);
FklVMvalue** fklNiReturn(FklVMvalue*,uint32_t* ap,FklVMstack*);
void fklNiSetBpWithTp(FklVMstack* s);
uint32_t fklNiSetBp(uint32_t nbp,FklVMstack* s);
int fklNiResBp(uint32_t* ap,FklVMstack*);
//void fklNiSetTp(FklVMstack*);
//void fklNiResTp(FklVMstack*);
//void fklNiPopTp(FklVMstack*);
void fklNiEnd(uint32_t* ap,FklVMstack*);
void fklNiBegin(uint32_t* ap,FklVMstack*);
void fklNiDoSomeAfterSetLoc(FklVMvalue*,uint32_t idx,FklVMframe* f,FklVM* exe);
void fklNiDoSomeAfterSetRef(FklVMvalue*,uint32_t idx,FklVMframe* f,FklVM* exe);
#define FKL_NI_BEGIN(EXE) FklVMstack* stack=(EXE)->stack;\
uint32_t ap=0;\
fklNiBegin(&ap,stack);
#define FKL_NI_CHECK_TYPE(V,P,ERR_INFO,EXE) if(!P(V))FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_INCORRECT_TYPE_VALUE,EXE)
#define FKL_NI_CHECK_REST_ARG(PAP,STACK,ERR_INFO,EXE) if(fklNiResBp((PAP),(STACK)))\
FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_ERR_TOOMANYARG,EXE)

#ifdef __cplusplus
}
#endif
#endif
