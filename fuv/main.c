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

static inline void init_fuv_public_data(FuvPublicData* pd,FklVM* exe)
{
	static const char* loop_mode[]=
	{
		"default",
		"once",
		"nowait",
	};
	pd->loop_mode[UV_RUN_DEFAULT]=fklVMaddSymbolCstr(exe->gc,loop_mode[UV_RUN_DEFAULT])->id;
	pd->loop_mode[UV_RUN_ONCE]=fklVMaddSymbolCstr(exe->gc,loop_mode[UV_RUN_ONCE])->id;
	pd->loop_mode[UV_RUN_NOWAIT]=fklVMaddSymbolCstr(exe->gc,loop_mode[UV_RUN_NOWAIT])->id;

	static const char* fuv_err_sym[]=
	{
		"dummy",
		"fuv-handle-error",
		"fuv-handle-error",
	};
	for(size_t i=0;i<FUV_ERR_NUM;i++)
		pd->fuv_err_sid[i]=fklVMaddSymbolCstr(exe->gc,fuv_err_sym[i])->id;

	static const char* loop_config_sym[]=
	{
		"loop-block-signal",
		"metrics-idle-time",
	};
	pd->loop_block_signal_sid=fklVMaddSymbolCstr(exe->gc,loop_config_sym[UV_LOOP_BLOCK_SIGNAL])->id;
	pd->metrics_idle_time_sid=fklVMaddSymbolCstr(exe->gc,loop_config_sym[UV_METRICS_IDLE_TIME])->id;

#define XX(code,_) pd->uv_err_sid_##code=fklVMaddSymbolCstr(exe->gc,"UV-"#code)->id;
	UV_ERRNO_MAP(XX);
#undef XX

#ifdef SIGHUP
	pd->SIGHUP_sid=fklVMaddSymbolCstr(exe->gc,"sighup")->id;
#endif
#ifdef SIGINT
	pd->SIGINT_sid=fklVMaddSymbolCstr(exe->gc,"sigint")->id;
#endif
#ifdef SIGQUIT
	pd->SIGQUIT_sid=fklVMaddSymbolCstr(exe->gc,"sigquit")->id;
#endif
#ifdef SIGILL
	pd->SIGILL_sid=fklVMaddSymbolCstr(exe->gc,"sigill")->id;
#endif
#ifdef SIGTRAP
	pd->SIGTRAP_sid=fklVMaddSymbolCstr(exe->gc,"sigtrap")->id;
#endif
#ifdef SIGABRT
	pd->SIGABRT_sid=fklVMaddSymbolCstr(exe->gc,"sigabrt")->id;
#endif
#ifdef SIGIOT
	pd->SIGIOT_sid=fklVMaddSymbolCstr(exe->gc,"sigiot")->id;
#endif
#ifdef SIGBUS
	pd->SIGBUS_sid=fklVMaddSymbolCstr(exe->gc,"sigbus")->id;
#endif
#ifdef SIGFPE
	pd->SIGFPE_sid=fklVMaddSymbolCstr(exe->gc,"sigfpe")->id;
#endif
#ifdef SIGKILL
	pd->SIGKILL_sid=fklVMaddSymbolCstr(exe->gc,"sigkill")->id;
#endif
#ifdef SIGUSR1
	pd->SIGUSR1_sid=fklVMaddSymbolCstr(exe->gc,"sigusr1")->id;
#endif
#ifdef SIGSEGV
	pd->SIGSEGV_sid=fklVMaddSymbolCstr(exe->gc,"sigsegv")->id;
#endif
#ifdef SIGUSR2
	pd->SIGUSR2_sid=fklVMaddSymbolCstr(exe->gc,"sigusr2")->id;
#endif
#ifdef SIGPIPE
	pd->SIGPIPE_sid=fklVMaddSymbolCstr(exe->gc,"sigpipe")->id;
#endif
#ifdef SIGALRM
	pd->SIGALRM_sid=fklVMaddSymbolCstr(exe->gc,"sigalrm")->id;
#endif
#ifdef SIGTERM
	pd->SIGTERM_sid=fklVMaddSymbolCstr(exe->gc,"sigterm")->id;
#endif
#ifdef SIGCHLD
	pd->SIGCHLD_sid=fklVMaddSymbolCstr(exe->gc,"sigchld")->id;
#endif
#ifdef SIGSTKFLT
	pd->SIGSTKFLT_sid=fklVMaddSymbolCstr(exe->gc,"sigstkflt")->id;
#endif
#ifdef SIGCONT
	pd->SIGCONT_sid=fklVMaddSymbolCstr(exe->gc,"sigcont")->id;
#endif
#ifdef SIGSTOP
	pd->SIGSTOP_sid=fklVMaddSymbolCstr(exe->gc,"sigstop")->id;
#endif
#ifdef SIGTSTP
	pd->SIGTSTP_sid=fklVMaddSymbolCstr(exe->gc,"sigtstp")->id;
#endif
#ifdef SIGBREAK
	pd->SIGBREAK_sid=fklVMaddSymbolCstr(exe->gc,"sigbreak")->id;
#endif
#ifdef SIGTTIN
	pd->SIGTTIN_sid=fklVMaddSymbolCstr(exe->gc,"sigttin")->id;
#endif
#ifdef SIGTTOU
	pd->SIGTTOU_sid=fklVMaddSymbolCstr(exe->gc,"sigttou")->id;
#endif
#ifdef SIGURG
	pd->SIGURG_sid=fklVMaddSymbolCstr(exe->gc,"sigurg")->id;
#endif
#ifdef SIGXCPU
	pd->SIGXCPU_sid=fklVMaddSymbolCstr(exe->gc,"sigxcpu")->id;
#endif
#ifdef SIGXFSZ
	pd->SIGXFSZ_sid=fklVMaddSymbolCstr(exe->gc,"sigxfsz")->id;
#endif
#ifdef SIGVTALRM
	pd->SIGVTALRM_sid=fklVMaddSymbolCstr(exe->gc,"sigvtalrm")->id;
#endif
#ifdef SIGPROF
	pd->SIGPROF_sid=fklVMaddSymbolCstr(exe->gc,"sigprof")->id;
#endif
#ifdef SIGWINCH
	pd->SIGWINCH_sid=fklVMaddSymbolCstr(exe->gc,"sigwinch")->id;
#endif
#ifdef SIGIO
	pd->SIGIO_sid=fklVMaddSymbolCstr(exe->gc,"sigio")->id;
#endif
#ifdef SIGPOLL
	pd->SIGPOLL_sid=fklVMaddSymbolCstr(exe->gc,"sigpoll")->id;
#endif
#ifdef SIGLOST
	pd->SIGLOST_sid=fklVMaddSymbolCstr(exe->gc,"siglost")->id;
#endif
#ifdef SIGPWR
	pd->SIGPWR_sid=fklVMaddSymbolCstr(exe->gc,"sigpwr")->id;
#endif
#ifdef SIGSYS
	pd->SIGSYS_sid=fklVMaddSymbolCstr(exe->gc,"sigsys")->id;
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

static int fuv_loop_close1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-close1";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_loop_close(&fuv_loop->loop);
	CHECK_UV_RESULT(r,Pname,exe,ctx->pd);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

typedef enum
{
	FUV_RUN_OK=0,
	FUV_RUN_ERR_OCCUR,
}FuvLoopRunStatus;

static int fuv_loop_error_callback(FKL_VM_ERROR_CALLBACK_ARGL)
{
	FklCprocFrameContext* ctx=(FklCprocFrameContext*)f->data;
	vm->tp=ctx->rtp;
	FklVMvalue* loop_obj=FKL_VM_POP_TOP_VALUE(vm);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	FKL_VM_PUSH_VALUE(vm,ev);
	longjmp(fuv_loop->data.buf,FUV_RUN_ERR_OCCUR);
}

typedef struct
{
	jmp_buf* buf;
}FuvProcCallCtx;

static int fuv_proc_call_frame_end(void* data)
{
	return 0;
}

static void fuv_proc_call_frame_step(void* data,FklVM* exe)
{
	FuvProcCallCtx* ctx=(FuvProcCallCtx*)data;
	longjmp(*ctx->buf,1);
}

static const FklVMframeContextMethodTable FuvProcCallCtxMethodTable=
{
	.end=fuv_proc_call_frame_end,
	.step=fuv_proc_call_frame_step,
};

static int fuv_loop_run1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-run!";
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
				origin_top_frame->errorCallBack=fuv_loop_error_callback;
				if(setjmp(fuv_loop->data.buf))
				{
					ctx->context=1;
					need_continue=1;
				}
				else
				{
					exe->top_frame=fklCreateOtherObjVMframe(exe,&FuvProcCallCtxMethodTable,origin_top_frame);
					fuv_loop->data.mode=mode;
					int r=uv_run(&fuv_loop->loop,mode);
					if(r<0)
					{
						FKL_VM_PUSH_VALUE(exe,createUvError(Pname,r,exe,ctx->pd));
						ctx->context=1;
						need_continue=1;
					}
				}
				while(exe->top_frame!=origin_top_frame)
					fklPopVMframe(exe);
				fuv_loop->data.mode=-1;
				exe->state=FKL_VM_READY;
				origin_top_frame->errorCallBack=NULL;
				return need_continue;
			}
			break;
		case 1:
			fklRaiseVMerror(FKL_VM_POP_TOP_VALUE(exe),exe);
			break;
	}
	return 0;
}

