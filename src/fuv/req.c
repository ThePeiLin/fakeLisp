#include <fakeLisp/common.h>
#include <fakeLisp/vm.h>
#include <string.h>

#include "fuv.h"

// static FKL_ALWAYS_INLINE FuvValueReq *as_req(const FklVMvalue *v) {
//     FKL_ASSERT(isFuvReq(v));
//     return FKL_TYPE_CAST(FuvValueReq *, v);
// }

static void fuv_req_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(req, FuvReq, ud);
    FuvValueReq *req = FUV_REQ(ud);
    fklVMgcToGray(req->data.loop, gc);
    fklVMgcToGray(req->data.callback, gc);
    fklVMgcToGray(req->data.write_data, gc);
}

void fuvReqCleanUp(FuvValueReq *req) {
    FuvReqData *data = &req->data;
    if (data->loop) {
        data->callback = NULL;
        fuvLoopRemoveReqRef(data->loop, req);
    }
}

void fuvFsReqCleanUp(FuvValueFsReq *req, FuvFsReqCleanUpOption opt) {
    uv_fs_t *fs_req = &req->req;
    if (opt == FUV_FS_REQ_CLEANUP_NOT_IN_FINALIZING && req->dir) {
        refFuvDir(req->dir, NULL);
        req->dir = NULL;
    }
    if (fs_req->fs_type == UV_FS_OPENDIR && fs_req->ptr) {
        cleanUpDir(fs_req->ptr, FUV_DIR_CLEANUP_CLOSE_DIR);
        fs_req->ptr = NULL;
    }
    uv_dir_t *d = fs_req->ptr;
    uv_fs_req_cleanup(fs_req);
    if (req->dir && fs_req->fs_type == UV_FS_READDIR && d) {
        req->dir = NULL;
        fs_req->ptr = NULL;
        cleanUpDir(d, FUV_DIR_CLEANUP_ALL);
    }
}

static inline int fuv_req_ud_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(req, FuvReq, ud);
    FuvValueReq *req = FUV_REQ(ud);
    if (req->data.loop) {
        fuvLoopAddGcObj(req->data.loop, ud);
        return FKL_VM_UD_FINALIZE_DELAY;
    }
    fuvReqCleanUp(req);
    return FKL_VM_UD_FINALIZE_NOW;
}

static FKL_ALWAYS_INLINE FuvValueFsReq *as_fs_req(const FklVMvalue *v) {
    FKL_ASSERT(isFuvFsReq(v));
    return FKL_TYPE_CAST(FuvValueFsReq *, v);
}

static int fuv_fs_req_ud_finalizer(FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(req, FuvFsReq, ud);
    if (fuv_req_ud_finalizer(ud, gc))
        return FKL_VM_UD_FINALIZE_DELAY;
    fuvFsReqCleanUp(as_fs_req(ud), FUV_FS_REQ_CLEANUP_IN_FINALIZING);
    return FKL_VM_UD_FINALIZE_NOW;
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_getaddrinfo_as_print, "getaddrinfo");
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_getnameinfo_as_print, "getnameinfo");
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_write_as_print, "write");
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_shutdown_as_print, "shutdown");
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_connect_as_print, "connect");
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_udp_send_as_print, "udp-send");
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_fs_req_as_print, "fs-req");
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_random_as_print, "random");

static FKL_ALWAYS_INLINE FuvValueWrite *as_write(const FklVMvalue *v) {
    FKL_ASSERT(isFuvWrite(v));
    return FKL_TYPE_CAST(FuvValueWrite *, v);
}

static void fuv_write_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(req, struct FuvWrite, ud);
    FuvValueWrite *req = as_write(ud);
    fklVMgcToGray(req->data.loop, gc);
    fklVMgcToGray(req->data.callback, gc);
    fklVMgcToGray(req->data.write_data, gc);
    for (FklVMvalue **cur = req->write_objs; *cur; cur++)
        fklVMgcToGray(*cur, gc);
}

static FKL_ALWAYS_INLINE FuvValueUdpSend *as_udp_send(const FklVMvalue *v) {
    FKL_ASSERT(isFuvUdpSend(v));
    return FKL_TYPE_CAST(FuvValueUdpSend *, v);
}

static void fuv_udp_send_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(req, struct FuvUdpSend, ud);
    FuvValueUdpSend *req = as_udp_send(ud);
    fklVMgcToGray(req->data.loop, gc);
    fklVMgcToGray(req->data.callback, gc);
    fklVMgcToGray(req->data.write_data, gc);
    for (FklVMvalue **cur = req->send_objs; *cur; cur++)
        fklVMgcToGray(*cur, gc);
}

