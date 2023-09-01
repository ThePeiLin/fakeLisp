#ifndef FKL_SYMBOL_H
#define FKL_SYMBOL_H

#include"base.h"
#include<stdint.h>
#include<stdio.h>

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

void fklSetSidKey(void* k0,const void* k1);
uintptr_t fklSidHashFunc(const void* k);
int fklSidKeyEqual(const void* k0,const void* k1);

FklSymbolTable* fklCreateSymbolTable();

FklSymTabNode* fklCreateSymTabNode(const FklString*);
FklSymTabNode* fklCreateSymTabNodeCstr(const char*);
FklSymTabNode* fklCreateSymTabNodeCharBuf(size_t len,const char* buf);

FklSymTabNode* fklAddSymbol(const FklString*,FklSymbolTable*);
FklSymTabNode* fklAddSymbolCstr(const char*,FklSymbolTable*);
FklSymTabNode* fklAddSymbolCharBuf(const char*,size_t,FklSymbolTable*);

FklSymTabNode* fklAddSymbolToPst(const FklString*);
FklSymTabNode* fklAddSymbolCstrToPst(const char*);
FklSymTabNode* fklAddSymbolCharBufToPst(const char*,size_t);

FklSymTabNode* fklGetSymbolWithId(FklSid_t id,const FklSymbolTable*);
FklSymTabNode* fklGetSymbolWithIdFromPst(FklSid_t id);

FklSymbolTable* fklGetPubSymTab(void);
void fklUninitPubSymTab(void);

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
void fklInitSidSet(FklHashTable* t);
#ifdef __cplusplus
}
#endif

#endif
