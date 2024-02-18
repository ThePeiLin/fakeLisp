#include<fakeLisp/vm.h>
#include<uv.h>
#include"fuv.h"

#define PREDICATE(condition) FKL_DECL_AND_CHECK_ARG(val,exe);\
	FKL_CHECK_REST_ARG(exe);\
	FKL_VM_PUSH_VALUE(exe,(condition)\
			?FKL_VM_TRUE\
			:FKL_VM_NIL);\
	return 0;

#define CHECK_HANDLE_CLOSED(H,EXE,PD) if((H)==NULL)raiseFuvError(FUV_ERR_HANDLE_CLOSED,(EXE),(PD))

#define GET_HANDLE(FUV_HANDLE) (&((FUV_HANDLE)->handle))

#define DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(NAME,OBJ,EXE,PD) FKL_DECL_VM_UD_DATA((NAME),FuvHandleUd,(OBJ));\
	CHECK_HANDLE_CLOSED(*(NAME),(EXE),(PD))

#define GET_REQ(FUV_REQ) (&((FUV_REQ)->req))

#define CHECK_REQ_CANCELED(R,EXE,PD) if((R)==NULL)raiseFuvError(FUV_ERR_REQ_CANCELED,(EXE),(PD))

#define CHECK_UV_RESULT(R,EXE,PD) if((R)<0)raiseUvError((R),(EXE),(PD))

#define CHECK_UV_RESULT_AND_CLEANUP_REQ(R,REQ,EXE,PD) if((R)<0){cleanup_req((REQ));raiseUvError((R),(EXE),PD);}

#define CHECK_UV_RESULT_AND_CLEANUP_HANDLE(R,HANDLE,LOOP,EXE,PD) if((R)<0){cleanup_handle((HANDLE),(LOOP));raiseUvError((R),(EXE),PD);}

static FklVMudMetaTable FuvPublicDataMetaTable=
{
	.size=sizeof(FuvPublicData),
};

static inline void destroy_req(FuvReq* fuv_req)
{
	uv_req_t* req=&fuv_req->req;
	if(req->type==UV_FS)
		uv_fs_req_cleanup((uv_fs_t*)req);
	free(fuv_req);
}

static inline void cleanup_req(FklVMvalue* req_obj)
{
	FKL_DECL_VM_UD_DATA(fuv_req,FuvReqUd,req_obj);
	FuvReq* req=*fuv_req;
	uninitFuvReq(fuv_req);
	destroy_req(req);
}

static inline void cleanup_handle(FklVMvalue* handle_obj,FklVMvalue* loop_obj)
{
	if(handle_obj==NULL)
		return;
	FKL_DECL_VM_UD_DATA(handle_ud,FuvHandleUd,handle_obj);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	FuvHandle* fuv_handle=*handle_ud;
	FuvHandleData* hdata=&fuv_handle->data;
	FuvLoopData* ldata=&fuv_loop->data;
	hdata->loop=NULL;
	free(fuv_handle);
	if(handle_obj)
	{
		fklDelHashItem(&handle_obj,&ldata->gc_values,NULL);
		*handle_ud=NULL;
	}
}

static inline void init_fuv_public_data(FuvPublicData* pd,FklSymbolTable* st)
{
	static const char* loop_mode[]=
	{
		"default",
		"once",
		"nowait",
	};
	pd->loop_mode[UV_RUN_DEFAULT]=fklAddSymbolCstr(loop_mode[UV_RUN_DEFAULT],st)->id;
	pd->loop_mode[UV_RUN_ONCE]=fklAddSymbolCstr(loop_mode[UV_RUN_ONCE],st)->id;
	pd->loop_mode[UV_RUN_NOWAIT]=fklAddSymbolCstr(loop_mode[UV_RUN_NOWAIT],st)->id;

	static const char* fuv_err_sym[]=
	{
		"dummy",
		"fuv-handle-error",
		"fuv-handle-error",
		"fuv-req-error",
	};
	for(size_t i=0;i<FUV_ERR_NUM;i++)
		pd->fuv_err_sid[i]=fklAddSymbolCstr(fuv_err_sym[i],st)->id;

	static const char* loop_config_sym[]=
	{
		"loop-block-signal",
		"metrics-idle-time",
	};
	pd->loop_block_signal_sid=fklAddSymbolCstr(loop_config_sym[UV_LOOP_BLOCK_SIGNAL],st)->id;
	pd->metrics_idle_time_sid=fklAddSymbolCstr(loop_config_sym[UV_METRICS_IDLE_TIME],st)->id;

#define XX(code,_) pd->uv_err_sid_##code=fklAddSymbolCstr("UV_"#code,st)->id;
	UV_ERRNO_MAP(XX);
#undef XX

	pd->UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS_sid=fklAddSymbolCstr("verbatim",st)->id;
	pd->UV_PROCESS_DETACHED_sid=fklAddSymbolCstr("detached",st)->id;
	pd->UV_PROCESS_WINDOWS_HIDE_sid=fklAddSymbolCstr("hide",st)->id;
	pd->UV_PROCESS_WINDOWS_HIDE_CONSOLE_sid=fklAddSymbolCstr("hide-console",st)->id;
	pd->UV_PROCESS_WINDOWS_HIDE_GUI_sid=fklAddSymbolCstr("hide-gui",st)->id;

	pd->UV_TTY_MODE_NORMAL_sid=fklAddSymbolCstr("normal",st)->id;
	pd->UV_TTY_MODE_RAW_sid=fklAddSymbolCstr("raw",st)->id;
	pd->UV_TTY_MODE_IO_sid=fklAddSymbolCstr("io",st)->id;

	pd->UV_TTY_SUPPORTED_sid=fklAddSymbolCstr("supported",st)->id;
	pd->UV_TTY_UNSUPPORTED_sid=fklAddSymbolCstr("unsupported",st)->id;

	pd->UV_UDP_IPV6ONLY_sid=fklAddSymbolCstr("ipv6only",st)->id;
	pd->UV_UDP_PARTIAL_sid=fklAddSymbolCstr("partial",st)->id;
	pd->UV_UDP_REUSEADDR_sid=fklAddSymbolCstr("reuseaddr",st)->id;
	pd->UV_UDP_MMSG_CHUNK_sid=fklAddSymbolCstr("mmsg-chunk",st)->id;
	pd->UV_UDP_MMSG_FREE_sid=fklAddSymbolCstr("mmsg-free",st)->id;
	pd->UV_UDP_LINUX_RECVERR_sid=fklAddSymbolCstr("linux-recverr",st)->id;
	pd->UV_UDP_RECVMMSG_sid=fklAddSymbolCstr("recvmmsg",st)->id;

	pd->UV_LEAVE_GROUP_sid=fklAddSymbolCstr("leave",st)->id;
	pd->UV_JOIN_GROUP_sid=fklAddSymbolCstr("join",st)->id;

	pd->AI_ADDRCONFIG_sid=fklAddSymbolCstr("addrconfig",st)->id;
#ifdef AI_V4MAPPED
	pd->AI_V4MAPPED_sid=fklAddSymbolCstr("v4mapped",st)->id;
#endif
#ifdef AI_ALL
	pd->AI_ALL_sid=fklAddSymbolCstr("all",st)->id;
#endif
	pd->AI_NUMERICHOST_sid=fklAddSymbolCstr("numerichost",st)->id;
	pd->AI_PASSIVE_sid=fklAddSymbolCstr("passive",st)->id;
	pd->AI_NUMERICSERV_sid=fklAddSymbolCstr("numerserv",st)->id;
	pd->AI_CANONNAME_sid=fklAddSymbolCstr("canonname",st)->id;

	pd->f_ip_sid=fklAddSymbolCstr("ip",st)->id;
	pd->f_addr_sid=fklAddSymbolCstr("addr",st)->id;
	pd->f_port_sid=fklAddSymbolCstr("port",st)->id;
	pd->f_family_sid=fklAddSymbolCstr("family",st)->id;
	pd->f_socktype_sid=fklAddSymbolCstr("socktype",st)->id;
	pd->f_protocol_sid=fklAddSymbolCstr("protocol",st)->id;
	pd->f_canonname_sid=fklAddSymbolCstr("canonname",st)->id;
	pd->f_hostname_sid=fklAddSymbolCstr("hostname",st)->id;
	pd->f_service_sid=fklAddSymbolCstr("service",st)->id;

	pd->f_args_sid=fklAddSymbolCstr("args",st)->id;
	pd->f_env_sid=fklAddSymbolCstr("env",st)->id;
	pd->f_cwd_sid=fklAddSymbolCstr("cwd",st)->id;
	pd->f_stdio_sid=fklAddSymbolCstr("stdio",st)->id;
	pd->f_uid_sid=fklAddSymbolCstr("uid",st)->id;
	pd->f_gid_sid=fklAddSymbolCstr("gid",st)->id;

#ifdef AF_UNSPEC
	pd->AF_UNSPEC_sid=fklAddSymbolCstr("unspec",st)->id;
#endif
#ifdef AF_UNIX
	pd->AF_UNIX_sid=fklAddSymbolCstr("unix",st)->id;
#endif
#ifdef AF_INET
	pd->AF_INET_sid=fklAddSymbolCstr("inet",st)->id;
#endif
#ifdef AF_INET6
	pd->AF_INET6_sid=fklAddSymbolCstr("inet6",st)->id;
#endif
#ifdef AF_IPX
	pd->AF_IPX_sid=fklAddSymbolCstr("ipx",st)->id;
#endif
#ifdef AF_NETLINK
	pd->AF_NETLINK_sid=fklAddSymbolCstr("netlink",st)->id;
#endif
#ifdef AF_X25
	pd->AF_X25_sid=fklAddSymbolCstr("x25",st)->id;
#endif
#ifdef AF_AX25
	pd->AF_AX25_sid=fklAddSymbolCstr("ax25",st)->id;
#endif
#ifdef AF_ATMPVC
	pd->AF_ATMPVC_sid=fklAddSymbolCstr("atmpvc",st)->id;
#endif
#ifdef AF_APPLETALK
	pd->AF_APPLETALK_sid=fklAddSymbolCstr("appletalk",st)->id;
#endif
#ifdef AF_PACKET
	pd->AF_PACKET_sid=fklAddSymbolCstr("packet",st)->id;
#endif

#ifdef SOCK_STREAM
	pd->SOCK_STREAM_sid=fklAddSymbolCstr("stream",st)->id;
#endif
#ifdef SOCK_DGRAM
	pd->SOCK_DGRAM_sid=fklAddSymbolCstr("dgram",st)->id;
#endif
#ifdef SOCK_SEQPACKET
	pd->SOCK_SEQPACKET_sid=fklAddSymbolCstr("seqpacket",st)->id;
#endif
#ifdef SOCK_RAW
	pd->SOCK_RAW_sid=fklAddSymbolCstr("raw",st)->id;
#endif
#ifdef SOCK_RDM
	pd->SOCK_RDM_sid=fklAddSymbolCstr("rdm",st)->id;
#endif

#ifdef SIGHUP
	pd->SIGHUP_sid=fklAddSymbolCstr("sighup",st)->id;
#endif
#ifdef SIGINT
	pd->SIGINT_sid=fklAddSymbolCstr("sigint",st)->id;
#endif
#ifdef SIGQUIT
	pd->SIGQUIT_sid=fklAddSymbolCstr("sigquit",st)->id;
#endif
#ifdef SIGILL
	pd->SIGILL_sid=fklAddSymbolCstr("sigill",st)->id;
#endif
#ifdef SIGTRAP
	pd->SIGTRAP_sid=fklAddSymbolCstr("sigtrap",st)->id;
#endif
#ifdef SIGABRT
	pd->SIGABRT_sid=fklAddSymbolCstr("sigabrt",st)->id;
#endif
#ifdef SIGIOT
	pd->SIGIOT_sid=fklAddSymbolCstr("sigiot",st)->id;
#endif
#ifdef SIGBUS
	pd->SIGBUS_sid=fklAddSymbolCstr("sigbus",st)->id;
#endif
#ifdef SIGFPE
	pd->SIGFPE_sid=fklAddSymbolCstr("sigfpe",st)->id;
#endif
#ifdef SIGKILL
	pd->SIGKILL_sid=fklAddSymbolCstr("sigkill",st)->id;
#endif
#ifdef SIGUSR1
	pd->SIGUSR1_sid=fklAddSymbolCstr("sigusr1",st)->id;
#endif
#ifdef SIGSEGV
	pd->SIGSEGV_sid=fklAddSymbolCstr("sigsegv",st)->id;
#endif
#ifdef SIGUSR2
	pd->SIGUSR2_sid=fklAddSymbolCstr("sigusr2",st)->id;
#endif
#ifdef SIGPIPE
	pd->SIGPIPE_sid=fklAddSymbolCstr("sigpipe",st)->id;
#endif
#ifdef SIGALRM
	pd->SIGALRM_sid=fklAddSymbolCstr("sigalrm",st)->id;
#endif
#ifdef SIGTERM
	pd->SIGTERM_sid=fklAddSymbolCstr("sigterm",st)->id;
#endif
#ifdef SIGCHLD
	pd->SIGCHLD_sid=fklAddSymbolCstr("sigchld",st)->id;
#endif
#ifdef SIGSTKFLT
	pd->SIGSTKFLT_sid=fklAddSymbolCstr("sigstkflt",st)->id;
#endif
#ifdef SIGCONT
	pd->SIGCONT_sid=fklAddSymbolCstr("sigcont",st)->id;
#endif
#ifdef SIGSTOP
	pd->SIGSTOP_sid=fklAddSymbolCstr("sigstop",st)->id;
#endif
#ifdef SIGTSTP
	pd->SIGTSTP_sid=fklAddSymbolCstr("sigtstp",st)->id;
#endif
#ifdef SIGBREAK
	pd->SIGBREAK_sid=fklAddSymbolCstr("sigbreak",st)->id;
#endif
#ifdef SIGTTIN
	pd->SIGTTIN_sid=fklAddSymbolCstr("sigttin",st)->id;
#endif
#ifdef SIGTTOU
	pd->SIGTTOU_sid=fklAddSymbolCstr("sigttou",st)->id;
#endif
#ifdef SIGURG
	pd->SIGURG_sid=fklAddSymbolCstr("sigurg",st)->id;
#endif
#ifdef SIGXCPU
	pd->SIGXCPU_sid=fklAddSymbolCstr("sigxcpu",st)->id;
#endif
#ifdef SIGXFSZ
	pd->SIGXFSZ_sid=fklAddSymbolCstr("sigxfsz",st)->id;
#endif
#ifdef SIGVTALRM
	pd->SIGVTALRM_sid=fklAddSymbolCstr("sigvtalrm",st)->id;
#endif
#ifdef SIGPROF
	pd->SIGPROF_sid=fklAddSymbolCstr("sigprof",st)->id;
#endif
#ifdef SIGWINCH
	pd->SIGWINCH_sid=fklAddSymbolCstr("sigwinch",st)->id;
#endif
#ifdef SIGIO
	pd->SIGIO_sid=fklAddSymbolCstr("sigio",st)->id;
#endif
#ifdef SIGPOLL
	pd->SIGPOLL_sid=fklAddSymbolCstr("sigpoll",st)->id;
#endif
#ifdef SIGLOST
	pd->SIGLOST_sid=fklAddSymbolCstr("siglost",st)->id;
#endif
#ifdef SIGPWR
	pd->SIGPWR_sid=fklAddSymbolCstr("sigpwr",st)->id;
#endif
#ifdef SIGSYS
	pd->SIGSYS_sid=fklAddSymbolCstr("sigsys",st)->id;
#endif

#ifdef NI_NAMEREQD
	pd->NI_NAMEREQD_sid=fklAddSymbolCstr("namereqd",st)->id;
#endif
#ifdef NI_DGRAM
	pd->NI_DGRAM_sid=fklAddSymbolCstr("dgram",st)->id;
#endif
#ifdef NI_NOFQDN
	pd->NI_NOFQDN_sid=fklAddSymbolCstr("nofqdn",st)->id;
#endif
#ifdef NI_NUMERICHOST
	pd->NI_NUMERICHOST_sid=fklAddSymbolCstr("numerichost",st)->id;
#endif
#ifdef NI_NUMERICSERV
	pd->NI_NUMERICSERV_sid=fklAddSymbolCstr("numericserv",st)->id;
#endif
#ifdef NI_IDN
	pd->NI_IDN_sid=fklAddSymbolCstr("idn",st)->id;
#endif
#ifdef NI_IDN_ALLOW_UNASSIGNED
	pd->NI_IDN_ALLOW_UNASSIGNED_sid=fklAddSymbolCstr("idn-allow-unassigned",st)->id;
#endif
#ifdef NI_IDN_USE_STD3_ASCII_RULES
	pd->NI_IDN_USE_STD3_ASCII_RULES_sid=fklAddSymbolCstr("idn-use-std3-ascii-rules",st)->id;
#endif
}

static int fuv_loop_p(FKL_CPROC_ARGL){PREDICATE(isFuvLoop(val))}

