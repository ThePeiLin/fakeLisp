#ifndef FKL_SRC_CODEGEN_H
#define FKL_SRC_CODEGEN_H

#include <fakeLisp/codegen.h>
#include <stdint.h>
struct CustomActionCtx {
    uint64_t refcount;
    FklByteCodelnt *bcl;
    FklFuncPrototypes *pts;
    FklCodegenLibVector *macroLibStack;
    FklCodegenOuterCtx *codegen_outer_ctx;
    FklSymbolTable *pst;
    uint32_t prototype_id;
};

static inline FklGrammerProdGroupItem *
add_production_group(FklGraProdGroupHashMap *named_prod_groups,
                     FklSymbolTable *st, FklSid_t group_id) {
    FklGrammerProdGroupItem *group =
        fklGraProdGroupHashMapAdd1(named_prod_groups, group_id);
    if (!group->g.st) {
        fklInitEmptyGrammer(&group->g, st);
        fklInitSymbolTable(&group->reachable_terminals);
        fklProdPrintingVectorInit(&group->prod_printing, 8);
        fklNastNodeVectorInit(&group->ignore_printing, 8);
    }
    return group;
}

#endif
