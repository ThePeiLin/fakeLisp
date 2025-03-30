#include "fuv.h"

FklSid_t uvErrToSid(int err_id, FuvPublicData *pd) {
    FklSid_t id = 0;
    switch (err_id) {
#define XX(code, _)                                                            \
    case UV_##code:                                                            \
        id = pd->uv_err_sid_##code;                                            \
        break;
        UV_ERRNO_MAP(XX);
    default:
        id = pd->uv_err_sid_UNKNOWN;
    }
    return id;
}

static inline FklVMvalue *create_uv_error(int err_id, FklVM *exe,
                                          FuvPublicData *pd) {
    FklSid_t id = 0;
    switch (err_id) {
#define XX(code, _)                                                            \
    case UV_##code:                                                            \
        id = pd->uv_err_sid_##code;                                            \
        break;
        UV_ERRNO_MAP(XX);
    default:
        id = pd->uv_err_sid_UNKNOWN;
    }
    return fklCreateVMvalueError(exe, id,
                                 fklCreateStringFromCstr(uv_strerror(err_id)));
}

void raiseUvError(int err_id, FklVM *exe, FklVMvalue *pd_obj) {
    FKL_DECL_VM_UD_DATA(pd, FuvPublicData, pd_obj);
    fklRaiseVMerror(create_uv_error(err_id, exe, pd), exe);
}

FklVMvalue *createUvErrorWithFpd(int err_id, FklVM *exe, FuvPublicData *fpd) {
    return create_uv_error(err_id, exe, fpd);
}

FklVMvalue *createUvError(int err_id, FklVM *exe, FklVMvalue *pd_obj) {
    FKL_DECL_VM_UD_DATA(pd, FuvPublicData, pd_obj);
    return create_uv_error(err_id, exe, pd);
}

void raiseFuvError(FuvErrorType err, FklVM *exe, FklVMvalue *pd_obj) {
    FKL_DECL_VM_UD_DATA(pd, FuvPublicData, pd_obj);
    FklSid_t sid = pd->fuv_err_sid[err];
    static const char *fuv_err_msg[] = {
        NULL,
        "close closing handle",
        "handle has been closed",
        "request has been canceled",
        "can't close a using dir",
        "Number should not be less than 1",
    };
    FklVMvalue *ev = fklCreateVMvalueError(
        exe, sid, fklCreateStringFromCstr(fuv_err_msg[err]));
    fklRaiseVMerror(ev, exe);
}

int symbolToSignum(FklSid_t id, FuvPublicData *pd) {
#ifdef SIGHUP
    if (id == pd->SIGHUP_sid)
        return SIGHUP;
#endif
#ifdef SIGINT
    if (id == pd->SIGINT_sid)
        return SIGINT;
#endif
#ifdef SIGQUIT
    if (id == pd->SIGQUIT_sid)
        return SIGQUIT;
#endif
#ifdef SIGILL
    if (id == pd->SIGILL_sid)
        return SIGILL;
#endif
#ifdef SIGTRAP
    if (id == pd->SIGTRAP_sid)
        return SIGTRAP;
#endif
#ifdef SIGABRT
    if (id == pd->SIGABRT_sid)
        return SIGABRT;
#endif
#ifdef SIGIOT
    if (id == pd->SIGIOT_sid)
        return SIGIOT;
#endif
#ifdef SIGBUS
    if (id == pd->SIGBUS_sid)
        return SIGBUS;
#endif
#ifdef SIGFPE
    if (id == pd->SIGFPE_sid)
        return SIGFPE;
#endif
#ifdef SIGKILL
    if (id == pd->SIGKILL_sid)
        return SIGKILL;
#endif
#ifdef SIGUSR1
    if (id == pd->SIGUSR1_sid)
        return SIGUSR1;
#endif
#ifdef SIGSEGV
    if (id == pd->SIGSEGV_sid)
        return SIGSEGV;
#endif
#ifdef SIGUSR2
    if (id == pd->SIGUSR2_sid)
        return SIGUSR2;
#endif
#ifdef SIGPIPE
    if (id == pd->SIGPIPE_sid)
        return SIGPIPE;
#endif
#ifdef SIGALRM
    if (id == pd->SIGALRM_sid)
        return SIGALRM;
#endif
#ifdef SIGTERM
    if (id == pd->SIGTERM_sid)
        return SIGTERM;
#endif
#ifdef SIGCHLD
    if (id == pd->SIGCHLD_sid)
        return SIGCHLD;
#endif
#ifdef SIGSTKFLT
    if (id == pd->SIGSTKFLT_sid)
        return SIGSTKFLT;
#endif
#ifdef SIGCONT
    if (id == pd->SIGCONT_sid)
        return SIGCONT;
#endif
#ifdef SIGSTOP
    if (id == pd->SIGSTOP_sid)
        return SIGSTOP;
#endif
#ifdef SIGTSTP
    if (id == pd->SIGTSTP_sid)
        return SIGTSTP;
#endif
#ifdef SIGBREAK
    if (id == pd->SIGBREAK_sid)
        return SIGBREAK;
#endif
#ifdef SIGTTIN
    if (id == pd->SIGTTIN_sid)
        return SIGTTIN;
#endif
#ifdef SIGTTOU
    if (id == pd->SIGTTOU_sid)
        return SIGTTOU;
#endif
#ifdef SIGURG
    if (id == pd->SIGURG_sid)
        return SIGURG;
#endif
#ifdef SIGXCPU
    if (id == pd->SIGXCPU_sid)
        return SIGXCPU;
#endif
#ifdef SIGXFSZ
    if (id == pd->SIGXFSZ_sid)
        return SIGXFSZ;
#endif
#ifdef SIGVTALRM
    if (id == pd->SIGVTALRM_sid)
        return SIGVTALRM;
#endif
#ifdef SIGPROF
    if (id == pd->SIGPROF_sid)
        return SIGPROF;
#endif
#ifdef SIGWINCH
    if (id == pd->SIGWINCH_sid)
        return SIGWINCH;
#endif
#ifdef SIGIO
    if (id == pd->SIGIO_sid)
        return SIGIO;
#endif
#ifdef SIGPOLL
    if (id == pd->SIGPOLL_sid)
        return SIGPOLL;
#endif
#ifdef SIGLOST
    if (id == pd->SIGLOST_sid)
        return SIGLOST;
#endif
#ifdef SIGPWR
    if (id == pd->SIGPWR_sid)
        return SIGPWR;
#endif
#ifdef SIGSYS
    if (id == pd->SIGSYS_sid)
        return SIGSYS;
#endif
    return -1;
}

