#ifndef FKL_NI_H
#define FKL_NI_H
#include<fakeLisp/vm.h>

#ifdef __cplusplus
extern "C"{
#endif

FklVMvalue* fklNiGetArg(uint32_t* ap,FklVMstack*);
FklVMvalue* fklNiPopTop(uint32_t* ap,FklVMstack*);
void fklNiReturn(FklVMvalue*,uint32_t* ap,FklVMstack*);
void fklNiSetBp(uint64_t nbp,FklVMstack* s);
int fklNiResBp(uint32_t* ap,FklVMstack*);
void fklNiSetTp(FklVMstack*);
void fklNiResTp(FklVMstack*);
void fklNiPopTp(FklVMstack*);
void fklNiEnd(uint32_t* ap,FklVMstack*);
void fklNiBegin(uint32_t* ap,FklVMstack*);
FklVMvalue** fklNiGetTopSlot(FklVMstack*);
#define FKL_NI_BEGIN(EXE) FklVMstack* stack=(EXE)->stack;\
uint32_t ap=0;\
fklNiBegin(&ap,stack);
#define FKL_NI_CHECK_TYPE(V,P,ERR_INFO,RUNNABLE,EXE) if(!P(V))FKL_RAISE_BUILTIN_ERROR_CSTR(ERR_INFO,FKL_WRONGARG,RUNNABLE,EXE)
#ifdef __cplusplus
}
#endif
#endif
