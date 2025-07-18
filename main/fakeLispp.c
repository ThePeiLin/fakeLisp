#include "fakeLisp/grammer.h"
#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/nast.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <argtable3.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

static void
loadLib(FILE *, size_t *pnum, FklCodegenLib **libs, FklSymbolTable *);

static void print_compiler_macros(FklCodegenMacro *head,
        const FklSymbolTable *pst,
        const FklConstTable *pkt,
        FILE *fp,
        uint64_t *opcode_count);

static void print_reader_macros(const FklGraProdGroupHashMap *named_prod_groups,
        const FklSymbolTable *pst,
        const FklConstTable *pkt,
        FILE *fp);

static void print_replacements(const FklReplacementHashMap *replacements,
        const FklSymbolTable *pst,
        FILE *fp);

struct arg_lit *help;
struct arg_lit *stats;
struct arg_file *files;
struct arg_end *end;

static inline void do_gather_statistics(const FklByteCodelnt *bcl,
        uint64_t *array) {
    const FklByteCode *bc = bcl->bc;
    for (uint64_t i = 0; i < bc->len; i++)
        array[bc->code[i].op]++;
}

struct OpcodeStatistics {
    uint64_t count;
    FklOpcode op;
};

static int opstatcmp(const void *a, const void *b) {
    const struct OpcodeStatistics *aa = (const struct OpcodeStatistics *)a;
    const struct OpcodeStatistics *bb = (const struct OpcodeStatistics *)b;
    if (aa->count > bb->count)
        return 1;
    else if (aa->count < bb->count)
        return -1;
    return 0;
}

static inline void print_statistics(const char *filename,
        const uint64_t *count) {
    struct OpcodeStatistics statistics[FKL_OPCODE_NUM];
    printf("\nstatistics of %s:\n", filename);
    for (uint32_t i = 0; i < FKL_OPCODE_NUM; i++) {
        statistics[i].op = i;
        statistics[i].count = count[i];
    }
    qsort(statistics,
            FKL_OPCODE_NUM,
            sizeof(struct OpcodeStatistics),
            opstatcmp);
    for (uint32_t i = 0; i < FKL_OPCODE_NUM; i++)
        printf("%-*s:\t%" PRIu64 "\n",
                (int)FKL_MAX_OPCODE_NAME_LEN,
                fklGetOpcodeName(statistics[i].op),
                statistics[i].count);
    printf("\nmost used opcode is %s, the count is %" PRIu64 "\n",
            fklGetOpcodeName(statistics[FKL_OPCODE_NUM - 1].op),
            statistics[FKL_OPCODE_NUM - 1].count);
}

