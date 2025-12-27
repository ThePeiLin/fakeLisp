#include <fakeLisp/base.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include <stdio.h>
#include <stdlib.h>

int main() {
    FklVMgc *gc = fklCreateVMgc(fklCreateVMobarray());
    FklVM *vm = &gc->gcvm;

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

    FklParseError retval = 0;
    for (const char **exp = &exps[0]; *exp; exp++) {
        FklGrammerMatchCtx ctx = FKL_VMVALUE_PARSE_CTX_INIT(vm, NULL);

        FklParseStateVector stateStack;
        FklAnalysisSymbolVector symbolStack;

        fklParseStateVectorInit(&stateStack, 8);
        fklVMvaluePushState0ToStack(&stateStack);
        fklAnalysisSymbolVectorInit(&symbolStack, 8);
        size_t errLine = 0;
        FklVMvalue *ast = fklDefaultParseForCstr(*exp,
                &ctx,
                &retval,
                &errLine,
                &symbolStack,
                &stateStack);

        fklAnalysisSymbolVectorUninit(&symbolStack);
        fklParseStateVectorUninit(&stateStack);
        FKL_ASSERT(ast);
        if (retval)
            break;

        fklPrin1VMvalue(ast, stdout, vm);

        fputc('\n', stdout);
    }
    fklDestroyVMgc(gc);
    return retval;
}
