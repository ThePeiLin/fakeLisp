#ifndef AST_H
#define AST_H
#include"fakedef.h"
#include<stdint.h>
AST_cptr* castVMvalueToCptr(VMvalue*,int32_t,AST_pair*);
AST_cptr* createTree(const char*,intpr*);
static void addToList(AST_cptr*,const AST_cptr* src);
#endif
