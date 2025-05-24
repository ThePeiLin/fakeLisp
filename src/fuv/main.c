#include <fakeLisp/vm.h>

#include <uv.h>
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <string.h>

#include "fuv.h"

#define PREDICATE(condition)                                                   \
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);                                     \
    FklVMvalue *val = FKL_CPROC_GET_ARG(exe, ctx, 0);                          \
    FKL_CPROC_RETURN(exe, ctx, (condition) ? FKL_VM_TRUE : FKL_VM_NIL);        \
    return 0;

#define CHECK_HANDLE_CLOSED(H, EXE, PD)                                        \
    if ((H) == NULL)                                                           \
    raiseFuvError(FUV_ERR_HANDLE_CLOSED, (EXE), (PD))

#define GET_HANDLE(FUV_HANDLE) (&((FUV_HANDLE)->handle))

#define DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(NAME, OBJ, EXE, PD)                \
    FKL_DECL_VM_UD_DATA((NAME), FuvHandleUd, (OBJ));                           \
    CHECK_HANDLE_CLOSED(*(NAME), (EXE), (PD))

#define GET_REQ(FUV_REQ) (&((FUV_REQ)->req))

#define CHECK_REQ_CANCELED(R, EXE, PD)                                         \
    if ((R) == NULL)                                                           \
    raiseFuvError(FUV_ERR_REQ_CANCELED, (EXE), (PD))

#define CHECK_UV_RESULT(R, EXE, PD)                                            \
    if ((R) < 0)                                                               \
    raiseUvError((R), (EXE), (PD))

#define CHECK_UV_RESULT_AND_CLEANUP_REQ(R, REQ, EXE, PD)                       \
    if ((R) < 0) {                                                             \
        cleanup_req((REQ));                                                    \
        raiseUvError((R), (EXE), PD);                                          \
    }

#define CHECK_UV_RESULT_AND_CLEANUP_HANDLE(R, HANDLE, LOOP, EXE, PD)           \
    if ((R) < 0) {                                                             \
        cleanup_handle((HANDLE), (LOOP));                                      \
        raiseUvError((R), (EXE), PD);                                          \
    }

static FklVMudMetaTable FuvPublicDataMetaTable = {
    .size = sizeof(FuvPublicData),
};

static inline void fuv_fs_req_cleanup(FuvReq *req) {
    uv_req_t *r = &req->req;
    if (r->type == UV_FS) {
        struct FuvFsReq *freq = (struct FuvFsReq *)req;

        uv_fs_t *fs = (uv_fs_t *)r;
        if (fs->fs_type == UV_FS_OPENDIR && fs->ptr) {
            uv_fs_t req;
            uv_fs_closedir(NULL, &req, fs->ptr, NULL);
            uv_fs_req_cleanup(&req);
        }
        uv_fs_req_cleanup(fs);
        if (freq->dir) {
            unrefFuvDir(freq->dir);
            freq->dir = NULL;
        }
    }
}

static inline void cleanup_req(FklVMvalue *req_obj) {
    FKL_DECL_VM_UD_DATA(fuv_req, FuvReqUd, req_obj);
    FuvReq *req = *fuv_req;
    fuv_fs_req_cleanup(req);
    uninitFuvReq(fuv_req);
    free(req);
}

static inline void cleanup_handle(FklVMvalue *handle_obj,
                                  FklVMvalue *loop_obj) {
    if (handle_obj == NULL)
        return;
    FKL_DECL_VM_UD_DATA(handle_ud, FuvHandleUd, handle_obj);
    FuvHandle *fuv_handle = *handle_ud;
    FuvHandleData *hdata = &fuv_handle->data;
    hdata->callbacks[0] = NULL;
    hdata->callbacks[1] = NULL;
    if (uv_handle_get_loop(&fuv_handle->handle)) {
        uv_close(&fuv_handle->handle, fuvCloseLoopHandleCb);
    } else {
        FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
        FuvLoopData *ldata = &fuv_loop->data;
        hdata->loop = NULL;
        free(fuv_handle);
        if (handle_obj) {
            fklVMvalueHashSetDel2(&ldata->gc_values, handle_obj);
            *handle_ud = NULL;
        }
    }
}

static inline void init_fuv_public_data(FuvPublicData *pd, FklSymbolTable *st) {
    static const char *loop_mode[] = {
        "default",
        "once",
        "nowait",
    };
    pd->loop_mode[UV_RUN_DEFAULT] =
        fklAddSymbolCstr(loop_mode[UV_RUN_DEFAULT], st)->v;
    pd->loop_mode[UV_RUN_ONCE] =
        fklAddSymbolCstr(loop_mode[UV_RUN_ONCE], st)->v;
    pd->loop_mode[UV_RUN_NOWAIT] =
        fklAddSymbolCstr(loop_mode[UV_RUN_NOWAIT], st)->v;

    static const char *fuv_err_sym[] = {
        "dummy",         "fuv-handle-error", "fuv-handle-error",
        "fuv-req-error", "fuv-dir-error",    "value-error",
    };
    for (size_t i = 0; i < FUV_ERR_NUM; i++)
        pd->fuv_err_sid[i] = fklAddSymbolCstr(fuv_err_sym[i], st)->v;

    static const char *loop_config_sym[] = {
        "loop-block-signal",
        "metrics-idle-time",
        "loop-use-io-uring-sqpoll",
    };
    pd->loop_block_signal_sid =
        fklAddSymbolCstr(loop_config_sym[UV_LOOP_BLOCK_SIGNAL], st)->v;
    pd->metrics_idle_time_sid =
        fklAddSymbolCstr(loop_config_sym[UV_METRICS_IDLE_TIME], st)->v;
    pd->loop_use_io_uring_sqpoll =
        fklAddSymbolCstr(loop_config_sym[UV_LOOP_USE_IO_URING_SQPOLL], st)->v;

#define XX(code, _)                                                            \
    pd->uv_err_sid_##code = fklAddSymbolCstr("UV_" #code, st)->v;
    UV_ERRNO_MAP(XX);
#undef XX

    pd->UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS_sid =
        fklAddSymbolCstr("verbatim", st)->v;
    pd->UV_PROCESS_DETACHED_sid = fklAddSymbolCstr("detached", st)->v;
    pd->UV_PROCESS_WINDOWS_HIDE_sid = fklAddSymbolCstr("hide", st)->v;
    pd->UV_PROCESS_WINDOWS_HIDE_CONSOLE_sid =
        fklAddSymbolCstr("hide-console", st)->v;
    pd->UV_PROCESS_WINDOWS_HIDE_GUI_sid = fklAddSymbolCstr("hide-gui", st)->v;

    pd->UV_TTY_MODE_NORMAL_sid = fklAddSymbolCstr("normal", st)->v;
    pd->UV_TTY_MODE_RAW_sid = fklAddSymbolCstr("raw", st)->v;
    pd->UV_TTY_MODE_IO_sid = fklAddSymbolCstr("io", st)->v;

    pd->UV_TTY_SUPPORTED_sid = fklAddSymbolCstr("supported", st)->v;
    pd->UV_TTY_UNSUPPORTED_sid = fklAddSymbolCstr("unsupported", st)->v;

    pd->UV_UDP_IPV6ONLY_sid = fklAddSymbolCstr("ipv6only", st)->v;
    pd->UV_UDP_PARTIAL_sid = fklAddSymbolCstr("partial", st)->v;
    pd->UV_UDP_REUSEADDR_sid = fklAddSymbolCstr("reuseaddr", st)->v;
    pd->UV_UDP_MMSG_CHUNK_sid = fklAddSymbolCstr("mmsg-chunk", st)->v;
    pd->UV_UDP_MMSG_FREE_sid = fklAddSymbolCstr("mmsg-free", st)->v;
    pd->UV_UDP_LINUX_RECVERR_sid = fklAddSymbolCstr("linux-recverr", st)->v;
    pd->UV_UDP_RECVMMSG_sid = fklAddSymbolCstr("recvmmsg", st)->v;

    pd->UV_LEAVE_GROUP_sid = fklAddSymbolCstr("leave", st)->v;
    pd->UV_JOIN_GROUP_sid = fklAddSymbolCstr("join", st)->v;

    pd->AI_ADDRCONFIG_sid = fklAddSymbolCstr("addrconfig", st)->v;
#ifdef AI_V4MAPPED
    pd->AI_V4MAPPED_sid = fklAddSymbolCstr("v4mapped", st)->v;
#endif
#ifdef AI_ALL
    pd->AI_ALL_sid = fklAddSymbolCstr("all", st)->v;
#endif
    pd->AI_NUMERICHOST_sid = fklAddSymbolCstr("numerichost", st)->v;
    pd->AI_PASSIVE_sid = fklAddSymbolCstr("passive", st)->v;
    pd->AI_NUMERICSERV_sid = fklAddSymbolCstr("numerserv", st)->v;
    pd->AI_CANONNAME_sid = fklAddSymbolCstr("canonname", st)->v;

    pd->f_ip_sid = fklAddSymbolCstr("ip", st)->v;
    pd->f_addr_sid = fklAddSymbolCstr("addr", st)->v;
    pd->f_port_sid = fklAddSymbolCstr("port", st)->v;
    pd->f_family_sid = fklAddSymbolCstr("family", st)->v;
    pd->f_socktype_sid = fklAddSymbolCstr("socktype", st)->v;
    pd->f_protocol_sid = fklAddSymbolCstr("protocol", st)->v;
    pd->f_canonname_sid = fklAddSymbolCstr("canonname", st)->v;
    pd->f_hostname_sid = fklAddSymbolCstr("hostname", st)->v;
    pd->f_service_sid = fklAddSymbolCstr("service", st)->v;

    pd->f_args_sid = fklAddSymbolCstr("args", st)->v;
    pd->f_env_sid = fklAddSymbolCstr("env", st)->v;
    pd->f_cwd_sid = fklAddSymbolCstr("cwd", st)->v;
    pd->f_stdio_sid = fklAddSymbolCstr("stdio", st)->v;
    pd->f_uid_sid = fklAddSymbolCstr("uid", st)->v;
    pd->f_gid_sid = fklAddSymbolCstr("gid", st)->v;

#ifdef AF_UNSPEC
    pd->AF_UNSPEC_sid = fklAddSymbolCstr("unspec", st)->v;
#endif
#ifdef AF_UNIX
    pd->AF_UNIX_sid = fklAddSymbolCstr("unix", st)->v;
#endif
#ifdef AF_INET
    pd->AF_INET_sid = fklAddSymbolCstr("inet", st)->v;
#endif
#ifdef AF_INET6
    pd->AF_INET6_sid = fklAddSymbolCstr("inet6", st)->v;
#endif
#ifdef AF_IPX
    pd->AF_IPX_sid = fklAddSymbolCstr("ipx", st)->v;
#endif
#ifdef AF_NETLINK
    pd->AF_NETLINK_sid = fklAddSymbolCstr("netlink", st)->v;
#endif
#ifdef AF_X25
    pd->AF_X25_sid = fklAddSymbolCstr("x25", st)->v;
#endif
#ifdef AF_AX25
    pd->AF_AX25_sid = fklAddSymbolCstr("ax25", st)->v;
#endif
#ifdef AF_ATMPVC
    pd->AF_ATMPVC_sid = fklAddSymbolCstr("atmpvc", st)->v;
#endif
#ifdef AF_APPLETALK
    pd->AF_APPLETALK_sid = fklAddSymbolCstr("appletalk", st)->v;
#endif
#ifdef AF_PACKET
    pd->AF_PACKET_sid = fklAddSymbolCstr("packet", st)->v;
#endif

#ifdef SOCK_STREAM
    pd->SOCK_STREAM_sid = fklAddSymbolCstr("stream", st)->v;
#endif
#ifdef SOCK_DGRAM
    pd->SOCK_DGRAM_sid = fklAddSymbolCstr("dgram", st)->v;
#endif
#ifdef SOCK_SEQPACKET
    pd->SOCK_SEQPACKET_sid = fklAddSymbolCstr("seqpacket", st)->v;
#endif
#ifdef SOCK_RAW
    pd->SOCK_RAW_sid = fklAddSymbolCstr("raw", st)->v;
#endif
#ifdef SOCK_RDM
    pd->SOCK_RDM_sid = fklAddSymbolCstr("rdm", st)->v;
#endif

#ifdef SIGHUP
    pd->SIGHUP_sid = fklAddSymbolCstr("sighup", st)->v;
#endif
#ifdef SIGINT
    pd->SIGINT_sid = fklAddSymbolCstr("sigint", st)->v;
#endif
#ifdef SIGQUIT
    pd->SIGQUIT_sid = fklAddSymbolCstr("sigquit", st)->v;
#endif
#ifdef SIGILL
    pd->SIGILL_sid = fklAddSymbolCstr("sigill", st)->v;
#endif
#ifdef SIGTRAP
    pd->SIGTRAP_sid = fklAddSymbolCstr("sigtrap", st)->v;
#endif
#ifdef SIGABRT
    pd->SIGABRT_sid = fklAddSymbolCstr("sigabrt", st)->v;
#endif
#ifdef SIGIOT
    pd->SIGIOT_sid = fklAddSymbolCstr("sigiot", st)->v;
#endif
#ifdef SIGBUS
    pd->SIGBUS_sid = fklAddSymbolCstr("sigbus", st)->v;
#endif
#ifdef SIGFPE
    pd->SIGFPE_sid = fklAddSymbolCstr("sigfpe", st)->v;
#endif
#ifdef SIGKILL
    pd->SIGKILL_sid = fklAddSymbolCstr("sigkill", st)->v;
#endif
#ifdef SIGUSR1
    pd->SIGUSR1_sid = fklAddSymbolCstr("sigusr1", st)->v;
#endif
#ifdef SIGSEGV
    pd->SIGSEGV_sid = fklAddSymbolCstr("sigsegv", st)->v;
#endif
#ifdef SIGUSR2
    pd->SIGUSR2_sid = fklAddSymbolCstr("sigusr2", st)->v;
#endif
#ifdef SIGPIPE
    pd->SIGPIPE_sid = fklAddSymbolCstr("sigpipe", st)->v;
#endif
#ifdef SIGALRM
    pd->SIGALRM_sid = fklAddSymbolCstr("sigalrm", st)->v;
#endif
#ifdef SIGTERM
    pd->SIGTERM_sid = fklAddSymbolCstr("sigterm", st)->v;
#endif
#ifdef SIGCHLD
    pd->SIGCHLD_sid = fklAddSymbolCstr("sigchld", st)->v;
#endif
#ifdef SIGSTKFLT
    pd->SIGSTKFLT_sid = fklAddSymbolCstr("sigstkflt", st)->v;
#endif
#ifdef SIGCONT
    pd->SIGCONT_sid = fklAddSymbolCstr("sigcont", st)->v;
#endif
#ifdef SIGSTOP
    pd->SIGSTOP_sid = fklAddSymbolCstr("sigstop", st)->v;
#endif
#ifdef SIGTSTP
    pd->SIGTSTP_sid = fklAddSymbolCstr("sigtstp", st)->v;
#endif
#ifdef SIGBREAK
    pd->SIGBREAK_sid = fklAddSymbolCstr("sigbreak", st)->v;
#endif
#ifdef SIGTTIN
    pd->SIGTTIN_sid = fklAddSymbolCstr("sigttin", st)->v;
#endif
#ifdef SIGTTOU
    pd->SIGTTOU_sid = fklAddSymbolCstr("sigttou", st)->v;
#endif
#ifdef SIGURG
    pd->SIGURG_sid = fklAddSymbolCstr("sigurg", st)->v;
#endif
#ifdef SIGXCPU
    pd->SIGXCPU_sid = fklAddSymbolCstr("sigxcpu", st)->v;
#endif
#ifdef SIGXFSZ
    pd->SIGXFSZ_sid = fklAddSymbolCstr("sigxfsz", st)->v;
#endif
#ifdef SIGVTALRM
    pd->SIGVTALRM_sid = fklAddSymbolCstr("sigvtalrm", st)->v;
#endif
#ifdef SIGPROF
    pd->SIGPROF_sid = fklAddSymbolCstr("sigprof", st)->v;
#endif
#ifdef SIGWINCH
    pd->SIGWINCH_sid = fklAddSymbolCstr("sigwinch", st)->v;
#endif
#ifdef SIGIO
    pd->SIGIO_sid = fklAddSymbolCstr("sigio", st)->v;
#endif
#ifdef SIGPOLL
    pd->SIGPOLL_sid = fklAddSymbolCstr("sigpoll", st)->v;
#endif
#ifdef SIGLOST
    pd->SIGLOST_sid = fklAddSymbolCstr("siglost", st)->v;
#endif
#ifdef SIGPWR
    pd->SIGPWR_sid = fklAddSymbolCstr("sigpwr", st)->v;
#endif
#ifdef SIGSYS
    pd->SIGSYS_sid = fklAddSymbolCstr("sigsys", st)->v;
#endif

#ifdef NI_NAMEREQD
    pd->NI_NAMEREQD_sid = fklAddSymbolCstr("namereqd", st)->v;
#endif
#ifdef NI_DGRAM
    pd->NI_DGRAM_sid = fklAddSymbolCstr("dgram", st)->v;
#endif
#ifdef NI_NOFQDN
    pd->NI_NOFQDN_sid = fklAddSymbolCstr("nofqdn", st)->v;
#endif
#ifdef NI_NUMERICHOST
    pd->NI_NUMERICHOST_sid = fklAddSymbolCstr("numerichost", st)->v;
#endif
#ifdef NI_NUMERICSERV
    pd->NI_NUMERICSERV_sid = fklAddSymbolCstr("numericserv", st)->v;
#endif
#ifdef NI_IDN
    pd->NI_IDN_sid = fklAddSymbolCstr("idn", st)->v;
#endif
#ifdef NI_IDN_ALLOW_UNASSIGNED
    pd->NI_IDN_ALLOW_UNASSIGNED_sid =
        fklAddSymbolCstr("idn-allow-unassigned", st)->v;
#endif
#ifdef NI_IDN_USE_STD3_ASCII_RULES
    pd->NI_IDN_USE_STD3_ASCII_RULES_sid =
        fklAddSymbolCstr("idn-use-std3-ascii-rules", st)->v;
#endif

    pd->UV_FS_O_APPEND_sid = fklAddSymbolCstr("append", st)->v;
    pd->UV_FS_O_CREAT_sid = fklAddSymbolCstr("creat", st)->v;
    pd->UV_FS_O_EXCL_sid = fklAddSymbolCstr("excl", st)->v;
    pd->UV_FS_O_FILEMAP_sid = fklAddSymbolCstr("filemap", st)->v;
    pd->UV_FS_O_RANDOM_sid = fklAddSymbolCstr("random", st)->v;
    pd->UV_FS_O_RDONLY_sid = fklAddSymbolCstr("rdonly", st)->v;
    pd->UV_FS_O_RDWR_sid = fklAddSymbolCstr("rdwr", st)->v;
    pd->UV_FS_O_SEQUENTIAL_sid = fklAddSymbolCstr("sequential", st)->v;
    pd->UV_FS_O_SHORT_LIVED_sid = fklAddSymbolCstr("short_lived", st)->v;
    pd->UV_FS_O_TEMPORARY_sid = fklAddSymbolCstr("temporary", st)->v;
    pd->UV_FS_O_TRUNC_sid = fklAddSymbolCstr("trunc", st)->v;
    pd->UV_FS_O_WRONLY_sid = fklAddSymbolCstr("wronly", st)->v;
    pd->UV_FS_O_DIRECT_sid = fklAddSymbolCstr("direct", st)->v;
    pd->UV_FS_O_DIRECTORY_sid = fklAddSymbolCstr("directory", st)->v;
    pd->UV_FS_O_DSYNC_sid = fklAddSymbolCstr("dsync", st)->v;
    pd->UV_FS_O_EXLOCK_sid = fklAddSymbolCstr("exlock", st)->v;
    pd->UV_FS_O_NOATIME_sid = fklAddSymbolCstr("noatime", st)->v;
    pd->UV_FS_O_NOCTTY_sid = fklAddSymbolCstr("noctty", st)->v;
    pd->UV_FS_O_NOFOLLOW_sid = fklAddSymbolCstr("nofollow", st)->v;
    pd->UV_FS_O_NONBLOCK_sid = fklAddSymbolCstr("nonblock", st)->v;
    pd->UV_FS_O_SYMLINK_sid = fklAddSymbolCstr("symlink", st)->v;
    pd->UV_FS_O_SYNC_sid = fklAddSymbolCstr("sync", st)->v;

    pd->UV_FS_SYMLINK_DIR_sid = fklAddSymbolCstr("dir", st)->v;
    pd->UV_FS_SYMLINK_JUNCTION_sid = fklAddSymbolCstr("junction", st)->v;

    pd->UV_FS_COPYFILE_EXCL_sid = fklAddSymbolCstr("excl", st)->v;
    pd->UV_FS_COPYFILE_FICLONE_sid = fklAddSymbolCstr("ficlone", st)->v;
    pd->UV_FS_COPYFILE_FICLONE_FORCE_sid =
        fklAddSymbolCstr("ficlone-force", st)->v;

    pd->stat_f_dev_sid = fklAddSymbolCstr("dev", st)->v;
    pd->stat_f_mode_sid = fklAddSymbolCstr("mode", st)->v;
    pd->stat_f_nlink_sid = fklAddSymbolCstr("nlink", st)->v;
    pd->stat_f_uid_sid = fklAddSymbolCstr("uid", st)->v;
    pd->stat_f_gid_sid = fklAddSymbolCstr("gid", st)->v;
    pd->stat_f_rdev_sid = fklAddSymbolCstr("rdev", st)->v;
    pd->stat_f_ino_sid = fklAddSymbolCstr("ino", st)->v;
    pd->stat_f_size_sid = fklAddSymbolCstr("size", st)->v;
    pd->stat_f_blksize_sid = fklAddSymbolCstr("blksize", st)->v;
    pd->stat_f_blocks_sid = fklAddSymbolCstr("blocks", st)->v;
    pd->stat_f_flags_sid = fklAddSymbolCstr("flags", st)->v;
    pd->stat_f_gen_sid = fklAddSymbolCstr("gen", st)->v;
    pd->stat_f_atime_sid = fklAddSymbolCstr("atime", st)->v;
    pd->stat_f_mtime_sid = fklAddSymbolCstr("mtime", st)->v;
    pd->stat_f_ctime_sid = fklAddSymbolCstr("ctime", st)->v;
    pd->stat_f_birthtime_sid = fklAddSymbolCstr("birthtime", st)->v;
    pd->stat_f_type_sid = fklAddSymbolCstr("type", st)->v;

    pd->stat_type_file_sid = fklAddSymbolCstr("file", st)->v;
    pd->stat_type_directory_sid = fklAddSymbolCstr("directory", st)->v;
    pd->stat_type_link_sid = fklAddSymbolCstr("link", st)->v;
    pd->stat_type_fifo_sid = fklAddSymbolCstr("fifo", st)->v;
    pd->stat_type_socket_sid = fklAddSymbolCstr("socket", st)->v;
    pd->stat_type_char_sid = fklAddSymbolCstr("char", st)->v;
    pd->stat_type_block_sid = fklAddSymbolCstr("block", st)->v;
    pd->stat_type_unknown_sid = fklAddSymbolCstr("unknown", st)->v;

    pd->timespec_f_sec_sid = fklAddSymbolCstr("sec", st)->v;
    pd->timespec_f_nsec_sid = fklAddSymbolCstr("nsec", st)->v;
    pd->timeval_f_usec_sid = fklAddSymbolCstr("usec", st)->v;

    pd->statfs_f_type_sid = fklAddSymbolCstr("type", st)->v;
    pd->statfs_f_bsize_sid = fklAddSymbolCstr("bsize", st)->v;
    pd->statfs_f_blocks_sid = fklAddSymbolCstr("blocks", st)->v;
    pd->statfs_f_bfree_sid = fklAddSymbolCstr("bfree", st)->v;
    pd->statfs_f_bavail_sid = fklAddSymbolCstr("bavail", st)->v;
    pd->statfs_f_files_sid = fklAddSymbolCstr("files", st)->v;
    pd->statfs_f_ffree_sid = fklAddSymbolCstr("ffree", st)->v;

    pd->dirent_f_name_sid = fklAddSymbolCstr("name", st)->v;
    pd->dirent_f_type_sid = fklAddSymbolCstr("type", st)->v;

    pd->UV_DIRENT_UNKNOWN_sid = fklAddSymbolCstr("unknown", st)->v;
    pd->UV_DIRENT_FILE_sid = fklAddSymbolCstr("file", st)->v;
    pd->UV_DIRENT_DIR_sid = fklAddSymbolCstr("dir", st)->v;
    pd->UV_DIRENT_LINK_sid = fklAddSymbolCstr("link", st)->v;
    pd->UV_DIRENT_FIFO_sid = fklAddSymbolCstr("fifo", st)->v;
    pd->UV_DIRENT_SOCKET_sid = fklAddSymbolCstr("socket", st)->v;
    pd->UV_DIRENT_CHAR_sid = fklAddSymbolCstr("char", st)->v;
    pd->UV_DIRENT_BLOCK_sid = fklAddSymbolCstr("block", st)->v;

    pd->UV_FS_EVENT_WATCH_ENTRY_sid = fklAddSymbolCstr("watch-entry", st)->v;
    pd->UV_FS_EVENT_STAT_sid = fklAddSymbolCstr("stat", st)->v;
    pd->UV_FS_EVENT_RECURSIVE_sid = fklAddSymbolCstr("recursive", st)->v;

    pd->UV_RENAME_sid = fklAddSymbolCstr("rename", st)->v;
    pd->UV_CHANGE_sid = fklAddSymbolCstr("change", st)->v;

    pd->UV_CLOCK_MONOTONIC_sid = fklAddSymbolCstr("monotonic", st)->v;
    pd->UV_CLOCK_REALTIME_sid = fklAddSymbolCstr("realtime", st)->v;

    pd->utsname_sysname_sid = fklAddSymbolCstr("sysname", st)->v;
    pd->utsname_release_sid = fklAddSymbolCstr("release", st)->v;
    pd->utsname_version_sid = fklAddSymbolCstr("version", st)->v;
    pd->utsname_machine_sid = fklAddSymbolCstr("machine", st)->v;

    pd->rusage_utime_sid = fklAddSymbolCstr("utime", st)->v;
    pd->rusage_stime_sid = fklAddSymbolCstr("stime", st)->v;
    pd->rusage_maxrss_sid = fklAddSymbolCstr("maxrss", st)->v;
    pd->rusage_ixrss_sid = fklAddSymbolCstr("ixrss", st)->v;
    pd->rusage_idrss_sid = fklAddSymbolCstr("idrss", st)->v;
    pd->rusage_isrss_sid = fklAddSymbolCstr("isrss", st)->v;
    pd->rusage_minflt_sid = fklAddSymbolCstr("minflt", st)->v;
    pd->rusage_majflt_sid = fklAddSymbolCstr("majflt", st)->v;
    pd->rusage_nswap_sid = fklAddSymbolCstr("nswap", st)->v;
    pd->rusage_inblock_sid = fklAddSymbolCstr("inblock", st)->v;
    pd->rusage_oublock_sid = fklAddSymbolCstr("oublock", st)->v;
    pd->rusage_msgsnd_sid = fklAddSymbolCstr("msgsnd", st)->v;
    pd->rusage_msgrcv_sid = fklAddSymbolCstr("msgrcv", st)->v;
    pd->rusage_nsignals_sid = fklAddSymbolCstr("nsignals", st)->v;
    pd->rusage_nvcsw_sid = fklAddSymbolCstr("nvcsw", st)->v;
    pd->rusage_nivcsw_sid = fklAddSymbolCstr("nivcsw", st)->v;

    pd->cpu_info_model_sid = fklAddSymbolCstr("model", st)->v;
    pd->cpu_info_speed_sid = fklAddSymbolCstr("speed", st)->v;
    pd->cpu_info_times_sid = fklAddSymbolCstr("times", st)->v;
    pd->cpu_info_times_user_sid = fklAddSymbolCstr("user", st)->v;
    pd->cpu_info_times_nice_sid = fklAddSymbolCstr("nice", st)->v;
    pd->cpu_info_times_sys_sid = fklAddSymbolCstr("sys", st)->v;
    pd->cpu_info_times_idle_sid = fklAddSymbolCstr("idle", st)->v;
    pd->cpu_info_times_irq_sid = fklAddSymbolCstr("irq", st)->v;

    pd->passwd_username_sid = fklAddSymbolCstr("username", st)->v;
    pd->passwd_uid_sid = fklAddSymbolCstr("uid", st)->v;
    pd->passwd_gid_sid = fklAddSymbolCstr("gid", st)->v;
    pd->passwd_shell_sid = fklAddSymbolCstr("shell", st)->v;
    pd->passwd_homedir_sid = fklAddSymbolCstr("homedir", st)->v;

    pd->UV_PRIORITY_LOW_sid = fklAddSymbolCstr("low", st)->v;
    pd->UV_PRIORITY_BELOW_NORMAL_sid = fklAddSymbolCstr("below-normal", st)->v;
    pd->UV_PRIORITY_NORMAL_sid = fklAddSymbolCstr("normal", st)->v;
    pd->UV_PRIORITY_ABOVE_NORMAL_sid = fklAddSymbolCstr("above-normal", st)->v;
    pd->UV_PRIORITY_HIGH_sid = fklAddSymbolCstr("high", st)->v;
    pd->UV_PRIORITY_HIGHEST_sid = fklAddSymbolCstr("highest", st)->v;

    pd->ifa_f_name_sid = fklAddSymbolCstr("name", st)->v;
    pd->ifa_f_mac_sid = fklAddSymbolCstr("mac", st)->v;
    pd->ifa_f_internal_sid = fklAddSymbolCstr("internal", st)->v;
    pd->ifa_f_ip_sid = fklAddSymbolCstr("ip", st)->v;
    pd->ifa_f_netmask_sid = fklAddSymbolCstr("netmask", st)->v;
    pd->ifa_f_family_sid = fklAddSymbolCstr("family", st)->v;

    pd->metrics_loop_count_sid = fklAddSymbolCstr("loop-count", st)->v;
    pd->metrics_events_sid = fklAddSymbolCstr("events", st)->v;
    pd->metrics_events_waiting_sid = fklAddSymbolCstr("events-waiting", st)->v;
}

static int fuv_loop_p(FKL_CPROC_ARGL) { PREDICATE(isFuvLoop(val)) }

static int fuv_make_loop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    int err = 0;
    FklVMvalue *r = createFuvLoop(exe, ctx->proc, &err);
    CHECK_UV_RESULT(err, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, r);
    return 0;
}

