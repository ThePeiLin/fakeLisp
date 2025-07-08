#ifndef GRAMMER_ACTION_H
#define GRAMMER_ACTION_H

#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/nast.h>
#include <fakeLisp/utils.h>

static void *prod_action_symbol(void *args, void *ctx, void *ast[], size_t num,
                                size_t line) {
    FklNastNode **nodes = (FklNastNode **)ast;
    const char *start = "|";
    size_t start_size = 1;
    const char *end = "|";
    size_t end_size = 1;

    const FklString *str = nodes[0]->str;
    const char *cstr = str->str;
    size_t cstr_size = str->size;

    FklStringBuffer buffer;
    fklInitStringBuffer(&buffer);
    const char *end_cstr = cstr + str->size;
    while (cstr < end_cstr) {
        if (fklCharBufMatch(start, start_size, cstr, cstr_size) >= 0) {
            cstr += start_size;
            cstr_size -= start_size;
            size_t len = fklQuotedCharBufMatch(cstr, cstr_size, end, end_size);
            if (!len)
                return 0;
            size_t size = 0;
            char *s = fklCastEscapeCharBuf(cstr, len - end_size, &size);
            fklStringBufferBincpy(&buffer, s, size);
            fklZfree(s);
            cstr += len;
            cstr_size -= len;
            continue;
        }
        size_t len = 0;
        for (; (cstr + len) < end_cstr; len++)
            if (fklCharBufMatch(start, start_size, cstr + len, cstr_size - len)
                >= 0)
                break;
        fklStringBufferBincpy(&buffer, cstr, len);
        cstr += len;
        cstr_size -= len;
    }
    FklSid_t id = fklAddSymbolCharBuf(buffer.buf, buffer.index, ctx)->v;
    FklNastNode *node = fklCreateNastNode(FKL_NAST_SYM, nodes[0]->curline);
    node->sym = id;
    fklUninitStringBuffer(&buffer);
    return node;
}

static void *prod_action_first(void *args, void *ctx, void *nodes[], size_t num,
                               size_t line) {
    return fklMakeNastNodeRef(nodes[0]);
}

static void *prod_action_second(void *args, void *ctx, void *nodes[],
                                size_t num, size_t line) {
    return fklMakeNastNodeRef(nodes[1]);
}

static void *prod_action_string(void *args, void *ctx, void *ast[], size_t num,
                                size_t line) {
    FklNastNode **nodes = (FklNastNode **)ast;
    const size_t start_size = 1;
    const size_t end_size = 1;

    const FklString *str = nodes[0]->str;
    const char *cstr = str->str;

    size_t size = 0;
    char *s = fklCastEscapeCharBuf(&cstr[start_size],
                                   str->size - end_size - start_size, &size);
    FklNastNode *node = fklCreateNastNode(FKL_NAST_STR, nodes[0]->curline);
    node->str = fklCreateString(size, s);
    fklZfree(s);
    return node;
}

static void *prod_action_nil(void *args, void *ctx, void *nodes[], size_t num,
                             size_t line) {
    return fklCreateNastNode(FKL_NAST_NIL, line);
}

static void *prod_action_pair(void *args, void *ctx, void *nodes[], size_t num,
                              size_t line) {
    FklNastNode *car = nodes[0];
    FklNastNode *cdr = nodes[2];
    FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    pair->pair = fklCreateNastPair();
    pair->pair->car = fklMakeNastNodeRef(car);
    pair->pair->cdr = fklMakeNastNodeRef(cdr);
    return pair;
}

static void *prod_action_list(void *args, void *ctx, void *nodes[], size_t num,
                              size_t line) {
    FklNastNode *car = nodes[0];
    FklNastNode *cdr = nodes[1];
    FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    pair->pair = fklCreateNastPair();
    pair->pair->car = fklMakeNastNodeRef(car);
    pair->pair->cdr = fklMakeNastNodeRef(cdr);
    return pair;
}

