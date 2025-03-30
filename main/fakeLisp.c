#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>

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

static int runRepl(FklCodegenInfo *, FklCodegenEnv *main_env,
                   const char *eval_expression, int8_t interactive);
static void init_frame_to_repl_frame(FklVM *, FklCodegenInfo *codegen,
                                     FklCodegenEnv *,
                                     const char *eval_expression,
                                     int8_t interactive);
static void loadLib(FILE *, uint64_t *, FklVMlib **, FklVM *,
                    FklVMCompoundFrameVarRef *lr);
static int exitState = 0;

#define FKL_EXIT_FAILURE (255)

static inline int compileAndRun(const char *filename, int argc,
                                const char *const *argv) {
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
    FklCodegenEnv *main_env = fklInitGlobalCodegenInfo(
        &codegen, rp, fklCreateSymbolTable(), fklCreateConstTable(), 0,
        &outer_ctx, NULL, NULL, NULL);
    free(rp);
    FklByteCodelnt *mainByteCode =
        fklGenExpressionCodeWithFp(fp, &codegen, main_env);
    if (mainByteCode == NULL) {
        fklDestroyCodegenEnv(main_env);
        fklUninitCodegenInfo(&codegen);
        fklUninitCodegenOuterCtx(&outer_ctx);
        return FKL_EXIT_FAILURE;
    }
    fklUpdatePrototype(codegen.pts, main_env, codegen.runtime_symbol_table,
                       pst);
    fklDestroyCodegenEnv(main_env);
    fklPrintUndefinedRef(codegen.global_env, codegen.runtime_symbol_table, pst);

    FklCodegenLibVector *scriptLibStack = codegen.libStack;
    FklVM *anotherVM =
        fklCreateVMwithByteCode(mainByteCode, codegen.runtime_symbol_table,
                                codegen.runtime_kt, codegen.pts, 1);
    codegen.runtime_symbol_table = NULL;
    codegen.runtime_kt = NULL;
    codegen.pts = NULL;
    anotherVM->libNum = scriptLibStack->top;
    anotherVM->libs =
        (FklVMlib *)calloc((anotherVM->libNum + 1), sizeof(FklVMlib));
    FKL_ASSERT(anotherVM->libs);

    FklVMgc *gc = anotherVM->gc;
    while (!fklCodegenLibVectorIsEmpty(scriptLibStack)) {
        FklVMlib *curVMlib = &anotherVM->libs[scriptLibStack->top];
        FklCodegenLib *cur = *fklCodegenLibVectorPopBack(scriptLibStack);
        FklCodegenLibType type = cur->type;
        fklInitVMlibWithCodegenLibAndDestroy(cur, curVMlib, anotherVM,
                                             anotherVM->pts);
        if (type == FKL_CODEGEN_LIB_SCRIPT)
            fklInitMainProcRefs(anotherVM, curVMlib->proc);
    }

    fklUninitCodegenInfo(&codegen);

    fklChdir(outer_ctx.cwd);
    fklUninitCodegenOuterCtx(&outer_ctx);

    fklInitVMargs(gc, argc, argv);
    int r = fklRunVM(anotherVM);
    fklDestroySymbolTable(gc->st);
    fklDestroyConstTable(gc->kt);
    fklDestroyAllVMs(anotherVM);
    fklDestroyVMgc(gc);
    return r;
}

static inline void initLibWithPrototype(FklVMlib *lib, uint32_t num,
                                        FklFuncPrototypes *pts) {
    FklFuncPrototype *pta = pts->pa;
    for (uint32_t i = 1; i <= num; i++) {
        FklVMlib *cur = &lib[i];
        if (FKL_IS_PROC(cur->proc)) {
            FklVMproc *proc = FKL_VM_PROC(cur->proc);
            proc->lcount = pta[proc->protoId].lcount;
        }
    }
}

static inline int runCode(const char *filename, int argc,
                          const char *const *argv) {
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror(filename);
        return FKL_EXIT_FAILURE;
    }
    FklSymbolTable runtime_st;
    fklInitSymbolTable(&runtime_st);
    FklConstTable runtime_kt;
    fklInitConstTable(&runtime_kt);
    fklLoadSymbolTable(fp, &runtime_st);
    fklLoadConstTable(fp, &runtime_kt);
    FklFuncPrototypes *prototypes = fklLoadFuncPrototypes(fp);
    FklByteCodelnt *mainCodelnt = fklLoadByteCodelnt(fp);

    FklVM *anotherVM = fklCreateVMwithByteCode(mainCodelnt, &runtime_st,
                                               &runtime_kt, prototypes, 1);

    FklVMgc *gc = anotherVM->gc;

    loadLib(fp, &anotherVM->libNum, &anotherVM->libs, anotherVM,
            fklGetCompoundFrameLocRef(anotherVM->top_frame));

    fclose(fp);

    initLibWithPrototype(anotherVM->libs, anotherVM->libNum, anotherVM->pts);
    fklInitVMargs(anotherVM->gc, argc, argv);
    int r = fklRunVM(anotherVM);
    fklUninitSymbolTable(&runtime_st);
    fklUninitConstTable(&runtime_kt);
    fklDestroyAllVMs(anotherVM);
    fklDestroyVMgc(gc);
    return r;
}

