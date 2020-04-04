#ifndef SYNTAX_H
#define SYNTAX_H
#include"fakedef.h"
typedef struct SyntaxRule
{
	char* tokenName;
	int (*check)(cptr*);
	struct SyntaxRule* next;
}synRule;

void addSynRule(const char*,int (*)(cptr*));
cptr* checkAST(cptr*);
synRule* findSynRule(const char*);
#endif