static int fuv_make_loop(FKL_CPROC_ARGL)
{
	FKL_CHECK_REST_ARG(exe);
	int err=0;
	FklVMvalue* r=createFuvLoop(exe,ctx->proc,&err);
	CHECK_UV_RESULT(err,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int fuv_loop_close(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_loop_close(&fuv_loop->loop);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fuv_loop_run(FKL_CPROC_ARGL)
{
	switch(ctx->context)
	{
		case 0:
			{
				FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
				FklVMvalue* mode_obj=FKL_VM_POP_ARG(exe);
				FKL_CHECK_REST_ARG(exe);
				FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
				FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
				FKL_VM_PUSH_VALUE(exe,loop_obj);
				FklVMframe* origin_top_frame=exe->top_frame;
				ctx->rtp=exe->tp;
				fuv_loop->data.exe=exe;
				int need_continue=0;
				int mode=UV_RUN_DEFAULT;
				if(mode_obj)
				{
					FKL_CHECK_TYPE(mode_obj,FKL_IS_SYM,exe);
					FKL_DECL_VM_UD_DATA(pbd,FuvPublicData,ctx->pd);
					FklSid_t mode_id=FKL_GET_SYM(mode_obj);
					for(;mode<(int)(sizeof(pbd->loop_mode)/sizeof(FklSid_t))
							;mode++)
						if(pbd->loop_mode[mode]==mode_id)
							break;
					if(mode>UV_RUN_NOWAIT)
						FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
				}
				if(setjmp(fuv_loop->data.buf))
				{
					need_continue=1;
					ctx->context=(uintptr_t)fklMoveVMframeToTop(exe,origin_top_frame);
				}
				else
				{
					fuv_loop->data.mode=mode;
					fklUnlockThread(exe);
					int r=uv_run(&fuv_loop->loop,mode);
					fklLockThread(exe);
					if(r<0)
					{
						FKL_VM_PUSH_VALUE(exe,createUvError(r,exe,ctx->pd));
						ctx->context=1;
						need_continue=1;
					}
					while(exe->top_frame!=origin_top_frame)
						fklPopVMframe(exe);
				}
				fuv_loop->data.mode=-1;
				fuv_loop->data.exe=NULL;
				exe->state=FKL_VM_READY;
				return need_continue;
			}
			break;
		default:
			{
				FklVMframe* prev=(FklVMframe*)ctx->context;
				fklInsertTopVMframeAsPrev(exe,prev);
				fklRaiseVMerror(FKL_VM_POP_TOP_VALUE(exe),exe);
			}
			break;
	}
	return 0;
}

struct WalkCtx
{
	FklVM* exe;
	FklVMvalue* ev;
};

static void fuv_loop_walk_cb(uv_handle_t* handle,void* arg)
{
	struct WalkCtx* walk_ctx=arg;
	if(walk_ctx->ev)
		return;
	FuvLoopData* floop_data=uv_loop_get_data(uv_handle_get_loop(handle));
	if(handle==(uv_handle_t*)&floop_data->error_check_idle)
		return;
	FklVM* exe=walk_ctx->exe;
	fklLockThread(exe);
	FklVMvalue* proc=FKL_VM_GET_TOP_VALUE(exe);
	exe->state=FKL_VM_READY;
	FklVMframe* buttom_frame=exe->top_frame;
	FuvHandle* fuv_handle=uv_handle_get_data(handle);
	FuvHandleData* handle_data=&fuv_handle->data;
	fklSetBp(exe);
	FKL_VM_PUSH_VALUE(exe,handle_data->handle);
	fklCallObj(exe,proc);
	if(exe->thread_run_cb(exe,buttom_frame))
		walk_ctx->ev=FKL_VM_GET_TOP_VALUE(exe);
	else
		exe->tp--;
	fklUnlockThread(exe);
}

static int fuv_loop_walk(FKL_CPROC_ARGL)
{
	switch(ctx->context)
	{
		case 0:
			{
				FKL_DECL_AND_CHECK_ARG2(loop_obj,proc_obj,exe);
				FKL_CHECK_REST_ARG(exe);
				FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
				FKL_CHECK_TYPE(proc_obj,fklIsCallable,exe);
				FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
				FKL_VM_PUSH_VALUE(exe,loop_obj);
				FKL_VM_PUSH_VALUE(exe,proc_obj);
				FklVMframe* origin_top_frame=exe->top_frame;
				int need_continue=0;
				struct WalkCtx walk_ctx=
				{
					.exe=exe,
					.ev=NULL,
				};

				fklUnlockThread(exe);
				uv_walk(&fuv_loop->loop,fuv_loop_walk_cb,&walk_ctx);
				fklLockThread(exe);
				if(walk_ctx.ev)
				{
					need_continue=1;
					ctx->context=(uintptr_t)fklMoveVMframeToTop(exe,origin_top_frame);

				}
				else
				{
					while(exe->top_frame!=origin_top_frame)
						fklPopVMframe(exe);
					exe->tp--;
				}
				exe->state=FKL_VM_READY;
				return need_continue;
			}
			break;
		default:
			{
				FklVMframe* prev=(FklVMframe*)ctx->context;
				fklInsertTopVMframeAsPrev(exe,prev);
				fklRaiseVMerror(FKL_VM_POP_TOP_VALUE(exe),exe);
			}
			break;
	}
	return 0;
}

static int fuv_loop_configure(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(loop_obj,option_obj,exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_CHECK_TYPE(option_obj,FKL_IS_SYM,exe);
	FklSid_t option_id=FKL_GET_SYM(option_obj);
	uv_loop_option option=UV_LOOP_BLOCK_SIGNAL;
	FklVMvalue* pd=ctx->pd;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,pd);
	if(option_id==fpd->loop_block_signal_sid)
		option=UV_LOOP_BLOCK_SIGNAL;
	else if(option_id==fpd->metrics_idle_time_sid)
		option=UV_METRICS_IDLE_TIME;
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	uv_loop_t* loop=&fuv_loop->loop;
	switch(option)
	{
		case UV_LOOP_BLOCK_SIGNAL:
			{
				FklVMvalue* cur=FKL_VM_POP_ARG(exe);
				while(cur)
				{
					if(FKL_IS_FIX(cur))
					{
						int64_t i=FKL_GET_FIX(cur);
						int r=uv_loop_configure(loop,UV_LOOP_BLOCK_SIGNAL,i);
						CHECK_UV_RESULT(r,exe,pd);
					}
					else if(FKL_IS_SYM(cur))
					{
						FklSid_t sid=FKL_GET_SYM(cur);
						int signum=symbolToSignum(sid,fpd);
						if(signum<=0)
							FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
						int r=uv_loop_configure(loop,UV_LOOP_BLOCK_SIGNAL,signum);
						CHECK_UV_RESULT(r,exe,pd);
					}
					else
						FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
					cur=FKL_VM_POP_ARG(exe);
				}
				fklResBp(exe);
			}
			break;
		case UV_METRICS_IDLE_TIME:
			{
				FKL_CHECK_REST_ARG(exe);
				int r=uv_loop_configure(loop,UV_METRICS_IDLE_TIME);
				CHECK_UV_RESULT(r,exe,pd);
			}
			break;
	}
	FKL_VM_PUSH_VALUE(exe,loop_obj);
	return 0;
}

static int fuv_loop_alive_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_loop_alive(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,r?FKL_VM_TRUE:FKL_VM_NIL);
	return 0;
}

static int fuv_loop_stop(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	uv_stop(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,loop_obj);
	return 0;
}

static int fuv_loop_mode(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=fuv_loop->data.mode;
	if(r==-1)
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	else
	{
		FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
		FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_SYM(fpd->loop_mode[r]));
	}
	return 0;
}

static int fuv_loop_backend_fd(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_backend_fd(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,r==-1?FKL_VM_NIL:FKL_MAKE_VM_FIX(r));
	return 0;
}

static int fuv_loop_backend_timeout(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_backend_timeout(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,r==-1?FKL_VM_NIL:FKL_MAKE_VM_FIX(r));
	return 0;
}

static int fuv_loop_now(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	uint64_t r=uv_now(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(r,exe));
	return 0;
}

static int fuv_loop_update_time(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	uv_update_time(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,loop_obj);
	return 0;
}

static int fuv_handle_p(FKL_CPROC_ARGL){PREDICATE(isFuvHandle(val))}

static int fuv_handle_active_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle,handle_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,uv_is_active(GET_HANDLE(*fuv_handle))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int fuv_handle_closing_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle,handle_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,uv_is_closing(GET_HANDLE(*fuv_handle))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static inline void fuv_call_handle_callback_in_loop(uv_handle_t* handle
		,FuvHandleData* handle_data
		,uv_loop_t* loop
		,FuvLoopData* loop_data
		,int idx)
{
	FklVMvalue* proc=handle_data->callbacks[idx];
	if(proc)
	{
		FklVM* exe=loop_data->exe;
		fklLockThread(exe);
		uint32_t sbp=exe->bp;
		uint32_t stp=exe->tp;
		uint32_t ltp=exe->ltp;
		FklVMframe* buttom_frame=exe->top_frame;
		exe->state=FKL_VM_READY;
		fklSetBp(exe);
		fklCallObj(exe,proc);
		if(exe->thread_run_cb(exe,buttom_frame))
			startErrorHandle(loop,loop_data,exe,sbp,stp,ltp,buttom_frame);
		else
			exe->tp--;
		fklUnlockThread(exe);
	}
}

static void fuv_close_cb(uv_handle_t* handle)
{
	uv_loop_t* loop=uv_handle_get_loop(handle);
	FuvLoopData* ldata=uv_loop_get_data(loop);
	FuvHandle* fuv_handle=uv_handle_get_data(handle);
	FuvHandleData* hdata=&fuv_handle->data;
	fuv_call_handle_callback_in_loop(handle
			,hdata
			,loop
			,ldata
			,1);
	FklVMvalue* handle_obj=hdata->handle;
	hdata->loop=NULL;
	free(fuv_handle);
	if(handle_obj)
	{
		fklDelHashItem(&handle_obj,&ldata->gc_values,NULL);
		FKL_DECL_VM_UD_DATA(fuv_handle_ud,FuvHandleUd,handle_obj);
		*fuv_handle_ud=NULL;
	}
}

static int fuv_handle_close(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe);
	FklVMvalue* proc_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle,handle_obj,exe,ctx->pd);
	FuvHandle* handle=*fuv_handle;
	uv_handle_t* hh=GET_HANDLE(handle);
	if(uv_is_closing(hh))
		raiseFuvError(FUV_ERR_CLOSE_CLOSEING_HANDLE,exe,ctx->pd);
	if(proc_obj)
	{
		FKL_CHECK_TYPE(proc_obj,fklIsCallable,exe);
		handle->data.callbacks[1]=proc_obj;
	}
	uv_close(hh,fuv_close_cb);
	FKL_VM_PUSH_VALUE(exe,handle_obj);
	return 0;
}

static int fuv_handle_ref_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle,handle_obj,exe,ctx->pd);
	FuvHandle* handle=*fuv_handle;
	FKL_VM_PUSH_VALUE(exe,uv_has_ref(GET_HANDLE(handle))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int fuv_handle_ref(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle,handle_obj,exe,ctx->pd);
	FuvHandle* handle=*fuv_handle;
	uv_ref(GET_HANDLE(handle));
	FKL_VM_PUSH_VALUE(exe,handle_obj);
	return 0;
}

static int fuv_handle_unref(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle,handle_obj,exe,ctx->pd);
	FuvHandle* handle=*fuv_handle;
	uv_unref(GET_HANDLE(handle));
	FKL_VM_PUSH_VALUE(exe,handle_obj);
	return 0;
}

static int fuv_handle_send_buffer_size(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe);
	FklVMvalue* value_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle,handle_obj,exe,ctx->pd);
	FuvHandle* handle=*fuv_handle;
	int value=0;
	if(value_obj)
	{
		FKL_CHECK_TYPE(value_obj,fklIsVMint,exe);
		value=fklGetInt(value_obj);
	}
	int r=uv_send_buffer_size(GET_HANDLE(handle),&value);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(value));
	return 0;
}

static int fuv_handle_recv_buffer_size(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe);
	FklVMvalue* value_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle,handle_obj,exe,ctx->pd);
	FuvHandle* handle=*fuv_handle;
	int value=0;
	if(value_obj)
	{
		FKL_CHECK_TYPE(value_obj,fklIsVMint,exe);
		value=fklGetInt(value_obj);
	}
	int r=uv_recv_buffer_size(GET_HANDLE(handle),&value);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(value));
	return 0;
}

static int fuv_handle_fileno(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle,handle_obj,exe,ctx->pd);
	FuvHandle* handle=*fuv_handle;
	uv_os_fd_t fd=0;
	int r=uv_fileno(GET_HANDLE(handle),&fd);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint((int64_t)(ptrdiff_t)fd,exe));
	return 0;
}

static int fuv_handle_type(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(fuv_handle,handle_obj,exe,ctx->pd);
	FuvHandle* handle=*fuv_handle;
	uv_handle_type type_id=uv_handle_get_type(GET_HANDLE(handle));
	const char* name=uv_handle_type_name(type_id);
	FKL_VM_PUSH_VALUE(exe
			,name==NULL
			?FKL_VM_NIL
			:fklCreateVMvaluePair(exe
				,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(name))
				,FKL_MAKE_VM_FIX(type_id)));
	return 0;
}

static int fuv_timer_p(FKL_CPROC_ARGL){PREDICATE(isFuvTimer(val))}

