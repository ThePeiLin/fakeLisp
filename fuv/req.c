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

void uninitFuvReq(FuvReqUd* req_ud)
{
	FuvReq* fuv_req=*req_ud;
	if(fuv_req)
	{
		FuvReqData* req_data=&fuv_req->data;
		FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,req_data->loop);
		fklDelHashItem(&req_data->req,&fuv_loop->data.gc_values,NULL);
		fuv_req->data.req=NULL;
		fuv_req->data.callback=NULL;
		*req_ud=NULL;
	}
}

void uninitFuvReqValue(FklVMvalue* v)
{
	FKL_DECL_VM_UD_DATA(req_ud,FuvReqUd,v);
	uninitFuvReq(req_ud);
}

static void fuv_req_ud_finalizer(FklVMud* ud)
{
	FKL_DECL_UD_DATA(req_ud,FuvReqUd,ud);
	uninitFuvReq(req_ud);
}

FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_getaddrinfo_print,getaddrinfo);
FKL_VM_USER_DATA_DEFAULT_PRINT(fuv_getnameinfo_print,getnameinfo);

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
		.size=sizeof(FuvReqUd),
		.__prin1=fuv_getnameinfo_print,
		.__princ=fuv_getnameinfo_print,
		.__atomic=fuv_req_ud_atomic,
		.__finalizer=fuv_req_ud_finalizer,
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

static inline void init_fuv_req(FuvReqUd* r_ud
		,FuvReq* req
		,FklVMvalue* v
		,FklVMvalue* loop
		,FklVMvalue* callback)
{
	*r_ud=req;
	req->data.req=v;
	req->data.loop=loop;
	req->data.callback=callback;
	uv_req_set_data(&req->req,req);
	fuvLoopInsertFuvObj(loop,v);
}

#define CREATE_OBJ(TYPE) (TYPE*)malloc(sizeof(TYPE))

#define FUV_REQ_P(NAME,ENUM) int NAME(FklVMvalue* v)\
{\
	return FKL_IS_USERDATA(v)\
		&&FKL_VM_UD(v)->t==&ReqMetaTables[ENUM];\
}

FUV_REQ_P(isFuvGetaddrinfo,UV_GETADDRINFO);

struct FuvGetaddrinfo
{
	FuvReqData data;
	uv_getaddrinfo_t req;
};

uv_getaddrinfo_t* createFuvGetaddrinfo(FklVM* exe
		,FklVMvalue** ret
		,FklVMvalue* rel
		,FklVMvalue* loop
		,FklVMvalue* callback)
{
	FklVMvalue* v=fklCreateVMvalueUd(exe,&ReqMetaTables[UV_GETADDRINFO],rel);
	FKL_DECL_VM_UD_DATA(fuv_req,FuvReqUd,v);
	struct FuvGetaddrinfo* req=CREATE_OBJ(struct FuvGetaddrinfo);
	FKL_ASSERT(req);
	init_fuv_req(fuv_req,(FuvReq*)req,v,loop,callback);
	*ret=v;
	return &req->req;
}

