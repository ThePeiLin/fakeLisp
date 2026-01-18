#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_builder.h>
#include <fakeLisp/code_lw.h>
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

static int run_repl(const char *eval_expression, int8_t interactive);
static void init_frame_to_repl_frame(FklVM *exe,
        FklCgCtx *cg_ctx,
        FklVMvalueCgInfo *codegen,
        FklVMvalueCgEnv *main_env,
        FklCodeBuilder *build,
        const char *eval_expression,
        int8_t interactive);

static int exit_state = 0;

#define FKL_EXIT_FAILURE (255)

static inline int
compile_and_run(const char *filename, int argc, const char *const *argv) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror(filename);
        return FKL_EXIT_FAILURE;
    }
    FklCgCtx ctx;
    char *rp = fklRealpath(filename);

    FklVMgc *gc = fklCreateVMgc(fklCreateVMobarray());
    FklVM *vm = &gc->gcvm;

    fklInitCgCtx(&ctx, fklGetDir(rp), vm);

    fklChdir(ctx.main_file_real_path_dir);
    FklVMvalueCgInfo *info = fklCreateVMvalueCgInfo(&ctx,
            NULL,
            rp,
            &(FklCgInfoArgs){
                .is_lib = 1,
                .is_global = 1,
            });

    fklZfree(rp);
    FklVMvalue *bc = fklGenExpressionCodeWithFp(&ctx, fp, info, ctx.global_env);
    fklVMclearExtraMarkFunc(gc);

    if (bc == NULL) {
        fklUninitCgCtx(&ctx);
        fklDestroyVMgc(gc);
        return FKL_EXIT_FAILURE;
    }

    FklVMvalueProto *proto = fklCreateVMvalueProto2(&gc->gcvm, ctx.global_env);
    fklPrintUndefinedRef(ctx.global_env->prev, &gc->err_out);

    FklVM *anotherVM = fklCreateVMwithByteCode(bc, gc, proto, 0);

    fklChdir(ctx.cwd);
    fklUninitCgCtx(&ctx);

    fklVMclearSymbol(gc);
    fklVMgcCheck(anotherVM, 1);
    fklVMrestoreSymbol(gc);

    fklInitVMargs(gc, argc, argv);
    int r = fklRunVMidleLoop(anotherVM);
    fklDestroyAllVMs(anotherVM);
    fklDestroyVMgc(gc);
    return r;
}

static inline int
run_bytecode(const char *filename, int argc, const char *const *argv) {
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror(filename);
        return FKL_EXIT_FAILURE;
    }
    FklVMgc *gc = fklCreateVMgc(fklCreateVMobarray());
    FklVM *vm = &gc->gcvm;

    FklVMvalueProc *proc = fklLoadCodeFile(fp, vm, NULL);
    FKL_ASSERT(proc != NULL);
    fclose(fp);
    vm = fklCreateVM(FKL_VM_VAL(proc), gc);

    fklInitVMargs(vm->gc, argc, argv);
    int r = fklRunVMidleLoop(vm);

    fklDestroyAllVMs(vm);
    fklDestroyVMgc(gc);
    return r;
}

