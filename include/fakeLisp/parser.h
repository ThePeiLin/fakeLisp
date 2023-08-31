#ifndef FKL_PARSER_H
#define FKL_PARSER_H

#include"symbol.h"
#include"nast.h"

#ifdef __cplusplus
extern "C" {
#endif

FklNastNode* fklCreateNastNodeFromCstr(const char*,FklSymbolTable* publicSymbolTable);

char* fklReadWithBuiltinParser(FILE* fp
		,size_t* psize
		,size_t line
		,size_t* pline
		,FklSymbolTable* st
		,int* unexpectEOF
		,size_t* errLine
		,FklNastNode** output
		,FklPtrStack* symbolStack
		,FklPtrStack* stateStack);

#ifdef __cplusplus
}
#endif
#endif
