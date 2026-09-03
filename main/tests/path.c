#include <fakeLisp/base.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

static int verify(FklVM *vm, FklVMvalue *v, size_t count, ...) {
    if (!FKL_IS_VECTOR(v)) {
        fprintf(stderr, "not a vector\n");
        abort();
    }
    if (FKL_VM_VEC(v)->size != count) {
        fprintf(stderr, "count is not match\n");
        abort();
    }

    va_list ap;
    va_start(ap, count);
    for (size_t i = 0; i < count; ++i) {
        const char *s = va_arg(ap, const char *);
        FklVMvalue *ss = fklVMaddSymbolCstr(vm, s);
        FklVMvalue *a = FKL_VM_VEC(v)->base[i];
        if (!FKL_IS_SYM(a)) {
            fprintf(stderr, "%zu is not a symbol\n", i);
        }

        if (a != ss) {
            fprintf(stderr,
                    "expect \"%s\" but got \"%s\"\n",
                    s,
                    FKL_VM_SYM(a)->str);
            abort();
        }
    }
    va_end(ap);

    return 0;
}

int main() {
    FklVMgc *gc = fklCreateVMgc();
    FklVM *vm = &gc->gcvm;

    FklVMvalue *cwd = NULL;

    {
        char *c = fklSysgetcwd();
        cwd = fklVMaddSymbolCstr(vm, c);
        fklZfree(c);
    }

    if (cwd == NULL)
        abort();

    FklVMvalue *v = NULL;
    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    verify(vm, v, 1, FKL_VM_SYM(cwd)->str);

    fklSysSetEnv(FKL_PATH_ENV, ";;", 1);

    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    verify(vm, v, 0);

    fklSysSetEnv(FKL_PATH_ENV, "a;b;", 1);
    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    verify(vm, v, 2, "a", "b");

    fklSysSetEnv(FKL_PATH_ENV, "a;b;c", 1);
    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    verify(vm, v, 3, "a", "b", "c");

    fklSysSetEnv(FKL_PATH_ENV, "a;;c", 1);
    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    verify(vm, v, 2, "a", "c");

    fklSysSetEnv(FKL_PATH_ENV, "foo;;bar", 1);
    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    verify(vm, v, 2, "foo", "bar");

    fklDestroyVMgc(gc);
    return 0;
}
