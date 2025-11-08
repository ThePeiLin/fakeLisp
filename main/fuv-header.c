#include <fakeLisp/cb_helper.h>
#include <fakeLisp/code_builder.h>

#include <argtable3.h>
#include <uv.h>

#include <signal.h>
#include <stdlib.h>

#define COUNT(A) (sizeof(A) / sizeof(struct S))

struct S {
    const char *var_name;
    const char *sym_name;
    const char *desc;
};

static inline void build_str_in_hex(const char *s, FklCodeBuilder *build) {
    CB_FMT("\"");
    for (; *s; ++s) {
        CB_FMT("\\x%x", *s);
    }
    CB_FMT("\"");
}

static inline void build_xx(struct S const cur[],
        size_t count,
        FklCodeBuilder *build,
        int last_backslash) {
    struct S const *const end = &cur[count];
    for (; cur < end - 1; ++cur) {
        CB_LINE_START("XX(%s, ", cur->var_name);
        build_str_in_hex(cur->sym_name, build);
        CB_FMT(", ");
        if (cur->desc)
            build_str_in_hex(cur->desc, build);
        else
            CB_FMT("\"\"");
        CB_LINE_END(") \\");
    }

    CB_LINE_START("XX(%s, ", cur->var_name);
    build_str_in_hex(cur->sym_name, build);
    CB_FMT(", ");
    if (cur->desc)
        build_str_in_hex(cur->desc, build);
    else
        CB_FMT("\"\"");
    if (last_backslash)
        CB_LINE_END(") \\");
    else
        CB_LINE_END(")");
}

static inline void build_map_macro(const char *name,
        struct S const symbols[],
        size_t count,
        FklCodeBuilder *build) {
    CB_LINE("#define %s(XX) \\", name);
    CB_INDENT(flag) { build_xx(symbols, count, build, 0); }
}

