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
int constFolding(cptr*);
cptr* checkAST(cptr*);
int analyseSyntaxError(cptr*);
cptr* analyseAST(cptr*);
static cptr** divideAST(cptr*);
static int countAST(cptr*);
#endif