static int fuv_make_timer(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	int r;
	FklVMvalue* timer_obj=createFuvTimer(exe,ctx->proc,loop_obj,&r);
	CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r,timer_obj,loop_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static void fuv_timer_cb(uv_timer_t* handle)
{
	uv_loop_t* loop=uv_handle_get_loop((uv_handle_t*)handle);
	FuvLoopData* ldata=uv_loop_get_data(loop);
	FuvHandleData* hdata=&((FuvHandle*)uv_handle_get_data((uv_handle_t*)handle))->data;
	fuv_call_handle_callback_in_loop((uv_handle_t*)handle
			,hdata
			,loop
			,ldata
			,0);
}

static int fuv_timer_start(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG4(timer_obj,timer_cb,timeout_obj,repeat_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,exe);
	FKL_CHECK_TYPE(timer_cb,fklIsCallable,exe);
	FKL_CHECK_TYPE(timeout_obj,fklIsVMint,exe);
	FKL_CHECK_TYPE(repeat_obj,fklIsVMint,exe);
	if(fklIsVMnumberLt0(timeout_obj))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	if(fklIsVMnumberLt0(repeat_obj))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	uint64_t timeout=fklGetUint(timeout_obj);
	uint64_t repeat=fklGetUint(repeat_obj);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer,timer_obj,exe,ctx->pd);
	FuvHandle* fuv_handle=*timer;
	fuv_handle->data.callbacks[0]=timer_cb;
	int r=uv_timer_start((uv_timer_t*)GET_HANDLE(fuv_handle),fuv_timer_cb,timeout,repeat);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_stop(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer,timer_obj,exe,ctx->pd);
	FuvHandle* handle=*timer;
	int r=uv_timer_stop((uv_timer_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_again(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer,timer_obj,exe,ctx->pd);
	FuvHandle* handle=*timer;
	int r=uv_timer_again((uv_timer_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_repeat(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer,timer_obj,exe,ctx->pd);
	FuvHandle* handle=*timer;
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(uv_timer_get_repeat((uv_timer_t*)GET_HANDLE(handle)),exe));
	return 0;
}


static int fuv_timer_repeat_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(timer_obj,repeat_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,exe);
	FKL_CHECK_TYPE(repeat_obj,fklIsVMint,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer,timer_obj,exe,ctx->pd);
	FuvHandle* handle=*timer;
	if(fklIsVMnumberLt0(repeat_obj))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	uv_timer_set_repeat((uv_timer_t*)GET_HANDLE(handle),fklGetUint(repeat_obj));
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_due_in(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(timer,timer_obj,exe,ctx->pd);
	FuvHandle* handle=*timer;
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(uv_timer_get_due_in((uv_timer_t*)GET_HANDLE(handle)),exe));
	return 0;
}

static int fuv_prepare_p(FKL_CPROC_ARGL){PREDICATE(isFuvPrepare(val))}

static int fuv_make_prepare(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	int r;
	FklVMvalue* prepare_obj=createFuvPrepare(exe,ctx->proc,loop_obj,&r);
	CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r,prepare_obj,loop_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,prepare_obj);
	return 0;
}

static void fuv_prepare_cb(uv_prepare_t* handle)
{
	uv_loop_t* loop=uv_handle_get_loop((uv_handle_t*)handle);
	FuvLoopData* ldata=uv_loop_get_data(loop);
	FuvHandleData* hdata=&((FuvHandle*)uv_handle_get_data((uv_handle_t*)handle))->data;
	fuv_call_handle_callback_in_loop((uv_handle_t*)handle
			,hdata
			,loop
			,ldata
			,0);
}

static int fuv_prepare_start(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(prepare_obj,prepare_cb,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(prepare_obj,isFuvPrepare,exe);
	FKL_CHECK_TYPE(prepare_cb,fklIsCallable,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(prepare,prepare_obj,exe,ctx->pd);
	FuvHandle* fuv_handle=*prepare;
	fuv_handle->data.callbacks[0]=prepare_cb;
	int r=uv_prepare_start((uv_prepare_t*)GET_HANDLE(fuv_handle),fuv_prepare_cb);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,prepare_obj);
	return 0;
}

static int fuv_prepare_stop(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(prepare_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(prepare_obj,isFuvPrepare,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(prepare,prepare_obj,exe,ctx->pd);
	FuvHandle* handle=*prepare;
	int r=uv_prepare_stop((uv_prepare_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,prepare_obj);
	return 0;
}

static int fuv_idle_p(FKL_CPROC_ARGL){PREDICATE(isFuvIdle(val))}

static int fuv_make_idle(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	int r;
	FklVMvalue* idle=createFuvIdle(exe,ctx->proc,loop_obj,&r);
	CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r,idle,loop_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,idle);
	return 0;
}

static void fuv_idle_cb(uv_idle_t* handle)
{
	uv_loop_t* loop=uv_handle_get_loop((uv_handle_t*)handle);
	FuvLoopData* ldata=uv_loop_get_data(loop);
	FuvHandleData* hdata=&((FuvHandle*)uv_handle_get_data((uv_handle_t*)handle))->data;
	fuv_call_handle_callback_in_loop((uv_handle_t*)handle
			,hdata
			,loop
			,ldata
			,0);
}

static int fuv_idle_start(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(idle_obj,idle_cb,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(idle_obj,isFuvIdle,exe);
	FKL_CHECK_TYPE(idle_cb,fklIsCallable,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(idle,idle_obj,exe,ctx->pd);
	FuvHandle* fuv_handle=*idle;
	fuv_handle->data.callbacks[0]=idle_cb;
	int r=uv_idle_start((uv_idle_t*)GET_HANDLE(fuv_handle),fuv_idle_cb);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,idle_obj);
	return 0;
}

static int fuv_idle_stop(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(idle_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(idle_obj,isFuvIdle,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(idle,idle_obj,exe,ctx->pd);
	FuvHandle* handle=*idle;
	int r=uv_idle_stop((uv_idle_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,idle_obj);
	return 0;
}

static int fuv_check_p(FKL_CPROC_ARGL){PREDICATE(isFuvCheck(val))}

static int fuv_make_check(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	int r;
	FklVMvalue* check=createFuvCheck(exe,ctx->proc,loop_obj,&r);
	CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r,check,loop_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,check);
	return 0;
}

static void fuv_check_cb(uv_check_t* handle)
{
	uv_loop_t* loop=uv_handle_get_loop((uv_handle_t*)handle);
	FuvLoopData* ldata=uv_loop_get_data(loop);
	FuvHandleData* hdata=&((FuvHandle*)uv_handle_get_data((uv_handle_t*)handle))->data;
	fuv_call_handle_callback_in_loop((uv_handle_t*)handle
			,hdata
			,loop
			,ldata
			,0);
}

static int fuv_check_start(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(check_obj,check_cb,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(check_obj,isFuvCheck,exe);
	FKL_CHECK_TYPE(check_cb,fklIsCallable,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(check,check_obj,exe,ctx->pd);
	FuvHandle* fuv_handle=*check;
	fuv_handle->data.callbacks[0]=check_cb;
	int r=uv_check_start((uv_check_t*)GET_HANDLE(fuv_handle),fuv_check_cb);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,check_obj);
	return 0;
}

static int fuv_check_stop(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(check_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(check_obj,isFuvCheck,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(check,check_obj,exe,ctx->pd);
	FuvHandle* handle=*check;
	int r=uv_check_stop((uv_check_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,check_obj);
	return 0;
}

static int fuv_signal_p(FKL_CPROC_ARGL){PREDICATE(isFuvSignal(val))}

static int fuv_make_signal(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	int r;
	FklVMvalue* signal=createFuvSignal(exe,ctx->proc,loop_obj,&r);
	CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r,signal,loop_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,signal);
	return 0;
}

typedef void (*CallbackValueCreator)(FklVM *exe,void* arg);

static inline void fuv_call_handle_callback_in_loop_with_value_creator(uv_handle_t* handle
		,FuvHandleData* handle_data
		,FuvLoopData* loop_data
		,int idx
		,CallbackValueCreator creator
		,void* arg)
{
	FklVMvalue* proc=handle_data->callbacks[idx];
	if(proc)
	{
		FklVM* exe=loop_data->exe;
		fklLockThread(exe);
		uint32_t sbp=exe->bp;
		uint32_t stp=exe->tp;
		uint32_t ltp=exe->ltp;
		FklVMframe* buttom_frame=exe->top_frame;
		exe->state=FKL_VM_READY;
		fklSetBp(exe);
		if(creator)
			creator(exe,arg);
		fklCallObj(exe,proc);
		if(exe->thread_run_cb(exe,buttom_frame))
			startErrorHandle(uv_handle_get_loop(handle)
					,loop_data
					,exe
					,sbp
					,stp
					,ltp
					,buttom_frame);
		else
			exe->tp--;
		fklUnlockThread(exe);
	}
}

struct SignalCbValueCreateArg
{
	FuvPublicData* fpd;
	int num;
};

static void fuv_signal_cb_value_creator(FklVM* exe,void* a)
{
	struct SignalCbValueCreateArg* arg=a;
	FklVMvalue* signum_val=FKL_MAKE_VM_SYM(signumToSymbol(arg->num,arg->fpd));
	FKL_VM_PUSH_VALUE(exe,signum_val);
}

static void fuv_signal_cb(uv_signal_t* handle,int num)
{
	FuvLoopData* ldata=uv_loop_get_data(uv_handle_get_loop((uv_handle_t*)handle));
	FuvHandleData* hdata=&((FuvHandle*)uv_handle_get_data((uv_handle_t*)handle))->data;
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)ldata->exe->top_frame->data)->pd;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,fpd_obj);
	struct SignalCbValueCreateArg arg=
	{
		.fpd=fpd,
		.num=num,
	};
	fuv_call_handle_callback_in_loop_with_value_creator((uv_handle_t*)handle
			,hdata
			,ldata
			,0
			,fuv_signal_cb_value_creator
			,&arg);
}

static int fuv_signal_start(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(signal_obj,signal_cb,signum_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(signal_obj,isFuvSignal,exe);
	FKL_CHECK_TYPE(signal_cb,fklIsCallable,exe);
	int signum=0;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	if(FKL_IS_FIX(signum_obj))
		signum=FKL_GET_FIX(signum_obj);
	else if(FKL_IS_SYM(signum_obj))
		signum=symbolToSignum(FKL_GET_SYM(signum_obj),fpd);
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(signum<=0)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);

	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(signal,signal_obj,exe,ctx->pd);
	FuvHandle* fuv_handle=*signal;
	fuv_handle->data.callbacks[0]=signal_cb;
	int r=uv_signal_start((uv_signal_t*)GET_HANDLE(fuv_handle),fuv_signal_cb,signum);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,signal_obj);
	return 0;
}

static int fuv_signal_start_oneshot(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(signal_obj,signal_cb,signum_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(signal_obj,isFuvSignal,exe);
	FKL_CHECK_TYPE(signal_cb,fklIsCallable,exe);
	int signum=0;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	if(FKL_IS_FIX(signum_obj))
		signum=FKL_GET_FIX(signum_obj);
	else if(FKL_IS_SYM(signum_obj))
		signum=symbolToSignum(FKL_GET_SYM(signum_obj),fpd);
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(signum<=0)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);

	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(signal,signal_obj,exe,ctx->pd);
	FuvHandle* fuv_handle=*signal;
	fuv_handle->data.callbacks[0]=signal_cb;
	int r=uv_signal_start_oneshot((uv_signal_t*)GET_HANDLE(fuv_handle),fuv_signal_cb,signum);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,signal_obj);
	return 0;
}

static int fuv_signal_stop(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(signal_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(signal_obj,isFuvSignal,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(signal,signal_obj,exe,ctx->pd);
	FuvHandle* handle=*signal;
	int r=uv_signal_stop((uv_signal_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,signal_obj);
	return 0;
}

static int fuv_async_p(FKL_CPROC_ARGL){PREDICATE(isFuvAsync(val))}

static int fuv_make_async(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(loop_obj,proc_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_CHECK_TYPE(proc_obj,fklIsCallable,exe);
	int r;
	FklVMvalue* async=createFuvAsync(exe,ctx->proc,loop_obj,proc_obj,&r);
	CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r,async,loop_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,async);
	return 0;
}

static int fuv_async_send(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(async_obj,exe);
	FKL_CHECK_TYPE(async_obj,isFuvAsync,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(async,async_obj,exe,ctx->pd);
	FuvHandle* handle=*async;
	struct FuvAsync* async_handle=(struct FuvAsync*)handle;
	uint32_t arg_num=FKL_VM_GET_ARG_NUM(exe);
	if(!atomic_flag_test_and_set(&async_handle->send_ready))
	{
		struct FuvAsyncExtraData extra=
		{
			.num=arg_num,
			.base=&FKL_VM_GET_VALUE(exe,arg_num),
		};
		atomic_store(&async_handle->extra,&extra);
		int r=uv_async_send(&async_handle->handle);
		FKL_VM_PUSH_VALUE(exe,async_obj);
		FUV_ASYNC_WAIT_COPY(exe,async_handle);
		FUV_ASYNC_SEND_DONE(async_handle);
		exe->tp-=1;
		CHECK_UV_RESULT(r,exe,ctx->pd);
	}
	exe->tp-=arg_num;
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,async_obj);
	return 0;
}

static int fuv_req_p(FKL_CPROC_ARGL){PREDICATE(isFuvReq(val))}

static int fuv_req_cancel(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(req_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(req_obj,isFuvReq,exe);
	FKL_DECL_VM_UD_DATA(fuv_req,FuvReqUd,req_obj);
	FuvReq* req=*fuv_req;
	CHECK_REQ_CANCELED(req,exe,ctx->pd);
	int r=uv_cancel(GET_REQ(req));
	CHECK_UV_RESULT(r,exe,ctx->pd);
	uninitFuvReq(fuv_req);
	req->data.callback=NULL;
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fuv_req_type(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(req_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(req_obj,isFuvReq,exe);
	FKL_DECL_VM_UD_DATA(fuv_req,FuvReqUd,req_obj);
	FuvReq* req=*fuv_req;
	CHECK_REQ_CANCELED(req,exe,ctx->pd);

	uv_req_type type_id=uv_req_get_type(GET_REQ(req));
	const char* name=uv_req_type_name(type_id);
	FKL_VM_PUSH_VALUE(exe
			,name==NULL
			?FKL_VM_NIL
			:fklCreateVMvaluePair(exe
				,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(name))
				,FKL_MAKE_VM_FIX(type_id)));
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int get_protonum_with_cstr(const char* name)
{
	const struct protoent* proto;
	proto=getprotobyname(name);
	if(proto)
		return proto->p_proto;
	return -1;
}

static inline int sid_to_af_name(FklSid_t family_id,FuvPublicData* fpd)
{
#ifdef AF_UNIX
	if(family_id==fpd->AF_UNIX_sid)return AF_UNIX;
#endif
#ifdef AF_INET
	if(family_id==fpd->AF_INET_sid)return AF_INET;
#endif
#ifdef AF_INET6
	if(family_id==fpd->AF_INET6_sid)return AF_INET6;
#endif
#ifdef AF_IPX
	if(family_id==fpd->AF_IPX_sid)return AF_IPX;
#endif
#ifdef AF_NETLINK
	if(family_id==fpd->AF_NETLINK_sid)return AF_NETLINK;
#endif
#ifdef AF_X25
	if(family_id==fpd->AF_X25_sid)return AF_X25;
#endif
#ifdef AF_AX25
	if(family_id==fpd->AF_AX25_sid)return AF_AX25;
#endif
#ifdef AF_ATMPVC
	if(family_id==fpd->AF_ATMPVC_sid)return AF_ATMPVC;
#endif
#ifdef AF_APPLETALK
	if(family_id==fpd->AF_APPLETALK_sid)return AF_APPLETALK;
#endif
#ifdef AF_PACKET
	if(family_id==fpd->AF_PACKET_sid)return AF_PACKET;
#endif
#ifdef AF_UNSPEC
	if(family_id==fpd->AF_UNSPEC_sid)return AF_UNSPEC;
#endif
	return -1;
}

static inline int sid_to_socktype(FklSid_t socktype_id,FuvPublicData* fpd)
{
#ifdef SOCK_STREAM
	if(socktype_id==fpd->SOCK_STREAM_sid)return SOCK_STREAM;
#endif
#ifdef SOCK_DGRAM
	if(socktype_id==fpd->SOCK_DGRAM_sid)return SOCK_STREAM;
#endif
#ifdef SOCK_SEQPACKET
	if(socktype_id==fpd->SOCK_SEQPACKET_sid)return SOCK_SEQPACKET;
#endif
#ifdef SOCK_RAW
	if(socktype_id==fpd->SOCK_RAW_sid)return SOCK_RAW;
#endif
#ifdef SOCK_RDM
	if(socktype_id==fpd->SOCK_RDM_sid)return SOCK_RDM;
#endif
	return -1;
}

static inline FklBuiltinErrorType pop_addrinfo_hints(FklVM* exe
		,struct addrinfo* hints
		,FuvPublicData* fpd)
{
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	while(cur)
	{
		if(FKL_IS_SYM(cur))
		{
			FklSid_t id=FKL_GET_SYM(cur);
			if(id==fpd->f_family_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(!FKL_IS_SYM(cur))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				int af_num=sid_to_af_name(FKL_GET_SYM(cur),fpd);
				if(af_num<0)
					return FKL_ERR_INVALID_VALUE;
				hints->ai_family=af_num;
				goto done;
			}
			else if(id==fpd->f_socktype_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(!FKL_IS_SYM(cur))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				int socktype=sid_to_socktype(FKL_GET_SYM(cur),fpd);
				if(socktype<0)
					return FKL_ERR_INVALID_VALUE;
				hints->ai_socktype=socktype;
				goto done;
			}
			else if(id==fpd->f_protocol_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(FKL_IS_STR(cur))
				{
					const char* name=FKL_VM_STR(cur)->str;
					int proto=get_protonum_with_cstr(name);
					if(proto<0)
						return FKL_ERR_INVALID_VALUE;
					hints->ai_protocol=proto;
				}
				else if(FKL_IS_SYM(cur))
				{
					const char* name=fklVMgetSymbolWithId(exe->gc,FKL_GET_SYM(cur))->symbol->str;
					int proto=get_protonum_with_cstr(name);
					if(proto<0)
						return FKL_ERR_INVALID_VALUE;
					hints->ai_protocol=proto;
				}
				else
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				goto done;
			}
			if(id==fpd->AI_ADDRCONFIG_sid){hints->ai_flags|=AI_ADDRCONFIG;goto done;}
#ifdef AI_V4MAPPED
			if(id==fpd->AI_V4MAPPED_sid){hints->ai_flags|=AI_V4MAPPED;goto done;}
#endif
#ifdef AI_ALL
			if(id==fpd->AI_ALL_sid){hints->ai_flags|=AI_ALL;goto done;}
#endif
			if(id==fpd->AI_NUMERICHOST_sid){hints->ai_flags|=AI_NUMERICHOST;goto done;}
			if(id==fpd->AI_PASSIVE_sid){hints->ai_flags|=AI_PASSIVE;goto done;}
			if(id==fpd->AI_NUMERICSERV_sid){hints->ai_flags|=AI_NUMERICSERV;goto done;}
			if(id==fpd->AI_CANONNAME_sid){hints->ai_flags|=AI_CANONNAME;goto done;}
			return FKL_ERR_INVALID_VALUE;
		}
		else
			return FKL_ERR_INCORRECT_TYPE_VALUE;
done:
		cur=FKL_VM_POP_ARG(exe);
	}
	return 0;
}

static inline FklVMvalue* af_num_to_symbol(int ai_family,FuvPublicData* fpd)
{
#ifdef AF_UNIX
	if(ai_family==AF_UNIX)return FKL_MAKE_VM_SYM(fpd->AF_UNIX_sid);
#endif
#ifdef AF_INET
	if(ai_family==AF_INET)return FKL_MAKE_VM_SYM(fpd->AF_INET_sid);
#endif
#ifdef AF_INET6
	if(ai_family==AF_INET6)return FKL_MAKE_VM_SYM(fpd->AF_INET6_sid);
#endif
#ifdef AF_IPX
	if(ai_family==AF_IPX)return FKL_MAKE_VM_SYM(fpd->AF_IPX_sid);
#endif
#ifdef AF_NETLINK
	if(ai_family==AF_NETLINK)return FKL_MAKE_VM_SYM(fpd->AF_NETLINK_sid);
#endif
#ifdef AF_X25
	if(ai_family==AF_X25)return FKL_MAKE_VM_SYM(fpd->AF_X25_sid);
#endif
#ifdef AF_AX25
	if(ai_family==AF_AX25)return FKL_MAKE_VM_SYM(fpd->AF_AX25_sid);
#endif
#ifdef AF_ATMPVC
	if(ai_family==AF_ATMPVC)return FKL_MAKE_VM_SYM(fpd->AF_ATMPVC_sid);
#endif
#ifdef AF_APPLETALK
	if(ai_family==AF_APPLETALK)return FKL_MAKE_VM_SYM(fpd->AF_APPLETALK_sid);
#endif
#ifdef AF_PACKET
	if(ai_family==AF_PACKET)return FKL_MAKE_VM_SYM(fpd->AF_PACKET_sid);
#endif
	return NULL;
}

static inline FklVMvalue* sock_num_to_symbol(int ai_socktype,FuvPublicData* fpd)
{
#ifdef SOCK_STREAM
	if(ai_socktype==SOCK_STREAM)return FKL_MAKE_VM_SYM(fpd->SOCK_STREAM_sid);
#endif
#ifdef SOCK_DGRAM
	if(ai_socktype==SOCK_DGRAM)return FKL_MAKE_VM_SYM(fpd->SOCK_DGRAM_sid);
#endif
#ifdef SOCK_SEQPACKET
	if(ai_socktype==SOCK_SEQPACKET)return FKL_MAKE_VM_SYM(fpd->SOCK_SEQPACKET_sid);
#endif
#ifdef SOCK_RAW
	if(ai_socktype==SOCK_RAW)return FKL_MAKE_VM_SYM(fpd->SOCK_RAW_sid);
#endif
#ifdef SOCK_RDM
	if(ai_socktype==SOCK_RDM)return FKL_MAKE_VM_SYM(fpd->SOCK_RDM_sid);
#endif
	return NULL;
}

static inline FklVMvalue* proto_num_to_symbol(int num,FklVM* exe)
{
	struct protoent* proto=getprotobynumber(num);
	if(proto)
		return FKL_MAKE_VM_SYM(fklVMaddSymbolCstr(exe->gc,proto->p_name)->id);
	return NULL;
}

static inline FklVMvalue* addrinfo_to_vmhash(FklVM* exe
		,struct addrinfo* info
		,FuvPublicData* fpd)
{
	char ip[INET6_ADDRSTRLEN];
	FklVMvalue* v=fklCreateVMvalueHashEq(exe);
	FklHashTable* ht=FKL_VM_HASH(v);
	const char* addr=NULL;
	int port=0;
	if(info->ai_family==AF_INET||info->ai_family==AF_INET6)
	{
		addr=(const char*)&((struct sockaddr_in*)info->ai_addr)->sin_addr;
		port=((struct sockaddr_in*)info->ai_addr)->sin_port;
	}
	else
	{
		addr=(const char*)&((struct sockaddr_in6*)info->ai_addr)->sin6_addr;
		port=((struct sockaddr_in6*)info->ai_addr)->sin6_port;
	}
	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->f_family_sid)
			,af_num_to_symbol(info->ai_family,fpd)
			,ht);
	uv_inet_ntop(info->ai_family,addr,ip,INET6_ADDRSTRLEN);

	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->f_addr_sid)
			,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(ip))
			,ht);

	if(ntohs(port))
		fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->f_port_sid)
				,FKL_MAKE_VM_FIX(ntohs(port))
				,ht);
	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->f_socktype_sid)
			,sock_num_to_symbol(info->ai_socktype,fpd)
			,ht);
	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->f_protocol_sid)
			,proto_num_to_symbol(info->ai_protocol,exe)
			,ht);
	if(info->ai_canonname)
		fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->f_canonname_sid)
				,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(info->ai_canonname))
				,ht);
	return v;
}

static inline FklVMvalue* addrinfo_to_value(FklVM* exe
		,struct addrinfo* info
		,FuvPublicData* fpd)
{
	FklVMvalue* r=FKL_VM_NIL;
	FklVMvalue** pr=&r;
	for(;info;info=info->ai_next)
	{
		*pr=fklCreateVMvaluePairWithCar(exe,addrinfo_to_vmhash(exe,info,fpd));
		pr=&FKL_VM_CDR(*pr);
	}
	return r;
}

static void fuv_call_req_callback_in_loop_with_value_creator(uv_req_t* req
		,CallbackValueCreator creator
		,void* arg)
{
	FuvReq* fuv_req=uv_req_get_data((uv_req_t*)req);
	FklVMvalue* proc=fuv_req->data.callback;
	FuvReqData* rdata=&fuv_req->data;
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,rdata->loop);
	FuvLoopData* ldata=&fuv_loop->data;
	FklVM* exe=ldata->exe;
	if(proc==NULL||exe==NULL)
	{
		if(rdata->req)
			uninitFuvReqValue(rdata->req);
		destroy_req(fuv_req);
		return;
	}
	fklLockThread(exe);

	exe->state=FKL_VM_READY;
	uint32_t sbp=exe->bp;
	uint32_t stp=exe->tp;
	uint32_t ltp=exe->ltp;
	FklVMframe* buttom_frame=exe->top_frame;
	fklSetBp(exe);
	if(creator)
		creator(exe,arg);
	uninitFuvReqValue(rdata->req);
	destroy_req(fuv_req);
	fklCallObj(exe,proc);
	if(exe->thread_run_cb(exe,buttom_frame))
		startErrorHandle(&fuv_loop->loop,ldata,exe,sbp,stp,ltp,buttom_frame);
	else
		exe->tp--;
	fklUnlockThread(exe);
}

struct GetaddrinfoValueCreateArg
{
	struct addrinfo* res;
	int status;
};

