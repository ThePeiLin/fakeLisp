#ifndef FKL_PATTERN_H
#define FKL_PATTERN_H

#include "symbol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklNastNode FklNastNode;

typedef struct {
    FklSid_t id;
    FklNastNode *node;
} FklPatternMatchingHashTableItem;

#define FKL_PATTERN_NOT_EQUAL (0)
#define FKL_PATTERN_COVER (1)
#define FKL_PATTERN_BE_COVER (2)
#define FKL_PATTERN_EQUAL (3)

FklNastNode *fklCreatePatternFromNast(FklNastNode *, FklSidTable **);
int fklPatternMatch(const FklNastNode *pattern, const FklNastNode *exp,
                    FklHashTable *ht);
void fklPatternMatchingHashTableSet(FklSid_t sid, const FklNastNode *node,
                                    FklHashTable *ht);

int fklPatternCoverState(const FklNastNode *p0, const FklNastNode *p1);
FklNastNode *fklPatternMatchingHashTableRef(FklSid_t sid,
                                            const FklHashTable *ht);
FklHashTable *fklCreatePatternMatchingHashTable(void);
void fklInitPatternMatchHashTable(FklHashTable *ht);

#ifdef __cplusplus
}
#endif
#endif
