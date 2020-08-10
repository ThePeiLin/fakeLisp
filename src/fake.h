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
void addFunc(const char*,errorStatus (*)(cptr*,env*));
defines* addDefine(const char*,const cptr*,env*);
errorStatus (*(findFunc(const char*)))(cptr*,env*);
int retree(cptr**,cptr*);
errorStatus eval(cptr*,env*);
void addKeyWord(const char*);
keyWord* hasKeyWord(const cptr*);
void printAllKeyWord();
#endif