static inline int
run_pre_compile(const char *filename, int argc, const char *const *argv) {
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror(filename);
        return FKL_EXIT_FAILURE;
    }
    FklVMgc *gc = fklCreateVMgc(fklCreateVMobarray());
    FklVM *vm = &gc->gcvm;

    FklCgCtx ctx = { 0 };
    char *rp = fklRealpath(filename);
    fklInitCgCtx(&ctx, fklGetDir(rp), vm);

    FklLoadPreCompileArgs args = {
        .ctx = &ctx,
        .libraries = ctx.libraries,
    };

    const FklCgLib *cg_lib = fklLoadPreCompile(fp, rp, &args);

    fklZfree(rp);

    fklVMclearExtraMarkFunc(gc);

    fclose(fp);

    if (cg_lib == NULL) {
        fklUninitCgCtx(&ctx);
        fklDestroyVMgc(gc);
        if (args.error) {
            fprintf(stderr, "%s\n", args.error);
            fklZfree(args.error);
            args.error = NULL;
        }
        fprintf(stderr, "%s: Load pre-compile file failed.\n", filename);
        return 1;
    }

    FklVM *anotherVM = fklCreateVM(cg_lib->lib->proc, gc);

    fklChdir(ctx.cwd);
    fklUninitCgCtx(&ctx);

    fklVMclearSymbol(gc);
    fklVMgcCheck(anotherVM, 1);
    fklVMrestoreSymbol(gc);

    fklInitVMargs(gc, argc, argv);
    int r = fklRunVMidleLoop(anotherVM);
    fklDestroyAllVMs(anotherVM);
    fklDestroyVMgc(gc);
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
        exit_state = EXIT_FAILURE;
        goto exit;
    }

    if (interactive->count > 0 || eval->count > 0) {
        if (rest->count > 0 || priority->count > 0)
            goto error;
        goto interactive;
    } else if (rest->count == 0) {
    interactive:
        exit_state = run_repl(eval->count > 0 ? eval->sval[0] : NULL,
                interactive->count > 0);
    } else {
        const char *filename = rest->sval[0];
        int argc = rest->count;
        const char *const *argv = rest->sval;
        if (module->count > 0)
            goto handle_module;
        if (fklIsAccessibleRegFile(filename)) {
            if (fklIsScriptFile(filename))
                exit_state = compile_and_run(filename, argc, argv);
            else if (fklIsByteCodeFile(filename))
                exit_state = run_bytecode(filename, argc, argv);
            else if (fklIsPrecompileFile(filename))
                exit_state = run_pre_compile(filename, argc, argv);
            else {
                exit_state = FKL_EXIT_FAILURE;
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

            FklStrBuf main_script_buf;
            fklInitStrBuf(&main_script_buf);

            fklStrBufConcatWithCstr(&main_script_buf, filename);
            char *module_script_file =
                    fklStrCat(fklZstrdup(main_script_buf.buf),
                            FKL_SCRIPT_FILE_EXTENSION);
            char *module_bytecode_file =
                    fklStrCat(fklZstrdup(main_script_buf.buf),
                            FKL_BYTECODE_FILE_EXTENSION);

            fklStrBufConcatWithCstr(&main_script_buf, FKL_PATH_SEPARATOR_STR);
            fklStrBufConcatWithCstr(&main_script_buf, "main.fkl");

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
                        exit_state =
                                compile_and_run(module_script_file, argc, argv);
                        goto execute_done;
                    }
                    break;
                case PRIORITY_MODULE_BYTECODE:
                    if (fklIsAccessibleRegFile(module_bytecode_file)) {
                        exit_state =
                                run_bytecode(module_bytecode_file, argc, argv);
                        goto execute_done;
                    }
                    break;
                case PRIORITY_PACKAGE_SCRIPT:
                    if (fklIsAccessibleRegFile(main_script_buf.buf)) {
                        exit_state = compile_and_run(main_script_buf.buf,
                                argc,
                                argv);
                        goto execute_done;
                    }
                    break;
                case PRIORITY_PACKAGE_BYTECODE:
                    if (fklIsAccessibleRegFile(main_code_file)) {
                        exit_state = run_bytecode(main_code_file, argc, argv);
                        goto execute_done;
                    }
                    break;

                case PRIORITY_PACKAGE_PRECOMPILE:
                    if (fklIsAccessibleRegFile(main_pre_file)) {
                        exit_state = run_pre_compile(main_pre_file, argc, argv);
                        goto execute_done;
                    }
                    break;
                }
            }

        execute_err:
            exit_state = FKL_EXIT_FAILURE;
            fprintf(stderr, "%s: No such file or directory\n", filename);

        execute_done:
            fklUninitStrBuf(&main_script_buf);
            fklZfree(main_code_file);
            fklZfree(main_pre_file);
            fklZfree(module_script_file);
            fklZfree(module_bytecode_file);
        }
    }

