#include"fuv.h"
#include"timer.h"

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
		.size=sizeof(FuvTimer),
		.__prin1=fuv_timer_print,
		.__princ=fuv_timer_print,
		.__atomic=fuv_timer_atomic,
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
	handle->data.loop=loop;
	handle->data.callbacks[0]=FKL_VM_NIL;
	handle->data.callbacks[1]=NULL;
	uv_handle_set_data(&handle->handle,&handle->data);
	fuvLoopInsertFuvHandle(loop,v);
}

FklVMvalue* createFuvTimer(FklVM* vm
		,FklVMvalue* rel
		,FklVMvalue* loop_obj
		,int* err)
{
	FklVMvalue* v=fklCreateVMvalueUd(vm,&HandleMetaTables[UV_TIMER],rel);
	FKL_DECL_VM_UD_DATA(h,FuvTimer,v);
	FKL_DECL_VM_UD_DATA(loop,FuvLoop,loop_obj);
	initFuvHandle(v,(FuvHandle*)h,loop_obj);
	*err=uv_timer_init(&loop->loop,&h->handle);
	return v;
}

