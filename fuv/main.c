#include"fuv.h"

#define PREDICATE(condition,err_infor) FKL_DECL_AND_CHECK_ARG(val,exe,err_infor);\
	FKL_CHECK_REST_ARG(exe,err_infor);\
	FKL_VM_PUSH_VALUE(exe,(condition)\
			?FKL_VM_TRUE\
			:FKL_VM_NIL);\
	return 0;

static FklVMudMetaTable FuvPublicDataMetaTable=
{
	.size=sizeof(FuvPublicData),
};

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

	pd->aif_ip_sid=fklAddSymbolCstr("ip",st)->id;
	pd->aif_addr_sid=fklAddSymbolCstr("addr",st)->id;
	pd->aif_port_sid=fklAddSymbolCstr("port",st)->id;
	pd->aif_family_sid=fklAddSymbolCstr("family",st)->id;
	pd->aif_socktype_sid=fklAddSymbolCstr("socktype",st)->id;
	pd->aif_protocol_sid=fklAddSymbolCstr("protocol",st)->id;
	pd->aif_canonname_sid=fklAddSymbolCstr("canonname",st)->id;
	pd->aif_hostname_sid=fklAddSymbolCstr("hostname",st)->id;
	pd->aif_service_sid=fklAddSymbolCstr("service",st)->id;

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

static int fuv_loop_p(FKL_CPROC_ARGL){PREDICATE(isFuvLoop(val),"fuv.loop?")}

