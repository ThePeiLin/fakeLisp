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

static inline void print_nast_node_with_null_chr(const FklNastNode *node,
        const FklSymbolTable *st,
        FILE *fp) {
    fklPrintNastNode(node, fp, st);
    fputc('\0', fp);
}

static inline void write_export_sid_idx_table(const FklCgExportSidIdxHashMap *t,
        FILE *fp) {
    fwrite(&t->count, sizeof(t->count), 1, fp);
    for (FklCgExportSidIdxHashMapNode *sid_idx = t->first; sid_idx;
            sid_idx = sid_idx->next) {
        fwrite(&sid_idx->k, sizeof(sid_idx->k), 1, fp);
        fwrite(&sid_idx->v.idx, sizeof(sid_idx->v.idx), 1, fp);
        fwrite(&sid_idx->v.oidx, sizeof(sid_idx->v.oidx), 1, fp);
    }
}

static inline void write_compiler_macros(const FklCodegenMacro *head,
        const FklSymbolTable *pst,
        FILE *fp) {
    uint64_t count = 0;
    for (const FklCodegenMacro *c = head; c; c = c->next)
        count++;
    fwrite(&count, sizeof(count), 1, fp);
    for (const FklCodegenMacro *c = head; c; c = c->next) {
        print_nast_node_with_null_chr(c->origin_exp, pst, fp);
        fwrite(&c->prototype_id, sizeof(c->prototype_id), 1, fp);
        fklWriteByteCodelnt(c->bcl, fp);
    }
}

static inline void write_replacements(const FklReplacementHashMap *ht,
        const FklSymbolTable *st,
        FILE *fp) {
    if (ht == NULL) {
        uint32_t count = 0;
        fwrite(&count, sizeof(count), 1, fp);
        return;
    }
    fwrite(&ht->count, sizeof(ht->count), 1, fp);
    for (const FklReplacementHashMapNode *rep_list = ht->first; rep_list;
            rep_list = rep_list->next) {
        fwrite(&rep_list->k, sizeof(rep_list->k), 1, fp);
        print_nast_node_with_null_chr(rep_list->v, st, fp);
    }
}

static inline void write_codegen_script_lib_path(const char *rp,
        const char *main_dir,
        FILE *outfp) {
    char *relpath = fklRelpath(main_dir, rp);
    size_t count = 0;
    char **slices = fklSplit(relpath, FKL_PATH_SEPARATOR_STR, &count);

    for (size_t i = 0; i < count; i++) {
        fputs(slices[i], outfp);
        fputc('\0', outfp);
    }
    fputc('\0', outfp);
    fklZfree(relpath);
    fklZfree(slices);
}

static inline void write_codegen_script_lib(const FklCodegenLib *lib,
        const FklSymbolTable *st,
        const char *main_dir,
        FILE *outfp) {
    write_codegen_script_lib_path(lib->rp, main_dir, outfp);
    fwrite(&lib->prototypeId, sizeof(lib->prototypeId), 1, outfp);
    fklWriteByteCodelnt(lib->bcl, outfp);
    fwrite(&lib->spc, sizeof(lib->spc), 1, outfp);
    write_export_sid_idx_table(&lib->exports, outfp);
    write_compiler_macros(lib->head, st, outfp);
    write_replacements(lib->replacements, st, outfp);
    fklWriteNamedProds(&lib->named_prod_groups, st, outfp);
}

static inline void write_codegen_dll_lib_path(const FklCodegenLib *lib,
        const char *main_dir,
        FILE *outfp) {
    char *relpath = fklRelpath(main_dir, lib->rp);
    size_t count = 0;
    char **slices = fklSplit(relpath, FKL_PATH_SEPARATOR_STR, &count);
    count--;

    for (size_t i = 0; i < count; i++) {
        fputs(slices[i], outfp);
        fputc('\0', outfp);
    }
    uint64_t len = strlen(slices[count]) - FKL_DLL_FILE_TYPE_STR_LEN;
    fwrite(slices[count], len, 1, outfp);
    fputc('\0', outfp);
    fputc('\0', outfp);
    fklZfree(relpath);
    fklZfree(slices);
}

static inline void write_codegen_dll_lib(const FklCodegenLib *lib,
        const char *main_dir,
        const char *target_dir,
        FILE *outfp) {
    write_codegen_dll_lib_path(lib, target_dir ? target_dir : main_dir, outfp);
    write_export_sid_idx_table(&lib->exports, outfp);
}

