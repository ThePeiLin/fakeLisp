#ifndef FKL_LEXER_H
#define FKL_LEXER_H

#include"base.h"
#include"pattern.h"
#include<stddef.h>
#include<stdio.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	FKL_TOKEN_RESERVE_STR=0,
	FKL_TOKEN_CHAR,
	FKL_TOKEN_NUM,
	FKL_TOKEN_STRING,
	FKL_TOKEN_SYMBOL,
	FKL_TOKEN_COMMENT,
}FklTokenType;

#define FKL_INCOMPLETED_TOKEN ((void*)0x1)
typedef struct
{
	FklTokenType type;
	FklString* value;
	size_t line;
}FklToken;

FklToken* fklCreateToken(FklTokenType type,FklString* str,size_t line);
FklToken* fklCreateTokenCopyStr(FklTokenType type,const FklString* str,size_t line);
void fklDestroyToken(FklToken* token);
void fklPrintToken(FklPtrStack*,FILE* fp);

FklStringMatchSet* fklSplitStringIntoTokenWithPattern(const char* buf
		,size_t size
		,size_t line
		,size_t* pline
		,size_t j
		,size_t* pj
		,FklPtrStack* retvalStack
		,FklStringMatchSet* matchSet
		,FklStringMatchPattern* patterns
		,FklStringMatchRouteNode* route
		,FklStringMatchRouteNode** proute
		,int* err);
int fklIsAllComment(FklPtrStack*);
#ifdef __cplusplus
}
#endif

#endif
