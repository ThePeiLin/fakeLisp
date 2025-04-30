#include <fakeLisp/vm.h>

static int fs_facce_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, ctx, 1);
    FklVMvalue *filename = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(filename, FKL_IS_STR, exe);
    FKL_CPROC_RETURN(exe, ctx,
                     (fklIsAccessibleRegFile(FKL_VM_STR(filename)->str)
                      || fklIsAccessibleDirectory(FKL_VM_STR(filename)->str))
                         ? FKL_VM_TRUE
                         : FKL_VM_NIL);
    return 0;
}

static int fs_freg_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, ctx, 1);
    FklVMvalue *filename = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(filename, FKL_IS_STR, exe);
    FKL_CPROC_RETURN(exe, ctx,
                     fklIsAccessibleRegFile(FKL_VM_STR(filename)->str)
                         ? FKL_VM_TRUE
                         : FKL_VM_NIL);
    return 0;
}

static int fs_fdir_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, ctx, 1);
    FklVMvalue *filename = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(filename, FKL_IS_STR, exe);
    FKL_CPROC_RETURN(exe, ctx,
                     fklIsAccessibleDirectory(FKL_VM_STR(filename)->str)
                         ? FKL_VM_TRUE
                         : FKL_VM_NIL);
    return 0;
}

static int fs_freopen(FKL_CPROC_ARGL) {
    uint32_t const arg_num = FKL_CPROC_GET_ARG_NUM(exe, ctx);
    FKL_CPROC_CHECK_ARG_NUM2(exe, arg_num, 2, 3);
    FklVMvalue *stream = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *filename = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *mode = arg_num > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    if (!fklIsVMvalueFp(stream) || !FKL_IS_STR(filename)
        || (mode && !FKL_IS_STR(mode)))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    FklVMfp *vfp = FKL_VM_FP(stream);
    const char *modeStr = mode ? FKL_VM_STR(mode)->str : "r";
    FILE *fp = freopen(FKL_VM_STR(filename)->str, modeStr, vfp->fp);
    if (!fp)
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_FILEFAILURE, exe,
                                    "Failed for file: %s", filename);
    vfp->fp = fp;
    vfp->rw = fklGetVMfpRwFromCstr(modeStr);
    FKL_CPROC_RETURN(exe, ctx, stream);
    return 0;
}

static int fs_realpath(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, ctx, 1);
    FklVMvalue *filename = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(filename, FKL_IS_STR, exe);
    char *rp = fklRealpath(FKL_VM_STR(filename)->str);
    if (rp) {
        FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStrFromCstr(exe, rp));
        free(rp);
    } else
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int fs_relpath(FKL_CPROC_ARGL) {
    uint32_t const arg_num = FKL_CPROC_GET_ARG_NUM(exe, ctx);
    FKL_CPROC_CHECK_ARG_NUM2(exe, arg_num, 1, 2);
    FklVMvalue *path = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *start = arg_num > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(path, FKL_IS_STR, exe);

    if (start)
        FKL_CHECK_TYPE(start, FKL_IS_STR, exe);

    char *path_rp = fklRealpath(FKL_VM_STR(path)->str);
    if (!path_rp) {
        path_rp = fklStrCat(fklSysgetcwd(), FKL_PATH_SEPARATOR_STR);
        path_rp = fklStrCat(path_rp, FKL_VM_STR(path)->str);
    }

    char *start_rp =
        start ? fklRealpath(FKL_VM_STR(start)->str) : fklRealpath(".");
    if (!start_rp) {
        start_rp = fklStrCat(fklSysgetcwd(), FKL_PATH_SEPARATOR_STR);
        start_rp = fklStrCat(start_rp, FKL_VM_STR(start)->str);
    }

    char *relpath_cstr =
        fklRelpath(start_rp ? start_rp : FKL_VM_STR(start)->str,
                   path_rp ? path_rp : FKL_VM_STR(path)->str);

    if (relpath_cstr) {
        FKL_CPROC_RETURN(exe, ctx,
                         fklCreateVMvalueStrFromCstr(exe, relpath_cstr));
        free(relpath_cstr);
    } else
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    free(start_rp);
    free(path_rp);
    return 0;
}

