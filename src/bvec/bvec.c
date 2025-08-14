#include <fakeLisp/base.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#define BV_U_S_8_REF(TYPE)                                                     \
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);                                     \
    FklVMvalue *bvec = FKL_CPROC_GET_ARG(exe, ctx, 0);                         \
    FklVMvalue *place = FKL_CPROC_GET_ARG(exe, ctx, 1);                        \
    if (!fklIsVMint(place) || !FKL_IS_BYTEVECTOR(bvec))                        \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);            \
    if (fklIsVMnumberLt0(place))                                               \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);       \
    size_t index = fklVMgetUint(place);                                        \
    FklBytevector *bv = FKL_VM_BVEC(bvec);                                     \
    size_t size = bv->size;                                                    \
    if (index >= size || size - index < sizeof(TYPE))                          \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);                   \
    TYPE r = bv->ptr[index];                                                   \
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(r));                            \
    return 0;

#define BV_REF(TYPE, MAKER)                                                    \
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);                                     \
    FklVMvalue *bvec = FKL_CPROC_GET_ARG(exe, ctx, 0);                         \
    FklVMvalue *place = FKL_CPROC_GET_ARG(exe, ctx, 1);                        \
    if (!fklIsVMint(place) || !FKL_IS_BYTEVECTOR(bvec))                        \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);            \
    if (fklIsVMnumberLt0(place))                                               \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);       \
    size_t index = fklVMgetUint(place);                                        \
    FklBytevector *bv = FKL_VM_BVEC(bvec);                                     \
    size_t size = bv->size;                                                    \
    if (index >= size || size - index < sizeof(TYPE))                          \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);                   \
    TYPE r;                                                                    \
    memcpy(&r, &bv->ptr[index], sizeof(r));                                    \
    FKL_CPROC_RETURN(exe, ctx, MAKER(r, exe));                                 \
    return 0;

#define BV_S_REF(TYPE) BV_REF(TYPE, fklMakeVMint)
#define BV_U_REF(TYPE) BV_REF(TYPE, fklMakeVMuint)

static int export_bvs8ref(FKL_CPROC_ARGL) { BV_U_S_8_REF(int8_t) }
static int export_bvs16ref(FKL_CPROC_ARGL) { BV_S_REF(int16_t) }
static int export_bvs32ref(FKL_CPROC_ARGL) { BV_S_REF(int32_t) }
static int export_bvs64ref(FKL_CPROC_ARGL) { BV_S_REF(int64_t) }

static int export_bvu8ref(FKL_CPROC_ARGL) { BV_U_S_8_REF(uint8_t) }
static int export_bvu16ref(FKL_CPROC_ARGL) { BV_U_REF(uint16_t) }
static int export_bvu32ref(FKL_CPROC_ARGL) { BV_U_REF(uint32_t) }
static int export_bvu64ref(FKL_CPROC_ARGL){ BV_U_REF(uint64_t) }

#undef BV_REF
#undef BV_S_REF
#undef BV_U_REF
#undef BV_U_S_8_REF

#define BV_F_REF(TYPE)                                                         \
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);                                     \
    FklVMvalue *bvec = FKL_CPROC_GET_ARG(exe, ctx, 0);                         \
    FklVMvalue *place = FKL_CPROC_GET_ARG(exe, ctx, 1);                        \
    if (!fklIsVMint(place) || !FKL_IS_BYTEVECTOR(bvec))                        \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);            \
    if (fklIsVMnumberLt0(place))                                               \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);       \
    size_t index = fklVMgetUint(place);                                        \
    FklBytevector *bv = FKL_VM_BVEC(bvec);                                     \
    size_t size = bv->size;                                                    \
    if (index >= size || size - index < sizeof(TYPE))                          \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);                   \
    TYPE r;                                                                    \
    memcpy(&r, &bv->ptr[index], sizeof(r));                                    \
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueF64(exe, r));                   \
    return 0;

static int export_bvf32ref(FKL_CPROC_ARGL) {
    BV_F_REF(float)
}
static int export_bvf64ref(FKL_CPROC_ARGL){ BV_F_REF(double) }
#undef BV_F_REF

