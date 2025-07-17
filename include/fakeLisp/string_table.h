#ifndef FKL_STRING_TABLE_H
#define FKL_STRING_TABLE_H

#include "base.h"
#include "zmalloc.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void fklStrKeyFree(FklString *s) { fklZfree(s); }

// FklStrHashSet
#define FKL_HASH_KEY_TYPE FklString *
#define FKL_HASH_ELM_NAME Str
#define FKL_HASH_KEY_HASH return fklStringHash(*pk);
#define FKL_HASH_KEY_EQUAL(A, B) fklStringEqual(*(A), *(B))
#define FKL_HASH_KEY_UNINIT(K)                                                 \
    {                                                                          \
        fklStrKeyFree(*(K));                                                   \
    }
#include "cont/hash.h"

typedef FklStrHashSet FklStringTable;

void fklInitStringTable(FklStringTable *st);
FklStringTable *fklCreateStringTable(void);

const FklString *fklAddString(FklStringTable *s, const FklString *);
const FklString *fklAddStringCstr(FklStringTable *, const char *);
const FklString *fklAddStringCharBuf(FklStringTable *, const char *, size_t);

void fklDestroyStringTable(FklStringTable *);
void fklUninitStringTable(FklStringTable *);
void fklClearStringTable(FklStringTable *);

void fklWriteStringTable(const FklStringTable *, FILE *);
void fklLoadStringTable(FILE *, FklStringTable *table);

#ifdef __cplusplus
}
#endif

#endif
