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

struct VMfileCreateReq
{
	uv_fs_t req;
	const char* path;
	const char* caller;
	FklVMfpRW rw;
	unsigned int arg_num;
};

static void vm_create_file_cb(uv_fs_t* fs)
{
	struct VMfileCreateReq* req=(struct VMfileCreateReq*)fs;
	uv_file result=fs->result;
	FklVM* exe=uv_req_get_data((uv_req_t*)fs);
	const char* path=req->path;
	const char* caller=req->caller;
	FklVMfpRW rw=req->rw;
	unsigned int arg_num=req->arg_num;
	uv_fs_req_cleanup(fs);
	free(fs);
	fklResumeThread(exe);
	if(result<0)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR(caller,(char*)path,0,FKL_ERR_FILEFAILURE,exe);
	exe->tp-=arg_num;
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueFd(exe,result,rw));
}

int fklVMfopen(FklVM* exe
		,const char* path
		,const char* mode
		,const char* caller
		,unsigned int arg_num)
{
	int omode;
	int oflags=0;
	FklVMfpRW rw;
	switch(*mode)
	{
		case 'r':
			omode=UV_FS_O_RDONLY;
			rw=FKL_VM_FP_R;
			break;
		case 'w':
			omode=UV_FS_O_WRONLY;
			rw=FKL_VM_FP_W;
			break;
		case 'a':
			rw=FKL_VM_FP_W;
			omode=UV_FS_O_WRONLY;
			oflags=UV_FS_O_CREAT|UV_FS_O_APPEND;
			break;
		default:
			return 1;
	}
	for(int8_t i=1;i<7;++i)
	{
		switch(*++mode)
		{
			case '\0':
			case ',':
				break;
			case '+':
				rw=FKL_VM_FP_RW;
				omode=UV_FS_O_RDWR;
				continue;
				break;
			case 'x':
				oflags|=UV_FS_O_EXCL;
				continue;
				break;
			case 'b':
				continue;
				break;
			default:
				continue;
				break;
		}
		break;
	}

	fklSuspendThread(exe);
	struct VMfileCreateReq* req=(struct VMfileCreateReq*)malloc(sizeof(struct VMfileCreateReq));
	FKL_ASSERT(req);
	req->caller=caller;
	req->rw=rw;
	req->arg_num=arg_num;
	req->path=path;
	uv_req_set_data((uv_req_t*)req,exe);
	return uv_fs_open(exe->loop,(uv_fs_t*)req,path,omode,oflags,vm_create_file_cb);
}

struct VMfileCloseReq
{
	uv_fs_t req;
	const char* caller;
};

static void vm_close_file_cb(uv_fs_t* fs)
{
	struct VMfileCloseReq* req=(struct VMfileCloseReq*)fs;
	uv_file result=fs->result;
	FklVM* exe=uv_req_get_data((uv_req_t*)fs);
	const char* caller=req->caller;
	uv_fs_req_cleanup(fs);
	free(fs);
	fklResumeThread(exe);
	if(result<0)
		FKL_RAISE_BUILTIN_ERROR_CSTR(caller,FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
}

int fklVMfclose(FklVM* exe
		,FklVMvalue* vfp
		,const char* caller)
{
	FklVMfp* fp=FKL_VM_FP(vfp);
	fklUninitPtrQueue(&fp->next);
	struct VMfileCloseReq* req=(struct VMfileCloseReq*)malloc(sizeof(struct VMfileCloseReq));
	FKL_ASSERT(req);
	req->caller=caller;
	uv_file fd=fp->fd;
	fp->fd=-1;
	uv_req_set_data((uv_req_t*)req,exe);
	fklSuspendThread(exe);
	return uv_fs_close(exe->loop,(uv_fs_t*)req,fd,vm_close_file_cb);
}

static void vm_reg_file_cb(uv_fs_t* fs)
{
	FklVM* exe=uv_req_get_data((uv_req_t*)fs);
	int result=S_ISREG(fs->statbuf.st_mode);
	uv_fs_req_cleanup(fs);
	free(fs);
	fklResumeThread(exe);
	FKL_VM_PUSH_VALUE(exe,result?FKL_VM_TRUE:FKL_VM_NIL);
}

static void vm_access_file_cb(uv_fs_t* fs)
{
	uv_file result=fs->result;
	FklVM* exe=uv_req_get_data((uv_req_t*)fs);
	const char* path=fs->path;
	fs->path=NULL;
	uv_fs_req_cleanup(fs);
	if(result<0)
	{
		free(fs);
		fklResumeThread(exe);
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
		return;
	}
	uv_req_set_data((uv_req_t*)fs,exe);
	uv_fs_stat(exe->loop,fs,path,vm_reg_file_cb);
	free((void*)path);
}

int fklVMfacceReg(FklVM* exe
		,const char* path)
{
	uv_fs_t* req=(uv_fs_t*)malloc(sizeof(uv_fs_t));
	FKL_ASSERT(req);
	uv_req_set_data((uv_req_t*)req,exe);
	fklSuspendThread(exe);
	return uv_fs_access(exe->loop,req,path,FKL_UV_R_OK,vm_access_file_cb);
}

