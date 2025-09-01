#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/readline.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <argtable3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

static int runRepl(FklCodegenInfo *,
        FklCodegenEnv *main_env,
        const char *eval_expression,
        int8_t interactive);
static void init_frame_to_repl_frame(FklVM *,
        FklCodegenInfo *codegen,
        FklCodegenEnv *,
        const char *eval_expression,
        int8_t interactive);

static int exitState = 0;

#define FKL_EXIT_FAILURE (255)

static inline int
compileAndRun(const char *filename, int argc, const char *const *argv) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror(filename);
        return FKL_EXIT_FAILURE;
    }
    FklCodegenOuterCtx outer_ctx;
    char *rp = fklRealpath(filename);
    fklInitCodegenOuterCtx(&outer_ctx, fklGetDir(rp));
    FklSymbolTable *pst = &outer_ctx.public_symbol_table;
    fklAddSymbolCstr(filename, pst);
    FklCodegenInfo codegen = {
        .fid = 0,
    };
    FklCodegenEnv *main_env = fklInitGlobalCodegenInfo(&codegen,
            rp,
            fklCreateSymbolTable(),
            fklCreateConstTable(),
            0,
            &outer_ctx,
            NULL,
            NULL,
            NULL);
    fklZfree(rp);
    FklByteCodelnt *mainByteCode =
            fklGenExpressionCodeWithFp(fp, &codegen, main_env);
    if (mainByteCode == NULL) {
        fklDestroyCodegenEnv(main_env);
        fklUninitCodegenInfo(&codegen);
        fklUninitCodegenOuterCtx(&outer_ctx);
        return FKL_EXIT_FAILURE;
    }
    fklUpdatePrototype(codegen.pts,
            main_env,
            codegen.runtime_symbol_table,
            pst);
    fklDestroyCodegenEnv(main_env);
    fklPrintUndefinedRef(codegen.global_env, codegen.runtime_symbol_table, pst);

    FklCodegenLibVector *scriptLibStack = codegen.libraries;
    FklVM *anotherVM = fklCreateVMwithByteCode(mainByteCode,
            codegen.runtime_symbol_table,
            codegen.runtime_kt,
            codegen.pts,
            1,
            0);
    codegen.runtime_symbol_table = NULL;
    codegen.runtime_kt = NULL;
    codegen.pts = NULL;
    FklVMgc *gc = anotherVM->gc;
    gc->lib_num = scriptLibStack->size;
    gc->libs = (FklVMlib *)fklZcalloc((gc->lib_num + 1), sizeof(FklVMlib));
    FKL_ASSERT(gc->libs);

    while (!fklCodegenLibVectorIsEmpty(scriptLibStack)) {
        FklVMlib *curVMlib = &gc->libs[scriptLibStack->size];
        FklCodegenLib *cur = fklCodegenLibVectorPopBackNonNull(scriptLibStack);
        FklCodegenLibType type = cur->type;
        fklInitVMlibWithCodegenLibMove(cur,
                curVMlib,
                anotherVM,
                anotherVM->pts);
        if (type == FKL_CODEGEN_LIB_SCRIPT)
            fklInitMainProcRefs(anotherVM, curVMlib->proc);
    }

    fklUninitCodegenInfo(&codegen);

    fklChdir(outer_ctx.cwd);
    fklUninitCodegenOuterCtx(&outer_ctx);

    fklInitVMargs(gc, argc, argv);
    int r = fklRunVMidleLoop(anotherVM);
    fklDestroySymbolTable(gc->st);
    fklDestroyConstTable(gc->kt);
    fklDestroyAllVMs(anotherVM);
    fklDestroyVMgc(gc);
    return r;
}

static inline void
initLibWithPrototype(FklVMlib *lib, uint32_t num, FklFuncPrototypes *pts) {
    FklFuncPrototype *pta = pts->pa;
    for (uint32_t i = 1; i <= num; i++) {
        FklVMlib *cur = &lib[i];
        if (FKL_IS_PROC(cur->proc)) {
            FklVMproc *proc = FKL_VM_PROC(cur->proc);
            proc->lcount = pta[proc->protoId].lcount;
        }
    }
}

typedef struct {
    FklVM *exe;
} VMlibInitArgs;

static void vmlib_initer(FklReadCodeFileArgs *read_args,
        void *lib_addr,
        void *lib_init_args,
        FklCodegenLibType type,
        uint32_t prototypeId,
        uint64_t spc,
        const FklByteCodelnt *bcl,
        const FklString *dll_name) {

    FklVMlib *lib = FKL_TYPE_CAST(FklVMlib *, lib_addr);
    VMlibInitArgs *args = FKL_TYPE_CAST(VMlibInitArgs *, lib_init_args);
    if (args->exe == NULL) {
        args->exe = fklCreateVMwithByteCode(read_args->main_func,
                read_args->runtime_st,
                read_args->runtime_kt,
                read_args->pts,
                1,
                0);
    }

    switch (type) {
    case FKL_CODEGEN_LIB_SCRIPT: {
        FklVMvalue *codeObj = fklCreateVMvalueCodeObj(args->exe, bcl);
        fklInitVMlibWithCodeObj(lib, codeObj, args->exe, prototypeId, spc);
        fklInitMainProcRefs(args->exe, lib->proc);
    } break;
    case FKL_CODEGEN_LIB_DLL: {
        FklVMvalue *sv = fklCreateVMvalueStr(args->exe, dll_name);
        fklInitVMlib(lib, sv, 0);
    } break;
    case FKL_CODEGEN_LIB_UNINIT:
        FKL_UNREACHABLE();
        break;
    }
}

static inline int
runCode(const char *filename, int argc, const char *const *argv) {
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror(filename);
        return FKL_EXIT_FAILURE;
    }
    FklSymbolTable runtime_st;
    fklInitSymbolTable(&runtime_st);
    FklConstTable runtime_kt;
    fklInitConstTable(&runtime_kt);

    VMlibInitArgs lib_init_args = { .exe = NULL };
    FklReadCodeFileArgs args = {
        .runtime_st = &runtime_st,
        .runtime_kt = &runtime_kt,
        .library_initer = vmlib_initer,
        .lib_init_args = &lib_init_args,
        .lib_size = sizeof(FklVMlib),
    };

    fklReadCodeFile(fp, &args);

    FklVM *anotherVM = lib_init_args.exe;

    FklVMgc *gc = anotherVM->gc;

    fclose(fp);

    gc->libs = FKL_TYPE_CAST(FklVMlib *, args.libs);
    gc->lib_num = args.lib_count;
    initLibWithPrototype(gc->libs, gc->lib_num, anotherVM->pts);
    fklInitVMargs(anotherVM->gc, argc, argv);
    int r = fklRunVMidleLoop(anotherVM);
    fklUninitSymbolTable(&runtime_st);
    fklUninitConstTable(&runtime_kt);
    fklDestroyAllVMs(anotherVM);
    fklDestroyVMgc(gc);
    return r;
}

