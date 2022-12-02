#include"fsym.h"
#include<fakeLisp/utils.h>

//void fklcInitFsym(void)
//{
//	OuterSymbolTable=fklCreateSymbolTable();
//}
//
//FklSymbolTable* fklcGetOuterSymbolTable(void)
//{
//	return OuterSymbolTable;
//}
//
//void fklcUninitFsym(void)
//{
//	fklDestroySymbolTable(OuterSymbolTable);
//}
//
//FklSid_t fklcGetSymbolIdWithOuterSymbolId(FklSid_t sid)
//{
//	const FklString* sym=fklGetSymbolWithId(sid,OuterSymbolTable)->symbol;
//	return fklAddSymbolToGlob(sym)->id;
//}

void fklFklcInitErrorTypeId(FklSid_t fklcErrorTypeId[FKL_FKLC_ERROR_NUM],FklSymbolTable* table)
{
	static const char* fklcErrType[]=
	{
		"invalid-type-declare",
		"invalid-type-name",
		"invalid-mem-mode",
		"invalid-selector",
		"invalid-assign",
	};
	for(size_t i=0;i<FKL_FKLC_ERROR_NUM;i++)
		fklcErrorTypeId[i]=fklAddSymbolCstr(fklcErrType[i],table)->id;
}

FklSid_t fklcGetErrorType(FklFklcErrType type,FklSid_t fklcErrorTypeId[FKL_FKLC_ERROR_NUM])
{
	return fklcErrorTypeId[type];
}

FklString* fklcGenErrorMessage(FklFklcErrType type)
{
	FklString* t=NULL;
	switch(type)
	{
		case FKL_FKLC_ERR_IMCOMPILABLE_OBJ_OCCUR:
			t=fklCreateStringFromCstr("imcompilable obj occur");
			break;
		case FKL_FKLC_ERR_INVALID_SYNTAX_PATTERN:
			t=fklCreateStringFromCstr("invalid syntax pattern ");
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
	return t;
}

