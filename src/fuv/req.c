#include <fakeLisp/common.h>
#include <fakeLisp/vm.h>
#include <string.h>

#include "fuv.h"

static void fuv_req_ud_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(req, FuvReq, ud);
    fklVMgcToGray(req->data.loop, gc);
    fklVMgcToGray(req->data.callback, gc);
    fklVMgcToGray(req->data.write_data, gc);
}

void fuvReqCleanUp(FuvReq *req) {
    FuvReqData *data = &req->data;
    if (data->loop) {
        data->callback = NULL;
        fuvLoopRemoveReqRef(data->loop, req);
    }
}

void fuvFsReqCleanUp(FuvFsReq *req, FuvFsReqCleanUpOption opt) {
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

static inline int fuv_req_ud_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(req, FuvReq, ud);
    if (req->data.loop) {
        FklVMvalue *v = FKL_VM_VALUE_OF(ud);
        fuvLoopAddGcObj(req->data.loop, v);
        return FKL_VM_UD_FINALIZE_DELAY;
    }
    fuvReqCleanUp(req);
    return FKL_VM_UD_FINALIZE_NOW;
}

static int fuv_fs_req_ud_finalizer(FklVMud *ud) {
    FKL_DECL_UD_DATA(req, FuvFsReq, ud);
    if (fuv_req_ud_finalizer(ud))
        return FKL_VM_UD_FINALIZE_DELAY;
    fuvFsReqCleanUp(req, FUV_FS_REQ_CLEANUP_IN_FINALIZING);
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

static void fuv_write_ud_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(req, struct FuvWrite, ud);
    fklVMgcToGray(req->data.loop, gc);
    fklVMgcToGray(req->data.callback, gc);
    fklVMgcToGray(req->data.write_data, gc);
    for (FklVMvalue **cur = req->write_objs; *cur; cur++)
        fklVMgcToGray(*cur, gc);
}

static void fuv_udp_send_ud_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(req, struct FuvUdpSend, ud);
    fklVMgcToGray(req->data.loop, gc);
    fklVMgcToGray(req->data.callback, gc);
    fklVMgcToGray(req->data.write_data, gc);
    for (FklVMvalue **cur = req->send_objs; *cur; cur++)
        fklVMgcToGray(*cur, gc);
}

static void fuv_fs_req_ud_atomic(const FklVMud *ud, FklVMgc *gc) {
    FKL_DECL_UD_DATA(req, struct FuvFsReq, ud);
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
            .size = sizeof(struct FuvConnect),
            .__as_prin1 = fuv_connect_as_print,
            .__as_princ = fuv_connect_as_print,
            .__atomic = fuv_req_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_WRITE] =
        {
            .size = sizeof(struct FuvWrite),
            .__as_prin1 = fuv_write_as_print,
            .__as_princ = fuv_write_as_print,
            .__atomic = fuv_write_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_SHUTDOWN] =
        {
            .size = sizeof(struct FuvShutdown),
            .__as_prin1 = fuv_shutdown_as_print,
            .__as_princ = fuv_shutdown_as_print,
            .__atomic = fuv_req_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_UDP_SEND] =
        {
            .size = sizeof(struct FuvUdpSend),
            .__as_prin1 = fuv_udp_send_as_print,
            .__as_princ = fuv_udp_send_as_print,
            .__atomic = fuv_udp_send_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_FS] =
        {
            .size = sizeof(struct FuvFsReq),
            .__as_prin1 = fuv_fs_req_as_print,
            .__as_princ = fuv_fs_req_as_print,
            .__atomic = fuv_fs_req_ud_atomic,
            .__finalizer = fuv_fs_req_ud_finalizer,
        },

    [UV_WORK] = {0},

    [UV_GETADDRINFO] =
        {
            .size = sizeof(struct FuvGetaddrinfo),
            .__as_prin1 = fuv_getaddrinfo_as_print,
            .__as_princ = fuv_getaddrinfo_as_print,
            .__atomic = fuv_req_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_GETNAMEINFO] =
        {
            .size = sizeof(struct FuvGetnameinfo),
            .__as_prin1 = fuv_getnameinfo_as_print,
            .__as_princ = fuv_getnameinfo_as_print,
            .__atomic = fuv_req_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },

    [UV_RANDOM] =
        {
            .size = sizeof(struct FuvRandom),
            .__as_prin1 = fuv_random_as_print,
            .__as_princ = fuv_random_as_print,
            .__atomic = fuv_req_ud_atomic,
            .__finalizer = fuv_req_ud_finalizer,
        },
};

