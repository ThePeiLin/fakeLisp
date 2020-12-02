#ifndef READER_H
#define READER_H
typedef struct Reader_Macro
{
	char* macroChr;
	AST_cptr* expression;
	struct Reader_Macro* next;
}ReaderMacro;

ReaderMacro* newReaderMacro(const char*,AST_cptr*);
void freeReaderMacro(ReaderMacro*);
char* getMacroChr(const char*);
ReaderMacro* findReaderMacro(const char*);
AST_cptr* matchReaderMacro(const char*,ReaderMacro*);
#endif
