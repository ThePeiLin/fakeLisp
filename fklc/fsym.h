#ifndef FKL_FKLC_SYM
#define FKL_FKLC_SYM
#include<fakeLisp/vm.h>
#include<fakeLisp/symbol.h>
#ifdef __cplusplus
extern "C"{
#endif

void fklcInitFsym(void);
void fklcUninitFsym(void);
FklSid_t fklcGetSymbolIdWithOuterSymbolId(FklSid_t);
typedef enum
{
	FKL_FKLC_ERR_IMCOMPILABLE_OBJ_OCCUR,
	FKL_FKLC_ERR_INVALID_SYNTAX_PATTERN,
}FklFklcErrType;

int fklcIsValidSyntaxPattern(FklVMvalue* pattern);
int fklcMatchPattern(FklVMvalue* pattern,FklVMvalue* exp,FklVMhashTable* hash,FklVMgc*);
char* fklcGenErrorMessage(FklFklcErrType);
FklSid_t fklcGetErrorType(FklFklcErrType);
#ifdef __cplusplus
}
#endif
#endif
