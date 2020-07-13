#ifndef EXECUTABLE_H
#define EXECUTABLE_H
#include"fakedef.h"

typedef struct StackValue
{
	enum{NIL,STR,SYM,NUM,LIST,PAR} type;
	union
	{
		void* nil;
		char* str;
		char* sym;
		int64_t num;
		struct SubPair cel;
		union SubStackValue* list;
	}
}stackValue;

typedef struct SubStackValue
{
	enum{NIL,SYM,STR,NUM,PAR} type;
	union
	{
		void* nil;
		char* sym;
		char* str;
		int num;
		struct SubPair cel;
	}
}substackValue;

typedef struct SubPair
{
	int car;
	int cdr;
}subcell;

typedef struct StaticStack
{
	int size;
	stackValue* stack;
}statStack;

typedef struct FunctionValue
{
	int size;
	struct ExcutableCode* func;
}funcVal;

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
