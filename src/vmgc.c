#include <fakeLisp/builtin.h>
#include <fakeLisp/vm.h>
#include <fakeLisp/zmalloc.h>
#include <uv.h>

#include <string.h>

void fklVMgcToGray(FklVMvalue *v, FklVMgc *gc) {
    if (v && FKL_IS_PTR(v) && v->mark < FKL_MARK_G) {
        v->mark = FKL_MARK_G;
        v->gray_next = gc->gray_list;
        gc->gray_list = v;
    }
}

static inline void mark_atexit(FklVM *vm) {
    FklVMgc *gc = vm->gc;
    for (struct FklVMatExit *c = vm->atexit; c; c = c->next)
        if (c->mark)
            c->mark(c->arg, gc);
}

static inline void mark_interrupt_handler(FklVMgc *gc,
        struct FklVMinterruptHandleList *l) {
    for (; l; l = l->next)
        if (l->mark)
            l->mark(l->int_handle_arg, gc);
}

static inline void remove_closed_var_ref_from_list(FklVMvarRefList **ll) {
    while (*ll) {
        FklVMvarRefList *l = *ll;
        FklVMvalue *ref = l->ref;
        if (fklIsClosedVMvalueVarRef(ref)) {
            *ll = l->next;
            fklZfree(l);
        } else
            ll = &l->next;
    }
}

static inline void remove_closed_var_ref(FklVM *exe) {
    for (FklVMframe *f = exe->top_frame; f; f = f->prev)
        if (f->type == FKL_FRAME_COMPOUND)
            remove_closed_var_ref_from_list(&f->c.lr.lrefl);
}

static inline void do_atomic_frame(FklVMframe *f, FklVMgc *gc) {
    switch (f->type) {
    case FKL_FRAME_COMPOUND:
        for (FklVMvarRefList *l = fklGetCompoundFrameLocRef(f)->lrefl; l;
                l = l->next)
            fklVMgcToGray(l->ref, gc);
        fklVMgcToGray(fklGetCompoundFrameProc(f), gc);
        break;
    case FKL_FRAME_OTHEROBJ:
        if (f->t->atomic)
            f->t->atomic(fklGetFrameData(f), gc);
        break;
    }
}

static inline void gc_mark_root_to_gray(FklVM *exe) {
    remove_closed_var_ref(exe);
    mark_atexit(exe);
    mark_interrupt_handler(exe->gc, exe->int_list);
    FklVMgc *gc = exe->gc;

    for (FklVMframe *cur = exe->top_frame; cur; cur = cur->prev)
        do_atomic_frame(cur, gc);

    FklVMvalue **base = exe->base;
    for (uint32_t i = 0; i < exe->tp; i++)
        fklVMgcToGray(base[i], gc);
    fklVMgcToGray(exe->chan, gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, exe->libs), gc);
    fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, exe->pts), gc);
}

static inline void mark_obarray(FklVMgc *gc, FklVMobarray *a) {
    uv_mutex_lock(&a->lock);
    for (const FklStrValueHashMapNode *cur = a->map.first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->v, gc);
    }
    uv_mutex_unlock(&a->lock);
}

void fklVMgcMarkPrototypes(FklVMgc *gc, const FklFuncPrototypes *pts) {
    for (size_t i = 0; i < pts->count; ++i) {
        FklFuncPrototype *pt = &pts->pa[i + 1];
        fklVMgcToGray(pt->file, gc);
        fklVMgcToGray(pt->name, gc);
        for (size_t j = 0; j < pt->konsts_count; ++j)
            fklVMgcToGray(pt->konsts[j], gc);
        for (size_t j = 0; j < pt->rcount; ++j)
            fklVMgcToGray(pt->refs[j].k.id, gc);
    }
}

void fklVMgcMarkCodeObject(FklVMgc *gc, const FklByteCodelnt *t) {
    for (uint32_t i = 0; i < t->ls; ++i)
        fklVMgcToGray(t->l[i].fid, gc);
}

