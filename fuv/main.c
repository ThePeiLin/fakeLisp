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
#define XX(code,_) pd->uv_err_sid_##code=fklVMaddSymbolCstr(exe->gc,"UV-"#code)->id;
	UV_ERRNO_MAP(XX)
#undef XX
	static const char* fuv_err_sym[]=
	{
		"dummy",
		"fuv-close-error",
	};
	for(size_t i=0;i<FUV_ERR_NUM;i++)
		pd->fuv_err_sid[i]=fklVMaddSymbolCstr(exe->gc,fuv_err_sym[i])->id;
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
				int need_cont=0;
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
					need_cont=1;
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
						need_cont=1;
					}
				}
				while(exe->top_frame!=origin_top_frame)
					fklPopVMframe(exe);
				fuv_loop->data.mode=-1;
				exe->state=FKL_VM_READY;
				origin_top_frame->errorCallBack=NULL;
				return need_cont;
			}
			break;
		case 1:
			fklRaiseVMerror(FKL_VM_POP_TOP_VALUE(exe),exe);
			break;
	}
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
		FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,FKL_VM_CPROC(ctx->proc)->pd);
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
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandle,handle_obj);
	FKL_VM_PUSH_VALUE(exe,uv_is_active(&fuv_handle->handle)
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
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandle,handle_obj);
	FKL_VM_PUSH_VALUE(exe,uv_is_closing(&fuv_handle->handle)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
	return 0;
}

static inline void fuv_call_proc_in_loop(uv_handle_t* handle,int idx)
{
	FuvHandleData* handle_data=uv_handle_get_data(handle);
	FklVMvalue* proc=handle_data->callbacks[idx];
	if(proc)
	{
		uv_loop_t* loop=uv_handle_get_loop(handle);
		FuvLoopData* loop_data=uv_loop_get_data(loop);
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
	fuv_call_proc_in_loop(handle,1);
	uv_loop_t* loop=uv_handle_get_loop(handle);
	FuvLoopData* ldata=uv_loop_get_data(loop);
	FuvHandleData* hdata=uv_handle_get_data(handle);
	FklVMvalue* handle_obj=hdata->handle;
	hdata->loop=NULL;
	fklDelHashItem(&handle_obj,&ldata->gc_values,NULL);
}

static int fuv_handle_close1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-close!";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FklVMvalue* proc_obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandle,handle_obj);
	if(uv_is_closing(&fuv_handle->handle))
		raiseFuvError(Pname,FUV_ERR_CLOSE_CLOSEING_HANDLE,exe,ctx->pd);
	if(proc_obj)
	{
		FKL_CHECK_TYPE(proc_obj,fklIsCallable,Pname,exe);
		fuv_handle->data.callbacks[1]=proc_obj;
	}
	uv_close(&fuv_handle->handle,fuv_close_cb);
	FKL_VM_PUSH_VALUE(exe,handle_obj);
	return 0;
}

static int fuv_handle_ref_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-ref?";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandle,handle_obj);
	FKL_VM_PUSH_VALUE(exe,uv_has_ref(&fuv_handle->handle)
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
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandle,handle_obj);
	uv_ref(&fuv_handle->handle);
	FKL_VM_PUSH_VALUE(exe,handle_obj);
	return 0;
}

static int fuv_handle_unref1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.handle-unref!";
	FKL_DECL_AND_CHECK_ARG(handle_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(handle_obj,isFuvHandle,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandle,handle_obj);
	uv_unref(&fuv_handle->handle);
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
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandle,handle_obj);
	int value=0;
	if(value_obj)
	{
		FKL_CHECK_TYPE(value_obj,fklIsVMint,Pname,exe);
		value=fklGetInt(value_obj);
	}
	int r=uv_send_buffer_size(&fuv_handle->handle,&value);
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
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandle,handle_obj);
	int value=0;
	if(value_obj)
	{
		FKL_CHECK_TYPE(value_obj,fklIsVMint,Pname,exe);
		value=fklGetInt(value_obj);
	}
	int r=uv_recv_buffer_size(&fuv_handle->handle,&value);
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
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandle,handle_obj);
	uv_os_fd_t fd=0;
	int r=uv_fileno(&fuv_handle->handle,&fd);
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
	FKL_DECL_VM_UD_DATA(fuv_handle,FuvHandle,handle_obj);
	uv_handle_type type_id=uv_handle_get_type(&fuv_handle->handle);
	const char* name=uv_handle_type_name(type_id);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe
				,name==NULL
				?FKL_VM_NIL
				:fklCreateVMvalueStr(exe,fklCreateStringFromCstr(name))
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
	fuv_call_proc_in_loop((uv_handle_t*)handle,0);
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
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	timer->data.callbacks[0]=timer_cb;
	int r=uv_timer_start(&timer->handle,fuv_timer_cb,timeout,repeat);
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
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	int r=uv_timer_stop(&timer->handle);
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
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	int r=uv_timer_again(&timer->handle);
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
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(uv_timer_get_repeat(&timer->handle),exe));
	return 0;
}


static int fuv_timer_repeat_set1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-repeat-set!";
	FKL_DECL_AND_CHECK_ARG2(timer_obj,repeat_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_CHECK_TYPE(repeat_obj,fklIsVMint,Pname,exe);
	if(fklIsVMnumberLt0(repeat_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	uv_timer_set_repeat(&timer->handle,fklGetUint(repeat_obj));
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_due_in(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-repeat";
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(uv_timer_get_due_in(&timer->handle),exe));
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
	{"loop-stop!",               fuv_incomplete,               },
	{"loop-backend-fd",          fuv_loop_backend_fd,          },
	{"loop-backend-timeout",     fuv_loop_backend_timeout,     },
	{"loop-now",                 fuv_loop_now,                 },
	{"loop-update-time!",        fuv_loop_update_time1,        },
	{"loop-walk!",               fuv_incomplete,               },
	{"loop-configure!",          fuv_incomplete,               },

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

	// timer
	{"timer?",                   fuv_timer_p,                  },
	{"make-timer",               fuv_make_timer,               },
	{"timer-start!",             fuv_timer_start1,             },
	{"timer-stop!",              fuv_timer_stop1,              },
	{"timer-again!",             fuv_timer_again1,             },
	{"timer-due-in",             fuv_timer_due_in,             },
	{"timer-repeat",             fuv_timer_repeat,             },
	{"timer-repeat-set!",        fuv_timer_repeat_set1,        },
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
