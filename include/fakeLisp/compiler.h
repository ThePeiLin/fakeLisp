#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"

#define FREE_ALL_LINE_NUMBER_TABLE(l,s) {int32_t i=0;\
	for(;i<(s);i++)\
	fklFreeLineNumTabNode((l)[i]);\
}

PreMacro* fklPreMacroMatch(const AST_cptr*,PreEnv**,CompEnv*,CompEnv**);
int fklPreMacroExpand(AST_cptr*,CompEnv*,Intpr* inter);
int fklAddMacro(AST_cptr*,ByteCodelnt*,CompEnv* curEnv);
void fklFreeMacroEnv();
CompEnv* fklCreateMacroCompEnv(const AST_cptr*,CompEnv*);
int fklRetree(AST_cptr**,AST_cptr*);
void fklUnInitPreprocess();
void fklInitGlobKeyWord(CompEnv*);
void fklInitNativeDefTypes(VMDefTypes*);
StringMatchPattern* fklAddStringPattern(char**,int32_t,AST_cptr*,Intpr*);
StringMatchPattern* fklAddReDefStringPattern(char**,int32_t,AST_cptr*,Intpr*);
void fklInitBuiltInStringPattern(void);

ByteCodelnt* fklCompileFile(Intpr*,int evalIm,int*);
ByteCodelnt* fklCompile(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileQsquote(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileUnquote(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileUnqtesp(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileConst(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCode* fklCompileQuote(AST_cptr*);
ByteCode* fklCompileAtom(AST_cptr*);
ByteCode* fklCompilePair(AST_cptr*);
ByteCode* fklCompileNil();
ByteCodelnt* fklCompileFuncCall(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileDef(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileSetq(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileSetf(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileGetf(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileSzof(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileSym(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileCond(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileLambda(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileBegin(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileAnd(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileOr(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileLoad(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileProgn(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileImport(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileTry(AST_cptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
#endif
