#ifndef FKL_PATTERN_H
#define FKL_PATTERN_H

#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

// FklPmatchHashMap
#define FKL_HASH_KEY_TYPE FklVMvalue *
#define FKL_HASH_VAL_TYPE FklVMvalue *
#define FKL_HASH_ELM_NAME Pmatch
#define FKL_HASH_KEY_HASH return fklVMvalueEqHashv(*pk);
#include "cont/hash.h"

#define FKL_PATTERN_NOT_EQUAL (0)
#define FKL_PATTERN_COVER (1)
#define FKL_PATTERN_BE_COVER (2)
#define FKL_PATTERN_EQUAL (3)

struct FklVMslot {
    FklVMvalue *v;
};

FKL_VM_DEF_UD_STRUCT(FklVMvalueSlot,
        { //
            union {
                struct FklVMslot _slot;
               alignas(struct FklVMslot) FklVMvalue *s;
            };
        });

#define FKL_VM_SLOT_SYM(V) (((FklVMvalueSlot *)(V))->s)

FklVMvalue *
fklCreatePatternFromNast(FklVM *exe, FklVMvalue *, FklVMvalueHashSet **);
int fklPatternMatch(const FklVMvalue *pattern,
        const FklVMvalue *exp,
        FklPmatchHashMap *ht);

int fklPatternCoverState(const FklVMvalue *p0, const FklVMvalue *p1);

FklVMvalue *fklCreateVMvalueSlot(FklVM *, FklVMvalue *s);
int fklIsVMvalueSlot(const FklVMvalue *s);

#ifdef __cplusplus
}
#endif
#endif