#define CHECK_LOOP_OPEN(LOOP, EXE, PD)                                         \
    if (fuvLoopIsClosed(LOOP))                                                 \
        FKL_RAISE_BUILTIN_ERROR_FMT(FKL_ERR_INVALIDACCESS, EXE,                \
                                    "loop is closed");

static int fuv_loop_close(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    CHECK_LOOP_OPEN(fuv_loop, exe, ctx->pd);
    int r = uv_loop_close(&fuv_loop->loop);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    fuvLoopSetClosed(fuv_loop);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

#define LOOP_RUN_STATE(CTX) ((CTX)->c[0].uptr)
#define LOOP_RUN_FRAME(CTX) (*FKL_TYPE_CAST(FklVMframe **, &(CTX)->c[1].ptr))
#define LOOP_RUN_STATE_RUN (0)
#define LOOP_RUN_STATE_RETURN (1)

static int fuv_loop_run(FKL_CPROC_ARGL) {
    switch (LOOP_RUN_STATE(ctx)) {
    case LOOP_RUN_STATE_RUN: {
        FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
        FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *mode_obj = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
        FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
        FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
        FklVMframe *origin_top_frame = exe->top_frame;
        fuv_loop->data.exe = exe;
        int need_continue = 0;
        int mode = UV_RUN_DEFAULT;
        if (mode_obj) {
            FKL_CHECK_TYPE(mode_obj, FKL_IS_SYM, exe);
            FKL_DECL_VM_UD_DATA(pbd, FuvPublicData, ctx->pd);
            FklSid_t mode_id = FKL_GET_SYM(mode_obj);
            for (; mode < (int)(sizeof(pbd->loop_mode) / sizeof(FklSid_t));
                 mode++)
                if (pbd->loop_mode[mode] == mode_id)
                    break;
            if (mode > UV_RUN_NOWAIT)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
        }
        if (setjmp(fuv_loop->data.buf)) {
            need_continue = 1;
            LOOP_RUN_FRAME(ctx) = fklMoveVMframeToTop(exe, origin_top_frame);
            LOOP_RUN_STATE(ctx) = LOOP_RUN_STATE_RETURN;
        } else {
            fuv_loop->data.mode = mode;
            fklUnlockThread(exe);
            int r = uv_run(&fuv_loop->loop, mode);
            fklLockThread(exe);
            if (r < 0) {
                FKL_VM_PUSH_VALUE(exe, createUvError(r, exe, ctx->pd));
                LOOP_RUN_STATE(ctx) = LOOP_RUN_STATE_RETURN;
                need_continue = 1;
            } else {
                FKL_CPROC_RETURN(exe, ctx, loop_obj);
            }
            while (exe->top_frame != origin_top_frame)
                fklPopVMframe(exe);
        }
        fuv_loop->data.mode = -1;
        fuv_loop->data.exe = NULL;
        exe->state = FKL_VM_READY;
        return need_continue;
    } break;
    case LOOP_RUN_STATE_RETURN: {
        FklVMframe *prev = LOOP_RUN_FRAME(ctx);
        fklInsertTopVMframeAsPrev(exe, prev);
        fklRaiseVMerror(FKL_VM_POP_TOP_VALUE(exe), exe);
    } break;
    default:
        fprintf(stderr, "[%s: %d] unreachable!\n", __FILE__, __LINE__);
        fklPrintBacktrace(exe, stderr);
        abort();
        break;
    }
    return 0;
}

struct WalkCtx {
    FklVM *exe;
    FklVMvalue *ev;
};

static void fuv_loop_walk_cb(uv_handle_t *handle, void *arg) {
    struct WalkCtx *walk_ctx = arg;
    if (walk_ctx->ev)
        return;
    FuvLoopData *floop_data = uv_loop_get_data(uv_handle_get_loop(handle));
    if (handle == (uv_handle_t *)&floop_data->error_check_idle)
        return;
    FklVM *exe = walk_ctx->exe;
    uint32_t const old_tp = exe->tp;
    fklLockThread(exe);
    FklVMvalue *proc = FKL_VM_GET_TOP_VALUE(exe);
    exe->state = FKL_VM_READY;
    FklVMframe *buttom_frame = exe->top_frame;
    FuvHandle *fuv_handle = uv_handle_get_data(handle);
    FuvHandleData *handle_data = &fuv_handle->data;
    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, proc);
    FKL_VM_PUSH_VALUE(exe, handle_data->handle);
    fklCallObj(exe, proc);
    if (exe->thread_run_cb(exe, buttom_frame))
        walk_ctx->ev = FKL_VM_GET_TOP_VALUE(exe);
    else
        exe->tp = old_tp;
    fklUnlockThread(exe);
}

static int fuv_loop_walk(FKL_CPROC_ARGL) {
    switch (LOOP_RUN_STATE(ctx)) {
    case LOOP_RUN_STATE_RUN: {
        FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
        FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
        FklVMvalue *proc_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
        FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
        FKL_CHECK_TYPE(proc_obj, fklIsCallable, exe);
        FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
        FklVMframe *origin_top_frame = exe->top_frame;
        int need_continue = 0;
        struct WalkCtx walk_ctx = {
            .exe = exe,
            .ev = NULL,
        };

        fklUnlockThread(exe);
        uv_walk(&fuv_loop->loop, fuv_loop_walk_cb, &walk_ctx);
        fklLockThread(exe);
        if (walk_ctx.ev) {
            need_continue = 1;
            LOOP_RUN_FRAME(ctx) = fklMoveVMframeToTop(exe, origin_top_frame);
            LOOP_RUN_STATE(ctx) = LOOP_RUN_STATE_RETURN;
        } else {
            while (exe->top_frame != origin_top_frame)
                fklPopVMframe(exe);
            FKL_CPROC_RETURN(exe, ctx, loop_obj);
        }
        exe->state = FKL_VM_READY;
        return need_continue;
    } break;
    case LOOP_RUN_STATE_RETURN: {
        FklVMframe *prev = LOOP_RUN_FRAME(ctx);
        fklInsertTopVMframeAsPrev(exe, prev);
        fklRaiseVMerror(FKL_VM_POP_TOP_VALUE(exe), exe);
    } break;
    }
    return 0;
}

#undef LOOP_RUN_STATE
#undef LOOP_RUN_FRAME
#undef LOOP_RUN_STATE_RUN
#undef LOOP_RUN_STATE_RETURN

static int fuv_loop_configure(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *loop_obj = arg_base[0];
    FklVMvalue *option_obj = arg_base[1];
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(option_obj, FKL_IS_SYM, exe);
    FklSid_t option_id = FKL_GET_SYM(option_obj);
    uv_loop_option option = UV_LOOP_BLOCK_SIGNAL;
    FklVMvalue *pd = ctx->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    if (option_id == fpd->loop_block_signal_sid)
        option = UV_LOOP_BLOCK_SIGNAL;
    else if (option_id == fpd->metrics_idle_time_sid)
        option = UV_METRICS_IDLE_TIME;
    else if (option_id == fpd->loop_use_io_uring_sqpoll)
        option = UV_LOOP_USE_IO_URING_SQPOLL;
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    uv_loop_t *loop = &fuv_loop->loop;
    switch (option) {
    case UV_LOOP_BLOCK_SIGNAL: {
        FklVMvalue **const end = arg_base + argc;
        for (FklVMvalue **parg = arg_base + 2; parg < end; ++parg) {
            FklVMvalue *cur = *parg;
            if (FKL_IS_FIX(cur)) {
                int64_t i = FKL_GET_FIX(cur);
                int r = uv_loop_configure(loop, UV_LOOP_BLOCK_SIGNAL, i);
                CHECK_UV_RESULT(r, exe, pd);
            } else if (FKL_IS_SYM(cur)) {
                FklSid_t sid = FKL_GET_SYM(cur);
                int signum = symbolToSignum(sid, fpd);
                if (signum <= 0)
                    FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
                int r = uv_loop_configure(loop, UV_LOOP_BLOCK_SIGNAL, signum);
                CHECK_UV_RESULT(r, exe, pd);
            } else
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        }
    } break;
    case UV_METRICS_IDLE_TIME:
    case UV_LOOP_USE_IO_URING_SQPOLL: {
        if (argc > 2)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, exe);
        int r = uv_loop_configure(loop, option);
        CHECK_UV_RESULT(r, exe, pd);
    } break;
    }
    FKL_CPROC_RETURN(exe, ctx, loop_obj);
    return 0;
}

static int fuv_loop_alive_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    CHECK_LOOP_OPEN(fuv_loop, exe, ctx->pd);
    int r = uv_loop_alive(&fuv_loop->loop);
    FKL_CPROC_RETURN(exe, ctx, r ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int fuv_loop_stop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    CHECK_LOOP_OPEN(fuv_loop, exe, ctx->pd);
    uv_stop(&fuv_loop->loop);
    FKL_CPROC_RETURN(exe, ctx, loop_obj);
    return 0;
}

static int fuv_loop_mode(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    int r = fuv_loop->data.mode;
    if (r == -1)
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    else {
        FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
        FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_SYM(fpd->loop_mode[r]));
    }
    return 0;
}

static int fuv_loop_backend_fd(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    CHECK_LOOP_OPEN(fuv_loop, exe, ctx->pd);
    int r = uv_backend_fd(&fuv_loop->loop);
    FKL_CPROC_RETURN(exe, ctx, r == -1 ? FKL_VM_NIL : FKL_MAKE_VM_FIX(r));
    return 0;
}

static int fuv_loop_backend_timeout(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    CHECK_LOOP_OPEN(fuv_loop, exe, ctx->pd);
    int r = uv_backend_timeout(&fuv_loop->loop);
    FKL_CPROC_RETURN(exe, ctx, r == -1 ? FKL_VM_NIL : FKL_MAKE_VM_FIX(r));
    return 0;
}

static int fuv_loop_now(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    CHECK_LOOP_OPEN(fuv_loop, exe, ctx->pd);
    uint64_t r = uv_now(&fuv_loop->loop);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(r, exe));
    return 0;
}

static int fuv_loop_update_time(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    CHECK_LOOP_OPEN(fuv_loop, exe, ctx->pd);
    uv_update_time(&fuv_loop->loop);
    FKL_CPROC_RETURN(exe, ctx, loop_obj);
    return 0;
}

static int fuv_handle_p(FKL_CPROC_ARGL) { PREDICATE(isFuvHandle(val)) }

static int fuv_handle_active_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *handle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(handle_obj, isFuvHandle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle, handle_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx,
                     uv_is_active(GET_HANDLE(*fuv_handle)) ? FKL_VM_TRUE
                                                           : FKL_VM_NIL);
    return 0;
}

static int fuv_handle_closing_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *handle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(handle_obj, isFuvHandle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle, handle_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx,
                     uv_is_closing(GET_HANDLE(*fuv_handle)) ? FKL_VM_TRUE
                                                            : FKL_VM_NIL);
    return 0;
}

static inline void fuv_call_handle_callback_in_loop(uv_handle_t *handle,
                                                    FuvHandleData *handle_data,
                                                    uv_loop_t *loop,
                                                    FuvLoopData *loop_data,
                                                    int idx) {
    FklVMvalue *proc = handle_data->callbacks[idx];
    if (proc) {
        FklVM *exe = loop_data->exe;
        fklLockThread(exe);
        uint32_t const sbp = exe->bp;
        uint32_t const stp = exe->tp;
        FklVMframe *buttom_frame = exe->top_frame;
        exe->state = FKL_VM_READY;
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, proc);
        fklCallObj(exe, proc);
        if (exe->thread_run_cb(exe, buttom_frame))
            startErrorHandle(loop, loop_data, exe, sbp, stp, buttom_frame);
        else
            exe->tp = stp;
        fklUnlockThread(exe);
    }
}

static void fuv_close_cb(uv_handle_t *handle) {
    uv_loop_t *loop = uv_handle_get_loop(handle);
    FuvLoopData *ldata = uv_loop_get_data(loop);
    FuvHandle *fuv_handle = uv_handle_get_data(handle);
    FuvHandleData *hdata = &fuv_handle->data;
    fuv_call_handle_callback_in_loop(handle, hdata, loop, ldata, 1);
    FklVMvalue *handle_obj = hdata->handle;
    hdata->loop = NULL;
    free(fuv_handle);
    if (handle_obj) {
        fklVMvalueHashSetDel2(&ldata->gc_values, handle_obj);
        FKL_DECL_VM_UD_DATA(fuv_handle_ud, FuvHandleUd, handle_obj);
        *fuv_handle_ud = NULL;
    }
}

static int fuv_handle_close(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *handle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *proc_obj = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(handle_obj, isFuvHandle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle, handle_obj, exe, ctx->pd);
    FuvHandle *handle = *fuv_handle;
    uv_handle_t *hh = GET_HANDLE(handle);
    if (uv_is_closing(hh))
        raiseFuvError(FUV_ERR_CLOSE_CLOSEING_HANDLE, exe, ctx->pd);
    if (proc_obj) {
        FKL_CHECK_TYPE(proc_obj, fklIsCallable, exe);
        handle->data.callbacks[1] = proc_obj;
    }
    uv_close(hh, fuv_close_cb);
    FKL_CPROC_RETURN(exe, ctx, handle_obj);
    return 0;
}

static int fuv_handle_ref_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *handle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(handle_obj, isFuvHandle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle, handle_obj, exe, ctx->pd);
    FuvHandle *handle = *fuv_handle;
    FKL_CPROC_RETURN(exe, ctx,
                     uv_has_ref(GET_HANDLE(handle)) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int fuv_handle_ref(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *handle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(handle_obj, isFuvHandle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle, handle_obj, exe, ctx->pd);
    FuvHandle *handle = *fuv_handle;
    uv_ref(GET_HANDLE(handle));
    FKL_CPROC_RETURN(exe, ctx, handle_obj);
    return 0;
}

static int fuv_handle_unref(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *handle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(handle_obj, isFuvHandle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle, handle_obj, exe, ctx->pd);
    FuvHandle *handle = *fuv_handle;
    uv_unref(GET_HANDLE(handle));
    FKL_CPROC_RETURN(exe, ctx, handle_obj);
    return 0;
}

static int fuv_handle_send_buffer_size(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *handle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *value_obj = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(handle_obj, isFuvHandle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle, handle_obj, exe, ctx->pd);
    FuvHandle *handle = *fuv_handle;
    int value = 0;
    if (value_obj) {
        FKL_CHECK_TYPE(value_obj, fklIsVMint, exe);
        value = fklVMgetInt(value_obj);
    }
    int r = uv_send_buffer_size(GET_HANDLE(handle), &value);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(value));
    return 0;
}

static int fuv_handle_recv_buffer_size(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *handle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *value_obj = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(handle_obj, isFuvHandle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle, handle_obj, exe, ctx->pd);
    FuvHandle *handle = *fuv_handle;
    int value = 0;
    if (value_obj) {
        FKL_CHECK_TYPE(value_obj, fklIsVMint, exe);
        value = fklVMgetInt(value_obj);
    }
    int r = uv_recv_buffer_size(GET_HANDLE(handle), &value);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(value));
    return 0;
}

static int fuv_handle_fileno(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *handle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(handle_obj, isFuvHandle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle, handle_obj, exe, ctx->pd);
    FuvHandle *handle = *fuv_handle;
    uv_os_fd_t fd = 0;
    int r = uv_fileno(GET_HANDLE(handle), &fd);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint((int64_t)(ptrdiff_t)fd, exe));
    return 0;
}

static int fuv_handle_type(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *handle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(handle_obj, isFuvHandle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle, handle_obj, exe, ctx->pd);
    FuvHandle *handle = *fuv_handle;
    uv_handle_type type_id = uv_handle_get_type(GET_HANDLE(handle));
    const char *name = uv_handle_type_name(type_id);
    FKL_CPROC_RETURN(
        exe, ctx,
        name == NULL
            ? FKL_VM_NIL
            : fklCreateVMvaluePair(exe, fklCreateVMvalueStrFromCstr(exe, name),
                                   FKL_MAKE_VM_FIX(type_id)));
    return 0;
}

static int fuv_handle_loop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *handle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(handle_obj, isFuvHandle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle, handle_obj, exe, ctx->pd);
    FuvHandle *handle = *fuv_handle;
    FKL_CPROC_RETURN(exe, ctx, handle->data.loop);
    return 0;
}

static int fuv_timer_p(FKL_CPROC_ARGL) { PREDICATE(isFuvTimer(val)) }

static int fuv_make_timer(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    int r;
    FklVMvalue *timer_obj = createFuvTimer(exe, ctx->proc, loop_obj, &r);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, timer_obj, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, timer_obj);
    return 0;
}

static void fuv_timer_cb(uv_timer_t *handle) {
    uv_loop_t *loop = uv_handle_get_loop((uv_handle_t *)handle);
    FuvLoopData *ldata = uv_loop_get_data(loop);
    FuvHandleData *hdata =
        &((FuvHandle *)uv_handle_get_data((uv_handle_t *)handle))->data;
    fuv_call_handle_callback_in_loop((uv_handle_t *)handle, hdata, loop, ldata,
                                     0);
}

static int fuv_timer_start(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 4);
    FklVMvalue *timer_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *timer_cb = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *timeout_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *repeat_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FKL_CHECK_TYPE(timer_obj, isFuvTimer, exe);
    FKL_CHECK_TYPE(timer_cb, fklIsCallable, exe);
    FKL_CHECK_TYPE(timeout_obj, fklIsVMint, exe);
    FKL_CHECK_TYPE(repeat_obj, fklIsVMint, exe);
    if (fklIsVMnumberLt0(timeout_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    if (fklIsVMnumberLt0(repeat_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    uint64_t timeout = fklVMgetUint(timeout_obj);
    uint64_t repeat = fklVMgetUint(repeat_obj);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer, timer_obj, exe, ctx->pd);
    FuvHandle *fuv_handle = *timer;
    fuv_handle->data.callbacks[0] = timer_cb;
    int r = uv_timer_start((uv_timer_t *)GET_HANDLE(fuv_handle), fuv_timer_cb,
                           timeout, repeat);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, timer_obj);
    return 0;
}

static int fuv_timer_stop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *timer_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(timer_obj, isFuvTimer, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer, timer_obj, exe, ctx->pd);
    FuvHandle *handle = *timer;
    int r = uv_timer_stop((uv_timer_t *)GET_HANDLE(handle));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, timer_obj);
    return 0;
}

static int fuv_timer_again(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *timer_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(timer_obj, isFuvTimer, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer, timer_obj, exe, ctx->pd);
    FuvHandle *handle = *timer;
    int r = uv_timer_again((uv_timer_t *)GET_HANDLE(handle));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, timer_obj);
    return 0;
}

static int fuv_timer_repeat(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *timer_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(timer_obj, isFuvTimer, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer, timer_obj, exe, ctx->pd);
    FuvHandle *handle = *timer;
    FKL_CPROC_RETURN(
        exe, ctx,
        fklMakeVMuint(uv_timer_get_repeat((uv_timer_t *)GET_HANDLE(handle)),
                      exe));
    return 0;
}

static int fuv_timer_repeat_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *timer_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *repeat_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(timer_obj, isFuvTimer, exe);
    FKL_CHECK_TYPE(repeat_obj, fklIsVMint, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer, timer_obj, exe, ctx->pd);
    FuvHandle *handle = *timer;
    if (fklIsVMnumberLt0(repeat_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    uv_timer_set_repeat((uv_timer_t *)GET_HANDLE(handle),
                        fklVMgetUint(repeat_obj));
    FKL_CPROC_RETURN(exe, ctx, timer_obj);
    return 0;
}

static int fuv_timer_due_in(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *timer_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(timer_obj, isFuvTimer, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer, timer_obj, exe, ctx->pd);
    FuvHandle *handle = *timer;
    FKL_CPROC_RETURN(
        exe, ctx,
        fklMakeVMuint(uv_timer_get_due_in((uv_timer_t *)GET_HANDLE(handle)),
                      exe));
    return 0;
}

static int fuv_prepare_p(FKL_CPROC_ARGL) { PREDICATE(isFuvPrepare(val)) }

static int fuv_make_prepare(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    int r;
    FklVMvalue *prepare_obj = createFuvPrepare(exe, ctx->proc, loop_obj, &r);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, prepare_obj, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, prepare_obj);
    return 0;
}

static void fuv_prepare_cb(uv_prepare_t *handle) {
    uv_loop_t *loop = uv_handle_get_loop((uv_handle_t *)handle);
    FuvLoopData *ldata = uv_loop_get_data(loop);
    FuvHandleData *hdata =
        &((FuvHandle *)uv_handle_get_data((uv_handle_t *)handle))->data;
    fuv_call_handle_callback_in_loop((uv_handle_t *)handle, hdata, loop, ldata,
                                     0);
}

static int fuv_prepare_start(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *prepare_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *prepare_cb = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(prepare_obj, isFuvPrepare, exe);
    FKL_CHECK_TYPE(prepare_cb, fklIsCallable, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(prepare, prepare_obj, exe, ctx->pd);
    FuvHandle *fuv_handle = *prepare;
    fuv_handle->data.callbacks[0] = prepare_cb;
    int r = uv_prepare_start((uv_prepare_t *)GET_HANDLE(fuv_handle),
                             fuv_prepare_cb);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, prepare_obj);
    return 0;
}

static int fuv_prepare_stop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *prepare_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(prepare_obj, isFuvPrepare, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(prepare, prepare_obj, exe, ctx->pd);
    FuvHandle *handle = *prepare;
    int r = uv_prepare_stop((uv_prepare_t *)GET_HANDLE(handle));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, prepare_obj);
    return 0;
}

static int fuv_idle_p(FKL_CPROC_ARGL) { PREDICATE(isFuvIdle(val)) }

static int fuv_make_idle(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    int r;
    FklVMvalue *idle = createFuvIdle(exe, ctx->proc, loop_obj, &r);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, idle, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, idle);
    return 0;
}

static void fuv_idle_cb(uv_idle_t *handle) {
    uv_loop_t *loop = uv_handle_get_loop((uv_handle_t *)handle);
    FuvLoopData *ldata = uv_loop_get_data(loop);
    FuvHandleData *hdata =
        &((FuvHandle *)uv_handle_get_data((uv_handle_t *)handle))->data;
    fuv_call_handle_callback_in_loop((uv_handle_t *)handle, hdata, loop, ldata,
                                     0);
}

static int fuv_idle_start(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *idle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *idle_cb = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(idle_obj, isFuvIdle, exe);
    FKL_CHECK_TYPE(idle_cb, fklIsCallable, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(idle, idle_obj, exe, ctx->pd);
    FuvHandle *fuv_handle = *idle;
    fuv_handle->data.callbacks[0] = idle_cb;
    int r = uv_idle_start((uv_idle_t *)GET_HANDLE(fuv_handle), fuv_idle_cb);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, idle_obj);
    return 0;
}

static int fuv_idle_stop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *idle_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(idle_obj, isFuvIdle, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(idle, idle_obj, exe, ctx->pd);
    FuvHandle *handle = *idle;
    int r = uv_idle_stop((uv_idle_t *)GET_HANDLE(handle));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, idle_obj);
    return 0;
}

static int fuv_check_p(FKL_CPROC_ARGL) { PREDICATE(isFuvCheck(val)) }

static int fuv_make_check(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    int r;
    FklVMvalue *check = createFuvCheck(exe, ctx->proc, loop_obj, &r);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, check, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, check);
    return 0;
}

static void fuv_check_cb(uv_check_t *handle) {
    uv_loop_t *loop = uv_handle_get_loop((uv_handle_t *)handle);
    FuvLoopData *ldata = uv_loop_get_data(loop);
    FuvHandleData *hdata =
        &((FuvHandle *)uv_handle_get_data((uv_handle_t *)handle))->data;
    fuv_call_handle_callback_in_loop((uv_handle_t *)handle, hdata, loop, ldata,
                                     0);
}

static int fuv_check_start(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *check_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *check_cb = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(check_obj, isFuvCheck, exe);
    FKL_CHECK_TYPE(check_cb, fklIsCallable, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(check, check_obj, exe, ctx->pd);
    FuvHandle *fuv_handle = *check;
    fuv_handle->data.callbacks[0] = check_cb;
    int r = uv_check_start((uv_check_t *)GET_HANDLE(fuv_handle), fuv_check_cb);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, check_obj);
    return 0;
}

static int fuv_check_stop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *check_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(check_obj, isFuvCheck, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(check, check_obj, exe, ctx->pd);
    FuvHandle *handle = *check;
    int r = uv_check_stop((uv_check_t *)GET_HANDLE(handle));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, check_obj);
    return 0;
}

static int fuv_signal_p(FKL_CPROC_ARGL) { PREDICATE(isFuvSignal(val)) }

static int fuv_make_signal(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    int r;
    FklVMvalue *signal = createFuvSignal(exe, ctx->proc, loop_obj, &r);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, signal, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, signal);
    return 0;
}

typedef int (*CallbackValueCreator)(FklVM *exe, void *arg);

static inline void fuv_call_handle_callback_in_loop_with_value_creator(
    uv_handle_t *handle, FuvHandleData *handle_data, FuvLoopData *loop_data,
    int idx, CallbackValueCreator creator, void *arg) {
    FklVMvalue *proc = handle_data->callbacks[idx];
    if (proc) {
        FklVM *exe = loop_data->exe;
        fklLockThread(exe);
        uint32_t const sbp = exe->bp;
        uint32_t const stp = exe->tp;
        FklVMframe *buttom_frame = exe->top_frame;
        exe->state = FKL_VM_READY;
        fklSetBp(exe);
        FKL_VM_PUSH_VALUE(exe, proc);
        if (creator)
            creator(exe, arg);
        fklCallObj(exe, proc);
        if (exe->thread_run_cb(exe, buttom_frame))
            startErrorHandle(uv_handle_get_loop(handle), loop_data, exe, sbp,
                             stp, buttom_frame);
        else
            exe->tp = stp;
        fklUnlockThread(exe);
    }
}

struct SignalCbValueCreateArg {
    FuvPublicData *fpd;
    int num;
};

static int fuv_signal_cb_value_creator(FklVM *exe, void *a) {
    struct SignalCbValueCreateArg *arg = a;
    FklVMvalue *signum_val =
        FKL_MAKE_VM_SYM(signumToSymbol(arg->num, arg->fpd));
    FKL_VM_PUSH_VALUE(exe, signum_val);
    return 0;
}

static void fuv_signal_cb(uv_signal_t *handle, int num) {
    FuvLoopData *ldata =
        uv_loop_get_data(uv_handle_get_loop((uv_handle_t *)handle));
    FuvHandleData *hdata =
        &((FuvHandle *)uv_handle_get_data((uv_handle_t *)handle))->data;
    FklVMvalue *fpd_obj =
        ((FklCprocFrameContext *)ldata->exe->top_frame->data)->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, fpd_obj);
    struct SignalCbValueCreateArg arg = {
        .fpd = fpd,
        .num = num,
    };
    fuv_call_handle_callback_in_loop_with_value_creator(
        (uv_handle_t *)handle, hdata, ldata, 0, fuv_signal_cb_value_creator,
        &arg);
}

static int fuv_signal_start(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
    FklVMvalue *signal_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *signal_cb = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *signum_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FKL_CHECK_TYPE(signal_obj, isFuvSignal, exe);
    FKL_CHECK_TYPE(signal_cb, fklIsCallable, exe);
    int signum = 0;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    if (FKL_IS_FIX(signum_obj))
        signum = FKL_GET_FIX(signum_obj);
    else if (FKL_IS_SYM(signum_obj))
        signum = symbolToSignum(FKL_GET_SYM(signum_obj), fpd);
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (signum <= 0)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);

    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(signal, signal_obj, exe, ctx->pd);
    FuvHandle *fuv_handle = *signal;
    fuv_handle->data.callbacks[0] = signal_cb;
    int r = uv_signal_start((uv_signal_t *)GET_HANDLE(fuv_handle),
                            fuv_signal_cb, signum);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, signal_obj);
    return 0;
}

static int fuv_signal_start_oneshot(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
    FklVMvalue *signal_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *signal_cb = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *signum_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FKL_CHECK_TYPE(signal_obj, isFuvSignal, exe);
    FKL_CHECK_TYPE(signal_cb, fklIsCallable, exe);
    int signum = 0;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    if (FKL_IS_FIX(signum_obj))
        signum = FKL_GET_FIX(signum_obj);
    else if (FKL_IS_SYM(signum_obj))
        signum = symbolToSignum(FKL_GET_SYM(signum_obj), fpd);
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (signum <= 0)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);

    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(signal, signal_obj, exe, ctx->pd);
    FuvHandle *fuv_handle = *signal;
    fuv_handle->data.callbacks[0] = signal_cb;
    int r = uv_signal_start_oneshot((uv_signal_t *)GET_HANDLE(fuv_handle),
                                    fuv_signal_cb, signum);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, signal_obj);
    return 0;
}

static int fuv_signal_stop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *signal_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(signal_obj, isFuvSignal, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(signal, signal_obj, exe, ctx->pd);
    FuvHandle *handle = *signal;
    int r = uv_signal_stop((uv_signal_t *)GET_HANDLE(handle));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, signal_obj);
    return 0;
}

static int fuv_async_p(FKL_CPROC_ARGL) { PREDICATE(isFuvAsync(val)) }

static int fuv_make_async(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *proc_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(proc_obj, fklIsCallable, exe);
    int r;
    FklVMvalue *async = createFuvAsync(exe, ctx->proc, loop_obj, proc_obj, &r);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, async, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, async);
    return 0;
}

