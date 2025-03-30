#include <fakeLisp/common.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t len;
    const char *buf;
} SymbolTableKey;

static void symbol_hash_set_val(void *d0, const void *d1) {
    *(FklSymbolHashItem *)d0 = *(const FklSymbolHashItem *)d1;
}

static void symbol_other_hash_set_val(void *d0, const void *d1) {
    FklSymbolHashItem *n = (FklSymbolHashItem *)d0;
    const SymbolTableKey *k = (const SymbolTableKey *)d1;
    n->symbol = fklCreateString(k->len, k->buf);
    n->id = 0;
}

static int symbol_hash_key_equal(const void *k0, const void *k1) {
    return fklStringEqual(*(const FklString **)k0, *(const FklString **)k1);
}

static int symbol_other_hash_key_equal(const void *k0, const void *k1) {
    const SymbolTableKey *k = (const SymbolTableKey *)k1;
    return !fklStringCharBufCmp(*(const FklString **)k0, k->len, k->buf);
}

static uintptr_t symbol_hash_func(const void *k0) {
    return fklStringHash(*(const FklString **)k0);
}

static uintptr_t symbol_other_hash_func(const void *k0) {
    const SymbolTableKey *k = (const SymbolTableKey *)k0;
    return fklCharBufHash(k->buf, k->len);
}

static void symbol_hash_uninit(void *d) {
    FklSymbolHashItem *dd = (FklSymbolHashItem *)d;
    free(dd->symbol);
}

static const FklHashTableMetaTable SymbolHashMetaTable = {
    .size = sizeof(FklSymbolHashItem),
    .__getKey = fklHashDefaultGetKey,
    .__setKey = fklPtrKeySet,
    .__setVal = symbol_hash_set_val,
    .__keyEqual = symbol_hash_key_equal,
    .__hashFunc = symbol_hash_func,
    .__uninitItem = symbol_hash_uninit,
};

void fklInitSymbolTable(FklSymbolTable *tmp) {
    tmp->idl = NULL;
    tmp->num = 0;
    tmp->idl_size = 0;
    fklInitHashTable(&tmp->ht, &SymbolHashMetaTable);
}

FklSymbolTable *fklCreateSymbolTable() {
    FklSymbolTable *tmp = (FklSymbolTable *)malloc(sizeof(FklSymbolTable));
    FKL_ASSERT(tmp);
    fklInitSymbolTable(tmp);
    return tmp;
}

FklSymbolHashItem *fklAddSymbol(const FklString *sym, FklSymbolTable *table) {
    return fklAddSymbolCharBuf(sym->str, sym->size, table);
}

FklSymbolHashItem *fklAddSymbolCstr(const char *sym, FklSymbolTable *table) {
    return fklAddSymbolCharBuf(sym, strlen(sym), table);
}

FklSymbolHashItem *fklAddSymbolCharBuf(const char *buf, size_t len,
                                       FklSymbolTable *table) {
    FklHashTable *ht = &table->ht;
    SymbolTableKey key = {.len = len, .buf = buf};
    FklSymbolHashItem *node = fklGetOrPutWithOtherKey(
        &key, symbol_other_hash_func, symbol_other_hash_key_equal,
        symbol_other_hash_set_val, ht);
    if (node->id == 0) {
        node->id = ht->num;
        table->num++;
        if (table->num >= table->idl_size) {
            table->idl_size += FKL_DEFAULT_INC;
            table->idl = (FklSymbolHashItem **)fklRealloc(
                table->idl, table->idl_size * sizeof(FklSymbolHashItem *));
            FKL_ASSERT(table->idl);
        }
        table->idl[table->num - 1] = node;
    }
    return node;
}

void fklUninitSymbolTable(FklSymbolTable *table) {
    fklUninitHashTable(&table->ht);
    free(table->idl);
}

void fklDestroySymbolTable(FklSymbolTable *table) {
    fklUninitSymbolTable(table);
    free(table);
}

