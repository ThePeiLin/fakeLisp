#include "bdb.h"
#include "fakeLisp/bytecode.h"

static void interrupt_queue_work_cb(FklVM *vm, void *a) {
    DbgInterruptArg *arg = (DbgInterruptArg *)a;
    DebugCtx *ctx = arg->ctx;
    if (ctx->reached_thread)
        return;
    if (arg->bp) {
        Breakpoint *bp = arg->bp;
        atomic_fetch_sub(&bp->reached_count, 1);
        for (; bp; bp = bp->next) {
            if (bp->is_deleted || bp->is_disabled)
                continue;
            if (bp->cond_exp_obj) {
                if (!bp->is_compiled) {
                    EvalCompileErr err;
                    bp->is_compiled = 1;
                    FklVMvalue *proc = compileConditionExpression(ctx,
                            vm,
                            bp->cond_exp,
                            vm->top_frame,
                            &err);
                    bp->cond_exp = NULL;
                    bp->proc = proc;
                    switch (err) {
                    case EVAL_COMP_UNABLE:
                        printUnableToCompile(stderr);
                        break;
                    case EVAL_COMP_IMPORT:
                        printNotAllowImport(stderr);
                        break;
                    }
                }
                if (bp->proc) {
                    FklVMvalue *value =
                            callEvalProc(ctx, vm, bp->proc, vm->top_frame);
                    if (value == FKL_VM_NIL)
                        continue;
                }
            }
            ctx->reached_breakpoint = bp;
            setReachedThread(ctx, vm);
            getCurLineStr(ctx, bp->fid, bp->line);
            bp->count++;
            if (bp->is_temporary)
                delBreakpoint(ctx, bp->idx);
            longjmp(*ctx->jmpb, DBG_INTERRUPTED);
        }
    } else if (arg->ln) {
        setReachedThread(ctx, vm);
        getCurLineStr(ctx, arg->ln->fid, arg->ln->line);
        longjmp(*ctx->jmpb, DBG_INTERRUPTED);
    } else {
        setReachedThread(ctx, vm);
        if (arg->err)
            longjmp(*ctx->jmpb, DBG_ERROR_OCCUR);
        else
            longjmp(*ctx->jmpb, DBG_INTERRUPTED);
    }
}

void dbgInterrupt(FklVM *exe, DbgInterruptArg *arg) {
    fklQueueWorkInIdleThread(exe, interrupt_queue_work_cb, arg);
}

FklVMinterruptResult dbgInterruptHandler(FklVM *exe,
        FklVMvalue *int_val,
        FklVMvalue **pv,
        void *arg) {
    FKL_ASSERT(int_val);

    DebugCtx *ctx = (DebugCtx *)arg;
    if (isBreakpointWrapper(int_val)) {
        Breakpoint *bp = getBreakpointFromWrapper(int_val);
        DbgInterruptArg arg = {
            .bp = bp,
            .ctx = ctx,
        };
        dbgInterrupt(exe, &arg);
    } else if (int_val == BDB_STEP_BREAK) {
        DbgInterruptArg arg = {
            .ctx = ctx,
        };
        unsetStepping(ctx);
        dbgInterrupt(exe, &arg);
    } else if (fklIsVMvalueError(int_val)) {
        if (exe->is_single_thread)
            return FKL_INT_NEXT;
        else {
            fklPrintErrBacktrace(int_val, exe, stdout);
            DbgInterruptArg arg = {
                .err = 1,
                .ctx = ctx,
            };
            unsetStepping(ctx);
            dbgInterrupt(exe, &arg);
        }
    } else
        return FKL_INT_NEXT;
    return FKL_INT_DONE;
}

typedef struct {
    size_t i;
    uint8_t flags;
} SetSteppingArgs;

static inline void set_stepping_target(struct SteppingCtx *stepping_ctx,
        FklInstruction *target,
        const SetSteppingArgs *args) {
    stepping_ctx->ins[args->i] = *target;
    stepping_ctx->target_ins[args->i] = target;
    FklOpcode op = target->op;
    memset(target, 0, sizeof(FklInstruction));
    target->op = FKL_OP_DUMMY;
    target->au = args->flags;

    if (op == FKL_OP_DUMMY)
        target->au |= INT3_STEP_AT_BP;
}

