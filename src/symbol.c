#include<fakeLisp/symbol.h>
#include<fakeLisp/utils.h>
#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

static const char* builtInErrorType[FKL_NUM_OF_BUILT_IN_ERROR_TYPE]=
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
	"invalid-member-symbol",
	"no-member-type",
	"non-scalar-type",
	"invalid-assign",
	"invalid-access",
	"inport-failed",
	"invalid-macro-pattern",
	"faild-to-create-big-int-from-mem",
	"differ-list-in-list",
	"cross-c-call-continuation",
};

static const char* builtInSymbolList[]=
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
	"char",
	"integer",
	"f64",
	"as-str",
	"symbol",
	"nth",
	"length",
	"apply",
	"call/cc",
	"fopen",
	"read",
	"prin1",
	"princ",
	"dlopen",
	"dlsym",
	"argv",
	"go",
	"chanl",
	"send",
	"recv",
	"error",
	"raise",
	"reverse",
	"i32",
	"i64",
	"fclose",
	"feof",
	"vref",
	"nthcdr",
	"char?",
	"integer?",
	"i32?",
	"i64?",
	"f64?",
	"pair?",
	"symbol?",
	"string?",
	"error?",
	"procedure?",
	"proc?",
	"dlproc?",
	"vector?",
	"chanl?",
	"dll?",
	"vector",
	"getdir",
	"fgetc",
	"fwrite",
	"to-str",
	"fgets",
	"to-int",
	"to-f64",
	"big-int?",
	"big-int",
	"set-car!",
	"set-cdr!",
	"set-nth!",
	"set-nthcdr!",
	"set-vref!",
	"list?",
	"box",
	"unbox",
	"set-box!",
	"box-cas!",
	"box?",
	"fix-int?",
	"number?",
	"map",
	"sref",
	"set-sref!",
	"foreach",
};

FklSymbolTable* GlobSymbolTable=NULL;

FklSymbolTable* fklNewSymbolTable()
{
	FklSymbolTable* tmp=(FklSymbolTable*)malloc(sizeof(FklSymbolTable));
	FKL_ASSERT(tmp,__func__);
	tmp->list=NULL;
	tmp->idl=NULL;
	tmp->num=0;
	pthread_rwlock_init(&tmp->rwlock,NULL);
	return tmp;
}

FklSymTabNode* fklNewSymTabNodeCstr(const char* symbol)
{
	FklSymTabNode* tmp=(FklSymTabNode*)malloc(sizeof(FklSymTabNode));
	FKL_ASSERT(tmp,__func__);
	tmp->id=0;
	tmp->symbol=fklNewStringFromCstr(symbol);
	return tmp;
}

FklSymTabNode* fklNewSymTabNode(const FklString* symbol)
{
	FklSymTabNode* tmp=(FklSymTabNode*)malloc(sizeof(FklSymTabNode));
	FKL_ASSERT(tmp,__func__);
	tmp->id=0;
	tmp->symbol=fklCopyString(symbol);
	return tmp;
}

FklSymTabNode* fklAddSymbol(const FklString* sym,FklSymbolTable* table)
{
	FklSymTabNode* node=NULL;
	pthread_rwlock_wrlock(&table->rwlock);
	if(!table->list)
	{
		node=fklNewSymTabNode(sym);
		table->num=1;
		node->id=table->num;
		table->list=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->list,__func__);
		table->idl=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->idl,__func__);
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
				pthread_rwlock_unlock(&table->rwlock);
				return node;
			}
		}
		if(fklStringcmp(table->list[mid]->symbol,sym)<=0)
			mid++;
		table->num+=1;
		int32_t i=table->num-1;
		table->list=(FklSymTabNode**)realloc(table->list,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->list,__func__);
		node=fklNewSymTabNode(sym);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->num;
		table->idl=(FklSymTabNode**)realloc(table->idl,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->idl,__func__);
		table->idl[table->num-1]=node;
	}
	pthread_rwlock_unlock(&table->rwlock);
	return node;
}

