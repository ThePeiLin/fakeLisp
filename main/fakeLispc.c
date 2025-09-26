#include <fakeLisp/builtin.h>
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
        char *argv[],
        FklCodegenCtx *ctx) {
    FklSymbolTable *pst = &ctx->public_st;
    fklAddSymbolCstr(main_file_name, pst);
    FILE *fp = fopen(main_file_name, "r");
    char *rp = fklRealpath(main_file_name);
    fklSetCodegenCtxMainFileRealPathDir(ctx, fklGetDir(rp));
    const char *main_dir = ctx->main_file_real_path_dir;
    fklChdir(ctx->main_file_real_path_dir);
    FklVMvalueCodegenInfo *codegen = fklCreateVMvalueCodegenInfo(ctx,
            NULL,
            rp,
            &(FklCodegenInfoArgs){
                .is_global = 1,
                .is_lib = 1,
                .st = &ctx->public_st,
                .kt = &ctx->public_kt,
            });

    FklVMvalueCodegenEnv *main_env = ctx->global_env;
    FklByteCodelnt *mainByteCode =
            fklGenExpressionCodeWithFpForPrecompile(fp, codegen, main_env);
    if (mainByteCode == NULL) {
        fklZfree(rp);
        return EXIT_FAILURE;
    }
    fklUpdatePrototype(codegen->pts, main_env, codegen->st, pst);
    fklPrintUndefinedRef(codegen->global_env, codegen->st, pst);

    char *outputname = (char *)fklZmalloc(sizeof(char) * (strlen(rp) + 2));
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
    FILE *outfp = fopen(outputname, "wb");
    if (!outfp) {
        fprintf(stderr, "%s: Can't create pre-compile file!", outputname);
        return EXIT_FAILURE;
    }

    fklWritePreCompile(codegen, main_dir, output_dir, mainByteCode, outfp);
    fklDestroyByteCodelnt(mainByteCode);
    fclose(outfp);
    fklZfree(outputname);
    return 0;
}

static inline int compile(const char *filename,
        const char *output,
        const char *cwd,
        int argc,
        char *argv[],
        FklCodegenCtx *ctx) {
    FklSymbolTable *pst = &ctx->public_st;
    fklAddSymbolCstr(filename, pst);
    FILE *fp = fopen(filename, "r");
    char *rp = fklRealpath(filename);
    fklSetCodegenCtxMainFileRealPathDir(ctx, fklGetDir(rp));
    fklChdir(ctx->main_file_real_path_dir);
    FklVMvalueCodegenInfo *codegen = fklCreateVMvalueCodegenInfo(ctx,
            NULL,
            rp,
            &(FklCodegenInfoArgs){
                .is_lib = 1,
                .is_global = 1,
            });
    FklVMvalueCodegenEnv *main_env = ctx->global_env;
    FklByteCodelnt *mainByteCode =
            fklGenExpressionCodeWithFp(fp, codegen, main_env);
    if (mainByteCode == NULL) {
        fklZfree(rp);
        return EXIT_FAILURE;
    }
    fklUpdatePrototype(codegen->pts, main_env, codegen->st, pst);
    fklPrintUndefinedRef(codegen->global_env, codegen->st, pst);

    char *outputname = NULL;
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
    FILE *outfp = fopen(outputname, "wb");
    if (!outfp) {
        fprintf(stderr, "%s: Can't create byte code file!\n", outputname);
        return EXIT_FAILURE;
    }

    fklWriteCodeFile(outfp,
            &(FklWriteCodeFileArgs){
                .runtime_st = codegen->st,
                .runtime_kt = codegen->kt,
                .pts = codegen->pts,
                .main_func = mainByteCode,
                .libs = codegen->libraries,
            });

    fklDestroyByteCodelnt(mainByteCode);
    fclose(outfp);
    fklZfree(outputname);
    return 0;
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

    FklCodegenCtx ctx;
    FklSymbolTable *st = fklCreateSymbolTable();
    FklConstTable *kt = fklCreateConstTable();
    fklInitCodegenCtx(&ctx, NULL, st, kt);

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
        FklStringBuffer buffer;
        fklInitStringBuffer(&buffer);
        fklStringBufferConcatWithCstr(&buffer, package_name);
        fklStringBufferConcatWithCstr(&buffer, FKL_PATH_SEPARATOR_STR);
        fklStringBufferConcatWithCstr(&buffer, FKL_PACKAGE_MAIN_FILE);

        if (!fklIsAccessibleRegFile(buffer.buf)) {
            perror(buffer.buf);
            fklUninitStringBuffer(&buffer);
            goto compile_error;
        }

        char *output_dir = dir->count > 0
                                 ? fklStrCat(fklStrCat(fklZstrdup(cwd),
                                                     FKL_PATH_SEPARATOR_STR),
                                           dir->filename[0])
                                 : NULL;
        if (pre_compile(buffer.buf, output_dir, argc, argv, &ctx)) {
            fklZfree(output_dir);
            fklUninitStringBuffer(&buffer);
            goto compile_error;
        }
        fklZfree(output_dir);
        fklUninitStringBuffer(&buffer);
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
                        argv,
                        &ctx)) {
            compile_error:
                exitcode = 255;
                goto exit;
            }
        } else {
            FklStringBuffer buffer;
            fklInitStringBuffer(&buffer);
            fklStringBufferConcatWithCstr(&buffer, filename);
            fklStringBufferConcatWithCstr(&buffer, FKL_PATH_SEPARATOR_STR);
            fklStringBufferConcatWithCstr(&buffer, "main.fkl");

            if (!fklIsAccessibleRegFile(buffer.buf)) {
                perror(buffer.buf);
                fklUninitStringBuffer(&buffer);
                goto compile_error;
            }
            if (compile(buffer.buf,
                        output->count > 0 ? output->filename[0] : NULL,
                        cwd,
                        argc,
                        argv,
                        &ctx)) {
                fklUninitStringBuffer(&buffer);
                goto compile_error;
            }
            fklUninitStringBuffer(&buffer);
        }
    }

exit:
    fklZfree(cwd);
    fklUninitCodegenCtx(&ctx);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exitcode;
}