int main(int argc, char **argv) {
    int exitState = 0;
    const char *progname = argv[0];
    void *argtable[] = {
        help = arg_lit0("h", "help", "display this help and exit"),
        stats = arg_lit0("s", "stat", "display the stats of every opcode"),
        files = arg_filen(NULL,
                NULL,
                "files",
                1,
                argc + 2,
                "the bytecode files you want to print"),
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
        arg_print_errors(stdout, end, progname);
        printf("Try '%s --help' for more informaction.\n", progname);
        exitState = EXIT_FAILURE;
        goto exit;
    }

    for (int i = 0; i < files->count; i++) {
        uint64_t opcode_count[FKL_OPCODE_NUM] = { 0 };

        const char *filename = files->filename[i];
        const char *extension = files->extension[i];

        if (!strcmp(extension, FKL_BYTECODE_FILE_EXTENSION)) {
            FILE *fp = fopen(filename, "rb");
            if (fp == NULL) {
                perror(filename);
                exitState = EXIT_FAILURE;
                goto exit;
            }
            FklSymbolTable runtime_st;
            fklInitSymbolTable(&runtime_st);
            fklLoadSymbolTable(fp, &runtime_st);

            FklConstTable runtime_kt;
            fklInitConstTable(&runtime_kt);
            fklLoadConstTable(fp, &runtime_kt);
            fklDestroyFuncPrototypes(fklLoadFuncPrototypes(fp));
            char *rp = fklRealpath(filename);
            fklZfree(rp);

            FklByteCodelnt *bcl = fklLoadByteCodelnt(fp);
            printf("file: %s\n\n", filename);
            fklPrintByteCodelnt(bcl, stdout, &runtime_st, &runtime_kt);
            if (stats->count > 0)
                do_gather_statistics(bcl, opcode_count);
            fputc('\n', stdout);
            fklDestroyByteCodelnt(bcl);
            FklCodegenLib *libs = NULL;
            size_t num = 0;
            loadLib(fp, &num, &libs, &runtime_st);
            for (size_t i = 0; i < num; i++) {
                FklCodegenLib *cur = &libs[i];
                fputc('\n', stdout);
                printf("lib %" PRIu64 ":\n", i + 1);
                switch (cur->type) {
                case FKL_CODEGEN_LIB_SCRIPT:
                    fklPrintByteCodelnt(cur->bcl,
                            stdout,
                            &runtime_st,
                            &runtime_kt);
                    if (stats->count > 0)
                        do_gather_statistics(cur->bcl, opcode_count);
                    break;
                case FKL_CODEGEN_LIB_DLL:
                    fputs(cur->rp, stdout);
                    break;
                }
                fputc('\n', stdout);
                fklUninitCodegenLib(cur);
            }
            puts("\nruntime symbol table:");
            fklPrintSymbolTable(&runtime_st, stdout);

            puts("\nruntime const table:");
            fklPrintConstTable(&runtime_kt, stdout);

            fklZfree(libs);
            fclose(fp);
            fklUninitSymbolTable(&runtime_st);
            fklUninitConstTable(&runtime_kt);
        } else if (!strcmp(extension, FKL_PRE_COMPILE_FILE_EXTENSION)) {
            char *cwd = fklSysgetcwd();
            fklZfree(cwd);
            FILE *fp = fopen(filename, "rb");
            if (fp == NULL) {
                perror(filename);
                exitState = EXIT_FAILURE;
                return EXIT_FAILURE;
            }
            FklSymbolTable runtime_st;
            fklInitSymbolTable(&runtime_st);

            FklConstTable runtime_kt;
            fklInitConstTable(&runtime_kt);

            FklFuncPrototypes pts;
            fklInitFuncPrototypes(&pts, 0);

            FklFuncPrototypes macro_pts;
            fklInitFuncPrototypes(&macro_pts, 0);

            FklCodegenLibVector scriptLibStack;
            fklCodegenLibVectorInit(&scriptLibStack, 8);

            FklCodegenLibVector macroScriptLibStack;
            fklCodegenLibVectorInit(&macroScriptLibStack, 8);

            FklCodegenOuterCtx ctx;
            fklInitCodegenOuterCtxExceptPattern(&ctx);

            char *rp = fklRealpath(filename);
            char *errorStr = NULL;
            int load_result = fklLoadPreCompile(&pts,
                    &macro_pts,
                    &scriptLibStack,
                    &macroScriptLibStack,
                    &runtime_st,
                    &runtime_kt,
                    &ctx,
                    rp,
                    fp,
                    &errorStr);
            fklZfree(rp);
            fclose(fp);
            if (load_result) {
                if (errorStr) {
                    fprintf(stderr, "%s\n", errorStr);
                    fklZfree(errorStr);
                } else
                    fprintf(stderr, "%s: load failed\n", filename);
                exitState = EXIT_FAILURE;
                goto precompile_exit;
            }

            printf("file: %s\n\n", filename);
            FklSymbolTable *pst = &ctx.public_symbol_table;
            FklConstTable *pkt = &ctx.public_kt;
            uint32_t num = scriptLibStack.size;
            for (size_t i = 0; i < num; i++) {
                const FklCodegenLib *cur = &scriptLibStack.base[i];
                printf("lib %" PRIu64 ":\n", i + 1);
                switch (cur->type) {
                case FKL_CODEGEN_LIB_SCRIPT:
                    fklPrintByteCodelnt(cur->bcl,
                            stdout,
                            &runtime_st,
                            &runtime_kt);
                    if (stats->count > 0)
                        do_gather_statistics(cur->bcl, opcode_count);
                    fputc('\n', stdout);
                    if (cur->head)
                        print_compiler_macros(cur->head,
                                pst,
                                pkt,
                                stdout,
                                opcode_count);
                    if (cur->named_prod_groups.buckets)
                        print_reader_macros(&cur->named_prod_groups,
                                pst,
                                pkt,
                                stdout);
                    if (cur->replacements->buckets)
                        print_replacements(cur->replacements, pst, stdout);
                    if (!cur->head && !cur->named_prod_groups.buckets)
                        fputc('\n', stdout);
                    break;
                case FKL_CODEGEN_LIB_DLL:
                    fputs(cur->rp, stdout);
                    fputs("\n\n", stdout);
                    break;
                }
                fputc('\n', stdout);
            }

            if (macroScriptLibStack.size > 0) {
                fputc('\n', stdout);
                puts("macro loaded libs:\n");
                num = macroScriptLibStack.size;
                for (size_t i = 0; i < num; i++) {
                    const FklCodegenLib *cur = &macroScriptLibStack.base[i];
                    printf("lib %" PRIu64 ":\n", i + 1);
                    switch (cur->type) {
                    case FKL_CODEGEN_LIB_SCRIPT:
                        fklPrintByteCodelnt(cur->bcl, stdout, pst, pkt);
                        if (stats->count > 0)
                            do_gather_statistics(cur->bcl, opcode_count);
                        fputc('\n', stdout);
                        if (cur->head)
                            print_compiler_macros(cur->head,
                                    pst,
                                    pkt,
                                    stdout,
                                    opcode_count);
                        if (cur->named_prod_groups.buckets)
                            print_reader_macros(&cur->named_prod_groups,
                                    pst,
                                    pkt,
                                    stdout);
                        if (!cur->head && !cur->named_prod_groups.buckets)
                            fputc('\n', stdout);
                        break;
                    case FKL_CODEGEN_LIB_DLL:
                        fputs(cur->rp, stdout);
                        fputs("\n\n", stdout);
                        break;
                    }
                    fputc('\n', stdout);
                }
            }
            puts("\nruntime symbol table:");
            fklPrintSymbolTable(&runtime_st, stdout);

            puts("\npublic symbol table:");
            fklPrintSymbolTable(&ctx.public_symbol_table, stdout);

            puts("\npublic const table:");
            fklPrintConstTable(&ctx.public_kt, stdout);

            puts("\nruntime const table:");
            fklPrintConstTable(&runtime_kt, stdout);

        precompile_exit:
            fklUninitFuncPrototypes(&pts);
            fklUninitFuncPrototypes(&macro_pts);

            while (!fklCodegenLibVectorIsEmpty(&scriptLibStack))
                fklUninitCodegenLib(
                        fklCodegenLibVectorPopBack(&scriptLibStack));
            fklCodegenLibVectorUninit(&scriptLibStack);
            while (!fklCodegenLibVectorIsEmpty(&macroScriptLibStack))
                fklUninitCodegenLib(
                        fklCodegenLibVectorPopBack(&macroScriptLibStack));
            fklCodegenLibVectorUninit(&macroScriptLibStack);

            fklUninitSymbolTable(&runtime_st);
            fklUninitConstTable(&runtime_kt);
            fklUninitSymbolTable(&ctx.public_symbol_table);
            fklUninitConstTable(&ctx.public_kt);
            fklUninitGrammer(&ctx.builtin_g);

            if (exitState)
                break;
        } else {
            fprintf(stderr, "%s: Not a correct file!\n", filename);
            break;
        }

        if (stats->count > 0)
            print_statistics(filename, opcode_count);
    }

exit:
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exitState;
}

