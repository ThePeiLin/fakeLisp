#include <fakeLisp/base.h>
#include <fakeLisp/builtin.h>
#include <fakeLisp/bytecode.h>
#include <fakeLisp/code_lw.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/common.h>
#include <fakeLisp/grammer.h>
#include <fakeLisp/opcode.h>
#include <fakeLisp/optimizer.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/parser_grammer.h>
#include <fakeLisp/pattern.h>
#include <fakeLisp/regex.h>
#include <fakeLisp/str_buf.h>
#include <fakeLisp/symbol.h>
#include <fakeLisp/utils.h>
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

#define GCVM (&ctx->gc->gcvm)
#define IGCVM (&info->ctx->gc->gcvm)

static FklVMvalue *gen_push_literal_code(FklVM *exe,
        const FklPmatchRes *node,
        FklVMvalueCgInfo *info,
        FklVMvalueCgEnv *env,
        uint32_t scope);

static inline FklVMvalue *cdr(const FklVMvalue *node) {
    return FKL_VM_CDR(node);
}

static inline FklVMvalue *car(const FklVMvalue *node) {
    return FKL_VM_CAR(node);
}

static inline FklVMvalue *cadr(const FklVMvalue *node) {
    return car(cdr(node));
}

static inline FklVMvalue *cddr(const FklVMvalue *node) {
    return cdr(cdr(node));
}

static inline FklVMvalue *caddr(const FklVMvalue *node) {
    return car(cdr(cdr(node)));
}

static inline FklVMvalue *caadr(const FklVMvalue *node) {
    return car(car(cdr(node)));
}

static inline int
is_defmacro_exp(const FklVMvalue *c, FklCgCtx *ctx, FklPmatchHashMap *ht) {
    return fklPatternMatch(
            ctx->builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFMACRO],
            c,
            ht);
}

static inline int
is_def_reader_exp(const FklVMvalue *c, FklCgCtx *ctx, FklPmatchHashMap *ht) {
    return fklPatternMatch(
            ctx->builtin_pattern_node[FKL_CODEGEN_PATTERN_DEF_READER_MACROS],
            c,
            ht);
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

static inline int is_export_none_exp(FklVMvalue *c, FklCgCtx *ctx) {
    return fklPatternMatch(
            ctx->builtin_pattern_node[FKL_CODEGEN_PATTERN_IMPORT_NONE],
            c,
            NULL);
}

static inline int is_import_exp(FklVMvalue *c, FklCgCtx *ctx) {
    for (FklCgPatternEnum i = FKL_CODEGEN_PATTERN_IMPORT_PREFIX;
            i <= FKL_CODEGEN_PATTERN_IMPORT_NONE;
            ++i) {
        if (fklPatternMatch(ctx->builtin_pattern_node[i], c, NULL)) {
            return 1;
        }
    }

    return 0;
}

static const FklCgActCtxMethodTable DefaultStackContextMethodTable = {
    .size = 0
};

static FklCgActCtx *createCgActCtx(const FklCgActCtxMethodTable *t) {
    FklCgActCtx *r =
            (FklCgActCtx *)fklZcalloc(1, sizeof(FklCgActCtx) + t->size);
    FKL_ASSERT(r);
    r->t = t;
    return r;
}

static FklCgActCtx *createDefaultStackContext(void) {
    return createCgActCtx(&DefaultStackContextMethodTable);
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

static FklCgNextExp *createCgNextExp(const FklNextExpressionMethodTable *t,
        void *context,
        uint8_t must_has_retval) {
    FklCgNextExp *r = (FklCgNextExp *)fklZmalloc(sizeof(FklCgNextExp));
    FKL_ASSERT(r);
    r->t = t;
    r->context = context;
    r->must_has_retval = must_has_retval;
    return r;
}

static int _default_codegen_get_next_expression(void *context,
        FklPmatchRes *out,
        FklCgErrorState *error_state) {
    FklPmatchRes *head = cgExpQueuePop(FKL_TYPE_CAST(CgExpQueue *, context));
    if (head == NULL)
        return 0;
    *out = *head;
    return 1;
}

static void _default_codegen_next_expression_finalizer(void *context) {
    CgExpQueue *q = FKL_TYPE_CAST(CgExpQueue *, context);
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
            .finalize = _default_codegen_next_expression_finalizer,
            .atomic = _default_codegen_next_expression_atomic,
        };

static FklCgNextExp *createDefaultQueueNextExpression(CgExpQueue *queue) {
    return createCgNextExp(&_default_codegen_next_expression_method_table,
            queue,
            DO_NOT_NEED_RETVAL);
}

static FklCgNextExp *createMustHasRetvalQueueNextExpression(CgExpQueue *queue) {
    return createCgNextExp(&_default_codegen_next_expression_method_table,
            queue,
            ALL_MUST_HAS_RETVAL);
}

static FklCgNextExp *createFirstHasRetvalQueueNextExpression(
        CgExpQueue *queue) {
    return createCgNextExp(&_default_codegen_next_expression_method_table,
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

static inline FklVMvalue *create_0len_bcl(FklVM *exe) {
    return fklCreateVMvalueCodeObj1(exe);
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

static inline FklVMvalue *set_and_append_ins_with_unsigned_imm(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        FklOpcode op,
        uint64_t k,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklInstruction ins[3] = { FKL_INSTRUCTION_STATIC_INIT };
    int l = set_ins_with_unsigned_imm(ins, op, k);
    if (bcl == NULL)
        bcl = create_0len_bcl(exe);
    switch (m) {
    case INS_APPEND_BACK:
        for (int i = 0; i < l; i++)
            fklByteCodeLntPushBackIns(FKL_VM_CO(bcl),
                    &ins[i],
                    fid,
                    line,
                    scope);
        break;
    case INS_APPEND_FRONT:
        for (; l > 0; l--)
            fklByteCodeLntInsertFrontIns(&ins[l - 1],
                    FKL_VM_CO(bcl),
                    fid,
                    line,
                    scope);
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

static inline FklVMvalue *set_and_append_ins_with_2_unsigned_imm(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        FklOpcode op,
        uint64_t ux,
        uint64_t uy,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklInstruction ins[4] = { FKL_INSTRUCTION_STATIC_INIT };
    int l = set_ins_with_2_unsigned_imm(ins, op, ux, uy);
    if (bcl == NULL)
        bcl = create_0len_bcl(exe);
    switch (m) {
    case INS_APPEND_BACK:
        for (int i = 0; i < l; i++)
            fklByteCodeLntPushBackIns(FKL_VM_CO(bcl),
                    &ins[i],
                    fid,
                    line,
                    scope);
        break;
    case INS_APPEND_FRONT:
        for (; l > 0; l--)
            fklByteCodeLntInsertFrontIns(&ins[l - 1],
                    FKL_VM_CO(bcl),
                    fid,
                    line,
                    scope);
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

static inline FklVMvalue *set_and_append_ins_with_signed_imm(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        FklOpcode op,
        int64_t k,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklInstruction ins[3] = { FKL_INSTRUCTION_STATIC_INIT };
    int l = set_ins_with_signed_imm(ins, op, k);
    if (bcl == NULL)
        bcl = create_0len_bcl(exe);
    switch (m) {
    case INS_APPEND_BACK:
        for (int i = 0; i < l; i++)
            fklByteCodeLntPushBackIns(FKL_VM_CO(bcl),
                    &ins[i],
                    fid,
                    line,
                    scope);
        break;
    case INS_APPEND_FRONT:
        for (; l > 0; l--)
            fklByteCodeLntInsertFrontIns(&ins[l - 1],
                    FKL_VM_CO(bcl),
                    fid,
                    line,
                    scope);
    }
    return bcl;
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

static inline FklByteCode *append_push_const_ins_to_bc(InsAppendMode m,
        FklByteCode *bc,
        uint32_t k,
        FklVMvalueCgInfo *info) {
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
        FklVMvalueCgInfo *info) {
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

static inline FklVMvalue *append_push_i24_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        int64_t k,
        FklVMvalueCgInfo *info,
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
            bcl = create_0len_bcl(exe);
        switch (m) {
        case INS_APPEND_BACK:
            fklByteCodeLntPushBackIns(FKL_VM_CO(bcl), &ins, fid, line, scope);
            break;
        case INS_APPEND_FRONT:
            fklByteCodeLntInsertFrontIns(&ins,
                    FKL_VM_CO(bcl),
                    fid,
                    line,
                    scope);
        }
    } else {
        FKL_UNREACHABLE();
    }
    return bcl;
}

static inline FklVMvalue *append_push_proc_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t prototypeId,
        uint64_t len,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(exe,
            m,
            bcl,
            FKL_OP_PUSH_PROC,
            prototypeId,
            len,
            fid,
            line,
            scope);
}

static inline FklVMvalue *append_jmp_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
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
        return set_and_append_ins_with_signed_imm(exe,
                m,
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
            return set_and_append_ins_with_signed_imm(exe,
                    m,
                    bcl,
                    op,
                    len,
                    fid,
                    line,
                    scope);
        len -= 1;
        if (len >= INT32_MIN)
            return set_and_append_ins_with_signed_imm(exe,
                    m,
                    bcl,
                    op,
                    len,
                    fid,
                    line,
                    scope);
        len -= 1;
        return set_and_append_ins_with_signed_imm(exe,
                m,
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

static inline FklVMvalue *append_import_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t idx1,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(exe,
            m,
            bcl,
            FKL_OP_IMPORT,
            idx,
            idx1,
            fid,
            line,
            scope);
}

static inline FklVMvalue *append_load_lib_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t lib_id,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
            bcl,
            FKL_OP_LOAD_LIB,
            lib_id,
            fid,
            line,
            scope);
}

static inline FklVMvalue *append_load_dll_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t lib_id,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
            bcl,
            FKL_OP_LOAD_DLL,
            lib_id,
            fid,
            line,
            scope);
}

static inline FklVMvalue *append_export_to_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t libId,
        uint32_t num,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(exe,
            m,
            bcl,
            FKL_OP_EXPORT_TO,
            libId,
            num,
            fid,
            line,
            scope);
}

static inline FklVMvalue *append_export_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
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

static inline FklVMvalue *append_pop_loc_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
            bcl,
            FKL_OP_POP_LOC,
            idx,
            fid,
            line,
            scope);
}

static inline FklVMvalue *append_put_loc_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
            bcl,
            FKL_OP_PUT_LOC,
            idx,
            fid,
            line,
            scope);
}

static inline FklVMvalue *append_put_var_ref_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
            bcl,
            FKL_OP_PUT_VAR_REF,
            idx,
            fid,
            line,
            scope);
}

static FklCgAct *create_cg_action(FklCgActCb f,
        FklCgActCtx *context,
        FklCgNextExp *nextExpression,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalueCgEnv *env,
        uint64_t curline,
        FklCgAct *prev,
        FklVMvalueCgInfo *info) {
    FklCgAct *r = (FklCgAct *)fklZmalloc(sizeof(FklCgAct));
    FKL_ASSERT(r);
    r->scope = scope;
    r->macros = macro_scope;
    r->cb = f;
    r->ctx = context;
    r->exps = nextExpression;
    r->env = env;
    r->curline = curline;
    r->info = info;
    r->prev = prev;
    fklValueVectorInit(&r->bcl_vector, 0);
    return r;
}

static void destroy_cg_action(FklCgAct *action) {
    FklCgNextExp *nextExpression = action->exps;
    if (nextExpression) {
        if (nextExpression->t->finalize)
            nextExpression->t->finalize(nextExpression->context);
        fklZfree(nextExpression);
    }
    FklCgActCtx *ctx = action->ctx;
    if (ctx->t->finalize) {
        ctx->t->finalize(ctx->d);
    }
    fklZfree(action->ctx);
    action->ctx = NULL;

    action->ctx = NULL;

    fklValueVectorUninit(&action->bcl_vector);

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
    fklCgActVectorPushBack2((CODEGEN_CONTEXT),                                 \
            create_cg_action((F),                                              \
                    (STACK),                                                   \
                    (NEXT_EXPRESSIONS),                                        \
                    (SCOPE),                                                   \
                    (MACRO_SCOPE),                                             \
                    (ENV),                                                     \
                    (LINE),                                                    \
                    NULL,                                                      \
                    (CODEGEN)))

static FklVMvalue *_default_bc_process(const FklCgActCbArgs *args) {
    FklValueVector *bcl_vec = args->bcl_vec;

    if (!fklValueVectorIsEmpty(bcl_vec))
        return *fklValueVectorPopBackNonNull(bcl_vec);
    else
        return NULL;
}

static FklVMvalue *_empty_bc_process(const FklCgActCbArgs *args) {
    return NULL;
}

static FklVMvalue *sequnce_exp_bc_process(FklVM *exe,
        FklValueVector *stack,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    if (stack->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklVMvalue *retval = create_0len_bcl(exe);
        size_t top = stack->size;
        for (size_t i = 0; i < top; i++) {
            FklVMvalue *cur = stack->base[i];
            FklByteCodelnt *co = FKL_VM_CO(cur);
            if (co->bc.len) {
                fklCodeLntConcat(FKL_VM_CO(retval), co);
                if (i < top - 1)
                    fklByteCodeLntPushBackIns(FKL_VM_CO(retval),
                            &drop,
                            fid,
                            line,
                            scope);
            }
        }
        stack->size = 0;
        return retval;
    } else
        return fklCreateVMvalueCodeObjExt(exe,
                create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static FklVMvalue *_begin_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    return sequnce_exp_bc_process(GCVM, bcl_vec, fid, line, scope);
}

static inline void
pushListItemToQueue(FklVMvalue *list, CgExpQueue *queue, FklVMvalue **last) {
    for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list))
        cgExpQueuePush2(queue,
                (FklPmatchRes){
                    .value = FKL_VM_CAR(list),
                    .container = list,
                });
    if (last)
        *last = list;
}

static inline void pushListItemToStack(FklVMvalue *list,
        FklValueVector *stack,
        FklVMvalue **last) {
    for (; FKL_IS_PAIR(list); list = FKL_VM_CDR(list))
        fklValueVectorPushBack2(stack, FKL_VM_CAR(list));
    if (last)
        *last = list;
}

static inline int is_get_var_ref_ins(const FklInstruction *ins) {
    return ins->op >= FKL_OP_GET_VAR_REF && ins->op <= FKL_OP_GET_VAR_REF_X;
}

static inline FklBuiltinInlineFunc is_inlinable_func_ref(const FklByteCode *bc,
        FklVMvalueCgEnv *env,
        uint32_t argNum,
        FklVMvalueCgInfo *info) {
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
                        if (!env->prev && env == info->global_env)
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

static FklVMvalue *_funcall_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    if (bcl_vec->size) {
        FklVMvalue *func = bcl_vec->base[0];
        FklByteCode *funcBc = &FKL_VM_CO(func)->bc;
        uint32_t argNum = bcl_vec->size - 1;
        FklBuiltinInlineFunc inlFunc = NULL;
        if (argNum < 4
                && (inlFunc = is_inlinable_func_ref(funcBc,
                            env,
                            argNum,
                            info))) {
            bcl_vec->size = 0;
            return inlFunc(GCVM, bcl_vec->base + 1, fid, line, scope);
        } else {
            FklVMvalue *retval = create_0len_bcl(GCVM);
            FklVMvalue **base = bcl_vec->base;
            FklVMvalue **const end = base + bcl_vec->size;
            for (; base < end; ++base) {
                FklVMvalue *cur = *base;
                fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
            }
            bcl_vec->size = 0;
            FklInstruction setBp = create_op_ins(FKL_OP_SET_BP);
            FklInstruction call = create_op_ins(FKL_OP_CALL);
            fklByteCodeLntInsertFrontIns(&setBp,
                    FKL_VM_CO(retval),
                    fid,
                    line,
                    scope);
            fklByteCodeLntPushBackIns(FKL_VM_CO(retval),
                    &call,
                    fid,
                    line,
                    scope);
            return retval;
        }
    } else
        return fklCreateVMvalueCodeObjExt(GCVM,
                create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

#define CURLINE(V) get_curline(info, V)

static inline FklVMvalue *make_syntax_error(FklVM *exe, FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_SYNTAXERROR,
            exe,
            "Invalid syntax %S",
            place);
}

static inline FklVMvalue *make_assign_const_error(FklVM *exe,
        FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_ASSIGN_CONSTANT,
            exe,
            "Attempt to assign constant %S",
            place);
}

static inline FklVMvalue *make_redefine_const_error(FklVM *exe,
        FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_REDEFINE_VARIABLE_AS_CONSTANT,
            exe,
            "Attempt to redefine variable %S as constant",
            place);
}

static inline FklVMvalue *make_file_failure_error(FklVM *exe,
        FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_FILEFAILURE,
            exe,
            "Failed for file: %S",
            place);
}

static inline FklVMvalue *
make_file_failure_error2(FklVM *exe, const char *msg2, FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_FILEFAILURE,
            exe,
            "%s %S",
            fklCreateVMvalueStrFromCstr(exe, msg2),
            place);
}

static inline FklVMvalue *
make_import_reader_macro_error(FklVM *exe, const char *msg, FklVMvalue *place) {
    if (place) {
        return FKL_MAKE_VM_ERR(FKL_ERR_IMPORT_READER_MACRO_ERROR,
                exe,
                "%s %S",
                fklCreateVMvalueStrFromCstr(exe, msg),
                place);
    }
    return FKL_MAKE_VM_ERR(FKL_ERR_IMPORT_READER_MACRO_ERROR,
            exe,
            "%S",
            fklCreateVMvalueStrFromCstr(exe, msg));
}

static inline FklVMvalue *make_import_missing_error(FklVM *exe,
        FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_IMPORT_MISSING,
            exe,
            "Missing import for %S",
            place);
}

static inline FklVMvalue *make_has_no_value_error(FklVM *exe,
        FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_EXP_HAS_NO_VALUE,
            exe,
            "Expression %S has no value",
            place);
}

static inline FklVMvalue *make_import_failed_error(FklVM *exe,
        FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_IMPORTFAILED,
            exe,
            "Failed for importing module: %S",
            place);
}

static inline FklVMvalue *
make_import_failed_error2(FklVM *exe, const char *str, FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_IMPORTFAILED,
            exe,
            "%s %S",
            fklCreateVMvalueStrFromCstr(exe, str),
            place);
}

static inline FklVMvalue *make_grammer_create_error(FklVM *exe) {
    return FKL_MAKE_VM_ERR(FKL_ERR_GRAMMER_CREATE_FAILED,
            exe,
            "Failed to create grammer");
}

static inline FklVMvalue *
make_grammer_create_error2(FklVM *exe, const char *s, FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_GRAMMER_CREATE_FAILED,
            exe,
            "%s %S",
            fklCreateVMvalueStrFromCstr(exe, s),
            place);
}

static inline FklVMvalue *make_parse_error(FklVM *exe, FklParseError e) {
    const char *msg = NULL;
    FklBuiltinErrorType err_type = FKL_ERR_DUMMY;
    switch (e) {
    default:
        FKL_UNREACHABLE();
        break;
    case FKL_PARSE_TERMINAL_MATCH_FAILED:
        msg = "Unexpected eof";
        err_type = FKL_ERR_UNEXPECTED_EOF;
        break;
    case FKL_PARSE_REDUCE_FAILED:
        msg = "Invalid expression";
        err_type = FKL_ERR_INVALIDEXPR;
        break;
    }

    FKL_ASSERT(msg);
    return FKL_MAKE_VM_ERR(err_type, exe, msg);
}

static inline FklVMvalue *make_circular_load_error(FklVM *exe,
        FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_CIRCULARLOAD,
            exe,
            "Circular load file %S",
            place);
}

static inline FklVMvalue *make_macro_pattern_error(FklVM *exe,
        FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_INVALID_MACRO_PATTERN,
            exe,
            "Invalid macro pattern %S",
            place);
}

static inline FklVMvalue *
make_export_error(FklVM *exe, const char *msg, FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_EXPORT_ERROR,
            exe,
            "%s %S",
            fklCreateVMvalueStrFromCstr(exe, msg),
            place);
}

static void codegen_funcall(const FklPmatchRes *rest,
        FklCgActVector *actions,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info,
        FklCgCtx *ctx,
        FklCgErrorState *error_state) {
    CgExpQueue *queue = cgExpQueueCreate();
    FklVMvalue *last = NULL;
    pushListItemToQueue(rest->value, queue, &last);
    if (last != FKL_VM_NIL) {
        error_state->error = make_syntax_error(GCVM, rest->value);
        error_state->line = CURLINE(rest->container);

        cgExpQueueDestroy(queue);
    } else {
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_funcall_exp_bc_process,
                createDefaultStackContext(),
                createMustHasRetvalQueueNextExpression(queue),
                scope,
                macro_scope,
                env,
                CURLINE(rest->container),
                info,
                actions);
    }
}

typedef struct {
    const FklPmatchRes *orig;
    FklPmatchHashMap *ht;
    FklCgActVector *actions;
    uint32_t scope;
    FklVMvalueCgMacroScope *macro_scope;
    FklVMvalueCgEnv *env;
    FklVMvalueCgInfo *info;
    FklCgCtx *ctx;
    FklCgErrorState *err_state;
    uint8_t must_has_retval;
} CgCbArgs;

static void codegen_begin(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_begin_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(rest->container),
            info,
            actions);
}

static inline int reset_flag_and_check_var_be_refed(uint8_t *flags,
        FklCgEnvScope *sc,
        uint32_t scope,
        FklVMvalueCgEnv *env,
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

static inline FklVMvalue *append_close_ref_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *retval,
        uint32_t s,
        uint32_t e,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_2_unsigned_imm(exe,
            m,
            retval,
            FKL_OP_CLOSE_REF,
            s,
            e,
            fid,
            line,
            scope);
}

static inline void check_and_close_ref(FklVM *exe,
        FklVMvalue *retval,
        uint32_t scope,
        FklVMvalueCgEnv *env,
        FklFuncPrototypes *pa,
        FklVMvalue *fid,
        uint32_t line) {
    FklCgEnvScope *cur = &env->scopes[scope - 1];
    uint32_t start = cur->start;
    uint32_t end = start + 1;
    if (reset_flag_and_check_var_be_refed(env->slotFlags,
                cur,
                scope,
                env,
                pa,
                &start,
                &end))
        append_close_ref_ins(exe,
                INS_APPEND_BACK,
                retval,
                start,
                end,
                fid,
                line,
                scope);
}

static FklVMvalue *_local_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval =
            sequnce_exp_bc_process(GCVM, bcl_vec, fid, line, scope);
    check_and_close_ref(GCVM, retval, scope, env, &info->pts->p, fid, line);
    return retval;
}

static void codegen_local(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);
    pushListItemToQueue(rest->value, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_local_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            info,
            actions);
}

static void codegen_let0(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);
    pushListItemToQueue(rest->value, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_local_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            info,
            actions);
}

typedef struct Let1Context {
    FklValueVector *ss;
} Let1Context;

static void _let1_finalizer(void *d) {
    Let1Context *dd = (Let1Context *)d;
    fklValueVectorDestroy(dd->ss);
}

static FklCgActCtxMethodTable Let1ContextMethodTable = {
    .size = sizeof(Let1Context),
    .finalize = _let1_finalizer,
};

static FklCgActCtx *createLet1CgCtx(FklValueVector *ss) {
    FklCgActCtx *r = createCgActCtx(&Let1ContextMethodTable);
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

static FklCgActCtxMethodTable Do1ContextMethodTable = {
    .size = sizeof(Do1Context),
    .finalize = _do1_finalizer,
};

static FklCgActCtx *createDo1CgCtx(FklUintVector *ss) {
    FklCgActCtx *r = createCgActCtx(&Do1ContextMethodTable);
    FKL_TYPE_CAST(Do1Context *, r->d)->ss = ss;
    return r;
}

static int is_valid_let_arg(const FklVMvalue *node,
        FklVMvalue *const *builtin_pattern_node) {
    return FKL_IS_PAIR(node)          //
        && fklIsList(node)            //
        && fklVMlistLength(node) == 2 //
        && FKL_IS_SYM(FKL_VM_CAR(node));
}

static int is_valid_let_args(const FklVMvalue *sl,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklValueVector *stack,
        FklVMvalue *const *builtin_pattern_node) {
    if (fklIsList(sl)) {
        for (; FKL_IS_PAIR(sl); sl = FKL_VM_CDR(sl)) {
            FklVMvalue *cc = FKL_VM_CAR(sl);
            if (!is_valid_let_arg(cc, builtin_pattern_node))
                return 0;
            FklVMvalue *id = FKL_VM_CAR(cc);
            if (fklIsSymbolDefined(id, scope, env))
                return 0;
            fklValueVectorPushBack2(stack, id);
            fklAddCgDefBySid(id, scope, env);
        }
        return 1;
    } else
        return 0;
}

static int is_valid_letrec_args(const FklVMvalue *sl,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklValueVector *stack,
        FklVMvalue *const *builtin_pattern_node) {
    if (fklIsList(sl)) {
        for (; FKL_IS_PAIR(sl); sl = FKL_VM_CDR(sl)) {
            FklVMvalue *cc = FKL_VM_CAR(sl);
            if (!is_valid_let_arg(cc, builtin_pattern_node))
                return 0;
            FklVMvalue *id = FKL_VM_CAR(cc);
            if (fklIsSymbolDefined(id, scope, env))
                return 0;
            fklValueVectorPushBack2(stack, id);
            fklAddCgPreDefBySid(id, scope, 0, env);
        }
        return 1;
    } else
        return 0;
}

static FklVMvalue *_let1_exp_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    Let1Context *let_ctx = FKL_TYPE_CAST(Let1Context *, data);
    FklValueVector *symbolStack = let_ctx->ss;

    FklVMvalue *retval = *fklValueVectorPopBackNonNull(bcl_vec);
    FklVMvalue **cur_sid = symbolStack->base;
    FklVMvalue **const sid_end = cur_sid + symbolStack->size;
    for (; cur_sid < sid_end; ++cur_sid) {
        FklVMvalue *id = *cur_sid;
        uint32_t idx = fklAddCgDefBySid(id, scope, env)->idx;
        append_pop_loc_ins(GCVM,
                INS_APPEND_FRONT,
                retval,
                idx,
                fid,
                line,
                scope);
    }
    if (!fklValueVectorIsEmpty(bcl_vec)) {
        FklVMvalue *args = *fklValueVectorPopBackNonNull(bcl_vec);
        fklCodeLntReverseConcat(FKL_VM_CO(args), FKL_VM_CO(retval));
    }
    return retval;
}

static FklVMvalue *_let_arg_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklValueVector *bcl_vec = args->bcl_vec;

    if (bcl_vec->size) {
        FklVMvalue *retval = create_0len_bcl(GCVM);
        FklVMvalue **cur_bcl = bcl_vec->base;
        FklVMvalue **const end = cur_bcl + bcl_vec->size;
        for (; cur_bcl < end; ++cur_bcl) {
            FklVMvalue *cur = *cur_bcl;
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }
        bcl_vec->size = 0;
        return retval;
    } else
        return NULL;
}

