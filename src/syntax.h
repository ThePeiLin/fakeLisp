#ifndef SYNTAX_H
#define SYNTAX_H
#include"fakedef.h"
typedef struct SyntaxRule
{
	int (*check)(const ANS_cptr*);
	struct SyntaxRule* next;
}synRule;

KeyWord* hasKeyWord(const ANS_cptr*);
void addKeyWord(const char*);
void printAllKeyWord();
void freeAllKeyWord();
void addSynRule(int (*)(const ANS_cptr*));
int (*checkAST(const ANS_cptr*))(const ANS_cptr*);
//void initSyntax();
int isLegal(const ANS_cptr*);
int isPreprocess(const ANS_cptr*);
int isDefExpression(const ANS_cptr*);
int isSetqExpression(const ANS_cptr*);
int isSetfExpression(const ANS_cptr*);
int isLambdaExpression(const ANS_cptr*);
int isCondExpression(const ANS_cptr*);
int isConst(const ANS_cptr*);
int isNil(const ANS_cptr*);
int isListForm(const ANS_cptr*);
int isSymbol(const ANS_cptr*);
int isAndExpression(const ANS_cptr*);
int isOrExpression(const ANS_cptr*);
int isQuoteExpression(const ANS_cptr*);
int isLoadExpression(const ANS_cptr*);
int isImportExpression(const ANS_cptr*);
int isDefmacroExpression(const ANS_cptr*);
#endif
