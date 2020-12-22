#ifndef FAKELISP_H
#define FAKELISP_H
#include"fakedef.h"
void initEvalution();
void runIntpr(intpr*);
ByteCode* loadRawproc(FILE*,int32_t*);
ByteCode* loadByteCode(FILE*);
#endif
