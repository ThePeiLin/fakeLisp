#include <fakeLisp/base.h>
#include <fakeLisp/code_builder.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <stdio.h>
#include <string.h>

FKL_VM_DEF_UD_STRUCT(FklVMvalueStrBuf, { FklStrBuf buf; });

static FklVMudMetaTable const StringBufferMetaTable;

static inline int is_strbuf_ud(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &StringBufferMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueStrBuf *as_strbuf(const FklVMvalue *v) {
    FKL_ASSERT(is_strbuf_ud(v));
    return FKL_TYPE_CAST(FklVMvalueStrBuf *, v);
}

static int strbuf_equal(const FklVMvalue *a, const FklVMvalue *b) {
    FklStrBuf *bufa = &as_strbuf(a)->buf;
    FklStrBuf *bufb = &as_strbuf(b)->buf;
    return bufa->index == bufb->index
        && !memcmp(bufa->buf, bufb->buf, bufa->index);
}

static int strbuf_finalize(FklVMvalue *p, FklVMgc *gc) {
    fklUninitStrBuf(&as_strbuf(p)->buf);
    return FKL_VM_UD_FINALIZE_NOW;
}

static void
strbuf_prin1(const FklVMvalue *ud, FklCodeBuilder *build, FklVM *exe) {
    FklStrBuf *bufa = &as_strbuf(ud)->buf;
    fklCodeBuilderPuts(build, "#<strbuf ");
    fklPrintBufLiteralExt(bufa->index, bufa->buf, "\"", "\"", '"', build);
    fklCodeBuilderPutc(build, '>');
}

static void
strbuf_princ(const FklVMvalue *ud, FklCodeBuilder *build, FklVM *exe) {
    FklStrBuf *bufa = &as_strbuf(ud)->buf;
    fklCodeBuilderWrite(build, bufa->index, bufa->buf);
}

static void strbuf_write(const FklVMvalue *ud, FklCodeBuilder *build) {
    FklStrBuf *buf = &as_strbuf(ud)->buf;
    fklCodeBuilderWrite(build, buf->index, buf->buf);
}

static int
strbuf_append(FklVMvalue *ud, uint32_t argc, FklVMvalue *const *base) {
    FklStrBuf *buf = &as_strbuf(ud)->buf;
    FklVMvalue *const *const end = base + argc;
    for (; base < end; ++base) {
        FklVMvalue *cur = *base;
        if (FKL_IS_CHR(cur))
            fklStrBufPutc(buf, FKL_GET_CHR(cur));
        else if (FKL_IS_STR(cur))
            fklStrBufConcatWithString(buf, FKL_VM_STR(cur));
        else if (is_strbuf_ud(cur)) {
            fklStrBufConcatWithStrBuf(buf, &as_strbuf(cur)->buf);
        } else
            return 1;
    }
    return 0;
}

static inline FklVMvalue *
create_vmstrbuf(FklVM *exe, size_t capacity, FklVMvalue *dll) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &StringBufferMetaTable, dll);
    fklInitStrBuf(&as_strbuf(r)->buf);
    fklStrBufReserve(&as_strbuf(r)->buf, capacity);
    return r;
}

static inline FklVMvalue *
create_vmstrbuf2(FklVM *exe, size_t size, const char *ptr, FklVMvalue *dll) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &StringBufferMetaTable, dll);
    FklStrBuf *buf = &as_strbuf(r)->buf;
    fklInitStrBuf(buf);
    fklStrBufReserve(buf, size);
    fklStrBufBincpy(buf, ptr, size);
    return r;
}