static int fuv_async_send(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *async_obj = arg_base[0];
    FKL_CHECK_TYPE(async_obj, isFuvAsync, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(async, async_obj, exe, ctx->pd);
    FuvHandle *handle = *async;
    struct FuvAsync *async_handle = (struct FuvAsync *)handle;
    if (!atomic_flag_test_and_set(&async_handle->send_ready)) {
        struct FuvAsyncExtraData extra = {
            .num = argc - 1,
            .base = &arg_base[1],
        };
        atomic_store(&async_handle->extra, &extra);
        int r = uv_async_send(&async_handle->handle);
        FUV_ASYNC_WAIT_COPY(exe, async_handle);
        FUV_ASYNC_SEND_DONE(async_handle);
        CHECK_UV_RESULT(r, exe, ctx->pd);
    }
    FKL_CPROC_RETURN(exe, ctx, async_obj);
    return 0;
}

static int fuv_req_p(FKL_CPROC_ARGL) { PREDICATE(isFuvReq(val)) }

static int fuv_req_cancel(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *req_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(req_obj, isFuvReq, exe);
    FKL_DECL_VM_UD_DATA(fuv_req, FuvReqUd, req_obj);
    FuvReq *req = *fuv_req;
    CHECK_REQ_CANCELED(req, exe, ctx->pd);
    int r = uv_cancel(GET_REQ(req));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    uninitFuvReq(fuv_req);
    req->data.callback = NULL;
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int fuv_req_type(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *req_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(req_obj, isFuvReq, exe);
    FKL_DECL_VM_UD_DATA(fuv_req, FuvReqUd, req_obj);
    FuvReq *req = *fuv_req;
    CHECK_REQ_CANCELED(req, exe, ctx->pd);

    uv_req_type type_id = uv_req_get_type(GET_REQ(req));
    const char *name = uv_req_type_name(type_id);
    FKL_CPROC_RETURN(
        exe, ctx,
        name == NULL
            ? FKL_VM_NIL
            : fklCreateVMvaluePair(exe, fklCreateVMvalueStrFromCstr(exe, name),
                                   FKL_MAKE_VM_FIX(type_id)));
    return 0;
}

static int get_protonum_with_cstr(const char *name) {
    const struct protoent *proto;
    proto = getprotobyname(name);
    if (proto)
        return proto->p_proto;
    return -1;
}

static inline int sid_to_af_name(FklSid_t family_id, FuvPublicData *fpd) {
#ifdef AF_UNIX
    if (family_id == fpd->AF_UNIX_sid)
        return AF_UNIX;
#endif
#ifdef AF_INET
    if (family_id == fpd->AF_INET_sid)
        return AF_INET;
#endif
#ifdef AF_INET6
    if (family_id == fpd->AF_INET6_sid)
        return AF_INET6;
#endif
#ifdef AF_IPX
    if (family_id == fpd->AF_IPX_sid)
        return AF_IPX;
#endif
#ifdef AF_NETLINK
    if (family_id == fpd->AF_NETLINK_sid)
        return AF_NETLINK;
#endif
#ifdef AF_X25
    if (family_id == fpd->AF_X25_sid)
        return AF_X25;
#endif
#ifdef AF_AX25
    if (family_id == fpd->AF_AX25_sid)
        return AF_AX25;
#endif
#ifdef AF_ATMPVC
    if (family_id == fpd->AF_ATMPVC_sid)
        return AF_ATMPVC;
#endif
#ifdef AF_APPLETALK
    if (family_id == fpd->AF_APPLETALK_sid)
        return AF_APPLETALK;
#endif
#ifdef AF_PACKET
    if (family_id == fpd->AF_PACKET_sid)
        return AF_PACKET;
#endif
#ifdef AF_UNSPEC
    if (family_id == fpd->AF_UNSPEC_sid)
        return AF_UNSPEC;
#endif
    return -1;
}

static inline int sid_to_socktype(FklSid_t socktype_id, FuvPublicData *fpd) {
#ifdef SOCK_STREAM
    if (socktype_id == fpd->SOCK_STREAM_sid)
        return SOCK_STREAM;
#endif
#ifdef SOCK_DGRAM
    if (socktype_id == fpd->SOCK_DGRAM_sid)
        return SOCK_STREAM;
#endif
#ifdef SOCK_SEQPACKET
    if (socktype_id == fpd->SOCK_SEQPACKET_sid)
        return SOCK_SEQPACKET;
#endif
#ifdef SOCK_RAW
    if (socktype_id == fpd->SOCK_RAW_sid)
        return SOCK_RAW;
#endif
#ifdef SOCK_RDM
    if (socktype_id == fpd->SOCK_RDM_sid)
        return SOCK_RDM;
#endif
    return -1;
}

static inline FklBuiltinErrorType
get_addrinfo_hints(FklVMgc *gc, FklVMvalue **parg, FklVMvalue **const arg_end,
                   struct addrinfo *hints, FuvPublicData *fpd) {
#define GET_NEXT_ARG() ((parg < (arg_end - 1)) ? *(++parg) : NULL)
    for (; parg < arg_end; ++parg) {
        FklVMvalue *cur = *parg;
        if (FKL_IS_SYM(cur)) {
            FklSid_t id = FKL_GET_SYM(cur);
            if (id == fpd->f_family_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                if (!FKL_IS_SYM(cur))
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                int af_num = sid_to_af_name(FKL_GET_SYM(cur), fpd);
                if (af_num < 0)
                    return FKL_ERR_INVALID_VALUE;
                hints->ai_family = af_num;
                continue;
            } else if (id == fpd->f_socktype_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                if (!FKL_IS_SYM(cur))
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                int socktype = sid_to_socktype(FKL_GET_SYM(cur), fpd);
                if (socktype < 0)
                    return FKL_ERR_INVALID_VALUE;
                hints->ai_socktype = socktype;
                continue;
            } else if (id == fpd->f_protocol_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                if (FKL_IS_STR(cur)) {
                    const char *name = FKL_VM_STR(cur)->str;
                    int proto = get_protonum_with_cstr(name);
                    if (proto < 0)
                        return FKL_ERR_INVALID_VALUE;
                    hints->ai_protocol = proto;
                } else if (FKL_IS_SYM(cur)) {
                    const char *name =
                        fklVMgetSymbolWithId(gc, FKL_GET_SYM(cur))->k->str;
                    int proto = get_protonum_with_cstr(name);
                    if (proto < 0)
                        return FKL_ERR_INVALID_VALUE;
                    hints->ai_protocol = proto;
                } else
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                continue;
            }
            if (id == fpd->AI_ADDRCONFIG_sid) {
                hints->ai_flags |= AI_ADDRCONFIG;
                continue;
            }
#ifdef AI_V4MAPPED
            if (id == fpd->AI_V4MAPPED_sid) {
                hints->ai_flags |= AI_V4MAPPED;
                continue;
            }
#endif
#ifdef AI_ALL
            if (id == fpd->AI_ALL_sid) {
                hints->ai_flags |= AI_ALL;
                continue;
            }
#endif
            if (id == fpd->AI_NUMERICHOST_sid) {
                hints->ai_flags |= AI_NUMERICHOST;
                continue;
            }
            if (id == fpd->AI_PASSIVE_sid) {
                hints->ai_flags |= AI_PASSIVE;
                continue;
            }
            if (id == fpd->AI_NUMERICSERV_sid) {
                hints->ai_flags |= AI_NUMERICSERV;
                continue;
            }
            if (id == fpd->AI_CANONNAME_sid) {
                hints->ai_flags |= AI_CANONNAME;
                continue;
            }
            return FKL_ERR_INVALID_VALUE;
        } else
            return FKL_ERR_INCORRECT_TYPE_VALUE;
    }
    return 0;
#undef GET_NEXT_ARG
}

static inline FklVMvalue *af_num_to_symbol(int ai_family, FuvPublicData *fpd) {
#ifdef AF_UNIX
    if (ai_family == AF_UNIX)
        return FKL_MAKE_VM_SYM(fpd->AF_UNIX_sid);
#endif
#ifdef AF_INET
    if (ai_family == AF_INET)
        return FKL_MAKE_VM_SYM(fpd->AF_INET_sid);
#endif
#ifdef AF_INET6
    if (ai_family == AF_INET6)
        return FKL_MAKE_VM_SYM(fpd->AF_INET6_sid);
#endif
#ifdef AF_IPX
    if (ai_family == AF_IPX)
        return FKL_MAKE_VM_SYM(fpd->AF_IPX_sid);
#endif
#ifdef AF_NETLINK
    if (ai_family == AF_NETLINK)
        return FKL_MAKE_VM_SYM(fpd->AF_NETLINK_sid);
#endif
#ifdef AF_X25
    if (ai_family == AF_X25)
        return FKL_MAKE_VM_SYM(fpd->AF_X25_sid);
#endif
#ifdef AF_AX25
    if (ai_family == AF_AX25)
        return FKL_MAKE_VM_SYM(fpd->AF_AX25_sid);
#endif
#ifdef AF_ATMPVC
    if (ai_family == AF_ATMPVC)
        return FKL_MAKE_VM_SYM(fpd->AF_ATMPVC_sid);
#endif
#ifdef AF_APPLETALK
    if (ai_family == AF_APPLETALK)
        return FKL_MAKE_VM_SYM(fpd->AF_APPLETALK_sid);
#endif
#ifdef AF_PACKET
    if (ai_family == AF_PACKET)
        return FKL_MAKE_VM_SYM(fpd->AF_PACKET_sid);
#endif
    return FKL_VM_NIL;
}

static inline FklVMvalue *sock_num_to_symbol(int ai_socktype,
                                             FuvPublicData *fpd) {
#ifdef SOCK_STREAM
    if (ai_socktype == SOCK_STREAM)
        return FKL_MAKE_VM_SYM(fpd->SOCK_STREAM_sid);
#endif
#ifdef SOCK_DGRAM
    if (ai_socktype == SOCK_DGRAM)
        return FKL_MAKE_VM_SYM(fpd->SOCK_DGRAM_sid);
#endif
#ifdef SOCK_SEQPACKET
    if (ai_socktype == SOCK_SEQPACKET)
        return FKL_MAKE_VM_SYM(fpd->SOCK_SEQPACKET_sid);
#endif
#ifdef SOCK_RAW
    if (ai_socktype == SOCK_RAW)
        return FKL_MAKE_VM_SYM(fpd->SOCK_RAW_sid);
#endif
#ifdef SOCK_RDM
    if (ai_socktype == SOCK_RDM)
        return FKL_MAKE_VM_SYM(fpd->SOCK_RDM_sid);
#endif
    return FKL_VM_NIL;
}

static inline FklVMvalue *proto_num_to_symbol(int num, FklVM *exe) {
    struct protoent *proto = getprotobynumber(num);
    if (proto)
        return FKL_MAKE_VM_SYM(fklVMaddSymbolCstr(exe->gc, proto->p_name)->v);
    return FKL_VM_NIL;
}

static inline FklVMvalue *addrinfo_to_vmhash(FklVM *exe, struct addrinfo *info,
                                             FuvPublicData *fpd) {
    char ip[INET6_ADDRSTRLEN];
    FklVMvalue *v = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(v);
    const char *addr = NULL;
    int port = 0;
    if (info->ai_family == AF_INET || info->ai_family == AF_INET6) {
        addr = (const char *)&((struct sockaddr_in *)info->ai_addr)->sin_addr;
        port = ((struct sockaddr_in *)info->ai_addr)->sin_port;
    } else {
        addr = (const char *)&((struct sockaddr_in6 *)info->ai_addr)->sin6_addr;
        port = ((struct sockaddr_in6 *)info->ai_addr)->sin6_port;
    }
    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->f_family_sid),
                      af_num_to_symbol(info->ai_family, fpd));
    uv_inet_ntop(info->ai_family, addr, ip, INET6_ADDRSTRLEN);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->f_addr_sid),
                      fklCreateVMvalueStrFromCstr(exe, ip));

    if (ntohs(port))
        fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->f_port_sid),
                          FKL_MAKE_VM_FIX(ntohs(port)));
    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->f_socktype_sid),
                      sock_num_to_symbol(info->ai_socktype, fpd));
    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->f_protocol_sid),
                      proto_num_to_symbol(info->ai_protocol, exe));
    if (info->ai_canonname)
        fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->f_canonname_sid),
                          fklCreateVMvalueStrFromCstr(exe, info->ai_canonname));
    return v;
}

static inline FklVMvalue *addrinfo_to_value(FklVM *exe, struct addrinfo *info,
                                            FuvPublicData *fpd) {
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **pr = &r;
    for (; info; info = info->ai_next) {
        *pr = fklCreateVMvaluePairWithCar(exe,
                                          addrinfo_to_vmhash(exe, info, fpd));
        pr = &FKL_VM_CDR(*pr);
    }
    return r;
}

static void fuv_call_req_callback_in_loop_with_value_creator(
    uv_req_t *req, CallbackValueCreator creator, void *arg) {
    FuvReq *fuv_req = uv_req_get_data((uv_req_t *)req);
    FklVMvalue *proc = fuv_req->data.callback;
    FuvReqData *rdata = &fuv_req->data;
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, rdata->loop);
    FuvLoopData *ldata = &fuv_loop->data;
    FklVM *exe = ldata->exe;
    if (proc == NULL || exe == NULL) {
        if (rdata->req)
            uninitFuvReqValue(rdata->req);
        fuv_fs_req_cleanup(fuv_req);
        free(fuv_req);
        return;
    }
    fklLockThread(exe);

    exe->state = FKL_VM_READY;
    uint32_t const sbp = exe->bp;
    uint32_t const stp = exe->tp;
    FklVMframe *buttom_frame = exe->top_frame;
    fklSetBp(exe);
    FKL_VM_PUSH_VALUE(exe, proc);
    if (creator)
        if (creator(exe, arg))
            goto call;
    uninitFuvReqValue(rdata->req);
    fuv_fs_req_cleanup(fuv_req);
    free(fuv_req);
call:
    fklCallObj(exe, proc);
    if (exe->thread_run_cb(exe, buttom_frame))
        startErrorHandle(&fuv_loop->loop, ldata, exe, sbp, stp, buttom_frame);
    else
        exe->tp = stp;
    fklUnlockThread(exe);
}

struct GetaddrinfoValueCreateArg {
    struct addrinfo *res;
    int status;
};

static int fuv_getaddrinfo_cb_value_creator(FklVM *exe, void *a) {
    FklVMvalue *fpd_obj = ((FklCprocFrameContext *)exe->top_frame->data)->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, fpd_obj);

    struct GetaddrinfoValueCreateArg *arg = a;
    FklVMvalue *err = arg->status < 0
                        ? createUvErrorWithFpd(arg->status, exe, fpd)
                        : FKL_VM_NIL;
    FklVMvalue *res = addrinfo_to_value(exe, arg->res, fpd);
    FKL_VM_PUSH_VALUE(exe, err);
    FKL_VM_PUSH_VALUE(exe, res);
    return 0;
}

static void fuv_getaddrinfo_cb(uv_getaddrinfo_t *req, int status,
                               struct addrinfo *res) {
    struct GetaddrinfoValueCreateArg arg = {
        .res = res,
        .status = status,
    };
    fuv_call_req_callback_in_loop_with_value_creator(
        (uv_req_t *)req, fuv_getaddrinfo_cb_value_creator, &arg);
    uv_freeaddrinfo(res);
}

static int fuv_getaddrinfo(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *loop_obj = arg_base[0];
    FklVMvalue *node_obj = arg_base[1];
    FklVMvalue *service_obj = arg_base[2];
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FklVMvalue *proc_obj = argc > 3 ? arg_base[3] : NULL;
    const char *node = NULL;
    const char *service = NULL;
    if (FKL_IS_STR(node_obj))
        node = FKL_VM_STR(node_obj)->str;
    else if (node_obj != FKL_VM_NIL)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    if (FKL_IS_STR(service_obj))
        service = FKL_VM_STR(service_obj)->str;
    else if (service_obj != FKL_VM_NIL)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    if (proc_obj && proc_obj != FKL_VM_NIL && !fklIsCallable(proc_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    struct addrinfo hints = {
        .ai_flags = 0,
    };
    FklBuiltinErrorType err_type =
        argc > 4 ? get_addrinfo_hints(exe->gc, &arg_base[4], arg_base + argc,
                                      &hints, fpd)
                 : 0;
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    if (hints.ai_flags & AI_NUMERICSERV && service == NULL)
        service = "00";

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    if (!proc_obj || proc_obj == FKL_VM_NIL) {
        uv_getaddrinfo_t req;
        fklUnlockThread(exe);
        int r =
            uv_getaddrinfo(&fuv_loop->loop, &req, NULL, node, service, &hints);
        fklLockThread(exe);
        CHECK_UV_RESULT(r, exe, ctx->pd);
        FKL_CPROC_RETURN(exe, ctx, addrinfo_to_value(exe, req.addrinfo, fpd));
        uv_freeaddrinfo(req.addrinfo);
    } else {
        FklVMvalue *retval = NULL;
        uv_getaddrinfo_t *req =
            createFuvGetaddrinfo(exe, &retval, ctx->proc, loop_obj, proc_obj);
        int r = uv_getaddrinfo(&fuv_loop->loop, req, fuv_getaddrinfo_cb, node,
                               service, &hints);
        CHECK_UV_RESULT_AND_CLEANUP_REQ(r, retval, exe, ctx->pd);
        FKL_CPROC_RETURN(exe, ctx, retval);
    }
    return 0;
}

static int fuv_getaddrinfo_p(FKL_CPROC_ARGL) {
    PREDICATE(isFuvGetaddrinfo(val))
}
static int fuv_getnameinfo_p(FKL_CPROC_ARGL) {
    PREDICATE(isFuvGetnameinfo(val))
}
static int fuv_write_p(FKL_CPROC_ARGL) { PREDICATE(isFuvWrite(val)) }
static int fuv_shutdown_p(FKL_CPROC_ARGL) { PREDICATE(isFuvShutdown(val)) }
static int fuv_connect_p(FKL_CPROC_ARGL) { PREDICATE(isFuvConnect(val)) }
static int fuv_udp_send_p(FKL_CPROC_ARGL) { PREDICATE(isFuvUdpSend(val)) }
static int fuv_fs_req_p(FKL_CPROC_ARGL) { PREDICATE(isFuvFsReq(val)) }
static int fuv_random_p(FKL_CPROC_ARGL) { PREDICATE(isFuvRandom(val)) }

static inline FklBuiltinErrorType
get_sockaddr_flags(FklVMvalue **parg, FklVMvalue **const arg_end,
                   struct sockaddr_storage *addr, int *flags,
                   FuvPublicData *fpd, FklVMvalue **pip_obj,
                   FklVMvalue **pport_obj, int *uv_err) {
    const char *ip = NULL;
    int port = 0;
    int has_af = 0;
    int af_num = 0;
#define GET_NEXT_ARG() ((parg < (arg_end - 1)) ? *(++parg) : NULL)
    for (; parg < arg_end; ++parg) {
        FklVMvalue *cur = *parg;
        if (FKL_IS_SYM(cur)) {
            FklSid_t id = FKL_GET_SYM(cur);
            if (id == fpd->f_family_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                if (!FKL_IS_SYM(cur))
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                FklSid_t family_id = FKL_GET_SYM(cur);
                af_num = sid_to_af_name(family_id, fpd);
                if (af_num < 0)
                    return FKL_ERR_INVALID_VALUE;
                has_af = 1;
                continue;
            }
            if (id == fpd->f_ip_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                if (!FKL_IS_STR(cur))
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                ip = FKL_VM_STR(cur)->str;
                *pip_obj = cur;
                continue;
            }
            if (id == fpd->f_port_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                if (!FKL_IS_FIX(cur))
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                port = FKL_GET_FIX(cur);
                *pport_obj = cur;
                continue;
            }
#ifdef NI_NAMEREQD
            if (id == fpd->NI_NAMEREQD_sid) {
                (*flags) |= NI_NAMEREQD;
                continue;
            }
#endif
#ifdef NI_DGRAM
            if (id == fpd->NI_DGRAM_sid) {
                (*flags) |= NI_DGRAM;
                continue;
            }
#endif
#ifdef NI_NOFQDN
            if (id == fpd->NI_NOFQDN_sid) {
                (*flags) |= NI_NOFQDN;
                continue;
            }
#endif
#ifdef NI_NUMERICHOST
            if (id == fpd->NI_NUMERICHOST_sid) {
                (*flags) |= NI_NUMERICHOST;
                continue;
            }
#endif
#ifdef NI_NUMERICSERV
            if (id == fpd->NI_NUMERICSERV_sid) {
                (*flags) |= NI_NUMERICSERV;
                continue;
            }
#endif
#ifdef NI_IDN
            if (id == fpd->NI_IDN_sid) {
                (*flags) |= NI_IDN;
                continue;
            }
#endif
#ifdef NI_IDN_ALLOW_UNASSIGNED
            if (id == fpd->NI_IDN_ALLOW_UNASSIGNED_sid) {
                (*flags) |= NI_IDN_ALLOW_UNASSIGNED;
                continue;
            }
#endif
#ifdef NI_IDN_USE_STD3_ASCII_RULES
            if (id == fpd->NI_IDN_USE_STD3_ASCII_RULES_sid) {
                (*flags) |= NI_IDN_USE_STD3_ASCII_RULES;
                continue;
            }
#endif
            return FKL_ERR_INVALID_VALUE;
        } else
            return FKL_ERR_INCORRECT_TYPE_VALUE;
    }
    if (ip || port) {
        if (!ip)
            ip = "0.0.0.0";
        int r = 0;
        if (!(r = uv_ip4_addr(ip, port, (struct sockaddr_in *)addr))) {
            if (!has_af)
                addr->ss_family = AF_INET;
        } else if (!(r = uv_ip6_addr(ip, port, (struct sockaddr_in6 *)addr))) {
            if (!has_af)
                addr->ss_family = AF_INET6;
        }
        *uv_err = r;
    }
    if (has_af)
        addr->ss_family = af_num;
    return 0;
#undef GET_NEXT_ARG
}

static inline FklVMvalue *host_service_to_hash(FklVM *exe, const char *hostname,
                                               const char *service,
                                               FuvPublicData *fpd) {
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);
    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->f_hostname_sid),
                      hostname ? fklCreateVMvalueStrFromCstr(exe, hostname)
                               : FKL_VM_NIL);
    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->f_service_sid),
                      service ? fklCreateVMvalueStrFromCstr(exe, service)
                              : FKL_VM_NIL);
    return hash;
}

struct GetnameinfoValueCreateArg {
    const char *hostname;
    const char *service;
    int status;
};

static int fuv_getnameinfo_cb_value_creator(FklVM *exe, void *a) {
    struct GetnameinfoValueCreateArg *arg = a;
    FklVMvalue *fpd_obj = ((FklCprocFrameContext *)exe->top_frame->data)->pd;

    FklVMvalue *err =
        arg->status < 0 ? createUvError(arg->status, exe, fpd_obj) : FKL_VM_NIL;
    FklVMvalue *hostname = arg->hostname
                             ? fklCreateVMvalueStrFromCstr(exe, arg->hostname)
                             : FKL_VM_NIL;
    FklVMvalue *service = arg->service
                            ? fklCreateVMvalueStrFromCstr(exe, arg->service)
                            : FKL_VM_NIL;
    FKL_VM_PUSH_VALUE(exe, err);
    FKL_VM_PUSH_VALUE(exe, hostname);
    FKL_VM_PUSH_VALUE(exe, service);
    return 0;
}

static void fuv_getnameinfo_cb(uv_getnameinfo_t *req, int status,
                               const char *hostname, const char *service) {
    struct GetnameinfoValueCreateArg arg = {
        .hostname = hostname,
        .service = service,
        .status = status,
    };
    fuv_call_req_callback_in_loop_with_value_creator(
        (uv_req_t *)req, fuv_getnameinfo_cb_value_creator, &arg);
}

static int fuv_getnameinfo(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *loop_obj = arg_base[0];
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FklVMvalue *proc_obj = argc > 1 ? arg_base[1] : NULL;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    struct sockaddr_storage addr = {.ss_family = AF_UNSPEC};
    int flags = 0;

    FklVMvalue *ip_obj = FKL_VM_NIL;
    FklVMvalue *port_obj = FKL_VM_NIL;
    int r = 0;
    FklBuiltinErrorType error_type =
        argc > 2 ? get_sockaddr_flags(&arg_base[2], arg_base + argc, &addr,
                                      &flags, fpd, &ip_obj, &port_obj, &r)
                 : 0;
    if (error_type)
        FKL_RAISE_BUILTIN_ERROR(error_type, exe);
    CHECK_UV_RESULT(r, exe, ctx->pd);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    if (!proc_obj || proc_obj == FKL_VM_NIL) {
        uv_getnameinfo_t req;
        fklUnlockThread(exe);
        int r = uv_getnameinfo(&fuv_loop->loop, &req, NULL,
                               (struct sockaddr *)&addr, flags);
        fklLockThread(exe);
        CHECK_UV_RESULT(r, exe, ctx->pd);
        FKL_CPROC_RETURN(exe, ctx,
                         host_service_to_hash(exe, req.host, req.service, fpd));
    } else {
        FklVMvalue *retval = NULL;
        uv_getnameinfo_t *req =
            createFuvGetnameinfo(exe, &retval, ctx->proc, loop_obj, proc_obj);
        int r = uv_getnameinfo(&fuv_loop->loop, req, fuv_getnameinfo_cb,
                               (struct sockaddr *)&addr, flags);
        CHECK_UV_RESULT_AND_CLEANUP_REQ(r, retval, exe, ctx->pd);
        FKL_CPROC_RETURN(exe, ctx, retval);
    }
    return 0;
}

static int fuv_process_p(FKL_CPROC_ARGL) { PREDICATE(isFuvProcess(val)) }

static inline int is_string_list_and_get_len(const FklVMvalue *p,
                                             uint64_t *plen) {
    uint64_t len = 0;
    for (; p != FKL_VM_NIL; p = FKL_VM_CDR(p)) {
        if (!FKL_IS_PAIR(p) || !FKL_IS_STR(FKL_VM_CAR(p)))
            return 0;
        len++;
    }
    *plen = len;
    return 1;
}

static inline FklBuiltinErrorType
get_process_options(FklVMvalue **const rest_args, FklVMvalue **const arg_end,
                    uv_process_options_t *options, FuvPublicData *fpd,
                    int *uv_err, FklVMvalue **args_obj, FklVMvalue **env_obj,
                    FklVMvalue **stdio_obj, FklVMvalue **cwd_obj,
                    FuvErrorType *fuv_err) {
#define GET_NEXT_ARG() ((parg < (arg_end - 1)) ? *(++parg) : NULL)
    for (FklVMvalue **parg = rest_args; parg < arg_end; ++parg) {
        FklVMvalue *cur = *parg;
        if (FKL_IS_SYM(cur)) {
            FklSid_t id = FKL_GET_SYM(cur);
            if (id == fpd->f_args_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                uint64_t len = 0;
                if (!is_string_list_and_get_len(cur, &len))
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                free(options->args);
                char **args = (char **)calloc(len + 1, sizeof(char *));
                FKL_ASSERT(args);
                *args_obj = cur;
                for (uint64_t i = 0; i < len; i++) {
                    args[i] = FKL_VM_STR(FKL_VM_CAR(cur))->str;
                    cur = FKL_VM_CDR(cur);
                }
                options->args = args;
                continue;
            }
            if (id == fpd->f_env_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                uint64_t len = 0;
                if (!is_string_list_and_get_len(cur, &len))
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                free(options->env);
                char **env = (char **)calloc(len + 1, sizeof(char *));
                FKL_ASSERT(env);
                *env_obj = cur;
                for (uint64_t i = 0; i < len; i++) {
                    env[i] = FKL_VM_STR(FKL_VM_CAR(cur))->str;
                    cur = FKL_VM_CDR(cur);
                }
                options->env = env;
                continue;
            }
            if (id == fpd->f_cwd_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                if (!FKL_IS_STR(cur))
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                options->cwd = FKL_VM_STR(cur)->str;
                *cwd_obj = cur;
                continue;
            }
            if (id == fpd->f_stdio_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                if (!fklIsList(cur))
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                free(options->stdio);
                uint64_t len = fklVMlistLength(cur);
                options->stdio_count = len;
                *stdio_obj = cur;
                if (len == 0)
                    continue;
                uv_stdio_container_t *containers =
                    (uv_stdio_container_t *)malloc(len
                                                   * sizeof(*(options->stdio)));
                FKL_ASSERT(containers);
                options->stdio = containers;
                for (uint64_t i = 0; i < len; i++) {
                    FklVMvalue *stream = FKL_VM_CAR(cur);
                    if (stream == FKL_VM_NIL)
                        containers[i].flags = UV_IGNORE;
                    else if (FKL_IS_FIX(stream)) {
                        int fd = FKL_GET_FIX(stream);
                        containers[i].data.fd = fd;
                        containers[i].flags = UV_INHERIT_FD;
                    } else if (fklIsVMvalueFp(stream)) {
                        FklVMfp *fp = FKL_VM_FP(stream);
                        int fd = fklVMfpFileno(fp);
                        containers[i].data.fd = fd;
                        containers[i].flags = UV_INHERIT_FD;
                    } else if (isFuvStream(stream)) {
                        FKL_DECL_VM_UD_DATA(s, FuvHandleUd, stream);
                        if (*s == NULL) {
                            *fuv_err = FUV_ERR_HANDLE_CLOSED;
                            return 0;
                        }
                        uv_os_fd_t fd;
                        uv_stream_t *ss = (uv_stream_t *)GET_HANDLE(*s);
                        int err = uv_fileno((uv_handle_t *)ss, &fd);
                        if (err) {
                            int flags = UV_CREATE_PIPE;
                            if (i == 0 || i > 2)
                                flags |= UV_READABLE_PIPE;
                            if (i > 0)
                                flags |= UV_WRITABLE_PIPE;
                            containers[i].flags = (uv_stdio_flags)flags;
                        } else
                            containers[i].flags = UV_INHERIT_STREAM;
                        containers[i].data.stream = ss;
                    } else
                        return FKL_ERR_INCORRECT_TYPE_VALUE;
                    cur = FKL_VM_CDR(cur);
                }
                continue;
            }
            if (id == fpd->f_uid_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                if (!FKL_IS_FIX(cur))
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                options->flags |= UV_PROCESS_SETUID;
                options->uid = FKL_GET_FIX(cur);
                continue;
            }
            if (id == fpd->f_gid_sid) {
                cur = GET_NEXT_ARG();
                if (cur == NULL)
                    return FKL_ERR_TOOFEWARG;
                if (!FKL_IS_FIX(cur))
                    return FKL_ERR_INCORRECT_TYPE_VALUE;
                options->flags |= UV_PROCESS_SETUID;
                options->gid = FKL_GET_FIX(cur);
                continue;
            }
            if (id == fpd->UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS_sid) {
                options->flags |= UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS;
                continue;
            }
            if (id == fpd->UV_PROCESS_DETACHED_sid) {
                options->flags |= UV_PROCESS_DETACHED;
                continue;
            }
            if (id == fpd->UV_PROCESS_WINDOWS_HIDE_sid) {
                options->flags |= UV_PROCESS_WINDOWS_HIDE;
                continue;
            }
            if (id == fpd->UV_PROCESS_WINDOWS_HIDE_CONSOLE_sid) {
                options->flags |= UV_PROCESS_WINDOWS_HIDE_CONSOLE;
                continue;
            }
            if (id == fpd->UV_PROCESS_WINDOWS_HIDE_GUI_sid) {
                options->flags |= UV_PROCESS_WINDOWS_HIDE_GUI;
                continue;
            }
            return FKL_ERR_INVALID_VALUE;
        } else
            return FKL_ERR_INCORRECT_TYPE_VALUE;
    }
    return 0;
#undef GET_NEXT_ARG
}

static inline void clean_options(uv_process_options_t *options) {
    free(options->args);
    free(options->env);
    free(options->stdio);
}

struct ProcessExitValueCreateArg {
    FuvPublicData *fpd;
    int64_t exit_status;
    int term_signal;
};

