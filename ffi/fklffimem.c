#include"fklffimem.h"
#include<fakeLisp/utils.h>

static FklSid_t FfiMemUdSid=0;
static FklSid_t FfiAtomicSid=0;
static FklSid_t FfiRawSid=0;

static void _mem_finalizer(void* p)
{
	free(p);
}

static void _mem_atomic_finalizer(void* p)
{
	FklFfiMem* m=p;
	free(m->mem);
	free(p);
}

static FklVMudMethodTable FfiMemMethodTable=
{
	._princ=NULL,
	._prin1=NULL,
	._finalizer=_mem_finalizer,
	._eq=NULL,
};

static FklVMudMethodTable FfiAtomicMemMethodTable=
{
	._princ=NULL,
	._prin1=NULL,
	._finalizer=_mem_atomic_finalizer,
	._eq=NULL,
};

void fklFfiMemInit(void)
{
	FfiMemUdSid=fklAddSymbolToGlob("ffi-mem")->id;
	FfiAtomicSid=fklAddSymbolToGlob("atomic")->id;
	FfiRawSid=fklAddSymbolToGlob("raw")->id;
}

FklFfiMem* fklFfiNewMem(FklTypeId_t type,size_t size)
{
	FklFfiMem* r=(FklFfiMem*)malloc(sizeof(FklFfiMem));
	FKL_ASSERT(r,__func__);
	r->type=type;
	void* p=malloc(size);
	FKL_ASSERT(p,__func__);
	r->mem=p;
	return r;
}

FklVMudata* fklFfiNewMemUd(FklTypeId_t type,size_t size,FklVMvalue* atomic)
{
	if(FKL_GET_SYM(atomic)==FfiAtomicSid)
		return fklNewVMudata(FfiMemUdSid,&FfiAtomicMemMethodTable,fklFfiNewMem(type,size));
	else if(atomic==NULL||FKL_GET_SYM(atomic)==FfiRawSid)
		return fklNewVMudata(FfiMemUdSid,&FfiMemMethodTable,fklFfiNewMem(type,size));
	else
		return NULL;
}

int fklFfiIsMem(FklVMvalue* p)
{
	return FKL_IS_USERDATA(p)&&p->u.p->type==FfiMemUdSid;
}