static void fuv_loop_walk_cb(uv_handle_t* handle,void* arg)
{
	FklVM* exe=arg;
	FklVMvalue* proc=FKL_VM_GET_TOP_VALUE(exe);
	exe->state=FKL_VM_READY;
	FklVMframe* fuv_proc_call_frame=exe->top_frame;
	FuvProcCallCtx* ctx=(FuvProcCallCtx*)fuv_proc_call_frame->data;
	jmp_buf buf;
	ctx->buf=&buf;
	if(setjmp(buf))
	{
		exe->tp--;
		return;
	}
	else
	{
		FuvHandle* fuv_handle=uv_handle_get_data(handle);
		FuvHandleData* handle_data=&fuv_handle->data;
		fklSetBp(exe);
		FKL_VM_PUSH_VALUE(exe,handle_data->handle);
		fklCallObj(exe,proc);
		exe->thread_run_cb(exe);
	}
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
				uint32_t rtp=exe->tp;
				FKL_VM_PUSH_VALUE(exe,proc_obj);
				FklVMframe* origin_top_frame=exe->top_frame;
				int need_continue=0;
				origin_top_frame->errorCallBack=fuv_loop_error_callback;
				if(setjmp(fuv_loop->data.buf))
				{
					ctx->context=1;
					need_continue=1;
				}
				else
				{
					exe->top_frame=fklCreateOtherObjVMframe(exe,&FuvProcCallCtxMethodTable,origin_top_frame);
					uv_walk(&fuv_loop->loop,fuv_loop_walk_cb,exe);
				}
				while(exe->top_frame!=origin_top_frame)
					fklPopVMframe(exe);
				exe->state=FKL_VM_READY;
				origin_top_frame->errorCallBack=NULL;
				exe->tp=rtp;
				return need_continue;
			}
			break;
		case 1:
			fklRaiseVMerror(FKL_VM_POP_TOP_VALUE(exe),exe);
			break;
	}
	return 0;
}

