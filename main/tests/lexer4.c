#include <fakeLisp/parser.h>
#include <fakeLisp/vm.h>

static const char *expressions[] = {
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
    FklVMgc *gc = fklCreateVMgc();
    FklVM *vm = &gc->gcvm;

    fputs("parse with builtin parser\n", stderr);
    for (const char **pexp = &expressions[0]; *pexp; ++pexp) {
        FklVMvalue *node = fklCreateAst1(vm, *pexp, NULL);
        FKL_ASSERT(node);
        fklPrin1VMvalue(node, stderr, vm);
        fputc('\n', stderr);
    }

    FklGrammer *g = fklCreateBuiltinGrammer(vm);
    if (!g) {
        fklDestroyVMgc(gc);
        fprintf(stderr, "garmmer create fail\n");
        exit(1);
    }

    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fklLr0ToLalrItems(itemSet, g);

    FklStrBuf err_msg;
    fklInitStrBuf(&err_msg);
    if (fklGenerateLalrAnalyzeTable(vm, g, itemSet, &err_msg)) {
        fklLalrItemSetHashMapDestroy(itemSet);
        fklDestroyVMgc(gc);
        fklDestroyGrammer(g);
        fprintf(stderr, "not lalr garmmer\n");
        fprintf(stderr, "%s\n", err_msg.buf);
        fklUninitStrBuf(&err_msg);
        return 1;
    }

    fklUninitStrBuf(&err_msg);
    fputs("\nparse with custom parser\n", stderr);
    for (const char **pexp = &expressions[0]; *pexp; ++pexp) {
        FklParseError err = 0;
        FklGrammerMatchCtx ctx = FKL_VMVALUE_PARSE_CTX_INIT(vm, NULL);
        FklVMvalue *node = fklParseWithTableForCstr(g, *pexp, &ctx, &err);
        FKL_ASSERT(node && err == 0);
        fklPrin1VMvalue(node, stderr, vm);
        fputc('\n', stderr);
    }
    fklLalrItemSetHashMapDestroy(itemSet);
    fklDestroyGrammer(g);
    fklDestroyVMgc(gc);
    return 0;
}
