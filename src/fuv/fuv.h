#ifndef FKL_FUV_FUV_H
#define FKL_FUV_FUV_H

#include <fakeLisp/common.h>
#include <fakeLisp/vm.h>

#include <uv.h>

#include <stdnoreturn.h>

#include "fuv-macros.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {

#define XX(A, B, C) A,
    FUV_ERROR_MAP(XX)
#undef XX
} FuvErrorType;

typedef struct {
    FklVM *exe;
    FklVMvalue *refs;
    FklVMvalue *gclist;
    int mode;
    int8_t is_closed;
    int8_t error_occured;
} FuvLoopData;

typedef enum {
    FUV_RUN_OK = 1,
    FUV_RUN_ERR_OCCUR,
} FuvLoopRunStatus;

typedef struct {
    FklVMvalue *prev;
    FklVMvalue *next;
    FklVMvalue *loop;
    FklVMvalue *callbacks[2];
    uv_close_cb close_callback;
} FuvHandleData;

struct FuvAsyncExtraData {
    uint32_t num;
    FklVMvalue **base;
};

typedef struct {
    FklVMvalue *prev;
    FklVMvalue *next;
    FklVMvalue *loop;
    FklVMvalue *callback;
    FklVMvalue *write_data;
} FuvReqData;

typedef struct FuvFsReqArgs {
    FklVMvalue *dir_obj;
    size_t len;
    const char *str;
} FuvFsReqArgs;

FKL_VM_DEF_UD_STRUCT(FuvValueLoop, {
    uv_loop_t loop;
    FuvLoopData data;
});

#define FUV_DEF_HANDLE(NAME, UV_TYPE_NAME, OTHER_MEMBERS)                      \
    FKL_VM_DEF_UD_STRUCT(NAME, {                                               \
        FuvHandleData data;                                                    \
        uv_##UV_TYPE_NAME##_t handle;                                          \
        struct OTHER_MEMBERS;                                                  \
    });

FUV_DEF_HANDLE(FuvValueHandle, handle, {});
FUV_DEF_HANDLE(FuvValueAsync, async, {
    _Atomic(struct FuvAsyncExtraData *) extra;
    atomic_flag send_ready;
    atomic_flag copy_done;
    atomic_flag send_done;
});
FUV_DEF_HANDLE(FuvValueCheck, check, {});
FUV_DEF_HANDLE(FuvValueFsEvent, fs_event, {});
FUV_DEF_HANDLE(FuvValueFsPoll, fs_poll, {});
FUV_DEF_HANDLE(FuvValueIdle, idle, {});
FUV_DEF_HANDLE(FuvValuePipe, pipe, { FklVMvalue *fp; });
FUV_DEF_HANDLE(FuvValuePoll, poll, { FklVMvalue *fp; });
FUV_DEF_HANDLE(FuvValuePrepare, prepare, {});
FUV_DEF_HANDLE(FuvValueProcess, process, {
    FklVMvalue *args_obj;
    FklVMvalue *env_obj;
    FklVMvalue *file_obj;
    FklVMvalue *stdio_obj;
    FklVMvalue *cwd_obj;
});
FUV_DEF_HANDLE(FuvValueTcp, tcp, {});
FUV_DEF_HANDLE(FuvValueTimer, timer, {});
FUV_DEF_HANDLE(FuvValueTty, tty, { FklVMvalue *fp; })
FUV_DEF_HANDLE(FuvValueUdp, udp, { int64_t mmsg_num_msgs; })
FUV_DEF_HANDLE(FuvValueSignal, signal, {})

#define FUV_DEF_REQ(NAME, UV_TYPE_NAME, OTHER_MEMBERS)                         \
    FKL_VM_DEF_UD_STRUCT(NAME, {                                               \
        FuvReqData data;                                                       \
        uv_##UV_TYPE_NAME##_t req;                                             \
        struct OTHER_MEMBERS;                                                  \
    });

FUV_DEF_REQ(FuvValueReq, req, {});
FUV_DEF_REQ(FuvValueConnect, connect, {});
FUV_DEF_REQ(FuvValueWrite, write, { FklVMvalue *write_objs[1]; });
FUV_DEF_REQ(FuvValueShutdown, shutdown, {});
FUV_DEF_REQ(FuvValueUdpSend, udp_send, { FklVMvalue *send_objs[1]; });
FUV_DEF_REQ(FuvValueFsReq, fs, {
    FklVMvalue *dest_path;
    size_t nentries;
    FklVMvalue *dir;
    uv_buf_t buf;
    char base[1];
});
FUV_DEF_REQ(FuvValueGetaddrinfo, getaddrinfo, {});
FUV_DEF_REQ(FuvValueGetnameinfo, getnameinfo, {});
FUV_DEF_REQ(FuvValueRandom, random, { uint8_t buf[1]; });

