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
#define XX(A, B, C) FklVMvalue *A##_sid;
    FUV_SYMBOLS_MAP(XX)
#undef XX
} FuvPublicData;

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
    uv_loop_t loop;
    FuvLoopData data;
} FuvLoop;

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
    FuvHandleData data;
    uv_handle_t handle;
} FuvHandle;

typedef struct FuvAsync {
    FuvHandleData data;
    uv_async_t handle;
    _Atomic(struct FuvAsyncExtraData *) extra;
    atomic_flag send_ready;
    atomic_flag copy_done;
    atomic_flag send_done;
} FuvAsync;

typedef struct FuvProcess {
    FuvHandleData data;
    uv_process_t handle;
    FklVMvalue *args_obj;
    FklVMvalue *env_obj;
    FklVMvalue *file_obj;
    FklVMvalue *stdio_obj;
    FklVMvalue *cwd_obj;
} FuvProcess;

typedef struct FuvPipe {
    FuvHandleData data;
    uv_pipe_t handle;
    FklVMvalue *fp;
} FuvPipe;

typedef struct FuvPoll {
    FuvHandleData data;
    uv_poll_t handle;
    FklVMvalue *fp;
} FuvPoll;

typedef struct FuvTty {
    FuvHandleData data;
    uv_tty_t handle;
    FklVMvalue *fp;
} FuvTty;

typedef struct FuvTimer {
    FuvHandleData data;
    uv_timer_t handle;
} FuvTimer;

typedef struct FuvPrepare {
    FuvHandleData data;
    uv_prepare_t handle;
} FuvPrepare;

typedef struct FuvIdle {
    FuvHandleData data;
    uv_idle_t handle;
} FuvIdle;

typedef struct FuvCheck {
    FuvHandleData data;
    uv_check_t handle;
} FuvCheck;

typedef struct FuvSignal {
    FuvHandleData data;
    uv_signal_t handle;
} FuvSignal;

typedef struct FuvTcp {
    FuvHandleData data;
    uv_tcp_t handle;
} FuvTcp;

typedef struct FuvUdp {
    FuvHandleData data;
    uv_udp_t handle;
    int64_t mmsg_num_msgs;
} FuvUdp;

typedef struct FuvFsPoll {
    FuvHandleData data;
    uv_fs_poll_t handle;
} FuvFsPoll;

typedef struct FuvFsEvent {
    FuvHandleData data;
    uv_fs_event_t handle;
} FuvFsEvent;

int isFuvLoop(const FklVMvalue *v);
FklVMvalue *createFuvLoop(FklVM *, FklVMvalue *dll, int *err);
void startErrorHandle(uv_loop_t *loop, FuvLoopData *ldata, FklVM *exe);
void fuvLoopAddGcObj(FklVMvalue *looop, FklVMvalue *obj);

static inline int fuvLoopIsClosed(const FuvLoop *l) {
    return l->data.is_closed;
}
static inline void fuvLoopSetClosed(FuvLoop *l) {
    l->data.mode = -1;
    l->data.is_closed = 1;
    l->data.error_occured = 0;
}

void fuvHandleClose(FuvHandle *h, uv_close_cb cb);
int isFuvHandle(const FklVMvalue *v);

static inline int isFuvHandleClosed(const FuvHandle *h) {
    return h->data.loop == NULL;
}