static inline int
runPreCompile(const char *filename, int argc, const char *const *argv) {
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror(filename);
        return FKL_EXIT_FAILURE;
    }
    FklSymbolTable runtime_st;
    fklInitSymbolTable(&runtime_st);

    FklConstTable runtime_kt;
    fklInitConstTable(&runtime_kt);

    FklFuncPrototypes *pts = fklCreateFuncPrototypes(0);

    FklFuncPrototypes macro_pts;
    fklInitFuncPrototypes(&macro_pts, 0);

    FklCodegenLibVector scriptLibStack;
    fklCodegenLibVectorInit(&scriptLibStack, 8);

    FklCodegenLibVector macroScriptLibStack;
    fklCodegenLibVectorInit(&macroScriptLibStack, 8);

    FklCodegenOuterCtx ctx;
    fklInitCodegenOuterCtxExceptPattern(&ctx);

    char *errorStr = NULL;
    char *rp = fklRealpath(filename);
    int load_result = fklLoadPreCompile(pts,
            &macro_pts,
            &scriptLibStack,
            &macroScriptLibStack,
            &runtime_st,
            &runtime_kt,
            &ctx,
            rp,
            fp,
            &errorStr);
    fklUninitFuncPrototypes(&macro_pts);
    while (!fklCodegenLibVectorIsEmpty(&macroScriptLibStack))
        fklUninitCodegenLib(
                fklCodegenLibVectorPopBackNonNull(&macroScriptLibStack));
    fklCodegenLibVectorUninit(&macroScriptLibStack);
    fklZfree(rp);
    fclose(fp);
    fklUninitSymbolTable(&ctx.public_symbol_table);
    fklUninitConstTable(&ctx.public_kt);
    fklUninitGrammer(&ctx.builtin_g);
    if (load_result) {
        fklUninitSymbolTable(&runtime_st);
        fklDestroyFuncPrototypes(pts);
        while (!fklCodegenLibVectorIsEmpty(&scriptLibStack))
            fklUninitCodegenLib(
                    fklCodegenLibVectorPopBackNonNull(&scriptLibStack));
        fklCodegenLibVectorUninit(&scriptLibStack);
        if (errorStr) {
            fprintf(stderr, "%s\n", errorStr);
            fklZfree(errorStr);
        }
        fprintf(stderr, "%s: Load pre-compile file failed.\n", filename);
        return 1;
    }

    FklCodegenLib *main_lib =
            fklCodegenLibVectorPopBackNonNull(&scriptLibStack);
    FklByteCodelnt *main_byte_code = main_lib->bcl;

    main_lib->bcl = NULL;

    FklVM *anotherVM = fklCreateVMwithByteCode(main_byte_code,
            &runtime_st,
            &runtime_kt,
            pts,
            1,
            0);

    fklUninitCodegenLib(main_lib);

    FklVMgc *gc = anotherVM->gc;
    gc->lib_num = scriptLibStack.size + 1;
    gc->libs = (FklVMlib *)fklZcalloc((gc->lib_num + 1), sizeof(FklVMlib));
    FKL_ASSERT(gc->libs);

    while (!fklCodegenLibVectorIsEmpty(&scriptLibStack)) {
        FklVMlib *curVMlib = &gc->libs[scriptLibStack.size];
        FklCodegenLib *cur = fklCodegenLibVectorPopBackNonNull(&scriptLibStack);
        FklCodegenLibType type = cur->type;
        fklInitVMlibWithCodegenLibMove(cur,
                curVMlib,
                anotherVM,
                anotherVM->pts);
        if (type == FKL_CODEGEN_LIB_SCRIPT)
            fklInitMainProcRefs(anotherVM, curVMlib->proc);
    }
    fklCodegenLibVectorUninit(&scriptLibStack);

    fklInitVMargs(anotherVM->gc, argc, argv);
    int r = fklRunVMidleLoop(anotherVM);
    fklDestroyAllVMs(anotherVM);
    fklDestroyVMgc(gc);
    fklUninitSymbolTable(&runtime_st);
    fklUninitConstTable(&runtime_kt);
    return r;
}

enum PriorityEnum {
    PRIORITY_UNSET = 0,
    PRIORITY_PACKAGE_SCRIPT,
    PRIORITY_MODULE_SCRIPT,
    PRIORITY_PACKAGE_BYTECODE,
    PRIORITY_MODULE_BYTECODE,
    PRIORITY_PACKAGE_PRECOMPILE,
};

static const char *priority_str[PRIORITY_PACKAGE_PRECOMPILE + 1] = {
    NULL,
    "package-script",
    "module-script",
    "package-bytecode",
    "module-bytecode",
    "package-precompile",
};

#define PRIORITY_PLACEHOLDER ("~")

static inline int load_priority(char *str,
        enum PriorityEnum priority_array[PRIORITY_PACKAGE_PRECOMPILE]) {
    struct {
        enum PriorityEnum type;
        int used;
    } priority_used_array[PRIORITY_PACKAGE_PRECOMPILE + 1] = {
        { PRIORITY_UNSET, 0 },
        { PRIORITY_PACKAGE_SCRIPT, 0 },
        { PRIORITY_MODULE_SCRIPT, 0 },
        { PRIORITY_PACKAGE_BYTECODE, 0 },
        { PRIORITY_MODULE_BYTECODE, 0 },
        { PRIORITY_PACKAGE_PRECOMPILE, 0 },
    };
    char *priority_strs[PRIORITY_PACKAGE_PRECOMPILE] = {
        NULL,
    };

    char *context = NULL;

    uint32_t token_count = 0;
    char *token = NULL;
    if (str[0] == ',')
        goto error;
    else
        token = fklStrTok(str, ",", &context);
    while (token) {
        if (token_count < PRIORITY_PACKAGE_PRECOMPILE) {
            priority_strs[token_count] = token;
            token_count++;
        }
        token = fklStrTok(NULL, ",", &context);
    }

    uint32_t end = PRIORITY_PACKAGE_PRECOMPILE + 1;

    for (uint32_t i = 0; i < token_count; i++) {
        const char *cur = fklTrim(priority_strs[i]);

        if (*cur != *PRIORITY_PLACEHOLDER) {
            uint32_t j = 1;
            for (; j < PRIORITY_PACKAGE_PRECOMPILE + 1; j++) {
                if (!strcmp(cur, priority_str[j])) {
                    if (!priority_used_array[j].used) {
                        priority_array[i] = j;
                        priority_used_array[j].used = 1;
                        break;
                    }
                }
            }

            if (j > PRIORITY_PACKAGE_PRECOMPILE)
                goto error;
        } else if (strcmp(cur, PRIORITY_PLACEHOLDER))
            goto error;
    }

    for (uint32_t i = 0; i < PRIORITY_PACKAGE_PRECOMPILE; i++) {
        if (priority_array[i] == PRIORITY_UNSET) {
            for (uint32_t j = 1; j < end; j++) {
                if (!priority_used_array[j].used) {
                    priority_array[i] = priority_used_array[j].type;
                    priority_used_array[j].used = 1;
                    break;
                }
            }
        }
    }
    return 0;

error:
    return 1;
}

static inline int load_specify(char *str,
        enum PriorityEnum priority_array[PRIORITY_PACKAGE_PRECOMPILE]) {
    struct {
        enum PriorityEnum type;
        int used;
    } priority_used_array[PRIORITY_PACKAGE_PRECOMPILE + 1] = {
        { PRIORITY_UNSET, 0 },
        { PRIORITY_PACKAGE_SCRIPT, 0 },
        { PRIORITY_MODULE_SCRIPT, 0 },
        { PRIORITY_PACKAGE_BYTECODE, 0 },
        { PRIORITY_MODULE_BYTECODE, 0 },
        { PRIORITY_PACKAGE_PRECOMPILE, 0 },
    };
    char *priority_strs[PRIORITY_PACKAGE_PRECOMPILE] = {
        NULL,
    };

    char *context = NULL;

    uint32_t token_count = 0;
    char *token = NULL;
    if (str[0] == ',')
        goto error;
    else
        token = fklStrTok(str, ",", &context);
    while (token) {
        if (token_count < PRIORITY_PACKAGE_PRECOMPILE) {
            priority_strs[token_count] = token;
            token_count++;
        }
        token = fklStrTok(NULL, ",", &context);
    }

    for (uint32_t i = 0; i < token_count; i++) {
        const char *cur = fklTrim(priority_strs[i]);

        uint32_t j = 1;
        for (; j < PRIORITY_PACKAGE_PRECOMPILE + 1; j++) {
            if (!strcmp(cur, priority_str[j])) {
                if (!priority_used_array[j].used) {
                    priority_array[i] = j;
                    priority_used_array[j].used = 1;
                    break;
                }
            }
        }

        if (j > PRIORITY_PACKAGE_PRECOMPILE)
            goto error;
    }

    return 0;

error:
    return 1;
}

