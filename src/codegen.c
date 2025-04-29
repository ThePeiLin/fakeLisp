#include <fakeLisp/builtin.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/vm.h>

#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

static inline FklNastNode *cadr_nast_node(const FklNastNode *node) {
    return node->pair->cdr->pair->car;
}

static inline int
isExportDefmacroExp(const FklNastNode *c,
                    FklNastNode *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFMACRO],
                           c, NULL)
        && cadr_nast_node(c)->type != FKL_NAST_BOX;
}

static inline int
isExportDefReaderMacroExp(const FklNastNode *c,
                          FklNastNode *const *builtin_pattern_node) {
    return (fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFMACRO],
                            c, NULL)
            && cadr_nast_node(c)->type == FKL_NAST_BOX)
        || fklPatternMatch(
               builtin_pattern_node[FKL_CODEGEN_PATTERN_DEF_READER_MACROS], c,
               NULL);
}

static inline int isExportDefineExp(const FklNastNode *c,
                                    FklNastNode *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFINE], c,
                           NULL)
        || fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFCONST],
                           c, NULL);
}

static inline int isBeginExp(const FklNastNode *c,
                             FklNastNode *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_BEGIN], c,
                           NULL);
}

static inline int isExportDefunExp(const FklNastNode *c,
                                   FklNastNode *const *builtin_pattern_node) {
    return fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFUN], c,
                           NULL)
        || fklPatternMatch(
               builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFUN_CONST], c, NULL);
}

static inline int isExportImportExp(FklNastNode *c,
                                    FklNastNode *const *builtin_pattern_node) {
    return fklPatternMatch(
               builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_PREFIX], c, NULL)
        || fklPatternMatch(
               builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_ONLY], c, NULL)
        || fklPatternMatch(
               builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_ALIAS], c, NULL)
        || fklPatternMatch(
               builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_EXCEPT], c, NULL)
        || fklPatternMatch(
               builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_COMMON], c, NULL)
        || fklPatternMatch(builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT], c,
                           NULL)
        || fklPatternMatch(
               builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_NONE], c, NULL);
}

static void _default_stack_context_finalizer(void *data) {
    FklByteCodelntVector *s = data;
    while (!fklByteCodelntVectorIsEmpty(s))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBack(s));
    fklByteCodelntVectorDestroy(s);
}

static void _default_stack_context_put_bcl(void *data, FklByteCodelnt *bcl) {
    FklByteCodelntVector *s = data;
    fklByteCodelntVectorPushBack2(s, bcl);
}

static FklByteCodelntVector *_default_stack_context_get_bcl_stack(void *data) {
    return data;
}

static const FklCodegenQuestContextMethodTable DefaultStackContextMethodTable =
    {
        .__finalizer = _default_stack_context_finalizer,
        .__put_bcl = _default_stack_context_put_bcl,
        .__get_bcl_stack = _default_stack_context_get_bcl_stack,
};

static FklCodegenQuestContext *
createCodegenQuestContext(void *data,
                          const FklCodegenQuestContextMethodTable *t) {
    FklCodegenQuestContext *r =
        (FklCodegenQuestContext *)malloc(sizeof(FklCodegenQuestContext));
    FKL_ASSERT(r);
    r->data = data;
    r->t = t;
    return r;
}

static FklCodegenQuestContext *
createDefaultStackContext(FklByteCodelntVector *s) {
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

static FklCodegenNextExpression *
createCodegenNextExpression(const FklNextExpressionMethodTable *t,
                            void *context, uint8_t must_has_retval) {
    FklCodegenNextExpression *r =
        (FklCodegenNextExpression *)malloc(sizeof(FklCodegenNextExpression));
    FKL_ASSERT(r);
    r->t = t;
    r->context = context;
    r->must_has_retval = must_has_retval;
    return r;
}

static FklNastNode *
_default_codegen_get_next_expression(void *context,
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

static FklCodegenNextExpression *
createDefaultQueueNextExpression(FklNastNodeQueue *queue) {
    return createCodegenNextExpression(
        &_default_codegen_next_expression_method_table, queue,
        DO_NOT_NEED_RETVAL);
}

static FklCodegenNextExpression *
createMustHasRetvalQueueNextExpression(FklNastNodeQueue *queue) {
    return createCodegenNextExpression(
        &_default_codegen_next_expression_method_table, queue,
        ALL_MUST_HAS_RETVAL);
}

static FklCodegenNextExpression *
createFirstHasRetvalQueueNextExpression(FklNastNodeQueue *queue) {
    return createCodegenNextExpression(
        &_default_codegen_next_expression_method_table, queue,
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

static inline int set_ins_with_unsigned_imm(FklInstruction *ins, FklOpcode op,
                                            uint64_t k) {
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

static inline FklByteCodelnt *
set_and_append_ins_with_unsigned_imm(InsAppendMode m, FklByteCodelnt *bcl,
                                     FklOpcode op, uint64_t k, FklSid_t fid,
                                     uint32_t line, uint32_t scope) {
    FklInstruction ins[3] = {FKL_INSTRUCTION_STATIC_INIT};
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

static inline int set_ins_with_2_unsigned_imm(FklInstruction *ins, FklOpcode op,
                                              uint64_t ux, uint64_t uy) {
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
    InsAppendMode m, FklByteCodelnt *bcl, FklOpcode op, uint64_t ux,
    uint64_t uy, FklSid_t fid, uint32_t line, uint32_t scope) {
    FklInstruction ins[4] = {FKL_INSTRUCTION_STATIC_INIT};
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

static inline int set_ins_with_signed_imm(FklInstruction *ins, FklOpcode op,
                                          int64_t k) {
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

static inline FklByteCodelnt *
set_and_append_ins_with_signed_imm(InsAppendMode m, FklByteCodelnt *bcl,
                                   FklOpcode op, int64_t k, FklSid_t fid,
                                   uint32_t line, uint32_t scope) {
    FklInstruction ins[3] = {FKL_INSTRUCTION_STATIC_INIT};
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

static inline FklByteCode *
set_and_append_ins_with_unsigned_imm_to_bc(InsAppendMode m, FklByteCode *bc,
                                           FklOpcode op, uint64_t k) {
    FklInstruction ins[3] = {FKL_INSTRUCTION_STATIC_INIT};
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
    return set_and_append_ins_with_unsigned_imm_to_bc(m, bc, FKL_OP_PUSH_SYM,
                                                      sid);
}

static inline FklByteCode *append_push_chr_ins_to_bc(InsAppendMode m,
                                                     FklByteCode *bc, char ch) {
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
    return set_and_append_ins_with_unsigned_imm_to_bc(
        m, bc, FKL_OP_PUSH_STR, fklAddStrConst(info->runtime_kt, k));
}

static inline FklByteCode *append_push_i64_ins_to_bc(InsAppendMode m,
                                                     FklByteCode *bc, int64_t k,
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
        bc = set_and_append_ins_with_unsigned_imm_to_bc(
            m, bc,
            (k >= FKL_FIX_INT_MIN && k <= FKL_FIX_INT_MAX) ? FKL_OP_PUSH_I64F
                                                           : FKL_OP_PUSH_I64B,
            id);
    }
    return bc;
}

static inline FklByteCodelnt *
append_push_i64_ins(InsAppendMode m, FklByteCodelnt *bcl, int64_t k,
                    FklCodegenInfo *info, FklSid_t fid, uint32_t line,
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
        bcl = set_and_append_ins_with_unsigned_imm(
            m, bcl,
            (k >= FKL_FIX_INT_MIN && k <= FKL_FIX_INT_MAX) ? FKL_OP_PUSH_I64F
                                                           : FKL_OP_PUSH_I64B,
            fklAddI64Const(info->runtime_kt, k), fid, line, scope);
    }
    return bcl;
}

static inline FklByteCode *append_push_bigint_ins_to_bc(InsAppendMode m,
                                                        FklByteCode *bc,
                                                        const FklBigInt *k,
                                                        FklCodegenInfo *info) {
    if (fklIsBigIntGtLtI64(k))
        return set_and_append_ins_with_unsigned_imm_to_bc(
            m, bc, FKL_OP_PUSH_BI, fklAddBigIntConst(info->runtime_kt, k));
    else
        return set_and_append_ins_with_unsigned_imm_to_bc(
            m, bc, FKL_OP_PUSH_I64B,
            fklAddI64Const(info->runtime_kt, fklBigIntToI(k)));
}

static inline FklByteCode *append_push_bvec_ins_to_bc(InsAppendMode m,
                                                      FklByteCode *bc,
                                                      const FklBytevector *k,
                                                      FklCodegenInfo *info) {
    return set_and_append_ins_with_unsigned_imm_to_bc(
        m, bc, FKL_OP_PUSH_BVEC, fklAddBvecConst(info->runtime_kt, k));
}

static inline FklByteCode *append_push_f64_ins_to_bc(InsAppendMode m,
                                                     FklByteCode *bc, double k,
                                                     FklCodegenInfo *info) {
    return set_and_append_ins_with_unsigned_imm_to_bc(
        m, bc, FKL_OP_PUSH_F64, fklAddF64Const(info->runtime_kt, k));
}

static inline FklByteCode *
append_push_list_ins_to_bc(InsAppendMode m, FklByteCode *bc, uint64_t len) {
    return set_and_append_ins_with_unsigned_imm_to_bc(m, bc, FKL_OP_PUSH_LIST,
                                                      len);
}

static inline FklByteCode *
append_push_vec_ins_to_bc(InsAppendMode m, FklByteCode *bc, uint64_t len) {
    return set_and_append_ins_with_unsigned_imm_to_bc(m, bc, FKL_OP_PUSH_VEC,
                                                      len);
}

static inline FklByteCode *append_push_hash_ins_to_bc(InsAppendMode m,
                                                      FklByteCode *bc,
                                                      FklHashTableEqType type,
                                                      uint64_t len) {
    switch (type) {
    case FKL_HASH_EQ:
        return set_and_append_ins_with_unsigned_imm_to_bc(
            m, bc, FKL_OP_PUSH_HASHEQ, len);
        break;
    case FKL_HASH_EQV:
        return set_and_append_ins_with_unsigned_imm_to_bc(
            m, bc, FKL_OP_PUSH_HASHEQV, len);
        break;
    case FKL_HASH_EQUAL:
        return set_and_append_ins_with_unsigned_imm_to_bc(
            m, bc, FKL_OP_PUSH_HASHEQUAL, len);
        break;
    }
    return bc;
}

static inline FklByteCodelnt *
append_push_proc_ins(InsAppendMode m, FklByteCodelnt *bcl, uint32_t prototypeId,
                     uint64_t len, FklSid_t fid, uint32_t line,
                     uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(
        m, bcl, FKL_OP_PUSH_PROC, prototypeId, len, fid, line, scope);
}

static inline FklByteCodelnt *
append_jmp_ins(InsAppendMode m, FklByteCodelnt *bcl, int64_t len,
               JmpInsCondition condition, JmpInsOrientation orien, FklSid_t fid,
               uint32_t line, uint32_t scope) {
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
        return set_and_append_ins_with_signed_imm(m, bcl, op, len, fid, line,
                                                  scope);
        break;
    case JMP_BACKWARD:
        len = -len - 1;
        if (len >= FKL_I24_MIN)
            return set_and_append_ins_with_signed_imm(m, bcl, op, len, fid,
                                                      line, scope);
        len -= 1;
        if (len >= INT32_MIN)
            return set_and_append_ins_with_signed_imm(m, bcl, op, len, fid,
                                                      line, scope);
        len -= 1;
        return set_and_append_ins_with_signed_imm(m, bcl, op, len, fid, line,
                                                  scope);
        break;
    }
    return bcl;
}

static inline FklByteCodelnt *
append_import_ins(InsAppendMode m, FklByteCodelnt *bcl, uint32_t idx1,
                  uint32_t idx, FklSid_t fid, uint32_t line, uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(m, bcl, FKL_OP_IMPORT, idx,
                                                  idx1, fid, line, scope);
}

static inline FklByteCodelnt *
append_load_lib_ins(InsAppendMode m, FklByteCodelnt *bcl, uint32_t lib_id,
                    FklSid_t fid, uint32_t line, uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m, bcl, FKL_OP_LOAD_LIB, lib_id,
                                                fid, line, scope);
}

static inline FklByteCodelnt *
append_load_dll_ins(InsAppendMode m, FklByteCodelnt *bcl, uint32_t lib_id,
                    FklSid_t fid, uint32_t line, uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m, bcl, FKL_OP_LOAD_DLL, lib_id,
                                                fid, line, scope);
}

static inline FklByteCodelnt *append_export_to_ins(InsAppendMode m,
                                                   FklByteCodelnt *bcl,
                                                   uint32_t libId, uint32_t num,
                                                   FklSid_t fid, uint32_t line,
                                                   uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(m, bcl, FKL_OP_EXPORT_TO,
                                                  libId, num, fid, line, scope);
}

static inline FklByteCodelnt *append_export_ins(InsAppendMode m,
                                                FklByteCodelnt *bcl,
                                                uint32_t idx, FklSid_t fid,
                                                uint32_t line, uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m, bcl, FKL_OP_EXPORT, idx, fid,
                                                line, scope);
}

static inline FklInstruction create_op_ins(FklOpcode op) {
    return (FklInstruction){.op = op};
}

static inline FklByteCodelnt *create_bc_lnt(FklByteCode *bc, FklSid_t fid,
                                            uint32_t line, uint32_t scope) {
    FklByteCodelnt *r = fklCreateByteCodelnt(bc);
    r->ls = 1;
    r->l = (FklLineNumberTableItem *)malloc(sizeof(FklLineNumberTableItem));
    FKL_ASSERT(r->l);
    fklInitLineNumTabNode(&r->l[0], fid, 0, line, scope);
    return r;
}

// static inline FklByteCodelnt *
// append_pop_arg_ins(InsAppendMode m, FklByteCodelnt *bcl, uint32_t idx,
//                    FklSid_t fid, uint32_t line, uint32_t scope) {
//     return set_and_append_ins_with_unsigned_imm(m, bcl, FKL_OP_POP_ARG, idx,
//                                                 fid, line, scope);
// }

static inline FklByteCodelnt *
append_pop_loc_ins(InsAppendMode m, FklByteCodelnt *bcl, uint32_t idx,
                   FklSid_t fid, uint32_t line, uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m, bcl, FKL_OP_POP_LOC, idx,
                                                fid, line, scope);
}

// static inline FklByteCodelnt *
// append_pop_rest_arg_ins(InsAppendMode m, FklByteCodelnt *bcl, uint32_t idx,
//                         FklSid_t fid, uint32_t line, uint32_t scope) {
//     return set_and_append_ins_with_unsigned_imm(m, bcl, FKL_OP_POP_REST_ARG,
//                                                 idx, fid, line, scope);
// }

static inline FklByteCodelnt *
append_put_loc_ins(InsAppendMode m, FklByteCodelnt *bcl, uint32_t idx,
                   FklSid_t fid, uint32_t line, uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m, bcl, FKL_OP_PUT_LOC, idx,
                                                fid, line, scope);
}

static inline FklByteCodelnt *
append_put_var_ref_ins(InsAppendMode m, FklByteCodelnt *bcl, uint32_t idx,
                       FklSid_t fid, uint32_t line, uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m, bcl, FKL_OP_PUT_VAR_REF, idx,
                                                fid, line, scope);
}

static inline FklCodegenMacroScope *
make_macro_scope_ref(FklCodegenMacroScope *s) {
    if (s)
        s->refcount++;
    return s;
}

FklCodegenQuest *
fklCreateCodegenQuest(FklByteCodeProcesser f, FklCodegenQuestContext *context,
                      FklCodegenNextExpression *nextExpression, uint32_t scope,
                      FklCodegenMacroScope *macroScope, FklCodegenEnv *env,
                      uint64_t curline, FklCodegenQuest *prev,
                      FklCodegenInfo *codegen) {
    FklCodegenQuest *r = (FklCodegenQuest *)malloc(sizeof(FklCodegenQuest));
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
        free(nextExpression);
    }
    fklDestroyCodegenEnv(quest->env);
    fklDestroyCodegenMacroScope(quest->macroScope);
    quest->context->t->__finalizer(quest->context->data);
    free(quest->context);
    fklDestroyCodegenInfo(quest->codegen);
    free(quest);
}

#define FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(F, STACK, NEXT_EXPRESSIONS,    \
                                                SCOPE, MACRO_SCOPE, ENV, LINE, \
                                                CODEGEN, CODEGEN_CONTEXT)      \
    fklCodegenQuestVectorPushBack2(                                            \
        (CODEGEN_CONTEXT),                                                     \
        fklCreateCodegenQuest((F), (STACK), (NEXT_EXPRESSIONS), (SCOPE),       \
                              (MACRO_SCOPE), (ENV), (LINE), NULL, (CODEGEN)))

#define BC_PROCESS(NAME)                                                       \
    static FklByteCodelnt *NAME(                                               \
        FklCodegenInfo *codegen, FklCodegenEnv *env, uint32_t scope,           \
        FklCodegenMacroScope *cms, FklCodegenQuestContext *context,            \
        FklSid_t fid, uint64_t line, FklCodegenErrorState *errorState,         \
        FklCodegenOuterCtx *outer_ctx)

#define GET_STACK(CONTEXT) ((CONTEXT)->t->__get_bcl_stack((CONTEXT)->data))
BC_PROCESS(_default_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    if (!fklByteCodelntVectorIsEmpty(stack))
        return *fklByteCodelntVectorPopBack(stack);
    else
        return NULL;
}

BC_PROCESS(_empty_bc_process) { return NULL; }

static FklByteCodelnt *sequnce_exp_bc_process(FklByteCodelntVector *stack,
                                              FklSid_t fid, uint32_t line,
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
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL), fid,
                                       line, scope);
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

static inline FklBuiltinInlineFunc
is_inlinable_func_ref(const FklByteCode *bc, FklCodegenEnv *env,
                      uint32_t argNum, FklCodegenInfo *codegen) {
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
            && (inlFunc =
                    is_inlinable_func_ref(funcBc, env, argNum, codegen))) {
            fklDestroyByteCodelnt(func);
            stack->size = 0;
            return inlFunc((FklByteCodelnt **)stack->base + 1, fid, line,
                           scope);
        } else {
            FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));
            FklByteCodelnt **base = stack->base;
            FklByteCodelnt **const end = base + stack->size;
            for (; base < end; ++base) {
                // while (stack->size > 1) {
                // FklByteCodelnt *cur = *fklByteCodelntVectorPopBack(stack);
                FklByteCodelnt *cur = *base;
                fklCodeLntConcat(retval, cur);
                fklDestroyByteCodelnt(cur);
            }
            stack->size = 0;
            FklInstruction setBp = create_op_ins(FKL_OP_SET_BP);
            FklInstruction call = create_op_ins(FKL_OP_CALL);
            fklByteCodeLntInsertFrontIns(&setBp, retval, fid, line, scope);
            // fklCodeLntConcat(retval, func);
            fklByteCodeLntPushBackIns(retval, &call, fid, line, scope);
            return retval;
        }
    } else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL), fid,
                                       line, scope);
}

static int pushFuncallListToQueue(FklNastNode *list, FklNastNodeQueue *queue,
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
                            uint32_t scope, FklCodegenMacroScope *macroScope,
                            FklCodegenEnv *env, FklCodegenInfo *codegen,
                            FklCodegenErrorState *errorState) {
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    FklNastNode *last = NULL;
    int r = pushFuncallListToQueue(rest, queue, &last,
                                   codegen->outer_ctx->builtin_pattern_node);
    if (r || last->type != FKL_NAST_NIL) {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(rest);
        while (!fklNastNodeQueueIsEmpty(queue))
            fklDestroyNastNode(*fklNastNodeQueuePop(queue));
        fklNastNodeQueueDestroy(queue);
    } else
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            _funcall_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(32)),
            createMustHasRetvalQueueNextExpression(queue), scope, macroScope,
            env, rest->curline, codegen, codegenQuestStack);
}

#define CODEGEN_ARGS                                                           \
    FklNastNode *origExp, FklPmatchHashMap *ht,                                \
        FklCodegenQuestVector *codegenQuestStack, uint32_t scope,              \
        FklCodegenMacroScope *macroScope, FklCodegenEnv *curEnv,               \
        FklCodegenInfo *codegen, FklCodegenOuterCtx *outer_ctx,                \
        FklCodegenErrorState *errorState, uint8_t must_has_retval

#define CODEGEN_FUNC(NAME) void NAME(CODEGEN_ARGS)

static CODEGEN_FUNC(codegen_begin) {
    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _begin_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(32)),
        createDefaultQueueNextExpression(queue), scope, macroScope, curEnv,
        rest->curline, codegen, codegenQuestStack);
}

static inline uint32_t enter_new_scope(uint32_t p, FklCodegenEnv *env) {
    uint32_t r = ++env->sc;
    FklCodegenEnvScope *scopes = (FklCodegenEnvScope *)fklRealloc(
        env->scopes, r * sizeof(FklCodegenEnvScope));
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

static inline void process_unresolve_ref(FklCodegenEnv *env, uint32_t scope,
                                         FklFuncPrototypes *pts) {
    FklUnReSymbolRefVector *urefs = &env->uref;
    FklFuncPrototype *pa = pts->pa;
    FklUnReSymbolRefVector urefs1;
    uint32_t count = urefs->size;

    fklUnReSymbolRefVectorInit(&urefs1, count);
    for (uint32_t i = 0; i < count; i++) {
        FklUnReSymbolRef *uref = &urefs->base[i];
        if (uref->scope < scope) {
            // 忽略来自父作用域的未解决引用
            fklUnReSymbolRefVectorPushBack(&urefs1, uref);
            continue;
        }
        FklFuncPrototype *cpt = &pa[uref->prototypeId];
        FklSymDefHashMapMutElm *ref = &cpt->refs[uref->idx];
        FklSymDefHashMapElm *def =
            fklFindSymbolDefByIdAndScope(uref->id, uref->scope, env);
        if (def) {
            env->slotFlags[def->v.idx] = FKL_CODEGEN_ENV_SLOT_REF;
            ref->v.cidx = def->v.idx;
            ref->v.isLocal = 1;
        } else if (env->scopes[uref->scope - 1].p) {
            uref->scope = env->scopes[uref->scope - 1].p;
            fklUnReSymbolRefVectorPushBack(&urefs1, uref);
        } else if (env->prev) {
            ref->v.cidx = fklAddCodegenRefBySidRetIndex(
                uref->id, env, uref->fid, uref->line, uref->assign);
        } else {
            fklAddCodegenBuiltinRefBySid(uref->id, env);
            fklUnReSymbolRefVectorPushBack(&urefs1, uref);
        }
    }
    urefs->size = 0;
    while (!fklUnReSymbolRefVectorIsEmpty(&urefs1))
        fklUnReSymbolRefVectorPushBack2(
            urefs, *fklUnReSymbolRefVectorPopBack(&urefs1));
    fklUnReSymbolRefVectorUninit(&urefs1);
}

static inline int reset_flag_and_check_var_be_refed(
    uint8_t *flags, FklCodegenEnvScope *sc, uint32_t scope, FklCodegenEnv *env,
    FklFuncPrototypes *cp, uint32_t *start, uint32_t *end) {
    process_unresolve_ref(env, scope, cp);
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

static inline FklByteCodelnt *
append_close_ref_ins(InsAppendMode m, FklByteCodelnt *retval, uint32_t s,
                     uint32_t e, FklSid_t fid, uint32_t line, uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(m, retval, FKL_OP_CLOSE_REF,
                                                  s, e, fid, line, scope);
}

static inline void check_and_close_ref(FklByteCodelnt *retval, uint32_t scope,
                                       FklCodegenEnv *env,
                                       FklFuncPrototypes *pa, FklSid_t fid,
                                       uint32_t line) {
    FklCodegenEnvScope *cur = &env->scopes[scope - 1];
    uint32_t start = cur->start;
    uint32_t end = start + 1;
    if (reset_flag_and_check_var_be_refed(env->slotFlags, cur, scope, env, pa,
                                          &start, &end))
        append_close_ref_ins(INS_APPEND_BACK, retval, start, end, fid, line,
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
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _local_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(32)),
        createDefaultQueueNextExpression(queue), cs, cms, curEnv, rest->curline,
        codegen, codegenQuestStack);
}

static CODEGEN_FUNC(codegen_let0) {
    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    uint32_t cs = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
    pushListItemToQueue(rest, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _local_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(32)),
        createDefaultQueueNextExpression(queue), cs, cms, curEnv, rest->curline,
        codegen, codegenQuestStack);
}

typedef struct Let1Context {
    FklByteCodelntVector stack;
    FklSidVector *ss;
} Let1Context;

static inline Let1Context *createLet1Context(FklSidVector *ss) {
    Let1Context *ctx = (Let1Context *)malloc(sizeof(Let1Context));
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
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBack(s));
    fklByteCodelntVectorUninit(s);
    fklSidVectorDestroy(dd->ss);
    free(dd);
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

static int is_valid_let_args(const FklNastNode *sl, FklCodegenEnv *env,
                             uint32_t scope, FklSidVector *stack,
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

static int is_valid_letrec_args(const FklNastNode *sl, FklCodegenEnv *env,
                                uint32_t scope, FklSidVector *stack,
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

    FklByteCodelnt *retval = *fklByteCodelntVectorPopBack(bcls);
    FklSid_t *cur_sid = symbolStack->base;
    FklSid_t *const sid_end = cur_sid + symbolStack->size;
    for (; cur_sid < sid_end; ++cur_sid) {
        // while (!fklSidVectorIsEmpty(symbolStack)) {
        // FklSid_t id = *fklSidVectorPopBack(symbolStack);
        FklSid_t id = *cur_sid;
        uint32_t idx = fklAddCodegenDefBySid(id, scope, env)->idx;
        append_pop_loc_ins(INS_APPEND_FRONT, retval, idx, fid, line, scope);
    }
    if (!fklByteCodelntVectorIsEmpty(bcls)) {
        FklByteCodelnt *args = *fklByteCodelntVectorPopBack(bcls);
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
            // while (!fklByteCodelntVectorIsEmpty(bcls)) {
            // FklByteCodelnt *cur = *fklByteCodelntVectorPopBack(bcls);
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

    FklByteCodelnt *body = *fklByteCodelntVectorPopBack(s);
    FklByteCodelnt *retval = *fklByteCodelntVectorPopBack(s);
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
        // while (!fklSidVectorIsEmpty(symbolStack)) {
        // FklByteCodelnt *value_bc = *fklByteCodelntVectorPopBack(bcls);
        // FklSid_t id = *fklSidVectorPopBack(symbolStack);
        FklByteCodelnt *value_bc = *cur_bcl;
        FklSid_t id = *cur_sid;
        uint32_t idx = fklAddCodegenDefBySid(id, scope, env)->idx;
        fklResolveCodegenPreDef(id, scope, env, codegen->pts);
        // append_pop_loc_ins(INS_APPEND_FRONT, retval, idx, fid, line, scope);
        // fklCodeLntReverseConcat(value_bc, retval);
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
        if (!is_valid_let_args(args, curEnv, cs, symStack,
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
            fklNastNodeQueuePush2(
                valueQueue, fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)));
    }

    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklCodegenQuest *let1Quest = fklCreateCodegenQuest(
        _let1_exp_bc_process, createLet1CodegenContext(symStack), NULL, cs, cms,
        curEnv, origExp->curline, NULL, codegen);

    fklCodegenQuestVectorPushBack2(codegenQuestStack, let1Quest);

    uint32_t len = fklNastNodeQueueLength(queue);

    FklCodegenQuest *restQuest = fklCreateCodegenQuest(
        _local_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(len)),
        createDefaultQueueNextExpression(queue), cs, cms, curEnv, rest->curline,
        let1Quest, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, restQuest);

    len = fklNastNodeQueueLength(valueQueue);

    FklCodegenQuest *argQuest = fklCreateCodegenQuest(
        _let_arg_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(len)),
        createMustHasRetvalQueueNextExpression(valueQueue), scope, macroScope,
        curEnv, firstSymbol->curline, let1Quest, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, argQuest);
}

static inline FklNastNode *create_nast_list(FklNastNode **a, size_t num,
                                            uint32_t line) {
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

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _default_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(1)),
        createDefaultQueueNextExpression(queue), scope, macroScope, curEnv,
        letHead->curline, codegen, codegenQuestStack);
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

    if (!is_valid_letrec_args(args, curEnv, cs, symStack,
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
        fklNastNodeQueuePush2(
            valueQueue, fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)));

    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklCodegenQuest *let1Quest = fklCreateCodegenQuest(
        _letrec_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(2)), NULL, cs, cms,
        curEnv, origExp->curline, NULL, codegen);

    fklCodegenQuestVectorPushBack2(codegenQuestStack, let1Quest);

    uint32_t len = fklNastNodeQueueLength(queue);

    FklCodegenQuest *restQuest = fklCreateCodegenQuest(
        _local_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(len)),
        createDefaultQueueNextExpression(queue), cs, cms, curEnv, rest->curline,
        let1Quest, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, restQuest);

    FklCodegenQuest *argQuest = fklCreateCodegenQuest(
        _letrec_arg_exp_bc_process, createLet1CodegenContext(symStack),
        createMustHasRetvalQueueNextExpression(valueQueue), cs, cms, curEnv,
        firstSymbol->curline, let1Quest, codegen);
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

    append_jmp_ins(INS_APPEND_BACK, cond, rest->bc->len + jmp_back_ins_len,
                   JMP_IF_TRUE, JMP_FORWARD, fid, line, scope);

    append_jmp_ins(INS_APPEND_BACK, rest, rest->bc->len + cond->bc->len,
                   JMP_UNCOND, JMP_BACKWARD, fid, line, scope);

    uint32_t jmp_back_actual_len = rest_bc->len - rest_len;

    while (jmp_back_ins_len > jmp_back_actual_len) {
        rest_bc->len = rest_len;
        cond_bc->len = cond_len;
        jmp_back_ins_len = jmp_back_actual_len;

        append_jmp_ins(INS_APPEND_BACK, cond, rest->bc->len + jmp_back_ins_len,
                       JMP_IF_TRUE, JMP_FORWARD, fid, line, scope);

        append_jmp_ins(INS_APPEND_BACK, rest, rest->bc->len + cond->bc->len,
                       JMP_UNCOND, JMP_BACKWARD, fid, line, scope);

        jmp_back_actual_len = rest_bc->len - rest_len;
    }
}

