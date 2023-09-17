#include<fakeLisp/symbol.h>
#include<fakeLisp/utils.h>
#include<ctype.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

typedef struct
{
	size_t len;
	const char* buf;
}SymbolTableKey;

static void symbol_hash_set_val(void* d0,const void* d1)
{
	*(FklSymbolHashItem*)d0=*(const FklSymbolHashItem*)d1;
}

static void symbol_other_hash_set_val(void* d0,const void* d1)
{
	FklSymbolHashItem* n=(FklSymbolHashItem*)d0;
	const SymbolTableKey* k=(const SymbolTableKey*)d1;
	n->symbol=fklCreateString(k->len,k->buf);
	n->id=0;
}

static int symbol_hash_key_equal(const void* k0,const void* k1)
{
	return fklStringEqual(*(const FklString**)k0,*(const FklString**)k1);
}

static int symbol_other_hash_key_equal(const void* k0,const void* k1)
{
	const SymbolTableKey* k=(const SymbolTableKey*)k1;
	return !fklStringCharBufCmp(*(const FklString**)k0,k->len,k->buf);
}

static uintptr_t symbol_hash_func(const void* k0)
{
	return fklStringHash(*(const FklString**)k0);
}

static uintptr_t symbol_other_hash_func(const void* k0)
{
	const SymbolTableKey* k=(const SymbolTableKey*)k0;
	return fklCharBufHash(k->buf,k->len);
}

static void symbol_hash_uninit(void* d)
{
	FklSymbolHashItem* dd=(FklSymbolHashItem*)d;
	free(dd->symbol);
}

static const FklHashTableMetaTable SymbolHashMetaTable=
{
	.size=sizeof(FklSymbolHashItem),
	.__getKey=fklHashDefaultGetKey,
	.__setKey=fklHashDefaultSetPtrKey,
	.__setVal=symbol_hash_set_val,
	.__keyEqual=symbol_hash_key_equal,
	.__hashFunc=symbol_hash_func,
	.__uninitItem=symbol_hash_uninit,
};

inline void fklInitSymbolTable(FklSymbolTable* tmp)
{
	tmp->idl=NULL;
	tmp->num=0;
	tmp->idl_size=0;
	fklInitHashTable(&tmp->ht,&SymbolHashMetaTable);
}

FklSymbolTable* fklCreateSymbolTable()
{
	FklSymbolTable* tmp=(FklSymbolTable*)malloc(sizeof(FklSymbolTable));
	FKL_ASSERT(tmp);
	fklInitSymbolTable(tmp);
	return tmp;
}

inline FklSymbolHashItem* fklAddSymbol(const FklString* sym,FklSymbolTable* table)
{
	return fklAddSymbolCharBuf(sym->str,sym->size,table);
}

inline FklSymbolHashItem* fklAddSymbolCstr(const char* sym,FklSymbolTable* table)
{
	return fklAddSymbolCharBuf(sym,strlen(sym),table);
}

inline FklSymbolHashItem* fklAddSymbolCharBuf(const char* buf,size_t len,FklSymbolTable* table)
{
	FklHashTable* ht=&table->ht;
	SymbolTableKey key={.len=len,.buf=buf};
	FklSymbolHashItem* node=fklGetOrPutWithOtherKey(&key
			,symbol_other_hash_func
			,symbol_other_hash_key_equal
			,symbol_other_hash_set_val
			,ht);
	if(node->id==0)
	{
		node->id=ht->num;
		table->num++;
		if(table->num>=table->idl_size)
		{
			table->idl_size+=FKL_DEFAULT_INC;
			table->idl=(FklSymbolHashItem**)fklRealloc(table->idl,sizeof(FklSymbolHashItem*)*table->idl_size);
			FKL_ASSERT(table->idl);
		}
		table->idl[table->num-1]=node;
	}
	return node;
}

void fklDestroySymTabNode(FklSymbolHashItem* node)
{
	free(node->symbol);
	free(node);
}

inline void fklUninitSymbolTable(FklSymbolTable* table)
{
	fklUninitHashTable(&table->ht);
	free(table->idl);
}

void fklDestroySymbolTable(FklSymbolTable* table)
{
	fklUninitSymbolTable(table);
	free(table);
}

FklSymbolHashItem* fklGetSymbolWithId(FklSid_t id,const FklSymbolTable* table)
{
	if(id==0)
		return NULL;
	return table->idl[id-1];
}

void fklPrintSymbolTable(const FklSymbolTable* table,FILE* fp)
{
	for(uint32_t i=0;i<table->num;i++)
	{
		FklSymbolHashItem* cur=table->idl[i];
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
	fwrite(&count,sizeof(count),1,fp);
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

void fklSetSidKey(void* k0,const void* k1)
{
	*(FklSid_t*)k0=*(const FklSid_t*)k1;
}

uintptr_t fklSidHashFunc(const void* k)
{
	return *(const FklSid_t*)k;
}

int fklSidKeyEqual(const void* k0,const void* k1)
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
	.__uninitItem=fklDoNothingUninitHashItem,
};

FklHashTable* fklCreateSidSet(void)
{
	return fklCreateHashTable(&SidSetMetaTable);
}

void fklInitSidSet(FklHashTable* t)
{
	fklInitHashTable(t,&SidSetMetaTable);
}
