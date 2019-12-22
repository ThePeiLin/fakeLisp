#ifndef _FAKEDEF_H_
#define _FAKEDEF_H_
#define OUTOFMEMORY 1
typedef struct
{
	enum {nil,con,atm} type;
	void* twig;
}branch;
typedef struct ConsPair
{
	struct ConsPair* prev;
	branch left,right;
}consPair;

typedef struct Atom
{
	consPair* prev;
	enum{sym,num,str} type;
	void* value;
}atom

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
	void (*function)(branch*);
	struct NativeFunc* next;
}nativeFunc;

void errors(int);
void errors(int types)
{
	static char* inform[]=
	{
		"dummy",
		"Out of memory!\n",
	};
	fprintf(stderr,"error:%s",inform[types]);
	exit(1);
}
#endif
