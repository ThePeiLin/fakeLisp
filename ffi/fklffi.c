#include<fakeLisp/utils.h>
#include<fakeLisp/fklni.h>
#include"fklffi.h"
#include"ffidll.h"
#define ARGL FklVM* exe,pthread_rwlock_t* gclock

void FKL_ffi_malloc(ARGL)
{
}

void FKL_ffi_free(ARGL)
{
}

void FKL_ffi_sizeof(ARGL)
{
}

void FKL_ffi_typedef(ARGL)
{
}

void FKL_ffi_call(ARGL)
{
}

void FKL_ffi_load(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* vpath=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("ffi.load",FKL_TOOMANYARG,r,exe);
	if(exe->VMid==-1)
		return;
	if(!vpath)
		FKL_RAISE_BUILTIN_ERROR("ffi.load",FKL_TOOFEWARG,r,exe);
	if(!FKL_IS_STR(vpath))
		FKL_RAISE_BUILTIN_ERROR("ffi.load",FKL_WRONGARG,r,exe);
	char* path=fklCharBufToStr(vpath->u.str->str,vpath->u.str->size);
	FklVMdllHandle handle=fklLoadDll(path);
	if(!handle)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR("ffi.load",path,1,FKL_LOADDLLFAILD,r,exe);
	free(path);
	fklAddSharedObj(handle);
	fklNiReturn(vpath,&ap,stack);
	fklNiEnd(&ap,stack);
}

void FKL_ffi_ref(ARGL)
{
}

void FKL_ffi_set(ARGL)
{
}
#undef ARGL
