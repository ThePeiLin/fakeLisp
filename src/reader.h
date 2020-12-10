#ifndef READER_H
#define READER_H
#include"fakedef.h"
#include<stdint.h>
typedef struct String_Match_Pattern
{
	int32_t num;
	char** parts;
	struct String_Match_Pattern* next;
}StringMatchPattern;

typedef struct Reader_Macro
{
	StringMatchPattern* pattern;
	AST_cptr* expression;
	struct Reader_Macro* next;
}ReaderMacro;

StringMatchPattern* newStringPattern(const char**,int32_t);
void freeStringPattern(StringMatchPattern*);
StringMatchPattern* findStringPattern(const char*,StringMatchPattern*);
char** splitPattern(const char*,int32_t*);
ReaderMacro* newReaderMacro(StringMatchPattern*,AST_cptr*);
void freeReaderMacro(ReaderMacro*);
ReaderMacro* stringMatch(const char*);
AST_cptr* ReaderMacroExpand(const char*,ReaderMacro*);
char* readInPattern(intpr*,StringMatchPattern*);
char* readSingle(FILE*);
static void skipComment(FILE*);
static char* readList(FILE*);
static char* readString(FILE*);
static char* readAtom(FILE*);
static char* readSpace(FILE*);
static int32_t countStringParts(const char*);
static int32_t skipSpace(const char*);
/*static*/ int32_t* matchPartOfPattern(const char*,StringMatchPattern*,int32_t*);
static int32_t countInPattern(const char* str,StringMatchPattern*);
static int isKeyString(const char*);
static int32_t skipSingleUntilNext(const char* str,const char*);
static int32_t skipParentheses(const char*);
static int32_t skipAtom(const char*,char);
static int32_t skipString(const char*);
#endif
