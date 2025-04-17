#ifndef FKL_PATTERN_H
#define FKL_PATTERN_H

#include "symbol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FklNastNode FklNastNode;

#define FKL_TABLE_KEY_TYPE FklSid_t
#define FKL_TABLE_VAL_TYPE FklNastNode *
#define FKL_TABLE_ELM_NAME Pmatch
#include "table.h"

#define FKL_PATTERN_NOT_EQUAL (0)
#define FKL_PATTERN_COVER (1)
#define FKL_PATTERN_BE_COVER (2)
#define FKL_PATTERN_EQUAL (3)

FklNastNode *fklCreatePatternFromNast(FklNastNode *, FklSidHashSet **);
int fklPatternMatch(const FklNastNode *pattern, const FklNastNode *exp,
                    FklPmatchTable *ht);

int fklPatternCoverState(const FklNastNode *p0, const FklNastNode *p1);

#ifdef __cplusplus
}
#endif
#endif
