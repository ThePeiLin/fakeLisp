#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"

#define FREE_ALL_LINE_NUMBER_TABLE(l,s) {int32_t i=0;\
	for(;i<(s);i++)\
	freeLineNumTabNode((l)[i]);\
}

PreMacro* PreMacroMatch(const AST_cptr*,PreEnv**,CompEnv*,CompEnv**);
int PreMacroExpand(AST_cptr*,CompEnv*,Intpr* inter);
int addMacro(AST_cptr*,ByteCodelnt*,CompEnv* curEnv);
void freeMacroEnv();
CompEnv* createMacroCompEnv(const AST_cptr*,CompEnv*,SymbolTable*);
int retree(AST_cptr**,AST_cptr*);
void unInitPreprocess();
void initGlobKeyWord(CompEnv*);
StringMatchPattern* addStringPattern(char**,int32_t,AST_cptr*,Intpr*);

ByteCodelnt* compileFile(Intpr*,int evalIm,int*);
ByteCodelnt* compile(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileQsquote(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileUnquote(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileUnqtesp(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileConst(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCode* compileQuote(AST_cptr*,SymbolTable*);
ByteCode* compileAtom(AST_cptr*);
ByteCode* compilePair(AST_cptr*);
ByteCode* compileNil();
ByteCodelnt* compileFuncCall(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileDef(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileSetq(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileSetf(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileSym(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileCond(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileLambda(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileBegin(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileAnd(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileOr(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileLoad(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileProgn(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileImport(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
ByteCodelnt* compileTry(AST_cptr*,CompEnv*,Intpr*,ErrorStatus*,int evalIm);
#endif
