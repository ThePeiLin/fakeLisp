#include <fakeLisp/base.h>
#include <fakeLisp/common.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void init_as_empty_pt(FklFuncPrototype *pt) {
    pt->refs = NULL;
    pt->lcount = 0;
    pt->rcount = 0;
    pt->konsts = NULL;
    pt->konsts_count = 0;
}

void fklInitFuncPrototypes(FklFuncPrototypes *r, uint32_t count) {
    r->count = count;
    r->pa = (FklFuncPrototype *)fklZmalloc(
            (count + 1) * sizeof(FklFuncPrototype));
    FKL_ASSERT(r->pa);
    init_as_empty_pt(r->pa);
}

uint32_t fklInsertEmptyFuncPrototype(FklFuncPrototypes *pts) {
    pts->count++;
    FklFuncPrototype *pa = (FklFuncPrototype *)fklZrealloc(pts->pa,
            (pts->count + 1) * sizeof(FklFuncPrototype));
    FKL_ASSERT(pts);
    pts->pa = pa;
    FklFuncPrototype *cpt = &pa[pts->count];
    memset(cpt, 0, sizeof(FklFuncPrototype));
    return pts->count;
}

void fklUninitFuncPrototype(FklFuncPrototype *p) {
    p->rcount = 0;
    p->konsts_count = 0;
    fklZfree(p->refs);
    fklZfree(p->konsts);
}

void fklDestroyFuncPrototypes(FklFuncPrototypes *p) {
    if (p) {
        fklUninitFuncPrototypes(p);
        fklZfree(p);
    }
}

void fklUninitFuncPrototypes(FklFuncPrototypes *p) {
    FklFuncPrototype *pts = p->pa;
    uint32_t end = p->count + 1;
    for (uint32_t i = 1; i < end; i++)
        fklUninitFuncPrototype(&pts[i]);
    fklZfree(pts);
    p->pa = 0;
    p->count = 0;
}

static inline void write_symbol_def(const FklSymDefHashMapMutElm *def,
        FILE *fp) {
    FKL_TODO();
    // fwrite(&def->k.id, sizeof(def->k.id), 1, fp);
    // fwrite(&def->k.scope, sizeof(def->k.scope), 1, fp);
    // fwrite(&def->v.idx, sizeof(def->v.idx), 1, fp);
    // fwrite(&def->v.cidx, sizeof(def->v.cidx), 1, fp);
    // fwrite(&def->v.isLocal, sizeof(def->v.isLocal), 1, fp);
}

static inline void write_prototype(const FklFuncPrototype *pt, FILE *fp) {
    FKL_TODO();
    // uint32_t count = pt->lcount;
    // fwrite(&count, sizeof(count), 1, fp);
    // count = pt->rcount;
    // FklSymDefHashMapMutElm *defs = pt->refs;
    // fwrite(&count, sizeof(count), 1, fp);
    // for (uint32_t i = 0; i < count; i++)
    //     write_symbol_def(&defs[i], fp);
    // fwrite(&pt->sid, sizeof(pt->sid), 1, fp);
    // fwrite(&pt->fid, sizeof(pt->fid), 1, fp);
    // fwrite(&pt->line, sizeof(pt->line), 1, fp);
}

void fklWriteFuncPrototypes(const FklFuncPrototypes *pts, FILE *fp) {
    uint32_t count = pts->count;
    FklFuncPrototype *pta = pts->pa;
    fwrite(&count, sizeof(count), 1, fp);
    uint32_t end = count + 1;
    for (uint32_t i = 1; i < end; i++)
        write_prototype(&pta[i], fp);
}

static inline void load_symbol_def(FklSymDefHashMapMutElm *def, FILE *fp) {
    FKL_TODO();
    // fread(&def->k.id, sizeof(def->k.id), 1, fp);
    // fread(&def->k.scope, sizeof(def->k.scope), 1, fp);
    // fread(&def->v.idx, sizeof(def->v.idx), 1, fp);
    // fread(&def->v.cidx, sizeof(def->v.cidx), 1, fp);
    // fread(&def->v.isLocal, sizeof(def->v.isLocal), 1, fp);
}

static inline void load_prototype(FklFuncPrototype *pt, FILE *fp) {
    FKL_TODO();
    // uint32_t count = 0;

    // fread(&count, sizeof(count), 1, fp);
    // pt->lcount = count;
    // fread(&count, sizeof(count), 1, fp);
    // pt->rcount = count;
    // FklSymDefHashMapMutElm *defs;
    // if (count == 0)
    //     defs = NULL;
    // else {
    //     defs = (FklSymDefHashMapMutElm *)fklZcalloc(count,
    //             sizeof(FklSymDefHashMapMutElm));
    //     FKL_ASSERT(defs);
    //     for (uint32_t i = 0; i < count; i++)
    //         load_symbol_def(&defs[i], fp);
    // }

    // pt->refs = defs;
    // fread(&pt->sid, sizeof(pt->sid), 1, fp);
    // fread(&pt->fid, sizeof(pt->fid), 1, fp);
    // fread(&pt->line, sizeof(pt->line), 1, fp);
}

FklFuncPrototypes *fklLoadFuncPrototypes(FILE *fp) {
	FKL_TODO();
    // uint32_t count = 0;
    // fread(&count, sizeof(count), 1, fp);
    // FklFuncPrototypes *pts = fklCreateFuncPrototypes(count);
    // FklFuncPrototype *pta = pts->pa;
    // for (uint32_t i = 1; i <= count; i++)
    //     load_prototype(&pta[i], fp);
    // return pts;
}
