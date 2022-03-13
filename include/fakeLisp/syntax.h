#ifndef FK_SYNTAX_H
#define FK_SYNTAX_H
#include"compiler.h"
#include"ast.h"

#ifdef __cplusplus
extern "C" {
#endif

FklKeyWord* fklHasKeyWord(const FklAstCptr*,FklCompEnv*);
void fklAddKeyWord(const char*,FklCompEnv*);
void fklPrintAllKeyWord();
void fklAddSynRule(int (*)(const FklAstCptr*));
int (*checkAST(const FklAstCptr*))(const FklAstCptr*);
int fklIsValid(const FklAstCptr*);
int fklIsPreprocess(const FklAstCptr*);
int fklIsAnyExpression(const char* str,const FklAstCptr* objCptr);
int fklIsDefExpression(const FklAstCptr*);
int fklIsSetqExpression(const FklAstCptr*);
int fklIsSetfExpression(const FklAstCptr*);
int fklIsGetfExpression(const FklAstCptr*);
int fklIsSzofExpression(const FklAstCptr*);
int fklIsLambdaExpression(const FklAstCptr*);
int fklIsBeginExpression(const FklAstCptr*);
int fklIsCondExpression(const FklAstCptr*);
int fklIsConst(const FklAstCptr*);
int fklIsNil(const FklAstCptr*);
int fklIsFuncCall(const FklAstCptr*,FklCompEnv*);
int fklIsSymbol(const FklAstCptr*);
int fklIsAndExpression(const FklAstCptr*);
int fklIsOrExpression(const FklAstCptr*);
int fklIsQuoteExpression(const FklAstCptr*);
int fklIsUnquoteExpression(const FklAstCptr*);
int fklIsQsquoteExpression(const FklAstCptr*);
int fklIsUnqtespExpression(const FklAstCptr*);
int fklIsLoadExpression(const FklAstCptr*);
int fklIsImportExpression(const FklAstCptr*);
int fklIsLibraryExpression(const FklAstCptr*);
int fklIsExportExpression(const FklAstCptr*);
int fklIsDefmacroExpression(const FklAstCptr*);
int fklIsPrognExpression(const FklAstCptr*);
int fklIsTryExpression(const FklAstCptr*);
int fklIsCatchExpression(const FklAstCptr*);
int fklIsKeyWord(const char*,FklCompEnv* curEnv);
int fklIsDeftypeExpression(const FklAstCptr*);
int fklIsGetfExpression(const FklAstCptr*);
int fklIsFlsymExpression(const FklAstCptr*);

#ifdef __cplusplus
}
#endif

#endif
