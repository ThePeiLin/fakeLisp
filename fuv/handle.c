#include"fuv.h"

static void fuv_handle_ud_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(fuv_handle,FuvHandleUd,ud);
	FuvHandle* handle=*fuv_handle;
	if(handle)
	{
		fklVMgcToGray(handle->data.loop,gc);
		fklVMgcToGray(handle->data.callbacks[0],gc);
		fklVMgcToGray(handle->data.callbacks[1],gc);
	}
}

static void fuv_handle_ud_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(handle_ud,FuvHandleUd,ud);
	FuvHandle* fuv_handle=*handle_ud;
	if(fuv_handle)
	{
		FuvHandleData* handle_data=&fuv_handle->data;
		FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,handle_data->loop);
		fklDelHashItem(&handle_data->handle,&fuv_loop->data.gc_values,NULL);
		fuv_handle->data.handle=NULL;
		*handle_ud=NULL;
	}
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_timer_as_print,timer);

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_prepare_as_print,prepare);

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_idle_as_print,idle);

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_check_as_print,check);

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_signal_as_print,signal);

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_async_as_print,async);

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_process_as_print,process);

static void fuv_process_ud_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(fuv_handle,FuvHandleUd,ud);
	struct FuvProcess* handle=(struct FuvProcess*)*fuv_handle;
	if(handle)
	{
		fklVMgcToGray(handle->data.loop,gc);
		fklVMgcToGray(handle->data.callbacks[0],gc);
		fklVMgcToGray(handle->data.callbacks[1],gc);
		fklVMgcToGray(handle->env_obj,gc);
		fklVMgcToGray(handle->args_obj,gc);
		fklVMgcToGray(handle->file_obj,gc);
		fklVMgcToGray(handle->stdio_obj,gc);
	}
}

static void fuv_pipe_ud_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(fuv_handle,FuvHandleUd,ud);
	struct FuvPipe* handle=(struct FuvPipe*)*fuv_handle;
	if(handle)
	{
		fklVMgcToGray(handle->data.loop,gc);
		fklVMgcToGray(handle->data.callbacks[0],gc);
		fklVMgcToGray(handle->data.callbacks[1],gc);
		fklVMgcToGray(handle->fp,gc);
	}
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_pipe_as_print,pipe);

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_tcp_as_print,tcp);

static void fuv_tty_ud_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(fuv_handle,FuvHandleUd,ud);
	struct FuvTTY* handle=(struct FuvTTY*)*fuv_handle;
	if(handle)
	{
		fklVMgcToGray(handle->data.loop,gc);
		fklVMgcToGray(handle->data.callbacks[0],gc);
		fklVMgcToGray(handle->data.callbacks[1],gc);
		fklVMgcToGray(handle->fp,gc);
	}
}

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_tty_as_print,tcp);