static void fuv_getaddrinfo_cb_value_creator(FklVM* exe,void* a)
{
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)exe->top_frame->data)->pd;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,fpd_obj);

	struct GetaddrinfoValueCreateArg* arg=a;
	FklVMvalue* err=arg->status<0
		?createUvErrorWithFpd(arg->status,exe,fpd)
		:FKL_VM_NIL;
	FklVMvalue* res=addrinfo_to_value(exe,arg->res,fpd);
	FKL_VM_PUSH_VALUE(exe,res);
	FKL_VM_PUSH_VALUE(exe,err);
}

static void fuv_getaddrinfo_cb(uv_getaddrinfo_t* req
		,int status
		,struct addrinfo* res)
{
	struct GetaddrinfoValueCreateArg arg=
	{
		.res=res,
		.status=status,
	};
	fuv_call_req_callback_in_loop_with_value_creator((uv_req_t*)req
			,fuv_getaddrinfo_cb_value_creator
			,&arg);
	uv_freeaddrinfo(res);
}

static int fuv_getaddrinfo(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(loop_obj,node_obj,service_obj,exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FklVMvalue* proc_obj=FKL_VM_POP_ARG(exe);
	const char* node=NULL;
	const char* service=NULL;
	if(FKL_IS_STR(node_obj))
		node=FKL_VM_STR(node_obj)->str;
	else if(node_obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	if(FKL_IS_STR(service_obj))
		service=FKL_VM_STR(service_obj)->str;
	else if(service_obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	if(proc_obj&&proc_obj!=FKL_VM_NIL&&!fklIsCallable(proc_obj))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	struct addrinfo hints={.ai_flags=0,};
	FklBuiltinErrorType err_type=pop_addrinfo_hints(exe,&hints,fpd);
	if(err_type)
		FKL_RAISE_BUILTIN_ERROR(err_type,exe);

	fklResBp(exe);

	if(hints.ai_flags&AI_NUMERICSERV&&service==NULL)
		service="00";

	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	if(!proc_obj||proc_obj==FKL_VM_NIL)
	{
		uv_getaddrinfo_t req;
		uint32_t rtp=exe->tp;
		FKL_VM_PUSH_VALUE(exe,loop_obj);
		FKL_VM_PUSH_VALUE(exe,node_obj);
		FKL_VM_PUSH_VALUE(exe,service_obj);
		fklUnlockThread(exe);
		int r=uv_getaddrinfo(&fuv_loop->loop,&req,NULL,node,service,&hints);
		fklLockThread(exe);
		CHECK_UV_RESULT(r,exe,ctx->pd);
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,addrinfo_to_value(exe,req.addrinfo,fpd));
		uv_freeaddrinfo(req.addrinfo);
	}
	else
	{
		FklVMvalue* retval=NULL;
		uv_getaddrinfo_t* req=createFuvGetaddrinfo(exe,&retval,ctx->proc,loop_obj,proc_obj);
		int r=uv_getaddrinfo(&fuv_loop->loop,req,fuv_getaddrinfo_cb,node,service,&hints);
		CHECK_UV_RESULT_AND_CLEANUP_REQ(r,retval,exe,ctx->pd);
		FKL_VM_PUSH_VALUE(exe,retval);
	}
	return 0;
}

static int fuv_getaddrinfo_p(FKL_CPROC_ARGL){PREDICATE(isFuvGetaddrinfo(val))}
static int fuv_getnameinfo_p(FKL_CPROC_ARGL){PREDICATE(isFuvGetnameinfo(val))}
static int fuv_write_p(FKL_CPROC_ARGL){PREDICATE(isFuvWrite(val))}
static int fuv_shutdown_p(FKL_CPROC_ARGL){PREDICATE(isFuvShutdown(val))}
static int fuv_connect_p(FKL_CPROC_ARGL){PREDICATE(isFuvConnect(val))}
static int fuv_udp_send_p(FKL_CPROC_ARGL){PREDICATE(isFuvUdpSend(val))}
static int fuv_fs_req_p(FKL_CPROC_ARGL){PREDICATE(isFuvFsReq(val))}

static inline FklBuiltinErrorType pop_sockaddr_flags(FklVM* exe
		,struct sockaddr_storage* addr
		,int* flags
		,FuvPublicData* fpd
		,FklVMvalue** pip_obj
		,FklVMvalue** pport_obj
		,int* uv_err)
{
	const char* ip=NULL;
	int port=0;
	int has_af=0;
	int af_num=0;
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	while(cur)
	{
		if(FKL_IS_SYM(cur))
		{
			FklSid_t id=FKL_GET_SYM(cur);
			if(id==fpd->f_family_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(!FKL_IS_SYM(cur))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				FklSid_t family_id=FKL_GET_SYM(cur);
				af_num=sid_to_af_name(family_id,fpd);
				if(af_num<0)
					return FKL_ERR_INVALID_VALUE;
				has_af=1;
				goto done;
			}
			if(id==fpd->f_ip_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(!FKL_IS_STR(cur))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				ip=FKL_VM_STR(cur)->str;
				*pip_obj=cur;
				goto done;
			}
			if(id==fpd->f_port_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(!FKL_IS_FIX(cur))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				port=FKL_GET_FIX(cur);
				*pport_obj=cur;
				goto done;
			}
#ifdef NI_NAMEREQD
			if(id==fpd->NI_NAMEREQD_sid){(*flags)|=NI_NAMEREQD;goto done;}
#endif
#ifdef NI_DGRAM
			if(id==fpd->NI_DGRAM_sid){(*flags)|=NI_DGRAM;goto done;}
#endif
#ifdef NI_NOFQDN
			if(id==fpd->NI_NOFQDN_sid){(*flags)|=NI_NOFQDN;goto done;}
#endif
#ifdef NI_NUMERICHOST
			if(id==fpd->NI_NUMERICHOST_sid){(*flags)|=NI_NUMERICHOST;goto done;}
#endif
#ifdef NI_NUMERICSERV
			if(id==fpd->NI_NUMERICSERV_sid){(*flags)|=NI_NUMERICSERV;goto done;}
#endif
#ifdef NI_IDN
			if(id==fpd->NI_IDN_sid){(*flags)|=NI_IDN;goto done;}
#endif
#ifdef NI_IDN_ALLOW_UNASSIGNED
			if(id==fpd->NI_IDN_ALLOW_UNASSIGNED_sid){(*flags)|=NI_IDN_ALLOW_UNASSIGNED;goto done;}
#endif
#ifdef NI_IDN_USE_STD3_ASCII_RULES
			if(id==fpd->NI_IDN_USE_STD3_ASCII_RULES_sid){(*flags)|=NI_IDN_USE_STD3_ASCII_RULES;goto done;}
#endif
			return FKL_ERR_INVALID_VALUE;
		}
		else
			return FKL_ERR_INCORRECT_TYPE_VALUE;
done:
		cur=FKL_VM_POP_ARG(exe);
	}
	if(ip||port)
	{
		if(!ip)ip="0.0.0.0";
		int r=0;
		if(!(r=uv_ip4_addr(ip,port,(struct sockaddr_in*)addr)))
		{
			if(!has_af)
				addr->ss_family=AF_INET;
		}
		else if(!(r=uv_ip6_addr(ip,port,(struct sockaddr_in6*)addr)))
		{
			if(!has_af)
				addr->ss_family=AF_INET6;
		}
		*uv_err=r;
	}
	if(has_af)
		addr->ss_family=af_num;
	return 0;
}

static inline FklVMvalue* host_service_to_hash(FklVM* exe
		,const char* hostname
		,const char* service
		,FuvPublicData* fpd)
{
	FklVMvalue* hash=fklCreateVMvalueHashEq(exe);
	FklHashTable* ht=FKL_VM_HASH(hash);
	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->f_hostname_sid)
			,hostname?fklCreateVMvalueStr(exe,fklCreateStringFromCstr(hostname)):FKL_VM_NIL
			,ht);
	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->f_service_sid)
			,service?fklCreateVMvalueStr(exe,fklCreateStringFromCstr(service)):FKL_VM_NIL
			,ht);
	return hash;
}

struct GetnameinfoValueCreateArg
{
	const char* hostname;
	const char* service;
	int status;
};

static void fuv_getnameinfo_cb_value_creator(FklVM* exe,void* a)
{
	struct GetnameinfoValueCreateArg* arg=a;
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)exe->top_frame->data)->pd;

	FklVMvalue* err=arg->status<0
		?createUvError(arg->status,exe,fpd_obj)
		:FKL_VM_NIL;
	FklVMvalue* hostname=arg->hostname?fklCreateVMvalueStr(exe,fklCreateStringFromCstr(arg->hostname)):FKL_VM_NIL;
	FklVMvalue* service=arg->service?fklCreateVMvalueStr(exe,fklCreateStringFromCstr(arg->service)):FKL_VM_NIL;
	FKL_VM_PUSH_VALUE(exe,service);
	FKL_VM_PUSH_VALUE(exe,hostname);
	FKL_VM_PUSH_VALUE(exe,err);
}

static void fuv_getnameinfo_cb(uv_getnameinfo_t* req
		,int status
		,const char* hostname
		,const char* service)
{
	struct GetnameinfoValueCreateArg arg=
	{
		.hostname=hostname,
		.service=service,
		.status=status,
	};
	fuv_call_req_callback_in_loop_with_value_creator((uv_req_t*)req
			,fuv_getnameinfo_cb_value_creator
			,&arg);
}

static int fuv_getnameinfo(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FklVMvalue* proc_obj=FKL_VM_POP_ARG(exe);
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	struct sockaddr_storage addr={.ss_family=AF_UNSPEC};
	int flags=0;

	FklVMvalue* ip_obj=FKL_VM_NIL;
	FklVMvalue* port_obj=FKL_VM_NIL;
	int r=0;
	FklBuiltinErrorType error_type=pop_sockaddr_flags(exe,&addr,&flags,fpd,&ip_obj,&port_obj,&r);
	if(error_type)
		FKL_RAISE_BUILTIN_ERROR(error_type,exe);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	fklResBp(exe);

	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	if(!proc_obj||proc_obj==FKL_VM_NIL)
	{
		uv_getnameinfo_t req;
		uint32_t rtp=exe->tp;
		FKL_VM_PUSH_VALUE(exe,loop_obj);
		FKL_VM_PUSH_VALUE(exe,ip_obj);
		FKL_VM_PUSH_VALUE(exe,port_obj);
		fklUnlockThread(exe);
		int r=uv_getnameinfo(&fuv_loop->loop,&req,NULL,(struct sockaddr*)&addr,flags);
		fklLockThread(exe);
		CHECK_UV_RESULT(r,exe,ctx->pd);
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,host_service_to_hash(exe,req.host,req.service,fpd));
	}
	else
	{
		FklVMvalue* retval=NULL;
		uv_getnameinfo_t* req=createFuvGetnameinfo(exe,&retval,ctx->proc,loop_obj,proc_obj);
		int r=uv_getnameinfo(&fuv_loop->loop,req,fuv_getnameinfo_cb,(struct sockaddr*)&addr,flags);
		CHECK_UV_RESULT_AND_CLEANUP_REQ(r,retval,exe,ctx->pd);
		FKL_VM_PUSH_VALUE(exe,retval);
	}
	return 0;
}

static int fuv_process_p(FKL_CPROC_ARGL){PREDICATE(isFuvProcess(val))}

static inline int is_string_list_and_get_len(const FklVMvalue* p,uint64_t* plen)
{
	uint64_t len=0;
	for(;p!=FKL_VM_NIL;p=FKL_VM_CDR(p))
	{
		if(!FKL_IS_PAIR(p)||!FKL_IS_STR(FKL_VM_CAR(p)))
			return 0;
		len++;
	}
	*plen=len;
	return 1;
}

static inline FklBuiltinErrorType pop_process_options(FklVM* exe
		,uv_process_options_t* options
		,FuvPublicData* fpd
		,int* uv_err
		,FklVMvalue** args_obj
		,FklVMvalue** env_obj
		,FklVMvalue** stdio_obj
		,FklVMvalue** cwd_obj
		,FuvErrorType* fuv_err)
{
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	while(cur)
	{
		if(FKL_IS_SYM(cur))
		{
			FklSid_t id=FKL_GET_SYM(cur);
			if(id==fpd->f_args_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				uint64_t len=0;
				if(!is_string_list_and_get_len(cur,&len))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				free(options->args);
				char** args=(char**)calloc(len+1,sizeof(char*));
				FKL_ASSERT(args);
				*args_obj=cur;
				for(uint64_t i=0;i<len;i++)
				{
					args[i]=FKL_VM_STR(FKL_VM_CAR(cur))->str;
					cur=FKL_VM_CDR(cur);
				}
				options->args=args;
				goto done;
			}
			if(id==fpd->f_env_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				uint64_t len=0;
				if(!is_string_list_and_get_len(cur,&len))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				free(options->env);
				char** env=(char**)calloc(len+1,sizeof(char*));
				FKL_ASSERT(env);
				*env_obj=cur;
				for(uint64_t i=0;i<len;i++)
				{
					env[i]=FKL_VM_STR(FKL_VM_CAR(cur))->str;
					cur=FKL_VM_CDR(cur);
				}
				options->env=env;
				goto done;
			}
			if(id==fpd->f_cwd_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(!FKL_IS_STR(cur))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				options->cwd=FKL_VM_STR(cur)->str;
				*cwd_obj=cur;
				goto done;
			}
			if(id==fpd->f_stdio_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(!fklIsList(cur))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				free(options->stdio);
				uint64_t len=fklVMlistLength(cur);
				options->stdio_count=len;
				*stdio_obj=cur;
				if(len==0)
					goto done;
				uv_stdio_container_t* containers=(uv_stdio_container_t*)malloc(len*sizeof(*(options->stdio)));
				FKL_ASSERT(containers);
				options->stdio=containers;
				for(uint64_t i=0;i<len;i++)
				{
					FklVMvalue* stream=FKL_VM_CAR(cur);
					if(stream==FKL_VM_NIL)
						containers[i].flags=UV_IGNORE;
					else if(FKL_IS_FIX(stream))
					{
						int fd=FKL_GET_FIX(stream);
						containers[i].data.fd=fd;
						containers[i].flags=UV_INHERIT_FD;
					}
					else if(FKL_IS_FP(stream))
					{
						FklVMfp* fp=FKL_VM_FP(stream);
						int fd=fklVMfpFileno(fp);
						containers[i].data.fd=fd;
						containers[i].flags=UV_INHERIT_FD;
					}
					else if(isFuvStream(stream))
					{
						FKL_DECL_VM_UD_DATA(s,FuvHandleUd,stream);
						if(*s==NULL)
						{
							*fuv_err=FUV_ERR_HANDLE_CLOSED;
							return 0;
						}
						uv_os_fd_t fd;
						uv_stream_t* ss=(uv_stream_t*)GET_HANDLE(*s);
						int err=uv_fileno((uv_handle_t*)ss,&fd);
						if(err)
						{
							int flags=UV_CREATE_PIPE;
							if(i==0||i>2)
								flags|=UV_READABLE_PIPE;
							if(i>0)
								flags|=UV_WRITABLE_PIPE;
							containers[i].flags=(uv_stdio_flags)flags;
						}
						else
							containers[i].flags=UV_INHERIT_STREAM;
						containers[i].data.stream=ss;
					}
					else
						return FKL_ERR_INCORRECT_TYPE_VALUE;
					cur=FKL_VM_CDR(cur);
				}
				goto done;
			}
			if(id==fpd->f_uid_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(!FKL_IS_FIX(cur))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				options->flags|=UV_PROCESS_SETUID;
				options->uid=FKL_GET_FIX(cur);
				goto done;
			}
			if(id==fpd->f_gid_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(!FKL_IS_FIX(cur))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				options->flags|=UV_PROCESS_SETUID;
				options->gid=FKL_GET_FIX(cur);
				goto done;
			}
			if(id==fpd->UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS_sid)
			{
				options->flags|=UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS;
				goto done;
			}
			if(id==fpd->UV_PROCESS_DETACHED_sid)
			{
				options->flags|=UV_PROCESS_DETACHED;
				goto done;
			}
			if(id==fpd->UV_PROCESS_WINDOWS_HIDE_sid)
			{
				options->flags|=UV_PROCESS_WINDOWS_HIDE;
				goto done;
			}
			if(id==fpd->UV_PROCESS_WINDOWS_HIDE_CONSOLE_sid)
			{
				options->flags|=UV_PROCESS_WINDOWS_HIDE_CONSOLE;
				goto done;
			}
			if(id==fpd->UV_PROCESS_WINDOWS_HIDE_GUI_sid)
			{
				options->flags|=UV_PROCESS_WINDOWS_HIDE_GUI;
				goto done;
			}
			return FKL_ERR_INVALID_VALUE;
		}
		else
			return FKL_ERR_INCORRECT_TYPE_VALUE;
done:
		cur=FKL_VM_POP_ARG(exe);
	}
	return 0;
}

static inline void clean_options(uv_process_options_t* options)
{
	free(options->args);
	free(options->env);
	free(options->stdio);
}

struct ProcessExitValueCreateArg
{
	FuvPublicData* fpd;
	int64_t exit_status;
	int term_signal;
};

static void fuv_process_exit_cb_value_creator(FklVM* exe,void* a)
{
	struct ProcessExitValueCreateArg* arg=a;
	FklSid_t id=signumToSymbol(arg->term_signal,arg->fpd);
	FklVMvalue* signum_val=id?FKL_MAKE_VM_SYM(id):FKL_VM_NIL;
	FklVMvalue* exit_status=fklMakeVMint(arg->exit_status,exe);
	FKL_VM_PUSH_VALUE(exe,signum_val);
	FKL_VM_PUSH_VALUE(exe,exit_status);
}

static void fuv_process_exit_cb(uv_process_t* handle,int64_t exit_status,int term_signal)
{
	FuvLoopData* ldata=uv_loop_get_data(uv_handle_get_loop((uv_handle_t*)handle));
	FuvHandleData* hdata=&((FuvHandle*)uv_handle_get_data((uv_handle_t*)handle))->data;
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)ldata->exe->top_frame->data)->pd;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,fpd_obj);
	struct ProcessExitValueCreateArg arg=
	{
		.fpd=fpd,
		.term_signal=term_signal,
		.exit_status=exit_status,
	};
	fuv_call_handle_callback_in_loop_with_value_creator((uv_handle_t*)handle
			,hdata
			,ldata
			,0
			,fuv_process_exit_cb_value_creator
			,&arg);
}

