#ifndef FKL_SYMBOL_H
#define FKL_SYMBOL_H
#include<stdint.h>
#include<stdio.h>
typedef uint32_t FklSid_t;
typedef struct FklSymTabNode
{
	char* symbol;
	int32_t id;
}FklSymTabNode;

typedef struct FklSymbolTable
{
	int32_t num;
	FklSymTabNode** list;
	FklSymTabNode** idl;
}FklSymbolTable;

const char** fklGetBuiltInSymbolList(void);
const char** fklGetBuiltInErrorType(void);
FklSymbolTable* fklNewSymbolTable();
FklSymTabNode* fklNewSymTabNode(const char*);
FklSymTabNode* fklAddSymbol(const char*,FklSymbolTable*);
FklSymTabNode* fklAddSymbolToGlob(const char*);
FklSymTabNode* fklFindSymbol(const char*,FklSymbolTable*);
FklSymTabNode* fklFindSymbolInGlob(const char*);
FklSymTabNode* fklGetGlobSymbolWithId(FklSid_t id);
void fklPrintSymbolTable(FklSymbolTable*,FILE*);
void fklPrintGlobSymbolTable(FILE*);
void fklFreeSymTabNode(FklSymTabNode*);
void fklFreeSymbolTable(FklSymbolTable*);
void fklFreeGlobSymbolTable();

void fklWriteSymbolTable(FklSymbolTable*,FILE*);
void fklWriteGlobSymbolTable(FILE*);


#endif
