#ifndef FAKELISP_H
#define FAKELISP_H
#include"fakedef.h"
void initEvalution();
int isscript(const char*);
int iscode(const char*);
void runIntpr(intpr*);
byteCode* castRawproc(byteCode*,rawproc*);
byteCode* loadRawproc(FILE*);
byteCode* loadByteCode(FILE*);
#endif
