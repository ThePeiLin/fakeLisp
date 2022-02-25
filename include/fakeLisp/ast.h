#ifndef AST_H
#define AST_H
#include"fakedef.h"
#include<stdint.h>
FklAstCptr* fklCastVMvalueToCptr(FklVMvalue*,int32_t);
FklAstCptr* fklCreateTree(const char*,FklIntpr*,FklStringMatchPattern*);
#endif