static inline void set_step_ins(DebugCtx *ctx,
        FklVM *exe,
        StepTargetPlacingType placing_type,
        SteppingMode mode) {
    if (exe == NULL || exe->top_frame == NULL)
        return;
    uint8_t int3_flags = INT3_STEPPING;
    ctx->stepping_ctx.vm = exe;

    FklVMframe *f = exe->top_frame;
    for (; f && f->type == FKL_FRAME_OTHEROBJ; f = f->prev)
        ;
    if (f == NULL)
        return;
    int r = 0;
    FklInstruction *ins[2] = { NULL, NULL };
    if (placing_type == STEP_INS_CUR) {
        ins[0] = f->c.pc;
        r = 1;
        goto set_target;
    }

    r = fklGetNextIns2(f->c.pc, ins);
    if (r != 0)
        goto set_target;

    r = 1;
    if (fklIsCallIns(f->c.pc)) {
        FklVMvalue *proc = exe->base[exe->bp];
        if (mode == STEP_INTO && FKL_IS_PROC(proc)) {
            FklVMproc *p = FKL_VM_PROC(proc);
            ins[0] = p->spc;
        } else {
            FklInstructionArg arg = { 0 };
            int8_t l = fklGetInsOpArg(f->c.pc, &arg);
            ins[0] = f->c.pc + l;
        }

    } else if (fklIsLoadLibIns(f->c.pc)) {
        FklInstructionArg arg = { 0 };
        int8_t l = fklGetInsOpArg(f->c.pc, &arg);
        FklVMlib *plib = &exe->gc->libs[arg.ux];
        int state = atomic_load(&plib->import_state);
        if (mode == STEP_INTO && state == FKL_VM_LIB_NONE) {
            FklVMproc *p = FKL_VM_PROC(plib->proc);
            ins[0] = p->spc;
        } else {
            ins[0] = f->c.pc + l;
        }
    } else if (f->retCallBack == NULL) {
        switch (f->c.mark) {
        case FKL_VM_COMPOUND_FRAME_MARK_RET:
            for (f = f->prev; f; f = f->prev) {
                if (f->type == FKL_FRAME_COMPOUND) {
                    ins[0] = f->c.pc;
                    break;
                }
            }
            if (ins[0] == NULL)
                r = 0;
            break;
        case FKL_VM_COMPOUND_FRAME_MARK_CALL:
            ins[0] = f->c.spc;
            break;
        }
    } else {
        int3_flags |= INT3_GET_NEXT_INS;
        ins[0] = f->c.pc;
    }

set_target:
    for (int i = 0; i < r; ++i) {
        uint8_t flags = int3_flags;

        set_stepping_target(&ctx->stepping_ctx,
                ins[i],
                &(SetSteppingArgs){
                    .i = i,
                    .flags = flags,
                });
    }
}

static inline void set_step_line(DebugCtx *ctx,
        FklVM *exe,
        StepTargetPlacingType placing_type,
        SteppingMode mode) {
    struct SteppingCtx *sctx = &ctx->stepping_ctx;
    if (exe == NULL || exe->top_frame == NULL)
        return;
    uint8_t int3_flags = INT3_STEPPING;
    ctx->stepping_ctx.vm = exe;

    FklVMframe *f = exe->top_frame;
    for (; f && f->type == FKL_FRAME_OTHEROBJ; f = f->prev)
        ;
    if (f == NULL)
        return;

    int r = 0;
    FklInstruction *ins[2] = { NULL, NULL };
    FklInstruction *cur_ins = f->c.pc;

    const FklLineNumberTableItem *ln = getCurLineNumberItemWithCp(cur_ins,
            FKL_VM_CO(FKL_VM_PROC(f->c.proc)->codeObj));

    if (placing_type == STEP_INS_CUR) {
        FKL_ASSERT(sctx->ln);
        if (sctx->ln->line == ln->line && sctx->ln->fid == ln->fid) {
            int3_flags |= INT3_STEP_LINE;
            int3_flags |= INT3_GET_NEXT_INS;
        }
        ins[0] = cur_ins;
        r = 1;
        goto set_target;
    }

    sctx->ln = ln;

    for (;;) {
        r = fklGetNextIns2(cur_ins, ins);
        if (r != 1)
            break;
    get_next_line_ins:
        ln = getCurLineNumberItemWithCp(ins[0],
                FKL_VM_CO(FKL_VM_PROC(f->c.proc)->codeObj));
        if (sctx->ln->line != ln->line || sctx->ln->fid != ln->fid) {
            goto set_target;
        }
        cur_ins = ins[0];
    }

    FklInstruction tmp_ins = *cur_ins;

    if (cur_ins->op == FKL_OP_DUMMY) {
        const FklInstruction *oins =
                &getBreakpointHashItem(ctx, cur_ins)->origin_ins;
        *cur_ins = *oins;
        r = fklGetNextIns2(cur_ins, ins);
        *cur_ins = tmp_ins;

        if (r == 1) {
            cur_ins = ins[0];
            goto get_next_line_ins;
        }
        tmp_ins = *oins;
    }

    if (r == 2) {
        r = 1;
        ins[0] = cur_ins;
        int3_flags |= INT3_GET_NEXT_INS;
        int3_flags |= INT3_STEP_LINE;
        goto set_target;
    }
    r = 1;
    if (fklIsCallIns(&tmp_ins)) {
        FklVMvalue *proc = exe->base[exe->bp];
        if (cur_ins != f->c.pc) {
            ins[0] = cur_ins;
            int3_flags |= INT3_GET_NEXT_INS;
            int3_flags |= INT3_STEP_LINE;
            goto set_target;
        }
        if (mode == STEP_INTO && FKL_IS_PROC(proc)) {
            FklVMproc *p = FKL_VM_PROC(proc);
            ins[0] = p->spc;
            goto set_target;
        }

        FklInstructionArg arg = { 0 };
        int8_t l = fklGetInsOpArg(cur_ins, &arg);
        ins[0] = cur_ins + l;
        cur_ins = ins[0];
        goto get_next_line_ins;

    } else if (fklIsLoadLibIns(&tmp_ins)) {
        FklInstructionArg arg = { 0 };
        int8_t l = fklGetInsOpArg(cur_ins, &arg);
        FklVMlib *plib = &exe->gc->libs[arg.ux];
        int state = atomic_load(&plib->import_state);
        if (cur_ins != f->c.pc) {
            ins[0] = cur_ins;
            int3_flags |= INT3_GET_NEXT_INS;
            int3_flags |= INT3_STEP_LINE;
            goto set_target;
        }
        if (mode == STEP_INTO && state == FKL_VM_LIB_NONE) {
            FklVMproc *p = FKL_VM_PROC(plib->proc);
            ins[0] = p->spc;
            goto set_target;
        }
        ins[0] = cur_ins + l;
        cur_ins = ins[0];
        goto get_next_line_ins;

    } else if (f->retCallBack == NULL) {
        switch (f->c.mark) {
        case FKL_VM_COMPOUND_FRAME_MARK_RET:
            for (f = f->prev; f; f = f->prev) {
                if (f->type == FKL_FRAME_COMPOUND) {
                    ins[0] = f->c.pc;
                    break;
                }
            }
            if (ins[0] == NULL)
                r = 0;
            break;
        case FKL_VM_COMPOUND_FRAME_MARK_CALL:
            ins[0] = f->c.spc;
            break;
        }
    } else {
        int3_flags |= INT3_STEP_LINE;
        int3_flags |= INT3_GET_NEXT_INS;
        ins[0] = cur_ins;
    }

set_target:
    for (int i = 0; i < r; ++i) {
        uint8_t flags = int3_flags;

        set_stepping_target(&ctx->stepping_ctx,
                ins[i],
                &(SetSteppingArgs){
                    .i = i,
                    .flags = flags,
                });
    }
}

