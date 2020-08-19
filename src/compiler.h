#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"

static int isTailCall(const char*,const cptr*);
void printByteCode(const byteCode*);
byteCode* compile(cptr*,compEnv*,intpr*);
byteCode* compileConst(cptr*,compEnv*,intpr*);
byteCode* compileQuote(cptr*);
byteCode* compileAtom(cptr*);
byteCode* compilePair(cptr*);
byteCode* compileListForm(cptr*,compEnv*,intpr*);
byteCode* compileNil();
byteCode* compileDef(cptr*,compEnv*,intpr*);
byteCode* compileSetq(cptr*,compEnv*,intpr*);
byteCode* compileSym(cptr*,compEnv*,intpr*);
byteCode* compileCond(cptr*,compEnv*,intpr*);
byteCode* compileLambda(cptr*,compEnv*,intpr*);
byteCode* compileAnd(cptr*,compEnv*,intpr*);
byteCode* compileOr(cptr*,compEnv*,intpr*);
#endif
