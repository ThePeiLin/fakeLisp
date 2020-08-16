#ifndef EXECUTABLE_H
#define EXECUTABLE_H
#include"fakedef.h"

typedef struct
{
	varstack local;
	byteCode* proc;
}fakeproc;

typedef struct
{
	int32_t prev;
	int32_t car;
	int32_t cdr;
}subpair;

typedef struct
{
	enum{NIL,SYM,STR,NUM,PAR} type;
	union
	{
		void* nil;
		char* sym;
		char* str;
		int32_t num;
		double dbl;
		subpair par;
		compProc proc;
	}
}substackValue;

typedef struct
{
	enum{NIL,STR,SYM,NUM,PAR,FUN} type;
	union
	{
		void* nil;
		char* str;
		char* sym;
		int32_t num;
		double dbl;
		substackValue* par;
		compProc* proc;
	}
}stackValue;

typedef struct
{
	int32_t bound;
	int size;
	stackValue* stack;
}varstack;

typedef struct
{
	int count;
	char* name;
}subSymbol;

typedef struct SymbolList
{
	int size;
	subSymbol* symbols;
}symlist;

int execute(exCode*);
#endif