static inline int runPreCompile(const char *filename, int argc,
                                const char *const *argv) {
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
    int load_result = fklLoadPreCompile(pts, &macro_pts, &scriptLibStack,
                                        &macroScriptLibStack, &runtime_st,
                                        &runtime_kt, &ctx, rp, fp, &errorStr);
    fklUninitFuncPrototypes(&macro_pts);
    while (!fklCodegenLibVectorIsEmpty(&macroScriptLibStack))
        fklDestroyCodegenLib(*fklCodegenLibVectorPopBack(&macroScriptLibStack));
    fklCodegenLibVectorUninit(&macroScriptLibStack);
    free(rp);
    fclose(fp);
    fklUninitSymbolTable(&ctx.public_symbol_table);
    fklUninitConstTable(&ctx.public_kt);
    if (load_result) {
        fklUninitSymbolTable(&runtime_st);
        fklDestroyFuncPrototypes(pts);
        while (!fklCodegenLibVectorIsEmpty(&scriptLibStack))
            fklDestroyCodegenLib(*fklCodegenLibVectorPopBack(&scriptLibStack));
        fklCodegenLibVectorUninit(&scriptLibStack);
        if (errorStr) {
            fprintf(stderr, "%s\n", errorStr);
            free(errorStr);
        }
        fprintf(stderr, "%s: Load pre-compile file failed.\n", filename);
        return 1;
    }

    FklCodegenLib *main_lib = *fklCodegenLibVectorPopBack(&scriptLibStack);
    FklByteCodelnt *main_byte_code = main_lib->bcl;
    fklDestroyCodegenLibExceptBclAndDll(main_lib);

    FklVM *anotherVM = fklCreateVMwithByteCode(main_byte_code, &runtime_st,
                                               &runtime_kt, pts, 1);

    anotherVM->libNum = scriptLibStack.top + 1;
    anotherVM->libs =
        (FklVMlib *)calloc((anotherVM->libNum + 1), sizeof(FklVMlib));
    FKL_ASSERT(anotherVM->libs);

    FklVMgc *gc = anotherVM->gc;
    while (!fklCodegenLibVectorIsEmpty(&scriptLibStack)) {
        FklVMlib *curVMlib = &anotherVM->libs[scriptLibStack.top];
        FklCodegenLib *cur = *fklCodegenLibVectorPopBack(&scriptLibStack);
        FklCodegenLibType type = cur->type;
        fklInitVMlibWithCodegenLibAndDestroy(cur, curVMlib, anotherVM,
                                             anotherVM->pts);
        if (type == FKL_CODEGEN_LIB_SCRIPT)
            fklInitMainProcRefs(anotherVM, curVMlib->proc);
    }
    fklCodegenLibVectorUninit(&scriptLibStack);

    fklInitVMargs(anotherVM->gc, argc, argv);
    int r = fklRunVM(anotherVM);
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

static inline int process_priority(
    char *str, enum PriorityEnum priority_array[PRIORITY_PACKAGE_PRECOMPILE]) {
    struct {
        enum PriorityEnum type;
        int used;
    } priority_used_array[PRIORITY_PACKAGE_PRECOMPILE + 1] = {
        {PRIORITY_UNSET, 0},           {PRIORITY_PACKAGE_SCRIPT, 0},
        {PRIORITY_MODULE_SCRIPT, 0},   {PRIORITY_PACKAGE_BYTECODE, 0},
        {PRIORITY_MODULE_BYTECODE, 0}, {PRIORITY_PACKAGE_PRECOMPILE, 0},
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

static inline int
process_specify(char *str,
                enum PriorityEnum priority_array[PRIORITY_PACKAGE_PRECOMPILE]) {
    struct {
        enum PriorityEnum type;
        int used;
    } priority_used_array[PRIORITY_PACKAGE_PRECOMPILE + 1] = {
        {PRIORITY_UNSET, 0},           {PRIORITY_PACKAGE_SCRIPT, 0},
        {PRIORITY_MODULE_SCRIPT, 0},   {PRIORITY_PACKAGE_BYTECODE, 0},
        {PRIORITY_MODULE_BYTECODE, 0}, {PRIORITY_PACKAGE_PRECOMPILE, 0},
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

        eval =
            arg_str0("e", "eval", "<expr>", "evaluate the given expressions"),

        specify =
            arg_str0("s", "specify", NULL,
                     "specify the file type of module, the argument can be "
                     "'package-script', 'module-script', 'package-bytecode', "
                     "'module-bytecode' and 'package-precompile'. "
                     "Also allow to combine them"),

        priority = arg_str0("p", "priority", NULL,
                            "order of which type to filename be handled. the "
                            "default priority is "
                            "'package-script, module-script, package-bytecode, "
                            "module-bytecode, package-precompile'. "
                            "'~' can be used as placeholder"),

        rest =
            arg_strn(NULL, NULL, "file [args]", 0, argc + 2, "file and args"),

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
        FklCodegenEnv *main_env = fklInitGlobalCodegenInfo(
            &codegen, NULL, &outer_ctx.public_symbol_table,
            &outer_ctx.public_kt, 0, &outer_ctx, NULL, NULL, NULL);
        exitState =
            runRepl(&codegen, main_env, eval->count > 0 ? eval->sval[0] : NULL,
                    interactive->count > 0);
        codegen.runtime_symbol_table = NULL;
        FklCodegenLibVector *loadedLibStack = codegen.libStack;
        while (!fklCodegenLibVectorIsEmpty(loadedLibStack)) {
            FklCodegenLib *lib = *fklCodegenLibVectorPopBack(loadedLibStack);
            fklDestroyCodegenLibMacroScope(lib);
            if (lib->type == FKL_CODEGEN_LIB_DLL)
                uv_dlclose(&lib->dll);
            fklUninitCodegenLibInfo(lib);
            free(lib);
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
                PRIORITY_UNSET};

            if (priority->count > 0) {
                if (process_priority((char *)priority->sval[0], priority_array))
                    goto error;
            } else if (specify->count) {
                if (process_specify((char *)specify->sval[0], priority_array))
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
            char *module_script_file = fklStrCat(
                fklCopyCstr(main_script_buf.buf), FKL_SCRIPT_FILE_EXTENSION);
            char *module_bytecode_file = fklStrCat(
                fklCopyCstr(main_script_buf.buf), FKL_BYTECODE_FILE_EXTENSION);

            fklStringBufferConcatWithCstr(&main_script_buf,
                                          FKL_PATH_SEPARATOR_STR);
            fklStringBufferConcatWithCstr(&main_script_buf, "main.fkl");

            char *main_code_file = fklStrCat(fklCopyCstr(main_script_buf.buf),
                                             FKL_BYTECODE_FKL_SUFFIX_STR);
            char *main_pre_file = fklStrCat(fklCopyCstr(main_script_buf.buf),
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
            free(main_code_file);
            free(main_pre_file);
            free(module_script_file);
            free(module_bytecode_file);
        }
    }

exit:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exitState;
}

static int runRepl(FklCodegenInfo *codegen, FklCodegenEnv *main_env,
                   const char *eval_expression, int8_t interactive) {
    FklVM *anotherVM =
        fklCreateVMwithByteCode(NULL, codegen->runtime_symbol_table,
                                codegen->runtime_kt, codegen->pts, 1);
    anotherVM->libs = (FklVMlib *)calloc(1, sizeof(FklVMlib));
    FKL_ASSERT(anotherVM->libs);

    init_frame_to_repl_frame(anotherVM, codegen, main_env, eval_expression,
                             interactive);

    int err = fklRunVM(anotherVM);

    FklVMgc *gc = anotherVM->gc;
    fklDestroyAllVMs(anotherVM);
    fklDestroyVMgc(gc);
    codegen->pts = NULL;
    return err;
}

static void loadLib(FILE *fp, uint64_t *plibNum, FklVMlib **plibs, FklVM *exe,
                    FklVMCompoundFrameVarRef *lr) {
    fread(plibNum, sizeof(uint64_t), 1, fp);
    size_t libNum = *plibNum;
    FklVMlib *libs = (FklVMlib *)calloc((libNum + 1), sizeof(FklVMlib));
    FKL_ASSERT(libs);
    *plibs = libs;
    for (size_t i = 1; i <= libNum; i++) {
        FklCodegenLibType libType = FKL_CODEGEN_LIB_SCRIPT;
        fread(&libType, sizeof(char), 1, fp);
        if (libType == FKL_CODEGEN_LIB_SCRIPT) {
            uint32_t protoId = 0;
            fread(&protoId, sizeof(protoId), 1, fp);
            FklByteCodelnt *bcl = fklLoadByteCodelnt(fp);
            FklVMvalue *codeObj = fklCreateVMvalueCodeObj(exe, bcl);
            fklInitVMlibWithCodeObj(&libs[i], codeObj, exe, protoId);
            fklInitMainProcRefs(exe, libs[i].proc);
        } else {
            uint64_t len = 0;
            fread(&len, sizeof(uint64_t), 1, fp);
            FklVMvalue *sv = fklCreateVMvalueStr2(exe, len, NULL);
            FklString *s = FKL_VM_STR(sv);
            fread(s->str, len, 1, fp);
            fklInitVMlib(&libs[i], sv);
        }
    }
}

typedef struct {
    size_t offset;
    FklPtrVector stateStack;
    FklAnalysisSymbolVector symbolStack;
    FklUintVector lineStack;
} NastCreatCtx;

#include <replxx.h>

struct ReplFrameCtx {
    FklVMvarRefList *lrefl;
    FklVMvalue **lref;
    FklVMvalue *ret_proc;
    uint32_t lcount;
};

typedef struct {
    FklCodegenInfo *codegen;
    FklCodegenEnv *main_env;
    NastCreatCtx *cc;
    FklVM *exe;
    FklVMvalue *stdinVal;
    FklVMvalue *mainProc;
    FklStringBuffer buf;
    enum {
        READY,
        WAITING,
        READING,
    } state : 8;
    int8_t eof;
    int8_t interactive;
    uint32_t new_var_count;
    Replxx *replxx;
    struct ReplFrameCtx *fctx;
} ReplCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(ReplCtx);

static inline void inc_lref(FklVMCompoundFrameVarRef *lr, uint32_t lcount) {
    if (!lr->lref) {
        lr->lref = (FklVMvalue **)calloc(lcount, sizeof(FklVMvalue *));
        FKL_ASSERT(lr->lref);
    }
}

static inline FklVMvalue *insert_local_ref(FklVMCompoundFrameVarRef *lr,
                                           FklVMvalue *ref, uint32_t cidx) {
    FklVMvarRefList *rl = (FklVMvarRefList *)malloc(sizeof(FklVMvarRefList));
    FKL_ASSERT(rl);
    rl->ref = ref;
    rl->next = lr->lrefl;
    lr->lrefl = rl;
    lr->lref[cidx] = ref;
    return ref;
}

struct ProcessUnresolveRefArg {
    FklSymbolDef *def;
    uint32_t prototypeId;
    uint32_t idx;
};

struct ProcessUnresolveRefArgStack {
    FklVMvalue **loc;
    FklVMCompoundFrameVarRef *lr;
    struct ProcessUnresolveRefArg *base;
    uint32_t top;
    uint32_t size;
    int is_need_update_const_array;
};

static inline void
process_unresolve_work_func(FklVM *exe, struct ProcessUnresolveRefArgStack *s) {
    if (s->is_need_update_const_array)
        fklVMgcUpdateConstArray(exe->gc, exe->gc->kt);
    struct ProcessUnresolveRefArg *cur = s->base;
    const struct ProcessUnresolveRefArg *const end = &cur[s->top];
    FklVMvalue **loc = s->loc;
    FklVMCompoundFrameVarRef *lr = s->lr;
    for (; cur < end; cur++) {
        FklSymbolDef *def = cur->def;
        uint32_t prototypeId = cur->prototypeId;
        uint32_t idx = cur->idx;
        for (FklVMvalue *v = exe->gc->head; v; v = v->next)
            if (FKL_IS_PROC(v) && FKL_VM_PROC(v)->protoId == prototypeId) {
                FklVMproc *proc = FKL_VM_PROC(v);
                if (lr->lref[def->idx])
                    proc->closure[idx] = lr->lref[def->idx];
                else {
                    FklVMvalue *ref = proc->closure[idx];
                    FKL_VM_VAR_REF(ref)->idx = def->idx;
                    FKL_VM_VAR_REF(ref)->ref = &loc[def->idx];
                    insert_local_ref(lr, ref, def->idx);
                }
            }

        for (FklVMvalue *v = exe->obj_head; v; v = v->next)
            if (FKL_IS_PROC(v) && FKL_VM_PROC(v)->protoId == prototypeId) {
                FklVMproc *proc = FKL_VM_PROC(v);
                if (lr->lref[def->idx])
                    proc->closure[idx] = lr->lref[def->idx];
                else {
                    FklVMvalue *ref = proc->closure[idx];
                    FKL_VM_VAR_REF(ref)->idx = def->idx;
                    FKL_VM_VAR_REF(ref)->ref = &loc[def->idx];
                    insert_local_ref(lr, ref, def->idx);
                }
            }

        for (FklVM *cur = exe->next; cur != exe; cur = cur->next) {
            for (FklVMvalue *v = cur->obj_head; v; v = v->next)
                if (FKL_IS_PROC(v) && FKL_VM_PROC(v)->protoId == prototypeId) {
                    FklVMproc *proc = FKL_VM_PROC(v);
                    if (lr->lref[def->idx])
                        proc->closure[idx] = lr->lref[def->idx];
                    else {
                        FklVMvalue *ref = proc->closure[idx];
                        FKL_VM_VAR_REF(ref)->idx = def->idx;
                        FKL_VM_VAR_REF(ref)->ref = &loc[def->idx];
                        insert_local_ref(lr, ref, def->idx);
                    }
                }
        }
    }
}

static void process_update_const_array_cb(FklVM *exe, void *arg) {
    fklVMgcUpdateConstArray(exe->gc, exe->gc->kt);
}

static void process_unresolve_ref_cb(FklVM *exe, void *arg) {
    struct ProcessUnresolveRefArgStack *aarg =
        (struct ProcessUnresolveRefArgStack *)arg;
    process_unresolve_work_func(exe, aarg);
}

#define PROCESS_UNRESOLVE_REF_ARG_STACK_INC (16)

static inline void
process_unresolve_ref_arg_push(struct ProcessUnresolveRefArgStack *s,
                               FklSymbolDef *def, uint32_t prototypeId,
                               uint32_t idx) {
    if (s->top == s->size) {
        struct ProcessUnresolveRefArg *tmpData =
            (struct ProcessUnresolveRefArg *)fklRealloc(
                s->base, (s->size + PROCESS_UNRESOLVE_REF_ARG_STACK_INC)
                             * sizeof(struct ProcessUnresolveRefArg));
        FKL_ASSERT(tmpData);
        s->base = tmpData;
        s->size += PROCESS_UNRESOLVE_REF_ARG_STACK_INC;
    }
    struct ProcessUnresolveRefArg *arg = &s->base[s->top++];
    arg->def = def;
    arg->prototypeId = prototypeId;
    arg->idx = idx;
}

static inline void
init_process_unresolve_ref_arg_stack(struct ProcessUnresolveRefArgStack *s,
                                     FklVMvalue **loc,
                                     FklVMCompoundFrameVarRef *lr) {
    s->loc = loc;
    s->lr = lr;
    s->size = PROCESS_UNRESOLVE_REF_ARG_STACK_INC;
    s->base = (struct ProcessUnresolveRefArg *)malloc(
        sizeof(struct ProcessUnresolveRefArg) * s->size);
    FKL_ASSERT(s->base);
    s->top = 0;
}

static inline void
uninit_process_unresolve_ref_arg_stack(struct ProcessUnresolveRefArgStack *s) {
    s->loc = NULL;
    s->lr = NULL;
    s->size = 0;
    free(s->base);
    s->base = NULL;
    s->top = 0;
}

static inline void process_unresolve_ref_and_update_const_array_for_repl(
    FklCodegenEnv *env, FklCodegenEnv *global_env, FklFuncPrototypes *cp,
    FklVM *exe, FklVMvalue **loc, FklVMframe *mainframe,
    int is_need_update_const_array) {
    FklVMCompoundFrameVarRef *lr = fklGetCompoundFrameLocRef(mainframe);
    struct ProcessUnresolveRefArgStack process_unresolve_ref_arg_stack = {
        .size = 0, .is_need_update_const_array = is_need_update_const_array};
    FklUnReSymbolRefVector *urefs = &env->uref;
    FklFuncPrototype *pts = cp->pa;
    FklUnReSymbolRefVector urefs1;
    fklUnReSymbolRefVectorInit(&urefs1, 16);
    uint32_t count = urefs->top;
    for (uint32_t i = 0; i < count; i++) {
        FklUnReSymbolRef *uref = urefs->base[i];
        FklFuncPrototype *cpt = &pts[uref->prototypeId];
        FklSymbolDef *ref = &cpt->refs[uref->idx];
        FklSymbolDef *def =
            fklFindSymbolDefByIdAndScope(uref->id, uref->scope, env);
        if (def) {
            env->slotFlags[def->idx] = FKL_CODEGEN_ENV_SLOT_REF;
            ref->cidx = def->idx;
            ref->isLocal = 1;
            uint32_t prototypeId = uref->prototypeId;
            uint32_t idx = ref->idx;
            inc_lref(lr, lr->lcount);
            if (process_unresolve_ref_arg_stack.size == 0)
                init_process_unresolve_ref_arg_stack(
                    &process_unresolve_ref_arg_stack, loc, lr);

            process_unresolve_ref_arg_push(&process_unresolve_ref_arg_stack,
                                           def, prototypeId, idx);

            free(uref);
        } else if (env->prev != global_env) {
            ref->cidx = fklAddCodegenRefBySidRetIndex(uref->id, env, uref->fid,
                                                      uref->line, uref->assign);
            free(uref);
        } else
            fklUnReSymbolRefVectorPushBack2(&urefs1, uref);
    }
    urefs->top = 0;
    while (!fklUnReSymbolRefVectorIsEmpty(&urefs1))
        fklUnReSymbolRefVectorPushBack2(
            urefs, *fklUnReSymbolRefVectorPopBack(&urefs1));
    fklUnReSymbolRefVectorUninit(&urefs1);

    if (process_unresolve_ref_arg_stack.top > 0 || is_need_update_const_array) {
        fklQueueWorkInIdleThread(exe, process_unresolve_ref_cb,
                                 &process_unresolve_ref_arg_stack);
        uninit_process_unresolve_ref_arg_stack(
            &process_unresolve_ref_arg_stack);
    }
}

static inline void update_prototype_lcount(FklFuncPrototypes *cp,
                                           FklCodegenEnv *env) {
    FklFuncPrototype *pts = &cp->pa[env->prototypeId];
    pts->lcount = env->lcount;
}

static inline void alloc_more_space_for_var_ref(FklVMCompoundFrameVarRef *lr,
                                                uint32_t i, uint32_t n) {
    if (lr->lref && i < n) {
        FklVMvalue **lref =
            (FklVMvalue **)fklRealloc(lr->lref, sizeof(FklVMvalue *) * n);
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
    fklStringBufferClear(s);
    s->buf[0] = '\0';
    FklAnalysisSymbolVector *ss = &cc->symbolStack;
    while (!fklAnalysisSymbolVectorIsEmpty(ss)) {
        FklAnalysisSymbol *s = *fklAnalysisSymbolVectorPopBack(ss);
        fklDestroyNastNode(s->ast);
        free(s);
    }
    cc->stateStack.top = 0;
    cc->lineStack.top = 0;
    if (g && g->aTable.num)
        fklAnalysisStateVectorPushBack2(
            (FklAnalysisStateVector *)&cc->stateStack, &g->aTable.states[0]);
    else
        fklNastPushState0ToStack((FklParseStateFuncVector *)&cc->stateStack);
}

static inline void eval_nast_ctx_reset(NastCreatCtx *cc, FklStringBuffer *s,
                                       FklGrammer *g) {
    cc->offset = 0;
    FklAnalysisSymbolVector *ss = &cc->symbolStack;
    while (!fklAnalysisSymbolVectorIsEmpty(ss)) {
        FklAnalysisSymbol *s = *fklAnalysisSymbolVectorPopBack(ss);
        fklDestroyNastNode(s->ast);
        free(s);
    }
    cc->stateStack.top = 0;
    cc->lineStack.top = 0;
    if (g && g->aTable.num)
        fklAnalysisStateVectorPushBack2(
            (FklAnalysisStateVector *)&cc->stateStack, &g->aTable.states[0]);
    else
        fklNastPushState0ToStack((FklParseStateFuncVector *)&cc->stateStack);
}

#define REPL_PROMPT ">>> "
#define RETVAL_PREFIX ";=> "

static inline const char *replxx_input_string_buffer(Replxx *replxx,
                                                     FklStringBuffer *buf) {
    const char *next = replxx_input(replxx, buf->index ? "" : REPL_PROMPT);
    if (next)
        fklStringBufferConcatWithCstr(buf, next);
    else
        fputc('\n', stdout);
    return next;
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

#define I24_L8_MASK (0xFF)
#define I32_L16_MASK (0xFFFF)

static inline void set_ins_uc(FklInstruction *ins, uint32_t k) {
    ins->au = k & I24_L8_MASK;
    ins->bu = k >> FKL_BYTE_WIDTH;
}

static inline void set_ins_ux(FklInstruction *ins, uint32_t k) {
    ins[0].bu = k & I32_L16_MASK;
    ins[1].op = FKL_OP_EXTRA_ARG;
    ins[1].bu = k >> FKL_I16_WIDTH;
}

#undef I24_L8_MASK
#undef I32_L16_MASK

static inline uint32_t make_get_loc_ins(FklInstruction ins[3], uint32_t k) {
    uint32_t l = 1;
    if (k <= UINT16_MAX) {
        ins[0].op = FKL_OP_GET_LOC;
        ins[0].bu = k;
    } else if (k <= FKL_U24_MAX) {
        ins[0].op = FKL_OP_GET_LOC + 1;
        set_ins_uc(&ins[0], k);
    } else {
        ins[0].op = FKL_OP_GET_LOC + 2;
        set_ins_ux(ins, k);
        l = 2;
    }
    return l;
}

static inline void set_last_ins_get_loc(FklByteCodelnt *bcl, uint32_t idx) {
    FklInstruction ins[3] = {{.op = FKL_OP_GET_LOC}};
    uint32_t l = make_get_loc_ins(ins, idx);
    FklInstruction *last_ins = &bcl->bc->code[bcl->bc->len - 1];
    *last_ins = ins[0];
    for (uint32_t i = 1; i < l; i++)
        fklByteCodeLntPushBackIns(bcl, &ins[i], 0, 0, 0);
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

static int repl_frame_step(void *data, FklVM *exe) {
    ReplCtx *ctx = (ReplCtx *)data;
    struct ReplFrameCtx *fctx = ctx->fctx;
    NastCreatCtx *cc = ctx->cc;

    if (ctx->state == READY) {
        ctx->state = WAITING;
        return 1;
    } else if (ctx->state == WAITING) {
        exe->ltp = fctx->lcount;
        ctx->state = READING;
        if (exe->tp != 0) {
            fputs(RETVAL_PREFIX, stdout);
            fklDBG_printVMstack(exe, stdout, 0, exe->gc);
        }
        exe->tp = 0;

        fklUnlockThread(exe);
        ctx->eof = replxx_input_string_buffer(ctx->replxx, &ctx->buf) == NULL;
        fklLockThread(exe);
        return 1;
    } else
        ctx->state = WAITING;

    FklCodegenInfo *codegen = ctx->codegen;
    FklCodegenEnv *main_env = ctx->main_env;
    FklSymbolTable *pst = &codegen->outer_ctx->public_symbol_table;
    FklStringBuffer *s = &ctx->buf;
    int is_eof = ctx->eof;
    FklNastNode *ast = NULL;
    FklGrammerMatchOuterCtx outerCtx =
        FKL_NAST_PARSE_OUTER_CTX_INIT(exe->gc->st);
    outerCtx.line = codegen->curline;
    size_t restLen = fklStringBufferLen(s) - cc->offset;
    int err = 0;
    size_t errLine = 0;
    FklGrammer *g = *(codegen->g);

    fklVMacquireSt(exe->gc);
    if (g && g->aTable.num) {
        ast = fklParseWithTableForCharBuf(
            g, fklStringBufferBody(s) + cc->offset, restLen, &restLen,
            &outerCtx, exe->gc->st, &err, &errLine, &cc->symbolStack,
            &cc->lineStack, (FklAnalysisStateVector *)&cc->stateStack);
    } else {
        ast = fklDefaultParseForCharBuf(
            fklStringBufferBody(s) + cc->offset, restLen, &restLen, &outerCtx,
            &err, &errLine, &cc->symbolStack, &cc->lineStack,
            (FklParseStateFuncVector *)&cc->stateStack);
    }
    fklVMreleaseSt(exe->gc);

    cc->offset = fklStringBufferLen(s) - restLen;
    codegen->curline = outerCtx.line;
    if (!restLen && cc->symbolStack.top == 0 && is_eof)
        return 0;
    else if ((err == FKL_PARSE_WAITING_FOR_MORE
              || (err == FKL_PARSE_TERMINAL_MATCH_FAILED && !restLen))
             && is_eof) {
        repl_nast_ctx_and_buf_reset(cc, s, g);
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNEXPECTED_EOF, exe);
    } else if (err == FKL_PARSE_TERMINAL_MATCH_FAILED && restLen) {
        repl_nast_ctx_and_buf_reset(cc, s, g);
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR, exe);
    } else if (err == FKL_PARSE_REDUCE_FAILED) {
        repl_nast_ctx_and_buf_reset(cc, s, g);
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR, exe);
    } else if (ast) {
        if (restLen) {
            size_t idx = fklStringBufferLen(s) - restLen;
            replxx_set_preload_buffer(ctx->replxx, &s->buf[idx]);
            s->buf[idx] = '\0';
        }

        fklMakeNastNodeRef(ast);
        size_t libNum = codegen->libStack->top;

        fklVMacquireSt(exe->gc);
        FklConstTable *kt = codegen->runtime_kt;
        struct ConstArrayCount const_count = {
            .i64_count = kt->ki64t.count,
            .f64_count = kt->kf64t.count,
            .str_count = kt->kstrt.count,
            .bvec_count = kt->kbvect.count,
            .bi_count = kt->kbit.count,
        };
        FklByteCodelnt *mainCode = fklGenExpressionCode(ast, main_env, codegen);
        fklVMreleaseSt(exe->gc);

        replxx_history_add(ctx->replxx, s->buf);
        g = *(codegen->g);
        repl_nast_ctx_and_buf_reset(cc, s, g);
        fklDestroyNastNode(ast);
        size_t unloadlibNum = codegen->libStack->top - libNum;
        if (unloadlibNum) {
            FklVMproc *proc = FKL_VM_PROC(ctx->mainProc);
            libNum += unloadlibNum;
            FklVMlib *nlibs =
                (FklVMlib *)calloc((libNum + 1), sizeof(FklVMlib));
            FKL_ASSERT(nlibs);
            memcpy(nlibs, exe->libs, sizeof(FklVMlib) * (exe->libNum + 1));
            for (size_t i = exe->libNum; i < libNum; i++) {
                FklVMlib *curVMlib = &nlibs[i + 1];
                FklCodegenLib *curCGlib = codegen->libStack->base[i];
                fklInitVMlibWithCodegenLibRefs(curCGlib, curVMlib, exe,
                                               proc->closure, proc->rcount, 0,
                                               exe->pts);
            }
            FklVMlib *prev = exe->libs;
            exe->libs = nlibs;
            exe->libNum = libNum;
            free(prev);
        }
        if (mainCode) {
            uint32_t o_lcount = fctx->lcount;
            ctx->state = READY;

            update_prototype_lcount(codegen->pts, main_env);

            fklVMacquireSt(exe->gc);
            fklUpdatePrototypeRef(codegen->pts, main_env,
                                  codegen->runtime_symbol_table, pst);

            uint32_t ret_proc_idx = codegen->pts->pa[1].lcount;
            set_last_ins_get_loc(mainCode, ret_proc_idx);
            FklInstruction ins = {
                .op = FKL_OP_CALL,
            };
            fklByteCodeLntPushBackIns(mainCode, &ins, 0, 0, 0);
            fklVMreleaseSt(exe->gc);

            FklVMvalue *mainProc =
                fklCreateVMvalueProc(exe, NULL, 0, FKL_VM_NIL, 1);
            fklInitMainProcRefs(exe, mainProc);
            FklVMproc *proc = FKL_VM_PROC(mainProc);
            ctx->mainProc = mainProc;

            FklVMvalue *mainCodeObj = fklCreateVMvalueCodeObj(exe, mainCode);
            fctx->lcount = ret_proc_idx;
            ctx->new_var_count = fctx->lcount - o_lcount;

            mainCode = FKL_VM_CO(mainCodeObj);
            proc->lcount = fctx->lcount;
            proc->codeObj = mainCodeObj;
            proc->spc = mainCode->bc->code;
            proc->end = proc->spc + mainCode->bc->len;

            FklVMframe *mainframe = fklCreateVMframeWithProcValue(
                exe, ctx->mainProc, exe->top_frame);
            mainframe->c.lr.lrefl = fctx->lrefl;
            mainframe->c.lr.lref = fctx->lref;
            FklVMCompoundFrameVarRef *f = &mainframe->c.lr;
            f->base = 0;
            f->loc = fklAllocMoreSpaceForMainFrame(exe, proc->lcount + 1);
            f->loc[ret_proc_idx] = fctx->ret_proc;
            f->lcount = proc->lcount;
            alloc_more_space_for_var_ref(f, o_lcount, f->lcount);
            init_mainframe_lref(f->lref, fctx->lcount, fctx->lrefl);

            process_unresolve_ref_and_update_const_array_for_repl(
                main_env, codegen->global_env, codegen->pts, exe, exe->locv,
                mainframe, is_need_update_const_array(&const_count, kt));

            exe->top_frame = mainframe;

            return 1;
        } else {
            fklClearCodegenPreDef(main_env);
            if (is_need_update_const_array(&const_count, kt))
                fklQueueWorkInIdleThread(exe, process_update_const_array_cb,
                                         NULL);
            ctx->state = WAITING;
            return 1;
        }
    } else
        fklStringBufferPutc(&ctx->buf, '\n');
    return 1;
}

static void repl_frame_print_backtrace(void *data, FILE *fp, FklVMgc *gc) {}

static void repl_frame_atomic(void *data, FklVMgc *gc) {
    ReplCtx *ctx = (ReplCtx *)data;
    struct ReplFrameCtx *fctx = ctx->fctx;
    fklVMgcToGray(ctx->stdinVal, gc);
    fklVMgcToGray(ctx->mainProc, gc);
    FklVMvalue **locv = ctx->exe->locv;
    uint32_t lcount = fctx->lcount;
    for (uint32_t i = 0; i < lcount; i++)
        fklVMgcToGray(locv[i], gc);
    for (FklVMvarRefList *l = fctx->lrefl; l; l = l->next)
        fklVMgcToGray(l->ref, gc);
    fklVMgcToGray(fctx->ret_proc, gc);
}

static inline void destroyNastCreatCtx(NastCreatCtx *cc) {
    fklPtrVectorUninit(&cc->stateStack);
    while (!fklAnalysisSymbolVectorIsEmpty(&cc->symbolStack)) {
        FklAnalysisSymbol *s =
            *fklAnalysisSymbolVectorPopBack(&cc->symbolStack);
        fklDestroyNastNode(s->ast);
        free(s);
    }
    fklUintVectorUninit(&cc->lineStack);
    fklAnalysisSymbolVectorUninit(&cc->symbolStack);
    free(cc);
}

static void repl_frame_finalizer(void *data) {
    ReplCtx *ctx = (ReplCtx *)data;
    fklUninitStringBuffer(&ctx->buf);
    destroyNastCreatCtx(ctx->cc);
    replxx_end(ctx->replxx);
    struct ReplFrameCtx *fctx = ctx->fctx;
    free(fctx->lref);

    for (FklVMvarRefList *l = fctx->lrefl; l;) {
        fklCloseVMvalueVarRef(l->ref);
        FklVMvarRefList *c = l;
        l = c->next;
        free(c);
    }
    free(fctx);
}

static const FklVMframeContextMethodTable ReplContextMethodTable = {
    .atomic = repl_frame_atomic,
    .finalizer = repl_frame_finalizer,
    .copy = NULL,
    .print_backtrace = repl_frame_print_backtrace,
    .step = repl_frame_step,
};

static int replErrorCallBack(FklVMframe *f, FklVMvalue *errValue, FklVM *exe) {
    exe->tp = 0;
    exe->bp = 0;
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

        while (exe->top_frame->prev) {
            FklVMframe *cur = exe->top_frame;
            exe->top_frame = cur->prev;
            fklDestroyVMframe(cur, exe);
        }

        ReplCtx *ctx = (ReplCtx *)exe->top_frame->data;
        FklVMvalue **cur = &exe->locv[lcount - ctx->new_var_count];
        for (uint32_t i = 0; i < ctx->new_var_count; i++)
            if (!cur[i])
                cur[i] = FKL_VM_NIL;

        ctx->fctx->lrefl = lrefl;
        ctx->fctx->lref = lref;
        ctx->state = READY;
    }
    return 1;
}

static inline void clear_ast_queue(FklPtrQueue *q) {
    while (!fklIsPtrQueueEmpty(q))
        fklDestroyNastNode(fklPopPtrQueue(q));
    free(q);
}

static int eval_frame_step(void *data, FklVM *exe) {
    FklVMframe *repl_frame = exe->top_frame;
    ReplCtx *ctx = (ReplCtx *)data;
    if (fklStringBufferLen(&ctx->buf) == 0)
        return 0;

    struct ReplFrameCtx *fctx = ctx->fctx;
    NastCreatCtx *cc = ctx->cc;

    FklPtrQueue *queue = fklCreatePtrQueue();
    ;

    FklCodegenInfo *codegen = ctx->codegen;
    FklCodegenEnv *main_env = ctx->main_env;
    FklSymbolTable *pst = &codegen->outer_ctx->public_symbol_table;
    FklNastNode *ast = NULL;
    FklGrammerMatchOuterCtx outerCtx =
        FKL_NAST_PARSE_OUTER_CTX_INIT(exe->gc->st);
    outerCtx.line = codegen->curline;

    const char *eval_expression_str = fklStringBufferBody(&ctx->buf);
    size_t restLen = fklStringBufferLen(&ctx->buf);
    int err = 0;
    size_t errLine = 0;
    FklGrammer *g = *(codegen->g);

    while (restLen) {
        fklVMacquireSt(exe->gc);

        ast = fklDefaultParseForCharBuf(
            eval_expression_str, restLen, &restLen, &outerCtx, &err, &errLine,
            &cc->symbolStack, &cc->lineStack,
            (FklParseStateFuncVector *)&cc->stateStack);

        fklVMreleaseSt(exe->gc);

        cc->offset = strlen(eval_expression_str) - restLen;
        codegen->curline = outerCtx.line;
        if (err == FKL_PARSE_WAITING_FOR_MORE
            || (err == FKL_PARSE_TERMINAL_MATCH_FAILED && !restLen)
            || (err == FKL_PARSE_TERMINAL_MATCH_FAILED && restLen)
            || (err == FKL_PARSE_REDUCE_FAILED)) {
            clear_ast_queue(queue);
            eval_nast_ctx_reset(cc, &ctx->buf, g);
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR, exe);
        } else if (ast) {
            eval_expression_str = eval_expression_str + cc->offset;
            eval_nast_ctx_reset(cc, &ctx->buf, g);
            fklPushPtrQueue(ast, queue);
        }
    }

    size_t libNum = codegen->libStack->top;

    fklVMacquireSt(exe->gc);
    FklConstTable *kt = codegen->runtime_kt;
    struct ConstArrayCount const_count = {
        .i64_count = kt->ki64t.count,
        .f64_count = kt->kf64t.count,
        .str_count = kt->kstrt.count,
        .bvec_count = kt->kbvect.count,
        .bi_count = kt->kbit.count,
    };
    FklByteCodelnt *mainCode =
        fklGenExpressionCodeWithQueue(queue, codegen, main_env);
    fklVMreleaseSt(exe->gc);

    g = *(codegen->g);
    repl_nast_ctx_and_buf_reset(cc, &ctx->buf, g);

    size_t unloadlibNum = codegen->libStack->top - libNum;
    if (unloadlibNum) {
        FklVMproc *proc = FKL_VM_PROC(ctx->mainProc);
        libNum += unloadlibNum;
        FklVMlib *nlibs = (FklVMlib *)calloc((libNum + 1), sizeof(FklVMlib));
        FKL_ASSERT(nlibs);
        memcpy(nlibs, exe->libs, sizeof(FklVMlib) * (exe->libNum + 1));
        for (size_t i = exe->libNum; i < libNum; i++) {
            FklVMlib *curVMlib = &nlibs[i + 1];
            FklCodegenLib *curCGlib = codegen->libStack->base[i];
            fklInitVMlibWithCodegenLibRefs(curCGlib, curVMlib, exe,
                                           proc->closure, proc->rcount, 0,
                                           exe->pts);
        }
        FklVMlib *prev = exe->libs;
        exe->libs = nlibs;
        exe->libNum = libNum;
        free(prev);
    }
    if (mainCode) {
        uint32_t o_lcount = fctx->lcount;
        ctx->state = READY;

        update_prototype_lcount(codegen->pts, main_env);

        fklVMacquireSt(exe->gc);
        fklUpdatePrototypeRef(codegen->pts, main_env,
                              codegen->runtime_symbol_table, pst);

        uint32_t proc_idx = codegen->pts->pa[1].lcount;
        set_last_ins_get_loc(mainCode, proc_idx);
        FklInstruction ins = {
            .op = FKL_OP_CALL,
        };
        fklByteCodeLntPushBackIns(mainCode, &ins, 0, 0, 0);
        fklVMreleaseSt(exe->gc);

        FklVMvalue *mainProc =
            fklCreateVMvalueProc(exe, NULL, 0, FKL_VM_NIL, 1);
        fklInitMainProcRefs(exe, mainProc);
        FklVMproc *proc = FKL_VM_PROC(mainProc);
        ctx->mainProc = mainProc;

        FklVMvalue *mainCodeObj = fklCreateVMvalueCodeObj(exe, mainCode);
        fctx->lcount = proc_idx;

        mainCode = FKL_VM_CO(mainCodeObj);
        proc->lcount = fctx->lcount;
        proc->codeObj = mainCodeObj;
        proc->spc = mainCode->bc->code;
        proc->end = proc->spc + mainCode->bc->len;

        FklVMframe *mainframe =
            fklCreateVMframeWithProcValue(exe, ctx->mainProc, exe->top_frame);
        mainframe->c.lr.lrefl = fctx->lrefl;
        mainframe->c.lr.lref = fctx->lref;
        FklVMCompoundFrameVarRef *f = &mainframe->c.lr;
        f->base = 0;
        f->loc = fklAllocMoreSpaceForMainFrame(exe, proc->lcount + 1);
        f->loc[proc_idx] = fctx->ret_proc;
        f->lcount = proc->lcount;
        alloc_more_space_for_var_ref(f, o_lcount, f->lcount);
        init_mainframe_lref(f->lref, fctx->lcount, fctx->lrefl);

        process_unresolve_ref_and_update_const_array_for_repl(
            main_env, codegen->global_env, codegen->pts, exe, exe->locv,
            mainframe, is_need_update_const_array(&const_count, kt));

        exe->top_frame = mainframe;
        if (ctx->interactive) {
            repl_frame->errorCallBack = replErrorCallBack;
            repl_frame->t = &ReplContextMethodTable;
        }
    }

    return 1;
}

static const FklVMframeContextMethodTable EvalContextMethodTable = {
    .atomic = repl_frame_atomic,
    .finalizer = repl_frame_finalizer,
    .copy = NULL,
    .print_backtrace = repl_frame_print_backtrace,
    .step = eval_frame_step,
};

static inline NastCreatCtx *createNastCreatCtx(void) {
    NastCreatCtx *cc = (NastCreatCtx *)malloc(sizeof(NastCreatCtx));
    FKL_ASSERT(cc);
    cc->offset = 0;
    fklAnalysisSymbolVectorInit(&cc->symbolStack, 16);
    fklUintVectorInit(&cc->lineStack, 16);
    fklPtrVectorInit(&cc->stateStack, 16);
    fklNastPushState0ToStack((FklParseStateFuncVector *)&cc->stateStack);
    return cc;
}

static int repl_ret_proc(FKL_CPROC_ARGL) {
    FklVMframe *frame = exe->top_frame->prev;
    FklVMvarRefList *lrefl = frame->c.lr.lrefl;
    FklVMvalue **lref = frame->c.lr.lref;
    frame->c.lr.lrefl = NULL;
    frame->c.lr.lref = NULL;
    frame->c.lr.lcount = 1;
    fklPopVMframe(exe);
    fklPopVMframe(exe);
    FklVMframe *buttom_frame = exe->top_frame;
    ReplCtx *rctx = (ReplCtx *)buttom_frame->data;
    rctx->fctx->lrefl = lrefl;
    rctx->fctx->lref = lref;
    return 1;
}

static inline void init_frame_to_repl_frame(FklVM *exe, FklCodegenInfo *codegen,
                                            FklCodegenEnv *main_env,
                                            const char *eval_expression,
                                            int8_t interactive) {
    FklVMframe *replFrame = (FklVMframe *)calloc(1, sizeof(FklVMframe));
    FKL_ASSERT(replFrame);
    replFrame->prev = NULL;
    replFrame->errorCallBack = replErrorCallBack;
    exe->top_frame = replFrame;
    replFrame->type = FKL_FRAME_OTHEROBJ;
    replFrame->t = &ReplContextMethodTable;
    ReplCtx *ctx = (ReplCtx *)replFrame->data;

    FklVMvalue *mainProc = fklCreateVMvalueProc(exe, NULL, 0, FKL_VM_NIL, 1);
    ctx->replxx = replxx_init();
    ctx->exe = exe;
    ctx->mainProc = mainProc;

    ctx->stdinVal = FKL_VM_VAR_REF(exe->gc->builtin_refs[FKL_VM_STDIN_IDX])->v;
    ctx->codegen = codegen;
    ctx->main_env = main_env;
    NastCreatCtx *cc = createNastCreatCtx();
    ctx->cc = cc;
    ctx->state = READY;
    ctx->fctx = (struct ReplFrameCtx *)calloc(1, sizeof(struct ReplFrameCtx));
    FKL_ASSERT(ctx->fctx);
    ctx->fctx->lrefl = NULL;
    ctx->fctx->ret_proc =
        fklCreateVMvalueCproc(exe, repl_ret_proc, NULL, NULL, 0);
    ctx->interactive = interactive;
    ctx->new_var_count = 0;
    fklInitStringBuffer(&ctx->buf);
    if (eval_expression) {
        replFrame->errorCallBack = NULL;
        replFrame->t = &EvalContextMethodTable;
        fklStringBufferConcatWithCstr(&ctx->buf, eval_expression);
    }
}