struct arg_lit *help;
struct arg_lit *interactive;
struct arg_lit *module;
struct arg_str *rest;
struct arg_str *priority;
struct arg_str *specify;
struct arg_str *eval;
struct arg_end *end;

int main(int argc, char *argv[]) {
    argv = uv_setup_args(argc, argv);
    const char *progname = argv[0];

    void *argtable[] = {
        help = arg_lit0("h", "help", "display this help and exit"),
        interactive = arg_lit0("i", "interactive", "interactive mode"),
        module = arg_lit0("m", "module", "treat filename as module"),

        eval = arg_str0("e",
                "eval",
                "<expr>",
                "evaluate the given expressions"),

        specify = arg_str0("s",
                "specify",
                NULL,
                "specify the file type of module, the argument can be "
                "'package-script', 'module-script', 'package-bytecode', "
                "'module-bytecode' and 'package-precompile'. "
                "Also allow to combine them"),

        priority = arg_str0("p",
                "priority",
                NULL,
                "order of which type to filename be handled. the "
                "default priority is "
                "'package-script, module-script, package-bytecode, "
                "module-bytecode, package-precompile'. "
                "'~' can be used as placeholder"),

        rest = arg_strn(NULL,
                NULL,
                "file [args]",
                0,
                argc + 2,
                "file and args"),

        end = arg_end(20),
    };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        printf("Usage: %s", progname);
        arg_print_syntaxv(stdout, argtable, "\n");
        arg_print_glossary_gnu(stdout, argtable);
        goto exit;
    }

    if (nerrors) {
    error:
        arg_print_errors(stdout, end, progname);
        printf("Try '%s --help' for more informaction.\n", progname);
        exitState = EXIT_FAILURE;
        goto exit;
    }

    if (interactive->count > 0 || eval->count > 0) {
        if (rest->count > 0 || priority->count > 0)
            goto error;
        goto interactive;
    } else if (rest->count == 0) {
    interactive:;
        FklCodegenOuterCtx outer_ctx;
        FklCodegenInfo codegen = {
            .fid = 0,
        };
        fklInitCodegenOuterCtx(&outer_ctx, NULL);
        FklCodegenEnv *main_env = fklInitGlobalCodegenInfo(&codegen,
                NULL,
                &outer_ctx.public_symbol_table,
                &outer_ctx.public_kt,
                0,
                &outer_ctx,
                NULL,
                NULL,
                NULL);
        exitState = runRepl(&codegen,
                main_env,
                eval->count > 0 ? eval->sval[0] : NULL,
                interactive->count > 0);
        codegen.runtime_symbol_table = NULL;
        FklCodegenLibVector *loadedLibStack = codegen.libraries;
        while (!fklCodegenLibVectorIsEmpty(loadedLibStack)) {
            FklCodegenLib *lib =
                    fklCodegenLibVectorPopBackNonNull(loadedLibStack);
            fklUninitCodegenLib(lib);
        }
        fklDestroyCodegenEnv(main_env);
        fklUninitCodegenInfo(&codegen);
        fklUninitCodegenOuterCtx(&outer_ctx);
    } else {
        const char *filename = rest->sval[0];
        int argc = rest->count;
        const char *const *argv = rest->sval;
        if (module->count > 0)
            goto handle_module;
        if (fklIsAccessibleRegFile(filename)) {
            if (fklIsScriptFile(filename))
                exitState = compileAndRun(filename, argc, argv);
            else if (fklIsByteCodeFile(filename))
                exitState = runCode(filename, argc, argv);
            else if (fklIsPrecompileFile(filename))
                exitState = runPreCompile(filename, argc, argv);
            else {
                exitState = FKL_EXIT_FAILURE;
                fprintf(stderr, "%s: It is not a correct file.\n", filename);
            }
        } else {
        handle_module:;
            if (priority->count > 0 && specify->count > 0)
                goto error;

            enum PriorityEnum priority_array[PRIORITY_PACKAGE_PRECOMPILE] = {
                PRIORITY_UNSET
            };

            if (priority->count > 0) {
                if (load_priority((char *)priority->sval[0], priority_array))
                    goto error;
            } else if (specify->count) {
                if (load_specify((char *)specify->sval[0], priority_array))
                    goto error;
            } else {
                priority_array[0] = PRIORITY_PACKAGE_SCRIPT;
                priority_array[1] = PRIORITY_MODULE_SCRIPT;
                priority_array[2] = PRIORITY_PACKAGE_BYTECODE;
                priority_array[3] = PRIORITY_MODULE_BYTECODE;
                priority_array[4] = PRIORITY_PACKAGE_PRECOMPILE;
            }

            FklStringBuffer main_script_buf;
            fklInitStringBuffer(&main_script_buf);

            fklStringBufferConcatWithCstr(&main_script_buf, filename);
            char *module_script_file =
                    fklStrCat(fklZstrdup(main_script_buf.buf),
                            FKL_SCRIPT_FILE_EXTENSION);
            char *module_bytecode_file =
                    fklStrCat(fklZstrdup(main_script_buf.buf),
                            FKL_BYTECODE_FILE_EXTENSION);

            fklStringBufferConcatWithCstr(&main_script_buf,
                    FKL_PATH_SEPARATOR_STR);
            fklStringBufferConcatWithCstr(&main_script_buf, "main.fkl");

            char *main_code_file = fklStrCat(fklZstrdup(main_script_buf.buf),
                    FKL_BYTECODE_FKL_SUFFIX_STR);
            char *main_pre_file = fklStrCat(fklZstrdup(main_script_buf.buf),
                    FKL_PRE_COMPILE_FKL_SUFFIX_STR);

            for (uint32_t i = 0; i < PRIORITY_PACKAGE_PRECOMPILE; i++) {
                switch (priority_array[i]) {
                case PRIORITY_UNSET:
                    goto execute_err;
                    break;
                case PRIORITY_MODULE_SCRIPT:
                    if (fklIsAccessibleRegFile(module_script_file)) {
                        exitState =
                                compileAndRun(module_script_file, argc, argv);
                        goto execute_done;
                    }
                    break;
                case PRIORITY_MODULE_BYTECODE:
                    if (fklIsAccessibleRegFile(module_bytecode_file)) {
                        exitState = runCode(module_bytecode_file, argc, argv);
                        goto execute_done;
                    }
                    break;
                case PRIORITY_PACKAGE_SCRIPT:
                    if (fklIsAccessibleRegFile(main_script_buf.buf)) {
                        exitState =
                                compileAndRun(main_script_buf.buf, argc, argv);
                        goto execute_done;
                    }
                    break;
                case PRIORITY_PACKAGE_BYTECODE:
                    if (fklIsAccessibleRegFile(main_code_file)) {
                        exitState = runCode(main_code_file, argc, argv);
                        goto execute_done;
                    }
                    break;

                case PRIORITY_PACKAGE_PRECOMPILE:
                    if (fklIsAccessibleRegFile(main_pre_file)) {
                        exitState = runPreCompile(main_pre_file, argc, argv);
                        goto execute_done;
                    }
                    break;
                }
            }

        execute_err:
            exitState = FKL_EXIT_FAILURE;
            fprintf(stderr, "%s: No such file or directory\n", filename);

        execute_done:
            fklUninitStringBuffer(&main_script_buf);
            fklZfree(main_code_file);
            fklZfree(main_pre_file);
            fklZfree(module_script_file);
            fklZfree(module_bytecode_file);
        }
    }

