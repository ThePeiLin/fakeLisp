#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_lw.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/regex.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include "codegen.h"

static FklByteCodelnt *gen_push_literal_code(const FklPmatchRes *node,
        FklVMvalueCodegenInfo *info,
        FklVMvalueCodegenEnv *env,
        uint32_t scope);

static inline FklVMvalue *cdr(const FklVMvalue *node) {
    return FKL_VM_CDR(node);
    // return node->pair->cdr->pair->car;
}

static inline FklVMvalue *car(const FklVMvalue *node) {
    return FKL_VM_CAR(node);
    // return node->pair->cdr->pair->car;
}

static inline FklVMvalue *cadr(const FklVMvalue *node) {
    return car(cdr(node));
    // return node->pair->cdr->pair->car;
}

static inline FklVMvalue *cddr(const FklVMvalue *node) {
    return cdr(cdr(node));
    // return node->pair->cdr->pair->car;
}

static inline FklVMvalue *caddr(const FklVMvalue *node) {
    return car(cdr(cdr(node)));
    // return node->pair->cdr->pair->cdr->pair->car;
}

static inline FklVMvalue *caadr(const FklVMvalue *node) {
    return car(car(cdr(node)));
    // return node->pair->cdr->pair->car->pair->car;
}

static inline int isExportDefmacroExp(const FklVMvalue *c,
        FklVMvalue *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFMACRO],
                   c,
                   NULL)
        && !FKL_IS_BOX(cadr(c));
}

static inline int isExportDefReaderMacroExp(const FklVMvalue *c,
        FklVMvalue *const *builtin_pattern_node) {
    return (fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFMACRO],
                    c,
                    NULL)
                   && FKL_IS_BOX(cadr(c)))
        || fklPatternMatch(
                builtin_pattern_node[FKL_CODEGEN_PATTERN_DEF_READER_MACROS],
                c,
                NULL);
}

static inline int isExportDefineExp(const FklVMvalue *c,
        FklVMvalue *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFINE],
                   c,
                   NULL)
        || fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFCONST],
                c,
                NULL);
}

static inline int isBeginExp(const FklVMvalue *c,
        FklVMvalue *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_BEGIN],
            c,
            NULL);
}

static inline int isExportDefunExp(const FklVMvalue *c,
        FklVMvalue *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFUN],
                   c,
                   NULL)
        || fklPatternMatch(
                builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFUN_CONST],
                c,
                NULL);
}

static inline int isExportImportExp(FklVMvalue *c,
        FklVMvalue *const *builtin_pattern_node) {
    return fklPatternMatch(
                   builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_PREFIX],
                   c,
                   NULL)
        || fklPatternMatch(
                builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_ONLY],
                c,
                NULL)
        || fklPatternMatch(
                builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_ALIAS],
                c,
                NULL)
        || fklPatternMatch(
                builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_EXCEPT],
                c,
                NULL)
        || fklPatternMatch(
                builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_COMMON],
                c,
                NULL)
        || fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT],
                c,
                NULL)
        || fklPatternMatch(
                builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_NONE],
                c,
                NULL);
}

static const FklCodegenActionContextMethodTable
        DefaultStackContextMethodTable = { .size = 0 };

static FklCodegenActionContext *createCodegenActionContext(
        const FklCodegenActionContextMethodTable *t) {
    FklCodegenActionContext *r = (FklCodegenActionContext *)fklZcalloc(1,
            sizeof(FklCodegenActionContext) + t->size);
    FKL_ASSERT(r);
    r->t = t;
    return r;
}

static FklCodegenActionContext *createDefaultStackContext(void) {
    return createCodegenActionContext(&DefaultStackContextMethodTable);
}

#define DO_NOT_NEED_RETVAL (0)
#define ALL_MUST_HAS_RETVAL (1)
#define FIRST_MUST_HAS_RETVAL (2)

// CgExpQueue
#define FKL_QUEUE_TYPE_PREFIX Cg
#define FKL_QUEUE_METHOD_PREFIX cg
#define FKL_QUEUE_ELM_TYPE FklPmatchRes
#define FKL_QUEUE_ELM_TYPE_NAME Exp
#include <fakeLisp/cont/queue.h>

static FklCodegenNextExpression *createCodegenNextExpression(
        const FklNextExpressionMethodTable *t,
        void *context,
        uint8_t must_has_retval) {
    FklCodegenNextExpression *r = (FklCodegenNextExpression *)fklZmalloc(
            sizeof(FklCodegenNextExpression));
    FKL_ASSERT(r);
    r->t = t;
    r->context = context;
    r->must_has_retval = must_has_retval;
    return r;
}

static int _default_codegen_get_next_expression(void *context,
        FklPmatchRes *out,
        FklCodegenErrorState *error_state) {
    FklPmatchRes *head = cgExpQueuePop(FKL_TYPE_CAST(CgExpQueue *, context));
    if (head == NULL)
        return 0;
    *out = *head;
    return 1;
}

static void _default_codegen_next_expression_finalizer(void *context) {
    CgExpQueue *q = FKL_TYPE_CAST(CgExpQueue *, context);
    // while (!fklNastNodeQueueIsEmpty(q))
    //     fklDestroyNastNode(*fklNastNodeQueuePop(q));
    cgExpQueueDestroy(q);
}

static void _default_codegen_next_expression_atomic(FklVMgc *gc, void *ctx) {
    CgExpQueue *q = FKL_TYPE_CAST(CgExpQueue *, ctx);
    for (const CgExpQueueNode *c = q->head; c; c = c->next) {
        fklVMgcToGray(c->data.value, gc);
        fklVMgcToGray(c->data.container, gc);
    }
}

static const FklNextExpressionMethodTable
        _default_codegen_next_expression_method_table = {
            .get_next_exp = _default_codegen_get_next_expression,
            .finalizer = _default_codegen_next_expression_finalizer,
            .atomic = _default_codegen_next_expression_atomic,
        };

static FklCodegenNextExpression *createDefaultQueueNextExpression(
        CgExpQueue *queue) {
    return createCodegenNextExpression(
            &_default_codegen_next_expression_method_table,
            queue,
            DO_NOT_NEED_RETVAL);
}

static FklCodegenNextExpression *createMustHasRetvalQueueNextExpression(
        CgExpQueue *queue) {
    return createCodegenNextExpression(
            &_default_codegen_next_expression_method_table,
            queue,
            ALL_MUST_HAS_RETVAL);
}

static FklCodegenNextExpression *createFirstHasRetvalQueueNextExpression(
        CgExpQueue *queue) {
    return createCodegenNextExpression(
            &_default_codegen_next_expression_method_table,
            queue,
            FIRST_MUST_HAS_RETVAL);
}

static const char *builtInSubPattern[FKL_CODEGEN_SUB_PATTERN_NUM + 1] = {
    "~(unquote ~value)",
    "~(unqtesp ~value)",
    "~(define ~value)",
    "~(defmacro ~value)",
    "~(import ~value)",
    "~(and,~rest)",
    "~(or,~rest)",
    "~(not ~value)",
    "~(eq ~arg0 ~arg1)",
    "~(match ~arg0 ~arg1)",
    NULL,
};

typedef enum {
    INS_APPEND_FRONT,
    INS_APPEND_BACK,
} InsAppendMode;

typedef enum {
    JMP_UNCOND,
    JMP_IF_TRUE,
    JMP_IF_FALSE,
} JmpInsCondition;

typedef enum {
    JMP_FORWARD,
    JMP_BACKWARD,
} JmpInsOrientation;

static inline FklByteCodelnt *create_0len_bcl(void) {
    return fklCreateByteCodelnt(0);
}

#define I24_L8_MASK (0xFF)
#define I32_L16_MASK (0xFFFF)
#define I64_L24_MASK (0xFFFFFF)

static inline void set_ins_uc(FklInstruction *ins, uint32_t k) {
    ins->au = k & I24_L8_MASK;
    ins->bu = k >> FKL_BYTE_WIDTH;
}

static inline void set_ins_uxx(FklInstruction *ins, uint64_t k) {
    set_ins_uc(&ins[0], k & I64_L24_MASK);
    ins[1].op = FKL_OP_EXTRA_ARG;
    set_ins_uc(&ins[1], (k >> FKL_I24_WIDTH) & I64_L24_MASK);
    ins[2].op = FKL_OP_EXTRA_ARG;
    ins[2].bu = (k >> (FKL_I24_WIDTH * 2));
}

static inline void set_ins_ux(FklInstruction *ins, uint32_t k) {
    ins[0].bu = k & I32_L16_MASK;
    ins[1].op = FKL_OP_EXTRA_ARG;
    ins[1].bu = k >> FKL_I16_WIDTH;
}

static inline int
set_ins_with_unsigned_imm(FklInstruction *ins, FklOpcode op, uint64_t k) {
    int l = 1;
    if (k <= UINT16_MAX) {
        ins[0].op = op;
        ins[0].bu = k;
    } else if (k <= FKL_U24_MAX) {
        ins[0].op = op + 1;
        set_ins_uc(&ins[0], k);
    } else if (k <= UINT32_MAX) {
        ins[0].op = op + 2;
        set_ins_ux(ins, k);
        l = 2;
    } else {
        ins[0].op = op + 3;
        set_ins_uxx(ins, k);
        l = 3;
    }
    return l;
}

static inline FklByteCodelnt *set_and_append_ins_with_unsigned_imm(
        InsAppendMode m,
        FklByteCodelnt *bcl,
        FklOpcode op,
        uint64_t k,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklInstruction ins[3] = { FKL_INSTRUCTION_STATIC_INIT };
    int l = set_ins_with_unsigned_imm(ins, op, k);
    if (bcl == NULL)
        bcl = create_0len_bcl();
    switch (m) {
    case INS_APPEND_BACK:
        for (int i = 0; i < l; i++)
            fklByteCodeLntPushBackIns(bcl, &ins[i], fid, line, scope);
        break;
    case INS_APPEND_FRONT:
        for (; l > 0; l--)
            fklByteCodeLntInsertFrontIns(&ins[l - 1], bcl, fid, line, scope);
    }
    return bcl;
}

static inline int set_ins_with_2_unsigned_imm(FklInstruction *ins,
        FklOpcode op,
        uint64_t ux,
        uint64_t uy) {
    int l = 1;
    if (ux <= UINT8_MAX && uy <= UINT16_MAX) {
        ins[0].op = op;
        ins[0].au = ux;
        ins[0].bu = uy;
    } else if (ux <= FKL_U24_MAX && uy <= FKL_U24_MAX) {
        ins[0].op = op + 1;
        ins[1].op = FKL_OP_EXTRA_ARG;
        set_ins_uc(&ins[0], ux);
        set_ins_uc(&ins[1], uy);
        l = 2;
    } else if (ux <= UINT32_MAX && uy <= UINT32_MAX) {
        ins[0].op = op + 2;
        set_ins_uc(&ins[0], ux & I64_L24_MASK);
        ins[1].op = FKL_OP_EXTRA_ARG;
        ins[1].au = ux >> FKL_I24_WIDTH;
        ins[1].bu = uy & I32_L16_MASK;
        ins[2].op = FKL_OP_EXTRA_ARG;
        ins[2].bu = uy >> FKL_I16_WIDTH;
        l = 3;
    } else {
        ins[0].op = op + 3;
        set_ins_uc(&ins[0], ux & I64_L24_MASK);
        ins[1].op = FKL_OP_EXTRA_ARG;
        ins[1].au = ux >> FKL_I24_WIDTH;
        ins[1].bu = uy & I32_L16_MASK;

        ins[2].op = FKL_OP_EXTRA_ARG;
        set_ins_uc(&ins[2], (uy >> FKL_I16_WIDTH) & I64_L24_MASK);

        ins[3].op = FKL_OP_EXTRA_ARG;
        set_ins_uc(&ins[3],
                (uy >> (FKL_I16_WIDTH + FKL_I24_WIDTH)) & I64_L24_MASK);
        l = 4;
    }
    return l;
}

#undef I24_L8_MASK
#undef I32_L16_MASK
#undef I64_L24_MASK

static inline FklByteCodelnt *set_and_append_ins_with_2_unsigned_imm(
        InsAppendMode m,
        FklByteCodelnt *bcl,
        FklOpcode op,
        uint64_t ux,
        uint64_t uy,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklInstruction ins[4] = { FKL_INSTRUCTION_STATIC_INIT };
    int l = set_ins_with_2_unsigned_imm(ins, op, ux, uy);
    if (bcl == NULL)
        bcl = create_0len_bcl();
    switch (m) {
    case INS_APPEND_BACK:
        for (int i = 0; i < l; i++)
            fklByteCodeLntPushBackIns(bcl, &ins[i], fid, line, scope);
        break;
    case INS_APPEND_FRONT:
        for (; l > 0; l--)
            fklByteCodeLntInsertFrontIns(&ins[l - 1], bcl, fid, line, scope);
    }
    return bcl;
}

static inline int
set_ins_with_signed_imm(FklInstruction *ins, FklOpcode op, int64_t k) {
    int l = 1;
    if (k >= INT16_MIN && k <= INT16_MAX) {
        ins[0].op = op;
        ins[0].bi = k;
    } else if (k >= FKL_I24_MIN && k <= FKL_I24_MAX) {
        ins[0].op = op + 1;
        set_ins_uc(ins, k + FKL_I24_OFFSET);
    } else if (k >= INT32_MIN && k <= INT32_MAX) {
        ins[0].op = op + 2;
        set_ins_ux(ins, k);
        l = 2;
    } else {
        ins[0].op = op + 3;
        set_ins_uxx(ins, k);
        l = 3;
    }
    return l;
}

static inline FklByteCodelnt *set_and_append_ins_with_signed_imm(
        InsAppendMode m,
        FklByteCodelnt *bcl,
        FklOpcode op,
        int64_t k,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklInstruction ins[3] = { FKL_INSTRUCTION_STATIC_INIT };
    int l = set_ins_with_signed_imm(ins, op, k);
    if (bcl == NULL)
        bcl = create_0len_bcl();
    switch (m) {
    case INS_APPEND_BACK:
        for (int i = 0; i < l; i++)
            fklByteCodeLntPushBackIns(bcl, &ins[i], fid, line, scope);
        break;
    case INS_APPEND_FRONT:
        for (; l > 0; l--)
            fklByteCodeLntInsertFrontIns(&ins[l - 1], bcl, fid, line, scope);
    }
    return bcl;
}

// static inline FklByteCode *set_and_append_ins_with_unsigned_imm_to_bc(
//         InsAppendMode m,
//         FklByteCode *bc,
//         FklOpcode op,
//         uint64_t k) {
//     FklInstruction ins[3] = { FKL_INSTRUCTION_STATIC_INIT };
//     int l = set_ins_with_unsigned_imm(ins, op, k);
//     if (bc == NULL)
//         bc = fklCreateByteCode(0);
//     switch (m) {
//     case INS_APPEND_BACK:
//         for (int i = 0; i < l; i++)
//             fklByteCodePushBack(bc, ins[i]);
//         break;
//     case INS_APPEND_FRONT:
//         for (; l > 0; l--)
//             fklByteCodeInsertFront(ins[l - 1], bc);
//     }
//     return bc;
// }
//
// static inline FklByteCode *
// append_push_sym_ins_to_bc(InsAppendMode m, FklByteCode *bc, FklVMvalue *sid)
// {
//     return set_and_append_ins_with_unsigned_imm_to_bc(m,
//             bc,
//             FKL_OP_PUSH_SYM,
//             sid);
// }

static inline FklByteCode *
append_push_chr_ins_to_bc(InsAppendMode m, FklByteCode *bc, char ch) {
    FklInstruction ins = FKL_INSTRUCTION_STATIC_INIT;
    ins.op = FKL_OP_PUSH_CHAR;
    ins.bu = (uint8_t)ch;
    if (bc == NULL)
        bc = fklCreateByteCode(0);
    switch (m) {
    case INS_APPEND_BACK:
        fklByteCodePushBack(bc, ins);
        break;
    case INS_APPEND_FRONT:
        fklByteCodeInsertFront(ins, bc);
        break;
    }
    return bc;
}

// static inline FklByteCode *append_push_str_ins_to_bc(InsAppendMode m,
//         FklByteCode *bc,
//         const FklString *k,
//         FklVMvalueCodegenInfo *info) {
//     return set_and_append_ins_with_unsigned_imm_to_bc(m,
//             bc,
//             FKL_OP_PUSH_STR,
//             fklAddStrConst(info->kt, k));
// }

static inline FklByteCode *append_push_const_ins_to_bc(InsAppendMode m,
        FklByteCode *bc,
        uint32_t k,
        FklVMvalueCodegenInfo *info) {
    FklInstruction ins = FKL_INSTRUCTION_STATIC_INIT;
    ins.op = FKL_OP_PUSH_CONST;
    set_ins_uc(&ins, k);
    if (bc == NULL)
        bc = fklCreateByteCode(0);
    switch (m) {
    case INS_APPEND_BACK:
        fklByteCodePushBack(bc, ins);
        break;
    case INS_APPEND_FRONT:
        fklByteCodeInsertFront(ins, bc);
    }
    return bc;
}

static inline FklByteCode *append_push_i24_ins_to_bc(InsAppendMode m,
        FklByteCode *bc,
        int32_t k,
        FklVMvalueCodegenInfo *info) {
    FklInstruction ins = FKL_INSTRUCTION_STATIC_INIT;
    if (k == 0 || k == 1) {
        ins.op = FKL_OP_PUSH_0 + k;
        goto append;
    } else if (k >= INT8_MIN && k <= INT8_MAX) {
        ins.op = FKL_OP_PUSH_I8;
        ins.ai = k;
        goto append;
    } else if (k >= INT16_MIN && k <= INT16_MAX) {
        ins.op = FKL_OP_PUSH_I16;
        ins.bi = k;
        goto append;
    } else if (k >= FKL_I24_MIN && k <= FKL_I24_MAX) {
        ins.op = FKL_OP_PUSH_I24;
        set_ins_uc(&ins, k + FKL_I24_OFFSET);
    append:
        if (bc == NULL)
            bc = fklCreateByteCode(0);
        switch (m) {
        case INS_APPEND_BACK:
            fklByteCodePushBack(bc, ins);
            break;
        case INS_APPEND_FRONT:
            fklByteCodeInsertFront(ins, bc);
        }
    } else {
        FKL_UNREACHABLE();
    }
    return bc;
}

static inline FklByteCodelnt *append_push_i24_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        int64_t k,
        FklVMvalueCodegenInfo *info,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklInstruction ins = FKL_INSTRUCTION_STATIC_INIT;
    if (k == 0 || k == 1) {
        ins.op = FKL_OP_PUSH_0 + k;
        goto append;
    } else if (k >= INT8_MIN && k <= INT8_MAX) {
        ins.op = FKL_OP_PUSH_I8;
        ins.ai = k;
        goto append;
    } else if (k >= INT16_MIN && k <= INT16_MAX) {
        ins.op = FKL_OP_PUSH_I16;
        ins.bi = k;
        goto append;
    } else if (k >= FKL_I24_MIN && k <= FKL_I24_MAX) {
        ins.op = FKL_OP_PUSH_I24;
        set_ins_uc(&ins, k + FKL_I24_OFFSET);
    append:
        if (bcl == NULL)
            bcl = create_0len_bcl();
        switch (m) {
        case INS_APPEND_BACK:
            fklByteCodeLntPushBackIns(bcl, &ins, fid, line, scope);
            break;
        case INS_APPEND_FRONT:
            fklByteCodeLntInsertFrontIns(&ins, bcl, fid, line, scope);
        }
    } else {
        FKL_UNREACHABLE();
    }
    return bcl;
}

// static inline FklByteCode *append_push_bigint_ins_to_bc(InsAppendMode m,
//         FklByteCode *bc,
//         const FklBigInt *k,
//         FklVMvalueCodegenInfo *info) {
//     if (fklIsBigIntGtLtI64(k))
//         return set_and_append_ins_with_unsigned_imm_to_bc(m,
//                 bc,
//                 FKL_OP_PUSH_BI,
//                 fklAddBigIntConst(info->kt, k));
//     else
//         return set_and_append_ins_with_unsigned_imm_to_bc(m,
//                 bc,
//                 FKL_OP_PUSH_I64B,
//                 fklAddI64Const(info->kt, fklBigIntToI(k)));
// }
//
// static inline FklByteCode *append_push_bvec_ins_to_bc(InsAppendMode m,
//         FklByteCode *bc,
//         const FklBytevector *k,
//         FklVMvalueCodegenInfo *info) {
//     return set_and_append_ins_with_unsigned_imm_to_bc(m,
//             bc,
//             FKL_OP_PUSH_BVEC,
//             fklAddBvecConst(info->kt, k));
// }
//
// static inline FklByteCode *append_push_f64_ins_to_bc(InsAppendMode m,
//         FklByteCode *bc,
//         double k,
//         FklVMvalueCodegenInfo *info) {
//     return set_and_append_ins_with_unsigned_imm_to_bc(m,
//             bc,
//             FKL_OP_PUSH_F64,
//             fklAddF64Const(info->kt, k));
// }

static inline FklByteCodelnt *append_push_proc_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t prototypeId,
        uint64_t len,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(m,
            bcl,
            FKL_OP_PUSH_PROC,
            prototypeId,
            len,
            fid,
            line,
            scope);
}

static inline FklByteCodelnt *append_jmp_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        int64_t len,
        JmpInsCondition condition,
        JmpInsOrientation orien,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklOpcode op = FKL_OP_DUMMY;
    switch (condition) {
    case JMP_UNCOND:
        op = FKL_OP_JMP;
        break;
    case JMP_IF_TRUE:
        op = FKL_OP_JMP_IF_TRUE;
        break;
    case JMP_IF_FALSE:
        op = FKL_OP_JMP_IF_FALSE;
        break;
    }
    switch (orien) {
    case JMP_FORWARD:
        return set_and_append_ins_with_signed_imm(m,
                bcl,
                op,
                len,
                fid,
                line,
                scope);
        break;
    case JMP_BACKWARD:
        len = -len - 1;
        if (len >= FKL_I24_MIN)
            return set_and_append_ins_with_signed_imm(m,
                    bcl,
                    op,
                    len,
                    fid,
                    line,
                    scope);
        len -= 1;
        if (len >= INT32_MIN)
            return set_and_append_ins_with_signed_imm(m,
                    bcl,
                    op,
                    len,
                    fid,
                    line,
                    scope);
        len -= 1;
        return set_and_append_ins_with_signed_imm(m,
                bcl,
                op,
                len,
                fid,
                line,
                scope);
        break;
    }
    return bcl;
}

static inline FklByteCodelnt *append_import_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t idx1,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(m,
            bcl,
            FKL_OP_IMPORT,
            idx,
            idx1,
            fid,
            line,
            scope);
}

static inline FklByteCodelnt *append_load_lib_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t lib_id,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m,
            bcl,
            FKL_OP_LOAD_LIB,
            lib_id,
            fid,
            line,
            scope);
}

static inline FklByteCodelnt *append_load_dll_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t lib_id,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m,
            bcl,
            FKL_OP_LOAD_DLL,
            lib_id,
            fid,
            line,
            scope);
}

static inline FklByteCodelnt *append_export_to_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t libId,
        uint32_t num,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(m,
            bcl,
            FKL_OP_EXPORT_TO,
            libId,
            num,
            fid,
            line,
            scope);
}

static inline FklByteCodelnt *append_export_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m,
            bcl,
            FKL_OP_EXPORT,
            idx,
            fid,
            line,
            scope);
}

static inline FklInstruction create_op_ins(FklOpcode op) {
    return (FklInstruction){ .op = op };
}

static inline FklByteCodelnt *append_pop_loc_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m,
            bcl,
            FKL_OP_POP_LOC,
            idx,
            fid,
            line,
            scope);
}

static inline FklByteCodelnt *append_put_loc_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m,
            bcl,
            FKL_OP_PUT_LOC,
            idx,
            fid,
            line,
            scope);
}

static inline FklByteCodelnt *append_put_var_ref_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m,
            bcl,
            FKL_OP_PUT_VAR_REF,
            idx,
            fid,
            line,
            scope);
}

static FklCodegenAction *create_cg_action(FklCodegenActionCb f,
        FklCodegenActionContext *context,
        FklCodegenNextExpression *nextExpression,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        FklVMvalueCodegenEnv *env,
        uint64_t curline,
        FklCodegenAction *prev,
        FklVMvalueCodegenInfo *codegen) {
    FklCodegenAction *r =
            (FklCodegenAction *)fklZmalloc(sizeof(FklCodegenAction));
    FKL_ASSERT(r);
    r->scope = scope;
    r->macros = macro_scope;
    r->processer = f;
    r->context = context;
    r->expressions = nextExpression;
    r->env = env;
    r->curline = curline;
    r->codegen = codegen;
    r->prev = prev;
    r->codegen = codegen;
    fklByteCodelntVectorInit(&r->bcl_vector, 0);
    return r;
}

static void destroy_cg_action(FklCodegenAction *action) {
    FklCodegenNextExpression *nextExpression = action->expressions;
    if (nextExpression) {
        if (nextExpression->t->finalizer)
            nextExpression->t->finalizer(nextExpression->context);
        fklZfree(nextExpression);
    }
    FklCodegenActionContext *ctx = action->context;
    if (ctx->t->__finalizer) {
        ctx->t->__finalizer(ctx->d);
    }
    fklZfree(action->context);
    action->context = NULL;

    action->context = NULL;

    FklByteCodelntVector *s = &action->bcl_vector;
    while (!fklByteCodelntVectorIsEmpty(s))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBackNonNull(s));
    fklByteCodelntVectorUninit(s);

    fklZfree(action);
}

#define FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(F,                            \
        STACK,                                                                 \
        NEXT_EXPRESSIONS,                                                      \
        SCOPE,                                                                 \
        MACRO_SCOPE,                                                           \
        ENV,                                                                   \
        LINE,                                                                  \
        CODEGEN,                                                               \
        CODEGEN_CONTEXT)                                                       \
    fklCodegenActionVectorPushBack2((CODEGEN_CONTEXT),                         \
            create_cg_action((F),                                              \
                    (STACK),                                                   \
                    (NEXT_EXPRESSIONS),                                        \
                    (SCOPE),                                                   \
                    (MACRO_SCOPE),                                             \
                    (ENV),                                                     \
                    (LINE),                                                    \
                    NULL,                                                      \
                    (CODEGEN)))

#define BC_PROCESS(NAME)                                                       \
    static FklByteCodelnt *NAME(FklVMvalueCodegenInfo *codegen,                \
            FklVMvalueCodegenEnv *env,                                         \
            uint32_t scope,                                                    \
            FklVMvalueCodegenMacroScope *cms,                                  \
            void *data,                                                        \
            FklByteCodelntVector *bcl_vec,                                     \
            FklVMvalue *fid,                                                   \
            uint64_t line,                                                     \
            FklCodegenErrorState *error_state,                                 \
            FklCodegenCtx *ctx)

BC_PROCESS(_default_bc_process) {
    if (!fklByteCodelntVectorIsEmpty(bcl_vec))
        return *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    else
        return NULL;
}

BC_PROCESS(_empty_bc_process) { return NULL; }

static FklByteCodelnt *sequnce_exp_bc_process(FklByteCodelntVector *stack,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    if (stack->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *retval = fklCreateByteCodelnt(0);
        size_t top = stack->size;
        for (size_t i = 0; i < top; i++) {
            FklByteCodelnt *cur = stack->base[i];
            if (cur->bc.len) {
                fklCodeLntConcat(retval, cur);
                if (i < top - 1)
                    fklByteCodeLntPushBackIns(retval, &drop, fid, line, scope);
            }
            fklDestroyByteCodelnt(cur);
        }
        stack->size = 0;
        return retval;
    } else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

BC_PROCESS(_begin_exp_bc_process) {
    return sequnce_exp_bc_process(bcl_vec, fid, line, scope);
}

static inline void
pushListItemToQueue(FklVMvalue *list, CgExpQueue *queue, FklVMvalue **last) {
    for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list))
        cgExpQueuePush2(queue,
                (FklPmatchRes){
                    .value = FKL_VM_CAR(list),
                    .container = list,
                });
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr)
    //     fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(list->pair->car));
    if (last)
        *last = list;
}

static inline void pushListItemToStack(FklVMvalue *list,
        FklVMvalueVector *stack,
        FklVMvalue **last) {
    for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list))
        fklVMvalueVectorPushBack2(stack, FKL_VM_CAR(list));
    // for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr)
    //     fklNastNodeVectorPushBack2(stack, list->pair->car);
    if (last)
        *last = list;
}

static inline int is_get_var_ref_ins(const FklInstruction *ins) {
    return ins->op >= FKL_OP_GET_VAR_REF && ins->op <= FKL_OP_GET_VAR_REF_X;
}

static inline FklBuiltinInlineFunc is_inlinable_func_ref(const FklByteCode *bc,
        FklVMvalueCodegenEnv *env,
        uint32_t argNum,
        FklVMvalueCodegenInfo *codegen) {
    FklInstructionArg arg;
    const FklInstruction *ins = &bc->code[0];
    if (is_get_var_ref_ins(ins)
            && bc->len == (unsigned)fklGetInsOpArg(ins, &arg)) {
        uint32_t idx = arg.ux;
        FklSymDefHashMapElm *ref = NULL;
        while (env) {
            FklSymDefHashMapNode *list = env->refs.first;
            for (; list; list = list->next) {
                if (list->v.idx == idx) {
                    if (list->v.isLocal)
                        env = NULL;
                    else {
                        if (!env->prev && env == codegen->global_env)
                            ref = &list->elm;
                        else
                            idx = list->v.cidx;
                        if (idx == FKL_VAR_REF_INVALID_CIDX)
                            return NULL;
                        env = env->prev;
                    }
                    break;
                }
            }
            if (!list)
                break;
        }
        if (ref && idx < FKL_BUILTIN_SYMBOL_NUM)
            return fklGetBuiltinInlineFunc(idx, argNum);
    }
    return NULL;
}

BC_PROCESS(_funcall_exp_bc_process) {
    if (bcl_vec->size) {
        FklByteCodelnt *func = bcl_vec->base[0];
        FklByteCode *funcBc = &func->bc;
        uint32_t argNum = bcl_vec->size - 1;
        FklBuiltinInlineFunc inlFunc = NULL;
        if (argNum < 4
                && (inlFunc = is_inlinable_func_ref(funcBc,
                            env,
                            argNum,
                            codegen))) {
            fklDestroyByteCodelnt(func);
            bcl_vec->size = 0;
            return inlFunc(bcl_vec->base + 1, fid, line, scope);
        } else {
            FklByteCodelnt *retval = fklCreateByteCodelnt(0);
            FklByteCodelnt **base = bcl_vec->base;
            FklByteCodelnt **const end = base + bcl_vec->size;
            for (; base < end; ++base) {
                FklByteCodelnt *cur = *base;
                fklCodeLntConcat(retval, cur);
                fklDestroyByteCodelnt(cur);
            }
            bcl_vec->size = 0;
            FklInstruction setBp = create_op_ins(FKL_OP_SET_BP);
            FklInstruction call = create_op_ins(FKL_OP_CALL);
            fklByteCodeLntInsertFrontIns(&setBp, retval, fid, line, scope);
            fklByteCodeLntPushBackIns(retval, &call, fid, line, scope);
            return retval;
        }
    } else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

#define CURLINE(V) get_curline(codegen, V)

static void codegen_funcall(const FklPmatchRes *rest,
        FklCodegenActionVector *actions,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenErrorState *error_state) {
    CgExpQueue *queue = cgExpQueueCreate();
    FklVMvalue *last = NULL;
    pushListItemToQueue(rest->value, queue, &last);
    if (last != FKL_VM_NIL) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(rest->value, CURLINE(rest->container));
        // error_state->place = rest->value;
        // error_state->line = CURLINE(rest->container);
        // while (!fklNastNodeQueueIsEmpty(queue))
        //     fklDestroyNastNode(*fklNastNodeQueuePop(queue));
        cgExpQueueDestroy(queue);
    } else {
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_funcall_exp_bc_process,
                createDefaultStackContext(),
                createMustHasRetvalQueueNextExpression(queue),
                scope,
                macro_scope,
                env,
                CURLINE(rest->container),
                codegen,
                actions);
    }
}

#define CODEGEN_ARGS                                                           \
    const FklPmatchRes *orig, FklPmatchHashMap *ht,                            \
            FklCodegenActionVector *actions, uint32_t scope,                   \
            FklVMvalueCodegenMacroScope *macro_scope,                          \
            FklVMvalueCodegenEnv *env, FklVMvalueCodegenInfo *codegen,         \
            FklCodegenCtx *ctx, FklCodegenErrorState *error_state,             \
            uint8_t must_has_retval

typedef struct {
    const FklPmatchHashMap *orig;
    FklPmatchHashMap *ht;
    FklCodegenActionVector *actions;
    uint32_t scope;
    FklVMvalueCodegenMacroScope *macro_scope;
    FklVMvalueCodegenEnv *env;
    FklVMvalueCodegenInfo *codegen;
    FklCodegenCtx *ctx;
    FklCodegenErrorState *error_state;
    uint8_t must_has_retval;
} CgCbArgs;

#define CODEGEN_FUNC(NAME) void NAME(CODEGEN_ARGS)

static CODEGEN_FUNC(codegen_begin) {
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_begin_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(rest->container),
            codegen,
            actions);
}

static inline int reset_flag_and_check_var_be_refed(uint8_t *flags,
        FklCodegenEnvScope *sc,
        uint32_t scope,
        FklVMvalueCodegenEnv *env,
        FklFuncPrototypes *cp,
        uint32_t *start,
        uint32_t *end) {
    fklResolveRef(env, scope, cp, NULL);
    int r = 0;
    uint32_t last = sc->start + sc->end;
    uint32_t i = sc->start;
    uint32_t s = i;
    for (; i < last; i++) {
        r = flags[i] == FKL_CODEGEN_ENV_SLOT_REF;
        flags[i] = 0;
        if (r) {
            s = i;
            break;
        }
    }
    uint32_t e = i;
    for (; i < last; i++) {
        if (flags[i] == FKL_CODEGEN_ENV_SLOT_REF)
            e = i;
        flags[i] = 0;
    }
    *start = s;
    *end = e + 1;
    return r;
}

static inline FklByteCodelnt *append_close_ref_ins(InsAppendMode m,
        FklByteCodelnt *retval,
        uint32_t s,
        uint32_t e,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(m,
            retval,
            FKL_OP_CLOSE_REF,
            s,
            e,
            fid,
            line,
            scope);
}

static inline void check_and_close_ref(FklByteCodelnt *retval,
        uint32_t scope,
        FklVMvalueCodegenEnv *env,
        FklFuncPrototypes *pa,
        FklVMvalue *fid,
        uint32_t line) {
    FklCodegenEnvScope *cur = &env->scopes[scope - 1];
    uint32_t start = cur->start;
    uint32_t end = start + 1;
    if (reset_flag_and_check_var_be_refed(env->slotFlags,
                cur,
                scope,
                env,
                pa,
                &start,
                &end))
        append_close_ref_ins(INS_APPEND_BACK,
                retval,
                start,
                end,
                fid,
                line,
                scope);
}

BC_PROCESS(_local_exp_bc_process) {
    FklByteCodelnt *retval = sequnce_exp_bc_process(bcl_vec, fid, line, scope);
    check_and_close_ref(retval, scope, env, &codegen->pts->p, fid, line);
    return retval;
}

static CODEGEN_FUNC(codegen_local) {
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);
    pushListItemToQueue(rest->value, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_local_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            codegen,
            actions);
}

static CODEGEN_FUNC(codegen_let0) {
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);
    pushListItemToQueue(rest->value, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_local_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            codegen,
            actions);
}

typedef struct Let1Context {
    FklVMvalueVector *ss;
} Let1Context;

static void _let1_finalizer(void *d) {
    Let1Context *dd = (Let1Context *)d;
    fklVMvalueVectorDestroy(dd->ss);
}

static FklCodegenActionContextMethodTable Let1ContextMethodTable = {
    .size = sizeof(Let1Context),
    .__finalizer = _let1_finalizer,
};

static FklCodegenActionContext *createLet1CodegenContext(FklVMvalueVector *ss) {
    FklCodegenActionContext *r =
            createCodegenActionContext(&Let1ContextMethodTable);
    FKL_TYPE_CAST(Let1Context *, r->d)->ss = ss;
    return r;
}

typedef struct Do1Context {
    FklUintVector *ss;
} Do1Context;

static void _do1_finalizer(void *d) {
    Do1Context *dd = (Do1Context *)d;
    fklUintVectorDestroy(dd->ss);
}

static FklCodegenActionContextMethodTable Do1ContextMethodTable = {
    .size = sizeof(Do1Context),
    .__finalizer = _do1_finalizer,
};

static FklCodegenActionContext *createDo1CodegenContext(FklUintVector *ss) {
    FklCodegenActionContext *r =
            createCodegenActionContext(&Do1ContextMethodTable);
    FKL_TYPE_CAST(Do1Context *, r->d)->ss = ss;
    return r;
}