FklSymTabNode* fklAddSymbolCstr(const char* sym,FklSymbolTable* table)
{
	FklSymTabNode* node=NULL;
	pthread_rwlock_wrlock(&table->rwlock);
	if(!table->list)
	{
		node=fklNewSymTabNodeCstr(sym);
		table->num=1;
		node->id=table->num;
		table->list=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->list,__func__);
		table->idl=(FklSymTabNode**)malloc(sizeof(FklSymTabNode*)*1);
		FKL_ASSERT(table->idl,__func__);
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
				pthread_rwlock_unlock(&table->rwlock);
				return node;
			}
		}
		if(fklStringCstrCmp(table->list[mid]->symbol,sym)<=0)
			mid++;
		table->num+=1;
		int32_t i=table->num-1;
		table->list=(FklSymTabNode**)realloc(table->list,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->list,__func__);
		node=fklNewSymTabNodeCstr(sym);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->num;
		table->idl=(FklSymTabNode**)realloc(table->idl,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->idl,__func__);
		table->idl[table->num-1]=node;
	}
	pthread_rwlock_unlock(&table->rwlock);
	return node;
}

FklSymTabNode* fklAddSymbolToGlobCstr(const char* sym)
{
	if(!GlobSymbolTable)
		GlobSymbolTable=fklNewSymbolTable();
	return fklAddSymbolCstr(sym,GlobSymbolTable);
}

FklSymTabNode* fklAddSymbolToGlob(const FklString* sym)
{
	if(!GlobSymbolTable)
		GlobSymbolTable=fklNewSymbolTable();
	return fklAddSymbol(sym,GlobSymbolTable);
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
	for(;i<GlobSymbolTable->num;i++)
		fklFreeSymTabNode(GlobSymbolTable->list[i]);
	free(GlobSymbolTable->list);
	free(GlobSymbolTable->idl);
	free(GlobSymbolTable);
}

FklSymTabNode* fklFindSymbolCstr(const char* symbol,FklSymbolTable* table)
{
	FklSymTabNode* retval=NULL;
	pthread_rwlock_rdlock(&table->rwlock);
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
	pthread_rwlock_unlock(&table->rwlock);
	return retval;
}

FklSymTabNode* fklFindSymbolInGlobCstr(const char* sym)
{
	return fklFindSymbolCstr(sym,GlobSymbolTable);
}

FklSymTabNode* fklGetGlobSymbolWithId(FklSid_t id)
{
	return fklGetSymbolWithId(id,GlobSymbolTable);
}

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

void fklPrintGlobSymbolTable(FILE* fp)
{
	fklPrintSymbolTable(GlobSymbolTable,fp);
}

void fklWriteSymbolTable(FklSymbolTable* table,FILE* fp)
{
	fwrite(&table->num,sizeof(table->num),1,fp);
	for(uint64_t i=0;i<table->num;i++)
		fwrite(table->idl[i]->symbol,table->idl[i]->symbol->size+sizeof(table->idl[i]->symbol->size),1,fp);
}

void fklWriteGlobSymbolTable(FILE* fp)
{
	fklWriteSymbolTable(GlobSymbolTable,fp);
}

const char** fklGetBuiltInSymbolList(void)
{
	return builtInSymbolList;
}

const char* fklGetBuiltInSymbol(uint64_t i)
{
	return builtInSymbolList[i];
}

FklSid_t fklGetBuiltInErrorType(FklErrorType type)
{
	static FklSid_t errorTypeId[FKL_NUM_OF_BUILT_IN_ERROR_TYPE]={0};
	if(!errorTypeId[type])
		errorTypeId[type]=fklAddSymbolToGlobCstr(builtInErrorType[type])->id;
	return errorTypeId[type];
}

FklSymbolTable* fklGetGlobSymbolTable(void)
{
	return GlobSymbolTable;
}

void fklSetGlobSymbolTable(FklSymbolTable* t)
{
	GlobSymbolTable=t;
}
