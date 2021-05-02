#ifndef AST_H
#define AST_H
#include"fakedef.h"
#include<stdint.h>
AST_cptr* castVMvalueToCptr(VMvalue*,int32_t);
AST_cptr* createTree(const char*,Intpr*,StringMatchPattern*);
#endif
