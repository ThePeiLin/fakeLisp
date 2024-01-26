#ifndef FKL_FUV_FUV_H
#define FKL_FUV_FUV_H

#include<fakeLisp/vm.h>
#include<uv.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	FUV_EBUSY
}FuvErrorType;

typedef struct
{
	uv_loop_t loop;
	int mode;
	FklHashTable gc_values;
}FuvLoop;

typedef struct
{
	FklVMvalue* handle;
	FklVMvalue* loop;
	FklVMvalue* callbacks[2];
}FuvHandleData;

typedef struct
{
	FuvHandleData data;
	uv_handle_t handle;
}FuvHandle;

typedef struct
{
	FuvHandleData data;
	uv_timer_t handle;
}FuvTimer;

int isFuvLoop(FklVMvalue* v);
FklVMvalue* createFuvLoop(FklVM*,FklVMvalue* rel);

int isFuvTimer(FklVMvalue* v);
FklVMvalue* createFuvTimer(FklVM*,FklVMvalue* rel,FklVMvalue* loop);

#define RAISE_FUV_ERROR(WHO,TYPE,EXE) abort()

#define CHECK_UV_RESULT(R,WHO,EXE) if((R)<0)RAISE_FUV_ERROR(WHO,R,EXE)
#ifdef __cplusplus
}
#endif

#endif