static FklVMvalue *strbuf_copy_append(FklVM *exe,
        const FklVMvalue *ud_,
        uint32_t argc,
        FklVMvalue *const *base) {
    FklVMvalueStrBuf *ud = as_strbuf(ud_);
    FklVMvalue *retval = create_vmstrbuf(exe, 0, ud->dll_);
    FklVMvalue *const *const end = base + argc;
    FklStrBuf *rbuf = &as_strbuf(retval)->buf;
    fklStrBufConcatWithStrBuf(rbuf, &ud->buf);
    for (; base < end; ++base) {
        FklVMvalue *cur = *base;
        if (FKL_IS_CHR(cur))
            fklStrBufPutc(rbuf, FKL_GET_CHR(cur));
        else if (FKL_IS_STR(cur))
            fklStrBufConcatWithString(rbuf, FKL_VM_STR(cur));
        else if (is_strbuf_ud(cur)) {
            fklStrBufConcatWithStrBuf(rbuf, &as_strbuf(cur)->buf);
        } else
            return NULL;
    }
    return retval;
}

static size_t strbuf_length(const FklVMvalue *ud) {
    FklStrBuf *buf = &as_strbuf(ud)->buf;
    return buf->index;
}

static uintptr_t strbuf_hash(const FklVMvalue *ud) {
    FklStrBuf *buf = &as_strbuf(ud)->buf;
    return fklCharBufHash(buf->buf, buf->size);
}

static int strbuf_cmp(const FklVMvalue *ud, const FklVMvalue *v, int *err) {
    FklStrBuf *buf = &as_strbuf(ud)->buf;
    if (FKL_IS_STR(v))
        return -fklStringCharBufCmp(FKL_VM_STR(v), buf->index, buf->buf);
    else if (is_strbuf_ud(v)) {
        return fklStrBufCmp(buf, &as_strbuf(v)->buf);
    } else
        *err = 1;
    return 0;
}

static FklVMudMetaTable const StringBufferMetaTable = {
    .size = sizeof(FklVMvalueStrBuf),
    .equal = strbuf_equal,
    .prin1 = strbuf_prin1,
    .princ = strbuf_princ,
    .write = strbuf_write,
    .append = strbuf_append,
    .copy_append = strbuf_copy_append,
    .length = strbuf_length,
    .hash = strbuf_hash,
    .cmp = strbuf_cmp,
    .finalize = strbuf_finalize,
};

