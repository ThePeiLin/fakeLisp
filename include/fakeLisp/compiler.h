#ifndef COMPILER_H
#define COMPILER_H
#include"symbol.h"
#include"basicADT.h"
#include"ast.h"
#include"deftype.h"
#include"bytecode.h"
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
	FklDefTypes* deftypes;
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
	struct LineNumberTable* lnt;
	struct FklInterpreter* prev;
	FklDefTypes* deftypes;
}FklInterpreter;

typedef struct FklKeyWord
{
	char* word;
	struct FklKeyWord* next;
}FklKeyWord;

void fklSetInterpreterPath(const char*);
void fklFreeInterpeterPath(void);
void fklExError(const FklAstCptr*,int,FklInterpreter*);
FklPreDef* fklAddDefine(const char*,const FklAstCptr*,FklPreEnv*);
FklPreDef* fklFindDefine(const char*,const FklPreEnv*);
FklPreDef* fklNewDefines(const char*);

FklCompDef* fklAddCompDef(const char*,FklCompEnv*);
FklCompEnv* fklNewCompEnv(FklCompEnv*);
void fklDestroyCompEnv(FklCompEnv* objEnv);
void fklFreeAllMacroThenDestroyCompEnv(FklCompEnv* env);
FklCompDef* fklFindCompDef(const char*,FklCompEnv*);
FklInterpreter* fklNewIntpr(const char*,FILE*,FklCompEnv*,LineNumberTable*,FklDefTypes* deftypes);
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
void fklUnInitPreprocess();
void fklInitGlobKeyWord(FklCompEnv*);
void fklInitNativeDefTypes(FklDefTypes*);
FklStringMatchPattern* fklAddStringPattern(char**,int32_t,FklAstCptr*,FklInterpreter*);
FklStringMatchPattern* fklAddReDefStringPattern(char**,int32_t,FklAstCptr*,FklInterpreter*);
void fklInitBuiltInStringPattern(void);

FklByteCodelnt* fklCompileFile(FklInterpreter*,int evalIm,int*);
FklByteCodelnt* fklCompile(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileQsquote(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileUnquote(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileUnqtesp(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileConst(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCode* fklCompileQuote(FklAstCptr*);
FklByteCode* fklCompileAtom(FklAstCptr*);
FklByteCode* fklCompilePair(FklAstCptr*);
FklByteCode* fklCompileNil();
FklByteCodelnt* fklCompileFuncCall(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileDef(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileSetq(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileSetf(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileGetf(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileSzof(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileSym(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileCond(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileLambda(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileBegin(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileAnd(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileOr(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileLoad(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileProgn(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileImport(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileTry(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
FklByteCodelnt* fklCompileFlsym(FklAstCptr*,FklCompEnv*,FklInterpreter*,FklErrorState*,int evalIm);
#endif
