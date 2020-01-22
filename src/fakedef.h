#ifndef _FAKEDEF_H_
#define _FAKEDEF_H_
#include<stdlib.h>
#define OUTOFMEMORY 1
typedef struct
{
	enum {nil,con,atm} type;
	void* value;
}cell;


typedef struct ConsPair
{
	struct ConsPair* prev;
	cell left,right;
}consPair;

typedef struct Atom
{
	consPair* prev;
	enum{sym,num,str} type;
	void* value;
}atom;

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