FklSymbolHashItem *fklGetSymbolWithId(FklSid_t id,
                                      const FklSymbolTable *table) {
    if (id == 0)
        return NULL;
    return table->idl[id - 1];
}

#include <math.h>
void fklPrintSymbolTable(const FklSymbolTable *table, FILE *fp) {
    int numLen = table->num ? (int)(log10(table->num) + 1) : 1;
    fprintf(fp, "size:%" FKL_PRT64U "\n", table->num);
    for (uint32_t i = 0; i < table->num; i++) {
        FklSymbolHashItem *cur = table->idl[i];
        fprintf(fp, "%-*" FKL_PRT64U ":\t", numLen, cur->id);
        fklPrintRawSymbol(cur->symbol, fp);
        fputc('\n', fp);
    }
}

void fklWriteSymbolTable(const FklSymbolTable *table, FILE *fp) {
    fwrite(&table->num, sizeof(table->num), 1, fp);
    for (uint64_t i = 0; i < table->num; i++)
        fwrite(table->idl[i]->symbol,
               table->idl[i]->symbol->size
                   + sizeof(table->idl[i]->symbol->size),
               1, fp);
}

void fklLoadSymbolTable(FILE *fp, FklSymbolTable *table) {
    uint64_t size = 0;
    fread(&size, sizeof(size), 1, fp);
    for (uint64_t i = 0; i < size; i++) {
        uint64_t len = 0;
        fread(&len, sizeof(len), 1, fp);
        FklString *buf = fklCreateString(len, NULL);
        fread(buf->str, len, 1, fp);
        fklAddSymbol(buf, table);
        free(buf);
    }
}

static inline void init_as_empty_pt(FklFuncPrototype *pt) {
    pt->refs = NULL;
    pt->lcount = 0;
    pt->rcount = 0;
}

void fklInitFuncPrototypes(FklFuncPrototypes *r, uint32_t count) {
    r->count = count;
    r->pa = (FklFuncPrototype *)malloc((count + 1) * sizeof(FklFuncPrototype));
    FKL_ASSERT(r->pa);
    init_as_empty_pt(r->pa);
}

FklFuncPrototypes *fklCreateFuncPrototypes(uint32_t count) {
    FklFuncPrototypes *r = (FklFuncPrototypes *)malloc(sizeof(*r));
    FKL_ASSERT(r);
    fklInitFuncPrototypes(r, count);
    return r;
}

uint32_t fklInsertEmptyFuncPrototype(FklFuncPrototypes *pts) {
    pts->count++;
    FklFuncPrototype *pa = (FklFuncPrototype *)fklRealloc(
        pts->pa, (pts->count + 1) * sizeof(FklFuncPrototype));
    FKL_ASSERT(pts);
    pts->pa = pa;
    FklFuncPrototype *cpt = &pa[pts->count];
    memset(cpt, 0, sizeof(FklFuncPrototype));
    return pts->count;
}

void fklUninitFuncPrototype(FklFuncPrototype *p) {
    free(p->refs);
    p->rcount = 0;
}

void fklDestroyFuncPrototypes(FklFuncPrototypes *p) {
    if (p) {
        fklUninitFuncPrototypes(p);
        free(p);
    }
}

void fklUninitFuncPrototypes(FklFuncPrototypes *p) {
    FklFuncPrototype *pts = p->pa;
    uint32_t end = p->count + 1;
    for (uint32_t i = 1; i < end; i++)
        fklUninitFuncPrototype(&pts[i]);
    free(pts);
}

static inline void write_symbol_def(const FklSymbolDef *def, FILE *fp) {
    fwrite(&def->k.id, sizeof(def->k.id), 1, fp);
    fwrite(&def->k.scope, sizeof(def->k.scope), 1, fp);
    fwrite(&def->idx, sizeof(def->idx), 1, fp);
    fwrite(&def->cidx, sizeof(def->cidx), 1, fp);
    fwrite(&def->isLocal, sizeof(def->isLocal), 1, fp);
}

