#include "fuv.h"

static void fuv_handle_ud_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(handle, FuvHandle, ud);
    fklVMgcToGray(handle->data.loop, gc);
    fklVMgcToGray(handle->data.callbacks[0], gc);
    fklVMgcToGray(handle->data.callbacks[1], gc);
}

static int fuv_handle_ud_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(handle, FuvHandle, ud);
    FuvHandleData *handle_data = &handle->data;
    if (handle_data->loop) {
        fuvLoopAddGcObj(handle_data->loop, FKL_VM_VALUE_OF(ud));
        handle_data->loop = NULL;
        return FKL_VM_UD_FINALIZE_DELAY;
    }
    return FKL_VM_UD_FINALIZE_NOW;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_timer_as_print, "timer");

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_prepare_as_print, "prepare");

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_idle_as_print, "idle");

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_check_as_print, "check");

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_signal_as_print, "signal");

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_async_as_print, "async");

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_process_as_print, "process");

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_fs_poll_as_print, "fs-poll");

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_fs_event_as_print, "fs-event");

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_poll_as_print, "poll");

static void fuv_process_ud_atomic(const FklVMud *ud, FklVMgc *gc) {
    fuv_handle_ud_atomic(ud, gc);
    FKL_DECL_UD_DATA(handle, FuvProcess, ud);
    fklVMgcToGray(handle->env_obj, gc);
    fklVMgcToGray(handle->args_obj, gc);
    fklVMgcToGray(handle->file_obj, gc);
    fklVMgcToGray(handle->stdio_obj, gc);
}

static void fuv_pipe_ud_atomic(const FklVMud *ud, FklVMgc *gc) {
    fuv_handle_ud_atomic(ud, gc);
    FKL_DECL_UD_DATA(handle, FuvPipe, ud);
    fklVMgcToGray(handle->fp, gc);
}

static void fuv_poll_ud_atomic(const FklVMud *ud, FklVMgc *gc) {
    fuv_handle_ud_atomic(ud, gc);
    FKL_DECL_UD_DATA(handle, FuvPoll, ud);
    fklVMgcToGray(handle->fp, gc);
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_pipe_as_print, "pipe");

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_tcp_as_print, "tcp");

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_udp_as_print, "udp");

static void fuv_tty_ud_atomic(const FklVMud *ud, FklVMgc *gc) {
    fuv_handle_ud_atomic(ud, gc);
    FKL_DECL_UD_DATA(handle, FuvTty, ud);
    fklVMgcToGray(handle->fp, gc);
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_tty_as_print, "tty");

