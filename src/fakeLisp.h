#ifndef FAKELISP_H
#define FAKELISP_H
#include"fakedef.h"
void runIntpr(Intpr*);
ByteCode* loadByteCode(FILE*);
void loadSymbolTable(FILE*);
LineNumberTable* loadLineNumberTable(FILE*);
#endif
