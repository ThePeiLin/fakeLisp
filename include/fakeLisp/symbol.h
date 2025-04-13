#ifndef FKL_SYMBOL_H
#define FKL_SYMBOL_H

#include "base.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t FklSid_t;

// FklStrIdTable
#define FKL_TABLE_KEY_TYPE FklString *
#define FKL_TABLE_VAL_TYPE FklSid_t
#define FKL_TABLE_ELM_NAME StrId
#define FKL_TABLE_KEY_HASH return fklStringHash(*pk);
#define FKL_TABLE_KEY_EQUAL(A, B) fklStringEqual(*(A), *(B))
#define FKL_TABLE_KEY_UNINIT(K)                                                \
    {                                                                          \
        free(*(K));                                                            \
    }
#include "table.h"

typedef struct FklSymboTable {
    FklSid_t num;
    size_t idl_size;
    FklStrIdTableElm **idl;
    FklStrIdTable ht;
} FklSymbolTable;

void fklSetSidKey(void *k0, const void *k1);
uintptr_t fklSidHashFunc(const void *k);
int fklSidKeyEqual(const void *k0, const void *k1);

void fklInitSymbolTable(FklSymbolTable *st);
FklSymbolTable *fklCreateSymbolTable(void);

FklStrIdTableElm *fklAddSymbol(const FklString *, FklSymbolTable *);
FklStrIdTableElm *fklAddSymbolCstr(const char *, FklSymbolTable *);
FklStrIdTableElm *fklAddSymbolCharBuf(const char *, size_t, FklSymbolTable *);
FklStrIdTableElm *fklGetSymbolWithId(FklSid_t id, const FklSymbolTable *);

void fklPrintSymbolTable(const FklSymbolTable *, FILE *);

void fklDestroySymbolTable(FklSymbolTable *);
void fklUninitSymbolTable(FklSymbolTable *);

void fklWriteSymbolTable(const FklSymbolTable *, FILE *);
void fklLoadSymbolTable(FILE *, FklSymbolTable *table);

typedef struct {
    FklSid_t id;
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
    FklSid_t id;
    uint32_t scope;
    uint32_t idx;
    uint32_t prototypeId;
    uint32_t assign;
    FklSid_t fid;
    uint64_t line;
} FklUnReSymbolRef;

// FklUnReSymbolRefVector
#define FKL_VECTOR_ELM_TYPE FklUnReSymbolRef
#define FKL_VECTOR_ELM_TYPE_NAME UnReSymbolRef
#include "vector.h"

// FklSidVector
#define FKL_VECTOR_ELM_TYPE FklSid_t
#define FKL_VECTOR_ELM_TYPE_NAME Sid
#include "vector.h"

// FklSidTable
#define FKL_TABLE_KEY_TYPE FklSid_t
#define FKL_TABLE_ELM_NAME Sid
#include "table.h"

// FklSymDefTable
#define FKL_TABLE_KEY_TYPE FklSidScope
#define FKL_TABLE_VAL_TYPE FklSymDef
#define FKL_TABLE_ELM_NAME SymDef
#define FKL_TABLE_KEY_HASH                                                     \
    return fklHashCombine(fklHash32Shift((pk)->id), (pk)->scope);
#define FKL_TABLE_KEY_EQUAL(A, B) (A)->id == (B)->id && (A)->scope == (B)->scope
#include "table.h"

typedef struct FklFuncPrototype {
    FklSymDefTableMutElm *refs;
    uint32_t lcount;
    uint32_t rcount;
    FklSid_t sid;
    FklSid_t fid;
    uint64_t line;
} FklFuncPrototype;

typedef struct {
    FklFuncPrototype *pa;
    uint32_t count;
} FklFuncPrototypes;

void fklInitFuncPrototypes(FklFuncPrototypes *pts, uint32_t count);
void fklUninitFuncPrototypes(FklFuncPrototypes *pts);

FklFuncPrototypes *fklCreateFuncPrototypes(uint32_t count);
uint32_t fklInsertEmptyFuncPrototype(FklFuncPrototypes *pts);

void fklWriteFuncPrototypes(const FklFuncPrototypes *pts, FILE *fp);
FklFuncPrototypes *fklLoadFuncPrototypes(FILE *fp);

void fklUninitFuncPrototype(FklFuncPrototype *p);
void fklDestroyFuncPrototypes(FklFuncPrototypes *p);

#ifdef __cplusplus
}
#endif

#endif
