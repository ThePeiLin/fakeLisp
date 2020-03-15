#ifndef PREPARE_H
#define PREPARE_H
#include"fakedef.h"

typedef struct Macro
{
	cptr* format;
	cptr* express;
	struct macro* next;
}macro;

typedef struct MacroSym
{
	char* symName;
	int (*Func)(cptr*);
	struct MacroSym* next;
}masym;

int macroMatch(cptr*);
cptr* macroExpand(cptr*);
macro* createMacro(cptr*,cptr*);
int addMacro(macro*);
masym* findMasym(const char*)
#endif
