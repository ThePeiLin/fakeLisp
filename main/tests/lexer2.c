#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/nast.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/utils.h>

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
    FklSymbolTable *st = fklCreateSymbolTable();

    FklParserGrammerParseArg args;
    FklGrammer *g = fklCreateEmptyGrammer(st);

    fklInitParserGrammerParseArg(&args,
            g,
            1,
            builtin_prod_action_resolver,
            NULL);
    int err = fklParseProductionRuleWithCstr(&args, example_grammer_rules);
    if (err) {
        fklPrintParserGrammerParseError(err, &args, stderr);
        fklDestroySymbolTable(st);
        fklDestroyGrammer(g);
        fprintf(stderr, "garmmer create fail\n");
        fklUninitParserGrammerParseArg(&args);
        exit(1);
    }

    fklUninitParserGrammerParseArg(&args);

    FklGrammerNonterm nonterm = { 0 };
    if (fklCheckAndInitGrammerSymbols(g, &nonterm)) {
        fputs("nonterm: ", stderr);
        fklPrintRawSymbol(fklGetSymbolWithId(nonterm.sid, st)->k, stderr);
        fputs(" is not defined\n", stderr);
        fklDestroySymbolTable(st);
        fklDestroyGrammer(g);
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
    fputs("item sets:\n", stdout);
    FILE *gzf = fopen("items.gz.txt", "w");
    FILE *lalrgzf = fopen("items-lalr.gz.txt", "w");
    fklPrintItemStateSetAsDot(itemSet, g, st, gzf);
    fklLr0ToLalrItems(itemSet, g);
    fklPrintItemStateSet(itemSet, g, st, stdout);
    fklPrintItemStateSetAsDot(itemSet, g, st, lalrgzf);

    FklStringBuffer err_msg;
    fklInitStringBuffer(&err_msg);
    if (fklGenerateLalrAnalyzeTable(g, itemSet, &err_msg)) {
        fklDestroySymbolTable(st);
        fprintf(stderr, "not lalr garmmer\n");
        fprintf(stderr, "%s\n", err_msg.buf);
        fklUninitStringBuffer(&err_msg);
        exit(1);
    }
    fklPrintAnalysisTable(g, st, stdout);
    fklLalrItemSetHashMapDestroy(itemSet);
    fklUninitStringBuffer(&err_msg);

    FILE *tablef = fopen("table.txt", "w");
    fklPrintAnalysisTableForGraphEasy(g, st, tablef);

    fklDestroyGrammer(g);
    fklDestroySymbolTable(st);
    fclose(tablef);
    fclose(gzf);
    fclose(lalrgzf);

    return 0;
}
