#include "bdb.h"

#include <fakeLisp/builtin.h>
#include <fakeLisp/zmalloc.h>
#include <string.h>

static inline void replace_func_prototype(FklVMvalueCodegenInfo *info,
        uint32_t p,
        FklVMvalueCodegenEnv *env,
        FklSid_t sid,
        uint32_t line,
        FklSymbolTable *pst,
        uint32_t replaced_prototype_id) {
    FklFuncPrototype *pt = &info->pts->pa[replaced_prototype_id];
    fklUninitFuncPrototype(pt);
    env->prototypeId = replaced_prototype_id;
    fklInitFuncPrototypeWithEnv(pt, info, env, sid, line, pst);
}

static inline FklVMvalueCodegenEnv *init_codegen_info_common_helper(
        DebugCtx *ctx,
        FklVMvalueCodegenEnv **origin_outer_env,
        FklVMframe *f,
        FklVMvalueCodegenEnv **penv) {
    FklVMproc *proc = FKL_VM_PROC(f->c.proc);
    FklByteCodelnt *code = FKL_VM_CO(proc->codeObj);
    const FklLineNumberTableItem *ln =
            getCurLineNumberItemWithCp(f->c.pc, code);
    if (ln->scope == 0)
        return NULL;
    FklVMvalueCodegenEnv *env = getEnv(ctx, proc->protoId);
    if (env == NULL)
        return NULL;
    *origin_outer_env = env->prev;
    env->prev = NULL;
    FklVMvalueCodegenEnv *new_env =
            fklCreateVMvalueCodegenEnv(&ctx->codegen_ctx, env, ln->scope, NULL);
    new_env->is_debugging = 1;
    ctx->eval_info = fklCreateVMvalueCodegenInfo(&ctx->codegen_ctx,
            NULL,
            NULL,
            &(FklCodegenInfoArgs){
                .st = ctx->st,
                .kt = ctx->kt,
                .pts = ctx->gc.pts,
            });

    *penv = env;
    ctx->eval_env = new_env;
    return new_env;
}

static inline FklVMvalueCodegenEnv *init_codegen_info_with_debug_ctx(
        DebugCtx *ctx,
        FklVMvalueCodegenEnv **origin_outer_env,
        FklVMframe *f) {
    FklVMvalueCodegenEnv *env = NULL;
    FklVMvalueCodegenEnv *new_env =
            init_codegen_info_common_helper(ctx, origin_outer_env, f, &env);

    if (new_env) {
        replace_func_prototype(ctx->eval_info,
                env->prototypeId,
                new_env,
                0,
                ctx->curline,
                ctx->st,
                ctx->temp_proc_prototype_id);
        return new_env;
    }
    return NULL;
}

static inline void set_back_origin_prev_env(FklVMvalueCodegenEnv *new_env,
        FklVMvalueCodegenEnv *origin_outer_env) {
    FklVMvalueCodegenEnv *env = new_env->prev;
    env->prev = origin_outer_env;
}

static inline void resolve_reference(DebugCtx *ctx,
        FklVMvalueCodegenEnv *env,
        FklVM *vm,
        FklFuncPrototype *pt,
        FklVMvalue *proc_obj,
        FklVMframe *cur_frame) {
    FklVMproc *proc = FKL_VM_PROC(proc_obj);
    uint32_t rcount = pt->rcount;
    proc->rcount = rcount;
    proc->closure = (FklVMvalue **)fklZcalloc(rcount, sizeof(FklVMvalue *));
    FKL_ASSERT(proc->closure);
    fklCreateVMvalueClosureFrom(vm, proc->closure, cur_frame, 0, pt);
    FklUnReSymbolRefVector *urefs = &env->prev->uref;
    FklVMvalue **builtin_refs = vm->gc->builtin_refs;
    while (!fklUnReSymbolRefVectorIsEmpty(urefs)) {
        FklUnReSymbolRef *uref = fklUnReSymbolRefVectorPopBackNonNull(urefs);
        FklSymDef *ref = fklGetCodegenRefBySid(uref->id, ctx->glob_env);
        if (ref)
            proc->closure[uref->idx] = builtin_refs[ref->cidx];
    }
}

static inline int has_import_op_code(const FklByteCodelnt *lnt) {
    const FklInstruction *cur = lnt->bc.code;
    const FklInstruction *const end = &cur[lnt->bc.len];
    for (; cur < end; cur++)
        if (cur->op == FKL_OP_LOAD_DLL || cur->op == FKL_OP_LOAD_LIB)
            return 1;
    return 0;
}

