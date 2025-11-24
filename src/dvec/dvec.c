#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

static FklVMudMetaTable const DvecMetaTable;

FKL_VM_DEF_UD_STRUCT(FklVMvalueDvec, { FklValueVector vec; });

static inline int is_dvec_ud(const FklVMvalue *ud) {
    return FKL_IS_USERDATA(ud) && FKL_VM_UD(ud)->mt_ == &DvecMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueDvec *as_dvec(const FklVMvalue *v) {
    FKL_ASSERT(is_dvec_ud(v));
    return FKL_TYPE_CAST(FklVMvalueDvec *, v);
}

static int _dvec_equal(const FklVMvalue *a, const FklVMvalue *b) {
    FklValueVector *da = &as_dvec(a)->vec;
    FklValueVector *db = &as_dvec(b)->vec;
    // FKL_DECL_UD_DATA(da, FklValueVector, a);
    // FKL_DECL_UD_DATA(db, FklValueVector, b);
    if (da->size != db->size)
        return 0;
    for (size_t i = 0; i < da->size; i++)
        if (!fklVMvalueEqual(da->base[i], db->base[i]))
            return 0;
    return 1;
}

static void _dvec_atomic(const FklVMvalue *d, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(dd, FklValueVector, d);
    FklValueVector *dd = &as_dvec(d)->vec;
    for (size_t i = 0; i < dd->size; i++)
        fklVMgcToGray(dd->base[i], gc);
}

static size_t _dvec_length(const FklVMvalue *d) {
    // FKL_DECL_UD_DATA(dd, FklValueVector, d);
    FklValueVector *dd = &as_dvec(d)->vec;
    return dd->size;
}

static uintptr_t _dvec_hash(const FklVMvalue *ud) {
    // FKL_DECL_UD_DATA(vec, FklValueVector, ud);
    FklValueVector *vec = &as_dvec(ud)->vec;
    uintptr_t seed = vec->size;
    for (size_t i = 0; i < vec->size; ++i)
        seed = fklHashCombine(seed, fklVMvalueEqualHashv(vec->base[i]));
    return seed;
}

static int _dvec_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(vec, FklValueVector, ud);
    // fklValueVectorUninit(vec);
    fklValueVectorUninit(&as_dvec(ud)->vec);
    return FKL_VM_UD_FINALIZE_NOW;
}

static int
_dvec_append(FklVMvalue *ud, uint32_t argc, FklVMvalue *const *base) {
    // FKL_DECL_UD_DATA(dvec, FklValueVector, ud);
    FklValueVector *dvec = &as_dvec(ud)->vec;
    size_t new_size = dvec->size;
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        if (is_dvec_ud(cur)) {
            new_size += as_dvec(cur)->vec.size;
        } else if (FKL_IS_VECTOR(cur))
            new_size += FKL_VM_VEC(cur)->size;
        else
            return 1;
    }
    fklValueVectorReserve(dvec, new_size);
    new_size = dvec->size;
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        size_t ss;
        FklVMvalue **mem;
        if (is_dvec_ud(cur)) {
            FklValueVector *d = &as_dvec(cur)->vec;
            ss = d->size;
            mem = d->base;
        } else {
            ss = FKL_VM_VEC(cur)->size;
            mem = FKL_VM_VEC(cur)->base;
        }
        memcpy(&dvec->base[new_size], mem, ss * sizeof(FklVMvalue *));
        new_size += ss;
    }
    dvec->size = new_size;
    return 0;
}

static inline FklVMvalue *
create_dvec(FklVM *exe, size_t size, FklVMvalue *dll) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &DvecMetaTable, dll);
    FklValueVector *v = &as_dvec(r)->vec;
    fklValueVectorInit(v, size);
    v->size = size;
    return r;
}

static inline FklVMvalue *
create_dvec_with_capacity(FklVM *exe, size_t capacity, FklVMvalue *dll) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &DvecMetaTable, dll);
    fklValueVectorInit(&as_dvec(r)->vec, capacity);
    return r;
}

static inline FklVMvalue *
create_dvec2(FklVM *exe, size_t size, FklVMvalue *const *ptr, FklVMvalue *dll) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &DvecMetaTable, dll);
    FklValueVector *v = &as_dvec(r)->vec;
    fklValueVectorInit(v, size);
    v->size = size;
    if (size)
        memcpy(v->base, ptr, size * sizeof(FklVMvalue *));
    return r;
}

