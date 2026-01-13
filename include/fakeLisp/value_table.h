#ifndef FKL_VALUE_TABLE_H
#define FKL_VALUE_TABLE_H

#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

// for type-safe
typedef struct {
    FklValueTable vt;
} FklProtoTable;

static FKL_ALWAYS_INLINE void fklInitProtoTable(FklProtoTable *t) {
    fklInitValueTable(&t->vt);
}

static FKL_ALWAYS_INLINE void fklUninitProtoTable(FklProtoTable *t) {
    fklUninitValueTable(&t->vt);
}

static FKL_ALWAYS_INLINE FklValueId fklProtoTableAdd(FklProtoTable *t,
        const FklVMvalueProto *v) {
    return fklValueTableAdd(&t->vt, FKL_VM_VAL(v));
}

static FKL_ALWAYS_INLINE FklValueId fklProtoTableGet(const FklProtoTable *t,
        const FklVMvalueProto *v) {
    return fklValueTableGet(&t->vt, FKL_VM_VAL(v));
}

static FKL_ALWAYS_INLINE FklValueIdHashMapNode *fklProtoTableFirst(
        const FklProtoTable *t) {
    return t->vt.ht.first;
}

static FKL_ALWAYS_INLINE FklValueIdHashMapNode *fklProtoTableLast(
        const FklProtoTable *t) {
    return t->vt.ht.last;
}

// for type-safe
typedef struct {
    FklValueTable vt;
} FklLibTable;

static FKL_ALWAYS_INLINE void fklInitLibTable(FklLibTable *t) {
    fklInitValueTable(&t->vt);
}

static FKL_ALWAYS_INLINE void fklUninitLibTable(FklLibTable *t) {
    fklUninitValueTable(&t->vt);
}

static FKL_ALWAYS_INLINE FklValueId fklLibTableAdd(FklLibTable *t,
        const FklVMvalueLib *v) {
    return fklValueTableAdd(&t->vt, FKL_VM_VAL(v));
}

static FKL_ALWAYS_INLINE FklValueId fklLibTableGet(const FklLibTable *t,
        const FklVMvalueLib *v) {
    return fklValueTableGet(&t->vt, FKL_VM_VAL(v));
}

static FKL_ALWAYS_INLINE FklValueIdHashMapNode *fklLibTableFirst(
        const FklLibTable *t) {
    return t->vt.ht.first;
}

static FKL_ALWAYS_INLINE FklValueIdHashMapNode *fklLibTableLast(
        const FklLibTable *t) {
    return t->vt.ht.last;
}

#ifdef __cplusplus
}
#endif

#endif