#define SET_BV_S_U_8_REF(TYPE)                                                 \
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);                                     \
    FklVMvalue *bvec = FKL_CPROC_GET_ARG(exe, ctx, 0);                         \
    FklVMvalue *place = FKL_CPROC_GET_ARG(exe, ctx, 1);                        \
    FklVMvalue *target = FKL_CPROC_GET_ARG(exe, ctx, 2);                       \
    if (!fklIsVMint(place) || !FKL_IS_BYTEVECTOR(bvec) || !fklIsVMint(target)) \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);            \
    if (fklIsVMnumberLt0(place))                                               \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);       \
    size_t index = fklVMgetUint(place);                                        \
    FklBytevector *bv = FKL_VM_BVEC(bvec);                                     \
    size_t size = bv->size;                                                    \
    if (index >= size || size - index < sizeof(TYPE))                          \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);                   \
    TYPE r = fklVMgetUint(target);                                             \
    bv->ptr[index] = r;                                                        \
    FKL_CPROC_RETURN(exe, ctx, target);                                        \
    return 0;

#define SET_BV_REF(TYPE)                                                       \
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);                                     \
    FklVMvalue *bvec = FKL_CPROC_GET_ARG(exe, ctx, 0);                         \
    FklVMvalue *place = FKL_CPROC_GET_ARG(exe, ctx, 1);                        \
    FklVMvalue *target = FKL_CPROC_GET_ARG(exe, ctx, 2);                       \
    if (!fklIsVMint(place) || !FKL_IS_BYTEVECTOR(bvec) || !fklIsVMint(target)) \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);            \
    if (fklIsVMnumberLt0(place))                                               \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);       \
    size_t index = fklVMgetUint(place);                                        \
    FklBytevector *bv = FKL_VM_BVEC(bvec);                                     \
    size_t size = bv->size;                                                    \
    if (index >= size || size - index < sizeof(TYPE))                          \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);                   \
    TYPE r = fklVMgetUint(target);                                             \
    memcpy(&bv->ptr[index], &r, sizeof(r));                                    \
    FKL_CPROC_RETURN(exe, ctx, target);                                        \
    return 0;

static int export_bvs8set1(FKL_CPROC_ARGL) {
    SET_BV_S_U_8_REF(int8_t)
}
static int export_bvs16set1(FKL_CPROC_ARGL) { SET_BV_REF(int16_t) }
static int export_bvs32set1(FKL_CPROC_ARGL) { SET_BV_REF(int32_t) }
static int export_bvs64set1(FKL_CPROC_ARGL) { SET_BV_REF(int64_t) }

static int export_bvu8set1(FKL_CPROC_ARGL) { SET_BV_S_U_8_REF(uint8_t) }
static int export_bvu16set1(FKL_CPROC_ARGL) { SET_BV_REF(uint16_t) }
static int export_bvu32set1(FKL_CPROC_ARGL) { SET_BV_REF(uint32_t) }
static int export_bvu64set1(FKL_CPROC_ARGL){ SET_BV_REF(uint64_t) }

#undef SET_BV_S_U_8_REF
#undef SET_BV_REF

#define SET_BV_F_REF(TYPE)                                                     \
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);                                     \
    FklVMvalue *bvec = FKL_CPROC_GET_ARG(exe, ctx, 0);                         \
    FklVMvalue *place = FKL_CPROC_GET_ARG(exe, ctx, 1);                        \
    FklVMvalue *target = FKL_CPROC_GET_ARG(exe, ctx, 2);                       \
    if (!fklIsVMint(place) || !FKL_IS_BYTEVECTOR(bvec) || !FKL_IS_F64(target)) \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);            \
    if (fklIsVMnumberLt0(place))                                               \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);       \
    size_t index = fklVMgetUint(place);                                        \
    FklBytevector *bv = FKL_VM_BVEC(bvec);                                     \
    size_t size = bv->size;                                                    \
    if (index >= size || size - index < sizeof(TYPE))                          \
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);                   \
    TYPE r = FKL_VM_F64(target);                                               \
    memcpy(&bv->ptr[index], &r, sizeof(r));                                    \
    FKL_CPROC_RETURN(exe, ctx, target);                                        \
    return 0;

static int export_bvf32set1(FKL_CPROC_ARGL) {
    SET_BV_F_REF(float)
}
static int export_bvf64set1(FKL_CPROC_ARGL) { SET_BV_F_REF(double) }
#undef SET_BV_F_REF

