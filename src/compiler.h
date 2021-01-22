#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"


PreMacro* PreMacroMatch(const AST_cptr*);
int PreMacroExpand(AST_cptr*,Intpr* inter);
int addMacro(AST_cptr*,ByteCode*,int32_t,RawProc*);
static int MacroPatternCmp(const AST_cptr*,const AST_cptr*);
void freeMacroEnv();
void freeAllMacro();
static int fmatcmp(const AST_cptr*,const AST_cptr*);
int MacroPatternCmp(const AST_cptr*,const AST_cptr*);
CompEnv* createMacroCompEnv(const AST_cptr*,CompEnv*);
static int isVal(const char*);
void addFunc(const char*,ErrorStatus (*)(AST_cptr*,PreEnv*,Intpr*));
void freeAllFunc();
ErrorStatus (*(findFunc(const char*)))(AST_cptr*,PreEnv*,Intpr*);
int retree(AST_cptr**,AST_cptr*);
ErrorStatus eval(AST_cptr*,PreEnv*,Intpr*);
void unInitPreprocess();
void initPreprocess();
static ErrorStatus N_import(AST_cptr*,PreEnv*,Intpr*);
static ErrorStatus N_defmacro(AST_cptr*,PreEnv*,Intpr*);
StringMatchPattern* addStringPattern(char**,int32_t,AST_cptr*,Intpr*);
static CompEnv* createPatternCompEnv(char**,int32_t,CompEnv*);

ByteCode* compileFile(Intpr*);
ByteCode* compile(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileConst(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileQuote(AST_cptr*);
ByteCode* compileAtom(AST_cptr*);
ByteCode* compilePair(AST_cptr*);
ByteCode* compileListForm(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileNil();
ByteCode* compileDef(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileSetq(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileSetf(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileSym(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileCond(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileLambda(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileAnd(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileOr(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
ByteCode* compileLoad(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*);
#endif
