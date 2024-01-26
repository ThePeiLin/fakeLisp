#include"fuv.h"

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_timer_print,timer);

static void fuv_timer_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(fuv_timer,FuvTimer,ud);
	fklVMgcToGray(fuv_timer->data.loop,gc);
	fklVMgcToGray(fuv_timer->data.callbacks[0],gc);
}