static int fuv_make_loop(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.make-loop";
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* r=createFuvLoop(exe,ctx->proc);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int fuv_loop_close(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-close";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_loop_close(&fuv_loop->loop);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fuv_proc_call_frame_end(void* data)
{
	return 0;
}

static int fuv_proc_call_frame_error_callback(FKL_VM_ERROR_CALLBACK_ARGL)
{
	FklVMframe* run_loop_frame=f->prev;
	FuvProcCallCtx* ctx=(FuvProcCallCtx*)f->data;
	FklCprocFrameContext* run_loop_ctx=(FklCprocFrameContext*)run_loop_frame->data;
	vm->tp=run_loop_ctx->rtp;
	FKL_VM_PUSH_VALUE(vm,ev);
	longjmp(*ctx->buf,FUV_RUN_ERR_OCCUR);
	return 1;
}

static void fuv_proc_call_frame_step(void* data,FklVM* exe)
{
	FuvProcCallCtx* ctx=(FuvProcCallCtx*)data;
	longjmp(*ctx->buf,FUV_RUN_OK);
}

static const FklVMframeContextMethodTable FuvProcCallCtxMethodTable=
{
	.end=fuv_proc_call_frame_end,
	.step=fuv_proc_call_frame_step,
};

static inline void origin_top_frame_swap(FklVM* exe,FklVMframe* origin_top_frame)
{
	FklVMframe* prev=exe->top_frame;
	for(;prev&&prev->prev!=origin_top_frame;prev=prev->prev);
	prev->prev=origin_top_frame->prev;
	origin_top_frame->prev=exe->top_frame;
	exe->top_frame=origin_top_frame;
}

static inline void buttom_frame_swap(FklVM* exe)
{
	FklVMframe* buttom_frame=exe->top_frame;
	for(;buttom_frame->type==FKL_FRAME_COMPOUND
			||buttom_frame->t!=&FuvProcCallCtxMethodTable
			;buttom_frame=buttom_frame->prev);
	FklVMframe* prev=exe->top_frame->prev;
	exe->top_frame->prev=buttom_frame->prev;
	buttom_frame->prev=exe->top_frame;
	exe->top_frame=prev;
}

static int fuv_loop_run(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-run";
	switch(ctx->context)
	{
		case 0:
			{
				FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
				FklVMvalue* mode_obj=FKL_VM_POP_ARG(exe);
				FKL_CHECK_REST_ARG(exe,Pname);
				FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
				FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
				FKL_VM_PUSH_VALUE(exe,loop_obj);
				FklVMframe* origin_top_frame=exe->top_frame;
				ctx->rtp=exe->tp;
				fuv_loop->data.exe=exe;
				int need_continue=0;
				int mode=UV_RUN_DEFAULT;
				if(mode_obj)
				{
					FKL_CHECK_TYPE(mode_obj,FKL_IS_SYM,Pname,exe);
					FKL_DECL_VM_UD_DATA(pbd,FuvPublicData,ctx->pd);
					FklSid_t mode_id=FKL_GET_SYM(mode_obj);
					for(;mode<(int)(sizeof(pbd->loop_mode)/sizeof(FklSid_t))
							;mode++)
						if(pbd->loop_mode[mode]==mode_id)
							break;
					if(mode>UV_RUN_NOWAIT)
						FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
				}
				exe->top_frame=fklCreateOtherObjVMframe(exe,&FuvProcCallCtxMethodTable,origin_top_frame);
				FklVMframe* buttom_frame=exe->top_frame;
				buttom_frame->errorCallBack=fuv_proc_call_frame_error_callback;
				if(setjmp(fuv_loop->data.buf))
				{
					ctx->context=1;
					need_continue=1;
					buttom_frame->errorCallBack=NULL;
					origin_top_frame_swap(exe,origin_top_frame);
				}
				else
				{
					fuv_loop->data.mode=mode;
					fklUnlockThread(exe);
					int r=uv_run(&fuv_loop->loop,mode);
					fklLockThread(exe);
					if(r<0)
					{
						FKL_VM_PUSH_VALUE(exe,createUvError(Pname,r,exe,ctx->pd));
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
		case 1:
			buttom_frame_swap(exe);
			fklRaiseVMerror(FKL_VM_POP_TOP_VALUE(exe),exe);
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
	FklVMframe* fuv_proc_call_frame=exe->top_frame;
	FuvProcCallCtx* ctx=(FuvProcCallCtx*)fuv_proc_call_frame->data;
	jmp_buf buf;
	ctx->buf=&buf;
	int r=setjmp(buf);
	if(r==FUV_RUN_OK)
		exe->tp--;
	else if(r==FUV_RUN_ERR_OCCUR)
		walk_ctx->ev=FKL_VM_GET_TOP_VALUE(exe);
	else
	{
		FuvHandle* fuv_handle=uv_handle_get_data(handle);
		FuvHandleData* handle_data=&fuv_handle->data;
		fklSetBp(exe);
		FKL_VM_PUSH_VALUE(exe,handle_data->handle);
		fklCallObj(exe,proc);
		exe->thread_run_cb(exe);
	}
	fklUnlockThread(exe);
}

static int fuv_loop_walk(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-walk";
	switch(ctx->context)
	{
		case 0:
			{
				FKL_DECL_AND_CHECK_ARG2(loop_obj,proc_obj,exe,Pname);
				FKL_CHECK_REST_ARG(exe,Pname);
				FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
				FKL_CHECK_TYPE(proc_obj,fklIsCallable,Pname,exe);
				FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
				FKL_VM_PUSH_VALUE(exe,loop_obj);
				FKL_VM_PUSH_VALUE(exe,proc_obj);
				FklVMframe* origin_top_frame=exe->top_frame;
				int need_continue=0;
				exe->top_frame=fklCreateOtherObjVMframe(exe,&FuvProcCallCtxMethodTable,origin_top_frame);
				FklVMframe* buttom_frame=exe->top_frame;
				buttom_frame->errorCallBack=fuv_proc_call_frame_error_callback;
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
					ctx->context=1;
					need_continue=1;
					buttom_frame->errorCallBack=NULL;
					origin_top_frame_swap(exe,origin_top_frame);
				}
				else
					while(exe->top_frame!=origin_top_frame)
						fklPopVMframe(exe);
				exe->state=FKL_VM_READY;
				return need_continue;
			}
			break;
		case 1:
			buttom_frame_swap(exe);
			fklRaiseVMerror(FKL_VM_POP_TOP_VALUE(exe),exe);
			break;
	}
	return 0;
}

static int fuv_loop_configure(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-configure";
	FKL_DECL_AND_CHECK_ARG2(loop_obj,option_obj,exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_CHECK_TYPE(option_obj,FKL_IS_SYM,Pname,exe);
	FklSid_t option_id=FKL_GET_SYM(option_obj);
	uv_loop_option option=UV_LOOP_BLOCK_SIGNAL;
	FklVMvalue* pd=ctx->pd;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,pd);
	if(option_id==fpd->loop_block_signal_sid)
		option=UV_LOOP_BLOCK_SIGNAL;
	else if(option_id==fpd->metrics_idle_time_sid)
		option=UV_METRICS_IDLE_TIME;
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
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
						CHECK_UV_RESULT(r,Pname,exe,pd);
					}
					else if(FKL_IS_SYM(cur))
					{
						FklSid_t sid=FKL_GET_SYM(cur);
						int signum=symbolToSignum(sid,fpd);
						if(!signum)
							FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);
						int r=uv_loop_configure(loop,UV_LOOP_BLOCK_SIGNAL,signum);
						CHECK_UV_RESULT(r,Pname,exe,pd);
					}
					else
						FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
					cur=FKL_VM_POP_ARG(exe);
				}
				fklResBp(exe);
			}
			break;
		case UV_METRICS_IDLE_TIME:
			{
				FKL_CHECK_REST_ARG(exe,Pname);
				int r=uv_loop_configure(loop,UV_METRICS_IDLE_TIME);
				CHECK_UV_RESULT(r,Pname,exe,pd);
			}
			break;
	}
	FKL_VM_PUSH_VALUE(exe,loop_obj);
	return 0;
}

static int fuv_loop_alive_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-alive?";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_loop_alive(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,r?FKL_VM_TRUE:FKL_VM_NIL);
	return 0;
}

static int fuv_loop_stop(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-stop";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	uv_stop(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,loop_obj);
	return 0;
}

static int fuv_loop_mode(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-mode";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
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
	static const char Pname[]="fuv.loop-backend-fd";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_backend_fd(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,r==-1?FKL_VM_NIL:FKL_MAKE_VM_FIX(r));
	return 0;
}

