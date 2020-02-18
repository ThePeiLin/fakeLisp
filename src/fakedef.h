#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdlib.h>
#define SYMUNDEFINE 1
#define SYNTAXERROR 2
#define FAILRETURN 3
typedef struct
{
	void* outer;
	enum {nil,con,atm} type;
	void* value;
}cell;

typedef struct ConsPair
{
	struct ConsPair* prev;
	cell left,right;
}conspair;

typedef struct Atom
{
	conspair* prev;
	enum{sym,num,str} type;
	void* value;
}atom;
#endif
