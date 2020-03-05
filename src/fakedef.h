#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdlib.h>
#define SYMUNDEFINE 1
#define SYNTAXERROR 2
#define FAILRETURN 3
typedef struct
{
	struct Cell* outer;
	enum {nil,cel,atm} type;
	void* value;
}cptr;

typedef struct Cell
{
	struct Cell* prev;
	cptr car,cdr;
}cell;

typedef struct Atom
{
	cell* prev;
	enum{sym,num,str} type;
	char* value;
}atom;
#endif
