#include "fuv.h"
#include "uv.h"

#include <fakeLisp/common.h>
#include <fakeLisp/vm.h>
#include <string.h>

// static FKL_ALWAYS_INLINE FuvValueLoop *as_loop(const FklVMvalue *v) {
//     FKL_ASSERT(isFuvLoop(v));
//     return FKL_TYPE_CAST(FuvValueLoop *, v);
// }

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_loop_as_print, "loop");

static inline FklVMvalue *get_obj_next(FklVMvalue *v) {
    FKL_ASSERT(FKL_IS_USERDATA(v));
    if (isFuvHandle(v)) {
        return FUV_HANDLE(v)->h.data.next;
    } else if (isFuvReq(v)) {
        return FUV_REQ(v)->r.data.next;
    } else {
        FKL_UNREACHABLE();
    }
    return NULL;
}

static void fuv_loop_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(fuv_loop, FuvLoop, ud);
    FuvLoop *l = &FUV_LOOP(ud)->l;

    for (FklVMvalue *ref = l->data.refs; ref; ref = get_obj_next(ref))
        fklVMgcToGray(ref, gc);
}

static void fuv_close_loop_walk_cb(uv_handle_t *handle, void *arg) {
    if (uv_is_closing(handle)) {
        FuvValueHandle *fh = uv_handle_get_data(handle);
        fh->h.data.callbacks[1] = NULL;
    } else {
        fuvHandleClose(uv_handle_get_data(handle), NULL);
    }
}

static inline void fuv_loop_remove_obj_ref(FklVMvalue *v) {
    if (isFuvHandle(v)) {
        FuvValueHandle *h = FUV_HANDLE(v);
        fuvLoopRemoveHandleRef(h->h.data.loop, h);
    } else {
        FuvValueReq *h = FUV_REQ(v);
        fuvLoopRemoveReqRef(h->r.data.loop, h);
    }
}

static int fuv_loop_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(fuv_loop, FuvLoop, ud);
    FuvValueLoop *l = FUV_LOOP(ud);
    if (fuvLoopIsClosed(l))
        goto closed;
    uv_walk(&l->l.loop, fuv_close_loop_walk_cb, l);
    while (uv_loop_close(&l->l.loop))
        uv_run(&l->l.loop, UV_RUN_DEFAULT);
    while (l->l.data.refs) {
        fuv_loop_remove_obj_ref(l->l.data.refs);
    }
closed:
    fklVMgcSweep(gc, l->l.data.gclist);
    l->l.data.gclist = NULL;
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable const FuvLoopMetaTable = {
    // .size = sizeof(FuvLoop),
    .size = sizeof(FuvValueLoop),
    .__as_prin1 = fuv_loop_as_print,
    .__as_princ = fuv_loop_as_print,
    .__atomic = fuv_loop_atomic,
    .__finalizer = fuv_loop_finalizer,
};

FklVMvalue *createFuvLoop(FklVM *vm, FklVMvalue *dll, int *err) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &FuvLoopMetaTable, dll);
    // FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, v);
    FuvValueLoop *l = FUV_LOOP(v);
    l->l.data.mode = -1;
    l->l.data.is_closed = 0;
    l->l.data.error_occured = 0;
    int r = uv_loop_init(&l->l.loop);
    if (r) {
        *err = r;
        return NULL;
    }
    uv_loop_set_data(&l->l.loop, &l->l.data);
    return v;
}

void startErrorHandle(uv_loop_t *loop, FuvLoopData *ldata, FklVM *exe) {
    ldata->error_occured = 1;
    uv_stop(loop);
}

int isFuvLoop(const FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->mt_ == &FuvLoopMetaTable;
}

void fuvLoopAddGcObj(FklVMvalue *loop_obj, FklVMvalue *v) {
    FKL_ASSERT(isFuvLoop(loop_obj));
    fuv_loop_remove_obj_ref(v);
    // FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    FuvValueLoop *l = FUV_LOOP(loop_obj);
    v->next_ = l->l.data.gclist;
    l->l.data.gclist = v;
}

static inline void set_obj_prev(FklVMvalue *v, FklVMvalue *prev) {
    FKL_ASSERT(FKL_IS_USERDATA(v));
    if (isFuvHandle(v)) {
        FUV_HANDLE(v)->h.data.prev = prev;
    } else if (isFuvReq(v)) {
        FUV_REQ(v)->r.data.prev = prev;
    } else {
        FKL_UNREACHABLE();
    }
}

static inline void set_obj_next(FklVMvalue *v, FklVMvalue *next) {
    FKL_ASSERT(FKL_IS_USERDATA(v));
    if (isFuvHandle(v)) {
        FUV_HANDLE(v)->h.data.next = next;
    } else if (isFuvReq(v)) {
        FUV_REQ(v)->r.data.next = next;
    } else {
        FKL_UNREACHABLE();
    }
}

void fuvLoopAddHandleRef(FklVMvalue *loop_obj, FuvValueHandle *h) {
    FKL_ASSERT(isFuvLoop(loop_obj));
    // FKL_DECL_VM_UD_DATA(loop, FuvLoop, loop_obj);
    FuvValueLoop *loop = FUV_LOOP(loop_obj);

    FklVMvalue *v = FKL_VM_VAL(h); // fuvHandleValueOf(h);

    h->h.data.next = loop->l.data.refs;

    if (loop->l.data.refs)
        set_obj_prev(loop->l.data.refs, v);

    loop->l.data.refs = v;
}

void fuvLoopAddReqRef(FklVMvalue *loop_obj, FuvValueReq *r) {
    FKL_ASSERT(isFuvLoop(loop_obj));
    // FKL_DECL_VM_UD_DATA(loop, FuvLoop, loop_obj);
    FuvValueLoop *loop = FUV_LOOP(loop_obj);

    FklVMvalue *v = FKL_VM_VAL(r); // fuvReqValueOf(r);

    r->r.data.next = loop->l.data.refs;

    if (loop->l.data.refs)
        set_obj_prev(loop->l.data.refs, v);

    loop->l.data.refs = v;
}

void fuvLoopRemoveHandleRef(FklVMvalue *loop_obj, FuvValueHandle *h) {
    FKL_ASSERT(isFuvLoop(loop_obj));
    // FKL_DECL_VM_UD_DATA(loop, FuvLoop, loop_obj);
    FuvValueLoop *loop = FUV_LOOP(loop_obj);

    if (h->h.data.prev)
        set_obj_next(h->h.data.prev, h->h.data.next);
    else {
        loop->l.data.refs = h->h.data.next;
    }

    if (h->h.data.next)
        set_obj_prev(h->h.data.next, h->h.data.prev);

    h->h.data.next = NULL;
    h->h.data.prev = NULL;
    h->h.data.loop = NULL;
}

void fuvLoopRemoveReqRef(FklVMvalue *loop_obj, FuvValueReq *r) {
    FKL_ASSERT(isFuvLoop(loop_obj));
    // FKL_DECL_VM_UD_DATA(loop, FuvLoop, loop_obj);
    FuvValueLoop *loop = FUV_LOOP(loop_obj);

    if (r->r.data.prev)
        set_obj_next(r->r.data.prev, r->r.data.next);
    else {
        loop->l.data.refs = r->r.data.next;
    }

    if (r->r.data.next)
        set_obj_prev(r->r.data.next, r->r.data.prev);

    r->r.data.next = NULL;
    r->r.data.prev = NULL;
    r->r.data.loop = NULL;
}
