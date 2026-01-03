#include <fakeLisp/codegen.h>
#include <fakeLisp/vm.h>

int main() {
    printf("sizeof(FklVM) = %zu\n", sizeof(FklVM));
    printf("sizeof(FklVMgc) = %zu\n", sizeof(FklVMgc));
    printf("sizeof(FklVMvalueUd) = %zu\n", sizeof(FklVMvalueUd));
    printf("sizeof(FklVMvalueCgMacroScope) = %zu\n",
            sizeof(FklVMvalueCgMacroScope));
    printf("sizeof(FklVMvalue) = %zu\n", sizeof(FklVMvalue));
    printf("sizeof(FklVMvalueProc) = %zu\n", sizeof(FklVMvalueProc));
    printf("FKL_OPCODE_NUM = %u\n", FKL_OPCODE_NUM);
    return 0;
}
