#ifndef FKL_FUV_FUV_H
#define FKL_FUV_FUV_H

#include<fakeLisp/vm.h>
#include<uv.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	FUV_ERR_DUMMY=0,
	FUV_ERR_CLOSE_CLOSEING_HANDLE,
	FUV_ERR_NUM,
}FuvErrorType;

typedef struct
{
	FklSid_t loop_mode[3];
	FklSid_t fuv_err_sid[FUV_ERR_NUM];
#define XX(code,_) FklSid_t uv_err_sid_##code;
	UV_ERRNO_MAP(XX);
#undef XX
}FuvPublicData;

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

void raiseUvError(const char* who,int err,FklVM* exe,FklVMvalue* pd);
void raiseFuvError(const char* who,FuvErrorType,FklVM* exe,FklVMvalue* pd);
FklVMvalue* createUvError(const char* who,int err_id,FklVM* exe,FklVMvalue* pd);

#define CHECK_UV_RESULT(R,WHO,EXE,PD) if((R)<0)raiseUvError(WHO,R,EXE,PD)

#ifdef __cplusplus
}
#endif

#endif