int isFuvReq(const FklVMvalue *v) {
    if (FKL_IS_USERDATA(v)) {
        const FklVMudMetaTable *t = FKL_VM_UD(v)->t;
        return t > &ReqMetaTables[UV_UNKNOWN_REQ]
            && t < &ReqMetaTables[UV_REQ_TYPE_MAX];
    }
    return 0;
}

static inline void init_fuv_req(FuvReq *req,
        FklVMvalue *v,
        FklVMvalue *loop,
        FklVMvalue *callback) {
    req->data.loop = loop;
    req->data.callback = callback;
    uv_req_set_data(&req->req, req);
    fuvLoopAddReqRef(loop, req);
}

#define FUV_REQ_P(NAME, ENUM)                                                  \
    int NAME(FklVMvalue *v) {                                                  \
        return FKL_IS_USERDATA(v) && FKL_VM_UD(v)->t == &ReqMetaTables[ENUM];  \
    }

FUV_REQ_P(isFuvGetaddrinfo, UV_GETADDRINFO);

#define NORMAL_REQ_CREATOR(TYPE, NAME, ENUM)                                   \
    uv_##NAME##_t *create##TYPE(FklVM *vm,                                     \
            FklVMvalue **ret,                                                  \
            FklVMvalue *dll,                                                   \
            FklVMvalue *loop,                                                  \
            FklVMvalue *callback) {                                            \
        FklVMvalue *v = fklCreateVMvalueUd(vm, &ReqMetaTables[ENUM], dll);     \
        FKL_DECL_VM_UD_DATA(req, TYPE, v);                                     \
        init_fuv_req(FKL_TYPE_CAST(FuvReq *, req), v, loop, callback);         \
        *ret = v;                                                              \
        return &req->req;                                                      \
    }

NORMAL_REQ_CREATOR(FuvGetaddrinfo, getaddrinfo, UV_GETADDRINFO);

FUV_REQ_P(isFuvGetnameinfo, UV_GETNAMEINFO);

NORMAL_REQ_CREATOR(FuvGetnameinfo, getnameinfo, UV_GETNAMEINFO);

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
    FKL_DECL_VM_UD_DATA(req, FuvWrite, v);
    init_fuv_req(FKL_TYPE_CAST(FuvReq *, req), v, loop, callback);
    *ret = v;
    return &req->req;
}

FUV_REQ_P(isFuvShutdown, UV_SHUTDOWN);

NORMAL_REQ_CREATOR(FuvShutdown, shutdown, UV_SHUTDOWN);

FUV_REQ_P(isFuvConnect, UV_CONNECT);

NORMAL_REQ_CREATOR(FuvConnect, connect, UV_CONNECT);

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
    FKL_DECL_VM_UD_DATA(req, FuvUdpSend, v);
    init_fuv_req(FKL_TYPE_CAST(FuvReq *, req), v, loop, callback);
    *ret = v;
    return &req->req;
}

FUV_REQ_P(isFuvFsReq, UV_FS);

struct FuvFsReq *createFuvFsReq(FklVM *exe,
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
    FKL_DECL_VM_UD_DATA(req, FuvFsReq, v);
    init_fuv_req(FKL_TYPE_CAST(FuvReq *, req), v, loop, callback);
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

struct FuvRandom *createFuvRandom(FklVM *exe,
        FklVMvalue **ret,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        size_t len) {
    FklVMvalue *v = fklCreateVMvalueUd2(exe,
            &ReqMetaTables[UV_RANDOM],
            len * sizeof(uint8_t),
            dll);
    FKL_DECL_VM_UD_DATA(req, FuvRandom, v);
    init_fuv_req(FKL_TYPE_CAST(FuvReq *, req), v, loop, callback);
    *ret = v;
    return req;
}
