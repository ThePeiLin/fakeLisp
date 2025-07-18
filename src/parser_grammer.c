#include <fakeLisp/base.h>
#include <fakeLisp/common.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/zmalloc.h>

#include <ctype.h>
#include <stddef.h>
#include <string.h>

static struct DelimMatching {
    const char *s;
    ssize_t l;
} SortedTerminals[] = {
#define X(T) { T, sizeof(T) - 1 },
    FKL_PG_SORTED_DELIMS
#undef X
    { NULL, 0 },
};

typedef enum TokenType {
    TOKEN_NONE,
    TOKEN_LEFT_ARROW,
    TOKEN_REDUCE_TO,
    TOKEN_CONCAT,
    TOKEN_LB,
    TOKEN_RB,
    TOKEN_END,
    TOKEN_COMMA,

    TOKEN_IDENTIFIER,
    TOKEN_TERM_REGEX,
    TOKEN_TERM_STRING,
    TOKEN_TERM_KEYWORD,
} TokenType;

static struct OperatorMatching {
    const char *s;
    ssize_t l;
    TokenType t;
} SortedOperators[] = {
#define X(T, Y) { T, sizeof(T) - 1, TOKEN_##Y },
    FKL_PG_SORTED_OPERATORS
#undef X
    { NULL, 0, TOKEN_NONE },
};

typedef struct Token {
    TokenType type;
    size_t line;
    size_t len;
    const char *str;
} Token;

#define MATCH_DELIM(buf, end, DELIM)                                           \
    ((size_t)fklCharBufMatch(DELIM, strlen(DELIM), buf, end - buf) == strlen(DELIM))

static inline int is_comment(const char *buf, const char *const end) {
    return MATCH_DELIM(buf, end, FKL_PG_DELIM_COMMENT);
}

static inline const char *skip_comment(const char *buf, const char *const end) {
    for (; buf < end; ++buf) {
        if (*buf == '\n') {
            ++buf;
            break;
        }
    }
    return buf;
}

static inline const char *
skip_ignore(size_t *line, const char *buf, const char *const end) {
    const char *start = buf;
    while (buf < end) {
        if (isspace(*buf))
            ++buf;
        else if (is_comment(buf, end)) {
            buf = skip_comment(buf, end);
        } else
            break;
    }
    (*line) += fklCountCharInBuf(start, buf - start, '\n');
    return buf;
}

#define ERR_UNEXPECTED_EOF (1)
#define ERR_UNEXPECTED_TOKEN (2)
#define ERR_UNRESOLVED_ACTION_NAME (3)
#define ERR_ADDING_PRODUCTION (4)
#define ERR_ADDING_IGNORE (5)
#define ERR_INVALID_LEFT_PART (6)
#define ERR_UNRESOLVED_BUILTIN_TERMINAL (7)
#define ERR_BUILTIN_TERMINAL_INIT_FAILED (8)

static inline int
get_string_token(Token *t, const char *buf, const char *const end) {
    const char *start = buf++;
    size_t len = 1;
    while (buf < end) {
        if (*buf == '\\') {
            if (buf < end - 1) {
                buf += 2;
                len += 2;
            } else {
                break;
            }
        } else {
            ++len;
            if (MATCH_DELIM(buf, end, FKL_PG_DELIM_STRING))
                goto create_token;
            ++buf;
        }
    }

    return ERR_UNEXPECTED_EOF;

create_token:

    t->type = TOKEN_TERM_STRING;
    t->len = len;
    t->str = start;
    return 0;
}

static inline int
get_keyword_token(Token *t, const char *buf, const char *const end) {
    const char *start = buf++;
    size_t len = 1;
    while (buf < end) {
        if (*buf == '\\') {
            if (buf < end - 1) {
                buf += 2;
                len += 2;
            } else {
                break;
            }
        } else {
            ++len;
            if (MATCH_DELIM(buf, end, FKL_PG_DELIM_KEYWORD))
                goto create_token;
            ++buf;
        }
    }

    return ERR_UNEXPECTED_EOF;

create_token:

    t->type = TOKEN_TERM_KEYWORD;
    t->len = len;
    t->str = start;
    return 0;
}

