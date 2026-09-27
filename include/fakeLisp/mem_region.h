#ifndef FKL_MEM_REGION_H
#define FKL_MEM_REGION_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

FKL_API void *fklMemRegionReserve(size_t size);

/// current_commited + size
FKL_API
FKL_NODISCARD
int fklMemRegionGrow(void *, size_t size);

FKL_API
FKL_NODISCARD
int fklMemRegionGrowTo(void *, size_t size);

FKL_API
FKL_NODISCARD
int fklMemRegionShrinkTo(void *, size_t size);

FKL_API
FKL_NODISCARD
int fklMemRegionRelease(void *);

/// return total_size - header
FKL_API size_t fklMemRegionSize(void *);

/// return current_commited - header
FKL_API size_t fklMemRegionUsableSize(void *);

#ifdef __cplusplus
}
#endif

#endif
