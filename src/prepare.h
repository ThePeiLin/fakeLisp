#ifndef PREPARE_H
#define PREPARE_H
#include"fakedef.h"

typedef struct Matching
{
	cell* format;
	cell* express;
	struct Matching* next;
}matching;

typedef struct Macro
{
	char* name;
	matching* term;
	struct macro* next;
}macro;

int macroMatch(cell*);
cell* macroExpand(cell*);
macro* findMacro(const char*);
macro* createMacro(const char*,cell*);
#endif
