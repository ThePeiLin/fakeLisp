#ifndef SYNTAX_H
#define SYNTAX_H
#include"fakedef.h"

KeyWord* fklHasKeyWord(const FklAstCptr*,CompEnv*);
void fklAddKeyWord(const char*,CompEnv*);
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
int fklIsFuncCall(const FklAstCptr*,CompEnv*);
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
int fklIsKeyWord(const char*,CompEnv* curEnv);
int fklIsDeftypeExpression(const FklAstCptr*);
int fklIsGetfExpression(const FklAstCptr*);
int fklIsFlsymExpression(const FklAstCptr*);
#endif
