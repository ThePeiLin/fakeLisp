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
#include <fakeLisp/value_table.h>
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

static void print_compiler_macros_list(FklVM *vm,
        FklVMvalue *macros_list,
        FklCodeBuilder *build,
        uint64_t *opcode_count,
        const FklLibTable *lib_table);

static void print_compiler_macros(FklVM *vm,
        const FklVMvalueCgMacroHashMap *macros,
        FklCodeBuilder *build,
        uint64_t *opcode_count,
        const FklLibTable *lib_table) {
    for (const FklValueHashMapNode *cur = macros->ht.first; cur;
            cur = cur->next) {
        print_compiler_macros_list(vm, cur->v, build, opcode_count, lib_table);
    }
}

static void print_reader_macros(FklVM *vm,
        const FklVMvalueCgRmacroHashMap *reader_macros,
        FklCodeBuilder *build,
        uint64_t *opcode_count,
        const FklLibTable *lib_table);

static void print_replacements(FklVM *vm,
        const FklVMvalueCgRplHashMap *replacements,
        FklCodeBuilder *build);

struct arg_lit *help;
struct arg_lit *stats;
struct arg_file *files;
struct arg_end *end;

static inline void do_gather_statistics(const FklByteCodelnt *bcl,
        uint64_t *array) {
    const FklByteCode *bc = &bcl->bc;
    for (uint64_t i = 0; i < bc->len; i++)
        array[FKL_INS_OP(bc->code[i])]++;
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

static inline void print_lib_name(FklVM *vm,
        const FklVMvalueLib *l,
        uint64_t id,
        FklCodeBuilder *build) {
    CB_FMT("lib ");
    fklPrin1VMvalue2(l->name, build, vm);
    CB_LINE(" (%" PRIu64 "):", id);
}

static inline void
print_export_symbols(FklVM *vm, const FklVMvalueLib *l, FklCodeBuilder *build) {
    CB_LINE("export symbols %" PRIu32 ":", l->count);
    CB_INDENT(flag) {
        int digits_count = fklComputeDigitsCount(l->count);
        FklVMvalue *const *names = fklVMvalueLibNames(l);
        for (size_t i = 0; i < l->count; ++i) {
            CB_LINE_START("%-*zu:\t", digits_count, i + 1);
            fklPrin1VMvalue2(names[i], build, vm);
            CB_LINE_END("");
        }
    }
}

static inline void print_lib_table(FklVM *vm,
        const FklLibTable *lib_table,
        uint64_t *opcode_count,
        FklCodeBuilder *build) {
    for (const FklValueIdHashMapNode *cur = fklLibTableFirst(lib_table); cur;
            cur = cur->next) {
        const FklVMvalueLib *l = fklVMvalueLib(cur->k);
        CB_LINE("");
        uint64_t id = cur->v;
        print_lib_name(vm, l, id, build);
        print_export_symbols(vm, l, build);

        if (l->proc == NULL) {
            CB_LINE("<external>");
            continue;
        }

        if (FKL_IS_PROC(l->proc)) {
            FKL_DIS_PROC(vm,
                    FKL_VM_PROC(l->proc),
                    build,
                    .lib_table = lib_table);
            if (stats->count > 0)
                do_gather_statistics(FKL_VM_CO(FKL_VM_PROC(l->proc)->bcl),
                        opcode_count);
        } else if (FKL_IS_SYM(l->proc)) {
            CB_LINE("%s", FKL_VM_SYM(l->proc)->str);
        } else {
            FKL_UNREACHABLE();
        }

        CB_LINE("");
    }
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

            char *rp = fklRealpath(filename);
            char *dir = fklDupDir(rp);

            FklVMgc *gc = fklCreateVMgc();
            FklVM *vm = &gc->gcvm;

            FklLibTable lib_table = { 0 };
            fklInitLibTable(&lib_table);

            FklVMvalueProc *proc = fklLoadCodeFile(fp, vm, dir, &lib_table);
            FKL_ASSERT(proc != NULL);
            fclose(fp);

            FklCodeBuilder builder = { 0 };
            fklInitCodeBuilderFp(&builder, stdout, NULL);
            FklCodeBuilder *const build = &builder;

            CB_LINE("realpath: %s", rp);
            CB_LINE("file: %s", filename);
            CB_LINE("dir: %s", dir);
            fklZfree(dir);
            fklZfree(rp);

            CB_LINE("");

            CB_LINE("main func:");
            FKL_DIS_PROC(vm, proc, build, .lib_table = &lib_table);
            if (stats->count > 0)
                do_gather_statistics(FKL_VM_CO(proc->bcl), opcode_count);

            CB_LINE("");

            print_lib_table(vm, &lib_table, opcode_count, build);

            CB_LINE("\nobarray:");

            fklPrintObarray(vm, gc->obarray, build);

            CB_LINE("\nkeywords:");
            fklPrintObarray(vm, gc->keywords, build);

            fklUninitLibTable(&lib_table);
            fklDestroyVMgc(gc);
        } else if (!strcmp(extension, FKL_PRE_COMPILE_FILE_EXTENSION)) {
            FILE *fp = fopen(filename, "rb");
            if (fp == NULL) {
                perror(filename);
                exitState = EXIT_FAILURE;
                goto exit;
            }

            FklVMgc *gc = fklCreateVMgc();
            FklVM *vm = &gc->gcvm;

            FklCgCtx ctx = { 0 };
            char *rp = fklRealpath(filename);
            fklInitCgCtx(&ctx, fklTruncDir(fklTruncDir(fklZstrdup(rp))), vm);

            FklLibTable lib_table = { 0 };
            fklInitLibTable(&lib_table);

            FklLoadPreCompileArgs args = {
                .ctx = &ctx,
                .libraries = ctx.libraries,

                .lib_table = &lib_table,
            };

            const FklVMvalueCgLib *cg_lib = fklLoadPreCompile(fp, rp, &args);

            char *dir = fklDupDir(rp);
            fklZfree(rp);

            fklVMunregisterExtraMarkFunc(gc, (FklVMextraMarkArgs *)&ctx);

            fclose(fp);

            if (cg_lib == NULL) {
                if (args.error_fmt) {
                    FklVMvalue *error = FKL_MAKE_VM_ERR(FKL_ERR_IMPORTFAILED,
                            &gc->gcvm,
                            args.error_fmt,
                            args.error_obj);
                    fklPrincVMvalue(FKL_VM_ERR(error)->message, stderr, NULL);
                } else {
                    fprintf(stderr, "%s: load failed\n", filename);
                }
                exitState = EXIT_FAILURE;
                goto precompile_exit;
            }

            FklCodeBuilder builder = { 0 };
            fklInitCodeBuilderFp(&builder, stdout, NULL);
            FklCodeBuilder *const build = &builder;

            const FklVMvalueLib *lib = cg_lib->lib;
            const FklVMvalueProc *proc = FKL_VM_PROC(lib->proc);

            CB_LINE("dir: %s", dir);
            print_lib_name(vm, lib, 0, build);
            print_export_symbols(vm, lib, build);
            FKL_DIS_PROC(vm, proc, build, .lib_table = &lib_table);
            if (stats->count > 0)
                do_gather_statistics(FKL_VM_CO(proc->bcl), opcode_count);
            CB_LINE("");
            if (cg_lib->macros) {
                print_compiler_macros(vm,
                        cg_lib->macros,
                        build,
                        opcode_count,
                        &lib_table);
            }
            if (cg_lib->rmacros)
                print_reader_macros(vm,
                        cg_lib->rmacros,
                        build,
                        opcode_count,
                        &lib_table);
            if (cg_lib->replacements->ht.buckets)
                print_replacements(vm, cg_lib->replacements, build);
            if (!cg_lib->macros && !cg_lib->rmacros)
                CB_LINE("");
            CB_LINE("");

            print_lib_table(vm, &lib_table, opcode_count, build);

            CB_LINE("\nobarray:");
            fklPrintObarray(vm, gc->obarray, build);

            CB_LINE("\nkeywords:");
            fklPrintObarray(vm, gc->keywords, build);

        precompile_exit:
            fklUninitCgCtx(&ctx);
            fklUninitLibTable(&lib_table);
            fklDestroyVMgc(gc);
            fklZfree(dir);
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

static inline void print_compiler_macro(FklVM *vm,
        const FklVMvalueCgMacro *cur,
        FklCodeBuilder *build,
        uint64_t *opcode_count,
        const FklLibTable *lib_table) {
    CB_LINE_START("pattern:\t");
    fklPrin1VMvalue2(cur->pattern, build, vm);
    CB_LINE_END("");

    const FklVMvalueProc *proc = FKL_VM_PROC(cur->proc);
    FKL_DIS_PROC(vm, proc, build, .indents = 1, .lib_table = lib_table);
    if (stats->count > 0)
        do_gather_statistics(FKL_VM_CO(proc->bcl), opcode_count);
    CB_LINE("");
}

static void print_compiler_macros_list(FklVM *vm,
        FklVMvalue *macros_list,
        FklCodeBuilder *build,
        uint64_t *opcode_count,
        const FklLibTable *lib_table) {
    if (macros_list == FKL_VM_NIL)
        return;
    CB_LINE("\ncompiler macros:");
    for (const FklVMvalue *cur_pair = macros_list; FKL_IS_PAIR(cur_pair);
            cur_pair = FKL_VM_CDR(cur_pair)) {
        print_compiler_macro(vm,
                fklVMvalueCgMacro(FKL_VM_CAR(cur_pair)),
                build,
                opcode_count,
                lib_table);
    }
}

static void print_reader_macro_action(FklVM *vm,
        FklVMvalue *act,
        FklCodeBuilder *build,
        const FklLibTable *lib_table) {
    FKL_ASSERT(act != NULL);
    if (fklIsVMvalueCustomActCtx(act)) {
        CB_LINE_END("custom");
        FklVMvalueCustomActCtx *ctx = fklVMvalueCustomActCtx(act);
        const FklVMvalueProc *proc = FKL_VM_PROC(ctx->proc);
        FKL_DIS_PROC(vm, proc, build, .indents = 1, .lib_table = lib_table);
    } else if (fklIsVMvalueSimpleActCtx(act)) {
        fklPrin1VMvalue2(fklVMvalueSimpleActCtx(act)->vec, build, vm);
    } else {
        fklPrin1VMvalue2(act, build, vm);
        CB_LINE_END("");
    }
}

static void print_prod_sym(FklVM *vm,
        const FklCgRmacroGraSym *sym,
        const FklCgRmacroGraSym *end,
        FklCodeBuilder *build) {
    switch (sym->type) {
    case FKL_TERM_NONE:
    case FKL_TERM_EOF:
        FKL_UNREACHABLE();
        break;

    case FKL_TERM_IGNORE:
        CB_FMT("?e");
        break;
    case FKL_TERM_BUILTIN:
    case FKL_TERM_STRING:
    case FKL_TERM_NONTERM:
        fklPrin1VMvalue2(sym->v, build, vm);
        break;

    case FKL_TERM_KEYWORD:
        CB_FMT(":keyword ");
        fklPrin1VMvalue2(sym->v, build, vm);
        break;

    case FKL_TERM_REGEX:
        CB_FMT(":regex ");
        fklPrin1VMvalue2(sym->v, build, vm);
        break;

    case FKL_TERM_COMP: {
        size_t len = (size_t)FKL_GET_FIX(sym->v);
        for (size_t i = 0; i < len; i++) {
            if (i)
                CB_FMT("..");
            FKL_ASSERT(&sym[i + 1] < end);
            print_prod_sym(vm, &sym[i + 1], end, build);
        }
        break;
    }
    }
}

static void print_reader_macro_prod_syms(FklVM *vm,
        const FklVMvalueCgRmacroProd *prod,
        FklCodeBuilder *build,
        int is_ignore) {
    size_t len = prod->len;
    for (size_t i = 0; i < len;) {
        print_prod_sym(vm, &prod->syms[i], &prod->syms[len], build);

        if (prod->syms[i].type == FKL_TERM_COMP) {
            i += 1 + FKL_GET_FIX(prod->syms[i].v);
            if (i < len && prod->syms[i].type == FKL_TERM_IGNORE)
                i++;
            else if (i < len)
                CB_FMT(" .. ");
            continue;
        }

        ++i;
        if (!is_ignore && i < prod->len
                && prod->syms[i].type != FKL_TERM_IGNORE) {
            CB_FMT(" .. ");
        } else {
            ++i;
        }
    }
}

static void print_reader_macros(FklVM *vm,
        const FklVMvalueCgRmacroHashMap *ht,
        FklCodeBuilder *build,
        uint64_t *opcode_count,
        const FklLibTable *lib_table) {
    FKL_ASSERT(ht);

    if (ht->ht.count == 0)
        return;
    CB_LINE("\nreader macros:");
    for (const FklValueHashMapNode *l = ht->ht.first; l; l = l->next) {
        CB_LINE_START("group name: ");
        fklPrin1VMvalue2(l->k, build, vm);
        CB_LINE_END("");

        FklVMvalueCgRmacro *rmacro = fklVMvalueCgRmacro(l->v);

        FklVMvalueCgRmacroProd *prod = NULL;
        for (size_t i = 0; i < rmacro->len; ++i) {
            const FklCgRmacroCmd *cmd = &rmacro->cmds[i];
            switch (cmd->op) {
            case FKL_CG_RMACRO_NONE:
                FKL_UNREACHABLE();

            case FKL_CG_RMACRO_ADD_DELIM:
                CB_LINE_START(":delim ");
                fklPrin1VMvalue2(cmd->args, build, vm);
                CB_LINE_END("");
                break;
            case FKL_CG_RMACRO_ADD_IGNORE: {
                CB_LINE_START(":ignore ");
                prod = fklVMvalueCgRmacroProd(cmd->args);
                print_reader_macro_prod_syms(vm, prod, build, 1);
                CB_LINE_END("");
            } break;

            case FKL_CG_RMACRO_ADD_PROD:
                prod = fklVMvalueCgRmacroProd(cmd->args);
                CB_LINE_START("");
                if (prod->left == FKL_VM_NIL) {
                    CB_FMT(":s-exp");
                } else {
                    fklPrin1VMvalue2(prod->left, build, vm);
                }

                if (prod->add_extra) {
                    CB_FMT(" :s-exp");
                }
                CB_FMT(" -> ");

                print_reader_macro_prod_syms(vm, prod, build, 1);
                CB_FMT(" => ");
                fklPrincVMvalue2(prod->action_type, build, vm);
                CB_FMT(" ");
                print_reader_macro_action(vm, prod->action, build, lib_table);
                break;
            }
        }

        CB_LINE("");
    }
}

static void print_replacements(FklVM *vm,
        const FklVMvalueCgRplHashMap *replacements,
        FklCodeBuilder *build) {
    if (replacements->ht.count == 0)
        return;
    CB_LINE("\nreplacements:");
    for (const FklValueHashMapNode *cur = replacements->ht.first; cur;
            cur = cur->next) {
        CB_LINE_START("");
        fklPrin1VMvalue2(cur->k, build, vm);
        CB_FMT(" => ");
        FklVMvalue *v = fklVMvalueCgRpl(cur->v)->value;
        fklPrin1VMvalue2(v, build, vm);
        CB_LINE_END("");
    }
}
