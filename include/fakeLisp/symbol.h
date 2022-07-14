#ifndef FKL_SYMBOL_H
#define FKL_SYMBOL_H
#include<stdint.h>
#include<stdio.h>
#include<pthread.h>
#include<fakeLisp/basicADT.h>

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
	pthread_rwlock_t rwlock;
}FklSymbolTable;

FklSymbolTable* fklNewSymbolTable();

FklSymTabNode* fklNewSymTabNode(const FklString*);
FklSymTabNode* fklNewSymTabNodeCstr(const char*);

FklSymTabNode* fklAddSymbol(const FklString*,FklSymbolTable*);
FklSymTabNode* fklAddSymbolCstr(const char*,FklSymbolTable*);

FklSymTabNode* fklAddSymbolToGlob(const FklString*);
FklSymTabNode* fklAddSymbolToGlobCstr(const char*);

FklSymTabNode* fklFindSymbol(const FklString*,FklSymbolTable*);
FklSymTabNode* fklFindSymbolCstr(const char*,FklSymbolTable*);

FklSymTabNode* fklFindSymbolInGlob(const FklString*);
FklSymTabNode* fklFindSymbolInGlobCstr(const char*);

FklSymTabNode* fklGetSymbolWithId(FklSid_t id,FklSymbolTable*);
FklSymTabNode* fklGetGlobSymbolWithId(FklSid_t id);
void fklPrintSymbolTable(FklSymbolTable*,FILE*);
void fklPrintGlobSymbolTable(FILE*);
void fklFreeSymTabNode(FklSymTabNode*);
void fklFreeSymbolTable(FklSymbolTable*);
void fklFreeGlobSymbolTable();

void fklWriteSymbolTable(FklSymbolTable*,FILE*);
void fklWriteGlobSymbolTable(FILE*);

FklSymbolTable* fklGetGlobSymbolTable(void);
void fklSetGlobSymbolTable(FklSymbolTable*);
#ifdef __cplusplus
}
#endif

#endif
