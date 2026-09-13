#include <fakeLisp/base.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include <string.h>

#ifndef TEST_DIR_PATH
#define TEST_DIR_PATH "."
#endif

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

static void check_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "check failed: %s\n", msg);
        abort();
    }
}

static void check_str(const char *expect, const char *got, const char *msg) {
    if (got == NULL || strcmp(expect, got) != 0) {
        fprintf(stderr,
                "check failed: %s: expect \"%s\" but got \"%s\"\n",
                msg,
                expect,
                got ? got : "(null)");
        abort();
    }
}

static const char *path_type_str(FklCgLibPathType pt) {
    switch (pt) {
    case FKL_CG_LIB_PATH_REL:
        return "REL";
    case FKL_CG_LIB_PATH_ENV:
        return "ENV";
    case FKL_CG_LIB_PATH_ABS:
        return "ABS";
    case FKL_CG_LIB_PATH_NONE:
        return "NONE";
    }
    return "?";
}

static const char *file_type_str(FklFileType ft) {
    switch (ft) {
    case FKL_FILE_NONE:
        return "NONE";
    case FKL_FILE_SCRIPT:
        return "SCRIPT";
    case FKL_FILE_PACKAGE:
        return "PACKAGE";
    case FKL_FILE_PRECOMPILE:
        return "PRECOMPILE";
    case FKL_FILE_DLL:
        return "DLL";
    }
    return "?";
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

    fklSysSetEnv(FKL_PATH_ENV, "/a;/b;", 1);
    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    verify(vm, v, 2, "/a", "/b");

    fklSysSetEnv(FKL_PATH_ENV, "/a;/b;/c", 1);
    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    verify(vm, v, 3, "/a", "/b", "/c");

    fklSysSetEnv(FKL_PATH_ENV, "/a;;/c", 1);
    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    verify(vm, v, 2, "/a", "/c");

    fklSysSetEnv(FKL_PATH_ENV, "/foo;;/bar", 1);
    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    verify(vm, v, 2, "/foo", "/bar");

    fklSysSetEnv(FKL_PATH_ENV, ".;..", 1);
    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    char *dir1 = fklRealpath(".");
    char *dir2 = fklRealpath("..");
    verify(vm, v, 2, dir1, dir2);
    fklZfree(dir1);
    fklZfree(dir2);
    dir1 = NULL;
    dir2 = NULL;

    fklSysSetEnv(FKL_PATH_ENV, "./foo;../foo", 1);
    v = fklInitDefaultLibPath(vm);
    fklPrin1VMvalue(v, stdout, vm);
    putchar('\n');

    verify(vm, v, 2, "./foo", "../foo");
    fklZfree(dir1);
    fklZfree(dir2);
    dir1 = NULL;
    dir2 = NULL;

    // classification of import path types
    {
        const char *env_path = fklSysGetEnv(FKL_PATH_ENV);
        printf("\n[classify] FKL_PATH=%s\n", env_path ? env_path : "(null)");

        FklVMvalue *paths_v = fklInitDefaultLibPath(vm);
        FklVMvalueVec *paths = FKL_VM_VEC(paths_v);
        printf("[classify] paths: ");
        fklPrin1VMvalue(paths_v, stdout, vm);
        putchar('\n');

        FklFileType ft = FKL_FILE_NONE;
        FklCgLibPathType pt = FKL_CG_LIB_PATH_NONE;
        const char *cwd_str = FKL_VM_SYM(cwd)->str;

        const char *names[] = { "foo", "./foo", "../foo", "_foo" };
        const FklCgLibPathType expect[] = {
            FKL_CG_LIB_PATH_ENV,
            FKL_CG_LIB_PATH_REL,
            FKL_CG_LIB_PATH_REL,
            FKL_CG_LIB_PATH_REL,
        };

        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
            FklVMvalue *rp = fklSearchLibPath(vm,
                    cwd_str,
                    paths,
                    fklVMaddSymbolCstr(vm, names[i]),
                    &ft,
                    &pt);

            printf("[classify] %-8s -> path type %s, file type %s, rp %s\n",
                    names[i],
                    path_type_str(pt),
                    file_type_str(ft),
                    rp ? FKL_VM_SYM(rp)->str : "(null)");

            if (pt != expect[i]) {
                fprintf(stderr,
                        "check failed: %s should be %s but got %s\n",
                        names[i],
                        path_type_str(expect[i]),
                        path_type_str(pt));
                abort();
            }
        }
    }

    // multi entry FKL_PATH: the lib is in the second entry
    {
        fklSysSetEnv(FKL_PATH_ENV, "/fkl-nonexistent-xxx;" TEST_DIR_PATH, 1);
        printf("\n[multi-entry] FKL_PATH=%s\n", fklSysGetEnv(FKL_PATH_ENV));

        FklVMvalue *paths_v = fklInitDefaultLibPath(vm);
        FklVMvalueVec *paths = FKL_VM_VEC(paths_v);
        printf("[multi-entry] paths: ");
        fklPrin1VMvalue(paths_v, stdout, vm);
        putchar('\n');

        FklFileType ft = FKL_FILE_NONE;
        FklCgLibPathType pt = FKL_CG_LIB_PATH_NONE;

        FklVMvalue *rp = fklSearchLibPath(vm,
                FKL_VM_SYM(cwd)->str,
                paths,
                fklVMaddSymbolCstr(vm, "test-lib"),
                &ft,
                &pt);

        printf("[multi-entry] test-lib -> path type %s, file type %s, rp %s\n",
                path_type_str(pt),
                file_type_str(ft),
                rp ? FKL_VM_SYM(rp)->str : "(null)");

        check_true(rp != NULL,
                "test-lib should be found in 2nd FKL_PATH entry");
        check_true(ft == FKL_FILE_SCRIPT, "test-lib should be a script");
        check_true(pt == FKL_CG_LIB_PATH_ENV, "test-lib should be ENV");

        char *exp = fklRealpath(
                TEST_DIR_PATH FKL_PATH_SEPARATOR_STR "test-lib.fkl");
        check_true(exp != NULL, "realpath of test-lib.fkl");
        printf("[multi-entry] expected rp %s\n", exp);
        check_str(exp, FKL_VM_SYM(rp)->str, "ENV search result path");
        fklZfree(exp);
    }

    // relative resolution
    {
        printf("\n[relative] cwd=%s\n", TEST_DIR_PATH);

        FklFileType ft = FKL_FILE_NONE;
        FklVMvalue *rp = fklSearchLibPath1(vm,
                TEST_DIR_PATH,
                NULL,
                fklVMaddSymbolCstr(vm, "test-lib"),
                FKL_CG_LIB_PATH_REL,
                &ft);

        printf("[relative] test-lib -> file type %s, rp %s\n",
                file_type_str(ft),
                rp ? FKL_VM_SYM(rp)->str : "(null)");

        check_true(rp != NULL, "REL test-lib should be found");
        check_true(ft == FKL_FILE_SCRIPT, "REL test-lib should be a script");

        char *exp = fklRealpath(
                TEST_DIR_PATH FKL_PATH_SEPARATOR_STR "test-lib.fkl");
        check_true(exp != NULL, "realpath of test-lib.fkl");
        printf("[relative] expected rp %s\n", exp);
        check_str(exp, FKL_VM_SYM(rp)->str, "REL search result path");
        fklZfree(exp);
    }

    // fklVMpathVecToString
    {
        printf("\n[path-vec->string]\n");

        FklVMvalue *pv = fklCreateVMvalueVec(vm, 2);
        FKL_VM_VEC(pv)->base[0] = fklVMaddSymbolCstr(vm, "/a");
        FKL_VM_VEC(pv)->base[1] = fklVMaddSymbolCstr(vm, "/b");

        FklVMvalue *s = fklVMpathVecToString(vm, pv);
        check_true(s != NULL && FKL_IS_STR(s), "path vec -> string");
        printf("[path-vec->string] [/a /b] -> %s\n", FKL_VM_STR(s)->str);
        check_str("/a;/b", FKL_VM_STR(s)->str, "joined path");

        FklVMvalue *empty = fklCreateVMvalueVec(vm, 0);
        FklVMvalue *es = fklVMpathVecToString(vm, empty);
        check_true(es != NULL && FKL_IS_STR(es), "empty path vec -> string");
        printf("[path-vec->string] [] -> \"%s\"\n", FKL_VM_STR(es)->str);
        check_str("", FKL_VM_STR(es)->str, "empty joined path");

        FklVMvalue *bad = fklCreateVMvalueVec(vm, 1);
        FKL_VM_VEC(bad)->base[0] = FKL_MAKE_VM_FIX(1);
        FklVMvalue *bs = fklVMpathVecToString(vm, bad);
        printf("[path-vec->string] [1] -> %s\n", bs ? "value" : "NULL");
        check_true(bs == NULL, "invalid path element -> NULL");
    }

    // fklVMpathStrToVec
    {
        const char *input = "fkl-no-a;;fkl-no-b;";
        printf("\n[path-string->vec] input \"%s\"\n", input);

        FklVMvalue *v = fklVMpathStrToVec(vm, input);
        FklVMvalueVec *vec = FKL_VM_VEC(v);
        printf("[path-string->vec] result: ");
        fklPrin1VMvalue(v, stdout, vm);
        putchar('\n');

        check_true(vec->size == 2, "path string -> vec count");
        check_str("fkl-no-a", FKL_VM_SYM(vec->base[0])->str, "path entry 0");
        check_str("fkl-no-b", FKL_VM_SYM(vec->base[1])->str, "path entry 1");
    }

    // fklDupDir
    {
        const char *s = "";
        char *r = fklDupDir(s);
        check_str(".", r, "dir dup 1");
        fklZfree(r);
        r = NULL;

        s = "abcd/efgh/";
        r = fklDupDir(s);
        check_str("abcd", r, "dir dup 2");
        fklZfree(r);
        r = NULL;

        s = "abcd";
        r = fklDupDir(s);
        check_str(".", r, "dir dup 3");
        fklZfree(r);
        r = NULL;

        s = "abcd/";
        r = fklDupDir(s);
        check_str(".", r, "dir dup 4");
        fklZfree(r);
        r = NULL;

        s = "/abcd";
        r = fklDupDir(s);
        check_str("/", r, "dir dup 5");
        fklZfree(r);
        r = NULL;
    }

    fklDestroyVMgc(gc);
    return 0;
}
