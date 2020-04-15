#ifndef TREE_H_
#define TREE_H_
#include<stdio.h>
#include"fakedef.h"
defines* findDefine(const char*,const env*);
cptr* createTree(const char*);
void addFunc(const char*,errorStatus (*)(cptr*,env*));
defines* addDefine(const char*,const cptr*,env*);
errorStatus (*(findFunc(const char*)))(cptr*,env*);
int retree(cptr*,cptr*);
env* newEnv(env*);
void destroyEnv(env*);
void printList(const cptr*,FILE*);
errorStatus eval(cptr*,env*);
void exError(const cptr*,int);
static int hasAlpha(const char*);
#endif
