#ifndef GRAMMER_ACTION_H
#define GRAMMER_ACTION_H

#include <fakeLisp/base.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

#include "vmaction.c"

static const FklGrammerBuiltinAction builtin_actions[] = {
    { "symbol", prod_action_symbol },
    { "first", prod_action_first },
    { "second", prod_action_second },
    { "string", prod_action_string },
    { "nil", prod_action_nil },
    { "pair", prod_action_pair },
    { "list", prod_action_list },
    { "dec_integer", prod_action_dec_integer },
    { "hex_integer", prod_action_hex_integer },
    { "oct_integer", prod_action_oct_integer },
    { "float", prod_action_float },
    { "char", prod_action_char },
    { "box", prod_action_box },
    { "vector", prod_action_vector },
    { "quote", prod_action_quote },
    { "unquote", prod_action_unquote },
    { "qsquote", prod_action_qsquote },
    { "unqtesp", prod_action_unqtesp },
    { "pair_list", prod_action_pair_list },
    { "hasheq", prod_action_hasheq },
    { "hasheqv", prod_action_hasheqv },
    { "hashequal", prod_action_hashequal },
    { "bytes", prod_action_bytes },
    { "keyword", prod_action_keyword },
    { "raw_string", prod_action_raw_string },
    { NULL, NULL },
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

#endif