static int fuv_process_spawn(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(loop_obj,file_obj,exit_cb_obj,exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_CHECK_TYPE(file_obj,FKL_IS_STR,exe);
	FKL_CHECK_TYPE(exit_cb_obj,fklIsCallable,exe);
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	uv_process_options_t options=
	{
		.file=FKL_VM_STR(file_obj)->str,
		.exit_cb=fuv_process_exit_cb,
	};
	int uv_err=0;
	FuvErrorType fuv_err=0;
	FklVMvalue* env_obj=NULL;
	FklVMvalue* args_obj=NULL;
	FklVMvalue* stdio_obj=NULL;
	FklVMvalue* cwd_obj=NULL;
	FklBuiltinErrorType fkl_err=pop_process_options(exe
			,&options
			,fpd
			,&uv_err
			,&env_obj
			,&args_obj
			,&stdio_obj
			,&cwd_obj
			,&fuv_err);
	CHECK_UV_RESULT(uv_err,exe,ctx->pd);
	if(fkl_err)
		FKL_RAISE_BUILTIN_ERROR(fkl_err,exe);
	if(fuv_err)
		raiseFuvError(fuv_err,exe,ctx->pd);
	fklResBp(exe);
	FklVMvalue* retval=NULL;
	uv_process_t* handle=createFuvProcess(exe
			,&retval
			,ctx->proc
			,loop_obj
			,exit_cb_obj
			,args_obj
			,env_obj
			,file_obj
			,stdio_obj
			,cwd_obj);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	uv_err=uv_spawn(&fuv_loop->loop,handle,&options);
	clean_options(&options);
	CHECK_UV_RESULT_AND_CLEANUP_HANDLE(uv_err,retval,loop_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int fuv_disable_stdio_inheritance(FKL_CPROC_ARGL)
{
	FKL_CHECK_REST_ARG(exe);
	uv_disable_stdio_inheritance();
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fuv_kill(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(pid_obj,signum_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(pid_obj,FKL_IS_FIX,exe);
	int signum=0;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	if(FKL_IS_FIX(signum_obj))
		signum=FKL_GET_FIX(signum_obj);
	else if(FKL_IS_SYM(signum_obj))
		signum=symbolToSignum(FKL_GET_SYM(signum_obj),fpd);
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(signum<=0)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	int pid=FKL_GET_FIX(pid_obj);
	int r=uv_kill(pid,signum);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fuv_process_kill(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(process_obj,signum_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(process_obj,isFuvProcess,exe);
	int signum=0;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	if(FKL_IS_FIX(signum_obj))
		signum=FKL_GET_FIX(signum_obj);
	else if(FKL_IS_SYM(signum_obj))
		signum=symbolToSignum(FKL_GET_SYM(signum_obj),fpd);
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(signum<=0)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(process_ud,process_obj,exe,ctx->pd);
	uv_process_t* process=(uv_process_t*)&(*process_ud)->handle;
	int r=uv_process_kill(process,signum);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fuv_process_pid(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(process_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(process_obj,isFuvProcess,exe);

	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(process_ud,process_obj,exe,ctx->pd);
	uv_process_t* process=(uv_process_t*)GET_HANDLE(*process_ud);
	int r=uv_process_get_pid(process);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(r));
	return 0;
}

static int fuv_stream_p(FKL_CPROC_ARGL){PREDICATE(isFuvStream(val))}

static int fuv_stream_readable_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(stream_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(stream_obj,isFuvStream,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud,stream_obj,exe,ctx->pd);
	uv_stream_t* stream=(uv_stream_t*)GET_HANDLE(*stream_ud);
	FKL_VM_PUSH_VALUE(exe,uv_is_readable(stream)?FKL_VM_TRUE:FKL_VM_NIL);
	return 0;
}

static int fuv_stream_writable_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(stream_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(stream_obj,isFuvStream,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud,stream_obj,exe,ctx->pd);
	uv_stream_t* stream=(uv_stream_t*)GET_HANDLE(*stream_ud);
	FKL_VM_PUSH_VALUE(exe,uv_is_writable(stream)?FKL_VM_TRUE:FKL_VM_NIL);
	return 0;
}

static int fuv_stream_write_queue_size(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(stream_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(stream_obj,isFuvStream,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud,stream_obj,exe,ctx->pd);
	uv_stream_t* stream=(uv_stream_t*)GET_HANDLE(*stream_ud);
	uint64_t size=uv_stream_get_write_queue_size(stream);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(size,exe));
	return 0;
}

static void fuv_alloc_cb(uv_handle_t* handle,size_t suggested_size,uv_buf_t* buf)
{
	char* base=(char*)malloc(suggested_size);
	FKL_ASSERT(base||!suggested_size);
	buf->base=base;
	buf->len=suggested_size;
}

struct ReadValueCreateArg
{
	FuvPublicData* fpd;
	ssize_t nread;
	const uv_buf_t* buf;
};

static void fuv_read_cb_value_creator(FklVM* exe,void* a)
{
	struct ReadValueCreateArg* arg=(struct ReadValueCreateArg*)a;
	ssize_t nread=arg->nread;
	if(nread<0)
	{
		FklVMvalue* err=createUvErrorWithFpd(nread,exe,arg->fpd);
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		FKL_VM_PUSH_VALUE(exe,err);
	}
	else
	{
		const uv_buf_t* buf=arg->buf;
		if(nread>0)
			FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueStr(exe,fklCreateString(nread,buf->base)));
		else
			FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	free(arg->buf->base);
}

static void fuv_read_cb(uv_stream_t* stream,ssize_t nread,const uv_buf_t* buf)
{
	FuvLoopData* ldata=uv_loop_get_data(uv_handle_get_loop((uv_handle_t*)stream));
	FuvHandleData* hdata=&((FuvHandle*)uv_handle_get_data((uv_handle_t*)stream))->data;
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)ldata->exe->top_frame->data)->pd;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,fpd_obj);
	struct ReadValueCreateArg arg=
	{
		.fpd=fpd,
		.nread=nread,
		.buf=buf,
	};
	fuv_call_handle_callback_in_loop_with_value_creator((uv_handle_t*)stream
			,hdata
			,ldata
			,0
			,fuv_read_cb_value_creator
			,&arg);
}

static int fuv_stream_read_start(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(stream_obj,cb_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(stream_obj,isFuvStream,exe);
	FKL_CHECK_TYPE(cb_obj,fklIsCallable,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud,stream_obj,exe,ctx->pd);
	FuvHandle* handle=*stream_ud;
	uv_stream_t* stream=(uv_stream_t*)GET_HANDLE(handle);
	handle->data.callbacks[0]=cb_obj;
	int ret=uv_read_start(stream,fuv_alloc_cb,fuv_read_cb);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,stream_obj);
	return 0;
}

static int fuv_stream_read_stop(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(stream_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(stream_obj,isFuvStream,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud,stream_obj,exe,ctx->pd);
	uv_stream_t* stream=(uv_stream_t*)GET_HANDLE(*stream_ud);
	int ret=uv_read_stop(stream);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,stream_obj);
	return 0;
}

static void fuv_req_cb_error_value_creator(FklVM* exe,void* a)
{
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)exe->top_frame->data)->pd;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,fpd_obj);

	int status=*(int*)a;
	FklVMvalue* err=status<0
		?createUvErrorWithFpd(status,exe,fpd)
		:FKL_VM_NIL;
	FKL_VM_PUSH_VALUE(exe,err);
}

static void fuv_write_cb(uv_write_t* req,int status)
{
	fuv_call_req_callback_in_loop_with_value_creator((uv_req_t*)req,fuv_req_cb_error_value_creator,&status);
}

static inline FklBuiltinErrorType setup_write_data(FklVM* exe
		,uint32_t num
		,FklVMvalue* write_obj
		,uv_buf_t** pbufs)
{
	FKL_DECL_VM_UD_DATA(fuv_req,FuvReqUd,write_obj);
	struct FuvWrite* req=(struct FuvWrite*)*fuv_req;
	FklVMvalue** cur=req->write_objs;
	uv_buf_t* bufs=(uv_buf_t*)malloc(num*sizeof(uv_buf_t));
	FKL_ASSERT(bufs);
	FklVMvalue* val=FKL_VM_POP_ARG(exe);
	uint32_t i=0;
	while(val)
	{
		if(FKL_IS_STR(val))
		{
			*cur=val;
			FklString* str=FKL_VM_STR(val);
			bufs[i].base=str->str;
			bufs[i].len=str->size;
		}
		else if(FKL_IS_BYTEVECTOR(val))
		{
			*cur=val;
			FklBytevector* bvec=FKL_VM_BVEC(val);
			bufs[i].base=(char*)bvec->ptr;
			bufs[i].len=bvec->size;
		}
		else
		{
			free(bufs);
			return FKL_ERR_INCORRECT_TYPE_VALUE;
		}
		val=FKL_VM_POP_ARG(exe);
		i++;
		cur++;
	}
	*pbufs=bufs;
	return 0;
}

static int fuv_stream_write(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(stream_obj,cb_obj,exe);
	FKL_CHECK_TYPE(stream_obj,isFuvStream,exe);
	if(cb_obj==FKL_VM_NIL)
		cb_obj=NULL;
	else if(!fklIsCallable(cb_obj))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud,stream_obj,exe,ctx->pd);
	uint32_t arg_num=FKL_VM_GET_ARG_NUM(exe);
	FuvHandle* handle=*stream_ud;
	FklVMvalue* write_obj=NULL;
	uv_write_t* write=NULL;
	FklVMvalue* send_handle_obj=NULL;
	uv_buf_t* bufs=NULL;
	if(arg_num)
	{
		write=createFuvWrite(exe,&write_obj,ctx->proc,handle->data.loop,cb_obj,arg_num);
		FklVMvalue* top_value=FKL_VM_POP_TOP_VALUE(exe);
		if(isFuvStream(top_value))
		{
			arg_num--;
			send_handle_obj=top_value;
		}
		else
			FKL_VM_PUSH_VALUE(exe,top_value);
		FklBuiltinErrorType err_type=setup_write_data(exe,arg_num,write_obj,&bufs);
		if(err_type)
			FKL_RAISE_BUILTIN_ERROR(err_type,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
	fklResBp(exe);
	uv_stream_t* stream=(uv_stream_t*)GET_HANDLE(handle);
	int ret=0;
	if(send_handle_obj)
	{
		FKL_DECL_VM_UD_DATA(send_handle_ud,FuvHandleUd,send_handle_obj);
		if(*send_handle_ud==NULL)
		{
			free(bufs);
			raiseFuvError(FUV_ERR_HANDLE_CLOSED,exe,ctx->pd);
		}
		ret=uv_write2(write,stream,bufs,arg_num,(uv_stream_t*)GET_HANDLE(*send_handle_ud),fuv_write_cb);
	}
	else
		ret=uv_write(write,stream,bufs,arg_num,fuv_write_cb);
	free(bufs);
	CHECK_UV_RESULT_AND_CLEANUP_REQ(ret,write_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,write_obj);
	return 0;
}

static inline FklBuiltinErrorType setup_try_write_data(FklVM* exe
		,uint32_t num
		,uv_buf_t** pbufs)
{
	uv_buf_t* bufs=(uv_buf_t*)malloc(num*sizeof(uv_buf_t));
	FKL_ASSERT(bufs);
	FklVMvalue* val=FKL_VM_POP_ARG(exe);
	uint32_t i=0;
	while(val)
	{
		if(FKL_IS_STR(val))
		{
			FklString* str=FKL_VM_STR(val);
			bufs[i].base=str->str;
			bufs[i].len=str->size;
		}
		else if(FKL_IS_BYTEVECTOR(val))
		{
			FklBytevector* bvec=FKL_VM_BVEC(val);
			bufs[i].base=(char*)bvec->ptr;
			bufs[i].len=bvec->size;
		}
		else
		{
			free(bufs);
			return FKL_ERR_INCORRECT_TYPE_VALUE;
		}
		val=FKL_VM_POP_ARG(exe);
		i++;
	}
	*pbufs=bufs;
	return 0;
}

static int fuv_stream_try_write(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(stream_obj,exe);
	FKL_CHECK_TYPE(stream_obj,isFuvStream,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud,stream_obj,exe,ctx->pd);
	uint32_t arg_num=FKL_VM_GET_ARG_NUM(exe);
	FklVMvalue* send_handle_obj=NULL;
	uv_buf_t* bufs=NULL;
	if(arg_num)
	{
		FklVMvalue* top_value=FKL_VM_POP_TOP_VALUE(exe);
		if(isFuvStream(top_value))
		{
			arg_num--;
			send_handle_obj=top_value;
		}
		else
			FKL_VM_PUSH_VALUE(exe,top_value);
		FklBuiltinErrorType err_type=setup_try_write_data(exe,arg_num,&bufs);
		if(err_type)
			FKL_RAISE_BUILTIN_ERROR(err_type,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
	fklResBp(exe);
	uv_stream_t* stream=(uv_stream_t*)GET_HANDLE(*stream_ud);
	int ret=0;
	if(send_handle_obj)
	{
		FKL_DECL_VM_UD_DATA(send_handle_ud,FuvHandleUd,send_handle_obj);
		if(*send_handle_ud==NULL)
		{
			free(bufs);
			raiseFuvError(FUV_ERR_HANDLE_CLOSED,exe,ctx->pd);
		}
		ret=uv_try_write2(stream,bufs,arg_num,(uv_stream_t*)GET_HANDLE(*send_handle_ud));
	}
	else
		ret=uv_try_write(stream,bufs,arg_num);
	free(bufs);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(ret));
	return 0;
}

static void fuv_shutdown_cb(uv_shutdown_t* req,int status)
{
	fuv_call_req_callback_in_loop_with_value_creator((uv_req_t*)req,fuv_req_cb_error_value_creator,&status);
}

static int fuv_stream_shutdown(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(stream_obj,exe);
	FklVMvalue* cb_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(cb_obj)
		FKL_CHECK_TYPE(cb_obj,fklIsCallable,exe);
	FKL_CHECK_TYPE(stream_obj,isFuvStream,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud,stream_obj,exe,ctx->pd);
	FuvHandle* handle=*stream_ud;
	FklVMvalue* shutdown_obj=NULL;
	uv_shutdown_t* write=createFuvShutdown(exe,&shutdown_obj,ctx->proc,handle->data.loop,cb_obj);
	uv_stream_t* stream=(uv_stream_t*)GET_HANDLE(*stream_ud);
	int ret=uv_shutdown(write,stream,fuv_shutdown_cb);
	CHECK_UV_RESULT_AND_CLEANUP_REQ(ret,shutdown_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,shutdown_obj);
	return 0;
}

struct ConnectionValueCreateArg
{
	FuvPublicData* fpd;
	int status;
};

static void fuv_connection_cb_value_creator(FklVM* exe,void* a)
{
	struct ConnectionValueCreateArg* arg=a;
	FklVMvalue* err=arg->status<0
		?createUvErrorWithFpd(arg->status,exe,arg->fpd)
		:FKL_VM_NIL;
	FKL_VM_PUSH_VALUE(exe,err);
}

static void fuv_connection_cb(uv_stream_t* handle,int status)
{
	FuvLoopData* ldata=uv_loop_get_data(uv_handle_get_loop((uv_handle_t*)handle));
	FuvHandleData* hdata=&((FuvHandle*)uv_handle_get_data((uv_handle_t*)handle))->data;
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)ldata->exe->top_frame->data)->pd;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,fpd_obj);
	struct ConnectionValueCreateArg arg=
	{
		.fpd=fpd,
		.status=status,
	};
	fuv_call_handle_callback_in_loop_with_value_creator((uv_handle_t*)handle
			,hdata
			,ldata
			,0
			,fuv_connection_cb_value_creator
			,&arg);
}

static int fuv_stream_listen(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(server_obj,backlog_obj,cb_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(server_obj,isFuvStream,exe);
	FKL_CHECK_TYPE(backlog_obj,FKL_IS_FIX,exe);
	FKL_CHECK_TYPE(cb_obj,fklIsCallable,exe);

	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(server_ud,server_obj,exe,ctx->pd);
	FuvHandle* handle=*server_ud;
	uv_stream_t* server=(uv_stream_t*)GET_HANDLE(handle);

	handle->data.callbacks[0]=cb_obj;
	int ret=uv_listen(server,FKL_GET_FIX(backlog_obj),fuv_connection_cb);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,server_obj);
	return 0;
}

static int fuv_stream_accept(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(server_obj,client_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(server_obj,isFuvStream,exe);
	FKL_CHECK_TYPE(client_obj,isFuvStream,exe);

	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(server_ud,server_obj,exe,ctx->pd);
	uv_stream_t* server=(uv_stream_t*)GET_HANDLE(*server_ud);

	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(client_ud,client_obj,exe,ctx->pd);
	uv_stream_t* client=(uv_stream_t*)GET_HANDLE(*client_ud);

	int ret=uv_accept(server,client);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,client_obj);
	return 0;
}

static int fuv_stream_blocking_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(stream_obj,set_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(stream_obj,isFuvStream,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(stream_ud,stream_obj,exe,ctx->pd);
	uv_stream_t* stream=(uv_stream_t*)GET_HANDLE(*stream_ud);
	int ret=uv_stream_set_blocking(stream,set_obj==FKL_VM_NIL?0:1);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,stream_obj);
	return 0;
}

static int fuv_pipe_p(FKL_CPROC_ARGL){PREDICATE(isFuvPipe(val))}

static int fuv_make_pipe(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FklVMvalue* ipc_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	int ipc=ipc_obj&&ipc_obj!=FKL_VM_NIL?1:0;
	FklVMvalue* retval=NULL;
	uv_pipe_t* pipe=createFuvPipe(exe,&retval,ctx->proc,loop_obj);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_pipe_init(&fuv_loop->loop,pipe,ipc);
	CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r,retval,loop_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int fuv_pipe(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(read_flags_obj,write_flags_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	int read_flags=0;
	int write_flags=0;
	if(read_flags_obj!=FKL_VM_NIL)
		read_flags|=UV_NONBLOCK_PIPE;
	if(write_flags_obj!=FKL_VM_NIL)
		write_flags|=UV_NONBLOCK_PIPE;
	uv_file fds[2];
	int ret=uv_pipe(fds,read_flags,write_flags);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe,FKL_MAKE_VM_FIX(fds[0]),FKL_MAKE_VM_FIX(fds[1])));
	return 0;
}

static int fuv_pipe_open(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(pipe_obj,fd_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(pipe_obj,isFuvPipe,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,pipe_obj,exe,ctx->pd);
	struct FuvPipe* handle=(struct FuvPipe*)*handle_ud;
	uv_pipe_t* pipe=(uv_pipe_t*)GET_HANDLE(handle);
	int ret=0;
	if(FKL_IS_FP(fd_obj))
	{
		handle->fp=fd_obj;
		ret=uv_pipe_open(pipe,fklVMfpFileno(FKL_VM_FP(fd_obj)));
	}
	else if(FKL_IS_FIX(fd_obj))
		ret=uv_pipe_open(pipe,FKL_GET_FIX(fd_obj));
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,pipe_obj);
	return 0;
}

static int fuv_pipe_bind(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(pipe_obj,name_obj,exe);
	FklVMvalue* no_truncate=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(pipe_obj,isFuvPipe,exe);
	FKL_CHECK_TYPE(name_obj,FKL_IS_STR,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,pipe_obj,exe,ctx->pd);
	FuvHandle* handle=*handle_ud;
	uv_pipe_t* pipe=(uv_pipe_t*)GET_HANDLE(handle);
	FklString* str=FKL_VM_STR(name_obj);
	int flags=no_truncate&&no_truncate!=FKL_VM_NIL?UV_PIPE_NO_TRUNCATE:0;
	int ret=uv_pipe_bind2(pipe,str->str,str->size,flags);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,pipe_obj);
	return 0;
}

