#include "fakeLisp/bytecode.h"
#include <fakeLisp/base.h>
#include <fakeLisp/code_builder.h>
#include <fakeLisp/dis.h>
#include <fakeLisp/vm.h>

#include <fakeLisp/cb_helper.h>

#include <inttypes.h>
#include <string.h>

static inline int print_single_ins(int digits_count,
        const FklInstruction *ins,
        uint64_t i,
        const FklFuncPrototype *pt,
        FklCodeBuilder *build) {
    FklOpcode op = ins->op;
    FklOpcodeMode mode = fklGetOpcodeMode(op);

    CB_FMT("%-*" PRIu64 ":", digits_count, i);
    CB_LINE_START("%s", fklGetOpcodeName(op));

    CB_FMT(" ");

    FklInstructionArg ins_arg = { 0 };
    int len = fklGetInsOpArg(ins, &ins_arg);

    if (mode == FKL_OP_MODE_I)
        goto end;
    switch (ins->op) {
    case FKL_OP_PUSH_CONST: {
        CB_FMT("%" PRIu64 "\t#\t", ins_arg.ux);
        fklPrin1VMvalue2(pt->konsts[ins_arg.ux], build, NULL);
    } break;

    case FKL_OP_PUSH_CHAR: {
        CB_FMT("%" PRIu64 "\t#\t", ins_arg.ux);
        fklPrin1VMvalue2(FKL_MAKE_VM_CHR(ins_arg.ux), build, NULL);
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

static inline void disassemble_byte_code_lnt(int digits_count,
        const FklByteCodelnt *bcl,
        const FklInstruction *pc,
        const FklInstruction *end,
        uint32_t prototype_id,
        const FklVMvalueProtos *pts,
        uint64_t i,
        FklCodeBuilder *build) {
    const FklFuncPrototype *pt = &pts->p.pa[prototype_id];
    while (pc < end) {
        print_single_ins(digits_count, pc, i, pt, build);
        if (fklIsPushProcIns(pc)) {
            FklInstructionArg ins_arg = { 0 };
            int len = fklGetInsOpArg(pc, &ins_arg);

            ++pc;
            ++i;
            for (int j = 1; j < len; ++j) {
                print_single_ins(digits_count, pc++, i++, pt, build);
            }

            CB_INDENT(flag) {
                disassemble_byte_code_lnt(digits_count,
                        bcl,
                        pc,
                        pc + ins_arg.uy,
                        ins_arg.ux,
                        pts,
                        i,
                        build);
            }

            pc += ins_arg.uy;
            i += ins_arg.uy;
        } else {
            ++pc;
            ++i;
        }
    }
}

static inline int compute_digits_count(uint64_t len) {
    if (len == 0)
        return 1;
    int sum = 0;
    while (len > 0) {
        ++sum;
        len /= 10;
    }

    return sum;
}

void fklDisassembleByteCodelnt(const FklByteCodelnt *bcl,
        uint32_t proto_id,
        const FklVMvalueProtos *protos,
        FklCodeBuilder *build) {
    int digits_count = compute_digits_count(bcl->bc.len);
    uint64_t i = 0;
    CB_INDENT(flag) {
        disassemble_byte_code_lnt(digits_count,
                bcl,
                bcl->bc.code,
                bcl->bc.code + bcl->bc.len,
                proto_id,
                protos,
                i,
                build);
    }
}

void fklDisassembleProc(const FklVMvalue *proc, FklCodeBuilder *build) {
    const FklVMvalueProc *p = FKL_VM_PROC(proc);
    const FklVMvalue *co = p->codeObj;
    const FklByteCodelnt *bcl = FKL_VM_CO(co);

    int digits_count = compute_digits_count(bcl->bc.len);
    uint64_t i = 0;
    CB_INDENT(flag) {
        disassemble_byte_code_lnt(digits_count,
                bcl,
                p->spc,
                p->end,
                p->protoId,
                p->pts,
                i,
                build);
    }
}

void fklPrintObarray(const FklVMobarray *a, FklCodeBuilder *build) {

    int digits_count = compute_digits_count(a->map.count);

    CB_LINE("count:\t%" PRIu32 "", a->map.count);

    size_t i = 0;
    for (const FklStrValueHashMapNode *cur = a->map.first; cur;
            cur = cur->next) {

        CB_LINE_START("%-*zu:\t", digits_count, i + 1);
        fklPrin1VMvalue2(cur->v, build, NULL);
        CB_LINE_END("");

        ++i;
    }
}
