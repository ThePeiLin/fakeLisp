#ifndef FKL_NAST_H
#define FKL_NAST_H

#include"symbol.h"
#include"base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
	FKL_NAST_NIL=0,
	FKL_NAST_FIX,
	FKL_NAST_SYM,
	FKL_NAST_CHR,
	FKL_NAST_F64,
	FKL_NAST_BIG_INT,
	FKL_NAST_STR,
	FKL_NAST_VECTOR,
	FKL_NAST_PAIR,
	FKL_NAST_BOX,
	FKL_NAST_BYTEVECTOR,
	FKL_NAST_HASHTABLE,
	FKL_NAST_SLOT,
	FKL_NAST_RC_SYM,
}FklNastType;

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

typedef enum
{
	FKL_HASH_EQ=0,
	FKL_HASH_EQV,
	FKL_HASH_EQUAL,
}FklHashTableEqType;

typedef struct FklNastHashTable
{
	FklHashTableEqType type;
	uint64_t num;
	FklNastPair* items;
}FklNastHashTable;

typedef struct FklNastNode
{
	FklNastType type;
	size_t curline;
	size_t refcount;
	union
	{
		char chr;
		int64_t fix;
		double f64;
		FklSid_t sym;
		FklString* str;
		FklBytevector* bvec;
		FklBigInt* bigInt;
		FklNastVector* vec;
		FklNastPair* pair;
		FklNastHashTable* hash;
		struct FklNastNode* box;
	};
}FklNastNode;

FklNastNode* fklCreateNastNode(FklNastType type,uint64_t line);

FklNastNode* fklCopyNastNode(const FklNastNode*);

void fklDestroyNastNode(FklNastNode*);

void fklPrintNastNode(const FklNastNode* node
		,FILE* fp
		,const FklSymbolTable*);

int fklNastNodeEqual(const FklNastNode* n0,const FklNastNode* n1);

FklNastNode* fklNastConsWithSym(FklSid_t,FklNastNode*,uint64_t l1,uint64_t l2);
FklNastNode* fklNastCons(FklNastNode*,FklNastNode*,uint64_t l1);
FklNastPair* fklCreateNastPair(void);
FklNastHashTable* fklCreateNastHash(FklHashTableEqType,size_t);
FklNastVector* fklCreateNastVector(size_t);
size_t fklNastListLength(const FklNastNode* list);
int fklIsNastNodeList(const FklNastNode* list);
int fklIsNastNodeListAndHasSameType(const FklNastNode* list,FklNastType type);

FklNastNode* fklMakeNastNodeRef(FklNastNode* n);

#ifdef __cplusplus
}
#endif
#endif