exit:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exit_state;
}

static int run_repl(const char *eval_expression, int8_t interactive) {
    FklVMgc *gc = fklCreateVMgc(fklCreateVMobarray());
    FklVM *vm = fklCreateVM(NULL, gc);
    FklCgCtx ctx = { 0 };
    fklInitCgCtx(&ctx, NULL, vm);

    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, stderr, NULL);

    FklVMvalueCgInfo *info = fklCreateVMvalueCgInfo(&ctx,
            NULL,
            NULL,
            &(FklCgInfoArgs){
                .is_lib = 1,
                .is_global = 1,
            });

    FklVMvalueCgEnv *main_env = ctx.global_env;

    init_frame_to_repl_frame(vm,
            &ctx,
            info,
            main_env,
            &builder,
            eval_expression,
            interactive);

    int err = fklRunVMidleLoop(vm);

    fklDestroyAllVMs(vm);
    fklDestroyVMgc(gc);
    fklUninitCgCtx(&ctx);
    return err;
}

typedef struct {
    FklCgCtx *cg_ctx;
    FklVMvalueCgInfo *info;
    FklVMvalueCgEnv *main_env;

    int err;
    FklVMvalue *node;
    FklVM *exe;
    enum {
        READY,
        WAITING,
        READING,
    } state : 8;
    int8_t eof;
    int8_t interactive;
    uint32_t new_var_count;

    FklVMvalue *stdinVal;
    FklVMvalue *main_proc;
    FklVMvalueWeakHashEq *weak_var_refs;
    FklStrBuf buf;
    FklVMvalue *lrefl;
    FklVMvalue *lref;
    uint32_t lcount;
    uint32_t bp;
    uint32_t sp;

    size_t offset;
    FklParseStateVector states;
    FklAnalysisSymbolVector symbols;

    FklCodeBuilder *build;
} ReplCtx;

typedef struct {
    ReplCtx *c;
} ReplFrameCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(ReplFrameCtx);

static inline FklVMvalue *
insert_local_ref(FklVM *vm, FklVMframe *f, FklVMvalue *ref, uint32_t cidx) {
    FklVMvalue *rl = fklCreateVMvaluePair(vm, ref, f->lrefl);
    f->lrefl = rl;
    FKL_VM_VEC(f->lref)->base[cidx] = ref;
    return ref;
}

typedef struct {
    const FklSymDefHashMapElm *def;
} ResolveRefArg;

// FklResolveRefArgVector
#define FKL_VECTOR_ELM_TYPE ResolveRefArg
#define FKL_VECTOR_ELM_TYPE_NAME ResolveRefArg
#include <fakeLisp/cont/vector.h>

typedef struct {
    FklVMvalue **loc;
    FklVMframe *f;
    FklResolveRefArgVector *resolvable_refs;
    FklVMvalueWeakHashEq *weak_var_refs;
} ResolveRefArgs;

static inline FklVMvalue *resolve_var_ref_for_repl(FklVM *vm,
        const ResolveRefArgs *s,
        ResolveRefArg *cur) {
    FklVMvalue **loc = s->loc;
    FklVMframe *f = s->f;
    const FklSymDefHashMapElm *def = cur->def;

    FklVMvalueWeakHashEq *weak_var_refs = s->weak_var_refs;

    FklVMvalue **pref = fklVMvalueWeakHashEqGet(weak_var_refs, cur->def->k.id);
    FKL_ASSERT(pref);
    FklVMvalue *ref = FKL_VM_CDR(*pref);

    if (!fklIsClosedVMvalueVarRef(ref))
        return ref;

    FKL_VM_VAR_REF(ref)->idx = def->v.idx;
    FKL_VM_VAR_REF(ref)->ref = &loc[def->v.idx];
    insert_local_ref(vm, f, ref, def->v.idx);
    return ref;
}

