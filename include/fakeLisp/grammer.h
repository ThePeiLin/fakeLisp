#ifndef FKL_GRAMMER_H
#define FKL_GRAMMER_H

#include"base.h"
#include"symbol.h"
#include"nast.h"
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
			,size_t restLen
			,size_t* matchLen
			,FklGrammerMatchOuterCtx* outerCtx
			,int* is_waiting_for_more);
	int (*ctx_cmp)(const void* c0,const void* c1);
	int (*ctx_equal)(const void* c0,const void* c1);
	uintptr_t (*ctx_hash)(const void* c);
	void* (*ctx_create)(const struct FklGrammerSym* next,const FklSymbolTable* tt,int* failed);
	void* (*ctx_global_create)(size_t,struct FklGrammerProduction* prod,struct FklGrammer* g,int* failed);
	void (*ctx_destroy)(void*);
	const char* name;
	void (*print_src)(const struct FklGrammer* g,FILE* fp);
	void (*print_c_match_cond)(void* ctx,FILE* fp);
}FklLalrBuiltinMatch;

typedef struct
{
	const FklLalrBuiltinMatch* t;
	void* c;
}FklLalrBuiltinGrammerSym;

typedef struct
{
	FklSid_t group;
	FklSid_t sid;
}FklGrammerNonterm;

typedef struct FklGrammerSym
{
	unsigned int delim:1;
	unsigned int isterm:1;
	unsigned int isbuiltin:1;
	unsigned int end_with_terminal:1;
	union
	{
		FklGrammerNonterm nt;
		FklLalrBuiltinGrammerSym b;
	};
}FklGrammerSym;

struct FklGrammerProduction;

uint64_t fklGetFirstNthLine(FklNastNode* nodes[],size_t num,size_t line);

typedef FklNastNode* (*FklBuiltinProdAction)(void* ctx,FklNastNode* nodes[],size_t num,size_t line,FklSymbolTable* st);

typedef struct FklGrammerProduction
{
	FklGrammerNonterm left;
	size_t len;
	size_t idx;
	FklGrammerSym* syms;
	struct FklGrammerProduction* next;
	int isBuiltin;
	const char* name;
	void* ctx;
	FklBuiltinProdAction func;
	void* (*ctx_copyer)(const void*);
	void (*ctx_destroyer)(void*);
}FklGrammerProduction;

typedef struct
{
	FklGrammerNonterm left;
	FklGrammerProduction* prods;
}FklGrammerProdHashItem;

FklGrammerProduction* fklCopyUninitedGrammerProduction(FklGrammerProduction* prod);
void fklInitGrammerProductionTable(FklHashTable*);
FklHashTable* fklCreateGrammerProductionTable(void);

typedef enum
{
	FKL_LALR_MATCH_NONE,
	FKL_LALR_MATCH_EOF,
	FKL_LALR_MATCH_STRING,
	FKL_LALR_MATCH_BUILTIN,
	FKL_LALR_MATCH_STRING_END_WITH_TERMINAL,
}FklLalrMatchType;

#define FKL_LALR_MATCH_NONE_INIT ((FklLalrItemLookAhead){.t=FKL_LALR_MATCH_NONE,})
#define FKL_LALR_MATCH_EOF_INIT ((FklLalrItemLookAhead){.t=FKL_LALR_MATCH_EOF,})

typedef struct
{
	FklLalrMatchType t;
	unsigned int delim:1;
	unsigned int end_with_terminal:1;
	union
	{
		const FklString* s;
		FklLalrBuiltinGrammerSym b;
	};
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
	FklGrammerNonterm nt;
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
	};
}FklAnalysisStateActionMatch;

typedef struct FklAnalysisStateAction
{
	FklAnalysisStateActionMatch match;

	FklAnalysisStateActionEnum action;
	union
	{
		const struct FklAnalysisState* state;
		const FklGrammerProduction* prod;
	};

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
	};
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

	FklSymbolTable* reachable_terminals;

	size_t sortedTerminalsNum;
	const FklString** sortedTerminals;

	FklGrammerNonterm start;

	size_t prodNum;
	FklHashTable productions;

	FklHashTable builtins;

	FklHashTable firstSets;

	FklGrammerIgnore* ignores;

	FklAnalysisTable aTable;
}FklGrammer;

typedef struct
{
	const char* cstr;
	const char* action_name;
	FklBuiltinProdAction func;
}FklGrammerCstrAction;

typedef struct
{
	FklGrammerNonterm nt;
	FklNastNode* ast;
}FklAnalyzingSymbol;

int fklInitGrammer(FklGrammer* g);
void fklDestroyAnayzingSymbol(FklAnalyzingSymbol*);
FklHashTable* fklCreateTerminalIndexSet(FklString* const* terminals,size_t num);

FklGrammer* fklCreateGrammerFromCstr(const char* str[],FklSymbolTable* st);
FklGrammer* fklCreateGrammerFromCstrAction(const FklGrammerCstrAction prodAction[],FklSymbolTable* st);

int fklAddProdAndExtraToGrammer(FklGrammer* g,FklGrammerProduction* prod);
int fklAddProdToGrammer(FklHashTable* productions
		,FklHashTable* builtins
		,FklGrammerProduction* prod);
void fklDestroyGrammer(FklGrammer*);
void fklClearGrammer(FklGrammer*);

FklHashTable* fklGenerateLr0Items(FklGrammer* grammer);

