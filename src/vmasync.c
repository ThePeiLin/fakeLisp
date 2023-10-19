#include<fakeLisp/vm.h>
#include<unistd.h>

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

int fklThreadSleep(FklVM* exe,uint64_t ms)
{
	fklSuspendThread(exe);
	uv_timer_t* timer=(uv_timer_t*)malloc(sizeof(uv_timer_t));
	FKL_ASSERT(timer);
	uv_timer_init(exe->loop,timer);
	uv_handle_set_data((uv_handle_t*)timer,exe);
	uv_update_time(exe->loop);
	return uv_timer_start(timer,sleep_timer_cb,ms,0);
}
