#include <fakeLisp/base.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/vm.h>

#include <string.h>

// FklVMvalueSlotVector
#define FKL_VECTOR_ELM_TYPE FklVMvalue **
#define FKL_VECTOR_ELM_TYPE_NAME Slot
#include <fakeLisp/cont/vector.h>

static inline FklVMvalueSlot *as_slot(const FklVMvalue *v) {
    return FKL_TYPE_CAST(FklVMvalueSlot *, v);
}

static void _slot_userdata_as_print(const FklVMvalue *ud,
        FklCodeBuilder *build,
        FklVM *exe) {
    FklVMvalueSlot *s = as_slot(ud);
    // FKL_DECL_UD_DATA(s, struct FklVMslot, ud);
    fklVMformat(exe, build, "#<slot %S>", NULL, 1, (FklVMvalue *[]){ s->s });
}

static void _slot_userdata_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(s, struct FklVMslot, ud);
    // fklVMgcToGray(s->v, gc);
    fklVMgcToGray(as_slot(ud)->s, gc);
}

static FklVMudMetaTable const SlotUserDataMetaTable = {
    // .size = sizeof(struct FklVMslot),
    .size = sizeof(FklVMvalueSlot),
    .__as_princ = _slot_userdata_as_print,
    .__as_prin1 = _slot_userdata_as_print,
    .__atomic = _slot_userdata_atomic,
};

int fklIsVMvalueSlot(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &SlotUserDataMetaTable;
}

FklVMvalue *fklCreateVMvalueSlot(FklVM *vm, FklVMvalue *s) {
    FKL_ASSERT(FKL_IS_SYM(s));
    FklVMvalueSlot *r = (FklVMvalueSlot *)fklCreateVMvalueUd(vm,
            &SlotUserDataMetaTable,
            NULL);
    r->s = s;
    return (FklVMvalue *)r;
}

typedef struct {
    const FklVMvalue *pat;
    FklVMvalue *exp;
    const FklVMvalue *cont;
} PmatchPair;

#define FKL_VECTOR_TYPE_PREFIX Pa
#define FKL_VECTOR_METHOD_PREFIX pa
#define FKL_VECTOR_ELM_TYPE PmatchPair
#define FKL_VECTOR_ELM_TYPE_NAME PmatchPair
#define FKL_HASH_KEY_HASH return fklVMvalueEqHashv(*pk);
#include <fakeLisp/cont/vector.h>

int fklPatternMatch(const FklVMvalue *pattern,
        const FklVMvalue *exp,
        FklPmatchHashMap *ht) {
    if (!FKL_IS_PAIR(exp))
        return 0;
    if (!FKL_IS_SYM(FKL_VM_CAR(exp)) || FKL_VM_CAR(pattern) != FKL_VM_CAR(exp))
        return 0;
    PaPmatchPairVector s;
    paPmatchPairVectorInit(&s, 8);
    paPmatchPairVectorPushBack(&s,
            &(PmatchPair){
                .pat = FKL_VM_CDR(pattern),
                .exp = FKL_VM_CDR(exp),
                .cont = FKL_TYPE_CAST(FklVMvalue *, exp),
            });

    while (!paPmatchPairVectorIsEmpty(&s)) {
        const PmatchPair *top = paPmatchPairVectorPopBackNonNull(&s);
        const FklVMvalue *pat = top->pat;
        FklVMvalue *exp = top->exp;
        if (fklIsVMvalueSlot(pat)) {
            if (ht != NULL)
                fklPmatchHashMapAdd2(ht,
                        // n0->sym,
                        FKL_VM_SLOT_SYM(pat),
                        // FKL_TYPE_CAST(FklNastNode *, n1)
                        (FklPmatchRes){ .value = exp, .container = top->cont });
        } else if (FKL_IS_PAIR(pat) && FKL_IS_PAIR(exp)) {
            paPmatchPairVectorPushBack(&s,
                    &(PmatchPair){
                        .pat = FKL_VM_CDR(pat),
                        .exp = FKL_VM_CDR(exp),
                        .cont = exp,
                    });

            paPmatchPairVectorPushBack(&s,
                    &(PmatchPair){
                        .pat = FKL_VM_CAR(pat),
                        .exp = FKL_VM_CAR(exp),
                        .cont = exp,
                    });
        } else if (!fklVMvalueEqual(pat, exp)) {
            paPmatchPairVectorUninit(&s);
            return 0;
        }
    }
    paPmatchPairVectorUninit(&s);
    return 1;
}