static int is_valid_let_arg(const FklVMvalue *node,
        FklVMvalue *const *builtin_pattern_node) {
    return FKL_IS_PAIR(node) && fklIsList(node) && fklVMlistLength(node) == 2
        && FKL_IS_SYM(FKL_VM_CAR(node));

    // return node->type == FKL_NAST_PAIR && fklIsNastNodeList(node)
    //     && nast_list_len(node) == 2 && node->pair->car->type == FKL_NAST_SYM;
}

static int is_valid_let_args(const FklVMvalue *sl,
        FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklVMvalueVector *stack,
        FklVMvalue *const *builtin_pattern_node) {
    if (fklIsList(sl)) {
        for (; FKL_IS_PAIR(sl); sl = FKL_VM_CDR(sl)) {
            FklVMvalue *cc = FKL_VM_CAR(sl);
            if (!is_valid_let_arg(cc, builtin_pattern_node))
                return 0;
            FklVMvalue *id = FKL_VM_CAR(cc);
            if (fklIsSymbolDefined(id, scope, env))
                return 0;
            fklVMvalueVectorPushBack2(stack, id);
            fklAddCodegenDefBySid(id, scope, env);
        }
        return 1;
    } else
        return 0;
}

static int is_valid_letrec_args(const FklVMvalue *sl,
        FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklVMvalueVector *stack,
        FklVMvalue *const *builtin_pattern_node) {
    if (fklIsList(sl)) {
        for (; FKL_IS_PAIR(sl); sl = FKL_VM_CDR(sl)) {
            FklVMvalue *cc = FKL_VM_CAR(sl);
            if (!is_valid_let_arg(cc, builtin_pattern_node))
                return 0;
            FklVMvalue *id = FKL_VM_CAR(cc);
            if (fklIsSymbolDefined(id, scope, env))
                return 0;
            fklVMvalueVectorPushBack2(stack, id);
            fklAddCodegenPreDefBySid(id, scope, 0, env);
        }
        return 1;
    } else
        return 0;
}

BC_PROCESS(_let1_exp_bc_process) {
    Let1Context *let_ctx = FKL_TYPE_CAST(Let1Context *, data);
    FklVMvalueVector *symbolStack = let_ctx->ss;

    FklByteCodelnt *retval = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    FklVMvalue **cur_sid = symbolStack->base;
    FklVMvalue **const sid_end = cur_sid + symbolStack->size;
    for (; cur_sid < sid_end; ++cur_sid) {
        FklVMvalue *id = *cur_sid;
        uint32_t idx = fklAddCodegenDefBySid(id, scope, env)->idx;
        append_pop_loc_ins(INS_APPEND_FRONT, retval, idx, fid, line, scope);
    }
    if (!fklByteCodelntVectorIsEmpty(bcl_vec)) {
        FklByteCodelnt *args = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
        fklCodeLntReverseConcat(args, retval);
        fklDestroyByteCodelnt(args);
    }
    return retval;
}

BC_PROCESS(_let_arg_exp_bc_process) {
    if (bcl_vec->size) {
        FklByteCodelnt *retval = fklCreateByteCodelnt(0);
        FklByteCodelnt **cur_bcl = bcl_vec->base;
        FklByteCodelnt **const end = cur_bcl + bcl_vec->size;
        for (; cur_bcl < end; ++cur_bcl) {
            FklByteCodelnt *cur = *cur_bcl;
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }
        bcl_vec->size = 0;
        return retval;
    } else
        return NULL;
}

BC_PROCESS(_letrec_exp_bc_process) {
    FklByteCodelnt *body = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    FklByteCodelnt *retval = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    fklCodeLntConcat(retval, body);
    fklDestroyByteCodelnt(body);
    return retval;
}

BC_PROCESS(_letrec_arg_exp_bc_process) {
    Let1Context *let_ctx = FKL_TYPE_CAST(Let1Context *, data);
    FklVMvalueVector *symbolStack = let_ctx->ss;

    FklByteCodelnt *retval = create_0len_bcl();
    FklVMvalue **cur_sid = symbolStack->base;
    FklVMvalue **const sid_end = cur_sid + symbolStack->size;
    FklByteCodelnt **cur_bcl = bcl_vec->base;
    for (; cur_sid < sid_end; ++cur_sid, ++cur_bcl) {
        FklByteCodelnt *value_bc = *cur_bcl;
        FklVMvalue *id = *cur_sid;
        uint32_t idx = fklAddCodegenDefBySid(id, scope, env)->idx;
        fklResolveCodegenPreDef(id, scope, env, &codegen->pts->p);
        fklCodeLntConcat(retval, value_bc);
        append_pop_loc_ins(INS_APPEND_BACK, retval, idx, fid, line, scope);
        fklDestroyByteCodelnt(value_bc);
    }
    bcl_vec->size = 0;
    return retval;
}

static CODEGEN_FUNC(codegen_let1) {
    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    FklVMvalueVector *symStack = fklVMvalueVectorCreate(8);
    const FklPmatchRes *first =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    if (!FKL_IS_SYM(first->value)) {
        fklVMvalueVectorDestroy(symStack);
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }
    const FklPmatchRes *item =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);

    FklVMvalue *args = item ? item->value : NULL;
    uint32_t cs = enter_new_scope(scope, env);

    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);

    fklAddCodegenDefBySid(first->value, cs, env);
    fklVMvalueVectorPushBack2(symStack, first->value);

    CgExpQueue *valueQueue = cgExpQueueCreate();
    cgExpQueuePush(valueQueue, value);

    if (args) {
        if (!is_valid_let_args(args, env, cs, symStack, builtin_pattern_node)) {
            cgExpQueueDestroy(valueQueue);
            fklVMvalueVectorDestroy(symStack);
            error_state->type = FKL_ERR_SYNTAXERROR;
            error_state->p = PLACE(orig->value, CURLINE(orig->container));
            // error_state->place = orig->value;
            return;
        }
        for (FklVMvalue *cur = args; FKL_IS_PAIR(cur); cur = FKL_VM_CDR(cur))
            cgExpQueuePush2(valueQueue,
                    (FklPmatchRes){
                        .value = cadr(FKL_VM_CAR(cur)),
                        .container = cdr(FKL_VM_CAR(cur)),
                    });
    }

    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    FklCodegenAction *let1Action = create_cg_action(_let1_exp_bc_process,
            createLet1CodegenContext(symStack),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            codegen);

    fklCodegenActionVectorPushBack2(actions, let1Action);

    FklCodegenAction *restAction = create_cg_action(_local_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->value),
            let1Action,
            codegen);
    fklCodegenActionVectorPushBack2(actions, restAction);

    FklCodegenAction *argAction = create_cg_action(_let_arg_exp_bc_process,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(valueQueue),
            scope,
            macro_scope,
            env,
            CURLINE(first->value),
            let1Action,
            codegen);
    fklCodegenActionVectorPushBack2(actions, argAction);
}

static CODEGEN_FUNC(codegen_let81) {
    FklVMvalue *letHead = add_symbol_cstr(ctx, "let");
    // FklVMvalue *letHead =
    //         fklCreateNastNode(FKL_NAST_SYM, orig->pair->car->curline);
    // letHead->sym = letHeadId;

    FklVMvalue *first_name = cadr(orig->value);
    FKL_VM_CDR(first_name) = FKL_VM_NIL;

    const FklPmatchRes *args =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);

    // FklNastNode *restLet8 = fklCreateNastNode(FKL_NAST_PAIR, args->curline);
    // restLet8->pair = fklCreateNastPair();
    // restLet8->pair->car = args;
    // restLet8->pair->cdr = fklMakeNastNodeRef(rest);
    // FklNastNode *old = orig->pair->cdr;
    // orig->pair->cdr = restLet8;

    FklVMvalue *restLet8 =
            fklCreateVMvaluePair(&ctx->gc->gcvm, args->value, rest->value);
    FKL_VM_CDR(orig->value) = restLet8;

    ListElm a[3] = {
        { .v = letHead, .line = CURLINE(letHead) },
        { .v = first_name, .line = CURLINE(first_name) },
        { .v = orig->value, .line = CURLINE(orig->container) },
    };
    letHead =
            create_nast_list(a, 3, CURLINE(orig->value), &ctx->gc->gcvm, NULL);
    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush2(queue,
            (FklPmatchRes){
                .value = letHead,
                .container = orig->value,
            });
    // firstNameValue->pair->cdr =
    //         fklCreateNastNode(FKL_NAST_NIL,
    //         firstNameValue->pair->cdr->curline);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_default_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            codegen,
            actions);
}

static CODEGEN_FUNC(codegen_letrec) {
    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    FklVMvalueVector *symStack = fklVMvalueVectorCreate(8);
    const FklPmatchRes *first =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    if (!FKL_IS_SYM(first->value)) {
        fklVMvalueVectorDestroy(symStack);
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }
    const FklPmatchRes *args =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);
    uint32_t cs = enter_new_scope(scope, env);

    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);

    fklAddCodegenPreDefBySid(first->value, cs, 0, env);
    fklVMvalueVectorPushBack2(symStack, first->value);

    if (!is_valid_letrec_args(args->value,
                env,
                cs,
                symStack,
                builtin_pattern_node)) {
        fklVMvalueVectorDestroy(symStack);
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }

    CgExpQueue *valueQueue = cgExpQueueCreate();
    cgExpQueuePush(valueQueue, value);
    for (FklVMvalue *cur = args->value; FKL_IS_PAIR(cur);
            cur = FKL_VM_CDR(cur)) {
        cgExpQueuePush2(valueQueue,
                (FklPmatchRes){
                    .value = cadr(FKL_VM_CAR(cur)),
                    .container = cdr(FKL_VM_CAR(cur)),
                });
    }
    // for (FklNastNode *cur = args; cur->type == FKL_NAST_PAIR;
    //         cur = cur->pair->cdr)
    //     fklNastNodeQueuePush2(valueQueue,
    //             fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)));

    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    FklCodegenAction *let1Action = create_cg_action(_letrec_exp_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            codegen);

    fklCodegenActionVectorPushBack2(actions, let1Action);

    FklCodegenAction *restAction = create_cg_action(_local_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            let1Action,
            codegen);
    fklCodegenActionVectorPushBack2(actions, restAction);

    FklCodegenAction *argAction = create_cg_action(_letrec_arg_exp_bc_process,
            createLet1CodegenContext(symStack),
            createMustHasRetvalQueueNextExpression(valueQueue),
            cs,
            cms,
            env,
            CURLINE(orig->container),
            // firstSymbol->curline,
            let1Action,
            codegen);
    fklCodegenActionVectorPushBack2(actions, argAction);
}

static inline void insert_jmp_if_true_and_jmp_back_between(FklByteCodelnt *cond,
        FklByteCodelnt *rest,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklByteCode *cond_bc = &cond->bc;
    FklByteCode *rest_bc = &rest->bc;
    uint32_t jmp_back_ins_len = 3;

    uint64_t cond_len = cond_bc->len;
    uint64_t rest_len = rest_bc->len;

    append_jmp_ins(INS_APPEND_BACK,
            cond,
            rest_bc->len + jmp_back_ins_len,
            JMP_IF_TRUE,
            JMP_FORWARD,
            fid,
            line,
            scope);

    append_jmp_ins(INS_APPEND_BACK,
            rest,
            rest_bc->len + cond_bc->len,
            JMP_UNCOND,
            JMP_BACKWARD,
            fid,
            line,
            scope);

    uint32_t jmp_back_actual_len = rest_bc->len - rest_len;

    while (jmp_back_ins_len > jmp_back_actual_len) {
        rest_bc->len = rest_len;
        cond_bc->len = cond_len;
        jmp_back_ins_len = jmp_back_actual_len;

        append_jmp_ins(INS_APPEND_BACK,
                cond,
                rest_bc->len + jmp_back_ins_len,
                JMP_IF_TRUE,
                JMP_FORWARD,
                fid,
                line,
                scope);

        append_jmp_ins(INS_APPEND_BACK,
                rest,
                rest_bc->len + cond_bc->len,
                JMP_UNCOND,
                JMP_BACKWARD,
                fid,
                line,
                scope);

        jmp_back_actual_len = rest_bc->len - rest_len;
    }
}

BC_PROCESS(_do0_exp_bc_process) {
    FklByteCodelnt *rest = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    FklByteCodelnt *value = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    FklByteCodelnt *cond = *fklByteCodelntVectorPopBackNonNull(bcl_vec);

    FklInstruction pop = create_op_ins(FKL_OP_DROP);

    fklByteCodeLntInsertFrontIns(&pop, rest, fid, line, scope);
    insert_jmp_if_true_and_jmp_back_between(cond, rest, fid, line, scope);

    fklCodeLntConcat(cond, rest);
    fklDestroyByteCodelnt(rest);

    if (value->bc.len)
        fklByteCodeLntPushBackIns(cond, &pop, fid, line, scope);
    fklCodeLntReverseConcat(cond, value);
    fklDestroyByteCodelnt(cond);

    return value;
}

BC_PROCESS(_do_rest_exp_bc_process) {
    if (bcl_vec->size) {
        FklInstruction pop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *r = sequnce_exp_bc_process(bcl_vec, fid, line, scope);
        fklByteCodeLntPushBackIns(r, &pop, fid, line, scope);
        return r;
    }
    return fklCreateByteCodelnt(0);
}

static CODEGEN_FUNC(codegen_do0) {
    const FklPmatchRes *cond =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_cond);
    const FklPmatchRes *item =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);

    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);

    FklCodegenAction *do0Action = create_cg_action(_do0_exp_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(cond->container),
            // cond->curline,
            NULL,
            codegen);
    fklCodegenActionVectorPushBack2(actions, do0Action);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_do_rest_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(cond->container),
            // cond->curline,
            codegen,
            actions);

    if (item) {
        CgExpQueue *vQueue = cgExpQueueCreate();
        cgExpQueuePush(vQueue, item);
        FklCodegenAction *do0VAction = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                createMustHasRetvalQueueNextExpression(vQueue),
                cs,
                cms,
                env,
                CURLINE(orig->container),
                // orig->curline,
                do0Action,
                codegen);
        fklCodegenActionVectorPushBack2(actions, do0VAction);
    } else {
        FklByteCodelnt *v = fklCreateByteCodelnt(0);
        FklCodegenAction *action = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                NULL,
                cs,
                cms,
                env,
                CURLINE(orig->container),
                // orig->curline,
                do0Action,
                codegen);
        fklByteCodelntVectorPushBack2(&action->bcl_vector, v);
        fklCodegenActionVectorPushBack2(actions, action);
    }
    CgExpQueue *cQueue = cgExpQueueCreate();
    cgExpQueuePush(cQueue, cond);
    FklCodegenAction *do0CAction = create_cg_action(_default_bc_process,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(cQueue),
            cs,
            cms,
            env,
            CURLINE(orig->container),
            // orig->curline,
            do0Action,
            codegen);
    fklCodegenActionVectorPushBack2(actions, do0CAction);
}

static inline int is_valid_do_var_bind(const FklVMvalue *list,
        FklPmatchRes *next,
        FklVMvalue *const *builtin_pattern_node) {
    if (!fklIsList(list))
        return 0;
    // if (list->pair->car->type != FKL_NAST_SYM)
    if (!FKL_IS_SYM(FKL_VM_CAR(list)))
        return 0;
    size_t len = fklVMlistLength(list);
    if (len != 2 && len != 3)
        return 0;
    if (len == 3) {
        *next = (FklPmatchRes){
            .value = caddr(list),
            .container = cddr(list),
        };
        // *nextV = next;
    }
    return 1;
}

static inline int is_valid_do_bind_list(const FklVMvalue *sl,
        FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklUintVector *stack,
        FklUintVector *nstack,
        CgExpQueue *valueQueue,
        CgExpQueue *nextQueue,
        FklVMvalue *const *builtin_pattern_node) {
    if (fklIsList(sl)) {
        for (; FKL_IS_PAIR(sl); sl = FKL_VM_CDR(sl)) {
            FklVMvalue *cc = FKL_VM_CAR(sl);
            // FklVMvalue *nextExp = NULL;
            FklPmatchRes nextExp = { NULL, NULL };
            if (!is_valid_do_var_bind(cc, &nextExp, builtin_pattern_node))
                return 0;
            FklVMvalue *id = FKL_VM_CAR(cc);
            if (fklIsSymbolDefined(id, scope, env))
                return 0;
            uint32_t idx = fklAddCodegenDefBySid(id, scope, env)->idx;
            fklUintVectorPushBack2(stack, idx);
            cgExpQueuePush2(valueQueue,
                    (FklPmatchRes){
                        .value = cadr(cc),
                        .container = cdr(cc),
                    });
            if (nextExp.value) {
                fklUintVectorPushBack2(nstack, idx);
                cgExpQueuePush(nextQueue, &nextExp);
            }
        }
        return 1;
    }
    return 0;
}

BC_PROCESS(_do1_init_val_bc_process) {
    FklByteCodelnt *ret = fklCreateByteCodelnt(0);
    Do1Context *let_ctx = FKL_TYPE_CAST(Do1Context *, data);
    FklUintVector *ss = let_ctx->ss;

    FklInstruction pop = create_op_ins(FKL_OP_DROP);

    uint64_t *idxbase = ss->base;
    FklByteCodelnt **bclBase = bcl_vec->base;
    uint32_t top = bcl_vec->size;
    for (uint32_t i = 0; i < top; i++) {
        uint32_t idx = idxbase[i];
        FklByteCodelnt *curBcl = bclBase[i];
        append_put_loc_ins(INS_APPEND_BACK, curBcl, idx, fid, line, scope);
        fklByteCodeLntPushBackIns(curBcl, &pop, fid, line, scope);
        fklCodeLntConcat(ret, curBcl);
    }
    return ret;
}

BC_PROCESS(_do1_next_val_bc_process) {
    Do1Context *let_ctx = FKL_TYPE_CAST(Do1Context *, data);
    FklUintVector *ss = let_ctx->ss;

    if (bcl_vec->size) {
        FklByteCodelnt *ret = fklCreateByteCodelnt(0);
        FklInstruction pop = create_op_ins(FKL_OP_DROP);

        uint64_t *idxbase = ss->base;
        FklByteCodelnt **bclBase = bcl_vec->base;
        uint32_t top = bcl_vec->size;
        for (uint32_t i = 0; i < top; i++) {
            uint32_t idx = idxbase[i];
            FklByteCodelnt *curBcl = bclBase[i];
            append_put_loc_ins(INS_APPEND_BACK, curBcl, idx, fid, line, scope);
            fklByteCodeLntPushBackIns(curBcl, &pop, fid, line, scope);
            fklCodeLntConcat(ret, curBcl);
        }
        return ret;
    }
    return NULL;
}

BC_PROCESS(_do1_bc_process) {
    FklByteCodelnt *next = bcl_vec->size == 5
                                 ? *fklByteCodelntVectorPopBackNonNull(bcl_vec)
                                 : NULL;
    FklByteCodelnt *rest = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    FklByteCodelnt *value = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    FklByteCodelnt *cond = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    FklByteCodelnt *init = *fklByteCodelntVectorPopBackNonNull(bcl_vec);

    FklInstruction pop = create_op_ins(FKL_OP_DROP);

    fklByteCodeLntInsertFrontIns(&pop, rest, fid, line, scope);
    if (next) {
        fklCodeLntConcat(rest, next);
        fklDestroyByteCodelnt(next);
    }

    insert_jmp_if_true_and_jmp_back_between(cond, rest, fid, line, scope);

    fklCodeLntConcat(cond, rest);
    fklDestroyByteCodelnt(rest);

    if (value->bc.len)
        fklByteCodeLntPushBackIns(cond, &pop, fid, line, scope);
    fklCodeLntReverseConcat(cond, value);
    fklDestroyByteCodelnt(cond);

    fklCodeLntReverseConcat(init, value);
    fklDestroyByteCodelnt(init);
    check_and_close_ref(value, scope, env, &codegen->pts->p, fid, line);
    return value;
}

static CODEGEN_FUNC(codegen_do1) {
    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    FklVMvalue *bindlist = cadr(orig->value);

    const FklPmatchRes *cond =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_cond);
    const FklPmatchRes *item =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);

    FklUintVector *symStack = fklUintVectorCreate(4);
    FklUintVector *nextSymStack = fklUintVectorCreate(4);

    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);

    CgExpQueue *valueQueue = cgExpQueueCreate();
    CgExpQueue *nextValueQueue = cgExpQueueCreate();
    if (!is_valid_do_bind_list(bindlist,
                env,
                cs,
                symStack,
                nextSymStack,
                valueQueue,
                nextValueQueue,
                builtin_pattern_node)) {
        fklUintVectorDestroy(symStack);
        fklUintVectorDestroy(nextSymStack);
        cgExpQueueDestroy(valueQueue);
        cgExpQueueDestroy(nextValueQueue);

        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }

    FklCodegenAction *do1Action = create_cg_action(_do1_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            codegen);
    fklCodegenActionVectorPushBack2(actions, do1Action);

    FklCodegenAction *do1NextValAction =
            create_cg_action(_do1_next_val_bc_process,
                    createDo1CodegenContext(nextSymStack),
                    createMustHasRetvalQueueNextExpression(nextValueQueue),
                    cs,
                    cms,
                    env,
                    CURLINE(orig->container),
                    do1Action,
                    codegen);

    fklCodegenActionVectorPushBack2(actions, do1NextValAction);

    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    FklCodegenAction *do1RestAction = create_cg_action(_do_rest_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(orig->container),
            do1Action,
            codegen);
    fklCodegenActionVectorPushBack2(actions, do1RestAction);

    if (item) {
        CgExpQueue *vQueue = cgExpQueueCreate();
        cgExpQueuePush(vQueue, item);
        FklCodegenAction *do1VAction = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                createMustHasRetvalQueueNextExpression(vQueue),
                cs,
                cms,
                env,
                CURLINE(orig->container),
                // orig->curline,
                do1Action,
                codegen);
        fklCodegenActionVectorPushBack2(actions, do1VAction);
    } else {
        FklByteCodelnt *v = fklCreateByteCodelnt(0);
        FklCodegenAction *action = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                NULL,
                cs,
                cms,
                env,
                CURLINE(orig->container),
                // orig->curline,
                do1Action,
                codegen);
        fklByteCodelntVectorPushBack2(&action->bcl_vector, v);
        fklCodegenActionVectorPushBack2(actions, action);
    }

    CgExpQueue *cQueue = cgExpQueueCreate();
    cgExpQueuePush(cQueue, cond);
    FklCodegenAction *do1CAction = create_cg_action(_default_bc_process,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(cQueue),
            cs,
            cms,
            env,
            CURLINE(orig->container),
            // orig->curline,
            do1Action,
            codegen);
    fklCodegenActionVectorPushBack2(actions, do1CAction);

    FklCodegenAction *do1InitValAction =
            create_cg_action(_do1_init_val_bc_process,
                    createDo1CodegenContext(symStack),
                    createMustHasRetvalQueueNextExpression(valueQueue),
                    scope,
                    macro_scope,
                    env,
                    CURLINE(orig->container),
                    // orig->curline,
                    do1Action,
                    codegen);
    fklCodegenActionVectorPushBack2(actions, do1InitValAction);
}

// static inline FklSid_t get_sid_in_gs_with_id_in_ps(FklSid_t id,
//         FklSymbolTable *gs,
//         FklSymbolTable *ps) {
//     return fklAddSymbol(fklGetSymbolWithId(id, ps), gs);
// }

static inline FklVMvalue *
get_sid_with_idx(FklCodegenEnvScope *sc, uint32_t idx, FklCodegenCtx *ctx) {
    for (FklSymDefHashMapNode *l = sc->defs.first; l; l = l->next) {
        if (l->v.idx == idx)
            return l->k.id;
        // get_sid_in_gs_with_id_in_ps(l->k.id, gs, ps);
    }
    return 0;
}

static inline FklVMvalue *
get_sid_with_ref_idx(FklSymDefHashMap *refs, uint32_t idx, FklCodegenCtx *ctx) {
    for (FklSymDefHashMapNode *l = refs->first; l; l = l->next) {
        if (l->v.idx == idx)
            return l->k.id;
        // get_sid_in_gs_with_id_in_ps(l->k.id, gs, ps);
    }
    return 0;
}

static inline FklByteCodelnt *process_set_var(FklByteCodelntVector *stack,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenCtx *ctx,
        FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklVMvalue *fid,
        uint32_t line) {
    if (stack->size >= 2) {
        FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(stack);
        FklByteCodelnt *popVar = *fklByteCodelntVectorPopBackNonNull(stack);
        const FklInstruction *cur_ins = &cur->bc.code[0];
        if (fklIsPushProcIns(cur_ins)) {
            const FklInstruction *popVar_ins = &popVar->bc.code[0];
            if (fklIsPutLocIns(popVar_ins)) {
                FklInstructionArg arg;
                fklGetInsOpArg(cur_ins, &arg);
                uint64_t prototypeId = arg.ux;

                fklGetInsOpArg(popVar_ins, &arg);
                uint64_t idx = arg.ux;

                if (!codegen->pts->p.pa[prototypeId].name)
                    codegen->pts->p.pa[prototypeId].name =
                            get_sid_with_idx(&env->scopes[scope - 1], idx, ctx);
            } else if (fklIsPutVarRefIns(popVar_ins)) {
                FklInstructionArg arg;
                fklGetInsOpArg(cur_ins, &arg);
                uint64_t prototypeId = arg.ux;

                fklGetInsOpArg(popVar_ins, &arg);
                uint64_t idx = arg.ux;
                if (!codegen->pts->p.pa[prototypeId].name)
                    codegen->pts->p.pa[prototypeId].name =
                            get_sid_with_ref_idx(&env->refs, idx, ctx);
            }
        }
        fklCodeLntReverseConcat(cur, popVar);
        fklDestroyByteCodelnt(cur);
        return popVar;
    } else {
        FklByteCodelnt *popVar = *fklByteCodelntVectorPopBackNonNull(stack);
        FklInstruction pushNil = create_op_ins(FKL_OP_PUSH_NIL);
        fklByteCodeLntInsertFrontIns(&pushNil, popVar, fid, line, scope);
        return popVar;
    }
}

typedef struct {
    FklVMvalue *id;
    const FklVMvalue *container;
    uint32_t scope;
    uint32_t line;
} DefineVarContext;

static const FklCodegenActionContextMethodTable DefineVarContextMethodTable = {
    .size = sizeof(DefineVarContext),
};

static inline FklCodegenActionContext *
create_def_var_context(const FklPmatchRes *id, uint32_t scope, uint32_t line) {
    FklCodegenActionContext *r =
            createCodegenActionContext(&DefineVarContextMethodTable);
    DefineVarContext *ctx = FKL_TYPE_CAST(DefineVarContext *, r->d);
    ctx->id = id->value;
    ctx->container = id->container;
    ctx->scope = scope;
    ctx->line = line;
    return r;
}

BC_PROCESS(_def_var_exp_bc_process) {
    DefineVarContext *var_ctx = FKL_TYPE_CAST(DefineVarContext *, data);
    uint32_t idx = fklAddCodegenDefBySid(var_ctx->id, var_ctx->scope, env)->idx;
    fklResolveCodegenPreDef(var_ctx->id, var_ctx->scope, env, &codegen->pts->p);
    fklByteCodelntVectorInsertFront2(bcl_vec,
            append_put_loc_ins(INS_APPEND_BACK,
                    NULL,
                    idx,
                    codegen->fid,
                    var_ctx->line,
                    scope));
    return process_set_var(bcl_vec, codegen, ctx, env, scope, fid, line);
}

BC_PROCESS(_set_var_exp_bc_process) {
    return process_set_var(bcl_vec, codegen, ctx, env, scope, fid, line);
}

BC_PROCESS(_lambda_exp_bc_process) {
    FklByteCodelnt *retval = NULL;
    FklInstruction ret = create_op_ins(FKL_OP_RET);
    if (bcl_vec->size > 1) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        retval = fklCreateByteCodelnt(0);
        size_t top = bcl_vec->size;
        for (size_t i = 1; i < top; i++) {
            FklByteCodelnt *cur = bcl_vec->base[i];
            if (cur->bc.len) {
                fklCodeLntConcat(retval, cur);
                if (i < top - 1)
                    fklByteCodeLntPushBackIns(retval, &drop, fid, line, scope);
            }
            fklDestroyByteCodelnt(cur);
        }
        fklByteCodeLntPushBackIns(retval, &ret, fid, line, scope);
    } else {
        retval = fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
        fklByteCodeLntPushBackIns(retval, &ret, fid, line, scope);
    }
    fklCodeLntReverseConcat(bcl_vec->base[0], retval);
    fklDestroyByteCodelnt(bcl_vec->base[0]);
    bcl_vec->size = 0;
    fklScanAndSetTailCall(&retval->bc);
    FklFuncPrototypes *pts = &codegen->pts->p;
    fklUpdatePrototype(pts, env);
    append_push_proc_ins(INS_APPEND_FRONT,
            retval,
            env->prototypeId,
            retval->bc.len,
            fid,
            line,
            scope);
    return retval;
}

static inline FklByteCodelnt *processArgs(FklVMvalue *args,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen) {
    FklByteCodelnt *retval = fklCreateByteCodelnt(0);

    uint32_t arg_count = 0;
    for (; FKL_IS_PAIR(args); args = FKL_VM_CDR(args)) {
        FklVMvalue *cur = FKL_VM_CAR(args);
        if (!FKL_IS_SYM(cur)) {
            fklDestroyByteCodelnt(retval);
            return NULL;
        }
        if (fklIsSymbolDefined(cur, 1, env)) {
            fklDestroyByteCodelnt(retval);
            return NULL;
        }
        if (arg_count > UINT16_MAX) {
            fklDestroyByteCodelnt(retval);
            return NULL;
        }
        fklAddCodegenDefBySid(cur, 1, env);
        ++arg_count;
    }
    if (args != FKL_VM_NIL && !FKL_IS_SYM(args)) {
        fklDestroyByteCodelnt(retval);
        return NULL;
    }

    int rest_list = 0;
    if (FKL_IS_SYM(args)) {
        if (fklIsSymbolDefined(args, 1, env)) {
            fklDestroyByteCodelnt(retval);
            return NULL;
        }
        rest_list = 1;
        fklAddCodegenDefBySid(args, 1, env);
    }
    FklInstruction check_arg = {
        .op = FKL_OP_CHECK_ARG,
        .ai = rest_list,
        .bu = arg_count,
    };
    fklByteCodeLntPushBackIns(retval,
            &check_arg,
            codegen->fid,
            CURLINE(args),
            1);
    return retval;
}

static inline FklByteCodelnt *processArgsInStack(FklVMvalueVector *stack,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen,
        uint64_t curline) {
    FklByteCodelnt *retval = fklCreateByteCodelnt(0);
    uint32_t top = stack->size;
    FklVMvalue **base = stack->base;
    for (uint32_t i = 0; i < top; i++) {
        FklVMvalue *curId = base[i];

        fklAddCodegenDefBySid(curId, 1, env);
    }
    FklInstruction check_arg = { .op = FKL_OP_CHECK_ARG, .ai = 0, .bu = top };
    fklByteCodeLntPushBackIns(retval, &check_arg, codegen->fid, curline, 1);
    return retval;
}

BC_PROCESS(_named_let_set_var_exp_bc_process) {
    FklByteCodelnt *popVar = NULL;
    if (bcl_vec->size >= 2) {
        FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
        popVar = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
        fklCodeLntReverseConcat(cur, popVar);
        fklDestroyByteCodelnt(cur);
    } else {
        FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
        popVar = cur;
        FklInstruction pushNil = create_op_ins(FKL_OP_PUSH_NIL);
        fklByteCodeLntInsertFrontIns(&pushNil, popVar, fid, line, scope);
    }
    check_and_close_ref(popVar, scope, env, &codegen->pts->p, fid, line);
    return popVar;
}

static CODEGEN_FUNC(codegen_named_let0) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_arg0);
    if (!FKL_IS_SYM(name->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_funcall_exp_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            // name->curline,
            codegen,
            actions);

    // FklVMobarray *pst = &ctx->public_st;
    uint32_t idx = fklAddCodegenDefBySid(name->value, cs, env)->idx;

    fklReplacementHashMapAdd2(cms->replacements,
            add_symbol_cstr(ctx, "*func*"),
            // fklAddSymbolCstr("*func*", pst),
            name->value);

    FklCodegenAction *action =
            create_cg_action(_named_let_set_var_exp_bc_process,
                    createDefaultStackContext(),
                    NULL,
                    cs,
                    cms,
                    env,
                    CURLINE(orig->container),
                    // name->curline,
                    NULL,
                    codegen);
    fklByteCodelntVectorPushBack2(&action->bcl_vector,
            append_put_loc_ins(INS_APPEND_BACK,
                    NULL,
                    idx,
                    codegen->fid,
                    CURLINE(orig->container),
                    // name->curline,
                    scope));
    fklCodegenActionVectorPushBack2(actions, action);

    FklVMvalueCodegenEnv *lambdaCodegenEnv =
            fklCreateVMvalueCodegenEnv(ctx, env, cs, cms);
    FklVMvalue *argsNode = caddr(orig->value);
    FklByteCodelnt *argBc = processArgs(argsNode, lambdaCodegenEnv, codegen);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);

    create_and_insert_to_pool(codegen,
            env->prototypeId,
            lambdaCodegenEnv,
            name->value,
            // get_sid_in_gs_with_id_in_ps(name->sym, codegen->st, pst),
            // orig->curline,
            CURLINE(orig->container));

    FklCodegenAction *action1 = create_cg_action(_lambda_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            lambdaCodegenEnv->macros,
            lambdaCodegenEnv,
            CURLINE(rest->container),
            // rest->curline,
            NULL,
            codegen);
    fklByteCodelntVectorPushBack2(&action1->bcl_vector, argBc);
    fklCodegenActionVectorPushBack2(actions, action1);
}

static CODEGEN_FUNC(codegen_named_let1) {
    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_arg0);
    if (!FKL_IS_SYM(name->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }
    FklVMvalueVector *symStack = fklVMvalueVectorCreate(8);
    const FklPmatchRes *first =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    if (!FKL_IS_SYM(first->value)) {
        fklVMvalueVectorDestroy(symStack);
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }
    const FklPmatchRes *args =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);

    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);

    FklVMvalueCodegenEnv *lambdaCodegenEnv =
            fklCreateVMvalueCodegenEnv(ctx, env, cs, cms);
    fklAddCodegenDefBySid(first->value, 1, lambdaCodegenEnv);
    fklVMvalueVectorPushBack2(symStack, first->value);

    if (!is_valid_let_args(args->value,
                lambdaCodegenEnv,
                1,
                symStack,
                builtin_pattern_node)) {
        fklVMvalueVectorDestroy(symStack);
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }

    CgExpQueue *valueQueue = cgExpQueueCreate();

    cgExpQueuePush(valueQueue, value);
    for (FklVMvalue *cur = args->value; FKL_IS_PAIR(cur); cur = FKL_VM_CDR(cur))
        cgExpQueuePush2(valueQueue,
                (FklPmatchRes){
                    .value = cadr(FKL_VM_CAR(cur)),
                    .container = cdr(FKL_VM_CAR(cur)),
                });

    FklCodegenAction *funcallAction = create_cg_action(_funcall_exp_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            // name->curline,
            NULL,
            codegen);
    fklCodegenActionVectorPushBack2(actions, funcallAction);

    uint32_t idx = fklAddCodegenDefBySid(name->value, cs, env)->idx;

    // FklSymbolTable *pst = &ctx->public_st;

    fklReplacementHashMapAdd2(cms->replacements,
            add_symbol_cstr(ctx, "*func*"),
            // fklAddSymbolCstr("*func*", pst),
            name->value);
    fklCodegenActionVectorPushBack2(actions,
            create_cg_action(_let_arg_exp_bc_process,
                    createDefaultStackContext(),
                    createMustHasRetvalQueueNextExpression(valueQueue),
                    scope,
                    macro_scope,
                    env,
                    CURLINE(first->container),
                    // firstSymbol->curline,
                    funcallAction,
                    codegen));

    FklCodegenAction *action =
            create_cg_action(_named_let_set_var_exp_bc_process,
                    createDefaultStackContext(),
                    NULL,
                    cs,
                    cms,
                    env,
                    CURLINE(orig->container),
                    // name->curline,
                    funcallAction,
                    codegen);
    fklByteCodelntVectorPushBack2(&action->bcl_vector,
            append_put_loc_ins(INS_APPEND_BACK,
                    NULL,
                    idx,
                    codegen->fid,
                    CURLINE(orig->container),
                    // name->curline,
                    scope));

    fklCodegenActionVectorPushBack2(actions, action);

    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);

    FklByteCodelnt *argBc = processArgsInStack(symStack,
            lambdaCodegenEnv,
            codegen,
            CURLINE(orig->container)
            // firstSymbol->curline
    );

    fklVMvalueVectorDestroy(symStack);
    create_and_insert_to_pool(codegen,
            env->prototypeId,
            lambdaCodegenEnv,
            name->value,
            // get_sid_in_gs_with_id_in_ps(name->sym, codegen->st, pst),
            // orig->curline,
            CURLINE(orig->container));

    FklCodegenAction *action1 = create_cg_action(_lambda_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            lambdaCodegenEnv->macros,
            lambdaCodegenEnv,
            CURLINE(rest->value),
            // rest->curline,
            NULL,
            codegen);
    fklByteCodelntVectorPushBack2(&action1->bcl_vector, argBc);

    fklCodegenActionVectorPushBack2(actions, action1);
}

