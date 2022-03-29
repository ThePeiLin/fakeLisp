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
	char** parts;
	FklValueType type;
	union{
		void (*fProc)(FklVM*);
		FklByteCodelnt* bProc;
	}u;
	struct FklStringMatchPattern* prev;
	struct FklStringMatchPattern* next;
}FklStringMatchPattern;

FklStringMatchPattern* fklFindStringPattern(const char*);
FklStringMatchPattern* fklNewStringMatchPattern(int32_t,char**,FklByteCodelnt*);
FklStringMatchPattern* fklNewFStringMatchPattern(int32_t num,char** parts,void(*fproc)(FklVM* exe));
char* fklGetNthReverseCharOfStringMatchPattern(FklStringMatchPattern* pattern,uint32_t nth);
char* fklGetNthPartOfStringMatchPattern(FklStringMatchPattern* pattern,uint32_t nth);
char** fklSplitPattern(const char*,int32_t*);
char** fklSplitStringInPattern(const char*,FklStringMatchPattern*,int32_t*);
void fklFreeAllStringPattern();
void fklFreeStringPattern(FklStringMatchPattern*);
void fklInitBuiltInStringPattern(void);
int32_t fklFindKeyString(const char*);
int fklIsInValidStringPattern(const char*);
int fklIsReDefStringPattern(const char*);
int fklIsVar(const char*);
int fklIsMustList(const char*);
int fklMaybePatternPrefix(const char*);

void fklFreeStringArry(char** ss,int32_t num);
size_t fklSkipInPattern(const char*,FklStringMatchPattern*);
size_t fklSkipSpace(const char*);
int32_t fklCountInPattern(const char* str,FklStringMatchPattern*);
size_t fklSkipUntilNext(const char* str,const char*);
size_t fklSkipUntilNextWhenReading(const char*,const char*);
size_t fklSkipParentheses(const char*);
size_t fklSkipAtom(const char*,const char*);
char* fklGetVarName(const char*);
void fklPrintInPattern(char**,FklStringMatchPattern*,FILE*,int32_t);

#ifdef __cplusplus
}
#endif
#endif
