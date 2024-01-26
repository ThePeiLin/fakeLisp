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
	FklVM* exe;
	FklHashTable gc_values;
	jmp_buf buf;
	int mode;
}FuvLoopData;

typedef struct
{
	uv_loop_t loop;
	FuvLoopData data;
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
void fuvLoopInsertFuvHandle(FklVMvalue* loop,FklVMvalue* handle);

int isFuvHandle(FklVMvalue* v);
int isFuvTimer(FklVMvalue* v);
FklVMvalue* createFuvTimer(FklVM*,FklVMvalue* rel,FklVMvalue* loop,int* err);
void initFuvHandle(FklVMvalue* v,FuvHandle* h,FklVMvalue* loop);

#define RAISE_FUV_ERROR(WHO,TYPE,EXE) abort()

#define CHECK_UV_RESULT(R,WHO,EXE) if((R)<0)RAISE_FUV_ERROR(WHO,R,EXE)
#ifdef __cplusplus
}
#endif

#endif
