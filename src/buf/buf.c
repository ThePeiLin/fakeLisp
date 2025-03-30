#include <fakeLisp/base.h>
#include <fakeLisp/vm.h>

#include <stdio.h>
#include <string.h>

#define PREDICATE(condition)                                                   \
    FKL_DECL_AND_CHECK_ARG(val, exe);                                          \
    FKL_CHECK_REST_ARG(exe);                                                   \
    FKL_VM_PUSH_VALUE(exe, (condition) ? FKL_VM_TRUE : FKL_VM_NIL);            \
    return 0;

static int strbuf_equal(const FklVMud *a, const FklVMud *b) {
    FKL_DECL_UD_DATA(bufa, FklStringBuffer, a);
    FKL_DECL_UD_DATA(bufb, FklStringBuffer, b);
    return bufa->index == bufb->index
        && !memcmp(bufa->buf, bufb->buf, bufa->index);
}

static void strbuf_finalizer(FklVMud *p) {
    FKL_DECL_UD_DATA(buf, FklStringBuffer, p);
    fklUninitStringBuffer(buf);
}

static void strbuf_as_prin1(const FklVMud *ud, FklStringBuffer *buf,
                            FklVMgc *gc) {
    FKL_DECL_UD_DATA(bufa, FklStringBuffer, ud);
    fklPrintRawCharBufToStringBuffer(buf, bufa->index, bufa->buf, "\"", "\"",
                                     '"');
}

static void strbuf_as_princ(const FklVMud *ud, FklStringBuffer *buf,
                            FklVMgc *gc) {
    FKL_DECL_UD_DATA(bufa, FklStringBuffer, ud);
    fklStringBufferConcatWithStringBuffer(buf, bufa);
}

static void strbuf_write(const FklVMud *ud, FILE *fp) {
    FKL_DECL_UD_DATA(buf, FklStringBuffer, ud);
    fwrite(buf->buf, buf->index, 1, fp);
}

static FklVMudMetaTable StringBufferMetaTable;

static inline int is_strbuf_ud(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->t == &StringBufferMetaTable;
}

static int strbuf_append(FklVMud *ud, uint32_t argc, FklVMvalue *const *top) {
    FKL_DECL_UD_DATA(buf, FklStringBuffer, ud);
    FklVMvalue *const *base = top - argc;
    for (; top > base; --top) {
        FklVMvalue *cur = *top;
        if (FKL_IS_CHR(cur))
            fklStringBufferPutc(buf, FKL_GET_CHR(cur));
        else if (FKL_IS_STR(cur))
            fklStringBufferConcatWithString(buf, FKL_VM_STR(cur));
        else if (is_strbuf_ud(cur)) {
            FKL_DECL_VM_UD_DATA(other, FklStringBuffer, cur);
            fklStringBufferConcatWithStringBuffer(buf, other);
        } else
            return 1;
    }
    return 0;
}

static inline FklVMvalue *create_vmstrbuf(FklVM *exe, size_t capacity,
                                          FklVMvalue *rel) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &StringBufferMetaTable, rel);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, r);
    fklInitStringBuffer(buf);
    fklStringBufferReverse(buf, capacity);
    return r;
}

static inline FklVMvalue *create_vmstrbuf2(FklVM *exe, size_t size,
                                           const char *ptr, FklVMvalue *rel) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &StringBufferMetaTable, rel);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, r);
    fklInitStringBuffer(buf);
    fklStringBufferReverse(buf, size);
    fklStringBufferBincpy(buf, ptr, size);
    return r;
}

