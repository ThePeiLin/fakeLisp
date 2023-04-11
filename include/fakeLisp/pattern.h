#ifndef FKL_PATTERN_H
#define FKL_PATTERN_H
#include"bytecode.h"
#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	FKL_STRING_PATTERN_BUILTIN=0,
	FKL_STRING_PATTERN_DEFINED,
}FklStringPatternType;

typedef struct FklNastNode FklNastNode;
typedef struct FklNastVector FklNastVector;
typedef struct FklStringMatchPattern
{
	FklNastNode* parts;
	FklStringPatternType type;
	union
	{
		FklByteCodelnt* proc;
		FklNastNode* (*func)(FklPtrStack*,uint64_t,size_t*,const FklSid_t[4]);
	}u;
	FklPrototypes* ptpool;
	struct FklStringMatchPattern* next;
	uint8_t own;
}FklStringMatchPattern;

typedef struct FklStringMatchRouteNode
{
	FklStringMatchPattern* pattern;
	size_t start;
	size_t end;
	struct FklStringMatchRouteNode* parent;
	struct FklStringMatchRouteNode* siblings;
	struct FklStringMatchRouteNode* children;
}FklStringMatchRouteNode;

typedef struct FklStringMatchState
{
	FklStringMatchPattern* pattern;
	size_t index;
	struct FklStringMatchState* next;
}FklStringMatchState;

#define FKL_STRING_PATTERN_UNIVERSAL_SET ((FklStringMatchSet*)0x1)

typedef struct FklStringMatchSet
{
	FklStringMatchState* str;
	FklStringMatchState* box;
	FklStringMatchState* sym;
	struct FklStringMatchSet* prev;
}FklStringMatchSet;

void fklDestroyStringMatchState(FklStringMatchState* state);
void fklDestroyStringMatchSet(FklStringMatchSet* set);

FklStringMatchPattern* fklInitBuiltInStringPattern(FklSymbolTable* publicSymbolTable);

FklStringMatchPattern* fklFindStringPatternBuf(const char* buf,size_t size);
FklStringMatchPattern* fklCreateStringMatchPattern(FklNastNode*
		,FklByteCodelnt*
		,FklPrototypes* ptpool
		,FklStringMatchPattern* next
		,int own);

void fklAddStringMatchPattern(FklNastNode*
		,FklByteCodelnt*
		,FklStringMatchPattern** head
		,FklPrototypes* ptpool
		,int own);
void fklDestroyStringPattern(FklStringMatchPattern*);

int fklStringPatternCoverState(const FklNastNode* p0,const FklNastNode* p1);
void fklDestroyAllStringPattern(FklStringMatchPattern*);
int fklIsValidStringPattern(const FklNastNode*,FklHashTable** psymbolTable);

FklStringMatchRouteNode* fklCreateStringMatchRouteNode(FklStringMatchPattern* p
		,size_t s
		,size_t e
		,FklStringMatchRouteNode* parent
		,FklStringMatchRouteNode* siblings
		,FklStringMatchRouteNode* children);

void fklInsertMatchRouteNodeAsLastChild(FklStringMatchRouteNode* parent,FklStringMatchRouteNode* c);
void fklInsertMatchRouteNodeAsFirstChild(FklStringMatchRouteNode* parent,FklStringMatchRouteNode* c);
void fklDestroyStringMatchRoute(FklStringMatchRouteNode* start);
void fklPrintStringMatchRoute(FklStringMatchRouteNode* root,FILE*);

FklSid_t* fklGetVarId(const FklNastNode*);

typedef struct
{
	FklSid_t id;
	FklNastNode* node;
}FklPatternMatchingHashTableItem;

#define FKL_PATTERN_COVER    (1)
#define FKL_PATTERN_BE_COVER (2)
#define FKL_PATTERN_EQUAL    (3)

FklNastNode* fklCreatePatternFromNast(FklNastNode*,FklHashTable**);
int fklPatternMatch(const FklNastNode* pattern
		,const FklNastNode* exp
		,FklHashTable* ht);
void fklPatternMatchingHashTableSet(FklSid_t sid,FklNastNode* node,FklHashTable* ht);

int fklPatternCoverState(const FklNastNode* p0,const FklNastNode* p1);
FklNastNode* fklPatternMatchingHashTableRef(FklSid_t sid,FklHashTable* ht);
FklHashTable* fklCreatePatternMatchingHashTable(void);

#ifdef __cplusplus
}
#endif
#endif