static const FklVMudMetaTable HandleMetaTables[UV_HANDLE_TYPE_MAX] = {
    [UV_UNKNOWN_HANDLE] = {0},

    [UV_ASYNC] =
        {
            .size = sizeof(FuvAsync),
            .__as_prin1 = fuv_async_as_print,
            .__as_princ = fuv_async_as_print,
            .__atomic = fuv_handle_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_CHECK] =
        {
            .size = sizeof(FuvCheck),
            .__as_prin1 = fuv_check_as_print,
            .__as_princ = fuv_check_as_print,
            .__atomic = fuv_handle_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_FS_EVENT] =
        {
            .size = sizeof(FuvFsEvent),
            .__as_prin1 = fuv_fs_event_as_print,
            .__as_princ = fuv_fs_event_as_print,
            .__atomic = fuv_handle_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_FS_POLL] =
        {
            .size = sizeof(FuvFsPoll),
            .__as_prin1 = fuv_fs_poll_as_print,
            .__as_princ = fuv_fs_poll_as_print,
            .__atomic = fuv_handle_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_HANDLE] = {0},

    [UV_IDLE] =
        {
            .size = sizeof(FuvIdle),
            .__as_prin1 = fuv_idle_as_print,
            .__as_princ = fuv_idle_as_print,
            .__atomic = fuv_handle_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_NAMED_PIPE] =
        {
            .size = sizeof(FuvPipe),
            .__as_prin1 = fuv_pipe_as_print,
            .__as_princ = fuv_pipe_as_print,
            .__atomic = fuv_pipe_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_POLL] =
        {
            .size = sizeof(FuvPoll),
            .__as_prin1 = fuv_poll_as_print,
            .__as_princ = fuv_poll_as_print,
            .__atomic = fuv_poll_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_PREPARE] =
        {
            .size = sizeof(FuvPrepare),
            .__as_prin1 = fuv_prepare_as_print,
            .__as_princ = fuv_prepare_as_print,
            .__atomic = fuv_handle_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_PROCESS] =
        {
            .size = sizeof(FuvProcess),
            .__as_prin1 = fuv_process_as_print,
            .__as_princ = fuv_process_as_print,
            .__atomic = fuv_process_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_STREAM] = {0},

    [UV_TCP] =
        {
            .size = sizeof(FuvTcp),
            .__as_prin1 = fuv_tcp_as_print,
            .__as_princ = fuv_tcp_as_print,
            .__atomic = fuv_handle_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_TIMER] =
        {
            .size = sizeof(FuvTimer),
            .__as_prin1 = fuv_timer_as_print,
            .__as_princ = fuv_timer_as_print,
            .__atomic = fuv_handle_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_TTY] =
        {
            .size = sizeof(FuvTty),
            .__as_prin1 = fuv_tty_as_print,
            .__as_princ = fuv_tty_as_print,
            .__atomic = fuv_tty_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_UDP] =
        {
            .size = sizeof(FuvUdp),
            .__as_prin1 = fuv_udp_as_print,
            .__as_princ = fuv_udp_as_print,
            .__atomic = fuv_handle_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_SIGNAL] =
        {
            .size = sizeof(FuvSignal),
            .__as_prin1 = fuv_signal_as_print,
            .__as_princ = fuv_signal_as_print,
            .__atomic = fuv_handle_ud_atomic,
            .__finalizer = fuv_handle_ud_finalizer,
        },

    [UV_FILE] = {0},
};

int isFuvHandle(FklVMvalue *v) {
    if (FKL_IS_USERDATA(v)) {
        const FklVMudMetaTable *t = FKL_VM_UD(v)->t;
        return t > &HandleMetaTables[UV_UNKNOWN_HANDLE]
            && t < &HandleMetaTables[UV_HANDLE_TYPE_MAX];
    }
    return 0;
}

static inline void init_fuv_handle(FuvHandle *handle, FklVMvalue *v,
                                   FklVMvalue *loop) {
    handle->data.loop = loop;
    handle->data.callbacks[0] = NULL;
    handle->data.callbacks[1] = NULL;
    uv_handle_set_data(&handle->handle, handle);
}

#define FUV_HANDLE_P(NAME, ENUM)                                               \
    int NAME(FklVMvalue *v) {                                                  \
        return FKL_IS_USERDATA(v)                                              \
            && FKL_VM_UD(v)->t == &HandleMetaTables[ENUM];                     \
    }

#define FUV_HANDLE_CREATOR(TYPE, NAME, ENUM)                                   \
    FklVMvalue *create##TYPE(FklVM *vm, FklVMvalue *rel, FklVMvalue *loop_obj, \
                             int *err) {                                       \
        FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[ENUM], rel);  \
        FKL_DECL_VM_UD_DATA(handle, TYPE, v);                                  \
        FKL_DECL_VM_UD_DATA(loop, FuvLoop, loop_obj);                          \
        init_fuv_handle(FKL_TYPE_CAST(FuvHandle *, handle), v, loop_obj);      \
        *err = uv_##NAME##_init(&loop->loop, &handle->handle);                 \
        return v;                                                              \
    }

FUV_HANDLE_P(isFuvTimer, UV_TIMER);
FUV_HANDLE_CREATOR(FuvTimer, timer, UV_TIMER);

