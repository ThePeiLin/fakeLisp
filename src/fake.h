#ifndef TREE_H_
#define TREE_H_
#include<stddef.h>
#include"fakedef.h"
defines* findDefines(const char*,env*);
char* getListFromFile(FILE*);
char* getStringFromList(const char*);
branch* createTree(const char*);
branch* eval(branch*,env*);
int addFunction(char*,void(*)(branch*,env*));
defines* addDefine(const char*,const branch*,env*);
void callFunction(branch*,env*);
void (*(findFunction(const char*)))(branch*,env*);
void returnList(branch*);
void printList(branch*,FILE*);
consPair* createCons(consPair*);
branch* createBranch();
int copyList(branch*,branch*);
branch* distoryList(branch*);
branch* deleteCons(branch*);
env* newEnv();
#endif
