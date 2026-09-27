#include <fakeLisp/common.h>
#include <fakeLisp/mem_region.h>
#include <fakeLisp/vmem.h>

#include <stdalign.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct {
    uint64_t total;
    uint64_t commited;
    alignas(alignof(max_align_t)) uint8_t data[FKL_FLEX_ARRAY_MEMBER];
} MemRegionHeader;

void *fklMemRegionReserve(size_t size) {
    size_t const page_size = fklVmemPageSize();
    // 总不能真有这么小的页吧
    FKL_ASSERT(page_size > sizeof(MemRegionHeader));

    size_t total_size = size + sizeof(MemRegionHeader);

    total_size = fklVmemReserveSize(total_size);
    void *addr = fklVmemReserve(total_size);

    if (addr == NULL)
        return NULL;

    // 提交第一页，我们要用来存储一些元信息
    if (fklVmemCommit(addr, page_size) != 0) {
        int r = fklVmemRelease(addr, total_size);
        (void)r;
        return NULL;
    }

    MemRegionHeader *header = (MemRegionHeader *)addr;

    header->total = total_size;
    header->commited = page_size;

    return header->data;
}

static FKL_ALWAYS_INLINE size_t page_round_up(size_t s) {
    return fklVmemRoundUp(s, fklVmemPageSize());
}

int fklMemRegionGrow(void *data, size_t size) {
    MemRegionHeader *header = FKL_CONTAINER_OF(data, MemRegionHeader, data);
    size = page_round_up(size);
    size_t new_size = header->commited + size;
    if (new_size > header->total)
        return -1;

    int r = fklVmemCommit(header, new_size);
    if (r != 0)
        return -1;

    header->commited = new_size;
    return 0;
}

int fklMemRegionGrowTo(void *data, size_t size) {
    MemRegionHeader *header = FKL_CONTAINER_OF(data, MemRegionHeader, data);
    size = page_round_up(size);
    if (header->total < size)
        return -1;

    if (size < header->commited)
        return 0;

    int r = fklVmemCommit(header, size);
    if (r != 0)
        return -1;

    header->commited = size;
    return 0;
}

int fklMemRegionShrinkTo(void *data, size_t size) {
    MemRegionHeader *header = FKL_CONTAINER_OF(data, MemRegionHeader, data);
    size = page_round_up(size);
    if (size > header->commited)
        return -1;
    int r = fklVmemDecommit(header, size);
    if (r != 0)
        return -1;
    header->commited = size;
    return 0;
}

int fklMemRegionRelease(void *data) {
    if (data == NULL)
        return 0;

    MemRegionHeader *header = FKL_CONTAINER_OF(data, MemRegionHeader, data);
    size_t total_size = header->total;
    return fklVmemRelease(header, total_size);
}

size_t fklMemRegionSize(void *data) {
    MemRegionHeader *header = FKL_CONTAINER_OF(data, MemRegionHeader, data);
    size_t header_size = offsetof(MemRegionHeader, data);
    return header->total - header_size;
}

size_t fklMemRegionUsableSize(void *data) {
    MemRegionHeader *header = FKL_CONTAINER_OF(data, MemRegionHeader, data);
    size_t header_size = offsetof(MemRegionHeader, data);
    return header->commited - header_size;
}