static void mark_nonterm(FklVMgc *gc, const FklGrammerNonterm *nt) {
    fklVMgcToGray(nt->group, gc);
    fklVMgcToGray(nt->sid, gc);
}

void fklVMgcMarkGrammerProd(FklVMgc *gc,
        const FklGrammerProduction *prod,
        void (*ctx_atomic)(FklVMgc *gc, void *ctx)) {

    mark_nonterm(gc, &prod->left);
    const FklGrammerSym *cur = prod->syms;
    const FklGrammerSym *const end = cur + prod->len;
    for (; cur < end; ++cur) {
        switch (cur->type) {
        case FKL_TERM_NONTERM:
            mark_nonterm(gc, &cur->nt);
            break;
        default:
            break;
        }
    }

    if (ctx_atomic) {
        ctx_atomic(prod->ctx, gc);
    } else if (ctx_atomic != FKL_VM_GRAMMER_CTX_MARKER_NONE) {
        fklVMgcToGray(FKL_TYPE_CAST(FklVMvalue *, prod->ctx), gc);
    }
}

void fklVMgcMarkGrammer(FklVMgc *gc,
        const FklGrammer *g,
        void (*ctx_atomic)(FklVMgc *, void *)) {
    for (const FklGraSidBuiltinHashMapNode *cur = g->builtins.first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->k, gc);
    }

    mark_nonterm(gc, &g->start);

    for (const FklProdHashMapNode *cur = g->productions.first; cur;
            cur = cur->next) {
        mark_nonterm(gc, &cur->k);
        for (const FklGrammerProduction *prod = cur->v; prod;
                prod = prod->next) {
            fklVMgcMarkGrammerProd(gc, prod, ctx_atomic);
        }
    }

    for (const FklGraSidBuiltinHashMapNode *cur = g->builtins.first; cur;
            cur = cur->next) {
        fklVMgcToGray(cur->k, gc);
    }
}

static inline void gc_extra_mark(FklVMgc *gc) {
    for (struct FklVMextraMarkObjList *l = gc->extra_mark_list; l; l = l->next)
        l->func(gc, l->arg);
    mark_obarray(gc, gc->obarray);

    for (size_t i = 0; i < FKL_BUILTIN_ERR_NUM; ++i) {
        fklVMgcToGray(gc->builtinErrorTypeId[i], gc);
    }

    fklVMgcToGray(gc->seek_set, gc);
    fklVMgcToGray(gc->seek_cur, gc);
    fklVMgcToGray(gc->seek_end, gc);

    fklVMgcToGray(gc->sym_quote, gc);
    fklVMgcToGray(gc->sym_unquote, gc);
    fklVMgcToGray(gc->sym_qsquote, gc);
    fklVMgcToGray(gc->sym_unquote, gc);
}

void fklVMgcMarkAllRootToGray(FklVM *curVM) {
    FklVMgc *gc = curVM->gc;
    FklVMvalue **ref = curVM->gc->builtin_refs;
    FklVMvalue **end = &ref[FKL_BUILTIN_SYMBOL_NUM];
    for (; ref < end; ref++)
        fklVMgcToGray(*ref, gc);
    gc_extra_mark(curVM->gc);
    mark_interrupt_handler(curVM->gc, curVM->gc->int_list);
    gc_mark_root_to_gray(curVM);

    for (FklVM *cur = curVM->next; cur != curVM; cur = cur->next)
        gc_mark_root_to_gray(cur);
}

static void atomic_var_ref(FklVMvalue *ref, FklVMgc *gc) {
    fklVMgcToGray(*(FKL_VM_VAR_REF_GET(ref)), gc);
}

