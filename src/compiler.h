#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"

byteCode* compile(ANS_cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileConst(ANS_cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileQuote(ANS_cptr*);
byteCode* compileAtom(ANS_cptr*);
byteCode* compilePair(ANS_cptr*);
byteCode* compileListForm(ANS_cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileNil();
byteCode* compileDef(ANS_cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileSetq(ANS_cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileSym(ANS_cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileCond(ANS_cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileLambda(ANS_cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileAnd(ANS_cptr*,compEnv*,intpr*,ErrorStatus*);
byteCode* compileOr(ANS_cptr*,compEnv*,intpr*,ErrorStatus*);
#endif