static int fuv_process_exit_cb_value_creator(FklVM *exe, void *a) {
    struct ProcessExitValueCreateArg *arg = a;
    FklSid_t id = signumToSymbol(arg->term_signal, arg->fpd);
    FklVMvalue *signum_val = id ? FKL_MAKE_VM_SYM(id) : FKL_VM_NIL;
    FklVMvalue *exit_status = fklMakeVMint(arg->exit_status, exe);
    FKL_VM_PUSH_VALUE(exe, exit_status);
    FKL_VM_PUSH_VALUE(exe, signum_val);
    return 0;
}

static void fuv_process_exit_cb(uv_process_t *handle, int64_t exit_status,
                                int term_signal) {
    FuvLoopData *ldata =
        uv_loop_get_data(uv_handle_get_loop((uv_handle_t *)handle));
    FuvHandleData *hdata =
        &((FuvHandle *)uv_handle_get_data((uv_handle_t *)handle))->data;
    FklVMvalue *fpd_obj =
        ((FklCprocFrameContext *)ldata->exe->top_frame->data)->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, fpd_obj);
    struct ProcessExitValueCreateArg arg = {
        .fpd = fpd,
        .term_signal = term_signal,
        .exit_status = exit_status,
    };
    fuv_call_handle_callback_in_loop_with_value_creator(
        (uv_handle_t *)handle, hdata, ldata, 0,
        fuv_process_exit_cb_value_creator, &arg);
}

static int fuv_process_spawn(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *loop_obj = arg_base[0];
    FklVMvalue *file_obj = arg_base[1];
    FklVMvalue *exit_cb_obj = arg_base[2];
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(file_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(exit_cb_obj, fklIsCallable, exe);
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    uv_process_options_t options = {
        .file = FKL_VM_STR(file_obj)->str,
        .exit_cb = fuv_process_exit_cb,
    };
    int uv_err = 0;
    FuvErrorType fuv_err = 0;
    FklVMvalue *env_obj = NULL;
    FklVMvalue *args_obj = NULL;
    FklVMvalue *stdio_obj = NULL;
    FklVMvalue *cwd_obj = NULL;
    FklBuiltinErrorType fkl_err = get_process_options(
        arg_base + 3, arg_base + argc, &options, fpd, &uv_err, &env_obj,
        &args_obj, &stdio_obj, &cwd_obj, &fuv_err);
    CHECK_UV_RESULT(uv_err, exe, ctx->pd);
    if (fkl_err)
        FKL_RAISE_BUILTIN_ERROR(fkl_err, exe);
    if (fuv_err)
        raiseFuvError(fuv_err, exe, ctx->pd);
    FklVMvalue *retval = NULL;
    uv_process_t *handle =
        createFuvProcess(exe, &retval, ctx->proc, loop_obj, exit_cb_obj,
                         args_obj, env_obj, file_obj, stdio_obj, cwd_obj);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    uv_err = uv_spawn(&fuv_loop->loop, handle, &options);
    clean_options(&options);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(uv_err, retval, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, retval);
    return 0;
}

static int fuv_disable_stdio_inheritance(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uv_disable_stdio_inheritance();
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int fuv_kill(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *pid_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *signum_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(pid_obj, FKL_IS_FIX, exe);
    int signum = 0;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    if (FKL_IS_FIX(signum_obj))
        signum = FKL_GET_FIX(signum_obj);
    else if (FKL_IS_SYM(signum_obj))
        signum = symbolToSignum(FKL_GET_SYM(signum_obj), fpd);
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (signum <= 0)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    int pid = FKL_GET_FIX(pid_obj);
    int r = uv_kill(pid, signum);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int fuv_process_kill(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *process_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *signum_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(process_obj, isFuvProcess, exe);
    int signum = 0;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    if (FKL_IS_FIX(signum_obj))
        signum = FKL_GET_FIX(signum_obj);
    else if (FKL_IS_SYM(signum_obj))
        signum = symbolToSignum(FKL_GET_SYM(signum_obj), fpd);
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (signum <= 0)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(process_ud, process_obj, exe, ctx->pd);
    uv_process_t *process = (uv_process_t *)&(*process_ud)->handle;
    int r = uv_process_kill(process, signum);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int fuv_process_pid(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *process_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(process_obj, isFuvProcess, exe);

    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(process_ud, process_obj, exe, ctx->pd);
    uv_process_t *process = (uv_process_t *)GET_HANDLE(*process_ud);
    int r = uv_process_get_pid(process);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(r));
    return 0;
}

static int fuv_stream_p(FKL_CPROC_ARGL) { PREDICATE(isFuvStream(val)) }

static int fuv_stream_readable_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *stream_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(stream_obj, isFuvStream, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud, stream_obj, exe, ctx->pd);
    uv_stream_t *stream = (uv_stream_t *)GET_HANDLE(*stream_ud);
    FKL_CPROC_RETURN(exe, ctx,
                     uv_is_readable(stream) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int fuv_stream_writable_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *stream_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(stream_obj, isFuvStream, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud, stream_obj, exe, ctx->pd);
    uv_stream_t *stream = (uv_stream_t *)GET_HANDLE(*stream_ud);
    FKL_CPROC_RETURN(exe, ctx,
                     uv_is_writable(stream) ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int fuv_stream_write_queue_size(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *stream_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(stream_obj, isFuvStream, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud, stream_obj, exe, ctx->pd);
    uv_stream_t *stream = (uv_stream_t *)GET_HANDLE(*stream_ud);
    uint64_t size = uv_stream_get_write_queue_size(stream);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(size, exe));
    return 0;
}

static void fuv_alloc_cb(uv_handle_t *handle, size_t suggested_size,
                         uv_buf_t *buf) {
    char *base;
    if (!suggested_size)
        base = NULL;
    else {
        base = (char *)malloc(suggested_size);
        FKL_ASSERT(base);
    }
    buf->base = base;
    buf->len = suggested_size;
}

struct ReadValueCreateArg {
    FuvPublicData *fpd;
    ssize_t nread;
    const uv_buf_t *buf;
};

static int fuv_read_cb_value_creator(FklVM *exe, void *a) {
    struct ReadValueCreateArg *arg = (struct ReadValueCreateArg *)a;
    ssize_t nread = arg->nread;
    if (nread < 0) {
        FklVMvalue *err = createUvErrorWithFpd(nread, exe, arg->fpd);
        FKL_VM_PUSH_VALUE(exe, err);
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
    } else {
        const uv_buf_t *buf = arg->buf;
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        if (nread > 0)
            FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueStr2(exe, nread, buf->base));
        else
            FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
    }
    free(arg->buf->base);
    return 0;
}

static void fuv_read_cb(uv_stream_t *stream, ssize_t nread,
                        const uv_buf_t *buf) {
    FuvLoopData *ldata =
        uv_loop_get_data(uv_handle_get_loop((uv_handle_t *)stream));
    FuvHandleData *hdata =
        &((FuvHandle *)uv_handle_get_data((uv_handle_t *)stream))->data;
    FklVMvalue *fpd_obj =
        ((FklCprocFrameContext *)ldata->exe->top_frame->data)->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, fpd_obj);
    struct ReadValueCreateArg arg = {
        .fpd = fpd,
        .nread = nread,
        .buf = buf,
    };
    fuv_call_handle_callback_in_loop_with_value_creator(
        (uv_handle_t *)stream, hdata, ldata, 0, fuv_read_cb_value_creator,
        &arg);
}

static int fuv_stream_read_start(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *stream_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *cb_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(stream_obj, isFuvStream, exe);
    FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud, stream_obj, exe, ctx->pd);
    FuvHandle *handle = *stream_ud;
    uv_stream_t *stream = (uv_stream_t *)GET_HANDLE(handle);
    handle->data.callbacks[0] = cb_obj;
    int ret = uv_read_start(stream, fuv_alloc_cb, fuv_read_cb);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, stream_obj);
    return 0;
}

static int fuv_stream_read_stop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *stream_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(stream_obj, isFuvStream, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud, stream_obj, exe, ctx->pd);
    uv_stream_t *stream = (uv_stream_t *)GET_HANDLE(*stream_ud);
    int ret = uv_read_stop(stream);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, stream_obj);
    return 0;
}

static int fuv_req_cb_error_value_creator(FklVM *exe, void *a) {
    FklVMvalue *fpd_obj = ((FklCprocFrameContext *)exe->top_frame->data)->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, fpd_obj);

    int status = *(int *)a;
    FklVMvalue *err =
        status < 0 ? createUvErrorWithFpd(status, exe, fpd) : FKL_VM_NIL;
    FKL_VM_PUSH_VALUE(exe, err);
    return 0;
}

static void fuv_write_cb(uv_write_t *req, int status) {
    fuv_call_req_callback_in_loop_with_value_creator(
        (uv_req_t *)req, fuv_req_cb_error_value_creator, &status);
}

static inline FklBuiltinErrorType
setup_write_data(FklVMvalue **parg, FklVMvalue **const arg_end, uint32_t num,
                 FklVMvalue *write_obj, uv_buf_t **pbufs) {
    FKL_DECL_VM_UD_DATA(fuv_req, FuvReqUd, write_obj);
    struct FuvWrite *req = (struct FuvWrite *)*fuv_req;
    FklVMvalue **cur = req->write_objs;
    uv_buf_t *bufs = (uv_buf_t *)malloc(num * sizeof(uv_buf_t));
    FKL_ASSERT(bufs);
    for (uint32_t i = 0; parg < arg_end; ++i, ++parg, ++cur) {
        FklVMvalue *val = *parg;
        if (FKL_IS_STR(val)) {
            *cur = val;
            FklString *str = FKL_VM_STR(val);
            bufs[i].base = str->str;
            bufs[i].len = str->size;
        } else if (FKL_IS_BYTEVECTOR(val)) {
            *cur = val;
            FklBytevector *bvec = FKL_VM_BVEC(val);
            bufs[i].base = (char *)bvec->ptr;
            bufs[i].len = bvec->size;
        } else {
            free(bufs);
            return FKL_ERR_INCORRECT_TYPE_VALUE;
        }
    }
    *pbufs = bufs;
    return 0;
}

static inline int isSendableHandle(FklVMvalue *o) {
    return isFuvPipe(o) || isFuvTcp(o);
}

static int fuv_stream_write(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *stream_obj = arg_base[0];
    FklVMvalue *cb_obj = arg_base[1];
    FklVMvalue **rest_arg = &arg_base[2];
    FKL_CHECK_TYPE(stream_obj, isFuvStream, exe);
    if (cb_obj == FKL_VM_NIL)
        cb_obj = NULL;
    else if (!fklIsCallable(cb_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud, stream_obj, exe, ctx->pd);
    FuvHandle *handle = *stream_ud;
    FklVMvalue *write_obj = NULL;
    uv_write_t *write = NULL;
    FklVMvalue *send_handle_obj =
        isSendableHandle(*rest_arg) ? *(rest_arg++) : NULL;
    uv_buf_t *bufs = NULL;

    uint32_t const buf_count = argc - 2 - (send_handle_obj != NULL);
    write = createFuvWrite(exe, &write_obj, ctx->proc, handle->data.loop,
                           cb_obj, buf_count);
    FklBuiltinErrorType err_type = setup_write_data(
        rest_arg, arg_base + argc, buf_count, write_obj, &bufs);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    uv_stream_t *stream = (uv_stream_t *)GET_HANDLE(handle);
    int ret = 0;
    if (send_handle_obj) {
        DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(send_handle_ud, send_handle_obj,
                                            exe, ctx->pd);
        ret =
            uv_write2(write, stream, bufs, buf_count,
                      (uv_stream_t *)GET_HANDLE(*send_handle_ud), fuv_write_cb);
    } else
        ret = uv_write(write, stream, bufs, buf_count, fuv_write_cb);
    free(bufs);
    CHECK_UV_RESULT_AND_CLEANUP_REQ(ret, write_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, write_obj);
    return 0;
}

static inline FklBuiltinErrorType
setup_try_write_data(FklVMvalue **parg, FklVMvalue **const arg_end,
                     uint32_t num, uv_buf_t **pbufs) {
    uv_buf_t *bufs = (uv_buf_t *)malloc(num * sizeof(uv_buf_t));
    FKL_ASSERT(bufs);
    for (uint32_t i = 0; parg < arg_end; ++i, ++parg) {
        FklVMvalue *val = *parg;
        if (FKL_IS_STR(val)) {
            FklString *str = FKL_VM_STR(val);
            bufs[i].base = str->str;
            bufs[i].len = str->size;
        } else if (FKL_IS_BYTEVECTOR(val)) {
            FklBytevector *bvec = FKL_VM_BVEC(val);
            bufs[i].base = (char *)bvec->ptr;
            bufs[i].len = bvec->size;
        } else {
            free(bufs);
            return FKL_ERR_INCORRECT_TYPE_VALUE;
        }
    }
    *pbufs = bufs;
    return 0;
}

static int fuv_stream_try_write(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *stream_obj = arg_base[0];
    FklVMvalue **rest_arg = &arg_base[1];
    FKL_CHECK_TYPE(stream_obj, isFuvStream, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud, stream_obj, exe, ctx->pd);
    FklVMvalue *send_handle_obj =
        isSendableHandle(*rest_arg) ? *(rest_arg++) : NULL;
    uv_buf_t *bufs = NULL;

    uint32_t const buf_count = argc - 1 - (send_handle_obj != NULL);
    FklBuiltinErrorType err_type =
        setup_try_write_data(rest_arg, arg_base + argc, buf_count, &bufs);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    uv_stream_t *stream = (uv_stream_t *)GET_HANDLE(*stream_ud);
    int ret = 0;
    if (send_handle_obj) {
        DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(send_handle_ud, send_handle_obj,
                                            exe, ctx->pd);
        ret = uv_try_write2(stream, bufs, buf_count,
                            (uv_stream_t *)GET_HANDLE(*send_handle_ud));
    } else
        ret = uv_try_write(stream, bufs, buf_count);
    free(bufs);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(ret));
    return 0;
}

static void fuv_shutdown_cb(uv_shutdown_t *req, int status) {
    fuv_call_req_callback_in_loop_with_value_creator(
        (uv_req_t *)req, fuv_req_cb_error_value_creator, &status);
}

static int fuv_stream_shutdown(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *stream_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *cb_obj = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    FKL_CHECK_TYPE(stream_obj, isFuvStream, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud, stream_obj, exe, ctx->pd);
    FuvHandle *handle = *stream_ud;
    FklVMvalue *shutdown_obj = NULL;
    uv_shutdown_t *write = createFuvShutdown(exe, &shutdown_obj, ctx->proc,
                                             handle->data.loop, cb_obj);
    uv_stream_t *stream = (uv_stream_t *)GET_HANDLE(*stream_ud);
    int ret = uv_shutdown(write, stream, fuv_shutdown_cb);
    CHECK_UV_RESULT_AND_CLEANUP_REQ(ret, shutdown_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, shutdown_obj);
    return 0;
}

struct ConnectionValueCreateArg {
    FuvPublicData *fpd;
    int status;
};

static int fuv_connection_cb_value_creator(FklVM *exe, void *a) {
    struct ConnectionValueCreateArg *arg = a;
    FklVMvalue *err = arg->status < 0
                        ? createUvErrorWithFpd(arg->status, exe, arg->fpd)
                        : FKL_VM_NIL;
    FKL_VM_PUSH_VALUE(exe, err);
    return 0;
}

static void fuv_connection_cb(uv_stream_t *handle, int status) {
    FuvLoopData *ldata =
        uv_loop_get_data(uv_handle_get_loop((uv_handle_t *)handle));
    FuvHandleData *hdata =
        &((FuvHandle *)uv_handle_get_data((uv_handle_t *)handle))->data;
    FklVMvalue *fpd_obj =
        ((FklCprocFrameContext *)ldata->exe->top_frame->data)->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, fpd_obj);
    struct ConnectionValueCreateArg arg = {
        .fpd = fpd,
        .status = status,
    };
    fuv_call_handle_callback_in_loop_with_value_creator(
        (uv_handle_t *)handle, hdata, ldata, 0, fuv_connection_cb_value_creator,
        &arg);
}

static int fuv_stream_listen(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
    FklVMvalue *server_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *backlog_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FKL_CHECK_TYPE(server_obj, isFuvStream, exe);
    FKL_CHECK_TYPE(backlog_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(server_ud, server_obj, exe, ctx->pd);
    FuvHandle *handle = *server_ud;
    uv_stream_t *server = (uv_stream_t *)GET_HANDLE(handle);

    handle->data.callbacks[0] = cb_obj;
    int ret = uv_listen(server, FKL_GET_FIX(backlog_obj), fuv_connection_cb);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, server_obj);
    return 0;
}

static int fuv_stream_accept(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *server_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *client_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(server_obj, isFuvStream, exe);
    FKL_CHECK_TYPE(client_obj, isFuvStream, exe);

    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(server_ud, server_obj, exe, ctx->pd);
    uv_stream_t *server = (uv_stream_t *)GET_HANDLE(*server_ud);

    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(client_ud, client_obj, exe, ctx->pd);
    uv_stream_t *client = (uv_stream_t *)GET_HANDLE(*client_ud);

    int ret = uv_accept(server, client);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, client_obj);
    return 0;
}

static int fuv_stream_blocking_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *stream_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *set_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(stream_obj, isFuvStream, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud, stream_obj, exe, ctx->pd);
    uv_stream_t *stream = (uv_stream_t *)GET_HANDLE(*stream_ud);
    int ret = uv_stream_set_blocking(stream, set_obj == FKL_VM_NIL ? 0 : 1);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, stream_obj);
    return 0;
}

static int fuv_pipe_p(FKL_CPROC_ARGL) { PREDICATE(isFuvPipe(val)) }

static int fuv_make_pipe(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *ipc_obj = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    int ipc = ipc_obj && ipc_obj != FKL_VM_NIL ? 1 : 0;
    FklVMvalue *retval = NULL;
    uv_pipe_t *pipe = createFuvPipe(exe, &retval, ctx->proc, loop_obj);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    int r = uv_pipe_init(&fuv_loop->loop, pipe, ipc);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, retval, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, retval);
    return 0;
}

static int fuv_pipe(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *read_flags_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *write_flags_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    int read_flags = 0;
    int write_flags = 0;
    if (read_flags_obj != FKL_VM_NIL)
        read_flags |= UV_NONBLOCK_PIPE;
    if (write_flags_obj != FKL_VM_NIL)
        write_flags |= UV_NONBLOCK_PIPE;
    uv_file fds[2];
    int ret = uv_pipe(fds, read_flags, write_flags);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx,
                     fklCreateVMvaluePair(exe, FKL_MAKE_VM_FIX(fds[0]),
                                          FKL_MAKE_VM_FIX(fds[1])));
    return 0;
}

static int fuv_pipe_open(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *pipe_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(pipe_obj, isFuvPipe, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, pipe_obj, exe, ctx->pd);
    struct FuvPipe *handle = (struct FuvPipe *)*handle_ud;
    uv_pipe_t *pipe = (uv_pipe_t *)GET_HANDLE(handle);
    int ret = 0;
    if (fklIsVMvalueFp(fd_obj)) {
        handle->fp = fd_obj;
        ret = uv_pipe_open(pipe, fklVMfpFileno(FKL_VM_FP(fd_obj)));
    } else if (FKL_IS_FIX(fd_obj))
        ret = uv_pipe_open(pipe, FKL_GET_FIX(fd_obj));
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, pipe_obj);
    return 0;
}

static int fuv_pipe_bind(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *pipe_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *name_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *no_truncate = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(pipe_obj, isFuvPipe, exe);
    FKL_CHECK_TYPE(name_obj, FKL_IS_STR, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, pipe_obj, exe, ctx->pd);
    FuvHandle *handle = *handle_ud;
    uv_pipe_t *pipe = (uv_pipe_t *)GET_HANDLE(handle);
    FklString *str = FKL_VM_STR(name_obj);
    int flags =
        no_truncate && no_truncate != FKL_VM_NIL ? UV_PIPE_NO_TRUNCATE : 0;
    int ret = uv_pipe_bind2(pipe, str->str, str->size, flags);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, pipe_obj);
    return 0;
}

static void fuv_connect_cb(uv_connect_t *req, int status) {
    fuv_call_req_callback_in_loop_with_value_creator(
        (uv_req_t *)req, fuv_req_cb_error_value_creator, &status);
}

static int fuv_pipe_connect(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 4);
    FklVMvalue *pipe_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *name_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *no_truncate = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    if (cb_obj == FKL_VM_NIL)
        cb_obj = NULL;
    else if (!fklIsCallable(cb_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    FKL_CHECK_TYPE(pipe_obj, isFuvPipe, exe);
    FKL_CHECK_TYPE(name_obj, FKL_IS_STR, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, pipe_obj, exe, ctx->pd);
    FuvHandle *handle = *handle_ud;
    uv_pipe_t *pipe = (uv_pipe_t *)GET_HANDLE(handle);
    FklVMvalue *connect_obj = NULL;
    uv_connect_t *connect = createFuvConnect(exe, &connect_obj, ctx->proc,
                                             handle->data.loop, cb_obj);
    FklString *str = FKL_VM_STR(name_obj);
    int flags =
        no_truncate && no_truncate != FKL_VM_NIL ? UV_PIPE_NO_TRUNCATE : 0;
    int ret = uv_pipe_connect2(connect, pipe, str->str, str->size, flags,
                               fuv_connect_cb);
    CHECK_UV_RESULT_AND_CLEANUP_REQ(ret, connect_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, connect_obj);
    return 0;
}

static int fuv_pipe_chmod(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *pipe_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *flags_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(pipe_obj, isFuvPipe, exe);
    FKL_CHECK_TYPE(flags_obj, FKL_IS_STR, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, pipe_obj, exe, ctx->pd);
    FuvHandle *handle = *handle_ud;
    uv_pipe_t *pipe = (uv_pipe_t *)GET_HANDLE(handle);
    int flags = 0;
    FklString *str = FKL_VM_STR(flags_obj);
    if (!fklStringCstrCmp(str, "r"))
        flags = UV_READABLE;
    else if (!fklStringCstrCmp(str, "w"))
        flags = UV_WRITABLE;
    else if ((!fklStringCstrCmp(str, "rw")) || (!fklStringCstrCmp(str, "wr")))
        flags = UV_READABLE | UV_WRITABLE;
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    fklUnlockThread(exe);
    int ret = uv_pipe_chmod(pipe, flags);
    fklLockThread(exe);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, pipe_obj);
    return 0;
}

#include <fakeLisp/common.h>

static int fuv_pipe_peername(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *pipe_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(pipe_obj, isFuvPipe, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, pipe_obj, exe, ctx->pd);
    size_t len = 2 * FKL_PATH_MAX;
    char buf[2 * FKL_PATH_MAX];
    int ret =
        uv_pipe_getpeername((uv_pipe_t *)GET_HANDLE(*handle_ud), buf, &len);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStr2(exe, len, buf));
    return 0;
}

static int fuv_pipe_sockname(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *pipe_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(pipe_obj, isFuvPipe, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, pipe_obj, exe, ctx->pd);
    size_t len = 2 * FKL_PATH_MAX;
    char buf[2 * FKL_PATH_MAX];
    int ret =
        uv_pipe_getsockname((uv_pipe_t *)GET_HANDLE(*handle_ud), buf, &len);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStr2(exe, len, buf));
    return 0;
}

static int fuv_pipe_pending_count(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *pipe_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(pipe_obj, isFuvPipe, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, pipe_obj, exe, ctx->pd);
    FuvHandle *handle = *handle_ud;
    uv_pipe_t *pipe = (uv_pipe_t *)GET_HANDLE(handle);
    int ret = uv_pipe_pending_count(pipe);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(ret));
    return 0;
}

static int fuv_pipe_pending_type(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *pipe_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(pipe_obj, isFuvPipe, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, pipe_obj, exe, ctx->pd);
    FuvHandle *handle = *handle_ud;
    uv_pipe_t *pipe = (uv_pipe_t *)GET_HANDLE(handle);
    uv_handle_type type_id = uv_pipe_pending_type(pipe);
    const char *name = uv_handle_type_name(type_id);
    FKL_CPROC_RETURN(
        exe, ctx,
        name == NULL
            ? FKL_VM_NIL
            : fklCreateVMvaluePair(exe, fklCreateVMvalueStrFromCstr(exe, name),
                                   FKL_MAKE_VM_FIX(type_id)));
    return 0;
}

static int fuv_pipe_pending_instances(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *pipe_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *count_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(pipe_obj, isFuvPipe, exe);
    FKL_CHECK_TYPE(count_obj, FKL_IS_FIX, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, pipe_obj, exe, ctx->pd);
    FuvHandle *handle = *handle_ud;
    uv_pipe_t *pipe = (uv_pipe_t *)GET_HANDLE(handle);
    uv_pipe_pending_instances(pipe, FKL_GET_FIX(count_obj));
    FKL_CPROC_RETURN(exe, ctx, pipe_obj);
    return 0;
}

static int fuv_tcp_p(FKL_CPROC_ARGL) { PREDICATE(isFuvTcp(val)) }

static inline FklBuiltinErrorType get_tcp_flags(FklVMvalue **parg,
                                                FklVMvalue **const arg_end,
                                                FuvPublicData *fpd,
                                                unsigned int *flags) {
    for (; parg < arg_end; ++parg) {
        FklVMvalue *cur = *parg;
        if (FKL_IS_SYM(cur)) {
            int af = sid_to_af_name(FKL_GET_SYM(cur), fpd);
            if (af < 0)
                return FKL_ERR_INVALID_VALUE;
            (*flags) &= ~0xFF;
            (*flags) |= af;
        } else
            return FKL_ERR_INCORRECT_TYPE_VALUE;
    }
    return 0;
}

static int fuv_make_tcp(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *loop_obj = arg_base[0];
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    unsigned int flags = AF_UNSPEC;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    FklBuiltinErrorType err_type =
        get_tcp_flags(&arg_base[1], arg_base + argc, fpd, &flags);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);
    FklVMvalue *retval = NULL;
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    uv_tcp_t *tcp = createFuvTcp(exe, &retval, ctx->proc, loop_obj);
    int r = uv_tcp_init_ex(&fuv_loop->loop, tcp, flags);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, retval, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, retval);
    return 0;
}

static int fuv_tcp_open(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *tcp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *sock_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(tcp_obj, isFuvTcp, exe);
    FKL_CHECK_TYPE(sock_obj, fklIsVMint, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, tcp_obj, exe, ctx->pd);
    uv_os_sock_t sock = fklVMgetInt(sock_obj);
    int r = uv_tcp_open((uv_tcp_t *)GET_HANDLE(*handle_ud), sock);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, tcp_obj);
    return 0;
}

static int fuv_tcp_nodelay(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *tcp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *enable_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(tcp_obj, isFuvTcp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, tcp_obj, exe, ctx->pd);
    int enable = enable_obj != FKL_VM_NIL;
    int r = uv_tcp_nodelay((uv_tcp_t *)GET_HANDLE(*handle_ud), enable);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, tcp_obj);
    return 0;
}

static int fuv_tcp_keepalive(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *tcp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *enable_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *delay_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(tcp_obj, isFuvTcp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, tcp_obj, exe, ctx->pd);
    int enable = enable_obj != FKL_VM_NIL;
    int delay = 0;
    if (enable) {
        if (delay_obj == NULL)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);
        if (!FKL_IS_FIX(delay_obj))
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
        delay = FKL_GET_FIX(delay_obj);
    } else if (delay_obj)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, exe);
    int r = uv_tcp_keepalive((uv_tcp_t *)GET_HANDLE(*handle_ud), enable, delay);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, tcp_obj);
    return 0;
}

static int fuv_tcp_simultaneous_accepts(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *tcp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *enable_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(tcp_obj, isFuvTcp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, tcp_obj, exe, ctx->pd);
    int enable = enable_obj != FKL_VM_NIL;
    int r =
        uv_tcp_simultaneous_accepts((uv_tcp_t *)GET_HANDLE(*handle_ud), enable);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, tcp_obj);
    return 0;
}

static inline FklVMvalue *
parse_sockaddr_with_fpd(FklVM *exe, struct sockaddr_storage *address,
                        FuvPublicData *fpd) {
    char ip[INET6_ADDRSTRLEN] = {0};
    int port = 0;
    if (address->ss_family == AF_INET) {
        struct sockaddr_in *addrin = (struct sockaddr_in *)address;
        uv_inet_ntop(AF_INET, &(addrin->sin_addr), ip, INET6_ADDRSTRLEN);
        port = ntohs(addrin->sin_port);
    } else if (address->ss_family == AF_INET6) {
        struct sockaddr_in6 *addrin6 = (struct sockaddr_in6 *)address;
        uv_inet_ntop(AF_INET6, &(addrin6->sin6_addr), ip, INET6_ADDRSTRLEN);
        port = ntohs(addrin6->sin6_port);
    }

    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->f_family_sid),
                      af_num_to_symbol(address->ss_family, fpd));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->f_ip_sid),
                      fklCreateVMvalueStrFromCstr(exe, ip));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->f_port_sid),
                      FKL_MAKE_VM_FIX(port));

    return hash;
}

static inline FklVMvalue *
parse_sockaddr(FklVM *exe, struct sockaddr_storage *address, FklVMvalue *pd) {
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    return parse_sockaddr_with_fpd(exe, address, fpd);
}

static int fuv_tcp_sockname(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *tcp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(tcp_obj, isFuvTcp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, tcp_obj, exe, ctx->pd);
    struct sockaddr_storage addr;
    int addrlen = sizeof(addr);
    int ret = uv_tcp_getsockname((uv_tcp_t *)GET_HANDLE(*handle_ud),
                                 (struct sockaddr *)&addr, &addrlen);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, parse_sockaddr(exe, &addr, ctx->pd));
    return 0;
}

static int fuv_tcp_peername(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *tcp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(tcp_obj, isFuvTcp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, tcp_obj, exe, ctx->pd);
    struct sockaddr_storage addr;
    int addrlen = sizeof(addr);
    int ret = uv_tcp_getpeername((uv_tcp_t *)GET_HANDLE(*handle_ud),
                                 (struct sockaddr *)&addr, &addrlen);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, parse_sockaddr(exe, &addr, ctx->pd));
    return 0;
}

