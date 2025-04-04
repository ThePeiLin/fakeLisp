#ifndef FKL_BASE_H
#define FKL_BASE_H

#include "bigint.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FKL_FIX_INT_MAX (1152921504606846975)
#define FKL_FIX_INT_MIN (-1152921504606846975 - 1)

typedef struct FklString {
    uint64_t size;
    char str[1];
} FklString;

FklString *fklCreateString(size_t, const char *);
FklString *fklStringRealloc(FklString *s, size_t new_size);
FklString *fklCopyString(const FklString *);
FklString *fklCreateEmptyString(void);
FklString *fklCreateStringFromCstr(const char *);
size_t fklCountCharInString(FklString *s, char c);
void fklStringCat(FklString **, const FklString *);
void fklStringCstrCat(FklString **, const char *);
void fklStringCharBufCat(FklString **, const char *buf, size_t s);
char *fklCstrStringCat(char *, const FklString *);
int fklStringCmp(const FklString *, const FklString *);
int fklStringEqual(const FklString *fir, const FklString *sec);

int fklStringCharBufCmp(const FklString *, size_t len, const char *buf);
int fklStringCstrCmp(const FklString *, const char *);
size_t fklStringCharBufMatch(const FklString *, const char *, size_t len);

size_t fklCharBufMatch(const char *s0, size_t l0, const char *s1, size_t l1);

char *fklStringToCstr(const FklString *str);
void fklPrintRawString(const FklString *str, FILE *fp);
void fklPrintString(const FklString *str, FILE *fp);
void fklPrintRawSymbol(const FklString *str, FILE *fp);
FklString *fklStringAppend(const FklString *, const FklString *);
void fklWriteStringToCstr(char *, const FklString *);

uintptr_t fklStringHash(const FklString *s);

uintptr_t fklCharBufHash(const char *str, size_t len);

struct FklStringBuffer;

void fklPrintRawCharBufToStringBuffer(struct FklStringBuffer *s, size_t len,
                                      const char *fstr, const char *begin_str,
                                      const char *end_str, char se);
static inline void fklPrintRawCstrToStringBuffer(struct FklStringBuffer *s,
                                                 const char *str,
                                                 const char *begin_str,
                                                 const char *end_str, char se) {
    fklPrintRawCharBufToStringBuffer(s, strlen(str), str, begin_str, end_str,
                                     se);
}
static inline void fklPrintRawStringToStringBuffer(struct FklStringBuffer *s,
                                                   const FklString *fstr,
                                                   const char *begin_str,
                                                   const char *end_str,
                                                   char se) {
    fklPrintRawCharBufToStringBuffer(s, fstr->size, fstr->str, begin_str,
                                     end_str, se);
}

FklString *fklStringToRawString(const FklString *str);
FklString *fklStringToRawSymbol(const FklString *str);

typedef struct FklStringBuffer {
    uint32_t size;
    uint32_t index;
    char *buf;
} FklStringBuffer;

#define FKL_STRING_BUFFER_INIT {0, 0, NULL}

static inline uint32_t fklStringBufferLen(FklStringBuffer *b) {
    return b->index;
}

static inline char *fklStringBufferBody(FklStringBuffer *b) { return b->buf; }

FklStringBuffer *fklCreateStringBuffer(void);
void fklInitStringBuffer(FklStringBuffer *);
void fklInitStringBufferWithCapacity(FklStringBuffer *, size_t s);
void fklStringBufferReverse(FklStringBuffer *, size_t s);
void fklStringBufferShrinkTo(FklStringBuffer *, size_t s);
void fklStringBufferResize(FklStringBuffer *, size_t s, char content);
void fklUninitStringBuffer(FklStringBuffer *);
void fklDestroyStringBuffer(FklStringBuffer *);
void fklStringBufferClear(FklStringBuffer *);
void fklStringBufferFill(FklStringBuffer *, char);
void fklStringBufferBincpy(FklStringBuffer *, const void *, size_t);
void fklStringBufferConcatWithCstr(FklStringBuffer *, const char *);
void fklStringBufferConcatWithString(FklStringBuffer *, const FklString *);
void fklStringBufferConcatWithStringBuffer(FklStringBuffer *,
                                           const FklStringBuffer *);
void fklStringBufferPutc(FklStringBuffer *, char);
void fklStringBufferPrintf(FklStringBuffer *, const char *fmt, ...);
FklString *fklStringBufferToString(FklStringBuffer *);
int fklIsSpecialCharAndPrintToStringBuffer(FklStringBuffer *s, char ch);
int fklStringBufferCmp(const FklStringBuffer *a, const FklStringBuffer *b);

typedef struct FklBytevector {
    uint64_t size;
    uint8_t ptr[];
} FklBytevector;
FklBytevector *fklStringBufferToBytevector(FklStringBuffer *);

FklBytevector *fklCreateBytevector(size_t, const uint8_t *);
FklBytevector *fklBytevectorRealloc(FklBytevector *b, size_t new_size);
FklBytevector *fklCopyBytevector(const FklBytevector *);
void fklBytevectorCat(FklBytevector **, const FklBytevector *);
int fklBytevectorCmp(const FklBytevector *, const FklBytevector *);
int fklBytevectorEqual(const FklBytevector *fir, const FklBytevector *sec);
void fklPrintRawBytevector(const FklBytevector *str, FILE *fp);
FklString *fklBytevectorToString(const FklBytevector *);
void fklPrintBytevectorToStringBuffer(FklStringBuffer *,
                                      const FklBytevector *bvec);

