#include<fakeLisp/vm.h>


#ifdef WIN32
#include<sys/stat.h>
#include<sys/types.h>
#define S_ISREG(m) ((S_IFMT&m)==S_IFREG)
#define S_ISDIR(m) ((S_IFMT&m)==S_IFDIR)
#endif

static void sleep_timer_close_cb(uv_handle_t* h)
{
	free(h);
}

static void sleep_timer_cb(uv_timer_t* handle)
{
	fklResumeThread(uv_handle_get_data((uv_handle_t*)handle));
	uv_timer_stop(handle);
	uv_close((uv_handle_t*)handle,sleep_timer_close_cb);
}

int fklVMsleep(FklVM* exe,uint64_t ms)
{
	fklSuspendThread(exe);
	uv_timer_t* timer=(uv_timer_t*)malloc(sizeof(uv_timer_t));
	FKL_ASSERT(timer);
	uv_timer_init(exe->loop,timer);
	uv_handle_set_data((uv_handle_t*)timer,exe);
	uv_update_time(exe->loop);
	return uv_timer_start(timer,sleep_timer_cb,ms,0);
}

FklVMasyncReadCtx* fklCreateVMasyncReadCtx(FklVM* exe
		,FILE* fp
		,FklStringBuffer* buf
		,uint64_t len
		,int d)
{
	FklVMasyncReadCtx* ctx=(FklVMasyncReadCtx*)malloc(sizeof(FklVMasyncReadCtx));
	FKL_ASSERT(ctx);
	uv_req_set_data((uv_req_t*)&ctx->req,exe);
	ctx->fp=fp;
	ctx->len=len;
	ctx->buf=buf;
	ctx->d=d;
	return ctx;
}

static void vm_async_read_cb(uv_work_t* req)
{
	FklVMasyncReadCtx* ctx=(FklVMasyncReadCtx*)req;
	FklStringBuffer* buf=ctx->buf;
	FILE* fp=ctx->fp;
	uint64_t len=ctx->len;
	if(ctx->d!=EOF)
		fklGetDelim(fp,buf,ctx->d);
	else
		buf->index=fread(buf->buf,sizeof(char),len,fp);
}

static void vm_async_after_read_cb(uv_work_t* req,int status)
{
	if(status==UV_ECANCELED)
		return;
	FklVM* exe=uv_req_get_data((uv_req_t*)req);
	fklResumeThread(exe);
}

int fklVMread(FklVM* exe,FklVMasyncReadCtx* ctx)
{
	fklSuspendThread(exe);
	uv_req_set_data((uv_req_t*)ctx,exe);
	return uv_queue_work(exe->loop,(uv_work_t*)ctx,vm_async_read_cb,vm_async_after_read_cb);
}

