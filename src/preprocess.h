#ifndef PREPARE_H
#define PREPARE_H
#include"fakedef.h"


defines* findDefine(const char*,const env*);
void addFunc(const char*,ErrorStatus (*)(ANS_cptr*,env*));
defines* addDefine(const char*,const ANS_cptr*,env*);
defines* newDefines(const char*);
ErrorStatus (*(findFunc(const char*)))(ANS_cptr*,env*);
int retree(ANS_cptr**,ANS_cptr*);
ErrorStatus eval(ANS_cptr*,env*);
void addKeyWord(const char*);
keyWord* hasKeyWord(const ANS_cptr*);
void printAllKeyWord();
macro* macroMatch(const ANS_cptr*);
int macroExpand(ANS_cptr*);
int addMacro(ANS_cptr*,ANS_cptr*);
masym* findMasym(const char*);
int fmatcmp(const ANS_cptr*,const ANS_cptr*);
void addRule(const char*,int (*)(const ANS_cptr*,const ANS_cptr*,const char*,env*));
int M_ATOM(const ANS_cptr*,const ANS_cptr*,const char*,env*);
int M_PAIR(const ANS_cptr*,const ANS_cptr*,const char*,env*);
int M_ANY(const ANS_cptr*,const ANS_cptr*,const char*,env*);
int M_VAREPT(const ANS_cptr*,const ANS_cptr*,const char*,env*);
int M_COLT(const ANS_cptr*,const ANS_cptr*,const char*,env*);
int M_RAW(const ANS_cptr*,const ANS_cptr*,const char*,env*);
void initPreprocess();
void deleteMacro(ANS_cptr*);
static const char* hasAnotherName(const char*);
static void addToList(ANS_cptr*,const ANS_cptr*);
static int addToDefine(const char*,env*,ANS_cptr*);
static int getWordNum(const char*);
static char* getWord(const char*,int);
#endif
