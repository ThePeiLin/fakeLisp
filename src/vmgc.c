#include <fakeLisp/vm.h>

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

static inline void gc_mark_root_to_gray(FklVM *exe) {
    remove_closed_var_ref(exe);
    mark_atexit(exe);
    mark_interrupt_handler(exe->gc, exe->int_list);
    FklVMgc *gc = exe->gc;

    for (FklVMframe *cur = exe->top_frame; cur; cur = cur->prev)
        fklDoAtomicFrame(cur, gc);

    for (size_t i = 1; i <= exe->libNum; i++) {
        FklVMlib *lib = &exe->libs[i];
        fklVMgcToGray(lib->proc, gc);
        if (lib->imported) {
            for (uint32_t i = 0; i < lib->count; i++)
                fklVMgcToGray(lib->loc[i], gc);
        }
    }
    FklVMvalue **base = exe->base;
    for (uint32_t i = 0; i < exe->tp; i++)
        fklVMgcToGray(base[i], gc);
    fklVMgcToGray(exe->chan, gc);
}

static inline void gc_extra_mark(FklVMgc *gc) {
    for (struct FklVMextraMarkObjList *l = gc->extra_mark_list; l; l = l->next)
        l->func(gc, l->arg);
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
    static void (
            *const fkl_atomic_value_method_table[FKL_VM_VALUE_GC_TYPE_NUM])(
        FklVMvalue *, FklVMgc *) = {
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
    size_t count = 0;
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
            count++;
        } else {
            cur->mark = FKL_MARK_W;
            phead = &cur->next;
        }
    }
    *phead = gc->head;
    gc->head = head;
    atomic_fetch_sub(&gc->num, count);
}

void fklVMgcSweep(FklVMvalue *head) {
    FklVMvalue **phead = &head;
    while (*phead) {
        FklVMvalue *cur = *phead;
        *phead = cur->next;
        cur->next = NULL;
        fklDestroyVMvalue(cur);
    }
}

void fklGetGCstateAndGCNum(FklVMgc *gc, FklGCstate *s, int *cr) {
    *s = gc->running;
    *cr = atomic_load(&gc->num) > gc->threshold;
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

void fklVMgcUpdateConstArray(FklVMgc *gc, FklConstTable *kt) {
    fklZfree(gc->ki64);
    if (kt->ki64t.count) {
        size_t size = sizeof(int64_t) * kt->ki64t.count;
        gc->ki64 = (int64_t *)fklZmalloc(size);
        FKL_ASSERT(gc->ki64);
        memcpy(gc->ki64, kt->ki64t.base, size);
    } else
        gc->ki64 = NULL;

    fklZfree(gc->kf64);
    if (kt->kf64t.count) {
        size_t size = sizeof(double) * kt->kf64t.count;
        gc->kf64 = (double *)fklZmalloc(size);
        FKL_ASSERT(gc->kf64);
        memcpy(gc->kf64, kt->kf64t.base, size);
    } else
        gc->kf64 = NULL;

    fklZfree(gc->kstr);
    if (kt->kstrt.count) {
        size_t size = sizeof(FklString *) * kt->kstrt.count;
        gc->kstr = (FklString **)fklZmalloc(size);
        FKL_ASSERT(gc->kstr);
        memcpy(gc->kstr, kt->kstrt.base, size);
    } else
        gc->kstr = NULL;

    fklZfree(gc->kbvec);
    if (kt->kbvect.count) {
        size_t size = sizeof(FklBytevector *) * kt->kbvect.count;
        gc->kbvec = (FklBytevector **)fklZmalloc(size);
        FKL_ASSERT(gc->kbvec);
        memcpy(gc->kbvec, kt->kbvect.base, size);
    } else
        gc->kbvec = NULL;

    fklZfree(gc->kbi);
    if (kt->kbit.count) {
        size_t size = sizeof(FklBigInt *) * kt->kbit.count;
        gc->kbi = (FklBigInt **)fklZmalloc(size);
        FKL_ASSERT(gc->kbi);
        memcpy(gc->kbi, kt->kbit.base, size);
    } else
        gc->kbi = NULL;
}

FklVMgc *fklCreateVMgc(FklSymbolTable *st, FklConstTable *kt,
                       FklFuncPrototypes *pts) {
    FklVMgc *gc = (FklVMgc *)fklZcalloc(1, sizeof(FklVMgc));
    FKL_ASSERT(gc);
    gc->threshold = FKL_VM_GC_THRESHOLD_SIZE;
    uv_rwlock_init(&gc->st_lock);
    uv_mutex_init(&gc->extra_mark_lock);
    gc->kt = kt;
    gc->st = st;
    gc->pts = pts;
    fklVMgcUpdateConstArray(gc, kt);
    fklInitBuiltinErrorType(gc->builtinErrorTypeId, st);

    init_idle_work_queue(gc);
    init_vm_queue(&gc->q);
    init_locv_cache(gc);
    return gc;
}

FklVMvalue **fklAllocLocalVarSpaceFromGC(FklVMgc *gc, uint32_t llast,
                                         uint32_t *pllast) {
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
        atomic_fetch_add(&gc->num, llast);
    }
    return r;
}

