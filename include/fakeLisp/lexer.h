#ifndef FKL_LEXER_H
#define FKL_LEXER_H
#include"symbol.h"
#include"base.h"
#include"ast.h"
#ifdef __cplusplus
extern "C" {
#endif

//typedef enum{
//	FKL_TYPE_NIL=0,
//	FKL_TYPE_I32,
//	FKL_TYPE_SYM,
//	FKL_TYPE_CHR,
//	FKL_TYPE_F64,
//	FKL_TYPE_I64,
//	FKL_TYPE_BIG_INT,
//	FKL_TYPE_STR,
//	FKL_TYPE_VECTOR,
//	FKL_TYPE_PAIR,
//	FKL_TYPE_BOX,
//	FKL_TYPE_BYTEVECTOR,
//	FKL_TYPE_USERDATA,
//	FKL_TYPE_PROC,
//	FKL_TYPE_CONT,
//	FKL_TYPE_CHAN,
//	FKL_TYPE_FP,
//	FKL_TYPE_DLL,
//	FKL_TYPE_DLPROC,
//	FKL_TYPE_ERR,
//	FKL_TYPE_ENV,
//	FKL_TYPE_HASHTABLE,
//}FklValueType;

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

void fklInitLexer(void);
FklNastNode* fklNewNastNodeFromTokenStack(FklPtrStack*);
FklNastNode* fklNewNastNodeFromCstr(const char*);
void fklFreeNastNode(FklNastNode*);
void fklPrintNastNode(const FklNastNode* node,FILE* fp);

int fklPatternMatch(const FklNastNode* pattern
		,FklNastNode* exp
		,FklHashTable* ht);

FklNastNode* fklPatternMatchingHashTableRef(FklSid_t sid,FklHashTable* ht);
FklHashTable* fklNewPatternMatchingHashTable(void);
void fklFreePatternMatchingHashTable(FklHashTable*);
#ifdef __cplusplus
}
#endif
#endif
