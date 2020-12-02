#ifndef READER_H
#define READER_H
#include<stdint.h>
typedef struct String_Match_Pattern
{
	int32_t size;
	char** parts;
}StringMatchPattern;

typedef struct Reader_Macro
{
	StringMatchPattern* pattern;
	AST_cptr* expression;
	struct Reader_Macro* next;
}ReaderMacro;

StringMatchPattern* newStringPattern(const char**);
void freeStringPattern(StringMatchPattern*);
char** splitPattern(const char*);
ReaderMacro* newReaderMacro(StringMatchPattern*,AST_cptr*);
void freeReaderMacro(ReaderMacro*);
char** getMacroChr(const char*);
ReaderMacro* stringMatch(const char*);
AST_cptr* ReaderMacroExpand(const char*,ReaderMacro*);
#endif