BC_PROCESS(_do0_exp_bc_process) {
    FklByteCodelntVector *s = GET_STACK(context);
    FklByteCodelnt *rest = *fklByteCodelntVectorPopBack(s);
    FklByteCodelnt *value = *fklByteCodelntVectorPopBack(s);
    FklByteCodelnt *cond = *fklByteCodelntVectorPopBack(s);

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

    FklCodegenQuest *do0Quest = fklCreateCodegenQuest(
        _do0_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(3)), NULL, cs, cms,
        curEnv, cond->curline, NULL, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do0Quest);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _do_rest_exp_bc_process,
        createDefaultStackContext(
            fklByteCodelntVectorCreate(fklNastNodeQueueLength(queue))),
        createDefaultQueueNextExpression(queue), cs, cms, curEnv, cond->curline,
        codegen, codegenQuestStack);

    if (value) {
        FklNastNodeQueue *vQueue = fklNastNodeQueueCreate();
        fklNastNodeQueuePush2(vQueue, fklMakeNastNodeRef(value));
        FklCodegenQuest *do0VQuest = fklCreateCodegenQuest(
            _default_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(1)),
            createMustHasRetvalQueueNextExpression(vQueue), cs, cms, curEnv,
            origExp->curline, do0Quest, codegen);
        fklCodegenQuestVectorPushBack2(codegenQuestStack, do0VQuest);
    } else {
        FklByteCodelnt *v = fklCreateByteCodelnt(fklCreateByteCode(0));
        FklByteCodelntVector *s = fklByteCodelntVectorCreate(1);
        fklByteCodelntVectorPushBack2(s, v);
        FklCodegenQuest *do0VQuest = fklCreateCodegenQuest(
            _default_bc_process, createDefaultStackContext(s), NULL, cs, cms,
            curEnv, origExp->curline, do0Quest, codegen);
        fklCodegenQuestVectorPushBack2(codegenQuestStack, do0VQuest);
    }
    FklNastNodeQueue *cQueue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(cQueue, fklMakeNastNodeRef(cond));
    FklCodegenQuest *do0CQuest = fklCreateCodegenQuest(
        _default_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(1)),
        createMustHasRetvalQueueNextExpression(cQueue), cs, cms, curEnv,
        origExp->curline, do0Quest, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do0CQuest);
}

static inline void destroy_next_exp_queue(FklNastNodeQueue *q) {
    while (!fklNastNodeQueueIsEmpty(q))
        fklDestroyNastNode(*fklNastNodeQueuePop(q));
    fklNastNodeQueueDestroy(q);
}

static inline int
is_valid_do_var_bind(const FklNastNode *list, FklNastNode **nextV,
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

static inline int
is_valid_do_bind_list(const FklNastNode *sl, FklCodegenEnv *env, uint32_t scope,
                      FklSidVector *stack, FklSidVector *nstack,
                      FklNastNodeQueue *valueQueue, FklNastNodeQueue *nextQueue,
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
        s->size == 5 ? *fklByteCodelntVectorPopBack(s) : NULL;
    FklByteCodelnt *rest = *fklByteCodelntVectorPopBack(s);
    FklByteCodelnt *value = *fklByteCodelntVectorPopBack(s);
    FklByteCodelnt *cond = *fklByteCodelntVectorPopBack(s);
    FklByteCodelnt *init = *fklByteCodelntVectorPopBack(s);

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
    if (!is_valid_do_bind_list(bindlist, curEnv, cs, symStack, nextSymStack,
                               valueQueue, nextValueQueue,
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

    FklCodegenQuest *do1Quest = fklCreateCodegenQuest(
        _do1_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(5)), NULL, cs, cms,
        curEnv, origExp->curline, NULL, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do1Quest);

    FklCodegenQuest *do1NextValQuest = fklCreateCodegenQuest(
        _do1_next_val_bc_process, createLet1CodegenContext(nextSymStack),
        createMustHasRetvalQueueNextExpression(nextValueQueue), cs, cms, curEnv,
        origExp->curline, do1Quest, codegen);

    fklCodegenQuestVectorPushBack2(codegenQuestStack, do1NextValQuest);

    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklCodegenQuest *do1RestQuest = fklCreateCodegenQuest(
        _do_rest_exp_bc_process,
        createDefaultStackContext(
            fklByteCodelntVectorCreate(fklNastNodeQueueLength(queue))),
        createDefaultQueueNextExpression(queue), cs, cms, curEnv,
        origExp->curline, do1Quest, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do1RestQuest);

    if (value) {
        FklNastNodeQueue *vQueue = fklNastNodeQueueCreate();
        fklNastNodeQueuePush2(vQueue, fklMakeNastNodeRef(value));
        FklCodegenQuest *do1VQuest = fklCreateCodegenQuest(
            _default_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(1)),
            createMustHasRetvalQueueNextExpression(vQueue), cs, cms, curEnv,
            origExp->curline, do1Quest, codegen);
        fklCodegenQuestVectorPushBack2(codegenQuestStack, do1VQuest);
    } else {
        FklByteCodelnt *v = fklCreateByteCodelnt(fklCreateByteCode(0));
        FklByteCodelntVector *s = fklByteCodelntVectorCreate(1);
        fklByteCodelntVectorPushBack2(s, v);
        FklCodegenQuest *do1VQuest = fklCreateCodegenQuest(
            _default_bc_process, createDefaultStackContext(s), NULL, cs, cms,
            curEnv, origExp->curline, do1Quest, codegen);
        fklCodegenQuestVectorPushBack2(codegenQuestStack, do1VQuest);
    }

    FklNastNodeQueue *cQueue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(cQueue, fklMakeNastNodeRef(cond));
    FklCodegenQuest *do1CQuest = fklCreateCodegenQuest(
        _default_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(1)),
        createMustHasRetvalQueueNextExpression(cQueue), cs, cms, curEnv,
        origExp->curline, do1Quest, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do1CQuest);

    FklCodegenQuest *do1InitValQuest = fklCreateCodegenQuest(
        _do1_init_val_bc_process, createLet1CodegenContext(symStack),
        createMustHasRetvalQueueNextExpression(valueQueue), scope, macroScope,
        curEnv, origExp->curline, do1Quest, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, do1InitValQuest);
}

static inline FklSid_t get_sid_in_gs_with_id_in_ps(FklSid_t id,
                                                   FklSymbolTable *gs,
                                                   FklSymbolTable *ps) {
    return fklAddSymbol(fklGetSymbolWithId(id, ps)->k, gs)->v;
}

static inline FklSid_t get_sid_with_idx(FklCodegenEnvScope *sc, uint32_t idx,
                                        FklSymbolTable *gs,
                                        FklSymbolTable *ps) {
    for (FklSymDefHashMapNode *l = sc->defs.first; l; l = l->next) {
        if (l->v.idx == idx)
            return get_sid_in_gs_with_id_in_ps(l->k.id, gs, ps);
    }
    return 0;
}

static inline FklSid_t get_sid_with_ref_idx(FklSymDefHashMap *refs,
                                            uint32_t idx, FklSymbolTable *gs,
                                            FklSymbolTable *ps) {
    for (FklSymDefHashMapNode *l = refs->first; l; l = l->next) {
        if (l->v.idx == idx)
            return get_sid_in_gs_with_id_in_ps(l->k.id, gs, ps);
    }
    return 0;
}

static inline FklByteCodelnt *
process_set_var(FklByteCodelntVector *stack, FklCodegenInfo *codegen,
                FklCodegenOuterCtx *outer_ctx, FklCodegenEnv *env,
                uint32_t scope, FklSid_t fid, uint32_t line) {
    if (stack->size >= 2) {
        FklByteCodelnt *cur = *fklByteCodelntVectorPopBack(stack);
        FklByteCodelnt *popVar = *fklByteCodelntVectorPopBack(stack);
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
                        get_sid_with_idx(&env->scopes[scope - 1], idx,
                                         codegen->runtime_symbol_table,
                                         &outer_ctx->public_symbol_table);
            } else if (fklIsPutVarRefIns(popVar_ins)) {
                FklInstructionArg arg;
                fklGetInsOpArg(cur_ins, &arg);
                uint64_t prototypeId = arg.ux;

                fklGetInsOpArg(popVar_ins, &arg);
                uint64_t idx = arg.ux;
                if (!codegen->pts->pa[prototypeId].sid)
                    codegen->pts->pa[prototypeId].sid = get_sid_with_ref_idx(
                        &env->refs, idx, codegen->runtime_symbol_table,
                        &outer_ctx->public_symbol_table);
            }
        }
        fklCodeLntReverseConcat(cur, popVar);
        fklDestroyByteCodelnt(cur);
        return popVar;
    } else {
        FklByteCodelnt *popVar = *fklByteCodelntVectorPopBack(stack);
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
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBack(s));
    fklByteCodelntVectorUninit(s);
    free(ctx);
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
        (DefineVarContext *)malloc(sizeof(DefineVarContext));
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
    fklByteCodelntVectorInsertFront2(
        stack, append_put_loc_ins(INS_APPEND_BACK, NULL, idx, codegen->fid,
                                  ctx->line, scope));
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
        retval = fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL), fid,
                                         line, scope);
        fklByteCodeLntPushBackIns(retval, &ret, fid, line, scope);
    }
    fklCodeLntReverseConcat(stack->base[0], retval);
    fklDestroyByteCodelnt(stack->base[0]);
    stack->size = 0;
    fklScanAndSetTailCall(retval->bc);
    FklFuncPrototypes *pts = codegen->pts;
    fklUpdatePrototype(pts, env, codegen->runtime_symbol_table,
                       &outer_ctx->public_symbol_table);
    append_push_proc_ins(INS_APPEND_FRONT, retval, env->prototypeId,
                         retval->bc->len, fid, line, scope);
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
        // append_pop_arg_ins(INS_APPEND_BACK, retval,
        //                    fklAddCodegenDefBySid(cur->sym, 1, curEnv)->idx,
        //                    codegen->fid, cur->curline, 1);
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
        // append_pop_rest_arg_ins(
        //     INS_APPEND_BACK, retval,
        //     fklAddCodegenDefBySid(args->sym, 1, curEnv)->idx, codegen->fid,
        //     args->curline, 1);
    }
    // FklInstruction resBp = create_op_ins(FKL_OP_RES_BP);
    // fklByteCodeLntPushBackIns(retval, &resBp, codegen->fid, args->curline,
    // 1);
    FklInstruction check_arg = {
        .op = FKL_OP_CHECK_ARG,
        .ai = rest_list,
        .bu = arg_count,
    };
    fklByteCodeLntPushBackIns(retval, &check_arg, codegen->fid, args->curline,
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
        // uint32_t idx = fklAddCodegenDefBySid(curId, 1, curEnv)->idx;

        // append_pop_arg_ins(INS_APPEND_BACK, retval, idx, codegen->fid,
        // curline,
        //                    1);
    }
    // FklInstruction resBp = create_op_ins(FKL_OP_RES_BP);
    // fklByteCodeLntPushBackIns(retval, &resBp, codegen->fid, curline, 1);
    FklInstruction check_arg = {.op = FKL_OP_CHECK_ARG, .ai = 0, .bu = top};
    fklByteCodeLntPushBackIns(retval, &check_arg, codegen->fid, curline, 1);
    return retval;
}

static inline FklSymDefHashMapMutElm *
sid_ht_to_idx_key_ht(FklSymDefHashMap *sht, FklSymbolTable *globalSymTable,
                     FklSymbolTable *pst) {
    FklSymDefHashMapMutElm *refs;
    if (!sht->count)
        refs = NULL;
    else {
        refs = (FklSymDefHashMapMutElm *)malloc(
            sht->count * sizeof(FklSymDefHashMapMutElm));
        FKL_ASSERT(refs);
    }
    FklSymDefHashMapNode *list = sht->first;
    for (; list; list = list->next) {
        FklSid_t sid =
            fklAddSymbol(fklGetSymbolWithId(list->k.id, pst)->k, globalSymTable)
                ->v;
        refs[list->v.idx] = (FklSymDefHashMapMutElm){
            .k = (FklSidScope){sid, 0},
            .v = (FklSymDef){list->v.idx, list->v.cidx, list->v.isLocal,
                             list->v.isConst}};
    }
    return refs;
}

void fklInitFuncPrototypeWithEnv(FklFuncPrototype *cpt, FklCodegenInfo *info,
                                 FklCodegenEnv *env, FklSid_t sid,
                                 uint32_t line, FklSymbolTable *pst) {
    cpt->lcount = env->lcount;
    cpt->refs =
        sid_ht_to_idx_key_ht(&env->refs, info->runtime_symbol_table, pst);
    cpt->rcount = env->refs.count;
    cpt->sid = sid;
    cpt->fid = info->fid;
    cpt->line = line;
}

static inline void create_and_insert_to_pool(FklCodegenInfo *info, uint32_t p,
                                             FklCodegenEnv *env, FklSid_t sid,
                                             uint32_t line,
                                             FklSymbolTable *pst) {
    FklSid_t fid = info->fid;
    FklSymbolTable *gst = info->runtime_symbol_table;
    FklFuncPrototypes *cp = info->pts;
    cp->count += 1;
    FklFuncPrototype *pts = (FklFuncPrototype *)fklRealloc(
        cp->pa, (cp->count + 1) * sizeof(FklFuncPrototype));
    FKL_ASSERT(pts);
    cp->pa = pts;
    FklFuncPrototype *cpt = &pts[cp->count];
    env->prototypeId = cp->count;
    cpt->lcount = env->lcount;
    cpt->refs = sid_ht_to_idx_key_ht(&env->refs, gst, pst);
    cpt->rcount = env->refs.count;
    cpt->sid = sid;
    cpt->fid = fid;
    cpt->line = line;

    if (info->work.env_work_cb)
        info->work.env_work_cb(info, env, info->work.work_ctx);
}

void fklCreateFuncPrototypeAndInsertToPool(FklCodegenInfo *info, uint32_t p,
                                           FklCodegenEnv *env, FklSid_t sid,
                                           uint32_t line, FklSymbolTable *pst) {
    create_and_insert_to_pool(info, p, env, sid, line, pst);
}

BC_PROCESS(_named_let_set_var_exp_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    FklByteCodelnt *popVar = NULL;
    if (stack->size >= 2) {
        FklByteCodelnt *cur = *fklByteCodelntVectorPopBack(stack);
        popVar = *fklByteCodelntVectorPopBack(stack);
        fklCodeLntReverseConcat(cur, popVar);
        fklDestroyByteCodelnt(cur);
    } else {
        FklByteCodelnt *cur = *fklByteCodelntVectorPopBack(stack);
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

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _funcall_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(2)), NULL, cs, cms,
        curEnv, name->curline, codegen, codegenQuestStack);

    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklByteCodelntVector *stack = fklByteCodelntVectorCreate(2);
    uint32_t idx = fklAddCodegenDefBySid(name->sym, cs, curEnv)->idx;
    fklByteCodelntVectorPushBack2(
        stack, append_put_loc_ins(INS_APPEND_BACK, NULL, idx, codegen->fid,
                                  name->curline, scope));

    fklReplacementHashMapAdd2(cms->replacements,
                              fklAddSymbolCstr("*func*", pst)->v, name);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _named_let_set_var_exp_bc_process, createDefaultStackContext(stack),
        NULL, cs, cms, curEnv, name->curline, codegen, codegenQuestStack);

    FklCodegenEnv *lambdaCodegenEnv = fklCreateCodegenEnv(curEnv, cs, cms);
    FklByteCodelntVector *lStack = fklByteCodelntVectorCreate(2);
    FklNastNode *argsNode = caddr_nast_node(origExp);
    FklByteCodelnt *argBc = processArgs(argsNode, lambdaCodegenEnv, codegen);
    fklByteCodelntVectorPushBack2(lStack, argBc);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);

    create_and_insert_to_pool(
        codegen, curEnv->prototypeId, lambdaCodegenEnv,
        get_sid_in_gs_with_id_in_ps(name->sym, codegen->runtime_symbol_table,
                                    pst),
        origExp->curline, pst);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _lambda_exp_bc_process, createDefaultStackContext(lStack),
        createDefaultQueueNextExpression(queue), 1, lambdaCodegenEnv->macros,
        lambdaCodegenEnv, rest->curline, codegen, codegenQuestStack);
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

    if (!is_valid_let_args(args, lambdaCodegenEnv, 1, symStack,
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
        fklNastNodeQueuePush2(
            valueQueue, fklMakeNastNodeRef(cadr_nast_node(cur->pair->car)));

    FklCodegenQuest *funcallQuest = fklCreateCodegenQuest(
        _funcall_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(2)), NULL, cs, cms,
        curEnv, name->curline, NULL, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, funcallQuest);

    FklByteCodelntVector *stack = fklByteCodelntVectorCreate(2);
    uint32_t idx = fklAddCodegenDefBySid(name->sym, cs, curEnv)->idx;
    fklByteCodelntVectorPushBack2(
        stack, append_put_loc_ins(INS_APPEND_BACK, NULL, idx, codegen->fid,
                                  name->curline, scope));

    FklSymbolTable *pst = &outer_ctx->public_symbol_table;

    fklReplacementHashMapAdd2(cms->replacements,
                              fklAddSymbolCstr("*func*", pst)->v, name);
    fklCodegenQuestVectorPushBack2(
        codegenQuestStack,
        fklCreateCodegenQuest(
            _let_arg_exp_bc_process,
            createDefaultStackContext(
                fklByteCodelntVectorCreate(fklNastNodeQueueLength(valueQueue))),
            createMustHasRetvalQueueNextExpression(valueQueue), scope,
            macroScope, curEnv, firstSymbol->curline, funcallQuest, codegen));

    fklCodegenQuestVectorPushBack2(
        codegenQuestStack,
        fklCreateCodegenQuest(_named_let_set_var_exp_bc_process,
                              createDefaultStackContext(stack), NULL, cs, cms,
                              curEnv, name->curline, funcallQuest, codegen));

    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklByteCodelntVector *lStack = fklByteCodelntVectorCreate(2);

    FklByteCodelnt *argBc = processArgsInStack(symStack, lambdaCodegenEnv,
                                               codegen, firstSymbol->curline);

    fklByteCodelntVectorPushBack2(lStack, argBc);

    fklSidVectorDestroy(symStack);
    create_and_insert_to_pool(
        codegen, curEnv->prototypeId, lambdaCodegenEnv,
        get_sid_in_gs_with_id_in_ps(name->sym, codegen->runtime_symbol_table,
                                    pst),
        origExp->curline, pst);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _lambda_exp_bc_process, createDefaultStackContext(lStack),
        createDefaultQueueNextExpression(queue), 1, lambdaCodegenEnv->macros,
        lambdaCodegenEnv, rest->curline, codegen, codegenQuestStack);
}

BC_PROCESS(_and_exp_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    if (stack->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));
        while (!fklByteCodelntVectorIsEmpty(stack)) {
            if (retval->bc->len) {
                fklByteCodeLntInsertFrontIns(&drop, retval, fid, line, scope);
                append_jmp_ins(INS_APPEND_FRONT, retval, retval->bc->len,
                               JMP_IF_FALSE, JMP_FORWARD, fid, line, scope);
            }
            FklByteCodelnt *cur = *fklByteCodelntVectorPopBack(stack);
            fklCodeLntReverseConcat(cur, retval);
            fklDestroyByteCodelnt(cur);
        }
        return retval;
    } else
        return append_push_i64_ins(INS_APPEND_BACK, NULL, 1, codegen, fid, line,
                                   scope);
}

static CODEGEN_FUNC(codegen_and) {
    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    uint32_t cs = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _and_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(32)),
        createDefaultQueueNextExpression(queue), cs, cms, curEnv, rest->curline,
        codegen, codegenQuestStack);
}

BC_PROCESS(_or_exp_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    if (stack->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklByteCodelnt *retval = fklCreateByteCodelnt(fklCreateByteCode(0));
        while (!fklByteCodelntVectorIsEmpty(stack)) {
            if (retval->bc->len) {
                fklByteCodeLntInsertFrontIns(&drop, retval, fid, line, scope);
                append_jmp_ins(INS_APPEND_FRONT, retval, retval->bc->len,
                               JMP_IF_TRUE, JMP_FORWARD, fid, line, scope);
            }
            FklByteCodelnt *cur = *fklByteCodelntVectorPopBack(stack);
            fklCodeLntReverseConcat(cur, retval);
            fklDestroyByteCodelnt(cur);
        }
        return retval;
    } else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL), fid,
                                       line, scope);
}

static CODEGEN_FUNC(codegen_or) {
    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    uint32_t cs = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);
    pushListItemToQueue(rest, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _or_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(32)),
        createDefaultQueueNextExpression(queue), cs, cms, curEnv, rest->curline,
        codegen, codegenQuestStack);
}

void fklUpdatePrototypeRef(FklFuncPrototypes *cp, FklCodegenEnv *env,
                           FklSymbolTable *globalSymTable,
                           FklSymbolTable *pst) {
    FKL_ASSERT(env->pdef.count == 0);
    FklFuncPrototype *pts = &cp->pa[env->prototypeId];
    FklSymDefHashMap *eht = &env->refs;
    uint32_t count = eht->count;

    FklSymDefHashMapMutElm *refs;
    if (!count)
        refs = NULL;
    else {
        refs = (FklSymDefHashMapMutElm *)fklRealloc(
            pts->refs, count * sizeof(FklSymDefHashMapMutElm));
        FKL_ASSERT(refs);
    }
    pts->refs = refs;
    pts->rcount = count;
    for (FklSymDefHashMapNode *list = eht->first; list; list = list->next) {
        FklSid_t sid =
            fklAddSymbol(fklGetSymbolWithId(list->k.id, pst)->k, globalSymTable)
                ->v;
        refs[list->v.idx] = (FklSymDefHashMapMutElm){
            .k = (FklSidScope){.id = sid, .scope = list->k.scope},
            .v = (FklSymDef){.idx = list->v.idx,
                             .cidx = list->v.cidx,
                             .isLocal = list->v.isLocal}};
    }
}

void fklUpdatePrototype(FklFuncPrototypes *cp, FklCodegenEnv *env,
                        FklSymbolTable *globalSymTable, FklSymbolTable *pst) {
    FKL_ASSERT(env->pdef.count == 0);
    FklFuncPrototype *pts = &cp->pa[env->prototypeId];
    pts->lcount = env->lcount;
    process_unresolve_ref(env, 1, cp);
    fklUpdatePrototypeRef(cp, env, globalSymTable, pst);
}

FklCodegenEnv *fklCreateCodegenEnv(FklCodegenEnv *prev, uint32_t pscope,
                                   FklCodegenMacroScope *macroScope) {
    FklCodegenEnv *r = (FklCodegenEnv *)malloc(sizeof(FklCodegenEnv));
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
            free(scopes);
            free(cur->slotFlags);
            fklSymDefHashMapUninit(&cur->refs);
            FklUnReSymbolRefVector *unref = &cur->uref;
            fklUnReSymbolRefVectorUninit(unref);
            fklDestroyCodegenMacroScope(cur->macros);

            fklPredefHashMapUninit(&cur->pdef);
            fklPreDefRefVectorUninit(&cur->ref_pdef);
            free(cur);
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
    create_and_insert_to_pool(codegen, curEnv->prototypeId, lambdaCodegenEnv, 0,
                              origExp->curline,
                              &outer_ctx->public_symbol_table);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _lambda_exp_bc_process, createDefaultStackContext(stack),
        createDefaultQueueNextExpression(queue), 1, lambdaCodegenEnv->macros,
        lambdaCodegenEnv, rest->curline, codegen, codegenQuestStack);
}

static inline int is_constant_defined(FklSid_t id, uint32_t scope,
                                      FklCodegenEnv *curEnv) {
    FklSymDefHashMapElm *def = fklGetCodegenDefByIdInScope(id, scope, curEnv);
    return def && def->v.isConst;
}

static inline int is_variable_defined(FklSid_t id, uint32_t scope,
                                      FklCodegenEnv *curEnv) {
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
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _def_var_exp_bc_process,
        create_def_var_context(name->sym, scope, name->curline),
        createMustHasRetvalQueueNextExpression(queue), scope, macroScope,
        curEnv, name->curline, codegen, codegenQuestStack);
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
        errorState->fid =
            assign_uref->fid
                ? fklAddSymbol(fklGetSymbolWithId(assign_uref->fid,
                                                  codegen->runtime_symbol_table)
                                   ->k,
                               &outer_ctx->public_symbol_table)
                      ->v
                : 0;
        return NULL;
    }
    FklSymDef *def = fklAddCodegenDefBySid(ctx->id, ctx->scope, env);
    def->isConst = 1;
    uint32_t idx = def->idx;
    fklByteCodelntVectorInsertFront2(
        stack, append_put_loc_ins(INS_APPEND_BACK, NULL, idx, codegen->fid,
                                  ctx->line, ctx->scope));
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
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _def_const_exp_bc_process,
        create_def_var_context(name->sym, scope, name->curline),
        createMustHasRetvalQueueNextExpression(queue), scope, macroScope,
        curEnv, name->curline, codegen, codegenQuestStack);
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

    FklCodegenQuest *prevQuest = fklCreateCodegenQuest(
        _def_var_exp_bc_process,
        create_def_var_context(name->sym, scope, name->curline), NULL, scope,
        macroScope, curEnv, name->curline, NULL, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, prevQuest);

    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklByteCodelntVector *lStack = fklByteCodelntVectorCreate(32);
    fklByteCodelntVectorPushBack2(lStack, argsBc);
    create_and_insert_to_pool(
        codegen, curEnv->prototypeId, lambdaCodegenEnv,
        get_sid_in_gs_with_id_in_ps(name->sym, codegen->runtime_symbol_table,
                                    pst),
        origExp->curline, pst);
    FklNastNode *name_str = fklCreateNastNode(FKL_NAST_STR, name->curline);
    name_str->str = fklCopyString(fklGetSymbolWithId(name->sym, pst)->k);
    fklReplacementHashMapAdd2(lambdaCodegenEnv->macros->replacements,
                              fklAddSymbolCstr("*func*", pst)->v, name_str);
    fklDestroyNastNode(name_str);
    FklCodegenQuest *cur = fklCreateCodegenQuest(
        _lambda_exp_bc_process, createDefaultStackContext(lStack),
        createDefaultQueueNextExpression(queue), 1, lambdaCodegenEnv->macros,
        lambdaCodegenEnv, rest->curline, prevQuest, codegen);
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

    FklCodegenQuest *prevQuest = fklCreateCodegenQuest(
        _def_const_exp_bc_process,
        create_def_var_context(name->sym, scope, name->curline), NULL, scope,
        macroScope, curEnv, name->curline, NULL, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, prevQuest);

    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    pushListItemToQueue(rest, queue, NULL);
    FklByteCodelntVector *lStack = fklByteCodelntVectorCreate(32);
    fklByteCodelntVectorPushBack2(lStack, argsBc);
    create_and_insert_to_pool(
        codegen, curEnv->prototypeId, lambdaCodegenEnv,
        get_sid_in_gs_with_id_in_ps(name->sym, codegen->runtime_symbol_table,
                                    pst),
        origExp->curline, pst);
    FklNastNode *name_str = fklCreateNastNode(FKL_NAST_STR, name->curline);
    name_str->str = fklCopyString(fklGetSymbolWithId(name->sym, pst)->k);
    fklReplacementHashMapAdd2(lambdaCodegenEnv->macros->replacements,
                              fklAddSymbolCstr("*func*", pst)->v, name_str);
    fklDestroyNastNode(name_str);
    FklCodegenQuest *cur = fklCreateCodegenQuest(
        _lambda_exp_bc_process, createDefaultStackContext(lStack),
        createDefaultQueueNextExpression(queue), 1, lambdaCodegenEnv->macros,
        lambdaCodegenEnv, rest->curline, prevQuest, codegen);
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
        fklByteCodelntVectorPushBack2(
            stack, append_put_loc_ins(INS_APPEND_BACK, NULL, def->v.idx,
                                      codegen->fid, name->curline, scope));
    } else {
        FklSymDefHashMapElm *def = fklAddCodegenRefBySid(
            name->sym, curEnv, codegen->fid, name->curline, 1);
        if (def->v.isConst) {
            errorState->type = FKL_ERR_ASSIGN_CONSTANT;
            errorState->place = fklMakeNastNodeRef(name);
            return;
        }
        fklByteCodelntVectorPushBack2(
            stack, append_put_var_ref_ins(INS_APPEND_BACK, NULL, def->v.idx,
                                          codegen->fid, name->curline, scope));
    }
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _set_var_exp_bc_process, createDefaultStackContext(stack),
        createMustHasRetvalQueueNextExpression(queue), scope, macroScope,
        curEnv, name->curline, codegen, codegenQuestStack);
}

static inline void push_default_codegen_quest(
    FklNastNode *value, FklCodegenQuestVector *codegenQuestStack,
    uint32_t scope, FklCodegenMacroScope *macroScope, FklCodegenEnv *curEnv,
    FklCodegenQuest *prev, FklCodegenInfo *codegen) {
    FklByteCodelntVector *stack = fklByteCodelntVectorCreate(1);
    fklByteCodelntVectorPushBack2(
        stack, create_bc_lnt(fklCodegenNode(value, codegen), codegen->fid,
                             value->curline, scope));
    FklCodegenQuest *quest = fklCreateCodegenQuest(
        _default_bc_process, createDefaultStackContext(stack), NULL, scope,
        macroScope, curEnv, value->curline, prev, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, quest);
}

static CODEGEN_FUNC(codegen_macroexpand) {
    FklNastNode *value =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    fklMakeNastNodeRef(value);
    if (value->type == FKL_NAST_PAIR)
        value = fklTryExpandCodegenMacro(value, codegen, curEnv->macros,
                                         errorState);
    if (errorState->type)
        return;
    push_default_codegen_quest(value, codegenQuestStack, scope, macroScope,
                               curEnv, NULL, codegen);
    fklDestroyNastNode(value);
}

static inline void uninit_codegen_macro(FklCodegenMacro *macro) {
    fklDestroyNastNode(macro->pattern);
    fklDestroyNastNode(macro->origin_exp);
    if (macro->own)
        fklDestroyByteCodelnt(macro->bcl);
}

static void add_compiler_macro(FklCodegenMacro **pmacro, FklNastNode *pattern,
                               FklNastNode *origin_exp, FklByteCodelnt *bcl,
                               uint32_t prototype_id, int own) {
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
        FklCodegenMacro *macro = fklCreateCodegenMacro(
            pattern, origin_exp, bcl, *phead, prototype_id, own);
        *phead = macro;
    } else if (coverState == FKL_PATTERN_EQUAL) {
        FklCodegenMacro *macro = *pmacro;
        uninit_codegen_macro(macro);
        macro->own = own;
        macro->prototype_id = prototype_id;
        macro->pattern = pattern;
        macro->bcl = bcl;
        macro->origin_exp = origin_exp;
    } else {
        FklCodegenMacro *next = *pmacro;
        *pmacro = fklCreateCodegenMacro(pattern, origin_exp, bcl, next,
                                        prototype_id, own);
    }
}