static void fuv_connect_cb(uv_connect_t* req,int status)
{
	fuv_call_req_callback_in_loop_with_value_creator((uv_req_t*)req,fuv_req_cb_error_value_creator,&status);
}

static int fuv_pipe_connect(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(pipe_obj,name_obj,cb_obj,exe);
	FklVMvalue* no_truncate=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	if(cb_obj==FKL_VM_NIL)
		cb_obj=NULL;
	else if(!fklIsCallable(cb_obj))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_CHECK_TYPE(pipe_obj,isFuvPipe,exe);
	FKL_CHECK_TYPE(name_obj,FKL_IS_STR,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,pipe_obj,exe,ctx->pd);
	FuvHandle* handle=*handle_ud;
	uv_pipe_t* pipe=(uv_pipe_t*)GET_HANDLE(handle);
	FklVMvalue* connect_obj=NULL;
	uv_connect_t* connect=createFuvConnect(exe,&connect_obj,ctx->proc,handle->data.loop,cb_obj);
	FklString* str=FKL_VM_STR(name_obj);
	int flags=no_truncate&&no_truncate!=FKL_VM_NIL?UV_PIPE_NO_TRUNCATE:0;
	int ret=uv_pipe_connect2(connect,pipe,str->str,str->size,flags,fuv_connect_cb);
	CHECK_UV_RESULT_AND_CLEANUP_REQ(ret,connect_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,connect_obj);
	return 0;
}

static int fuv_pipe_chmod(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(pipe_obj,flags_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(pipe_obj,isFuvPipe,exe);
	FKL_CHECK_TYPE(flags_obj,FKL_IS_STR,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,pipe_obj,exe,ctx->pd);
	FuvHandle* handle=*handle_ud;
	uv_pipe_t* pipe=(uv_pipe_t*)GET_HANDLE(handle);
	int flags=0;
	FklString* str=FKL_VM_STR(flags_obj);
	if(!fklStringCstrCmp(str,"r"))
		flags=UV_READABLE;
	else if(!fklStringCstrCmp(str,"w"))
		flags=UV_WRITABLE;
	else if((!fklStringCstrCmp(str,"rw"))||(!fklStringCstrCmp(str,"wr")))
		flags=UV_READABLE|UV_WRITABLE;
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe,pipe_obj);
	fklUnlockThread(exe);
	int ret=uv_pipe_chmod(pipe,flags);
	fklLockThread(exe);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	return 0;
}

#include<fakeLisp/common.h>

static int fuv_pipe_peername(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(pipe_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(pipe_obj,isFuvPipe,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,pipe_obj,exe,ctx->pd);
	size_t len=2*FKL_PATH_MAX;
	char buf[2*FKL_PATH_MAX];
	int ret=uv_pipe_getpeername((uv_pipe_t*)GET_HANDLE(*handle_ud),buf,&len);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueStr(exe,fklCreateString(len,buf)));
	return 0;
}

static int fuv_pipe_sockname(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(pipe_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(pipe_obj,isFuvPipe,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,pipe_obj,exe,ctx->pd);
	size_t len=2*FKL_PATH_MAX;
	char buf[2*FKL_PATH_MAX];
	int ret=uv_pipe_getsockname((uv_pipe_t*)GET_HANDLE(*handle_ud),buf,&len);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueStr(exe,fklCreateString(len,buf)));
	return 0;
}

static int fuv_pipe_pending_count(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(pipe_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(pipe_obj,isFuvPipe,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,pipe_obj,exe,ctx->pd);
	FuvHandle* handle=*handle_ud;
	uv_pipe_t* pipe=(uv_pipe_t*)GET_HANDLE(handle);
	int ret=uv_pipe_pending_count(pipe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(ret));
	return 0;
}

static int fuv_pipe_pending_type(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(pipe_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(pipe_obj,isFuvPipe,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,pipe_obj,exe,ctx->pd);
	FuvHandle* handle=*handle_ud;
	uv_pipe_t* pipe=(uv_pipe_t*)GET_HANDLE(handle);
	uv_handle_type type_id=uv_pipe_pending_type(pipe);
	const char* name=uv_handle_type_name(type_id);
	FKL_VM_PUSH_VALUE(exe
			,name==NULL
			?FKL_VM_NIL
			:fklCreateVMvaluePair(exe
				,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(name))
				,FKL_MAKE_VM_FIX(type_id)));
	return 0;
}

static int fuv_pipe_pending_instances(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(pipe_obj,count_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(pipe_obj,isFuvPipe,exe);
	FKL_CHECK_TYPE(count_obj,FKL_IS_FIX,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,pipe_obj,exe,ctx->pd);
	FuvHandle* handle=*handle_ud;
	uv_pipe_t* pipe=(uv_pipe_t*)GET_HANDLE(handle);
	uv_pipe_pending_instances(pipe,FKL_GET_FIX(count_obj));
	FKL_VM_PUSH_VALUE(exe,pipe_obj);
	return 0;
}

static int fuv_exepath(FKL_CPROC_ARGL)
{
	FKL_CHECK_REST_ARG(exe);
	size_t size=2*FKL_PATH_MAX;
	char exe_path[2*FKL_PATH_MAX];
	int r=uv_exepath(exe_path,&size);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(exe_path)));
	return 0;
}

static int fuv_guess_handle(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(fd_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(fd_obj,FKL_IS_FIX,exe);
	uv_file fd=FKL_GET_FIX(fd_obj);
	uv_handle_type type_id=uv_guess_handle(fd);
	const char* name=uv_handle_type_name(type_id);
	FKL_VM_PUSH_VALUE(exe
			,name==NULL
			?FKL_VM_NIL
			:fklCreateVMvaluePair(exe
				,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(name))
				,FKL_MAKE_VM_FIX(type_id)));
	return 0;
}

static int fuv_tcp_p(FKL_CPROC_ARGL){PREDICATE(isFuvTcp(val))}

static inline FklBuiltinErrorType pop_tcp_flags(FklVM* exe,FuvPublicData* fpd,unsigned int* flags)
{
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	while(cur)
	{
		if(FKL_IS_SYM(cur))
		{
			int af=sid_to_af_name(FKL_GET_SYM(cur),fpd);
			if(af<0)
				return FKL_ERR_INVALID_VALUE;
			(*flags)&=~0xFF;
			(*flags)|=af;
		}
		else
			return FKL_ERR_INCORRECT_TYPE_VALUE;
		cur=FKL_VM_POP_ARG(exe);
	}
	return 0;
}

static int fuv_make_tcp(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	unsigned int flags=AF_UNSPEC;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	FklBuiltinErrorType err_type=pop_tcp_flags(exe,fpd,&flags);
	if(err_type)
		FKL_RAISE_BUILTIN_ERROR(err_type,exe);
	fklResBp(exe);
	FklVMvalue* retval=NULL;
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	uv_tcp_t* tcp=createFuvTcp(exe,&retval,ctx->proc,loop_obj);
	int r=uv_tcp_init_ex(&fuv_loop->loop,tcp,flags);
	CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r,retval,loop_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,retval);
	return 0;
}

static int fuv_tcp_open(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(tcp_obj,sock_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(tcp_obj,isFuvTcp,exe);
	FKL_CHECK_TYPE(sock_obj,fklIsVMint,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,tcp_obj,exe,ctx->pd);
	uv_os_sock_t sock=fklGetInt(sock_obj);
	int r=uv_tcp_open((uv_tcp_t*)GET_HANDLE(*handle_ud),sock);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,tcp_obj);
	return 0;
}

static int fuv_tcp_nodelay(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(tcp_obj,enable_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(tcp_obj,isFuvTcp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,tcp_obj,exe,ctx->pd);
	int enable=enable_obj!=FKL_VM_NIL;
	int r=uv_tcp_nodelay((uv_tcp_t*)GET_HANDLE(*handle_ud),enable);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,tcp_obj);
	return 0;
}

static int fuv_tcp_keepalive(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(tcp_obj,enable_obj,exe);
	FklVMvalue* delay_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(tcp_obj,isFuvTcp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,tcp_obj,exe,ctx->pd);
	int enable=enable_obj!=FKL_VM_NIL;
	int delay=0;
	if(enable)
	{
		if(delay_obj==NULL)
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
		if(!FKL_IS_FIX(delay_obj))
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		delay=FKL_GET_FIX(delay_obj);
	}
	else if(delay_obj)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOMANYARG,exe);
	int r=uv_tcp_keepalive((uv_tcp_t*)GET_HANDLE(*handle_ud),enable,delay);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,tcp_obj);
	return 0;
}

static int fuv_tcp_simultaneous_accepts(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(tcp_obj,enable_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(tcp_obj,isFuvTcp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,tcp_obj,exe,ctx->pd);
	int enable=enable_obj!=FKL_VM_NIL;
	int r=uv_tcp_simultaneous_accepts((uv_tcp_t*)GET_HANDLE(*handle_ud),enable);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,tcp_obj);
	return 0;
}

static inline FklVMvalue* parse_sockaddr_with_fpd(FklVM* exe
		,struct sockaddr_storage* address
		,FuvPublicData* fpd)
{
	char ip[INET6_ADDRSTRLEN]={0};
	int port=0;
	if(address->ss_family==AF_INET)
	{
		struct sockaddr_in* addrin=(struct sockaddr_in*)address;
		uv_inet_ntop(AF_INET,&(addrin->sin_addr),ip,INET6_ADDRSTRLEN);
		port=ntohs(addrin->sin_port);
	}
	else if(address->ss_family==AF_INET6)
	{
		struct sockaddr_in6* addrin6=(struct sockaddr_in6*)address;
		uv_inet_ntop(AF_INET6,&(addrin6->sin6_addr),ip,INET6_ADDRSTRLEN);
		port=ntohs(addrin6->sin6_port);
	}

	FklVMvalue* hash=fklCreateVMvalueHashEq(exe);
	FklHashTable* ht=FKL_VM_HASH(hash);

	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->f_family_sid)
			,af_num_to_symbol(address->ss_family,fpd)
			,ht);

	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->f_ip_sid)
			,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(ip))
			,ht);

	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->f_port_sid)
			,FKL_MAKE_VM_FIX(port)
			,ht);

	return hash;
}

static inline FklVMvalue* parse_sockaddr(FklVM* exe
		,struct sockaddr_storage* address
		,FklVMvalue* pd)
{
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,pd);
	return parse_sockaddr_with_fpd(exe,address,fpd);
}

static int fuv_tcp_sockname(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(tcp_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(tcp_obj,isFuvTcp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,tcp_obj,exe,ctx->pd);
	struct sockaddr_storage addr;
	int addrlen=sizeof(addr);
	int ret=uv_tcp_getsockname((uv_tcp_t*)GET_HANDLE(*handle_ud),(struct sockaddr*)&addr,&addrlen);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,parse_sockaddr(exe,&addr,ctx->pd));
	return 0;
}

static int fuv_tcp_peername(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(tcp_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(tcp_obj,isFuvTcp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,tcp_obj,exe,ctx->pd);
	struct sockaddr_storage addr;
	int addrlen=sizeof(addr);
	int ret=uv_tcp_getpeername((uv_tcp_t*)GET_HANDLE(*handle_ud),(struct sockaddr*)&addr,&addrlen);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,parse_sockaddr(exe,&addr,ctx->pd));
	return 0;
}

static int fuv_tcp_bind(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(tcp_obj,host_obj,port_obj,exe);
	FklVMvalue* ipv6only_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(tcp_obj,isFuvTcp,exe);
	FKL_CHECK_TYPE(host_obj,FKL_IS_STR,exe);
	FKL_CHECK_TYPE(port_obj,FKL_IS_FIX,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,tcp_obj,exe,ctx->pd);
	int port=FKL_GET_FIX(port_obj);
	const char* host=FKL_VM_STR(host_obj)->str;
	struct sockaddr_storage addr;
	if(uv_ip4_addr(host,port,(struct sockaddr_in*)&addr)
			&&uv_ip6_addr(host,port,(struct sockaddr_in6*)&addr))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	unsigned int flags=ipv6only_obj&&ipv6only_obj!=FKL_VM_NIL?UV_TCP_IPV6ONLY:0;
	int ret=uv_tcp_bind((uv_tcp_t*)GET_HANDLE(*handle_ud),(struct sockaddr*)&addr,flags);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,tcp_obj);
	return 0;
}

static int fuv_tcp_connect(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(tcp_obj,host_obj,port_obj,exe);
	FklVMvalue* cb_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(tcp_obj,isFuvTcp,exe);
	FKL_CHECK_TYPE(host_obj,FKL_IS_STR,exe);
	FKL_CHECK_TYPE(port_obj,FKL_IS_FIX,exe);
	if(cb_obj)
		FKL_CHECK_TYPE(cb_obj,fklIsCallable,exe);
	int port=FKL_GET_FIX(port_obj);
	const char* host=FKL_VM_STR(host_obj)->str;
	struct sockaddr_storage addr;
	if(uv_ip4_addr(host,port,(struct sockaddr_in*)&addr)
			&&uv_ip6_addr(host,port,(struct sockaddr_in6*)&addr))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,tcp_obj,exe,ctx->pd);
	FuvHandle* handle=*handle_ud;
	FklVMvalue* connect_obj=NULL;
	uv_connect_t* connect=createFuvConnect(exe,&connect_obj,ctx->proc,handle->data.loop,cb_obj);
	int ret=uv_tcp_connect(connect,(uv_tcp_t*)GET_HANDLE(*handle_ud),(struct sockaddr*)&addr,fuv_connect_cb);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,connect_obj);
	return 0;
}

static int fuv_tcp_close_reset(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(tcp_obj,exe);
	FklVMvalue* proc_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(tcp_obj,isFuvTcp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,tcp_obj,exe,ctx->pd);
	FuvHandle* handle=*handle_ud;
	uv_tcp_t* tcp=(uv_tcp_t*)GET_HANDLE(handle);
	if(proc_obj)
	{
		FKL_CHECK_TYPE(proc_obj,fklIsCallable,exe);
		handle->data.callbacks[1]=proc_obj;
	}
	int ret=uv_tcp_close_reset(tcp,fuv_close_cb);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,tcp_obj);
	return 0;
}

static int fuv_socketpair(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG4(socktype_obj
			,protocol_obj
			,non_block_flags0_obj
			,non_block_flags1_obj
			,exe);
	FKL_CHECK_REST_ARG(exe);
	int socktype=SOCK_STREAM;
	int protocol=0;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	if(FKL_IS_SYM(socktype_obj))
	{
		int type=sid_to_socktype(FKL_GET_SYM(socktype_obj),fpd);
		if(type<0)
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
		socktype=type;
	}
	else if(socktype_obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	if(FKL_IS_STR(protocol_obj))
	{
		const char* name=FKL_VM_STR(protocol_obj)->str;
		int proto=get_protonum_with_cstr(name);
		if(proto<0)
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
		protocol=proto;
	}
	else if(FKL_IS_SYM(protocol_obj))
	{
		const char* name=fklVMgetSymbolWithId(exe->gc,FKL_GET_SYM(protocol_obj))->symbol->str;
		int proto=get_protonum_with_cstr(name);
		if(proto<0)
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
		protocol=proto;
	}
	else if(protocol_obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	int flags0=non_block_flags0_obj!=FKL_VM_NIL?UV_NONBLOCK_PIPE:0;
	int flags1=non_block_flags1_obj!=FKL_VM_NIL?UV_NONBLOCK_PIPE:0;

	uv_os_sock_t socks[2];
	int ret=uv_socketpair(socktype,protocol,socks,flags0,flags1);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe
			,fklCreateVMvaluePair(exe
				,fklMakeVMint(socks[0],exe)
				,fklMakeVMint(socks[1],exe)));
	return 0;
}

static int fuv_tty_p(FKL_CPROC_ARGL){PREDICATE(isFuvTTY(val))}

static int fuv_make_tty(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(loop_obj,fd_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FklVMvalue* fp_obj=NULL;
	uv_file fd=0;
	if(FKL_IS_FP(fd_obj))
	{
		fp_obj=fd_obj;
		fd=fklVMfpFileno(FKL_VM_FP(fd_obj));
	}
	else if(FKL_IS_FIX(fd_obj))
		fd=FKL_GET_FIX(fd_obj);
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMvalue* tty_obj=NULL;
	uv_tty_t* tty=createFuvTTY(exe,&tty_obj,ctx->proc,loop_obj,fp_obj);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_tty_init(&fuv_loop->loop,tty,fd,0);
	CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r,tty_obj,loop_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,tty_obj);
	return 0;
}

static int fuv_tty_mode_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(tty_obj,mode_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(tty_obj,isFuvTTY,exe);
	FKL_CHECK_TYPE(mode_obj,FKL_IS_SYM,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,tty_obj,exe,ctx->pd);
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	uv_tty_mode_t mode=0;
	FklSid_t mode_id=FKL_GET_SYM(mode_obj);
	if(mode_id==fpd->UV_TTY_MODE_NORMAL_sid)
		mode=UV_TTY_MODE_NORMAL;
	else if(mode_id==fpd->UV_TTY_MODE_RAW_sid)
		mode=UV_TTY_MODE_RAW;
	else if(mode_id==fpd->UV_TTY_MODE_IO_sid)
		mode=UV_TTY_MODE_IO;
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);

	int r=uv_tty_set_mode((uv_tty_t*)GET_HANDLE(*handle_ud),mode);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,tty_obj);
	return 0;
}

static int fuv_tty_winsize(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(tty_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(tty_obj,isFuvTTY,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(handle_ud,tty_obj,exe,ctx->pd);
	int width=0;
	int height=0;
	int r=uv_tty_get_winsize((uv_tty_t*)GET_HANDLE(*handle_ud),&width,&height);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe,FKL_MAKE_VM_FIX(width),FKL_MAKE_VM_FIX(height)));
	return 0;
}

static int fuv_tty_vterm_state(FKL_CPROC_ARGL)
{
	FKL_CHECK_REST_ARG(exe);
	uv_tty_vtermstate_t state;
	int r=uv_tty_get_vterm_state(&state);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	FklSid_t id=0;
	switch(state)
	{
		case UV_TTY_SUPPORTED:
			id=fpd->UV_TTY_SUPPORTED_sid;
			break;
		case UV_TTY_UNSUPPORTED:
			id=fpd->UV_TTY_UNSUPPORTED_sid;
			break;
	}
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_SYM(id));
	return 0;
}

