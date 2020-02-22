#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdlib.h>
#define SYMUNDEFINE 1
#define SYNTAXERROR 2
#define FAILRETURN 3
typedef struct
{
	void* outer;
	enum {nil,cel,atm} type;
	void* value;
}cptr;

typedef struct Cell
{
	struct Cell* prev;
	cptr left,right;
}cell;

typedef struct Atom
{
	cell* prev;
	enum{sym,num,str} type;
	void* value;
}atom;
#endif
