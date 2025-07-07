#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/nast.h>
#include <fakeLisp/utils.h>

#include <stdio.h>
#include <stdlib.h>

static void *fklNastTerminalCreate(const char *s, size_t len, size_t line,
                                   void *ctx) {
    FklNastNode *ast = fklCreateNastNode(FKL_NAST_STR, line);
    ast->str = fklCreateString(len, s);
    return ast;
}

int main() {
    FklSymbolTable *st = fklCreateSymbolTable();
    FklGrammer *g = fklCreateBuiltinGrammer(st);
    if (!g) {
        fklDestroySymbolTable(st);
        fprintf(stderr, "garmmer create fail\n");
        exit(1);
    }
    if (g->sortedTerminals) {
        fputs("\nterminals:\n", stdout);
        for (size_t i = 0; i < g->sortedTerminalsNum; i++)
            fprintf(stdout, "%s\n", g->sortedTerminals[i]->str);
        fputc('\n', stdout);
    }
    fputs("grammer:\n", stdout);
    fklPrintGrammer(stdout, g, st);
    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fputc('\n', stdout);
    // fputs("item sets:\n",stdout);
    fklLr0ToLalrItems(itemSet, g);
    // fklPrintItemStateSet(itemSet,g,st,stdout);

    FklStringBuffer err_msg;
    fklInitStringBuffer(&err_msg);
    if (fklGenerateLalrAnalyzeTable(g, itemSet, &err_msg)) {
        fklDestroySymbolTable(st);
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

    int retval = 0;
    FklGrammerMatchOuterCtx outerCtx = {
        .maxNonterminalLen = 0,
        .line = 1,
        .start = NULL,
        .cur = NULL,
        .create = fklNastTerminalCreate,
        .destroy = (void (*)(void *))fklDestroyNastNode,
        .ctx = st};

    for (const char **exp = &exps[0]; *exp; exp++) {

        FklNastNode *ast =
            fklParseWithTableForCstr(g, *exp, &outerCtx, st, &retval);

        if (retval)
            break;

        fklPrintNastNode(ast, stdout, st);
        fklDestroyNastNode(ast);
        fputc('\n', stdout);
    }
    fklDestroyGrammer(g);
    fklDestroySymbolTable(st);
    return retval;
}