static int fuv_tty_vterm_state_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(state_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(state_obj,FKL_IS_SYM,exe);

	FklSid_t state_id=FKL_GET_SYM(state_obj);
	uv_tty_vtermstate_t state=0;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	if(state_id==fpd->UV_TTY_SUPPORTED_sid)
		state=UV_TTY_SUPPORTED;
	else if(state_id==fpd->UV_TTY_UNSUPPORTED_sid)
		state=UV_TTY_UNSUPPORTED;
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);

	uv_tty_set_vterm_state(state);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fuv_tty_mode_reset1(FKL_CPROC_ARGL)
{
	FKL_CHECK_REST_ARG(exe);
	int r=uv_tty_reset_mode();
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fuv_udp_p(FKL_CPROC_ARGL){PREDICATE(isFuvUdp(val))}

static int fuv_make_udp(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	unsigned int flags=AF_UNSPEC;
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	int64_t mmsg_num_msgs=0;
	while(cur)
	{
		if(FKL_IS_SYM(cur))
		{
			FklSid_t id=FKL_GET_SYM(cur);
			int family=sid_to_af_name(id,fpd);
			if(family<0)
			{
				if(id==fpd->UV_UDP_RECVMMSG_sid)
				{
					flags|=UV_UDP_RECVMMSG;
					cur=FKL_VM_POP_ARG(exe);
					if(cur==NULL)
						FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
					if(!FKL_IS_FIX(cur))
						FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
					mmsg_num_msgs=FKL_GET_FIX(cur);
					if(mmsg_num_msgs<0)
						FKL_RAISE_BUILTIN_ERROR(FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
				}
				else
					FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
			}
			else
			{
				flags&=~0xFF;
				flags|=family;
			}
		}
		else
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		cur=FKL_VM_POP_ARG(exe);
	}
	fklResBp(exe);
	FklVMvalue* udp_obj=NULL;
	uv_udp_t* udp=createFuvUdp(exe,&udp_obj,ctx->proc,loop_obj,mmsg_num_msgs);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_udp_init_ex(&fuv_loop->loop,udp,flags);
	CHECK_UV_RESULT_AND_CLEANUP_HANDLE(r,udp_obj,loop_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static int fuv_udp_recv_stop(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(udp_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	uv_udp_t* udp=(uv_udp_t*)GET_HANDLE(*udp_ud);
	int r=uv_udp_recv_stop(udp);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static int fuv_udp_open(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(udp_obj,sock_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	FKL_CHECK_TYPE(sock_obj,fklIsVMint,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	uv_udp_t* udp=(uv_udp_t*)GET_HANDLE(*udp_ud);
	uv_os_sock_t sock=fklGetInt(sock_obj);
	int r=uv_udp_open(udp,sock);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static int fuv_udp_multicast_loop_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(udp_obj,on_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	uv_udp_t* udp=(uv_udp_t*)GET_HANDLE(*udp_ud);
	int r=uv_udp_set_multicast_loop(udp,on_obj==FKL_VM_NIL?0:1);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static int fuv_udp_multicast_ttl_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(udp_obj,ttl_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	FKL_CHECK_TYPE(ttl_obj,FKL_IS_FIX,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	uv_udp_t* udp=(uv_udp_t*)GET_HANDLE(*udp_ud);
	int r=uv_udp_set_multicast_ttl(udp,FKL_GET_FIX(ttl_obj));
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static int fuv_udp_multicast_interface_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(udp_obj,exe);
	FklVMvalue* interface_addr_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);

	uv_udp_t* udp=(uv_udp_t*)GET_HANDLE(*udp_ud);
	const char* interface_addr=NULL;
	if(interface_addr_obj)
	{
		if(FKL_IS_STR(interface_addr_obj))
			interface_addr=FKL_VM_STR(interface_addr_obj)->str;
		else if(interface_addr_obj!=FKL_VM_NIL)
			FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	}
	int r=uv_udp_set_multicast_interface(udp,interface_addr);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}



static int fuv_udp_broadcast_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(udp_obj,on_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	uv_udp_t* udp=(uv_udp_t*)GET_HANDLE(*udp_ud);
	int r=uv_udp_set_broadcast(udp,on_obj==FKL_VM_NIL?0:1);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static int fuv_udp_ttl_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(udp_obj,ttl_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	FKL_CHECK_TYPE(ttl_obj,FKL_IS_FIX,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	uv_udp_t* udp=(uv_udp_t*)GET_HANDLE(*udp_ud);
	int r=uv_udp_set_ttl(udp,FKL_GET_FIX(ttl_obj));
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static inline struct sockaddr* setup_udp_addr(FklVM* exe
		,struct sockaddr_storage* addr
		,FklVMvalue* host_obj
		,FklVMvalue* port_obj
		,FklBuiltinErrorType* err_type)
{
	const char* host=NULL;
	int port=0;
	if(host_obj==FKL_VM_NIL&&port_obj==FKL_VM_NIL)
		return NULL;
	if(!FKL_IS_STR(host_obj))
	{
		*err_type=FKL_ERR_INCORRECT_TYPE_VALUE;
		return NULL;
	}
	if(!FKL_IS_FIX(port_obj))
	{
		*err_type=FKL_ERR_INCORRECT_TYPE_VALUE;
		return NULL;
	}
	host=FKL_VM_STR(host_obj)->str;
	port=FKL_GET_FIX(port_obj);
	if(uv_ip4_addr(host,port,(struct sockaddr_in*)addr)
			&&uv_ip6_addr(host,port,(struct sockaddr_in6*)addr))
	{
		*err_type=FKL_ERR_INVALID_VALUE;
		return NULL;
	}
	return (struct sockaddr*)addr;
}

static int fuv_udp_connect(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(udp_obj,host_obj,port_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	struct sockaddr_storage addr;
	FklBuiltinErrorType err_type=0;
	struct sockaddr* addr_ptr=setup_udp_addr(exe
			,&addr
			,host_obj
			,port_obj
			,&err_type);
	if(err_type)
		FKL_RAISE_BUILTIN_ERROR(err_type,exe);
	int r=uv_udp_connect((uv_udp_t*)GET_HANDLE(*udp_ud),addr_ptr);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static inline int sid_to_uv_flag(FklSid_t id,FuvPublicData* fpd)
{
	if(id==fpd->UV_UDP_IPV6ONLY_sid)
		return UV_UDP_IPV6ONLY;
	if(id==fpd->UV_UDP_PARTIAL_sid)
		return UV_UDP_PARTIAL;
	if(id==fpd->UV_UDP_REUSEADDR_sid)
		return UV_UDP_REUSEADDR;
	if(id==fpd->UV_UDP_MMSG_CHUNK_sid)
		return UV_UDP_MMSG_CHUNK;
	if(id==fpd->UV_UDP_MMSG_FREE_sid)
		return UV_UDP_MMSG_FREE;
	if(id==fpd->UV_UDP_LINUX_RECVERR_sid)
		return UV_UDP_LINUX_RECVERR;
	if(id==fpd->UV_UDP_RECVMMSG_sid)
		return UV_UDP_RECVMMSG;
	return -1;
}

static inline FklBuiltinErrorType pop_udp_flags(FklVM* exe
		,unsigned int* flags
		,FuvPublicData* fpd)
{
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	while(cur)
	{
		if(FKL_IS_SYM(cur))
		{
			FklSid_t id=FKL_GET_SYM(cur);
			int flag=sid_to_uv_flag(id,fpd);
			if(flag<0)
				return FKL_ERR_INVALID_VALUE;
			*flags|=flag;
		}
		else
			return FKL_ERR_INCORRECT_TYPE_VALUE;
		cur=FKL_VM_POP_ARG(exe);
	}
	return 0;
}

static int fuv_udp_bind(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(udp_obj,host_obj,port_obj,exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	FKL_CHECK_TYPE(host_obj,FKL_IS_STR,exe);
	FKL_CHECK_TYPE(port_obj,FKL_IS_FIX,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	struct sockaddr_storage addr;
	const char* host=FKL_VM_STR(host_obj)->str;
	int port=FKL_GET_FIX(port_obj);
	if(uv_ip4_addr(host,port,(struct sockaddr_in*)&addr)
			&&uv_ip6_addr(host,port,(struct sockaddr_in6*)&addr))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	unsigned int flags=0;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	FklBuiltinErrorType err_type=pop_udp_flags(exe,&flags,fpd);
	if(err_type)
		FKL_RAISE_BUILTIN_ERROR(err_type,exe);
	fklResBp(exe);
	int r=uv_udp_bind((uv_udp_t*)GET_HANDLE(*udp_ud),(struct sockaddr*)&addr,flags);
	CHECK_UV_RESULT(r,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static int fuv_udp_using_recvmmsg_p(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(udp_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	uv_udp_t* udp=(uv_udp_t*)GET_HANDLE(*udp_ud);
	int r=uv_udp_using_recvmmsg(udp);
	FKL_VM_PUSH_VALUE(exe,r?FKL_VM_TRUE:FKL_VM_NIL);
	return 0;
}

static int fuv_udp_send_queue_size(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(udp_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	uv_udp_t* udp=(uv_udp_t*)GET_HANDLE(*udp_ud);
	size_t size=uv_udp_get_send_queue_size(udp);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(size,exe));
	return 0;
}

static int fuv_udp_send_queue_count(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(udp_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	uv_udp_t* udp=(uv_udp_t*)GET_HANDLE(*udp_ud);
	size_t size=uv_udp_get_send_queue_count(udp);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(size,exe));
	return 0;
}

static int fuv_udp_peername(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(udp_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	struct sockaddr_storage addr;
	int addrlen=sizeof(addr);
	int ret=uv_udp_getpeername((uv_udp_t*)GET_HANDLE(*udp_ud),(struct sockaddr*)&addr,&addrlen);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,parse_sockaddr(exe,&addr,ctx->pd));
	return 0;
}

static int fuv_udp_sockname(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG(udp_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	struct sockaddr_storage addr;
	int addrlen=sizeof(addr);
	int ret=uv_udp_getsockname((uv_udp_t*)GET_HANDLE(*udp_ud),(struct sockaddr*)&addr,&addrlen);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,parse_sockaddr(exe,&addr,ctx->pd));
	return 0;
}

static inline int sid_to_membership(FklSid_t id,FuvPublicData* fpd,uv_membership* membership)
{
	if(id==fpd->UV_LEAVE_GROUP_sid)
	{
		*membership=UV_LEAVE_GROUP;
		return 0;
	}
	if(id==fpd->UV_JOIN_GROUP_sid)
	{
		*membership=UV_JOIN_GROUP;
		return 0;
	}
	return 1;
}

static int fuv_udp_membership_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG4(udp_obj
			,multicast_addr_obj
			,interface_addr_obj
			,membership_obj
			,exe
			);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	FKL_CHECK_TYPE(multicast_addr_obj,FKL_IS_STR,exe);
	FKL_CHECK_TYPE(membership_obj,FKL_IS_SYM,exe);
	if(interface_addr_obj!=FKL_VM_NIL&&!FKL_IS_STR(interface_addr_obj))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	const char* multicast_addr=FKL_VM_STR(multicast_addr_obj)->str;
	const char* interface_addr=interface_addr_obj==FKL_VM_NIL?NULL:FKL_VM_STR(interface_addr_obj)->str;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	FklSid_t id=FKL_GET_SYM(membership_obj);
	uv_membership membership;
	if(sid_to_membership(id,fpd,&membership))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	int ret=uv_udp_set_membership((uv_udp_t*)GET_HANDLE(*udp_ud)
			,multicast_addr
			,interface_addr
			,membership);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static int fuv_udp_source_membership_set1(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG4(udp_obj
			,multicast_addr_obj
			,interface_addr_obj
			,source_addr_obj
			,exe
			);
	FklVMvalue* membership_obj=FKL_VM_POP_ARG(exe);
	if(membership_obj==NULL)
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	FKL_CHECK_TYPE(multicast_addr_obj,FKL_IS_STR,exe);
	FKL_CHECK_TYPE(source_addr_obj,FKL_IS_STR,exe);
	FKL_CHECK_TYPE(membership_obj,FKL_IS_SYM,exe);
	if(interface_addr_obj!=FKL_VM_NIL&&!FKL_IS_STR(interface_addr_obj))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	const char* multicast_addr=FKL_VM_STR(multicast_addr_obj)->str;
	const char* interface_addr=interface_addr_obj==FKL_VM_NIL?NULL:FKL_VM_STR(interface_addr_obj)->str;
	const char* source_addr=FKL_VM_STR(source_addr_obj)->str;

	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	FklSid_t id=FKL_GET_SYM(membership_obj);
	uv_membership membership;
	if(sid_to_membership(id,fpd,&membership))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INVALID_VALUE,exe);
	int ret=uv_udp_set_source_membership((uv_udp_t*)GET_HANDLE(*udp_ud)
			,multicast_addr
			,interface_addr
			,source_addr
			,membership);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static inline FklBuiltinErrorType setup_udp_send_data(FklVM* exe
		,uint32_t num
		,FklVMvalue* write_obj
		,uv_buf_t** pbufs)
{
	FKL_DECL_VM_UD_DATA(fuv_req,FuvReqUd,write_obj);
	struct FuvUdpSend* req=(struct FuvUdpSend*)*fuv_req;
	FklVMvalue** cur=req->send_objs;
	uv_buf_t* bufs=(uv_buf_t*)malloc(num*sizeof(uv_buf_t));
	FKL_ASSERT(bufs);
	FklVMvalue* val=FKL_VM_POP_ARG(exe);
	uint32_t i=0;
	while(val)
	{
		if(FKL_IS_STR(val))
		{
			*cur=val;
			FklString* str=FKL_VM_STR(val);
			bufs[i].base=str->str;
			bufs[i].len=str->size;
		}
		else if(FKL_IS_BYTEVECTOR(val))
		{
			*cur=val;
			FklBytevector* bvec=FKL_VM_BVEC(val);
			bufs[i].base=(char*)bvec->ptr;
			bufs[i].len=bvec->size;
		}
		else
		{
			free(bufs);
			return FKL_ERR_INCORRECT_TYPE_VALUE;
		}
		val=FKL_VM_POP_ARG(exe);
		i++;
		cur++;
	}
	*pbufs=bufs;
	return 0;
}

static void fuv_udp_send_cb(uv_udp_send_t* req,int status)
{
	fuv_call_req_callback_in_loop_with_value_creator((uv_req_t*)req,fuv_req_cb_error_value_creator,&status);
}

static int fuv_udp_send(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG4(udp_obj,host_obj,port_obj,cb_obj,exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	if(cb_obj==FKL_VM_NIL)
		cb_obj=NULL;
	else if(!fklIsCallable(cb_obj))
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);

	struct sockaddr_storage addr;
	FklBuiltinErrorType err_type=0;
	struct sockaddr* addr_ptr=setup_udp_addr(exe
			,&addr
			,host_obj
			,port_obj
			,&err_type);
	if(err_type)
		FKL_RAISE_BUILTIN_ERROR(err_type,exe);

	FuvHandle* handle=*udp_ud;
	uint32_t arg_num=FKL_VM_GET_ARG_NUM(exe);
	uv_buf_t* bufs=NULL;
	FklVMvalue* udp_send_obj=NULL;
	uv_udp_send_t* udp_send=NULL;
	if(arg_num)
	{
		udp_send=createFuvUdpSend(exe,&udp_send_obj,ctx->proc,handle->data.loop,cb_obj,arg_num);
		err_type=setup_udp_send_data(exe,arg_num,udp_send_obj,&bufs);
		if(err_type)
			FKL_RAISE_BUILTIN_ERROR(err_type,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
	fklResBp(exe);

	int ret=uv_udp_send(udp_send,(uv_udp_t*)GET_HANDLE(handle),bufs,arg_num,addr_ptr,fuv_udp_send_cb);
	free(bufs);
	CHECK_UV_RESULT_AND_CLEANUP_REQ(ret,udp_send_obj,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_send_obj);
	return 0;
}

#define MAX_DGRAM_SIZE (64*1024)

static void fuv_udp_alloc_cb(uv_handle_t* handle,size_t suggested_size,uv_buf_t* buf)
{
	size_t buffer_size=suggested_size;
	struct FuvUdp* udp_handle=(struct FuvUdp*)uv_handle_get_data(handle);
	if(uv_udp_using_recvmmsg((uv_udp_t*)handle))
	{
		int64_t num_msgs=udp_handle->mmsg_num_msgs;
		buffer_size=MAX_DGRAM_SIZE*num_msgs;
	}
	char* base=(char*)malloc(buffer_size);
	FKL_ASSERT(base||!buffer_size);
	buf->base=base;
	buf->len=buffer_size;
}

struct UdpRecvArg
{
	ssize_t nread;
	const struct sockaddr* addr;
	const uv_buf_t* buf;
	unsigned flags;
	FuvPublicData* fpd;
};

static void fuv_udp_recv_cb_value_creator(FklVM* exe,void* a)
{
	struct UdpRecvArg* arg=(struct UdpRecvArg*)a;
	ssize_t nread=arg->nread;
	FklVMvalue* err=nread<0
		?createUvErrorWithFpd(nread,exe,arg->fpd)
		:FKL_VM_NIL;
	FklVMvalue* res=FKL_VM_NIL;
	if(nread==0)
	{
		if(arg->addr)
			res=fklCreateVMvalueStr(exe,fklCreateStringFromCstr(""));
	}
	else if(nread>0)
		res=fklCreateVMvalueStr(exe,fklCreateString(nread,arg->buf->base));

	if(arg->buf&&!(arg->flags&UV_UDP_MMSG_CHUNK))
		free(arg->buf->base);

	FuvPublicData* fpd=arg->fpd;
	FklVMvalue* addr=arg->addr
		?parse_sockaddr_with_fpd(exe,(struct sockaddr_storage*)arg->addr,fpd)
		:FKL_VM_NIL;

	FklVMvalue* flags=FKL_VM_NIL;

	FklVMvalue** pcur=&flags;
	if(arg->flags&UV_UDP_IPV6ONLY)
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_SYM(fpd->UV_UDP_IPV6ONLY_sid));
		pcur=&FKL_VM_CDR(*pcur);
	}
	if(arg->flags&UV_UDP_PARTIAL)
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_SYM(fpd->UV_UDP_PARTIAL_sid));
		pcur=&FKL_VM_CDR(*pcur);
	}
	if(arg->flags&UV_UDP_REUSEADDR)
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_SYM(fpd->UV_UDP_REUSEADDR_sid));
		pcur=&FKL_VM_CDR(*pcur);
	}
	if(arg->flags&UV_UDP_MMSG_CHUNK)
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_SYM(fpd->UV_UDP_MMSG_CHUNK_sid));
		pcur=&FKL_VM_CDR(*pcur);
	}
	if(arg->flags&UV_UDP_MMSG_FREE)
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_SYM(fpd->UV_UDP_MMSG_FREE_sid));
		pcur=&FKL_VM_CDR(*pcur);
	}
	if(arg->flags&UV_UDP_LINUX_RECVERR)
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_SYM(fpd->UV_UDP_LINUX_RECVERR_sid));
		pcur=&FKL_VM_CDR(*pcur);
	}
	if(arg->flags&UV_UDP_RECVMMSG)
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,FKL_MAKE_VM_SYM(fpd->UV_UDP_RECVMMSG_sid));
		pcur=&FKL_VM_CDR(*pcur);
	}
	FKL_VM_PUSH_VALUE(exe,flags);
	FKL_VM_PUSH_VALUE(exe,addr);
	FKL_VM_PUSH_VALUE(exe,res);
	FKL_VM_PUSH_VALUE(exe,err);
}

static void fuv_udp_recv_cb(uv_udp_t* handle
		,ssize_t nread
		,const uv_buf_t* buf
		,const struct sockaddr* addr
		,unsigned flags)
{
	if(flags&UV_UDP_MMSG_FREE)
	{
		free(buf->base);
		return;
	}
	FuvLoopData* ldata=uv_loop_get_data(uv_handle_get_loop((uv_handle_t*)handle));
	FuvHandleData* hdata=&((FuvHandle*)uv_handle_get_data((uv_handle_t*)handle))->data;
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)ldata->exe->top_frame->data)->pd;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,fpd_obj);
	struct UdpRecvArg arg=
	{
		.nread=nread,
		.addr=addr,
		.buf=buf,
		.flags=flags,
		.fpd=fpd,
	};
	fuv_call_handle_callback_in_loop_with_value_creator((uv_handle_t*)handle
			,hdata
			,ldata
			,0
			,fuv_udp_recv_cb_value_creator
			,&arg);
}

static int fuv_udp_recv_start(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(udp_obj,cb_obj,exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	FKL_CHECK_TYPE(cb_obj,fklIsCallable,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);
	FuvHandle* handle=*udp_ud;
	uv_udp_t* udp=(uv_udp_t*)GET_HANDLE(handle);
	handle->data.callbacks[0]=cb_obj;
	int ret=uv_udp_recv_start(udp,fuv_udp_alloc_cb,fuv_udp_recv_cb);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,udp_obj);
	return 0;
}

static int fuv_udp_try_send(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG3(udp_obj,host_obj,port_obj,exe);
	FKL_CHECK_TYPE(udp_obj,isFuvUdp,exe);
	DECL_FUV_HANDLE_UD_AND_CHECK_CLOSED(udp_ud,udp_obj,exe,ctx->pd);

	struct sockaddr_storage addr;
	FklBuiltinErrorType err_type=0;
	struct sockaddr* addr_ptr=setup_udp_addr(exe
			,&addr
			,host_obj
			,port_obj
			,&err_type);
	if(err_type)
		FKL_RAISE_BUILTIN_ERROR(err_type,exe);

	uint32_t arg_num=FKL_VM_GET_ARG_NUM(exe);
	uv_buf_t* bufs=NULL;
	if(arg_num)
	{
		err_type=setup_try_write_data(exe,arg_num,&bufs);
		if(err_type)
			FKL_RAISE_BUILTIN_ERROR(err_type,exe);
	}
	else
		FKL_RAISE_BUILTIN_ERROR(FKL_ERR_TOOFEWARG,exe);
	fklResBp(exe);

	int ret=uv_udp_try_send((uv_udp_t*)GET_HANDLE(*udp_ud),bufs,arg_num,addr_ptr);
	free(bufs);
	CHECK_UV_RESULT(ret,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(ret));
	return 0;
}

static inline FklVMvalue* create_fs_uv_err(FklVM* exe
		,int r
		,uv_fs_t* req
		,FklVMvalue* dest_path
		,FklVMvalue* pd)
{
	FklVMvalue* err=NULL;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,pd);
	if(dest_path)
	{
		FklStringBuffer buf;
		fklInitStringBuffer(&buf);
		fklStringBufferPrintf(&buf,"%s: %s -> %s"
				,uv_strerror(r)
				,req->path
				,FKL_VM_STR(dest_path)->str);
		err=fklCreateVMvalueError(exe
				,uvErrToSid(r,fpd)
				,fklStringBufferToString(&buf));
		fklUninitStringBuffer(&buf);
	}
	else if(req->path)
	{
		FklStringBuffer buf;
		fklInitStringBuffer(&buf);
		fklStringBufferPrintf(&buf,"%s: %s",uv_strerror(r),req->path);
		err=fklCreateVMvalueError(exe
				,uvErrToSid(r,fpd)
				,fklStringBufferToString(&buf));
		fklUninitStringBuffer(&buf);
	}
	else
		err=createUvErrorWithFpd(r,exe,fpd);
	return err;
}

static inline void check_fs_uv_result(int r
		,uv_fs_t* req
		,FklVMvalue* dest_path
		,FklVM* exe
		,FklVMvalue* pd
		,FklVMvalue* req_obj
		,int sync)
{
	FklVMvalue* err=NULL;
	if(r<0)
	{
		err=create_fs_uv_err(exe,r,req,dest_path,pd);
		if(req_obj)
			cleanup_req(req_obj);
	}
	if(sync)
		uv_fs_req_cleanup(req);
	if(NULL)
		fklRaiseVMerror(err,exe);
	return;
}

struct FsCbValueCreatorArg
{
	uv_fs_type fs_type;
	ssize_t result;
	const char* path;
	uv_stat_t statbuf;
	void* ptr;
};

static void fuv_fs_cb_value_creator(FklVM* exe,void* a)
{
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)exe->top_frame->data)->pd;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,fpd_obj);

	struct FsCbValueCreatorArg* arg=a;
	FklVMvalue* err=FKL_VM_NIL;
	switch(arg->fs_type)
	{
		case UV_FS_CLOSE:
			if(arg->result<0)
				err=createUvErrorWithFpd(arg->result,exe,fpd);
			break;
	}
	FKL_VM_PUSH_VALUE(exe,err);
}

static void fuv_fs_cb(uv_fs_t* req)
{
	struct FsCbValueCreatorArg arg=
	{
		.fs_type=req->fs_type,
		.result=req->result,
		.path=req->path,
		.statbuf=req->statbuf,
		.ptr=req->ptr,
	};
	fuv_call_req_callback_in_loop_with_value_creator((uv_req_t*)req
			,fuv_fs_cb_value_creator
			,&arg);
}

static int fuv_fs_close(FKL_CPROC_ARGL)
{
	FKL_DECL_AND_CHECK_ARG2(loop_obj,fd_obj,exe);
	FklVMvalue* cb_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,exe);
	FKL_CHECK_TYPE(fd_obj,FKL_IS_FIX,exe);
	if(cb_obj)
		FKL_CHECK_TYPE(cb_obj,fklIsCallable,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	if(cb_obj)
	{
		FklVMvalue* req_obj=NULL;
		uv_fs_t* req=createFuvFsReq(exe,&req_obj,ctx->proc,loop_obj,cb_obj,NULL);
		int r=uv_fs_close(&fuv_loop->loop,req,FKL_GET_FIX(fd_obj),fuv_fs_cb);
		check_fs_uv_result(r,req,NULL,exe,ctx->pd,req_obj,0);
		FKL_VM_PUSH_VALUE(exe,req_obj);
	}
	else
	{
		uv_fs_t req;
		int r=uv_fs_close(&fuv_loop->loop,&req,FKL_GET_FIX(fd_obj),NULL);
		check_fs_uv_result(r,&req,NULL,exe,ctx->pd,NULL,1);
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	return 0;
}

static int fuv_incomplete(FKL_CPROC_ARGL)
{
	abort();
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	// loop
	{"loop?",                        fuv_loop_p,                       },
	{"make-loop",                    fuv_make_loop,                    },
	{"loop-close",                   fuv_loop_close,                   },
	{"loop-run",                     fuv_loop_run,                     },
	{"loop-mode",                    fuv_loop_mode,                    },
	{"loop-alive?",                  fuv_loop_alive_p,                 },
	{"loop-stop",                    fuv_loop_stop,                    },
	{"loop-backend-fd",              fuv_loop_backend_fd,              },
	{"loop-backend-timeout",         fuv_loop_backend_timeout,         },
	{"loop-now",                     fuv_loop_now,                     },
	{"loop-update-time",             fuv_loop_update_time,             },
	{"loop-walk",                    fuv_loop_walk,                    },
	{"loop-configure",               fuv_loop_configure,               },

	// handle
	{"handle?",                      fuv_handle_p,                     },
	{"handle-active?",               fuv_handle_active_p,              },
	{"handle-closing?",              fuv_handle_closing_p,             },
	{"handle-close",                 fuv_handle_close,                 },
	{"handle-ref?",                  fuv_handle_ref_p,                 },
	{"handle-ref",                   fuv_handle_ref,                   },
	{"handle-unref",                 fuv_handle_unref,                 },
	{"handle-send-buffer-size",      fuv_handle_send_buffer_size,      },
	{"handle-recv-buffer-size",      fuv_handle_recv_buffer_size,      },
	{"handle-fileno",                fuv_handle_fileno,                },
	{"handle-type",                  fuv_handle_type,                  },

	// timer
	{"timer?",                       fuv_timer_p,                      },
	{"make-timer",                   fuv_make_timer,                   },
	{"timer-start",                  fuv_timer_start,                  },
	{"timer-stop",                   fuv_timer_stop,                   },
	{"timer-again",                  fuv_timer_again,                  },
	{"timer-due-in",                 fuv_timer_due_in,                 },
	{"timer-repeat",                 fuv_timer_repeat,                 },
	{"timer-repeat-set!",            fuv_timer_repeat_set1,            },

	// prepare
	{"prepare?",                     fuv_prepare_p,                    },
	{"make-prepare",                 fuv_make_prepare,                 },
	{"prepare-start",                fuv_prepare_start,                },
	{"prepare-stop",                 fuv_prepare_stop,                 },

	// idle
	{"idle?",                        fuv_idle_p,                       },
	{"make-idle",                    fuv_make_idle,                    },
	{"idle-start",                   fuv_idle_start,                   },
	{"idle-stop",                    fuv_idle_stop,                    },

	// check
	{"check?",                       fuv_check_p,                      },
	{"make-check",                   fuv_make_check,                   },
	{"check-start",                  fuv_check_start,                  },
	{"check-stop",                   fuv_check_stop,                   },

	// signal
	{"signal?",                      fuv_signal_p,                     },
	{"make-signal",                  fuv_make_signal,                  },
	{"signal-start",                 fuv_signal_start,                 },
	{"signal-start-oneshot",         fuv_signal_start_oneshot,         },
	{"signal-stop",                  fuv_signal_stop,                  },

	// async
	{"async?",                       fuv_async_p,                      },
	{"make-async",                   fuv_make_async,                   },
	{"async-send",                   fuv_async_send,                   },

	// process
	{"process?",                     fuv_process_p,                    },
	{"process-spawn",                fuv_process_spawn,                },
	{"process-kill",                 fuv_process_kill,                 },
	{"process-pid",                  fuv_process_pid,                  },
	{"disable-stdio-inheritance",    fuv_disable_stdio_inheritance,    },
	{"kill",                         fuv_kill,                         },

	// stream
	{"stream?",                      fuv_stream_p,                     },
	{"stream-readable?",             fuv_stream_readable_p,            },
	{"stream-writable?",             fuv_stream_writable_p,            },
	{"stream-shutdown",              fuv_stream_shutdown,              },
	{"stream-listen",                fuv_stream_listen,                },
	{"stream-accept",                fuv_stream_accept,                },
	{"stream-read-start",            fuv_stream_read_start,            },
	{"stream-read-stop",             fuv_stream_read_stop,             },
	{"stream-write",                 fuv_stream_write,                 },
	{"stream-try-write",             fuv_stream_try_write,             },
	{"stream-blocking-set!",         fuv_stream_blocking_set1,         },
	{"stream-write-queue-size",      fuv_stream_write_queue_size,      },

	// pipe
	{"pipe?",                        fuv_pipe_p,                       },
	{"make-pipe",                    fuv_make_pipe,                    },
	{"pipe-open",                    fuv_pipe_open,                    },
	{"pipe-bind",                    fuv_pipe_bind,                    },
	{"pipe-connect",                 fuv_pipe_connect,                 },
	{"pipe-sockname",                fuv_pipe_sockname,                },
	{"pipe-peername",                fuv_pipe_peername,                },
	{"pipe-pending-instances",       fuv_pipe_pending_instances,       },
	{"pipe-pending-count",           fuv_pipe_pending_count,           },
	{"pipe-pending-type",            fuv_pipe_pending_type,            },
	{"pipe-chmod",                   fuv_pipe_chmod,                   },
	{"pipe",                         fuv_pipe,                         },

	// tcp
	{"tcp?",                         fuv_tcp_p,                        },
	{"make-tcp",                     fuv_make_tcp,                     },
	{"tcp-open",                     fuv_tcp_open,                     },
	{"tcp-nodelay",                  fuv_tcp_nodelay,                  },
	{"tcp-keepalive",                fuv_tcp_keepalive,                },
	{"tcp-simultaneous-accepts",     fuv_tcp_simultaneous_accepts,     },
	{"tcp-bind",                     fuv_tcp_bind,                     },
	{"tcp-sockname",                 fuv_tcp_sockname,                 },
	{"tcp-peername",                 fuv_tcp_peername,                 },
	{"tcp-connect",                  fuv_tcp_connect,                  },
	{"tcp-close-reset",              fuv_tcp_close_reset,              },
	{"socketpair",                   fuv_socketpair,                   },

	// tty
	{"tty?",                         fuv_tty_p,                        },
	{"make-tty",                     fuv_make_tty,                     },
	{"tty-mode-set!",                fuv_tty_mode_set1,                },
	{"tty-mode-reset!",              fuv_tty_mode_reset1,              },
	{"tty-winsize",                  fuv_tty_winsize,                  },
	{"tty-vterm-state",              fuv_tty_vterm_state,              },
	{"tty-vterm-state-set!",         fuv_tty_vterm_state_set1,         },

	// udp
	{"udp?",                         fuv_udp_p,                        },
	{"make-udp",                     fuv_make_udp,                     },
	{"udp-open",                     fuv_udp_open,                     },
	{"udp-bind",                     fuv_udp_bind,                     },
	{"udp-connect",                  fuv_udp_connect,                  },
	{"udp-peername",                 fuv_udp_peername,                 },
	{"udp-sockname",                 fuv_udp_sockname,                 },
	{"udp-membership-set!",          fuv_udp_membership_set1,          },
	{"udp-source-membership-set!",   fuv_udp_source_membership_set1,   },
	{"udp-multicast-loop-set!",      fuv_udp_multicast_loop_set1,      },
	{"udp-multicast-ttl-set!",       fuv_udp_multicast_ttl_set1,       },
	{"udp-multicast-interface-set!", fuv_udp_multicast_interface_set1, },
	{"udp-broadcast-set!",           fuv_udp_broadcast_set1,           },
	{"udp-ttl-set!",                 fuv_udp_ttl_set1,                 },
	{"udp-send",                     fuv_udp_send,                     },
	{"udp-try-send",                 fuv_udp_try_send,                 },
	{"udp-recv-start",               fuv_udp_recv_start,               },
	{"udp-using-recvmmsg?",          fuv_udp_using_recvmmsg_p,         },
	{"udp-recv-stop",                fuv_udp_recv_stop,                },
	{"udp-send-queue-size",          fuv_udp_send_queue_size,          },
	{"udp-send-queue-count",         fuv_udp_send_queue_count,         },

	// req
	{"req?",                         fuv_req_p,                        },
	{"req-cancel",                   fuv_req_cancel,                   },
	{"req-type",                     fuv_req_type,                     },

	{"getaddrinfo?",                 fuv_getaddrinfo_p,                },
	{"getnameinfo?",                 fuv_getnameinfo_p,                },
	{"write?",                       fuv_write_p,                      },
	{"shutdown?",                    fuv_shutdown_p,                   },
	{"connect?",                     fuv_connect_p,                    },
	{"udp-send?",                    fuv_udp_send_p,                   },
	{"fs-req?",                          fuv_fs_req_p,                         },

	// fs
	{"fs-close",                      fuv_fs_close,                   },
	{"fs-open",                      fuv_incomplete,                   },
	{"fs-read",                      fuv_incomplete,                   },
	{"fs-unlink",                      fuv_incomplete,                   },
	{"fs-write",                      fuv_incomplete,                   },
	{"fs-mkdir",                      fuv_incomplete,                   },
	{"fs-mkdtemp",                      fuv_incomplete,                   },
	{"fs-mkstemp",                      fuv_incomplete,                   },
	{"fs-rmdir",                      fuv_incomplete,                   },
	{"fs-opendir",                      fuv_incomplete,                   },
	{"fs-closedir",                      fuv_incomplete,                   },
	{"fs-readdir",                      fuv_incomplete,                   },
	{"fs-scandir",                      fuv_incomplete,                   },

	// dns
	{"getaddrinfo",                  fuv_getaddrinfo,                  },
	{"getnameinfo",                  fuv_getnameinfo,                  },

	// misc
	{"exepath",                      fuv_exepath,                      },
	{"guess-handle",                 fuv_guess_handle,                 },
};

static const size_t EXPORT_NUM=sizeof(exports_and_func)/sizeof(struct SymFunc);

FKL_DLL_EXPORT void _fklExportSymbolInit(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS)
{
	*num=EXPORT_NUM;
	FklSid_t* symbols=(FklSid_t*)malloc(sizeof(FklSid_t)*EXPORT_NUM);
	FKL_ASSERT(symbols);
	for(size_t i=0;i<EXPORT_NUM;i++)
		symbols[i]=fklAddSymbolCstr(exports_and_func[i].sym,st)->id;
	*exports=symbols;
}

FKL_DLL_EXPORT FklVMvalue** _fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS)
{
	*count=EXPORT_NUM;
	FklVMvalue** loc=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*EXPORT_NUM);
	FKL_ASSERT(loc);

	FklVMvalue* fpd=fklCreateVMvalueUd(exe
			,&FuvPublicDataMetaTable
			,dll);

	FKL_DECL_VM_UD_DATA(pd,FuvPublicData,fpd);

	fklVMacquireSt(exe->gc);
	FklSymbolTable* st=exe->gc->st;
	init_fuv_public_data(pd,st);
	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklAddSymbolCstr(exports_and_func[i].sym,st)->id;
		FklVMcFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,fpd,id);
		loc[i]=dlproc;
	}
	fklVMreleaseSt(exe->gc);
	return loc;
}
