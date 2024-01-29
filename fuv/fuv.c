#include"fuv.h"

static inline FklVMvalue* create_uv_error(const char* who
		,int err_id
		,FklVM* exe
		,FklVMvalue* pd_obj)
{
	FKL_DECL_VM_UD_DATA(pd,FuvPublicData,pd_obj);
	FklSid_t id=0;
	switch(err_id)
	{
#define XX(code,_) case UV_##code: id=pd->uv_err_sid_##code;break;
		UV_ERRNO_MAP(XX);
		default:
		id=pd->uv_err_sid_UNKNOWN;
	}
	return fklCreateVMvalueErrorWithCstr(exe
			,id
			,who
			,fklCreateStringFromCstr(uv_strerror(err_id)));
}

void raiseUvError(const char* who
		,int err_id
		,FklVM* exe
		,FklVMvalue* pd_obj)
{
	fklRaiseVMerror(create_uv_error(who,err_id,exe,pd_obj),exe);
}

FklVMvalue* createUvError(const char* who
		,int err_id
		,FklVM* exe
		,FklVMvalue* pd_obj)
{
	return create_uv_error(who,err_id,exe,pd_obj);
}

void raiseFuvError(const char* who,FuvErrorType err,FklVM* exe,FklVMvalue* pd_obj)
{
	FKL_DECL_VM_UD_DATA(pd,FuvPublicData,pd_obj);
	FklSid_t sid=pd->fuv_err_sid[err];
	static const char* fuv_err_msg[]=
	{
		NULL,
		"close closing handle",
		"handle has been closed",
	};
	FklVMvalue* ev=fklCreateVMvalueErrorWithCstr(exe
			,sid
			,who
			,fklCreateStringFromCstr(fuv_err_msg[err]));
	fklRaiseVMerror(ev,exe);
}

int sigSymbolToSignum(FklSid_t id,FuvPublicData* pd)
{
#ifdef SIGHUP
	if(id==pd->SIGHUP_sid)return SIGHUP;
#endif
#ifdef SIGINT
	if(id==pd->SIGINT_sid)return SIGINT;
#endif
#ifdef SIGQUIT
	if(id==pd->SIGQUIT_sid)return SIGQUIT;
#endif
#ifdef SIGILL
	if(id==pd->SIGILL_sid)return SIGILL;
#endif
#ifdef SIGTRAP
	if(id==pd->SIGTRAP_sid)return SIGTRAP;
#endif
#ifdef SIGABRT
	if(id==pd->SIGABRT_sid)return SIGABRT;
#endif
#ifdef SIGIOT
	if(id==pd->SIGIOT_sid)return SIGIOT;
#endif
#ifdef SIGBUS
	if(id==pd->SIGBUS_sid)return SIGBUS;
#endif
#ifdef SIGFPE
	if(id==pd->SIGFPE_sid)return SIGFPE;
#endif
#ifdef SIGKILL
	if(id==pd->SIGKILL_sid)return SIGKILL;
#endif
#ifdef SIGUSR1
	if(id==pd->SIGUSR1_sid)return SIGUSR1;
#endif
#ifdef SIGSEGV
	if(id==pd->SIGSEGV_sid)return SIGSEGV;
#endif
#ifdef SIGUSR2
	if(id==pd->SIGUSR2_sid)return SIGUSR2;
#endif
#ifdef SIGPIPE
	if(id==pd->SIGPIPE_sid)return SIGPIPE;
#endif
#ifdef SIGALRM
	if(id==pd->SIGALRM_sid)return SIGALRM;
#endif
#ifdef SIGTERM
	if(id==pd->SIGTERM_sid)return SIGTERM;
#endif
#ifdef SIGCHLD
	if(id==pd->SIGCHLD_sid)return SIGCHLD;
#endif
#ifdef SIGSTKFLT
	if(id==pd->SIGSTKFLT_sid)return SIGSTKFLT;
#endif
#ifdef SIGCONT
	if(id==pd->SIGCONT_sid)return SIGCONT;
#endif
#ifdef SIGSTOP
	if(id==pd->SIGSTOP_sid)return SIGSTOP;
#endif
#ifdef SIGTSTP
	if(id==pd->SIGTSTP_sid)return SIGTSTP;
#endif
#ifdef SIGBREAK
	if(id==pd->SIGBREAK_sid)return SIGBREAK;
#endif
#ifdef SIGTTIN
	if(id==pd->SIGTTIN_sid)return SIGTTIN;
#endif
#ifdef SIGTTOU
	if(id==pd->SIGTTOU_sid)return SIGTTOU;
#endif
#ifdef SIGURG
	if(id==pd->SIGURG_sid)return SIGURG;
#endif
#ifdef SIGXCPU
	if(id==pd->SIGXCPU_sid)return SIGXCPU;
#endif
#ifdef SIGXFSZ
	if(id==pd->SIGXFSZ_sid)return SIGXFSZ;
#endif
#ifdef SIGVTALRM
	if(id==pd->SIGVTALRM_sid)return SIGVTALRM;
#endif
#ifdef SIGPROF
	if(id==pd->SIGPROF_sid)return SIGPROF;
#endif
#ifdef SIGWINCH
	if(id==pd->SIGWINCH_sid)return SIGWINCH;
#endif
#ifdef SIGIO
	if(id==pd->SIGIO_sid)return SIGIO;
#endif
#ifdef SIGPOLL
	if(id==pd->SIGPOLL_sid)return SIGPOLL;
#endif
#ifdef SIGLOST
	if(id==pd->SIGLOST_sid)return SIGLOST;
#endif
#ifdef SIGPWR
	if(id==pd->SIGPWR_sid)return SIGPWR;
#endif
#ifdef SIGSYS
	if(id==pd->SIGSYS_sid)return SIGSYS;
#endif
	return 0;
}
