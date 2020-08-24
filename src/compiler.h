#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"

static int isTailCall(const char*,const cptr*);
void printByteCode(const byteCode*);
byteCode* compile(cptr*,compEnv*,intpr*,errorStatus*);
byteCode* compileConst(cptr*,compEnv*,intpr*,errorStatus*);
byteCode* compileQuote(cptr*);
byteCode* compileAtom(cptr*);
byteCode* compilePair(cptr*);
byteCode* compileListForm(cptr*,compEnv*,intpr*,errorStatus*);
byteCode* compileNil();
byteCode* compileDef(cptr*,compEnv*,intpr*,errorStatus*);
byteCode* compileSetq(cptr*,compEnv*,intpr*,errorStatus*);
byteCode* compileSym(cptr*,compEnv*,intpr*,errorStatus*);
byteCode* compileCond(cptr*,compEnv*,intpr*,errorStatus*);
byteCode* compileLambda(cptr*,compEnv*,intpr*,errorStatus*);
byteCode* compileAnd(cptr*,compEnv*,intpr*,errorStatus*);
byteCode* compileOr(cptr*,compEnv*,intpr*,errorStatus*);
#endif
