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

typedef struct FklNastPair
{
	struct FklNastNode* car;
	struct FklNastNode* cdr;
}FklNastPair;

typedef struct FklNastVector
{
	size_t size;
	struct FklNastNode** base;
}FklNastVector;

typedef struct FklNastHashTable
{
	uint32_t type;
	uint64_t num;
	FklNastPair* items;
}FklNastHashTable;

typedef struct FklNastNode
{
	FklValueType type;
	uint64_t curline;
	union
	{
		char chr;
		int32_t i32;
		int64_t i64;
		double f64;
		FklSid_t sym;
		FklString* str;
		FklBytevector* bvec;
		FklBigInt* bigInt;
		FklNastVector* vec;
		FklNastPair* pair;
		FklNastHashTable* hash;
		struct FklNastNode* box;
	}u;
}FklNastNode;

typedef struct FklCompEnvN
{
	struct FklCompEnvN* prev;
	FklHashTable* defs;
	uint64_t refcount;
}FklCompEnvN;

typedef struct FklCompiler
{
	char* filename;
	char* realpath;
	char* curDir;
	FILE* file;
	uint64_t curline;
	FklCompEnvN* glob;
	FklSymbolTable* globTable;
	FklSid_t fid;
	struct FklLineNumberTable* lnt;
	struct FklCompiler* prev;
}FklCompiler;

typedef FklByteCodelnt* (*FklByteCodeProcesser)(FklPtrStack* stack,FklSid_t,uint64_t);
typedef void (*FklFormCompilerFunc)(FklHashTable* ht
		,FklPtrStack* compileQuestStack
		,FklCompEnvN* env
		,FklCompiler* compiler);
typedef struct FklCompileQuest
{
	FklByteCodeProcesser processer;
	FklPtrStack* stack;
	FklPtrQueue* queue;
	FklCompEnvN* env;
    uint64_t curline;
	FklCompiler* comp;
}FklCompileQuest;

FklNastNode* fklNewNastNodeFromTokenStack(FklPtrStack*);
FklNastNode* fklNewNastNodeFromCstr(const char*);
void fklFreeNastNode(FklNastNode*);
void fklInitBuiltInPattern(void);
int fklPatternMatch(const FklNastNode* pattern
		,FklNastNode* exp
		,FklHashTable* ht);
FklByteCode* fklCompileNode(const FklNastNode*,FklCompiler* compiler);
FklByteCodelnt* fklCompileExpression(const FklNastNode* exp
		,FklCompEnvN* globalEnv
		,FklCompiler* compiler);
void fklAddCompDefBySid(FklSid_t sid,FklCompEnvN*);
int fklIsSymbolDefined(FklSid_t sid,FklCompEnvN*);

FklCompEnvN* fklNewCompEnvN(FklCompEnvN* prev);
#ifdef __cplusplus
}
#endif
#endif
