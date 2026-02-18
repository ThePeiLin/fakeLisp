#include "bdb.h"

#include <fakeLisp/bytecode.h>
#include <fakeLisp/symbol.h>

typedef struct {
    size_t i;
    uint8_t flags;
} SetSteppingArgs;

static inline void set_stepping_target(struct SteppingCtx *stepping_ctx,
        const FklIns *target,
        const SetSteppingArgs *args) {
    stepping_ctx->ins[args->i] = *target;
    stepping_ctx->target_ins[args->i] = target;
    FklOpcode op = target->op;
    uint8_t flags = args->flags;
    if (op == FKL_OP_DUMMY)
        flags |= INT3_STEP_AT_BP;

    assign_ins(target, &(FklIns){ .op = FKL_OP_DUMMY, .au = flags });
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
    const FklIns *ins[2] = { NULL, NULL };
    if (placing_type == STEP_INS_CUR) {
        ins[0] = f->pc;
        r = 1;
        goto set_target;
    }

    r = fklGetNextIns(f->pc, ins);
    if (r != 0)
        goto set_target;

    r = 1;
    if (fklIsCallIns(f->pc)) {
        FklVMvalue *proc = exe->base[exe->bp];
        if (mode == STEP_INTO && FKL_IS_PROC(proc)) {
            FklVMvalueProc *p = FKL_VM_PROC(proc);
            ins[0] = p->spc;
        } else {
            FklInsArg arg = { 0 };
            int8_t l = fklGetInsOpArg(f->pc, &arg);
            ins[0] = f->pc + l;
        }

    } else if (fklIsLoadLibIns(f->pc)) {
        FklInsArg arg = { 0 };
        int8_t l = fklGetInsOpArg(f->pc, &arg);
        FklVMvalueProto *proto = FKL_VM_PROC(f->proc)->proto;
        FklVMvalueLib *lib = fklVMvalueProtoUsedLibs(proto)[arg.ux];

        int state = atomic_load(&lib->import_state);
        if (mode == STEP_INTO && state == FKL_VM_LIB_NONE) {
            FklVMvalueProc *p = FKL_VM_PROC(lib->proc);
            ins[0] = p->spc;
        } else {
            ins[0] = f->pc + l;
        }
    } else if (f->ret_cb == NULL) {
        switch (f->mark) {
        case FKL_VM_COMPOUND_FRAME_MARK_RET:
            for (f = f->prev; f; f = f->prev) {
                if (f->type == FKL_FRAME_COMPOUND) {
                    ins[0] = f->pc;
                    break;
                }
            }
            if (ins[0] == NULL)
                r = 0;
            break;
        case FKL_VM_COMPOUND_FRAME_MARK_CALL:
            ins[0] = f->spc;
            break;
        }
    } else {
        int3_flags |= INT3_GET_NEXT_INS;
        ins[0] = f->pc;
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
    const FklIns *ins[2] = { NULL, NULL };
    const FklIns *cur_ins = f->pc;

    FklByteCodelnt *bcl = FKL_VM_CO(FKL_VM_PROC(f->proc)->bcl);
    const FklLntItem *ln = fklGetLntItem(bcl, cur_ins);

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
        r = fklGetNextIns(cur_ins, ins);
        if (r != 1)
            break;
    get_next_line_ins:
        ln = fklGetLntItem(FKL_VM_CO(FKL_VM_PROC(f->proc)->bcl), ins[0]);
        if (sctx->ln->line != ln->line || sctx->ln->fid != ln->fid) {
            goto set_target;
        }
        cur_ins = ins[0];
    }

    FklIns tmp_ins = *cur_ins;

    if (cur_ins->op == FKL_OP_DUMMY) {
        const FklIns *oins = &getBreakpointHashItem(ctx, cur_ins)->origin_ins;
        assign_ins(cur_ins, oins);

        r = fklGetNextIns(cur_ins, ins);
        assign_ins(cur_ins, &tmp_ins);

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
        if (cur_ins != f->pc) {
            ins[0] = cur_ins;
            int3_flags |= INT3_GET_NEXT_INS;
            int3_flags |= INT3_STEP_LINE;
            goto set_target;
        }
        if (mode == STEP_INTO && FKL_IS_PROC(proc)) {
            FklVMvalueProc *p = FKL_VM_PROC(proc);
            ins[0] = p->spc;
            goto set_target;
        }

        FklInsArg arg = { 0 };
        int8_t l = fklGetInsOpArg(cur_ins, &arg);
        ins[0] = cur_ins + l;
        cur_ins = ins[0];
        goto get_next_line_ins;

    } else if (fklIsLoadLibIns(&tmp_ins)) {
        FklInsArg arg = { 0 };
        int8_t l = fklGetInsOpArg(cur_ins, &arg);
        FklVMvalueProto *proto = FKL_VM_PROC(f->proc)->proto;
        FklVMvalueLib *lib = fklVMvalueProtoUsedLibs(proto)[arg.ux];

        int state = atomic_load(&lib->import_state);
        if (cur_ins != f->pc) {
            ins[0] = cur_ins;
            int3_flags |= INT3_GET_NEXT_INS;
            int3_flags |= INT3_STEP_LINE;
            goto set_target;
        }
        if (mode == STEP_INTO && state == FKL_VM_LIB_NONE) {
            FklVMvalueProc *p = FKL_VM_PROC(lib->proc);
            ins[0] = p->spc;
            goto set_target;
        }
        ins[0] = cur_ins + l;
        cur_ins = ins[0];
        goto get_next_line_ins;

    } else if (f->ret_cb == NULL) {
        switch (f->mark) {
        case FKL_VM_COMPOUND_FRAME_MARK_RET:
            for (f = f->prev; f; f = f->prev) {
                if (f->type == FKL_FRAME_COMPOUND) {
                    ins[0] = f->pc;
                    break;
                }
            }
            if (ins[0] == NULL)
                r = 0;
            break;
        case FKL_VM_COMPOUND_FRAME_MARK_CALL:
            ins[0] = f->spc;
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

    const FklIns *ins = f->pc;

    set_stepping_target(&ctx->stepping_ctx,
            ins,
            &(SetSteppingArgs){ .i = 0, .flags = INT3_STEPPING });
}

void setStepUntil(DebugCtx *ctx, uint32_t target_line) {
    FklVM *exe = ctx->reached_thread;

    if (exe == NULL || exe->top_frame == NULL)
        return;

    ctx->stepping_ctx.vm = exe;
    BdbPutBpErrorType err = 0;
    const FklString *s = bdbSymbol(ctx->curfile_lines->k);
    const FklIns *target = getIns2(ctx, s, target_line, &err);
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
            assign_ins(sctx->target_ins[i], &sctx->ins[i]);
            sctx->target_ins[i] = NULL;
        }
    }
    memset(&sctx->ins[0], 0xff, sizeof(sctx->ins));

    if (sctx->vm) {
        sctx->vm = NULL;
    }
}
