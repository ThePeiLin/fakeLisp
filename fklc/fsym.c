#include"fsym.h"
#include<fakeLisp/utils.h>
static FklSymbolTable* OuterSymbolTable=NULL;

void fklcInitFsym(FklSymbolTable* table)
{
	OuterSymbolTable=table;
}

FklSymbolTable* fklcGetOuterSymbolTable(void)
{
	return OuterSymbolTable;
}

void fklcUninitFsym(void)
{
	if(fklGetGlobSymbolTable())
		fklFreeGlobSymbolTable();
}

FklSid_t fklcGetSymbolIdWithOuterSymbolId(FklSid_t sid)
{
	const FklString* sym=fklGetSymbolWithId(sid,OuterSymbolTable)->symbol;
	return fklAddSymbolToGlob(sym)->id;
}

static const char* FklcErrType[]=
{
	"invalid-type-declare",
	"invalid-type-name",
	"invalid-mem-mode",
	"invalid-selector",
	"invalid-assign",
};

FklSid_t fklcGetErrorType(FklFklcErrType type)
{
	static FklSid_t fklcErrorTypeId[sizeof(FklcErrType)/sizeof(const char*)]={0};
	if(!fklcErrorTypeId[type])
		fklcErrorTypeId[type]=fklAddSymbolToGlobCstr(FklcErrType[type])->id;
	return fklcErrorTypeId[type];
}

char* fklcGenErrorMessage(FklFklcErrType type)
{
	char* t=fklCopyCstr("");
	switch(type)
	{
		case FKL_FKLC_ERR_IMCOMPILABLE_OBJ_OCCUR:
			t=fklStrCat(t,"imcompilable obj occur ");
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
	return t;
}
