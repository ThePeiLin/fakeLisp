#include<fakeLisp/utils.h>
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
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* vpath=fklPopVMstack(stack);
	if(fklResBp(stack))
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
	FKL_SET_RETURN(__func__,vpath,stack);
}

void FKL_ffi_ref(ARGL)
{
}

void FKL_ffi_set(ARGL)
{
}
#undef ARGL