BC_PROCESS(_and_exp_bc_process) {
    if (bcl_vec->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *retval = fklCreateByteCodelnt(0);
        while (!fklByteCodelntVectorIsEmpty(bcl_vec)) {
            if (retval->bc.len) {
                fklByteCodeLntInsertFrontIns(&drop, retval, fid, line, scope);
                append_jmp_ins(INS_APPEND_FRONT,
                        retval,
                        retval->bc.len,
                        JMP_IF_FALSE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
            }
            FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
            fklCodeLntReverseConcat(cur, retval);
            fklDestroyByteCodelnt(cur);
        }
        return retval;
    } else
        return append_push_i24_ins(INS_APPEND_BACK,
                NULL,
                1,
                codegen,
                fid,
                line,
                scope);
}

static CODEGEN_FUNC(codegen_and) {
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_and_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            // rest->curline,
            codegen,
            actions);
}

BC_PROCESS(_or_exp_bc_process) {
    if (bcl_vec->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *retval = fklCreateByteCodelnt(0);
        while (!fklByteCodelntVectorIsEmpty(bcl_vec)) {
            if (retval->bc.len) {
                fklByteCodeLntInsertFrontIns(&drop, retval, fid, line, scope);
                append_jmp_ins(INS_APPEND_FRONT,
                        retval,
                        retval->bc.len,
                        JMP_IF_TRUE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
            }
            FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
            fklCodeLntReverseConcat(cur, retval);
            fklDestroyByteCodelnt(cur);
        }
        return retval;
    } else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static CODEGEN_FUNC(codegen_or) {
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);
    pushListItemToQueue(rest->value, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_or_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            // rest->curline,
            codegen,
            actions);
}

int fklIsReplacementDefined(FklVMvalue *id, FklVMvalueCodegenEnv *env) {
    return fklReplacementHashMapGet2(env->macros->replacements, id) != NULL;
}

FklVMvalue *fklGetReplacement(FklVMvalue *id,
        const FklReplacementHashMap *rep) {
    FklVMvalue **pn = fklReplacementHashMapGet2(rep, id);
    return pn ? *pn : NULL;
}

static CODEGEN_FUNC(codegen_lambda) {
    const FklPmatchRes *args =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    FklVMvalueCodegenEnv *lambdaCodegenEnv =
            fklCreateVMvalueCodegenEnv(ctx, env, scope, macro_scope);
    FklByteCodelnt *argsBc =
            processArgs(args->value, lambdaCodegenEnv, codegen);
    if (!argsBc) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    create_and_insert_to_pool(codegen,
            env->prototypeId,
            lambdaCodegenEnv,
            0,
            // orig->curline,
            CURLINE(orig->container));
    FklCodegenAction *action = create_cg_action(_lambda_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            lambdaCodegenEnv->macros,
            lambdaCodegenEnv,
            CURLINE(rest->container),
            // rest->curline,
            NULL,
            codegen);
    fklByteCodelntVectorPushBack2(&action->bcl_vector, argsBc);
    fklCodegenActionVectorPushBack2(actions, action);
}

static inline int
is_constant_defined(FklVMvalue *id, uint32_t scope, FklVMvalueCodegenEnv *env) {
    FklSymDefHashMapElm *def =
            fklGetCodegenDefByIdInScope(id, scope, env->scopes);
    return def && def->v.isConst;
}

static inline int
is_variable_defined(FklVMvalue *id, uint32_t scope, FklVMvalueCodegenEnv *env) {
    FklSymDefHashMapElm *def =
            fklGetCodegenDefByIdInScope(id, scope, env->scopes);
    return def != NULL;
}

static CODEGEN_FUNC(codegen_define) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    if (!FKL_IS_SYM(name->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->type = FKL_ERR_ASSIGN_CONSTANT;
        error_state->p = PLACE(name->value, CURLINE(name->container));
        // error_state->place = name->value;
        return;
    }
    if (!is_variable_defined(name->value, scope, env))
        fklAddCodegenPreDefBySid(name->value, scope, 0, env);

    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, value);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_def_var_exp_bc_process,
            create_def_var_context(name, scope, CURLINE(orig->container)),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            // name->curline,
            codegen,
            actions);
}

static inline FklUnReSymbolRef *get_resolvable_assign_ref(FklVMvalue *id,
        uint32_t scope,
        FklVMvalueCodegenEnv *env) {
    FklUnReSymbolRef *urefs = env->uref.base;
    uint32_t top = env->uref.size;
    for (uint32_t i = 0; i < top; i++) {
        FklUnReSymbolRef *cur = &urefs[i];
        if (cur->id == id && cur->scope == scope && cur->assign)
            return cur;
    }
    return NULL;
}

BC_PROCESS(_def_const_exp_bc_process) {
    DefineVarContext *var_ctx = FKL_TYPE_CAST(DefineVarContext *, data);
    FklUnReSymbolRef *assign_uref =
            get_resolvable_assign_ref(var_ctx->id, var_ctx->scope, env);
    if (assign_uref) {
        error_state->p = PLACE(var_ctx->id, CURLINE(var_ctx->container));
        // FklNastNode *place = fklCreateNastNode(FKL_NAST_SYM,
        // assign_uref->line); place->sym = var_ctx->id; error_state->type =
        // FKL_ERR_ASSIGN_CONSTANT; error_state->line = assign_uref->line;
        // error_state->place = place;
        // error_state->place = var_ctx->id;
        // const FklString *sym =
        //         fklGetSymbolWithId(assign_uref->fid, codegen->st);

        error_state->fid = assign_uref->fid;
        // assign_uref->fid ? fklAddSymbol(sym, &ctx->public_st) : NULL;
        return NULL;
    }
    FklSymDef *def = fklAddCodegenDefBySid(var_ctx->id, var_ctx->scope, env);
    def->isConst = 1;
    uint32_t idx = def->idx;
    fklResolveCodegenPreDef(var_ctx->id, var_ctx->scope, env, &codegen->pts->p);
    fklByteCodelntVectorInsertFront2(bcl_vec,
            append_put_loc_ins(INS_APPEND_BACK,
                    NULL,
                    idx,
                    codegen->fid,
                    var_ctx->line,
                    var_ctx->scope));
    return process_set_var(bcl_vec, codegen, ctx, env, scope, fid, line);
}

static CODEGEN_FUNC(codegen_defconst) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    if (!FKL_IS_SYM(name->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->type = FKL_ERR_ASSIGN_CONSTANT;
        error_state->p = PLACE(name->value, CURLINE(name->container));
        // error_state->place = name->value;
        return;
    }
    if (is_variable_defined(name->value, scope, env)) {
        error_state->type = FKL_ERR_REDEFINE_VARIABLE_AS_CONSTANT;
        error_state->p = PLACE(name->value, CURLINE(name->container));
        // error_state->place = name->value;
        return;
    } else
        fklAddCodegenPreDefBySid(name->value, scope, 1, env);

    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, value);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_def_const_exp_bc_process,
            create_def_var_context(name, scope, CURLINE(orig->container)),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            // name->curline,
            codegen,
            actions);
}

static CODEGEN_FUNC(codegen_defun) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *args =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    if (!FKL_IS_SYM(name->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->type = FKL_ERR_ASSIGN_CONSTANT;
        error_state->p = PLACE(name->value, CURLINE(name->container));
        // error_state->place = name->value;
        return;
    }

    FklVMvalueCodegenEnv *lambdaCodegenEnv =
            fklCreateVMvalueCodegenEnv(ctx, env, scope, macro_scope);
    FklByteCodelnt *argsBc =
            processArgs(args->value, lambdaCodegenEnv, codegen);
    if (!argsBc) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }

    if (!is_variable_defined(name->value, scope, env))
        fklAddCodegenPreDefBySid(name->value, scope, 0, env);

    FklCodegenAction *prevAction = create_cg_action(_def_var_exp_bc_process,
            create_def_var_context(name, scope, CURLINE(orig->container)),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            // name->curline,
            NULL,
            codegen);
    fklCodegenActionVectorPushBack2(actions, prevAction);

    // FklSymbolTable *pst = &ctx->public_st;
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    create_and_insert_to_pool(codegen,
            env->prototypeId,
            lambdaCodegenEnv,
            name->value,
            // get_sid_in_gs_with_id_in_ps(name->sym, codegen->st, pst),
            // orig->curline,
            CURLINE(orig->container));
    // FklNastNode *name_str = fklCreateNastNode(FKL_NAST_STR, name->curline);
    // name_str->str = fklCopyString(fklGetSymbolWithId(name->sym, pst));

    // FklVMvalue* name_str =fklCreateVMvalueStr(&ctx->gc->gcvm,
    // FKL_VM_SYM(name));
    fklReplacementHashMapAdd2(lambdaCodegenEnv->macros->replacements,
            add_symbol_cstr(ctx, "*func*"),
            // fklAddSymbolCstr("*func*", pst),
            name->value);
    // fklDestroyNastNode(name_str);
    FklCodegenAction *cur = create_cg_action(_lambda_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            lambdaCodegenEnv->macros,
            lambdaCodegenEnv,
            CURLINE(rest->container),
            // rest->curline,
            prevAction,
            codegen);
    fklByteCodelntVectorPushBack2(&cur->bcl_vector, argsBc);
    fklCodegenActionVectorPushBack2(actions, cur);
}

static CODEGEN_FUNC(codegen_defun_const) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *args =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    if (!FKL_IS_SYM(name->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->type = FKL_ERR_ASSIGN_CONSTANT;
        error_state->p = PLACE(name->value, CURLINE(name->container));
        // error_state->place = name->value;
        return;
    }
    if (is_variable_defined(name->value, scope, env)) {
        error_state->type = FKL_ERR_REDEFINE_VARIABLE_AS_CONSTANT;
        error_state->p = PLACE(name->value, CURLINE(name->container));
        // error_state->place = name->value;
        return;
    }

    FklVMvalueCodegenEnv *lambdaCodegenEnv =
            fklCreateVMvalueCodegenEnv(ctx, env, scope, macro_scope);
    FklByteCodelnt *argsBc =
            processArgs(args->value, lambdaCodegenEnv, codegen);
    if (!argsBc) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        // error_state->place = orig->value;
        return;
    }

    FklCodegenAction *prevAction = create_cg_action(_def_const_exp_bc_process,
            create_def_var_context(name, scope, CURLINE(orig->container)),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            // name->curline,
            NULL,
            codegen);
    fklCodegenActionVectorPushBack2(actions, prevAction);

    // FklSymbolTable *pst = &ctx->public_st;
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    create_and_insert_to_pool(codegen,
            env->prototypeId,
            lambdaCodegenEnv,
            name->value,
            // get_sid_in_gs_with_id_in_ps(name->sym, codegen->st, pst),
            // orig->curline,
            CURLINE(orig->container));
    // FklNastNode *name_str = fklCreateNastNode(FKL_NAST_STR, name->curline);
    // name_str->str = fklCopyString(fklGetSymbolWithId(name->sym, pst));
    fklReplacementHashMapAdd2(lambdaCodegenEnv->macros->replacements,
            add_symbol_cstr(ctx, "*func*"),
            // fklAddSymbolCstr("*func*", pst),
            name->value);
    // fklDestroyNastNode(name_str);
    FklCodegenAction *cur = create_cg_action(_lambda_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            lambdaCodegenEnv->macros,
            lambdaCodegenEnv,
            CURLINE(rest->container),
            // rest->curline,
            prevAction,
            codegen);
    fklByteCodelntVectorPushBack2(&cur->bcl_vector, argsBc);
    fklCodegenActionVectorPushBack2(actions, cur);
}

static CODEGEN_FUNC(codegen_setq) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    if (!FKL_IS_SYM(name->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        return;
    }
    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, value);
    FklSymDefHashMapElm *def =
            fklFindSymbolDefByIdAndScope(name->value, scope, env->scopes);
    FklCodegenAction *cur = create_cg_action(_set_var_exp_bc_process,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            // name->curline,
            NULL,
            codegen);
    if (def) {
        if (def->v.isConst) {
            error_state->type = FKL_ERR_ASSIGN_CONSTANT;
            // error_state->place = name->value;
            error_state->p = PLACE(name->value, CURLINE(name->container));
            return;
        }
        fklByteCodelntVectorPushBack2(&cur->bcl_vector,
                append_put_loc_ins(INS_APPEND_BACK,
                        NULL,
                        def->v.idx,
                        codegen->fid,
                        CURLINE(orig->container),
                        // name->curline,
                        scope));
    } else {
        FklSymDefHashMapElm *def = fklAddCodegenRefBySid(name->value,
                env,
                codegen->fid,
                CURLINE(orig->container),
                // name->curline,
                1);
        if (def->v.isConst) {
            error_state->type = FKL_ERR_ASSIGN_CONSTANT;
            error_state->p = PLACE(name->value, CURLINE(name->container));
            // error_state->place = name->value;
            return;
        }
        fklByteCodelntVectorPushBack2(&cur->bcl_vector,
                append_put_var_ref_ins(INS_APPEND_BACK,
                        NULL,
                        def->v.idx,
                        codegen->fid,
                        CURLINE(orig->container),
                        // name->curline,
                        scope));
    }
    fklCodegenActionVectorPushBack2(actions, cur);
}

static inline void push_default_codegen_quest(const FklPmatchRes *value,
        FklCodegenActionVector *actions,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        FklVMvalueCodegenEnv *env,
        FklCodegenAction *prev,
        FklVMvalueCodegenInfo *codegen) {
    FklCodegenAction *cur = create_cg_action(_default_bc_process,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(value->container),
            // value->curline,
            prev,
            codegen);
    fklByteCodelntVectorPushBack2(&cur->bcl_vector,
            gen_push_literal_code(value, codegen, env, scope));
    fklCodegenActionVectorPushBack2(actions, cur);
}

static CODEGEN_FUNC(codegen_macroexpand) {
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    // fklMakeNastNodeRef(value);
    FklVMvalue *next_value = value->value;
    if (FKL_IS_PAIR(value->value))
        next_value = fklTryExpandCodegenMacro(value,
                codegen,
                env->macros,
                error_state);
    if (error_state->type)
        return;
    push_default_codegen_quest(
            &(FklPmatchRes){
                .value = next_value,
                .container = value->container,
            },
            actions,
            scope,
            macro_scope,
            env,
            NULL,
            codegen);
    // fklDestroyNastNode(value);
}

static void add_compiler_macro(FklCodegenMacro **pmacro,
        FklVMvalue *pattern,
        // FklVMvalue *origin_exp,
        const FklByteCodelnt *bcl,
        uint32_t prototype_id) {
    int coverState = FKL_PATTERN_NOT_EQUAL;
    FklCodegenMacro **phead = pmacro;
    for (FklCodegenMacro *cur = *pmacro; cur;
            pmacro = &cur->next, cur = cur->next) {
        coverState = fklPatternCoverState(cur->pattern, pattern);
        if (coverState == FKL_PATTERN_BE_COVER)
            phead = &cur->next;
        else if (coverState)
            break;
    }
    if (coverState == FKL_PATTERN_NOT_EQUAL) {
        FklCodegenMacro *macro =
                fklCreateCodegenMacro(pattern, bcl, *phead, prototype_id);
        *phead = macro;
    } else if (coverState == FKL_PATTERN_EQUAL) {
        FklCodegenMacro *macro = *pmacro;
        uninit_codegen_macro(macro);
        macro->prototype_id = prototype_id;
        macro->pattern = pattern;
        macro->bcl = fklCopyByteCodelnt(bcl);
    } else {
        FklCodegenMacro *next = *pmacro;
        *pmacro = fklCreateCodegenMacro(pattern, bcl, next, prototype_id);
    }
}

BC_PROCESS(_export_macro_bc_process) {
    FklVMvalueCodegenMacroScope *target_macro_scope = cms->prev;
    for (; codegen && !codegen->is_lib; codegen = codegen->prev)
        ;
    for (FklCodegenMacro *cur = cms->head; cur; cur = cur->next) {
        add_compiler_macro(&codegen->export_macro,
                cur->pattern,
                // cur->origin_exp,
                cur->bcl,
                cur->prototype_id);
        add_compiler_macro(&target_macro_scope->head,
                cur->pattern,
                // cur->origin_exp,
                cur->bcl,
                cur->prototype_id);
    }
    for (FklReplacementHashMapNode *cur = cms->replacements->first; cur;
            cur = cur->next) {
        fklReplacementHashMapAdd2(codegen->export_replacement, cur->k, cur->v);
        fklReplacementHashMapAdd2(target_macro_scope->replacements,
                cur->k,
                cur->v);
    }
    return NULL;
}

static inline void add_export_symbol(FklVMvalueCodegenInfo *codegen,
        FklVMvalue *orig,
        FklVMvalue *rest,
        CgExpQueue *exportQueue) {
    // FklNastPair *prevPair = orig->pair;
    // FklNastNode *exportHead = orig->pair->car;
    FklVMvalue *prev = orig;
    FklVMvalue *head = FKL_VM_CAR(orig);
    for (; FKL_IS_PAIR(rest); rest = FKL_VM_CDR(rest)) {
        // FklNastNode *restExp = fklNastCons(fklMakeNastNodeRef(exportHead),
        //         rest,
        //         rest->curline);
        // fklNastNodeQueuePush2(exportQueue, restExp);
        // prevPair->cdr = fklCreateNastNode(FKL_NAST_NIL, rest->curline);
        // prevPair = rest->pair;
        FklVMvalue *new_rest =
                fklCreateVMvaluePair(&codegen->ctx->gc->gcvm, head, rest);
        put_line_number(&codegen->ctx->lnt->ht, new_rest, CURLINE(rest));
        cgExpQueuePush2(exportQueue,
                (FklPmatchRes){ .value = new_rest, .container = new_rest });
        FKL_VM_CDR(prev) = FKL_VM_NIL;
        prev = rest;
    }
}

static inline void push_single_bcl_codegen_quest(FklByteCodelnt *bcl,
        FklCodegenActionVector *actions,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        FklVMvalueCodegenEnv *env,
        FklCodegenAction *prev,
        FklVMvalueCodegenInfo *codegen,
        uint64_t curline) {
    FklCodegenAction *quest = create_cg_action(_default_bc_process,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            curline,
            prev,
            codegen);
    fklByteCodelntVectorPushBack2(&quest->bcl_vector, bcl);
    fklCodegenActionVectorPushBack2(actions, quest);
}

typedef FklVMvalue *(*ReplacementFunc)(const FklPmatchRes *orig,
        const FklVMvalueCodegenEnv *env,
        const FklVMvalueCodegenInfo *codegen);

static inline ReplacementFunc findBuiltInReplacementWithId(FklVMvalue *id,
        FklVMvalue *const builtin_replacement_id[]);

static inline int is_replacement_define(FklVMvalue *value,
        const FklVMvalueCodegenInfo *info,
        const FklCodegenCtx *ctx,
        const FklVMvalueCodegenMacroScope *macro_scope,
        const FklVMvalueCodegenEnv *env) {
    FklVMvalue *replacement = NULL;
    ReplacementFunc f = NULL;
    for (const FklVMvalueCodegenMacroScope *cs = macro_scope;
            cs && !replacement;
            cs = cs->prev)
        replacement = fklGetReplacement(value, cs->replacements);
    if (replacement) {
        return 1;
    } else if ((f = findBuiltInReplacementWithId(value,
                        ctx->builtin_replacement_id)))
        return 1;
    else
        return 0;
}

static inline int is_replacement_true(const FklPmatchRes *value,
        const FklVMvalueCodegenInfo *info,
        const FklCodegenCtx *ctx,
        const FklVMvalueCodegenMacroScope *macro_scope,
        const FklVMvalueCodegenEnv *env) {
    FklVMvalue *replacement = NULL;
    ReplacementFunc f = NULL;
    for (const FklVMvalueCodegenMacroScope *cs = macro_scope;
            cs && !replacement;
            cs = cs->prev)
        replacement = fklGetReplacement(value->value, cs->replacements);
    if (replacement) {
        int r = replacement != FKL_VM_NIL;
        return r;
    } else if ((f = findBuiltInReplacementWithId(value->value,
                        ctx->builtin_replacement_id))) {
        FklVMvalue *t = f(value, env, info);
        int r = t != FKL_VM_NIL;
        // fklDestroyNastNode(t);
        return r;
    } else
        return 0;
}

static inline FklVMvalue *get_replacement(const FklPmatchRes *value,
        const FklVMvalueCodegenInfo *info,
        const FklCodegenCtx *ctx,
        const FklVMvalueCodegenMacroScope *macro_scope,
        const FklVMvalueCodegenEnv *env) {
    FklVMvalue *replacement = NULL;
    ReplacementFunc f = NULL;
    for (const FklVMvalueCodegenMacroScope *cs = macro_scope;
            cs && !replacement;
            cs = cs->prev)
        replacement = fklGetReplacement(value->value, cs->replacements);
    if (replacement)
        return replacement;
    else if ((f = findBuiltInReplacementWithId(value->value,
                      ctx->builtin_replacement_id)))
        return f(value, env, info);
    else
        return NULL;
}

static inline char *combineFileNameFromListAndCheckPrivate(
        const FklVMvalue *list);

static inline int is_valid_compile_check_pattern(const FklVMvalue *p) {
    return p != FKL_VM_NIL            //
        && FKL_IS_PAIR(p)             //
        && FKL_IS_SYM(FKL_VM_CAR(p))  //
        && FKL_IS_PAIR(FKL_VM_CDR(p)) //
        && FKL_VM_CDR(FKL_VM_CDR(p)) == FKL_VM_NIL;
}

static inline int is_compile_check_pattern_slot_node(FklVMvalue *slot_id,
        const FklVMvalue *n) {
    return n != FKL_VM_NIL            //
        && FKL_IS_PAIR(n)             //
        && FKL_IS_SYM(FKL_VM_CAR(n))  //
        && FKL_VM_CAR(n) == slot_id   //
        && FKL_IS_PAIR(FKL_VM_CDR(n)) //
        && FKL_VM_CDR(FKL_VM_CDR(n)) == FKL_VM_NIL;
}

static inline int is_compile_check_pattern_matched(FklVMvalue *p,
        FklVMvalue *e) {
    // FklNastImmPairVector s;
    // fklNastImmPairVectorInit(&s, 8);
    // fklNastImmPairVectorPushBack2(&s,
    //         (FklNastImmPair){ .car = cadr_nast_node(p), .cdr = e });
    FklVMpairVector s;
    fklVMpairVectorInit(&s, 8);
    fklVMpairVectorPushBack2(&s, (FklVMpair){ .car = cadr(p), .cdr = e });

    FklVMvalue *slot_id = FKL_VM_CAR(p);
    int r = 1;
    while (!fklVMpairVectorIsEmpty(&s)) {
        const FklVMpair *top = fklVMpairVectorPopBackNonNull(&s);
        const FklVMvalue *p = top->car;
        const FklVMvalue *e = top->cdr;
        if (is_compile_check_pattern_slot_node(slot_id, p))
            continue;
        if (FKL_GET_TAG(p) != FKL_GET_TAG(e)) {
            r = 0;
            goto exit;
        }

        if (FKL_IS_PTR(p)) {
            switch (p->type) {
            case FKL_TYPE_BOX:
                fklVMpairVectorPushBack2(&s,
                        (FklVMpair){
                            .car = FKL_VM_BOX(p),
                            .cdr = FKL_VM_BOX(e),
                        });
                break;
            case FKL_TYPE_PAIR:
                fklVMpairVectorPushBack2(&s,
                        (FklVMpair){
                            .car = FKL_VM_CAR(p),
                            .cdr = FKL_VM_CAR(e),
                        });
                fklVMpairVectorPushBack2(&s,
                        (FklVMpair){
                            .car = FKL_VM_CDR(p),
                            .cdr = FKL_VM_CDR(e),
                        });
                break;
            case FKL_TYPE_VECTOR:
                if (FKL_VM_VEC(p)->size != FKL_VM_VEC(e)->size) {
                    r = 0;
                    goto exit;
                }
                for (size_t i = 0; i < FKL_VM_VEC(p)->size; i++) {
                    fklVMpairVectorPushBack2(&s,
                            (FklVMpair){
                                .car = FKL_VM_VEC(p)->base[i],
                                .cdr = FKL_VM_VEC(e)->base[i],
                            });
                }
                break;
            case FKL_TYPE_HASHTABLE:
                if (FKL_VM_HASH(p)->eq_type != FKL_VM_HASH(e)->eq_type) {
                    r = 0;
                    goto exit;
                }

                if (FKL_VM_HASH(p)->ht.count != FKL_VM_HASH(e)->ht.count) {
                    r = 0;
                    goto exit;
                }

                FklVMvalueHashMapNode *a = FKL_VM_HASH(p)->ht.first;
                FklVMvalueHashMapNode *b = FKL_VM_HASH(e)->ht.first;
                for (; a; a = a->next, b = b->next) {
                    fklVMpairVectorPushBack2(&s,
                            (FklVMpair){
                                .car = a->k,
                                .cdr = b->k,
                            });

                    fklVMpairVectorPushBack2(&s,
                            (FklVMpair){
                                .car = a->v,
                                .cdr = b->v,
                            });
                }
                break;
            default:
                if (!fklVMvalueEqual(p, e)) {
                    r = 0;
                    goto exit;
                }
            }
        } else if (p != e) {
            r = 0;
            goto exit;
        }
    }

exit:
    fklVMpairVectorUninit(&s);
    return r;
}

typedef struct CfgCtx {
    FklVMvalue *(*method)(struct CfgCtx *ctx, int r);
    const FklVMvalue *rest;
    int r;
} CfgCtx;

// CgCfgCtxVector
#define FKL_VECTOR_TYPE_PREFIX Cg
#define FKL_VECTOR_METHOD_PREFIX cg
#define FKL_VECTOR_ELM_TYPE CfgCtx
#define FKL_VECTOR_ELM_TYPE_NAME CfgCtx
#include <fakeLisp/cont/vector.h>

static inline int cfg_check_defined(const FklVMvalueCodegenInfo *codegen,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCodegenCtx *ctx,
        const FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklCodegenErrorState *error_state) {
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    if (!FKL_IS_SYM(value->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = exp;
        error_state->p = PLACE(exp->value, CURLINE(exp->container));
        return 0;
    }
    return fklFindSymbolDefByIdAndScope(value->value, scope, env->scopes)
        != NULL;
}

static inline int cfg_check_importable(const FklVMvalueCodegenInfo *codegen,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCodegenCtx *ctx,
        const FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklCodegenErrorState *error_state) {
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    if (value->value == FKL_VM_NIL || !is_symbol_list(value->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = exp;
        error_state->p = PLACE(exp->value, CURLINE(exp->container));
        // fklMakeNastNodeRef(FKL_TYPE_CAST(FklNastNode *, exp));
        return 0;
    }
    char *filename = combineFileNameFromListAndCheckPrivate(value->value);

    if (filename) {
        char *packageMainFileName =
                fklStrCat(fklZstrdup(filename), FKL_PATH_SEPARATOR_STR);
        packageMainFileName =
                fklStrCat(packageMainFileName, FKL_PACKAGE_MAIN_FILE);

        char *preCompileFileName = fklStrCat(fklZstrdup(packageMainFileName),
                FKL_PRE_COMPILE_FKL_SUFFIX_STR);

        char *scriptFileName =
                fklStrCat(fklZstrdup(filename), FKL_SCRIPT_FILE_EXTENSION);

        char *dllFileName = fklStrCat(fklZstrdup(filename), FKL_DLL_FILE_TYPE);

        int r = fklIsAccessibleRegFile(packageMainFileName)
             || fklIsAccessibleRegFile(scriptFileName)
             || fklIsAccessibleRegFile(preCompileFileName)
             || fklIsAccessibleRegFile(dllFileName);
        fklZfree(filename);
        fklZfree(scriptFileName);
        fklZfree(dllFileName);
        fklZfree(packageMainFileName);
        fklZfree(preCompileFileName);
        return r;
    } else
        return 0;
}

static inline int cfg_check_macro_defined(const FklVMvalueCodegenInfo *codegen,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCodegenCtx *ctx,
        const FklVMvalueCodegenEnv *env,
        uint32_t scope,
        const FklVMvalueCodegenMacroScope *macro_scope,
        const FklVMvalueCodegenInfo *info,
        FklCodegenErrorState *error_state) {
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    if (FKL_IS_SYM(value->value))
        return is_replacement_define(value->value, info, ctx, macro_scope, env);
    else if (FKL_IS_PAIR(value->value)                 //
             && FKL_VM_CDR(value->value) == FKL_VM_NIL //
             && FKL_IS_SYM(FKL_VM_CAR(value->value))) {
        int r = 0;
        FklVMvalue *id = FKL_VM_CAR(value->value);
        for (const FklVMvalueCodegenMacroScope *macros = macro_scope; macros;
                macros = macros->prev) {
            for (const FklCodegenMacro *cur = macros->head; cur;
                    cur = cur->next) {
                if (id == FKL_VM_CAR(cur->pattern)) {
                    r = 1;
                    break;
                }
            }
            if (r == 1)
                break;
        }
        return r;
    } else if (FKL_IS_VECTOR(value->value)            //
               && FKL_VM_VEC(value->value)->size == 1 //
               && FKL_IS_SYM(FKL_VM_VEC(value->value)->base[0])) {
        FklVMvalue *id = FKL_VM_VEC(value->value)->base[0];
        return *(info->g) != NULL
            && fklGraProdGroupHashMapGet2(info->named_prod_groups, id) != NULL;
    } else {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = exp;
        error_state->p = PLACE(exp->value, CURLINE(exp->container));
        // fklMakeNastNodeRef(FKL_TYPE_CAST(FklNastNode *, exp));
        return 0;
    }
}

static inline int cfg_check_eq(

        const FklVMvalueCodegenInfo *codegen,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCodegenCtx *ctx,
        const FklVMvalueCodegenEnv *env,
        const FklVMvalueCodegenMacroScope *macro_scope,
        const FklVMvalueCodegenInfo *info,
        FklCodegenErrorState *error_state) {
    const FklPmatchRes *arg0 =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_arg0);
    const FklPmatchRes *arg1 =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_arg1);
    if (!FKL_IS_SYM(arg0->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = exp;
        error_state->p = PLACE(exp->value, CURLINE(exp->container));
        // fklMakeNastNodeRef(FKL_TYPE_CAST(FklNastNode *, exp));
        return 0;
    }
    FklVMvalue *node = get_replacement(arg0, info, ctx, macro_scope, env);
    if (node) {
        int r = fklVMvalueEqual(node, arg1->value);
        // fklDestroyNastNode(node);
        return r;
    } else
        return 0;
}

static inline int cfg_check_matched(const FklVMvalueCodegenInfo *codegen,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCodegenCtx *ctx,
        const FklVMvalueCodegenEnv *env,
        const FklVMvalueCodegenMacroScope *macro_scope,
        const FklVMvalueCodegenInfo *info,
        FklCodegenErrorState *error_state) {
    const FklPmatchRes *arg0 =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_arg0);
    const FklPmatchRes *arg1 =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_arg1);
    if (!FKL_IS_SYM(arg0->value)
            || !is_valid_compile_check_pattern(arg1->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = exp;
        error_state->p = PLACE(exp->value, CURLINE(exp->container));
        // fklMakeNastNodeRef(FKL_TYPE_CAST(FklNastNode *, exp));
        return 0;
    }
    FklVMvalue *node = get_replacement(arg0, info, ctx, macro_scope, env);
    if (node) {
        int r = is_compile_check_pattern_matched(arg1->value, node);
        // fklDestroyNastNode(node);
        return r;
    } else
        return 0;
}

static FklVMvalue *cfg_check_not_method(CfgCtx *ctx, int r) {
    ctx->r = !r;
    return NULL;
}

static FklVMvalue *cfg_check_and_method(CfgCtx *ctx, int r) {
    ctx->r &= r;
    if (!ctx->r)
        return NULL;
    else if (ctx->rest == FKL_VM_NIL)
        return NULL;
    else {
        FklVMvalue *r = FKL_VM_CAR(ctx->rest);
        ctx->rest = FKL_VM_CDR(ctx->rest);
        return r;
    }
}

static FklVMvalue *cfg_check_or_method(CfgCtx *ctx, int r) {
    ctx->r |= r;
    if (ctx->r)
        return NULL;
    else if (ctx->rest == FKL_VM_NIL)
        return NULL;
    else {
        FklVMvalue *r = FKL_VM_CAR(ctx->rest);
        ctx->rest = FKL_VM_CDR(ctx->rest);
        return r;
    }
}

static inline int is_check_subpattern_true(

        const FklVMvalueCodegenInfo *codegen,
        const FklPmatchRes *exp_,
        const FklVMvalueCodegenInfo *info,
        const FklCodegenCtx *ctx,
        uint32_t scope,
        const FklVMvalueCodegenMacroScope *macro_scope,
        const FklVMvalueCodegenEnv *env,
        FklCodegenErrorState *error_state) {
    FklPmatchRes exp = *exp_;
    int r = 0;
    FklPmatchHashMap ht;
    CgCfgCtxVector s;
    CfgCtx *cfg_ctx;

    if (exp.value == FKL_VM_NIL)
        return 0;
    else if (FKL_IS_SYM(exp.value))
        return is_replacement_true(&exp, info, ctx, macro_scope, env);
    else if (FKL_IS_PAIR(exp.value))
        goto check_nested_sub_pattern;
    return 1;

    // switch (exp->type) {
    // case FKL_NAST_NIL:
    //     r = 0;
    //     break;
    // case FKL_NAST_SYM:
    //     r = is_replacement_true(exp, info, ctx, macro_scope, env);
    //     break;
    // case FKL_NAST_FIX:
    // case FKL_NAST_CHR:
    // case FKL_NAST_F64:
    // case FKL_NAST_STR:
    // case FKL_NAST_BYTEVECTOR:
    // case FKL_NAST_BIGINT:
    // case FKL_NAST_BOX:
    // case FKL_NAST_VECTOR:
    // case FKL_NAST_HASHTABLE:
    //     r = 1;
    //     break;
    // case FKL_NAST_RC_SYM:
    // case FKL_NAST_SLOT:
    //     FKL_UNREACHABLE();
    //     break;
    // case FKL_NAST_PAIR:
    //     goto check_nested_sub_pattern;
    //     break;
    // }
    // return r;

check_nested_sub_pattern:
    fklPmatchHashMapInit(&ht);
    cgCfgCtxVectorInit(&s, 8);

    for (;;) {
    loop_start:
        if (exp.value == FKL_VM_NIL)
            r = 0;
        else if (FKL_IS_SYM(exp.value))
            r = is_replacement_true(&exp, info, ctx, macro_scope, env);
        else if (FKL_IS_PAIR(exp.value)) {
            if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                        [FKL_CODEGEN_SUB_PATTERN_DEFINE],
                        exp.value,
                        &ht)) {
                r = cfg_check_defined(codegen,
                        &exp,
                        &ht,
                        ctx,
                        env,
                        scope,
                        error_state);
                if (error_state->type)
                    goto exit;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_IMPORT],
                               exp.value,
                               &ht)) {
                r = cfg_check_importable(codegen,
                        &exp,
                        &ht,
                        ctx,
                        env,
                        scope,
                        error_state);
                if (error_state->type)
                    goto exit;
            } else if (fklPatternMatch(
                               ctx->builtin_sub_pattern_node
                                       [FKL_CODEGEN_SUB_PATTERN_DEFMACRO],
                               exp.value,
                               &ht)) {
                r = cfg_check_macro_defined(codegen,
                        &exp,
                        &ht,
                        ctx,
                        env,
                        scope,
                        macro_scope,
                        info,
                        error_state);
                if (error_state->type)
                    goto exit;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_EQ],
                               exp.value,
                               &ht)) {
                r = cfg_check_eq(codegen,
                        &exp,
                        &ht,
                        ctx,
                        env,
                        macro_scope,
                        info,
                        error_state);
                if (error_state->type)
                    goto exit;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_MATCH],
                               exp.value,
                               &ht)) {
                r = cfg_check_matched(codegen,
                        &exp,
                        &ht,
                        ctx,
                        env,
                        macro_scope,
                        info,
                        error_state);
                if (error_state->type)
                    goto exit;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_NOT],
                               exp.value,
                               &ht)) {
                cfg_ctx = cgCfgCtxVectorPushBack(&s, NULL);
                cfg_ctx->r = 0;
                cfg_ctx->rest = NULL;
                cfg_ctx->method = cfg_check_not_method;
                exp = *fklPmatchHashMapGet2(&ht, ctx->builtInPatternVar_value);
                continue;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_AND],
                               exp.value,
                               &ht)) {
                const FklPmatchRes *rest =
                        fklPmatchHashMapGet2(&ht, ctx->builtInPatternVar_rest);
                if (rest->value == FKL_VM_NIL)
                    r = 1;
                else if (fklIsList(rest->value)) {
                    cfg_ctx = cgCfgCtxVectorPushBack(&s, NULL);
                    cfg_ctx->r = 1;
                    cfg_ctx->method = cfg_check_and_method;
                    exp = (FklPmatchRes){
                        .value = FKL_VM_CAR(rest->value),
                        .container = rest->value,
                    };
                    cfg_ctx->rest = FKL_VM_CDR(rest->value);
                    continue;
                } else {
                    error_state->type = FKL_ERR_SYNTAXERROR;
                    // error_state->place = exp.value;
                    error_state->p = PLACE(exp.value, CURLINE(exp.container));
                    // fklMakeNastNodeRef(FKL_TYPE_CAST(FklNastNode *, exp));
                    goto exit;
                }
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_OR],
                               exp.value,
                               &ht)) {
                const FklPmatchRes *rest =
                        fklPmatchHashMapGet2(&ht, ctx->builtInPatternVar_rest);
                if (rest->value == FKL_VM_NIL)
                    r = 0;
                else if (fklIsList(rest->value)) {
                    cfg_ctx = cgCfgCtxVectorPushBack(&s, NULL);
                    cfg_ctx->r = 0;
                    cfg_ctx->method = cfg_check_or_method;
                    exp = (FklPmatchRes){
                        .value = FKL_VM_CAR(rest->value),
                        .container = rest->value,
                    };
                    cfg_ctx->rest = FKL_VM_CDR(rest->value);
                    continue;
                } else {
                    error_state->type = FKL_ERR_SYNTAXERROR;
                    // error_state->place = exp.value;
                    error_state->p = PLACE(exp.value, CURLINE(exp.container));
                    // fklMakeNastNodeRef(FKL_TYPE_CAST(FklNastNode *, exp));
                    goto exit;
                }
            } else {
                error_state->type = FKL_ERR_SYNTAXERROR;
                // error_state->place = exp.value;
                error_state->p = PLACE(exp.value, CURLINE(exp.container));
                // fklMakeNastNodeRef(FKL_TYPE_CAST(FklNastNode *, exp));
                goto exit;
            }
        } else
            r = 1;

        while (!cgCfgCtxVectorIsEmpty(&s)) {
            cfg_ctx = cgCfgCtxVectorBack(&s);
            exp.value = cfg_ctx->method(cfg_ctx, r);
            if (exp.value)
                goto loop_start;
            else {
                r = cfg_ctx->r;
                cgCfgCtxVectorPopBack(&s);
            }
        }
        break;
    }