static const FklVMudMetaTable HandleMetaTables[UV_HANDLE_TYPE_MAX]=
{
	// UV_UNKNOWN_HANDLE
	{
	},

	// UV_ASYNC,
	{
		.size=sizeof(FuvHandleUd),
		.__as_prin1=fuv_async_as_print,
		.__as_princ=fuv_async_as_print,
		.__atomic=fuv_handle_ud_atomic,
		.__finalizer=fuv_handle_ud_finalizer,
	},

	// UV_CHECK,
	{
		.size=sizeof(FuvHandleUd),
		.__as_prin1=fuv_check_as_print,
		.__as_princ=fuv_check_as_print,
		.__atomic=fuv_handle_ud_atomic,
		.__finalizer=fuv_handle_ud_finalizer,
	},

	// UV_FS_EVENT,
	{
	},

	// UV_FS_POLL,
	{
	},

	// UV_HANDLE,
	{
	},

	// UV_IDLE,
	{
		.size=sizeof(FuvHandleUd),
		.__as_prin1=fuv_idle_as_print,
		.__as_princ=fuv_idle_as_print,
		.__atomic=fuv_handle_ud_atomic,
		.__finalizer=fuv_handle_ud_finalizer,
	},

	// UV_NAMED_PIPE,
	{
		.size=sizeof(FuvHandleUd),
		.__as_prin1=fuv_pipe_as_print,
		.__as_princ=fuv_pipe_as_print,
		.__atomic=fuv_pipe_ud_atomic,
		.__finalizer=fuv_handle_ud_finalizer,
	},

	// UV_POLL,
	{
	},

	// UV_PREPARE,
	{
		.size=sizeof(FuvHandleUd),
		.__as_prin1=fuv_prepare_as_print,
		.__as_princ=fuv_prepare_as_print,
		.__atomic=fuv_handle_ud_atomic,
		.__finalizer=fuv_handle_ud_finalizer,
	},

	// UV_PROCESS,
	{
		.size=sizeof(FuvHandleUd),
		.__as_prin1=fuv_process_as_print,
		.__as_princ=fuv_process_as_print,
		.__atomic=fuv_process_ud_atomic,
		.__finalizer=fuv_handle_ud_finalizer,
	},

	// UV_STREAM,
	{
	},

	// UV_TCP,
	{
		.size=sizeof(FuvHandleUd),
		.__as_prin1=fuv_tcp_as_print,
		.__as_princ=fuv_tcp_as_print,
		.__atomic=fuv_handle_ud_atomic,
		.__finalizer=fuv_handle_ud_finalizer,
	},

	// UV_TIMER,
	{
		.size=sizeof(FuvHandleUd),
		.__as_prin1=fuv_timer_as_print,
		.__as_princ=fuv_timer_as_print,
		.__atomic=fuv_handle_ud_atomic,
		.__finalizer=fuv_handle_ud_finalizer,
	},

	// UV_TTY,
	{
		.size=sizeof(FuvHandleUd),
		.__as_prin1=fuv_tty_as_print,
		.__as_princ=fuv_tty_as_print,
		.__atomic=fuv_tty_ud_atomic,
		.__finalizer=fuv_handle_ud_finalizer,
	},

	// UV_UDP,
	{
	},

	// UV_SIGNAL,
	{
		.size=sizeof(FuvHandleUd),
		.__as_prin1=fuv_signal_as_print,
		.__as_princ=fuv_signal_as_print,
		.__atomic=fuv_handle_ud_atomic,
		.__finalizer=fuv_handle_ud_finalizer,
	},

	// UV_FILE,
	{
	},
};

int isFuvHandle(FklVMvalue* v)
{
	if(FKL_IS_USERDATA(v))
	{
		const FklVMudMetaTable* t=FKL_VM_UD(v)->t;
		return t>&HandleMetaTables[UV_UNKNOWN_HANDLE]
			&&t<&HandleMetaTables[UV_HANDLE_TYPE_MAX];
	}
	return 0;
}

static inline void init_fuv_handle(FuvHandleUd* h_ud
		,FuvHandle* handle
		,FklVMvalue* v
		,FklVMvalue* loop)
{
	*h_ud=handle;
	handle->data.handle=v;
	handle->data.loop=loop;
	handle->data.callbacks[0]=NULL;
	handle->data.callbacks[1]=NULL;
	uv_handle_set_data(&handle->handle,handle);
	fuvLoopInsertFuvObj(loop,v);
}

struct FuvTimer
{
	FuvHandleData data;
	uv_timer_t handle;
};

#define CREATE_OBJ(TYPE) (TYPE*)malloc(sizeof(TYPE))

#define FUV_HANDLE_P(NAME,ENUM) int NAME(FklVMvalue* v)\
{\
	return FKL_IS_USERDATA(v)\
		&&FKL_VM_UD(v)->t==&HandleMetaTables[ENUM];\
}

