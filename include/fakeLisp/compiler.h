#ifndef FKL_COMPILER_H
#define FKL_COMPILER_H
#include"symbol.h"
#include"base.h"
#include"ast.h"
#include"bytecode.h"
#include"builtin.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	FklBuiltInErrorType state;
	FklAstCptr* place;
}FklErrorState;

typedef struct FklPreDef
{
	FklString* symbol;
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

typedef struct FklCompEnv
{
	struct FklCompEnv* prev;
	FklString* prefix;
	const FklString** exp;
	uint32_t n;
	FklHashTable* defs;
	FklPreMacro* macro;
	FklByteCodelnt* proc;
	uint32_t refcount;
	struct FklKeyWord* keyWords;
}FklCompEnv;

typedef struct
{
	FklSid_t id;
}FklCompEnvHashItem;

typedef struct FklInterpreter
{
	char* filename;
	char* realpath;
	char* curDir;
	FILE* file;
	int curline;
	FklCompEnv* glob;
	struct FklLineNumberTable* lnt;
	struct FklInterpreter* prev;
}FklInterpreter;

typedef struct FklKeyWord
{
	FklString* word;
	struct FklKeyWord* next;
}FklKeyWord;

void fklSetCwd(const char*);
void fklFreeCwd(void);
const char* fklGetCwd(void);
void fklSetMainFileRealPath(const char* path);
void fklFreeMainFileRealPath(void);
void fklSetMainFileRealPathWithCwd(void);
const char* fklGetMainFileRealPath(void);
void fklPrintCompileError(const FklAstCptr*,FklBuiltInErrorType,FklInterpreter*);
FklPreDef* fklAddDefine(const FklString*,const FklAstCptr*,FklPreEnv*);
FklPreDef* fklFindDefine(const FklString*,const FklPreEnv*);
FklPreDef* fklNewDefines(const FklString*);

FklCompEnvHashItem* fklAddCompDef(const FklString*,FklCompEnv*);
FklCompEnvHashItem* fklAddCompDefBySid(FklSid_t,FklCompEnv*);
FklCompEnvHashItem* fklAddCompDefCstr(const char*,FklCompEnv*);
FklCompEnv* fklNewCompEnv(FklCompEnv*);
FklCompEnv* fklNewGlobCompEnv(FklCompEnv*);
void fklDestroyCompEnv(FklCompEnv* objEnv);
void fklFreeAllMacroThenDestroyCompEnv(FklCompEnv* env);
FklCompEnvHashItem* fklFindCompDef(const FklString*,FklCompEnv*);
FklCompEnvHashItem* fklFindCompDefBySid(FklSid_t,FklCompEnv*);
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
FklStringMatchPattern* fklAddStringPattern(FklString**,uint32_t,FklAstCptr*,FklInterpreter*);
FklStringMatchPattern* fklAddReDefStringPattern(FklString**,uint32_t,FklAstCptr*,FklInterpreter*);
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
FklByteCodelnt* fklCompileLibrary(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileTry(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);
FklByteCodelnt* fklCompileFlsym(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*);

FklByteCode* fklCompileHashtable(FklAstCptr*);
FklByteCode* fklCompileVector(FklAstCptr*);
FklByteCode* fklCompileBox(FklAstCptr*);
void fklPrintUndefinedSymbol(FklByteCodelnt* code);


#ifdef __cplusplus
}
#endif

#endif
