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
    r->pa = (FklFuncPrototype *)fklZcalloc((count + 1),
            sizeof(FklFuncPrototype));
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