static inline void resolve_ref_work_func(FklVM *exe, const ResolveRefArgs *s) {
    const FklResolveRefArgVector *unresolve_refs = s->resolvable_refs;
    ResolveRefArg *cur = unresolve_refs->base;
    const ResolveRefArg *const end = &cur[unresolve_refs->size];

    for (; cur < end; cur++) {
        resolve_var_ref_for_repl(exe, s, cur);
    }
}

static void resolve_ref_cb(FklVM *exe, void *arg) {
    const ResolveRefArgs *aarg = (ResolveRefArgs *)arg;
    resolve_ref_work_func(exe, aarg);
}

static inline void resolve_ref_for_repl(FklVMvalueCgEnv *env,
        FklVMvalueCgEnv *top_env,
        FklVMvalueWeakHashEq *weak_var_refs,
        FklVM *exe,
        FklVMframe *mainframe,
        FklResolveRefArgVector *resolvable_refs) {

    if (resolvable_refs->size) {
        ResolveRefArgs resolve_ref_args = {
            .f = mainframe,
            .weak_var_refs = weak_var_refs,
            .loc = &FKL_VM_GET_ARG(exe, mainframe, 0),
            .resolvable_refs = resolvable_refs,
        };

        fklQueueWorkInIdleThread(exe, resolve_ref_cb, &resolve_ref_args);
    }
}

static inline void
alloc_more_space_for_var_ref(FklVM *vm, FklVMframe *f, uint32_t i, uint32_t n) {
    FklVMvalue *o_lref = f->lref;
    if (!FKL_IS_VECTOR(o_lref)) {
        f->lref = fklCreateVMvalueVec(vm, n);
        return;
    }

    if (i >= n)
        return;

    FklVMvalue *lref = fklCreateVMvalueVec(vm, n);
    for (size_t i = 0; i < FKL_VM_VEC(o_lref)->size; ++i) {
        FKL_VM_VEC(lref)->base[i] = FKL_VM_VEC(o_lref)->base[i];
    }

    f->lref = lref;
}

#include <fakeLisp/grammer.h>

static inline void
repl_ctx_buf_reset(ReplCtx *cc, FklStrBuf *s, FklGrammer *g) {
    cc->offset = 0;
    if (s) {
        fklStrBufClear(s);
        s->buf[0] = '\0';
    }
    cc->symbols.size = 0;
    cc->states.size = 0;
    if (g && g->aTable.num)
        fklParseStateVectorPushBack2(&cc->states,
                (FklParseState){ .state = &g->aTable.states[0] });
    else
        fklVMvaluePushState0ToStack(&cc->states);
}

static inline void
eval_ctx_buf_reset(ReplCtx *cc, FklStrBuf *s, FklGrammer *g) {
    if (s) {
        fklStrBufClear(s);
        s->buf[0] = '\0';
    }
    cc->offset = 0;
    cc->symbols.size = 0;
    cc->states.size = 0;
    if (g && g->aTable.num)
        fklParseStateVectorPushBack2(&cc->states,
                (FklParseState){ .state = &g->aTable.states[0] });
    else
        fklVMvaluePushState0ToStack(&cc->states);
}

#define REPL_PROMPT ">>> "
#define RETVAL_PREFIX ";=> "

#define READ_ERROR_UNEXPETED_EOF (1)
#define READ_ERROR_INVALIDEXPR (2)

