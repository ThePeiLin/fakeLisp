#ifndef FAKEDEF_H
#define FAKEDEF_H
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
#endif
