#include "fakeLisp/base.h"
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
		int64_t l=0;
		int64_t h=table->num-1;
		int64_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			int r=fklStringCmp(table->list[mid]->symbol,sym);
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
		if(fklStringCmp(table->list[mid]->symbol,sym)<=0)
			mid++;
		table->num+=1;
		int64_t i=table->num-1;
		table->list=(FklSymTabNode**)fklRealloc(table->list,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->list);
		node=fklCreateSymTabNode(sym);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->num;
		table->idl=(FklSymTabNode**)fklRealloc(table->idl,sizeof(FklSymTabNode*)*table->num);
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
		int64_t l=0;
		int64_t h=table->num-1;
		int64_t mid=0;
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
		int64_t i=table->num-1;
		table->list=(FklSymTabNode**)fklRealloc(table->list,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->list);
		node=fklCreateSymTabNodeCstr(sym);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->num;
		table->idl=(FklSymTabNode**)fklRealloc(table->idl,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->idl);
		table->idl[table->num-1]=node;
	}
	return node;
}

FklSymTabNode* fklAddSymbolCharBuf(const char* buf,size_t len,FklSymbolTable* table)
{
	FklSymTabNode* node=NULL;
	FklString* sym=fklCreateString(len,buf);
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
		int64_t l=0;
		int64_t h=table->num-1;
		int64_t mid=0;
		while(l<=h)
		{
			mid=l+(h-l)/2;
			int r=fklStringCmp(table->list[mid]->symbol,sym);
			if(r>0)
				h=mid-1;
			else if(r<0)
				l=mid+1;
			else
			{
				free(sym);
				node=table->list[mid];
				return node;
			}
		}
		if(fklStringCmp(table->list[mid]->symbol,sym)<=0)
			mid++;
		table->num+=1;
		int64_t i=table->num-1;
		table->list=(FklSymTabNode**)fklRealloc(table->list,sizeof(FklSymTabNode*)*table->num);
		FKL_ASSERT(table->list);
		node=fklCreateSymTabNode(sym);
		for(;i>mid;i--)
			table->list[i]=table->list[i-1];
		table->list[mid]=node;
		node->id=table->num;
		table->idl=(FklSymTabNode**)fklRealloc(table->idl,sizeof(FklSymTabNode*)*table->num);
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
	for(uint32_t i=0;i<table->num;i++)
		fklDestroySymTabNode(table->list[i]);
	free(table->list);
	free(table->idl);
	free(table);
}

FklSymTabNode* fklFindSymbolCstr(const char* symbol,const FklSymbolTable* table)
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

FklSymTabNode* fklGetSymbolWithId(FklSid_t id,const FklSymbolTable* table)
{
	if(id==0)
		return NULL;
	return table->idl[id-1];
}

void fklPrintSymbolTable(const FklSymbolTable* table,FILE* fp)
{
	for(uint32_t i=0;i<table->num;i++)
	{
		FklSymTabNode* cur=table->list[i];
		fprintf(fp,"symbol:");
		fklPrintString(cur->symbol,fp);
		fprintf(fp," id:%lu\n",cur->id);
	}
	fprintf(fp,"size:%lu\n",table->num);
}

void fklWriteSymbolTable(const FklSymbolTable* table,FILE* fp)
{
	fwrite(&table->num,sizeof(table->num),1,fp);
	for(uint64_t i=0;i<table->num;i++)
		fwrite(table->idl[i]->symbol,table->idl[i]->symbol->size+sizeof(table->idl[i]->symbol->size),1,fp);
}

static inline void init_as_empty_pt(FklFuncPrototype* pt)
{
	// pt->loc=NULL;
	pt->refs=NULL;
	pt->lcount=0;
	pt->rcount=0;
}

