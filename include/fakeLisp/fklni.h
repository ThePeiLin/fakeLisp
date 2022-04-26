#ifndef FKL_NI_H
#define FKL_NI_H
#include<fakeLisp/symbol.h>
#include<fakeLisp/basicADT.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>

FklVMvalue* fklNiGetArg(FklVMstack*);
void fklNiReturn(FklVMvalue*,FklVMstack*);
int fklNiResBp(FklVMstack*);
#endif
