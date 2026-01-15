#ifndef FKL_SYMBOL_H
#define FKL_SYMBOL_H

#include "base.h"
#include "vm_fwd.h"

#include <assert.h>
#include <stdalign.h>
#include <stdint.h>

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

// unresolved symbol ref
typedef struct {
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

typedef struct {
    FklVMvalue *sid;
    FklVMvalue *cidx;
    FklVMvalue *is_local;
} FklVarRefDef;

#define FKL_VAR_REF_DEF_MEMBER_COUNT (3)

static_assert(alignof(FklVarRefDef) == alignof(FklVMvalue *), "What the f**k?");

static_assert((sizeof(FklVarRefDef) / sizeof(FklVMvalue *))
                      == FKL_VAR_REF_DEF_MEMBER_COUNT,
        "What the f**k?");

static_assert(sizeof(FklVarRefDef) < sizeof(FklSymDefHashMapMutElm),
        "What the f**k?");

FKL_VM_DEF_UD_STRUCT(FklVMvalueProto, {
    FklVMvalue *name;
    FklVMvalue *file;
    uint64_t line;

    uint32_t local_count;
    uint32_t total_val_count;

    uint32_t ref_count;
    uint32_t ref_offset;

    uint32_t konsts_count;
    uint32_t konsts_offset;

    uint32_t child_proto_count;
    uint32_t child_proto_offset;

    uint32_t used_libraries_count;
    uint32_t used_libraries_offset;
    FklVMvalue *vals[];
});

int fklIsVMvalueProto(const FklVMvalue *v);
static FKL_ALWAYS_INLINE FklVMvalueProto *fklVMvalueProto(const FklVMvalue *v) {
    FKL_ASSERT(fklIsVMvalueProto(v));
    return FKL_TYPE_CAST(FklVMvalueProto *, v);
}

FklVMvalueProto *fklCreateVMvalueProto(FklVM *exe, uint32_t val_count);

static FKL_ALWAYS_INLINE FklVarRefDef *fklVMvalueProtoVarRefs(
        const FklVMvalueProto *v) {
    FKL_ASSERT(fklIsVMvalueProto(FKL_TYPE_CAST(FklVMvalue *, v)));
    return (FklVarRefDef *)&v->vals[v->ref_offset];
}

static FKL_ALWAYS_INLINE FklVMvalueProto *const *fklVMvalueProtoChildren(
        const FklVMvalueProto *v) {
    FKL_ASSERT(fklIsVMvalueProto(FKL_TYPE_CAST(FklVMvalue *, v)));
    return (FklVMvalueProto *const *)&v->vals[v->child_proto_offset];
}

static FKL_ALWAYS_INLINE FklVMvalue *const *fklVMvalueProtoConsts(
        const FklVMvalueProto *v) {
    FKL_ASSERT(fklIsVMvalueProto(FKL_TYPE_CAST(FklVMvalue *, v)));
    return &v->vals[v->konsts_offset];
}

static FKL_ALWAYS_INLINE FklVMvalueLib *const *fklVMvalueProtoUsedLibs(
        const FklVMvalueProto *v) {
    FKL_ASSERT(fklIsVMvalueProto(FKL_TYPE_CAST(FklVMvalue *, v)));
    return (FklVMvalueLib *const *)&v->vals[v->used_libraries_offset];
}

#ifdef __cplusplus
}
#endif

#endif
