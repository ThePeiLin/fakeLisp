#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

static int test_func(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    fputs("testing dll\n", stdout);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

struct SymFunc {
    const char *sym;
    const FklVMvalue *v;
} exports_and_func[] = {
    // clang-format off
    { "test-func", (const FklVMvalue *)&FKL_VM_CPROC_STATIC_INIT("test-func", test_func) },
    // clang-format on
};

static const size_t EXPORT_NUM =
        sizeof(exports_and_func) / sizeof(struct SymFunc);

FKL_DLL_EXPORT FklVMvalue **_fklExportSymbolInit(FklVM *vm, uint32_t *num) {
    *num = EXPORT_NUM;
    FklVMvalue **symbols =
            (FklVMvalue **)fklZmalloc(EXPORT_NUM * sizeof(FklVMvalue *));
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklVMaddSymbolCstr(vm, exports_and_func[i].sym);
    return symbols;
}

FKL_DLL_EXPORT int _fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    FKL_ASSERT(count == EXPORT_NUM);
    for (size_t i = 0; i < EXPORT_NUM; i++) {
        FklVMvalue *r = NULL;
        const FklVMvalue *v = exports_and_func[i].v;
        if (FKL_IS_CPROC(v)) {
            const FklVMvalueCproc *from = FKL_VM_CPROC(v);
            r = fklCreateVMvalueCproc(exe, from->func, dll, NULL, from->name);
        }
        FKL_ASSERT(r);

        values[i] = r;
    }
    return 0;
}

FKL_CHECK_EXPORT_DLL_INIT_FUNC();
FKL_CHECK_IMPORT_DLL_INIT_FUNC();
