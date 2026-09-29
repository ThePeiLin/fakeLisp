#include <fakeLisp/mem_region.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/vmem.h>

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#define fkl_test_dup _dup
#define fkl_test_dup2 _dup2
#define fkl_test_fileno _fileno
#define fkl_test_close _close
#else
#include <unistd.h>
#define fkl_test_dup dup
#define fkl_test_dup2 dup2
#define fkl_test_fileno fileno
#define fkl_test_close close
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

static int push_and_verify(FklVM *vm, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i)
        FKL_VM_PUSH_VALUE(vm, FKL_MAKE_VM_FIX((int64_t)i));

    if (vm->tp != n)
        return 0;

    for (uint32_t i = 0; i < n; ++i)
        if (vm->base[i] != FKL_MAKE_VM_FIX((int64_t)i))
            return 0;

    return 1;
}

static int g_stderr_saved = -1;
static FILE *g_stderr_tmp = NULL;

static int capture_stderr_begin(void) {
    fflush(stderr);
    g_stderr_saved = fkl_test_dup(fkl_test_fileno(stderr));
    if (g_stderr_saved < 0)
        return -1;
    g_stderr_tmp = tmpfile();
    if (g_stderr_tmp == NULL) {
        fkl_test_close(g_stderr_saved);
        g_stderr_saved = -1;
        return -1;
    }
    if (fkl_test_dup2(fkl_test_fileno(g_stderr_tmp),
                fkl_test_fileno(stderr))
            < 0) {
        fclose(g_stderr_tmp);
        g_stderr_tmp = NULL;
        fkl_test_close(g_stderr_saved);
        g_stderr_saved = -1;
        return -1;
    }
    return 0;
}

static void capture_stderr_end(char *buf, size_t cap) {
    fflush(stderr);
    fkl_test_dup2(g_stderr_saved, fkl_test_fileno(stderr));
    fkl_test_close(g_stderr_saved);
    g_stderr_saved = -1;
    buf[0] = '\0';
    if (g_stderr_tmp) {
        rewind(g_stderr_tmp);
        size_t r = fread(buf, 1, cap - 1, g_stderr_tmp);
        buf[r] = '\0';
        fclose(g_stderr_tmp);
        g_stderr_tmp = NULL;
    }
}

