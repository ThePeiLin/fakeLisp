#ifndef FKL_SYMBOL_H
#define FKL_SYMBOL_H
#include<stdint.h>
#include<stdio.h>
#include"base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t FklSid_t;

typedef struct FklSymTabNode
{
	FklString* symbol;
	FklSid_t id;
}FklSymTabNode;

typedef struct FklSymbolTable
{
	FklSid_t num;
	FklSymTabNode** list;
	FklSymTabNode** idl;
}FklSymbolTable;

FklSymbolTable* fklCreateSymbolTable();

FklSymTabNode* fklCreateSymTabNode(const FklString*);
FklSymTabNode* fklCreateSymTabNodeCstr(const char*);

FklSymTabNode* fklAddSymbol(const FklString*,FklSymbolTable*);
FklSymTabNode* fklAddSymbolCstr(const char*,FklSymbolTable*);
FklSymTabNode* fklAddSymbolCharBuf(const char*,size_t,FklSymbolTable*);

FklSymTabNode* fklFindSymbol(const FklString*,const FklSymbolTable*);
FklSymTabNode* fklFindSymbolCstr(const char*,const FklSymbolTable*);

FklSymTabNode* fklGetSymbolWithId(FklSid_t id,const FklSymbolTable*);

void fklPrintSymbolTable(const FklSymbolTable*,FILE*);

void fklDestroySymTabNode(FklSymTabNode*);
void fklDestroySymbolTable(FklSymbolTable*);

void fklWriteSymbolTable(const FklSymbolTable*,FILE*);

typedef struct FklFuncPrototype
{
	struct FklSymbolDef* refs;
	uint32_t lcount;
	uint32_t rcount;
	FklSid_t sid;
	FklSid_t fid;
	uint64_t line;
}FklFuncPrototype;

typedef struct
{
	FklFuncPrototype* pts;
	uint32_t count;
}FklFuncPrototypes;

typedef struct
{
	FklSid_t id;
	uint32_t scope;
}FklSidScope;

typedef struct FklSymbolDef
{
	FklSidScope k;
	uint32_t idx;
	uint32_t cidx;
	uint8_t isLocal;
}FklSymbolDef;

typedef struct //unresolved symbol ref
{
	FklSid_t id;
	uint32_t scope;
	uint32_t idx;
	uint32_t prototypeId;
	FklSid_t fid;
	uint64_t line;
}FklUnReSymbolRef;

FklFuncPrototypes* fklCreateFuncPrototypes(uint32_t count);
void fklWriteFuncPrototypes(const FklFuncPrototypes* pts,FILE* fp);
FklFuncPrototypes* fklLoadFuncPrototypes(FILE* fp);
FklSymbolDef* fklCreateSymbolDef(FklSid_t key,uint32_t scope,uint32_t idx,uint32_t cidx,uint8_t isLocal);

void fklUninitFuncPrototype(FklFuncPrototype* p);
void fklDestroyFuncPrototypes(FklFuncPrototypes* p);

FklHashTable* fklCreateSidSet(void);
#ifdef __cplusplus
}
#endif

#endif
