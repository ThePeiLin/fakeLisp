#ifndef FKL_PATTERN_H
#define FKL_PATTERN_H
#include"bytecode.h"
#include"parser.h"
//#include"vm.h"
#ifdef __cplusplus
extern "C"{
#endif

typedef struct FklStringMatchPattern
{
	uint32_t num;
	uint32_t reserveCharNum;
	FklString** parts;
	FklByteCodelnt* proc;
	struct FklStringMatchPattern* prev;
	struct FklStringMatchPattern* next;
}FklStringMatchPattern;

FklStringMatchPattern* fklFindStringPattern(const FklString*);
FklStringMatchPattern* fklFindStringPatternBuf(const char* buf,size_t size);
FklStringMatchPattern* fklCreateStringMatchPattern(uint32_t,FklString**,FklByteCodelnt*);
const FklString* fklGetNthReverseCharOfStringMatchPattern(FklStringMatchPattern* pattern,uint32_t nth);
const FklString* fklGetNthPartOfStringMatchPattern(FklStringMatchPattern* pattern,uint32_t nth);
FklString** fklSplitPattern(const FklString*,uint32_t*);
void fklDestroyAllStringPattern();
void fklDestroyStringPattern(FklStringMatchPattern*);
int fklIsInValidStringPattern(FklString* const*,uint32_t);
int fklIsReDefStringPattern(FklString* const*,uint32_t);
int fklIsVar(const FklString*);
int fklIsMustList(const FklString*);
//int fklMaybePatternPrefix(const char*);
//uint32_t fklFindKeyString(const FklString*);
//FklString** fklSplitStringInPattern(const FklString*,FklStringMatchPattern*,uint32_t*);

void fklDestroyCstrArray(char** ss,uint32_t num);
FklString* fklGetVarName(const FklString*);
//size_t fklSkipInPattern(const FklString*,size_t i,FklStringMatchPattern*);
//size_t fklSkipSpace(const FklString*,size_t i);
//uint32_t fklCountInPattern(const FklString* str,FklStringMatchPattern*);
//size_t fklSkipUntilNext(const FklString* str,size_t,const FklString*);
//size_t fklSkipUntilNextWhenReading(const FklString*,size_t,const FklString*);
//size_t fklSkipParentheses(const FklString*,size_t);
//size_t fklSkipAtom(const FklString*,size_t,const FklString*);
//void fklPrintInPattern(FklString**,FklStringMatchPattern*,FILE*,uint32_t);

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
