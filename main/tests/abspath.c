#include <fakeLisp/common.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/zmalloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

static void fail(const char *msg, const char *input, const char *expect,
        const char *got) {
    fprintf(stderr,
            "FAIL %s: abspath(\"%s\") = \"%s\", expect \"%s\"\n",
            msg,
            input,
            got ? got : "(null)",
            expect ? expect : "(null)");
    g_fail = 1;
}

static void check_abspath(const char *input,
        const char *expect,
        const char *msg) {
    char *got = fklAbspath(input);
    if (got == NULL || strcmp(got, expect) != 0) {
        fail(msg, input, expect, got);
    } else {
        printf("ok   %s: abspath(\"%s\") = \"%s\"\n", msg, input, got);
    }
    fklZfree(got);
}

static char *join_expected(const char *dir, const char *rel) {
    size_t dir_len = strlen(dir);
    size_t rel_len = strlen(rel);
    char *r = (char *)fklZmalloc(dir_len + 1 + rel_len + 1);
    if (r == NULL)
        abort();
    memcpy(r, dir, dir_len);
    r[dir_len] = FKL_PATH_SEPARATOR;
    memcpy(r + dir_len + 1, rel, rel_len + 1);
    return r;
}

// Rewrite '/' to the platform separator so the same literals can be used on
// POSIX and Windows. On POSIX this is just a copy.
static char *native_sep(const char *s) {
    char *r = fklZstrdup(s);
    if (r == NULL)
        abort();
    for (char *p = r; *p; ++p)
        if (*p == '/')
            *p = FKL_PATH_SEPARATOR;
    return r;
}

int main(void) {
    // NULL in, NULL out
    if (fklAbspath(NULL) != NULL) {
        fprintf(stderr, "FAIL null: abspath(NULL) should be NULL\n");
        g_fail = 1;
    } else {
        printf("ok   null: abspath(NULL) = (null)\n");
    }

#ifndef _WIN32
    // Lexical normalization of absolute paths is a POSIX-only guarantee: on
    // Windows fklAbspath() delegates to GetFullPathNameW() (via _fullpath(),
    // see fklRealpath()), which also prefixes the drive and rewrites the
    // separators, so the exact expected strings differ there.
    check_abspath("/a/b/../c", "/a/c", "dotdot");
    check_abspath("/a/./b", "/a/b", "dot");
    check_abspath("/a//b/", "/a/b", "collapse");
    check_abspath("/a/b/../../c", "/c", "dotdot-above");
    check_abspath("/a/../../b", "/b", "dotdot-past-root");
    check_abspath("/..", "/", "dotdot-at-root");
    check_abspath("/", "/", "root");
    check_abspath("//a", "//a", "two-leading-slashes");
#endif

    char *cwd = fklSysgetcwd();
    if (cwd == NULL)
        abort();

    // "." resolves to the current working directory
    check_abspath(".", cwd, "curdir");

    // relative path is joined with cwd and normalized
    {
        char *e = join_expected(cwd, "foo");
        check_abspath("foo", e, "relative");
        fklZfree(e);
    }
    {
        char *in = native_sep("foo/./bar/../baz");
        char *rel = native_sep("foo/baz");
        char *e = join_expected(cwd, rel);
        check_abspath(in, e, "relative-normalized");
        fklZfree(in);
        fklZfree(rel);
        fklZfree(e);
    }
    {
        // "../foo" -> parent(cwd)/foo
        char *in = native_sep("../foo");
        char *parent = fklDupDir(cwd);
        char *e = join_expected(parent, "foo");
        check_abspath(in, e, "parent");
        fklZfree(in);
        fklZfree(e);
        fklZfree(parent);
    }
    {
        // "a/../../b" -> parent(cwd)/b (a leading ".." is kept for relatives)
        char *in = native_sep("a/../../b");
        char *rel = native_sep("b");
        char *parent = fklDupDir(cwd);
        char *e = join_expected(parent, rel);
        check_abspath(in, e, "relative-dotdot");
        fklZfree(in);
        fklZfree(rel);
        fklZfree(e);
        fklZfree(parent);
    }

    // already absolute input is returned as-is (modulo normalization)
    check_abspath(cwd, cwd, "cwd-absolute");

    // idempotency: abspath(abspath(x)) == abspath(x)
    {
        char *in = native_sep("../foo/./bar/..");
        char *a = fklAbspath(in);
        char *b = a ? fklAbspath(a) : NULL;
        if (a == NULL || b == NULL || strcmp(a, b) != 0) {
            fprintf(stderr,
                    "FAIL idempotent: \"%s\" vs \"%s\"\n",
                    a ? a : "(null)",
                    b ? b : "(null)");
            g_fail = 1;
        } else {
            printf("ok   idempotent: \"%s\"\n", a);
        }
        fklZfree(in);
        fklZfree(a);
        fklZfree(b);
    }

    fklZfree(cwd);

    if (g_fail) {
        fprintf(stderr, "abspath: FAILED\n");
        return 1;
    }
    printf("abspath: all passed\n");
    return 0;
}