static int fuv_loop_backend_timeout(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-backend-timeout";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_backend_timeout(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,r==-1?FKL_VM_NIL:FKL_MAKE_VM_FIX(r));
	return 0;
}

static int fuv_loop_now(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-now";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	uint64_t r=uv_now(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(r,exe));
	return 0;
}

static int fuv_loop_update_time(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-update-time";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	uv_update_time(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,loop_obj);
	return 0;
}

static int fuv_handle_p(FKL_CPROC_ARGL){PREDICATE(isFuvHandle(val),"fuv.handle?")}

static int fuv_handle_active_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-active?";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandleUd,handle_obj);
	FuvHandle* handle=*fuv_handle;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,uv_is_active(GET_HANDLE(handle))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int fuv_handle_closing_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-closing?";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandleUd,handle_obj);
	FuvHandle* handle=*fuv_handle;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,uv_is_closing(GET_HANDLE(handle))
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
		uint32_t stp=exe->tp;
		uint32_t ltp=exe->ltp;
		FklVMframe* buttom_frame=exe->top_frame;
		exe->state=FKL_VM_READY;
		FklVMframe* fuv_proc_call_frame=exe->top_frame;
		FuvProcCallCtx* ctx=(FuvProcCallCtx*)fuv_proc_call_frame->data;
		jmp_buf buf;
		ctx->buf=&buf;
		int r=setjmp(buf);
		if(r==FUV_RUN_OK)
			exe->tp--;
		else if(r==FUV_RUN_ERR_OCCUR)
			startErrorHandle(loop,loop_data,exe,stp,ltp,buttom_frame);
		else
		{
			fklSetBp(exe);
			fklCallObj(exe,proc);
			exe->thread_run_cb(exe);
		}
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
	static const char Pname[]="fuv.handle-close";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FklVMvalue* proc_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandleUd,handle_obj);
	FuvHandle* handle=*fuv_handle;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	uv_handle_t* hh=GET_HANDLE(handle);
	if(uv_is_closing(hh))
		raiseFuvError(Pname,FUV_ERR_CLOSE_CLOSEING_HANDLE,exe,ctx->pd);
	if(proc_obj)
	{
		FKL_CHECK_TYPE(proc_obj,fklIsCallable,Pname,exe);
		handle->data.callbacks[1]=proc_obj;
	}
	uv_close(hh,fuv_close_cb);
	FKL_VM_PUSH_VALUE(exe,handle_obj);
	return 0;
}

static int fuv_handle_ref_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-ref?";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandleUd,handle_obj);
	FuvHandle* handle=*fuv_handle;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,uv_has_ref(GET_HANDLE(handle))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static int fuv_handle_ref(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-ref";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandleUd,handle_obj);
	FuvHandle* handle=*fuv_handle;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	uv_ref(GET_HANDLE(handle));
	FKL_VM_PUSH_VALUE(exe,handle_obj);
	return 0;
}

static int fuv_handle_unref(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-unref";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandleUd,handle_obj);
	FuvHandle* handle=*fuv_handle;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	uv_unref(GET_HANDLE(handle));
	FKL_VM_PUSH_VALUE(exe,handle_obj);
	return 0;
}

static int fuv_handle_send_buffer_size(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-send-buffer-size";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FklVMvalue* value_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandleUd,handle_obj);
	FuvHandle* handle=*fuv_handle;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	int value=0;
	if(value_obj)
	{
		FKL_CHECK_TYPE(value_obj,fklIsVMint,Pname,exe);
		value=fklGetInt(value_obj);
	}
	int r=uv_send_buffer_size(GET_HANDLE(handle),&value);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(value));
	return 0;
}

static int fuv_handle_recv_buffer_size(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-recv-buffer-size";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FklVMvalue* value_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandleUd,handle_obj);
	FuvHandle* handle=*fuv_handle;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	int value=0;
	if(value_obj)
	{
		FKL_CHECK_TYPE(value_obj,fklIsVMint,Pname,exe);
		value=fklGetInt(value_obj);
	}
	int r=uv_recv_buffer_size(GET_HANDLE(handle),&value);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(value));
	return 0;
}

static int fuv_handle_fileno(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-fileno";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandleUd,handle_obj);
	FuvHandle* handle=*fuv_handle;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	uv_os_fd_t fd=0;
	int r=uv_fileno(GET_HANDLE(handle),&fd);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint((int64_t)(ptrdiff_t)fd,exe));
	return 0;
}