#define FUV_HANDLE_CREATOR(TYPE,NAME,ENUM) FklVMvalue* create##TYPE(FklVM* vm\
		,FklVMvalue* rel\
		,FklVMvalue* loop_obj\
		,int* err)\
{\
	FklVMvalue* v=fklCreateVMvalueUd(vm,&HandleMetaTables[ENUM],rel);\
	FKL_DECL_VM_UD_DATA(hud,FuvHandleUd,v);\
	FKL_DECL_VM_UD_DATA(loop,FuvLoop,loop_obj);\
	struct TYPE* handle=CREATE_OBJ(struct TYPE);\
	FKL_ASSERT(handle);\
	init_fuv_handle(hud,(FuvHandle*)handle,v,loop_obj);\
	*err=uv_##NAME##_init(&loop->loop,&handle->handle);\
	return v;\
}

FUV_HANDLE_P(isFuvTimer,UV_TIMER);
FUV_HANDLE_CREATOR(FuvTimer,timer,UV_TIMER);

struct FuvPrepare
{
	FuvHandleData data;
	uv_prepare_t handle;
};

FUV_HANDLE_P(isFuvPrepare,UV_PREPARE);
FUV_HANDLE_CREATOR(FuvPrepare,prepare,UV_PREPARE);

struct FuvIdle
{
	FuvHandleData data;
	uv_idle_t handle;
};

FUV_HANDLE_P(isFuvIdle,UV_IDLE);
FUV_HANDLE_CREATOR(FuvIdle,idle,UV_IDLE);

struct FuvCheck
{
	FuvHandleData data;
	uv_check_t handle;
};

FUV_HANDLE_P(isFuvCheck,UV_CHECK);
FUV_HANDLE_CREATOR(FuvCheck,check,UV_CHECK);

struct FuvSignal
{
	FuvHandleData data;
	uv_signal_t handle;
};

FUV_HANDLE_P(isFuvSignal,UV_SIGNAL);
FUV_HANDLE_CREATOR(FuvSignal,signal,UV_SIGNAL);

FUV_HANDLE_P(isFuvAsync,UV_ASYNC);

static void fuv_async_cb(uv_async_t* handle)
{
	uv_loop_t* loop=uv_handle_get_loop((uv_handle_t*)handle);
	FuvLoopData* ldata=uv_loop_get_data(loop);
	struct FuvAsync* async_handle=uv_handle_get_data((uv_handle_t*)handle);
	FuvHandleData* hdata=&async_handle->data;
	FklVMvalue* proc=hdata->callbacks[0];
	if(proc)
	{
		FklVM* exe=ldata->exe;
		fklLockThread(exe);
		uint32_t sbp=exe->bp;
		uint32_t stp=exe->tp;
		uint32_t ltp=exe->ltp;
		exe->state=FKL_VM_READY;
		FklVMframe* buttom_frame=exe->top_frame;
		struct FuvAsyncExtraData* extra=atomic_load(&async_handle->extra);
		fklSetBp(exe);
		FklVMvalue** cur=extra->base;
		FklVMvalue** const end=&cur[extra->num];
		for(;cur<end;cur++)
			FKL_VM_PUSH_VALUE(exe,*cur);
		fklCallObj(exe,proc);
		FUV_ASYNC_COPY_DONE(async_handle);
		FUV_ASYNC_WAIT_SEND(exe,async_handle);
		if(exe->thread_run_cb(exe,buttom_frame))
			startErrorHandle(loop,ldata,exe,sbp,stp,ltp,buttom_frame);
		else
			exe->tp--;
		fklUnlockThread(exe);
		return;
	}
}

