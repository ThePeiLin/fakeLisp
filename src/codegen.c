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

#include <fakeLisp/ins_helper.h>

#include <ctype.h>
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

typedef FklVMvalueCgLib FklCgLib;

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
    FklVMvalue *pat = ctx->builtin_pattern_node[FKL_CODEGEN_PATTERN_DEFMACRO];
    return fklPatternMatch(pat, c, ht);
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

static inline int is_begin_exp(const FklVMvalue *c,
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

static const FklCgActCtxMt StackContextMethodTable = { .size = 0 };

static FklCgActCtx *createStackCtx(void) {
    return createCgActCtx(&StackContextMethodTable);
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

static inline FklVMvalue *set_and_append_ins_with_unsigned_imm(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        FklOpcode op,
        uint64_t k,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklIns ins[4] = { FKL_INS_STATIC_INIT };
    int l = FKL_MAKE_INS(ins, op, .ux = k);
    if (bcl == NULL)
        bcl = create_0len_bcl(exe);
    switch (m) {
    case INS_APPEND_BACK:
        for (int i = 0; i < l; i++) {
            fklByteCodeLntPushBackIns(FKL_VM_CO(bcl), ins[i], fid, line, scope);
        }
        break;
    case INS_APPEND_FRONT:
        for (; l > 0; l--) {
            FklIns c = ins[l - 1];
            fklByteCodeLntInsertFrontIns(c, FKL_VM_CO(bcl), fid, line, scope);
        }
    }
    return bcl;
}

static inline FklVMvalue *set_and_append_ins_with_signed_imm(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        FklOpcode op,
        int64_t k,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklIns ins[4] = { FKL_INS_STATIC_INIT };
    int l = FKL_MAKE_INS(ins, op, .ix = k);
    FKL_ASSERT(l);
    if (bcl == NULL)
        bcl = create_0len_bcl(exe);
    switch (m) {
    case INS_APPEND_BACK:
        for (int i = 0; i < l; i++) {
            fklByteCodeLntPushBackIns(FKL_VM_CO(bcl), ins[i], fid, line, scope);
        }
        break;
    case INS_APPEND_FRONT:
        for (; l > 0; l--) {
            FklIns c = ins[l - 1];
            fklByteCodeLntInsertFrontIns(c, FKL_VM_CO(bcl), fid, line, scope);
        }
        break;
    }
    return bcl;
}

static inline FklVMvalue *append_push_i24_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        int64_t k,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklIns ins = FKL_INS_STATIC_INIT;
    if (k == 0 || k == 1) {
        ins = FKL_MAKE_INS_I(FKL_OP_PUSH_0 + k);
        goto append;
    } else if (k >= FKL_I24_MIN && k <= FKL_I24_MAX) {
        int l = FKL_MAKE_INS(&ins, FKL_OP_PUSH_I8, .ix = k);
        FKL_ASSERT(l == 1);
        (void)l;
    append:
        if (bcl == NULL)
            bcl = create_0len_bcl(exe);
        switch (m) {
        case INS_APPEND_BACK:
            fklByteCodeLntPushBackIns(FKL_VM_CO(bcl), ins, fid, line, scope);
            break;
        case INS_APPEND_FRONT:
            fklByteCodeLntInsertFrontIns(ins, FKL_VM_CO(bcl), fid, line, scope);
            break;
        }
    } else {
        FKL_UNREACHABLE();
    }
    return bcl;
}

static inline FklVMvalue *append_push_proc_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        uint32_t proto_id,
        uint64_t len,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    FklOpcode ops[2] = { 0 };
    FklInsArg args[2] = { 0 };
    switch (m) {
    case INS_APPEND_BACK:
        ops[0] = FKL_OP_LOAD_PROTO;
        args[0].ux = proto_id;

        ops[1] = FKL_OP_MAKE_PROC;
        args[1].ux = len;
        break;
    case INS_APPEND_FRONT:
        ops[0] = FKL_OP_MAKE_PROC;
        args[0].ux = len;

        ops[1] = FKL_OP_LOAD_PROTO;
        args[1].ux = proto_id;
        break;
    }

    bcl = set_and_append_ins_with_unsigned_imm(exe,
            m,
            bcl,
            ops[0],
            args[0].ux,
            fid,
            line,
            scope);
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
            bcl,
            ops[1],
            args[1].ux,
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
        FKL_ASSERT(len >= FKL_I24_MIN && len <= FKL_I24_MAX);
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
        FKL_ASSERT(len >= FKL_I24_MIN && len <= FKL_I24_MAX);
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
        uint32_t idx,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
            bcl,
            FKL_OP_IMPORT,
            idx,
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

static inline FklVMvalue *append_drop_one_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *bcl,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
            bcl,
            FKL_OP_DROP,
            0,
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

#define MAKE_AND_PUSH_CG_ACT(F,                                                \
        STACK,                                                                 \
        NEXT_EXPRESSIONS,                                                      \
        SCOPE,                                                                 \
        MACRO_SCOPE,                                                           \
        ENV,                                                                   \
        LINE,                                                                  \
        INFO,                                                                  \
        ACTIONS)                                                               \
    fklCgActVectorPushBack2((ACTIONS),                                         \
            make_cg_act((F),                                                   \
                    (STACK),                                                   \
                    (NEXT_EXPRESSIONS),                                        \
                    (SCOPE),                                                   \
                    (MACRO_SCOPE),                                             \
                    (ENV),                                                     \
                    (LINE),                                                    \
                    NULL,                                                      \
                    (INFO)))

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
        FklIns drop = FKL_MAKE_INS_I(FKL_OP_DROP);
        FklVMvalue *retval = create_0len_bcl(exe);
        size_t top = stack->size;
        for (size_t i = 0; i < top; i++) {
            FklVMvalue *cur = stack->base[i];
            FklByteCodelnt *co = FKL_VM_CO(cur);
            FklByteCodelnt *rco = FKL_VM_CO(retval);
            if (co->bc.len) {
                fklCodeLntConcat(rco, co);
                if (i < top - 1) {
                    fklByteCodeLntPushBackIns(rco, drop, fid, line, scope);
                }
            }
        }
        stack->size = 0;
        return retval;
    } else {
        FklIns r = FKL_MAKE_INS_I(FKL_OP_PUSH_NIL);
        return fklCreateVMvalueCodeObjExt(exe, r, fid, line, scope);
    }
}

static FklVMvalue *_begin_exp_bc_process(const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    return sequnce_exp_bc_process(vm, bcl_vec, fid, line, scope);
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

static inline int is_get_var_ref_ins(const FklIns ins) {
    return OP(ins) == FKL_OP_GET_VAR_REF;
}

static inline FklBuiltinInlineFunc is_inlinable_func_ref(const FklByteCode *bc,
        FklVMvalueCgEnv *env,
        uint32_t argNum,
        FklVMvalueCgInfo *info) {
    FklInsArg arg;
    const FklIns *ins = &bc->code[0];
    unsigned ins_len = (unsigned)fklGetInsOpArg(ins, &arg);
    if (is_get_var_ref_ins(*ins) && bc->len == ins_len) {
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
    FklVM *vm = args->ctx->vm;
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
            return inlFunc(vm, bcl_vec->base + 1, fid, line, scope);
        } else {
            FklVMvalue *retval = create_0len_bcl(vm);
            FklVMvalue **base = bcl_vec->base;
            FklVMvalue **const end = base + bcl_vec->size;
            for (; base < end; ++base) {
                FklVMvalue *cur = *base;
                fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
            }
            bcl_vec->size = 0;
            FklIns setBp = FKL_MAKE_INS_I(FKL_OP_SET_BP);
            FklIns call = FKL_MAKE_INS_I(FKL_OP_CALL);

            FklByteCodelnt *rco = FKL_VM_CO(retval);
            fklByteCodeLntInsertFrontIns(setBp, rco, fid, line, scope);
            fklByteCodeLntPushBackIns(rco, call, fid, line, scope);
            return retval;
        }
    } else {
        FklIns ins = FKL_MAKE_INS_I(FKL_OP_PUSH_NIL);
        return fklCreateVMvalueCodeObjExt(vm, ins, fid, line, scope);
    }
}

static inline FklVMvalue *make_not_in_top_level_error(FklVM *exe,
        FklVMvalue *place) {
    return FKL_MAKE_VM_ERR(FKL_ERR_SYNTAXERROR,
            exe,
            "Expression %S should in top level",
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
            fklCreateVMvalueStr1(exe, msg2),
            place);
}

static inline FklVMvalue *
make_import_reader_macro_error(FklVM *exe, const char *msg, FklVMvalue *place) {
    if (place) {
        return FKL_MAKE_VM_ERR(FKL_ERR_IMPORT_READER_MACRO_ERROR,
                exe,
                "%s %S",
                fklCreateVMvalueStr1(exe, msg),
                place);
    }
    return FKL_MAKE_VM_ERR(FKL_ERR_IMPORT_READER_MACRO_ERROR,
            exe,
            "%S",
            fklCreateVMvalueStr1(exe, msg));
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
    return FKL_MAKE_VM_ERR(FKL_ERR_IMPORTFAILED, exe, str, place);
}

static inline FklVMvalue *make_grammer_create_error(FklVM *exe) {
    return FKL_MAKE_VM_ERR(FKL_ERR_GRAMMER_CREATE_FAILED,
            exe,
            "Failed to create grammer");
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
    return FKL_MAKE_VM_ERR(FKL_ERR_EXPORT_ERROR, exe, msg, place);
}

