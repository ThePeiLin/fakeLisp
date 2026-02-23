#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include <stdio.h>
#include <stdlib.h>

static const FklGrammerBuiltinAction builtin_actions[] = { { "symbol", NULL } };

static inline const FklGrammerBuiltinAction *
builtin_prod_action_resolver(void *ctx, const char *str, size_t len) {
    return &builtin_actions[0];
}

static char example_grammer_rules[] = //
        ""
        "S -> L \"=\" R => test\n"
        "S -> R => test\n"
        "L -> \"*\" R => test\n"
        "L -> ?identifier => test\n"
        "R -> L => test\n"
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
        fklPrintSymbolLiteral2(FKL_VM_SYM(nonterm.sid), &err_out);
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

    fklDestroyGrammer(g);
    fklDestroyVMgc(gc);
    fclose(tablef);
    fclose(gzf);
    fclose(lalrgzf);

    return 0;
}
