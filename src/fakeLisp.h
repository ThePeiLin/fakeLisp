#ifndef FAKELISP_H
#define FAKELISP_H
#include"fakedef.h"
void initEvalution();
int isscript(const char*);
void runIntpr(intpr*);
byteCode* castRawproc(byteCode*,rawproc*);
#endif
