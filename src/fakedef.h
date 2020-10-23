#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#define SYMUNDEFINE 1
#define SYNTAXERROR 2

typedef enum{NIL,SYM,STR,INT,CHR,BYTE,DBL,PAIR,PRC,ATM} ValueType;

typedef struct
{
	struct ANS_Pair* outer;
	int curline;
	ValueType type;
	void* value;
}ANS_cptr;

typedef struct ANS_Pair
{
	struct ANS_Pair* prev;
	ANS_cptr car,cdr;
}ANS_pair;

typedef struct ANS_atom
{
	ANS_pair* prev;
	ValueType type;
	union
	{
		char* str;
		char chr;
		int32_t num;
		double dbl;
	} value;
}ANS_atom;

typedef struct
{
	int status;
	ANS_cptr* place;
}ErrorStatus;

typedef struct Defines
{
	char* symName;
	ANS_cptr obj;//node or symbol or val
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
	ErrorStatus (*function)(ANS_cptr*,env*);
	struct NativeFunc* next;
}nativeFunc;

typedef struct Macro
{
	ANS_cptr* format;
	ANS_cptr* express;
	struct Macro* next;
}macro;

typedef struct MacroSym
{
	char* symName;
	int (*Func)(const ANS_cptr*,const ANS_cptr*,const char*,env*);
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
//	compEnv* curEnv;
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