static inline FuvHandle *getFuvHandle(const FklVMvalue *v) {
    FKL_ASSERT(isFuvHandle(v));
    FKL_DECL_VM_UD_DATA(handle, FuvHandle, v);
    return handle;
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

typedef struct {
    FklVMvalue *prev;
    FklVMvalue *next;
    FklVMvalue *loop;
    FklVMvalue *callback;
    FklVMvalue *write_data;
} FuvReqData;

typedef struct {
    FuvReqData data;
    uv_req_t req;
} FuvReq;

typedef struct FuvWrite {
    FuvReqData data;
    uv_write_t req;
    FklVMvalue *write_objs[1];
} FuvWrite;

typedef struct FuvUdpSend {
    FuvReqData data;
    uv_udp_send_t req;
    FklVMvalue *send_objs[1];
} FuvUdpSend;

typedef struct FuvDir {
    uv_dir_t *dir;
    FklVMvalue *req;
} FuvDir;

typedef struct FuvFsReq {
    FuvReqData data;
    uv_fs_t req;
    FklVMvalue *dest_path;
    size_t nentries;
    FklVMvalue *dir;
    uv_buf_t buf;
    char base[1];
} FuvFsReq;

typedef struct FuvConnect {
    FuvReqData data;
    uv_connect_t req;
} FuvConnect;

typedef struct FuvShutdown {
    FuvReqData data;
    uv_shutdown_t req;
} FuvShutdown;

typedef struct FuvGetaddrinfo {
    FuvReqData data;
    uv_getaddrinfo_t req;
} FuvGetaddrinfo;

typedef struct FuvGetnameinfo {
    FuvReqData data;
    uv_getnameinfo_t req;
} FuvGetnameinfo;

typedef struct FuvRandom {
    FuvReqData data;
    uv_random_t req;
    uint8_t buf[1];
} FuvRandom;

typedef struct FuvFsReqArgs {
    FklVMvalue *dir_obj;
    size_t len;
    const char *str;
} FuvFsReqArgs;

FKL_VM_DEF_UD_STRUCT(FuvValueLoop, { FuvLoop l; });

FKL_VM_DEF_UD_STRUCT(FuvValueHandle, { FuvHandle h; });
FKL_VM_DEF_UD_STRUCT(FuvValueAsync, { FuvAsync h; });
FKL_VM_DEF_UD_STRUCT(FuvValueCheck, { FuvCheck h; });
FKL_VM_DEF_UD_STRUCT(FuvValueFsEvent, { FuvFsEvent h; });
FKL_VM_DEF_UD_STRUCT(FuvValueFsPoll, { FuvFsPoll h; });
FKL_VM_DEF_UD_STRUCT(FuvValueIdle, { FuvIdle h; });
FKL_VM_DEF_UD_STRUCT(FuvValuePipe, { FuvPipe h; });
FKL_VM_DEF_UD_STRUCT(FuvValuePoll, { FuvPoll h; });
FKL_VM_DEF_UD_STRUCT(FuvValuePrepare, { FuvPrepare h; });
FKL_VM_DEF_UD_STRUCT(FuvValueProcess, { FuvProcess h; });
FKL_VM_DEF_UD_STRUCT(FuvValueTcp, { FuvTcp h; });
FKL_VM_DEF_UD_STRUCT(FuvValueTimer, { FuvTimer h; });
FKL_VM_DEF_UD_STRUCT(FuvValueTty, { FuvTty h; });
FKL_VM_DEF_UD_STRUCT(FuvValueUdp, { FuvUdp h; });
FKL_VM_DEF_UD_STRUCT(FuvValueSignal, { FuvSignal h; });

FKL_VM_DEF_UD_STRUCT(FuvValueReq, { FuvReq r; });
FKL_VM_DEF_UD_STRUCT(FuvValueConnect, { FuvConnect r; });
FKL_VM_DEF_UD_STRUCT(FuvValueWrite, { FuvWrite r; });
FKL_VM_DEF_UD_STRUCT(FuvValueShutdown, { FuvShutdown r; });
FKL_VM_DEF_UD_STRUCT(FuvValueUdpSend, { FuvUdpSend r; });
FKL_VM_DEF_UD_STRUCT(FuvValueFsReq, { FuvFsReq r; });
FKL_VM_DEF_UD_STRUCT(FuvValueGetaddrinfo, { FuvGetaddrinfo r; });
FKL_VM_DEF_UD_STRUCT(FuvValueGetnameinfo, { FuvGetnameinfo r; });
FKL_VM_DEF_UD_STRUCT(FuvValueRandom, { FuvRandom r; });

FKL_VM_DEF_UD_STRUCT(FuvValueDir, { FuvDir d; });

int isFuvReq(const FklVMvalue *v);
void fuvReqCleanUp(FuvReq *req);
static inline int isFuvReqCanceled(const FuvReq *req) {
    return req->data.loop == NULL;
}

static inline FuvReq *getFuvReq(const FklVMvalue *v) {
    FKL_ASSERT(isFuvReq(v));
    FKL_DECL_VM_UD_DATA(req, FuvReq, v);
    return req;
}

typedef enum FuvFsReqCleanUpOption {
    FUV_FS_REQ_CLEANUP_IN_FINALIZING = 0,
    FUV_FS_REQ_CLEANUP_NOT_IN_FINALIZING,
} FuvFsReqCleanUpOption;

static inline FklVMvalue *fuvReqValueOf(FuvReq *req) {
    return FKL_VM_VALUE_OF(FKL_VM_UDATA_OF(req));
}

static inline FklVMvalue *fuvHandleValueOf(FuvHandle *req) {
    return FKL_VM_VALUE_OF(FKL_VM_UDATA_OF(req));
}

void fuvLoopAddHandleRef(FklVMvalue *loop, FuvHandle *handle);
void fuvLoopAddReqRef(FklVMvalue *loop, FuvReq *handle);

void fuvLoopRemoveHandleRef(FklVMvalue *loop, FuvHandle *handle);
void fuvLoopRemoveReqRef(FklVMvalue *loop, FuvReq *handle);

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

struct FuvFsReq *createFuvFsReq(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        const FuvFsReqArgs *args);

void fuvFsReqCleanUp(FuvFsReq *req, FuvFsReqCleanUpOption);

int isFuvRandom(const FklVMvalue *v);
struct FuvRandom *createFuvRandom(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        size_t buflen);

int isFuvDir(const FklVMvalue *v);
FklVMvalue *
createFuvDir(FklVM *vm, FklVMvalue *dll, uv_fs_t *dir, size_t nentries);

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
