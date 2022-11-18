#ifndef FKL_FFI_DLL_H
#define FKL_FFI_DLL_H
#include"fklffitype.h"

#ifdef __cplusplus
extern "C" {
#endif

#include<ffi.h>
#ifdef _WIN32
#include<windows.h>
typedef HMODULE FklFfidllHandle;
#else
#include<dlfcn.h>
typedef void* FklFfidllHandle;
#endif

typedef struct FklFfiProc
{
	void* func;
	ffi_cif cif;
	ffi_type** atypes;
	FklTypeId_t type;
	FklSid_t sid;
}FklFfiProc;

int fklFfiIsProc(FklVMvalue*);
void fklFfiDestroyAllSharedObj(void);
void fklFfiAddSharedObj(FklFfidllHandle handle);
FklVMudata* fklFfiCreateProcUd(FklTypeId_t id,const char*,FklVMvalue*);
FklFfiProc* fklFfiCreateProc(FklTypeId_t type,void* func,FklSid_t);
int fklFfiIsValidFunctionTypeId(FklTypeId_t type);

#ifdef __cplusplus
}
#endif
#endif
