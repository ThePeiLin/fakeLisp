#include <fakeLisp/codegen.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/vm.h>

#include <string.h>

int fklPatternMatch(const FklNastNode *pattern,
        const FklNastNode *exp,
        FklPmatchHashMap *ht) {
    if (exp->type != FKL_NAST_PAIR)
        return 0;
    if (exp->pair->car->type != FKL_NAST_SYM
            || pattern->pair->car->sym != exp->pair->car->sym)
        return 0;
    FklNastImmPairVector s;
    fklNastImmPairVectorInit(&s, 8);
    fklNastImmPairVectorPushBack(&s,
            &(FklNastImmPair){ .car = pattern->pair->cdr,
                .cdr = exp->pair->cdr });
    while (!fklNastImmPairVectorIsEmpty(&s)) {
        const FklNastImmPair *top = fklNastImmPairVectorPopBackNonNull(&s);
        const FklNastNode *n0 = top->car;
        const FklNastNode *n1 = top->cdr;
        if (n0->type == FKL_NAST_SLOT) {
            if (ht != NULL)
                fklPmatchHashMapAdd2(ht,
                        n0->sym,
                        FKL_REMOVE_CONST(FklNastNode, n1));
        } else if (n0->type == FKL_NAST_PAIR && n1->type == FKL_NAST_PAIR) {
            fklNastImmPairVectorPushBack(&s,
                    &(FklNastImmPair){ .car = n0->pair->cdr,
                        .cdr = n1->pair->cdr });

            fklNastImmPairVectorPushBack(&s,
                    &(FklNastImmPair){ .car = n0->pair->car,
                        .cdr = n1->pair->car });
        } else if (!fklNastNodeEqual(n0, n1)) {
            fklNastImmPairVectorUninit(&s);
            return 0;
        }
    }
    fklNastImmPairVectorUninit(&s);
    return 1;
}

static inline int is_pattern_equal(const FklNastNode *pattern,
        const FklNastNode *exp) {
    if (exp->type != FKL_NAST_PAIR)
        return 0;
    if (exp->pair->car->type != FKL_NAST_SYM
            || pattern->pair->car->sym != exp->pair->car->sym)
        return 0;
    FklNastImmPairVector s;
    fklNastImmPairVectorInit(&s, 8);
    fklNastImmPairVectorPushBack(&s,
            &(FklNastImmPair){ .car = pattern->pair->cdr,
                .cdr = exp->pair->cdr });
    int r = 1;
    while (r && !fklNastImmPairVectorIsEmpty(&s)) {
        const FklNastImmPair *top = fklNastImmPairVectorPopBackNonNull(&s);
        const FklNastNode *n0 = top->car;
        const FklNastNode *n1 = top->cdr;
        if (n0->type != n1->type)
            r = 0;
        else if (n0->type == FKL_NAST_SLOT)
            continue;
        else if (n0->type == FKL_NAST_PAIR && n1->type == FKL_NAST_PAIR) {
            fklNastImmPairVectorPushBack(&s,
                    &(FklNastImmPair){ .car = n0->pair->cdr,
                        .cdr = n1->pair->cdr });

            fklNastImmPairVectorPushBack(&s,
                    &(FklNastImmPair){ .car = n0->pair->car,
                        .cdr = n1->pair->car });
        } else if (!fklNastNodeEqual(n0, n1))
            r = 0;
    }
    fklNastImmPairVectorUninit(&s);
    return r;
}

