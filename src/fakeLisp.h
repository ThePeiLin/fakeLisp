#ifndef FAKELISP_H
#define FAKELISP_H
#include"fakedef.h"
cptr* evalution(const char*,int,intpr*);
void initEvalution();
int isscript(const char*);
void runIntpr(intpr*);
#endif
