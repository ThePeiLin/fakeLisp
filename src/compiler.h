#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"
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

typedef struct Compiler
{
	intpr* inter;
	compEnv* glob;
	rawproc* procs;
}complr;

static int isTailCall(const char*,const cptr*);
complr* newCompiler(intpr*);
void freeCompiler(complr*);
compDef* addCompDef(compEnv*,const char*);
rawproc* newRawProc(int32_t);
rawproc* addRawProc(byteCode*,complr*);
compEnv* newCompEnv(compEnv*);
compDef* findCompDef(const char*,compEnv*);
void destroyCompEnv(compEnv*);
byteCode* compile(const cptr*,compEnv*,complr*);
static byteCode* compileConst(const cptr*,compEnv*,complr*);
static byteCode* compileQuote(const cptr*);
static byteCode* compileAtom(const cptr*);
static byteCode* compilePair(const cptr*);
static byteCode* compileListForm(const cptr*,compEnv*,complr*);
static byteCode* compileNil();
static byteCode* compileDef(const cptr*,compEnv*,complr*);
static byteCode* compileSetq(const cptr*,compEnv*,complr*);
static byteCode* compileSym(const cptr*,compEnv*,complr*);
static byteCode* compileCond(const cptr*,compEnv*,complr*);
static byteCode* compileLambda(const cptr*,compEnv*,complr*);
static byteCode* compileAnd(const cptr*,compEnv*,complr*);
static byteCode* compileOr(const cptr*,compEnv*,complr*);
static byteCode* createByteCode(unsigned int);
static byteCode* codeCat(const byteCode*,const byteCode*);
static void freeByteCode(byteCode*);
#endif
