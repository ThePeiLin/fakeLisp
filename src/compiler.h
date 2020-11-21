#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"

ByteCode* compileFile(intpr*);
ByteCode* compile(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileConst(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileQuote(ANS_cptr*);
ByteCode* compileAtom(ANS_cptr*);
ByteCode* compilePair(ANS_cptr*);
ByteCode* compileListForm(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileNil();
ByteCode* compileDef(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileSetq(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileSetf(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileSym(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileCond(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileLambda(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileAnd(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileOr(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileLoad(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
//ByteCode* compileImport(ANS_cptr*,CompEnv*,intpr*,ErrorStatus*);
#endif
