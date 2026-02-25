#include <fakeLisp/base.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_builder.h>
#include <fakeLisp/dis.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
#include <fakeLisp/value_table.h>
#include <fakeLisp/vm.h>

#include <fakeLisp/cb_helper.h>

#include <inttypes.h>
#include <string.h>

static inline int print_single_ins(FklVM *vm,
        int digits_count,
        const FklIns *ins,
        uint64_t i,
        const FklVMvalueProto *pt,
        FklCodeBuilder *build,
        int indents,
        const char *indent_str,
        const FklLibTable *lib_table) {
    FklOpcode op = ins->op;
    FklOpcodeMode mode = fklGetOpcodeMode(op);

    for (int i = 0; i < indents; ++i) {
        CB_FMT("%s", indent_str);
    }

    CB_FMT("%-*" PRIu64 ":", digits_count, i);
    CB_LINE_START("%s", fklGetOpcodeName(op));

    CB_FMT(" ");

    FklInsArg ins_arg = { 0 };
    int len = fklGetInsOpArg(ins, &ins_arg);

    FklVMvalue *const *konsts = fklVMvalueProtoConsts(pt);
    if (mode == FKL_OP_MODE_I)
        goto end;
    switch (ins->op) {
    case FKL_OP_PUSH_CONST: {
        CB_FMT("%" PRIu64 "\t#\t", ins_arg.ux);
        fklPrin1VMvalue2(konsts[ins_arg.ux], build, vm);
    } break;

    case FKL_OP_PUSH_CHAR: {
        CB_FMT("%" PRIu64 "\t#\t", ins_arg.ux);
        fklPrin1VMvalue2(FKL_MAKE_VM_CHR(ins_arg.ux), build, vm);
    } break;

    case FKL_OP_PUSH_PROC:
    case FKL_OP_PUSH_PROC_X:
    case FKL_OP_PUSH_PROC_XX:
    case FKL_OP_PUSH_PROC_XXX:
        CB_FMT("%" PRIu64 "  %" PRIu64, ins_arg.ux, ins_arg.uy);
        break;

    case FKL_OP_DROP:
    case FKL_OP_PAIR:
    case FKL_OP_VEC:
    case FKL_OP_STR:
    case FKL_OP_BVEC:
    case FKL_OP_BOX:
    case FKL_OP_HASH:
        CB_FMT("::%s", fklGetSubOpcodeName(ins->op, ins_arg.ix));
        break;
    case FKL_OP_LOAD_LIB:
    case FKL_OP_LOAD_LIB_C:
    case FKL_OP_LOAD_LIB_X:
    case FKL_OP_LOAD_DLL:
    case FKL_OP_LOAD_DLL_C:
    case FKL_OP_LOAD_DLL_X:
        CB_FMT("%" PRIu64, ins_arg.ux);
        if (lib_table != NULL) {
            FklVMvalueLib *const *libs = fklVMvalueProtoUsedLibs(pt);
            const FklVMvalueLib *l = libs[ins_arg.ux];
            uint64_t id = fklLibTableGet(lib_table, l);
            CB_FMT("\t#\t lib %" PRIu64 "", id);
        }
        break;

    default:
        switch (mode) {
        case FKL_OP_MODE_IsA:
        case FKL_OP_MODE_IsB:
        case FKL_OP_MODE_IsC:
        case FKL_OP_MODE_IsBB:
        case FKL_OP_MODE_IsCCB:
            CB_FMT("%" PRId64, ins_arg.ix);
            break;
        case FKL_OP_MODE_IuB:
        case FKL_OP_MODE_IuC:
        case FKL_OP_MODE_IuBB:
        case FKL_OP_MODE_IuCCB:
            CB_FMT("%" PRIu64, ins_arg.ux);
            break;
        case FKL_OP_MODE_IsAuB:
            CB_FMT("%" PRId64 "\t%" PRIu64, ins_arg.ix, ins_arg.uy);
            break;
        case FKL_OP_MODE_IuAuB:
        case FKL_OP_MODE_IuCuC:
        case FKL_OP_MODE_IuCAuBB:
        case FKL_OP_MODE_IuCAuBCC:
            CB_FMT("%" PRIu64 "\t%" PRIu64, ins_arg.ux, ins_arg.uy);
            break;
        case FKL_OP_MODE_I:
            break;

        case FKL_OP_MODE_IxAxB:
            CB_FMT("%#" PRIx64 "\t%#" PRIx64, ins_arg.ux, ins_arg.uy);
            break;
        }
        break;
    }
end:
    CB_LINE_END("");
    return len;
}

