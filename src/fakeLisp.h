#ifndef FAKELISP_H
#define FAKELISP_H
#include"fakedef.h"
void initEvalution();
void runIntpr(intpr*);
byteCode* castRawproc(byteCode*,rawproc*);
byteCode* loadRawproc(FILE*);
byteCode* loadByteCode(FILE*);
#endif
