#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"

#define FREE_ALL_LINE_NUMBER_TABLE(l,s) {int32_t i=0;\
	for(;i<(s);i++)\
	fklFreeLineNumTabNode((l)[i]);\
}

PreMacro* fklPreMacroMatch(const FklAstCptr*,PreEnv**,CompEnv*,CompEnv**);
int fklPreMacroExpand(FklAstCptr*,CompEnv*,Intpr* inter);
int fklAddMacro(FklAstCptr*,ByteCodelnt*,CompEnv* curEnv);
void fklFreeMacroEnv();
CompEnv* fklCreateMacroCompEnv(const FklAstCptr*,CompEnv*);
int fklRetree(FklAstCptr**,FklAstCptr*);
void fklUnInitPreprocess();
void fklInitGlobKeyWord(CompEnv*);
void fklInitNativeDefTypes(VMDefTypes*);
StringMatchPattern* fklAddStringPattern(char**,int32_t,FklAstCptr*,Intpr*);
StringMatchPattern* fklAddReDefStringPattern(char**,int32_t,FklAstCptr*,Intpr*);
void fklInitBuiltInStringPattern(void);

ByteCodelnt* fklCompileFile(Intpr*,int evalIm,int*);
ByteCodelnt* fklCompile(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileQsquote(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileUnquote(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileUnqtesp(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileConst(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCode* fklCompileQuote(FklAstCptr*);
ByteCode* fklCompileAtom(FklAstCptr*);
ByteCode* fklCompilePair(FklAstCptr*);
ByteCode* fklCompileNil();
ByteCodelnt* fklCompileFuncCall(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileDef(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileSetq(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileSetf(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileGetf(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileSzof(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileSym(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileCond(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileLambda(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileBegin(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileAnd(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileOr(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileLoad(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileProgn(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileImport(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileTry(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
ByteCodelnt* fklCompileFlsym(FklAstCptr*,CompEnv*,Intpr*,ErrorState*,int evalIm);
#endif
