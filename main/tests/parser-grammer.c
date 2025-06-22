#include <fakeLisp/grammer.h>
#include <fakeLisp/parser_grammer.h>
#include <string.h>

static const FklGrammerBuiltinAction Actions[] = {{"first", NULL}};

static const FklGrammerBuiltinAction *resolver(void *ctx, const char *str,
                                               size_t len) {
    return &Actions[0];
}

static const char prod_rule[] = {
#include "lisp.g.h"
    '\0',
};

// "*s-exp* -> *list* => first\n"
//     "        -> *hasheq* => first\n"
//     "        -> *symbol* => first\n"
//     "        -> *float* => first\n"
//     "        -> *string* => first\n"
//     "        -> *char* => first\n"
//     "*list* -> \"(\" *list-items* \")\" => second\n"
//     "       -> \"[\" *list-items* \"]\" => second\n"
//     "*hasheq* -> \"#hash(\" *hash-items* \")\" => hasheqv\n"
//     "*list-items* ->                       => nil\n"
//     "             -> *s-exp* *list-items*  => list\n"
//     "             -> *s-exp* \",\" *s-exp* => pair\n"
//     "*string* -> /\"\"|\"(\\\\.|.)*\"/ => string\n"
//     "*symbol* -> ?symbol[\"|\"] => symbol\n"
//     "*float* -> ?s-dfloat[\"|\"] => float\n"
//     "        -> ?s-xfloat[\"|\"] => float\n"
//     "*char* -> ?s-char[\"#\\\\\"] => char\n"
//     "*vector* -> \"#(\" *vector-items* \")\" => second\n"
//     "         -> \"#[\" *vector-items* \"]\" => second\n"
//     "# this is comment\n"
//     "?e -> /\\s+/;\n"
//     "?? -> \"\\\"\";\n";

int main() {
    FklSymbolTable *st = fklCreateSymbolTable();
    FklGrammer *g = fklCreateEmptyGrammer(st);
    FklParserGrammerParseArg args;
    fklInitParserGrammerParseArg(&args, g, 1, resolver, NULL);

    int err = fklParseProductionRuleWithCstr(&args, prod_rule);
    if (err) {
        fklPrintParserGrammerParseError(err, &args, stderr);
        fklDestroySymbolTable(st);
        fklDestroyGrammer(g);
        exit(1);
    }

    if (!g) {
        fklDestroySymbolTable(st);
        fprintf(stderr, "garmmer create fail\n");
        exit(1);
    }

    fklPrintGrammer(stdout, g, st);
    fklUninitParserGrammerParseArg(&args);

    fklDestroySymbolTable(st);
    fklDestroyGrammer(g);

    return 0;
}