typedef struct {
    size_t offset;
    FklVM *vm;
    ReplCtx *cc;
    FklVMvalueCgInfo *info;
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

    FklVMvalueCgInfo *codegen = args->info;
    FklGrammer *g = *(codegen->g);

    FklVM *vm = args->vm;
    ReplCtx *cc = args->cc;

    cc->err = 0;

    FklGrammerMatchCtx ctx = FKL_VMVALUE_PARSE_CTX_INIT(vm, args->info->lnt);

    FklParseError err = 0;
    size_t restLen = str_len - cc->offset;

    FKL_VM_LOCK_BLOCK(vm, flag) {
        if (g && g->aTable.num) {
            cc->node = fklParseWithTableForCharBuf2(g,
                    str + cc->offset,
                    restLen,
                    &restLen,
                    &ctx,
                    &err,
                    &args->errline,
                    &cc->symbols,
                    &cc->states);
        } else {
            cc->node = fklDefaultParseForCharBuf(str + cc->offset,
                    restLen,
                    &restLen,
                    &ctx,
                    &err,
                    &args->errline,
                    &cc->symbols,
                    &cc->states);
        }
    }

    cc->offset = str_len - restLen;
    codegen->curline += ctx.line;

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

static inline int repl_read_expression(FklStrBuf *buf,
        ReadExpressionEndArgs *args) {
    char *next = fklReadline3(REPL_PROMPT,
            buf->buf,
            read_expression_end_cb,
            FKL_TYPE_CAST(void *, args));
    fklStrBufClear(buf);
    int eof = next == NULL;
    if (next) {
        fklStrBufConcatWithCstr(buf, next);
        fklZfree(next);
    }
    return eof;
}

static inline void
init_mainframe_lref(FklVMvalue *lref, uint32_t lcount, FklVMvalue *lrefl) {
    for (; FKL_IS_PAIR(lrefl); lrefl = FKL_VM_CDR(lrefl)) {
        FklVMvalue *ref = FKL_VM_CAR(lrefl);
        FKL_VM_VEC(lref)->base[FKL_VM_VAR_REF(ref)->idx] = ref;
    }
}

static int repl_frame_ret_callback(FklVM *exe, FklVMframe *f) {
    FklVMvalue *lrefl = f->lrefl;
    FklVMvalue *lref = f->lref;
    f->lrefl = FKL_VM_NIL;
    f->lref = FKL_VM_NIL;
    f->lcount = 1;
    uint32_t bp = f->bp;
    uint32_t sp = f->sp;
    fklPopVMframe(exe);

    FklVMframe *buttom_frame = exe->top_frame;
    ReplFrameCtx *rctx = FKL_TYPE_CAST(ReplFrameCtx *, buttom_frame->data);
    rctx->c->lrefl = lrefl;
    rctx->c->lref = lref;
    rctx->c->bp = bp;
    rctx->c->sp = sp;

    exe->bp = bp;

    return 1;
}

static inline ReplCtx *create_repl_ctx_impl(void) {
    ReplCtx *cc = (ReplCtx *)fklZcalloc(1, sizeof(ReplCtx));
    FKL_ASSERT(cc);
    fklAnalysisSymbolVectorInit(&cc->symbols, 16);
    fklParseStateVectorInit(&cc->states, 16);
    fklVMvaluePushState0ToStack(&cc->states);
    return cc;
}
static void collect_resolvable_ref_cb(const FklVarRefDef *ref,
        const FklSymDefHashMapElm *def,
        const FklUnReSymbolRef *uref,
        FklVMvalueProto *proto,
        void *vargs) {
    FklResolveRefArgVector *resolvable_refs =
            FKL_TYPE_CAST(FklResolveRefArgVector *, vargs);

    fklResolveRefArgVectorPushBack(resolvable_refs,
            &(ResolveRefArg){ .def = def });
}

static inline void
execute_repl_compile_result(FklVM *exe, ReplCtx *fctx, FklVMvalue *main_bc) {
    uint32_t o_lcount = fctx->lcount;
    fctx->state = READY;

    FklResolveRefArgVector resolvable_refs;
    fklResolveRefArgVectorInit(&resolvable_refs, fctx->main_env->uref.size);

    FklVMvalueProto *pt = FKL_CREATE_VMVALUE_PROTO(exe,
            fctx->main_env,
            .vm = exe,
            .weak_refs = fctx->weak_var_refs,
            .top_env = fctx->info->global_env,
            .no_refs_to_builtins = 1,
            .resolve_ref_to_def_cb = collect_resolvable_ref_cb,
            .resolve_ref_to_def_cb_args = (void *)&resolvable_refs);

    fctx->main_env->child_proc_protos.size = 0;
    FklVMvalue *main_proc = fklCreateVMvalueProc(exe, main_bc, pt);
    fklInitMainProcRefs(exe, main_proc);
    fctx->main_proc = main_proc;

    fctx->lcount = pt->local_count;
    fctx->new_var_count = fctx->lcount - o_lcount;

    exe->base[1] = fctx->main_proc;
    FklVMframe *f = fklCreateVMframeWithProc(exe, fctx->main_proc);
    fklPushVMframe(f, exe);
    f->lrefl = fctx->lrefl;
    f->lref = fctx->lref;

    f->bp = fctx->bp;
    f->ret_cb = repl_frame_ret_callback;
    fklVMframeSetSp(exe, f, pt->local_count);

    FKL_VM_GET_ARG(exe, f, o_lcount) = NULL;
    alloc_more_space_for_var_ref(exe, f, o_lcount, f->lcount);
    init_mainframe_lref(f->lref, fctx->lcount, fctx->lrefl);

    resolve_ref_for_repl(fctx->main_env,
            fctx->info->global_env,
            fctx->weak_var_refs,
            exe,
            f,
            &resolvable_refs);

    fklResolveRefArgVectorUninit(&resolvable_refs);

    exe->top_frame = f;
    fctx->lrefl = FKL_VM_NIL;
    fctx->lref = FKL_VM_NIL;
}

static int repl_frame_step(void *data, FklVM *exe) {
    ReplFrameCtx *ctx = FKL_TYPE_CAST(ReplFrameCtx *, data);
    ReplCtx *c = ctx->c;

    switch (c->state) {
    default:
        FKL_UNREACHABLE();
    case READY:
        c->state = WAITING;
        return 1;
        break;
    case WAITING: {
        c->state = READING;
        if (exe->tp - c->sp != 0) {
            fklCodeBuilderPuts(c->build, RETVAL_PREFIX);
            fklDBG_printVMstack(exe, exe->tp - c->sp, c->build, 0, exe);
        }
        exe->tp = c->sp;

        FKL_VM_UNLOCK_BLOCK(exe, flag) {
            c->node = NULL;
            c->err = 0;
            ReadExpressionEndArgs args = {
                .vm = exe,
                .cc = c,
                .info = c->info,
            };

            c->eof = repl_read_expression(&c->buf, &args);
        }

        return 1;
    } break;
    case READING:
        c->state = WAITING;
        break;
    }

    FklVMvalueCgInfo *codegen = c->info;
    FklVMvalueCgEnv *main_env = c->main_env;
    FklStrBuf *s = &c->buf;
    int is_eof = c->eof;

    FklGrammer *g = *(codegen->g);

    FklVMvalue *ast = c->node;
    if (ast == NULL && c->err) {
        repl_ctx_buf_reset(c, s, g);
        if (c->err == READ_ERROR_UNEXPETED_EOF)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNEXPECTED_EOF, exe);
        else if (c->err == READ_ERROR_INVALIDEXPR)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR, exe);
    } else if (ast) {
        FklVMvalue *main_bc = fklGenExpressionCode(c->cg_ctx, //
                ast,
                main_env,
                c->info);

        fklStoreHistoryInStrBuf(s, c->offset);

        g = *(codegen->g);
        repl_ctx_buf_reset(c, NULL, g);

        if (main_bc) {
            execute_repl_compile_result(exe, c, main_bc);
            return 1;
        } else {
            fklClearCgPreDef(main_env);
            c->state = WAITING;
            return 1;
        }
    } else if (ast == NULL && is_eof) {
        return 0;
    } else
        fklStrBufPutc(&c->buf, '\n');
    return 1;
}

