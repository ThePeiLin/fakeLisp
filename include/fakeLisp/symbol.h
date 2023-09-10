#ifndef FKL_SYMBOL_H
#define FKL_SYMBOL_H

#include"base.h"
#include<stdint.h>
#include<stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t FklSid_t;

typedef struct
{
	FklString* symbol;
	FklSid_t id;
}FklSymbolHashItem;

typedef struct FklSymboTable
{
	FklSid_t num;
	size_t idl_size;
	FklSymbolHashItem** idl;
	FklHashTable ht;
}FklSymbolTable;

void fklSetSidKey(void* k0,const void* k1);
uintptr_t fklSidHashFunc(const void* k);
int fklSidKeyEqual(const void* k0,const void* k1);

void fklInitSymbolTable(FklSymbolTable* st);
FklSymbolTable* fklCreateSymbolTable();

FklSymbolHashItem* fklAddSymbol(const FklString*,FklSymbolTable*);
FklSymbolHashItem* fklAddSymbolCstr(const char*,FklSymbolTable*);
FklSymbolHashItem* fklAddSymbolCharBuf(const char*,size_t,FklSymbolTable*);

FklSymbolHashItem* fklAddSymbolToPst(const FklString*);
FklSymbolHashItem* fklAddSymbolCstrToPst(const char*);
FklSymbolHashItem* fklAddSymbolCharBufToPst(const char*,size_t);

FklSymbolHashItem* fklGetSymbolWithId(FklSid_t id,const FklSymbolTable*);
FklSymbolHashItem* fklGetSymbolWithIdFromPst(FklSid_t id);

FklSymbolTable* fklGetPubSymTab(void);
void fklUninitPubSymTab(void);

void fklPrintSymbolTable(const FklSymbolTable*,FILE*);

void fklDestroySymTabNode(FklSymbolHashItem*);
void fklDestroySymbolTable(FklSymbolTable*);
void fklUninitSymbolTable(FklSymbolTable*);

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
void fklInitSidSet(FklHashTable* t);
#ifdef __cplusplus
}
#endif

#endif
