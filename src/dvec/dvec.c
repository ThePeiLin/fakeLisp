#include <fakeLisp/vm.h>

#define PREDICATE(condition)                                                   \
    FKL_DECL_AND_CHECK_ARG(val, exe);                                          \
    FKL_CHECK_REST_ARG(exe);                                                   \
    FKL_VM_PUSH_VALUE(exe, (condition) ? FKL_VM_TRUE : FKL_VM_NIL);            \
    return 0;

static FklVMudMetaTable DvecMetaTable;

static inline int is_dvec_ud(FklVMvalue *ud) {
    return FKL_IS_USERDATA(ud) && FKL_VM_UD(ud)->t == &DvecMetaTable;
}

static int _dvec_equal(const FklVMud *a, const FklVMud *b) {
    FKL_DECL_UD_DATA(da, FklVMvalueVector, a);
    FKL_DECL_UD_DATA(db, FklVMvalueVector, b);
    if (da->size != db->size)
        return 0;
    for (size_t i = 0; i < da->size; i++)
        if (!fklVMvalueEqual(da->base[i], db->base[i]))
            return 0;
    return 1;
}

static void _dvec_atomic(const FklVMud *d, FklVMgc *gc) {
    FKL_DECL_UD_DATA(dd, FklVMvalueVector, d);
    for (size_t i = 0; i < dd->size; i++)
        fklVMgcToGray(dd->base[i], gc);
}

static size_t _dvec_length(const FklVMud *d) {
    FKL_DECL_UD_DATA(dd, FklVMvalueVector, d);
    return dd->size;
}

static uintptr_t _dvec_hash(const FklVMud *ud) {
    FKL_DECL_UD_DATA(vec, FklVMvalueVector, ud);
    uintptr_t seed = vec->size;
    for (size_t i = 0; i < vec->size; ++i)
        seed = fklHashCombine(seed, fklVMvalueEqualHashv(vec->base[i]));
    return seed;
}

static void _dvec_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(vec, FklVMvalueVector, ud);
    fklVMvalueVectorUninit(vec);
}

static int _dvec_append(FklVMud *ud, uint32_t argc, FklVMvalue *const *base) {
    FKL_DECL_UD_DATA(dvec, FklVMvalueVector, ud);
    size_t new_size = dvec->size;
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        if (is_dvec_ud(cur)) {
            FKL_DECL_VM_UD_DATA(d, FklVMvalueVector, cur);
            new_size += d->size;
        } else if (FKL_IS_VECTOR(cur))
            new_size += FKL_VM_VEC(cur)->size;
        else
            return 1;
    }
    fklVMvalueVectorReserve(dvec, new_size);
    new_size = dvec->size;
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        size_t ss;
        FklVMvalue **mem;
        if (is_dvec_ud(cur)) {
            FKL_DECL_VM_UD_DATA(d, FklVMvalueVector, cur);
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

static inline FklVMvalue *create_dvec(FklVM *exe, size_t size,
                                      FklVMvalue *dll) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &DvecMetaTable, dll);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, r);
    fklVMvalueVectorInit(v, size);
    v->size = size;
    return r;
}

static inline FklVMvalue *create_dvec_with_capacity(FklVM *exe, size_t capacity,
                                                    FklVMvalue *dll) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &DvecMetaTable, dll);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, r);
    fklVMvalueVectorInit(v, capacity);
    return r;
}

static inline FklVMvalue *
create_dvec2(FklVM *exe, size_t size, FklVMvalue *const *ptr, FklVMvalue *dll) {
    FklVMvalue *r = fklCreateVMvalueUd(exe, &DvecMetaTable, dll);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, r);
    fklVMvalueVectorInit(v, size);
    v->size = size;
    if (size)
        memcpy(v->base, ptr, size * sizeof(FklVMvalue *));
    return r;
}

