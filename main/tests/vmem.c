#include <fakeLisp/vmem.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <setjmp.h>
#include <signal.h>
#endif

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

#ifndef _WIN32
static sigjmp_buf fault_jmp;

static void fault_handler(int sig) {
    (void)sig;
    siglongjmp(fault_jmp, 1);
}

typedef void (*fault_fn)(void *);

static int expect_fault(fault_fn fn, void *p) {
    struct sigaction sa;
    struct sigaction old;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = fault_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, &old) != 0)
        return -1;
    int faulted = 0;
    if (sigsetjmp(fault_jmp, 1) == 0)
        fn(p);
    else
        faulted = 1;
    sigaction(SIGSEGV, &old, NULL);
    return faulted;
}

static void do_write_byte(void *p) { *(volatile unsigned char *)p = 1; }

static void do_read_byte(void *p) { (void)*(volatile unsigned char *)p; }
#endif

static int is_zeroed(unsigned char *p, size_t size) {
    for (size_t i = 0; i < size; ++i)
        if (p[i] != 0)
            return 0;
    return 1;
}

static int is_filled(unsigned char *p, size_t size, unsigned char v) {
    for (size_t i = 0; i < size; ++i)
        if (p[i] != v)
            return 0;
    return 1;
}

int main(void) {
    size_t page = fklVmemPageSize();
    size_t gran = fklVmemGranularity();
    printf("page=%zu gran=%zu\n", page, gran);
    CHECK(page >= 4096 && (page & (page - 1)) == 0,
            "page size >= 4K and a power of two");
    CHECK(gran >= page && gran % page == 0 && (gran & (gran - 1)) == 0,
            "granularity is sane");

    {
        size_t a = fklVmemReserveSize(114);
        size_t b = fklVmemReserveSize(a);
        CHECK(a == b, "round up");
    }

    {
        void *p = fklVmemReserve(page * 2);
        CHECK(p != NULL, "reserve(1) succeeds");
        CHECK((uintptr_t)p % gran == 0, "reserve base is granularity aligned");
        CHECK(fklVmemReserve(0) == NULL, "reserve(0) fails");
        CHECK(fklVmemReserve(SIZE_MAX) == NULL, "reserve(SIZE_MAX) fails");

        void *p2 = fklVmemReserve(1);
        CHECK(p2 != NULL && p2 != p, "second reserve is distinct");
        int r = fklVmemRelease(p2, 1);
        CHECK(r == 0, "release succeeds");

#ifndef _WIN32
        CHECK(expect_fault(do_write_byte, p) == 1,
                "write to reserved page faults");
        CHECK(expect_fault(do_read_byte, p) == 1,
                "read from reserved page faults");
#endif

        CHECK(fklVmemCommit(p, page) == 0, "commit first page");
        CHECK(fklVmemCommit((char *)p + 1, page) == -1,
                "commit with unaligned address fails");
        CHECK(fklVmemCommit(p, 0) == -1, "commit with zero size fails");
        CHECK(fklVmemCommit(NULL, page) == -1, "commit with NULL fails");
        CHECK(is_zeroed(p, page), "freshly committed page is zero filled");

        memset(p, 0xAB, page);
        CHECK(is_filled(p, page, 0xAB), "write/read pattern on committed page");

        CHECK(fklVmemCommit((char *)p + page, 1) == 0,
                "commit second page (size rounded up)");
        ((volatile unsigned char *)p)[page] = 42;
        CHECK(((volatile unsigned char *)p)[page] == 42, "second page usable");

        CHECK(fklVmemProtect(p, page, FKL_VMEM_R) == 0, "protect to R");
        (void)((volatile unsigned char *)p)[0];
#ifndef _WIN32
        CHECK(expect_fault(do_write_byte, p) == 1, "write to R page faults");
#endif
        CHECK(fklVmemProtect(p, page, FKL_VMEM_R | FKL_VMEM_W) == 0,
                "protect back to RW");
        ((volatile unsigned char *)p)[1] = 9;
        CHECK(((volatile unsigned char *)p)[1] == 9,
                "RW write after reprotect");

        CHECK(fklVmemProtect(p, page, FKL_VMEM_NONE) == 0, "protect to NONE");
#ifndef _WIN32
        CHECK(expect_fault(do_read_byte, p) == 1, "read from NONE page faults");
#endif
        CHECK(fklVmemProtect(p, page, (FklVmemProt)(FKL_VMEM_R | 0x8)) == -1,
                "invalid prot flags rejected");
        CHECK(fklVmemProtect(p, page, FKL_VMEM_R | FKL_VMEM_W) == 0,
                "restore RW");

        memset(p, 0xCD, page);
        CHECK(fklVmemDecommit(p, page) == 0, "decommit");
#ifndef _WIN32
        CHECK(expect_fault(do_read_byte, p) == 1,
                "read from decommitted page faults");
#endif
        CHECK(fklVmemCommit(p, page) == 0, "re-commit after decommit");
        CHECK(is_zeroed(p, page), "re-committed page is zero again");

        r = fklVmemRelease(p, 1);
        CHECK(r == 0, "release succeeds");

        CHECK(fklVmemCommit(p, page) == -1, "commit after release fails");
    }

    {
        void *a = fklVmemAlloc(2 * page, FKL_VMEM_R | FKL_VMEM_W);
        CHECK(a != NULL, "alloc RW");
        memset(a, 0x5A, 2 * page);
        CHECK(is_filled(a, 2 * page, 0x5A), "alloc RW write/read");
        int r = fklVmemRelease(a, 2 * page);
        CHECK(r == 0, "release succeeds");
    }

    {
        void *e = fklVmemAlloc(page, FKL_VMEM_R | FKL_VMEM_X);
        CHECK(e != NULL, "alloc R|X");
        (void)((volatile unsigned char *)e)[0];
#if !defined(_WIN32) && !defined(__APPLE__)
        CHECK(expect_fault(do_write_byte, e) == 1, "write to R|X page faults");
#endif
        int r = fklVmemRelease(e, page);
        CHECK(r == 0, "release succeeds");
    }

    {
        size_t big_size = (size_t)64 << 20;
        void *big = fklVmemReserve(big_size);
        CHECK(big != NULL, "reserve 64 MiB");
        CHECK(fklVmemCommit((char *)big + gran, page) == 0,
                "commit a page in the middle of a big reservation");
        ((volatile unsigned char *)big)[gran] = 7;
        CHECK(((volatile unsigned char *)big)[gran] == 7,
                "big reservation page usable");
        int r = fklVmemRelease(big, big_size);
        CHECK(r == 0, "release succeeds");
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
