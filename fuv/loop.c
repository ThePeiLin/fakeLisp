#include"fuv.h"


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

static void fuv_loop_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(fuv_loop,FuvLoop,ud);
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

