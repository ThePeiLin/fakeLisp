#ifndef FKL_PARSER_H
#define FKL_PARSER_H
#include<stddef.h>
#include"basicADT.h"
typedef enum
{
	FKL_TOKEN_RESERVE_STR,
	FKL_TOKEN_CHAR,
	FKL_TOKEN_NUM,
	FKL_TOKEN_BYTS,
	FKL_TOKEN_STRING,
	FKL_TOKEN_SYMBOL,
}FklTokenType;

typedef struct
{
	FklTokenType type;
	char* value;
}FklToken;

FklPtrStack* spiltStringPartsIntoToken(char** parts,uint32_t inum);
#endif
