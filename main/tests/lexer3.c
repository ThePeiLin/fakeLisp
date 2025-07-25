#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/utils.h>

#ifndef ACTION_FILE_PATH
#define ACTION_FILE_PATH "action.c"
#endif

static const char test_op_grammer_rules[] = //
        ""
        "E -> E \"+\" T      => test\n"
        "  -> T              => test\n"

        "T -> T \"*\" F      => test\n"
        "  -> F              => test\n"

        "F -> \"(\" E \")\"  => test\n"
        "  -> \"(\" E \")\"  => test\n"

        "?e -> /#!.*\\n?/    \n"
        "   -> /;.*\\n?/     \n"
        "   -> /\\s+/        \n"
        "   ;\n"
        "?? -> \"#!\"        \n"
        "   -> \";\"         \n"
        "";

static const char test_json_grammer_rules[] = //
        ""
        "json-value -> null    => test  \n"
        "           -> bool    => test  \n"
        "           -> string  => test  \n"
        "           -> integer => test  \n"
        "           -> float   => test  \n"
        "           -> object  => test  \n"
        "           -> array   => test  \n"
        "null -> |null| => test \n"
        "bool -> |true|  => test \n"
        "     -> |false| => test \n"
        "string -> /\"\"|\"(\\\\.|.)*\"/ => test \n"
        "integer -> ?dint => test \n"
        "        -> ?xint => test \n"
        "        -> ?oint => test \n"
        "float -> ?dfloat => test \n"
        "      -> ?xfloat => test \n"
        "array -> \"[\" array-items \"]\" => test \n"
        "array-items ->                              => test \n"
        "            -> json-value                   => test \n"
        "            -> json-value \",\" array-items => test \n"
        "object -> \"{\" object-items \"}\" => test \n"
        "object-item -> string \":\" json-value => test \n"
        "object-items ->                                => test \n"
        "             -> object-item                    => test \n"
        "             -> object-item \",\" object-items => test \n"
        "?e -> /\\s+/  \n"
        "   ;\n"
        "?? -> \"\\\"\"  \n"
        "";

static const char test_ignore_grammer_rules[] = //
        ""
        "s -> \"#\" item-list => test \n"
        "s -> \"#\" .. item-list => test \n"
        "s -> \"#\" .. vector-list => test \n"
        "s -> \"#\" vector-list => test \n"
        "vector-list -> \"[\" items \"]\" => test\n"
        "item-list -> \"(\" items \")\" => test \n"
        "items ->            => test \n"
        "      -> items item => test \n"
        "item  -> ?dint => test \n"
        "?e -> /\\s+/; \n"
        // "other -> ?dint \n"
        "";

static inline const FklGrammerBuiltinAction *
test_prod_action_resolver(void *ctx, const char *str, size_t len) {
    static FklGrammerBuiltinAction action = { "test", NULL };
    return &action;
}

static inline FklGrammer *create_grammer_with_cstr(FklSymbolTable *st,
        const char *rules) {
    FklParserGrammerParseArg args;
    FklGrammer *g = fklCreateEmptyGrammer(st);

    fklInitParserGrammerParseArg(&args, g, 1, test_prod_action_resolver, NULL);
    int err = fklParseProductionRuleWithCstr(&args, rules);
    if (err) {
        fklPrintParserGrammerParseError(err, &args, stderr);
        fklDestroyGrammer(g);
        fprintf(stderr, "garmmer create fail\n");
        fklUninitParserGrammerParseArg(&args);
        return NULL;
    }

    fklUninitParserGrammerParseArg(&args);

    FklGrammerNonterm nonterm = { 0 };
    if (fklCheckAndInitGrammerSymbols(g, &nonterm)) {
        fputs("nonterm: ", stderr);
        fklPrintRawSymbol(fklGetSymbolWithId(nonterm.sid, st)->k, stderr);
        fputs(" is not defined\n", stderr);
        fklDestroyGrammer(g);
        return NULL;
    }
    return g;
}

int main(int argc, const char *argv[]) {
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
    if (!strcmp(grammer_select, "op")) {
        g = create_grammer_with_cstr(st, test_op_grammer_rules);
    } else if (!strcmp(grammer_select, "json"))
        g = create_grammer_with_cstr(st, test_json_grammer_rules);
    else if (!strcmp(grammer_select, "builtin"))
        g = fklCreateBuiltinGrammer(st);
    else if (!strcmp(grammer_select, "ignore"))
        g = create_grammer_with_cstr(st, test_ignore_grammer_rules);
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
    FILE *parse = fopen(output_file_name, "w");
    fklPrintAnalysisTableAsCfunc(g,
            st,
            action_file,
            ast_creator_name,
            ast_destroy_name,
            state_0_push_name,
            parse);
    fclose(parse);
    fclose(action_file);

    fklDestroySymbolTable(st);
    fklDestroyGrammer(g);
    fklLalrItemSetHashMapDestroy(itemSet);
    return 0;
}
