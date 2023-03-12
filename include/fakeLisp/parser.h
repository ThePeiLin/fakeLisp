#ifndef FKL_LEXER_H
#define FKL_LEXER_H
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

typedef struct FklNastHashTable
{
	uint32_t type;
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
	}u;
}FklNastNode;

FklNastNode* fklCreateNastNode(FklNastType type,uint64_t line);

struct FklStringMatchRouteNode;
struct FklCodegen;
FklNastNode* fklCreateNastNodeFromTokenStackAndMatchRoute(FklPtrStack*
		,struct FklStringMatchRouteNode*
		,size_t* errorLine
		,const FklSid_t t[4]
		,struct FklCodegen*
		,FklSymbolTable*);

struct FklStringMatchPattern;
FklNastNode* fklCreateNastNodeFromCstr(const char*
		,const FklSid_t t[4]
		,struct FklStringMatchPattern* pattern
		,FklSymbolTable* publicSymbolTable);
void fklDestroyNastNode(FklNastNode*);
void fklPrintNastNode(const FklNastNode* node
		,FILE* fp
		,FklSymbolTable*);

int fklNastNodeEqual(const FklNastNode* n0,const FklNastNode* n1);

typedef enum
{
	FKL_VM_HASH_EQ=0,
	FKL_VM_HASH_EQV,
	FKL_VM_HASH_EQUAL,
}FklVMhashTableEqType;

FklNastNode* fklNastConsWithSym(FklSid_t,FklNastNode*,uint64_t l1,uint64_t l2);
FklNastNode* fklNastCons(FklNastNode*,FklNastNode*,uint64_t l1);
FklNastPair* fklCreateNastPair(void);
FklNastHashTable* fklCreateNastHash(FklVMhashTableEqType,size_t);
FklNastVector* fklCreateNastVector(size_t);
int fklIsNastNodeList(const FklNastNode* list);
int fklIsNastNodeListAndHasSameType(const FklNastNode* list,FklNastType type);

FklNastNode* fklMakeNastNodeRef(FklNastNode* n);
#ifdef __cplusplus
}
#endif
#endif
