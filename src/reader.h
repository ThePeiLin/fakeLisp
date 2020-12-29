#ifndef READER_H
#define READER_H
#include"fakedef.h"
#include<stdint.h>

StringMatchPattern* findStringPattern(const char*,StringMatchPattern*);
StringMatchPattern* newStringMatchPattern();
char** splitPattern(const char*,int32_t*);
char** splitStringInPattern(const char*,StringMatchPattern*,int32_t*);
char* readInPattern(FILE*,StringMatchPattern**);
char* readSingle(FILE*);
char* getVarName(const char*);
void printInPattern(char**,StringMatchPattern*,FILE*,int32_t);
void freeStringArry(char** ss,int32_t num);
void freeAllStringPattern();
void freeStringPattern(StringMatchPattern*);
int isInValidStringPattern(const char*);
int isVar(const char*);
int isMustList(const char*);
int32_t skipInPattern(const char*,StringMatchPattern*);
int32_t skipSpace(const char*);
int32_t countInPattern(const char* str,StringMatchPattern*);
int32_t skipUntilNext(const char* str,const char*);
int32_t skipParentheses(const char*);
int32_t skipAtom(const char*,const char*);
int32_t skipString(const char*);
static void skipComment(FILE*);
static void ungetString(const char*,FILE*);
static char* readList(FILE*);
static char* readString(FILE*);
static char* readAtom(FILE*);
static char* findKeyString(const char*,StringMatchPattern*);
static char* readSpace(FILE*);
static char* exStrCat(char*,const char*,int32_t);
static int32_t matchStringPattern(const char*,StringMatchPattern* pattern);
static int32_t countStringParts(const char*);
static int32_t* matchPartOfPattern(const char*,StringMatchPattern*,int32_t*);
#endif
