#ifndef FKL_STRING_TABLE_H
#define FKL_STRING_TABLE_H

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

// FklStrHashSet
#define FKL_HASH_KEY_TYPE FklString *
#define FKL_HASH_ELM_NAME Str
#define FKL_HASH_KEY_HASH return fklStringHash(*pk);
#define FKL_HASH_KEY_EQUAL(A, B) fklStringEqual(*(A), *(B))
#define FKL_HASH_KEY_UNINIT(K) fklZfree(*(K))
#include "cont/hash.h"

typedef FklStrHashSet FklStringTable;

FKL_API void fklInitStringTable(FklStringTable *st);
FKL_API FklStringTable *fklCreateStringTable(void);

FKL_API
const FklString *fklAddString(FklStringTable *s, const FklString *);
FKL_API
const FklString *fklAddStringCstr(FklStringTable *, const char *);
FKL_API
const FklString *fklAddStringCharBuf(FklStringTable *, const char *, size_t);

FKL_API void fklDestroyStringTable(FklStringTable *);
FKL_API void fklUninitStringTable(FklStringTable *);
FKL_API void fklClearStringTable(FklStringTable *);

FKL_API void fklWriteStringTable(const FklStringTable *, FILE *);
FKL_API void fklLoadStringTable(FILE *, FklStringTable *table);

#ifdef __cplusplus
}
#endif

#endif