static void *prod_action_dec_integer(void *args, void *ctx, void *nodes[],
                                     size_t num, size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0])->str;
    int64_t i = strtoll(str->str, NULL, 10);
    FklNastNode *r = fklCreateNastNode(FKL_NAST_FIX, line);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN) {
        FklBigInt bInt = FKL_BIGINT_0;
        fklInitBigIntWithDecCharBuf(&bInt, str->str, str->size);
        FklBigInt *bi = (FklBigInt *)fklZmalloc(sizeof(FklBigInt));
        FKL_ASSERT(bi);
        *bi = bInt;
        r->bigInt = bi;
        r->type = FKL_NAST_BIGINT;
    } else {
        r->type = FKL_NAST_FIX;
        r->fix = i;
    }
    return r;
}

static void *prod_action_hex_integer(void *args, void *ctx, void *nodes[],
                                     size_t num, size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0])->str;
    int64_t i = strtoll(str->str, NULL, 16);
    FklNastNode *r = fklCreateNastNode(FKL_NAST_FIX, line);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN) {
        FklBigInt bInt = FKL_BIGINT_0;
        fklInitBigIntWithHexCharBuf(&bInt, str->str, str->size);
        FklBigInt *bi = (FklBigInt *)fklZmalloc(sizeof(FklBigInt));
        FKL_ASSERT(bi);
        *bi = bInt;
        r->bigInt = bi;
        r->type = FKL_NAST_BIGINT;
    } else {
        r->type = FKL_NAST_FIX;
        r->fix = i;
    }
    return r;
}

static void *prod_action_oct_integer(void *args, void *ctx, void *nodes[],
                                     size_t num, size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0])->str;
    int64_t i = strtoll(str->str, NULL, 8);
    FklNastNode *r = fklCreateNastNode(FKL_NAST_FIX, line);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN) {
        FklBigInt bInt = FKL_BIGINT_0;
        fklInitBigIntWithOctCharBuf(&bInt, str->str, str->size);
        FklBigInt *bi = (FklBigInt *)fklZmalloc(sizeof(FklBigInt));
        FKL_ASSERT(bi);
        *bi = bInt;
        r->bigInt = bi;
        r->type = FKL_NAST_BIGINT;
    } else {
        r->type = FKL_NAST_FIX;
        r->fix = i;
    }
    return r;
}

static void *prod_action_float(void *args, void *ctx, void *nodes[], size_t num,
                               size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0])->str;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_F64, line);
    double i = strtod(str->str, NULL);
    r->f64 = i;
    return r;
}

static void *prod_action_char(void *args, void *ctx, void *nodes[], size_t num,
                              size_t line) {
    const FklString *str = ((FklNastNode *)nodes[0])->str;
    if (!fklIsValidCharBuf(str->str + 2, str->size - 2))
        return NULL;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_CHR, line);
    r->chr = fklCharBufToChar(str->str + 2, str->size - 2);
    return r;
}

static void *prod_action_box(void *args, void *ctx, void *nodes[], size_t num,
                             size_t line) {
    FklNastNode *box = fklCreateNastNode(FKL_NAST_BOX, line);
    box->box = fklMakeNastNodeRef(nodes[1]);
    return box;
}

static void *prod_action_vector(void *args, void *ctx, void *nodes[],
                                size_t num, size_t line) {
    FklNastNode *list = nodes[1];
    FklNastNode *r = fklCreateNastNode(FKL_NAST_VECTOR, line);
    size_t len = fklNastListLength(list);
    FklNastVector *vec = fklCreateNastVector(len);
    r->vec = vec;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++)
        vec->base[i] = fklMakeNastNodeRef(list->pair->car);
    return r;
}

static inline FklNastNode *create_nast_list(FklNastNode *a[], size_t num,
                                            uint64_t line) {
    FklNastNode *r = NULL;
    FklNastNode **cur = &r;
    for (size_t i = 0; i < num; i++) {
        (*cur) = fklCreateNastNode(FKL_NAST_PAIR, a[i]->curline);
        (*cur)->pair = fklCreateNastPair();
        (*cur)->pair->car = a[i];
        cur = &(*cur)->pair->cdr;
    }
    (*cur) = fklCreateNastNode(FKL_NAST_NIL, line);
    return r;
}

