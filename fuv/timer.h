#include"fuv.h"

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_timer_print,timer);

static void fuv_timer_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(fuv_timer,FuvHandleUd,ud);
	FuvHandle* handle=fuv_timer->handle;
	if(handle)
	{
		fklVMgcToGray(handle->data.loop,gc);
		fklVMgcToGray(handle->data.callbacks[0],gc);
	}
}