exit:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exitState;
}

static int runRepl(FklCodegenInfo *codegen,
        FklCodegenEnv *main_env,
        const char *eval_expression,
        int8_t interactive) {
    FklVM *anotherVM = fklCreateVMwithByteCode(NULL,
            codegen->runtime_symbol_table,
            codegen->runtime_kt,
            codegen->pts,
            1,
            0);
    FklVMgc *gc = anotherVM->gc;
    gc->libs = (FklVMlib *)fklZcalloc(1, sizeof(FklVMlib));
    FKL_ASSERT(gc->libs);

    init_frame_to_repl_frame(anotherVM,
            codegen,
            main_env,
            eval_expression,
            interactive);

    int err = fklRunVMidleLoop(anotherVM);

    fklDestroyAllVMs(anotherVM);
    fklDestroyVMgc(gc);
    codegen->pts = NULL;
    return err;
}

typedef struct {
    size_t offset;
    FklParseStateVector stateStack;
    FklAnalysisSymbolVector symbolStack;
    FklUintVector lineStack;

    int err;
    FklNastNode *node;
} NastCreatCtx;

struct ReplFrameCtx {
    FklVMvalue *stdinVal;
    FklVMvalue *mainProc;
    FklStringBuffer buf;
    FklVMvarRefList *lrefl;
    FklVMvalue **lref;
    uint32_t lcount;
    uint32_t bp;
    uint32_t sp;
};

typedef struct {
    FklCodegenInfo *codegen;
    FklCodegenEnv *main_env;
    NastCreatCtx *cc;
    FklVM *exe;
    enum {
        READY,
        WAITING,
        READING,
    } state : 8;
    int8_t eof;
    int8_t interactive;
    uint32_t new_var_count;
    struct ReplFrameCtx *c;
} ReplCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(ReplCtx);

static inline void inc_lref(FklVMCompoundFrameVarRef *lr, uint32_t lcount) {
    if (!lr->lref) {
        lr->lref = (FklVMvalue **)fklZcalloc(lcount, sizeof(FklVMvalue *));
        FKL_ASSERT(lr->lref);
    }
}

static inline FklVMvalue *
insert_local_ref(FklVMCompoundFrameVarRef *lr, FklVMvalue *ref, uint32_t cidx) {
    FklVMvarRefList *rl =
            (FklVMvarRefList *)fklZmalloc(sizeof(FklVMvarRefList));
    FKL_ASSERT(rl);
    rl->ref = ref;
    rl->next = lr->lrefl;
    lr->lrefl = rl;
    lr->lref[cidx] = ref;
    return ref;
}

struct ResolveRefArg {
    const FklSymDefHashMapElm *def;
    uint32_t prototypeId;
    uint32_t idx;
};

// FklResolveRefArgVector
#define FKL_VECTOR_ELM_TYPE struct ResolveRefArg
#define FKL_VECTOR_ELM_TYPE_NAME ResolveRefArg
#include <fakeLisp/cont/vector.h>

struct ResolveRefArgs {
    FklVMvalue **loc;
    FklVMCompoundFrameVarRef *lr;
    FklResolveRefArgVector unresolve_refs;
    uint64_t new_lib_count;
    FklVMlib *new_libs;
    int is_need_update_const_array;
};

static inline void
update_vm_libs(FklVMgc *gc, uint64_t new_libs_count, FklVMlib *new_libs) {
    FklVMlib *libs = fklZrealloc(gc->libs,
            (gc->lib_num + new_libs_count + 1) * sizeof(FklVMlib));
    FKL_ASSERT(libs);
    for (uint64_t i = 0; i < new_libs_count; ++i) {
        libs[1 + i + gc->lib_num] = new_libs[i];
    }
    gc->libs = libs;
    gc->lib_num += new_libs_count;
}

static inline void resolve_ref_work_func(FklVM *exe,
        const struct ResolveRefArgs *s) {
    if (s->is_need_update_const_array)
        fklVMgcUpdateConstArray(exe->gc, exe->gc->kt);
    const FklResolveRefArgVector *unresolve_refs = &s->unresolve_refs;
    struct ResolveRefArg *cur = unresolve_refs->base;
    const struct ResolveRefArg *const end = &cur[unresolve_refs->size];
    FklVMvalue **loc = s->loc;

    FklVMCompoundFrameVarRef *lr = s->lr;
    for (; cur < end; cur++) {
        const FklSymDefHashMapElm *def = cur->def;
        uint32_t prototypeId = cur->prototypeId;
        uint32_t idx = cur->idx;
        for (FklVMvalue *v = exe->gc->head; v; v = v->next)
            if (FKL_IS_PROC(v) && FKL_VM_PROC(v)->protoId == prototypeId) {
                FklVMproc *proc = FKL_VM_PROC(v);
                if (lr->lref[def->v.idx])
                    proc->closure[idx] = lr->lref[def->v.idx];
                else {
                    FklVMvalue *ref = proc->closure[idx];
                    FKL_VM_VAR_REF(ref)->idx = def->v.idx;
                    FKL_VM_VAR_REF(ref)->ref = &loc[def->v.idx];
                    insert_local_ref(lr, ref, def->v.idx);
                }
            }

        for (FklVMvalue *v = exe->obj_head; v; v = v->next)
            if (FKL_IS_PROC(v) && FKL_VM_PROC(v)->protoId == prototypeId) {
                FklVMproc *proc = FKL_VM_PROC(v);
                if (lr->lref[def->v.idx])
                    proc->closure[idx] = lr->lref[def->v.idx];
                else {
                    FklVMvalue *ref = proc->closure[idx];
                    FKL_VM_VAR_REF(ref)->idx = def->v.idx;
                    FKL_VM_VAR_REF(ref)->ref = &loc[def->v.idx];
                    insert_local_ref(lr, ref, def->v.idx);
                }
            }

        for (FklVM *cur = exe->next; cur != exe; cur = cur->next) {
            for (FklVMvalue *v = cur->obj_head; v; v = v->next)
                if (FKL_IS_PROC(v) && FKL_VM_PROC(v)->protoId == prototypeId) {
                    FklVMproc *proc = FKL_VM_PROC(v);
                    if (lr->lref[def->v.idx])
                        proc->closure[idx] = lr->lref[def->v.idx];
                    else {
                        FklVMvalue *ref = proc->closure[idx];
                        FKL_VM_VAR_REF(ref)->idx = def->v.idx;
                        FKL_VM_VAR_REF(ref)->ref = &loc[def->v.idx];
                        insert_local_ref(lr, ref, def->v.idx);
                    }
                }
        }
    }

    if (s->new_libs)
        update_vm_libs(exe->gc, s->new_lib_count, s->new_libs);
}

struct UpdateConstArrayArgs {
    uint64_t new_libs_count;
    FklVMlib *new_libs;
    int is_need_update_const_array;
};

static void update_const_array_cb(FklVM *exe, void *arg) {
    const struct UpdateConstArrayArgs *args = arg;
    if (args->is_need_update_const_array) {
        fklVMgcUpdateConstArray(exe->gc, exe->gc->kt);
    }
    if (args->new_libs)
        update_vm_libs(exe->gc, args->new_libs_count, args->new_libs);
}

static void resolve_ref_cb(FklVM *exe, void *arg) {
    const struct ResolveRefArgs *aarg = (struct ResolveRefArgs *)arg;
    resolve_ref_work_func(exe, aarg);
}

