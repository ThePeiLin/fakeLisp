#ifndef PREPARE_H
#define PREPARE_H
#include"fakedef.h"

typedef struct Pattern
{
	cptr* format;
	cptr* express;
	struct Matching* next;
}pattern;

typedef struct Macro
{
	pattern* term;
	struct macro* next;
}macro;

int macroMatch(cptr*);
cptr* macroExpand(cptr*);
macro* findMacro(const char*);
macro* createMacro(cptr*,cptr*);
int addMacro(macro*);
#endif
