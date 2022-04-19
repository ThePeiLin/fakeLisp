#include<ffi.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include"deftype.h"
#include"ffi.h"
#define ARGL FklVM* exe,pthread_rwlock_t* gclock

void FKL_C_malloc(ARGL)
{
}

void FKL_C_free(ARGL)
{
}

void FKL_C_sizeof(ARGL)
{
}

void FKL_C_typedef(ARGL)
{
}

void FKL_C_call(ARGL)
{
}

void FKL_C_load(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* vpath=fklPopVMstack(stack);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("ffi.c.load",FKL_TOOMANYARG,r,exe);
	if(exe->VMid==-1)
		return;
	if(!vpath)
		FKL_RAISE_BUILTIN_ERROR("ffi.c.load",FKL_TOOFEWARG,r,exe);
	if(!FKL_IS_STR(vpath))
		FKL_RAISE_BUILTIN_ERROR("ffi.c.load",FKL_WRONGARG,r,exe);
	const char* path=vpath->u.str;
#ifdef _WIN32
	FklVMdllHandle handle=LoadLibrary(path);
	if(!handle)
	{
		TCHAR szBuf[128];
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage (
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				dw,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
				(LPTSTR) &lpMsgBuf,
				0, NULL );
		wsprintf(szBuf,
				_T("%s error Message (error code=%d): %s"),
				_T("CreateDirectory"), dw, lpMsgBuf);
		LocalFree(lpMsgBuf);
		fprintf(stderr,"%s\n",szBuf);
	}
#else
	FklVMdllHandle handle=dlopen(path,RTLD_LAZY);
	if(!handle)
	{
		perror(dlerror());
		putc('\n',stderr);
	}
#endif
	fklAddSharedObj(handle);
}


#undef ARGL
