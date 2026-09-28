#include <fakeLisp/code.h>
#include <fakeLisp/mem_region.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/vmem.h>

#include "../src/bdb/bdb.h"

int main() {
    printf("sizeof(FklVM) = %zu\n", sizeof(FklVM));
    printf("sizeof(FklVMgc) = %zu\n", sizeof(FklVMgc));
    printf("sizeof(FklVMvalueUd) = %zu\n", sizeof(FklVMvalueUd));
    printf("sizeof(FklVMvalueDll) = %zu\n", sizeof(FklVMvalueDll));
    printf("sizeof(FklVMvalueCgMacroScope) = %zu\n",
            sizeof(FklVMvalueCgMacroScope));
    printf("sizeof(FklVMvalue) = %zu\n", sizeof(FklVMvalue));
    printf("sizeof(FklVMvalueProc) = %zu\n", sizeof(FklVMvalueProc));
    printf("sizeof(FklVMframe) = %zu\n", sizeof(FklVMframe));
    printf("FKL_OPCODE_NUM = %u\n", FKL_OPCODE_NUM);

    printf("sizeof(DebugCtx) = %zu\n", sizeof(DebugCtx));
    printf("fklVmemPageSize() = %zu\n", fklVmemPageSize());
    printf("fklVmemGranularity() = %zu\n", fklVmemGranularity());

    FklVMgc *gc = fklCreateVMgc(NULL);

    {
        size_t total = fklMemRegionSize(gc);
        size_t usable = fklMemRegionUsableSize(gc);
        size_t offset = offsetof(FklVMgc, gcvm.base);
        printf("total = %zu, usable = %zu, offset = %zu, FklVMgc.gcvm.last = %zu\n",
                total,
                usable,
                offset,
                fklComputeVMstackSize(offset, usable));
    }

    FklVM *vm = fklCreateVM(NULL, gc);

    {
        size_t total = fklMemRegionSize(vm);
        size_t usable = fklMemRegionUsableSize(vm);
        size_t offset = offsetof(FklVM, base);
        printf("total = %zu, usable = %zu, offset = %zu, FklVM.last = %zu, total FklVM.last = %zu\n",
                total,
                usable,
                offset,
                fklComputeVMstackSize(offset, usable),
                fklComputeVMstackSize(offset, total));
    }

    fklDestroyAllVMs(vm);
    fklDestroyVMgc(gc);
    return 0;
}
