#ifndef FKL_PATTERN_H
#define FKL_PATTERN_H
#include"bytecode.h"
#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	FKL_STRING_PATTERN_ATOM,
	FKL_STRING_PATTERN_BUILTIN,
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
		FklNastNode* (*func)(FklPtrStack*,uint64_t,size_t*);
	}u;
	struct FklStringMatchPattern* next;
}FklStringMatchPattern;

typedef struct FklStringMatchRouteNode
{
	FklStringMatchPattern* pattern;
	size_t start;
	size_t end;
	struct FklStringMatchRouteNode* siblings;
	struct FklStringMatchRouteNode* children;
}FklStringMatchRouteNode;

typedef struct FklStringMatchState
{
	FklStringMatchPattern* pattern;
	size_t index;
	struct FklStringMatchState* next;
}FklStringMatchState;

#define FKL_STRING_PATTERN_UNIVERSAL_SET ((void*)0x1)

typedef struct FklStringMatchSet
{
	FklStringMatchState* str;
	FklStringMatchState* box;
	FklStringMatchState* sym;
	struct FklStringMatchSet* prev;
}FklStringMatchSet;

void fklDestroyStringMatchState(FklStringMatchState* state);
void fklDestroyStringMatchSet(FklStringMatchSet* set);

FklStringMatchPattern* fklInitBuiltInStringPattern(void);

FklStringMatchPattern* fklFindStringPatternBuf(const char* buf,size_t size);
FklStringMatchPattern* fklCreateStringMatchPattern(FklNastNode*
		,FklByteCodelnt*
		,FklStringMatchPattern* next);

void fklAddStringMatchPattern(FklNastNode*
		,FklByteCodelnt*
		,FklStringMatchPattern** head);
void fklDestroyStringPattern(FklStringMatchPattern*);

int fklStringPatternCoverState(const FklNastVector* p0,const FklNastVector* p1);
void fklDestroyAllStringPattern(FklStringMatchPattern*);
int fklIsInValidStringPattern(FklNastNode*);

FklSid_t* fklGetVarId(const FklNastNode*);

typedef struct
{
	FklSid_t id;
	FklNastNode* node;
}FklPatternMatchingHashTableItem;

#define FKL_PATTERN_COVER    (1)
#define FKL_PATTERN_BE_COVER (2)
#define FKL_PATTERN_EQUAL    (3)

int fklPatternMatch(const FklNastNode* pattern
		,const FklNastNode* exp
		,FklHashTable* ht);

int fklIsValidSyntaxPattern(const FklNastNode* p);
int fklPatternCoverState(const FklNastNode* p0,const FklNastNode* p1);
FklNastNode* fklPatternMatchingHashTableRef(FklSid_t sid,FklHashTable* ht);
FklHashTable* fklCreatePatternMatchingHashTable(void);

#ifdef __cplusplus
}
#endif
#endif