static inline void propagateMark(FklVMvalue *root, FklVMgc *gc) {
    FKL_ASSERT(root->type < FKL_VM_VALUE_GC_TYPE_NUM);
    static void (*const
                    fkl_atomic_value_method_table[FKL_VM_VALUE_GC_TYPE_NUM])(
            FklVMvalue *,
            FklVMgc *) = {
        [FKL_TYPE_VECTOR] = fklAtomicVMvec,
        [FKL_TYPE_PAIR] = fklAtomicVMpair,
        [FKL_TYPE_BOX] = fklAtomicVMbox,
        [FKL_TYPE_USERDATA] = fklAtomicVMuserdata,
        [FKL_TYPE_PROC] = fklAtomicVMproc,
        [FKL_TYPE_CPROC] = fklAtomicVMcproc,
        [FKL_TYPE_HASHTABLE] = fklAtomicVMhashTable,
        [FKL_TYPE_VAR_REF] = atomic_var_ref,
    };
    void (*atomic_value_func)(FklVMvalue *, FklVMgc *) =
            fkl_atomic_value_method_table[root->type];
    if (atomic_value_func)
        atomic_value_func(root, gc);
}

int fklVMgcPropagate(FklVMgc *gc) {
    FklVMvalue *v = gc->gray_list;
    if (v) {
        gc->gray_list = v->gray_next;
        v->gray_next = NULL;
        if (v->mark == FKL_MARK_G) {
            v->mark = FKL_MARK_B;
            propagateMark(v, gc);
        }
    }
    return gc->gray_list == NULL;
}

void fklVMgcCollect(FklVMgc *gc, FklVMvalue **pw) {
    FklVMvalue *head = gc->head;
    gc->head = NULL;
    gc->running = FKL_GC_SWEEPING;
    FklVMvalue **phead = &head;
    while (*phead) {
        FklVMvalue *cur = *phead;
        if (cur->mark == FKL_MARK_W) {
            *phead = cur->next;
            cur->next = *pw;
            *pw = cur;
        } else {
            cur->mark = FKL_MARK_W;
            phead = &cur->next;
        }
    }
    *phead = gc->head;
    gc->head = head;
}

static void destroy_vm_value(FklVMgc *gc, FklVMvalue *cur) {
    switch (cur->type) {
    case FKL_TYPE_USERDATA:
        if (fklFinalizeVMud(FKL_VM_UD(cur)) == FKL_VM_UD_FINALIZE_DELAY)
            return;
        break;
    case FKL_TYPE_F64:
    case FKL_TYPE_BIGINT:
    case FKL_TYPE_STR:
    case FKL_TYPE_SYM:
    case FKL_TYPE_VECTOR:
    case FKL_TYPE_PAIR:
    case FKL_TYPE_BOX:
    case FKL_TYPE_BYTEVECTOR:
    case FKL_TYPE_CPROC:
    case FKL_TYPE_VAR_REF:
        break;
    case FKL_TYPE_PROC:
        fklZfree(FKL_VM_PROC(cur)->closure);
        break;
    case FKL_TYPE_HASHTABLE:
        fklVMvalueHashMapUninit(&FKL_VM_HASH(cur)->ht);
        break;
    default:
        FKL_UNREACHABLE();
        break;
    }
    atomic_fetch_sub(&gc->alloced_size, fklZmallocSize(cur));
    fklZfree((void *)cur);
}

void fklVMgcSweep(FklVMgc *gc, FklVMvalue *head) {
    FklVMvalue **phead = &head;
    while (*phead) {
        FklVMvalue *cur = *phead;
        *phead = cur->next;
        cur->next = NULL;
        destroy_vm_value(gc, cur);
    }
}

