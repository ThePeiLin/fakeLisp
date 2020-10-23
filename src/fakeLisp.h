#ifndef FAKELISP_H
#define FAKELISP_H
#include"fakedef.h"
void initEvalution();
void runIntpr(intpr*);
ByteCode* castRawproc(ByteCode*,RawProc*);
ByteCode* loadRawproc(FILE*);
ByteCode* loadByteCode(FILE*);
#endif
