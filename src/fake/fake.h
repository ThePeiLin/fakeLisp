#ifndef TREE_H_
#define TREE_H_
#include<stdio.h>
#include"../fakedef.h"
defines* findDefine(const char*,env*);
cptr* createTree(const char*);
int addFunc(char*,int (*)(cptr*,env*));
defines* addDefine(const char*,const cptr*,env*);
void callFunc(cptr*,env*);
int (*(findFunc(const char*)))(cptr*,env*);
int retree(cptr*,cptr*);
int copyCptr(cptr*,const cptr*);
env* newEnv(env*);
void destroyEnv(env*);
void printList(const cptr*,FILE*);
int eval(cptr*,env*);
static void exError(const cptr*,int);
#endif
