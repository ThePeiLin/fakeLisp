#ifndef FKL_PARSER_GRAMMER_H
#define FKL_PARSER_GRAMMER_H

#include "grammer.h"
// 解析表示上下文无关文法的表达式
// e.g: *list* -> "(" *list-items* ")" => second
#ifdef __cplusplus
extern "C" {
#endif

#define FKL_PG_TERM_LEFT_ARROW "->"
#define FKL_PG_TERM_REDUCE_TO "=>"
#define FKL_PG_TERM_CONCAT ".."
#define FKL_PG_TERM_END ";"
#define FKL_PG_TERM_COMMA ","

#define FKL_PG_DELIM_STRING "\""
#define FKL_PG_DELIM_REGEX "/"
#define FKL_PG_DELIM_KEYWORD "|"
#define FKL_PG_DELIM_LB "["
#define FKL_PG_DELIM_RB "]"
#define FKL_PG_DELIM_COMMENT "#"
#define FKL_PG_SPECIAL_PREFIX "?"

#define FKL_PG_IGNORE "?e"
#define FKL_PG_TERMINALS "??"

#define FKL_PG_SORTED_DELIMS                                                   \
    X(FKL_PG_TERM_LEFT_ARROW)                                                  \
    X(FKL_PG_TERM_REDUCE_TO)                                                   \
    X(FKL_PG_TERM_CONCAT)                                                      \
    X(FKL_PG_DELIM_STRING)                                                     \
    X(FKL_PG_DELIM_REGEX)                                                      \
    X(FKL_PG_DELIM_KEYWORD)                                                    \
    X(FKL_PG_TERM_END)                                                         \
    X(FKL_PG_TERM_COMMA)                                                       \
    X(FKL_PG_DELIM_LB)                                                         \
    X(FKL_PG_DELIM_RB)                                                         \
    X(FKL_PG_DELIM_COMMENT)

#define FKL_PG_SORTED_OPERATORS                                                \
    X(FKL_PG_TERM_LEFT_ARROW, LEFT_ARROW)                                      \
    X(FKL_PG_TERM_REDUCE_TO, REDUCE_TO)                                        \
    X(FKL_PG_TERM_CONCAT, CONCAT)                                              \
    X(FKL_PG_TERM_END, END)                                                    \
    X(FKL_PG_TERM_COMMA, COMMA)                                                \
    X(FKL_PG_DELIM_LB, LB)                                                     \
    X(FKL_PG_DELIM_RB, RB)

typedef const FklGrammerBuiltinAction *(*FklProdActionResolver)(void *,
                                                                const char *str,
                                                                size_t len);

typedef enum {
    FKL_PARSER_GRAMMER_ADDING_PRODUCTION = 0,
    FKL_PARSER_GRAMMER_ADDING_IGNORE,
    FKL_PARSER_GRAMMER_ADDING_TERMINAL,
} FklParserGrammerState;

typedef struct {
    size_t line;
    size_t errline;
    FklProdActionResolver prod_action_resolver;
    FklGrammer *g;
    FklSid_t current_nonterm;
    FklParserGrammerState state;
    FklStringBuffer error_msg;
    void *resolver_ctx;
    int need_add_extra_prod;
} FklParserGrammerParseArg;

static inline void fklInitParserGrammerParseArg(FklParserGrammerParseArg *arg,
                                                FklGrammer *g,
                                                int need_add_extra_prod,
                                                FklProdActionResolver resolver,
                                                void *resolver_ctx) {
    arg->g = g;
    arg->current_nonterm = 0;
    arg->state = FKL_PARSER_GRAMMER_ADDING_PRODUCTION;
    arg->line = 1;
    arg->errline = 0;
    arg->prod_action_resolver = resolver;
    arg->resolver_ctx = resolver_ctx;
    arg->need_add_extra_prod = need_add_extra_prod;
    fklInitStringBuffer(&arg->error_msg);
}

static inline void
fklUninitParserGrammerParseArg(FklParserGrammerParseArg *arg) {
    arg->g = NULL;
    arg->current_nonterm = 0;
    arg->state = FKL_PARSER_GRAMMER_ADDING_PRODUCTION;
    arg->line = 0;
    arg->errline = 0;
    arg->prod_action_resolver = NULL;
    arg->resolver_ctx = NULL;
    arg->need_add_extra_prod = 0;
    fklUninitStringBuffer(&arg->error_msg);
}

int fklParseProductionRuleWithCharBuf(FklParserGrammerParseArg *arg,
                                      const char *buf, size_t len);

void fklPrintParserGrammerParseError(int err,
                                     const FklParserGrammerParseArg *arg,
                                     FILE *fp);

static inline int
fklParseProductionRuleWithString(FklParserGrammerParseArg *arg,
                                 FklString *str) {
    return fklParseProductionRuleWithCharBuf(arg, str->str, str->size);
}

static inline int fklParseProductionRuleWithCstr(FklParserGrammerParseArg *arg,
                                                 const char *str) {
    return fklParseProductionRuleWithCharBuf(arg, str, strlen(str));
}

#ifdef __cplusplus
}
#endif

#endif
