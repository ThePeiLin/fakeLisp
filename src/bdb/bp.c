#include "bdb.h"

#include <fakeLisp/base.h>

void initBreakpointTable(BreakpointTable *bt) {
    bdbBpInsTableInit(&bt->ins_ht);
    bdbBpIdxTableInit(&bt->idx_ht);
    fklUintVectorInit(&bt->unused_prototype_id_for_cond_bp, 16);
    bt->next_idx = 1;
    bt->deleted_breakpoints = NULL;
}

static inline void mark_breakpoint_deleted(Breakpoint *bp,
                                           BreakpointTable *bt) {
    if (bp->is_compiled && bp->proc) {
        FklVMproc *proc = FKL_VM_PROC(bp->proc);
        fklUintVectorPushBack2(&bt->unused_prototype_id_for_cond_bp,
                               proc->protoId);
        bp->proc = NULL;
    } else if (bp->cond_exp) {
        fklDestroyNastNode(bp->cond_exp);
        bp->cond_exp = NULL;
    }

    bp->is_compiled = 1;
    bp->is_deleted = 1;

    bp->next_del = bt->deleted_breakpoints;
    bt->deleted_breakpoints = bp;
}

static inline void remove_breakpoint(Breakpoint *bp, BreakpointTable *bt) {
    BdbBpInsTableElm *ins_item = bp->item;
    *(bp->pnext) = bp->next;
    if (bp->next)
        bp->next->pnext = bp->pnext;
    bp->pnext = NULL;

    if (ins_item->v.bp == NULL) {
        FklInstruction *ins = ins_item->k;
        ins->op = ins_item->v.origin_op;
        bdbBpInsTableDel2(&bt->ins_ht, ins);
    }
}

static inline void clear_breakpoint(BreakpointTable *bt) {
    for (BdbBpIdxTableNode *l = bt->idx_ht.first; l; l = l->next) {
        Breakpoint *bp = l->v;
        mark_breakpoint_deleted(bp, bt);
        remove_breakpoint(bp, bt);
    }
}

static inline void destroy_all_deleted_breakpoint(BreakpointTable *bt) {
    Breakpoint *bp = bt->deleted_breakpoints;
    while (bp) {
        Breakpoint *cur = bp;
        bp = bp->next;
        if (cur->cond_exp)
            fklDestroyNastNode(cur->cond_exp);
        free(cur);
    }
}

void uninitBreakpointTable(BreakpointTable *bt) {
    clear_breakpoint(bt);
    bdbBpInsTableUninit(&bt->ins_ht);
    bdbBpIdxTableUninit(&bt->idx_ht);
    destroy_all_deleted_breakpoint(bt);
    fklUintVectorUninit(&bt->unused_prototype_id_for_cond_bp);
}

#include <fakeLisp/common.h>

static FklVMudMetaTable BreakpointWrapperMetaTable = {
    .size = sizeof(BreakpointWrapper),
};

FklVMvalue *createBreakpointWrapper(FklVM *vm, Breakpoint *bp) {
    FklVMvalue *r = fklCreateVMvalueUd(vm, &BreakpointWrapperMetaTable, NULL);
    FKL_DECL_VM_UD_DATA(bpw, BreakpointWrapper, r);
    bpw->bp = bp;
    return r;
}

int isBreakpointWrapper(FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->t == &BreakpointWrapperMetaTable;
}

Breakpoint *getBreakpointFromWrapper(FklVMvalue *v) {
    FKL_DECL_VM_UD_DATA(bpw, BreakpointWrapper, v);
    return bpw->bp;
}

Breakpoint *getBreakpointWithIdx(DebugCtx *dctx, uint32_t idx) {
    Breakpoint **i = bdbBpIdxTableGet2(&dctx->bt.idx_ht, idx);
    if (i)
        return *i;
    return NULL;
}

Breakpoint *disBreakpoint(DebugCtx *dctx, uint32_t idx) {
    Breakpoint *bp = getBreakpointWithIdx(dctx, idx);
    if (bp) {
        bp->is_disabled = 1;
        return bp;
    }
    return NULL;
}

Breakpoint *enableBreakpoint(DebugCtx *dctx, uint32_t idx) {
    Breakpoint *bp = getBreakpointWithIdx(dctx, idx);
    if (bp) {
        bp->is_disabled = 0;
        return bp;
    }
    return NULL;
}

void clearDeletedBreakpoint(DebugCtx *dctx) {
    BreakpointTable *bt = &dctx->bt;
    Breakpoint **phead = &bt->deleted_breakpoints;
    while (*phead) {
        Breakpoint *cur = *phead;
        if (atomic_load(&cur->reached_count))
            phead = &cur->next_del;
        else {
            remove_breakpoint(cur, bt);
            *phead = cur->next_del;
            if (cur->cond_exp)
                fklDestroyNastNode(cur->cond_exp);
            free(cur);
        }
    }
}

Breakpoint *delBreakpoint(DebugCtx *dctx, uint32_t idx) {
    Breakpoint *bp = NULL;
    BreakpointTable *bt = &dctx->bt;
    if (bdbBpIdxTableEarase(&bt->idx_ht, &idx, &bp, NULL)) {
        mark_breakpoint_deleted(bp, bt);
        return bp;
    }
    return NULL;
}

void markBreakpointCondExpObj(DebugCtx *dctx, FklVMgc *gc) {
    BreakpointTable *bt = &dctx->bt;
    for (BdbBpIdxTableNode *list = bt->idx_ht.first; list; list = list->next) {
        Breakpoint *bp = list->v;
        if (bp->cond_exp_obj)
            fklVMgcToGray(bp->cond_exp_obj, gc);
    }
    for (Breakpoint *bp = bt->deleted_breakpoints; bp; bp = bp->next) {
        if (bp->cond_exp_obj)
            fklVMgcToGray(bp->cond_exp_obj, gc);
    }
}

static inline Breakpoint *make_breakpoint(BdbBpInsTableElm *item,
                                          BreakpointTable *bt, FklSid_t fid,
                                          uint32_t line) {
    Breakpoint *bp = (Breakpoint *)calloc(1, sizeof(Breakpoint));
    FKL_ASSERT(bp);
    bp->fid = fid;
    bp->line = line;
    bp->idx = bt->next_idx++;
    bp->item = item;
    if (item->v.bp)
        (item->v.bp)->pnext = &bp->next;
    bp->pnext = &item->v.bp;
    bp->next = item->v.bp;
    item->v.bp = bp;
    bdbBpIdxTableAdd2(&bt->idx_ht, bp->idx, bp);
    return bp;
}

Breakpoint *createBreakpoint(FklSid_t fid, uint32_t line, FklInstruction *ins,
                             DebugCtx *ctx) {
    BreakpointTable *bt = &ctx->bt;
    BdbBpInsTableElm *item = bdbBpInsTableInsert2(
        &bt->ins_ht, ins, (BdbCodepoint){.origin_op = ins->op, .bp = NULL});
    item->v.bp = make_breakpoint(item, bt, fid, line);
    ins->op = FKL_OP_DUMMY;
    return item->v.bp;
}

BdbCodepoint *getBreakpointHashItem(DebugCtx *dctx, const FklInstruction *ins) {
    return bdbBpInsTableGet2(&dctx->bt.ins_ht,
                             FKL_REMOVE_CONST(FklInstruction, ins));
}
