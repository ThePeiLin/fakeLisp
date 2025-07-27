#include "fuv.h"
#include "uv.h"

#include <fakeLisp/common.h>
#include <fakeLisp/vm.h>
#include <string.h>

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_loop_as_print, "loop");

static inline FklVMvalue *get_obj_next(FklVMvalue *v) {
    FKL_ASSERT(FKL_IS_USERDATA(v));
    if (isFuvHandle(v)) {
        return getFuvHandle(v)->data.next;
    } else if (isFuvReq(v)) {
        return getFuvReq(v)->data.next;
    } else {
        FKL_UNREACHABLE();
    }
    return NULL;
}

static void fuv_loop_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(fuv_loop, FuvLoop, ud);
    struct FuvErrorRecoverData *rd = &fuv_loop->data.error_recover_data;
    for (FklVMframe *frame = rd->frame; frame; frame = frame->prev)
        fklDoAtomicFrame(frame, gc);
    FklVMvalue **cur = rd->stack_values;
    FklVMvalue **end = &cur[rd->stack_values_num];
    for (; cur < end; cur++)
        fklVMgcToGray(*cur, gc);
    end = &cur[rd->local_values_num];
    for (; cur < end; cur++)
        fklVMgcToGray(*cur, gc);

    for (FklVMvalue *ref = fuv_loop->data.refs; ref; ref = get_obj_next(ref))
        fklVMgcToGray(ref, gc);
}

static void fuv_close_loop_walk_cb(uv_handle_t *handle, void *arg) {
    FuvLoop *fuv_loop = arg;
    if (handle == (uv_handle_t *)&fuv_loop->data.error_check_idle)
        return;
    if (uv_is_closing(handle)) {
        FuvHandle *fh = uv_handle_get_data(handle);
        fh->data.callbacks[1] = NULL;
    } else {
        fuvHandleClose(uv_handle_get_data(handle), NULL);
    }
}

static inline void fuv_loop_remove_obj_ref(FklVMvalue *v) {
    if (isFuvHandle(v)) {
        FuvHandle *h = getFuvHandle(v);
        fuvLoopRemoveHandleRef(h->data.loop, h);
    } else {
        FuvReq *h = getFuvReq(v);
        fuvLoopRemoveReqRef(h->data.loop, h);
    }
}

static int fuv_loop_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(fuv_loop, FuvLoop, ud);
    if (fuvLoopIsClosed(fuv_loop))
        goto closed;
    uv_walk(&fuv_loop->loop, fuv_close_loop_walk_cb, fuv_loop);
    while (uv_loop_close(&fuv_loop->loop))
        uv_run(&fuv_loop->loop, UV_RUN_DEFAULT);
    while (fuv_loop->data.refs) {
        fuv_loop_remove_obj_ref(fuv_loop->data.refs);
    }
closed:
    fklVMgcSweep(fuv_loop->data.gclist);
    fuv_loop->data.gclist = NULL;
    return FKL_VM_UD_FINALIZE_NOW;
}

static FklVMudMetaTable FuvLoopMetaTable = {
    .size = sizeof(FuvLoop),
    .__as_prin1 = fuv_loop_as_print,
    .__as_princ = fuv_loop_as_print,
    .__atomic = fuv_loop_atomic,
    .__finalizer = fuv_loop_finalizer,
};

static inline FklVMvalue *recover_error_scene(FklVM *exe,
        struct FuvErrorRecoverData *rd) {
    FklVMvalue **cur = rd->stack_values;
    FklVMvalue **end = &cur[rd->stack_values_num];
    for (; cur < end; cur++)
        FKL_VM_PUSH_VALUE(exe, *cur);

    FklVMframe *f = rd->frame;
    while (f) {
        FklVMframe *prev = f->prev;
        f->prev = exe->top_frame;
        exe->top_frame = f;
        f = prev;
    }

    fklZfree(rd->stack_values);
    memset(rd, 0, sizeof(*rd));
    return FKL_VM_POP_TOP_VALUE(exe);
}

static void error_check_idle_close_cb(uv_handle_t *h) {
    FuvLoop *fuv_loop = uv_handle_get_data(h);
    fklLockThread(fuv_loop->data.exe);
    FklVM *exe = fuv_loop->data.exe;
    FklVMvalue *ev =
            recover_error_scene(exe, &fuv_loop->data.error_recover_data);
    FKL_VM_PUSH_VALUE(fuv_loop->data.exe, ev);
    longjmp(*fuv_loop->data.buf, FUV_RUN_ERR_OCCUR);
}

static void error_check_idle_cb(uv_idle_t *idle) {
    uv_close((uv_handle_t *)idle, error_check_idle_close_cb);
}

