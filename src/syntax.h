#ifndef SYNTAX_H
#define SYNTAX_H
#include"fakedef.h"
typedef struct SyntaxRule
{
	int (*check)(const cptr*);
	struct SyntaxRule* next;
}synRule;

void addSynRule(int (*)(const cptr*));
int (*checkAST(const cptr*))(const cptr*);
//void initSyntax();
int isLegal(const cptr*);
int isPreprocess(const cptr*);
int isDefExpression(const cptr*);
int isSetqExpression(const cptr*);
int isLambdaExpression(const cptr*);
int isCondExpression(const cptr*);
int isConst(const cptr*);
int isListForm(const cptr*);
int isSymbol(const cptr*);
int isAndExpression(const cptr*);
int isOrExpression(const cptr*);
int isQuoteExpression(const cptr*);
#endif
