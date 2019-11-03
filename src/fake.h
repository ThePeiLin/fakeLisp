#ifndef TREE_H_
#define TREE_H_
#include<stddef.h>
#include"fakedef.h"
#define OUTOFMEMORY 1
defines* findDefine(const char);
void errors(int);
char* getListFromFile(FILE*);
char* getStringFromList(const char* str);
listTreeNode* becomeTree(const char*);
listTreeNode* eval(listTreeNode*);
int addFunction(char*,void(*)(listTreeNode*));
int addDefine(const char*,void*,enum TYPES);
void callFunction(listTreeNode*);
void (*(findFunction(const char*)))(listTreeNode*);
void returnTree(listTreeNode*);
void printList(listTreeNode*,FILE*);
listTreeNode* createNode(listTreeNode* const);
listTreeNode* copyTree(listTreeNode*);
listTreeNode* deleteTree(listTreeNode*);
listTreeNode* deleteNode(listTreeNode*);
#endif
