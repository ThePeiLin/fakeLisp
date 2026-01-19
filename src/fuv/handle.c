#include "fuv.h"
#include <uv.h>

static void fuv_handle_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    FuvValueHandle *handle = FUV_HANDLE(ud);
    fklVMgcToGray(handle->data.loop, gc);
    fklVMgcToGray(handle->data.callbacks[0], gc);
    fklVMgcToGray(handle->data.callbacks[1], gc);
}

static int fuv_handle_ud_finalize(FklVMvalue *ud, FklVMgc *gc) {
    FuvValueHandle *handle = FUV_HANDLE(ud);
    FuvHandleData *handle_data = &handle->data;
    if (handle_data->loop) {
        fuvLoopAddGcObj(handle_data->loop, ud);
        return FKL_VM_UD_FINALIZE_DELAY;
    }
    return FKL_VM_UD_FINALIZE_NOW;
}

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_timer_print, "timer");

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_prepare_print, "prepare");

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_idle_print, "idle");

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_check_print, "check");

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_signal_print, "signal");

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_async_print, "async");

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_process_print, "process");

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_fs_poll_print, "fs-poll");

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_fs_event_print, "fs-event");

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_poll_print, "poll");

static FKL_ALWAYS_INLINE FuvValueProcess *as_process(const FklVMvalue *v) {
    FKL_ASSERT(isFuvProcess(v));
    return FKL_TYPE_CAST(FuvValueProcess *, v);
}

static void fuv_process_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    fuv_handle_ud_atomic(ud, gc);
    FuvValueProcess *handle = as_process(ud);
    fklVMgcToGray(handle->env_obj, gc);
    fklVMgcToGray(handle->args_obj, gc);
    fklVMgcToGray(handle->file_obj, gc);
    fklVMgcToGray(handle->stdio_obj, gc);
}

static FKL_ALWAYS_INLINE FuvValuePipe *as_pipe(const FklVMvalue *v) {
    FKL_ASSERT(isFuvPipe(v));
    return FKL_TYPE_CAST(FuvValuePipe *, v);
}

static void fuv_pipe_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    fuv_handle_ud_atomic(ud, gc);
    fklVMgcToGray(as_pipe(ud)->fp, gc);
}

static FKL_ALWAYS_INLINE FuvValuePoll *as_poll(const FklVMvalue *v) {
    FKL_ASSERT(isFuvPoll(v));
    return FKL_TYPE_CAST(FuvValuePoll *, v);
}

static void fuv_poll_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    fuv_handle_ud_atomic(ud, gc);
    fklVMgcToGray(as_poll(ud)->fp, gc);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_pipe_print, "pipe");

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_tcp_print, "tcp");

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_udp_print, "udp");

static FKL_ALWAYS_INLINE FuvValueTty *as_tty(const FklVMvalue *v) {
    FKL_ASSERT(isFuvTty(v));
    return FKL_TYPE_CAST(FuvValueTty *, v);
}

static void fuv_tty_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    fuv_handle_ud_atomic(ud, gc);
    fklVMgcToGray(as_tty(ud)->fp, gc);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_tty_print, "tty");

