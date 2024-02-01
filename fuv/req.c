#include"fuv.h"

static void fuv_req_ud_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(fuv_req,FuvReqUd,ud);
	FuvReq* req=*fuv_req;
	if(req)
	{
		fklVMgcToGray(req->data.loop,gc);
		fklVMgcToGray(req->data.callback,gc);
		fklVMgcToGray(req->data.write_data,gc);
	}
}

static void fuv_req_ud_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(req_ud,FuvReqUd,ud);
	FuvReq* fuv_req=*req_ud;
	if(fuv_req)
	{
		FuvReqData* req_data=&fuv_req->data;
		FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,req_data->loop);
		fklDelHashItem(&req_data->req,&fuv_loop->data.gc_values,NULL);
		fuv_req->data.req=NULL;
		*req_ud=NULL;
	}
}

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_getaddrinfo_print,getaddrinfo);

static const FklVMudMetaTable ReqMetaTables[UV_REQ_TYPE_MAX]=
{
	// UV_UNKNOWN_REQ
	{
	},

	// UV_REQ
	{
	},

	// UV_CONNECT
	{
	},

	// UV_WRITE
	{
	},

	// UV_SHUTDOWN
	{
	},

	// UV_UDP_SEND
	{
	},

	// UV_FS
	{
	},

	// UV_WORK
	{
	},

	// UV_GETADDRINFO
	{
		.size=sizeof(FuvReqUd),
		.__prin1=fuv_getaddrinfo_print,
		.__princ=fuv_getaddrinfo_print,
		.__atomic=fuv_req_ud_atomic,
		.__finalizer=fuv_req_ud_finalizer,
	},

	// UV_GETNAMEINFO
	{
	},

	// UV_RANDOM
	{
	},
};

int isFuvReq(FklVMvalue* v)
{
	if(FKL_IS_USERDATA(v))
	{
		const FklVMudMetaTable* t=FKL_VM_UD(v)->t;
		return t>&ReqMetaTables[UV_UNKNOWN_REQ]
			&&t<&ReqMetaTables[UV_REQ_TYPE_MAX];
	}
	return 0;
}

