#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include <stdio.h>
#include <stdlib.h>

#ifndef ACTION_FILE_PATH
#define ACTION_FILE_PATH "action.c"
#endif

static void *action_nil(void *args,
        void *ctx,
        const FklAnalysisSymbol asts[],
        size_t count,
        size_t line) {
    fprintf(stderr, "[%s: %d] nil!\n", __FILE__, __LINE__);
    return fklVMvalueTerminalCreate("()", strlen("()"), line, ctx);
}

static void *action_not_allow_ignore(void *args,
        void *ctx,
        const FklAnalysisSymbol asts[],
        size_t count,
        size_t line) {
    fprintf(stderr, "[%s: %d] not allow ignore!\n", __FILE__, __LINE__);
    return fklVMvalueTerminalCreate("#()", strlen("#()"), line, ctx);
}

static void *action_start_with_ignore(void *args,
        void *ctx,
        const FklAnalysisSymbol asts[],
        size_t count,
        size_t line) {
    fprintf(stderr, "[%s: %d] start with ignore!\n", __FILE__, __LINE__);
    return fklVMvalueTerminalCreate("# ()", strlen("# ()"), line, ctx);
}

static void *action_first(void *args,
        void *ctx,
        const FklAnalysisSymbol asts[],
        size_t count,
        size_t line) {
    fprintf(stderr, "[%s: %d] first!\n", __FILE__, __LINE__);
    return asts[0].ast;
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
        FklCodeBuilder err_out = { 0 };
        fklInitCodeBuilderFp(&err_out, stderr, NULL);

        fklPrintParserGrammerParseError(err, &args, &err_out);
        fklCodeBuilderPuts(&err_out, "garmmer create fail\n");
        fklUninitParserGrammerParseArg(&args);
        fklDestroyVMgc(gc);
        fklDestroyGrammer(g);
        exit(1);
    }

    fklUninitParserGrammerParseArg(&args);

    FklGrammerNonterm nonterm = { 0 };
    if (fklCheckAndInitGrammerSymbols(g, &nonterm)) {
        fputs("nonterm: ", stderr);
        fklPrintSymbolLiteral(FKL_VM_SYM(nonterm.sid), stderr);
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
    fklPrintGrammer(gc, g, stdout);
    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fputc('\n', stdout);
    fputs("lr0 item sets:\n", stdout);
    fklPrintItemStateSet(gc, itemSet, g, stdout);

    FILE *gzf = fopen("items.gz.txt", "w");
    FILE *lalrgzf = fopen("items-lalr.gz.txt", "w");
    fklPrintItemStateSetAsDot(gc, itemSet, g, gzf);
    fklLr0ToLalrItems(itemSet, g);
    fputs("lalr item set:\n", stdout);
    fklPrintItemStateSet(gc, itemSet, g, stdout);
    fklPrintItemStateSetAsDot(gc, itemSet, g, lalrgzf);

    FklStrBuf err_msg;
    fklInitStrBuf(&err_msg);
    if (fklGenerateLalrAnalyzeTable(gc, g, itemSet, &err_msg)) {
        fklDestroyVMgc(gc);
        fprintf(stderr, "not lalr garmmer\n");
        fprintf(stderr, "%s\n", err_msg.buf);
        fklUninitStrBuf(&err_msg);
        exit(1);
    }
    fklPrintAnalysisTable(g, stdout);
    fklLalrItemSetHashMapDestroy(itemSet);
    fklUninitStrBuf(&err_msg);

    FILE *tablef = fopen("table.txt", "w");
    fklPrintAnalysisTableForGraphEasy(g, tablef);

    const char *output_file_name = "test_lexer5_parse.c";
    const char *action_file_name = ACTION_FILE_PATH;
    const char *ast_creator_name = "fklNastTerminalCreate";
    const char *ast_destroy_name = "fklDestroyNastNode";
    const char *state_0_push_name = "fklNastPushState0ToStack";

    FILE *action_file = fopen(action_file_name, "r");
    FILE *parse = fopen(output_file_name, "w");
    fklPrintAnalysisTableAsCfunc(g,
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

    FklParseError retval = 0;
    FklGrammerMatchCtx ctx = FKL_VMVALUE_PARSE_CTX_INIT(&gc->gcvm, NULL);

    for (const char **exp = &exps[0]; *exp; exp++) {
        FklVMvalue *ast = fklParseWithTableForCstr(g, *exp, &ctx, &retval);

        if (retval) {
            fprintf(stderr, "error: %d\n", retval);
            break;
        }

        fklPrin1VMvalue(ast, stdout, &gc->gcvm);
        fputc('\n', stdout);
    }
    fklDestroyGrammer(g);
    fklDestroyVMgc(gc);

    return 0;
}
