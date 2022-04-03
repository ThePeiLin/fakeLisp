#ifndef FKL_READER_H
#define FKL_READER_H
#include"ast.h"
#include"bytecode.h"
#include"vm.h"
#include"pattern.h"
#include<stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
char* fklReadInPattern(FILE*,char**,int*);
char* fklReadInStringPattern(FILE*,char**,uint32_t,int*,FklPtrStack* retval,char* (*)(FILE*,int*));
#ifdef __cplusplus
}
#endif

#endif
