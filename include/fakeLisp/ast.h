#ifndef AST_H
#define AST_H
#include"fakedef.h"
#include<stdint.h>
AST_cptr* fklCastVMvalueToCptr(VMvalue*,int32_t);
AST_cptr* fklCreateTree(const char*,Intpr*,StringMatchPattern*);
#endif