exit:
    cgCfgCtxVectorUninit(&s);
    fklPmatchHashMapUninit(&ht);
    return r;
}

static CODEGEN_FUNC(codegen_check) {
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    int r = is_check_subpattern_true(codegen,
            value,
            codegen,
            ctx,
            scope,
            macro_scope,
            env,
            error_state);
    if (error_state->type)
        return;
    FklByteCodelnt *bcl = NULL;
    bcl = fklCreateSingleInsBclnt(
            create_op_ins(r ? FKL_OP_PUSH_1 : FKL_OP_PUSH_NIL),
            codegen->fid,
            CURLINE(orig->container),
            // orig->curline,
            scope);
    push_single_bcl_codegen_quest(bcl,
            actions,
            scope,
            macro_scope,
            env,
            NULL,
            codegen,
            // orig->curline
            CURLINE(orig->container));
}

static CODEGEN_FUNC(codegen_cond_compile) {
    const FklPmatchRes *cond =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_cond);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    if (fklVMlistLength(rest->value) % 2 == 1) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        return;
    }
    int r = is_check_subpattern_true(codegen,
            cond,
            codegen,
            ctx,
            scope,
            macro_scope,
            env,
            error_state);
    if (error_state->type)
        return;
    if (r) {
        CgExpQueue *q = cgExpQueueCreate();
        cgExpQueuePush(q, value);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_default_bc_process,
                createDefaultStackContext(),
                must_has_retval ? createMustHasRetvalQueueNextExpression(q)
                                : createDefaultQueueNextExpression(q),
                scope,
                macro_scope,
                env,
                CURLINE(value->container),
                // value->curline,
                codegen,
                actions);
        return;
    }
    FklVMvalue *rest_value = rest->value;
    while (FKL_IS_PAIR(rest_value)) {
        FklVMvalue *cond = FKL_VM_CAR(rest_value);
        rest_value = FKL_VM_CDR(rest_value);
        FklVMvalue *value = FKL_VM_CAR(rest_value);
        rest_value = FKL_VM_CDR(rest_value);
        int r = is_check_subpattern_true(codegen,
                &(FklPmatchRes){
                    .value = cond,
                    .container = rest_value,
                },
                codegen,
                ctx,
                scope,
                macro_scope,
                env,
                error_state);
        if (error_state->type)
            return;
        if (r) {
            CgExpQueue *q = cgExpQueueCreate();
            cgExpQueuePush2(q,
                    (FklPmatchRes){ .value = value, .container = rest_value });
            FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_default_bc_process,
                    createDefaultStackContext(),
                    must_has_retval ? createMustHasRetvalQueueNextExpression(q)
                                    : createDefaultQueueNextExpression(q),
                    scope,
                    macro_scope,
                    env,
                    CURLINE(value),
                    // value->curline,
                    codegen,
                    actions);
            return;
        }
    }

    if (must_has_retval) {
        error_state->type = FKL_ERR_EXP_HAS_NO_VALUE;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        return;
    }
}

static CODEGEN_FUNC(codegen_quote) {
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    push_default_codegen_quest(value,
            actions,
            scope,
            macro_scope,
            env,
            NULL,
            codegen);
}

static inline void unquoteHelperFunc(const FklPmatchRes *value,
        FklCodegenActionVector *actions,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        FklVMvalueCodegenEnv *env,
        FklCodegenActionCb func,
        FklCodegenAction *prev,
        FklVMvalueCodegenInfo *codegen) {
    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, value);
    FklCodegenAction *quest = create_cg_action(func,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(value->container),
            // value->curline,
            prev,
            codegen);
    fklCodegenActionVectorPushBack2(actions, quest);
}

static CODEGEN_FUNC(codegen_unquote) {
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    unquoteHelperFunc(value,
            actions,
            scope,
            macro_scope,
            env,
            _default_bc_process,
            NULL,
            codegen);
}

BC_PROCESS(_qsquote_box_bc_process) {
    FklByteCodelnt *retval = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    FklInstruction pushBox = { .op = FKL_OP_PUSH_BOX, .ai = 1 };
    fklByteCodeLntPushBackIns(retval, &pushBox, fid, line, scope);
    return retval;
}

BC_PROCESS(_qsquote_vec_bc_process) {
    FklByteCodelnt *retval = bcl_vec->base[0];
    for (size_t i = 1; i < bcl_vec->size; i++) {
        FklByteCodelnt *cur = bcl_vec->base[i];
        fklCodeLntConcat(retval, cur);
        fklDestroyByteCodelnt(cur);
    }
    bcl_vec->size = 0;
    FklInstruction pushVec = create_op_ins(FKL_OP_PUSH_VEC_0);
    FklInstruction setBp = create_op_ins(FKL_OP_SET_BP);
    fklByteCodeLntInsertFrontIns(&setBp, retval, fid, line, scope);
    fklByteCodeLntPushBackIns(retval, &pushVec, fid, line, scope);
    return retval;
}

BC_PROCESS(_unqtesp_vec_bc_process) {
    FklByteCodelnt *retval = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    FklByteCodelnt *other =
            fklByteCodelntVectorIsEmpty(bcl_vec)
                    ? NULL
                    : *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    if (other) {
        while (!fklByteCodelntVectorIsEmpty(bcl_vec)) {
            FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
            fklCodeLntConcat(other, cur);
            fklDestroyByteCodelnt(cur);
        }
        fklCodeLntReverseConcat(other, retval);
        fklDestroyByteCodelnt(other);
    }
    FklInstruction listPush = { .op = FKL_OP_PAIR,
        .ai = FKL_SUBOP_PAIR_UNPACK };
    fklByteCodeLntPushBackIns(retval, &listPush, fid, line, scope);
    return retval;
}

BC_PROCESS(_qsquote_pair_bc_process) {
    FklByteCodelnt *retval = bcl_vec->base[0];
    for (size_t i = 1; i < bcl_vec->size; i++) {
        FklByteCodelnt *cur = bcl_vec->base[i];
        fklCodeLntConcat(retval, cur);
        fklDestroyByteCodelnt(cur);
    }
    bcl_vec->size = 0;
    FklInstruction pushPair = create_op_ins(FKL_OP_PUSH_LIST_0);
    FklInstruction setBp = create_op_ins(FKL_OP_SET_BP);
    fklByteCodeLntInsertFrontIns(&setBp, retval, fid, line, scope);
    fklByteCodeLntPushBackIns(retval, &pushPair, fid, line, scope);
    return retval;
}

BC_PROCESS(_qsquote_list_bc_process) {
    FklByteCodelnt *retval = bcl_vec->base[0];
    for (size_t i = 1; i < bcl_vec->size; i++) {
        FklByteCodelnt *cur = bcl_vec->base[i];
        fklCodeLntConcat(retval, cur);
        fklDestroyByteCodelnt(cur);
    }
    bcl_vec->size = 0;
    FklInstruction pushPair = { .op = FKL_OP_PAIR,
        .ai = FKL_SUBOP_PAIR_APPEND };
    fklByteCodeLntPushBackIns(retval, &pushPair, fid, line, scope);
    return retval;
}

typedef enum {
    QSQUOTE_NONE = 0,
    QSQUOTE_UNQTESP_CAR,
    QSQUOTE_UNQTESP_VEC,
} QsquoteHelperEnum;

typedef struct {
    QsquoteHelperEnum e;
    FklPmatchRes node;
    FklCodegenAction *prev;
} QsquoteHelperStruct;

static inline void init_qsquote_helper_struct(QsquoteHelperStruct *r,
        QsquoteHelperEnum e,
        const FklPmatchRes *node,
        FklCodegenAction *prev) {
    r->e = e;
    r->node = *node;
    r->prev = prev;
}

// CgQsquoteHelperVector
#define FKL_VECTOR_TYPE_PREFIX Cg
#define FKL_VECTOR_METHOD_PREFIX cg
#define FKL_VECTOR_ELM_TYPE QsquoteHelperStruct
#define FKL_VECTOR_ELM_TYPE_NAME QsquoteHelper
#include <fakeLisp/cont/vector.h>

static inline int
is_unquote(FklCodegenCtx *ctx, FklVMvalue *v, FklPmatchHashMap *unquoteHt) {
    return fklPatternMatch(
            ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_UNQUOTE],
            v,
            unquoteHt);
}

static inline int
is_unqtesp(FklCodegenCtx *ctx, FklVMvalue *v, FklPmatchHashMap *unquoteHt) {
    return fklPatternMatch(
            ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_UNQTESP],
            v,
            unquoteHt);
}

typedef struct {
    FklCodegenCtx *ctx;
    const QsquoteHelperStruct *top;
    FklCodegenActionVector *action_vector;
    uint32_t scope;
    FklVMvalueCodegenMacroScope *macro_scope;
    FklVMvalueCodegenEnv *env;
    FklVMvalueCodegenInfo *codegen;
    CgQsquoteHelperVector *pending;
} QsquoteStateNoneArgs;

static void qsquote_state_none_pair(FklVMvalue *value,
        const FklVMvalue *container,
        const QsquoteStateNoneArgs *args,
        FklPmatchHashMap *table) {

    FklCodegenCtx *ctx = args->ctx;
    const QsquoteHelperStruct *top = args->top;
    FklVMvalueCodegenEnv *env = args->env;
    uint32_t scope = args->scope;
    FklCodegenAction *prev = top->prev;
    FklVMvalueCodegenMacroScope *macro_scope = args->macro_scope;
    FklCodegenActionVector *action_vector = args->action_vector;
    FklVMvalueCodegenInfo *codegen = args->codegen;
    CgQsquoteHelperVector *pending = args->pending;

    FklCodegenAction *curAction = create_cg_action(_qsquote_pair_bc_process,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(container),
            // curValue->curline,
            prev,
            codegen);
    fklCodegenActionVectorPushBack2(action_vector, curAction);
    for (FklVMvalue *node = value;;) {
        if (is_unquote(ctx, node, table)) {
            const FklPmatchRes *unquote_v =
                    fklPmatchHashMapGet2(table, ctx->builtInPatternVar_value);
            unquoteHelperFunc(unquote_v,
                    action_vector,
                    scope,
                    macro_scope,
                    env,
                    _default_bc_process,
                    curAction,
                    codegen);
            break;
        }
        FklVMvalue *cur = FKL_VM_CAR(node);
        if (is_unqtesp(ctx, cur, table)) {
            const FklPmatchRes *unqtesp_v =
                    fklPmatchHashMapGet2(table, ctx->builtInPatternVar_value);
            if (FKL_VM_CDR(node) != FKL_VM_NIL) {
                FklCodegenAction *appendAction =
                        create_cg_action(_qsquote_list_bc_process,
                                createDefaultStackContext(),
                                NULL,
                                scope,
                                macro_scope,
                                env,
                                CURLINE(container),
                                // curValue->curline,
                                curAction,
                                codegen);
                fklCodegenActionVectorPushBack2(action_vector, appendAction);
                init_qsquote_helper_struct(
                        cgQsquoteHelperVectorPushBack(pending, NULL),
                        QSQUOTE_UNQTESP_CAR,
                        unqtesp_v,
                        appendAction);
                init_qsquote_helper_struct(
                        cgQsquoteHelperVectorPushBack(pending, NULL),
                        QSQUOTE_NONE,
                        &(FklPmatchRes){
                            .value = FKL_VM_CDR(node),
                            .container = node,
                        },
                        // node->pair->cdr,
                        appendAction);
            } else
                init_qsquote_helper_struct(
                        cgQsquoteHelperVectorPushBack(pending, NULL),
                        QSQUOTE_UNQTESP_CAR,
                        unqtesp_v,
                        curAction);
            break;
        } else
            init_qsquote_helper_struct(
                    cgQsquoteHelperVectorPushBack(pending, NULL),
                    QSQUOTE_NONE,
                    &(FklPmatchRes){
                        .value = cur,
                        .container = node,
                    },
                    curAction);
        FklVMvalue *container = node;
        node = FKL_VM_CDR(node);
        if (!FKL_IS_PAIR(node)) {
            init_qsquote_helper_struct(
                    cgQsquoteHelperVectorPushBack(pending, NULL),
                    QSQUOTE_NONE,
                    &(FklPmatchRes){
                        .value = node,
                        .container = container,
                    },
                    curAction);
            break;
        }
    }
}

static void qsquote_state_none_vector(FklVMvalue *value,
        const FklVMvalue *container,
        const QsquoteStateNoneArgs *args,
        FklPmatchHashMap *table) {

    FklCodegenCtx *ctx = args->ctx;
    const QsquoteHelperStruct *top = args->top;
    FklVMvalueCodegenEnv *env = args->env;
    uint32_t scope = args->scope;
    FklCodegenAction *prev = top->prev;
    FklVMvalueCodegenMacroScope *macro_scope = args->macro_scope;
    FklCodegenActionVector *action_vector = args->action_vector;
    FklVMvalueCodegenInfo *codegen = args->codegen;
    CgQsquoteHelperVector *pending = args->pending;

    size_t vec_len = FKL_VM_VEC(value)->size;
    FklCodegenAction *action = create_cg_action(_qsquote_vec_bc_process,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(container),
            // curValue->curline,
            prev,
            codegen);
    fklCodegenActionVectorPushBack2(action_vector, action);
    for (size_t i = 0; i < vec_len; i++) {
        if (is_unqtesp(ctx,
                    FKL_VM_VEC(value)->base[i],
                    // curValue->vec->base[i],
                    table))
            init_qsquote_helper_struct(
                    cgQsquoteHelperVectorPushBack(pending, NULL),
                    QSQUOTE_UNQTESP_VEC,
                    fklPmatchHashMapGet2(table, ctx->builtInPatternVar_value),
                    action);
        else
            init_qsquote_helper_struct(
                    cgQsquoteHelperVectorPushBack(pending, NULL),
                    QSQUOTE_NONE,
                    &(FklPmatchRes){
                        .value = FKL_VM_VEC(value)->base[i],
                        .container = value,
                    },
                    // curValue->vec->base[i],
                    action);
    }
}

static void qsquote_state_none_box(FklVMvalue *value,
        const FklVMvalue *container,
        const QsquoteStateNoneArgs *args,
        FklPmatchHashMap *table) {

    const QsquoteHelperStruct *top = args->top;
    FklVMvalueCodegenEnv *env = args->env;
    uint32_t scope = args->scope;
    FklCodegenAction *prev = top->prev;
    FklVMvalueCodegenMacroScope *macro_scope = args->macro_scope;
    FklCodegenActionVector *action_vector = args->action_vector;
    FklVMvalueCodegenInfo *codegen = args->codegen;
    CgQsquoteHelperVector *pending = args->pending;

    FklCodegenAction *action = create_cg_action(_qsquote_box_bc_process,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(container),
            // curValue->curline,
            prev,
            codegen);
    fklCodegenActionVectorPushBack2(action_vector, action);
    init_qsquote_helper_struct(cgQsquoteHelperVectorPushBack(pending, NULL),
            QSQUOTE_NONE,
            &(FklPmatchRes){
                .value = FKL_VM_BOX(value),
                .container = value,
            },
            // curValue->box,
            action);
}

static void qsquote_state_none(const QsquoteStateNoneArgs *args) {
    FklCodegenCtx *ctx = args->ctx;
    const QsquoteHelperStruct *top = args->top;
    FklVMvalueCodegenEnv *env = args->env;
    uint32_t scope = args->scope;
    FklCodegenAction *prev = top->prev;
    FklVMvalueCodegenMacroScope *macro_scope = args->macro_scope;
    FklCodegenActionVector *action_vector = args->action_vector;
    FklVMvalueCodegenInfo *codegen = args->codegen;

    FklVMvalue *value = top->node.value;
    const FklVMvalue *container = top->node.container;

    FklPmatchHashMap table;
    fklPmatchHashMapInit(&table);
    if (is_unquote(ctx, value, &table)) {
        const FklPmatchRes *unquoteValue =
                fklPmatchHashMapGet2(&table, ctx->builtInPatternVar_value);
        unquoteHelperFunc(unquoteValue,
                action_vector,
                scope,
                macro_scope,
                env,
                _default_bc_process,
                prev,
                codegen);
    } else if (FKL_IS_PAIR(value)) {
        qsquote_state_none_pair(value, container, args, &table);
    } else if (FKL_IS_VECTOR(value)) {
        qsquote_state_none_vector(value, container, args, &table);
    } else if (FKL_IS_BOX(value)) {
        qsquote_state_none_box(value, container, args, &table);
    } else
        push_default_codegen_quest(&top->node,
                action_vector,
                scope,
                macro_scope,
                env,
                prev,
                codegen);
    fklPmatchHashMapUninit(&table);
}

static CODEGEN_FUNC(codegen_qsquote) {
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    CgQsquoteHelperVector pending;
    cgQsquoteHelperVectorInit(&pending, 8);
    init_qsquote_helper_struct(cgQsquoteHelperVectorPushBack(&pending, NULL),
            QSQUOTE_NONE,
            value,
            NULL);
    while (!cgQsquoteHelperVectorIsEmpty(&pending)) {
        const QsquoteHelperStruct top =
                *cgQsquoteHelperVectorPopBackNonNull(&pending);
        QsquoteHelperEnum e = top.e;
        FklCodegenAction *prevAction = top.prev;
        switch (e) {
        case QSQUOTE_UNQTESP_CAR:
            unquoteHelperFunc(&top.node,
                    actions,
                    scope,
                    macro_scope,
                    env,
                    _default_bc_process,
                    prevAction,
                    codegen);
            break;
        case QSQUOTE_UNQTESP_VEC:
            unquoteHelperFunc(&top.node,
                    actions,
                    scope,
                    macro_scope,
                    env,
                    _unqtesp_vec_bc_process,
                    prevAction,
                    codegen);
            break;
        case QSQUOTE_NONE: {
            QsquoteStateNoneArgs args = {
                .ctx = ctx,
                .top = &top,
                .action_vector = actions,
                .scope = scope,
                .macro_scope = macro_scope,
                .env = env,
                .codegen = codegen,
                .pending = &pending,
            };
            qsquote_state_none(&args);
        } break;
        }
    }
    cgQsquoteHelperVectorUninit(&pending);
}

BC_PROCESS(_cond_exp_bc_process_0) {
    FklByteCodelnt *retval = NULL;
    if (bcl_vec->size)
        retval = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    else
        retval = fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
    return retval;
}

static inline int is_const_true_bytecode_lnt(const FklByteCodelnt *bcl) {
    const FklInstruction *cur = bcl->bc.code;
    const FklInstruction *const end = &cur[bcl->bc.len];
    for (; cur < end; cur++) {
        switch (cur->op) {
        case FKL_OP_SET_BP:
        case FKL_OP_PUSH_0:
        case FKL_OP_PUSH_1:
        case FKL_OP_PUSH_I8:
        case FKL_OP_PUSH_I16:
        case FKL_OP_PUSH_I24:
        case FKL_OP_PUSH_CHAR:
        case FKL_OP_PUSH_CONST:
        // case FKL_OP_PUSH_F64:
        // case FKL_OP_PUSH_F64_C:
        // case FKL_OP_PUSH_F64_X:
        // case FKL_OP_PUSH_STR:
        // case FKL_OP_PUSH_STR_C:
        // case FKL_OP_PUSH_STR_X:
        // case FKL_OP_PUSH_SYM:
        // case FKL_OP_PUSH_SYM_C:
        // case FKL_OP_PUSH_SYM_X:
        case FKL_OP_PUSH_PAIR:
        case FKL_OP_PUSH_LIST_0:
        case FKL_OP_PUSH_LIST:
        case FKL_OP_PUSH_VEC_0:
        case FKL_OP_PUSH_VEC:
        // case FKL_OP_PUSH_BI:
        // case FKL_OP_PUSH_BI_C:
        // case FKL_OP_PUSH_BI_X:
        // case FKL_OP_PUSH_BOX:
        // case FKL_OP_PUSH_BVEC:
        // case FKL_OP_PUSH_BVEC_X:
        // case FKL_OP_PUSH_BVEC_C:
        case FKL_OP_PUSH_HASHEQ_0:
        case FKL_OP_PUSH_HASHEQ:
        case FKL_OP_PUSH_HASHEQV_0:
        case FKL_OP_PUSH_HASHEQV:
        case FKL_OP_PUSH_HASHEQUAL_0:
        case FKL_OP_PUSH_HASHEQUAL:
        // case FKL_OP_PUSH_I64B:
        // case FKL_OP_PUSH_I64B_C:
        // case FKL_OP_PUSH_I64B_X:
        case FKL_OP_TRUE:
        case FKL_OP_EXTRA_ARG:
            break;
        default:
            return 0;
        }
    }
    return 1;
}

BC_PROCESS(_cond_exp_bc_process_1) {
    FklByteCodelnt *retval = NULL;
    if (bcl_vec->size >= 2) {
        FklByteCodelnt *prev = bcl_vec->base[0];
        FklByteCodelnt *first = bcl_vec->base[1];
        retval = fklCreateByteCodelnt(0);

        FklInstruction drop = create_op_ins(FKL_OP_DROP);

        fklByteCodeLntInsertFrontIns(&drop, prev, fid, line, scope);

        uint64_t prev_len = prev->bc.len;

        int true_bcl = is_const_true_bytecode_lnt(first);

        if (bcl_vec->size > 2) {
            FklByteCodelnt *cur = bcl_vec->base[2];
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }

        for (size_t i = 3; i < bcl_vec->size; i++) {
            FklByteCodelnt *cur = bcl_vec->base[i];
            fklByteCodeLntPushBackIns(retval, &drop, fid, line, scope);
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }

        size_t retval_len = retval->bc.len;

        check_and_close_ref(retval, scope, env, &codegen->pts->p, fid, line);

        if (bcl_vec->size > 2) {
            append_jmp_ins(INS_APPEND_BACK,
                    retval,
                    prev_len,
                    JMP_UNCOND,
                    JMP_FORWARD,
                    fid,
                    line,
                    scope);
        }

        if (retval_len) {
            if (!true_bcl) {
                fklByteCodeLntInsertFrontIns(&drop, retval, fid, line, scope);
                append_jmp_ins(INS_APPEND_FRONT,
                        retval,
                        retval->bc.len,
                        JMP_IF_FALSE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
                fklCodeLntReverseConcat(first, retval);
            }
        } else {
            append_jmp_ins(INS_APPEND_BACK,
                    first,
                    prev->bc.len,
                    true_bcl ? JMP_UNCOND : JMP_IF_TRUE,
                    JMP_FORWARD,
                    fid,
                    line,
                    scope);
            fklCodeLntReverseConcat(first, retval);
        }

        fklCodeLntConcat(retval, prev);
        fklDestroyByteCodelnt(prev);
        fklDestroyByteCodelnt(first);
    } else
        retval = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    bcl_vec->size = 0;

    return retval;
}

BC_PROCESS(_cond_exp_bc_process_2) {
    FklInstruction drop = create_op_ins(FKL_OP_DROP);

    FklByteCodelnt *retval = NULL;
    if (bcl_vec->size) {
        retval = fklCreateByteCodelnt(0);
        FklByteCodelnt *first = bcl_vec->base[0];
        int true_bcl = is_const_true_bytecode_lnt(first);
        if (bcl_vec->size > 1) {
            FklByteCodelnt *cur = bcl_vec->base[1];
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }
        for (size_t i = 2; i < bcl_vec->size; i++) {
            FklByteCodelnt *cur = bcl_vec->base[i];
            fklByteCodeLntPushBackIns(retval, &drop, fid, line, scope);
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }

        if (retval->bc.len) {
            if (!true_bcl) {
                fklByteCodeLntInsertFrontIns(&drop, retval, fid, line, scope);
                append_jmp_ins(INS_APPEND_FRONT,
                        retval,
                        retval->bc.len,
                        JMP_IF_FALSE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
                fklCodeLntReverseConcat(first, retval);
            }
        } else
            fklCodeLntReverseConcat(first, retval);
        fklDestroyByteCodelnt(first);
    }
    bcl_vec->size = 0;
    return retval;
}

static CODEGEN_FUNC(codegen_cond) {
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    FklCodegenAction *quest = create_cg_action(_cond_exp_bc_process_0,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(rest->value),
            // rest->curline,
            NULL,
            codegen);
    fklCodegenActionVectorPushBack2(actions, quest);
    if (rest->value != FKL_VM_NIL) {
        FklVMvalueVector tmpStack;
        fklVMvalueVectorInit(&tmpStack, 32);
        pushListItemToStack(rest->value, &tmpStack, NULL);
        FklVMvalue *lastExp = *fklVMvalueVectorPopBackNonNull(&tmpStack);
        FklCodegenAction *prevAction = quest;
        for (size_t i = 0; i < tmpStack.size; i++) {
            FklVMvalue *curExp = tmpStack.base[i];
            if (!FKL_IS_PAIR(curExp)) {
                error_state->type = FKL_ERR_SYNTAXERROR;
                // error_state->place = orig->value;
                error_state->p = PLACE(orig->value, CURLINE(orig->container));
                // fklMakeNastNodeRef(orig);
                fklVMvalueVectorUninit(&tmpStack);
                return;
            }
            FklVMvalue *last = FKL_VM_NIL;
            CgExpQueue *curQueue = cgExpQueueCreate();
            pushListItemToQueue(curExp, curQueue, &last);
            if (last != FKL_VM_NIL) {
                error_state->type = FKL_ERR_SYNTAXERROR;
                // error_state->place = orig->value;
                error_state->p = PLACE(orig->value, CURLINE(orig->container));
                // fklMakeNastNodeRef(orig);
                // while (!fklNastNodeQueueIsEmpty(curQueue))
                //     fklDestroyNastNode(*fklNastNodeQueuePop(curQueue));
                cgExpQueueDestroy(curQueue);
                fklVMvalueVectorUninit(&tmpStack);
                return;
            }
            uint32_t curScope = enter_new_scope(scope, env);
            FklVMvalueCodegenMacroScope *cms =
                    fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);
            FklCodegenAction *curAction =
                    create_cg_action(_cond_exp_bc_process_1,
                            createDefaultStackContext(),
                            createFirstHasRetvalQueueNextExpression(curQueue),
                            curScope,
                            cms,
                            env,
                            CURLINE(curExp),
                            // curExp->curline,
                            prevAction,
                            codegen);
            fklCodegenActionVectorPushBack2(actions, curAction);
            prevAction = curAction;
        }
        FklVMvalue *last = FKL_VM_NIL;
        if (!FKL_IS_PAIR(lastExp)) {
            error_state->type = FKL_ERR_SYNTAXERROR;
            // error_state->place = orig->value;
            error_state->p = PLACE(orig->value, CURLINE(orig->container));
            // fklMakeNastNodeRef(orig);
            fklVMvalueVectorUninit(&tmpStack);
            return;
        }
        CgExpQueue *lastQueue = cgExpQueueCreate();
        pushListItemToQueue(lastExp, lastQueue, &last);
        if (last != FKL_VM_NIL) {
            error_state->type = FKL_ERR_SYNTAXERROR;
            // error_state->place = orig->value;
            error_state->p = PLACE(orig->value, CURLINE(orig->container));
            // fklMakeNastNodeRef(orig);
            // while (!fklNastNodeQueueIsEmpty(lastQueue))
            //     fklDestroyNastNode(*fklNastNodeQueuePop(lastQueue));
            cgExpQueueDestroy(lastQueue);
            fklVMvalueVectorUninit(&tmpStack);
            return;
        }
        uint32_t curScope = enter_new_scope(scope, env);
        FklVMvalueCodegenMacroScope *cms =
                fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);
        fklCodegenActionVectorPushBack2(actions,
                create_cg_action(_cond_exp_bc_process_2,
                        createDefaultStackContext(),
                        createFirstHasRetvalQueueNextExpression(lastQueue),
                        curScope,
                        cms,
                        env,
                        CURLINE(lastExp),
                        // lastExp->curline,
                        prevAction,
                        codegen));
        fklVMvalueVectorUninit(&tmpStack);
    }
}

BC_PROCESS(_if_exp_bc_process_0) {
    FklInstruction drop = create_op_ins(FKL_OP_DROP);

    if (bcl_vec->size >= 2) {
        FklByteCodelnt *exp = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
        FklByteCodelnt *cond = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
        fklByteCodeLntInsertFrontIns(&drop, exp, fid, line, scope);
        append_jmp_ins(INS_APPEND_BACK,
                cond,
                exp->bc.len,
                JMP_IF_FALSE,
                JMP_FORWARD,
                fid,
                line,
                scope);
        fklCodeLntConcat(cond, exp);
        fklDestroyByteCodelnt(exp);
        return cond;
    } else if (bcl_vec->size >= 1)
        return *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static CODEGEN_FUNC(codegen_if0) {
    const FklPmatchRes *cond =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    const FklPmatchRes *exp =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);

    CgExpQueue *nextQueue = cgExpQueueCreate();
    cgExpQueuePush(nextQueue, cond);
    cgExpQueuePush(nextQueue, exp);

    uint32_t curScope = enter_new_scope(scope, env);
    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);
    fklCodegenActionVectorPushBack2(actions,
            create_cg_action(_if_exp_bc_process_0,
                    createDefaultStackContext(),
                    createMustHasRetvalQueueNextExpression(nextQueue),
                    curScope,
                    cms,
                    env,
                    CURLINE(orig->container),
                    // orig->curline,
                    NULL,
                    codegen));
}

BC_PROCESS(_if_exp_bc_process_1) {
    FklInstruction drop = create_op_ins(FKL_OP_DROP);

    if (bcl_vec->size >= 3) {
        FklByteCodelnt *exp0 = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
        FklByteCodelnt *cond = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
        FklByteCodelnt *exp1 = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
        fklByteCodeLntInsertFrontIns(&drop, exp0, fid, line, scope);
        fklByteCodeLntInsertFrontIns(&drop, exp1, fid, line, scope);
        append_jmp_ins(INS_APPEND_BACK,
                exp0,
                exp1->bc.len,
                JMP_UNCOND,
                JMP_FORWARD,
                fid,
                line,
                scope);
        append_jmp_ins(INS_APPEND_BACK,
                cond,
                exp0->bc.len,
                JMP_IF_FALSE,
                JMP_FORWARD,
                fid,
                line,
                scope);
        fklCodeLntConcat(cond, exp0);
        fklCodeLntConcat(cond, exp1);
        fklDestroyByteCodelnt(exp0);
        fklDestroyByteCodelnt(exp1);
        return cond;
    } else if (bcl_vec->size >= 2) {
        FklByteCodelnt *exp0 = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
        FklByteCodelnt *cond = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
        fklByteCodeLntInsertFrontIns(&drop, exp0, fid, line, scope);
        append_jmp_ins(INS_APPEND_BACK,
                cond,
                exp0->bc.len,
                JMP_IF_FALSE,
                JMP_FORWARD,
                fid,
                line,
                scope);
        fklCodeLntConcat(cond, exp0);
        fklDestroyByteCodelnt(exp0);
        return cond;
    } else if (bcl_vec->size >= 1)
        return *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static CODEGEN_FUNC(codegen_if1) {
    const FklPmatchRes *cond =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    const FklPmatchRes *exp0 =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    const FklPmatchRes *exp1 =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);

    CgExpQueue *exp0Queue = cgExpQueueCreate();
    cgExpQueuePush(exp0Queue, cond);
    cgExpQueuePush(exp0Queue, exp0);

    CgExpQueue *exp1Queue = cgExpQueueCreate();
    cgExpQueuePush(exp1Queue, exp1);

    uint32_t curScope = enter_new_scope(scope, env);
    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);
    FklCodegenAction *prev = create_cg_action(_if_exp_bc_process_1,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(exp0Queue),
            curScope,
            cms,
            env,
            CURLINE(orig->container),
            // orig->curline,
            NULL,
            codegen);
    fklCodegenActionVectorPushBack2(actions, prev);

    curScope = enter_new_scope(scope, env);
    cms = fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);
    fklCodegenActionVectorPushBack2(actions,
            create_cg_action(_default_bc_process,
                    createDefaultStackContext(),
                    createMustHasRetvalQueueNextExpression(exp1Queue),
                    curScope,
                    cms,
                    env,
                    CURLINE(orig->container),
                    // orig->curline,
                    prev,
                    codegen));
}

BC_PROCESS(_when_exp_bc_process) {
    if (bcl_vec->size) {
        FklByteCodelnt *cond = bcl_vec->base[0];
        FklByteCodelnt *retval = fklCreateByteCodelnt(0);

        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        for (size_t i = 1; i < bcl_vec->size; i++) {
            FklByteCodelnt *cur = bcl_vec->base[i];
            fklByteCodeLntPushBackIns(retval, &drop, fid, line, scope);
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }
        bcl_vec->size = 0;
        if (retval->bc.len)
            append_jmp_ins(INS_APPEND_FRONT,
                    retval,
                    retval->bc.len,
                    JMP_IF_FALSE,
                    JMP_FORWARD,
                    fid,
                    line,
                    scope);
        fklCodeLntReverseConcat(cond, retval);
        fklDestroyByteCodelnt(cond);
        check_and_close_ref(retval, scope, env, &codegen->pts->p, fid, line);
        return retval;
    } else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

BC_PROCESS(_unless_exp_bc_process) {
    if (bcl_vec->size) {
        FklByteCodelnt *cond = bcl_vec->base[0];
        FklByteCodelnt *retval = fklCreateByteCodelnt(0);

        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        for (size_t i = 1; i < bcl_vec->size; i++) {
            FklByteCodelnt *cur = bcl_vec->base[i];
            fklByteCodeLntPushBackIns(retval, &drop, fid, line, scope);
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }
        bcl_vec->size = 0;
        if (retval->bc.len)
            append_jmp_ins(INS_APPEND_FRONT,
                    retval,
                    retval->bc.len,
                    JMP_IF_TRUE,
                    JMP_FORWARD,
                    fid,
                    line,
                    scope);
        fklCodeLntReverseConcat(cond, retval);
        fklDestroyByteCodelnt(cond);
        check_and_close_ref(retval, scope, env, &codegen->pts->p, fid, line);
        return retval;
    } else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static inline void codegen_when_unless(FklPmatchHashMap *ht,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenActionVector *actions,
        FklCodegenActionCb func,
        FklCodegenErrorState *error_state,
        const FklPmatchRes *orig) {
    FklCodegenCtx *ctx = codegen->ctx;
    const FklPmatchRes *cond =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);

    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, cond);
    pushListItemToQueue(rest->value, queue, NULL);

    uint32_t curScope = enter_new_scope(scope, env);
    FklVMvalueCodegenMacroScope *cms =
            fklCreateVMvalueCodegenMacroScope(codegen->ctx, macro_scope);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(func,
            createDefaultStackContext(),
            createFirstHasRetvalQueueNextExpression(queue),
            curScope,
            cms,
            env,
            CURLINE(rest->container),
            // rest->curline,
            codegen,
            actions);
}

static CODEGEN_FUNC(codegen_when) {
    codegen_when_unless(ht,
            scope,
            macro_scope,
            env,
            codegen,
            actions,
            _when_exp_bc_process,
            error_state,
            orig);
}

static CODEGEN_FUNC(codegen_unless) {
    codegen_when_unless(ht,
            scope,
            macro_scope,
            env,
            codegen,
            actions,
            _unless_exp_bc_process,
            error_state,
            orig);
}

typedef struct {
    FILE *fp;
    FklVMvalueCodegenInfo *codegen;
    FklAnalysisSymbolVector symbolStack;
    FklParseStateVector stateStack;
    FklUintVector lineStack;
} CodegenLoadContext;

