#ifndef AST_H
#define AST_H
#include"fakedef.h"
#include<stdint.h>
FklAstCptr* fklCastVMvalueToCptr(VMvalue*,int32_t);
FklAstCptr* fklCreateTree(const char*,Intpr*,StringMatchPattern*);
#endif
