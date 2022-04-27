#ifndef FKL_NI_H
#define FKL_NI_H
#include<fakeLisp/symbol.h>
#include<fakeLisp/basicADT.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>

FklVMvalue* fklNiGetArg(uint32_t* ap,FklVMstack*);
void fklNiReturn(FklVMvalue*,uint32_t* ap,FklVMstack*);
int fklNiResBp(uint32_t* ap,FklVMstack*);
void fklNiEnd(uint32_t* ap,FklVMstack*);
void fklNiBegin(uint32_t* ap,FklVMstack*);
FklVMvalue* fklNiNewVMvalue(FklValueType type,void* p,FklVMstack*,FklVMheap* heap);
#define FKL_NI_BEGIN(EXE) FklVMstack* stack=(EXE)->stack;uint32_t ap=0;fklNiBegin(&ap,stack)
//#define FKL_NI_END() fklNiEnd(&ap,stack)
#endif
