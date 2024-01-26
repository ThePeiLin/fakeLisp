#include"fuv.h"
#include "fakeLisp/vm.h"
#include "uv.h"

#define PREDICATE(condition,err_infor) FKL_DECL_AND_CHECK_ARG(val,exe,err_infor);\
	FKL_CHECK_REST_ARG(exe,err_infor);\
	FKL_VM_PUSH_VALUE(exe,(condition)\
			?FKL_VM_TRUE\
			:FKL_VM_NIL);\
	return 0;

typedef struct
{
	FklSid_t loop_mode[3];
}FuvPublicData;

static FklVMudMetaTable FuvPublicDataMetaTable=
{
	.size=sizeof(FuvPublicData),
};

static inline void init_fuv_public_data(FuvPublicData* pd,FklVM* exe)
{
	static const char* loop_mode[]=
	{
		"default",
		"once",
		"nowait",
	};
	pd->loop_mode[UV_RUN_DEFAULT]=fklVMaddSymbolCstr(exe->gc,loop_mode[UV_RUN_DEFAULT])->id;
	pd->loop_mode[UV_RUN_ONCE]=fklVMaddSymbolCstr(exe->gc,loop_mode[UV_RUN_ONCE])->id;
	pd->loop_mode[UV_RUN_NOWAIT]=fklVMaddSymbolCstr(exe->gc,loop_mode[UV_RUN_NOWAIT])->id;
}

static int fuv_loop_p(FKL_CPROC_ARGL){PREDICATE(isFuvLoop(val),"fuv.loop?")}

static int fuv_make_loop(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.make-loop";
	FKL_CHECK_REST_ARG(exe,Pname);
	FklVMvalue* r=createFuvLoop(exe,ctx->proc);
	FKL_VM_PUSH_VALUE(exe,r);
	return 0;
}

static int fuv_loop_close1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-close1";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_loop_close(&fuv_loop->loop);
	CHECK_UV_RESULT(r,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fuv_loop_run1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-run1";
	abort();
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_run(&fuv_loop->loop,UV_RUN_DEFAULT);
	CHECK_UV_RESULT(r,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	return 0;
}

static int fuv_loop_alive_p(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-alive?";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_loop_alive(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,r?FKL_VM_TRUE:FKL_VM_NIL);
	return 0;
}

static int fuv_loop_mode(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-mode";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=fuv_loop->mode;
	if(r==-1)
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	else
	{
		FKL_DECL_VM_UD_DATA(fpd,FuvPublicData,FKL_VM_CPROC(ctx->proc)->pd);
		FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_SYM(fpd->loop_mode[r]));
	}
	return 0;
}

static int fuv_loop_backend_fd(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-backend-fd";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_backend_fd(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,r==-1?FKL_VM_NIL:FKL_MAKE_VM_FIX(r));
	return 0;
}

static int fuv_loop_backend_timeout(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-backend-timeout";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	int r=uv_backend_timeout(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,r==-1?FKL_VM_NIL:FKL_MAKE_VM_FIX(r));
	return 0;
}

static int fuv_loop_now(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-now";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	uint64_t r=uv_now(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(r,exe));
	return 0;
}

static int fuv_loop_update_time1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.loop-update-time!";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(fuv_loop,FuvLoop,loop_obj);
	uv_update_time(&fuv_loop->loop);
	FKL_VM_PUSH_VALUE(exe,loop_obj);
	return 0;
}

static int fuv_timer_p(FKL_CPROC_ARGL){PREDICATE(isFuvTimer(val),"fuv.timer?")}