static FklVMvalue *_letrec_exp_bc_process(const FklCgActCbArgs *args) {
    FklValueVector *bcl_vec = args->bcl_vec;

    FklVMvalue *body = *fklValueVectorPopBackNonNull(bcl_vec);
    FklVMvalue *retval = *fklValueVectorPopBackNonNull(bcl_vec);
    fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(body));
    return retval;
}

static FklVMvalue *_letrec_arg_exp_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    Let1Context *let_ctx = FKL_TYPE_CAST(Let1Context *, data);
    FklValueVector *symbolStack = let_ctx->ss;

    FklVMvalue *retval = create_0len_bcl(GCVM);
    FklVMvalue **cur_sid = symbolStack->base;
    FklVMvalue **const sid_end = cur_sid + symbolStack->size;
    FklVMvalue **cur_bcl = bcl_vec->base;
    for (; cur_sid < sid_end; ++cur_sid, ++cur_bcl) {
        FklVMvalue *value_bc = *cur_bcl;
        FklVMvalue *id = *cur_sid;
        uint32_t idx = fklAddCgDefBySid(id, scope, env)->idx;
        fklResolveCgPreDef(id, scope, env, &info->pts->p);
        fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(value_bc));
        append_pop_loc_ins(GCVM,
                INS_APPEND_BACK,
                retval,
                idx,
                fid,
                line,
                scope);
    }
    bcl_vec->size = 0;
    return retval;
}

static void codegen_let1(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    FklValueVector *symStack = fklValueVectorCreate(8);
    const FklPmatchRes *first = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(first->value)) {
        fklValueVectorDestroy(symStack);
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    const FklPmatchRes *item = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);

    FklVMvalue *argl = item ? item->value : NULL;
    uint32_t cs = enter_new_scope(scope, env);

    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);

    fklAddCgDefBySid(first->value, cs, env);
    fklValueVectorPushBack2(symStack, first->value);

    CgExpQueue *valueQueue = cgExpQueueCreate();
    cgExpQueuePush(valueQueue, value);

    if (argl) {
        if (!is_valid_let_args(argl, env, cs, symStack, builtin_pattern_node)) {
            cgExpQueueDestroy(valueQueue);
            fklValueVectorDestroy(symStack);
            error_state->error = make_syntax_error(GCVM, orig->value);
            error_state->line = CURLINE(orig->container);
            return;
        }
        for (FklVMvalue *cur = argl; FKL_IS_PAIR(cur); cur = FKL_VM_CDR(cur))
            cgExpQueuePush2(valueQueue,
                    (FklPmatchRes){
                        .value = cadr(FKL_VM_CAR(cur)),
                        .container = cdr(FKL_VM_CAR(cur)),
                    });
    }

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    FklCgAct *let1Action = create_cg_action(_let1_exp_bc_process,
            createLet1CgCtx(symStack),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            info);

    fklCgActVectorPushBack2(actions, let1Action);

    FklCgAct *restAction = create_cg_action(_local_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->value),
            let1Action,
            info);
    fklCgActVectorPushBack2(actions, restAction);

    FklCgAct *argAction = create_cg_action(_let_arg_exp_bc_process,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(valueQueue),
            scope,
            macro_scope,
            env,
            CURLINE(first->value),
            let1Action,
            info);
    fklCgActVectorPushBack2(actions, argAction);
}

static void codegen_let81(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklVMvalue *letHead = add_symbol_cstr(ctx, "let");
    const FklPmatchRes *orig = args->orig;

    FklVMvalue *first_name = cadr(orig->value);
    FKL_VM_CDR(first_name) = FKL_VM_NIL;

    const FklPmatchRes *argl = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);

    FklVMvalue *restLet8 = fklCreateVMvaluePair(GCVM, argl->value, rest->value);
    FKL_VM_CDR(orig->value) = restLet8;

    ListElm a[3] = {
        { .v = letHead, .line = CURLINE(letHead) },
        { .v = first_name, .line = CURLINE(first_name) },
        { .v = orig->value, .line = CURLINE(orig->container) },
    };
    letHead = create_nast_list(a, 3, CURLINE(orig->value), GCVM, NULL);
    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush2(queue,
            (FklPmatchRes){
                .value = letHead,
                .container = orig->value,
            });

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_default_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            info,
            actions);
}

static void codegen_letrec(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    FklValueVector *symStack = fklValueVectorCreate(8);
    const FklPmatchRes *first = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(first->value)) {
        fklValueVectorDestroy(symStack);
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    const FklPmatchRes *argl = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);
    uint32_t cs = enter_new_scope(scope, env);

    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);

    fklAddCgPreDefBySid(first->value, cs, 0, env);
    fklValueVectorPushBack2(symStack, first->value);

    if (!is_valid_letrec_args(argl->value,
                env,
                cs,
                symStack,
                builtin_pattern_node)) {
        fklValueVectorDestroy(symStack);
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }

    CgExpQueue *valueQueue = cgExpQueueCreate();
    cgExpQueuePush(valueQueue, value);
    for (FklVMvalue *cur = argl->value; FKL_IS_PAIR(cur);
            cur = FKL_VM_CDR(cur)) {
        cgExpQueuePush2(valueQueue,
                (FklPmatchRes){
                    .value = cadr(FKL_VM_CAR(cur)),
                    .container = cdr(FKL_VM_CAR(cur)),
                });
    }

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    FklCgAct *let1Action = create_cg_action(_letrec_exp_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            info);

    fklCgActVectorPushBack2(actions, let1Action);

    FklCgAct *restAction = create_cg_action(_local_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            let1Action,
            info);
    fklCgActVectorPushBack2(actions, restAction);

    FklCgAct *argAction = create_cg_action(_letrec_arg_exp_bc_process,
            createLet1CgCtx(symStack),
            createMustHasRetvalQueueNextExpression(valueQueue),
            cs,
            cms,
            env,
            CURLINE(orig->container),
            let1Action,
            info);
    fklCgActVectorPushBack2(actions, argAction);
}

static inline void insert_jmp_if_true_and_jmp_back_between(FklVM *exe,
        FklVMvalue *cond,
        FklVMvalue *rest,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklByteCode *cond_bc = &FKL_VM_CO(cond)->bc;
    FklByteCode *rest_bc = &FKL_VM_CO(rest)->bc;
    uint32_t jmp_back_ins_len = 3;

    uint64_t cond_len = cond_bc->len;
    uint64_t rest_len = rest_bc->len;

    append_jmp_ins(exe,
            INS_APPEND_BACK,
            cond,
            rest_bc->len + jmp_back_ins_len,
            JMP_IF_TRUE,
            JMP_FORWARD,
            fid,
            line,
            scope);

    append_jmp_ins(exe,
            INS_APPEND_BACK,
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

        append_jmp_ins(exe,
                INS_APPEND_BACK,
                cond,
                rest_bc->len + jmp_back_ins_len,
                JMP_IF_TRUE,
                JMP_FORWARD,
                fid,
                line,
                scope);

        append_jmp_ins(exe,
                INS_APPEND_BACK,
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

static FklVMvalue *_do0_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *rest = *fklValueVectorPopBackNonNull(bcl_vec);
    FklVMvalue *value = *fklValueVectorPopBackNonNull(bcl_vec);
    FklVMvalue *cond = *fklValueVectorPopBackNonNull(bcl_vec);

    FklInstruction pop = create_op_ins(FKL_OP_DROP);

    fklByteCodeLntInsertFrontIns(&pop, FKL_VM_CO(rest), fid, line, scope);
    insert_jmp_if_true_and_jmp_back_between(GCVM, cond, rest, fid, line, scope);

    fklCodeLntConcat(FKL_VM_CO(cond), FKL_VM_CO(rest));

    if (FKL_VM_CO(value)->bc.len)
        fklByteCodeLntPushBackIns(FKL_VM_CO(cond), &pop, fid, line, scope);
    fklCodeLntReverseConcat(FKL_VM_CO(cond), FKL_VM_CO(value));

    return value;
}

static FklVMvalue *_do_rest_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    if (bcl_vec->size) {
        FklInstruction pop = create_op_ins(FKL_OP_DROP);
        FklVMvalue *r = sequnce_exp_bc_process(GCVM, bcl_vec, fid, line, scope);
        fklByteCodeLntPushBackIns(FKL_VM_CO(r), &pop, fid, line, scope);
        return r;
    }
    return create_0len_bcl(GCVM);
}

static void codegen_do0(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *cond = fklPmatchHashMapGet2(ht, ctx->builtin_sym_cond);
    const FklPmatchRes *item = fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);

    FklCgAct *do0Action = create_cg_action(_do0_exp_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(cond->container),
            NULL,
            info);
    fklCgActVectorPushBack2(actions, do0Action);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_do_rest_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(cond->container),
            info,
            actions);

    if (item) {
        CgExpQueue *vQueue = cgExpQueueCreate();
        cgExpQueuePush(vQueue, item);
        FklCgAct *do0VAction = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                createMustHasRetvalQueueNextExpression(vQueue),
                cs,
                cms,
                env,
                CURLINE(orig->container),
                do0Action,
                info);
        fklCgActVectorPushBack2(actions, do0VAction);
    } else {
        FklVMvalue *v = create_0len_bcl(GCVM);
        FklCgAct *action = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                NULL,
                cs,
                cms,
                env,
                CURLINE(orig->container),
                do0Action,
                info);
        fklValueVectorPushBack2(&action->bcl_vector, v);
        fklCgActVectorPushBack2(actions, action);
    }
    CgExpQueue *cQueue = cgExpQueueCreate();
    cgExpQueuePush(cQueue, cond);
    FklCgAct *do0CAction = create_cg_action(_default_bc_process,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(cQueue),
            cs,
            cms,
            env,
            CURLINE(orig->container),
            do0Action,
            info);
    fklCgActVectorPushBack2(actions, do0CAction);
}

static inline int is_valid_do_var_bind(const FklVMvalue *list,
        FklPmatchRes *next,
        FklVMvalue *const *builtin_pattern_node) {
    if (!fklIsList(list))
        return 0;
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
    }
    return 1;
}

static inline int is_valid_do_bind_list(const FklVMvalue *sl,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklUintVector *stack,
        FklUintVector *nstack,
        CgExpQueue *valueQueue,
        CgExpQueue *nextQueue,
        FklVMvalue *const *builtin_pattern_node) {
    if (fklIsList(sl)) {
        for (; FKL_IS_PAIR(sl); sl = FKL_VM_CDR(sl)) {
            FklVMvalue *cc = FKL_VM_CAR(sl);
            FklPmatchRes nextExp = { 0 };
            if (!is_valid_do_var_bind(cc, &nextExp, builtin_pattern_node))
                return 0;
            FklVMvalue *id = FKL_VM_CAR(cc);
            if (fklIsSymbolDefined(id, scope, env))
                return 0;
            uint32_t idx = fklAddCgDefBySid(id, scope, env)->idx;
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

static FklVMvalue *_do1_init_val_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *ret = create_0len_bcl(GCVM);
    Do1Context *let_ctx = FKL_TYPE_CAST(Do1Context *, data);
    FklUintVector *ss = let_ctx->ss;

    FklInstruction pop = create_op_ins(FKL_OP_DROP);

    uint64_t *idxbase = ss->base;
    FklVMvalue **bclBase = bcl_vec->base;
    uint32_t top = bcl_vec->size;
    for (uint32_t i = 0; i < top; i++) {
        uint32_t idx = idxbase[i];
        FklVMvalue *curBcl = bclBase[i];
        append_put_loc_ins(GCVM,
                INS_APPEND_BACK,
                curBcl,
                idx,
                fid,
                line,
                scope);
        fklByteCodeLntPushBackIns(FKL_VM_CO(curBcl), &pop, fid, line, scope);
        fklCodeLntConcat(FKL_VM_CO(ret), FKL_VM_CO(curBcl));
    }
    return ret;
}

static FklVMvalue *_do1_next_val_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    Do1Context *let_ctx = FKL_TYPE_CAST(Do1Context *, data);
    FklUintVector *ss = let_ctx->ss;

    if (bcl_vec->size) {
        FklVMvalue *ret = create_0len_bcl(GCVM);
        FklInstruction pop = create_op_ins(FKL_OP_DROP);

        uint64_t *idxbase = ss->base;
        FklVMvalue **bclBase = bcl_vec->base;
        uint32_t top = bcl_vec->size;
        for (uint32_t i = 0; i < top; i++) {
            uint32_t idx = idxbase[i];
            FklVMvalue *curBcl = bclBase[i];
            append_put_loc_ins(GCVM,
                    INS_APPEND_BACK,
                    curBcl,
                    idx,
                    fid,
                    line,
                    scope);
            fklByteCodeLntPushBackIns(FKL_VM_CO(curBcl),
                    &pop,
                    fid,
                    line,
                    scope);
            fklCodeLntConcat(FKL_VM_CO(ret), FKL_VM_CO(curBcl));
        }
        return ret;
    }
    return NULL;
}

static FklVMvalue *_do1_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *next =
            bcl_vec->size == 5 ? *fklValueVectorPopBackNonNull(bcl_vec) : NULL;
    FklVMvalue *rest = *fklValueVectorPopBackNonNull(bcl_vec);
    FklVMvalue *value = *fklValueVectorPopBackNonNull(bcl_vec);
    FklVMvalue *cond = *fklValueVectorPopBackNonNull(bcl_vec);
    FklVMvalue *init = *fklValueVectorPopBackNonNull(bcl_vec);

    FklInstruction pop = create_op_ins(FKL_OP_DROP);

    fklByteCodeLntInsertFrontIns(&pop, FKL_VM_CO(rest), fid, line, scope);
    if (next) {
        fklCodeLntConcat(FKL_VM_CO(rest), FKL_VM_CO(next));
    }

    insert_jmp_if_true_and_jmp_back_between(GCVM, cond, rest, fid, line, scope);

    fklCodeLntConcat(FKL_VM_CO(cond), FKL_VM_CO(rest));

    if (FKL_VM_CO(value)->bc.len)
        fklByteCodeLntPushBackIns(FKL_VM_CO(cond), &pop, fid, line, scope);
    fklCodeLntReverseConcat(FKL_VM_CO(cond), FKL_VM_CO(value));

    fklCodeLntReverseConcat(FKL_VM_CO(init), FKL_VM_CO(value));
    check_and_close_ref(GCVM, value, scope, env, &info->pts->p, fid, line);
    return value;
}

static void codegen_do1(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    FklVMvalue *bindlist = cadr(orig->value);

    const FklPmatchRes *cond = fklPmatchHashMapGet2(ht, ctx->builtin_sym_cond);
    const FklPmatchRes *item = fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);

    FklUintVector *symStack = fklUintVectorCreate(4);
    FklUintVector *nextSymStack = fklUintVectorCreate(4);

    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);

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

        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }

    FklCgAct *do1Action = create_cg_action(_do1_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            info);
    fklCgActVectorPushBack2(actions, do1Action);

    FklCgAct *do1NextValAction = create_cg_action(_do1_next_val_bc_process,
            createDo1CgCtx(nextSymStack),
            createMustHasRetvalQueueNextExpression(nextValueQueue),
            cs,
            cms,
            env,
            CURLINE(orig->container),
            do1Action,
            info);

    fklCgActVectorPushBack2(actions, do1NextValAction);

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    FklCgAct *do1RestAction = create_cg_action(_do_rest_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(orig->container),
            do1Action,
            info);
    fklCgActVectorPushBack2(actions, do1RestAction);

    if (item) {
        CgExpQueue *vQueue = cgExpQueueCreate();
        cgExpQueuePush(vQueue, item);
        FklCgAct *do1VAction = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                createMustHasRetvalQueueNextExpression(vQueue),
                cs,
                cms,
                env,
                CURLINE(orig->container),
                do1Action,
                info);
        fklCgActVectorPushBack2(actions, do1VAction);
    } else {
        FklVMvalue *v = create_0len_bcl(GCVM);
        FklCgAct *action = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                NULL,
                cs,
                cms,
                env,
                CURLINE(orig->container),
                do1Action,
                info);
        fklValueVectorPushBack2(&action->bcl_vector, v);
        fklCgActVectorPushBack2(actions, action);
    }

    CgExpQueue *cQueue = cgExpQueueCreate();
    cgExpQueuePush(cQueue, cond);
    FklCgAct *do1CAction = create_cg_action(_default_bc_process,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(cQueue),
            cs,
            cms,
            env,
            CURLINE(orig->container),
            do1Action,
            info);
    fklCgActVectorPushBack2(actions, do1CAction);

    FklCgAct *do1InitValAction = create_cg_action(_do1_init_val_bc_process,
            createDo1CgCtx(symStack),
            createMustHasRetvalQueueNextExpression(valueQueue),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            do1Action,
            info);
    fklCgActVectorPushBack2(actions, do1InitValAction);
}

static inline FklVMvalue *
get_sid_with_idx(FklCgEnvScope *sc, uint32_t idx, FklCgCtx *ctx) {
    for (FklSymDefHashMapNode *l = sc->defs.first; l; l = l->next) {
        if (l->v.idx == idx)
            return l->k.id;
    }
    return 0;
}

static inline FklVMvalue *
get_sid_with_ref_idx(FklSymDefHashMap *refs, uint32_t idx, FklCgCtx *ctx) {
    for (FklSymDefHashMapNode *l = refs->first; l; l = l->next) {
        if (l->v.idx == idx)
            return l->k.id;
    }
    return 0;
}

static inline FklVMvalue *process_set_var(FklValueVector *stack,
        FklVMvalueCgInfo *info,
        FklCgCtx *ctx,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalue *fid,
        uint32_t line) {
    if (stack->size >= 2) {
        FklVMvalue *cur = *fklValueVectorPopBackNonNull(stack);
        FklVMvalue *popVar = *fklValueVectorPopBackNonNull(stack);
        const FklInstruction *cur_ins = &FKL_VM_CO(cur)->bc.code[0];
        if (fklIsPushProcIns(cur_ins)) {
            const FklInstruction *popVar_ins = &FKL_VM_CO(popVar)->bc.code[0];
            if (fklIsPutLocIns(popVar_ins)) {
                FklInstructionArg arg;
                fklGetInsOpArg(cur_ins, &arg);
                uint64_t prototypeId = arg.ux;

                fklGetInsOpArg(popVar_ins, &arg);
                uint64_t idx = arg.ux;

                if (!info->pts->p.pa[prototypeId].name)
                    info->pts->p.pa[prototypeId].name =
                            get_sid_with_idx(&env->scopes[scope - 1], idx, ctx);
            } else if (fklIsPutVarRefIns(popVar_ins)) {
                FklInstructionArg arg;
                fklGetInsOpArg(cur_ins, &arg);
                uint64_t prototypeId = arg.ux;

                fklGetInsOpArg(popVar_ins, &arg);
                uint64_t idx = arg.ux;
                if (!info->pts->p.pa[prototypeId].name)
                    info->pts->p.pa[prototypeId].name =
                            get_sid_with_ref_idx(&env->refs, idx, ctx);
            }
        }
        fklCodeLntReverseConcat(FKL_VM_CO(cur), FKL_VM_CO(popVar));
        return popVar;
    } else {
        FklVMvalue *popVar = *fklValueVectorPopBackNonNull(stack);
        FklInstruction pushNil = create_op_ins(FKL_OP_PUSH_NIL);
        fklByteCodeLntInsertFrontIns(&pushNil,
                FKL_VM_CO(popVar),
                fid,
                line,
                scope);
        return popVar;
    }
}

typedef struct {
    FklVMvalue *id;
    const FklVMvalue *container;
    uint32_t scope;
    uint32_t line;
} DefineVarContext;

static const FklCgActCtxMethodTable DefineVarContextMethodTable = {
    .size = sizeof(DefineVarContext),
};

static inline FklCgActCtx *
create_def_var_context(const FklPmatchRes *id, uint32_t scope, uint32_t line) {
    FklCgActCtx *r = createCgActCtx(&DefineVarContextMethodTable);
    DefineVarContext *ctx = FKL_TYPE_CAST(DefineVarContext *, r->d);
    ctx->id = id->value;
    ctx->container = id->container;
    ctx->scope = scope;
    ctx->line = line;
    return r;
}

static FklVMvalue *_def_var_exp_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    DefineVarContext *var_ctx = FKL_TYPE_CAST(DefineVarContext *, data);
    uint32_t idx = fklAddCgDefBySid(var_ctx->id, var_ctx->scope, env)->idx;
    fklResolveCgPreDef(var_ctx->id, var_ctx->scope, env, &info->pts->p);
    fklValueVectorInsertFront2(bcl_vec,
            append_put_loc_ins(GCVM,
                    INS_APPEND_BACK,
                    NULL,
                    idx,
                    info->fid,
                    var_ctx->line,
                    scope));
    return process_set_var(bcl_vec, info, ctx, env, scope, fid, line);
}

static FklVMvalue *_set_var_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    return process_set_var(bcl_vec, info, ctx, env, scope, fid, line);
}

static FklVMvalue *_lambda_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = NULL;
    FklInstruction ret = create_op_ins(FKL_OP_RET);
    if (bcl_vec->size > 1) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        retval = create_0len_bcl(GCVM);
        size_t top = bcl_vec->size;
        for (size_t i = 1; i < top; i++) {
            FklVMvalue *cur = bcl_vec->base[i];
            if (FKL_VM_CO(cur)->bc.len) {
                fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
                if (i < top - 1)
                    fklByteCodeLntPushBackIns(FKL_VM_CO(retval),
                            &drop,
                            fid,
                            line,
                            scope);
            }
        }
        fklByteCodeLntPushBackIns(FKL_VM_CO(retval), &ret, fid, line, scope);
    } else {
        retval = fklCreateVMvalueCodeObjExt(GCVM,
                create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
        fklByteCodeLntPushBackIns(FKL_VM_CO(retval), &ret, fid, line, scope);
    }
    fklCodeLntReverseConcat(FKL_VM_CO(bcl_vec->base[0]), FKL_VM_CO(retval));
    bcl_vec->size = 0;
    fklScanAndSetTailCall(&FKL_VM_CO(retval)->bc);
    FklFuncPrototypes *pts = &info->pts->p;
    fklUpdatePrototype(pts, env);
    append_push_proc_ins(GCVM,
            INS_APPEND_FRONT,
            retval,
            env->prototypeId,
            FKL_VM_CO(retval)->bc.len,
            fid,
            line,
            scope);
    return retval;
}

static inline FklVMvalue *processArgs(FklVM *exe,
        FklVMvalue *args,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info) {
    FklVMvalue *retval = create_0len_bcl(exe);

    uint32_t arg_count = 0;
    for (; FKL_IS_PAIR(args); args = FKL_VM_CDR(args)) {
        FklVMvalue *cur = FKL_VM_CAR(args);
        if (!FKL_IS_SYM(cur)) {
            return NULL;
        }
        if (fklIsSymbolDefined(cur, 1, env)) {
            return NULL;
        }
        if (arg_count > UINT16_MAX) {
            return NULL;
        }
        fklAddCgDefBySid(cur, 1, env);
        ++arg_count;
    }
    if (args != FKL_VM_NIL && !FKL_IS_SYM(args)) {
        return NULL;
    }

    int rest_list = 0;
    if (FKL_IS_SYM(args)) {
        if (fklIsSymbolDefined(args, 1, env)) {
            return NULL;
        }
        rest_list = 1;
        fklAddCgDefBySid(args, 1, env);
    }
    FklInstruction check_arg = {
        .op = FKL_OP_CHECK_ARG,
        .ai = rest_list,
        .bu = arg_count,
    };
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval),
            &check_arg,
            info->fid,
            CURLINE(args),
            1);
    return retval;
}

static inline FklVMvalue *processArgsInStack(FklVM *exe,
        FklValueVector *stack,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info,
        uint64_t curline) {
    FklVMvalue *retval = create_0len_bcl(exe);
    uint32_t top = stack->size;
    FklVMvalue **base = stack->base;
    for (uint32_t i = 0; i < top; i++) {
        FklVMvalue *curId = base[i];

        fklAddCgDefBySid(curId, 1, env);
    }
    FklInstruction check_arg = { .op = FKL_OP_CHECK_ARG, .ai = 0, .bu = top };
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval),
            &check_arg,
            info->fid,
            curline,
            1);
    return retval;
}

static FklVMvalue *_named_let_set_var_exp_bc_process(
        const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *pop_var = NULL;
    if (bcl_vec->size >= 2) {
        FklVMvalue *cur = *fklValueVectorPopBackNonNull(bcl_vec);
        pop_var = *fklValueVectorPopBackNonNull(bcl_vec);
        fklCodeLntReverseConcat(FKL_VM_CO(cur), FKL_VM_CO(pop_var));
    } else {
        FklVMvalue *cur = *fklValueVectorPopBackNonNull(bcl_vec);
        pop_var = cur;
        FklInstruction pushNil = create_op_ins(FKL_OP_PUSH_NIL);
        fklByteCodeLntInsertFrontIns(&pushNil,
                FKL_VM_CO(pop_var),
                fid,
                line,
                scope);
    }
    check_and_close_ref(GCVM, pop_var, scope, env, &info->pts->p, fid, line);
    return pop_var;
}

static void codegen_named_let0(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg0);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_funcall_exp_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            info,
            actions);

    uint32_t idx = fklAddCgDefBySid(name->value, cs, env)->idx;

    fklReplacementHashMapAdd2(cms->replacements,
            add_symbol_cstr(ctx, "*func*"),
            name->value);

    FklCgAct *action = create_cg_action(_named_let_set_var_exp_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            info);
    fklValueVectorPushBack2(&action->bcl_vector,
            append_put_loc_ins(GCVM,
                    INS_APPEND_BACK,
                    NULL,
                    idx,
                    info->fid,
                    CURLINE(orig->container),
                    scope));
    fklCgActVectorPushBack2(actions, action);

    FklVMvalueCgEnv *lambda_env = fklCreateVMvalueCgEnv(ctx, env, cs, cms);
    FklVMvalue *argsNode = caddr(orig->value);
    FklVMvalue *argBc = processArgs(GCVM, argsNode, lambda_env, info);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);

    create_and_insert_to_pool(info,
            env->prototypeId,
            lambda_env,
            name->value,
            CURLINE(orig->container));

    FklCgAct *action1 = create_cg_action(_lambda_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            lambda_env->macros,
            lambda_env,
            CURLINE(rest->container),
            NULL,
            info);
    fklValueVectorPushBack2(&action1->bcl_vector, argBc);
    fklCgActVectorPushBack2(actions, action1);
}