static int export_strbuf_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CPROC_RETURN(exe, ctx, (is_strbuf_ud(val)) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int export_make_strbuf(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 0, 2);
    FklVMvalue *dll = FKL_VM_CPROC(ctx->proc)->dll;
    if (argc == 0) {
        FklVMvalue *v = create_vmstrbuf(exe, 0, dll);
        FKL_CPROC_RETURN(exe, ctx, v);
        return 0;
    }
    FklVMvalue *size = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(size, fklIsVMint, exe);
    FklVMvalue *content = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    if (fklIsVMnumberLt0(size))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t len = fklVMgetUint(size);
    FklVMvalue *r = create_vmstrbuf(exe, len, dll);
    FklStrBuf *buf = &as_strbuf(r)->buf;
    char ch = 0;
    if (content) {
        FKL_CHECK_TYPE(content, FKL_IS_CHR, exe);
        ch = FKL_GET_CHR(content);
    }
    memset(buf->buf, ch, len);
    buf->index = len;
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_make_strbuf_with_capacity(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *size = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(size, fklIsVMint, exe);
    if (fklIsVMnumberLt0(size))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t len = fklVMgetUint(size);
    FklVMvalue *v = create_vmstrbuf(exe, len, FKL_VM_CPROC(ctx->proc)->dll);
    FKL_CPROC_RETURN(exe, ctx, v);
    return 0;
}

static int export_strbuf(FKL_CPROC_ARGL) {
    FklVMvalue *dll = FKL_VM_CPROC(ctx->proc)->dll;
    if (argc == 1) {
        FklVMvalue *str_or_buf = FKL_CPROC_GET_ARG(exe, ctx, 0);
        if (FKL_IS_CHR(str_or_buf)) {
            char c = FKL_GET_CHR(str_or_buf);
            FklVMvalue *v = create_vmstrbuf2(exe, 1, &c, dll);
            FKL_CPROC_RETURN(exe, ctx, v);
        } else if (FKL_IS_STR(str_or_buf)) {
            const FklString *b = FKL_VM_STR(str_or_buf);
            FklVMvalue *v = create_vmstrbuf2(exe, b->size, b->str, dll);
            FKL_CPROC_RETURN(exe, ctx, v);
        } else if (is_strbuf_ud(str_or_buf)) {
            FklStrBuf *b = &as_strbuf(str_or_buf)->buf;
            FklVMvalue *v = create_vmstrbuf2(exe, b->index, b->buf, dll);
            FKL_CPROC_RETURN(exe, ctx, v);
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } else {
        FklVMvalue *r = create_vmstrbuf(exe, 0, dll);
        FklStrBuf *b = &as_strbuf(r)->buf;
        FklVMvalue **arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue **const arg_end = arg_base + argc;
        for (; arg_base < arg_end; ++arg_base) {
            FklVMvalue *cur = *arg_base;
            FKL_CHECK_TYPE(cur, FKL_IS_CHR, exe);
            fklStrBufPutc(b, FKL_GET_CHR(cur));
        }
        FKL_CPROC_RETURN(exe, ctx, r);
    }
    return 0;
}

static int export_strbuf_to_string(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *buf = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(buf, is_strbuf_ud, exe);
    FklStrBuf *b = &as_strbuf(buf)->buf;
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStr2(exe, b->index, b->buf));
    return 0;
}

static int export_strbuf_to_bytevector(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *buf = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(buf, is_strbuf_ud, exe);
    FklStrBuf *b = &as_strbuf(buf)->buf;
    FKL_CPROC_RETURN(exe,
            ctx,
            fklCreateVMvalueBvec2(exe,
                    b->index,
                    FKL_TYPE_CAST(const uint8_t *, b->buf)));
    return 0;
}

static int export_strbuf_to_list(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    FklStrBuf *buf = &as_strbuf(obj)->buf;
    size_t len = buf->index;
    const char *ptr = buf->buf;
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **cur = &r;
    for (size_t i = 0; i < len; i++) {
        *cur = fklCreateVMvaluePair1(exe, FKL_MAKE_VM_CHR(ptr[i]));
        cur = &FKL_VM_CDR(*cur);
    }
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_strbuf_to_vector(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    FklStrBuf *buf = &as_strbuf(obj)->buf;
    size_t len = buf->index;
    const char *ptr = buf->buf;
    FklVMvalue *r = fklCreateVMvalueVec(exe, len);
    FklVMvalueVec *vec = FKL_VM_VEC(r);
    for (size_t i = 0; i < len; i++)
        vec->base[i] = FKL_MAKE_VM_CHR(ptr[i]);
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_strbuf_to_symbol(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    FklStrBuf *buf = &as_strbuf(obj)->buf;
    FKL_CPROC_RETURN(exe,
            ctx,
            fklVMaddSymbolCharBuf(exe, buf->buf, buf->index));
    return 0;
}

static int export_strbuf_ref(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *str = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *place = FKL_CPROC_GET_ARG(exe, ctx, 1);
    if (!fklIsVMint(place) || !is_strbuf_ud(str))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (fklIsVMnumberLt0(place))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t index = fklVMgetUint(place);
    FklStrBuf *buf = &as_strbuf(str)->buf;
    if (index >= buf->index)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_CHR(buf->buf[index]));
    return 0;
}

static int export_strbuf_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
    FklVMvalue *str = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *place = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *target = FKL_CPROC_GET_ARG(exe, ctx, 2);
    if (!fklIsVMint(place) || !is_strbuf_ud(str))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (fklIsVMnumberLt0(place))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t index = fklVMgetUint(place);
    FklStrBuf *buf = &as_strbuf(str)->buf;
    if (index >= buf->index)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    if (!FKL_IS_CHR(target) && !fklIsVMint(target))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    buf->buf[index] =
            FKL_IS_CHR(target) ? FKL_GET_CHR(target) : fklVMgetInt(target);
    FKL_CPROC_RETURN(exe, ctx, target);
    return 0;
}

static int export_strbuf_clear(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    fklStrBufClear(&as_strbuf(obj)->buf);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int export_strbuf_reserve(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *str = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *place = FKL_CPROC_GET_ARG(exe, ctx, 1);
    if (!fklIsVMint(place) || !is_strbuf_ud(str))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (fklIsVMnumberLt0(place))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t capacity = fklVMgetUint(place);
    fklStrBufReserve(&as_strbuf(str)->buf, capacity);
    FKL_CPROC_RETURN(exe, ctx, str);
    return 0;
}

static int export_strbuf_capacity(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(as_strbuf(obj)->buf.size, exe));
    return 0;
}

static int export_strbuf_empty(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_strbuf_ud, exe);
    FKL_CPROC_RETURN(exe,
            ctx,
            as_strbuf(obj)->buf.index == 0 ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int export_strbuf_fmt(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, argc);
    FklVMvalue *buf_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fmt_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    if (!is_strbuf_ud(buf_obj)
            || (!FKL_IS_STR(fmt_obj) && !is_strbuf_ud(fmt_obj)))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    uint64_t len = 0;
    FklBuiltinErrorType err_type;
    FklVMvalue **start = &FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderStrBuf(&builder, &as_strbuf(buf_obj)->buf, NULL);
    if (FKL_IS_STR(fmt_obj)) {
        err_type = fklVMformat2(exe,
                &builder,
                FKL_VM_STR(fmt_obj),
                &len,
                argc - 2,
                start);
    } else {
        FklStrBuf *fmt_buf = &as_strbuf(fmt_obj)->buf;
        err_type = fklVMformat3(exe,
                &builder,
                fmt_buf->index,
                fmt_buf->buf,
                &len,
                argc - 2,
                start);
    }

    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    FKL_CPROC_RETURN(exe, ctx, buf_obj);
    return 0;
}

static int export_strbuf_shrink(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *buf_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *shrink_to_val =
            argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(buf_obj, is_strbuf_ud, exe);
    FklStrBuf *buf = &as_strbuf(buf_obj)->buf;
    size_t shrink_to;
    if (shrink_to_val) {
        FKL_CHECK_TYPE(shrink_to_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(shrink_to_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        shrink_to = fklVMgetUint(shrink_to_val);
    } else
        shrink_to = buf->index + 1;
    if (shrink_to < buf->size && shrink_to > buf->index)
        fklStrBufShrinkTo(buf, shrink_to);
    FKL_CPROC_RETURN(exe, ctx, buf_obj);
    return 0;
}

static int export_strbuf_resize(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *buf_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *resize_to = FKL_CPROC_GET_ARG(exe, ctx, 1);

    FklVMvalue *content_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(buf_obj, is_strbuf_ud, exe);
    FKL_CHECK_TYPE(resize_to, fklIsVMint, exe);
    char content;
    if (content_obj) {
        FKL_CHECK_TYPE(content_obj, FKL_IS_CHR, exe);
        content = FKL_GET_CHR(content_obj);
    } else
        content = '\0';
    FklStrBuf *buf = &as_strbuf(buf_obj)->buf;
    if (fklIsVMnumberLt0(resize_to))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    fklStrBufResize(buf, fklVMgetUint(resize_to), content);
    FKL_CPROC_RETURN(exe, ctx, buf_obj);
    return 0;
}

static int export_substrbuf(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
    FklVMvalue *ostr = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *vstart = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *vend = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FKL_CHECK_TYPE(ostr, is_strbuf_ud, exe);
    FKL_CHECK_TYPE(vstart, fklIsVMint, exe);
    FKL_CHECK_TYPE(vend, fklIsVMint, exe);
    if (fklIsVMnumberLt0(vstart) || fklIsVMnumberLt0(vend))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FklStrBuf *buf = &as_strbuf(ostr)->buf;
    size_t size = buf->index;
    size_t start = fklVMgetUint(vstart);
    size_t end = fklVMgetUint(vend);
    if (start > size || end < start || end > size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    size = end - start;
    FklVMvalue *dll = FKL_VM_CPROC(ctx->proc)->dll;
    FklVMvalue *r = create_vmstrbuf2(exe, size, buf->buf + start, dll);
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_sub_strbuf(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
    FklVMvalue *ostr = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *vstart = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *vsize = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FKL_CHECK_TYPE(ostr, is_strbuf_ud, exe);
    FKL_CHECK_TYPE(vstart, fklIsVMint, exe);
    FKL_CHECK_TYPE(vsize, fklIsVMint, exe);
    if (fklIsVMnumberLt0(vstart) || fklIsVMnumberLt0(vsize))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FklStrBuf *buf = &as_strbuf(ostr)->buf;
    size_t size = buf->index;
    size_t start = fklVMgetUint(vstart);
    size_t osize = fklVMgetUint(vsize);
    if (start + osize > size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FklVMvalue *dll = FKL_VM_CPROC(ctx->proc)->dll;
    FklVMvalue *r = create_vmstrbuf2(exe, osize, buf->buf + start, dll);
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

struct SymFunc {
    const char *sym;
    const FklVMvalue *v;
} exports_and_func[] = {
    // clang-format off
    // strbuf
    {"strbuf?",              (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf?",              export_strbuf_p                 )},
    {"strbuf",               (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf",               export_strbuf                   )},
    {"substrbuf",            (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("substrbuf",            export_substrbuf                )},
    {"sub-strbuf",           (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("sub-strbuf",           export_sub_strbuf               )},
    {"make-strbuf",          (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("make-strbuf",          export_make_strbuf              )},
    {"make-strbuf/capacity", (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("make-strbuf/capacity", export_make_strbuf_with_capacity)},
    {"strbuf-ref",           (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf-ref",           export_strbuf_ref               )},
    {"strbuf-capacity",      (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf-capacity",      export_strbuf_capacity          )},
    {"strbuf-empty?",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf-empty?",        export_strbuf_empty             )},
    {"strbuf-set!",          (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf-set!",          export_strbuf_set1              )},
    {"strbuf-clear!",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf-clear!",        export_strbuf_clear             )},
    {"strbuf-reserve!",      (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf-reserve!",      export_strbuf_reserve           )},
    {"strbuf-shrink!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf-shrink!",       export_strbuf_shrink            )},
    {"strbuf-resize!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf-resize!",       export_strbuf_resize            )},
    {"strbuf-fmt!",          (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf-fmt!",          export_strbuf_fmt               )},
    {"strbuf->string",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf->string",       export_strbuf_to_string         )},
    {"strbuf->bytes",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf->bytes",        export_strbuf_to_bytevector     )},
    {"strbuf->vector",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf->vector",       export_strbuf_to_vector         )},
    {"strbuf->list",         (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf->list",         export_strbuf_to_list           )},
    {"strbuf->symbol",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("strbuf->symbol",       export_strbuf_to_symbol         )},
    // clang-format on
};

static const size_t EXPORT_NUM =
        sizeof(exports_and_func) / sizeof(struct SymFunc);

FKL_DLL_EXPORT FklVMvalue **_fklExportSymbolInit(FklVM *vm, uint32_t *num) {
    *num = EXPORT_NUM;
    FklVMvalue **symbols =
            (FklVMvalue **)fklZmalloc(EXPORT_NUM * sizeof(FklVMvalue *));
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklVMaddSymbolCstr(vm, exports_and_func[i].sym);
    return symbols;
}

FKL_DLL_EXPORT int _fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    FKL_ASSERT(count == EXPORT_NUM);
    for (size_t i = 0; i < EXPORT_NUM; i++) {
        FklVMvalue *r = NULL;
        const FklVMvalue *v = exports_and_func[i].v;
        if (FKL_IS_CPROC(v)) {
            const FklVMvalueCproc *from = FKL_VM_CPROC(v);
            r = fklCreateVMvalueCproc(exe, from->func, dll, NULL, from->name);
        }
        FKL_ASSERT(r);

        values[i] = r;
    }
    return 0;
}

FKL_CHECK_EXPORT_DLL_INIT_FUNC();
FKL_CHECK_IMPORT_DLL_INIT_FUNC();
