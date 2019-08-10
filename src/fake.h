#ifndef TREE_H_
#define TREE_H_
#include<stdio.h>
typedef struct ListTreeNode
{
	enum{nil,sym,chars,node}
	leftType,
	rightType;
	struct ListTreeNode* prev;
	void* left;
	void* right;
}listTreeNode;



typedef struct SFC
{
	char* functionName;
	void (*function)(listTreeNode*);
	struct SFC* next;
}sfc;
char* getListFromFile(FILE*);
listTreeNode* becomeTree(const char*);
char* getStringFromList(const char*);
listTreeNode* eval(listTreeNode*);
int addSpecialFunction(char*,void(*)(listTreeNode*));
int addFunction(char*,listTreeNode*);
void callFunction(listTreeNode*);
void (*(findSpecialFunction(const char*)))(listTreeNode*);
void returnTree(listTreeNode*);
#endif