static void codegen_named_let1(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg0);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    FklValueVector *symStack = fklValueVectorCreate(8);
    const FklPmatchRes *first = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(first->value)) {
        fklValueVectorDestroy(symStack);
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    const FklPmatchRes *argl = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);

    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);

    FklVMvalueCgEnv *lambda_env = fklCreateVMvalueCgEnv(ctx, env, cs, cms);
    fklAddCgDefBySid(first->value, 1, lambda_env);
    fklValueVectorPushBack2(symStack, first->value);

    if (!is_valid_let_args(argl->value,
                lambda_env,
                1,
                symStack,
                builtin_pattern_node)) {
        fklValueVectorDestroy(symStack);
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }

    CgExpQueue *valueQueue = cgExpQueueCreate();

    cgExpQueuePush(valueQueue, value);
    for (FklVMvalue *cur = argl->value; FKL_IS_PAIR(cur); cur = FKL_VM_CDR(cur))
        cgExpQueuePush2(valueQueue,
                (FklPmatchRes){
                    .value = cadr(FKL_VM_CAR(cur)),
                    .container = cdr(FKL_VM_CAR(cur)),
                });

    FklCgAct *funcallAction = create_cg_action(_funcall_exp_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            info);
    fklCgActVectorPushBack2(actions, funcallAction);

    uint32_t idx = fklAddCgDefBySid(name->value, cs, env)->idx;

    fklReplacementHashMapAdd2(cms->replacements,
            add_symbol_cstr(ctx, "*func*"),
            name->value);
    fklCgActVectorPushBack2(actions,
            create_cg_action(_let_arg_exp_bc_process,
                    createDefaultStackContext(),
                    createMustHasRetvalQueueNextExpression(valueQueue),
                    scope,
                    macro_scope,
                    env,
                    CURLINE(first->container),
                    funcallAction,
                    info));

    FklCgAct *action = create_cg_action(_named_let_set_var_exp_bc_process,
            createDefaultStackContext(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            funcallAction,
            info);
    fklValueVectorPushBack2(&action->bcl_vector,
            append_put_loc_ins(GCVM,
                    INS_APPEND_BACK,
                    NULL,
                    idx,
                    info->fid,
                    CURLINE(orig->container),
                    scope));

    fklCgActVectorPushBack2(actions, action);

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);

    FklVMvalue *argBc = processArgsInStack(GCVM,
            symStack,
            lambda_env,
            info,
            CURLINE(orig->container));

    fklValueVectorDestroy(symStack);
    create_and_insert_to_pool(info,
            env->prototypeId,
            lambda_env,
            name->value,
            CURLINE(orig->container));

    FklCgAct *action1 = create_cg_action(_lambda_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            lambda_env->macros,
            lambda_env,
            CURLINE(rest->value),
            NULL,
            info);
    fklValueVectorPushBack2(&action1->bcl_vector, argBc);

    fklCgActVectorPushBack2(actions, action1);
}

static FklVMvalue *_and_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    if (bcl_vec->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklVMvalue *retval = create_0len_bcl(GCVM);
        while (!fklValueVectorIsEmpty(bcl_vec)) {
            if (FKL_VM_CO(retval)->bc.len) {
                fklByteCodeLntInsertFrontIns(&drop,
                        FKL_VM_CO(retval),
                        fid,
                        line,
                        scope);
                append_jmp_ins(GCVM,
                        INS_APPEND_FRONT,
                        retval,
                        FKL_VM_CO(retval)->bc.len,
                        JMP_IF_FALSE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
            }
            FklVMvalue *cur = *fklValueVectorPopBackNonNull(bcl_vec);
            fklCodeLntReverseConcat(FKL_VM_CO(cur), FKL_VM_CO(retval));
        }
        return retval;
    } else
        return append_push_i24_ins(GCVM,
                INS_APPEND_BACK,
                NULL,
                1,
                info,
                fid,
                line,
                scope);
}

static void codegen_and(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_and_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            info,
            actions);
}

static FklVMvalue *_or_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    if (bcl_vec->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklVMvalue *retval = create_0len_bcl(GCVM);
        while (!fklValueVectorIsEmpty(bcl_vec)) {
            if (FKL_VM_CO(retval)->bc.len) {
                fklByteCodeLntInsertFrontIns(&drop,
                        FKL_VM_CO(retval),
                        fid,
                        line,
                        scope);
                append_jmp_ins(GCVM,
                        INS_APPEND_FRONT,
                        retval,
                        FKL_VM_CO(retval)->bc.len,
                        JMP_IF_TRUE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
            }
            FklVMvalue *cur = *fklValueVectorPopBackNonNull(bcl_vec);
            fklCodeLntReverseConcat(FKL_VM_CO(cur), FKL_VM_CO(retval));
        }
        return retval;
    } else
        return fklCreateVMvalueCodeObjExt(GCVM,
                create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static void codegen_or(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    CgExpQueue *queue = cgExpQueueCreate();
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);
    pushListItemToQueue(rest->value, queue, NULL);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_or_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            info,
            actions);
}

int fklIsReplacementDefined(FklVMvalue *id, FklVMvalueCgEnv *env) {
    return fklReplacementHashMapGet2(env->macros->replacements, id) != NULL;
}

FklVMvalue *fklGetReplacement(FklVMvalue *id,
        const FklReplacementHashMap *rep) {
    FklVMvalue **pn = fklReplacementHashMapGet2(rep, id);
    return pn ? *pn : NULL;
}

static void codegen_lambda(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    const FklPmatchRes *orig = args->orig;
    FklCgErrorState *error_state = args->err_state;

    const FklPmatchRes *argl = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    FklVMvalueCgEnv *lambda_env =
            fklCreateVMvalueCgEnv(ctx, env, scope, macro_scope);
    FklVMvalue *argsBc = processArgs(GCVM, argl->value, lambda_env, info);
    if (!argsBc) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    create_and_insert_to_pool(info,
            env->prototypeId,
            lambda_env,
            0,
            CURLINE(orig->container));
    FklCgAct *action = create_cg_action(_lambda_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            lambda_env->macros,
            lambda_env,
            CURLINE(rest->container),
            NULL,
            info);
    fklValueVectorPushBack2(&action->bcl_vector, argsBc);
    fklCgActVectorPushBack2(actions, action);
}

static inline int
is_constant_defined(FklVMvalue *id, uint32_t scope, FklVMvalueCgEnv *env) {
    FklSymDefHashMapElm *def = fklGetCgDefByIdInScope(id, scope, env->scopes);
    return def && def->v.isConst;
}

static inline int
is_variable_defined(FklVMvalue *id, uint32_t scope, FklVMvalueCgEnv *env) {
    FklSymDefHashMapElm *def = fklGetCgDefByIdInScope(id, scope, env->scopes);
    return def != NULL;
}

static void codegen_define(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    const FklPmatchRes *orig = args->orig;
    FklCgErrorState *error_state = args->err_state;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->error = make_assign_const_error(GCVM, name->value);
        error_state->line = CURLINE(name->container);
        return;
    }
    if (!is_variable_defined(name->value, scope, env))
        fklAddCgPreDefBySid(name->value, scope, 0, env);

    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, value);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_def_var_exp_bc_process,
            create_def_var_context(name, scope, CURLINE(orig->container)),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            info,
            actions);
}

static inline FklUnReSymbolRef *get_resolvable_assign_ref(FklVMvalue *id,
        uint32_t scope,
        FklVMvalueCgEnv *env) {
    FklUnReSymbolRef *urefs = env->uref.base;
    uint32_t top = env->uref.size;
    for (uint32_t i = 0; i < top; i++) {
        FklUnReSymbolRef *cur = &urefs[i];
        if (cur->id == id && cur->scope == scope && cur->assign)
            return cur;
    }
    return NULL;
}

static FklVMvalue *_def_const_exp_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;
    FklCgErrorState *error_state = args->error_state;

    DefineVarContext *var_ctx = FKL_TYPE_CAST(DefineVarContext *, data);
    FklUnReSymbolRef *assign_uref =
            get_resolvable_assign_ref(var_ctx->id, var_ctx->scope, env);
    if (assign_uref) {
        error_state->error = make_assign_const_error(GCVM, var_ctx->id);
        error_state->line = CURLINE(var_ctx->container);
        error_state->fid = assign_uref->fid;
        return NULL;
    }
    FklSymDef *def = fklAddCgDefBySid(var_ctx->id, var_ctx->scope, env);
    def->isConst = 1;
    uint32_t idx = def->idx;
    fklResolveCgPreDef(var_ctx->id, var_ctx->scope, env, &info->pts->p);
    fklValueVectorInsertFront2(bcl_vec,
            append_put_loc_ins(GCVM,
                    INS_APPEND_BACK,
                    NULL,
                    idx,
                    info->fid,
                    var_ctx->line,
                    var_ctx->scope));
    return process_set_var(bcl_vec, info, ctx, env, scope, fid, line);
}

static void codegen_defconst(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->error = make_assign_const_error(GCVM, name->value);
        error_state->line = CURLINE(name->container);
        return;
    }
    if (is_variable_defined(name->value, scope, env)) {
        error_state->error = make_redefine_const_error(GCVM, name->value);
        error_state->line = CURLINE(name->container);
        return;
    } else
        fklAddCgPreDefBySid(name->value, scope, 1, env);

    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, value);
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_def_const_exp_bc_process,
            create_def_var_context(name, scope, CURLINE(orig->container)),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            info,
            actions);
}

static void codegen_defun(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *argl = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->error = make_assign_const_error(GCVM, name->value);
        error_state->line = CURLINE(name->container);
        return;
    }

    FklVMvalueCgEnv *lambda_env =
            fklCreateVMvalueCgEnv(ctx, env, scope, macro_scope);
    FklVMvalue *argsBc = processArgs(GCVM, argl->value, lambda_env, info);
    if (!argsBc) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }

    if (!is_variable_defined(name->value, scope, env))
        fklAddCgPreDefBySid(name->value, scope, 0, env);

    FklCgAct *prevAction = create_cg_action(_def_var_exp_bc_process,
            create_def_var_context(name, scope, CURLINE(orig->container)),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            NULL,
            info);
    fklCgActVectorPushBack2(actions, prevAction);

    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    create_and_insert_to_pool(info,
            env->prototypeId,
            lambda_env,
            name->value,
            CURLINE(orig->container));

    fklReplacementHashMapAdd2(lambda_env->macros->replacements,
            add_symbol_cstr(ctx, "*func*"),
            name->value);

    FklCgAct *cur = create_cg_action(_lambda_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            lambda_env->macros,
            lambda_env,
            CURLINE(rest->container),
            prevAction,
            info);
    fklValueVectorPushBack2(&cur->bcl_vector, argsBc);
    fklCgActVectorPushBack2(actions, cur);
}

static void codegen_defun_const(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *argl = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->error = make_assign_const_error(GCVM, name->value);
        error_state->line = CURLINE(name->container);
        return;
    }
    if (is_variable_defined(name->value, scope, env)) {
        error_state->error = make_redefine_const_error(GCVM, name->value);
        error_state->line = CURLINE(name->container);
        return;
    }

    FklVMvalueCgEnv *lambda_env =
            fklCreateVMvalueCgEnv(ctx, env, scope, macro_scope);
    FklVMvalue *argsBc = processArgs(GCVM, argl->value, lambda_env, info);
    if (!argsBc) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }

    FklCgAct *prevAction = create_cg_action(_def_const_exp_bc_process,
            create_def_var_context(name, scope, CURLINE(orig->container)),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            NULL,
            info);
    fklCgActVectorPushBack2(actions, prevAction);

    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);
    create_and_insert_to_pool(info,
            env->prototypeId,
            lambda_env,
            name->value,
            CURLINE(orig->container));
    fklReplacementHashMapAdd2(lambda_env->macros->replacements,
            add_symbol_cstr(ctx, "*func*"),
            name->value);
    FklCgAct *cur = create_cg_action(_lambda_exp_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            lambda_env->macros,
            lambda_env,
            CURLINE(rest->container),
            prevAction,
            info);
    fklValueVectorPushBack2(&cur->bcl_vector, argsBc);
    fklCgActVectorPushBack2(actions, cur);
}

static void codegen_setq(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, value);
    FklSymDefHashMapElm *def =
            fklFindSymbolDefByIdAndScope(name->value, scope, env->scopes);
    FklCgAct *cur = create_cg_action(_set_var_exp_bc_process,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            NULL,
            info);
    if (def) {
        if (def->v.isConst) {
            error_state->error = make_assign_const_error(GCVM, name->value);
            error_state->line = CURLINE(name->container);
            return;
        }
        fklValueVectorPushBack2(&cur->bcl_vector,
                append_put_loc_ins(GCVM,
                        INS_APPEND_BACK,
                        NULL,
                        def->v.idx,
                        info->fid,
                        CURLINE(orig->container),
                        scope));
    } else {
        FklSymDefHashMapElm *def = fklAddCgRefBySid(name->value,
                env,
                info->fid,
                CURLINE(orig->container),
                1);
        if (def->v.isConst) {
            error_state->error = make_assign_const_error(GCVM, name->value);
            error_state->line = CURLINE(name->container);
            return;
        }
        fklValueVectorPushBack2(&cur->bcl_vector,
                append_put_var_ref_ins(GCVM,
                        INS_APPEND_BACK,
                        NULL,
                        def->v.idx,
                        info->fid,
                        CURLINE(orig->container),
                        scope));
    }
    fklCgActVectorPushBack2(actions, cur);
}

static inline void push_default_codegen_quest(FklVM *exe,
        const FklPmatchRes *value,
        FklCgActVector *actions,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalueCgEnv *env,
        FklCgAct *prev,
        FklVMvalueCgInfo *info) {
    FklCgAct *cur = create_cg_action(_default_bc_process,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(value->container),
            prev,
            info);
    fklValueVectorPushBack2(&cur->bcl_vector,
            gen_push_literal_code(exe, value, info, env, scope));
    fklCgActVectorPushBack2(actions, cur);
}

static void add_compiler_macro(FklMacroHashMap *macros,
        FklVMvalue *pattern,
        FklVMvalue *bcl,
        uint32_t prototype_id) {
    int coverState = FKL_PATTERN_NOT_EQUAL;
    FklCgMacro **pmacro = fklMacroHashMapAdd1(macros, FKL_VM_CAR(pattern));
    FklCgMacro **phead = pmacro;
    for (FklCgMacro *cur = *pmacro; cur; pmacro = &cur->next, cur = cur->next) {
        coverState = fklPatternCoverState(cur->pattern, pattern);
        if (coverState == FKL_PATTERN_BE_COVER)
            phead = &cur->next;
        else if (coverState)
            break;
    }
    if (coverState == FKL_PATTERN_NOT_EQUAL) {
        FklCgMacro *macro =
                fklCreateCgMacro(pattern, bcl, *phead, prototype_id);
        *phead = macro;
    } else if (coverState == FKL_PATTERN_EQUAL) {
        FklCgMacro *macro = *pmacro;
        uninit_codegen_macro(macro);
        macro->prototype_id = prototype_id;
        macro->pattern = pattern;
        macro->bcl = bcl;
    } else {
        FklCgMacro *next = *pmacro;
        *pmacro = fklCreateCgMacro(pattern, bcl, next, prototype_id);
    }
}

static inline void add_export_symbol(FklVMvalueCgInfo *info,
        FklVMvalue *orig,
        FklVMvalue *rest,
        CgExpQueue *exportQueue) {
    FklVMvalue *prev = orig;
    FklVMvalue *head = FKL_VM_CAR(orig);
    for (; FKL_IS_PAIR(rest); rest = FKL_VM_CDR(rest)) {
        FklVMvalue *new_rest = fklCreateVMvaluePair(IGCVM, head, rest);
        put_line_number(info->ctx->lnt, new_rest, CURLINE(rest));
        cgExpQueuePush2(exportQueue,
                (FklPmatchRes){ .value = new_rest, .container = new_rest });
        FKL_VM_CDR(prev) = FKL_VM_NIL;
        prev = rest;
    }
}

static inline void push_single_bcl_codegen_quest(FklVMvalue *bcl,
        FklCgActVector *actions,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalueCgEnv *env,
        FklCgAct *prev,
        FklVMvalueCgInfo *info,
        uint64_t curline) {
    FklCgAct *quest = create_cg_action(_default_bc_process,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            curline,
            prev,
            info);
    fklValueVectorPushBack2(&quest->bcl_vector, bcl);
    fklCgActVectorPushBack2(actions, quest);
}

typedef FklVMvalue *(*ReplacementFunc)(const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info);

static inline ReplacementFunc findBuiltInReplacementWithId(FklVMvalue *id,
        FklVMvalue *const builtin_replacement_id[]);

static inline int is_replacement_define(FklVMvalue *value,
        const FklVMvalueCgInfo *info,
        const FklCgCtx *ctx,
        const FklVMvalueCgMacroScope *macro_scope,
        const FklVMvalueCgEnv *env) {
    FklVMvalue *replacement = NULL;
    ReplacementFunc f = NULL;
    for (const FklVMvalueCgMacroScope *cs = macro_scope; cs && !replacement;
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
        const FklVMvalueCgInfo *info,
        const FklCgCtx *ctx,
        const FklVMvalueCgMacroScope *macro_scope,
        const FklVMvalueCgEnv *env) {
    FklVMvalue *replacement = NULL;
    ReplacementFunc f = NULL;
    for (const FklVMvalueCgMacroScope *cs = macro_scope; cs && !replacement;
            cs = cs->prev)
        replacement = fklGetReplacement(value->value, cs->replacements);
    if (replacement) {
        int r = replacement != FKL_VM_NIL;
        return r;
    } else if ((f = findBuiltInReplacementWithId(value->value,
                        ctx->builtin_replacement_id))) {
        FklVMvalue *t = f(value, env, info);
        int r = t != FKL_VM_NIL;
        return r;
    } else
        return 0;
}

static inline FklVMvalue *get_replacement(const FklPmatchRes *value,
        const FklVMvalueCgInfo *info,
        const FklCgCtx *ctx,
        const FklVMvalueCgMacroScope *macro_scope,
        const FklVMvalueCgEnv *env) {
    FklVMvalue *replacement = NULL;
    ReplacementFunc f = NULL;
    for (const FklVMvalueCgMacroScope *cs = macro_scope; cs && !replacement;
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
    FklPairVector s;
    fklPairVectorInit(&s, 8);
    fklPairVectorPushBack2(&s, (FklPair){ .car = cadr(p), .cdr = e });

    FklVMvalue *slot_id = FKL_VM_CAR(p);
    int r = 1;
    while (!fklPairVectorIsEmpty(&s)) {
        const FklPair *top = fklPairVectorPopBackNonNull(&s);
        const FklVMvalue *p = top->car;
        const FklVMvalue *e = top->cdr;
        if (is_compile_check_pattern_slot_node(slot_id, p))
            continue;
        if (FKL_GET_TAG(p) != FKL_GET_TAG(e)) {
            r = 0;
            goto exit;
        }

        if (FKL_IS_PTR(p)) {
            switch (p->type_) {
            case FKL_TYPE_BOX:
                fklPairVectorPushBack2(&s,
                        (FklPair){
                            .car = FKL_VM_BOX(p),
                            .cdr = FKL_VM_BOX(e),
                        });
                break;
            case FKL_TYPE_PAIR:
                fklPairVectorPushBack2(&s,
                        (FklPair){
                            .car = FKL_VM_CAR(p),
                            .cdr = FKL_VM_CAR(e),
                        });
                fklPairVectorPushBack2(&s,
                        (FklPair){
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
                    fklPairVectorPushBack2(&s,
                            (FklPair){
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

                FklValueHashMapNode *a = FKL_VM_HASH(p)->ht.first;
                FklValueHashMapNode *b = FKL_VM_HASH(e)->ht.first;
                for (; a; a = a->next, b = b->next) {
                    fklPairVectorPushBack2(&s,
                            (FklPair){
                                .car = a->k,
                                .cdr = b->k,
                            });

                    fklPairVectorPushBack2(&s,
                            (FklPair){
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
    fklPairVectorUninit(&s);
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

static inline int cfg_check_defined(const FklVMvalueCgInfo *info,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCgCtx *ctx,
        const FklVMvalueCgEnv *env,
        uint32_t scope,
        FklCgErrorState *error_state) {
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(value->value)) {
        error_state->error = make_syntax_error(GCVM, exp->value);
        error_state->line = CURLINE(exp->container);
        return 0;
    }
    return fklFindSymbolDefByIdAndScope(value->value, scope, env->scopes)
        != NULL;
}

static inline int cfg_check_importable(const FklVMvalueCgInfo *info,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCgCtx *ctx,
        const FklVMvalueCgEnv *env,
        uint32_t scope,
        FklCgErrorState *error_state) {
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (value->value == FKL_VM_NIL || !is_symbol_list(value->value)) {
        error_state->error = make_syntax_error(GCVM, exp->value);
        error_state->line = CURLINE(exp->container);
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

static inline int cfg_check_macro_defined(const FklVMvalueCgInfo *info,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCgCtx *ctx,
        const FklVMvalueCgEnv *env,
        uint32_t scope,
        const FklVMvalueCgMacroScope *macro_scope,
        FklCgErrorState *error_state) {
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (FKL_IS_SYM(value->value))
        return is_replacement_define(value->value, info, ctx, macro_scope, env);
    else if (FKL_IS_PAIR(value->value)                 //
             && FKL_VM_CDR(value->value) == FKL_VM_NIL //
             && FKL_IS_SYM(FKL_VM_CAR(value->value))) {
        int r = 0;
        FklVMvalue *id = FKL_VM_CAR(value->value);
        for (const FklVMvalueCgMacroScope *cur_scope = macro_scope; cur_scope;
                cur_scope = cur_scope->prev) {
            const FklMacroHashMap *macros = cur_scope->macros;
            FklCgMacro *const *const pm = fklMacroHashMapGet2(macros, id);
            if (pm != NULL) {
                r = 1;
                break;
            }
        }
        return r;
    } else if (FKL_IS_BOX(value->value) //
               && FKL_IS_SYM(FKL_VM_BOX(value->value))) {
        FklVMvalue *id = FKL_VM_BOX(value->value);
        return *(info->g) != NULL
            && fklGraProdGroupHashMapGet2(info->prod_groups, id) != NULL;
    } else {
        error_state->error = make_syntax_error(GCVM, value->value);
        error_state->line = CURLINE(value->container);
        return 0;
    }
}

static inline int cfg_check_eq(const FklVMvalueCgInfo *info,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCgCtx *ctx,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgMacroScope *macro_scope,
        FklCgErrorState *error_state) {
    const FklPmatchRes *arg0 = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg0);
    const FklPmatchRes *arg1 = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg1);
    if (!FKL_IS_SYM(arg0->value)) {
        error_state->error = make_syntax_error(GCVM, exp->value);
        error_state->line = CURLINE(exp->container);
        return 0;
    }
    FklVMvalue *node = get_replacement(arg0, info, ctx, macro_scope, env);
    if (node) {
        int r = fklVMvalueEqual(node, arg1->value);
        return r;
    } else
        return 0;
}

static inline int cfg_check_matched(const FklVMvalueCgInfo *info,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCgCtx *ctx,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgMacroScope *macro_scope,
        FklCgErrorState *error_state) {
    const FklPmatchRes *arg0 = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg0);
    const FklPmatchRes *arg1 = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg1);
    if (!FKL_IS_SYM(arg0->value)
            || !is_valid_compile_check_pattern(arg1->value)) {
        error_state->error = make_syntax_error(GCVM, exp->value);
        error_state->line = CURLINE(exp->container);
        return 0;
    }
    FklVMvalue *node = get_replacement(arg0, info, ctx, macro_scope, env);
    if (node) {
        int r = is_compile_check_pattern_matched(arg1->value, node);
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

static inline int is_check_subpattern_true(const FklVMvalueCgInfo *info,
        const FklPmatchRes *exp_,
        const FklCgCtx *ctx,
        uint32_t scope,
        const FklVMvalueCgMacroScope *macro_scope,
        const FklVMvalueCgEnv *env,
        FklCgErrorState *error_state) {
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
                r = cfg_check_defined(info,
                        &exp,
                        &ht,
                        ctx,
                        env,
                        scope,
                        error_state);
                if (error_state->error)
                    goto exit;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_IMPORT],
                               exp.value,
                               &ht)) {
                r = cfg_check_importable(info,
                        &exp,
                        &ht,
                        ctx,
                        env,
                        scope,
                        error_state);
                if (error_state->error)
                    goto exit;
            } else if (fklPatternMatch(
                               ctx->builtin_sub_pattern_node
                                       [FKL_CODEGEN_SUB_PATTERN_DEFMACRO],
                               exp.value,
                               &ht)) {
                r = cfg_check_macro_defined(info,
                        &exp,
                        &ht,
                        ctx,
                        env,
                        scope,
                        macro_scope,
                        error_state);
                if (error_state->error)
                    goto exit;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_EQ],
                               exp.value,
                               &ht)) {
                r = cfg_check_eq(info,
                        &exp,
                        &ht,
                        ctx,
                        env,
                        macro_scope,
                        error_state);
                if (error_state->error)
                    goto exit;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_MATCH],
                               exp.value,
                               &ht)) {
                r = cfg_check_matched(info,
                        &exp,
                        &ht,
                        ctx,
                        env,
                        macro_scope,
                        error_state);
                if (error_state->error)
                    goto exit;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_NOT],
                               exp.value,
                               &ht)) {
                cfg_ctx = cgCfgCtxVectorPushBack(&s, NULL);
                cfg_ctx->r = 0;
                cfg_ctx->rest = NULL;
                cfg_ctx->method = cfg_check_not_method;
                exp = *fklPmatchHashMapGet2(&ht, ctx->builtin_sym_value);
                continue;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_AND],
                               exp.value,
                               &ht)) {
                const FklPmatchRes *rest =
                        fklPmatchHashMapGet2(&ht, ctx->builtin_sym_rest);
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
                    error_state->error = make_syntax_error(GCVM, exp.value);
                    error_state->line = CURLINE(exp.container);
                    goto exit;
                }
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_OR],
                               exp.value,
                               &ht)) {
                const FklPmatchRes *rest =
                        fklPmatchHashMapGet2(&ht, ctx->builtin_sym_rest);
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
                    error_state->error = make_syntax_error(GCVM, exp.value);
                    error_state->line = CURLINE(exp.container);
                    goto exit;
                }
            } else {
                error_state->error = make_syntax_error(GCVM, exp.value);
                error_state->line = CURLINE(exp.container);
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

static void codegen_check(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    int r = is_check_subpattern_true(info,
            value,
            ctx,
            scope,
            macro_scope,
            env,
            error_state);
    if (error_state->error)
        return;
    FklVMvalue *bcl = fklCreateVMvalueCodeObjExt(GCVM,
            create_op_ins(r ? FKL_OP_PUSH_1 : FKL_OP_PUSH_NIL),
            info->fid,
            CURLINE(orig->container),
            scope);
    push_single_bcl_codegen_quest(bcl,
            actions,
            scope,
            macro_scope,
            env,
            NULL,
            info,
            CURLINE(orig->container));
}

static void codegen_cond_compile(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *cond = fklPmatchHashMapGet2(ht, ctx->builtin_sym_cond);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    if (fklVMlistLength(rest->value) % 2 == 1) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    int r = is_check_subpattern_true(info,
            cond,
            ctx,
            scope,
            macro_scope,
            env,
            error_state);
    if (error_state->error)
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
                info,
                actions);
        return;
    }
    FklVMvalue *rest_value = rest->value;
    while (FKL_IS_PAIR(rest_value)) {
        FklVMvalue *cond = FKL_VM_CAR(rest_value);
        rest_value = FKL_VM_CDR(rest_value);
        FklVMvalue *value = FKL_VM_CAR(rest_value);
        rest_value = FKL_VM_CDR(rest_value);
        int r = is_check_subpattern_true(info,
                &(FklPmatchRes){
                    .value = cond,
                    .container = rest_value,
                },
                ctx,
                scope,
                macro_scope,
                env,
                error_state);
        if (error_state->error)
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
                    info,
                    actions);
            return;
        }
    }

    if (must_has_retval) {
        error_state->error = make_has_no_value_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
}

