#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_builder.h>
#include <fakeLisp/code_lw.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/dis.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <fakeLisp/cb_helper.h>

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

static void print_compiler_macros(FklVM *vm,
        const FklMacroHashMap *macros,
        const FklVMvalueProtos *macro_pts,
        FklCodeBuilder *build,
        uint64_t *opcode_count);

static void print_reader_macros(FklVM *vm,
        const FklGraProdGroupHashMap *named_prod_groups,
        const FklVMvalueProtos *macro_pts,
        FklCodeBuilder *build,
        uint64_t *opcode_count);

static void print_replacements(FklVM *vm,
        const FklReplacementHashMap *replacements,
        FklCodeBuilder *build);

struct arg_lit *help;
struct arg_lit *stats;
struct arg_file *files;
struct arg_end *end;

static inline void do_gather_statistics(const FklByteCodelnt *bcl,
        uint64_t *array) {
    const FklByteCode *bc = &bcl->bc;
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
            FklVMgc *gc = fklCreateVMgc(fklCreateVMobarray());
            FklVM *vm = &gc->gcvm;
            FklVMvalueProtos *pts = fklCreateVMvalueProtos(vm, 0);
            FklVMvalueLibs *libs = fklCreateVMvalueLibs(vm);

            FklLoadCodeFileArgs args = {
                .vm = vm,
                .pts = pts,
                .libs = libs,

            };

            int r = fklLoadCodeFile(fp, &args);
            FKL_ASSERT(r == 0);
            fclose(fp);

            FklCodeBuilder builder = { 0 };
            fklInitCodeBuilderFp(&builder, stdout, NULL);
            FklCodeBuilder *const build = &builder;

            CB_LINE("file: %s", filename);
            CB_LINE("");

            CB_LINE("main func:");
            fklDisassembleProc(vm, args.main_func, build);
            if (stats->count > 0)
                do_gather_statistics(
                        FKL_VM_CO(FKL_VM_PROC(args.main_func)->codeObj),
                        opcode_count);

            CB_LINE("");
            size_t count = libs->count;

            for (size_t i = 1; i <= count; ++i) {
                const FklVMlib *cur = &libs->libs[i];
                CB_LINE("");
                CB_LINE("lib %" PRIu64 ":", i);
                if (FKL_IS_PROC(cur->proc)) {
                    CB_LINE("epc %" PRIu64 "", cur->epc);
                    fklDisassembleProc(vm, cur->proc, build);
                    if (stats->count > 0)
                        do_gather_statistics(
                                FKL_VM_CO(FKL_VM_PROC(cur->proc)->codeObj),
                                opcode_count);
                } else {
                    CB_LINE("%s", FKL_VM_STR(cur->proc)->str);
                }

                CB_LINE("");
            }

            CB_LINE("\nobarray:");

            fklPrintObarray(vm, gc->obarray, build);
            fklDestroyVMgc(gc);
        } else if (!strcmp(extension, FKL_PRE_COMPILE_FILE_EXTENSION)) {
            FILE *fp = fopen(filename, "rb");
            if (fp == NULL) {
                perror(filename);
                exitState = EXIT_FAILURE;
                goto exit;
            }

            FklVMgc *gc = fklCreateVMgc(fklCreateVMobarray());
            FklVM *vm = &gc->gcvm;
            FklVMvalueProtos *pts = fklCreateVMvalueProtos(vm, 0);

            FklCgCtx ctx = { 0 };
            char *rp = fklRealpath(filename);
            fklInitCgCtx(&ctx, fklGetDir(rp), pts, vm);

            FklLoadPreCompileArgs args = {
                .ctx = &ctx,
                .libraries = ctx.libraries,
                .macro_libraries = ctx.macro_libraries,
                .pts = pts,
                .macro_pts = ctx.macro_pts,
            };

            int load_result = fklLoadPreCompile(fp, rp, &args);

            fklZfree(rp);

            fklVMclearExtraMarkFunc(gc);

            fclose(fp);

            if (load_result) {
                if (args.error) {
                    fprintf(stderr, "%s\n", args.error);
                    fklZfree(args.error);
                } else
                    fprintf(stderr, "%s: load failed\n", filename);
                exitState = EXIT_FAILURE;
                goto precompile_exit;
            }

            FklCodeBuilder builder = { 0 };
            fklInitCodeBuilderFp(&builder, stdout, NULL);
            FklCodeBuilder *const build = &builder;

            CB_LINE("file: %s", filename);
            CB_LINE("");
            uint64_t count = fklVMvalueCgLibsCount(ctx.libraries);
            const FklCgLib *base = fklVMvalueCgLibs(ctx.libraries);
            for (uint64_t i = 0; i < count; ++i) {
                const FklCgLib *cur = &base[i];
                CB_LINE("lib %" PRIu64 ":", i + 1);
                switch (cur->type) {
                case FKL_CODEGEN_LIB_SCRIPT:
                    CB_LINE("epc %" PRIu64 "", cur->epc);
                    fklDisassembleByteCodelnt(vm,
                            FKL_VM_CO(cur->bcl),
                            cur->prototypeId,
                            ctx.pts,
                            build);
                    if (stats->count > 0)
                        do_gather_statistics(FKL_VM_CO(cur->bcl), opcode_count);
                    CB_LINE("");
                    if (cur->macros)
                        print_compiler_macros(vm,
                                cur->macros,
                                ctx.macro_pts,
                                build,
                                opcode_count);
                    if (cur->named_prod_groups)
                        print_reader_macros(vm,
                                cur->named_prod_groups,
                                ctx.macro_pts,
                                build,
                                opcode_count);
                    if (cur->replacements->buckets)
                        print_replacements(vm, cur->replacements, build);
                    if (!cur->macros && !cur->named_prod_groups)
                        CB_LINE("");
                    break;
                case FKL_CODEGEN_LIB_DLL:
                    CB_LINE("%s", cur->rp);
                    CB_LINE("");
                    break;
                case FKL_CODEGEN_LIB_UNINIT:
                    FKL_UNREACHABLE();
                    break;
                }
                CB_LINE("");
            }

            if (fklVMvalueCgLibsCount(ctx.macro_libraries) > 0) {
                CB_LINE("");
                CB_LINE("macro loaded libs:");
                uint64_t count = fklVMvalueCgLibsCount(ctx.macro_libraries);
                const FklCgLib *base = fklVMvalueCgLibs(ctx.macro_libraries);
                for (uint64_t i = 0; i < count; ++i) {
                    const FklCgLib *cur = &base[i];
                    CB_LINE("lib %" PRIu64 ":", i + 1);
                    switch (cur->type) {
                    case FKL_CODEGEN_LIB_SCRIPT:
                        CB_LINE("epc %" PRIu64 "", cur->epc);
                        fklDisassembleByteCodelnt(vm,
                                FKL_VM_CO(cur->bcl),
                                cur->prototypeId,
                                ctx.macro_pts,
                                build);
                        if (stats->count > 0)
                            do_gather_statistics(FKL_VM_CO(cur->bcl),
                                    opcode_count);
                        CB_LINE("");
                        if (cur->macros)
                            print_compiler_macros(vm,
                                    cur->macros,
                                    ctx.macro_pts,
                                    build,
                                    opcode_count);
                        if (cur->named_prod_groups)
                            print_reader_macros(vm,
                                    cur->named_prod_groups,
                                    ctx.macro_pts,
                                    build,
                                    opcode_count);
                        if (cur->replacements->buckets)
                            print_replacements(vm, cur->replacements, build);
                        if (!cur->macros && !cur->named_prod_groups)
                            CB_LINE("");
                        break;
                    case FKL_CODEGEN_LIB_DLL:
                        CB_LINE("%s", cur->rp);
                        CB_LINE("");
                        break;
                    case FKL_CODEGEN_LIB_UNINIT:
                        FKL_UNREACHABLE();
                        break;
                    }
                    CB_LINE("");
                }
            }

            CB_LINE("\nobarray:");
            fklPrintObarray(vm, gc->obarray, build);

        precompile_exit:
            fklUninitCgCtx(&ctx);
            fklDestroyVMgc(gc);
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

static inline void print_compiler_macro_list(FklVM *vm,
        const FklCgMacro *cur,
        const FklVMvalueProtos *macro_pts,
        FklCodeBuilder *build,
        uint64_t *opcode_count) {
    CB_LINE("pattern:");
    CB_LINE_START("");
    fklPrin1VMvalue2(cur->pattern, build, vm);
    CB_LINE_END("");

    fklDisassembleByteCodelnt(vm,
            FKL_VM_CO(cur->bcl),
            cur->prototype_id,
            macro_pts,
            build);
    if (stats->count > 0)
        do_gather_statistics(FKL_VM_CO(cur->bcl), opcode_count);
    CB_LINE("");
}

static void print_compiler_macros(FklVM *vm,
        const FklMacroHashMap *macros,
        const FklVMvalueProtos *macro_pts,
        FklCodeBuilder *build,
        uint64_t *opcode_count) {
    CB_LINE("\ncompiler macros:");
    for (const FklMacroHashMapNode *cur = macros->first; cur; cur = cur->next) {
        print_compiler_macro_list(vm, cur->v, macro_pts, build, opcode_count);
    }
}

static void print_reader_macro_action(FklVM *vm,
        const FklGrammerProduction *prod,
        const FklVMvalueProtos *macro_pts,
        FklCodeBuilder *build) {
    if (fklIsCustomActionProd(prod)) {
        CB_LINE_END("custom");
        FklVMvalueCustomActCtx *ctx = prod->ctx;
        fklDisassembleByteCodelnt(vm,
                FKL_VM_CO(ctx->bcl),
                ctx->prototype_id,
                macro_pts,
                build);
    } else {
        if (prod->ctx == NULL) {
            CB_FMT("|first|");
        } else {
            fklPrin1VMvalue2(prod->ctx, build, vm);
        }
        CB_LINE_END("");
    }
}

static void print_reader_macros(FklVM *vm,
        const FklGraProdGroupHashMap *ht,
        const FklVMvalueProtos *macro_pts,
        FklCodeBuilder *build,
        uint64_t *opcode_count) {
    CB_LINE("\nreader macros:");
    for (FklGraProdGroupHashMapNode *l = ht->first; l; l = l->next) {
        CB_LINE_START("group name: ");
        fklPrin1VMvalue2(l->k, build, vm);
        CB_LINE_END("");

        if (l->v.g.ignores) {
            CB_LINE("\nignores:");
            fklPrintGrammerIgnores(&l->v.g, &l->v.g.regexes, build);
        }

        if (l->v.g.productions.first) {
            CB_LINE("\nprods:");
            for (const FklProdHashMapNode *cur = l->v.g.productions.first; cur;
                    cur = cur->next) {
                for (const FklGrammerProduction *prod = cur->v; prod;
                        prod = prod->next) {
                    CB_LINE_START("");
                    fklPrintGrammerProduction(vm, prod, &l->v.g.regexes, build);
                    CB_FMT(" => ");
                    print_reader_macro_action(vm, prod, macro_pts, build);
                }
                CB_LINE("");
            }
        }

        CB_LINE("");
    }
}

static void print_replacements(FklVM *vm,
        const FklReplacementHashMap *replacements,
        FklCodeBuilder *build) {
    if (replacements->count == 0)
        return;
    CB_LINE("\nreplacements:");
    for (const FklReplacementHashMapNode *cur = replacements->first; cur;
            cur = cur->next) {
        CB_LINE_START("");
        fklPrin1VMvalue2(cur->k, build, vm);
        CB_FMT(" => ");
        fklPrin1VMvalue2(cur->v, build, vm);
        CB_LINE_END("");
    }
}