static inline void write_codegen_lib(const FklCodegenLib *lib,
        const FklSymbolTable *st,
        const char *main_dir,
        const char *target_dir,
        FILE *fp) {
    uint8_t type_byte = lib->type;
    fwrite(&type_byte, sizeof(type_byte), 1, fp);
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        write_codegen_script_lib(lib, st, main_dir, fp);
        break;
    case FKL_CODEGEN_LIB_DLL:
        write_codegen_dll_lib(lib, main_dir, target_dir, fp);
        break;
    case FKL_CODEGEN_LIB_UNINIT:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void write_lib_main_file(const FklCodegenInfo *codegen,
        const FklByteCodelnt *bcl,
        const FklSymbolTable *st,
        const char *main_dir,
        FILE *outfp) {
    write_codegen_script_lib_path(codegen->realpath, main_dir, outfp);
    fklWriteByteCodelnt(bcl, outfp);
    fwrite(&codegen->spc, sizeof(codegen->spc), 1, outfp);
    write_export_sid_idx_table(&codegen->exports, outfp);
    write_compiler_macros(codegen->export_macro, st, outfp);
    write_replacements(codegen->export_replacement, st, outfp);
    fklWriteExportNamedProds(codegen->export_named_prod_groups,
            codegen->named_prod_groups,
            st,
            outfp);
}

static inline void write_lib_stack(FklCodegenLibVector *loadedLibStack,
        const FklSymbolTable *st,
        const char *main_dir,
        const char *target_dir,
        FILE *outfp) {
    uint64_t num = loadedLibStack->size;
    fwrite(&num, sizeof(uint64_t), 1, outfp);
    for (size_t i = 0; i < num; i++) {
        const FklCodegenLib *lib = &loadedLibStack->base[i];
        write_codegen_lib(lib, st, main_dir, target_dir, outfp);
    }
}

static inline void write_pre_compile(FklCodegenInfo *codegen,
        const char *main_dir,
        const char *target_dir,
        FklByteCodelnt *bcl,
        FILE *outfp) {
    FklSymbolTable target_st;
    fklInitSymbolTable(&target_st);

    FklConstTable target_kt;
    fklInitConstTable(&target_kt);
    const FklSymbolTable *origin_st = codegen->runtime_symbol_table;
    const FklConstTable *origin_kt = codegen->runtime_kt;

    for (uint32_t i = 0; i < codegen->libStack->size; ++i) {
        fklClearCodegenLibMacros(&codegen->libStack->base[i]);
    }

    for (uint32_t i = 0; i < codegen->macroLibStack->size; ++i) {
        fklClearCodegenLibMacros(&codegen->macroLibStack->base[i]);
    }

    fklRecomputeSidForSingleTableInfo(codegen,
            bcl,
            origin_st,
            &target_st,
            origin_kt,
            &target_kt,
            FKL_CODEGEN_SID_RECOMPUTE_MARK_SYM_AS_RC_SYM);

    fklWriteSymbolTable(&target_st, outfp);
    fklWriteConstTable(&target_kt, outfp);

    fklWriteFuncPrototypes(codegen->pts, outfp);
    write_lib_stack(codegen->libStack, &target_st, main_dir, target_dir, outfp);
    write_lib_main_file(codegen, bcl, &target_st, main_dir, outfp);

    fklWriteFuncPrototypes(codegen->macro_pts, outfp);
    write_lib_stack(codegen->macroLibStack,
            &target_st,
            main_dir,
            target_dir,
            outfp);

    fklUninitSymbolTable(&target_st);
    fklUninitConstTable(&target_kt);
}

static inline int pre_compile(const char *main_file_name,
        const char *output_dir,
        int argc,
        char *argv[],
        FklCodegenOuterCtx *outer_ctx) {
    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    fklAddSymbolCstr(main_file_name, pst);
    FILE *fp = fopen(main_file_name, "r");
    FklCodegenInfo codegen = {
        .fid = 0,
    };
    char *rp = fklRealpath(main_file_name);
    fklSetCodegenOuterCtxMainFileRealPathDir(outer_ctx, fklGetDir(rp));
    const char *main_dir = outer_ctx->main_file_real_path_dir;
    fklChdir(outer_ctx->main_file_real_path_dir);
    FklCodegenEnv *main_env = fklInitGlobalCodegenInfo(&codegen,
            rp,
            &outer_ctx->public_symbol_table,
            &outer_ctx->public_kt,
            0,
            outer_ctx,
            NULL,
            NULL,
            NULL);
    FklByteCodelnt *mainByteCode =
            fklGenExpressionCodeWithFpForPrecompile(fp, &codegen, main_env);
    if (mainByteCode == NULL) {
        fklZfree(rp);
        fklDestroyCodegenEnv(main_env);
        fklUninitCodegenInfo(&codegen);
        return EXIT_FAILURE;
    }
    fklUpdatePrototype(codegen.pts,
            main_env,
            codegen.runtime_symbol_table,
            pst);
    fklDestroyCodegenEnv(main_env);
    fklPrintUndefinedRef(codegen.global_env, codegen.runtime_symbol_table, pst);

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
        fklUninitCodegenInfo(&codegen);
        return EXIT_FAILURE;
    }

    write_pre_compile(&codegen, main_dir, output_dir, mainByteCode, outfp);
    fklDestroyByteCodelnt(mainByteCode);
    fclose(outfp);
    fklUninitCodegenInfo(&codegen);
    fklZfree(outputname);
    return 0;
}

