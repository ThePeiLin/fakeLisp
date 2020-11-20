#ifndef FAKEDEF_H
#define FAKEDEF_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#define SYMUNDEFINE 1
#define SYNTAXERROR 2
#define ILLEGALEXPR 3
#define CIRCULARLOAD 4

typedef enum{NIL,INT,CHR,DBL,SYM,STR,BYTE,PRC,PAIR,ATM} ValueType;

typedef struct
{
	int32_t refcount;
	int32_t size;
	uint8_t* arry;
}ByteArry;

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
		ByteArry byte;
	} value;
}ANS_atom;

typedef struct
{
	int status;
	ANS_cptr* place;
}ErrorStatus;

typedef struct Pre_Def
{
	char* symName;
	ANS_cptr obj;//node or symbol or val
	struct Pre_Def* next;
}PreDef;

typedef struct Pre_Env
{
	struct Pre_Env* prev;
	PreDef* symbols;
	struct Pre_Env* next;
}PreEnv;

typedef struct Pre_Func//function and form
{
	char* functionName;
	ErrorStatus (*function)(ANS_cptr*,PreEnv*);
	struct Pre_Func* next;
}PreFunc;

typedef struct Pre_Macro
{
	ANS_cptr* format;
	ANS_cptr* express;
	struct Pre_Macro* next;
}PreMacro;

typedef struct Pre_MacroSym
{
	char* symName;
	int (*Func)(const ANS_cptr*,const ANS_cptr*,const char*,PreEnv*);
	struct Pre_MacroSym* next;
}PreMasym;

typedef struct
{
	int32_t size;
	char* code;
}ByteCode;

typedef struct Comp_Def
{
	int32_t count;
	char* symName;
	struct Comp_Def* next;
}CompDef;

typedef struct Comp_Env
{
	struct Comp_Env* prev;
	CompDef* symbols;
}CompEnv;

typedef struct Raw_Proc
{
	int32_t count;
	ByteCode* proc;
//	CompEnv* curEnv;
	struct Raw_Proc* next;
}RawProc;

typedef struct Interpreter
{
	char* filename;
	FILE* file;
	int curline;
	CompEnv* glob;
	RawProc* procs;
	struct Interpreter* prev;
}intpr;

typedef struct Key_Word
{
	char* word;
	struct Key_Word* next;
}KeyWord;
#endif
