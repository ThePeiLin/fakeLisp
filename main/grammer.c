#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/utils.h>

int main(int argc, char *argv[]) {
    if (argc < 6)
        return 1;

    const char *outer_file_name = argv[1];
    const char *action_file_name = argv[2];
    const char *ast_creator_name = argv[3];
    const char *ast_destroy_name = argv[4];
    const char *state_0_push_name = argv[5];

    FklSymbolTable *st = fklCreateSymbolTable();
    FklGrammer *g = fklCreateBuiltinGrammer(st);
    if (!g) {
        fklDestroySymbolTable(st);
        fprintf(stderr, "garmmer create fail\n");
        exit(1);
    }

    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fklLr0ToLalrItems(itemSet, g);

    FklStringBuffer err_msg;
    fklInitStringBuffer(&err_msg);
    if (fklGenerateLalrAnalyzeTable(g, itemSet, &err_msg)) {
        fklLalrItemSetHashMapDestroy(itemSet);
        fklDestroySymbolTable(st);
        fklDestroyGrammer(g);
        fprintf(stderr, "not lalr garmmer\n");
        fprintf(stderr, "%s\n", err_msg.buf);
        fklUninitStringBuffer(&err_msg);
        return 1;
    }
    fklUninitStringBuffer(&err_msg);

    FILE *action_file = fopen(action_file_name, "r");
    FILE *parse = fopen(outer_file_name, "w");
    fklPrintAnalysisTableAsCfunc(g, st, action_file, ast_creator_name,
                                 ast_destroy_name, state_0_push_name, parse);
    fclose(parse);
    fclose(action_file);

    fklDestroySymbolTable(st);
    fklDestroyGrammer(g);
    fklLalrItemSetHashMapDestroy(itemSet);
    return 0;
}