void fklVMgcAddLocvCache(FklVMgc *gc, uint32_t llast, FklVMvalue **locv) {
    struct FklLocvCacheLevel *locv_cache_level = gc->locv_cache;
    uint32_t idx = fklVMgcComputeLocvLevelIdx(llast);

    struct FklLocvCacheLevel *locv_cache = &locv_cache_level[idx];
    uint32_t num = locv_cache->num;
    struct FklLocvCache *locvs = locv_cache->locv;

    uint8_t i = 0;
    for (; i < num; i++) {
        if (llast < locvs[i].llast)
            break;
    }

    if (i < FKL_VM_GC_LOCV_CACHE_NUM) {
        if (num == FKL_VM_GC_LOCV_CACHE_NUM) {
            atomic_fetch_sub(&gc->alloced_size,
                    fklZmallocSize(locvs[FKL_VM_GC_LOCV_CACHE_LAST_IDX].locv));
            fklZfree(locvs[FKL_VM_GC_LOCV_CACHE_LAST_IDX].locv);
            num--;
        } else
            locv_cache->num++;
        for (uint8_t j = num; j > i; j--)
            locvs[j] = locvs[j - 1];
        locvs[i].llast = llast;
        locvs[i].locv = locv;
    } else {
        atomic_fetch_sub(&gc->alloced_size, fklZmallocSize(locv));
        fklZfree(locv);
    }
}

void fklVMgcMoveLocvCache(FklVM *vm, FklVMgc *gc) {
    FklVMlocvList *cur = vm->old_locv_list;
    uint32_t i = vm->old_locv_count;
    for (; i > FKL_VM_GC_LOCV_CACHE_NUM; i--) {
        fklVMgcAddLocvCache(gc, cur->llast, cur->locv);

        FklVMlocvList *prev = cur;
        cur = cur->next;
        fklZfree(prev);
    }

    for (uint32_t j = 0; j < i; j++) {
        FklVMlocvList *cur = &vm->old_locv_cache[j];

        fklVMgcAddLocvCache(gc, cur->llast, cur->locv);
        cur->llast = 0;
        cur->locv = NULL;
    }
    vm->old_locv_list = NULL;
    vm->old_locv_count = 0;
}

void fklGetGCstateAndGCNum(FklVMgc *gc, FklGCstate *s, int *cr) {
    *s = gc->running;
    *cr = atomic_load(&gc->alloced_size) > gc->threshold;
}

static inline void init_vm_queue(FklVMqueue *q) {
    uv_mutex_init(&q->pre_running_lock);
    fklThreadQueueInit(&q->pre_running_q);
    atomic_init(&q->running_count, 0);
    fklThreadQueueInit(&q->running_q);
}

static inline void uninit_vm_queue(FklVMqueue *q) {
    uv_mutex_destroy(&q->pre_running_lock);
    fklThreadQueueUninit(&q->pre_running_q);
    fklThreadQueueUninit(&q->running_q);
}

static inline void init_locv_cache(FklVMgc *gc) {
    uv_mutex_init(&gc->locv_cache[0].lock);
    uv_mutex_init(&gc->locv_cache[1].lock);
    uv_mutex_init(&gc->locv_cache[2].lock);
    uv_mutex_init(&gc->locv_cache[3].lock);
    uv_mutex_init(&gc->locv_cache[4].lock);
}

void fklInitVMargs(FklVMgc *gc, int argc, const char *const *argv) {
    gc->argc = argc;
    if (gc->argc == 0) {
        gc->argv = NULL;
        return;
    }
    size_t size = sizeof(const char *) * gc->argc;
    char **argv_v = (char **)fklZmalloc(size);
    FKL_ASSERT(argv_v);
    for (int i = 0; i < gc->argc; i++)
        argv_v[i] = fklZstrdup(argv[i]);
    gc->argv = argv_v;
}

static inline void init_idle_work_queue(FklVMgc *gc) {
    uv_mutex_init(&gc->workq_lock);
    gc->workq.tail = &gc->workq.head;
}