static inline int is_pattern_equal(const FklVMvalue *pattern,
        const FklVMvalue *exp) {
    if (!FKL_IS_PAIR(exp))
        return 0;
    if (!FKL_IS_SYM(FKL_VM_CAR(exp)) || FKL_VM_CAR(pattern) != FKL_VM_CAR(exp))
        return 0;
    FklVMpairVector s;
    fklVMpairVectorInit(&s, 8);
    fklVMpairVectorPushBack(&s,
            &(FklVMpair){
                .car = FKL_VM_CDR(pattern),
                .cdr = FKL_VM_CDR(exp),
            });
    int r = 1;
    while (r && !fklVMpairVectorIsEmpty(&s)) {
        const FklVMpair *top = fklVMpairVectorPopBackNonNull(&s);
        FklVMvalue *n0 = top->car;
        FklVMvalue *n1 = top->cdr;
        if (fklIsVMvalueSlot(n0) && fklIsVMvalueSlot(n1))
            continue;
        else if (FKL_IS_PAIR(n0) && FKL_IS_PAIR(n1)) {
            fklVMpairVectorPushBack(&s,
                    &(FklVMpair){
                        .car = FKL_VM_CDR(n0),
                        .cdr = FKL_VM_CDR(n1),
                    });

            fklVMpairVectorPushBack(&s,
                    &(FklVMpair){
                        .car = FKL_VM_CAR(n0),
                        .cdr = FKL_VM_CAR(n1),
                    });
        } else if (!fklVMvalueEqual(n0, n1))
            r = 0;
    }
    fklVMpairVectorUninit(&s);
    return r;
}

static inline int is_partly_covered(const FklVMvalue *pattern,
        const FklVMvalue *exp) {
    int r = 0;
    if (!FKL_IS_PAIR(exp))
        return r;
    if (!FKL_IS_SYM(FKL_VM_CAR(exp)) || FKL_VM_CAR(pattern) != FKL_VM_CAR(exp))
        return r;
    FklVMpairVector s;
    fklVMpairVectorInit(&s, 8);
    fklVMpairVectorPushBack(&s,
            &(FklVMpair){
                .car = FKL_VM_CDR(pattern),
                .cdr = FKL_VM_CDR(exp),
            });
    while (!fklVMpairVectorIsEmpty(&s)) {
        const FklVMpair *top = fklVMpairVectorPopBackNonNull(&s);
        FklVMvalue *n0 = top->car;
        FklVMvalue *n1 = top->cdr;
        if (fklIsVMvalueSlot(n0)) {
            r = 1;
            break;
        } else if (FKL_IS_PAIR(n0) && FKL_IS_PAIR(n1)) {
            fklVMpairVectorPushBack(&s,
                    &(FklVMpair){
                        .car = FKL_VM_CDR(n0),
                        .cdr = FKL_VM_CDR(n1),
                    });

            fklVMpairVectorPushBack(&s,
                    &(FklVMpair){
                        .car = FKL_VM_CAR(n0),
                        .cdr = FKL_VM_CAR(n1),
                    });
        } else if (!fklVMvalueEqual(n0, n1))
            break;
    }
    fklVMpairVectorUninit(&s);
    return r;
}