static inline int
get_regex_token(Token *t, const char *buf, const char *const end) {
    const char *start = buf++;
    size_t len = 1;
    while (buf < end) {
        if (*buf == '\\') {
            if (buf < end - 1) {
                buf += 2;
                len += 2;
            } else {
                break;
            }
        } else {
            ++len;
            if (MATCH_DELIM(buf, end, FKL_PG_DELIM_REGEX))
                goto create_token;
            ++buf;
        }
    }

    return ERR_UNEXPECTED_EOF;

create_token:

    t->type = TOKEN_TERM_REGEX;
    t->len = len;
    t->str = start;
    return 0;
}

static inline int match_delims(const char *buf, const char *const end) {
    for (struct DelimMatching *cur = &SortedTerminals[0]; cur->l; ++cur) {
        if (fklCharBufMatch(cur->s, cur->l, buf, end - buf) == cur->l) {
            return 1;
        }
    }
    return 0;
}

static inline void
get_identifier_token(Token *t, const char *buf, const char *const end) {
    const char *start = buf;
    size_t len = 0;
    while (buf < end) {
        if (isspace(*buf))
            break;
        else if (match_delims(buf, end))
            break;
        ++len;
        ++buf;
    }

    t->type = TOKEN_IDENTIFIER;
    t->len = len;
    t->str = start;
}

static inline Token next_token(FklParserGrammerParseArg *arg,
        int *err,
        const char **pbuf,
        const char *const end) {
    *err = 0;
    Token t = { .type = TOKEN_NONE };
    const char *buf = *pbuf;
    if (buf >= end)
        return t;
    buf = skip_ignore(&arg->line, buf, end);
    t.line = arg->line;
    for (struct OperatorMatching *cur = &SortedOperators[0]; cur->l; ++cur) {
        if (fklCharBufMatch(cur->s, cur->l, buf, end - buf) == cur->l) {
            t.type = cur->t;
            t.len = cur->l;
            t.str = buf;
            buf += cur->l;
            *pbuf = buf;
            return t;
        }
    }

    if (MATCH_DELIM(buf, end, FKL_PG_DELIM_STRING)) {
        *err = get_string_token(&t, buf, end);
    } else if (MATCH_DELIM(buf, end, FKL_PG_DELIM_KEYWORD)) {
        *err = get_keyword_token(&t, buf, end);
    } else if (MATCH_DELIM(buf, end, FKL_PG_DELIM_REGEX)) {
        *err = get_regex_token(&t, buf, end);
    } else if (buf < end) {
        get_identifier_token(&t, buf, end);
    }

    arg->line += fklCountCharInBuf(buf, t.len, '\n');
    buf += t.len;
    *pbuf = buf;
    return t;
}

static inline void unexpected_token_error(FklParserGrammerParseArg *arg,
        const Token *t) {

    arg->errline = t->line;
    FklStringBuffer *buf = &arg->error_msg;
    fklStringBufferPrintf(buf, "unexcpet token: ");
    fklStringBufferBincpy(buf, t->str, t->len);
}

static inline void unresolved_action_name_error(FklParserGrammerParseArg *arg,
        const Token *t) {
    arg->errline = t->line;
    FklStringBuffer *buf = &arg->error_msg;
    fklStringBufferPrintf(buf, "unresolved action name: ");
    fklStringBufferBincpy(buf, t->str, t->len);
}

static inline void unresolved_builtin_terminal_error(
        FklParserGrammerParseArg *arg,
        const Token *t) {
    arg->errline = t->line;
    FklStringBuffer *buf = &arg->error_msg;
    fklStringBufferPrintf(buf, "unresolved builtin terminal: ");
    fklStringBufferBincpy(buf, t->str, t->len);
}

static inline void invalid_left_part_error(FklParserGrammerParseArg *arg,
        const Token *t) {
    arg->errline = t->line;
    FklStringBuffer *buf = &arg->error_msg;
    fklStringBufferPrintf(buf, "invalid production rule left part: ");
    fklStringBufferBincpy(buf, t->str, t->len);
}