static const FklVMudMetaTable HandleMetaTables[UV_HANDLE_TYPE_MAX] = {
    [UV_UNKNOWN_HANDLE] = {0},

    [UV_ASYNC] =
        {
			.size = sizeof(FuvValueAsync),
            .prin1 = fuv_async_print,
            .princ = fuv_async_print,
            .atomic = fuv_handle_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_CHECK] =
        {
            .size = sizeof(FuvValueCheck),
            .prin1 = fuv_check_print,
            .princ = fuv_check_print,
            .atomic = fuv_handle_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_FS_EVENT] =
        {
			.size = sizeof(FuvValueFsEvent),
            .prin1 = fuv_fs_event_print,
            .princ = fuv_fs_event_print,
            .atomic = fuv_handle_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_FS_POLL] =
        {
			.size = sizeof(FuvValueFsPoll),
            .prin1 = fuv_fs_poll_print,
            .princ = fuv_fs_poll_print,
            .atomic = fuv_handle_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_HANDLE] = {0},

    [UV_IDLE] =
        {
			.size = sizeof(FuvValueIdle),
            .prin1 = fuv_idle_print,
            .princ = fuv_idle_print,
            .atomic = fuv_handle_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_NAMED_PIPE] =
        {
			.size = sizeof(FuvValuePipe),
            .prin1 = fuv_pipe_print,
            .princ = fuv_pipe_print,
            .atomic = fuv_pipe_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_POLL] =
        {
            .size = sizeof(FuvValuePoll),
            .prin1 = fuv_poll_print,
            .princ = fuv_poll_print,
            .atomic = fuv_poll_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_PREPARE] =
        {
            .size = sizeof(FuvValuePrepare),
            .prin1 = fuv_prepare_print,
            .princ = fuv_prepare_print,
            .atomic = fuv_handle_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_PROCESS] =
        {
			.size = sizeof(FuvValueProcess),
            .prin1 = fuv_process_print,
            .princ = fuv_process_print,
            .atomic = fuv_process_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_STREAM] = {0},

    [UV_TCP] =
        {
			.size = sizeof(FuvValueTcp),
            .prin1 = fuv_tcp_print,
            .princ = fuv_tcp_print,
            .atomic = fuv_handle_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_TIMER] =
        {
            .size = sizeof(FuvValueTimer),
            .prin1 = fuv_timer_print,
            .princ = fuv_timer_print,
            .atomic = fuv_handle_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_TTY] =
        {
            .size = sizeof(FuvValueTty),
            .prin1 = fuv_tty_print,
            .princ = fuv_tty_print,
            .atomic = fuv_tty_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_UDP] =
        {
            .size = sizeof(FuvValueUdp),
            .prin1 = fuv_udp_print,
            .princ = fuv_udp_print,
            .atomic = fuv_handle_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_SIGNAL] =
        {
            .size = sizeof(FuvValueSignal),
            .prin1 = fuv_signal_print,
            .princ = fuv_signal_print,
            .atomic = fuv_handle_ud_atomic,
            .finalize = fuv_handle_ud_finalize,
        },

    [UV_FILE] = {0},
};

static void close_callback_wrapper(uv_handle_t *handle) {
    FuvValueHandle *fuv_handle = uv_handle_get_data(handle);
    if (fuv_handle->data.loop) {
        fuvLoopRemoveHandleRef(fuv_handle->data.loop, fuv_handle);
    }
    if (fuv_handle->data.close_callback)
        fuv_handle->data.close_callback(handle);
    fuv_handle->data.close_callback = NULL;
}

void fuvHandleClose(FuvValueHandle *handle, uv_close_cb cb) {
    if (uv_handle_get_loop(&handle->handle)) {
        handle->data.close_callback = cb;
        uv_close(&handle->handle, close_callback_wrapper);
    } else {
        fuvLoopRemoveHandleRef(handle->data.loop, handle);
    }
}

int isFuvHandle(const FklVMvalue *v) {
    if (FKL_IS_USERDATA(v)) {
        const FklVMudMetaTable *t = FKL_VM_UD(v)->mt_;
        return t > &HandleMetaTables[UV_UNKNOWN_HANDLE]
            && t < &HandleMetaTables[UV_HANDLE_TYPE_MAX];
    }
    return 0;
}

static inline void init_fuv_handle(FuvValueHandle *handle, FklVMvalue *loop) {
    handle->data.loop = loop;
    handle->data.callbacks[0] = NULL;
    handle->data.callbacks[1] = NULL;
    uv_handle_set_data(&handle->handle, handle);
    fuvLoopAddHandleRef(loop, handle);
}

#define FUV_HANDLE_P(NAME, ENUM)                                               \
    int NAME(const FklVMvalue *v) {                                            \
        return FKL_IS_USERDATA(v)                                              \
            && FKL_VM_UD(v)->mt_ == &HandleMetaTables[ENUM];                   \
    }

#define FUV_HANDLE_CREATOR(TYPE, NAME, ENUM)                                   \
    FklVMvalue *createFuv##TYPE(FklVM *vm,                                     \
            FklVMvalue *dll,                                                   \
            FklVMvalue *loop_obj,                                              \
            int *err) {                                                        \
        FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[ENUM], dll);  \
        FuvValue##TYPE *handle = FKL_TYPE_CAST(FuvValue##TYPE *, v);           \
        FuvValueLoop *loop = FUV_LOOP(loop_obj);                               \
        init_fuv_handle(FUV_HANDLE(v), loop_obj);                              \
        *err = uv_##NAME##_init(&loop->loop, &handle->handle);                 \
        return v;                                                              \
    }

