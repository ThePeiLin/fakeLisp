#include "bdb.h"

#include <fakeLisp/base.h>
#include <fakeLisp/zmalloc.h>
#include <string.h>

void initBreakpointTable(BdbBpTable *bt) {
    bdbBpInsHashMapInit(&bt->ins_ht);
    bdbBpIdxHashMapInit(&bt->idx_ht);
    bt->next_idx = 1;
    bt->deleted_breakpoints = NULL;
}

static inline void mark_breakpoint_deleted(BdbBp *bp, BdbBpTable *bt) {
    bp->cond_exp = BDB_NONE;
    bp->cond_proc = BDB_NONE;
    bp->filename = BDB_NONE;

    bp->is_errored = 0;
    bp->is_deleted = 1;

    bp->next_del = bt->deleted_breakpoints;
    bt->deleted_breakpoints = bp;
}

static inline void remove_breakpoint(BdbBp *bp, BdbBpTable *bt) {
    BdbBpInsHashMapElm *ins_item = bp->item;
    *(bp->pnext) = bp->next;
    if (bp->next)
        bp->next->pnext = bp->pnext;
    bp->pnext = NULL;

    if (ins_item->v.bp == NULL) {
        const FklIns *ins = ins_item->k;
        assign_ins(ins, &ins_item->v.origin_ins);
        // *ins = ins_item->v.origin_ins;
        bdbBpInsHashMapDel2(&bt->ins_ht, ins);
    }
}

static inline void clear_breakpoint(BdbBpTable *bt) {
    for (BdbBpIdxHashMapNode *l = bt->idx_ht.first; l; l = l->next) {
        BdbBp *bp = l->v;
        mark_breakpoint_deleted(bp, bt);
        remove_breakpoint(bp, bt);
    }
}

static inline void destroy_all_deleted_breakpoint(BdbBpTable *bt) {
    BdbBp *bp = bt->deleted_breakpoints;
    while (bp) {
        BdbBp *cur = bp;
        bp = bp->next_del;
        cur->cond_exp = BDB_NONE;
        cur->cond_proc = BDB_NONE;
        cur->filename = BDB_NONE;
        fklZfree(cur);
    }
    bt->deleted_breakpoints = NULL;
}

void uninitBreakpointTable(BdbBpTable *bt) {
    clear_breakpoint(bt);
    bdbBpInsHashMapUninit(&bt->ins_ht);
    bdbBpIdxHashMapUninit(&bt->idx_ht);
    destroy_all_deleted_breakpoint(bt);
}

#include <fakeLisp/common.h>

FKL_VM_USER_DATA_DEFAULT_PRINT(bp_wrapper_print, "breakpoint-wrapper");

static FklVMudMetaTable BreakpointWrapperMetaTable = {
    .size = sizeof(FklVMvalueBpWrapper),
    .prin1 = bp_wrapper_print,
    .princ = bp_wrapper_print,
};

FklVMvalueBpWrapper *createBpWrapper(FklVM *vm, BdbBp *bp) {
    FklVMvalueBpWrapper *r = (FklVMvalueBpWrapper *)fklCreateVMvalueUd(vm,
            &BreakpointWrapperMetaTable,
            NULL);

    r->bp = bp;
    return r;
}

int isBpWrapper(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v)
        && FKL_VM_UD(v)->mt_ == &BreakpointWrapperMetaTable;
}

static FKL_ALWAYS_INLINE FklVMvalueBpWrapper *as_bp_wrapper(
        const FklVMvalue *v) {
    FKL_ASSERT(isBpWrapper(v));
    return (FklVMvalueBpWrapper *)v;
}

BdbBp *getBp(const FklVMvalue *v) { return as_bp_wrapper(v)->bp; }

BdbBp *getBreakpoint(DebugCtx *dctx, uint32_t idx) {
    BdbBp **i = bdbBpIdxHashMapGet2(&dctx->bt.idx_ht, idx);
    if (i)
        return *i;
    return NULL;
}

BdbBp *disBreakpoint(DebugCtx *dctx, uint32_t idx) {
    BdbBp *bp = getBreakpoint(dctx, idx);
    if (bp) {
        bp->is_disabled = 1;
        return bp;
    }
    return NULL;
}

BdbBp *enableBreakpoint(DebugCtx *dctx, uint32_t idx) {
    BdbBp *bp = getBreakpoint(dctx, idx);
    if (bp) {
        bp->is_disabled = 0;
        return bp;
    }
    return NULL;
}

void clearDeletedBreakpoint(DebugCtx *dctx) {
    BdbBpTable *bt = &dctx->bt;
    BdbBp **phead = &bt->deleted_breakpoints;
    while (*phead) {
        BdbBp *cur = *phead;
        if (atomic_load(&cur->reached_count))
            phead = &cur->next_del;
        else {
            remove_breakpoint(cur, bt);
            *phead = cur->next_del;
            cur->cond_exp = BDB_NONE;
            cur->cond_proc = BDB_NONE;
            cur->filename = BDB_NONE;
            fklZfree(cur);
        }
    }
}

BdbBp *delBreakpoint(DebugCtx *dctx, uint32_t idx) {
    BdbBp *bp = NULL;
    BdbBpTable *bt = &dctx->bt;
    if (bdbBpIdxHashMapErase(&bt->idx_ht, &idx, &bp, NULL)) {
        mark_breakpoint_deleted(bp, bt);
        return bp;
    }
    return NULL;
}

BdbCodepoint *getBreakpointHashItem(DebugCtx *dctx, const FklIns *ins) {
    return bdbBpInsHashMapGet2(&dctx->bt.ins_ht, FKL_TYPE_CAST(FklIns *, ins));
}

FklVMvalue *bdbCreateBpVec(FklVM *exe, DebugCtx *dctx, const BdbBp *bp) {
    BdbPos pos = bdbBpPos(bp);
    FklVMvalue *filename = fklCreateVMvalueStr(exe, pos.filename);
    FklVMvalue *line = FKL_MAKE_VM_FIX(pos.line);
    FklVMvalue *num = FKL_MAKE_VM_FIX(bp->idx);

    FklVMvalue *exp_str = FKL_VM_NIL;
    if (bdbHas(bp->cond_exp)) {
        FklStrBuf buf = { 0 };
        fklInitStrBuf(&buf);
        FklCodeBuilder builder = { 0 };
        fklInitCodeBuilderStrBuf(&builder, &buf, NULL);
        fklPrin1VMvalue2(bdbUnwrap(bp->cond_exp), &builder, &dctx->gc.gcvm);
        exp_str = fklCreateVMvalueStr2(exe, buf.index, buf.buf);
        fklUninitStrBuf(&buf);
    }

    FklVMvalue *r = fklCreateVMvalueVecExt(exe,
            6,
            num,
            filename,
            line,
            exp_str,
            fklMakeVMuint(bp->count, exe),
            fklCreateVMvaluePair(exe,
                    bp->is_disabled ? FKL_VM_NIL : FKL_VM_TRUE,
                    bp->is_temporary ? FKL_VM_TRUE : FKL_VM_NIL));
    return r;
}

BdbPos bdbBpPos(const BdbBp *bp) {
    BdbPos pos = { 0 };
    pos.filename = FKL_VM_SYM(bdbUnwrap(bp->filename));
    pos.line = bp->line;
    pos.str = NULL;

    return pos;
}