static void
repl_frame_print_backtrace(void *data, FklCodeBuilder *fp, FklVMgc *gc) {}

static void repl_frame_atomic(void *data, FklVMgc *gc) {
    ReplFrameCtx *ctx = FKL_TYPE_CAST(ReplFrameCtx *, data);
    ReplCtx *fctx = ctx->c;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, fctx->weak_var_refs), gc);
    fklVMgcToGray(fctx->stdinVal, gc);
    fklVMgcToGray(fctx->main_proc, gc);
    fklVMgcToGray(fctx->lrefl, gc);
    fklVMgcToGray(fctx->lref, gc);
}

static void repl_frame_finalizer(void *data) {
    ReplFrameCtx *ctx = (ReplFrameCtx *)data;
    ReplCtx *cc = ctx->c;
    fklUninitStrBuf(&cc->buf);

    fklParseStateVectorUninit(&cc->states);
    fklAnalysisSymbolVectorUninit(&cc->symbols);

    cc->lref = FKL_VM_NIL;
    cc->lrefl = FKL_VM_NIL;

    fklZfree(cc);
}

static const FklVMframeContextMethodTable ReplContextMethodTable = {
    .atomic = repl_frame_atomic,
    .finalizer = repl_frame_finalizer,
    .print_backtrace = repl_frame_print_backtrace,
    .step = repl_frame_step,
};

