#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"


PreMacro* PreMacroMatch(const AST_cptr*);
int PreMacroExpand(AST_cptr*,Intpr* inter);
int addMacro(AST_cptr*,ByteCode*,RawProc*);
void freeMacroEnv();
void freeAllMacro();
CompEnv* createMacroCompEnv(const AST_cptr*,CompEnv*,SymbolTable*);
void addFunc(const char*,ErrorStatus (*)(AST_cptr*,PreEnv*,Intpr*));
void freeAllFunc();
ErrorStatus (*(findFunc(const char*)))(AST_cptr*,PreEnv*,Intpr*);
int retree(AST_cptr**,AST_cptr*);
ErrorStatus eval(AST_cptr*,PreEnv*,Intpr*);
void unInitPreprocess();
void initPreprocess();
StringMatchPattern* addStringPattern(char**,int32_t,AST_cptr*,Intpr*);

ByteCode* compileFile(Intpr*,int evalIm);
ByteCode* compile(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCode* compileConst(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileQuote(AST_cptr*);
ByteCode* compileAtom(AST_cptr*);
ByteCode* compilePair(AST_cptr*);
ByteCode* compileFuncCall(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCode* compileNil();
ByteCode* compileDef(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCode* compileSetq(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCode* compileSetf(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCode* compileSym(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCode* compileCond(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCode* compileLambda(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCode* compileAnd(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCode* compileOr(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCode* compileLoad(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
#endif
