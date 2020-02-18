#ifndef TREE_H_
#define TREE_H_
#include<stdio.h>
#include"fakedef.h"

typedef struct Defines
{
	char* symName;
	cell obj;//node or symbol or val
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
	int (*function)(cell*,env*);
	struct NativeFunc* next;
}nativeFunc;

defines* findDefines(const char*,env*);
char* getListFromFile(FILE*);
char* getStringFromList(const char*);
cell* createTree(const char*);
int addFunc(char*,int (*)(cell*,env*));
defines* addDefine(const char*,const cell*,env*);
void callFunc(cell*,env*);
int (*(findFunc(const char*)))(cell*,env*);
int retree(cell*,cell*);
void printList(cell*,FILE*);
conspair* createCons(conspair*);
cell* createCell();
atom* createAtom(int type,const char*,conspair*);
int copyList(cell*,const cell*);
int distoryList(cell*);
int deleteCons(cell*);
env* newEnv();
static void replace(cell*,cell*);
int eval(cell*,env*);
#endif