FklVMvalue* createFuvAsync(FklVM* vm
		,FklVMvalue* rel
		,FklVMvalue* loop_obj
		,FklVMvalue* proc_obj
		,int* err)
{
	FklVMvalue* v=fklCreateVMvalueUd(vm,&HandleMetaTables[UV_ASYNC],rel);
	FKL_DECL_VM_UD_DATA(hud,FuvHandleUd,v);
	FKL_DECL_VM_UD_DATA(loop,FuvLoop,loop_obj);
	struct FuvAsync* handle=CREATE_OBJ(struct FuvAsync);
	FKL_ASSERT(handle);
	handle->extra=NULL;
	handle->send_ready=(atomic_flag)ATOMIC_FLAG_INIT;
	handle->copy_done=(atomic_flag)ATOMIC_FLAG_INIT;
	atomic_flag_test_and_set(&handle->copy_done);
	atomic_flag_test_and_set(&handle->send_done);
	init_fuv_handle(hud,(FuvHandle*)handle,v,loop_obj);
	handle->data.callbacks[0]=proc_obj;
	*err=uv_async_init(&loop->loop,&handle->handle,fuv_async_cb);
	return v;
}

FUV_HANDLE_P(isFuvProcess,UV_PROCESS);

uv_process_t* createFuvProcess(FklVM* vm
		,FklVMvalue** pr
		,FklVMvalue* rel
		,FklVMvalue* loop_obj
		,FklVMvalue* proc_obj
		,FklVMvalue* args_obj
		,FklVMvalue* env_obj
		,FklVMvalue* file_obj
		,FklVMvalue* stdio_obj
		,FklVMvalue* cwd_obj)
{
	FklVMvalue* v=fklCreateVMvalueUd(vm,&HandleMetaTables[UV_PROCESS],rel);
	FKL_DECL_VM_UD_DATA(hud,FuvHandleUd,v);
	struct FuvProcess* handle=CREATE_OBJ(struct FuvProcess);
	FKL_ASSERT(handle);
	init_fuv_handle(hud,(FuvHandle*)handle,v,loop_obj);
	handle->data.callbacks[0]=proc_obj;
	handle->args_obj=args_obj;
	handle->env_obj=env_obj;
	handle->file_obj=file_obj;
	handle->stdio_obj=stdio_obj;
	handle->cwd_obj=cwd_obj;
	*pr=v;
	return &handle->handle;
}
#undef CREATE_OBJ

int isFuvStream(FklVMvalue* v)
{
	if(FKL_IS_USERDATA(v))
	{
		const FklVMudMetaTable* t=FKL_VM_UD(v)->t;
		return t==&HandleMetaTables[UV_NAMED_PIPE]
			||t==&HandleMetaTables[UV_TTY]
			||t==&HandleMetaTables[UV_TCP]
			||t==&HandleMetaTables[UV_STREAM];
	}
	return 0;
}

FUV_HANDLE_P(isFuvPipe,UV_NAMED_PIPE);

#define CREATE_OBJ(TYPE) (TYPE*)calloc(1,sizeof(TYPE))

#define OTHER_HANDLE_CREATOR(TYPE,NAME,ENUM) uv_##NAME##_t* create##TYPE(FklVM* vm,FklVMvalue** pr,FklVMvalue* rel,FklVMvalue* loop_obj)\
{\
	FklVMvalue* v=fklCreateVMvalueUd(vm,&HandleMetaTables[ENUM],rel);\
	FKL_DECL_VM_UD_DATA(hud,FuvHandleUd,v);\
	struct TYPE* handle=CREATE_OBJ(struct TYPE);\
	FKL_ASSERT(handle);\
	init_fuv_handle(hud,(FuvHandle*)handle,v,loop_obj);\
	*pr=v;\
	return &handle->handle;\
}

OTHER_HANDLE_CREATOR(FuvPipe,pipe,UV_NAMED_PIPE);

struct FuvTcp
{
	FuvHandleData data;
	uv_tcp_t handle;
};

FUV_HANDLE_P(isFuvTcp,UV_TCP);

OTHER_HANDLE_CREATOR(FuvTcp,tcp,UV_TCP);

FUV_HANDLE_P(isFuvTTY,UV_TTY);
OTHER_HANDLE_CREATOR(FuvTTY,tty,UV_TTY);

#undef FUV_HANDLE_P
#undef FUV_HANDLE_CREATOR
#undef OTHER_HANDLE_CREATOR
#undef CREATE_OBJ
