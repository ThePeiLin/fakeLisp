#ifndef FKL_FKLC_SYM
#define FKL_FKLC_SYM
#include<fakeLisp/symbol.h>
#ifdef __cplusplus
extern "C"{
#endif

void fklcInitFsym(FklSymbolTable*);
FklSymbolTable* fklcGetOuterSymbolTable(void);
void fklcUninitFsym(void);
FklSid_t fklcGetSymbolIdWithOuterSymbolId(FklSid_t);
typedef enum
{
	FKL_FKLC_ERR_IMCOMPILABLE_OBJ_OCCUR,
}FklFklcErrType;

char* fklcGenErrorMessage(FklFklcErrType);
FklSid_t fklcGetErrorType(FklFklcErrType);
#ifdef __cplusplus
}
#endif
#endif
