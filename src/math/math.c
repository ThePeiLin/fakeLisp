#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#define _USE_MATH_DEFINES
#include <float.h>
#include <math.h>

#define TRIM64(X) ((X) & 0xffffffffffffffffu)

static inline uint64_t rotl(uint64_t x, int n) {
    return (x << n) | (TRIM64(x) >> (64 - n));
}

static inline uint64_t next_rand(uint64_t s[4]) {
    uint64_t s0 = s[0];
    uint64_t s1 = s[1];
    uint64_t s2 = s[2] ^ s0;
    uint64_t s3 = s[3] ^ s1;
    uint64_t res = rotl(s1 * 5, 7) * 9;
    s[0] = s0 ^ s3;
    s[1] = s1 ^ s2;
    s[2] = s2 ^ (s1 << 17);
    s[3] = rotl(s3, 45);
    return res;
}

static inline void set_seed(uint64_t s[4], uint64_t n1, uint64_t n2) {
    s[0] = (uint64_t)n1;
    s[1] = 0xff;
    s[2] = (uint64_t)n2;
    s[3] = 0;
    for (int i = 0; i < 16; i++)
        next_rand(s);
}

static int math_srand(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 0, 2);
    FklVMvalue *x = argc > 0 ? FKL_CPROC_GET_ARG(exe, ctx, 0) : NULL;
    FklVMvalue *y = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    int64_t seed1 = 0;
    int64_t seed2 = 0;
    if (x) {
        FKL_CHECK_TYPE(x, fklIsVMint, exe);
        seed1 = fklVMgetInt(x);
        if (y) {
            FKL_CHECK_TYPE(y, fklIsVMint, exe);
            seed2 = fklVMgetInt(y);
        }
        set_seed(exe->rand_state, seed1, seed2);
    } else {
        seed1 = (int64_t)time(NULL);
        seed2 = (int64_t)(uintptr_t)exe;
        set_seed(exe->rand_state, seed1, seed2);
    }
    FKL_CPROC_RETURN(exe,
            ctx,
            fklCreateVMvaluePair(exe,
                    fklMakeVMint(seed1, exe),
                    fklMakeVMint(seed2, exe)));
    return 0;
}

#define FIGS DBL_MANT_DIG
#define SHIFT64_FIG (64 - FIGS)
#define SCALE_FIG (0.5 / ((uint64_t)1 << (FIGS - 1)))
#define INT_TO_DOUBLE(X) ((double)(TRIM64(X) >> SHIFT64_FIG) * SCALE_FIG)

static inline uint64_t project(uint64_t rv, uint64_t n, uint64_t s[4]) {
    if ((n & (n + 1)) == 0)
        return rv & n;
    else {
        uint64_t lim = n;
        lim |= (lim >> 1);
        lim |= (lim >> 2);
        lim |= (lim >> 4);
        lim |= (lim >> 8);
        lim |= (lim >> 16);
        lim |= (lim >> 32);
        FKL_ASSERT((lim & (lim + 1)) == 0 && lim >= n && (lim >> 1) < n);
        while ((rv &= lim) > n)
            rv = next_rand(s);
        return rv;
    }
}

static int math_rand(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 0, 2);
    uint64_t rv = next_rand(exe->rand_state);
    int64_t low = 0;
    int64_t up = 0;
    switch (argc) {
    case 0:
        FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueF64(exe, INT_TO_DOUBLE(rv)));
        return 0;
        break;
    case 1: {
        low = 1;
        FklVMvalue *up_v = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FKL_CHECK_TYPE(up_v, fklIsVMint, exe);
        up = fklVMgetInt(up_v);
        if (up == 0) {
            FKL_CPROC_RETURN(exe, ctx, fklMakeVMint(TRIM64(rv), exe));
            return 0;
        }
    } break;
    case 2: {
        FklVMvalue *low_v = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *up_v = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FKL_CHECK_TYPE(low_v, fklIsVMint, exe);
        FKL_CHECK_TYPE(up_v, fklIsVMint, exe);
        low = fklVMgetInt(low_v);
        up = fklVMgetInt(up_v);
    } break;
    default:
        fprintf(stderr,
                "[%s: %d] %s: unreachable!\n",
                __FILE__,
                __LINE__,
                __FUNCTION__);
        abort();
        break;
    }
    if (low > up)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    uint64_t p = project(rv,
            FKL_TYPE_CAST(uint64_t, up) - FKL_TYPE_CAST(uint64_t, low),
            exe->rand_state);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMint(p + (uint64_t)low, exe));
    return 0;
}

