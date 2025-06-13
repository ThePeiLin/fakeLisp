#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/utils.h>

#ifndef ACTION_FILE_PATH
#define ACTION_FILE_PATH "action.c"
#endif
static const FklGrammerCstrAction test_grammer_and_action[] = {
    // clang-format off
	{"E &E #+ &T",   "test_action", NULL },
	{"E &T",         "test_action", NULL },

	{"T &T #* &F",   "test_action", NULL },
	{"T &F",         "test_action", NULL },

	{"F #( &E #)",   "test_action", NULL },
	{"F &?dfloat",   "test_action", NULL },

    {"+ /\\s+",      NULL,          NULL },
    {"+ /^;.*\\n?",  NULL,          NULL },
    {"+ /^#!.*\\n?", NULL,          NULL },
    {NULL,           NULL,          NULL },
    // clang-format on
};

static const FklGrammerCstrAction test_json_and_action[] = {
    // clang-format off
	{"json-value &null",                           "test_action", NULL },
	{"json-value &bool",                           "test_action", NULL },
	{"json-value &string",                         "test_action", NULL },
	{"json-value &integer",                        "test_action", NULL },
	{"json-value &float",                          "test_action", NULL },
	{"json-value &object",                         "test_action", NULL },
	{"json-value &array",                          "test_action", NULL },

	{"null :null",                                 "test_action", NULL },
	{"bool :true",                                 "test_action", NULL },
	{"bool :false",                                "test_action", NULL },
	{"string /\"\"|^\"(\\\\.|.)*\"$",              "test_action", NULL },
	{"integer &?dint",                             "test_action", NULL },
	{"integer &?xint",                             "test_action", NULL },
	{"integer &?oint",                             "test_action", NULL },

	{"float &?dfloat",                             "test_action", NULL },
	{"float &?xfloat",                             "test_action", NULL },

	{"array #[ &array-items #]",                   "test_action", NULL },
	{"array-items ",                               "test_action", NULL },
	{"array-items &json-value",                    "test_action", NULL },
	{"array-items &json-value #, &array-items",    "test_action", NULL },

	{"object #{ &object-items #}",                 "test_action", NULL },
	{"object-item &string #: &json-value",         "test_action", NULL },
	{"object-items ",                              "test_action", NULL },
	{"object-items &object-item",                  "test_action", NULL },
	{"object-items &object-item #, &object-items", "test_action", NULL },

    {"+ /\\s+",                                    NULL,          NULL },
    {NULL,                                         NULL,          NULL },
    // clang-format on
};

static const FklGrammerCstrAction test_ignore_and_action[]={
    // clang-format off
	{"s ## &item-list",      "test_action", NULL },
	{"s ## &item-list",        "test_action", NULL },

	{"item-list #( &items #)", "test_action", NULL },
	{"items ",                 "test_action", NULL },
	{"items &items &item",     "test_action", NULL },

	{"item &?dint",            "test_action", NULL },

    {"+ /\\s+",                NULL,          NULL },
    {NULL,                     NULL,          NULL },

	{"other &?dint",           "test_action", NULL },
    // clang-format on
};

int main(int argc,const char* argv[]) {
    if (argc > 2)
        return 1;
    const char *grammer_select = argc > 1 ? argv[1] : "builtin";
    const char *output_file_name = "test_lexer3_parse.c";
    const char *action_file_name = ACTION_FILE_PATH;
    const char *ast_creator_name = "fklNastTerminalCreate";
    const char *ast_destroy_name = "fklDestroyNastNode";
    const char *state_0_push_name = "fklNastPushState0ToStack";

    FklSymbolTable *st = fklCreateSymbolTable();
    FklGrammer *g;
    if (!strcmp(grammer_select, "op"))
        g = fklCreateGrammerFromCstrAction(test_grammer_and_action, st);
    else if (!strcmp(grammer_select, "json"))
        g = fklCreateGrammerFromCstrAction(test_json_and_action, st);
    else if (!strcmp(grammer_select, "builtin"))
        g = fklCreateBuiltinGrammer(st);
    else if (!strcmp(grammer_select, "ignore"))
        g = fklCreateGrammerFromCstrAction(test_ignore_and_action, st);
    else {
        fklDestroySymbolTable(st);
        fprintf(stderr, "invalid selection\n");
        exit(1);
    }

    if (!g) {
        fklDestroySymbolTable(st);
        fprintf(stderr, "garmmer create fail\n");
        exit(1);
    }

    fklPrintGrammer(stdout, g, st);
    fputs("\n===\n\n", stdout);
    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    printf("lr0 item set:\n");
    fklPrintItemStateSet(itemSet, g, st, stdout);
    fputc('\n', stdout);

    fklLr0ToLalrItems(itemSet, g);
    fprintf(stdout, "lalr item set:\n");
    fklPrintItemStateSet(itemSet, g, st, stdout);

    if (fklGenerateLalrAnalyzeTable(g, itemSet)) {
        fklLalrItemSetHashMapDestroy(itemSet);
        fklDestroySymbolTable(st);
        fklDestroyGrammer(g);
        fprintf(stderr, "not lalr garmmer\n");
        return 1;
    }

    FILE *action_file = fopen(action_file_name, "r");
    FILE *parse = fopen(output_file_name, "w");
    fklPrintAnalysisTableAsCfunc(g, st, action_file, ast_creator_name,
                                 ast_destroy_name, state_0_push_name, parse);
    fclose(parse);
    fclose(action_file);

    fklDestroySymbolTable(st);
    fklDestroyGrammer(g);
    fklLalrItemSetHashMapDestroy(itemSet);
    return 0;
}
