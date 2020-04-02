#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#define SYMUNDEFINE 1
#define SYNTAXERROR 2
#define TOOMUCHARGS 3
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

typedef struct Defines
{
	char* symName;
	cptr obj;//node or symbol or val
	struct Defines* next;
}defines;

typedef struct Env
{
	struct Env* prev;
	defines* symbols;
}env;

typedef struct NativeFunc//function and form
{
	char* functionName;
	int (*function)(cptr*,env*);
	struct NativeFunc* next;
}nativeFunc;

typedef struct SymbolForCompiler
{
	int count;
	char* symName;
	cptr* obj;
	struct SymbolForCompiler* next;
}compSym;
#endif
