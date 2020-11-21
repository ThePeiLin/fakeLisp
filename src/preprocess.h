#ifndef PREPARE_H
#define PREPARE_H
#include"fakedef.h"


PreDef* findDefine(const char*,const PreEnv*);
void addFunc(const char*,ErrorStatus (*)(ANS_cptr*,PreEnv*,intpr*));
PreDef* addDefine(const char*,const ANS_cptr*,PreEnv*);
PreDef* newDefines(const char*);
ErrorStatus (*(findFunc(const char*)))(ANS_cptr*,PreEnv*,intpr*);
int retree(ANS_cptr**,ANS_cptr*);
ErrorStatus eval(ANS_cptr*,PreEnv*,intpr*);
void addKeyWord(const char*);
KeyWord* hasKeyWord(const ANS_cptr*);
void printAllKeyWord();
PreMacro* PreMacroMatch(const ANS_cptr*);
int PreMacroExpand(ANS_cptr*,intpr* inter);
int addMacro(ANS_cptr*,ANS_cptr*);
PreMasym* findMasym(const char*);
int fmatcmp(const ANS_cptr*,const ANS_cptr*);
void addRule(const char*,int (*)(const ANS_cptr*,const ANS_cptr*,const char*,PreEnv*));
int M_ATOM(const ANS_cptr*,const ANS_cptr*,const char*,PreEnv*);
int M_PAIR(const ANS_cptr*,const ANS_cptr*,const char*,PreEnv*);
int M_ANY(const ANS_cptr*,const ANS_cptr*,const char*,PreEnv*);
int M_VAREPT(const ANS_cptr*,const ANS_cptr*,const char*,PreEnv*);
int M_COLT(const ANS_cptr*,const ANS_cptr*,const char*,PreEnv*);
int M_RAW(const ANS_cptr*,const ANS_cptr*,const char*,PreEnv*);
void initPreprocess(intpr*);
void deleteMacro(ANS_cptr*);
static const char* hasAnotherName(const char*);
static void addToList(ANS_cptr*,const ANS_cptr*);
static int addToDefine(const char*,PreEnv*,ANS_cptr*);
static int getWordNum(const char*);
static char* getWord(const char*,int);
#endif