static inline FklVMvalue *compile_expression_common_helper(DebugCtx *ctx,
        FklVMvalueCodegenInfo *info,
        FklVMvalueCodegenEnv *tmp_env,
        FklNastNode *exp,
        FklVMvalueCodegenEnv *origin_outer_env,
        FklVM *vm,
        FklVMframe *cur_frame,
        EvalCompileErr *err) {
    FklByteCodelnt *code = fklGenExpressionCode(exp, tmp_env, info);
    fklDestroyNastNode(exp);
    set_back_origin_prev_env(tmp_env, origin_outer_env);
    FklVMvalue *proc = NULL;
    if (code) {
        if (has_import_op_code(code)) {
            *err = EVAL_COMP_IMPORT;
            fklDestroyByteCodelnt(code);
            return NULL;
        }
        FklFuncPrototypes *pts = info->pts;
        fklUpdatePrototype(pts, tmp_env, ctx->st, ctx->st);

        FklFuncPrototype *pt = &pts->pa[tmp_env->prototypeId];
        FklVMvalue *code_obj = fklCreateVMvalueCodeObjMove(vm, code);
        fklDestroyByteCodelnt(code);
        proc = fklCreateVMvalueProc(vm, code_obj, tmp_env->prototypeId);
        resolve_reference(ctx, tmp_env, vm, pt, proc, cur_frame);
        fklVMgcUpdateConstArray(vm->gc, vm->gc->kt);
    }
    return proc;
}

FklVMvalue *compileEvalExpression(DebugCtx *ctx,
        FklVM *vm,
        FklNastNode *exp,
        FklVMframe *cur_frame,
        EvalCompileErr *err) {
    FklVMvalueCodegenEnv *origin_outer_env = NULL;
    FklVMvalueCodegenEnv *tmp_env =
            init_codegen_info_with_debug_ctx(ctx, &origin_outer_env, cur_frame);
    if (tmp_env == NULL) {
        *err = EVAL_COMP_UNABLE;
        fklDestroyNastNode(exp);
        return NULL;
    }
    *err = 0;
    fklMakeNastNodeRef(exp);
    FklVMvalue *proc = compile_expression_common_helper(ctx,
            ctx->eval_info,
            tmp_env,
            exp,
            origin_outer_env,
            vm,
            cur_frame,
            err);

    return proc;
}

static inline FklVMvalueCodegenEnv *
init_codegen_info_for_cond_bp_with_debug_ctx(DebugCtx *ctx,
        FklVMvalueCodegenEnv **origin_outer_env,
        FklVMframe *f) {
    FklVMvalueCodegenEnv *env = NULL;
    FklVMvalueCodegenEnv *new_env =
            init_codegen_info_common_helper(ctx, origin_outer_env, f, &env);

    if (new_env) {
        if (fklUintVectorIsEmpty(&ctx->bt.unused_prototype_id_for_cond_bp))
            fklCreateFuncPrototypeAndInsertToPool(ctx->eval_info,
                    env->prototypeId,
                    new_env,
                    0,
                    ctx->curline,
                    ctx->st);
        else {
            uint32_t pid = *fklUintVectorPopBackNonNull(
                    &ctx->bt.unused_prototype_id_for_cond_bp);
            replace_func_prototype(ctx->eval_info,
                    env->prototypeId,
                    new_env,
                    0,
                    ctx->curline,
                    ctx->st,
                    pid);
        }
        return new_env;
    }
    return NULL;
}

FklVMvalue *compileConditionExpression(DebugCtx *ctx,
        FklVM *vm,
        FklNastNode *exp,
        FklVMframe *cur_frame,
        EvalCompileErr *err) {
    FklVMvalueCodegenEnv *origin_outer_env = NULL;
    FklVMvalueCodegenEnv *tmp_env =
            init_codegen_info_for_cond_bp_with_debug_ctx(ctx,
                    &origin_outer_env,
                    cur_frame);
    if (tmp_env == NULL) {
        *err = EVAL_COMP_UNABLE;
        fklDestroyNastNode(exp);
        return NULL;
    }
    *err = 0;
    fklMakeNastNodeRef(exp);
    FklVMvalue *proc = compile_expression_common_helper(ctx,
            ctx->eval_info,
            tmp_env,
            exp,
            origin_outer_env,
            vm,
            cur_frame,
            err);

    if (!proc)
        fklUintVectorPushBack2(&ctx->bt.unused_prototype_id_for_cond_bp,
                tmp_env->prototypeId);
    return proc;
}

FklVMvalue *callEvalProc(DebugCtx *ctx,
        FklVM *vm,
        FklVMvalue *proc,
        FklVMframe *origin_cur_frame) {
    FklVMvalue *retval = NULL;

    FklVMrecoverArgs recover_args = { 0 };

    FklVMcallResult r = fklVMcall2(fklRunVM2, vm, &recover_args, proc, 0, NULL);

    FklVMvalue *v = r.v;
    if (r.err) {
        fklPrintErrBacktrace(v, vm, stderr);
    } else {
        retval = r.v;
    }

    fklVMrecover(vm, &recover_args);

    return retval;
}

void putEnv(DebugCtx *ctx, FklVMvalueCodegenEnv *env) {
    bdbEnvHashMapPut2(&ctx->envs, env->prototypeId, env);
}

FklVMvalueCodegenEnv *getEnv(DebugCtx *ctx, uint32_t id) {
    FklVMvalueCodegenEnv **i = bdbEnvHashMapGet2(&ctx->envs, id);
    if (i)
        return *i;
    return NULL;
}