static inline void uninit_resolve_ref_arg_stack(struct ResolveRefArgs *s) {
    s->loc = NULL;
    s->lr = NULL;
    fklResolveRefArgVectorUninit(&s->unresolve_refs);
    s->new_lib_count = 0;
    fklZfree(s->new_libs);
    s->new_libs = NULL;
    s->is_need_update_const_array = 0;
}

typedef struct {
    FklVMCompoundFrameVarRef *lr;
    FklResolveRefArgVector *unresolve_refs;
} ResolveRefToDefCbArgs;

static void resolve_ref_to_def_cb(const FklSymDefHashMapMutElm *ref,
        const FklSymDefHashMapElm *def,
        const FklUnReSymbolRef *uref,
        FklFuncPrototype *p,
        void *vargs) {

    const ResolveRefToDefCbArgs *args = (const ResolveRefToDefCbArgs *)vargs;
    FklVMCompoundFrameVarRef *lr = args->lr;
    FklResolveRefArgVector *unresolve_refs = args->unresolve_refs;

    uint32_t prototypeId = uref->prototypeId;
    uint32_t idx = ref->v.idx;
    inc_lref(lr, lr->lcount);

    fklResolveRefArgVectorPushBack(unresolve_refs,
            &(struct ResolveRefArg){
                .def = def,
                .prototypeId = prototypeId,
                .idx = idx,
            });
}

static inline void resolve_ref_and_update_const_array_for_repl(
        FklCodegenEnv *env,
        FklCodegenEnv *top_env,
        FklFuncPrototypes *cp,
        FklVM *exe,
        FklVMframe *mainframe,
        int is_need_update_const_array,
        uint64_t new_lib_count,
        FklVMlib *new_libs) {
    uint32_t scope = 1;

    FklVMCompoundFrameVarRef *lr = fklGetCompoundFrameLocRef(mainframe);

    struct ResolveRefArgs resolve_ref_args = {
        .lr = lr,
        .loc = &FKL_VM_GET_ARG(exe, mainframe, 0),
        .is_need_update_const_array = is_need_update_const_array,
        .new_lib_count = new_lib_count,
        .new_libs = new_libs,
    };

    FklResolveRefArgVector *unresolve_refs = &resolve_ref_args.unresolve_refs;
    fklResolveRefArgVectorInit(unresolve_refs, env->uref.size);

    fklResolveRef(env,
            scope,
            cp,
            &(const FklResolveRefArgs){
                .top_env = top_env,
                .no_refs_to_builtins = 1,
                .resolve_ref_to_def_cb = resolve_ref_to_def_cb,
                .resolve_ref_to_def_cb_args =
                        (void *)&(const ResolveRefToDefCbArgs){
                            .lr = lr,
                            .unresolve_refs = unresolve_refs,
                        },
            });

    if (unresolve_refs->size || is_need_update_const_array || new_lib_count) {
        fklQueueWorkInIdleThread(exe, resolve_ref_cb, &resolve_ref_args);
    }

    uninit_resolve_ref_arg_stack(&resolve_ref_args);
}

static inline void update_prototype_lcount(FklFuncPrototypes *cp,
        FklCodegenEnv *env) {
    FklFuncPrototype *pts = &cp->pa[env->prototypeId];
    pts->lcount = env->lcount;
}

static inline void alloc_more_space_for_var_ref(FklVMCompoundFrameVarRef *lr,
        uint32_t i,
        uint32_t n) {
    if (lr->lref && i < n) {
        FklVMvalue **lref =
                (FklVMvalue **)fklZrealloc(lr->lref, sizeof(FklVMvalue *) * n);
        FKL_ASSERT(lref);
        for (; i < n; i++)
            lref[i] = NULL;
        lr->lref = lref;
    }
}

#include <fakeLisp/grammer.h>

static inline void repl_nast_ctx_and_buf_reset(NastCreatCtx *cc,
        FklStringBuffer *s,
        FklGrammer *g) {
    cc->offset = 0;
    if (s) {
        fklStringBufferClear(s);
        s->buf[0] = '\0';
    }
    FklAnalysisSymbolVector *ss = &cc->symbolStack;
    FklAnalysisSymbol *cur_sym = ss->base;
    FklAnalysisSymbol *const sym_end = cur_sym + ss->size;
    for (; cur_sym < sym_end; ++cur_sym)
        fklDestroyNastNode(cur_sym->ast);
    ss->size = 0;
    cc->stateStack.size = 0;
    cc->lineStack.size = 0;
    if (g && g->aTable.num)
        fklParseStateVectorPushBack2(&cc->stateStack,
                (FklParseState){ .state = &g->aTable.states[0] });
    else
        fklNastPushState0ToStack(&cc->stateStack);
}

static inline void
eval_nast_ctx_reset(NastCreatCtx *cc, FklStringBuffer *s, FklGrammer *g) {
    cc->offset = 0;
    FklAnalysisSymbolVector *ss = &cc->symbolStack;
    FklAnalysisSymbol *cur_sym = ss->base;
    FklAnalysisSymbol *const sym_end = cur_sym + ss->size;
    for (; cur_sym < sym_end; ++cur_sym)
        fklDestroyNastNode(cur_sym->ast);
    ss->size = 0;
    cc->stateStack.size = 0;
    cc->lineStack.size = 0;
    if (g && g->aTable.num)
        fklParseStateVectorPushBack2(&cc->stateStack,
                (FklParseState){ .state = &g->aTable.states[0] });
    else
        fklNastPushState0ToStack(&cc->stateStack);
}

#define REPL_PROMPT ">>> "
#define RETVAL_PREFIX ";=> "

#define READ_ERROR_UNEXPETED_EOF (1)
#define READ_ERROR_INVALIDEXPR (2)

typedef struct {
    size_t offset;
    FklVMgc *gc;
    NastCreatCtx *cc;
    FklCodegenInfo *codegen;
    size_t errline;
} ReadExpressionEndArgs;

static int read_expression_end_cb(const char *str,
        int str_len,
        const uint32_t *u32_buf,
        int u32_len,
        int pos,
        void *vargs) {
    if (pos != u32_len)
        return 0;

    ReadExpressionEndArgs *args = FKL_TYPE_CAST(ReadExpressionEndArgs *, vargs);

    FklCodegenInfo *codegen = args->codegen;
    FklGrammer *g = *(codegen->g);

    FklVMgc *gc = args->gc;
    NastCreatCtx *cc = args->cc;

    cc->err = 0;

    FklGrammerMatchOuterCtx outerCtx = FKL_NAST_PARSE_OUTER_CTX_INIT(gc->st);

    int err = 0;
    size_t restLen = str_len - cc->offset;

    FKL_VM_ACQUIRE_ST_BLOCK(gc, flag) {
        if (g && g->aTable.num) {
            cc->node = fklParseWithTableForCharBuf2(g,
                    str + cc->offset,
                    restLen,
                    &restLen,
                    &outerCtx,
                    gc->st,
                    &err,
                    &args->errline,
                    &cc->symbolStack,
                    &cc->lineStack,
                    &cc->stateStack);
        } else {
            cc->node = fklDefaultParseForCharBuf(str + cc->offset,
                    restLen,
                    &restLen,
                    &outerCtx,
                    &err,
                    &args->errline,
                    &cc->symbolStack,
                    &cc->lineStack,
                    &cc->stateStack);
        }
    }

    cc->offset = str_len - restLen;
    codegen->curline = outerCtx.line;

    if (err == FKL_PARSE_TERMINAL_MATCH_FAILED && restLen) {
        cc->err = READ_ERROR_INVALIDEXPR;
        return 1;
    } else if (err == FKL_PARSE_REDUCE_FAILED) {
        cc->err = READ_ERROR_INVALIDEXPR;
        return 1;
    } else if (cc->node) {
        return 1;
    } else {
        return 0;
    }
}

