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

#define FKL_FKLC_ERROR_NUM (FKL_FKLC_ERR_INVALID_SYNTAX_PATTERN+1)

typedef struct
{
	FklSid_t bcUdSid;
	FklSymbolTable* outerSymbolTable;
	FklSid_t fklcErrorTypeId[FKL_FKLC_ERROR_NUM];
}FklFklcPublicData;

void fklFklcInitErrorTypeId(FklSid_t fklcErrorTypeId[FKL_FKLC_ERROR_NUM]
		,FklSymbolTable* table);
FklString* fklcGenErrorMessage(FklFklcErrType);
FklSid_t fklcGetErrorType(FklFklcErrType,FklSid_t fklcErrorTypeId[FKL_FKLC_ERROR_NUM]);
#ifdef __cplusplus
}
#endif
#endif