BC_PROCESS(_export_macro_bc_process) {
    FklCodegenMacroScope *target_macro_scope = cms->prev;
    for (; codegen && !codegen->is_lib; codegen = codegen->prev)
        ;
    for (FklCodegenMacro *cur = cms->head; cur; cur = cur->next) {
        cur->own = 0;
        add_compiler_macro(&codegen->export_macro,
                           fklMakeNastNodeRef(cur->pattern),
                           fklMakeNastNodeRef(cur->origin_exp), cur->bcl,
                           cur->prototype_id, 1);
        add_compiler_macro(&target_macro_scope->head,
                           fklMakeNastNodeRef(cur->pattern),
                           fklMakeNastNodeRef(cur->origin_exp), cur->bcl,
                           cur->prototype_id, 0);
    }
    for (FklReplacementHashMapNode *cur = cms->replacements->first; cur;
         cur = cur->next) {
        fklReplacementHashMapAdd2(codegen->export_replacement, cur->k, cur->v);
        fklReplacementHashMapAdd2(target_macro_scope->replacements, cur->k,
                                  cur->v);
    }
    return NULL;
}

static inline void add_export_symbol(FklCodegenInfo *libCodegen,
                                     FklNastNode *origExp, FklNastNode *rest,
                                     FklNastNodeQueue *exportQueue) {
    FklNastPair *prevPair = origExp->pair;
    FklNastNode *exportHead = origExp->pair->car;
    for (; rest->type == FKL_NAST_PAIR; rest = rest->pair->cdr) {
        FklNastNode *restExp =
            fklNastCons(fklMakeNastNodeRef(exportHead), rest, rest->curline);
        fklNastNodeQueuePush2(exportQueue, restExp);
        prevPair->cdr = fklCreateNastNode(FKL_NAST_NIL, rest->curline);
        prevPair = rest->pair;
    }
}

static inline void push_single_bcl_codegen_quest(
    FklByteCodelnt *bcl, FklCodegenQuestVector *codegenQuestStack,
    uint32_t scope, FklCodegenMacroScope *macroScope, FklCodegenEnv *curEnv,
    FklCodegenQuest *prev, FklCodegenInfo *codegen, uint64_t curline) {
    FklByteCodelntVector *stack = fklByteCodelntVectorCreate(1);
    fklByteCodelntVectorPushBack2(stack, bcl);
    FklCodegenQuest *quest = fklCreateCodegenQuest(
        _default_bc_process, createDefaultStackContext(stack), NULL, scope,
        macroScope, curEnv, curline, prev, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, quest);
}

typedef FklNastNode *(*ReplacementFunc)(const FklNastNode *orig,
                                        const FklCodegenEnv *env,
                                        const FklCodegenInfo *codegen);

static inline ReplacementFunc
findBuiltInReplacementWithId(FklSid_t id,
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
    } else if ((f = findBuiltInReplacementWithId(
                    value->sym, ctx->builtin_replacement_id))) {
        FklNastNode *t = f(value, curEnv, info);
        int r = t->type != FKL_NAST_NIL;
        fklDestroyNastNode(t);
        return r;
    } else
        return 0;
}

static inline FklNastNode *
get_replacement(const FklNastNode *value, const FklCodegenInfo *info,
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

static inline char *
combineFileNameFromListAndCheckPrivate(const FklNastNode *list,
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
    fklNastImmPairVectorPushBack2(
        &s, (FklNastImmPair){.car = cadr_nast_node(p), .cdr = e});
    FklSid_t slot_id = p->pair->car->sym;
    int r = 1;
    while (!fklNastImmPairVectorIsEmpty(&s)) {
        const FklNastImmPair *top = fklNastImmPairVectorPopBack(&s);
        const FklNastNode *p = top->car;
        const FklNastNode *e = top->cdr;
        if (is_compile_check_pattern_slot_node(slot_id, p))
            continue;
        else if (p->type == e->type) {
            switch (p->type) {
            case FKL_NAST_BOX:
                fklNastImmPairVectorPushBack2(
                    &s, (FklNastImmPair){.car = p->box, .cdr = e->box});
                break;
            case FKL_NAST_PAIR:
                fklNastImmPairVectorPushBack2(
                    &s,
                    (FklNastImmPair){.car = p->pair->car, .cdr = e->pair->car});
                fklNastImmPairVectorPushBack2(
                    &s,
                    (FklNastImmPair){.car = p->pair->cdr, .cdr = e->pair->cdr});
                break;
            case FKL_NAST_VECTOR:
                if (p->vec->size != e->vec->size) {
                    r = 0;
                    goto exit;
                }
                for (size_t i = 0; i < p->vec->size; i++) {
                    fklNastImmPairVectorPushBack2(
                        &s, (FklNastImmPair){.car = p->vec->base[i],
                                             .cdr = e->vec->base[i]});
                }
                break;
            case FKL_NAST_HASHTABLE:
                if (p->hash->num != e->hash->num) {
                    r = 0;
                    goto exit;
                }
                for (size_t i = 0; i < p->hash->num; i++) {
                    fklNastImmPairVectorPushBack2(
                        &s, (FklNastImmPair){.car = p->hash->items[i].car,
                                             .cdr = e->hash->items[i].car});
                    fklNastImmPairVectorPushBack2(
                        &s, (FklNastImmPair){.car = p->hash->items[i].cdr,
                                             .cdr = e->hash->items[i].cdr});
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
#include <fakeLisp/vector.h>

static inline int cfg_check_defined(const FklNastNode *exp,
                                    const FklPmatchHashMap *ht,
                                    const FklCodegenOuterCtx *outer_ctx,
                                    const FklCodegenEnv *curEnv, uint32_t scope,
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
    char *filename = combineFileNameFromListAndCheckPrivate(
        value, &outer_ctx->public_symbol_table);

    if (filename) {
        char *packageMainFileName =
            fklStrCat(fklCopyCstr(filename), FKL_PATH_SEPARATOR_STR);
        packageMainFileName =
            fklStrCat(packageMainFileName, FKL_PACKAGE_MAIN_FILE);

        char *preCompileFileName = fklStrCat(fklCopyCstr(packageMainFileName),
                                             FKL_PRE_COMPILE_FKL_SUFFIX_STR);

        char *scriptFileName =
            fklStrCat(fklCopyCstr(filename), FKL_SCRIPT_FILE_EXTENSION);

        char *dllFileName = fklStrCat(fklCopyCstr(filename), FKL_DLL_FILE_TYPE);

        int r = fklIsAccessibleRegFile(packageMainFileName)
             || fklIsAccessibleRegFile(scriptFileName)
             || fklIsAccessibleRegFile(preCompileFileName)
             || fklIsAccessibleRegFile(dllFileName);
        free(filename);
        free(scriptFileName);
        free(dllFileName);
        free(packageMainFileName);
        free(preCompileFileName);
        return r;
    } else
        return 0;
}

static inline int cfg_check_macro_defined(
    const FklNastNode *exp, const FklPmatchHashMap *ht,
    const FklCodegenOuterCtx *outer_ctx, const FklCodegenEnv *curEnv,
    uint32_t scope, const FklCodegenMacroScope *macroScope,
    const FklCodegenInfo *info, FklCodegenErrorState *errorState) {
    const FklNastNode *value =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    if (value->type == FKL_NAST_SYM)
        return is_replacement_define(value, info, outer_ctx, macroScope,
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

static inline int
cfg_check_eq(const FklNastNode *exp, const FklPmatchHashMap *ht,
             const FklCodegenOuterCtx *outer_ctx, const FklCodegenEnv *curEnv,
             const FklCodegenMacroScope *macroScope, const FklCodegenInfo *info,
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

static inline int
is_check_subpattern_true(const FklNastNode *exp, const FklCodegenInfo *info,
                         const FklCodegenOuterCtx *outer_ctx, uint32_t scope,
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
        fprintf(stderr, "[ERROR %s: %u]\tunreachable \n", __FUNCTION__,
                __LINE__);
        abort();
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
            fprintf(stderr, "[ERROR %s: %u]\tunreachable \n", __FUNCTION__,
                    __LINE__);
            abort();
            break;
        case FKL_NAST_PAIR:
            if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                    [FKL_CODEGEN_SUB_PATTERN_DEFINE],
                                exp, &ht)) {
                r = cfg_check_defined(exp, &ht, outer_ctx, curEnv, scope,
                                      errorState);
                if (errorState->type)
                    goto exit;
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                           [FKL_CODEGEN_SUB_PATTERN_IMPORT],
                                       exp, &ht)) {
                r = cfg_check_importable(exp, &ht, outer_ctx, curEnv, scope,
                                         errorState);
                if (errorState->type)
                    goto exit;
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                           [FKL_CODEGEN_SUB_PATTERN_DEFMACRO],
                                       exp, &ht)) {
                r = cfg_check_macro_defined(exp, &ht, outer_ctx, curEnv, scope,
                                            macroScope, info, errorState);
                if (errorState->type)
                    goto exit;
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                           [FKL_CODEGEN_SUB_PATTERN_EQ],
                                       exp, &ht)) {
                r = cfg_check_eq(exp, &ht, outer_ctx, curEnv, macroScope, info,
                                 errorState);
                if (errorState->type)
                    goto exit;
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                           [FKL_CODEGEN_SUB_PATTERN_MATCH],
                                       exp, &ht)) {
                r = cfg_check_matched(exp, &ht, outer_ctx, curEnv, macroScope,
                                      info, errorState);
                if (errorState->type)
                    goto exit;
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                           [FKL_CODEGEN_SUB_PATTERN_NOT],
                                       exp, &ht)) {
                ctx = cgCfgCtxVectorPushBack(&s, NULL);
                ctx->r = 0;
                ctx->rest = NULL;
                ctx->method = cfg_check_not_method;
                exp = *fklPmatchHashMapGet2(&ht,
                                            outer_ctx->builtInPatternVar_value);
                continue;
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                           [FKL_CODEGEN_SUB_PATTERN_AND],
                                       exp, &ht)) {
                const FklNastNode *rest = *fklPmatchHashMapGet2(
                    &ht, outer_ctx->builtInPatternVar_rest);
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
                    errorState->place =
                        fklMakeNastNodeRef(FKL_REMOVE_CONST(FklNastNode, exp));
                    goto exit;
                }
            } else if (fklPatternMatch(outer_ctx->builtin_sub_pattern_node
                                           [FKL_CODEGEN_SUB_PATTERN_OR],
                                       exp, &ht)) {
                const FklNastNode *rest = *fklPmatchHashMapGet2(
                    &ht, outer_ctx->builtInPatternVar_rest);
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
                    errorState->place =
                        fklMakeNastNodeRef(FKL_REMOVE_CONST(FklNastNode, exp));
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
    int r = is_check_subpattern_true(value, codegen, outer_ctx, scope,
                                     macroScope, curEnv, errorState);
    if (errorState->type)
        return;
    FklByteCodelnt *bcl = NULL;
    bcl = fklCreateSingleInsBclnt(
        create_op_ins(r ? FKL_OP_PUSH_1 : FKL_OP_PUSH_NIL), codegen->fid,
        origExp->curline, scope);
    push_single_bcl_codegen_quest(bcl, codegenQuestStack, scope, macroScope,
                                  curEnv, NULL, codegen, origExp->curline);
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
    int r = is_check_subpattern_true(cond, codegen, outer_ctx, scope,
                                     macroScope, curEnv, errorState);
    if (errorState->type)
        return;
    if (r) {
        FklNastNodeQueue *q = fklNastNodeQueueCreate();
        fklNastNodeQueuePush2(q, fklMakeNastNodeRef(value));
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            _default_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(1)),
            must_has_retval ? createMustHasRetvalQueueNextExpression(q)
                            : createDefaultQueueNextExpression(q),
            scope, macroScope, curEnv, value->curline, codegen,
            codegenQuestStack);
    } else {
        while (rest->type == FKL_NAST_PAIR) {
            FklNastNode *cond = rest->pair->car;
            rest = rest->pair->cdr;
            FklNastNode *value = rest->pair->car;
            rest = rest->pair->cdr;
            int r = is_check_subpattern_true(cond, codegen, outer_ctx, scope,
                                             macroScope, curEnv, errorState);
            if (errorState->type)
                return;
            if (r) {
                FklNastNodeQueue *q = fklNastNodeQueueCreate();
                fklNastNodeQueuePush2(q, fklMakeNastNodeRef(value));
                FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
                    _default_bc_process,
                    createDefaultStackContext(fklByteCodelntVectorCreate(1)),
                    must_has_retval ? createMustHasRetvalQueueNextExpression(q)
                                    : createDefaultQueueNextExpression(q),
                    scope, macroScope, curEnv, value->curline, codegen,
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
    push_default_codegen_quest(value, codegenQuestStack, scope, macroScope,
                               curEnv, NULL, codegen);
}

static inline void
unquoteHelperFunc(FklNastNode *value, FklCodegenQuestVector *codegenQuestStack,
                  uint32_t scope, FklCodegenMacroScope *macroScope,
                  FklCodegenEnv *curEnv, FklByteCodeProcesser func,
                  FklCodegenQuest *prev, FklCodegenInfo *codegen) {
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(value));
    FklCodegenQuest *quest = fklCreateCodegenQuest(
        func, createDefaultStackContext(fklByteCodelntVectorCreate(1)),
        createMustHasRetvalQueueNextExpression(queue), scope, macroScope,
        curEnv, value->curline, prev, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, quest);
}

static CODEGEN_FUNC(codegen_unquote) {
    FklNastNode *value =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    unquoteHelperFunc(value, codegenQuestStack, scope, macroScope, curEnv,
                      _default_bc_process, NULL, codegen);
}

BC_PROCESS(_qsquote_box_bc_process) {
    FklByteCodelntVector *stack = GET_STACK(context);
    FklByteCodelnt *retval = *fklByteCodelntVectorPopBack(stack);
    FklInstruction pushBox = {.op = FKL_OP_BOX, .ai = FKL_SUBOP_BOX_NEW_BOX};
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
    FklByteCodelnt *retval = *fklByteCodelntVectorPopBack(stack);
    FklByteCodelnt *other = fklByteCodelntVectorIsEmpty(stack)
                              ? NULL
                              : *fklByteCodelntVectorPopBack(stack);
    if (other) {
        while (!fklByteCodelntVectorIsEmpty(stack)) {
            FklByteCodelnt *cur = *fklByteCodelntVectorPopBack(stack);
            fklCodeLntConcat(other, cur);
            fklDestroyByteCodelnt(cur);
        }
        fklCodeLntReverseConcat(other, retval);
        fklDestroyByteCodelnt(other);
    }
    FklInstruction listPush = create_op_ins(FKL_OP_LIST_PUSH);
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
    FklInstruction pushPair = create_op_ins(FKL_OP_LIST_APPEND);
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
#include <fakeLisp/vector.h>

static CODEGEN_FUNC(codegen_qsquote) {
    FklNastNode *value =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_value);
    CgQsquoteHelperVector valueStack;
    cgQsquoteHelperVectorInit(&valueStack, 8);
    init_qsquote_helper_struct(cgQsquoteHelperVectorPushBack(&valueStack, NULL),
                               QSQUOTE_NONE, value, NULL);
    FklNastNode *const *builtInSubPattern = outer_ctx->builtin_sub_pattern_node;
    while (!cgQsquoteHelperVectorIsEmpty(&valueStack)) {
        const QsquoteHelperStruct *curNode =
            cgQsquoteHelperVectorPopBack(&valueStack);
        QsquoteHelperEnum e = curNode->e;
        FklCodegenQuest *prevQuest = curNode->prev;
        FklNastNode *curValue = curNode->node;
        switch (e) {
        case QSQUOTE_UNQTESP_CAR:
            unquoteHelperFunc(curValue, codegenQuestStack, scope, macroScope,
                              curEnv, _default_bc_process, prevQuest, codegen);
            break;
        case QSQUOTE_UNQTESP_VEC:
            unquoteHelperFunc(curValue, codegenQuestStack, scope, macroScope,
                              curEnv, _unqtesp_vec_bc_process, prevQuest,
                              codegen);
            break;
        case QSQUOTE_NONE: {
            FklPmatchHashMap unquoteHt;
            fklPmatchHashMapInit(&unquoteHt);
            if (fklPatternMatch(
                    builtInSubPattern[FKL_CODEGEN_SUB_PATTERN_UNQUOTE],
                    curValue, &unquoteHt)) {
                FklNastNode *unquoteValue = *fklPmatchHashMapGet2(
                    &unquoteHt, outer_ctx->builtInPatternVar_value);
                unquoteHelperFunc(unquoteValue, codegenQuestStack, scope,
                                  macroScope, curEnv, _default_bc_process,
                                  prevQuest, codegen);
            } else if (curValue->type == FKL_NAST_PAIR) {
                FklCodegenQuest *curQuest = fklCreateCodegenQuest(
                    _qsquote_pair_bc_process,
                    createDefaultStackContext(fklByteCodelntVectorCreate(2)),
                    NULL, scope, macroScope, curEnv, curValue->curline,
                    prevQuest, codegen);
                fklCodegenQuestVectorPushBack2(codegenQuestStack, curQuest);
                FklNastNode *node = curValue;
                for (;;) {
                    if (fklPatternMatch(
                            builtInSubPattern[FKL_CODEGEN_SUB_PATTERN_UNQUOTE],
                            node, &unquoteHt)) {
                        FklNastNode *unquoteValue = *fklPmatchHashMapGet2(
                            &unquoteHt, outer_ctx->builtInPatternVar_value);
                        unquoteHelperFunc(
                            unquoteValue, codegenQuestStack, scope, macroScope,
                            curEnv, _default_bc_process, curQuest, codegen);
                        break;
                    }
                    FklNastNode *cur = node->pair->car;
                    if (fklPatternMatch(
                            builtInSubPattern[FKL_CODEGEN_SUB_PATTERN_UNQTESP],
                            cur, &unquoteHt)) {
                        FklNastNode *unqtespValue = *fklPmatchHashMapGet2(
                            &unquoteHt, outer_ctx->builtInPatternVar_value);
                        if (node->pair->cdr->type != FKL_NAST_NIL) {
                            FklCodegenQuest *appendQuest =
                                fklCreateCodegenQuest(
                                    _qsquote_list_bc_process,
                                    createDefaultStackContext(
                                        fklByteCodelntVectorCreate(2)),
                                    NULL, scope, macroScope, curEnv,
                                    curValue->curline, curQuest, codegen);
                            fklCodegenQuestVectorPushBack2(codegenQuestStack,
                                                           appendQuest);
                            init_qsquote_helper_struct(
                                cgQsquoteHelperVectorPushBack(&valueStack,
                                                              NULL),
                                QSQUOTE_UNQTESP_CAR, unqtespValue, appendQuest);
                            init_qsquote_helper_struct(
                                cgQsquoteHelperVectorPushBack(&valueStack,
                                                              NULL),
                                QSQUOTE_NONE, node->pair->cdr, appendQuest);
                        } else
                            init_qsquote_helper_struct(
                                cgQsquoteHelperVectorPushBack(&valueStack,
                                                              NULL),
                                QSQUOTE_UNQTESP_CAR, unqtespValue, curQuest);
                        break;
                    } else
                        init_qsquote_helper_struct(
                            cgQsquoteHelperVectorPushBack(&valueStack, NULL),
                            QSQUOTE_NONE, cur, curQuest);
                    node = node->pair->cdr;
                    if (node->type != FKL_NAST_PAIR) {
                        init_qsquote_helper_struct(
                            cgQsquoteHelperVectorPushBack(&valueStack, NULL),
                            QSQUOTE_NONE, node, curQuest);
                        break;
                    }
                }
            } else if (curValue->type == FKL_NAST_VECTOR) {
                size_t vecSize = curValue->vec->size;
                FklCodegenQuest *curQuest = fklCreateCodegenQuest(
                    _qsquote_vec_bc_process,
                    createDefaultStackContext(
                        fklByteCodelntVectorCreate(vecSize)),
                    NULL, scope, macroScope, curEnv, curValue->curline,
                    prevQuest, codegen);
                fklCodegenQuestVectorPushBack2(codegenQuestStack, curQuest);
                for (size_t i = 0; i < vecSize; i++) {
                    if (fklPatternMatch(
                            builtInSubPattern[FKL_CODEGEN_SUB_PATTERN_UNQTESP],
                            curValue->vec->base[i], &unquoteHt))
                        init_qsquote_helper_struct(
                            cgQsquoteHelperVectorPushBack(&valueStack, NULL),
                            QSQUOTE_UNQTESP_VEC,
                            *fklPmatchHashMapGet2(
                                &unquoteHt, outer_ctx->builtInPatternVar_value),
                            curQuest);
                    else
                        init_qsquote_helper_struct(
                            cgQsquoteHelperVectorPushBack(&valueStack, NULL),
                            QSQUOTE_NONE, curValue->vec->base[i], curQuest);
                }
            } else if (curValue->type == FKL_NAST_BOX) {
                FklCodegenQuest *curQuest = fklCreateCodegenQuest(
                    _qsquote_box_bc_process,
                    createDefaultStackContext(fklByteCodelntVectorCreate(1)),
                    NULL, scope, macroScope, curEnv, curValue->curline,
                    prevQuest, codegen);
                fklCodegenQuestVectorPushBack2(codegenQuestStack, curQuest);
                init_qsquote_helper_struct(
                    cgQsquoteHelperVectorPushBack(&valueStack, NULL),
                    QSQUOTE_NONE, curValue->box, curQuest);
            } else
                push_default_codegen_quest(curValue, codegenQuestStack, scope,
                                           macroScope, curEnv, prevQuest,
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
        retval = *fklByteCodelntVectorPopBack(stack);
    else
        retval = fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL), fid,
                                         line, scope);
    return retval;
}

static inline int is_const_true_bytecode_lnt(const FklByteCodelnt *bcl) {
    const FklInstruction *cur = bcl->bc->code;
    const FklInstruction *const end = &cur[bcl->bc->len];
    for (; cur < end; cur++) {
        switch (cur->op) {
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
        case FKL_OP_PUSH_VEC:
        case FKL_OP_PUSH_VEC_C:
        case FKL_OP_PUSH_VEC_X:
        case FKL_OP_PUSH_VEC_XX:
        case FKL_OP_PUSH_BI:
        case FKL_OP_PUSH_BI_C:
        case FKL_OP_PUSH_BI_X:
        case FKL_OP_BOX:
        case FKL_OP_PUSH_BVEC:
        case FKL_OP_PUSH_BVEC_X:
        case FKL_OP_PUSH_BVEC_C:
        case FKL_OP_PUSH_HASHEQ:
        case FKL_OP_PUSH_HASHEQ_C:
        case FKL_OP_PUSH_HASHEQ_X:
        case FKL_OP_PUSH_HASHEQ_XX:
        case FKL_OP_PUSH_HASHEQV:
        case FKL_OP_PUSH_HASHEQV_C:
        case FKL_OP_PUSH_HASHEQV_X:
        case FKL_OP_PUSH_HASHEQV_XX:
        case FKL_OP_PUSH_HASHEQUAL:
        case FKL_OP_PUSH_HASHEQUAL_C:
        case FKL_OP_PUSH_HASHEQUAL_X:
        case FKL_OP_PUSH_HASHEQUAL_XX:
        case FKL_OP_PUSH_LIST:
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

        append_jmp_ins(INS_APPEND_BACK, retval, prev_len, JMP_UNCOND,
                       JMP_FORWARD, fid, line, scope);

        if (retval_len) {
            if (!true_bcl) {
                fklByteCodeLntInsertFrontIns(&drop, retval, fid, line, scope);
                append_jmp_ins(INS_APPEND_FRONT, retval, retval->bc->len,
                               JMP_IF_FALSE, JMP_FORWARD, fid, line, scope);
                fklCodeLntReverseConcat(first, retval);
            }
        } else
            fklCodeLntReverseConcat(first, retval);

        fklCodeLntConcat(retval, prev);
        fklDestroyByteCodelnt(prev);
        fklDestroyByteCodelnt(first);
    } else
        retval = *fklByteCodelntVectorPopBack(stack);
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
                append_jmp_ins(INS_APPEND_FRONT, retval, retval->bc->len,
                               JMP_IF_FALSE, JMP_FORWARD, fid, line, scope);
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
    FklCodegenQuest *quest = fklCreateCodegenQuest(
        _cond_exp_bc_process_0,
        createDefaultStackContext(fklByteCodelntVectorCreate(32)), NULL, scope,
        macroScope, curEnv, rest->curline, NULL, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, quest);
    if (rest->type != FKL_NAST_NIL) {
        FklNastNodeVector tmpStack;
        fklNastNodeVectorInit(&tmpStack, 32);
        pushListItemToStack(rest, &tmpStack, NULL);
        FklNastNode *lastExp = *fklNastNodeVectorPopBack(&tmpStack);
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
                createFirstHasRetvalQueueNextExpression(curQueue), curScope,
                cms, curEnv, curExp->curline, prevQuest, codegen);
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
        fklCodegenQuestVectorPushBack2(
            codegenQuestStack,
            fklCreateCodegenQuest(
                _cond_exp_bc_process_2,
                createDefaultStackContext(fklByteCodelntVectorCreate(32)),
                createFirstHasRetvalQueueNextExpression(lastQueue), curScope,
                cms, curEnv, lastExp->curline, prevQuest, codegen));
        fklNastNodeVectorUninit(&tmpStack);
    }
}

BC_PROCESS(_if_exp_bc_process_0) {
    FklByteCodelntVector *stack = GET_STACK(context);
    FklInstruction drop = create_op_ins(FKL_OP_DROP);

    if (stack->size >= 2) {
        FklByteCodelnt *exp = *fklByteCodelntVectorPopBack(stack);
        FklByteCodelnt *cond = *fklByteCodelntVectorPopBack(stack);
        fklByteCodeLntInsertFrontIns(&drop, exp, fid, line, scope);
        append_jmp_ins(INS_APPEND_BACK, cond, exp->bc->len, JMP_IF_FALSE,
                       JMP_FORWARD, fid, line, scope);
        fklCodeLntConcat(cond, exp);
        fklDestroyByteCodelnt(exp);
        return cond;
    } else if (stack->size >= 1)
        return *fklByteCodelntVectorPopBack(stack);
    else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL), fid,
                                       line, scope);
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
    fklCodegenQuestVectorPushBack2(
        codegenQuestStack,
        fklCreateCodegenQuest(
            _if_exp_bc_process_0,
            createDefaultStackContext(fklByteCodelntVectorCreate(2)),
            createMustHasRetvalQueueNextExpression(nextQueue), curScope, cms,
            curEnv, origExp->curline, NULL, codegen));
}

BC_PROCESS(_if_exp_bc_process_1) {
    FklByteCodelntVector *stack = GET_STACK(context);

    FklInstruction drop = create_op_ins(FKL_OP_DROP);

    if (stack->size >= 3) {
        FklByteCodelnt *exp0 = *fklByteCodelntVectorPopBack(stack);
        FklByteCodelnt *cond = *fklByteCodelntVectorPopBack(stack);
        FklByteCodelnt *exp1 = *fklByteCodelntVectorPopBack(stack);
        fklByteCodeLntInsertFrontIns(&drop, exp0, fid, line, scope);
        fklByteCodeLntInsertFrontIns(&drop, exp1, fid, line, scope);
        append_jmp_ins(INS_APPEND_BACK, exp0, exp1->bc->len, JMP_UNCOND,
                       JMP_FORWARD, fid, line, scope);
        append_jmp_ins(INS_APPEND_BACK, cond, exp0->bc->len, JMP_IF_FALSE,
                       JMP_FORWARD, fid, line, scope);
        fklCodeLntConcat(cond, exp0);
        fklCodeLntConcat(cond, exp1);
        fklDestroyByteCodelnt(exp0);
        fklDestroyByteCodelnt(exp1);
        return cond;
    } else if (stack->size >= 2) {
        FklByteCodelnt *exp0 = *fklByteCodelntVectorPopBack(stack);
        FklByteCodelnt *cond = *fklByteCodelntVectorPopBack(stack);
        fklByteCodeLntInsertFrontIns(&drop, exp0, fid, line, scope);
        append_jmp_ins(INS_APPEND_BACK, cond, exp0->bc->len, JMP_IF_FALSE,
                       JMP_FORWARD, fid, line, scope);
        fklCodeLntConcat(cond, exp0);
        fklDestroyByteCodelnt(exp0);
        return cond;
    } else if (stack->size >= 1)
        return *fklByteCodelntVectorPopBack(stack);
    else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL), fid,
                                       line, scope);
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
    FklCodegenQuest *prev = fklCreateCodegenQuest(
        _if_exp_bc_process_1,
        createDefaultStackContext(fklByteCodelntVectorCreate(2)),
        createMustHasRetvalQueueNextExpression(exp0Queue), curScope, cms,
        curEnv, origExp->curline, NULL, codegen);
    fklCodegenQuestVectorPushBack2(codegenQuestStack, prev);

    curScope = enter_new_scope(scope, curEnv);
    cms = fklCreateCodegenMacroScope(macroScope);
    fklCodegenQuestVectorPushBack2(
        codegenQuestStack,
        fklCreateCodegenQuest(
            _default_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(2)),
            createMustHasRetvalQueueNextExpression(exp1Queue), curScope, cms,
            curEnv, origExp->curline, prev, codegen));
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
            append_jmp_ins(INS_APPEND_FRONT, retval, retval->bc->len,
                           JMP_IF_FALSE, JMP_FORWARD, fid, line, scope);
        fklCodeLntReverseConcat(cond, retval);
        fklDestroyByteCodelnt(cond);
        check_and_close_ref(retval, scope, env, codegen->pts, fid, line);
        return retval;
    } else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL), fid,
                                       line, scope);
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
            append_jmp_ins(INS_APPEND_FRONT, retval, retval->bc->len,
                           JMP_IF_TRUE, JMP_FORWARD, fid, line, scope);
        fklCodeLntReverseConcat(cond, retval);
        fklDestroyByteCodelnt(cond);
        check_and_close_ref(retval, scope, env, codegen->pts, fid, line);
        return retval;
    } else
        return fklCreateSingleInsBclnt(create_op_ins(FKL_OP_PUSH_NIL), fid,
                                       line, scope);
}

static inline void codegen_when_unless(
    FklPmatchHashMap *ht, uint32_t scope, FklCodegenMacroScope *macroScope,
    FklCodegenEnv *curEnv, FklCodegenInfo *codegen,
    FklCodegenQuestVector *codegenQuestStack, FklByteCodeProcesser func,
    FklCodegenErrorState *errorState, FklNastNode *origExp) {
    FklNastNode *cond =
        *fklPmatchHashMapGet2(ht, codegen->outer_ctx->builtInPatternVar_value);

    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, codegen->outer_ctx->builtInPatternVar_rest);
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(cond));
    pushListItemToQueue(rest, queue, NULL);

    uint32_t curScope = enter_new_scope(scope, curEnv);
    FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        func, createDefaultStackContext(fklByteCodelntVectorCreate(32)),
        createFirstHasRetvalQueueNextExpression(queue), curScope, cms, curEnv,
        rest->curline, codegen, codegenQuestStack);
}

