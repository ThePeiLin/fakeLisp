#ifndef FAKELISP_H
#define FAKELISP_H
#include"fakedef.h"
void runIntpr(Intpr*);
ByteCode* loadByteCode(FILE*);
SymbolTable* loadSymbolTable(FILE*);
LineNumberTable* loadLineNumberTable(FILE*);
#endif
