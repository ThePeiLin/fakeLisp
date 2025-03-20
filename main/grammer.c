#include<fakeLisp/grammer.h>
#include<fakeLisp/base.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>

int main(int argc,char* argv[])
{
	if(argc<6)
		return 1;

	const char* outer_file_name=argv[1];
	const char* action_file_name=argv[2];
	const char* ast_creator_name=argv[3];
	const char* ast_destroy_name=argv[4];
	const char* state_0_push_name=argv[5];

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

	FILE* action_file=fopen(action_file_name,"r");
	FILE* parse=fopen(outer_file_name,"w");
	fklPrintAnalysisTableAsCfunc(g
			,st
			,action_file
			,ast_creator_name
			,ast_destroy_name
			,state_0_push_name
			,parse);
	fclose(parse);
	fclose(action_file);

	fklDestroySymbolTable(st);
	fklDestroyGrammer(g);
	fklDestroyHashTable(itemSet);
	return 0;
}