static inline int repl_read_expression(FklStringBuffer *buf,
        ReadExpressionEndArgs *args) {
    char *next = fklReadline3(REPL_PROMPT,
            buf->buf,
            read_expression_end_cb,
            FKL_TYPE_CAST(void *, args));
    fklStringBufferClear(buf);
    int eof = next == NULL;
    if (next) {
        fklStringBufferConcatWithCstr(buf, next);
        fklZfree(next);
    }
    return eof;
}

static inline FklVMvalue **init_mainframe_lref(FklVMvalue **lref,
        uint32_t lcount,
        FklVMvarRefList *lrefl) {
    for (; lrefl; lrefl = lrefl->next) {
        FklVMvalue *ref = lrefl->ref;
        ;
        lref[FKL_VM_VAR_REF(ref)->idx] = ref;
    }
    return lref;
}

struct ConstArrayCount {
    uint32_t i64_count;
    uint32_t f64_count;
    uint32_t str_count;
    uint32_t bvec_count;
    uint32_t bi_count;
};

static inline int is_need_update_const_array(const struct ConstArrayCount *cc,
        const FklConstTable *kt) {
    return cc->i64_count != kt->ki64t.count || cc->f64_count != kt->kf64t.count
        || cc->str_count != kt->kstrt.count
        || cc->bvec_count != kt->kbvect.count || cc->bi_count != kt->kbit.count;
}

static int repl_frame_ret_callback(FklVM *exe, FklVMframe *frame) {
    FklVMvarRefList *lrefl = frame->c.lr.lrefl;
    FklVMvalue **lref = frame->c.lr.lref;
    frame->c.lr.lrefl = NULL;
    frame->c.lr.lref = NULL;
    frame->c.lr.lcount = 1;
    uint32_t bp = frame->bp;
    uint32_t sp = frame->c.sp;
    fklPopVMframe(exe);

    FklVMframe *buttom_frame = exe->top_frame;
    ReplCtx *rctx = FKL_TYPE_CAST(ReplCtx *, buttom_frame->data);
    rctx->c->lrefl = lrefl;
    rctx->c->lref = lref;
    rctx->c->bp = bp;
    rctx->c->sp = sp;

    exe->bp = bp;

    return 1;
}

static int repl_frame_step(void *data, FklVM *exe) {
    ReplCtx *ctx = (ReplCtx *)data;
    struct ReplFrameCtx *fctx = ctx->c;
    NastCreatCtx *cc = ctx->cc;

    switch (ctx->state) {
    default:
        FKL_UNREACHABLE();
    case READY:
        ctx->state = WAITING;
        return 1;
        break;
    case WAITING: {
        ctx->state = READING;
        if (exe->tp - fctx->sp != 0) {
            fputs(RETVAL_PREFIX, stdout);
            fklDBG_printVMstack(exe, exe->tp - fctx->sp, stdout, 0, exe);
        }
        exe->tp = fctx->sp;

        FKL_VM_UNLOCK_BLOCK(exe, flag) {
            cc->node = NULL;
            cc->err = 0;
            ReadExpressionEndArgs args = {
                .gc = exe->gc,
                .cc = cc,
                .codegen = ctx->codegen,
            };

            ctx->eof = repl_read_expression(&fctx->buf, &args);
        }

        return 1;
    } break;
    case READING:
        ctx->state = WAITING;
        break;
    }

    FklCodegenInfo *codegen = ctx->codegen;
    FklCodegenEnv *main_env = ctx->main_env;
    FklSymbolTable *pst = &codegen->outer_ctx->public_symbol_table;
    FklStringBuffer *s = &fctx->buf;
    int is_eof = ctx->eof;

    FklGrammer *g = *(codegen->g);

    FklNastNode *ast = cc->node;
    if (ast == NULL && cc->err) {
        repl_nast_ctx_and_buf_reset(cc, s, g);
        if (cc->err == READ_ERROR_UNEXPETED_EOF)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNEXPECTED_EOF, exe);
        else if (cc->err == READ_ERROR_INVALIDEXPR)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR, exe);
    } else if (ast) {
        fklMakeNastNodeRef(ast);
        size_t libs_count = codegen->libraries->size;

        FklConstTable *kt = codegen->runtime_kt;
        struct ConstArrayCount const_count = {
            .i64_count = kt->ki64t.count,
            .f64_count = kt->kf64t.count,
            .str_count = kt->kstrt.count,
            .bvec_count = kt->kbvect.count,
            .bi_count = kt->kbit.count,
        };

        FklByteCodelnt *mainCode = NULL;
        FKL_VM_ACQUIRE_ST_BLOCK(exe->gc, flag) {
            mainCode = fklGenExpressionCode(ast, main_env, codegen);
        }

        fklStoreHistoryInStringBuffer(s, cc->offset);

        g = *(codegen->g);
        repl_nast_ctx_and_buf_reset(cc, NULL, g);
        fklDestroyNastNode(ast);

        size_t new_libs_count = codegen->libraries->size - libs_count;
        FklVMlib *new_libs = NULL;

        if (new_libs_count) {
            FklVMproc *proc = FKL_VM_PROC(fctx->mainProc);
            new_libs = (FklVMlib *)fklZcalloc(new_libs_count, sizeof(FklVMlib));
            FKL_ASSERT(new_libs);
            for (size_t i = 0; i < new_libs_count; i++) {
                FklVMlib *curVMlib = &new_libs[i];
                FklCodegenLib *curCGlib =
                        &codegen->libraries->base[i + libs_count];
                fklInitVMlibWithCodegenLibRefs(curCGlib,
                        curVMlib,
                        exe,
                        proc->closure,
                        proc->rcount,
                        exe->pts);
            }
        }

        if (mainCode) {
            uint32_t o_lcount = fctx->lcount;
            ctx->state = READY;

            update_prototype_lcount(codegen->pts, main_env);

            uint32_t lcount = 0;
            FKL_VM_ACQUIRE_ST_BLOCK(exe->gc, flag) {
                fklUpdatePrototypeRef(codegen->pts,
                        main_env,
                        codegen->runtime_symbol_table,
                        pst);

                lcount = codegen->pts->pa[1].lcount;
            }

            FklVMvalue *mainProc =
                    fklCreateVMvalueProc2(exe, NULL, 0, FKL_VM_NIL, 1);
            fklInitMainProcRefs(exe, mainProc);
            FklVMproc *proc = FKL_VM_PROC(mainProc);
            fctx->mainProc = mainProc;

            FklVMvalue *mainCodeObj =
                    fklCreateVMvalueCodeObjMove(exe, mainCode);
            fklDestroyByteCodelnt(mainCode);
            fctx->lcount = lcount;
            ctx->new_var_count = fctx->lcount - o_lcount;

            mainCode = FKL_VM_CO(mainCodeObj);
            proc->lcount = fctx->lcount;
            proc->codeObj = mainCodeObj;
            proc->spc = mainCode->bc.code;
            proc->end = proc->spc + mainCode->bc.len;

            exe->base[1] = fctx->mainProc;
            FklVMframe *mainframe =
                    fklCreateVMframeWithProc(exe, fctx->mainProc);
            fklPushVMframe(mainframe, exe);
            mainframe->c.lr.lrefl = fctx->lrefl;
            mainframe->c.lr.lref = fctx->lref;
            FklVMCompoundFrameVarRef *f = &mainframe->c.lr;

            mainframe->bp = fctx->bp;
            mainframe->retCallBack = repl_frame_ret_callback;
            fklVMframeSetSp(exe, mainframe, lcount);

            FKL_VM_GET_ARG(exe, mainframe, o_lcount) = NULL;
            f->lcount = proc->lcount;
            alloc_more_space_for_var_ref(f, o_lcount, f->lcount);
            init_mainframe_lref(f->lref, fctx->lcount, fctx->lrefl);

            resolve_ref_and_update_const_array_for_repl(main_env,
                    codegen->global_env,
                    codegen->pts,
                    exe,
                    mainframe,
                    is_need_update_const_array(&const_count, kt),
                    new_libs_count,
                    new_libs);

            exe->top_frame = mainframe;
            fctx->lrefl = NULL;
            fctx->lref = NULL;

            return 1;
        } else {
            fklClearCodegenPreDef(main_env);
            int is_need_update_const =
                    is_need_update_const_array(&const_count, kt);
            if (is_need_update_const_array(&const_count, kt)
                    || new_libs_count) {
                fklQueueWorkInIdleThread(exe,
                        update_const_array_cb,
                        &(struct UpdateConstArrayArgs){
                            .is_need_update_const_array = is_need_update_const,
                            .new_libs_count = new_libs_count,
                            .new_libs = new_libs,
                        });
                new_libs_count = 0;
                fklZfree(new_libs);
                new_libs = NULL;
            }
            ctx->state = WAITING;
            return 1;
        }
    } else if (ast == NULL && is_eof) {
        return 0;
    } else
        fklStringBufferPutc(&fctx->buf, '\n');
    return 1;
}