static FklVMvalue *strbuf_copy_append(FklVM *exe, const FklVMud *ud,
                                      uint32_t argc, FklVMvalue *const *top) {
    FKL_DECL_UD_DATA(buf, FklStringBuffer, ud);
    FklVMvalue *retval = create_vmstrbuf(exe, 0, ud->rel);
    FklVMvalue *const *base = top - argc;
    FKL_DECL_VM_UD_DATA(rbuf, FklStringBuffer, retval);
    fklStringBufferConcatWithStringBuffer(rbuf, buf);
    for (; top > base; --top) {
        FklVMvalue *cur = *top;
        if (FKL_IS_CHR(cur))
            fklStringBufferPutc(rbuf, FKL_GET_CHR(cur));
        else if (FKL_IS_STR(cur))
            fklStringBufferConcatWithString(rbuf, FKL_VM_STR(cur));
        else if (is_strbuf_ud(cur)) {
            FKL_DECL_VM_UD_DATA(other, FklStringBuffer, cur);
            fklStringBufferConcatWithStringBuffer(rbuf, other);
        } else
            return NULL;
    }
    return retval;
}

static size_t strbuf_length(const FklVMud *ud) {
    FKL_DECL_UD_DATA(buf, FklStringBuffer, ud);
    return buf->index;
}

static uintptr_t strbuf_hash(const FklVMud *ud, FklVMvalueVector *s) {
    FKL_DECL_UD_DATA(buf, FklStringBuffer, ud);
    return fklCharBufHash(buf->buf, buf->size);
}

static int strbuf_cmp(const FklVMud *ud, const FklVMvalue *v, int *err) {
    FKL_DECL_UD_DATA(buf, FklStringBuffer, ud);
    if (FKL_IS_STR(v))
        return -fklStringCharBufCmp(FKL_VM_STR(v), buf->index, buf->buf);
    else if (is_strbuf_ud(v)) {
        FKL_DECL_VM_UD_DATA(buf1, FklStringBuffer, v);
        return fklStringBufferCmp(buf, buf1);
    } else
        *err = 1;
    return 0;
}

static FklVMudMetaTable StringBufferMetaTable = {
    .size = sizeof(FklStringBuffer),
    .__equal = strbuf_equal,
    .__as_prin1 = strbuf_as_prin1,
    .__as_princ = strbuf_as_princ,
    .__write = strbuf_write,
    .__append = strbuf_append,
    .__copy_append = strbuf_copy_append,
    .__length = strbuf_length,
    .__hash = strbuf_hash,
    .__cmp = strbuf_cmp,
    .__finalizer = strbuf_finalizer,
};

static int export_strbuf_p(FKL_CPROC_ARGL) { PREDICATE(is_strbuf_ud(val)) }

static int export_make_strbuf(FKL_CPROC_ARGL) {
    if (FKL_VM_GET_ARG_NUM(exe) == 0) {
        fklResBp(exe);
        FKL_VM_PUSH_VALUE(
            exe, create_vmstrbuf(exe, 0, FKL_VM_CPROC(ctx->proc)->dll));
        return 0;
    }
    FKL_DECL_AND_CHECK_ARG(size, exe);
    FKL_CHECK_TYPE(size, fklIsVMint, exe);
    FklVMvalue *content = FKL_VM_POP_ARG(exe);
    FKL_CHECK_REST_ARG(exe);
    if (fklIsVMnumberLt0(size))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t len = fklVMgetUint(size);
    FklVMvalue *r = create_vmstrbuf(exe, len, FKL_VM_CPROC(ctx->proc)->dll);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, r);
    char ch = 0;
    if (content) {
        FKL_CHECK_TYPE(content, FKL_IS_CHR, exe);
        ch = FKL_GET_CHR(content);
    }
    memset(buf->buf, ch, len);
    buf->index = len;
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_make_strbuf_with_capacity(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(size, exe);
    FKL_CHECK_TYPE(size, fklIsVMint, exe);
    FKL_CHECK_REST_ARG(exe);
    if (fklIsVMnumberLt0(size))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t len = fklVMgetUint(size);
    FKL_VM_PUSH_VALUE(exe,
                      create_vmstrbuf(exe, len, FKL_VM_CPROC(ctx->proc)->dll));
    return 0;
}