#include <fakeLisp/grammer.h>

static CodegenLoadContext *createCodegenLoadContext(FILE *fp,
        FklVMvalueCodegenInfo *codegen) {
    CodegenLoadContext *r =
            (CodegenLoadContext *)fklZmalloc(sizeof(CodegenLoadContext));
    FKL_ASSERT(r);
    r->codegen = codegen;
    r->fp = fp;
    fklParseStateVectorInit(&r->stateStack, 16);
    fklUintVectorInit(&r->lineStack, 16);
    fklAnalysisSymbolVectorInit(&r->symbolStack, 16);
    return r;
}

static void _codegen_load_finalizer(void *pcontext) {
    CodegenLoadContext *context = pcontext;
    FklAnalysisSymbolVector *symbolStack = &context->symbolStack;
    // while (!fklAnalysisSymbolVectorIsEmpty(symbolStack))
    //     fklDestroyNastNode(
    //             fklAnalysisSymbolVectorPopBackNonNull(symbolStack)->ast);
    fklAnalysisSymbolVectorUninit(symbolStack);
    fklParseStateVectorUninit(&context->stateStack);
    fklUintVectorUninit(&context->lineStack);
    fclose(context->fp);
    fklZfree(context);
}

static void _codegen_load_atomic(FklVMgc *gc, void *ctx) {
    CodegenLoadContext *c = FKL_TYPE_CAST(CodegenLoadContext *, ctx);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, c->codegen), gc);
}

static inline FklVMvalue *getExpressionFromFile(FklVMvalueCodegenInfo *codegen,
        FILE *fp,
        int *unexpectEOF,
        size_t *errorLine,
        FklCodegenErrorState *error_state,
        FklAnalysisSymbolVector *symbolStack,
        FklUintVector *lineStack,
        FklParseStateVector *stateStack) {
    // FklVMobarray *pst = &codegen->ctx->public_st;
    // FklSymbolTable *pst = &codegen->ctx->public_st;
    size_t size = 0;
    FklVMvalue *begin = NULL;
    char *list = NULL;
    stateStack->size = 0;
    FklGrammer *g = *codegen->g;
    if (g) {
        codegen->ctx->cur_file_dir = codegen->dir;
        fklParseStateVectorPushBack2(stateStack,
                (FklParseState){ .state = &g->aTable.states[0] });
        list = fklReadWithAnalysisTable(g,
                fp,
                &size,
                codegen->curline,
                &codegen->curline,
                &codegen->ctx->gc->gcvm,
                unexpectEOF,
                errorLine,
                &begin,
                symbolStack,
                lineStack,
                stateStack,
                &codegen->ctx->lnt->ht);
    } else {
        fklVMvaluePushState0ToStack(stateStack);
        list = fklReadWithBuiltinParser(fp,
                &size,
                codegen->curline,
                &codegen->curline,
                &codegen->ctx->gc->gcvm,
                unexpectEOF,
                errorLine,
                &begin,
                symbolStack,
                lineStack,
                stateStack,
                &codegen->ctx->lnt->ht);
    }
    if (list)
        fklZfree(list);
    if (*unexpectEOF)
        begin = NULL;
    // while (!fklAnalysisSymbolVectorIsEmpty(symbolStack))
    //     fklDestroyNastNode(
    //             fklAnalysisSymbolVectorPopBackNonNull(symbolStack)->ast);
    lineStack->size = 0;
    return begin;
}

#include <fakeLisp/grammer.h>

static int _codegen_load_get_next_expression(void *pcontext,
        FklPmatchRes *out,
        FklCodegenErrorState *error_state) {
    CodegenLoadContext *context = pcontext;
    FklVMvalueCodegenInfo *codegen = context->codegen;
    FklParseStateVector *stateStack = &context->stateStack;
    FklAnalysisSymbolVector *symbolStack = &context->symbolStack;
    FklUintVector *lineStack = &context->lineStack;
    FILE *fp = context->fp;
    int unexpectEOF = 0;
    size_t errorLine = 0;
    uint64_t curline = codegen->curline;
    FklVMvalue *begin = getExpressionFromFile(codegen,
            fp,
            &unexpectEOF,
            &errorLine,
            error_state,
            symbolStack,
            lineStack,
            stateStack);
    if (unexpectEOF) {
        error_state->p = PLACE(NULL, errorLine);
        // error_state->place = NULL;
        // error_state->line = errorLine;
        error_state->type = unexpectEOF == FKL_PARSE_TERMINAL_MATCH_FAILED
                                  ? FKL_ERR_UNEXPECTED_EOF
                                  : FKL_ERR_INVALIDEXPR;
        return 0;
    }
    if (begin == NULL)
        return 0;

    FklVMvalue *contianer = fklCreateVMvalueBox(&codegen->ctx->gc->gcvm, begin);
    put_line_number(&codegen->ctx->lnt->ht, contianer, curline);
    *out = (FklPmatchRes){ .value = begin, .container = contianer };
    return 1;
}

static int hasLoadSameFile(const char *rpath,
        const FklVMvalueCodegenInfo *codegen) {
    for (; codegen; codegen = codegen->prev)
        if (codegen->realpath && !strcmp(rpath, codegen->realpath))
            return 1;
    return 0;
}

static const FklNextExpressionMethodTable
        _codegen_load_get_next_expression_method_table = {
            .get_next_exp = _codegen_load_get_next_expression,
            .finalizer = _codegen_load_finalizer,
            .atomic = _codegen_load_atomic,
        };

static FklCodegenNextExpression *createFpNextExpression(FILE *fp,
        FklVMvalueCodegenInfo *codegen) {
    CodegenLoadContext *context = createCodegenLoadContext(fp, codegen);
    return createCodegenNextExpression(
            &_codegen_load_get_next_expression_method_table,
            context,
            0);
}

static CODEGEN_FUNC(codegen_load) {
    const FklPmatchRes *filename =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    if (!FKL_IS_STR(filename->value)) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        return;
    }

    if (rest->value != FKL_VM_NIL) {
        CgExpQueue *queue = cgExpQueueCreate();

        // FklNastPair *prevPair = orig->pair->cdr->pair;

        // FklNastNode *loadHead = orig->pair->car;

        FklVMvalue *prev = FKL_VM_CDR(orig->value);
        FklVMvalue *head = FKL_VM_CAR(orig->value);

        for (FklVMvalue *rv = rest->value; FKL_IS_PAIR(rv);
                rv = FKL_VM_CDR(rv)) {
            // FklNastNode *restExp = fklNastCons(fklMakeNastNodeRef(loadHead),
            //         rest,
            //         rest->curline);
            // prevPair->cdr = fklCreateNastNode(FKL_NAST_NIL, rest->curline);
            //
            // fklNastNodeQueuePush2(queue, restExp);
            //
            // prevPair = rest->pair;

            FklVMvalue *new_rest =
                    fklCreateVMvaluePair(&ctx->gc->gcvm, head, rv);
            put_line_number(&codegen->ctx->lnt->ht, new_rest, CURLINE(rv));
            cgExpQueuePush2(queue,
                    (FklPmatchRes){
                        .value = new_rest,
                        .container = new_rest,
                    });
            FKL_VM_CDR(prev) = FKL_VM_NIL;
            prev = rv;
        }
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_begin_exp_bc_process,
                createDefaultStackContext(),
                createDefaultQueueNextExpression(queue),
                scope,
                macro_scope,
                env,
                CURLINE(orig->container),
                // orig->curline,
                codegen,
                actions);
    }

    const FklString *filenameStr = FKL_VM_STR(filename->value);
    if (!fklIsAccessibleRegFile(filenameStr->str)) {
        error_state->type = FKL_ERR_FILEFAILURE;
        // error_state->place = filename->value;
        error_state->p = PLACE(filename->value, CURLINE(filename->container));
        return;
    }

    FklVMvalueCodegenInfo *nextCodegen =
            fklCreateVMvalueCodegenInfo(codegen->ctx,
                    codegen,
                    filenameStr->str,
                    &(FklCodegenInfoArgs){ .inherit_grammer = 1 });

    if (hasLoadSameFile(nextCodegen->realpath, codegen)) {
        error_state->type = FKL_ERR_CIRCULARLOAD;
        // error_state->place = filename->value;
        error_state->p = PLACE(filename->value, CURLINE(filename->container));
        return;
    }

    FILE *fp = fopen(filenameStr->str, "r");
    if (!fp) {
        error_state->type = FKL_ERR_FILEFAILURE;
        // error_state->place = filename->value;
        error_state->p = PLACE(filename->value, CURLINE(filename->container));
        return;
    }
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_begin_exp_bc_process,
            createDefaultStackContext(),
            createFpNextExpression(fp, nextCodegen),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            // orig->curline,
            nextCodegen,
            actions);
}

static inline char *combineFileNameFromListAndCheckPrivate(
        const FklVMvalue *list) {
    char *r = NULL;
    for (const FklVMvalue *curPair = list; FKL_IS_PAIR(curPair);
            curPair = FKL_VM_CDR(curPair)) {
        // FklNastNode *cur = curPair->pair->car;
        FklVMvalue *cur = FKL_VM_CAR(curPair);
        const FklString *str =
                FKL_VM_SYM(cur); // fklGetSymbolWithId(cur->sym, pst);
        if (curPair != list && str->str[0] == '_') {
            fklZfree(r);
            return NULL;
        }
        r = fklCstrStringCat(r, str);
        if (FKL_VM_CDR(curPair) != FKL_VM_NIL)
            r = fklStrCat(r, FKL_PATH_SEPARATOR_STR);
    }
    return r;
}

static void add_symbol_to_local_env_in_array(FklVMvalueCodegenEnv *env,
        const FklCgExportSidIdxHashMap *exports,
        FklByteCodelnt *bcl,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        append_import_ins(INS_APPEND_BACK,
                bcl,
                fklAddCodegenDefBySid(l->k, scope, env)->idx,
                l->v.idx,
                fid,
                line,
                scope);
    }
}

static void add_symbol_with_prefix_to_local_env_in_array(
        FklVMvalueCodegenEnv *env,
        const FklString *prefix,
        const FklCgExportSidIdxHashMap *exports,
        FklByteCodelnt *bcl,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope,
        FklCodegenCtx *ctx) {
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        const FklString *origSymbol = FKL_VM_SYM(l->k);
        fklStringBufferConcatWithString(&buf, prefix);
        fklStringBufferConcatWithString(&buf, origSymbol);

        FklVMvalue *sym = add_symbol_char_buf(ctx, buf.buf, buf.index);
        // fklAddSymbolCharBuf(buf.buf, buf.index, pst);
        uint32_t idx = fklAddCodegenDefBySid(sym, scope, env)->idx;
        append_import_ins(INS_APPEND_BACK,
                bcl,
                idx,
                l->v.idx,
                fid,
                line,
                scope);
        fklStringBufferClear(&buf);
    }
    fklUninitStringBuffer(&buf);
}

static FklVMvalue *add_prefix_for_exp_head(FklVMvalue *orig,
        const FklString *prefix,
        FklVMvalueCodegenInfo *codegen) {
    const FklString *head = FKL_VM_SYM(FKL_VM_CAR(orig));
    // fklGetSymbolWithId(orig->pair->car->sym, pst);
    FklString *symbolWithPrefix = fklStringAppend(prefix, head);
    FklVMvalue *patternWithPrefix = fklCreateVMvaluePair(&codegen->ctx->gc->gcvm,
            add_symbol(codegen->ctx, symbolWithPrefix),
            FKL_VM_CDR(orig));
    // fklNastConsWithSym(fklAddSymbol(symbolWithPrefix, pst),
    //         fklMakeNastNodeRef(orig->pair->cdr),
    //         orig->curline,
    //         orig->pair->car->curline);
    put_line_number(&codegen->ctx->lnt->ht, patternWithPrefix, CURLINE(orig));
    fklZfree(symbolWithPrefix);
    return patternWithPrefix;
}

static FklVMvalue *
replace_head_for_exp(FklVMvalue *orig, FklVMvalue *head, FklCodegenCtx *ctx) {
    FklVMvalue *r = fklCreateVMvaluePair(&ctx->gc->gcvm, head, orig);
    return r;
}

static inline void export_replacement_with_prefix(
        FklReplacementHashMap *replacements,
        FklVMvalueCodegenMacroScope *macros,
        const FklString *prefix,
        FklCodegenCtx *ctx) {
    for (FklReplacementHashMapNode *cur = replacements->first; cur;
            cur = cur->next) {
        const FklString *origSymbol = FKL_VM_SYM(cur->k);
        FklString *symbolWithPrefix = fklStringAppend(prefix, origSymbol);
        FklVMvalue *id = add_symbol(ctx, symbolWithPrefix);
        fklReplacementHashMapAdd2(macros->replacements, id, cur->v);
        fklZfree(symbolWithPrefix);
    }
}

typedef struct {
    FklVMvalueCodegenInfo *codegen;
    FklVMvalueCodegenEnv *env;
    FklVMvalueCodegenMacroScope *cms;
    FklVMvalueHashSet named_prod_group_ids;
    FklVMvalue *prefix;
    FklVMvalue *only;
    FklVMvalue *alias;
    FklVMvalue *except;
    uint32_t scope;
} ExportContextData;

static void export_context_data_finalizer(void *data) {
    ExportContextData *d = (ExportContextData *)data;
    fklVMvalueHashSetUninit(&d->named_prod_group_ids);
    d->env = NULL;
    d->codegen = NULL;
    d->prefix = NULL;
    d->only = NULL;
    d->alias = NULL;
    d->except = NULL;
}

static void export_context_data_atomic(FklVMgc *gc, void *ctx) {
    ExportContextData *d = ctx;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->cms), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->env), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->codegen), gc);
    fklVMgcToGray(d->prefix, gc);
    fklVMgcToGray(d->only, gc);
    fklVMgcToGray(d->alias, gc);
    fklVMgcToGray(d->except, gc);
}

static const FklCodegenActionContextMethodTable ExportContextMethodTable = {
    .size = sizeof(ExportContextData),
    .__finalizer = export_context_data_finalizer,
    .__atomic = export_context_data_atomic,
};

static inline int merge_all_grammer(FklVMvalueCodegenInfo *codegen) {
    FklGrammer *g = *codegen->g;
    if (fklMergeGrammer(g, codegen->unnamed_g, NULL))
        return 1;
    for (FklGraProdGroupHashMapNode *group_items =
                    codegen->named_prod_groups->first;
            group_items;
            group_items = group_items->next) {
        if (fklMergeGrammer(g, &group_items->v.g, NULL)) {
            return 1;
        }
    }

    return 0;
}
static inline int add_all_group_to_grammer(uint64_t line,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenErrorState *error_state) {
    if (codegen->g == NULL)
        return 0;

    FklGrammer *g = *codegen->g;

    if (g == NULL)
        return 0;

    if (merge_all_grammer(codegen))
        return 1;

    FklGrammerNonterm nonterm = { 0 };
    if (fklCheckAndInitGrammerSymbols(g, &nonterm)) {
        error_state->msg = "unresolovd non-terminal";
        FKL_ASSERT(nonterm.sid);
        if (nonterm.group) {
            FklVMvalue *n = fklCreateVMvaluePair(&codegen->ctx->gc->gcvm,
                    nonterm.group,
                    nonterm.group);
            // FklNastNode *n = fklCreateNastNode(FKL_NAST_PAIR, line);
            // n->pair = fklCreateNastPair();
            //
            // n->pair->car = fklCreateNastNode(FKL_NAST_SYM, line);
            // n->pair->car->sym = nonterm.group;
            //
            // n->pair->cdr = fklCreateNastNode(FKL_NAST_SYM, line);
            // n->pair->cdr->sym = nonterm.sid;

            // error_state->place = n;
            error_state->p = PLACE(n, line);
        } else {
            // error_state->place = fklCreateNastNode(FKL_NAST_SYM, line);
            // error_state->place->sym = nonterm.sid;
            // error_state->place = nonterm.sid;
            error_state->p = PLACE(nonterm.sid, line);
        }
        return 1;
    }

    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fklLr0ToLalrItems(itemSet, g);
    FklStringBuffer err_msg;
    fklInitStringBuffer(&err_msg);

    int r = fklGenerateLalrAnalyzeTable(g, itemSet, &err_msg);
    if (r)
        error_state->msg2 = fklZstrdup(err_msg.buf);
    fklUninitStringBuffer(&err_msg);
    fklLalrItemSetHashMapDestroy(itemSet);

    return r;
}

static inline int import_reader_macro(FklVMvalueCodegenInfo *codegen,
        FklCodegenErrorState *error_state,
        uint64_t curline,
        const FklCodegenLib *lib,
        FklGrammerProdGroupItem *group,
        FklVMvalue *origin_group_id,
        FklVMvalue *new_group_id) {

    FklVMgc *pst = codegen->ctx->gc;

    FklGrammerProdGroupItem *target_group =
            add_production_group(codegen->named_prod_groups, pst, new_group_id);

    merge_group(target_group,
            group,
            &(FklRecomputeGroupIdArgs){ .old_group_id = origin_group_id,
                .new_group_id = new_group_id });
    return 0;
}

static inline FklGrammer *init_builtin_grammer_and_prod_group(
        FklVMvalueCodegenInfo *codegen) {
    FklGrammer *g = *codegen->g;
    if (!g) {
        *codegen->g = fklCreateEmptyGrammer(&codegen->ctx->gc->gcvm);
        fklGraProdGroupHashMapInit(codegen->named_prod_groups);
        fklInitEmptyGrammer(codegen->unnamed_g, &codegen->ctx->gc->gcvm);
        g = *codegen->g;
        if (fklMergeGrammer(g, &codegen->ctx->builtin_g, NULL))
            FKL_UNREACHABLE();
    }

    return g;
}

static inline FklByteCodelnt *process_import_imported_lib_common(uint32_t libId,
        FklVMvalueCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        uint64_t curline,
        FklCodegenErrorState *error_state) {
    for (FklCodegenMacro *cur = lib->head; cur; cur = cur->next)
        add_compiler_macro(&macro_scope->head,
                cur->pattern,
                // cur->origin_exp,
                cur->bcl,
                cur->prototype_id);

    for (FklReplacementHashMapNode *cur = lib->replacements->first; cur;
            cur = cur->next) {
        fklReplacementHashMapAdd2(macro_scope->replacements, cur->k, cur->v);
    }

    if (lib->named_prod_groups.buckets) {
        for (FklGraProdGroupHashMapNode *prod_group_item =
                        lib->named_prod_groups.first;
                prod_group_item;
                prod_group_item = prod_group_item->next) {
            init_builtin_grammer_and_prod_group(codegen);

            if (import_reader_macro(codegen,
                        error_state,
                        curline,
                        lib,
                        &prod_group_item->v,
                        prod_group_item->k,
                        prod_group_item->k))
                break;
        }

        if (!error_state->type
                && add_all_group_to_grammer(curline, codegen, error_state)) {
            error_state->p = PLACE(NULL, curline);
            // error_state->line = curline;
            error_state->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
            // error_state->place = NULL;
            return NULL;
        }
    }

    FklByteCodelnt *load_lib = append_load_lib_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            curline,
            scope);

    add_symbol_to_local_env_in_array(env,
            &lib->exports,
            load_lib,
            codegen->fid,
            curline,
            scope);

    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib_prefix(uint32_t libId,
        FklVMvalueCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *prefixNode,
        FklCodegenErrorState *error_state) {
    FklCodegenCtx *ctx = codegen->ctx;
    const FklString *prefix = FKL_VM_SYM(prefixNode);
    // fklGetSymbolWithId(prefixNode->sym, pst);
    for (FklCodegenMacro *cur = lib->head; cur; cur = cur->next)
        add_compiler_macro(&macro_scope->head,
                add_prefix_for_exp_head(cur->pattern, prefix, codegen),
                // add_prefix_for_exp_head(cur->origin_exp, prefix, ctx),
                // add_prefix_for_pattern_origin_exp(cur->origin_exp, prefix,
                // pst),
                cur->bcl,
                cur->prototype_id);

    export_replacement_with_prefix(lib->replacements, macro_scope, prefix, ctx);
    if (lib->named_prod_groups.buckets) {
        FklStringBuffer buffer;
        fklInitStringBuffer(&buffer);
        for (FklGraProdGroupHashMapNode *prod_group_item =
                        lib->named_prod_groups.first;
                prod_group_item;
                prod_group_item = prod_group_item->next) {
            fklStringBufferConcatWithString(&buffer, prefix);
            fklStringBufferConcatWithString(&buffer,
                    FKL_VM_SYM(prod_group_item->k));
            // fklStringBufferConcatWithString(&buffer,
            //         fklGetSymbolWithId(prod_group_item->k, pst));

            FklVMvalue *group_id_with_prefix =
                    add_symbol_char_buf(ctx, buffer.buf, buffer.index);
            // fklAddSymbolCharBuf(buffer.buf, buffer.index, pst);

            init_builtin_grammer_and_prod_group(codegen);
            if (import_reader_macro(codegen,
                        error_state,
                        curline,
                        lib,
                        &prod_group_item->v,
                        prod_group_item->k,
                        group_id_with_prefix))
                break;

            fklStringBufferClear(&buffer);
        }
        fklUninitStringBuffer(&buffer);
        if (!error_state->type
                && add_all_group_to_grammer(curline, codegen, error_state)) {
            error_state->p = PLACE(NULL, curline);
            // error_state->line = curline;
            error_state->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
            // error_state->place = NULL;
            return NULL;
        }
    }
    FklByteCodelnt *load_lib = append_load_lib_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            curline,
            scope);

    add_symbol_with_prefix_to_local_env_in_array(env,
            prefix,
            &lib->exports,
            load_lib,
            codegen->fid,
            curline,
            scope,
            ctx);

    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib_only(uint32_t libId,
        FklVMvalueCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *only,
        FklCodegenErrorState *error_state) {
    FklByteCodelnt *load_lib = append_load_lib_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            curline,
            scope);

    const FklCgExportSidIdxHashMap *exports = &lib->exports;
    const FklReplacementHashMap *replace = lib->replacements;
    const FklCodegenMacro *head = lib->head;

    for (; FKL_IS_PAIR(only); only = FKL_VM_CDR(only)) {
        FklVMvalue *cur = FKL_VM_CAR(only);
        int r = 0;
        for (const FklCodegenMacro *macro = head; macro; macro = macro->next) {
            FklVMvalue *patternHead = FKL_VM_CAR(macro->pattern);
            if (patternHead == cur) {
                r = 1;
                add_compiler_macro(&macro_scope->head,
                        macro->pattern,
                        // fklMakeNastNodeRef(macro->origin_exp),
                        macro->bcl,
                        macro->prototype_id);
            }
        }
        FklVMvalue *rep = fklGetReplacement(cur, replace);
        if (rep) {
            r = 1;
            fklReplacementHashMapAdd2(macro_scope->replacements, cur, rep);
        }

        FklGrammerProdGroupItem *group = NULL;
        if (lib->named_prod_groups.buckets
                && (group = fklGraProdGroupHashMapGet2(&lib->named_prod_groups,
                            cur))) {
            r = 1;
            init_builtin_grammer_and_prod_group(codegen);
            if (import_reader_macro(codegen,
                        error_state,
                        curline,
                        lib,
                        group,
                        cur,
                        cur))
                break;
        }

        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);
        if (item) {
            uint32_t idx = fklAddCodegenDefBySid(cur, scope, env)->idx;
            append_import_ins(INS_APPEND_BACK,
                    load_lib,
                    idx,
                    item->idx,
                    codegen->fid,
                    CURLINE(only),
                    // only->curline,
                    scope);
        } else if (!r) {
            error_state->type = FKL_ERR_IMPORT_MISSING;
            error_state->p = PLACE(FKL_VM_CAR(only), CURLINE(only));
            // error_state->place = FKL_VM_CAR(only);
            // error_state->line = CURLINE(only);
            error_state->fid = add_symbol_cstr(codegen->ctx, codegen->filename);
            // fklAddSymbolCstr(codegen->filename, &codegen->ctx->public_st);
            break;
        }
    }

    if (!error_state->type
            && add_all_group_to_grammer(curline, codegen, error_state)) {
        error_state->p = PLACE(NULL, curline);
        // error_state->line = curline;
        error_state->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
        // error_state->place = NULL;
    }
    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib_except(uint32_t libId,
        FklVMvalueCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *except,
        FklCodegenErrorState *error_state) {
    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    const FklReplacementHashMap *replace = lib->replacements;
    const FklCodegenMacro *head = lib->head;

    FklVMvalueHashSet excepts;
    fklVMvalueHashSetInit(&excepts);
    FklByteCodelnt *load_lib = NULL;

    for (FklVMvalue *list = except; FKL_IS_PAIR(list); list = FKL_VM_CDR(list))
        fklVMvalueHashSetPut2(&excepts, FKL_VM_CAR(list));

    for (const FklCodegenMacro *macro = head; macro; macro = macro->next) {
        FklVMvalue *patternHead = FKL_VM_CAR(macro->pattern);
        if (!fklVMvalueHashSetHas2(&excepts, patternHead)) {
            add_compiler_macro(&macro_scope->head,
                    macro->pattern,
                    // fklMakeNastNodeRef(macro->origin_exp),
                    macro->bcl,
                    macro->prototype_id);
        }
    }

    for (FklReplacementHashMapNode *reps = replace->first; reps;
            reps = reps->next) {
        if (!fklVMvalueHashSetHas2(&excepts, reps->k))
            fklReplacementHashMapAdd2(macro_scope->replacements,
                    reps->k,
                    reps->v);
    }
    if (lib->named_prod_groups.buckets) {
        for (FklGraProdGroupHashMapNode *prod_group_item =
                        lib->named_prod_groups.first;
                prod_group_item;
                prod_group_item = prod_group_item->next) {
            if (!fklVMvalueHashSetHas2(&excepts, prod_group_item->k)) {
                init_builtin_grammer_and_prod_group(codegen);
                if (import_reader_macro(codegen,
                            error_state,
                            curline,
                            lib,
                            &prod_group_item->v,
                            prod_group_item->k,
                            prod_group_item->k))
                    break;
            }
        }
        if (!error_state->type
                && add_all_group_to_grammer(curline, codegen, error_state)) {
            error_state->p = PLACE(NULL, curline);
            // error_state->line = curline;
            error_state->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
            // error_state->place = NULL;
            load_lib = NULL;
            goto exit;
        }
    }

    load_lib = append_load_lib_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            curline,
            scope);

    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        if (!fklVMvalueHashSetHas2(&excepts, l->k)) {
            uint32_t idx = fklAddCodegenDefBySid(l->k, scope, env)->idx;

            append_import_ins(INS_APPEND_BACK,
                    load_lib,
                    idx,
                    l->v.idx,
                    codegen->fid,
                    CURLINE(except),
                    // except->curline,
                    scope);
        }
    }

exit:
    fklVMvalueHashSetUninit(&excepts);
    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib_alias(uint32_t libId,
        FklVMvalueCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *alias,
        FklCodegenErrorState *error_state) {
    FklByteCodelnt *load_lib = append_load_lib_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            curline,
            scope);

    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    const FklReplacementHashMap *replace = lib->replacements;
    const FklCodegenMacro *head = lib->head;

    for (; FKL_IS_PAIR(alias); alias = FKL_VM_CDR(alias)) {
        FklVMvalue *curNode = FKL_VM_CAR(alias);
        FklVMvalue *cur = FKL_VM_CAR(curNode);
        FklVMvalue *cur0 = FKL_VM_CAR(FKL_VM_CDR(curNode));
        // curNode->pair->cdr->pair->car->sym;
        int r = 0;
        for (const FklCodegenMacro *macro = head; macro; macro = macro->next) {
            FklVMvalue *patternHead = FKL_VM_CAR(macro->pattern);
            if (patternHead == cur) {
                r = 1;

                // FklNastNode *orig = macro->pattern;
                // FklNastNode *pattern = fklNastConsWithSym(cur0,
                //         fklMakeNastNodeRef(orig->pair->cdr),
                //         orig->curline,
                //         orig->pair->car->curline);

                add_compiler_macro(&macro_scope->head,
                        replace_head_for_exp(macro->pattern, cur, codegen->ctx),
                        // pattern,
                        // replace_pattern_origin_exp_head(macro->origin_exp,
                        //         cur0),
                        macro->bcl,
                        macro->prototype_id);
            }
        }
        FklVMvalue *rep = fklGetReplacement(cur, replace);
        if (rep) {
            r = 1;
            fklReplacementHashMapAdd2(macro_scope->replacements, cur0, rep);
        }

        FklGrammerProdGroupItem *group = NULL;
        if (lib->named_prod_groups.buckets
                && (group = fklGraProdGroupHashMapGet2(&lib->named_prod_groups,
                            cur))) {
            r = 1;
            init_builtin_grammer_and_prod_group(codegen);
            if (import_reader_macro(codegen,
                        error_state,
                        curline,
                        lib,
                        group,
                        cur,
                        cur0))
                break;
        }

        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);
        if (item) {
            uint32_t idx = fklAddCodegenDefBySid(cur0, scope, env)->idx;
            append_import_ins(INS_APPEND_BACK,
                    load_lib,
                    idx,
                    item->idx,
                    codegen->fid,
                    CURLINE(alias),
                    // alias->curline,
                    scope);
        } else if (!r) {
            error_state->type = FKL_ERR_IMPORT_MISSING;
            error_state->p = PLACE(FKL_VM_CAR(curNode), CURLINE(alias));
            // error_state->place = FKL_VM_CAR(curNode);
            // error_state->line = CURLINE(alias);
            error_state->fid = add_symbol_cstr(codegen->ctx, codegen->filename);
            // fklAddSymbolCstr(codegen->filename, &codegen->ctx->public_st);
            break;
        }
    }

    if (!error_state->type
            && add_all_group_to_grammer(curline, codegen, error_state)) {
        error_state->p = PLACE(NULL, curline);
        // error_state->line = curline;
        error_state->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
        // error_state->place = NULL;
    }
    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib(uint32_t libId,
        FklVMvalueCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *cms,
        uint32_t line,
        FklVMvalue *prefix,
        FklVMvalue *only,
        FklVMvalue *alias,
        FklVMvalue *except,
        FklCodegenErrorState *error_state) {
    if (prefix)
        return process_import_imported_lib_prefix(libId,
                codegen,
                lib,
                env,
                scope,
                cms,
                line,
                prefix,
                error_state);
    if (only)
        return process_import_imported_lib_only(libId,
                codegen,
                lib,
                env,
                scope,
                cms,
                line,
                only,
                error_state);
    if (alias)
        return process_import_imported_lib_alias(libId,
                codegen,
                lib,
                env,
                scope,
                cms,
                line,
                alias,
                error_state);
    if (except)
        return process_import_imported_lib_except(libId,
                codegen,
                lib,
                env,
                scope,
                cms,
                line,
                except,
                error_state);
    return process_import_imported_lib_common(libId,
            codegen,
            lib,
            env,
            scope,
            cms,
            line,
            error_state);
}

static inline int is_exporting_outer_ref_group(FklVMvalueCodegenInfo *codegen) {
    for (FklVMvalueHashSetNode *sid_list =
                    codegen->export_named_prod_groups->first;
            sid_list;
            sid_list = sid_list->next) {
        FklVMvalue *id = FKL_TYPE_CAST(FklVMvalue *, sid_list->k);
        FklGrammerProdGroupItem *group =
                fklGraProdGroupHashMapGet2(codegen->named_prod_groups, id);
        if ((group->flags & FKL_GRAMMER_GROUP_HAS_OUTER_REF) != 0)
            return 1;
    }
    return 0;
}

static inline void process_export_bc(FklVMvalueCodegenInfo *info,
        FklByteCodelnt *libBc,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    info->epc = libBc->bc.len;

    append_export_to_ins(INS_APPEND_BACK,
            libBc,
            info->libraries->size + 1,
            info->exports.count,
            fid,
            line,
            scope);
    for (const FklCgExportSidIdxHashMapNode *l = info->exports.first; l;
            l = l->next) {
        append_export_ins(INS_APPEND_BACK, libBc, l->v.oidx, fid, line, scope);
    }

    FklInstruction ret = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(libBc, &ret, fid, line, scope);
}

BC_PROCESS(_library_bc_process) {
    if (is_exporting_outer_ref_group(codegen)) {
        error_state->type = FKL_ERR_EXPORT_OUTER_REF_PROD_GROUP;

        error_state->p = PLACE(NULL, line);
        // error_state->line = line;
        // error_state->place = NULL;
        return NULL;
    }

    fklUpdatePrototype(&codegen->pts->p, env);
    fklPrintUndefinedRef(env);

    ExportContextData *d = FKL_TYPE_CAST(ExportContextData *, data);
    FklByteCodelnt *libBc = sequnce_exp_bc_process(bcl_vec, fid, line, scope);

    FklInstruction ret = create_op_ins(FKL_OP_RET);

    fklByteCodeLntPushBackIns(libBc, &ret, fid, line, scope);

    if (!env->is_debugging)
        fklPeepholeOptimize(libBc);

    process_export_bc(codegen, libBc, fid, line, scope);

    FklCodegenLib *lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
    fklInitCodegenScriptLib(lib, codegen, libBc, codegen->epc, env);

    codegen->realpath = NULL;

    codegen->export_macro = NULL;
    codegen->export_replacement = NULL;
    return process_import_imported_lib(codegen->libraries->size,
            d->codegen,
            lib,
            d->env,
            d->scope,
            d->cms,
            line,
            d->prefix,
            d->only,
            d->alias,
            d->except,
            error_state);
}

static FklCodegenActionContext *createExportContext(
        FklVMvalueCodegenInfo *codegen,
        FklVMvalueCodegenEnv *targetEnv,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *cms,
        FklVMvalue *prefix,
        FklVMvalue *only,
        FklVMvalue *alias,
        FklVMvalue *except) {
    FklCodegenActionContext *r =
            createCodegenActionContext(&ExportContextMethodTable);
    ExportContextData *data = FKL_TYPE_CAST(ExportContextData *, r->d);

    data->codegen = codegen;

    data->scope = scope;
    data->env = targetEnv;

    data->cms = cms;

    data->prefix = prefix;

    data->only = only;
    data->except = except;
    data->alias = alias;

    fklVMvalueHashSetInit(&data->named_prod_group_ids);
    if (codegen->named_prod_groups) {
        for (const FklGraProdGroupHashMapNode *cur =
                        codegen->named_prod_groups->first;
                cur;
                cur = cur->next) {
            fklVMvalueHashSetPut2(&data->named_prod_group_ids, cur->k);
        }
    }
    return r;
}

struct RecomputeImportSrcIdxCtx {
    FklVMvalue *id;
    FklVMvalueCodegenEnv *env;
    FklVMvalue **id_base;
};

static int recompute_import_dst_idx_predicate(FklOpcode op) {
    return op >= FKL_OP_IMPORT && op <= FKL_OP_IMPORT_XX;
}

static int recompute_import_dst_idx_func(void *cctx,
        FklOpcode *op,
        FklOpcodeMode *pmode,
        FklInstructionArg *ins_arg) {
    struct RecomputeImportSrcIdxCtx *ctx = cctx;
    FklVMvalue *id = ctx->id_base[ins_arg->uy];
    if (is_constant_defined(id, 1, ctx->env)) {
        ctx->id = id;
        return 1;
    }
    ins_arg->uy = fklAddCodegenDefBySid(id, 1, ctx->env)->idx;
    *op = FKL_OP_IMPORT;
    *pmode = FKL_OP_MODE_IuAuB;
    return 0;
}

static inline FklVMvalue *recompute_import_src_idx(FklByteCodelnt *bcl,
        FklVMvalueCodegenEnv *env,
        FklVMvalueVector *idPstack) {
    struct RecomputeImportSrcIdxCtx ctx = { .id = 0,
        .env = env,
        .id_base = idPstack->base };
    fklRecomputeInsImm(bcl,
            &ctx,
            recompute_import_dst_idx_predicate,
            recompute_import_dst_idx_func);
    return ctx.id;
}

static inline FklByteCodelnt *export_sequnce_exp_bc_process(
        FklByteCodelntVector *stack,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    if (stack->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *retval = fklCreateByteCodelnt(0);
        size_t top = stack->size;
        for (size_t i = 0; i < top; i++) {
            FklByteCodelnt *cur = stack->base[i];
            if (cur->bc.len) {
                fklCodeLntConcat(retval, cur);
                if (i < top - 1)
                    fklByteCodeLntPushBackIns(retval, &drop, fid, line, scope);
            }
            fklDestroyByteCodelnt(cur);
        }
        stack->size = 0;
        return retval;
    }
    return NULL;
}