static void repl_frame_print_backtrace(void *data, FILE *fp, FklVMgc *gc) {}

static void repl_frame_atomic(void *data, FklVMgc *gc) {
    ReplCtx *ctx = (ReplCtx *)data;
    struct ReplFrameCtx *fctx = ctx->c;
    fklVMgcToGray(fctx->stdinVal, gc);
    fklVMgcToGray(fctx->mainProc, gc);
    for (FklVMvarRefList *l = fctx->lrefl; l; l = l->next)
        fklVMgcToGray(l->ref, gc);
}

static inline void destroyNastCreatCtx(NastCreatCtx *cc) {
    fklParseStateVectorUninit(&cc->stateStack);

    FklAnalysisSymbolVector *ss = &cc->symbolStack;
    FklAnalysisSymbol *cur_sym = ss->base;
    FklAnalysisSymbol *const sym_end = cur_sym + ss->size;
    for (; cur_sym < sym_end; ++cur_sym)
        fklDestroyNastNode(cur_sym->ast);
    ss->size = 0;

    fklUintVectorUninit(&cc->lineStack);
    fklAnalysisSymbolVectorUninit(&cc->symbolStack);
    fklZfree(cc);
}

static void repl_frame_finalizer(void *data) {
    ReplCtx *ctx = (ReplCtx *)data;
    fklUninitStringBuffer(&ctx->c->buf);
    destroyNastCreatCtx(ctx->cc);

    struct ReplFrameCtx *fctx = ctx->c;
    fklZfree(fctx->lref);

    for (FklVMvarRefList *l = fctx->lrefl; l;) {
        fklCloseVMvalueVarRef(l->ref);
        FklVMvarRefList *c = l;
        l = c->next;
        fklZfree(c);
    }
    fklZfree(fctx);
}

static const FklVMframeContextMethodTable ReplContextMethodTable = {
    .atomic = repl_frame_atomic,
    .finalizer = repl_frame_finalizer,
    .print_backtrace = repl_frame_print_backtrace,
    .step = repl_frame_step,
};

static int replErrorCallBack(FklVMframe *f, FklVMvalue *errValue, FklVM *exe) {
    fklPrintErrBacktrace(errValue, exe, stderr);
    FklVMframe *main_frame = exe->top_frame;
    if (main_frame->prev) {
        for (; main_frame->prev->prev; main_frame = main_frame->prev)
            ;
        FklVMvarRefList *lrefl = main_frame->c.lr.lrefl;
        FklVMvalue **lref = main_frame->c.lr.lref;

        uint32_t lcount = main_frame->c.lr.lcount;
        main_frame->c.lr.lrefl = NULL;
        main_frame->c.lr.lref = NULL;
        main_frame->c.lr.lcount = 1;
        uint32_t bp = main_frame->bp;
        uint32_t sp = main_frame->c.sp;

        while (exe->top_frame->prev) {
            FklVMframe *cur = exe->top_frame;
            exe->top_frame = cur->prev;
            fklDestroyVMframe(cur, exe);
        }

        ReplCtx *ctx = FKL_TYPE_CAST(ReplCtx *, exe->top_frame->data);
        FklVMvalue **cur =
                &FKL_VM_GET_ARG(exe, main_frame, lcount - ctx->new_var_count);

        for (uint32_t i = 0; i < ctx->new_var_count; i++)
            if (!cur[i])
                cur[i] = FKL_VM_NIL;

        ctx->c->lrefl = lrefl;
        ctx->c->lref = lref;

        ctx->c->bp = bp;
        ctx->c->sp = sp;

        exe->tp = sp;
        exe->bp = bp;
        ctx->state = READY;
    }
    return 1;
}

static inline void clear_ast_queue(FklNastNodeQueue *q) {
    while (!fklNastNodeQueueIsEmpty(q)) {
        FklNastNode *const *pnode = fklNastNodeQueuePop(q);
        FKL_ASSERT(pnode);
        fklDestroyNastNode(*pnode);
    }
    fklZfree(q);
}