FklSid_t signumToSymbol(int signum, FuvPublicData *pd) {
#ifdef SIGHUP
    if (signum == SIGHUP)
        return pd->SIGHUP_sid;
#endif
#ifdef SIGINT
    if (signum == SIGINT)
        return pd->SIGINT_sid;
#endif
#ifdef SIGQUIT
    if (signum == SIGQUIT)
        return pd->SIGQUIT_sid;
#endif
#ifdef SIGILL
    if (signum == SIGILL)
        return pd->SIGILL_sid;
#endif
#ifdef SIGTRAP
    if (signum == SIGTRAP)
        return pd->SIGTRAP_sid;
#endif
#ifdef SIGABRT
    if (signum == SIGABRT)
        return pd->SIGABRT_sid;
#endif
#ifdef SIGIOT
    if (signum == SIGIOT)
        return pd->SIGIOT_sid;
#endif
#ifdef SIGBUS
    if (signum == SIGBUS)
        return pd->SIGBUS_sid;
#endif
#ifdef SIGFPE
    if (signum == SIGFPE)
        return pd->SIGFPE_sid;
#endif
#ifdef SIGKILL
    if (signum == SIGKILL)
        return pd->SIGKILL_sid;
#endif
#ifdef SIGUSR1
    if (signum == SIGUSR1)
        return pd->SIGUSR1_sid;
#endif
#ifdef SIGSEGV
    if (signum == SIGSEGV)
        return pd->SIGSEGV_sid;
#endif
#ifdef SIGUSR2
    if (signum == SIGUSR2)
        return pd->SIGUSR2_sid;
#endif
#ifdef SIGPIPE
    if (signum == SIGPIPE)
        return pd->SIGPIPE_sid;
#endif
#ifdef SIGALRM
    if (signum == SIGALRM)
        return pd->SIGALRM_sid;
#endif
#ifdef SIGTERM
    if (signum == SIGTERM)
        return pd->SIGTERM_sid;
#endif
#ifdef SIGCHLD
    if (signum == SIGCHLD)
        return pd->SIGCHLD_sid;
#endif
#ifdef SIGSTKFLT
    if (signum == SIGSTKFLT)
        return pd->SIGSTKFLT_sid;
#endif
#ifdef SIGCONT
    if (signum == SIGCONT)
        return pd->SIGCONT_sid;
#endif
#ifdef SIGSTOP
    if (signum == SIGSTOP)
        return pd->SIGSTOP_sid;
#endif
#ifdef SIGTSTP
    if (signum == SIGTSTP)
        return pd->SIGTSTP_sid;
#endif
#ifdef SIGBREAK
    if (signum == SIGBREAK)
        return pd->SIGBREAK_sid;
#endif
#ifdef SIGTTIN
    if (signum == SIGTTIN)
        return pd->SIGTTIN_sid;
#endif
#ifdef SIGTTOU
    if (signum == SIGTTOU)
        return pd->SIGTTOU_sid;
#endif
#ifdef SIGURG
    if (signum == SIGURG)
        return pd->SIGURG_sid;
#endif
#ifdef SIGXCPU
    if (signum == SIGXCPU)
        return pd->SIGXCPU_sid;
#endif
#ifdef SIGXFSZ
    if (signum == SIGXFSZ)
        return pd->SIGXFSZ_sid;
#endif
#ifdef SIGVTALRM
    if (signum == SIGVTALRM)
        return pd->SIGVTALRM_sid;
#endif
#ifdef SIGPROF
    if (signum == SIGPROF)
        return pd->SIGPROF_sid;
#endif
#ifdef SIGWINCH
    if (signum == SIGWINCH)
        return pd->SIGWINCH_sid;
#endif
#ifdef SIGIO
    if (signum == SIGIO)
        return pd->SIGIO_sid;
#endif
#ifdef SIGPOLL
    if (signum == SIGPOLL)
        return pd->SIGPOLL_sid;
#endif
#ifdef SIGLOST
    if (signum == SIGLOST)
        return pd->SIGLOST_sid;
#endif
#ifdef SIGPWR
    if (signum == SIGPWR)
        return pd->SIGPWR_sid;
#endif
#ifdef SIGSYS
    if (signum == SIGSYS)
        return pd->SIGSYS_sid;
#endif
    return 0;
}
