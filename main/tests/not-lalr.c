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

static const char example_grammer_rules[] = ""
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
        exit(0);
    } else {
        fklDestroyVMgc(gc);

        fklCodeBuilderFmt(&err_out, "expect error\n");
        fklUninitStrBuf(&err_msg);
        exit(1);
    }

    return 0;
}
