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
	struct FklCodegen* prev;
}FklCodegen;

typedef FklByteCodelnt* (*FklByteCodeProcesser)(FklPtrStack* stack,FklSid_t,uint64_t);
typedef void (*FklFormCodegenFunc)(FklHashTable* ht
		,FklPtrStack* codegenQuestStack
		,FklCodegenEnv* env
		,FklCodegen* codegen);
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

void fklInitCodegener(FklCodegen* codegen
		,const char* filename
		,FILE* fp
		,FklCodegenEnv* globalEnv
		,FklCodegen* prev
		,FklSymbolTable*);
void fklUninitCodegener(FklCodegen* codegen);
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
