#include <fakeLisp/builtin.h>
#include <fakeLisp/code_lw.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <argtable3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

static inline int pre_compile(const char *main_file_name,
        const char *output_dir,
        int argc,
        char *argv[]) {
    int r = 0;
    FILE *fp = fopen(main_file_name, "r");
    if (fp == NULL) {
        perror(main_file_name);
        return EXIT_FAILURE;
    }

    char *rp = fklRealpath(main_file_name);
    FklCgCtx ctx;

    FklVMgc *gc = fklCreateVMgc(fklCreateVMobarray());
    FklVM *vm = &gc->gcvm;

    fklInitCgCtx(&ctx, fklGetDir(rp), vm);

    const char *main_dir = ctx.main_file_real_path_dir;
    fklChdir(main_dir);

    FklVMvalueCgInfo *codegen = fklCreateVMvalueCgInfo(&ctx,
            NULL,
            rp,
            &(FklCgInfoArgs){
                .is_global = 1,
                .is_lib = 1,
                .is_precompile = 1,
            });

    FklVMvalue *co = fklGenExpressionCodeWithFpForPrecompile(&ctx,
            fp,
            codegen,
            ctx.global_env);

    char *outputname = NULL;
    if (co == NULL) {
        fklZfree(rp);
        r = EXIT_FAILURE;
        goto pre_compile_exit;
    }

    FklVMvalueProto *proto = fklCreateVMvalueProto2(&gc->gcvm, ctx.global_env);
    fklPrintUndefinedRef(ctx.global_env->prev, &gc->err_out);
    FklVMvalue *proc = fklCreateVMvalueProc(&gc->gcvm, co, proto);

    outputname = (char *)fklZmalloc(sizeof(char) * (strlen(rp) + 2));

    FKL_ASSERT(outputname);
    strcpy(outputname, rp);
    strcat(outputname, FKL_PRE_COMPILE_FKL_SUFFIX_STR);
    if (output_dir) {
        if (!fklIsAccessibleDirectory(output_dir))
            fklMkdir(output_dir);
        char *new_output_name =
                fklStrCat(fklZstrdup(output_dir), FKL_PATH_SEPARATOR_STR);
        char *rel_new_output_name = fklRelpath(main_dir, outputname);
        new_output_name = fklStrCat(new_output_name, rel_new_output_name);
        fklZfree(rel_new_output_name);
        fklZfree(outputname);
        outputname = new_output_name;
    }

    fklZfree(rp);

    fklChdir(ctx.cwd);

    FILE *outfp = fopen(outputname, "wb");
    if (!outfp) {
        fprintf(stderr, "%s: Can't create pre-compile file!", outputname);
        r = EXIT_FAILURE;
        goto pre_compile_exit;
    }

    fklClearCgLibMacros2(&ctx);
    fklWritePreCompile(outfp,
            output_dir,
            &(FklWritePreCompileArgs){
                .ctx = &ctx,
                .main_info = codegen,
                .main_proc = FKL_VM_PROC(proc),
            });

    fclose(outfp);

pre_compile_exit:
    fklVMclearExtraMarkFunc(gc);
    fklDestroyVMgc(gc);
    fklUninitCgCtx(&ctx);

    fklZfree(outputname);
    return r;
}

static inline int compile(const char *filename,
        const char *output,
        const char *cwd,
        int argc,
        char *argv[]) {
    int r = 0;
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror(filename);
        return -1;
    }

    char *rp = fklRealpath(filename);
    FklCgCtx ctx;

    FklVMgc *gc = fklCreateVMgc(fklCreateVMobarray());
    FklVM *vm = &gc->gcvm;

    fklInitCgCtx(&ctx, fklGetDir(rp), vm);

    fklChdir(ctx.main_file_real_path_dir);
    FklVMvalueCgInfo *codegen = fklCreateVMvalueCgInfo(&ctx,
            NULL,
            rp,
            &(FklCgInfoArgs){
                .is_lib = 1,
                .is_global = 1,
            });

    char *outputname = NULL;
    FklVM *anotherVM = NULL;
    FklVMvalue *co = fklGenExpressionCodeWithFp(&ctx, //
            fp,
            codegen,
            ctx.global_env);
    fklVMclearExtraMarkFunc(gc);

    if (co == NULL) {
        fklZfree(rp);
        r = EXIT_FAILURE;
        goto compile_exit;
    }

    FklVMvalueProto *proto = fklCreateVMvalueProto2(&gc->gcvm, ctx.global_env);
    fklPrintUndefinedRef(ctx.global_env->prev, &gc->err_out);
    (void)proto;

    if (output) {
        outputname = fklStrCat(fklZstrdup(cwd), FKL_PATH_SEPARATOR_STR);
        outputname = fklStrCat(outputname, output);
        outputname = fklStrCat(outputname, FKL_BYTECODE_FILE_EXTENSION);
    } else {
        outputname = (char *)fklZmalloc(sizeof(char) * (strlen(rp) + 2));
        FKL_ASSERT(outputname);
        strcpy(outputname, rp);
        strcat(outputname, FKL_BYTECODE_FKL_SUFFIX_STR);
    }
    fklZfree(rp);

    fklChdir(ctx.cwd);

    FklVMvalueProto *pt = fklCreateVMvalueProto2(&gc->gcvm, ctx.global_env);
    FklVMvalue *proc = fklCreateVMvalueProc(&gc->gcvm, co, pt);
    anotherVM = fklCreateVM(proc, gc);

    FKL_ASSERT(anotherVM->top_frame->type == FKL_FRAME_COMPOUND);

    FILE *outfp = fopen(outputname, "wb");
    if (!outfp) {
        fprintf(stderr, "%s: Can't create byte code file!\n", outputname);
        r = EXIT_FAILURE;
        goto compile_exit;
    }

    fklVMclearSymbol(gc);
    fklCheckAndGC(anotherVM, 1);
    fklVMrestoreSymbol(gc);

    fklWriteCodeFile(outfp,
            &(FklWriteCodeFileArgs){
                .proc = FKL_VM_PROC(proc),
            });

    fclose(outfp);