inline FklFuncPrototypes* fklCreateFuncPrototypes(uint32_t count)
{
	FklFuncPrototypes* r=(FklFuncPrototypes*)malloc(sizeof(*r));
	FKL_ASSERT(r);
	r->count=count;
	r->pts=(FklFuncPrototype*)malloc(sizeof(FklFuncPrototype)*(count+1));
	FKL_ASSERT(r->pts);
	init_as_empty_pt(r->pts);
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

void fklUninitFuncPrototype(FklFuncPrototype* p)
{
	free(p->refs);
	// free(p->loc);
	p->rcount=0;
}

void fklDestroyFuncPrototypes(FklFuncPrototypes* p)
{
	if(p)
	{
		FklFuncPrototype* pts=p->pts;
		uint32_t count=p->count;
		for(uint32_t i=1;i<=count;i++)
			fklUninitFuncPrototype(&pts[i]);
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

static inline void write_prototype(const FklFuncPrototype* pt,FILE* fp)
{
	uint32_t count=pt->lcount;
	// FklSymbolDef* defs=pt->loc;
	fwrite(&count,sizeof(count),1,fp);
	// for(uint32_t i=0;i<count;i++)
	// 	write_symbol_def(&defs[i],fp);
	count=pt->rcount;
	 FklSymbolDef* defs=pt->refs;
	fwrite(&count,sizeof(count),1,fp);
	for(uint32_t i=0;i<count;i++)
		write_symbol_def(&defs[i],fp);
	fwrite(&pt->sid,sizeof(pt->sid),1,fp);
	fwrite(&pt->fid,sizeof(pt->fid),1,fp);
	fwrite(&pt->line,sizeof(pt->line),1,fp);
}

inline void fklWriteFuncPrototypes(const FklFuncPrototypes* pts,FILE* fp)
{
	uint32_t count=pts->count;
	FklFuncPrototype* pta=pts->pts;
	fwrite(&count,sizeof(count),1,fp);
	for(uint32_t i=1;i<=count;i++)
		write_prototype(&pta[i],fp);
}

static inline void load_symbol_def(FklSymbolDef* def,FILE* fp)
{
	fread(&def->k.id,sizeof(def->k.id),1,fp);
	fread(&def->k.scope,sizeof(def->k.scope),1,fp);
	fread(&def->idx,sizeof(def->idx),1,fp);
	fread(&def->cidx,sizeof(def->cidx),1,fp);
	fread(&def->isLocal,sizeof(def->isLocal),1,fp);
}

static inline void load_prototype(FklFuncPrototype* pt,FILE* fp)
{
	uint32_t count=0;
	fread(&count,sizeof(count),1,fp);
	pt->lcount=count;
	// FklSymbolDef* defs=(FklSymbolDef*)malloc(sizeof(FklSymbolDef)*count);
	// FKL_ASSERT(defs||!count);
	// pt->loc=defs;
	// for(uint32_t i=0;i<count;i++)
	// 	load_symbol_def(&defs[i],fp);
	fread(&count,sizeof(count),1,fp);
	pt->rcount=count;
	FklSymbolDef* defs=(FklSymbolDef*)malloc(sizeof(FklSymbolDef)*count);
	FKL_ASSERT(defs||!count);
	pt->refs=defs;
	for(uint32_t i=0;i<count;i++)
		load_symbol_def(&defs[i],fp);
	fread(&pt->sid,sizeof(pt->sid),1,fp);
	fread(&pt->fid,sizeof(pt->fid),1,fp);
	fread(&pt->line,sizeof(pt->line),1,fp);
}

inline FklFuncPrototypes* fklLoadFuncPrototypes(FILE* fp)
{
	uint32_t count=0;
	fread(&count,sizeof(count),1,fp);
	FklFuncPrototypes* pts=fklCreateFuncPrototypes(count);
	FklFuncPrototype* pta=pts->pts;
	FKL_ASSERT(pts||!count);
	for(uint32_t i=1;i<=count;i++)
		load_prototype(&pta[i],fp);
	return pts;
}

static void fklSetSidKey(void* k0,void* k1)
{
	*(FklSid_t*)k0=*(FklSid_t*)k1;
}

static uintptr_t fklSidHashFunc(const void* k)
{
	return *(const FklSid_t*)k;
}

static int fklSidKeyEqual(const void* k0,const void* k1)
{
	return *(const FklSid_t*)k0==*(const FklSid_t*)k1;
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