static FklVMvalue *_dvec_copy_append(FklVM *exe,
        const FklVMvalue *v_,
        uint32_t argc,
        FklVMvalue *const *base) {
    // FKL_DECL_UD_DATA(dvec, FklValueVector, v);
    FklVMvalueDvec *v = as_dvec(v_);
    FklValueVector *dvec = &v->vec;
    size_t new_size = dvec->size;
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        if (is_dvec_ud(cur)) {
            new_size += as_dvec(cur)->vec.size;
        } else if (FKL_IS_VECTOR(cur))
            new_size += FKL_VM_VEC(cur)->size;
        else
            return NULL;
    }
    FklVMvalue *new_vec_val = create_dvec(exe, new_size, v->dll_);
    FklValueVector *new_vec = &as_dvec(new_vec_val)->vec;
    new_size = dvec->size;
    if (new_vec->base)
        memcpy(new_vec->base, dvec->base, new_size * sizeof(FklVMvalue *));
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        size_t ss;
        FklVMvalue **mem;
        if (is_dvec_ud(cur)) {
            FklValueVector *d = &as_dvec(cur)->vec;
            ss = d->size;
            mem = d->base;
        } else {
            ss = FKL_VM_VEC(cur)->size;
            mem = FKL_VM_VEC(cur)->base;
        }
        memcpy(&new_vec->base[new_size], mem, ss * sizeof(FklVMvalue *));
        new_size += ss;
    }
    return new_vec_val;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(_dvec_print, "dvec");

static FklVMudMetaTable const DvecMetaTable = {
    // .size = sizeof(FklValueVector),
    .size = sizeof(FklVMvalueDvec),
    .equal = _dvec_equal,
    .prin1 = _dvec_print,
    .princ = _dvec_print,
    .append = _dvec_append,
    .copy_append = _dvec_copy_append,
    .length = _dvec_length,
    .hash = _dvec_hash,
    .atomic = _dvec_atomic,
    .finalize = _dvec_finalizer,
};

