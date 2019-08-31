#ifndef TREE_H_
#define TREE_H_
#include<stdio.h>
enum TYPES{nil,node,sym,chars};
typedef struct ListTreeNode
{
	enum TYPES
	leftType,
	rightType;
	struct ListTreeNode* prev;
	void* left;
	void* right;
}listTreeNode;

typedef struct Defines
{
	enum TYPES type;
	char* symName;
	void* NSC;//node or symbol or chars
	struct Defines* next;
}defines;

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
void printList(listTreeNode*);
listTreeNode* createNode(listTreeNode* const);
listTreeNode* copyTree(listTreeNode*);
listTreeNode* deleteTree(listTreeNode*);
listTreeNode* deleteNode(listTreeNode*);
#endif
