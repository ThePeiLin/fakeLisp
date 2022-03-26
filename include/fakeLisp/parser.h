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
	FKL_TOKEN_BYTS,
	FKL_TOKEN_STRING,
	FKL_TOKEN_SYMBOL,
	FKL_TOKEN_COMMENT,
}FklTokenType;

typedef struct
{
	FklTokenType type;
	char* value;
	uint32_t line;
}FklToken;

int fklSpiltStringPartsIntoToken(char** parts
		,uint32_t inum
		,uint32_t line
		,FklPtrStack* retvalStack);
void fklPrintToken(FklPtrStack*,FILE* fp);

#ifdef __cplusplus
}
#endif

#endif