static FklSid_t *dll_init(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS) {
    return NULL;
}

static void
loadLib(FILE *fp, size_t *pnum, FklCodegenLib **plibs, FklSymbolTable *table) {
    fread(pnum, sizeof(uint64_t), 1, fp);
    size_t num = *pnum;
    FklCodegenLib *libs;
    if (!num)
        libs = NULL;
    else {
        libs = (FklCodegenLib *)fklZmalloc(sizeof(FklCodegenInfo) * num);
        FKL_ASSERT(libs);
    }
    *plibs = libs;
    for (size_t i = 0; i < num; i++) {
        FklCodegenLibType libType = FKL_CODEGEN_LIB_SCRIPT;
        fread(&libType, sizeof(char), 1, fp);
        if (libType == FKL_CODEGEN_LIB_SCRIPT) {
            uint32_t protoId = 0;
            fread(&protoId, sizeof(protoId), 1, fp);
            FklByteCodelnt *bcl = fklCreateByteCodelnt(NULL);
            fklLoadLineNumberTable(fp, &bcl->l, &bcl->ls);
            FklByteCode *bc = fklLoadByteCode(fp);
            bcl->bc = bc;
            fklInitCodegenScriptLib(&libs[i], NULL, bcl, NULL);
            libs[i].prototypeId = protoId;
        } else {
            uv_lib_t lib = { 0 };
            uint64_t len = 0;
            uint64_t typelen = strlen(FKL_DLL_FILE_TYPE);
            fread(&len, sizeof(uint64_t), 1, fp);
            char *rp = (char *)fklZcalloc(len + typelen + 1, sizeof(char));
            FKL_ASSERT(rp);
            fread(rp, len, 1, fp);
            strcat(rp, FKL_DLL_FILE_TYPE);
            fklInitCodegenDllLib(&libs[i], rp, lib, NULL, dll_init, table);
        }
    }
}

