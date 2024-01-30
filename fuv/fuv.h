#ifndef FKL_FUV_FUV_H
#define FKL_FUV_FUV_H

#include<fakeLisp/vm.h>
#include<uv.h>
#include<signal.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef enum
{
	FUV_ERR_DUMMY=0,
	FUV_ERR_CLOSE_CLOSEING_HANDLE,
	FUV_ERR_HANDLE_CLOSED,
	FUV_ERR_NUM,
}FuvErrorType;

typedef struct
{
	FklSid_t loop_mode[3];
	FklSid_t fuv_err_sid[FUV_ERR_NUM];
	FklSid_t loop_block_signal_sid;
	FklSid_t metrics_idle_time_sid;
#define XX(code,_) FklSid_t uv_err_sid_##code;
	UV_ERRNO_MAP(XX);
#undef XX

#ifdef SIGHUP
	FklSid_t SIGHUP_sid;
#endif
#ifdef SIGINT
	FklSid_t SIGINT_sid;
#endif
#ifdef SIGQUIT
	FklSid_t SIGQUIT_sid;
#endif
#ifdef SIGILL
	FklSid_t SIGILL_sid;
#endif
#ifdef SIGTRAP
	FklSid_t SIGTRAP_sid;
#endif
#ifdef SIGABRT
	FklSid_t SIGABRT_sid;
#endif
#ifdef SIGIOT
	FklSid_t SIGIOT_sid;
#endif
#ifdef SIGBUS
	FklSid_t SIGBUS_sid;
#endif
#ifdef SIGFPE
	FklSid_t SIGFPE_sid;
#endif
#ifdef SIGKILL
	FklSid_t SIGKILL_sid;
#endif
#ifdef SIGUSR1
	FklSid_t SIGUSR1_sid;
#endif
#ifdef SIGSEGV
	FklSid_t SIGSEGV_sid;
#endif
#ifdef SIGUSR2
	FklSid_t SIGUSR2_sid;
#endif
#ifdef SIGPIPE
	FklSid_t SIGPIPE_sid;
#endif
#ifdef SIGALRM
	FklSid_t SIGALRM_sid;
#endif
#ifdef SIGTERM
	FklSid_t SIGTERM_sid;
#endif
#ifdef SIGCHLD
	FklSid_t SIGCHLD_sid;
#endif
#ifdef SIGSTKFLT
	FklSid_t SIGSTKFLT_sid;
#endif
#ifdef SIGCONT
	FklSid_t SIGCONT_sid;
#endif
#ifdef SIGSTOP
	FklSid_t SIGSTOP_sid;
#endif
#ifdef SIGTSTP
	FklSid_t SIGTSTP_sid;
#endif
#ifdef SIGBREAK
	FklSid_t SIGBREAK_sid;
#endif
#ifdef SIGTTIN
	FklSid_t SIGTTIN_sid;
#endif
#ifdef SIGTTOU
	FklSid_t SIGTTOU_sid;
#endif
#ifdef SIGURG
	FklSid_t SIGURG_sid;
#endif
#ifdef SIGXCPU
	FklSid_t SIGXCPU_sid;
#endif
#ifdef SIGXFSZ
	FklSid_t SIGXFSZ_sid;
#endif
#ifdef SIGVTALRM
	FklSid_t SIGVTALRM_sid;
#endif
#ifdef SIGPROF
	FklSid_t SIGPROF_sid;
#endif
#ifdef SIGWINCH
	FklSid_t SIGWINCH_sid;
#endif
#ifdef SIGIO
	FklSid_t SIGIO_sid;
#endif
#ifdef SIGPOLL
	FklSid_t SIGPOLL_sid;
#endif
#ifdef SIGLOST
	FklSid_t SIGLOST_sid;
#endif
#ifdef SIGPWR
	FklSid_t SIGPWR_sid;
#endif
#ifdef SIGSYS
	FklSid_t SIGSYS_sid;
#endif

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

typedef struct FuvHandle
{
	FuvHandleData data;
	uv_handle_t handle;
}FuvHandle;

typedef FuvHandle* FuvHandleUd;

int isFuvLoop(FklVMvalue* v);
FklVMvalue* createFuvLoop(FklVM*,FklVMvalue* rel);
void fuvLoopInsertFuvHandle(FklVMvalue* loop,FklVMvalue* handle);

int isFuvHandle(FklVMvalue* v);
void initFuvHandle(FklVMvalue* v,FuvHandle* h,FklVMvalue* loop);

int isFuvTimer(FklVMvalue* v);
FklVMvalue* createFuvTimer(FklVM*,FklVMvalue* rel,FklVMvalue* loop,int* err);

int isFuvPrepare(FklVMvalue* v);
FklVMvalue* createFuvPrepare(FklVM*,FklVMvalue* rel,FklVMvalue* loop,int* err);

int isFuvIdle(FklVMvalue* v);
FklVMvalue* createFuvIdle(FklVM*,FklVMvalue* rel,FklVMvalue* loop,int* err);

int isFuvCheck(FklVMvalue* v);
FklVMvalue* createFuvCheck(FklVM*,FklVMvalue* rel,FklVMvalue* loop,int* err);

int isFuvSignal(FklVMvalue* v);
FklVMvalue* createFuvSignal(FklVM*,FklVMvalue* rel,FklVMvalue* loop,int* err);

void raiseUvError(const char* who,int err,FklVM* exe,FklVMvalue* pd);
void raiseFuvError(const char* who,FuvErrorType,FklVM* exe,FklVMvalue* pd);
FklVMvalue* createUvError(const char* who,int err_id,FklVM* exe,FklVMvalue* pd);

int symbolToSignum(FklSid_t,FuvPublicData* pd);
FklSid_t signumToSymbol(int,FuvPublicData* pd);

#define CHECK_HANDLE_CLOSED(H,WHO,EXE,PD) if((H)==NULL)raiseFuvError((WHO),FUV_ERR_HANDLE_CLOSED,(EXE),(PD))
#define CHECK_UV_RESULT(R,WHO,EXE,PD) if((R)<0)raiseUvError((WHO),(R),(EXE),(PD))
#define GET_HANDLE(FUV_HANDLE) &((FUV_HANDLE)->handle)

#ifdef __cplusplus
}
#endif

#endif