static inline void builtin_terminal_init_failed_error(
        FklBuiltinTerminalInitError err,
        FklParserGrammerParseArg *arg,
        const Token *t) {
    arg->errline = t->line;
    FklStringBuffer *buf = &arg->error_msg;
    fklStringBufferPrintf(buf, "failed to init builtin terminal: ");
    fklStringBufferBincpy(buf, t->str, t->len);
    fklStringBufferPrintf(buf, ", %s", fklBuiltinTerminalInitErrorToCstr(err));
}

static inline const char *parse_adding_terminal(FklParserGrammerParseArg *arg,
        int *err,
        const char *buf,
        const char *const end) {
    Token token = next_token(arg, err, &buf, end);
    if (*err)
        return NULL;
    if (token.type != TOKEN_LEFT_ARROW) {
        *err = ERR_UNEXPECTED_TOKEN;
        unexpected_token_error(arg, &token);
        return NULL;
    }

    token = next_token(arg, err, &buf, end);
    if (token.type == TOKEN_TERM_STRING) {
        size_t const start_size = sizeof(FKL_PG_DELIM_STRING) - 1;
        size_t const end_size = start_size;
        size_t len = 0;
        char *str = fklCastEscapeCharBuf(&token.str[start_size],
                token.len - end_size - start_size,
                &len);
        fklAddStringCharBuf(&arg->g->terminals, str, len);
        fklAddStringCharBuf(&arg->g->delimiters, str, len);
        fklZfree(str);
    } else if (token.type == TOKEN_NONE) {
        *err = ERR_UNEXPECTED_EOF;
        return NULL;
    } else {
        *err = ERR_UNEXPECTED_TOKEN;
        unexpected_token_error(arg, &token);
        return NULL;
    }

    return buf;
}

static inline const char *parse_ignore(FklParserGrammerParseArg *arg,
        int *err,
        const char *buf,
        const char *const end) {
    FklStringTable *terminals = &arg->g->terminals;
    FklRegexTable *regexes = &arg->g->regexes;
    Token token = next_token(arg, err, &buf, end);
    if (*err)
        return NULL;
    if (token.type != TOKEN_LEFT_ARROW) {
        *err = ERR_UNEXPECTED_TOKEN;
        unexpected_token_error(arg, &token);
        return NULL;
    }

    FklGraSymVector gsym_vector;
    fklGraSymVectorInit(&gsym_vector, 2);

    for (; buf < end;) {
        FklGrammerSym s = { .type = FKL_TERM_STRING };
        Token token = next_token(arg, err, &buf, end);
        if (*err)
            goto error_happened;

        switch (token.type) {
        case TOKEN_NONE:
            continue;
            break;
        case TOKEN_IDENTIFIER: {
            if (MATCH_DELIM(token.str, token.str + token.len, FKL_PG_IGNORE)) {
                *err = ERR_UNEXPECTED_TOKEN;
                unexpected_token_error(arg, &token);
                goto error_happened;
            } else if (MATCH_DELIM(token.str,
                               token.str + token.len,
                               FKL_PG_TERMINALS)) {
                *err = ERR_UNEXPECTED_TOKEN;
                unexpected_token_error(arg, &token);
                goto error_happened;
            }

            if (MATCH_DELIM(token.str,
                        token.str + token.len,
                        FKL_PG_SPECIAL_PREFIX)) {
                FKL_UNREACHABLE();
            } else {
                *err = ERR_UNEXPECTED_TOKEN;
                unexpected_token_error(arg, &token);
                goto error_happened;
            }
        } break;

        case TOKEN_TERM_STRING: {
            size_t const start_size = sizeof(FKL_PG_DELIM_STRING) - 1;
            size_t const end_size = start_size;
            size_t len = 0;
            char *str = fklCastEscapeCharBuf(&token.str[start_size],
                    token.len - end_size - start_size,
                    &len);
            s.str = fklAddStringCharBuf(terminals, str, len);
            fklAddStringCharBuf(&arg->g->delimiters, str, len);
            fklZfree(str);
        } break;

        case TOKEN_TERM_REGEX: {
            size_t const start_size = sizeof(FKL_PG_DELIM_REGEX) - 1;
            size_t const end_size = start_size;
            s.type = FKL_TERM_REGEX;
            const FklRegexCode *re = fklAddRegexCharBuf(regexes,
                    &token.str[start_size],
                    token.len - end_size - start_size);
            s.re = re;
        } break;

        case TOKEN_END:
        case TOKEN_LEFT_ARROW: {
            buf -= token.len;
            goto loop_break;
        } break;

        case TOKEN_COMMA:
        case TOKEN_TERM_KEYWORD:
        case TOKEN_REDUCE_TO:
        case TOKEN_CONCAT:
        case TOKEN_LB:
        case TOKEN_RB: {
            *err = ERR_UNEXPECTED_TOKEN;
            unexpected_token_error(arg, &token);
            goto error_happened;
        } break;
        }

        fklGraSymVectorPushBack(&gsym_vector, &s);
    }

loop_break:;
    FklGrammerIgnore *ig =
            fklGrammerSymbolsToIgnore(gsym_vector.base, gsym_vector.size);

    if (fklAddIgnoreToIgnoreList(&arg->g->ignores, ig)) {
        fklDestroyIgnore(ig);
        *err = ERR_ADDING_IGNORE;
        goto error_happened;
    }

    fklGraSymVectorUninit(&gsym_vector);
    return buf;

error_happened:
    while (!fklGraSymVectorIsEmpty(&gsym_vector)) {
        FklGrammerSym *s = fklGraSymVectorPopBack(&gsym_vector);

        if (s->type == FKL_TERM_BUILTIN && s->b.len) {
            s->b.len = 0;
            fklZfree(s->b.args);
            s->b.args = NULL;
        }
    }

    fklGraSymVectorUninit(&gsym_vector);

    return NULL;
}

