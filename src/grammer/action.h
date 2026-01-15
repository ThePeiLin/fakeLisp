#ifndef GRAMMER_ACTION_H
#define GRAMMER_ACTION_H

#include <fakeLisp/base.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

static inline void
put_line_number(const FklVMparseCtx *c, FklVMvalue *v, uint64_t line) {
    if (c->ln)
        fklVMvalueLntPut(c->ln, v, line);
}

static void *prod_action_symbol(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const char *start = "|";
    const size_t start_size = 1;
    const char *end = "|";
    const size_t end_size = 1;

    FklVMvalue *first = nodes[0].ast;
    const FklString *str = FKL_VM_STR(first);
    const char *cstr = str->str;
    size_t cstr_size = str->size;

    FklStrBuf buffer;
    fklInitStrBuf(&buffer);
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
            fklStrBufBincpy(&buffer, s, size);
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
        fklStrBufBincpy(&buffer, cstr, len);
        cstr += len;
        cstr_size -= len;
    }
    const FklVMparseCtx *c = (const FklVMparseCtx *)ctx;
    FklVMvalue *retval =
            fklVMaddSymbolCharBuf(c->exe, buffer.buf, buffer.index);
    fklUninitStrBuf(&buffer);
    return retval;
}

static void *prod_action_first(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    return nodes[0].ast;
}

static void *prod_action_second(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    return nodes[1].ast;
}

static void *prod_action_string(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const size_t start_size = 1;
    const size_t end_size = 1;

    FklVMvalue *first = nodes[0].ast;
    const FklString *str = FKL_VM_STR(first);
    const char *cstr = str->str;

    size_t size = 0;
    char *s = fklCastEscapeCharBuf(&cstr[start_size],
            str->size - end_size - start_size,
            &size);
    const FklVMparseCtx *c = (const FklVMparseCtx *)ctx;
    FklVM *exe = c->exe;
    FklVMvalue *retval = fklCreateVMvalueStr2(exe, size, s);
    fklZfree(s);
    return retval;
}

static void *prod_action_nil(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    return FKL_VM_NIL;
}

static void *prod_action_pair(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = (const FklVMparseCtx *)ctx;
    FklVM *exe = c->exe;
    FklVMvalue *car = nodes[0].ast;
    FklVMvalue *cdr = nodes[2].ast;
    FklVMvalue *pair = fklCreateVMvaluePair(exe, car, cdr);
    put_line_number(c, pair, line);
    return pair;
}

static void *prod_action_list(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = (const FklVMparseCtx *)ctx;
    FklVM *exe = c->exe;
    FklVMvalue *car = nodes[0].ast;
    FklVMvalue *cdr = nodes[1].ast;
    FklVMvalue *pair = fklCreateVMvaluePair(exe, car, cdr);
    put_line_number(c, pair, line);
    return pair;
}

static void *prod_action_dec_integer(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = (const FklVMparseCtx *)ctx;
    FklVM *exe = c->exe;
    const FklString *str = FKL_VM_STR((FklVMvalue *)nodes[0].ast);
    int64_t i = strtoll(str->str, NULL, 10);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN)
        return fklCreateVMvalueBigIntWithDecString(exe, str);
    else
        return FKL_MAKE_VM_FIX(i);
}

static void *prod_action_hex_integer(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = (const FklVMparseCtx *)ctx;
    FklVM *exe = c->exe;
    const FklString *str = FKL_VM_STR((FklVMvalue *)nodes[0].ast);
    int64_t i = strtoll(str->str, NULL, 16);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN)
        return fklCreateVMvalueBigIntWithHexString(exe, str);
    else
        return FKL_MAKE_VM_FIX(i);
}

static void *prod_action_oct_integer(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = (const FklVMparseCtx *)ctx;
    FklVM *exe = c->exe;
    const FklString *str = FKL_VM_STR((FklVMvalue *)nodes[0].ast);
    int64_t i = strtoll(str->str, NULL, 8);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN)
        return fklCreateVMvalueBigIntWithOctString(exe, str);
    else
        return FKL_MAKE_VM_FIX(i);
}

static void *prod_action_float(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = (const FklVMparseCtx *)ctx;
    FklVM *exe = c->exe;
    const FklString *str = FKL_VM_STR((FklVMvalue *)nodes[0].ast);
    return fklCreateVMvalueF64(exe, strtod(str->str, NULL));
}

static void *prod_action_char(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklString *str = FKL_VM_STR((FklVMvalue *)nodes[0].ast);
    if (!fklIsValidCharBuf(str->str + 2, str->size - 2))
        return NULL;
    return FKL_MAKE_VM_CHR(fklCharBufToChar(str->str + 2, str->size - 2));
}

static void *prod_action_box(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = (const FklVMparseCtx *)ctx;
    FklVM *exe = c->exe;
    FklVMvalue *r = fklCreateVMvalueBox(exe, nodes[1].ast);
    put_line_number(c, r, line);
    return r;
}

static void *prod_action_vector(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = (const FklVMparseCtx *)ctx;
    FklVM *exe = c->exe;
    FklVMvalue *list = nodes[1].ast;
    size_t len = fklVMlistLength(list);
    FklVMvalue *r = fklCreateVMvalueVec(exe, len);
    FklVMvalueVec *vec = FKL_VM_VEC(r);
    for (size_t i = 0; FKL_IS_PAIR(list); list = FKL_VM_CDR(list), i++)
        vec->base[i] = FKL_VM_CAR(list);
    put_line_number(c, r, line);
    return r;
}