static int math_abs(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(obj, fklIsVMnumber, exe);
    if (FKL_IS_F64(obj)) {
        FKL_CPROC_RETURN(exe,
                ctx,
                fklCreateVMvalueF64(exe, fabs(FKL_VM_F64(obj))));
    } else if (FKL_IS_FIX(obj)) {
        int64_t i = fklAbs(FKL_GET_FIX(obj));
        FKL_CPROC_RETURN(exe,
                ctx,
                i > FKL_FIX_INT_MAX ? fklCreateVMvalueBigIntWithI64(exe, i)
                                    : FKL_MAKE_VM_FIX(i));
    } else {
        FklVMvalue *r = fklCreateVMvalueBigIntWithOther(exe, FKL_VM_BI(obj));
        FKL_VM_BI(r)->num = fklAbs(FKL_VM_BI(r)->num);
        FKL_CPROC_RETURN(exe, ctx, r);
    }
    return 0;
}

static int math_odd_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(val, fklIsVMint, exe);
    int r;
    if (FKL_IS_FIX(val))
        r = FKL_GET_FIX(val) % 2;
    else
        r = fklIsVMbigIntOdd(FKL_VM_BI(val));
    FKL_CPROC_RETURN(exe, ctx, r ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int math_even_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(val, fklIsVMint, exe);
    int r;
    if (FKL_IS_FIX(val))
        r = FKL_GET_FIX(val) % 2 == 0;
    else
        r = fklIsVMbigIntEven(FKL_VM_BI(val));
    FKL_CPROC_RETURN(exe, ctx, r ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

#define TO_RAD(N) ((N) * (M_PI / 180.0))
#define TO_DEG(N) ((N) * (180.0 / M_PI))

#define SINGLE_ARG_MATH_FUNC(NAME, FUNC)                                       \
    static int math_##NAME(FKL_CPROC_ARGL) {                                   \
        FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);                                 \
        FklVMvalue *num = FKL_CPROC_GET_ARG(exe, ctx, 0);                      \
        FKL_CHECK_TYPE(num, fklIsVMnumber, exe);                               \
        FKL_CPROC_RETURN(exe,                                                  \
                ctx,                                                           \
                fklCreateVMvalueF64(exe, FUNC(fklVMgetDouble(num))));          \
        return 0;                                                              \
    }

SINGLE_ARG_MATH_FUNC(rad, TO_RAD);
SINGLE_ARG_MATH_FUNC(deg, TO_DEG);
SINGLE_ARG_MATH_FUNC(sqrt, sqrt);
SINGLE_ARG_MATH_FUNC(cbrt, cbrt);

SINGLE_ARG_MATH_FUNC(sin, sin);
SINGLE_ARG_MATH_FUNC(cos, cos);
SINGLE_ARG_MATH_FUNC(tan, tan);

SINGLE_ARG_MATH_FUNC(sinh, sinh);
SINGLE_ARG_MATH_FUNC(cosh, cosh);
SINGLE_ARG_MATH_FUNC(tanh, tanh);

SINGLE_ARG_MATH_FUNC(asin, asin);
SINGLE_ARG_MATH_FUNC(acos, acos);
SINGLE_ARG_MATH_FUNC(atan, atan);

SINGLE_ARG_MATH_FUNC(asinh, asinh);
SINGLE_ARG_MATH_FUNC(acosh, acosh);
SINGLE_ARG_MATH_FUNC(atanh, atanh);

SINGLE_ARG_MATH_FUNC(ceil, ceil);
SINGLE_ARG_MATH_FUNC(floor, floor);
SINGLE_ARG_MATH_FUNC(round, round);
SINGLE_ARG_MATH_FUNC(trunc, trunc);

SINGLE_ARG_MATH_FUNC(exp, exp);
SINGLE_ARG_MATH_FUNC(exp2, exp2);
SINGLE_ARG_MATH_FUNC(expm1, expm1);

SINGLE_ARG_MATH_FUNC(log2, log2);
SINGLE_ARG_MATH_FUNC(log10, log10);
SINGLE_ARG_MATH_FUNC(log1p, log1p);