static int export_bytevector_to_s8_list(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, FKL_IS_BYTEVECTOR, exe);
    FklBytevector *bvec = FKL_VM_BVEC(obj);
    size_t size = bvec->size;
    int8_t *s8a = (int8_t *)bvec->ptr;
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **cur = &r;
    for (size_t i = 0; i < size; i++) {
        *cur = fklCreateVMvaluePairWithCar(exe, FKL_MAKE_VM_FIX(s8a[i]));
        cur = &FKL_VM_CDR(*cur);
    }
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_bytevector_to_u8_list(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, FKL_IS_BYTEVECTOR, exe);
    FklBytevector *bvec = FKL_VM_BVEC(obj);
    size_t size = bvec->size;
    uint8_t *u8a = bvec->ptr;
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **cur = &r;
    for (size_t i = 0; i < size; i++) {
        *cur = fklCreateVMvaluePairWithCar(exe, FKL_MAKE_VM_FIX(u8a[i]));
        cur = &FKL_VM_CDR(*cur);
    }
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int export_bytevector_to_s8_vector(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, FKL_IS_BYTEVECTOR, exe);
    FklBytevector *bvec = FKL_VM_BVEC(obj);
    size_t size = bvec->size;
    int8_t *s8a = (int8_t *)bvec->ptr;
    FklVMvalue *vec = fklCreateVMvalueVec(exe, size);
    FklVMvec *v = FKL_VM_VEC(vec);
    for (size_t i = 0; i < size; i++)
        v->base[i] = FKL_MAKE_VM_FIX(s8a[i]);
    FKL_CPROC_RETURN(exe, ctx, vec);
    return 0;
}

static int export_bytevector_to_u8_vector(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, FKL_IS_BYTEVECTOR, exe);
    FklBytevector *bvec = FKL_VM_BVEC(obj);
    size_t size = bvec->size;
    uint8_t *u8a = bvec->ptr;
    FklVMvalue *vec = fklCreateVMvalueVec(exe, size);
    FklVMvec *v = FKL_VM_VEC(vec);
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
    // bvec
    {"bvs8ref",         (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvs8ref",         export_bvs8ref                )},
    {"bvs16ref",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvs16ref",        export_bvs16ref               )},
    {"bvs32ref",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvs32ref",        export_bvs32ref               )},
    {"bvs64ref",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvs64ref",        export_bvs64ref               )},
    {"bvu8ref",         (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvu8ref",         export_bvu8ref                )},
    {"bvu16ref",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvu16ref",        export_bvu16ref               )},
    {"bvu32ref",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvu32ref",        export_bvu32ref               )},
    {"bvu64ref",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvu64ref",        export_bvu64ref               )},
    {"bvf32ref",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvf32ref",        export_bvf32ref               )},
    {"bvf64ref",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvf64ref",        export_bvf64ref               )},
    {"bvs8set!",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvs8set!",        export_bvs8set1               )},
    {"bvs16set!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvs16set!",       export_bvs16set1              )},
    {"bvs32set!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvs32set!",       export_bvs32set1              )},
    {"bvs64set!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvs64set!",       export_bvs64set1              )},
    {"bvu8set!",        (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvu8set!",        export_bvu8set1               )},
    {"bvu16set!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvu16set!",       export_bvu16set1              )},
    {"bvu32set!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvu32set!",       export_bvu32set1              )},
    {"bvu64set!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvu64set!",       export_bvu64set1              )},
    {"bvf32set!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvf32set!",       export_bvf32set1              )},
    {"bvf64set!",       (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvf64set!",       export_bvf64set1              )},
    {"bvec->s8-list",   (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvec->s8-list",   export_bytevector_to_s8_list  )},
    {"bvec->u8-list",   (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvec->u8-list",   export_bytevector_to_u8_list  )},
    {"bvec->s8-vector", (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvec->s8-vector", export_bytevector_to_s8_vector)},
    {"bvec->u8-vector", (const FklVMvalue*)&FKL_VM_CPROC_STATIC_INIT("bvec->u8-vector", export_bytevector_to_u8_vector)},
    // clang-format on
};

static const size_t EXPORT_NUM =
        sizeof(exports_and_func) / sizeof(struct SymFunc);

FKL_DLL_EXPORT FklSid_t *_fklExportSymbolInit(
        FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS) {
    *num = EXPORT_NUM;
    FklSid_t *symbols = (FklSid_t *)fklZmalloc(sizeof(FklSid_t) * EXPORT_NUM);
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklAddSymbolCstr(exports_and_func[i].sym, st)->v;
    return symbols;
}

FKL_DLL_EXPORT FklVMvalue **_fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    *count = EXPORT_NUM;
    FklVMvalue **loc =
            (FklVMvalue **)fklZmalloc(sizeof(FklVMvalue *) * EXPORT_NUM);
    FKL_ASSERT(loc);
    for (size_t i = 0; i < EXPORT_NUM; i++) {
        loc[i] = FKL_REMOVE_CONST(FklVMvalue, exports_and_func[i].v);
    }
    return loc;
}