static inline FklVMvalue *create_vmvalue_list(FklVM *exe,
        const FklVMparseCtx *c,
        FklVMvalue **a,
        size_t num,
        uint64_t line) {
    FklVMvalue *r = NULL;
    FklVMvalue **cur = &r;
    for (size_t i = 0; i < num; i++) {
        (*cur) = fklCreateVMvaluePair1(exe, a[i]);
        put_line_number(c, *cur, line);
        cur = &FKL_VM_CDR(*cur);
    }
    (*cur) = FKL_VM_NIL;
    return r;
}

static void *prod_action_quote(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = ctx;
    FklVM *exe = c->exe;
    FKL_ASSERT(exe->gc->sym_quote);
    FklVMvalue *head = exe->gc->sym_quote;
    FklVMvalue *rest = nodes[1].ast;

    FklVMvalue *items[] = { head, rest };

    return create_vmvalue_list(exe, c, items, 2, line);
}

static void *prod_action_unquote(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    const FklVMparseCtx *c = ctx;
    FklVM *exe = c->exe;
    FKL_ASSERT(exe->gc->sym_unquote);
    FklVMvalue *head = exe->gc->sym_unquote;
    FklVMvalue *rest = nodes[1].ast;

    FklVMvalue *items[] = { head, rest };

    return create_vmvalue_list(exe, c, items, 2, line);
}

static void *prod_action_qsquote(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    const FklVMparseCtx *c = ctx;
    FklVM *exe = c->exe;
    FKL_ASSERT(exe->gc->sym_qsquote);
    FklVMvalue *head = exe->gc->sym_qsquote;
    FklVMvalue *rest = nodes[1].ast;

    FklVMvalue *items[] = { head, rest };

    return create_vmvalue_list(exe, c, items, 2, line);
}

static void *prod_action_unqtesp(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    const FklVMparseCtx *c = ctx;
    FklVM *exe = c->exe;
    FKL_ASSERT(exe->gc->sym_unqtesp);
    FklVMvalue *head = exe->gc->sym_unqtesp;
    FklVMvalue *rest = nodes[1].ast;

    FklVMvalue *items[] = { head, rest };

    return create_vmvalue_list(exe, c, items, 2, line);
}

static void *prod_action_pair_list(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    const FklVMparseCtx *c = ctx;
    FklVM *exe = c->exe;

    FklVMvalue *car = nodes[1].ast;
    FklVMvalue *cdr = nodes[3].ast;
    FklVMvalue *rest = nodes[5].ast;

    FklVMvalue *pair = fklCreateVMvaluePair(exe, car, cdr);
    put_line_number(c, pair, line);

    FklVMvalue *r = fklCreateVMvaluePair(exe, pair, rest);
    put_line_number(c, r, line);

    return r;
}

static void *prod_action_hasheq(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = ctx;
    FklVM *exe = c->exe;

    FklVMvalue *list = nodes[1].ast;
    FklVMvalue *r = fklCreateVMvalueHashEq(exe);
    for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list)) {
        FklVMvalue *key_value = FKL_VM_CAR(list);
        fklVMhashTableSet(FKL_VM_HASH(r),
                FKL_VM_CAR(key_value),
                FKL_VM_CDR(key_value));
    }
    put_line_number(c, r, line);
    return r;
}

static void *prod_action_hasheqv(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = ctx;
    FklVM *exe = c->exe;

    FklVMvalue *list = nodes[1].ast;
    FklVMvalue *r = fklCreateVMvalueHashEq(exe);
    for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list)) {
        FklVMvalue *key_value = FKL_VM_CAR(list);
        fklVMhashTableSet(FKL_VM_HASH(r),
                FKL_VM_CAR(key_value),
                FKL_VM_CDR(key_value));
    }
    put_line_number(c, r, line);
    return r;
}

static void *prod_action_hashequal(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const FklVMparseCtx *c = ctx;
    FklVM *exe = c->exe;

    FklVMvalue *list = nodes[1].ast;
    FklVMvalue *r = fklCreateVMvalueHashEq(exe);
    for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list)) {
        FklVMvalue *key_value = FKL_VM_CAR(list);
        fklVMhashTableSet(FKL_VM_HASH(r),
                FKL_VM_CAR(key_value),
                FKL_VM_CDR(key_value));
    }
    put_line_number(c, r, line);
    return r;
}

static void *prod_action_bytes(void *args,
        void *ctx,
        const FklAnalysisSymbol nodes[],
        size_t count,
        size_t line) {
    const size_t start_size = 2;
    const size_t end_size = 1;

    FklVMvalue *first = nodes[0].ast;
    const FklString *str = FKL_VM_STR(first);
    const char *cstr = str->str;

    size_t size = 0;
    char *s = fklCastEscapeCharBuf(&cstr[start_size],
            str->size - end_size - start_size,
            &size);

    const FklVMparseCtx *c = ctx;
    FklVM *exe = c->exe;
    FklVMvalue *retval = fklCreateVMvalueBvec2(exe, size, (uint8_t *)s);

    fklZfree(s);
    return retval;
}

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
