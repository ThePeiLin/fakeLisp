#ifndef FKL_READER_H
#define FKL_READER_H
#include"pattern.h"
#include"base.h"
#include<stdio.h>
#include<stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

char* fklReadWithBuiltinParser(FILE* fp
		,size_t* psize
		,size_t line
		,size_t* pline
		,FklSymbolTable* st
		,int* unexpectEOF
		,FklNastNode** output);

char* fklReadInStringPattern(FILE*
		,size_t* size
		,size_t line
		,size_t* pline
		,int*
		,FklPtrStack* retval
		,FklStringMatchPattern* patterns
		,FklStringMatchRouteNode** proute);

int fklIsAllSpaceBufSize(const char* buf,size_t size);
#ifdef __cplusplus
}
#endif

#endif