static inline const char *parse_builtin_args(FklGrammerSym *s,
        FklParserGrammerParseArg *arg,
        int *err,
        const char *buf,
        const char *const end) {
    Token token = next_token(arg, err, &buf, end);
    if (token.type != TOKEN_LB) {
        *err = ERR_UNEXPECTED_TOKEN;
        unexpected_token_error(arg, &token);
        return NULL;
    }
    token = next_token(arg, err, &buf, end);
    if (token.type == TOKEN_RB) {
        s->b.len = 0;
        s->b.args = NULL;
        return buf;
    }

    FklStringVector str_vec;
    fklStringVectorInit(&str_vec, 2);
    while (buf < end) {
        if (token.type != TOKEN_TERM_STRING) {
            *err = ERR_UNEXPECTED_TOKEN;
            unexpected_token_error(arg, &token);
            goto error_happened;
        }

        size_t const start_size = sizeof(FKL_PG_DELIM_STRING) - 1;
        size_t const end_size = start_size;
        size_t len = 0;
        char *str = fklCastEscapeCharBuf(&token.str[start_size],
                token.len - end_size - start_size,
                &len);
        FklString const *arg_str =
                fklAddStringCharBuf(&arg->g->terminals, str, len);
        fklStringVectorPushBack2(&str_vec,
                FKL_REMOVE_CONST(FklString, arg_str));
        fklZfree(str);

        token = next_token(arg, err, &buf, end);
        if (*err) {
            goto error_happened;
        }

        if (token.type == TOKEN_RB)
            break;
        else if (token.type == TOKEN_NONE)
            break;
        else if (token.type != TOKEN_COMMA) {
            *err = ERR_UNEXPECTED_TOKEN;
            unexpected_token_error(arg, &token);
            goto error_happened;
        }

        token = next_token(arg, err, &buf, end);
        if (*err)
            goto error_happened;
    }

    if (token.type != TOKEN_RB) {
        *err = ERR_UNEXPECTED_TOKEN;
        unexpected_token_error(arg, &token);
        goto error_happened;
    }

    FklString const **args = fklZmalloc(str_vec.size * sizeof(FklString *));
    for (size_t i = 0; i < str_vec.size; ++i) {
        args[i] = str_vec.base[i];
    }

    s->b.len = str_vec.size;
    s->b.args = args;

    fklStringVectorUninit(&str_vec);
    return buf;

error_happened:
    fklStringVectorUninit(&str_vec);
    return NULL;
}

