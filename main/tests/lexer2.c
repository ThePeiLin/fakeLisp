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
    FklVMgc *gc = fklCreateVMgc(fklCreateVMobarray());

    FklParserGrammerParseArg args;
    FklGrammer *g = fklCreateEmptyGrammer(&gc->gcvm);

    fklInitParserGrammerParseArg(&args,
            g,
            gc,
            1,
            builtin_prod_action_resolver,
            NULL);
    int err = fklParseProductionRuleWithCstr(&args, example_grammer_rules);
    if (err) {
        fklPrintParserGrammerParseError(err, &args, stderr);
        fklDestroyVMgc(gc);
        fklDestroyGrammer(g);
        fprintf(stderr, "garmmer create fail\n");
        fklUninitParserGrammerParseArg(&args);
        exit(1);
    }

    fklUninitParserGrammerParseArg(&args);

    FklGrammerNonterm nonterm = { 0 };
    if (fklCheckAndInitGrammerSymbols(g, &nonterm)) {
        fputs("nonterm: ", stderr);
        fklPrintRawSymbol(FKL_VM_SYM(nonterm.sid), stderr);
        fputs(" is not defined\n", stderr);
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
    fklPrintGrammer(g, stdout);
    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fputc('\n', stdout);
    fputs("item sets:\n", stdout);
    FILE *gzf = fopen("items.gz.txt", "w");
    FILE *lalrgzf = fopen("items-lalr.gz.txt", "w");
    fklPrintItemStateSetAsDot(itemSet, g, gzf);
    fklLr0ToLalrItems(itemSet, g);
    fklPrintItemStateSet(itemSet, g, stdout);
    fklPrintItemStateSetAsDot(itemSet, g, lalrgzf);

    FklStringBuffer err_msg;
    fklInitStringBuffer(&err_msg);
    if (fklGenerateLalrAnalyzeTable(g, itemSet, &err_msg)) {
        fklDestroyVMgc(gc);
        fprintf(stderr, "not lalr garmmer\n");
        fprintf(stderr, "%s\n", err_msg.buf);
        fklUninitStringBuffer(&err_msg);
        exit(1);
    }
    fklPrintAnalysisTable(g, stdout);
    fklLalrItemSetHashMapDestroy(itemSet);
    fklUninitStringBuffer(&err_msg);

    FILE *tablef = fopen("table.txt", "w");
    fklPrintAnalysisTableForGraphEasy(g, tablef);

    fklDestroyGrammer(g);
    fklDestroyVMgc(gc);
    fclose(tablef);
    fclose(gzf);
    fclose(lalrgzf);

    return 0;
}
