#ifndef SYNTAX_H
#define SYNTAX_H
#include"fakedef.h"

KeyWord* hasKeyWord(const AST_cptr*);
void addKeyWord(const char*);
void printAllKeyWord();
void freeAllKeyWord();
void addSynRule(int (*)(const AST_cptr*));
int (*checkAST(const AST_cptr*))(const AST_cptr*);
int isValid(const AST_cptr*);
int isPreprocess(const AST_cptr*);
int isDefExpression(const AST_cptr*);
int isSetqExpression(const AST_cptr*);
int isSetfExpression(const AST_cptr*);
int isLambdaExpression(const AST_cptr*);
int isBeginExpression(const AST_cptr*);
int isCondExpression(const AST_cptr*);
int isConst(const AST_cptr*);
int isNil(const AST_cptr*);
int isFuncCall(const AST_cptr*);
int isSymbol(const AST_cptr*);
int isAndExpression(const AST_cptr*);
int isOrExpression(const AST_cptr*);
int isQuoteExpression(const AST_cptr*);
int isUnquoteExpression(const AST_cptr*);
int isQsquoteExpression(const AST_cptr*);
int isUnqtespExpression(const AST_cptr*);
int isLoadExpression(const AST_cptr*);
int isImportExpression(const AST_cptr*);
int isLibraryExpression(const AST_cptr*);
int isExportExpression(const AST_cptr*);
int isDefmacroExpression(const AST_cptr*);
int isProcExpression(const AST_cptr*);
int isKeyWord(const char*);
#endif
