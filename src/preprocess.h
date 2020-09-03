#ifndef PREPARE_H
#define PREPARE_H
#include"fakedef.h"


defines* findDefine(const char*,const env*);
void addFunc(const char*,errorStatus (*)(cptr*,env*));
defines* addDefine(const char*,const cptr*,env*);
errorStatus (*(findFunc(const char*)))(cptr*,env*);
int retree(cptr**,cptr*);
errorStatus eval(cptr*,env*);
void addKeyWord(const char*);
keyWord* hasKeyWord(const cptr*);
void printAllKeyWord();
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
int M_RAW(const cptr*,const cptr*,const char*,env*);
void initPreprocess();
void deleteMacro(cptr*);
static const char* hasAnotherName(const char*);
static void addToList(cptr*,const cptr*);
static int addToDefine(const char*,env*,cptr*);
static int getWordNum(const char*);
static char* getWord(const char*,int);
#endif