static void fuv_fs_req_ud_atomic(const FklVMvalue *ud, FklVMgc *gc) {
    // FKL_DECL_UD_DATA(req, struct FuvFsReq, ud);
    FuvValueFsReq *req = as_fs_req(ud);
    fklVMgcToGray(req->data.loop, gc);
    fklVMgcToGray(req->data.callback, gc);
    fklVMgcToGray(req->data.write_data, gc);
    fklVMgcToGray(req->dest_path, gc);
    fklVMgcToGray(req->dir, gc);
}

static const FklVMudMetaTable ReqMetaTables[UV_REQ_TYPE_MAX] = {
    [UV_UNKNOWN_REQ] = {0},

    [UV_REQ] = {0},

    [UV_CONNECT] =
        {
            // .size = sizeof(struct FuvConnect),
			.size = sizeof(FuvValueConnect),
            .__as_prin1 = fuv_connect_as_print,
            .__as_princ = fuv_connect_as_print,
            .__atomic = fuv_req_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_WRITE] =
        {
            // .size = sizeof(struct FuvWrite),
			.size = sizeof(FuvValueWrite),
            .__as_prin1 = fuv_write_as_print,
            .__as_princ = fuv_write_as_print,
            .__atomic = fuv_write_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_SHUTDOWN] =
        {
            // .size = sizeof(struct FuvShutdown),
            .size = sizeof(FuvValueShutdown),
            .__as_prin1 = fuv_shutdown_as_print,
            .__as_princ = fuv_shutdown_as_print,
            .__atomic = fuv_req_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_UDP_SEND] =
        {
            // .size = sizeof(struct FuvUdpSend),
            .size = sizeof(FuvValueUdpSend),
            .__as_prin1 = fuv_udp_send_as_print,
            .__as_princ = fuv_udp_send_as_print,
            .__atomic = fuv_udp_send_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_FS] =
        {
            // .size = sizeof(struct FuvFsReq),
            .size = sizeof(FuvValueFsReq),
            .__as_prin1 = fuv_fs_req_as_print,
            .__as_princ = fuv_fs_req_as_print,
            .__atomic = fuv_fs_req_ud_atomic,
            .__finalizer = fuv_fs_req_ud_finalizer,
        },

    [UV_WORK] = {0},

    [UV_GETADDRINFO] =
        {
            // .size = sizeof(struct FuvGetaddrinfo),
            .size = sizeof(FuvValueGetaddrinfo),
            .__as_prin1 = fuv_getaddrinfo_as_print,
            .__as_princ = fuv_getaddrinfo_as_print,
            .__atomic = fuv_req_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_GETNAMEINFO] =
        {
            // .size = sizeof(struct FuvGetnameinfo),
            .size = sizeof(FuvValueGetnameinfo),
            .__as_prin1 = fuv_getnameinfo_as_print,
            .__as_princ = fuv_getnameinfo_as_print,
            .__atomic = fuv_req_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_RANDOM] =
        {
            // .size = sizeof(struct FuvRandom),
            .size = sizeof(FuvValueRandom),
            .__as_prin1 = fuv_random_as_print,
            .__as_princ = fuv_random_as_print,
            .__atomic = fuv_req_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },
};

int isFuvReq(const FklVMvalue *v) {
    if (FKL_IS_USERDATA(v)) {
        const FklVMudMetaTable *t = FKL_VM_UD(v)->mt_;
        return t > &ReqMetaTables[UV_UNKNOWN_REQ]
            && t < &ReqMetaTables[UV_REQ_TYPE_MAX];
    }
    return 0;
}

static inline void
init_fuv_req(FuvValueReq *req, FklVMvalue *loop, FklVMvalue *callback) {
    req->data.loop = loop;
    req->data.callback = callback;
    uv_req_set_data(&req->req, req);
    fuvLoopAddReqRef(loop, req);
}

#define FUV_REQ_P(NAME, ENUM)                                                  \
    int NAME(const FklVMvalue *v) {                                            \
        return FKL_IS_USERDATA(v)                                              \
            && FKL_VM_UD(v)->mt_ == &ReqMetaTables[ENUM];                      \
    }

FUV_REQ_P(isFuvGetaddrinfo, UV_GETADDRINFO);