static int fuv_tcp_bind(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 4);
    FklVMvalue *tcp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *host_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *port_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *ipv6only_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    FKL_CHECK_TYPE(tcp_obj, isFuvTcp, exe);
    FKL_CHECK_TYPE(host_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(port_obj, FKL_IS_FIX, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, tcp_obj, exe, ctx->pd);
    int port = FKL_GET_FIX(port_obj);
    const char *host = FKL_VM_STR(host_obj)->str;
    struct sockaddr_storage addr;
    if (uv_ip4_addr(host, port, (struct sockaddr_in *)&addr)
        && uv_ip6_addr(host, port, (struct sockaddr_in6 *)&addr))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    unsigned int flags =
        ipv6only_obj && ipv6only_obj != FKL_VM_NIL ? UV_TCP_IPV6ONLY : 0;
    int ret = uv_tcp_bind((uv_tcp_t *)GET_HANDLE(*handle_ud),
                          (struct sockaddr *)&addr, flags);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, tcp_obj);
    return 0;
}

static int fuv_tcp_connect(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 4);
    FklVMvalue *tcp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *host_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *port_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *cb_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    FKL_CHECK_TYPE(tcp_obj, isFuvTcp, exe);
    FKL_CHECK_TYPE(host_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(port_obj, FKL_IS_FIX, exe);
    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    int port = FKL_GET_FIX(port_obj);
    const char *host = FKL_VM_STR(host_obj)->str;
    struct sockaddr_storage addr;
    if (uv_ip4_addr(host, port, (struct sockaddr_in *)&addr)
        && uv_ip6_addr(host, port, (struct sockaddr_in6 *)&addr))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, tcp_obj, exe, ctx->pd);
    FuvHandle *handle = *handle_ud;
    FklVMvalue *connect_obj = NULL;
    uv_connect_t *connect = createFuvConnect(exe, &connect_obj, ctx->proc,
                                             handle->data.loop, cb_obj);
    int ret = uv_tcp_connect(connect, (uv_tcp_t *)GET_HANDLE(*handle_ud),
                             (struct sockaddr *)&addr, fuv_connect_cb);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, connect_obj);
    return 0;
}

static int fuv_tcp_close_reset(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *tcp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *proc_obj = argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(tcp_obj, isFuvTcp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, tcp_obj, exe, ctx->pd);
    FuvHandle *handle = *handle_ud;
    uv_tcp_t *tcp = (uv_tcp_t *)GET_HANDLE(handle);
    if (proc_obj) {
        FKL_CHECK_TYPE(proc_obj, fklIsCallable, exe);
        handle->data.callbacks[1] = proc_obj;
    }
    int ret = uv_tcp_close_reset(tcp, fuv_close_cb);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, tcp_obj);
    return 0;
}

static int fuv_socketpair(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 4);
    FklVMvalue *socktype_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *protocol_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *non_block_flags0_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *non_block_flags1_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    int socktype = SOCK_STREAM;
    int protocol = 0;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    if (FKL_IS_SYM(socktype_obj)) {
        int type = sid_to_socktype(FKL_GET_SYM(socktype_obj), fpd);
        if (type < 0)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
        socktype = type;
    } else if (socktype_obj != FKL_VM_NIL)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    if (FKL_IS_STR(protocol_obj)) {
        const char *name = FKL_VM_STR(protocol_obj)->str;
        int proto = get_protonum_with_cstr(name);
        if (proto < 0)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
        protocol = proto;
    } else if (FKL_IS_SYM(protocol_obj)) {
        const char *name =
            fklVMgetSymbolWithId(exe->gc, FKL_GET_SYM(protocol_obj))->k->str;
        int proto = get_protonum_with_cstr(name);
        if (proto < 0)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
        protocol = proto;
    } else if (protocol_obj != FKL_VM_NIL)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    int flags0 = non_block_flags0_obj != FKL_VM_NIL ? UV_NONBLOCK_PIPE : 0;
    int flags1 = non_block_flags1_obj != FKL_VM_NIL ? UV_NONBLOCK_PIPE : 0;

    uv_os_sock_t socks[2];
    int ret = uv_socketpair(socktype, protocol, socks, flags0, flags1);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx,
                     fklCreateVMvaluePair(exe, fklMakeVMint(socks[0], exe),
                                          fklMakeVMint(socks[1], exe)));
    return 0;
}

static int fuv_tty_p(FKL_CPROC_ARGL) { PREDICATE(isFuvTTY(val)) }

static int fuv_make_tty(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FklVMvalue *fp_obj = NULL;
    uv_file fd = 0;
    if (fklIsVMvalueFp(fd_obj)) {
        fp_obj = fd_obj;
        fd = fklVMfpFileno(FKL_VM_FP(fd_obj));
    } else if (FKL_IS_FIX(fd_obj))
        fd = FKL_GET_FIX(fd_obj);
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    FklVMvalue *tty_obj = NULL;
    uv_tty_t *tty = createFuvTTY(exe, &tty_obj, ctx->proc, loop_obj, fp_obj);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    int r = uv_tty_init(&fuv_loop->loop, tty, fd, 0);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, tty_obj, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, tty_obj);
    return 0;
}

static int fuv_tty_mode_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *tty_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *mode_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(tty_obj, isFuvTTY, exe);
    FKL_CHECK_TYPE(mode_obj, FKL_IS_SYM, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, tty_obj, exe, ctx->pd);
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    uv_tty_mode_t mode = 0;
    FklSid_t mode_id = FKL_GET_SYM(mode_obj);
    if (mode_id == fpd->UV_TTY_MODE_NORMAL_sid)
        mode = UV_TTY_MODE_NORMAL;
    else if (mode_id == fpd->UV_TTY_MODE_RAW_sid)
        mode = UV_TTY_MODE_RAW;
    else if (mode_id == fpd->UV_TTY_MODE_IO_sid)
        mode = UV_TTY_MODE_IO;
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);

    int r = uv_tty_set_mode((uv_tty_t *)GET_HANDLE(*handle_ud), mode);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, tty_obj);
    return 0;
}

static int fuv_tty_winsize(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *tty_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(tty_obj, isFuvTTY, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, tty_obj, exe, ctx->pd);
    int width = 0;
    int height = 0;
    int r =
        uv_tty_get_winsize((uv_tty_t *)GET_HANDLE(*handle_ud), &width, &height);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx,
                     fklCreateVMvaluePair(exe, FKL_MAKE_VM_FIX(width),
                                          FKL_MAKE_VM_FIX(height)));
    return 0;
}

static int fuv_tty_vterm_state(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uv_tty_vtermstate_t state;
    int r = uv_tty_get_vterm_state(&state);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    FklSid_t id = 0;
    switch (state) {
    case UV_TTY_SUPPORTED:
        id = fpd->UV_TTY_SUPPORTED_sid;
        break;
    case UV_TTY_UNSUPPORTED:
        id = fpd->UV_TTY_UNSUPPORTED_sid;
        break;
    }
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_SYM(id));
    return 0;
}

static int fuv_tty_vterm_state_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *state_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(state_obj, FKL_IS_SYM, exe);

    FklSid_t state_id = FKL_GET_SYM(state_obj);
    uv_tty_vtermstate_t state = 0;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    if (state_id == fpd->UV_TTY_SUPPORTED_sid)
        state = UV_TTY_SUPPORTED;
    else if (state_id == fpd->UV_TTY_UNSUPPORTED_sid)
        state = UV_TTY_UNSUPPORTED;
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);

    uv_tty_set_vterm_state(state);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int fuv_tty_mode_reset1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    int r = uv_tty_reset_mode();
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int fuv_udp_p(FKL_CPROC_ARGL) { PREDICATE(isFuvUdp(val)) }

static int fuv_make_udp(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue **const arg_end = arg_base + argc;
    FklVMvalue *loop_obj = arg_base[0];
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    unsigned int flags = AF_UNSPEC;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    int64_t mmsg_num_msgs = 0;
    for (FklVMvalue **parg = &arg_base[1]; parg < arg_end; ++parg) {
        FklVMvalue *cur = *parg;
        if (FKL_IS_SYM(cur)) {
            FklSid_t id = FKL_GET_SYM(cur);
            int family = sid_to_af_name(id, fpd);
            if (family < 0) {
                if (id == fpd->UV_UDP_RECVMMSG_sid) {
                    flags |= UV_UDP_RECVMMSG;
                    cur = ((parg < (arg_end - 1)) ? *(++parg) : NULL);
                    if (cur == NULL)
                        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG, exe);
                    if (!FKL_IS_FIX(cur))
                        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,
                                                exe);
                    mmsg_num_msgs = FKL_GET_FIX(cur);
                    if (mmsg_num_msgs < 0)
                        FKL_RAISE_BUILTIN_ERROR(
                            FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
                } else
                    FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
            } else {
                flags &= ~0xFF;
                flags |= family;
            }
        } else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    }
    FklVMvalue *udp_obj = NULL;
    uv_udp_t *udp =
        createFuvUdp(exe, &udp_obj, ctx->proc, loop_obj, mmsg_num_msgs);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    int r = uv_udp_init_ex(&fuv_loop->loop, udp, flags);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, udp_obj, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static int fuv_udp_recv_stop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    uv_udp_t *udp = (uv_udp_t *)GET_HANDLE(*udp_ud);
    int r = uv_udp_recv_stop(udp);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static int fuv_udp_open(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *sock_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    FKL_CHECK_TYPE(sock_obj, fklIsVMint, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    uv_udp_t *udp = (uv_udp_t *)GET_HANDLE(*udp_ud);
    uv_os_sock_t sock = fklVMgetInt(sock_obj);
    int r = uv_udp_open(udp, sock);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static int fuv_udp_multicast_loop_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *on_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    uv_udp_t *udp = (uv_udp_t *)GET_HANDLE(*udp_ud);
    int r = uv_udp_set_multicast_loop(udp, on_obj == FKL_VM_NIL ? 0 : 1);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static int fuv_udp_multicast_ttl_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *ttl_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    FKL_CHECK_TYPE(ttl_obj, FKL_IS_FIX, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    uv_udp_t *udp = (uv_udp_t *)GET_HANDLE(*udp_ud);
    int r = uv_udp_set_multicast_ttl(udp, FKL_GET_FIX(ttl_obj));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static int fuv_udp_multicast_interface_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 1, 2);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *interface_addr_obj =
        argc > 1 ? FKL_CPROC_GET_ARG(exe, ctx, 1) : NULL;
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);

    uv_udp_t *udp = (uv_udp_t *)GET_HANDLE(*udp_ud);
    const char *interface_addr = NULL;
    if (interface_addr_obj) {
        if (FKL_IS_STR(interface_addr_obj))
            interface_addr = FKL_VM_STR(interface_addr_obj)->str;
        else if (interface_addr_obj != FKL_VM_NIL)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    }
    int r = uv_udp_set_multicast_interface(udp, interface_addr);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static int fuv_udp_broadcast_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *on_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    uv_udp_t *udp = (uv_udp_t *)GET_HANDLE(*udp_ud);
    int r = uv_udp_set_broadcast(udp, on_obj == FKL_VM_NIL ? 0 : 1);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static int fuv_udp_ttl_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *ttl_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    FKL_CHECK_TYPE(ttl_obj, FKL_IS_FIX, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    uv_udp_t *udp = (uv_udp_t *)GET_HANDLE(*udp_ud);
    int r = uv_udp_set_ttl(udp, FKL_GET_FIX(ttl_obj));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static inline struct sockaddr *
setup_udp_addr(FklVM *exe, struct sockaddr_storage *addr, FklVMvalue *host_obj,
               FklVMvalue *port_obj, FklBuiltinErrorType *err_type) {
    const char *host = NULL;
    int port = 0;
    if (host_obj == FKL_VM_NIL && port_obj == FKL_VM_NIL)
        return NULL;
    if (!FKL_IS_STR(host_obj)) {
        *err_type = FKL_ERR_INCORRECT_TYPE_VALUE;
        return NULL;
    }
    if (!FKL_IS_FIX(port_obj)) {
        *err_type = FKL_ERR_INCORRECT_TYPE_VALUE;
        return NULL;
    }
    host = FKL_VM_STR(host_obj)->str;
    port = FKL_GET_FIX(port_obj);
    if (uv_ip4_addr(host, port, (struct sockaddr_in *)addr)
        && uv_ip6_addr(host, port, (struct sockaddr_in6 *)addr)) {
        *err_type = FKL_ERR_INVALID_VALUE;
        return NULL;
    }
    return (struct sockaddr *)addr;
}

static int fuv_udp_connect(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 3);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *host_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *port_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    struct sockaddr_storage addr;
    FklBuiltinErrorType err_type = 0;
    struct sockaddr *addr_ptr =
        setup_udp_addr(exe, &addr, host_obj, port_obj, &err_type);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);
    int r = uv_udp_connect((uv_udp_t *)GET_HANDLE(*udp_ud), addr_ptr);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static inline int sid_to_udp_flag(FklSid_t id, FuvPublicData *fpd) {
    if (id == fpd->UV_UDP_IPV6ONLY_sid)
        return UV_UDP_IPV6ONLY;
    if (id == fpd->UV_UDP_PARTIAL_sid)
        return UV_UDP_PARTIAL;
    if (id == fpd->UV_UDP_REUSEADDR_sid)
        return UV_UDP_REUSEADDR;
    if (id == fpd->UV_UDP_MMSG_CHUNK_sid)
        return UV_UDP_MMSG_CHUNK;
    if (id == fpd->UV_UDP_MMSG_FREE_sid)
        return UV_UDP_MMSG_FREE;
    if (id == fpd->UV_UDP_LINUX_RECVERR_sid)
        return UV_UDP_LINUX_RECVERR;
    if (id == fpd->UV_UDP_RECVMMSG_sid)
        return UV_UDP_RECVMMSG;
    return -1;
}

static inline FklBuiltinErrorType get_udp_flags(FklVMvalue **parg,
                                                FklVMvalue **const arg_end,
                                                unsigned int *flags,
                                                FuvPublicData *fpd) {
    for (; parg < arg_end; ++parg) {
        FklVMvalue *cur = *parg;
        if (FKL_IS_SYM(cur)) {
            FklSid_t id = FKL_GET_SYM(cur);
            int flag = sid_to_udp_flag(id, fpd);
            if (flag < 0)
                return FKL_ERR_INVALID_VALUE;
            *flags |= flag;
        } else
            return FKL_ERR_INCORRECT_TYPE_VALUE;
    }
    return 0;
}

static int fuv_udp_bind(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *udp_obj = arg_base[0];
    FklVMvalue *host_obj = arg_base[1];
    FklVMvalue *port_obj = arg_base[2];
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    FKL_CHECK_TYPE(host_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(port_obj, FKL_IS_FIX, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    struct sockaddr_storage addr;
    const char *host = FKL_VM_STR(host_obj)->str;
    int port = FKL_GET_FIX(port_obj);
    if (uv_ip4_addr(host, port, (struct sockaddr_in *)&addr)
        && uv_ip6_addr(host, port, (struct sockaddr_in6 *)&addr))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    unsigned int flags = 0;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    FklBuiltinErrorType err_type =
        argc > 3 ? get_udp_flags(&arg_base[3], arg_base + argc, &flags, fpd)
                 : 0;
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);
    int r = uv_udp_bind((uv_udp_t *)GET_HANDLE(*udp_ud),
                        (struct sockaddr *)&addr, flags);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static int fuv_udp_using_recvmmsg_p(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    uv_udp_t *udp = (uv_udp_t *)GET_HANDLE(*udp_ud);
    int r = uv_udp_using_recvmmsg(udp);
    FKL_CPROC_RETURN(exe, ctx, r ? FKL_VM_TRUE : FKL_VM_NIL);
    return 0;
}

static int fuv_udp_send_queue_size(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    uv_udp_t *udp = (uv_udp_t *)GET_HANDLE(*udp_ud);
    size_t size = uv_udp_get_send_queue_size(udp);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(size, exe));
    return 0;
}

static int fuv_udp_send_queue_count(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    uv_udp_t *udp = (uv_udp_t *)GET_HANDLE(*udp_ud);
    size_t size = uv_udp_get_send_queue_count(udp);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(size, exe));
    return 0;
}

static int fuv_udp_peername(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    struct sockaddr_storage addr;
    int addrlen = sizeof(addr);
    int ret = uv_udp_getpeername((uv_udp_t *)GET_HANDLE(*udp_ud),
                                 (struct sockaddr *)&addr, &addrlen);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, parse_sockaddr(exe, &addr, ctx->pd));
    return 0;
}

static int fuv_udp_sockname(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    struct sockaddr_storage addr;
    int addrlen = sizeof(addr);
    int ret = uv_udp_getsockname((uv_udp_t *)GET_HANDLE(*udp_ud),
                                 (struct sockaddr *)&addr, &addrlen);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, parse_sockaddr(exe, &addr, ctx->pd));
    return 0;
}

static inline int sid_to_membership(FklSid_t id, FuvPublicData *fpd,
                                    uv_membership *membership) {
    if (id == fpd->UV_LEAVE_GROUP_sid) {
        *membership = UV_LEAVE_GROUP;
        return 0;
    }
    if (id == fpd->UV_JOIN_GROUP_sid) {
        *membership = UV_JOIN_GROUP;
        return 0;
    }
    return 1;
}

static int fuv_udp_membership_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 4);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *multicast_addr_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *interface_addr_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *membership_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    FKL_CHECK_TYPE(multicast_addr_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(membership_obj, FKL_IS_SYM, exe);
    if (interface_addr_obj != FKL_VM_NIL && !FKL_IS_STR(interface_addr_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    const char *multicast_addr = FKL_VM_STR(multicast_addr_obj)->str;
    const char *interface_addr = interface_addr_obj == FKL_VM_NIL
                                   ? NULL
                                   : FKL_VM_STR(interface_addr_obj)->str;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    FklSid_t id = FKL_GET_SYM(membership_obj);
    uv_membership membership = 0;
    if (sid_to_membership(id, fpd, &membership))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    int ret = uv_udp_set_membership((uv_udp_t *)GET_HANDLE(*udp_ud),
                                    multicast_addr, interface_addr, membership);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static int fuv_udp_source_membership_set1(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 5);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *multicast_addr_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *interface_addr_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *source_addr_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FklVMvalue *membership_obj = FKL_CPROC_GET_ARG(exe, ctx, 4);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    FKL_CHECK_TYPE(multicast_addr_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(source_addr_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(membership_obj, FKL_IS_SYM, exe);
    if (interface_addr_obj != FKL_VM_NIL && !FKL_IS_STR(interface_addr_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    const char *multicast_addr = FKL_VM_STR(multicast_addr_obj)->str;
    const char *interface_addr = interface_addr_obj == FKL_VM_NIL
                                   ? NULL
                                   : FKL_VM_STR(interface_addr_obj)->str;
    const char *source_addr = FKL_VM_STR(source_addr_obj)->str;

    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    FklSid_t id = FKL_GET_SYM(membership_obj);
    uv_membership membership = 0;
    if (sid_to_membership(id, fpd, &membership))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    int ret = uv_udp_set_source_membership((uv_udp_t *)GET_HANDLE(*udp_ud),
                                           multicast_addr, interface_addr,
                                           source_addr, membership);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static inline FklBuiltinErrorType
setup_udp_send_data(FklVMvalue **parg, FklVMvalue **const arg_end, uint32_t num,
                    FklVMvalue *write_obj, uv_buf_t **pbufs) {
    FKL_DECL_VM_UD_DATA(fuv_req, FuvReqUd, write_obj);
    struct FuvUdpSend *req = (struct FuvUdpSend *)*fuv_req;
    FklVMvalue **cur = req->send_objs;
    uv_buf_t *bufs = (uv_buf_t *)malloc(num * sizeof(uv_buf_t));
    FKL_ASSERT(bufs);
    for (uint32_t i = 0; parg < arg_end; ++i, ++parg, ++cur) {
        FklVMvalue *val = *parg;
        if (FKL_IS_STR(val)) {
            *cur = val;
            FklString *str = FKL_VM_STR(val);
            bufs[i].base = str->str;
            bufs[i].len = str->size;
        } else if (FKL_IS_BYTEVECTOR(val)) {
            *cur = val;
            FklBytevector *bvec = FKL_VM_BVEC(val);
            bufs[i].base = (char *)bvec->ptr;
            bufs[i].len = bvec->size;
        } else {
            free(bufs);
            return FKL_ERR_INCORRECT_TYPE_VALUE;
        }
    }
    *pbufs = bufs;
    return 0;
}

static void fuv_udp_send_cb(uv_udp_send_t *req, int status) {
    fuv_call_req_callback_in_loop_with_value_creator(
        (uv_req_t *)req, fuv_req_cb_error_value_creator, &status);
}

static int fuv_udp_send(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 5, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *udp_obj = arg_base[0];
    FklVMvalue *host_obj = arg_base[1];
    FklVMvalue *port_obj = arg_base[2];
    FklVMvalue *cb_obj = arg_base[3];
    FklVMvalue **rest_arg = &arg_base[4];
    uint32_t const buf_count = argc - 4;

    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    if (cb_obj == FKL_VM_NIL)
        cb_obj = NULL;
    else if (!fklIsCallable(cb_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);

    struct sockaddr_storage addr;
    FklBuiltinErrorType err_type = 0;
    struct sockaddr *addr_ptr =
        setup_udp_addr(exe, &addr, host_obj, port_obj, &err_type);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    FuvHandle *handle = *udp_ud;
    uv_buf_t *bufs = NULL;
    FklVMvalue *udp_send_obj = NULL;
    uv_udp_send_t *udp_send = NULL;
    udp_send = createFuvUdpSend(exe, &udp_send_obj, ctx->proc,
                                handle->data.loop, cb_obj, buf_count);
    err_type = setup_udp_send_data(rest_arg, arg_base + argc, buf_count,
                                   udp_send_obj, &bufs);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    int ret = uv_udp_send(udp_send, (uv_udp_t *)GET_HANDLE(handle), bufs,
                          buf_count, addr_ptr, fuv_udp_send_cb);
    free(bufs);
    CHECK_UV_RESULT_AND_CLEANUP_REQ(ret, udp_send_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_send_obj);
    return 0;
}

#define MAX_DGRAM_SIZE (64 * 1024)

static void fuv_udp_alloc_cb(uv_handle_t *handle, size_t suggested_size,
                             uv_buf_t *buf) {
    size_t buffer_size = suggested_size;
    struct FuvUdp *udp_handle = (struct FuvUdp *)uv_handle_get_data(handle);
    if (uv_udp_using_recvmmsg((uv_udp_t *)handle)) {
        int64_t num_msgs = udp_handle->mmsg_num_msgs;
        buffer_size = MAX_DGRAM_SIZE * num_msgs;
    }
    char *base;
    if (!buffer_size)
        base = NULL;
    else {
        base = (char *)malloc(buffer_size);
        FKL_ASSERT(base);
    }
    buf->base = base;
    buf->len = buffer_size;
}

struct UdpRecvArg {
    ssize_t nread;
    const struct sockaddr *addr;
    const uv_buf_t *buf;
    unsigned flags;
    FuvPublicData *fpd;
};

static int fuv_udp_recv_cb_value_creator(FklVM *exe, void *a) {
    struct UdpRecvArg *arg = (struct UdpRecvArg *)a;
    ssize_t nread = arg->nread;
    FklVMvalue *err =
        nread < 0 ? createUvErrorWithFpd(nread, exe, arg->fpd) : FKL_VM_NIL;
    FklVMvalue *res = FKL_VM_NIL;
    if (nread == 0) {
        if (arg->addr)
            res = fklCreateVMvalueStrFromCstr(exe, "");
    } else if (nread > 0)
        res = fklCreateVMvalueStr2(exe, nread, arg->buf->base);

    if (arg->buf && !(arg->flags & UV_UDP_MMSG_CHUNK))
        free(arg->buf->base);

    FuvPublicData *fpd = arg->fpd;
    FklVMvalue *addr = arg->addr
                         ? parse_sockaddr_with_fpd(
                               exe, (struct sockaddr_storage *)arg->addr, fpd)
                         : FKL_VM_NIL;

    FklVMvalue *flags = FKL_VM_NIL;

    FklVMvalue **pcur = &flags;
    if (arg->flags & UV_UDP_IPV6ONLY) {
        *pcur = fklCreateVMvaluePairWithCar(
            exe, FKL_MAKE_VM_SYM(fpd->UV_UDP_IPV6ONLY_sid));
        pcur = &FKL_VM_CDR(*pcur);
    }
    if (arg->flags & UV_UDP_PARTIAL) {
        *pcur = fklCreateVMvaluePairWithCar(
            exe, FKL_MAKE_VM_SYM(fpd->UV_UDP_PARTIAL_sid));
        pcur = &FKL_VM_CDR(*pcur);
    }
    if (arg->flags & UV_UDP_REUSEADDR) {
        *pcur = fklCreateVMvaluePairWithCar(
            exe, FKL_MAKE_VM_SYM(fpd->UV_UDP_REUSEADDR_sid));
        pcur = &FKL_VM_CDR(*pcur);
    }
    if (arg->flags & UV_UDP_MMSG_CHUNK) {
        *pcur = fklCreateVMvaluePairWithCar(
            exe, FKL_MAKE_VM_SYM(fpd->UV_UDP_MMSG_CHUNK_sid));
        pcur = &FKL_VM_CDR(*pcur);
    }
    if (arg->flags & UV_UDP_MMSG_FREE) {
        *pcur = fklCreateVMvaluePairWithCar(
            exe, FKL_MAKE_VM_SYM(fpd->UV_UDP_MMSG_FREE_sid));
        pcur = &FKL_VM_CDR(*pcur);
    }
    if (arg->flags & UV_UDP_LINUX_RECVERR) {
        *pcur = fklCreateVMvaluePairWithCar(
            exe, FKL_MAKE_VM_SYM(fpd->UV_UDP_LINUX_RECVERR_sid));
        pcur = &FKL_VM_CDR(*pcur);
    }
    if (arg->flags & UV_UDP_RECVMMSG) {
        *pcur = fklCreateVMvaluePairWithCar(
            exe, FKL_MAKE_VM_SYM(fpd->UV_UDP_RECVMMSG_sid));
        pcur = &FKL_VM_CDR(*pcur);
    }
    FKL_VM_PUSH_VALUE(exe, err);
    FKL_VM_PUSH_VALUE(exe, res);
    FKL_VM_PUSH_VALUE(exe, addr);
    FKL_VM_PUSH_VALUE(exe, flags);
    return 0;
}

static void fuv_udp_recv_cb(uv_udp_t *handle, ssize_t nread,
                            const uv_buf_t *buf, const struct sockaddr *addr,
                            unsigned flags) {
    if (flags & UV_UDP_MMSG_FREE) {
        free(buf->base);
        return;
    }
    FuvLoopData *ldata =
        uv_loop_get_data(uv_handle_get_loop((uv_handle_t *)handle));
    FuvHandleData *hdata =
        &((FuvHandle *)uv_handle_get_data((uv_handle_t *)handle))->data;
    FklVMvalue *fpd_obj =
        ((FklCprocFrameContext *)ldata->exe->top_frame->data)->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, fpd_obj);
    struct UdpRecvArg arg = {
        .nread = nread,
        .addr = addr,
        .buf = buf,
        .flags = flags,
        .fpd = fpd,
    };
    fuv_call_handle_callback_in_loop_with_value_creator(
        (uv_handle_t *)handle, hdata, ldata, 0, fuv_udp_recv_cb_value_creator,
        &arg);
}

static int fuv_udp_recv_start(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *udp_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *cb_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);
    FuvHandle *handle = *udp_ud;
    uv_udp_t *udp = (uv_udp_t *)GET_HANDLE(handle);
    handle->data.callbacks[0] = cb_obj;
    int ret = uv_udp_recv_start(udp, fuv_udp_alloc_cb, fuv_udp_recv_cb);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, udp_obj);
    return 0;
}

static int fuv_udp_try_send(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 4, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *udp_obj = arg_base[0];
    FklVMvalue *host_obj = arg_base[1];
    FklVMvalue *port_obj = arg_base[2];
    FklVMvalue **rest_arg = &arg_base[3];
    uint32_t const buf_count = argc - 3;

    FKL_CHECK_TYPE(udp_obj, isFuvUdp, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud, udp_obj, exe, ctx->pd);

    struct sockaddr_storage addr;
    FklBuiltinErrorType err_type = 0;
    struct sockaddr *addr_ptr =
        setup_udp_addr(exe, &addr, host_obj, port_obj, &err_type);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    uv_buf_t *bufs = NULL;
    err_type =
        setup_try_write_data(rest_arg, arg_base + argc, buf_count, &bufs);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    int ret = uv_udp_try_send((uv_udp_t *)GET_HANDLE(*udp_ud), bufs, buf_count,
                              addr_ptr);
    free(bufs);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(ret));
    return 0;
}

static int fuv_fs_event_p(FKL_CPROC_ARGL) { PREDICATE(isFuvFsEvent(val)) }

static int fuv_make_fs_event(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    int r;
    FklVMvalue *fs_event = createFuvFsEvent(exe, ctx->proc, loop_obj, &r);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, fs_event, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fs_event);
    return 0;
}

static inline FklBuiltinErrorType get_fs_event_flags(FklVMvalue **parg,
                                                     FklVMvalue **const arg_end,
                                                     unsigned int *flags,
                                                     FklVMvalue *pd) {
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    for (; parg < arg_end; ++parg) {
        FklVMvalue *cur = *parg;
        if (FKL_IS_SYM(cur)) {
            FklSid_t id = FKL_GET_SYM(cur);
            if (id == fpd->UV_FS_EVENT_WATCH_ENTRY_sid)
                (*flags) |= UV_FS_EVENT_WATCH_ENTRY;
            else if (id == fpd->UV_FS_EVENT_STAT_sid)
                (*flags) |= UV_FS_EVENT_STAT;
            else if (id == fpd->UV_FS_EVENT_RECURSIVE_sid)
                (*flags) |= UV_FS_EVENT_RECURSIVE;
            else
                return FKL_ERR_INVALID_VALUE;
        } else
            return FKL_ERR_INCORRECT_TYPE_VALUE;
    }
    return 0;
}

struct FsEventCbValueCreateArg {
    FuvPublicData *fpd;
    const char *path;
    int events;
    int status;
};

static int fuv_fs_event_value_creator(FklVM *exe, void *a) {
    struct FsEventCbValueCreateArg *arg = a;
    FuvPublicData *fpd = arg->fpd;
    FklVMvalue *err = arg->status < 0
                        ? createUvErrorWithFpd(arg->status, exe, fpd)
                        : FKL_VM_NIL;
    FklVMvalue *path =
        arg->path ? fklCreateVMvalueStrFromCstr(exe, arg->path) : FKL_VM_NIL;

    FklVMvalue *events = FKL_VM_NIL;
    FklVMvalue **pevents = &events;
    if (arg->events & UV_RENAME) {
        *pevents = fklCreateVMvaluePairWithCar(
            exe, FKL_MAKE_VM_SYM(fpd->UV_RENAME_sid));
        pevents = &FKL_VM_CDR(*pevents);
    }
    if (arg->events & UV_CHANGE) {
        *pevents = fklCreateVMvaluePairWithCar(
            exe, FKL_MAKE_VM_SYM(fpd->UV_CHANGE_sid));
        pevents = &FKL_VM_CDR(*pevents);
    }

    FKL_VM_PUSH_VALUE(exe, err);
    FKL_VM_PUSH_VALUE(exe, path);
    FKL_VM_PUSH_VALUE(exe, events);
    return 0;
}

static void fuv_fs_event_cb(uv_fs_event_t *handle, const char *path, int events,
                            int status) {
    uv_loop_t *loop = uv_handle_get_loop((uv_handle_t *)handle);
    FuvLoopData *ldata = uv_loop_get_data(loop);
    FuvHandleData *hdata =
        &((FuvHandle *)uv_handle_get_data((uv_handle_t *)handle))->data;
    FklVMvalue *fpd_obj =
        ((FklCprocFrameContext *)ldata->exe->top_frame->data)->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, fpd_obj);

    struct FsEventCbValueCreateArg arg = {
        .fpd = fpd,
        .path = path,
        .events = events,
        .status = status,
    };

    fuv_call_handle_callback_in_loop_with_value_creator(
        (uv_handle_t *)handle, hdata, ldata, 0, fuv_fs_event_value_creator,
        &arg);
}

static int fuv_fs_event_start(FKL_CPROC_ARGL) {
    unsigned int flags = 0;
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fs_event_obj = arg_base[0];
    FklVMvalue *path_obj = arg_base[1];
    FklVMvalue *cb_obj = arg_base[2];
    FklBuiltinErrorType err_type =
        get_fs_event_flags(&arg_base[3], arg_base + argc, &flags, ctx->pd);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fs_event, fs_event_obj, exe, ctx->pd);
    FuvHandle *fuv_handle = *fs_event;
    fuv_handle->data.callbacks[0] = cb_obj;
    int r =
        uv_fs_event_start((uv_fs_event_t *)GET_HANDLE(fuv_handle),
                          fuv_fs_event_cb, FKL_VM_STR(path_obj)->str, flags);

    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fs_event_obj);
    return 0;
}

