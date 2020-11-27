#ifndef FAKELISP_H
#define FAKELISP_H
#include"fakedef.h"
void initEvalution();
void runIntpr(intpr*);
void freeRawProc(ByteCode*,int32_t);
ByteCode* castRawproc(ByteCode*,RawProc*);
ByteCode* loadRawproc(FILE*,int32_t*);
ByteCode* loadByteCode(FILE*);
#endif
