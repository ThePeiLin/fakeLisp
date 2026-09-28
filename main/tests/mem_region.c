#include <fakeLisp/mem_region.h>
#include <fakeLisp/vmem.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        ++checks;                                                              \
        if (cond) {                                                            \
            printf("ok: %s\n", msg);                                           \
        } else {                                                               \
            ++failures;                                                        \
            printf("FAILED: %s (line %d)\n", msg, __LINE__);                   \
        }                                                                      \
    } while (0)

static int is_zeroed(const unsigned char *p, size_t size) {
    for (size_t i = 0; i < size; ++i)
        if (p[i] != 0)
            return 0;
    return 1;
}

static int is_filled(const unsigned char *p, size_t size, unsigned char v) {
    for (size_t i = 0; i < size; ++i)
        if (p[i] != v)
            return 0;
    return 1;
}

int main(void) {
    size_t page = fklVmemPageSize();
    size_t gran = fklVmemGranularity();
    printf("page=%zu gran=%zu\n", page, gran);

    {
        size_t hdr = fklMemRegionHeaderSize();
        CHECK(hdr > 0 && hdr < page, "header size is small and positive");
    }

    /* reserve: basic shape and writability */
    {
        void *r = fklMemRegionReserve(1);
        CHECK(r != NULL, "reserve(1) succeeds");

        size_t size = fklMemRegionSize(r);
        size_t usable = fklMemRegionUsableSize(r);
        CHECK(size >= 1, "usable reserved size >= 1");
        CHECK(usable > 0 && usable <= size, "0 < usable <= reserved");
        CHECK(size == usable, "reserve(1) is fully committed");
        CHECK((size + fklMemRegionHeaderSize()) % gran == 0,
                "reserved total is granularity aligned");

        CHECK(is_zeroed(r, usable), "usable region starts zeroed");
        memset(r, 0xAB, usable);
        CHECK(is_filled(r, usable, 0xAB), "usable region is writable");

        CHECK(fklMemRegionRelease(r) == 0, "release succeeds");
    }

    /* reserve: failure / edge inputs */
    {
        CHECK(fklMemRegionReserve(SIZE_MAX / 2) == NULL,
                "reserve(too large) fails");

        void *z = fklMemRegionReserve(0);
        CHECK(z != NULL, "reserve(0) still yields a region");
        CHECK(fklMemRegionUsableSize(z) > 0, "reserve(0) has usable space");
        CHECK(fklMemRegionRelease(z) == 0, "release succeeds");
    }

    /* grow: success, zero-fill of the newly committed part, and limits */
    {
        void *r = fklMemRegionReserve(2 * page);
        size_t u0 = fklMemRegionUsableSize(r);
        size_t s0 = fklMemRegionSize(r);
        CHECK(u0 < s0, "reserve(2 pages) has an uncommitted tail");

        CHECK(fklMemRegionGrow(r, 0) == -1, "grow(0) fails");
        CHECK(fklMemRegionGrow(r, page) == 0, "grow by one page");
        CHECK(fklMemRegionUsableSize(r) == u0 + page,
                "usable grew by exactly one page");
        CHECK(is_zeroed((const unsigned char *)r + u0, page),
                "newly grown region is zeroed");

        memset((unsigned char *)r + u0, 0x5A, page);
        CHECK(is_filled((const unsigned char *)r + u0, page, 0x5A),
                "grown region is writable");

        CHECK(fklMemRegionGrow(r, 100 * page) == -1,
                "grow beyond reservation fails");
        CHECK(fklMemRegionGrow(r, SIZE_MAX) == -1, "grow(overflow) fails");

        CHECK(fklMemRegionRelease(r) == 0, "release succeeds");
    }

    /* reserve(1) has no room to grow */
    {
        void *r = fklMemRegionReserve(1);
        CHECK(fklMemRegionGrow(r, page) == -1, "grow past a one page region fails");
        CHECK(fklMemRegionRelease(r) == 0, "release succeeds");
    }

    /* growTo: absolute usable size, idempotence and limits */
    {
        void *r = fklMemRegionReserve(2 * page);
        CHECK(fklMemRegionGrowTo(r, page) == 0, "growTo one page");
        size_t u = fklMemRegionUsableSize(r);
        CHECK(u >= page, "usable reached one page");

        CHECK(fklMemRegionGrowTo(r, page) == 0, "growTo is idempotent");
        CHECK(fklMemRegionUsableSize(r) == u, "idempotent growTo keeps size");

        CHECK(fklMemRegionGrowTo(r, 1) == 0, "growTo smaller is a no-op");
        CHECK(fklMemRegionUsableSize(r) == u, "no-op growTo keeps size");

        CHECK(fklMemRegionGrowTo(r, 100 * page) == -1,
                "growTo beyond reservation fails");

        CHECK(fklMemRegionRelease(r) == 0, "release succeeds");
    }

    /* shrinkTo: shrink, no-op, failure, and zero on regrow */
    {
        void *r = fklMemRegionReserve(2 * page);
        CHECK(fklMemRegionGrowTo(r, 2 * page) == 0, "growTo two pages");
        size_t full = fklMemRegionUsableSize(r);
        CHECK(full >= 2 * page, "usable at least two pages");

        memset(r, 0xCD, full);
        CHECK(is_filled(r, full, 0xCD), "pattern written over full region");

        CHECK(fklMemRegionShrinkTo(r, page) == 0, "shrinkTo one page");
        size_t shrunk = fklMemRegionUsableSize(r);
        CHECK(shrunk >= page && shrunk < full, "usable shrank");

        CHECK(fklMemRegionShrinkTo(r, page) == 0, "shrinkTo current is a no-op");
        CHECK(fklMemRegionUsableSize(r) == shrunk, "no-op shrinkTo keeps size");

        CHECK(fklMemRegionShrinkTo(r, 100 * page) == -1,
                "shrinkTo larger fails");

        CHECK(fklMemRegionGrowTo(r, 2 * page) == 0, "growTo back to two pages");
        CHECK(fklMemRegionUsableSize(r) == full, "usable restored");
        CHECK(is_filled(r, shrunk, 0xCD),
                "still committed part keeps its pattern");
        CHECK(is_zeroed((const unsigned char *)r + shrunk, full - shrunk),
                "decommitted tail reads zero after regrow");

        CHECK(fklMemRegionRelease(r) == 0, "release succeeds");
    }

    /* independent regions */
    {
        void *a = fklMemRegionReserve(page);
        void *b = fklMemRegionReserve(page);
        CHECK(a != NULL && b != NULL && a != b, "two regions are distinct");

        size_t ua = fklMemRegionUsableSize(a);
        memset(a, 0x11, ua);
        CHECK(is_filled(a, ua, 0x11), "region a is writable");
        CHECK(is_zeroed(b, fklMemRegionUsableSize(b)), "region b is untouched");

        CHECK(fklMemRegionRelease(a) == 0, "release a");
        CHECK(fklMemRegionRelease(b) == 0, "release b");
    }

    CHECK(fklMemRegionRelease(NULL) == 0, "release(NULL) succeeds");

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
