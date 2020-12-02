#ifndef SYNTAX_H
#define SYNTAX_H
#include"fakedef.h"
typedef struct SyntaxRule
{
	int (*check)(const AST_cptr*);
	struct SyntaxRule* next;
}synRule;

KeyWord* hasKeyWord(const AST_cptr*);
void addKeyWord(const char*);
void printAllKeyWord();
void freeAllKeyWord();
void addSynRule(int (*)(const AST_cptr*));
int (*checkAST(const AST_cptr*))(const AST_cptr*);
//void initSyntax();
int isLegal(const AST_cptr*);
int isPreprocess(const AST_cptr*);
int isDefExpression(const AST_cptr*);
int isSetqExpression(const AST_cptr*);
int isSetfExpression(const AST_cptr*);
int isLambdaExpression(const AST_cptr*);
int isCondExpression(const AST_cptr*);
int isConst(const AST_cptr*);
int isNil(const AST_cptr*);
int isListForm(const AST_cptr*);
int isSymbol(const AST_cptr*);
int isAndExpression(const AST_cptr*);
int isOrExpression(const AST_cptr*);
int isQuoteExpression(const AST_cptr*);
int isLoadExpression(const AST_cptr*);
int isImportExpression(const AST_cptr*);
int isDefmacroExpression(const AST_cptr*);
#endif
