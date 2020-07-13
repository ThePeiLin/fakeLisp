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
	int (*Func)(const cptr*,const cptr*,const char*,env*);
	struct MacroSym* next;
}masym;

macro* macroMatch(const cptr*);
int macroExpand(cptr*);
int addMacro(cptr*,cptr*);
masym* findMasym(const char*);
int fmatcmp(const cptr*,const cptr*);
void addRule(const char*,int (*)(const cptr*,const cptr*,const char*,env*));
int M_ATOM(const cptr*,const cptr*,const char*,env*);
int M_PAIR(const cptr*,const cptr*,const char*,env*);
int M_ANY(const cptr*,const cptr*,const char*,env*);
int M_VAREPT(const cptr*,const cptr*,const char*,env*);
int M_COLT(const cptr*,const cptr*,const char*,env*);
void initPretreatment();
void deleteMacro(cptr*);
static const char* hasAnotherName(const char*);
static void addToList(cptr*,const cptr*);
static int addToDefine(const char*,env*,cptr*);
#endif
