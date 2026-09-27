#include <fakeLisp/vmem.h>

#include <sys/mman.h>
#include <unistd.h>

#define DEFAULT_PAGE_SIZE (4096)

static inline int vmem_check_args(void *p, size_t size) {
    if (p == NULL || size == 0)
        return -1;
    if ((uintptr_t)p % fklVmemPageSize() != 0)
        return -1;
    return 0;
}

static inline int vmem_map_prot(FklVmemProt in, int *out) {
    switch (in) {
    case FKL_VMEM_NONE:
        *out = PROT_NONE;
        break;

    case FKL_VMEM_R:
        *out = PROT_READ;
        break;

    case FKL_VMEM_W:
        *out = PROT_WRITE;
        break;

    case FKL_VMEM_X:
        *out = PROT_EXEC;
        break;

    case FKL_VMEM_RW:
        *out = PROT_READ | PROT_WRITE;
        break;

    case FKL_VMEM_RX:
        *out = PROT_READ | PROT_EXEC;
        break;

    case FKL_VMEM_WX:
        *out = PROT_WRITE | PROT_EXEC;
        break;

    case FKL_VMEM_RWX:
        *out = PROT_READ | PROT_WRITE | PROT_EXEC;
        break;
    default:
        return -1;
    }

    return 0;
}

size_t fklVmemPageSize(void) {
    long page_size = sysconf(_SC_PAGE_SIZE);
    return page_size > 0 ? (size_t)page_size : DEFAULT_PAGE_SIZE;
}

size_t fklVmemGranularity(void) { return fklVmemPageSize(); }

void *fklVmemReserve(size_t size) {
    if (size == 0)
        return NULL;
    size_t actual_size = fklVmemReserveSize(size);
    if (actual_size == 0)
        return NULL;
    void *p = mmap(NULL,
            actual_size,
            PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
            -1,
            0);

    return p == MAP_FAILED ? NULL : p;
}

int fklVmemCommit(void *p, size_t size) {
    if (vmem_check_args(p, size) != 0)
        return -1;
    size_t sz = fklVmemRoundUp(size, fklVmemPageSize());
    if (sz == 0)
        return -1;
    return mprotect(p, sz, PROT_READ | PROT_WRITE);
}

int fklVmemDecommit(void *p, size_t size) {
    if (vmem_check_args(p, size) != 0)
        return -1;
    size_t sz = fklVmemRoundUp(size, fklVmemPageSize());
    if (sz == 0)
        return -1;
    if (madvise(p, sz, MADV_DONTNEED) != 0)
        return -1;
    return mprotect(p, sz, PROT_NONE);
}

int fklVmemProtect(void *p, size_t size, FklVmemProt fkl_prot) {
    int prot = 0;
    if (vmem_map_prot(fkl_prot, &prot) != 0)
        return -1;
    return mprotect(p, size, prot);
}

int fklVmemRelease(void *p, size_t size) {
    if (p == NULL)
        return 0;
    size_t actual_size = fklVmemReserveSize(size);
    if (actual_size == 0)
        actual_size = fklVmemGranularity();
    return munmap(p, actual_size);
}

void *fklVmemAlloc(size_t size, FklVmemProt prot) {
    void *p = fklVmemReserve(size);
    if (p == NULL)
        return NULL;
    if (fklVmemCommit(p, size) != 0) {
        goto error;
    }

    if (prot == (FKL_VMEM_R | FKL_VMEM_W)) {
        return p;
    }

    if (fklVmemProtect(p, size, prot) != 0) {
        goto error;
    }

    return p;

    int r = 0;

error:
    r = fklVmemRelease(p, size);
    (void)r;
    return NULL;
}
