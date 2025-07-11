#include <fakeLisp/grammer.h>
#include <fakeLisp/vm.h>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-function"
#endif

static inline void *
prod_action_symbol(void *ctx, void *ast[], size_t num, size_t line) {
    FklVMvalue **values = (FklVMvalue **)ast;
    const char *start = "|";
    const size_t start_size = 1;
    const char *end = "|";
    const size_t end_size = 1;

    const FklString *str = FKL_VM_STR(values[0]);
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
    FklSid_t id =
            fklVMaddSymbolCharBuf(((FklVM *)ctx)->gc, buffer.buf, buffer.index)
                    ->v;
    fklUninitStringBuffer(&buffer);
    FklVMvalue *retval = FKL_MAKE_VM_SYM(id);
    return retval;
}

static inline void *
prod_action_first(void *ctx, void *nodes[], size_t num, size_t line) {
    return nodes[0];
}

static inline void *
prod_action_second(void *ctx, void *nodes[], size_t num, size_t line) {
    return nodes[1];
}

static inline void *
prod_action_string(void *ctx, void *ast[], size_t num, size_t line) {
    FklVMvalue **values = (FklVMvalue **)ast;
    const size_t start_size = 1;
    const size_t end_size = 1;

    const FklString *str = FKL_VM_STR(values[0]);
    const char *cstr = str->str;

    size_t size = 0;
    char *s = fklCastEscapeCharBuf(&cstr[start_size],
            str->size - end_size - start_size,
            &size);

    FklVM *exe = (FklVM *)ctx;
    FklVMvalue *retval = fklCreateVMvalueStr2(exe, size, s);

    fklZfree(s);
    return retval;
}

static inline void *
prod_action_nil(void *ctx, void *nodes[], size_t num, size_t line) {
    return FKL_VM_NIL;
}

static inline void *
prod_action_pair(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    FklVMvalue *car = nodes[0];
    FklVMvalue *cdr = nodes[2];
    FklVMvalue *pair = fklCreateVMvaluePair(exe, car, cdr);
    return pair;
}

static inline void *
prod_action_list(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    FklVMvalue *car = nodes[0];
    FklVMvalue *cdr = nodes[1];
    FklVMvalue *pair = fklCreateVMvaluePair(exe, car, cdr);
    return pair;
}

static inline void *
prod_action_dec_integer(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    const FklString *str = FKL_VM_STR((FklVMvalue *)nodes[0]);
    int64_t i = strtoll(str->str, NULL, 10);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN)
        return fklCreateVMvalueBigIntWithDecString(exe, str);
    else
        return FKL_MAKE_VM_FIX(i);
}

static inline void *
prod_action_hex_integer(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    const FklString *str = FKL_VM_STR((FklVMvalue *)nodes[0]);
    int64_t i = strtoll(str->str, NULL, 16);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN)
        return fklCreateVMvalueBigIntWithHexString(exe, str);
    else
        return FKL_MAKE_VM_FIX(i);
}

static inline void *
prod_action_oct_integer(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    const FklString *str = FKL_VM_STR((FklVMvalue *)nodes[0]);
    int64_t i = strtoll(str->str, NULL, 8);
    if (i > FKL_FIX_INT_MAX || i < FKL_FIX_INT_MIN)
        return fklCreateVMvalueBigIntWithOctString(exe, str);
    else
        return FKL_MAKE_VM_FIX(i);
}

static inline void *
prod_action_float(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    const FklString *str = FKL_VM_STR((FklVMvalue *)nodes[0]);
    return fklCreateVMvalueF64(exe, strtod(str->str, NULL));
}

static inline void *
prod_action_char(void *ctx, void *nodes[], size_t num, size_t line) {
    const FklString *str = FKL_VM_STR((FklVMvalue *)nodes[0]);
    if (!fklIsValidCharBuf(str->str + 2, str->size - 2))
        return NULL;
    return FKL_MAKE_VM_CHR(fklCharBufToChar(str->str + 2, str->size - 2));
}

static inline void *
prod_action_box(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    return fklCreateVMvalueBox(exe, nodes[1]);
}

static inline void *
prod_action_vector(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    FklVMvalue *list = nodes[1];
    size_t len = fklVMlistLength(list);
    FklVMvalue *r = fklCreateVMvalueVec(exe, len);
    FklVMvec *vec = FKL_VM_VEC(r);
    for (size_t i = 0; FKL_IS_PAIR(list); list = FKL_VM_CDR(list), i++)
        vec->base[i] = FKL_VM_CAR(list);
    return r;
}

