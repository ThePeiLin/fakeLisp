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
