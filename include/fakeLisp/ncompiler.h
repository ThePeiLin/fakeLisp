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
	struct FklNastNode** items;
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
		FklBytevector bvec;
		FklNastVector vec;
		FklNastPair pair;
		FklBigInt bigInt;
		FklNastHashTable hash;
		struct FklNastNode* box;
	}value;
}FklNastNode;

typedef struct FklCompEnvN
{
	struct FklCompEnvN* prev;
	FklHashTable* defs;
}FklCompEnvN;

typedef struct FklCompiler
{
	char* filename;
	char* realpath;
	char* curDir;
	FILE* file;
	uint64_t curline;
	FklCompEnvN* glob;
	struct FklLineNumberTable* lnt;
	struct FklInterpreter* prev;
}FklCompiler;

int fklPatternMatch(const FklNastNode* pattern,FklNastNode* exp,FklHashTable* ht);
FklByteCode* fklCompileNode(const FklNastNode*);
FklByteCodelnt* fklCompileExpression(const FklNastNode* exp
		,FklCompEnvN* globalEnv
		,FklCompiler* compiler);
#ifdef __cplusplus
}
#endif
#endif
