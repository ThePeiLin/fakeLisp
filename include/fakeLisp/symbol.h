#ifndef FKL_SYMBOL_H
#define FKL_SYMBOL_H

#include "base.h"
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t FklSid_t;

typedef struct {
    FklString *symbol;
    FklSid_t id;
} FklSymbolHashItem;

typedef struct FklSymboTable {
    FklSid_t num;
    size_t idl_size;
    FklSymbolHashItem **idl;
    FklHashTable ht;
} FklSymbolTable;

void fklSetSidKey(void *k0, const void *k1);
uintptr_t fklSidHashFunc(const void *k);
int fklSidKeyEqual(const void *k0, const void *k1);

void fklInitSymbolTable(FklSymbolTable *st);
FklSymbolTable *fklCreateSymbolTable(void);

FklSymbolHashItem *fklAddSymbol(const FklString *, FklSymbolTable *);
FklSymbolHashItem *fklAddSymbolCstr(const char *, FklSymbolTable *);
FklSymbolHashItem *fklAddSymbolCharBuf(const char *, size_t, FklSymbolTable *);
FklSymbolHashItem *fklGetSymbolWithId(FklSid_t id, const FklSymbolTable *);

void fklPrintSymbolTable(const FklSymbolTable *, FILE *);

void fklDestroySymbolTable(FklSymbolTable *);
void fklUninitSymbolTable(FklSymbolTable *);

void fklWriteSymbolTable(const FklSymbolTable *, FILE *);
void fklLoadSymbolTable(FILE *, FklSymbolTable *table);

typedef struct FklFuncPrototype {
    struct FklSymbolDef *refs;
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

typedef struct {
    FklSid_t id;
    uint32_t scope;
} FklSidScope;

#define FKL_VAR_REF_INVALID_CIDX (UINT32_MAX)

typedef struct FklSymbolDef {
    FklSidScope k;
    uint32_t idx;
    uint32_t cidx;
    uint8_t isLocal;
    uint8_t isConst;
} FklSymbolDef;

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

void fklInitFuncPrototypes(FklFuncPrototypes *pts, uint32_t count);
void fklUninitFuncPrototypes(FklFuncPrototypes *pts);

FklFuncPrototypes *fklCreateFuncPrototypes(uint32_t count);
uint32_t fklInsertEmptyFuncPrototype(FklFuncPrototypes *pts);

void fklWriteFuncPrototypes(const FklFuncPrototypes *pts, FILE *fp);
FklFuncPrototypes *fklLoadFuncPrototypes(FILE *fp);

void fklUninitFuncPrototype(FklFuncPrototype *p);
void fklDestroyFuncPrototypes(FklFuncPrototypes *p);

FklHashTable *fklCreateSidSet(void);
void fklInitSidSet(FklHashTable *t);
void fklPutSidToSidSet(FklHashTable *t, FklSid_t);
#ifdef __cplusplus
}
#endif

#endif
