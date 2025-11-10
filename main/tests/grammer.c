#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    FklVMgc *gc = fklCreateVMgc(fklCreateVMobarray());
    FklGrammer *g = fklCreateBuiltinGrammer(&gc->gcvm);
    if (!g) {
        fklDestroyVMgc(gc);
        fprintf(stderr, "garmmer create fail\n");
        exit(1);
    }
    if (g->sorted_delimiters) {
        fputs("\nterminals:\n", stdout);
        for (size_t i = 0; i < g->sorted_delimiters_num; i++)
            fprintf(stdout, "%s\n", g->sorted_delimiters[i]->str);
        fputc('\n', stdout);
    }
    fputs("grammer:\n", stdout);
    fklPrintGrammer(g, stdout);
    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fputc('\n', stdout);
    // fputs("item sets:\n",stdout);
    fklLr0ToLalrItems(itemSet, g);
    // fklPrintItemStateSet(itemSet,g,st,stdout);

    FklStringBuffer err_msg;
    fklInitStringBuffer(&err_msg);
    if (fklGenerateLalrAnalyzeTable(g, itemSet, &err_msg)) {
        fklDestroyVMgc(gc);
        fprintf(stderr, "not lalr garmmer\n");
        fprintf(stderr, "%s\n", err_msg.buf);
        fklUninitStringBuffer(&err_msg);
        exit(1);
    }
    // fklPrintAnalysisTable(g,st,stdout);
    fklLalrItemSetHashMapDestroy(itemSet);
    fklUninitStringBuffer(&err_msg);

    fputc('\n', stdout);

    const char *exps[] = {
        "#\\\\11",
        "#\\\\z",
        "#\\\\n",
        "#\\\\",
        "#\\;",
        "#\\|",
        "#\\\"",
        "#\\(",
        "#\\\\s",
        "(abcd)abcd",
        ";comments\nabcd",
        "foobar|foo|foobar|bar|",
        "(\"foo\" \"bar\" \"foobar\",;abcd\n\"i\")",
        "[(foobar;comments\nfoo bar),abcd]",
        "(foo bar abcd|foo \\|bar|efgh foo \"foo\\\"\",bar)",
        "#hash((a,1) (b,2))",
        "#hashequal((a,1) (b,2))",
        "#\"\\114\\51\\11\\45\\14\"",
        "114514",
        "#\\ ",
        "'#&#(foo 0x114514 \"foobar\" .1 0x1p1 114514|foo|bar #\\a #\\\\0 #\\\\x11 #\\\\0123 #\\\\0177 #\\\\0777)",
        "\"foobar\"",
        "114514",
        NULL,
    };

    FklParseError retval = 0;
    FklGrammerMatchCtx ctx = FKL_VMVALUE_PARSE_CTX_INIT(&gc->gcvm, NULL);

    for (const char **exp = &exps[0]; *exp; exp++) {

        FklVMvalue *ast = fklParseWithTableForCstr(g, *exp, &ctx, &retval);

        if (retval)
            break;

        fklPrin1VMvalue(ast, stdout, &gc->gcvm);
        fputc('\n', stdout);
    }
    fklDestroyGrammer(g);
    fklDestroyVMgc(gc);
    return retval;
}
