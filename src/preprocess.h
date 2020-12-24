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
int addMacro(AST_cptr*,ByteCode*,int32_t,RawProc*);
static int MacroPatternCmp(const AST_cptr*,const AST_cptr*);
void freeMacroEnv();
void unInitPreprocess();
void freeAllMacro();
int fmatcmp(const AST_cptr*,const AST_cptr*);
int MacroPatternCmp(const AST_cptr*,const AST_cptr*);
void initPreprocess();
CompEnv* createMacroCompEnv(const AST_cptr*,CompEnv*);
static VMenv* castPreEnvToVMenv(PreEnv*,int32_t,VMenv*,VMheap*);
static ErrorStatus N_import(AST_cptr*,PreEnv*,intpr*);
static ErrorStatus N_defmacro(AST_cptr*,PreEnv*,intpr*);
static int isVal(const char*);
#endif