static int fuv_fs_event_stop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *fs_event_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(fs_event_obj, isFuvFsEvent, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fs_event, fs_event_obj, exe, ctx->pd);
    FuvHandle *handle = *fs_event;
    int r = uv_fs_event_stop((uv_fs_event_t *)GET_HANDLE(handle));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fs_event_obj);
    return 0;
}

static int fuv_fs_event_path(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *fs_event_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(fs_event_obj, isFuvFsEvent, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, fs_event_obj, exe, ctx->pd);
    size_t len = 2 * FKL_PATH_MAX;
    char buf[2 * FKL_PATH_MAX];
    int ret =
        uv_fs_event_getpath((uv_fs_event_t *)GET_HANDLE(*handle_ud), buf, &len);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStr2(exe, len, buf));
    return 0;
}

static int fuv_fs_poll_p(FKL_CPROC_ARGL) { PREDICATE(isFuvFsPoll(val)) }

static int fuv_make_fs_poll(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    int r;
    FklVMvalue *fs_poll = createFuvFsPoll(exe, ctx->proc, loop_obj, &r);
    CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r, fs_poll, loop_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fs_poll);
    return 0;
}

static inline FklVMvalue *
timespec_to_vmtable(FklVM *exe, const uv_timespec_t *spec, FuvPublicData *fpd) {
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->timespec_f_sec_sid),
                      fklMakeVMint(spec->tv_sec, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->timespec_f_nsec_sid),
                      fklMakeVMint(spec->tv_nsec, exe));

    return hash;
}

#ifdef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef S_ISREG
#define S_ISREG(x) (((x) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(x) (((x) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(x) (((x) & _S_IFMT) == _S_IFIFO)
#endif
#ifndef S_ISCHR
#define S_ISCHR(x) (((x) & _S_IFMT) == _S_IFCHR)
#endif
#ifndef S_ISBLK
#define S_ISBLK(x) 0
#endif
#ifndef S_ISLNK
#define S_ISLNK(x) (((x) & S_IFLNK) == S_IFLNK)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(x) 0
#endif
#else
#include <unistd.h>
#endif

static inline FklVMvalue *stat_to_vmtable(FklVM *exe, const uv_stat_t *stat,
                                          FuvPublicData *fpd) {
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_dev_sid),
                      fklMakeVMuint(stat->st_dev, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_mode_sid),
                      fklMakeVMuint(stat->st_mode, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_nlink_sid),
                      fklMakeVMuint(stat->st_nlink, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_uid_sid),
                      fklMakeVMuint(stat->st_uid, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_gid_sid),
                      fklMakeVMuint(stat->st_gid, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_rdev_sid),
                      fklMakeVMuint(stat->st_rdev, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_ino_sid),
                      fklMakeVMuint(stat->st_ino, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_size_sid),
                      fklMakeVMuint(stat->st_size, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_blksize_sid),
                      fklMakeVMuint(stat->st_blksize, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_blocks_sid),
                      fklMakeVMuint(stat->st_blocks, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_flags_sid),
                      fklMakeVMuint(stat->st_flags, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_gen_sid),
                      fklMakeVMuint(stat->st_gen, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_atime_sid),
                      timespec_to_vmtable(exe, &stat->st_atim, fpd));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_mtime_sid),
                      timespec_to_vmtable(exe, &stat->st_mtim, fpd));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_ctime_sid),
                      timespec_to_vmtable(exe, &stat->st_ctim, fpd));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_birthtime_sid),
                      timespec_to_vmtable(exe, &stat->st_birthtim, fpd));

    FklSid_t type = 0;
    if (S_ISREG(stat->st_mode))
        type = fpd->stat_type_file_sid;
    else if (S_ISDIR(stat->st_mode))
        type = fpd->stat_type_directory_sid;
    else if (S_ISFIFO(stat->st_mode))
        type = fpd->stat_type_fifo_sid;
#ifdef S_ISSOCK
    else if (S_ISSOCK(stat->st_mode))
        type = fpd->stat_type_socket_sid;
#endif
    else if (S_ISCHR(stat->st_mode))
        type = fpd->stat_type_char_sid;
    else if (S_ISBLK(stat->st_mode))
        type = fpd->stat_type_block_sid;
    else if (S_ISLNK(stat->st_mode))
        type = fpd->stat_type_link_sid;
    else
        type = fpd->stat_type_unknown_sid;

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->stat_f_type_sid),
                      FKL_MAKE_VM_SYM(type));
    return hash;
}

struct FsPollCbValueCreateArg {
    FuvPublicData *fpd;
    const uv_stat_t *prev;
    const uv_stat_t *curr;
    int status;
};

static int fuv_fs_poll_value_creator(FklVM *exe, void *a) {
    struct FsPollCbValueCreateArg *arg = a;
    FuvPublicData *fpd = arg->fpd;
    FklVMvalue *err = arg->status < 0
                        ? createUvErrorWithFpd(arg->status, exe, fpd)
                        : FKL_VM_NIL;
    FklVMvalue *prev =
        arg->prev ? stat_to_vmtable(exe, arg->prev, fpd) : FKL_VM_NIL;
    FklVMvalue *curr =
        arg->curr ? stat_to_vmtable(exe, arg->curr, fpd) : FKL_VM_NIL;

    FKL_VM_PUSH_VALUE(exe, err);
    FKL_VM_PUSH_VALUE(exe, prev);
    FKL_VM_PUSH_VALUE(exe, curr);
    return 0;
}

static void fuv_fs_poll_cb(uv_fs_poll_t *handle, int status,
                           const uv_stat_t *prev, const uv_stat_t *curr) {
    uv_loop_t *loop = uv_handle_get_loop((uv_handle_t *)handle);
    FuvLoopData *ldata = uv_loop_get_data(loop);
    FuvHandleData *hdata =
        &((FuvHandle *)uv_handle_get_data((uv_handle_t *)handle))->data;
    FklVMvalue *fpd_obj =
        ((FklCprocFrameContext *)ldata->exe->top_frame->data)->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, fpd_obj);

    struct FsPollCbValueCreateArg arg = {
        .fpd = fpd,
        .prev = prev,
        .curr = curr,
        .status = status,
    };

    fuv_call_handle_callback_in_loop_with_value_creator(
        (uv_handle_t *)handle, hdata, ldata, 0, fuv_fs_poll_value_creator,
        &arg);
}

static int fuv_fs_poll_start(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 4);
    FklVMvalue *fs_poll_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *interval_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *cb_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FKL_CHECK_TYPE(fs_poll_obj, isFuvFsPoll, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(interval_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    int64_t interval = FKL_GET_FIX(interval_obj);
    if (interval < 0)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);

    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fs_poll, fs_poll_obj, exe, ctx->pd);
    FuvHandle *fuv_handle = *fs_poll;
    fuv_handle->data.callbacks[0] = cb_obj;
    int r =
        uv_fs_poll_start((uv_fs_poll_t *)GET_HANDLE(fuv_handle), fuv_fs_poll_cb,
                         FKL_VM_STR(path_obj)->str, interval);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fs_poll_obj);
    return 0;
}

static int fuv_fs_poll_stop(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *fs_poll_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(fs_poll_obj, isFuvFsPoll, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fs_poll, fs_poll_obj, exe, ctx->pd);
    FuvHandle *handle = *fs_poll;
    int r = uv_fs_poll_stop((uv_fs_poll_t *)GET_HANDLE(handle));
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fs_poll_obj);
    return 0;
}

static int fuv_fs_poll_path(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *fs_poll_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(fs_poll_obj, isFuvFsPoll, exe);
    DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud, fs_poll_obj, exe, ctx->pd);
    size_t len = 2 * FKL_PATH_MAX;
    char buf[2 * FKL_PATH_MAX];
    int ret =
        uv_fs_poll_getpath((uv_fs_poll_t *)GET_HANDLE(*handle_ud), buf, &len);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStr2(exe, len, buf));
    return 0;
}

static inline FklVMvalue *create_fs_uv_err(FklVM *exe, int r, uv_fs_t *req,
                                           FklVMvalue *dest_path,
                                           FklVMvalue *pd) {
    FklVMvalue *err = NULL;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    if (dest_path) {
        FklStringBuffer buf;
        fklInitStringBuffer(&buf);
        fklStringBufferPrintf(&buf, "%s: %s -> %s", uv_strerror(r), req->path,
                              FKL_VM_STR(dest_path)->str);
        err = fklCreateVMvalueError(exe, uvErrToSid(r, fpd),
                                    fklStringBufferToString(&buf));
        fklUninitStringBuffer(&buf);
    } else if (req->path) {
        FklStringBuffer buf;
        fklInitStringBuffer(&buf);
        fklStringBufferPrintf(&buf, "%s: %s", uv_strerror(r), req->path);
        err = fklCreateVMvalueError(exe, uvErrToSid(r, fpd),
                                    fklStringBufferToString(&buf));
        fklUninitStringBuffer(&buf);
    } else
        err = createUvErrorWithFpd(r, exe, fpd);
    return err;
}

static inline FklVMvalue *statfs_to_vmtable(FklVM *exe, uv_statfs_t *s,
                                            FuvPublicData *fpd) {
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->statfs_f_type_sid),
                      fklMakeVMuint(s->f_type, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->statfs_f_bsize_sid),
                      fklMakeVMuint(s->f_bsize, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->statfs_f_blocks_sid),
                      fklMakeVMuint(s->f_blocks, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->statfs_f_bfree_sid),
                      fklMakeVMuint(s->f_bfree, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->statfs_f_bavail_sid),
                      fklMakeVMuint(s->f_bavail, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->statfs_f_files_sid),
                      fklMakeVMuint(s->f_files, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->statfs_f_ffree_sid),
                      fklMakeVMuint(s->f_ffree, exe));

    return hash;
}

static inline FklSid_t dirent_type_to_sid(uv_dirent_type_t type,
                                          FuvPublicData *fpd) {
    switch (type) {
    case UV_DIRENT_FILE:
        return fpd->UV_DIRENT_FILE_sid;
        break;
    case UV_DIRENT_DIR:
        return fpd->UV_DIRENT_DIR_sid;
        break;
    case UV_DIRENT_LINK:
        return fpd->UV_DIRENT_LINK_sid;
        break;
    case UV_DIRENT_FIFO:
        return fpd->UV_DIRENT_FIFO_sid;
        break;
    case UV_DIRENT_SOCKET:
        return fpd->UV_DIRENT_SOCKET_sid;
        break;
    case UV_DIRENT_CHAR:
        return fpd->UV_DIRENT_CHAR_sid;
        break;
    case UV_DIRENT_BLOCK:
        return fpd->UV_DIRENT_BLOCK_sid;
        break;

    case UV_DIRENT_UNKNOWN:
    default:
        return fpd->UV_DIRENT_UNKNOWN_sid;
        break;
    }
}

static inline FklVMvalue *dirent_to_vmtable(FklVM *exe, uv_dirent_t *d,
                                            FuvPublicData *fpd) {
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->dirent_f_type_sid),
                      FKL_MAKE_VM_SYM(dirent_type_to_sid(d->type, fpd)));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->dirent_f_name_sid),
                      fklCreateVMvalueStrFromCstr(exe, d->name));

    return hash;
}

static inline FklVMvalue *readdir_result_to_list(FklVM *exe, ssize_t result,
                                                 uv_dir_t *dir,
                                                 FuvPublicData *fpd) {
    FklVMvalue *r = FKL_VM_NIL;
    FklVMvalue **pr = &r;
    for (ssize_t i = 0; i < result; i++) {
        *pr = fklCreateVMvaluePairWithCar(
            exe, dirent_to_vmtable(exe, &dir->dirents[i], fpd));
        pr = &FKL_VM_CDR(*pr);
    }
    return r;
}

static inline FklVMvalue *
create_fs_retval_sync(FklVM *exe, struct FuvFsReq *fs_req, FklVMvalue *pd) {
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    uv_fs_t *req = &fs_req->req;
    switch (req->fs_type) {
    case UV_FS_CLOSE:
    case UV_FS_MKDIR:
    case UV_FS_RMDIR:
    case UV_FS_RENAME:
    case UV_FS_LINK:
    case UV_FS_UNLINK:
    case UV_FS_UTIME:
    case UV_FS_FUTIME:
    case UV_FS_LUTIME:
    case UV_FS_FSYNC:
    case UV_FS_FDATASYNC:
    case UV_FS_FTRUNCATE:
    case UV_FS_CHMOD:
    case UV_FS_FCHMOD:
    case UV_FS_ACCESS:
    case UV_FS_SYMLINK:
    case UV_FS_COPYFILE:
    case UV_FS_CHOWN:
    case UV_FS_LCHOWN:
    case UV_FS_FCHOWN:
    case UV_FS_CLOSEDIR:
    case UV_FS_SCANDIR:
    case UV_FS_UNKNOWN:
    case UV_FS_CUSTOM:
        return FKL_VM_NIL;
        break;
    case UV_FS_READ:
        return fklCreateVMvalueStr2(exe, req->result, fs_req->buf.base);
        break;
    case UV_FS_OPEN:
        return FKL_MAKE_VM_FIX(req->result);
        break;
    case UV_FS_WRITE:
    case UV_FS_SENDFILE:
        return fklMakeVMint(req->result, exe);
    case UV_FS_MKDTEMP:
        return fklCreateVMvalueStrFromCstr(exe, req->path);
        break;
    case UV_FS_MKSTEMP:
        return fklCreateVMvaluePair(
            exe, FKL_MAKE_VM_FIX(req->result),
            fklCreateVMvalueStrFromCstr(exe, req->path));
        break;
    case UV_FS_REALPATH:
    case UV_FS_READLINK:
        return fklCreateVMvalueStrFromCstr(exe, req->ptr);
        break;
    case UV_FS_STAT:
    case UV_FS_FSTAT:
    case UV_FS_LSTAT:
        return stat_to_vmtable(exe, &req->statbuf, fpd);
        break;
    case UV_FS_STATFS:
        return statfs_to_vmtable(exe, req->ptr, fpd);
        break;
    case UV_FS_OPENDIR:
        return createFuvDir(exe, pd, req, fs_req->nentries);
        break;
    case UV_FS_READDIR:
        return readdir_result_to_list(exe, req->result, req->ptr, fpd);
        break;
    }
    return FKL_VM_NIL;
}

static inline FklVMvalue *check_fs_uv_result(ssize_t r, struct FuvFsReq *fs_req,
                                             FklVM *exe, FklVMvalue *pd,
                                             int sync) {
    FklVMvalue *req_obj = fs_req->data.req;
    FklVMvalue *retval = FKL_VM_NIL;
    FklVMvalue *err = NULL;
    if (r < 0)
        err = create_fs_uv_err(exe, r, &fs_req->req, fs_req->dest_path, pd);
    if (sync) {
        if (r >= 0)
            retval = create_fs_retval_sync(exe, fs_req, pd);
        cleanup_req(req_obj);
    }
    if (err)
        fklRaiseVMerror(err, exe);
    return retval;
}

static int fuv_fs_cb_value_creator(FklVM *exe, void *a) {
    FklVMvalue *fpd_obj = ((FklCprocFrameContext *)exe->top_frame->data)->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, fpd_obj);

    struct FuvFsReq *freq = a;
    uv_fs_t *req = &freq->req;
    FklVMvalue *err =
        req->result < 0
            ? create_fs_uv_err(exe, req->result, req, freq->dest_path, fpd_obj)
            : FKL_VM_NIL;
    FKL_VM_PUSH_VALUE(exe, err);
    if (req->result < 0) {
        switch (req->fs_type) {
        case UV_FS_CLOSE:
        case UV_FS_MKDIR:
        case UV_FS_RMDIR:
        case UV_FS_RENAME:
        case UV_FS_LINK:
        case UV_FS_UNLINK:
        case UV_FS_UTIME:
        case UV_FS_FUTIME:
        case UV_FS_LUTIME:
        case UV_FS_FSYNC:
        case UV_FS_FDATASYNC:
        case UV_FS_FTRUNCATE:
        case UV_FS_CHMOD:
        case UV_FS_FCHMOD:
        case UV_FS_ACCESS:
        case UV_FS_SYMLINK:
        case UV_FS_COPYFILE:
        case UV_FS_CHOWN:
        case UV_FS_LCHOWN:
        case UV_FS_FCHOWN:
        case UV_FS_CLOSEDIR:
        case UV_FS_SCANDIR:
        case UV_FS_UNKNOWN:
        case UV_FS_CUSTOM:
            break;
        case UV_FS_READ:
        case UV_FS_OPEN:
        case UV_FS_WRITE:
        case UV_FS_SENDFILE:
        case UV_FS_MKDTEMP:
        case UV_FS_REALPATH:
        case UV_FS_READLINK:
        case UV_FS_STAT:
        case UV_FS_FSTAT:
        case UV_FS_LSTAT:
        case UV_FS_STATFS:
        case UV_FS_OPENDIR:
        case UV_FS_READDIR:
            FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
            break;
        case UV_FS_MKSTEMP:
            FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
            FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
            break;
        }
    } else {
        switch (req->fs_type) {
        case UV_FS_CLOSE:
        case UV_FS_MKDIR:
        case UV_FS_RMDIR:
        case UV_FS_RENAME:
        case UV_FS_LINK:
        case UV_FS_UNLINK:
        case UV_FS_UTIME:
        case UV_FS_FUTIME:
        case UV_FS_LUTIME:
        case UV_FS_FSYNC:
        case UV_FS_FDATASYNC:
        case UV_FS_FTRUNCATE:
        case UV_FS_CHMOD:
        case UV_FS_FCHMOD:
        case UV_FS_ACCESS:
        case UV_FS_SYMLINK:
        case UV_FS_COPYFILE:
        case UV_FS_CHOWN:
        case UV_FS_LCHOWN:
        case UV_FS_FCHOWN:
        case UV_FS_CLOSEDIR:
        case UV_FS_SCANDIR:
        case UV_FS_UNKNOWN:
        case UV_FS_CUSTOM:
            break;
        case UV_FS_READ:
            FKL_VM_PUSH_VALUE(
                exe, fklCreateVMvalueStr2(exe, req->result, freq->buf.base));
            break;
        case UV_FS_OPEN:
            FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_FIX(req->result));
            break;
        case UV_FS_WRITE:
        case UV_FS_SENDFILE:
            FKL_VM_PUSH_VALUE(exe, fklMakeVMint(req->result, exe));
            break;
        case UV_FS_MKDTEMP:
            FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueStrFromCstr(exe, req->path));
            break;
        case UV_FS_REALPATH:
        case UV_FS_READLINK:
            FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueStrFromCstr(exe, req->ptr));
            break;
        case UV_FS_MKSTEMP:
            FKL_VM_PUSH_VALUE(exe, FKL_MAKE_VM_FIX(req->result));
            FKL_VM_PUSH_VALUE(exe, fklCreateVMvalueStrFromCstr(exe, req->path));
            break;
        case UV_FS_STAT:
        case UV_FS_FSTAT:
        case UV_FS_LSTAT:
            FKL_VM_PUSH_VALUE(exe, stat_to_vmtable(exe, &req->statbuf, fpd));
            break;
        case UV_FS_STATFS:
            FKL_VM_PUSH_VALUE(exe, statfs_to_vmtable(exe, req->ptr, fpd));
            break;
        case UV_FS_OPENDIR:
            FKL_VM_PUSH_VALUE(exe,
                              createFuvDir(exe, fpd_obj, req, freq->nentries));
            break;
        case UV_FS_READDIR:
            FKL_VM_PUSH_VALUE(
                exe, readdir_result_to_list(exe, req->result, req->ptr, fpd));
            break;
        }
    }

    return 0;
}

static void fuv_fs_cb(uv_fs_t *req) {
    fuv_call_req_callback_in_loop_with_value_creator(
        (uv_req_t *)req, fuv_fs_cb_value_creator,
        uv_req_get_data((uv_req_t *)req));
}

#define FS_CALL(EXE, PD, FUNC, FUV_LOOP, REQ, REQ_OBJ, CB_OBJ, ...)            \
    {                                                                          \
        if (CB_OBJ) {                                                          \
            int r =                                                            \
                FUNC(&(FUV_LOOP)->loop, &(REQ)->req, __VA_ARGS__, fuv_fs_cb);  \
            check_fs_uv_result(r, (REQ), (EXE), (PD), 0);                      \
            FKL_CPROC_RETURN((EXE), ctx, (REQ_OBJ));                           \
        } else {                                                               \
            int r = FUNC(&(FUV_LOOP)->loop, &(REQ)->req, __VA_ARGS__, NULL);   \
            FKL_CPROC_RETURN((EXE), ctx,                                       \
                             check_fs_uv_result(r, (REQ), (EXE), (PD), 1));    \
        }                                                                      \
    }

static int fuv_dir_p(FKL_CPROC_ARGL) { PREDICATE(isFuvDir(val)) }

static inline const char *fs_type_name(uv_fs_type type_id) {
    switch (type_id) {
    case UV_FS_UNKNOWN:
        return NULL;
        break;
    case UV_FS_CUSTOM:
        return "custom";
        break;
    case UV_FS_OPEN:
        return "open";
        break;
    case UV_FS_CLOSE:
        return "close";
        break;
    case UV_FS_READ:
        return "read";
        break;
    case UV_FS_WRITE:
        return "write";
        break;
    case UV_FS_SENDFILE:
        return "sendfile";
        break;
    case UV_FS_STAT:
        return "stat";
        break;
    case UV_FS_LSTAT:
        return "lstat";
        break;
    case UV_FS_FSTAT:
        return "fstat";
        break;
    case UV_FS_STATFS:
        return "statfs";
        break;
    case UV_FS_FTRUNCATE:
        return "ftruncate";
        break;
    case UV_FS_UTIME:
        return "utime";
        break;
    case UV_FS_FUTIME:
        return "futime";
        break;
    case UV_FS_ACCESS:
        return "access";
        break;
    case UV_FS_CHMOD:
        return "chmod";
        break;
    case UV_FS_FCHMOD:
        return "fchmod";
        break;
    case UV_FS_FSYNC:
        return "fsync";
        break;
    case UV_FS_FDATASYNC:
        return "fdatasync";
        break;
    case UV_FS_UNLINK:
        return "unlink";
        break;
    case UV_FS_RMDIR:
        return "rmdir";
        break;
    case UV_FS_MKDIR:
        return "mkdir";
        break;
    case UV_FS_MKDTEMP:
        return "mktemp";
        break;
    case UV_FS_RENAME:
        return "rename";
        break;
    case UV_FS_SCANDIR:
        return "scandir";
        break;
    case UV_FS_LINK:
        return "link";
        break;
    case UV_FS_SYMLINK:
        return "symlink";
        break;
    case UV_FS_READLINK:
        return "readlink";
        break;
    case UV_FS_CHOWN:
        return "chown";
        break;
    case UV_FS_FCHOWN:
        return "fchown";
        break;
    case UV_FS_REALPATH:
        return "realpath";
        break;
    case UV_FS_COPYFILE:
        return "copyfile";
        break;
    case UV_FS_LCHOWN:
        return "lchown";
        break;
    case UV_FS_OPENDIR:
        return "opendir";
        break;
    case UV_FS_READDIR:
        return "readdir";
        break;
    case UV_FS_CLOSEDIR:
        return "closedir";
        break;
    case UV_FS_MKSTEMP:
        return "mkstemp";
        break;
    case UV_FS_LUTIME:
        return "lutime";
        break;
    }
    return NULL;
}

static int fuv_fs_type(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *req_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(req_obj, isFuvFsReq, exe);
    FKL_DECL_VM_UD_DATA(req_ud, FuvReqUd, req_obj);
    struct FuvFsReq *freq = (struct FuvFsReq *)*req_ud;

    uv_fs_t *fs = &freq->req;
    uv_fs_type type_id = uv_fs_get_type(fs);
    const char *name = fs_type_name(type_id);

    FKL_CPROC_RETURN(
        exe, ctx,
        name == NULL
            ? FKL_VM_NIL
            : fklCreateVMvaluePair(exe, fklCreateVMvalueStrFromCstr(exe, name),
                                   FKL_MAKE_VM_FIX(type_id)));
    return 0;
}

static int fuv_fs_close(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(fd_obj, FKL_IS_FIX, exe);
    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_close, fuv_loop, req, req_obj, cb_obj,
            FKL_GET_FIX(fd_obj));
    return 0;
}

static int sid_to_fs_flags(FklSid_t id, FuvPublicData *fpd) {
    if (id == fpd->UV_FS_O_APPEND_sid)
        return UV_FS_O_APPEND;
    if (id == fpd->UV_FS_O_CREAT_sid)
        return UV_FS_O_CREAT;
    if (id == fpd->UV_FS_O_EXCL_sid)
        return UV_FS_O_EXCL;
    if (id == fpd->UV_FS_O_FILEMAP_sid)
        return UV_FS_O_FILEMAP;
    if (id == fpd->UV_FS_O_RANDOM_sid)
        return UV_FS_O_RANDOM;
    if (id == fpd->UV_FS_O_RDONLY_sid)
        return UV_FS_O_RDONLY;
    if (id == fpd->UV_FS_O_RDWR_sid)
        return UV_FS_O_RDWR;
    if (id == fpd->UV_FS_O_SEQUENTIAL_sid)
        return UV_FS_O_SEQUENTIAL;
    if (id == fpd->UV_FS_O_SHORT_LIVED_sid)
        return UV_FS_O_SHORT_LIVED;
    if (id == fpd->UV_FS_O_TEMPORARY_sid)
        return UV_FS_O_TEMPORARY;
    if (id == fpd->UV_FS_O_TRUNC_sid)
        return UV_FS_O_TRUNC;
    if (id == fpd->UV_FS_O_WRONLY_sid)
        return UV_FS_O_WRONLY;
    if (id == fpd->UV_FS_O_DIRECT_sid)
        return UV_FS_O_DIRECT;
    if (id == fpd->UV_FS_O_DIRECTORY_sid)
        return UV_FS_O_DIRECTORY;
    if (id == fpd->UV_FS_O_DSYNC_sid)
        return UV_FS_O_DSYNC;
    if (id == fpd->UV_FS_O_EXLOCK_sid)
        return UV_FS_O_EXLOCK;
    if (id == fpd->UV_FS_O_NOATIME_sid)
        return UV_FS_O_NOATIME;
    if (id == fpd->UV_FS_O_NOCTTY_sid)
        return UV_FS_O_NOCTTY;
    if (id == fpd->UV_FS_O_NOFOLLOW_sid)
        return UV_FS_O_NOFOLLOW;
    if (id == fpd->UV_FS_O_NONBLOCK_sid)
        return UV_FS_O_NONBLOCK;
    if (id == fpd->UV_FS_O_SYMLINK_sid)
        return UV_FS_O_SYMLINK;
    if (id == fpd->UV_FS_O_SYNC_sid)
        return UV_FS_O_SYNC;
    return -1;
}

static inline FklBuiltinErrorType list_to_fs_flags(FklVMvalue *cur_pair,
                                                   int *flags, FklVMvalue *pd) {
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    for (; cur_pair != FKL_VM_NIL; cur_pair = FKL_VM_CDR(cur_pair)) {
        FklVMvalue *cur = FKL_VM_CAR(cur_pair);
        if (FKL_IS_SYM(cur)) {
            int flag = sid_to_fs_flags(FKL_GET_SYM(cur), fpd);
            if (flag < 0)
                return FKL_ERR_INVALID_VALUE;
            (*flags) |= flag;
        } else
            return FKL_ERR_INCORRECT_TYPE_VALUE;
    }
    return 0;
}

static inline int str_to_fs_flags(const char *str) {
    if (strcmp(str, "r") == 0)
        return UV_FS_O_RDONLY;
    if (strcmp(str, "rs") == 0 || strcmp(str, "sr") == 0)
        return UV_FS_O_RDONLY | UV_FS_O_SYNC;

    if (strcmp(str, "r+") == 0)
        return UV_FS_O_RDWR;
    if (strcmp(str, "rs+") == 0 || strcmp(str, "sr+") == 0)
        return UV_FS_O_RDWR | UV_FS_O_SYNC;

    if (strcmp(str, "w") == 0)
        return UV_FS_O_TRUNC | UV_FS_O_CREAT | UV_FS_O_WRONLY;
    if (strcmp(str, "wx") == 0 || strcmp(str, "xw") == 0)
        return UV_FS_O_TRUNC | UV_FS_O_CREAT | UV_FS_O_WRONLY | UV_FS_O_EXCL;

    if (strcmp(str, "w+") == 0)
        return UV_FS_O_TRUNC | UV_FS_O_CREAT | UV_FS_O_RDWR;
    if (strcmp(str, "wx+") == 0 || strcmp(str, "xw+") == 0)
        return UV_FS_O_TRUNC | UV_FS_O_CREAT | UV_FS_O_RDWR | UV_FS_O_EXCL;

    if (strcmp(str, "a") == 0)
        return UV_FS_O_APPEND | UV_FS_O_CREAT | UV_FS_O_WRONLY;
    if (strcmp(str, "ax") == 0 || strcmp(str, "xa") == 0)
        return UV_FS_O_APPEND | UV_FS_O_CREAT | UV_FS_O_WRONLY | UV_FS_O_EXCL;

    if (strcmp(str, "a+") == 0)
        return UV_FS_O_APPEND | UV_FS_O_CREAT | UV_FS_O_RDWR;
    if (strcmp(str, "ax+") == 0 || strcmp(str, "xa+") == 0)
        return UV_FS_O_APPEND | UV_FS_O_CREAT | UV_FS_O_RDWR | UV_FS_O_EXCL;
    return -1;
}

