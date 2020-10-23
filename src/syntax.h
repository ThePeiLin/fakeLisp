#ifndef SYNTAX_H
#define SYNTAX_H
#include"fakedef.h"
typedef struct SyntaxRule
{
	int (*check)(const Cptr*);
	struct SyntaxRule* next;
}synRule;

void addSynRule(int (*)(const Cptr*));
int (*checkAST(const Cptr*))(const Cptr*);
//void initSyntax();
int isLegal(const Cptr*);
int isPreprocess(const Cptr*);
int isDefExpression(const Cptr*);
int isSetqExpression(const Cptr*);
int isLambdaExpression(const Cptr*);
int isCondExpression(const Cptr*);
int isConst(const Cptr*);
int isNil(const Cptr*);
int isListForm(const Cptr*);
int isSymbol(const Cptr*);
int isAndExpression(const Cptr*);
int isOrExpression(const Cptr*);
int isQuoteExpression(const Cptr*);
#endif
