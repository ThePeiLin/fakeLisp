#include "bdb.h"

#include <fakeLisp/builtin.h>
#include <fakeLisp/codegen.h>
#include <fakeLisp/parser.h>
#include <fakeLisp/str_buf.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>

#include <string.h>

static FKL_ALWAYS_INLINE FklVMvalueCgEnv *get_env(DebugCtx *ctx,
        FklVMvalueProto *pt) {
    FklVMvalueCgEnvWeakMap *weak_map = ctx->cg_ctx.proto_env_map;
    FklVMvalueCgEnv *i = fklVMvalueCgEnvWeakMapGet(weak_map, pt);
    return i;
}

static inline FklVMvalueCgEnv *init_codegen_info_impl(DebugCtx *ctx,
        FklVMvalueCgEnv **origin_outer_env,
        FklVMframe *f,
        FklVMvalueCgEnv **penv) {
    FklVMvalueProc *proc = FKL_VM_PROC(f->proc);
    FklByteCodelnt *code = FKL_VM_CO(proc->bcl);
    const FklLntItem *ln = fklGetLntItem(code, f->pc);
    if (ln->scope == 0)
        return NULL;
    FklVMvalueProto *proto = proc->proto;
    FklVMvalueCgEnv *env = get_env(ctx, proto);
    if (env == NULL)
        return NULL;
    *origin_outer_env = env->prev;

    env->prev = NULL;

    // 防止 eval 创建的 proto 不会被清理
    env->child_proc_protos.size = proto->child_proto_count;

    FklVMvalueCgEnv *new_env = fklCreateVMvalueCgEnv(&ctx->cg_ctx,
            &(const FklCgEnvCreateArgs){
                .prev_env = env,
                .prev_ms = NULL,
                .parent_scope = ln->scope,
                .filename = FKL_VM_NIL,
                .name = FKL_VM_NIL,
                .line = 1,
            });
    new_env->is_debugging = 1;
    ctx->eval_info = fklCreateVMvalueCgInfo(&ctx->cg_ctx, NULL, NULL, NULL);

    *penv = env;
    ctx->eval_env = new_env;
    return new_env;
}

static FKL_ALWAYS_INLINE FklVMvalueCgEnv *init_eval_codegen_info(DebugCtx *ctx,
        FklVMvalueCgEnv **pout_env,
        FklVMframe *f) {
    FklVMvalueCgEnv *env = NULL;
    FklVMvalueCgEnv *new_env = init_codegen_info_impl(ctx, pout_env, f, &env);
    return new_env;
}

static FKL_ALWAYS_INLINE FklVMvalueCgEnv *
set_back_origin_prev_env(FklVMvalueCgEnv *new_env, FklVMvalueCgEnv *outer_env) {
    FklVMvalueCgEnv *env = new_env->prev;
    env->prev = outer_env;
    return env;
}

static inline int has_import_op_code(const FklByteCodelnt *lnt) {
    const FklIns *cur = lnt->bc.code;
    const FklIns *const end = &cur[lnt->bc.len];
    for (; cur < end; cur++)
        if (cur->op == FKL_OP_LOAD_DLL || cur->op == FKL_OP_LOAD_LIB)
            return 1;
    return 0;
}

typedef struct {
    FklVMvalueCgInfo *info;
    FklVMvalueCgEnv *exp_env;
    FklVMvalueCgEnv *outer_env;
    FklVMvalue *exp;

    FklVM *vm;
    FklVMframe *cur_frame;

    EvalErr *err;
} CompEvalExpArgs;

static inline FklVMvalueProc *compile_eval_expression(DebugCtx *ctx,
        const CompEvalExpArgs *args) {
    FklVMvalueCgInfo *info = args->info;
    FklVMvalueCgEnv *exp_env = args->exp_env;
    FklVMvalueCgEnv *outer_env = args->outer_env;
    FklVMvalue *exp = args->exp;

    FklVM *vm = args->vm;
    FklVMframe *cur_frame = args->cur_frame;
    EvalErr *err = args->err;

    FklCgCtx *cg_ctx = &ctx->cg_ctx;
    FklVMvalue *code = fklGenExpressionCode(cg_ctx, exp, exp_env, info);
    FklVMvalueCgEnv *env = set_back_origin_prev_env(exp_env, outer_env);

    FklVMvalueProc *ret = NULL;
    if (code) {
        if (has_import_op_code(FKL_VM_CO(code))) {
            *err = EVAL_ERR_IMPORT;
            return NULL;
        }
        FklVMvalueProto *proto = fklCreateVMvalueProto2(vm, exp_env);
        FklVMvalue *proc = fklCreateVMvalueProc(vm, code, proto);
        ret = FKL_VM_PROC(proc);

        // 设置对内建符号的引用
        const FklUnbound *cur = &env->uref.base[0];
        const FklUnbound *const end = &env->uref.base[env->uref.size];

        const FklVMvalueCgEnv *glob_env = ctx->glob_env;
        FklVMvalue **const proc_refs = ret->closure;
        FklVMvalue *const *builtin_refs = vm->gc->builtin_refs;
        for (; cur < end; ++cur) {
            // 不允许对内建符号赋值
            if (cur->assign) {
                continue;
            }

            const FklSymDef *ref;
            FklSidScope key = { .sid = cur->sid, .scope = 1 };
            ref = fklSymDefHashMapGet(&glob_env->refs, &key);

            if (ref == NULL)
                continue;

            proc_refs[cur->idx] = builtin_refs[ref->cidx];
        }

        fklVMfetchVarRef(vm, ret, cur_frame);
    }
    env->uref.size = 0;
    fklUnboundVectorShrinkToFit(&env->uref);

    return ret;
}

