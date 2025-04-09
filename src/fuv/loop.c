#include "fuv.h"
#include <string.h>

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_loop_as_print, loop);

static void fuv_loop_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(fuv_loop, FuvLoop, ud);
    for (FklVMvalueUsetNode *l = fuv_loop->data.gc_values.first; l;
         l = l->next) {
        FklVMvalue *v = FKL_REMOVE_CONST(FklVMvalue, l->k);
        fklVMgcToGray(v, gc);
    }
    struct FuvErrorRecoverData *rd = &fuv_loop->data.error_recover_data;
    for (FklVMframe *frame = rd->frame; frame; frame = frame->prev)
        fklDoAtomicFrame(frame, gc);
    FklVMvalue **cur = rd->stack_values;
    FklVMvalue **end = &cur[rd->stack_values_num];
    for (; cur < end; cur++)
        fklVMgcToGray(*cur, gc);
    cur = rd->local_values;
    end = &cur[rd->local_values_num];
    for (; cur < end; cur++)
        fklVMgcToGray(*cur, gc);
}

static void fuv_close_loop_handle_cb(uv_handle_t *handle) {
    FuvHandle *fuv_handle = uv_handle_get_data(handle);
    FuvHandleData *hdata = &fuv_handle->data;
    FklVMvalue *handle_obj = hdata->handle;
    free(fuv_handle);
    if (handle_obj) {
        FKL_DECL_VM_UD_DATA(fuv_handle_ud, FuvHandleUd, handle_obj);
        *fuv_handle_ud = NULL;
    }
}

static void fuv_close_loop_walk_cb(uv_handle_t *handle, void *arg) {
    FuvLoop *fuv_loop = arg;
    if (handle == (uv_handle_t *)&fuv_loop->data.error_check_idle)
        return;
    if (uv_is_closing(handle)) {
        FuvHandle *fh = uv_handle_get_data(handle);
        fh->data.callbacks[1] = NULL;
    } else
        uv_close(handle, fuv_close_loop_handle_cb);
}

static void fuv_loop_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(fuv_loop, FuvLoop, ud);
    uv_walk(&fuv_loop->loop, fuv_close_loop_walk_cb, fuv_loop);
    while (uv_loop_close(&fuv_loop->loop))
        uv_run(&fuv_loop->loop, UV_RUN_DEFAULT);
    fklVMvalueUsetUninit(&fuv_loop->data.gc_values);
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
    FklVMvalue **locv = fklAllocSpaceForLocalVar(exe, rd->local_values_num);
    memcpy(locv, rd->local_values, sizeof(FklVMvalue *) * rd->local_values_num);

    FklVMframe *f = rd->frame;
    while (f) {
        FklVMframe *prev = f->prev;
        f->prev = exe->top_frame;
        exe->top_frame = f;
        f = prev;
    }

    free(rd->stack_values);
    free(rd->local_values);
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
    longjmp(fuv_loop->data.buf, FUV_RUN_ERR_OCCUR);
}

static void error_check_idle_cb(uv_idle_t *idle) {
    uv_close((uv_handle_t *)idle, error_check_idle_close_cb);
}

FklVMvalue *createFuvLoop(FklVM *vm, FklVMvalue *rel, int *err) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &FuvLoopMetaTable, rel);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, v);
    fuv_loop->data.mode = -1;
    int r = uv_loop_init(&fuv_loop->loop);
    if (r) {
        *err = r;
        return NULL;
    }
    fklVMvalueUsetInit(&fuv_loop->data.gc_values);
    uv_loop_set_data(&fuv_loop->loop, &fuv_loop->data);
    uv_handle_set_data((uv_handle_t *)&fuv_loop->data.error_check_idle,
                       fuv_loop);
    return v;
}

void startErrorHandle(uv_loop_t *loop, FuvLoopData *ldata, FklVM *exe,
                      uint32_t sbp, uint32_t stp, uint32_t ltp,
                      FklVMframe *buttom_frame) {
    struct FuvErrorRecoverData *rd = &ldata->error_recover_data;

    uint32_t origin_ltp = exe->ltp;
    exe->ltp = ltp + rd->local_values_num;
    FklVMframe *f = rd->frame;
    while (f) {
        FklVMframe *prev = f->prev;
        fklDestroyVMframe(f, exe);
        f = prev;
    }

    exe->ltp = origin_ltp;
    rd->local_values_num = exe->ltp - ltp;
    rd->stack_values_num = exe->tp - stp;

    free(rd->local_values);
    free(rd->stack_values);
    rd->local_values = fklCopyMemory(
        &exe->locv[ltp], sizeof(FklVMvalue *) * rd->local_values_num);
    rd->stack_values = fklCopyMemory(
        &exe->base[stp], sizeof(FklVMvalue *) * rd->stack_values_num);

    exe->bp = sbp;
    exe->tp = stp;
    exe->ltp = ltp;

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

void fuvLoopInsertFuvObj(FklVMvalue *loop_obj, FklVMvalue *v) {
    FKL_DECL_VM_UD_DATA(loop, FuvLoop, loop_obj);
    fklVMvalueUsetPut2(&loop->data.gc_values, v);
}