FUV_HANDLE_P(isFuvPrepare, UV_PREPARE);
FUV_HANDLE_CREATOR(FuvPrepare, prepare, UV_PREPARE);

FUV_HANDLE_P(isFuvIdle, UV_IDLE);
FUV_HANDLE_CREATOR(FuvIdle, idle, UV_IDLE);

FUV_HANDLE_P(isFuvCheck, UV_CHECK);
FUV_HANDLE_CREATOR(FuvCheck, check, UV_CHECK);

FUV_HANDLE_P(isFuvSignal, UV_SIGNAL);
FUV_HANDLE_CREATOR(FuvSignal, signal, UV_SIGNAL);

FUV_HANDLE_P(isFuvAsync, UV_ASYNC);

static void fuv_async_cb(uv_async_t *handle) {
    uv_loop_t *loop = uv_handle_get_loop((uv_handle_t *)handle);
    FuvLoopData *ldata = uv_loop_get_data(loop);
    struct FuvAsync *async_handle = uv_handle_get_data((uv_handle_t *)handle);
    FuvHandleData *hdata = &async_handle->data;
    FklVMvalue *proc = hdata->callbacks[0];
    if (proc) {
        FklVM *exe = ldata->exe;
        fklLockThread(exe);
        uint32_t sbp = exe->bp;
        uint32_t stp = exe->tp;
        exe->state = FKL_VM_READY;
        FklVMframe *buttom_frame = exe->top_frame;
        struct FuvAsyncExtraData *extra = atomic_load(&async_handle->extra);
        fklSetBp(exe);
        FklVMvalue **cur = extra->base;
        FklVMvalue **const end = &cur[extra->num];
        FKL_VM_PUSH_VALUE(exe, proc);
        for (; cur < end; ++cur)
            FKL_VM_PUSH_VALUE(exe, *cur);
        fklCallObj(exe, proc);
        FUV_ASYNC_COPY_DONE(async_handle);
        FUV_ASYNC_WAIT_SEND(exe, async_handle);
        if (exe->thread_run_cb(exe, buttom_frame))
            startErrorHandle(loop, ldata, exe, sbp, stp, buttom_frame);
        else
            exe->tp--;
        fklUnlockThread(exe);
        return;
    }
}

FklVMvalue *createFuvAsync(FklVM *vm, FklVMvalue *rel, FklVMvalue *loop_obj,
                           FklVMvalue *proc_obj, int *err) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[UV_ASYNC], rel);
    FKL_DECL_VM_UD_DATA(handle, FuvAsync, v);
    FKL_DECL_VM_UD_DATA(loop, FuvLoop, loop_obj);
    handle->extra = NULL;
    handle->send_ready = (atomic_flag)ATOMIC_FLAG_INIT;
    handle->copy_done = (atomic_flag)ATOMIC_FLAG_INIT;
    atomic_flag_test_and_set(&handle->copy_done);
    atomic_flag_test_and_set(&handle->send_done);
    init_fuv_handle(FKL_TYPE_CAST(FuvHandle *, handle), v, loop_obj);
    handle->data.callbacks[0] = proc_obj;
    *err = uv_async_init(&loop->loop, &handle->handle, fuv_async_cb);
    return v;
}

FUV_HANDLE_P(isFuvProcess, UV_PROCESS);

uv_process_t *createFuvProcess(FklVM *vm, FklVMvalue **pr, FklVMvalue *rel,
                               FklVMvalue *loop_obj, FklVMvalue *proc_obj,
                               FklVMvalue *args_obj, FklVMvalue *env_obj,
                               FklVMvalue *file_obj, FklVMvalue *stdio_obj,
                               FklVMvalue *cwd_obj) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[UV_PROCESS], rel);
    FKL_DECL_VM_UD_DATA(handle, FuvProcess, v);
    init_fuv_handle(FKL_TYPE_CAST(FuvHandle *, handle), v, loop_obj);
    handle->data.callbacks[0] = proc_obj;
    handle->args_obj = args_obj;
    handle->env_obj = env_obj;
    handle->file_obj = file_obj;
    handle->stdio_obj = stdio_obj;
    handle->cwd_obj = cwd_obj;
    *pr = v;
    return &handle->handle;
}

