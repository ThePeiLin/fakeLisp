#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"

byteCode* compile(Cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileConst(Cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileQuote(Cptr*);
byteCode* compileAtom(Cptr*);
byteCode* compilePair(Cptr*);
byteCode* compileListForm(Cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileNil();
byteCode* compileDef(Cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileSetq(Cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileSym(Cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileCond(Cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileLambda(Cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileAnd(Cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileOr(Cptr*,compEnv*,intpr*,ErrorStatus*);
#endif