static CODEGEN_FUNC(codegen_when) {
    codegen_when_unless(ht, scope, macroScope, curEnv, codegen,
                        codegenQuestStack, _when_exp_bc_process, errorState,
                        origExp);
}

static CODEGEN_FUNC(codegen_unless) {
    codegen_when_unless(ht, scope, macroScope, curEnv, codegen,
                        codegenQuestStack, _unless_exp_bc_process, errorState,
                        origExp);
}

static FklCodegenInfo *createCodegenInfo(FklCodegenInfo *prev,
                                         const char *filename, int libMark,
                                         int macroMark,
                                         FklCodegenOuterCtx *outer_ctx) {
    FklCodegenInfo *r = (FklCodegenInfo *)malloc(sizeof(FklCodegenInfo));
    FKL_ASSERT(r);
    fklInitCodegenInfo(r, filename, prev, prev->runtime_symbol_table,
                       prev->runtime_kt, 1, libMark, macroMark, outer_ctx);
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
        (CodegenLoadContext *)malloc(sizeof(CodegenLoadContext));
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
        fklDestroyNastNode(fklAnalysisSymbolVectorPopBack(symbolStack)->ast);
    fklAnalysisSymbolVectorUninit(symbolStack);
    fklParseStateVectorUninit(&context->stateStack);
    fklUintVectorUninit(&context->lineStack);
    fclose(context->fp);
    free(context);
}

static inline FklNastNode *getExpressionFromFile(
    FklCodegenInfo *codegen, FILE *fp, int *unexpectEOF, size_t *errorLine,
    FklCodegenErrorState *errorState, FklAnalysisSymbolVector *symbolStack,
    FklUintVector *lineStack, FklParseStateVector *stateStack) {
    FklSymbolTable *pst = &codegen->outer_ctx->public_symbol_table;
    size_t size = 0;
    FklNastNode *begin = NULL;
    char *list = NULL;
    stateStack->size = 0;
    FklGrammer *g = *codegen->g;
    if (g) {
        codegen->outer_ctx->cur_file_dir = codegen->dir;
        fklParseStateVectorPushBack2(
            stateStack, (FklParseState){.state = &g->aTable.states[0]});
        list = fklReadWithAnalysisTable(
            g, fp, &size, codegen->curline, &codegen->curline, pst, unexpectEOF,
            errorLine, &begin, symbolStack, lineStack, stateStack);
    } else {
        fklNastPushState0ToStack(stateStack);
        list = fklReadWithBuiltinParser(
            fp, &size, codegen->curline, &codegen->curline, pst, unexpectEOF,
            errorLine, &begin, symbolStack, lineStack, stateStack);
    }
    if (list)
        free(list);
    if (*unexpectEOF)
        begin = NULL;
    while (!fklAnalysisSymbolVectorIsEmpty(symbolStack))
        fklDestroyNastNode(fklAnalysisSymbolVectorPopBack(symbolStack)->ast);
    lineStack->size = 0;
    return begin;
}

#include <fakeLisp/grammer.h>

static FklNastNode *
_codegen_load_get_next_expression(void *pcontext,
                                  FklCodegenErrorState *errorState) {
    CodegenLoadContext *context = pcontext;
    FklCodegenInfo *codegen = context->codegen;
    FklParseStateVector *stateStack = &context->stateStack;
    FklAnalysisSymbolVector *symbolStack = &context->symbolStack;
    FklUintVector *lineStack = &context->lineStack;
    FILE *fp = context->fp;
    int unexpectEOF = 0;
    size_t errorLine = 0;
    FklNastNode *begin =
        getExpressionFromFile(codegen, fp, &unexpectEOF, &errorLine, errorState,
                              symbolStack, lineStack, stateStack);
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

static FklCodegenNextExpression *
createFpNextExpression(FILE *fp, FklCodegenInfo *codegen) {
    CodegenLoadContext *context = createCodegenLoadContext(fp, codegen);
    return createCodegenNextExpression(
        &_codegen_load_get_next_expression_method_table, context, 0);
}

static inline void init_load_codegen_grammer_ptr(FklCodegenInfo *next,
                                                 FklCodegenInfo *prev) {
    next->g = &prev->self_g;
    next->named_prod_groups = &prev->self_named_prod_groups;
    next->unnamed_prods = &prev->self_unnamed_prods;
    next->unnamed_ignores = &prev->self_unnamed_ignores;
    next->builtin_prods = &prev->self_builtin_prods;
    next->builtin_ignores = &prev->self_builtin_ignores;
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
            FklNastNode *restExp =
                fklNastCons(fklMakeNastNodeRef(loadHead), rest, rest->curline);

            prevPair->cdr = fklCreateNastNode(FKL_NAST_NIL, rest->curline);

            fklNastNodeQueuePush2(queue, restExp);

            prevPair = rest->pair;
        }
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            _begin_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(2)),
            createDefaultQueueNextExpression(queue), scope, macroScope, curEnv,
            origExp->curline, codegen, codegenQuestStack);
    }

    const FklString *filenameStr = filename->str;
    if (!fklIsAccessibleRegFile(filenameStr->str)) {
        errorState->type = FKL_ERR_FILEFAILURE;
        errorState->place = fklMakeNastNodeRef(filename);
        return;
    }
    FklCodegenInfo *nextCodegen =
        createCodegenInfo(codegen, filenameStr->str, 0, 0, codegen->outer_ctx);
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
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        _begin_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(32)),
        createFpNextExpression(fp, nextCodegen), scope, macroScope, curEnv,
        origExp->curline, nextCodegen, codegenQuestStack);
}

static inline char *
combineFileNameFromListAndCheckPrivate(const FklNastNode *list,
                                       const FklSymbolTable *pst) {
    char *r = NULL;
    for (const FklNastNode *curPair = list; curPair->type == FKL_NAST_PAIR;
         curPair = curPair->pair->cdr) {
        FklNastNode *cur = curPair->pair->car;
        const FklString *str = fklGetSymbolWithId(cur->sym, pst)->k;
        if (curPair != list && str->str[0] == '_') {
            free(r);
            return NULL;
        }
        r = fklCstrStringCat(r, str);
        if (curPair->pair->cdr->type != FKL_NAST_NIL)
            r = fklStrCat(r, FKL_PATH_SEPARATOR_STR);
    }
    return r;
}

static void add_symbol_to_local_env_in_array(
    FklCodegenEnv *env, FklSymbolTable *symbolTable,
    const FklCgExportSidIdxHashMap *exports, FklByteCodelnt *bcl, FklSid_t fid,
    uint32_t line, uint32_t scope, FklSymbolTable *pst) {
    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
         l = l->next) {
        append_import_ins(INS_APPEND_BACK, bcl,
                          fklAddCodegenDefBySid(l->k, scope, env)->idx,
                          l->v.idx, fid, line, scope);
    }
}

static void add_symbol_with_prefix_to_local_env_in_array(
    FklCodegenEnv *env, FklSymbolTable *symbolTable, FklString *prefix,
    const FklCgExportSidIdxHashMap *exports, FklByteCodelnt *bcl, FklSid_t fid,
    uint32_t line, uint32_t scope, FklSymbolTable *pst) {
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
         l = l->next) {
        FklString *origSymbol = fklGetSymbolWithId(l->k, pst)->k;
        fklStringBufferConcatWithString(&buf, prefix);
        fklStringBufferConcatWithString(&buf, origSymbol);

        FklSid_t sym = fklAddSymbolCharBuf(buf.buf, buf.index, pst)->v;
        uint32_t idx = fklAddCodegenDefBySid(sym, scope, env)->idx;
        append_import_ins(INS_APPEND_BACK, bcl, idx, l->v.idx, fid, line,
                          scope);
        fklStringBufferClear(&buf);
    }
    fklUninitStringBuffer(&buf);
}

static FklNastNode *createPatternWithPrefixFromOrig(FklNastNode *orig,
                                                    FklString *prefix,
                                                    FklSymbolTable *pst) {
    const FklString *head = fklGetSymbolWithId(orig->pair->car->sym, pst)->k;
    FklString *symbolWithPrefix = fklStringAppend(prefix, head);
    FklNastNode *patternWithPrefix =
        fklNastConsWithSym(fklAddSymbol(symbolWithPrefix, pst)->v,
                           fklMakeNastNodeRef(orig->pair->cdr), orig->curline,
                           orig->pair->car->curline);
    free(symbolWithPrefix);
    return patternWithPrefix;
}

static FklNastNode *add_prefix_for_pattern_origin_exp(FklNastNode *orig,
                                                      FklString *prefix,
                                                      FklSymbolTable *pst) {
    FklNastNode *retval = fklCopyNastNode(orig);
    FklNastNode *head_node = caadr_nast_node(retval);
    const FklString *head = fklGetSymbolWithId(head_node->sym, pst)->k;
    FklString *symbolWithPrefix = fklStringAppend(prefix, head);
    FklSid_t id = fklAddSymbol(symbolWithPrefix, pst)->v;
    head_node->sym = id;
    free(symbolWithPrefix);
    return retval;
}

static FklNastNode *replace_pattern_origin_exp_head(FklNastNode *orig,
                                                    FklSid_t head) {
    FklNastNode *retval = fklCopyNastNode(orig);
    FklNastNode *head_node = caadr_nast_node(retval);
    head_node->sym = head;
    return retval;
}

static inline void
export_replacement_with_prefix(FklReplacementHashMap *replacements,
                               FklCodegenMacroScope *macros, FklString *prefix,
                               FklSymbolTable *pst) {
    for (FklReplacementHashMapNode *cur = replacements->first; cur;
         cur = cur->next) {
        FklString *origSymbol = fklGetSymbolWithId(cur->k, pst)->k;
        FklString *symbolWithPrefix = fklStringAppend(prefix, origSymbol);
        FklSid_t id = fklAddSymbol(symbolWithPrefix, pst)->v;
        fklReplacementHashMapAdd2(macros->replacements, id, cur->v);
        free(symbolWithPrefix);
    }
}

void fklPrintUndefinedRef(const FklCodegenEnv *env,
                          FklSymbolTable *globalSymTable, FklSymbolTable *pst) {
    const FklUnReSymbolRefVector *urefs = &env->uref;
    for (uint32_t i = urefs->size; i > 0; i--) {
        FklUnReSymbolRef *ref = &urefs->base[i - 1];
        fprintf(stderr, "warning: Symbol ");
        fklPrintRawSymbol(fklGetSymbolWithId(ref->id, pst)->k, stderr);
        fprintf(stderr, " is undefined at line %" FKL_PRT64U, ref->line);
        if (ref->fid) {
            fputs(" of ", stderr);
            fklPrintString(fklGetSymbolWithId(ref->fid, globalSymTable)->k,
                           stderr);
        }
        fputc('\n', stderr);
    }
}

typedef struct {
    FklCodegenInfo *codegen;
    FklCodegenEnv *env;
    FklCodegenMacroScope *cms;
    FklByteCodelntVector stack;
    FklNastNode *prefix;
    FklNastNode *only;
    FklNastNode *alias;
    FklNastNode *except;
    uint32_t scope;
} ExportContextData;

static void export_context_data_finalizer(void *data) {
    ExportContextData *d = (ExportContextData *)data;
    while (!fklByteCodelntVectorIsEmpty(&d->stack))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBack(&d->stack));
    fklByteCodelntVectorUninit(&d->stack);
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
    free(d);
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

static inline void recompute_ignores_terminal_sid_and_regex(
    FklGrammerIgnore *igs, FklSymbolTable *target_table,
    FklRegexTable *target_rt, const FklRegexTable *orig_rt) {
    FklGrammerIgnoreSym *syms = igs->ig;
    size_t len = igs->len;
    for (size_t i = len; i > 0; i--) {
        size_t idx = i - 1;
        FklGrammerIgnoreSym *cur = &syms[idx];
        if (cur->term_type == FKL_TERM_BUILTIN) {
            int failed = 0;
            cur->b.c = cur->b.t->ctx_create(
                idx + 1 < len ? syms[idx + 1].str : NULL, &failed);
        } else if (cur->term_type == FKL_TERM_REGEX)
            cur->re = fklAddRegexStr(
                target_rt, fklGetStringWithRegex(orig_rt, cur->re, NULL));
        else
            cur->str = fklAddSymbol(cur->str, target_table)->k;
    }
}

static inline void
recompute_prod_terminal_sid(FklGrammerProduction *prod,
                            FklSymbolTable *target_table,
                            const FklSymbolTable *origin_table) {
    size_t len = prod->len;
    FklGrammerSym *sym = prod->syms;
    for (size_t i = 0; i < len; i++) {
        FklGrammerSym *cur = &sym[i];
        if (cur->isterm && cur->term_type != FKL_TERM_BUILTIN)
            cur->nt.sid =
                fklAddSymbol(fklGetSymbolWithId(cur->nt.sid, origin_table)->k,
                             target_table)
                    ->v;
    }
}

static inline void replace_group_id(FklGrammerProduction *prod,
                                    FklSid_t new_id) {
    size_t len = prod->len;
    if (prod->left.group)
        prod->left.group = new_id;
    FklGrammerSym *sym = prod->syms;
    for (size_t i = 0; i < len; i++) {
        FklGrammerSym *cur = &sym[i];
        if (!cur->isterm && cur->nt.group)
            cur->nt.group = new_id;
    }
}

static inline int init_builtin_grammer(FklCodegenInfo *codegen,
                                       FklSymbolTable *st);
static inline int add_group_prods(FklGrammer *g, const FklProdHashMap *prods);
static inline int add_group_ignores(FklGrammer *g, FklGrammerIgnore *igs);
static inline FklGrammerProdGroupItem *
add_production_group(FklGraProdGroupHashMap *named_prod_groups,
                     FklSid_t group_id);
static inline int add_all_group_to_grammer(FklCodegenInfo *codegen);

static inline int
import_reader_macro(int is_grammer_inited, int *p_is_grammer_inited,
                    FklCodegenInfo *codegen, FklCodegenErrorState *errorState,
                    uint64_t curline, const FklCodegenLib *lib,
                    FklGrammerProdGroupItem *group, FklSid_t origin_group_id,
                    FklSid_t new_group_id) {
    FklGrammer *g = *codegen->g;
    FklSymbolTable *pst = &codegen->outer_ctx->public_symbol_table;
    if (!is_grammer_inited) {
        if (!g) {
            if (init_builtin_grammer(codegen, pst)) {
            reader_macro_error:
                errorState->line = curline;
                errorState->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
                errorState->place = NULL;
                return 1;
            }
            g = *codegen->g;
        } else {
            fklClearGrammer(g);
            if (add_group_ignores(g, *codegen->builtin_ignores))
                goto reader_macro_error;

            if (add_group_prods(g, codegen->builtin_prods))
                goto reader_macro_error;
        }
        *p_is_grammer_inited = 1;
    }

    const FklSymbolTable *origin_table = &lib->terminal_table;
    const FklRegexTable *orig_rt = &lib->regexes;
    FklGrammerProdGroupItem *target_group =
        add_production_group(codegen->named_prod_groups, new_group_id);

    for (FklGrammerIgnore *igs = group->ignore; igs; igs = igs->next) {
        FklGrammerIgnore *ig = (FklGrammerIgnore *)malloc(
            sizeof(FklGrammerIgnore) + igs->len * sizeof(FklGrammerIgnoreSym));
        FKL_ASSERT(ig);
        ig->len = igs->len;
        ig->next = NULL;
        memcpy(ig->ig, igs->ig, sizeof(FklGrammerIgnoreSym) * ig->len);

        recompute_ignores_terminal_sid_and_regex(ig, &g->terminals, &g->regexes,
                                                 orig_rt);
        if (fklAddIgnoreToIgnoreList(&target_group->ignore, ig)) {
            free(ig);
            goto reader_macro_error;
        }
    }

    for (FklProdHashMapNode *prod_hash_item = group->prods.first;
         prod_hash_item; prod_hash_item = prod_hash_item->next) {
        for (FklGrammerProduction *prods = prod_hash_item->v; prods;
             prods = prods->next) {
            FklGrammerProduction *prod =
                fklCopyUninitedGrammerProduction(prods);
            recompute_prod_terminal_sid(prod, &g->terminals, origin_table);
            if (origin_group_id != new_group_id)
                replace_group_id(prod, new_group_id);
            if (fklAddProdToProdTable(&target_group->prods, &g->builtins,
                                      prod)) {
                fklDestroyGrammerProduction(prod);
                goto reader_macro_error;
            }
        }
    }
    return 0;
}

static inline FklByteCodelnt *process_import_imported_lib_common(
    uint32_t libId, FklCodegenInfo *codegen, const FklCodegenLib *lib,
    FklCodegenEnv *curEnv, uint32_t scope, FklCodegenMacroScope *macroScope,
    uint64_t curline, FklCodegenErrorState *errorState, FklSymbolTable *pst) {
    for (FklCodegenMacro *cur = lib->head; cur; cur = cur->next)
        add_compiler_macro(&macroScope->head, fklMakeNastNodeRef(cur->pattern),
                           fklMakeNastNodeRef(cur->origin_exp), cur->bcl,
                           cur->prototype_id, 0);

    for (FklReplacementHashMapNode *cur = lib->replacements->first; cur;
         cur = cur->next) {
        fklReplacementHashMapAdd2(macroScope->replacements, cur->k, cur->v);
    }

    if (lib->named_prod_groups.buckets) {
        int is_grammer_inited = 0;
        for (FklGraProdGroupHashMapNode *prod_group_item =
                 lib->named_prod_groups.first;
             prod_group_item; prod_group_item = prod_group_item->next) {
            if (import_reader_macro(is_grammer_inited, &is_grammer_inited,
                                    codegen, errorState, curline, lib,
                                    &prod_group_item->v, prod_group_item->k,
                                    prod_group_item->k))
                break;
        }
        if (!errorState->type && add_all_group_to_grammer(codegen)) {
            errorState->line = curline;
            errorState->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
            errorState->place = NULL;
            return NULL;
        }
    }

    FklByteCodelnt *load_lib = append_load_lib_ins(
        INS_APPEND_BACK, NULL, libId, codegen->fid, curline, scope);

    add_symbol_to_local_env_in_array(curEnv, codegen->runtime_symbol_table,
                                     &lib->exports, load_lib, codegen->fid,
                                     curline, scope, pst);

    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib_prefix(
    uint32_t libId, FklCodegenInfo *codegen, const FklCodegenLib *lib,
    FklCodegenEnv *curEnv, uint32_t scope, FklCodegenMacroScope *macroScope,
    uint64_t curline, FklNastNode *prefixNode, FklCodegenErrorState *errorState,
    FklSymbolTable *pst) {
    FklString *prefix = fklGetSymbolWithId(prefixNode->sym, pst)->k;
    for (FklCodegenMacro *cur = lib->head; cur; cur = cur->next)
        add_compiler_macro(
            &macroScope->head,
            createPatternWithPrefixFromOrig(cur->pattern, prefix, pst),
            add_prefix_for_pattern_origin_exp(cur->origin_exp, prefix, pst),
            cur->bcl, cur->prototype_id, 0);

    export_replacement_with_prefix(lib->replacements, macroScope, prefix, pst);
    if (lib->named_prod_groups.buckets) {
        int is_grammer_inited = 0;
        FklStringBuffer buffer;
        fklInitStringBuffer(&buffer);
        for (FklGraProdGroupHashMapNode *prod_group_item =
                 lib->named_prod_groups.first;
             prod_group_item; prod_group_item = prod_group_item->next) {
            fklStringBufferConcatWithString(&buffer, prefix);
            fklStringBufferConcatWithString(
                &buffer, fklGetSymbolWithId(prod_group_item->k, pst)->k);

            FklSid_t group_id_with_prefix =
                fklAddSymbolCharBuf(buffer.buf, buffer.index, pst)->v;

            if (import_reader_macro(is_grammer_inited, &is_grammer_inited,
                                    codegen, errorState, curline, lib,
                                    &prod_group_item->v, prod_group_item->k,
                                    group_id_with_prefix))
                break;

            fklStringBufferClear(&buffer);
        }
        fklUninitStringBuffer(&buffer);
        if (!errorState->type && add_all_group_to_grammer(codegen)) {
            errorState->line = curline;
            errorState->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
            errorState->place = NULL;
            return NULL;
        }
    }
    FklByteCodelnt *load_lib = append_load_lib_ins(
        INS_APPEND_BACK, NULL, libId, codegen->fid, curline, scope);

    add_symbol_with_prefix_to_local_env_in_array(
        curEnv, codegen->runtime_symbol_table, prefix, &lib->exports, load_lib,
        codegen->fid, curline, scope, pst);

    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib_only(
    uint32_t libId, FklCodegenInfo *codegen, const FklCodegenLib *lib,
    FklCodegenEnv *curEnv, uint32_t scope, FklCodegenMacroScope *macroScope,
    uint64_t curline, FklNastNode *only, FklCodegenErrorState *errorState) {
    FklByteCodelnt *load_lib = append_load_lib_ins(
        INS_APPEND_BACK, NULL, libId, codegen->fid, curline, scope);

    const FklCgExportSidIdxHashMap *exports = &lib->exports;
    const FklReplacementHashMap *replace = lib->replacements;
    const FklCodegenMacro *head = lib->head;

    int is_grammer_inited = 0;
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
                                   macro->bcl, macro->prototype_id, 0);
            }
        }
        FklNastNode *rep = fklGetReplacement(cur, replace);
        if (rep) {
            r = 1;
            fklReplacementHashMapAdd2(macroScope->replacements, cur, rep);
        }

        FklGrammerProdGroupItem *group = NULL;
        if (lib->named_prod_groups.buckets
            && (group =
                    fklGraProdGroupHashMapGet2(&lib->named_prod_groups, cur))) {
            r = 1;
            if (import_reader_macro(is_grammer_inited, &is_grammer_inited,
                                    codegen, errorState, curline, lib, group,
                                    cur, cur))
                break;
        }

        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);
        if (item) {
            uint32_t idx = fklAddCodegenDefBySid(cur, scope, curEnv)->idx;
            append_import_ins(INS_APPEND_BACK, load_lib, idx, item->idx,
                              codegen->fid, only->curline, scope);
        } else if (!r) {
            errorState->type = FKL_ERR_IMPORT_MISSING;
            errorState->place = fklMakeNastNodeRef(only->pair->car);
            errorState->line = only->curline;
            errorState->fid =
                fklAddSymbolCstr(codegen->filename,
                                 &codegen->outer_ctx->public_symbol_table)
                    ->v;
            break;
        }
    }

    if (!errorState->type && is_grammer_inited
        && add_all_group_to_grammer(codegen)) {
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

static inline FklByteCodelnt *process_import_imported_lib_except(
    uint32_t libId, FklCodegenInfo *codegen, const FklCodegenLib *lib,
    FklCodegenEnv *curEnv, uint32_t scope, FklCodegenMacroScope *macroScope,
    uint64_t curline, FklNastNode *except, FklCodegenErrorState *errorState) {
    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    const FklReplacementHashMap *replace = lib->replacements;
    const FklCodegenMacro *head = lib->head;

    for (const FklCodegenMacro *macro = head; macro; macro = macro->next) {
        FklNastNode *patternHead = macro->pattern->pair->car;
        if (!is_in_except_list(except, patternHead->sym)) {
            add_compiler_macro(&macroScope->head,
                               fklMakeNastNodeRef(macro->pattern),
                               fklMakeNastNodeRef(macro->origin_exp),
                               macro->bcl, macro->prototype_id, 0);
        }
    }

    for (FklReplacementHashMapNode *reps = replace->first; reps;
         reps = reps->next) {
        if (!is_in_except_list(except, reps->k))
            fklReplacementHashMapAdd2(macroScope->replacements, reps->k,
                                      reps->v);
    }
    if (lib->named_prod_groups.buckets) {
        int is_grammer_inited = 0;
        for (FklGraProdGroupHashMapNode *prod_group_item =
                 lib->named_prod_groups.first;
             prod_group_item; prod_group_item = prod_group_item->next) {
            if (!is_in_except_list(except, prod_group_item->k)) {
                if (import_reader_macro(is_grammer_inited, &is_grammer_inited,
                                        codegen, errorState, curline, lib,
                                        &prod_group_item->v, prod_group_item->k,
                                        prod_group_item->k))
                    break;
            }
        }
        if (!errorState->type && add_all_group_to_grammer(codegen)) {
            errorState->line = curline;
            errorState->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
            errorState->place = NULL;
            return NULL;
        }
    }

    FklByteCodelnt *load_lib = append_load_lib_ins(
        INS_APPEND_BACK, NULL, libId, codegen->fid, curline, scope);

    FklSidHashSet excepts;
    fklSidHashSetInit(&excepts);

    for (FklNastNode *list = except; list->type == FKL_NAST_PAIR;
         list = list->pair->cdr)
        fklSidHashSetPut2(&excepts, list->pair->car->sym);

    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
         l = l->next) {
        if (!fklSidHashSetHas2(&excepts, l->k)) {
            uint32_t idx = fklAddCodegenDefBySid(l->k, scope, curEnv)->idx;

            append_import_ins(INS_APPEND_BACK, load_lib, idx, l->v.idx,
                              codegen->fid, except->curline, scope);
        }
    }
    fklSidHashSetUninit(&excepts);
    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib_alias(
    uint32_t libId, FklCodegenInfo *codegen, const FklCodegenLib *lib,
    FklCodegenEnv *curEnv, uint32_t scope, FklCodegenMacroScope *macroScope,
    uint64_t curline, FklNastNode *alias, FklCodegenErrorState *errorState,
    FklSymbolTable *pst) {
    FklByteCodelnt *load_lib = append_load_lib_ins(
        INS_APPEND_BACK, NULL, libId, codegen->fid, curline, scope);

    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    const FklReplacementHashMap *replace = lib->replacements;
    const FklCodegenMacro *head = lib->head;

    int is_grammer_inited = 0;
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
                FklNastNode *pattern = fklNastConsWithSym(
                    cur0, fklMakeNastNodeRef(orig->pair->cdr), orig->curline,
                    orig->pair->car->curline);

                add_compiler_macro(
                    &macroScope->head, pattern,
                    replace_pattern_origin_exp_head(macro->origin_exp, cur0),
                    macro->bcl, macro->prototype_id, 0);
            }
        }
        FklNastNode *rep = fklGetReplacement(cur, replace);
        if (rep) {
            r = 1;
            fklReplacementHashMapAdd2(macroScope->replacements, cur0, rep);
        }

        FklGrammerProdGroupItem *group = NULL;
        if (lib->named_prod_groups.buckets
            && (group =
                    fklGraProdGroupHashMapGet2(&lib->named_prod_groups, cur))) {
            r = 1;
            if (import_reader_macro(is_grammer_inited, &is_grammer_inited,
                                    codegen, errorState, curline, lib, group,
                                    cur, cur0))
                break;
        }

        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);
        if (item) {
            uint32_t idx = fklAddCodegenDefBySid(cur0, scope, curEnv)->idx;
            append_import_ins(INS_APPEND_BACK, load_lib, idx, item->idx,
                              codegen->fid, alias->curline, scope);
        } else if (!r) {
            errorState->type = FKL_ERR_IMPORT_MISSING;
            errorState->place = fklMakeNastNodeRef(curNode->pair->car);
            errorState->line = alias->curline;
            errorState->fid =
                fklAddSymbolCstr(codegen->filename,
                                 &codegen->outer_ctx->public_symbol_table)
                    ->v;
            break;
        }
    }

    if (!errorState->type && is_grammer_inited
        && add_all_group_to_grammer(codegen)) {
        errorState->line = curline;
        errorState->type = FKL_ERR_IMPORT_READER_MACRO_ERROR;
        errorState->place = NULL;
    }
    return load_lib;
}

static inline FklByteCodelnt *process_import_imported_lib(
    uint32_t libId, FklCodegenInfo *codegen, const FklCodegenLib *lib,
    FklCodegenEnv *env, uint32_t scope, FklCodegenMacroScope *cms,
    uint32_t line, FklNastNode *prefix, FklNastNode *only, FklNastNode *alias,
    FklNastNode *except, FklCodegenErrorState *errorState,
    FklSymbolTable *pst) {
    if (prefix)
        return process_import_imported_lib_prefix(libId, codegen, lib, env,
                                                  scope, cms, line, prefix,
                                                  errorState, pst);
    if (only)
        return process_import_imported_lib_only(libId, codegen, lib, env, scope,
                                                cms, line, only, errorState);
    if (alias)
        return process_import_imported_lib_alias(
            libId, codegen, lib, env, scope, cms, line, alias, errorState, pst);
    if (except)
        return process_import_imported_lib_except(
            libId, codegen, lib, env, scope, cms, line, except, errorState);
    return process_import_imported_lib_common(libId, codegen, lib, env, scope,
                                              cms, line, errorState, pst);
}

static inline int is_exporting_outer_ref_group(FklCodegenInfo *codegen) {
    for (FklSidHashSetNode *sid_list = codegen->export_named_prod_groups->first;
         sid_list; sid_list = sid_list->next) {
        FklSid_t id = sid_list->k;
        FklGrammerProdGroupItem *group =
            fklGraProdGroupHashMapGet2(codegen->named_prod_groups, id);
        if (group->is_ref_outer)
            return 1;
    }
    return 0;
}

static inline void process_export_bc(FklCodegenInfo *info,
                                     FklByteCodelnt *libBc, FklSid_t fid,
                                     uint32_t line, uint32_t scope) {
    append_export_to_ins(INS_APPEND_BACK, libBc, info->libStack->size + 1,
                         info->exports.count, fid, line, scope);
    for (const FklCgExportSidIdxHashMapNode *l = info->exports.first; l;
         l = l->next) {
        append_export_ins(INS_APPEND_BACK, libBc, l->v.oidx, fid, line, scope);
    }
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
    process_export_bc(codegen, libBc, fid, line, scope);

    FklInstruction ret = create_op_ins(FKL_OP_RET);

    fklByteCodeLntPushBackIns(libBc, &ret, fid, line, scope);

    fklPeepholeOptimize(libBc);
    FklCodegenLib *lib = fklCodegenLibVectorPushBack(codegen->libStack, NULL);
    fklInitCodegenScriptLib(lib, codegen, libBc, env);

    codegen->realpath = NULL;

    codegen->export_macro = NULL;
    codegen->export_replacement = NULL;
    return process_import_imported_lib(
        codegen->libStack->size, data->codegen, lib, data->env, data->scope,
        data->cms, line, data->prefix, data->only, data->alias, data->except,
        errorState, pst);
}

