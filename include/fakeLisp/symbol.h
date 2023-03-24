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

FklSymTabNode* fklFindSymbol(const FklString*,FklSymbolTable*);
FklSymTabNode* fklFindSymbolCstr(const char*,FklSymbolTable*);

FklSymTabNode* fklGetSymbolWithId(FklSid_t id,FklSymbolTable*);

void fklPrintSymbolTable(FklSymbolTable*,FILE*);

void fklDestroySymTabNode(FklSymTabNode*);
void fklDestroySymbolTable(FklSymbolTable*);

void fklWriteSymbolTable(FklSymbolTable*,FILE*);

typedef struct FklPrototype
{
	struct FklSymbolDef* loc;
	struct FklSymbolDef* refs;
	uint32_t lcount;
	uint32_t rcount;
}FklPrototype;

typedef struct
{
	FklPrototype* pts;
	uint32_t count;
}FklPrototypePool;

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

FklPrototypePool* fklCreatePrototypePool(void);
void fklWritePrototypePool(const FklPrototypePool* ptpool,FILE* fp);
FklPrototypePool* fklLoadPrototypePool(FILE* fp);
FklSymbolDef* fklCreateSymbolDef(FklSid_t key,uint32_t scope,uint32_t idx,uint32_t cidx,uint8_t isLocal);

void fklUninitPrototype(FklPrototype* p);
void fklDestroyPrototypePool(FklPrototypePool* p);

FklHashTable* fklCreateSidSet(void);
#ifdef __cplusplus
}
#endif

#endif
