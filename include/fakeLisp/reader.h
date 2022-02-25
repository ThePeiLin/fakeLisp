#ifndef READER_H
#define READER_H
#include"fakedef.h"
#include<stdint.h>

FklStringMatchPattern* fklFindStringPattern(const char*);
FklStringMatchPattern* fklNewStringMatchPattern(int32_t,char**,FklByteCodelnt*);
FklStringMatchPattern* fklNewFStringMatchPattern(int32_t num,char** parts,void(*fproc)(FklVM* exe));
char** fklSplitPattern(const char*,int32_t*);
char** fklSplitStringInPattern(const char*,FklStringMatchPattern*,int32_t*);
char* fklReadInPattern(FILE*,char**,int*);
char* fklGetVarName(const char*);
void fklPrintInPattern(char**,FklStringMatchPattern*,FILE*,int32_t);
void fklFreeStringArry(char** ss,int32_t num);
void fklFreeAllStringPattern();
void fklFreeStringPattern(FklStringMatchPattern*);
int fklIsInValidStringPattern(const char*);
int fklIsReDefStringPattern(const char*);
int fklIsVar(const char*);
int fklIsMustList(const char*);
int32_t fklSkipInPattern(const char*,FklStringMatchPattern*);
int32_t fklSkipSpace(const char*);
int32_t fklCountInPattern(const char* str,FklStringMatchPattern*);
int32_t fklSkipUntilNext(const char* str,const char*);
int32_t fklSkipParentheses(const char*);
int32_t fklSkipAtom(const char*,const char*);
int32_t fklFindKeyString(const char*);
#endif
