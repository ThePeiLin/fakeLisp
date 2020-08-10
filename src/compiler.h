#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"
typedef struct CompilerDefines
{
	int count;
	char* symName;
	cptr obj;
	struct CompilerDefines* next;
}compDef;

typedef struct CompilerEnv
{
	struct CompilerEnv* prev;
	struct CompilerEnv* next;
	compDef* symbols;
}compEnv;

typedef struct Compiler
{
	intpr* inter;
	compEnv* glob;
}complr;

static int isTailCall(compDef*);
complr* newCompiler(intpr*);
void destroyCompiler(complr*);
compDef* addCompDef(compEnv*,const char*,const cptr*);
compEnv* newCompEnv(compEnv*);
compDef* findCompDef(const char*,compEnv*);
void destroyCompEnv(compEnv*);
byteCode* compile(const cptr*);
static byteCode* compileConst(const cptr*);
static byteCode* compileQuote(const cptr*);
static byteCode* compileAtom(const cptr*);
static byteCode* compilePair(const cptr*);
static byteCode* compileListForm(const cptr*);
static byteCode* compileNil();
static byteCode* compileDef(const cptr*);
static byteCode* compileSetq(const cptr*);
static byteCode* compileSym(const cptr*);
static byteCode* compileCond(const cptr*);
static byteCode* compileLambda(const cptr*);
static byteCode* compileAnd(const cptr*);
static byteCode* compileOr(const cptr*);
static byteCode* createByteCode(unsigned int);
static byteCode* codeCat(const byteCode*,const byteCode*);
static void freeByteCode(byteCode*);
#endif
