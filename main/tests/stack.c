#include <fakeLisp/mem_region.h>
#include <fakeLisp/vmem.h>
#include <fakeLisp/vm.h>

#include <stddef.h>
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
    printf("reserved=%zu, initial capacity=%u\n", fklVMreservedSize(), vm->last);

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
        CHECK(gv->last
                      == fklComputeVMstackSize(gv_off,
                              fklMemRegionUsableSize(gc)),
                "gcvm capacity matches its region");

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

    fklDestroyAllVMs(vm);
    fklDestroyVMgc(gc);

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