static FklCodegenQuestContext *
createExportContext(FklCodegenInfo *codegen, FklCodegenEnv *targetEnv,
                    uint32_t scope, FklCodegenMacroScope *cms,
                    FklNastNode *prefix, FklNastNode *only, FklNastNode *alias,
                    FklNastNode *except) {
    ExportContextData *data =
        (ExportContextData *)malloc(sizeof(ExportContextData));
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

static int recompute_import_dst_idx_func(void *cctx, FklOpcode *op,
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
    struct RecomputeImportSrcIdxCtx ctx = {
        .id = 0, .env = env, .id_base = idPstack->base};
    fklRecomputeInsImm(bcl, &ctx, recompute_import_dst_idx_predicate,
                       recompute_import_dst_idx_func);
    return ctx.id;
}

static inline FklByteCodelnt *
export_sequnce_exp_bc_process(FklByteCodelntVector *stack, FklSid_t fid,
                              uint32_t line, uint32_t scope) {
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
            errorState->fid =
                fid ? fklAddSymbol(
                          fklGetSymbolWithId(fid, codegen->runtime_symbol_table)
                              ->k,
                          &outer_ctx->public_symbol_table)
                          ->v
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
                fklCgExportSidIdxHashMapAdd(
                    exports, &id,
                    &(FklCodegenExportIdx){
                        .idx = idx,
                        .oidx = fklGetCodegenDefByIdInScope(id, 1, targetEnv)
                                    ->v.idx});
            }
        }
    }

    FklCodegenMacroScope *macros = targetEnv->macros;

    for (FklCodegenMacro *head = env->macros->head; head; head = head->next) {
        add_compiler_macro(&macros->head, fklMakeNastNodeRef(head->pattern),
                           fklMakeNastNodeRef(head->origin_exp), head->bcl,
                           head->prototype_id, 0);

        add_compiler_macro(&codegen->export_macro,
                           fklMakeNastNodeRef(head->pattern),
                           fklMakeNastNodeRef(head->origin_exp), head->bcl,
                           head->prototype_id, 0);
    }

    for (FklReplacementHashMapNode *cur = env->macros->replacements->first; cur;
         cur = cur->next) {
        fklReplacementHashMapAdd2(codegen->export_replacement, cur->k, cur->v);
        fklReplacementHashMapAdd2(macros->replacements, cur->k, cur->v);
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
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            _empty_bc_process, createEmptyContext(), NULL, scope, macroScope,
            curEnv, origExp->curline, codegen, codegenQuestStack);
    } else {
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
}

static void export_sequnce_context_data_finalizer(void *data) {
    ExportSequnceContextData *d = (ExportSequnceContextData *)data;
    while (!fklByteCodelntVectorIsEmpty(d->stack))
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBack(d->stack));
    fklByteCodelntVectorDestroy(d->stack);
    fklDestroyNastNode(d->origExp);
    free(d);
}

static FklByteCodelntVector *
export_sequnce_context_data_get_bcl_stack(void *data) {
    return ((ExportSequnceContextData *)data)->stack;
}

static void export_sequnce_context_data_put_bcl(void *data,
                                                FklByteCodelnt *bcl) {
    ExportSequnceContextData *d = (ExportSequnceContextData *)data;
    fklByteCodelntVectorPushBack2(d->stack, bcl);
}

static const FklCodegenQuestContextMethodTable ExportSequnceContextMethodTable =
    {
        .__finalizer = export_sequnce_context_data_finalizer,
        .__get_bcl_stack = export_sequnce_context_data_get_bcl_stack,
        .__put_bcl = export_sequnce_context_data_put_bcl,
};

static FklCodegenQuestContext *
create_export_sequnce_context(FklNastNode *origExp, FklByteCodelntVector *s,
                              uint8_t must_has_retval) {
    ExportSequnceContextData *data =
        (ExportSequnceContextData *)malloc(sizeof(ExportSequnceContextData));
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

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            exports_bc_process,
            create_export_sequnce_context(
                orig_exp, fklByteCodelntVectorCreate(2), must_has_retval),
            createDefaultQueueNextExpression(exportQueue), scope, macroScope,
            curEnv, origExp->curline, codegen, codegenQuestStack);
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
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBack(s));
    fklByteCodelntVectorUninit(s);
    free(ctx);
}

static void _export_define_context_put_bcl(void *data, FklByteCodelnt *bcl) {
    ExportDefineContext *ctx = data;
    FklByteCodelntVector *s = &ctx->stack;
    fklByteCodelntVectorPushBack2(s, bcl);
}

static FklByteCodelntVector *_export_define_context_get_bcl_stack(void *data) {
    return &((ExportDefineContext *)data)->stack;
}

static const FklCodegenQuestContextMethodTable ExportDefineContextMethodTable =
    {
        .__finalizer = _export_define_context_finalizer,
        .__put_bcl = _export_define_context_put_bcl,
        .__get_bcl_stack = _export_define_context_get_bcl_stack,
};

static inline FklCodegenQuestContext *
create_export_define_context(FklSid_t id, FklCodegenExportIdx *item) {
    ExportDefineContext *ctx =
        (ExportDefineContext *)malloc(sizeof(ExportDefineContext));
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
    value =
        fklTryExpandCodegenMacro(orig_value, codegen, macroScope, errorState);
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
        fklCodegenQuestVectorPushBack2(
            codegenQuestStack,
            fklCreateCodegenQuest(
                exports_bc_process,
                create_export_sequnce_context(
                    orig_exp, fklByteCodelntVectorCreate(2), must_has_retval),
                createDefaultQueueNextExpression(queue), 1, macroScope, curEnv,
                origExp->curline, NULL, codegen));
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
            item = fklCgExportSidIdxHashMapAdd(
                &libCodegen->exports, &name->sym,
                &(FklCodegenExportIdx){.idx = idx,
                                       .oidx = FKL_VAR_REF_INVALID_CIDX});
        }

        fklCodegenQuestVectorPushBack2(
            codegenQuestStack,
            fklCreateCodegenQuest(_export_define_bc_process,
                                  create_export_define_context(name->sym, item),
                                  createDefaultQueueNextExpression(queue), 1,
                                  macroScope, curEnv, origExp->curline, NULL,
                                  codegen));
    } else if (isExportDefmacroExp(value, builtin_pattern_node)) {
        if (must_has_retval)
            goto must_has_retval_error;

        FklCodegenMacroScope *cms = fklCreateCodegenMacroScope(macroScope);

        fklCodegenQuestVectorPushBack2(
            codegenQuestStack,
            fklCreateCodegenQuest(
                _export_macro_bc_process, createEmptyContext(),
                createDefaultQueueNextExpression(queue), 1, cms, curEnv,
                origExp->curline, NULL, codegen));
    } else if (isExportDefReaderMacroExp(value, builtin_pattern_node)) {
        if (must_has_retval)
            goto must_has_retval_error;

        FklSid_t group_id = get_reader_macro_group_id(value);
        if (!group_id)
            goto error;

        fklSidHashSetPut2(libCodegen->export_named_prod_groups, group_id);
        fklCodegenQuestVectorPushBack2(
            codegenQuestStack,
            fklCreateCodegenQuest(_empty_bc_process, createEmptyContext(),
                                  createDefaultQueueNextExpression(queue), 1,
                                  macroScope, curEnv, origExp->curline, NULL,
                                  codegen));
    } else if (isExportImportExp(value, builtin_pattern_node)) {

        if (fklPatternMatch(
                builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_NONE], value,
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

        fklCodegenQuestVectorPushBack2(
            codegenQuestStack,
            fklCreateCodegenQuest(
                _export_import_bc_process,
                createExportContext(libCodegen, curEnv, 1, cms, NULL, NULL,
                                    NULL, NULL),
                createDefaultQueueNextExpression(queue), 1, cms, exportEnv,
                origExp->curline, NULL, codegen));
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
                               FklCodegenLibVector *loadedLibStack) {
    for (size_t i = 0; i < loadedLibStack->size; i++) {
        const FklCodegenLib *cur = &loadedLibStack->base[i];
        if (!strcmp(realpath, cur->rp))
            return i + 1;
    }
    return 0;
}

static inline void process_import_script_common_header(
    FklNastNode *origExp, FklNastNode *name, const char *filename,
    FklCodegenEnv *curEnv, FklCodegenInfo *codegen,
    FklCodegenErrorState *errorState, FklCodegenQuestVector *codegenQuestStack,
    uint32_t scope, FklCodegenMacroScope *macroScope, FklNastNode *prefix,
    FklNastNode *only, FklNastNode *alias, FklNastNode *except,
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
    size_t libId = check_loaded_lib(nextCodegen->realpath, codegen->libStack);
    if (!libId) {
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            nextCodegen->refcount = 1;
            fklDestroyCodegenInfo(nextCodegen);
            return;
        }
        FklCodegenEnv *libEnv = fklCreateCodegenEnv(
            codegen->global_env, 0, codegen->global_env->macros);
        create_and_insert_to_pool(nextCodegen, 0, libEnv, 0, 1, pst);

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            _library_bc_process,
            createExportContext(codegen, curEnv, scope, macroScope, prefix,
                                only, alias, except),
            createFpNextExpression(fp, nextCodegen), 1, libEnv->macros, libEnv,
            origExp->curline, nextCodegen, codegenQuestStack);
    } else {
        fklReplacementHashMapDestroy(nextCodegen->export_replacement);
        nextCodegen->export_replacement = NULL;
        const FklCodegenLib *lib = &codegen->libStack->base[libId - 1];

        FklByteCodelnt *importBc = process_import_imported_lib(
            libId, codegen, lib, curEnv, scope, macroScope, origExp->curline,
            prefix, only, alias, except, errorState, pst);
        FklByteCodelntVector *bcStack = fklByteCodelntVectorCreate(1);
        fklByteCodelntVectorPushBack2(bcStack, importBc);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            _default_bc_process, createDefaultStackContext(bcStack), NULL, 1,
            codegen->global_env->macros, codegen->global_env, origExp->curline,
            nextCodegen, codegenQuestStack);
    }
}

FklCodegenDllLibInitExportFunc fklGetCodegenInitExportFunc(uv_lib_t *dll) {
    return fklGetAddress("_fklExportSymbolInit", dll);
}

static inline FklByteCodelnt *process_import_from_dll_only(
    FklNastNode *origExp, FklNastNode *name, FklNastNode *only,
    const char *filename, FklCodegenEnv *curEnv, FklCodegenInfo *codegen,
    FklCodegenErrorState *errorState, uint32_t scope, FklSymbolTable *pst) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libStack);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            free(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCodegenDllLibInitExportFunc initExport =
            fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            errorState->type = FKL_ERR_IMPORTFAILED;
            errorState->place = fklMakeNastNodeRef(name);
            free(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libStack, NULL);
        fklInitCodegenDllLib(lib, realpath, dll, codegen->runtime_symbol_table,
                             initExport, pst);
        libId = codegen->libStack->size;
    } else {
        lib = &codegen->libStack->base[libId - 1];
        dll = lib->dll;
        free(realpath);
    }

    FklByteCodelnt *load_dll = append_load_dll_ins(
        INS_APPEND_BACK, NULL, libId, codegen->fid, origExp->curline, scope);

    FklCgExportSidIdxHashMap *exports = &lib->exports;

    for (; only->type == FKL_NAST_PAIR; only = only->pair->cdr) {
        FklSid_t cur = only->pair->car->sym;
        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);
        if (item) {
            uint32_t idx = fklAddCodegenDefBySid(cur, scope, curEnv)->idx;
            append_import_ins(INS_APPEND_BACK, load_dll, idx, item->idx,
                              codegen->fid, only->curline, scope);
        } else {
            errorState->type = FKL_ERR_IMPORT_MISSING;
            errorState->place = fklMakeNastNodeRef(only->pair->car);
            errorState->line = only->curline;
            errorState->fid =
                fklAddSymbolCstr(codegen->filename,
                                 &codegen->outer_ctx->public_symbol_table)
                    ->v;
            break;
        }
    }

    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_except(
    FklNastNode *origExp, FklNastNode *name, FklNastNode *except,
    const char *filename, FklCodegenEnv *curEnv, FklCodegenInfo *codegen,
    FklCodegenErrorState *errorState, uint32_t scope, FklSymbolTable *pst) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libStack);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            free(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCodegenDllLibInitExportFunc initExport =
            fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            errorState->type = FKL_ERR_IMPORTFAILED;
            errorState->place = fklMakeNastNodeRef(name);
            free(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libStack, NULL);
        fklInitCodegenDllLib(lib, realpath, dll, codegen->runtime_symbol_table,
                             initExport, pst);
        libId = codegen->libStack->size;
    } else {
        lib = &codegen->libStack->base[libId - 1];
        dll = lib->dll;
        free(realpath);
    }

    FklByteCodelnt *load_dll = append_load_dll_ins(
        INS_APPEND_BACK, NULL, libId, codegen->fid, origExp->curline, scope);

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
            append_import_ins(INS_APPEND_BACK, load_dll, idx, l->v.idx,
                              codegen->fid, except->curline, scope);
        }
    }

    fklSidHashSetUninit(&excepts);
    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_common(
    FklNastNode *origExp, FklNastNode *name, const char *filename,
    FklCodegenEnv *curEnv, FklCodegenInfo *codegen,
    FklCodegenErrorState *errorState, uint32_t scope, FklSymbolTable *pst) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libStack);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            free(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCodegenDllLibInitExportFunc initExport =
            fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            errorState->type = FKL_ERR_IMPORTFAILED;
            errorState->place = fklMakeNastNodeRef(name);
            free(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libStack, NULL);
        fklInitCodegenDllLib(lib, realpath, dll, codegen->runtime_symbol_table,
                             initExport, pst);
        libId = codegen->libStack->size;
    } else {
        lib = &codegen->libStack->base[libId - 1];
        dll = lib->dll;
        free(realpath);
    }

    FklByteCodelnt *load_dll = append_load_dll_ins(
        INS_APPEND_BACK, NULL, libId, codegen->fid, origExp->curline, scope);

    add_symbol_to_local_env_in_array(curEnv, codegen->runtime_symbol_table,
                                     &lib->exports, load_dll, codegen->fid,
                                     origExp->curline, scope, pst);
    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_prefix(
    FklNastNode *origExp, FklNastNode *name, FklNastNode *prefixNode,
    const char *filename, FklCodegenEnv *curEnv, FklCodegenInfo *codegen,
    FklCodegenErrorState *errorState, uint32_t scope, FklSymbolTable *pst) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libStack);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            free(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCodegenDllLibInitExportFunc initExport =
            fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            errorState->type = FKL_ERR_IMPORTFAILED;
            errorState->place = fklMakeNastNodeRef(name);
            free(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libStack, NULL);
        fklInitCodegenDllLib(lib, realpath, dll, codegen->runtime_symbol_table,
                             initExport, pst);
        libId = codegen->libStack->size;
    } else {
        lib = &codegen->libStack->base[libId - 1];
        dll = lib->dll;
        free(realpath);
    }

    FklString *prefix = fklGetSymbolWithId(prefixNode->sym, pst)->k;

    FklByteCodelnt *load_dll = append_load_dll_ins(
        INS_APPEND_BACK, NULL, libId, codegen->fid, origExp->curline, scope);

    add_symbol_with_prefix_to_local_env_in_array(
        curEnv, codegen->runtime_symbol_table, prefix, &lib->exports, load_dll,
        codegen->fid, origExp->curline, scope, pst);

    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll_alias(
    FklNastNode *origExp, FklNastNode *name, FklNastNode *alias,
    const char *filename, FklCodegenEnv *curEnv, FklCodegenInfo *codegen,
    FklCodegenErrorState *errorState, uint32_t scope, FklSymbolTable *pst) {
    char *realpath = fklRealpath(filename);
    size_t libId = check_loaded_lib(realpath, codegen->libStack);
    uv_lib_t dll;
    FklCodegenLib *lib = NULL;
    if (!libId) {
        if (uv_dlopen(realpath, &dll)) {
            fprintf(stderr, "%s\n", uv_dlerror(&dll));
            errorState->type = FKL_ERR_FILEFAILURE;
            errorState->place = fklMakeNastNodeRef(name);
            free(realpath);
            uv_dlclose(&dll);
            return NULL;
        }
        FklCodegenDllLibInitExportFunc initExport =
            fklGetCodegenInitExportFunc(&dll);
        if (!initExport) {
            uv_dlclose(&dll);
            errorState->type = FKL_ERR_IMPORTFAILED;
            errorState->place = fklMakeNastNodeRef(name);
            free(realpath);
            return NULL;
        }
        lib = fklCodegenLibVectorPushBack(codegen->libStack, NULL);
        fklInitCodegenDllLib(lib, realpath, dll, codegen->runtime_symbol_table,
                             initExport, pst);
        libId = codegen->libStack->size;
    } else {
        lib = &codegen->libStack->base[libId - 1];
        dll = lib->dll;
        free(realpath);
    }

    FklByteCodelnt *load_dll = append_load_dll_ins(
        INS_APPEND_BACK, NULL, libId, codegen->fid, origExp->curline, scope);

    FklCgExportSidIdxHashMap *exports = &lib->exports;

    for (; alias->type == FKL_NAST_PAIR; alias = alias->pair->cdr) {
        FklNastNode *curNode = alias->pair->car;
        FklSid_t cur = curNode->pair->car->sym;
        FklCodegenExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);

        if (item) {
            FklSid_t cur0 = curNode->pair->cdr->pair->car->sym;
            uint32_t idx = fklAddCodegenDefBySid(cur0, scope, curEnv)->idx;
            append_import_ins(INS_APPEND_BACK, load_dll, idx, item->idx,
                              codegen->fid, alias->curline, scope);
        } else {
            errorState->type = FKL_ERR_IMPORT_MISSING;
            errorState->place = fklMakeNastNodeRef(curNode->pair->car);
            errorState->line = alias->curline;
            errorState->fid =
                fklAddSymbolCstr(codegen->filename,
                                 &codegen->outer_ctx->public_symbol_table)
                    ->v;
            break;
        }
    }

    return load_dll;
}

static inline FklByteCodelnt *process_import_from_dll(
    FklNastNode *origExp, FklNastNode *name, const char *filename,
    FklCodegenEnv *curEnv, FklCodegenInfo *codegen,
    FklCodegenErrorState *errorState, uint32_t scope, FklNastNode *prefix,
    FklNastNode *only, FklNastNode *alias, FklNastNode *except,
    FklSymbolTable *pst) {
    if (prefix)
        return process_import_from_dll_prefix(origExp, name, prefix, filename,
                                              curEnv, codegen, errorState,
                                              scope, pst);
    if (only)
        return process_import_from_dll_only(origExp, name, only, filename,
                                            curEnv, codegen, errorState, scope,
                                            pst);
    if (alias)
        return process_import_from_dll_alias(origExp, name, alias, filename,
                                             curEnv, codegen, errorState, scope,
                                             pst);
    if (except)
        return process_import_from_dll_except(origExp, name, except, filename,
                                              curEnv, codegen, errorState,
                                              scope, pst);
    return process_import_from_dll_common(origExp, name, filename, curEnv,
                                          codegen, errorState, scope, pst);
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

static inline void codegen_import_helper(
    FklNastNode *origExp, FklNastNode *name, FklNastNode *rest,
    FklCodegenInfo *codegen, FklCodegenErrorState *errorState,
    FklCodegenEnv *curEnv, uint32_t scope, FklCodegenMacroScope *macroScope,
    FklCodegenQuestVector *codegenQuestStack, FklNastNode *prefix,
    FklNastNode *only, FklNastNode *alias, FklNastNode *except,
    FklSymbolTable *pst) {
    if (name->type == FKL_NAST_NIL
        || !fklIsNastNodeListAndHasSameType(name, FKL_NAST_SYM)
        || (prefix && prefix->type != FKL_NAST_SYM)
        || (only && !fklIsNastNodeListAndHasSameType(only, FKL_NAST_SYM))
        || (except && !fklIsNastNodeListAndHasSameType(except, FKL_NAST_SYM))
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
                                               rest, rest->curline);

            prevPair->cdr = fklCreateNastNode(FKL_NAST_NIL, rest->curline);

            fklNastNodeQueuePush2(queue, restExp);

            prevPair = rest->pair;
        }
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            _begin_exp_bc_process,
            createDefaultStackContext(fklByteCodelntVectorCreate(2)),
            createDefaultQueueNextExpression(queue), scope, macroScope, curEnv,
            origExp->curline, codegen, codegenQuestStack);
    }

    char *filename = combineFileNameFromListAndCheckPrivate(name, pst);

    if (filename == NULL) {
        errorState->type = FKL_ERR_IMPORTFAILED;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }

    char *packageMainFileName =
        fklStrCat(fklCopyCstr(filename), FKL_PATH_SEPARATOR_STR);
    packageMainFileName = fklStrCat(packageMainFileName, FKL_PACKAGE_MAIN_FILE);

    char *preCompileFileName = fklStrCat(fklCopyCstr(packageMainFileName),
                                         FKL_PRE_COMPILE_FKL_SUFFIX_STR);

    char *scriptFileName =
        fklStrCat(fklCopyCstr(filename), FKL_SCRIPT_FILE_EXTENSION);

    char *dllFileName = fklStrCat(fklCopyCstr(filename), FKL_DLL_FILE_TYPE);

    if (fklIsAccessibleRegFile(packageMainFileName))
        process_import_script_common_header(
            origExp, name, packageMainFileName, curEnv, codegen, errorState,
            codegenQuestStack, scope, macroScope, prefix, only, alias, except,
            pst);
    else if (fklIsAccessibleRegFile(scriptFileName))
        process_import_script_common_header(
            origExp, name, scriptFileName, curEnv, codegen, errorState,
            codegenQuestStack, scope, macroScope, prefix, only, alias, except,
            pst);
    else if (fklIsAccessibleRegFile(preCompileFileName)) {
        size_t libId = check_loaded_lib(preCompileFileName, codegen->libStack);
        if (!libId) {
            char *errorStr = NULL;
            if (fklLoadPreCompile(codegen->pts, codegen->macro_pts,
                                  codegen->libStack, codegen->macroLibStack,
                                  codegen->runtime_symbol_table,
                                  codegen->runtime_kt, codegen->outer_ctx,
                                  preCompileFileName, NULL, &errorStr)) {
                if (errorStr) {
                    fprintf(stderr, "%s\n", errorStr);
                    free(errorStr);
                }
                errorState->type = FKL_ERR_IMPORTFAILED;
                errorState->place = fklMakeNastNodeRef(name);
                goto exit;
            }
            libId = codegen->libStack->size;
        }
        const FklCodegenLib *lib = &codegen->libStack->base[libId - 1];

        FklByteCodelnt *importBc = process_import_imported_lib(
            libId, codegen, lib, curEnv, scope, macroScope, origExp->curline,
            prefix, only, alias, except, errorState, pst);
        FklByteCodelntVector *bcStack = fklByteCodelntVectorCreate(1);
        fklByteCodelntVectorPushBack2(bcStack, importBc);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            _default_bc_process, createDefaultStackContext(bcStack), NULL, 1,
            codegen->global_env->macros, codegen->global_env, origExp->curline,
            codegen, codegenQuestStack);
    } else if (fklIsAccessibleRegFile(dllFileName)) {
        FklByteCodelnt *bc = process_import_from_dll(
            origExp, name, dllFileName, curEnv, codegen, errorState, scope,
            prefix, only, alias, except, pst);
        if (bc) {
            FklByteCodelntVector *bcStack = fklByteCodelntVectorCreate(1);
            fklByteCodelntVectorPushBack2(bcStack, bc);
            FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
                _default_bc_process, createDefaultStackContext(bcStack), NULL,
                1, NULL, NULL, origExp->curline, codegen, codegenQuestStack);
        }
    } else {
        errorState->type = FKL_ERR_IMPORTFAILED;
        errorState->place = fklMakeNastNodeRef(name);
    }
exit:
    free(filename);
    free(scriptFileName);
    free(dllFileName);
    free(packageMainFileName);
    free(preCompileFileName);
}

static CODEGEN_FUNC(codegen_import) {
    FklNastNode *name =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp, name, rest, codegen, errorState, curEnv,
                          scope, macroScope, codegenQuestStack, NULL, NULL,
                          NULL, NULL, &outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_none) {
    if (must_has_retval) {
        errorState->type = FKL_ERR_EXP_HAS_NO_VALUE;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    fklCodegenQuestVectorPushBack2(
        codegenQuestStack,
        fklCreateCodegenQuest(_empty_bc_process, createEmptyContext(), NULL, 1,
                              macroScope, curEnv, origExp->curline, NULL,
                              codegen));
}

static CODEGEN_FUNC(codegen_import_prefix) {
    FklNastNode *name =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *prefix =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp, name, rest, codegen, errorState, curEnv,
                          scope, macroScope, codegenQuestStack, prefix, NULL,
                          NULL, NULL, &outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_only) {
    FklNastNode *name =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *only =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp, name, rest, codegen, errorState, curEnv,
                          scope, macroScope, codegenQuestStack, NULL, only,
                          NULL, NULL, &outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_alias) {
    FklNastNode *name =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *alias =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp, name, rest, codegen, errorState, curEnv,
                          scope, macroScope, codegenQuestStack, NULL, NULL,
                          alias, NULL, &outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_except) {
    FklNastNode *name =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *except =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp, name, rest, codegen, errorState, curEnv,
                          scope, macroScope, codegenQuestStack, NULL, NULL,
                          NULL, except, &outer_ctx->public_symbol_table);
}

static CODEGEN_FUNC(codegen_import_common) {
    FklNastNode *name =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_name);
    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_args);

    codegen_import_helper(origExp, name, rest, codegen, errorState, curEnv,
                          scope, macroScope, codegenQuestStack, NULL, NULL,
                          NULL, NULL, &outer_ctx->public_symbol_table);
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
    MacroContext *r = (MacroContext *)malloc(sizeof(MacroContext));
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
    FklByteCodelnt *macroBcl = *fklByteCodelntVectorPopBack(stack);
    FklInstruction ret = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(macroBcl, &ret, fid, line, scope);

    FklNastNode *pattern = d->pattern;
    FklCodegenMacroScope *macros = d->macroScope;
    uint32_t prototype_id = d->prototype_id;

    fklPeepholeOptimize(macroBcl);
    add_compiler_macro(&macros->head, fklMakeNastNodeRef(pattern),
                       fklMakeNastNodeRef(d->origin_exp), macroBcl,
                       prototype_id, 1);
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
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBack(s));
    fklByteCodelntVectorUninit(s);
    fklDestroyNastNode(d->pattern);
    fklDestroyNastNode(d->origin_exp);
    if (d->macroScope)
        fklDestroyCodegenMacroScope(d->macroScope);
    free(d);
}

static const FklCodegenQuestContextMethodTable MacroStackContextMethodTable = {
    .__get_bcl_stack = _macro_stack_context_get_bcl_stack,
    .__put_bcl = _macro_stack_context_put_bcl,
    .__finalizer = _macro_stack_context_finalizer,
};

static inline FklCodegenQuestContext *
createMacroQuestContext(FklNastNode *origin_exp, FklNastNode *pattern,
                        FklCodegenMacroScope *macroScope,
                        uint32_t prototype_id) {
    return createCodegenQuestContext(
        createMacroContext(origin_exp, pattern, macroScope, prototype_id),
        &MacroStackContextMethodTable);
}

struct CustomActionCtx {
    uint64_t refcount;
    FklByteCodelnt *bcl;
    FklFuncPrototypes *pts;
    FklCodegenLibVector *macroLibStack;
    FklCodegenOuterCtx *codegen_outer_ctx;
    FklSymbolTable *pst;
    uint32_t prototype_id;
};

struct ReaderMacroCtx {
    FklByteCodelntVector stack;
    FklFuncPrototypes *pts;
    struct CustomActionCtx *action_ctx;
};

static void *custom_action(void *c, void *outerCtx, void *nodes[], size_t num,
                           size_t line) {
    FklNastNode *nodes_vector = fklCreateNastNode(FKL_NAST_VECTOR, line);
    nodes_vector->vec = fklCreateNastVector(num);
    for (size_t i = 0; i < num; i++)
        nodes_vector->vec->base[i] = fklMakeNastNodeRef(nodes[i]);
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
    fklPmatchHashMapAdd2(&ht, fklAddSymbolCstr("$$", ctx->pst)->v,
                         nodes_vector);
    fklPmatchHashMapAdd2(&ht, fklAddSymbolCstr("line", ctx->pst)->v, line_node);

    FklNastNode *r = NULL;
    const char *cwd = ctx->codegen_outer_ctx->cwd;
    const char *file_dir = ctx->codegen_outer_ctx->cur_file_dir;
    fklChdir(file_dir);

    FklVM *anotherVM =
        fklInitMacroExpandVM(ctx->bcl, ctx->pts, ctx->prototype_id, &ht,
                             &lineHash, ctx->macroLibStack, &r, line, ctx->pst,
                             &ctx->codegen_outer_ctx->public_kt);
    FklVMgc *gc = anotherVM->gc;

    int e = fklRunVM(anotherVM);

    fklChdir(cwd);

    gc->pts = NULL;
    if (e)
        fklDeleteCallChain(anotherVM);

    fklDestroyNastNode(nodes_vector);
    fklDestroyNastNode(line_node);

    fklPmatchHashMapUninit(&ht);
    fklLineNumHashMapUninit(&lineHash);
    fklDestroyAllVMs(anotherVM);
    fklDestroyVMgc(gc);
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
        free(ctx);
    }
}

static inline struct ReaderMacroCtx *
createReaderMacroContext(struct CustomActionCtx *ctx, FklFuncPrototypes *pts) {
    struct ReaderMacroCtx *r =
        (struct ReaderMacroCtx *)malloc(sizeof(struct ReaderMacroCtx));
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
    FklByteCodelnt *macroBcl = *fklByteCodelntVectorPopBack(stack);
    FklInstruction ret = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(macroBcl, &ret, fid, line, scope);

    fklPeepholeOptimize(macroBcl);
    custom_ctx->pts = pts;
    custom_ctx->bcl = macroBcl;
    return NULL;
}

static FklByteCodelntVector *
_reader_macro_stack_context_get_bcl_stack(void *data) {
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
        fklDestroyByteCodelnt(*fklByteCodelntVectorPopBack(s));
    fklByteCodelntVectorUninit(s);
    if (d->action_ctx)
        custom_action_ctx_destroy(d->action_ctx);
    free(d);
}

static const FklCodegenQuestContextMethodTable
    ReaderMacroStackContextMethodTable = {
        .__get_bcl_stack = _reader_macro_stack_context_get_bcl_stack,
        .__put_bcl = _reader_macro_stack_context_put_bcl,
        .__finalizer = _reader_macro_stack_context_finalizer,
};