static int fuv_make_timer(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.make-timer";
	FKL_DECL_AND_CHECK_ARG(loop_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(loop_obj,isFuvLoop,Pname,exe);
	FKL_DECL_VM_UD_DATA(loop,FuvLoop,loop_obj);
	FklVMvalue* timer_obj=createFuvTimer(exe,ctx->proc,loop_obj);
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	int r=uv_timer_init(&loop->loop,&timer->handle);
	CHECK_UV_RESULT(r,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static void fuv_timer_cb(uv_timer_t* handle)
{
	abort();
}

static int fuv_timer_start1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-start!";
	FKL_DECL_AND_CHECK_ARG3(timer_obj,timer_cb,timeout_obj,exe,Pname);
	FklVMvalue* repeat_obj=FKL_VM_POP_ARG(exe);
	if(repeat_obj==NULL)
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_TOOFEWARG,exe);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_CHECK_TYPE(timer_cb,fklIsCallable,Pname,exe);
	FKL_CHECK_TYPE(timeout_obj,fklIsVMint,Pname,exe);
	FKL_CHECK_TYPE(repeat_obj,fklIsVMint,Pname,exe);
	if(fklIsVMnumberLt0(timeout_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	if(fklIsVMnumberLt0(repeat_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	uint64_t timeout=fklGetUint(timeout_obj);
	uint64_t repeat=fklGetUint(repeat_obj);
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	int r=uv_timer_start(&timer->handle,fuv_timer_cb,timeout,repeat);
	CHECK_UV_RESULT(r,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_stop1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-stop!";
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	int r=uv_timer_stop(&timer->handle);
	CHECK_UV_RESULT(r,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_again1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-again!";
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	int r=uv_timer_again(&timer->handle);
	CHECK_UV_RESULT(r,Pname,exe);
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_repeat(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-repeat";
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(uv_timer_get_repeat(&timer->handle),exe));
	return 0;
}


static int fuv_timer_repeat_set1(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-repeat-set!";
	FKL_DECL_AND_CHECK_ARG2(timer_obj,repeat_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_CHECK_TYPE(repeat_obj,fklIsVMint,Pname,exe);
	if(fklIsVMnumberLt0(repeat_obj))
		FKL_RAISE_BUILTIN_ERROR_CSTR(Pname,FKL_ERR_NUMBER_SHOULD_NOT_BE_LT_0,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	uv_timer_set_repeat(&timer->handle,fklGetUint(repeat_obj));
	FKL_VM_PUSH_VALUE(exe,timer_obj);
	return 0;
}

static int fuv_timer_due_in(FKL_CPROC_ARGL)
{
	static const char Pname[]="fuv.timer-repeat";
	FKL_DECL_AND_CHECK_ARG(timer_obj,exe,Pname);
	FKL_CHECK_REST_ARG(exe,Pname);
	FKL_CHECK_TYPE(timer_obj,isFuvTimer,Pname,exe);
	FKL_DECL_VM_UD_DATA(timer,FuvTimer,timer_obj);
	FKL_VM_PUSH_VALUE(exe,fklMakeVMuint(uv_timer_get_due_in(&timer->handle),exe));
	return 0;
}

static int fuv_incomplete(FKL_CPROC_ARGL)
{
	abort();
}

struct SymFunc
{
	const char* sym;
	FklVMcFunc f;
}exports_and_func[]=
{
	// loop
	{"loop?",                fuv_loop_p,               },
	{"make-loop",            fuv_make_loop,            },
	{"loop-close!",          fuv_loop_close1,          },
	{"loop-run!",            fuv_loop_run1,            },
	{"loop-mode",            fuv_loop_mode,            },
	{"loop-alive?",          fuv_loop_alive_p,         },
	{"loop-stop!",           fuv_incomplete,           },
	{"loop-backend-fd",      fuv_loop_backend_fd,      },
	{"loop-backend-timeout", fuv_loop_backend_timeout, },
	{"loop-now",             fuv_loop_now,             },
	{"loop-update-time!",    fuv_loop_update_time1,    },
	{"loop-walk!",           fuv_incomplete,           },
	{"loop-configure!",      fuv_incomplete,           },

	// timer
	{"timer?",               fuv_timer_p,              },
	{"make-timer",           fuv_make_timer,           },
	{"timer-start!",         fuv_timer_start1,         },
	{"timer-stop!",          fuv_timer_stop1,          },
	{"timer-again!",         fuv_timer_again1,         },
	{"timer-due-in",         fuv_timer_due_in,         },
	{"timer-repeat",         fuv_timer_repeat,         },
	{"timer-repeat-set!",    fuv_timer_repeat_set1,    },
};

static const size_t EXPORT_NUM=sizeof(exports_and_func)/sizeof(struct SymFunc);

FKL_DLL_EXPORT void _fklExportSymbolInit(FKL_CODEGEN_DLL_LIB_INIT_EXPORT_FUNC_ARGS)
{
	*num=EXPORT_NUM;
	FklSid_t* symbols=(FklSid_t*)malloc(sizeof(FklSid_t)*EXPORT_NUM);
	FKL_ASSERT(symbols);
	for(size_t i=0;i<EXPORT_NUM;i++)
		symbols[i]=fklAddSymbolCstr(exports_and_func[i].sym,st)->id;
	*exports=symbols;
}

FKL_DLL_EXPORT FklVMvalue** _fklImportInit(FKL_IMPORT_DLL_INIT_FUNC_ARGS)
{
	*count=EXPORT_NUM;
	FklVMvalue** loc=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*EXPORT_NUM);
	FKL_ASSERT(loc);

	FklVMvalue* fpd=fklCreateVMvalueUd(exe
			,&FuvPublicDataMetaTable
			,dll);

	FKL_DECL_VM_UD_DATA(pd,FuvPublicData,fpd);
	init_fuv_public_data(pd,exe);

	for(size_t i=0;i<EXPORT_NUM;i++)
	{
		FklSid_t id=fklVMaddSymbolCstr(exe->gc,exports_and_func[i].sym)->id;
		FklVMcFunc func=exports_and_func[i].f;
		FklVMvalue* dlproc=fklCreateVMvalueCproc(exe,func,dll,fpd,id);
		loc[i]=dlproc;
	}
	return loc;
}