static inline int compile(const char *filename,
        const char *output,
        const char *cwd,
        int argc,
        char *argv[],
        FklCodegenOuterCtx *outer_ctx) {
    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    fklAddSymbolCstr(filename, pst);
    FILE *fp = fopen(filename, "r");
    FklCodegenInfo codegen = {
        .fid = 0,
    };
    char *rp = fklRealpath(filename);
    fklSetCodegenOuterCtxMainFileRealPathDir(outer_ctx, fklGetDir(rp));
    fklChdir(outer_ctx->main_file_real_path_dir);
    FklCodegenEnv *main_env = fklInitGlobalCodegenInfo(&codegen,
            rp,
            fklCreateSymbolTable(),
            fklCreateConstTable(),
            0,
            outer_ctx,
            NULL,
            NULL,
            NULL);
    FklByteCodelnt *mainByteCode =
            fklGenExpressionCodeWithFp(fp, &codegen, main_env);
    if (mainByteCode == NULL) {
        fklZfree(rp);
        fklDestroyCodegenEnv(main_env);
        fklUninitCodegenInfo(&codegen);
        return EXIT_FAILURE;
    }
    fklUpdatePrototype(codegen.pts,
            main_env,
            codegen.runtime_symbol_table,
            pst);
    fklDestroyCodegenEnv(main_env);
    fklPrintUndefinedRef(codegen.global_env, codegen.runtime_symbol_table, pst);

    FklCodegenLibVector *loadedLibStack = codegen.libStack;
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
        fklUninitCodegenInfo(&codegen);
        return EXIT_FAILURE;
    }

    fklWriteSymbolTable(codegen.runtime_symbol_table, outfp);
    fklWriteConstTable(codegen.runtime_kt, outfp);
    fklWriteFuncPrototypes(codegen.pts, outfp);
    fklWriteByteCodelnt(mainByteCode, outfp);
    uint64_t num = loadedLibStack->size;
    fwrite(&num, sizeof(uint64_t), 1, outfp);
    for (size_t i = 0; i < num; i++) {
        const FklCodegenLib *lib = &loadedLibStack->base[i];
        uint8_t type_byte = lib->type;
        fwrite(&type_byte, sizeof(type_byte), 1, outfp);
        if (lib->type == FKL_CODEGEN_LIB_SCRIPT) {
            fwrite(&lib->prototypeId, sizeof(lib->prototypeId), 1, outfp);
            FklByteCodelnt *bcl = lib->bcl;
            fklWriteByteCodelnt(bcl, outfp);
            fwrite(&lib->spc, sizeof(lib->spc), 1, outfp);
        } else {
            const char *rp = lib->rp;
            uint64_t typelen = strlen(FKL_DLL_FILE_TYPE);
            uint64_t len = strlen(rp) - typelen;
            fwrite(&len, sizeof(uint64_t), 1, outfp);
            fwrite(rp, len, 1, outfp);
        }
    }
    fklDestroyByteCodelnt(mainByteCode);
    fclose(outfp);
    fklUninitCodegenInfo(&codegen);
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

    FklCodegenOuterCtx outer_ctx;
    fklInitCodegenOuterCtx(&outer_ctx, NULL);

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
        if (pre_compile(buffer.buf, output_dir, argc, argv, &outer_ctx)) {
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
                        &outer_ctx)) {
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
                        &outer_ctx)) {
                fklUninitStringBuffer(&buffer);
                goto compile_error;
            }
            fklUninitStringBuffer(&buffer);
        }
    }

exit:
    fklZfree(cwd);
    fklUninitCodegenOuterCtx(&outer_ctx);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exitcode;
}
