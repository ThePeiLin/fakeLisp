#include "fakeLisp/base.h"
#include <fakeLisp/common.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fklInitSymbolTable(FklSymbolTable *tmp) {
    tmp->idl = NULL;
    tmp->num = 0;
    tmp->idl_size = 0;
    fklStrIdTableInit(&tmp->ht);
}

FklSymbolTable *fklCreateSymbolTable() {
    FklSymbolTable *tmp = (FklSymbolTable *)malloc(sizeof(FklSymbolTable));
    FKL_ASSERT(tmp);
    fklInitSymbolTable(tmp);
    return tmp;
}

FklStrIdTableElm *fklAddSymbol(const FklString *sym, FklSymbolTable *table) {
    return fklAddSymbolCharBuf(sym->str, sym->size, table);
}

FklStrIdTableElm *fklAddSymbolCstr(const char *sym, FklSymbolTable *table) {
    return fklAddSymbolCharBuf(sym, strlen(sym), table);
}

FklStrIdTableElm *fklAddSymbolCharBuf(const char *buf, size_t len,
                                      FklSymbolTable *table) {
    uintptr_t hashv = fklCharBufHash(buf, len);
    FklStrIdTable *ht = &table->ht;
    FklStrIdTableNode *const *bkt = fklStrIdTableBucket(ht, hashv);
    for (; *bkt; bkt = &(*bkt)->bkt_next) {
        FklStrIdTableNode *cur = *bkt;
        if (!fklStringCharBufCmp(cur->k, len, buf))
            return &cur->elm;
    }

    FklStrIdTableNode *node =
        fklStrIdTableCreateNode2(hashv, fklCreateString(len, buf));
    fklStrIdTableInsertNode(ht, hashv, node);
    node->v = ht->count;
    if ((++table->num) >= table->idl_size) {
        table->idl_size += FKL_DEFAULT_INC;
        table->idl = (FklStrIdTableElm **)fklRealloc(
            table->idl, table->idl_size * sizeof(FklStrIdTableElm *));
        FKL_ASSERT(table->idl);
    }
    table->idl[table->num - 1] = &node->elm;
    return &node->elm;
}

void fklUninitSymbolTable(FklSymbolTable *table) {
    fklStrIdTableUninit(&table->ht);
    free(table->idl);
}

void fklDestroySymbolTable(FklSymbolTable *table) {
    fklUninitSymbolTable(table);
    free(table);
}

FklStrIdTableElm *fklGetSymbolWithId(FklSid_t id, const FklSymbolTable *table) {
    if (id == 0)
        return NULL;
    return table->idl[id - 1];
}

#include <math.h>
void fklPrintSymbolTable(const FklSymbolTable *table, FILE *fp) {
    int numLen = table->num ? (int)(log10(table->num) + 1) : 1;
    fprintf(fp, "size:%" FKL_PRT64U "\n", table->num);
    for (uint32_t i = 0; i < table->num; i++) {
        FklStrIdTableElm *cur = table->idl[i];
        fprintf(fp, "%-*" FKL_PRT64U ":\t", numLen, cur->v);
        fklPrintRawSymbol(cur->k, fp);
        fputc('\n', fp);
    }
}

void fklWriteSymbolTable(const FklSymbolTable *table, FILE *fp) {
    fwrite(&table->num, sizeof(table->num), 1, fp);
    for (uint64_t i = 0; i < table->num; i++)
        fwrite(table->idl[i]->k,
               table->idl[i]->k->size + sizeof(table->idl[i]->k->size), 1, fp);
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

static inline void write_symbol_def(const FklSymDefTableMutElm *def, FILE *fp) {
    fwrite(&def->k.id, sizeof(def->k.id), 1, fp);
    fwrite(&def->k.scope, sizeof(def->k.scope), 1, fp);
    fwrite(&def->v.idx, sizeof(def->v.idx), 1, fp);
    fwrite(&def->v.cidx, sizeof(def->v.cidx), 1, fp);
    fwrite(&def->v.isLocal, sizeof(def->v.isLocal), 1, fp);
}

static inline void write_prototype(const FklFuncPrototype *pt, FILE *fp) {
    uint32_t count = pt->lcount;
    fwrite(&count, sizeof(count), 1, fp);
    count = pt->rcount;
    FklSymDefTableMutElm *defs = pt->refs;
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

static inline void load_symbol_def(FklSymDefTableMutElm *def, FILE *fp) {
    fread(&def->k.id, sizeof(def->k.id), 1, fp);
    fread(&def->k.scope, sizeof(def->k.scope), 1, fp);
    fread(&def->v.idx, sizeof(def->v.idx), 1, fp);
    fread(&def->v.cidx, sizeof(def->v.cidx), 1, fp);
    fread(&def->v.isLocal, sizeof(def->v.isLocal), 1, fp);
}

static inline void load_prototype(FklFuncPrototype *pt, FILE *fp) {
    uint32_t count = 0;

    fread(&count, sizeof(count), 1, fp);
    pt->lcount = count;
    fread(&count, sizeof(count), 1, fp);
    pt->rcount = count;
    FklSymDefTableMutElm *defs;
    if (count == 0)
        defs = NULL;
    else {
        defs =
            (FklSymDefTableMutElm *)calloc(count, sizeof(FklSymDefTableMutElm));
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

