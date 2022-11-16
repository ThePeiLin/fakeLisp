#ifndef FKL_LEXER_H
#define FKL_LEXER_H
#include"symbol.h"
#include"base.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
	FKL_NAST_NIL=0,
	FKL_NAST_I32,
	FKL_NAST_SYM,
	FKL_NAST_CHR,
	FKL_NAST_F64,
	FKL_NAST_I64,
	FKL_NAST_BIG_INT,
	FKL_NAST_STR,
	FKL_NAST_VECTOR,
	FKL_NAST_PAIR,
	FKL_NAST_BOX,
	FKL_NAST_BYTEVECTOR,
	FKL_NAST_HASHTABLE,
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

//void fklInitLexer(void);
FklNastNode* fklCreateNastNode(FklNastType type,uint64_t line);
FklNastNode* fklCreateNastNodeFromTokenStack(FklPtrStack*,size_t* errorLine,const FklSid_t t[4]);
struct FklStringMatchPattern;
FklNastNode* fklCreateNastNodeFromCstr(const char*,const FklSid_t t[4],struct FklStringMatchPattern* pattern);
FklNastNode* fklCopyNastNode(const FklNastNode* n);
void fklDestroyNastNode(FklNastNode*);
void fklPrintNastNode(const FklNastNode* node,FILE* fp);

int fklNastNodeEqual(const FklNastNode* n0,const FklNastNode* n1);

enum FklVMhashTableEqType;
FklNastPair* fklCreateNastPair(void);
FklNastHashTable* fklCreateNastHash(enum FklVMhashTableEqType,size_t);
FklNastVector* fklCreateNastVector(size_t);
int fklIsNastNodeList(const FklNastNode* list);
int fklIsNastNodeListAndHasSameType(const FklNastNode* list,FklNastType type);

FklNastNode* fklMakeNastNodeRef(FklNastNode* n);
#ifdef __cplusplus
}
#endif
#endif
