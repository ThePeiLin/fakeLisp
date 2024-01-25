#include"fuv.h"


FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_loop_print,loop);

static void fuv_loop_atomic(const FklVMud* ud,FklVMgc* gc)
{
	abort();
}

static void fuv_loop_finalizer(FklVMud* ud)
{
	abort();
}

static FklVMudMetaTable FuvLoopMetaTable=
{
	.size=sizeof(FuvLoop),
	.__prin1=fuv_loop_print,
	.__princ=fuv_loop_print,
	.__atomic=fuv_loop_atomic,
	.__finalizer=fuv_loop_finalizer,
};

