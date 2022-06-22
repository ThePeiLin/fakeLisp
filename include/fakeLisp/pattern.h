#ifndef FKL_PATTERN_H
#define FKL_PATTERN_H
#include"bytecode.h"
#include"vm.h"
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
FklStringMatchPattern* fklNewStringMatchPattern(uint32_t,FklString**,FklByteCodelnt*);
const FklString* fklGetNthReverseCharOfStringMatchPattern(FklStringMatchPattern* pattern,uint32_t nth);
const FklString* fklGetNthPartOfStringMatchPattern(FklStringMatchPattern* pattern,uint32_t nth);
FklString** fklSplitPattern(const FklString*,uint32_t*);
FklString** fklSplitStringInPattern(const FklString*,FklStringMatchPattern*,uint32_t*);
void fklFreeAllStringPattern();
void fklFreeStringPattern(FklStringMatchPattern*);
//uint32_t fklFindKeyString(const FklString*);
int fklIsInValidStringPattern(const FklString*);
int fklIsReDefStringPattern(const FklString*);
int fklIsVar(const FklString*);
int fklIsMustList(const FklString*);
//int fklMaybePatternPrefix(const char*);

void fklFreeCstrArray(char** ss,uint32_t num);
size_t fklSkipInPattern(const FklString*,size_t i,FklStringMatchPattern*);
size_t fklSkipSpace(const FklString*,size_t i);
uint32_t fklCountInPattern(const FklString* str,FklStringMatchPattern*);
size_t fklSkipUntilNext(const FklString* str,size_t,const FklString*);
size_t fklSkipUntilNextWhenReading(const FklString*,size_t,const FklString*);
size_t fklSkipParentheses(const FklString*,size_t);
size_t fklSkipAtom(const FklString*,size_t,const FklString*);
FklString* fklGetVarName(const FklString*);
void fklPrintInPattern(FklString**,FklStringMatchPattern*,FILE*,uint32_t);

#ifdef __cplusplus
}
#endif
#endif
