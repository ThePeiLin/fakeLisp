#include"fuv.h"

static inline FklVMvalue* create_uv_error(const char* who
		,int err_id
		,FklVM* exe
		,FklVMvalue* pd_obj)
{
	FKL_DECL_VM_UD_DATA(pd,FuvPublicData,pd_obj);
	FklSid_t id=0;
	switch(err_id)
	{
#define XX(code,_) case UV_##code: id=pd->uv_err_sid_##code;break;
		UV_ERRNO_MAP(XX);
		default:
		id=pd->uv_err_sid_UNKNOWN;
	}
	return fklCreateVMvalueErrorWithCstr(exe
			,id
			,who
			,fklCreateStringFromCstr(uv_strerror(err_id)));
}

void raiseUvError(const char* who
		,int err_id
		,FklVM* exe
		,FklVMvalue* pd_obj)
{
	fklRaiseVMerror(create_uv_error(who,err_id,exe,pd_obj),exe);
}

FklVMvalue* createUvError(const char* who
		,int err_id
		,FklVM* exe
		,FklVMvalue* pd_obj)
{
	return create_uv_error(who,err_id,exe,pd_obj);
}

void raiseFuvError(const char* who,FuvErrorType err,FklVM* exe,FklVMvalue* pd_obj)
{
	FKL_DECL_VM_UD_DATA(pd,FuvPublicData,pd_obj);
	FklSid_t sid=pd->fuv_err_sid[err];
	static const char* fuv_err_msg[]=
	{
		NULL,
		"close closing handle",
	};
	FklVMvalue* ev=fklCreateVMvalueErrorWithCstr(exe
			,sid
			,who
			,fklCreateStringFromCstr(fuv_err_msg[err]));
	fklRaiseVMerror(ev,exe);
}

