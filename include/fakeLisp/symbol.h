#ifndef FKL_SYMBOL_H
#define FKL_SYMBOL_H

#include "base.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    struct FklVMvalue *id;
    uint32_t scope;
} FklSidScope;

#define FKL_VAR_REF_INVALID_CIDX (UINT32_MAX)

typedef struct FklSymDef {
    uint32_t idx;
    uint32_t cidx;
    uint8_t isLocal;
    uint8_t isConst;
} FklSymDef;

typedef struct // unresolved symbol ref
{
    struct FklVMvalue *id;
    struct FklVMvalue *fid;
    uint32_t scope;
    uint32_t idx;
    uint32_t prototypeId;
    uint32_t assign;
    uint64_t line;
} FklUnReSymbolRef;

// FklUnReSymbolRefVector
#define FKL_VECTOR_ELM_TYPE FklUnReSymbolRef
#define FKL_VECTOR_ELM_TYPE_NAME UnReSymbolRef
#include "cont/vector.h"

static inline uintptr_t fklSymDefHashv(const FklSidScope *k) {
    return fklHashCombine(fklHash64Shift(FKL_TYPE_CAST(uintptr_t, k->id)),
            k->scope);
}

// FklSymDefHashMap
#define FKL_HASH_KEY_TYPE FklSidScope
#define FKL_HASH_VAL_TYPE FklSymDef
#define FKL_HASH_ELM_NAME SymDef
#define FKL_HASH_KEY_HASH return fklSymDefHashv(pk);
#define FKL_HASH_KEY_EQUAL(A, B) (A)->id == (B)->id && (A)->scope == (B)->scope
#include "cont/hash.h"

typedef struct FklFuncPrototype {
    FklSymDefHashMapMutElm *refs;
    uint32_t rcount;
    uint32_t lcount;
    struct FklVMvalue *name;
    struct FklVMvalue *file;
    struct FklVMvalue **konsts;
	uint32_t konsts_count;
    uint64_t line;
} FklFuncPrototype;

typedef struct {
    FklFuncPrototype *pa;
    uint32_t count;
} FklFuncPrototypes;

void fklInitFuncPrototypes(FklFuncPrototypes *pts, uint32_t count);
void fklUninitFuncPrototypes(FklFuncPrototypes *pts);

uint32_t fklInsertEmptyFuncPrototype(FklFuncPrototypes *pts);

void fklWriteFuncPrototypes(const FklFuncPrototypes *pts, FILE *fp);
FklFuncPrototypes *fklLoadFuncPrototypes(FILE *fp);

void fklUninitFuncPrototype(FklFuncPrototype *p);
void fklDestroyFuncPrototypes(FklFuncPrototypes *p);

#ifdef __cplusplus
}
#endif

#endif
