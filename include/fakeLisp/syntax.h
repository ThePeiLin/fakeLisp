#ifndef SYNTAX_H
#define SYNTAX_H
#include"fakedef.h"

KeyWord* fklHasKeyWord(const AST_cptr*,CompEnv*);
void fklAddKeyWord(const char*,CompEnv*);
void fklPrintAllKeyWord();
void fklAddSynRule(int (*)(const AST_cptr*));
int (*checkAST(const AST_cptr*))(const AST_cptr*);
int fklIsValid(const AST_cptr*);
int fklIsPreprocess(const AST_cptr*);
int fklIsAnyExpression(const char* str,const AST_cptr* objCptr);
int fklIsDefExpression(const AST_cptr*);
int fklIsSetqExpression(const AST_cptr*);
int fklIsSetfExpression(const AST_cptr*);
int fklIsGetfExpression(const AST_cptr*);
int fklIsSzofExpression(const AST_cptr*);
int fklIsLambdaExpression(const AST_cptr*);
int fklIsBeginExpression(const AST_cptr*);
int fklIsCondExpression(const AST_cptr*);
int fklIsConst(const AST_cptr*);
int fklIsNil(const AST_cptr*);
int fklIsFuncCall(const AST_cptr*,CompEnv*);
int fklIsSymbol(const AST_cptr*);
int fklIsAndExpression(const AST_cptr*);
int fklIsOrExpression(const AST_cptr*);
int fklIsQuoteExpression(const AST_cptr*);
int fklIsUnquoteExpression(const AST_cptr*);
int fklIsQsquoteExpression(const AST_cptr*);
int fklIsUnqtespExpression(const AST_cptr*);
int fklIsLoadExpression(const AST_cptr*);
int fklIsImportExpression(const AST_cptr*);
int fklIsLibraryExpression(const AST_cptr*);
int fklIsExportExpression(const AST_cptr*);
int fklIsDefmacroExpression(const AST_cptr*);
int fklIsPrognExpression(const AST_cptr*);
int fklIsTryExpression(const AST_cptr*);
int fklIsCatchExpression(const AST_cptr*);
int fklIsKeyWord(const char*,CompEnv* curEnv);
int fklIsDeftypeExpression(const AST_cptr*);
int fklIsGetfExpression(const AST_cptr*);
int fklIsFlsymExpression(const AST_cptr*);
#endif