void fklInitVMgc(FklVMgc *gc, FklVMobarray *obarray) {
    memset(gc, 0, sizeof(FklVMgc));
    gc->threshold = FKL_VM_GC_THRESHOLD_SIZE;
    uv_mutex_init(&gc->extra_mark_lock);
    uv_mutex_init(&gc->print_backtrace_lock);
    gc->obarray = obarray;
    init_idle_work_queue(gc);
    init_vm_queue(&gc->q);
    init_locv_cache(gc);

    gc->obarray = obarray;
    gc->gcvm.gc = gc;
    gc->gcvm.next = &gc->gcvm;
    gc->gcvm.prev = &gc->gcvm;

    gc->seek_set = fklVMaddSymbolCstr(&gc->gcvm, "set");
    gc->seek_cur = fklVMaddSymbolCstr(&gc->gcvm, "cur");
    gc->seek_end = fklVMaddSymbolCstr(&gc->gcvm, "end");

    gc->sym_quote = fklVMaddSymbolCstr(&gc->gcvm, "quote");
    gc->sym_unquote = fklVMaddSymbolCstr(&gc->gcvm, "unquote");
    gc->sym_qsquote = fklVMaddSymbolCstr(&gc->gcvm, "qsquote");
    gc->sym_unqtesp = fklVMaddSymbolCstr(&gc->gcvm, "unqtesp");

    fklInitBuiltinErrorType(gc->builtinErrorTypeId, gc);
    fklInitGlobalVMclosureForGC(gc);
}

FklVMgc *fklCreateVMgc(FklVMobarray *obarray) {
    FklVMgc *gc = (FklVMgc *)fklZmalloc(sizeof(FklVMgc));
    FKL_ASSERT(gc);
    fklInitVMgc(gc, obarray);
    return gc;
}

void fklInitVMobarray(FklVMobarray *obarray) {
    uv_mutex_init(&obarray->lock);
    fklStrValueHashMapInit(&obarray->map);
}

void fklUninitVMobarray(FklVMobarray *obarray) {
    uv_mutex_destroy(&obarray->lock);
    fklStrValueHashMapUninit(&obarray->map);
}

FklVMobarray *fklCreateVMobarray(void) {
    FklVMobarray *obarray = (FklVMobarray *)fklZcalloc(1, sizeof(FklVMobarray));
    fklInitVMobarray(obarray);
    return obarray;
}

void fklDestroyVMobarray(FklVMobarray *obarray) {
    fklUninitVMobarray(obarray);
    fklZfree(obarray);
}

FklVMvalue **
fklAllocLocalVarSpaceFromGC(FklVMgc *gc, uint32_t llast, uint32_t *pllast) {
    uint32_t idx = fklVMgcComputeLocvLevelIdx(llast);
    FklVMvalue **r = NULL;
    for (uint8_t i = idx; !r && i < FKL_VM_GC_LOCV_CACHE_LEVEL_NUM; i++) {
        struct FklLocvCacheLevel *l = &gc->locv_cache[i];
        uv_mutex_lock(&l->lock);
        if (l->num) {
            struct FklLocvCache *ll = l->locv;
            for (uint8_t j = 0; j < FKL_VM_GC_LOCV_CACHE_NUM; j++) {
                if (ll[j].llast >= llast) {
                    *pllast = ll[j].llast;
                    r = ll[j].locv;
                    l->num--;
                    for (uint8_t k = j; k < l->num; k++)
                        ll[k] = ll[k + 1];
                    ll[l->num].llast = 0;
                    ll[l->num].locv = 0;
                    break;
                }
            }
        }
        uv_mutex_unlock(&l->lock);
    }
    if (!r) {
        *pllast = llast;
        if (!llast)
            r = NULL;
        else {
            r = (FklVMvalue **)fklZmalloc(llast * sizeof(FklVMvalue *));
            FKL_ASSERT(r);
        }
        atomic_fetch_add(&gc->alloced_size, fklZmallocSize(r));
    }
    return r;
}

