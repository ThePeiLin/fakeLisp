#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"

ByteCode* compileFile(intpr*);
ByteCode* compile(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileConst(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileQuote(AST_cptr*);
ByteCode* compileAtom(AST_cptr*);
ByteCode* compilePair(AST_cptr*);
ByteCode* compileListForm(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileNil();
ByteCode* compileDef(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileSetq(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileSetf(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileSym(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileCond(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileLambda(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileAnd(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileOr(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
ByteCode* compileLoad(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
//ByteCode* compileImport(AST_cptr*,CompEnv*,intpr*,ErrorStatus*);
#endif