int fklPatternCoverState(const FklVMvalue *p0, const FklVMvalue *p1) {
    if (is_pattern_equal(p0, p1))
        return FKL_PATTERN_EQUAL;
    else if (is_partly_covered(p0, p1))
        return FKL_PATTERN_COVER;
    else if (is_partly_covered(p1, p0))
        return FKL_PATTERN_BE_COVER;
    else
        return FKL_PATTERN_NOT_EQUAL;
}

static inline int is_valid_pattern_nast(const FklVMvalue *p) {
    if (!FKL_IS_PAIR(p))
        return 0;
    if (!FKL_IS_SYM(FKL_VM_CAR(p)))
        return 0;
    return 1;
}

static inline int is_pattern_slot(const FklVMvalue *s, const FklVMvalue *p) {
    return FKL_IS_PAIR(p)                          //
        && FKL_IS_PAIR(FKL_VM_CDR(p))              //
        && FKL_VM_CDR(FKL_VM_CDR(p)) == FKL_VM_NIL //
        && FKL_IS_SYM(FKL_VM_CAR(p))               //
        && FKL_VM_CAR(p) == s                      //
        && FKL_IS_SYM(FKL_VM_CAR(FKL_VM_CDR(p)));
}

FklVMvalue *fklCreatePatternFromNast(FklVM *vm,
        FklVMvalue *node,
        FklValueHashSet **psymbolTable) {
    FklVMvalue *r = NULL;
    if (FKL_IS_PAIR(node)                                 //
            && fklIsList(node)                            //
            && FKL_IS_SYM(FKL_VM_CAR(node))               //
            && FKL_IS_PAIR(FKL_VM_CDR(node))              //
            && FKL_VM_CDR(FKL_VM_CDR(node)) == FKL_VM_NIL //
            && is_valid_pattern_nast(FKL_VM_CAR(FKL_VM_CDR(node)))) {
        FklValueHashSet *symbolTable = fklValueHashSetCreate();
        // FklNastNode *exp = fklCopyNastNode(node->pair->cdr->pair->car);
        // FklSid_t slotId = node->pair->car->sym;
        // FklNastNode *rest = exp->pair->cdr;

        FklVMvalue *exp = fklCopyVMlistOrAtom(FKL_VM_CAR(FKL_VM_CDR(node)), vm);
        FklVMvalue *slotId = FKL_VM_CAR(node);

        FklSlotVector stack;
        fklSlotVectorInit(&stack, 32);
        fklSlotVectorPushBack2(&stack, &FKL_VM_CDR(exp));
        while (!fklSlotVectorIsEmpty(&stack)) {
            FklVMvalue **c = *fklSlotVectorPopBackNonNull(&stack);
            FklVMvalue *cur = *c;
            if (FKL_IS_PAIR(cur)) {
                if (is_pattern_slot(slotId, cur)) {
                    FklVMvalue *sym = FKL_VM_CAR(FKL_VM_CDR(cur));
                    if (fklValueHashSetPut2(symbolTable, sym)) {
                        fklValueHashSetDestroy(symbolTable);
                        fklSlotVectorUninit(&stack);
                        *psymbolTable = NULL;
                        // fklDestroyNastNode(exp);
                        return NULL;
                    }
                    // fklDestroyNastNode(c->pair->car);
                    // fklDestroyNastNode(c->pair->cdr);
                    // fklZfree(c->pair);
                    *c = fklCreateVMvalueSlot(vm, sym);
                    // c->type = FKL_NAST_SLOT;
                    // c->sym = sym;
                } else {
                    fklSlotVectorPushBack2(&stack, &FKL_VM_CDR(cur));
                    fklSlotVectorPushBack2(&stack, &FKL_VM_CAR(cur));
                }
            }
        }
        r = exp;
        fklSlotVectorUninit(&stack);
        if (psymbolTable)
            *psymbolTable = symbolTable;
        else
            fklValueHashSetDestroy(symbolTable);
    }
    return r;
}