static int fuv_handle_type(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-fileno";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandleUd,handle_obj);
	FuvHandle* handle=*fuv_handle;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
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

static int fuv_timer_p(FKL_CPROC_ARGL){PREDICATE(isFuvTimer(val),"fuv.timer?")}

static int fuv_make_timer(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.make-timer";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	int r;
	FklVMvalue* timer_obj=createFuvTimer(exe,ctx->proc,loop_obj,&r);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
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
	static const char Pname[]="fuv.timer-start";
	FKL_DECL_AND_CHECK_ARG4(timer_obj,timer_cb,timeout_obj,repeat_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_CHECK_TYPE(timer_cb,fklIsCallable,Pname,exe);
	FKL_CHECK_TYPE(timeout_obj,fklIsVMint,Pname,exe);
	FKL_CHECK_TYPE(repeat_obj,fklIsVMint,Pname,exe);
	if(fklIsVMnumberLt0(timeout_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	if(fklIsVMnumberLt0(repeat_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	uint64_t timeout=fklGetUint(timeout_obj);
	uint64_t repeat=fklGetUint(repeat_obj);
	FKL_DECL_VM_UD_DATA(timer,FuvHandleUd,timer_obj);
	FuvHandle* fuv_handle=*timer;
	CHECK_HANDLE_CLOSED(fuv_handle,Pname,exe,ctx->pd);
	fuv_handle->data.callbacks[0]=timer_cb;
	int r=uv_timer_start((uv_timer_t*)GET_HANDLE(fuv_handle),fuv_timer_cb,timeout,repeat);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_stop(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-stop";
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvHandleUd,timer_obj);
	FuvHandle* handle=*timer;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	int r=uv_timer_stop((uv_timer_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_again(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-again";
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvHandleUd,timer_obj);
	FuvHandle* handle=*timer;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	int r=uv_timer_again((uv_timer_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_repeat(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-repeat";
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvHandleUd,timer_obj);
	FuvHandle* handle=*timer;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(uv_timer_get_repeat((uv_timer_t*)GET_HANDLE(handle)),exe));
	return 0;
}


static int fuv_timer_repeat_set1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-repeat-set!";
	FKL_DECL_AND_CHECK_ARG2(timer_obj,repeat_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_CHECK_TYPE(repeat_obj,fklIsVMint,Pname,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvHandleUd,timer_obj);
	FuvHandle* handle=*timer;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	if(fklIsVMnumberLt0(repeat_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	uv_timer_set_repeat((uv_timer_t*)GET_HANDLE(handle),fklGetUint(repeat_obj));
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_due_in(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-repeat";
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvHandleUd,timer_obj);
	FuvHandle* handle=*timer;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(uv_timer_get_due_in((uv_timer_t*)GET_HANDLE(handle)),exe));
	return 0;
}

static int fuv_prepare_p(FKL_CPROC_ARGL){PREDICATE(isFuvPrepare(val),"fuv.prepare?")}

static int fuv_make_prepare(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.make-prepare";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	int r;
	FklVMvalue* prepare_obj=createFuvPrepare(exe,ctx->proc,loop_obj,&r);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
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
	static const char Pname[]="fuv.prepare-start";
	FKL_DECL_AND_CHECK_ARG2(prepare_obj,prepare_cb,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(prepare_obj,isFuvPrepare,Pname,exe);
	FKL_CHECK_TYPE(prepare_cb,fklIsCallable,Pname,exe);
	FKL_DECL_VM_UD_DATA(prepare,FuvHandleUd,prepare_obj);
	FuvHandle* fuv_handle=*prepare;
	CHECK_HANDLE_CLOSED(fuv_handle,Pname,exe,ctx->pd);
	fuv_handle->data.callbacks[0]=prepare_cb;
	int r=uv_prepare_start((uv_prepare_t*)GET_HANDLE(fuv_handle),fuv_prepare_cb);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,prepare_obj);
	return 0;
}

static int fuv_prepare_stop(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.prepare-stop";
	FKL_DECL_AND_CHECK_ARG(prepare_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(prepare_obj,isFuvPrepare,Pname,exe);
	FKL_DECL_VM_UD_DATA(prepare,FuvHandleUd,prepare_obj);
	FuvHandle* handle=*prepare;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	int r=uv_prepare_stop((uv_prepare_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,prepare_obj);
	return 0;
}

static int fuv_idle_p(FKL_CPROC_ARGL){PREDICATE(isFuvIdle(val),"fuv.idle?")}

static int fuv_make_idle(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.make-idle";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	int r;
	FklVMvalue* idle=createFuvIdle(exe,ctx->proc,loop_obj,&r);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
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
	static const char Pname[]="fuv.idle-start";
	FKL_DECL_AND_CHECK_ARG2(idle_obj,idle_cb,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(idle_obj,isFuvIdle,Pname,exe);
	FKL_CHECK_TYPE(idle_cb,fklIsCallable,Pname,exe);
	FKL_DECL_VM_UD_DATA(idle,FuvHandleUd,idle_obj);
	FuvHandle* fuv_handle=*idle;
	CHECK_HANDLE_CLOSED(fuv_handle,Pname,exe,ctx->pd);
	fuv_handle->data.callbacks[0]=idle_cb;
	int r=uv_idle_start((uv_idle_t*)GET_HANDLE(fuv_handle),fuv_idle_cb);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,idle_obj);
	return 0;
}

static int fuv_idle_stop(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.idle-stop";
	FKL_DECL_AND_CHECK_ARG(idle_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(idle_obj,isFuvIdle,Pname,exe);
	FKL_DECL_VM_UD_DATA(idle,FuvHandleUd,idle_obj);
	FuvHandle* handle=*idle;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	int r=uv_idle_stop((uv_idle_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,idle_obj);
	return 0;
}

static int fuv_check_p(FKL_CPROC_ARGL){PREDICATE(isFuvCheck(val),"fuv.check?")}

static int fuv_make_check(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.make-check";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	int r;
	FklVMvalue* check=createFuvCheck(exe,ctx->proc,loop_obj,&r);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
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
	static const char Pname[]="fuv.check-start";
	FKL_DECL_AND_CHECK_ARG2(check_obj,check_cb,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(check_obj,isFuvCheck,Pname,exe);
	FKL_CHECK_TYPE(check_cb,fklIsCallable,Pname,exe);
	FKL_DECL_VM_UD_DATA(check,FuvHandleUd,check_obj);
	FuvHandle* fuv_handle=*check;
	CHECK_HANDLE_CLOSED(fuv_handle,Pname,exe,ctx->pd);
	fuv_handle->data.callbacks[0]=check_cb;
	int r=uv_check_start((uv_check_t*)GET_HANDLE(fuv_handle),fuv_check_cb);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,check_obj);
	return 0;
}

static int fuv_check_stop(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.check-stop";
	FKL_DECL_AND_CHECK_ARG(check_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(check_obj,isFuvCheck,Pname,exe);
	FKL_DECL_VM_UD_DATA(check,FuvHandleUd,check_obj);
	FuvHandle* handle=*check;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	int r=uv_check_stop((uv_check_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,check_obj);
	return 0;
}

static int fuv_signal_p(FKL_CPROC_ARGL){PREDICATE(isFuvSignal(val),"fuv.signal?")}

static int fuv_make_signal(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.make-signal";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	int r;
	FklVMvalue* signal=createFuvSignal(exe,ctx->proc,loop_obj,&r);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
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
		uint32_t stp=exe->tp;
		uint32_t ltp=exe->ltp;
		FklVMframe* buttom_frame=exe->top_frame;
		exe->state=FKL_VM_READY;
		exe->state=FKL_VM_READY;
		FklVMframe* fuv_proc_call_frame=exe->top_frame;
		FuvProcCallCtx* ctx=(FuvProcCallCtx*)fuv_proc_call_frame->data;
		jmp_buf buf;
		ctx->buf=&buf;
		int r=setjmp(buf);
		if(r==FUV_RUN_OK)
			exe->tp--;
		else if(r==FUV_RUN_ERR_OCCUR)
			startErrorHandle(uv_handle_get_loop(handle)
					,loop_data
					,exe
					,stp
					,ltp
					,buttom_frame);
		else
		{
			fklSetBp(exe);
			if(creator)
				creator(exe,arg);
			fklCallObj(exe,proc);
			exe->thread_run_cb(exe);
		}
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
	FklVMframe* run_loop_proc_frame=ldata->exe->top_frame->prev;
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)run_loop_proc_frame->data)->pd;
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
	static const char Pname[]="fuv.signal-start";
	FKL_DECL_AND_CHECK_ARG3(signal_obj,signal_cb,signum_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(signal_obj,isFuvSignal,Pname,exe);
	FKL_CHECK_TYPE(signal_cb,fklIsCallable,Pname,exe);
	int signum=0;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	if(FKL_IS_FIX(signum_obj))
		signum=FKL_GET_FIX(signum_obj);
	else if(FKL_IS_SYM(signum_obj))
		signum=symbolToSignum(FKL_GET_SYM(signum_obj),fpd);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(signum==0)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);

	FKL_DECL_VM_UD_DATA(signal,FuvHandleUd,signal_obj);
	FuvHandle* fuv_handle=*signal;
	CHECK_HANDLE_CLOSED(fuv_handle,Pname,exe,ctx->pd);
	fuv_handle->data.callbacks[0]=signal_cb;
	int r=uv_signal_start((uv_signal_t*)GET_HANDLE(fuv_handle),fuv_signal_cb,signum);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,signal_obj);
	return 0;
}

static int fuv_signal_start_oneshot(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.signal-start-oneshot";
	FKL_DECL_AND_CHECK_ARG3(signal_obj,signal_cb,signum_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(signal_obj,isFuvSignal,Pname,exe);
	FKL_CHECK_TYPE(signal_cb,fklIsCallable,Pname,exe);
	int signum=0;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	if(FKL_IS_FIX(signum_obj))
		signum=FKL_GET_FIX(signum_obj);
	else if(FKL_IS_SYM(signum_obj))
		signum=symbolToSignum(FKL_GET_SYM(signum_obj),fpd);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(signum==0)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INVALID_VALUE,exe);

	FKL_DECL_VM_UD_DATA(signal,FuvHandleUd,signal_obj);
	FuvHandle* fuv_handle=*signal;
	CHECK_HANDLE_CLOSED(fuv_handle,Pname,exe,ctx->pd);
	fuv_handle->data.callbacks[0]=signal_cb;
	int r=uv_signal_start_oneshot((uv_signal_t*)GET_HANDLE(fuv_handle),fuv_signal_cb,signum);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,signal_obj);
	return 0;
}

static int fuv_signal_stop(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.signal-stop";
	FKL_DECL_AND_CHECK_ARG(signal_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(signal_obj,isFuvSignal,Pname,exe);
	FKL_DECL_VM_UD_DATA(signal,FuvHandleUd,signal_obj);
	FuvHandle* handle=*signal;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
	int r=uv_signal_stop((uv_signal_t*)GET_HANDLE(handle));
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,signal_obj);
	return 0;
}

static int fuv_async_p(FKL_CPROC_ARGL){PREDICATE(isFuvAsync(val),"fuv.async?")}

static int fuv_make_async(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.make-async";
	FKL_DECL_AND_CHECK_ARG2(loop_obj,proc_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_CHECK_TYPE(proc_obj,fklIsCallable,Pname,exe);
	int r;
	FklVMvalue* async=createFuvAsync(exe,ctx->proc,loop_obj,proc_obj,&r);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,async);
	return 0;
}

static int fuv_async_send(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.async-send";
	FKL_DECL_AND_CHECK_ARG(async_obj,exe,Pname);
	FKL_CHECK_TYPE(async_obj,isFuvAsync,Pname,exe);
	FKL_DECL_VM_UD_DATA(async,FuvHandleUd,async_obj);
	FuvHandle* handle=*async;
	CHECK_HANDLE_CLOSED(handle,Pname,exe,ctx->pd);
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
		CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	}
	exe->tp-=arg_num;
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,async_obj);
	return 0;
}

static int fuv_req_p(FKL_CPROC_ARGL){PREDICATE(isFuvReq(val),"fuv.req?")}

static int fuv_req_cancel(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.req-cancel";
	FKL_DECL_AND_CHECK_ARG(req_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(req_obj,isFuvReq,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_req,FuvReqUd,req_obj);
	FuvReq* req=*fuv_req;
	CHECK_REQ_CANCELED(req,Pname,exe,ctx->pd);
	int r=uv_cancel(GET_REQ(req));
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	uninitFuvReq(fuv_req);
	req->data.callback=NULL;
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fuv_req_type(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.req-cancel";
	FKL_DECL_AND_CHECK_ARG(req_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(req_obj,isFuvReq,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_req,FuvReqUd,req_obj);
	FuvReq* req=*fuv_req;
	CHECK_REQ_CANCELED(req,Pname,exe,ctx->pd);

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

static int fuv_getaddrinfo_p(FKL_CPROC_ARGL){PREDICATE(isFuvGetaddrinfo(val),"fuv.getaddrinfo?")}

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
	return -1;
}

static inline FklBuiltinErrorType process_addrinfo_hints(FklVM* exe
		,struct addrinfo* hints
		,FuvPublicData* fpd)
{
	FklVMvalue* cur=FKL_VM_POP_ARG(exe);
	while(cur)
	{
		if(FKL_IS_SYM(cur))
		{
			FklSid_t id=FKL_GET_SYM(cur);
			if(id==fpd->aif_family_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(!FKL_IS_SYM(cur))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				FklSid_t family_id=FKL_GET_SYM(cur);
				int af_num=sid_to_af_name(family_id,fpd);
				if(af_num<0)
					return FKL_ERR_INVALID_VALUE;
				hints->ai_family=af_num;
				goto done;
			}
			else if(id==fpd->aif_socktype_sid)
			{
				cur=FKL_VM_POP_ARG(exe);
				if(cur==NULL)
					return FKL_ERR_TOOFEWARG;
				if(!FKL_IS_SYM(cur))
					return FKL_ERR_INCORRECT_TYPE_VALUE;
				FklSid_t socktype_id=FKL_GET_SYM(cur);
#ifdef SOCK_STREAM
				if(socktype_id==fpd->SOCK_STREAM_sid){hints->ai_socktype=SOCK_STREAM;goto done;}
#endif
#ifdef SOCK_DGRAM
				if(socktype_id==fpd->SOCK_DGRAM_sid){hints->ai_socktype=SOCK_STREAM;goto done;}
#endif
#ifdef SOCK_SEQPACKET
				if(socktype_id==fpd->SOCK_SEQPACKET_sid){hints->ai_socktype=SOCK_SEQPACKET;goto done;}
#endif
#ifdef SOCK_RAW
				if(socktype_id==fpd->SOCK_RAW_sid){hints->ai_socktype=SOCK_RAW;goto done;}
#endif
#ifdef SOCK_RDM
				if(socktype_id==fpd->SOCK_RDM_sid){hints->ai_socktype=SOCK_RDM;goto done;}
#endif
				return FKL_ERR_INVALID_VALUE;
			}
			else if(id==fpd->aif_protocol_sid)
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
	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->aif_family_sid)
			,af_num_to_symbol(info->ai_family,fpd)
			,ht);
	uv_inet_ntop(info->ai_family,addr,ip,INET6_ADDRSTRLEN);

	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->aif_addr_sid)
			,fklCreateVMvalueStr(exe,fklCreateStringFromCstr(ip))
			,ht);

	if(ntohs(port))
		fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->aif_port_sid)
				,FKL_MAKE_VM_FIX(ntohs(port))
				,ht);
	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->aif_socktype_sid)
			,sock_num_to_symbol(info->ai_socktype,fpd)
			,ht);
	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->aif_protocol_sid)
			,proto_num_to_symbol(info->ai_protocol,exe)
			,ht);
	if(info->ai_canonname)
		fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->aif_canonname_sid)
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
		free(fuv_req);
		return;
	}
	fklLockThread(exe);

	exe->state=FKL_VM_READY;
	uint32_t stp=exe->tp;
	uint32_t ltp=exe->ltp;
	FklVMframe* buttom_frame=exe->top_frame;
	FuvProcCallCtx* ctx=(FuvProcCallCtx*)exe->top_frame->data;
	jmp_buf buf;
	ctx->buf=&buf;
	int r=setjmp(buf);
	if(r==FUV_RUN_OK)
		exe->tp--;
	else if(r==FUV_RUN_ERR_OCCUR)
		startErrorHandle(&fuv_loop->loop,ldata,exe,stp,ltp,buttom_frame);
	else
	{
		fklSetBp(exe);
		if(creator)
			creator(exe,arg);
		uninitFuvReqValue(rdata->req);
		free(fuv_req);
		fklCallObj(exe,proc);
		exe->thread_run_cb(exe);
	}
	fklUnlockThread(exe);
}

#define FUV_GETADDRINFO_PNAME ("fuv.getaddrinfo")

struct GetaddrinfoValueCreateArg
{
	struct addrinfo* res;
	int status;
};

static void fuv_getaddrinfo_cb_value_creator(FklVM* exe,void* a)
{
	FklVMframe* fuv_proc_call_frame=exe->top_frame;
	FklVMframe* run_loop_proc_frame=fuv_proc_call_frame->prev;
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)run_loop_proc_frame->data)->pd;
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,fpd_obj);

	struct GetaddrinfoValueCreateArg* arg=a;
	FklVMvalue* err=arg->status<0
		?createUvErrorWithFpd(FUV_GETADDRINFO_PNAME,arg->status,exe,fpd)
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
	static const char Pname[]=FUV_GETADDRINFO_PNAME;
	FKL_DECL_AND_CHECK_ARG3(loop_obj,node_obj,service_obj,exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FklVMvalue* proc_obj=FKL_VM_POP_ARG(exe);
	const char* node=NULL;
	const char* service=NULL;
	if(FKL_IS_STR(node_obj))
		node=FKL_VM_STR(node_obj)->str;
	else if(node_obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	if(FKL_IS_STR(service_obj))
		service=FKL_VM_STR(service_obj)->str;
	else if(service_obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	if(proc_obj&&proc_obj!=FKL_VM_NIL&&!fklIsCallable(proc_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_INCORRECT_TYPE_VALUE,exe);

	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	struct addrinfo hints={.ai_flags=0,};
	FklBuiltinErrorType err_type=process_addrinfo_hints(exe,&hints,fpd);
	if(err_type)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,err_type,exe);

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
		CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,addrinfo_to_value(exe,req.addrinfo,fpd));
		uv_freeaddrinfo(req.addrinfo);
	}
	else
	{
		FklVMvalue* retval=NULL;
		uv_getaddrinfo_t* req=createFuvGetaddrinfo(exe,&retval,ctx->proc,loop_obj,proc_obj);
		int r=uv_getaddrinfo(&fuv_loop->loop,req,fuv_getaddrinfo_cb,node,service,&hints);
		CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
		FKL_VM_PUSH_VALUE(exe,retval);
	}
	return 0;
}

#undef FUV_GETADDRINFO_PNAME

static int fuv_getnameinfo_p(FKL_CPROC_ARGL){PREDICATE(isFuvGetnameinfo(val),"fuv.getnameinfo?")}

static inline FklBuiltinErrorType process_sockaddr_flags(FklVM* exe
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
			if(id==fpd->aif_family_sid)
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
			if(id==fpd->aif_ip_sid)
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
			if(id==fpd->aif_port_sid)
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
	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->aif_hostname_sid)
			,hostname?fklCreateVMvalueStr(exe,fklCreateStringFromCstr(hostname)):FKL_VM_NIL
			,ht);
	fklVMhashTableSet(FKL_MAKE_VM_SYM(fpd->aif_service_sid)
			,service?fklCreateVMvalueStr(exe,fklCreateStringFromCstr(service)):FKL_VM_NIL
			,ht);
	return hash;
}

