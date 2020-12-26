#ifndef READER_H
#define READER_H
#include"fakedef.h"
#include<stdint.h>

StringMatchPattern* addStringPattern(char**,int32_t,AST_cptr*,intpr*);
void freeStringPattern(StringMatchPattern*);
StringMatchPattern* findStringPattern(const char*,StringMatchPattern*);
char** splitPattern(const char*,int32_t*);
char** splitStringInPattern(const char*,StringMatchPattern*,int32_t*);
char* readInPattern(FILE*,StringMatchPattern**);
char* readSingle(FILE*);
void printInPattern(char**,StringMatchPattern*,FILE*,int32_t);
static int32_t matchStringPattern(const char*,StringMatchPattern* pattern);
int32_t skipInPattern(const char*,StringMatchPattern*);
static void skipComment(FILE*);
static char* readList(FILE*);
static char* readString(FILE*);
static char* readAtom(FILE*);
static char* findKeyString(const char*,StringMatchPattern*);
static char* readSpace(FILE*);
static int32_t countStringParts(const char*);
static void ungetString(const char*,FILE*);
static int32_t skipSpace(const char*);
static int32_t* matchPartOfPattern(const char*,StringMatchPattern*,int32_t*);
static int32_t countInPattern(const char* str,StringMatchPattern*);
static int32_t skipUntilNext(const char* str,const char*);
static int32_t skipParentheses(const char*);
static int32_t skipAtom(const char*,const char*);
static int32_t skipString(const char*);
static void freeStringArry(char** ss,int32_t num);
static char* exStrCat(char*,const char*,int32_t);
static CompEnv* createPatternCompEnv(char**,int32_t,CompEnv*);
int isInValidStringPattern(const char*);
int isKeyString(const char*);
char* getKeyString(const char*);
#endif
