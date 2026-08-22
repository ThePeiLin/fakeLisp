#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include <inttypes.h>

int main(int argc, char *argv[]) {
    if (argc < 7)
        return 1;

    const char *outer_file_name = argv[1];
    const char *action_file_name = argv[2];
    const char *ast_creator_name = argv[3];
    const char *ast_destroy_name = argv[4];
    const char *state_0_push_name = argv[5];
    const char *builtin_terminal_name = argv[6];

    FklVMgc *gc = fklCreateVMgc();
    FklVM *vm = &gc->gcvm;
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

    FILE *action_file = fopen(action_file_name, "r");
    if (action_file == NULL) {
        perror(action_file_name);
        return EXIT_FAILURE;
    }

    FILE *builtin_term_fp = fopen(builtin_terminal_name, "r");
    if (builtin_term_fp == NULL) {
        perror(builtin_terminal_name);
        return EXIT_FAILURE;
    }

    FILE *parse = fopen(outer_file_name, "w");
    if (parse == NULL) {
        perror(outer_file_name);
        return EXIT_FAILURE;
    }

    FklStringVector lines = { 0 };
    fklStringVectorInit(&lines, 32);

    int r = fklLoadLines(&lines, builtin_term_fp);
    if (r < 0) {
        fprintf(stderr, "load builtin terminal source lines failed\n");
        goto exit;
    }

    FklBuiltinTermSrcHashMap maps = { 0 };
    fklBuiltinTermSrcHashMapInit(&maps);

    r = fklParseBuiltinTermSrc(&maps, &lines);
    if (r < 0) {
        fprintf(stderr, "parse builtin terminal source failed\n");
        goto exit;
    }

    r = fklPrintAnalysisTableAsCfunc(g,
            action_file,
            ast_creator_name,
            ast_destroy_name,
            state_0_push_name,
            &maps,
            parse);

exit:
    fklBuiltinTermSrcHashMapUninit(&maps);

    for (size_t i = 0; i < lines.size; ++i) {
        fklZfree(lines.base[i]);
        lines.base[i] = NULL;
    }

    fklStringVectorUninit(&lines);

    fclose(parse);
    fclose(action_file);
    fclose(builtin_term_fp);

    if (r < 0) {
        remove(outer_file_name);
    }

    fklDestroyVMgc(gc);
    fklDestroyGrammer(g);
    fklLalrItemSetHashMapDestroy(itemSet);
    return r;
}
