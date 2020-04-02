#ifndef PREPARE_H
#define PREPARE_H
#include"../fakedef.h"

typedef struct Macro
{
	cptr* format;
	cptr* express;
	struct Macro* next;
}macro;

typedef struct MacroSym
{
	char* symName;
	int (*Func)(cptr*);
	struct MacroSym* next;
}masym;

macro* macroMatch(cptr*);
int macroExpand(cptr*);
int addMacro(cptr*,cptr*);
masym* findMasym(const char*);
int fmatcmp(cptr*,cptr*);
void addRule(const char*,int (*)(cptr*));
int M_ATOM(cptr*);
int M_CELL(cptr*);
int M_ANY(cptr*);
void initPretreatment();
static void clearCount();
void deleteMacro(cptr*);
#endif
