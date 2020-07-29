#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"
typedef struct CompilerDefines
{
	int count;
	char* symName;
	cptr* obj;
	struct CompilerDefines* next;
}compDef;

typedef struct CompilerEnv
{
	struct CompilerEnv* prev;
	comDef* symbols;
}compEnv;

int newCompiler(int,FILE*);
byteCode* compiler(const cptr*);
int isTailCall(comDef*);
status analyseAST(cptr*);//t
cptr* checkAst(const cptr*);
static cptr** divideAST(cptr*);//t
static int countAST(cptr*);//t
static void deleteASTs(cptr**,int);
static byteCode* compileConstant(const cptr*);
static byteCode* compileNum(const cptr*);
static byteCode* appendByteCode(const byteCode*,const byteCode*);
static void freeByteCode(byteCode*);
#endif
