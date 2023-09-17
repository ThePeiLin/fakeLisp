#ifndef FKL_BUILTIN_H
#define FKL_BUILTIN_H

#include"symbol.h"
#include"bytecode.h"

#ifdef __cplusplus
extern "C" {
#endif

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
	FKL_ERR_UNEXPECTED_EOF,
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
	FKL_ERR_UNSUPPORTED_OP,
	FKL_ERR_IMPORT_MISSING,
	FKL_ERR_EXPORT_OUTER_REF_PROD_GROUP,
	FKL_ERR_IMPORT_READER_MACRO_ERROR,
	FKL_ERR_ANALYSIS_TABLE_GENERATE_FAILED,
	FKL_ERR_GRAMMER_CREATE_FAILED,
}FklBuiltInErrorType;

typedef FklByteCodelnt* (*FklBuiltinInlineFunc)(FklByteCodelnt*[],FklSid_t,uint64_t);
FklBuiltinInlineFunc fklGetBuiltInInlineFunc(uint32_t idx,uint32_t argNum);

uint8_t* fklGetBuiltinSymbolModifyMark(uint32_t*);
#define FKL_BUILTIN_ERR_NUM (FKL_ERR_CIR_REF+1)
typedef struct FklCodegenEnv FklCodegenEnv;
typedef struct FklVM FklVM;
struct FklVMframe;
typedef struct FklVMproc FklVMproc;
void fklInitGlobCodegenEnv(FklCodegenEnv*,FklSymbolTable* pst);
void fklInitSymbolTableWithBuiltinSymbol(FklSymbolTable* symbolTable);
void fklInitBuiltinErrorType(FklSid_t errorTypeId[FKL_BUILTIN_ERR_NUM],FklSymbolTable* table);
FklSid_t fklGetBuiltInErrorType(FklBuiltInErrorType type,FklSid_t errorTypeId[FKL_ERR_INCORRECT_TYPE_VALUE]);
void fklInitGlobalVMclosure(struct FklVMframe* frame,FklVM*);
void fklInitGlobalVMclosureForProc(FklVMproc*,FklVM*);

#define FKL_VM_STDIN_IDX (0)
#define FKL_VM_STDOUT_IDX (1)
#define FKL_VM_STDERR_IDX (2)

#ifdef __cplusplus
}
#endif

#endif