static int fs_mkdir(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, ctx, 1);
    FklVMvalue *filename = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(filename, FKL_IS_STR, exe);
    if (fklMkdir(FKL_VM_STR(filename)->str))
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_FILEFAILURE, exe,
                                    "Failed to make dir: %s", filename);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static inline int isVMfpWritable(const FklVMvalue *fp) {
    return FKL_VM_FP(fp)->rw & FKL_VM_FP_W_MASK;
}

#define CHECK_FP_WRITABLE(V, E)                                                \
    if (!isVMfpWritable(V))                                                    \
    FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNSUPPORTED_OP, E)

#define CHECK_FP_OPEN(V, E)                                                    \
    if (!FKL_VM_FP(V)->fp)                                                     \
    FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, E)

static int fs_fprint(FKL_CPROC_ARGL) {
    uint32_t const arg_num = FKL_CPROC_GET_ARG_NUM(exe, ctx);
    FKL_CPROC_CHECK_ARG_NUM2(exe, arg_num, 1, -1);
    FklVMvalue **arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *f = arg_base[0];
    FKL_CHECK_TYPE(f, fklIsVMvalueFp, exe);
    CHECK_FP_OPEN(f, exe);
    CHECK_FP_WRITABLE(f, exe);
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **const arg_end = arg_base + arg_num;
    FILE *fp = FKL_VM_FP(f)->fp;
    for (++arg_base; arg_base < arg_end; ++arg_base) {
        r = *arg_base;
        fklPrincVMvalue(r, fp, exe->gc);
    }
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int fs_fprintln(FKL_CPROC_ARGL) {
    uint32_t const arg_num = FKL_CPROC_GET_ARG_NUM(exe, ctx);
    FKL_CPROC_CHECK_ARG_NUM2(exe, arg_num, 1, -1);
    FklVMvalue **arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *f = arg_base[0];
    FKL_CHECK_TYPE(f, fklIsVMvalueFp, exe);
    CHECK_FP_OPEN(f, exe);
    CHECK_FP_WRITABLE(f, exe);
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **const arg_end = arg_base + arg_num;
    FILE *fp = FKL_VM_FP(f)->fp;
    for (++arg_base; arg_base < arg_end; ++arg_base) {
        r = *arg_base;
        fklPrincVMvalue(r, fp, exe->gc);
    }
    fputc('\n', stdout);
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int fs_fprin1(FKL_CPROC_ARGL) {
    uint32_t const arg_num = FKL_CPROC_GET_ARG_NUM(exe, ctx);
    FKL_CPROC_CHECK_ARG_NUM2(exe, arg_num, 1, -1);
    FklVMvalue **arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *f = arg_base[0];
    FKL_CHECK_TYPE(f, fklIsVMvalueFp, exe);
    CHECK_FP_OPEN(f, exe);
    CHECK_FP_WRITABLE(f, exe);
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **const arg_end = arg_base + arg_num;
    FILE *fp = FKL_VM_FP(f)->fp;
    for (++arg_base; arg_base < arg_end; ++arg_base) {
        r = *arg_base;
        fklPrin1VMvalue(r, fp, exe->gc);
    }
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int fs_fprin1n(FKL_CPROC_ARGL) {
    uint32_t const arg_num = FKL_CPROC_GET_ARG_NUM(exe, ctx);
    FKL_CPROC_CHECK_ARG_NUM2(exe, arg_num, 1, -1);
    FklVMvalue **arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *f = arg_base[0];
    FKL_CHECK_TYPE(f, fklIsVMvalueFp, exe);
    CHECK_FP_OPEN(f, exe);
    CHECK_FP_WRITABLE(f, exe);
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **const arg_end = arg_base + arg_num;
    FILE *fp = FKL_VM_FP(f)->fp;
    for (++arg_base; arg_base < arg_end; ++arg_base) {
        r = *arg_base;
        fklPrin1VMvalue(r, fp, exe->gc);
    }
    fputc('\n', stdout);
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int fs_fwrite(FKL_CPROC_ARGL) {
    uint32_t const arg_num = FKL_CPROC_GET_ARG_NUM(exe, ctx);
    FKL_CPROC_CHECK_ARG_NUM2(exe, arg_num, 1, -1);
    FklVMvalue **arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *f = arg_base[0];
    FKL_CHECK_TYPE(f, fklIsVMvalueFp, exe);
    CHECK_FP_OPEN(f, exe);
    CHECK_FP_WRITABLE(f, exe);

    FILE *fp = FKL_VM_FP(f)->fp;
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **const arg_end = arg_base + arg_num;
    for (++arg_base; arg_base < arg_end; ++arg_base) {
        r = *arg_base;
        if (FKL_IS_STR(r)) {
            FklString *str = FKL_VM_STR(r);
            fwrite(str->str, str->size, 1, fp);
        } else if (FKL_IS_BYTEVECTOR(r)) {
            FklBytevector *bvec = FKL_VM_BVEC(r);
            fwrite(bvec->ptr, bvec->size, 1, fp);
        } else if (FKL_IS_USERDATA(r) && fklIsWritableUd(FKL_VM_UD(r)))
            fklWriteVMud(FKL_VM_UD(r), fp);
        else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    }
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

static int fs_fflush(FKL_CPROC_ARGL) {
    uint32_t const arg_num = FKL_CPROC_GET_ARG_NUM(exe, ctx);
    FKL_CPROC_CHECK_ARG_NUM2(exe, arg_num, 0, 1);

    if (arg_num) {
        FklVMvalue *fp = FKL_CPROC_GET_ARG(exe, ctx, 0);
        ;
        FKL_CHECK_TYPE(fp, fklIsVMvalueFp, exe);
        CHECK_FP_OPEN(fp, exe);
        if (fflush(FKL_VM_FP(fp)->fp))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    } else if (fflush(NULL))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int fs_fclerr(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, ctx, 1);
    FklVMvalue *fp = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(fp, fklIsVMvalueFp, exe);
    CHECK_FP_OPEN(fp, exe);
    clearerr(FKL_VM_FP(fp)->fp);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int fs_fprintf(FKL_CPROC_ARGL) {
    uint32_t const arg_num = FKL_CPROC_GET_ARG_NUM(exe, ctx);
    FKL_CPROC_CHECK_ARG_NUM2(exe, arg_num, 2, -1);
    FklVMvalue *fp = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fmt_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);

    FKL_CHECK_TYPE(fp, fklIsVMvalueFp, exe);
    FKL_CHECK_TYPE(fmt_obj, FKL_IS_STR, exe);
    CHECK_FP_OPEN(fp, exe);
    CHECK_FP_WRITABLE(fp, exe);

    uint64_t len = 0;
    FklVMvalue **start = &FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklBuiltinErrorType err_type =
        fklVMprintf2(exe, FKL_VM_FP(fp)->fp, FKL_VM_STR(fmt_obj), &len, start,
                     start + arg_num - 2);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(len, exe));
    return 0;
}

static int fs_fileno(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, ctx, 1);
    FklVMvalue *vfp = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(vfp, fklIsVMvalueFp, exe);
    FklVMfp *fp = FKL_VM_FP(vfp);
    int fileno = fklVMfpFileno(fp);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(fileno));
    return 0;
}

struct SymFunc {
    const char *sym;
    FklVMcFunc f;
} exports_and_func[] = {
    // clang-format off
    {"facce?",   fs_facce_p  },
    {"freg?",    fs_freg_p   },
    {"fdir?",    fs_fdir_p   },
    {"freopen",  fs_freopen  },
    {"realpath", fs_realpath },
    {"relpath",  fs_relpath  },
    {"mkdir",    fs_mkdir    },
    {"fprint",   fs_fprint   },
    {"fprintln", fs_fprintln },
    {"fprin1",   fs_fprin1   },
    {"fprin1n",  fs_fprin1n  },
    {"fwrite",   fs_fwrite   },
    {"fflush",   fs_fflush   },
    {"fclerr",   fs_fclerr   },
    {"fprintf",  fs_fprintf  },
    {"fileno",   fs_fileno   },
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
