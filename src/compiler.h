#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"


PreDef* findDefine(const char*,const PreEnv*);
PreDef* addDefine(const char*,const AST_cptr*,PreEnv*);
PreDef* newDefines(const char*);
PreMacro* PreMacroMatch(const AST_cptr*);
int PreMacroExpand(AST_cptr*,intpr* inter);
int addMacro(AST_cptr*,ByteCode*,int32_t,RawProc*);
static int MacroPatternCmp(const AST_cptr*,const AST_cptr*);
void freeMacroEnv();
void freeAllMacro();
static int fmatcmp(const AST_cptr*,const AST_cptr*);
int MacroPatternCmp(const AST_cptr*,const AST_cptr*);
CompEnv* createMacroCompEnv(const AST_cptr*,CompEnv*);
VMenv* castPreEnvToVMenv(PreEnv*,int32_t,VMenv*,VMheap*);
static int isVal(const char*);
void addFunc(const char*,ErrorStatus (*)(AST_cptr*,PreEnv*,intpr*));
void freeAllFunc();
ErrorStatus (*(findFunc(const char*)))(AST_cptr*,PreEnv*,intpr*);
int retree(AST_cptr**,AST_cptr*);
ErrorStatus eval(AST_cptr*,PreEnv*,intpr*);
void unInitPreprocess();
void initPreprocess();
static ErrorStatus N_import(AST_cptr*,PreEnv*,intpr*);
static ErrorStatus N_defmacro(AST_cptr*,PreEnv*,intpr*);

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
