#ifndef PREPARE_H
#define PREPARE_H
#include"fakedef.h"


defines* findDefine(const char*,const env*);
void addFunc(const char*,ErrorStatus (*)(Cptr*,env*));
defines* addDefine(const char*,const Cptr*,env*);
defines* newDefines(const char*);
ErrorStatus (*(findFunc(const char*)))(Cptr*,env*);
int retree(Cptr**,Cptr*);
ErrorStatus eval(Cptr*,env*);
void addKeyWord(const char*);
keyWord* hasKeyWord(const Cptr*);
void printAllKeyWord();
macro* macroMatch(const Cptr*);
int macroExpand(Cptr*);
int addMacro(Cptr*,Cptr*);
masym* findMasym(const char*);
int fmatcmp(const Cptr*,const Cptr*);
void addRule(const char*,int (*)(const Cptr*,const Cptr*,const char*,env*));
int M_ATOM(const Cptr*,const Cptr*,const char*,env*);
int M_PAIR(const Cptr*,const Cptr*,const char*,env*);
int M_ANY(const Cptr*,const Cptr*,const char*,env*);
int M_VAREPT(const Cptr*,const Cptr*,const char*,env*);
int M_COLT(const Cptr*,const Cptr*,const char*,env*);
int M_RAW(const Cptr*,const Cptr*,const char*,env*);
void initPreprocess();
void deleteMacro(Cptr*);
static const char* hasAnotherName(const char*);
static void addToList(Cptr*,const Cptr*);
static int addToDefine(const char*,env*,Cptr*);
static int getWordNum(const char*);
static char* getWord(const char*,int);
#endif