FUV_HANDLE_P(isFuvTimer, UV_TIMER);
FUV_HANDLE_CREATOR(Timer, timer, UV_TIMER);

FUV_HANDLE_P(isFuvPrepare, UV_PREPARE);
FUV_HANDLE_CREATOR(Prepare, prepare, UV_PREPARE);

FUV_HANDLE_P(isFuvIdle, UV_IDLE);
FUV_HANDLE_CREATOR(Idle, idle, UV_IDLE);

FUV_HANDLE_P(isFuvCheck, UV_CHECK);
FUV_HANDLE_CREATOR(Check, check, UV_CHECK);

FUV_HANDLE_P(isFuvSignal, UV_SIGNAL);
FUV_HANDLE_CREATOR(Signal, signal, UV_SIGNAL);

FUV_HANDLE_P(isFuvAsync, UV_ASYNC);

struct AsyncCreatorArgs {
    FuvValueAsync *async_handle;
    size_t count;
    FklVMvalue **values;
};

static void fuv_async_cb_creator(FklVM *exe, void *args) {
    struct AsyncCreatorArgs *a = FKL_TYPE_CAST(struct AsyncCreatorArgs *, args);
    FuvValueAsync *async_handle = a->async_handle;

    FklVMvalue **cur = a->values;
    FklVMvalue **const end = &cur[a->count];
    for (; cur < end; ++cur)
        FKL_VM_PUSH_VALUE(exe, *cur);
    FUV_ASYNC_COPY_DONE(async_handle);
    FUV_ASYNC_WAIT_SEND(exe, async_handle);
}

static void fuv_async_cb(uv_async_t *handle) {
    uv_loop_t *loop = uv_handle_get_loop((uv_handle_t *)handle);
    FuvLoopData *ldata = uv_loop_get_data(loop);
    FuvValueAsync *async_handle = uv_handle_get_data((uv_handle_t *)handle);
    FuvHandleData *hdata = &async_handle->data;
    FklVMvalue *proc = hdata->callbacks[0];
    if (proc) {
        FklVM *exe = ldata->exe;
        FKL_VM_LOCK_BLOCK(exe, flag) {

            struct FuvAsyncExtraData *extra = atomic_load(&async_handle->extra);
            FklVMrecoverArgs recover_args = { 0 };

            struct AsyncCreatorArgs args = {
                .async_handle = async_handle,
                .count = extra->num,
                .values = extra->base,
            };

            FklVMcallResult r = fklVMcall3(fklRunVM,
                    exe,
                    &recover_args,
                    proc,
                    fuv_async_cb_creator,
                    &args);

            if (r.err) {
                startErrorHandle(loop, ldata, exe);
            } else {
                fklVMrecover(exe, &recover_args);
            }
        }
        return;
    }
}

FklVMvalue *createFuvAsync(FklVM *vm,
        FklVMvalue *dll,
        FklVMvalue *loop_obj,
        FklVMvalue *proc_obj,
        int *err) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[UV_ASYNC], dll);
    FuvValueAsync *handle = FKL_TYPE_CAST(FuvValueAsync *, v);
    FuvValueLoop *loop = FKL_TYPE_CAST(FuvValueLoop *, loop_obj);
    handle->extra = NULL;
    handle->send_ready = (atomic_flag)ATOMIC_FLAG_INIT;
    handle->copy_done = (atomic_flag)ATOMIC_FLAG_INIT;
    atomic_flag_test_and_set(&handle->copy_done);
    atomic_flag_test_and_set(&handle->send_done);
    init_fuv_handle(FUV_HANDLE(v), loop_obj);
    handle->data.callbacks[0] = proc_obj;
    *err = uv_async_init(&loop->loop, &handle->handle, fuv_async_cb);
    return v;
}

FUV_HANDLE_P(isFuvProcess, UV_PROCESS);