static int export_strbuf(FKL_CPROC_ARGL) {
    size_t size = FKL_VM_GET_ARG_NUM(exe);
    if (size == 1) {
        FKL_DECL_AND_CHECK_ARG(str_or_buf, exe);
        FKL_CHECK_REST_ARG(exe);
        if (FKL_IS_CHR(str_or_buf)) {
            char c = FKL_GET_CHR(str_or_buf);
            FKL_VM_PUSH_VALUE(
                exe,
                create_vmstrbuf2(exe, 1, &c, FKL_VM_CPROC(ctx->proc)->dll));
        } else if (FKL_IS_STR(str_or_buf)) {
            const FklString *b = FKL_VM_STR(str_or_buf);
            FKL_VM_PUSH_VALUE(exe,
                              create_vmstrbuf2(exe, b->size, b->str,
                                               FKL_VM_CPROC(ctx->proc)->dll));
        } else if (is_strbuf_ud(str_or_buf)) {
            FKL_DECL_VM_UD_DATA(b, FklStringBuffer, str_or_buf);
            FKL_VM_PUSH_VALUE(exe,
                              create_vmstrbuf2(exe, b->index, b->buf,
                                               FKL_VM_CPROC(ctx->proc)->dll));
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } else {
        FklVMvalue *r = create_vmstrbuf(exe, 0, FKL_VM_CPROC(ctx->proc)->dll);
        FKL_DECL_VM_UD_DATA(b, FklStringBuffer, r);
        for (FklVMvalue *cur = FKL_VM_POP_ARG(exe); cur != NULL;
             cur = FKL_VM_POP_ARG(exe)) {
            FKL_CHECK_TYPE(cur, FKL_IS_CHR, exe);
            fklStringBufferPutc(b, FKL_GET_CHR(cur));
        }
        fklResBp(exe);
        FKL_VM_PUSH_VALUE(exe, r);
    }
    return 0;
}

static int export_strbuf_to_string(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(buf, exe);
    FKL_CHECK_TYPE(buf, is_strbuf_ud, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_DECL_VM_UD_DATA(b, FklStringBuffer, buf);
    FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueStr2(exe, b->index, b->buf));
    return 0;
}

static int export_strbuf_to_bytevector(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(buf, exe);
    FKL_CHECK_TYPE(buf, is_strbuf_ud, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_DECL_VM_UD_DATA(b, FklStringBuffer, buf);
    FKL_VM_PUSH_VALUE(
        exe, fklCreateVMvalueBvec2(exe, b->index, (const uint8_t *)b->buf));
    return 0;
}

static int export_strbuf_to_list(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, obj);
    size_t len = buf->index;
    const char *ptr = buf->buf;
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **cur = &r;
    for (size_t i = 0; i < len; i++) {
        *cur = fklCreateVMvaluePairWithCar(exe, FKL_MAKE_VM_CHR(ptr[i]));
        cur = &FKL_VM_CDR(*cur);
    }
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_strbuf_to_vector(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe)
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, obj);
    size_t len = buf->index;
    const char *ptr = buf->buf;
    FklVMvalue *r = fklCreateVMvalueVec(exe, len);
    FklVMvec *vec = FKL_VM_VEC(r);
    for (size_t i = 0; i < len; i++)
        vec->base[i] = FKL_MAKE_VM_CHR(ptr[i]);
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_strbuf_to_symbol(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, obj);
    FKL_VM_PUSH_VALUE(
        exe, FKL_MAKE_VM_SYM(
                 fklVMaddSymbolCharBuf(exe->gc, buf->buf, buf->index)->id));
    return 0;
}

static int export_strbuf_ref(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG2(str, place, exe);
    FKL_CHECK_REST_ARG(exe);
    if (!fklIsVMint(place) || !is_strbuf_ud(str))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (fklIsVMnumberLt0(place))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t index = fklVMgetUint(place);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, str);
    if (index >= buf->index)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_CHR(buf->buf[index]));
    return 0;
}

