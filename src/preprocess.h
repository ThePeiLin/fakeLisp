#ifndef PREPARE_H
#define PREPARE_H
#include"fakedef.h"


PreDef* findDefine(const char*,const PreEnv*);
void addFunc(const char*,ErrorStatus (*)(AST_cptr*,PreEnv*,intpr*));
void freeAllFunc();
PreDef* addDefine(const char*,const AST_cptr*,PreEnv*);
PreDef* newDefines(const char*);
ErrorStatus (*(findFunc(const char*)))(AST_cptr*,PreEnv*,intpr*);
int retree(AST_cptr**,AST_cptr*);
ErrorStatus eval(AST_cptr*,PreEnv*,intpr*);
PreMacro* PreMacroMatch(const AST_cptr*);
int PreMacroExpand(AST_cptr*,intpr* inter);
//int addMacro(AST_cptr*,AST_cptr*);
int addMacro(AST_cptr*,ByteCode*,int32_t,RawProc*);
void freeMacroEnv();
void freeAllRule();
void unInitPreprocess();
void freeAllMacro();
PreMasym* findMasym(const char*);
int fmatcmp(const AST_cptr*,const AST_cptr*);
void addRule(const char*,int (*)(const AST_cptr*,const AST_cptr*,const char*,PreEnv*));
int M_ATOM(const AST_cptr*,const AST_cptr*,const char*,PreEnv*);
int M_PAIR(const AST_cptr*,const AST_cptr*,const char*,PreEnv*);
int M_ANY(const AST_cptr*,const AST_cptr*,const char*,PreEnv*);
int M_VAREPT(const AST_cptr*,const AST_cptr*,const char*,PreEnv*);
int M_COLT(const AST_cptr*,const AST_cptr*,const char*,PreEnv*);
int M_RAW(const AST_cptr*,const AST_cptr*,const char*,PreEnv*);
void initPreprocess();
void deleteMacro(AST_cptr*);
static const char* hasAnotherName(const char*);
static void addToList(AST_cptr*,const AST_cptr*);
static int addToDefine(const char*,PreEnv*,AST_cptr*);
static int getWordNum(const char*);
static char* getWord(const char*,int);
CompEnv* createMacroCompEnv(const AST_cptr*,CompEnv*);
static VMenv* castPreEnvToVMenv(PreEnv*,int32_t,VMenv*,VMheap*);
static int isVal(const char*);
#endif
