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
    fklVMformat(exe, build, "#<slot %S>", NULL, 1, (FklVMvalue *[]){ s->s });
}

static void _slot_userdata_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    fklVMgcToGray(as_slot(ud)->s, gc);
}

static FklVMudMetaTable const SlotUserDataMetaTable = {
    .size = sizeof(FklVMvalueSlot),
    .princ = _slot_userdata_as_print,
    .prin1 = _slot_userdata_as_print,
    .atomic = _slot_userdata_atomic,
};

int fklIsVMvalueSlot(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &SlotUserDataMetaTable;
}

FklVMvalue *fklCreateVMvalueSlot(FklVM *vm, FklVMvalue *s, int need_expand) {
    FKL_ASSERT(FKL_IS_SYM(s));
    FklVMvalueSlot *r = (FklVMvalueSlot *)fklCreateVMvalueUd(vm,
            &SlotUserDataMetaTable,
            NULL);
    r->s = s;
    r->need_expand = need_expand;
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
            if (ht != NULL) {
                fklPmatchHashMapAdd2(ht,
                        FKL_VM_SLOT(pat)->s,
                        (FklPmatchRes){
                            .value = exp,
                            .container = top->cont,
                            .need_expand = FKL_VM_SLOT(pat)->need_expand,
                        });
            }
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
    FklPairVector s;
    fklPairVectorInit(&s, 8);
    fklPairVectorPushBack(&s,
            &(FklPair){
                .car = FKL_VM_CDR(pattern),
                .cdr = FKL_VM_CDR(exp),
            });
    int r = 1;
    while (r && !fklPairVectorIsEmpty(&s)) {
        const FklPair *top = fklPairVectorPopBackNonNull(&s);
        FklVMvalue *n0 = top->car;
        FklVMvalue *n1 = top->cdr;
        if (fklIsVMvalueSlot(n0) && fklIsVMvalueSlot(n1))
            continue;
        else if (FKL_IS_PAIR(n0) && FKL_IS_PAIR(n1)) {
            fklPairVectorPushBack(&s,
                    &(FklPair){
                        .car = FKL_VM_CDR(n0),
                        .cdr = FKL_VM_CDR(n1),
                    });

            fklPairVectorPushBack(&s,
                    &(FklPair){
                        .car = FKL_VM_CAR(n0),
                        .cdr = FKL_VM_CAR(n1),
                    });
        } else if (!fklVMvalueEqual(n0, n1))
            r = 0;
    }
    fklPairVectorUninit(&s);
    return r;
}

static inline int is_partly_covered(const FklVMvalue *pattern,
        const FklVMvalue *exp) {
    int r = 0;
    if (!FKL_IS_PAIR(exp))
        return r;
    if (!FKL_IS_SYM(FKL_VM_CAR(exp)) || FKL_VM_CAR(pattern) != FKL_VM_CAR(exp))
        return r;
    FklPairVector s;
    fklPairVectorInit(&s, 8);
    fklPairVectorPushBack(&s,
            &(FklPair){
                .car = FKL_VM_CDR(pattern),
                .cdr = FKL_VM_CDR(exp),
            });
    while (!fklPairVectorIsEmpty(&s)) {
        const FklPair *top = fklPairVectorPopBackNonNull(&s);
        FklVMvalue *n0 = top->car;
        FklVMvalue *n1 = top->cdr;
        if (fklIsVMvalueSlot(n0)) {
            r = 1;
            break;
        } else if (FKL_IS_PAIR(n0) && FKL_IS_PAIR(n1)) {
            fklPairVectorPushBack(&s,
                    &(FklPair){
                        .car = FKL_VM_CDR(n0),
                        .cdr = FKL_VM_CDR(n1),
                    });

            fklPairVectorPushBack(&s,
                    &(FklPair){
                        .car = FKL_VM_CAR(n0),
                        .cdr = FKL_VM_CAR(n1),
                    });
        } else if (!fklVMvalueEqual(n0, n1))
            break;
    }
    fklPairVectorUninit(&s);
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
    if (FKL_IS_PAIR(node)                                 //
            && fklIsList(node)                            //
            && FKL_IS_SYM(FKL_VM_CAR(node))               //
            && FKL_IS_PAIR(FKL_VM_CDR(node))              //
            && FKL_VM_CDR(FKL_VM_CDR(node)) == FKL_VM_NIL //
            && is_valid_pattern_nast(FKL_VM_CAR(FKL_VM_CDR(node)))) {

        FklVMvalue *r = NULL;
        FklValueHashSet *symbolTable = fklValueHashSetCreate();
        FklVMvalue *exp = fklCopyVMlistOrAtom(FKL_VM_CAR(FKL_VM_CDR(node)), vm);
        FklVMvalue *slotId = FKL_VM_CAR(node);

        FklSlotVector stack;
        fklSlotVectorInit(&stack, 32);
        fklSlotVectorPushBack2(&stack, &FKL_VM_CDR(exp));
        while (!fklSlotVectorIsEmpty(&stack)) {
            FklVMvalue **c = *fklSlotVectorPopBackNonNull(&stack);
            FklVMvalue *cur = *c;
            if (!FKL_IS_PAIR(cur))
                continue;

            if (FKL_VM_CAR(cur) != slotId) {
                fklSlotVectorPushBack2(&stack, &FKL_VM_CDR(cur));
                fklSlotVectorPushBack2(&stack, &FKL_VM_CAR(cur));
                continue;
            }

            FklVMvalue *rest = FKL_VM_CDR(cur);
            if (!FKL_IS_PAIR(rest) || FKL_VM_CDR(rest) != FKL_VM_NIL)
                goto error;

            FklVMvalue *sym = FKL_VM_CAR(rest);

            if (FKL_IS_SYM(sym)) {
                if (fklValueHashSetPut2(symbolTable, sym)) {
                    goto error;
                }
                *c = fklCreateVMvalueSlot(vm, sym, 0);
                continue;
            } else if (is_pattern_slot(slotId, sym)) {
                sym = FKL_VM_CAR(FKL_VM_CDR(sym));
                if (fklValueHashSetPut2(symbolTable, sym)) {
                    goto error;
                }
                *c = fklCreateVMvalueSlot(vm, sym, 1);
                continue;
            } else {
                goto error;
            }
        }
        r = exp;
        fklSlotVectorUninit(&stack);
        if (psymbolTable)
            *psymbolTable = symbolTable;
        else
            fklValueHashSetDestroy(symbolTable);
        return r;
    error:
        fklValueHashSetDestroy(symbolTable);
        fklSlotVectorUninit(&stack);
        *psymbolTable = NULL;
        return NULL;
    }
    return NULL;
}