uintptr_t fklBytevectorHash(const FklBytevector *bv);

typedef struct FklHashTableItem {
    struct FklHashTableItem *prev;
    struct FklHashTableItem *next;
    struct FklHashTableItem *ni;
    uint8_t data[];
} FklHashTableItem;

typedef struct {
    uint8_t size;
    void (*__setKey)(void *, const void *);
    void (*__setVal)(void *, const void *);
    uintptr_t (*__hashFunc)(const void *);
    void (*__uninitItem)(void *);
    int (*__keyEqual)(const void *, const void *);
    void *(*__getKey)(void *);
} FklHashTableMetaTable;

typedef struct FklHashTable {
    const FklHashTableMetaTable *t;
    FklHashTableItem **base;
    FklHashTableItem *first;
    FklHashTableItem *last;
    uint32_t num;
    uint32_t size;
    uint32_t mask;
} FklHashTable;

#define FKL_HASH_GET_KEY(name, type, field)                                    \
    void *name(void *item) { return &((type *)item)->field; }

#define FKL_HASH_SET_KEY_VAL(name, type)                                       \
    void name(void *a, const void *b) { *(type *)a = *(type const *)b; }

#define FKL_DEFAULT_HASH_LOAD_FACTOR (2.0)

void fklInitHashTable(FklHashTable *, const FklHashTableMetaTable *);
FklHashTable *fklCreateHashTable(const FklHashTableMetaTable *);

void fklPtrKeySet(void *k0, const void *k1);
uintptr_t fklPtrKeyHashFunc(const void *k);
int fklPtrKeyEqual(const void *k0, const void *k1);

void fklInitPtrSet(FklHashTable *);
FklHashTable *fklCreatPtrSet(void);

void *fklGetOrPutWithOtherKey(void *pkey, uintptr_t (*hashv)(const void *key),
                              int (*keq)(const void *k0, const void *k1),
                              void (*setVal)(void *d0, const void *d1),
                              FklHashTable *ht);

void *fklGetHashItem(const void *key, const FklHashTable *);
FklHashTableItem **fklGetHashItemSlot(FklHashTable *, uintptr_t);

void *fklPutHashItem(const void *key, FklHashTable *);
int fklPutHashItem2(FklHashTable *, const void *key, void **pitem);
void *fklPutHashItemInSlot(FklHashTable *, FklHashTableItem **);

void *fklGetOrPutHashItem(void *data, FklHashTable *);
int fklDelHashItem(void *key, FklHashTable *, void *deleted);
void fklRemoveHashItem(FklHashTable *ht, FklHashTableItem *item);
void fklRemoveHashItemWithSlot(FklHashTable *ht, FklHashTableItem **slot);

void fklRehashTable(FklHashTable *table);
void fklClearHashTable(FklHashTable *ht);
void fklUninitHashTable(FklHashTable *);
void fklDestroyHashTable(FklHashTable *);

static inline void fklDoNothingUninitHashItem(void *v) {}
void *fklHashDefaultGetKey(void *i);

typedef struct FklQueueNode {
    void *data;
    struct FklQueueNode *next;
} FklQueueNode;

typedef struct {
    FklQueueNode *head;
    FklQueueNode **tail;
} FklPtrQueue;

FklQueueNode *fklCreateQueueNode(void *);
void fklDestroyQueueNode(FklQueueNode *);

void fklInitPtrQueue(FklPtrQueue *q);
void fklUninitPtrQueue(FklPtrQueue *q);
FklPtrQueue *fklCreatePtrQueue(void);
void fklDestroyPtrQueue(FklPtrQueue *);
int fklIsPtrQueueEmpty(FklPtrQueue *);
uint64_t fklLengthPtrQueue(FklPtrQueue *);
void *fklFirstPtrQueue(FklPtrQueue *);
void *fklPopPtrQueue(FklPtrQueue *);

FklQueueNode *fklPopPtrQueueNode(FklPtrQueue *);

void fklPushPtrQueue(void *, FklPtrQueue *);
void fklPushPtrQueueNode(FklPtrQueue *, FklQueueNode *);
void fklPushPtrQueueToFront(void *, FklPtrQueue *);
void fklPushPtrQueueNodeToFront(FklPtrQueue *q, FklQueueNode *n);
FklPtrQueue *fklCopyPtrQueue(FklPtrQueue *);

// FklUintVector
#define FKL_VECTOR_ELM_TYPE uintmax_t
#define FKL_VECTOR_ELM_TYPE_NAME Uint
#include "vector.h"

// FklstringVector
#define FKL_VECTOR_ELM_TYPE FklString *
#define FKL_VECTOR_ELM_TYPE_NAME String
#include "vector.h"

// FklQueueVector
#define FKL_VECTOR_ELM_TYPE FklPtrQueue *
#define FKL_VECTOR_ELM_TYPE_NAME Queue
#include "vector.h"

size_t fklBigIntToStringBuffer(const FklBigInt *a,
                               FklStringBuffer *string_buffer, uint8_t radix,
                               FklBigIntFmtFlags flags);

FklString *fklBigIntToString(const FklBigInt *a, uint8_t radix,
                             FklBigIntFmtFlags flags);

int fklIsBigIntGtLtFix(const FklBigInt *a);
int fklIsBigIntAdd1InFixIntRange(const FklBigInt *a);
int fklIsBigIntSub1InFixIntRange(const FklBigInt *a);
#ifdef __cplusplus
}
#endif
#endif
