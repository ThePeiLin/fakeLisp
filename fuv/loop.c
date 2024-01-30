#include"fuv.h"
#include "uv.h"


FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_loop_print,loop);

static void fuv_loop_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(fuv_loop,FuvLoop,ud);
	for(FklHashTableItem* l=fuv_loop->data.gc_values.first
			;l
			;l=l->next)
	{
		FklVMvalue* v=*((void**)l->data);
		fklVMgcToGray(v,gc);
	}
}

static void fuv_close_loop_handle_cb(uv_handle_t* handle)
{
	FuvHandle* fuv_handle=uv_handle_get_data(handle);
	FuvHandleData* hdata=&fuv_handle->data;
	FklVMvalue* handle_obj=hdata->handle;
	free(fuv_handle);
	if(handle_obj)
	{
		FKL_DECL_VM_UD_DATA(fuv_handle_ud,FuvHandleUd,handle_obj);
		*fuv_handle_ud=NULL;
	}
}

static void fuv_close_loop_walk_cb(uv_handle_t* handle,void* arg)
{
	if(uv_is_closing(handle))
	{
		FuvHandle* fh=uv_handle_get_data(handle);
		fh->data.callbacks[1]=NULL;
	}
	else
		uv_close(handle,fuv_close_loop_handle_cb);
}

static void fuv_loop_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(fuv_loop,FuvLoop,ud);
	uv_walk(&fuv_loop->loop,fuv_close_loop_walk_cb,NULL);
	while(uv_loop_close(&fuv_loop->loop))
		uv_run(&fuv_loop->loop,UV_RUN_DEFAULT);
	fklUninitHashTable(&fuv_loop->data.gc_values);
}

static FklVMudMetaTable FuvLoopMetaTable=
{
	.size=sizeof(FuvLoop),
	.__prin1=fuv_loop_print,
	.__princ=fuv_loop_print,
	.__atomic=fuv_loop_atomic,
	.__finalizer=fuv_loop_finalizer,
};

FklVMvalue* createFuvLoop(FklVM* vm,FklVMvalue* rel)
{
	FklVMvalue* v=fklCreateVMvalueUd(vm,&FuvLoopMetaTable,rel);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,v);
	fuv_loop->data.mode=-1;
	uv_loop_init(&fuv_loop->loop);
	fklInitPtrSet(&fuv_loop->data.gc_values);
	uv_loop_set_data(&fuv_loop->loop,&fuv_loop->data);
	return v;
}

int isFuvLoop(FklVMvalue* v)
{
	return FKL_IS_USERDATA(v)&&FKL_VM_UD(v)->t==&FuvLoopMetaTable;
}

void fuvLoopInsertFuvHandle(FklVMvalue* loop_obj,FklVMvalue* h)
{
	FKL_DECL_VM_UD_DATA(loop,FuvLoop,loop_obj);
	fklPutHashItem(&h,&loop->data.gc_values);
}