void setStepIns(DebugCtx *ctx,
        FklVM *exe,
        StepTargetPlacingType placing_type,
        SteppingMode mode,
        SteppingType type) {
    switch (type) {
    case STEP_INS:
        set_step_ins(ctx, exe, placing_type, mode);
        break;
    case STEP_LINE:
        set_step_line(ctx, exe, placing_type, mode);
        break;
    }
}

void setStepOut(DebugCtx *ctx) {
    FklVM *exe = ctx->reached_thread;

    if (exe == NULL || exe->top_frame == NULL)
        return;
    ctx->stepping_ctx.vm = exe;
    FklVMframe *f = exe->top_frame;
    for (; f && f->type == FKL_FRAME_OTHEROBJ; f = f->prev)
        ;
    if (f == NULL)
        return;
    for (f = f->prev; f; f = f->prev) {
        if (f->type == FKL_FRAME_COMPOUND)
            break;
    }

    if (f == NULL)
        return;

    FklInstruction *ins = f->c.pc;

    set_stepping_target(&ctx->stepping_ctx,
            ins,
            &(SetSteppingArgs){ .i = 0, .flags = INT3_STEPPING });
}

void setStepUntil(DebugCtx *ctx, uint32_t target_line) {
    FklVM *exe = ctx->reached_thread;

    if (exe == NULL || exe->top_frame == NULL)
        return;

    ctx->stepping_ctx.vm = exe;
    PutBreakpointErrorType err = 0;
    FklInstruction *target =
            getInsWithFileAndLine(ctx, ctx->curline_file, target_line, &err);
    if (target == NULL)
        return;

    set_stepping_target(&ctx->stepping_ctx,
            target,
            &(SetSteppingArgs){ .i = 0, .flags = INT3_STEPPING });
}

void unsetStepping(DebugCtx *ctx) {
    struct SteppingCtx *sctx = &ctx->stepping_ctx;
    sctx->ln = NULL;
    for (size_t i = 0; i < BDB_STEPPING_TARGET_INS_COUNT; ++i) {
        if (sctx->target_ins[i]) {
            *(sctx->target_ins[i]) = sctx->ins[i];
            sctx->target_ins[i] = NULL;
        }
    }
    memset(&sctx->ins[0], 0xff, sizeof(sctx->ins));

    if (sctx->vm) {
        sctx->vm = NULL;
    }
}
