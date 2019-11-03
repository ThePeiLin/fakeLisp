#ifndef _FAKEDEF_H_
#define _FAKEDEF_H_
typedef struct
{
	enum {nil,node,sym,val} type;
	void* twig;
}branch;
typedef struct ListTreeNode
{
	struct ListTreeNode* prev;
	branch left,right;
}listTreeNode;

typedef struct Leaf
{
	enum{num,str} type;
	listTreeNode* prev;
	void* value;
}leaf;

typedef struct Defines
{
	char* symName;
	branch twig;//node or symbol or val
	struct Defines* next;
}defines;

typedef struct NativeFunc//function and form
{
	char* functionName;
	void (*function)(listTreeNode*);
	struct NativeFunc* next;
}nativeFunc;

#endif
