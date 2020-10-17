#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#define SYMUNDEFINE 1
#define SYNTAXERROR 2

typedef enum{NIL,SYM,STR,INT,CHR,BARY,DBL,PAIR,PRC,ATM} valueType;

typedef struct
{
	struct Pair* outer;
	int curline;
	valueType type;
	void* value;
}cptr;

typedef struct Pair
{
	struct Pair* prev;
	cptr car,cdr;
}prepair;

typedef struct Atom
{
	prepair* prev;
	valueType type;
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

typedef struct Macro
{
	cptr* format;
	cptr* express;
	struct Macro* next;
}macro;

typedef struct MacroSym
{
	char* symName;
	int (*Func)(const cptr*,const cptr*,const char*,env*);
	struct MacroSym* next;
}masym;

typedef struct ByteCode
{
	uint32_t size;
	char* code;
}byteCode;

typedef struct CompilerDefines
{
	int32_t count;
	char* symName;
	struct CompilerDefines* next;
}compDef;

typedef struct CompilerEnv
{
	struct CompilerEnv* prev;
	compDef* symbols;
}compEnv;

typedef struct RawProc
{
	int32_t count;
	byteCode* proc;
	struct RawProc* next;
}rawproc;

typedef struct
{
	char* filename;
	FILE* file;
	int curline;
	compEnv* glob;
	rawproc* procs;
}intpr;

typedef struct KeyWord
{
	char* word;
	struct KeyWord* next;
}keyWord;
#endif