static void codegen_quote(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;

    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    push_default_codegen_quest(GCVM,
            value,
            actions,
            scope,
            macro_scope,
            env,
            NULL,
            info);
}

static inline void unquoteHelperFunc(const FklPmatchRes *value,
        FklCgActVector *actions,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalueCgEnv *env,
        FklCgActCb func,
        FklCgAct *prev,
        FklVMvalueCgInfo *info) {
    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, value);
    FklCgAct *quest = create_cg_action(func,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(value->container),
            prev,
            info);
    fklCgActVectorPushBack2(actions, quest);
}

static void codegen_unquote(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;

    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    unquoteHelperFunc(value,
            actions,
            scope,
            macro_scope,
            env,
            _default_bc_process,
            NULL,
            info);
}

static FklVMvalue *_qsquote_box_bc_process(const FklCgActCbArgs *args) {
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = *fklValueVectorPopBackNonNull(bcl_vec);
    FklInstruction pushBox = { .op = FKL_OP_PUSH_BOX, .ai = 1 };
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval), &pushBox, fid, line, scope);
    return retval;
}

static FklVMvalue *_qsquote_vec_bc_process(const FklCgActCbArgs *args) {
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = bcl_vec->base[0];
    for (size_t i = 1; i < bcl_vec->size; i++) {
        FklVMvalue *cur = bcl_vec->base[i];
        fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
    }
    bcl_vec->size = 0;
    FklInstruction push_vec = create_op_ins(FKL_OP_PUSH_VEC_0);
    FklInstruction set_bp = create_op_ins(FKL_OP_SET_BP);
    fklByteCodeLntInsertFrontIns(&set_bp, FKL_VM_CO(retval), fid, line, scope);
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval), &push_vec, fid, line, scope);
    return retval;
}

static FklVMvalue *_qsquote_hash_bc_process(const FklCgActCbArgs *args) {
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *push_hash = bcl_vec->base[0];
    FklVMvalue *retval = bcl_vec->base[1];
    for (size_t i = 2; i < bcl_vec->size; ++i) {
        FklVMvalue *cur = bcl_vec->base[i];
        fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
    }
    fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(push_hash));
    bcl_vec->size = 0;
    FklInstruction set_bp = create_op_ins(FKL_OP_SET_BP);
    fklByteCodeLntInsertFrontIns(&set_bp, FKL_VM_CO(retval), fid, line, scope);
    return retval;
}

static FklVMvalue *_unqtesp_vec_bc_process(const FklCgActCbArgs *args) {
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = *fklValueVectorPopBackNonNull(bcl_vec);
    FklVMvalue *other = fklValueVectorIsEmpty(bcl_vec)
                              ? NULL
                              : *fklValueVectorPopBackNonNull(bcl_vec);
    if (other) {
        while (!fklValueVectorIsEmpty(bcl_vec)) {
            FklVMvalue *cur = *fklValueVectorPopBackNonNull(bcl_vec);
            fklCodeLntConcat(FKL_VM_CO(other), FKL_VM_CO(cur));
        }
        fklCodeLntReverseConcat(FKL_VM_CO(other), FKL_VM_CO(retval));
    }
    FklInstruction listPush = { .op = FKL_OP_PAIR,
        .ai = FKL_SUBOP_PAIR_UNPACK };
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval), &listPush, fid, line, scope);
    return retval;
}

static FklVMvalue *_qsquote_pair_bc_process(const FklCgActCbArgs *args) {
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = bcl_vec->base[0];
    for (size_t i = 1; i < bcl_vec->size; i++) {
        FklVMvalue *cur = bcl_vec->base[i];
        fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
    }
    bcl_vec->size = 0;
    FklInstruction pushPair = create_op_ins(FKL_OP_PUSH_LIST_0);
    FklInstruction setBp = create_op_ins(FKL_OP_SET_BP);
    fklByteCodeLntInsertFrontIns(&setBp, FKL_VM_CO(retval), fid, line, scope);
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval), &pushPair, fid, line, scope);
    return retval;
}

static FklVMvalue *_qsquote_list_bc_process(const FklCgActCbArgs *args) {
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = bcl_vec->base[0];
    for (size_t i = 1; i < bcl_vec->size; i++) {
        FklVMvalue *cur = bcl_vec->base[i];
        fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
    }
    bcl_vec->size = 0;
    FklInstruction pushPair = { .op = FKL_OP_PAIR,
        .ai = FKL_SUBOP_PAIR_APPEND };
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval), &pushPair, fid, line, scope);
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
    FklCgAct *prev;
} QsquoteHelperStruct;

static inline void init_qsquote_helper_struct(QsquoteHelperStruct *r,
        QsquoteHelperEnum e,
        const FklPmatchRes *node,
        FklCgAct *prev) {
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
is_unquote(FklCgCtx *ctx, FklVMvalue *v, FklPmatchHashMap *unquoteHt) {
    return fklPatternMatch(
            ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_UNQUOTE],
            v,
            unquoteHt);
}

static inline int
is_unqtesp(FklCgCtx *ctx, FklVMvalue *v, FklPmatchHashMap *unquoteHt) {
    return fklPatternMatch(
            ctx->builtin_sub_pattern_node[FKL_CODEGEN_SUB_PATTERN_UNQTESP],
            v,
            unquoteHt);
}

typedef struct {
    FklCgCtx *ctx;
    const QsquoteHelperStruct *top;
    FklCgActVector *action_vector;
    uint32_t scope;
    FklVMvalueCgMacroScope *macro_scope;
    FklVMvalueCgEnv *env;
    FklVMvalueCgInfo *info;
    CgQsquoteHelperVector *pending;
} QsquoteStateNoneArgs;

static void qsquote_state_none_pair(FklVMvalue *value,
        const FklVMvalue *container,
        const QsquoteStateNoneArgs *args,
        FklPmatchHashMap *table) {

    FklCgCtx *ctx = args->ctx;
    const QsquoteHelperStruct *top = args->top;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklCgAct *prev = top->prev;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklCgActVector *action_vector = args->action_vector;
    FklVMvalueCgInfo *info = args->info;
    CgQsquoteHelperVector *pending = args->pending;

    FklCgAct *curAction = create_cg_action(_qsquote_pair_bc_process,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(container),
            prev,
            info);
    fklCgActVectorPushBack2(action_vector, curAction);
    for (FklVMvalue *node = value;;) {
        if (is_unquote(ctx, node, table)) {
            const FklPmatchRes *unquote_v =
                    fklPmatchHashMapGet2(table, ctx->builtin_sym_value);
            unquoteHelperFunc(unquote_v,
                    action_vector,
                    scope,
                    macro_scope,
                    env,
                    _default_bc_process,
                    curAction,
                    info);
            break;
        }
        FklVMvalue *cur = FKL_VM_CAR(node);
        if (is_unqtesp(ctx, cur, table)) {
            const FklPmatchRes *unqtesp_v =
                    fklPmatchHashMapGet2(table, ctx->builtin_sym_value);
            if (FKL_VM_CDR(node) != FKL_VM_NIL) {
                FklCgAct *appendAction =
                        create_cg_action(_qsquote_list_bc_process,
                                createDefaultStackContext(),
                                NULL,
                                scope,
                                macro_scope,
                                env,
                                CURLINE(container),
                                curAction,
                                info);
                fklCgActVectorPushBack2(action_vector, appendAction);
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

    FklCgCtx *ctx = args->ctx;
    const QsquoteHelperStruct *top = args->top;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklCgAct *prev = top->prev;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklCgActVector *action_vector = args->action_vector;
    FklVMvalueCgInfo *info = args->info;
    CgQsquoteHelperVector *pending = args->pending;

    size_t vec_len = FKL_VM_VEC(value)->size;
    FklCgAct *action = create_cg_action(_qsquote_vec_bc_process,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(container),
            prev,
            info);
    fklCgActVectorPushBack2(action_vector, action);
    for (size_t i = 0; i < vec_len; i++) {
        if (is_unqtesp(ctx, FKL_VM_VEC(value)->base[i], table)) {
            init_qsquote_helper_struct(
                    cgQsquoteHelperVectorPushBack(pending, NULL),
                    QSQUOTE_UNQTESP_VEC,
                    fklPmatchHashMapGet2(table, ctx->builtin_sym_value),
                    action);
        } else {
            init_qsquote_helper_struct(
                    cgQsquoteHelperVectorPushBack(pending, NULL),
                    QSQUOTE_NONE,
                    &(FklPmatchRes){
                        .value = FKL_VM_VEC(value)->base[i],
                        .container = value,
                    },
                    action);
        }
    }
}

static void qsquote_state_none_hash(FklVMvalue *value,
        const FklVMvalue *container,
        const QsquoteStateNoneArgs *args,
        FklPmatchHashMap *table) {

    FklCgCtx *ctx = args->ctx;
    const QsquoteHelperStruct *top = args->top;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklCgAct *prev = top->prev;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklCgActVector *action_vector = args->action_vector;
    FklVMvalueCgInfo *info = args->info;
    CgQsquoteHelperVector *pending = args->pending;

    FklCgAct *action = create_cg_action(_qsquote_hash_bc_process,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(container),
            prev,
            info);
    fklCgActVectorPushBack2(action_vector, action);

    FklVMvalueHash *hash = FKL_VM_HASH(value);
    FklVMvalue *push_hash = fklCreateVMvalueCodeObjExt(GCVM,
            create_op_ins(FKL_OP_PUSH_HASHEQ_0 + hash->eq_type),
            info->fid,
            CURLINE(container),
            scope);

    fklValueVectorPushBack2(&action->bcl_vector, push_hash);

    for (const FklValueHashMapNode *cur = hash->ht.first; cur;
            cur = cur->next) {
        init_qsquote_helper_struct(cgQsquoteHelperVectorPushBack(pending, NULL),
                QSQUOTE_NONE,
                &(FklPmatchRes){
                    .value = cur->k,
                    .container = value,
                },
                action);
        init_qsquote_helper_struct(cgQsquoteHelperVectorPushBack(pending, NULL),
                QSQUOTE_NONE,
                &(FklPmatchRes){
                    .value = cur->v,
                    .container = value,
                },
                action);
    }
}

static void qsquote_state_none_box(FklVMvalue *value,
        const FklVMvalue *container,
        const QsquoteStateNoneArgs *args,
        FklPmatchHashMap *table) {

    const QsquoteHelperStruct *top = args->top;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklCgAct *prev = top->prev;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklCgActVector *action_vector = args->action_vector;
    FklVMvalueCgInfo *info = args->info;
    CgQsquoteHelperVector *pending = args->pending;

    FklCgAct *action = create_cg_action(_qsquote_box_bc_process,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(container),
            prev,
            info);
    fklCgActVectorPushBack2(action_vector, action);
    init_qsquote_helper_struct(cgQsquoteHelperVectorPushBack(pending, NULL),
            QSQUOTE_NONE,
            &(FklPmatchRes){
                .value = FKL_VM_BOX(value),
                .container = value,
            },
            action);
}

static void qsquote_state_none(const QsquoteStateNoneArgs *args) {
    FklCgCtx *ctx = args->ctx;
    const QsquoteHelperStruct *top = args->top;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklCgAct *prev = top->prev;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklCgActVector *action_vector = args->action_vector;
    FklVMvalueCgInfo *info = args->info;

    FklVMvalue *value = top->node.value;
    const FklVMvalue *container = top->node.container;

    FklPmatchHashMap table;
    fklPmatchHashMapInit(&table);
    if (is_unquote(ctx, value, &table)) {
        const FklPmatchRes *unquoteValue =
                fklPmatchHashMapGet2(&table, ctx->builtin_sym_value);
        unquoteHelperFunc(unquoteValue,
                action_vector,
                scope,
                macro_scope,
                env,
                _default_bc_process,
                prev,
                info);
    } else if (FKL_IS_PAIR(value)) {
        qsquote_state_none_pair(value, container, args, &table);
    } else if (FKL_IS_VECTOR(value)) {
        qsquote_state_none_vector(value, container, args, &table);
    } else if (FKL_IS_BOX(value)) {
        qsquote_state_none_box(value, container, args, &table);
    } else if (FKL_IS_HASHTABLE(value)) {
        qsquote_state_none_hash(value, container, args, &table);
    } else
        push_default_codegen_quest(GCVM,
                &top->node,
                action_vector,
                scope,
                macro_scope,
                env,
                prev,
                info);
    fklPmatchHashMapUninit(&table);
}

static void codegen_qsquote(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;

    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
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
        FklCgAct *prevAction = top.prev;
        switch (e) {
        case QSQUOTE_UNQTESP_CAR:
            unquoteHelperFunc(&top.node,
                    actions,
                    scope,
                    macro_scope,
                    env,
                    _default_bc_process,
                    prevAction,
                    info);
            break;
        case QSQUOTE_UNQTESP_VEC:
            unquoteHelperFunc(&top.node,
                    actions,
                    scope,
                    macro_scope,
                    env,
                    _unqtesp_vec_bc_process,
                    prevAction,
                    info);
            break;
        case QSQUOTE_NONE: {
            if (is_unqtesp(ctx, top.node.value, NULL)) {
                error_state->error = make_syntax_error(GCVM, top.node.value);
                error_state->line = CURLINE(top.node.container);
                goto done;
            }

            QsquoteStateNoneArgs args = {
                .ctx = ctx,
                .top = &top,
                .action_vector = actions,
                .scope = scope,
                .macro_scope = macro_scope,
                .env = env,
                .info = info,
                .pending = &pending,
            };
            qsquote_state_none(&args);
        } break;
        }
    }
done:
    cgQsquoteHelperVectorUninit(&pending);
}

static FklVMvalue *_cond_exp_bc_process_0(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = NULL;
    if (bcl_vec->size)
        retval = *fklValueVectorPopBackNonNull(bcl_vec);
    else
        retval = fklCreateVMvalueCodeObjExt(GCVM,
                create_op_ins(FKL_OP_PUSH_NIL),
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
        case FKL_OP_PUSH_PAIR:
        case FKL_OP_PUSH_LIST_0:
        case FKL_OP_PUSH_VEC_0:
        case FKL_OP_PUSH_HASHEQ_0:
        case FKL_OP_PUSH_HASHEQV_0:
        case FKL_OP_PUSH_HASHEQUAL_0:
        case FKL_OP_TRUE:
        case FKL_OP_EXTRA_ARG:
            break;
        default:
            return 0;
        }
    }
    return 1;
}

static FklVMvalue *_cond_exp_bc_process_1(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = NULL;
    if (bcl_vec->size >= 2) {
        FklVMvalue *prev = bcl_vec->base[0];
        FklVMvalue *first = bcl_vec->base[1];
        retval = create_0len_bcl(GCVM);

        FklInstruction drop = create_op_ins(FKL_OP_DROP);

        fklByteCodeLntInsertFrontIns(&drop, FKL_VM_CO(prev), fid, line, scope);

        uint64_t prev_len = FKL_VM_CO(prev)->bc.len;

        int true_bcl = is_const_true_bytecode_lnt(FKL_VM_CO(first));

        if (bcl_vec->size > 2) {
            FklVMvalue *cur = bcl_vec->base[2];
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }

        for (size_t i = 3; i < bcl_vec->size; i++) {
            FklVMvalue *cur = bcl_vec->base[i];
            fklByteCodeLntPushBackIns(FKL_VM_CO(retval),
                    &drop,
                    fid,
                    line,
                    scope);
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }

        size_t retval_len = FKL_VM_CO(retval)->bc.len;

        check_and_close_ref(GCVM, retval, scope, env, &info->pts->p, fid, line);

        if (bcl_vec->size > 2) {
            append_jmp_ins(GCVM,
                    INS_APPEND_BACK,
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
                fklByteCodeLntInsertFrontIns(&drop,
                        FKL_VM_CO(retval),
                        fid,
                        line,
                        scope);
                append_jmp_ins(GCVM,
                        INS_APPEND_FRONT,
                        retval,
                        FKL_VM_CO(retval)->bc.len,
                        JMP_IF_FALSE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
                fklCodeLntReverseConcat(FKL_VM_CO(first), FKL_VM_CO(retval));
            }
        } else {
            append_jmp_ins(GCVM,
                    INS_APPEND_BACK,
                    first,
                    FKL_VM_CO(prev)->bc.len,
                    true_bcl ? JMP_UNCOND : JMP_IF_TRUE,
                    JMP_FORWARD,
                    fid,
                    line,
                    scope);
            fklCodeLntReverseConcat(FKL_VM_CO(first), FKL_VM_CO(retval));
        }

        fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(prev));
    } else
        retval = *fklValueVectorPopBackNonNull(bcl_vec);
    bcl_vec->size = 0;

    return retval;
}

static FklVMvalue *_cond_exp_bc_process_2(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklInstruction drop = create_op_ins(FKL_OP_DROP);

    FklVMvalue *retval = NULL;
    if (bcl_vec->size) {
        retval = create_0len_bcl(GCVM);
        FklVMvalue *first = bcl_vec->base[0];
        int true_bcl = is_const_true_bytecode_lnt(FKL_VM_CO(first));
        if (bcl_vec->size > 1) {
            FklVMvalue *cur = bcl_vec->base[1];
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }
        for (size_t i = 2; i < bcl_vec->size; i++) {
            FklVMvalue *cur = bcl_vec->base[i];
            fklByteCodeLntPushBackIns(FKL_VM_CO(retval),
                    &drop,
                    fid,
                    line,
                    scope);
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }

        if (FKL_VM_CO(retval)->bc.len) {
            if (!true_bcl) {
                fklByteCodeLntInsertFrontIns(&drop,
                        FKL_VM_CO(retval),
                        fid,
                        line,
                        scope);
                append_jmp_ins(GCVM,
                        INS_APPEND_FRONT,
                        retval,
                        FKL_VM_CO(retval)->bc.len,
                        JMP_IF_FALSE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
                fklCodeLntReverseConcat(FKL_VM_CO(first), FKL_VM_CO(retval));
            }
        } else
            fklCodeLntReverseConcat(FKL_VM_CO(first), FKL_VM_CO(retval));
    }
    bcl_vec->size = 0;
    return retval;
}

static void codegen_cond(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    FklCgAct *quest = create_cg_action(_cond_exp_bc_process_0,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(rest->value),
            NULL,
            info);
    fklCgActVectorPushBack2(actions, quest);
    if (rest->value != FKL_VM_NIL) {
        FklValueVector tmpStack;
        fklValueVectorInit(&tmpStack, 32);
        pushListItemToStack(rest->value, &tmpStack, NULL);
        FklVMvalue *lastExp = *fklValueVectorPopBackNonNull(&tmpStack);
        FklCgAct *prevAction = quest;
        for (size_t i = 0; i < tmpStack.size; i++) {
            FklVMvalue *curExp = tmpStack.base[i];
            if (!FKL_IS_PAIR(curExp)) {
                error_state->error = make_syntax_error(GCVM, orig->value);
                error_state->line = CURLINE(orig->container);
                fklValueVectorUninit(&tmpStack);
                return;
            }
            FklVMvalue *last = FKL_VM_NIL;
            CgExpQueue *curQueue = cgExpQueueCreate();
            pushListItemToQueue(curExp, curQueue, &last);
            if (last != FKL_VM_NIL) {
                error_state->error = make_syntax_error(GCVM, orig->value);
                error_state->line = CURLINE(orig->container);
                cgExpQueueDestroy(curQueue);
                fklValueVectorUninit(&tmpStack);
                return;
            }
            uint32_t curScope = enter_new_scope(scope, env);
            FklVMvalueCgMacroScope *cms =
                    fklCreateVMvalueCgMacroScope(ctx, macro_scope);
            FklCgAct *curAction = create_cg_action(_cond_exp_bc_process_1,
                    createDefaultStackContext(),
                    createFirstHasRetvalQueueNextExpression(curQueue),
                    curScope,
                    cms,
                    env,
                    CURLINE(curExp),
                    prevAction,
                    info);
            fklCgActVectorPushBack2(actions, curAction);
            prevAction = curAction;
        }
        FklVMvalue *last = FKL_VM_NIL;
        if (!FKL_IS_PAIR(lastExp)) {
            error_state->error = make_syntax_error(GCVM, orig->value);
            error_state->line = CURLINE(orig->container);
            fklValueVectorUninit(&tmpStack);
            return;
        }
        CgExpQueue *lastQueue = cgExpQueueCreate();
        pushListItemToQueue(lastExp, lastQueue, &last);
        if (last != FKL_VM_NIL) {
            error_state->error = make_syntax_error(GCVM, orig->value);
            error_state->line = CURLINE(orig->container);
            cgExpQueueDestroy(lastQueue);
            fklValueVectorUninit(&tmpStack);
            return;
        }
        uint32_t curScope = enter_new_scope(scope, env);
        FklVMvalueCgMacroScope *cms =
                fklCreateVMvalueCgMacroScope(ctx, macro_scope);
        fklCgActVectorPushBack2(actions,
                create_cg_action(_cond_exp_bc_process_2,
                        createDefaultStackContext(),
                        createFirstHasRetvalQueueNextExpression(lastQueue),
                        curScope,
                        cms,
                        env,
                        CURLINE(lastExp),
                        prevAction,
                        info));
        fklValueVectorUninit(&tmpStack);
    }
}

static FklVMvalue *_if_exp_bc_process_0(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklInstruction drop = create_op_ins(FKL_OP_DROP);

    if (bcl_vec->size >= 2) {
        FklVMvalue *exp = *fklValueVectorPopBackNonNull(bcl_vec);
        FklVMvalue *cond = *fklValueVectorPopBackNonNull(bcl_vec);
        fklByteCodeLntInsertFrontIns(&drop, FKL_VM_CO(exp), fid, line, scope);
        append_jmp_ins(GCVM,
                INS_APPEND_BACK,
                cond,
                FKL_VM_CO(exp)->bc.len,
                JMP_IF_FALSE,
                JMP_FORWARD,
                fid,
                line,
                scope);
        fklCodeLntConcat(FKL_VM_CO(cond), FKL_VM_CO(exp));
        return cond;
    } else if (bcl_vec->size >= 1)
        return *fklValueVectorPopBackNonNull(bcl_vec);
    else
        return fklCreateVMvalueCodeObjExt(GCVM,
                create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static void codegen_if0(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *cond = fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    const FklPmatchRes *exp = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);

    CgExpQueue *nextQueue = cgExpQueueCreate();
    cgExpQueuePush(nextQueue, cond);
    cgExpQueuePush(nextQueue, exp);

    uint32_t curScope = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);
    fklCgActVectorPushBack2(actions,
            create_cg_action(_if_exp_bc_process_0,
                    createDefaultStackContext(),
                    createMustHasRetvalQueueNextExpression(nextQueue),
                    curScope,
                    cms,
                    env,
                    CURLINE(orig->container),
                    NULL,
                    info));
}

static FklVMvalue *_if_exp_bc_process_1(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklInstruction drop = create_op_ins(FKL_OP_DROP);

    if (bcl_vec->size >= 3) {
        FklVMvalue *exp0 = *fklValueVectorPopBackNonNull(bcl_vec);
        FklVMvalue *cond = *fklValueVectorPopBackNonNull(bcl_vec);
        FklVMvalue *exp1 = *fklValueVectorPopBackNonNull(bcl_vec);
        fklByteCodeLntInsertFrontIns(&drop, FKL_VM_CO(exp0), fid, line, scope);
        fklByteCodeLntInsertFrontIns(&drop, FKL_VM_CO(exp1), fid, line, scope);
        append_jmp_ins(GCVM,
                INS_APPEND_BACK,
                exp0,
                FKL_VM_CO(exp1)->bc.len,
                JMP_UNCOND,
                JMP_FORWARD,
                fid,
                line,
                scope);
        append_jmp_ins(GCVM,
                INS_APPEND_BACK,
                cond,
                FKL_VM_CO(exp0)->bc.len,
                JMP_IF_FALSE,
                JMP_FORWARD,
                fid,
                line,
                scope);
        fklCodeLntConcat(FKL_VM_CO(cond), FKL_VM_CO(exp0));
        fklCodeLntConcat(FKL_VM_CO(cond), FKL_VM_CO(exp1));
        return cond;
    } else if (bcl_vec->size >= 2) {
        FklVMvalue *exp0 = *fklValueVectorPopBackNonNull(bcl_vec);
        FklVMvalue *cond = *fklValueVectorPopBackNonNull(bcl_vec);
        fklByteCodeLntInsertFrontIns(&drop, FKL_VM_CO(exp0), fid, line, scope);
        append_jmp_ins(GCVM,
                INS_APPEND_BACK,
                cond,
                FKL_VM_CO(exp0)->bc.len,
                JMP_IF_FALSE,
                JMP_FORWARD,
                fid,
                line,
                scope);
        fklCodeLntConcat(FKL_VM_CO(cond), FKL_VM_CO(exp0));
        return cond;
    } else if (bcl_vec->size >= 1)
        return *fklValueVectorPopBackNonNull(bcl_vec);
    else
        return fklCreateVMvalueCodeObjExt(GCVM,
                create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static void codegen_if1(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *cond = fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    const FklPmatchRes *exp0 = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    const FklPmatchRes *exp1 = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);

    CgExpQueue *exp0Queue = cgExpQueueCreate();
    cgExpQueuePush(exp0Queue, cond);
    cgExpQueuePush(exp0Queue, exp0);

    CgExpQueue *exp1Queue = cgExpQueueCreate();
    cgExpQueuePush(exp1Queue, exp1);

    uint32_t curScope = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);
    FklCgAct *prev = create_cg_action(_if_exp_bc_process_1,
            createDefaultStackContext(),
            createMustHasRetvalQueueNextExpression(exp0Queue),
            curScope,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            info);
    fklCgActVectorPushBack2(actions, prev);

    curScope = enter_new_scope(scope, env);
    cms = fklCreateVMvalueCgMacroScope(ctx, macro_scope);
    fklCgActVectorPushBack2(actions,
            create_cg_action(_default_bc_process,
                    createDefaultStackContext(),
                    createMustHasRetvalQueueNextExpression(exp1Queue),
                    curScope,
                    cms,
                    env,
                    CURLINE(orig->container),
                    prev,
                    info));
}

static FklVMvalue *_when_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    if (bcl_vec->size) {
        FklVMvalue *cond = bcl_vec->base[0];
        FklVMvalue *retval = create_0len_bcl(GCVM);

        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        for (size_t i = 1; i < bcl_vec->size; i++) {
            FklVMvalue *cur = bcl_vec->base[i];
            fklByteCodeLntPushBackIns(FKL_VM_CO(retval),
                    &drop,
                    fid,
                    line,
                    scope);
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }
        bcl_vec->size = 0;
        if (FKL_VM_CO(retval)->bc.len)
            append_jmp_ins(GCVM,
                    INS_APPEND_FRONT,
                    retval,
                    FKL_VM_CO(retval)->bc.len,
                    JMP_IF_FALSE,
                    JMP_FORWARD,
                    fid,
                    line,
                    scope);
        fklCodeLntReverseConcat(FKL_VM_CO(cond), FKL_VM_CO(retval));
        check_and_close_ref(GCVM, retval, scope, env, &info->pts->p, fid, line);
        return retval;
    } else
        return fklCreateVMvalueCodeObjExt(GCVM,
                create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static FklVMvalue *_unless_exp_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    if (bcl_vec->size) {
        FklVMvalue *cond = bcl_vec->base[0];
        FklVMvalue *retval = create_0len_bcl(GCVM);

        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        for (size_t i = 1; i < bcl_vec->size; i++) {
            FklVMvalue *cur = bcl_vec->base[i];
            fklByteCodeLntPushBackIns(FKL_VM_CO(retval),
                    &drop,
                    fid,
                    line,
                    scope);
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }
        bcl_vec->size = 0;
        if (FKL_VM_CO(retval)->bc.len)
            append_jmp_ins(GCVM,
                    INS_APPEND_FRONT,
                    retval,
                    FKL_VM_CO(retval)->bc.len,
                    JMP_IF_TRUE,
                    JMP_FORWARD,
                    fid,
                    line,
                    scope);
        fklCodeLntReverseConcat(FKL_VM_CO(cond), FKL_VM_CO(retval));
        check_and_close_ref(GCVM, retval, scope, env, &info->pts->p, fid, line);
        return retval;
    } else
        return fklCreateVMvalueCodeObjExt(GCVM,
                create_op_ins(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
}

static inline void codegen_when_unless(const CgCbArgs *args, FklCgActCb func) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;

    const FklPmatchRes *cond = fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);

    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, cond);
    pushListItemToQueue(rest->value, queue, NULL);

    uint32_t curScope = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(info->ctx, macro_scope);

    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(func,
            createDefaultStackContext(),
            createFirstHasRetvalQueueNextExpression(queue),
            curScope,
            cms,
            env,
            CURLINE(rest->container),
            info,
            actions);
}

static void codegen_when(const CgCbArgs *args) {
    codegen_when_unless(args, _when_exp_bc_process);
}

static void codegen_unless(const CgCbArgs *args) {
    codegen_when_unless(args, _unless_exp_bc_process);
}

typedef struct {
    FILE *fp;
    FklVMvalueCgInfo *info;
    FklAnalysisSymbolVector symbolStack;
    FklParseStateVector stateStack;
} CgLoadCtx;

#include <fakeLisp/grammer.h>

static CgLoadCtx *createCgLoadCtx(FILE *fp, FklVMvalueCgInfo *info) {
    CgLoadCtx *r = (CgLoadCtx *)fklZmalloc(sizeof(CgLoadCtx));
    FKL_ASSERT(r);
    r->info = info;
    r->fp = fp;
    fklParseStateVectorInit(&r->stateStack, 16);
    fklAnalysisSymbolVectorInit(&r->symbolStack, 16);
    return r;
}

static void _codegen_load_finalizer(void *pcontext) {
    CgLoadCtx *context = pcontext;
    FklAnalysisSymbolVector *symbolStack = &context->symbolStack;
    fklAnalysisSymbolVectorUninit(symbolStack);
    fklParseStateVectorUninit(&context->stateStack);
    fclose(context->fp);
    fklZfree(context);
}

static void _codegen_load_atomic(FklVMgc *gc, void *ctx) {
    CgLoadCtx *c = FKL_TYPE_CAST(CgLoadCtx *, ctx);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, c->info), gc);
}

