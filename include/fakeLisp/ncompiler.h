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
	struct
	{
		struct FklNastNode* key;
		struct FklNastNode* value;
	}* items;
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
		FklString str;
		FklSid_t sym;
		FklBytevector bvec;
		FklNastVector vec;
		FklNastPair pair;
		FklBigInt bigInt;
		FklNastHashTable hash;
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
		,FklCompEnvN* env
		,FklPtrStack* nextCompileStack
		,FklCompiler* compiler);
typedef struct FklNextCompile
{
	FklByteCodeProcesser processer;
	FklPtrStack* stack;
	FklPtrQueue* queue;
	FklCompEnvN* env;
    uint64_t curline;
	FklCompiler* comp;
}FklNextCompile;

FklNastNode* fklNewNastNodeFromTokenStack(FklPtrStack*);
void fklInitBuiltInPattern(void);
int fklPatternMatch(const FklNastNode* pattern,FklNastNode* exp,FklHashTable* ht);
FklByteCode* fklCompileNode(const FklNastNode*);
FklByteCodelnt* fklCompileExpression(const FklNastNode* exp
		,FklCompEnvN* globalEnv
		,FklCompiler* compiler);
#ifdef __cplusplus
}
#endif
#endif