static void codegen_funcall(const FklPmatchRes *rest,
        FklCgActVector *actions,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info,
        FklCgCtx *ctx) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;
    CgExpQueue *queue = cgExpQueueCreate();
    FklVMvalue *last = NULL;
    pushListItemToQueue(rest->value, queue, &last);
    if (last != FKL_VM_NIL) {
        error_state->error = make_syntax_error(vm, rest->value);
        error_state->line = CURLINE(rest->container);

        cgExpQueueDestroy(queue);
    } else {
        MAKE_AND_PUSH_CG_ACT(_funcall_exp_bc_process,
                createStackCtx(),
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
    MAKE_AND_PUSH_CG_ACT(_begin_exp_bc_process,
            createStackCtx(),
            createDefaultQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(rest->container),
            info,
            actions);
}

static inline int reset_flag_and_check_var_be_refed(FklVMvalueCgEnv *env,
        uint32_t scope_id,
        uint32_t *start) {
    FklCgEnvSlotVector *flags = &env->slots;
    const FklCgEnvScope *sc = fklCgEnvScopeGet(env, scope_id);
    *start = sc->start;
    fklResolveRef(env, scope_id, NULL);
    int r = 0;
    uint32_t last = sc->start + sc->end;
    uint32_t i = sc->start;
    uint32_t s = i;
    for (; i < last; i++) {
        r = flags->base[i] == FKL_CODEGEN_ENV_SLOT_REF;
        flags->base[i] = FKL_CODEGEN_ENV_SLOT_NONE;
        if (r) {
            s = i;
            break;
        }
    }

    for (; i < last; i++) {
        flags->base[i] = FKL_CODEGEN_ENV_SLOT_NONE;
    }

    *start = s;
    return r;
}

static inline FklVMvalue *append_close_ref_ins(FklVM *exe,
        InsAppendMode m,
        FklVMvalue *retval,
        uint32_t s,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    return set_and_append_ins_with_unsigned_imm(exe,
            m,
            retval,
            FKL_OP_CLOSE_REF,
            s,
            fid,
            line,
            scope);
}

static inline void check_and_close_ref(FklVM *exe,
        FklVMvalue *retval,
        uint32_t scope,
        FklVMvalueCgEnv *env,
        FklVMvalue *fid,
        uint32_t line) {
    uint32_t start = 0;
    if (reset_flag_and_check_var_be_refed(env, scope, &start)) {
        append_close_ref_ins(exe,
                INS_APPEND_BACK,
                retval,
                start,
                fid,
                line,
                scope);
    }
}

static FklVMvalue *_local_exp_bc_process(const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = sequnce_exp_bc_process(vm, bcl_vec, fid, line, scope);
    check_and_close_ref(vm, retval, scope, env, fid, line);
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
    MAKE_AND_PUSH_CG_ACT(_local_exp_bc_process,
            createStackCtx(),
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
    MAKE_AND_PUSH_CG_ACT(_local_exp_bc_process,
            createStackCtx(),
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

static FklCgActCtxMt Let1ContextMt = {
    .size = sizeof(Let1Context),
    .finalize = _let1_finalizer,
};

static FklCgActCtx *createLet1CgCtx(FklValueVector *ss) {
    FklCgActCtx *r = createCgActCtx(&Let1ContextMt);
    FKL_TYPE_CAST(Let1Context *, r->d)->ss = ss;
    return r;
}

typedef struct {
    FklVMvalue *car;
    FklVMvalue *cdr;
} PairCtx;

static void pair_ctx_atomic(FklVMgc *gc, void *data) {
    PairCtx *d = data;
    fklVMgcToGray(d->car, gc);
    fklVMgcToGray(d->cdr, gc);
}

static FklCgActCtxMt PairCtxMt = {
    .size = sizeof(PairCtx),
    .atomic = pair_ctx_atomic,
};

static FklCgActCtx *createPairCtx(FklVMvalue *car, FklVMvalue *cdr) {
    FklCgActCtx *r = createCgActCtx(&PairCtxMt);
    PairCtx *b = FKL_TYPE_CAST(PairCtx *, r->d);
    b->car = car;
    b->cdr = cdr;
    return r;
}

typedef struct Do1Context {
    FklUintVector *ss;
} Do1Context;

static void _do1_finalizer(void *d) {
    Do1Context *dd = (Do1Context *)d;
    fklUintVectorDestroy(dd->ss);
}

static FklCgActCtxMt Do1ContextMethodTable = {
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
    FklVM *vm = args->ctx->vm;
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
        append_pop_loc_ins(vm, INS_APPEND_FRONT, retval, idx, fid, line, scope);
    }
    if (!fklValueVectorIsEmpty(bcl_vec)) {
        FklVMvalue *args = *fklValueVectorPopBackNonNull(bcl_vec);
        fklCodeLntReverseConcat(FKL_VM_CO(args), FKL_VM_CO(retval));
    }
    return retval;
}

static FklVMvalue *_let_arg_exp_bc_process(const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
    FklValueVector *bcl_vec = args->bcl_vec;

    if (bcl_vec->size) {
        FklVMvalue *retval = create_0len_bcl(vm);
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
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    Let1Context *let_ctx = FKL_TYPE_CAST(Let1Context *, data);
    FklValueVector *symbolStack = let_ctx->ss;

    FklVMvalue *retval = create_0len_bcl(vm);
    FklVMvalue **cur_sid = symbolStack->base;
    FklVMvalue **const sid_end = cur_sid + symbolStack->size;
    FklVMvalue **cur_bcl = bcl_vec->base;
    for (; cur_sid < sid_end; ++cur_sid, ++cur_bcl) {
        FklVMvalue *value_bc = *cur_bcl;
        FklVMvalue *id = *cur_sid;
        uint32_t idx = fklAddCgDefBySid(id, scope, env)->idx;
        fklResolveCgPreDef(id, scope, env);
        fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(value_bc));
        append_pop_loc_ins(vm, INS_APPEND_BACK, retval, idx, fid, line, scope);
    }
    bcl_vec->size = 0;
    return retval;
}

static void codegen_let1(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *orig = args->orig;

    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    FklValueVector *symStack = fklValueVectorCreate(8);
    const FklPmatchRes *first = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(first->value)) {
        fklValueVectorDestroy(symStack);
        error_state->error = make_syntax_error(vm, orig->value);
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
            error_state->error = make_syntax_error(vm, orig->value);
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
    FklCgAct *let1Action = make_cg_act(_let1_exp_bc_process,
            createLet1CgCtx(symStack),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            info);

    fklCgActVectorPushBack2(actions, let1Action);

    FklCgAct *restAction = make_cg_act(_local_exp_bc_process,
            createStackCtx(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->value),
            let1Action,
            info);
    fklCgActVectorPushBack2(actions, restAction);

    FklCgAct *argAction = make_cg_act(_let_arg_exp_bc_process,
            createStackCtx(),
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
    FklVM *vm = args->ctx->vm;
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

    FklVMvalue *restLet8 = fklCreateVMvaluePair(vm, argl->value, rest->value);
    FKL_VM_CDR(orig->value) = restLet8;

    ListElm a[3] = {
        { .v = letHead, .line = CURLINE(letHead) },
        { .v = first_name, .line = CURLINE(first_name) },
        { .v = orig->value, .line = CURLINE(orig->container) },
    };
    letHead = create_list(a, 3, CURLINE(orig->value), vm, ctx->lnt);
    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush2(queue,
            (FklPmatchRes){
                .value = letHead,
                .container = orig->value,
            });

    MAKE_AND_PUSH_CG_ACT(_default_bc_process,
            createStackCtx(),
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *orig = args->orig;

    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    FklValueVector *symStack = fklValueVectorCreate(8);
    const FklPmatchRes *first = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(first->value)) {
        fklValueVectorDestroy(symStack);
        error_state->error = make_syntax_error(vm, orig->value);
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
        error_state->error = make_syntax_error(vm, orig->value);
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
    FklCgAct *let1Action = make_cg_act(_letrec_exp_bc_process,
            createStackCtx(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            info);

    fklCgActVectorPushBack2(actions, let1Action);

    FklCgAct *restAction = make_cg_act(_local_exp_bc_process,
            createStackCtx(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            let1Action,
            info);
    fklCgActVectorPushBack2(actions, restAction);

    FklCgAct *argAction = make_cg_act(_letrec_arg_exp_bc_process,
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
    uint32_t scope = args->scope;
    FklVM *vm = args->ctx->vm;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *rest = *fklValueVectorPopBackNonNull(bcl_vec);
    FklVMvalue *value = *fklValueVectorPopBackNonNull(bcl_vec);
    FklVMvalue *cond = *fklValueVectorPopBackNonNull(bcl_vec);

    FklIns pop = FKL_MAKE_INS_I(FKL_OP_DROP);

    fklByteCodeLntInsertFrontIns(pop, FKL_VM_CO(rest), fid, line, scope);
    insert_jmp_if_true_and_jmp_back_between(vm, cond, rest, fid, line, scope);

    fklCodeLntConcat(FKL_VM_CO(cond), FKL_VM_CO(rest));

    if (FKL_VM_CO(value)->bc.len) {
        fklByteCodeLntPushBackIns(FKL_VM_CO(cond), pop, fid, line, scope);
    }
    fklCodeLntReverseConcat(FKL_VM_CO(cond), FKL_VM_CO(value));

    return value;
}

static FklVMvalue *_do_rest_exp_bc_process(const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    if (bcl_vec->size) {
        FklIns pop = FKL_MAKE_INS_I(FKL_OP_DROP);
        FklVMvalue *r = sequnce_exp_bc_process(vm, bcl_vec, fid, line, scope);
        fklByteCodeLntPushBackIns(FKL_VM_CO(r), pop, fid, line, scope);
        return r;
    }
    return create_0len_bcl(vm);
}

static void codegen_do0(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
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

    FklCgAct *do0Action = make_cg_act(_do0_exp_bc_process,
            createStackCtx(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(cond->container),
            NULL,
            info);
    fklCgActVectorPushBack2(actions, do0Action);

    MAKE_AND_PUSH_CG_ACT(_do_rest_exp_bc_process,
            createStackCtx(),
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
        FklCgAct *do0VAction = make_cg_act(_default_bc_process,
                createStackCtx(),
                createMustHasRetvalQueueNextExpression(vQueue),
                cs,
                cms,
                env,
                CURLINE(orig->container),
                do0Action,
                info);
        fklCgActVectorPushBack2(actions, do0VAction);
    } else {
        FklVMvalue *v = create_0len_bcl(vm);
        FklCgAct *action = make_cg_act(_default_bc_process,
                createStackCtx(),
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
    FklCgAct *do0CAction = make_cg_act(_default_bc_process,
            createStackCtx(),
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *ret = create_0len_bcl(vm);
    Do1Context *let_ctx = FKL_TYPE_CAST(Do1Context *, data);
    FklUintVector *ss = let_ctx->ss;

    FklIns pop = FKL_MAKE_INS_I(FKL_OP_DROP);

    uint64_t *idxbase = ss->base;
    FklVMvalue **bclBase = bcl_vec->base;
    uint32_t top = bcl_vec->size;
    for (uint32_t i = 0; i < top; i++) {
        uint32_t idx = idxbase[i];
        FklVMvalue *curBcl = bclBase[i];
        append_put_loc_ins(vm, INS_APPEND_BACK, curBcl, idx, fid, line, scope);
        fklByteCodeLntPushBackIns(FKL_VM_CO(curBcl), pop, fid, line, scope);
        fklCodeLntConcat(FKL_VM_CO(ret), FKL_VM_CO(curBcl));
    }
    return ret;
}

static FklVMvalue *_do1_next_val_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    Do1Context *let_ctx = FKL_TYPE_CAST(Do1Context *, data);
    FklUintVector *ss = let_ctx->ss;

    if (bcl_vec->size) {
        FklVMvalue *ret = create_0len_bcl(vm);
        FklIns pop = FKL_MAKE_INS_I(FKL_OP_DROP);

        uint64_t *idxbase = ss->base;
        FklVMvalue **bclBase = bcl_vec->base;
        uint32_t top = bcl_vec->size;
        for (uint32_t i = 0; i < top; i++) {
            uint32_t idx = idxbase[i];
            FklVMvalue *curBcl = bclBase[i];
            append_put_loc_ins(vm,
                    INS_APPEND_BACK,
                    curBcl,
                    idx,
                    fid,
                    line,
                    scope);
            fklByteCodeLntPushBackIns(FKL_VM_CO(curBcl), pop, fid, line, scope);
            fklCodeLntConcat(FKL_VM_CO(ret), FKL_VM_CO(curBcl));
        }
        return ret;
    }
    return NULL;
}

static FklVMvalue *_do1_bc_process(const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
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

    FklIns pop = FKL_MAKE_INS_I(FKL_OP_DROP);

    fklByteCodeLntInsertFrontIns(pop, FKL_VM_CO(rest), fid, line, scope);
    if (next) {
        fklCodeLntConcat(FKL_VM_CO(rest), FKL_VM_CO(next));
    }

    insert_jmp_if_true_and_jmp_back_between(vm, cond, rest, fid, line, scope);

    fklCodeLntConcat(FKL_VM_CO(cond), FKL_VM_CO(rest));

    if (FKL_VM_CO(value)->bc.len)
        fklByteCodeLntPushBackIns(FKL_VM_CO(cond), pop, fid, line, scope);
    fklCodeLntReverseConcat(FKL_VM_CO(cond), FKL_VM_CO(value));

    fklCodeLntReverseConcat(FKL_VM_CO(init), FKL_VM_CO(value));
    check_and_close_ref(vm, value, scope, env, fid, line);
    return value;
}

static void codegen_do1(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
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

        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }

    FklCgAct *do1Action = make_cg_act(_do1_bc_process,
            createStackCtx(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            info);
    fklCgActVectorPushBack2(actions, do1Action);

    FklCgAct *do1NextValAction = make_cg_act(_do1_next_val_bc_process,
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
    FklCgAct *do1RestAction = make_cg_act(_do_rest_exp_bc_process,
            createStackCtx(),
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
        FklCgAct *do1VAction = make_cg_act(_default_bc_process,
                createStackCtx(),
                createMustHasRetvalQueueNextExpression(vQueue),
                cs,
                cms,
                env,
                CURLINE(orig->container),
                do1Action,
                info);
        fklCgActVectorPushBack2(actions, do1VAction);
    } else {
        FklVMvalue *v = create_0len_bcl(vm);
        FklCgAct *action = make_cg_act(_default_bc_process,
                createStackCtx(),
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
    FklCgAct *do1CAction = make_cg_act(_default_bc_process,
            createStackCtx(),
            createMustHasRetvalQueueNextExpression(cQueue),
            cs,
            cms,
            env,
            CURLINE(orig->container),
            do1Action,
            info);
    fklCgActVectorPushBack2(actions, do1CAction);

    FklCgAct *do1InitValAction = make_cg_act(_do1_init_val_bc_process,
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
get_sid_with_idx(const FklCgEnvScope *sc, uint32_t idx, FklCgCtx *ctx) {
    for (FklSymDefHashMapNode *l = sc->defs.first; l; l = l->next) {
        if (l->v.idx == idx)
            return l->k.sid;
    }
    return 0;
}

static inline FklVMvalue *
get_sid_with_ref_idx(FklSymDefHashMap *refs, uint32_t idx, FklCgCtx *ctx) {
    for (FklSymDefHashMapNode *l = refs->first; l; l = l->next) {
        if (l->v.idx == idx)
            return l->k.sid;
    }
    return 0;
}

static inline FklVMvalue *process_set_var(FklValueVector *stack,
        FklVMvalueCgInfo *info,
        FklCgCtx *ctx,
        FklVMvalueCgEnv *env,
        uint32_t scope_id,
        FklVMvalue *fid,
        uint32_t line) {
    if (stack->size >= 2) {
        FklVMvalue *cur = *fklValueVectorPopBackNonNull(stack);
        FklVMvalue *popVar = *fklValueVectorPopBackNonNull(stack);
        const FklIns *cur_ins = &FKL_VM_CO(cur)->bc.code[0];
        if (fklIsLoadProto(*cur_ins)) {
            const FklIns *popVar_ins = &FKL_VM_CO(popVar)->bc.code[0];
            FklInsArg arg;
            fklGetInsOpArg(cur_ins, &arg);
            uint64_t proto_id = arg.ux;
            fklGetInsOpArg(popVar_ins, &arg);
            uint64_t idx = arg.ux;

            FklVMvalue *pt_v = env->child_proc_protos.base[proto_id];
            FKL_ASSERT(pt_v && fklIsVMvalueProto(pt_v));
            FklVMvalueProto *proto = fklVMvalueProto(pt_v);
            if (fklIsPutLocIns(*popVar_ins)) {
                if (proto->name == NULL) {
                    const FklCgEnvScope *sc = fklCgEnvScopeGet(env, scope_id);
                    proto->name = get_sid_with_idx(sc, idx, ctx);
                }
            } else if (fklIsPutVarRefIns(*popVar_ins)) {
                if (proto->name == NULL) {
                    proto->name = get_sid_with_ref_idx(&env->refs, idx, ctx);
                }
            }
        }

        fklCodeLntReverseConcat(FKL_VM_CO(cur), FKL_VM_CO(popVar));
        return popVar;
    } else {
        FklVMvalue *popVar = *fklValueVectorPopBackNonNull(stack);
        FklByteCodelnt *rco = FKL_VM_CO(popVar);
        FklIns ins = FKL_MAKE_INS_I(FKL_OP_PUSH_NIL);
        fklByteCodeLntInsertFrontIns(ins, rco, fid, line, scope_id);
        return popVar;
    }
}

typedef struct {
    FklVMvalue *id;
    const FklVMvalue *container;
    uint32_t scope;
    uint32_t line;
} DefineVarContext;

static const FklCgActCtxMt DefineVarContextMethodTable = {
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
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    DefineVarContext *var_ctx = FKL_TYPE_CAST(DefineVarContext *, data);
    uint32_t idx = fklAddCgDefBySid(var_ctx->id, var_ctx->scope, env)->idx;
    fklResolveCgPreDef(var_ctx->id, var_ctx->scope, env);
    fklValueVectorInsertFront2(bcl_vec,
            append_put_loc_ins(vm,
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
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = NULL;
    FklIns ret = FKL_MAKE_INS_I(FKL_OP_RET);
    if (bcl_vec->size > 1) {
        FklIns drop = FKL_MAKE_INS_I(FKL_OP_DROP);
        retval = create_0len_bcl(vm);
        size_t top = bcl_vec->size;
        for (size_t i = 1; i < top; i++) {
            FklVMvalue *cur = bcl_vec->base[i];
            if (FKL_VM_CO(cur)->bc.len) {
                fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
                if (i < top - 1) {
                    FklByteCodelnt *rco = FKL_VM_CO(retval);
                    fklByteCodeLntPushBackIns(rco, drop, fid, line, scope);
                }
            }
        }
        fklByteCodeLntPushBackIns(FKL_VM_CO(retval), ret, fid, line, scope);
    } else {
        FklIns push_nil = FKL_MAKE_INS_I(FKL_OP_PUSH_NIL);
        retval = fklCreateVMvalueCodeObjExt(vm, push_nil, fid, line, scope);
        fklByteCodeLntPushBackIns(FKL_VM_CO(retval), ret, fid, line, scope);
    }
    fklCodeLntReverseConcat(FKL_VM_CO(bcl_vec->base[0]), FKL_VM_CO(retval));
    bcl_vec->size = 0;
    fklScanAndSetTailCall(&FKL_VM_CO(retval)->bc);

    FklVMvalueProto *proto = fklCreateVMvalueProto2(vm, env);
    (void)proto;

    append_push_proc_ins(vm,
            INS_APPEND_FRONT,
            retval,
            env->proto_id,
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
    FklIns check_arg = FKL_MAKE_INS_IsA(FKL_OP_CHECK_ARG, rest_list);
    check_arg = set_uB(check_arg, arg_count);

    FklByteCodelnt *rco = FKL_VM_CO(retval);
    fklByteCodeLntPushBackIns(rco, check_arg, info->fid, CURLINE(args), 1);
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
    FklIns check_arg = FKL_MAKE_INS_IsA(FKL_OP_CHECK_ARG, 0);
    check_arg = set_uB(check_arg, top);
    FklByteCodelnt *rco = FKL_VM_CO(retval);
    fklByteCodeLntPushBackIns(rco, check_arg, info->fid, curline, 1);
    return retval;
}

static FklVMvalue *_named_let_set_var_exp_bc_process(
        const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
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
        FklIns push_nil = FKL_MAKE_INS_I(FKL_OP_PUSH_NIL);
        FklByteCodelnt *rco = FKL_VM_CO(pop_var);
        fklByteCodeLntInsertFrontIns(push_nil, rco, fid, line, scope);
    }
    check_and_close_ref(vm, pop_var, scope, env, fid, line);
    return pop_var;
}

static inline void
add_func_rpl(FklCgCtx *ctx, FklVMvalueCgRplHashMap *rpls, FklVMvalue *value) {
    FklVMvalue *sym = add_symbol_cstr(ctx, "*func*");
    FklVMvalueCgRpl *rpl = fklCreateVMvalueCgRpl(ctx, value);
    fklCgRplHashMapSet(rpls, sym, rpl);
}

static void codegen_named_let0(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg0);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms = NULL;
    cms = fklCreateVMvalueCgMacroScope(ctx, macro_scope);

    MAKE_AND_PUSH_CG_ACT(_funcall_exp_bc_process,
            createStackCtx(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            info,
            actions);

    uint32_t idx = fklAddCgDefBySid(name->value, cs, env)->idx;

    add_func_rpl(ctx, cms->replacements, name->value);

    FklCgAct *action = make_cg_act(_named_let_set_var_exp_bc_process,
            createStackCtx(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            info);
    fklValueVectorPushBack2(&action->bcl_vector,
            append_put_loc_ins(vm,
                    INS_APPEND_BACK,
                    NULL,
                    idx,
                    info->fid,
                    CURLINE(orig->container),
                    scope));
    fklCgActVectorPushBack2(actions, action);

    FklVMvalueCgEnv *lambda_env = fklCreateVMvalueCgEnv(ctx,
            &(const FklCgEnvCreateArgs){
                .prev_env = env,
                .prev_ms = cms,
                .parent_scope = cs,
                .filename = info->fid,
                .name = name->value,
                .line = CURLINE(orig->container),
            });
    FklVMvalue *argsNode = caddr(orig->value);
    FklVMvalue *argBc = processArgs(vm, argsNode, lambda_env, info);
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);

    FklCgAct *action1 = make_cg_act(_lambda_exp_bc_process,
            createStackCtx(),
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *orig = args->orig;

    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;
    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg0);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    FklValueVector *symStack = fklValueVectorCreate(8);
    const FklPmatchRes *first = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(first->value)) {
        fklValueVectorDestroy(symStack);
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    const FklPmatchRes *argl = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);

    uint32_t cs = enter_new_scope(scope, env);
    FklVMvalueCgMacroScope *cms =
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);

    FklVMvalueCgEnv *lambda_env = fklCreateVMvalueCgEnv(ctx,
            &(const FklCgEnvCreateArgs){
                .prev_env = env,
                .prev_ms = cms,
                .parent_scope = cs,
                .filename = info->fid,
                .name = name->value,
                .line = CURLINE(orig->container),
            });
    fklAddCgDefBySid(first->value, 1, lambda_env);
    fklValueVectorPushBack2(symStack, first->value);

    if (!is_valid_let_args(argl->value,
                lambda_env,
                1,
                symStack,
                builtin_pattern_node)) {
        fklValueVectorDestroy(symStack);
        error_state->error = make_syntax_error(vm, orig->value);
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

    FklCgAct *funcallAction = make_cg_act(_funcall_exp_bc_process,
            createStackCtx(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            NULL,
            info);
    fklCgActVectorPushBack2(actions, funcallAction);

    uint32_t idx = fklAddCgDefBySid(name->value, cs, env)->idx;

    add_func_rpl(ctx, cms->replacements, name->value);

    fklCgActVectorPushBack2(actions,
            make_cg_act(_let_arg_exp_bc_process,
                    createStackCtx(),
                    createMustHasRetvalQueueNextExpression(valueQueue),
                    scope,
                    macro_scope,
                    env,
                    CURLINE(first->container),
                    funcallAction,
                    info));

    FklCgAct *action = make_cg_act(_named_let_set_var_exp_bc_process,
            createStackCtx(),
            NULL,
            cs,
            cms,
            env,
            CURLINE(orig->container),
            funcallAction,
            info);
    fklValueVectorPushBack2(&action->bcl_vector,
            append_put_loc_ins(vm,
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

    FklVMvalue *argBc = processArgsInStack(vm,
            symStack,
            lambda_env,
            info,
            CURLINE(orig->container));

    fklValueVectorDestroy(symStack);

    FklCgAct *action1 = make_cg_act(_lambda_exp_bc_process,
            createStackCtx(),
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    if (bcl_vec->size) {
        FklIns drop = FKL_MAKE_INS_I(FKL_OP_DROP);
        FklVMvalue *retval = create_0len_bcl(vm);
        while (!fklValueVectorIsEmpty(bcl_vec)) {
            FklByteCodelnt *rco = FKL_VM_CO(retval);
            if (rco->bc.len) {
                fklByteCodeLntInsertFrontIns(drop, rco, fid, line, scope);
                append_jmp_ins(vm,
                        INS_APPEND_FRONT,
                        retval,
                        rco->bc.len,
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
    } else {
        return append_push_i24_ins(vm,
                INS_APPEND_BACK,
                NULL,
                1,
                fid,
                line,
                scope);
    }
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
    MAKE_AND_PUSH_CG_ACT(_and_exp_bc_process,
            createStackCtx(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            info,
            actions);
}

static FklVMvalue *_or_exp_bc_process(const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    if (bcl_vec->size) {
        FklIns drop = FKL_MAKE_INS_I(FKL_OP_DROP);
        FklVMvalue *retval = create_0len_bcl(vm);
        while (!fklValueVectorIsEmpty(bcl_vec)) {
            FklByteCodelnt *rco = FKL_VM_CO(retval);
            if (rco->bc.len) {
                fklByteCodeLntInsertFrontIns(drop, rco, fid, line, scope);
                append_jmp_ins(vm,
                        INS_APPEND_FRONT,
                        retval,
                        rco->bc.len,
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
    } else {
        FklIns i = FKL_MAKE_INS_I(FKL_OP_PUSH_NIL);
        return fklCreateVMvalueCodeObjExt(vm, i, fid, line, scope);
    }
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
    MAKE_AND_PUSH_CG_ACT(_or_exp_bc_process,
            createStackCtx(),
            createDefaultQueueNextExpression(queue),
            cs,
            cms,
            env,
            CURLINE(rest->container),
            info,
            actions);
}

int fklIsRplDefined(FklVMvalue *id, FklVMvalueCgEnv *env) {
    return fklCgRplHashMapGet(env->macros->replacements, id) != NULL;
}

static void codegen_lambda(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    const FklPmatchRes *orig = args->orig;
    FklCgErrorState *error_state = ctx->error_state;

    const FklPmatchRes *argl = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    FklVMvalueCgEnv *lambda_env = fklCreateVMvalueCgEnv(ctx, //
            &(const FklCgEnvCreateArgs){
                .prev_env = env,
                .prev_ms = macro_scope,
                .parent_scope = scope,
                .filename = info->fid,
                .name = FKL_VM_NIL,
                .line = CURLINE(orig->container),
            });
    FklVMvalue *argsBc = processArgs(vm, argl->value, lambda_env, info);
    if (!argsBc) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    CgExpQueue *queue = cgExpQueueCreate();
    pushListItemToQueue(rest->value, queue, NULL);

    FklCgAct *action = make_cg_act(_lambda_exp_bc_process,
            createStackCtx(),
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
    FklSymDefHashMapElm *def = fklGetCgDefByIdInScope(id, scope, env);
    return def && def->v.isConst;
}

static inline int
is_variable_defined(FklVMvalue *id, uint32_t scope, FklVMvalueCgEnv *env) {
    FklSymDefHashMapElm *def = fklGetCgDefByIdInScope(id, scope, env);
    return def != NULL;
}

static void codegen_define(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    const FklPmatchRes *orig = args->orig;
    FklCgErrorState *error_state = ctx->error_state;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->error = make_assign_const_error(vm, name->value);
        error_state->line = CURLINE(name->container);
        return;
    }
    if (!is_variable_defined(name->value, scope, env))
        fklAddCgPreDefBySid(name->value, scope, 0, env);

    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, value);
    MAKE_AND_PUSH_CG_ACT(_def_var_exp_bc_process,
            create_def_var_context(name, scope, CURLINE(orig->container)),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            info,
            actions);
}

static inline FklUnbound *get_resolvable_assign_ref(FklVMvalue *id,
        uint32_t scope,
        FklVMvalueCgEnv *env) {
    FklUnbound *urefs = env->uref.base;
    uint32_t top = env->uref.size;
    for (uint32_t i = 0; i < top; i++) {
        FklUnbound *cur = &urefs[i];
        if (cur->sid == id && cur->scope == scope && cur->assign)
            return cur;
    }
    return NULL;
}

static FklVMvalue *_def_const_exp_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;
    FklCgErrorState *error_state = ctx->error_state;

    DefineVarContext *var_ctx = FKL_TYPE_CAST(DefineVarContext *, data);
    FklUnbound *assign_uref =
            get_resolvable_assign_ref(var_ctx->id, var_ctx->scope, env);
    if (assign_uref) {
        error_state->error = make_assign_const_error(vm, var_ctx->id);
        error_state->line = CURLINE(var_ctx->container);
        error_state->fid = assign_uref->fid;
        return NULL;
    }
    FklSymDef *def = fklAddCgDefBySid(var_ctx->id, var_ctx->scope, env);
    def->isConst = 1;
    uint32_t idx = def->idx;
    fklResolveCgPreDef(var_ctx->id, var_ctx->scope, env);
    fklValueVectorInsertFront2(bcl_vec,
            append_put_loc_ins(vm,
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->error = make_assign_const_error(vm, name->value);
        error_state->line = CURLINE(name->container);
        return;
    }
    if (is_variable_defined(name->value, scope, env)) {
        error_state->error = make_redefine_const_error(vm, name->value);
        error_state->line = CURLINE(name->container);
        return;
    } else
        fklAddCgPreDefBySid(name->value, scope, 1, env);

    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, value);
    MAKE_AND_PUSH_CG_ACT(_def_const_exp_bc_process,
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *argl = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->error = make_assign_const_error(vm, name->value);
        error_state->line = CURLINE(name->container);
        return;
    }

    FklVMvalueCgEnv *lambda_env = fklCreateVMvalueCgEnv(ctx, //
            &(const FklCgEnvCreateArgs){
                .prev_env = env,
                .prev_ms = macro_scope,
                .parent_scope = scope,
                .filename = info->fid,
                .name = name->value,
                .line = CURLINE(orig->container),
            });
    FklVMvalue *argsBc = processArgs(vm, argl->value, lambda_env, info);
    if (!argsBc) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }

    if (!is_variable_defined(name->value, scope, env))
        fklAddCgPreDefBySid(name->value, scope, 0, env);

    FklCgAct *prevAction = make_cg_act(_def_var_exp_bc_process,
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

    add_func_rpl(ctx, lambda_env->macros->replacements, name->value);

    FklCgAct *cur = make_cg_act(_lambda_exp_bc_process,
            createStackCtx(),
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *argl = fklPmatchHashMapGet2(ht, ctx->builtin_sym_args);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    if (is_constant_defined(name->value, scope, env)) {
        error_state->error = make_assign_const_error(vm, name->value);
        error_state->line = CURLINE(name->container);
        return;
    }
    if (is_variable_defined(name->value, scope, env)) {
        error_state->error = make_redefine_const_error(vm, name->value);
        error_state->line = CURLINE(name->container);
        return;
    }

    FklVMvalueCgEnv *lambda_env = fklCreateVMvalueCgEnv(ctx,
            &(FklCgEnvCreateArgs){
                .prev_env = env,
                .prev_ms = macro_scope,
                .parent_scope = scope,
                .filename = info->fid,
                .name = name->value,
                .line = CURLINE(orig->container),
            });

    FklVMvalue *argsBc = processArgs(vm, argl->value, lambda_env, info);
    if (!argsBc) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }

    FklCgAct *prevAction = make_cg_act(_def_const_exp_bc_process,
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

    add_func_rpl(ctx, lambda_env->macros->replacements, name->value);

    FklCgAct *cur = make_cg_act(_lambda_exp_bc_process,
            createStackCtx(),
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(name->value)) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush(queue, value);
    FklSymDefHashMapElm *def;
    def = fklFindSymbolDef(name->value, scope, env);
    FklCgAct *cur = make_cg_act(_set_var_exp_bc_process,
            createStackCtx(),
            createMustHasRetvalQueueNextExpression(queue),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            NULL,
            info);
    if (def) {
        if (def->v.isConst) {
            error_state->error = make_assign_const_error(vm, name->value);
            error_state->line = CURLINE(name->container);
            return;
        }
        fklValueVectorPushBack2(&cur->bcl_vector,
                append_put_loc_ins(vm,
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
            error_state->error = make_assign_const_error(vm, name->value);
            error_state->line = CURLINE(name->container);
            return;
        }
        fklValueVectorPushBack2(&cur->bcl_vector,
                append_put_var_ref_ins(vm,
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
    FklCgAct *cur = make_cg_act(_default_bc_process,
            createStackCtx(),
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

static void add_compiler_macro(const FklCgCtx *c,
        FklVMvalueCgMacroHashMap *macros,
        FklVMvalue *head,
        FklVMvalueCgMacro *macro) {
    FKL_ASSERT(FKL_IS_SYM(head));
    int cover = FKL_PATTERN_NOT_EQUAL;
    FklValueHashMapElm *pmacro = fklCgMacroHashMapRef1(macros, head);
    FklVMvalue *const pattern = macro->pattern;
    FklVMvalue **phead = &pmacro->v;

    for (FklVMvalue *p = *phead; FKL_IS_PAIR(p); p = *phead) {
        FklVMvalueCgMacro *cur = fklVMvalueCgMacro(FKL_VM_CAR(p));
        cover = fklPatternCoverState(cur->pattern, pattern);
        if (cover == FKL_PATTERN_EQUAL || cover == FKL_PATTERN_COVER)
            break;
        phead = &FKL_VM_CDR(p);
    }

    switch (cover) {
    case FKL_PATTERN_NOT_EQUAL:
        pmacro->v = fklCreateVMvaluePair(c->vm, FKL_VM_VAL(macro), pmacro->v);
        break;
    case FKL_PATTERN_EQUAL:
        FKL_VM_CAR(*phead) = FKL_VM_VAL(macro);
        break;

    case FKL_PATTERN_BE_COVER:
    case FKL_PATTERN_COVER:
        *phead = fklCreateVMvaluePair(c->vm, FKL_VM_VAL(macro), *phead);
        break;
    }
}

static inline void add_export_symbol(FklCgCtx *ctx,
        FklVMvalueCgInfo *info,
        FklVMvalue *orig,
        FklVMvalue *rest,
        CgExpQueue *exportQueue) {
    FklVMvalue *prev = orig;
    FklVM *vm = ctx->vm;
    FklVMvalue *head = FKL_VM_CAR(orig);
    for (; FKL_IS_PAIR(rest); rest = FKL_VM_CDR(rest)) {
        FklVMvalue *new_rest = fklCreateVMvaluePair(vm, head, rest);
        put_line_number(info->lnt, new_rest, CURLINE(rest));
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
    FklCgAct *quest = make_cg_act(_default_bc_process,
            createStackCtx(),
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

typedef FklVMvalue *(*ReplacementFunc)(const FklCgCtx *ctx,
        const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info);

static inline ReplacementFunc find_builtin_replacements(FklVMvalue *id,
        FklVMvalue *const builtin_replacement_id[]);

static inline int is_replacement_define(FklVMvalue *value,
        const FklVMvalueCgInfo *info,
        const FklCgCtx *ctx,
        const FklVMvalueCgMacroScope *macro_scope,
        const FklVMvalueCgEnv *env) {
    FklVMvalueCgRpl *rpl = NULL;
    for (const FklVMvalueCgMacroScope *cs = macro_scope; cs; cs = cs->prev) {
        rpl = fklCgRplHashMapGet(cs->replacements, value);
        if (rpl)
            return 1;
    }

    ReplacementFunc f = NULL;
    f = find_builtin_replacements(value, ctx->builtin_replacement_id);
    return f ? 1 : 0;
}

static inline int is_replacement_true(const FklPmatchRes *value,
        const FklVMvalueCgInfo *info,
        const FklCgCtx *ctx,
        const FklVMvalueCgMacroScope *macro_scope,
        const FklVMvalueCgEnv *env) {
    FklVMvalueCgRpl *rpl = NULL;
    for (const FklVMvalueCgMacroScope *cs = macro_scope; cs; cs = cs->prev) {
        rpl = fklCgRplHashMapGet(cs->replacements, value->value);
        if (rpl) {
            return rpl->value != FKL_VM_NIL;
        }
    }

    ReplacementFunc f = NULL;
    f = find_builtin_replacements(value->value, ctx->builtin_replacement_id);
    if (f == NULL)
        return 0;

    FklVMvalue *t = f(ctx, value, env, info);
    return t != FKL_VM_NIL;
}

static inline FklVMvalue *get_replacement(const FklPmatchRes *value,
        const FklVMvalueCgInfo *info,
        const FklCgCtx *ctx,
        const FklVMvalueCgMacroScope *macro_scope,
        const FklVMvalueCgEnv *env) {
    FklVMvalueCgRpl *rpl = NULL;
    for (const FklVMvalueCgMacroScope *cs = macro_scope; cs; cs = cs->prev) {
        rpl = fklCgRplHashMapGet(cs->replacements, value->value);
        if (rpl)
            return rpl->value;
    }

    ReplacementFunc f = NULL;
    f = find_builtin_replacements(value->value, ctx->builtin_replacement_id);
    if (f != NULL)
        return f(ctx, value, env, info);
    else
        return NULL;
}

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
        uint32_t scope) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(value->value)) {
        error_state->error = make_syntax_error(vm, exp->value);
        error_state->line = CURLINE(exp->container);
        return 0;
    }
    return fklFindSymbolDef(value->value, scope, env) != NULL;
}

// steal from zuo: https://github.com/racket/zuo
// zuo_is_symbol_module_char
static inline int is_module_path_char(char c) {
    return c != 0 && (isalpha(c) || isdigit(c) || strchr(".-_+/", c) != NULL);
}

// steal from zuo: https://github.com/racket/zuo
// zuo_is_module_path
static inline int is_module_path(const FklString *s) {
    if (s->size == 0)
        return 0;
    uint64_t saw_slash = 0;
    uint64_t const end = s->size - 1;
    for (uint64_t i = 0; i < s->size; ++i) {
        char c = s->str[i];
        if (c == '/') {
            if (i == 0 || i == end || saw_slash == i)
                return 0;
            saw_slash = i + 1;
        } else if (!is_module_path_char(c)) {
            return 0;
        }
    }
    return 1;
}

static inline FklVMvalue *get_priv_sub_mod_name(FklVM *v, const FklString *s) {
    FKL_ASSERT(s->size != 0);
    uint64_t saw_slash = 0;
    uint64_t end = 0;
    for (uint64_t i = 0; i < s->size; ++i) {
        char c = s->str[i];
        if (c == '/')
            saw_slash = i + 1;
        else if (c == '_' && i != 0 && saw_slash == i)
            goto importing_private_module;
    }

    return NULL;
importing_private_module:

    end = saw_slash;
    for (; end < s->size; ++end) {
        if (s->str[end] == '/')
            break;
    }

    FklVMvalue *name = fklCreateVMvalueStr2(v, end, s->str);
    return name;
}

static FklFileType get_mod_file_type(const char *name_cstr, FklStrBuf *buf) {
    fklStrBufPrintf(buf,
            "%s%c%s",
            name_cstr,
            FKL_PATH_SEPARATOR,
            FKL_PACKAGE_MAIN_FILE);

    if (fklIsAccessibleRegFile(fklStrBufBody(buf))) {
        return FKL_FILE_PACKAGE;
    }

    fklStrBufClear(buf);
    fklStrBufPrintf(buf, "%s%s", name_cstr, FKL_SCRIPT_FILE_EXTENSION);

    if (fklIsAccessibleRegFile(fklStrBufBody(buf))) {
        return FKL_FILE_SCRIPT;
    }

    fklStrBufClear(buf);
    fklStrBufPrintf(buf,
            "%s%c%s%s",
            name_cstr,
            FKL_PATH_SEPARATOR,
            FKL_PACKAGE_MAIN_FILE,
            FKL_PRE_COMPILE_FKL_SUFFIX_STR);

    if (fklIsAccessibleRegFile(fklStrBufBody(buf))) {
        return FKL_FILE_PRECOMPILE;
    }

    fklStrBufClear(buf);
    fklStrBufPrintf(buf, "%s%s", name_cstr, FKL_DLL_FILE_EXTENSION);

    if (fklIsAccessibleRegFile(fklStrBufBody(buf))) {
        return FKL_FILE_DLL;
    }

    return FKL_FILE_NONE;
}

FklVMvalue *fklResolveLibPath(FklVM *vm,
        const char *main_dir,
        FklVMvalue *name,
        FklFileType *ft) {
    FklStrBuf in_buf;
    fklInitStrBuf(&in_buf);

    FklStrBuf out_buf;
    fklInitStrBuf(&out_buf);

    fklStrBufPrintf(&in_buf,
            "%s%c%s",
            main_dir,
            FKL_PATH_SEPARATOR,
            FKL_VM_SYM(name)->str);

    const char *name_cstr = fklStrBufBody(&in_buf);

    FklFileType t = get_mod_file_type(name_cstr, &out_buf);

    FklVMvalue *rp = NULL;
    switch (t) {
    case FKL_FILE_NONE:
        // do nothing
        break;
    case FKL_FILE_DLL:
    case FKL_FILE_PRECOMPILE:
    case FKL_FILE_SCRIPT:
    case FKL_FILE_PACKAGE: {
        char *rp_cstr = fklRealpath(fklStrBufBody(&out_buf));
        rp = fklVMaddSymbolCstr(vm, rp_cstr);
        fklZfree(rp_cstr);
    } break;
    }

    if (ft) {
        *ft = t;
    }

    fklUninitStrBuf(&in_buf);
    fklUninitStrBuf(&out_buf);

    return rp;
}

static inline int cfg_check_importable(const FklVMvalueCgInfo *info,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCgCtx *ctx,
        const FklVMvalueCgEnv *env,
        uint32_t scope) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (!FKL_IS_SYM(value->value)) {
        error_state->error = make_syntax_error(vm, exp->value);
        error_state->line = CURLINE(exp->container);
        return 0;
    }

    FklVMvalue *priv_sub_mod_name =
            get_priv_sub_mod_name(vm, FKL_VM_SYM(value->value));

    if (priv_sub_mod_name != NULL)
        return 0;

    FklStrBuf buf;
    fklInitStrBuf(&buf);

    FklFileType type = get_mod_file_type(FKL_VM_SYM(value->value)->str, &buf);
    fklUninitStrBuf(&buf);

    switch (type) {
    case FKL_FILE_NONE:
        return 0;
        break;
    case FKL_FILE_DLL:
    case FKL_FILE_PRECOMPILE:
    case FKL_FILE_SCRIPT:
    case FKL_FILE_PACKAGE:
        return 1;
        break;
    }

    return 0;
}

static inline int cfg_check_macro_defined(const FklVMvalueCgInfo *info,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCgCtx *ctx,
        const FklVMvalueCgEnv *env,
        uint32_t scope,
        const FklVMvalueCgMacroScope *macro_scope) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;
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
            const FklVMvalueCgMacroHashMap *macros = cur_scope->macros;
            FklValueHashMapElm const *pm = fklCgMacroHashMapGet(macros, id);
            if (pm != NULL) {
                r = 1;
                break;
            }
        }
        return r;
    } else if (FKL_IS_BOX(value->value) //
               && FKL_IS_SYM(FKL_VM_BOX(value->value))) {
        FklVMvalue *id = FKL_VM_BOX(value->value);
        return info->g != NULL
            && fklCgRmacroHashMapGet(info->rmacros, id) != NULL;
    } else {
        error_state->error = make_syntax_error(vm, value->value);
        error_state->line = CURLINE(value->container);
        return 0;
    }
}

static inline int cfg_check_eq(const FklVMvalueCgInfo *info,
        const FklPmatchRes *exp,
        const FklPmatchHashMap *ht,
        const FklCgCtx *ctx,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgMacroScope *macro_scope) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *arg0 = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg0);
    const FklPmatchRes *arg1 = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg1);
    if (!FKL_IS_SYM(arg0->value)) {
        error_state->error = make_syntax_error(vm, exp->value);
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
        const FklVMvalueCgMacroScope *macro_scope) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *arg0 = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg0);
    const FklPmatchRes *arg1 = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg1);
    if (!FKL_IS_SYM(arg0->value)
            || !is_valid_compile_check_pattern(arg1->value)) {
        error_state->error = make_syntax_error(vm, exp->value);
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

static inline int is_check_subpattern_true(FklCgCtx *ctx,
        const FklVMvalueCgInfo *info,
        const FklPmatchRes *exp_,
        uint32_t scope,
        const FklVMvalueCgMacroScope *macro_scope,
        const FklVMvalueCgEnv *env) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;
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
    FklPmatchStorage storage = { .ht = &ht };
    fklPushCgPmatchStorage(ctx, &storage);

    cgCfgCtxVectorInit(&s, 8);

    for (;;) {
    loop_start:
        if (exp.value == FKL_VM_NIL)
            r = 0;
        else if (FKL_IS_SYM(exp.value))
            r = is_replacement_true(&exp, info, ctx, macro_scope, env);
        else if (FKL_IS_PAIR(exp.value)) {
            FklVMvalue *old = exp.value;
            exp.value = fklTryExpandCgMacro(ctx, &exp, info, macro_scope);
            if (error_state->error)
                goto exit;
            if (exp.value != old)
                goto loop_start;

            if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                        [FKL_CODEGEN_SUB_PATTERN_DEFINE],
                        exp.value,
                        &ht)) {
                r = cfg_check_defined(info, &exp, &ht, ctx, env, scope);
                if (error_state->error)
                    goto exit;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_IMPORT],
                               exp.value,
                               &ht)) {
                r = cfg_check_importable(info, &exp, &ht, ctx, env, scope);
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
                        macro_scope);
                if (error_state->error)
                    goto exit;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_EQ],
                               exp.value,
                               &ht)) {
                r = cfg_check_eq(info, &exp, &ht, ctx, env, macro_scope);
                if (error_state->error)
                    goto exit;
            } else if (fklPatternMatch(ctx->builtin_sub_pattern_node
                                               [FKL_CODEGEN_SUB_PATTERN_MATCH],
                               exp.value,
                               &ht)) {
                r = cfg_check_matched(info, &exp, &ht, ctx, env, macro_scope);
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
                    error_state->error = make_syntax_error(vm, exp.value);
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
                    error_state->error = make_syntax_error(vm, exp.value);
                    error_state->line = CURLINE(exp.container);
                    goto exit;
                }
            } else {
                error_state->error = make_syntax_error(vm, exp.value);
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

    fklPopCgPmatchStorage(ctx, &storage);
    fklPmatchHashMapUninit(&ht);
    return r;
}

static void codegen_check(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    int r = is_check_subpattern_true(ctx, info, value, scope, macro_scope, env);
    if (error_state->error)
        return;
    FklVMvalue *bcl = fklCreateVMvalueCodeObjExt(vm,
            FKL_MAKE_INS_I(r ? FKL_OP_PUSH_1 : FKL_OP_PUSH_NIL),
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *cond = fklPmatchHashMapGet2(ht, ctx->builtin_sym_cond);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    if (fklVMlistLength(rest->value) % 2 == 1) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    int r = is_check_subpattern_true(ctx, info, cond, scope, macro_scope, env);
    if (error_state->error)
        return;
    if (r) {
        CgExpQueue *q = cgExpQueueCreate();
        cgExpQueuePush(q, value);
        MAKE_AND_PUSH_CG_ACT(_default_bc_process,
                createStackCtx(),
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
        int r = is_check_subpattern_true(ctx,
                info,
                &(FklPmatchRes){
                    .value = cond,
                    .container = rest_value,
                },
                scope,
                macro_scope,
                env);
        if (error_state->error)
            return;
        if (r) {
            CgExpQueue *q = cgExpQueueCreate();
            cgExpQueuePush2(q,
                    (FklPmatchRes){ .value = value, .container = rest_value });
            MAKE_AND_PUSH_CG_ACT(_default_bc_process,
                    createStackCtx(),
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
        error_state->error = make_has_no_value_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
}

static void codegen_quote(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;

    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    push_default_codegen_quest(vm,
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
    FklCgAct *quest = make_cg_act(func,
            createStackCtx(),
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
    FklIns pushBox = FKL_MAKE_INS_IsA(FKL_OP_PUSH_BOX, 1);
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval), pushBox, fid, line, scope);
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
    FklIns push_vec = FKL_MAKE_INS_I(FKL_OP_PUSH_VEC_0);
    FklIns set_bp = FKL_MAKE_INS_I(FKL_OP_SET_BP);
    fklByteCodeLntInsertFrontIns(set_bp, FKL_VM_CO(retval), fid, line, scope);
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval), push_vec, fid, line, scope);
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
    FklIns set_bp = FKL_MAKE_INS_I(FKL_OP_SET_BP);
    fklByteCodeLntInsertFrontIns(set_bp, FKL_VM_CO(retval), fid, line, scope);
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
    FklIns listPush = FKL_MAKE_INS_IsA(FKL_OP_PAIR, FKL_SUBOP_PAIR_UNPACK);
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval), listPush, fid, line, scope);
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
    FklIns pushPair = FKL_MAKE_INS_I(FKL_OP_PUSH_LIST_0);
    FklIns setBp = FKL_MAKE_INS_I(FKL_OP_SET_BP);
    fklByteCodeLntInsertFrontIns(setBp, FKL_VM_CO(retval), fid, line, scope);
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval), pushPair, fid, line, scope);
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
    FklIns pushPair = FKL_MAKE_INS_IsA(FKL_OP_PAIR, FKL_SUBOP_PAIR_APPEND);
    fklByteCodeLntPushBackIns(FKL_VM_CO(retval), pushPair, fid, line, scope);
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

    FklCgAct *curAction = make_cg_act(_qsquote_pair_bc_process,
            createStackCtx(),
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
                FklCgAct *appendAction = make_cg_act(_qsquote_list_bc_process,
                        createStackCtx(),
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
    FklCgAct *action = make_cg_act(_qsquote_vec_bc_process,
            createStackCtx(),
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

    FklVM *vm = args->ctx->vm;
    const QsquoteHelperStruct *top = args->top;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklCgAct *prev = top->prev;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklCgActVector *action_vector = args->action_vector;
    FklVMvalueCgInfo *info = args->info;
    CgQsquoteHelperVector *pending = args->pending;

    FklCgAct *action = make_cg_act(_qsquote_hash_bc_process,
            createStackCtx(),
            NULL,
            scope,
            macro_scope,
            env,
            CURLINE(container),
            prev,
            info);
    fklCgActVectorPushBack2(action_vector, action);

    FklVMvalueHash *hash = FKL_VM_HASH(value);
    FklVMvalue *push_hash = fklCreateVMvalueCodeObjExt(vm,
            FKL_MAKE_INS_I(FKL_OP_PUSH_HASHEQ_0 + hash->eq_type),
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

    FklCgAct *action = make_cg_act(_qsquote_box_bc_process,
            createStackCtx(),
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
    FklVM *vm = args->ctx->vm;
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
    FklPmatchStorage storage = { .ht = &table };
    fklPushCgPmatchStorage(ctx, &storage);

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
        push_default_codegen_quest(vm,
                &top->node,
                action_vector,
                scope,
                macro_scope,
                env,
                prev,
                info);

    fklPopCgPmatchStorage(ctx, &storage);
    fklPmatchHashMapUninit(&table);
}

static void codegen_qsquote(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklVM *vm = args->ctx->vm;
    FklCgCtx *ctx = args->ctx;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;

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
                error_state->error = make_syntax_error(vm, top.node.value);
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = NULL;
    if (bcl_vec->size)
        retval = *fklValueVectorPopBackNonNull(bcl_vec);
    else
        retval = fklCreateVMvalueCodeObjExt(vm,
                FKL_MAKE_INS_I(FKL_OP_PUSH_NIL),
                fid,
                line,
                scope);
    return retval;
}

static inline int is_const_true_bytecode_lnt(const FklByteCodelnt *bcl) {
    const FklIns *cur = bcl->bc.code;
    const FklIns *const end = &cur[bcl->bc.len];
    for (; cur < end; cur++) {
        switch (OP(*cur)) {
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
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *retval = NULL;
    if (bcl_vec->size >= 2) {
        FklVMvalue *prev = bcl_vec->base[0];
        FklVMvalue *first = bcl_vec->base[1];
        retval = create_0len_bcl(vm);

        FklIns drop = FKL_MAKE_INS_I(FKL_OP_DROP);

        fklByteCodeLntInsertFrontIns(drop, FKL_VM_CO(prev), fid, line, scope);

        uint64_t prev_len = FKL_VM_CO(prev)->bc.len;

        int true_bcl = is_const_true_bytecode_lnt(FKL_VM_CO(first));

        if (bcl_vec->size > 2) {
            FklVMvalue *cur = bcl_vec->base[2];
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }

        for (size_t i = 3; i < bcl_vec->size; i++) {
            FklVMvalue *cur = bcl_vec->base[i];
            FklByteCodelnt *rco = FKL_VM_CO(retval);
            fklByteCodeLntPushBackIns(rco, drop, fid, line, scope);
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }

        size_t retval_len = FKL_VM_CO(retval)->bc.len;

        check_and_close_ref(vm, retval, scope, env, fid, line);

        if (bcl_vec->size > 2) {
            append_jmp_ins(vm,
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
                FklByteCodelnt *co = FKL_VM_CO(retval);
                fklByteCodeLntInsertFrontIns(drop, co, fid, line, scope);
                append_jmp_ins(vm,
                        INS_APPEND_FRONT,
                        retval,
                        co->bc.len,
                        JMP_IF_FALSE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
                fklCodeLntReverseConcat(FKL_VM_CO(first), co);
            }
        } else {
            append_jmp_ins(vm,
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklIns drop = FKL_MAKE_INS_I(FKL_OP_DROP);

    FklVMvalue *retval = NULL;
    if (bcl_vec->size) {
        retval = create_0len_bcl(vm);
        FklVMvalue *first = bcl_vec->base[0];
        int true_bcl = is_const_true_bytecode_lnt(FKL_VM_CO(first));
        if (bcl_vec->size > 1) {
            FklVMvalue *cur = bcl_vec->base[1];
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }
        for (size_t i = 2; i < bcl_vec->size; i++) {
            FklVMvalue *cur = bcl_vec->base[i];
            FklByteCodelnt *co = FKL_VM_CO(retval);
            fklByteCodeLntPushBackIns(co, drop, fid, line, scope);
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }

        FklByteCodelnt *co = FKL_VM_CO(retval);
        if (co->bc.len) {
            if (!true_bcl) {
                fklByteCodeLntInsertFrontIns(drop, co, fid, line, scope);
                append_jmp_ins(vm,
                        INS_APPEND_FRONT,
                        retval,
                        co->bc.len,
                        JMP_IF_FALSE,
                        JMP_FORWARD,
                        fid,
                        line,
                        scope);
                fklCodeLntReverseConcat(FKL_VM_CO(first), co);
            }
        } else {
            fklCodeLntReverseConcat(FKL_VM_CO(first), co);
        }
    }
    bcl_vec->size = 0;
    return retval;
}

static void codegen_cond(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    FklCgAct *quest = make_cg_act(_cond_exp_bc_process_0,
            createStackCtx(),
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
                error_state->error = make_syntax_error(vm, orig->value);
                error_state->line = CURLINE(orig->container);
                fklValueVectorUninit(&tmpStack);
                return;
            }
            FklVMvalue *last = FKL_VM_NIL;
            CgExpQueue *curQueue = cgExpQueueCreate();
            pushListItemToQueue(curExp, curQueue, &last);
            if (last != FKL_VM_NIL) {
                error_state->error = make_syntax_error(vm, orig->value);
                error_state->line = CURLINE(orig->container);
                cgExpQueueDestroy(curQueue);
                fklValueVectorUninit(&tmpStack);
                return;
            }
            uint32_t curScope = enter_new_scope(scope, env);
            FklVMvalueCgMacroScope *cms =
                    fklCreateVMvalueCgMacroScope(ctx, macro_scope);
            FklCgAct *curAction = make_cg_act(_cond_exp_bc_process_1,
                    createStackCtx(),
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
            error_state->error = make_syntax_error(vm, orig->value);
            error_state->line = CURLINE(orig->container);
            fklValueVectorUninit(&tmpStack);
            return;
        }
        CgExpQueue *lastQueue = cgExpQueueCreate();
        pushListItemToQueue(lastExp, lastQueue, &last);
        if (last != FKL_VM_NIL) {
            error_state->error = make_syntax_error(vm, orig->value);
            error_state->line = CURLINE(orig->container);
            cgExpQueueDestroy(lastQueue);
            fklValueVectorUninit(&tmpStack);
            return;
        }
        uint32_t curScope = enter_new_scope(scope, env);
        FklVMvalueCgMacroScope *cms =
                fklCreateVMvalueCgMacroScope(ctx, macro_scope);
        fklCgActVectorPushBack2(actions,
                make_cg_act(_cond_exp_bc_process_2,
                        createStackCtx(),
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklIns drop = FKL_MAKE_INS_I(FKL_OP_DROP);

    if (bcl_vec->size >= 2) {
        FklVMvalue *exp = *fklValueVectorPopBackNonNull(bcl_vec);
        FklVMvalue *cond = *fklValueVectorPopBackNonNull(bcl_vec);
        fklByteCodeLntInsertFrontIns(drop, FKL_VM_CO(exp), fid, line, scope);
        append_jmp_ins(vm,
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
    } else if (bcl_vec->size >= 1) {
        return *fklValueVectorPopBackNonNull(bcl_vec);
    } else {
        FklIns i = FKL_MAKE_INS_I(FKL_OP_PUSH_NIL);
        return fklCreateVMvalueCodeObjExt(vm, i, fid, line, scope);
    }
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
            make_cg_act(_if_exp_bc_process_0,
                    createStackCtx(),
                    createMustHasRetvalQueueNextExpression(nextQueue),
                    curScope,
                    cms,
                    env,
                    CURLINE(orig->container),
                    NULL,
                    info));
}

static FklVMvalue *_if_exp_bc_process_1(const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklIns drop = FKL_MAKE_INS_I(FKL_OP_DROP);

    if (bcl_vec->size >= 3) {
        FklVMvalue *exp0 = *fklValueVectorPopBackNonNull(bcl_vec);
        FklVMvalue *cond = *fklValueVectorPopBackNonNull(bcl_vec);
        FklVMvalue *exp1 = *fklValueVectorPopBackNonNull(bcl_vec);
        fklByteCodeLntInsertFrontIns(drop, FKL_VM_CO(exp0), fid, line, scope);
        fklByteCodeLntInsertFrontIns(drop, FKL_VM_CO(exp1), fid, line, scope);
        append_jmp_ins(vm,
                INS_APPEND_BACK,
                exp0,
                FKL_VM_CO(exp1)->bc.len,
                JMP_UNCOND,
                JMP_FORWARD,
                fid,
                line,
                scope);
        append_jmp_ins(vm,
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
        fklByteCodeLntInsertFrontIns(drop, FKL_VM_CO(exp0), fid, line, scope);
        append_jmp_ins(vm,
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
        return fklCreateVMvalueCodeObjExt(vm,
                FKL_MAKE_INS_I(FKL_OP_PUSH_NIL),
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
    FklCgAct *prev = make_cg_act(_if_exp_bc_process_1,
            createStackCtx(),
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
            make_cg_act(_default_bc_process,
                    createStackCtx(),
                    createMustHasRetvalQueueNextExpression(exp1Queue),
                    curScope,
                    cms,
                    env,
                    CURLINE(orig->container),
                    prev,
                    info));
}

static FklVMvalue *_when_exp_bc_process(const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    if (bcl_vec->size) {
        FklVMvalue *cond = bcl_vec->base[0];
        FklVMvalue *retval = create_0len_bcl(vm);

        FklIns drop = FKL_MAKE_INS_I(FKL_OP_DROP);
        FklByteCodelnt *co = FKL_VM_CO(retval);
        for (size_t i = 1; i < bcl_vec->size; i++) {
            FklVMvalue *cur = bcl_vec->base[i];
            fklByteCodeLntPushBackIns(co, drop, fid, line, scope);
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }
        bcl_vec->size = 0;
        if (FKL_VM_CO(retval)->bc.len)
            append_jmp_ins(vm,
                    INS_APPEND_FRONT,
                    retval,
                    FKL_VM_CO(retval)->bc.len,
                    JMP_IF_FALSE,
                    JMP_FORWARD,
                    fid,
                    line,
                    scope);
        fklCodeLntReverseConcat(FKL_VM_CO(cond), FKL_VM_CO(retval));
        check_and_close_ref(vm, retval, scope, env, fid, line);
        return retval;
    } else {
        FklIns i = FKL_MAKE_INS_I(FKL_OP_PUSH_NIL);
        return fklCreateVMvalueCodeObjExt(vm, i, fid, line, scope);
    }
}

static FklVMvalue *_unless_exp_bc_process(const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    if (bcl_vec->size) {
        FklVMvalue *cond = bcl_vec->base[0];
        FklVMvalue *retval = create_0len_bcl(vm);

        FklIns drop = FKL_MAKE_INS_I(FKL_OP_DROP);
        FklByteCodelnt *co = FKL_VM_CO(retval);
        for (size_t i = 1; i < bcl_vec->size; i++) {
            FklVMvalue *cur = bcl_vec->base[i];
            fklByteCodeLntPushBackIns(co, drop, fid, line, scope);
            fklCodeLntConcat(FKL_VM_CO(retval), FKL_VM_CO(cur));
        }
        bcl_vec->size = 0;
        if (co->bc.len)
            append_jmp_ins(vm,
                    INS_APPEND_FRONT,
                    retval,
                    FKL_VM_CO(retval)->bc.len,
                    JMP_IF_TRUE,
                    JMP_FORWARD,
                    fid,
                    line,
                    scope);
        fklCodeLntReverseConcat(FKL_VM_CO(cond), FKL_VM_CO(retval));
        check_and_close_ref(vm, retval, scope, env, fid, line);
        return retval;
    } else {
        FklIns i = FKL_MAKE_INS_I(FKL_OP_PUSH_NIL);
        return fklCreateVMvalueCodeObjExt(vm, i, fid, line, scope);
    }
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
            fklCreateVMvalueCgMacroScope(ctx, macro_scope);

    MAKE_AND_PUSH_CG_ACT(func,
            createStackCtx(),
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

static inline FklVMvalue *getExpressionFromFile(FklCgCtx *ctx,
        FklVMvalueCgInfo *info,
        FILE *fp,
        int *unexpect_eof,
        size_t *output_line,
        FklAnalysisSymbolVector *symbols,
        FklParseStateVector *states) {
    FklVM *vm = ctx->vm;
    char *list = NULL;
    states->size = 0;
    const FklVMvalueCgGrammer *gg = info->g;

    FklReadArgs args = {
        .line = info->curline,
        .symbols = symbols,
        .states = states,
        .vm = vm,
        .ln = ctx->lnt,
        .opa = ctx,
    };

    if (gg != NULL) {
        const FklGrammer *const g = &gg->g;
        ctx->cur_file_dir = info->dir;
        FklParseState const state = { .state = &g->aTable.states[0] };
        fklParseStateVectorPushBack2(states, state);
        list = fklReadWithAnalysisTable(g, fp, &args);
    } else {
        fklVMvaluePushState0ToStack(states);
        list = fklReadWithBuiltinParser(fp, &args);
    }

    *unexpect_eof = args.unexpect_eof;

    fklZfree(list);

    if (*unexpect_eof) {
        args.output = NULL;
        *output_line = args.curline;
        return NULL;
    }

    *output_line = args.ast_line;
    info->curline = args.curline;

    return args.output;
}

#include <fakeLisp/grammer.h>

static int _codegen_load_get_next_expression(FklCgCtx *ctx,
        void *pcontext,
        FklPmatchRes *out) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;
    CgLoadCtx *context = pcontext;
    FklVMvalueCgInfo *info = context->info;
    FklParseStateVector *states = &context->stateStack;
    FklAnalysisSymbolVector *symbols = &context->symbolStack;
    FILE *fp = context->fp;
    int unexpectEOF = 0;
    size_t output_line = 0;
    FklVMvalue *begin = getExpressionFromFile(ctx,
            info,
            fp,
            &unexpectEOF,
            &output_line,
            symbols,
            states);
    if (unexpectEOF) {
        if (error_state->error == NULL)
            error_state->error = make_parse_error(vm, unexpectEOF);
        error_state->line = output_line;
        return 0;
    }
    if (begin == NULL)
        return 0;

    FklVMvalue *contianer = fklCreateVMvalueBox(vm, begin);
    put_line_number(info->lnt, contianer, output_line);
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    const FklPmatchRes *orig = args->orig;

    const FklPmatchRes *filename =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *rest = fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
    if (!FKL_IS_STR(filename->value)) {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }

    if (rest->value != FKL_VM_NIL) {
        CgExpQueue *queue = cgExpQueueCreate();

        FklVMvalue *prev = FKL_VM_CDR(orig->value);
        FklVMvalue *head = FKL_VM_CAR(orig->value);

        for (FklVMvalue *rv = rest->value; FKL_IS_PAIR(rv);
                rv = FKL_VM_CDR(rv)) {
            FklVMvalue *new_rest = fklCreateVMvaluePair(vm, head, rv);
            put_line_number(info->lnt, new_rest, CURLINE(rv));
            cgExpQueuePush2(queue,
                    (FklPmatchRes){
                        .value = new_rest,
                        .container = new_rest,
                    });
            FKL_VM_CDR(prev) = FKL_VM_NIL;
            prev = rv;
        }
        MAKE_AND_PUSH_CG_ACT(_begin_exp_bc_process,
                createStackCtx(),
                createDefaultQueueNextExpression(queue),
                scope,
                macro_scope,
                env,
                CURLINE(orig->container),
                info,
                actions);
    }

    const FklString *filename_str = FKL_VM_STR(filename->value);
    if (!fklIsAccessibleRegFile(filename_str->str)) {
        error_state->error = make_file_failure_error(vm, filename->value);
        error_state->line = CURLINE(filename->container);
        return;
    }

    FklVMvalueCgInfo *next_info = fklCreateVMvalueCgInfo(ctx,
            info,
            filename_str->str,
            &(FklCgInfoArgs){ .inherit_grammer = 1 });

    if (hasLoadSameFile(next_info->realpath, info)) {
        error_state->error = make_circular_load_error(vm, filename->value);
        error_state->line = CURLINE(filename->container);
        return;
    }

    FILE *fp = fopen(filename_str->str, "r");
    if (!fp) {
        error_state->error = make_file_failure_error(vm, filename->value);
        error_state->line = CURLINE(filename->container);
        return;
    }
    MAKE_AND_PUSH_CG_ACT(_begin_exp_bc_process,
            createStackCtx(),
            createFpNextExpression(fp, next_info),
            scope,
            macro_scope,
            env,
            CURLINE(orig->container),
            next_info,
            actions);
}

static inline void export_compiler_macro(const FklCgCtx *c,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalue *head,
        FklVMvalueCgMacro *macro,
        FklVMvalueCgInfo *lib_info) {
    add_compiler_macro(c, macro_scope->macros, head, macro);

    if (lib_info == NULL)
        return;

    add_compiler_macro(c, lib_info->export_macros, head, macro);
}

static inline void import_replacement(FklVMvalueCgMacroScope *macro_scope,
        FklVMvalue *k,
        FklVMvalueCgRpl *rpl,
        FklVMvalueCgInfo *lib_info) {
    fklCgRplHashMapSet(macro_scope->replacements, k, rpl);
    if (lib_info) {
        fklCgRplHashMapSet(lib_info->export_replacement, k, rpl);
    }
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

static inline void export_symbol(FklVM *exe,
        FklVMvalue *k,
        const FklCgExportIdx *v,
        FklVMvalueCgEnv *env,
        FklVMvalue *bcl,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope,
        FklVMvalueCgInfo *lib_info) {
    uint32_t target_idx = fklAddCgDefBySid(k, scope, env)->idx;
    append_import_ins(exe, INS_APPEND_BACK, bcl, v->idx, fid, line, scope);
    append_put_loc_ins(exe, INS_APPEND_BACK, bcl, target_idx, fid, line, scope);
    if (lib_info == NULL)
        goto done;

    FklCgExportIdx *item = NULL;
    item = fklCgExportSidIdxHashMapGet2(&lib_info->exports, k);
    if (item == NULL) {
        uint32_t idx = lib_info->exports.count;
        item = fklCgExportSidIdxHashMapAdd(&lib_info->exports,
                &k,
                &(FklCgExportIdx){
                    .idx = idx,
                    .oidx = target_idx,
                });
    }

    append_export_ins(exe, INS_APPEND_BACK, bcl, item->idx, fid, line, scope);

done:
    append_drop_one_ins(exe, INS_APPEND_BACK, bcl, fid, line, scope);
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

static inline void import_replacements_with_prefix(FklCgCtx *ctx,
        FklVMvalueCgMacroScope *macros,
        const FklVMvalueCgRplHashMap *from,
        const FklString *prefix,
        FklVMvalueCgInfo *lib_info) {
    for (FklValueHashMapNode *cur = from->ht.first; cur; cur = cur->next) {
        const FklString *origSymbol = FKL_VM_SYM(cur->k);
        FklString *symbolWithPrefix = fklStringAppend(prefix, origSymbol);
        FklVMvalue *id = add_symbol(ctx, symbolWithPrefix);
        fklZfree(symbolWithPrefix);

        import_replacement(macros, id, fklVMvalueCgRpl(cur->v), lib_info);
    }
}

static inline void import_replacements(FklVMvalueCgMacroScope *macros,
        const FklVMvalueCgRplHashMap *from,
        FklVMvalueCgInfo *lib_info) {
    for (FklValueHashMapNode *cur = from->ht.first; cur; cur = cur->next) {
        import_replacement(macros, cur->k, fklVMvalueCgRpl(cur->v), lib_info);
    }
}

typedef FklVMvalue *(*ImportLibCb)(FklCgCtx *ctx,
        FklVMvalue *load_lib,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *args,
        FklVMvalueCgInfo *lib_info);

typedef struct {
    FklVMvalueCgInfo *info;
    FklVMvalueCgEnv *env;
    FklVMvalue *rp;
    FklVMvalueCgMacroScope *cms;
    FklVMvalueCgInfo *lib_info;

    FklVMvalue *mod_name;
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
    d->mod_name = NULL;
}

static void export_context_data_atomic(FklVMgc *gc, void *ctx) {
    ExportContextData *d = ctx;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->cms), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->env), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->info), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->lib_info), gc);

    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->mod_name), gc);
    fklVMgcToGray(d->import_cb_args, gc);
}

static const FklCgActCtxMt ExportContextMethodTable = {
    .size = sizeof(ExportContextData),
    .finalize = export_context_data_finalizer,
    .atomic = export_context_data_atomic,
};

static inline int merge_all_grammer(FklCgCtx *ctx, FklVMvalueCgInfo *info) {
    FKL_ASSERT(info->rmacros != NULL);
    FKL_ASSERT(info->g != NULL);

    // 尽量降低清空 grammer 的情况
    FklGrammer *g = &info->g->g;
    fklClearGrammer(g);
    if (fklMergeGrammer(g, &ctx->builtin_g))
        FKL_UNREACHABLE();

    for (FklValueHashMapNode *cur = info->rmacros->ht.first; cur;
            cur = cur->next) {
        FklVMvalueCgRmacro *rmacro = fklVMvalueCgRmacro(cur->v);

        if (fklExecuteCgRmacro(ctx, info, cur->k, rmacro)) {
            return 1;
        }
    }

    return 0;
}

// 新添加的 reader-macro 会储存在参数 new_rmacros 中
// 如果 need_rebuild_all 为真，则忽略 new_rmacros
// 我管你是新来的还是旧的，之间把原 grammer 清空，重新添加 reader-macro
static inline int update_grammer_impl(FklCgCtx *ctx,
        int need_rebuild_all,
        uint64_t line,
        FklVMvalueCgInfo *info,
        const FklPairVector *new_rmacros) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *errors = ctx->error_state;
    if (info->g == NULL)
        return 0;

    int err = 0;
    if (need_rebuild_all) {
        err = merge_all_grammer(ctx, info);
    } else {
        FKL_ASSERT(info->g != NULL);
        for (size_t i = 0; i < new_rmacros->size; ++i) {
            FklVMvalue *name = new_rmacros->base[i].car;
            FklVMvalue *rmacro_v = new_rmacros->base[i].cdr;
            FklVMvalueCgRmacro *rmacro = fklVMvalueCgRmacro(rmacro_v);
            int err = fklExecuteCgRmacro(ctx, info, name, rmacro);
            if (err)
                break;
        }
    }

    if (err != 0)
        return err;

    FklGrammer *const g = &info->g->g;

    FklGrammerNonterm nonterm = { 0 };
    if (fklCheckAndInitGrammerSymbols(g, &nonterm)) {
        FKL_ASSERT(nonterm);
        FklVMvalue *place = nonterm;

        errors->error = make_grammer_create_error2(vm, //
                "Undefined non-terminal",
                place);
        errors->fid = info->fid;
        errors->line = line;
        return 1;
    }

    FklLalrItemSetHashMap *itemSet = fklGenerateLr0Items(g);
    fklLr0ToLalrItems(itemSet, g);
    FklStrBuf err_msg;
    fklInitStrBuf(&err_msg);

    int r = fklGenerateLalrAnalyzeTable(vm, g, itemSet, &err_msg);
    if (r) {
        errors->error = make_import_reader_macro_error(vm, err_msg.buf, NULL);
        errors->line = line;
    }

    fklUninitStrBuf(&err_msg);
    fklLalrItemSetHashMapDestroy(itemSet);

    return r;
}

static inline FklVMvalueCgInfo *get_reader_start_info(
        FklVMvalueCgInfo *lib_info) {
    for (; lib_info; lib_info = lib_info->prev)
        if (lib_info->is_lib || lib_info->is_macro)
            return lib_info;
    return lib_info;
}

static inline void init_builtin_grammer(FklCgCtx *ctx, FklVMvalueCgInfo *info) {
    if (info->g == NULL) {
        FklVMvalueCgInfo *reader_start_info = get_reader_start_info(info);
        FKL_ASSERT(reader_start_info != NULL);
        FklVMvalueCgGrammer *gg = fklCreateVMvalueCgGrammer(ctx);
        FklVMvalueCgRmacroHashMap *map = fklCreateVMvalueCgRmacroHashMap(ctx);

        reader_start_info->g = gg;
        reader_start_info->rmacros = map;

        for (; info != reader_start_info; info = info->prev) {
            info->g = gg;
            info->rmacros = map;
        }

        FklGrammer *g = &info->g->g;
        if (fklMergeGrammer(g, &ctx->builtin_g))
            FKL_UNREACHABLE();
    }
}

FKL_NODISCARD
static inline int do_add_rmacro(FklCgCtx *ctx,
        FklVMvalueCgRmacroHashMap *map,
        FklVMvalueCgRmacro *rmacro,
        FklVMvalue *new_id) {
    FklVMvalueCgRmacro *old = fklCgRmacroHashMapDel(map, new_id);
    FklValueHashMapElm *e = NULL;
    e = fklCgRmacroHashMapRef1(map, new_id);
    e->v = FKL_VM_VAL(rmacro);

    return old != NULL;
}

FKL_NODISCARD
static inline int import_reader_macro(FklCgCtx *ctx,
        FklVMvalueCgInfo *info,
        FklVMvalueCgRmacro *item,
        FklVMvalue *new_id,
        FklVMvalueCgInfo *lib_info) {
    init_builtin_grammer(ctx, info);

    FklVMvalueCgRmacroHashMap *map = info->rmacros;
    int need_rebuild_all = do_add_rmacro(ctx, map, item, new_id);

    if (lib_info == NULL)
        return need_rebuild_all;

    FklVMvalueCgRmacroHashMap *export_map = lib_info->export_rmacros;
    int tmp = do_add_rmacro(ctx, export_map, item, new_id);
    (void)tmp;

    return need_rebuild_all;
}

FKL_NODISCARD
static inline int import_reader_macros(FklCgCtx *ctx,
        FklVMvalueCgMacroScope *macros,
        const FklVMvalueCgRmacroHashMap *from,
        FklVMvalueCgInfo *info,
        FklVMvalueCgInfo *lib_info,
        FklPairVector *new_rmacros) {
    int need_rebuild_all = 0;
    for (FklValueHashMapNode *cur = from->ht.first; cur; cur = cur->next) {
        need_rebuild_all |= import_reader_macro(ctx,
                info,
                fklVMvalueCgRmacro(cur->v),
                cur->k,
                lib_info);

        FklPair pair = {
            .car = cur->k,
            .cdr = cur->v,
        };

        fklPairVectorPushBack(new_rmacros, &pair);
    }

    return need_rebuild_all;
}

FKL_NODISCARD
static inline int import_reader_macros_with_prefix(FklCgCtx *ctx,
        FklVMvalueCgMacroScope *macros,
        const FklVMvalueCgRmacroHashMap *from,
        const FklString *prefix,
        FklVMvalueCgInfo *info,
        FklVMvalueCgInfo *lib_info,
        FklPairVector *new_rmacros) {
    int need_rebuild_all = 0;
    FklStrBuf buffer;
    fklInitStrBuf(&buffer);
    for (FklValueHashMapNode *cur = from->ht.first; cur; cur = cur->next) {
        fklStrBufConcatWithString(&buffer, prefix);
        fklStrBufConcatWithString(&buffer, FKL_VM_SYM(cur->k));

        FklVMvalue *new_id = add_symbol_char_buf(ctx, buffer.buf, buffer.index);

        fklStrBufClear(&buffer);

        need_rebuild_all |= import_reader_macro(ctx,
                info,
                fklVMvalueCgRmacro(cur->v),
                new_id,
                lib_info);
        FklPair pair = {
            .car = new_id,
            .cdr = cur->v,
        };

        fklPairVectorPushBack(new_rmacros, &pair);
    }
    fklUninitStrBuf(&buffer);

    return need_rebuild_all;
}

static inline FklVMvalue *
append_symbol_prefix(FklCgCtx *ctx, const FklString *prefix, FklVMvalue *sym) {
    const FklString *head = FKL_VM_SYM(sym);
    FklString *name_with_prefix = fklStringAppend(prefix, head);
    FklVMvalue *r = add_symbol(ctx, name_with_prefix);
    fklZfree(name_with_prefix);
    return r;
}

static inline void import_macro_list(const FklCgCtx *c,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalue *cur_pair,
        FklVMvalue *head,
        FklVMvalueCgInfo *lib_info) {
    for (; FKL_IS_PAIR(cur_pair); cur_pair = FKL_VM_CDR(cur_pair)) {
        FklVMvalueCgMacro *macro = fklVMvalueCgMacro(FKL_VM_CAR(cur_pair));
        export_compiler_macro(c, macro_scope, head, macro, lib_info);
    }
}

static inline FklVMvalue *process_import_imported_lib_common(FklCgCtx *ctx,
        FklVMvalue *load_lib,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *args,
        FklVMvalueCgInfo *lib_info) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *errors = ctx->error_state;
    const FklVMvalueCgMacroHashMap *macros = lib->macros;
    const FklVMvalueCgRplHashMap *replacements = lib->replacements;
    const FklVMvalueCgRmacroHashMap *reader_macros = lib->rmacros;

    FKL_ASSERT(replacements);
    FKL_ASSERT(reader_macros);

    for (const FklValueHashMapNode *cur = macros->ht.first; cur;
            cur = cur->next) {
        FklVMvalue *head = cur->k;
        import_macro_list(ctx, macro_scope, cur->v, head, lib_info);
    }

    import_replacements(macro_scope, replacements, lib_info);

    FklPairVector new_rmacros = { 0 };
    fklPairVectorInit(&new_rmacros, reader_macros->ht.count + 1);

    int need_rebuild_all = import_reader_macros(ctx,
            macro_scope,
            reader_macros,
            info,
            lib_info,
            &new_rmacros);

    if (errors->error != NULL) {
        fklPairVectorUninit(&new_rmacros);
        return NULL;
    }

    int rmacro_err = update_grammer_impl(ctx,
            need_rebuild_all,
            curline,
            info,
            &new_rmacros);

    fklPairVectorUninit(&new_rmacros);

    if (rmacro_err != 0) {
        errors->line = curline;
        return NULL;
    }

    add_symbol_to_local_env_in_array(vm,
            env,
            &lib->exports,
            load_lib,
            info->fid,
            curline,
            scope,
            lib_info);
    return load_lib;
}

static inline FklVMvalue *process_import_imported_lib_prefix(FklCgCtx *ctx,
        FklVMvalue *load_lib,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *prefix_v,
        FklVMvalueCgInfo *lib_info) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;
    const FklVMvalueCgMacroHashMap *macros = lib->macros;
    const FklVMvalueCgRplHashMap *replacements = lib->replacements;
    const FklVMvalueCgRmacroHashMap *reader_macros = lib->rmacros;

    FKL_ASSERT(replacements);
    FKL_ASSERT(reader_macros);

    const FklString *prefix = FKL_VM_SYM(prefix_v);
    for (const FklValueHashMapNode *cur = macros->ht.first; cur;
            cur = cur->next) {
        FklVMvalue *head = cur->k;
        head = append_symbol_prefix(ctx, prefix, head);
        import_macro_list(ctx, macro_scope, cur->v, head, lib_info);
    }

    import_replacements_with_prefix(ctx,
            macro_scope,
            replacements,
            prefix,
            lib_info);

    FklPairVector new_rmacros = { 0 };
    fklPairVectorInit(&new_rmacros, reader_macros->ht.count + 1);

    int need_rebuild_all = import_reader_macros_with_prefix(ctx,
            macro_scope,
            reader_macros,
            prefix,
            info,
            lib_info,
            &new_rmacros);

    if (error_state->error) {
        fklPairVectorUninit(&new_rmacros);
        return NULL;
    }

    int rmacro_err = update_grammer_impl(ctx,
            need_rebuild_all,
            curline,
            info,
            &new_rmacros);
    fklPairVectorUninit(&new_rmacros);

    if (rmacro_err) {
        error_state->line = curline;
        return NULL;
    }

    add_symbol_with_prefix_to_local_env_in_array(vm,
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

static inline FklVMvalue *process_import_imported_lib_only(FklCgCtx *ctx,
        FklVMvalue *load_lib,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *only,
        FklVMvalueCgInfo *lib_info) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;

    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    const FklVMvalueCgMacroHashMap *macros = lib->macros;
    const FklVMvalueCgRplHashMap *replacements = lib->replacements;
    const FklVMvalueCgRmacroHashMap *reader_macros = lib->rmacros;

    FKL_ASSERT(macros);
    FKL_ASSERT(replacements);
    FKL_ASSERT(reader_macros);

    int need_rebuild_all = 0;

    FklPairVector new_rmacros = { 0 };
    fklPairVectorInit(&new_rmacros, reader_macros->ht.count + 1);

    for (; FKL_IS_PAIR(only); only = FKL_VM_CDR(only)) {
        FklVMvalue *head = FKL_VM_CAR(only);
        int has_entity = 0;

        FklValueHashMapElm *pmacro = fklCgMacroHashMapGet(macros, head);
        if (pmacro != NULL) {
            has_entity = 1;
            FKL_ASSERT(pmacro->k == head);
            import_macro_list(ctx, macro_scope, pmacro->v, head, lib_info);
        }

        FklVMvalueCgRpl *rep = fklCgRplHashMapGet(replacements, head);
        if (rep) {
            has_entity = 1;
            import_replacement(macro_scope, head, rep, lib_info);
        }

        FklValueHashMapElm *e = fklCgRmacroHashMapGet(reader_macros, head);

        if (e != NULL) {
            has_entity = 1;
            need_rebuild_all |= import_reader_macro(ctx,
                    info,
                    fklVMvalueCgRmacro(e->v),
                    head,
                    lib_info);
            FklPair pair = {
                .car = head,
                .cdr = e->v,
            };

            fklPairVectorPushBack(&new_rmacros, &pair);
        }

        FklCgExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, head);
        if (item) {
            export_symbol(vm,
                    head,
                    item,
                    env,
                    load_lib,
                    info->fid,
                    CURLINE(only),
                    scope,
                    lib_info);
        } else if (!has_entity) {
            error_state->error = make_import_missing_error(vm, head);
            error_state->line = CURLINE(only);
            error_state->fid = add_symbol_cstr(ctx, info->filename);
            break;
        }
    }

    if (error_state->error) {
        fklPairVectorUninit(&new_rmacros);
        return NULL;
    }

    int rmacro_err = update_grammer_impl(ctx,
            need_rebuild_all,
            curline,
            info,
            &new_rmacros);
    fklPairVectorUninit(&new_rmacros);

    if (rmacro_err) {
        error_state->line = curline;
    }

    return load_lib;
}

static inline FklVMvalue *process_import_imported_lib_except(FklCgCtx *ctx,
        FklVMvalue *load_lib,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *except,
        FklVMvalueCgInfo *lib_info) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;
    const FklVMvalueCgMacroHashMap *macros = lib->macros;
    const FklVMvalueCgRplHashMap *replacements = lib->replacements;
    const FklVMvalueCgRmacroHashMap *reader_macros = lib->rmacros;

    FKL_ASSERT(replacements);
    FKL_ASSERT(reader_macros);

    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    FklValueHashSet excepts;
    fklValueHashSetInit(&excepts);

    for (FklVMvalue *list = except; FKL_IS_PAIR(list); list = FKL_VM_CDR(list))
        fklValueHashSetPut2(&excepts, FKL_VM_CAR(list));

    for (const FklValueHashMapNode *cur = macros->ht.first; cur;
            cur = cur->next) {
        if (fklValueHashSetHas2(&excepts, cur->k))
            continue;
        import_macro_list(ctx, macro_scope, cur->v, cur->k, lib_info);
    }

    for (const FklValueHashMapNode *cur = replacements->ht.first; cur;
            cur = cur->next) {
        if (fklValueHashSetHas2(&excepts, cur->k))
            continue;
        FklVMvalueCgRpl *rpl = fklVMvalueCgRpl(cur->v);
        import_replacement(macro_scope, cur->k, rpl, lib_info);
    }

    FklPairVector new_rmacros = { 0 };
    fklPairVectorInit(&new_rmacros, reader_macros->ht.count + 1);

    int need_rebuild_all = 0;
    for (const FklValueHashMapNode *cur = reader_macros->ht.first; cur;
            cur = cur->next) {
        if (!fklValueHashSetHas2(&excepts, cur->k)) {
            need_rebuild_all |= import_reader_macro(ctx,
                    info,
                    fklVMvalueCgRmacro(cur->v),
                    cur->k,
                    lib_info);
            FklPair pair = {
                .car = cur->k,
                .cdr = cur->v,
            };

            fklPairVectorPushBack(&new_rmacros, &pair);
        }
    }

    if (error_state->error) {
        fklPairVectorUninit(&new_rmacros);
        load_lib = NULL;
        goto exit;
    }

    int rmacro_err = update_grammer_impl(ctx,
            need_rebuild_all,
            curline,
            info,
            &new_rmacros);

    fklPairVectorUninit(&new_rmacros);

    if (rmacro_err) {
        error_state->line = curline;
        load_lib = NULL;
        goto exit;
    }

    for (const FklCgExportSidIdxHashMapNode *l = exports->first; l;
            l = l->next) {
        if (!fklValueHashSetHas2(&excepts, l->k)) {
            export_symbol(vm,
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

static inline FklVMvalue *process_import_imported_lib_alias(FklCgCtx *ctx,
        FklVMvalue *load_lib,
        FklVMvalueCgInfo *info,
        const FklCgLib *lib,
        FklVMvalueCgEnv *env,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        uint64_t curline,
        FklVMvalue *alias,
        FklVMvalueCgInfo *lib_info) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *error_state = ctx->error_state;

    const FklCgExportSidIdxHashMap *exports = &lib->exports;

    const FklVMvalueCgMacroHashMap *macros = lib->macros;
    const FklVMvalueCgRplHashMap *replacements = lib->replacements;
    const FklVMvalueCgRmacroHashMap *reader_macros = lib->rmacros;

    FKL_ASSERT(replacements);
    FKL_ASSERT(reader_macros);

    int need_rebuild_all = 0;
    FklPairVector new_rmacros = { 0 };
    fklPairVectorInit(&new_rmacros, reader_macros->ht.count + 1);

    for (; FKL_IS_PAIR(alias); alias = FKL_VM_CDR(alias)) {
        FklVMvalue *cur_alias_list = FKL_VM_CAR(alias);
        FklVMvalue *cur_head = FKL_VM_CAR(cur_alias_list);
        FklVMvalue *cur_alias = FKL_VM_CAR(FKL_VM_CDR(cur_alias_list));
        int has_entity = 0;

        FklValueHashMapElm *pmacro = fklCgMacroHashMapGet(macros, cur_head);
        if (pmacro != NULL) {
            FklVMvalue *list = pmacro->v;
            has_entity = 1;
            import_macro_list(ctx, macro_scope, list, cur_alias, lib_info);
        }

        FklVMvalueCgRpl *rpl = fklCgRplHashMapGet(replacements, cur_head);
        if (rpl) {
            has_entity = 1;
            import_replacement(macro_scope, cur_alias, rpl, lib_info);
        }

        FklValueHashMapElm *e = NULL;
        e = fklCgRmacroHashMapGet(reader_macros, cur_head);

        if (e != NULL) {
            has_entity = 1;
            need_rebuild_all |= import_reader_macro(ctx,
                    info,
                    fklVMvalueCgRmacro(e->v),
                    cur_alias,
                    lib_info);
            FklPair pair = {
                .car = cur_alias,
                .cdr = e->v,
            };
            fklPairVectorPushBack(&new_rmacros, &pair);
        }

        FklCgExportIdx *item = fklCgExportSidIdxHashMapGet2(exports, cur_head);
        if (item) {
            export_symbol(vm,
                    cur_alias,
                    item,
                    env,
                    load_lib,
                    info->fid,
                    CURLINE(alias),
                    scope,
                    lib_info);
        } else if (!has_entity) {
            error_state->error = make_import_missing_error(vm, cur_head);
            error_state->line = CURLINE(alias);
            error_state->fid = add_symbol_cstr(ctx, info->filename);
            break;
        }
    }

    if (error_state->error) {
        fklPairVectorUninit(&new_rmacros);
        return NULL;
    }

    int rmacro_err = update_grammer_impl(ctx,
            need_rebuild_all,
            curline,
            info,
            &new_rmacros);

    fklPairVectorUninit(&new_rmacros);

    if (rmacro_err) {
        error_state->line = curline;
    }
    return load_lib;
}

static FklVMvalue *load_lib_cb(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FKL_ASSERT(bcl_vec->size == 1);
    FklCgLib *lib = fklVMvalueCgLib(bcl_vec->base[0]);

    ExportContextData *d = FKL_TYPE_CAST(ExportContextData *, data);

    const char *rp = FKL_VM_SYM(d->rp)->str;
    const FklLibId *lib_id = fklVMvalueCgEnvAddUsedLib(d->env, rp, lib->lib);

    FklVMvalue *load_lib = append_load_lib_ins(vm,
            INS_APPEND_BACK,
            NULL,
            lib_id->id,
            fid,
            line,
            d->scope);

    return d->import_cb(ctx,
            load_lib,
            d->info,
            lib,
            d->env,
            d->scope,
            d->cms,
            line,
            d->import_cb_args,
            d->lib_info);
}

static FklCgActCtx *createExportContext(FklVMvalueCgInfo *info,
        FklVMvalueCgEnv *targetEnv,
        FklVMvalue *rp,
        uint32_t scope,
        FklVMvalueCgMacroScope *cms,
        FklVMvalue *mod_name,
        ImportLibCb import_cb,
        FklVMvalue *import_cb_args,
        FklVMvalueCgInfo *lib_info) {
    FklCgActCtx *r = createCgActCtx(&ExportContextMethodTable);
    ExportContextData *data = FKL_TYPE_CAST(ExportContextData *, r->d);

    data->info = info;
    data->rp = rp;

    data->scope = scope;
    data->env = targetEnv;

    data->cms = cms;

    data->import_cb = import_cb;
    data->import_cb_args = import_cb_args;
    data->mod_name = mod_name;

    data->lib_info = lib_info;
    return r;
}

static inline FklVMvalue *make_export_sequnce(FklVM *exe,
        FklValueVector *stack,
        FklVMvalue *fid,
        uint32_t line,
        uint32_t scope) {
    if (stack->size) {
        FklIns drop = FKL_MAKE_INS_I(FKL_OP_DROP);
        FklVMvalue *retval = create_0len_bcl(exe);
        size_t top = stack->size;
        FklByteCodelnt *co = FKL_VM_CO(retval);
        for (size_t i = 0; i < top; i++) {
            FklVMvalue *cur = stack->base[i];
            if (FKL_VM_CO(cur)->bc.len) {
                fklCodeLntConcat(co, FKL_VM_CO(cur));
                if (i < top - 1) {
                    fklByteCodeLntPushBackIns(co, drop, fid, line, scope);
                }
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
} ExportSeqCtxData;

static FklVMvalue *exports_bc_process(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgInfo *info = args->info;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;
    FklCgErrorState *error_state = ctx->error_state;

    ExportSeqCtxData *d = FKL_TYPE_CAST(ExportSeqCtxData *, data);
    FklVMvalue *bcl = make_export_sequnce(vm, bcl_vec, fid, line, scope);
    if (d->must_has_retval && !bcl) {
        error_state->error = make_has_no_value_error(vm, d->orig.value);
        error_state->line = CURLINE(d->orig.container);
    }
    return bcl;
}

static void codegen_export_none(const CgCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    if (must_has_retval) {
        error_state->error = make_has_no_value_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
    FklVMvalueCgInfo *lib_info = get_lib_info(info);

    if (lib_info && scope == 1 && env->prev == info->global_env
            && macro_scope->prev == info->global_env->macros) {
        MAKE_AND_PUSH_CG_ACT(_empty_bc_process,
                createStackCtx(),
                NULL,
                scope,
                macro_scope,
                env,
                CURLINE(orig->container),
                info,
                actions);
    } else {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
}

static void export_sequnce_context_data_atomic(FklVMgc *gc, void *data) {
    ExportSeqCtxData *d = (ExportSeqCtxData *)data;
    fklVMgcToGray(d->orig.value, gc);
    fklVMgcToGray(d->orig.container, gc);
}

static const FklCgActCtxMt ExportSequnceContextMethodTable = {
    .size = sizeof(ExportSeqCtxData),
    .atomic = export_sequnce_context_data_atomic,
};

static FklCgActCtx *create_export_sequnce_context(const FklPmatchRes *orig,
        uint8_t must_has_retval) {
    FklCgActCtx *r = createCgActCtx(&ExportSequnceContextMethodTable);
    ExportSeqCtxData *data = FKL_TYPE_CAST(ExportSeqCtxData *, r->d);

    data->must_has_retval = must_has_retval;
    data->orig = *orig;
    return r;
}

static void codegen_export(const CgCbArgs *args) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *error_state = ctx->error_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    FklVMvalueCgInfo *lib_info = get_lib_info(info);

    if (lib_info && scope == 1 && env->prev == info->global_env
            && macro_scope->prev == info->global_env->macros) {
        CgExpQueue *exportQueue = cgExpQueueCreate();
        const FklPmatchRes *rest =
                fklPmatchHashMapGet2(ht, ctx->builtin_sym_rest);
        add_export_symbol(ctx, lib_info, orig->value, rest->value, exportQueue);

        MAKE_AND_PUSH_CG_ACT(exports_bc_process,
                create_export_sequnce_context(orig, must_has_retval),
                createDefaultQueueNextExpression(exportQueue),
                scope,
                macro_scope,
                env,
                CURLINE(orig->container),
                info,
                actions);
    } else {
        error_state->error = make_syntax_error(vm, orig->value);
        error_state->line = CURLINE(orig->container);
        return;
    }
}

typedef struct {
    FklVMvalue *id;
    FklCgExportIdx *item;
} ExportDefineContext;

static const FklCgActCtxMt ExportDefineContextMethodTable = {
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
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FKL_ASSERT(bcl_vec->size == 1);

    ExportDefineContext *exp_ctx = FKL_TYPE_CAST(ExportDefineContext *, data);
    exp_ctx->item->oidx = fklGetCgDefByIdInScope(exp_ctx->id, 1, env)->v.idx;
    FklVMvalue *r = bcl_vec->base[0];

    r = append_export_ins(vm,
            INS_APPEND_BACK,
            r,
            exp_ctx->item->idx,
            fid,
            line,
            scope);
    return r;
}

typedef int (*ImportLibCbCheck)(const FklVMvalue *args);

typedef struct {
    FklVMvalue *name;
    FklVMvalue *rest;

    ImportLibCbCheck import_check_cb;
    ImportLibCb import_cb;
    FklVMvalue *import_cb_args;
} CgImportHelperArgs;

static inline void import_lib_impl(const CgCbArgs *args,
        const CgImportHelperArgs *import_args,
        const char *filename,
        FklVMvalueCgInfo *lib_info,
        FklFileType ft) {
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    FklVMvalue *orig = args->orig->value;
    FklVMvalueCgInfo *info = args->info;
    FklCgErrorState *errors = ctx->error_state;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklCgActVector *actions = args->actions;

    FklVMvalue *name = import_args->name;

    FklVMvalue *rp_v = NULL;
    {
        char *rp = fklRealpath(filename);
        rp_v = fklVMaddSymbolCstr(vm, rp);
        fklZfree(rp);
    };

    const char *rp = FKL_VM_SYM(rp_v)->str;
    FklVMvalue *module_name = fklCgRealpathToModuleName(ctx, rp);

    if (hasLoadSameFile(rp, info)) {
        errors->error = make_circular_load_error(vm, name);
        errors->line = CURLINE(orig);
        return;
    }

    FklCgAct *load_lib_act = make_cg_act(load_lib_cb,
            createExportContext(info,
                    env,
                    rp_v,
                    scope,
                    macro_scope,
                    module_name,
                    import_args->import_cb,
                    import_args->import_cb_args,
                    lib_info),
            NULL,
            scope,
            macro_scope,
            NULL,
            CURLINE(orig),
            NULL,
            info);

    FklCgAct *import_act = fklMakeImportAct(ctx, //
            name,
            ft,
            rp_v,
            info,
            load_lib_act);

    fklCgActVectorPushBack2(actions, load_lib_act);
    fklCgActVectorPushBack2(actions, import_act);
    return;
}

static inline const FklCgLib *load_dll(FklCgCtx *ctx,
        FklVMvalue *name,
        FklVMvalueCgLibs *libs,
        FklVMvalue *rp_v,
        uint64_t cur_line) {
    FklVM *vm = ctx->vm;
    FklCgErrorState *errors = ctx->error_state;

    FklCgLib *lib = fklVMvalueCgLibsGet1(libs, rp_v);
    if (lib != NULL) {
        return lib;
    }

    const char *rp = FKL_VM_SYM(rp_v)->str;
    uv_lib_t dll = { 0 };
    if (uv_dlopen(rp, &dll)) {
        const char *msg = uv_dlerror(&dll);
        errors->error = make_file_failure_error2(vm, msg, name);
        errors->line = cur_line;
        uv_dlclose(&dll);
        return 0;
    }
    FklCgDllLibInitExportCb initExport = fklGetCgInitExportFunc(&dll);
    if (!initExport) {
        uv_dlclose(&dll);
        errors->error = make_import_failed_error(vm, name);
        errors->line = cur_line;
        return 0;
    }

    FklVMvalue *module_name = fklCgRealpathToModuleName(ctx, rp);
    lib = fklVMvalueCgLibsAdd1(vm, libs, rp_v);
    fklInitCgDllLib(ctx, module_name, lib, rp_v, dll, initExport);
    uv_dlclose(&dll);

    return lib;
}

FklCgDllLibInitExportCb fklGetCgInitExportFunc(uv_lib_t *dll) {
    return (FklCgDllLibInitExportCb)fklGetAddress("_fklExportSymbolInit", dll);
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

typedef struct {
    FklVMvalue *rp;
    FklVMvalue *name;
    FklFileType ft;

    FklVMvalueCgInfo *info;
} CheckImportedCtx;

static void check_imported_ctx_atomic(FklVMgc *gc, void *data) {
    CheckImportedCtx *d = (CheckImportedCtx *)data;
    fklVMgcToGray(d->rp, gc);
    fklVMgcToGray(d->name, gc);
    fklVMgcToGray(FKL_VM_VAL(d->info), gc);
}

static const FklCgActCtxMt CheckImportedCtxMt = {
    .size = sizeof(CheckImportedCtx),
    .atomic = check_imported_ctx_atomic,
};

static inline FklCgActCtx *make_import_act_ctx(FklVMvalue *name,
        FklFileType ft,
        FklVMvalue *rp,
        FklVMvalueCgInfo *info) {
    FklCgActCtx *r = createCgActCtx(&CheckImportedCtxMt);
    CheckImportedCtx *d = FKL_TYPE_CAST(CheckImportedCtx *, r->d);
    d->ft = ft;
    d->rp = rp;
    d->name = name,

    d->info = info;
    return r;
}

static FklVMvalue *lib_create_cb(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;
    FklVMvalueCgInfo *info = args->info;

    CheckImportedCtx *d = FKL_TYPE_CAST(CheckImportedCtx *, data);

    FklVMvalueProto *pt = fklCreateVMvalueProto2(ctx->vm, env);
    fklPrintUndefinedRef(env, &ctx->vm->gc->err_out);

    FklVMvalue *co = sequnce_exp_bc_process(vm, bcl_vec, fid, line, scope);

    FklIns ret = FKL_MAKE_INS_I(FKL_OP_RET);

    fklByteCodeLntPushBackIns(FKL_VM_CO(co), ret, fid, line, scope);

    if (!env->is_debugging)
        fklPeepholeOptimize(FKL_VM_CO(co));

    FklVMvalue *proc = fklCreateVMvalueProc(ctx->vm, co, pt);
    fklInitMainProcRefs(ctx->vm, proc);

    FklCgLib *lib = fklVMvalueCgLibsAdd1(vm, d->info->libraries, d->rp);
    FklVMvalue *name = fklCgRealpathToModuleName(ctx, FKL_VM_SYM(d->rp)->str);
    fklInitCgScriptLib(ctx, lib, name, info, proc);

    return FKL_VM_VAL(lib);
}

static inline FklCgAct *make_lib_create_act(const CheckImportedCtx *d,
        const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklCgErrorState *errors = ctx->error_state;
    FklVM *vm = ctx->vm;

    FklVMvalueCgInfo *info = d->info;
    FklVMvalue *rp = d->rp;
    FklFileType ft = d->ft;
    FklVMvalue *name = d->name;

    FILE *fp = fopen(FKL_VM_SYM(d->rp)->str, "r");
    if (!fp) {
        errors->error = make_file_failure_error(vm, name);
        errors->line = args->line;
        return NULL;
    }

    FklVMvalueCgInfo *next_info = fklCreateVMvalueCgInfo(ctx,
            info,
            FKL_VM_SYM(d->rp)->str,
            &(FklCgInfoArgs){
                .is_lib = 1,
            });

    FklVMvalueCgEnv *env = fklCreateVMvalueCgEnv(ctx,
            &(const FklCgEnvCreateArgs){
                .prev_env = info->global_env,
                .prev_ms = info->global_env->macros,
                .parent_scope = 1,
                .filename = next_info->fid,
                .name = FKL_VM_NIL,
                .line = 1,
            });

    FklCgActCtx *act_ctx = make_import_act_ctx(name, ft, rp, info);
    FklCgAct *act = make_cg_act(lib_create_cb,
            act_ctx,
            createFpNextExpression(fp, next_info),
            1,
            env->macros,
            env,
            args->line,
            args->prev,
            next_info);
    return act;
}

static FklVMvalue *fixup_done_cb(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;

    PairCtx *d = FKL_TYPE_CAST(PairCtx *, data);
    FklVMvaluePcFixup *fixup = fklVMvaluePcFixup(d->car);
    FklVMvalueVec *v = FKL_VM_VEC(d->cdr);

    FKL_ASSERT(v->size == 3);

    int r = fklPreCompileFixup(&fixup->f, ctx);
    FKL_ASSERT(r == 0);
    (void)r;

    FklVMvalue *rp = v->base[0];
    FklVMvalueCgLib *l = fklVMvalueCgLib(v->base[1]);
    FklVMvalueCgLibs *libs = fklVMvalueCgLibs(v->base[2]);

    fklVMvalueCgLibsAdd2(libs, rp, l);
    return FKL_VM_VAL(l);
}

static inline FklCgAct *make_fixup_done_act(FklCgCtx *ctx,
        FklVMvalueCgInfo *info,
        FklVMvaluePcFixup *fixup,
        FklVMvalueVec *vec,
        FklCgAct *prev) {
    FklCgAct *act = make_cg_act(fixup_done_cb,
            createPairCtx(FKL_VM_VAL(fixup), FKL_VM_VAL(vec)),
            NULL,
            1,
            info->global_env->macros,
            info->global_env,
            0,
            prev,
            info);
    return act;
}

static FklVMvalue *fixup_begin_cb(const FklCgActCbArgs *args) {
    void *data = args->data;
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;

    PairCtx *d = FKL_TYPE_CAST(PairCtx *, data);
    FklVMvaluePcFixup *fixup = fklVMvaluePcFixup(d->car);
    FklVMvalueVec *v = FKL_VM_VEC(d->cdr);

    FKL_ASSERT(v->size == 3);

    FklCgAct *last_act = make_fixup_done_act(ctx, //
            info,
            fixup,
            v,
            args->prev);

    fklCgActVectorPushBack2(ctx->action_vector, last_act);

    FklVMvalue *rp_s = v->base[0];
    const char *rp = FKL_VM_SYM(rp_s)->str;

    FklVMvalueCgInfo *info1 = fklCreateVMvalueCgInfo(ctx,
            info,
            rp,
            &(FklCgInfoArgs){
                .is_lib = 1,
            });

    FklVMvalueCgInfo *macro_info = fklCreateVMvalueCgInfo(ctx,
            info,
            rp,
            &(FklCgInfoArgs){
                .is_lib = 1,
                .is_macro = 1,
            });

    const FklPcDepVector *pendings = &fixup->f.pendings;
    for (size_t i = 0; i < pendings->size; ++i) {
        const FklPcDep *dep = &pendings->base[i];

        FklVMvalueCgInfo *info = dep->is_imported_by_macro ? macro_info : info1;

        FklVMvalue *rp = dep->rp;
        FklFileType ft = dep->ft;
        FklVMvalue *name = dep->name;

        FKL_ASSERT(ft != FKL_FILE_NONE);

        FklCgAct *a = fklMakeImportAct(ctx, name, ft, rp, info, last_act);
        fklCgActVectorPushBack2(ctx->action_vector, a);
    }

    return NULL;
}

static inline FklCgAct *make_fixup_begin_act(FklCgCtx *ctx,
        FklVMvalue *rp,
        FklVMvalueCgInfo *info,
        FklVMvaluePcFixup *fixup,
        FklCgLib *l,
        FklCgAct *prev) {
    FklVMvalueCgInfo *next_info = fklCreateVMvalueCgInfo(ctx,
            info,
            FKL_VM_SYM(rp)->str,
            &(FklCgInfoArgs){
                .is_lib = 1,
            });

    FklVMvalue *p = fklCreateVMvalueVecExt(ctx->vm,
            3,
            rp,
            FKL_VM_VAL(l),
            FKL_VM_VAL(info->libraries));

    FklCgAct *act = make_cg_act(fixup_begin_cb,
            createPairCtx(FKL_VM_VAL(fixup), p),
            NULL,
            1,
            next_info->global_env->macros,
            next_info->global_env,
            0,
            prev,
            next_info);
    return act;
}

static inline FklCgAct *make_fixup_act(const CheckImportedCtx *d,
        const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklCgErrorState *errors = ctx->error_state;
    FklVM *vm = ctx->vm;

    FklVMvalueCgInfo *info = d->info;
    FklVMvalue *rp_v = d->rp;
    FklVMvalue *name = d->name;

    const char *rp = FKL_VM_SYM(rp_v)->str;

    FILE *fp = fopen(FKL_VM_SYM(d->rp)->str, "rb");
    if (!fp) {
        errors->error = make_file_failure_error(vm, name);
        errors->line = args->line;
        return NULL;
    }

    FklVMvaluePcFixup *fixup = fklCreateVMvaluePcFixup(vm);

    FklLoadPreCompileArgs load_args = {
        .ctx = ctx,
        .libraries = info->libraries,
        .fixup = &fixup->f,
    };

    FklCgLib *lib = fklLoadPreCompile(fp, rp, &load_args);

    if (lib == NULL) {
        if (load_args.error_fmt) {
            errors->error = make_import_failed_error2(vm,
                    load_args.error_fmt,
                    load_args.error_obj);
            load_args.error_fmt = NULL;
        } else {
            errors->error = make_import_failed_error(vm, name);
        }

        errors->line = args->line;
        return NULL;
    }

    return make_fixup_begin_act(ctx, rp_v, info, fixup, lib, args->prev);
}

static FklVMvalue *check_imported_cb(const FklCgActCbArgs *args) {
    FklCgCtx *ctx = args->ctx;

    void *data = args->data;
    CheckImportedCtx *d = FKL_TYPE_CAST(CheckImportedCtx *, data);

    FklVMvalueCgInfo *info = d->info;
    FklVMvalue *rp = d->rp;

    FklVMvalue *name = d->name;
    const FklCgLib *lib = fklVMvalueCgLibsGet1(info->libraries, rp);
    if (lib == NULL) {
        switch (d->ft) {
        case FKL_FILE_PACKAGE:
        case FKL_FILE_SCRIPT: {
            FklCgAct *act = make_lib_create_act(d, args);
            if (act == NULL)
                break;

            fklCgActVectorPushBack2(ctx->action_vector, act);
        } break;

        case FKL_FILE_DLL:
            lib = load_dll(args->ctx, name, info->libraries, rp, args->line);
            break;

        case FKL_FILE_PRECOMPILE:
            FklCgAct *act = make_fixup_act(d, args);
            if (act == NULL)
                break;
            fklCgActVectorPushBack2(ctx->action_vector, act);
            break;

        case FKL_FILE_NONE:
            FKL_UNREACHABLE();
            break;
        }
    }

    return FKL_VM_VAL(lib);
}

FklCgAct *fklMakeImportAct(FklCgCtx *ctx,
        FklVMvalue *name,
        FklFileType ft,
        FklVMvalue *rp,
        FklVMvalueCgInfo *info,
        FklCgAct *prev) {
    FklCgActCtx *act_ctx = make_import_act_ctx(name, ft, rp, info);
    FklCgAct *r = make_cg_act(check_imported_cb,
            act_ctx,
            NULL,
            1,
            info->global_env->macros,
            info->global_env,
            0,
            prev,
            info);
    return r;
}

static FklVMvalue *collect_val_vec_cb(const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *r = fklCreateVMvalueVec2(vm, bcl_vec->size, bcl_vec->base);
    return r;
}

FklCgAct *
fklMakeCollectAct(FklCgCtx *ctx, FklVMvalueCgInfo *info, FklCgAct *prev) {
    FklCgAct *r = make_cg_act(collect_val_vec_cb,
            createStackCtx(),
            NULL,
            1,
            info->global_env->macros,
            info->global_env,
            0,
            prev,
            info);
    return r;
}

static inline void codegen_import_helper(const CgCbArgs *args,
        const CgImportHelperArgs *import_args,
        FklVMvalueCgInfo *lib_info) {
    FKL_ASSERT(import_args);

    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    FklVMvalue *orig = args->orig->value;
    FklVMvalueCgInfo *info = args->info;
    FklCgErrorState *errors = ctx->error_state;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklCgActVector *actions = args->actions;

    FklVMvalue *name = import_args->name;
    FklVMvalue *rest = import_args->rest;

    if (!FKL_IS_SYM(name)) {
        errors->error = make_syntax_error(vm, orig);
        errors->line = CURLINE(orig);
        return;
    }

    if (!is_module_path(FKL_VM_SYM(name))) {
        errors->error = make_syntax_error(vm, orig);
        errors->line = CURLINE(orig);
        return;
    }

    FklVMvalue *priv_sub_mod_name = get_priv_sub_mod_name(vm, FKL_VM_SYM(name));
    if (priv_sub_mod_name != NULL) {
        errors->error = make_import_failed_error2(vm,
                "import private module: %S",
                priv_sub_mod_name);
        errors->line = CURLINE(orig);
        return;
    }

    ImportLibCbCheck const check_cb = import_args->import_check_cb;
    if (check_cb != NULL && !check_cb(import_args->import_cb_args)) {
        errors->error = make_syntax_error(vm, import_args->import_cb_args);
        errors->line = CURLINE(orig);
        return;
    }

    if (rest != FKL_VM_NIL) {
        CgExpQueue *queue = cgExpQueueCreate();

        FklVMvalue *prev = FKL_VM_CDR(orig);

        FklVMvalue *head = FKL_VM_CAR(orig);

        FklVMvalue *export_head = add_symbol_cstr(ctx, "export");
        for (; FKL_IS_PAIR(rest); rest = FKL_VM_CDR(rest)) {
            FklVMvalue *new_rest = fklCreateVMvaluePair(vm, head, rest);
            put_line_number(info->lnt, new_rest, CURLINE(rest));
            if (lib_info) {
                ListElm a[2] = {
                    { .v = export_head, .line = CURLINE(rest) },
                    { .v = new_rest, .line = CURLINE(rest) },
                };

                new_rest = create_list(a, 2, CURLINE(rest), vm, info->lnt);
            }
            cgExpQueuePush2(queue,
                    (FklPmatchRes){
                        .value = new_rest,
                        .container = new_rest,
                    });
            FKL_VM_CDR(prev) = FKL_VM_NIL;
            prev = rest;
        }

        MAKE_AND_PUSH_CG_ACT(_begin_exp_bc_process,
                createStackCtx(),
                createDefaultQueueNextExpression(queue),
                scope,
                macro_scope,
                env,
                CURLINE(orig),
                info,
                actions);
    }

    FklStrBuf buf;
    fklInitStrBuf(&buf);

    FklFileType type = get_mod_file_type(FKL_VM_SYM(name)->str, &buf);
    if (type == FKL_FILE_NONE) {
        errors->error = make_import_failed_error(vm, name);
        errors->line = CURLINE(orig);
    }

    import_lib_impl(args, import_args, fklStrBufBody(&buf), lib_info, type);

    fklUninitStrBuf(&buf);
}

static void codegen_import_impl(const CgCbArgs *args,
        const CgImportHelperArgs *import_args,
        FklVMvalueCgInfo *lib_info) {
    if (import_args != NULL && import_args->name != NULL) {
        codegen_import_helper(args, import_args, lib_info);
        return;
    }

    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *errors = ctx->error_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    if (must_has_retval) {
        errors->error = make_has_no_value_error(vm, orig->value);
        errors->line = CURLINE(orig->container);
        return;
    }

    fklCgActVectorPushBack2(actions,
            make_cg_act(_empty_bc_process,
                    createStackCtx(),
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
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalueProto *proto = fklCreateVMvalueProto2(ctx->vm, env);
    fklPrintUndefinedRef(env->prev, &ctx->vm->gc->err_out);

    MacroContext *d = FKL_TYPE_CAST(MacroContext *, data);
    FklVMvalue *macro_bc = *fklValueVectorPopBackNonNull(bcl_vec);
    FklIns ret = FKL_MAKE_INS_I(FKL_OP_RET);
    fklByteCodeLntPushBackIns(FKL_VM_CO(macro_bc), ret, fid, line, scope);

    FklVMvalue *pattern = d->pattern;
    FklVMvalueCgMacroScope *macros = d->macro_scope;

    fklPeepholeOptimize(FKL_VM_CO(macro_bc));

    FklVMvalue *proc = fklCreateVMvalueProc(ctx->vm, macro_bc, proto);
    fklInitMainProcRefs(ctx->vm, proc);

    FklVMvalueCgMacro *macro = NULL;
    macro = fklCreateVMvalueCgMacro(ctx, FKL_VM_CDR(pattern), proc);
    FklVMvalue *head = FKL_VM_CAR(pattern);
    add_compiler_macro(ctx, macros->macros, head, macro);

    if (d->lib_info) {
        add_compiler_macro(ctx, d->lib_info->export_macros, head, macro);
    }

    return NULL;
}

static void _macro_stack_context_atomic(FklVMgc *gc, void *data) {
    MacroContext *d = data;
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->macro_scope), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, d->lib_info), gc);
    fklVMgcToGray(d->pattern, gc);
}

static const FklCgActCtxMt MacroStackContextMethodTable = {
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

static FklVMvalue *b_nil_replacement(const FklCgCtx *ctx,
        const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    return FKL_VM_NIL;
}

static FklVMvalue *b_is_main_replacement(const FklCgCtx *ctx,
        const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    if (env->proto_id != 0)
        return FKL_VM_NIL;
    if (env->prev == NULL || env->prev->proto_id == FKL_TOP_ENV_PROTO_ID)
        return FKL_VM_TRUE;
    return FKL_VM_NIL;
}

static FklVMvalue *b_platform_replacement(const FklCgCtx *ctx,
        const FklPmatchRes *orig,
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
    return fklCreateVMvalueStr1(ctx->vm, platform);
}

static FklVMvalue *b_file_dir_replacement(const FklCgCtx *ctx,
        const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    const char *d = info->filename == NULL //
                          ? ctx->cwd
                          : info->dir;

    const size_t d_len = strlen(d);
    const size_t s_len = strlen(FKL_PATH_SEPARATOR_STR);

    FklVMvalue *r = fklCreateVMvalueStr2(ctx->vm, s_len + d_len, NULL);
    memcpy(FKL_VM_STR(r)->str, d, d_len);
    strcat(FKL_VM_STR(r)->str, FKL_PATH_SEPARATOR_STR);

    return r;
}

static FklVMvalue *b_file_replacement(const FklCgCtx *ctx,
        const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    return info->filename == NULL
                 ? FKL_VM_NIL
                 : fklCreateVMvalueStr1(ctx->vm, info->filename);
}

static FklVMvalue *b_file_rp_replacement(const FklCgCtx *ctx,
        const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    return info->realpath == NULL
                 ? FKL_VM_NIL
                 : fklCreateVMvalueStr1(ctx->vm, info->realpath);
}

static FklVMvalue *b_line_replacement(const FklCgCtx *ctx,
        const FklPmatchRes *orig,
        const FklVMvalueCgEnv *env,
        const FklVMvalueCgInfo *info) {
    uint64_t line = CURLINE(orig->container);
    return line <= FKL_FIX_INT_MAX
                 ? FKL_MAKE_VM_FIX(line)
                 : fklCreateVMvalueBigIntWithU64(ctx->vm, line);
}

static struct SymbolReplacement {
    const char *s;
    ReplacementFunc func;
} builtInSymbolReplacement[FKL_BUILTIN_REPLACEMENT_NUM] = {
    { "nil", b_nil_replacement },
    { "*line*", b_line_replacement },
    { "*file*", b_file_replacement },
    { "*file-rp*", b_file_rp_replacement },
    { "*file-dir*", b_file_dir_replacement },
    { "*main?*", b_is_main_replacement },
    { "*platform*", b_platform_replacement },
};

static inline ReplacementFunc find_builtin_replacements(FklVMvalue *id,
        FklVMvalue *const builtin_replacement_id[]) {
    for (size_t i = 0; i < FKL_BUILTIN_REPLACEMENT_NUM; i++) {
        if (builtin_replacement_id[i] == id)
            return builtInSymbolReplacement[i].func;
    }
    return NULL;
}

typedef struct {
    int need_rebuild_all;
    FklVMvalue *name;
    FklVMvalue *rmacro;
} UpdateGrammerCtx;

static void update_gra_ctx_atomic(FklVMgc *gc, void *ctx) {
    UpdateGrammerCtx *d = ctx;
    fklVMgcToGray(d->name, gc);
    fklVMgcToGray(d->rmacro, gc);
}

static const FklCgActCtxMt UpdateGrammerCtxMethodTable = {
    .size = sizeof(UpdateGrammerCtx),
    .atomic = update_gra_ctx_atomic,
};

static FklCgActCtx *createUpdateGrammerCtx(int need_rebuild_all,
        FklVMvalue *name,
        FklVMvalue *rmacro) {
    FKL_ASSERT(name != NULL);
    FKL_ASSERT(rmacro != NULL);
    FklCgActCtx *r = createCgActCtx(&UpdateGrammerCtxMethodTable);
    UpdateGrammerCtx *p = FKL_TYPE_CAST(UpdateGrammerCtx *, r->d);
    p->need_rebuild_all = need_rebuild_all;
    p->name = name;
    p->rmacro = rmacro;
    return r;
}

static FklVMvalue *update_grammer_cb(const FklCgActCbArgs *args) {
    UpdateGrammerCtx *data = FKL_TYPE_CAST(UpdateGrammerCtx *, args->data);
    int need_rebuild_all = data->need_rebuild_all;
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    uint64_t line = args->line;
    FklCgErrorState *error_state = ctx->error_state;

    FklPairVector new_rmacros = { 0 };
    fklPairVectorInit(&new_rmacros, 1);
    fklPairVectorPushBack2(&new_rmacros,
            (FklPair){ .car = data->name, .cdr = data->rmacro });

    int err = update_grammer_impl(ctx,
            need_rebuild_all,
            line,
            info,
            &new_rmacros);
    fklPairVectorUninit(&new_rmacros);

    if (error_state->error != NULL)
        return NULL;

    if (err) {
        error_state->error = make_grammer_create_error(ctx->vm);
        error_state->line = line;
        return NULL;
    }
    return NULL;
}

static inline void add_new_rmacro(FklVMvalueCgInfo *lib_info,
        FklVMvalue *id,
        FklVMvalueCgRmacro *rmacro,
        const CgCbArgs *args) {
    FklCgCtx *ctx = args->ctx;
    FklVMvalueCgInfo *info = args->info;
    const FklPmatchRes *orig = args->orig;

    init_builtin_grammer(args->ctx, info);

    // 使用一个空的 reader macro 对象替换原来已有的 reader macro
    // 如果发生了替换，清空整个原本的 grammer ，重新添加所有 reader macro
    // 中的产生式
    // 同时，我们可能需要调换被覆盖的那个 hashmap node 的顺序
    // 将其移动到 hashmap 链表的末端，以保证顺序符合 defmacro 的顺序
    FklVMvalueCgRmacroHashMap *map = info->rmacros;

    int need_rebuild_all = do_add_rmacro(ctx, map, rmacro, id);

    MAKE_AND_PUSH_CG_ACT(update_grammer_cb,
            createUpdateGrammerCtx(need_rebuild_all, id, FKL_VM_VAL(rmacro)),
            NULL,
            args->scope,
            args->macro_scope,
            args->env,
            CURLINE(orig->container),
            args->info,
            args->actions);

    if (lib_info == NULL)
        return;

    FklVMvalueCgRmacroHashMap *export_map = lib_info->export_rmacros;
    int tmp = do_add_rmacro(ctx, export_map, rmacro, id);
    (void)tmp;
}

static inline int check_export_symbol_interned(FklVM *vm,
        FklVMvalue *s,
        uint64_t line,
        FklCgErrorState *errors) {
    FKL_ASSERT(FKL_IS_SYM(s));
    if (!FKL_VM_SYM_INTERNED(s)) {
        errors->error = make_export_error(vm,
                "export uninterned symbol %S is not allowed",
                s);
        errors->line = line;
        return 0;
    }
    return 1;
}

static FKL_ALWAYS_INLINE int is_top_level(FklVMvalueCgInfo *info,
        FklVMvalueCgEnv *env,
        FklVMvalueCgMacroScope *ms,
        uint32_t scope) {
    return env->prev == info->global_env //
        && scope < 2                     //
        && ms->prev == info->global_env->macros;
}

static void codegen_defmacro_impl(const CgCbArgs *args,
        FklVMvalueCgInfo *lib_info) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = ctx->vm;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *errors = ctx->error_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    if (must_has_retval) {
        errors->error = make_has_no_value_error(vm, orig->value);
        errors->line = CURLINE(orig->container);
        return;
    }

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_name);
    const FklPmatchRes *value =
            fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    if (FKL_IS_SYM(name->value)) {
        FklVMvalueCgRpl *rpl = NULL;
        rpl = fklCreateVMvalueCgRpl(ctx, value->value);
        fklCgRplHashMapSet(macro_scope->replacements, name->value, rpl);
        if (lib_info) {
            if (!check_export_symbol_interned(vm,
                        name->value,
                        CURLINE(name->container),
                        errors))
                return;
            fklCgRplHashMapSet(lib_info->export_replacement, name->value, rpl);
        }
    } else if (FKL_IS_PAIR(name->value)) {
        FklValueHashSet *symbol_set = NULL;
        FklVMvalue *pattern = fklCreatePattern(vm, name->value, &symbol_set);
        if (!pattern) {
            errors->error = make_macro_pattern_error(vm, name->value);
            errors->line = CURLINE(name->container);
            return;
        }

        if (lib_info
                && !check_export_symbol_interned(vm,
                        FKL_VM_CAR(pattern),
                        CURLINE(name->container),
                        errors)) {
            fklValueHashSetDestroy(symbol_set);
            return;
        }

        if (fklValueHashSetPut2(symbol_set, ctx->builtin_sym_orig)) {
            fklValueHashSetDestroy(symbol_set);
            errors->error = make_macro_pattern_error(vm, name->value);
            errors->line = CURLINE(name->container);
            return;
        }

        FklVMvalueCgEnv *macroEnv = NULL;
        FklVMvalueCgInfo *macro_info = macro_compile_prepare(ctx,
                info,
                macro_scope,
                symbol_set,
                &macroEnv,
                CURLINE(value->container));
        fklValueHashSetDestroy(symbol_set);

        CgExpQueue *queue = cgExpQueueCreate();
        cgExpQueuePush(queue, value);
        MAKE_AND_PUSH_CG_ACT(compiler_macro_bc_process,
                createMacroActionContext(name->value,
                        pattern,
                        macro_scope,
                        lib_info,
                        macroEnv->proto_id),
                createMustHasRetvalQueueNextExpression(queue),
                1,
                macroEnv->macros,
                macroEnv,
                CURLINE(value->container),
                macro_info,
                actions);
    } else if (FKL_IS_BOX(name->value)) {
        FklVMvalue *group_id = FKL_VM_BOX(name->value);
        if (lib_info
                && !check_export_symbol_interned(vm,
                        group_id,
                        CURLINE(name->container),
                        errors))
            return;

        if (!FKL_IS_SYM(group_id)) {
            errors->error = make_syntax_error(vm, name->value);
            errors->line = CURLINE(name->container);
            return;
        }

        FklVMvalue *rest = cddr(orig->value);

        FklVMvalueCgRmacro *rmacro = fklCgParseReaderMacroDefine(ctx,
                actions,
                rest,
                info,
                macro_scope);
        if (rmacro == NULL)
            return;

        add_new_rmacro(lib_info, group_id, rmacro, args);
    }
}

static void codegen_defmacro(const CgCbArgs *args) {
    codegen_defmacro_impl(args, NULL);
}

static void codegen_def_reader_macros_impl(const CgCbArgs *args,
        FklVMvalueCgInfo *lib_info) {
    FklPmatchHashMap *ht = args->ht;
    FklCgCtx *ctx = args->ctx;
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *errors = ctx->error_state;
    uint8_t const must_has_retval = args->must_has_retval;
    const FklPmatchRes *orig = args->orig;

    if (must_has_retval) {
        errors->error = make_has_no_value_error(vm, orig->value);
        errors->line = CURLINE(orig->container);
        return;
    }

    const FklPmatchRes *name = fklPmatchHashMapGet2(ht, ctx->builtin_sym_arg0);

    FklPmatchRes err_node = { 0 };
    if (!FKL_IS_BOX(name->value)) {
        err_node = *name;
        goto reader_macro_error;
    }

    FklVMvalue *group_id = FKL_VM_BOX(name->value);
    if (!FKL_IS_SYM(group_id)) {
        err_node = *name;
    reader_macro_error:
        if (errors->error != NULL)
            return;
        errors->error = make_syntax_error(vm, err_node.value);
        errors->line = CURLINE(err_node.container);
        return;
    }

    if (lib_info
            && !check_export_symbol_interned(vm,
                    group_id,
                    CURLINE(name->container),
                    errors)) {
        goto reader_macro_error;
    }

    FklVMvalue *rest = cddr(orig->value);
    FklVMvalueCgRmacro *rmacro =
            fklCgParseReaderMacroDefine(ctx, actions, rest, info, macro_scope);

    if (rmacro == NULL)
        goto reader_macro_error;

    add_new_rmacro(lib_info, group_id, rmacro, args);
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
    FklVM *vm = args->ctx->vm;
    uint32_t scope = args->scope;
    FklVMvalueCgMacroScope *macro_scope = args->macro_scope;
    FklVMvalueCgEnv *env = args->env;
    FklVMvalueCgInfo *info = args->info;
    FklCgActVector *actions = args->actions;
    FklCgErrorState *errors = ctx->error_state;
    uint8_t const must_has_retval = args->must_has_retval;
    FklPmatchRes orig = *args->orig;

    FklVMvalueCgInfo *lib_info = get_lib_info(info);
    if (!lib_info || !is_top_level(info, env, macro_scope, scope)) {
        errors->error = make_not_in_top_level_error(vm, orig.value);
        errors->line = CURLINE(orig.container);
        return;
    }

    FklPmatchRes v = *fklPmatchHashMapGet2(ht, ctx->builtin_sym_value);
    v.value = fklTryExpandCgMacro(ctx, &v, info, macro_scope);

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
            errors->error = make_has_no_value_error(vm, orig.value);
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

    if (is_begin_exp(v.value, patterns)) {
        FKL_VM_CAR(v.value) = FKL_VM_CAR(orig.value);

        queue = cgExpQueueCreate();
        cgExpQueuePush2(queue,
                (FklPmatchRes){
                    .value = v.value,
                    .container = v.container,
                });

        FklCgAct *act = make_cg_act(exports_bc_process,
                create_export_sequnce_context(&orig, must_has_retval),
                createDefaultQueueNextExpression(queue),
                1,
                macro_scope,
                env,
                CURLINE(orig.container),
                NULL,
                info);
        fklCgActVectorPushBack2(actions, act);

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

        if (!check_export_symbol_interned(vm, name, 0, errors))
            goto error;

        FklCgExportIdx *item = NULL;
        item = fklCgExportSidIdxHashMapGet2(&lib_info->exports, name);
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
                make_cg_act(_export_define_bc_process,
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

    if (errors->error == NULL)
        errors->error = make_syntax_error(vm, orig.value);
    errors->line = CURLINE(orig.container);
    return;
}

typedef void (*FklCgFunc)(const CgCbArgs *args);

static FklVMvalue *last_bc_process(const FklCgActCbArgs *args) {
    FklVM *vm = args->ctx->vm;
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *env = args->env;
    uint32_t scope = args->scope;
    FklValueVector *bcl_vec = args->bcl_vec;
    FklVMvalue *fid = args->fid;
    uint64_t line = args->line;

    FklVMvalue *r = sequnce_exp_bc_process(vm, bcl_vec, fid, line, scope);
    FklIns r_ins = FKL_MAKE_INS_I(FKL_OP_RET);
    fklByteCodeLntPushBackIns(FKL_VM_CO(r), r_ins, info->fid, info->curline, 1);
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
        fklByteCodeInsertFront(FKL_MAKE_INS_I(FKL_OP_PUSH_NIL), retval);
        break;
    case FKL_TAG_CHR: {
        char ch = FKL_GET_CHR(node);
        FklIns ins = FKL_MAKE_INS_IuB(FKL_OP_PUSH_CHAR, (uint16_t)ch);
        fklByteCodeInsertFront(ins, retval);
    } break;
    case FKL_TAG_FIX: {
        int64_t i = FKL_GET_FIX(node);
        if (i >= FKL_I24_MIN && i <= FKL_I24_MAX) {
            append_push_i24_ins(exe,
                    INS_APPEND_FRONT,
                    r,
                    i,
                    info->fid,
                    CURLINE(node),
                    scope);
            return r;
        } else {
        add_consts:
            k = fklValueTableAdd(&env->konsts, node) - 1;
            FKL_ASSERT(k <= FKL_U24_MAX);
            FklIns ins = FKL_MAKE_INS_IuC(FKL_OP_PUSH_CONST, k);
            fklByteCodeInsertFront(ins, retval);
        }
    } break;
    }

    FklByteCodelnt *rr = FKL_VM_CO(r);
    rr->ls = 1;
    rr->l = (FklLntItem *)fklZmalloc(sizeof(FklLntItem));
    FKL_ASSERT(rr->l);
    fklInitLineNumTabNode(&rr->l[0], info->fid, 0, CURLINE(node), scope);

    return r;
}

static inline int match_and_call(FklCgCtx *ctx,
        FklCgFunc func,
        const FklVMvalue *pattern,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        const FklPmatchRes *exp,
        FklCgActVector *actions,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info,
        uint8_t must_has_retval) {
    FklPmatchHashMap ht;
    fklPmatchHashMapInit(&ht);
    FklPmatchStorage storage = { .ht = &ht };
    fklPushCgPmatchStorage(ctx, &storage);

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
            .ctx = ctx,
            .must_has_retval = must_has_retval,
        };

        func(&args);
        fklChdir(ctx->cwd);
    }

    fklPopCgPmatchStorage(ctx, &storage);
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

static void mark_match_hash_map(const FklPmatchHashMap *ht, FklVMgc *gc) {
    for (const FklPmatchHashMapNode *cur = ht->first; cur; cur = cur->next) {
        fklVMgcToGray(cur->k, gc);
        fklVMgcToGray(cur->v.container, gc);
        fklVMgcToGray(cur->v.value, gc);
    }
}

static void codegen_ctx_extra_mark_func(FklVMgc *gc, FklVMextraMarkArgs *c) {
    FklCgCtx *ctx = FKL_TYPE_CAST(FklCgCtx *, c);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->main_env), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->main_info), gc);

    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->lnt), gc);
    fklVMgcToGray(FKL_VM_VAL(ctx->proto_env_map), gc);
    fklVMgcToGray(FKL_VM_VAL(ctx->hash_singleton), gc);

    fklVMgcToGray(ctx->cur_exp.value, gc);
    fklVMgcToGray(ctx->cur_exp.container, gc);

    for (const FklPmatchStorage *cur = ctx->ht_storage; cur; cur = cur->next) {
        mark_match_hash_map(cur->ht, gc);
    }

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

    fklVMgcMarkGrammer(gc, &ctx->builtin_g, NULL);

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

    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->libraries), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, ctx->macro_libraries), gc);
}

void fklRegisterCgCtx(FklCgCtx *ctx) {
    fklVMregisterExtraMarkFunc(ctx->vm->gc,
            FKL_TYPE_CAST(FklVMextraMarkArgs *, ctx),
            codegen_ctx_extra_mark_func,
            NULL);
}

void fklInitCgCtxExceptPattern(FklCgCtx *ctx, FklVM *vm) {
    memset(ctx, 0, sizeof(FklCgCtx));
    ctx->libraries = fklCreateVMvalueCgLibs(vm);
    ctx->macro_libraries = fklCreateVMvalueCgLibs(vm);

    ctx->vm = vm;

    fklRegisterCgCtx(ctx);

    fklInitBuiltinGrammer(&ctx->builtin_g, vm);

    ctx->lnt = fklCreateVMvalueLnt(vm);
    ctx->proto_env_map = (FklVMvalueCgEnvWeakMap *)FKL_VM_NIL;
    ctx->hash_singleton = FKL_VM_HASH(fklCreateVMvalueHashEq(vm));
}

static inline void init_builtin_patterns(FklCgCtx *ctx) {
    FklVM *vm = ctx->vm;
    FklVMvalue **const builtin_pattern_node = ctx->builtin_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++) {
        const char *str = builtin_pattern_cstr_func[i].ps;
        FklVMvalue *node = fklCreateAst1(vm, str, NULL);
        builtin_pattern_node[i] = fklCreatePattern(vm, node, NULL);
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
    FklVM *vm = ctx->vm;
    FklVMvalue **const builtin_sub_pattern_node = ctx->builtin_sub_pattern_node;
    for (size_t i = 0; i < FKL_CODEGEN_SUB_PATTERN_NUM; i++) {
        FklVMvalue *node = fklCreateAst1(vm, builtInSubPattern[i], NULL);
        builtin_sub_pattern_node[i] = fklCreatePattern(vm, node, NULL);
    }
}

void fklInitCgCtx(FklCgCtx *ctx, char *main_dir, FklVM *gc) {
    fklInitCgCtxExceptPattern(ctx, gc);
    ctx->cwd = fklSysgetcwd();
    ctx->main_file_real_path_dir = main_dir ? main_dir : fklZstrdup(ctx->cwd);

#define XX(A) ctx->builtin_sym_##A = add_symbol_cstr(ctx, #A);
    FKL_CODEGEN_SYMBOL_MAP
#undef XX

    fklInitProdActionList(ctx);

    init_builtin_patterns(ctx);
    init_builtin_sub_patterns(ctx);
    init_builtin_replacements(ctx);
}

void fklUnregisterCgCtx(FklCgCtx *ctx) {
    fklVMunregisterExtraMarkFunc(ctx->vm->gc, (FklVMextraMarkArgs *)ctx);
}

void fklUninitCgCtx(FklCgCtx *ctx) {
    fklUnregisterCgCtx(ctx);

    fklUninitGrammer(&ctx->builtin_g);
    ctx->vm = NULL;
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

    ctx->libraries = NULL;
    ctx->macro_libraries = NULL;
}

// 返回 0 表示匹配内建特殊形式
// 返回 1 表示该表达式是字面量
FKL_NODISCARD
static inline int map_builtin_pattern(FklCgCtx *ctx,
        const FklPmatchRes *cur_exp,
        FklCgActVector *actions,
        uint32_t scope,
        FklVMvalueCgMacroScope *macro_scope,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info,
        uint8_t must_has_retval) {
    FklVMvalue *const *builtin_pattern_node = ctx->builtin_pattern_node;

    if (fklIsList(cur_exp->value))
        for (size_t i = 0; i < FKL_CODEGEN_PATTERN_NUM; i++)
            if (match_and_call(ctx,
                        builtin_pattern_cstr_func[i].func,
                        builtin_pattern_node[i],
                        scope,
                        macro_scope,
                        cur_exp,
                        actions,
                        env,
                        info,
                        must_has_retval))
                return 0;

    if (FKL_IS_PAIR(cur_exp->value)) {
        codegen_funcall(cur_exp, actions, scope, macro_scope, env, info, ctx);
        return 0;
    }
    return 1;
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

// 检查是否需要将结果传给上一个 action
static inline int is_need_process_bc(const FklCgAct *cur_action,
        const FklCgActVector *act_vec) {
    return (fklCgActVectorIsEmpty(act_vec)
            || *fklCgActVectorBack(act_vec) != cur_action);
}

static inline FklVMvalue *try_get_rpl(const FklCgCtx *ctx,
        FklVMvalueCgMacroScope *cs,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info,
        FklPmatchRes exp) {
    FklVMvalueCgRpl *rpl = NULL;
    for (; cs; cs = cs->prev) {
        rpl = fklCgRplHashMapGet(cs->replacements, exp.value);
        if (rpl != NULL)
            break;
    }

    if (rpl) {
        return rpl->value;
    }

    ReplacementFunc f = NULL;
    f = find_builtin_replacements(exp.value, ctx->builtin_replacement_id);
    if (f != NULL) {
        FklVMvalue *t = f(ctx, &exp, env, info);
        FKL_ASSERT(t);
        return t;
    }

    return NULL;
}

static FKL_ALWAYS_INLINE FklCgAct *get_prev_act(FklCgAct *cur_action,
        const FklCgActVector *act_vec) {
    FklCgAct *prev = cur_action->prev //
                           ? cur_action->prev
                           : fklCgActVectorIsEmpty(act_vec)
                                     ? NULL
                                     : *fklCgActVectorBackNonNull(act_vec);
    return prev;
}

FklVMvalue *fklGenExpressionCodeExt(FklCgCtx *ctx,
        size_t act_count,
        FklCgAct *const *actions) {
    FklVM *vm = ctx->vm;
    FklValueVector results;
    fklValueVectorInit(&results, 1);
    FklCgErrorState error_state = { 0 };
    FklCgActVector act_vec;
    fklCgActVectorInit(&act_vec, 32);

    for (size_t i = 0; i < act_count; ++i) {
        fklCgActVectorPushBack2(&act_vec, actions[i]);
    }

    ctx->action_vector = &act_vec;
    ctx->error_state = &error_state;
    ctx->bcl_vector = &results;

    while (!fklCgActVectorIsEmpty(&act_vec)) {
        FklCgAct *cur_action = *fklCgActVectorBackNonNull(&act_vec);
        FklVMvalueCgInfo *info = cur_action->info;
        FklCgNextExp *expressions = cur_action->exps;
        FklVMvalueCgEnv *env = cur_action->env;
        FklCgActCtx *cur_context = cur_action->ctx;
        int r = 1;
        if (expressions) {
            FklCgGetNextExpCb get_next_expression =
                    expressions->t->get_next_exp;
            uint8_t must_has_retval = expressions->must_has_retval;

            FklPmatchRes exp = { 0 };
            while (get_next_expression(ctx, expressions->context, &exp)) {
            skip:
                ctx->cur_exp = exp;
                if (FKL_IS_PAIR(exp.value)) {
                    exp.value = fklTryExpandCgMacro(ctx,
                            &exp,
                            info,
                            cur_action->macros);
                    if (error_state.error) {
                        break;
                    }
                    if (must_has_retval == FIRST_MUST_HAS_RETVAL) {
                        must_has_retval = DO_NOT_NEED_RETVAL;
                        expressions->must_has_retval = DO_NOT_NEED_RETVAL;
                    }
                } else if (FKL_IS_SYM(exp.value)) {
                    FklVMvalueCgMacroScope *cs = cur_action->macros;
                    FklVMvalue *rpl_v = try_get_rpl(ctx, cs, env, info, exp);
                    if (rpl_v != NULL) {
                        exp.value = rpl_v;
                        goto skip;
                    }

                    FklVMvalue *bcl = NULL;
                    FklSymDefHashMapElm *def = NULL;
                    def = fklFindSymbolDef(exp.value, cur_action->scope, env);
                    if (def) {
                        bcl = append_get_loc_ins(vm,
                                INS_APPEND_BACK,
                                NULL,
                                def->v.idx,
                                info->fid,
                                CURLINE(exp.container),
                                cur_action->scope);
                    } else {
                        uint32_t idx = fklAddCgRefBySidRetIndex(exp.value,
                                env,
                                info->fid,
                                CURLINE(exp.container),
                                0);
                        bcl = append_get_var_ref_ins(vm,
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

                FKL_ASSERT(exp.value);
                r = map_builtin_pattern(ctx,
                        &exp,
                        &act_vec,
                        cur_action->scope,
                        cur_action->macros,
                        env,
                        info,
                        must_has_retval);
                if (r) {
                    fklValueVectorPushBack2(&cur_action->bcl_vector,
                            gen_push_literal_code(vm,
                                    &exp,
                                    info,
                                    env,
                                    cur_action->scope));
                }

                if (error_state.error != NULL)
                    break;

                if (!r && is_need_process_bc(cur_action, &act_vec))
                    break;
            }
        }
        if (error_state.error) {
        print_error:
            fklPrintCgError(ctx, info, &ctx->vm->gc->err_out);
            ctx->action_vector = NULL;
            ctx->error_state = NULL;
            while (!fklCgActVectorIsEmpty(&act_vec)) {
                destroy_cg_action(*fklCgActVectorPopBackNonNull(&act_vec));
            }
            fklCgActVectorUninit(&act_vec);
            fklValueVectorUninit(&results);
            fklVMgcCheckPoint(vm, FORCE_GC);
            return NULL;
        }

        FklCgAct *other_cg_act = *fklCgActVectorBackNonNull(&act_vec);
        if (other_cg_act == cur_action) {
            fklCgActVectorPopBack(&act_vec);
            FklCgAct *prev_act = get_prev_act(cur_action, &act_vec);

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
                .prev = cur_action->prev,
            };

            FklVMvalue *result = cur_action->cb(&args);

            if (result) {
                if (fklCgActVectorIsEmpty(&act_vec))
                    fklValueVectorPushBack2(&results, result);
                else {
                    fklValueVectorPushBack2(&prev_act->bcl_vector, result);
                }
                fklVMgcCheckPoint(vm, FORCE_GC);
            }
            if (error_state.error) {
                fklCgActVectorPushBack2(&act_vec, cur_action);
                goto print_error;
            }
            destroy_cg_action(cur_action);
        }
    }
    fklVMgcCheckPoint(vm, FORCE_GC);
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

static inline FklCgAct *make_last_act(FklCgAct *act) {
    return make_cg_act(last_bc_process,
            createStackCtx(),
            NULL,
            act->scope,
            act->macros,
            act->env,
            act->curline,
            NULL,
            act->info);
}

FklVMvalue *fklGenExpressionCodeWithAction(FklCgCtx *ctx,
        FklCgAct *initial_action) {

    FklCgAct *action1 = make_last_act(initial_action);
    FklCgAct *actions[2] = { action1, initial_action };
    return fklGenExpressionCodeExt(ctx, 2, actions);
}

FklVMvalue *fklGenExpressionCodeWithFp(FklCgCtx *ctx,
        FILE *fp,
        FklVMvalueCgInfo *info,
        FklVMvalueCgEnv *env) {
    FklCgAct *initialAction = make_cg_act(_begin_exp_bc_process,
            createStackCtx(),
            createFpNextExpression(fp, info),
            1,
            env->macros,
            env,
            1,
            NULL,
            info);
    return fklGenExpressionCodeWithAction(ctx, initialAction);
}

FklVMvalue *fklGenExpressionCode(FklCgCtx *ctx,
        FklVMvalue *exp,
        FklVMvalueCgEnv *env,
        FklVMvalueCgInfo *info) {
    FklVMvalue *cont = fklCreateVMvalueBox(ctx->vm, exp);
    put_line_number(info->lnt, cont, info->curline);
    CgExpQueue *queue = cgExpQueueCreate();
    cgExpQueuePush2(queue, (FklPmatchRes){ .value = exp, .container = cont });
    FklCgAct *initialAction = make_cg_act(_default_bc_process,
            createStackCtx(),
            createDefaultQueueNextExpression(queue),
            1,
            env->macros,
            env,
            CURLINE(exp),
            NULL,
            info);
    return fklGenExpressionCodeWithAction(ctx, initialAction);
}
