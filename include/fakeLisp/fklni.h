#ifndef FKL_NI_H
#define FKL_NI_H
#include<fakeLisp/vm.h>

FklVMvalue* fklNiGetArg(uint32_t* ap,FklVMstack*);
FklVMvalue* fklNiNewVMvalue(FklValueType type,void* p,FklVMstack*,FklVMheap* heap);
FklVMvalue* fklNiPopTop(uint32_t* ap,FklVMstack*);
void fklNiReturn(FklVMvalue*,uint32_t* ap,FklVMstack*);
void fklNiReturnMref(uint32_t,void* mem,uint32_t* ap,FklVMstack* s);
int fklNiResBp(uint32_t* ap,FklVMstack*);
void fklNiEnd(uint32_t* ap,FklVMstack*);
void fklNiBegin(uint32_t* ap,FklVMstack*);
#define FKL_NI_BEGIN(EXE) FklVMstack* stack=(EXE)->stack;uint32_t ap=0;fklNiBegin(&ap,stack)
#endif
