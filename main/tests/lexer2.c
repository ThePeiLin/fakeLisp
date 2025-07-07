#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/nast.h>
#include <fakeLisp/utils.h>

#include <stdio.h>
#include <stdlib.h>

static const FklGrammerCstrAction example_grammer_action[] = {
    // clang-format off
    {"S &L #= &R",     NULL, NULL },
    {"S &R",           NULL, NULL },
    {"L #* &R",        NULL, NULL },
    {"L &?identifier", NULL, NULL },
    {"R &L",           NULL, NULL },
    {NULL,             NULL, NULL },
    // clang-format on
};

int main() {
    FklSymbolTable *st = fklCreateSymbolTable();
    // FklGrammer* g=fklCreateGrammerFromCstrAction(example_grammer,st);
    FklGrammer *g = fklCreateGrammerFromCstrAction(example_grammer_action, st);
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
