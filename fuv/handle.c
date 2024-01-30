#include"fuv.h"

static void fuv_handle_ud_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(fuv_timer,FuvHandleUd,ud);
	FuvHandle* handle=*fuv_timer;
	if(handle)
	{
		fklVMgcToGray(handle->data.loop,gc);
		fklVMgcToGray(handle->data.callbacks[0],gc);
	}
}

static void fuv_handle_ud_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(handle_ud,FuvHandleUd,ud);
	FuvHandle* fuv_handle=*handle_ud;
	if(fuv_handle)
	{
		uv_handle_t* handle=&fuv_handle->handle;
		FuvHandleData* handle_data=&fuv_handle->data;
		uv_loop_t* loop=uv_handle_get_loop(handle);
		FuvLoopData* loop_data=uv_loop_get_data(loop);
		fklDelHashItem(&handle_data->handle,&loop_data->gc_values,NULL);
		fuv_handle->data.handle=NULL;
		*handle_ud=NULL;
	}
}

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_timer_print,timer);

static const FklVMudMetaTable HandleMetaTables[UV_HANDLE_TYPE_MAX]=
{
	// UV_UNKNOWN_HANDLE
	{
	},

	// UV_ASYNC,
	{
	},

	// UV_CHECK,
	{
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
	},

	// UV_NAMED_TYPE,
	{
	},

	// UV_POLL,
	{
	},

	// UV_PREPARE,
	{
	},

	// UV_PROCESS,
	{
	},

	// UV_STREAM,
	{
	},

	// UV_TCP,
	{
	},

	// UV_TIMER,
	{
		.size=sizeof(FuvHandleUd),
		.__prin1=fuv_timer_print,
		.__princ=fuv_timer_print,
		.__atomic=fuv_handle_ud_atomic,
		.__finalizer=fuv_handle_ud_finalizer,
	},

	// UV_TTY,
	{
	},

	// UV_UDP,
	{
	},

	// UV_SIGNAL,
	{
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

int isFuvTimer(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)
		&&FKL_VM_UD(v)->t==&HandleMetaTables[UV_TIMER];
}

void initFuvHandle(FklVMvalue* v,FuvHandle* handle,FklVMvalue* loop)
{
	handle->data.handle=v;
	handle->data.loop=loop;
	handle->data.callbacks[0]=FKL_VM_NIL;
	handle->data.callbacks[1]=NULL;
	uv_handle_set_data(&handle->handle,handle);
	fuvLoopInsertFuvHandle(loop,v);
}

struct FuvTimer
{
	FuvHandleData data;
	uv_timer_t handle;
};

#define CREATE_HANDLE(TYPE) (TYPE*)malloc(sizeof(TYPE))

FklVMvalue* createFuvTimer(FklVM* vm
		,FklVMvalue* rel
		,FklVMvalue* loop_obj
		,int* err)
{
	FklVMvalue* v=fklCreateVMvalueUd(vm,&HandleMetaTables[UV_TIMER],rel);
	FKL_DECL_VM_UD_DATA(hud,FuvHandleUd,v);
	FKL_DECL_VM_UD_DATA(loop,FuvLoop,loop_obj);
	struct FuvTimer* handle=CREATE_HANDLE(struct FuvTimer);
	*hud=(FuvHandle*)handle;
	initFuvHandle(v,(FuvHandle*)handle,loop_obj);
	*err=uv_timer_init(&loop->loop,&handle->handle);
	return v;
}