static inline FklVMvalue *getExpressionFromFile(FklVMvalueCgInfo *info,
        FILE *fp,
        int *unexpectEOF,
        size_t *output_line,
        FklCgErrorState *error_state,
        FklAnalysisSymbolVector *symbolStack,
        FklParseStateVector *stateStack) {
    size_t size = 0;
    FklVMvalue *begin = NULL;
    char *list = NULL;
    stateStack->size = 0;
    FklGrammer *g = *info->g;
    if (g) {
        info->ctx->cur_file_dir = info->dir;
        fklParseStateVectorPushBack2(stateStack,
                (FklParseState){ .state = &g->aTable.states[0] });
        list = fklReadWithAnalysisTable(g,
                fp,
                &size,
                info->curline,
                &info->curline,
                IGCVM,
                unexpectEOF,
                output_line,
                &begin,
                symbolStack,
                stateStack,
                info->ctx->lnt);
    } else {
        fklVMvaluePushState0ToStack(stateStack);
        list = fklReadWithBuiltinParser(fp,
                &size,
                info->curline,
                &info->curline,
                IGCVM,
                unexpectEOF,
                output_line,
                &begin,
                symbolStack,
                stateStack,
                info->ctx->lnt);
    }
    if (list)
        fklZfree(list);
    if (*unexpectEOF)
        begin = NULL;
    return begin;
}

#include <fakeLisp/grammer.h>

static int _codegen_load_get_next_expression(void *pcontext,
        FklPmatchRes *out,
        FklCgErrorState *error_state) {
    CgLoadCtx *context = pcontext;
    FklVMvalueCgInfo *info = context->info;
    FklParseStateVector *stateStack = &context->stateStack;
    FklAnalysisSymbolVector *symbolStack = &context->symbolStack;
    FILE *fp = context->fp;
    int unexpectEOF = 0;
    size_t output_line = 0;
    FklVMvalue *begin = getExpressionFromFile(info,
            fp,
            &unexpectEOF,
            &output_line,
            error_state,
            symbolStack,
            stateStack);
    if (unexpectEOF) {
        if (error_state->error == NULL)
            error_state->error = make_parse_error(IGCVM, unexpectEOF);
        error_state->line = output_line;
        return 0;
    }
    if (begin == NULL)
        return 0;

    FklVMvalue *contianer = fklCreateVMvalueBox(IGCVM, begin);
    put_line_number(info->ctx->lnt, contianer, output_line);
    *out = (FklPmatchRes){ .value = begin, .container = contianer };
    return 1;
}

static int hasLoadSameFile(const char *rpath, const FklVMvalueCgInfo *info) {
    for (; info; info = info->prev)
        if (info->realpath && !strcmp(rpath, info->realpath))
            return 1;
    return 0;
}

static const FklNextExpressionMethodTable
        _codegen_load_get_next_expression_method_table = {
            .get_next_exp = _codegen_load_get_next_expression,
            .finalize = _codegen_load_finalizer,
            .atomic = _codegen_load_atomic,
        };

static FklCgNextExp *createFpNextExpression(FILE *fp, FklVMvalueCgInfo *info) {
    CgLoadCtx *context = createCgLoadCtx(fp, info);
    return createCgNextExp(&_codegen_load_get_next_expression_method_table,
            context,
            0);
}

static void codegen_load(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *filename =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    if (!FKL_IS_STR(filename->value)) {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }

    if (rest->value != FKL_VM_NIL) {
        CgExpQueue *queue = cgExpQueueCreate();

        FklVMvalue *prev = FKL_VM_CDR(orig->value);
        FklVMvalue *head = FKL_VM_CAR(orig->value);

        for (FklVMvalue *rv = rest->value; FKL_IS_PAIR(rv);
                rv = FKL_VM_CDR(rv)) {
            FklVMvalue *new_rest = fklCreateVMvaluePair(GCVM, head, rv);
            put_line_number(info->ctx->lnt, new_rest, CURLINE(rv));
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
                info,
                actions);
    }

    const FklString *filenameStr = FKL_VM_STR(filename->value);
    if (!fklIsAccessibleRegFile(filenameStr->str)) {
        error_state->error = make_file_failure_error(GCVM, filename->value);
        error_state->line = CURLINE(filename->container);
        return;
    }

    FklVMvalueCgInfo *next_info = fklCreateVMvalueCgInfo(info->ctx,
            info,
            filenameStr->str,
            &(FklCgInfoArgs){ .inherit_grammer = 1 });

    if (hasLoadSameFile(next_info->realpath, info)) {
        error_state->error = make_circular_load_error(GCVM, filename->value);
        error_state->line = CURLINE(filename->container);
        return;
    }

    FILE *fp = fopen(filenameStr->str, "r");
    if (!fp) {
        error_state->error = make_file_failure_error(GCVM, filename->value);
        error_state->line = CURLINE(filename->container);
        return;
    }
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_begin_exp_bc_process,
            createDefaultStackContext(),
            createFpNextExpression(fp, next_info),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            next_info,
            actions);
}

static inline char *combineFileNameFromListAndCheckPrivate(
        const FklVMvalue *list) {
    char *r = NULL;
    for (const FklVMvalue *curPair = list; FKL_IS_PAIR(curPair);
            curPair = FKL_VM_CDR(curPair)) {
        FklVMvalue *cur = FKL_VM_CAR(curPair);
        const FklString *str = FKL_VM_SYM(cur);
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

static inline void export_compiler_macro(FklVMvalueCgMacroScope *macro_scope,
        FklVMvalue *pattern,
        FklVMvalue *bcl,
        uint32_t prototype_id,
        FklVMvalueCgInfo *lib_info) {
    add_compiler_macro(macro_scope->macros, pattern, bcl, prototype_id);

    if (lib_info) {
        add_compiler_macro(lib_info->export_macros, pattern, bcl, prototype_id);
    }
}

static inline void export_replacement(FklVMvalueCgMacroScope *macro_scope,
        FklVMvalue *k,
        FklVMvalue *v,
        FklVMvalueCgInfo *lib_info) {
    fklReplacementHashMapAdd2(macro_scope->replacements, k, v);
    if (lib_info) {
        fklReplacementHashMapAdd2(lib_info->export_replacement, k, v);
    }
}

static inline void export_symbol(FklVM *exe,
        FklVMvalue *k,
        const FklCgExportIdx *v,
        FklVMvalueCgEnv *env,
        FklVMvalue *bcl,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope,
        FklVMvalueCgInfo *lib_info) {
    append_import_ins(exe,
            INS_APPEND_BACK,
            bcl,
            fklAddCgDefBySid(k, scope, env)->idx,
            v->idx,
            fid,
            line,
            scope);
    if (lib_info == NULL)
        return;
    fklCgExportSidIdxHashMapPut2(&lib_info->exports, k, *v);
}

static void add_symbol_to_local_env_in_array(FklVM *exe,
        FklVMvalueCgEnv *env,
        const FklCgExportSidIdxHashMap *exports,
        FklVMvalue *bcl,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope,
        FklVMvalueCgInfo *lib_info) {
    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        export_symbol(exe, l->k, &l->v, env, bcl, fid, line, scope, lib_info);
    }
}

static void add_symbol_with_prefix_to_local_env_in_array(FklVM *exe,
        FklVMvalueCgEnv *env,
        const FklString *prefix,
        const FklCgExportSidIdxHashMap *exports,
        FklVMvalue *bcl,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope,
        FklCgCtx *ctx,
        FklVMvalueCgInfo *lib_info) {
    FklStrBuf buf;
    fklInitStrBuf(&buf);
    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        const FklString *origSymbol = FKL_VM_SYM(l->k);
        fklStrBufConcatWithString(&buf, prefix);
        fklStrBufConcatWithString(&buf, origSymbol);

        FklVMvalue *sym = add_symbol_char_buf(ctx, buf.buf, buf.index);
        fklStrBufClear(&buf);

        export_symbol(exe, sym, &l->v, env, bcl, fid, line, scope, lib_info);
    }
    fklUninitStrBuf(&buf);
}

static FklVMvalue *
replace_head_for_exp(FklVMvalue *orig, FklVMvalue *head, FklCgCtx *ctx) {
    FklVMvalue *r = fklCreateVMvaluePair(GCVM, head, FKL_VM_CDR(orig));
    return r;
}

static FklVMvalue *add_prefix_for_exp_head(FklVMvalue *orig,
        const FklString *prefix,
        FklVMvalueCgInfo *info) {
    const FklString *head = FKL_VM_SYM(FKL_VM_CAR(orig));
    FklString *name_with_prefix = fklStringAppend(prefix, head);

    FklVMvalue *pattern = replace_head_for_exp(orig,
            add_symbol(info->ctx, name_with_prefix),
            info->ctx);
    put_line_number(info->ctx->lnt, pattern, CURLINE(orig));
    fklZfree(name_with_prefix);
    return pattern;
}

static inline void export_replacement_with_prefix(FklCgCtx *ctx,
        FklVMvalueCgMacroScope *macros,
        const FklReplacementHashMap *replacements,
        const FklString *prefix,
        FklVMvalueCgInfo *lib_info) {
    for (FklReplacementHashMapNode *cur = replacements->first; cur;
            cur = cur->next) {
        const FklString *origSymbol = FKL_VM_SYM(cur->k);
        FklString *symbolWithPrefix = fklStringAppend(prefix, origSymbol);
        FklVMvalue *id = add_symbol(ctx, symbolWithPrefix);
        fklZfree(symbolWithPrefix);

        export_replacement(macros, id, cur->v, lib_info);
    }
}

typedef FklVMvalue *(*ImportLibCb)(uint32_t libId,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *args,
        FklCgErrorState *error_state,
        FklVMvalueCgInfo *lib_info);

typedef FklVMvalue *(*ImportDllCb)(FklVMvalue *orig,
        FklVMvalue *args,
        FklVMvalue *load_dll_bc,
        FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgInfo *info,
        FklCgErrorState *error_state,
        FklVMvalueCgInfo *lib_info);

typedef struct {
    FklVMvalueCgInfo *info;
    FklVMvalueCgEnv *env;
    FklVMvalueCgMacroScope *cms;
    FklVMvalueCgInfo *lib_info;

    FklVMvalue *import_cb_args;
    ImportLibCb import_cb;
    uint32_t scope;
} ExportContextData;

static void export_context_data_finalizer(void *data) {
    ExportContextData *d = (ExportContextData *)data;
    d->env = NULL;
    d->info = NULL;
    d->import_cb = NULL;
    d->import_cb_args = NULL;
}

static void export_context_data_atomic(FklVMgc *gc, void *ctx) {
    ExportContextData *d = ctx;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->cms), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->env), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->info), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->lib_info), gc);

    fklVMgcToGray(d->import_cb_args, gc);
}

static const FklCgActCtxMethodTable ExportContextMethodTable = {
    .size = sizeof(ExportContextData),
    .finalize = export_context_data_finalizer,
    .atomic = export_context_data_atomic,
};

static inline int merge_all_grammer(FklVMvalueCgInfo *info) {
    FklGrammer *g = *info->g;
    for (FklGraProdGroupHashMapNode *group_items = info->prod_groups->first;
            group_items;
            group_items = group_items->next) {
        if (fklMergeGrammer(g, &group_items->v.g, NULL)) {
            return 1;
        }
    }

    return 0;
}
static inline int add_all_group_to_grammer(uint64_t line,
        FklVMvalueCgInfo *info,
        FklCgErrorState *errors) {
    if (info->g == NULL)
        return 0;

    FklGrammer *g = *info->g;

    if (g == NULL)
        return 0;

    if (merge_all_grammer(info))
        return 1;

    FklGrammerNonterm nonterm = { 0 };
    if (fklCheckAndInitGrammerSymbols(g, &nonterm)) {
        FKL_ASSERT(nonterm.sid);
        FklVMvalue *place = NULL;
        if (nonterm.group) {
            FklVMvalue *n =
                    fklCreateVMvaluePair(IGCVM, nonterm.group, nonterm.sid);
            place = n;
        } else {
            place = nonterm.sid;
        }

        errors->error = make_import_reader_macro_error(IGCVM,
                "Undefined non-terminal",
                place);
        errors->fid = info->fid;
        return 1;
    }

    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fklLr0ToLalrItems(itemSet, g);
    FklStrBuf err_msg;
    fklInitStrBuf(&err_msg);

    int r = fklGenerateLalrAnalyzeTable(info->ctx->gc, g, itemSet, &err_msg);
    if (r) {
        errors->error = make_import_reader_macro_error(IGCVM, //
                err_msg.buf,
                NULL);
    }

    fklUninitStrBuf(&err_msg);
    fklLalrItemSetHashMapDestroy(itemSet);

    return r;
}

static inline int import_reader_macro(FklVMvalueCgInfo *info,
        FklCgErrorState *error_state,
        const FklGrammerProdGroupItem *group,
        FklVMvalue *origin_group_id,
        FklVMvalue *new_group_id) {

    FklVMgc *gc = info->ctx->gc;

    FklGrammerProdGroupItem *target_group =
            add_production_group(info->prod_groups, gc, new_group_id);

    merge_group(target_group,
            group,
            &(FklRecomputeGroupIdArgs){ .old_group_id = origin_group_id,
                .new_group_id = new_group_id });
    return 0;
}

static inline FklGrammer *init_builtin_grammer_and_prod_group(
        FklVMvalueCgInfo *info) {
    FklGrammer *g = *info->g;
    if (!g) {
        *info->g = fklCreateEmptyGrammer(IGCVM);
        fklGraProdGroupHashMapInit(info->prod_groups);
        g = *info->g;
        if (fklMergeGrammer(g, &info->ctx->builtin_g, NULL))
            FKL_UNREACHABLE();
    }

    return g;
}

FKL_NODISCARD
static inline int export_reader_macro(FklVMvalueCgInfo *info,
        const FklGrammerProdGroupItem *item,
        FklVMvalue *old_group_id,
        FklVMvalue *new_group_id,
        FklCgErrorState *errors,
        FklVMvalueCgInfo *lib_info) {
    init_builtin_grammer_and_prod_group(info);

    if (import_reader_macro(info, errors, item, old_group_id, new_group_id))
        return -1;

    if (lib_info == NULL)
        return 0;

    FklGrammerProdGroupItem *target_group =
            add_production_group(lib_info->export_prod_groups,
                    info->ctx->gc,
                    new_group_id);

    merge_group(target_group,
            item,
            &(FklRecomputeGroupIdArgs){
                .old_group_id = old_group_id,
                .new_group_id = new_group_id,
            });

    return 0;
}

static inline FklVMvalue *process_import_imported_lib_common(uint32_t libId,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *args,
        FklCgErrorState *errors,
        FklVMvalueCgInfo *lib_info) {
    const FklMacroHashMap *macros = lib->macros;
    const FklReplacementHashMap *replacements = lib->replacements;
    const FklGraProdGroupHashMap *named_prod_groups = lib->named_prod_groups;

    FKL_ASSERT(replacements);
    FKL_ASSERT(named_prod_groups);

    for (const FklMacroHashMapNode *cur = macros->first; cur; cur = cur->next) {
        for (const FklCgMacro *macro = cur->v; macro; macro = macro->next) {
            export_compiler_macro(macro_scope,
                    macro->pattern,
                    macro->bcl,
                    macro->prototype_id,
                    lib_info);
        }
    }

    for (FklReplacementHashMapNode *cur = replacements->first; cur;
            cur = cur->next) {
        export_replacement(macro_scope, cur->k, cur->v, lib_info);
    }

    for (FklGraProdGroupHashMapNode *prod_group_item = named_prod_groups->first;
            prod_group_item;
            prod_group_item = prod_group_item->next) {
        int r = export_reader_macro(info,
                &prod_group_item->v,
                prod_group_item->k,
                prod_group_item->k,
                errors,
                lib_info);
        if (r != 0)
            break;
    }

    if (errors->error != NULL)
        return NULL;

    if (add_all_group_to_grammer(curline, info, errors)) {
        errors->line = curline;
        return NULL;
    }

    FklVMvalue *load_lib = append_load_lib_ins(IGCVM,
            INS_APPEND_BACK,
            NULL,
            libId,
            info->fid,
            curline,
            scope);

    add_symbol_to_local_env_in_array(IGCVM,
            env,
            &lib->exports,
            load_lib,
            info->fid,
            curline,
            scope,
            lib_info);
    return load_lib;
}

static inline FklVMvalue *process_import_imported_lib_prefix(uint32_t libId,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *prefix_v,
        FklCgErrorState *error_state,
        FklVMvalueCgInfo *lib_info) {
    const FklMacroHashMap *macros = lib->macros;
    const FklReplacementHashMap *replacements = lib->replacements;
    const FklGraProdGroupHashMap *named_prod_groups = lib->named_prod_groups;

    FKL_ASSERT(replacements);
    FKL_ASSERT(named_prod_groups);

    FklCgCtx *ctx = info->ctx;
    const FklString *prefix = FKL_VM_SYM(prefix_v);

    for (const FklMacroHashMapNode *cur = macros->first; cur; cur = cur->next) {
        for (const FklCgMacro *macro = cur->v; macro; macro = macro->next) {
            FklVMvalue *pattern = add_prefix_for_exp_head(macro->pattern, //
                    prefix,
                    info);
            export_compiler_macro(macro_scope,
                    pattern,
                    macro->bcl,
                    macro->prototype_id,
                    lib_info);
        }
    }

    export_replacement_with_prefix(ctx,
            macro_scope,
            replacements,
            prefix,
            lib_info);

    FklStrBuf buffer;
    fklInitStrBuf(&buffer);
    for (FklGraProdGroupHashMapNode *prod_group_item = named_prod_groups->first;
            prod_group_item;
            prod_group_item = prod_group_item->next) {
        fklStrBufConcatWithString(&buffer, prefix);
        fklStrBufConcatWithString(&buffer, FKL_VM_SYM(prod_group_item->k));

        FklVMvalue *group_id_with_prefix =
                add_symbol_char_buf(ctx, buffer.buf, buffer.index);

        fklStrBufClear(&buffer);

        int r = export_reader_macro(info,
                &prod_group_item->v,
                prod_group_item->k,
                group_id_with_prefix,
                error_state,
                lib_info);
        if (r != 0)
            break;
    }
    fklUninitStrBuf(&buffer);

    if (!error_state->error
            && add_all_group_to_grammer(curline, info, error_state)) {
        error_state->line = curline;
        return NULL;
    }

    FklVMvalue *load_lib = append_load_lib_ins(GCVM,
            INS_APPEND_BACK,
            NULL,
            libId,
            info->fid,
            curline,
            scope);

    add_symbol_with_prefix_to_local_env_in_array(GCVM,
            env,
            prefix,
            &lib->exports,
            load_lib,
            info->fid,
            curline,
            scope,
            ctx,
            lib_info);

    return load_lib;
}

static inline FklVMvalue *process_import_imported_lib_only(uint32_t libId,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *only,
        FklCgErrorState *error_state,
        FklVMvalueCgInfo *lib_info) {
    FklVMvalue *load_lib = append_load_lib_ins(IGCVM,
            INS_APPEND_BACK,
            NULL,
            libId,
            info->fid,
            curline,
            scope);

    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    const FklMacroHashMap *macros = lib->macros;
    const FklReplacementHashMap *replacements = lib->replacements;
    const FklGraProdGroupHashMap *named_prod_groups = lib->named_prod_groups;

    FKL_ASSERT(replacements);
    FKL_ASSERT(named_prod_groups);

    for (; FKL_IS_PAIR(only); only = FKL_VM_CDR(only)) {
        FklVMvalue *cur = FKL_VM_CAR(only);
        int r = 0;

        FklCgMacro **pmacro = fklMacroHashMapGet2(macros, cur);
        if (pmacro != NULL) {
            r = 1;
            for (const FklCgMacro *macro = *pmacro; macro;
                    macro = macro->next) {
                export_compiler_macro(macro_scope,
                        macro->pattern,
                        macro->bcl,
                        macro->prototype_id,
                        lib_info);
            }
        }

        FklVMvalue *rep = fklGetReplacement(cur, replacements);
        if (rep) {
            r = 1;
            export_replacement(macro_scope, cur, rep, lib_info);
        }

        FklGrammerProdGroupItem *group =
                fklGraProdGroupHashMapGet2(named_prod_groups, cur);

        if (group) {
            r = 1;
            int e = export_reader_macro(info,
                    group,
                    cur,
                    cur,
                    error_state,
                    lib_info);
            if (e != 0)
                break;
        }

        FklCgExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);
        if (item) {
            export_symbol(IGCVM,
                    cur,
                    item,
                    env,
                    load_lib,
                    info->fid,
                    CURLINE(only),
                    scope,
                    lib_info);
        } else if (!r) {
            error_state->error = make_import_missing_error(IGCVM, cur);
            error_state->line = CURLINE(only);
            error_state->fid = add_symbol_cstr(info->ctx, info->filename);
            break;
        }
    }

    if (error_state->error)
        return NULL;

    if (add_all_group_to_grammer(curline, info, error_state)) {
        error_state->line = curline;
    }
    return load_lib;
}

static inline FklVMvalue *process_import_imported_lib_except(uint32_t libId,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *except,
        FklCgErrorState *error_state,
        FklVMvalueCgInfo *lib_info) {
    const FklMacroHashMap *macros = lib->macros;
    const FklReplacementHashMap *replacements = lib->replacements;
    const FklGraProdGroupHashMap *named_prod_groups = lib->named_prod_groups;

    FKL_ASSERT(replacements);
    FKL_ASSERT(named_prod_groups);

    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    FklValueHashSet excepts;
    fklValueHashSetInit(&excepts);
    FklVMvalue *load_lib = NULL;

    for (FklVMvalue *list = except; FKL_IS_PAIR(list); list = FKL_VM_CDR(list))
        fklValueHashSetPut2(&excepts, FKL_VM_CAR(list));

    for (const FklMacroHashMapNode *cur = macros->first; cur; cur = cur->next) {
        if (fklValueHashSetHas2(&excepts, cur->k))
            continue;

        for (const FklCgMacro *macro = cur->v; macro; macro = macro->next) {
            export_compiler_macro(macro_scope,
                    macro->pattern,
                    macro->bcl,
                    macro->prototype_id,
                    lib_info);
        }
    }

    for (const FklReplacementHashMapNode *reps = replacements->first; reps;
            reps = reps->next) {
        if (!fklValueHashSetHas2(&excepts, reps->k)) {
            export_replacement(macro_scope, reps->k, reps->v, lib_info);
        }
    }

    for (FklGraProdGroupHashMapNode *prod_group_item =
                    lib->named_prod_groups->first;
            prod_group_item;
            prod_group_item = prod_group_item->next) {
        if (!fklValueHashSetHas2(&excepts, prod_group_item->k)) {
            int r = export_reader_macro(info,
                    &prod_group_item->v,
                    prod_group_item->k,
                    prod_group_item->k,
                    error_state,
                    lib_info);
            if (r != 0)
                break;
        }
    }

    if (error_state->error) {
        load_lib = NULL;
        goto exit;
    }

    if (add_all_group_to_grammer(curline, info, error_state)) {
        error_state->line = curline;
        load_lib = NULL;
        goto exit;
    }

    load_lib = append_load_lib_ins(IGCVM,
            INS_APPEND_BACK,
            NULL,
            libId,
            info->fid,
            curline,
            scope);

    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        if (!fklValueHashSetHas2(&excepts, l->k)) {
            export_symbol(IGCVM,
                    l->k,
                    &l->v,
                    env,
                    load_lib,
                    info->fid,
                    CURLINE(except),
                    scope,
                    lib_info);
        }
    }

exit:
    fklValueHashSetUninit(&excepts);
    return load_lib;
}

