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
exCode* compiler(cptr*);
int isTailCall(comDef*);
void constFolding(cptr*);
int analyseSyntaxError(cptr*);//t
cptr* analyseAST(cptr*);//t
static cptr** divideAST(cptr*);//t
static int countAST(cptr*);//t
static void deleteASTs(cptr**,int);
static int isFoldable(cptr**);
#endif