// TODO: 添加打印行号表
static inline void disassemble_byte_code_lnt(FklVM *vm,
        int digits_count,
        const FklByteCodelnt *bcl,
        const FklIns *pc,
        const FklIns *end,
        const FklVMvalueProto *pt,
        uint64_t i,
        FklCodeBuilder *build,
        int indents,
        const char *indent_str,
        const FklLibTable *lib_table) {
    while (pc < end) {
        print_single_ins(vm,
                digits_count,
                pc,
                i,
                pt,
                build,
                indents,
                indent_str,
                lib_table);
        if (fklIsPushProcIns(pc)) {
            FklInsArg ins_arg = { 0 };
            int len = fklGetInsOpArg(pc, &ins_arg);

            ++pc;
            ++i;
            for (int j = 1; j < len; ++j) {
                print_single_ins(vm,
                        digits_count,
                        pc++,
                        i++,
                        pt,
                        build,
                        indents,
                        indent_str,
                        lib_table);
            }

            CB_INDENT(flag) {
                disassemble_byte_code_lnt(vm,
                        digits_count,
                        bcl,
                        pc,
                        pc + ins_arg.uy,
                        fklVMvalueProtoChildren(pt)[ins_arg.ux],
                        i,
                        build,
                        indents,
                        indent_str,
                        lib_table);
            }

            pc += ins_arg.uy;
            i += ins_arg.uy;
        } else {
            ++pc;
            ++i;
        }
    }
}

void fklDisassembleByteCodelnt(FklVM *vm,
        const FklByteCodelnt *bcl,
        const FklVMvalueProto *pt,
        FklCodeBuilder *build,
        const FklDisArgs *args) {
    int digits_count = fklComputeDigitsCount(bcl->bc.len);
    int indents = args ? args->indents : 0;
    const char *indent_str = args && args->indent_str //
                                   ? args->indent_str
                                   : "    ";
    const FklLibTable *lib_table = args ? args->lib_table : NULL;
    uint64_t i = 0;
    CB_INDENT(flag) {
        disassemble_byte_code_lnt(vm,
                digits_count,
                bcl,
                bcl->bc.code,
                bcl->bc.code + bcl->bc.len,
                pt,
                i,
                build,
                indents,
                indent_str,
                lib_table);
    }
}

void fklDisassembleProc(FklVM *vm,
        const FklVMvalueProc *p,
        FklCodeBuilder *build,
        const FklDisArgs *args) {
    const FklVMvalue *co = p->bcl;
    const FklByteCodelnt *bcl = FKL_VM_CO(co);

    int indents = args ? args->indents : 0;
    const char *indent_str = args && args->indent_str //
                                   ? args->indent_str
                                   : "    ";
    const FklLibTable *lib_table = args ? args->lib_table : NULL;
    int digits_count = fklComputeDigitsCount(bcl->bc.len);
    uint64_t i = 0;
    CB_INDENT(flag) {
        disassemble_byte_code_lnt(vm,
                digits_count,
                bcl,
                p->spc,
                p->end,
                p->proto,
                i,
                build,
                indents,
                indent_str,
                lib_table);
    }
}

void fklPrintObarray(FklVM *vm,
        const FklVMvalueObarray *a,
        FklCodeBuilder *build) {

    int digits_count = fklComputeDigitsCount(a->map.count);

    CB_LINE("count:\t%" PRIu32 "", a->map.count);

    size_t i = 0;
    for (const FklStrValueHashMapNode *cur = a->map.first; cur;
            cur = cur->next) {

        CB_LINE_START("%-*zu:\t", digits_count, i + 1);
        fklPrin1VMvalue2(cur->v, build, vm);
        CB_LINE_END("");

        ++i;
    }
}