int isFuvStream(FklVMvalue *v) {
    if (FKL_IS_USERDATA(v)) {
        const FklVMudMetaTable *t = FKL_VM_UD(v)->t;
        return t == &HandleMetaTables[UV_NAMED_PIPE]
            || t == &HandleMetaTables[UV_TTY] || t == &HandleMetaTables[UV_TCP]
            || t == &HandleMetaTables[UV_STREAM];
    }
    return 0;
}

FUV_HANDLE_P(isFuvPipe, UV_NAMED_PIPE);

#define OTHER_HANDLE_CREATOR(TYPE, NAME, ENUM)                                 \
    uv_##NAME##_t *create##TYPE(FklVM *vm, FklVMvalue **pr, FklVMvalue *rel,   \
                                FklVMvalue *loop_obj) {                        \
        FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[ENUM], rel);  \
        FKL_DECL_VM_UD_DATA(handle, TYPE, v);                                  \
        init_fuv_handle(FKL_TYPE_CAST(FuvHandle *, handle), v, loop_obj);      \
        *pr = v;                                                               \
        return &handle->handle;                                                \
    }

OTHER_HANDLE_CREATOR(FuvPipe, pipe, UV_NAMED_PIPE);

FUV_HANDLE_P(isFuvTcp, UV_TCP);

OTHER_HANDLE_CREATOR(FuvTcp, tcp, UV_TCP);

FUV_HANDLE_P(isFuvTty, UV_TTY);

uv_tty_t *createFuvTty(FklVM *vm, FklVMvalue **pr, FklVMvalue *rel,
                       FklVMvalue *loop_obj, FklVMvalue *fp_obj) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[UV_TTY], rel);
    FKL_DECL_VM_UD_DATA(handle, FuvTty, v);
    init_fuv_handle(FKL_TYPE_CAST(FuvHandle *, handle), v, loop_obj);
    handle->fp = fp_obj;
    *pr = v;
    return &handle->handle;
}

FUV_HANDLE_P(isFuvUdp, UV_UDP);

uv_udp_t *createFuvUdp(FklVM *vm, FklVMvalue **pr, FklVMvalue *rel,
                       FklVMvalue *loop_obj, int64_t mmsg_num_msgs) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[UV_UDP], rel);
    FKL_DECL_VM_UD_DATA(handle, FuvUdp, v);
    init_fuv_handle(FKL_TYPE_CAST(FuvHandle *, handle), v, loop_obj);
    handle->mmsg_num_msgs = mmsg_num_msgs;
    *pr = v;
    return &handle->handle;
}

FUV_HANDLE_P(isFuvFsPoll, UV_FS_POLL);
FUV_HANDLE_CREATOR(FuvFsPoll, fs_poll, UV_FS_POLL);

FUV_HANDLE_P(isFuvFsEvent, UV_FS_EVENT);
FUV_HANDLE_CREATOR(FuvFsEvent, fs_event, UV_FS_EVENT);

FUV_HANDLE_P(isFuvPoll, UV_POLL);

uv_poll_t *createFuvPoll(FklVM *vm, FklVMvalue **pr, FklVMvalue *rel,
                         FklVMvalue *loop_obj, FklVMvalue *fp_obj) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[UV_POLL], rel);
    FKL_DECL_VM_UD_DATA(handle, FuvPoll, v);
    init_fuv_handle(FKL_TYPE_CAST(FuvHandle *, handle), v, loop_obj);
    handle->fp = fp_obj;
    *pr = v;
    return &handle->handle;
}

#undef FUV_HANDLE_P
#undef FUV_HANDLE_CREATOR
#undef OTHER_HANDLE_CREATOR