static inline FklCodegenQuestContext *
createReaderMacroQuestContext(struct CustomActionCtx *ctx,
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

static inline ReplacementFunc
findBuiltInReplacementWithId(FklSid_t id,
                             const FklSid_t builtin_replacement_id[]) {
    for (size_t i = 0; i < FKL_BUILTIN_REPLACEMENT_NUM; i++) {
        if (builtin_replacement_id[i] == id)
            return builtInSymbolReplacement[i].func;
    }
    return NULL;
}

static void *codegen_prod_action_nil(void *ctx, void *outerCtx, void *nodes[],
                                     size_t num, size_t line) {
    return fklCreateNastNode(FKL_NAST_NIL, line);
}

static void *codegen_prod_action_first(void *ctx, void *outerCtx, void *nodes[],
                                       size_t num, size_t line) {
    if (num < 1)
        return NULL;
    return fklMakeNastNodeRef(nodes[0]);
}

static void *codegen_prod_action_symbol(void *ctx, void *outerCtx,
                                        void *nodes[], size_t num,
                                        size_t line) {
    if (num < 1)
        return NULL;
    FklNastNode *node = nodes[0];
    if (node->type != FKL_NAST_STR)
        return NULL;
    FklNastNode *sym = fklCreateNastNode(FKL_NAST_SYM, node->curline);
    sym->sym = fklAddSymbol(node->str, outerCtx)->v;
    return sym;
}

static void *codegen_prod_action_second(void *ctx, void *outerCtx,
                                        void *nodes[], size_t num,
                                        size_t line) {
    if (num < 2)
        return NULL;
    return fklMakeNastNodeRef(nodes[1]);
}

static void *codegen_prod_action_third(void *ctx, void *outerCtx, void *nodes[],
                                       size_t num, size_t line) {
    if (num < 3)
        return NULL;
    return fklMakeNastNodeRef(nodes[2]);
}

static inline void *codegen_prod_action_pair(void *ctx, void *outerCtx,
                                             void *nodes[], size_t num,
                                             size_t line) {
    if (num < 3)
        return NULL;
    FklNastNode *car = nodes[0];
    FklNastNode *cdr = nodes[2];
    FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
    pair->pair = fklCreateNastPair();
    pair->pair->car = fklMakeNastNodeRef(car);
    pair->pair->cdr = fklMakeNastNodeRef(cdr);
    return pair;
}

static inline void *codegen_prod_action_cons(void *ctx, void *outerCtx,
                                             void *nodes[], size_t num,
                                             size_t line) {
    if (num == 1) {
        FklNastNode *car = nodes[0];
        FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
        pair->pair = fklCreateNastPair();
        pair->pair->car = fklMakeNastNodeRef(car);
        pair->pair->cdr = fklCreateNastNode(FKL_NAST_NIL, line);
        return pair;
    } else if (num == 2) {
        FklNastNode *car = nodes[0];
        FklNastNode *cdr = nodes[1];
        FklNastNode *pair = fklCreateNastNode(FKL_NAST_PAIR, line);
        pair->pair = fklCreateNastPair();
        pair->pair->car = fklMakeNastNodeRef(car);
        pair->pair->cdr = fklMakeNastNodeRef(cdr);
        return pair;
    } else
        return NULL;
}

static inline void *codegen_prod_action_box(void *ctx, void *outerCtx,
                                            void *nodes[], size_t num,
                                            size_t line) {
    if (num < 2)
        return NULL;
    FklNastNode *box = fklCreateNastNode(FKL_NAST_BOX, line);
    box->box = fklMakeNastNodeRef(nodes[1]);
    return box;
}

static inline void *codegen_prod_action_vector(void *ctx, void *outerCtx,
                                               void *nodes[], size_t num,
                                               size_t line) {
    if (num < 2)
        return NULL;
    FklNastNode *list = nodes[1];
    FklNastNode *r = fklCreateNastNode(FKL_NAST_VECTOR, line);
    size_t len = fklNastListLength(list);
    FklNastVector *vec = fklCreateNastVector(len);
    r->vec = vec;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++)
        vec->base[i] = fklMakeNastNodeRef(list->pair->car);
    return r;
}