static void print_compiler_macros(FklCodegenMacro *head,
        const FklSymbolTable *pst,
        const FklConstTable *pkt,
        FILE *fp,
        uint64_t *opcode_count) {
    fputs("\ncompiler macros:\n", fp);
    for (; head; head = head->next) {
        fputs("pattern:\n", fp);
        fklPrintNastNode(head->origin_exp, fp, pst);
        fputc('\n', fp);
        fklPrintByteCodelnt(head->bcl, fp, pst, pkt);
        if (stats->count > 0)
            do_gather_statistics(head->bcl, opcode_count);
        fputs("\n\n", fp);
    }
}

static void print_reader_macros(const FklGraProdGroupHashMap *ht,
        const FklSymbolTable *pst,
        const FklConstTable *pkt,
        FILE *fp) {
    fputs("\nreader macros:\n", fp);
    for (FklGraProdGroupHashMapNode *l = ht->first; l; l = l->next) {
        fputs("group name:", fp);
        fklPrintRawSymbol(fklGetSymbolWithId(l->k, pst)->k, fp);

        if (l->v.g.ignores) {
            fputs("\nignores:\n", fp);
            fklPrintGrammerIgnores(&l->v.g, &l->v.g.regexes, fp);
        }

        if (l->v.g.productions.first) {
            fputs("\nprods:\n", fp);
            for (const FklProdHashMapNode *cur = l->v.g.productions.first; cur;
                    cur = cur->next) {
                for (const FklGrammerProduction *prod = cur->v; prod;
                        prod = prod->next) {
                    fklPrintGrammerProduction(fp,
                            prod,
                            l->v.g.st,
                            &l->v.g.terminals,
                            &l->v.g.regexes);
                    fputs(" => ", fp);
                    fklPrintReaderMacroAction(fp, prod, l->v.g.st, pkt);
                    fputc('\n', fp);
                }
            }
        }

        fputc('\n', fp);
    }
}

static void print_replacements(const FklReplacementHashMap *replacements,
        const FklSymbolTable *pst,
        FILE *fp) {
    fputs("\nreplacements:\n", fp);
    for (const FklReplacementHashMapNode *cur = replacements->first; cur;
            cur = cur->next) {
        fklPrintRawSymbol(fklGetSymbolWithId(cur->k, pst)->k, fp);
        fputs(" => ", fp);
        fklPrintNastNode(cur->v, fp, pst);
        fputc('\n', fp);
    }
}