FklVMvalue **fklAllocLocalVarSpaceFromGCwithoutLock(FklVMgc *gc, uint32_t llast,
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
        atomic_fetch_add(&gc->num, llast);
    }
    return r;
}

void fklAddToGC(FklVMvalue *v, FklVM *vm) {
    if (FKL_IS_PTR(v)) {
        v->next = vm->obj_head;
        vm->obj_head = v;
        if (!vm->obj_tail)
            vm->obj_tail = v;
        atomic_fetch_add(&vm->gc->num, 1);
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
            if (cur_cache->llast)
                fklZfree(cur_cache->locv);
        }
    }
}

void fklVMgcUpdateThreshold(FklVMgc *gc) {
    gc->threshold = atomic_load(&gc->num) * 2;
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

static inline void
destroy_interrupt_handle_list(struct FklVMinterruptHandleList *l) {
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

void fklDestroyVMgc(FklVMgc *gc) {
    fklDestroyFuncPrototypes(gc->pts);
    uv_rwlock_destroy(&gc->st_lock);
    uv_mutex_destroy(&gc->workq_lock);
    uv_mutex_destroy(&gc->extra_mark_lock);
    destroy_interrupt_handle_list(gc->int_list);
    destroy_extra_mark_list(gc->extra_mark_list);
    destroy_argv(gc);
    destroy_all_locv_cache(gc);
    fklVMgcSweep(gc->head);
    gc->head = NULL;
    uninit_vm_queue(&gc->q);
    fklZfree(gc->ki64);
    fklZfree(gc->kf64);
    fklZfree(gc->kstr);
    fklZfree(gc->kbvec);
    fklZfree(gc->kbi);
    fklZfree(gc);
}

void fklVMacquireWq(FklVMgc *gc) { uv_mutex_lock(&gc->workq_lock); }

void fklVMreleaseWq(FklVMgc *gc) { uv_mutex_unlock(&gc->workq_lock); }

void fklVMacquireSt(FklVMgc *gc) { uv_rwlock_wrlock(&gc->st_lock); }

void fklVMreleaseSt(FklVMgc *gc) { uv_rwlock_wrunlock(&gc->st_lock); }

FklStrIdHashMapElm *fklVMaddSymbol(FklVMgc *gc, const FklString *str) {
    return fklVMaddSymbolCharBuf(gc, str->str, str->size);
}

FklStrIdHashMapElm *fklVMaddSymbolCstr(FklVMgc *gc, const char *str) {
    return fklVMaddSymbolCharBuf(gc, str, strlen(str));
}

FklStrIdHashMapElm *fklVMaddSymbolCharBuf(FklVMgc *gc, const char *str,
                                          size_t size) {
    uv_rwlock_wrlock(&gc->st_lock);
    FklStrIdHashMapElm *r = fklAddSymbolCharBuf(str, size, gc->st);
    uv_rwlock_wrunlock(&gc->st_lock);
    return r;
}

FklStrIdHashMapElm *fklVMgetSymbolWithId(FklVMgc *gc, FklSid_t id) {
    uv_rwlock_rdlock(&gc->st_lock);
    FklStrIdHashMapElm *r = fklGetSymbolWithId(id, gc->st);
    uv_rwlock_rdunlock(&gc->st_lock);
    return r;
}

FklVMinterruptResult fklVMinterrupt(FklVM *vm, FklVMvalue *ev,
                                    FklVMvalue **pv) {
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

void fklVMpushInterruptHandler(FklVMgc *gc, FklVMinterruptHandler func,
                               FklVMextraMarkFunc mark,
                               void (*finalizer)(void *), void *arg) {
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

void fklVMpushInterruptHandlerLocal(FklVM *exe, FklVMinterruptHandler func,
                                    FklVMextraMarkFunc mark,
                                    void (*finalizer)(void *), void *arg) {
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

void fklVMpushExtraMarkFunc(FklVMgc *gc, FklVMextraMarkFunc func,
                            void (*finalizer)(void *), void *arg) {
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
