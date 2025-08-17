#ifndef FKL_FUV_FUV_H
#define FKL_FUV_FUV_H

#include <fakeLisp/common.h>
#include <fakeLisp/vm.h>
#include <signal.h>
#include <stdnoreturn.h>
#include <uv.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FUV_ERR_DUMMY = 0,
    FUV_ERR_CLOSE_CLOSEING_HANDLE,
    FUV_ERR_HANDLE_CLOSED,
    FUV_ERR_REQ_CANCELED,
    FUV_ERR_CLOSE_USING_DIR,
    FUV_ERR_NUMBER_SHOULD_NOT_BE_LT_1,
    FUV_ERR_NUM,
} FuvErrorType;

typedef struct {
    FklSid_t loop_mode[3];
    FklSid_t fuv_err_sid[FUV_ERR_NUM];
    FklSid_t loop_block_signal_sid;
    FklSid_t metrics_idle_time_sid;
    FklSid_t loop_use_io_uring_sqpoll;

#define XX(code, _) FklSid_t uv_err_sid_##code;
    UV_ERRNO_MAP(XX)
#undef XX

    FklSid_t AI_ADDRCONFIG_sid;
#ifdef AI_V4MAPPED
    FklSid_t AI_V4MAPPED_sid;
#endif
#ifdef AI_ALL
    FklSid_t AI_ALL_sid;
#endif
    FklSid_t AI_NUMERICHOST_sid;
    FklSid_t AI_PASSIVE_sid;
    FklSid_t AI_NUMERICSERV_sid;
    FklSid_t AI_CANONNAME_sid;

    FklSid_t f_ip_sid;
    FklSid_t f_addr_sid;
    FklSid_t f_port_sid;
    FklSid_t f_family_sid;
    FklSid_t f_socktype_sid;
    FklSid_t f_protocol_sid;
    FklSid_t f_canonname_sid;
    FklSid_t f_hostname_sid;
    FklSid_t f_service_sid;

    FklSid_t f_args_sid;
    FklSid_t f_env_sid;
    FklSid_t f_cwd_sid;
    FklSid_t f_stdio_sid;
    FklSid_t f_uid_sid;
    FklSid_t f_gid_sid;

    FklSid_t UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS_sid;
    FklSid_t UV_PROCESS_DETACHED_sid;
    FklSid_t UV_PROCESS_WINDOWS_HIDE_sid;
    FklSid_t UV_PROCESS_WINDOWS_HIDE_CONSOLE_sid;
    FklSid_t UV_PROCESS_WINDOWS_HIDE_GUI_sid;

    FklSid_t UV_TTY_MODE_NORMAL_sid;
    FklSid_t UV_TTY_MODE_RAW_sid;
    FklSid_t UV_TTY_MODE_IO_sid;

    FklSid_t UV_TTY_SUPPORTED_sid;
    FklSid_t UV_TTY_UNSUPPORTED_sid;

    FklSid_t UV_UDP_IPV6ONLY_sid;
    FklSid_t UV_UDP_PARTIAL_sid;
    FklSid_t UV_UDP_REUSEADDR_sid;
    FklSid_t UV_UDP_MMSG_CHUNK_sid;
    FklSid_t UV_UDP_MMSG_FREE_sid;
    FklSid_t UV_UDP_LINUX_RECVERR_sid;
    FklSid_t UV_UDP_RECVMMSG_sid;

    FklSid_t UV_LEAVE_GROUP_sid;
    FklSid_t UV_JOIN_GROUP_sid;

#ifdef AF_UNSPEC
    FklSid_t AF_UNSPEC_sid;
#endif
#ifdef AF_UNIX
    FklSid_t AF_UNIX_sid;
#endif
#ifdef AF_INET
    FklSid_t AF_INET_sid;
#endif
#ifdef AF_INET6
    FklSid_t AF_INET6_sid;
#endif
#ifdef AF_IPX
    FklSid_t AF_IPX_sid;
#endif
#ifdef AF_NETLINK
    FklSid_t AF_NETLINK_sid;
#endif
#ifdef AF_X25
    FklSid_t AF_X25_sid;
#endif
#ifdef AF_AX25
    FklSid_t AF_AX25_sid;
#endif
#ifdef AF_ATMPVC
    FklSid_t AF_ATMPVC_sid;
#endif
#ifdef AF_APPLETALK
    FklSid_t AF_APPLETALK_sid;
#endif
#ifdef AF_PACKET
    FklSid_t AF_PACKET_sid;
#endif

#ifdef SOCK_STREAM
    FklSid_t SOCK_STREAM_sid;
#endif
#ifdef SOCK_DGRAM
    FklSid_t SOCK_DGRAM_sid;
#endif
#ifdef SOCK_SEQPACKET
    FklSid_t SOCK_SEQPACKET_sid;
#endif
#ifdef SOCK_RAW
    FklSid_t SOCK_RAW_sid;
#endif
#ifdef SOCK_RDM
    FklSid_t SOCK_RDM_sid;
#endif

#ifdef SIGHUP
    FklSid_t SIGHUP_sid;
#endif
#ifdef SIGINT
    FklSid_t SIGINT_sid;
#endif
#ifdef SIGQUIT
    FklSid_t SIGQUIT_sid;
#endif
#ifdef SIGILL
    FklSid_t SIGILL_sid;
#endif
#ifdef SIGTRAP
    FklSid_t SIGTRAP_sid;
#endif
#ifdef SIGABRT
    FklSid_t SIGABRT_sid;
#endif
#ifdef SIGIOT
    FklSid_t SIGIOT_sid;
#endif
#ifdef SIGBUS
    FklSid_t SIGBUS_sid;
#endif
#ifdef SIGFPE
    FklSid_t SIGFPE_sid;
#endif
#ifdef SIGKILL
    FklSid_t SIGKILL_sid;
#endif
#ifdef SIGUSR1
    FklSid_t SIGUSR1_sid;
#endif
#ifdef SIGSEGV
    FklSid_t SIGSEGV_sid;
#endif
#ifdef SIGUSR2
    FklSid_t SIGUSR2_sid;
#endif
#ifdef SIGPIPE
    FklSid_t SIGPIPE_sid;
#endif
#ifdef SIGALRM
    FklSid_t SIGALRM_sid;
#endif
#ifdef SIGTERM
    FklSid_t SIGTERM_sid;
#endif
#ifdef SIGCHLD
    FklSid_t SIGCHLD_sid;
#endif
#ifdef SIGSTKFLT
    FklSid_t SIGSTKFLT_sid;
#endif
#ifdef SIGCONT
    FklSid_t SIGCONT_sid;
#endif
#ifdef SIGSTOP
    FklSid_t SIGSTOP_sid;
#endif
#ifdef SIGTSTP
    FklSid_t SIGTSTP_sid;
#endif
#ifdef SIGBREAK
    FklSid_t SIGBREAK_sid;
#endif
#ifdef SIGTTIN
    FklSid_t SIGTTIN_sid;
#endif
#ifdef SIGTTOU
    FklSid_t SIGTTOU_sid;
#endif
#ifdef SIGURG
    FklSid_t SIGURG_sid;
#endif
#ifdef SIGXCPU
    FklSid_t SIGXCPU_sid;
#endif
#ifdef SIGXFSZ
    FklSid_t SIGXFSZ_sid;
#endif
#ifdef SIGVTALRM
    FklSid_t SIGVTALRM_sid;
#endif
#ifdef SIGPROF
    FklSid_t SIGPROF_sid;
#endif
#ifdef SIGWINCH
    FklSid_t SIGWINCH_sid;
#endif
#ifdef SIGIO
    FklSid_t SIGIO_sid;
#endif
#ifdef SIGPOLL
    FklSid_t SIGPOLL_sid;
#endif
#ifdef SIGLOST
    FklSid_t SIGLOST_sid;
#endif
#ifdef SIGPWR
    FklSid_t SIGPWR_sid;
#endif
#ifdef SIGSYS
    FklSid_t SIGSYS_sid;
#endif

#ifdef NI_NAMEREQD
    FklSid_t NI_NAMEREQD_sid;
#endif
#ifdef NI_DGRAM
    FklSid_t NI_DGRAM_sid;
#endif
#ifdef NI_NOFQDN
    FklSid_t NI_NOFQDN_sid;
#endif
#ifdef NI_NUMERICHOST
    FklSid_t NI_NUMERICHOST_sid;
#endif
#ifdef NI_NUMERICSERV
    FklSid_t NI_NUMERICSERV_sid;
#endif
#ifdef NI_IDN
    FklSid_t NI_IDN_sid;
#endif
#ifdef NI_IDN_ALLOW_UNASSIGNED
    FklSid_t NI_IDN_ALLOW_UNASSIGNED_sid;
#endif
#ifdef NI_IDN_USE_STD3_ASCII_RULES
    FklSid_t NI_IDN_USE_STD3_ASCII_RULES_sid;
#endif

    FklSid_t UV_FS_O_APPEND_sid;
    FklSid_t UV_FS_O_CREAT_sid;
    FklSid_t UV_FS_O_EXCL_sid;
    FklSid_t UV_FS_O_FILEMAP_sid;
    FklSid_t UV_FS_O_RANDOM_sid;
    FklSid_t UV_FS_O_RDONLY_sid;
    FklSid_t UV_FS_O_RDWR_sid;
    FklSid_t UV_FS_O_SEQUENTIAL_sid;
    FklSid_t UV_FS_O_SHORT_LIVED_sid;
    FklSid_t UV_FS_O_TEMPORARY_sid;
    FklSid_t UV_FS_O_TRUNC_sid;
    FklSid_t UV_FS_O_WRONLY_sid;
    FklSid_t UV_FS_O_DIRECT_sid;
    FklSid_t UV_FS_O_DIRECTORY_sid;
    FklSid_t UV_FS_O_DSYNC_sid;
    FklSid_t UV_FS_O_EXLOCK_sid;
    FklSid_t UV_FS_O_NOATIME_sid;
    FklSid_t UV_FS_O_NOCTTY_sid;
    FklSid_t UV_FS_O_NOFOLLOW_sid;
    FklSid_t UV_FS_O_NONBLOCK_sid;
    FklSid_t UV_FS_O_SYMLINK_sid;
    FklSid_t UV_FS_O_SYNC_sid;

    FklSid_t UV_FS_SYMLINK_DIR_sid;
    FklSid_t UV_FS_SYMLINK_JUNCTION_sid;

    FklSid_t UV_FS_COPYFILE_EXCL_sid;
    FklSid_t UV_FS_COPYFILE_FICLONE_sid;
    FklSid_t UV_FS_COPYFILE_FICLONE_FORCE_sid;

    FklSid_t stat_f_dev_sid;
    FklSid_t stat_f_mode_sid;
    FklSid_t stat_f_nlink_sid;
    FklSid_t stat_f_uid_sid;
    FklSid_t stat_f_gid_sid;
    FklSid_t stat_f_rdev_sid;
    FklSid_t stat_f_ino_sid;
    FklSid_t stat_f_size_sid;
    FklSid_t stat_f_blksize_sid;
    FklSid_t stat_f_blocks_sid;
    FklSid_t stat_f_flags_sid;
    FklSid_t stat_f_gen_sid;
    FklSid_t stat_f_atime_sid;
    FklSid_t stat_f_mtime_sid;
    FklSid_t stat_f_ctime_sid;
    FklSid_t stat_f_birthtime_sid;
    FklSid_t stat_f_type_sid;

    FklSid_t stat_type_file_sid;
    FklSid_t stat_type_directory_sid;
    FklSid_t stat_type_link_sid;
    FklSid_t stat_type_fifo_sid;
    FklSid_t stat_type_socket_sid;
    FklSid_t stat_type_char_sid;
    FklSid_t stat_type_block_sid;
    FklSid_t stat_type_unknown_sid;

    FklSid_t timespec_f_sec_sid;
    FklSid_t timespec_f_nsec_sid;
    FklSid_t timeval_f_usec_sid;

    FklSid_t statfs_f_type_sid;
    FklSid_t statfs_f_bsize_sid;
    FklSid_t statfs_f_blocks_sid;
    FklSid_t statfs_f_bfree_sid;
    FklSid_t statfs_f_bavail_sid;
    FklSid_t statfs_f_files_sid;
    FklSid_t statfs_f_ffree_sid;

    FklSid_t dirent_f_name_sid;
    FklSid_t dirent_f_type_sid;

    FklSid_t UV_DIRENT_UNKNOWN_sid;
    FklSid_t UV_DIRENT_FILE_sid;
    FklSid_t UV_DIRENT_DIR_sid;
    FklSid_t UV_DIRENT_LINK_sid;
    FklSid_t UV_DIRENT_FIFO_sid;
    FklSid_t UV_DIRENT_SOCKET_sid;
    FklSid_t UV_DIRENT_CHAR_sid;
    FklSid_t UV_DIRENT_BLOCK_sid;

    FklSid_t UV_FS_EVENT_WATCH_ENTRY_sid;
    FklSid_t UV_FS_EVENT_STAT_sid;
    FklSid_t UV_FS_EVENT_RECURSIVE_sid;

    FklSid_t UV_RENAME_sid;
    FklSid_t UV_CHANGE_sid;

    FklSid_t UV_CLOCK_MONOTONIC_sid;
    FklSid_t UV_CLOCK_REALTIME_sid;

    FklSid_t utsname_sysname_sid;
    FklSid_t utsname_release_sid;
    FklSid_t utsname_version_sid;
    FklSid_t utsname_machine_sid;

    FklSid_t rusage_utime_sid;
    FklSid_t rusage_stime_sid;
    FklSid_t rusage_maxrss_sid;
    FklSid_t rusage_ixrss_sid;
    FklSid_t rusage_idrss_sid;
    FklSid_t rusage_isrss_sid;
    FklSid_t rusage_minflt_sid;
    FklSid_t rusage_majflt_sid;
    FklSid_t rusage_nswap_sid;
    FklSid_t rusage_inblock_sid;
    FklSid_t rusage_oublock_sid;
    FklSid_t rusage_msgsnd_sid;
    FklSid_t rusage_msgrcv_sid;
    FklSid_t rusage_nsignals_sid;
    FklSid_t rusage_nvcsw_sid;
    FklSid_t rusage_nivcsw_sid;

    FklSid_t cpu_info_model_sid;
    FklSid_t cpu_info_speed_sid;
    FklSid_t cpu_info_times_sid;
    FklSid_t cpu_info_times_user_sid;
    FklSid_t cpu_info_times_nice_sid;
    FklSid_t cpu_info_times_sys_sid;
    FklSid_t cpu_info_times_idle_sid;
    FklSid_t cpu_info_times_irq_sid;

    FklSid_t passwd_username_sid;
    FklSid_t passwd_uid_sid;
    FklSid_t passwd_gid_sid;
    FklSid_t passwd_shell_sid;
    FklSid_t passwd_homedir_sid;

    FklSid_t UV_PRIORITY_LOW_sid;
    FklSid_t UV_PRIORITY_BELOW_NORMAL_sid;
    FklSid_t UV_PRIORITY_NORMAL_sid;
    FklSid_t UV_PRIORITY_ABOVE_NORMAL_sid;
    FklSid_t UV_PRIORITY_HIGH_sid;
    FklSid_t UV_PRIORITY_HIGHEST_sid;

    FklSid_t ifa_f_name_sid;
    FklSid_t ifa_f_mac_sid;
    FklSid_t ifa_f_internal_sid;
    FklSid_t ifa_f_ip_sid;
    FklSid_t ifa_f_netmask_sid;
    FklSid_t ifa_f_family_sid;

    FklSid_t metrics_loop_count_sid;
    FklSid_t metrics_events_sid;
    FklSid_t metrics_events_waiting_sid;

    FklSid_t UV_READABLE_sid;
    FklSid_t UV_WRITABLE_sid;
    FklSid_t UV_DISCONNECT_sid;
    FklSid_t UV_PRIORITIZED_sid;
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

int isFuvLoop(FklVMvalue *v);
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

int isFuvTimer(FklVMvalue *v);
FklVMvalue *
createFuvTimer(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvPrepare(FklVMvalue *v);
FklVMvalue *
createFuvPrepare(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvIdle(FklVMvalue *v);
FklVMvalue *createFuvIdle(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvCheck(FklVMvalue *v);
FklVMvalue *
createFuvCheck(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvSignal(FklVMvalue *v);
FklVMvalue *
createFuvSignal(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvAsync(FklVMvalue *v);
FklVMvalue *createFuvAsync(FklVM *,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *proc_obj,
        int *err);

int isFuvProcess(FklVMvalue *v);
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

int isFuvStream(FklVMvalue *v);
int isFuvPipe(FklVMvalue *v);
uv_pipe_t *
createFuvPipe(FklVM *, FklVMvalue **pr, FklVMvalue *dll, FklVMvalue *loop_obj);

int isFuvTcp(FklVMvalue *v);
uv_tcp_t *
createFuvTcp(FklVM *, FklVMvalue **pr, FklVMvalue *dll, FklVMvalue *loop_obj);

int isFuvTty(FklVMvalue *v);
uv_tty_t *createFuvTty(FklVM *,
        FklVMvalue **pr,
        FklVMvalue *dll,
        FklVMvalue *loop_obj,
        FklVMvalue *fp_obj);

int isFuvUdp(FklVMvalue *v);
uv_udp_t *createFuvUdp(FklVM *,
        FklVMvalue **pr,
        FklVMvalue *dll,
        FklVMvalue *loop_obj,
        int64_t);

int isFuvFsPoll(FklVMvalue *v);
FklVMvalue *
createFuvFsPoll(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvFsEvent(FklVMvalue *v);
FklVMvalue *
createFuvFsEvent(FklVM *, FklVMvalue *dll, FklVMvalue *loop, int *err);

int isFuvPoll(FklVMvalue *v);
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

noreturn void raiseUvError(int err, FklVM *exe, FklVMvalue *pd);
noreturn void raiseFuvError(FuvErrorType, FklVM *exe, FklVMvalue *pd);
FklSid_t uvErrToSid(int err_id, FuvPublicData *pd);
FklVMvalue *createUvError(int err_id, FklVM *exe, FklVMvalue *pd);
FklVMvalue *createUvErrorWithFpd(int err_id, FklVM *exe, FuvPublicData *fpd);

int symbolToSignum(FklSid_t, FuvPublicData *pd);
FklSid_t signumToSymbol(int, FuvPublicData *pd);

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

int isFuvGetaddrinfo(FklVMvalue *v);
uv_getaddrinfo_t *createFuvGetaddrinfo(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback);

int isFuvGetnameinfo(FklVMvalue *v);
uv_getnameinfo_t *createFuvGetnameinfo(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback);

int isFuvWrite(FklVMvalue *);
uv_write_t *createFuvWrite(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        uint32_t count);

int isFuvShutdown(FklVMvalue *);
uv_shutdown_t *createFuvShutdown(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback);

int isFuvConnect(FklVMvalue *);
uv_connect_t *createFuvConnect(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback);

int isFuvUdpSend(FklVMvalue *);
uv_udp_send_t *createFuvUdpSend(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        uint32_t count);

int isFuvFsReq(FklVMvalue *v);

struct FuvFsReq *createFuvFsReq(FklVM *exe,
        FklVMvalue **r,
        FklVMvalue *dll,
        FklVMvalue *loop,
        FklVMvalue *callback,
        const FuvFsReqArgs *args);

void fuvFsReqCleanUp(FuvFsReq *req, FuvFsReqCleanUpOption);

int isFuvRandom(FklVMvalue *v);
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