static inline FklVMvalue *process_import_imported_lib_alias(uint32_t libId,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *alias,
        FklCgErrorState *error_state,
        FklVMvalueCgInfo *lib_info) {
    FklVMvalue *load_lib = append_load_lib_ins(IGCVM,
            INS_APPEND_BACK,
            NULL,
            libId,
            info->fid,
            curline,
            scope);

    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    const FklMacroHashMap *macros = lib->macros;
    const FklReplacementHashMap *replacements = lib->replacements;
    const FklGraProdGroupHashMap *named_prod_groups = lib->named_prod_groups;

    FKL_ASSERT(replacements);
    FKL_ASSERT(named_prod_groups);

    for (; FKL_IS_PAIR(alias); alias = FKL_VM_CDR(alias)) {
        FklVMvalue *cur_alias = FKL_VM_CAR(alias);
        FklVMvalue *cur_head = FKL_VM_CAR(cur_alias);
        FklVMvalue *cur_alias_sym = FKL_VM_CAR(FKL_VM_CDR(cur_alias));
        int r = 0;

        FklCgMacro **pmacro = fklMacroHashMapGet2(macros, cur_head);
        if (pmacro != NULL) {
            r = 1;
            for (const FklCgMacro *macro = *pmacro; macro;
                    macro = macro->next) {
                FklVMvalue *pattern = replace_head_for_exp(macro->pattern,
                        cur_alias_sym,
                        info->ctx);

                export_compiler_macro(macro_scope,
                        pattern,
                        macro->bcl,
                        macro->prototype_id,
                        lib_info);
            }
        }

        FklVMvalue *rep = fklGetReplacement(cur_head, replacements);
        if (rep) {
            r = 1;
            export_replacement(macro_scope, cur_alias_sym, rep, lib_info);
        }

        FklGrammerProdGroupItem *group =
                fklGraProdGroupHashMapGet2(named_prod_groups, cur_head);

        if (group != NULL) {
            r = 1;
            int e = export_reader_macro(info,
                    group,
                    cur_head,
                    cur_alias_sym,
                    error_state,
                    lib_info);
            if (e != 0)
                break;
        }

        FklCgExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur_head);
        if (item) {
            export_symbol(IGCVM,
                    cur_alias_sym,
                    item,
                    env,
                    load_lib,
                    info->fid,
                    CURLINE(alias),
                    scope,
                    lib_info);
        } else if (!r) {
            error_state->error = make_import_missing_error(IGCVM, cur_head);
            error_state->line = CURLINE(alias);
            error_state->fid = add_symbol_cstr(info->ctx, info->filename);
            break;
        }
    }

    if (error_state->error)
        return NULL;

    if (add_all_group_to_grammer(curline, info, error_state)) {
        error_state->line = curline;
    }
    return load_lib;
}

static inline FklVMvalue *is_exporting_outer_ref_group(FklVMvalueCgInfo *info) {
    for (FklGraProdGroupHashMapNode *cur = info->export_prod_groups->first; cur;
            cur = cur->next) {
        if ((cur->v.flags & FKL_GRAMMER_GROUP_HAS_OUTER_REF) != 0)
            return cur->k;
    }
    return NULL;
}

static inline int has_undefined_non_terminal(FklVMvalueCgInfo *info,
        FklGrammerNonterm *nt) {
    for (FklGraProdGroupHashMapNode *cur = info->export_prod_groups->first; cur;
            cur = cur->next) {
        if (fklCheckUndefinedNonterm(&cur->v.g, nt))
            return 1;
    }
    return 0;
}

static inline void process_export_bc(FklVMvalueCgInfo *info,
        FklVMvalue *libBc,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    info->epc = FKL_VM_CO(libBc)->bc.len;

    append_export_to_ins(IGCVM,
            INS_APPEND_BACK,
            libBc,
            info->libraries->size + 1,
            info->exports.count,
            fid,
            line,
            scope);
    for (const FklCgExportSidIdxHashMapNode *l = info->exports.first; l;
            l = l->next) {
        append_export_ins(IGCVM,
                INS_APPEND_BACK,
                libBc,
                l->v.oidx,
                fid,
                line,
                scope);
    }

    FklInstruction ret = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(FKL_VM_CO(libBc), &ret, fid, line, scope);
}

static FklVMvalue *_library_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;
    FklCgErrorState *errors = args->error_state;

    FklVMvalue *group_id = is_exporting_outer_ref_group(info);
    if (group_id) {
        errors->error = make_export_error(GCVM,
                "Group %S has reference to other group",
                group_id);
        errors->line = line;
        return NULL;
    }

    FklGrammerNonterm nt = { .group = NULL, .sid = NULL };
    if (has_undefined_non_terminal(info, &nt)) {

        FklVMvalue *place = NULL;
        if (nt.group) {
            FklVMvalue *n = fklCreateVMvaluePair(IGCVM, nt.group, nt.sid);
            place = n;
        } else {
            place = nt.sid;
        }

        errors->error = make_export_error(IGCVM, //
                "Undefined non-terminal",
                place);
        errors->fid = info->fid;
        errors->line = info->curline;
        return NULL;
    }

    fklUpdatePrototype(&info->pts->p, env);
    fklPrintUndefinedRef(env, &info->ctx->gc->err_out);

    ExportContextData *d = FKL_TYPE_CAST(ExportContextData *, data);
    FklVMvalue *co = sequnce_exp_bc_process(GCVM, bcl_vec, fid, line, scope);

    FklInstruction ret = create_op_ins(FKL_OP_RET);

    fklByteCodeLntPushBackIns(FKL_VM_CO(co), &ret, fid, line, scope);

    if (!env->is_debugging)
        fklPeepholeOptimize(FKL_VM_CO(co));

    process_export_bc(info, co, fid, line, scope);

    FklCgLib *lib = fklCgLibVectorPushBack(info->libraries, NULL);
    fklInitCgScriptLib(lib, info, co, info->epc, env);

    info->realpath = NULL;

    info->export_macros = NULL;
    info->export_replacement = NULL;
    return d->import_cb(info->libraries->size,
            d->info,
            lib,
            d->env,
            d->scope,
            d->cms,
            line,
            d->import_cb_args,
            errors,
            d->lib_info);
}

static FklCgActCtx *createExportContext(FklVMvalueCgInfo *info,
        FklVMvalueCgEnv *targetEnv,
        uint32_t scope,
        FklVMvalueCgMacroScope *cms,
        ImportLibCb import_cb,
        FklVMvalue *import_cb_args,
        FklVMvalueCgInfo *lib_info) {
    FklCgActCtx *r = createCgActCtx(&ExportContextMethodTable);
    ExportContextData *data = FKL_TYPE_CAST(ExportContextData *, r->d);

    data->info = info;

    data->scope = scope;
    data->env = targetEnv;

    data->cms = cms;

    data->import_cb = import_cb;
    data->import_cb_args = import_cb_args;

    data->lib_info = lib_info;
    return r;
}

static inline FklVMvalue *export_sequnce_exp_bc_process(FklVM *exe,
        FklValueVector *stack,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    if (stack->size) {
        FklInstruction drop = create_op_ins(FKL_OP_DROP);
        FklVMvalue *retval = create_0len_bcl(exe);
        size_t top = stack->size;
        for (size_t i = 0; i < top; i++) {
            FklVMvalue *cur = stack->base[i];
            if (FKL_VM_CO(cur)->bc.len) {
                fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
                if (i < top - 1)
                    fklByteCodeLntPushBackIns(FKL_VM_CO(retval),
                            &drop,
                            fid,
                            line,
                            scope);
            }
        }
        stack->size = 0;
        return retval;
    }
    return NULL;
}

static inline FklVMvalueCgInfo *get_lib_info(FklVMvalueCgInfo *lib_info) {
    for (; lib_info && !lib_info->is_lib; lib_info = lib_info->prev)
        if (lib_info->is_macro)
            return NULL;
    return lib_info;
}

typedef struct {
    FklPmatchRes orig;
    uint8_t must_has_retval;
} ExportSequnceContextData;

static FklVMvalue *exports_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;
    FklCgErrorState *error_state = args->error_state;

    ExportSequnceContextData *d =
            FKL_TYPE_CAST(ExportSequnceContextData *, data);
    FklVMvalue *bcl =
            export_sequnce_exp_bc_process(GCVM, bcl_vec, fid, line, scope);
    if (d->must_has_retval && !bcl) {
        error_state->error = make_has_no_value_error(GCVM, d->orig.value);
        error_state->line = CURLINE(d->orig.container);
    }
    return bcl;
}

static void codegen_export_none(const CgCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    if (must_has_retval) {
        error_state->error = make_has_no_value_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    FklVMvalueCgInfo *lib_info = get_lib_info(info);

    if (lib_info && scope == 1 && env->prev == info->global_env
            && macro_scope->prev == info->global_env->macros) {
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_empty_bc_process,
                createDefaultStackContext(),
                NULL,
                scope,
                macro_scope,
                env,
                CURLINE(orig->container),
                info,
                actions);
    } else {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
}

static void export_sequnce_context_data_atomic(FklVMgc *gc, void *data) {
    ExportSequnceContextData *d = (ExportSequnceContextData *)data;
    fklVMgcToGray(d->orig.value, gc);
    fklVMgcToGray(d->orig.container, gc);
}

static const FklCgActCtxMethodTable ExportSequnceContextMethodTable = {
    .size = sizeof(ExportSequnceContextData),
    .atomic = export_sequnce_context_data_atomic,
};

static FklCgActCtx *create_export_sequnce_context(const FklPmatchRes *orig,
        uint8_t must_has_retval) {
    FklCgActCtx *r = createCgActCtx(&ExportSequnceContextMethodTable);
    ExportSequnceContextData *data =
            FKL_TYPE_CAST(ExportSequnceContextData *, r->d);

    data->must_has_retval = must_has_retval;
    data->orig = *orig;
    return r;
}

static void codegen_export(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    FklVMvalueCgInfo *lib_info = get_lib_info(info);

    if (lib_info && scope == 1 && env->prev == info->global_env
            && macro_scope->prev == info->global_env->macros) {
        CgExpQueue *exportQueue = cgExpQueueCreate();
        const FklPmatchRes *rest =
                fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
        add_export_symbol(lib_info, orig->value, rest->value, exportQueue);

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(exports_bc_process,
                create_export_sequnce_context(orig, must_has_retval),
                createDefaultQueueNextExpression(exportQueue),
                scope,
                macro_scope,
                env,
                CURLINE(orig->container),
                info,
                actions);
    } else {
        error_state->error = make_syntax_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
}

typedef struct {
    FklVMvalue *id;
    FklCgExportIdx *item;
} ExportDefineContext;

static const FklCgActCtxMethodTable ExportDefineContextMethodTable = {
    .size = sizeof(ExportDefineContext),
};

static inline FklCgActCtx *create_export_define_context(FklVMvalue *id,
        FklCgExportIdx *item) {
    FklCgActCtx *r = createCgActCtx(&ExportDefineContextMethodTable);
    ExportDefineContext *ctx = FKL_TYPE_CAST(ExportDefineContext *, r->d);
    ctx->id = id;
    ctx->item = item;
    return r;
}

static FklVMvalue *_export_define_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    ExportDefineContext *exp_ctx = FKL_TYPE_CAST(ExportDefineContext *, data);
    exp_ctx->item->oidx =
            fklGetCgDefByIdInScope(exp_ctx->id, 1, env->scopes)->v.idx;
    return sequnce_exp_bc_process(GCVM, bcl_vec, fid, line, scope);
}

static size_t check_loaded_lib(const char *realpath,
        FklCgLibVector *loaded_libraries) {
    for (size_t i = 0; i < loaded_libraries->size; i++) {
        const FklCgLib *cur = &loaded_libraries->base[i];
        if (!strcmp(realpath, cur->rp))
            return i + 1;
    }
    return 0;
}

typedef int (*ImportLibCbCheck)(const FklVMvalue *args);

typedef struct {
    FklVMvalue *name;
    FklVMvalue *rest;

    ImportLibCbCheck import_check_cb;
    ImportLibCb import_cb;
    ImportDllCb import_dll_cb;
    FklVMvalue *import_cb_args;
} CgImportHelperArgs;

static inline void process_import_script_common_header(const CgCbArgs *args,
        const CgImportHelperArgs *import_args,
        const char *filename,
        FklVMvalueCgInfo *lib_info) {

    FklVMvalue *orig = args->orig->value;
    FklVMvalueCgInfo *info = args->info;
    FklCgErrorState *errors = args->err_state;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklCgActVector *actions = args->actions;

    FklVMvalue *name = import_args->name;

    FklVMvalueCgInfo *next_info = fklCreateVMvalueCgInfo(info->ctx,
            info,
            filename,
            &(FklCgInfoArgs){
                .is_lib = 1,
            });

    if (hasLoadSameFile(next_info->realpath, info)) {
        errors->error = make_circular_load_error(IGCVM, name);
        errors->line = CURLINE(orig);
        return;
    }
    size_t libId = check_loaded_lib(next_info->realpath, info->libraries);
    if (!libId) {
        FILE *fp = fopen(filename, "r");
        if (!fp) {
            errors->error = make_file_failure_error(IGCVM, name);
            errors->line = CURLINE(orig);
            return;
        }
        FklVMvalueCgEnv *libEnv = fklCreateVMvalueCgEnv(info->ctx,
                info->global_env,
                0,
                info->global_env->macros);
        create_and_insert_to_pool(next_info, 0, libEnv, 0, 1);

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_library_bc_process,
                createExportContext(info,
                        env,
                        scope,
                        macro_scope,
                        import_args->import_cb,
                        import_args->import_cb_args,
                        lib_info),
                createFpNextExpression(fp, next_info),
                1,
                libEnv->macros,
                libEnv,
                CURLINE(orig),
                next_info,
                actions);
    } else {
        fklReplacementHashMapDestroy(next_info->export_replacement);
        next_info->export_replacement = NULL;
        const FklCgLib *lib = &info->libraries->base[libId - 1];

        FklVMvalue *importBc = import_args->import_cb(libId,
                info,
                lib,
                env,
                scope,
                macro_scope,
                CURLINE(orig),
                import_args->import_cb_args,
                errors,
                lib_info);

        FklCgAct *cur = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                NULL,
                1,
                info->global_env->macros,
                info->global_env,
                CURLINE(orig),
                NULL,
                next_info);

        fklValueVectorPushBack2(&cur->bcl_vector, importBc);
        fklCgActVectorPushBack2(actions, cur);
    }
}

FKL_NODISCARD
static inline int import_pre_compiler_impl(const CgCbArgs *args,
        const CgImportHelperArgs *import_args,
        const char *filename,
        FklVMvalueCgInfo *lib_info) {
    FklVMvalue *orig = args->orig->value;
    FklVMvalueCgInfo *info = args->info;
    FklCgErrorState *errors = args->err_state;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklCgActVector *actions = args->actions;

    FklVMvalue *name = import_args->name;

    size_t libId = check_loaded_lib(filename, info->libraries);
    if (!libId) {
        FILE *fp = fopen(filename, "rb");
        if (fp == NULL) {
            errors->error = make_import_failed_error(IGCVM, name);
            errors->line = CURLINE(orig);
            return -1;
        }

        FklCgCtx *ctx = info->ctx;

        FklLoadPreCompileArgs args = {
            .ctx = ctx,
            .libraries = info->libraries,
            .macro_libraries = &ctx->macro_libraries,
            .pts = info->pts,
            .macro_pts = ctx->macro_pts,
        };

        if (fklLoadPreCompile(fp, filename, &args)) {
            if (args.error) {
                errors->error =
                        make_import_failed_error2(IGCVM, args.error, name);
                fklZfree(args.error);
                args.error = NULL;
            } else {
                errors->error = make_import_failed_error(IGCVM, name);
            }

            errors->line = CURLINE(orig);
            return -1;
        }

        libId = info->libraries->size;
    }
    const FklCgLib *lib = &info->libraries->base[libId - 1];

    FklVMvalue *importBc = import_args->import_cb(libId,
            info,
            lib,
            env,
            scope,
            macro_scope,
            CURLINE(orig),
            import_args->import_cb_args,
            errors,
            lib_info);
    FklCgAct *action = create_cg_action(_default_bc_process,
            createDefaultStackContext(),
            NULL,
            1,
            info->global_env->macros,
            info->global_env,
            CURLINE(orig),
            NULL,
            info);
    fklValueVectorPushBack2(&action->bcl_vector, importBc);
    fklCgActVectorPushBack2(actions, action);

    return 0;
}

static inline size_t load_dll(FklVMvalue *orig,
        FklVMvalue *name,
        const char *filename,
        FklVMvalueCgInfo *info,
        FklCgErrorState *errors) {
    char *realpath = fklRealpath(filename);
    size_t lib_id = check_loaded_lib(realpath, info->libraries);
    if (lib_id != 0) {
        fklZfree(realpath);
        return lib_id;
    }

    uv_lib_t dll = { 0 };
    if (uv_dlopen(realpath, &dll)) {
        const char *msg = uv_dlerror(&dll);
        errors->error = make_file_failure_error2(IGCVM, msg, name);
        errors->line = CURLINE(orig);
        fklZfree(realpath);
        uv_dlclose(&dll);
        return 0;
    }
    FklCgDllLibInitExportCb initExport = fklGetCgInitExportFunc(&dll);
    if (!initExport) {
        uv_dlclose(&dll);
        errors->error = make_import_failed_error(IGCVM, name);
        errors->line = CURLINE(orig);
        fklZfree(realpath);
        return 0;
    }
    FklCgLib *lib = fklCgLibVectorPushBack(info->libraries, NULL);
    fklInitCgDllLib(info->ctx, lib, realpath, dll, initExport);
    lib_id = info->libraries->size;

    return lib_id;
}

FKL_NODISCARD
static inline int import_dll_impl(const CgCbArgs *args,
        const CgImportHelperArgs *import_args,
        const char *filename,
        FklVMvalueCgInfo *lib_info) {
    FklVMvalue *orig = args->orig->value;
    FklVMvalueCgInfo *info = args->info;
    FklCgErrorState *errors = args->err_state;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklCgActVector *actions = args->actions;

    FklVMvalue *name = import_args->name;

    size_t lib_id = load_dll(orig, name, filename, info, errors);

    if (lib_id == 0)
        return -1;

    FklVMvalue *load_dll_bc = append_load_dll_ins(IGCVM,
            INS_APPEND_BACK,
            NULL,
            lib_id,
            info->fid,
            CURLINE(orig),
            scope);

    FklVMvalue *bc = import_args->import_dll_cb(orig,
            import_args->import_cb_args,
            load_dll_bc,
            &info->libraries->base[lib_id - 1],
            env,
            scope,
            info,
            errors,
            lib_info);

    if (bc) {
        FklCgAct *action = create_cg_action(_default_bc_process,
                createDefaultStackContext(),
                NULL,
                1,
                NULL,
                NULL,
                CURLINE(orig),
                NULL,
                info);
        fklValueVectorPushBack2(&action->bcl_vector, bc);
        fklCgActVectorPushBack2(actions, action);
    }

    return 0;
}

FklCgDllLibInitExportCb fklGetCgInitExportFunc(uv_lib_t *dll) {
    return (FklCgDllLibInitExportCb)fklGetAddress("_fklExportSymbolInit", dll);
}

static inline FklVMvalue *process_import_from_dll_only(FklVMvalue *orig,
        FklVMvalue *only,
        FklVMvalue *load_dll_bc,
        FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgInfo *info,
        FklCgErrorState *error_state,
        FklVMvalueCgInfo *lib_info) {
    FklCgExportSidIdxHashMap *exports = &lib->exports;

    for (; FKL_IS_PAIR(only); only = FKL_VM_CDR(only)) {
        FklVMvalue *cur = FKL_VM_CAR(only);
        FklCgExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);
        if (item) {
            export_symbol(IGCVM,
                    cur,
                    item,
                    env,
                    load_dll_bc,
                    info->fid,
                    CURLINE(only),
                    scope,
                    lib_info);
        } else {
            error_state->error = make_import_missing_error(IGCVM, cur);
            error_state->line = CURLINE(only);
            error_state->fid = add_symbol_cstr(info->ctx, info->filename);
            break;
        }
    }

    return load_dll_bc;
}

static inline FklVMvalue *process_import_from_dll_except(FklVMvalue *orig,
        FklVMvalue *except,
        FklVMvalue *load_dll_bc,
        FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgInfo *info,
        FklCgErrorState *error_state,
        FklVMvalueCgInfo *lib_info) {
    FklCgExportSidIdxHashMap *exports = &lib->exports;
    FklValueHashSet excepts;
    fklValueHashSetInit(&excepts);

    for (FklVMvalue *list = except; FKL_IS_PAIR(list); list = FKL_VM_CDR(list))
        fklValueHashSetPut2(&excepts, FKL_VM_CAR(list));

    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        if (fklValueHashSetHas2(&excepts, l->k))
            continue;
        export_symbol(IGCVM,
                l->k,
                &l->v,
                env,
                load_dll_bc,
                info->fid,
                CURLINE(except),
                scope,
                lib_info);
    }

    fklValueHashSetUninit(&excepts);
    return load_dll_bc;
}

static inline FklVMvalue *process_import_from_dll_common(FklVMvalue *orig,
        FklVMvalue *args,
        FklVMvalue *load_dll_bc,
        FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgInfo *info,
        FklCgErrorState *error_state,
        FklVMvalueCgInfo *lib_info) {
    add_symbol_to_local_env_in_array(IGCVM,
            env,
            &lib->exports,
            load_dll_bc,
            info->fid,
            CURLINE(orig),
            scope,
            lib_info);

    return load_dll_bc;
}

static inline FklVMvalue *process_import_from_dll_prefix(FklVMvalue *orig,
        FklVMvalue *prefix_v,
        FklVMvalue *load_dll_bc,
        FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgInfo *info,
        FklCgErrorState *error_state,
        FklVMvalueCgInfo *lib_info) {
    const FklString *prefix = FKL_VM_SYM(prefix_v);

    add_symbol_with_prefix_to_local_env_in_array(IGCVM,
            env,
            prefix,
            &lib->exports,
            load_dll_bc,
            info->fid,
            CURLINE(orig),
            scope,
            info->ctx,
            lib_info);

    return load_dll_bc;
}

static inline FklVMvalue *process_import_from_dll_alias(FklVMvalue *orig,
        FklVMvalue *alias,
        FklVMvalue *load_dll_bc,
        FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgInfo *info,
        FklCgErrorState *error_state,
        FklVMvalueCgInfo *lib_info) {
    FklCgExportSidIdxHashMap *exports = &lib->exports;

    for (; FKL_IS_PAIR(alias); alias = FKL_VM_CDR(alias)) {
        FklVMvalue *cur_alias = FKL_VM_CAR(alias);
        FklVMvalue *cur = FKL_VM_CAR(cur_alias);
        FklCgExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur);

        if (item) {
            FklVMvalue *cur0 = FKL_VM_CAR(FKL_VM_CDR(cur_alias));
            export_symbol(IGCVM,
                    cur0,
                    item,
                    env,
                    load_dll_bc,
                    info->fid,
                    CURLINE(alias),
                    scope,
                    lib_info);
        } else {
            error_state->error = make_import_missing_error(IGCVM, cur);
            error_state->line = CURLINE(alias);
            error_state->fid = add_symbol_cstr(info->ctx, info->filename);
            break;
        }
    }

    return load_dll_bc;
}

static inline int is_valid_alias_sym_list(const FklVMvalue *alias) {
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

static inline void codegen_import_helper(const CgCbArgs *args,
        const CgImportHelperArgs *import_args,
        FklVMvalueCgInfo *lib_info) {
    FKL_ASSERT(import_args);

    FklVMvalue *orig = args->orig->value;
    FklVMvalueCgInfo *info = args->info;
    FklCgErrorState *errors = args->err_state;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklCgActVector *actions = args->actions;

    FklVMvalue *name = import_args->name;
    FklVMvalue *rest = import_args->rest;

    if (name == FKL_VM_NIL || !is_symbol_list(name)) {
        errors->error = make_syntax_error(IGCVM, orig);
        errors->line = CURLINE(orig);
        return;
    }

    if (import_args->import_check_cb != NULL
            && !import_args->import_check_cb(import_args->import_cb_args)) {
        errors->error = make_syntax_error(IGCVM, import_args->import_cb_args);
        errors->line = CURLINE(orig);
        return;
    }

    if (rest != FKL_VM_NIL) {
        CgExpQueue *queue = cgExpQueueCreate();

        FklVMvalue *prev = FKL_VM_CDR(orig);

        FklVMvalue *head = FKL_VM_CAR(orig);

        for (; FKL_IS_PAIR(rest); rest = FKL_VM_CDR(rest)) {
            FklVMvalue *new_rest = fklCreateVMvaluePair(IGCVM, head, rest);
            put_line_number(info->ctx->lnt, new_rest, CURLINE(rest));
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
                info,
                actions);
    }

    char *filename = combineFileNameFromListAndCheckPrivate(name);

    if (filename == NULL) {
        errors->error = make_import_failed_error(IGCVM, name);
        errors->line = CURLINE(orig);
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

    if (fklIsAccessibleRegFile(packageMainFileName)) {
        process_import_script_common_header(args,
                import_args,
                packageMainFileName,
                lib_info);
    } else if (fklIsAccessibleRegFile(scriptFileName)) {
        process_import_script_common_header(args,
                import_args,
                scriptFileName,
                lib_info);
    } else if (fklIsAccessibleRegFile(preCompileFileName)) {
        int r = import_pre_compiler_impl(args,
                import_args,
                preCompileFileName,
                lib_info);
        if (r != 0)
            goto exit;
    } else if (fklIsAccessibleRegFile(dllFileName)) {
        int r = import_dll_impl(args, import_args, dllFileName, lib_info);
        if (r != 0)
            goto exit;
    } else {
        errors->error = make_import_failed_error(IGCVM, name);
        errors->line = CURLINE(orig);
    }
exit:
    fklZfree(filename);
    fklZfree(scriptFileName);
    fklZfree(dllFileName);
    fklZfree(packageMainFileName);
    fklZfree(preCompileFileName);
}

static void codegen_import_impl(const CgCbArgs *args,
        const CgImportHelperArgs *import_args,
        FklVMvalueCgInfo *lib_info) {
    if (import_args != NULL && import_args->name != NULL) {
        codegen_import_helper(args, import_args, lib_info);
        return;
    }

    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *errors = args->err_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    if (must_has_retval) {
        errors->error = make_has_no_value_error(GCVM, orig->value);
        errors->line = CURLINE(orig->container);
        return;
    }

    fklCgActVectorPushBack2(actions,
            create_cg_action(_empty_bc_process,
                    createDefaultStackContext(),
                    NULL,
                    1,
                    macro_scope,
                    env,
                    CURLINE(orig->container),
                    NULL,
                    info));
}

static void codegen_import_none_impl(const CgCbArgs *args,
        FklVMvalueCgInfo *lib_info) {
    codegen_import_impl(args, NULL, lib_info);
}

static void codegen_import_none(const CgCbArgs *args) {
    codegen_import_none_impl(args, NULL);
}

static void codegen_import_common_impl(const CgCbArgs *args,
        FklVMvalueCgInfo *lib_info) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);

    const CgImportHelperArgs import_args = {
        .name = name->value,
        .rest = rest->value,

        .import_cb_args = NULL,
        .import_cb = process_import_imported_lib_common,
        .import_dll_cb = process_import_from_dll_common,
    };

    codegen_import_impl(args, &import_args, lib_info);
}