FklVMvalue **fklAllocLocalVarSpaceFromGCwithoutLock(FklVMgc *gc,
        uint32_t llast,
        uint32_t *pllast) {
    uint32_t idx = fklVMgcComputeLocvLevelIdx(llast);
    FklVMvalue **r = NULL;
    for (uint8_t i = idx; !r && i < FKL_VM_GC_LOCV_CACHE_LEVEL_NUM; i++) {
        struct FklLocvCacheLevel *l = &gc->locv_cache[i];
        if (l->num) {
            struct FklLocvCache *ll = l->locv;
            for (uint8_t j = 0; j < FKL_VM_GC_LOCV_CACHE_NUM; j++) {
                if (ll[j].llast >= llast) {
                    *pllast = ll[j].llast;
                    r = ll[j].locv;
                    l->num--;
                    for (uint8_t k = j; k < l->num; k++)
                        ll[k] = ll[k + 1];
                    ll[l->num].llast = 0;
                    ll[l->num].locv = 0;
                    break;
                }
            }
        }
    }
    if (!r) {
        *pllast = llast;
        if (!llast)
            r = NULL;
        else {
            r = (FklVMvalue **)fklZmalloc(llast * sizeof(FklVMvalue *));
            FKL_ASSERT(r);
        }
        atomic_fetch_add(&gc->alloced_size, fklZmallocSize(r));
    }
    return r;
}

void fklAddToGC(FklVMvalue *v, FklVM *vm) {
    if (FKL_IS_PTR(v)) {
        v->next = vm->obj_head;
        vm->obj_head = v;
        if (!vm->obj_tail)
            vm->obj_tail = v;
        atomic_fetch_add(&vm->gc->alloced_size, fklZmallocSize(v));
    }
}

static inline void destroy_all_locv_cache(FklVMgc *gc) {
    struct FklLocvCacheLevel *levels = gc->locv_cache;
    for (uint8_t i = 0; i < FKL_VM_GC_LOCV_CACHE_LEVEL_NUM; i++) {
        struct FklLocvCacheLevel *cur_level = &levels[i];
        uv_mutex_destroy(&cur_level->lock);
        struct FklLocvCache *cache = cur_level->locv;
        for (uint8_t j = 0; j < FKL_VM_GC_LOCV_CACHE_NUM; j++) {
            struct FklLocvCache *cur_cache = &cache[j];
            if (cur_cache->locv) {
                atomic_fetch_sub(&gc->alloced_size,
                        fklZmallocSize(cur_cache->locv));
                fklZfree(cur_cache->locv);
            }
        }
    }
}

void fklVMgcUpdateThreshold(FklVMgc *gc) {
    gc->threshold = atomic_load(&gc->alloced_size) * 2;
}

static inline void destroy_argv(FklVMgc *gc) {
    if (gc->argc) {
        char **const end = &gc->argv[gc->argc];
        for (char **cur = gc->argv; cur < end; cur++)
            fklZfree(*cur);
        fklZfree(gc->argv);
    }
}

void fklDestroyVMinterruptHandlerList(struct FklVMinterruptHandleList *l) {
    if (l->finalizer)
        l->finalizer(l->int_handle_arg);
    fklZfree(l);
}

static inline void destroy_interrupt_handle_list(
        struct FklVMinterruptHandleList *l) {
    while (l) {
        struct FklVMinterruptHandleList *c = l;
        if (l->finalizer)
            l->finalizer(l->int_handle_arg);
        l = l->next;
        fklZfree(c);
    }
}

static inline void destroy_extra_mark_list(struct FklVMextraMarkObjList *l) {
    while (l) {
        struct FklVMextraMarkObjList *c = l;
        if (l->finalizer)
            l->finalizer(l->arg);
        l = l->next;
        fklZfree(c);
    }
}

void fklVMclearExtraMarkFunc(FklVMgc *gc) {
    uv_mutex_lock(&gc->extra_mark_lock);
    destroy_extra_mark_list(gc->extra_mark_list);
    gc->extra_mark_list = NULL;
    uv_mutex_unlock(&gc->extra_mark_lock);
}