int fklGenerateLalrAnalyzeTable(FklGrammer* grammer,FklHashTable* states);
void fklPrintAnalysisTable(const FklGrammer* grammer,const FklSymbolTable* st,FILE* fp);
void fklPrintAnalysisTableForGraphEasy(const FklGrammer* grammer,const FklSymbolTable* st,FILE* fp);
void fklPrintAnalysisTableAsCfunc(const FklGrammer* grammer
		,const FklSymbolTable* st
		,FILE* action_src_fp
		,FILE* fp);

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

FklGrammerProduction* fklCreateEmptyProduction(FklSid_t group
		,FklSid_t sid
		,size_t len
		,const char* name
		,FklBuiltinProdAction func
		,void* ctx
		,void (*destroy)(void*)
		,void* (*copyer)(const void*));

FklGrammerProduction* fklCreateProduction(FklSid_t group
		,FklSid_t sid
		,size_t len
		,FklGrammerSym* syms
		,const char* name
		,FklBuiltinProdAction func
		,void* ctx
		,void (*destroy)(void*)
		,void* (*copyer)(const void*));

void fklProdCtxDestroyFree(void* c);
void fklProdCtxDestroyDoNothing(void* c);
void* fklProdCtxCopyerDoNothing(const void* c);

void fklDestroyGrammerProduction(FklGrammerProduction* h);
void fklUninitGrammerSymbols(FklGrammerSym* syms,size_t len);
FklGrammerIgnore* fklGrammerSymbolsToIgnore(FklGrammerSym* syms
		,size_t len
		,const FklSymbolTable* tt);
const FklLalrBuiltinMatch* fklGetBuiltinMatch(const FklHashTable* ht,FklSid_t id);

int fklIsNonterminalExist(FklHashTable* prods,FklSid_t group,FklSid_t id);

FklGrammerProduction* fklGetProductions(const FklHashTable* prod,FklSid_t group,FklSid_t id);
FklGrammerProduction* fklGetGrammerProductions(const FklGrammer* g,FklSid_t group,FklSid_t id);

void fklPrintGrammerProduction(FILE* fp,const FklGrammerProduction* prod,const FklSymbolTable* st,const FklSymbolTable* tt);
void fklPrintGrammer(FILE* fp,const FklGrammer* grammer,FklSymbolTable* st);
int fklTokenizeCstr(FklGrammer* g,const char* str,FklPtrStack* stack);

FklNastNode* fklParseWithTableForCstr(const FklGrammer*
		,const char* str
		,FklGrammerMatchOuterCtx*
		,FklSymbolTable* st
		,int* err);

FklNastNode* fklParseWithTableForCstrDbg(const FklGrammer*
		,const char* str
		,FklGrammerMatchOuterCtx*
		,FklSymbolTable* st
		,int* err);

FklNastNode* fklParseWithTableForCharBuf(const FklGrammer*
		,const char* str
		,size_t len
		,size_t* restLen
		,FklGrammerMatchOuterCtx*
		,FklSymbolTable* st
		,int* err
		,size_t* errLine
		,FklPtrStack* symbols
		,FklPtrStack* states);

#define FKL_PARSE_TERMINAL_MATCH_FAILED (1)
#define FKL_PARSE_REDUCE_FAILED (2)
#define FKL_PARSE_WAITING_FOR_MORE (3)

typedef int (*FklStateFuncPtr)(FklPtrStack*
		,FklPtrStack*
		,int
		,FklSid_t
		,void**
		,const char*
		,const char**
		,size_t*
		,FklGrammerMatchOuterCtx*
		,FklSymbolTable*
		,int*
		,size_t* errLine);

void fklPushState0ToStack(FklPtrStack* stateStack);

FklNastNode* fklDefaultParseForCstr(const char* str
		,FklGrammerMatchOuterCtx*
		,FklSymbolTable* st
		,int* err
		,size_t* errLine
		,FklPtrStack* symbols
		,FklPtrStack* states);

FklNastNode* fklDefaultParseForCharBuf(const char* str
		,size_t len
		,size_t* restLen
		,FklGrammerMatchOuterCtx*
		,FklSymbolTable* st
		,int* err
		,size_t* errLine
		,FklPtrStack* symbols
		,FklPtrStack* states);

FklGrammerIgnore* fklInitBuiltinProductionSet(FklHashTable* ht
		,FklSymbolTable* st
		,FklSymbolTable* tt
		,FklHashTable* builtins);

FklGrammer* fklCreateEmptyGrammer(FklSymbolTable* st);
FklGrammer* fklCreateBuiltinGrammer(FklSymbolTable* st);

int fklAddIgnoreToGrammer(FklGrammerIgnore** pp,FklGrammerIgnore* ig);
void fklDestroyIgnore(FklGrammerIgnore* ig);

size_t fklQuotedStringMatch(const char* cstr,size_t restLen,const FklString* end);
size_t fklQuotedCharBufMatch(const char* cstr,size_t restLen,const char* end,size_t end_size);

int fklIsDecInt(const char* cstr,size_t maxLen);
int fklIsOctInt(const char* cstr,size_t maxLen);
int fklIsHexInt(const char* cstr,size_t maxLen);
int fklIsDecFloat(const char* cstr,size_t maxLen);
int fklIsHexFloat(const char* cstr,size_t maxLen);

#ifdef __cplusplus
}
#endif

#endif
