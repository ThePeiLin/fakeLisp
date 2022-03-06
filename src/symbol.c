#include<fakeLisp/symbol.h>
#include<fakeLisp/utils.h>
#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

const char* builtInErrorType[NUM_OF_BUILT_IN_ERROR_TYPE]=
{
	"dummy",
	"symbol-undefined",
	"syntax-error",
	"invalid-expression",
	"invalid-type-def",
	"circular-load",
	"invalid-pattern",
	"wrong-types-of-arguements",
	"stack-error",
	"too-many-arguements",
	"too-few-arguements",
	"cant-create-threads",
	"thread-error",
	"macro-expand-error",
	"invoke-error",
	"load-dll-faild",
	"invalid-symbol",
	"library-undefined",
	"unexpect-eof",
	"div-zero-error",
	"file-failure",
	"cant-dereference",
	"cant-get-element",
	"invalid-member-symbol",
	"no-member-type",
	"non-scalar-type",
	"invalid-assign",
	"invalid-access",
};

const char* builtInSymbolList[]=
{
	"nil",
	"stdin",
	"stdout",
	"stderr",
	"car",
	"cdr",
	"cons",
	"append",
	"atom",
	"null",
	"not",
	"eq",
	"equal",
	"=",
	"+",
	"1+",
	"-",
	"-1+",
	"*",
	"/",
	"%",
	">",
	">=",
	"<",
	"<=",
	"chr",
	"int",
	"dbl",
	"str",
	"sym",
	"byts",
	"type",
	"nth",
	"length",
	"apply",
	"clcc",
	"file",
	"read",
	"getb",
	"prin1",
	"putb",
	"princ",
	"dll",
	"dlsym",
	"argv",
	"go",
	"chanl",
	"send",
	"recv",
	"error",
	"raise",
	"newf",
	"delf",
	"lfdl",
};

FklSymbolTable GlobSymbolTable=STATIC_SYMBOL_INIT;

FklSymbolTable* fklNewSymbolTable()
{
	FklSymbolTable* tmp=(FklSymbolTable*)malloc(sizeof(FklSymbolTable));
	FKL_ASSERT(tmp,"fklNewSymbolTable",__FILE__,__LINE__);
	tmp->list=NULL;
	tmp->idl=NULL;
	tmp->num=0;
	return tmp;
}

FklSymTabNode* fklNewSymTabNode(const char* symbol)
{
	FklSymTabNode* tmp=(FklSymTabNode*)malloc(sizeof(FklSymTabNode));
	FKL_ASSERT(tmp,"fklNewSymbolTable",__FILE__,__LINE__);
	tmp->id=0;
	tmp->symbol=fklCopyStr(symbol);
	return tmp;
}

FklSymTabNode* fklAddSymbol(const char* sym,FklSymbolTable* table)
{
	FklSymTabNode* node=NULL;
	if(!table->list)
	{
		node=fklNewSymTabNode(sym);
		table->num=1;
		node->id=table->num;
		table->list=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->list,"fklAddSymbol",__FILE__,__LINE__);
		table->idl=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->idl,"fklAddSymbol",__FILE__,__LINE__);
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
			int r=strcmp(table->list[mid]->symbol,sym);
			if(r>0)
				h=mid-1;
			else if(r<0)
				l=mid+1;
			else
				return table->list[mid];
		}
		if(strcmp(table->list[mid]->symbol,sym)<=0)
			mid++;
		table->num+=1;
		int32_t i=table->num-1;
		table->list=(FklSymTabNode**)realloc(table->list,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->list,"fklAddSymbol",__FILE__,__LINE__);
		node=fklNewSymTabNode(sym);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->num;
		table->idl=(FklSymTabNode**)realloc(table->idl,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->idl,"fklAddSymbol",__FILE__,__LINE__);
		table->idl[table->num-1]=node;
	}
	return node;
}

FklSymTabNode* fklAddSymbolToGlob(const char* sym)
{
	return fklAddSymbol(sym,&GlobSymbolTable);
}


void fklFreeSymTabNode(FklSymTabNode* node)
{
	free(node->symbol);
	free(node);
}

void fklFreeSymbolTable(FklSymbolTable* table)
{
	int32_t i=0;
	for(;i<table->num;i++)
		fklFreeSymTabNode(table->list[i]);
	free(table->list);
	free(table->idl);
	free(table);
}

void fklFreeGlobSymbolTable()
{
	int32_t i=0;
	for(;i<GlobSymbolTable.num;i++)
		fklFreeSymTabNode(GlobSymbolTable.list[i]);
	free(GlobSymbolTable.list);
	free(GlobSymbolTable.idl);
}

FklSymTabNode* fklFindSymbol(const char* symbol,FklSymbolTable* table)
{
	if(!table->list)
		return NULL;
	int32_t l=0;
	int32_t h=table->num-1;
	int32_t mid;
	while(l<=h)
	{
		mid=l+(h-l)/2;
		int resultOfCmp=strcmp(table->list[mid]->symbol,symbol);
		if(resultOfCmp>0)
			h=mid-1;
		else if(resultOfCmp<0)
			l=mid+1;
		else
			return table->list[mid];
	}
	return NULL;
}

FklSymTabNode* fklFindSymbolInGlob(const char* sym)
{
	return fklFindSymbol(sym,&GlobSymbolTable);
}

FklSymTabNode* fklGetGlobSymbolWithId(FklSid_t id)
{
	if(id==0)
		return NULL;
	return GlobSymbolTable.idl[id-1];
}

void fklPrintSymbolTable(FklSymbolTable* table,FILE* fp)
{
	int32_t i=0;
	for(;i<table->num;i++)
	{
		FklSymTabNode* cur=table->list[i];
		fprintf(fp,"symbol:%s id:%d\n",cur->symbol,cur->id+1);
	}
	fprintf(fp,"size:%d\n",table->num);
}

void fklPrintGlobSymbolTable(FILE* fp)
{
	fklPrintSymbolTable(&GlobSymbolTable,fp);
}

void fklWriteSymbolTable(FklSymbolTable* table,FILE* fp)
{
	int32_t size=table->num;
	fwrite(&size,sizeof(size),1,fp);
	int32_t i=0;
	for(;i<size;i++)
		fwrite(table->idl[i]->symbol,strlen(table->idl[i]->symbol)+1,1,fp);
}

void fklWriteGlobSymbolTable(FILE* fp)
{
	fklWriteSymbolTable(&GlobSymbolTable,fp);
}

const char** fklGetBuiltInSymbolList(void)
{
	return builtInSymbolList;
}

const char** fklGetBuiltInErrorType(void)
{
	return builtInErrorType;
}
