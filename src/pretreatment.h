#ifndef PREPARE_H
#define PREPARE_H
#include"fakedef.h"

typedef struct Macro
{
	cptr* format;
	cptr* express;
	struct Macro* next;
}macro;

typedef struct MacroSym
{
	char* symName;
	int (*Func)(const cptr*,const char*);
	struct MacroSym* next;
}masym;

macro* macroMatch(const cptr*);
int macroExpand(cptr*);
int addMacro(cptr*,cptr*);
masym* findMasym(const char*);
int fmatcmp(const cptr*,const cptr*);
void addRule(const char*,int (*)(const cptr*,const char*));
int M_ATOM(const cptr*,const char*);
int M_CELL(const cptr*,const char*);
int M_ANY(const cptr*,const char*);
void initPretreatment();
static void clearCount();
static const char* hasAnotherName(const char*);
void deleteMacro(cptr*);
#endif