#define FUV_GETNAMEINFO_PNAME ("fuv.getnameinfo")

struct GetnameinfoValueCreateArg
{
	const char* hostname;
	const char* service;
	int status;
};

static void fuv_getnameinfo_cb_value_creator(FklVM* exe,void* a)
{
	struct GetnameinfoValueCreateArg* arg=a;
	FklVMframe* fuv_proc_call_frame=exe->top_frame;
	FklVMframe* run_loop_proc_frame=fuv_proc_call_frame->prev;
	FklVMvalue* fpd_obj=((FklCprocFrameContext*)run_loop_proc_frame->data)->pd;

	FklVMvalue* err=arg->status<0
		?createUvError(FUV_GETNAMEINFO_PNAME,arg->status,exe,fpd_obj)
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
	static const char Pname[]=FUV_GETNAMEINFO_PNAME;
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FklVMvalue* proc_obj=FKL_VM_POP_ARG(exe);
	FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,ctx->pd);
	struct sockaddr_storage addr={.ss_family=AF_UNSPEC};
	int flags=0;

	FklVMvalue* ip_obj=FKL_VM_NIL;
	FklVMvalue* port_obj=FKL_VM_NIL;
	int r=0;
	FklBuiltinErrorType error_type=process_sockaddr_flags(exe,&addr,&flags,fpd,&ip_obj,&port_obj,&r);
	if(error_type)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,error_type,exe);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);

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
		CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
		FKL_VM_SET_TP_AND_PUSH_VALUE(exe,rtp,host_service_to_hash(exe,req.host,req.service,fpd));
	}
	else
	{
		FklVMvalue* retval=NULL;
		uv_getnameinfo_t* req=createFuvGetnameinfo(exe,&retval,ctx->proc,loop_obj,proc_obj);
		int r=uv_getnameinfo(&fuv_loop->loop,req,fuv_getnameinfo_cb,(struct sockaddr*)&addr,flags);
		CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
		FKL_VM_PUSH_VALUE(exe,retval);
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
	{"loop?",                   fuv_loop_p,                  },
	{"make-loop",               fuv_make_loop,               },
	{"loop-close",              fuv_loop_close,              },
	{"loop-run",                fuv_loop_run,                },
	{"loop-mode",               fuv_loop_mode,               },
	{"loop-alive?",             fuv_loop_alive_p,            },
	{"loop-stop",               fuv_loop_stop,               },
	{"loop-backend-fd",         fuv_loop_backend_fd,         },
	{"loop-backend-timeout",    fuv_loop_backend_timeout,    },
	{"loop-now",                fuv_loop_now,                },
	{"loop-update-time",        fuv_loop_update_time,        },
	{"loop-walk",               fuv_loop_walk,               },
	{"loop-configure",          fuv_loop_configure,          },

	// handle
	{"handle?",                 fuv_handle_p,                },
	{"handle-active?",          fuv_handle_active_p,         },
	{"handle-closing?",         fuv_handle_closing_p,        },
	{"handle-close",            fuv_handle_close,            },
	{"handle-ref?",             fuv_handle_ref_p,            },
	{"handle-ref",              fuv_handle_ref,              },
	{"handle-unref",            fuv_handle_unref,            },
	{"handle-send-buffer-size", fuv_handle_send_buffer_size, },
	{"handle-recv-buffer-size", fuv_handle_recv_buffer_size, },
	{"handle-fileno",           fuv_handle_fileno,           },
	{"handle-type",             fuv_handle_type,             },

	// timer
	{"timer?",                  fuv_timer_p,                 },
	{"make-timer",              fuv_make_timer,              },
	{"timer-start",             fuv_timer_start,             },
	{"timer-stop",              fuv_timer_stop,              },
	{"timer-again",             fuv_timer_again,             },
	{"timer-due-in",            fuv_timer_due_in,            },
	{"timer-repeat",            fuv_timer_repeat,            },
	{"timer-repeat-set!",       fuv_timer_repeat_set1,       },

	// prepare
	{"prepare?",                fuv_prepare_p,               },
	{"make-prepare",            fuv_make_prepare,            },
	{"prepare-start",           fuv_prepare_start,           },
	{"prepare-stop",            fuv_prepare_stop,            },

	// idle
	{"idle?",                   fuv_idle_p,                  },
	{"make-idle",               fuv_make_idle,               },
	{"idle-start",              fuv_idle_start,              },
	{"idle-stop",               fuv_idle_stop,               },

	// check
	{"check?",                  fuv_check_p,                 },
	{"make-check",              fuv_make_check,              },
	{"check-start",             fuv_check_start,             },
	{"check-stop",              fuv_check_stop,              },

	// signal
	{"signal?",                 fuv_signal_p,                },
	{"make-signal",             fuv_make_signal,             },
	{"signal-start",            fuv_signal_start,            },
	{"signal-start-oneshot",    fuv_signal_start_oneshot,    },
	{"signal-stop",             fuv_signal_stop,             },

	// async
	{"async?",                  fuv_async_p,                 },
	{"make-async",              fuv_make_async,              },
	{"async-send",              fuv_async_send,              },

	// req
	{"req?",                    fuv_req_p,                   },
	{"req-cancel",              fuv_req_cancel,              },
	{"req-type",                fuv_req_type,                },

	// dns
	{"getaddrinfo?",            fuv_getaddrinfo_p,           },
	{"getaddrinfo",             fuv_getaddrinfo,             },
	{"getnameinfo?",            fuv_getnameinfo_p,           },
	{"getnameinfo",             fuv_getnameinfo,             },
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