static inline const char *parse_right_part(FklParserGrammerParseArg *arg,
        int *err,
        const char *buf,
        const char *const end) {
    FklStringTable *terminals = &arg->g->terminals;
    FklRegexTable *regexes = &arg->g->regexes;
    Token token = next_token(arg, err, &buf, end);
    if (*err)
        return NULL;
    if (token.type != TOKEN_LEFT_ARROW) {
        *err = ERR_UNEXPECTED_TOKEN;
        unexpected_token_error(arg, &token);
        return NULL;
    }

    FklGraSymVector gsym_vector;
    fklGraSymVectorInit(&gsym_vector, 2);

    int has_ignore = 0;

    const char *action_name = NULL;
    FklProdActionFunc action_func = NULL;
    for (; buf < end;) {
        FklGrammerSym s = { .type = FKL_TERM_STRING };
        Token token = next_token(arg, err, &buf, end);
        if (*err)
            goto error_happened;

        switch (token.type) {
        case TOKEN_NONE:
            continue;
            break;
        case TOKEN_CONCAT:
            if (!has_ignore) {
                *err = ERR_UNEXPECTED_TOKEN;
                unexpected_token_error(arg, &token);
                goto error_happened;
            } else
                has_ignore = 0;
            continue;
            break;

        case TOKEN_IDENTIFIER: {
            if (MATCH_DELIM(token.str, token.str + token.len, FKL_PG_IGNORE)) {
                *err = ERR_UNEXPECTED_TOKEN;
                unexpected_token_error(arg, &token);
                goto error_happened;
            } else if (MATCH_DELIM(token.str,
                               token.str + token.len,
                               FKL_PG_TERMINALS)) {
                *err = ERR_UNEXPECTED_TOKEN;
                unexpected_token_error(arg, &token);
                goto error_happened;
            }

            if (MATCH_DELIM(token.str,
                        token.str + token.len,
                        FKL_PG_SPECIAL_PREFIX)) {
                Token ntoken = next_token(arg, err, &buf, end);
                buf -= ntoken.len;
                s.type = FKL_TERM_BUILTIN;
                FklSid_t id =
                        fklAddSymbolCharBuf(token.str, token.len, arg->g->st)
                                ->v;
                const FklLalrBuiltinMatch *builtin =
                        fklGetBuiltinMatch(&arg->g->builtins, id);
                if (builtin) {
                    s.b.t = builtin;

                    if (ntoken.type == TOKEN_LB) {
                        buf = parse_builtin_args(&s, arg, err, buf, end);
                        if (*err)
                            goto error_happened;
                    } else {
                        s.b.len = 0;
                        s.b.args = NULL;
                    }

                    if (s.b.t->ctx_create) {
                        FklBuiltinTerminalInitError init_err =
                                FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY;
                        init_err = s.b.t->ctx_create(s.b.len, s.b.args, arg->g);
                        if (init_err) {
                            *err = ERR_BUILTIN_TERMINAL_INIT_FAILED;
                            builtin_terminal_init_failed_error(init_err,
                                    arg,
                                    &token);
                            s.b.len = 0;
                            fklZfree(s.b.args);
                            s.b.args = NULL;
                            goto error_happened;
                        }
                    } else if (s.b.len != 0) {
                        *err = ERR_BUILTIN_TERMINAL_INIT_FAILED;
                        builtin_terminal_init_failed_error(
                                FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS,
                                arg,
                                &token);
                        s.b.len = 0;
                        fklZfree(s.b.args);
                        s.b.args = NULL;
                        goto error_happened;
                    }

                    for (size_t i = 0; i < s.b.len; ++i) {
                        fklAddString(&arg->g->delimiters, s.b.args[i]);
                    }
                } else {
                    *err = ERR_UNRESOLVED_BUILTIN_TERMINAL;
                    unresolved_builtin_terminal_error(arg, &token);
                    goto error_happened;
                }
            } else {
                FklSid_t id =
                        fklAddSymbolCharBuf(token.str, token.len, arg->g->st)
                                ->v;
                s.type = FKL_TERM_NONTERM;
                s.nt.sid = id;
            }
        } break;
        case TOKEN_TERM_STRING: {
            size_t const start_size = sizeof(FKL_PG_DELIM_STRING) - 1;
            size_t const end_size = start_size;
            size_t len = 0;
            char *str = fklCastEscapeCharBuf(&token.str[start_size],
                    token.len - end_size - start_size,
                    &len);
            s.str = fklAddStringCharBuf(terminals, str, len);
            fklAddStringCharBuf(&arg->g->delimiters, str, len);
            fklZfree(str);
        } break;

        case TOKEN_TERM_KEYWORD: {
            s.type = FKL_TERM_KEYWORD;
            size_t const start_size = sizeof(FKL_PG_DELIM_KEYWORD) - 1;
            size_t const end_size = start_size;
            size_t len = 0;
            char *str = fklCastEscapeCharBuf(&token.str[start_size],
                    token.len - end_size - start_size,
                    &len);
            s.str = fklAddStringCharBuf(terminals, str, len);
            fklZfree(str);
        } break;

        case TOKEN_TERM_REGEX: {
            size_t const start_size = sizeof(FKL_PG_DELIM_REGEX) - 1;
            size_t const end_size = start_size;
            s.type = FKL_TERM_REGEX;
            const FklRegexCode *re = fklAddRegexCharBuf(regexes,
                    &token.str[start_size],
                    token.len - end_size - start_size);
            s.re = re;
        } break;

        case TOKEN_REDUCE_TO: {
            Token token = next_token(arg, err, &buf, end);
            if (*err)
                goto error_happened;
            if (token.type != TOKEN_IDENTIFIER) {
                *err = ERR_UNEXPECTED_TOKEN;
                unexpected_token_error(arg, &token);
                goto error_happened;
            }

            if (arg->prod_action_resolver) {
                const FklGrammerBuiltinAction *action =
                        arg->prod_action_resolver(arg->resolver_ctx,
                                token.str,
                                token.len);
                if (action == NULL) {
                    *err = ERR_UNRESOLVED_ACTION_NAME;
                    unresolved_action_name_error(arg, &token);
                    goto error_happened;
                }
                action_name = action->name;
                action_func = action->func;
            }
            goto loop_break;
        } break;

        case TOKEN_COMMA:
        case TOKEN_END:
        case TOKEN_LB:
        case TOKEN_RB:
        case TOKEN_LEFT_ARROW: {
            *err = ERR_UNEXPECTED_TOKEN;
            unexpected_token_error(arg, &token);
            goto error_happened;
        } break;
        }

        if (has_ignore) {
            FklGrammerSym s = { .type = FKL_TERM_IGNORE };
            fklGraSymVectorPushBack(&gsym_vector, &s);
        }
        fklGraSymVectorPushBack(&gsym_vector, &s);
        has_ignore = 1;
    }
loop_break:;

    FklGrammerProduction *prod = fklCreateEmptyProduction(0,
            arg->current_nonterm,
            gsym_vector.size,
            action_name,
            action_func,
            NULL,
            fklProdCtxDestroyDoNothing,
            fklProdCtxCopyerDoNothing);

    for (size_t i = 0; i < gsym_vector.size; ++i)
        prod->syms[i] = gsym_vector.base[i];

    if (fklAddProdAndExtraToGrammer(arg->g, prod)) {
        fklDestroyGrammerProduction(prod);
        *err = ERR_ADDING_PRODUCTION;
        goto error_happened;
    }

    fklGraSymVectorUninit(&gsym_vector);
    return buf;

error_happened:
    while (!fklGraSymVectorIsEmpty(&gsym_vector)) {
        FklGrammerSym *s = fklGraSymVectorPopBack(&gsym_vector);

        if (s->type == FKL_TERM_BUILTIN && s->b.len) {
            s->b.len = 0;
            fklZfree(s->b.args);
            s->b.args = NULL;
        }
    }

    fklGraSymVectorUninit(&gsym_vector);

    return NULL;
}

