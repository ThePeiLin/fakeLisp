#ifndef FKL_BUILTIN_H
#define FKL_BUILTIN_H
#include"symbol.h"

typedef enum
{
	FKL_ERR_DUMMY=0,
	FKL_ERR_SYMUNDEFINE,
	FKL_ERR_SYNTAXERROR,
	FKL_ERR_INVALIDEXPR,
	FKL_ERR_CIRCULARLOAD,
	FKL_ERR_INVALIDPATTERN,
	FKL_ERR_INCORRECT_TYPE_VALUE,
	FKL_ERR_STACKERROR,
	FKL_ERR_TOOMANYARG,
	FKL_ERR_TOOFEWARG,
	FKL_ERR_CANTCREATETHREAD,
	FKL_ERR_THREADERROR,
	FKL_ERR_MACROEXPANDFAILED,
	FKL_ERR_CALL_ERROR,
	FKL_ERR_LOADDLLFAILD,
	FKL_ERR_INVALIDSYMBOL,
	FKL_ERR_LIBUNDEFINED,
	FKL_ERR_UNEXPECTEOF,
	FKL_ERR_DIVZEROERROR,
	FKL_ERR_FILEFAILURE,
	FKL_ERR_INVALID_VALUE,
	FKL_ERR_INVALIDASSIGN,
	FKL_ERR_INVALIDACCESS,
	FKL_ERR_IMPORTFAILED,
	FKL_ERR_INVALID_MACRO_PATTERN,
	FKL_ERR_FAILD_TO_CREATE_BIG_INT_FROM_MEM,
	FKL_ERR_LIST_DIFFER_IN_LENGTH,
	FKL_ERR_CROSS_C_CALL_CONTINUATION,
	FKL_ERR_INVALIDRADIX,
	FKL_ERR_NO_VALUE_FOR_KEY,
	FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,
	FKL_ERR_CIR_REF,
}FklBuiltInErrorType;

#define FKL_BUILTIN_ERR_NUM (FKL_ERR_CIR_REF+1)
//typedef struct FklCompEnv FklCompEnv;
typedef struct FklVMenv FklVMenv;
typedef struct FklVMgc FklVMgc;
typedef struct FklCodegenEnv FklCodegenEnv;

//void fklInitCompEnv(FklCompEnv* curEnv);
void fklInitGlobCodegenEnv(FklCodegenEnv*,FklSymbolTable*);
void fklInitGlobEnv(FklVMenv*,FklVMgc*,FklSymbolTable*);
void fklInitSymbolTableWithBuiltinSymbol(FklSymbolTable* symbolTable);
void fklInitBuiltinErrorType(FklSid_t errorTypeId[FKL_BUILTIN_ERR_NUM],FklSymbolTable* table);
FklSid_t fklGetBuiltInErrorType(FklBuiltInErrorType type,FklSid_t errorTypeId[FKL_ERR_INCORRECT_TYPE_VALUE]);
#endif