static void *prod_action_quote(void *args, void *ctx, void *nodes[], size_t num,
                               size_t line) {
    FklSid_t id = fklAddSymbolCstr("quote", ctx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static void *prod_action_unquote(void *args, void *ctx, void *nodes[],
                                 size_t num, size_t line) {
    FklSid_t id = fklAddSymbolCstr("unquote", ctx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static void *prod_action_qsquote(void *args, void *ctx, void *nodes[],
                                 size_t num, size_t line) {
    FklSid_t id = fklAddSymbolCstr("qsquote", ctx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static void *prod_action_unqtesp(void *args, void *ctx, void *nodes[],
                                 size_t num, size_t line) {
    FklSid_t id = fklAddSymbolCstr("unqtesp", ctx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static void *prod_action_pair_list(void *args, void *ctx, void *nodes[],
                                   size_t num, size_t line) {
    FklNastNode *car = nodes[1];
    FklNastNode *cdr = nodes[3];
    FklNastNode *rest = nodes[5];

    FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    pair->pair = fklCreateNastPair();
    pair->pair->car = fklMakeNastNodeRef(car);
    pair->pair->cdr = fklMakeNastNodeRef(cdr);

    FklNastNode *r = fklCreateNastNode(FKL_NAST_PAIR, line);
    r->pair = fklCreateNastPair();
    r->pair->car = pair;
    r->pair->cdr = fklMakeNastNodeRef(rest);
    return r;
}

static void *prod_action_hasheq(void *args, void *ctx, void *nodes[],
                                size_t num, size_t line) {
    FklNastNode *list = nodes[1];
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQ, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static void *prod_action_hasheqv(void *args, void *ctx, void *nodes[],
                                 size_t num, size_t line) {
    FklNastNode *list = nodes[1];
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQV, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static void *prod_action_hashequal(void *args, void *ctx, void *nodes[],
                                   size_t num, size_t line) {
    FklNastNode *list = nodes[1];
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQUAL, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static void *prod_action_bytes(void *args, void *ctx, void *ast[], size_t num,
                               size_t line) {
    FklNastNode **nodes = (FklNastNode **)ast;
    const size_t start_size = 2;
    const size_t end_size = 1;

    const FklString *str = nodes[0]->str;
    const char *cstr = str->str;

    size_t size = 0;
    char *s = fklCastEscapeCharBuf(&cstr[start_size],
                                   str->size - end_size - start_size, &size);
    FklNastNode *node =
        fklCreateNastNode(FKL_NAST_BYTEVECTOR, nodes[0]->curline);
    node->bvec = fklCreateBytevector(size, (uint8_t *)s);
    fklZfree(s);
    return node;
}

static const FklGrammerBuiltinAction builtin_actions[] = {
    {"symbol", prod_action_symbol},
    {"first", prod_action_first},
    {"second", prod_action_second},
    {"string", prod_action_string},
    {"nil", prod_action_nil},
    {"pair", prod_action_pair},
    {"list", prod_action_list},
    {"dec_integer", prod_action_dec_integer},
    {"hex_integer", prod_action_hex_integer},
    {"oct_integer", prod_action_oct_integer},
    {"float", prod_action_float},
    {"char", prod_action_char},
    {"box", prod_action_box},
    {"vector", prod_action_vector},
    {"quote", prod_action_quote},
    {"unquote", prod_action_unquote},
    {"qsquote", prod_action_qsquote},
    {"unqtesp", prod_action_unqtesp},
    {"pair_list", prod_action_pair_list},
    {"hasheq", prod_action_hasheq},
    {"hasheqv", prod_action_hasheqv},
    {"hashequal", prod_action_hashequal},
    {"bytes", prod_action_bytes},
    {NULL, NULL},
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
