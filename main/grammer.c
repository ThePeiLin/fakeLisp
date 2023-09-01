#include<fakeLisp/grammer.h>
#include<fakeLisp/base.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<ctype.h>

int main()
{
	FklSymbolTable* st=fklCreateSymbolTable();
	FklGrammer* g=fklCreateBuiltinGrammer(st);
	if(!g)
	{
		fklDestroySymbolTable(st);
		fprintf(stderr,"garmmer create fail\n");
		exit(1);
	}

	FklHashTable* itemSet=fklGenerateLr0Items(g);
	fklLr0ToLalrItems(itemSet,g);

	if(fklGenerateLalrAnalyzeTable(g,itemSet))
	{
		fklDestroyHashTable(itemSet);
		fklDestroySymbolTable(st);
		fklDestroyGrammer(g);
		fprintf(stderr,"not lalr garmmer\n");
		return 1;
	}
	FILE* parse=fopen("parse.c","w");
	fklPrintAnalysisTableAsCfunc(g,st,stdin,parse);
	fclose(parse);
	fklDestroySymbolTable(st);
	fklDestroyGrammer(g);
	fklDestroyHashTable(itemSet);
	return 0;
}
