#ifndef TREE_H_
#define TREE_H_
#include<stdio.h>
typedef struct ListTreeNode
{
	enum{nil,node,sym,chars}
	leftType,
	rightType;
	struct ListTreeNode* prev;
	void* left;
	void* right;
}listTreeNode;



typedef struct FAF//function and form
{
	char* functionName;
	void (*function)(listTreeNode*);
	struct FAF* next;
}faf;
char* getListFromFile(FILE*);
listTreeNode* becomeTree(const char*);
char* getStringFromList(const char*);
listTreeNode* eval(listTreeNode*);
int addFunction(char*,void(*)(listTreeNode*));
int addDefine(char*,listTreeNode*);
void callFunction(listTreeNode*);
void (*(findFunction(const char*)))(listTreeNode*);
void returnTree(listTreeNode*);
void printfList(listTreeNode*);
listTreeNode* createNode(listTreeNode* const);
listTreeNode* copyTree(listTreeNode*);
listTreeNode* deleteTree(listTreeNode*);
listTreeNode* deleteNode(listTreeNode*);
#endif