static inline void *codegen_prod_action_quote(void *ctx, void *outerCtx,
                                              void *nodes[], size_t num,
                                              size_t line) {
    if (num < 2)
        return NULL;
    FklSid_t id = fklAddSymbolCstr("quote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static inline void *codegen_prod_action_unquote(void *ctx, void *outerCtx,
                                                void *nodes[], size_t num,
                                                size_t line) {
    if (num < 2)
        return NULL;
    FklSid_t id = fklAddSymbolCstr("unquote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static inline void *codegen_prod_action_qsquote(void *ctx, void *outerCtx,
                                                void *nodes[], size_t num,
                                                size_t line) {
    if (num < 2)
        return NULL;
    FklSid_t id = fklAddSymbolCstr("qsquote", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static inline void *codegen_prod_action_unqtesp(void *ctx, void *outerCtx,
                                                void *nodes[], size_t num,
                                                size_t line) {
    if (num < 2)
        return NULL;
    FklSid_t id = fklAddSymbolCstr("unqtesp", outerCtx)->v;
    FklNastNode *s_exp = fklMakeNastNodeRef(nodes[1]);
    FklNastNode *head = fklCreateNastNode(FKL_NAST_SYM, line);
    head->sym = id;
    FklNastNode *s_exps[] = {head, s_exp};
    return create_nast_list(s_exps, 2, line);
}

static inline void *codegen_prod_action_hasheq(void *ctx, void *outerCtx,
                                               void *nodes[], size_t num,
                                               size_t line) {
    if (num < 2)
        return NULL;
    FklNastNode *list = nodes[1];
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

static inline void *codegen_prod_action_hasheqv(void *ctx, void *outerCtx,
                                                void *nodes[], size_t num,
                                                size_t line) {
    if (num < 2)
        return 0;
    FklNastNode *list = nodes[1];
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

static inline void *codegen_prod_action_hashequal(void *ctx, void *outerCtx,
                                                  void *nodes[], size_t num,
                                                  size_t line) {
    if (num < 2)
        return 0;
    FklNastNode *list = nodes[1];
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

static inline void *codegen_prod_action_bytevector(void *ctx, void *outerCtx,
                                                   void *nodes[], size_t num,
                                                   size_t line) {
    if (num < 2)
        return NULL;
    FklNastNode *list = nodes[1];
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

static inline void
init_builtin_prod_action_list(FklSid_t *builtin_prod_action_id,
                              FklSymbolTable *pst) {
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++)
        builtin_prod_action_id[i] =
            fklAddSymbolCstr(CodegenProdActions[i].name, pst)->v;
}

static inline FklProdActionFunc
find_builtin_prod_action(FklSid_t id, FklSid_t *builtin_prod_action_id) {
    for (size_t i = 0; i < FKL_CODEGEN_BUILTIN_PROD_ACTION_NUM; i++) {
        if (builtin_prod_action_id[i] == id)
            return CodegenProdActions[i].func;
    }
    return NULL;
}

static void *simple_action_nth_create(FklNastNode *rest[], size_t rest_len,
                                      int *failed) {
    if (rest_len != 1 || rest[0]->type != FKL_NAST_FIX || rest[0]->fix < 0) {
        *failed = 1;
        return NULL;
    }
    return (void *)(rest[0]->fix);
}

static void *simple_action_nth(void *ctx, void *outerCtx, void *nodes[],
                               size_t num, size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    return fklMakeNastNodeRef(nodes[nth]);
}

struct SimpleActionConsCtx {
    uint64_t car;
    uint64_t cdr;
};

static void *simple_action_cons_create(FklNastNode *rest[], size_t rest_len,
                                       int *failed) {
    if (rest_len != 2 || rest[0]->type != FKL_NAST_FIX || rest[0]->fix < 0
        || rest[1]->type != FKL_NAST_FIX || rest[1]->fix < 0) {
        *failed = 1;
        return NULL;
    }
    struct SimpleActionConsCtx *ctx = (struct SimpleActionConsCtx *)malloc(
        sizeof(struct SimpleActionConsCtx));
    FKL_ASSERT(ctx);
    ctx->car = rest[0]->fix;
    ctx->cdr = rest[1]->fix;
    return ctx;
}

static void *simple_action_cons_copy(const void *c) {
    struct SimpleActionConsCtx *ctx = (struct SimpleActionConsCtx *)malloc(
        sizeof(struct SimpleActionConsCtx));
    FKL_ASSERT(ctx);
    *ctx = *(struct SimpleActionConsCtx *)c;
    return ctx;
}

static void *simple_action_cons(void *c, void *outerCtx, void *nodes[],
                                size_t num, size_t line) {
    struct SimpleActionConsCtx *cc = (struct SimpleActionConsCtx *)c;
    if (cc->car >= num || cc->cdr >= num)
        return NULL;
    FklNastNode *retval = fklCreateNastNode(FKL_NAST_PAIR, line);
    retval->pair = fklCreateNastPair();
    retval->pair->car = fklMakeNastNodeRef(nodes[cc->car]);
    retval->pair->cdr = fklMakeNastNodeRef(nodes[cc->cdr]);
    return retval;
}

struct SimpleActionHeadCtx {
    FklNastNode *head;
    uint64_t idx_num;
    uint64_t idx[];
};

static void *simple_action_head(void *c, void *outerCtx, void *nodes[],
                                size_t num, size_t line) {
    struct SimpleActionHeadCtx *cc = (struct SimpleActionHeadCtx *)c;
    for (size_t i = 0; i < cc->idx_num; i++)
        if (cc->idx[i] >= num)
            return NULL;
    FklNastNode *head = fklMakeNastNodeRef(cc->head);
    FklNastNode **exps =
        (FklNastNode **)malloc((1 + cc->idx_num) * sizeof(FklNastNode *));
    FKL_ASSERT(exps);
    exps[0] = head;
    for (size_t i = 0; i < cc->idx_num; i++)
        exps[i + 1] = fklMakeNastNodeRef(nodes[cc->idx[i]]);
    FklNastNode *retval = create_nast_list(exps, 1 + cc->idx_num, line);
    free(exps);
    return retval;
}

static void *simple_action_head_create(FklNastNode *rest[], size_t rest_len,
                                       int *failed) {
    if (rest_len < 2 || rest[1]->type != FKL_NAST_FIX || rest[1]->fix < 0) {
        *failed = 1;
        return NULL;
    }
    for (size_t i = 1; i < rest_len; i++)
        if (rest[i]->type != FKL_NAST_FIX || rest[i]->fix < 0)
            return NULL;
    struct SimpleActionHeadCtx *ctx = (struct SimpleActionHeadCtx *)malloc(
        sizeof(struct SimpleActionHeadCtx)
        + ((rest_len - 1) * sizeof(uint64_t)));
    FKL_ASSERT(ctx);
    ctx->head = fklMakeNastNodeRef(rest[0]);
    ctx->idx_num = rest_len - 1;
    for (size_t i = 1; i < rest_len; i++)
        ctx->idx[i - 1] = rest[i]->fix;
    return ctx;
}

static void *simple_action_head_copy(const void *c) {
    struct SimpleActionHeadCtx *cc = (struct SimpleActionHeadCtx *)c;
    size_t size =
        sizeof(struct SimpleActionHeadCtx) + (sizeof(uint64_t) * cc->idx_num);
    struct SimpleActionHeadCtx *ctx =
        (struct SimpleActionHeadCtx *)malloc(size);
    FKL_ASSERT(ctx);
    memcpy(ctx, cc, size);
    fklMakeNastNodeRef(ctx->head);
    return ctx;
}

static void simple_action_head_destroy(void *cc) {
    struct SimpleActionHeadCtx *ctx = (struct SimpleActionHeadCtx *)cc;
    fklDestroyNastNode(ctx->head);
    free(ctx);
}

static inline void *simple_action_box(void *ctx, void *outerCtx, void *nodes[],
                                      size_t num, size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *box = fklCreateNastNode(FKL_NAST_BOX, line);
    box->box = fklMakeNastNodeRef(nodes[nth]);
    return box;
}

static inline void *simple_action_vector(void *ctx, void *outerCtx,
                                         void *nodes[], size_t num,
                                         size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *list = nodes[nth];
    FklNastNode *r = fklCreateNastNode(FKL_NAST_VECTOR, line);
    size_t len = fklNastListLength(list);
    FklNastVector *vec = fklCreateNastVector(len);
    r->vec = vec;
    size_t i = 0;
    for (; list->type == FKL_NAST_PAIR; list = list->pair->cdr, i++)
        vec->base[i] = fklMakeNastNodeRef(list->pair->car);
    return r;
}

static inline void *simple_action_hasheq(void *ctx, void *outerCtx,
                                         void *nodes[], size_t num,
                                         size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *list = nodes[nth];
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

static inline void *simple_action_hasheqv(void *ctx, void *outerCtx,
                                          void *nodes[], size_t num,
                                          size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *list = nodes[nth];
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

static inline void *simple_action_hashequal(void *ctx, void *outerCtx,
                                            void *nodes[], size_t num,
                                            size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *list = nodes[nth];
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

static inline void *simple_action_bytevector(void *ctx, void *outerCtx,
                                             void *nodes[], size_t num,
                                             size_t line) {
    uintptr_t nth = (uintptr_t)ctx;
    if (nth >= num)
        return NULL;
    FklNastNode *list = nodes[nth];
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

static void *simple_action_symbol_create(FklNastNode *rest[], size_t rest_len,
                                         int *failed) {
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
    struct SimpleActionSymbolCtx *ctx = (struct SimpleActionSymbolCtx *)malloc(
        sizeof(struct SimpleActionSymbolCtx));
    FKL_ASSERT(ctx);
    ctx->nth = rest[0]->fix;
    ctx->start = start ? fklCopyString(start) : NULL;
    ctx->end = end ? fklCopyString(end) : NULL;
    return ctx;
}

static void *simple_action_symbol_copy(const void *cc) {
    struct SimpleActionSymbolCtx *ctx = (struct SimpleActionSymbolCtx *)malloc(
        sizeof(struct SimpleActionSymbolCtx));
    FKL_ASSERT(ctx);
    *ctx = *(struct SimpleActionSymbolCtx *)cc;
    ctx->end = fklCopyString(ctx->end);
    ctx->start = fklCopyString(ctx->start);
    return ctx;
}

static void simple_action_symbol_destroy(void *cc) {
    struct SimpleActionSymbolCtx *ctx = (struct SimpleActionSymbolCtx *)cc;
    free(ctx->end);
    free(ctx->start);
    free(ctx);
}

static void *simple_action_symbol(void *c, void *outerCtx, void *nodes[],
                                  size_t num, size_t line) {
    struct SimpleActionSymbolCtx *ctx = (struct SimpleActionSymbolCtx *)c;
    if (ctx->nth >= num)
        return NULL;
    FklNastNode *node = nodes[ctx->nth];
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
            if (fklCharBufMatch(start, start_size, cstr, cstr_size)) {
                cstr += start_size;
                cstr_size -= start_size;
                size_t len =
                    fklQuotedCharBufMatch(cstr, cstr_size, end, end_size);
                if (!len)
                    return 0;
                size_t size = 0;
                char *s = fklCastEscapeCharBuf(cstr, len - end_size, &size);
                fklStringBufferBincpy(&buffer, s, size);
                free(s);
                cstr += len;
                cstr_size -= len;
                continue;
            }
            size_t len = 0;
            for (; (cstr + len) < end_cstr; len++)
                if (fklCharBufMatch(start, start_size, cstr + len,
                                    cstr_size - len))
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

static inline void *simple_action_string(void *c, void *outerCtx, void *nodes[],
                                         size_t num, size_t line) {
    struct SimpleActionSymbolCtx *ctx = (struct SimpleActionSymbolCtx *)c;
    if (ctx->nth >= num)
        return NULL;
    FklNastNode *node = nodes[ctx->nth];
    if (node->type != FKL_NAST_STR)
        return NULL;
    if (ctx->start) {
        size_t start_size = ctx->start->size;
        size_t end_size = ctx->end->size;

        const FklString *str = node->str;
        const char *cstr = str->str;

        size_t size = 0;
        char *s = fklCastEscapeCharBuf(
            &cstr[start_size], str->size - end_size - start_size, &size);
        FklNastNode *retval = fklCreateNastNode(FKL_NAST_STR, node->curline);
        retval->str = fklCreateString(size, s);
        free(s);
        return retval;
    } else
        return fklMakeNastNodeRef(node);
}

static struct CstrIdCreatorProdAction {
    const char *name;
    FklProdActionFunc func;
    void *(*creator)(FklNastNode *rest[], size_t rest_len, int *failed);
    void *(*ctx_copyer)(const void *);
    void (*ctx_destroy)(void *);
} CodegenProdCreatorActions[FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM] = {
    // clang-format off
    {"nth",       simple_action_nth,        simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing   },
    {"cons",      simple_action_cons,       simple_action_cons_create,   simple_action_cons_copy,   fklProdCtxDestroyFree        },
    {"head",      simple_action_head,       simple_action_head_create,   simple_action_head_copy,   simple_action_head_destroy   },
    {"symbol",    simple_action_symbol,     simple_action_symbol_create, simple_action_symbol_copy, simple_action_symbol_destroy },
    {"string",    simple_action_string,     simple_action_symbol_create, simple_action_symbol_copy, simple_action_symbol_destroy },
    {"box",       simple_action_box,        simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing   },
    {"vector",    simple_action_vector,     simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing   },
    {"hasheq",    simple_action_hasheq,     simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing   },
    {"hasheqv",   simple_action_hasheqv,    simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing   },
    {"hashequal", simple_action_hashequal,  simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing   },
    {"bytes",     simple_action_bytevector, simple_action_nth_create,    fklProdCtxCopyerDoNothing, fklProdCtxDestroyDoNothing   },
    // clang-format on
};

static inline void init_simple_prod_action_list(FklSid_t *simple_prod_action_id,
                                                FklSymbolTable *pst) {
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++)
        simple_prod_action_id[i] =
            fklAddSymbolCstr(CodegenProdCreatorActions[i].name, pst)->v;
}

static inline struct CstrIdCreatorProdAction *
find_simple_prod_action(FklSid_t id, FklSid_t *simple_prod_action_id) {
    for (size_t i = 0; i < FKL_CODEGEN_SIMPLE_PROD_ACTION_NUM; i++) {
        if (simple_prod_action_id[i] == id)
            return &CodegenProdCreatorActions[i];
    }
    return NULL;
}

static inline FklGrammerSym *nast_vector_to_production_right_part(
    const FklNastVector *vec, FklGraSidBuiltinHashMap *builtin_term,
    FklSymbolTable *tt, FklRegexTable *rt, size_t *num, int *failed) {
    FklGrammerSym *retval = NULL;

    FklNastNodeVector valid_items;
    fklNastNodeVectorInit(&valid_items, vec->size);

    uint8_t *delim;
    if (!vec->size)
        delim = NULL;
    else {
        delim = (uint8_t *)malloc(vec->size * sizeof(uint8_t));
        FKL_ASSERT(delim);
        memset(delim, 1, sizeof(uint8_t) * vec->size);
    }

    for (size_t i = 0; i < vec->size; i++) {
        const FklNastNode *cur = vec->base[i];
        if (cur->type == FKL_NAST_STR || cur->type == FKL_NAST_SYM
            || (cur->type == FKL_NAST_BOX && cur->box->type == FKL_NAST_STR)
            || (cur->type == FKL_NAST_VECTOR && cur->vec->size == 1
                && cur->vec->base[0]->type == FKL_NAST_STR)
            || (cur->type == FKL_NAST_PAIR
                && cur->pair->car->type == FKL_NAST_SYM
                && cur->pair->cdr->type == FKL_NAST_SYM))
            fklNastNodeVectorPushBack2(&valid_items, (FklNastNode *)cur);
        else if (cur->type == FKL_NAST_NIL) {
            delim[valid_items.size] = 0;
            continue;
        } else {
            *failed = 1;
            goto end;
        }
    }
    if (valid_items.size) {
        size_t top = valid_items.size;
        retval = (FklGrammerSym *)malloc(top * sizeof(FklGrammerSym));
        FKL_ASSERT(retval);
        FklNastNode *const *base = (FklNastNode *const *)valid_items.base;
        for (size_t i = 0; i < top; i++) {
            const FklNastNode *cur = base[i];
            FklGrammerSym *ss = &retval[i];
            ss->delim = delim[i];
            ss->end_with_terminal = 0;
            if (cur->type == FKL_NAST_STR) {
                ss->isterm = 1;
                ss->term_type = FKL_TERM_STRING;
                ss->nt.group = 0;
                ss->nt.sid = fklAddSymbol(cur->str, tt)->v;
                ss->end_with_terminal = 0;
            } else if (cur->type == FKL_NAST_PAIR) {
                ss->isterm = 0;
                ss->term_type = FKL_TERM_STRING;
                ss->nt.group = cur->pair->car->sym;
                ss->nt.sid = cur->pair->cdr->sym;
            } else if (cur->type == FKL_NAST_BOX) {
                ss->isterm = 1;
                ss->term_type = FKL_TERM_REGEX;
                const FklRegexCode *re = fklAddRegexStr(rt, cur->box->str);
                if (!re) {
                    *failed = 2;
                    free(retval);
                    retval = NULL;
                    goto end;
                }
                ss->re = re;
            } else if (cur->type == FKL_NAST_VECTOR) {
                ss->isterm = 1;
                ss->term_type = FKL_TERM_STRING;
                ss->nt.group = 0;
                const FklString *str = cur->vec->base[0]->str;
                ss->nt.sid = fklAddSymbol(str, tt)->v;
                ss->end_with_terminal = 1;
            } else {
                if (fklGetBuiltinMatch(builtin_term, cur->sym)) {
                    ss->isterm = 1;
                    ss->term_type = FKL_TERM_BUILTIN;
                    ss->b.t = fklGetBuiltinMatch(builtin_term, cur->sym);
                    ss->b.c = NULL;
                } else {
                    ss->isterm = 0;
                    ss->term_type = FKL_TERM_STRING;
                    ss->nt.group = 0;
                    ss->nt.sid = cur->sym;
                }
            }
        }
    }
    *num = valid_items.size;

end:
    fklNastNodeVectorUninit(&valid_items);
    free(delim);
    return retval;
}

static inline FklGrammerIgnore *
nast_vector_to_ignore(const FklNastVector *vec,
                      FklGraSidBuiltinHashMap *builtin_term, FklSymbolTable *tt,
                      FklRegexTable *rt, int *failed) {
    size_t num = 0;
    FklGrammerSym *syms = nast_vector_to_production_right_part(
        vec, builtin_term, tt, rt, &num, failed);
    if (*failed)
        return NULL;
    FklGrammerIgnore *ig = fklGrammerSymbolsToIgnore(syms, num, tt);
    fklUninitGrammerSymbols(syms, num);
    free(syms);
    return ig;
}

FklGrammerIgnore *fklNastVectorToIgnore(FklNastNode *ast, FklSymbolTable *tt,
                                        FklRegexTable *rt,
                                        FklGraSidBuiltinHashMap *builtin) {
    int failed = 0;
    return nast_vector_to_ignore(ast->vec, builtin, tt, rt, &failed);
}

FklGrammerProduction *fklCodegenProdPrintingToProduction(
    const FklCodegenProdPrinting *p, FklSymbolTable *tt, FklRegexTable *rt,
    FklGraSidBuiltinHashMap *builtin_terms, FklCodegenOuterCtx *outer_ctx,
    FklFuncPrototypes *pts, FklCodegenLibVector *macroLibStack) {
    const FklNastVector *vec = p->vec->vec;
    size_t len = 0;
    int failed = 0;
    FklGrammerSym *syms = nast_vector_to_production_right_part(
        vec, builtin_terms, tt, rt, &len, &failed);
    FklGrammerProduction *prod = fklCreateProduction(
        p->sid == 0 ? 0 : p->group_id, p->sid, len, syms, NULL, NULL, NULL,
        fklProdCtxDestroyDoNothing, fklProdCtxCopyerDoNothing);

    if (p->type == FKL_CODEGEN_PROD_BUILTIN) {
        FklProdActionFunc action = find_builtin_prod_action(
            p->forth->sym, outer_ctx->builtin_prod_action_id);
        prod->func = action;
    } else if (p->type == FKL_CODEGEN_PROD_SIMPLE) {
        struct CstrIdCreatorProdAction *action = find_simple_prod_action(
            p->forth->vec->base[0]->sym, outer_ctx->simple_prod_action_id);
        void *ctx = action->creator(&p->forth->vec->base[1],
                                    p->forth->vec->size - 1, &failed);
        prod->ctx = ctx;
        prod->func = action->func;
        prod->ctx_destroyer = action->ctx_destroy;
        prod->ctx_copyer = action->ctx_copyer;
    } else {
        struct CustomActionCtx *ctx =
            (struct CustomActionCtx *)calloc(1, sizeof(struct CustomActionCtx));
        FKL_ASSERT(ctx);
        ctx->codegen_outer_ctx = outer_ctx;
        ctx->prototype_id = p->prototype_id;
        ctx->pst = &outer_ctx->public_symbol_table;
        ctx->macroLibStack = macroLibStack;
        ctx->bcl = fklCopyByteCodelnt(p->bcl);
        ctx->pts = pts;
        prod->func = custom_action;
        prod->ctx = ctx;
        prod->ctx_destroyer = custom_action_ctx_destroy;
        prod->ctx_copyer = custom_action_ctx_copy;
    }

    return prod;
}

static inline FklCodegenInfo *
macro_compile_prepare(FklCodegenInfo *codegen, FklCodegenMacroScope *macroScope,
                      FklSidHashSet *symbolSet, FklCodegenEnv **pmacroEnv,
                      FklSymbolTable *pst, FklConstTable *pkt) {
    FklCodegenEnv *macro_glob_env = fklCreateCodegenEnv(NULL, 0, macroScope);
    fklInitGlobCodegenEnv(macro_glob_env, pst);

    FklCodegenInfo *macroCodegen =
        createCodegenInfo(codegen, NULL, 0, 1, codegen->outer_ctx);
    codegen->global_env->refcount--;
    macroCodegen->global_env = macro_glob_env;
    macro_glob_env->refcount++;

    free(macroCodegen->dir);
    macroCodegen->dir = fklCopyCstr(codegen->dir);
    macroCodegen->filename = fklCopyCstr(codegen->filename);
    macroCodegen->realpath = fklCopyCstr(codegen->realpath);
    macroCodegen->pts = codegen->macro_pts;
    macroCodegen->macro_pts = fklCreateFuncPrototypes(0);

    macroCodegen->runtime_symbol_table = pst;
    macroCodegen->runtime_kt = pkt;
    macroCodegen->fid = macroCodegen->filename
                          ? fklAddSymbolCstr(macroCodegen->filename, pst)->v
                          : 0;
    macroCodegen->libStack = macroCodegen->macroLibStack;
    macroCodegen->macroLibStack = fklCodegenLibVectorCreate(8);

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
    free(data);
}

static const FklCodegenQuestContextMethodTable AddingProductionCtxMethodTable =
    {
        .__finalizer = _adding_production_ctx_finalizer,
        .__get_bcl_stack = NULL,
        .__put_bcl = NULL,
};

static inline FklCodegenQuestContext *
createAddingProductionCtx(FklGrammerProduction *prod, FklSid_t sid,
                          FklSid_t group_id, uint8_t add_extra,
                          FklNastNode *vec, FklCodegenProdActionType type,
                          FklNastNode *action_ast) {
    AddingProductionCtx *ctx =
        (AddingProductionCtx *)malloc(sizeof(AddingProductionCtx));
    FKL_ASSERT(ctx);
    ctx->prod = prod;
    ctx->sid = sid;
    ctx->group_id = group_id;
    ctx->add_extra = add_extra;
    ctx->vec = fklMakeNastNodeRef(vec);
    ctx->type = type;
    if (type != FKL_CODEGEN_PROD_CUSTOM)
        ctx->action = fklMakeNastNodeRef(action_ast);
    return createCodegenQuestContext(ctx, &AddingProductionCtxMethodTable);
}

static inline int check_group_outer_ref(FklCodegenInfo *codegen,
                                        FklProdHashMap *productions,
                                        FklSid_t group_id) {
    int is_ref_outer = 0;
    for (FklProdHashMapNode *item = productions->first; item;
         item = item->next) {
        for (FklGrammerProduction *prods = item->v; prods;
             prods = prods->next) {
            for (size_t i = 0; i < prods->len; i++) {
                FklGrammerSym *sym = &prods->syms[i];
                if (!sym->isterm) {
                    if (sym->nt.group)
                        is_ref_outer |= sym->nt.group != group_id;
                    else {
                        FklGrammerNonterm left = {.group = group_id,
                                                  .sid = sym->nt.sid};
                        if (fklProdHashMapGet(productions, &left))
                            sym->nt.group = group_id;
                        else if (fklProdHashMapGet(codegen->unnamed_prods,
                                                   &left))
                            is_ref_outer = 1;
                    }
                }
            }
        }
    }
    return is_ref_outer;
}

FklGrammerProduction *fklCreateExtraStartProduction(FklSid_t group,
                                                    FklSid_t sid) {
    FklGrammerProduction *prod = fklCreateEmptyProduction(
        0, 0, 1, NULL, NULL, NULL, fklProdCtxDestroyDoNothing,
        fklProdCtxCopyerDoNothing);
    prod->func = codegen_prod_action_first;
    prod->idx = 0;
    FklGrammerSym *u = &prod->syms[0];
    u->delim = 1;
    u->term_type = FKL_TERM_STRING;
    u->isterm = 0;
    u->nt.group = group;
    u->nt.sid = sid;
    return prod;
}

static inline void
init_codegen_prod_printing(FklCodegenProdPrinting *p, FklSid_t group,
                           FklSid_t sid, FklNastNode *vec, uint8_t type,
                           uint8_t add_extra, FklNastNode *forth,
                           uint32_t prototype_id, FklByteCodelnt *bcl) {
    p->group_id = group;
    p->sid = sid;
    p->vec = fklMakeNastNodeRef(vec);
    p->type = type;
    p->add_extra = add_extra;
    if (p->type != FKL_CODEGEN_PROD_CUSTOM)
        p->forth = fklMakeNastNodeRef(forth);
    else {
        p->prototype_id = prototype_id;
        p->bcl = fklCopyByteCodelnt(bcl);
    }
}

BC_PROCESS(process_adding_production) {
    AddingProductionCtx *ctx = context->data;
    FklSid_t group_id = ctx->group_id;
    FklGrammerProduction *prod = ctx->prod;
    ctx->prod = NULL;
    FklGrammer *g = *(codegen->g);
    FklSid_t sid = ctx->sid;
    if (group_id == 0) {
        if (fklAddProdToProdTableNoRepeat(codegen->unnamed_prods, &g->builtins,
                                          prod)) {
            fklDestroyGrammerProduction(prod);
        reader_macro_error:
            errorState->type = FKL_ERR_GRAMMER_CREATE_FAILED;
            errorState->line = line;
            return NULL;
        }
        if (ctx->add_extra) {
            FklGrammerProduction *extra_prod =
                fklCreateExtraStartProduction(group_id, sid);
            if (fklAddProdToProdTable(codegen->unnamed_prods, &g->builtins,
                                      extra_prod)) {
                fklDestroyGrammerProduction(extra_prod);
                goto reader_macro_error;
            }
        }
    } else {
        FklGrammerProdGroupItem *group =
            add_production_group(codegen->named_prod_groups, group_id);
        if (fklAddProdToProdTableNoRepeat(&group->prods, &g->builtins, prod)) {
            fklDestroyGrammerProduction(prod);
            goto reader_macro_error;
        }
        if (ctx->add_extra) {
            FklGrammerProduction *extra_prod =
                fklCreateExtraStartProduction(group_id, sid);
            if (fklAddProdToProdTable(&group->prods, &g->builtins,
                                      extra_prod)) {
                fklDestroyGrammerProduction(extra_prod);
                goto reader_macro_error;
            }
        }
        group->is_ref_outer =
            check_group_outer_ref(codegen, &group->prods, group_id);
        uint32_t prototype_id = 0;
        FklByteCodelnt *bcl = NULL;
        if (ctx->type == FKL_CODEGEN_PROD_CUSTOM) {
            struct CustomActionCtx *ctx = prod->ctx;
            prototype_id = ctx->prototype_id;
            bcl = ctx->bcl;
        }
        init_codegen_prod_printing(
            fklProdPrintingVectorPushBack(&group->prod_printing, NULL),
            group_id, sid, ctx->vec, ctx->type, ctx->add_extra, ctx->action,
            prototype_id, bcl);
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

static inline FklGrammerProduction *nast_vector_to_production(
    FklNastNode *vec_node, uint32_t line, uint8_t add_extra,
    FklGraSidBuiltinHashMap *builtin_term, FklSymbolTable *tt,
    FklRegexTable *rt, FklSid_t left_group, FklSid_t sid, FklSid_t action_type,
    FklNastNode *action_ast, FklSid_t group_id, FklCodegenInfo *codegen,
    FklCodegenEnv *curEnv, FklCodegenMacroScope *macroScope,
    FklCodegenQuestVector *codegenQuestStack, FklSymbolTable *pst,
    int *failed) {
    size_t len = 0;
    const FklNastVector *vec = vec_node->vec;
    FklGrammerSym *syms = nast_vector_to_production_right_part(
        vec, builtin_term, tt, rt, &len, failed);
    if (*failed)
        return NULL;
    if (action_type == codegen->outer_ctx->builtInPatternVar_builtin) {
        if (action_ast->type != FKL_NAST_SYM)
            return NULL;
        FklProdActionFunc action = find_builtin_prod_action(
            action_ast->sym, codegen->outer_ctx->builtin_prod_action_id);
        if (!action)
            return NULL;
        FklGrammerProduction *prod = fklCreateProduction(
            left_group, sid, len, syms, NULL, action, NULL,
            fklProdCtxDestroyDoNothing, fklProdCtxCopyerDoNothing);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            process_adding_production,
            createAddingProductionCtx(prod, sid, group_id, add_extra, vec_node,
                                      FKL_CODEGEN_PROD_BUILTIN, action_ast),
            NULL, 1, macroScope, curEnv, line, codegen, codegenQuestStack);
        return prod;
    } else if (action_type == codegen->outer_ctx->builtInPatternVar_simple) {
        if (action_ast->type != FKL_NAST_VECTOR || !action_ast->vec->size
            || action_ast->vec->base[0]->type != FKL_NAST_SYM)
            return NULL;
        struct CstrIdCreatorProdAction *action =
            find_simple_prod_action(action_ast->vec->base[0]->sym,
                                    codegen->outer_ctx->simple_prod_action_id);
        if (!action)
            return NULL;
        int failed = 0;
        void *ctx = action->creator(&action_ast->vec->base[1],
                                    action_ast->vec->size - 1, &failed);
        if (failed)
            return NULL;
        FklGrammerProduction *prod =
            fklCreateProduction(left_group, sid, len, syms, NULL, action->func,
                                ctx, action->ctx_destroy, action->ctx_copyer);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            process_adding_production,
            createAddingProductionCtx(prod, sid, group_id, add_extra, vec_node,
                                      FKL_CODEGEN_PROD_SIMPLE, action_ast),
            NULL, 1, macroScope, curEnv, line, codegen, codegenQuestStack);
        return prod;
    } else if (action_type == codegen->outer_ctx->builtInPatternVar_custom) {
        FklSidHashSet symbolSet;
        fklSidHashSetInit(&symbolSet);
        fklSidHashSetPut2(&symbolSet, fklAddSymbolCstr("$$", pst)->v);
        FklCodegenEnv *macroEnv = NULL;
        FklCodegenInfo *macroCodegen =
            macro_compile_prepare(codegen, macroScope, &symbolSet, &macroEnv,
                                  pst, &codegen->outer_ctx->public_kt);
        fklSidHashSetUninit(&symbolSet);

        create_and_insert_to_pool(macroCodegen, 0, macroEnv, 0,
                                  action_ast->curline, pst);
        FklNastNodeQueue *queue = fklNastNodeQueueCreate();
        struct CustomActionCtx *ctx =
            (struct CustomActionCtx *)calloc(1, sizeof(struct CustomActionCtx));
        FKL_ASSERT(ctx);
        ctx->codegen_outer_ctx = codegen->outer_ctx;
        ctx->prototype_id = macroEnv->prototypeId;
        ctx->pst = pst;
        ctx->macroLibStack = codegen->macroLibStack;

        FklGrammerProduction *prod = fklCreateProduction(
            left_group, sid, len, syms, NULL, custom_action, ctx,
            custom_action_ctx_destroy, custom_action_ctx_copy);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            process_adding_production,
            createAddingProductionCtx(prod, sid, group_id, add_extra, vec_node,
                                      FKL_CODEGEN_PROD_CUSTOM, NULL),
            NULL, 1, macroScope, curEnv, line, codegen, codegenQuestStack);
        fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(action_ast));
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            _reader_macro_bc_process,
            createReaderMacroQuestContext(ctx, macroCodegen->pts),
            createMustHasRetvalQueueNextExpression(queue), 1, macroEnv->macros,
            macroEnv, action_ast->curline, macroCodegen, codegenQuestStack);
        return prod;
    }
    return NULL;
}

static inline int add_group_prods(FklGrammer *g, const FklProdHashMap *prods) {
    for (FklProdHashMapNode *i = prods->first; i; i = i->next) {
        for (FklGrammerProduction *prods = i->v; prods; prods = prods->next) {
            FklGrammerProduction *prod =
                fklCopyUninitedGrammerProduction(prods);
            if (prod->left.sid == 0 && prod->left.group == 0)
                prod->left = g->start;
            if (fklAddProdAndExtraToGrammer(g, prod)) {
                fklDestroyGrammerProduction(prod);
                return 1;
            }
        }
    }
    return 0;
}

static inline int add_group_ignores(FklGrammer *g, FklGrammerIgnore *igs) {
    for (; igs; igs = igs->next) {
        FklGrammerIgnore *ig = (FklGrammerIgnore *)malloc(
            sizeof(FklGrammerIgnore) + igs->len * sizeof(FklGrammerIgnoreSym));
        FKL_ASSERT(ig);
        ig->len = igs->len;
        ig->next = NULL;
        memcpy(ig->ig, igs->ig, sizeof(FklGrammerIgnoreSym) * ig->len);

        if (fklAddIgnoreToIgnoreList(&g->ignores, ig)) {
            free(ig);
            return 1;
        }
    }
    return 0;
}

static inline int add_start_prods_to_grammer(FklGrammer *g,
                                             FklProdHashMap *prods) {
    for (FklProdHashMapNode *itemlist = prods->first; itemlist;
         itemlist = itemlist->next) {
        for (FklGrammerProduction *prods = itemlist->v; prods;
             prods = prods->next) {
            if (prods->left.group == 0 && prods->left.sid == 0) {
                FklGrammerProduction *prod =
                    fklCopyUninitedGrammerProduction(prods);
                prod->left = g->start;
                if (fklAddProdAndExtraToGrammer(g, prod)) {
                    fklDestroyGrammerProduction(prod);
                    return 1;
                }
            }
        }
    }
    return 0;
}

static inline int add_all_ignores_to_grammer(FklCodegenInfo *codegen) {
    FklGrammer *g = *codegen->g;

    if (add_group_ignores(g, *codegen->unnamed_ignores))
        return 1;

    for (FklGraProdGroupHashMapNode *group_items =
             codegen->named_prod_groups->first;
         group_items; group_items = group_items->next) {
        if (add_group_ignores(g, group_items->v.ignore))
            return 1;
    }
    return 0;
}

static inline int add_all_start_prods_to_grammer(FklCodegenInfo *codegen) {
    FklGrammer *g = *codegen->g;
    if (add_start_prods_to_grammer(g, codegen->unnamed_prods))
        return 1;

    for (FklGraProdGroupHashMapNode *group_items =
             codegen->named_prod_groups->first;
         group_items; group_items = group_items->next) {
        if (add_start_prods_to_grammer(g, &group_items->v.prods))
            return 1;
    }

    return 0;
}

static inline int add_prods_list_to_grammer(FklGrammer *g,
                                            FklGrammerProduction *prods) {
    for (; prods; prods = prods->next) {
        FklGrammerProduction *prod = fklCopyUninitedGrammerProduction(prods);
        if (fklAddProdAndExtraToGrammer(g, prod)) {
            fklDestroyGrammerProduction(prod);
            return 1;
        }
    }
    return 0;
}

static inline int scan_and_add_reachable_productions(FklCodegenInfo *codegen) {
    FklGrammer *g = *codegen->g;
    FklProdHashMap *productions = &g->productions;
    for (FklProdHashMapNode *nonterm_item = productions->first; nonterm_item;
         nonterm_item = nonterm_item->next) {
        for (FklGrammerProduction *prods = nonterm_item->v; prods;
             prods = prods->next) {
            size_t len = prods->len;
            FklGrammerSym *syms = prods->syms;
            for (size_t i = 0; i < len; i++) {
                FklGrammerSym *cur = &syms[i];
                if (!cur->isterm
                    && !fklIsNonterminalExist(&g->productions, cur->nt.group,
                                              cur->nt.sid)) {
                    if (cur->nt.group) {
                        FklGrammerProdGroupItem *group =
                            fklGraProdGroupHashMapGet2(
                                codegen->named_prod_groups, cur->nt.group);
                        if (!group)
                            return 1;
                        FklGrammerProduction *prods = fklGetProductions(
                            &group->prods, cur->nt.group, cur->nt.sid);
                        if (!prods)
                            return 1;
                        if (add_prods_list_to_grammer(g, prods))
                            return 1;
                    } else {
                        FklGrammerProduction *prods = fklGetProductions(
                            codegen->unnamed_prods, cur->nt.group, cur->nt.sid);
                        if (!prods)
                            return 1;
                        if (add_prods_list_to_grammer(g, prods))
                            return 1;
                    }
                }
            }
        }
    }
    return 0;
}

static inline int add_all_group_to_grammer(FklCodegenInfo *codegen) {
    if (add_all_ignores_to_grammer(codegen))
        return 1;

    if (add_all_start_prods_to_grammer(codegen))
        return 1;

    if (scan_and_add_reachable_productions(codegen))
        return 1;

    FklGrammer *g = *codegen->g;

    if (fklCheckAndInitGrammerSymbols(g))
        return 1;

    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fklLr0ToLalrItems(itemSet, g);
    int r = fklGenerateLalrAnalyzeTable(g, itemSet);
    fklLalrItemSetHashMapDestroy(itemSet);

    return r;
}

static inline void destroy_grammer_ignore_list(FklGrammerIgnore *ig) {
    while (ig) {
        FklGrammerIgnore *next = ig->next;
        fklDestroyIgnore(ig);
        ig = next;
    }
}

static inline FklGrammerProdGroupItem *
get_production_group(const FklGraProdGroupHashMap *ht, FklSid_t group) {
    return fklGraProdGroupHashMapGet2(ht, group);
}

static inline FklGrammerProdGroupItem *
add_production_group(FklGraProdGroupHashMap *named_prod_groups,
                     FklSid_t group_id) {
    FklGrammerProdGroupItem *group =
        fklGraProdGroupHashMapAdd1(named_prod_groups, group_id);
    if (!group->prods.buckets) {
        fklProdHashMapInit(&group->prods);
        fklProdPrintingVectorInit(&group->prod_printing, 8);
        fklNastNodeVectorInit(&group->ignore_printing, 8);
    }
    return group;
}

static inline int init_builtin_grammer(FklCodegenInfo *codegen,
                                       FklSymbolTable *st) {
    *codegen->g = fklCreateEmptyGrammer(st);
    FklGrammer *g = *codegen->g;
    fklProdHashMapInit(codegen->builtin_prods);
    fklProdHashMapInit(codegen->unnamed_prods);
    fklGraProdGroupHashMapInit(codegen->named_prod_groups);
    codegen->self_unnamed_ignores = NULL;
    *codegen->unnamed_ignores = NULL;
    *codegen->builtin_ignores = fklInitBuiltinProductionSet(
        codegen->builtin_prods, st, &g->terminals, &g->regexes, &g->builtins);

    g->ignores = NULL;

    fklClearGrammer(g);
    if (add_group_ignores(g, *codegen->builtin_ignores))
        return 1;

    if (add_group_prods(g, codegen->builtin_prods))
        return 1;
    return 0;
}

static inline int process_add_production(
    FklSid_t group_id, FklCodegenInfo *codegen, FklNastNode *vector_node,
    FklCodegenErrorState *errorState, FklCodegenEnv *curEnv,
    FklCodegenMacroScope *macroScope, FklCodegenQuestVector *codegenQuestStack,
    FklSymbolTable *pst) {
    FklGrammer *g = *codegen->g;
    FklNastVector *vec = vector_node->vec;
    if (vec->size == 1) {
        if (vec->base[0]->type != FKL_NAST_VECTOR
            || vec->base[0]->vec->size == 0)
            goto reader_macro_error;
        int failed = 0;
        FklGrammerIgnore *ignore =
            nast_vector_to_ignore(vec->base[0]->vec, &g->builtins,
                                  &g->terminals, &g->regexes, &failed);
        if (!ignore) {
            if (failed == 2) {
            regex_compile_error:
                errorState->type = FKL_ERR_REGEX_COMPILE_FAILED;
                errorState->place = fklMakeNastNodeRef(vector_node);
                return 1;
            }
        reader_macro_error:
            errorState->type = FKL_ERR_SYNTAXERROR;
            errorState->place = fklMakeNastNodeRef(vector_node);
            return 1;
        }
        if (group_id == 0) {
            if (fklAddIgnoreToIgnoreList(codegen->unnamed_ignores, ignore)) {
                fklDestroyIgnore(ignore);
                goto reader_macro_error;
            }
        } else {
            FklGrammerProdGroupItem *group =
                add_production_group(codegen->named_prod_groups, group_id);
            if (fklAddIgnoreToIgnoreList(&group->ignore, ignore)) {
                fklDestroyIgnore(ignore);
                goto reader_macro_error;
            }
            fklNastNodeVectorPushBack2(&group->ignore_printing,
                                       fklMakeNastNodeRef(vec->base[0]));
        }
    } else if (vec->size == 4) {
        FklNastNode **base = vec->base;
        if (base[2]->type != FKL_NAST_SYM)
            goto reader_macro_error;
        if (base[0]->type == FKL_NAST_NIL && base[1]->type == FKL_NAST_VECTOR) {
            int failed = 0;
            FklGrammerProduction *prod = nast_vector_to_production(
                base[1], vector_node->curline, 0, &g->builtins, &g->terminals,
                &g->regexes, 0, 0, base[2]->sym, base[3], group_id, codegen,
                curEnv, macroScope, codegenQuestStack, pst, &failed);
            if (failed == 2)
                goto regex_compile_error;
            if (!prod)
                goto reader_macro_error;
        } else if (base[0]->type == FKL_NAST_SYM
                   && base[1]->type == FKL_NAST_VECTOR) {
            FklNastNode *vect = base[1];
            FklSid_t sid = base[0]->sym;
            if (group_id == 0
                && (fklGraSidBuiltinHashMapGet2(&g->builtins, sid)
                    || fklIsNonterminalExist(codegen->builtin_prods, group_id,
                                             sid)))
                goto reader_macro_error;
            int failed = 0;
            FklGrammerProduction *prod = nast_vector_to_production(
                vect, vector_node->curline, 1, &g->builtins, &g->terminals,
                &g->regexes, group_id, sid, base[2]->sym, base[3], group_id,
                codegen, curEnv, macroScope, codegenQuestStack, pst, &failed);
            if (failed == 2)
                goto regex_compile_error;
            if (!prod)
                goto reader_macro_error;
        } else if (base[0]->type == FKL_NAST_VECTOR
                   && base[1]->type == FKL_NAST_SYM) {
            FklNastNode *vect = base[0];
            FklSid_t sid = base[1]->sym;
            if (group_id == 0
                && (fklGraSidBuiltinHashMapGet2(&g->builtins, sid)
                    || fklIsNonterminalExist(codegen->builtin_prods, group_id,
                                             sid)))
                goto reader_macro_error;
            int failed = 0;
            FklGrammerProduction *prod = nast_vector_to_production(
                vect, vector_node->curline, 0, &g->builtins, &g->terminals,
                &g->regexes, group_id, sid, base[2]->sym, base[3], group_id,
                codegen, curEnv, macroScope, codegenQuestStack, pst, &failed);
            if (failed == 2)
                goto regex_compile_error;
            if (!prod)
                goto reader_macro_error;
        } else
            goto reader_macro_error;
    } else
        goto reader_macro_error;
    return 0;
}

BC_PROCESS(update_grammer) {
    if (add_all_group_to_grammer(codegen)) {
        errorState->type = FKL_ERR_GRAMMER_CREATE_FAILED;
        errorState->line = line;
        return NULL;
    }
    return NULL;
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
        FklCodegenInfo *macroCodegen =
            macro_compile_prepare(codegen, macroScope, symbolSet, &macroEnv,
                                  pst, &codegen->outer_ctx->public_kt);
        fklSidHashSetDestroy(symbolSet);

        create_and_insert_to_pool(macroCodegen, 0, macroEnv, 0, value->curline,
                                  pst);
        FklNastNodeQueue *queue = fklNastNodeQueueCreate();
        fklNastNodeQueuePush2(queue, fklMakeNastNodeRef(value));
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            _compiler_macro_bc_process,
            createMacroQuestContext(name, pattern, macroScope,
                                    macroEnv->prototypeId),
            createMustHasRetvalQueueNextExpression(queue), 1, macroEnv->macros,
            macroEnv, value->curline, macroCodegen, codegenQuestStack);
    } else if (name->type == FKL_NAST_BOX) {
        FklNastNode *group_node = name->box;
        if (value->type != FKL_NAST_VECTOR
            || (group_node->type != FKL_NAST_SYM
                && group_node->type != FKL_NAST_NIL)) {
            errorState->type = FKL_ERR_SYNTAXERROR;
            errorState->place = fklMakeNastNodeRef(origExp);
            return;
        }
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
            update_grammer, createEmptyContext(), NULL, scope, macroScope,
            curEnv, origExp->curline, codegen, codegenQuestStack);
        FklSid_t group_id =
            group_node->type == FKL_NAST_NIL ? 0 : group_node->sym;
        FklSymbolTable *st = pst;
        FklGrammer *g = *codegen->g;
        if (!g) {
            if (init_builtin_grammer(codegen, st))
                goto reader_macro_error;
            g = *codegen->g;
        } else {
            fklClearGrammer(g);
            if (add_group_ignores(g, *codegen->builtin_ignores))
                goto reader_macro_error;

            if (add_group_prods(g, codegen->builtin_prods))
                goto reader_macro_error;
        }
        if (process_add_production(group_id, codegen, value, errorState, curEnv,
                                   macroScope, codegenQuestStack, pst))
            return;
    } else {
    reader_macro_error:
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
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
    if (name->type != FKL_NAST_BOX)
        goto reader_macro_error;
    FklNastNode *arg =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_arg1);
    if (arg->type != FKL_NAST_VECTOR)
        goto reader_macro_error;
    fklNastNodeQueuePush2(&prod_vector_queue, arg);
    FklNastNode *rest =
        *fklPmatchHashMapGet2(ht, outer_ctx->builtInPatternVar_rest);
    for (; rest->type != FKL_NAST_NIL; rest = rest->pair->cdr) {
        if (rest->pair->car->type != FKL_NAST_VECTOR)
            goto reader_macro_error;
        fklNastNodeQueuePush2(&prod_vector_queue, rest->pair->car);
    }
    FklNastNode *group_node = name->box;
    if (group_node->type != FKL_NAST_NIL && group_node->type != FKL_NAST_SYM) {
    reader_macro_error:
        fklNastNodeQueueUninit(&prod_vector_queue);
        errorState->type = FKL_ERR_SYNTAXERROR;
        errorState->place = fklMakeNastNodeRef(origExp);
        return;
    }
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_QUEST(
        update_grammer, createEmptyContext(), NULL, scope, macroScope, curEnv,
        origExp->curline, codegen, codegenQuestStack);

    FklSid_t group_id = group_node->type == FKL_NAST_SYM ? group_node->sym : 0;
    FklSymbolTable *st = pst;
    FklGrammer *g = *codegen->g;
    if (!g) {
        if (init_builtin_grammer(codegen, st))
            goto reader_macro_error;
        g = *codegen->g;
    } else {
        fklClearGrammer(g);
        if (add_group_ignores(g, *codegen->builtin_ignores))
            goto reader_macro_error;

        if (add_group_prods(g, codegen->builtin_prods))
            goto reader_macro_error;
    }
    for (FklNastNodeQueueNode *first = prod_vector_queue.head; first;
         first = first->next)
        if (process_add_production(group_id, codegen, first->data, errorState,
                                   curEnv, macroScope, codegenQuestStack,
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
    FklNastNodeVector stack;
    fklNastNodeVectorInit(&stack, 32);
    fklNastNodeVectorPushBack2(&stack, FKL_REMOVE_CONST(FklNastNode, node));
    FklByteCode *retval = fklCreateByteCode(0);
    FklSymbolTable *pst = &info->outer_ctx->public_symbol_table;
    while (!fklNastNodeVectorIsEmpty(&stack)) {
        FklNastNode *node = *fklNastNodeVectorPopBack(&stack);
        switch (node->type) {
        case FKL_NAST_SYM:
            append_push_sym_ins_to_bc(
                INS_APPEND_FRONT, retval,
                fklAddSymbol(fklGetSymbolWithId(node->sym, pst)->k,
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
            append_push_i64_ins_to_bc(INS_APPEND_FRONT, retval, node->fix,
                                      info);
            break;
        case FKL_NAST_F64:
            append_push_f64_ins_to_bc(INS_APPEND_FRONT, retval, node->f64,
                                      info);
            break;
        case FKL_NAST_BYTEVECTOR:
            append_push_bvec_ins_to_bc(INS_APPEND_FRONT, retval, node->bvec,
                                       info);
            break;
        case FKL_NAST_STR:
            append_push_str_ins_to_bc(INS_APPEND_FRONT, retval, node->str,
                                      info);
            break;
        case FKL_NAST_BIGINT:
            append_push_bigint_ins_to_bc(INS_APPEND_FRONT, retval, node->bigInt,
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
            else
                append_push_list_ins_to_bc(INS_APPEND_FRONT, retval, i);
        } break;
        case FKL_NAST_BOX:
            if (node->box->type == FKL_NAST_NIL) {
                fklByteCodeInsertFront(
                    (FklInstruction){.op = FKL_OP_BOX,
                                     .ai = FKL_SUBOP_BOX_NEW_NIL_BOX},
                    retval);
            } else {
                fklByteCodeInsertFront(
                    (FklInstruction){.op = FKL_OP_BOX,
                                     .ai = FKL_SUBOP_BOX_NEW_BOX},
                    retval);
                fklNastNodeVectorPushBack2(&stack, node->box);
            }
            break;
        case FKL_NAST_VECTOR:
            append_push_vec_ins_to_bc(INS_APPEND_FRONT, retval,
                                      node->vec->size);
            for (size_t i = 0; i < node->vec->size; i++)
                fklNastNodeVectorPushBack2(&stack, node->vec->base[i]);
            break;
        case FKL_NAST_HASHTABLE:
            append_push_hash_ins_to_bc(INS_APPEND_FRONT, retval,
                                       node->hash->type, node->hash->num);
            for (size_t i = 0; i < node->hash->num; i++) {
                fklNastNodeVectorPushBack2(&stack, node->hash->items[i].car);
                fklNastNodeVectorPushBack2(&stack, node->hash->items[i].cdr);
            }
            break;
        default:
            abort();
            break;
        }
    }
    fklNastNodeVectorUninit(&stack);
    return retval;
}

static inline int matchAndCall(FklCodegenFunc func, const FklNastNode *pattern,
                               uint32_t scope, FklCodegenMacroScope *macroScope,
                               FklNastNode *exp,
                               FklCodegenQuestVector *codegenQuestStack,
                               FklCodegenEnv *env, FklCodegenInfo *codegenr,
                               FklCodegenErrorState *errorState,
                               uint8_t must_has_retval) {
    FklPmatchHashMap ht;
    fklPmatchHashMapInit(&ht);
    int r = fklPatternMatch(pattern, exp, &ht);
    if (r) {
        fklChdir(codegenr->dir);
        func(exp, &ht, codegenQuestStack, scope, macroScope, env, codegenr,
             codegenr->outer_ctx, errorState, must_has_retval);
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
}

void fklInitCodegenOuterCtx(FklCodegenOuterCtx *outerCtx,
                            char *main_file_real_path_dir) {
    outerCtx->cwd = fklSysgetcwd();
    outerCtx->main_file_real_path_dir = main_file_real_path_dir
                                          ? main_file_real_path_dir
                                          : fklCopyCstr(outerCtx->cwd);
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
        FklNastNode *node = fklCreateNastNodeFromCstr(
            builtin_pattern_cstr_func[i].ps, publicSymbolTable);
        builtin_pattern_node[i] = fklCreatePatternFromNast(node, NULL);
        fklDestroyNastNode(node);
    }

    for (size_t i = 0; i < FKL_BUILTIN_REPLACEMENT_NUM; i++)
        outerCtx->builtin_replacement_id[i] =
            fklAddSymbolCstr(builtInSymbolReplacement[i].s, publicSymbolTable)
                ->v;

    FklNastNode **builtin_sub_pattern_node = outerCtx->builtin_sub_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_SUB_PATTERN_NUM; i++) {
        FklNastNode *node =
            fklCreateNastNodeFromCstr(builtInSubPattern[i], publicSymbolTable);
        builtin_sub_pattern_node[i] = fklCreatePatternFromNast(node, NULL);
        fklDestroyNastNode(node);
    }
}

void fklSetCodegenOuterCtxMainFileRealPathDir(FklCodegenOuterCtx *outer_ctx,
                                              char *dir) {
    if (outer_ctx->main_file_real_path_dir)
        free(outer_ctx->main_file_real_path_dir);
    outer_ctx->main_file_real_path_dir = dir;
}

void fklUninitCodegenOuterCtx(FklCodegenOuterCtx *outer_ctx) {
    fklUninitSymbolTable(&outer_ctx->public_symbol_table);
    fklUninitConstTable(&outer_ctx->public_kt);
    FklNastNode **nodes = outer_ctx->builtin_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++)
        fklDestroyNastNode(nodes[i]);
    nodes = outer_ctx->builtin_sub_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_SUB_PATTERN_NUM; i++)
        fklDestroyNastNode(nodes[i]);
    free(outer_ctx->cwd);
    free(outer_ctx->main_file_real_path_dir);
}

void fklDestroyCodegenInfo(FklCodegenInfo *codegen) {
    while (codegen && codegen->is_destroyable) {
        codegen->refcount -= 1;
        FklCodegenInfo *prev = codegen->prev;
        if (!codegen->refcount) {
            fklUninitCodegenInfo(codegen);
            free(codegen);
            codegen = prev;
        } else
            break;
    }
}

static inline int
mapAllBuiltInPattern(FklNastNode *curExp,
                     FklCodegenQuestVector *codegenQuestStack, uint32_t scope,
                     FklCodegenMacroScope *macroScope, FklCodegenEnv *curEnv,
                     FklCodegenInfo *codegenr, FklCodegenErrorState *errorState,
                     uint8_t must_has_retval) {
    FklNastNode *const *builtin_pattern_node =
        codegenr->outer_ctx->builtin_pattern_node;

    if (fklIsNastNodeList(curExp))
        for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++)
            if (matchAndCall(builtin_pattern_cstr_func[i].func,
                             builtin_pattern_node[i], scope, macroScope, curExp,
                             codegenQuestStack, curEnv, codegenr, errorState,
                             must_has_retval))
                return 0;

    if (curExp->type == FKL_NAST_PAIR) {
        codegen_funcall(curExp, codegenQuestStack, scope, macroScope, curEnv,
                        codegenr, errorState);
        return 0;
    }
    return 1;
}

static inline FklByteCodelnt *
append_get_loc_ins(InsAppendMode m, FklByteCodelnt *bcl, uint32_t idx,
                   FklSid_t fid, uint32_t line, uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m, bcl, FKL_OP_GET_LOC, idx,
                                                fid, line, scope);
}

static inline FklByteCodelnt *
append_get_var_ref_ins(InsAppendMode m, FklByteCodelnt *bcl, uint32_t idx,
                       FklSid_t fid, uint32_t curline, uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(m, bcl, FKL_OP_GET_VAR_REF, idx,
                                                fid, curline, scope);
}

FklByteCodelnt *fklGenExpressionCodeWithQuest(FklCodegenQuest *initialQuest,
                                              FklCodegenInfo *codegener) {
    FklCodegenOuterCtx *outer_ctx = codegener->outer_ctx;
    FklSymbolTable *pst = &outer_ctx->public_symbol_table;
    FklByteCodelntVector resultStack;
    fklByteCodelntVectorInit(&resultStack, 1);
    FklCodegenErrorState errorState = {0, NULL, 0, 0};
    FklCodegenQuestVector codegenQuestStack;
    fklCodegenQuestVectorInit(&codegenQuestStack, 32);
    fklCodegenQuestVectorPushBack2(&codegenQuestStack, initialQuest);
    while (!fklCodegenQuestVectorIsEmpty(&codegenQuestStack)) {
        FklCodegenQuest *curCodegenQuest =
            *fklCodegenQuestVectorBack(&codegenQuestStack);
        FklCodegenInfo *curCodegen = curCodegenQuest->codegen;
        FklCodegenNextExpression *nextExpression =
            curCodegenQuest->nextExpression;
        FklCodegenEnv *curEnv = curCodegenQuest->env;
        FklCodegenQuestContext *curContext = curCodegenQuest->context;
        int r = 1;
        if (nextExpression) {
            FklNastNode *(*get_next_expression)(void *,
                                                FklCodegenErrorState *) =
                nextExpression->t->getNextExpression;
            uint8_t must_has_retval = nextExpression->must_has_retval;

            for (FklNastNode *curExp =
                     get_next_expression(nextExpression->context, &errorState);
                 curExp; curExp = get_next_expression(nextExpression->context,
                                                      &errorState)) {
            skip:
                if (curExp->type == FKL_NAST_PAIR) {
                    FklNastNode *orig_cur_exp = fklMakeNastNodeRef(curExp);
                    curExp = fklTryExpandCodegenMacro(
                        orig_cur_exp, curCodegen, curCodegenQuest->macroScope,
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
                         cs && !replacement; cs = cs->prev)
                        replacement =
                            fklGetReplacement(curExp->sym, cs->replacements);
                    if (replacement) {
                        fklDestroyNastNode(curExp);
                        curExp = fklMakeNastNodeRef(replacement);
                        goto skip;
                    } else if ((f = findBuiltInReplacementWithId(
                                    curExp->sym,
                                    outer_ctx->builtin_replacement_id))) {
                        FklNastNode *t = f(curExp, curEnv, curCodegen);
                        FKL_ASSERT(curExp);
                        fklDestroyNastNode(curExp);
                        curExp = t;
                        goto skip;
                    } else {
                        FklByteCodelnt *bcl = NULL;
                        FklSymDefHashMapElm *def = fklFindSymbolDefByIdAndScope(
                            curExp->sym, curCodegenQuest->scope, curEnv);
                        if (def)
                            bcl = append_get_loc_ins(
                                INS_APPEND_BACK, NULL, def->v.idx,
                                curCodegen->fid, curExp->curline,
                                curCodegenQuest->scope);
                        else {
                            uint32_t idx = fklAddCodegenRefBySidRetIndex(
                                curExp->sym, curEnv, curCodegen->fid,
                                curExp->curline, 0);
                            bcl = append_get_var_ref_ins(
                                INS_APPEND_BACK, NULL, idx, curCodegen->fid,
                                curExp->curline, curCodegenQuest->scope);
                        }
                        curContext->t->__put_bcl(curContext->data, bcl);
                        fklDestroyNastNode(curExp);
                        continue;
                    }
                }
                FKL_ASSERT(curExp);
                r = mapAllBuiltInPattern(
                    curExp, &codegenQuestStack, curCodegenQuest->scope,
                    curCodegenQuest->macroScope, curEnv, curCodegen,
                    &errorState, must_has_retval);
                if (r) {
                    curContext->t->__put_bcl(
                        curContext->data,
                        create_bc_lnt(fklCodegenNode(curExp, curCodegen),
                                      curCodegen->fid, curExp->curline,
                                      curCodegenQuest->scope));
                }
                fklDestroyNastNode(curExp);
                if ((!r
                     && (fklCodegenQuestVectorIsEmpty(&codegenQuestStack)
                         || *fklCodegenQuestVectorBack(&codegenQuestStack)
                                != curCodegenQuest))
                    || errorState.type)
                    break;
            }
        }
        if (errorState.type) {
        print_error:
            fklPrintCodegenError(errorState.place, errorState.type, curCodegen,
                                 curCodegen->runtime_symbol_table,
                                 errorState.line, errorState.fid, pst);
            while (!fklCodegenQuestVectorIsEmpty(&codegenQuestStack)) {
                FklCodegenQuest *curCodegenQuest =
                    *fklCodegenQuestVectorPopBack(&codegenQuestStack);
                destroyCodegenQuest(curCodegenQuest);
            }
            fklCodegenQuestVectorUninit(&codegenQuestStack);
            fklByteCodelntVectorUninit(&resultStack);
            return NULL;
        }
        FklCodegenQuest *otherCodegenQuest =
            *fklCodegenQuestVectorBack(&codegenQuestStack);
        if (otherCodegenQuest == curCodegenQuest) {
            fklCodegenQuestVectorPopBack(&codegenQuestStack);
            FklCodegenQuest *prevQuest =
                curCodegenQuest->prev ? curCodegenQuest->prev
                : fklCodegenQuestVectorIsEmpty(&codegenQuestStack)
                    ? NULL
                    : *fklCodegenQuestVectorBack(&codegenQuestStack);
            FklByteCodelnt *resultBcl = curCodegenQuest->processer(
                curCodegenQuest->codegen, curEnv, curCodegenQuest->scope,
                curCodegenQuest->macroScope, curContext,
                curCodegenQuest->codegen->fid, curCodegenQuest->curline,
                &errorState, outer_ctx);
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
        retval = *fklByteCodelntVectorPopBack(&resultStack);
    fklByteCodelntVectorUninit(&resultStack);
    fklCodegenQuestVectorUninit(&codegenQuestStack);
    if (retval) {
        FklInstruction ret = create_op_ins(FKL_OP_RET);
        fklByteCodeLntPushBackIns(retval, &ret, codegener->fid,
                                  codegener->curline, 1);
        fklPeepholeOptimize(retval);
    }
    return retval;
}

FklByteCodelnt *
fklGenExpressionCodeWithFpForPrecompile(FILE *fp, FklCodegenInfo *codegen,
                                        FklCodegenEnv *cur_env) {
    FklCodegenQuest *initialQuest = fklCreateCodegenQuest(
        pre_compile_main_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(32)),
        createFpNextExpression(fp, codegen), 1, cur_env->macros, cur_env, 1,
        NULL, codegen);
    return fklGenExpressionCodeWithQuest(initialQuest, codegen);
}

FklByteCodelnt *fklGenExpressionCodeWithFp(FILE *fp, FklCodegenInfo *codegen,
                                           FklCodegenEnv *cur_env) {
    FklCodegenQuest *initialQuest = fklCreateCodegenQuest(
        _begin_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(32)),
        createFpNextExpression(fp, codegen), 1, cur_env->macros, cur_env, 1,
        NULL, codegen);
    return fklGenExpressionCodeWithQuest(initialQuest, codegen);
}

FklByteCodelnt *fklGenExpressionCode(FklNastNode *exp, FklCodegenEnv *cur_env,
                                     FklCodegenInfo *codegenr) {
    FklNastNodeQueue *queue = fklNastNodeQueueCreate();
    fklNastNodeQueuePush2(queue, exp);
    FklCodegenQuest *initialQuest = fklCreateCodegenQuest(
        _default_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(32)),
        createDefaultQueueNextExpression(queue), 1, cur_env->macros, cur_env,
        exp->curline, NULL, codegenr);
    return fklGenExpressionCodeWithQuest(initialQuest, codegenr);
}

FklByteCodelnt *fklGenExpressionCodeWithQueue(FklNastNodeQueue *q,
                                              FklCodegenInfo *codegen,
                                              FklCodegenEnv *cur_env) {
    FklCodegenQuest *initialQuest = fklCreateCodegenQuest(
        _begin_exp_bc_process,
        createDefaultStackContext(fklByteCodelntVectorCreate(32)),
        createDefaultQueueNextExpression(q), 1, cur_env->macros, cur_env, 1,
        NULL, codegen);
    return fklGenExpressionCodeWithQuest(initialQuest, codegen);
}

static inline void init_codegen_grammer_ptr(FklCodegenInfo *codegen) {
    codegen->self_g = NULL;
    codegen->g = &codegen->self_g;
    codegen->named_prod_groups = &codegen->self_named_prod_groups;
    codegen->unnamed_prods = &codegen->self_unnamed_prods;
    codegen->unnamed_ignores = &codegen->self_unnamed_ignores;
    codegen->builtin_prods = &codegen->self_builtin_prods;
    codegen->builtin_ignores = &codegen->self_builtin_ignores;
}

FklCodegenEnv *
fklInitGlobalCodegenInfo(FklCodegenInfo *codegen, const char *rp,
                         FklSymbolTable *runtime_st, FklConstTable *runtime_kt,
                         int destroyAbleMark, FklCodegenOuterCtx *outer_ctx,
                         FklCodegenInfoWorkCb work_cb,
                         FklCodegenInfoEnvWorkCb env_work_cb, void *work_ctx) {
    codegen->runtime_symbol_table = runtime_st;
    codegen->runtime_kt = runtime_kt;
    codegen->outer_ctx = outer_ctx;
    if (rp != NULL) {
        codegen->dir = fklGetDir(rp);
        codegen->filename = fklRelpath(outer_ctx->main_file_real_path_dir, rp);
        codegen->realpath = fklCopyCstr(rp);
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

    codegen->libStack = fklCodegenLibVectorCreate(8);
    codegen->macroLibStack = fklCodegenLibVectorCreate(8);
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

    FklCodegenEnv *main_env = fklCreateCodegenEnv(codegen->global_env, 1,
                                                  codegen->global_env->macros);
    create_and_insert_to_pool(codegen, 0, main_env, 0, 1,
                              &outer_ctx->public_symbol_table);
    main_env->refcount++;
    return main_env;
}

void fklInitCodegenInfo(FklCodegenInfo *codegen, const char *filename,
                        FklCodegenInfo *prev, FklSymbolTable *runtime_st,
                        FklConstTable *runtime_kt, int destroyAbleMark,
                        int libMark, int macroMark,
                        FklCodegenOuterCtx *outer_ctx) {
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
        codegen->libStack = prev->libStack;
        codegen->macroLibStack = prev->macroLibStack;
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

        codegen->libStack = fklCodegenLibVectorCreate(8);
        codegen->macroLibStack = fklCodegenLibVectorCreate(8);
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
        FklCodegenLibVector *macroLibStack = codegen->macroLibStack;
        while (!fklCodegenLibVectorIsEmpty(macroLibStack))
            fklUninitCodegenLib(fklCodegenLibVectorPopBack(macroLibStack));
        fklCodegenLibVectorDestroy(macroLibStack);
    }

    if (!codegen->is_destroyable) {
        if (codegen->runtime_symbol_table
            && codegen->runtime_symbol_table
                   != &codegen->outer_ctx->public_symbol_table)
            fklDestroySymbolTable(codegen->runtime_symbol_table);
        if (codegen->runtime_kt
            && codegen->runtime_kt != &codegen->outer_ctx->public_kt)
            fklDestroyConstTable(codegen->runtime_kt);

        while (!fklCodegenLibVectorIsEmpty(codegen->libStack))
            fklUninitCodegenLib(fklCodegenLibVectorPopBack(codegen->libStack));
        fklCodegenLibVectorDestroy(codegen->libStack);

        if (codegen->pts)
            fklDestroyFuncPrototypes(codegen->pts);
    }
    free(codegen->dir);
    if (codegen->filename)
        free(codegen->filename);
    if (codegen->realpath)
        free(codegen->realpath);
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
        fklClearGrammer(g);
        fklUninitRegexTable(&g->regexes);
        fklUninitSymbolTable(&g->terminals);
        fklUninitSymbolTable(&g->reachable_terminals);
        fklGraSidBuiltinHashMapUninit(&g->builtins);
        fklFirstSetHashMapUninit(&g->firstSets);
        fklProdHashMapUninit(&g->productions);
        destroy_grammer_ignore_list(*codegen->builtin_ignores);
        destroy_grammer_ignore_list(*codegen->unnamed_ignores);
        fklProdHashMapUninit(codegen->builtin_prods);
        fklProdHashMapUninit(codegen->unnamed_prods);
        fklGraProdGroupHashMapUninit(codegen->named_prod_groups);
        free(g);
        codegen->g = NULL;
    }
}

static inline void recompute_all_terminal_sid(FklGrammerProdGroupItem *group,
                                              FklSymbolTable *target_table,
                                              FklSymbolTable *origin_table,
                                              FklRegexTable *target_rt,
                                              FklRegexTable *orig_rt) {
    for (FklGrammerIgnore *igs = group->ignore; igs; igs = igs->next)
        recompute_ignores_terminal_sid_and_regex(igs, target_table, target_rt,
                                                 orig_rt);
    for (FklProdHashMapNode *prod_hash_item_list = group->prods.first;
         prod_hash_item_list; prod_hash_item_list = prod_hash_item_list->next) {
        for (FklGrammerProduction *prods = prod_hash_item_list->v; prods;
             prods = prods->next)
            recompute_prod_terminal_sid(prods, target_table, origin_table);
    }
}

void fklInitCodegenScriptLib(FklCodegenLib *lib, FklCodegenInfo *codegen,
                             FklByteCodelnt *bcl, FklCodegenEnv *env) {
    lib->type = FKL_CODEGEN_LIB_SCRIPT;
    lib->bcl = bcl;
    lib->named_prod_groups.buckets = NULL;
    lib->terminal_table.ht.buckets = NULL;
    lib->exports.buckets = NULL;
    if (codegen) {

        lib->rp = codegen->realpath;

        lib->head = codegen->export_macro;
        lib->replacements = codegen->export_replacement;
        if (codegen->export_named_prod_groups
            && codegen->export_named_prod_groups->count) {
            fklInitSymbolTable(&lib->terminal_table);
            fklInitRegexTable(&lib->regexes);
            FklGrammer *g = *codegen->g;
            fklGraProdGroupHashMapInit(&lib->named_prod_groups);
            for (FklSidHashSetNode *sid_list =
                     codegen->export_named_prod_groups->first;
                 sid_list; sid_list = sid_list->next) {
                FklSid_t id = sid_list->k;
                FklGrammerProdGroupItem *group =
                    fklGraProdGroupHashMapGet2(codegen->named_prod_groups, id);
                FklGrammerProdGroupItem *target_group =
                    add_production_group(&lib->named_prod_groups, id);
                for (FklGrammerIgnore *igs = group->ignore; igs;
                     igs = igs->next) {
                    FklGrammerIgnore *ig = (FklGrammerIgnore *)malloc(
                        sizeof(FklGrammerIgnore)
                        + igs->len * sizeof(FklGrammerIgnoreSym));
                    FKL_ASSERT(ig);
                    ig->len = igs->len;
                    ig->next = NULL;
                    memcpy(ig->ig, igs->ig,
                           sizeof(FklGrammerIgnoreSym) * ig->len);
                    fklAddIgnoreToIgnoreList(&target_group->ignore, ig);
                }
                uint32_t top = group->ignore_printing.size;
                FklNastNode **base = group->ignore_printing.base;
                for (uint32_t i = 0; i < top; i++)
                    fklNastNodeVectorPushBack2(&target_group->ignore_printing,
                                               base[i]);
                group->ignore_printing.size = 0;

                top = group->prod_printing.size;
                FklCodegenProdPrinting *base2 = group->prod_printing.base;
                for (uint32_t i = 0; i < top; i++)
                    fklProdPrintingVectorPushBack(&target_group->prod_printing,
                                                  &base2[i]);
                group->prod_printing.size = 0;

                for (FklProdHashMapNode *prod_hash_item_list =
                         group->prods.first;
                     prod_hash_item_list;
                     prod_hash_item_list = prod_hash_item_list->next) {
                    for (FklGrammerProduction *prods = prod_hash_item_list->v;
                         prods; prods = prods->next) {
                        FklGrammerProduction *prod =
                            fklCopyUninitedGrammerProduction(prods);
                        fklAddProdToProdTable(&target_group->prods,
                                              &g->builtins, prod);
                    }
                }
                recompute_all_terminal_sid(target_group, &lib->terminal_table,
                                           &g->terminals, &lib->regexes,
                                           &g->regexes);
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
             sid_idx_list; sid_idx_list = sid_idx_list->next) {
            fklCgExportSidIdxHashMapPut(exports_index, &sid_idx_list->k,
                                        &sid_idx_list->v);
        }
    } else
        lib->prototypeId = 0;
}

void fklInitCodegenDllLib(FklCodegenLib *lib, char *rp, uv_lib_t dll,
                          FklSymbolTable *table,
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
            fklCgExportSidIdxHashMapAdd(exports_idx, &exports[i],
                                        &(FklCodegenExportIdx){.idx = i});
        }
    }
    if (exports)
        free(exports);
}

void fklDestroyCodegenMacroList(FklCodegenMacro *cur) {
    while (cur) {
        FklCodegenMacro *t = cur;
        cur = cur->next;
        fklDestroyCodegenMacro(t);
    }
}

void fklDestroyCodegenLibMacroScope(FklCodegenLib *lib) {
    fklDestroyCodegenMacroList(lib->head);
    if (lib->replacements)
        fklReplacementHashMapDestroy(lib->replacements);
}

void fklUninitCodegenLibInfo(FklCodegenLib *lib) {
    if (lib->exports.buckets)
        fklCgExportSidIdxHashMapUninit(&lib->exports);
    free(lib->rp);
    if (lib->named_prod_groups.buckets) {
        fklGraProdGroupHashMapUninit(&lib->named_prod_groups);
        fklUninitSymbolTable(&lib->terminal_table);
        fklUninitRegexTable(&lib->regexes);
    }
}

void fklUninitCodegenLib(FklCodegenLib *lib) {
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        fklDestroyByteCodelnt(lib->bcl);
        break;
    case FKL_CODEGEN_LIB_DLL:
        uv_dlclose(&lib->dll);
        break;
    }
    fklUninitCodegenLibInfo(lib);
    fklDestroyCodegenLibMacroScope(lib);
}