static inline void write_prototype(const FklFuncPrototype *pt, FILE *fp) {
    uint32_t count = pt->lcount;
    fwrite(&count, sizeof(count), 1, fp);
    count = pt->rcount;
    FklSymbolDef *defs = pt->refs;
    fwrite(&count, sizeof(count), 1, fp);
    for (uint32_t i = 0; i < count; i++)
        write_symbol_def(&defs[i], fp);
    fwrite(&pt->sid, sizeof(pt->sid), 1, fp);
    fwrite(&pt->fid, sizeof(pt->fid), 1, fp);
    fwrite(&pt->line, sizeof(pt->line), 1, fp);
}

void fklWriteFuncPrototypes(const FklFuncPrototypes *pts, FILE *fp) {
    uint32_t count = pts->count;
    FklFuncPrototype *pta = pts->pa;
    fwrite(&count, sizeof(count), 1, fp);
    uint32_t end = count + 1;
    for (uint32_t i = 1; i < end; i++)
        write_prototype(&pta[i], fp);
}

static inline void load_symbol_def(FklSymbolDef *def, FILE *fp) {
    fread(&def->k.id, sizeof(def->k.id), 1, fp);
    fread(&def->k.scope, sizeof(def->k.scope), 1, fp);
    fread(&def->idx, sizeof(def->idx), 1, fp);
    fread(&def->cidx, sizeof(def->cidx), 1, fp);
    fread(&def->isLocal, sizeof(def->isLocal), 1, fp);
}

static inline void load_prototype(FklFuncPrototype *pt, FILE *fp) {
    uint32_t count = 0;

    fread(&count, sizeof(count), 1, fp);
    pt->lcount = count;
    fread(&count, sizeof(count), 1, fp);
    pt->rcount = count;
    FklSymbolDef *defs;
    if (count == 0)
        defs = NULL;
    else {
        defs = (FklSymbolDef *)calloc(count, sizeof(FklSymbolDef));
        FKL_ASSERT(defs);
        for (uint32_t i = 0; i < count; i++)
            load_symbol_def(&defs[i], fp);
    }

    pt->refs = defs;
    fread(&pt->sid, sizeof(pt->sid), 1, fp);
    fread(&pt->fid, sizeof(pt->fid), 1, fp);
    fread(&pt->line, sizeof(pt->line), 1, fp);
}

FklFuncPrototypes *fklLoadFuncPrototypes(FILE *fp) {
    uint32_t count = 0;
    fread(&count, sizeof(count), 1, fp);
    FklFuncPrototypes *pts = fklCreateFuncPrototypes(count);
    FklFuncPrototype *pta = pts->pa;
    for (uint32_t i = 1; i <= count; i++)
        load_prototype(&pta[i], fp);
    return pts;
}

void fklSetSidKey(void *k0, const void *k1) {
    *(FklSid_t *)k0 = *(const FklSid_t *)k1;
}

uintptr_t fklSidHashFunc(const void *k) { return *(const FklSid_t *)k; }

int fklSidKeyEqual(const void *k0, const void *k1) {
    return *(const FklSid_t *)k0 == *(const FklSid_t *)k1;
}

static const FklHashTableMetaTable SidSetMetaTable = {
    .size = sizeof(FklSid_t),
    .__setKey = fklSetSidKey,
    .__setVal = fklSetSidKey,
    .__hashFunc = fklSidHashFunc,
    .__keyEqual = fklSidKeyEqual,
    .__getKey = fklHashDefaultGetKey,
    .__uninitItem = fklDoNothingUninitHashItem,
};

FklHashTable *fklCreateSidSet(void) {
    return fklCreateHashTable(&SidSetMetaTable);
}

void fklInitSidSet(FklHashTable *t) { fklInitHashTable(t, &SidSetMetaTable); }

void fklPutSidToSidSet(FklHashTable *t, FklSid_t id) { fklPutHashItem(&id, t); }
