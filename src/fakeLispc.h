#ifndef FAKELISPC_H
#define FAKELISPC_H
#include"fakedef.h"
void initEvalution();
ByteCode* compileFile(intpr*);
ByteCode* castRawproc(RawProc*);
#endif
