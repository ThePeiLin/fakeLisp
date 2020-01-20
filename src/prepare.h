#ifndef PREPARE_H
#define PREPARE_H
#include"fakedef.h"

typedef struct Matching
{
	branch* format;
	branch* express;
	struct Match* next;
}matching;

typedef struct Macro
{
	char* name;
	matching* term;
	struct macro* next;
}macro;

int macroMatch(branch*);
branch* macroExpand(branch*);
macro* findMacro(const char*);
macro* createMacro(const char*,branch*);
#endif
