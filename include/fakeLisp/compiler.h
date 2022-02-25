#ifndef COMPILER_H
#define COMPILER_H
#include"fakedef.h"

#define FKL_FREE_ALL_LINE_NUMBER_TABLE(l,s) {int32_t i=0;\
	for(;i<(s);i++)\
	fklFreeLineNumTabNode((l)[i]);\
}

FklPreMacro* fklPreMacroMatch(const FklAstCptr*,FklPreEnv**,FklCompEnv*,FklCompEnv**);
int fklPreMacroExpand(FklAstCptr*,FklCompEnv*,FklIntpr* inter);
int fklAddMacro(FklAstCptr*,FklByteCodelnt*,FklCompEnv* curEnv);
void fklFreeMacroEnv();
FklCompEnv* fklCreateMacroCompEnv(const FklAstCptr*,FklCompEnv*);
int fklRetree(FklAstCptr**,FklAstCptr*);
void fklUnInitPreprocess();
void fklInitGlobKeyWord(FklCompEnv*);
void fklInitNativeDefTypes(FklVMDefTypes*);
FklStringMatchPattern* fklAddStringPattern(char**,int32_t,FklAstCptr*,FklIntpr*);
FklStringMatchPattern* fklAddReDefStringPattern(char**,int32_t,FklAstCptr*,FklIntpr*);
void fklInitBuiltInStringPattern(void);

FklByteCodelnt* fklCompileFile(FklIntpr*,int evalIm,int*);
FklByteCodelnt* fklCompile(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileQsquote(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileUnquote(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileUnqtesp(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileConst(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCode* fklCompileQuote(FklAstCptr*);
FklByteCode* fklCompileAtom(FklAstCptr*);
FklByteCode* fklCompilePair(FklAstCptr*);
FklByteCode* fklCompileNil();
FklByteCodelnt* fklCompileFuncCall(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileDef(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileSetq(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileSetf(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileGetf(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileSzof(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileSym(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileCond(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileLambda(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileBegin(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileAnd(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileOr(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileLoad(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileProgn(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileImport(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileTry(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileFlsym(FklAstCptr*,FklCompEnv*,FklIntpr*,FklErrorState*,int evalIm);
#endif
