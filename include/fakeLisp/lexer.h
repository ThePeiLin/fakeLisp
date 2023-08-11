#ifndef FKL_PARSER_H
#define FKL_PARSER_H
#include"base.h"
#include"pattern.h"
#include<stddef.h>
#include<stdio.h>

#ifdef __cplusplus
extern "C"{
#endif


typedef struct FklGrammerSym
{
	FklSid_t nt:61;
	unsigned int isterm:1;
	unsigned int no_delim:1;
	unsigned int skip_space:1;
	unsigned int repeat:1;
}FklGrammerSym;

typedef struct FklGrammerProduction
{
	FklSid_t left;
	size_t len;
	size_t idx;
	FklGrammerSym* syms;
	struct FklGrammerProduction* next;
	// union
	// {
	// 	FklByteCodelnt* proc;
	// 	void* func;
	// }action;
	int isBuiltin;
}FklGrammerProduction;

typedef enum
{
	FKL_LALR_LOOKAHEAD_NONE,
	FKL_LALR_LOOKAHEAD_EOF,
	FKL_LALR_LOOKAHEAD_STRING,
	FKL_LALR_LOOKAHEAD_BUILTIN,
}FklLalrItemLookAheadType;

#define FKL_LALR_LOOKAHEAD_NONE_INIT ((FklLalrItemLookAhead){.t=FKL_LALR_LOOKAHEAD_NONE,.u.ptr=NULL})
#define FKL_LALR_LOOKAHEAD_EOF_INIT ((FklLalrItemLookAhead){.t=FKL_LALR_LOOKAHEAD_EOF,.u.ptr=NULL})

typedef int (*FklBuiltinLookAhead)(const char* str,size_t* matchLen);

typedef struct
{
	FklLalrItemLookAheadType t;
	union
	{
		const FklString* s;
		FklBuiltinLookAhead func;
		void* ptr;
	}u;
}FklLalrItemLookAhead;

typedef struct
{
	FklGrammerProduction* prod;
	uint32_t idx;
	FklLalrItemLookAhead la;
}FklLalrItem;

typedef struct FklLalrItemSetLink
{
	FklGrammerSym sym;
	struct FklLalrItemSet* dst;
	struct FklLalrItemSetLink* next;
}FklLalrItemSetLink;

typedef struct FklLookAheadSpreads
{
	FklLalrItem src;
	FklHashTable* dstItems;
	FklLalrItem dst;
	struct FklLookAheadSpreads* next;
}FklLookAheadSpreads;

typedef struct FklLalrItemSet
{
	FklHashTable* items;
	FklLookAheadSpreads* spreads;
	FklLalrItemSetLink* links;
}FklLalrItemSet;

typedef enum
{
	FKL_ANALYSIS_SHIFT,
	FKL_ANALYSIS_ACCEPT,
	FKL_ANALYSIS_REDUCE,
	FKL_ANALYSIS_IGNORE,
}FklAnalysisStateActionEnum;

typedef struct FklAnalysisStateGoto
{
	FklSid_t nt;
	struct FklAnalysisState* state;
	struct FklAnalysisStateGoto* next;
}FklAnalysisStateGoto;

typedef struct FklAnalysisStateAction
{
	FklLalrItemLookAhead la;
	FklAnalysisStateActionEnum action;
	union
	{
		const struct FklAnalysisState* state;
		const FklGrammerProduction* prod;
	}u;

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

	FklSid_t start;
	FklHashTable* nonterminals;

	size_t prodNum;
	FklHashTable* productions;

	FklAnalysisTable aTable;
}FklGrammer;

FklHashTable* fklCreateTerminalIndexSet(FklString* const* terminals,size_t num);

FklGrammer* fklCreateGrammerFromCstr(const char* str[],FklSymbolTable* st);

FklHashTable* fklGenerateLr0Items(FklGrammer* grammer);

int fklGenerateLalrAnalyzeTable(FklGrammer* grammer,FklHashTable* states);
void fklPrintAnalysisTable(const FklGrammer* grammer,const FklSymbolTable* st,FILE* fp);
void fklPrintAnalysisTableForGraphEasy(const FklGrammer* grammer,const FklSymbolTable* st,FILE* fp);

void fklLr0ToLalrItems(FklHashTable*,FklGrammer* grammer);

void fklPrintItemSet(const FklHashTable* itemSet,const FklGrammer* g,const FklSymbolTable* st,FILE* fp);
void fklPrintItemStateSet(const FklHashTable* i
		,const FklGrammer* g
		,const FklSymbolTable* st
		,FILE* fp);

void fklPrintItemStateSetAsDot(const FklHashTable* i
		,const FklGrammer* g
		,const FklSymbolTable* st
		,FILE* fp);

FklGrammerProduction* fklGetGrammerProductions(const FklGrammer* g,FklSid_t left);
void fklPrintGrammerProduction(FILE* fp,const FklGrammerProduction* prod,const FklSymbolTable* st,const FklSymbolTable* tt);
void fklPrintGrammer(FILE* fp,const FklGrammer* grammer,FklSymbolTable* st);
int fklTokenizeCstr(FklGrammer* g,const char* str,FklPtrStack* stack);

int fklParseForCstr(const FklAnalysisTable* aTable,const char* str,FklPtrStack* stack);

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
