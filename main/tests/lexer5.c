#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/nast.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/utils.h>

#include <stdio.h>
#include <stdlib.h>

#ifndef ACTION_FILE_PATH
#define ACTION_FILE_PATH "action.c"
#endif

static void *
fklNastTerminalCreate(const char *s, size_t len, size_t line, void *ctx) {
    FklNastNode *ast = fklCreateNastNode(FKL_NAST_STR, line);
    ast->str = fklCreateString(len, s);
    return ast;
}

static void *action_nil(void *args,
        void *ctx,
        const FklAnalysisSymbol asts[],
        size_t count,
        size_t line) {
    fprintf(stderr, "[%s: %d] nil!\n", __FILE__, __LINE__);
    return fklNastTerminalCreate("()", strlen("()"), line, ctx);
}

static void *action_not_allow_ignore(void *args,
        void *ctx,
        const FklAnalysisSymbol asts[],
        size_t count,
        size_t line) {
    fprintf(stderr, "[%s: %d] not allow ignore!\n", __FILE__, __LINE__);
    return fklNastTerminalCreate("#()", strlen("#()"), line, ctx);
}

static void *action_start_with_ignore(void *args,
        void *ctx,
        const FklAnalysisSymbol asts[],
        size_t count,
        size_t line) {
    fprintf(stderr, "[%s: %d] start with ignore!\n", __FILE__, __LINE__);
    return fklNastTerminalCreate("# ()", strlen("# ()"), line, ctx);
}

static void *action_first(void *args,
        void *ctx,
        const FklAnalysisSymbol asts[],
        size_t count,
        size_t line) {
    fprintf(stderr, "[%s: %d] first!\n", __FILE__, __LINE__);
    return fklMakeNastNodeRef(asts[0].ast);
}

static const FklGrammerBuiltinAction builtin_actions[] = {
    { "not_allow_ignore", action_not_allow_ignore },
    { "start_with_ignore", action_start_with_ignore },
    { "first", action_first },
    { "nil", action_nil },
};

static inline const FklGrammerBuiltinAction *
builtin_prod_action_resolver(void *ctx, const char *str, size_t len) {
    for (const FklGrammerBuiltinAction *cur = &builtin_actions[0]; cur->name;
            ++cur) {
        size_t cur_len = strlen(cur->name);
        if (cur_len == len && memcmp(cur->name, str, cur_len) == 0)
            return cur;
    }
    return NULL;
}

static char example_grammer_rules[] = //
        ""
        "C -> A => first\n"
        "A -> \"#\" .. B => not_allow_ignore\n"
        "A -> \"#\" B => start_with_ignore\n"
        "B -> \"(\" \")\" => nil\n"
        "?e -> /\\s+/\n"
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
        fklPrintRawSymbol(fklGetSymbolWithId(nonterm.sid, st), stderr);
        fputs(" is not defined\n", stderr);
        fklDestroySymbolTable(st);
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
    fklPrintGrammer(stdout, g, st);
    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fputc('\n', stdout);
    fputs("lr0 item sets:\n", stdout);
    fklPrintItemStateSet(itemSet, g, st, stdout);

    FILE *gzf = fopen("items.gz.txt", "w");
    FILE *lalrgzf = fopen("items-lalr.gz.txt", "w");
    fklPrintItemStateSetAsDot(itemSet, g, st, gzf);
    fklLr0ToLalrItems(itemSet, g);
    fputs("lalr item set:\n", stdout);
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

    const char *output_file_name = "test_lexer5_parse.c";
    const char *action_file_name = ACTION_FILE_PATH;
    const char *ast_creator_name = "fklNastTerminalCreate";
    const char *ast_destroy_name = "fklDestroyNastNode";
    const char *state_0_push_name = "fklNastPushState0ToStack";

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

    fclose(tablef);
    fclose(gzf);
    fclose(lalrgzf);

    fputc('\n', stdout);

    const char *exps[] = {
        "#()",
        "# ()",
        NULL,
    };

    int retval = 0;
    FklGrammerMatchOuterCtx outerCtx = {
        .maxNonterminalLen = 0,
        .line = 1,
        .start = NULL,
        .cur = NULL,
        .create = fklNastTerminalCreate,
        .destroy = (void (*)(void *))fklDestroyNastNode,
        .ctx = st,
    };

    for (const char **exp = &exps[0]; *exp; exp++) {
        FklNastNode *ast =
                fklParseWithTableForCstr(g, *exp, &outerCtx, st, &retval);

        if (retval) {
            fprintf(stderr, "error: %d\n", retval);
            break;
        }

        fklPrintNastNode(ast, stdout, st);
        fklDestroyNastNode(ast);
        fputc('\n', stdout);
    }
    fklDestroyGrammer(g);
    fklDestroySymbolTable(st);

    return 0;
}
