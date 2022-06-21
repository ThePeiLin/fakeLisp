#include"fsym.h"
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