BC_PROCESS(_export_import_bc_process) {
    ExportContextData *d = FKL_TYPE_CAST(ExportContextData *, data);
    FklByteCodelnt *bcl =
            export_sequnce_exp_bc_process(bcl_vec, fid, line, scope);
    if (bcl == NULL)
        return NULL;
    FklVMvalueCodegenEnv *targetEnv = d->env;

    FklSymDefHashMap *defs = &env->scopes[0].defs;

    FklVMvalueVector idPstack;
    fklVMvalueVectorInit(&idPstack, defs->count);

    for (FklSymDefHashMapNode *list = defs->first; list; list = list->next) {
        FklVMvalue *idp = list->k.id;
        fklVMvalueVectorPushBack2(&idPstack, idp);
    }

    if (idPstack.size) {
        FklVMvalue *const_def_id =
                recompute_import_src_idx(bcl, targetEnv, &idPstack);
        if (const_def_id) {
            fklDestroyByteCodelnt(bcl);
            bcl = NULL;
            // FklVMvalue *place = fklCreateNastNode(FKL_NAST_SYM, line);
            // place->sym = const_def_id;
            error_state->type = FKL_ERR_ASSIGN_CONSTANT;
            // error_state->line = line;
            // const FklString *sym = fklGetSymbolWithId(fid, codegen->st);
            // error_state->fid = fid ? fklAddSymbol(sym, &ctx->public_st) : 0;
            error_state->fid = fid;
            // error_state->place = const_def_id;
            error_state->p = PLACE(const_def_id, line);
            // error_state->place = place;
            goto exit;
        }

        FklCgExportSidIdxHashMap *exports = &codegen->exports;

        FklVMvalue **idPbase = idPstack.base;
        uint32_t top = idPstack.size;

        for (uint32_t i = 0; i < top; i++) {
            FklVMvalue *id = idPbase[i];
            FklCodegenExportIdx *item =
                    fklCgExportSidIdxHashMapGet2(exports, id);
            if (item == NULL) {
                uint32_t idx = exports->count;

                const FklSymDefHashMapElm *idx_elm =
                        fklGetCodegenDefByIdInScope(id, 1, targetEnv->scopes);

                fklCgExportSidIdxHashMapAdd(exports,
                        &id,
                        &(FklCodegenExportIdx){ .idx = idx,
                            .oidx = idx_elm->v.idx });
            }
        }
    }

    FklVMvalueCodegenMacroScope *macros = targetEnv->macros;

    for (FklCodegenMacro *head = env->macros->head; head; head = head->next) {
        add_compiler_macro(&macros->head,
                head->pattern,
                // fklMakeNastNodeRef(head->origin_exp),
                head->bcl,
                head->prototype_id);

        add_compiler_macro(&codegen->export_macro,
                head->pattern,
                // fklMakeNastNodeRef(head->origin_exp),
                head->bcl,
                head->prototype_id);
    }

    for (FklReplacementHashMapNode *cur = env->macros->replacements->first; cur;
            cur = cur->next) {
        fklReplacementHashMapAdd2(codegen->export_replacement, cur->k, cur->v);
        fklReplacementHashMapAdd2(macros->replacements, cur->k, cur->v);
    }

    FklVMvalueCodegenInfo *lib_codegen = d->codegen;
    if (lib_codegen->named_prod_groups->first) {
        for (const FklGraProdGroupHashMapNode *cur =
                        lib_codegen->named_prod_groups->first;
                cur;
                cur = cur->next) {
            if (!fklVMvalueHashSetHas2(&d->named_prod_group_ids, cur->k)) {
                FKL_ASSERT(lib_codegen->export_named_prod_groups);
                fklVMvalueHashSetPut2(lib_codegen->export_named_prod_groups,
                        cur->k);
            }
        }
    }
exit:
    fklVMvalueVectorUninit(&idPstack);

    return bcl;
}

static inline FklVMvalueCodegenInfo *get_lib_codegen(
        FklVMvalueCodegenInfo *libCodegen) {
    for (; libCodegen && !libCodegen->is_lib; libCodegen = libCodegen->prev)
        if (libCodegen->is_macro)
            return NULL;
    return libCodegen;
}

typedef struct {
    FklPmatchRes orig;
    uint8_t must_has_retval;
} ExportSequnceContextData;

BC_PROCESS(exports_bc_process) {
    ExportSequnceContextData *d =
            FKL_TYPE_CAST(ExportSequnceContextData *, data);
    FklByteCodelnt *bcl =
            export_sequnce_exp_bc_process(bcl_vec, fid, line, scope);
    if (d->must_has_retval && !bcl) {
        error_state->type = FKL_ERR_EXP_HAS_NO_VALUE;
        // error_state->place = d->orig;
        error_state->p = PLACE(d->orig.value, CURLINE(d->orig.container));
    }
    return bcl;
}

static CODEGEN_FUNC(codegen_export_none) {
    if (must_has_retval) {
        error_state->type = FKL_ERR_EXP_HAS_NO_VALUE;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        return;
    }
    FklVMvalueCodegenInfo *libCodegen = get_lib_codegen(codegen);

    if (libCodegen && scope == 1 && env->prev == codegen->global_env
            && macro_scope->prev == codegen->global_env->macros) {
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_empty_bc_process,
                createDefaultStackContext(),
                NULL,
                scope,
                macro_scope,
                env,
                CURLINE(orig->container),
                // orig->curline,
                codegen,
                actions);
    } else {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        return;
    }
}

static void export_sequnce_context_data_atomic(FklVMgc *gc, void *data) {
    ExportSequnceContextData *d = (ExportSequnceContextData *)data;
    fklVMgcToGray(d->orig.value, gc);
    fklVMgcToGray(d->orig.container, gc);
    // fklDestroyNastNode(d->orig);
}

static const FklCodegenActionContextMethodTable
        ExportSequnceContextMethodTable = {
            .size = sizeof(ExportSequnceContextData),
            .__atomic = export_sequnce_context_data_atomic,
        };

static FklCodegenActionContext *create_export_sequnce_context(
        const FklPmatchRes *orig,
        uint8_t must_has_retval) {
    FklCodegenActionContext *r =
            createCodegenActionContext(&ExportSequnceContextMethodTable);
    ExportSequnceContextData *data =
            FKL_TYPE_CAST(ExportSequnceContextData *, r->d);

    data->must_has_retval = must_has_retval;
    data->orig = *orig;
    return r;
}

static CODEGEN_FUNC(codegen_export) {
    FklVMvalueCodegenInfo *libCodegen = get_lib_codegen(codegen);

    if (libCodegen && scope == 1 && env->prev == codegen->global_env
            && macro_scope->prev == codegen->global_env->macros) {
        CgExpQueue *exportQueue = cgExpQueueCreate();
        // FklVMvalue *orig_exp = fklCopyNastNode(orig);
        const FklPmatchRes *rest =
                fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
        add_export_symbol(libCodegen, orig->value, rest->value, exportQueue);

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(exports_bc_process,
                create_export_sequnce_context(orig, must_has_retval),
                createDefaultQueueNextExpression(exportQueue),
                scope,
                macro_scope,
                env,
                CURLINE(orig->container),
                // orig->curline,
                codegen,
                actions);
    } else {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        return;
    }
}

static inline FklVMvalue *get_reader_macro_group_id(const FklVMvalue *node) {
    const FklVMvalue *name = cadr(node);
    if (FKL_IS_BOX(name)) {
        FklVMvalue *group_id_node = FKL_VM_BOX(name);
        if (FKL_IS_SYM(group_id_node))
            return group_id_node;
    }
    return 0;
}

typedef struct {
    FklVMvalue *id;
    FklCodegenExportIdx *item;
} ExportDefineContext;

static const FklCodegenActionContextMethodTable
        ExportDefineContextMethodTable = {
            .size = sizeof(ExportDefineContext),
        };

static inline FklCodegenActionContext *
create_export_define_context(FklVMvalue *id, FklCodegenExportIdx *item) {
    FklCodegenActionContext *r =
            createCodegenActionContext(&ExportDefineContextMethodTable);
    ExportDefineContext *ctx = FKL_TYPE_CAST(ExportDefineContext *, r->d);
    ctx->id = id;
    ctx->item = item;
    return r;
}

BC_PROCESS(_export_define_bc_process) {
    ExportDefineContext *exp_ctx = FKL_TYPE_CAST(ExportDefineContext *, data);
    exp_ctx->item->oidx =
            fklGetCodegenDefByIdInScope(exp_ctx->id, 1, env->scopes)->v.idx;
    return sequnce_exp_bc_process(bcl_vec, fid, line, scope);
}

static CODEGEN_FUNC(codegen_export_single) {
    FklVMvalueCodegenInfo *libCodegen = get_lib_codegen(codegen);
    if (!libCodegen || env->prev != codegen->global_env || scope > 1
            || macro_scope->prev != codegen->global_env->macros) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        return;
    }

    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    FklVMvalue *value_v = value->value;
    value_v =
            fklTryExpandCodegenMacro(value, codegen, macro_scope, error_state);
    if (error_state->type)
        return;

    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush2(queue,
            (FklPmatchRes){
                .value = value_v,
                .container = value->container,
            });

    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    FklVMvalue *name = NULL;
    if (!fklIsList(value_v))
        goto error;
    if (isBeginExp(value_v, builtin_pattern_node)) {
        // uint32_t refcount = value->pair->car->refcount;
        // uint32_t line = value->pair->car->curline;
        // *(value->pair->car) = *(orig->pair->car);
        // value->pair->car->refcount = refcount;
        // value->pair->car->curline = line;
        FKL_VM_CAR(value_v) = FKL_VM_CAR(orig->value);

        fklCodegenActionVectorPushBack2(actions,
                create_cg_action(exports_bc_process,
                        create_export_sequnce_context(orig, must_has_retval),
                        createDefaultQueueNextExpression(queue),
                        1,
                        macro_scope,
                        env,
                        CURLINE(orig->container),
                        // orig->curline,
                        NULL,
                        codegen));
    } else if (isExportDefunExp(value_v, builtin_pattern_node)) {
        // name = cadr_nast_node(value)->pair->car;
        name = caadr(value_v);
        goto process_def_in_lib;
    } else if (isExportDefineExp(value_v, builtin_pattern_node)) {
        name = cadr(value_v);
    process_def_in_lib:
        if (!FKL_IS_SYM(name))
            goto error;
        FklCodegenExportIdx *item =
                fklCgExportSidIdxHashMapGet2(&libCodegen->exports, name);
        if (item == NULL) {
            uint32_t idx = libCodegen->exports.count;
            item = fklCgExportSidIdxHashMapAdd(&libCodegen->exports,
                    &name,
                    &(FklCodegenExportIdx){ .idx = idx,
                        .oidx = FKL_VAR_REF_INVALID_CIDX });
        }

        fklCodegenActionVectorPushBack2(actions,
                create_cg_action(_export_define_bc_process,
                        create_export_define_context(name, item),
                        createDefaultQueueNextExpression(queue),
                        1,
                        macro_scope,
                        env,
                        CURLINE(orig->container),
                        // orig->curline,
                        NULL,
                        codegen));
    } else if (isExportDefmacroExp(value_v, builtin_pattern_node)) {
        if (must_has_retval)
            goto must_has_retval_error;

        FklVMvalueCodegenMacroScope *cms =
                fklCreateVMvalueCodegenMacroScope(ctx, macro_scope);

        fklCodegenActionVectorPushBack2(actions,
                create_cg_action(_export_macro_bc_process,
                        createDefaultStackContext(),
                        createDefaultQueueNextExpression(queue),
                        1,
                        cms,
                        env,
                        CURLINE(orig->container),
                        // orig->curline,
                        NULL,
                        codegen));
    } else if (isExportDefReaderMacroExp(value_v, builtin_pattern_node)) {
        if (must_has_retval)
            goto must_has_retval_error;

        FklVMvalue *group_id = get_reader_macro_group_id(value_v);
        if (!group_id)
            goto error;

        fklVMvalueHashSetPut2(libCodegen->export_named_prod_groups, group_id);
        fklCodegenActionVectorPushBack2(actions,
                create_cg_action(_empty_bc_process,
                        createDefaultStackContext(),
                        createDefaultQueueNextExpression(queue),
                        1,
                        macro_scope,
                        env,
                        CURLINE(orig->container),
                        // orig->curline,
                        NULL,
                        codegen));
    } else if (isExportImportExp(value_v, builtin_pattern_node)) {
        if (fklPatternMatch(
                    builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_NONE],
                    value_v,
                    NULL)
                && must_has_retval) {
        must_has_retval_error:
            // fklDestroyNastNode(value);
            cgExpQueueDestroy(queue);
            error_state->type = FKL_ERR_EXP_HAS_NO_VALUE;
            // error_state->place = orig->value;
            error_state->p = PLACE(orig->value, CURLINE(orig->value));
            return;
        }

        FklVMvalueCodegenEnv *exportEnv =
                fklCreateVMvalueCodegenEnv(ctx, NULL, 0, macro_scope);
        FklVMvalueCodegenMacroScope *cms = exportEnv->macros;

        fklCodegenActionVectorPushBack2(actions,
                create_cg_action(_export_import_bc_process,
                        createExportContext(libCodegen,
                                env,
                                1,
                                cms,
                                NULL,
                                NULL,
                                NULL,
                                NULL),
                        createDefaultQueueNextExpression(queue),
                        1,
                        cms,
                        exportEnv,
                        CURLINE(orig->container),
                        // orig->curline,
                        NULL,
                        codegen));
    } else {
    error:
        // fklDestroyNastNode(value);
        cgExpQueueDestroy(queue);
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        return;
    }
}

static size_t check_loaded_lib(const char *realpath,
        FklCodegenLibVector *loaded_libraries) {
    for (size_t i = 0; i < loaded_libraries->size; i++) {
        const FklCodegenLib *cur = &loaded_libraries->base[i];
        if (!strcmp(realpath, cur->rp))
            return i + 1;
    }
    return 0;
}

static inline void process_import_script_common_header(FklVMvalue *orig,
        FklVMvalue *name,
        const char *filename,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenErrorState *error_state,
        FklCodegenActionVector *actions,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        FklVMvalue *prefix,
        FklVMvalue *only,
        FklVMvalue *alias,
        FklVMvalue *except) {
    FklVMvalueCodegenInfo *nextCodegen =
            fklCreateVMvalueCodegenInfo(codegen->ctx,
                    codegen,
                    filename,
                    &(FklCodegenInfoArgs){
                        .is_lib = 1,
                    });

    if (hasLoadSameFile(nextCodegen->realpath, codegen)) {
        error_state->type = FKL_ERR_CIRCULARLOAD;
        // error_state->place = name;
        error_state->p = PLACE(name, CURLINE(orig));
        return;
    }
    size_t libId = check_loaded_lib(nextCodegen->realpath, codegen->libraries);
    if (!libId) {
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            error_state->type = FKL_ERR_FILEFAILURE;
            // error_state->place = name;
            error_state->p = PLACE(name, CURLINE(orig));
            return;
        }
        FklVMvalueCodegenEnv *libEnv = fklCreateVMvalueCodegenEnv(codegen->ctx,
                codegen->global_env,
                0,
                codegen->global_env->macros);
        create_and_insert_to_pool(nextCodegen, 0, libEnv, 0, 1);

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_library_bc_process,
                createExportContext(codegen,
                        env,
                        scope,
                        macro_scope,
                        prefix,
                        only,
                        alias,
                        except),
                createFpNextExpression(fp, nextCodegen),
                1,
                libEnv->macros,
                libEnv,
                CURLINE(orig),
                // orig->curline,
                nextCodegen,
                actions);
    } else {
        fklReplacementHashMapDestroy(nextCodegen->export_replacement);
        nextCodegen->export_replacement = NULL;
        const FklCodegenLib *lib = &codegen->libraries->base[libId - 1];

        FklByteCodelnt *importBc = process_import_imported_lib(libId,
                codegen,
                lib,
                env,
                scope,
                macro_scope,
                CURLINE(orig),
                // orig->curline,
                prefix,
                only,
                alias,
                except,
                error_state);

        FklCodegenAction *cur = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                NULL,
                1,
                codegen->global_env->macros,
                codegen->global_env,
                CURLINE(orig),
                // orig->curline,
                NULL,
                nextCodegen);

        fklByteCodelntVectorPushBack2(&cur->bcl_vector, importBc);
        fklCodegenActionVectorPushBack2(actions, cur);
    }
}

FklCgDllLibInitExportCb fklGetCodegenInitExportFunc(uv_lib_t *dll) {
    return (FklCgDllLibInitExportCb)fklGetAddress("_fklExportSymbolInit", dll);
}

static inline FklByteCodelnt *process_import_from_dll_only(FklVMvalue *orig,
        FklVMvalue *name,
        FklVMvalue *only,
        const char *filename,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenErrorState *error_state,
        uint32_t scope) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libraries);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            error_state->type = FKL_ERR_FILEFAILURE;
            // error_state->place = name;
            error_state->p = PLACE(name, CURLINE(orig));
            fklZfree(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCgDllLibInitExportCb initExport = fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            error_state->type = FKL_ERR_IMPORTFAILED;
            // error_state->place = name;
            error_state->p = PLACE(name, CURLINE(orig));
            fklZfree(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
        fklInitCodegenDllLib(codegen->ctx, lib, realpath, dll, initExport);
        libId = codegen->libraries->size;
    } else {
        lib = &codegen->libraries->base[libId - 1];
        dll = lib->dll;
        fklZfree(realpath);
    }

    FklByteCodelnt *load_dll = append_load_dll_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            CURLINE(orig),
            // orig->curline,
            scope);

    FklCgExportSidIdxHashMap *exports = &lib->exports;

    for (; FKL_IS_PAIR(only); only = FKL_VM_CDR(only)) {
        FklVMvalue *cur = FKL_VM_CAR(only);
        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);
        if (item) {
            uint32_t idx = fklAddCodegenDefBySid(cur, scope, env)->idx;
            append_import_ins(INS_APPEND_BACK,
                    load_dll,
                    idx,
                    item->idx,
                    codegen->fid,
                    CURLINE(only),
                    // only->curline,
                    scope);
        } else {
            error_state->type = FKL_ERR_IMPORT_MISSING;
            error_state->p = PLACE(FKL_VM_CAR(only), CURLINE(only));
            // error_state->place = FKL_VM_CAR(only);
            // error_state->line = CURLINE(only);
            error_state->fid = add_symbol_cstr(codegen->ctx, codegen->filename);
            break;
        }
    }

    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_except(FklVMvalue *orig,
        FklVMvalue *name,
        FklVMvalue *except,
        const char *filename,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenErrorState *error_state,
        uint32_t scope) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libraries);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            error_state->type = FKL_ERR_FILEFAILURE;
            // error_state->place = name;
            error_state->p = PLACE(name, CURLINE(orig));
            fklZfree(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCgDllLibInitExportCb initExport = fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            error_state->type = FKL_ERR_IMPORTFAILED;
            // error_state->place = name;
            error_state->p = PLACE(name, CURLINE(orig));
            fklZfree(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
        fklInitCodegenDllLib(codegen->ctx, lib, realpath, dll, initExport);
        libId = codegen->libraries->size;
    } else {
        lib = &codegen->libraries->base[libId - 1];
        dll = lib->dll;
        fklZfree(realpath);
    }

    FklByteCodelnt *load_dll = append_load_dll_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            CURLINE(orig),
            // orig->curline,
            scope);

    FklCgExportSidIdxHashMap *exports = &lib->exports;
    FklVMvalueHashSet excepts;
    fklVMvalueHashSetInit(&excepts);

    for (FklVMvalue *list = except; FKL_IS_PAIR(list); list = FKL_VM_CDR(list))
        fklVMvalueHashSetPut2(&excepts, FKL_VM_CAR(list));

    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        if (!fklVMvalueHashSetHas2(&excepts, l->k)) {
            uint32_t idx = fklAddCodegenDefBySid(l->k, scope, env)->idx;
            append_import_ins(INS_APPEND_BACK,
                    load_dll,
                    idx,
                    l->v.idx,
                    codegen->fid,
                    CURLINE(except),
                    // except->curline,
                    scope);
        }
    }

    fklVMvalueHashSetUninit(&excepts);
    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_common(FklVMvalue *orig,
        FklVMvalue *name,
        const char *filename,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenErrorState *error_state,
        uint32_t scope) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libraries);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            error_state->type = FKL_ERR_FILEFAILURE;
            // error_state->place = name;
            error_state->p = PLACE(name, CURLINE(orig));
            fklZfree(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCgDllLibInitExportCb initExport = fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            error_state->type = FKL_ERR_IMPORTFAILED;
            // error_state->place = name;
            error_state->p = PLACE(name, CURLINE(orig));
            fklZfree(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
        fklInitCodegenDllLib(codegen->ctx, lib, realpath, dll, initExport);
        libId = codegen->libraries->size;
    } else {
        lib = &codegen->libraries->base[libId - 1];
        dll = lib->dll;
        fklZfree(realpath);
    }

    FklByteCodelnt *load_dll = append_load_dll_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            CURLINE(orig),
            // orig->curline,
            scope);

    add_symbol_to_local_env_in_array(env,
            &lib->exports,
            load_dll,
            codegen->fid,
            CURLINE(orig),
            // orig->curline,
            scope);
    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_prefix(FklVMvalue *orig,
        FklVMvalue *name,
        FklVMvalue *prefixNode,
        const char *filename,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenErrorState *error_state,
        uint32_t scope) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libraries);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            error_state->type = FKL_ERR_FILEFAILURE;
            // error_state->place = name;
            error_state->p = PLACE(name, CURLINE(orig));
            fklZfree(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCgDllLibInitExportCb initExport = fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            error_state->type = FKL_ERR_IMPORTFAILED;
            // error_state->place = name;
            error_state->p = PLACE(name, CURLINE(orig));
            fklZfree(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
        fklInitCodegenDllLib(codegen->ctx, lib, realpath, dll, initExport);
        libId = codegen->libraries->size;
    } else {
        lib = &codegen->libraries->base[libId - 1];
        dll = lib->dll;
        fklZfree(realpath);
    }

    const FklString *prefix = FKL_VM_SYM(prefixNode);

    FklByteCodelnt *load_dll = append_load_dll_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            CURLINE(orig),
            // orig->curline,
            scope);

    add_symbol_with_prefix_to_local_env_in_array(env,
            prefix,
            &lib->exports,
            load_dll,
            codegen->fid,
            CURLINE(orig),
            // orig->curline,
            scope,
            codegen->ctx);

    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_alias(FklVMvalue *orig,
        FklVMvalue *name,
        FklVMvalue *alias,
        const char *filename,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenErrorState *error_state,
        uint32_t scope) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libraries);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            error_state->type = FKL_ERR_FILEFAILURE;
            // error_state->place = name;
            error_state->p = PLACE(name, CURLINE(orig));
            fklZfree(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCgDllLibInitExportCb initExport = fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            error_state->type = FKL_ERR_IMPORTFAILED;
            // error_state->place = name;
            error_state->p = PLACE(name, CURLINE(orig));
            fklZfree(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
        fklInitCodegenDllLib(codegen->ctx, lib, realpath, dll, initExport);
        libId = codegen->libraries->size;
    } else {
        lib = &codegen->libraries->base[libId - 1];
        dll = lib->dll;
        fklZfree(realpath);
    }

    FklByteCodelnt *load_dll = append_load_dll_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            CURLINE(orig),
            // orig->curline,
            scope);

    FklCgExportSidIdxHashMap *exports = &lib->exports;

    for (; FKL_IS_PAIR(alias); alias = FKL_VM_CDR(alias)) {
        FklVMvalue *curNode = FKL_VM_CAR(alias);
        FklVMvalue *cur = FKL_VM_CAR(curNode);
        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);

        if (item) {
            // FklSid_t cur0 = curNode->pair->cdr->pair->car->sym;
            FklVMvalue *cur0 = FKL_VM_CAR(FKL_VM_CDR(curNode));
            uint32_t idx = fklAddCodegenDefBySid(cur0, scope, env)->idx;
            append_import_ins(INS_APPEND_BACK,
                    load_dll,
                    idx,
                    item->idx,
                    codegen->fid,
                    CURLINE(alias),
                    // alias->curline,
                    scope);
        } else {
            error_state->type = FKL_ERR_IMPORT_MISSING;
            error_state->p = PLACE(FKL_VM_CAR(curNode), CURLINE(alias));
            // error_state->place = FKL_VM_CAR(curNode);
            // error_state->line = CURLINE(alias); // alias->curline;
            error_state->fid = add_symbol_cstr(codegen->ctx, codegen->filename);
            // fklAddSymbolCstr(codegen->filename, &codegen->ctx->public_st);
            break;
        }
    }

    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll(FklVMvalue *orig,
        FklVMvalue *name,
        const char *filename,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenErrorState *error_state,
        uint32_t scope,
        FklVMvalue *prefix,
        FklVMvalue *only,
        FklVMvalue *alias,
        FklVMvalue *except) {
    if (prefix)
        return process_import_from_dll_prefix(orig,
                name,
                prefix,
                filename,
                env,
                codegen,
                error_state,
                scope);
    if (only)
        return process_import_from_dll_only(orig,
                name,
                only,
                filename,
                env,
                codegen,
                error_state,
                scope);
    if (alias)
        return process_import_from_dll_alias(orig,
                name,
                alias,
                filename,
                env,
                codegen,
                error_state,
                scope);
    if (except)
        return process_import_from_dll_except(orig,
                name,
                except,
                filename,
                env,
                codegen,
                error_state,
                scope);
    return process_import_from_dll_common(orig,
            name,
            filename,
            env,
            codegen,
            error_state,
            scope);
}

static inline int is_valid_alias_sym_list(FklVMvalue *alias) {
    for (; FKL_IS_PAIR(alias); alias = FKL_VM_CDR(alias)) {
        FklVMvalue *cur = FKL_VM_CAR(alias);
        if (!FKL_IS_PAIR(cur)                               //
                || !FKL_IS_SYM(FKL_VM_CAR(cur))             //
                || !FKL_IS_PAIR(FKL_VM_CDR(cur))            //
                || !FKL_IS_SYM(FKL_VM_CAR(FKL_VM_CDR(cur))) //
                || FKL_VM_CDR(FKL_VM_CDR(cur)) != FKL_VM_NIL)
            return 0;
    }

    return alias == FKL_VM_NIL;
}

static inline void codegen_import_helper(FklVMvalue *orig,
        FklVMvalue *name,
        FklVMvalue *rest,
        FklVMvalueCodegenInfo *codegen,
        FklCodegenErrorState *error_state,
        FklVMvalueCodegenEnv *env,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        FklCodegenActionVector *actions,
        FklVMvalue *prefix,
        FklVMvalue *only,
        FklVMvalue *alias,
        FklVMvalue *except) {
    // if (name->type == FKL_NAST_NIL
    //         || !fklIsNastNodeListAndHasSameType(name, FKL_NAST_SYM)
    //         || (prefix && prefix->type != FKL_NAST_SYM)
    //         || (only && !fklIsNastNodeListAndHasSameType(only, FKL_NAST_SYM))
    //         || (except
    //                 && !fklIsNastNodeListAndHasSameType(except,
    //                 FKL_NAST_SYM))
    //         || (alias && !is_valid_alias_sym_list(alias))) {
    if (name == FKL_VM_NIL                     //
            || !is_symbol_list(name)           //
            || (prefix && !FKL_IS_SYM(prefix)) //
            || (only && !is_symbol_list(only)) //
            || (except && !is_symbol_list(except))
            || (alias && !is_valid_alias_sym_list(alias))) {
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = orig;
        error_state->p = PLACE(orig, CURLINE(orig));
        return;
    }

    if (rest != FKL_VM_NIL) {
        CgExpQueue *queue = cgExpQueueCreate();

        // FklNastPair *prevPair = orig->pair->cdr->pair;
        //
        // FklNastNode *importHead = orig->pair->car;

        FklVMvalue *prev = FKL_VM_CDR(orig);

        FklVMvalue *head = FKL_VM_CAR(orig);

        for (; FKL_IS_PAIR(rest); rest = FKL_VM_CDR(rest)) {
            // FklVMvalue *restExp = fklNastCons(fklMakeNastNodeRef(importHead),
            //         rest,
            //         rest->curline);
            //
            // prevPair->cdr = fklCreateNastNode(FKL_NAST_NIL, rest->curline);
            //
            // fklNastNodeQueuePush2(queue, restExp);
            //
            // prevPair = rest->pair;
            FklVMvalue *new_rest =
                    fklCreateVMvaluePair(&codegen->ctx->gc->gcvm, head, rest);
            put_line_number(&codegen->ctx->lnt->ht, new_rest, CURLINE(rest));
            cgExpQueuePush2(queue,
                    (FklPmatchRes){
                        .value = new_rest,
                        .container = new_rest,
                    });
            FKL_VM_CDR(prev) = FKL_VM_NIL;
            prev = rest;
        }
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_begin_exp_bc_process,
                createDefaultStackContext(),
                createDefaultQueueNextExpression(queue),
                scope,
                macro_scope,
                env,
                CURLINE(orig),
                // orig->curline,
                codegen,
                actions);
    }

    char *filename = combineFileNameFromListAndCheckPrivate(name);

    if (filename == NULL) {
        error_state->type = FKL_ERR_IMPORTFAILED;
        // error_state->place = orig;
        error_state->p = PLACE(orig, CURLINE(orig));
        return;
    }

    char *packageMainFileName =
            fklStrCat(fklZstrdup(filename), FKL_PATH_SEPARATOR_STR);
    packageMainFileName = fklStrCat(packageMainFileName, FKL_PACKAGE_MAIN_FILE);

    char *preCompileFileName = fklStrCat(fklZstrdup(packageMainFileName),
            FKL_PRE_COMPILE_FKL_SUFFIX_STR);

    char *scriptFileName =
            fklStrCat(fklZstrdup(filename), FKL_SCRIPT_FILE_EXTENSION);

    char *dllFileName = fklStrCat(fklZstrdup(filename), FKL_DLL_FILE_TYPE);

    if (fklIsAccessibleRegFile(packageMainFileName))
        process_import_script_common_header(orig,
                name,
                packageMainFileName,
                env,
                codegen,
                error_state,
                actions,
                scope,
                macro_scope,
                prefix,
                only,
                alias,
                except);
    else if (fklIsAccessibleRegFile(scriptFileName))
        process_import_script_common_header(orig,
                name,
                scriptFileName,
                env,
                codegen,
                error_state,
                actions,
                scope,
                macro_scope,
                prefix,
                only,
                alias,
                except);
    else if (fklIsAccessibleRegFile(preCompileFileName)) {
        size_t libId = check_loaded_lib(preCompileFileName, codegen->libraries);
        if (!libId) {
            FILE *fp = fopen(preCompileFileName, "rb");
            if (fp == NULL) {
                error_state->type = FKL_ERR_IMPORTFAILED;
                // error_state->place = name;
                error_state->p = PLACE(name, CURLINE(orig));
                goto exit;
            }

            FklCodegenCtx *ctx = codegen->ctx;

            FklLoadPreCompileArgs args = {
                .ctx = ctx,
                .libraries = codegen->libraries,
                .macro_libraries = &ctx->macro_libraries,
                .pts = codegen->pts,
                .macro_pts = ctx->pts,
            };

            if (fklLoadPreCompile(fp, preCompileFileName, &args)) {
                if (args.error) {
                    fprintf(stderr, "%s\n", args.error);
                    fklZfree(args.error);
                    args.error = NULL;
                }

                error_state->type = FKL_ERR_IMPORTFAILED;
                // error_state->place = name;
                error_state->p = PLACE(name, CURLINE(orig));
                goto exit;
            }

            libId = codegen->libraries->size;
        }
        const FklCodegenLib *lib = &codegen->libraries->base[libId - 1];

        FklByteCodelnt *importBc = process_import_imported_lib(libId,
                codegen,
                lib,
                env,
                scope,
                macro_scope,
                CURLINE(orig),
                // orig->curline,
                prefix,
                only,
                alias,
                except,
                error_state);
        FklCodegenAction *action = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                NULL,
                1,
                codegen->global_env->macros,
                codegen->global_env,
                CURLINE(orig),
                // orig->curline,
                NULL,
                codegen);
        fklByteCodelntVectorPushBack2(&action->bcl_vector, importBc);
        fklCodegenActionVectorPushBack2(actions, action);
    } else if (fklIsAccessibleRegFile(dllFileName)) {
        FklByteCodelnt *bc = process_import_from_dll(orig,
                name,
                dllFileName,
                env,
                codegen,
                error_state,
                scope,
                prefix,
                only,
                alias,
                except);
        if (bc) {
            FklCodegenAction *action = create_cg_action(_default_bc_process,
                    createDefaultStackContext(),
                    NULL,
                    1,
                    NULL,
                    NULL,
                    CURLINE(orig),
                    // orig->curline,
                    NULL,
                    codegen);
            fklByteCodelntVectorPushBack2(&action->bcl_vector, bc);
            fklCodegenActionVectorPushBack2(actions, action);
        }
    } else {
        error_state->type = FKL_ERR_IMPORTFAILED;
        // error_state->place = name;
        error_state->p = PLACE(name, CURLINE(orig));
    }
exit:
    fklZfree(filename);
    fklZfree(scriptFileName);
    fklZfree(dllFileName);
    fklZfree(packageMainFileName);
    fklZfree(preCompileFileName);
}

static CODEGEN_FUNC(codegen_import) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);

    codegen_import_helper(orig->value,
            name->value,
            rest->value,
            codegen,
            error_state,
            env,
            scope,
            macro_scope,
            actions,
            NULL,
            NULL,
            NULL,
            NULL);
}

static CODEGEN_FUNC(codegen_import_none) {
    if (must_has_retval) {
        error_state->type = FKL_ERR_EXP_HAS_NO_VALUE;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        return;
    }
    fklCodegenActionVectorPushBack2(actions,
            create_cg_action(_empty_bc_process,
                    createDefaultStackContext(),
                    NULL,
                    1,
                    macro_scope,
                    env,
                    CURLINE(orig->container),
                    // orig->curline,
                    NULL,
                    codegen));
}

static CODEGEN_FUNC(codegen_import_prefix) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *prefix =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);

    codegen_import_helper(orig->value,
            name->value,
            rest->value,
            codegen,
            error_state,
            env,
            scope,
            macro_scope,
            actions,
            prefix->value,
            NULL,
            NULL,
            NULL);
}

static CODEGEN_FUNC(codegen_import_only) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *only =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);

    codegen_import_helper(orig->value,
            name->value,
            rest->value,
            codegen,
            error_state,
            env,
            scope,
            macro_scope,
            actions,
            NULL,
            only->value,
            NULL,
            NULL);
}

static CODEGEN_FUNC(codegen_import_alias) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *alias =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);

    codegen_import_helper(orig->value,
            name->value,
            rest->value,
            codegen,
            error_state,
            env,
            scope,
            macro_scope,
            actions,
            NULL,
            NULL,
            alias->value,
            NULL);
}

static CODEGEN_FUNC(codegen_import_except) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *except =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);

    codegen_import_helper(orig->value,
            name->value,
            rest->value,
            codegen,
            error_state,
            env,
            scope,
            macro_scope,
            actions,
            NULL,
            NULL,
            NULL,
            except->value);
}

static CODEGEN_FUNC(codegen_import_common) {
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_args);

    codegen_import_helper(orig->value,
            name->value,
            rest->value,
            codegen,
            error_state,
            env,
            scope,
            macro_scope,
            actions,
            NULL,
            NULL,
            NULL,
            NULL);
}

typedef struct {
    FklVMvalue *pattern;
    FklVMvalueCodegenMacroScope *macro_scope;
    uint32_t prototype_id;
} MacroContext;

static inline void init_macro_context(MacroContext *r,
        FklVMvalue *pattern,
        FklVMvalueCodegenMacroScope *macro_scope,
        uint32_t prototype_id) {
    r->pattern = pattern;
    r->macro_scope = macro_scope;
    r->prototype_id = prototype_id;
}

BC_PROCESS(_compiler_macro_bc_process) {
    fklUpdatePrototype(&codegen->pts->p, env);
    fklPrintUndefinedRef(env->prev);

    MacroContext *d = FKL_TYPE_CAST(MacroContext *, data);
    FklByteCodelnt *macroBcl = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    FklInstruction ret = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(macroBcl, &ret, fid, line, scope);

    FklVMvalue *pattern = d->pattern;
    FklVMvalueCodegenMacroScope *macros = d->macro_scope;
    uint32_t prototype_id = d->prototype_id;

    fklPeepholeOptimize(macroBcl);
    add_compiler_macro(&macros->head,
            pattern,
            // fklMakeNastNodeRef(d->origin_exp),
            macroBcl,
            prototype_id);
    fklDestroyByteCodelnt(macroBcl);
    return NULL;
}

static void _macro_stack_context_atomic(FklVMgc *gc, void *data) {
    MacroContext *d = data;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->macro_scope), gc);
    fklVMgcToGray(d->pattern, gc);
}

static const FklCodegenActionContextMethodTable MacroStackContextMethodTable = {
    .size = sizeof(MacroContext),
    .__atomic = _macro_stack_context_atomic,
};

static inline FklCodegenActionContext *createMacroActionContext(
        FklVMvalue *origin_exp,
        FklVMvalue *pattern,
        FklVMvalueCodegenMacroScope *macro_scope,
        uint32_t prototype_id) {
    FklCodegenActionContext *r =
            createCodegenActionContext(&MacroStackContextMethodTable);
    init_macro_context(FKL_TYPE_CAST(MacroContext *, r->d),
            pattern,
            macro_scope,
            prototype_id);
    return r;
}

struct ReaderMacroCtx {
    FklVMvalueCustomActionCtx *action_ctx;
};

static inline void init_reader_macro_context(struct ReaderMacroCtx *r,
        FklVMvalueCustomActionCtx *ctx) {
    r->action_ctx = ctx;
}