static FklVMvalue *_dvec_copy_append(FklVM *exe, const FklVMud *v,
                                     uint32_t argc, FklVMvalue *const *base) {
    FKL_DECL_UD_DATA(dvec, FklVMvalueVector, v);
    size_t new_size = dvec->size;
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        if (is_dvec_ud(cur)) {
            FKL_DECL_VM_UD_DATA(d, FklVMvalueVector, cur);
            new_size += d->size;
        } else if (FKL_IS_VECTOR(cur))
            new_size += FKL_VM_VEC(cur)->size;
        else
            return NULL;
    }
    FklVMvalue *new_vec_val = create_dvec(exe, new_size, v->rel);
    FKL_DECL_VM_UD_DATA(new_vec, FklVMvalueVector, new_vec_val);
    new_size = dvec->size;
    if (new_vec->base)
        memcpy(new_vec->base, dvec->base, new_size * sizeof(FklVMvalue *));
    for (uint32_t i = 0; i < argc; ++i) {
        FklVMvalue *cur = base[i];
        size_t ss;
        FklVMvalue **mem;
        if (is_dvec_ud(cur)) {
            FKL_DECL_VM_UD_DATA(d, FklVMvalueVector, cur);
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

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(_dvec_as_print, dvec);

static FklVMudMetaTable DvecMetaTable = {
    .size = sizeof(FklVMvalueVector),
    .__equal = _dvec_equal,
    .__as_prin1 = _dvec_as_print,
    .__as_princ = _dvec_as_print,
    .__append = _dvec_append,
    .__copy_append = _dvec_copy_append,
    .__length = _dvec_length,
    .__hash = _dvec_hash,
    .__atomic = _dvec_atomic,
    .__finalizer = _dvec_finalizer,
};

static int export_dvec_p(FKL_CPROC_ARGL) { PREDICATE(is_dvec_ud(val)) }

static int export_dvec(FKL_CPROC_ARGL) {
    size_t size = exe->tp - exe->bp;
    FklVMvalue *vec = create_dvec(exe, size, FKL_VM_CPROC(ctx->proc)->dll);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    for (size_t i = 0; i < size; i++)
        v->base[i] = FKL_VM_POP_ARG(exe);
    fklResBp(exe);
    FKL_VM_PUSH_VALUE(exe, vec);
    return 0;
}

static int export_make_dvec(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(size, exe);
    FKL_CHECK_TYPE(size, fklIsVMint, exe);
    FklVMvalue *content = FKL_VM_POP_ARG(exe);
    FKL_CHECK_REST_ARG(exe);
    if (fklIsVMnumberLt0(size))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t len = fklVMgetUint(size);
    FklVMvalue *r = create_dvec(exe, len, FKL_VM_CPROC(ctx->proc)->dll);
    if (!content)
        content = FKL_VM_NIL;
    FKL_DECL_VM_UD_DATA(vec, FklVMvalueVector, r);
    for (size_t i = 0; i < len; i++)
        vec->base[i] = content;
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_make_dvec_with_capacity(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(size, exe);
    FKL_CHECK_TYPE(size, fklIsVMint, exe);
    FKL_CHECK_REST_ARG(exe);
    if (fklIsVMnumberLt0(size))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t len = fklVMgetUint(size);
    FklVMvalue *r =
        create_dvec_with_capacity(exe, len, FKL_VM_CPROC(ctx->proc)->dll);
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_dvec_empty_p(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, obj);
    FKL_VM_PUSH_VALUE(exe, v->size ? FKL_VM_NIL : FKL_VM_TRUE);
    return 0;
}

static int export_dvec_ref(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG2(vec, place, exe);
    FKL_CHECK_REST_ARG(exe);
    if (!fklIsVMint(place) || !is_dvec_ud(vec))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (fklIsVMnumberLt0(place))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t index = fklVMgetUint(place);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    if (index >= v->size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FKL_VM_PUSH_VALUE(exe, v->base[index]);
    return 0;
}

static int export_dvec_set(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG3(vec, place, target, exe);
    FKL_CHECK_REST_ARG(exe);
    if (!fklIsVMint(place) || !is_dvec_ud(vec))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (fklIsVMnumberLt0(place))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    size_t index = fklVMgetUint(place);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    size_t size = v->size;
    if (index >= size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    v->base[index] = target;
    FKL_VM_PUSH_VALUE(exe, target);
    return 0;
}

static int export_dvec_to_vector(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueVecWithPtr(exe, v->size, v->base));
    return 0;
}

static int export_dvec_to_list(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **cur = &r;
    for (size_t i = 0; i < v->size; i++) {
        *cur = fklCreateVMvaluePairWithCar(exe, v->base[i]);
        cur = &FKL_VM_CDR(*cur);
    }
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_subdvec(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG3(ovec, vstart, vend, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(ovec, is_dvec_ud, exe);
    FKL_CHECK_TYPE(vstart, fklIsVMint, exe);
    FKL_CHECK_TYPE(vend, fklIsVMint, exe);
    if (fklIsVMnumberLt0(vstart) || fklIsVMnumberLt0(vend))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FKL_DECL_VM_UD_DATA(vec, FklVMvalueVector, ovec);
    size_t size = vec->size;
    size_t start = fklVMgetUint(vstart);
    size_t end = fklVMgetUint(vend);
    if (start > size || end < start || end > size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    size = end - start;
    FklVMvalue *r = create_dvec2(exe, size, vec->base + start,
                                 FKL_VM_CPROC(ctx->proc)->dll);
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_sub_dvec(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG3(ovec, vstart, vsize, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(ovec, is_dvec_ud, exe);
    FKL_CHECK_TYPE(vstart, fklIsVMint, exe);
    FKL_CHECK_TYPE(vsize, fklIsVMint, exe);
    if (fklIsVMnumberLt0(vstart) || fklIsVMnumberLt0(vsize))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FKL_DECL_VM_UD_DATA(vec, FklVMvalueVector, ovec);
    size_t size = vec->size;
    size_t start = fklVMgetUint(vstart);
    size_t osize = fklVMgetUint(vsize);
    if (start + osize > size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FklVMvalue *r = create_dvec2(exe, osize, vec->base + start,
                                 FKL_VM_CPROC(ctx->proc)->dll);
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_dvec_capacity(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, obj);
    FKL_VM_PUSH_VALUE(exe, fklMakeVMuint(v->capacity, exe));
    return 0;
}

static int export_dvec_first(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    if (!v->size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FKL_VM_PUSH_VALUE(exe, v->base[0]);
    return 0;
}

static int export_dvec_last(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    if (!v->size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FKL_VM_PUSH_VALUE(exe, v->base[v->size - 1]);
    return 0;
}

static int export_dvec_assign(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    uint32_t arg_num = FKL_VM_GET_ARG_NUM(exe);
    switch (arg_num) {
    case 0:
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);
        break;
    case 1: {
        FklVMvalue *another_vec_val = FKL_VM_POP_ARG(exe);
        FKL_ASSERT(another_vec_val);
        fklResBp(exe);
        if (FKL_IS_VECTOR(another_vec_val)) {
            FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
            FklVMvec *another_vec = FKL_VM_VEC(another_vec_val);
            fklVMvalueVectorReserve(v, another_vec->size);
            memcpy(v->base, another_vec->base,
                   another_vec->size * sizeof(FklVMvalue *));
            v->size = another_vec->size;
        } else if (is_dvec_ud(another_vec_val)) {
            FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
            FKL_DECL_VM_UD_DATA(another_vec, FklVMvalueVector, another_vec_val);
            fklVMvalueVectorReserve(v, another_vec->size);
            memcpy(v->base, another_vec->base,
                   another_vec->size * sizeof(FklVMvalue *));
            v->size = another_vec->size;
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } break;
    case 2: {
        FklVMvalue *count_val = FKL_VM_POP_ARG(exe);
        FklVMvalue *content = FKL_VM_POP_ARG(exe);
        FKL_ASSERT(count_val && content);
        fklResBp(exe);
        FKL_CHECK_TYPE(count_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(count_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t count = fklVMgetUint(count_val);
        FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
        fklVMvalueVectorReserve(v, count);
        for (size_t i = 0; i < count; i++)
            v->base[i] = content;
        v->size = count;
    } break;
    default:
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, exe);
        break;
    }
    FKL_VM_PUSH_VALUE(exe, vec);
    return 0;
}

static int export_dvec_fill(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG2(vec, content, exe)
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    size_t size = v->size;
    for (size_t i = 0; i < size; i++)
        v->base[i] = content;
    FKL_VM_PUSH_VALUE(exe, vec);
    return 0;
}

static int export_dvec_clear(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    v->size = 0;
    FKL_VM_PUSH_VALUE(exe, vec);
    return 0;
}

static int export_dvec_reserve(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG2(vec, new_cap_val, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_CHECK_TYPE(new_cap_val, fklIsVMint, exe);
    if (fklIsVMnumberLt0(new_cap_val))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    size_t new_cap = fklVMgetUint(new_cap_val);
    fklVMvalueVectorReserve(v, new_cap);
    FKL_VM_PUSH_VALUE(exe, vec);
    return 0;
}

static int export_dvec_shrink(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FklVMvalue *shrink_to_val = FKL_VM_POP_ARG(exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    size_t shrink_to;
    if (shrink_to_val) {
        FKL_CHECK_TYPE(shrink_to_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(shrink_to_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        shrink_to = fklVMgetUint(shrink_to_val);
    } else
        shrink_to = v->size;
    fklVMvalueVectorShrink(v, shrink_to);
    FKL_VM_PUSH_VALUE(exe, vec);
    return 0;
}

static int export_dvec_resize(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG2(vec, new_size_val, exe);
    FklVMvalue *content = FKL_VM_POP_ARG(exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_CHECK_TYPE(new_size_val, fklIsVMint, exe);
    if (fklIsVMnumberLt0(new_size_val))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    size_t new_size = fklVMgetUint(new_size_val);
    if (!content)
        content = FKL_VM_NIL;
    fklVMvalueVectorResize(v, new_size, &content);
    FKL_VM_PUSH_VALUE(exe, vec);
    return 0;
}

static int export_dvec_push(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG2(vec, obj, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    fklVMvalueVectorPushBack2(v, obj);
    FKL_VM_PUSH_VALUE(exe, obj);
    return 0;
}

static int export_dvec_pop(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    if (!v->size)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    FKL_VM_PUSH_VALUE(exe, v->base[--v->size]);
    return 0;
}

static int export_dvec_pop7(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    if (v->size)
        FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueBox(exe, v->base[--v->size]));
    else
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
    return 0;
}

static int export_dvec_insert(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    uint32_t arg_num = FKL_VM_GET_ARG_NUM(exe);
    switch (arg_num) {
    case 0:
    case 1:
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);
        break;
    case 2: {
        FklVMvalue *idx_val = FKL_VM_POP_ARG(exe);
        FklVMvalue *obj = FKL_VM_POP_ARG(exe);
        FKL_ASSERT(idx_val && obj);
        fklResBp(exe);
        FKL_CHECK_TYPE(idx_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(idx_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t idx = fklVMgetUint(idx_val);
        FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
        if (idx > v->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        fklVMvalueVectorReserve(v, v->size + 1);
        FklVMvalue **const end = &v->base[idx];
        for (FklVMvalue **last = &v->base[v->size]; last > end; last--)
            *last = last[-1];
        v->base[idx] = obj;
        v->size++;
    } break;
    case 3: {
        FklVMvalue *idx_val = FKL_VM_POP_ARG(exe);
        FklVMvalue *count_val = FKL_VM_POP_ARG(exe);
        FklVMvalue *obj = FKL_VM_POP_ARG(exe);
        FKL_ASSERT(idx_val && count_val && obj);
        fklResBp(exe);
        FKL_CHECK_TYPE(idx_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(idx_val) || fklIsVMnumberLt0(count_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t idx = fklVMgetUint(idx_val);
        FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
        if (idx > v->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        size_t count = fklVMgetUint(count_val);
        if (count > 0) {
            fklVMvalueVectorReserve(v, v->size + count);

            FklVMvalue **const end = &v->base[idx + count - 1];
            for (FklVMvalue **last = &v->base[v->size + count - 1]; last > end;
                 last--)
                *last = last[-count];

            for (FklVMvalue **cur = &v->base[idx]; cur <= end; cur++)
                *cur = obj;
            v->size += count;
        }
    } break;
    case 4: {
        FklVMvalue *idx_val = FKL_VM_POP_ARG(exe);
        FklVMvalue *another_vec_val = FKL_VM_POP_ARG(exe);
        FklVMvalue *start_idx_val = FKL_VM_POP_ARG(exe);
        FklVMvalue *count_val = FKL_VM_POP_ARG(exe);
        FKL_ASSERT(idx_val && another_vec_val && start_idx_val && count_val);
        fklResBp(exe);
        FKL_CHECK_TYPE(idx_val, fklIsVMint, exe);
        FKL_CHECK_TYPE(start_idx_val, fklIsVMint, exe);
        FKL_CHECK_TYPE(count_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(idx_val) || fklIsVMnumberLt0(start_idx_val)
            || fklIsVMnumberLt0(count_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t idx = fklVMgetUint(idx_val);
        FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
        if (idx > v->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        FklVMvalue **src_mem = NULL;
        size_t start_idx = fklVMgetUint(start_idx_val);
        size_t count = fklVMgetUint(count_val);
        if (FKL_IS_VECTOR(another_vec_val)) {
            FklVMvec *v = FKL_VM_VEC(another_vec_val);
            if (start_idx + count > v->size)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
            src_mem = &v->base[start_idx];
        } else if (is_dvec_ud(another_vec_val)) {
            FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, another_vec_val);
            if (start_idx + count > v->size)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
            src_mem = &v->base[start_idx];
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        if (count > 0) {
            fklVMvalueVectorReserve(v, v->size + count);
            FklVMvalue **const end = &v->base[idx + count - 1];
            for (FklVMvalue **last = &v->base[v->size + count - 1]; last > end;
                 last--)
                *last = last[-count];
            memcpy(&v->base[idx], src_mem, count * sizeof(FklVMvalue *));
            v->size += count;
        }
    } break;
    }
    FKL_VM_PUSH_VALUE(exe, vec);
    return 0;
}

static int export_dvec_remove(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    uint32_t arg_num = FKL_VM_GET_ARG_NUM(exe);
    switch (arg_num) {
    case 0:
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);
        break;
    case 1: {
        FklVMvalue *index_val = FKL_VM_POP_ARG(exe);
        fklResBp(exe);
        FKL_CHECK_TYPE(index_val, fklIsVMint, exe);
        if (fklIsVMnumberLt0(index_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t index = fklVMgetUint(index_val);
        FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
        if (index >= v->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        FklVMvalue *r = v->base[index];
        v->size--;
        if (index < v->size) {
            FklVMvalue **const end = &v->base[v->size];
            for (FklVMvalue **cur = &v->base[index]; cur < end; cur++)
                *cur = cur[1];
        }
        FKL_VM_PUSH_VALUE(exe, r);
    } break;
    case 2: {
        FklVMvalue *start_index_val = FKL_VM_POP_ARG(exe);
        FklVMvalue *size_val = FKL_VM_POP_ARG(exe);
        fklResBp(exe);
        if (fklIsVMnumberLt0(start_index_val) || fklIsVMnumberLt0(size_val))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
        size_t start_index = fklVMgetUint(start_index_val);
        size_t size = fklVMgetUint(size_val);
        FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
        if (start_index + size > v->size)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
        v->size -= size;
        FklVMvalue **cur = &v->base[start_index];
        FklVMvalue *r = fklCreateVMvalueVecWithPtr(exe, size, cur);
        if (start_index < v->size) {
            FklVMvalue **const end = &v->base[v->size];
            for (; cur < end; cur++)
                *cur = cur[size];
        }
        FKL_VM_PUSH_VALUE(exe, r);
    } break;
    default:
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, exe);
        break;
    }
    return 0;
}

static int export_dvec_to_string(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    size_t size = v->size;
    FklVMvalue *r = fklCreateVMvalueStr2(exe, size, NULL);
    FklString *str = FKL_VM_STR(r);
    for (size_t i = 0; i < size; i++) {
        FKL_CHECK_TYPE(v->base[i], FKL_IS_CHR, exe);
        str->str[i] = FKL_GET_CHR(v->base[i]);
    }
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_dvec_to_bytevector(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(vec, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(vec, is_dvec_ud, exe);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    size_t size = v->size;
    FklVMvalue *r = fklCreateVMvalueBvec2(exe, v->size, NULL);
    FklVMvalue **base = v->base;
    uint8_t *ptr = FKL_VM_BVEC(r)->ptr;
    for (uint64_t i = 0; i < size; i++) {
        FklVMvalue *cur = base[i];
        FKL_CHECK_TYPE(cur, fklIsVMint, exe);
        ptr[i] = fklVMgetInt(cur);
    }
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_string_to_dvec(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe)
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, FKL_IS_STR, exe);
    FklString *str = FKL_VM_STR(obj);
    size_t len = str->size;
    FklVMvalue *r = create_dvec(exe, len, FKL_VM_CPROC(ctx->proc)->dll);
    FKL_DECL_VM_UD_DATA(vec, FklVMvalueVector, r);
    for (size_t i = 0; i < len; i++)
        vec->base[i] = FKL_MAKE_VM_CHR(str->str[i]);
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_vector_to_dvec(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, FKL_IS_VECTOR, exe);
    FklVMvec *vec = FKL_VM_VEC(obj);
    FKL_VM_PUSH_VALUE(exe, create_dvec2(exe, vec->size, vec->base,
                                        FKL_VM_CPROC(ctx->proc)->dll));
    return 0;
}

static int export_list_to_dvec(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, fklIsList, exe);
    size_t len = fklVMlistLength(obj);
    FklVMvalue *r = create_dvec(exe, len, FKL_VM_CPROC(ctx->proc)->dll);
    if (len == 0)
        goto exit;
    FKL_DECL_VM_UD_DATA(vec, FklVMvalueVector, r);
    for (size_t i = 0; obj != FKL_VM_NIL; i++, obj = FKL_VM_CDR(obj))
        vec->base[i] = FKL_VM_CAR(obj);
exit:
    FKL_VM_PUSH_VALUE(exe, r);
    return 0;
}

static int export_bytevector_to_dvec(FKL_CPROC_ARGL) {
    FKL_DECL_AND_CHECK_ARG(obj, exe);
    FKL_CHECK_REST_ARG(exe);
    FKL_CHECK_TYPE(obj, FKL_IS_BYTEVECTOR, exe);
    FklBytevector *bvec = FKL_VM_BVEC(obj);
    size_t size = bvec->size;
    uint8_t *u8a = bvec->ptr;
    FklVMvalue *vec = create_dvec(exe, size, FKL_VM_CPROC(ctx->proc)->dll);
    FKL_DECL_VM_UD_DATA(v, FklVMvalueVector, vec);
    for (size_t i = 0; i < size; i++)
        v->base[i] = FKL_MAKE_VM_FIX(u8a[i]);
    FKL_VM_PUSH_VALUE(exe, vec);
    return 0;
}

struct SymFunc {
    const char *sym;
    FklVMcFunc f;
} exports_and_func[] = {
    // clang-format off
    // dvec
    {"dvec?",              export_dvec_p                  },
    {"dvec",               export_dvec                    },
    {"make-dvec",          export_make_dvec               },
    {"make-dvec/capacity", export_make_dvec_with_capacity },
    {"subdvec",            export_subdvec                 },
    {"sub-dvec",           export_sub_dvec                },
    {"dvec-empty?",        export_dvec_empty_p            },
    {"dvec-capacity",      export_dvec_capacity           },
    {"dvec-first",         export_dvec_first              },
    {"dvec-last",          export_dvec_last               },
    {"dvec-ref",           export_dvec_ref                },
    {"dvec-set!",          export_dvec_set                },
    {"dvec-assign!",       export_dvec_assign             },
    {"dvec-fill!",         export_dvec_fill               },
    {"dvec-clear!",        export_dvec_clear              },
    {"dvec-reserve!",      export_dvec_reserve            },
    {"dvec-shrink!",       export_dvec_shrink             },
    {"dvec-resize!",       export_dvec_resize             },
    {"dvec-push!",         export_dvec_push               },
    {"dvec-pop!",          export_dvec_pop                },
    {"dvec-pop&!",         export_dvec_pop7               },
    {"dvec-insert!",       export_dvec_insert             },
    {"dvec-remove!",       export_dvec_remove             },
    {"dvec->vector",       export_dvec_to_vector          },
    {"dvec->list",         export_dvec_to_list            },
    {"dvec->string",       export_dvec_to_string          },
    {"dvec->bytes",        export_dvec_to_bytevector      },
    {"list->dvec",         export_list_to_dvec            },
    {"vector->dvec",       export_vector_to_dvec          },
    {"string->dvec",       export_string_to_dvec          },
    {"bytes->dvec",        export_bytevector_to_dvec      },
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
        symbols[i] = fklAddSymbolCstr(exports_and_func[i].sym, st)->v;
    return symbols;
}

FKL_DLL_EXPORT FklVMvalue **_fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    *count = EXPORT_NUM;
    FklVMvalue **loc = (FklVMvalue **)malloc(sizeof(FklVMvalue *) * EXPORT_NUM);
    FKL_ASSERT(loc);
    fklVMacquireSt(exe->gc);
    FklSymbolTable *st = exe->gc->st;
    for (size_t i = 0; i < EXPORT_NUM; i++) {
        FklSid_t id = fklAddSymbolCstr(exports_and_func[i].sym, st)->v;
        FklVMcFunc func = exports_and_func[i].f;
        FklVMvalue *dlproc = fklCreateVMvalueCproc(exe, func, dll, NULL, id);
        loc[i] = dlproc;
    }
    fklVMreleaseSt(exe->gc);
    return loc;
}
