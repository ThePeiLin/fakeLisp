#ifndef FKL_SRC_CODEGEN_H
#define FKL_SRC_CODEGEN_H

#include <fakeLisp/codegen.h>
#include <fakeLisp/parser.h>
#include <stdint.h>

struct CustomActionCtx {
    uint64_t refcount;
    FklByteCodelnt *bcl;
    FklFuncPrototypes *pts;
    FklCodegenLibVector *macroLibStack;
    FklCodegenOuterCtx *codegen_outer_ctx;
    FklSymbolTable *pst;
    uint32_t prototype_id;
    uint8_t is_recomputed;
};

static inline FklGrammerProdGroupItem *add_production_group(
        FklGraProdGroupHashMap *named_prod_groups,
        FklSymbolTable *st,
        FklSid_t group_id) {
    FklGrammerProdGroupItem *group =
            fklGraProdGroupHashMapAdd1(named_prod_groups, group_id);
    if (!group->g.st) {
        fklInitEmptyGrammer(&group->g, st);
        fklInitSymbolTable(&group->reachable_terminals);
    }
    return group;
}

static inline void print_nast_node_with_null_chr(const FklNastNode *node,
        const FklSymbolTable *st,
        FILE *fp) {
    fklPrintNastNode(node, fp, st);
    fputc('\0', fp);
}

static inline FklNastNode *load_nast_node_with_null_chr(FklSymbolTable *st,
        FILE *fp) {
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    char ch = 0;
    while ((ch = fgetc(fp)))
        fklStringBufferPutc(&buf, ch);
    FklNastNode *node = fklCreateNastNodeFromCstr(buf.buf, st);
    fklUninitStringBuffer(&buf);
    return node;
}

static inline void replace_sid(FklSid_t *id,
        const FklSymbolTable *origin_st,
        FklSymbolTable *target_st) {
    FklSid_t sid = *id;
    *id = fklAddSymbol(fklGetSymbolWithId(sid, origin_st)->k, target_st)->v;
}

#endif
