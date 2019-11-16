#ifndef _FAKEDEF_H_
#define _FAKEDEF_H_
#define OUTOFMEMORY 1
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
	branch obj;//node or symbol or val
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
	void (*function)(listTreeNode*);
	struct NativeFunc* next;
}nativeFunc;

void errors(int);
void errors(int types)
{
	static char* inform[]=
	{
		"dummy",
		"Out of memory!\n"
	};
	fprintf(stderr,"error:%s",inform[types]);
	exit(1);
}
#endif