SINGLE_ARG_MATH_FUNC(erf, erf);
SINGLE_ARG_MATH_FUNC(erfc, erfc);
SINGLE_ARG_MATH_FUNC(gamma, tgamma);
SINGLE_ARG_MATH_FUNC(lgamma, lgamma);

#undef SINGLE_ARG_MATH_FUNC

static int math_modf(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *num = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(num, fklIsVMnumber, exe);
    double cdr = 0.0;
    double car = modf(fklVMgetDouble(num), &cdr);
    FKL_CPROC_RETURN(exe,
            ctx,
            fklCreateVMvaluePair(exe,
                    fklCreateVMvalueF64(exe, car),
                    fklCreateVMvalueF64(exe, cdr)));
    return 0;
}

static int math_frexp(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *num = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(num, fklIsVMnumber, exe);
    int32_t cdr = 0;
    double car = frexp(fklVMgetDouble(num), &cdr);
    FKL_CPROC_RETURN(exe,
            ctx,
            fklCreateVMvaluePair(exe,
                    fklCreateVMvalueF64(exe, car),
                    FKL_MAKE_VM_FIX(cdr)));
    return 0;
}

static int math_log(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *x = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *base = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(x, fklIsVMnumber, exe);
    if (base) {
        FKL_CHECK_TYPE(base, fklIsVMnumber, exe);
        double b = fklVMgetDouble(base);
        if (!islessgreater(b, 2.0))
            FKL_CPROC_RETURN(exe,
                    ctx,
                    fklCreateVMvalueF64(exe, log2(fklVMgetDouble(x))));
        else if (!islessgreater(b, 10.0))
            FKL_CPROC_RETURN(exe,
                    ctx,
                    fklCreateVMvalueF64(exe, log10(fklVMgetDouble(x))));
        else
            FKL_CPROC_RETURN(exe,
                    ctx,
                    fklCreateVMvalueF64(exe,
                            log2(fklVMgetDouble(x)) / log2(b)));
    } else
        FKL_CPROC_RETURN(exe,
                ctx,
                fklCreateVMvalueF64(exe, log(fklVMgetDouble(x))));
    return 0;
}

#define DOUBLE_ARG_MATH_FUNC(NAME, ERROR_NAME, FUNC)                           \
    static int math_##NAME(FKL_CPROC_ARGL) {                                   \
        FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);                                 \
        FklVMvalue *x = FKL_CPROC_GET_ARG(exe, ctx, 0);                        \
        FklVMvalue *y = FKL_CPROC_GET_ARG(exe, ctx, 1);                        \
        FKL_CHECK_TYPE(x, fklIsVMnumber, exe);                                 \
        FKL_CHECK_TYPE(y, fklIsVMnumber, exe);                                 \
        FKL_CPROC_RETURN(exe,                                                  \
                ctx,                                                           \
                fklCreateVMvalueF64(exe,                                       \
                        FUNC(fklVMgetDouble(x), fklVMgetDouble(y))));          \
        return 0;                                                              \
    }

DOUBLE_ARG_MATH_FUNC(remainder, remainder, remainder);
DOUBLE_ARG_MATH_FUNC(hypot, hypot, hypot);
DOUBLE_ARG_MATH_FUNC(atan2, atan2, atan2);
DOUBLE_ARG_MATH_FUNC(pow, pow, pow);
DOUBLE_ARG_MATH_FUNC(ldexp, ldexp, ldexp);
DOUBLE_ARG_MATH_FUNC(nextafter, nextafter, nextafter);
DOUBLE_ARG_MATH_FUNC(copysign, copysign, copysign);

#undef DOUBLE_ARG_MATH_FUNC

#define PREDICATE_FUNC(NAME, FUNC)                                             \
    static int math_##NAME(FKL_CPROC_ARGL) {                                   \
        FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);                                 \
        FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);                      \
        FKL_CHECK_TYPE(val, fklIsVMnumber, exe);                               \
        FKL_CPROC_RETURN(exe,                                                  \
                ctx,                                                           \
                FUNC(fklVMgetDouble(val)) ? FKL_VM_TRUE : FKL_VM_NIL);         \
        return 0;                                                              \
    }