void fklUninitVMgc(FklVMgc *gc) {
    fklMoveThreadObjectsToGc(&gc->gcvm, gc);
    if (gc->obarray)
        fklDestroyVMobarray(gc->obarray);
    uv_mutex_destroy(&gc->workq_lock);
    uv_mutex_destroy(&gc->extra_mark_lock);
    uv_mutex_destroy(&gc->print_backtrace_lock);
    destroy_interrupt_handle_list(gc->int_list);
    destroy_extra_mark_list(gc->extra_mark_list);
    destroy_argv(gc);
    destroy_all_locv_cache(gc);
    fklVMgcSweep(gc, gc->head);
    gc->head = NULL;
    uninit_vm_queue(&gc->q);

    if (gc->alloced_size) {
        fprintf(stderr,
                "[WARNING %s: %d] still has %zu bytes not freed\n",
                __REL_FILE__,
                __LINE__,
                gc->alloced_size);
    }

    memset(gc, 0, sizeof(FklVMgc));
}

void fklDestroyVMgc(FklVMgc *gc) {
    fklUninitVMgc(gc);
    fklZfree(gc);
}

void fklVMacquireWq(FklVMgc *gc) { uv_mutex_lock(&gc->workq_lock); }

void fklVMreleaseWq(FklVMgc *gc) { uv_mutex_unlock(&gc->workq_lock); }

FklVMvalue *fklVMaddSymbol(FklVM *vm, const FklString *str) {
    return fklVMaddSymbolCharBuf(vm, str->str, str->size);
}

FklVMvalue *fklVMaddSymbolCstr(FklVM *vm, const char *str) {
    return fklVMaddSymbolCharBuf(vm, str, strlen(str));
}

static inline FklStrValueHashMapNode *const *
get_btk(FklStrValueHashMap *ht, uintptr_t *hashv, const char *buf, size_t len) {
    *hashv = fklCharBufHash(buf, len);
    FklStrValueHashMapNode *const *bkt = fklStrValueHashMapBucket(ht, *hashv);
    for (; *bkt; bkt = &(*bkt)->bkt_next) {
        FklStrValueHashMapNode *cur = *bkt;
        if (!fklStringCharBufCmp(cur->k, len, buf)) {
            break;
        }
    }

    return bkt;
}

FklVMvalue *fklVMaddSymbolCharBuf(FklVM *vm, const char *buf, size_t len) {
    FklVMgc *gc = vm->gc;
    uv_mutex_lock(&gc->obarray->lock);

    FklVMvalue *r = NULL;

    uintptr_t hashv = 0;
    FklStrValueHashMap *ht = &gc->obarray->map;
    FklStrValueHashMapNode *const *bkt = get_btk(ht, &hashv, buf, len);
    if (*bkt) {
        r = (*bkt)->v;
    } else {
        FklString *str = fklCreateString(len, buf);
        FklStrValueHashMapNode *node =
                fklStrValueHashMapCreateNode2(hashv, str);
        r = fklCreateVMvalueSym2(vm, len, buf);
        FKL_VM_SYM_INTERNED(r) = 1;
        node->v = r;
        fklStrValueHashMapInsertNode(ht, node);
    }

    uv_mutex_unlock(&gc->obarray->lock);

    return r;
    // uv_rwlock_wrlock(&gc->st_lock);
    // FklSid_t r = fklAddSymbolCharBuf(str, size, gc->st);
    // uv_rwlock_wrunlock(&gc->st_lock);
    // return r;
}

static inline FklVMvalue *add_symbol_value_unlock(FklStrValueHashMap *ht,
        FklVMvalue *v) {
    FklVMvalue *r = NULL;
    const FklString *s = FKL_VM_SYM(v);
    uintptr_t hashv = 0;
    FklStrValueHashMapNode *const *bkt = get_btk(ht, &hashv, s->str, s->size);

    if (*bkt) {
        r = (*bkt)->v;
    } else {
        FklString *str = fklCreateString(s->size, s->str);
        FklStrValueHashMapNode *node =
                fklStrValueHashMapCreateNode2(hashv, str);
        r = v;
        FKL_VM_SYM_INTERNED(r) = 1;
        node->v = r;
        fklStrValueHashMapInsertNode(ht, node);
    }

    return r;
}

