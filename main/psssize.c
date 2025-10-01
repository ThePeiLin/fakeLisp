#include <fakeLisp/codegen.h>
#include <fakeLisp/vm.h>

int main() {
    printf("sizeof(FklVM) = %zu\n", sizeof(FklVM));
    printf("sizeof(FklVMgc) = %zu\n", sizeof(FklVMgc));
    printf("sizeof(FklVMvalueUd) = %zu\n", sizeof(FklVMvalueUd));
    printf("sizeof(FklVMvalueCodegenMacroScope) = %zu\n",
            sizeof(FklVMvalueCodegenMacroScope));
    printf("sizeof(FklVMvalue) = %zu\n", sizeof(FklVMvalue));
    printf("FKL_OPCODE_NUM = %u\n", FKL_OPCODE_NUM);
    return 0;
}
