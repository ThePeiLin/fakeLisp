#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#define SYMUNDEFINE 1
#define SYNTAXERROR 2
typedef struct
{
	struct Cell* outer;
	enum {NIL,CEL,ATM} type;
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
	enum{SYM,NUM,STR} type;
	char* value;
}atom;

typedef struct
{
	int status;
	cptr* place;
}errorStatus;

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
	errorStatus (*function)(cptr*,env*);
	struct NativeFunc* next;
}nativeFunc;

typedef struct SymbolForCompiler
{
	int count;
	char* symName;
	cptr* obj;
	struct SymbolForCompiler* next;
}compSym;

typedef struct ExcutableByteCode
{
	int64_t size;
	void* opcode;
}exByCode;
#endif