compile_exit:
    fklUninitCgCtx(&ctx);

    fklDestroyAllVMs(anotherVM);
    fklDestroyVMgc(gc);

    fklZfree(outputname);
    return r;
}

struct arg_lit *help;

struct arg_file *precompile;

struct arg_file *dir;

struct arg_file *output;

struct arg_file *file;

struct arg_end *end;

int main(int argc, char **argv) {
    argv = uv_setup_args(argc, argv);
    const char *progname = argv[0];

    void *argtable[] = {
        help = arg_lit0("h", "help", "display this help and exit"),
        precompile =
                arg_file0("p", "pre", "<package-name>", "pre-compile package"),
        dir = arg_file0("d",
                "dir",
                "<target-dir>",
                "set pre-compile file target directory"),
        output = arg_file0("o",
                NULL,
                "<file-name>",
                "set bytecode file base name"),
        file = arg_filen(NULL, NULL, NULL, 0, argc + 2, "compile files"),
        end = arg_end(20),
    };

    int exitcode = 0;
    int nerrors = arg_parse(argc, argv, argtable);

    char *cwd = fklSysgetcwd();

    if (help->count > 0) {
        printf("Usage: %s", progname);
        arg_print_syntaxv(stdout, argtable, "\n");
        printf("compile a file into bytecode or pre-compile a package.\n\n");
        arg_print_glossary_gnu(stdout, argtable);
        goto exit;
    }

    if (nerrors) {
    error:
        arg_print_errors(stdout, end, progname);
        printf("Try '%s --help' for more informaction.\n", progname);
        exitcode = EXIT_FAILURE;
        goto exit;
    }

    if (dir->count > 0 && precompile->count < 1)
        goto error;

    if (precompile->count > 0) {
        if (file->count > 0 || output->count > 0)
            goto error;
        const char *package_name = precompile->filename[0];
        FklStrBuf buffer;
        fklInitStrBuf(&buffer);
        fklStrBufConcatWithCstr(&buffer, package_name);
        fklStrBufConcatWithCstr(&buffer, FKL_PATH_SEPARATOR_STR);
        fklStrBufConcatWithCstr(&buffer, FKL_PACKAGE_MAIN_FILE);

        if (!fklIsAccessibleRegFile(buffer.buf)) {
            perror(buffer.buf);
            fklUninitStrBuf(&buffer);
            goto compile_error;
        }

        char *output_dir = dir->count > 0
                                 ? fklStrCat(fklStrCat(fklZstrdup(cwd),
                                                     FKL_PATH_SEPARATOR_STR),
                                           dir->filename[0])
                                 : NULL;
        if (pre_compile(buffer.buf, output_dir, argc, argv)) {
            fklZfree(output_dir);
            fklUninitStrBuf(&buffer);
            goto compile_error;
        }
        fklZfree(output_dir);
        fklUninitStrBuf(&buffer);
        goto exit;
    }

    if (file->count < 1 || (file->count > 1 && output->count > 0))
        goto error;

    for (int i = 0; i < file->count; i++) {
        const char *filename = file->filename[i];
        if (fklIsScriptFile(filename) && fklIsAccessibleRegFile(filename)) {
            if (compile(filename,
                        output->count > 0 ? output->filename[0] : NULL,
                        cwd,
                        argc,
                        argv)) {
            compile_error:
                exitcode = 255;
                goto exit;
            }
        } else {
            FklStrBuf buffer;
            fklInitStrBuf(&buffer);
            fklStrBufConcatWithCstr(&buffer, filename);
            fklStrBufConcatWithCstr(&buffer, FKL_PATH_SEPARATOR_STR);
            fklStrBufConcatWithCstr(&buffer, "main.fkl");

            if (!fklIsAccessibleRegFile(buffer.buf)) {
                perror(buffer.buf);
                fklUninitStrBuf(&buffer);
                goto compile_error;
            }
            if (compile(buffer.buf,
                        output->count > 0 ? output->filename[0] : NULL,
                        cwd,
                        argc,
                        argv)) {
                fklUninitStrBuf(&buffer);
                goto compile_error;
            }
            fklUninitStrBuf(&buffer);
        }
    }

exit:
    fklZfree(cwd);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exitcode;
}
