#include "bdb.h"

#include <fakeLisp/base.h>

static FKL_HASH_SET_KEY_VAL(breakpoint_ins_set_val, BreakpointInsHashItem);

static uintptr_t breakpoint_ins_hash_func(const void *k) {
    return ((uintptr_t)*(FklInstruction *const *)k) >> (sizeof(FklInstruction));
}

static const FklHashTableMetaTable BreakpointInsHashMetaTable = {
    .size = sizeof(BreakpointInsHashItem),
    .__getKey = fklHashDefaultGetKey,
    .__setKey = fklPtrKeySet,
    .__setVal = breakpoint_ins_set_val,
    .__hashFunc = breakpoint_ins_hash_func,
    .__keyEqual = fklPtrKeyEqual,
    .__uninitItem = fklDoNothingUninitHashItem,
};

static FKL_HASH_SET_KEY_VAL(breakpoint_idx_set_val, BreakpointIdxHashItem);
static FKL_HASH_SET_KEY_VAL(breakpoint_idx_set_key, uint32_t);

static uintptr_t breakpoint_idx_hash_func(const void *k) {
    return *(const uint32_t *)k;
}

static int breakpoint_idx_key_equal(const void *a, const void *b) {
    return *(const uint32_t *)a == *(const uint32_t *)b;
}

static const FklHashTableMetaTable BreakpointIdxHashMetaTable = {
    .size = sizeof(BreakpointIdxHashItem),
    .__getKey = fklHashDefaultGetKey,
    .__setKey = breakpoint_idx_set_key,
    .__setVal = breakpoint_idx_set_val,
    .__hashFunc = breakpoint_idx_hash_func,
    .__keyEqual = breakpoint_idx_key_equal,
    .__uninitItem = fklDoNothingUninitHashItem,
};

void initBreakpointTable(BreakpointTable *bt) {
    fklInitHashTable(&bt->ins_ht, &BreakpointInsHashMetaTable);
    fklInitHashTable(&bt->idx_ht, &BreakpointIdxHashMetaTable);
    fklInitUintStack(&bt->unused_prototype_id_for_cond_bp, 16, 16);
    bt->next_idx = 1;
    bt->deleted_breakpoints = NULL;
}

static inline void mark_breakpoint_deleted(Breakpoint *bp,
                                           BreakpointTable *bt) {
    if (bp->is_compiled && bp->proc) {
        FklVMproc *proc = FKL_VM_PROC(bp->proc);
        fklPushUintStack(proc->protoId, &bt->unused_prototype_id_for_cond_bp);
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
    BreakpointInsHashItem *ins_item = bp->item;
    *(bp->pnext) = bp->next;
    if (bp->next)
        bp->next->pnext = bp->pnext;
    bp->pnext = NULL;

    if (ins_item->bp == NULL) {
        FklInstruction *ins = ins_item->ins;
        ins->op = ins_item->origin_op;
        fklDelHashItem(&ins, &bt->ins_ht, NULL);
    }
}

static inline void clear_breakpoint(BreakpointTable *bt) {
    for (FklHashTableItem *l = bt->idx_ht.first; l; l = l->next) {
        Breakpoint *bp = ((BreakpointIdxHashItem *)l->data)->bp;
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
    fklUninitHashTable(&bt->ins_ht);
    fklUninitHashTable(&bt->idx_ht);
    destroy_all_deleted_breakpoint(bt);
    fklUninitUintStack(&bt->unused_prototype_id_for_cond_bp);
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
    BreakpointIdxHashItem *item = fklGetHashItem(&idx, &dctx->bt.idx_ht);
    if (item)
        return item->bp;
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
    BreakpointIdxHashItem item = {
        .idx = 0,
        .bp = NULL,
    };
    BreakpointTable *bt = &dctx->bt;
    if (fklDelHashItem(&idx, &bt->idx_ht, &item)) {
        Breakpoint *bp = item.bp;
        mark_breakpoint_deleted(bp, bt);
        return bp;
    }
    return NULL;
}

void markBreakpointCondExpObj(DebugCtx *dctx, FklVMgc *gc) {
    BreakpointTable *bt = &dctx->bt;
    for (FklHashTableItem *list = bt->idx_ht.first; list; list = list->next) {
        Breakpoint *bp = ((BreakpointIdxHashItem *)list->data)->bp;
        if (bp->cond_exp_obj)
            fklVMgcToGray(bp->cond_exp_obj, gc);
    }
    for (Breakpoint *bp = bt->deleted_breakpoints; bp; bp = bp->next) {
        if (bp->cond_exp_obj)
            fklVMgcToGray(bp->cond_exp_obj, gc);
    }
}

static inline Breakpoint *make_breakpoint(BreakpointInsHashItem *item,
                                          BreakpointTable *bt, FklSid_t fid,
                                          uint32_t line) {
    Breakpoint *bp = (Breakpoint *)calloc(1, sizeof(Breakpoint));
    FKL_ASSERT(bp);
    bp->fid = fid;
    bp->line = line;
    bp->idx = bt->next_idx++;
    bp->item = item;
    if (item->bp)
        (item->bp)->pnext = &bp->next;
    bp->pnext = &item->bp;
    bp->next = item->bp;
    item->bp = bp;
    BreakpointIdxHashItem *idx_item = fklPutHashItem(&bp->idx, &bt->idx_ht);
    idx_item->bp = bp;
    return bp;
}

Breakpoint *createBreakpoint(FklSid_t fid, uint32_t line, FklInstruction *ins,
                             DebugCtx *ctx) {
    BreakpointTable *bt = &ctx->bt;
    BreakpointInsHashItem tmp_item = {
        .ins = ins,
        .origin_op = ins->op,
        .bp = NULL,
    };
    BreakpointInsHashItem *item = fklGetOrPutHashItem(&tmp_item, &bt->ins_ht);
    item->bp = make_breakpoint(item, bt, fid, line);
    ins->op = FKL_OP_DUMMY;
    return item->bp;
}

BreakpointInsHashItem *getBreakpointHashItem(DebugCtx *dctx,
                                             const FklInstruction *ins) {
    return fklGetHashItem(&ins, &dctx->bt.ins_ht);
}
