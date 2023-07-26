#ifndef FKL_PARSER_H
#define FKL_PARSER_H
#include"base.h"
#include"pattern.h"
#include<stddef.h>
#include<stdio.h>

#ifdef __cplusplus
extern "C"{
#endif


typedef struct FklGrammerProductionUnit
{
	FklSid_t nt:63;
	unsigned int term:1;
	unsigned int delim:1;
	unsigned int space:1;
	unsigned int repeat:1;
}FklGrammerProductionUnit;

typedef struct FklGrammerProduction
{
	FklSid_t left;
	size_t len;
	FklGrammerProductionUnit* units;
	struct FklGrammerProduction* next;
	union
	{
		FklByteCodelnt* proc;
		void* func;
	}action;
	int isBuiltin;
}FklGrammerProduction;

typedef struct
{
	FklGrammerProduction* production;
	uint32_t idx;
	FklString* lookAhead;
}FklLalrItem;

typedef enum FklAnalysisAction
{
	FKL_ANALYSIS_SHIFT,
	FKL_ANALYSIS_REDUCE,
}FklAnalysisAction;

typedef struct FklAnalysisStateGoto
{
	FklSid_t nt;
	uint32_t idx;
	struct FklAnalysisStateGoto* next;
}FklAnalysisStateGoto;

typedef struct FklAnalysisStateAction
{
	FklString* str;
	FklAnalysisAction action;
	uint32_t idx;

	struct FklAnalysisStateAction* next;
}FklAnalysisStateAction;

typedef struct FklAnalysisState
{
	unsigned int builtin:1;
	union
	{
		void* func;
		struct
		{
			FklAnalysisStateAction* action;
			FklAnalysisStateGoto* gt;
		}state;
	};
}FklAnalysisState;

typedef struct FklAnalysisTable
{
	size_t num;
	FklAnalysisState* states;
}FklAnalysisTable;

typedef struct
{
	FklSymbolTable* terminals;

	FklSid_t* nonterminals;
	size_t ntNum;

	FklHashTable* productions;

	FklAnalysisTable aTable;
}FklLalr1Grammer;

FklHashTable* fklCreateTerminalIndexSet(FklString* const* terminals,size_t num);

FklLalr1Grammer* fklCreateLalr1GrammerFromCstr(const char* str[],FklSymbolTable* st);

void fklPrintGrammerProduction(FILE* fp,const FklGrammerProduction* prod,const FklSymbolTable* st,const FklSymbolTable* tt);
int fklLalr1TokenizeCstr(FklLalr1Grammer* g,const char* str,FklPtrStack* stack);

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
