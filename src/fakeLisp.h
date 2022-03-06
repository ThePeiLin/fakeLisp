#ifndef FAKELISP_H
#define FAKELISP_H
#include<fakeLisp/bytecode.h>
#include<fakeLisp/compiler.h>
void runIntpr(FklInterpreter*);
FklByteCode* loadByteCode(FILE*);
void loadSymbolTable(FILE*);
LineNumberTable* loadLineNumberTable(FILE*);
#endif
