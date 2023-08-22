#ifndef FKL_PARSER_H
#define FKL_PARSER_H
#include"base.h"
#include"pattern.h"
#include<stddef.h>
#include<stdio.h>

#ifdef __cplusplus
extern "C"{
#endif


struct FklGrammerProduction;
struct FklGrammer;
struct FklGrammerSym;

typedef struct
{
	size_t maxNonterminalLen;
	size_t line;
	const char* start;
	const char* cur;
}FklGrammerMatchOuterCtx;

#define FKL_GRAMMER_MATCH_OUTER_CTX_INIT {.maxNonterminalLen=0,.line=1,.start=NULL,.cur=NULL}

typedef struct
{
	int (*match)(void* ctx
			,const char* start
			,const char* str
			,size_t* matchLen
			,FklGrammerMatchOuterCtx* outerCtx);
	int (*ctx_cmp)(const void* c0,const void* c1);
	int (*ctx_equal)(const void* c0,const void* c1);
	uintptr_t (*ctx_hash)(const void* c);
	void* (*ctx_create)(const struct FklGrammerSym* next,const FklSymbolTable* tt,int* failed);
	void* (*ctx_global_create)(size_t,struct FklGrammerProduction* prod,struct FklGrammer* g,int* failed);
	void (*ctx_destroy)(void*);
	const char* name;
}FklLalrBuiltinMatch;

typedef struct
{
	const FklLalrBuiltinMatch* t;
	void* c;
}FklLalrBuiltinGrammerSym;

typedef struct FklGrammerSym
{
	unsigned int delim:1;
	unsigned int isterm:1;
	unsigned int isbuiltin:1;
	union
	{
		FklSid_t nt;
		FklLalrBuiltinGrammerSym b;
	}u;
}FklGrammerSym;

struct FklGrammerProduction;
typedef FklNastNode* (*FklBuiltinProdAction)(const struct FklGrammerProduction* this,FklNastNode** nodes,size_t num,size_t line,FklSymbolTable* st);
typedef struct FklGrammerProduction
{
	FklSid_t left;
	size_t len;
	size_t idx;
	FklGrammerSym* syms;
	struct FklGrammerProduction* next;
	FklBuiltinProdAction func;
	// union
	// {
	// 	FklByteCodelnt* proc;
	// 	void* func;
	// }action;
	int isBuiltin;
}FklGrammerProduction;

typedef enum
{
	FKL_LALR_MATCH_NONE,
	FKL_LALR_MATCH_EOF,
	FKL_LALR_MATCH_STRING,
	FKL_LALR_MATCH_BUILTIN,
}FklLalrMatchType;

#define FKL_LALR_MATCH_NONE_INIT ((FklLalrItemLookAhead){.t=FKL_LALR_MATCH_NONE,})
#define FKL_LALR_MATCH_EOF_INIT ((FklLalrItemLookAhead){.t=FKL_LALR_MATCH_EOF,})

typedef struct
{
	FklLalrMatchType t;
	unsigned int delim:1;
	union
	{
		const FklString* s;
		FklLalrBuiltinGrammerSym b;
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
	FklHashTable items;
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

typedef struct
{
	FklLalrMatchType t;
	union
	{
		const FklString* str;
		FklLalrBuiltinGrammerSym func;
	}m;
}FklAnalysisStateActionMatch;

typedef struct FklAnalysisStateAction
{
	FklAnalysisStateActionMatch match;

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
	union
	{
		const FklString* str;
		FklLalrBuiltinGrammerSym b;
	}u;
	unsigned int isbuiltin:1;
}FklGrammerIgnoreSym;

typedef struct FklGrammerIgnore
{
	struct FklGrammerIgnore* next;
	size_t len;
	FklGrammerIgnoreSym ig[];
}FklGrammerIgnore;

typedef struct FklGrammer
{
	FklSymbolTable* terminals;

	size_t sortedTerminalsNum;
	const FklString** sortedTerminals;

	FklSid_t start;
	FklHashTable nonterminals;

	size_t prodNum;
	FklHashTable productions;

	FklHashTable builtins;

	FklGrammerIgnore* ignores;

	FklAnalysisTable aTable;
}FklGrammer;

typedef struct
{
	const char* cstr;
	FklBuiltinProdAction func;
}FklGrammerCstrAction;

FklHashTable* fklCreateTerminalIndexSet(FklString* const* terminals,size_t num);

FklGrammer* fklCreateGrammerFromCstr(const char* str[],FklSymbolTable* st);
FklGrammer* fklCreateGrammerFromCstrAction(const FklGrammerCstrAction prodAction[],FklSymbolTable* st);

void fklDestroyGrammer(FklGrammer*);

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

int fklParseForCstr(const FklAnalysisTable* aTable
		,const char* str
		,FklPtrStack* stack
		,FklGrammerMatchOuterCtx*
		,FklSymbolTable* st);
int fklParseForCstrDbg(const FklAnalysisTable* aTable
		,const char* str
		,FklPtrStack* stack
		,FklGrammerMatchOuterCtx*
		,FklSymbolTable* st);

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