static int fuv_fs_open(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 4, 5);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *flags_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *mode_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FklVMvalue *cb_obj = argc > 4 ? FKL_CPROC_GET_ARG(exe, ctx, 4) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    int flags = 0;
    if (fklIsList(flags_obj)) {
        FklBuiltinErrorType err_type =
            list_to_fs_flags(flags_obj, &flags, ctx->pd);
        if (err_type)
            FKL_RAISE_BUILTIN_ERROR(err_type, exe);
    } else if (FKL_IS_STR(flags_obj)) {
        flags = str_to_fs_flags(FKL_VM_STR(flags_obj)->str);
        if (flags < 0)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    int mode = 0;
    if (FKL_IS_FIX(mode_obj))
        mode = FKL_GET_FIX(mode_obj);
    else if (mode_obj == FKL_VM_NIL)
        mode = 0644;
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_open, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, flags, mode);
    return 0;
}

static inline FklBuiltinErrorType setup_fs_write_buf(FklVMvalue **parg,
                                                     FklVMvalue **const arg_end,
                                                     FklStringBuffer *buf) {
    for (; parg < arg_end; ++parg) {
        FklVMvalue *cur = *parg;
        if (FKL_IS_STR(cur))
            fklStringBufferConcatWithString(buf, FKL_VM_STR(cur));
        else if (FKL_IS_BYTEVECTOR(cur))
            fklStringBufferBincpy(buf, FKL_VM_BVEC(cur)->ptr,
                                  FKL_VM_BVEC(cur)->size);
        else
            return FKL_ERR_INCORRECT_TYPE_VALUE;
    }
    return 0;
}

static int fuv_fs_read(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 4, 5);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *len_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *offset_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FklVMvalue *cb_obj = argc > 4 ? FKL_CPROC_GET_ARG(exe, ctx, 4) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(fd_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(len_obj, fklIsVMint, exe);

    if (offset_obj != FKL_VM_NIL && !fklIsVMint(offset_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    int64_t len = fklVMgetInt(len_obj);
    if (len < 0)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    int64_t offset =
        offset_obj && offset_obj != FKL_VM_NIL ? fklVMgetInt(offset_obj) : -1;

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, len);

    FS_CALL(exe, ctx->pd, uv_fs_read, fuv_loop, req, req_obj, cb_obj,
            FKL_GET_FIX(fd_obj), &req->buf, 1, offset);
    return 0;
}

static int fuv_fs_unlink(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_unlink, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str);
    return 0;
}

static int fuv_fs_write(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 4, -1);
    FklVMvalue **const arg_base = &FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *loop_obj = arg_base[0];
    FklVMvalue *fd_obj = arg_base[1];
    FklVMvalue *offset_obj = arg_base[2];
    FklVMvalue *cb_obj = arg_base[3];
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(fd_obj, FKL_IS_FIX, exe);

    if (cb_obj == FKL_VM_NIL)
        cb_obj = NULL;

    if (offset_obj != FKL_VM_NIL && !fklIsVMint(offset_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    int64_t offset =
        offset_obj && offset_obj != FKL_VM_NIL ? fklVMgetInt(offset_obj) : -1;

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    FklBuiltinErrorType err_type =
        argc > 4 ? setup_fs_write_buf(&arg_base[4], arg_base + argc, &buf) : 0;

    if (err_type) {
        fklUninitStringBuffer(&buf);
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);
    }

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, buf.index);
    memcpy(req->buf.base, buf.buf, sizeof(char) * buf.index);
    fklUninitStringBuffer(&buf);

    FS_CALL(exe, ctx->pd, uv_fs_write, fuv_loop, req, req_obj, cb_obj,
            FKL_GET_FIX(fd_obj), &req->buf, 1, offset);
    return 0;
}

static int fuv_fs_mkdir(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 4);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *mode_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *cb_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    int mode = 0;
    if (FKL_IS_FIX(mode_obj))
        mode = FKL_GET_FIX(mode_obj);
    else if (mode_obj == FKL_VM_NIL)
        mode = 0755;
    else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_mkdir, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, mode);
    return 0;
}

static int fuv_fs_mkdtemp(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *tpl_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(tpl_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_mkdtemp, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(tpl_obj)->str);
    return 0;
}

static int fuv_fs_mkstemp(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *tpl_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(tpl_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_mkstemp, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(tpl_obj)->str);
    return 0;
}

static int fuv_fs_rmdir(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_rmdir, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str);
    return 0;
}

static int fuv_fs_opendir(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 4);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FklVMvalue *nentries_obj = FKL_VM_TRUE;
    if (cb_obj) {
        if (fklIsCallable(cb_obj)) {
            if (argc > 3)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, exe);
        } else {
            nentries_obj = cb_obj;
            cb_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
        }
    }
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(nentries_obj, fklIsVMnumber, exe);

    size_t nentries = fklVMgetUint(nentries_obj);
    if (fklIsVMnumberLt0(nentries_obj) || nentries < 1)
        raiseFuvError(FUV_ERR_NUMBER_SHOULD_NOT_BE_LT_1, exe, ctx->pd);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);
    req->nentries = nentries;

    FS_CALL(exe, ctx->pd, uv_fs_opendir, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str);
    return 0;
}

static int fuv_fs_closedir(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *dir_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(dir_obj, isFuvDir, exe);

    FKL_DECL_VM_UD_DATA(dir, FuvDir, dir_obj);

    if (isFuvDirUsing(dir))
        raiseFuvError(FUV_ERR_CLOSE_USING_DIR, exe, ctx->pd);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);
    req->dir = refFuvDir(dir);

    uv_dir_t *d = dir->dir;
    free(d->dirents);
    d->nentries = 0;
    dir->dir = NULL;

    FS_CALL(exe, ctx->pd, uv_fs_closedir, fuv_loop, req, req_obj, cb_obj, d);
    return 0;
}

static int fuv_fs_readdir(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *dir_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(dir_obj, isFuvDir, exe);

    FKL_DECL_VM_UD_DATA(dir, FuvDir, dir_obj);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);
    req->dir = refFuvDir(dir);

    FS_CALL(exe, ctx->pd, uv_fs_readdir, fuv_loop, req, req_obj, cb_obj,
            dir->dir);
    return 0;
}

static int fuv_scandir_cb_value_creator(FklVM *exe, void *a) {
    struct FuvFsReq *req = a;
    uv_fs_t *fs = &req->req;
    if (fs->result < 0) {
        FklVMvalue *fpd_obj =
            ((FklCprocFrameContext *)exe->top_frame->data)->pd;
        FklVMvalue *err =
            fs->result < 0
                ? create_fs_uv_err(exe, fs->result, fs, req->dest_path, fpd_obj)
                : FKL_VM_NIL;
        FKL_VM_PUSH_VALUE(exe, err);
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        return 0;
    } else {
        FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, req->data.loop);
        fklVMvalueHashSetDel2(&fuv_loop->data.gc_values, req->data.req);
        req->data.loop = NULL;
        req->data.callback = NULL;
        FKL_VM_PUSH_VALUE(exe, FKL_VM_NIL);
        FKL_VM_PUSH_VALUE(exe, req->data.req);
        return 1;
    }
}

static void fuv_scandir_cb(uv_fs_t *req) {
    fuv_call_req_callback_in_loop_with_value_creator(
        (uv_req_t *)req, fuv_scandir_cb_value_creator,
        uv_req_get_data((uv_req_t *)req));
}

static int fuv_fs_scandir(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;

    int ret = 0;
    if (cb_obj) {
        struct FuvFsReq *req =
            createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);
        ret = uv_fs_scandir(&fuv_loop->loop, &req->req,
                            FKL_VM_STR(path_obj)->str, 0, fuv_scandir_cb);
    } else {
        struct FuvFsReq *req =
            createFuvFsReq(exe, &req_obj, ctx->proc, NULL, NULL, 0);
        ret = uv_fs_scandir(&fuv_loop->loop, &req->req,
                            FKL_VM_STR(path_obj)->str, 0, NULL);
    }
    CHECK_UV_RESULT_AND_CLEANUP_REQ(ret, req_obj, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, req_obj);
    return 0;
}

static int fuv_fs_scandir_next(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *req_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(req_obj, isFuvFsReq, exe);

    FKL_DECL_VM_UD_DATA(req_ud, FuvReqUd, req_obj);
    struct FuvFsReq *freq = (struct FuvFsReq *)*req_ud;

    uv_fs_t *fs = &freq->req;
    if (fs->fs_type != UV_FS_SCANDIR)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    uv_dirent_t ent = {.name = NULL};
    int ret = uv_fs_scandir_next(fs, &ent);
    if (ret == UV_EOF)
        FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    else {
        CHECK_UV_RESULT(ret, exe, ctx->pd);
        FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
        FKL_CPROC_RETURN(exe, ctx, dirent_to_vmtable(exe, &ent, fpd));
    }
    return 0;
}

static int fuv_fs_stat(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_stat, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str);
    return 0;
}

static int fuv_fs_fstat(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(fd_obj, FKL_IS_FIX, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_fstat, fuv_loop, req, req_obj, cb_obj,
            FKL_GET_FIX(fd_obj));
    return 0;
}

static int fuv_fs_lstat(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_lstat, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str);
    return 0;
}

static int fuv_fs_statfs(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_statfs, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str);
    return 0;
}

static int fuv_fs_rename(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 4);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *new_path_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *cb_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(new_path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);
    req->dest_path = new_path_obj;

    FS_CALL(exe, ctx->pd, uv_fs_rename, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, FKL_VM_STR(new_path_obj)->str);
    return 0;
}

static int fuv_fs_fsync(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(fd_obj, FKL_IS_FIX, exe);
    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_fsync, fuv_loop, req, req_obj, cb_obj,
            FKL_GET_FIX(fd_obj));
    return 0;
}

static int fuv_fs_fdatasync(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(fd_obj, FKL_IS_FIX, exe);
    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_fdatasync, fuv_loop, req, req_obj, cb_obj,
            FKL_GET_FIX(fd_obj));
    return 0;
}

static int fuv_fs_ftruncate(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 4);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *offset_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *cb_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(fd_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(offset_obj, fklIsVMint, exe);
    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_ftruncate, fuv_loop, req, req_obj, cb_obj,
            FKL_GET_FIX(fd_obj), fklVMgetInt(offset_obj));
    return 0;
}

static inline FklBuiltinErrorType
list_to_copyfile_flags(FklVMvalue *cur_pair, int *flags, FklVMvalue *pd) {
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    for (; cur_pair != FKL_VM_NIL; cur_pair = FKL_VM_CDR(cur_pair)) {
        FklVMvalue *cur = FKL_VM_CAR(cur_pair);
        if (FKL_IS_SYM(cur)) {
            FklSid_t id = FKL_GET_SYM(cur);
            if (id == fpd->UV_FS_COPYFILE_EXCL_sid)
                (*flags) |= UV_FS_COPYFILE_EXCL;
            else if (id == fpd->UV_FS_COPYFILE_FICLONE_sid)
                (*flags) |= UV_FS_COPYFILE_FICLONE;
            else if (id == fpd->UV_FS_COPYFILE_FICLONE_FORCE_sid)
                (*flags) |= UV_FS_COPYFILE_FICLONE_FORCE;
            else
                return FKL_ERR_INVALID_VALUE;
        } else
            return FKL_ERR_INCORRECT_TYPE_VALUE;
    }
    return 0;
}

static int fuv_fs_copyfile(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 5);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *new_path_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *cb_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    FklVMvalue *flags_obj = FKL_VM_NIL;
    if (cb_obj) {
        if (fklIsCallable(cb_obj)) {
            if (argc > 4)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, exe);
        } else {
            flags_obj = cb_obj;
            cb_obj = argc > 4 ? FKL_CPROC_GET_ARG(exe, ctx, 4) : NULL;
        }
    }

    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(new_path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(flags_obj, fklIsList, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    int flags = 0;
    FklBuiltinErrorType err_type =
        list_to_copyfile_flags(flags_obj, &flags, ctx->pd);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);
    req->dest_path = new_path_obj;

    FS_CALL(exe, ctx->pd, uv_fs_copyfile, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, FKL_VM_STR(new_path_obj)->str, flags);
    return 0;
}

static int fuv_fs_sendfile(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 5, 6);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *out_fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *in_fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *offset_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FklVMvalue *len_obj = FKL_CPROC_GET_ARG(exe, ctx, 4);
    FklVMvalue *cb_obj = argc > 5 ? FKL_CPROC_GET_ARG(exe, ctx, 5) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(out_fd_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(in_fd_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(offset_obj, fklIsVMint, exe);
    FKL_CHECK_TYPE(len_obj, fklIsVMint, exe);

    if (fklIsVMnumberLt0(len_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_sendfile, fuv_loop, req, req_obj, cb_obj,
            FKL_GET_FIX(out_fd_obj), FKL_GET_FIX(in_fd_obj),
            fklVMgetInt(offset_obj), fklVMgetUint(len_obj));
    return 0;
}

static inline int str_to_amode(FklString *str) {
    int mode = F_OK;
    const char *cur = str->str;
    const char *end = &cur[str->size];
    for (; cur < end; cur++) {
        switch (*cur) {
        case 'r':
        case 'R':
            mode |= R_OK;
            break;
        case 'w':
        case 'W':
            mode |= W_OK;
            break;
        case 'x':
        case 'X':
            mode |= X_OK;
            break;
        default:
            return -1;
            break;
        }
    }
    return mode;
}

static int fuv_fs_access(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 4);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *mode_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *cb_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);

    int amode = 0;
    if (FKL_IS_FIX(mode_obj))
        amode = FKL_GET_FIX(mode_obj);
    else if (FKL_IS_STR(mode_obj)) {
        amode = str_to_amode(FKL_VM_STR(mode_obj));
        if (amode < 0)
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_access, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, amode);
    return 0;
}

static int fuv_fs_chmod(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 4);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *mode_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *cb_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(mode_obj, FKL_IS_FIX, exe);
    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_chmod, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, FKL_GET_FIX(mode_obj));
    return 0;
}

static int fuv_fs_fchmod(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 4);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *mode_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *cb_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(fd_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(mode_obj, FKL_IS_FIX, exe);
    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_fchmod, fuv_loop, req, req_obj, cb_obj,
            FKL_GET_FIX(fd_obj), FKL_GET_FIX(mode_obj));
    return 0;
}

static int fuv_fs_utime(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 4, 5);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *atime_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *mtime_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FklVMvalue *cb_obj = argc > 4 ? FKL_CPROC_GET_ARG(exe, ctx, 4) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(atime_obj, FKL_IS_F64, exe);
    FKL_CHECK_TYPE(mtime_obj, FKL_IS_F64, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_utime, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, FKL_VM_F64(atime_obj),
            FKL_VM_F64(mtime_obj));
    return 0;
}

static int fuv_fs_futime(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 4, 5);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *atime_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *mtime_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FklVMvalue *cb_obj = argc > 4 ? FKL_CPROC_GET_ARG(exe, ctx, 4) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(fd_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(atime_obj, FKL_IS_F64, exe);
    FKL_CHECK_TYPE(mtime_obj, FKL_IS_F64, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_futime, fuv_loop, req, req_obj, cb_obj,
            FKL_GET_FIX(fd_obj), FKL_VM_F64(atime_obj), FKL_VM_F64(mtime_obj));
    return 0;
}

static int fuv_fs_lutime(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 4, 5);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *atime_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *mtime_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FklVMvalue *cb_obj = argc > 4 ? FKL_CPROC_GET_ARG(exe, ctx, 4) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(atime_obj, FKL_IS_F64, exe);
    FKL_CHECK_TYPE(mtime_obj, FKL_IS_F64, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_lutime, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, FKL_VM_F64(atime_obj),
            FKL_VM_F64(mtime_obj));
    return 0;
}

static int fuv_fs_link(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 4);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *new_path_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *cb_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(new_path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);
    req->dest_path = new_path_obj;

    FS_CALL(exe, ctx->pd, uv_fs_link, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, FKL_VM_STR(new_path_obj)->str);
    return 0;
}

static inline FklBuiltinErrorType
list_to_symlink_flags(FklVMvalue *cur_pair, int *flags, FklVMvalue *pd) {
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    for (; cur_pair != FKL_VM_NIL; cur_pair = FKL_VM_CDR(cur_pair)) {
        FklVMvalue *cur = FKL_VM_CAR(cur_pair);
        if (FKL_IS_SYM(cur)) {
            FklSid_t id = FKL_GET_SYM(cur);
            if (id == fpd->UV_FS_SYMLINK_DIR_sid)
                (*flags) |= UV_FS_SYMLINK_DIR;
            else if (id == fpd->UV_FS_SYMLINK_JUNCTION_sid)
                (*flags) |= UV_FS_SYMLINK_JUNCTION;
            else
                return FKL_ERR_INVALID_VALUE;
        } else
            return FKL_ERR_INCORRECT_TYPE_VALUE;
    }
    return 0;
}

static int fuv_fs_symlink(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 3, 5);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *new_path_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *cb_obj = argc > 3 ? FKL_CPROC_GET_ARG(exe, ctx, 3) : NULL;
    FklVMvalue *flags_obj = FKL_VM_NIL;
    if (cb_obj) {
        if (fklIsCallable(cb_obj)) {
            if (argc > 4)
                FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG, exe);
        } else {
            flags_obj = cb_obj;
            cb_obj = argc > 4 ? FKL_CPROC_GET_ARG(exe, ctx, 4) : NULL;
        }
    }

    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(new_path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(flags_obj, fklIsList, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    int flags = 0;
    FklBuiltinErrorType err_type =
        list_to_symlink_flags(flags_obj, &flags, ctx->pd);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);
    req->dest_path = new_path_obj;

    FS_CALL(exe, ctx->pd, uv_fs_symlink, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, FKL_VM_STR(new_path_obj)->str, flags);
    return 0;
}

static int fuv_fs_readlink(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_readlink, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str);
    return 0;
}

static int fuv_fs_realpath(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_realpath, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str);
    return 0;
}

static int fuv_fs_chown(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 4, 5);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *uid_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *gid_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FklVMvalue *cb_obj = argc > 4 ? FKL_CPROC_GET_ARG(exe, ctx, 4) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(uid_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(gid_obj, FKL_IS_FIX, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_chown, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, FKL_GET_FIX(uid_obj),
            FKL_GET_FIX(gid_obj));
    return 0;
}

static int fuv_fs_fchown(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 4, 5);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *uid_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *gid_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FklVMvalue *cb_obj = argc > 4 ? FKL_CPROC_GET_ARG(exe, ctx, 4) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(fd_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(uid_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(gid_obj, FKL_IS_FIX, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_fchown, fuv_loop, req, req_obj, cb_obj,
            FKL_GET_FIX(fd_obj), FKL_GET_FIX(uid_obj), FKL_GET_FIX(gid_obj));
    return 0;
}

static int fuv_fs_lchown(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 4, 5);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *uid_obj = FKL_CPROC_GET_ARG(exe, ctx, 2);
    FklVMvalue *gid_obj = FKL_CPROC_GET_ARG(exe, ctx, 3);
    FklVMvalue *cb_obj = argc > 4 ? FKL_CPROC_GET_ARG(exe, ctx, 4) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(uid_obj, FKL_IS_FIX, exe);
    FKL_CHECK_TYPE(gid_obj, FKL_IS_FIX, exe);

    if (cb_obj)
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);

    FklVMvalue *req_obj = NULL;
    struct FuvFsReq *req =
        createFuvFsReq(exe, &req_obj, ctx->proc, loop_obj, cb_obj, 0);

    FS_CALL(exe, ctx->pd, uv_fs_lchown, fuv_loop, req, req_obj, cb_obj,
            FKL_VM_STR(path_obj)->str, FKL_GET_FIX(uid_obj),
            FKL_GET_FIX(gid_obj));
    return 0;
}

static int fuv_guess_handle(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *fd_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(fd_obj, FKL_IS_FIX, exe);
    uv_file fd = FKL_GET_FIX(fd_obj);
    uv_handle_type type_id = uv_guess_handle(fd);
    const char *name = uv_handle_type_name(type_id);
    FKL_CPROC_RETURN(
        exe, ctx,
        name == NULL
            ? FKL_VM_NIL
            : fklCreateVMvaluePair(exe, fklCreateVMvalueStrFromCstr(exe, name),
                                   FKL_MAKE_VM_FIX(type_id)));
    return 0;
}

#define MAX_TITLE_LENGTH (8192)

static int fuv_get_process_title(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    char title[MAX_TITLE_LENGTH];
    int r = uv_get_process_title(title, MAX_TITLE_LENGTH);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStrFromCstr(exe, title));
    return 0;
}

static int fuv_set_process_title(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *title_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(title_obj, FKL_IS_STR, exe);
    int r = uv_set_process_title(FKL_VM_STR(title_obj)->str);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, title_obj);
    return 0;
}

static int fuv_resident_set_memory(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    size_t rss;
    int r = uv_resident_set_memory(&rss);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(rss, exe));
    return 0;
}

static int fuv_uptime(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    double uptime;
    int r = uv_uptime(&uptime);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueF64(exe, uptime));
    return 0;
}

static inline FklVMvalue *
timeval_to_vmtable(FklVM *exe, const uv_timeval_t *spec, FuvPublicData *fpd) {
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->timespec_f_sec_sid),
                      fklMakeVMint(spec->tv_sec, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->timeval_f_usec_sid),
                      fklMakeVMint(spec->tv_usec, exe));

    return hash;
}

static inline FklVMvalue *rusage_to_vmtable(FklVM *exe, uv_rusage_t *r,
                                            FklVMvalue *pd) {
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_utime_sid),
                      timeval_to_vmtable(exe, &r->ru_utime, fpd));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_stime_sid),
                      timeval_to_vmtable(exe, &r->ru_stime, fpd));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_maxrss_sid),
                      fklMakeVMuint(r->ru_maxrss, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_ixrss_sid),
                      fklMakeVMuint(r->ru_ixrss, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_idrss_sid),
                      fklMakeVMuint(r->ru_idrss, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_isrss_sid),
                      fklMakeVMuint(r->ru_isrss, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_minflt_sid),
                      fklMakeVMuint(r->ru_minflt, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_majflt_sid),
                      fklMakeVMuint(r->ru_majflt, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_nswap_sid),
                      fklMakeVMuint(r->ru_nswap, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_inblock_sid),
                      fklMakeVMuint(r->ru_inblock, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_oublock_sid),
                      fklMakeVMuint(r->ru_oublock, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_msgsnd_sid),
                      fklMakeVMuint(r->ru_msgsnd, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_msgrcv_sid),
                      fklMakeVMuint(r->ru_msgrcv, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_nsignals_sid),
                      fklMakeVMuint(r->ru_nsignals, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_nvcsw_sid),
                      fklMakeVMuint(r->ru_nvcsw, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->rusage_nivcsw_sid),
                      fklMakeVMuint(r->ru_nivcsw, exe));
    return hash;
}

static int fuv_getrusage(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uv_rusage_t buf;
    int ret = uv_getrusage(&buf);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, rusage_to_vmtable(exe, &buf, ctx->pd));
    return 0;
}

static int fuv_getrusage_thread(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uv_rusage_t buf;
    int ret = uv_getrusage_thread(&buf);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, rusage_to_vmtable(exe, &buf, ctx->pd));
    return 0;
}

static int fuv_os_getpid(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uv_pid_t pid = uv_os_getpid();
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(pid));
    return 0;
}

static int fuv_os_getppid(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uv_pid_t ppid = uv_os_getppid();
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(ppid));
    return 0;
}

static int fuv_available_parallelism(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    unsigned int rc = uv_available_parallelism();
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(rc));
    return 0;
}

static inline FklVMvalue *cpu_info_to_vmtable(FklVM *exe, uv_cpu_info_t *info,
                                              FuvPublicData *fpd) {
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);
    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->cpu_info_model_sid),
                      fklCreateVMvalueStrFromCstr(exe, info->model));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->cpu_info_speed_sid),
                      FKL_MAKE_VM_FIX(info->speed));

    FklVMvalue *cpu_times = fklCreateVMvalueHashEq(exe);
    FklVMhash *cht = FKL_VM_HASH(cpu_times);

    fklVMhashTableSet(cht, FKL_MAKE_VM_SYM(fpd->cpu_info_times_user_sid),
                      fklMakeVMuint(info->cpu_times.user, exe));

    fklVMhashTableSet(cht, FKL_MAKE_VM_SYM(fpd->cpu_info_times_nice_sid),
                      fklMakeVMuint(info->cpu_times.nice, exe));

    fklVMhashTableSet(cht, FKL_MAKE_VM_SYM(fpd->cpu_info_times_sys_sid),
                      fklMakeVMuint(info->cpu_times.sys, exe));

    fklVMhashTableSet(cht, FKL_MAKE_VM_SYM(fpd->cpu_info_times_idle_sid),
                      fklMakeVMuint(info->cpu_times.idle, exe));

    fklVMhashTableSet(cht, FKL_MAKE_VM_SYM(fpd->cpu_info_times_irq_sid),
                      fklMakeVMuint(info->cpu_times.irq, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->cpu_info_times_sid), cpu_times);
    return hash;
}

static inline FklVMvalue *cpu_infos_to_vmvec(FklVM *exe, uv_cpu_info_t *infos,
                                             int count, FklVMvalue *pd) {
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    FklVMvalue *v = fklCreateVMvalueVec(exe, count);
    FklVMvec *vec = FKL_VM_VEC(v);

    for (int i = 0; i < count; i++)
        vec->base[i] = cpu_info_to_vmtable(exe, &infos[i], fpd);
    return v;
}

static int fuv_cpu_info(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    int count = 0;
    uv_cpu_info_t *infos = NULL;
    int ret = uv_cpu_info(&infos, &count);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, cpu_infos_to_vmvec(exe, infos, count, ctx->pd));
    uv_free_cpu_info(infos, count);
    return 0;
}

static int fuv_cpumask_size(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    int r = uv_cpumask_size();
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(r));
    return 0;
}

static inline FklVMvalue *
interface_addresses_to_vec(FklVM *exe, uv_interface_address_t *addresses,
                           int count, FklVMvalue *pd) {
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    FklVMvalue *v = fklCreateVMvalueVec(exe, count);
    FklVMvec *vec = FKL_VM_VEC(v);
    char ip[INET6_ADDRSTRLEN];
    char netmask[INET6_ADDRSTRLEN];
    for (int i = 0; i < count; i++) {
        uv_interface_address_t *cur = &addresses[i];
        FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
        vec->base[i] = hash;
        FklVMhash *ht = FKL_VM_HASH(hash);

        fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->ifa_f_name_sid),
                          fklCreateVMvalueStrFromCstr(exe, cur->name));

        fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->ifa_f_mac_sid),
                          fklCreateVMvalueBvec2(
                              exe, sizeof(cur->phys_addr),
                              FKL_TYPE_CAST(const uint8_t *, cur->phys_addr)));

        fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->ifa_f_internal_sid),
                          cur->is_internal ? FKL_VM_TRUE : FKL_VM_NIL);

        if (cur->address.address4.sin_family == AF_INET) {
            uv_ip4_name(&cur->address.address4, ip, sizeof(ip));
            uv_ip4_name(&cur->netmask.netmask4, netmask, sizeof(netmask));
        } else if (cur->address.address4.sin_family == AF_INET6) {
            uv_ip6_name(&cur->address.address6, ip, sizeof(ip));
            uv_ip6_name(&cur->netmask.netmask6, netmask, sizeof(ip));
        } else {
            strncpy(ip, "<unknown>", INET6_ADDRSTRLEN);
            strncpy(netmask, "<unknown>", INET6_ADDRSTRLEN);
        }

        fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->ifa_f_ip_sid),
                          fklCreateVMvalueStrFromCstr(exe, ip));
        fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->ifa_f_netmask_sid),
                          fklCreateVMvalueStrFromCstr(exe, netmask));
        fklVMhashTableSet(
            ht, FKL_MAKE_VM_SYM(fpd->ifa_f_family_sid),
            af_num_to_symbol(cur->address.address4.sin_family, fpd));
    }
    return v;
}

static int fuv_interface_address(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uv_interface_address_t *addresses;
    int count = 0;
    int r = uv_interface_addresses(&addresses, &count);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(
        exe, ctx, interface_addresses_to_vec(exe, addresses, count, ctx->pd));
    uv_free_interface_addresses(addresses, count);
    return 0;
}

static int fuv_loadavg(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    double r[3];
    uv_loadavg(r);
    FKL_CPROC_RETURN(exe, ctx,
                     fklCreateVMvalueVec3(exe, fklCreateVMvalueF64(exe, r[0]),
                                          fklCreateVMvalueF64(exe, r[1]),
                                          fklCreateVMvalueF64(exe, r[2])));
    return 0;
}

static int fuv_if_indextoname(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *idx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(idx_obj, FKL_IS_FIX, exe);
    int64_t idx = FKL_GET_FIX(idx_obj);
    if (idx < 0)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    char buf[UV_IF_NAMESIZE];
    size_t size = UV_IF_NAMESIZE;
    int r = uv_if_indextoname(idx, buf, &size);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStrFromCstr(exe, buf));
    return 0;
}

static int fuv_if_indextoiid(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *idx_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(idx_obj, FKL_IS_FIX, exe);
    int64_t idx = FKL_GET_FIX(idx_obj);
    if (idx < 0)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    char buf[UV_IF_NAMESIZE];
    size_t size = UV_IF_NAMESIZE;
    int r = uv_if_indextoiid(idx, buf, &size);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStrFromCstr(exe, buf));
    return 0;
}

static int fuv_exepath(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    size_t size = 2 * FKL_PATH_MAX;
    char exe_path[2 * FKL_PATH_MAX];
    int r = uv_exepath(exe_path, &size);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStrFromCstr(exe, exe_path));
    return 0;
}

static int fuv_cwd(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    size_t size = 2 * FKL_PATH_MAX;
    char exe_path[2 * FKL_PATH_MAX];
    int r = uv_cwd(exe_path, &size);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStrFromCstr(exe, exe_path));
    return 0;
}