BdbWrapper compileEvalExpression(DebugCtx *ctx,
        FklVM *vm,
        const FklString *exp,
        FklVMframe *cur_frame,
        EvalErr *err) {
    FklVMvalue *v = fklCreateAst(vm, exp, NULL);
    if (v == NULL) {
        *err = EVAL_ERR_INVALID_EXP;
        return BDB_NONE;
    }
    return compileEvalExpression1(ctx, vm, bdbWrap(v), cur_frame, err);
}

BdbWrapper compileEvalExpression1(DebugCtx *ctx,
        FklVM *vm,
        BdbWrapper exp,
        FklVMframe *cur_frame,
        EvalErr *err) {
    FKL_ASSERT(bdbHas(exp));
    FklVMvalue *v = bdbUnwrap(exp);

    FklVMvalueCgEnv *out_env = NULL;
    FklVMvalueCgEnv *exp_env;
    exp_env = init_eval_codegen_info(ctx, &out_env, cur_frame);
    if (exp_env == NULL) {
        *err = EVAL_ERR_UNABLE;
        return BDB_NONE;
    }
    *err = 0;

    CompEvalExpArgs eval_args = {
        .info = ctx->eval_info,
        .exp_env = exp_env,
        .outer_env = out_env,
        .exp = v,

        .vm = vm,
        .cur_frame = cur_frame,
        .err = err,
    };

    FklVMvalueProc *proc = compile_eval_expression(ctx, &eval_args);

    return bdbWrap(FKL_VM_VAL(proc));
}

static inline FklVMvalue *get_backtrace_info(FklVM *host_vm,
        FklVM *reached_thread) {
    FklVMvalue *retval = NULL;
    FklVMvalue **ppcdr = &retval;

    FklStrBuf buf = { 0 };
    fklInitStrBuf(&buf);

    FklCodeBuilder builder = { 0 };
    fklInitCodeBuilderStrBuf(&builder, &buf, NULL);

    for (const FklVMframe *cur = reached_thread->top_frame; cur;
            cur = cur->prev) {
        fklStrBufClear(&buf);
        fklPrintFrame(cur, reached_thread, &builder);

        FklVMvalue *frame_info = fklCreateVMvalueStr2(host_vm, //
                buf.index,
                buf.buf);
        FklVMvalue *r = fklCreateVMvaluePair(host_vm, FKL_VM_NIL, frame_info);
        FklVMvalue *cur_pair = fklCreateVMvaluePair1(host_vm, r);
        *ppcdr = cur_pair;
        ppcdr = &FKL_VM_CDR(cur_pair);
    }

    fklUninitStrBuf(&buf);

    return retval;
}

BdbWrapper callEvalProc(DebugCtx *ctx,
        FklVM *host_vm,
        FklVM *reached_thread,
        BdbWrapper proc,
        FklVMframe *origin_cur_frame) {

    FklVMrecoverArgs recover_args = { 0 };

    FklVMcallResult r;
    r = fklVMcall2(fklRunVM2,
            reached_thread,
            &recover_args,
            bdbUnwrap(proc),
            0,
            NULL);

    FklVMvalue *v = r.v;
    BdbWrapper ret = BDB_NONE;
    if (r.err) {
        ret = bdbWrap(v);
        if (host_vm != NULL) {
            FklVMvalue *v = get_backtrace_info(host_vm, reached_thread);
            ctx->backtrace_list = v;
        }
    } else {
        ret = bdbWrap(fklCreateVMvalueBox(reached_thread, r.v));
    }

    fklVMrecover(reached_thread, &recover_args);

    return ret;
}

FklVMvalue *listThreads(DebugCtx *ctx, FklVM *host_vm) {
    FklVMvalue *retval = FKL_VM_NIL;
    FklVMvalue **ppcdr = &retval;
    FklVM **base = (FklVM **)ctx->threads.base;
    uint32_t top = ctx->threads.size;
    for (uint32_t i = 0; i < top; i++) {
        FklVM *cur = base[i];
        int is_cur = i + 1 == ctx->curthread_idx;

        FklVMvalue *no = FKL_MAKE_VM_FIX(i + 1);

        FklVMvalue *backtrace_list = cur->top_frame == NULL
                                           ? FKL_VM_NIL
                                           : get_backtrace_info(host_vm, cur);
        FklVMvalue *info_vec = fklCreateVMvalueVecExt(host_vm,
                3,
                is_cur ? FKL_VM_TRUE : FKL_VM_NIL,
                no,
                backtrace_list);

        FklVMvalue *cur_pair = fklCreateVMvaluePair1(host_vm, info_vec);

        *ppcdr = cur_pair;
        ppcdr = &FKL_VM_CDR(cur_pair);
    }

    return retval;
}
