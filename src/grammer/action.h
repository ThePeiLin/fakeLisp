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
    // clang-format off
    { "symbol",      "symbol",      prod_action_symbol      },
    { "first",       "first",       prod_action_first       },
    { "second",      "second",      prod_action_second      },
    { "string",      "string",      prod_action_string      },
    { "nil",         "nil",         prod_action_nil         },
    { "pair",        "pair",        prod_action_pair        },
    { "list",        "list",        prod_action_list        },
    { "dec-integer", "dec_integer", prod_action_dec_integer },
    { "hex-integer", "hex_integer", prod_action_hex_integer },
    { "oct-integer", "oct_integer", prod_action_oct_integer },
    { "float",       "float",       prod_action_float       },
    { "char",        "char",        prod_action_char        },
    { "box",         "box",         prod_action_box         },
    { "vector",      "vector",      prod_action_vector      },
    { "quote",       "quote",       prod_action_quote       },
    { "unquote",     "unquote",     prod_action_unquote     },
    { "qsquote",     "qsquote",     prod_action_qsquote     },
    { "unqtesp",     "unquote",     prod_action_unqtesp     },
    { "pair-list",   "pair_list",   prod_action_pair_list   },
    { "hasheq",      "hasheq",      prod_action_hasheq      },
    { "hasheqv",     "hasheqv",     prod_action_hasheqv     },
    { "hashequal",   "hashequal",   prod_action_hashequal   },
    { "bytes",       "bytes",       prod_action_bytes       },
    { "keyword",     "keyword",     prod_action_keyword     },
    { "raw-string",  "raw-string",  prod_action_raw_string  },
    { NULL,          NULL,          NULL                    },
    // clang-format on
};

static inline const FklGrammerBuiltinAction *
builtin_prod_action_resolver(void *ctx, const char *key, size_t len) {
    for (const FklGrammerBuiltinAction *cur = &builtin_actions[0]; cur->key;
            ++cur) {
        size_t cur_len = strlen(cur->key);
        if (cur_len == len && memcmp(cur->key, key, cur_len) == 0)
            return cur;
    }
    return NULL;
}

#endif
