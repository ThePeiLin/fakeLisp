#ifndef FKL_FFI_DLL_H
#define FKL_FFI_DLL_H
#include"fklffitype.h"
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

void fklFfiFreeAllSharedObj(void);
void fklFfiAddSharedObj(FklFfidllHandle handle);
FklVMudata* fklFfiNewProcUd(FklTypeId_t id,const char* );
FklFfiProc* fklFfiNewProc(FklTypeId_t type,void* func,FklSid_t);
int fklFfiIsValidFunctionTypeId(FklTypeId_t type);
#endif