static int eval_frame_step(void *data, FklVM *exe) {
    FklVMframe *repl_frame = exe->top_frame;
    ReplCtx *ctx = (ReplCtx *)data;
    if (fklStringBufferLen(&ctx->c->buf) == 0)
        return 0;

    struct ReplFrameCtx *fctx = ctx->c;
    NastCreatCtx *cc = ctx->cc;

    FklNastNodeQueue *queue = fklNastNodeQueueCreate();

    FklCodegenInfo *codegen = ctx->codegen;
    FklCodegenEnv *main_env = ctx->main_env;
    FklSymbolTable *pst = &codegen->outer_ctx->public_symbol_table;
    FklNastNode *ast = NULL;
    FklGrammerMatchOuterCtx outerCtx =
            FKL_NAST_PARSE_OUTER_CTX_INIT(exe->gc->st);
    outerCtx.line = codegen->curline;

    const char *eval_expression_str = fklStringBufferBody(&ctx->c->buf);
    size_t restLen = fklStringBufferLen(&ctx->c->buf);
    int err = 0;
    size_t errLine = 0;
    FklGrammer *g = *(codegen->g);

    while (restLen) {
        FKL_VM_ACQUIRE_ST_BLOCK(exe->gc, flag) {
            ast = fklDefaultParseForCharBuf(eval_expression_str,
                    restLen,
                    &restLen,
                    &outerCtx,
                    &err,
                    &errLine,
                    &cc->symbolStack,
                    &cc->lineStack,
                    &cc->stateStack);
        }

        cc->offset = strlen(eval_expression_str) - restLen;
        codegen->curline = outerCtx.line;
        if (err == FKL_PARSE_WAITING_FOR_MORE
                || (err == FKL_PARSE_TERMINAL_MATCH_FAILED && !restLen)
                || (err == FKL_PARSE_TERMINAL_MATCH_FAILED && restLen)
                || (err == FKL_PARSE_REDUCE_FAILED)) {
            clear_ast_queue(queue);
            eval_nast_ctx_reset(cc, &ctx->c->buf, g);
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR, exe);
        } else if (ast) {
            eval_expression_str = eval_expression_str + cc->offset;
            eval_nast_ctx_reset(cc, &ctx->c->buf, g);
            fklNastNodeQueuePush2(queue, ast);
        }
    }

    size_t libs_count = codegen->libraries->size;

    FklConstTable *kt = codegen->runtime_kt;
    struct ConstArrayCount const_count = {
        .i64_count = kt->ki64t.count,
        .f64_count = kt->kf64t.count,
        .str_count = kt->kstrt.count,
        .bvec_count = kt->kbvect.count,
        .bi_count = kt->kbit.count,
    };

    FklByteCodelnt *mainCode = NULL;

    FKL_VM_ACQUIRE_ST_BLOCK(exe->gc, flag) {
        mainCode = fklGenExpressionCodeWithQueue(queue, codegen, main_env);
    }

    g = *(codegen->g);
    repl_nast_ctx_and_buf_reset(cc, &ctx->c->buf, g);

    size_t new_libs_count = codegen->libraries->size - libs_count;
    FklVMlib *new_libs = NULL;

    if (new_libs_count) {
        FklVMproc *proc = FKL_VM_PROC(ctx->c->mainProc);
        new_libs = (FklVMlib *)fklZcalloc(new_libs_count, sizeof(FklVMlib));
        FKL_ASSERT(new_libs);
        for (size_t i = 0; i < new_libs_count; i++) {
            FklVMlib *curVMlib = &new_libs[i];
            FklCodegenLib *curCGlib = &codegen->libraries->base[i + libs_count];
            fklInitVMlibWithCodegenLibRefs(curCGlib,
                    curVMlib,
                    exe,
                    proc->closure,
                    proc->rcount,
                    exe->pts);
        }
    }

    if (mainCode) {
        uint32_t o_lcount = fctx->lcount;
        ctx->state = READY;

        update_prototype_lcount(codegen->pts, main_env);

        uint32_t lcount = 0;
        FKL_VM_ACQUIRE_ST_BLOCK(exe->gc, flag) {
            fklUpdatePrototypeRef(codegen->pts,
                    main_env,
                    codegen->runtime_symbol_table,
                    pst);

            lcount = codegen->pts->pa[1].lcount;
        }

        FklVMvalue *mainProc =
                fklCreateVMvalueProc2(exe, NULL, 0, FKL_VM_NIL, 1);
        fklInitMainProcRefs(exe, mainProc);
        FklVMproc *proc = FKL_VM_PROC(mainProc);
        ctx->c->mainProc = mainProc;

        FklVMvalue *mainCodeObj = fklCreateVMvalueCodeObjMove(exe, mainCode);
        fklDestroyByteCodelnt(mainCode);
        fctx->lcount = lcount;

        mainCode = FKL_VM_CO(mainCodeObj);
        proc->lcount = fctx->lcount;
        proc->codeObj = mainCodeObj;
        proc->spc = mainCode->bc.code;
        proc->end = proc->spc + mainCode->bc.len;

        exe->base[1] = ctx->c->mainProc;
        FklVMframe *mainframe = fklCreateVMframeWithProc(exe, ctx->c->mainProc);
        fklPushVMframe(mainframe, exe);
        mainframe->c.lr.lrefl = fctx->lrefl;
        mainframe->c.lr.lref = fctx->lref;
        FklVMCompoundFrameVarRef *f = &mainframe->c.lr;

        mainframe->bp = fctx->bp;
        mainframe->retCallBack = repl_frame_ret_callback;
        fklVMframeSetSp(exe, mainframe, lcount);

        f->lcount = proc->lcount;
        alloc_more_space_for_var_ref(f, o_lcount, f->lcount);
        init_mainframe_lref(f->lref, fctx->lcount, fctx->lrefl);

        resolve_ref_and_update_const_array_for_repl(main_env,
                codegen->global_env,
                codegen->pts,
                exe,
                mainframe,
                is_need_update_const_array(&const_count, kt),
                new_libs_count,
                new_libs);

        exe->top_frame = mainframe;
        fctx->lrefl = NULL;
        fctx->lref = NULL;
        if (ctx->interactive) {
            repl_frame->errorCallBack = replErrorCallBack;
            repl_frame->t = &ReplContextMethodTable;
        }
    } else {
        fklClearCodegenPreDef(main_env);
        int is_need_update_const = is_need_update_const_array(&const_count, kt);
        if (is_need_update_const_array(&const_count, kt) || new_libs_count) {
            fklQueueWorkInIdleThread(exe,
                    update_const_array_cb,
                    &(struct UpdateConstArrayArgs){
                        .is_need_update_const_array = is_need_update_const,
                        .new_libs_count = new_libs_count,
                        .new_libs = new_libs,
                    });
            new_libs_count = 0;
            fklZfree(new_libs);
            new_libs = NULL;
        }
        ctx->state = WAITING;
        return 1;
    }

    return 1;
}

static const FklVMframeContextMethodTable EvalContextMethodTable = {
    .atomic = repl_frame_atomic,
    .finalizer = repl_frame_finalizer,
    .print_backtrace = repl_frame_print_backtrace,
    .step = eval_frame_step,
};

static inline NastCreatCtx *createNastCreatCtx(void) {
    NastCreatCtx *cc = (NastCreatCtx *)fklZcalloc(1, sizeof(NastCreatCtx));
    FKL_ASSERT(cc);
    fklAnalysisSymbolVectorInit(&cc->symbolStack, 16);
    fklUintVectorInit(&cc->lineStack, 16);
    fklParseStateVectorInit(&cc->stateStack, 16);
    fklNastPushState0ToStack(&cc->stateStack);
    return cc;
}

static inline void init_frame_to_repl_frame(FklVM *exe,
        FklCodegenInfo *codegen,
        FklCodegenEnv *main_env,
        const char *eval_expression,
        int8_t interactive) {
    FklVMframe *replFrame =
            fklCreateNewOtherObjVMframe(&ReplContextMethodTable);
    replFrame->errorCallBack = replErrorCallBack;
    exe->top_frame = replFrame;
    ReplCtx *ctx = FKL_TYPE_CAST(ReplCtx *, replFrame->data);
    ctx->c = (struct ReplFrameCtx *)fklZcalloc(1, sizeof(struct ReplFrameCtx));
    FKL_ASSERT(ctx->c);

    FklVMvalue *mainProc = fklCreateVMvalueProc2(exe, NULL, 0, FKL_VM_NIL, 1);

    ctx->exe = exe;
    ctx->c->mainProc = mainProc;

    ctx->c->stdinVal =
            FKL_VM_VAR_REF(exe->gc->builtin_refs[FKL_VM_STDIN_IDX])->v;
    ctx->codegen = codegen;
    ctx->main_env = main_env;
    NastCreatCtx *cc = createNastCreatCtx();
    ctx->cc = cc;
    ctx->state = READY;
    ctx->c->lrefl = NULL;
    ctx->interactive = interactive;
    ctx->new_var_count = 0;
    fklInitStringBuffer(&ctx->c->buf);
    if (eval_expression) {
        replFrame->errorCallBack = NULL;
        replFrame->t = &EvalContextMethodTable;
        fklStringBufferConcatWithCstr(&ctx->c->buf, eval_expression);
    }
    fklSetBp(exe);
    ctx->c->bp = exe->bp;
    ctx->c->sp = exe->bp + 1;
    fklVMstackReserve(exe, ctx->c->sp + 1);
    if (ctx->c->sp > exe->tp) {
        memset(&exe->base[exe->tp],
                0,
                (ctx->c->sp - exe->tp) * sizeof(FklVMvalue *));
        exe->tp = ctx->c->sp;
    }
}
