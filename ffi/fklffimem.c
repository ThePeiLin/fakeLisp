#include"fklffimem.h"
#include<fakeLisp/utils.h>

static FklSid_t FfiMemUdTypeId=0;

static void _mem_finalizer(void* p)
{
	free(p);
}

static FklVMudMethodTable FfiMemMethodTable=
{
	NULL,
	NULL,
	_mem_finalizer,
};

void fklFfiMemInit(void)
{
	FfiMemUdTypeId=fklAddSymbolToGlob("ffi-mem")->id;
}

FklFfiMem* fklFfiNewMem(FklTypeId_t type,void* mem)
{
	FklFfiMem* r=(FklFfiMem*)malloc(sizeof(FklFfiMem));
	FKL_ASSERT(r,__func__);
	r->type=type;
	r->mem=mem;
	return r;
}

FklVMudata* fklFfiNewMemUd(FklTypeId_t type,void* mem)
{
	return fklNewVMudata(FfiMemUdTypeId,&FfiMemMethodTable,fklFfiNewMem(type,mem));
}

int fklFfiIsMem(FklVMvalue* p)
{
	return FKL_IS_USERDATA(p)&&p->u.p->type==FfiMemUdTypeId;
}