static void codegen_import_prefix_impl(const CgCbArgs *args,
        FklVMvalueCgInfo *lib_info) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *pref = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);

    const CgImportHelperArgs import_args = {
        .name = name->value,
        .rest = rest->value,

        .import_check_cb = FKL_IS_SYM,
        .import_cb = process_import_imported_lib_prefix,
        .import_dll_cb = process_import_from_dll_prefix,
        .import_cb_args = pref->value,
    };

    codegen_import_impl(args, &import_args, lib_info);
}

static void codegen_import_prefix(const CgCbArgs *args) {
    codegen_import_prefix_impl(args, NULL);
}

static void codegen_import_only_impl(const CgCbArgs *args,
        FklVMvalueCgInfo *lib_info) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *only = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);

    const CgImportHelperArgs import_args = {
        .name = name->value,
        .rest = rest->value,

        .import_check_cb = is_symbol_list,
        .import_cb = process_import_imported_lib_only,
        .import_dll_cb = process_import_from_dll_only,
        .import_cb_args = only->value,
    };

    codegen_import_impl(args, &import_args, lib_info);
}

static void codegen_import_only(const CgCbArgs *args) {
    codegen_import_only_impl(args, NULL);
}

static void codegen_import_alias_impl(const CgCbArgs *args,
        FklVMvalueCgInfo *lib_info) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *alias = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);

    const CgImportHelperArgs import_args = {
        .name = name->value,
        .rest = rest->value,

        .import_check_cb = is_valid_alias_sym_list,
        .import_cb = process_import_imported_lib_alias,
        .import_dll_cb = process_import_from_dll_alias,
        .import_cb_args = alias->value,
    };

    codegen_import_impl(args, &import_args, lib_info);
}

static void codegen_import_alias(const CgCbArgs *args) {
    codegen_import_alias_impl(args, NULL);
}

static void codegen_import_except_impl(const CgCbArgs *args,
        FklVMvalueCgInfo *lib_info) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *exce = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);

    const CgImportHelperArgs import_args = {
        .name = name->value,
        .rest = rest->value,

        .import_check_cb = is_symbol_list,
        .import_cb = process_import_imported_lib_except,
        .import_dll_cb = process_import_from_dll_except,
        .import_cb_args = exce->value,
    };

    codegen_import_impl(args, &import_args, lib_info);
}

static void codegen_import_except(const CgCbArgs *args) {
    codegen_import_except_impl(args, NULL);
}

static void codegen_import_common(const CgCbArgs *args) {
    codegen_import_common_impl(args, NULL);
}

typedef struct {
    FklVMvalue *pattern;
    FklVMvalueCgMacroScope *macro_scope;
    FklVMvalueCgInfo *lib_info;
    uint32_t prototype_id;
} MacroContext;

static inline void init_macro_context(MacroContext *r,
        FklVMvalue *pattern,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalueCgInfo *lib_info,
        uint32_t prototype_id) {
    r->pattern = pattern;
    r->macro_scope = macro_scope;
    r->lib_info = lib_info;
    r->prototype_id = prototype_id;
}

static FklVMvalue *compiler_macro_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    fklUpdatePrototype(&info->pts->p, env);
    fklPrintUndefinedRef(env->prev, &info->ctx->gc->err_out);

    MacroContext *d = FKL_TYPE_CAST(MacroContext *, data);
    FklVMvalue *macroBcl = *fklValueVectorPopBackNonNull(bcl_vec);
    FklInstruction ret = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(FKL_VM_CO(macroBcl), &ret, fid, line, scope);

    FklVMvalue *pattern = d->pattern;
    FklVMvalueCgMacroScope *macros = d->macro_scope;
    uint32_t prototype_id = d->prototype_id;

    fklPeepholeOptimize(FKL_VM_CO(macroBcl));
    add_compiler_macro(macros->macros, pattern, macroBcl, prototype_id);

    if (d->lib_info) {
        add_compiler_macro(d->lib_info->export_macros,
                pattern,
                macroBcl,
                prototype_id);
    }
    return NULL;
}

static void _macro_stack_context_atomic(FklVMgc *gc, void *data) {
    MacroContext *d = data;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->macro_scope), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->lib_info), gc);
    fklVMgcToGray(d->pattern, gc);
}

static const FklCgActCtxMethodTable MacroStackContextMethodTable = {
    .size = sizeof(MacroContext),
    .atomic = _macro_stack_context_atomic,
};

static inline FklCgActCtx *createMacroActionContext(FklVMvalue *origin_exp,
        FklVMvalue *pattern,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalueCgInfo *lib_info,
        uint32_t prototype_id) {
    FklCgActCtx *r = createCgActCtx(&MacroStackContextMethodTable);
    init_macro_context(FKL_TYPE_CAST(MacroContext *, r->d),
            pattern,
            macro_scope,
            lib_info,
            prototype_id);
    return r;
}

struct ReaderMacroCtx {
    FklVMvalueCustomActCtx *action_ctx;
};

static inline void init_reader_macro_context(struct ReaderMacroCtx *r,
        FklVMvalueCustomActCtx *ctx) {
    r->action_ctx = ctx;
}

static FklVMvalue *_reader_macro_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    struct ReaderMacroCtx *d = FKL_TYPE_CAST(struct ReaderMacroCtx *, data);
    FklVMvalueCustomActCtx *custom_ctx = d->action_ctx;
    d->action_ctx = NULL;

    fklUpdatePrototype(&info->pts->p, env);
    fklPrintUndefinedRef(env->prev, &info->ctx->gc->err_out);

    FklVMvalue *macroBcl = *fklValueVectorPopBackNonNull(bcl_vec);
    FklInstruction ret = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(FKL_VM_CO(macroBcl), &ret, fid, line, scope);

    fklPeepholeOptimize(FKL_VM_CO(macroBcl));
    custom_ctx->bcl = macroBcl;
    return NULL;
}

static void _reader_macro_stack_context_atomic(FklVMgc *gc, void *data) {
    struct ReaderMacroCtx *d = (struct ReaderMacroCtx *)data;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->action_ctx), gc);
}

static const FklCgActCtxMethodTable ReaderMacroStackContextMethodTable = {
    .size = sizeof(struct ReaderMacroCtx),
    .atomic = _reader_macro_stack_context_atomic,
};

static inline FklCgActCtx *createReaderMacroActionContext(
        FklVMvalueCustomActCtx *ctx) {
    FklCgActCtx *r = createCgActCtx(&ReaderMacroStackContextMethodTable);

    init_reader_macro_context(FKL_TYPE_CAST(struct ReaderMacroCtx *, r->d),
            ctx);

    return r;
}

static FklVMvalue *_nil_replacement(const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    return FKL_VM_NIL;
}

static FklVMvalue *_is_main_replacement(const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    if (env->prototypeId == 1)
        return FKL_VM_TRUE;
    else
        return FKL_VM_NIL;
}

static FklVMvalue *_platform_replacement(const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {

#if __linux__
    static const char *platform = "linux";
#elif __unix__
    static const char *platform = "unix";
#elif defined _WIN32
    static const char *platform = "win32";
#elif __APPLE__
    static const char *platform = "apple";
#endif
    return fklCreateVMvalueStrFromCstr(IGCVM, platform);
}

static FklVMvalue *_file_dir_replacement(const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    const char *d = info->filename == NULL //
                          ? info->ctx->cwd
                          : info->dir;

    const size_t d_len = strlen(d);
    const size_t s_len = strlen(FKL_PATH_SEPARATOR_STR);

    FklVMvalue *r = fklCreateVMvalueStr2(IGCVM, s_len + d_len, NULL);
    memcpy(FKL_VM_STR(r)->str, d, d_len);
    strcat(FKL_VM_STR(r)->str, FKL_PATH_SEPARATOR_STR);

    return r;
}

static FklVMvalue *_file_replacement(const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    return info->filename == NULL
                 ? FKL_VM_NIL
                 : fklCreateVMvalueStrFromCstr(IGCVM, info->filename);
}

static FklVMvalue *_file_rp_replacement(const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    return info->realpath == NULL
                 ? FKL_VM_NIL
                 : fklCreateVMvalueStrFromCstr(IGCVM, info->realpath);
}

static FklVMvalue *_line_replacement(const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    uint64_t line = CURLINE(orig->container);
    return line <= FKL_FIX_INT_MAX ? FKL_MAKE_VM_FIX(line)
                                   : fklCreateVMvalueBigIntWithU64(IGCVM, line);
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
        s->nt.group = car;
        s->nt.sid = cdr;
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

static inline FklVMvalueCgInfo *macro_compile_prepare(FklVMvalueCgInfo *info,
        FklVMvalueCgMacroScope *macro_scope,
        FklValueHashSet *symbol_set,
        FklVMvalueCgEnv **penv) {
    FklVMvalueCgInfo *macro_info = fklCreateVMvalueCgInfo(info->ctx,
            info,
            NULL,
            &(FklCgInfoArgs){
                .is_macro = 1,
                .macro_scope = macro_scope,
            });

    FklVMvalueCgEnv *macro_main_env = fklCreateVMvalueCgEnv(info->ctx,
            macro_info->global_env,
            1,
            macro_scope);

    *penv = macro_main_env;
    if (symbol_set == NULL)
        return macro_info;

    for (FklValueHashSetNode *list = symbol_set->first; list;
            list = list->next) {
        FklVMvalue *id = FKL_TYPE_CAST(FklVMvalue *, list->k);
        fklAddCgDefBySid(id, 1, macro_main_env);
    }

    return macro_info;
}

typedef struct {
    FklGrammerProduction *prod;
    FklVMvalue *sid;
    FklVMvalue *group_id;
    FklVMvalue *vec;
    uint8_t add_extra;
    uint8_t type;
    FklVMvalue *action;
    FklVMvalueCgInfo *lib_info;
} AddingProductionCtx;

static void _adding_production_ctx_finalizer(void *data) {
    AddingProductionCtx *ctx = data;
    if (ctx->prod) {
        fklDestroyGrammerProduction(ctx->prod);
        ctx->prod = NULL;
    }
    ctx->vec = NULL;
    if (ctx->type != FKL_CODEGEN_PROD_CUSTOM) {
        ctx->action = NULL;
    }
    ctx->lib_info = NULL;
}

static void _adding_production_ctx_atomic(FklVMgc *gc, void *data) {
    AddingProductionCtx *ctx = data;
    if (ctx->prod) {
        fklVMgcMarkGrammerProd(gc, ctx->prod, NULL);
    }
    fklVMgcToGray(ctx->vec, gc);
    fklVMgcToGray(ctx->action, gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->lib_info), gc);
}

static const FklCgActCtxMethodTable AddingProductionCtxMethodTable = {
    .size = sizeof(AddingProductionCtx),
    .finalize = _adding_production_ctx_finalizer,
    .atomic = _adding_production_ctx_atomic,
};

static inline FklCgActCtx *createAddingProductionCtx(FklGrammerProduction *prod,
        FklVMvalue *sid,
        FklVMvalue *group_id,
        uint8_t add_extra,
        const FklVMvalue *vec,
        FklCgProdActionType type,
        const FklVMvalue *action_ast,
        FklVMvalueCgInfo *lib_info) {
    FklCgActCtx *r = createCgActCtx(&AddingProductionCtxMethodTable);
    AddingProductionCtx *ctx = FKL_TYPE_CAST(AddingProductionCtx *, r->d);
    ctx->prod = prod;
    ctx->sid = sid;
    ctx->group_id = group_id;
    ctx->add_extra = add_extra;
    ctx->vec = FKL_TYPE_CAST(FklVMvalue *, vec);
    ctx->type = type;
    ctx->action = FKL_TYPE_CAST(FklVMvalue *, action_ast);
    ctx->lib_info = lib_info;
    return r;
}

static inline int set_non_terminal_group(FklGrammerSym *sym,
        const FklProdHashMap *productions,
        const FklProdHashMap *builtin_g,
        FklVMvalue *group_id) {
    if (sym->type != FKL_TERM_NONTERM)
        return 0;
    int is_ref_outer = 0;

    if (sym->nt.group && sym->nt.group != group_id) {
        is_ref_outer |= sym->nt.group != group_id;
    } else if (!fklIsNonterminalExist(builtin_g, NULL, sym->nt.sid)) {
        sym->nt.group = group_id;
    }

    return is_ref_outer;
}

static inline int check_group_outer_ref(FklVMvalueCgInfo *info,
        const FklProdHashMap *productions,
        FklVMvalue *group_id) {
    int is_ref_outer = 0;
    for (FklProdHashMapNode *item = productions->first; item;
            item = item->next) {
        for (FklGrammerProduction *prods = item->v; prods;
                prods = prods->next) {
            for (size_t i = 0; i < prods->len; i++) {
                is_ref_outer |= set_non_terminal_group(&prods->syms[i],
                        productions,
                        &info->ctx->builtin_g.productions,
                        group_id);
            }
        }
    }
    return is_ref_outer;
}

static FklVMvalue *process_adding_production(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    uint64_t line = args->line;
    FklCgErrorState *errors = args->error_state;

    AddingProductionCtx *prod_ctx = FKL_TYPE_CAST(AddingProductionCtx *, data);
    FklVMvalue *group_id = prod_ctx->group_id;
    FKL_ASSERT(group_id);

    FklVMvalueCgInfo *lib_info = prod_ctx->lib_info;

    FklGrammerProduction *prod = prod_ctx->prod;
    prod_ctx->prod = NULL;

    FklVMvalue *sid = prod_ctx->sid;
    FklGrammerProdGroupItem *item =
            add_production_group(info->prod_groups, info->ctx->gc, group_id);
    if (fklAddProdToProdTableNoRepeat(&item->g, prod)) {
        fklDestroyGrammerProduction(prod);
        goto reader_macro_error;
    }

    FklGrammerProduction *extra_prod = NULL;
    if (prod_ctx->add_extra) {
        extra_prod = fklCreateExtraStartProduction(info->ctx, group_id, sid);
        if (fklAddProdToProdTable(&item->g, extra_prod)) {
            fklDestroyGrammerProduction(extra_prod);
        reader_macro_error:
            errors->error = make_grammer_create_error(GCVM);
            errors->line = line;
            return NULL;
        }
    }

    int outer_ref = check_group_outer_ref(info, &item->g.productions, group_id);

    if (outer_ref) {
        item->flags |= FKL_GRAMMER_GROUP_HAS_OUTER_REF;
        if (lib_info != NULL) {
            errors->error = make_export_error(GCVM,
                    "Group %S has reference to other group",
                    group_id);
            errors->line = line;
            return NULL;
        }
    }

    if (lib_info == NULL)
        return NULL;

    FklGrammerProdGroupItem *item2 =
            add_production_group(lib_info->export_prod_groups,
                    info->ctx->gc,
                    group_id);

    fklMergeGrammerProd(&item2->g, prod, &item->g, NULL);
    if (extra_prod)
        fklMergeGrammerProd(&item2->g, extra_prod, &item->g, NULL);

    return NULL;
}

typedef struct {
    // in
    uint32_t line;
    uint8_t add_extra;
    FklVMvalue *left_group;
    FklVMvalue *left_sid;
    FklVMvalue *action_type;
    FklVMvalue *action_ast;
    FklVMvalue *group_id;
    FklVMvalueCgInfo *info;
    FklVMvalueCgEnv *env;
    FklVMvalueCgMacroScope *macro_scope;
    FklCgActVector *actions;
    FklCgCtx *ctx;
    FklVMvalueCgInfo *lib_info;

    // out
    FklGrammer *g;
    FklVMvalue *err_node;
    FklGrammerProduction *production;
} NastToProductionArgs;

static inline NastToGrammerSymErr
nast_vector_to_production(const FklVMvalue *vec, NastToProductionArgs *args) {
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
    FklVMvalueCgInfo *info = args->info;
    FklVMvalue *action_ast = args->action_ast;
    FklVMvalue *left_sid = args->left_sid;
    FklVMvalue *left_group = args->left_group;
    FklVMvalue *group_id = args->group_id;
    uint8_t add_extra = args->add_extra;

    if (action_type == info->ctx->builtin_sym_builtin) {
        if (!FKL_IS_SYM(action_ast)) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }
        FklGrammerProduction *prod = fklCreateBuiltinActionProd(info->ctx,
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
                        action_ast,
                        args->lib_info),
                NULL,
                1,
                args->macro_scope,
                args->env,
                args->line,
                info,
                args->actions);
        args->production = prod;
    } else if (action_type == info->ctx->builtin_sym_simple) {
        if (!FKL_IS_VECTOR(action_ast)               //
                || FKL_VM_VEC(action_ast)->size == 0 //
                || !FKL_IS_SYM(FKL_VM_VEC(action_ast)->base[0])) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }

        FklGrammerProduction *prod = fklCreateSimpleActionProd(info->ctx,
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
                        action_ast,
                        args->lib_info),
                NULL,
                1,
                args->macro_scope,
                args->env,
                args->line,
                info,
                args->actions);
        args->production = prod;
    } else if (action_type == info->ctx->builtin_sym_custom) {
        FklVMvalueCgEnv *macro_env = NULL;
        FklVMvalueCgInfo *macro_info = macro_compile_prepare(info,
                args->macro_scope,
                NULL,
                &macro_env);

        create_and_insert_to_pool(macro_info,
                0,
                macro_env,
                0,
                CURLINE(action_ast));
        CgExpQueue *queue = cgExpQueueCreate();
        int failed = 0;
        if (failed) {
            args->err_node = action_ast;
            err = NAST_TO_GRAMMER_SYM_ERR_INVALID_ACTION_AST;
            goto error_happened;
        }

        FklGrammerProduction *prod = fklCreateCustomActionProd(info->ctx,
                left_group,
                left_sid,
                other_args.len,
                other_args.syms,
                macro_env->prototypeId);
        FklVMvalueCustomActCtx *ctx = prod->ctx;
        for (size_t i = 0; i < ctx->actual_len; ++i) {
            fklAddCgDefBySid(ctx->dollers[i], 1, macro_env);
        }
        fklAddCgDefBySid(ctx->doller_s, 1, macro_env);
        fklAddCgDefBySid(ctx->line_s, 1, macro_env);

        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(process_adding_production,
                createAddingProductionCtx(prod,
                        left_sid,
                        group_id,
                        add_extra,
                        vec,
                        FKL_CODEGEN_PROD_CUSTOM,
                        NULL,
                        args->lib_info),
                NULL,
                1,
                args->macro_scope,
                args->env,
                args->line,
                info,
                args->actions);
        cgExpQueuePush2(queue,
                (FklPmatchRes){
                    .value = action_ast,
                    .container = action_ast,
                });
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(_reader_macro_bc_process,
                createReaderMacroActionContext(prod->ctx),
                createMustHasRetvalQueueNextExpression(queue),
                1,
                macro_env->macros,
                macro_env,
                CURLINE(action_ast),
                macro_info,
                args->actions);
        args->production = prod;
    } else if (action_type == info->ctx->builtin_sym_replace) {
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
                        action_ast,
                        args->lib_info),
                NULL,
                1,
                args->macro_scope,
                args->env,
                args->line,
                info,
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

static inline int add_ignore(FklVMvalueCgInfo *info,
        FklVMvalue *vector_node,
        FklGrammer *g,
        FklCgErrorState *errors) {
    FklVMvalue *ignore_obj = FKL_VM_BOX(vector_node);
    FKL_ASSERT(FKL_IS_VECTOR(ignore_obj));

    NastToGrammerSymArgs args = { .g = g };
    NastToGrammerSymErr err = 0;
    FklGrammerIgnore *ignore = nast_vector_to_ignore(ignore_obj, &args, &err);
    if (err) {
        const char *msg = get_nast_to_grammer_sym_err_msg(err);
        errors->error = make_grammer_create_error2(IGCVM, msg, args.err_node);
        errors->line = CURLINE(vector_node);
        return 1;
    }

    if (fklAddIgnoreToIgnoreList(&g->ignores, ignore)) {
        fklDestroyIgnore(ignore);
    }

    return 0;
}

static int add_delimiters(FklVMvalueCgInfo *info,
        FklVMvalue *vector_node,
        FklGrammerProdGroupItem *item,
        FklCgErrorState *errors) {

    FklGrammer *g = &item->g;
    if (FKL_IS_STR(vector_node)) {
        fklAddString(&g->terminals, FKL_VM_STR(vector_node));
        fklAddString(&g->delimiters, FKL_VM_STR(vector_node));
        return 0;
    }

    if (FKL_IS_PAIR(vector_node)) {
        const FklVMvalue *cur = vector_node;
        for (; FKL_IS_PAIR(cur); cur = FKL_VM_CDR(cur)) {
            FKL_ASSERT(FKL_IS_STR(FKL_VM_CAR(cur)));
            fklAddString(&g->terminals, FKL_VM_STR(FKL_VM_CAR(cur)));
            fklAddString(&g->delimiters, FKL_VM_STR(FKL_VM_CAR(cur)));
        }

        if (cur != FKL_VM_NIL) {
            errors->error = make_syntax_error(IGCVM, vector_node);
            errors->line = CURLINE(vector_node);
            return 1;
        }

        return 0;
    }

    if (FKL_IS_BOX(vector_node)) {
        if (add_ignore(info, vector_node, &item->g, errors))
            return 1;
        return 0;
    }

    errors->error = make_syntax_error(IGCVM, vector_node);
    errors->line = CURLINE(vector_node);

    return 1;
}

static inline int process_add_production(FklVMvalue *group_id,
        FklVMvalueCgInfo *info,
        FklVMvalue *vector_node,
        FklCgErrorState *errors,
        FklVMvalueCgEnv *env,
        FklVMvalueCgMacroScope *macro_scope,
        FklCgActVector *actions,
        FklVMvalueCgInfo *lib_info) {
    FKL_ASSERT(group_id);

    FklGrammerProdGroupItem *item =
            add_production_group(info->prod_groups, info->ctx->gc, group_id);

    if (!FKL_IS_VECTOR(vector_node)) {
        if (add_delimiters(info, vector_node, item, errors))
            return 1;

        if (lib_info == NULL)
            return 0;

        item = add_production_group(lib_info->export_prod_groups,
                info->ctx->gc,
                group_id);

        if (add_delimiters(lib_info, vector_node, item, errors))
            return 1;

        return 0;
    }

    FklVMvalue *error_node = NULL;
    FklVMvalue *vec = vector_node;

    if (FKL_VM_VEC(vec)->size != 4) {
        error_node = vector_node;
    reader_macro_syntax_error:
        errors->error = make_syntax_error(IGCVM, error_node);
        errors->line = CURLINE(vec);
        return 1;
    }

    FklVMvalue **base = FKL_VM_VEC(vec)->base;

    if (!FKL_IS_SYM(base[2])) {
        error_node = base[2];
        goto reader_macro_syntax_error;
    }

    FklVMvalue *vect = NULL;
    NastToProductionArgs args = {
        .line = CURLINE(vec),
        .add_extra = 1,
        .action_type = base[2],
        .action_ast = base[3],
        .group_id = group_id,
        .info = info,
        .env = env,
        .macro_scope = macro_scope,
        .actions = actions,
        .ctx = info->ctx,
        .lib_info = lib_info,
        .g = &item->g,
    };

    if (base[0] == FKL_VM_NIL && FKL_IS_VECTOR(base[1])) {
        vect = base[1];
        args.left_group = info->ctx->builtin_g.start.group;
        args.left_sid = info->ctx->builtin_g.start.sid;
        args.add_extra = 0;
    } else if (FKL_IS_SYM(base[0]) && FKL_IS_VECTOR(base[1])) {
        vect = base[1];
        FklVMvalue *sid = base[0];

        args.left_group = group_id;
        args.left_sid = sid;
    } else if (FKL_IS_VECTOR(base[0]) && FKL_IS_SYM(base[1])) {
        vect = base[0];
        FklVMvalue *sid = base[1];

        args.left_group = group_id;
        args.left_sid = sid;
        args.add_extra = 0;
    } else {
        error_node = vector_node;
        goto reader_macro_syntax_error;
    }

    NastToGrammerSymErr err = nast_vector_to_production(vect, &args);
    if (err == NAST_TO_GRAMMER_SYM_ERR_DUMMY)
        return 0;

    FklVMvalue *err_val = args.err_node == NULL ? base[2] : args.err_node;
    const char *msg = get_nast_to_grammer_sym_err_msg(err);
    errors->error = make_grammer_create_error2(IGCVM, msg, err_val);
    errors->line = CURLINE(vect);
    return 1;

    return 0;
}

static FklVMvalue *update_grammer(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    uint64_t line = args->line;
    FklCgErrorState *error_state = args->error_state;

    if (add_all_group_to_grammer(line, info, error_state)) {
        error_state->error = make_grammer_create_error(GCVM);
        error_state->line = line;
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

static void codegen_defmacro_impl(const CgCbArgs *args,
        FklVMvalueCgInfo *lib_info) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *errors = args->err_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    if (must_has_retval) {
        errors->error = make_has_no_value_error(GCVM, orig->value);
        errors->line = CURLINE(orig->container);
        return;
    }
    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (FKL_IS_SYM(name->value)) {
        fklReplacementHashMapAdd2(macro_scope->replacements,
                name->value,
                value->value);
        if (lib_info) {
            fklReplacementHashMapAdd2(lib_info->export_replacement,
                    name->value,
                    value->value);
        }
    } else if (FKL_IS_PAIR(name->value)) {
        FklValueHashSet *symbolSet = NULL;
        FklVMvalue *pattern =
                fklCreatePatternFromNast(IGCVM, name->value, &symbolSet);
        if (!pattern) {
            errors->error = make_macro_pattern_error(IGCVM, name->value);
            errors->line = CURLINE(name->container);
            return;
        }
        if (fklValueHashSetPut2(symbolSet, ctx->builtin_sym_orig)) {
            errors->error = make_macro_pattern_error(IGCVM, name->value);
            errors->line = CURLINE(name->container);
            return;
        }

        FklVMvalueCgEnv *macroEnv = NULL;
        FklVMvalueCgInfo *macro_info =
                macro_compile_prepare(info, macro_scope, symbolSet, &macroEnv);
        fklValueHashSetDestroy(symbolSet);

        create_and_insert_to_pool(macro_info,
                0,
                macroEnv,
                0,
                CURLINE(value->container));
        CgExpQueue *queue = cgExpQueueCreate();
        cgExpQueuePush(queue, value);
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(compiler_macro_bc_process,
                createMacroActionContext(name->value,
                        pattern,
                        macro_scope,
                        lib_info,
                        macroEnv->prototypeId),
                createMustHasRetvalQueueNextExpression(queue),
                1,
                macroEnv->macros,
                macroEnv,
                CURLINE(value->container),
                macro_info,
                actions);
    } else if (FKL_IS_BOX(name->value)) {
        FklVMvalue *group_id = FKL_VM_BOX(name->value);
        if (!is_valid_production_rule_node(value->value)) {
            errors->error = make_syntax_error(IGCVM, value->value);
            errors->line = CURLINE(value->container);
            return;
        }

        if (!FKL_IS_SYM(group_id)) {
            errors->error = make_syntax_error(IGCVM, name->value);
            errors->line = CURLINE(name->container);
            return;
        }
        FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(update_grammer,
                createDefaultStackContext(),
                NULL,
                scope,
                macro_scope,
                env,
                CURLINE(orig->container),
                info,
                actions);
        init_builtin_grammer_and_prod_group(info);

        if (process_add_production(group_id,
                    info,
                    value->value,
                    errors,
                    env,
                    macro_scope,
                    actions,
                    lib_info))
            return;
    }
}

static void codegen_defmacro(const CgCbArgs *args) {
    codegen_defmacro_impl(args, NULL);
}

static void codegen_def_reader_macros_impl(const CgCbArgs *args,
        FklVMvalueCgInfo *lib_info) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = args->err_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    if (must_has_retval) {
        error_state->error = make_has_no_value_error(GCVM, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    FklValueQueue prod_vector_queue;
    fklValueQueueInit(&prod_vector_queue);
    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg0);
    FklPmatchRes err_node = { 0 };
    if (!FKL_IS_BOX(name->value)) {
        err_node = *name;
        goto reader_macro_error;
    }
    const FklPmatchRes *arg = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg1);
    if (!is_valid_production_rule_node(arg->value)) {
        err_node = *arg;
        goto reader_macro_error;
    }
    fklValueQueuePush2(&prod_vector_queue, arg->value);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);

    for (FklVMvalue *rv = rest->value; FKL_IS_PAIR(rv); rv = FKL_VM_CDR(rv)) {
        if (!is_valid_production_rule_node(FKL_VM_CAR(rv))) {
            err_node =
                    (FklPmatchRes){ .value = FKL_VM_CAR(rv), .container = rv };
            goto reader_macro_error;
        }
        fklValueQueuePush2(&prod_vector_queue, FKL_VM_CAR(rv));
    }

    FklVMvalue *group_id = FKL_VM_BOX(name->value);
    if (!FKL_IS_SYM(group_id)) {
        err_node = *name;
    reader_macro_error:
        fklValueQueueUninit(&prod_vector_queue);
        error_state->error = make_syntax_error(IGCVM, err_node.value);
        error_state->line = CURLINE(err_node.container);
        return;
    }
    FKL_PUSH_NEW_DEFAULT_PREV_CODEGEN_ACTION(update_grammer,
            createDefaultStackContext(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            info,
            actions);

    init_builtin_grammer_and_prod_group(info);

    for (FklValueQueueNode *first = prod_vector_queue.head; first;
            first = first->next)
        if (process_add_production(group_id,
                    info,
                    first->data,
                    error_state,
                    env,
                    macro_scope,
                    actions,
                    lib_info)) {
            fklValueQueueUninit(&prod_vector_queue);
            return;
        }
    fklValueQueueUninit(&prod_vector_queue);
}