static int replErrorCallBack(FklVMframe *f, FklVMvalue *errValue, FklVM *exe) {
    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderFp(&builder, stderr, NULL);
    fklPrintErrBacktrace(errValue, exe, &builder);
    FklVMframe *main_frame = exe->top_frame;
    if (main_frame->prev) {
        for (; main_frame->prev->prev; main_frame = main_frame->prev)
            ;
        FklVMvalue *lrefl = main_frame->lrefl;
        FklVMvalue *lref = main_frame->lref;

        uint32_t lcount = main_frame->lcount;
        main_frame->lrefl = FKL_VM_NIL;
        main_frame->lref = FKL_VM_NIL;
        main_frame->lcount = 1;
        uint32_t bp = main_frame->bp;
        uint32_t sp = main_frame->sp;

        while (exe->top_frame->prev) {
            FklVMframe *cur = exe->top_frame;
            exe->top_frame = cur->prev;
            fklDestroyVMframe(cur, exe);
        }

        ReplCtx *c = FKL_TYPE_CAST(ReplFrameCtx *, exe->top_frame->data)->c;
        FklVMvalue **cur = &FKL_VM_GET_ARG(exe, //
                main_frame,
                lcount - c->new_var_count);

        for (uint32_t i = 0; i < c->new_var_count; i++)
            if (!cur[i])
                cur[i] = FKL_VM_NIL;

        c->lrefl = lrefl;
        c->lref = lref;
        c->bp = bp;
        c->sp = sp;

        exe->tp = sp;
        exe->bp = bp;

        c->state = READY;
    }
    return 1;
}

