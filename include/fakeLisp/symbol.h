#ifndef FKL_SYMBOL_H
#define FKL_SYMBOL_H
#include<stdint.h>
#include<stdio.h>
#include<pthread.h>
#include<fakeLisp/basicADT.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FKL_NUM_OF_BUILT_IN_SYMBOL (103)
#define FKL_NUM_OF_BUILT_IN_ERROR_TYPE (31)
typedef uint64_t FklSid_t;

typedef enum
{
	FKL_SYMUNDEFINE=1,
	FKL_SYNTAXERROR,
	FKL_INVALIDEXPR,
	FKL_INVALIDTYPEDEF,
	FKL_CIRCULARLOAD,
	FKL_INVALIDPATTERN,
	FKL_WRONGARG,
	FKL_STACKERROR,
	FKL_TOOMANYARG,
	FKL_TOOFEWARG,
	FKL_CANTCREATETHREAD,
	FKL_THREADERROR,
	FKL_MACROEXPANDFAILED,
	FKL_CALL_ERROR,
	FKL_LOADDLLFAILD,
	FKL_INVALIDSYMBOL,
	FKL_LIBUNDEFINED,
	FKL_UNEXPECTEOF,
	FKL_DIVZERROERROR,
	FKL_FILEFAILURE,
	FKL_INVALIDMEMBER,
	FKL_NOMEMBERTYPE,
	FKL_NONSCALARTYPE,
	FKL_INVALIDASSIGN,
	FKL_INVALIDACCESS,
	FKL_IMPORTFAILED,
	FKL_INVALID_MACRO_PATTERN,
	FKL_FAILD_TO_CREATE_BIG_INT_FROM_MEM,
	FKL_LIST_DIFFER_IN_LENGTH,
	FKL_CROSS_C_CALL_CONTINUATION,
}FklErrorType;

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

const char** fklGetBuiltInSymbolList(void);
const char* fklGetBuiltInSymbol(uint64_t);
FklSid_t fklGetBuiltInErrorType(FklErrorType);
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
