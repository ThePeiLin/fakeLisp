#ifndef TREE_H_
#define TREE_H_
#include<stdio.h>
#include"fakedef.h"

typedef struct Defines
{
	char* symName;
	cptr obj;//node or symbol or val
	struct Defines* next;
}defines;

typedef struct Env
{
	struct Env* prev;
	defines* symbols;
	struct Env* next;
}env;

typedef struct NativeFunc//function and form
{
	char* functionName;
	int (*function)(cptr*,env*);
	struct NativeFunc* next;
}nativeFunc;

defines* findDefine(const char*,env*);
char* getListFromFile(FILE*);
char* getStringFromList(const char*);
cptr* createTree(const char*);
int addFunc(char*,int (*)(cptr*,env*));
defines* addDefine(const char*,const cptr*,env*);
void callFunc(cptr*,env*);
int (*(findFunc(const char*)))(cptr*,env*);
int retree(cptr*,cptr*);
void printList(cptr*,FILE*);
cell* createCell(cell*);
cptr* createCptr();
atom* createAtom(int type,const char*,cell*);
int copyList(cptr*,const cptr*);
int distoryList(cptr*);
int deleteCptr(cptr*);
env* newEnv();
void destroyEnv(env*);
void replace(cptr*,const cptr*);
int eval(cptr*,env*);
int cptrcmp(cptr*,cptr*);
#endif
