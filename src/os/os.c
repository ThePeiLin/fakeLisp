#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <stdlib.h>
#include <time.h>

static int os_system(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *cmd = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(cmd, FKL_IS_STR, exe);
    const FklString *cmd_str = FKL_VM_STR(cmd);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMint(system(cmd_str->str), exe));
    return 0;
}

static int os_time(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMint((int64_t)time(NULL), exe));
    return 0;
}

static int os_clock(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMint((int64_t)clock(), exe));
    return 0;
}

static int os_date(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 0, 2);
    switch (argc) {
    case 0: {
        time_t stamp = time(NULL);
        const struct tm *tblock = localtime(&stamp);

        FklStringBuffer buf;
        fklInitStringBuffer(&buf);

        fklStringBufferPrintf(&buf,
                "%04u-%02u-%02u_%02u_%02u_%02u",
                tblock->tm_year + 1900,
                tblock->tm_mon + 1,
                tblock->tm_mday,
                tblock->tm_hour,
                tblock->tm_min,
                tblock->tm_sec);

        FklVMvalue *tmpVMvalue = fklCreateVMvalueStr2(exe, buf.index, buf.buf);
        fklUninitStringBuffer(&buf);

        FKL_CPROC_RETURN(exe, ctx, tmpVMvalue);
    } break;
    case 1: {
        FklVMvalue *arg1 = FKL_CPROC_GET_ARG(exe, ctx, 0);
        if (FKL_IS_STR(arg1)) {
            FklVMvalue *fmt = arg1;
            FklStringBuffer buf;
            fklInitStringBuffer(&buf);
            time_t stamp = time(NULL);
            const struct tm *tblock = localtime(&stamp);
            const char *format = FKL_VM_STR(fmt)->str;
            size_t n = 0;
            while (!(n = strftime(buf.buf, buf.size, format, tblock)))
                fklStringBufferReverse(&buf, buf.size * 2);
            fklStringBufferReverse(&buf, n + 1);
            buf.index = n;

            FklVMvalue *tmpVMvalue =
                    fklCreateVMvalueStr2(exe, buf.index, buf.buf);
            fklUninitStringBuffer(&buf);
            FKL_CPROC_RETURN(exe, ctx, tmpVMvalue);
        } else if (fklIsVMint(arg1)) {
            static const char default_fmt[] = "%Y-%m-%d_%H_%M_%S";
            FklVMvalue *timestamp = arg1;
            FklStringBuffer buf;
            fklInitStringBuffer(&buf);
            int64_t stamp = fklVMgetInt(timestamp);
            const struct tm *tblock = localtime(&stamp);
            size_t n = 0;
            while (!(n = strftime(buf.buf, buf.size, default_fmt, tblock)))
                fklStringBufferReverse(&buf, buf.size * 2);
            fklStringBufferReverse(&buf, n + 1);
            buf.index = n;

            FklVMvalue *tmpVMvalue =
                    fklCreateVMvalueStr2(exe, buf.index, buf.buf);
            fklUninitStringBuffer(&buf);
            FKL_CPROC_RETURN(exe, ctx, tmpVMvalue);
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } break;
    case 2: {
        FklVMvalue *arg1 = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *arg2 = FKL_CPROC_GET_ARG(exe, ctx, 1);
        if (FKL_IS_STR(arg1) && fklIsVMint(arg2)) {
            FklVMvalue *fmt = arg1;
            FklVMvalue *timestamp = arg2;
            FklStringBuffer buf;
            fklInitStringBuffer(&buf);
            int64_t stamp = fklVMgetInt(timestamp);
            const struct tm *tblock = localtime(&stamp);
            const char *format = FKL_VM_STR(fmt)->str;
            size_t n = 0;
            while (!(n = strftime(buf.buf, buf.size, format, tblock)))
                fklStringBufferReverse(&buf, buf.size * 2);
            fklStringBufferReverse(&buf, n + 1);
            buf.index = n;

            FklVMvalue *tmpVMvalue =
                    fklCreateVMvalueStr2(exe, buf.index, buf.buf);
            fklUninitStringBuffer(&buf);
            FKL_CPROC_RETURN(exe, ctx, tmpVMvalue);
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    } break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    return 0;
}

static int os_remove(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *name = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(name, FKL_IS_STR, exe);
    const FklString *name_str = FKL_VM_STR(name);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(remove(name_str->str)));
    return 0;
}

static int os_rename(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *old_name = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *new_name = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(old_name, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(new_name, FKL_IS_STR, exe);
    const FklString *old_name_str = FKL_VM_STR(old_name);
    const FklString *new_name_str = FKL_VM_STR(new_name);
    FKL_CPROC_RETURN(exe,
            ctx,
            FKL_MAKE_VM_FIX(rename(old_name_str->str, new_name_str->str)));
    return 0;
}

static int os_chdir(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *dir = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(dir, FKL_IS_STR, exe);
    int r = fklChdir(FKL_VM_STR(dir)->str);
    if (r)
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_FILEFAILURE,
                exe,
                "Failed to Change dir to: %s",
                dir);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int os_getcwd(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    char *cwd = fklSysgetcwd();
    FklVMvalue *s = fklCreateVMvalueStrFromCstr(exe, cwd);
    fklZfree(cwd);
    FKL_CPROC_RETURN(exe, ctx, s);
    return 0;
}

static int os_getenv(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *name = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(name, FKL_IS_STR, exe);
    const FklString *name_str = FKL_VM_STR(name);
    const char *env_var = getenv(name_str->str);
    FKL_CPROC_RETURN(exe,
            ctx,
            env_var ? fklCreateVMvalueStrFromCstr(exe, env_var) : FKL_VM_NIL);
    return 0;
}

#ifdef _WIN32
static inline int setenv(const char *name, const char *value, int overwrite) {
    if (!overwrite) {
        size_t envsize = 0;
        int errcode = getenv_s(&envsize, NULL, 0, name);
        if (errcode || envsize)
            return errcode;
    }
    return _putenv_s(name, value);
}

static inline int unsetenv(const char *name) { return _putenv_s(name, ""); }
#endif

static int os_setenv(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 3);
    FklVMvalue *name = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *value = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FklVMvalue *overwrite = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(name, FKL_IS_STR, exe);
    const FklString *name_str = FKL_VM_STR(name);
    if (value) {
        FKL_CHECK_TYPE(value, FKL_IS_STR, exe);
        int o = (overwrite && overwrite != FKL_VM_NIL) ? 1 : 0;
        if (setenv(name_str->str, FKL_VM_STR(value)->str, o))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    } else {
        if (unsetenv(name_str->str))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    }
    return 0;
}

struct SymFunc {
    const char *sym;
    FklVMcFunc f;
} exports_and_func[] = {
    // clang-format off
    {"system", os_system },
    {"remove", os_remove },
    {"rename", os_rename },
    {"clock",  os_clock  },
    {"time",   os_time   },
    {"date",   os_date   },
    {"chdir",  os_chdir  },
    {"getcwd", os_getcwd },
    {"getenv", os_getenv },
    {"setenv", os_setenv },
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
