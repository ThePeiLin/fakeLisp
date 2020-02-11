#ifndef TREE_H_
#define TREE_H_
#include<stddef.h>
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
int evalution(cell*,env*);
int addFunc(char*,void(*)(cell*,env*));
defines* addDefine(const char*,const cell*,env*);
void callFunc(cell*,env*);
void (*(findFunc(const char*)))(cell*,env*);
void returnList(cell*);
void printList(cell*,FILE*);
conspair* createCons(conspair*);
cell* createCell();
atom* createAtom(int type,const char*,conspair*);
void copyList(cell*,cell*);
cell* distoryList(cell*);
cell* deleteCons(cell*);
env* newEnv();
static void replace(cell*,cell*);
static int eval(cell*,env*);
#endif
