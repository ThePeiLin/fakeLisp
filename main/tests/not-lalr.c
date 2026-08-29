#include <fakeLisp/base.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/grammer/action.h"

static const char example_grammer_rules[] =
        ""
		"S -> \"a\" A \"a\" => second\n"
		"  -> \"b\" A \"b\" => second\n"
		"  -> \"a\" B \"b\" => second\n"
		"  -> \"b\" B \"a\" => second\n"
		"A -> \"c\" => first\n"
		"B -> \"c\" => first\n"
        "";

int main() {
    FklVMgc *gc = fklCreateVMgc();
    FklVM *vm = &gc->gcvm;

    FklParserGrammerParseArg args;
    FklGrammer *g = fklCreateEmptyGrammer(vm);

    fklInitParserGrammerParseArg(&args,
            g,
            vm,
            1,
            builtin_prod_action_resolver,
            NULL);
    int err = fklParseProductionRuleWithCstr(&args, example_grammer_rules);
    FklCodeBuilder err_out = { 0 };
    fklInitCodeBuilderFp(&err_out, stderr, NULL);
    if (err) {
        fklPrintParserGrammerParseError(err, &args, &err_out);
        fklCodeBuilderPuts(&err_out, "garmmer create fail\n");
        fklDestroyVMgc(gc);
        fklDestroyGrammer(g);
        fklUninitParserGrammerParseArg(&args);
        exit(1);
    }

    fklUninitParserGrammerParseArg(&args);

    FklGrammerNonterm nonterm = { 0 };
    if (fklCheckAndInitGrammerSymbols(g, &nonterm)) {

        fklCodeBuilderPuts(&err_out, "nonterm: ");
        fklPrintSymbolLiteral2(FKL_VM_SYM(nonterm), &err_out);
        fklCodeBuilderPuts(&err_out, " is not defined\n");

        fklDestroyVMgc(gc);
        fklDestroyGrammer(g);
        exit(1);
    }

    if (g->sorted_delimiters) {
        fputs("\nterminals:\n", stdout);
        for (size_t i = 0; i < g->sorted_delimiters_num; i++)
            fprintf(stdout, "%s\n", g->sorted_delimiters[i]->str);
        fputc('\n', stdout);
    }
    fputs("grammer:\n", stdout);
    fklPrintGrammer(vm, g, stdout);
    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);

    fputc('\n', stdout);
    fputs("item sets:\n", stdout);
    FILE *gzf = fopen("items.gz.txt", "w");
    FILE *lalrgzf = fopen("items-lalr.gz.txt", "w");
    fklPrintItemStateSetAsDot(vm, itemSet, g, gzf);
    fklLr0ToLalrItems(itemSet, g);
    fklPrintItemStateSet(vm, itemSet, g, stdout);
    fklPrintItemStateSetAsDot(vm, itemSet, g, lalrgzf);

    FklStrBuf err_msg;
    fklInitStrBuf(&err_msg);
    if (fklGenerateLalrAnalyzeTable(vm, g, itemSet, &err_msg)) {
        fklDestroyVMgc(gc);

        fklCodeBuilderFmt(&err_out, "not lalr garmmer\n%s\n", err_msg.buf);
        fklUninitStrBuf(&err_msg);
        exit(1);
    }
    fklPrintAnalysisTable(g, stdout);
    fklLalrItemSetHashMapDestroy(itemSet);
    fklUninitStrBuf(&err_msg);

    FILE *tablef = fopen("table.txt", "w");
    fklPrintAnalysisTableForGraphEasy(g, tablef);

    fclose(tablef);
    fclose(gzf);
    fclose(lalrgzf);

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
        "#vu8(114 514 114514)",
        "114514",
        "#\\ ",
        "'#&#(foo 0x114514 \"foobar\" .1 0x1p1 114514|foo|bar #\\a #\\\\0 #\\\\x11 #\\\\0123 #\\\\0177 #\\\\0777)",
        "\"foobar\"",
        "114514",
        NULL,
    };

    FklParseError retval = 0;
    FklGrammerMatchCtx ctx = FKL_VMVALUE_PARSE_CTX_INIT(vm, NULL);

    for (const char **exp = &exps[0]; *exp; exp++) {
        FklVMvalue *ast = fklParseWithTableForCstr(g, *exp, &ctx, &retval);

        if (retval)
            break;

        fklPrin1VMvalue(ast, stdout, vm);
        // fklDestroyNastNode(gc);
        fputc('\n', stdout);
    }
    fklDestroyGrammer(g);
    fklDestroyVMgc(gc);
    return retval;
}