FklVMvalue *fklVMaddSymbolValue(FklVM *vm, FklVMvalue *v) {
    FKL_ASSERT(FKL_IS_SYM(v));
    FklVMgc *gc = vm->gc;
    uv_mutex_lock(&gc->obarray->lock);

    FklVMvalue *r = add_symbol_value_unlock(&gc->obarray->map, v);

    uv_mutex_unlock(&gc->obarray->lock);

    return r;
}

void fklVMclearSymbol(FklVMgc *gc) {
    uv_mutex_lock(&gc->obarray->lock);
    fklStrValueHashMapClear(&gc->obarray->map);
    uv_mutex_unlock(&gc->obarray->lock);
}

void fklVMrestoreSymbol(FklVMgc *gc) {
    uv_mutex_lock(&gc->obarray->lock);
    FklStrValueHashMap *ht = &gc->obarray->map;
    for (FklVMvalue *cur = gc->head; cur; cur = cur->next) {
        if (FKL_IS_SYM(cur) && FKL_VM_SYM_INTERNED(cur)) {
            FklVMvalue *r = add_symbol_value_unlock(ht, cur);
            FKL_ASSERT(r == cur);
            (void)r;
        }
    }
    uv_mutex_unlock(&gc->obarray->lock);
}

FklVMinterruptResult
fklVMinterrupt(FklVM *vm, FklVMvalue *ev, FklVMvalue **pv) {
    struct FklVMinterruptHandleList *l = vm->int_list;
    for (; l; l = l->next)
        if (l->int_handler(vm, ev, &ev, l->int_handle_arg) == FKL_INT_DONE)
            return FKL_INT_DONE;
    FklVMgc *gc = vm->gc;
    for (l = gc->int_list; l; l = l->next)
        if (l->int_handler(vm, ev, &ev, l->int_handle_arg) == FKL_INT_DONE)
            return FKL_INT_DONE;
    if (pv)
        *pv = ev;
    return FKL_INT_NEXT;
}

void fklVMpushInterruptHandler(FklVMgc *gc,
        FklVMinterruptHandler func,
        FklVMextraMarkFunc mark,
        void (*finalizer)(void *),
        void *arg) {
    struct FklVMinterruptHandleList *l =
            (struct FklVMinterruptHandleList *)fklZmalloc(
                    sizeof(struct FklVMinterruptHandleList));
    FKL_ASSERT(l);
    l->int_handler = func;
    l->int_handle_arg = arg;
    l->mark = mark;
    l->finalizer = finalizer;
    l->next = gc->int_list;
    gc->int_list = l;
}

void fklVMpushInterruptHandlerLocal(FklVM *exe,
        FklVMinterruptHandler func,
        FklVMextraMarkFunc mark,
        void (*finalizer)(void *),
        void *arg) {
    struct FklVMinterruptHandleList *l =
            (struct FklVMinterruptHandleList *)fklZmalloc(
                    sizeof(struct FklVMinterruptHandleList));
    FKL_ASSERT(l);
    l->int_handler = func;
    l->int_handle_arg = arg;
    l->mark = mark;
    l->finalizer = finalizer;
    l->next = exe->int_list;
    exe->int_list = l;
}

void fklVMpushExtraMarkFunc(FklVMgc *gc,
        FklVMextraMarkFunc func,
        void (*finalizer)(void *),
        void *arg) {
    struct FklVMextraMarkObjList *l =
            (struct FklVMextraMarkObjList *)fklZmalloc(
                    sizeof(struct FklVMextraMarkObjList));
    FKL_ASSERT(l);
    l->func = func;
    l->arg = arg;
    l->finalizer = finalizer;
    uv_mutex_lock(&gc->extra_mark_lock);
    l->next = gc->extra_mark_list;
    gc->extra_mark_list = l;
    uv_mutex_unlock(&gc->extra_mark_lock);
}
