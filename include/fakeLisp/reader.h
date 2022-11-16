#ifndef FKL_READER_H
#define FKL_READER_H
#include"pattern.h"
#include"base.h"
#include<stdio.h>
#include<stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
char* fklReadInStringPattern(FILE*
		,char**
		,size_t* size
		,size_t* prevSize
		,size_t
		,int*
		,FklPtrStack* retval
		,char* (*)(FILE*,size_t* size)
		,FklStringMatchPattern* patterns);

int fklIsAllSpaceBufSize(const char* buf,size_t size);
#ifdef __cplusplus
}
#endif

#endif
