#ifndef FKL_READER_H
#define FKL_READER_H
//#include"ast.h"
#include"bytecode.h"
#include"vm.h"
#include"pattern.h"
#include<stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
char* fklReadInStringPattern(FILE*
		,char**
		,size_t* size
		,size_t* prevSize
		,uint32_t
		,int*
		,FklPtrStack* retval
		,char* (*)(FILE*,size_t* size));

int fklIsAllSpaceBufSize(const char* buf,size_t size);
#ifdef __cplusplus
}
#endif

#endif