static int export_strbuf_set1(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG3(str, place, target, exe);
    FKL_CHECK_REST_ARG(exe);
    if (!fklIsVMint(place) || !is_strbuf_ud(str))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (fklIsVMnumberLt0(place))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t index = fklVMgetUint(place);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, str);
    if (index >= buf->index)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    if (!FKL_IS_CHR(target) && !fklIsVMint(target))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    buf->buf[index] =
        FKL_IS_CHR(target) ? FKL_GET_CHR(target) : fklVMgetInt(target);
    FKL_VM_PUSH_VALUE(exe, target);
    return 0;
}

static int export_strbuf_clear(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, obj);
    fklStringBufferClear(buf);
    FKL_VM_PUSH_VALUE(exe, obj);
    return 0;
}

static int export_strbuf_reserve(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG2(str, place, exe);
    FKL_CHECK_REST_ARG(exe);
    if (!fklIsVMint(place) || !is_strbuf_ud(str))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (fklIsVMnumberLt0(place))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t capacity = fklVMgetUint(place);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, str);
    fklStringBufferReverse(buf, capacity);
    FKL_VM_PUSH_VALUE(exe, str);
    return 0;
}

static int export_strbuf_capacity(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, obj);
    FKL_VM_PUSH_VALUE(exe, fklMakeVMuint(buf->size, exe));
    return 0;
}

static int export_strbuf_empty(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, obj);
    FKL_VM_PUSH_VALUE(exe, buf->index == 0 ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int export_strbuf_fmt(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG2(buf_obj, fmt_obj, exe);
    FKL_CHECK_TYPE(fmt_obj, FKL_IS_STR, exe);
    if (!is_strbuf_ud(buf_obj)
        || (!FKL_IS_STR(fmt_obj) && !is_strbuf_ud(fmt_obj)))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    uint64_t len = 0;
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, buf_obj);
    FklBuiltinErrorType err_type;
    if (FKL_IS_STR(fmt_obj))
        err_type = fklVMformat(exe, buf, FKL_VM_STR(fmt_obj), &len);
    else {
        FKL_DECL_VM_UD_DATA(fmt_buf, FklStringBuffer, fmt_obj);
        err_type = fklVMformat2(exe, buf, fmt_buf->buf,
                                &fmt_buf->buf[fmt_buf->index], &len);
    }

    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    FKL_CHECK_REST_ARG(exe);
    FKL_VM_PUSH_VALUE(exe, buf_obj);
    return 0;
}