FklVMvalue *createFuvLoop(FklVM *vm, FklVMvalue *dll, int *err) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &FuvLoopMetaTable, dll);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, v);
    fuv_loop->data.mode = -1;
    fuv_loop->data.is_closed = 0;
    int r = uv_loop_init(&fuv_loop->loop);
    if (r) {
        *err = r;
        return NULL;
    }
    uv_loop_set_data(&fuv_loop->loop, &fuv_loop->data);
    uv_handle_set_data((uv_handle_t *)&fuv_loop->data.error_check_idle,
            fuv_loop);
    return v;
}

void startErrorHandle(uv_loop_t *loop,
        FuvLoopData *ldata,
        FklVM *exe,
        uint32_t sbp,
        uint32_t stp,
        FklVMframe *buttom_frame) {
    struct FuvErrorRecoverData *rd = &ldata->error_recover_data;

    FklVMframe *f = rd->frame;
    while (f) {
        FklVMframe *prev = f->prev;
        fklDestroyVMframe(f, exe);
        f = prev;
    }

    rd->stack_values_num = exe->tp - stp;

    fklZfree(rd->stack_values);
    rd->stack_values = fklCopyMemory(&exe->base[stp],
            sizeof(FklVMvalue *) * rd->stack_values_num);

    exe->bp = sbp;
    exe->tp = stp;

    rd->frame = NULL;

    f = exe->top_frame;
    while (f != buttom_frame) {
        FklVMframe *prev = f->prev;
        f->prev = rd->frame;
        rd->frame = f;
        f = prev;
    }
    exe->top_frame = buttom_frame;

    uv_idle_t *idle = &ldata->error_check_idle;
    if (uv_is_active((uv_handle_t *)idle) || uv_is_closing((uv_handle_t *)idle))
        return;
    uv_idle_init(loop, idle);
    uv_idle_start(&ldata->error_check_idle, error_check_idle_cb);
}

int isFuvLoop(FklVMvalue *v) {
    return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->t == &FuvLoopMetaTable;
}

void fuvLoopAddGcObj(FklVMvalue *loop_obj, FklVMvalue *v) {
    FKL_ASSERT(isFuvLoop(loop_obj));
    fuv_loop_remove_obj_ref(v);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    v->next = fuv_loop->data.gclist;
    fuv_loop->data.gclist = v;
}

static inline void set_obj_prev(FklVMvalue *v, FklVMvalue *prev) {
    FKL_ASSERT(FKL_IS_USERDATA(v));
    if (isFuvHandle(v)) {
        getFuvHandle(v)->data.prev = prev;
    } else if (isFuvReq(v)) {
        getFuvReq(v)->data.prev = prev;
    } else {
        FKL_UNREACHABLE();
    }
}

static inline void set_obj_next(FklVMvalue *v, FklVMvalue *next) {
    FKL_ASSERT(FKL_IS_USERDATA(v));
    if (isFuvHandle(v)) {
        getFuvHandle(v)->data.next = next;
    } else if (isFuvReq(v)) {
        getFuvReq(v)->data.next = next;
    } else {
        FKL_UNREACHABLE();
    }
}

void fuvLoopAddHandleRef(FklVMvalue *loop_obj, FuvHandle *h) {
    FKL_ASSERT(isFuvLoop(loop_obj));
    FKL_DECL_VM_UD_DATA(loop, FuvLoop, loop_obj);

    FklVMvalue *v = fuvHandleValueOf(h);

    h->data.next = loop->data.refs;

    if (loop->data.refs)
        set_obj_prev(loop->data.refs, v);

    loop->data.refs = v;
}

void fuvLoopAddReqRef(FklVMvalue *loop_obj, FuvReq *r) {
    FKL_ASSERT(isFuvLoop(loop_obj));
    FKL_DECL_VM_UD_DATA(loop, FuvLoop, loop_obj);

    FklVMvalue *v = fuvReqValueOf(r);

    r->data.next = loop->data.refs;

    if (loop->data.refs)
        set_obj_prev(loop->data.refs, v);

    loop->data.refs = v;
}

void fuvLoopRemoveHandleRef(FklVMvalue *loop_obj, FuvHandle *h) {
    FKL_ASSERT(isFuvLoop(loop_obj));
    FKL_DECL_VM_UD_DATA(loop, FuvLoop, loop_obj);

    if (h->data.prev)
        set_obj_next(h->data.prev, h->data.next);
    else {
        loop->data.refs = h->data.next;
    }

    if (h->data.next)
        set_obj_prev(h->data.next, h->data.prev);

    h->data.next = NULL;
    h->data.prev = NULL;
    h->data.loop = NULL;
}

void fuvLoopRemoveReqRef(FklVMvalue *loop_obj, FuvReq *r) {
    FKL_ASSERT(isFuvLoop(loop_obj));
    FKL_DECL_VM_UD_DATA(loop, FuvLoop, loop_obj);

    if (r->data.prev)
        set_obj_next(r->data.prev, r->data.next);
    else {
        loop->data.refs = r->data.next;
    }

    if (r->data.next)
        set_obj_prev(r->data.next, r->data.prev);

    r->data.next = NULL;
    r->data.prev = NULL;
    r->data.loop = NULL;
}
