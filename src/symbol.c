#include<fakeLisp/symbol.h>
#include<fakeLisp/utils.h>
#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

FklSymbolTable* fklCreateSymbolTable()
{
	FklSymbolTable* tmp=(FklSymbolTable*)malloc(sizeof(FklSymbolTable));
	FKL_ASSERT(tmp);
	tmp->list=NULL;
	tmp->idl=NULL;
	tmp->num=0;
	return tmp;
}

FklSymTabNode* fklCreateSymTabNodeCstr(const char* symbol)
{
	FklSymTabNode* tmp=(FklSymTabNode*)malloc(sizeof(FklSymTabNode));
	FKL_ASSERT(tmp);
	tmp->id=0;
	tmp->symbol=fklCreateStringFromCstr(symbol);
	return tmp;
}

FklSymTabNode* fklCreateSymTabNode(const FklString* symbol)
{
	FklSymTabNode* tmp=(FklSymTabNode*)malloc(sizeof(FklSymTabNode));
	FKL_ASSERT(tmp);
	tmp->id=0;
	tmp->symbol=fklCopyString(symbol);
	return tmp;
}

FklSymTabNode* fklAddSymbol(const FklString* sym,FklSymbolTable* table)
{
	FklSymTabNode* node=NULL;
	if(!table->list)
	{
		node=fklCreateSymTabNode(sym);
		table->num=1;
		node->id=table->num;
		table->list=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->list);
		table->idl=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->idl);
		table->list[0]=node;
		table->idl[0]=node;
	}
	else
	{
		int32_t l=0;
		int32_t h=table->num-1;
		int32_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			int r=fklStringcmp(table->list[mid]->symbol,sym);
			if(r>0)
				h=mid-1;
			else if(r<0)
				l=mid+1;
			else
			{
				node=table->list[mid];
				return node;
			}
		}
		if(fklStringcmp(table->list[mid]->symbol,sym)<=0)
			mid++;
		table->num+=1;
		int32_t i=table->num-1;
		table->list=(FklSymTabNode**)realloc(table->list,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->list);
		node=fklCreateSymTabNode(sym);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->num;
		table->idl=(FklSymTabNode**)realloc(table->idl,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->idl);
		table->idl[table->num-1]=node;
	}
	return node;
}

FklSymTabNode* fklAddSymbolCstr(const char* sym,FklSymbolTable* table)
{
	FklSymTabNode* node=NULL;
	if(!table->list)
	{
		node=fklCreateSymTabNodeCstr(sym);
		table->num=1;
		node->id=table->num;
		table->list=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->list);
		table->idl=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->idl);
		table->list[0]=node;
		table->idl[0]=node;
	}
	else
	{
		int32_t l=0;
		int32_t h=table->num-1;
		int32_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			int r=fklStringCstrCmp(table->list[mid]->symbol,sym);
			if(r>0)
				h=mid-1;
			else if(r<0)
				l=mid+1;
			else
			{
				node=table->list[mid];
				return node;
			}
		}
		if(fklStringCstrCmp(table->list[mid]->symbol,sym)<=0)
			mid++;
		table->num+=1;
		int32_t i=table->num-1;
		table->list=(FklSymTabNode**)realloc(table->list,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->list);
		node=fklCreateSymTabNodeCstr(sym);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->num;
		table->idl=(FklSymTabNode**)realloc(table->idl,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->idl);
		table->idl[table->num-1]=node;
	}
	return node;
}

//FklSymTabNode* fklAddSymbolToGlobCstr(const char* sym)
//{
//	if(!GlobSymbolTable)
//		GlobSymbolTable=fklCreateSymbolTable();
//	return fklAddSymbolCstr(sym,GlobSymbolTable);
//}

//FklSymTabNode* fklAddSymbolToGlob(const FklString* sym)
//{
//	if(!GlobSymbolTable)
//		GlobSymbolTable=fklCreateSymbolTable();
//	return fklAddSymbol(sym,GlobSymbolTable);
//}

void fklDestroySymTabNode(FklSymTabNode* node)
{
	free(node->symbol);
	free(node);
}

void fklDestroySymbolTable(FklSymbolTable* table)
{
	int32_t i=0;
	for(;i<table->num;i++)
		fklDestroySymTabNode(table->list[i]);
	free(table->list);
	free(table->idl);
	free(table);
}

//void fklDestroyGlobSymbolTable()
//{
//	int32_t i=0;
//	for(;i<GlobSymbolTable->num;i++)
//		fklDestroySymTabNode(GlobSymbolTable->list[i]);
//	free(GlobSymbolTable->list);
//	free(GlobSymbolTable->idl);
//	free(GlobSymbolTable);
//	GlobSymbolTable=NULL;
//}

FklSymTabNode* fklFindSymbolCstr(const char* symbol,FklSymbolTable* table)
{
	FklSymTabNode* retval=NULL;
	if(table->list)
	{
		int32_t l=0;
		int32_t h=table->num-1;
		int32_t mid;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			int resultOfCmp=fklStringCstrCmp(table->list[mid]->symbol,symbol);
			if(resultOfCmp>0)
				h=mid-1;
			else if(resultOfCmp<0)
				l=mid+1;
			else
			{
				retval=table->list[mid];
				break;
			}
		}
	}
	return retval;
}

//FklSymTabNode* fklFindSymbolInGlobCstr(const char* sym)
//{
//	return fklFindSymbolCstr(sym,GlobSymbolTable);
//}

//FklSymTabNode* fklGetGlobSymbolWithId(FklSid_t id)
//{
//	return fklGetSymbolWithId(id,GlobSymbolTable);
//}

FklSymTabNode* fklGetSymbolWithId(FklSid_t id,FklSymbolTable* table)
{
	if(id==0)
		return NULL;
	return table->idl[id-1];
}

void fklPrintSymbolTable(FklSymbolTable* table,FILE* fp)
{
	int32_t i=0;
	for(;i<table->num;i++)
	{
		FklSymTabNode* cur=table->list[i];
		fprintf(fp,"symbol:");
		fklPrintString(cur->symbol,fp);
		fprintf(fp," id:%lu\n",cur->id);
	}
	fprintf(fp,"size:%lu\n",table->num);
}

//void fklPrintGlobSymbolTable(FILE* fp)
//{
//	fklPrintSymbolTable(GlobSymbolTable,fp);
//}

void fklWriteSymbolTable(FklSymbolTable* table,FILE* fp)
{
	fwrite(&table->num,sizeof(table->num),1,fp);
	for(uint64_t i=0;i<table->num;i++)
		fwrite(table->idl[i]->symbol,table->idl[i]->symbol->size+sizeof(table->idl[i]->symbol->size),1,fp);
}

//void fklWriteGlobSymbolTable(FILE* fp)
//{
//	fklWriteSymbolTable(GlobSymbolTable,fp);
//}

//FklSymbolTable* fklGetGlobSymbolTable(void)
//{
//	if(!GlobSymbolTable)
//		GlobSymbolTable=fklCreateSymbolTable();
//	return GlobSymbolTable;
//}

//void fklSetGlobSymbolTable(FklSymbolTable* t)
//{
//	GlobSymbolTable=t;
//}

//FklSymbolTable* fklExchangeGlobSymbolTable(FklSymbolTable* other)
//{
//	FklSymbolTable* r=GlobSymbolTable;
//	GlobSymbolTable=other;
//	return r;
//}
