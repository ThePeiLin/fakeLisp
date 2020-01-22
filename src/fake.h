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
	void (*function)(cell*,env*);
	struct NativeFunc* next;
}nativeFunc;

defines* findDefines(const char*,env*);
char* getListFromFile(FILE*);
char* getStringFromList(const char*);
cell* createTree(const char*);
cell* eval(cell*,env*);
int addFunction(char*,void(*)(cell*,env*));
defines* addDefine(const char*,const cell*,env*);
void callFunction(cell*,env*);
void (*(findFunction(const char*)))(cell*,env*);
void returnList(cell*);
void printList(cell*,FILE*);
consPair* createCons(consPair*);
cell* createCell();
int copyList(cell*,cell*);
cell* distoryList(cell*);
cell* deleteCons(cell*);
env* newEnv();
#endif
