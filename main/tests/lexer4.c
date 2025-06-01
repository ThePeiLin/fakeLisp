#include <fakeLisp/nast.h>
#include <fakeLisp/parser.h>

static const char *exp[] = {
    ";;abcd\n(abcd,efgh)",                                //
    "(a, b)",                                             //
    "(a , b)",                                            //
    "(a,b)",                                              //
    "(a ,;abcd\n;efgh\n  b)",                             //
    "1234",                                               //
    "()",                                                 //
    "(1234)",                                             //
    "#(() ())",                                           //
    "(() ())",                                            //
    "(\"abcd\")",                                         //
    "#hash((foo,\"abcd\"))",                              //
    "foo",                                                //
    "( )",                                                //
    "foobar|foo|foobar|bar|",                             //
    "(\"foo\" \"bar\" \"foobar\",;abcd\n\"i\")",          //
    "[(foobar;comments\nfoo bar),abcd]",                  //
    "(foo bar abcd|foo \\|bar|efgh foo \"foo\\\"\",bar)", //
    "#hash((a,1) (b,2))",                                 //
    "#\"\\114\\51\\11\\45\\14\"",                         //
    "114514",                                             //
    "#\\ ",                                               //
    "'#&#(foo 0x114514 \"foobar\" .1 0x1p1 114514|foo|bar #\\a #\\\\0 #\\\\x11 #\\\\0123 #\\\\0177 #\\\\0777)", //
    "\"foobar\"", //
    "114514",     //
    "\"foobar\"", //
    "(define (i x)\n(when (not (eq 0 x)) (princ \"Hello,world!\n\") (i (-1+ x))))", //
    NULL, //
};

int main() {
    FklSymbolTable st;
    fklInitSymbolTable(&st);
    fputs("parse with builtin parser\n", stderr);
    for (const char **pexp = &exp[0]; *pexp; ++pexp) {
        FklNastNode *node = fklCreateNastNodeFromCstr(*pexp, &st);
        FKL_ASSERT(node);
        fklPrintNastNode(node, stderr, &st);
        fputc('\n', stderr);
        fklDestroyNastNode(node);
    }

    FklGrammer *g = fklCreateBuiltinGrammer(&st);
    if (!g) {
        fklUninitSymbolTable(&st);
        fprintf(stderr, "garmmer create fail\n");
        exit(1);
    }

    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fklLr0ToLalrItems(itemSet, g);

    if (fklGenerateLalrAnalyzeTable(g, itemSet)) {
        fklLalrItemSetHashMapDestroy(itemSet);
        fklUninitSymbolTable(&st);
        fklDestroyGrammer(g);
        fprintf(stderr, "not lalr garmmer\n");
        return 1;
    }

    fputs("\nparse with custom parser\n", stderr);
    for (const char **pexp = &exp[0]; *pexp; ++pexp) {
        int err = 0;
        FklGrammerMatchOuterCtx outerCtx = FKL_NAST_PARSE_OUTER_CTX_INIT(&st);
        FklNastNode *node =
            fklParseWithTableForCstr(g, *pexp, &outerCtx, &st, &err);
        FKL_ASSERT(node && err == 0);
        fklPrintNastNode(node, stderr, &st);
        fputc('\n', stderr);
        fklDestroyNastNode(node);
    }
    fklLalrItemSetHashMapDestroy(itemSet);
    fklDestroyGrammer(g);
    fklUninitSymbolTable(&st);
    return 0;
}
