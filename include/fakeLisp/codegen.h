#ifndef FKL_CODEGEN_H
#define FKL_CODEGEN_H
#include"symbol.h"
#include"base.h"
#include"lexer.h"
#include"bytecode.h"
#include"builtin.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklCodegenEnv
{
	struct FklCodegenEnv* prev;
	FklHashTable* defs;
	uint64_t refcount;
}FklCodegenEnv;

typedef struct FklCodegen
{
	char* filename;
	char* realpath;
	char* curDir;
	FILE* file;
	uint64_t curline;
	FklCodegenEnv* globalEnv;
	FklSymbolTable* globalSymTable;
	FklSid_t fid;
	FklPtrQueue* expressionQueue;
	struct FklCodegen* prev;
	unsigned int freeAbleMark:1;
	unsigned long refcount:63;
}FklCodegen;

typedef FklByteCodelnt* (*FklByteCodeProcesser)(FklPtrStack* stack,FklSid_t,uint64_t);
typedef struct FklCodegenQuest
{
	FklByteCodeProcesser processer;
	FklPtrStack* stack;
	FklPtrQueue* queue;
	FklCodegenEnv* env;
    uint64_t curline;
	FklCodegen* codegen;
	struct FklCodegenQuest* prev;
}FklCodegenQuest;

void fklInitGlobalCodegener(FklCodegen* codegen
		,const char* filename
		,FILE* fp
		,FklCodegenEnv* globalEnv
		,FklSymbolTable*
		,int freeAbleMark);
void fklInitCodegener(FklCodegen* codegen
		,const char* filename
		,FILE* fp
		,FklCodegenEnv* env
		,FklCodegen* prev
		,FklSymbolTable*
		,int freeAbleMark);

void fklUninitCodegener(FklCodegen* codegen);
void fklFreeCodegener(FklCodegen* codegen);
FklCodegen* fklNewCodegener(void);
void fklInitCodegen(void);
void fklUninitCodegen(void);
FklByteCode* fklCodegenNode(const FklNastNode*,FklCodegen* codegen);
FklByteCodelnt* fklGenExpressionCode(const FklNastNode* exp
		,FklCodegenEnv* globalEnv
		,FklCodegen* codegen);
void fklAddCodegenDefBySid(FklSid_t sid,FklCodegenEnv*);
int fklIsSymbolDefined(FklSid_t sid,FklCodegenEnv*);

FklCodegenEnv* fklNewCodegenEnv(FklCodegenEnv* prev);
void fklFreeCodegenEnv(FklCodegenEnv* env);
FklCodegenEnv* fklNewGlobCodegenEnv(void);

#ifdef __cplusplus
}
#endif
#endif
