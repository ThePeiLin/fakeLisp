#ifndef PREPARE_H
#define PREPARE_H
#include"fakedef.h"

typedef struct Pattern
{
	cell* format;
	cell* express;
	struct Matching* next;
}pattern;

typedef struct Macro
{
	char* name;
	pattern* term;
	struct macro* next;
}macro;

int macroMatch(cell*);
cell* macroExpand(cell*);
macro* findMacro(const char*);
macro* createMacro(const char*,cell*);
#endif
