#ifndef FKL_SYMBOL_H
#define FKL_SYMBOL_H
#include<stdint.h>
#include<stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FKL_NUM_OF_BUILT_IN_SYMBOL 59
typedef uint32_t FklSid_t;

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
	FKL_INVOKEERROR,
	FKL_LOADDLLFAILD,
	FKL_INVALIDSYMBOL,
	FKL_LIBUNDEFINED,
	FKL_UNEXPECTEOF,
	FKL_DIVZERROERROR,
	FKL_FILEFAILURE,
	FKL_CANTDEREFERENCE,
	FKL_CANTGETELEM,
	FKL_INVALIDMEMBER,
	FKL_NOMEMBERTYPE,
	FKL_NONSCALARTYPE,
	FKL_INVALIDASSIGN,
	FKL_INVALIDACCESS,
	FKL_IMPORTFAILED,
}FklErrorType;

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
const char* fklGetBuiltInSymbol(FklErrorType);
const char* fklGetBuiltInErrorType(FklErrorType);
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

#ifdef __cplusplus
}
#endif

#endif
