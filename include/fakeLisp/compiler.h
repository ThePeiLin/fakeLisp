#ifndef FKL_COMPILER_H
#define FKL_COMPILER_H
#include"symbol.h"
#include"basicADT.h"
#include"ast.h"
#include"bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	FklErrorType state;
	FklAstCptr* place;
}FklErrorState;

typedef struct FklPreDef
{
	char* symbol;
	FklAstCptr obj;//node or symbol or val
	struct FklPreDef* next;
}FklPreDef;

typedef struct FklPreEnv
{
	struct FklPreEnv* prev;
	FklPreDef* symbols;
	struct FklPreEnv* next;
}FklPreEnv;

typedef struct FklPreMacro
{
	FklAstCptr* pattern;
	FklByteCodelnt* proc;
	struct FklPreMacro* next;
	struct FklCompEnv* macroEnv;
}FklPreMacro;

typedef struct FklCompDef
{
	FklSid_t id;
	struct FklCompDef* next;
}FklCompDef;

typedef struct FklCompEnv
{
	struct FklCompEnv* prev;
	char* prefix;
	const char** exp;
	uint32_t n;
	FklCompDef* head;
	FklPreMacro* macro;
	FklByteCodelnt* proc;
	uint32_t refcount;
	struct FklKeyWord* keyWords;
}FklCompEnv;

typedef struct FklInterpreter
{
	char* filename;
	char* curDir;
	FILE* file;
	int curline;
	FklCompEnv* glob;
	struct FklLineNumberTable* lnt;
	struct FklInterpreter* prev;
}FklInterpreter;

typedef struct FklKeyWord
{
	char* word;
	struct FklKeyWord* next;
}FklKeyWord;

void fklSetCwd(const char*);
void fklFreeCwd(void);
const char* fklGetCwd(void);
void fklPrintCompileError(const FklAstCptr*,FklErrorType,FklInterpreter*);
FklPreDef* fklAddDefine(const char*,const FklAstCptr*,FklPreEnv*);
FklPreDef* fklFindDefine(const char*,const FklPreEnv*);
FklPreDef* fklNewDefines(const char*);

FklCompDef* fklAddCompDef(const char*,FklCompEnv*);
FklCompEnv* fklNewCompEnv(FklCompEnv*);
void fklDestroyCompEnv(FklCompEnv* objEnv);
void fklFreeAllMacroThenDestroyCompEnv(FklCompEnv* env);
FklCompDef* fklFindCompDef(const char*,FklCompEnv*);
FklInterpreter* fklNewIntpr(const char*,FILE*,FklCompEnv*,FklLineNumberTable*);
FklInterpreter* fklNewTmpIntpr(const char*,FILE*);

void fklFreeIntpr(FklInterpreter*);
FklPreEnv* fklNewEnv(FklPreEnv*);
void fklDestroyEnv(FklPreEnv*);

void fklInitCompEnv(FklCompEnv*);

int fklHasLoadSameFile(const char*,const FklInterpreter*);
FklInterpreter* fklGetFirstIntpr(FklInterpreter*);

char* fklGetLastWorkDir(FklInterpreter*);

void fklFreeAllMacro(FklPreMacro* head);
void fklFreeAllKeyWord(FklKeyWord*);

FklPreMacro* fklPreMacroMatch(const FklAstCptr*,FklPreEnv**,FklCompEnv*,FklCompEnv**);
int fklPreMacroExpand(FklAstCptr*,FklCompEnv*,FklInterpreter* inter);
int fklAddMacro(FklAstCptr*,FklByteCodelnt*,FklCompEnv* curEnv);
void fklFreeMacroEnv();
FklCompEnv* fklCreateMacroCompEnv(const FklAstCptr*,FklCompEnv*);
int fklRetree(FklAstCptr**,FklAstCptr*);
void fklUninitPreprocess();
void fklInitGlobKeyWord(FklCompEnv*);
FklStringMatchPattern* fklAddStringPattern(char**,int32_t,FklAstCptr*,FklInterpreter*);
FklStringMatchPattern* fklAddReDefStringPattern(char**,int32_t,FklAstCptr*,FklInterpreter*);
void fklInitBuiltInStringPattern(void);

FklByteCodelnt* fklCompileFile(FklInterpreter*,int*);
FklByteCodelnt* fklCompile(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileQsquote(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileUnquote(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileUnqtesp(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileConst(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCode* fklCompileQuote(FklAstCptr*);
FklByteCode* fklCompileAtom(FklAstCptr*);
FklByteCode* fklCompilePair(FklAstCptr*);
FklByteCode* fklCompileNil();
FklByteCodelnt* fklCompileFuncCall(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileDef(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileSetq(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
//FklByteCodelnt* fklCompileSetf(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileGetf(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileSzof(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileSym(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileCond(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileLambda(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileBegin(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileAnd(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileOr(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileLoad(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileProgn(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileImport(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileTry(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileFlsym(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);

FklByteCode* fklCompileVector(FklAstCptr*);
FklByteCode* fklCompileBox(FklAstCptr*);
void fklPrintUndefinedSymbol(FklByteCodelnt* code);


#ifdef __cplusplus
}
#endif

#endif