static int fuv_loop_configure1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-configure!";
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
						int signum=sigSymbolToSignum(sid,fpd);
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

static int fuv_loop_stop1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-stop!";
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

static int fuv_loop_update_time1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-update-time!";
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
		,FuvLoopData* loop_data
		,int idx)
{
	FklVMvalue* proc=handle_data->callbacks[idx];
	if(proc)
	{
		FklVM* exe=loop_data->exe;
		exe->state=FKL_VM_READY;
		FklVMframe* fuv_proc_call_frame=exe->top_frame;
		FuvProcCallCtx* ctx=(FuvProcCallCtx*)fuv_proc_call_frame->data;
		jmp_buf buf;
		ctx->buf=&buf;
		if(setjmp(buf))
		{
			exe->tp--;
			return;
		}
		else
		{
			fklSetBp(exe);
			fklCallObj(exe,proc);
			exe->thread_run_cb(exe);
		}
	}
}

static void fuv_close_cb(uv_handle_t* handle)
{
	FuvLoopData* ldata=uv_loop_get_data(uv_handle_get_loop(handle));
	FuvHandle* fuv_handle=uv_handle_get_data(handle);
	FuvHandleData* hdata=&fuv_handle->data;
	fuv_call_handle_callback_in_loop(handle
			,hdata
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

static int fuv_handle_close1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-close!";
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

static int fuv_handle_ref1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-ref!";
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

static int fuv_handle_unref1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-unref!";
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

static int fuv_handle_send_buffer_size1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-send-buffer-size!";
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

static int fuv_handle_recv_buffer_size1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-recv-buffer-size!";
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
	FKL_VM_PUSH_VALUE(exe,
			name==NULL
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
	FuvLoopData* ldata=uv_loop_get_data(uv_handle_get_loop((uv_handle_t*)handle));
	FuvHandleData* hdata=&((FuvHandle*)uv_handle_get_data((uv_handle_t*)handle))->data;
	fuv_call_handle_callback_in_loop((uv_handle_t*)handle
			,hdata
			,ldata
			,0);
}

static int fuv_timer_start1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-start!";
	FKL_DECL_AND_CHECK_ARG3(timer_obj,timer_cb,timeout_obj,exe,Pname);
	FklVMvalue* repeat_obj=FKL_VM_POP_ARG(exe);
	if(repeat_obj==NULL)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
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

static int fuv_timer_stop1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-stop!";
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

static int fuv_timer_again1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-again!";
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
	FuvLoopData* ldata=uv_loop_get_data(uv_handle_get_loop((uv_handle_t*)handle));
	FuvHandleData* hdata=&((FuvHandle*)uv_handle_get_data((uv_handle_t*)handle))->data;
	fuv_call_handle_callback_in_loop((uv_handle_t*)handle
			,hdata
			,ldata
			,0);
}


static int fuv_prepare_start1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.prepare-start!";
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

static int fuv_prepare_stop1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.prepare-stop!";
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
	{"loop?",                    fuv_loop_p,                   },
	{"make-loop",                fuv_make_loop,                },
	{"loop-close!",              fuv_loop_close1,              },
	{"loop-run!",                fuv_loop_run1,                },
	{"loop-mode",                fuv_loop_mode,                },
	{"loop-alive?",              fuv_loop_alive_p,             },
	{"loop-stop!",               fuv_loop_stop1,               },
	{"loop-backend-fd",          fuv_loop_backend_fd,          },
	{"loop-backend-timeout",     fuv_loop_backend_timeout,     },
	{"loop-now",                 fuv_loop_now,                 },
	{"loop-update-time!",        fuv_loop_update_time1,        },
	{"loop-walk",                fuv_loop_walk,                },
	{"loop-configure!",          fuv_loop_configure1,          },

	// handle
	{"handle?",                  fuv_handle_p,                 },
	{"handle-active?",           fuv_handle_active_p,          },
	{"handle-closing?",          fuv_handle_closing_p,         },
	{"handle-close!",            fuv_handle_close1,            },
	{"handle-ref?",              fuv_handle_ref_p,             },
	{"handle-ref!",              fuv_handle_ref1,              },
	{"handle-unref!",            fuv_handle_unref1,            },
	{"handle-send-buffer-size!", fuv_handle_send_buffer_size1, },
	{"handle-recv-buffer-size!", fuv_handle_recv_buffer_size1, },
	{"handle-fileno",            fuv_handle_fileno,            },
	{"handle-type",              fuv_handle_type,              },

	// req
	{"req?",                     fuv_incomplete,               },
	{"req-cancel!",              fuv_incomplete,               },
	{"req-type",                 fuv_incomplete,               },

	// timer
	{"timer?",                   fuv_timer_p,                  },
	{"make-timer",               fuv_make_timer,               },
	{"timer-start!",             fuv_timer_start1,             },
	{"timer-stop!",              fuv_timer_stop1,              },
	{"timer-again!",             fuv_timer_again1,             },
	{"timer-due-in",             fuv_timer_due_in,             },
	{"timer-repeat",             fuv_timer_repeat,             },
	{"timer-repeat-set!",        fuv_timer_repeat_set1,        },

	{"prepare?",                 fuv_prepare_p,                },
	{"make-prepare",             fuv_make_prepare,             },
	{"prepare-start!",           fuv_prepare_start1,           },
	{"prepare-stop!",            fuv_prepare_stop1,            },
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
	init_fuv_public_data(pd,exe);

	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklVMaddSymbolCstr(exe->gc,exports_and_func[i].sym)->id;
		FklVMcFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,fpd,id);
		loc[i]=dlproc;
	}
	return loc;
}
