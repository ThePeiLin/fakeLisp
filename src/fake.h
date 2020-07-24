#ifndef TREE_H_
#define TREE_H_
#include<stdio.h>
#include"fakedef.h"
typedef struct KeyWord
{
	char* word;
	struct KeyWord* next;
}keyWord;
defines* findDefine(const char*,const env*);
cptr* createTree(const char*);
void addFunc(const char*,errorStatus (*)(cptr*,env*));
defines* addDefine(const char*,const cptr*,env*);
errorStatus (*(findFunc(const char*)))(cptr*,env*);
int retree(cptr**,cptr*);
env* newEnv(env*);
void destroyEnv(env*);
void printList(const cptr*,FILE*);
errorStatus eval(cptr*,env*);
void exError(const cptr*,int);
int addKeyWord(const char*);
keyWord* hasKeyWord(const cptr*);
static int hasAlpha(const char*);
#endif