uv_process_t *createFuvProcess(FklVM *vm,
        FklVMvalue **pr,
        FklVMvalue *dll,
        FklVMvalue *loop_obj,
        FklVMvalue *proc_obj,
        FklVMvalue *args_obj,
        FklVMvalue *env_obj,
        FklVMvalue *file_obj,
        FklVMvalue *stdio_obj,
        FklVMvalue *cwd_obj) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[UV_PROCESS], dll);
    FuvValueProcess *handle = FKL_TYPE_CAST(FuvValueProcess *, v);
    init_fuv_handle(FUV_HANDLE(v), loop_obj);
    handle->data.callbacks[0] = proc_obj;
    handle->args_obj = args_obj;
    handle->env_obj = env_obj;
    handle->file_obj = file_obj;
    handle->stdio_obj = stdio_obj;
    handle->cwd_obj = cwd_obj;
    *pr = v;
    return &handle->handle;
}

int isFuvStream(const FklVMvalue *v) {
    if (FKL_IS_USERDATA(v)) {
        const FklVMudMetaTable *t = FKL_VM_UD(v)->mt_;
        return t == &HandleMetaTables[UV_NAMED_PIPE]
            || t == &HandleMetaTables[UV_TTY] || t == &HandleMetaTables[UV_TCP]
            || t == &HandleMetaTables[UV_STREAM];
    }
    return 0;
}

FUV_HANDLE_P(isFuvPipe, UV_NAMED_PIPE);

#define OTHER_HANDLE_CREATOR(TYPE, NAME, ENUM)                                 \
    uv_##NAME##_t *createFuv##TYPE(FklVM *vm,                                  \
            FklVMvalue **pr,                                                   \
            FklVMvalue *dll,                                                   \
            FklVMvalue *loop_obj) {                                            \
        FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[ENUM], dll);  \
        FuvValue##TYPE *handle = FKL_TYPE_CAST(FuvValue##TYPE *, v);           \
        init_fuv_handle(FUV_HANDLE(v), loop_obj);                              \
        *pr = v;                                                               \
        return &handle->handle;                                                \
    }

OTHER_HANDLE_CREATOR(Pipe, pipe, UV_NAMED_PIPE);

FUV_HANDLE_P(isFuvTcp, UV_TCP);

OTHER_HANDLE_CREATOR(Tcp, tcp, UV_TCP);

FUV_HANDLE_P(isFuvTty, UV_TTY);

uv_tty_t *createFuvTty(FklVM *vm,
        FklVMvalue **pr,
        FklVMvalue *dll,
        FklVMvalue *loop_obj,
        FklVMvalue *fp_obj) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[UV_TTY], dll);
    FuvValueTty *handle = FKL_TYPE_CAST(FuvValueTty *, v);
    init_fuv_handle(FUV_HANDLE(v), loop_obj);
    handle->fp = fp_obj;
    *pr = v;
    return &handle->handle;
}

FUV_HANDLE_P(isFuvUdp, UV_UDP);

uv_udp_t *createFuvUdp(FklVM *vm,
        FklVMvalue **pr,
        FklVMvalue *dll,
        FklVMvalue *loop_obj,
        int64_t mmsg_num_msgs) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[UV_UDP], dll);
    FuvValueUdp *handle = FKL_TYPE_CAST(FuvValueUdp *, v);
    init_fuv_handle(FUV_HANDLE(v), loop_obj);
    handle->mmsg_num_msgs = mmsg_num_msgs;
    *pr = v;
    return &handle->handle;
}

FUV_HANDLE_P(isFuvFsPoll, UV_FS_POLL);
FUV_HANDLE_CREATOR(FsPoll, fs_poll, UV_FS_POLL);

FUV_HANDLE_P(isFuvFsEvent, UV_FS_EVENT);
FUV_HANDLE_CREATOR(FsEvent, fs_event, UV_FS_EVENT);

FUV_HANDLE_P(isFuvPoll, UV_POLL);

uv_poll_t *createFuvPoll(FklVM *vm,
        FklVMvalue **pr,
        FklVMvalue *dll,
        FklVMvalue *loop_obj,
        FklVMvalue *fp_obj) {
    FklVMvalue *v = fklCreateVMvalueUd(vm, &HandleMetaTables[UV_POLL], dll);
    FuvValuePoll *handle = FKL_TYPE_CAST(FuvValuePoll *, v);
    init_fuv_handle(FUV_HANDLE(v), loop_obj);
    handle->fp = fp_obj;
    *pr = v;
    return &handle->handle;
}

#undef FUV_HANDLE_P
#undef FUV_HANDLE_CREATOR
#undef OTHER_HANDLE_CREATOR
