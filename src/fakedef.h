#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#define SYMUNDEFINE 1
#define SYNTAXERROR 2
typedef struct
{
	struct Pair* outer;
	int curline;
	enum {NIL,PAR,ATM} type;
	void* value;
}cptr;

typedef struct Pair
{
	struct Pair* prev;
	cptr car,cdr;
}pair;

typedef struct Atom
{
	pair* prev;
	enum{SYM=1,INT,CHR,DBL,STR} type;
	union
	{
		char* str;
		char chr;
		int32_t num;
		double dbl;
	} value;
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
	struct Env* next;
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

typedef struct ByteCode
{
	uint32_t size;
	char* code;
}byteCode;

typedef struct
{
	char* filename;
	FILE* file;
	env* glob;
	int curline;
}intpr;
#endif