#define NORMAL_REQ_CREATOR(TYPE, NAME, ENUM)                                   \
    uv_##NAME##_t *createFuv##TYPE(FklVM *vm,                                  \
            FklVMvalue **ret,                                                  \
            FklVMvalue *dll,                                                   \
            FklVMvalue *loop,                                                  \
            FklVMvalue *callback) {                                            \
        FklVMvalue *v = fklCreateVMvalueUd(vm, &ReqMetaTables[ENUM], dll);     \
        FuvValue##TYPE *req = FKL_TYPE_CAST(FuvValue##TYPE *, v);              \
        init_fuv_req(FUV_REQ(v), loop, callback);                              \
        *ret = v;                                                              \
        return &req->req;                                                      \
    }

NORMAL_REQ_CREATOR(Getaddrinfo, getaddrinfo, UV_GETADDRINFO);

FUV_REQ_P(isFuvGetnameinfo, UV_GETNAMEINFO);

NORMAL_REQ_CREATOR(Getnameinfo, getnameinfo, UV_GETNAMEINFO);

FUV_REQ_P(isFuvWrite, UV_WRITE);

uv_write_t *createFuvWrite(FklVM *exe,
        FklVMvalue **ret,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        uint32_t count) {
    FklVMvalue *v = fklCreateVMvalueUd2(exe,
            &ReqMetaTables[UV_WRITE],
            count * sizeof(FklVMvalue *),
            dll);
    // FKL_DECL_VM_UD_DATA(req, FuvWrite, v);
    FuvValueWrite *req = FKL_TYPE_CAST(FuvValueWrite *, v);
    init_fuv_req(FUV_REQ(v), loop, callback);
    *ret = v;
    return &req->req;
}

FUV_REQ_P(isFuvShutdown, UV_SHUTDOWN);

NORMAL_REQ_CREATOR(Shutdown, shutdown, UV_SHUTDOWN);

FUV_REQ_P(isFuvConnect, UV_CONNECT);

NORMAL_REQ_CREATOR(Connect, connect, UV_CONNECT);

FUV_REQ_P(isFuvUdpSend, UV_UDP_SEND);

uv_udp_send_t *createFuvUdpSend(FklVM *exe,
        FklVMvalue **ret,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        uint32_t count) {
    FklVMvalue *v = fklCreateVMvalueUd2(exe,
            &ReqMetaTables[UV_UDP_SEND],
            count * sizeof(FklVMvalue *),
            dll);
    // FKL_DECL_VM_UD_DATA(req, FuvUdpSend, v);
    FuvValueUdpSend *req = FKL_TYPE_CAST(FuvValueUdpSend *, v);
    init_fuv_req(FUV_REQ(v), loop, callback);
    *ret = v;
    return &req->req;
}

FUV_REQ_P(isFuvFsReq, UV_FS);

FuvValueFsReq *createFuvFsReq(FklVM *exe,
        FklVMvalue **ret,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        const FuvFsReqArgs *args) {

    size_t len = args ? args->len : 0;
    const char *str = args ? args->str : NULL;
    FklVMvalue *dir_obj = args ? args->dir_obj : NULL;

    FklVMvalue *v = fklCreateVMvalueUd2(exe,
            &ReqMetaTables[UV_FS],
            len * sizeof(char),
            dll);
    // FKL_DECL_VM_UD_DATA(req, FuvFsReq, v);
    FuvValueFsReq *req = FKL_TYPE_CAST(FuvValueFsReq *, v);
    init_fuv_req(FUV_REQ(v), loop, callback);
    req->buf = uv_buf_init(req->base, len);
    if (len && str) {
        memcpy(req->buf.base, str, len * sizeof(char));
    }
    if (dir_obj)
        req->dir = refFuvDir(dir_obj, v);
    *ret = v;
    return req;
}

FUV_REQ_P(isFuvRandom, UV_RANDOM);

FuvValueRandom *createFuvRandom(FklVM *exe,
        FklVMvalue **ret,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        size_t len) {
    FklVMvalue *v = fklCreateVMvalueUd2(exe,
            &ReqMetaTables[UV_RANDOM],
            len * sizeof(uint8_t),
            dll);
    // FKL_DECL_VM_UD_DATA(req, FuvRandom, v);
    FuvValueRandom *req = FKL_TYPE_CAST(FuvValueRandom *, v);
    init_fuv_req(FUV_REQ(v), loop, callback);
    *ret = v;
    return req;
}