int main(void) {
    FklVMgc *gc = fklCreateVMgc(NULL);
    CHECK(gc != NULL, "create gc");
    if (gc == NULL)
        return 1;

    FklVM *vm = fklCreateVM(NULL, gc);
    CHECK(vm != NULL, "create vm");
    if (vm == NULL) {
        fklDestroyVMgc(gc);
        return 1;
    }

    size_t stack_off = offsetof(FklVM, base);
    printf("reserved=%zu, initial capacity=%u\n",
            fklVMreservedSize(),
            vm->last);

    CHECK(vm->tp == 0, "initial tp == 0");
    CHECK(vm->bp == 0, "initial bp == 0");
    CHECK(vm->last > 0, "initial capacity > 0");
    CHECK(fklMemRegionUsableSize(vm) >= sizeof(FklVM),
            "region holds at least the FklVM struct");
    CHECK(vm->last
                    == fklComputeVMstackSize(stack_off,
                            fklMemRegionUsableSize(vm)),
            "last == slots computed from the usable region");

    uint32_t cap0 = vm->last;

    /* reserving within the current capacity is a no-op */
    CHECK(fklVMstackReserve(vm, 1) == 0, "reserve(1) succeeds");
    CHECK(vm->last == cap0, "reserve(1) keeps capacity");
    CHECK(fklVMstackReserve(vm, cap0) == 0, "reserve(capacity) succeeds");
    CHECK(vm->last == cap0, "reserve(capacity) keeps capacity");

    /* reserving above the capacity commits more of the region */
    CHECK(fklVMstackReserve(vm, cap0 + 4096) == 0,
            "reserve(capacity + 4096) succeeds");
    CHECK(vm->last >= cap0 + 4096, "capacity grew enough");
    CHECK(vm->last
                    == fklComputeVMstackSize(stack_off,
                            fklMemRegionUsableSize(vm)),
            "last matches usable region after growth");

    /* pushing past the capacity grows the stack without moving it */
    {
        uint32_t const n = 10000;
        CHECK(push_and_verify(vm, n), "pushed values are intact");
        CHECK(vm->last >= n, "capacity reached the pushed count");
        CHECK(vm->last > cap0, "pushing triggered stack growth");
    }

    /* pop */
    {
        uint32_t before = vm->tp;
        FklVMvalue *v = FKL_VM_POP_TOP_VALUE(vm);
        CHECK(v == FKL_MAKE_VM_FIX(9999), "pop returns the last pushed value");
        CHECK(vm->tp == before - 1, "pop decrements tp");
    }

    /* growing past the reservation fails */
    CHECK(fklVMstackReserve(vm, 1u << 20) == -1,
            "reserve beyond the reservation fails");

    /* with a mostly full stack the hysteresis keeps the capacity */
    {
        uint32_t before = vm->last;
        fklVMstackShrink(vm);
        CHECK(vm->last == before, "shrink with a mostly full stack is a no-op");
    }

    /* after unwinding, shrink reclaims the committed tail */
    {
        while (vm->tp)
            (void)FKL_VM_POP_TOP_VALUE(vm);
        CHECK(vm->tp == 0, "popped the whole stack");

        uint32_t grown = vm->last;
        fklVMstackShrink(vm);
        CHECK(vm->last < grown, "shrink reclaims capacity after unwinding");
        CHECK(vm->last >= vm->tp, "shrink keeps at least the used slots");
        CHECK(vm->last
                        == fklComputeVMstackSize(stack_off,
                                fklMemRegionUsableSize(vm)),
                "last matches usable region after shrink");

        CHECK(push_and_verify(vm, 1000), "stack still works after shrink");
    }

    /* --- gcvm: a VM embedded in the GC's own region --- */
    {
        FklVM *gv = &gc->gcvm;
        size_t gv_off = offsetof(FklVMgc, gcvm.base);

        CHECK(gv->region == gc, "gcvm region is the gc itself");
        CHECK(gv->tp == 0 && gv->bp == 0, "gcvm starts empty");
        CHECK(gv->last > 0, "gcvm has an initial operand stack");

        size_t expect_last =
                fklComputeVMstackSize(gv_off, fklMemRegionUsableSize(gc));

        CHECK(gv->last == expect_last, "gcvm capacity matches its region");

        uint32_t cap0 = gv->last;
        size_t max_cap = fklComputeVMstackSize(gv_off, fklMemRegionSize(gc));
        CHECK(max_cap > cap0, "gcvm can grow inside the gc region");

        uint32_t n = (uint32_t)(max_cap - 1 < 600 ? max_cap - 1 : 600);
        CHECK(n > cap0, "gcvm growth target exceeds the initial capacity");
        CHECK(push_and_verify(gv, n), "gcvm push/grow keeps values intact");
        CHECK(gv->last >= n, "gcvm capacity reached the pushed count");
        CHECK(gv->last
                        == fklComputeVMstackSize(gv_off,
                                fklMemRegionUsableSize(gc)),
                "gcvm last matches its region after growth");

        CHECK(fklVMstackReserve(gv, 1u << 20) == -1,
                "gcvm reserve beyond the gc region fails");

        while (gv->tp)
            (void)FKL_VM_POP_TOP_VALUE(gv);
        uint32_t grown = gv->last;
        fklVMstackShrink(gv);
        CHECK(gv->last < grown, "gcvm shrink reclaims after unwinding");
        CHECK(gv->last
                        == fklComputeVMstackSize(gv_off,
                                fklMemRegionUsableSize(gc)),
                "gcvm last matches its region after shrink");

        /* the gcvm operand stack must be a GC root */
        {
            FklVMvalue *s = fklCreateVMvalueStr1(gv, "gcvm-survives-gc");
            FKL_VM_PUSH_VALUE(gv, s);
            fklVMgcCheck(gv, 1);
            CHECK(gv->base[0] == s, "gcvm stack kept the value across GC");
            CHECK(FKL_IS_STR(gv->base[0]), "rooted value is still a string");
            CHECK(strcmp(FKL_VM_STR(gv->base[0])->str, "gcvm-survives-gc") == 0,
                    "rooted value content is intact");
            while (gv->tp)
                (void)FKL_VM_POP_TOP_VALUE(gv);
        }
    }

    /* alloced_size accounting: reserve grows it, shrink releases it */
    {
        FklVM *gv = &gc->gcvm;
        void *vm_region = vm->region;

        /* standalone vm */
        while (vm->tp)
            (void)FKL_VM_POP_TOP_VALUE(vm);
        fklVMstackShrink(vm);

        size_t u0 = fklMemRegionUsableSize(vm_region);
        size_t a0 = atomic_load(&gc->alloced_size);
        CHECK(fklVMstackReserve(vm, vm->last + 4096) == 0,
                "vm reserve succeeds");
        size_t u1 = fklMemRegionUsableSize(vm_region);
        size_t a1 = atomic_load(&gc->alloced_size);
        CHECK(u1 > u0, "vm reserve grew the region");
        CHECK(a1 - a0 == u1 - u0, "vm reserve accounts the usable delta");

        while (vm->tp)
            (void)FKL_VM_POP_TOP_VALUE(vm);
        size_t u2 = fklMemRegionUsableSize(vm_region);
        size_t a2 = atomic_load(&gc->alloced_size);
        fklVMstackShrink(vm);
        size_t u3 = fklMemRegionUsableSize(vm_region);
        size_t a3 = atomic_load(&gc->alloced_size);
        CHECK(u3 < u2, "vm shrink released the region");
        CHECK(a2 - a3 == u2 - u3, "vm shrink accounts the released delta");

        /* gcvm */
        while (gv->tp)
            (void)FKL_VM_POP_TOP_VALUE(gv);
        fklVMstackShrink(gv);

        size_t gu0 = fklMemRegionUsableSize(gv->region);
        size_t ga0 = atomic_load(&gc->alloced_size);
        CHECK(fklVMstackReserve(gv, gv->last + 256) == 0,
                "gcvm reserve succeeds");
        size_t gu1 = fklMemRegionUsableSize(gv->region);
        size_t ga1 = atomic_load(&gc->alloced_size);
        CHECK(gu1 > gu0, "gcvm reserve grew the region");
        CHECK(ga1 - ga0 == gu1 - gu0, "gcvm reserve accounts the usable delta");

        while (gv->tp)
            (void)FKL_VM_POP_TOP_VALUE(gv);
        size_t gu2 = fklMemRegionUsableSize(gv->region);
        size_t ga2 = atomic_load(&gc->alloced_size);
        fklVMstackShrink(gv);
        size_t gu3 = fklMemRegionUsableSize(gv->region);
        size_t ga3 = atomic_load(&gc->alloced_size);
        CHECK(gu3 < gu2, "gcvm shrink released the region");
        CHECK(ga2 - ga3 == gu2 - gu3, "gcvm shrink accounts the released delta");
    }

    /* stack-overflow-error: standalone VM with a tiny reservation */
    {
        size_t old = fklVMconfigReservedSize(fklVmemPageSize());
        FklVM *ov = fklCreateVM(NULL, gc);
        CHECK(ov != NULL, "create a small-reservation vm");

        jmp_buf jb;
        jmp_buf *saved = ov->buf;
        ov->buf = &jb;
        volatile int caught = 0;
        if (setjmp(jb) == 0)
            fklVMstackReserveOrRaise(ov, 1u << 20);
        else
            caught = 1;
        ov->buf = saved;

        CHECK(caught, "vm stack reserve overflow raises");
        FklVMvalue *ev = FKL_VM_GET_TOP_VALUE(ov);
        CHECK(fklIsVMvalueError(ev), "vm overflow raised an error value");
        CHECK(FKL_VM_ERR(ev)->type
                      == gc->builtinErrorTypeId[FKL_ERR_STACK_OVERFLOW],
                "vm overflow error type is stack-overflow-error");

        fklDestroyAllVMs(ov);
        fklVMconfigReservedSize(old);
    }

    /* stack-overflow-error: gcvm (bounded by the gc region) */
    {
        FklVM *gv = &gc->gcvm;
        while (gv->tp)
            (void)FKL_VM_POP_TOP_VALUE(gv);

        jmp_buf jb;
        jmp_buf *saved = gv->buf;
        gv->buf = &jb;
        volatile int caught = 0;
        if (setjmp(jb) == 0)
            fklVMstackReserveOrRaise(gv, 1u << 20);
        else
            caught = 1;
        gv->buf = saved;

        CHECK(caught, "gcvm stack reserve overflow raises");
        FklVMvalue *ev = FKL_VM_GET_TOP_VALUE(gv);
        CHECK(fklIsVMvalueError(ev), "gcvm overflow raised an error value");
        CHECK(FKL_VM_ERR(ev)->type
                      == gc->builtinErrorTypeId[FKL_ERR_STACK_OVERFLOW],
                "gcvm overflow error type is stack-overflow-error");

        while (gv->tp)
            (void)FKL_VM_POP_TOP_VALUE(gv);
    }

    fklDestroyAllVMs(vm);
    fklDestroyVMgc(gc);

    /* destroy a GC whose own gcvm stack was grown but never shrunk */
    {
        FklVMgc *gc2 = fklCreateVMgc(NULL);
        CHECK(gc2 != NULL, "create a fresh gc");
        FklVM *gv2 = &gc2->gcvm;
        size_t a0 = atomic_load(&gc2->alloced_size);
        CHECK(fklVMstackReserve(gv2, gv2->last + 256) == 0,
                "grow the fresh gcvm stack");
        CHECK(atomic_load(&gc2->alloced_size) > a0,
                "gcvm growth is accounted");

        char err[512];
        err[0] = '\0';
        if (capture_stderr_begin() == 0) {
            fklDestroyVMgc(gc2);
            capture_stderr_end(err, sizeof(err));
            CHECK(strstr(err, "not freed") == NULL,
                    "destroying a grown gcvm does not report a leak");
        } else {
            fklDestroyVMgc(gc2);
            CHECK(1, "destroyVMgc with an unshrunk gcvm stack survives");
        }
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