static int fuv_chdir(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *path_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(path_obj, FKL_IS_STR, exe);
    int r = uv_chdir(FKL_VM_STR(path_obj)->str);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int fuv_os_homedir(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    size_t size = 2 * FKL_PATH_MAX;
    char exe_path[2 * FKL_PATH_MAX];
    int r = uv_os_homedir(exe_path, &size);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStrFromCstr(exe, exe_path));
    return 0;
}

static int fuv_os_tmpdir(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    size_t size = 2 * FKL_PATH_MAX;
    char exe_path[2 * FKL_PATH_MAX];
    int r = uv_os_tmpdir(exe_path, &size);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStrFromCstr(exe, exe_path));
    return 0;
}

static inline FklVMvalue *passwd_to_vmtable(FklVM *exe, uv_passwd_t *passwd,
                                            FklVMvalue *pd) {
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->passwd_username_sid),
                      fklCreateVMvalueStrFromCstr(exe, passwd->username));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->passwd_uid_sid),
                      fklMakeVMint(passwd->uid, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->passwd_gid_sid),
                      fklMakeVMint(passwd->gid, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->passwd_shell_sid),
                      passwd->shell
                          ? fklCreateVMvalueStrFromCstr(exe, passwd->shell)
                          : FKL_VM_NIL);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->passwd_homedir_sid),
                      passwd->homedir
                          ? fklCreateVMvalueStrFromCstr(exe, passwd->homedir)
                          : FKL_VM_NIL);
    return hash;
}

static int fuv_os_get_passwd(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uv_passwd_t passwd;
    int r = uv_os_get_passwd(&passwd);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, passwd_to_vmtable(exe, &passwd, ctx->pd));
    uv_os_free_passwd(&passwd);
    return 0;
}

static int fuv_get_free_memory(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uint64_t mem = uv_get_free_memory();
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(mem, exe));
    return 0;
}

static int fuv_get_total_memory(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uint64_t mem = uv_get_total_memory();
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(mem, exe));
    return 0;
}

static int fuv_get_constrained_memory(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uint64_t mem = uv_get_constrained_memory();
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(mem, exe));
    return 0;
}

static int fuv_get_available_memory(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uint64_t mem = uv_get_available_memory();
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(mem, exe));
    return 0;
}

static int fuv_hrtime(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uint64_t t = uv_hrtime();
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(t, exe));
    return 0;
}

static inline FklVMvalue *timespec64_to_vmtable(FklVM *exe,
                                                const uv_timespec64_t *spec,
                                                FuvPublicData *fpd) {
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->timespec_f_sec_sid),
                      fklMakeVMint(spec->tv_sec, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->timespec_f_nsec_sid),
                      fklMakeVMint(spec->tv_nsec, exe));

    return hash;
}

static inline FklBuiltinErrorType
sid_to_clockid(FklVMvalue *obj, uv_clock_id *id, FuvPublicData *fpd) {
    FklSid_t sid = FKL_GET_SYM(obj);
    if (sid == fpd->UV_CLOCK_MONOTONIC_sid)
        *id = UV_CLOCK_MONOTONIC;
    else if (sid == fpd->UV_CLOCK_REALTIME_sid)
        *id = UV_CLOCK_REALTIME;
    else
        return FKL_ERR_INVALID_VALUE;
    return 0;
}

static int fuv_clock_gettime(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *clockid_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(clockid_obj, FKL_IS_SYM, exe);
    uv_timespec64_t ts;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    uv_clock_id id = 0;
    FklBuiltinErrorType err_type = sid_to_clockid(clockid_obj, &id, fpd);
    if (err_type)
        FKL_RAISE_BUILTIN_ERROR(err_type, exe);
    int ret = uv_clock_gettime(id, &ts);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, timespec64_to_vmtable(exe, &ts, fpd));
    return 0;
}

static int fuv_print_all_handles(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *stream_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(stream_obj, fklIsVMvalueFp, exe);
    FklVMfp *vfp = FKL_VM_FP(stream_obj);
    if (!vfp->fp)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    if (!(vfp->rw & FKL_VM_FP_W_MASK))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNSUPPORTED_OP, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    uv_print_all_handles(&fuv_loop->loop, vfp->fp);
    FKL_CPROC_RETURN(exe, ctx, loop_obj);
    return 0;
}

static int fuv_print_active_handles(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *stream_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(stream_obj, fklIsVMvalueFp, exe);
    FklVMfp *vfp = FKL_VM_FP(stream_obj);
    if (!vfp->fp)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALIDACCESS, exe);
    if (!(vfp->rw & FKL_VM_FP_W_MASK))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_UNSUPPORTED_OP, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    uv_print_active_handles(&fuv_loop->loop, vfp->fp);
    FKL_CPROC_RETURN(exe, ctx, loop_obj);
    return 0;
}

static int fuv_os_environ(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uv_env_item_t *items = NULL;
    int count = 0;
    int r = uv_os_environ(&items, &count);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FklVMvalue *v = fklCreateVMvalueVec(exe, count);
    FklVMvec *vec = FKL_VM_VEC(v);
    for (int i = 0; i < count; i++)
        vec->base[i] = fklCreateVMvaluePair(
            exe, fklCreateVMvalueStrFromCstr(exe, items[i].name),
            fklCreateVMvalueStrFromCstr(exe, items[i].value));
    FKL_CPROC_RETURN(exe, ctx, v);
    uv_os_free_environ(items, count);
    return 0;
}

static int fuv_os_getenv(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *env_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(env_obj, FKL_IS_STR, exe);
    FklStringBuffer buf;
    fklInitStringBuffer(&buf);
    size_t size = buf.size;
    int r = 0;
    while ((r = uv_os_getenv(FKL_VM_STR(env_obj)->str, buf.buf, &size))
           == UV_ENOBUFS)
        fklStringBufferReverse(&buf, size + 1);
    if (r == 0) {
        buf.index = size;
        FKL_CPROC_RETURN(exe, ctx,
                         fklCreateVMvalueStr2(exe, fklStringBufferLen(&buf),
                                              fklStringBufferBody(&buf)));
        fklUninitStringBuffer(&buf);
    } else {
        fklUninitStringBuffer(&buf);
        raiseUvError(r, exe, ctx->pd);
    }
    return 0;
}

static int fuv_os_setenv(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *env_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *val_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(env_obj, FKL_IS_STR, exe);
    FKL_CHECK_TYPE(val_obj, FKL_IS_STR, exe);
    int r = uv_os_setenv(FKL_VM_STR(env_obj)->str, FKL_VM_STR(val_obj)->str);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, val_obj);
    return 0;
}

static int fuv_os_unsetenv(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *env_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(env_obj, FKL_IS_STR, exe);
    int r = uv_os_unsetenv(FKL_VM_STR(env_obj)->str);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_VM_NIL);
    return 0;
}

static int fuv_os_gethostname(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    size_t size = UV_MAXHOSTNAMESIZE;
    char hostname[UV_MAXHOSTNAMESIZE];
    int r = uv_os_gethostname(hostname, &size);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, fklCreateVMvalueStrFromCstr(exe, hostname));
    return 0;
}

static int fuv_os_getpriority(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *pid_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(pid_obj, FKL_IS_FIX, exe);
    uv_pid_t pid = FKL_GET_FIX(pid_obj);
    int priority = 0;
    int r = uv_os_getpriority(pid, &priority);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(priority));
    return 0;
}

static int fuv_os_setpriority(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 2);
    FklVMvalue *pid_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *priority_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FKL_CHECK_TYPE(pid_obj, FKL_IS_FIX, exe);
    uv_pid_t pid = FKL_GET_FIX(pid_obj);
    int priority = 0;
    if (FKL_IS_FIX(priority_obj))
        priority = FKL_GET_FIX(priority_obj);
    else if (FKL_IS_SYM(priority_obj)) {
        FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
        FklSid_t id = FKL_GET_SYM(priority_obj);
        if (id == fpd->UV_PRIORITY_LOW_sid)
            priority = UV_PRIORITY_LOW;
        else if (id == fpd->UV_PRIORITY_BELOW_NORMAL_sid)
            priority = UV_PRIORITY_BELOW_NORMAL;
        else if (id == fpd->UV_PRIORITY_NORMAL_sid)
            priority = UV_PRIORITY_NORMAL;
        else if (id == fpd->UV_PRIORITY_ABOVE_NORMAL_sid)
            priority = UV_PRIORITY_ABOVE_NORMAL;
        else if (id == fpd->UV_PRIORITY_HIGH_sid)
            priority = UV_PRIORITY_HIGH;
        else if (id == fpd->UV_PRIORITY_HIGHEST_sid)
            priority = UV_PRIORITY_HIGHEST;
        else
            FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE, exe);
    } else
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE, exe);
    int r = uv_os_setpriority(pid, priority);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, FKL_MAKE_VM_FIX(priority));
    return 0;
}

static inline FklVMvalue *timeval64_to_vmtable(FklVM *exe,
                                               const uv_timeval64_t *spec,
                                               FuvPublicData *fpd) {
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->timespec_f_sec_sid),
                      fklMakeVMint(spec->tv_sec, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->timeval_f_usec_sid),
                      fklMakeVMint(spec->tv_usec, exe));

    return hash;
}

static inline FklVMvalue *utsname_to_vmtable(FklVM *exe, uv_utsname_t *buf,
                                             FuvPublicData *fpd) {
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->utsname_sysname_sid),
                      fklCreateVMvalueStrFromCstr(exe, buf->sysname));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->utsname_release_sid),
                      fklCreateVMvalueStrFromCstr(exe, buf->release));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->utsname_version_sid),
                      fklCreateVMvalueStrFromCstr(exe, buf->version));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->utsname_machine_sid),
                      fklCreateVMvalueStrFromCstr(exe, buf->machine));
    return hash;
}

static int fuv_os_uname(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uv_utsname_t buf;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    int ret = uv_os_uname(&buf);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, utsname_to_vmtable(exe, &buf, fpd));
    return 0;
}

static int fuv_gettimeofday(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 0);
    uv_timeval64_t tv;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, ctx->pd);
    int ret = uv_gettimeofday(&tv);
    CHECK_UV_RESULT(ret, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, timeval64_to_vmtable(exe, &tv, fpd));
    return 0;
}

struct RandomCbValueCreateArg {
    int status;
    void *buf;
    size_t buflen;
};

static int fuv_random_cb_value_creator(FklVM *exe, void *a) {
    FklVMvalue *fpd_obj = ((FklCprocFrameContext *)exe->top_frame->data)->pd;
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, fpd_obj);

    struct RandomCbValueCreateArg *arg = a;
    FklVMvalue *err = arg->status < 0
                        ? createUvErrorWithFpd(arg->status, exe, fpd)
                        : FKL_VM_NIL;
    FklVMvalue *res = fklCreateVMvalueBvec2(exe, arg->buflen, arg->buf);
    FKL_VM_PUSH_VALUE(exe, err);
    FKL_VM_PUSH_VALUE(exe, res);
    return 0;
}

static void fuv_random_cb(uv_random_t *req, int status, void *buf,
                          size_t buflen) {
    struct RandomCbValueCreateArg arg = {
        .buf = buf,
        .buflen = buflen,
        .status = status,
    };
    fuv_call_req_callback_in_loop_with_value_creator(
        (uv_req_t *)req, fuv_random_cb_value_creator, &arg);
}

static int fuv_random(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM2(exe, argc, 2, 3);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FklVMvalue *len_obj = FKL_CPROC_GET_ARG(exe, ctx, 1);
    FklVMvalue *cb_obj = argc > 2 ? FKL_CPROC_GET_ARG(exe, ctx, 2) : NULL;
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_CHECK_TYPE(len_obj, fklIsVMint, exe);

    if (fklIsVMnumberLt0(len_obj))
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);

    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    uint64_t len = fklVMgetUint(len_obj);
    if (cb_obj) {
        FKL_CHECK_TYPE(cb_obj, fklIsCallable, exe);
        FklVMvalue *retval = NULL;
        struct FuvRandom *ran =
            createFuvRandom(exe, &retval, ctx->proc, loop_obj, cb_obj, len);
        int r = uv_random(&fuv_loop->loop, &ran->req, &ran->buf, len, 0,
                          fuv_random_cb);
        CHECK_UV_RESULT_AND_CLEANUP_REQ(r, retval, exe, ctx->pd);
        FKL_CPROC_RETURN(exe, ctx, retval);
    } else {
        uv_random_t req;
        FklVMvalue *v = fklCreateVMvalueBvec2(exe, len, NULL);
        FklBytevector *bvec = FKL_VM_BVEC(v);
        int r = uv_random(&fuv_loop->loop, &req, bvec->ptr, len, 0, NULL);
        CHECK_UV_RESULT(r, exe, ctx->pd);
        FKL_CPROC_RETURN(exe, ctx, v);
    }
    return 0;
}

static int fuv_sleep(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *msec_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(msec_obj, FKL_IS_FIX, exe);
    int64_t msec = FKL_GET_FIX(msec_obj);
    if (msec < 0)
        FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0, exe);
    fklUnlockThread(exe);
    uv_sleep(msec);
    fklLockThread(exe);
    FKL_CPROC_RETURN(exe, ctx, msec_obj);
    return 0;
}

static int fuv_metrics_idle_time(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    uint64_t r = uv_metrics_idle_time(&fuv_loop->loop);
    FKL_CPROC_RETURN(exe, ctx, fklMakeVMuint(r, exe));
    return 0;
}

static inline FklVMvalue *metrics_to_vmtable(FklVM *exe, uv_metrics_t *metrics,
                                             FklVMvalue *pd) {
    FKL_DECL_VM_UD_DATA(fpd, FuvPublicData, pd);
    FklVMvalue *hash = fklCreateVMvalueHashEq(exe);
    FklVMhash *ht = FKL_VM_HASH(hash);

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->metrics_loop_count_sid),
                      fklMakeVMuint(metrics->loop_count, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->metrics_events_sid),
                      fklMakeVMuint(metrics->events, exe));

    fklVMhashTableSet(ht, FKL_MAKE_VM_SYM(fpd->metrics_events_waiting_sid),
                      fklMakeVMuint(metrics->events_waiting, exe));

    return hash;
}

static int fuv_metrics_info(FKL_CPROC_ARGL) {
    FKL_CPROC_CHECK_ARG_NUM(exe, argc, 1);
    FklVMvalue *loop_obj = FKL_CPROC_GET_ARG(exe, ctx, 0);
    FKL_CHECK_TYPE(loop_obj, isFuvLoop, exe);
    FKL_DECL_VM_UD_DATA(fuv_loop, FuvLoop, loop_obj);
    uv_metrics_t metrics;
    int r = uv_metrics_info(&fuv_loop->loop, &metrics);
    CHECK_UV_RESULT(r, exe, ctx->pd);
    FKL_CPROC_RETURN(exe, ctx, metrics_to_vmtable(exe, &metrics, ctx->pd));
    return 0;
}

struct SymFunc {
    const char *sym;
    FklVMcFunc f;
} exports_and_func[] = {
    // clang-format off
    // loop
    {"loop?",                        fuv_loop_p                       },
    {"make-loop",                    fuv_make_loop                    },
    {"loop-close",                   fuv_loop_close                   },
    {"loop-run",                     fuv_loop_run                     },
    {"loop-mode",                    fuv_loop_mode                    },
    {"loop-alive?",                  fuv_loop_alive_p                 },
    {"loop-stop",                    fuv_loop_stop                    },
    {"loop-backend-fd",              fuv_loop_backend_fd              },
    {"loop-backend-timeout",         fuv_loop_backend_timeout         },
    {"loop-now",                     fuv_loop_now                     },
    {"loop-update-time",             fuv_loop_update_time             },
    {"loop-walk",                    fuv_loop_walk                    },
    {"loop-configure",               fuv_loop_configure               },

    // handle
    {"handle?",                      fuv_handle_p                     },
    {"handle-active?",               fuv_handle_active_p              },
    {"handle-closing?",              fuv_handle_closing_p             },
    {"handle-close",                 fuv_handle_close                 },
    {"handle-ref?",                  fuv_handle_ref_p                 },
    {"handle-ref",                   fuv_handle_ref                   },
    {"handle-unref",                 fuv_handle_unref                 },
    {"handle-send-buffer-size",      fuv_handle_send_buffer_size      },
    {"handle-recv-buffer-size",      fuv_handle_recv_buffer_size      },
    {"handle-fileno",                fuv_handle_fileno                },
    {"handle-type",                  fuv_handle_type                  },
    {"handle-loop",                  fuv_handle_loop                  },

    // timer
    {"timer?",                       fuv_timer_p                      },
    {"make-timer",                   fuv_make_timer                   },
    {"timer-start",                  fuv_timer_start                  },
    {"timer-stop",                   fuv_timer_stop                   },
    {"timer-again",                  fuv_timer_again                  },
    {"timer-due-in",                 fuv_timer_due_in                 },
    {"timer-repeat",                 fuv_timer_repeat                 },
    {"timer-repeat-set!",            fuv_timer_repeat_set1            },

    // prepare
    {"prepare?",                     fuv_prepare_p                    },
    {"make-prepare",                 fuv_make_prepare                 },
    {"prepare-start",                fuv_prepare_start                },
    {"prepare-stop",                 fuv_prepare_stop                 },

    // idle
    {"idle?",                        fuv_idle_p                       },
    {"make-idle",                    fuv_make_idle                    },
    {"idle-start",                   fuv_idle_start                   },
    {"idle-stop",                    fuv_idle_stop                    },

    // check
    {"check?",                       fuv_check_p                      },
    {"make-check",                   fuv_make_check                   },
    {"check-start",                  fuv_check_start                  },
    {"check-stop",                   fuv_check_stop                   },

    // signal
    {"signal?",                      fuv_signal_p                     },
    {"make-signal",                  fuv_make_signal                  },
    {"signal-start",                 fuv_signal_start                 },
    {"signal-start-oneshot",         fuv_signal_start_oneshot         },
    {"signal-stop",                  fuv_signal_stop                  },

    // async
    {"async?",                       fuv_async_p                      },
    {"make-async",                   fuv_make_async                   },
    {"async-send",                   fuv_async_send                   },

    // process
    {"process?",                     fuv_process_p                    },
    {"process-spawn",                fuv_process_spawn                },
    {"process-kill",                 fuv_process_kill                 },
    {"process-pid",                  fuv_process_pid                  },
    {"disable-stdio-inheritance",    fuv_disable_stdio_inheritance    },
    {"kill",                         fuv_kill                         },

    // stream
    {"stream?",                      fuv_stream_p                     },
    {"stream-readable?",             fuv_stream_readable_p            },
    {"stream-writable?",             fuv_stream_writable_p            },
    {"stream-shutdown",              fuv_stream_shutdown              },
    {"stream-listen",                fuv_stream_listen                },
    {"stream-accept",                fuv_stream_accept                },
    {"stream-read-start",            fuv_stream_read_start            },
    {"stream-read-stop",             fuv_stream_read_stop             },
    {"stream-write",                 fuv_stream_write                 },
    {"stream-try-write",             fuv_stream_try_write             },
    {"stream-blocking-set!",         fuv_stream_blocking_set1         },
    {"stream-write-queue-size",      fuv_stream_write_queue_size      },

    // pipe
    {"pipe?",                        fuv_pipe_p                       },
    {"make-pipe",                    fuv_make_pipe                    },
    {"pipe-open",                    fuv_pipe_open                    },
    {"pipe-bind",                    fuv_pipe_bind                    },
    {"pipe-connect",                 fuv_pipe_connect                 },
    {"pipe-sockname",                fuv_pipe_sockname                },
    {"pipe-peername",                fuv_pipe_peername                },
    {"pipe-pending-instances",       fuv_pipe_pending_instances       },
    {"pipe-pending-count",           fuv_pipe_pending_count           },
    {"pipe-pending-type",            fuv_pipe_pending_type            },
    {"pipe-chmod",                   fuv_pipe_chmod                   },
    {"pipe",                         fuv_pipe                         },

    // tcp
    {"tcp?",                         fuv_tcp_p                        },
    {"make-tcp",                     fuv_make_tcp                     },
    {"tcp-open",                     fuv_tcp_open                     },
    {"tcp-nodelay",                  fuv_tcp_nodelay                  },
    {"tcp-keepalive",                fuv_tcp_keepalive                },
    {"tcp-simultaneous-accepts",     fuv_tcp_simultaneous_accepts     },
    {"tcp-bind",                     fuv_tcp_bind                     },
    {"tcp-sockname",                 fuv_tcp_sockname                 },
    {"tcp-peername",                 fuv_tcp_peername                 },
    {"tcp-connect",                  fuv_tcp_connect                  },
    {"tcp-close-reset",              fuv_tcp_close_reset              },
    {"socketpair",                   fuv_socketpair                   },

    // tty
    {"tty?",                         fuv_tty_p                        },
    {"make-tty",                     fuv_make_tty                     },
    {"tty-mode-set!",                fuv_tty_mode_set1                },
    {"tty-mode-reset!",              fuv_tty_mode_reset1              },
    {"tty-winsize",                  fuv_tty_winsize                  },
    {"tty-vterm-state",              fuv_tty_vterm_state              },
    {"tty-vterm-state-set!",         fuv_tty_vterm_state_set1         },

    // udp
    {"udp?",                         fuv_udp_p                        },
    {"make-udp",                     fuv_make_udp                     },
    {"udp-open",                     fuv_udp_open                     },
    {"udp-bind",                     fuv_udp_bind                     },
    {"udp-connect",                  fuv_udp_connect                  },
    {"udp-peername",                 fuv_udp_peername                 },
    {"udp-sockname",                 fuv_udp_sockname                 },
    {"udp-membership-set!",          fuv_udp_membership_set1          },
    {"udp-source-membership-set!",   fuv_udp_source_membership_set1   },
    {"udp-multicast-loop-set!",      fuv_udp_multicast_loop_set1      },
    {"udp-multicast-ttl-set!",       fuv_udp_multicast_ttl_set1       },
    {"udp-multicast-interface-set!", fuv_udp_multicast_interface_set1 },
    {"udp-broadcast-set!",           fuv_udp_broadcast_set1           },
    {"udp-ttl-set!",                 fuv_udp_ttl_set1                 },
    {"udp-send",                     fuv_udp_send                     },
    {"udp-try-send",                 fuv_udp_try_send                 },
    {"udp-recv-start",               fuv_udp_recv_start               },
    {"udp-using-recvmmsg?",          fuv_udp_using_recvmmsg_p         },
    {"udp-recv-stop",                fuv_udp_recv_stop                },
    {"udp-send-queue-size",          fuv_udp_send_queue_size          },
    {"udp-send-queue-count",         fuv_udp_send_queue_count         },

    // fs-event
    {"fs-event?",                    fuv_fs_event_p                   },
    {"make-fs-event",                fuv_make_fs_event                },
    {"fs-event-start",               fuv_fs_event_start               },
    {"fs-event-stop",                fuv_fs_event_stop                },
    {"fs-event-path",                fuv_fs_event_path                },

    // fs-poll
    {"fs-poll?",                     fuv_fs_poll_p                    },
    {"make-fs-poll",                 fuv_make_fs_poll                 },
    {"fs-poll-start",                fuv_fs_poll_start                },
    {"fs-poll-stop",                 fuv_fs_poll_stop                 },
    {"fs-poll-path",                 fuv_fs_poll_path                 },

    // req
    {"req?",                         fuv_req_p                        },
    {"req-cancel",                   fuv_req_cancel                   },
    {"req-type",                     fuv_req_type                     },

    {"getaddrinfo?",                 fuv_getaddrinfo_p                },
    {"getnameinfo?",                 fuv_getnameinfo_p                },
    {"write?",                       fuv_write_p                      },
    {"shutdown?",                    fuv_shutdown_p                   },
    {"connect?",                     fuv_connect_p                    },
    {"udp-send?",                    fuv_udp_send_p                   },
    {"fs-req?",                      fuv_fs_req_p                     },
    {"random?",                      fuv_random_p                     },

    // dir
    {"dir?",                         fuv_dir_p                        },

    // fs
    {"fs-type",                      fuv_fs_type                      },
    {"fs-close",                     fuv_fs_close                     },
    {"fs-open",                      fuv_fs_open                      },
    {"fs-read",                      fuv_fs_read                      },
    {"fs-unlink",                    fuv_fs_unlink                    },
    {"fs-write",                     fuv_fs_write                     },
    {"fs-mkdir",                     fuv_fs_mkdir                     },
    {"fs-mkdtemp",                   fuv_fs_mkdtemp                   },
    {"fs-mkstemp",                   fuv_fs_mkstemp                   },
    {"fs-rmdir",                     fuv_fs_rmdir                     },
    {"fs-opendir",                   fuv_fs_opendir                   },
    {"fs-closedir",                  fuv_fs_closedir                  },
    {"fs-readdir",                   fuv_fs_readdir                   },
    {"fs-scandir",                   fuv_fs_scandir                   },
    {"fs-scandir-next",              fuv_fs_scandir_next              },
    {"fs-stat",                      fuv_fs_stat                      },
    {"fs-fstat",                     fuv_fs_fstat                     },
    {"fs-lstat",                     fuv_fs_lstat                     },
    {"fs-statfs",                    fuv_fs_statfs                    },
    {"fs-rename",                    fuv_fs_rename                    },
    {"fs-fsync",                     fuv_fs_fsync                     },
    {"fs-fdatasync",                 fuv_fs_fdatasync                 },
    {"fs-ftruncate",                 fuv_fs_ftruncate                 },
    {"fs-copyfile",                  fuv_fs_copyfile                  },
    {"fs-sendfile",                  fuv_fs_sendfile                  },
    {"fs-access",                    fuv_fs_access                    },
    {"fs-chmod",                     fuv_fs_chmod                     },
    {"fs-fchmod",                    fuv_fs_fchmod                    },
    {"fs-utime",                     fuv_fs_utime                     },
    {"fs-futime",                    fuv_fs_futime                    },
    {"fs-lutime",                    fuv_fs_lutime                    },
    {"fs-link",                      fuv_fs_link                      },
    {"fs-symlink",                   fuv_fs_symlink                   },
    {"fs-readlink",                  fuv_fs_readlink                  },
    {"fs-realpath",                  fuv_fs_realpath                  },
    {"fs-chown",                     fuv_fs_chown                     },
    {"fs-fchown",                    fuv_fs_fchown                    },
    {"fs-lchown",                    fuv_fs_lchown                    },

    // dns
    {"getaddrinfo",                  fuv_getaddrinfo                  },
    {"getnameinfo",                  fuv_getnameinfo                  },

    // misc
    {"guess-handle",                 fuv_guess_handle                 },
    {"get-process-title",            fuv_get_process_title            },
    {"set-process-title",            fuv_set_process_title            },
    {"resident-set-memory",          fuv_resident_set_memory          },
    {"uptime",                       fuv_uptime                       },
    {"getrusage",                    fuv_getrusage                    },
    {"getrusage-thread",             fuv_getrusage_thread             },
    {"os-getpid",                    fuv_os_getpid                    },
    {"os-getppid",                   fuv_os_getppid                   },
    {"available-parallelism",        fuv_available_parallelism        },
    {"cpu-info",                     fuv_cpu_info                     },
    {"cpumask-size",                 fuv_cpumask_size                 },
    {"interface-addresses",          fuv_interface_address            },
    {"loadavg",                      fuv_loadavg                      },
    {"if-indextoname",               fuv_if_indextoname               },
    {"if-indextoiid",                fuv_if_indextoiid                },
    {"exepath",                      fuv_exepath                      },
    {"cwd",                          fuv_cwd                          },
    {"chdir",                        fuv_chdir                        },
    {"os-homedir",                   fuv_os_homedir                   },
    {"os-tmpdir",                    fuv_os_tmpdir                    },
    {"os-get-passwd",                fuv_os_get_passwd                },
    {"get-free-memory",              fuv_get_free_memory              },
    {"get-total-memory",             fuv_get_total_memory             },
    {"get-constrained-memory",       fuv_get_constrained_memory       },
    {"get-available-memory",         fuv_get_available_memory         },
    {"hrtime",                       fuv_hrtime                       },
    {"clock-gettime",                fuv_clock_gettime                },
    {"print-all-handles",            fuv_print_all_handles            },
    {"print-active-handles",         fuv_print_active_handles         },
    {"os-environ",                   fuv_os_environ                   },
    {"os-getenv",                    fuv_os_getenv                    },
    {"os-setenv",                    fuv_os_setenv                    },
    {"os-unsetenv",                  fuv_os_unsetenv                  },
    {"os-gethostname",               fuv_os_gethostname               },
    {"os-getpriority",               fuv_os_getpriority               },
    {"os-setpriority",               fuv_os_setpriority               },
    {"os-uname",                     fuv_os_uname                     },
    {"gettimeofday",                 fuv_gettimeofday                 },
    {"random",                       fuv_random                       },
    {"sleep",                        fuv_sleep                        },

    // metrics
    {"metrics-idle-time",            fuv_metrics_idle_time            },
    {"metrics-info",                 fuv_metrics_info                 },
    // clang-format on
};

static const size_t EXPORT_NUM =
    sizeof(exports_and_func) / sizeof(struct SymFunc);

FKL_DLL_EXPORT FklSid_t *
_fklExportSymbolInit(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS) {
    *num = EXPORT_NUM;
    FklSid_t *symbols = (FklSid_t *)malloc(sizeof(FklSid_t) * EXPORT_NUM);
    FKL_ASSERT(symbols);
    for (size_t i = 0; i < EXPORT_NUM; i++)
        symbols[i] = fklAddSymbolCstr(exports_and_func[i].sym, st)->v;
    return symbols;
}

FKL_DLL_EXPORT FklVMvalue **_fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS) {
    *count = EXPORT_NUM;
    FklVMvalue **loc = (FklVMvalue **)malloc(sizeof(FklVMvalue *) * EXPORT_NUM);
    FKL_ASSERT(loc);

    FklVMvalue *fpd = fklCreateVMvalueUd(exe, &FuvPublicDataMetaTable, dll);

    FKL_DECL_VM_UD_DATA(pd, FuvPublicData, fpd);

    fklVMacquireSt(exe->gc);
    FklSymbolTable *st = exe->gc->st;
    init_fuv_public_data(pd, st);
    for (size_t i = 0; i < EXPORT_NUM; i++) {
        FklSid_t id = fklAddSymbolCstr(exports_and_func[i].sym, st)->v;
        FklVMcFunc func = exports_and_func[i].f;
        FklVMvalue *dlproc = fklCreateVMvalueCproc(exe, func, dll, fpd, id);
        loc[i] = dlproc;
    }
    fklVMreleaseSt(exe->gc);
    return loc;
}
