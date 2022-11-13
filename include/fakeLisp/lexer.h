#ifndef FKL_PARSER_H
#define FKL_PARSER_H
#include"base.h"
#include<stddef.h>
#include<stdio.h>

#ifdef __cplusplus
extern "C"{
#endif

int fklSplitStringPartsIntoToken(const char** parts
		,size_t* sizes
		,uint32_t inum
		,size_t* line
		,FklPtrStack* retvalStack
		,FklPtrStack* matchStateStack
		,uint32_t* pi
		,uint32_t* pj);

int fklIsAllComment(FklPtrStack*);
#ifdef __cplusplus
}
#endif

#endif