static int export_strbuf_shrink(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(buf_obj, exe);
    FklVMvalue *shrink_to_val = FKL_VM_POP_ARG(exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(buf_obj, is_strbuf_ud, exe);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, buf_obj);
    size_t shrink_to;
    if (shrink_to_val) {
        FKL_CHECK_TYPE(shrink_to_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(shrink_to_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        shrink_to = fklVMgetUint(shrink_to_val);
    } else
        shrink_to = buf->index + 1;
    if (shrink_to < buf->size && shrink_to > buf->index)
        fklStringBufferShrinkTo(buf, shrink_to);
    FKL_VM_PUSH_VALUE(exe, buf_obj);
    return 0;
}

static int export_strbuf_resize(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG2(buf_obj, resize_to, exe);
    FklVMvalue *content_obj = FKL_VM_POP_ARG(exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(buf_obj, is_strbuf_ud, exe);
    FKL_CHECK_TYPE(resize_to, fklIsVMint, exe);
    char content;
    if (content_obj) {
        FKL_CHECK_TYPE(content_obj, FKL_IS_CHR, exe);
        content = FKL_GET_CHR(content_obj);
    } else
        content = '\0';
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, buf_obj);
    if (fklIsVMnumberLt0(resize_to))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    fklStringBufferResize(buf, fklVMgetUint(resize_to), content);
    FKL_VM_PUSH_VALUE(exe, buf_obj);
    return 0;
}

static int export_substrbuf(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG3(ostr, vstart, vend, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(ostr, is_strbuf_ud, exe);
    FKL_CHECK_TYPE(vstart, fklIsVMint, exe);
    FKL_CHECK_TYPE(vend, fklIsVMint, exe);
    if (fklIsVMnumberLt0(vstart) || fklIsVMnumberLt0(vend))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, ostr);
    size_t size = buf->index;
    size_t start = fklVMgetUint(vstart);
    size_t end = fklVMgetUint(vend);
    if (start > size || end < start || end > size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    size = end - start;
    FklVMvalue *r =
        create_vmstrbuf2(exe, size, buf->buf + start, FKL_VM_UD(ostr)->rel);
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_sub_strbuf(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG3(ostr, vstart, vsize, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(ostr, is_strbuf_ud, exe);
    FKL_CHECK_TYPE(vstart, fklIsVMint, exe);
    FKL_CHECK_TYPE(vsize, fklIsVMint, exe);
    if (fklIsVMnumberLt0(vstart) || fklIsVMnumberLt0(vsize))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FKL_DECL_VM_UD_DATA(buf, FklStringBuffer, ostr);
    size_t size = buf->index;
    size_t start = fklVMgetUint(vstart);
    size_t osize = fklVMgetUint(vsize);
    if (start + osize > size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FklVMvalue *r =
        create_vmstrbuf2(exe, osize, buf->buf + start, FKL_VM_UD(ostr)->rel);
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

struct SymFunc {
    const char *sym;
    FklVMcFunc f;
} exports_and_func[] = {
    // clang-format off
    // strbuf
    {"strbuf?",              export_strbuf_p                  },
    {"strbuf",               export_strbuf                    },
    {"substrbuf",            export_substrbuf                 },
    {"sub-strbuf",           export_sub_strbuf                },
    {"make-strbuf",          export_make_strbuf               },
    {"make-strbuf/capacity", export_make_strbuf_with_capacity },
    {"strbuf-ref",           export_strbuf_ref                },
    {"strbuf-capacity",      export_strbuf_capacity           },
    {"strbuf-empty?",        export_strbuf_empty              },
    {"strbuf-set!",          export_strbuf_set1               },
    {"strbuf-clear!",        export_strbuf_clear              },
    {"strbuf-reserve!",      export_strbuf_reserve            },
    {"strbuf-shrink!",       export_strbuf_shrink             },
    {"strbuf-resize!",       export_strbuf_resize             },
    {"strbuf-fmt!",          export_strbuf_fmt                },
    {"strbuf->string",       export_strbuf_to_string          },
    {"strbuf->bytes",        export_strbuf_to_bytevector      },
    {"strbuf->vector",       export_strbuf_to_vector          },
    {"strbuf->list",         export_strbuf_to_list            },
    {"strbuf->symbol",       export_strbuf_to_symbol          },
    // clang-format on
};

static const size_t EXPORT_NUM =
    sizeof(exports_and_func) / sizeof(struct SymFunc);

FKL_DLL_EXPORT FklSid_t *
_fklExportSymbolInit(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS) {
    *num = EXPORT_NUM;
    FklSid_t *symbols = (FklSid_t *)malloc(sizeof(FklSid_t) * EXPORT_NUM);
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklAddSymbolCstr(exports_and_func[i].sym, st)->id;
    return symbols;
}

FKL_DLL_EXPORT FklVMvalue **_fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    *count = EXPORT_NUM;
    FklVMvalue **loc = (FklVMvalue **)malloc(sizeof(FklVMvalue *) * EXPORT_NUM);
    FKL_ASSERT(loc);
    fklVMacquireSt(exe->gc);
    FklSymbolTable *st = exe->gc->st;
    for (size_t i = 0; i < EXPORT_NUM; i++) {
        FklSid_t id = fklAddSymbolCstr(exports_and_func[i].sym, st)->id;
        FklVMcFunc func = exports_and_func[i].f;
        FklVMvalue *dlproc = fklCreateVMvalueCproc(exe, func, dll, NULL, id);
        loc[i] = dlproc;
    }
    fklVMreleaseSt(exe->gc);
    return loc;
}
