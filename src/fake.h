#ifndef TREE_H_
#define TREE_H_
#include<stddef.h>
#include"fakedef.h"
defines* findDefine(const char,env*);
char* getListFromFile(FILE*);
char* getStringFromList(const char*);
branch* becomeTree(const char*);
branch* eval(branch*,env*);
int addFunction(char*,void(*)(consPair*,env*));
defines* addDefine(const char*,const branch*,env*);
void callFunction(consPair*,env*);
void (*(findFunction(const char*)))(branch*,env*);
void returnTree(branch*);
void printList(branch*,FILE*);
consPair* createNode(consPair* const);
branch* createBranch();
branch* copyTree(branch*);
branch* deleteTree(branch*);
branch* deleteNode(branch*);
env* newEnv();
#endif