static int export_dvec_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CPROC_RETURN(exe, ctx, (is_dvec_ud(val)) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int export_dvec(FKL_CPROC_ARGL) {
    FklVMvalue *vec = create_dvec(exe, argc, FKL_VM_CPROC(ctx->proc)->dll);
    FklValueVector *v = &as_dvec(vec)->vec;
    FklVMvalue **arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    if (v->base) {
        for (uint32_t i = 0; i < argc; ++i)
            v->base[i] = arg_base[i];
    }
    FKL_CPROC_RETURN(exe, ctx, vec);
    return 0;
}

static int export_make_dvec(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *size = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(size, fklIsVMint, exe);
    FklVMvalue *content =
            argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : FKL_VM_NIL;
    if (fklIsVMnumberLt0(size))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t len = fklVMgetUint(size);
    FklVMvalue *r = create_dvec(exe, len, FKL_VM_CPROC(ctx->proc)->dll);
    FklValueVector *vec = &as_dvec(r)->vec;
    for (size_t i = 0; i < len; i++)
        vec->base[i] = content;
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_make_dvec_with_capacity(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *size = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(size, fklIsVMint, exe);
    if (fklIsVMnumberLt0(size))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t len = fklVMgetUint(size);
    FklVMvalue *r =
            create_dvec_with_capacity(exe, len, FKL_VM_CPROC(ctx->proc)->dll);
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_dvec_empty_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(obj)->vec;
    FKL_CPROC_RETURN(exe, ctx, v->size ? FKL_VM_NIL : FKL_VM_TRUE);
    return 0;
}

static int export_dvec_ref(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *place = FKL_CPROC_GET_ARG(exe, ctx, 1);
    if (!fklIsVMint(place) || !is_dvec_ud(vec))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (fklIsVMnumberLt0(place))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t index = fklVMgetUint(place);
    FklValueVector *v = &as_dvec(vec)->vec;
    if (index >= v->size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FKL_CPROC_RETURN(exe, ctx, v->base[index]);
    return 0;
}

static int export_dvec_set(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *place = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *target = FKL_CPROC_GET_ARG(exe, ctx, 2);
    if (!fklIsVMint(place) || !is_dvec_ud(vec))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (fklIsVMnumberLt0(place))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t index = fklVMgetUint(place);
    FklValueVector *v = &as_dvec(vec)->vec;
    size_t size = v->size;
    if (index >= size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    v->base[index] = target;
    FKL_CPROC_RETURN(exe, ctx, target);
    return 0;
}

static int export_dvec_to_vector(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(vec)->vec;
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueVec2(exe, v->size, v->base));
    return 0;
}

static int export_dvec_to_list(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(vec)->vec;
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **cur = &r;
    for (size_t i = 0; i < v->size; i++) {
        *cur = fklCreateVMvaluePairWithCar(exe, v->base[i]);
        cur = &FKL_VM_CDR(*cur);
    }
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_subdvec(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
    FklVMvalue *ovec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *vstart = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *vend = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FKL_CHECK_TYPE(ovec, is_dvec_ud, exe);
    FKL_CHECK_TYPE(vstart, fklIsVMint, exe);
    FKL_CHECK_TYPE(vend, fklIsVMint, exe);
    if (fklIsVMnumberLt0(vstart) || fklIsVMnumberLt0(vend))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FklValueVector *vec = &as_dvec(ovec)->vec;
    size_t size = vec->size;
    size_t start = fklVMgetUint(vstart);
    size_t end = fklVMgetUint(vend);
    if (start > size || end < start || end > size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    size = end - start;
    FklVMvalue *r = create_dvec2(exe,
            size,
            vec->base + start,
            FKL_VM_CPROC(ctx->proc)->dll);
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_sub_dvec(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
    FklVMvalue *ovec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *vstart = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *vsize = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FKL_CHECK_TYPE(ovec, is_dvec_ud, exe);
    FKL_CHECK_TYPE(vstart, fklIsVMint, exe);
    FKL_CHECK_TYPE(vsize, fklIsVMint, exe);
    if (fklIsVMnumberLt0(vstart) || fklIsVMnumberLt0(vsize))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FklValueVector *vec = &as_dvec(ovec)->vec;
    size_t size = vec->size;
    size_t start = fklVMgetUint(vstart);
    size_t osize = fklVMgetUint(vsize);
    if (start + osize > size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FklVMvalue *r = create_dvec2(exe,
            osize,
            vec->base + start,
            FKL_VM_CPROC(ctx->proc)->dll);
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_dvec_capacity(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, is_dvec_ud, exe);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(as_dvec(obj)->vec.capacity, exe));
    return 0;
}

static int export_dvec_first(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(vec)->vec;
    if (!v->size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FKL_CPROC_RETURN(exe, ctx, v->base[0]);
    return 0;
}

static int export_dvec_last(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(vec)->vec;
    if (!v->size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FKL_CPROC_RETURN(exe, ctx, v->base[v->size - 1]);
    return 0;
}

static int export_dvec_assign(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3)
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    switch (argc) {
    case 2: {
        FklVMvalue *another_vec_val = FKL_CPROC_GET_ARG(exe, ctx, 1);
        if (FKL_IS_VECTOR(another_vec_val)) {
            FklValueVector *v = &as_dvec(vec)->vec;
            FklVMvalueVec *another_vec = FKL_VM_VEC(another_vec_val);
            fklValueVectorReserve(v, another_vec->size);
            memcpy(v->base,
                    another_vec->base,
                    another_vec->size * sizeof(FklVMvalue *));
            v->size = another_vec->size;
        } else if (is_dvec_ud(another_vec_val)) {
            FklValueVector *v = &as_dvec(vec)->vec;
            FklValueVector *another_vec = &as_dvec(another_vec_val)->vec;
            fklValueVectorReserve(v, another_vec->size);
            memcpy(v->base,
                    another_vec->base,
                    another_vec->size * sizeof(FklVMvalue *));
            v->size = another_vec->size;
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } break;
    case 3: {
        FklVMvalue *count_val = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FklVMvalue *content = FKL_CPROC_GET_ARG(exe, ctx, 2);
        FKL_CHECK_TYPE(count_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(count_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t count = fklVMgetUint(count_val);
        FklValueVector *v = &as_dvec(vec)->vec;
        fklValueVectorReserve(v, count);
        for (size_t i = 0; i < count; i++)
            v->base[i] = content;
        v->size = count;
    } break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    FKL_CPROC_RETURN(exe, ctx, vec);
    return 0;
}

static int export_dvec_fill(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *content = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(vec)->vec;
    size_t size = v->size;
    for (size_t i = 0; i < size; i++)
        v->base[i] = content;
    FKL_CPROC_RETURN(exe, ctx, vec);
    return 0;
}

static int export_dvec_clear(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(vec)->vec;
    v->size = 0;
    FKL_CPROC_RETURN(exe, ctx, vec);
    return 0;
}

static int export_dvec_reserve(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *new_cap_val = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_CHECK_TYPE(new_cap_val, fklIsVMint, exe);
    if (fklIsVMnumberLt0(new_cap_val))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t new_cap = fklVMgetUint(new_cap_val);
    fklValueVectorReserve(&as_dvec(vec)->vec, new_cap);
    FKL_CPROC_RETURN(exe, ctx, vec);
    return 0;
}

static int export_dvec_shrink(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *shrink_to_val =
            argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(vec)->vec;
    size_t shrink_to;
    if (shrink_to_val) {
        FKL_CHECK_TYPE(shrink_to_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(shrink_to_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        shrink_to = fklVMgetUint(shrink_to_val);
    } else
        shrink_to = v->size;
    fklValueVectorShrink(v, shrink_to);
    FKL_CPROC_RETURN(exe, ctx, vec);
    return 0;
}

static int export_dvec_resize(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *new_size_val = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *content =
            argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : FKL_VM_NIL;
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_CHECK_TYPE(new_size_val, fklIsVMint, exe);
    if (fklIsVMnumberLt0(new_size_val))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t new_size = fklVMgetUint(new_size_val);
    fklValueVectorResize(&as_dvec(vec)->vec, new_size, &content);
    FKL_CPROC_RETURN(exe, ctx, vec);
    return 0;
}

static int export_dvec_push(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    fklValueVectorPushBack2(&as_dvec(vec)->vec, obj);
    FKL_CPROC_RETURN(exe, ctx, obj);
    return 0;
}

static int export_dvec_pop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(vec)->vec;
    if (!v->size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FKL_CPROC_RETURN(exe, ctx, v->base[--v->size]);
    return 0;
}

static int export_dvec_pop7(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(vec)->vec;
    if (v->size)
        FKL_CPROC_RETURN(exe,
                ctx,
                fklCreateVMvalueBox(exe, v->base[--v->size]));
    else
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int export_dvec_insert(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 5);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    switch (argc) {
    case 3: {
        FklVMvalue *idx_val = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
        FKL_CHECK_TYPE(idx_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(idx_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t idx = fklVMgetUint(idx_val);
        FklValueVector *v = &as_dvec(vec)->vec;
        if (idx > v->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        fklValueVectorReserve(v, v->size + 1);
        FklVMvalue **const end = &v->base[idx];
        for (FklVMvalue **last = &v->base[v->size]; last > end; last--)
            *last = last[-1];
        v->base[idx] = obj;
        v->size++;
    } break;
    case 4: {
        FklVMvalue *idx_val = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FklVMvalue *count_val = FKL_CPROC_GET_ARG(exe, ctx, 2);
        FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
        FKL_CHECK_TYPE(idx_val, fklIsVMint, exe);
        FKL_CHECK_TYPE(count_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(idx_val) || fklIsVMnumberLt0(count_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t idx = fklVMgetUint(idx_val);
        FklValueVector *v = &as_dvec(vec)->vec;
        if (idx > v->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        size_t count = fklVMgetUint(count_val);
        if (count > 0) {
            fklValueVectorReserve(v, v->size + count);

            FklVMvalue **const end = &v->base[idx + count - 1];
            for (FklVMvalue **last = &v->base[v->size + count - 1]; last > end;
                    last--)
                *last = last[-count];

            for (FklVMvalue **cur = &v->base[idx]; cur <= end; cur++)
                *cur = obj;
            v->size += count;
        }
    } break;
    case 5: {
        FklVMvalue *idx_val = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FklVMvalue *another_vec_val = FKL_CPROC_GET_ARG(exe, ctx, 2);
        FklVMvalue *start_idx_val = FKL_CPROC_GET_ARG(exe, ctx, 3);
        FklVMvalue *count_val = FKL_CPROC_GET_ARG(exe, ctx, 4);
        FKL_CHECK_TYPE(idx_val, fklIsVMint, exe);
        FKL_CHECK_TYPE(start_idx_val, fklIsVMint, exe);
        FKL_CHECK_TYPE(count_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(idx_val) || fklIsVMnumberLt0(start_idx_val)
                || fklIsVMnumberLt0(count_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t idx = fklVMgetUint(idx_val);
        FklValueVector *v = &as_dvec(vec)->vec;
        if (idx > v->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        FklVMvalue **src_mem = NULL;
        size_t start_idx = fklVMgetUint(start_idx_val);
        size_t count = fklVMgetUint(count_val);
        if (FKL_IS_VECTOR(another_vec_val)) {
            FklVMvalueVec *v = FKL_VM_VEC(another_vec_val);
            if (start_idx + count > v->size)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
            src_mem = &v->base[start_idx];
        } else if (is_dvec_ud(another_vec_val)) {
            FklValueVector *v = &as_dvec(another_vec_val)->vec;
            if (start_idx + count > v->size)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
            src_mem = &v->base[start_idx];
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        if (count > 0) {
            fklValueVectorReserve(v, v->size + count);
            FklVMvalue **const end = &v->base[idx + count - 1];
            for (FklVMvalue **last = &v->base[v->size + count - 1]; last > end;
                    last--)
                *last = last[-count];
            memcpy(&v->base[idx], src_mem, count * sizeof(FklVMvalue *));
            v->size += count;
        }
    } break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    FKL_CPROC_RETURN(exe, ctx, vec);
    return 0;
}

static int export_dvec_remove(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    switch (argc) {
    case 2: {
        FklVMvalue *index_val = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FKL_CHECK_TYPE(index_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(index_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t index = fklVMgetUint(index_val);
        FklValueVector *v = &as_dvec(vec)->vec;
        if (index >= v->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        FklVMvalue *r = v->base[index];
        v->size--;
        if (index < v->size) {
            FklVMvalue **const end = &v->base[v->size];
            for (FklVMvalue **cur = &v->base[index]; cur < end; cur++)
                *cur = cur[1];
        }
        FKL_CPROC_RETURN(exe, ctx, r);
    } break;
    case 3: {
        FklVMvalue *start_index_val = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FklVMvalue *size_val = FKL_CPROC_GET_ARG(exe, ctx, 2);
        FKL_CHECK_TYPE(start_index_val, fklIsVMint, exe);
        FKL_CHECK_TYPE(size_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(start_index_val) || fklIsVMnumberLt0(size_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t start_index = fklVMgetUint(start_index_val);
        size_t size = fklVMgetUint(size_val);
        FklValueVector *v = &as_dvec(vec)->vec;
        if (start_index + size > v->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        v->size -= size;
        FklVMvalue **cur = &v->base[start_index];
        FklVMvalue *r = fklCreateVMvalueVec2(exe, size, cur);
        if (start_index < v->size) {
            FklVMvalue **const end = &v->base[v->size];
            for (; cur < end; cur++)
                *cur = cur[size];
        }
        FKL_CPROC_RETURN(exe, ctx, r);
    } break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    return 0;
}

static int export_dvec_to_string(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(vec)->vec;
    size_t size = v->size;
    FklVMvalue *r = fklCreateVMvalueStr2(exe, size, NULL);
    FklString *str = FKL_VM_STR(r);
    for (size_t i = 0; i < size; i++) {
        FKL_CHECK_TYPE(v->base[i], FKL_IS_CHR, exe);
        str->str[i] = FKL_GET_CHR(v->base[i]);
    }
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_dvec_to_bytevector(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *vec = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FklValueVector *v = &as_dvec(vec)->vec;
    size_t size = v->size;
    FklVMvalue *r = fklCreateVMvalueBvec2(exe, v->size, NULL);
    FklVMvalue **base = v->base;
    uint8_t *ptr = FKL_VM_BVEC(r)->ptr;
    for (uint64_t i = 0; i < size; i++) {
        FklVMvalue *cur = base[i];
        FKL_CHECK_TYPE(cur, fklIsVMint, exe);
        ptr[i] = fklVMgetInt(cur);
    }
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_string_to_dvec(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, FKL_IS_STR, exe);
    FklString *str = FKL_VM_STR(obj);
    size_t len = str->size;
    FklVMvalue *r = create_dvec(exe, len, FKL_VM_CPROC(ctx->proc)->dll);
    FklValueVector *vec = &as_dvec(r)->vec;
    for (size_t i = 0; i < len; i++)
        vec->base[i] = FKL_MAKE_VM_CHR(str->str[i]);
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_vector_to_dvec(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, FKL_IS_VECTOR, exe);
    FklVMvalueVec *vec = FKL_VM_VEC(obj);
    FKL_CPROC_RETURN(exe,
            ctx,
            create_dvec2(exe,
                    vec->size,
                    vec->base,
                    FKL_VM_CPROC(ctx->proc)->dll));
    return 0;
}

static int export_list_to_dvec(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, fklIsList, exe);
    size_t len = fklVMlistLength(obj);
    FklVMvalue *r = create_dvec(exe, len, FKL_VM_CPROC(ctx->proc)->dll);
    if (len == 0)
        goto exit;
    FklValueVector *vec = &as_dvec(r)->vec;
    for (size_t i = 0; obj != FKL_VM_NIL; i++, obj = FKL_VM_CDR(obj))
        vec->base[i] = FKL_VM_CAR(obj);
exit:
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_bytevector_to_dvec(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, FKL_IS_BYTEVECTOR, exe);
    FklBytevector *bvec = FKL_VM_BVEC(obj);
    size_t size = bvec->size;
    uint8_t *u8a = bvec->ptr;
    FklVMvalue *vec = create_dvec(exe, size, FKL_VM_CPROC(ctx->proc)->dll);
    FklValueVector *v = &as_dvec(vec)->vec;
    for (size_t i = 0; i < size; i++)
        v->base[i] = FKL_MAKE_VM_FIX(u8a[i]);
    FKL_CPROC_RETURN(exe, ctx, vec);
    return 0;
}

struct SymFunc {
    const char *sym;
    const FklVMvalue *v;
} exports_and_func[] = {
    // clang-format off
    // dvec
    {"dvec?",              (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec?",              export_dvec_p                 )},
    {"dvec",               (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec",               export_dvec                   )},
    {"make-dvec",          (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("make-dvec",          export_make_dvec              )},
    {"make-dvec/capacity", (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("make-dvec/capacity", export_make_dvec_with_capacity)},
    {"subdvec",            (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("subdvec",            export_subdvec                )},
    {"sub-dvec",           (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("sub-dvec",           export_sub_dvec               )},
    {"dvec-empty?",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-empty?",        export_dvec_empty_p           )},
    {"dvec-capacity",      (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-capacity",      export_dvec_capacity          )},
    {"dvec-first",         (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-first",         export_dvec_first             )},
    {"dvec-last",          (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-last",          export_dvec_last              )},
    {"dvec-ref",           (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-ref",           export_dvec_ref               )},
    {"dvec-set!",          (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-set!",          export_dvec_set               )},
    {"dvec-assign!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-assign!",       export_dvec_assign            )},
    {"dvec-fill!",         (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-fill!",         export_dvec_fill              )},
    {"dvec-clear!",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-clear!",        export_dvec_clear             )},
    {"dvec-reserve!",      (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-reserve!",      export_dvec_reserve           )},
    {"dvec-shrink!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-shrink!",       export_dvec_shrink            )},
    {"dvec-resize!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-resize!",       export_dvec_resize            )},
    {"dvec-push!",         (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-push!",         export_dvec_push              )},
    {"dvec-pop!",          (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-pop!",          export_dvec_pop               )},
    {"dvec-pop&!",         (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-pop&!",         export_dvec_pop7              )},
    {"dvec-insert!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-insert!",       export_dvec_insert            )},
    {"dvec-remove!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec-remove!",       export_dvec_remove            )},
    {"dvec->vector",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec->vector",       export_dvec_to_vector         )},
    {"dvec->list",         (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec->list",         export_dvec_to_list           )},
    {"dvec->string",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec->string",       export_dvec_to_string         )},
    {"dvec->bytes",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("dvec->bytes",        export_dvec_to_bytevector     )},
    {"list->dvec",         (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("list->dvec",         export_list_to_dvec           )},
    {"vector->dvec",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("vector->dvec",       export_vector_to_dvec         )},
    {"string->dvec",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("string->dvec",       export_string_to_dvec         )},
    {"bytes->dvec",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bytes->dvec",        export_bytevector_to_dvec     )},
    // clang-format on
};

static const size_t EXPORT_NUM =
        sizeof(exports_and_func) / sizeof(struct SymFunc);

FKL_DLL_EXPORT FklVMvalue **_fklExportSymbolInit(FklVMgc *gc, uint32_t *num) {
    *num = EXPORT_NUM;
    FklVMvalue **symbols =
            (FklVMvalue **)fklZmalloc(EXPORT_NUM * sizeof(FklVMvalue *));
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklVMaddSymbolCstr(&gc->gcvm, exports_and_func[i].sym);
    // fklAddSymbolCstr(exports_and_func[i].sym, st);
    return symbols;
}

FKL_DLL_EXPORT FklVMvalue **_fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    *count = EXPORT_NUM;
    FklVMvalue **loc =
            (FklVMvalue **)fklZmalloc(sizeof(FklVMvalue *) * EXPORT_NUM);
    FKL_ASSERT(loc);
    for (size_t i = 0; i < EXPORT_NUM; i++) {
        loc[i] = FKL_TYPE_CAST(FklVMvalue *, exports_and_func[i].v);
    }
    return loc;
}