PREDICATE_FUNC(signbit, signbit);
PREDICATE_FUNC(nan_p, isnan);
PREDICATE_FUNC(finite_p, isfinite);
PREDICATE_FUNC(inf_p, isinf);
PREDICATE_FUNC(normal_p, isnormal);

#undef PREDICATE_FUNC

static int math_unordered_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *x = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *y = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(x, fklIsVMnumber, exe);
    FKL_CHECK_TYPE(y, fklIsVMnumber, exe);
    FKL_CPROC_RETURN(exe,
            ctx,
            isunordered(fklVMgetDouble(x), fklVMgetDouble(y)) ? FKL_VM_TRUE
                                                              : FKL_VM_NIL);
    return 0;
}

struct SymFunc {
    const char *sym;
    FklVMcFunc f;
} exports_and_func[] = {
    // clang-format off
    {"even?",      math_even_p      },
    {"odd?",       math_odd_p       },

    {"srand",      math_srand       },
    {"rand",       math_rand        },

    {"sqrt",       math_sqrt        },
    {"cbrt",       math_cbrt        },
    {"pow",        math_pow         },
    {"hypot",      math_hypot       },

    {"abs",        math_abs         },
    {"remainder",  math_remainder   },

    {"rad",        math_rad         },
    {"deg",        math_deg         },

    {"asin",       math_asin        },
    {"acos",       math_acos        },
    {"atan",       math_atan        },
    {"atan2",      math_atan2       },

    {"asinh",      math_asinh       },
    {"acosh",      math_acosh       },
    {"atanh",      math_atanh       },

    {"sinh",       math_sinh        },
    {"cosh",       math_cosh        },
    {"tanh",       math_tanh        },

    {"sin",        math_sin         },
    {"cos",        math_cos         },
    {"tan",        math_tan         },

    {"ceil",       math_ceil        },
    {"floor",      math_floor       },
    {"trunc",      math_trunc       },
    {"round",      math_round       },

    {"frexp",      math_frexp       },
    {"ldexp",      math_ldexp       },
    {"modf",       math_modf        },
    {"nextafter",  math_nextafter   },
    {"copysign",   math_copysign    },

    {"log",        math_log         },
    {"log2",       math_log2        },
    {"log10",      math_log10       },
    {"log1p",      math_log1p       },

    {"exp",        math_exp         },
    {"exp2",       math_exp2        },
    {"expm1",      math_expm1       },

    {"erf",        math_erf         },
    {"erfc",       math_erfc        },
    {"gamma",      math_gamma       },
    {"lgamma",     math_lgamma      },

    {"signbit",    math_signbit     },
    {"nan?",       math_nan_p       },
    {"finite?",    math_finite_p    },
    {"inf?",       math_inf_p       },
    {"normal?",    math_normal_p    },
    {"unordered?", math_unordered_p },

    {"NAN",        NULL             },
    {"INF",        NULL             },
    {"E",          NULL             },
    {"PI",         NULL             },
    {"PI/2",       NULL             },
    {"PI/4",       NULL             },
    {"1/PI",       NULL             },
    {"2/PI",       NULL             },
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
    size_t i = 0;
    fklVMacquireSt(exe->gc);
    FklSymbolTable *st = exe->gc->st;
    for (; i < EXPORT_NUM; i++) {
        FklVMcFunc func = exports_and_func[i].f;
        if (func == NULL)
            break;
        FklSid_t id = fklAddSymbolCstr(exports_and_func[i].sym, st)->v;
        FklVMvalue *dlproc = fklCreateVMvalueCproc(exe, func, dll, NULL, id);
        loc[i] = dlproc;
    }
    fklVMreleaseSt(exe->gc);

    loc[i++] = fklCreateVMvalueF64(exe, NAN);
    loc[i++] = fklCreateVMvalueF64(exe, INFINITY);
    loc[i++] = fklCreateVMvalueF64(exe, M_E);
    loc[i++] = fklCreateVMvalueF64(exe, M_PI);
    loc[i++] = fklCreateVMvalueF64(exe, M_PI_2);
    loc[i++] = fklCreateVMvalueF64(exe, M_PI_4);
    loc[i++] = fklCreateVMvalueF64(exe, M_1_PI);
    loc[i++] = fklCreateVMvalueF64(exe, M_2_PI);
    return loc;
}