static int eval_frame_step(void *data, FklVM *exe) {
    FklVMframe *repl_frame = exe->top_frame;
    ReplFrameCtx *ctx = FKL_TYPE_CAST(ReplFrameCtx *, data);
    ReplCtx *c = ctx->c;
    if (fklStrBufLen(&c->buf) == 0)
        return 0;

    FklVMvalueCgInfo *info = c->info;
    FklVMvalueCgEnv *main_env = c->main_env;
    FklVMvalue *ast = NULL;
    FklGrammerMatchCtx gctx = FKL_VMVALUE_PARSE_CTX_INIT(exe, info->lnt);
    gctx.line = info->curline;

    const char *eval_expression_str = fklStrBufBody(&ctx->c->buf);
    size_t restLen = fklStrBufLen(&ctx->c->buf);
    FklParseError err = 0;
    size_t errLine = 0;
    FklGrammer *g = *(info->g);

    FklVMvalue *begin_s = fklVMaddSymbolCstr(exe, "begin");
    FklVMvalue *begin_exp = fklCreateVMvaluePair1(exe, begin_s);

    FklVMvalue **pnext = &FKL_VM_CDR(begin_exp);

    while (restLen) {
        ast = fklDefaultParseForCharBuf(eval_expression_str,
                restLen,
                &restLen,
                &gctx,
                &err,
                &errLine,
                &c->symbols,
                &c->states);

        c->offset = strlen(eval_expression_str) - restLen;
        info->curline = gctx.line;
        if (err == FKL_PARSE_WAITING_FOR_MORE
                || (err == FKL_PARSE_TERMINAL_MATCH_FAILED && !restLen)
                || (err == FKL_PARSE_TERMINAL_MATCH_FAILED && restLen)
                || (err == FKL_PARSE_REDUCE_FAILED)) {
            eval_ctx_buf_reset(c, &c->buf, g);
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDEXPR, exe);
        } else if (ast) {
            eval_expression_str = eval_expression_str + c->offset;
            eval_ctx_buf_reset(c, &c->buf, g);
            FklVMvalue *next_pair = fklCreateVMvaluePair1(exe, ast);
            *pnext = next_pair;
            pnext = &FKL_VM_CDR(next_pair);
        }
    }

    FklVMvalue *main_bc = fklGenExpressionCode(c->cg_ctx, //
            begin_exp,
            main_env,
            c->info);

    g = *(info->g);
    repl_ctx_buf_reset(c, &c->buf, g);

    if (main_bc) {
        execute_repl_compile_result(exe, c, main_bc);
        if (c->interactive) {
            repl_frame->errorCallBack = replErrorCallBack;
            repl_frame->t = &ReplContextMethodTable;
        }
    } else {
        fklClearCgPreDef(main_env);
        c->state = WAITING;
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

static inline void init_frame_to_repl_frame(FklVM *exe,
        FklCgCtx *cg_ctx,
        FklVMvalueCgInfo *codegen,
        FklVMvalueCgEnv *main_env,
        FklCodeBuilder *build,
        const char *eval_expression,
        int8_t interactive) {
    FklVMgc *gc = exe->gc;
    FklVMframe *frame = fklCreateNewOtherObjVMframe(&ReplContextMethodTable);
    frame->errorCallBack = replErrorCallBack;
    exe->top_frame = frame;
    ReplFrameCtx *f_ctx = FKL_TYPE_CAST(ReplFrameCtx *, frame->data);

    ReplCtx *c = create_repl_ctx_impl();

    f_ctx->c = c;

    c->build = build;
    c->cg_ctx = cg_ctx;
    c->exe = exe;
    c->main_proc = FKL_VM_NIL;

    c->weak_var_refs = fklCreateVMvalueWeakHashEq(exe);
    c->stdinVal = FKL_VM_VAR_REF(gc->builtin_refs[FKL_VM_STDIN_IDX])->v;
    c->info = codegen;
    c->main_env = main_env;
    c->state = READY;
    c->lrefl = FKL_VM_NIL;
    c->lref = FKL_VM_NIL;
    c->interactive = interactive;
    c->new_var_count = 0;

    fklInitStrBuf(&f_ctx->c->buf);
    if (eval_expression) {
        frame->errorCallBack = NULL;
        frame->t = &EvalContextMethodTable;
        fklStrBufConcatWithCstr(&f_ctx->c->buf, eval_expression);
    }
    fklSetBp(exe);
    f_ctx->c->bp = exe->bp;
    f_ctx->c->sp = exe->bp + 1;
    fklVMstackReserve(exe, f_ctx->c->sp + 1);
    if (f_ctx->c->sp > exe->tp) {
        memset(&exe->base[exe->tp],
                0,
                (f_ctx->c->sp - exe->tp) * sizeof(FklVMvalue *));
        exe->tp = f_ctx->c->sp;
    }
}
