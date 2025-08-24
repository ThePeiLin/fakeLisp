#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/nast.h>
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

static inline FklNastNode *cadr_nast_node(const FklNastNode *node) {
    return node->pair->cdr->pair->car;
}

static inline int isExportDefmacroExp(const FklNastNode *c,
        FklNastNode *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFMACRO],
                   c,
                   NULL)
        && cadr_nast_node(c)->type != FKL_NAST_BOX;
}

static inline int isExportDefReaderMacroExp(const FklNastNode *c,
        FklNastNode *const *builtin_pattern_node) {
    return (fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFMACRO],
                    c,
                    NULL)
                   && cadr_nast_node(c)->type == FKL_NAST_BOX)
        || fklPatternMatch(
                builtin_pattern_node[FKL_CODEGEN_PATTERN_DEF_READER_MACROS],
                c,
                NULL);
}

static inline int isExportDefineExp(const FklNastNode *c,
        FklNastNode *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFINE],
                   c,
                   NULL)
        || fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFCONST],
                c,
                NULL);
}

static inline int isBeginExp(const FklNastNode *c,
        FklNastNode *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_BEGIN],
            c,
            NULL);
}

static inline int isExportDefunExp(const FklNastNode *c,
        FklNastNode *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFUN],
                   c,
                   NULL)
        || fklPatternMatch(
                builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFUN_CONST],
                c,
                NULL);
}

static inline int isExportImportExp(FklNastNode *c,
        FklNastNode *const *builtin_pattern_node) {
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

static void _default_stack_context_finalizer(void *data) {
    FklByteCodelntVector *s = data;
    while (!fklByteCodelntVectorIsEmpty(s))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBackNonNull(s));
    fklByteCodelntVectorDestroy(s);
}

static void _default_stack_context_put_bcl(void *data, FklByteCodelnt *bcl) {
    FklByteCodelntVector *s = data;
    fklByteCodelntVectorPushBack2(s, bcl);
}

static FklByteCodelntVector *_default_stack_context_get_bcl_stack(void *data) {
    return data;
}

static const FklCodegenQuestContextMethodTable
        DefaultStackContextMethodTable = {
            .__finalizer = _default_stack_context_finalizer,
            .__put_bcl = _default_stack_context_put_bcl,
            .__get_bcl_stack = _default_stack_context_get_bcl_stack,
        };

static FklCodegenQuestContext *createCodegenQuestContext(void *data,
        const FklCodegenQuestContextMethodTable *t) {
    FklCodegenQuestContext *r = (FklCodegenQuestContext *)fklZmalloc(
            sizeof(FklCodegenQuestContext));
    FKL_ASSERT(r);
    r->data = data;
    r->t = t;
    return r;
}

static FklCodegenQuestContext *createDefaultStackContext(
        FklByteCodelntVector *s) {
    return createCodegenQuestContext(s, &DefaultStackContextMethodTable);
}

static void _empty_context_finalizer(void *data) {}

static const FklCodegenQuestContextMethodTable EmptyContextMethodTable = {
    .__finalizer = _empty_context_finalizer,
    .__get_bcl_stack = NULL,
    .__put_bcl = NULL,
};

static FklCodegenQuestContext *createEmptyContext(void) {
    return createCodegenQuestContext(NULL, &EmptyContextMethodTable);
}

#define DO_NOT_NEED_RETVAL (0)
#define ALL_MUST_HAS_RETVAL (1)
#define FIRST_MUST_HAS_RETVAL (2)

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

static FklNastNode *_default_codegen_get_next_expression(void *context,
        FklCodegenErrorState *errorState) {
    FklNastNode **head =
            fklNastNodeQueuePop(FKL_TYPE_CAST(FklNastNodeQueue *, context));
    return head ? *head : NULL;
}

static void _default_codegen_next_expression_finalizer(void *context) {
    FklNastNodeQueue *q = FKL_TYPE_CAST(FklNastNodeQueue *, context);
    while (!fklNastNodeQueueIsEmpty(q))
        fklDestroyNastNode(*fklNastNodeQueuePop(q));
    fklNastNodeQueueDestroy(q);
}

static const FklNextExpressionMethodTable
        _default_codegen_next_expression_method_table = {
            .getNextExpression = _default_codegen_get_next_expression,
            .finalizer = _default_codegen_next_expression_finalizer,
        };

static FklCodegenNextExpression *createDefaultQueueNextExpression(
        FklNastNodeQueue *queue) {
    return createCodegenNextExpression(
            &_default_codegen_next_expression_method_table,
            queue,
            DO_NOT_NEED_RETVAL);
}

static FklCodegenNextExpression *createMustHasRetvalQueueNextExpression(
        FklNastNodeQueue *queue) {
    return createCodegenNextExpression(
            &_default_codegen_next_expression_method_table,
            queue,
            ALL_MUST_HAS_RETVAL);
}

static FklCodegenNextExpression *createFirstHasRetvalQueueNextExpression(
        FklNastNodeQueue *queue) {
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
    return fklCreateByteCodelnt(fklCreateByteCode(0));
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
        FklSid_t fid,
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
        FklSid_t fid,
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
        FklSid_t fid,
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

static inline FklByteCode *set_and_append_ins_with_unsigned_imm_to_bc(
        InsAppendMode m,
        FklByteCode *bc,
        FklOpcode op,
        uint64_t k) {
    FklInstruction ins[3] = { FKL_INSTRUCTION_STATIC_INIT };
    int l = set_ins_with_unsigned_imm(ins, op, k);
    if (bc == NULL)
        bc = fklCreateByteCode(0);
    switch (m) {
    case INS_APPEND_BACK:
        for (int i = 0; i < l; i++)
            fklByteCodePushBack(bc, ins[i]);
        break;
    case INS_APPEND_FRONT:
        for (; l > 0; l--)
            fklByteCodeInsertFront(ins[l - 1], bc);
    }
    return bc;
}

static inline FklByteCode *
append_push_sym_ins_to_bc(InsAppendMode m, FklByteCode *bc, FklSid_t sid) {
    return set_and_append_ins_with_unsigned_imm_to_bc(m,
            bc,
            FKL_OP_PUSH_SYM,
            sid);
}

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

static inline FklByteCode *append_push_str_ins_to_bc(InsAppendMode m,
        FklByteCode *bc,
        const FklString *k,
        FklCodegenInfo *info) {
    return set_and_append_ins_with_unsigned_imm_to_bc(m,
            bc,
            FKL_OP_PUSH_STR,
            fklAddStrConst(info->runtime_kt, k));
}

static inline FklByteCode *append_push_i64_ins_to_bc(InsAppendMode m,
        FklByteCode *bc,
        int64_t k,
        FklCodegenInfo *info) {
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
        uint32_t id = fklAddI64Const(info->runtime_kt, k);
        bc = set_and_append_ins_with_unsigned_imm_to_bc(m,
                bc,
                (k >= FKL_FIX_INT_MIN && k <= FKL_FIX_INT_MAX)
                        ? FKL_OP_PUSH_I64F
                        : FKL_OP_PUSH_I64B,
                id);
    }
    return bc;
}

static inline FklByteCodelnt *append_push_i64_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        int64_t k,
        FklCodegenInfo *info,
        FklSid_t fid,
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
        bcl = set_and_append_ins_with_unsigned_imm(m,
                bcl,
                (k >= FKL_FIX_INT_MIN && k <= FKL_FIX_INT_MAX)
                        ? FKL_OP_PUSH_I64F
                        : FKL_OP_PUSH_I64B,
                fklAddI64Const(info->runtime_kt, k),
                fid,
                line,
                scope);
    }
    return bcl;
}

static inline FklByteCode *append_push_bigint_ins_to_bc(InsAppendMode m,
        FklByteCode *bc,
        const FklBigInt *k,
        FklCodegenInfo *info) {
    if (fklIsBigIntGtLtI64(k))
        return set_and_append_ins_with_unsigned_imm_to_bc(m,
                bc,
                FKL_OP_PUSH_BI,
                fklAddBigIntConst(info->runtime_kt, k));
    else
        return set_and_append_ins_with_unsigned_imm_to_bc(m,
                bc,
                FKL_OP_PUSH_I64B,
                fklAddI64Const(info->runtime_kt, fklBigIntToI(k)));
}

static inline FklByteCode *append_push_bvec_ins_to_bc(InsAppendMode m,
        FklByteCode *bc,
        const FklBytevector *k,
        FklCodegenInfo *info) {
    return set_and_append_ins_with_unsigned_imm_to_bc(m,
            bc,
            FKL_OP_PUSH_BVEC,
            fklAddBvecConst(info->runtime_kt, k));
}

static inline FklByteCode *append_push_f64_ins_to_bc(InsAppendMode m,
        FklByteCode *bc,
        double k,
        FklCodegenInfo *info) {
    return set_and_append_ins_with_unsigned_imm_to_bc(m,
            bc,
            FKL_OP_PUSH_F64,
            fklAddF64Const(info->runtime_kt, k));
}

static inline FklByteCodelnt *append_push_proc_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t prototypeId,
        uint64_t len,
        FklSid_t fid,
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
        FklSid_t fid,
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
        FklSid_t fid,
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
        FklSid_t fid,
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
        FklSid_t fid,
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
        FklSid_t fid,
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
        FklSid_t fid,
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

static inline FklByteCodelnt *
create_bc_lnt(FklByteCode *bc, FklSid_t fid, uint32_t line, uint32_t scope) {
    FklByteCodelnt *r = fklCreateByteCodelnt(bc);
    r->ls = 1;
    r->l = (FklLineNumberTableItem *)fklZmalloc(sizeof(FklLineNumberTableItem));
    FKL_ASSERT(r->l);
    fklInitLineNumTabNode(&r->l[0], fid, 0, line, scope);
    return r;
}

static inline FklByteCodelnt *append_pop_loc_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t idx,
        FklSid_t fid,
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
        FklSid_t fid,
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
        FklSid_t fid,
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

static inline FklCodegenMacroScope *make_macro_scope_ref(
        FklCodegenMacroScope *s) {
    if (s)
        s->refcount++;
    return s;
}

FklCodegenQuest *fklCreateCodegenQuest(FklByteCodeProcesser f,
        FklCodegenQuestContext *context,
        FklCodegenNextExpression *nextExpression,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        FklCodegenEnv *env,
        uint64_t curline,
        FklCodegenQuest *prev,
        FklCodegenInfo *codegen) {
    FklCodegenQuest *r = (FklCodegenQuest *)fklZmalloc(sizeof(FklCodegenQuest));
    FKL_ASSERT(r);
    r->scope = scope;
    r->macroScope = make_macro_scope_ref(macroScope);
    r->processer = f;
    r->context = context;
    if (env)
        env->refcount += 1;
    r->nextExpression = nextExpression;
    r->env = env;
    r->curline = curline;
    r->codegen = codegen;
    r->prev = prev;
    codegen->refcount += 1;
    r->codegen = codegen;
    return r;
}

static void destroyCodegenQuest(FklCodegenQuest *quest) {
    FklCodegenNextExpression *nextExpression = quest->nextExpression;
    if (nextExpression) {
        if (nextExpression->t->finalizer)
            nextExpression->t->finalizer(nextExpression->context);
        fklZfree(nextExpression);
    }
    fklDestroyCodegenEnv(quest->env);
    fklDestroyCodegenMacroScope(quest->macroScope);
    quest->context->t->__finalizer(quest->context->data);
    fklZfree(quest->context);
    fklDestroyCodegenInfo(quest->codegen);
    fklZfree(quest);
}

#define FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(F,                             \
        STACK,                                                                 \
        NEXT_EXPRESSIONS,                                                      \
        SCOPE,                                                                 \
        MACRO_SCOPE,                                                           \
        ENV,                                                                   \
        LINE,                                                                  \
        CODEGEN,                                                               \
        CODEGEN_CONTEXT)                                                       \
    fklCodegenQuestVectorPushBack2((CODEGEN_CONTEXT),                          \
            fklCreateCodegenQuest((F),                                         \
                    (STACK),                                                   \
                    (NEXT_EXPRESSIONS),                                        \
                    (SCOPE),                                                   \
                    (MACRO_SCOPE),                                             \
                    (ENV),                                                     \
                    (LINE),                                                    \
                    NULL,                                                      \
                    (CODEGEN)))

#define BC_PROCESS(NAME)                                                       \
    static FklByteCodelnt *NAME(FklCodegenInfo *codegen,                       \
            FklCodegenEnv *env,                                                \
            uint32_t scope,                                                    \
            FklCodegenMacroScope *cms,                                         \
            FklCodegenQuestContext *context,                                   \
            FklSid_t fid,                                                      \
            uint64_t line,                                                     \
            FklCodegenErrorState *errorState,                                  \
            FklCodegenOuterCtx *outer_ctx)

#define GET_STACK(CONTEXT) ((CONTEXT)->t->__get_bcl_stack((CONTEXT)->data))
BC_PROCESS(_default_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    if (!fklByteCodelntVectorIsEmpty(stack))
        return *fklByteCodelntVectorPopBackNonNull(stack);
    else
        return NULL;
}

BC_PROCESS(_empty_bc_process) { return NULL; }

static FklByteCodelnt *sequnce_exp_bc_process(FklByteCodelntVector *stack,
        FklSid_t fid,
        uint32_t line,
        uint32_t scope) {
    if (stack->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));
        size_t top = stack->size;
        for (size_t i = 0; i < top; i++) {
            FklByteCodelnt *cur = stack->base[i];
            if (cur->bc->len) {
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
    FklByteCodelntVector *stack = GET_STACK(context);
    return sequnce_exp_bc_process(stack, fid, line, scope);
}

static inline void pushListItemToQueue(FklNastNode *list,
        FklNastNodeQueue *queue,
        FklNastNode **last) {
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr)
        fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(list->pair->car));
    if (last)
        *last = list;
}

static inline void pushListItemToStack(FklNastNode *list,
        FklNastNodeVector *stack,
        FklNastNode **last) {
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr)
        fklNastNodeVectorPushBack2(stack, list->pair->car);
    if (last)
        *last = list;
}

static inline int is_get_var_ref_ins(const FklInstruction *ins) {
    return ins->op >= FKL_OP_GET_VAR_REF && ins->op <= FKL_OP_GET_VAR_REF_X;
}

static inline FklBuiltinInlineFunc is_inlinable_func_ref(const FklByteCode *bc,
        FklCodegenEnv *env,
        uint32_t argNum,
        FklCodegenInfo *codegen) {
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
    FklByteCodelntVector *stack = GET_STACK(context);
    if (stack->size) {
        FklByteCodelnt *func = stack->base[0];
        FklByteCode *funcBc = func->bc;
        uint32_t argNum = stack->size - 1;
        FklBuiltinInlineFunc inlFunc = NULL;
        if (argNum < 4
                && (inlFunc = is_inlinable_func_ref(funcBc,
                            env,
                            argNum,
                            codegen))) {
            fklDestroyByteCodelnt(func);
            stack->size = 0;
            return inlFunc((FklByteCodelnt **)stack->base + 1,
                    fid,
                    line,
                    scope);
        } else {
            FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));
            FklByteCodelnt **base = stack->base;
            FklByteCodelnt **const end = base + stack->size;
            for (; base < end; ++base) {
                FklByteCodelnt *cur = *base;
                fklCodeLntConcat(retval, cur);
                fklDestroyByteCodelnt(cur);
            }
            stack->size = 0;
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

static int pushFuncallListToQueue(FklNastNode *list,
        FklNastNodeQueue *queue,
        FklNastNode **last,
        FklNastNode *const *builtin_pattern_node) {
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr)
        fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(list->pair->car));
    if (last)
        *last = list;
    return 0;
}

static void codegen_funcall(FklNastNode *rest,
        FklCodegenQuestVector *codegenQuestStack,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        FklCodegenEnv *env,
        FklCodegenInfo *codegen,
        FklCodegenErrorState *errorState) {
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    FklNastNode *last = NULL;
    int r = pushFuncallListToQueue(rest,
            queue,
            &last,
            codegen->outer_ctx->builtin_pattern_node);
    if (r || last->type != FKL_NAST_NIL) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(rest);
        while (!fklNastNodeQueueIsEmpty(queue))
            fklDestroyNastNode(*fklNastNodeQueuePop(queue));
        fklNastNodeQueueDestroy(queue);
    } else
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_funcall_exp_bc_process,
                createDefaultStackContext(fklByteCodelntVectorCreate(32)),
                createMustHasRetvalQueueNextExpression(queue),
                scope,
                macroScope,
                env,
                rest->curline,
                codegen,
                codegenQuestStack);
}

#define CODEGEN_ARGS                                                           \
    FklNastNode *origExp, FklPmatchHashMap *ht,                                \
            FklCodegenQuestVector *codegenQuestStack, uint32_t scope,          \
            FklCodegenMacroScope *macroScope, FklCodegenEnv *curEnv,           \
            FklCodegenInfo *codegen, FklCodegenOuterCtx *outer_ctx,            \
            FklCodegenErrorState *errorState, uint8_t must_has_retval

#define CODEGEN_FUNC(NAME) void NAME(CODEGEN_ARGS)

static CODEGEN_FUNC(codegen_begin) {
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_begin_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            createDefaultQueueNextExpression(queue),
            scope,
            macroScope,
            curEnv,
            rest->curline,
            codegen,
            codegenQuestStack);
}

static inline uint32_t enter_new_scope(uint32_t p, FklCodegenEnv *env) {
    uint32_t r = ++env->sc;
    FklCodegenEnvScope *scopes = (FklCodegenEnvScope *)fklZrealloc(env->scopes,
            r * sizeof(FklCodegenEnvScope));
    FKL_ASSERT(scopes);
    env->scopes = scopes;
    FklCodegenEnvScope *newScope = &scopes[r - 1];
    newScope->p = p;
    fklSymDefHashMapInit(&newScope->defs);
    newScope->start = 0;
    newScope->end = 0;
    if (p)
        newScope->start = scopes[p - 1].start + scopes[p - 1].end;
    newScope->empty = newScope->start;
    return r;
}

static inline int reset_flag_and_check_var_be_refed(uint8_t *flags,
        FklCodegenEnvScope *sc,
        uint32_t scope,
        FklCodegenEnv *env,
        FklFuncPrototypes *cp,
        uint32_t *start,
        uint32_t *end) {
    fklProcessUnresolveRef(env, scope, cp, NULL);
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
        FklSid_t fid,
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
        FklCodegenEnv *env,
        FklFuncPrototypes *pa,
        FklSid_t fid,
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
    FklByteCodelntVector *stack = GET_STACK(context);
    FklByteCodelnt *retval = sequnce_exp_bc_process(stack, fid, line, scope);
    check_and_close_ref(retval, scope, env, codegen->pts, fid, line);
    return retval;
}

static CODEGEN_FUNC(codegen_local) {
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    uint32_t cs = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
    pushListItemToQueue(rest, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_local_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            curEnv,
            rest->curline,
            codegen,
            codegenQuestStack);
}

static CODEGEN_FUNC(codegen_let0) {
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    uint32_t cs = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
    pushListItemToQueue(rest, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_local_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            curEnv,
            rest->curline,
            codegen,
            codegenQuestStack);
}

typedef struct Let1Context {
    FklByteCodelntVector stack;
    FklSidVector *ss;
} Let1Context;

static inline Let1Context *createLet1Context(FklSidVector *ss) {
    Let1Context *ctx = (Let1Context *)fklZmalloc(sizeof(Let1Context));
    FKL_ASSERT(ctx);
    ctx->ss = ss;
    fklByteCodelntVectorInit(&ctx->stack, 8);
    return ctx;
}

static FklByteCodelntVector *_let1_get_stack(void *d) {
    return &((Let1Context *)d)->stack;
}

static void _let1_put_bcl(void *d, FklByteCodelnt *bcl) {
    fklByteCodelntVectorPushBack2(&((Let1Context *)d)->stack, bcl);
}

static void _let1_finalizer(void *d) {
    Let1Context *dd = (Let1Context *)d;
    FklByteCodelntVector *s = &dd->stack;
    while (!fklByteCodelntVectorIsEmpty(s))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBackNonNull(s));
    fklByteCodelntVectorUninit(s);
    fklSidVectorDestroy(dd->ss);
    fklZfree(dd);
}

static FklCodegenQuestContextMethodTable Let1ContextMethodTable = {
    .__finalizer = _let1_finalizer,
    .__get_bcl_stack = _let1_get_stack,
    .__put_bcl = _let1_put_bcl,
};

static FklCodegenQuestContext *createLet1CodegenContext(FklSidVector *ss) {
    return createCodegenQuestContext(createLet1Context(ss),
            &Let1ContextMethodTable);
}

static inline size_t nast_list_len(const FklNastNode *list) {
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr)
        i++;
    return i;
}

static int is_valid_let_arg(const FklNastNode *node,
        FklNastNode *const *builtin_pattern_node) {
    return node->type == FKL_NAST_PAIR && fklIsNastNodeList(node)
        && nast_list_len(node) == 2 && node->pair->car->type == FKL_NAST_SYM;
}

static int is_valid_let_args(const FklNastNode *sl,
        FklCodegenEnv *env,
        uint32_t scope,
        FklSidVector *stack,
        FklNastNode *const *builtin_pattern_node) {
    if (fklIsNastNodeList(sl)) {
        for (; sl->type == FKL_NAST_PAIR; sl = sl->pair->cdr) {
            FklNastNode *cc = sl->pair->car;
            if (!is_valid_let_arg(cc, builtin_pattern_node))
                return 0;
            FklSid_t id = cc->pair->car->sym;
            if (fklIsSymbolDefined(id, scope, env))
                return 0;
            fklSidVectorPushBack2(stack, id);
            fklAddCodegenDefBySid(id, scope, env);
        }
        return 1;
    } else
        return 0;
}

static int is_valid_letrec_args(const FklNastNode *sl,
        FklCodegenEnv *env,
        uint32_t scope,
        FklSidVector *stack,
        FklNastNode *const *builtin_pattern_node) {
    if (fklIsNastNodeList(sl)) {
        for (; sl->type == FKL_NAST_PAIR; sl = sl->pair->cdr) {
            FklNastNode *cc = sl->pair->car;
            if (!is_valid_let_arg(cc, builtin_pattern_node))
                return 0;
            FklSid_t id = cc->pair->car->sym;
            if (fklIsSymbolDefined(id, scope, env))
                return 0;
            fklSidVectorPushBack2(stack, id);
            fklAddCodegenPreDefBySid(id, scope, 0, env);
        }
        return 1;
    } else
        return 0;
}

static inline FklNastNode *caddr_nast_node(const FklNastNode *node) {
    return node->pair->cdr->pair->cdr->pair->car;
}

static inline FklNastNode *caadr_nast_node(const FklNastNode *node) {
    return node->pair->cdr->pair->car->pair->car;
}

BC_PROCESS(_let1_exp_bc_process) {
    Let1Context *ctx = context->data;
    FklByteCodelntVector *bcls = &ctx->stack;
    FklSidVector *symbolStack = ctx->ss;

    FklByteCodelnt *retval = *fklByteCodelntVectorPopBackNonNull(bcls);
    FklSid_t *cur_sid = symbolStack->base;
    FklSid_t *const sid_end = cur_sid + symbolStack->size;
    for (; cur_sid < sid_end; ++cur_sid) {
        FklSid_t id = *cur_sid;
        uint32_t idx = fklAddCodegenDefBySid(id, scope, env)->idx;
        append_pop_loc_ins(INS_APPEND_FRONT, retval, idx, fid, line, scope);
    }
    if (!fklByteCodelntVectorIsEmpty(bcls)) {
        FklByteCodelnt *args = *fklByteCodelntVectorPopBackNonNull(bcls);
        fklCodeLntReverseConcat(args, retval);
        fklDestroyByteCodelnt(args);
    }
    return retval;
}

BC_PROCESS(_let_arg_exp_bc_process) {
    FklByteCodelntVector *bcls = GET_STACK(context);
    if (bcls->size) {
        FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));
        FklByteCodelnt **cur_bcl = bcls->base;
        FklByteCodelnt **const end = cur_bcl + bcls->size;
        for (; cur_bcl < end; ++cur_bcl) {
            FklByteCodelnt *cur = *cur_bcl;
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }
        bcls->size = 0;
        return retval;
    } else
        return NULL;
}

BC_PROCESS(_letrec_exp_bc_process) {
    FklByteCodelntVector *s = GET_STACK(context);

    FklByteCodelnt *body = *fklByteCodelntVectorPopBackNonNull(s);
    FklByteCodelnt *retval = *fklByteCodelntVectorPopBackNonNull(s);
    fklCodeLntConcat(retval, body);
    fklDestroyByteCodelnt(body);
    return retval;
}

BC_PROCESS(_letrec_arg_exp_bc_process) {
    Let1Context *ctx = context->data;
    FklByteCodelntVector *bcls = &ctx->stack;
    FklSidVector *symbolStack = ctx->ss;

    FklByteCodelnt *retval = create_0len_bcl();
    FklSid_t *cur_sid = symbolStack->base;
    FklSid_t *sid_end = cur_sid + symbolStack->size;
    FklByteCodelnt **cur_bcl = bcls->base;
    for (; cur_sid < sid_end; ++cur_sid, ++cur_bcl) {
        FklByteCodelnt *value_bc = *cur_bcl;
        FklSid_t id = *cur_sid;
        uint32_t idx = fklAddCodegenDefBySid(id, scope, env)->idx;
        fklResolveCodegenPreDef(id, scope, env, codegen->pts);
        fklCodeLntConcat(retval, value_bc);
        append_pop_loc_ins(INS_APPEND_BACK, retval, idx, fid, line, scope);
        fklDestroyByteCodelnt(value_bc);
    }
    bcls->size = 0;
    return retval;
}

static CODEGEN_FUNC(codegen_let1) {
    FklNastNode *const *builtin_pattern_node = outer_ctx->builtin_pattern_node;
    FklSidVector *symStack = fklSidVectorCreate(8);
    FklNastNode *firstSymbol =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    if (firstSymbol->type != FKL_NAST_SYM) {
        fklSidVectorDestroy(symStack);
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    FklNastNode **item =
            fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);
    FklNastNode *args = item ? *item : NULL;
    uint32_t cs = enter_new_scope(scope, curEnv);

    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);

    fklAddCodegenDefBySid(firstSymbol->sym, cs, curEnv);
    fklSidVectorPushBack2(symStack, firstSymbol->sym);

    FklNastNodeQueue *valueQueue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(valueQueue, fklMakeNastNodeRef(value));

    if (args) {
        if (!is_valid_let_args(args,
                    curEnv,
                    cs,
                    symStack,
                    builtin_pattern_node)) {
            cms->refcount = 1;
            fklDestroyNastNode(value);
            fklNastNodeQueueDestroy(valueQueue);
            fklDestroyCodegenMacroScope(cms);
            fklSidVectorDestroy(symStack);
            errorState->type = FKL_ERR_SYNTAXERROR;
            errorState->place = fklMakeNastNodeRef(origExp);
            return;
        }
        for (FklNastNode *cur = args; cur->type == FKL_NAST_PAIR;
                cur = cur->pair->cdr)
            fklNastNodeQueuePush2(valueQueue,
                    fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)));
    }

    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklCodegenQuest *let1Quest = fklCreateCodegenQuest(_let1_exp_bc_process,
            createLet1CodegenContext(symStack),
            NULL,
            cs,
            cms,
            curEnv,
            origExp->curline,
            NULL,
            codegen);

    fklCodegenQuestVectorPushBack2(codegenQuestStack, let1Quest);

    uint32_t len = fklNastNodeQueueLength(queue);

    FklCodegenQuest *restQuest = fklCreateCodegenQuest(_local_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(len)),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            curEnv,
            rest->curline,
            let1Quest,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, restQuest);

    len = fklNastNodeQueueLength(valueQueue);

    FklCodegenQuest *argQuest = fklCreateCodegenQuest(_let_arg_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(len)),
            createMustHasRetvalQueueNextExpression(valueQueue),
            scope,
            macroScope,
            curEnv,
            firstSymbol->curline,
            let1Quest,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, argQuest);
}

static inline FklNastNode *
create_nast_list(FklNastNode **a, size_t num, uint32_t line) {
    FklNastNode *r = NULL;
    FklNastNode **cur = &r;
    for (size_t i = 0; i < num; i++) {
        (*cur) = fklCreateNastNode(FKL_NAST_PAIR, a[i]->curline);
        (*cur)->pair = fklCreateNastPair();
        (*cur)->pair->car = a[i];
        cur = &(*cur)->pair->cdr;
    }
    (*cur) = fklCreateNastNode(FKL_NAST_NIL, line);
    return r;
}

static CODEGEN_FUNC(codegen_let81) {
    FklSid_t letHeadId =
            fklAddSymbolCstr("let", &outer_ctx->public_symbol_table)->v;
    FklNastNode *letHead =
            fklCreateNastNode(FKL_NAST_SYM, origExp->pair->car->curline);
    letHead->sym = letHeadId;

    FklNastNode *firstNameValue = cadr_nast_node(origExp);

    FklNastNode *args =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);

    FklNastNode *restLet8 = fklCreateNastNode(FKL_NAST_PAIR, args->curline);
    restLet8->pair = fklCreateNastPair();
    restLet8->pair->car = args;
    restLet8->pair->cdr = fklMakeNastNodeRef(rest);
    FklNastNode *old = origExp->pair->cdr;
    origExp->pair->cdr = restLet8;

    FklNastNode *a[3] = {
        letHead,
        fklMakeNastNodeRef(firstNameValue),
        fklMakeNastNodeRef(origExp),
    };
    letHead = create_nast_list(a, 3, origExp->curline);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(queue, letHead);
    fklDestroyNastNode(old);
    firstNameValue->pair->cdr =
            fklCreateNastNode(FKL_NAST_NIL, firstNameValue->pair->cdr->curline);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(1)),
            createDefaultQueueNextExpression(queue),
            scope,
            macroScope,
            curEnv,
            letHead->curline,
            codegen,
            codegenQuestStack);
}

static CODEGEN_FUNC(codegen_letrec) {
    FklNastNode *const *builtin_pattern_node = outer_ctx->builtin_pattern_node;
    FklSidVector *symStack = fklSidVectorCreate(8);
    FklNastNode *firstSymbol =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    if (firstSymbol->type != FKL_NAST_SYM) {
        fklSidVectorDestroy(symStack);
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    FklNastNode *args =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);
    uint32_t cs = enter_new_scope(scope, curEnv);

    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);

    fklAddCodegenPreDefBySid(firstSymbol->sym, cs, 0, curEnv);
    fklSidVectorPushBack2(symStack, firstSymbol->sym);

    if (!is_valid_letrec_args(args,
                curEnv,
                cs,
                symStack,
                builtin_pattern_node)) {
        cms->refcount = 1;
        fklDestroyCodegenMacroScope(cms);
        fklSidVectorDestroy(symStack);
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }

    FklNastNodeQueue *valueQueue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(valueQueue, fklMakeNastNodeRef(value));
    for (FklNastNode *cur = args; cur->type == FKL_NAST_PAIR;
            cur = cur->pair->cdr)
        fklNastNodeQueuePush2(valueQueue,
                fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)));

    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklCodegenQuest *let1Quest = fklCreateCodegenQuest(_letrec_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(2)),
            NULL,
            cs,
            cms,
            curEnv,
            origExp->curline,
            NULL,
            codegen);

    fklCodegenQuestVectorPushBack2(codegenQuestStack, let1Quest);

    uint32_t len = fklNastNodeQueueLength(queue);

    FklCodegenQuest *restQuest = fklCreateCodegenQuest(_local_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(len)),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            curEnv,
            rest->curline,
            let1Quest,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, restQuest);

    FklCodegenQuest *argQuest =
            fklCreateCodegenQuest(_letrec_arg_exp_bc_process,
                    createLet1CodegenContext(symStack),
                    createMustHasRetvalQueueNextExpression(valueQueue),
                    cs,
                    cms,
                    curEnv,
                    firstSymbol->curline,
                    let1Quest,
                    codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, argQuest);
}

static inline void insert_jmp_if_true_and_jmp_back_between(FklByteCodelnt *cond,
        FklByteCodelnt *rest,
        FklSid_t fid,
        uint32_t line,
        uint32_t scope) {
    FklByteCode *cond_bc = cond->bc;
    FklByteCode *rest_bc = rest->bc;
    uint32_t jmp_back_ins_len = 3;

    uint64_t cond_len = cond_bc->len;
    uint64_t rest_len = rest_bc->len;

    append_jmp_ins(INS_APPEND_BACK,
            cond,
            rest->bc->len + jmp_back_ins_len,
            JMP_IF_TRUE,
            JMP_FORWARD,
            fid,
            line,
            scope);

    append_jmp_ins(INS_APPEND_BACK,
            rest,
            rest->bc->len + cond->bc->len,
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
                rest->bc->len + jmp_back_ins_len,
                JMP_IF_TRUE,
                JMP_FORWARD,
                fid,
                line,
                scope);

        append_jmp_ins(INS_APPEND_BACK,
                rest,
                rest->bc->len + cond->bc->len,
                JMP_UNCOND,
                JMP_BACKWARD,
                fid,
                line,
                scope);

        jmp_back_actual_len = rest_bc->len - rest_len;
    }
}

BC_PROCESS(_do0_exp_bc_process) {
    FklByteCodelntVector *s = GET_STACK(context);
    FklByteCodelnt *rest = *fklByteCodelntVectorPopBackNonNull(s);
    FklByteCodelnt *value = *fklByteCodelntVectorPopBackNonNull(s);
    FklByteCodelnt *cond = *fklByteCodelntVectorPopBackNonNull(s);

    FklInstruction pop = create_op_ins(FKL_OP_DROP);

    fklByteCodeLntInsertFrontIns(&pop, rest, fid, line, scope);
    insert_jmp_if_true_and_jmp_back_between(cond, rest, fid, line, scope);

    fklCodeLntConcat(cond, rest);
    fklDestroyByteCodelnt(rest);

    if (value->bc->len)
        fklByteCodeLntPushBackIns(cond, &pop, fid, line, scope);
    fklCodeLntReverseConcat(cond, value);
    fklDestroyByteCodelnt(cond);

    return value;
}

BC_PROCESS(_do_rest_exp_bc_process) {
    FklByteCodelntVector *s = GET_STACK(context);
    if (s->size) {
        FklInstruction pop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *r = sequnce_exp_bc_process(s, fid, line, scope);
        fklByteCodeLntPushBackIns(r, &pop, fid, line, scope);
        return r;
    }
    return fklCreateByteCodelnt(fklCreateByteCode(0));
}

static CODEGEN_FUNC(codegen_do0) {
    FklNastNode *cond =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_cond);
    FklNastNode **item =
            fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    FklNastNode *value = item ? *item : NULL;

    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    uint32_t cs = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);

    FklCodegenQuest *do0Quest = fklCreateCodegenQuest(_do0_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(3)),
            NULL,
            cs,
            cms,
            curEnv,
            cond->curline,
            NULL,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do0Quest);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_do_rest_exp_bc_process,
            createDefaultStackContext(
                    fklByteCodelntVectorCreate(fklNastNodeQueueLength(queue))),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            curEnv,
            cond->curline,
            codegen,
            codegenQuestStack);

    if (value) {
        FklNastNodeQueue *vQueue = fklNastNodeQueueCreate();
        fklNastNodeQueuePush2(vQueue, fklMakeNastNodeRef(value));
        FklCodegenQuest *do0VQuest = fklCreateCodegenQuest(_default_bc_process,
                createDefaultStackContext(fklByteCodelntVectorCreate(1)),
                createMustHasRetvalQueueNextExpression(vQueue),
                cs,
                cms,
                curEnv,
                origExp->curline,
                do0Quest,
                codegen);
        fklCodegenQuestVectorPushBack2(codegenQuestStack, do0VQuest);
    } else {
        FklByteCodelnt *v = fklCreateByteCodelnt(fklCreateByteCode(0));
        FklByteCodelntVector *s = fklByteCodelntVectorCreate(1);
        fklByteCodelntVectorPushBack2(s, v);
        FklCodegenQuest *do0VQuest = fklCreateCodegenQuest(_default_bc_process,
                createDefaultStackContext(s),
                NULL,
                cs,
                cms,
                curEnv,
                origExp->curline,
                do0Quest,
                codegen);
        fklCodegenQuestVectorPushBack2(codegenQuestStack, do0VQuest);
    }
    FklNastNodeQueue *cQueue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(cQueue, fklMakeNastNodeRef(cond));
    FklCodegenQuest *do0CQuest = fklCreateCodegenQuest(_default_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(1)),
            createMustHasRetvalQueueNextExpression(cQueue),
            cs,
            cms,
            curEnv,
            origExp->curline,
            do0Quest,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do0CQuest);
}

static inline void destroy_next_exp_queue(FklNastNodeQueue *q) {
    while (!fklNastNodeQueueIsEmpty(q))
        fklDestroyNastNode(*fklNastNodeQueuePop(q));
    fklNastNodeQueueDestroy(q);
}

static inline int is_valid_do_var_bind(const FklNastNode *list,
        FklNastNode **nextV,
        FklNastNode *const *builtin_pattern_node) {
    if (!fklIsNastNodeList(list))
        return 0;
    if (list->pair->car->type != FKL_NAST_SYM)
        return 0;
    size_t len = nast_list_len(list);
    if (len != 2 && len != 3)
        return 0;
    if (len == 3) {
        FklNastNode *next = caddr_nast_node(list);
        *nextV = next;
    }
    return 1;
}

static inline int is_valid_do_bind_list(const FklNastNode *sl,
        FklCodegenEnv *env,
        uint32_t scope,
        FklSidVector *stack,
        FklSidVector *nstack,
        FklNastNodeQueue *valueQueue,
        FklNastNodeQueue *nextQueue,
        FklNastNode *const *builtin_pattern_node) {
    if (fklIsNastNodeList(sl)) {
        for (; sl->type == FKL_NAST_PAIR; sl = sl->pair->cdr) {
            FklNastNode *cc = sl->pair->car;
            FklNastNode *nextExp = NULL;
            if (!is_valid_do_var_bind(cc, &nextExp, builtin_pattern_node))
                return 0;
            FklSid_t id = cc->pair->car->sym;
            if (fklIsSymbolDefined(id, scope, env))
                return 0;
            uint32_t idx = fklAddCodegenDefBySid(id, scope, env)->idx;
            fklSidVectorPushBack2(stack, idx);
            fklNastNodeQueuePush2(valueQueue,
                    fklMakeNastNodeRef(cadr_nast_node(cc)));
            if (nextExp) {
                fklSidVectorPushBack2(nstack, idx);
                fklNastNodeQueuePush2(nextQueue, fklMakeNastNodeRef(nextExp));
            }
        }
        return 1;
    }
    return 0;
}

BC_PROCESS(_do1_init_val_bc_process) {
    FklByteCodelnt *ret = fklCreateByteCodelnt(fklCreateByteCode(0));
    Let1Context *ctx = (Let1Context *)(context->data);
    FklByteCodelntVector *s = &ctx->stack;
    FklSidVector *ss = ctx->ss;

    FklInstruction pop = create_op_ins(FKL_OP_DROP);

    uint64_t *idxbase = ss->base;
    FklByteCodelnt **bclBase = (FklByteCodelnt **)s->base;
    uint32_t top = s->size;
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
    Let1Context *ctx = (Let1Context *)(context->data);
    FklByteCodelntVector *s = &ctx->stack;
    FklSidVector *ss = ctx->ss;

    if (s->size) {
        FklByteCodelnt *ret = fklCreateByteCodelnt(fklCreateByteCode(0));
        FklInstruction pop = create_op_ins(FKL_OP_DROP);

        uint64_t *idxbase = ss->base;
        FklByteCodelnt **bclBase = (FklByteCodelnt **)s->base;
        uint32_t top = s->size;
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
    FklByteCodelntVector *s = GET_STACK(context);
    FklByteCodelnt *next =
            s->size == 5 ? *fklByteCodelntVectorPopBackNonNull(s) : NULL;
    FklByteCodelnt *rest = *fklByteCodelntVectorPopBackNonNull(s);
    FklByteCodelnt *value = *fklByteCodelntVectorPopBackNonNull(s);
    FklByteCodelnt *cond = *fklByteCodelntVectorPopBackNonNull(s);
    FklByteCodelnt *init = *fklByteCodelntVectorPopBackNonNull(s);

    FklInstruction pop = create_op_ins(FKL_OP_DROP);

    fklByteCodeLntInsertFrontIns(&pop, rest, fid, line, scope);
    if (next) {
        fklCodeLntConcat(rest, next);
        fklDestroyByteCodelnt(next);
    }

    insert_jmp_if_true_and_jmp_back_between(cond, rest, fid, line, scope);

    fklCodeLntConcat(cond, rest);
    fklDestroyByteCodelnt(rest);

    if (value->bc->len)
        fklByteCodeLntPushBackIns(cond, &pop, fid, line, scope);
    fklCodeLntReverseConcat(cond, value);
    fklDestroyByteCodelnt(cond);

    fklCodeLntReverseConcat(init, value);
    fklDestroyByteCodelnt(init);
    check_and_close_ref(value, scope, env, codegen->pts, fid, line);
    return value;
}

static CODEGEN_FUNC(codegen_do1) {
    FklNastNode *const *builtin_pattern_node = outer_ctx->builtin_pattern_node;
    FklNastNode *bindlist = cadr_nast_node(origExp);

    FklNastNode *cond =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_cond);
    FklNastNode **item =
            fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    FklNastNode *value = item ? *item : NULL;

    FklSidVector *symStack = fklSidVectorCreate(4);
    FklSidVector *nextSymStack = fklSidVectorCreate(4);

    uint32_t cs = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);

    FklNastNodeQueue *valueQueue = fklNastNodeQueueCreate();
    FklNastNodeQueue *nextValueQueue = fklNastNodeQueueCreate();
    if (!is_valid_do_bind_list(bindlist,
                curEnv,
                cs,
                symStack,
                nextSymStack,
                valueQueue,
                nextValueQueue,
                builtin_pattern_node)) {
        cms->refcount = 1;
        fklDestroyCodegenMacroScope(cms);
        fklSidVectorDestroy(symStack);
        fklSidVectorDestroy(nextSymStack);
        destroy_next_exp_queue(valueQueue);
        destroy_next_exp_queue(nextValueQueue);

        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }

    FklCodegenQuest *do1Quest = fklCreateCodegenQuest(_do1_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(5)),
            NULL,
            cs,
            cms,
            curEnv,
            origExp->curline,
            NULL,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do1Quest);

    FklCodegenQuest *do1NextValQuest =
            fklCreateCodegenQuest(_do1_next_val_bc_process,
                    createLet1CodegenContext(nextSymStack),
                    createMustHasRetvalQueueNextExpression(nextValueQueue),
                    cs,
                    cms,
                    curEnv,
                    origExp->curline,
                    do1Quest,
                    codegen);

    fklCodegenQuestVectorPushBack2(codegenQuestStack, do1NextValQuest);

    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklCodegenQuest *do1RestQuest =
            fklCreateCodegenQuest(_do_rest_exp_bc_process,
                    createDefaultStackContext(fklByteCodelntVectorCreate(
                            fklNastNodeQueueLength(queue))),
                    createDefaultQueueNextExpression(queue),
                    cs,
                    cms,
                    curEnv,
                    origExp->curline,
                    do1Quest,
                    codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do1RestQuest);

    if (value) {
        FklNastNodeQueue *vQueue = fklNastNodeQueueCreate();
        fklNastNodeQueuePush2(vQueue, fklMakeNastNodeRef(value));
        FklCodegenQuest *do1VQuest = fklCreateCodegenQuest(_default_bc_process,
                createDefaultStackContext(fklByteCodelntVectorCreate(1)),
                createMustHasRetvalQueueNextExpression(vQueue),
                cs,
                cms,
                curEnv,
                origExp->curline,
                do1Quest,
                codegen);
        fklCodegenQuestVectorPushBack2(codegenQuestStack, do1VQuest);
    } else {
        FklByteCodelnt *v = fklCreateByteCodelnt(fklCreateByteCode(0));
        FklByteCodelntVector *s = fklByteCodelntVectorCreate(1);
        fklByteCodelntVectorPushBack2(s, v);
        FklCodegenQuest *do1VQuest = fklCreateCodegenQuest(_default_bc_process,
                createDefaultStackContext(s),
                NULL,
                cs,
                cms,
                curEnv,
                origExp->curline,
                do1Quest,
                codegen);
        fklCodegenQuestVectorPushBack2(codegenQuestStack, do1VQuest);
    }

    FklNastNodeQueue *cQueue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(cQueue, fklMakeNastNodeRef(cond));
    FklCodegenQuest *do1CQuest = fklCreateCodegenQuest(_default_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(1)),
            createMustHasRetvalQueueNextExpression(cQueue),
            cs,
            cms,
            curEnv,
            origExp->curline,
            do1Quest,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do1CQuest);

    FklCodegenQuest *do1InitValQuest =
            fklCreateCodegenQuest(_do1_init_val_bc_process,
                    createLet1CodegenContext(symStack),
                    createMustHasRetvalQueueNextExpression(valueQueue),
                    scope,
                    macroScope,
                    curEnv,
                    origExp->curline,
                    do1Quest,
                    codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do1InitValQuest);
}

static inline FklSid_t get_sid_in_gs_with_id_in_ps(FklSid_t id,
        FklSymbolTable *gs,
        FklSymbolTable *ps) {
    return fklAddSymbol(fklGetSymbolWithId(id, ps), gs)->v;
}

static inline FklSid_t get_sid_with_idx(FklCodegenEnvScope *sc,
        uint32_t idx,
        FklSymbolTable *gs,
        FklSymbolTable *ps) {
    for (FklSymDefHashMapNode *l = sc->defs.first; l; l = l->next) {
        if (l->v.idx == idx)
            return get_sid_in_gs_with_id_in_ps(l->k.id, gs, ps);
    }
    return 0;
}

static inline FklSid_t get_sid_with_ref_idx(FklSymDefHashMap *refs,
        uint32_t idx,
        FklSymbolTable *gs,
        FklSymbolTable *ps) {
    for (FklSymDefHashMapNode *l = refs->first; l; l = l->next) {
        if (l->v.idx == idx)
            return get_sid_in_gs_with_id_in_ps(l->k.id, gs, ps);
    }
    return 0;
}

static inline FklByteCodelnt *process_set_var(FklByteCodelntVector *stack,
        FklCodegenInfo *codegen,
        FklCodegenOuterCtx *outer_ctx,
        FklCodegenEnv *env,
        uint32_t scope,
        FklSid_t fid,
        uint32_t line) {
    if (stack->size >= 2) {
        FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(stack);
        FklByteCodelnt *popVar = *fklByteCodelntVectorPopBackNonNull(stack);
        const FklInstruction *cur_ins = &cur->bc->code[0];
        if (fklIsPushProcIns(cur_ins)) {
            const FklInstruction *popVar_ins = &popVar->bc->code[0];
            if (fklIsPutLocIns(popVar_ins)) {
                FklInstructionArg arg;
                fklGetInsOpArg(cur_ins, &arg);
                uint64_t prototypeId = arg.ux;

                fklGetInsOpArg(popVar_ins, &arg);
                uint64_t idx = arg.ux;

                if (!codegen->pts->pa[prototypeId].sid)
                    codegen->pts->pa[prototypeId].sid =
                            get_sid_with_idx(&env->scopes[scope - 1],
                                    idx,
                                    codegen->runtime_symbol_table,
                                    &outer_ctx->public_symbol_table);
            } else if (fklIsPutVarRefIns(popVar_ins)) {
                FklInstructionArg arg;
                fklGetInsOpArg(cur_ins, &arg);
                uint64_t prototypeId = arg.ux;

                fklGetInsOpArg(popVar_ins, &arg);
                uint64_t idx = arg.ux;
                if (!codegen->pts->pa[prototypeId].sid)
                    codegen->pts->pa[prototypeId].sid =
                            get_sid_with_ref_idx(&env->refs,
                                    idx,
                                    codegen->runtime_symbol_table,
                                    &outer_ctx->public_symbol_table);
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
    FklSid_t id;
    uint32_t scope;
    uint32_t line;
    FklByteCodelntVector stack;
} DefineVarContext;

static void _def_var_context_finalizer(void *data) {
    DefineVarContext *ctx = data;
    FklByteCodelntVector *s = &ctx->stack;
    while (!fklByteCodelntVectorIsEmpty(s))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBackNonNull(s));
    fklByteCodelntVectorUninit(s);
    fklZfree(ctx);
}

static void _def_var_context_put_bcl(void *data, FklByteCodelnt *bcl) {
    DefineVarContext *ctx = data;
    FklByteCodelntVector *s = &ctx->stack;
    fklByteCodelntVectorPushBack2(s, bcl);
}

static FklByteCodelntVector *_def_var_context_get_bcl_stack(void *data) {
    return &((DefineVarContext *)data)->stack;
}

static const FklCodegenQuestContextMethodTable DefineVarContextMethodTable = {
    .__finalizer = _def_var_context_finalizer,
    .__put_bcl = _def_var_context_put_bcl,
    .__get_bcl_stack = _def_var_context_get_bcl_stack,
};

static inline FklCodegenQuestContext *
create_def_var_context(FklSid_t id, uint32_t scope, uint32_t line) {
    DefineVarContext *ctx =
            (DefineVarContext *)fklZmalloc(sizeof(DefineVarContext));
    FKL_ASSERT(ctx);
    ctx->id = id;
    ctx->scope = scope;
    ctx->line = line;
    fklByteCodelntVectorInit(&ctx->stack, 2);
    return createCodegenQuestContext(ctx, &DefineVarContextMethodTable);
}

BC_PROCESS(_def_var_exp_bc_process) {
    DefineVarContext *ctx = context->data;
    FklByteCodelntVector *stack = &ctx->stack;
    uint32_t idx = fklAddCodegenDefBySid(ctx->id, ctx->scope, env)->idx;
    fklResolveCodegenPreDef(ctx->id, ctx->scope, env, codegen->pts);
    fklByteCodelntVectorInsertFront2(stack,
            append_put_loc_ins(INS_APPEND_BACK,
                    NULL,
                    idx,
                    codegen->fid,
                    ctx->line,
                    scope));
    return process_set_var(stack, codegen, outer_ctx, env, scope, fid, line);
}

BC_PROCESS(_set_var_exp_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    return process_set_var(stack, codegen, outer_ctx, env, scope, fid, line);
}

BC_PROCESS(_lambda_exp_bc_process) {
    FklByteCodelnt *retval = NULL;
    FklByteCodelntVector *stack = GET_STACK(context);
    FklInstruction ret = create_op_ins(FKL_OP_RET);
    if (stack->size > 1) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        retval = fklCreateByteCodelnt(fklCreateByteCode(0));
        size_t top = stack->size;
        for (size_t i = 1; i < top; i++) {
            FklByteCodelnt *cur = stack->base[i];
            if (cur->bc->len) {
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
    fklCodeLntReverseConcat(stack->base[0], retval);
    fklDestroyByteCodelnt(stack->base[0]);
    stack->size = 0;
    fklScanAndSetTailCall(retval->bc);
    FklFuncPrototypes *pts = codegen->pts;
    fklUpdatePrototype(pts,
            env,
            codegen->runtime_symbol_table,
            &outer_ctx->public_symbol_table);
    append_push_proc_ins(INS_APPEND_FRONT,
            retval,
            env->prototypeId,
            retval->bc->len,
            fid,
            line,
            scope);
    return retval;
}

static inline FklByteCodelnt *processArgs(const FklNastNode *args,
        FklCodegenEnv *curEnv,
        FklCodegenInfo *codegen) {
    FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));

    uint32_t arg_count = 0;
    for (; args->type == FKL_NAST_PAIR; args = args->pair->cdr) {
        FklNastNode *cur = args->pair->car;
        if (cur->type != FKL_NAST_SYM) {
            fklDestroyByteCodelnt(retval);
            return NULL;
        }
        if (fklIsSymbolDefined(cur->sym, 1, curEnv)) {
            fklDestroyByteCodelnt(retval);
            return NULL;
        }
        if (arg_count > UINT16_MAX) {
            fklDestroyByteCodelnt(retval);
            return NULL;
        }
        fklAddCodegenDefBySid(cur->sym, 1, curEnv);
        ++arg_count;
    }
    if (args->type != FKL_NAST_NIL && args->type != FKL_NAST_SYM) {
        fklDestroyByteCodelnt(retval);
        return NULL;
    }

    int rest_list = 0;
    if (args->type == FKL_NAST_SYM) {
        if (fklIsSymbolDefined(args->sym, 1, curEnv)) {
            fklDestroyByteCodelnt(retval);
            return NULL;
        }
        rest_list = 1;
        fklAddCodegenDefBySid(args->sym, 1, curEnv);
    }
    FklInstruction check_arg = {
        .op = FKL_OP_CHECK_ARG,
        .ai = rest_list,
        .bu = arg_count,
    };
    fklByteCodeLntPushBackIns(retval,
            &check_arg,
            codegen->fid,
            args->curline,
            1);
    return retval;
}

static inline FklByteCodelnt *processArgsInStack(FklSidVector *stack,
        FklCodegenEnv *curEnv,
        FklCodegenInfo *codegen,
        uint64_t curline) {
    FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));
    uint32_t top = stack->size;
    uint64_t *base = stack->base;
    for (uint32_t i = 0; i < top; i++) {
        FklSid_t curId = base[i];

        fklAddCodegenDefBySid(curId, 1, curEnv);
    }
    FklInstruction check_arg = { .op = FKL_OP_CHECK_ARG, .ai = 0, .bu = top };
    fklByteCodeLntPushBackIns(retval, &check_arg, codegen->fid, curline, 1);
    return retval;
}

static inline FklSymDefHashMapMutElm *sid_ht_to_idx_key_ht(
        const FklCodegenEnv *env,
        FklSymbolTable *runtime_st,
        FklSymbolTable *pst) {
    const FklSymDefHashMap *sht = &env->refs;
    FklSymDefHashMapMutElm *refs;
    if (!sht->count)
        refs = NULL;
    else {
        refs = (FklSymDefHashMapMutElm *)fklZmalloc(
                sht->count * sizeof(FklSymDefHashMapMutElm));
        FKL_ASSERT(refs);
    }
    FklSymDefHashMapNode *list = sht->first;
    for (; list; list = list->next) {
        FklSid_t sid = 0;
        if (env->is_debugging || list->v.cidx == FKL_VAR_REF_INVALID_CIDX) {
            const FklString *sym = fklGetSymbolWithId(list->k.id, pst);
            sid = fklAddSymbol(sym, runtime_st)->v;
        }

        refs[list->v.idx] = (FklSymDefHashMapMutElm){
            .k = { .id = sid, .scope = 0 },
            .v = { .idx = list->v.idx,
                .cidx = list->v.cidx,
                .isLocal = list->v.isLocal,
                .isConst = 0 },
        };
    }
    return refs;
}

void fklInitFuncPrototypeWithEnv(FklFuncPrototype *cpt,
        FklCodegenInfo *info,
        FklCodegenEnv *env,
        FklSid_t sid,
        uint32_t line,
        FklSymbolTable *pst) {
    cpt->lcount = env->lcount;
    cpt->refs = sid_ht_to_idx_key_ht(env, info->runtime_symbol_table, pst);
    cpt->rcount = env->refs.count;
    cpt->sid = sid;
    cpt->fid = info->fid;
    cpt->line = line;
}

static inline void create_and_insert_to_pool(FklCodegenInfo *info,
        uint32_t p,
        FklCodegenEnv *env,
        FklSid_t sid,
        uint32_t line,
        FklSymbolTable *pst) {
    FklSid_t fid = info->fid;
    FklSymbolTable *gst = info->runtime_symbol_table;
    FklFuncPrototypes *cp = info->pts;
    cp->count += 1;
    FklFuncPrototype *pts = (FklFuncPrototype *)fklZrealloc(cp->pa,
            (cp->count + 1) * sizeof(FklFuncPrototype));
    FKL_ASSERT(pts);
    cp->pa = pts;
    FklFuncPrototype *cpt = &pts[cp->count];
    env->prototypeId = cp->count;
    cpt->lcount = env->lcount;
    cpt->refs = sid_ht_to_idx_key_ht(env, gst, pst);
    cpt->rcount = env->refs.count;
    cpt->sid = sid;
    cpt->fid = fid;
    cpt->line = line;

    if (info->work.env_work_cb)
        info->work.env_work_cb(info, env, info->work.work_ctx);
}

void fklCreateFuncPrototypeAndInsertToPool(FklCodegenInfo *info,
        uint32_t p,
        FklCodegenEnv *env,
        FklSid_t sid,
        uint32_t line,
        FklSymbolTable *pst) {
    create_and_insert_to_pool(info, p, env, sid, line, pst);
}

BC_PROCESS(_named_let_set_var_exp_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    FklByteCodelnt *popVar = NULL;
    if (stack->size >= 2) {
        FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(stack);
        popVar = *fklByteCodelntVectorPopBackNonNull(stack);
        fklCodeLntReverseConcat(cur, popVar);
        fklDestroyByteCodelnt(cur);
    } else {
        FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(stack);
        popVar = cur;
        FklInstruction pushNil = create_op_ins(FKL_OP_PUSH_NIL);
        fklByteCodeLntInsertFrontIns(&pushNil, popVar, fid, line, scope);
    }
    check_and_close_ref(popVar, scope, env, codegen->pts, fid, line);
    return popVar;
}

static CODEGEN_FUNC(codegen_named_let0) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_arg0);
    if (name->type != FKL_NAST_SYM) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    uint32_t cs = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_funcall_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(2)),
            NULL,
            cs,
            cms,
            curEnv,
            name->curline,
            codegen,
            codegenQuestStack);

    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklByteCodelntVector *stack = fklByteCodelntVectorCreate(2);
    uint32_t idx = fklAddCodegenDefBySid(name->sym, cs, curEnv)->idx;
    fklByteCodelntVectorPushBack2(stack,
            append_put_loc_ins(INS_APPEND_BACK,
                    NULL,
                    idx,
                    codegen->fid,
                    name->curline,
                    scope));

    fklReplacementHashMapAdd2(cms->replacements,
            fklAddSymbolCstr("*func*", pst)->v,
            name);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_named_let_set_var_exp_bc_process,
            createDefaultStackContext(stack),
            NULL,
            cs,
            cms,
            curEnv,
            name->curline,
            codegen,
            codegenQuestStack);

    FklCodegenEnv *lambdaCodegenEnv = fklCreateCodegenEnv(curEnv, cs, cms);
    FklByteCodelntVector *lStack = fklByteCodelntVectorCreate(2);
    FklNastNode *argsNode = caddr_nast_node(origExp);
    FklByteCodelnt *argBc = processArgs(argsNode, lambdaCodegenEnv, codegen);
    fklByteCodelntVectorPushBack2(lStack, argBc);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);

    create_and_insert_to_pool(codegen,
            curEnv->prototypeId,
            lambdaCodegenEnv,
            get_sid_in_gs_with_id_in_ps(name->sym,
                    codegen->runtime_symbol_table,
                    pst),
            origExp->curline,
            pst);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_lambda_exp_bc_process,
            createDefaultStackContext(lStack),
            createDefaultQueueNextExpression(queue),
            1,
            lambdaCodegenEnv->macros,
            lambdaCodegenEnv,
            rest->curline,
            codegen,
            codegenQuestStack);
}

static CODEGEN_FUNC(codegen_named_let1) {
    FklNastNode *const *builtin_pattern_node = outer_ctx->builtin_pattern_node;
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_arg0);
    if (name->type != FKL_NAST_SYM) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    FklSidVector *symStack = fklSidVectorCreate(8);
    FklNastNode *firstSymbol =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    if (firstSymbol->type != FKL_NAST_SYM) {
        fklSidVectorDestroy(symStack);
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    FklNastNode *args =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    uint32_t cs = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);

    FklCodegenEnv *lambdaCodegenEnv = fklCreateCodegenEnv(curEnv, cs, cms);
    fklAddCodegenDefBySid(firstSymbol->sym, 1, lambdaCodegenEnv);
    fklSidVectorPushBack2(symStack, firstSymbol->sym);

    if (!is_valid_let_args(args,
                lambdaCodegenEnv,
                1,
                symStack,
                builtin_pattern_node)) {
        lambdaCodegenEnv->refcount = 1;
        fklDestroyCodegenEnv(lambdaCodegenEnv);
        fklSidVectorDestroy(symStack);
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }

    FklNastNodeQueue *valueQueue = fklNastNodeQueueCreate();

    fklNastNodeQueuePush2(valueQueue, fklMakeNastNodeRef(value));
    for (FklNastNode *cur = args; cur->type == FKL_NAST_PAIR;
            cur = cur->pair->cdr)
        fklNastNodeQueuePush2(valueQueue,
                fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)));

    FklCodegenQuest *funcallQuest =
            fklCreateCodegenQuest(_funcall_exp_bc_process,
                    createDefaultStackContext(fklByteCodelntVectorCreate(2)),
                    NULL,
                    cs,
                    cms,
                    curEnv,
                    name->curline,
                    NULL,
                    codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, funcallQuest);

    FklByteCodelntVector *stack = fklByteCodelntVectorCreate(2);
    uint32_t idx = fklAddCodegenDefBySid(name->sym, cs, curEnv)->idx;
    fklByteCodelntVectorPushBack2(stack,
            append_put_loc_ins(INS_APPEND_BACK,
                    NULL,
                    idx,
                    codegen->fid,
                    name->curline,
                    scope));

    FklSymbolTable *pst = &outer_ctx->public_symbol_table;

    fklReplacementHashMapAdd2(cms->replacements,
            fklAddSymbolCstr("*func*", pst)->v,
            name);
    fklCodegenQuestVectorPushBack2(codegenQuestStack,
            fklCreateCodegenQuest(_let_arg_exp_bc_process,
                    createDefaultStackContext(fklByteCodelntVectorCreate(
                            fklNastNodeQueueLength(valueQueue))),
                    createMustHasRetvalQueueNextExpression(valueQueue),
                    scope,
                    macroScope,
                    curEnv,
                    firstSymbol->curline,
                    funcallQuest,
                    codegen));

    fklCodegenQuestVectorPushBack2(codegenQuestStack,
            fklCreateCodegenQuest(_named_let_set_var_exp_bc_process,
                    createDefaultStackContext(stack),
                    NULL,
                    cs,
                    cms,
                    curEnv,
                    name->curline,
                    funcallQuest,
                    codegen));

    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklByteCodelntVector *lStack = fklByteCodelntVectorCreate(2);

    FklByteCodelnt *argBc = processArgsInStack(symStack,
            lambdaCodegenEnv,
            codegen,
            firstSymbol->curline);

    fklByteCodelntVectorPushBack2(lStack, argBc);

    fklSidVectorDestroy(symStack);
    create_and_insert_to_pool(codegen,
            curEnv->prototypeId,
            lambdaCodegenEnv,
            get_sid_in_gs_with_id_in_ps(name->sym,
                    codegen->runtime_symbol_table,
                    pst),
            origExp->curline,
            pst);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_lambda_exp_bc_process,
            createDefaultStackContext(lStack),
            createDefaultQueueNextExpression(queue),
            1,
            lambdaCodegenEnv->macros,
            lambdaCodegenEnv,
            rest->curline,
            codegen,
            codegenQuestStack);
}

BC_PROCESS(_and_exp_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    if (stack->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));
        while (!fklByteCodelntVectorIsEmpty(stack)) {
            if (retval->bc->len) {
                fklByteCodeLntInsertFrontIns(&drop, retval, fid, line, scope);
                append_jmp_ins(INS_APPEND_FRONT,
                        retval,
                        retval->bc->len,
                        JMP_IF_FALSE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
            }
            FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(stack);
            fklCodeLntReverseConcat(cur, retval);
            fklDestroyByteCodelnt(cur);
        }
        return retval;
    } else
        return append_push_i64_ins(INS_APPEND_BACK,
                NULL,
                1,
                codegen,
                fid,
                line,
                scope);
}

static CODEGEN_FUNC(codegen_and) {
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    uint32_t cs = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_and_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            curEnv,
            rest->curline,
            codegen,
            codegenQuestStack);
}

BC_PROCESS(_or_exp_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    if (stack->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));
        while (!fklByteCodelntVectorIsEmpty(stack)) {
            if (retval->bc->len) {
                fklByteCodeLntInsertFrontIns(&drop, retval, fid, line, scope);
                append_jmp_ins(INS_APPEND_FRONT,
                        retval,
                        retval->bc->len,
                        JMP_IF_TRUE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
            }
            FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(stack);
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
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    uint32_t cs = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
    pushListItemToQueue(rest, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_or_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            curEnv,
            rest->curline,
            codegen,
            codegenQuestStack);
}

void fklUpdatePrototypeRef(FklFuncPrototypes *cp,
        FklCodegenEnv *env,
        FklSymbolTable *runtime_st,
        const FklSymbolTable *pst) {
    FKL_ASSERT(env->pdef.count == 0);
    FklFuncPrototype *pts = &cp->pa[env->prototypeId];
    FklSymDefHashMap *eht = &env->refs;
    uint32_t count = eht->count;

    FklSymDefHashMapMutElm *refs;
    if (!count)
        refs = NULL;
    else {
        refs = (FklSymDefHashMapMutElm *)fklZrealloc(pts->refs,
                count * sizeof(FklSymDefHashMapMutElm));
        FKL_ASSERT(refs);
    }
    pts->refs = refs;
    pts->rcount = count;
    for (FklSymDefHashMapNode *list = eht->first; list; list = list->next) {

        FklSid_t sid = 0;
        if (env->is_debugging || list->v.cidx == FKL_VAR_REF_INVALID_CIDX) {
            const FklString *sym = fklGetSymbolWithId(list->k.id, pst);
            sid = fklAddSymbol(sym, runtime_st)->v;
        }

        refs[list->v.idx] = (FklSymDefHashMapMutElm){
            .k = { .id = sid, .scope = list->k.scope },
            .v = { .idx = list->v.idx,
                .cidx = list->v.cidx,
                .isLocal = list->v.isLocal,
                .isConst = 0 },
        };
    }
}

void fklUpdatePrototype(FklFuncPrototypes *cp,
        FklCodegenEnv *env,
        FklSymbolTable *runtime_st,
        const FklSymbolTable *pst) {
    FKL_ASSERT(env->pdef.count == 0);
    FklFuncPrototype *pts = &cp->pa[env->prototypeId];
    pts->lcount = env->lcount;
    fklProcessUnresolveRef(env, 1, cp, NULL);
    fklUpdatePrototypeRef(cp, env, runtime_st, pst);
}

FklCodegenEnv *fklCreateCodegenEnv(FklCodegenEnv *prev,
        uint32_t pscope,
        FklCodegenMacroScope *macroScope) {
    FklCodegenEnv *r = (FklCodegenEnv *)fklZcalloc(1, sizeof(FklCodegenEnv));
    FKL_ASSERT(r);
    r->pscope = pscope;
    r->sc = 0;
    r->scopes = NULL;
    enter_new_scope(0, r);
    r->prototypeId = 1;
    r->prev = prev;
    r->refcount = 0;
    r->lcount = 0;
    r->slotFlags = NULL;
    r->is_debugging = 0;
    fklSymDefHashMapInit(&r->refs);
    fklUnReSymbolRefVectorInit(&r->uref, 8);
    fklPredefHashMapInit(&r->pdef);
    fklPreDefRefVectorInit(&r->ref_pdef, 8);
    r->macros = make_macro_scope_ref(fklCreateCodegenMacroScope(macroScope));
    if (prev)
        prev->refcount += 1;
    return r;
}

void fklDestroyCodegenEnv(FklCodegenEnv *env) {
    while (env) {
        env->refcount -= 1;
        if (!env->refcount) {
            FklCodegenEnv *cur = env;
            env = env->prev;
            uint32_t sc = cur->sc;
            FklCodegenEnvScope *scopes = cur->scopes;
            for (uint32_t i = 0; i < sc; i++)
                fklSymDefHashMapUninit(&scopes[i].defs);
            fklZfree(scopes);
            fklZfree(cur->slotFlags);
            fklSymDefHashMapUninit(&cur->refs);
            FklUnReSymbolRefVector *unref = &cur->uref;
            fklUnReSymbolRefVectorUninit(unref);
            fklDestroyCodegenMacroScope(cur->macros);

            fklPredefHashMapUninit(&cur->pdef);
            fklPreDefRefVectorUninit(&cur->ref_pdef);
            fklZfree(cur);
        } else
            return;
    }
}

int fklIsReplacementDefined(FklSid_t id, FklCodegenEnv *env) {
    return fklReplacementHashMapGet2(env->macros->replacements, id) != NULL;
}

FklNastNode *fklGetReplacement(FklSid_t id, const FklReplacementHashMap *rep) {
    FklNastNode **pn = fklReplacementHashMapGet2(rep, id);
    return pn ? *pn : NULL;
}

static CODEGEN_FUNC(codegen_lambda) {
    FklNastNode *args =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklCodegenEnv *lambdaCodegenEnv =
            fklCreateCodegenEnv(curEnv, scope, macroScope);
    FklByteCodelnt *argsBc = processArgs(args, lambdaCodegenEnv, codegen);
    if (!argsBc) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        lambdaCodegenEnv->refcount++;
        fklDestroyCodegenEnv(lambdaCodegenEnv);
        return;
    }
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklByteCodelntVector *stack = fklByteCodelntVectorCreate(32);
    fklByteCodelntVectorPushBack2(stack, argsBc);
    create_and_insert_to_pool(codegen,
            curEnv->prototypeId,
            lambdaCodegenEnv,
            0,
            origExp->curline,
            &outer_ctx->public_symbol_table);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_lambda_exp_bc_process,
            createDefaultStackContext(stack),
            createDefaultQueueNextExpression(queue),
            1,
            lambdaCodegenEnv->macros,
            lambdaCodegenEnv,
            rest->curline,
            codegen,
            codegenQuestStack);
}

static inline int
is_constant_defined(FklSid_t id, uint32_t scope, FklCodegenEnv *curEnv) {
    FklSymDefHashMapElm *def = fklGetCodegenDefByIdInScope(id, scope, curEnv);
    return def && def->v.isConst;
}

static inline int
is_variable_defined(FklSid_t id, uint32_t scope, FklCodegenEnv *curEnv) {
    FklSymDefHashMapElm *def = fklGetCodegenDefByIdInScope(id, scope, curEnv);
    return def != NULL;
}

static CODEGEN_FUNC(codegen_define) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    if (name->type != FKL_NAST_SYM) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    if (is_constant_defined(name->sym, scope, curEnv)) {
        errorState->type = FKL_ERR_ASSIGN_CONSTANT;
        errorState->place = fklMakeNastNodeRef(name);
        return;
    }
    if (!is_variable_defined(name->sym, scope, curEnv))
        fklAddCodegenPreDefBySid(name->sym, scope, 0, curEnv);

    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(value));
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_def_var_exp_bc_process,
            create_def_var_context(name->sym, scope, name->curline),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macroScope,
            curEnv,
            name->curline,
            codegen,
            codegenQuestStack);
}

static inline FklUnReSymbolRef *
get_resolvable_assign_ref(FklSid_t id, uint32_t scope, FklCodegenEnv *env) {
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
    DefineVarContext *ctx = context->data;
    FklByteCodelntVector *stack = &ctx->stack;
    FklUnReSymbolRef *assign_uref =
            get_resolvable_assign_ref(ctx->id, ctx->scope, env);
    if (assign_uref) {
        FklNastNode *place = fklCreateNastNode(FKL_NAST_SYM, assign_uref->line);
        place->sym = ctx->id;
        errorState->type = FKL_ERR_ASSIGN_CONSTANT;
        errorState->line = assign_uref->line;
        errorState->place = place;
        const FklString *sym = fklGetSymbolWithId(assign_uref->fid,
                codegen->runtime_symbol_table);

        errorState->fid =
                assign_uref->fid
                        ? fklAddSymbol(sym, &outer_ctx->public_symbol_table)->v
                        : 0;
        return NULL;
    }
    FklSymDef *def = fklAddCodegenDefBySid(ctx->id, ctx->scope, env);
    def->isConst = 1;
    uint32_t idx = def->idx;
    fklResolveCodegenPreDef(ctx->id, ctx->scope, env, codegen->pts);
    fklByteCodelntVectorInsertFront2(stack,
            append_put_loc_ins(INS_APPEND_BACK,
                    NULL,
                    idx,
                    codegen->fid,
                    ctx->line,
                    ctx->scope));
    return process_set_var(stack, codegen, outer_ctx, env, scope, fid, line);
}

static CODEGEN_FUNC(codegen_defconst) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    if (name->type != FKL_NAST_SYM) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    if (is_constant_defined(name->sym, scope, curEnv)) {
        errorState->type = FKL_ERR_ASSIGN_CONSTANT;
        errorState->place = fklMakeNastNodeRef(name);
        return;
    }
    if (is_variable_defined(name->sym, scope, curEnv)) {
        errorState->type = FKL_ERR_REDEFINE_VARIABLE_AS_CONSTANT;
        errorState->place = fklMakeNastNodeRef(name);
        return;
    } else
        fklAddCodegenPreDefBySid(name->sym, scope, 1, curEnv);

    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(value));
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_def_const_exp_bc_process,
            create_def_var_context(name->sym, scope, name->curline),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macroScope,
            curEnv,
            name->curline,
            codegen,
            codegenQuestStack);
}

static CODEGEN_FUNC(codegen_defun) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *args =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    if (name->type != FKL_NAST_SYM) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    if (is_constant_defined(name->sym, scope, curEnv)) {
        errorState->type = FKL_ERR_ASSIGN_CONSTANT;
        errorState->place = fklMakeNastNodeRef(name);
        return;
    }

    FklCodegenEnv *lambdaCodegenEnv =
            fklCreateCodegenEnv(curEnv, scope, macroScope);
    FklByteCodelnt *argsBc = processArgs(args, lambdaCodegenEnv, codegen);
    if (!argsBc) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        lambdaCodegenEnv->refcount++;
        fklDestroyCodegenEnv(lambdaCodegenEnv);
        return;
    }

    if (!is_variable_defined(name->sym, scope, curEnv))
        fklAddCodegenPreDefBySid(name->sym, scope, 0, curEnv);

    FklCodegenQuest *prevQuest = fklCreateCodegenQuest(_def_var_exp_bc_process,
            create_def_var_context(name->sym, scope, name->curline),
            NULL,
            scope,
            macroScope,
            curEnv,
            name->curline,
            NULL,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, prevQuest);

    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklByteCodelntVector *lStack = fklByteCodelntVectorCreate(32);
    fklByteCodelntVectorPushBack2(lStack, argsBc);
    create_and_insert_to_pool(codegen,
            curEnv->prototypeId,
            lambdaCodegenEnv,
            get_sid_in_gs_with_id_in_ps(name->sym,
                    codegen->runtime_symbol_table,
                    pst),
            origExp->curline,
            pst);
    FklNastNode *name_str = fklCreateNastNode(FKL_NAST_STR, name->curline);
    name_str->str = fklCopyString(fklGetSymbolWithId(name->sym, pst));
    fklReplacementHashMapAdd2(lambdaCodegenEnv->macros->replacements,
            fklAddSymbolCstr("*func*", pst)->v,
            name_str);
    fklDestroyNastNode(name_str);
    FklCodegenQuest *cur = fklCreateCodegenQuest(_lambda_exp_bc_process,
            createDefaultStackContext(lStack),
            createDefaultQueueNextExpression(queue),
            1,
            lambdaCodegenEnv->macros,
            lambdaCodegenEnv,
            rest->curline,
            prevQuest,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, cur);
}

static CODEGEN_FUNC(codegen_defun_const) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *args =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    if (name->type != FKL_NAST_SYM) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    if (is_constant_defined(name->sym, scope, curEnv)) {
        errorState->type = FKL_ERR_ASSIGN_CONSTANT;
        errorState->place = fklMakeNastNodeRef(name);
        return;
    }
    if (is_variable_defined(name->sym, scope, curEnv)) {
        errorState->type = FKL_ERR_REDEFINE_VARIABLE_AS_CONSTANT;
        errorState->place = fklMakeNastNodeRef(name);
        return;
    }

    FklCodegenEnv *lambdaCodegenEnv =
            fklCreateCodegenEnv(curEnv, scope, macroScope);
    FklByteCodelnt *argsBc = processArgs(args, lambdaCodegenEnv, codegen);
    if (!argsBc) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        lambdaCodegenEnv->refcount++;
        fklDestroyCodegenEnv(lambdaCodegenEnv);
        return;
    }

    FklCodegenQuest *prevQuest =
            fklCreateCodegenQuest(_def_const_exp_bc_process,
                    create_def_var_context(name->sym, scope, name->curline),
                    NULL,
                    scope,
                    macroScope,
                    curEnv,
                    name->curline,
                    NULL,
                    codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, prevQuest);

    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklByteCodelntVector *lStack = fklByteCodelntVectorCreate(32);
    fklByteCodelntVectorPushBack2(lStack, argsBc);
    create_and_insert_to_pool(codegen,
            curEnv->prototypeId,
            lambdaCodegenEnv,
            get_sid_in_gs_with_id_in_ps(name->sym,
                    codegen->runtime_symbol_table,
                    pst),
            origExp->curline,
            pst);
    FklNastNode *name_str = fklCreateNastNode(FKL_NAST_STR, name->curline);
    name_str->str = fklCopyString(fklGetSymbolWithId(name->sym, pst));
    fklReplacementHashMapAdd2(lambdaCodegenEnv->macros->replacements,
            fklAddSymbolCstr("*func*", pst)->v,
            name_str);
    fklDestroyNastNode(name_str);
    FklCodegenQuest *cur = fklCreateCodegenQuest(_lambda_exp_bc_process,
            createDefaultStackContext(lStack),
            createDefaultQueueNextExpression(queue),
            1,
            lambdaCodegenEnv->macros,
            lambdaCodegenEnv,
            rest->curline,
            prevQuest,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, cur);
}

static CODEGEN_FUNC(codegen_setq) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    if (name->type != FKL_NAST_SYM) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    FklByteCodelntVector *stack = fklByteCodelntVectorCreate(2);
    fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(value));
    FklSymDefHashMapElm *def =
            fklFindSymbolDefByIdAndScope(name->sym, scope, curEnv);
    if (def) {
        if (def->v.isConst) {
            errorState->type = FKL_ERR_ASSIGN_CONSTANT;
            errorState->place = fklMakeNastNodeRef(name);
            return;
        }
        fklByteCodelntVectorPushBack2(stack,
                append_put_loc_ins(INS_APPEND_BACK,
                        NULL,
                        def->v.idx,
                        codegen->fid,
                        name->curline,
                        scope));
    } else {
        FklSymDefHashMapElm *def = fklAddCodegenRefBySid(name->sym,
                curEnv,
                codegen->fid,
                name->curline,
                1);
        if (def->v.isConst) {
            errorState->type = FKL_ERR_ASSIGN_CONSTANT;
            errorState->place = fklMakeNastNodeRef(name);
            return;
        }
        fklByteCodelntVectorPushBack2(stack,
                append_put_var_ref_ins(INS_APPEND_BACK,
                        NULL,
                        def->v.idx,
                        codegen->fid,
                        name->curline,
                        scope));
    }
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_set_var_exp_bc_process,
            createDefaultStackContext(stack),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macroScope,
            curEnv,
            name->curline,
            codegen,
            codegenQuestStack);
}

static inline void push_default_codegen_quest(FklNastNode *value,
        FklCodegenQuestVector *codegenQuestStack,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        FklCodegenEnv *curEnv,
        FklCodegenQuest *prev,
        FklCodegenInfo *codegen) {
    FklByteCodelntVector *stack = fklByteCodelntVectorCreate(1);
    fklByteCodelntVectorPushBack2(stack,
            create_bc_lnt(fklCodegenNode(value, codegen),
                    codegen->fid,
                    value->curline,
                    scope));
    FklCodegenQuest *quest = fklCreateCodegenQuest(_default_bc_process,
            createDefaultStackContext(stack),
            NULL,
            scope,
            macroScope,
            curEnv,
            value->curline,
            prev,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, quest);
}

static CODEGEN_FUNC(codegen_macroexpand) {
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    fklMakeNastNodeRef(value);
    if (value->type == FKL_NAST_PAIR)
        value = fklTryExpandCodegenMacro(value,
                codegen,
                curEnv->macros,
                errorState);
    if (errorState->type)
        return;
    push_default_codegen_quest(value,
            codegenQuestStack,
            scope,
            macroScope,
            curEnv,
            NULL,
            codegen);
    fklDestroyNastNode(value);
}

static inline void uninit_codegen_macro(FklCodegenMacro *macro) {
    fklDestroyNastNode(macro->pattern);
    macro->pattern = NULL;
    fklDestroyNastNode(macro->origin_exp);
    macro->origin_exp = NULL;
    fklDestroyByteCodelnt(macro->bcl);
    macro->bcl = NULL;
}

static void add_compiler_macro(FklCodegenMacro **pmacro,
        FklNastNode *pattern,
        FklNastNode *origin_exp,
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
        FklCodegenMacro *macro = fklCreateCodegenMacro(pattern,
                origin_exp,
                bcl,
                *phead,
                prototype_id);
        *phead = macro;
    } else if (coverState == FKL_PATTERN_EQUAL) {
        FklCodegenMacro *macro = *pmacro;
        uninit_codegen_macro(macro);
        macro->prototype_id = prototype_id;
        macro->pattern = pattern;
        macro->bcl = fklCopyByteCodelnt(bcl);
        macro->origin_exp = origin_exp;
    } else {
        FklCodegenMacro *next = *pmacro;
        *pmacro = fklCreateCodegenMacro(pattern,
                origin_exp,
                bcl,
                next,
                prototype_id);
    }
}

BC_PROCESS(_export_macro_bc_process) {
    FklCodegenMacroScope *target_macro_scope = cms->prev;
    for (; codegen && !codegen->is_lib; codegen = codegen->prev)
        ;
    for (FklCodegenMacro *cur = cms->head; cur; cur = cur->next) {
        add_compiler_macro(&codegen->export_macro,
                fklMakeNastNodeRef(cur->pattern),
                fklMakeNastNodeRef(cur->origin_exp),
                cur->bcl,
                cur->prototype_id);
        add_compiler_macro(&target_macro_scope->head,
                fklMakeNastNodeRef(cur->pattern),
                fklMakeNastNodeRef(cur->origin_exp),
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

static inline void add_export_symbol(FklCodegenInfo *libCodegen,
        FklNastNode *origExp,
        FklNastNode *rest,
        FklNastNodeQueue *exportQueue) {
    FklNastPair *prevPair = origExp->pair;
    FklNastNode *exportHead = origExp->pair->car;
    for (; rest->type == FKL_NAST_PAIR; rest = rest->pair->cdr) {
        FklNastNode *restExp = fklNastCons(fklMakeNastNodeRef(exportHead),
                rest,
                rest->curline);
        fklNastNodeQueuePush2(exportQueue, restExp);
        prevPair->cdr = fklCreateNastNode(FKL_NAST_NIL, rest->curline);
        prevPair = rest->pair;
    }
}

static inline void push_single_bcl_codegen_quest(FklByteCodelnt *bcl,
        FklCodegenQuestVector *codegenQuestStack,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        FklCodegenEnv *curEnv,
        FklCodegenQuest *prev,
        FklCodegenInfo *codegen,
        uint64_t curline) {
    FklByteCodelntVector *stack = fklByteCodelntVectorCreate(1);
    fklByteCodelntVectorPushBack2(stack, bcl);
    FklCodegenQuest *quest = fklCreateCodegenQuest(_default_bc_process,
            createDefaultStackContext(stack),
            NULL,
            scope,
            macroScope,
            curEnv,
            curline,
            prev,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, quest);
}

typedef FklNastNode *(*ReplacementFunc)(const FklNastNode *orig,
        const FklCodegenEnv *env,
        const FklCodegenInfo *codegen);

static inline ReplacementFunc findBuiltInReplacementWithId(FklSid_t id,
        const FklSid_t builtin_replacement_id[]);

static inline int is_replacement_define(const FklNastNode *value,
        const FklCodegenInfo *info,
        const FklCodegenOuterCtx *ctx,
        const FklCodegenMacroScope *macroScope,
        const FklCodegenEnv *curEnv) {
    FklNastNode *replacement = NULL;
    ReplacementFunc f = NULL;
    for (const FklCodegenMacroScope *cs = macroScope; cs && !replacement;
            cs = cs->prev)
        replacement = fklGetReplacement(value->sym, cs->replacements);
    if (replacement) {
        return 1;
    } else if ((f = findBuiltInReplacementWithId(value->sym,
                        ctx->builtin_replacement_id)))
        return 1;
    else
        return 0;
}

static inline int is_replacement_true(const FklNastNode *value,
        const FklCodegenInfo *info,
        const FklCodegenOuterCtx *ctx,
        const FklCodegenMacroScope *macroScope,
        const FklCodegenEnv *curEnv) {
    FklNastNode *replacement = NULL;
    ReplacementFunc f = NULL;
    for (const FklCodegenMacroScope *cs = macroScope; cs && !replacement;
            cs = cs->prev)
        replacement = fklGetReplacement(value->sym, cs->replacements);
    if (replacement) {
        int r = replacement->type != FKL_NAST_NIL;
        return r;
    } else if ((f = findBuiltInReplacementWithId(value->sym,
                        ctx->builtin_replacement_id))) {
        FklNastNode *t = f(value, curEnv, info);
        int r = t->type != FKL_NAST_NIL;
        fklDestroyNastNode(t);
        return r;
    } else
        return 0;
}

static inline FklNastNode *get_replacement(const FklNastNode *value,
        const FklCodegenInfo *info,
        const FklCodegenOuterCtx *ctx,
        const FklCodegenMacroScope *macroScope,
        const FklCodegenEnv *curEnv) {
    FklNastNode *replacement = NULL;
    ReplacementFunc f = NULL;
    for (const FklCodegenMacroScope *cs = macroScope; cs && !replacement;
            cs = cs->prev)
        replacement = fklGetReplacement(value->sym, cs->replacements);
    if (replacement)
        return fklMakeNastNodeRef(replacement);
    else if ((f = findBuiltInReplacementWithId(value->sym,
                      ctx->builtin_replacement_id)))
        return f(value, curEnv, info);
    else
        return NULL;
}

static inline char *combineFileNameFromListAndCheckPrivate(
        const FklNastNode *list,
        const FklSymbolTable *pst);

static inline int is_valid_compile_check_pattern(const FklNastNode *p) {
    return p->type == FKL_NAST_PAIR && p->type != FKL_NAST_NIL
        && p->pair->car->type == FKL_NAST_SYM
        && p->pair->cdr->type == FKL_NAST_PAIR
        && p->pair->cdr->pair->cdr->type == FKL_NAST_NIL;
}

static inline int is_compile_check_pattern_slot_node(FklSid_t slot_id,
        const FklNastNode *n) {
    return n->type == FKL_NAST_PAIR && n->type != FKL_NAST_NIL
        && n->pair->car->type == FKL_NAST_SYM && n->pair->car->sym == slot_id
        && n->pair->cdr->type == FKL_NAST_PAIR
        && n->pair->cdr->pair->cdr->type == FKL_NAST_NIL;
}

static inline int is_compile_check_pattern_matched(const FklNastNode *p,
        const FklNastNode *e) {
    FklNastImmPairVector s;
    fklNastImmPairVectorInit(&s, 8);
    fklNastImmPairVectorPushBack2(&s,
            (FklNastImmPair){ .car = cadr_nast_node(p), .cdr = e });
    FklSid_t slot_id = p->pair->car->sym;
    int r = 1;
    while (!fklNastImmPairVectorIsEmpty(&s)) {
        const FklNastImmPair *top = fklNastImmPairVectorPopBackNonNull(&s);
        const FklNastNode *p = top->car;
        const FklNastNode *e = top->cdr;
        if (is_compile_check_pattern_slot_node(slot_id, p))
            continue;
        else if (p->type == e->type) {
            switch (p->type) {
            case FKL_NAST_BOX:
                fklNastImmPairVectorPushBack2(&s,
                        (FklNastImmPair){ .car = p->box, .cdr = e->box });
                break;
            case FKL_NAST_PAIR:
                fklNastImmPairVectorPushBack2(&s,
                        (FklNastImmPair){ .car = p->pair->car,
                            .cdr = e->pair->car });
                fklNastImmPairVectorPushBack2(&s,
                        (FklNastImmPair){ .car = p->pair->cdr,
                            .cdr = e->pair->cdr });
                break;
            case FKL_NAST_VECTOR:
                if (p->vec->size != e->vec->size) {
                    r = 0;
                    goto exit;
                }
                for (size_t i = 0; i < p->vec->size; i++) {
                    fklNastImmPairVectorPushBack2(&s,
                            (FklNastImmPair){ .car = p->vec->base[i],
                                .cdr = e->vec->base[i] });
                }
                break;
            case FKL_NAST_HASHTABLE:
                if (p->hash->num != e->hash->num) {
                    r = 0;
                    goto exit;
                }
                for (size_t i = 0; i < p->hash->num; i++) {
                    fklNastImmPairVectorPushBack2(&s,
                            (FklNastImmPair){ .car = p->hash->items[i].car,
                                .cdr = e->hash->items[i].car });
                    fklNastImmPairVectorPushBack2(&s,
                            (FklNastImmPair){ .car = p->hash->items[i].cdr,
                                .cdr = e->hash->items[i].cdr });
                }
                break;
            default:
                if (!fklNastNodeEqual(p, e)) {
                    r = 0;
                    goto exit;
                }
            }
        } else {
            r = 0;
            goto exit;
        }
    }

exit:
    fklNastImmPairVectorUninit(&s);
    return r;
}

typedef struct CfgCtx {
    const FklNastNode *(*method)(struct CfgCtx *ctx, int r);
    const FklNastNode *rest;
    int r;
} CfgCtx;

// CgCfgCtxVector
#define FKL_VECTOR_TYPE_PREFIX Cg
#define FKL_VECTOR_METHOD_PREFIX cg
#define FKL_VECTOR_ELM_TYPE CfgCtx
#define FKL_VECTOR_ELM_TYPE_NAME CfgCtx
#include <fakeLisp/cont/vector.h>

static inline int cfg_check_defined(const FklNastNode *exp,
        const FklPmatchHashMap *ht,
        const FklCodegenOuterCtx *outer_ctx,
        const FklCodegenEnv *curEnv,
        uint32_t scope,
        FklCodegenErrorState *errorState) {
    const FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    if (value->type != FKL_NAST_SYM) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place =
                fklMakeNastNodeRef(FKL_REMOVE_CONST(FklNastNode, exp));
        return 0;
    }
    return fklFindSymbolDefByIdAndScope(value->sym, scope, curEnv) != NULL;
}

static inline int cfg_check_importable(const FklNastNode *exp,
        const FklPmatchHashMap *ht,
        const FklCodegenOuterCtx *outer_ctx,
        const FklCodegenEnv *curEnv,
        uint32_t scope,
        FklCodegenErrorState *errorState) {
    const FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    if (value->type == FKL_NAST_NIL
            || !fklIsNastNodeListAndHasSameType(value, FKL_NAST_SYM)) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place =
                fklMakeNastNodeRef(FKL_REMOVE_CONST(FklNastNode, exp));
        return 0;
    }
    char *filename = combineFileNameFromListAndCheckPrivate(value,
            &outer_ctx->public_symbol_table);

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

static inline int cfg_check_macro_defined(const FklNastNode *exp,
        const FklPmatchHashMap *ht,
        const FklCodegenOuterCtx *outer_ctx,
        const FklCodegenEnv *curEnv,
        uint32_t scope,
        const FklCodegenMacroScope *macroScope,
        const FklCodegenInfo *info,
        FklCodegenErrorState *errorState) {
    const FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    if (value->type == FKL_NAST_SYM)
        return is_replacement_define(value,
                info,
                outer_ctx,
                macroScope,
                curEnv);
    else if (value->type == FKL_NAST_PAIR
             && value->pair->cdr->type == FKL_NAST_NIL
             && value->pair->car->type == FKL_NAST_SYM) {
        int r = 0;
        FklSid_t id = value->pair->car->sym;
        for (const FklCodegenMacroScope *macros = macroScope; macros;
                macros = macros->prev) {
            for (const FklCodegenMacro *cur = macros->head; cur;
                    cur = cur->next) {
                if (id == cur->pattern->pair->car->sym) {
                    r = 1;
                    break;
                }
            }
            if (r == 1)
                break;
        }
        return r;
    } else if (value->type == FKL_NAST_VECTOR && value->vec->size == 1
               && value->vec->base[0]->type == FKL_NAST_SYM) {
        FklSid_t id = value->vec->base[0]->sym;
        return *(info->g) != NULL
            && fklGraProdGroupHashMapGet2(info->named_prod_groups, id) != NULL;
    } else {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place =
                fklMakeNastNodeRef(FKL_REMOVE_CONST(FklNastNode, exp));
        return 0;
    }
}

static inline int cfg_check_eq(const FklNastNode *exp,
        const FklPmatchHashMap *ht,
        const FklCodegenOuterCtx *outer_ctx,
        const FklCodegenEnv *curEnv,
        const FklCodegenMacroScope *macroScope,
        const FklCodegenInfo *info,
        FklCodegenErrorState *errorState) {
    const FklNastNode *arg0 =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_arg0);
    const FklNastNode *arg1 =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_arg1);
    if (arg0->type != FKL_NAST_SYM) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place =
                fklMakeNastNodeRef(FKL_REMOVE_CONST(FklNastNode, exp));
        return 0;
    }
    FklNastNode *node =
            get_replacement(arg0, info, outer_ctx, macroScope, curEnv);
    if (node) {
        int r = fklNastNodeEqual(node, arg1);
        fklDestroyNastNode(node);
        return r;
    } else
        return 0;
}

static inline int cfg_check_matched(const FklNastNode *exp,
        const FklPmatchHashMap *ht,
        const FklCodegenOuterCtx *outer_ctx,
        const FklCodegenEnv *curEnv,
        const FklCodegenMacroScope *macroScope,
        const FklCodegenInfo *info,
        FklCodegenErrorState *errorState) {
    const FklNastNode *arg0 =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_arg0);
    const FklNastNode *arg1 =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_arg1);
    if (arg0->type != FKL_NAST_SYM || !is_valid_compile_check_pattern(arg1)) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place =
                fklMakeNastNodeRef(FKL_REMOVE_CONST(FklNastNode, exp));
        return 0;
    }
    FklNastNode *node =
            get_replacement(arg0, info, outer_ctx, macroScope, curEnv);
    if (node) {
        int r = is_compile_check_pattern_matched(arg1, node);
        fklDestroyNastNode(node);
        return r;
    } else
        return 0;
}

static const FklNastNode *cfg_check_not_method(CfgCtx *ctx, int r) {
    ctx->r = !r;
    return NULL;
}

static const FklNastNode *cfg_check_and_method(CfgCtx *ctx, int r) {
    ctx->r &= r;
    if (!ctx->r)
        return NULL;
    else if (ctx->rest->type == FKL_NAST_NIL)
        return NULL;
    else {
        const FklNastNode *r = ctx->rest->pair->car;
        ctx->rest = ctx->rest->pair->cdr;
        return r;
    }
}

static const FklNastNode *cfg_check_or_method(CfgCtx *ctx, int r) {
    ctx->r |= r;
    if (ctx->r)
        return NULL;
    else if (ctx->rest->type == FKL_NAST_NIL)
        return NULL;
    else {
        const FklNastNode *r = ctx->rest->pair->car;
        ctx->rest = ctx->rest->pair->cdr;
        return r;
    }
}

static inline int is_check_subpattern_true(const FklNastNode *exp,
        const FklCodegenInfo *info,
        const FklCodegenOuterCtx *outer_ctx,
        uint32_t scope,
        const FklCodegenMacroScope *macroScope,
        const FklCodegenEnv *curEnv,
        FklCodegenErrorState *errorState) {
    int r = 0;
    FklPmatchHashMap ht;
    CgCfgCtxVector s;
    CfgCtx *ctx;

    switch (exp->type) {
    case FKL_NAST_NIL:
        r = 0;
        break;
    case FKL_NAST_SYM:
        r = is_replacement_true(exp, info, outer_ctx, macroScope, curEnv);
        break;
    case FKL_NAST_FIX:
    case FKL_NAST_CHR:
    case FKL_NAST_F64:
    case FKL_NAST_STR:
    case FKL_NAST_BYTEVECTOR:
    case FKL_NAST_BIGINT:
    case FKL_NAST_BOX:
    case FKL_NAST_VECTOR:
    case FKL_NAST_HASHTABLE:
        r = 1;
        break;
    case FKL_NAST_RC_SYM:
    case FKL_NAST_SLOT:
        FKL_UNREACHABLE();
        break;
    case FKL_NAST_PAIR:
        goto check_nested_sub_pattern;
        break;
    }
    return r;

check_nested_sub_pattern:
    fklPmatchHashMapInit(&ht);
    cgCfgCtxVectorInit(&s, 8);

    for (;;) {
    loop_start:
        switch (exp->type) {
        case FKL_NAST_NIL:
            r = 0;
            break;
        case FKL_NAST_SYM:
            r = is_replacement_true(exp, info, outer_ctx, macroScope, curEnv);
            break;
        case FKL_NAST_FIX:
        case FKL_NAST_CHR:
        case FKL_NAST_F64:
        case FKL_NAST_STR:
        case FKL_NAST_BYTEVECTOR:
        case FKL_NAST_BIGINT:
        case FKL_NAST_BOX:
        case FKL_NAST_VECTOR:
        case FKL_NAST_HASHTABLE:
            r = 1;
            break;
        case FKL_NAST_RC_SYM:
        case FKL_NAST_SLOT:
            FKL_UNREACHABLE();
            break;
        case FKL_NAST_PAIR:
            if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                        [FKL_CODEGEN_SUB_PATTERN_DEFINE],
                        exp,
                        &ht)) {
                r = cfg_check_defined(exp,
                        &ht,
                        outer_ctx,
                        curEnv,
                        scope,
                        errorState);
                if (errorState->type)
                    goto exit;
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_IMPORT],
                               exp,
                               &ht)) {
                r = cfg_check_importable(exp,
                        &ht,
                        outer_ctx,
                        curEnv,
                        scope,
                        errorState);
                if (errorState->type)
                    goto exit;
            } else if (fklPatternMatch(
                               outer_ctx->builtin_sub_pattern_node
                                       [FKL_CODEGEN_SUB_PATTERN_DEFMACRO],
                               exp,
                               &ht)) {
                r = cfg_check_macro_defined(exp,
                        &ht,
                        outer_ctx,
                        curEnv,
                        scope,
                        macroScope,
                        info,
                        errorState);
                if (errorState->type)
                    goto exit;
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_EQ],
                               exp,
                               &ht)) {
                r = cfg_check_eq(exp,
                        &ht,
                        outer_ctx,
                        curEnv,
                        macroScope,
                        info,
                        errorState);
                if (errorState->type)
                    goto exit;
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_MATCH],
                               exp,
                               &ht)) {
                r = cfg_check_matched(exp,
                        &ht,
                        outer_ctx,
                        curEnv,
                        macroScope,
                        info,
                        errorState);
                if (errorState->type)
                    goto exit;
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_NOT],
                               exp,
                               &ht)) {
                ctx = cgCfgCtxVectorPushBack(&s, NULL);
                ctx->r = 0;
                ctx->rest = NULL;
                ctx->method = cfg_check_not_method;
                exp = *fklPmatchHashMapGet2(&ht,
                        outer_ctx->builtInPatternVar_value);
                continue;
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_AND],
                               exp,
                               &ht)) {
                const FklNastNode *rest = *fklPmatchHashMapGet2(&ht,
                        outer_ctx->builtInPatternVar_rest);
                if (rest->type == FKL_NAST_NIL)
                    r = 1;
                else if (fklIsNastNodeList(rest)) {
                    ctx = cgCfgCtxVectorPushBack(&s, NULL);
                    ctx->r = 1;
                    ctx->method = cfg_check_and_method;
                    exp = rest->pair->car;
                    ctx->rest = rest->pair->cdr;
                    continue;
                } else {
                    errorState->type = FKL_ERR_SYNTAXERROR;
                    errorState->place = fklMakeNastNodeRef(
                            FKL_REMOVE_CONST(FklNastNode, exp));
                    goto exit;
                }
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_OR],
                               exp,
                               &ht)) {
                const FklNastNode *rest = *fklPmatchHashMapGet2(&ht,
                        outer_ctx->builtInPatternVar_rest);
                if (rest->type == FKL_NAST_NIL)
                    r = 0;
                else if (fklIsNastNodeList(rest)) {
                    ctx = cgCfgCtxVectorPushBack(&s, NULL);
                    ctx->r = 0;
                    ctx->method = cfg_check_or_method;
                    exp = rest->pair->car;
                    ctx->rest = rest->pair->cdr;
                    continue;
                } else {
                    errorState->type = FKL_ERR_SYNTAXERROR;
                    errorState->place = fklMakeNastNodeRef(
                            FKL_REMOVE_CONST(FklNastNode, exp));
                    goto exit;
                }
            } else {
                errorState->type = FKL_ERR_SYNTAXERROR;
                errorState->place =
                        fklMakeNastNodeRef(FKL_REMOVE_CONST(FklNastNode, exp));
                goto exit;
            }
            break;
        }

        while (!cgCfgCtxVectorIsEmpty(&s)) {
            ctx = cgCfgCtxVectorBack(&s);
            exp = ctx->method(ctx, r);
            if (exp)
                goto loop_start;
            else {
                r = ctx->r;
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
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    int r = is_check_subpattern_true(value,
            codegen,
            outer_ctx,
            scope,
            macroScope,
            curEnv,
            errorState);
    if (errorState->type)
        return;
    FklByteCodelnt *bcl = NULL;
    bcl = fklCreateSingleInsBclnt(
            create_op_ins(r ? FKL_OP_PUSH_1 : FKL_OP_PUSH_NIL),
            codegen->fid,
            origExp->curline,
            scope);
    push_single_bcl_codegen_quest(bcl,
            codegenQuestStack,
            scope,
            macroScope,
            curEnv,
            NULL,
            codegen,
            origExp->curline);
}

static CODEGEN_FUNC(codegen_cond_compile) {
    FklNastNode *cond =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_cond);
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    if (fklNastListLength(rest) % 2 == 1) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    int r = is_check_subpattern_true(cond,
            codegen,
            outer_ctx,
            scope,
            macroScope,
            curEnv,
            errorState);
    if (errorState->type)
        return;
    if (r) {
        FklNastNodeQueue *q = fklNastNodeQueueCreate();
        fklNastNodeQueuePush2(q, fklMakeNastNodeRef(value));
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process,
                createDefaultStackContext(fklByteCodelntVectorCreate(1)),
                must_has_retval ? createMustHasRetvalQueueNextExpression(q)
                                : createDefaultQueueNextExpression(q),
                scope,
                macroScope,
                curEnv,
                value->curline,
                codegen,
                codegenQuestStack);
    } else {
        while (rest->type == FKL_NAST_PAIR) {
            FklNastNode *cond = rest->pair->car;
            rest = rest->pair->cdr;
            FklNastNode *value = rest->pair->car;
            rest = rest->pair->cdr;
            int r = is_check_subpattern_true(cond,
                    codegen,
                    outer_ctx,
                    scope,
                    macroScope,
                    curEnv,
                    errorState);
            if (errorState->type)
                return;
            if (r) {
                FklNastNodeQueue *q = fklNastNodeQueueCreate();
                fklNastNodeQueuePush2(q, fklMakeNastNodeRef(value));
                FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process,
                        createDefaultStackContext(
                                fklByteCodelntVectorCreate(1)),
                        must_has_retval
                                ? createMustHasRetvalQueueNextExpression(q)
                                : createDefaultQueueNextExpression(q),
                        scope,
                        macroScope,
                        curEnv,
                        value->curline,
                        codegen,
                        codegenQuestStack);
                return;
            }
        }
        if (must_has_retval) {
            errorState->type = FKL_ERR_EXP_HAS_NO_VALUE;
            errorState->place = fklMakeNastNodeRef(origExp);
            return;
        }
    }
}

static CODEGEN_FUNC(codegen_quote) {
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    push_default_codegen_quest(value,
            codegenQuestStack,
            scope,
            macroScope,
            curEnv,
            NULL,
            codegen);
}

static inline void unquoteHelperFunc(FklNastNode *value,
        FklCodegenQuestVector *codegenQuestStack,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        FklCodegenEnv *curEnv,
        FklByteCodeProcesser func,
        FklCodegenQuest *prev,
        FklCodegenInfo *codegen) {
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(value));
    FklCodegenQuest *quest = fklCreateCodegenQuest(func,
            createDefaultStackContext(fklByteCodelntVectorCreate(1)),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macroScope,
            curEnv,
            value->curline,
            prev,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, quest);
}

static CODEGEN_FUNC(codegen_unquote) {
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    unquoteHelperFunc(value,
            codegenQuestStack,
            scope,
            macroScope,
            curEnv,
            _default_bc_process,
            NULL,
            codegen);
}

BC_PROCESS(_qsquote_box_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    FklByteCodelnt *retval = *fklByteCodelntVectorPopBackNonNull(stack);
    FklInstruction pushBox = { .op = FKL_OP_PUSH_BOX, .ai = 1 };
    fklByteCodeLntPushBackIns(retval, &pushBox, fid, line, scope);
    return retval;
}

BC_PROCESS(_qsquote_vec_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    FklByteCodelnt *retval = stack->base[0];
    for (size_t i = 1; i < stack->size; i++) {
        FklByteCodelnt *cur = stack->base[i];
        fklCodeLntConcat(retval, cur);
        fklDestroyByteCodelnt(cur);
    }
    stack->size = 0;
    FklInstruction pushVec = create_op_ins(FKL_OP_PUSH_VEC_0);
    FklInstruction setBp = create_op_ins(FKL_OP_SET_BP);
    fklByteCodeLntInsertFrontIns(&setBp, retval, fid, line, scope);
    fklByteCodeLntPushBackIns(retval, &pushVec, fid, line, scope);
    return retval;
}

BC_PROCESS(_unqtesp_vec_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    FklByteCodelnt *retval = *fklByteCodelntVectorPopBackNonNull(stack);
    FklByteCodelnt *other = fklByteCodelntVectorIsEmpty(stack)
                                  ? NULL
                                  : *fklByteCodelntVectorPopBackNonNull(stack);
    if (other) {
        while (!fklByteCodelntVectorIsEmpty(stack)) {
            FklByteCodelnt *cur = *fklByteCodelntVectorPopBackNonNull(stack);
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
    FklByteCodelntVector *stack = GET_STACK(context);
    FklByteCodelnt *retval = stack->base[0];
    for (size_t i = 1; i < stack->size; i++) {
        FklByteCodelnt *cur = stack->base[i];
        fklCodeLntConcat(retval, cur);
        fklDestroyByteCodelnt(cur);
    }
    stack->size = 0;
    FklInstruction pushPair = create_op_ins(FKL_OP_PUSH_LIST_0);
    FklInstruction setBp = create_op_ins(FKL_OP_SET_BP);
    fklByteCodeLntInsertFrontIns(&setBp, retval, fid, line, scope);
    fklByteCodeLntPushBackIns(retval, &pushPair, fid, line, scope);
    return retval;
}

BC_PROCESS(_qsquote_list_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    FklByteCodelnt *retval = stack->base[0];
    for (size_t i = 1; i < stack->size; i++) {
        FklByteCodelnt *cur = stack->base[i];
        fklCodeLntConcat(retval, cur);
        fklDestroyByteCodelnt(cur);
    }
    stack->size = 0;
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
    FklNastNode *node;
    FklCodegenQuest *prev;
} QsquoteHelperStruct;

static inline void init_qsquote_helper_struct(QsquoteHelperStruct *r,
        QsquoteHelperEnum e,
        FklNastNode *node,
        FklCodegenQuest *prev) {
    r->e = e;
    r->node = node;
    r->prev = prev;
}

// CgQsquoteHelperVector
#define FKL_VECTOR_TYPE_PREFIX Cg
#define FKL_VECTOR_METHOD_PREFIX cg
#define FKL_VECTOR_ELM_TYPE QsquoteHelperStruct
#define FKL_VECTOR_ELM_TYPE_NAME QsquoteHelper
#include <fakeLisp/cont/vector.h>

static CODEGEN_FUNC(codegen_qsquote) {
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    CgQsquoteHelperVector valueStack;
    cgQsquoteHelperVectorInit(&valueStack, 8);
    init_qsquote_helper_struct(cgQsquoteHelperVectorPushBack(&valueStack, NULL),
            QSQUOTE_NONE,
            value,
            NULL);
    FklNastNode *const *builtInSubPattern = outer_ctx->builtin_sub_pattern_node;
    while (!cgQsquoteHelperVectorIsEmpty(&valueStack)) {
        const QsquoteHelperStruct *curNode =
                cgQsquoteHelperVectorPopBackNonNull(&valueStack);
        QsquoteHelperEnum e = curNode->e;
        FklCodegenQuest *prevQuest = curNode->prev;
        FklNastNode *curValue = curNode->node;
        switch (e) {
        case QSQUOTE_UNQTESP_CAR:
            unquoteHelperFunc(curValue,
                    codegenQuestStack,
                    scope,
                    macroScope,
                    curEnv,
                    _default_bc_process,
                    prevQuest,
                    codegen);
            break;
        case QSQUOTE_UNQTESP_VEC:
            unquoteHelperFunc(curValue,
                    codegenQuestStack,
                    scope,
                    macroScope,
                    curEnv,
                    _unqtesp_vec_bc_process,
                    prevQuest,
                    codegen);
            break;
        case QSQUOTE_NONE: {
            FklPmatchHashMap unquoteHt;
            fklPmatchHashMapInit(&unquoteHt);
            if (fklPatternMatch(
                        builtInSubPattern[FKL_CODEGEN_SUB_PATTERN_UNQUOTE],
                        curValue,
                        &unquoteHt)) {
                FklNastNode *unquoteValue = *fklPmatchHashMapGet2(&unquoteHt,
                        outer_ctx->builtInPatternVar_value);
                unquoteHelperFunc(unquoteValue,
                        codegenQuestStack,
                        scope,
                        macroScope,
                        curEnv,
                        _default_bc_process,
                        prevQuest,
                        codegen);
            } else if (curValue->type == FKL_NAST_PAIR) {
                FklCodegenQuest *curQuest =
                        fklCreateCodegenQuest(_qsquote_pair_bc_process,
                                createDefaultStackContext(
                                        fklByteCodelntVectorCreate(2)),
                                NULL,
                                scope,
                                macroScope,
                                curEnv,
                                curValue->curline,
                                prevQuest,
                                codegen);
                fklCodegenQuestVectorPushBack2(codegenQuestStack, curQuest);
                FklNastNode *node = curValue;
                for (;;) {
                    if (fklPatternMatch(
                                builtInSubPattern
                                        [FKL_CODEGEN_SUB_PATTERN_UNQUOTE],
                                node,
                                &unquoteHt)) {
                        FklNastNode *unquoteValue =
                                *fklPmatchHashMapGet2(&unquoteHt,
                                        outer_ctx->builtInPatternVar_value);
                        unquoteHelperFunc(unquoteValue,
                                codegenQuestStack,
                                scope,
                                macroScope,
                                curEnv,
                                _default_bc_process,
                                curQuest,
                                codegen);
                        break;
                    }
                    FklNastNode *cur = node->pair->car;
                    if (fklPatternMatch(
                                builtInSubPattern
                                        [FKL_CODEGEN_SUB_PATTERN_UNQTESP],
                                cur,
                                &unquoteHt)) {
                        FklNastNode *unqtespValue =
                                *fklPmatchHashMapGet2(&unquoteHt,
                                        outer_ctx->builtInPatternVar_value);
                        if (node->pair->cdr->type != FKL_NAST_NIL) {
                            FklCodegenQuest *appendQuest =
                                    fklCreateCodegenQuest(
                                            _qsquote_list_bc_process,
                                            createDefaultStackContext(
                                                    fklByteCodelntVectorCreate(
                                                            2)),
                                            NULL,
                                            scope,
                                            macroScope,
                                            curEnv,
                                            curValue->curline,
                                            curQuest,
                                            codegen);
                            fklCodegenQuestVectorPushBack2(codegenQuestStack,
                                    appendQuest);
                            init_qsquote_helper_struct(
                                    cgQsquoteHelperVectorPushBack(&valueStack,
                                            NULL),
                                    QSQUOTE_UNQTESP_CAR,
                                    unqtespValue,
                                    appendQuest);
                            init_qsquote_helper_struct(
                                    cgQsquoteHelperVectorPushBack(&valueStack,
                                            NULL),
                                    QSQUOTE_NONE,
                                    node->pair->cdr,
                                    appendQuest);
                        } else
                            init_qsquote_helper_struct(
                                    cgQsquoteHelperVectorPushBack(&valueStack,
                                            NULL),
                                    QSQUOTE_UNQTESP_CAR,
                                    unqtespValue,
                                    curQuest);
                        break;
                    } else
                        init_qsquote_helper_struct(
                                cgQsquoteHelperVectorPushBack(&valueStack,
                                        NULL),
                                QSQUOTE_NONE,
                                cur,
                                curQuest);
                    node = node->pair->cdr;
                    if (node->type != FKL_NAST_PAIR) {
                        init_qsquote_helper_struct(
                                cgQsquoteHelperVectorPushBack(&valueStack,
                                        NULL),
                                QSQUOTE_NONE,
                                node,
                                curQuest);
                        break;
                    }
                }
            } else if (curValue->type == FKL_NAST_VECTOR) {
                size_t vecSize = curValue->vec->size;
                FklCodegenQuest *curQuest =
                        fklCreateCodegenQuest(_qsquote_vec_bc_process,
                                createDefaultStackContext(
                                        fklByteCodelntVectorCreate(vecSize)),
                                NULL,
                                scope,
                                macroScope,
                                curEnv,
                                curValue->curline,
                                prevQuest,
                                codegen);
                fklCodegenQuestVectorPushBack2(codegenQuestStack, curQuest);
                for (size_t i = 0; i < vecSize; i++) {
                    if (fklPatternMatch(
                                builtInSubPattern
                                        [FKL_CODEGEN_SUB_PATTERN_UNQTESP],
                                curValue->vec->base[i],
                                &unquoteHt))
                        init_qsquote_helper_struct(
                                cgQsquoteHelperVectorPushBack(&valueStack,
                                        NULL),
                                QSQUOTE_UNQTESP_VEC,
                                *fklPmatchHashMapGet2(&unquoteHt,
                                        outer_ctx->builtInPatternVar_value),
                                curQuest);
                    else
                        init_qsquote_helper_struct(
                                cgQsquoteHelperVectorPushBack(&valueStack,
                                        NULL),
                                QSQUOTE_NONE,
                                curValue->vec->base[i],
                                curQuest);
                }
            } else if (curValue->type == FKL_NAST_BOX) {
                FklCodegenQuest *curQuest =
                        fklCreateCodegenQuest(_qsquote_box_bc_process,
                                createDefaultStackContext(
                                        fklByteCodelntVectorCreate(1)),
                                NULL,
                                scope,
                                macroScope,
                                curEnv,
                                curValue->curline,
                                prevQuest,
                                codegen);
                fklCodegenQuestVectorPushBack2(codegenQuestStack, curQuest);
                init_qsquote_helper_struct(
                        cgQsquoteHelperVectorPushBack(&valueStack, NULL),
                        QSQUOTE_NONE,
                        curValue->box,
                        curQuest);
            } else
                push_default_codegen_quest(curValue,
                        codegenQuestStack,
                        scope,
                        macroScope,
                        curEnv,
                        prevQuest,
                        codegen);
            fklPmatchHashMapUninit(&unquoteHt);
        } break;
        }
    }
    cgQsquoteHelperVectorUninit(&valueStack);
}

BC_PROCESS(_cond_exp_bc_process_0) {
    FklByteCodelnt *retval = NULL;
    FklByteCodelntVector *stack = GET_STACK(context);
    if (stack->size)
        retval = *fklByteCodelntVectorPopBackNonNull(stack);
    else
        retval = fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
    return retval;
}

static inline int is_const_true_bytecode_lnt(const FklByteCodelnt *bcl) {
    const FklInstruction *cur = bcl->bc->code;
    const FklInstruction *const end = &cur[bcl->bc->len];
    for (; cur < end; cur++) {
        switch (cur->op) {
        case FKL_OP_SET_BP:
        case FKL_OP_PUSH_PAIR:
        case FKL_OP_PUSH_I24:
        case FKL_OP_PUSH_I64F:
        case FKL_OP_PUSH_I64F_C:
        case FKL_OP_PUSH_I64F_X:
        case FKL_OP_PUSH_CHAR:
        case FKL_OP_PUSH_F64:
        case FKL_OP_PUSH_F64_C:
        case FKL_OP_PUSH_F64_X:
        case FKL_OP_PUSH_STR:
        case FKL_OP_PUSH_STR_C:
        case FKL_OP_PUSH_STR_X:
        case FKL_OP_PUSH_SYM:
        case FKL_OP_PUSH_SYM_C:
        case FKL_OP_PUSH_SYM_X:
        case FKL_OP_PUSH_LIST_0:
        case FKL_OP_PUSH_LIST:
        case FKL_OP_PUSH_VEC_0:
        case FKL_OP_PUSH_VEC:
        case FKL_OP_PUSH_BI:
        case FKL_OP_PUSH_BI_C:
        case FKL_OP_PUSH_BI_X:
        case FKL_OP_PUSH_BOX:
        case FKL_OP_PUSH_BVEC:
        case FKL_OP_PUSH_BVEC_X:
        case FKL_OP_PUSH_BVEC_C:
        case FKL_OP_PUSH_HASHEQ_0:
        case FKL_OP_PUSH_HASHEQ:
        case FKL_OP_PUSH_HASHEQV_0:
        case FKL_OP_PUSH_HASHEQV:
        case FKL_OP_PUSH_HASHEQUAL_0:
        case FKL_OP_PUSH_HASHEQUAL:
        case FKL_OP_PUSH_0:
        case FKL_OP_PUSH_1:
        case FKL_OP_PUSH_I8:
        case FKL_OP_PUSH_I16:
        case FKL_OP_PUSH_I64B:
        case FKL_OP_PUSH_I64B_C:
        case FKL_OP_PUSH_I64B_X:
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
    FklByteCodelntVector *stack = GET_STACK(context);
    FklByteCodelnt *retval = NULL;
    if (stack->size >= 2) {
        FklByteCodelnt *prev = stack->base[0];
        FklByteCodelnt *first = stack->base[1];
        retval = fklCreateByteCodelnt(fklCreateByteCode(0));

        FklInstruction drop = create_op_ins(FKL_OP_DROP);

        fklByteCodeLntInsertFrontIns(&drop, prev, fid, line, scope);

        uint64_t prev_len = prev->bc->len;

        int true_bcl = is_const_true_bytecode_lnt(first);

        if (stack->size > 2) {
            FklByteCodelnt *cur = stack->base[2];
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }

        for (size_t i = 3; i < stack->size; i++) {
            FklByteCodelnt *cur = stack->base[i];
            fklByteCodeLntPushBackIns(retval, &drop, fid, line, scope);
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }

        size_t retval_len = retval->bc->len;

        check_and_close_ref(retval, scope, env, codegen->pts, fid, line);

        append_jmp_ins(INS_APPEND_BACK,
                retval,
                prev_len,
                JMP_UNCOND,
                JMP_FORWARD,
                fid,
                line,
                scope);

        if (retval_len) {
            if (!true_bcl) {
                fklByteCodeLntInsertFrontIns(&drop, retval, fid, line, scope);
                append_jmp_ins(INS_APPEND_FRONT,
                        retval,
                        retval->bc->len,
                        JMP_IF_FALSE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
                fklCodeLntReverseConcat(first, retval);
            }
        } else
            fklCodeLntReverseConcat(first, retval);

        fklCodeLntConcat(retval, prev);
        fklDestroyByteCodelnt(prev);
        fklDestroyByteCodelnt(first);
    } else
        retval = *fklByteCodelntVectorPopBackNonNull(stack);
    stack->size = 0;

    return retval;
}

BC_PROCESS(_cond_exp_bc_process_2) {
    FklByteCodelntVector *stack = GET_STACK(context);
    FklInstruction drop = create_op_ins(FKL_OP_DROP);

    FklByteCodelnt *retval = NULL;
    if (stack->size) {
        retval = fklCreateByteCodelnt(fklCreateByteCode(0));
        FklByteCodelnt *first = stack->base[0];
        int true_bcl = is_const_true_bytecode_lnt(first);
        if (stack->size > 1) {
            FklByteCodelnt *cur = stack->base[1];
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }
        for (size_t i = 2; i < stack->size; i++) {
            FklByteCodelnt *cur = stack->base[i];
            fklByteCodeLntPushBackIns(retval, &drop, fid, line, scope);
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }

        if (retval->bc->len) {
            if (!true_bcl) {
                fklByteCodeLntInsertFrontIns(&drop, retval, fid, line, scope);
                append_jmp_ins(INS_APPEND_FRONT,
                        retval,
                        retval->bc->len,
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
    stack->size = 0;
    return retval;
}

static CODEGEN_FUNC(codegen_cond) {
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklCodegenQuest *quest = fklCreateCodegenQuest(_cond_exp_bc_process_0,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            NULL,
            scope,
            macroScope,
            curEnv,
            rest->curline,
            NULL,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, quest);
    if (rest->type != FKL_NAST_NIL) {
        FklNastNodeVector tmpStack;
        fklNastNodeVectorInit(&tmpStack, 32);
        pushListItemToStack(rest, &tmpStack, NULL);
        FklNastNode *lastExp = *fklNastNodeVectorPopBackNonNull(&tmpStack);
        FklCodegenQuest *prevQuest = quest;
        for (size_t i = 0; i < tmpStack.size; i++) {
            FklNastNode *curExp = tmpStack.base[i];
            if (curExp->type != FKL_NAST_PAIR) {
                errorState->type = FKL_ERR_SYNTAXERROR;
                errorState->place = fklMakeNastNodeRef(origExp);
                fklNastNodeVectorUninit(&tmpStack);
                return;
            }
            FklNastNode *last = NULL;
            FklNastNodeQueue *curQueue = fklNastNodeQueueCreate();
            pushListItemToQueue(curExp, curQueue, &last);
            if (last->type != FKL_NAST_NIL) {
                errorState->type = FKL_ERR_SYNTAXERROR;
                errorState->place = fklMakeNastNodeRef(origExp);
                while (!fklNastNodeQueueIsEmpty(curQueue))
                    fklDestroyNastNode(*fklNastNodeQueuePop(curQueue));
                fklNastNodeQueueDestroy(curQueue);
                fklNastNodeVectorUninit(&tmpStack);
                return;
            }
            uint32_t curScope = enter_new_scope(scope, curEnv);
            FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
            FklCodegenQuest *curQuest = fklCreateCodegenQuest(
                    _cond_exp_bc_process_1,
                    createDefaultStackContext(fklByteCodelntVectorCreate(32)),
                    createFirstHasRetvalQueueNextExpression(curQueue),
                    curScope,
                    cms,
                    curEnv,
                    curExp->curline,
                    prevQuest,
                    codegen);
            fklCodegenQuestVectorPushBack2(codegenQuestStack, curQuest);
            prevQuest = curQuest;
        }
        FklNastNode *last = NULL;
        if (lastExp->type != FKL_NAST_PAIR) {
            errorState->type = FKL_ERR_SYNTAXERROR;
            errorState->place = fklMakeNastNodeRef(origExp);
            fklNastNodeVectorUninit(&tmpStack);
            return;
        }
        FklNastNodeQueue *lastQueue = fklNastNodeQueueCreate();
        pushListItemToQueue(lastExp, lastQueue, &last);
        if (last->type != FKL_NAST_NIL) {
            errorState->type = FKL_ERR_SYNTAXERROR;
            errorState->place = fklMakeNastNodeRef(origExp);
            while (!fklNastNodeQueueIsEmpty(lastQueue))
                fklDestroyNastNode(*fklNastNodeQueuePop(lastQueue));
            fklNastNodeQueueDestroy(lastQueue);
            fklNastNodeVectorUninit(&tmpStack);
            return;
        }
        uint32_t curScope = enter_new_scope(scope, curEnv);
        FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
        fklCodegenQuestVectorPushBack2(codegenQuestStack,
                fklCreateCodegenQuest(_cond_exp_bc_process_2,
                        createDefaultStackContext(
                                fklByteCodelntVectorCreate(32)),
                        createFirstHasRetvalQueueNextExpression(lastQueue),
                        curScope,
                        cms,
                        curEnv,
                        lastExp->curline,
                        prevQuest,
                        codegen));
        fklNastNodeVectorUninit(&tmpStack);
    }
}

BC_PROCESS(_if_exp_bc_process_0) {
    FklByteCodelntVector *stack = GET_STACK(context);
    FklInstruction drop = create_op_ins(FKL_OP_DROP);

    if (stack->size >= 2) {
        FklByteCodelnt *exp = *fklByteCodelntVectorPopBackNonNull(stack);
        FklByteCodelnt *cond = *fklByteCodelntVectorPopBackNonNull(stack);
        fklByteCodeLntInsertFrontIns(&drop, exp, fid, line, scope);
        append_jmp_ins(INS_APPEND_BACK,
                cond,
                exp->bc->len,
                JMP_IF_FALSE,
                JMP_FORWARD,
                fid,
                line,
                scope);
        fklCodeLntConcat(cond, exp);
        fklDestroyByteCodelnt(exp);
        return cond;
    } else if (stack->size >= 1)
        return *fklByteCodelntVectorPopBackNonNull(stack);
    else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static CODEGEN_FUNC(codegen_if0) {
    FklNastNode *cond =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    FklNastNode *exp =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);

    FklNastNodeQueue *nextQueue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(nextQueue, fklMakeNastNodeRef(cond));
    fklNastNodeQueuePush2(nextQueue, fklMakeNastNodeRef(exp));

    uint32_t curScope = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
    fklCodegenQuestVectorPushBack2(codegenQuestStack,
            fklCreateCodegenQuest(_if_exp_bc_process_0,
                    createDefaultStackContext(fklByteCodelntVectorCreate(2)),
                    createMustHasRetvalQueueNextExpression(nextQueue),
                    curScope,
                    cms,
                    curEnv,
                    origExp->curline,
                    NULL,
                    codegen));
}

BC_PROCESS(_if_exp_bc_process_1) {
    FklByteCodelntVector *stack = GET_STACK(context);

    FklInstruction drop = create_op_ins(FKL_OP_DROP);

    if (stack->size >= 3) {
        FklByteCodelnt *exp0 = *fklByteCodelntVectorPopBackNonNull(stack);
        FklByteCodelnt *cond = *fklByteCodelntVectorPopBackNonNull(stack);
        FklByteCodelnt *exp1 = *fklByteCodelntVectorPopBackNonNull(stack);
        fklByteCodeLntInsertFrontIns(&drop, exp0, fid, line, scope);
        fklByteCodeLntInsertFrontIns(&drop, exp1, fid, line, scope);
        append_jmp_ins(INS_APPEND_BACK,
                exp0,
                exp1->bc->len,
                JMP_UNCOND,
                JMP_FORWARD,
                fid,
                line,
                scope);
        append_jmp_ins(INS_APPEND_BACK,
                cond,
                exp0->bc->len,
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
    } else if (stack->size >= 2) {
        FklByteCodelnt *exp0 = *fklByteCodelntVectorPopBackNonNull(stack);
        FklByteCodelnt *cond = *fklByteCodelntVectorPopBackNonNull(stack);
        fklByteCodeLntInsertFrontIns(&drop, exp0, fid, line, scope);
        append_jmp_ins(INS_APPEND_BACK,
                cond,
                exp0->bc->len,
                JMP_IF_FALSE,
                JMP_FORWARD,
                fid,
                line,
                scope);
        fklCodeLntConcat(cond, exp0);
        fklDestroyByteCodelnt(exp0);
        return cond;
    } else if (stack->size >= 1)
        return *fklByteCodelntVectorPopBackNonNull(stack);
    else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static CODEGEN_FUNC(codegen_if1) {
    FklNastNode *cond =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    FklNastNode *exp0 =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNode *exp1 =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    FklNastNodeQueue *exp0Queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(exp0Queue, fklMakeNastNodeRef(cond));
    fklNastNodeQueuePush2(exp0Queue, fklMakeNastNodeRef(exp0));

    FklNastNodeQueue *exp1Queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(exp1Queue, fklMakeNastNodeRef(exp1));

    uint32_t curScope = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
    FklCodegenQuest *prev = fklCreateCodegenQuest(_if_exp_bc_process_1,
            createDefaultStackContext(fklByteCodelntVectorCreate(2)),
            createMustHasRetvalQueueNextExpression(exp0Queue),
            curScope,
            cms,
            curEnv,
            origExp->curline,
            NULL,
            codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, prev);

    curScope = enter_new_scope(scope, curEnv);
    cms = fklCreateCodegenMacroScope(macroScope);
    fklCodegenQuestVectorPushBack2(codegenQuestStack,
            fklCreateCodegenQuest(_default_bc_process,
                    createDefaultStackContext(fklByteCodelntVectorCreate(2)),
                    createMustHasRetvalQueueNextExpression(exp1Queue),
                    curScope,
                    cms,
                    curEnv,
                    origExp->curline,
                    prev,
                    codegen));
}

BC_PROCESS(_when_exp_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    if (stack->size) {
        FklByteCodelnt *cond = stack->base[0];
        FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));

        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        for (size_t i = 1; i < stack->size; i++) {
            FklByteCodelnt *cur = stack->base[i];
            fklByteCodeLntPushBackIns(retval, &drop, fid, line, scope);
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }
        stack->size = 0;
        if (retval->bc->len)
            append_jmp_ins(INS_APPEND_FRONT,
                    retval,
                    retval->bc->len,
                    JMP_IF_FALSE,
                    JMP_FORWARD,
                    fid,
                    line,
                    scope);
        fklCodeLntReverseConcat(cond, retval);
        fklDestroyByteCodelnt(cond);
        check_and_close_ref(retval, scope, env, codegen->pts, fid, line);
        return retval;
    } else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

BC_PROCESS(_unless_exp_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    if (stack->size) {
        FklByteCodelnt *cond = stack->base[0];
        FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));

        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        for (size_t i = 1; i < stack->size; i++) {
            FklByteCodelnt *cur = stack->base[i];
            fklByteCodeLntPushBackIns(retval, &drop, fid, line, scope);
            fklCodeLntConcat(retval, cur);
            fklDestroyByteCodelnt(cur);
        }
        stack->size = 0;
        if (retval->bc->len)
            append_jmp_ins(INS_APPEND_FRONT,
                    retval,
                    retval->bc->len,
                    JMP_IF_TRUE,
                    JMP_FORWARD,
                    fid,
                    line,
                    scope);
        fklCodeLntReverseConcat(cond, retval);
        fklDestroyByteCodelnt(cond);
        check_and_close_ref(retval, scope, env, codegen->pts, fid, line);
        return retval;
    } else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static inline void codegen_when_unless(FklPmatchHashMap *ht,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        FklCodegenEnv *curEnv,
        FklCodegenInfo *codegen,
        FklCodegenQuestVector *codegenQuestStack,
        FklByteCodeProcesser func,
        FklCodegenErrorState *errorState,
        FklNastNode *origExp) {
    FklNastNode *cond = *fklPmatchHashMapGet2(ht,
            codegen->outer_ctx->builtInPatternVar_value);

    FklNastNode *rest = *fklPmatchHashMapGet2(ht,
            codegen->outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(cond));
    pushListItemToQueue(rest, queue, NULL);

    uint32_t curScope = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(func,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            createFirstHasRetvalQueueNextExpression(queue),
            curScope,
            cms,
            curEnv,
            rest->curline,
            codegen,
            codegenQuestStack);
}

static CODEGEN_FUNC(codegen_when) {
    codegen_when_unless(ht,
            scope,
            macroScope,
            curEnv,
            codegen,
            codegenQuestStack,
            _when_exp_bc_process,
            errorState,
            origExp);
}

static CODEGEN_FUNC(codegen_unless) {
    codegen_when_unless(ht,
            scope,
            macroScope,
            curEnv,
            codegen,
            codegenQuestStack,
            _unless_exp_bc_process,
            errorState,
            origExp);
}

static FklCodegenInfo *createCodegenInfo(FklCodegenInfo *prev,
        const char *filename,
        int libMark,
        int macroMark,
        FklCodegenOuterCtx *outer_ctx) {
    FklCodegenInfo *r = (FklCodegenInfo *)fklZmalloc(sizeof(FklCodegenInfo));
    FKL_ASSERT(r);
    fklInitCodegenInfo(r,
            filename,
            prev,
            prev->runtime_symbol_table,
            prev->runtime_kt,
            1,
            libMark,
            macroMark,
            outer_ctx);
    return r;
}

typedef struct {
    FILE *fp;
    FklCodegenInfo *codegen;
    FklAnalysisSymbolVector symbolStack;
    FklParseStateVector stateStack;
    FklUintVector lineStack;
} CodegenLoadContext;

#include <fakeLisp/grammer.h>

static CodegenLoadContext *createCodegenLoadContext(FILE *fp,
        FklCodegenInfo *codegen) {
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
    while (!fklAnalysisSymbolVectorIsEmpty(symbolStack))
        fklDestroyNastNode(
                fklAnalysisSymbolVectorPopBackNonNull(symbolStack)->ast);
    fklAnalysisSymbolVectorUninit(symbolStack);
    fklParseStateVectorUninit(&context->stateStack);
    fklUintVectorUninit(&context->lineStack);
    fclose(context->fp);
    fklZfree(context);
}

static inline FklNastNode *getExpressionFromFile(FklCodegenInfo *codegen,
        FILE *fp,
        int *unexpectEOF,
        size_t *errorLine,
        FklCodegenErrorState *errorState,
        FklAnalysisSymbolVector *symbolStack,
        FklUintVector *lineStack,
        FklParseStateVector *stateStack) {
    FklSymbolTable *pst = &codegen->outer_ctx->public_symbol_table;
    size_t size = 0;
    FklNastNode *begin = NULL;
    char *list = NULL;
    stateStack->size = 0;
    FklGrammer *g = *codegen->g;
    if (g) {
        codegen->outer_ctx->cur_file_dir = codegen->dir;
        fklParseStateVectorPushBack2(stateStack,
                (FklParseState){ .state = &g->aTable.states[0] });
        list = fklReadWithAnalysisTable(g,
                fp,
                &size,
                codegen->curline,
                &codegen->curline,
                pst,
                unexpectEOF,
                errorLine,
                &begin,
                symbolStack,
                lineStack,
                stateStack);
    } else {
        fklNastPushState0ToStack(stateStack);
        list = fklReadWithBuiltinParser(fp,
                &size,
                codegen->curline,
                &codegen->curline,
                pst,
                unexpectEOF,
                errorLine,
                &begin,
                symbolStack,
                lineStack,
                stateStack);
    }
    if (list)
        fklZfree(list);
    if (*unexpectEOF)
        begin = NULL;
    while (!fklAnalysisSymbolVectorIsEmpty(symbolStack))
        fklDestroyNastNode(
                fklAnalysisSymbolVectorPopBackNonNull(symbolStack)->ast);
    lineStack->size = 0;
    return begin;
}

#include <fakeLisp/grammer.h>

static FklNastNode *_codegen_load_get_next_expression(void *pcontext,
        FklCodegenErrorState *errorState) {
    CodegenLoadContext *context = pcontext;
    FklCodegenInfo *codegen = context->codegen;
    FklParseStateVector *stateStack = &context->stateStack;
    FklAnalysisSymbolVector *symbolStack = &context->symbolStack;
    FklUintVector *lineStack = &context->lineStack;
    FILE *fp = context->fp;
    int unexpectEOF = 0;
    size_t errorLine = 0;
    FklNastNode *begin = getExpressionFromFile(codegen,
            fp,
            &unexpectEOF,
            &errorLine,
            errorState,
            symbolStack,
            lineStack,
            stateStack);
    if (unexpectEOF) {
        errorState->place = NULL;
        errorState->line = errorLine;
        errorState->type = unexpectEOF == FKL_PARSE_TERMINAL_MATCH_FAILED
                                 ? FKL_ERR_UNEXPECTED_EOF
                                 : FKL_ERR_INVALIDEXPR;
        return NULL;
    }
    return begin;
}

static int hasLoadSameFile(const char *rpath, const FklCodegenInfo *codegen) {
    for (; codegen; codegen = codegen->prev)
        if (codegen->realpath && !strcmp(rpath, codegen->realpath))
            return 1;
    return 0;
}

static const FklNextExpressionMethodTable
        _codegen_load_get_next_expression_method_table = {
            .getNextExpression = _codegen_load_get_next_expression,
            .finalizer = _codegen_load_finalizer,
        };

static FklCodegenNextExpression *createFpNextExpression(FILE *fp,
        FklCodegenInfo *codegen) {
    CodegenLoadContext *context = createCodegenLoadContext(fp, codegen);
    return createCodegenNextExpression(
            &_codegen_load_get_next_expression_method_table,
            context,
            0);
}

static inline void init_load_codegen_grammer_ptr(FklCodegenInfo *next,
        FklCodegenInfo *prev) {
    next->g = &prev->self_g;
    next->named_prod_groups = &prev->self_named_prod_groups;

    next->unnamed_g = &prev->self_unnamed_g;
}

static CODEGEN_FUNC(codegen_load) {
    FklNastNode *filename =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    if (filename->type != FKL_NAST_STR) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }

    if (rest->type != FKL_NAST_NIL) {
        FklNastNodeQueue *queue = fklNastNodeQueueCreate();

        FklNastPair *prevPair = origExp->pair->cdr->pair;

        FklNastNode *loadHead = origExp->pair->car;

        for (; rest->type == FKL_NAST_PAIR; rest = rest->pair->cdr) {
            FklNastNode *restExp = fklNastCons(fklMakeNastNodeRef(loadHead),
                    rest,
                    rest->curline);

            prevPair->cdr = fklCreateNastNode(FKL_NAST_NIL, rest->curline);

            fklNastNodeQueuePush2(queue, restExp);

            prevPair = rest->pair;
        }
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_begin_exp_bc_process,
                createDefaultStackContext(fklByteCodelntVectorCreate(2)),
                createDefaultQueueNextExpression(queue),
                scope,
                macroScope,
                curEnv,
                origExp->curline,
                codegen,
                codegenQuestStack);
    }

    const FklString *filenameStr = filename->str;
    if (!fklIsAccessibleRegFile(filenameStr->str)) {
        errorState->type = FKL_ERR_FILEFAILURE;
        errorState->place = fklMakeNastNodeRef(filename);
        return;
    }
    FklCodegenInfo *nextCodegen = createCodegenInfo(codegen,
            filenameStr->str,
            0,
            0,
            codegen->outer_ctx);
    if (hasLoadSameFile(nextCodegen->realpath, codegen)) {
        errorState->type = FKL_ERR_CIRCULARLOAD;
        errorState->place = fklMakeNastNodeRef(filename);
        nextCodegen->refcount = 1;
        fklDestroyCodegenInfo(nextCodegen);
        return;
    }
    init_load_codegen_grammer_ptr(nextCodegen, codegen);
    FILE *fp = fopen(filenameStr->str, "r");
    if (!fp) {
        errorState->type = FKL_ERR_FILEFAILURE;
        errorState->place = fklMakeNastNodeRef(filename);
        nextCodegen->refcount = 1;
        fklDestroyCodegenInfo(nextCodegen);
        return;
    }
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_begin_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            createFpNextExpression(fp, nextCodegen),
            scope,
            macroScope,
            curEnv,
            origExp->curline,
            nextCodegen,
            codegenQuestStack);
}

static inline char *combineFileNameFromListAndCheckPrivate(
        const FklNastNode *list,
        const FklSymbolTable *pst) {
    char *r = NULL;
    for (const FklNastNode *curPair = list; curPair->type == FKL_NAST_PAIR;
            curPair = curPair->pair->cdr) {
        FklNastNode *cur = curPair->pair->car;
        const FklString *str = fklGetSymbolWithId(cur->sym, pst);
        if (curPair != list && str->str[0] == '_') {
            fklZfree(r);
            return NULL;
        }
        r = fklCstrStringCat(r, str);
        if (curPair->pair->cdr->type != FKL_NAST_NIL)
            r = fklStrCat(r, FKL_PATH_SEPARATOR_STR);
    }
    return r;
}

static void add_symbol_to_local_env_in_array(FklCodegenEnv *env,
        const FklCgExportSidIdxHashMap *exports,
        FklByteCodelnt *bcl,
        FklSid_t fid,
        uint32_t line,
        uint32_t scope,
        FklSymbolTable *pst) {
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

static void add_symbol_with_prefix_to_local_env_in_array(FklCodegenEnv *env,
        const FklString *prefix,
        const FklCgExportSidIdxHashMap *exports,
        FklByteCodelnt *bcl,
        FklSid_t fid,
        uint32_t line,
        uint32_t scope,
        FklSymbolTable *pst) {
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        const FklString *origSymbol = fklGetSymbolWithId(l->k, pst);
        fklStringBufferConcatWithString(&buf, prefix);
        fklStringBufferConcatWithString(&buf, origSymbol);

        FklSid_t sym = fklAddSymbolCharBuf(buf.buf, buf.index, pst)->v;
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

static FklNastNode *createPatternWithPrefixFromOrig(FklNastNode *orig,
        const FklString *prefix,
        FklSymbolTable *pst) {
    const FklString *head = fklGetSymbolWithId(orig->pair->car->sym, pst);
    FklString *symbolWithPrefix = fklStringAppend(prefix, head);
    FklNastNode *patternWithPrefix =
            fklNastConsWithSym(fklAddSymbol(symbolWithPrefix, pst)->v,
                    fklMakeNastNodeRef(orig->pair->cdr),
                    orig->curline,
                    orig->pair->car->curline);
    fklZfree(symbolWithPrefix);
    return patternWithPrefix;
}

static FklNastNode *add_prefix_for_pattern_origin_exp(FklNastNode *orig,
        const FklString *prefix,
        FklSymbolTable *pst) {
    FklNastNode *retval = fklCopyNastNode(orig);
    FklNastNode *head_node = caadr_nast_node(retval);
    const FklString *head = fklGetSymbolWithId(head_node->sym, pst);
    FklString *symbolWithPrefix = fklStringAppend(prefix, head);
    FklSid_t id = fklAddSymbol(symbolWithPrefix, pst)->v;
    head_node->sym = id;
    fklZfree(symbolWithPrefix);
    return retval;
}

static FklNastNode *replace_pattern_origin_exp_head(FklNastNode *orig,
        FklSid_t head) {
    FklNastNode *retval = fklCopyNastNode(orig);
    FklNastNode *head_node = caadr_nast_node(retval);
    head_node->sym = head;
    return retval;
}

static inline void export_replacement_with_prefix(
        FklReplacementHashMap *replacements,
        FklCodegenMacroScope *macros,
        const FklString *prefix,
        FklSymbolTable *pst) {
    for (FklReplacementHashMapNode *cur = replacements->first; cur;
            cur = cur->next) {
        const FklString *origSymbol = fklGetSymbolWithId(cur->k, pst);
        FklString *symbolWithPrefix = fklStringAppend(prefix, origSymbol);
        FklSid_t id = fklAddSymbol(symbolWithPrefix, pst)->v;
        fklReplacementHashMapAdd2(macros->replacements, id, cur->v);
        fklZfree(symbolWithPrefix);
    }
}

void fklPrintUndefinedRef(const FklCodegenEnv *env,
        FklSymbolTable *runtime_st,
        FklSymbolTable *pst) {
    const FklUnReSymbolRefVector *urefs = &env->uref;
    for (uint32_t i = urefs->size; i > 0; i--) {
        FklUnReSymbolRef *ref = &urefs->base[i - 1];
        fprintf(stderr, "warning: Symbol ");
        fklPrintRawSymbol(fklGetSymbolWithId(ref->id, pst), stderr);
        fprintf(stderr, " is undefined at line %" PRIu64, ref->line);
        if (ref->fid) {
            fputs(" of ", stderr);
            fklPrintString(fklGetSymbolWithId(ref->fid, runtime_st), stderr);
        }
        fputc('\n', stderr);
    }
}

typedef struct {
    FklCodegenInfo *codegen;
    FklCodegenEnv *env;
    FklCodegenMacroScope *cms;
    FklByteCodelntVector stack;
    FklSidHashSet named_prod_group_ids;
    FklNastNode *prefix;
    FklNastNode *only;
    FklNastNode *alias;
    FklNastNode *except;
    uint32_t scope;
} ExportContextData;

static void export_context_data_finalizer(void *data) {
    ExportContextData *d = (ExportContextData *)data;
    while (!fklByteCodelntVectorIsEmpty(&d->stack))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBackNonNull(&d->stack));
    fklByteCodelntVectorUninit(&d->stack);
    fklSidHashSetUninit(&d->named_prod_group_ids);
    fklDestroyCodegenEnv(d->env);
    fklDestroyCodegenInfo(d->codegen);
    fklDestroyCodegenMacroScope(d->cms);

    if (d->prefix)
        fklDestroyNastNode(d->prefix);
    if (d->only)
        fklDestroyNastNode(d->only);
    if (d->alias)
        fklDestroyNastNode(d->alias);
    if (d->except)
        fklDestroyNastNode(d->except);
    fklZfree(d);
}

static FklByteCodelntVector *export_context_data_get_bcl_stack(void *data) {
    return &((ExportContextData *)data)->stack;
}

static void export_context_data_put_bcl(void *data, FklByteCodelnt *bcl) {
    ExportContextData *d = (ExportContextData *)data;
    fklByteCodelntVectorPushBack2(&d->stack, bcl);
}

static const FklCodegenQuestContextMethodTable ExportContextMethodTable = {
    .__finalizer = export_context_data_finalizer,
    .__get_bcl_stack = export_context_data_get_bcl_stack,
    .__put_bcl = export_context_data_put_bcl,
};

static inline int merge_all_grammer(FklCodegenInfo *codegen) {
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
        FklCodegenInfo *codegen,
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
            FklNastNode *n = fklCreateNastNode(FKL_NAST_PAIR, line);
            n->pair = fklCreateNastPair();

            n->pair->car = fklCreateNastNode(FKL_NAST_SYM, line);
            n->pair->car->sym = nonterm.group;

            n->pair->cdr = fklCreateNastNode(FKL_NAST_SYM, line);
            n->pair->cdr->sym = nonterm.sid;

            error_state->place = n;
        } else {
            error_state->place = fklCreateNastNode(FKL_NAST_SYM, line);
            error_state->place->sym = nonterm.sid;
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

static inline void merge_group(FklGrammerProdGroupItem *group,
        const FklGrammerProdGroupItem *other,
        const FklRecomputeGroupIdArgs *args) {
    for (const FklStrHashSetNode *cur = other->delimiters.first; cur;
            cur = cur->next)
        fklAddString(&group->delimiters, cur->k);
    fklMergeGrammer(&group->g, &other->g, args);
}

static inline int import_reader_macro(FklCodegenInfo *codegen,
        FklCodegenErrorState *errorState,
        uint64_t curline,
        const FklCodegenLib *lib,
        FklGrammerProdGroupItem *group,
        FklSid_t origin_group_id,
        FklSid_t new_group_id) {

    FklSymbolTable *pst = &codegen->outer_ctx->public_symbol_table;

    FklGrammerProdGroupItem *target_group =
            add_production_group(codegen->named_prod_groups, pst, new_group_id);

    merge_group(target_group,
            group,
            &(FklRecomputeGroupIdArgs){ .old_group_id = origin_group_id,
                .new_group_id = new_group_id });
    return 0;
}

static inline FklGrammer *init_builtin_grammer_and_prod_group(
        FklCodegenInfo *codegen) {
    FklGrammer *g = *codegen->g;
    if (!g) {
        *codegen->g =
                fklCreateEmptyGrammer(&codegen->outer_ctx->public_symbol_table);
        fklGraProdGroupHashMapInit(codegen->named_prod_groups);
        fklInitEmptyGrammer(codegen->unnamed_g,
                &codegen->outer_ctx->public_symbol_table);
        g = *codegen->g;
        if (fklMergeGrammer(g, &codegen->outer_ctx->builtin_g, NULL))
            FKL_UNREACHABLE();
    }

    return g;
}

static inline FklByteCodelnt *process_import_imported_lib_common(uint32_t libId,
        FklCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklCodegenEnv *curEnv,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        uint64_t curline,
        FklCodegenErrorState *errorState,
        FklSymbolTable *pst) {
    for (FklCodegenMacro *cur = lib->head; cur; cur = cur->next)
        add_compiler_macro(&macroScope->head,
                fklMakeNastNodeRef(cur->pattern),
                fklMakeNastNodeRef(cur->origin_exp),
                cur->bcl,
                cur->prototype_id);

    for (FklReplacementHashMapNode *cur = lib->replacements->first; cur;
            cur = cur->next) {
        fklReplacementHashMapAdd2(macroScope->replacements, cur->k, cur->v);
    }

    if (lib->named_prod_groups.buckets) {
        for (FklGraProdGroupHashMapNode *prod_group_item =
                        lib->named_prod_groups.first;
                prod_group_item;
                prod_group_item = prod_group_item->next) {
            init_builtin_grammer_and_prod_group(codegen);

            if (import_reader_macro(codegen,
                        errorState,
                        curline,
                        lib,
                        &prod_group_item->v,
                        prod_group_item->k,
                        prod_group_item->k))
                break;
        }

        if (!errorState->type
                && add_all_group_to_grammer(curline, codegen, errorState)) {
            errorState->line = curline;
            errorState->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
            errorState->place = NULL;
            return NULL;
        }
    }

    FklByteCodelnt *load_lib = append_load_lib_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            curline,
            scope);

    add_symbol_to_local_env_in_array(curEnv,
            &lib->exports,
            load_lib,
            codegen->fid,
            curline,
            scope,
            pst);

    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib_prefix(uint32_t libId,
        FklCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklCodegenEnv *curEnv,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        uint64_t curline,
        FklNastNode *prefixNode,
        FklCodegenErrorState *errorState,
        FklSymbolTable *pst) {
    const FklString *prefix = fklGetSymbolWithId(prefixNode->sym, pst);
    for (FklCodegenMacro *cur = lib->head; cur; cur = cur->next)
        add_compiler_macro(&macroScope->head,
                createPatternWithPrefixFromOrig(cur->pattern, prefix, pst),
                add_prefix_for_pattern_origin_exp(cur->origin_exp, prefix, pst),
                cur->bcl,
                cur->prototype_id);

    export_replacement_with_prefix(lib->replacements, macroScope, prefix, pst);
    if (lib->named_prod_groups.buckets) {
        FklStringBuffer buffer;
        fklInitStringBuffer(&buffer);
        for (FklGraProdGroupHashMapNode *prod_group_item =
                        lib->named_prod_groups.first;
                prod_group_item;
                prod_group_item = prod_group_item->next) {
            fklStringBufferConcatWithString(&buffer, prefix);
            fklStringBufferConcatWithString(&buffer,
                    fklGetSymbolWithId(prod_group_item->k, pst));

            FklSid_t group_id_with_prefix =
                    fklAddSymbolCharBuf(buffer.buf, buffer.index, pst)->v;

            init_builtin_grammer_and_prod_group(codegen);
            if (import_reader_macro(codegen,
                        errorState,
                        curline,
                        lib,
                        &prod_group_item->v,
                        prod_group_item->k,
                        group_id_with_prefix))
                break;

            fklStringBufferClear(&buffer);
        }
        fklUninitStringBuffer(&buffer);
        if (!errorState->type
                && add_all_group_to_grammer(curline, codegen, errorState)) {
            errorState->line = curline;
            errorState->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
            errorState->place = NULL;
            return NULL;
        }
    }
    FklByteCodelnt *load_lib = append_load_lib_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            curline,
            scope);

    add_symbol_with_prefix_to_local_env_in_array(curEnv,
            prefix,
            &lib->exports,
            load_lib,
            codegen->fid,
            curline,
            scope,
            pst);

    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib_only(uint32_t libId,
        FklCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklCodegenEnv *curEnv,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        uint64_t curline,
        FklNastNode *only,
        FklCodegenErrorState *errorState) {
    FklByteCodelnt *load_lib = append_load_lib_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            curline,
            scope);

    const FklCgExportSidIdxHashMap *exports = &lib->exports;
    const FklReplacementHashMap *replace = lib->replacements;
    const FklCodegenMacro *head = lib->head;

    for (; only->type == FKL_NAST_PAIR; only = only->pair->cdr) {
        FklSid_t cur = only->pair->car->sym;
        int r = 0;
        for (const FklCodegenMacro *macro = head; macro; macro = macro->next) {
            FklNastNode *patternHead = macro->pattern->pair->car;
            if (patternHead->sym == cur) {
                r = 1;
                add_compiler_macro(&macroScope->head,
                        fklMakeNastNodeRef(macro->pattern),
                        fklMakeNastNodeRef(macro->origin_exp),
                        macro->bcl,
                        macro->prototype_id);
            }
        }
        FklNastNode *rep = fklGetReplacement(cur, replace);
        if (rep) {
            r = 1;
            fklReplacementHashMapAdd2(macroScope->replacements, cur, rep);
        }

        FklGrammerProdGroupItem *group = NULL;
        if (lib->named_prod_groups.buckets
                && (group = fklGraProdGroupHashMapGet2(&lib->named_prod_groups,
                            cur))) {
            r = 1;
            init_builtin_grammer_and_prod_group(codegen);
            if (import_reader_macro(codegen,
                        errorState,
                        curline,
                        lib,
                        group,
                        cur,
                        cur))
                break;
        }

        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);
        if (item) {
            uint32_t idx = fklAddCodegenDefBySid(cur, scope, curEnv)->idx;
            append_import_ins(INS_APPEND_BACK,
                    load_lib,
                    idx,
                    item->idx,
                    codegen->fid,
                    only->curline,
                    scope);
        } else if (!r) {
            errorState->type = FKL_ERR_IMPORT_MISSING;
            errorState->place = fklMakeNastNodeRef(only->pair->car);
            errorState->line = only->curline;
            errorState->fid = fklAddSymbolCstr(codegen->filename,
                    &codegen->outer_ctx->public_symbol_table)
                                      ->v;
            break;
        }
    }

    if (!errorState->type
            && add_all_group_to_grammer(curline, codegen, errorState)) {
        errorState->line = curline;
        errorState->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
        errorState->place = NULL;
    }
    return load_lib;
}

static inline int is_in_except_list(const FklNastNode *list, FklSid_t id) {
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr)
        if (list->pair->car->sym == id)
            return 1;
    return 0;
}

static inline FklByteCodelnt *process_import_imported_lib_except(uint32_t libId,
        FklCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklCodegenEnv *curEnv,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        uint64_t curline,
        FklNastNode *except,
        FklCodegenErrorState *errorState) {
    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    const FklReplacementHashMap *replace = lib->replacements;
    const FklCodegenMacro *head = lib->head;

    for (const FklCodegenMacro *macro = head; macro; macro = macro->next) {
        FklNastNode *patternHead = macro->pattern->pair->car;
        if (!is_in_except_list(except, patternHead->sym)) {
            add_compiler_macro(&macroScope->head,
                    fklMakeNastNodeRef(macro->pattern),
                    fklMakeNastNodeRef(macro->origin_exp),
                    macro->bcl,
                    macro->prototype_id);
        }
    }

    for (FklReplacementHashMapNode *reps = replace->first; reps;
            reps = reps->next) {
        if (!is_in_except_list(except, reps->k))
            fklReplacementHashMapAdd2(macroScope->replacements,
                    reps->k,
                    reps->v);
    }
    if (lib->named_prod_groups.buckets) {
        for (FklGraProdGroupHashMapNode *prod_group_item =
                        lib->named_prod_groups.first;
                prod_group_item;
                prod_group_item = prod_group_item->next) {
            if (!is_in_except_list(except, prod_group_item->k)) {
                init_builtin_grammer_and_prod_group(codegen);
                if (import_reader_macro(codegen,
                            errorState,
                            curline,
                            lib,
                            &prod_group_item->v,
                            prod_group_item->k,
                            prod_group_item->k))
                    break;
            }
        }
        if (!errorState->type
                && add_all_group_to_grammer(curline, codegen, errorState)) {
            errorState->line = curline;
            errorState->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
            errorState->place = NULL;
            return NULL;
        }
    }

    FklByteCodelnt *load_lib = append_load_lib_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            curline,
            scope);

    FklSidHashSet excepts;
    fklSidHashSetInit(&excepts);

    for (FklNastNode *list = except; list->type == FKL_NAST_PAIR;
            list = list->pair->cdr)
        fklSidHashSetPut2(&excepts, list->pair->car->sym);

    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        if (!fklSidHashSetHas2(&excepts, l->k)) {
            uint32_t idx = fklAddCodegenDefBySid(l->k, scope, curEnv)->idx;

            append_import_ins(INS_APPEND_BACK,
                    load_lib,
                    idx,
                    l->v.idx,
                    codegen->fid,
                    except->curline,
                    scope);
        }
    }
    fklSidHashSetUninit(&excepts);
    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib_alias(uint32_t libId,
        FklCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklCodegenEnv *curEnv,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        uint64_t curline,
        FklNastNode *alias,
        FklCodegenErrorState *errorState,
        FklSymbolTable *pst) {
    FklByteCodelnt *load_lib = append_load_lib_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            curline,
            scope);

    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    const FklReplacementHashMap *replace = lib->replacements;
    const FklCodegenMacro *head = lib->head;

    for (; alias->type == FKL_NAST_PAIR; alias = alias->pair->cdr) {
        FklNastNode *curNode = alias->pair->car;
        FklSid_t cur = curNode->pair->car->sym;
        FklSid_t cur0 = curNode->pair->cdr->pair->car->sym;
        int r = 0;
        for (const FklCodegenMacro *macro = head; macro; macro = macro->next) {
            FklNastNode *patternHead = macro->pattern->pair->car;
            if (patternHead->sym == cur) {
                r = 1;

                FklNastNode *orig = macro->pattern;
                FklNastNode *pattern = fklNastConsWithSym(cur0,
                        fklMakeNastNodeRef(orig->pair->cdr),
                        orig->curline,
                        orig->pair->car->curline);

                add_compiler_macro(&macroScope->head,
                        pattern,
                        replace_pattern_origin_exp_head(macro->origin_exp,
                                cur0),
                        macro->bcl,
                        macro->prototype_id);
            }
        }
        FklNastNode *rep = fklGetReplacement(cur, replace);
        if (rep) {
            r = 1;
            fklReplacementHashMapAdd2(macroScope->replacements, cur0, rep);
        }

        FklGrammerProdGroupItem *group = NULL;
        if (lib->named_prod_groups.buckets
                && (group = fklGraProdGroupHashMapGet2(&lib->named_prod_groups,
                            cur))) {
            r = 1;
            init_builtin_grammer_and_prod_group(codegen);
            if (import_reader_macro(codegen,
                        errorState,
                        curline,
                        lib,
                        group,
                        cur,
                        cur0))
                break;
        }

        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);
        if (item) {
            uint32_t idx = fklAddCodegenDefBySid(cur0, scope, curEnv)->idx;
            append_import_ins(INS_APPEND_BACK,
                    load_lib,
                    idx,
                    item->idx,
                    codegen->fid,
                    alias->curline,
                    scope);
        } else if (!r) {
            errorState->type = FKL_ERR_IMPORT_MISSING;
            errorState->place = fklMakeNastNodeRef(curNode->pair->car);
            errorState->line = alias->curline;
            errorState->fid = fklAddSymbolCstr(codegen->filename,
                    &codegen->outer_ctx->public_symbol_table)
                                      ->v;
            break;
        }
    }

    if (!errorState->type
            && add_all_group_to_grammer(curline, codegen, errorState)) {
        errorState->line = curline;
        errorState->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
        errorState->place = NULL;
    }
    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib(uint32_t libId,
        FklCodegenInfo *codegen,
        const FklCodegenLib *lib,
        FklCodegenEnv *env,
        uint32_t scope,
        FklCodegenMacroScope *cms,
        uint32_t line,
        FklNastNode *prefix,
        FklNastNode *only,
        FklNastNode *alias,
        FklNastNode *except,
        FklCodegenErrorState *errorState,
        FklSymbolTable *pst) {
    if (prefix)
        return process_import_imported_lib_prefix(libId,
                codegen,
                lib,
                env,
                scope,
                cms,
                line,
                prefix,
                errorState,
                pst);
    if (only)
        return process_import_imported_lib_only(libId,
                codegen,
                lib,
                env,
                scope,
                cms,
                line,
                only,
                errorState);
    if (alias)
        return process_import_imported_lib_alias(libId,
                codegen,
                lib,
                env,
                scope,
                cms,
                line,
                alias,
                errorState,
                pst);
    if (except)
        return process_import_imported_lib_except(libId,
                codegen,
                lib,
                env,
                scope,
                cms,
                line,
                except,
                errorState);
    return process_import_imported_lib_common(libId,
            codegen,
            lib,
            env,
            scope,
            cms,
            line,
            errorState,
            pst);
}

static inline int is_exporting_outer_ref_group(FklCodegenInfo *codegen) {
    for (FklSidHashSetNode *sid_list = codegen->export_named_prod_groups->first;
            sid_list;
            sid_list = sid_list->next) {
        FklSid_t id = sid_list->k;
        FklGrammerProdGroupItem *group =
                fklGraProdGroupHashMapGet2(codegen->named_prod_groups, id);
        if (group->is_ref_outer)
            return 1;
    }
    return 0;
}

static inline void process_export_bc(FklCodegenInfo *info,
        FklByteCodelnt *libBc,
        FklSid_t fid,
        uint32_t line,
        uint32_t scope) {
    info->epc = libBc->bc->len;

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
        errorState->type = FKL_ERR_EXPORT_OUTER_REF_PROD_GROUP;
        errorState->line = line;
        errorState->place = NULL;
        return NULL;
    }

    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    fklUpdatePrototype(codegen->pts, env, codegen->runtime_symbol_table, pst);
    fklPrintUndefinedRef(env, codegen->runtime_symbol_table, pst);

    ExportContextData *data = context->data;
    FklByteCodelnt *libBc =
            sequnce_exp_bc_process(&data->stack, fid, line, scope);

    FklInstruction ret = create_op_ins(FKL_OP_RET);

    fklByteCodeLntPushBackIns(libBc, &ret, fid, line, scope);

    fklPeepholeOptimize(libBc);

    process_export_bc(codegen, libBc, fid, line, scope);

    FklCodegenLib *lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
    fklInitCodegenScriptLib(lib, codegen, libBc, codegen->epc, env);

    codegen->realpath = NULL;

    codegen->export_macro = NULL;
    codegen->export_replacement = NULL;
    return process_import_imported_lib(codegen->libraries->size,
            data->codegen,
            lib,
            data->env,
            data->scope,
            data->cms,
            line,
            data->prefix,
            data->only,
            data->alias,
            data->except,
            errorState,
            pst);
}

static FklCodegenQuestContext *createExportContext(FklCodegenInfo *codegen,
        FklCodegenEnv *targetEnv,
        uint32_t scope,
        FklCodegenMacroScope *cms,
        FklNastNode *prefix,
        FklNastNode *only,
        FklNastNode *alias,
        FklNastNode *except) {
    ExportContextData *data =
            (ExportContextData *)fklZmalloc(sizeof(ExportContextData));
    FKL_ASSERT(data);

    data->codegen = codegen;
    codegen->refcount++;

    data->scope = scope;
    data->env = targetEnv;
    targetEnv->refcount++;

    cms->refcount++;
    data->cms = cms;

    data->prefix = prefix ? fklMakeNastNodeRef(prefix) : NULL;

    data->only = only ? fklMakeNastNodeRef(only) : NULL;
    data->except = except ? fklMakeNastNodeRef(except) : NULL;
    data->alias = alias ? fklMakeNastNodeRef(alias) : NULL;
    fklByteCodelntVectorInit(&data->stack, 16);

    fklSidHashSetInit(&data->named_prod_group_ids);
    if (codegen->named_prod_groups) {
        for (const FklGraProdGroupHashMapNode *cur =
                        codegen->named_prod_groups->first;
                cur;
                cur = cur->next) {
            fklSidHashSetPut2(&data->named_prod_group_ids, cur->k);
        }
    }
    return createCodegenQuestContext(data, &ExportContextMethodTable);
}

struct RecomputeImportSrcIdxCtx {
    FklSid_t id;
    FklCodegenEnv *env;
    uint64_t *id_base;
};

static int recompute_import_dst_idx_predicate(FklOpcode op) {
    return op >= FKL_OP_IMPORT && op <= FKL_OP_IMPORT_XX;
}

static int recompute_import_dst_idx_func(void *cctx,
        FklOpcode *op,
        FklOpcodeMode *pmode,
        FklInstructionArg *ins_arg) {
    struct RecomputeImportSrcIdxCtx *ctx = cctx;
    FklSid_t id = ctx->id_base[ins_arg->uy];
    if (is_constant_defined(id, 1, ctx->env)) {
        ctx->id = id;
        return 1;
    }
    ins_arg->uy = fklAddCodegenDefBySid(id, 1, ctx->env)->idx;
    *op = FKL_OP_IMPORT;
    *pmode = FKL_OP_MODE_IuAuB;
    return 0;
}

static inline FklSid_t recompute_import_src_idx(FklByteCodelnt *bcl,
        FklCodegenEnv *env,
        FklSidVector *idPstack) {
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
        FklSid_t fid,
        uint32_t line,
        uint32_t scope) {
    if (stack->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));
        size_t top = stack->size;
        for (size_t i = 0; i < top; i++) {
            FklByteCodelnt *cur = stack->base[i];
            if (cur->bc->len) {
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
    ExportContextData *d = context->data;
    FklByteCodelnt *bcl =
            export_sequnce_exp_bc_process(&d->stack, fid, line, scope);
    if (bcl == NULL)
        return NULL;
    FklCodegenEnv *targetEnv = d->env;

    FklSymDefHashMap *defs = &env->scopes[0].defs;

    FklSidVector idPstack;
    fklSidVectorInit(&idPstack, defs->count);

    for (FklSymDefHashMapNode *list = defs->first; list; list = list->next) {
        FklSid_t idp = list->k.id;
        fklSidVectorPushBack2(&idPstack, idp);
    }

    if (idPstack.size) {
        FklSid_t const_def_id =
                recompute_import_src_idx(bcl, targetEnv, &idPstack);
        if (const_def_id) {
            fklDestroyByteCodelnt(bcl);
            bcl = NULL;
            FklNastNode *place = fklCreateNastNode(FKL_NAST_SYM, line);
            place->sym = const_def_id;
            errorState->type = FKL_ERR_ASSIGN_CONSTANT;
            errorState->line = line;
            const FklString *sym =
                    fklGetSymbolWithId(fid, codegen->runtime_symbol_table);

            errorState->fid =
                    fid ? fklAddSymbol(sym, &outer_ctx->public_symbol_table)->v
                        : 0;
            errorState->place = place;
            goto exit;
        }

        FklCgExportSidIdxHashMap *exports = &codegen->exports;

        uint64_t *idPbase = idPstack.base;
        uint32_t top = idPstack.size;

        for (uint32_t i = 0; i < top; i++) {
            FklSid_t id = idPbase[i];
            FklCodegenExportIdx *item =
                    fklCgExportSidIdxHashMapGet2(exports, id);
            if (item == NULL) {
                uint32_t idx = exports->count;

                const FklSymDefHashMapElm *idx_elm =
                        fklGetCodegenDefByIdInScope(id, 1, targetEnv);

                fklCgExportSidIdxHashMapAdd(exports,
                        &id,
                        &(FklCodegenExportIdx){ .idx = idx,
                            .oidx = idx_elm->v.idx });
            }
        }
    }

    FklCodegenMacroScope *macros = targetEnv->macros;

    for (FklCodegenMacro *head = env->macros->head; head; head = head->next) {
        add_compiler_macro(&macros->head,
                fklMakeNastNodeRef(head->pattern),
                fklMakeNastNodeRef(head->origin_exp),
                head->bcl,
                head->prototype_id);

        add_compiler_macro(&codegen->export_macro,
                fklMakeNastNodeRef(head->pattern),
                fklMakeNastNodeRef(head->origin_exp),
                head->bcl,
                head->prototype_id);
    }

    for (FklReplacementHashMapNode *cur = env->macros->replacements->first; cur;
            cur = cur->next) {
        fklReplacementHashMapAdd2(codegen->export_replacement, cur->k, cur->v);
        fklReplacementHashMapAdd2(macros->replacements, cur->k, cur->v);
    }

    FklCodegenInfo *lib_codegen = d->codegen;
    if (lib_codegen->named_prod_groups->first) {
        for (const FklGraProdGroupHashMapNode *cur =
                        lib_codegen->named_prod_groups->first;
                cur;
                cur = cur->next) {
            if (!fklSidHashSetHas2(&d->named_prod_group_ids, cur->k)) {
                FKL_ASSERT(lib_codegen->export_named_prod_groups);
                fklSidHashSetPut2(lib_codegen->export_named_prod_groups,
                        cur->k);
            }
        }
    }
exit:
    fklSidVectorUninit(&idPstack);

    return bcl;
}

static inline FklCodegenInfo *get_lib_codegen(FklCodegenInfo *libCodegen) {
    for (; libCodegen && !libCodegen->is_lib; libCodegen = libCodegen->prev)
        if (libCodegen->is_macro)
            return NULL;
    return libCodegen;
}

typedef struct {
    FklByteCodelntVector *stack;
    FklNastNode *origExp;
    uint8_t must_has_retval;
} ExportSequnceContextData;

BC_PROCESS(exports_bc_process) {
    ExportSequnceContextData *data = context->data;
    FklByteCodelnt *bcl =
            export_sequnce_exp_bc_process(data->stack, fid, line, scope);
    if (data->must_has_retval && !bcl) {
        errorState->type = FKL_ERR_EXP_HAS_NO_VALUE;
        errorState->place = fklMakeNastNodeRef(data->origExp);
    }
    return bcl;
}

static CODEGEN_FUNC(codegen_export_none) {
    if (must_has_retval) {
        errorState->type = FKL_ERR_EXP_HAS_NO_VALUE;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    FklCodegenInfo *libCodegen = get_lib_codegen(codegen);

    if (libCodegen && scope == 1 && curEnv->prev == codegen->global_env
            && macroScope->prev == codegen->global_env->macros) {
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_empty_bc_process,
                createEmptyContext(),
                NULL,
                scope,
                macroScope,
                curEnv,
                origExp->curline,
                codegen,
                codegenQuestStack);
    } else {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
}

static void export_sequnce_context_data_finalizer(void *data) {
    ExportSequnceContextData *d = (ExportSequnceContextData *)data;
    while (!fklByteCodelntVectorIsEmpty(d->stack))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBackNonNull(d->stack));
    fklByteCodelntVectorDestroy(d->stack);
    fklDestroyNastNode(d->origExp);
    fklZfree(d);
}

static FklByteCodelntVector *export_sequnce_context_data_get_bcl_stack(
        void *data) {
    return ((ExportSequnceContextData *)data)->stack;
}

static void export_sequnce_context_data_put_bcl(void *data,
        FklByteCodelnt *bcl) {
    ExportSequnceContextData *d = (ExportSequnceContextData *)data;
    fklByteCodelntVectorPushBack2(d->stack, bcl);
}

static const FklCodegenQuestContextMethodTable
        ExportSequnceContextMethodTable = {
            .__finalizer = export_sequnce_context_data_finalizer,
            .__get_bcl_stack = export_sequnce_context_data_get_bcl_stack,
            .__put_bcl = export_sequnce_context_data_put_bcl,
        };

static FklCodegenQuestContext *create_export_sequnce_context(
        FklNastNode *origExp,
        FklByteCodelntVector *s,
        uint8_t must_has_retval) {
    ExportSequnceContextData *data = (ExportSequnceContextData *)fklZmalloc(
            sizeof(ExportSequnceContextData));
    FKL_ASSERT(data);

    data->stack = s;
    data->must_has_retval = must_has_retval;
    data->origExp = origExp;
    return createCodegenQuestContext(data, &ExportSequnceContextMethodTable);
}

static CODEGEN_FUNC(codegen_export) {
    FklCodegenInfo *libCodegen = get_lib_codegen(codegen);

    if (libCodegen && scope == 1 && curEnv->prev == codegen->global_env
            && macroScope->prev == codegen->global_env->macros) {
        FklNastNodeQueue *exportQueue = fklNastNodeQueueCreate();
        FklNastNode *orig_exp = fklCopyNastNode(origExp);
        FklNastNode *rest =
                *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
        add_export_symbol(libCodegen, origExp, rest, exportQueue);

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(exports_bc_process,
                create_export_sequnce_context(orig_exp,
                        fklByteCodelntVectorCreate(2),
                        must_has_retval),
                createDefaultQueueNextExpression(exportQueue),
                scope,
                macroScope,
                curEnv,
                origExp->curline,
                codegen,
                codegenQuestStack);
    } else {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
}

static inline FklSid_t get_reader_macro_group_id(const FklNastNode *node) {
    const FklNastNode *name = cadr_nast_node(node);
    if (name->type == FKL_NAST_BOX) {
        const FklNastNode *group_id_node = name->box;
        if (group_id_node->type == FKL_NAST_SYM)
            return group_id_node->sym;
    }
    return 0;
}

typedef struct {
    FklSid_t id;
    FklCodegenExportIdx *item;
    FklByteCodelntVector stack;
} ExportDefineContext;

static void _export_define_context_finalizer(void *data) {
    ExportDefineContext *ctx = data;
    FklByteCodelntVector *s = &ctx->stack;
    while (!fklByteCodelntVectorIsEmpty(s))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBackNonNull(s));
    fklByteCodelntVectorUninit(s);
    fklZfree(ctx);
}

static void _export_define_context_put_bcl(void *data, FklByteCodelnt *bcl) {
    ExportDefineContext *ctx = data;
    FklByteCodelntVector *s = &ctx->stack;
    fklByteCodelntVectorPushBack2(s, bcl);
}

static FklByteCodelntVector *_export_define_context_get_bcl_stack(void *data) {
    return &((ExportDefineContext *)data)->stack;
}

static const FklCodegenQuestContextMethodTable
        ExportDefineContextMethodTable = {
            .__finalizer = _export_define_context_finalizer,
            .__put_bcl = _export_define_context_put_bcl,
            .__get_bcl_stack = _export_define_context_get_bcl_stack,
        };

static inline FklCodegenQuestContext *create_export_define_context(FklSid_t id,
        FklCodegenExportIdx *item) {
    ExportDefineContext *ctx =
            (ExportDefineContext *)fklZmalloc(sizeof(ExportDefineContext));
    FKL_ASSERT(ctx);
    ctx->id = id;
    ctx->item = item;
    fklByteCodelntVectorInit(&ctx->stack, 2);
    return createCodegenQuestContext(ctx, &ExportDefineContextMethodTable);
}

BC_PROCESS(_export_define_bc_process) {
    ExportDefineContext *ctx = context->data;
    FklByteCodelntVector *stack = &ctx->stack;
    ctx->item->oidx = fklGetCodegenDefByIdInScope(ctx->id, 1, env)->v.idx;
    return sequnce_exp_bc_process(stack, fid, line, scope);
}

static CODEGEN_FUNC(codegen_export_single) {
    FklCodegenInfo *libCodegen = get_lib_codegen(codegen);
    if (!libCodegen || curEnv->prev != codegen->global_env || scope > 1
            || macroScope->prev != codegen->global_env->macros) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }

    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    FklNastNode *orig_value = fklMakeNastNodeRef(value);
    value = fklTryExpandCodegenMacro(orig_value,
            codegen,
            macroScope,
            errorState);
    if (errorState->type)
        return;

    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(queue, value);

    FklNastNode *const *builtin_pattern_node = outer_ctx->builtin_pattern_node;
    FklNastNode *name = NULL;
    if (!fklIsNastNodeList(value))
        goto error;
    if (isBeginExp(value, builtin_pattern_node)) {
        uint32_t refcount = value->pair->car->refcount;
        uint32_t line = value->pair->car->curline;
        *(value->pair->car) = *(origExp->pair->car);
        value->pair->car->refcount = refcount;
        value->pair->car->curline = line;
        FklNastNode *orig_exp = fklCopyNastNode(origExp);
        fklCodegenQuestVectorPushBack2(codegenQuestStack,
                fklCreateCodegenQuest(exports_bc_process,
                        create_export_sequnce_context(orig_exp,
                                fklByteCodelntVectorCreate(2),
                                must_has_retval),
                        createDefaultQueueNextExpression(queue),
                        1,
                        macroScope,
                        curEnv,
                        origExp->curline,
                        NULL,
                        codegen));
    } else if (isExportDefunExp(value, builtin_pattern_node)) {
        name = cadr_nast_node(value)->pair->car;
        goto process_def_in_lib;
    } else if (isExportDefineExp(value, builtin_pattern_node)) {
        name = cadr_nast_node(value);
    process_def_in_lib:
        if (name->type != FKL_NAST_SYM)
            goto error;
        FklCodegenExportIdx *item =
                fklCgExportSidIdxHashMapGet2(&libCodegen->exports, name->sym);
        if (item == NULL) {
            uint32_t idx = libCodegen->exports.count;
            item = fklCgExportSidIdxHashMapAdd(&libCodegen->exports,
                    &name->sym,
                    &(FklCodegenExportIdx){ .idx = idx,
                        .oidx = FKL_VAR_REF_INVALID_CIDX });
        }

        fklCodegenQuestVectorPushBack2(codegenQuestStack,
                fklCreateCodegenQuest(_export_define_bc_process,
                        create_export_define_context(name->sym, item),
                        createDefaultQueueNextExpression(queue),
                        1,
                        macroScope,
                        curEnv,
                        origExp->curline,
                        NULL,
                        codegen));
    } else if (isExportDefmacroExp(value, builtin_pattern_node)) {
        if (must_has_retval)
            goto must_has_retval_error;

        FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);

        fklCodegenQuestVectorPushBack2(codegenQuestStack,
                fklCreateCodegenQuest(_export_macro_bc_process,
                        createEmptyContext(),
                        createDefaultQueueNextExpression(queue),
                        1,
                        cms,
                        curEnv,
                        origExp->curline,
                        NULL,
                        codegen));
    } else if (isExportDefReaderMacroExp(value, builtin_pattern_node)) {
        if (must_has_retval)
            goto must_has_retval_error;

        FklSid_t group_id = get_reader_macro_group_id(value);
        if (!group_id)
            goto error;

        fklSidHashSetPut2(libCodegen->export_named_prod_groups, group_id);
        fklCodegenQuestVectorPushBack2(codegenQuestStack,
                fklCreateCodegenQuest(_empty_bc_process,
                        createEmptyContext(),
                        createDefaultQueueNextExpression(queue),
                        1,
                        macroScope,
                        curEnv,
                        origExp->curline,
                        NULL,
                        codegen));
    } else if (isExportImportExp(value, builtin_pattern_node)) {

        if (fklPatternMatch(
                    builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_NONE],
                    value,
                    NULL)
                && must_has_retval) {
        must_has_retval_error:
            fklDestroyNastNode(value);
            fklNastNodeQueueDestroy(queue);
            errorState->type = FKL_ERR_EXP_HAS_NO_VALUE;
            errorState->place = fklMakeNastNodeRef(origExp);
            return;
        }

        FklCodegenEnv *exportEnv = fklCreateCodegenEnv(NULL, 0, macroScope);
        FklCodegenMacroScope *cms = exportEnv->macros;

        fklCodegenQuestVectorPushBack2(codegenQuestStack,
                fklCreateCodegenQuest(_export_import_bc_process,
                        createExportContext(libCodegen,
                                curEnv,
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
                        origExp->curline,
                        NULL,
                        codegen));
    } else {
    error:
        fklDestroyNastNode(value);
        fklNastNodeQueueDestroy(queue);
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
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

static inline void process_import_script_common_header(FklNastNode *origExp,
        FklNastNode *name,
        const char *filename,
        FklCodegenEnv *curEnv,
        FklCodegenInfo *codegen,
        FklCodegenErrorState *errorState,
        FklCodegenQuestVector *codegenQuestStack,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        FklNastNode *prefix,
        FklNastNode *only,
        FklNastNode *alias,
        FklNastNode *except,
        FklSymbolTable *pst) {
    FklCodegenInfo *nextCodegen =
            createCodegenInfo(codegen, filename, 1, 0, codegen->outer_ctx);

    if (hasLoadSameFile(nextCodegen->realpath, codegen)) {
        errorState->type = FKL_ERR_CIRCULARLOAD;
        errorState->place = fklMakeNastNodeRef(name);
        nextCodegen->refcount = 1;
        fklDestroyCodegenInfo(nextCodegen);
        return;
    }
    size_t libId = check_loaded_lib(nextCodegen->realpath, codegen->libraries);
    if (!libId) {
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            nextCodegen->refcount = 1;
            fklDestroyCodegenInfo(nextCodegen);
            return;
        }
        FklCodegenEnv *libEnv = fklCreateCodegenEnv(codegen->global_env,
                0,
                codegen->global_env->macros);
        create_and_insert_to_pool(nextCodegen, 0, libEnv, 0, 1, pst);

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_library_bc_process,
                createExportContext(codegen,
                        curEnv,
                        scope,
                        macroScope,
                        prefix,
                        only,
                        alias,
                        except),
                createFpNextExpression(fp, nextCodegen),
                1,
                libEnv->macros,
                libEnv,
                origExp->curline,
                nextCodegen,
                codegenQuestStack);
    } else {
        fklReplacementHashMapDestroy(nextCodegen->export_replacement);
        nextCodegen->export_replacement = NULL;
        const FklCodegenLib *lib = &codegen->libraries->base[libId - 1];

        FklByteCodelnt *importBc = process_import_imported_lib(libId,
                codegen,
                lib,
                curEnv,
                scope,
                macroScope,
                origExp->curline,
                prefix,
                only,
                alias,
                except,
                errorState,
                pst);
        FklByteCodelntVector *bcStack = fklByteCodelntVectorCreate(1);
        fklByteCodelntVectorPushBack2(bcStack, importBc);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process,
                createDefaultStackContext(bcStack),
                NULL,
                1,
                codegen->global_env->macros,
                codegen->global_env,
                origExp->curline,
                nextCodegen,
                codegenQuestStack);
    }
}

FklCodegenDllLibInitExportFunc fklGetCodegenInitExportFunc(uv_lib_t *dll) {
    return (FklCodegenDllLibInitExportFunc)fklGetAddress("_fklExportSymbolInit",
            dll);
}

static inline FklByteCodelnt *process_import_from_dll_only(FklNastNode *origExp,
        FklNastNode *name,
        FklNastNode *only,
        const char *filename,
        FklCodegenEnv *curEnv,
        FklCodegenInfo *codegen,
        FklCodegenErrorState *errorState,
        uint32_t scope,
        FklSymbolTable *pst) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libraries);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            fklZfree(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCodegenDllLibInitExportFunc initExport =
                fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            errorState->type = FKL_ERR_IMPORTFAILED;
            errorState->place = fklMakeNastNodeRef(name);
            fklZfree(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
        fklInitCodegenDllLib(lib, realpath, dll, initExport, pst);
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
            origExp->curline,
            scope);

    FklCgExportSidIdxHashMap *exports = &lib->exports;

    for (; only->type == FKL_NAST_PAIR; only = only->pair->cdr) {
        FklSid_t cur = only->pair->car->sym;
        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);
        if (item) {
            uint32_t idx = fklAddCodegenDefBySid(cur, scope, curEnv)->idx;
            append_import_ins(INS_APPEND_BACK,
                    load_dll,
                    idx,
                    item->idx,
                    codegen->fid,
                    only->curline,
                    scope);
        } else {
            errorState->type = FKL_ERR_IMPORT_MISSING;
            errorState->place = fklMakeNastNodeRef(only->pair->car);
            errorState->line = only->curline;
            errorState->fid = fklAddSymbolCstr(codegen->filename,
                    &codegen->outer_ctx->public_symbol_table)
                                      ->v;
            break;
        }
    }

    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_except(
        FklNastNode *origExp,
        FklNastNode *name,
        FklNastNode *except,
        const char *filename,
        FklCodegenEnv *curEnv,
        FklCodegenInfo *codegen,
        FklCodegenErrorState *errorState,
        uint32_t scope,
        FklSymbolTable *pst) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libraries);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            fklZfree(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCodegenDllLibInitExportFunc initExport =
                fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            errorState->type = FKL_ERR_IMPORTFAILED;
            errorState->place = fklMakeNastNodeRef(name);
            fklZfree(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
        fklInitCodegenDllLib(lib, realpath, dll, initExport, pst);
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
            origExp->curline,
            scope);

    FklCgExportSidIdxHashMap *exports = &lib->exports;
    FklSidHashSet excepts;
    fklSidHashSetInit(&excepts);

    for (FklNastNode *list = except; list->type == FKL_NAST_PAIR;
            list = list->pair->cdr)
        fklSidHashSetPut2(&excepts, list->pair->car->sym);

    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        if (!fklSidHashSetHas2(&excepts, l->k)) {
            uint32_t idx = fklAddCodegenDefBySid(l->k, scope, curEnv)->idx;
            append_import_ins(INS_APPEND_BACK,
                    load_dll,
                    idx,
                    l->v.idx,
                    codegen->fid,
                    except->curline,
                    scope);
        }
    }

    fklSidHashSetUninit(&excepts);
    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_common(
        FklNastNode *origExp,
        FklNastNode *name,
        const char *filename,
        FklCodegenEnv *curEnv,
        FklCodegenInfo *codegen,
        FklCodegenErrorState *errorState,
        uint32_t scope,
        FklSymbolTable *pst) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libraries);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            fklZfree(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCodegenDllLibInitExportFunc initExport =
                fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            errorState->type = FKL_ERR_IMPORTFAILED;
            errorState->place = fklMakeNastNodeRef(name);
            fklZfree(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
        fklInitCodegenDllLib(lib, realpath, dll, initExport, pst);
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
            origExp->curline,
            scope);

    add_symbol_to_local_env_in_array(curEnv,
            &lib->exports,
            load_dll,
            codegen->fid,
            origExp->curline,
            scope,
            pst);
    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_prefix(
        FklNastNode *origExp,
        FklNastNode *name,
        FklNastNode *prefixNode,
        const char *filename,
        FklCodegenEnv *curEnv,
        FklCodegenInfo *codegen,
        FklCodegenErrorState *errorState,
        uint32_t scope,
        FklSymbolTable *pst) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libraries);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            fklZfree(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCodegenDllLibInitExportFunc initExport =
                fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            errorState->type = FKL_ERR_IMPORTFAILED;
            errorState->place = fklMakeNastNodeRef(name);
            fklZfree(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
        fklInitCodegenDllLib(lib, realpath, dll, initExport, pst);
        libId = codegen->libraries->size;
    } else {
        lib = &codegen->libraries->base[libId - 1];
        dll = lib->dll;
        fklZfree(realpath);
    }

    const FklString *prefix = fklGetSymbolWithId(prefixNode->sym, pst);

    FklByteCodelnt *load_dll = append_load_dll_ins(INS_APPEND_BACK,
            NULL,
            libId,
            codegen->fid,
            origExp->curline,
            scope);

    add_symbol_with_prefix_to_local_env_in_array(curEnv,
            prefix,
            &lib->exports,
            load_dll,
            codegen->fid,
            origExp->curline,
            scope,
            pst);

    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_alias(
        FklNastNode *origExp,
        FklNastNode *name,
        FklNastNode *alias,
        const char *filename,
        FklCodegenEnv *curEnv,
        FklCodegenInfo *codegen,
        FklCodegenErrorState *errorState,
        uint32_t scope,
        FklSymbolTable *pst) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libraries);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            fklZfree(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCodegenDllLibInitExportFunc initExport =
                fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            errorState->type = FKL_ERR_IMPORTFAILED;
            errorState->place = fklMakeNastNodeRef(name);
            fklZfree(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libraries, NULL);
        fklInitCodegenDllLib(lib, realpath, dll, initExport, pst);
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
            origExp->curline,
            scope);

    FklCgExportSidIdxHashMap *exports = &lib->exports;

    for (; alias->type == FKL_NAST_PAIR; alias = alias->pair->cdr) {
        FklNastNode *curNode = alias->pair->car;
        FklSid_t cur = curNode->pair->car->sym;
        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);

        if (item) {
            FklSid_t cur0 = curNode->pair->cdr->pair->car->sym;
            uint32_t idx = fklAddCodegenDefBySid(cur0, scope, curEnv)->idx;
            append_import_ins(INS_APPEND_BACK,
                    load_dll,
                    idx,
                    item->idx,
                    codegen->fid,
                    alias->curline,
                    scope);
        } else {
            errorState->type = FKL_ERR_IMPORT_MISSING;
            errorState->place = fklMakeNastNodeRef(curNode->pair->car);
            errorState->line = alias->curline;
            errorState->fid = fklAddSymbolCstr(codegen->filename,
                    &codegen->outer_ctx->public_symbol_table)
                                      ->v;
            break;
        }
    }

    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll(FklNastNode *origExp,
        FklNastNode *name,
        const char *filename,
        FklCodegenEnv *curEnv,
        FklCodegenInfo *codegen,
        FklCodegenErrorState *errorState,
        uint32_t scope,
        FklNastNode *prefix,
        FklNastNode *only,
        FklNastNode *alias,
        FklNastNode *except,
        FklSymbolTable *pst) {
    if (prefix)
        return process_import_from_dll_prefix(origExp,
                name,
                prefix,
                filename,
                curEnv,
                codegen,
                errorState,
                scope,
                pst);
    if (only)
        return process_import_from_dll_only(origExp,
                name,
                only,
                filename,
                curEnv,
                codegen,
                errorState,
                scope,
                pst);
    if (alias)
        return process_import_from_dll_alias(origExp,
                name,
                alias,
                filename,
                curEnv,
                codegen,
                errorState,
                scope,
                pst);
    if (except)
        return process_import_from_dll_except(origExp,
                name,
                except,
                filename,
                curEnv,
                codegen,
                errorState,
                scope,
                pst);
    return process_import_from_dll_common(origExp,
            name,
            filename,
            curEnv,
            codegen,
            errorState,
            scope,
            pst);
}

static inline int is_valid_alias_sym_list(FklNastNode *alias) {
    for (; alias->type == FKL_NAST_PAIR; alias = alias->pair->cdr) {
        FklNastNode *cur = alias->pair->car;
        if (cur->type != FKL_NAST_PAIR || cur->pair->car->type != FKL_NAST_SYM
                || cur->pair->cdr->type != FKL_NAST_PAIR
                || cur->pair->cdr->pair->car->type != FKL_NAST_SYM
                || cur->pair->cdr->pair->cdr->type != FKL_NAST_NIL)
            return 0;
    }

    return alias->type == FKL_NAST_NIL;
}

static inline void codegen_import_helper(FklNastNode *origExp,
        FklNastNode *name,
        FklNastNode *rest,
        FklCodegenInfo *codegen,
        FklCodegenErrorState *errorState,
        FklCodegenEnv *curEnv,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        FklCodegenQuestVector *codegenQuestStack,
        FklNastNode *prefix,
        FklNastNode *only,
        FklNastNode *alias,
        FklNastNode *except,
        FklSymbolTable *pst) {
    if (name->type == FKL_NAST_NIL
            || !fklIsNastNodeListAndHasSameType(name, FKL_NAST_SYM)
            || (prefix && prefix->type != FKL_NAST_SYM)
            || (only && !fklIsNastNodeListAndHasSameType(only, FKL_NAST_SYM))
            || (except
                    && !fklIsNastNodeListAndHasSameType(except, FKL_NAST_SYM))
            || (alias && !is_valid_alias_sym_list(alias))) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }

    if (rest->type != FKL_NAST_NIL) {
        FklNastNodeQueue *queue = fklNastNodeQueueCreate();

        FklNastPair *prevPair = origExp->pair->cdr->pair;

        FklNastNode *importHead = origExp->pair->car;

        for (; rest->type == FKL_NAST_PAIR; rest = rest->pair->cdr) {
            FklNastNode *restExp = fklNastCons(fklMakeNastNodeRef(importHead),
                    rest,
                    rest->curline);

            prevPair->cdr = fklCreateNastNode(FKL_NAST_NIL, rest->curline);

            fklNastNodeQueuePush2(queue, restExp);

            prevPair = rest->pair;
        }
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_begin_exp_bc_process,
                createDefaultStackContext(fklByteCodelntVectorCreate(2)),
                createDefaultQueueNextExpression(queue),
                scope,
                macroScope,
                curEnv,
                origExp->curline,
                codegen,
                codegenQuestStack);
    }

    char *filename = combineFileNameFromListAndCheckPrivate(name, pst);

    if (filename == NULL) {
        errorState->type = FKL_ERR_IMPORTFAILED;
        errorState->place = fklMakeNastNodeRef(origExp);
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
        process_import_script_common_header(origExp,
                name,
                packageMainFileName,
                curEnv,
                codegen,
                errorState,
                codegenQuestStack,
                scope,
                macroScope,
                prefix,
                only,
                alias,
                except,
                pst);
    else if (fklIsAccessibleRegFile(scriptFileName))
        process_import_script_common_header(origExp,
                name,
                scriptFileName,
                curEnv,
                codegen,
                errorState,
                codegenQuestStack,
                scope,
                macroScope,
                prefix,
                only,
                alias,
                except,
                pst);
    else if (fklIsAccessibleRegFile(preCompileFileName)) {
        size_t libId = check_loaded_lib(preCompileFileName, codegen->libraries);
        if (!libId) {
            char *errorStr = NULL;
            if (fklLoadPreCompile(codegen->pts,
                        codegen->macro_pts,
                        codegen->libraries,
                        codegen->macro_libraries,
                        codegen->runtime_symbol_table,
                        codegen->runtime_kt,
                        codegen->outer_ctx,
                        preCompileFileName,
                        NULL,
                        &errorStr)) {
                if (errorStr) {
                    fprintf(stderr, "%s\n", errorStr);
                    fklZfree(errorStr);
                }
                errorState->type = FKL_ERR_IMPORTFAILED;
                errorState->place = fklMakeNastNodeRef(name);
                goto exit;
            }
            libId = codegen->libraries->size;
        }
        const FklCodegenLib *lib = &codegen->libraries->base[libId - 1];

        FklByteCodelnt *importBc = process_import_imported_lib(libId,
                codegen,
                lib,
                curEnv,
                scope,
                macroScope,
                origExp->curline,
                prefix,
                only,
                alias,
                except,
                errorState,
                pst);
        FklByteCodelntVector *bcStack = fklByteCodelntVectorCreate(1);
        fklByteCodelntVectorPushBack2(bcStack, importBc);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process,
                createDefaultStackContext(bcStack),
                NULL,
                1,
                codegen->global_env->macros,
                codegen->global_env,
                origExp->curline,
                codegen,
                codegenQuestStack);
    } else if (fklIsAccessibleRegFile(dllFileName)) {
        FklByteCodelnt *bc = process_import_from_dll(origExp,
                name,
                dllFileName,
                curEnv,
                codegen,
                errorState,
                scope,
                prefix,
                only,
                alias,
                except,
                pst);
        if (bc) {
            FklByteCodelntVector *bcStack = fklByteCodelntVectorCreate(1);
            fklByteCodelntVectorPushBack2(bcStack, bc);
            FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_default_bc_process,
                    createDefaultStackContext(bcStack),
                    NULL,
                    1,
                    NULL,
                    NULL,
                    origExp->curline,
                    codegen,
                    codegenQuestStack);
        }
    } else {
        errorState->type = FKL_ERR_IMPORTFAILED;
        errorState->place = fklMakeNastNodeRef(name);
    }
exit:
    fklZfree(filename);
    fklZfree(scriptFileName);
    fklZfree(dllFileName);
    fklZfree(packageMainFileName);
    fklZfree(preCompileFileName);
}

static CODEGEN_FUNC(codegen_import) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp,
            name,
            rest,
            codegen,
            errorState,
            curEnv,
            scope,
            macroScope,
            codegenQuestStack,
            NULL,
            NULL,
            NULL,
            NULL,
            &outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_none) {
    if (must_has_retval) {
        errorState->type = FKL_ERR_EXP_HAS_NO_VALUE;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    fklCodegenQuestVectorPushBack2(codegenQuestStack,
            fklCreateCodegenQuest(_empty_bc_process,
                    createEmptyContext(),
                    NULL,
                    1,
                    macroScope,
                    curEnv,
                    origExp->curline,
                    NULL,
                    codegen));
}

static CODEGEN_FUNC(codegen_import_prefix) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *prefix =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp,
            name,
            rest,
            codegen,
            errorState,
            curEnv,
            scope,
            macroScope,
            codegenQuestStack,
            prefix,
            NULL,
            NULL,
            NULL,
            &outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_only) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *only =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp,
            name,
            rest,
            codegen,
            errorState,
            curEnv,
            scope,
            macroScope,
            codegenQuestStack,
            NULL,
            only,
            NULL,
            NULL,
            &outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_alias) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *alias =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp,
            name,
            rest,
            codegen,
            errorState,
            curEnv,
            scope,
            macroScope,
            codegenQuestStack,
            NULL,
            NULL,
            alias,
            NULL,
            &outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_except) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *except =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp,
            name,
            rest,
            codegen,
            errorState,
            curEnv,
            scope,
            macroScope,
            codegenQuestStack,
            NULL,
            NULL,
            NULL,
            except,
            &outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_common) {
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp,
            name,
            rest,
            codegen,
            errorState,
            curEnv,
            scope,
            macroScope,
            codegenQuestStack,
            NULL,
            NULL,
            NULL,
            NULL,
            &outer_ctx->public_symbol_table);
}

typedef struct {
    FklByteCodelntVector stack;
    FklNastNode *pattern;
    FklNastNode *origin_exp;
    FklCodegenMacroScope *macroScope;
    uint32_t prototype_id;
} MacroContext;

static inline MacroContext *createMacroContext(FklNastNode *origin_exp,
        FklNastNode *pattern,
        FklCodegenMacroScope *macroScope,
        uint32_t prototype_id) {
    MacroContext *r = (MacroContext *)fklZmalloc(sizeof(MacroContext));
    FKL_ASSERT(r);
    r->pattern = pattern;
    r->origin_exp = fklMakeNastNodeRef(origin_exp);
    r->macroScope = macroScope ? make_macro_scope_ref(macroScope) : NULL;
    fklByteCodelntVectorInit(&r->stack, 1);
    r->prototype_id = prototype_id;
    return r;
}

BC_PROCESS(_compiler_macro_bc_process) {
    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    fklUpdatePrototype(codegen->pts, env, codegen->runtime_symbol_table, pst);
    fklPrintUndefinedRef(env->prev, codegen->runtime_symbol_table, pst);

    MacroContext *d = (MacroContext *)(context->data);
    FklByteCodelntVector *stack = &d->stack;
    FklByteCodelnt *macroBcl = *fklByteCodelntVectorPopBackNonNull(stack);
    FklInstruction ret = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(macroBcl, &ret, fid, line, scope);

    FklNastNode *pattern = d->pattern;
    FklCodegenMacroScope *macros = d->macroScope;
    uint32_t prototype_id = d->prototype_id;

    fklPeepholeOptimize(macroBcl);
    add_compiler_macro(&macros->head,
            fklMakeNastNodeRef(pattern),
            fklMakeNastNodeRef(d->origin_exp),
            macroBcl,
            prototype_id);
    fklDestroyByteCodelnt(macroBcl);
    return NULL;
}

static FklByteCodelntVector *_macro_stack_context_get_bcl_stack(void *data) {
    return &((MacroContext *)data)->stack;
}

static void _macro_stack_context_put_bcl(void *data, FklByteCodelnt *bcl) {
    MacroContext *d = (MacroContext *)data;
    fklByteCodelntVectorPushBack2(&d->stack, bcl);
}

static void _macro_stack_context_finalizer(void *data) {
    MacroContext *d = (MacroContext *)data;
    FklByteCodelntVector *s = &d->stack;
    while (!fklByteCodelntVectorIsEmpty(s))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBackNonNull(s));
    fklByteCodelntVectorUninit(s);
    fklDestroyNastNode(d->pattern);
    fklDestroyNastNode(d->origin_exp);
    if (d->macroScope)
        fklDestroyCodegenMacroScope(d->macroScope);
    fklZfree(d);
}

static const FklCodegenQuestContextMethodTable MacroStackContextMethodTable = {
    .__get_bcl_stack = _macro_stack_context_get_bcl_stack,
    .__put_bcl = _macro_stack_context_put_bcl,
    .__finalizer = _macro_stack_context_finalizer,
};

static inline FklCodegenQuestContext *createMacroQuestContext(
        FklNastNode *origin_exp,
        FklNastNode *pattern,
        FklCodegenMacroScope *macroScope,
        uint32_t prototype_id) {
    return createCodegenQuestContext(
            createMacroContext(origin_exp, pattern, macroScope, prototype_id),
            &MacroStackContextMethodTable);
}

struct ReaderMacroCtx {
    FklByteCodelntVector stack;
    FklFuncPrototypes *pts;
    struct CustomActionCtx *action_ctx;
};

static void *custom_action(void *c,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    FklNastNode *nodes_vector = fklCreateNastNode(FKL_NAST_VECTOR, line);
    nodes_vector->vec = fklCreateNastVector(num);
    for (size_t i = 0; i < num; i++)
        nodes_vector->vec->base[i] = fklMakeNastNodeRef(nodes[i].ast);
    FklLineNumHashMap lineHash;
    fklLineNumHashMapInit(&lineHash);
    FklPmatchHashMap ht;
    fklPmatchHashMapInit(&ht);
    FklNastNode *line_node = NULL;
    if (line > FKL_FIX_INT_MAX) {
        line_node = fklCreateNastNode(FKL_NAST_BIGINT, line);
        line_node->bigInt = fklCreateBigIntU(line);
    } else {
        line_node = fklCreateNastNode(FKL_NAST_FIX, line);
        line_node->fix = line;
    }

    struct CustomActionCtx *ctx = (struct CustomActionCtx *)c;
    fklPmatchHashMapAdd2(&ht,
            fklAddSymbolCstr("$$", ctx->pst)->v,
            nodes_vector);
    fklPmatchHashMapAdd2(&ht, fklAddSymbolCstr("line", ctx->pst)->v, line_node);

    FklNastNode *r = NULL;
    const char *cwd = ctx->codegen_outer_ctx->cwd;
    const char *file_dir = ctx->codegen_outer_ctx->cur_file_dir;
    fklChdir(file_dir);

    FklVM *exe = fklInitMacroExpandVM(ctx->codegen_outer_ctx,
            ctx->bcl,
            ctx->pts,
            ctx->prototype_id,
            &ht,
            &lineHash,
            ctx->macro_libraries,
            &r,
            line,
            ctx->pst,
            &ctx->codegen_outer_ctx->public_kt);
    FklVMgc *gc = exe->gc;

    int e = fklRunVM(exe);
    fklMoveThreadObjectsToGc(exe, gc);

    fklChdir(cwd);

    gc->pts = NULL;
    if (e)
        fklDeleteCallChain(exe);

    fklDestroyNastNode(nodes_vector);
    fklDestroyNastNode(line_node);

    fklPmatchHashMapUninit(&ht);
    fklLineNumHashMapUninit(&lineHash);
    fklDestroyAllVMs(exe);
    return r;
}

static void *custom_action_ctx_copy(const void *c) {
    struct CustomActionCtx *ctx = (struct CustomActionCtx *)c;
    ctx->refcount++;
    return ctx;
}

static void custom_action_ctx_destroy(void *c) {
    struct CustomActionCtx *ctx = (struct CustomActionCtx *)c;
    if (ctx->refcount)
        ctx->refcount--;
    else {
        if (ctx->bcl)
            fklDestroyByteCodelnt(ctx->bcl);
        fklZfree(ctx);
    }
}

static inline struct ReaderMacroCtx *
createReaderMacroContext(struct CustomActionCtx *ctx, FklFuncPrototypes *pts) {
    struct ReaderMacroCtx *r =
            (struct ReaderMacroCtx *)fklZmalloc(sizeof(struct ReaderMacroCtx));
    FKL_ASSERT(r);
    r->action_ctx = custom_action_ctx_copy(ctx);
    r->pts = pts;
    fklByteCodelntVectorInit(&r->stack, 1);
    return r;
}

BC_PROCESS(_reader_macro_bc_process) {
    struct ReaderMacroCtx *d = (struct ReaderMacroCtx *)(context->data);
    FklFuncPrototypes *pts = d->pts;
    struct CustomActionCtx *custom_ctx = d->action_ctx;
    d->pts = NULL;
    d->action_ctx = NULL;

    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    custom_action_ctx_destroy(custom_ctx);
    fklUpdatePrototype(codegen->pts, env, codegen->runtime_symbol_table, pst);
    fklPrintUndefinedRef(env->prev, codegen->runtime_symbol_table, pst);

    FklByteCodelntVector *stack = &d->stack;
    FklByteCodelnt *macroBcl = *fklByteCodelntVectorPopBackNonNull(stack);
    FklInstruction ret = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(macroBcl, &ret, fid, line, scope);

    fklPeepholeOptimize(macroBcl);
    custom_ctx->pts = pts;
    custom_ctx->bcl = macroBcl;
    return NULL;
}

static FklByteCodelntVector *_reader_macro_stack_context_get_bcl_stack(
        void *data) {
    return &((struct ReaderMacroCtx *)data)->stack;
}

static void _reader_macro_stack_context_put_bcl(void *data,
        FklByteCodelnt *bcl) {
    struct ReaderMacroCtx *d = (struct ReaderMacroCtx *)data;
    fklByteCodelntVectorPushBack2(&d->stack, bcl);
}

static void _reader_macro_stack_context_finalizer(void *data) {
    struct ReaderMacroCtx *d = (struct ReaderMacroCtx *)data;
    FklByteCodelntVector *s = &d->stack;
    while (!fklByteCodelntVectorIsEmpty(s))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBackNonNull(s));
    fklByteCodelntVectorUninit(s);
    if (d->action_ctx)
        custom_action_ctx_destroy(d->action_ctx);
    fklZfree(d);
}

static const FklCodegenQuestContextMethodTable
        ReaderMacroStackContextMethodTable = {
            .__get_bcl_stack = _reader_macro_stack_context_get_bcl_stack,
            .__put_bcl = _reader_macro_stack_context_put_bcl,
            .__finalizer = _reader_macro_stack_context_finalizer,
        };

static inline FklCodegenQuestContext *createReaderMacroQuestContext(
        struct CustomActionCtx *ctx,
        FklFuncPrototypes *pts) {
    return createCodegenQuestContext(createReaderMacroContext(ctx, pts),
            &ReaderMacroStackContextMethodTable);
}

static FklNastNode *_nil_replacement(const FklNastNode *orig,
        const FklCodegenEnv *env,
        const FklCodegenInfo *codegen) {
    return fklCreateNastNode(FKL_NAST_NIL, orig->curline);
}

static FklNastNode *_is_main_replacement(const FklNastNode *orig,
        const FklCodegenEnv *env,
        const FklCodegenInfo *codegen) {
    FklNastNode *n = fklCreateNastNode(FKL_NAST_FIX, orig->curline);
    if (env->prototypeId == 1)
        n->fix = 1;
    else
        n->type = FKL_NAST_NIL;
    return n;
}

static FklNastNode *_platform_replacement(const FklNastNode *orig,
        const FklCodegenEnv *env,
        const FklCodegenInfo *codegen) {

#if __linux__
    static const char *platform = "linux";
#elif __unix__
    static const char *platform = "unix";
#elif defined _WIN32
    static const char *platform = "win32";
#elif __APPLE__
    static const char *platform = "apple";
#endif
    FklNastNode *n = fklCreateNastNode(FKL_NAST_STR, orig->curline);
    n->str = fklCreateStringFromCstr(platform);
    return n;
}

static FklNastNode *_file_dir_replacement(const FklNastNode *orig,
        const FklCodegenEnv *env,
        const FklCodegenInfo *codegen) {
    FklString *s = NULL;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_STR, orig->curline);
    if (codegen->filename == NULL)
        s = fklCreateStringFromCstr(codegen->outer_ctx->cwd);
    else
        s = fklCreateStringFromCstr(codegen->dir);
    fklStringCstrCat(&s, FKL_PATH_SEPARATOR_STR);
    r->str = s;
    return r;
}

static FklNastNode *_file_replacement(const FklNastNode *orig,
        const FklCodegenEnv *env,
        const FklCodegenInfo *codegen) {
    FklNastNode *r = NULL;
    if (codegen->filename == NULL)
        r = fklCreateNastNode(FKL_NAST_NIL, orig->curline);
    else {
        r = fklCreateNastNode(FKL_NAST_STR, orig->curline);
        FklString *s = NULL;
        s = fklCreateStringFromCstr(codegen->filename);
        r->str = s;
    }
    return r;
}

static FklNastNode *_file_rp_replacement(const FklNastNode *orig,
        const FklCodegenEnv *env,
        const FklCodegenInfo *codegen) {
    FklNastNode *r = NULL;
    if (codegen->realpath == NULL)
        r = fklCreateNastNode(FKL_NAST_NIL, orig->curline);
    else {
        r = fklCreateNastNode(FKL_NAST_STR, orig->curline);
        FklString *s = NULL;
        s = fklCreateStringFromCstr(codegen->realpath);
        r->str = s;
    }
    return r;
}

static FklNastNode *_line_replacement(const FklNastNode *orig,
        const FklCodegenEnv *env,
        const FklCodegenInfo *codegen) {
    uint64_t line = orig->curline;
    FklNastNode *r = NULL;
    if (line < FKL_FIX_INT_MAX) {
        r = fklCreateNastNode(FKL_NAST_FIX, orig->curline);
        r->fix = line;
    } else {
        r = fklCreateNastNode(FKL_NAST_BIGINT, orig->curline);
        r->bigInt = fklCreateBigIntU(line);
    }
    return r;
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

static inline ReplacementFunc findBuiltInReplacementWithId(FklSid_t id,
        const FklSid_t builtin_replacement_id[]) {
    for (size_t i = 0; i < FKL_BUILTIN_REPLACEMENT_NUM; i++) {
        if (builtin_replacement_id[i] == id)
            return builtInSymbolReplacement[i].func;
    }
    return NULL;
}

static void *codegen_prod_action_nil(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    return fklCreateNastNode(FKL_NAST_NIL, line);
}

static void *codegen_prod_action_first(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 1)
        return NULL;
    return fklMakeNastNodeRef(nodes[0].ast);
}

static void *codegen_prod_action_symbol(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 1)
        return NULL;
    FklNastNode *node = nodes[0].ast;
    if (node->type != FKL_NAST_STR)
        return NULL;
    FklNastNode *sym = fklCreateNastNode(FKL_NAST_SYM, node->curline);
    sym->sym = fklAddSymbol(node->str, outerCtx)->v;
    return sym;
}

static void *codegen_prod_action_second(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    return fklMakeNastNodeRef(nodes[1].ast);
}

static void *codegen_prod_action_third(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 3)
        return NULL;
    return fklMakeNastNodeRef(nodes[2].ast);
}

static inline void *codegen_prod_action_pair(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 3)
        return NULL;
    FklNastNode *car = nodes[0].ast;
    FklNastNode *cdr = nodes[2].ast;
    FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    pair->pair = fklCreateNastPair();
    pair->pair->car = fklMakeNastNodeRef(car);
    pair->pair->cdr = fklMakeNastNodeRef(cdr);
    return pair;
}

static inline void *codegen_prod_action_cons(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num == 1) {
        FklNastNode *car = nodes[0].ast;
        FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
        pair->pair = fklCreateNastPair();
        pair->pair->car = fklMakeNastNodeRef(car);
        pair->pair->cdr = fklCreateNastNode(FKL_NAST_NIL, line);
        return pair;
    } else if (num == 2) {
        FklNastNode *car = nodes[0].ast;
        FklNastNode *cdr = nodes[1].ast;
        FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
        pair->pair = fklCreateNastPair();
        pair->pair->car = fklMakeNastNodeRef(car);
        pair->pair->cdr = fklMakeNastNodeRef(cdr);
        return pair;
    } else
        return NULL;
}

static inline void *codegen_prod_action_box(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklNastNode *box = fklCreateNastNode(FKL_NAST_BOX, line);
    box->box = fklMakeNastNodeRef(nodes[1].ast);
    return box;
}

static inline void *codegen_prod_action_vector(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklNastNode *list = nodes[1].ast;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_VECTOR, line);
    size_t len = fklNastListLength(list);
    FklNastVector *vec = fklCreateNastVector(len);
    r->vec = vec;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++)
        vec->base[i] = fklMakeNastNodeRef(list->pair->car);
    return r;
}

static inline void *codegen_prod_action_quote(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklSid_t id = fklAddSymbolCstr("quote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = { head, s_exp };
    return create_nast_list(s_exps, 2, line);
}

static inline void *codegen_prod_action_unquote(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklSid_t id = fklAddSymbolCstr("unquote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = { head, s_exp };
    return create_nast_list(s_exps, 2, line);
}

static inline void *codegen_prod_action_qsquote(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklSid_t id = fklAddSymbolCstr("qsquote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = { head, s_exp };
    return create_nast_list(s_exps, 2, line);
}

static inline void *codegen_prod_action_unqtesp(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklSid_t id = fklAddSymbolCstr("unqtesp", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1].ast);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = { head, s_exp };
    return create_nast_list(s_exps, 2, line);
}

static inline void *codegen_prod_action_hasheq(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklNastNode *list = nodes[1].ast;
    if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
        return NULL;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQ, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *codegen_prod_action_hasheqv(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return 0;
    FklNastNode *list = nodes[1].ast;
    if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
        return NULL;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQV, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *codegen_prod_action_hashequal(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return 0;
    FklNastNode *list = nodes[1].ast;
    if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
        return NULL;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQUAL, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *codegen_prod_action_bytevector(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    if (num < 2)
        return NULL;
    FklNastNode *list = nodes[1].ast;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_BYTEVECTOR, line);
    size_t len = fklNastListLength(list);
    FklBytevector *bv = fklCreateBytevector(len, NULL);
    r->bvec = bv;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        FklNastNode *cur = list->pair->car;
        if (cur->type == FKL_NAST_FIX)
            bv->ptr[i] = cur->fix > UINT8_MAX ? UINT8_MAX
                                              : (cur->fix < 0 ? 0 : cur->fix);
        else
            bv->ptr[i] = cur->bigInt->num < 0 ? 0 : UINT8_MAX;
    }
    return r;
}

static struct CstrIdProdAction {
    const char *name;
    FklProdActionFunc func;
} CodegenProdActions[FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM] = {
    // clang-format off
    {"nil",       codegen_prod_action_nil        },
    {"symbol",    codegen_prod_action_symbol     },
    {"first",     codegen_prod_action_first      },
    {"second",    codegen_prod_action_second     },
    {"third",     codegen_prod_action_third      },
    {"pair",      codegen_prod_action_pair       },
    {"cons",      codegen_prod_action_cons       },
    {"box",       codegen_prod_action_box        },
    {"vector",    codegen_prod_action_vector     },
    {"quote",     codegen_prod_action_quote      },
    {"unquote",   codegen_prod_action_unquote    },
    {"qsquote",   codegen_prod_action_qsquote    },
    {"unqtesp",   codegen_prod_action_unqtesp    },
    {"hasheq",    codegen_prod_action_hasheq     },
    {"hasheqv",   codegen_prod_action_hasheqv    },
    {"hashequal", codegen_prod_action_hashequal  },
    {"bytes",     codegen_prod_action_bytevector },
    // clang-format on
};

static inline void init_builtin_prod_action_list(
        FklSid_t *builtin_prod_action_id,
        FklSymbolTable *pst) {
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++)
        builtin_prod_action_id[i] =
                fklAddSymbolCstr(CodegenProdActions[i].name, pst)->v;
}

static inline FklProdActionFunc find_builtin_prod_action(FklSid_t id,
        FklSid_t *builtin_prod_action_id) {
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++) {
        if (builtin_prod_action_id[i] == id)
            return CodegenProdActions[i].func;
    }
    return NULL;
}

static inline const char *find_builtin_prod_action_name(
        FklProdActionFunc func) {
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++) {
        if (CodegenProdActions[i].func == func)
            return CodegenProdActions[i].name;
    }
    return NULL;
}

FklProdActionFunc fklFindBuiltinProdActionByName(const char *str) {
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++) {
        if (strcmp(CodegenProdActions[i].name, str) == 0)
            return CodegenProdActions[i].func;
    }
    return NULL;
}

static void *
simple_action_nth_create(FklNastNode *rest[], size_t rest_len, int *failed) {
    if (rest_len != 1 || rest[0]->type != FKL_NAST_FIX || rest[0]->fix < 0) {
        *failed = 1;
        return NULL;
    }
    return (void *)(rest[0]->fix);
}

static void
simple_action_nth_write(void *ctx, const FklSymbolTable *pst, FILE *fp) {
    uint64_t len = FKL_TYPE_CAST(uintptr_t, ctx);
    fwrite(&len, sizeof(len), 1, fp);
}

static void
simple_action_nth_print(void *ctx, const FklSymbolTable *pst, FILE *fp) {
    uint64_t len = FKL_TYPE_CAST(uintptr_t, ctx);
    fprintf(fp, "%" PRIu64, len);
}

static void *simple_action_nth_reader(FklSymbolTable *pst, FILE *fp) {
    uint64_t len = 0;
    fread(&len, sizeof(len), 1, fp);
    return FKL_TYPE_CAST(void *, len);
}

static void *simple_action_nth(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    return fklMakeNastNodeRef(nodes[nth].ast);
}

struct SimpleActionConsCtx {
    uint64_t car;
    uint64_t cdr;
};

static void *
simple_action_cons_create(FklNastNode *rest[], size_t rest_len, int *failed) {
    if (rest_len != 2 || rest[0]->type != FKL_NAST_FIX || rest[0]->fix < 0
            || rest[1]->type != FKL_NAST_FIX || rest[1]->fix < 0) {
        *failed = 1;
        return NULL;
    }
    struct SimpleActionConsCtx *ctx = (struct SimpleActionConsCtx *)fklZmalloc(
            sizeof(struct SimpleActionConsCtx));
    FKL_ASSERT(ctx);
    ctx->car = rest[0]->fix;
    ctx->cdr = rest[1]->fix;
    return ctx;
}

static void *simple_action_cons_copy(const void *c) {
    struct SimpleActionConsCtx *ctx = (struct SimpleActionConsCtx *)fklZmalloc(
            sizeof(struct SimpleActionConsCtx));
    FKL_ASSERT(ctx);
    *ctx = *(struct SimpleActionConsCtx *)c;
    return ctx;
}

static void *simple_action_cons(void *c,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    struct SimpleActionConsCtx *cc = (struct SimpleActionConsCtx *)c;
    if (cc->car >= num || cc->cdr >= num)
        return NULL;
    FklNastNode *retval = fklCreateNastNode(FKL_NAST_PAIR, line);
    retval->pair = fklCreateNastPair();
    retval->pair->car = fklMakeNastNodeRef(nodes[cc->car].ast);
    retval->pair->cdr = fklMakeNastNodeRef(nodes[cc->cdr].ast);
    return retval;
}

static void
simple_action_cons_write(void *c, const FklSymbolTable *pst, FILE *fp) {
    struct SimpleActionConsCtx *ctx = c;
    fwrite(&ctx->car, sizeof(ctx->car), 1, fp);
    fwrite(&ctx->cdr, sizeof(ctx->cdr), 1, fp);
}

static void
simple_action_cons_print(void *c, const FklSymbolTable *pst, FILE *fp) {
    struct SimpleActionConsCtx *ctx = c;
    fprintf(fp, "%" PRIu64 ", %" PRIu64, ctx->car, ctx->cdr);
}

static void *simple_action_cons_read(FklSymbolTable *pst, FILE *fp) {
    struct SimpleActionConsCtx *ctx = (struct SimpleActionConsCtx *)fklZmalloc(
            sizeof(struct SimpleActionConsCtx));
    FKL_ASSERT(ctx);
    fread(&ctx->car, sizeof(ctx->car), 1, fp);
    fread(&ctx->cdr, sizeof(ctx->cdr), 1, fp);

    return ctx;
}

struct SimpleActionHeadCtx {
    FklNastNode *head;
    uint64_t idx_num;
    uint64_t idx[];
};

static void *simple_action_head(void *c,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    struct SimpleActionHeadCtx *cc = (struct SimpleActionHeadCtx *)c;
    for (size_t i = 0; i < cc->idx_num; i++)
        if (cc->idx[i] >= num)
            return NULL;
    FklNastNode *head = fklMakeNastNodeRef(cc->head);
    FklNastNode **exps = (FklNastNode **)fklZmalloc(
            (1 + cc->idx_num) * sizeof(FklNastNode *));
    FKL_ASSERT(exps);
    exps[0] = head;
    for (size_t i = 0; i < cc->idx_num; i++)
        exps[i + 1] = fklMakeNastNodeRef(nodes[cc->idx[i]].ast);
    FklNastNode *retval = create_nast_list(exps, 1 + cc->idx_num, line);
    fklZfree(exps);
    return retval;
}

static void *
simple_action_head_create(FklNastNode *rest[], size_t rest_len, int *failed) {
    if (rest_len < 2 || rest[1]->type != FKL_NAST_FIX || rest[1]->fix < 0) {
        *failed = 1;
        return NULL;
    }
    for (size_t i = 1; i < rest_len; i++)
        if (rest[i]->type != FKL_NAST_FIX || rest[i]->fix < 0)
            return NULL;
    struct SimpleActionHeadCtx *ctx = (struct SimpleActionHeadCtx *)fklZmalloc(
            sizeof(struct SimpleActionHeadCtx)
            + ((rest_len - 1) * sizeof(uint64_t)));
    FKL_ASSERT(ctx);
    ctx->head = fklMakeNastNodeRef(rest[0]);
    ctx->idx_num = rest_len - 1;
    for (size_t i = 1; i < rest_len; i++)
        ctx->idx[i - 1] = rest[i]->fix;
    return ctx;
}

static void
simple_action_head_write(void *c, const FklSymbolTable *pst, FILE *fp) {
    struct SimpleActionHeadCtx *ctx = c;
    print_nast_node_with_null_chr(ctx->head, pst, fp);
    fwrite(&ctx->idx_num, sizeof(ctx->idx_num), 1, fp);
    fwrite(&ctx->idx[0], sizeof(ctx->idx[0]), ctx->idx_num, fp);
}

static void
simple_action_head_print(void *c, const FklSymbolTable *pst, FILE *fp) {
    struct SimpleActionHeadCtx *ctx = c;
    fklPrintNastNode(ctx->head, fp, pst);
    for (size_t i = 0; i < ctx->idx_num; ++i) {
        fprintf(fp, ", %" PRIu64, ctx->idx[i]);
    }
}

static void *simple_action_head_read(FklSymbolTable *pst, FILE *fp) {
    FklNastNode *node = load_nast_node_with_null_chr(pst, fp);
    uint64_t idx_num = 0;
    fread(&idx_num, sizeof(idx_num), 1, fp);
    struct SimpleActionHeadCtx *ctx = (struct SimpleActionHeadCtx *)fklZmalloc(
            sizeof(struct SimpleActionHeadCtx) + (idx_num * sizeof(uint64_t)));
    FKL_ASSERT(ctx);
    ctx->head = node;
    ctx->idx_num = idx_num;
    fread(&ctx->idx[0], sizeof(ctx->idx[0]), ctx->idx_num, fp);

    return ctx;
}

static void *simple_action_head_copy(const void *c) {
    struct SimpleActionHeadCtx *cc = (struct SimpleActionHeadCtx *)c;
    size_t size = sizeof(struct SimpleActionHeadCtx)
                + (sizeof(uint64_t) * cc->idx_num);
    struct SimpleActionHeadCtx *ctx =
            (struct SimpleActionHeadCtx *)fklZmalloc(size);
    FKL_ASSERT(ctx);
    memcpy(ctx, cc, size);
    fklMakeNastNodeRef(ctx->head);
    return ctx;
}

static void simple_action_head_destroy(void *cc) {
    struct SimpleActionHeadCtx *ctx = (struct SimpleActionHeadCtx *)cc;
    fklDestroyNastNode(ctx->head);
    fklZfree(ctx);
}

static inline void *simple_action_box(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *box = fklCreateNastNode(FKL_NAST_BOX, line);
    box->box = fklMakeNastNodeRef(nodes[nth].ast);
    return box;
}

static inline void *simple_action_vector(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *list = nodes[nth].ast;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_VECTOR, line);
    size_t len = fklNastListLength(list);
    FklNastVector *vec = fklCreateNastVector(len);
    r->vec = vec;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++)
        vec->base[i] = fklMakeNastNodeRef(list->pair->car);
    return r;
}

static inline void *simple_action_hasheq(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *list = nodes[nth].ast;
    if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
        return NULL;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQ, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *simple_action_hasheqv(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *list = nodes[nth].ast;
    if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
        return NULL;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQV, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *simple_action_hashequal(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *list = nodes[nth].ast;
    if (!fklIsNastNodeListAndHasSameType(list, FKL_NAST_PAIR))
        return NULL;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_HASHTABLE, line);
    size_t len = fklNastListLength(list);
    FklNastHashTable *ht = fklCreateNastHash(FKL_HASH_EQUAL, len);
    r->hash = ht;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        ht->items[i].car = fklMakeNastNodeRef(list->pair->car->pair->car);
        ht->items[i].cdr = fklMakeNastNodeRef(list->pair->car->pair->cdr);
    }
    return r;
}

static inline void *simple_action_bytevector(void *ctx,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *list = nodes[nth].ast;
    FklNastNode *r = fklCreateNastNode(FKL_NAST_BYTEVECTOR, line);
    size_t len = fklNastListLength(list);
    FklBytevector *bv = fklCreateBytevector(len, NULL);
    r->bvec = bv;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++) {
        FklNastNode *cur = list->pair->car;
        if (cur->type == FKL_NAST_FIX)
            bv->ptr[i] = cur->fix > UINT8_MAX ? UINT8_MAX
                                              : (cur->fix < 0 ? 0 : cur->fix);
        else
            bv->ptr[i] = cur->bigInt->num < 0 ? 0 : UINT8_MAX;
    }
    return r;
}

struct SimpleActionSymbolCtx {
    uint64_t nth;
    FklString *start;
    FklString *end;
};

static void *
simple_action_symbol_create(FklNastNode *rest[], size_t rest_len, int *failed) {
    FklString *start = NULL;
    FklString *end = NULL;
    if (rest_len == 0 || rest[0]->type != FKL_NAST_FIX || rest[0]->fix < 0) {
        *failed = 1;
        return NULL;
    }
    if (rest_len == 3) {
        if (rest[1]->type != FKL_NAST_STR || rest[1]->str->size == 0
                || rest[2]->type != FKL_NAST_STR || rest[2]->str->size == 0) {
            *failed = 1;
            return NULL;
        }
        start = rest[1]->str;
        end = rest[2]->str;
    } else if (rest_len == 2) {
        if (rest[1]->type != FKL_NAST_STR || rest[1]->str->size == 0) {
            *failed = 1;
            return NULL;
        }
        start = rest[1]->str;
        end = start;
    }
    struct SimpleActionSymbolCtx *ctx =
            (struct SimpleActionSymbolCtx *)fklZmalloc(
                    sizeof(struct SimpleActionSymbolCtx));
    FKL_ASSERT(ctx);
    ctx->nth = rest[0]->fix;
    ctx->start = start ? fklCopyString(start) : NULL;
    ctx->end = end ? fklCopyString(end) : NULL;
    return ctx;
}

static void
simple_action_symbol_write(void *c, const FklSymbolTable *pst, FILE *fp) {
    struct SimpleActionSymbolCtx *ctx = c;
    fwrite(&ctx->nth, sizeof(ctx->nth), 1, fp);
    if (ctx->start) {
        fwrite(ctx->start, sizeof(ctx->start->size) + ctx->start->size, 1, fp);
    } else {
        uint64_t len = 0;
        fwrite(&len, sizeof(len), 1, fp);
    }
    if (ctx->end) {
        fwrite(ctx->end, sizeof(ctx->end->size) + ctx->end->size, 1, fp);
    } else {
        uint64_t len = 0;
        fwrite(&len, sizeof(len), 1, fp);
    }
}

static void
simple_action_symbol_print(void *c, const FklSymbolTable *pst, FILE *fp) {
    struct SimpleActionSymbolCtx *ctx = c;
    fprintf(fp, "%" PRIu64, ctx->nth);
    if (ctx->start) {
        fputs(", ", fp);
        fklPrintRawString(ctx->start, fp);
    }
    if (ctx->end) {
        fputs(", ", fp);
        fklPrintRawString(ctx->end, fp);
    }
}
static void *simple_action_symbol_read(FklSymbolTable *pst, FILE *fp) {
    struct SimpleActionSymbolCtx *ctx =
            (struct SimpleActionSymbolCtx *)fklZmalloc(
                    sizeof(struct SimpleActionSymbolCtx));
    FKL_ASSERT(ctx);
    fread(&ctx->nth, sizeof(ctx->nth), 1, fp);

    {
        uint64_t len = 0;
        fread(&len, sizeof(len), 1, fp);
        if (len == 0)
            ctx->start = NULL;
        else {
            ctx->start = fklCreateString(len, NULL);
            fread(ctx->start->str, len, 1, fp);
        }
    }
    {
        uint64_t len = 0;
        fread(&len, sizeof(len), 1, fp);
        if (len == 0)
            ctx->end = NULL;
        else {
            ctx->end = fklCreateString(len, NULL);
            fread(ctx->end->str, len, 1, fp);
        }
    }

    return ctx;
}

static void *simple_action_symbol_copy(const void *cc) {
    struct SimpleActionSymbolCtx *ctx =
            (struct SimpleActionSymbolCtx *)fklZmalloc(
                    sizeof(struct SimpleActionSymbolCtx));
    FKL_ASSERT(ctx);
    *ctx = *(struct SimpleActionSymbolCtx *)cc;
    ctx->end = fklCopyString(ctx->end);
    ctx->start = fklCopyString(ctx->start);
    return ctx;
}

static void simple_action_symbol_destroy(void *cc) {
    struct SimpleActionSymbolCtx *ctx = (struct SimpleActionSymbolCtx *)cc;
    fklZfree(ctx->end);
    fklZfree(ctx->start);
    fklZfree(ctx);
}

static void *simple_action_symbol(void *c,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    struct SimpleActionSymbolCtx *ctx = (struct SimpleActionSymbolCtx *)c;
    if (ctx->nth >= num)
        return NULL;
    FklNastNode *node = nodes[ctx->nth].ast;
    if (node->type != FKL_NAST_STR)
        return NULL;
    FklNastNode *sym = NULL;
    if (ctx->start) {
        const char *start = ctx->start->str;
        size_t start_size = ctx->start->size;
        const char *end = ctx->end->str;
        size_t end_size = ctx->end->size;

        const FklString *str = node->str;
        const char *cstr = str->str;
        size_t cstr_size = str->size;

        FklStringBuffer buffer;
        fklInitStringBuffer(&buffer);
        const char *end_cstr = cstr + str->size;
        while (cstr < end_cstr) {
            if (fklCharBufMatch(start, start_size, cstr, cstr_size) >= 0) {
                cstr += start_size;
                cstr_size -= start_size;
                size_t len =
                        fklQuotedCharBufMatch(cstr, cstr_size, end, end_size);
                if (!len)
                    return 0;
                size_t size = 0;
                char *s = fklCastEscapeCharBuf(cstr, len - end_size, &size);
                fklStringBufferBincpy(&buffer, s, size);
                fklZfree(s);
                cstr += len;
                cstr_size -= len;
                continue;
            }
            size_t len = 0;
            for (; (cstr + len) < end_cstr; len++)
                if (fklCharBufMatch(start,
                            start_size,
                            cstr + len,
                            cstr_size - len)
                        >= 0)
                    break;
            fklStringBufferBincpy(&buffer, cstr, len);
            cstr += len;
            cstr_size -= len;
        }
        FklSid_t id =
                fklAddSymbolCharBuf(buffer.buf, buffer.index, outerCtx)->v;
        sym = fklCreateNastNode(FKL_NAST_SYM, node->curline);
        sym->sym = id;
        fklUninitStringBuffer(&buffer);
    } else {
        FklNastNode *sym = fklCreateNastNode(FKL_NAST_SYM, node->curline);
        sym->sym = fklAddSymbol(node->str, outerCtx)->v;
    }
    return sym;
}

static inline void *simple_action_string(void *c,
        void *outerCtx,
        const FklAnalysisSymbol nodes[],
        size_t num,
        size_t line) {
    struct SimpleActionSymbolCtx *ctx = (struct SimpleActionSymbolCtx *)c;
    if (ctx->nth >= num)
        return NULL;
    FklNastNode *node = nodes[ctx->nth].ast;
    if (node->type != FKL_NAST_STR)
        return NULL;
    if (ctx->start) {
        size_t start_size = ctx->start->size;
        size_t end_size = ctx->end->size;

        const FklString *str = node->str;
        const char *cstr = str->str;

        size_t size = 0;
        char *s = fklCastEscapeCharBuf(&cstr[start_size],
                str->size - end_size - start_size,
                &size);
        FklNastNode *retval = fklCreateNastNode(FKL_NAST_STR, node->curline);
        retval->str = fklCreateString(size, s);
        fklZfree(s);
        return retval;
    } else
        return fklMakeNastNodeRef(node);
}

static struct FklSimpleProdAction
        CodegenProdCreatorActions[FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM] = {
            {
                "nth",
                simple_action_nth,
                simple_action_nth_create,
                fklProdCtxCopyerDoNothing,
                fklProdCtxDestroyDoNothing,
                simple_action_nth_write,
                simple_action_nth_reader,
                simple_action_nth_print,
            },
            {
                "cons",
                simple_action_cons,
                simple_action_cons_create,
                simple_action_cons_copy,
                fklProdCtxDestroyFree,
                simple_action_cons_write,
                simple_action_cons_read,
                simple_action_cons_print,
            },
            {
                "head",
                simple_action_head,
                simple_action_head_create,
                simple_action_head_copy,
                simple_action_head_destroy,
                simple_action_head_write,
                simple_action_head_read,
                simple_action_head_print,
            },
            {
                "symbol",
                simple_action_symbol,
                simple_action_symbol_create,
                simple_action_symbol_copy,
                simple_action_symbol_destroy,
                simple_action_symbol_write,
                simple_action_symbol_read,
                simple_action_symbol_print,
            },
            {
                "string",
                simple_action_string,
                simple_action_symbol_create,
                simple_action_symbol_copy,
                simple_action_symbol_destroy,
                simple_action_symbol_write,
                simple_action_symbol_read,
                simple_action_symbol_print,
            },
            {
                "box",
                simple_action_box,
                simple_action_nth_create,
                fklProdCtxCopyerDoNothing,
                fklProdCtxDestroyDoNothing,
                simple_action_nth_write,
                simple_action_nth_reader,
                simple_action_nth_print,
            },
            {
                "vector",
                simple_action_vector,
                simple_action_nth_create,
                fklProdCtxCopyerDoNothing,
                fklProdCtxDestroyDoNothing,
                simple_action_nth_write,
                simple_action_nth_reader,
                simple_action_nth_print,
            },
            {
                "hasheq",
                simple_action_hasheq,
                simple_action_nth_create,
                fklProdCtxCopyerDoNothing,
                fklProdCtxDestroyDoNothing,
                simple_action_nth_write,
                simple_action_nth_reader,
                simple_action_nth_print,
            },
            {
                "hasheqv",
                simple_action_hasheqv,
                simple_action_nth_create,
                fklProdCtxCopyerDoNothing,
                fklProdCtxDestroyDoNothing,
                simple_action_nth_write,
                simple_action_nth_reader,
                simple_action_nth_print,
            },
            {
                "hashequal",
                simple_action_hashequal,
                simple_action_nth_create,
                fklProdCtxCopyerDoNothing,
                fklProdCtxDestroyDoNothing,
                simple_action_nth_write,
                simple_action_nth_reader,
                simple_action_nth_print,
            },
            {
                "bytes",
                simple_action_bytevector,
                simple_action_nth_create,
                fklProdCtxCopyerDoNothing,
                fklProdCtxDestroyDoNothing,
                simple_action_nth_write,
                simple_action_nth_reader,
                simple_action_nth_print,
            },
        };

static inline int is_simple_action(FklProdActionFunc func) {
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++) {
        if (CodegenProdCreatorActions[i].func == func)
            return 1;
    }
    return 0;
}

static inline void init_simple_prod_action_list(FklSid_t *simple_prod_action_id,
        FklSymbolTable *pst) {
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++)
        simple_prod_action_id[i] =
                fklAddSymbolCstr(CodegenProdCreatorActions[i].name, pst)->v;
}

static inline struct FklSimpleProdAction *find_simple_prod_action_name(
        FklProdActionFunc func) {
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++) {
        if (CodegenProdCreatorActions[i].func == func)
            return &CodegenProdCreatorActions[i];
    }
    return NULL;
}

static inline struct FklSimpleProdAction *find_simple_prod_action(FklSid_t id,
        FklSid_t *simple_prod_action_id) {
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++) {
        if (simple_prod_action_id[i] == id)
            return &CodegenProdCreatorActions[i];
    }
    return NULL;
}

const FklSimpleProdAction *fklFindSimpleProdActionByName(const char *str) {
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++) {
        if (strcmp(CodegenProdCreatorActions[i].name, str) == 0)
            return &CodegenProdCreatorActions[i];
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
        const FklNastVector *vec,
        FklGrammerSym *s,
        FklGrammer *g) {
    if (vec->size == 0)
        return NAST_TO_GRAMMER_SYM_ERR_INVALID;

    const FklNastNode *first = vec->base[0];
    if (first->type != FKL_NAST_SYM)
        return NAST_TO_GRAMMER_SYM_ERR_INVALID;

    const FklString *builtin_term_name = fklGetSymbolWithId(first->sym, g->st);
    if (fklCharBufMatch(FKL_PG_SPECIAL_PREFIX,
                sizeof(FKL_PG_SPECIAL_PREFIX) - 1,
                builtin_term_name->str,
                builtin_term_name->size)
            == sizeof(FKL_PG_SPECIAL_PREFIX) - 1) {

        const FklLalrBuiltinMatch *builtin_terminal =
                fklGetBuiltinMatch(&g->builtins, first->sym);
        if (builtin_terminal == NULL)
            return NAST_TO_GRAMMER_SYM_ERR_UNRESOLVED_BUILTIN;

        for (size_t i = 1; i < vec->size; ++i) {
            if (vec->base[i]->type != FKL_NAST_STR)
                return NAST_TO_GRAMMER_SYM_ERR_INVALID;
        }

        FklString const **args =
                fklZmalloc((vec->size - 1) * sizeof(FklString *));

        for (size_t i = 1; i < vec->size; ++i)
            args[i - 1] = fklAddString(&g->terminals, vec->base[i]->str);

        s->type = FKL_TERM_BUILTIN;
        s->b.t = builtin_terminal;
        s->b.len = 0;
        s->b.args = NULL;

        FklBuiltinTerminalInitError err = FKL_BUILTIN_TERMINAL_INIT_ERR_DUMMY;
        if (s->b.t->ctx_create)
            err = s->b.t->ctx_create(vec->size - 1, args, g);
        if (err) {
            fklZfree(args);
            return NAST_TO_GRAMMER_SYM_ERR_BUILTIN_TERMINAL_INIT_FAILED
                 | (err << CHAR_BIT);
        }
        s->b.len = vec->size - 1;
        s->b.args = args;

        return NAST_TO_GRAMMER_SYM_ERR_DUMMY;
    } else {
        return NAST_TO_GRAMMER_SYM_ERR_INVALID;
    }
}

static inline NastToGrammerSymErr nast_node_to_grammer_sym(
        const FklNastNode *node,
        FklGrammerSym *s,
        FklGrammer *g) {
    switch (node->type) {
    case FKL_NAST_VECTOR:
        return nast_vector_to_builtin_terminal(node->vec, s, g);
        break;
    case FKL_NAST_BYTEVECTOR:
        s->type = FKL_TERM_KEYWORD;
        s->str = fklAddStringCharBuf(&g->terminals,
                FKL_TYPE_CAST(const char *, node->bvec->ptr),
                node->bvec->size);
        break;
    case FKL_NAST_BOX:
        if (node->box->type != FKL_NAST_STR)
            return NAST_TO_GRAMMER_SYM_ERR_INVALID;
        s->type = FKL_TERM_REGEX;
        s->re = fklAddRegexStr(&g->regexes, node->box->str);
        if (s->re == NULL)
            return NAST_TO_GRAMMER_SYM_ERR_REGEX_COMPILE_FAILED;
        break;
    case FKL_NAST_STR:
        s->type = FKL_TERM_STRING;
        s->str = fklAddString(&g->terminals, node->str);
        fklAddString(&g->delimiters, node->str);
        break;
    case FKL_NAST_PAIR:
        s->type = FKL_TERM_NONTERM;
        s->nt.group = node->pair->car->sym;
        s->nt.sid = node->pair->cdr->sym;
        break;
    case FKL_NAST_SYM: {
        const FklString *str = fklGetSymbolWithId(node->sym, g->st);
        if (fklCharBufMatch(FKL_PG_SPECIAL_PREFIX,
                    sizeof(FKL_PG_SPECIAL_PREFIX) - 1,
                    str->str,
                    str->size)
                == sizeof(FKL_PG_SPECIAL_PREFIX) - 1) {
            const FklLalrBuiltinMatch *builtin_terminal =
                    fklGetBuiltinMatch(&g->builtins, node->sym);
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
            s->nt.sid = node->sym;
        }
    } break;

    case FKL_NAST_NIL:
    case FKL_NAST_FIX:
    case FKL_NAST_F64:
    case FKL_NAST_BIGINT:
    case FKL_NAST_CHR:
    case FKL_NAST_SLOT:
    case FKL_NAST_RC_SYM:
    case FKL_NAST_HASHTABLE:
        return NAST_TO_GRAMMER_SYM_ERR_INVALID;
        break;
    }
    return NAST_TO_GRAMMER_SYM_ERR_DUMMY;
}

static inline int is_concat_sym(const FklString *str) {
    return fklStringCstrCmp(str, FKL_PG_TERM_CONCAT) == 0;
}

typedef struct {
    const FklNastNode *err_node;
    FklGrammer *g;
    FklGrammerSym *syms;
    size_t len;
    int adding_ignore;
} NastToGrammerSymArgs;

static inline NastToGrammerSymErr nast_vector_to_production_right_part(
        NastToGrammerSymArgs *args,
        const FklNastVector *vec) {
    if (vec->size == 0) {
        args->len = 0;
        args->syms = NULL;
        return NAST_TO_GRAMMER_SYM_ERR_DUMMY;
    }

    FklGrammer *g = args->g;
    NastToGrammerSymErr err = NAST_TO_GRAMMER_SYM_ERR_DUMMY;
    FklGraSymVector gsym_vector;
    fklGraSymVectorInit(&gsym_vector, 2);

    int has_ignore = 0;
    for (size_t i = 0; i < vec->size; ++i) {
        FklGrammerSym s = { .type = FKL_TERM_STRING };
        const FklNastNode *cur = vec->base[i];
        if (cur->type == FKL_NAST_SYM
                && is_concat_sym(fklGetSymbolWithId(cur->sym, g->st))) {
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

static inline FklGrammerIgnore *nast_vector_to_ignore(const FklNastVector *vec,
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

static inline FklCodegenInfo *macro_compile_prepare(FklCodegenInfo *codegen,
        FklCodegenMacroScope *macroScope,
        FklSidHashSet *symbolSet,
        FklCodegenEnv **pmacroEnv,
        FklSymbolTable *pst,
        FklConstTable *pkt) {
    FklCodegenEnv *macro_glob_env = fklCreateCodegenEnv(NULL, 0, macroScope);
    fklInitGlobCodegenEnv(macro_glob_env, pst);

    FklCodegenInfo *macroCodegen =
            createCodegenInfo(codegen, NULL, 0, 1, codegen->outer_ctx);
    codegen->global_env->refcount--;
    macroCodegen->global_env = macro_glob_env;
    macro_glob_env->refcount++;

    fklZfree(macroCodegen->dir);
    macroCodegen->dir = fklZstrdup(codegen->dir);
    macroCodegen->filename = fklZstrdup(codegen->filename);
    macroCodegen->realpath = fklZstrdup(codegen->realpath);
    macroCodegen->pts = codegen->macro_pts;
    macroCodegen->macro_pts = fklCreateFuncPrototypes(0);

    macroCodegen->runtime_symbol_table = pst;
    macroCodegen->runtime_kt = pkt;
    macroCodegen->fid = macroCodegen->filename
                              ? fklAddSymbolCstr(macroCodegen->filename, pst)->v
                              : 0;
    macroCodegen->libraries = macroCodegen->macro_libraries;
    macroCodegen->macro_libraries = fklCodegenLibVectorCreate(8);

    FklCodegenEnv *macro_main_env =
            fklCreateCodegenEnv(macro_glob_env, 1, macroScope);

    for (FklSidHashSetNode *list = symbolSet->first; list; list = list->next) {
        FklSid_t id = list->k;
        fklAddCodegenDefBySid(id, 1, macro_main_env);
    }
    *pmacroEnv = macro_main_env;
    return macroCodegen;
}

typedef struct {
    FklGrammerProduction *prod;
    FklSid_t sid;
    FklSid_t group_id;
    FklNastNode *vec;
    uint8_t add_extra;
    uint8_t type;
    FklNastNode *action;
} AddingProductionCtx;

static void _adding_production_ctx_finalizer(void *data) {
    AddingProductionCtx *ctx = data;
    if (ctx->prod)
        fklDestroyGrammerProduction(ctx->prod);
    fklDestroyNastNode(ctx->vec);
    if (ctx->type != FKL_CODEGEN_PROD_CUSTOM)
        fklDestroyNastNode(ctx->action);
    fklZfree(data);
}

static const FklCodegenQuestContextMethodTable
        AddingProductionCtxMethodTable = {
            .__finalizer = _adding_production_ctx_finalizer,
            .__get_bcl_stack = NULL,
            .__put_bcl = NULL,
        };

static inline FklCodegenQuestContext *createAddingProductionCtx(
        FklGrammerProduction *prod,
        FklSid_t sid,
        FklSid_t group_id,
        uint8_t add_extra,
        const FklNastNode *vec,
        FklCodegenProdActionType type,
        const FklNastNode *action_ast) {
    AddingProductionCtx *ctx =
            (AddingProductionCtx *)fklZmalloc(sizeof(AddingProductionCtx));
    FKL_ASSERT(ctx);
    ctx->prod = prod;
    ctx->sid = sid;
    ctx->group_id = group_id;
    ctx->add_extra = add_extra;
    ctx->vec = fklMakeNastNodeRef(FKL_REMOVE_CONST(FklNastNode, vec));
    ctx->type = type;
    if (type != FKL_CODEGEN_PROD_CUSTOM)
        ctx->action =
                fklMakeNastNodeRef(FKL_REMOVE_CONST(FklNastNode, action_ast));
    return createCodegenQuestContext(ctx, &AddingProductionCtxMethodTable);
}

static inline int check_group_outer_ref(FklCodegenInfo *codegen,
        const FklProdHashMap *productions,
        FklSid_t group_id) {
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

FklGrammerProduction *fklCreateExtraStartProduction(
        FklCodegenOuterCtx *outer_ctx,
        FklSid_t group,
        FklSid_t sid) {
    FklGrammerProduction *prod =
            fklCreateEmptyProduction(outer_ctx->builtin_g.start.group,
                    outer_ctx->builtin_g.start.sid,
                    1,
                    NULL,
                    NULL,
                    NULL,
                    fklProdCtxDestroyDoNothing,
                    fklProdCtxCopyerDoNothing);
    prod->func = codegen_prod_action_first;
    prod->idx = 0;

    FklGrammerSym *u = &prod->syms[0];
    u->type = FKL_TERM_NONTERM;
    u->nt.group = group;
    u->nt.sid = sid;
    return prod;
}

BC_PROCESS(process_adding_production) {
    AddingProductionCtx *ctx = context->data;
    FklSid_t group_id = ctx->group_id;
    FklGrammerProduction *prod = ctx->prod;
    ctx->prod = NULL;
    FklGrammer *g = group_id == 0
                          ? codegen->unnamed_g
                          : &add_production_group(codegen->named_prod_groups,
                                    &codegen->outer_ctx->public_symbol_table,
                                    group_id)
                                     ->g;
    FklSid_t sid = ctx->sid;
    if (group_id == 0) {
        if (fklAddProdToProdTableNoRepeat(g, prod)) {
            fklDestroyGrammerProduction(prod);
        reader_macro_error:
            errorState->type = FKL_ERR_GRAMMER_CREATE_FAILED;
            errorState->line = line;
            return NULL;
        }
        if (ctx->add_extra) {
            FklGrammerProduction *extra_prod =
                    fklCreateExtraStartProduction(codegen->outer_ctx,
                            group_id,
                            sid);
            if (fklAddProdToProdTable(g, extra_prod)) {
                fklDestroyGrammerProduction(extra_prod);
                goto reader_macro_error;
            }
        }
    } else {
        FklGrammerProdGroupItem *group =
                add_production_group(codegen->named_prod_groups,
                        &codegen->outer_ctx->public_symbol_table,
                        group_id);
        if (fklAddProdToProdTableNoRepeat(&group->g, prod)) {
            fklDestroyGrammerProduction(prod);
            goto reader_macro_error;
        }
        if (ctx->add_extra) {
            FklGrammerProduction *extra_prod =
                    fklCreateExtraStartProduction(codegen->outer_ctx,
                            group_id,
                            sid);
            if (fklAddProdToProdTable(&group->g, extra_prod)) {
                fklDestroyGrammerProduction(extra_prod);
                goto reader_macro_error;
            }
        }
        group->is_ref_outer =
                check_group_outer_ref(codegen, &group->g.productions, group_id);
    }
    if (codegen->export_named_prod_groups
            && fklSidHashSetHas2(codegen->export_named_prod_groups, group_id)
            && fklGraProdGroupHashMapGet2(codegen->named_prod_groups, group_id)
                    ->is_ref_outer) {
        errorState->type = FKL_ERR_EXPORT_OUTER_REF_PROD_GROUP;
        errorState->line = line;
        errorState->place = NULL;
        return NULL;
    }
    return NULL;
}

typedef struct {
    uint32_t line;
    uint8_t add_extra;
    FklSid_t left_group;
    FklSid_t left_sid;
    FklSid_t action_type;
    FklNastNode *action_ast;
    FklSid_t group_id;
    FklCodegenInfo *codegen;
    FklCodegenEnv *curEnv;
    FklCodegenMacroScope *macroScope;
    FklCodegenQuestVector *codegenQuestStack;
    FklSymbolTable *pst;
    FklGrammer *g;

    const FklNastNode *err_node;
    FklGrammerProduction *production;
} NastToProductionArgs;

static inline NastToGrammerSymErr nast_vector_to_production(
        const FklNastNode *vec_node,
        NastToProductionArgs *args) {
    const FklNastVector *vec = vec_node->vec;

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

    FklSid_t action_type = args->action_type;
    FklCodegenInfo *codegen = args->codegen;
    const FklNastNode *action_ast = args->action_ast;
    FklSid_t left_sid = args->left_sid;
    FklSid_t left_group = args->left_group;
    FklSid_t group_id = args->group_id;
    uint8_t add_extra = args->add_extra;

    if (action_type == codegen->outer_ctx->builtInPatternVar_builtin) {
        if (action_ast->type != FKL_NAST_SYM) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }
        FklProdActionFunc action = find_builtin_prod_action(action_ast->sym,
                codegen->outer_ctx->builtin_prod_action_id);
        if (!action) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }
        FklGrammerProduction *prod = fklCreateProduction(left_group,
                left_sid,
                other_args.len,
                other_args.syms,
                NULL,
                action,
                NULL,
                fklProdCtxDestroyDoNothing,
                fklProdCtxCopyerDoNothing);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(process_adding_production,
                createAddingProductionCtx(prod,
                        left_sid,
                        group_id,
                        add_extra,
                        vec_node,
                        FKL_CODEGEN_PROD_BUILTIN,
                        action_ast),
                NULL,
                1,
                args->macroScope,
                args->curEnv,
                args->line,
                codegen,
                args->codegenQuestStack);
        args->production = prod;
    } else if (action_type == codegen->outer_ctx->builtInPatternVar_simple) {
        if (action_ast->type != FKL_NAST_VECTOR || !action_ast->vec->size
                || action_ast->vec->base[0]->type != FKL_NAST_SYM) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }
        struct FklSimpleProdAction *action =
                find_simple_prod_action(action_ast->vec->base[0]->sym,
                        codegen->outer_ctx->simple_prod_action_id);
        if (!action) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }
        int failed = 0;
        void *ctx = action->creator(&action_ast->vec->base[1],
                action_ast->vec->size - 1,
                &failed);
        if (failed) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }
        FklGrammerProduction *prod = fklCreateProduction(left_group,
                left_sid,
                other_args.len,
                other_args.syms,
                NULL,
                action->func,
                ctx,
                action->ctx_destroy,
                action->ctx_copyer);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(process_adding_production,
                createAddingProductionCtx(prod,
                        left_sid,
                        group_id,
                        add_extra,
                        vec_node,
                        FKL_CODEGEN_PROD_SIMPLE,
                        action_ast),
                NULL,
                1,
                args->macroScope,
                args->curEnv,
                args->line,
                codegen,
                args->codegenQuestStack);
        args->production = prod;
    } else if (action_type == codegen->outer_ctx->builtInPatternVar_custom) {
        FklSidHashSet symbolSet;
        fklSidHashSetInit(&symbolSet);
        fklSidHashSetPut2(&symbolSet, fklAddSymbolCstr("$$", args->pst)->v);
        FklCodegenEnv *macroEnv = NULL;
        FklCodegenInfo *macroCodegen = macro_compile_prepare(codegen,
                args->macroScope,
                &symbolSet,
                &macroEnv,
                args->pst,
                &codegen->outer_ctx->public_kt);
        fklSidHashSetUninit(&symbolSet);

        create_and_insert_to_pool(macroCodegen,
                0,
                macroEnv,
                0,
                action_ast->curline,
                args->pst);
        FklNastNodeQueue *queue = fklNastNodeQueueCreate();
        struct CustomActionCtx *ctx = (struct CustomActionCtx *)fklZcalloc(1,
                sizeof(struct CustomActionCtx));
        FKL_ASSERT(ctx);
        ctx->codegen_outer_ctx = codegen->outer_ctx;
        ctx->prototype_id = macroEnv->prototypeId;
        ctx->pst = args->pst;
        ctx->macro_libraries = codegen->macro_libraries;

        FklGrammerProduction *prod = fklCreateProduction(left_group,
                left_sid,
                other_args.len,
                other_args.syms,
                NULL,
                custom_action,
                ctx,
                custom_action_ctx_destroy,
                custom_action_ctx_copy);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(process_adding_production,
                createAddingProductionCtx(prod,
                        left_sid,
                        group_id,
                        add_extra,
                        vec_node,
                        FKL_CODEGEN_PROD_CUSTOM,
                        NULL),
                NULL,
                1,
                args->macroScope,
                args->curEnv,
                args->line,
                codegen,
                args->codegenQuestStack);
        fklNastNodeQueuePush2(queue,
                fklMakeNastNodeRef(FKL_REMOVE_CONST(FklNastNode, action_ast)));
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_reader_macro_bc_process,
                createReaderMacroQuestContext(ctx, macroCodegen->pts),
                createMustHasRetvalQueueNextExpression(queue),
                1,
                macroEnv->macros,
                macroEnv,
                action_ast->curline,
                macroCodegen,
                args->codegenQuestStack);
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

static inline FklGrammerProdGroupItem *
get_production_group(const FklGraProdGroupHashMap *ht, FklSid_t group) {
    return fklGraProdGroupHashMapGet2(ht, group);
}

static inline int process_add_production(FklSid_t group_id,
        FklCodegenInfo *codegen,
        FklNastNode *vector_node,
        FklCodegenErrorState *errorState,
        FklCodegenEnv *curEnv,
        FklCodegenMacroScope *macroScope,
        FklCodegenQuestVector *codegenQuestStack,
        FklSymbolTable *pst) {
    FklGrammer *g = group_id == 0
                          ? codegen->unnamed_g
                          : &add_production_group(codegen->named_prod_groups,
                                    &codegen->outer_ctx->public_symbol_table,
                                    group_id)
                                     ->g;
    if (vector_node->type == FKL_NAST_STR) {
        fklAddString(&g->terminals, vector_node->str);
        fklAddString(&g->delimiters, vector_node->str);
        if (group_id != 0) {
            FklGrammerProdGroupItem *group =
                    add_production_group(codegen->named_prod_groups,
                            pst,
                            group_id);
            fklAddString(&group->delimiters, vector_node->str);
        }
        return 0;
    }

    if (vector_node->type == FKL_NAST_PAIR) {
        for (const FklNastNode *cur = vector_node; cur->type == FKL_NAST_PAIR;
                cur = cur->pair->cdr) {
            FKL_ASSERT(cur->pair->car->type == FKL_NAST_STR);
            fklAddString(&g->terminals, cur->pair->car->str);
            fklAddString(&g->delimiters, cur->pair->car->str);
        }

        return 0;
    }

    if (vector_node->type == FKL_NAST_BOX) {
        FklNastNode *ignore_obj = vector_node->box;
        FKL_ASSERT(ignore_obj->type == FKL_NAST_VECTOR);

        NastToGrammerSymArgs args = { .g = g };
        NastToGrammerSymErr err = 0;
        FklGrammerIgnore *ignore =
                nast_vector_to_ignore(ignore_obj->vec, &args, &err);
        if (err) {
            errorState->type = FKL_ERR_GRAMMER_CREATE_FAILED;
            errorState->place = fklMakeNastNodeRef(
                    FKL_REMOVE_CONST(FklNastNode, args.err_node));
            errorState->msg = get_nast_to_grammer_sym_err_msg(err);
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
                            pst,
                            group_id);
            if (fklAddIgnoreToIgnoreList(&group->g.ignores, ignore)) {
                fklDestroyIgnore(ignore);
                return 0;
            }
        }
        return 0;
    }

    FklNastNode *error_node = NULL;
    FklNastVector *vec = vector_node->vec;
    if (vec->size == 4) {

        FklNastNode **base = vec->base;

        if (base[2]->type != FKL_NAST_SYM) {
            error_node = base[2];
        reader_macro_syntax_error:
            errorState->type = FKL_ERR_SYNTAXERROR;
            errorState->place = fklMakeNastNodeRef(error_node);
            return 1;
        }

        FklNastNode *vect = NULL;
        NastToProductionArgs args = {
            .line = vector_node->curline,
            .add_extra = 1,
            .action_type = base[2]->sym,
            .action_ast = base[3],
            .group_id = group_id,
            .codegen = codegen,
            .curEnv = curEnv,
            .macroScope = macroScope,
            .codegenQuestStack = codegenQuestStack,
            .pst = pst,
            .g = g,
        };
        if (base[0]->type == FKL_NAST_NIL && base[1]->type == FKL_NAST_VECTOR) {
            vect = base[1];
            args.left_group = codegen->outer_ctx->builtin_g.start.group;
            args.left_sid = codegen->outer_ctx->builtin_g.start.sid;
            args.add_extra = 0;
        } else if (base[0]->type == FKL_NAST_SYM
                   && base[1]->type == FKL_NAST_VECTOR) {
            vect = base[1];
            FklSid_t sid = base[0]->sym;
            if (group_id == 0
                    && (fklGraSidBuiltinHashMapGet2(&g->builtins, sid)
                            || fklIsNonterminalExist(
                                    &codegen->outer_ctx->builtin_g.productions,
                                    group_id,
                                    sid))) {
                error_node = base[0];
                goto reader_macro_syntax_error;
            }
            args.left_group = group_id;
            args.left_sid = sid;
        } else if (base[0]->type == FKL_NAST_VECTOR
                   && base[1]->type == FKL_NAST_SYM) {
            vect = base[0];
            FklSid_t sid = base[1]->sym;
            if (group_id == 0
                    && (fklGraSidBuiltinHashMapGet2(&g->builtins, sid)
                            || fklIsNonterminalExist(
                                    &codegen->outer_ctx->builtin_g.productions,
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
            errorState->type = FKL_ERR_GRAMMER_CREATE_FAILED;
            errorState->place = fklMakeNastNodeRef(
                    FKL_REMOVE_CONST(FklNastNode, args.err_node));
            errorState->msg = get_nast_to_grammer_sym_err_msg(err);
            return 1;
        }
    } else {
        error_node = vector_node;
        goto reader_macro_syntax_error;
    }
    return 0;
}

BC_PROCESS(update_grammer) {
    if (add_all_group_to_grammer(line, codegen, errorState)) {
        errorState->type = FKL_ERR_GRAMMER_CREATE_FAILED;
        errorState->line = line;
        return NULL;
    }
    return NULL;
}

static inline int is_valid_production_rule_node(const FklNastNode *n) {
    return n->type == FKL_NAST_VECTOR || n->type == FKL_NAST_STR
        || (n->type == FKL_NAST_BOX && n->box->type == FKL_NAST_VECTOR)
        || fklIsNastNodeListAndHasSameType(n, FKL_NAST_STR);
}

static CODEGEN_FUNC(codegen_defmacro) {
    if (must_has_retval) {
        errorState->type = FKL_ERR_EXP_HAS_NO_VALUE;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *value =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    if (name->type == FKL_NAST_SYM)
        fklReplacementHashMapAdd2(macroScope->replacements, name->sym, value);
    else if (name->type == FKL_NAST_PAIR) {
        FklSidHashSet *symbolSet = NULL;
        FklNastNode *pattern = fklCreatePatternFromNast(name, &symbolSet);
        if (!pattern) {
            errorState->type = FKL_ERR_INVALID_MACRO_PATTERN;
            errorState->place = fklMakeNastNodeRef(name);
            return;
        }
        if (fklSidHashSetPut2(symbolSet, outer_ctx->builtInPatternVar_orig)) {
            fklDestroyNastNode(pattern);
            errorState->type = FKL_ERR_INVALID_MACRO_PATTERN;
            errorState->place = fklMakeNastNodeRef(name);
            return;
        }

        FklCodegenEnv *macroEnv = NULL;
        FklCodegenInfo *macroCodegen = macro_compile_prepare(codegen,
                macroScope,
                symbolSet,
                &macroEnv,
                pst,
                &codegen->outer_ctx->public_kt);
        fklSidHashSetDestroy(symbolSet);

        create_and_insert_to_pool(macroCodegen,
                0,
                macroEnv,
                0,
                value->curline,
                pst);
        FklNastNodeQueue *queue = fklNastNodeQueueCreate();
        fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(value));
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(_compiler_macro_bc_process,
                createMacroQuestContext(name,
                        pattern,
                        macroScope,
                        macroEnv->prototypeId),
                createMustHasRetvalQueueNextExpression(queue),
                1,
                macroEnv->macros,
                macroEnv,
                value->curline,
                macroCodegen,
                codegenQuestStack);
    } else if (name->type == FKL_NAST_BOX) {
        FklNastNode *group_node = name->box;
        if (!is_valid_production_rule_node(value)) {
            errorState->type = FKL_ERR_SYNTAXERROR;
            errorState->place = fklMakeNastNodeRef(value);
            return;
        }

        if (group_node->type != FKL_NAST_SYM
                && group_node->type != FKL_NAST_NIL) {
            errorState->type = FKL_ERR_SYNTAXERROR;
            errorState->place = fklMakeNastNodeRef(group_node);
            return;
        }
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(update_grammer,
                createEmptyContext(),
                NULL,
                scope,
                macroScope,
                curEnv,
                origExp->curline,
                codegen,
                codegenQuestStack);
        init_builtin_grammer_and_prod_group(codegen);

        FklSid_t group_id =
                group_node->type == FKL_NAST_NIL ? 0 : group_node->sym;

        if (process_add_production(group_id,
                    codegen,
                    value,
                    errorState,
                    curEnv,
                    macroScope,
                    codegenQuestStack,
                    pst))
            return;
    }
}

static CODEGEN_FUNC(codegen_def_reader_macros) {
    if (must_has_retval) {
        errorState->type = FKL_ERR_EXP_HAS_NO_VALUE;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklNastNodeQueue prod_vector_queue;
    fklNastNodeQueueInit(&prod_vector_queue);
    FklNastNode *name =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_arg0);
    FklNastNode *err_node = NULL;
    if (name->type != FKL_NAST_BOX) {
        err_node = name;
        goto reader_macro_error;
    }
    FklNastNode *arg =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_arg1);
    if (!is_valid_production_rule_node(arg)) {
        err_node = arg;
        goto reader_macro_error;
    }
    fklNastNodeQueuePush2(&prod_vector_queue, arg);
    FklNastNode *rest =
            *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);

    for (; rest->type != FKL_NAST_NIL; rest = rest->pair->cdr) {
        if (!is_valid_production_rule_node(rest->pair->car)) {
            err_node = rest->pair->car;
            goto reader_macro_error;
        }
        fklNastNodeQueuePush2(&prod_vector_queue, rest->pair->car);
    }

    FklNastNode *group_node = name->box;
    if (group_node->type != FKL_NAST_NIL && group_node->type != FKL_NAST_SYM) {
        err_node = group_node;
    reader_macro_error:
        fklNastNodeQueueUninit(&prod_vector_queue);
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(err_node);
        return;
    }
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(update_grammer,
            createEmptyContext(),
            NULL,
            scope,
            macroScope,
            curEnv,
            origExp->curline,
            codegen,
            codegenQuestStack);

    init_builtin_grammer_and_prod_group(codegen);

    FklSid_t group_id = group_node->type == FKL_NAST_SYM ? group_node->sym : 0;

    for (FklNastNodeQueueNode *first = prod_vector_queue.head; first;
            first = first->next)
        if (process_add_production(group_id,
                    codegen,
                    first->data,
                    errorState,
                    curEnv,
                    macroScope,
                    codegenQuestStack,
                    pst)) {
            fklNastNodeQueueUninit(&prod_vector_queue);
            return;
        }
    fklNastNodeQueueUninit(&prod_vector_queue);
}

typedef void (*FklCodegenFunc)(CODEGEN_ARGS);

BC_PROCESS(pre_compile_main_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    FklByteCodelnt *bclnt = sequnce_exp_bc_process(stack, fid, line, scope);
    process_export_bc(codegen, bclnt, fid, line, scope);
    return bclnt;
}

#undef BC_PROCESS
#undef GET_STACK
#undef CODEGEN_ARGS
#undef CODEGEN_FUNC

FklByteCode *fklCodegenNode(const FklNastNode *node, FklCodegenInfo *info) {
    static const FklOpcode hash_opcode_map[][2] = {
        [FKL_HASH_EQ] = { FKL_OP_PUSH_HASHEQ_0, FKL_OP_PUSH_HASHEQ },
        [FKL_HASH_EQV] = { FKL_OP_PUSH_HASHEQV_0, FKL_OP_PUSH_HASHEQV },
        [FKL_HASH_EQUAL] = { FKL_OP_PUSH_HASHEQUAL_0, FKL_OP_PUSH_HASHEQUAL },
    };

    FklNastNodeVector stack;
    fklNastNodeVectorInit(&stack, 32);
    fklNastNodeVectorPushBack2(&stack, FKL_REMOVE_CONST(FklNastNode, node));
    FklByteCode *retval = fklCreateByteCode(0);
    FklSymbolTable *pst = &info->outer_ctx->public_symbol_table;
    while (!fklNastNodeVectorIsEmpty(&stack)) {
        FklNastNode *node = *fklNastNodeVectorPopBackNonNull(&stack);
        if (node == NULL) {
            fklByteCodeInsertFront(create_op_ins(FKL_OP_SET_BP), retval);
            continue;
        }
        switch (node->type) {
        case FKL_NAST_SYM:
            append_push_sym_ins_to_bc(INS_APPEND_FRONT,
                    retval,
                    fklAddSymbol(fklGetSymbolWithId(node->sym, pst),
                            info->runtime_symbol_table)
                            ->v);
            break;
        case FKL_NAST_NIL:
            fklByteCodeInsertFront(create_op_ins(FKL_OP_PUSH_NIL), retval);
            break;
        case FKL_NAST_CHR:
            append_push_chr_ins_to_bc(INS_APPEND_FRONT, retval, node->chr);
            break;
        case FKL_NAST_FIX:
            append_push_i64_ins_to_bc(INS_APPEND_FRONT,
                    retval,
                    node->fix,
                    info);
            break;
        case FKL_NAST_F64:
            append_push_f64_ins_to_bc(INS_APPEND_FRONT,
                    retval,
                    node->f64,
                    info);
            break;
        case FKL_NAST_BYTEVECTOR:
            append_push_bvec_ins_to_bc(INS_APPEND_FRONT,
                    retval,
                    node->bvec,
                    info);
            break;
        case FKL_NAST_STR:
            append_push_str_ins_to_bc(INS_APPEND_FRONT,
                    retval,
                    node->str,
                    info);
            break;
        case FKL_NAST_BIGINT:
            append_push_bigint_ins_to_bc(INS_APPEND_FRONT,
                    retval,
                    node->bigInt,
                    info);
            break;
        case FKL_NAST_PAIR: {
            size_t i = 1;
            FklNastNode *cur = node;
            for (; cur->type == FKL_NAST_PAIR; cur = cur->pair->cdr) {
                fklNastNodeVectorPushBack2(&stack, cur->pair->car);
                i++;
            }
            fklNastNodeVectorPushBack2(&stack, cur);
            if (i == 2)
                fklByteCodeInsertFront(create_op_ins(FKL_OP_PUSH_PAIR), retval);
            else if (i > UINT16_MAX) {
                fklNastNodeVectorInsert2(&stack, stack.size - i, NULL);
                fklByteCodeInsertFront(create_op_ins(FKL_OP_PUSH_LIST_0),
                        retval);
            } else {
                FklInstruction ins = { .op = FKL_OP_PUSH_LIST, .bu = i };
                fklByteCodeInsertFront(ins, retval);
            }
        } break;
        case FKL_NAST_BOX:
            if (node->box->type == FKL_NAST_NIL) {
                fklByteCodeInsertFront(
                        (FklInstruction){ .op = FKL_OP_PUSH_BOX, .ai = 0 },
                        retval);
            } else {
                fklByteCodeInsertFront(
                        (FklInstruction){ .op = FKL_OP_PUSH_BOX, .ai = 1 },
                        retval);
                fklNastNodeVectorPushBack2(&stack, node->box);
            }
            break;
        case FKL_NAST_VECTOR:
            if (node->vec->size > UINT16_MAX) {
                fklNastNodeVectorPushBack2(&stack, NULL);
                fklByteCodeInsertFront(create_op_ins(FKL_OP_PUSH_VEC_0),
                        retval);
            } else {
                FklInstruction ins = { .op = FKL_OP_PUSH_VEC,
                    .bu = node->vec->size };
                fklByteCodeInsertFront(ins, retval);
            }
            for (size_t i = 0; i < node->vec->size; i++)
                fklNastNodeVectorPushBack2(&stack, node->vec->base[i]);
            break;
        case FKL_NAST_HASHTABLE:
            if (node->hash->num > UINT16_MAX) {
                fklNastNodeVectorPushBack2(&stack, NULL);
                fklByteCodeInsertFront(
                        create_op_ins(hash_opcode_map[node->hash->type][0]),
                        retval);
            } else {
                FklInstruction ins = {
                    .op = hash_opcode_map[node->hash->type][1],
                    .bu = node->hash->num
                };
                fklByteCodeInsertFront(ins, retval);
            }
            for (size_t i = 0; i < node->hash->num; i++) {
                fklNastNodeVectorPushBack2(&stack, node->hash->items[i].car);
                fklNastNodeVectorPushBack2(&stack, node->hash->items[i].cdr);
            }
            break;
        default:
            FKL_UNREACHABLE();
            break;
        }
    }
    fklNastNodeVectorUninit(&stack);
    return retval;
}

static inline int matchAndCall(FklCodegenFunc func,
        const FklNastNode *pattern,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        FklNastNode *exp,
        FklCodegenQuestVector *codegenQuestStack,
        FklCodegenEnv *env,
        FklCodegenInfo *codegenr,
        FklCodegenErrorState *errorState,
        uint8_t must_has_retval) {
    FklPmatchHashMap ht;
    fklPmatchHashMapInit(&ht);
    int r = fklPatternMatch(pattern, exp, &ht);
    if (r) {
        fklChdir(codegenr->dir);
        func(exp,
                &ht,
                codegenQuestStack,
                scope,
                macroScope,
                env,
                codegenr,
                codegenr->outer_ctx,
                errorState,
                must_has_retval);
        fklChdir(codegenr->outer_ctx->cwd);
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

void fklInitCodegenOuterCtxExceptPattern(FklCodegenOuterCtx *outerCtx) {
    FklSymbolTable *publicSymbolTable = &outerCtx->public_symbol_table;
    fklInitSymbolTable(publicSymbolTable);
    fklInitConstTable(&outerCtx->public_kt);
    fklInitBuiltinGrammer(&outerCtx->builtin_g, &outerCtx->public_symbol_table);
    outerCtx->gc = NULL;
    outerCtx->ki64_count = 0;
    outerCtx->kf64_count = 0;
    outerCtx->kstr_count = 0;
    outerCtx->kbvec_count = 0;
    outerCtx->kbi_count = 0;
}

void fklInitCodegenOuterCtx(FklCodegenOuterCtx *outerCtx,
        char *main_file_real_path_dir) {
    outerCtx->cwd = fklSysgetcwd();
    outerCtx->main_file_real_path_dir = main_file_real_path_dir
                                              ? main_file_real_path_dir
                                              : fklZstrdup(outerCtx->cwd);
    fklInitCodegenOuterCtxExceptPattern(outerCtx);
    FklSymbolTable *publicSymbolTable = &outerCtx->public_symbol_table;

    outerCtx->builtInPatternVar_orig =
            fklAddSymbolCstr("orig", publicSymbolTable)->v;
    outerCtx->builtInPatternVar_rest =
            fklAddSymbolCstr("rest", publicSymbolTable)->v;
    outerCtx->builtInPatternVar_name =
            fklAddSymbolCstr("name", publicSymbolTable)->v;
    outerCtx->builtInPatternVar_cond =
            fklAddSymbolCstr("cond", publicSymbolTable)->v;
    outerCtx->builtInPatternVar_args =
            fklAddSymbolCstr("args", publicSymbolTable)->v;
    outerCtx->builtInPatternVar_arg0 =
            fklAddSymbolCstr("arg0", publicSymbolTable)->v;
    outerCtx->builtInPatternVar_arg1 =
            fklAddSymbolCstr("arg1", publicSymbolTable)->v;
    outerCtx->builtInPatternVar_value =
            fklAddSymbolCstr("value", publicSymbolTable)->v;

    outerCtx->builtInPatternVar_custom =
            fklAddSymbolCstr("custom", publicSymbolTable)->v;
    outerCtx->builtInPatternVar_builtin =
            fklAddSymbolCstr("builtin", publicSymbolTable)->v;
    outerCtx->builtInPatternVar_simple =
            fklAddSymbolCstr("simple", publicSymbolTable)->v;

    init_builtin_prod_action_list(outerCtx->builtin_prod_action_id,
            publicSymbolTable);
    init_simple_prod_action_list(outerCtx->simple_prod_action_id,
            publicSymbolTable);
    FklNastNode **builtin_pattern_node = outerCtx->builtin_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++) {
        FklNastNode *node =
                fklCreateNastNodeFromCstr(builtin_pattern_cstr_func[i].ps,
                        publicSymbolTable);
        builtin_pattern_node[i] = fklCreatePatternFromNast(node, NULL);
        fklDestroyNastNode(node);
    }

    for (size_t i = 0; i < FKL_BUILTIN_REPLACEMENT_NUM; i++)
        outerCtx->builtin_replacement_id[i] =
                fklAddSymbolCstr(builtInSymbolReplacement[i].s,
                        publicSymbolTable)
                        ->v;

    FklNastNode **builtin_sub_pattern_node = outerCtx->builtin_sub_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_SUB_PATTERN_NUM; i++) {
        FklNastNode *node = fklCreateNastNodeFromCstr(builtInSubPattern[i],
                publicSymbolTable);
        builtin_sub_pattern_node[i] = fklCreatePatternFromNast(node, NULL);
        fklDestroyNastNode(node);
    }
}

void fklSetCodegenOuterCtxMainFileRealPathDir(FklCodegenOuterCtx *outer_ctx,
        char *dir) {
    if (outer_ctx->main_file_real_path_dir)
        fklZfree(outer_ctx->main_file_real_path_dir);
    outer_ctx->main_file_real_path_dir = dir;
}

void fklUninitCodegenOuterCtx(FklCodegenOuterCtx *outer_ctx) {
    fklUninitSymbolTable(&outer_ctx->public_symbol_table);
    fklUninitConstTable(&outer_ctx->public_kt);
    fklUninitGrammer(&outer_ctx->builtin_g);
    if (outer_ctx->gc) {
        fklDestroyVMgc(outer_ctx->gc);
        outer_ctx->gc = NULL;
    }
    FklNastNode **nodes = outer_ctx->builtin_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++)
        fklDestroyNastNode(nodes[i]);
    nodes = outer_ctx->builtin_sub_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_SUB_PATTERN_NUM; i++)
        fklDestroyNastNode(nodes[i]);
    fklZfree(outer_ctx->cwd);
    fklZfree(outer_ctx->main_file_real_path_dir);
}

void fklDestroyCodegenInfo(FklCodegenInfo *codegen) {
    while (codegen && codegen->is_destroyable) {
        codegen->refcount -= 1;
        FklCodegenInfo *prev = codegen->prev;
        if (!codegen->refcount) {
            fklUninitCodegenInfo(codegen);
            fklZfree(codegen);
            codegen = prev;
        } else
            break;
    }
}

static inline int mapAllBuiltInPattern(FklNastNode *curExp,
        FklCodegenQuestVector *codegenQuestStack,
        uint32_t scope,
        FklCodegenMacroScope *macroScope,
        FklCodegenEnv *curEnv,
        FklCodegenInfo *codegenr,
        FklCodegenErrorState *errorState,
        uint8_t must_has_retval) {
    FklNastNode *const *builtin_pattern_node =
            codegenr->outer_ctx->builtin_pattern_node;

    if (fklIsNastNodeList(curExp))
        for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++)
            if (matchAndCall(builtin_pattern_cstr_func[i].func,
                        builtin_pattern_node[i],
                        scope,
                        macroScope,
                        curExp,
                        codegenQuestStack,
                        curEnv,
                        codegenr,
                        errorState,
                        must_has_retval))
                return 0;

    if (curExp->type == FKL_NAST_PAIR) {
        codegen_funcall(curExp,
                codegenQuestStack,
                scope,
                macroScope,
                curEnv,
                codegenr,
                errorState);
        return 0;
    }
    return 1;
}

static inline FklByteCodelnt *append_get_loc_ins(InsAppendMode m,
        FklByteCodelnt *bcl,
        uint32_t idx,
        FklSid_t fid,
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
        FklSid_t fid,
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

FklByteCodelnt *fklGenExpressionCodeWithQuest(FklCodegenQuest *initialQuest,
        FklCodegenInfo *codegener) {
    FklCodegenOuterCtx *outer_ctx = codegener->outer_ctx;
    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklByteCodelntVector resultStack;
    fklByteCodelntVectorInit(&resultStack, 1);
    FklCodegenErrorState errorState = { 0, NULL, 0, 0, NULL, NULL };
    FklCodegenQuestVector codegenQuestStack;
    fklCodegenQuestVectorInit(&codegenQuestStack, 32);
    fklCodegenQuestVectorPushBack2(&codegenQuestStack, initialQuest);
    while (!fklCodegenQuestVectorIsEmpty(&codegenQuestStack)) {
        FklCodegenQuest *curCodegenQuest =
                *fklCodegenQuestVectorBackNonNull(&codegenQuestStack);
        FklCodegenInfo *curCodegen = curCodegenQuest->codegen;
        FklCodegenNextExpression *nextExpression =
                curCodegenQuest->nextExpression;
        FklCodegenEnv *curEnv = curCodegenQuest->env;
        FklCodegenQuestContext *curContext = curCodegenQuest->context;
        int r = 1;
        if (nextExpression) {
            FklNastNode *(
                    *get_next_expression)(void *, FklCodegenErrorState *) =
                    nextExpression->t->getNextExpression;
            uint8_t must_has_retval = nextExpression->must_has_retval;

            for (FklNastNode *curExp =
                            get_next_expression(nextExpression->context,
                                    &errorState);
                    curExp;
                    curExp = get_next_expression(nextExpression->context,
                            &errorState)) {
            skip:
                if (curExp->type == FKL_NAST_PAIR) {
                    FklNastNode *orig_cur_exp = fklMakeNastNodeRef(curExp);
                    curExp = fklTryExpandCodegenMacro(orig_cur_exp,
                            curCodegen,
                            curCodegenQuest->macroScope,
                            &errorState);
                    if (errorState.type) {
                        fklDestroyNastNode(orig_cur_exp);
                        break;
                    }
                    if (must_has_retval == FIRST_MUST_HAS_RETVAL) {
                        must_has_retval = DO_NOT_NEED_RETVAL;
                        nextExpression->must_has_retval = DO_NOT_NEED_RETVAL;
                    }
                    fklDestroyNastNode(orig_cur_exp);
                } else if (curExp->type == FKL_NAST_SYM) {
                    FklNastNode *replacement = NULL;
                    ReplacementFunc f = NULL;
                    for (FklCodegenMacroScope *cs = curCodegenQuest->macroScope;
                            cs && !replacement;
                            cs = cs->prev)
                        replacement = fklGetReplacement(curExp->sym,
                                cs->replacements);
                    if (replacement) {
                        fklDestroyNastNode(curExp);
                        curExp = fklMakeNastNodeRef(replacement);
                        goto skip;
                    } else if ((f = findBuiltInReplacementWithId(curExp->sym,
                                        outer_ctx->builtin_replacement_id))) {
                        FklNastNode *t = f(curExp, curEnv, curCodegen);
                        FKL_ASSERT(curExp);
                        fklDestroyNastNode(curExp);
                        curExp = t;
                        goto skip;
                    } else {
                        FklByteCodelnt *bcl = NULL;
                        FklSymDefHashMapElm *def =
                                fklFindSymbolDefByIdAndScope(curExp->sym,
                                        curCodegenQuest->scope,
                                        curEnv);
                        if (def)
                            bcl = append_get_loc_ins(INS_APPEND_BACK,
                                    NULL,
                                    def->v.idx,
                                    curCodegen->fid,
                                    curExp->curline,
                                    curCodegenQuest->scope);
                        else {
                            uint32_t idx =
                                    fklAddCodegenRefBySidRetIndex(curExp->sym,
                                            curEnv,
                                            curCodegen->fid,
                                            curExp->curline,
                                            0);
                            bcl = append_get_var_ref_ins(INS_APPEND_BACK,
                                    NULL,
                                    idx,
                                    curCodegen->fid,
                                    curExp->curline,
                                    curCodegenQuest->scope);
                        }
                        curContext->t->__put_bcl(curContext->data, bcl);
                        fklDestroyNastNode(curExp);
                        continue;
                    }
                }
                FKL_ASSERT(curExp);
                r = mapAllBuiltInPattern(curExp,
                        &codegenQuestStack,
                        curCodegenQuest->scope,
                        curCodegenQuest->macroScope,
                        curEnv,
                        curCodegen,
                        &errorState,
                        must_has_retval);
                if (r) {
                    curContext->t->__put_bcl(curContext->data,
                            create_bc_lnt(fklCodegenNode(curExp, curCodegen),
                                    curCodegen->fid,
                                    curExp->curline,
                                    curCodegenQuest->scope));
                }
                fklDestroyNastNode(curExp);
                if ((!r
                            && (fklCodegenQuestVectorIsEmpty(&codegenQuestStack)
                                    || *fklCodegenQuestVectorBack(
                                               &codegenQuestStack)
                                               != curCodegenQuest))
                        || errorState.type)
                    break;
            }
        }
        if (errorState.type) {
        print_error:
            fklPrintCodegenError(&errorState,
                    curCodegen,
                    curCodegen->runtime_symbol_table,
                    pst);
            while (!fklCodegenQuestVectorIsEmpty(&codegenQuestStack)) {
                destroyCodegenQuest(*fklCodegenQuestVectorPopBackNonNull(
                        &codegenQuestStack));
            }
            fklCodegenQuestVectorUninit(&codegenQuestStack);
            fklByteCodelntVectorUninit(&resultStack);
            return NULL;
        }
        FklCodegenQuest *otherCodegenQuest =
                *fklCodegenQuestVectorBackNonNull(&codegenQuestStack);
        if (otherCodegenQuest == curCodegenQuest) {
            fklCodegenQuestVectorPopBack(&codegenQuestStack);
            FklCodegenQuest *prevQuest =
                    curCodegenQuest->prev ? curCodegenQuest->prev
                    : fklCodegenQuestVectorIsEmpty(&codegenQuestStack)
                            ? NULL
                            : *fklCodegenQuestVectorBackNonNull(
                                      &codegenQuestStack);
            FklByteCodelnt *resultBcl =
                    curCodegenQuest->processer(curCodegenQuest->codegen,
                            curEnv,
                            curCodegenQuest->scope,
                            curCodegenQuest->macroScope,
                            curContext,
                            curCodegenQuest->codegen->fid,
                            curCodegenQuest->curline,
                            &errorState,
                            outer_ctx);
            if (resultBcl) {
                if (fklCodegenQuestVectorIsEmpty(&codegenQuestStack))
                    fklByteCodelntVectorPushBack2(&resultStack, resultBcl);
                else {
                    FklCodegenQuestContext *prevContext = prevQuest->context;
                    prevContext->t->__put_bcl(prevContext->data, resultBcl);
                }
            }
            if (errorState.type) {
                fklCodegenQuestVectorPushBack2(&codegenQuestStack,
                        curCodegenQuest);
                goto print_error;
            }
            destroyCodegenQuest(curCodegenQuest);
        }
    }
    FklByteCodelnt *retval = NULL;
    if (!fklByteCodelntVectorIsEmpty(&resultStack))
        retval = *fklByteCodelntVectorPopBackNonNull(&resultStack);
    fklByteCodelntVectorUninit(&resultStack);
    fklCodegenQuestVectorUninit(&codegenQuestStack);
    if (retval) {
        FklInstruction ret = create_op_ins(FKL_OP_RET);
        fklByteCodeLntPushBackIns(retval,
                &ret,
                codegener->fid,
                codegener->curline,
                1);
        fklPeepholeOptimize(retval);
    }
    return retval;
}

FklByteCodelnt *fklGenExpressionCodeWithFpForPrecompile(FILE *fp,
        FklCodegenInfo *codegen,
        FklCodegenEnv *cur_env) {
    FklCodegenQuest *initialQuest =
            fklCreateCodegenQuest(pre_compile_main_bc_process,
                    createDefaultStackContext(fklByteCodelntVectorCreate(32)),
                    createFpNextExpression(fp, codegen),
                    1,
                    cur_env->macros,
                    cur_env,
                    1,
                    NULL,
                    codegen);
    return fklGenExpressionCodeWithQuest(initialQuest, codegen);
}

FklByteCodelnt *fklGenExpressionCodeWithFp(FILE *fp,
        FklCodegenInfo *codegen,
        FklCodegenEnv *cur_env) {
    FklCodegenQuest *initialQuest = fklCreateCodegenQuest(_begin_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            createFpNextExpression(fp, codegen),
            1,
            cur_env->macros,
            cur_env,
            1,
            NULL,
            codegen);
    return fklGenExpressionCodeWithQuest(initialQuest, codegen);
}

FklByteCodelnt *fklGenExpressionCode(FklNastNode *exp,
        FklCodegenEnv *cur_env,
        FklCodegenInfo *codegenr) {
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(queue, exp);
    FklCodegenQuest *initialQuest = fklCreateCodegenQuest(_default_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            createDefaultQueueNextExpression(queue),
            1,
            cur_env->macros,
            cur_env,
            exp->curline,
            NULL,
            codegenr);
    return fklGenExpressionCodeWithQuest(initialQuest, codegenr);
}

FklByteCodelnt *fklGenExpressionCodeWithQueue(FklNastNodeQueue *q,
        FklCodegenInfo *codegen,
        FklCodegenEnv *cur_env) {
    FklCodegenQuest *initialQuest = fklCreateCodegenQuest(_begin_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            createDefaultQueueNextExpression(q),
            1,
            cur_env->macros,
            cur_env,
            1,
            NULL,
            codegen);
    return fklGenExpressionCodeWithQuest(initialQuest, codegen);
}

static inline void init_codegen_grammer_ptr(FklCodegenInfo *codegen) {
    codegen->self_g = NULL;
    codegen->g = &codegen->self_g;
    codegen->named_prod_groups = &codegen->self_named_prod_groups;

    codegen->unnamed_g = &codegen->self_unnamed_g;
}

FklCodegenEnv *fklInitGlobalCodegenInfo(FklCodegenInfo *codegen,
        const char *rp,
        FklSymbolTable *runtime_st,
        FklConstTable *runtime_kt,
        int destroyAbleMark,
        FklCodegenOuterCtx *outer_ctx,
        FklCodegenInfoWorkCb work_cb,
        FklCodegenInfoEnvWorkCb env_work_cb,
        void *work_ctx) {
    memset(codegen, 0, sizeof(*codegen));
    codegen->runtime_symbol_table = runtime_st;
    codegen->runtime_kt = runtime_kt;
    codegen->outer_ctx = outer_ctx;
    if (rp != NULL) {
        codegen->dir = fklGetDir(rp);
        codegen->filename = fklRelpath(outer_ctx->main_file_real_path_dir, rp);
        codegen->realpath = fklZstrdup(rp);
        codegen->fid = fklAddSymbolCstr(codegen->filename, runtime_st)->v;
    } else {
        codegen->dir = fklSysgetcwd();
        codegen->filename = NULL;
        codegen->realpath = NULL;
        codegen->fid = 0;
    }
    codegen->is_lib = 1;
    codegen->is_macro = 0;
    codegen->curline = 1;
    codegen->prev = NULL;
    codegen->global_env = fklCreateCodegenEnv(NULL, 0, NULL);
    fklInitGlobCodegenEnv(codegen->global_env, &outer_ctx->public_symbol_table);
    codegen->global_env->refcount++;
    codegen->is_destroyable = destroyAbleMark;
    codegen->refcount = 0;

    codegen->libraries = fklCodegenLibVectorCreate(8);
    codegen->macro_libraries = fklCodegenLibVectorCreate(8);
    codegen->pts = fklCreateFuncPrototypes(0);
    codegen->macro_pts = fklCreateFuncPrototypes(0);
    codegen->outer_ctx = outer_ctx;

    fklCgExportSidIdxHashMapInit(&codegen->exports);
    codegen->export_replacement = fklReplacementHashMapCreate();
    codegen->export_named_prod_groups = fklSidHashSetCreate();

    init_codegen_grammer_ptr(codegen);

    codegen->work.work_ctx = work_ctx;
    codegen->work.work_cb = work_cb;
    codegen->work.env_work_cb = env_work_cb;

    if (work_cb)
        work_cb(codegen, work_ctx);

    FklCodegenEnv *main_env = fklCreateCodegenEnv(codegen->global_env,
            1,
            codegen->global_env->macros);
    create_and_insert_to_pool(codegen,
            0,
            main_env,
            0,
            1,
            &outer_ctx->public_symbol_table);
    main_env->refcount++;
    return main_env;
}

void fklInitCodegenInfo(FklCodegenInfo *codegen,
        const char *filename,
        FklCodegenInfo *prev,
        FklSymbolTable *runtime_st,
        FklConstTable *runtime_kt,
        int destroyAbleMark,
        int libMark,
        int macroMark,
        FklCodegenOuterCtx *outer_ctx) {
    memset(codegen, 0, sizeof(*codegen));
    if (filename != NULL) {
        char *rp = fklRealpath(filename);
        codegen->dir = fklGetDir(rp);
        codegen->filename = fklRelpath(outer_ctx->main_file_real_path_dir, rp);
        codegen->realpath = rp;
        codegen->fid = fklAddSymbolCstr(codegen->filename, runtime_st)->v;
    } else {
        codegen->dir = fklSysgetcwd();
        codegen->filename = NULL;
        codegen->realpath = NULL;
        codegen->fid = 0;
    }

    codegen->curline = 1;
    codegen->prev = prev;

    codegen->runtime_symbol_table = runtime_st;
    codegen->runtime_kt = runtime_kt;
    codegen->is_destroyable = destroyAbleMark;
    codegen->refcount = 0;
    codegen->exports.buckets = NULL;
    codegen->is_lib = libMark;
    codegen->is_macro = macroMark;

    codegen->export_macro = NULL;
    codegen->export_replacement =
            libMark ? fklReplacementHashMapCreate() : NULL;
    codegen->export_named_prod_groups = libMark ? fklSidHashSetCreate() : NULL;
    codegen->exports.buckets = NULL;
    if (libMark)
        fklCgExportSidIdxHashMapInit(&codegen->exports);

    init_codegen_grammer_ptr(codegen);
    if (prev) {
        prev->refcount += 1;
        codegen->global_env = prev->global_env;
        codegen->global_env->refcount++;
        codegen->work = prev->work;
        codegen->libraries = prev->libraries;
        codegen->macro_libraries = prev->macro_libraries;
        codegen->pts = prev->pts;
        codegen->macro_pts = prev->macro_pts;
    } else {
        codegen->global_env = fklCreateCodegenEnv(NULL, 1, NULL);
        fklInitGlobCodegenEnv(codegen->global_env,
                &outer_ctx->public_symbol_table);
        codegen->global_env->refcount++;

        codegen->work.work_cb = NULL;
        codegen->work.env_work_cb = NULL;
        codegen->work.work_ctx = NULL;

        codegen->libraries = fklCodegenLibVectorCreate(8);
        codegen->macro_libraries = fklCodegenLibVectorCreate(8);
        codegen->pts = fklCreateFuncPrototypes(0);
        codegen->macro_pts = fklCreateFuncPrototypes(0);
    }
    codegen->outer_ctx = outer_ctx;

    if (codegen->work.work_cb)
        codegen->work.work_cb(codegen, codegen->work.work_ctx);
}

void fklUninitCodegenInfo(FklCodegenInfo *codegen) {
    fklDestroyCodegenEnv(codegen->global_env);

    if (!codegen->is_destroyable || codegen->is_macro) {
        fklDestroyFuncPrototypes(codegen->macro_pts);
        FklCodegenLibVector *macro_libraries = codegen->macro_libraries;
        FklCodegenLib *cur_lib = macro_libraries->base;
        FklCodegenLib *const lib_end = cur_lib + macro_libraries->size;

        for (; cur_lib < lib_end; ++cur_lib)
            fklUninitCodegenLib(cur_lib);
        fklCodegenLibVectorDestroy(macro_libraries);
    }

    if (!codegen->is_destroyable) {
        if (codegen->runtime_symbol_table
                && codegen->runtime_symbol_table
                           != &codegen->outer_ctx->public_symbol_table)
            fklDestroySymbolTable(codegen->runtime_symbol_table);
        if (codegen->runtime_kt
                && codegen->runtime_kt != &codegen->outer_ctx->public_kt)
            fklDestroyConstTable(codegen->runtime_kt);

        while (!fklCodegenLibVectorIsEmpty(codegen->libraries))
            fklUninitCodegenLib(
                    fklCodegenLibVectorPopBackNonNull(codegen->libraries));
        fklCodegenLibVectorDestroy(codegen->libraries);

        if (codegen->pts)
            fklDestroyFuncPrototypes(codegen->pts);
    }
    fklZfree(codegen->dir);
    if (codegen->filename)
        fklZfree(codegen->filename);
    if (codegen->realpath)
        fklZfree(codegen->realpath);
    if (codegen->exports.buckets)
        fklCgExportSidIdxHashMapUninit(&codegen->exports);
    for (FklCodegenMacro *cur = codegen->export_macro; cur;) {
        FklCodegenMacro *t = cur;
        cur = cur->next;
        fklDestroyCodegenMacro(t);
    }
    if (codegen->export_named_prod_groups)
        fklSidHashSetDestroy(codegen->export_named_prod_groups);
    if (codegen->export_replacement)
        fklReplacementHashMapDestroy(codegen->export_replacement);
    if (codegen->g == &codegen->self_g && *codegen->g) {
        FklGrammer *g = *codegen->g;
        fklUninitGrammer(g);
        fklUninitGrammer(codegen->unnamed_g);
        fklGraProdGroupHashMapUninit(codegen->named_prod_groups);
        fklZfree(g);
        codegen->g = NULL;
    }
}

void fklInitCodegenScriptLib(FklCodegenLib *lib,
        FklCodegenInfo *codegen,
        FklByteCodelnt *bcl,
        uint64_t epc,
        FklCodegenEnv *env) {
    lib->type = FKL_CODEGEN_LIB_SCRIPT;
    lib->bcl = bcl;
    lib->epc = epc;
    lib->named_prod_groups.buckets = NULL;
    lib->exports.buckets = NULL;
    if (codegen) {

        lib->rp = codegen->realpath;

        lib->head = codegen->export_macro;
        lib->replacements = codegen->export_replacement;
        if (codegen->export_named_prod_groups
                && codegen->export_named_prod_groups->count) {
            fklGraProdGroupHashMapInit(&lib->named_prod_groups);
            for (FklSidHashSetNode *sid_list =
                            codegen->export_named_prod_groups->first;
                    sid_list;
                    sid_list = sid_list->next) {
                FklSid_t id = sid_list->k;
                FklGrammerProdGroupItem *group =
                        fklGraProdGroupHashMapGet2(codegen->named_prod_groups,
                                id);
                FKL_ASSERT(group);
                FklGrammerProdGroupItem *target_group =
                        add_production_group(&lib->named_prod_groups,
                                &codegen->outer_ctx->public_symbol_table,
                                id);
                merge_group(target_group, group, NULL);

                for (const FklStrHashSetNode *cur = group->delimiters.first;
                        cur;
                        cur = cur->next) {
                    fklAddString(&target_group->delimiters, cur->k);
                }
            }
        }
    } else {
        lib->rp = NULL;
        lib->head = NULL;
        lib->replacements = NULL;
    }
    if (env) {
        FklCgExportSidIdxHashMap *exports_index = &lib->exports;
        fklCgExportSidIdxHashMapInit(exports_index);
        FklCgExportSidIdxHashMap *export_sid_set = &codegen->exports;
        lib->prototypeId = env->prototypeId;
        for (const FklCgExportSidIdxHashMapNode *sid_idx_list =
                        export_sid_set->first;
                sid_idx_list;
                sid_idx_list = sid_idx_list->next) {
            fklCgExportSidIdxHashMapPut(exports_index,
                    &sid_idx_list->k,
                    &sid_idx_list->v);
        }
    } else
        lib->prototypeId = 0;
}

void fklInitCodegenDllLib(FklCodegenLib *lib,
        char *rp,
        uv_lib_t dll,
        FklCodegenDllLibInitExportFunc init,
        FklSymbolTable *pst) {
    lib->rp = rp;
    lib->type = FKL_CODEGEN_LIB_DLL;
    lib->dll = dll;
    lib->head = NULL;
    lib->replacements = NULL;
    lib->exports.buckets = NULL;
    lib->named_prod_groups.buckets = NULL;

    uint32_t num = 0;
    FklSid_t *exports = init(pst, &num);
    FklCgExportSidIdxHashMap *exports_idx = &lib->exports;
    fklCgExportSidIdxHashMapInit(exports_idx);
    if (num) {
        for (uint32_t i = 0; i < num; i++) {
            fklCgExportSidIdxHashMapAdd(exports_idx,
                    &exports[i],
                    &(FklCodegenExportIdx){ .idx = i });
        }
    }
    if (exports)
        fklZfree(exports);
}

void fklDestroyCodegenMacroList(FklCodegenMacro *cur) {
    while (cur) {
        FklCodegenMacro *t = cur;
        cur = cur->next;
        fklDestroyCodegenMacro(t);
    }
}

void fklClearCodegenLibMacros(FklCodegenLib *lib) {
    if (lib->head) {
        fklDestroyCodegenMacroList(lib->head);
        lib->head = NULL;
    }
    if (lib->replacements) {
        fklReplacementHashMapDestroy(lib->replacements);
        lib->replacements = NULL;
    }
    if (lib->named_prod_groups.buckets) {
        fklGraProdGroupHashMapUninit(&lib->named_prod_groups);
    }
}

static inline void uninit_codegen_lib_info(FklCodegenLib *lib) {
    if (lib->exports.buckets)
        fklCgExportSidIdxHashMapUninit(&lib->exports);
    fklZfree(lib->rp);
}

void fklUninitCodegenLib(FklCodegenLib *lib) {
    if (lib == NULL)
        return;
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        if (lib->bcl != NULL) {
            fklDestroyByteCodelnt(lib->bcl);
            lib->bcl = NULL;
        }
        break;
    case FKL_CODEGEN_LIB_DLL:
        uv_dlclose(&lib->dll);
        break;
    case FKL_CODEGEN_LIB_UNINIT:
        return;
        break;
    }
    uninit_codegen_lib_info(lib);
    fklClearCodegenLibMacros(lib);
}

FklCodegenMacro *fklCreateCodegenMacro(FklNastNode *pattern,
        FklNastNode *origin_exp,
        const FklByteCodelnt *bcl,
        FklCodegenMacro *next,
        uint32_t prototype_id) {
    FklCodegenMacro *r = (FklCodegenMacro *)fklZmalloc(sizeof(FklCodegenMacro));
    FKL_ASSERT(r);
    r->pattern = pattern;
    r->origin_exp = origin_exp;
    r->bcl = fklCopyByteCodelnt(bcl);
    r->next = next;
    r->prototype_id = prototype_id;
    return r;
}

FklCodegenMacro *fklCreateCodegenMacroMove(FklNastNode *pattern,
        FklNastNode *origin_exp,
        FklByteCodelnt *bcl,
        FklCodegenMacro *next,
        uint32_t prototype_id) {
    FklCodegenMacro *r = (FklCodegenMacro *)fklZmalloc(sizeof(FklCodegenMacro));
    FKL_ASSERT(r);
    r->pattern = pattern;
    r->origin_exp = origin_exp;
    r->bcl = bcl;
    r->next = next;
    r->prototype_id = prototype_id;
    return r;
}

void fklDestroyCodegenMacro(FklCodegenMacro *macro) {
    uninit_codegen_macro(macro);
    fklZfree(macro);
}

FklCodegenMacroScope *fklCreateCodegenMacroScope(FklCodegenMacroScope *prev) {
    FklCodegenMacroScope *r =
            (FklCodegenMacroScope *)fklZmalloc(sizeof(FklCodegenMacroScope));
    FKL_ASSERT(r);
    r->head = NULL;
    r->replacements = fklReplacementHashMapCreate();
    r->refcount = 0;
    r->prev = make_macro_scope_ref(prev);
    return r;
}

void fklDestroyCodegenMacroScope(FklCodegenMacroScope *macros) {
    while (macros) {
        macros->refcount--;
        if (macros->refcount)
            return;
        else {
            FklCodegenMacroScope *prev = macros->prev;
            for (FklCodegenMacro *cur = macros->head; cur;) {
                FklCodegenMacro *t = cur;
                cur = cur->next;
                fklDestroyCodegenMacro(t);
            }
            if (macros->replacements)
                fklReplacementHashMapDestroy(macros->replacements);
            fklZfree(macros);
            macros = prev;
        }
    }
}

static FklCodegenMacro *findMacro(FklNastNode *exp,
        FklCodegenMacroScope *macros,
        FklPmatchHashMap *pht) {
    if (!exp)
        return NULL;
    FklCodegenMacro *r = NULL;
    for (; !r && macros; macros = macros->prev) {
        for (FklCodegenMacro *cur = macros->head; cur; cur = cur->next) {
            if (pht->buckets == NULL)
                fklPmatchHashMapInit(pht);
            if (fklPatternMatch(cur->pattern, exp, pht)) {
                r = cur;
                break;
            }
            fklPmatchHashMapClear(pht);
        }
    }
    return r;
}

static void initVMframeFromPatternMatchTable(FklVM *exe,
        FklVMframe *frame,
        FklPmatchHashMap *ht,
        FklLineNumHashMap *lineHash,
        FklFuncPrototypes *pts,
        uint32_t prototype_id) {
    FklFuncPrototype *mainPts = &pts->pa[prototype_id];
    FklVMCompoundFrameVarRef *lr = fklGetCompoundFrameLocRef(frame);
    FklVMproc *proc = FKL_VM_PROC(fklGetCompoundFrameProc(frame));
    uint32_t count = mainPts->lcount;
    uint32_t idx = 0;
    for (FklPmatchHashMapNode *list = ht->first; list; list = list->next) {
        FklVMvalue *v = fklCreateVMvalueFromNastNode(exe, list->v, lineHash);
        FKL_VM_GET_ARG(exe, frame, idx) = v;
        idx++;
    }
    lr->lcount = count;
    lr->lref = NULL;
    lr->lrefl = NULL;
    proc->closure = lr->ref;
    proc->rcount = lr->rcount;
}

typedef struct MacroExpandCtx {
    FklNastNode **retval;
    FklLineNumHashMap *lineHash;
    FklSymbolTable *symbolTable;
    uint64_t curline;
} MacroExpandCtx;

FKL_CHECK_OTHER_OBJ_CONTEXT_SIZE(MacroExpandCtx);

static int macro_expand_frame_step(void *data, FklVM *exe) {
    MacroExpandCtx *ctx = (MacroExpandCtx *)data;
    FKL_VM_ACQUIRE_ST_BLOCK(exe->gc, flag) {
        *(ctx->retval) = fklCreateNastNodeFromVMvalue(fklGetTopValue(exe),
                ctx->curline,
                ctx->lineHash,
                exe->gc);
    }
    return 0;
}

static void macro_expand_frame_backtrace(void *data, FILE *fp, FklVMgc *gc) {
    fputs("at <macroexpand>\n", fp);
}

static const FklVMframeContextMethodTable MacroExpandMethodTable = {
    .step = macro_expand_frame_step,
    .finalizer = NULL,
    .print_backtrace = macro_expand_frame_backtrace,
};

static void push_macro_expand_frame(FklVM *exe,
        FklNastNode **ptr,
        FklLineNumHashMap *lineHash,
        FklSymbolTable *symbolTable,
        uint64_t curline) {
    FklVMframe *f = fklCreateOtherObjVMframe(exe, &MacroExpandMethodTable);
    MacroExpandCtx *ctx = (MacroExpandCtx *)f->data;
    ctx->retval = ptr;
    ctx->lineHash = lineHash;
    ctx->symbolTable = symbolTable;
    ctx->curline = curline;
    fklPushVMframe(f, exe);
}

static inline void update_new_codegen_to_new_vm_lib(FklVM *exe,
        const FklCodegenLibVector *macro_libraries,
        FklFuncPrototypes *pts) {
    FklVMgc *gc = exe->gc;
    if (gc->lib_num != macro_libraries->size) {
        gc->libs = (FklVMlib *)fklZrealloc(gc->libs,
                (macro_libraries->size + 1) * sizeof(FklVMlib));
        FKL_ASSERT(gc->libs);

        for (size_t i = gc->lib_num; i < macro_libraries->size; ++i) {
            const FklCodegenLib *cur = &macro_libraries->base[i];
            fklInitVMlibWithCodegenLib(cur, &gc->libs[i + 1], exe, pts);
            if (cur->type == FKL_CODEGEN_LIB_SCRIPT)
                fklInitMainProcRefs(exe, gc->libs[i + 1].proc);
        }
        gc->lib_num = macro_libraries->size;
    }
}

static inline int is_need_update_const_array(FklCodegenOuterCtx *outer_ctx) {
    return outer_ctx->ki64_count != outer_ctx->public_kt.ki64t.count
        || outer_ctx->kf64_count != outer_ctx->public_kt.kf64t.count
        || outer_ctx->kstr_count != outer_ctx->public_kt.kstrt.count
        || outer_ctx->kbvec_count != outer_ctx->public_kt.kbvect.count
        || outer_ctx->kbi_count != outer_ctx->public_kt.kbit.count;
}

FklVM *fklInitMacroExpandVM(FklCodegenOuterCtx *outer_ctx,
        FklByteCodelnt *bcl,
        FklFuncPrototypes *pts,
        uint32_t prototype_id,
        FklPmatchHashMap *ht,
        FklLineNumHashMap *lineHash,
        FklCodegenLibVector *macro_libraries,
        FklNastNode **pr,
        uint64_t curline,
        FklSymbolTable *public_st,
        FklConstTable *public_kt) {
    FklVMgc *gc = outer_ctx->gc;
    if (gc) {
        FklVM *exe;
        gc->pts = pts;
        exe = fklCreateVM(NULL, gc);
        FklVMvalue *code_obj = fklCreateVMvalueCodeObj(exe, bcl);
        FklVMvalue *proc = fklCreateVMvalueProc(exe, code_obj, prototype_id);

        push_macro_expand_frame(exe, pr, lineHash, public_st, curline);

        fklInitMainProcRefs(exe, proc);
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, proc);
        fklCallObj(exe, proc);
        initVMframeFromPatternMatchTable(exe,
                exe->top_frame,
                ht,
                lineHash,
                pts,
                prototype_id);

        if (is_need_update_const_array(outer_ctx)) {
            fklVMgcUpdateConstArray(gc, public_kt);
            outer_ctx->ki64_count = public_kt->ki64t.count;
            outer_ctx->kf64_count = public_kt->kf64t.count;
            outer_ctx->kstr_count = public_kt->kstrt.count;
            outer_ctx->kbvec_count = public_kt->kbvect.count;
            outer_ctx->kbi_count = public_kt->kbit.count;
        }

        update_new_codegen_to_new_vm_lib(exe, macro_libraries, pts);
        return exe;
    } else {
        FklVM *exe = fklCreateVMwithByteCode(NULL,
                public_st,
                public_kt,
                pts,
                prototype_id,
                0);

        FklVMgc *gc = exe->gc;
        gc->lib_num = macro_libraries->size;
        gc->libs = (FklVMlib *)fklZcalloc(macro_libraries->size + 1,
                sizeof(FklVMlib));
        FKL_ASSERT(gc->libs);
        for (size_t i = 0; i < macro_libraries->size; i++) {
            const FklCodegenLib *cur = &macro_libraries->base[i];
            fklInitVMlibWithCodegenLib(cur, &gc->libs[i + 1], exe, pts);
            if (cur->type == FKL_CODEGEN_LIB_SCRIPT)
                fklInitMainProcRefs(exe, gc->libs[i + 1].proc);
        }

        FklVMvalue *code_obj = fklCreateVMvalueCodeObj(exe, bcl);
        FklVMvalue *proc = fklCreateVMvalueProc(exe, code_obj, prototype_id);
        fklInitMainProcRefs(exe, proc);

        push_macro_expand_frame(exe, pr, lineHash, public_st, curline);

        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, proc);
        fklCallObj(exe, proc);
        initVMframeFromPatternMatchTable(exe,
                exe->top_frame,
                ht,
                lineHash,
                pts,
                prototype_id);

        outer_ctx->gc = exe->gc;
        gc = outer_ctx->gc;
        return exe;
    }
}

static inline void get_macro_pts_and_lib(FklCodegenInfo *info,
        FklFuncPrototypes **ppts,
        FklCodegenLibVector **plib) {
    for (; info->prev; info = info->prev)
        ;
    *ppts = info->macro_pts;
    *plib = info->macro_libraries;
}

FklNastNode *fklTryExpandCodegenMacro(FklNastNode *exp,
        FklCodegenInfo *codegen,
        FklCodegenMacroScope *macros,
        FklCodegenErrorState *errorState) {
    FklSymbolTable *pst = &codegen->outer_ctx->public_symbol_table;
    FklConstTable *pkt = &codegen->outer_ctx->public_kt;
    FklNastNode *r = exp;
    FklPmatchHashMap ht = { .buckets = NULL };
    uint64_t curline = exp->curline;
    for (FklCodegenMacro *macro = findMacro(r, macros, &ht);
            !errorState->type && macro;
            macro = findMacro(r, macros, &ht)) {
        fklPmatchHashMapAdd2(&ht,
                codegen->outer_ctx->builtInPatternVar_orig,
                exp);
        FklNastNode *retval = NULL;
        FklLineNumHashMap lineHash;
        fklLineNumHashMapInit(&lineHash);

        FklCodegenOuterCtx *outer_ctx = codegen->outer_ctx;
        const char *cwd = outer_ctx->cwd;
        fklChdir(codegen->dir);

        FklFuncPrototypes *pts = NULL;
        FklCodegenLibVector *macro_libraries = NULL;
        get_macro_pts_and_lib(codegen, &pts, &macro_libraries);
        FklVM *exe = fklInitMacroExpandVM(outer_ctx,
                macro->bcl,
                pts,
                macro->prototype_id,
                &ht,
                &lineHash,
                macro_libraries,
                &retval,
                r->curline,
                pst,
                pkt);
        FklVMgc *gc = outer_ctx->gc;
        int e = fklRunVM(exe);
        fklMoveThreadObjectsToGc(exe, gc);

        fklChdir(cwd);

        gc->pts = NULL;
        if (e) {
            errorState->type = FKL_ERR_MACROEXPANDFAILED;
            errorState->place = r;
            errorState->line = curline;
            fklDeleteCallChain(exe);
            r = NULL;
        } else {
            if (retval) {
                fklDestroyNastNode(r);
                r = retval;
            } else {
                errorState->type = FKL_ERR_CIR_REF;
                errorState->place = NULL;
                errorState->line = curline;
            }
        }
        fklLineNumHashMapUninit(&lineHash);
        fklDestroyAllVMs(exe);
    }
    if (ht.buckets)
        fklPmatchHashMapUninit(&ht);
    return r;
}

void fklInitVMlibWithCodegenLibRefs(FklCodegenLib *clib,
        FklVMlib *vlib,
        FklVM *exe,
        FklVMvalue **refs,
        uint32_t count,
        FklFuncPrototypes *pts) {
    FklVMvalue *val = FKL_VM_NIL;
    if (clib->type == FKL_CODEGEN_LIB_SCRIPT) {
        FklVMvalue *codeObj = fklCreateVMvalueCodeObj(exe, clib->bcl);
        FklVMvalue *proc =
                fklCreateVMvalueProc(exe, codeObj, clib->prototypeId);
        fklInitMainProcRefs(exe, proc);
        val = proc;
    } else
        val = fklCreateVMvalueStr2(exe,
                strlen(clib->rp) - strlen(FKL_DLL_FILE_TYPE),
                clib->rp);
    fklInitVMlib(vlib, val, clib->epc);
}

void fklInitVMlibWithCodegenLib(const FklCodegenLib *clib,
        FklVMlib *vlib,
        FklVM *exe,
        FklFuncPrototypes *pts) {
    FklVMvalue *val = FKL_VM_NIL;
    if (clib->type == FKL_CODEGEN_LIB_SCRIPT) {
        FklVMvalue *codeObj = fklCreateVMvalueCodeObj(exe, clib->bcl);
        FklVMvalue *proc =
                fklCreateVMvalueProc(exe, codeObj, clib->prototypeId);
        val = proc;
    } else
        val = fklCreateVMvalueStr2(exe,
                strlen(clib->rp) - strlen(FKL_DLL_FILE_TYPE),
                clib->rp);
    fklInitVMlib(vlib, val, clib->epc);
}

void fklInitVMlibWithCodegenLibMove(FklCodegenLib *clib,
        FklVMlib *vlib,
        FklVM *exe,
        FklFuncPrototypes *pts) {
    FklVMvalue *val = FKL_VM_NIL;
    if (clib->type == FKL_CODEGEN_LIB_SCRIPT) {
        FklVMvalue *codeObj = fklCreateVMvalueCodeObjMove(exe, clib->bcl);
        FklVMvalue *proc =
                fklCreateVMvalueProc(exe, codeObj, clib->prototypeId);
        val = proc;
    } else {
        val = fklCreateVMvalueStr2(exe,
                strlen(clib->rp) - strlen(FKL_DLL_FILE_TYPE),
                clib->rp);
        uv_dlclose(&clib->dll);
    }
    fklInitVMlib(vlib, val, clib->epc);

    fklUninitCodegenLib(clib);
}

static inline void
write_nonterm(const FklGrammerNonterm *nt, const FklGrammer *g, FILE *fp) {
    uint64_t str_len;
    if (nt->group == 0) {
        str_len = 0;
        fwrite(&str_len, sizeof(str_len), 1, fp);
    } else {
        const FklString *str = fklGetSymbolWithId(nt->group, g->st);
        fwrite(str, sizeof(str->size) + str->size, 1, fp);
    }
    if (nt->sid == 0) {
        str_len = 0;
        fwrite(&str_len, sizeof(str_len), 1, fp);
    } else {
        const FklString *str = fklGetSymbolWithId(nt->sid, g->st);
        fwrite(str, sizeof(str->size) + str->size, 1, fp);
    }
}

static inline void write_production_rule_action(const FklSymbolTable *pst,
        const FklGrammerProduction *prod,
        FILE *fp) {
    uint8_t type;
    if (prod->func == custom_action) {
        type = FKL_CODEGEN_PROD_CUSTOM;
        fwrite(&type, sizeof(type), 1, fp);
        struct CustomActionCtx *ctx = prod->ctx;
        fwrite(&ctx->prototype_id, sizeof(ctx->prototype_id), 1, fp);
        fklWriteByteCodelnt(ctx->bcl, fp);
    } else if (is_simple_action(prod->func)) {
        type = FKL_CODEGEN_PROD_SIMPLE;
        fwrite(&type, sizeof(type), 1, fp);
        struct FklSimpleProdAction *a =
                find_simple_prod_action_name(prod->func);
        FKL_ASSERT(a);
        const char *name = a->name;
        uint64_t str_len = strlen(name);
        fwrite(&str_len, sizeof(str_len), 1, fp);
        fputs(name, fp);

        a->write(prod->ctx, pst, fp);
    } else {
        type = FKL_CODEGEN_PROD_BUILTIN;
        fwrite(&type, sizeof(type), 1, fp);
        const char *name = find_builtin_prod_action_name(prod->func);
        FKL_ASSERT(name);
        uint64_t str_len = strlen(name);
        fwrite(&str_len, sizeof(str_len), 1, fp);
        fputs(name, fp);
    }
}

static inline void write_grammer_in_binary(const FklGrammer *g, FILE *fp) {
    FKL_ASSERT(g->start.group == 0 && g->start.sid == 0);
    uint64_t left_count = g->productions.count;
    fwrite(&left_count, sizeof(left_count), 1, fp);
    for (const FklProdHashMapNode *cur = g->productions.first; cur;
            cur = cur->next) {
        FKL_ASSERT(!(cur->k.group == 0 && cur->k.sid == 0));

        {
            uint64_t prod_count = 0;
            for (const FklGrammerProduction *prod = cur->v; prod;
                    prod = prod->next, ++prod_count)
                ;
            fwrite(&prod_count, sizeof(prod_count), 1, fp);
        }

        write_nonterm(&cur->k, g, fp);

        for (const FklGrammerProduction *prod = cur->v; prod;
                prod = prod->next) {
            uint64_t prod_len = prod->len;
            fwrite(&prod_len, sizeof(prod_len), 1, fp);
            for (size_t i = 0; i < prod_len; ++i) {
                const FklGrammerSym *cur = &prod->syms[i];
                fwrite(&cur->type, 1, 1, fp);
                switch (cur->type) {
                case FKL_TERM_NONTERM:
                    write_nonterm(&cur->nt, g, fp);
                    break;
                case FKL_TERM_STRING:
                case FKL_TERM_KEYWORD: {
                    const FklString *t = cur->str;
                    fwrite(t, sizeof(t->size) + t->size, 1, fp);
                } break;

                case FKL_TERM_REGEX: {
                    const FklString *t =
                            fklGetStringWithRegex(&g->regexes, cur->re, NULL);
                    fwrite(t, sizeof(t->size) + t->size, 1, fp);
                } break;

                case FKL_TERM_BUILTIN: {
                    uint64_t str_len = strlen(cur->b.t->name);
                    fwrite(&str_len, sizeof(str_len), 1, fp);
                    fputs(cur->b.t->name, fp);

                    uint64_t len = cur->b.len;
                    fwrite(&len, sizeof(len), 1, fp);
                    for (size_t i = 0; i < len; ++i) {
                        const FklString *t = cur->b.args[i];
                        fwrite(t, sizeof(t->size) + t->size, 1, fp);
                    }
                } break;

                case FKL_TERM_IGNORE:
                    break;
                case FKL_TERM_EOF:
                case FKL_TERM_NONE:
                    FKL_UNREACHABLE();
                }
            }

            write_production_rule_action(g->st, prod, fp);
        }
    }

    {
        uint64_t ignore_count = 0;
        for (const FklGrammerIgnore *ig = g->ignores; ig;
                ig = ig->next, ++ignore_count)
            ;
        fwrite(&ignore_count, sizeof(ignore_count), 1, fp);
    }

    for (const FklGrammerIgnore *ig = g->ignores; ig; ig = ig->next) {
        uint64_t len = ig->len;
        fwrite(&len, sizeof(len), 1, fp);
        for (uint64_t i = 0; i < len; ++i) {
            const FklGrammerIgnoreSym *cur = &ig->ig[i];
            fwrite(&cur->term_type, 1, 1, fp);
            switch (cur->term_type) {

            case FKL_TERM_STRING: {
                const FklString *t = cur->str;
                fwrite(t, sizeof(t->size) + t->size, 1, fp);
            } break;

            case FKL_TERM_REGEX: {
                const FklString *t =
                        fklGetStringWithRegex(&g->regexes, cur->re, NULL);
                fwrite(t, sizeof(t->size) + t->size, 1, fp);
            } break;

            case FKL_TERM_BUILTIN: {
                uint64_t str_len = strlen(cur->b.t->name);
                fwrite(&str_len, sizeof(str_len), 1, fp);
                fputs(cur->b.t->name, fp);

                uint64_t len = cur->b.len;
                fwrite(&len, sizeof(len), 1, fp);
                for (size_t i = 0; i < len; ++i) {
                    const FklString *t = cur->b.args[i];
                    fwrite(t, sizeof(t->size) + t->size, 1, fp);
                }
            } break;

            case FKL_TERM_NONTERM:
            case FKL_TERM_KEYWORD:
            case FKL_TERM_IGNORE:
            case FKL_TERM_EOF:
            case FKL_TERM_NONE:
                FKL_UNREACHABLE();
            }
        }
    }
}

void fklWriteExportNamedProds(const FklSidHashSet *export_named_prod_groups,
        const FklGraProdGroupHashMap *named_prod_groups,
        const FklSymbolTable *st,
        FILE *fp) {
    uint8_t has_named_prod = export_named_prod_groups->count > 0;
    fwrite(&has_named_prod, sizeof(has_named_prod), 1, fp);
    if (!has_named_prod)
        return;
    fwrite(&export_named_prod_groups->count,
            sizeof(export_named_prod_groups->count),
            1,
            fp);
    for (FklSidHashSetNode *list = export_named_prod_groups->first; list;
            list = list->next) {
        FklSid_t id = list->k;
        FklGrammerProdGroupItem *group =
                get_production_group(named_prod_groups, id);
        FKL_ASSERT(group);
        fwrite(&id, sizeof(id), 1, fp);
        fklWriteStringTable(&group->delimiters, fp);
        write_grammer_in_binary(&group->g, fp);
    }
}

void fklWriteNamedProds(const FklGraProdGroupHashMap *named_prod_groups,
        const FklSymbolTable *st,
        FILE *fp) {
    uint8_t has_named_prod = named_prod_groups->buckets != NULL;
    fwrite(&has_named_prod, sizeof(has_named_prod), 1, fp);
    if (!has_named_prod)
        return;
    fwrite(&named_prod_groups->count, sizeof(named_prod_groups->count), 1, fp);
    for (FklGraProdGroupHashMapNode *list = named_prod_groups->first; list;
            list = list->next) {
        fwrite(&list->k, sizeof(list->k), 1, fp);
        fklWriteStringTable(&list->v.delimiters, fp);
        write_grammer_in_binary(&list->v.g, fp);
    }
}

static inline void
load_nonterm(FklGrammerNonterm *nt, FklGrammer *g, FILE *fp) {
    uint64_t str_len;
    fread(&str_len, sizeof(str_len), 1, fp);
    if (str_len == 0) {
        nt->group = 0;
    } else {
        FklString *str = fklCreateString(str_len, NULL);
        fread(str->str, str_len, 1, fp);
        nt->group = fklAddSymbol(str, g->st)->v;
        fklZfree(str);
    }

    fread(&str_len, sizeof(str_len), 1, fp);
    if (str_len == 0) {
        nt->sid = 0;
    } else {
        FklString *str = fklCreateString(str_len, NULL);
        fread(str->str, str_len, 1, fp);
        nt->sid = fklAddSymbol(str, g->st)->v;
        fklZfree(str);
    }
}

static inline void read_production_rule_action(FklSymbolTable *pst,
        FklGrammerProduction *prod,
        FILE *fp) {
    uint8_t type = 255;
    fread(&type, sizeof(type), 1, fp);
    FKL_ASSERT(type != 255);
    switch ((FklCodegenProdActionType)type) {
    case FKL_CODEGEN_PROD_BUILTIN: {
        FklString *name = fklLoadString(fp);
        prod->func = fklFindBuiltinProdActionByName(name->str);
        FKL_ASSERT(prod->func);
        fklZfree(name);
    } break;

    case FKL_CODEGEN_PROD_CUSTOM: {
        prod->func = custom_action;
        struct CustomActionCtx *ctx = (struct CustomActionCtx *)fklZcalloc(1,
                sizeof(struct CustomActionCtx));
        FKL_ASSERT(ctx);
        fread(&ctx->prototype_id, sizeof(ctx->prototype_id), 1, fp);
        ctx->bcl = fklLoadByteCodelnt(fp);
        prod->ctx = ctx;
    } break;

    case FKL_CODEGEN_PROD_SIMPLE: {
        FklString *name = fklLoadString(fp);
        const FklSimpleProdAction *action =
                fklFindSimpleProdActionByName(name->str);
        FKL_ASSERT(action);
        prod->func = action->func;
        prod->ctx_destroyer = action->ctx_destroy;
        prod->ctx_copyer = action->ctx_copyer;
        prod->ctx = action->read(pst, fp);
        fklZfree(name);
    } break;
    }
}

static inline void load_grammer_in_binary(FklGrammer *g, FILE *fp) {
    uint64_t left_count = 0;
    fread(&left_count, sizeof(left_count), 1, fp);
    for (uint64_t i = 0; i < left_count; ++i) {
        FklGrammerNonterm nt;
        uint64_t prod_count = 0;
        fread(&prod_count, sizeof(prod_count), 1, fp);

        load_nonterm(&nt, g, fp);
        FKL_ASSERT(!(nt.group == 0 && nt.sid == 0));

        for (uint64_t i = 0; i < prod_count; ++i) {
            uint64_t prod_len;
            fread(&prod_len, sizeof(prod_len), 1, fp);
            FklGrammerProduction *prod = fklCreateEmptyProduction(nt.group,
                    nt.sid,
                    prod_len,
                    NULL,
                    NULL,
                    NULL,
                    fklProdCtxDestroyDoNothing,
                    fklProdCtxCopyerDoNothing);

            FklGrammerSym *syms = prod->syms;

            for (size_t i = 0; i < prod_len; ++i) {
                uint8_t type;
                fread(&type, sizeof(type), 1, fp);
                FklGrammerSym *cur = &syms[i];
                cur->type = type;
                switch (cur->type) {
                case FKL_TERM_NONTERM:
                    load_nonterm(&cur->nt, g, fp);
                    break;

                case FKL_TERM_STRING:
                case FKL_TERM_KEYWORD: {
                    FklString *str = fklLoadString(fp);
                    cur->str = fklAddString(&g->terminals, str);
                    if (type == FKL_TERM_STRING)
                        fklAddString(&g->delimiters, str);
                    fklZfree(str);
                } break;

                case FKL_TERM_BUILTIN: {
                    FklString *str = fklLoadString(fp);
                    FklSid_t id = fklAddSymbol(str, g->st)->v;
                    const FklLalrBuiltinMatch *t =
                            fklGetBuiltinMatch(&g->builtins, id);
                    FKL_ASSERT(t);
                    cur->b.t = t;

                    uint64_t len = 0;
                    fread(&len, sizeof(len), 1, fp);
                    cur->b.len = len;
                    cur->b.args = fklZmalloc(len * sizeof(FklString *));
                    FKL_ASSERT(cur->b.args);
                    for (size_t i = 0; i < len; ++len) {
                        FklString *s = fklLoadString(fp);
                        cur->b.args[i] = fklAddString(&g->terminals, s);
                        fklAddString(&g->delimiters, s);
                        fklZfree(s);
                    }
                    fklZfree(str);
                } break;

                case FKL_TERM_REGEX: {
                    FklString *s = fklLoadString(fp);
                    cur->re = fklAddRegexStr(&g->regexes, s);
                    FKL_ASSERT(cur->re);
                    fklZfree(s);
                } break;

                case FKL_TERM_IGNORE:
                    break;
                case FKL_TERM_NONE:
                case FKL_TERM_EOF:
                    FKL_UNREACHABLE();
                    break;
                }
            }

            read_production_rule_action(g->st, prod, fp);
            if (fklAddProdToProdTableNoRepeat(g, prod)) {
                FKL_UNREACHABLE();
            }
        }
    }

    uint64_t ignore_count = 0;
    fread(&ignore_count, sizeof(ignore_count), 1, fp);
    for (size_t i = 0; i < ignore_count; ++i) {
        uint64_t len = 0;
        fread(&len, sizeof(len), 1, fp);
        FklGrammerIgnore *ig = fklCreateEmptyGrammerIgnore(len);
        for (uint64_t i = 0; i < len; ++i) {
            FklGrammerIgnoreSym *cur = &ig->ig[i];
            fread(&cur->term_type, 1, 1, fp);
            switch (cur->term_type) {
            case FKL_TERM_STRING: {
                FklString *str = fklLoadString(fp);
                cur->str = fklAddString(&g->terminals, str);
                fklAddString(&g->delimiters, str);
                fklZfree(str);
            } break;
            case FKL_TERM_REGEX: {
                FklString *str = fklLoadString(fp);
                cur->re = fklAddRegexStr(&g->regexes, str);
                FKL_ASSERT(cur->re);
                fklZfree(str);
            } break;

            case FKL_TERM_BUILTIN: {
                FklString *str = fklLoadString(fp);
                FklSid_t id = fklAddSymbol(str, g->st)->v;
                const FklLalrBuiltinMatch *t =
                        fklGetBuiltinMatch(&g->builtins, id);
                FKL_ASSERT(t);
                cur->b.t = t;

                uint64_t len = 0;
                fread(&len, sizeof(len), 1, fp);
                cur->b.len = len;
                cur->b.args = fklZmalloc(len * sizeof(FklString *));
                FKL_ASSERT(cur->b.args);
                for (size_t i = 0; i < len; ++len) {
                    FklString *s = fklLoadString(fp);
                    cur->b.args[i] = fklAddString(&g->terminals, s);
                    fklAddString(&g->delimiters, s);
                    fklZfree(s);
                }
                fklZfree(str);
            } break;

            case FKL_TERM_NONTERM:
            case FKL_TERM_KEYWORD:
            case FKL_TERM_IGNORE:
            case FKL_TERM_EOF:
            case FKL_TERM_NONE:
                FKL_UNREACHABLE();
            }
        }

        if (fklAddIgnoreToIgnoreList(&g->ignores, ig)) {
            fklDestroyIgnore(ig);
        }
    }
}

void fklLoadNamedProds(FklGraProdGroupHashMap *ht,
        FklSymbolTable *st,
        FklCodegenOuterCtx *outer_ctx,
        FILE *fp)

{
    uint8_t has_named_prod = 0;
    fread(&has_named_prod, sizeof(has_named_prod), 1, fp);
    if (has_named_prod) {
        fklGraProdGroupHashMapInit(ht);
        uint32_t num = 0;
        fread(&num, sizeof(num), 1, fp);
        for (; num > 0; num--) {
            FklSid_t group_id = 0;
            fread(&group_id, sizeof(group_id), 1, fp);
            FklGrammerProdGroupItem *group = add_production_group(ht,
                    &outer_ctx->public_symbol_table,
                    group_id);
            fklLoadStringTable(fp, &group->delimiters);
            load_grammer_in_binary(&group->g, fp);
        }
    }
}

void fklRecomputeSidForNamedProdGroups(FklGraProdGroupHashMap *ht,
        const FklSymbolTable *origin_st,
        FklSymbolTable *target_st,
        const FklConstTable *origin_kt,
        FklConstTable *target_kt,
        FklCodegenRecomputeNastSidOption option) {
    if (ht && ht->buckets) {
        for (FklGraProdGroupHashMapNode *list = ht->first; list;
                list = list->next) {
            replace_sid(FKL_REMOVE_CONST(FklSid_t, &list->k),
                    origin_st,
                    target_st);
            *FKL_REMOVE_CONST(uintptr_t, &list->hashv) =
                    fklGraProdGroupHashMap__hashv(&list->k);

            for (FklProdHashMapNode *cur = list->v.g.productions.first; cur;
                    cur = cur->next) {
                for (FklGrammerProduction *prod = cur->v; prod;
                        prod = prod->next) {
                    if (prod->func == custom_action) {
                        struct CustomActionCtx *ctx = prod->ctx;
                        if (!ctx->is_recomputed) {
                            fklRecomputeSidAndConstIdForBcl(ctx->bcl,
                                    origin_st,
                                    target_st,
                                    origin_kt,
                                    target_kt);
                            ctx->is_recomputed = 1;
                        }
                    }
                }
            }
        }
        fklGraProdGroupHashMapRehash(ht);
    }
}

void fklInitPreLibReaderMacros(FklCodegenLibVector *libraries,
        FklSymbolTable *st,
        FklCodegenOuterCtx *outer_ctx,
        FklFuncPrototypes *pts,
        FklCodegenLibVector *macro_libraries) {
    uint32_t top = libraries->size;
    for (uint32_t i = 0; i < top; i++) {
        FklCodegenLib *lib = &libraries->base[i];
        if (lib->named_prod_groups.buckets) {
            for (FklGraProdGroupHashMapNode *l = lib->named_prod_groups.first;
                    l;
                    l = l->next) {

                for (const FklStrHashSetNode *cur = l->v.delimiters.first; cur;
                        cur = cur->next)
                    fklAddString(&l->v.g.delimiters, cur->k);

                for (FklProdHashMapNode *cur = l->v.g.productions.first; cur;
                        cur = cur->next) {
                    for (FklGrammerProduction *prod = cur->v; prod;
                            prod = prod->next) {
                        if (prod->func == custom_action) {
                            struct CustomActionCtx *ctx = prod->ctx;
                            ctx->codegen_outer_ctx = outer_ctx;
                            ctx->pst = &outer_ctx->public_symbol_table;
                            ctx->macro_libraries = macro_libraries;
                            ctx->pts = pts;
                            ctx->is_recomputed = 0;
                            prod->func = custom_action;
                            prod->ctx = ctx;
                            prod->ctx_destroyer = custom_action_ctx_destroy;
                            prod->ctx_copyer = custom_action_ctx_copy;
                        }
                    }
                }
            }
        }
    }
}

void fklPrintReaderMacroAction(FILE *fp,
        const FklGrammerProduction *prod,
        const FklSymbolTable *pst,
        const FklConstTable *pkt) {
    if (prod->func == custom_action) {
        struct CustomActionCtx *ctx = prod->ctx;
        fputs("custom\n", fp);
        fklPrintByteCodelnt(ctx->bcl, fp, pst, pkt);
        fputc('\n', fp);
    } else if (is_simple_action(prod->func)) {
        struct FklSimpleProdAction *a =
                find_simple_prod_action_name(prod->func);
        FKL_ASSERT(a);
        fprintf(fp, "simple %s(", a->name);
        a->print(prod->ctx, pst, fp);
        fputc(')', fp);
    } else {
        const char *name = find_builtin_prod_action_name(prod->func);
        FKL_ASSERT(name);
        fprintf(fp, "builtin %s", name);
    }
}

#define IS_LOAD_DLL_OP(OP)                                                     \
    ((OP) >= FKL_OP_LOAD_DLL && (OP) <= FKL_OP_LOAD_DLL_X)
#define IS_LOAD_LIB_OP(OP)                                                     \
    ((OP) >= FKL_OP_LOAD_LIB && (OP) <= FKL_OP_LOAD_LIB_X)
#define IS_PUSH_PROC_OP(OP)                                                    \
    ((OP) >= FKL_OP_PUSH_PROC && (OP) <= FKL_OP_PUSH_PROC_XXX)
#define IS_EXPORT_TO_OP(OP)                                                    \
    ((OP) >= FKL_OP_EXPORT_TO && (OP) <= FKL_OP_EXPORT_TO_XX)

struct IncreaseBclLibPrototypeCtx {
    uint32_t lib_count;
    uint32_t pts_count;
};

static int increase_bcl_lib_prototype_id_predicate(FklOpcode op) {
    return IS_LOAD_LIB_OP(op) || IS_LOAD_DLL_OP(op) || IS_PUSH_PROC_OP(op)
        || IS_EXPORT_TO_OP(op);
}

static int increase_bcl_lib_prototype_id_func(void *cctx,
        FklOpcode *popcode,
        FklOpcodeMode *pmode,
        FklInstructionArg *ins_arg) {
    struct IncreaseBclLibPrototypeCtx *ctx = cctx;
    FklOpcode op = *popcode;
    if (IS_LOAD_DLL_OP(op)) {
        ins_arg->ux += ctx->lib_count;
        *popcode = FKL_OP_LOAD_DLL;
        *pmode = FKL_OP_MODE_IuB;
    } else if (IS_LOAD_LIB_OP(op)) {
        ins_arg->ux += ctx->lib_count;
        *popcode = FKL_OP_LOAD_LIB;
        *pmode = FKL_OP_MODE_IuB;
    } else if (IS_EXPORT_TO_OP(op)) {
        ins_arg->ux += ctx->lib_count;
        *popcode = FKL_OP_EXPORT_TO;
        *pmode = FKL_OP_MODE_IuAuB;
    } else if (IS_PUSH_PROC_OP(op)) {
        ins_arg->ux += ctx->pts_count;
        *popcode = FKL_OP_PUSH_PROC;
        *pmode = FKL_OP_MODE_IuAuB;
    }
    return 0;
}

static inline void increase_bcl_lib_prototype_id(FklByteCodelnt *bcl,
        uint32_t lib_count,
        uint32_t pts_count) {
    struct IncreaseBclLibPrototypeCtx ctx = {
        .lib_count = lib_count,
        .pts_count = pts_count,
    };
    fklRecomputeInsImm(bcl,
            &ctx,
            increase_bcl_lib_prototype_id_predicate,
            increase_bcl_lib_prototype_id_func);
}

static inline void increase_compiler_macros_lib_prototype_id(
        FklCodegenMacro *head,
        uint32_t macro_lib_count,
        uint32_t macro_pts_count) {
    for (; head; head = head->next) {
        head->prototype_id += macro_pts_count;
        increase_bcl_lib_prototype_id(head->bcl,
                macro_lib_count,
                macro_pts_count);
    }
}

static inline void increase_reader_macro_lib_prototype_id(
        FklGraProdGroupHashMap *named_prod_groups,
        uint32_t lib_count,
        uint32_t count) {
    if (named_prod_groups->buckets) {
        for (FklGraProdGroupHashMapNode *list = named_prod_groups->first; list;
                list = list->next) {

            for (const FklProdHashMapNode *cur = list->v.g.productions.first;
                    cur;
                    cur = cur->next) {
                for (const FklGrammerProduction *prod = cur->v; prod;
                        prod = prod->next) {
                    if (prod->func == custom_action) {
                        struct CustomActionCtx *ctx = prod->ctx;
                        ctx->prototype_id += count;
                        increase_bcl_lib_prototype_id(ctx->bcl,
                                lib_count,
                                count);
                    }
                }
            }
        }
    }
}

void fklIncreaseLibIdAndPrototypeId(FklCodegenLib *lib,
        uint32_t lib_count,
        uint32_t macro_lib_count,
        uint32_t pts_count,
        uint32_t macro_pts_count) {
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        lib->prototypeId += pts_count;
        increase_bcl_lib_prototype_id(lib->bcl, lib_count, pts_count);
        increase_compiler_macros_lib_prototype_id(lib->head,
                macro_lib_count,
                macro_pts_count);
        increase_reader_macro_lib_prototype_id(&lib->named_prod_groups,
                macro_lib_count,
                macro_pts_count);
        break;
    case FKL_CODEGEN_LIB_DLL:
        break;
    case FKL_CODEGEN_LIB_UNINIT:
        FKL_UNREACHABLE();
        break;
    }
}