FKL_VM_DEF_UD_STRUCT(FuvValueDir, {
    uv_dir_t *dir;
    FklVMvalue *req;
});

int isFuvLoop(const FklVMvalue *v);
FklVMvalue *createFuvLoop(FklVM *, FklVMvalue *dll, int *err);
void startErrorHandle(uv_loop_t *loop, FuvLoopData *ldata, FklVM *exe);
void fuvLoopAddGcObj(FklVMvalue *looop, FklVMvalue *obj);

static FKL_ALWAYS_INLINE FuvValueLoop *FUV_LOOP(const FklVMvalue *v) {
    FKL_ASSERT(isFuvLoop(v));
    return FKL_TYPE_CAST(FuvValueLoop *, v);
}

static inline int fuvLoopIsClosed(const FuvValueLoop *l) {
    return l->data.is_closed;
}
static inline void fuvLoopSetClosed(FuvValueLoop *l) {
    l->data.mode = -1;
    l->data.is_closed = 1;
    l->data.error_occured = 0;
}

void fuvHandleClose(FuvValueHandle *h, uv_close_cb cb);
int isFuvHandle(const FklVMvalue *v);

static inline int isFuvHandleClosed(const FuvValueHandle *h) {
    return h->data.loop == NULL;
}

int isFuvTimer(const FklVMvalue *v);
FklVMvalue *
createFuvTimer(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvPrepare(const FklVMvalue *v);
FklVMvalue *
createFuvPrepare(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvIdle(const FklVMvalue *v);
FklVMvalue *createFuvIdle(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvCheck(const FklVMvalue *v);
FklVMvalue *
createFuvCheck(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvSignal(const FklVMvalue *v);
FklVMvalue *
createFuvSignal(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvAsync(const FklVMvalue *v);
FklVMvalue *createFuvAsync(FklVM *,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *proc_obj,
        int *err);

int isFuvProcess(const FklVMvalue *v);
uv_process_t *createFuvProcess(FklVM *,
        FklVMvalue **pr,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *proc_obj,
        FklVMvalue *args_obj,
        FklVMvalue *env_obj,
        FklVMvalue *file_obj,
        FklVMvalue *stdio_obj,
        FklVMvalue *cwd_obj);

int isFuvStream(const FklVMvalue *v);
int isFuvPipe(const FklVMvalue *v);
uv_pipe_t *
createFuvPipe(FklVM *, FklVMvalue **pr, FklVMvalue *dll, FklVMvalue *loop_obj);

int isFuvTcp(const FklVMvalue *v);
uv_tcp_t *
createFuvTcp(FklVM *, FklVMvalue **pr, FklVMvalue *dll, FklVMvalue *loop_obj);

int isFuvTty(const FklVMvalue *v);
uv_tty_t *createFuvTty(FklVM *,
        FklVMvalue **pr,
        FklVMvalue *dll,
        FklVMvalue *loop_obj,
        FklVMvalue *fp_obj);

int isFuvUdp(const FklVMvalue *v);
uv_udp_t *createFuvUdp(FklVM *,
        FklVMvalue **pr,
        FklVMvalue *dll,
        FklVMvalue *loop_obj,
        int64_t);

int isFuvFsPoll(const FklVMvalue *v);
FklVMvalue *
createFuvFsPoll(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvFsEvent(const FklVMvalue *v);
FklVMvalue *
createFuvFsEvent(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvPoll(const FklVMvalue *v);
uv_poll_t *createFuvPoll(FklVM *vm,
        FklVMvalue **pr,
        FklVMvalue *dll,
        FklVMvalue *loop_obj,
        FklVMvalue *fp_obj);

#define FUV_ASYNC_COPY_DONE(async_handle)                                      \
    atomic_flag_clear(&(async_handle)->copy_done)
#define FUV_ASYNC_SEND_DONE(async_handle)                                      \
    {                                                                          \
        atomic_flag_clear(&(async_handle)->send_done);                         \
        atomic_flag_clear(&(async_handle)->send_ready);                        \
    }

#define FUV_ASYNC_WAIT_COPY(exe, async_handle)                                 \
    {                                                                          \
        fklUnlockThread(exe);                                                  \
        while (atomic_flag_test_and_set(&async_handle->copy_done))             \
            uv_sleep(0);                                                       \
        fklLockThread(exe);                                                    \
    }

#define FUV_ASYNC_WAIT_SEND(exe, async_handle)                                 \
    {                                                                          \
        fklUnlockThread(exe);                                                  \
        while (atomic_flag_test_and_set(&async_handle->send_done))             \
            uv_sleep(0);                                                       \
        fklLockThread(exe);                                                    \
    }

int isFuvReq(const FklVMvalue *v);
void fuvReqCleanUp(FuvValueReq *req);
static inline int isFuvReqCanceled(const FuvValueReq *req) {
    return req->data.loop == NULL;
}

static inline FuvValueReq *FUV_REQ(const FklVMvalue *v) {
    FKL_ASSERT(isFuvReq(v));
    return FKL_TYPE_CAST(FuvValueReq *, v);
}

static inline FuvValueHandle *FUV_HANDLE(const FklVMvalue *v) {
    FKL_ASSERT(isFuvHandle(v));
    return FKL_TYPE_CAST(FuvValueHandle *, v);
}

typedef enum FuvFsReqCleanUpOption {
    FUV_FS_REQ_CLEANUP_IN_FINALIZING = 0,
    FUV_FS_REQ_CLEANUP_NOT_IN_FINALIZING,
} FuvFsReqCleanUpOption;

void fuvLoopAddHandleRef(FklVMvalue *loop, FuvValueHandle *handle);
void fuvLoopAddReqRef(FklVMvalue *loop, FuvValueReq *handle);

void fuvLoopRemoveHandleRef(FklVMvalue *loop, FuvValueHandle *handle);
void fuvLoopRemoveReqRef(FklVMvalue *loop, FuvValueReq *handle);

int isFuvGetaddrinfo(const FklVMvalue *v);
uv_getaddrinfo_t *createFuvGetaddrinfo(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback);

int isFuvGetnameinfo(const FklVMvalue *v);
uv_getnameinfo_t *createFuvGetnameinfo(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback);

int isFuvWrite(const FklVMvalue *);
uv_write_t *createFuvWrite(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        uint32_t count);

int isFuvShutdown(const FklVMvalue *);
uv_shutdown_t *createFuvShutdown(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback);

int isFuvConnect(const FklVMvalue *);
uv_connect_t *createFuvConnect(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback);

int isFuvUdpSend(const FklVMvalue *);
uv_udp_send_t *createFuvUdpSend(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        uint32_t count);

int isFuvFsReq(const FklVMvalue *v);

FuvValueFsReq *createFuvFsReq(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        const FuvFsReqArgs *args);

void fuvFsReqCleanUp(FuvValueFsReq *req, FuvFsReqCleanUpOption);

int isFuvRandom(const FklVMvalue *v);
FuvValueRandom *createFuvRandom(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        size_t buflen);

int isFuvDir(const FklVMvalue *v);
FklVMvalue *
createFuvDir(FklVM *vm, FklVMvalue *dll, uv_fs_t *dir, size_t nentries);

static FKL_ALWAYS_INLINE FuvValueDir *FUV_DIR(const FklVMvalue *v) {
    FKL_ASSERT(isFuvDir(v));
    return FKL_TYPE_CAST(FuvValueDir *, v);
}

#define FUV_DIR_CLEANUP_NONE (0)
#define FUV_DIR_CLEANUP_FREE_DIRENTS (1)
#define FUV_DIR_CLEANUP_CLOSE_DIR (2)
#define FUV_DIR_CLEANUP_ALL                                                    \
    (FUV_DIR_CLEANUP_CLOSE_DIR | FUV_DIR_CLEANUP_FREE_DIRENTS)

static inline void cleanUpDir(uv_dir_t *d, int cleanup_opt) {
    if (d == NULL)
        return;
    if ((cleanup_opt & FUV_DIR_CLEANUP_FREE_DIRENTS) && d->dirents) {
        d->nentries = 0;
        fklZfree(d->dirents);
        d->dirents = NULL;
    }
    if ((cleanup_opt & FUV_DIR_CLEANUP_CLOSE_DIR)) {
        uv_fs_t tmp_req;
        uv_fs_closedir(NULL, &tmp_req, d, NULL);
    }
}

int isFuvDirUsing(FklVMvalue *dir);

FklVMvalue *refFuvDir(FklVMvalue *dir, FklVMvalue *req_obj);
#ifdef __cplusplus
}
#endif

#endif
