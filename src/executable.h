#ifndef EXECUTABLE_H
#define EXECUTABLE_H
#include"fakedef.h"

typedef struct FunctionValue
{
	int num;
	byteCode** func;
}funcVal;

typedef struct
{
	int prev;
	int car;
	int cdr;
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
		subpair par;
		funVal func;
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
		substackValue par;
		funcval func;
	}
}stackValue;

typedef struct
{
	int size;
	stackValue* stack;
}statStack;

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