BC_PROCESS(_reader_macro_bc_process) {
    struct ReaderMacroCtx *d = FKL_TYPE_CAST(struct ReaderMacroCtx *, data);
    FklVMvalueCustomActionCtx *custom_ctx = d->action_ctx;
    d->action_ctx = NULL;

    // FklVMobarray *pst = &ctx->public_st;
    fklUpdatePrototype(&codegen->pts->p, env);
    fklPrintUndefinedRef(env->prev);

    FklByteCodelnt *macroBcl = *fklByteCodelntVectorPopBackNonNull(bcl_vec);
    FklInstruction ret = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(macroBcl, &ret, fid, line, scope);

    fklPeepholeOptimize(macroBcl);
    custom_ctx->c.bcl = macroBcl;
    return NULL;
}

static void _reader_macro_stack_context_atomic(FklVMgc *gc, void *data) {
    struct ReaderMacroCtx *d = (struct ReaderMacroCtx *)data;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->action_ctx), gc);
}

static const FklCodegenActionContextMethodTable
        ReaderMacroStackContextMethodTable = {
            .size = sizeof(struct ReaderMacroCtx),
            .__atomic = _reader_macro_stack_context_atomic,
        };

static inline FklCodegenActionContext *createReaderMacroActionContext(
        FklVMvalueCustomActionCtx *ctx) {
    FklCodegenActionContext *r =
            createCodegenActionContext(&ReaderMacroStackContextMethodTable);

    init_reader_macro_context(FKL_TYPE_CAST(struct ReaderMacroCtx *, r->d),
            ctx);

    return r;
}

static FklVMvalue *_nil_replacement(const FklPmatchRes *orig,
        const FklVMvalueCodegenEnv *env,
        const FklVMvalueCodegenInfo *codegen) {
    // return fklCreateNastNode(FKL_NAST_NIL, orig->curline);
    return FKL_VM_NIL;
}

static FklVMvalue *_is_main_replacement(const FklPmatchRes *orig,
        const FklVMvalueCodegenEnv *env,
        const FklVMvalueCodegenInfo *codegen) {
    if (env->prototypeId == 1)
        return FKL_VM_TRUE;
    else
        return FKL_VM_NIL;
    // FklNastNode *n = fklCreateNastNode(FKL_NAST_FIX, orig->curline);
    // if (env->prototypeId == 1)
    //     n->fix = 1;
    // else
    //     n->type = FKL_NAST_NIL;
    // return n;
}

static FklVMvalue *_platform_replacement(const FklPmatchRes *orig,
        const FklVMvalueCodegenEnv *env,
        const FklVMvalueCodegenInfo *codegen) {

#if __linux__
    static const char *platform = "linux";
#elif __unix__
    static const char *platform = "unix";
#elif defined _WIN32
    static const char *platform = "win32";
#elif __APPLE__
    static const char *platform = "apple";
#endif
    // FklNastNode *n = fklCreateNastNode(FKL_NAST_STR, orig->curline);
    // n->str = fklCreateStringFromCstr(platform);
    // return n;

    return fklCreateVMvalueStrFromCstr(&codegen->ctx->gc->gcvm, platform);
}

static FklVMvalue *_file_dir_replacement(const FklPmatchRes *orig,
        const FklVMvalueCodegenEnv *env,
        const FklVMvalueCodegenInfo *codegen) {
    // FklString *s = NULL;
    // FklNastNode *r = fklCreateNastNode(FKL_NAST_STR, orig->curline);
    // if (codegen->filename == NULL)
    //     s = fklCreateStringFromCstr(codegen->ctx->cwd);
    // else
    //     s = fklCreateStringFromCstr(codegen->dir);
    // fklStringCstrCat(&s, FKL_PATH_SEPARATOR_STR);
    // r->str = s;
    // return r;

    const char *d = codegen->filename == NULL //
                          ? codegen->ctx->cwd
                          : codegen->dir;

    const size_t d_len = strlen(d);
    const size_t s_len = strlen(FKL_PATH_SEPARATOR_STR);

    FklVMvalue *r =
            fklCreateVMvalueStr2(&codegen->ctx->gc->gcvm, s_len + d_len, NULL);
    memcpy(FKL_VM_STR(r)->str, d, d_len);
    strcat(FKL_VM_STR(r)->str, FKL_PATH_SEPARATOR_STR);

    return r;
}

static FklVMvalue *_file_replacement(const FklPmatchRes *orig,
        const FklVMvalueCodegenEnv *env,
        const FklVMvalueCodegenInfo *codegen) {
    // FklNastNode *r = NULL;
    // if (codegen->filename == NULL)
    //     r = fklCreateNastNode(FKL_NAST_NIL, orig->curline);
    // else {
    //     r = fklCreateNastNode(FKL_NAST_STR, orig->curline);
    //     FklString *s = NULL;
    //     s = fklCreateStringFromCstr(codegen->filename);
    //     r->str = s;
    // }
    // return r;

    return codegen->filename == NULL
                 ? FKL_VM_NIL
                 : fklCreateVMvalueStrFromCstr(&codegen->ctx->gc->gcvm,
                           codegen->filename);
}

static FklVMvalue *_file_rp_replacement(const FklPmatchRes *orig,
        const FklVMvalueCodegenEnv *env,
        const FklVMvalueCodegenInfo *codegen) {
    // FklNastNode *r = NULL;
    // if (codegen->realpath == NULL)
    //     r = fklCreateNastNode(FKL_NAST_NIL, orig->curline);
    // else {
    //     r = fklCreateNastNode(FKL_NAST_STR, orig->curline);
    //     FklString *s = NULL;
    //     s = fklCreateStringFromCstr(codegen->realpath);
    //     r->str = s;
    // }
    // return r;

    return codegen->realpath == NULL
                 ? FKL_VM_NIL
                 : fklCreateVMvalueStrFromCstr(&codegen->ctx->gc->gcvm,
                           codegen->realpath);
}

static FklVMvalue *_line_replacement(const FklPmatchRes *orig,
        const FklVMvalueCodegenEnv *env,
        const FklVMvalueCodegenInfo *codegen) {
    // uint64_t line = orig->curline;
    // FklNastNode *r = NULL;
    // if (line < FKL_FIX_INT_MAX) {
    //     r = fklCreateNastNode(FKL_NAST_FIX, orig->curline);
    //     r->fix = line;
    // } else {
    //     r = fklCreateNastNode(FKL_NAST_BIGINT, orig->curline);
    //     r->bigInt = fklCreateBigIntU(line);
    // }
    // return r;

    uint64_t line = CURLINE(orig->container);
    return line <= FKL_FIX_INT_MAX
                 ? FKL_MAKE_VM_FIX(line)
                 : fklCreateVMvalueBigIntWithU64(&codegen->ctx->gc->gcvm, line);
}

static struct SymbolReplacement {
    const char *s;
    ReplacementFunc func;
} builtInSymbolReplacement[FKL_BUILTIN_REPLACEMENT_NUM] = {
    {
        "nil",
        _nil_replacement,
    },
    {
        "*line*",
        _line_replacement,
    },
    {
        "*file*",
        _file_replacement,
    },
    {
        "*file-rp*",
        _file_rp_replacement,
    },
    {
        "*file-dir*",
        _file_dir_replacement,
    },
    {
        "*main?*",
        _is_main_replacement,
    },
    {
        "*platform*",
        _platform_replacement,
    },
};

static inline ReplacementFunc findBuiltInReplacementWithId(FklVMvalue *id,
        FklVMvalue *const builtin_replacement_id[]) {
    for (size_t i = 0; i < FKL_BUILTIN_REPLACEMENT_NUM; i++) {
        if (builtin_replacement_id[i] == id)
            return builtInSymbolReplacement[i].func;
    }
    return NULL;
}

typedef enum {
    NAST_TO_GRAMMER_SYM_ERR_DUMMY = 0,
    NAST_TO_GRAMMER_SYM_ERR_INVALID,
    NAST_TO_GRAMMER_SYM_ERR_REGEX_COMPILE_FAILED,
    NAST_TO_GRAMMER_SYM_ERR_UNRESOLVED_BUILTIN,
    NAST_TO_GRAMMER_SYM_ERR_BUILTIN_TERMINAL_INIT_FAILED,
    NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_TYPE,
    NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST,
} NastToGrammerSymErr;

static inline const char *get_nast_to_grammer_sym_err_msg(
        NastToGrammerSymErr err) {
    FklBuiltinTerminalInitError builtin_terminal_err = err >> CHAR_BIT;
    err &= 0xff;

    switch (err) {
    case NAST_TO_GRAMMER_SYM_ERR_DUMMY:
        FKL_UNREACHABLE();
        break;
    case NAST_TO_GRAMMER_SYM_ERR_INVALID:
        return "invalid syntax";
        break;
    case NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST:
        return "invalid action syntax";
        break;
    case NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_TYPE:
        return "invalid action type";
        break;
    case NAST_TO_GRAMMER_SYM_ERR_BUILTIN_TERMINAL_INIT_FAILED:
        switch (builtin_terminal_err) {
        case FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY:
            FKL_UNREACHABLE();
            break;
        case FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_MANY_ARGS:
            return "init builtin terminal with too many arguments";
            break;
        case FKL_BUILTIN_TERMINAL_INIT_ERR_TOO_FEW_ARGS:
            return "init builtin terminal with too few arguments";
            break;
        }
        return fklBuiltinTerminalInitErrorToCstr(builtin_terminal_err);
        break;
    case NAST_TO_GRAMMER_SYM_ERR_UNRESOLVED_BUILTIN:
        return "unresolved builtin terminal";
        break;
    case NAST_TO_GRAMMER_SYM_ERR_REGEX_COMPILE_FAILED:
        return "failed to compile regex";
        break;
    }

    return NULL;
}

static inline NastToGrammerSymErr nast_vector_to_builtin_terminal(
        const FklVMvalue *vec,
        FklGrammerSym *s,
        FklGrammer *g) {
    if (FKL_VM_VEC(vec)->size == 0)
        return NAST_TO_GRAMMER_SYM_ERR_INVALID;

    FklVMvalue *first = FKL_VM_VEC(vec)->base[0];
    if (!FKL_IS_SYM(first))
        return NAST_TO_GRAMMER_SYM_ERR_INVALID;

    const FklString *builtin_term_name = FKL_VM_SYM(first);
    if (fklCharBufMatch(FKL_PG_SPECIAL_PREFIX,
                sizeof(FKL_PG_SPECIAL_PREFIX) - 1,
                builtin_term_name->str,
                builtin_term_name->size)
            == sizeof(FKL_PG_SPECIAL_PREFIX) - 1) {

        const FklLalrBuiltinMatch *builtin_terminal =
                fklGetBuiltinMatch(&g->builtins, first);
        if (builtin_terminal == NULL)
            return NAST_TO_GRAMMER_SYM_ERR_UNRESOLVED_BUILTIN;

        for (size_t i = 1; i < FKL_VM_VEC(vec)->size; ++i) {
            if (!FKL_IS_STR(FKL_VM_VEC(vec)->base[i]))
                return NAST_TO_GRAMMER_SYM_ERR_INVALID;
        }

        FklString const **args =
                fklZmalloc((FKL_VM_VEC(vec)->size - 1) * sizeof(FklString *));

        for (size_t i = 1; i < FKL_VM_VEC(vec)->size; ++i)
            args[i - 1] = fklAddString(&g->terminals,
                    FKL_VM_STR(FKL_VM_VEC(vec)->base[i]));

        s->type = FKL_TERM_BUILTIN;
        s->b.t = builtin_terminal;
        s->b.len = 0;
        s->b.args = NULL;

        FklBuiltinTerminalInitError err = FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY;
        if (s->b.t->ctx_create)
            err = s->b.t->ctx_create(FKL_VM_VEC(vec)->size - 1, args, g);
        if (err) {
            fklZfree(args);
            return NAST_TO_GRAMMER_SYM_ERR_BUILTIN_TERMINAL_INIT_FAILED
                 | (err << CHAR_BIT);
        }
        s->b.len = FKL_VM_VEC(vec)->size - 1;
        s->b.args = args;

        return NAST_TO_GRAMMER_SYM_ERR_DUMMY;
    } else {
        return NAST_TO_GRAMMER_SYM_ERR_INVALID;
    }
}

static inline NastToGrammerSymErr nast_node_to_grammer_sym(
        const FklVMvalue *node,
        FklGrammerSym *s,
        FklGrammer *g) {
    if (FKL_IS_VECTOR(node)) {
        return nast_vector_to_builtin_terminal(node, s, g);
    } else if (FKL_IS_BYTEVECTOR(node)) {
        FklBytevector *bytes = FKL_VM_BVEC(node);
        s->type = FKL_TERM_KEYWORD;
        s->str = fklAddStringCharBuf(&g->terminals,
                FKL_TYPE_CAST(const char *, bytes->ptr),
                bytes->size);
    } else if (FKL_IS_BOX(node)) {
        FklVMvalue *v = FKL_VM_BOX(node);
        if (!FKL_IS_STR(v))
            return NAST_TO_GRAMMER_SYM_ERR_INVALID;
        s->type = FKL_TERM_REGEX;
        s->re = fklAddRegexStr(&g->regexes, FKL_VM_STR(v));
        if (s->re == NULL)
            return NAST_TO_GRAMMER_SYM_ERR_REGEX_COMPILE_FAILED;
    } else if (FKL_IS_STR(node)) {
        s->type = FKL_TERM_STRING;
        s->str = fklAddString(&g->terminals, FKL_VM_STR(node));
        fklAddString(&g->delimiters, FKL_VM_STR(node));
    } else if (FKL_IS_PAIR(node)) {
        FklVMvalue *car = FKL_VM_CAR(node);
        FklVMvalue *cdr = FKL_VM_CDR(node);
        if (!FKL_IS_SYM(car) || !FKL_IS_SYM(cdr))
            return NAST_TO_GRAMMER_SYM_ERR_INVALID;
        s->type = FKL_TERM_NONTERM;
        s->nt.group = car; // node->pair->car->sym;
        s->nt.sid = cdr;   // node->pair->cdr->sym;
    } else if (FKL_IS_SYM(node)) {
        const FklString *str = FKL_VM_SYM(node);
        if (fklCharBufMatch(FKL_PG_SPECIAL_PREFIX,
                    sizeof(FKL_PG_SPECIAL_PREFIX) - 1,
                    str->str,
                    str->size)
                == sizeof(FKL_PG_SPECIAL_PREFIX) - 1) {
            const FklLalrBuiltinMatch *builtin_terminal =
                    fklGetBuiltinMatch(&g->builtins,
                            FKL_TYPE_CAST(FklVMvalue *, node));
            if (builtin_terminal) {
                s->type = FKL_TERM_BUILTIN;
                s->b.t = builtin_terminal;
                s->b.args = NULL;
                s->b.len = 0;
                FklBuiltinTerminalInitError err =
                        FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY;
                if (s->b.t->ctx_create)
                    err = s->b.t->ctx_create(s->b.len, s->b.args, g);
                if (err) {
                    return NAST_TO_GRAMMER_SYM_ERR_BUILTIN_TERMINAL_INIT_FAILED
                         | (err << CHAR_BIT);
                }
            } else {
                return NAST_TO_GRAMMER_SYM_ERR_UNRESOLVED_BUILTIN;
            }
        } else {
            s->type = FKL_TERM_NONTERM;
            s->nt.group = 0;
            s->nt.sid = FKL_TYPE_CAST(FklVMvalue *, node);
        }
    } else {
        return NAST_TO_GRAMMER_SYM_ERR_INVALID;
    }

    return NAST_TO_GRAMMER_SYM_ERR_DUMMY;
    // switch (node->type) {
    // case FKL_NAST_VECTOR:
    //     return nast_vector_to_builtin_terminal(node->vec, s, g);
    //     break;
    // case FKL_NAST_BYTEVECTOR:
    //     s->type = FKL_TERM_KEYWORD;
    //     s->str = fklAddStringCharBuf(&g->terminals,
    //             FKL_TYPE_CAST(const char *, node->bvec->ptr),
    //             node->bvec->size);
    //     break;
    // case FKL_NAST_BOX:
    //     if (node->box->type != FKL_NAST_STR)
    //         return NAST_TO_GRAMMER_SYM_ERR_INVALID;
    //     s->type = FKL_TERM_REGEX;
    //     s->re = fklAddRegexStr(&g->regexes, node->box->str);
    //     if (s->re == NULL)
    //         return NAST_TO_GRAMMER_SYM_ERR_REGEX_COMPILE_FAILED;
    //     break;
    // case FKL_NAST_STR:
    //     s->type = FKL_TERM_STRING;
    //     s->str = fklAddString(&g->terminals, node->str);
    //     fklAddString(&g->delimiters, node->str);
    //     break;
    // case FKL_NAST_PAIR:
    //     s->type = FKL_TERM_NONTERM;
    //     s->nt.group = node->pair->car->sym;
    //     s->nt.sid = node->pair->cdr->sym;
    //     break;
    // case FKL_NAST_SYM: {
    //     const FklString *str = fklGetSymbolWithId(node->sym, g->st);
    //     if (fklCharBufMatch(FKL_PG_SPECIAL_PREFIX,
    //                 sizeof(FKL_PG_SPECIAL_PREFIX) - 1,
    //                 str->str,
    //                 str->size)
    //             == sizeof(FKL_PG_SPECIAL_PREFIX) - 1) {
    //         const FklLalrBuiltinMatch *builtin_terminal =
    //                 fklGetBuiltinMatch(&g->builtins, node->sym);
    //         if (builtin_terminal) {
    //             s->type = FKL_TERM_BUILTIN;
    //             s->b.t = builtin_terminal;
    //             s->b.args = NULL;
    //             s->b.len = 0;
    //             FklBuiltinTerminalInitError err =
    //                     FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY;
    //             if (s->b.t->ctx_create)
    //                 err = s->b.t->ctx_create(s->b.len, s->b.args, g);
    //             if (err) {
    //                 return
    //                 NAST_TO_GRAMMER_SYM_ERR_BUILTIN_TERMINAL_INIT_FAILED
    //                      | (err << CHAR_BIT);
    //             }
    //         } else {
    //             return NAST_TO_GRAMMER_SYM_ERR_UNRESOLVED_BUILTIN;
    //         }
    //     } else {
    //         s->type = FKL_TERM_NONTERM;
    //         s->nt.group = 0;
    //         s->nt.sid = node->sym;
    //     }
    // } break;
    //
    // case FKL_NAST_NIL:
    // case FKL_NAST_FIX:
    // case FKL_NAST_F64:
    // case FKL_NAST_BIGINT:
    // case FKL_NAST_CHR:
    // case FKL_NAST_SLOT:
    // case FKL_NAST_RC_SYM:
    // case FKL_NAST_HASHTABLE:
    //     return NAST_TO_GRAMMER_SYM_ERR_INVALID;
    //     break;
    // }
}

static inline int is_concat_sym(const FklString *str) {
    return fklStringCstrCmp(str, FKL_PG_TERM_CONCAT) == 0;
}

typedef struct {
    FklVMvalue *err_node;
    FklGrammer *g;
    FklGrammerSym *syms;
    size_t len;
    int adding_ignore;
} NastToGrammerSymArgs;

static inline NastToGrammerSymErr nast_vector_to_production_right_part(
        NastToGrammerSymArgs *args,
        const FklVMvalue *vec) {
    if (FKL_VM_VEC(vec)->size == 0) {
        args->len = 0;
        args->syms = NULL;
        return NAST_TO_GRAMMER_SYM_ERR_DUMMY;
    }

    FklGrammer *g = args->g;
    NastToGrammerSymErr err = NAST_TO_GRAMMER_SYM_ERR_DUMMY;
    FklGraSymVector gsym_vector;
    fklGraSymVectorInit(&gsym_vector, 2);

    int has_ignore = 0;
    for (size_t i = 0; i < FKL_VM_VEC(vec)->size; ++i) {
        FklGrammerSym s = { .type = FKL_TERM_STRING };
        FklVMvalue *cur = FKL_VM_VEC(vec)->base[i];
        if (FKL_IS_SYM(cur) && is_concat_sym(FKL_VM_SYM(cur))) {
            if (!has_ignore) {
                args->err_node = cur;
                err = NAST_TO_GRAMMER_SYM_ERR_INVALID;
                goto error_happened;
            } else {
                has_ignore = 0;
            }
            continue;
        } else {
            err = nast_node_to_grammer_sym(cur, &s, g);
            if (err) {
                args->err_node = cur;
                goto error_happened;
            }
        }

        if (has_ignore) {
            FklGrammerSym s = { .type = FKL_TERM_IGNORE };
            fklGraSymVectorPushBack(&gsym_vector, &s);
        }
        fklGraSymVectorPushBack(&gsym_vector, &s);
        has_ignore = !args->adding_ignore;
    }

    args->syms = (FklGrammerSym *)fklZmalloc(
            gsym_vector.size * sizeof(FklGrammerSym));
    FKL_ASSERT(args->syms);
    args->len = gsym_vector.size;

    for (size_t i = 0; i < args->len; ++i)
        args->syms[i] = gsym_vector.base[i];

    fklGraSymVectorUninit(&gsym_vector);

    return NAST_TO_GRAMMER_SYM_ERR_DUMMY;

error_happened:
    while (!fklGraSymVectorIsEmpty(&gsym_vector)) {
        FklGrammerSym *s = fklGraSymVectorPopBack(&gsym_vector);
        if (s->type == FKL_TERM_BUILTIN && s->b.len) {
            s->b.len = 0;
            fklZfree(s->b.args);
            s->b.args = NULL;
        }
    }
    fklGraSymVectorUninit(&gsym_vector);
    return err;
}

static inline FklGrammerIgnore *nast_vector_to_ignore(const FklVMvalue *vec,
        NastToGrammerSymArgs *args,
        NastToGrammerSymErr *perr) {

    args->adding_ignore = 1;
    NastToGrammerSymErr err = nast_vector_to_production_right_part(args, vec);
    *perr = err;
    if (err)
        return NULL;
    FklGrammerIgnore *ig = fklGrammerSymbolsToIgnore(args->syms, args->len);
    fklUninitGrammerSymbols(args->syms, args->len);

    args->len = 0;
    fklZfree(args->syms);
    args->syms = NULL;
    return ig;
}

static inline FklVMvalueCodegenInfo *macro_compile_prepare(
        FklVMvalueCodegenInfo *codegen,
        FklVMvalueCodegenMacroScope *macro_scope,
        FklVMvalueHashSet *symbolSet,
        FklVMvalueCodegenEnv **pmacroEnv) {
    FklVMvalueCodegenInfo *macroCodegen =
            fklCreateVMvalueCodegenInfo(codegen->ctx,
                    codegen,
                    NULL,
                    &(FklCodegenInfoArgs){
                        .is_macro = 1,
                        .macro_scope = macro_scope,
                    });

    FklVMvalueCodegenEnv *macro_main_env =
            fklCreateVMvalueCodegenEnv(codegen->ctx,
                    macroCodegen->global_env,
                    1,
                    macro_scope);

    for (FklVMvalueHashSetNode *list = symbolSet->first; list;
            list = list->next) {
        FklVMvalue *id = FKL_TYPE_CAST(FklVMvalue *, list->k);
        fklAddCodegenDefBySid(id, 1, macro_main_env);
    }
    *pmacroEnv = macro_main_env;
    return macroCodegen;
}

typedef struct {
    FklGrammerProduction *prod;
    FklVMvalue *sid;
    FklVMvalue *group_id;
    FklVMvalue *vec;
    uint8_t add_extra;
    uint8_t type;
    FklVMvalue *action;
} AddingProductionCtx;

static void _adding_production_ctx_finalizer(void *data) {
    AddingProductionCtx *ctx = data;
    if (ctx->prod) {
        fklDestroyGrammerProduction(ctx->prod);
        ctx->prod = NULL;
    }
    // fklDestroyNastNode(ctx->vec);
    ctx->vec = NULL;
    if (ctx->type != FKL_CODEGEN_PROD_CUSTOM) {
        // fklDestroyNastNode(ctx->action);
        ctx->action = NULL;
    }
}

static void _adding_production_ctx_atomic(FklVMgc *gc, void *data) {
    AddingProductionCtx *ctx = data;
    if (ctx->prod) {
        fklVMgcMarkGrammerProd(gc, ctx->prod, NULL);
    }
    // fklDestroyNastNode(ctx->vec);
    fklVMgcToGray(ctx->vec, gc);
    fklVMgcToGray(ctx->action, gc);
}

static const FklCodegenActionContextMethodTable
        AddingProductionCtxMethodTable = {
            .size = sizeof(AddingProductionCtx),
            .__finalizer = _adding_production_ctx_finalizer,
            .__atomic = _adding_production_ctx_atomic,
        };

static inline FklCodegenActionContext *createAddingProductionCtx(
        FklGrammerProduction *prod,
        FklVMvalue *sid,
        FklVMvalue *group_id,
        uint8_t add_extra,
        const FklVMvalue *vec,
        FklCodegenProdActionType type,
        const FklVMvalue *action_ast) {
    FklCodegenActionContext *r =
            createCodegenActionContext(&AddingProductionCtxMethodTable);
    AddingProductionCtx *ctx = FKL_TYPE_CAST(AddingProductionCtx *, r->d);
    ctx->prod = prod;
    ctx->sid = sid;
    ctx->group_id = group_id;
    ctx->add_extra = add_extra;
    ctx->vec = FKL_TYPE_CAST(FklVMvalue *, vec);
    ctx->type = type;
    ctx->action = FKL_TYPE_CAST(FklVMvalue *, action_ast);
    return r;
}

static inline int check_group_outer_ref(FklVMvalueCodegenInfo *codegen,
        const FklProdHashMap *productions,
        FklVMvalue *group_id) {
    int is_ref_outer = 0;
    for (FklProdHashMapNode *item = productions->first; item;
            item = item->next) {
        for (FklGrammerProduction *prods = item->v; prods;
                prods = prods->next) {
            for (size_t i = 0; i < prods->len; i++) {
                FklGrammerSym *sym = &prods->syms[i];
                if (sym->type == FKL_TERM_NONTERM) {
                    if (sym->nt.group)
                        is_ref_outer |= sym->nt.group != group_id;
                    else {
                        FklGrammerNonterm left = { .group = group_id,
                            .sid = sym->nt.sid };
                        if (fklProdHashMapGet(productions, &left))
                            sym->nt.group = group_id;
                        else if (fklProdHashMapGet(
                                         &codegen->unnamed_g->productions,
                                         &left))
                            is_ref_outer = 1;
                    }
                }
            }
        }
    }
    return is_ref_outer;
}

static inline int has_outer_ref(const FklVMvalueCodegenInfo *c,
        FklVMvalue *id) {
    return (c->export_named_prod_groups
            && fklVMvalueHashSetHas2(c->export_named_prod_groups, id)
            && (FKL_GRAMMER_GROUP_HAS_OUTER_REF
                       & fklGraProdGroupHashMapGet2(c->named_prod_groups, id)
                               ->flags)
                       != 0);
}

BC_PROCESS(process_adding_production) {
    AddingProductionCtx *prod_ctx = FKL_TYPE_CAST(AddingProductionCtx *, data);
    FklVMvalue *group_id = prod_ctx->group_id;
    FklGrammerProduction *prod = prod_ctx->prod;
    prod_ctx->prod = NULL;
    FklGrammer *g = group_id == 0
                          ? codegen->unnamed_g
                          : &add_production_group(codegen->named_prod_groups,
                                    codegen->ctx->gc,
                                    group_id)
                                     ->g;
    FklVMvalue *sid = prod_ctx->sid;
    if (group_id == 0) {
        if (fklAddProdToProdTableNoRepeat(g, prod)) {
            fklDestroyGrammerProduction(prod);
        reader_macro_error:
            error_state->type = FKL_ERR_GRAMMER_CREATE_FAILED;
            // error_state->line = line;
            error_state->p = PLACE(NULL, line);
            return NULL;
        }
        if (prod_ctx->add_extra) {
            FklGrammerProduction *extra_prod =
                    fklCreateExtraStartProduction(codegen->ctx, group_id, sid);
            if (fklAddProdToProdTable(g, extra_prod)) {
                fklDestroyGrammerProduction(extra_prod);
                goto reader_macro_error;
            }
        }
    } else {
        FklGrammerProdGroupItem *group =
                add_production_group(codegen->named_prod_groups,
                        codegen->ctx->gc,
                        group_id);
        if (fklAddProdToProdTableNoRepeat(&group->g, prod)) {
            fklDestroyGrammerProduction(prod);
            goto reader_macro_error;
        }
        if (prod_ctx->add_extra) {
            FklGrammerProduction *extra_prod =
                    fklCreateExtraStartProduction(codegen->ctx, group_id, sid);
            if (fklAddProdToProdTable(&group->g, extra_prod)) {
                fklDestroyGrammerProduction(extra_prod);
                goto reader_macro_error;
            }
        }

        if (check_group_outer_ref(codegen, &group->g.productions, group_id))
            group->flags |= FKL_GRAMMER_GROUP_HAS_OUTER_REF;
    }

    if (has_outer_ref(codegen, group_id)) {
        error_state->type = FKL_ERR_EXPORT_OUTER_REF_PROD_GROUP;
        // error_state->line = line;
        // error_state->place = NULL;
        error_state->p = PLACE(NULL, line);
        return NULL;
    }
    return NULL;
}

typedef struct {
    uint32_t line;
    uint8_t add_extra;
    FklVMvalue *left_group;
    FklVMvalue *left_sid;
    FklVMvalue *action_type;
    FklVMvalue *action_ast;
    FklVMvalue *group_id;
    FklVMvalueCodegenInfo *codegen;
    FklVMvalueCodegenEnv *env;
    FklVMvalueCodegenMacroScope *macro_scope;
    FklCodegenActionVector *actions;
    FklCodegenCtx *ctx;
    FklGrammer *g;

    FklVMvalue *err_node;
    FklGrammerProduction *production;
} NastToProductionArgs;

static inline NastToGrammerSymErr
nast_vector_to_production(const FklVMvalue *vec, NastToProductionArgs *args) {
    // const FklNastVector *vec = vec_node->vec;

    NastToGrammerSymArgs other_args = {
        .g = args->g,
    };
    NastToGrammerSymErr err =
            nast_vector_to_production_right_part(&other_args, vec);
    if (err) {
        args->err_node = other_args.err_node;
        other_args.len = 0;
        fklZfree(other_args.syms);
        other_args.syms = NULL;
        return err;
    }

    FklVMvalue *action_type = args->action_type;
    FklVMvalueCodegenInfo *codegen = args->codegen;
    FklVMvalue *action_ast = args->action_ast;
    FklVMvalue *left_sid = args->left_sid;
    FklVMvalue *left_group = args->left_group;
    FklVMvalue *group_id = args->group_id;
    uint8_t add_extra = args->add_extra;

    if (action_type == codegen->ctx->builtInPatternVar_builtin) {
        if (!FKL_IS_SYM(action_ast)) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }
        FklGrammerProduction *prod = fklCreateBuiltinActionProd(codegen->ctx,
                left_group,
                left_sid,
                other_args.len,
                other_args.syms,
                action_ast);

        if (prod == NULL) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(process_adding_production,
                createAddingProductionCtx(prod,
                        left_sid,
                        group_id,
                        add_extra,
                        vec,
                        FKL_CODEGEN_PROD_BUILTIN,
                        action_ast),
                NULL,
                1,
                args->macro_scope,
                args->env,
                args->line,
                codegen,
                args->actions);
        args->production = prod;
    } else if (action_type == codegen->ctx->builtInPatternVar_simple) {
        if (!FKL_IS_VECTOR(action_ast)               //
                || FKL_VM_VEC(action_ast)->size == 0 //
                || !FKL_IS_SYM(FKL_VM_VEC(action_ast)->base[0])) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }

        // FklVMvalue *action_ctx =
        //         fklCreateSimpleProdActionCtx(codegen->ctx, action_ast);

        // const struct FklSimpleProdAction *action =
        //         find_simple_prod_action(FKL_VM_VEC(action_ast)->base[0],
        //                 codegen->ctx->simple_prod_action_id);
        // if (action_ctx == NULL) {
        //     args->err_node = action_ast;
        //     err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
        //     goto error_happened;
        // }
        // int failed = 0;
        // void *ctx = action->initor(&FKL_VM_VEC(action_ast)->base[1],
        //         FKL_VM_VEC(action_ast)->size - 1,
        //         &failed);
        // if (failed) {
        //     args->err_node = action_ast;
        //     err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
        //     goto error_happened;
        // }

        FklGrammerProduction *prod = fklCreateSimpleActionProd(codegen->ctx,
                left_group,
                left_sid,
                other_args.len,
                other_args.syms,
                action_ast);

        if (prod == NULL) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(process_adding_production,
                createAddingProductionCtx(prod,
                        left_sid,
                        group_id,
                        add_extra,
                        vec,
                        FKL_CODEGEN_PROD_SIMPLE,
                        action_ast),
                NULL,
                1,
                args->macro_scope,
                args->env,
                args->line,
                codegen,
                args->actions);
        args->production = prod;
    } else if (action_type == codegen->ctx->builtInPatternVar_custom) {
        FklVMvalueHashSet symbolSet;
        fklVMvalueHashSetInit(&symbolSet);
        fklVMvalueHashSetPut2(&symbolSet, add_symbol_cstr(args->ctx, "$$"));
        FklVMvalueCodegenEnv *macroEnv = NULL;
        FklVMvalueCodegenInfo *macroCodegen = macro_compile_prepare(codegen,
                args->macro_scope,
                &symbolSet,
                &macroEnv);
        fklVMvalueHashSetUninit(&symbolSet);

        create_and_insert_to_pool(macroCodegen,
                0,
                macroEnv,
                0,
                // action_ast->curline,
                CURLINE(action_ast));
        CgExpQueue *queue = cgExpQueueCreate();
        int failed = 0;
        if (failed) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }

        FklGrammerProduction *prod = fklCreateCustomActionProd(codegen->ctx,
                left_group,
                left_sid,
                other_args.len,
                other_args.syms,
                macroEnv->prototypeId);

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(process_adding_production,
                createAddingProductionCtx(prod,
                        left_sid,
                        group_id,
                        add_extra,
                        vec,
                        FKL_CODEGEN_PROD_CUSTOM,
                        NULL),
                NULL,
                1,
                args->macro_scope,
                args->env,
                args->line,
                codegen,
                args->actions);
        cgExpQueuePush2(queue,
                (FklPmatchRes){
                    .value = action_ast,
                    .container = action_ast,
                });
        // fklMakeNastNodeRef(FKL_TYPE_CAST(FklNastNode *, action_ast)));
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_reader_macro_bc_process,
                createReaderMacroActionContext(prod->ctx),
                createMustHasRetvalQueueNextExpression(queue),
                1,
                macroEnv->macros,
                macroEnv,
                CURLINE(action_ast),
                // action_ast->curline,
                macroCodegen,
                args->actions);
        args->production = prod;
    } else if (action_type == codegen->ctx->builtInPatternVar_replace) {
        FklGrammerProduction *prod = fklCreateReplaceActionProd(left_group,
                left_sid,
                other_args.len,
                other_args.syms,
                action_ast);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(process_adding_production,
                createAddingProductionCtx(prod,
                        left_sid,
                        group_id,
                        add_extra,
                        vec,
                        FKL_CODEGEN_PROD_REPLACE,
                        action_ast),
                NULL,
                1,
                args->macro_scope,
                args->env,
                args->line,
                codegen,
                args->actions);
        args->production = prod;
    } else {
        args->err_node = NULL;
        err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_TYPE;
    error_happened:
        other_args.len = 0;
        fklZfree(other_args.syms);
        other_args.syms = NULL;
        return err;
    }

    other_args.len = 0;
    fklZfree(other_args.syms);
    other_args.syms = NULL;
    return NAST_TO_GRAMMER_SYM_ERR_DUMMY;
}

