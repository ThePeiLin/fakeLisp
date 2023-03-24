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

void fklWriteSymbolTable(FklSymbolTable* table,FILE* fp)
{
	fwrite(&table->num,sizeof(table->num),1,fp);
	for(uint64_t i=0;i<table->num;i++)
		fwrite(table->idl[i]->symbol,table->idl[i]->symbol->size+sizeof(table->idl[i]->symbol->size),1,fp);
}

inline FklPrototypePool* fklCreatePrototypePool(void)
{
	FklPrototypePool* r=(FklPrototypePool*)malloc(sizeof(*r));
	FKL_ASSERT(r);
	r->count=0;
	r->pts=NULL;
	return r;
}

FklSymbolDef* fklCreateSymbolDef(FklSid_t key,uint32_t scope,uint32_t idx,uint32_t cidx,uint8_t isLocal)
{
	FklSymbolDef* r=(FklSymbolDef*)malloc(sizeof(FklSymbolDef));
	FKL_ASSERT(r);
	r->k.id=key;
	r->k.scope=scope;
	r->idx=idx;
	r->cidx=cidx;
	r->isLocal=isLocal;
	return r;
}

void fklUninitPrototype(FklPrototype* p)
{
	free(p->refs);
	free(p->loc);
	p->rcount=0;
}

void fklDestroyPrototypePool(FklPrototypePool* p)
{
	if(p)
	{
		FklPrototype* pts=p->pts;
		uint32_t count=p->count;
		for(uint32_t i=0;i<count;i++)
			fklUninitPrototype(&pts[i]);
		free(pts);
		free(p);
	}
}

static inline void write_symbol_def(const FklSymbolDef* def,FILE* fp)
{
	fwrite(&def->k.id,sizeof(def->k.id),1,fp);
	fwrite(&def->k.scope,sizeof(def->k.scope),1,fp);
	fwrite(&def->idx,sizeof(def->idx),1,fp);
	fwrite(&def->cidx,sizeof(def->cidx),1,fp);
	fwrite(&def->isLocal,sizeof(def->isLocal),1,fp);
}

static inline void write_prototype(const FklPrototype* pt,FILE* fp)
{
	uint32_t count=pt->lcount;
	FklSymbolDef* defs=pt->loc;
	fwrite(&count,sizeof(count),1,fp);
	for(uint32_t i=0;i<count;i++)
		write_symbol_def(&defs[i],fp);
	count=pt->rcount;
	defs=pt->refs;
	fwrite(&count,sizeof(count),1,fp);
	for(uint32_t i=0;i<count;i++)
		write_symbol_def(&defs[i],fp);
}

inline void fklWritePrototypePool(const FklPrototypePool* ptpool,FILE* fp)
{
	uint32_t count=ptpool->count;
	FklPrototype* pts=ptpool->pts;
	fwrite(&count,sizeof(count),1,fp);
	for(uint32_t i=0;i<count;i++)
		write_prototype(&pts[i],fp);
}

static inline void load_symbol_def(FklSymbolDef* def,FILE* fp)
{
	fread(&def->k.id,sizeof(def->k.id),1,fp);
	fread(&def->k.scope,sizeof(def->k.scope),1,fp);
	fread(&def->idx,sizeof(def->idx),1,fp);
	fread(&def->cidx,sizeof(def->cidx),1,fp);
	fread(&def->isLocal,sizeof(def->isLocal),1,fp);
}

static inline void load_prototype(FklPrototype* pt,FILE* fp)
{
	uint32_t count=0;
	fread(&count,sizeof(count),1,fp);
	pt->lcount=count;
	FklSymbolDef* defs=(FklSymbolDef*)malloc(sizeof(FklSymbolDef)*count);
	FKL_ASSERT(defs||!count);
	pt->loc=defs;
	for(uint32_t i=0;i<count;i++)
		load_symbol_def(&defs[i],fp);
	fread(&count,sizeof(count),1,fp);
	pt->rcount=count;
	defs=(FklSymbolDef*)malloc(sizeof(FklSymbolDef)*count);
	FKL_ASSERT(defs||!count);
	pt->refs=defs;
	for(uint32_t i=0;i<count;i++)
		load_symbol_def(&defs[i],fp);
}

inline FklPrototypePool* fklLoadPrototypePool(FILE* fp)
{
	FklPrototypePool* ptpool=fklCreatePrototypePool();
	uint32_t count=0;
	fread(&count,sizeof(count),1,fp);
	FklPrototype* pts=(FklPrototype*)malloc(sizeof(FklPrototype)*count);
	FKL_ASSERT(pts||!count);
	for(uint32_t i=0;i<count;i++)
		load_prototype(&pts[i],fp);
	ptpool->count=count;
	ptpool->pts=pts;
	return ptpool;
}

static void fklSetSidKey(void* k0,void* k1)
{
	*(FklSid_t*)k0=*(FklSid_t*)k1;
}

static size_t fklSidHashFunc(void* k)
{
	return *(FklSid_t*)k;
}

static int fklSidKeyEqual(void* k0,void* k1)
{
	return *(FklSid_t*)k0==*(FklSid_t*)k1;
}

static const FklHashTableMetaTable SidSetMetaTable=
{
	.size=sizeof(FklSid_t),
	.__setKey=fklSetSidKey,
	.__setVal=fklSetSidKey,
	.__hashFunc=fklSidHashFunc,
	.__keyEqual=fklSidKeyEqual,
	.__getKey=fklHashDefaultGetKey,
	.__uninitItem=fklDoNothingUnintHashItem,
};

FklHashTable* fklCreateSidSet(void)
{
	return fklCreateHashTable(&SidSetMetaTable);
}