static void codegen_def_reader_macros(const CgCbArgs *args) {
    codegen_def_reader_macros_impl(args, NULL);
}

static void (*const export_import_callbacks[])(const CgCbArgs *args,
        FklVMvalueCgInfo *lib_info) = {
    [FKL_CODEGEN_PATTERN_IMPORT_PREFIX] = codegen_import_prefix_impl,
    [FKL_CODEGEN_PATTERN_IMPORT_ONLY] = codegen_import_only_impl,
    [FKL_CODEGEN_PATTERN_IMPORT_ALIAS] = codegen_import_alias_impl,
    [FKL_CODEGEN_PATTERN_IMPORT_EXCEPT] = codegen_import_except_impl,
    [FKL_CODEGEN_PATTERN_IMPORT_COMMON] = codegen_import_common_impl,
    [FKL_CODEGEN_PATTERN_IMPORT] = codegen_import_common_impl,
    [FKL_CODEGEN_PATTERN_IMPORT_NONE] = codegen_import_none_impl,
};

static void codegen_export_single(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *errors = args->err_state;
    uint8_t const must_has_retval = args->must_has_retval;
    FklPmatchRes orig = *args->orig;

    FklVMvalueCgInfo *lib_info = get_lib_info(info);
    if (!lib_info                            //
            || env->prev != info->global_env //
            || scope > 1                     //
            || macro_scope->prev != info->global_env->macros) {
        errors->error = make_syntax_error(GCVM, orig.value);
        errors->line = CURLINE(orig.container);
        return;
    }

    FklPmatchRes v = *fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    v.value = fklTryExpandCgMacro(&v, info, macro_scope, errors);

    if (errors->error)
        return;

    FklVMvalue *const *patterns = ctx->builtin_pattern_node;
    FklVMvalue *name = NULL;
    CgExpQueue *queue = NULL;

    if (!fklIsList(v.value))
        goto error;

    if (is_defmacro_exp(v.value, ctx, ht)) {
        if (must_has_retval)
            goto must_has_retval_error;
        CgCbArgs other_args = *args;
        other_args.orig = &v;
        codegen_defmacro_impl(&other_args, lib_info);
        return;
    } else if (is_def_reader_exp(v.value, ctx, ht)) {
        if (must_has_retval)
            goto must_has_retval_error;
        CgCbArgs other_args = *args;
        other_args.orig = &v;
        codegen_def_reader_macros_impl(&other_args, lib_info);
        return;
    } else if (is_import_exp(v.value, ctx)) {
        if (is_export_none_exp(v.value, ctx) && must_has_retval) {
        must_has_retval_error:
            errors->error = make_has_no_value_error(GCVM, orig.value);
            errors->line = CURLINE(orig.container);
            return;
        }

        for (FklCgPatternEnum i = FKL_CODEGEN_PATTERN_IMPORT_PREFIX;
                i <= FKL_CODEGEN_PATTERN_IMPORT_NONE;
                ++i) {
            if (fklPatternMatch(ctx->builtin_pattern_node[i], v.value, ht)) {
                CgCbArgs other_args = *args;
                other_args.orig = &v;
                export_import_callbacks[i](&other_args, lib_info);
                return;
            }
        }
        return;
    }

    if (isBeginExp(v.value, patterns)) {
        FKL_VM_CAR(v.value) = FKL_VM_CAR(orig.value);

        queue = cgExpQueueCreate();
        cgExpQueuePush2(queue,
                (FklPmatchRes){
                    .value = v.value,
                    .container = v.container,
                });

        fklCgActVectorPushBack2(actions,
                create_cg_action(exports_bc_process,
                        create_export_sequnce_context(&orig, must_has_retval),
                        createDefaultQueueNextExpression(queue),
                        1,
                        macro_scope,
                        env,
                        CURLINE(orig.container),
                        NULL,
                        info));
        return;
    } else if (isExportDefunExp(v.value, patterns)) {
        name = caadr(v.value);
        goto process_def_in_lib;
        return;
    } else if (isExportDefineExp(v.value, patterns)) {
        name = cadr(v.value);
    process_def_in_lib:
        if (!FKL_IS_SYM(name))
            goto error;
        FklCgExportIdx *item =
                fklCgExportSidIdxHashMapGet2(&lib_info->exports, name);
        if (item == NULL) {
            uint32_t idx = lib_info->exports.count;
            item = fklCgExportSidIdxHashMapAdd(&lib_info->exports,
                    &name,
                    &(FklCgExportIdx){
                        .idx = idx,
                        .oidx = FKL_VAR_REF_INVALID_CIDX,
                    });
        }

        queue = cgExpQueueCreate();
        cgExpQueuePush2(queue,
                (FklPmatchRes){
                    .value = v.value,
                    .container = v.container,
                });

        fklCgActVectorPushBack2(actions,
                create_cg_action(_export_define_bc_process,
                        create_export_define_context(name, item),
                        createDefaultQueueNextExpression(queue),
                        1,
                        macro_scope,
                        env,
                        CURLINE(orig.container),
                        NULL,
                        info));
        return;
    }

    return;
error:
    if (queue) {
        cgExpQueueDestroy(queue);
    }

    errors->error = make_syntax_error(GCVM, orig.value);
    errors->line = CURLINE(orig.container);
    return;
}

typedef void (*FklCgFunc)(const CgCbArgs *args);

static FklVMvalue *last_bc_process(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *r = sequnce_exp_bc_process(GCVM, bcl_vec, fid, line, scope);
    FklInstruction r_ins = create_op_ins(FKL_OP_RET);
    fklByteCodeLntPushBackIns(FKL_VM_CO(r),
            &r_ins,
            info->fid,
            info->curline,
            1);
    if (!env->is_debugging)
        fklPeepholeOptimize(FKL_VM_CO(r));
    return r;
}

static FklVMvalue *gen_push_literal_code(FklVM *exe,
        const FklPmatchRes *cnode,
        FklVMvalueCgInfo *info,
        FklVMvalueCgEnv *env,
        uint32_t scope) {
    FklVMvalue *node = cnode->value;

    FklVMvalue *r = create_0len_bcl(exe);
    FklByteCode *retval = &FKL_VM_CO(r)->bc;
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
            append_push_i24_ins_to_bc(INS_APPEND_FRONT, retval, i, info);
        } else {
        add_consts:
            k = fklValueTableAdd(&env->konsts, node) - 1;
            FKL_ASSERT(k <= FKL_U24_MAX);
            append_push_const_ins_to_bc(INS_APPEND_FRONT, retval, k, info);
        }
    } break;
    }

    FklByteCodelnt *rr = FKL_VM_CO(r);
    rr->ls = 1;
    rr->l = (FklLineNumberTableItem *)fklZmalloc(
            sizeof(FklLineNumberTableItem));
    FKL_ASSERT(rr->l);
    fklInitLineNumTabNode(&rr->l[0], info->fid, 0, CURLINE(node), scope);

    return r;
}

static inline int matchAndCall(FklCgFunc func,
        const FklVMvalue *pattern,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        const FklPmatchRes *exp,
        FklCgActVector *actions,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info,
        FklCgErrorState *error_state,
        uint8_t must_has_retval) {
    FklPmatchHashMap ht;
    fklPmatchHashMapInit(&ht);
    int r = fklPatternMatch(pattern, exp->value, &ht);
    if (r) {
        fklChdir(info->dir);
        CgCbArgs args = {
            .orig = exp,
            .ht = &ht,
            .actions = actions,
            .scope = scope,
            .macro_scope = macro_scope,
            .env = env,
            .info = info,
            .ctx = info->ctx,
            .err_state = error_state,
            .must_has_retval = must_has_retval,
        };

        func(&args);
        fklChdir(info->ctx->cwd);
    }
    fklPmatchHashMapUninit(&ht);
    return r;
}

static struct PatternAndFunc {
    const char *ps;
    FklCgFunc func;
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
    {"~(import ~name,~args)",                                  codegen_import_common     },
    {"~(import)",                                              codegen_import_none       },
    {"~(export ~value)",                                       codegen_export_single     },
    {"~(export)",                                              codegen_export_none       },
    {"~(export,~rest)",                                        codegen_export            },
    {"~(defmacro ~name ~value)",                               codegen_defmacro          },
    {"~(defmacro ~arg0 ~arg1,~rest)",                          codegen_def_reader_macros },
    {"~(cfg ~cond ~value,~rest)",                              codegen_cond_compile      },
    {NULL,                                                     NULL                      },
    // clang-format on
};

static inline void mark_action_vector(FklVMgc *gc, FklCgActVector *v) {
    if (v == NULL)
        return;
    FklCgAct **cur = v->base;
    FklCgAct **const end = cur + v->size;
    for (; cur < end; ++cur) {
        FklCgAct *a = *cur;
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, a->env), gc);
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, a->macros), gc);
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, a->info), gc);
        if (a->ctx->t->atomic) {
            a->ctx->t->atomic(gc, a->ctx->d);
        }
        if (a->exps && a->exps->t->atomic) {
            a->exps->t->atomic(gc, a->exps->context);
        }

        FklValueVector *bcv = &a->bcl_vector;
        for (size_t i = 0; i < bcv->size; ++i)
            fklVMgcToGray(bcv->base[i], gc);
    }
}

static inline void mark_codegen_lib(FklVMgc *gc, const FklCgLib *lib) {
    for (const FklCgExportSidIdxHashMapNode *cur = lib->exports.first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->k, gc);
    }
    switch (lib->type) {
    case FKL_CODEGEN_LIB_SCRIPT:
        fklVMgcToGray(lib->bcl, gc);

        for (const FklMacroHashMapNode *cur = lib->macros->first; cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            for (const FklCgMacro *m = cur->v; m; m = m->next) {
                fklVMgcToGray(m->pattern, gc);
                fklVMgcToGray(m->bcl, gc);
            }
        }

        for (const FklReplacementHashMapNode *cur = lib->replacements->first;
                cur;
                cur = cur->next) {
            fklVMgcToGray(cur->k, gc);
            fklVMgcToGray(cur->v, gc);
        }

        for (const FklGraProdGroupHashMapNode *cur =
                        lib->named_prod_groups->first;
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
        const FklCgLibVector *libs) {
    for (size_t i = 0; i < libs->size; ++i) {
        mark_codegen_lib(gc, &libs->base[i]);
    }
}

static void codegen_ctx_extra_mark_func(FklVMgc *gc, void *c) {
    FklCgCtx *ctx = FKL_TYPE_CAST(FklCgCtx *, c);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->global_env), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->global_info), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->macro_vm_libs), gc);

    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->macro_pts), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->pts), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->lnt), gc);

    mark_action_vector(gc, ctx->action_vector);

    if (ctx->error_state) {
        fklVMgcToGray(ctx->error_state->fid, gc);
        fklVMgcToGray(ctx->error_state->error, gc);
    }

    if (ctx->bcl_vector) {
        for (size_t i = 0; i < ctx->bcl_vector->size; ++i) {
            fklVMgcToGray(ctx->bcl_vector->base[i], gc);
        }
    }

    {
#define XX(A) fklVMgcToGray(ctx->builtin_sym_##A, gc);
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

void fklInitCgCtxExceptPattern(FklCgCtx *ctx,
        FklVMvalueProtos *pts,
        FklVMgc *gc) {
    memset(ctx, 0, sizeof(FklCgCtx));
    fklCgLibVectorInit(&ctx->libraries, 8);
    fklCgLibVectorInit(&ctx->macro_libraries, 8);

    ctx->pts = pts == NULL ? fklCreateVMvalueProtos(&gc->gcvm, 0) : pts;
    ctx->macro_pts = pts == NULL //
                           ? ctx->pts
                           : fklCreateVMvalueProtos(&gc->gcvm, 0);

    ctx->gc = gc;

    fklVMpushExtraMarkFunc(ctx->gc, codegen_ctx_extra_mark_func, NULL, ctx);

    fklInitBuiltinGrammer(&ctx->builtin_g, GCVM);

    ctx->lnt = fklCreateVMvalueLnt(GCVM);
}

static inline void init_builtin_patterns(FklCgCtx *ctx) {
    FklVMvalue **const builtin_pattern_node = ctx->builtin_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++) {
        FklVMvalue *node = fklCreateNastNodeFromCstr(GCVM,
                builtin_pattern_cstr_func[i].ps,
                NULL);
        builtin_pattern_node[i] = fklCreatePatternFromNast(GCVM, node, NULL);
    }
}

static inline void init_builtin_replacements(FklCgCtx *ctx) {
    FklVMvalue **const replacement_ids = ctx->builtin_replacement_id;
    struct SymbolReplacement *const builtin_replacements =
            builtInSymbolReplacement;

    for (size_t i = 0; i < FKL_BUILTIN_REPLACEMENT_NUM; i++)
        replacement_ids[i] = add_symbol_cstr(ctx, builtin_replacements[i].s);
}

static inline void init_builtin_sub_patterns(FklCgCtx *ctx) {
    FklVMvalue **const builtin_sub_pattern_node = ctx->builtin_sub_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_SUB_PATTERN_NUM; i++) {
        FklVMvalue *node =
                fklCreateNastNodeFromCstr(GCVM, builtInSubPattern[i], NULL);
        builtin_sub_pattern_node[i] =
                fklCreatePatternFromNast(GCVM, node, NULL);
    }
}

void fklInitCgCtx(FklCgCtx *ctx,
        char *main_file_real_path_dir,
        FklVMvalueProtos *pts,
        FklVMgc *gc) {
    fklInitCgCtxExceptPattern(ctx, pts, gc);
    ctx->cwd = fklSysgetcwd();
    ctx->main_file_real_path_dir = main_file_real_path_dir
                                         ? main_file_real_path_dir
                                         : fklZstrdup(ctx->cwd);

#define XX(A) ctx->builtin_sym_##A = add_symbol_cstr(ctx, #A);
    FKL_CODEGEN_SYMBOL_MAP
#undef XX

    fklInitProdActionList(ctx);

    init_builtin_patterns(ctx);
    init_builtin_sub_patterns(ctx);
    init_builtin_replacements(ctx);
}

void fklUninitCgCtx(FklCgCtx *ctx) {
    fklUninitGrammer(&ctx->builtin_g);
    ctx->gc = NULL;
    ctx->pts = NULL;
    ctx->macro_pts = NULL;
    ctx->lnt = NULL;

    FklVMvalue **nodes = ctx->builtin_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++) {
        nodes[i] = NULL;
    }
    nodes = ctx->builtin_sub_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_SUB_PATTERN_NUM; i++) {
        nodes[i] = NULL;
    }
    fklZfree(ctx->cwd);
    fklZfree(ctx->main_file_real_path_dir);

    while (!fklCgLibVectorIsEmpty(&ctx->libraries))
        fklUninitCgLib(fklCgLibVectorPopBackNonNull(&ctx->libraries));

    fklCgLibVectorUninit(&ctx->libraries);

    while (!fklCgLibVectorIsEmpty(&ctx->macro_libraries))
        fklUninitCgLib(fklCgLibVectorPopBackNonNull(&ctx->macro_libraries));

    fklCgLibVectorUninit(&ctx->macro_libraries);
}

static inline int mapAllBuiltInPattern(const FklPmatchRes *curExp,
        FklCgActVector *actions,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info,
        FklCgErrorState *error_state,
        uint8_t must_has_retval) {
    FklVMvalue *const *builtin_pattern_node = info->ctx->builtin_pattern_node;

    if (fklIsList(curExp->value))
        for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++)
            if (matchAndCall(builtin_pattern_cstr_func[i].func,
                        builtin_pattern_node[i],
                        scope,
                        macro_scope,
                        curExp,
                        actions,
                        env,
                        info,
                        error_state,
                        must_has_retval))
                return 0;

    if (FKL_IS_PAIR(curExp->value)) {
        codegen_funcall(curExp,
                actions,
                scope,
                macro_scope,
                env,
                info,
                info->ctx,
                error_state);
        return 0;
    }
    return 1;
}

static inline FklVMvalue *append_get_loc_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
            bcl,
            FKL_OP_GET_LOC,
            idx,
            fid,
            line,
            scope);
}

static inline FklVMvalue *append_get_var_ref_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t curline,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
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

FklVMvalue *fklGenExpressionCodeWithAction(FklCgAct *initial_action,
        FklVMvalueCgInfo *info) {
    FklCgCtx *ctx = info->ctx;
    FklValueVector results;
    fklValueVectorInit(&results, 1);
    FklCgErrorState error_state = { 0 };
    FklCgActVector act_vec;
    fklCgActVectorInit(&act_vec, 32);

    FklCgAct *action1 = create_cg_action(last_bc_process,
            createDefaultStackContext(),
            NULL,
            initial_action->scope,
            initial_action->macros,
            initial_action->env,
            initial_action->curline,
            NULL,
            initial_action->info);

    fklCgActVectorPushBack2(&act_vec, action1);
    fklCgActVectorPushBack2(&act_vec, initial_action);

    info->ctx->action_vector = &act_vec;
    info->ctx->error_state = &error_state;
    info->ctx->bcl_vector = &results;

    while (!fklCgActVectorIsEmpty(&act_vec)) {
        FklCgAct *cur_action = *fklCgActVectorBackNonNull(&act_vec);
        FklVMvalueCgInfo *info = cur_action->info;
        FklCgNextExp *expressions = cur_action->exps;
        FklVMvalueCgEnv *env = cur_action->env;
        FklCgActCtx *cur_context = cur_action->ctx;
        int r = 1;
        if (expressions) {
            CgGetNextExpCb get_next_expression = expressions->t->get_next_exp;
            uint8_t must_has_retval = expressions->must_has_retval;

            FklPmatchRes exp = { 0 };
            while (get_next_expression(expressions->context,
                    &exp,
                    &error_state)) {
            skip:
                if (FKL_IS_PAIR(exp.value)) {
                    exp.value = fklTryExpandCgMacro(&exp,
                            info,
                            cur_action->macros,
                            &error_state);
                    if (error_state.error) {
                        break;
                    }
                    if (must_has_retval == FIRST_MUST_HAS_RETVAL) {
                        must_has_retval = DO_NOT_NEED_RETVAL;
                        expressions->must_has_retval = DO_NOT_NEED_RETVAL;
                    }
                } else if (FKL_IS_SYM(exp.value)) {
                    FklVMvalue *replacement = NULL;
                    ReplacementFunc f = NULL;
                    for (FklVMvalueCgMacroScope *cs = cur_action->macros;
                            cs && !replacement;
                            cs = cs->prev)
                        replacement =
                                fklGetReplacement(exp.value, cs->replacements);
                    if (replacement) {
                        exp.value = replacement;
                        goto skip;
                    } else if ((f = findBuiltInReplacementWithId(exp.value,
                                        ctx->builtin_replacement_id))) {
                        FklVMvalue *t = f(&exp, env, info);
                        FKL_ASSERT(t);
                        exp.value = t;
                        goto skip;
                    } else {
                        FklVMvalue *bcl = NULL;
                        FklSymDefHashMapElm *def =
                                fklFindSymbolDefByIdAndScope(exp.value,
                                        cur_action->scope,
                                        env->scopes);
                        if (def)
                            bcl = append_get_loc_ins(GCVM,
                                    INS_APPEND_BACK,
                                    NULL,
                                    def->v.idx,
                                    info->fid,
                                    CURLINE(exp.container),
                                    cur_action->scope);
                        else {
                            uint32_t idx = fklAddCgRefBySidRetIndex(exp.value,
                                    env,
                                    info->fid,
                                    CURLINE(exp.container),
                                    0);
                            bcl = append_get_var_ref_ins(GCVM,
                                    INS_APPEND_BACK,
                                    NULL,
                                    idx,
                                    info->fid,
                                    CURLINE(exp.container),
                                    cur_action->scope);
                        }
                        fklValueVectorPushBack2(&cur_action->bcl_vector, bcl);
                        continue;
                    }
                }
                FKL_ASSERT(exp.value);
                r = mapAllBuiltInPattern(&exp,
                        &act_vec,
                        cur_action->scope,
                        cur_action->macros,
                        env,
                        info,
                        &error_state,
                        must_has_retval);
                if (r) {
                    fklValueVectorPushBack2(&cur_action->bcl_vector,
                            gen_push_literal_code(GCVM,
                                    &exp,
                                    info,
                                    env,
                                    cur_action->scope));
                }
                if ((!r
                            && (fklCgActVectorIsEmpty(&act_vec)
                                    || *fklCgActVectorBack(&act_vec)
                                               != cur_action))
                        || error_state.error)
                    break;
            }
        }
        if (error_state.error) {
        print_error:
            ctx->action_vector = NULL;
            ctx->error_state = NULL;
            fklPrintCgError(&error_state, info, &info->ctx->gc->err_out);
            while (!fklCgActVectorIsEmpty(&act_vec)) {
                destroy_cg_action(*fklCgActVectorPopBackNonNull(&act_vec));
            }
            fklCgActVectorUninit(&act_vec);
            fklValueVectorUninit(&results);
            fklCheckAndGC(GCVM, FORCE_GC);
            return NULL;
        }
        FklCgAct *other_cg_act = *fklCgActVectorBackNonNull(&act_vec);
        if (other_cg_act == cur_action) {
            fklCgActVectorPopBack(&act_vec);
            FklCgAct *prevAction = cur_action->prev ? cur_action->prev
                                 : fklCgActVectorIsEmpty(&act_vec)
                                         ? NULL
                                         : *fklCgActVectorBackNonNull(&act_vec);
            FklCgActCbArgs args = {
                .data = FKL_TYPE_CAST(void *, cur_context->d),
                .ctx = ctx,
                .info = cur_action->info,
                .env = cur_action->env,
                .scope = cur_action->scope,
                .cms = cur_action->macros,
                .bcl_vec = &cur_action->bcl_vector,
                .fid = cur_action->info->fid,
                .line = cur_action->curline,
                .error_state = &error_state,
            };

            FklVMvalue *resultBcl = cur_action->cb(&args);

            if (resultBcl) {
                if (fklCgActVectorIsEmpty(&act_vec))
                    fklValueVectorPushBack2(&results, resultBcl);
                else {
                    fklValueVectorPushBack2(&prevAction->bcl_vector, resultBcl);
                }
                fklCheckAndGC(GCVM, FORCE_GC);
            }
            if (error_state.error) {
                fklCgActVectorPushBack2(&act_vec, cur_action);
                goto print_error;
            }
            destroy_cg_action(cur_action);
        }
    }
    fklCheckAndGC(GCVM, FORCE_GC);
    ctx->action_vector = NULL;
    ctx->error_state = NULL;
    ctx->bcl_vector = NULL;
    FklVMvalue *retval = NULL;
    if (!fklValueVectorIsEmpty(&results))
        retval = *fklValueVectorPopBackNonNull(&results);
    fklValueVectorUninit(&results);
    fklCgActVectorUninit(&act_vec);
    return retval;
}

FklVMvalue *fklGenExpressionCodeWithFpForPrecompile(FILE *fp,
        FklVMvalueCgInfo *info,
        FklVMvalueCgEnv *env) {
    static const uint32_t scope = 1;
    static const uint64_t line = 1;

    FklCgAct *initialAction = create_cg_action(_begin_exp_bc_process,
            createDefaultStackContext(),
            createFpNextExpression(fp, info),
            1,
            env->macros,
            env,
            1,
            NULL,
            info);

    FklVMvalue *bcl = fklGenExpressionCodeWithAction(initialAction, info);
    if (bcl == NULL)
        return NULL;

    process_export_bc(info, bcl, info->fid, line, scope);

    return bcl;
}

FklVMvalue *fklGenExpressionCodeWithFp(FILE *fp,
        FklVMvalueCgInfo *info,
        FklVMvalueCgEnv *env) {
    FklCgAct *initialAction = create_cg_action(_begin_exp_bc_process,
            createDefaultStackContext(),
            createFpNextExpression(fp, info),
            1,
            env->macros,
            env,
            1,
            NULL,
            info);
    return fklGenExpressionCodeWithAction(initialAction, info);
}

FklVMvalue *fklGenExpressionCode(FklVMvalue *exp,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info) {
    FklVMvalue *cont = fklCreateVMvalueBox(IGCVM, exp);
    put_line_number(info->ctx->lnt, cont, info->curline);
    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush2(queue, (FklPmatchRes){ .value = exp, .container = cont });
    FklCgAct *initialAction = create_cg_action(_default_bc_process,
            createDefaultStackContext(),
            createDefaultQueueNextExpression(queue),
            1,
            env->macros,
            env,
            CURLINE(exp),
            NULL,
            info);
    return fklGenExpressionCodeWithAction(initialAction, info);
}
