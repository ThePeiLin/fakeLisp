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

FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_getaddrinfo_as_print,getaddrinfo);
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_getnameinfo_as_print,getnameinfo);
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_write_as_print,write);
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_shutdown_as_print,shutdown);
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_connect_as_print,connect);
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_udp_send_as_print,udp-send);
FKL_VM_USER_DATA_DEFAULT_AS_PRINT(fuv_fs_as_print,fs);

static void fuv_write_ud_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(fuv_req,FuvReqUd,ud);
	struct FuvWrite* req=(struct FuvWrite*)*fuv_req;
	if(req)
	{
		fklVMgcToGray(req->data.loop,gc);
		fklVMgcToGray(req->data.callback,gc);
		fklVMgcToGray(req->data.write_data,gc);
		for(FklVMvalue** cur=req->write_objs;*cur;cur++)
			fklVMgcToGray(*cur,gc);
	}
}

static void fuv_udp_send_ud_atomic(const FklVMud* ud,FklVMgc* gc)
{
	FKL_DECL_UD_DATA(fuv_req,FuvReqUd,ud);
	struct FuvUdpSend* req=(struct FuvUdpSend*)*fuv_req;
	if(req)
	{
		fklVMgcToGray(req->data.loop,gc);
		fklVMgcToGray(req->data.callback,gc);
		fklVMgcToGray(req->data.write_data,gc);
		for(FklVMvalue** cur=req->send_objs;*cur;cur++)
			fklVMgcToGray(*cur,gc);
	}
}

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
		.size=sizeof(FuvReqUd),
		.__as_prin1=fuv_connect_as_print,
		.__as_princ=fuv_connect_as_print,
		.__atomic=fuv_req_ud_atomic,
		.__finalizer=fuv_req_ud_finalizer,
	},

	// UV_WRITE
	{
		.size=sizeof(FuvReqUd),
		.__as_prin1=fuv_write_as_print,
		.__as_princ=fuv_write_as_print,
		.__atomic=fuv_write_ud_atomic,
		.__finalizer=fuv_req_ud_finalizer,
	},

	// UV_SHUTDOWN
	{
		.size=sizeof(FuvReqUd),
		.__as_prin1=fuv_shutdown_as_print,
		.__as_princ=fuv_shutdown_as_print,
		.__atomic=fuv_req_ud_atomic,
		.__finalizer=fuv_req_ud_finalizer,
	},

	// UV_UDP_SEND
	{
		.size=sizeof(FuvReqUd),
		.__as_prin1=fuv_udp_send_as_print,
		.__as_princ=fuv_udp_send_as_print,
		.__atomic=fuv_udp_send_ud_atomic,
		.__finalizer=fuv_req_ud_finalizer,
	},

	// UV_FS
	{
		.size=sizeof(FuvReqUd),
		.__as_prin1=fuv_fs_as_print,
		.__as_princ=fuv_fs_as_print,
		.__atomic=fuv_req_ud_atomic,
		.__finalizer=fuv_req_ud_finalizer,
	},

	// UV_WORK
	{
	},

	// UV_GETADDRINFO
	{
		.size=sizeof(FuvReqUd),
		.__as_prin1=fuv_getaddrinfo_as_print,
		.__as_princ=fuv_getaddrinfo_as_print,
		.__atomic=fuv_req_ud_atomic,
		.__finalizer=fuv_req_ud_finalizer,
	},

	// UV_GETNAMEINFO
	{
		.size=sizeof(FuvReqUd),
		.__as_prin1=fuv_getnameinfo_as_print,
		.__as_princ=fuv_getnameinfo_as_print,
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

struct FuvGetaddrinfo
{
	FuvReqData data;
	uv_getaddrinfo_t req;
};

FUV_REQ_P(isFuvGetaddrinfo,UV_GETADDRINFO);

#define NORMAL_REQ_CREATOR(TYPE,NAME,ENUM) uv_##NAME##_t* create##TYPE(FklVM* vm\
		,FklVMvalue** ret\
		,FklVMvalue* rel\
		,FklVMvalue* loop\
		,FklVMvalue* callback)\
{\
	FklVMvalue* v=fklCreateVMvalueUd(vm,&ReqMetaTables[ENUM],rel);\
	FKL_DECL_VM_UD_DATA(fuv_req,FuvReqUd,v);\
	struct TYPE* req=CREATE_OBJ(struct TYPE);\
	FKL_ASSERT(req);\
	init_fuv_req(fuv_req,(FuvReq*)req,v,loop,callback);\
	*ret=v;\
	return &req->req;\
}

NORMAL_REQ_CREATOR(FuvGetaddrinfo,getaddrinfo,UV_GETADDRINFO);

struct FuvGetnameinfo
{
	FuvReqData data;
	uv_getnameinfo_t req;
};

FUV_REQ_P(isFuvGetnameinfo,UV_GETNAMEINFO);

NORMAL_REQ_CREATOR(FuvGetnameinfo,getnameinfo,UV_GETNAMEINFO);

FUV_REQ_P(isFuvWrite,UV_WRITE);

uv_write_t* createFuvWrite(FklVM* exe
		,FklVMvalue** ret
		,FklVMvalue* rel
		,FklVMvalue* loop
		,FklVMvalue* callback
		,uint32_t count)
{
	FklVMvalue* v=fklCreateVMvalueUd(exe,&ReqMetaTables[UV_WRITE],rel);
	FKL_DECL_VM_UD_DATA(fuv_req,FuvReqUd,v);
	struct FuvWrite* req=(struct FuvWrite*)calloc(1,sizeof(struct FuvWrite)+sizeof(FklVMvalue*)*count);
	FKL_ASSERT(req);
	init_fuv_req(fuv_req,(FuvReq*)req,v,loop,callback);
	*ret=v;
	return &req->req;
}

struct FuvShutdown
{
	FuvReqData data;
	uv_shutdown_t req;
};

FUV_REQ_P(isFuvShutdown,UV_SHUTDOWN);

NORMAL_REQ_CREATOR(FuvShutdown,shutdown,UV_SHUTDOWN);

struct FuvConnect
{
	FuvReqData data;
	uv_connect_t req;
};

FUV_REQ_P(isFuvConnect,UV_CONNECT);

NORMAL_REQ_CREATOR(FuvConnect,connect,UV_CONNECT);

FUV_REQ_P(isFuvUdpSend,UV_UDP_SEND);

uv_udp_send_t* createFuvUdpSend(FklVM* exe
		,FklVMvalue** ret
		,FklVMvalue* rel
		,FklVMvalue* loop
		,FklVMvalue* callback
		,uint32_t count)
{
	FklVMvalue* v=fklCreateVMvalueUd(exe,&ReqMetaTables[UV_UDP_SEND],rel);
	FKL_DECL_VM_UD_DATA(fuv_req,FuvReqUd,v);
	struct FuvUdpSend* req=(struct FuvUdpSend*)calloc(1,sizeof(struct FuvUdpSend)+sizeof(FklVMvalue*)*count);
	FKL_ASSERT(req);
	init_fuv_req(fuv_req,(FuvReq*)req,v,loop,callback);
	*ret=v;
	return &req->req;
}

struct FuvFs
{
	FuvReqData data;
	uv_fs_t req;
};

FUV_REQ_P(isFuvFs,UV_FS);

NORMAL_REQ_CREATOR(FuvFs,fs,UV_FS);
