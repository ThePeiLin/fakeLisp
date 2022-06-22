#ifndef FKL_PARSER_H
#define FKL_PARSER_H
#include<stddef.h>
#include<stdio.h>
#include"basicADT.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	FKL_TOKEN_RESERVE_STR,
	FKL_TOKEN_CHAR,
	FKL_TOKEN_NUM,
	FKL_TOKEN_STRING,
	FKL_TOKEN_SYMBOL,
	FKL_TOKEN_LONG_SYMBOL,
	FKL_TOKEN_COMMENT,
}FklTokenType;

typedef struct
{
	FklTokenType type;
	FklString* value;
	uint32_t line;
}FklToken;

int fklSplitStringPartsIntoToken(char** parts
		,size_t* sizes
		,uint32_t inum
		,uint32_t* line
		,FklPtrStack* retvalStack
		,FklPtrStack* matchStateStack
		,uint32_t* pi
		,uint32_t* pj);
void fklPrintToken(FklPtrStack*,FILE* fp);

FklToken* fklNewToken(FklTokenType type,const FklString* str,uint32_t line);
void fklFreeToken(FklToken* token);
int fklIsAllComment(FklPtrStack*);
#ifdef __cplusplus
}
#endif

#endif
