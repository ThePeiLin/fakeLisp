#ifndef TREE_H_
#define TREE_H_
#include<stdio.h>
#include"fakedef.h"
defines* findDefine(const char*,env*);
cptr* createTree(const char*);
void addFunc(char*,errorStatus (*)(cptr*,env*));
defines* addDefine(const char*,const cptr*,env*);
errorStatus (*(findFunc(const char*)))(cptr*,env*);
int retree(cptr*,cptr*);
int copyCptr(cptr*,const cptr*);
env* newEnv(env*);
void destroyEnv(env*);
void printList(const cptr*,FILE*);
errorStatus eval(cptr*,env*);
void exError(const cptr*,int);
#endif
