#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/utils.h>

#include <stdio.h>
#include <stdlib.h>

int main() {
    FklSymbolTable *st = fklCreateSymbolTable();

    fputc('\n', stdout);

    const char *exps[] = {
        // clang-format off
        "#\\\\11",
        "#\\\\z",
        "#\\\\n",
        "#\\\\",
        "#\\;",
        "#\\|",
        "#\\\"",
        "#\\(",
        "#\\\\s",
        "(abcd)abcd",
        ";comments\nabcd",
        "foobar|foo|foobar|bar|",
        "(\"foo\" \"bar\" \"foobar\",;abcd\n\"i\")",
        "[(foobar;comments\nfoo bar),abcd]",
        "(foo bar abcd|foo \\|bar|efgh foo \"foo\\\"\",bar)",
        "#hash((a,1) (b,2))",
        "#\"\\114\\51\\11\\45\\14\"",
        "114514",
        "#\\ ",
        "'#&#(foo 0x114514 \"foobar\" .1 0x1p1 114514|foo|bar #\\a #\\\\0 #\\\\x11 #\\\\0123 #\\\\0177 #\\\\0777)",
        "\"foobar\"",
        "114514",
        "\"foobar\"",
        "(define (i x)\n(when (not (eq 0 x)) (princ \"Hello,world!\n\") (i (-1+ x))))",
        NULL,
        // clang-format on
    };

    int retval = 0;
    for (const char **exp = &exps[0]; *exp; exp++) {
        FklGrammerMatchOuterCtx outerCtx = FKL_NAST_PARSE_OUTER_CTX_INIT(st);

        FklParseStateVector stateStack;
        FklAnalysisSymbolVector symbolStack;
        FklUintVector lineStack;

        fklParseStateVectorInit(&stateStack, 8);
        fklNastPushState0ToStack(&stateStack);
        fklAnalysisSymbolVectorInit(&symbolStack, 8);
        fklUintVectorInit(&lineStack, 8);
        size_t errLine = 0;
        FklNastNode *ast =
            fklDefaultParseForCstr(*exp, &outerCtx, &retval, &errLine,
                                   &symbolStack, &lineStack, &stateStack);

        while (!fklAnalysisSymbolVectorIsEmpty(&symbolStack))
            free(*fklAnalysisSymbolVectorPopBack(&symbolStack));
        fklAnalysisSymbolVectorUninit(&symbolStack);
        fklParseStateVectorUninit(&stateStack);
        fklUintVectorUninit(&lineStack);
        if (retval)
            break;

        fklPrintNastNode(ast, stdout, st);
        fklDestroyNastNode(ast);

        fputc('\n', stdout);
    }
    fklDestroySymbolTable(st);
    return retval;
}