FklCodegenMacro *fklCreateCodegenMacro(FklNastNode *pattern,
                                       FklNastNode *origin_exp,
                                       FklByteCodelnt *bcl,
                                       FklCodegenMacro *next,
                                       uint32_t prototype_id, int own) {
    FklCodegenMacro *r = (FklCodegenMacro *)malloc(sizeof(FklCodegenMacro));
    FKL_ASSERT(r);
    r->pattern = pattern;
    r->origin_exp = origin_exp;
    r->bcl = bcl;
    r->next = next;
    r->prototype_id = prototype_id;
    r->own = own;
    return r;
}

void fklDestroyCodegenMacro(FklCodegenMacro *macro) {
    uninit_codegen_macro(macro);
    free(macro);
}

FklCodegenMacroScope *fklCreateCodegenMacroScope(FklCodegenMacroScope *prev) {
    FklCodegenMacroScope *r =
        (FklCodegenMacroScope *)malloc(sizeof(FklCodegenMacroScope));
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
            free(macros);
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

static void initVMframeFromPatternMatchTable(FklVM *exe, FklVMframe *frame,
                                             FklPmatchHashMap *ht,
                                             FklLineNumHashMap *lineHash,
                                             FklFuncPrototypes *pts,
                                             uint32_t prototype_id) {
    FklFuncPrototype *mainPts = &pts->pa[prototype_id];
    FklVMCompoundFrameVarRef *lr = fklGetCompoundFrameLocRef(frame);
    FklVMproc *proc = FKL_VM_PROC(fklGetCompoundFrameProc(frame));
    uint32_t count = mainPts->lcount;
    // FklVMvalue **loc = fklAllocSpaceForLocalVar(exe, count);
    uint32_t idx = 0;
    for (FklPmatchHashMapNode *list = ht->first; list; list = list->next) {
        FklVMvalue *v = fklCreateVMvalueFromNastNode(exe, list->v, lineHash);
        FKL_VM_GET_ARG(exe, frame, idx) = v;
        // loc[idx] = v;
        idx++;
    }
    // lr->loc = loc;
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
    fklVMacquireSt(exe->gc);
    *(ctx->retval) = fklCreateNastNodeFromVMvalue(
        fklGetTopValue(exe), ctx->curline, ctx->lineHash, exe->gc);
    fklVMreleaseSt(exe->gc);
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

static void insert_macro_expand_frame(FklVM *exe, FklVMframe *mainframe,
                                      FklNastNode **ptr,
                                      FklLineNumHashMap *lineHash,
                                      FklSymbolTable *symbolTable,
                                      uint64_t curline) {
    FklVMframe *f =
        fklCreateOtherObjVMframe(exe, &MacroExpandMethodTable, mainframe->prev);
    mainframe->prev = f;
    MacroExpandCtx *ctx = (MacroExpandCtx *)f->data;
    ctx->retval = ptr;
    ctx->lineHash = lineHash;
    ctx->symbolTable = symbolTable;
    ctx->curline = curline;
}

FklVM *fklInitMacroExpandVM(FklByteCodelnt *bcl, FklFuncPrototypes *pts,
                            uint32_t prototype_id, FklPmatchHashMap *ht,
                            FklLineNumHashMap *lineHash,
                            FklCodegenLibVector *macroLibStack,
                            FklNastNode **pr, uint64_t curline,
                            FklSymbolTable *publicSymbolTable,
                            FklConstTable *public_kt) {
    FklVM *anotherVM =
        fklCreateVMwithByteCode(fklCopyByteCodelnt(bcl), publicSymbolTable,
                                public_kt, pts, prototype_id);
    insert_macro_expand_frame(anotherVM, anotherVM->top_frame, pr, lineHash,
                              publicSymbolTable, curline);

    anotherVM->libNum = macroLibStack->size;
    anotherVM->libs =
        (FklVMlib *)calloc(macroLibStack->size + 1, sizeof(FklVMlib));
    FKL_ASSERT(anotherVM->libs);
    for (size_t i = 0; i < macroLibStack->size; i++) {
        const FklCodegenLib *cur = &macroLibStack->base[i];
        fklInitVMlibWithCodegenLib(cur, &anotherVM->libs[i + 1], anotherVM, 1,
                                   pts);
        if (cur->type == FKL_CODEGEN_LIB_SCRIPT)
            fklInitMainProcRefs(anotherVM, anotherVM->libs[i + 1].proc);
    }
    FklVMframe *mainframe = anotherVM->top_frame;
    initVMframeFromPatternMatchTable(anotherVM, mainframe, ht, lineHash, pts,
                                     prototype_id);
    return anotherVM;
}

static inline void get_macro_pts_and_lib(FklCodegenInfo *info,
                                         FklFuncPrototypes **ppts,
                                         FklCodegenLibVector **plib) {
    for (; info->prev; info = info->prev)
        ;
    *ppts = info->macro_pts;
    *plib = info->macroLibStack;
}

FklNastNode *fklTryExpandCodegenMacro(FklNastNode *exp, FklCodegenInfo *codegen,
                                      FklCodegenMacroScope *macros,
                                      FklCodegenErrorState *errorState) {
    FklSymbolTable *pst = &codegen->outer_ctx->public_symbol_table;
    FklConstTable *pkt = &codegen->outer_ctx->public_kt;
    FklNastNode *r = exp;
    FklPmatchHashMap ht = {.buckets = NULL};
    uint64_t curline = exp->curline;
    for (FklCodegenMacro *macro = findMacro(r, macros, &ht);
         !errorState->type && macro; macro = findMacro(r, macros, &ht)) {
        fklPmatchHashMapAdd2(&ht, codegen->outer_ctx->builtInPatternVar_orig,
                             exp);
        FklNastNode *retval = NULL;
        FklLineNumHashMap lineHash;
        fklLineNumHashMapInit(&lineHash);

        FklCodegenOuterCtx *outer_ctx = codegen->outer_ctx;
        const char *cwd = outer_ctx->cwd;
        fklChdir(codegen->dir);

        FklFuncPrototypes *pts = NULL;
        FklCodegenLibVector *macroLibStack = NULL;
        get_macro_pts_and_lib(codegen, &pts, &macroLibStack);
        FklVM *anotherVM = fklInitMacroExpandVM(
            macro->bcl, pts, macro->prototype_id, &ht, &lineHash, macroLibStack,
            &retval, r->curline, pst, pkt);
        FklVMgc *gc = anotherVM->gc;
        int e = fklRunVM(anotherVM);

        fklChdir(cwd);

        gc->pts = NULL;
        if (e) {
            errorState->type = FKL_ERR_MACROEXPANDFAILED;
            errorState->place = r;
            errorState->line = curline;
            fklDeleteCallChain(anotherVM);
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
        fklDestroyAllVMs(anotherVM);
        fklDestroyVMgc(gc);
    }
    if (ht.buckets)
        fklPmatchHashMapUninit(&ht);
    return r;
}

void fklInitVMlibWithCodegenLibRefs(FklCodegenLib *clib, FklVMlib *vlib,
                                    FklVM *exe, FklVMvalue **refs,
                                    uint32_t count, int needCopy,
                                    FklFuncPrototypes *pts) {
    FklVMvalue *val = FKL_VM_NIL;
    if (clib->type == FKL_CODEGEN_LIB_SCRIPT) {
        FklByteCode *bc = clib->bcl->bc;
        FklVMvalue *codeObj = fklCreateVMvalueCodeObj(
            exe, needCopy ? fklCopyByteCodelnt(clib->bcl) : clib->bcl);
        FklVMvalue *proc = fklCreateVMvalueProc(exe, bc->code, bc->len, codeObj,
                                                clib->prototypeId);
        fklInitMainProcRefs(exe, proc);
        val = proc;
    } else
        val = fklCreateVMvalueStr2(
            exe, strlen(clib->rp) - strlen(FKL_DLL_FILE_TYPE), clib->rp);
    fklInitVMlib(vlib, val);
}

void fklInitVMlibWithCodegenLib(const FklCodegenLib *clib, FklVMlib *vlib,
                                FklVM *exe, int needCopy,
                                FklFuncPrototypes *pts) {
    FklVMvalue *val = FKL_VM_NIL;
    if (clib->type == FKL_CODEGEN_LIB_SCRIPT) {
        FklByteCode *bc = clib->bcl->bc;
        FklVMvalue *codeObj = fklCreateVMvalueCodeObj(
            exe, needCopy ? fklCopyByteCodelnt(clib->bcl) : clib->bcl);
        FklVMvalue *proc = fklCreateVMvalueProc(exe, bc->code, bc->len, codeObj,
                                                clib->prototypeId);
        val = proc;
    } else
        val = fklCreateVMvalueStr2(
            exe, strlen(clib->rp) - strlen(FKL_DLL_FILE_TYPE), clib->rp);
    fklInitVMlib(vlib, val);
}

void fklUninitCodegenLibExceptBclAndDll(FklCodegenLib *clib) {
    fklDestroyCodegenLibMacroScope(clib);
    fklUninitCodegenLibInfo(clib);
}

void fklInitVMlibWithCodegenLibAndDestroy(FklCodegenLib *clib, FklVMlib *vlib,
                                          FklVM *exe, FklFuncPrototypes *pts) {
    FklVMvalue *val = FKL_VM_NIL;
    if (clib->type == FKL_CODEGEN_LIB_SCRIPT) {
        FklByteCode *bc = clib->bcl->bc;
        FklVMvalue *codeObj = fklCreateVMvalueCodeObj(exe, clib->bcl);
        FklVMvalue *proc = fklCreateVMvalueProc(exe, bc->code, bc->len, codeObj,
                                                clib->prototypeId);
        val = proc;
    } else {
        val = fklCreateVMvalueStr2(
            exe, strlen(clib->rp) - strlen(FKL_DLL_FILE_TYPE), clib->rp);
        uv_dlclose(&clib->dll);
    }
    fklInitVMlib(vlib, val);

    fklUninitCodegenLibExceptBclAndDll(clib);
}

static inline void print_nast_node_with_null_chr(const FklNastNode *node,
                                                 const FklSymbolTable *st,
                                                 FILE *fp) {
    fklPrintNastNode(node, fp, st);
    fputc('\0', fp);
}

static inline void write_ignores(FklNastNodeVector *igs,
                                 const FklSymbolTable *st, FILE *fp) {
    uint32_t top = igs->size;
    fwrite(&top, sizeof(top), 1, fp);
    for (uint32_t i = 0; i < top; i++) {
        FklNastNode *node = igs->base[i];
        print_nast_node_with_null_chr(node, st, fp);
    }
}

static inline void write_prods(FklProdPrintingVector *prods,
                               const FklSymbolTable *st, FILE *fp) {
    uint32_t top = prods->size;
    fwrite(&top, sizeof(top), 1, fp);
    for (uint32_t i = 0; i < top; i++) {
        const FklCodegenProdPrinting *printing = &prods->base[i];
        fwrite(&printing->group_id, sizeof(printing->group_id), 1, fp);
        fwrite(&printing->sid, sizeof(printing->sid), 1, fp);
        print_nast_node_with_null_chr(printing->vec, st, fp);
        fwrite(&printing->add_extra, sizeof(printing->add_extra), 1, fp);
        fwrite(&printing->type, sizeof(printing->type), 1, fp);
        if (printing->type == FKL_CODEGEN_PROD_CUSTOM) {
            uint32_t protoId = printing->prototype_id;
            fwrite(&protoId, sizeof(protoId), 1, fp);
            fklWriteByteCodelnt(printing->bcl, fp);
        } else
            print_nast_node_with_null_chr(printing->forth, st, fp);
    }
}

void fklWriteNamedProds(const FklGraProdGroupHashMap *named_prod_groups,
                        const FklSymbolTable *st, FILE *fp) {
    uint8_t has_named_prod = named_prod_groups->buckets != NULL;
    fwrite(&has_named_prod, sizeof(has_named_prod), 1, fp);
    if (!has_named_prod)
        return;
    fwrite(&named_prod_groups->count, sizeof(named_prod_groups->count), 1, fp);
    for (FklGraProdGroupHashMapNode *list = named_prod_groups->first; list;
         list = list->next) {
        fwrite(&list->k, sizeof(list->k), 1, fp);
        write_prods(&list->v.prod_printing, st, fp);
        write_ignores(&list->v.ignore_printing, st, fp);
    }
}

void fklWriteExportNamedProds(const FklSidHashSet *export_named_prod_groups,
                              const FklGraProdGroupHashMap *named_prod_groups,
                              const FklSymbolTable *st, FILE *fp) {
    uint8_t has_named_prod = export_named_prod_groups->count > 0;
    fwrite(&has_named_prod, sizeof(has_named_prod), 1, fp);
    if (!has_named_prod)
        return;
    fwrite(&export_named_prod_groups->count,
           sizeof(export_named_prod_groups->count), 1, fp);
    for (FklSidHashSetNode *list = export_named_prod_groups->first; list;
         list = list->next) {
        FklSid_t id = list->k;
        FklGrammerProdGroupItem *group =
            get_production_group(named_prod_groups, id);
        fwrite(&id, sizeof(id), 1, fp);
        write_prods(&group->prod_printing, st, fp);
        write_ignores(&group->ignore_printing, st, fp);
    }
}
