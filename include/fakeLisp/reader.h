#ifndef READER_H
#define READER_H
#include"fakedef.h"
#include<stdint.h>

StringMatchPattern* findStringPattern(const char*);
StringMatchPattern* newStringMatchPattern(int32_t,char**,ByteCodelnt*);
StringMatchPattern* newFStringMatchPattern(int32_t num,char** parts,void(*fproc)(FakeVM* exe));
char** splitPattern(const char*,int32_t*);
char** splitStringInPattern(const char*,StringMatchPattern*,int32_t*);
char* readInPattern(FILE*,char**,int*);
char* getVarName(const char*);
void printInPattern(char**,StringMatchPattern*,FILE*,int32_t);
void freeStringArry(char** ss,int32_t num);
void freeAllStringPattern();
void freeStringPattern(StringMatchPattern*);
int isInValidStringPattern(const char*);
int isReDefStringPattern(const char*);
int isVar(const char*);
int isMustList(const char*);
int32_t skipInPattern(const char*,StringMatchPattern*);
int32_t skipSpace(const char*);
int32_t countInPattern(const char* str,StringMatchPattern*);
int32_t skipUntilNext(const char* str,const char*);
int32_t skipParentheses(const char*);
int32_t skipAtom(const char*,const char*);
int32_t findKeyString(const char*);
#endif
