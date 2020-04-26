#ifndef EXECUTABLE_H
#define EXECUTABLE_H
#include"fakedef.h"

typedef struct StackValue
{
	enum{NIL,STR,SYM,NUM,LIST,CEL} type;
	union
	{
		void* nil;
		char* str;
		char* sym;
		int64_t num;
		struct SubCell cel;
		union SubStackValue* list;
	}
}stackValue;

typedef struct SubStackValue
{
	enum{NIL,SYM,STR,NUM,CEL} type;
	union
	{
		void* nil;
		char* sym;
		char* str;
		int num;
		struct SubCell cel;
	}
}substackValue;

typedef struct SubCell
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
