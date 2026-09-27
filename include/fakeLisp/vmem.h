#ifndef FKL_VMEM_H
#define FKL_VMEM_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FKL_VMEM_NONE = 0,
    FKL_VMEM_R = 1 << 0,
    FKL_VMEM_W = 1 << 1,
    FKL_VMEM_X = 1 << 2,

    FKL_VMEM_RW = (FKL_VMEM_R | FKL_VMEM_W),
    FKL_VMEM_RX = (FKL_VMEM_R | FKL_VMEM_X),

    FKL_VMEM_WX = (FKL_VMEM_W | FKL_VMEM_X),

    FKL_VMEM_RWX = (FKL_VMEM_R | FKL_VMEM_W | FKL_VMEM_X),
} FklVmemProt;

FKL_API size_t fklVmemPageSize(void);
FKL_API size_t fklVmemGranularity(void);

FKL_API void *fklVmemReserve(size_t size);

/// @return 0 if successful
FKL_API
FKL_NODISCARD
int fklVmemCommit(void *p, size_t size);

/// @return 0 if successful
FKL_API
FKL_NODISCARD
int fklVmemDecommit(void *p, size_t size);

/// @return 0 if successful
FKL_API
FKL_NODISCARD
int fklVmemProtect(void *p, size_t size, FklVmemProt prot);

/// @return 0 if successful
FKL_API
FKL_NODISCARD
int fklVmemRelease(void *p, size_t size);

FKL_API void *fklVmemAlloc(size_t size, FklVmemProt prot);

static FKL_ALWAYS_INLINE size_t fklVmemRoundUp(size_t s, size_t page_size) {
    FKL_ASSERT(page_size > 0);
    if (s > (SIZE_MAX - (page_size - 1)))
        return 0;

    if ((page_size & (page_size - 1)) == 0) {
        size_t mask = page_size - 1;
        return (s + mask) & ~mask;
    }

    return ((s + (page_size - 1)) / page_size) * page_size;
}

static FKL_ALWAYS_INLINE size_t fklVmemReserveSize(size_t size) {
    return fklVmemRoundUp(size, fklVmemGranularity());
}

#ifdef __cplusplus
}
#endif

#endif