static inline int process_add_production(FklVMvalue *group_id,
        FklVMvalueCodegenInfo *codegen,
        FklVMvalue *vector_node,
        FklCodegenErrorState *error_state,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenMacroScope *macro_scope,
        FklCodegenActionVector *actions) {
    FklGrammer *g = group_id == 0
                          ? codegen->unnamed_g
                          : &add_production_group(codegen->named_prod_groups,
                                    codegen->ctx->gc,
                                    group_id)
                                     ->g;
    if (FKL_IS_STR(vector_node)) {
        fklAddString(&g->terminals, FKL_VM_STR(vector_node));
        fklAddString(&g->delimiters, FKL_VM_STR(vector_node));
        if (group_id != 0) {
            FklGrammerProdGroupItem *group =
                    add_production_group(codegen->named_prod_groups,
                            codegen->ctx->gc,
                            group_id);
            fklAddString(&group->delimiters, FKL_VM_STR(vector_node));
        }
        return 0;
    }

    if (FKL_IS_PAIR(vector_node)) {
        for (const FklVMvalue *cur = vector_node; FKL_IS_PAIR(cur);
                cur = FKL_VM_CDR(cur)) {
            FKL_ASSERT(FKL_IS_STR(FKL_VM_CAR(cur)));
            fklAddString(&g->terminals, FKL_VM_STR(FKL_VM_CAR(cur)));
            fklAddString(&g->delimiters, FKL_VM_STR(FKL_VM_CAR(cur)));
        }

        return 0;
    }

    if (FKL_IS_BOX(vector_node)) {
        FklVMvalue *ignore_obj = FKL_VM_BOX(vector_node);
        FKL_ASSERT(FKL_IS_VECTOR(ignore_obj));

        NastToGrammerSymArgs args = { .g = g };
        NastToGrammerSymErr err = 0;
        FklGrammerIgnore *ignore =
                nast_vector_to_ignore(ignore_obj, &args, &err);
        if (err) {
            error_state->type = FKL_ERR_GRAMMER_CREATE_FAILED;
            error_state->p = PLACE(args.err_node, CURLINE(ignore_obj));
            // error_state->place = args.err_node;
            error_state->msg = get_nast_to_grammer_sym_err_msg(err);
            return 1;
        }
        if (group_id == 0) {
            if (fklAddIgnoreToIgnoreList(&codegen->unnamed_g->ignores,
                        ignore)) {
                fklDestroyIgnore(ignore);
                return 0;
            }
        } else {
            FklGrammerProdGroupItem *group =
                    add_production_group(codegen->named_prod_groups,
                            codegen->ctx->gc,
                            group_id);
            if (fklAddIgnoreToIgnoreList(&group->g.ignores, ignore)) {
                fklDestroyIgnore(ignore);
                return 0;
            }
        }
        return 0;
    }

    FklVMvalue *error_node = NULL;
    FklVMvalue *vec = vector_node;
    if (FKL_VM_VEC(vec)->size == 4) {

        FklVMvalue **base = FKL_VM_VEC(vec)->base;

        if (!FKL_IS_SYM(base[2])) {
            error_node = base[2];
        reader_macro_syntax_error:
            error_state->type = FKL_ERR_SYNTAXERROR;
            // error_state->place = error_node;
            error_state->p = PLACE(error_node, CURLINE(vec));
            return 1;
        }

        FklVMvalue *vect = NULL;
        NastToProductionArgs args = {
            // .line = vector_node->curline,
            .line = CURLINE(vec),
            .add_extra = 1,
            .action_type = base[2],
            .action_ast = base[3],
            .group_id = group_id,
            .codegen = codegen,
            .env = env,
            .macro_scope = macro_scope,
            .actions = actions,
            .ctx = codegen->ctx,
            .g = g,
        };
        if (base[0] == FKL_VM_NIL && FKL_IS_VECTOR(base[1])) {
            vect = base[1];
            args.left_group = codegen->ctx->builtin_g.start.group;
            args.left_sid = codegen->ctx->builtin_g.start.sid;
            args.add_extra = 0;
        } else if (FKL_IS_SYM(base[0]) && FKL_IS_VECTOR(base[1])) {
            vect = base[1];
            FklVMvalue *sid = base[0];
            if (group_id == 0
                    && (fklGraSidBuiltinHashMapGet2(&g->builtins, sid)
                            || fklIsNonterminalExist(
                                    &codegen->ctx->builtin_g.productions,
                                    group_id,
                                    sid))) {
                error_node = base[0];
                goto reader_macro_syntax_error;
            }
            args.left_group = group_id;
            args.left_sid = sid;
        } else if (FKL_IS_VECTOR(base[0]) && FKL_IS_SYM(base[1])) {
            vect = base[0];
            FklVMvalue *sid = base[1];
            if (group_id == 0
                    && (fklGraSidBuiltinHashMapGet2(&g->builtins, sid)
                            || fklIsNonterminalExist(
                                    &codegen->ctx->builtin_g.productions,
                                    group_id,
                                    sid))) {
                error_node = base[1];
                goto reader_macro_syntax_error;
            }
            args.left_group = group_id;
            args.left_sid = sid;
            args.add_extra = 0;
        } else {
            error_node = vector_node;
            goto reader_macro_syntax_error;
        }

        NastToGrammerSymErr err = nast_vector_to_production(vect, &args);
        if (err) {
            if (args.err_node == NULL)
                args.err_node = base[2];
            error_state->type = FKL_ERR_GRAMMER_CREATE_FAILED;
            // error_state->place = args.err_node;
            error_state->p = PLACE(args.err_node, CURLINE(vect));
            // fklMakeNastNodeRef(FKL_TYPE_CAST(FklNastNode *, args.err_node));
            error_state->msg = get_nast_to_grammer_sym_err_msg(err);
            return 1;
        }
    } else {
        error_node = vector_node;
        goto reader_macro_syntax_error;
    }
    return 0;
}

BC_PROCESS(update_grammer) {
    if (add_all_group_to_grammer(line, codegen, error_state)) {
        error_state->type = FKL_ERR_GRAMMER_CREATE_FAILED;
        // error_state->line = line;
        error_state->p = PLACE(NULL, line);
        return NULL;
    }
    return NULL;
}

static inline int is_valid_production_rule_node(const FklVMvalue *n) {
    return FKL_IS_VECTOR(n)                                //
        || FKL_IS_STR(n)                                   //
        || (FKL_IS_BOX(n) && FKL_IS_VECTOR(FKL_VM_BOX(n))) //
        || is_string_list(n);
}

static CODEGEN_FUNC(codegen_defmacro) {
    if (must_has_retval) {
        error_state->type = FKL_ERR_EXP_HAS_NO_VALUE;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->container));
        return;
    }
    // FklSymbolTable *pst = &ctx->public_st;
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_value);
    if (FKL_IS_SYM(name->value))
        fklReplacementHashMapAdd2(macro_scope->replacements,
                name->value,
                value->value);
    else if (FKL_IS_PAIR(name->value)) {
        FklVMvalueHashSet *symbolSet = NULL;
        FklVMvalue *pattern = fklCreatePatternFromNast(&codegen->ctx->gc->gcvm,
                name->value,
                &symbolSet);
        if (!pattern) {
            error_state->type = FKL_ERR_INVALID_MACRO_PATTERN;
            // error_state->place = name->value;
            error_state->p = PLACE(name->value, CURLINE(name->container));
            return;
        }
        if (fklVMvalueHashSetPut2(symbolSet, ctx->builtInPatternVar_orig)) {
            // fklDestroyNastNode(pattern);
            error_state->type = FKL_ERR_INVALID_MACRO_PATTERN;
            error_state->p = PLACE(name->value, CURLINE(name->container));
            // error_state->place = name->value;
            return;
        }

        FklVMvalueCodegenEnv *macroEnv = NULL;
        FklVMvalueCodegenInfo *macroCodegen = macro_compile_prepare(codegen,
                macro_scope,
                symbolSet,
                &macroEnv);
        fklVMvalueHashSetDestroy(symbolSet);

        create_and_insert_to_pool(macroCodegen,
                0,
                macroEnv,
                0,
                // value->curline,
                CURLINE(value->container));
        CgExpQueue *queue = cgExpQueueCreate();
        cgExpQueuePush(queue, value);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_compiler_macro_bc_process,
                createMacroActionContext(name->value,
                        pattern,
                        macro_scope,
                        macroEnv->prototypeId),
                createMustHasRetvalQueueNextExpression(queue),
                1,
                macroEnv->macros,
                macroEnv,
                // value->curline,
                CURLINE(value->container),
                macroCodegen,
                actions);
    } else if (FKL_IS_BOX(name->value)) {
        FklVMvalue *group_node = FKL_VM_BOX(name->value);
        if (!is_valid_production_rule_node(value->value)) {
            error_state->type = FKL_ERR_SYNTAXERROR;
            // error_state->place = value->value;
            error_state->p = PLACE(value->value, CURLINE(value->container));
            return;
        }

        if (!FKL_IS_SYM(group_node) && group_node != FKL_VM_NIL) {
            error_state->type = FKL_ERR_SYNTAXERROR;
            // error_state->place = group_node;
            error_state->p = PLACE(group_node, CURLINE(name->value));
            return;
        }
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(update_grammer,
                createDefaultStackContext(),
                NULL,
                scope,
                macro_scope,
                env,
                // orig->curline,
                CURLINE(orig->container),
                codegen,
                actions);
        init_builtin_grammer_and_prod_group(codegen);

        FklVMvalue *group_id = group_node == FKL_VM_NIL ? NULL : group_node;

        if (process_add_production(group_id,
                    codegen,
                    value->value,
                    error_state,
                    env,
                    macro_scope,
                    actions))
            return;
    }
}

static CODEGEN_FUNC(codegen_def_reader_macros) {
    if (must_has_retval) {
        error_state->type = FKL_ERR_EXP_HAS_NO_VALUE;
        // error_state->place = orig->value;
        error_state->p = PLACE(orig->value, CURLINE(orig->value));
        return;
    }
    // FklSymbolTable *pst = &ctx->public_st;
    FklVMvalueQueue prod_vector_queue;
    fklVMvalueQueueInit(&prod_vector_queue);
    const FklPmatchRes *name =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_arg0);
    FklVMvalue *err_node = NULL;
    if (!FKL_IS_BOX(name->value)) {
        err_node = name->value;
        goto reader_macro_error;
    }
    const FklPmatchRes *arg =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_arg1);
    if (!is_valid_production_rule_node(arg->value)) {
        err_node = arg->value;
        goto reader_macro_error;
    }
    fklVMvalueQueuePush2(&prod_vector_queue, arg->value);
    const FklPmatchRes *rest =
            fklPmatchHashMapGet2(ht, ctx->builtInPatternVar_rest);

    for (FklVMvalue *rv = rest->value; FKL_IS_PAIR(rv); rv = FKL_VM_CDR(rv)) {
        if (!is_valid_production_rule_node(FKL_VM_CAR(rv))) {
            err_node = FKL_VM_CAR(rv);
            goto reader_macro_error;
        }
        fklVMvalueQueuePush2(&prod_vector_queue, FKL_VM_CAR(rv));
    }

    FklVMvalue *group_node = FKL_VM_BOX(name->value);
    if (group_node != FKL_VM_NIL && !FKL_IS_SYM(group_node)) {
        err_node = group_node;
    reader_macro_error:
        fklVMvalueQueueUninit(&prod_vector_queue);
        error_state->type = FKL_ERR_SYNTAXERROR;
        // error_state->place = err_node;
        error_state->p = PLACE(err_node, CURLINE(err_node));
        return;
    }
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(update_grammer,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            // orig->curline,
            CURLINE(orig->container),
            codegen,
            actions);

    init_builtin_grammer_and_prod_group(codegen);

    FklVMvalue *group_id = FKL_IS_SYM(group_node) ? group_node : NULL;

    for (FklVMvalueQueueNode *first = prod_vector_queue.head; first;
            first = first->next)
        if (process_add_production(group_id,
                    codegen,
                    first->data,
                    error_state,
                    env,
                    macro_scope,
                    actions)) {
            fklVMvalueQueueUninit(&prod_vector_queue);
            return;
        }
    fklVMvalueQueueUninit(&prod_vector_queue);
}

typedef void (*FklCodegenFunc)(CODEGEN_ARGS);

BC_PROCESS(last_bc_process) {
    FklByteCodelnt *r = sequnce_exp_bc_process(bcl_vec, fid, line, scope);
    FklInstruction r_ins = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(r, &r_ins, codegen->fid, codegen->curline, 1);
    if (!env->is_debugging)
        fklPeepholeOptimize(r);
    return r;
}

#undef BC_PROCESS
#undef CODEGEN_ARGS
#undef CODEGEN_FUNC

static FklByteCodelnt *gen_push_literal_code(const FklPmatchRes *cnode,
        FklVMvalueCodegenInfo *codegen,
        FklVMvalueCodegenEnv *env,
        uint32_t scope) {
    FklVMvalue *node = cnode->value;

    FklByteCodelnt *r = fklCreateByteCodelnt(0);
    FklByteCode *retval = &r->bc;
    uint64_t k = 0;
    switch (FKL_TYPE_CAST(FklVMptrTag, FKL_GET_TAG(node))) {
    default:
        FKL_UNREACHABLE();
        break;
    case FKL_TAG_PTR:
        goto add_consts;
        break;
    case FKL_TAG_NIL:
        fklByteCodeInsertFront(create_op_ins(FKL_OP_PUSH_NIL), retval);
        break;
    case FKL_TAG_CHR:
        append_push_chr_ins_to_bc(INS_APPEND_FRONT, retval, FKL_GET_CHR(node));
        break;
    case FKL_TAG_FIX: {
        int64_t i = FKL_GET_FIX(node);
        if (i >= FKL_I24_MIN && i <= FKL_I24_MAX) {
            append_push_i24_ins_to_bc(INS_APPEND_FRONT, retval, i, codegen);
        } else {
        add_consts:
            k = fklValueTableAdd(&env->konsts, node) - 1;
            FKL_ASSERT(k <= FKL_U24_MAX);
            append_push_const_ins_to_bc(INS_APPEND_FRONT, retval, k, codegen);
        }
    } break;
    }

    r->ls = 1;
    r->l = (FklLineNumberTableItem *)fklZmalloc(sizeof(FklLineNumberTableItem));
    FKL_ASSERT(r->l);
    fklInitLineNumTabNode(&r->l[0], codegen->fid, 0, CURLINE(node), scope);

    return r;
}

static inline int matchAndCall(FklCodegenFunc func,
        const FklVMvalue *pattern,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        const FklPmatchRes *exp,
        FklCodegenActionVector *actions,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegenr,
        FklCodegenErrorState *error_state,
        uint8_t must_has_retval) {
    FklPmatchHashMap ht;
    fklPmatchHashMapInit(&ht);
    int r = fklPatternMatch(pattern, exp->value, &ht);
    if (r) {
        fklChdir(codegenr->dir);
        func(exp,
                &ht,
                actions,
                scope,
                macro_scope,
                env,
                codegenr,
                codegenr->ctx,
                error_state,
                must_has_retval);
        fklChdir(codegenr->ctx->cwd);
    }
    fklPmatchHashMapUninit(&ht);
    return r;
}

static struct PatternAndFunc {
    const char *ps;
    FklCodegenFunc func;
} builtin_pattern_cstr_func[FKL_CODEGEN_PATTERN_NUM + 1] = {
    // clang-format off
    {"~(begin,~rest)",                                         codegen_begin             },
    {"~(local,~rest)",                                         codegen_local             },
    {"~(let [],~rest)",                                        codegen_let0              },
    {"~(let [(~name ~value),~args],~rest)",                    codegen_let1              },
    {"~(let* [],~rest)",                                       codegen_let0              },
    {"~(let* [(~name ~value)],~rest)",                         codegen_let1              },
    {"~(let* [(~name ~value),~args],~rest)",                   codegen_let81             },
    {"~(let ~arg0 [],~rest)",                                  codegen_named_let0        },
    {"~(let ~arg0 [(~name ~value),~args],~rest)",              codegen_named_let1        },
    {"~(letrec [],~rest)",                                     codegen_let0              },
    {"~(letrec [(~name ~value),~args],~rest)",                 codegen_letrec            },

    {"~(do [] [~cond ~value],~rest)",                          codegen_do0               },
    {"~(do [] [~cond],~rest)",                                 codegen_do0               },

    {"~(do [(~name ~arg0),~args] [~cond ~value],~rest)",       codegen_do1               },
    {"~(do [(~name ~arg0),~args] [~cond],~rest)",              codegen_do1               },

    {"~(do [(~name ~arg0 ~arg1),~args] [~cond ~value],~rest)", codegen_do1               },
    {"~(do [(~name ~arg0 ~arg1),~args] [~cond],~rest)",        codegen_do1               },

    {"~(define (~name,~args),~rest)",                          codegen_defun             },
    {"~(define ~name ~value)",                                 codegen_define            },

    {"~(defconst (~name,~args),~rest)",                        codegen_defun_const       },
    {"~(defconst ~name ~value)",                               codegen_defconst          },

    {"~(setq ~name ~value)",                                   codegen_setq              },
    {"~(cfg ~value)",                                          codegen_check             },
    {"~(quote ~value)",                                        codegen_quote             },
    {"`(unquote `value)",                                      codegen_unquote           },
    {"~(qsquote ~value)",                                      codegen_qsquote           },
    {"~(lambda ~args,~rest)",                                  codegen_lambda            },
    {"~(and,~rest)",                                           codegen_and               },
    {"~(or,~rest)",                                            codegen_or                },
    {"~(cond,~rest)",                                          codegen_cond              },
    {"~(if ~value ~rest)",                                     codegen_if0               },
    {"~(if ~value ~rest ~args)",                               codegen_if1               },
    {"~(when ~value,~rest)",                                   codegen_when              },
    {"~(unless ~value,~rest)",                                 codegen_unless            },
    {"~(load ~name,~rest)",                                    codegen_load              },
    {"~(import (prefix ~name ~rest),~args)",                   codegen_import_prefix     },
    {"~(import (only ~name,~rest),~args)",                     codegen_import_only       },
    {"~(import (alias ~name,~rest),~args)",                    codegen_import_alias      },
    {"~(import (except ~name,~rest),~args)",                   codegen_import_except     },
    {"~(import (common ~name),~args)",                         codegen_import_common     },
    {"~(import ~name,~args)",                                  codegen_import            },
    {"~(macroexpand ~value)",                                  codegen_macroexpand       },
    {"~(defmacro ~name ~value)",                               codegen_defmacro          },
    {"~(import)",                                              codegen_import_none       },
    {"~(export ~value)",                                       codegen_export_single     },
    {"~(export)",                                              codegen_export_none       },
    {"~(export,~rest)",                                        codegen_export            },
    {"~(defmacro ~arg0 ~arg1,~rest)",                          codegen_def_reader_macros },
    {"~(cfg ~cond ~value,~rest)",                              codegen_cond_compile      },
    {NULL,                                                     NULL                      },
    // clang-format on
};

static inline void mark_action_vector(FklVMgc *gc, FklCodegenActionVector *v) {
    if (v == NULL)
        return;
    FklCodegenAction **cur = v->base;
    FklCodegenAction **const end = cur + v->size;
    for (; cur < end; ++cur) {
        FklCodegenAction *a = *cur;
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, a->env), gc);
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, a->macros), gc);
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, a->codegen), gc);
        if (a->context->t->__atomic) {
            a->context->t->__atomic(gc, a->context->d);
        }
        if (a->expressions && a->expressions->t->atomic) {
            a->expressions->t->atomic(gc, a->expressions->context);
        }

        FklByteCodelntVector *bcv = &a->bcl_vector;
        for (size_t i = 0; i < bcv->size; ++i)
            fklVMgcMarkCodeObject(gc, bcv->base[i]);
    }
}

static inline void mark_codegen_lib(FklVMgc *gc, const FklCodegenLib *lib) {
    for (const FklCgExportSidIdxHashMapNode *cur = lib->exports.first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->k, gc);
    }
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        for (const FklCodegenMacro *cur = lib->head; cur; cur = cur->next) {
            fklVMgcToGray(cur->pattern, gc);
            fklVMgcMarkCodeObject(gc, cur->bcl);
        }
        for (const FklReplacementHashMapNode *cur = lib->replacements->first;
                cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            fklVMgcToGray(cur->v, gc);
        }
        for (const FklGraProdGroupHashMapNode *cur =
                        lib->named_prod_groups.first;
                cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            fklVMgcMarkGrammer(gc, &cur->v.g, NULL);
        }

        break;
    case FKL_CODEGEN_LIB_DLL:
        break;
    case FKL_CODEGEN_LIB_UNINIT:
        FKL_UNREACHABLE();
        break;
    }
}

static inline void mark_codegen_lib_vector(FklVMgc *gc,
        const FklCodegenLibVector *libs) {
    for (size_t i = 0; i < libs->size; ++i) {
        mark_codegen_lib(gc, &libs->base[i]);
    }
}

static void codegen_ctx_extra_mark_func(FklVMgc *gc, void *c) {
    FklCodegenCtx *ctx = FKL_TYPE_CAST(FklCodegenCtx *, c);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->global_env), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->global_info), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->macro_vm_libs), gc);

    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->macro_pts), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->pts), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->lnt), gc);

    mark_action_vector(gc, ctx->action_vector);

    if (ctx->error_state) {
        fklVMgcToGray(ctx->error_state->fid, gc);
        fklVMgcToGray(ctx->error_state->p.place, gc);
    }

    {
#define XX(A) fklVMgcToGray(ctx->builtInPatternVar_##A, gc);
        FKL_CODEGEN_SYMBOL_MAP
#undef XX
    }

    for (size_t i = 0; i < FKL_BUILTIN_REPLACEMENT_NUM; ++i) {
        fklVMgcToGray(ctx->builtin_replacement_id[i], gc);
    }

    for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; ++i) {
        fklVMgcToGray(ctx->builtin_pattern_node[i], gc);
    }

    for (size_t i = 0; i < FKL_CODEGEN_SUB_PATTERN_NUM; ++i) {
        fklVMgcToGray(ctx->builtin_sub_pattern_node[i], gc);
    }

    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; ++i) {
        fklVMgcToGray(ctx->builtin_prod_action_id[i], gc);
    }

    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; ++i) {
        fklVMgcToGray(ctx->simple_prod_action_id[i], gc);
    }

    mark_codegen_lib_vector(gc, &ctx->libraries);
    mark_codegen_lib_vector(gc, &ctx->macro_libraries);
}

void fklInitCodegenCtxExceptPattern(FklCodegenCtx *ctx,
        FklVMvalueProtos *pts,
        FklVMgc *gc) {
    memset(ctx, 0, sizeof(FklCodegenCtx));
    // fklInitSymbolTable(&ctx->public_st);
    fklCodegenLibVectorInit(&ctx->libraries, 8);
    fklCodegenLibVectorInit(&ctx->macro_libraries, 8);

    ctx->pts = pts == NULL ? fklCreateVMvalueProtos(&gc->gcvm, 0) : pts;
    ctx->macro_pts = pts == NULL //
                           ? ctx->pts
                           : fklCreateVMvalueProtos(&gc->gcvm, 0);

    ctx->gc = gc;

    fklVMpushExtraMarkFunc(ctx->gc, codegen_ctx_extra_mark_func, NULL, ctx);

    fklInitBuiltinGrammer(&ctx->builtin_g, &ctx->gc->gcvm);

    ctx->lnt = fklCreateVMvalueCodegenLnt(&ctx->gc->gcvm);
}

static inline void init_builtin_patterns(FklCodegenCtx *ctx) {
    FklVMvalue **const builtin_pattern_node = ctx->builtin_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++) {
        FklVMvalue *node = fklCreateNastNodeFromCstr(&ctx->gc->gcvm,
                builtin_pattern_cstr_func[i].ps,
                NULL);
        builtin_pattern_node[i] =
                fklCreatePatternFromNast(&ctx->gc->gcvm, node, NULL);
    }
}

static inline void init_builtin_replacements(FklCodegenCtx *ctx) {
    FklVMvalue **const replacement_ids = ctx->builtin_replacement_id;
    struct SymbolReplacement *const builtin_replacements =
            builtInSymbolReplacement;

    for (size_t i = 0; i < FKL_BUILTIN_REPLACEMENT_NUM; i++)
        replacement_ids[i] = add_symbol_cstr(ctx, builtin_replacements[i].s);
}

static inline void init_builtin_sub_patterns(FklCodegenCtx *ctx) {
    FklVMvalue **const builtin_sub_pattern_node = ctx->builtin_sub_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_SUB_PATTERN_NUM; i++) {
        FklVMvalue *node = fklCreateNastNodeFromCstr(&ctx->gc->gcvm,
                builtInSubPattern[i],
                NULL);
        builtin_sub_pattern_node[i] =
                fklCreatePatternFromNast(&ctx->gc->gcvm, node, NULL);
    }
}

void fklInitCodegenCtx(FklCodegenCtx *ctx,
        char *main_file_real_path_dir,
        FklVMvalueProtos *pts,
        FklVMgc *gc) {
    fklInitCodegenCtxExceptPattern(ctx, pts, gc);
    ctx->cwd = fklSysgetcwd();
    ctx->main_file_real_path_dir = main_file_real_path_dir
                                         ? main_file_real_path_dir
                                         : fklZstrdup(ctx->cwd);

#define XX(A) ctx->builtInPatternVar_##A = add_symbol_cstr(ctx, #A);
    FKL_CODEGEN_SYMBOL_MAP
#undef XX

    fklInitProdActionList(ctx);

    init_builtin_patterns(ctx);
    init_builtin_sub_patterns(ctx);
    init_builtin_replacements(ctx);
}

void fklSetCodegenCtxMainFileRealPathDir(FklCodegenCtx *ctx, char *dir) {
    if (ctx->main_file_real_path_dir)
        fklZfree(ctx->main_file_real_path_dir);
    ctx->main_file_real_path_dir = dir;
}

void fklUninitCodegenCtx(FklCodegenCtx *ctx) {
    fklUninitGrammer(&ctx->builtin_g);
    ctx->gc = NULL;
    ctx->pts = NULL;
    ctx->macro_pts = NULL;
    ctx->lnt = NULL;

    FklVMvalue **nodes = ctx->builtin_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++) {
        // fklDestroyNastNode(nodes[i]);
        nodes[i] = NULL;
    }
    nodes = ctx->builtin_sub_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_SUB_PATTERN_NUM; i++) {
        // fklDestroyNastNode(nodes[i]);
        nodes[i] = NULL;
    }
    fklZfree(ctx->cwd);
    fklZfree(ctx->main_file_real_path_dir);

    while (!fklCodegenLibVectorIsEmpty(&ctx->libraries))
        fklUninitCodegenLib(fklCodegenLibVectorPopBackNonNull(&ctx->libraries));

    fklCodegenLibVectorUninit(&ctx->libraries);

    while (!fklCodegenLibVectorIsEmpty(&ctx->macro_libraries))
        fklUninitCodegenLib(
                fklCodegenLibVectorPopBackNonNull(&ctx->macro_libraries));

    fklCodegenLibVectorUninit(&ctx->macro_libraries);
}

static inline int mapAllBuiltInPattern(const FklPmatchRes *curExp,
        FklCodegenActionVector *actions,
        uint32_t scope,
        FklVMvalueCodegenMacroScope *macro_scope,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegenr,
        FklCodegenErrorState *error_state,
        uint8_t must_has_retval) {
    FklVMvalue *const *builtin_pattern_node =
            codegenr->ctx->builtin_pattern_node;

    if (fklIsList(curExp->value))
        for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++)
            if (matchAndCall(builtin_pattern_cstr_func[i].func,
                        builtin_pattern_node[i],
                        scope,
                        macro_scope,
                        curExp,
                        actions,
                        env,
                        codegenr,
                        error_state,
                        must_has_retval))
                return 0;

    if (FKL_IS_PAIR(curExp->value)) {
        codegen_funcall(curExp,
                actions,
                scope,
                macro_scope,
                env,
                codegenr,
                error_state);
        return 0;
    }
    return 1;
}

static inline FklByteCodelnt *append_get_loc_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m,
            bcl,
            FKL_OP_GET_LOC,
            idx,
            fid,
            line,
            scope);
}

static inline FklByteCodelnt *append_get_var_ref_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t curline,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m,
            bcl,
            FKL_OP_GET_VAR_REF,
            idx,
            fid,
            curline,
            scope);
}

#ifdef NDEBUG
#define FORCE_GC (0)
#else
#define FORCE_GC (1)
#endif

FklByteCodelnt *fklGenExpressionCodeWithAction(FklCodegenAction *initial_action,
        FklVMvalueCodegenInfo *info) {
    FklCodegenCtx *ctx = info->ctx;
    // FklSymbolTable *pst = &ctx->public_st;
    FklByteCodelntVector results;
    fklByteCodelntVectorInit(&results, 1);
    FklCodegenErrorState error_state = { 0, { NULL, 0 }, 0, NULL, NULL };
    FklCodegenActionVector codegen_action_vector;
    fklCodegenActionVectorInit(&codegen_action_vector, 32);

    FklCodegenAction *action1 = create_cg_action(last_bc_process,
            createDefaultStackContext(),
            NULL,
            initial_action->scope,
            initial_action->macros,
            initial_action->env,
            initial_action->curline,
            NULL,
            initial_action->codegen);

    fklCodegenActionVectorPushBack2(&codegen_action_vector, action1);
    fklCodegenActionVectorPushBack2(&codegen_action_vector, initial_action);

    info->ctx->action_vector = &codegen_action_vector;
    info->ctx->error_state = &error_state;

    while (!fklCodegenActionVectorIsEmpty(&codegen_action_vector)) {
        FklCodegenAction *cur_action =
                *fklCodegenActionVectorBackNonNull(&codegen_action_vector);
        FklVMvalueCodegenInfo *cur_info = cur_action->codegen;
        FklCodegenNextExpression *expressions = cur_action->expressions;
        FklVMvalueCodegenEnv *env = cur_action->env;
        FklCodegenActionContext *cur_context = cur_action->context;
        int r = 1;
        if (expressions) {
            CgGetNextExpCb get_next_expression = expressions->t->get_next_exp;
            uint8_t must_has_retval = expressions->must_has_retval;

            FklPmatchRes exp = { NULL, NULL };
            while (get_next_expression(expressions->context,
                    &exp,
                    &error_state)) {
            skip:
                if (FKL_IS_PAIR(exp.value)) {
                    exp.value = fklTryExpandCodegenMacro(&exp,
                            cur_info,
                            cur_action->macros,
                            &error_state);
                    if (error_state.type) {
                        // fklDestroyNastNode(orig_cur_exp);
                        break;
                    }
                    if (must_has_retval == FIRST_MUST_HAS_RETVAL) {
                        must_has_retval = DO_NOT_NEED_RETVAL;
                        expressions->must_has_retval = DO_NOT_NEED_RETVAL;
                    }
                    // fklDestroyNastNode(orig_cur_exp);
                } else if (FKL_IS_SYM(exp.value)) {
                    FklVMvalue *replacement = NULL;
                    ReplacementFunc f = NULL;
                    for (FklVMvalueCodegenMacroScope *cs = cur_action->macros;
                            cs && !replacement;
                            cs = cs->prev)
                        replacement =
                                fklGetReplacement(exp.value, cs->replacements);
                    if (replacement) {
                        // fklDestroyNastNode(curExp);
                        exp.value = replacement;
                        goto skip;
                    } else if ((f = findBuiltInReplacementWithId(exp.value,
                                        ctx->builtin_replacement_id))) {
                        FklVMvalue *t = f(&exp, env, cur_info);
                        FKL_ASSERT(t);
                        // fklDestroyNastNode(curExp);
                        exp.value = t;
                        goto skip;
                    } else {
                        FklByteCodelnt *bcl = NULL;
                        FklSymDefHashMapElm *def =
                                fklFindSymbolDefByIdAndScope(exp.value,
                                        cur_action->scope,
                                        env->scopes);
                        if (def)
                            bcl = append_get_loc_ins(INS_APPEND_BACK,
                                    NULL,
                                    def->v.idx,
                                    cur_info->fid,
                                    // curExp->curline,
                                    get_curline(cur_info, exp.container),
                                    cur_action->scope);
                        else {
                            uint32_t idx = fklAddCodegenRefBySidRetIndex(
                                    exp.value,
                                    env,
                                    cur_info->fid,
                                    // curExp->curline,
                                    get_curline(cur_info, exp.container),
                                    0);
                            bcl = append_get_var_ref_ins(INS_APPEND_BACK,
                                    NULL,
                                    idx,
                                    cur_info->fid,
                                    // curExp->curline,
                                    get_curline(cur_info, exp.container),
                                    cur_action->scope);
                        }
                        fklByteCodelntVectorPushBack2(&cur_action->bcl_vector,
                                bcl);
                        // fklDestroyNastNode(curExp);
                        continue;
                    }
                }
                FKL_ASSERT(exp.value);
                r = mapAllBuiltInPattern(&exp,
                        &codegen_action_vector,
                        cur_action->scope,
                        cur_action->macros,
                        env,
                        cur_info,
                        &error_state,
                        must_has_retval);
                if (r) {
                    fklByteCodelntVectorPushBack2(&cur_action->bcl_vector,
                            gen_push_literal_code(&exp,
                                    cur_info,
                                    env,
                                    cur_action->scope));
                }
                // fklDestroyNastNode(curExp);
                if ((!r
                            && (fklCodegenActionVectorIsEmpty(
                                        &codegen_action_vector)
                                    || *fklCodegenActionVectorBack(
                                               &codegen_action_vector)
                                               != cur_action))
                        || error_state.type)
                    break;
            }
        }
        if (error_state.type) {
        print_error:
            ctx->action_vector = NULL;
            ctx->error_state = NULL;
            fklPrintCodegenError(&error_state, cur_info);
            while (!fklCodegenActionVectorIsEmpty(&codegen_action_vector)) {
                destroy_cg_action(*fklCodegenActionVectorPopBackNonNull(
                        &codegen_action_vector));
            }
            fklCodegenActionVectorUninit(&codegen_action_vector);
            fklByteCodelntVectorUninit(&results);
            fklCheckAndGC(&ctx->gc->gcvm, FORCE_GC);
            return NULL;
        }
        FklCodegenAction *otherCodegenAction =
                *fklCodegenActionVectorBackNonNull(&codegen_action_vector);
        if (otherCodegenAction == cur_action) {
            fklCodegenActionVectorPopBack(&codegen_action_vector);
            FklCodegenAction *prevAction =
                    cur_action->prev ? cur_action->prev
                    : fklCodegenActionVectorIsEmpty(&codegen_action_vector)
                            ? NULL
                            : *fklCodegenActionVectorBackNonNull(
                                      &codegen_action_vector);
            FklByteCodelnt *resultBcl =
                    cur_action->processer(cur_action->codegen,
                            env,
                            cur_action->scope,
                            cur_action->macros,
                            FKL_TYPE_CAST(void *, cur_context->d),
                            &cur_action->bcl_vector,
                            cur_action->codegen->fid,
                            cur_action->curline,
                            &error_state,
                            ctx);
            if (resultBcl) {
                if (fklCodegenActionVectorIsEmpty(&codegen_action_vector))
                    fklByteCodelntVectorPushBack2(&results, resultBcl);
                else {
                    fklByteCodelntVectorPushBack2(&prevAction->bcl_vector,
                            resultBcl);
                }
                fklCheckAndGC(&ctx->gc->gcvm, FORCE_GC);
            }
            if (error_state.type) {
                fklCodegenActionVectorPushBack2(&codegen_action_vector,
                        cur_action);
                goto print_error;
            }
            destroy_cg_action(cur_action);
        }
    }
    fklCheckAndGC(&ctx->gc->gcvm, FORCE_GC);
    ctx->action_vector = NULL;
    ctx->error_state = NULL;
    FklByteCodelnt *retval = NULL;
    if (!fklByteCodelntVectorIsEmpty(&results))
        retval = *fklByteCodelntVectorPopBackNonNull(&results);
    fklByteCodelntVectorUninit(&results);
    fklCodegenActionVectorUninit(&codegen_action_vector);
    return retval;
}

FklByteCodelnt *fklGenExpressionCodeWithFpForPrecompile(FILE *fp,
        FklVMvalueCodegenInfo *codegen,
        FklVMvalueCodegenEnv *env) {
    static const uint32_t scope = 1;
    static const uint64_t line = 1;

    FklCodegenAction *initialAction = create_cg_action(_begin_exp_bc_process,
            createDefaultStackContext(),
            createFpNextExpression(fp, codegen),
            1,
            env->macros,
            env,
            1,
            NULL,
            codegen);

    FklByteCodelnt *bcl =
            fklGenExpressionCodeWithAction(initialAction, codegen);
    if (bcl == NULL)
        return NULL;

    process_export_bc(codegen, bcl, codegen->fid, line, scope);

    return bcl;
}

FklByteCodelnt *fklGenExpressionCodeWithFp(FILE *fp,
        FklVMvalueCodegenInfo *codegen,
        FklVMvalueCodegenEnv *env) {
    FklCodegenAction *initialAction = create_cg_action(_begin_exp_bc_process,
            createDefaultStackContext(),
            createFpNextExpression(fp, codegen),
            1,
            env->macros,
            env,
            1,
            NULL,
            codegen);
    return fklGenExpressionCodeWithAction(initialAction, codegen);
}

FklByteCodelnt *fklGenExpressionCode(FklVMvalue *exp,
        FklVMvalueCodegenEnv *env,
        FklVMvalueCodegenInfo *codegen) {
    FklVMvalue *cont = fklCreateVMvalueBox(&codegen->ctx->gc->gcvm, exp);
    put_line_number(&codegen->ctx->lnt->ht, cont, codegen->curline);
    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush2(queue, (FklPmatchRes){ .value = exp, .container = cont });
    FklCodegenAction *initialAction = create_cg_action(_default_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            env->macros,
            env,
            // exp->curline,
            CURLINE(exp),
            NULL,
            codegen);
    return fklGenExpressionCodeWithAction(initialAction, codegen);
}

FklByteCodelnt *fklGenExpressionCodeWithQueue(const FklVMvalueQueue *q,
        FklVMvalueCodegenInfo *codegen,
        FklVMvalueCodegenEnv *env) {
    FKL_UNREACHABLE();
    // FklCodegenAction *initialAction = create_cg_action(_begin_exp_bc_process,
    //         createDefaultStackContext(),
    //         createDefaultQueueNextExpression(q),
    //         1,
    //         env->macros,
    //         env,
    //         1,
    //         NULL,
    //         codegen);
    // return fklGenExpressionCodeWithAction(initialAction, codegen);
}