#define BUILD_MAP_MACRO(NAME, SYMBOLS, BUILD)                                  \
    build_map_macro(#NAME, SYMBOLS, COUNT(SYMBOLS), BUILD)

#define CONCAT_(A, B) A##B
#define CONCAT(A, B) CONCAT_(A, B)
#define STRINGIFY(A) #A

static struct S const uv_errno[] = {
#define XX(code, desc) { "UV_" #code, "UV_" #code, desc },
    UV_ERRNO_MAP(XX)
#undef XX
};

static struct S const ai_flags[] = {
    // clang-format off
    { "AI_ADDRCONFIG",  "addrconfig",  NULL },
    { "AI_NUMERICHOST", "numerichost", NULL },
    { "AI_PASSIVE",     "passive",     NULL },
    { "AI_NUMERICSERV", "numericserv", NULL },
    { "AI_CANONNAME",   "canonname",   NULL },
#ifdef AI_V4MAPPED
    { "AI_V4MAPPED",    "v4mapped",    NULL },
#endif
#ifdef AI_ALL
    { "AI_ALL",         "all",         NULL },
#endif
    // clang-format on
};

static struct S const signals[] = {
#ifdef SIGHUP
    // clang-format off
    { "SIGHUP",    "sighup",    NULL },
#endif
#ifdef SIGINT
    { "SIGINT",    "sigint",    NULL },
#endif
#ifdef SIGQUIT
    { "SIGQUIT",   "sigquit",   NULL },
#endif
#ifdef SIGILL
    { "SIGILL",    "sigill",    NULL },
#endif
#ifdef SIGTRAP
    { "SIGTRAP",   "sigtrap",   NULL },
#endif
#ifdef SIGABRT
    { "SIGABRT",   "sigabrt",   NULL },
#endif
#ifdef SIGIOT
    { "SIGIOT",    "sigiot",    NULL },
#endif
#ifdef SIGBUS
    { "SIGBUS",    "sigbus",    NULL },
#endif
#ifdef SIGFPE
    { "SIGFPE",    "sigfpe",    NULL },
#endif
#ifdef SIGKILL
    { "SIGKILL",   "sigkill",   NULL },
#endif
#ifdef SIGUSR1
    { "SIGUSR1",   "sigusr1",   NULL },
#endif
#ifdef SIGSEGV
    { "SIGSEGV",   "sigsegv",   NULL },
#endif
#ifdef SIGUSR2
    { "SIGUSR2",   "sigusr2",   NULL },
#endif
#ifdef SIGPIPE
    { "SIGPIPE",   "sigpipe",   NULL },
#endif
#ifdef SIGALRM
    { "SIGALRM",   "sigalrm",   NULL },
#endif
#ifdef SIGTERM
    { "SIGTERM",   "sigterm",   NULL },
#endif
#ifdef SIGCHLD
    { "SIGCHLD",   "sigchld",   NULL },
#endif
#ifdef SIGSTKFLT
    { "SIGSTKFLT", "sigstkflt", NULL },
#endif
#ifdef SIGCONT
    { "SIGCONT",   "sigcont",   NULL },
#endif
#ifdef SIGSTOP
    { "SIGSTOP",   "sigstop",   NULL },
#endif
#ifdef SIGTSTP
    { "SIGTSTP",   "sigtstp",   NULL },
#endif
#ifdef SIGBREAK
    { "SIGBREAK",  "sigbreak",  NULL },
#endif
#ifdef SIGTTIN
    { "SIGTTIN",   "sigttin",   NULL },
#endif
#ifdef SIGTTOU
    { "SIGTTOU",   "sigttou",   NULL },
#endif
#ifdef SIGURG
    { "SIGURG",    "sigurg",    NULL },
#endif
#ifdef SIGXCPU
    { "SIGXCPU",   "sigxcpu",   NULL },
#endif
#ifdef SIGXFSZ
    { "SIGXFSZ",   "sigxfsz",   NULL },
#endif
#ifdef SIGVTALRM
    { "SIGVTALRM", "sigvtalrm", NULL },
#endif
#ifdef SIGPROF
    { "SIGPROF",   "sigprof",   NULL },
#endif
#ifdef SIGWINCH
    { "SIGWINCH",  "sigwinch",  NULL },
#endif
#ifdef SIGIO
    { "SIGIO",     "sigio",     NULL },
#endif
#ifdef SIGPOLL
    { "SIGPOLL",   "sigpoll",   NULL },
#endif
#ifdef SIGLOST
    { "SIGLOST",   "siglost",   NULL },
#endif
#ifdef SIGPWR
    { "SIGPWR",    "sigpwr",    NULL },
#endif
#ifdef SIGSYS
    { "SIGSYS",    "sigsys",    NULL },
#endif
    // clang-format on
};

static struct S const address_families[] = {
#ifdef AF_UNSPEC
    // clang-format off
    { "AF_UNSPEC",    "unspec",    NULL },
#endif
#ifdef AF_UNIX
    { "AF_UNIX",      "unix",      NULL },
#endif
#ifdef AF_INET
    { "AF_INET",      "inet",      NULL },
#endif
#ifdef AF_INET6
    { "AF_INET6",     "inet6",     NULL },
#endif
#ifdef AF_IPX
    { "AF_IPX",       "ipx",       NULL },
#endif
#ifdef AF_NETLINK
    { "AF_NETLINK",   "netlink",   NULL },
#endif
#ifdef AF_X25
    { "AF_X25",       "x25",       NULL },
#endif
#ifdef AF_AX25
    { "AF_AX25",      "ax25",      NULL },
#endif
#ifdef AF_ATMPVC
    { "AF_ATMPVC",    "atmpvc",    NULL },
#endif
#ifdef AF_APPLETALK
    { "AF_APPLETALK", "appletalk", NULL },
#endif
#ifdef AF_PACKET
    { "AF_PACKET",    "packet",    NULL },
#endif
    // clang-format on
};

static struct S const socket_types[] = {
#ifdef SOCK_STREAM
    // clang-format off
	{"SOCK_STREAM",    "stream",    NULL},
#endif
#ifdef SOCK_DGRAM
	{"SOCK_DGRAM",     "dgram",     NULL},
#endif
#ifdef SOCK_SEQPACKET
	{"SOCK_SEQPACKET", "seqpacket", NULL},
#endif
#ifdef SOCK_RAW
	{"SOCK_RAW",       "raw",       NULL},
#endif
#ifdef SOCK_RDM
	{"SOCK_RDM",       "rdm",       NULL},
#endif
    // clang-format on
};

static struct S const ni_flags[] = {
#ifdef NI_NAMEREQD
    // clang-format off
	{"NI_NAMEREQD",                 "namereqd",                 NULL},
#endif
#ifdef NI_DGRAM
	{"NI_DGRAM",                    "dgram",                    NULL},
#endif
#ifdef NI_NOFQDN
	{"NI_NOFQDN",                   "nofqdn",                   NULL},
#endif
#ifdef NI_NUMERICHOST
	{"NI_NUMERICHOST",              "numerichost",              NULL},
#endif
#ifdef NI_NUMERICSERV
	{"NI_NUMERICSERV",              "numericserv",              NULL},
#endif
#ifdef NI_IDN
	{"NI_IDN",                      "idn",                      NULL},
#endif
#ifdef NI_IDN_ALLOW_UNASSIGNED
	{"NI_IDN_ALLOW_UNASSIGNED",     "idn-allow-unassigned",     NULL},
#endif
#ifdef NI_IDN_USE_STD3_ASCII_RULES
	{"NI_IDN_USE_STD3_ASCII_RULES", "idn-use-std3-ascii-rules", NULL},
#endif
    // clang-format on
};

static struct S const loop_run_modes[] = {
    // clang-format off
    { "UV_RUN_DEFAULT", "default", NULL},
    { "UV_RUN_ONCE",    "once",    NULL},
    { "UV_RUN_NOWAIT",  "nowait",  NULL},
    // clang-format on
};

static struct S const fuv_errors[] = {
    // clang-format off
    { "FUV_ERR_CLOSE_CLOSEING_HANDLE",     "fuv-handle-error", "close closing handle"             },
    { "FUV_ERR_HANDLE_CLOSED",             "fuv-handle-error", "handle has been closed"           },
    { "FUV_ERR_REQ_CANCELED",              "fuv-req-error",    "request has been canceled"        },
    { "FUV_ERR_CLOSE_USING_DIR",           "fuv-dir-error",    "can't close a using dir"          },
    { "FUV_ERR_NUMBER_SHOULD_NOT_BE_LT_1", "value-error",      "Number should not be less than 1" },
    // clang-format on
};

static struct S const loop_configures[] = {
    // clang-format off
    { "UV_LOOP_BLOCK_SIGNAL",        "loop-block-signal",        NULL },
    { "UV_METRICS_IDLE_TIME",        "metrics-idle-time",        NULL },
    { "UV_LOOP_USE_IO_URING_SQPOLL", "loop-use-io-uring-sqpoll", NULL },
    // clang-format on
};

static struct S const priorities[] = {
    // clang-format off
    { "UV_PRIORITY_LOW",          "low",          NULL },
    { "UV_PRIORITY_BELOW_NORMAL", "below-normal", NULL },
    { "UV_PRIORITY_NORMAL",       "normal",       NULL },
    { "UV_PRIORITY_ABOVE_NORMAL", "above-normal", NULL },
    { "UV_PRIORITY_HIGH",         "high",         NULL },
    { "UV_PRIORITY_HIGHEST",      "highest",      NULL },
    // clang-format on
};

static struct S const fs_o_flags[] = {
    // clang-format off
    { "UV_FS_O_APPEND",      "append",      NULL },
    { "UV_FS_O_CREAT",       "creat",       NULL },
    { "UV_FS_O_EXCL",        "excl",        NULL },
    { "UV_FS_O_FILEMAP",     "filemap",     NULL },
    { "UV_FS_O_RANDOM",      "random",      NULL },
    { "UV_FS_O_RDONLY",      "rdonly",      NULL },
    { "UV_FS_O_RDWR",        "rdwr",        NULL },
    { "UV_FS_O_SEQUENTIAL",  "sequential",  NULL },
    { "UV_FS_O_SHORT_LIVED", "short-lived", NULL },
    { "UV_FS_O_TEMPORARY",   "temporary",   NULL },
    { "UV_FS_O_TRUNC",       "trunc",       NULL },
    { "UV_FS_O_WRONLY",      "wronly",      NULL },
    { "UV_FS_O_DIRECT",      "direct",      NULL },
    { "UV_FS_O_DIRECTORY",   "directory",   NULL },
    { "UV_FS_O_DSYNC",       "dsync",       NULL },
    { "UV_FS_O_EXLOCK",      "exlock",      NULL },
    { "UV_FS_O_NOATIME",     "noatime",     NULL },
    { "UV_FS_O_NOCTTY",      "nocity",      NULL },
    { "UV_FS_O_NOFOLLOW",    "nofollow",    NULL },
    { "UV_FS_O_NONBLOCK",    "nonblock",    NULL },
    { "UV_FS_O_SYMLINK",     "symlink",     NULL },
    { "UV_FS_O_SYNC",        "sync",        NULL },
    // clang-format on
};

static struct S const udp_flags[] = {
    // clang-format off
    { "UV_UDP_IPV6ONLY",      "ipv6only",      NULL },
    { "UV_UDP_PARTIAL",       "partial",       NULL },
    { "UV_UDP_REUSEADDR",     "reuseaddr",     NULL },
    { "UV_UDP_MMSG_CHUNK",    "mmsg-chunk",    NULL },
    { "UV_UDP_MMSG_FREE",     "mmsg-free",     NULL },
    { "UV_UDP_LINUX_RECVERR", "linux-recverr", NULL },
    { "UV_UDP_RECVMMSG",      "recvmmsg",      NULL },
    // clang-format on
};

static struct S const process_flags[] = {
    // clang-format off
    { "UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS", "verbatim",     NULL },
    { "UV_PROCESS_DETACHED",                   "detached",     NULL },
    { "UV_PROCESS_WINDOWS_HIDE",               "hide",         NULL },
    { "UV_PROCESS_WINDOWS_HIDE_CONSOLE",       "hide-console", NULL },
    { "UV_PROCESS_WINDOWS_HIDE_GUI",           "hide-gui",     NULL },
    // clang-format on
};

static struct S const fs_events[] = {
    // clang-format off
    { "UV_FS_EVENT_WATCH_ENTRY", "watch-entry", NULL },
    { "UV_FS_EVENT_STAT", "stat", NULL },
    { "UV_FS_EVENT_RECURSIVE", "recursive", NULL },
    // clang-format on

};

static struct S const dirent_types[] = {
    // clang-format off
    { "UV_DIRENT_UNKNOWN", "unknown", NULL },
    { "UV_DIRENT_FILE",    "file",    NULL },
    { "UV_DIRENT_DIR",     "dir",     NULL },
    { "UV_DIRENT_LINK",    "link",    NULL },
    { "UV_DIRENT_FIFO",    "fifo",    NULL },
    { "UV_DIRENT_SOCKET",  "socket",  NULL },
    { "UV_DIRENT_CHAR",    "char",    NULL },
    { "UV_DIRENT_BLOCK",   "block",   NULL },
    // clang-format on
};

static struct S const poll_events[] = {
    // clang-format off
    { "UV_READABLE",    "readable",    NULL },
    { "UV_WRITABLE",    "writable",    NULL },
    { "UV_DISCONNECT",  "disconnect",  NULL },
    { "UV_PRIORITIZED", "prioritized", NULL },
    // clang-format on
};

static struct S const fuv_symbols[] = {
    // clang-format off
    { "UV_TTY_MODE_NORMAL",                    "normal",                   NULL },
    { "UV_TTY_MODE_RAW",                       "raw",                      NULL },
    { "UV_TTY_MODE_IO",                        "io",                       NULL },

    { "UV_TTY_SUPPORTED",                      "supported",                NULL },
    { "UV_TTY_UNSUPPORTED",                    "unsupported",              NULL },

    { "UV_LEAVE_GROUP",                        "leave",                    NULL },
    { "UV_JOIN_GROUP",                         "join",                     NULL },

    { "UV_FS_SYMLINK_DIR",                     "dir",                      NULL },
    { "UV_FS_SYMLINK_JUNCTION",                "junction",                 NULL },
    { "UV_FS_COPYFILE_EXCL",                   "excl",                     NULL },
    { "UV_FS_COPYFILE_FICLONE",                "ficlone",                  NULL },
    { "UV_FS_COPYFILE_FICLONE_FORCE",          "ficlone-force",            NULL },

    { "UV_RENAME",                             "rename",                   NULL },
    { "UV_CHANGE",                             "change",                   NULL },

    { "UV_CLOCK_MONOTONIC",                    "monotonic",                NULL },
    { "UV_CLOCK_REALTIME",                     "realtime",                 NULL },

    { "f_ip",                                  "ip",                       NULL },
    { "f_addr",                                "addr",                     NULL },
    { "f_port",                                "port",                     NULL },
    { "f_family",                              "family",                   NULL },
    { "f_socktype",                            "socktype",                 NULL },
    { "f_protocol",                            "protocol",                 NULL },
    { "f_canonname",                           "canonname",                NULL },
    { "f_hostname",                            "hostname",                 NULL },
    { "f_service",                             "service",                  NULL },
    { "f_args",                                "args",                     NULL },
    { "f_env",                                 "env",                      NULL },
    { "f_cwd",                                 "cwd",                      NULL },
    { "f_stdio",                               "stdio",                    NULL },
    { "f_uid",                                 "uid",                      NULL },
    { "f_gid",                                 "gid",                      NULL },

    { "stat_f_dev",                            "dev",                      NULL },
    { "stat_f_mode",                           "mode",                     NULL },
    { "stat_f_nlink",                          "nlink",                    NULL },
    { "stat_f_uid",                            "uid",                      NULL },
    { "stat_f_gid",                            "gid",                      NULL },
    { "stat_f_rdev",                           "rdev",                     NULL },
    { "stat_f_ino",                            "ino",                      NULL },
    { "stat_f_size",                           "size",                     NULL },
    { "stat_f_blksize",                        "blksize",                  NULL },
    { "stat_f_blocks",                         "blocks",                   NULL },
    { "stat_f_flags",                          "flags",                    NULL },
    { "stat_f_gen",                            "gen",                      NULL },
    { "stat_f_atime",                          "atime",                    NULL },
    { "stat_f_mtime",                          "mtime",                    NULL },
    { "stat_f_ctime",                          "ctime",                    NULL },
    { "stat_f_birthtime",                      "birthtime",                NULL },
    { "stat_f_type",                           "type",                     NULL },

    { "stat_type_file",                        "file",                     NULL },
    { "stat_type_directory",                   "directory",                NULL },
    { "stat_type_link",                        "link",                     NULL },
    { "stat_type_fifo",                        "fifo",                     NULL },
    { "stat_type_socket",                      "socket",                   NULL },
    { "stat_type_char",                        "char",                     NULL },
    { "stat_type_block",                       "block",                    NULL },
    { "stat_type_unknown",                     "unknown",                  NULL },

    { "time_f_sec",                            "sec",                      NULL },
    { "timespec_f_nsec",                       "nsec",                     NULL },
    { "timeval_f_usec",                        "usec",                     NULL },


	{ "statfs_f_type",                         "type",                     NULL },
    { "statfs_f_bsize",                        "bsize",                    NULL },
    { "statfs_f_blocks",                       "blocks",                   NULL },
    { "statfs_f_bfree",                        "bfree",                    NULL },
    { "statfs_f_bavail",                       "bavail",                   NULL },
    { "statfs_f_files",                        "files",                    NULL },
    { "statfs_f_ffree",                        "ffree",                    NULL },

    { "dirent_f_type",                         "type",                     NULL },
    { "dirent_f_name",                         "name",                     NULL },

    { "utsname_sysname",                       "sysname",                  NULL },
    { "utsname_version",                       "version",                  NULL },
    { "utsname_release",                       "release",                  NULL },
    { "utsname_machine",                       "machine",                  NULL },

    { "rusage_utime",                          "utime",                    NULL },
    { "rusage_stime",                          "stime",                    NULL },
    { "rusage_maxrss",                         "maxrss",                   NULL },
    { "rusage_ixrss",                          "ixrss",                    NULL },
    { "rusage_idrss",                          "idrss",                    NULL },
    { "rusage_isrss",                          "isrss",                    NULL },
    { "rusage_minflt",                         "minflt",                   NULL },
    { "rusage_majflt",                         "majflt",                   NULL },
    { "rusage_nswap",                          "nswap",                    NULL },
    { "rusage_inblock",                        "inblock",                  NULL },
    { "rusage_oublock",                        "oublock",                  NULL },
    { "rusage_msgsnd",                         "msgsnd",                   NULL },
    { "rusage_msgrcv",                         "msgrcv",                   NULL },
    { "rusage_nsignals",                       "nsignals",                 NULL },
    { "rusage_nvcsw",                          "nvcsw",                    NULL },
    { "rusage_nivcsw",                         "nivcsw",                   NULL },

    { "cpu_info_model",                        "model",                    NULL },
    { "cpu_info_speed",                        "speed",                    NULL },
    { "cpu_info_times",                        "times",                    NULL },
    { "cpu_info_times_user",                   "user",                     NULL },
    { "cpu_info_times_nice",                   "nice",                     NULL },
    { "cpu_info_times_sys",                    "sys",                      NULL },
    { "cpu_info_times_idle",                   "idle",                     NULL },
    { "cpu_info_times_irq",                    "irq",                      NULL },

    { "passwd_username",                       "username",                 NULL },
    { "passwd_uid",                            "uid",                      NULL },
    { "passwd_gid",                            "gid",                      NULL },
    { "passwd_shell",                          "shell",                    NULL },
    { "passwd_homedir",                        "homedir",                  NULL },

    { "ifa_f_name",                            "name",                     NULL },
    { "ifa_f_mac",                             "mac",                      NULL },
    { "ifa_f_internal",                        "internal",                 NULL },
    { "ifa_f_ip",                              "ip",                       NULL },
    { "ifa_f_netmask",                         "netmask",                  NULL },
    { "ifa_f_family",                          "family",                   NULL },

    { "metrics_loop_count",                    "loop-count",               NULL },
    { "metrics_events",                        "events",                   NULL },
    { "metrics_events_waiting",                "events-waiting",           NULL },
    // clang-format on
};

static inline void build_all_symbols(FklCodeBuilder *build) {
    CB_LINE("#define %s(XX) \\", "FUV_SYMBOLS_MAP");
    CB_INDENT(flag) {
        build_xx(fuv_symbols, COUNT(fuv_symbols), build, 1);
        build_xx(fuv_errors, COUNT(fuv_errors), build, 1);
        build_xx(loop_configures, COUNT(loop_configures), build, 1);
        build_xx(loop_run_modes, COUNT(loop_run_modes), build, 1);
        build_xx(fs_o_flags, COUNT(fs_o_flags), build, 1);
        build_xx(fs_events, COUNT(fs_events), build, 1);
        build_xx(dirent_types, COUNT(dirent_types), build, 1);
        build_xx(poll_events, COUNT(poll_events), build, 1);
        build_xx(udp_flags, COUNT(udp_flags), build, 1);
        build_xx(process_flags, COUNT(process_flags), build, 1);
        build_xx(priorities, COUNT(priorities), build, 1);
        build_xx(uv_errno, COUNT(uv_errno), build, 1);
        build_xx(ai_flags, COUNT(ai_flags), build, 1);
        build_xx(signals, COUNT(signals), build, 1);
        build_xx(address_families, COUNT(address_families), build, 1);
        build_xx(socket_types, COUNT(socket_types), build, 1);
        build_xx(ni_flags, COUNT(ni_flags), build, 0);
    }
}

struct arg_lit *help;
struct arg_file *output;
struct arg_end *end;

int main(int argc, char *argv[]) {
    const char *progname = argv[0];
    int exitcode = 0;

    void *argtable[] = {
        help = arg_lit0("h", "help", "display this help and exit"),
        output = arg_file1("o", NULL, "<file-name>", "output file name"),
        end = arg_end(20),
    };

    FILE *output_file = NULL;
    int nerrors = arg_parse(argc, argv, argtable);
    FklCodeBuilder builder = { 0 };

    if (help->count > 0) {
        printf("Usage: %s", progname);
        arg_print_syntaxv(stdout, argtable, "\n");
        printf("generate macros for fuv.\n\n");
        arg_print_glossary_gnu(stdout, argtable);
        goto exit;
    }

    if (nerrors) {
        arg_print_errors(stdout, end, progname);
        printf("Try '%s --help' for more informaction.\n", progname);
        exitcode = EXIT_FAILURE;
        goto exit;
    }

    output_file = fopen(output->filename[0], "w");
    if (output_file == NULL) {
        perror(output->filename[0]);
        exitcode = EXIT_FAILURE;
        goto exit;
    }

    fklInitCodeBuilderFp(&builder, output_file, NULL);

    FklCodeBuilder *build = &builder;

    BUILD_MAP_MACRO(FUV_UV_LOOP_MODE_MAP, loop_run_modes, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_UV_LOOP_CONF_MAP, loop_configures, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_UV_UDP_MAP, udp_flags, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_UV_PRIORITY_MAP, priorities, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_UV_FS_O_MAP, fs_o_flags, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_UV_FS_EVENT_MAP, fs_events, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_UV_DIRENT_TYPE_MAP, dirent_types, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_UV_POLL_EVENT_MAP, poll_events, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_UV_PROCESS_MAP, process_flags, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_ERROR_MAP, fuv_errors, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_UV_ERRNO_MAP, uv_errno, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_AI_FLAGS_MAP, ai_flags, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_SIGNAL_MAP, signals, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_AF_MAP, address_families, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_SOCKET_TYPES_MAP, socket_types, build);
    CB_LINE("");

    BUILD_MAP_MACRO(FUV_NI_FLAGS_MAP, ni_flags, build);
    CB_LINE("");

    build_all_symbols(build);
exit:

    if (output_file)
        fclose(output_file);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return exitcode;
}