static inline FklVMvalue *
create_vmvalue_list(FklVM *exe, FklVMvalue **a, size_t num) {
    FklVMvalue *r = NULL;
    FklVMvalue **cur = &r;
    for (size_t i = 0; i < num; i++) {
        (*cur) = fklCreateVMvaluePairWithCar(exe, a[i]);
        cur = &FKL_VM_CDR(*cur);
    }
    (*cur) = FKL_VM_NIL;
    return r;
}

static inline void *
prod_action_quote(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    FklSid_t id = fklVMaddSymbolCstr(exe->gc, "quote")->v;
    FklVMvalue *rest = nodes[1];
    FklVMvalue *head = FKL_MAKE_VM_SYM(id);

    FklVMvalue *items[] = { head, rest };

    return create_vmvalue_list(exe, items, 2);
}

static inline void *
prod_action_unquote(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    FklSid_t id = fklVMaddSymbolCstr(exe->gc, "unquote")->v;
    FklVMvalue *rest = nodes[1];
    FklVMvalue *head = FKL_MAKE_VM_SYM(id);

    FklVMvalue *items[] = { head, rest };

    return create_vmvalue_list(exe, items, 2);
}

static inline void *
prod_action_qsquote(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    FklSid_t id = fklVMaddSymbolCstr(exe->gc, "qsquote")->v;
    FklVMvalue *rest = nodes[1];
    FklVMvalue *head = FKL_MAKE_VM_SYM(id);

    FklVMvalue *items[] = { head, rest };

    return create_vmvalue_list(exe, items, 2);
}

static inline void *
prod_action_unqtesp(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    FklSid_t id = fklVMaddSymbolCstr(exe->gc, "unqtesp")->v;
    FklVMvalue *rest = nodes[1];
    FklVMvalue *head = FKL_MAKE_VM_SYM(id);

    FklVMvalue *items[] = { head, rest };

    return create_vmvalue_list(exe, items, 2);
}

static inline void *
prod_action_pair_list(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;

    FklVMvalue *car = nodes[1];
    FklVMvalue *cdr = nodes[3];
    FklVMvalue *rest = nodes[5];

    FklVMvalue *pair = fklCreateVMvaluePair(exe, car, cdr);

    FklVMvalue *r = fklCreateVMvaluePair(exe, pair, rest);
    return r;
}

static inline void *
prod_action_hasheq(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    FklVMvalue *list = nodes[1];
    FklVMvalue *r = fklCreateVMvalueHashEq(exe);
    for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list)) {
        FklVMvalue *key_value = FKL_VM_CAR(list);
        fklVMhashTableSet(FKL_VM_HASH(r),
                FKL_VM_CAR(key_value),
                FKL_VM_CDR(key_value));
    }
    return r;
}

static inline void *
prod_action_hasheqv(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    FklVMvalue *list = nodes[1];
    FklVMvalue *r = fklCreateVMvalueHashEqv(exe);
    for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list)) {
        FklVMvalue *key_value = FKL_VM_CAR(list);
        fklVMhashTableSet(FKL_VM_HASH(r),
                FKL_VM_CAR(key_value),
                FKL_VM_CDR(key_value));
    }
    return r;
}

static inline void *
prod_action_hashequal(void *ctx, void *nodes[], size_t num, size_t line) {
    FklVM *exe = (FklVM *)ctx;
    FklVMvalue *list = nodes[1];
    FklVMvalue *r = fklCreateVMvalueHashEqual(exe);
    for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list)) {
        FklVMvalue *key_value = FKL_VM_CAR(list);
        fklVMhashTableSet(FKL_VM_HASH(r),
                FKL_VM_CAR(key_value),
                FKL_VM_CDR(key_value));
    }
    return r;
}

static inline void *
prod_action_bytes(void *ctx, void *ast[], size_t num, size_t line) {
    FklVMvalue **values = (FklVMvalue **)ast;
    const size_t start_size = 2;
    const size_t end_size = 1;

    const FklString *str = FKL_VM_STR(values[0]);
    const char *cstr = str->str;

    size_t size = 0;
    char *s = fklCastEscapeCharBuf(&cstr[start_size],
            str->size - end_size - start_size,
            &size);

    FklVM *exe = (FklVM *)ctx;
    FklVMvalue *retval = fklCreateVMvalueBvec2(exe, size, (uint8_t *)s);

    fklZfree(s);
    return retval;
}
