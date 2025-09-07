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

FKL_DLL_EXPORT FklSid_t *_fklExportSymbolInit(
        FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS) {
    *num = EXPORT_NUM;
    FklSid_t *symbols = (FklSid_t *)fklZmalloc(sizeof(FklSid_t) * EXPORT_NUM);
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklAddSymbolCstr(exports_and_func[i].sym, st);
    return symbols;
}

FKL_DLL_EXPORT FklVMvalue **_fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    *count = EXPORT_NUM;
    FklVMvalue **loc =
            (FklVMvalue **)fklZmalloc(sizeof(FklVMvalue *) * EXPORT_NUM);
    FKL_ASSERT(loc);
    for (size_t i = 0; i < EXPORT_NUM; i++) {
        loc[i] = FKL_REMOVE_CONST(FklVMvalue, exports_and_func[i].v);
    }
    return loc;
}