static inline int is_partly_covered(const FklNastNode *pattern,
        const FklNastNode *exp) {
    int r = 0;
    if (exp->type != FKL_NAST_PAIR)
        return r;
    if (exp->pair->car->type != FKL_NAST_SYM
            || pattern->pair->car->sym != exp->pair->car->sym)
        return r;
    FklNastImmPairVector s;
    fklNastImmPairVectorInit(&s, 8);
    fklNastImmPairVectorPushBack(&s,
            &(FklNastImmPair){ .car = pattern->pair->cdr,
                .cdr = exp->pair->cdr });
    while (!fklNastImmPairVectorIsEmpty(&s)) {
        const FklNastImmPair *top = fklNastImmPairVectorPopBackNonNull(&s);
        const FklNastNode *n0 = top->car;
        const FklNastNode *n1 = top->cdr;
        if (n0->type == FKL_NAST_SLOT) {
            r = 1;
            break;
        } else if (n0->type == FKL_NAST_PAIR && n1->type == FKL_NAST_PAIR) {
            fklNastImmPairVectorPushBack(&s,
                    &(FklNastImmPair){ .car = n0->pair->cdr,
                        .cdr = n1->pair->cdr });

            fklNastImmPairVectorPushBack(&s,
                    &(FklNastImmPair){ .car = n0->pair->car,
                        .cdr = n1->pair->car });
        } else if (!fklNastNodeEqual(n0, n1))
            break;
    }
    fklNastImmPairVectorUninit(&s);
    return r;
}

int fklPatternCoverState(const FklNastNode *p0, const FklNastNode *p1) {
    if (is_pattern_equal(p0, p1))
        return FKL_PATTERN_EQUAL;
    else if (is_partly_covered(p0, p1))
        return FKL_PATTERN_COVER;
    else if (is_partly_covered(p1, p0))
        return FKL_PATTERN_BE_COVER;
    else
        return FKL_PATTERN_NOT_EQUAL;
}

static inline int is_valid_pattern_nast(const FklNastNode *p) {
    if (p->type != FKL_NAST_PAIR)
        return 0;
    if (p->pair->car->type != FKL_NAST_SYM)
        return 0;
    return 1;
}

static inline int is_pattern_slot(FklSid_t s, const FklNastNode *p) {
    return p->type == FKL_NAST_PAIR && p->pair->cdr->type == FKL_NAST_PAIR
        && p->pair->cdr->pair->cdr->type == FKL_NAST_NIL
        && p->pair->car->type == FKL_NAST_SYM && p->pair->car->sym == s
        && p->pair->cdr->pair->car->type == FKL_NAST_SYM;
}

FklNastNode *fklCreatePatternFromNast(FklNastNode *node,
        FklSidHashSet **psymbolTable) {
    FklNastNode *r = NULL;
    if (node->type == FKL_NAST_PAIR && fklIsNastNodeList(node)
            && node->pair->car->type == FKL_NAST_SYM
            && node->pair->cdr->type == FKL_NAST_PAIR
            && node->pair->cdr->pair->cdr->type == FKL_NAST_NIL
            && is_valid_pattern_nast(node->pair->cdr->pair->car)) {
        FklSidHashSet *symbolTable = fklSidHashSetCreate();
        FklNastNode *exp = fklCopyNastNode(node->pair->cdr->pair->car);
        FklSid_t slotId = node->pair->car->sym;
        FklNastNode *rest = exp->pair->cdr;

        FklNastNodeVector stack;
        fklNastNodeVectorInit(&stack, 32);
        fklNastNodeVectorPushBack2(&stack, rest);
        while (!fklNastNodeVectorIsEmpty(&stack)) {
            FklNastNode *c = *fklNastNodeVectorPopBackNonNull(&stack);
            if (c->type == FKL_NAST_PAIR) {
                if (is_pattern_slot(slotId, c)) {
                    FklSid_t sym = c->pair->cdr->pair->car->sym;
                    if (fklSidHashSetPut2(symbolTable, sym)) {
                        fklSidHashSetDestroy(symbolTable);
                        fklNastNodeVectorUninit(&stack);
                        *psymbolTable = NULL;
                        fklDestroyNastNode(exp);
                        return NULL;
                    }
                    fklDestroyNastNode(c->pair->car);
                    fklDestroyNastNode(c->pair->cdr);
                    fklZfree(c->pair);
                    c->type = FKL_NAST_SLOT;
                    c->sym = sym;
                } else {
                    fklNastNodeVectorPushBack2(&stack, c->pair->cdr);
                    fklNastNodeVectorPushBack2(&stack, c->pair->car);
                }
            }
        }
        r = exp;
        fklNastNodeVectorUninit(&stack);
        if (psymbolTable)
            *psymbolTable = symbolTable;
        else
            fklSidHashSetDestroy(symbolTable);
    }
    return r;
}