void fklPrintParserGrammerParseError(int err,
        const FklParserGrammerParseArg *arg,
        FILE *fp) {
    switch (err) {
    case ERR_UNEXPECTED_TOKEN:
    case ERR_UNRESOLVED_ACTION_NAME:
    case ERR_UNRESOLVED_BUILTIN_TERMINAL:
    case ERR_INVALID_LEFT_PART:
    case ERR_BUILTIN_TERMINAL_INIT_FAILED:
        fprintf(fp, "%s at %zu\n", arg->error_msg.buf, arg->errline);
        break;
    case ERR_UNEXPECTED_EOF:
        fprintf(fp, "unexcpet eof at %zu\n", arg->line);
        break;
    case ERR_ADDING_PRODUCTION:
        fprintf(fp, "failed to add production at %zu\n", arg->line);
        break;
    case ERR_ADDING_IGNORE:
        fprintf(fp, "failed to add ignore at %zu\n", arg->line);
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
}

int fklParseProductionRuleWithCharBuf(FklParserGrammerParseArg *arg,
        const char *buf,
        size_t len) {
    const char *const end = buf + len;
    int err = 0;
    for (; buf < end;) {
        Token token = next_token(arg, &err, &buf, end);
        if (err) {
            return err;
        }
        switch (token.type) {
        case TOKEN_END:
            arg->state = FKL_PARSER_GRAMMER_ADDING_PRODUCTION;
            arg->current_nonterm = 0;
        case TOKEN_NONE:
            continue;
            break;

        case TOKEN_IDENTIFIER:
            if (MATCH_DELIM(token.str, token.str + token.len, FKL_PG_IGNORE)) {
                arg->state = FKL_PARSER_GRAMMER_ADDING_IGNORE;
                arg->current_nonterm = 0;
                buf = parse_ignore(arg, &err, buf, end);
                if (err)
                    return err;
                break;
            } else if (MATCH_DELIM(token.str,
                               token.str + token.len,
                               FKL_PG_TERMINALS)) {
                arg->state = FKL_PARSER_GRAMMER_ADDING_TERMINAL;
                arg->current_nonterm = 0;
                buf = parse_adding_terminal(arg, &err, buf, end);
                if (err)
                    return err;
                break;
            }

            if (MATCH_DELIM(token.str,
                        token.str + token.len,
                        FKL_PG_SPECIAL_PREFIX)) {
                err = ERR_INVALID_LEFT_PART;
                invalid_left_part_error(arg, &token);
            }

            arg->current_nonterm =
                    fklAddSymbolCharBuf(token.str, token.len, arg->g->st)->v;
            arg->state = FKL_PARSER_GRAMMER_ADDING_PRODUCTION;
            buf = parse_right_part(arg, &err, buf, end);
            if (err)
                return err;
            break;

        case TOKEN_CONCAT:
        case TOKEN_REDUCE_TO:
        case TOKEN_LB:
        case TOKEN_RB:
        case TOKEN_TERM_STRING:
        case TOKEN_TERM_KEYWORD:
        case TOKEN_TERM_REGEX:
        case TOKEN_COMMA:
            goto unexpected_token;
            break;
        case TOKEN_LEFT_ARROW:
            if (arg->current_nonterm == 0
                    && arg->state == FKL_PARSER_GRAMMER_ADDING_PRODUCTION) {
            unexpected_token:
                err = ERR_UNEXPECTED_TOKEN;
                unexpected_token_error(arg, &token);
                return err;
            }

            switch (arg->state) {
            case FKL_PARSER_GRAMMER_ADDING_PRODUCTION:
                buf = parse_right_part(arg, &err, buf - token.len, end);
                break;
            case FKL_PARSER_GRAMMER_ADDING_IGNORE:
                buf = parse_ignore(arg, &err, buf - token.len, end);
                break;
            case FKL_PARSER_GRAMMER_ADDING_TERMINAL:
                buf = parse_adding_terminal(arg, &err, buf - token.len, end);
                break;
            }
            if (err)
                return err;
            break;
        }
    }

    return 0;
}
