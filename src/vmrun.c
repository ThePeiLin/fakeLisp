#define USE_CODE_NAME
#include<fakeLisp/reader.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/bytecode.h>
#include<string.h>
#include<math.h>
#ifdef _WIN32
#include<tchar.h>
#include<conio.h>
#include<windows.h>
#else
#include<termios.h>
#include<dlfcn.h>
#endif
#include<unistd.h>
#include<time.h>
#include<setjmp.h>

pthread_mutex_t GlobSharedObjsMutex=PTHREAD_MUTEX_INITIALIZER;
FklSharedObjNode* GlobSharedObjs=NULL;

void threadErrorCallBack(void* a)
{
	void** e=(void**)a;
	int* i=(int*)a;
	FklVM* exe=e[0];
	longjmp(exe->buf,i[(sizeof(void*)*2)/sizeof(int)]);
}

static int envNodeCmp(const void* a,const void* b)
{
	return ((*(FklVMenvNode**)a)->id-(*(FklVMenvNode**)b)->id);
}

pthread_rwlock_t GClock=PTHREAD_RWLOCK_INITIALIZER;

/*void ptr to FklVMvalue caster*/
#define ARGL uintptr_t p,FklVMheap* heap
#define CAST_TO_I32 return MAKE_VM_I32(p);
#define CAST_TO_I64 return fklNewVMvalue(FKL_I64,&p,heap);
FklVMvalue* castShortVp    (ARGL){CAST_TO_I32}
FklVMvalue* castIntVp      (ARGL){CAST_TO_I32}
FklVMvalue* castUShortVp   (ARGL){CAST_TO_I32}
FklVMvalue* castUintVp     (ARGL){CAST_TO_I32}
FklVMvalue* castLongVp     (ARGL){CAST_TO_I64}
FklVMvalue* castULongVp    (ARGL){CAST_TO_I64}
FklVMvalue* castLLongVp    (ARGL){CAST_TO_I64}
FklVMvalue* castULLongVp   (ARGL){CAST_TO_I64}
FklVMvalue* castPtrdiff_tVp(ARGL){CAST_TO_I64}
FklVMvalue* castSize_tVp   (ARGL){CAST_TO_I64}
FklVMvalue* castSsize_tVp  (ARGL){CAST_TO_I64}
FklVMvalue* castCharVp     (ARGL){return MAKE_VM_CHR(p);}
FklVMvalue* castWchar_tVp  (ARGL){CAST_TO_I32}
FklVMvalue* castFloatVp    (ARGL){return fklNewVMvalue(FKL_DBL,&p,heap);}
FklVMvalue* castDoubleVp   (ARGL){return fklNewVMvalue(FKL_DBL,&p,heap);}
FklVMvalue* castInt8_tVp   (ARGL){CAST_TO_I32}
FklVMvalue* castUint8_tVp  (ARGL){CAST_TO_I32}
FklVMvalue* castInt16_tVp  (ARGL){CAST_TO_I32}
FklVMvalue* castUint16_tVp (ARGL){CAST_TO_I32}
FklVMvalue* castInt32_t    (ARGL){CAST_TO_I32}
FklVMvalue* castUint32_tVp (ARGL){CAST_TO_I32}
FklVMvalue* castInt64_tVp  (ARGL){CAST_TO_I64}
FklVMvalue* castUint64_tVp (ARGL){CAST_TO_I64}
FklVMvalue* castIptrVp     (ARGL){CAST_TO_I64}
FklVMvalue* castUptrVp     (ARGL){CAST_TO_I64}
FklVMvalue* castVptrVp     (ARGL){return fklNewVMvalue(FKL_MEM,fklNewVMmem(0,(uint8_t*)p),heap);}
#undef CAST_TO_I32
#undef CAST_TO_I64
static FklVMvalue* (*castVptrToVMvalueFunctionsList[])(ARGL)=
{
	castShortVp    ,
	castIntVp      ,
	castUShortVp   ,
	castUintVp     ,
	castLongVp     ,
	castULongVp    ,
	castLLongVp    ,
	castULLongVp   ,
	castPtrdiff_tVp,
	castSize_tVp   ,
	castSsize_tVp  ,
	castCharVp     ,
	castWchar_tVp  ,
	castFloatVp    ,
	castDoubleVp   ,
	castInt8_tVp   ,
	castUint8_tVp  ,
	castInt16_tVp  ,
	castUint16_tVp ,
	castInt32_t    ,
	castUint32_tVp ,
	castInt64_tVp  ,
	castUint64_tVp ,
	castIptrVp     ,
	castUptrVp     ,
	castVptrVp     ,
};
#undef ARGL
/*--------------------------*/

/*procedure invoke functions*/
void invokeNativeProcdure(FklVM* exe,FklVMproc* tmpProc,FklVMrunnable* runnable)
{
	FklVMrunnable* prevProc=fklHasSameProc(tmpProc->scp,exe->rstack);
	if(fklIsTheLastExpress(runnable,prevProc,exe)&&prevProc)
		prevProc->mark=1;
	else
	{
		FklVMrunnable* tmpRunnable=fklNewVMrunnable(tmpProc);
		tmpRunnable->localenv=fklNewVMenv(tmpProc->prevEnv);
		fklPushPtrStack(tmpRunnable,exe->rstack);
	}
}

void invokeContinuation(FklVM* exe,VMcontinuation* cc)
{
	fklCreateCallChainWithContinuation(exe,cc);
}

void invokeDlProc(FklVM* exe,FklVMdlproc* dlproc)
{
	FklVMdllFunc dllfunc=dlproc->func;
	dllfunc(exe,&GClock);
}

void invokeFlproc(FklVM* exe,FklVMflproc* flproc)
{
	FklTypeId_t type=flproc->type;
	FklVMstack* stack=exe->stack;
	FklVMrunnable* curR=fklTopPtrStack(exe->rstack);
	FklDefFuncType* ft=(FklDefFuncType*)GET_TYPES_PTR(fklGetVMTypeUnion(type).all);
	uint32_t anum=ft->anum;
	FklTypeId_t* atypes=ft->atypes;
	uint32_t i=0;
	FklTypeId_t rtype=ft->rtype;
	FklVMvalue** args=(FklVMvalue**)malloc(sizeof(FklVMvalue*)*anum);
	FKL_ASSERT(args,"invokeFlproc",__FILE__,__LINE__);
	for(i=0;i<anum;i++)
	{
		FklVMvalue* v=fklPopVMstack(stack);
		if(v==NULL)
		{
			free(args);
			RAISE_BUILTIN_ERROR("flproc",FKL_TOOFEWARG,curR,exe);
		}
		if(IS_REF(v))
			v=*(FklVMvalue**)v;
		args[i]=v;
	}
	if(fklResBp(stack))
	{
		free(args);
		RAISE_BUILTIN_ERROR("flproc",FKL_TOOMANYARG,curR,exe);
	}
	void** pArgs=(void**)malloc(sizeof(void*)*anum);
	FKL_ASSERT(pArgs,"invokeFlproc",__FILE__,__LINE__);
	for(i=0;i<anum;i++)
		if(fklCastValueToVptr(atypes[i],args[i],&pArgs[i]))
		{
			free(args);
			free(pArgs);
			RAISE_BUILTIN_ERROR("flproc",FKL_WRONGARG,curR,exe);
		}
	uintptr_t retval=0x0;
	fklApplyFlproc(flproc,&retval,pArgs);
	if(rtype!=0)
	{
		if(rtype==fklGetStringTypeId())
			SET_RETURN("invokeFlproc",fklNewVMvalue(FKL_STR,(void*)retval,exe->heap),stack);
		else if(fklIsFunctionTypeId(rtype))
			SET_RETURN("invokeFlproc",fklNewVMvalue(FKL_FLPROC,fklNewVMflproc(rtype,(void*)retval),exe->heap),stack);
		else if(rtype==fklGetFILEpTypeId())
			SET_RETURN("invokeFlproc",fklNewVMvalue(FKL_FP,(void*)retval,exe->heap),stack);
		else
		{
			FklTypeId_t t=(rtype>fklGetLastNativeTypeId())?fklGetLastNativeTypeId():rtype;
			SET_RETURN("invokeFlproc",castVptrToVMvalueFunctionsList[t-1](retval,exe->heap),stack);
		}
	}
	for(i=0;i<anum;i++)
		free(pArgs[i]);
	free(pArgs);
	free(args);
}

/*--------------------------*/

FklVMlist GlobVMs={0,NULL};
static void B_dummy(FklVM*);
static void B_push_nil(FklVM*);
static void B_push_pair(FklVM*);
static void B_push_i32(FklVM*);
static void B_push_i64(FklVM*);
static void B_push_chr(FklVM*);
static void B_push_dbl(FklVM*);
static void B_push_str(FklVM*);
static void B_push_sym(FklVM*);
static void B_push_byte(FklVM*);
static void B_push_var(FklVM*);
//static void B_push_env_var(FklVM*);
static void B_push_top(FklVM*);
static void B_push_proc(FklVM*);
static void B_push_fproc(FklVM*);
static void B_push_ptr_ref(FklVM*);
static void B_push_def_ref(FklVM*);
static void B_push_ind_ref(FklVM*);
static void B_push_ref(FklVM*);
static void B_pop(FklVM*);
static void B_pop_var(FklVM*);
static void B_pop_arg(FklVM*);
static void B_pop_rest_arg(FklVM*);
static void B_pop_car(FklVM*);
static void B_pop_cdr(FklVM*);
static void B_pop_ref(FklVM*);
//static void B_pop_env(FklVM*);
//static void B_swap(FklVM*);
static void B_set_tp(FklVM*);
static void B_set_bp(FklVM*);
static void B_invoke(FklVM*);
static void B_res_tp(FklVM*);
static void B_pop_tp(FklVM*);
static void B_res_bp(FklVM*);
static void B_jmp_if_true(FklVM*);
static void B_jmp_if_false(FklVM*);
static void B_jmp(FklVM*);
static void B_push_try(FklVM*);
static void B_pop_try(FklVM*);
static void B_append(FklVM*);


static void (*ByteCodes[])(FklVM*)=
{
	B_dummy,
	B_push_nil,
	B_push_pair,
	B_push_i32,
	B_push_i64,
	B_push_chr,
	B_push_dbl,
	B_push_str,
	B_push_sym,
	B_push_byte,
	B_push_var,
//	B_push_env_var,
	B_push_top,
	B_push_proc,
	B_push_fproc,
	B_push_ptr_ref,
	B_push_def_ref,
	B_push_ind_ref,
	B_push_ref,
	B_pop,
	B_pop_var,
	B_pop_arg,
	B_pop_rest_arg,
	B_pop_car,
	B_pop_cdr,
	B_pop_ref,
//	B_pop_env,
//	B_swap,
	B_set_tp,
	B_set_bp,
	B_invoke,
	B_res_tp,
	B_pop_tp,
	B_res_bp,
	B_jmp_if_true,
	B_jmp_if_false,
	B_jmp,
	B_push_try,
	B_pop_try,
	B_append,
};

FklVM* fklNewVM(FklByteCode* mainCode)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe,"fklNewVM",__FILE__,__LINE__);
	FklVMproc* tmpVMproc=NULL;
	exe->code=NULL;
	exe->size=0;
	exe->rstack=fklNewPtrStack(32,16);
	if(mainCode!=NULL)
	{
		exe->code=fklCopyMemory(mainCode->code,mainCode->size);
		exe->size=mainCode->size;
		tmpVMproc=fklNewVMproc(0,mainCode->size);
		fklPushPtrStack(fklNewVMrunnable(tmpVMproc),exe->rstack);
		fklFreeVMproc(tmpVMproc);
	}
	exe->argc=0;
	exe->argv=NULL;
	exe->mark=1;
	exe->chan=NULL;
	exe->stack=fklNewVMstack(0);
	exe->tstack=fklNewPtrStack(32,16);
	exe->heap=fklNewVMheap();
	exe->callback=NULL;
	FklVM** ppVM=NULL;
	int i=0;
	for(;i<GlobVMs.num;i++)
		if(GlobVMs.VMs[i]==NULL)
			ppVM=GlobVMs.VMs+i;
	if(ppVM!=NULL)
	{
		exe->VMid=i;
		*ppVM=exe;
	}
	else
	{
		int32_t size=GlobVMs.num;
		GlobVMs.VMs=(FklVM**)realloc(GlobVMs.VMs,sizeof(FklVM*)*(size+1));
		FKL_ASSERT(!size||GlobVMs.VMs,"fklNewVM",__FILE__,__LINE__);
		GlobVMs.VMs[size]=exe;
		GlobVMs.num+=1;
		exe->VMid=size;
	}
	return exe;
}

FklVM* fklNewTmpVM(FklByteCode* mainCode)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe,"fklNewTmpVM",__FILE__,__LINE__);
	exe->code=NULL;
	exe->size=0;
	exe->rstack=fklNewPtrStack(32,16);
	if(mainCode!=NULL)
	{
		exe->code=fklCopyMemory(mainCode->code,mainCode->size);
		exe->size=mainCode->size;
		fklPushPtrStack(fklNewVMrunnable(fklNewVMproc(0,mainCode->size)),exe->rstack);
	}
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->chan=NULL;
	exe->stack=fklNewVMstack(0);
	exe->tstack=fklNewPtrStack(32,16);
	exe->heap=fklNewVMheap();
	exe->callback=threadErrorCallBack;
	exe->VMid=-1;
	return exe;
}

extern void SYS_car(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_cdr(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_cons(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_append(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_atom(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_null(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_not(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_eq(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_equal(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_eqn(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_add(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_add_1(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_sub(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_sub_1(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_mul(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_div(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_rem(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_gt(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_ge(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_lt(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_le(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_chr(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_int(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_dbl(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_str(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_sym(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_byts(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_type(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_nth(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_length(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_file(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_read(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_getb(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_prin1(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_putb(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_princ(FklVM* exe,pthread_rwlock_t* gclock);
extern void SYS_dll(FklVM*,pthread_rwlock_t*);
extern void SYS_dlsym(FklVM*,pthread_rwlock_t*);
extern void SYS_argv(FklVM*,pthread_rwlock_t*);
extern void SYS_go(FklVM*,pthread_rwlock_t*);
extern void SYS_chanl(FklVM*,pthread_rwlock_t*);
extern void SYS_send(FklVM*,pthread_rwlock_t*);
extern void SYS_recv(FklVM*,pthread_rwlock_t*);
extern void SYS_error(FklVM*,pthread_rwlock_t*);
extern void SYS_raise(FklVM*,pthread_rwlock_t*);
extern void SYS_clcc(FklVM*,pthread_rwlock_t*);
extern void SYS_apply(FklVM*,pthread_rwlock_t*);
extern void SYS_newf(FklVM*,pthread_rwlock_t*);
extern void SYS_delf(FklVM*,pthread_rwlock_t*);
extern void SYS_lfdl(FklVM*,pthread_rwlock_t*);

void fklInitGlobEnv(FklVMenv* obj,FklVMheap* heap)
{
	FklVMdllFunc syscallFunctionList[]=
	{
		SYS_car,
		SYS_cdr,
		SYS_cons,
		SYS_append,
		SYS_atom,
		SYS_null,
		SYS_not,
		SYS_eq,
		SYS_equal,
		SYS_eqn,
		SYS_add,
		SYS_add_1,
		SYS_sub,
		SYS_sub_1,
		SYS_mul,
		SYS_div,
		SYS_rem,
		SYS_gt,
		SYS_ge,
		SYS_lt,
		SYS_le,
		SYS_chr,
		SYS_int,
		SYS_dbl,
		SYS_str,
		SYS_sym,
		SYS_byts,
		SYS_type,
		SYS_nth,
		SYS_length,
		SYS_apply,
		SYS_clcc,
		SYS_file,
		SYS_read,
		SYS_getb,
		SYS_prin1,
		SYS_putb,
		SYS_princ,
		SYS_dll,
		SYS_dlsym,
		SYS_argv,
		SYS_go,
		SYS_chanl,
		SYS_send,
		SYS_recv,
		SYS_error,
		SYS_raise,
		SYS_newf,
		SYS_delf,
		SYS_lfdl,
	};
	obj->num=FKL_NUM_OF_BUILT_IN_SYMBOL;
	obj->list=(FklVMenvNode**)malloc(sizeof(FklVMenvNode*)*FKL_NUM_OF_BUILT_IN_SYMBOL);
	FKL_ASSERT(obj->list,"fklInitGlobEnv",__FILE__,__LINE__);
	obj->list[0]=fklNewVMenvNode(FKL_VM_NIL,fklAddSymbolToGlob(fklGetBuiltInSymbol(0))->id);
	obj->list[1]=fklNewVMenvNode(fklNewVMvalue(FKL_FP,stdin,heap),fklAddSymbolToGlob(fklGetBuiltInSymbol(1))->id);
	obj->list[2]=fklNewVMenvNode(fklNewVMvalue(FKL_FP,stdout,heap),fklAddSymbolToGlob(fklGetBuiltInSymbol(2))->id);
	obj->list[3]=fklNewVMenvNode(fklNewVMvalue(FKL_FP,stderr,heap),fklAddSymbolToGlob(fklGetBuiltInSymbol(3))->id);
	size_t i=4;
	for(;i<FKL_NUM_OF_BUILT_IN_SYMBOL;i++)
	{
		FklVMdlproc* proc=fklNewVMdlproc(syscallFunctionList[i-4],NULL);
		FklSymTabNode* node=fklAddSymbolToGlob(fklGetBuiltInSymbol(i));
		proc->sid=node->id;
		obj->list[i]=fklNewVMenvNode(fklNewVMvalue(FKL_DLPROC,proc,heap),node->id);
	}
	mergeSort(obj->list,obj->num,sizeof(FklVMenvNode*),envNodeCmp);
}

void* ThreadVMfunc(void* p)
{
	FklVM* exe=(FklVM*)p;
	int64_t state=fklRunVM(exe);
	FklVMchanl* tmpCh=exe->chan->u.chan;
	exe->chan=NULL;
	if(!state)
	{
		FklVMstack* stack=exe->stack;
		FklVMvalue* v=NULL;
		while((v=fklGET_VAL(fklPopVMstack(stack),exe->heap)))
			fklChanlSend(fklNewVMsend(v),tmpCh);
	}
	else
	{
		char* threadErrorMessage=fklCopyStr("error:occur in thread ");
		char* id=fklIntToString(exe->VMid);
		threadErrorMessage=fklStrCat(threadErrorMessage,id);
		threadErrorMessage=fklStrCat(threadErrorMessage,"\n");
		FklVMvalue* err=fklNewVMvalue(FKL_ERR,fklNewVMerror(NULL,fklGetBuiltInErrorType(FKL_THREADERROR),threadErrorMessage),exe->heap);
		free(threadErrorMessage);
		free(id);
		FklVMsend* t=fklNewVMsend(err);
		fklChanlSend(t,tmpCh);
	}
	fklFreeVMstack(exe->stack);
	exe->stack=NULL;
	exe->lnt=NULL;
	if(state!=0)
		fklDeleteCallChain(exe);
	fklFreePtrStack(exe->tstack);
	fklFreePtrStack(exe->rstack);
	exe->mark=0;
	return (void*)state;
}

void* ThreadVMdlprocFunc(void* p)
{
	void** a=(void**)p;
	FklVM* exe=a[0];
	FklVMdllFunc f=a[1];
	free(p);
	int64_t state=0;
	FklVMchanl* ch=exe->chan->u.chan;
	pthread_rwlock_rdlock(&GClock);
	if(!setjmp(exe->buf))
	{
		f(exe,&GClock);
		FklVMstack* stack=exe->stack;
		FklVMvalue* v=NULL;
		while((v=fklGET_VAL(fklPopVMstack(stack),exe->heap)))
			fklChanlSend(fklNewVMsend(v),ch);
	}
	else
	{
		char* threadErrorMessage=fklCopyStr("error:occur in thread ");
		char* id=fklIntToString(exe->VMid);
		threadErrorMessage=fklStrCat(threadErrorMessage,id);
		threadErrorMessage=fklStrCat(threadErrorMessage,"\n");
		FklVMvalue* err=fklNewVMvalue(FKL_ERR,fklNewVMerror(NULL,fklGetBuiltInErrorType(FKL_THREADERROR),threadErrorMessage),exe->heap);
		free(threadErrorMessage);
		free(id);
		FklVMsend* t=fklNewVMsend(err);
		fklChanlSend(t,ch);
		state=255;
	}
	pthread_rwlock_unlock(&GClock);
	fklFreeVMstack(exe->stack);
	exe->stack=NULL;
	exe->lnt=NULL;
	fklDeleteCallChain(exe);
	fklFreePtrStack(exe->rstack);
	fklFreePtrStack(exe->tstack);
	exe->mark=0;
	return (void*)state;
}

void* ThreadVMflprocFunc(void* p)
{
	void** a=(void**)p;
	FklVM* exe=a[0];
	FklVMflproc* f=a[1];
	free(p);
	int64_t state=0;
	FklVMchanl* ch=exe->chan->u.chan;
	pthread_rwlock_rdlock(&GClock);
	if(!setjmp(exe->buf))
	{
		invokeFlproc(exe,f);
		FklVMstack* stack=exe->stack;
		FklVMvalue* v=NULL;
		while((v=fklGET_VAL(fklPopVMstack(stack),exe->heap)))
			fklChanlSend(fklNewVMsend(v),ch);
	}
	else
	{
		char* threadErrorMessage=fklCopyStr("error:occur in thread ");
		char* id=fklIntToString(exe->VMid);
		threadErrorMessage=fklStrCat(threadErrorMessage,id);
		threadErrorMessage=fklStrCat(threadErrorMessage,"\n");
		FklVMvalue* err=fklNewVMvalue(FKL_ERR,fklNewVMerror(NULL,fklGetBuiltInErrorType(FKL_THREADERROR),threadErrorMessage),exe->heap);
		free(threadErrorMessage);
		free(id);
		FklVMsend* t=fklNewVMsend(err);
		fklChanlSend(t,ch);
		state=255;
	}
	pthread_rwlock_unlock(&GClock);
	fklFreeVMstack(exe->stack);
	exe->stack=NULL;
	exe->lnt=NULL;
	fklDeleteCallChain(exe);
	fklFreePtrStack(exe->rstack);
	fklFreePtrStack(exe->tstack);
	exe->mark=0;
	return (void*)state;
}

int fklRunVM(FklVM* exe)
{
	while(!fklIsPtrStackEmpty(exe->rstack))
	{
		FklVMrunnable* currunnable=fklTopPtrStack(exe->rstack);
		pthread_rwlock_rdlock(&GClock);
		if(currunnable->cp>=currunnable->cpc+currunnable->scp)
		{
			if(currunnable->mark)
			{
				currunnable->cp=currunnable->scp;
				currunnable->mark=0;
			}
			else
			{
				FklVMrunnable* r=fklPopPtrStack(exe->rstack);
				FklVMenv* tmpEnv=r->localenv;
				free(r);
				fklFreeVMenv(tmpEnv);
				continue;
			}
		}

		int32_t cp=currunnable->cp;
		if(setjmp(exe->buf)==1)
		{
			pthread_rwlock_unlock(&GClock);
			return 255;
		}
		ByteCodes[(uint8_t)exe->code[cp]](exe);
		pthread_rwlock_unlock(&GClock);
		if(exe->heap->num>exe->heap->threshold)
		{
			if(pthread_rwlock_trywrlock(&GClock))continue;
			int i=0;
			if(exe->VMid!=-1)
			{
				for(;i<GlobVMs.num;i++)
				{
					if(GlobVMs.VMs[i])
					{
						if(GlobVMs.VMs[i]->mark)
							fklGC_mark(GlobVMs.VMs[i]);
						else
						{
							pthread_join(GlobVMs.VMs[i]->tid,NULL);
							free(GlobVMs.VMs[i]);
							GlobVMs.VMs[i]=NULL;
						}
					}
				}
			}
			else fklGC_mark(exe);
			fklGC_sweep(exe->heap);
			pthread_mutex_lock(&exe->heap->lock);
			exe->heap->threshold=exe->heap->num+FKL_THRESHOLD_SIZE;
			pthread_mutex_unlock(&exe->heap->lock);
			pthread_rwlock_unlock(&GClock);
		}
	}
	return 0;
}

void B_dummy(FklVM* exe)
{
	FklVMrunnable* currunnable=fklTopPtrStack(exe->rstack);
	uint32_t scp=currunnable->scp;
	FklVMstack* stack=exe->stack;
	fklDBG_printByteCode(exe->code,scp,currunnable->cpc,stderr);
	putc('\n',stderr);
	fklDBG_printVMstack(exe->stack,stderr,1);
	putc('\n',stderr);
	fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
	fprintf(stderr,"cp=%d stack->bp=%d\n%s\n",currunnable->cp-scp,stack->bp,codeName[(uint8_t)exe->code[currunnable->cp]].codeName);
	fklDBG_printVMenv(currunnable->localenv,stderr);
	putc('\n',stderr);
	fprintf(stderr,"Wrong byts code!\n");
	exit(EXIT_FAILURE);
}

void B_push_nil(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	SET_RETURN("B_push_nil",FKL_VM_NIL,stack);
	runnable->cp+=1;
}

void B_push_pair(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	SET_RETURN("B_push_pair",fklNewVMvalue(FKL_PAIR,fklNewVMpair(),exe->heap),stack);
	runnable->cp+=1;

}

void B_push_i32(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	SET_RETURN("B_push_i32",MAKE_VM_I32(*(int32_t*)(exe->code+runnable->cp+1)),stack);
	runnable->cp+=5;
}

void B_push_i64(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	SET_RETURN("B_push_i32",fklNewVMvalue(FKL_I64,exe->code+r->cp+sizeof(char),exe->heap),stack);
	r->cp+=sizeof(char)+sizeof(int64_t);
}

void B_push_chr(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	SET_RETURN("B_push_chr",MAKE_VM_CHR(*(char*)(exe->code+runnable->cp+1)),stack);
	runnable->cp+=2;
}

void B_push_dbl(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	SET_RETURN("B_push_dbl",fklNewVMvalue(FKL_DBL,exe->code+runnable->cp+1,exe->heap),stack);
	runnable->cp+=9;
}

void B_push_str(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int len=strlen((char*)(exe->code+runnable->cp+1));
	SET_RETURN("B_push_str",fklNewVMvalue(FKL_STR,fklCopyStr((char*)exe->code+runnable->cp+1),exe->heap),stack);
	runnable->cp+=2+len;
}

void B_push_sym(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	SET_RETURN("B_push_sym",MAKE_VM_SYM(*(FklSid_t*)(exe->code+runnable->cp+1)),stack);
	runnable->cp+=5;
}

void B_push_byte(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int32_t size=*(int32_t*)(exe->code+runnable->cp+1);
	SET_RETURN("B_push_byte",fklNewVMvalue(FKL_BYTS,fklNewVMbyts(*(int32_t*)(exe->code+runnable->cp+1),exe->code+runnable->cp+5),exe->heap),stack);
	runnable->cp+=5+size;
}

void B_push_var(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklSid_t idOfVar=*(FklSid_t*)(exe->code+runnable->cp+1);
	FklVMenv* curEnv=runnable->localenv;
	FklVMenvNode* tmp=NULL;
	while(curEnv&&!tmp)
	{
		tmp=fklFindVMenvNode(idOfVar,curEnv);
		curEnv=curEnv->prev;
	}
	if(tmp==NULL)
		RAISE_BUILTIN_ERROR("b.push_var",FKL_SYMUNDEFINE,runnable,exe);
	SET_RETURN("B_push_var",tmp->value,stack);
	runnable->cp+=5;
}

//void B_push_env_var(FklVM* exe)
//{
//	FklVMstack* stack=exe->stack;
//	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
//	FklVMvalue* topValue=fklGET_VAL(fklGetTopValue(stack),exe->heap);
//	if(!IS_STR(topValue)&&!IS_SYM(topValue))
//		RAISE_BUILTIN_ERROR("b.push_env_var",FKL_WRONGARG,runnable,exe);
//	FklSymTabNode* stn=fklAddSymbolToGlob(topValue->u.str);
//	if(stn==NULL)
//		RAISE_BUILTIN_ERROR("b.push_env_var",FKL_INVALIDSYMBOL,runnable,exe);
//	FklSid_t idOfVar=stn->id;
//	FklVMenv* curEnv=runnable->localenv;
//	FklVMenvNode* tmp=NULL;
//	while(curEnv&&!tmp)
//	{
//		tmp=fklFindVMenvNode(idOfVar,curEnv);
//		curEnv=curEnv->prev;
//	}
//	if(tmp==NULL)
//		RAISE_BUILTIN_ERROR("b.push_env_var",FKL_INVALIDSYMBOL,runnable,exe);
//	stack->values[stack->tp-1]=tmp->value;
//	runnable->cp+=1;
//}

void B_push_top(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	if(stack->tp==stack->bp)
		RAISE_BUILTIN_ERROR("b.push_top",FKL_STACKERROR,runnable,exe);
	SET_RETURN("B_push_top",fklGET_VAL(fklGetTopValue(stack),exe->heap),stack);
	runnable->cp+=1;
}

void B_push_proc(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int32_t sizeOfProc=*(int32_t*)(exe->code+runnable->cp+1);
	FklVMproc* code=fklNewVMproc(runnable->cp+1+sizeof(int32_t),sizeOfProc);
	fklIncreaseVMenvRefcount(runnable->localenv);
	code->prevEnv=runnable->localenv;
	FklVMvalue* objValue=fklNewVMvalue(FKL_PRC,code,exe->heap);
	SET_RETURN("B_push_proc",objValue,stack);
	runnable->cp+=5+sizeOfProc;
}

void B_push_fproc(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklTypeId_t type=*(FklTypeId_t*)(exe->code+runnable->cp+1);
	FklVMheap* heap=exe->heap;
	FklVMvalue* sym=fklGET_VAL(fklPopVMstack(stack),heap);
	//FklVMvalue* dll=fklGET_VAL(fklPopVMstack(stack),heap);
	if((!IS_SYM(sym)&&!IS_STR(sym)))//||(!IS_DLL(dll)))
		RAISE_BUILTIN_ERROR("b.push_fproc",FKL_WRONGARG,runnable,exe);
	char* str=IS_SYM(sym)?fklGetGlobSymbolWithId(GET_SYM(sym))->symbol:sym->u.str;
	FklSharedObjNode* head=GlobSharedObjs;
	void* address=NULL;
	for(;head;head=head->next)
		address=fklGetAddress(str,head->dll);
	if(!address)
		RAISE_BUILTIN_INVALIDSYMBOL_ERROR("b.push_fproc",str,runnable,exe);
	SET_RETURN("B_push_fproc",fklNewVMvalue(FKL_FLPROC,fklNewVMflproc(type,address),heap),stack);
	runnable->cp+=5;
}

void B_push_ptr_ref(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	ssize_t offset=*(ssize_t*)(exe->code+r->cp+sizeof(char));
	FklTypeId_t ptrId=*(FklTypeId_t*)(exe->code+r->cp+sizeof(char)+sizeof(ssize_t));
	FklVMvalue* value=fklGetTopValue(stack);
	FklVMmem* mem=value->u.chf;
	if(!mem->mem)
		RAISE_BUILTIN_ERROR("b.push_ptr_ref",FKL_INVALIDACCESS,r,exe);
	stack->tp-=1;
	uint8_t* t=mem->mem+offset;
	FklVMmem* retval=fklNewVMmem(ptrId,(uint8_t*)t);
	SET_RETURN("B_push_ptr_ref",MAKE_VM_CHF(retval,exe->heap),stack);
	r->cp+=sizeof(char)+sizeof(ssize_t)+sizeof(FklTypeId_t);
}

void B_push_def_ref(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	ssize_t offset=*(ssize_t*)(exe->code+r->cp+sizeof(char));
	FklTypeId_t typeId=*(FklTypeId_t*)(exe->code+r->cp+sizeof(char)+sizeof(ssize_t));
	FklVMvalue* value=fklGetTopValue(stack);
	FklVMmem* mem=value->u.chf;
	if(!mem->mem)
		RAISE_BUILTIN_ERROR("b.push_def_ref",FKL_INVALIDACCESS,r,exe);
	stack->tp-=1;
	uint8_t* ptr=mem->mem+offset;
	FklVMvalue* retval=MAKE_VM_CHF(fklNewVMmem(typeId,ptr),exe->heap);
	SET_RETURN("B_push_def_ref",retval,stack);
	r->cp+=sizeof(char)+sizeof(ssize_t)+sizeof(FklTypeId_t);
}

void B_push_ind_ref(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* index=fklGetTopValue(stack);
	FklVMvalue* exp=fklGetValue(stack,stack->tp-2);
	FklTypeId_t type=*(FklTypeId_t*)(exe->code+r->cp+sizeof(char));
	uint32_t size=*(uint32_t*)(exe->code+r->cp+sizeof(char)+sizeof(FklTypeId_t));
	if(!IS_MEM(exp)&&!IS_CHF(exp))
		RAISE_BUILTIN_ERROR("b.push_ind_ref",FKL_WRONGARG,r,exe);
	if(!IS_I32(index))
		RAISE_BUILTIN_ERROR("b.push_ind_ref",FKL_WRONGARG,r,exe);
	FklVMmem* mem=exp->u.chf;
	if(!mem->mem)
		RAISE_BUILTIN_ERROR("b.push_ind_ref",FKL_INVALIDACCESS,r,exe);
	stack->tp-=2;
	FklVMvalue* retval=MAKE_VM_CHF(fklNewVMmem(type,mem->mem+GET_I32(index)*size),exe->heap);
	SET_RETURN("B_push_ind_ref",retval,stack);
	r->cp+=sizeof(char)+sizeof(FklTypeId_t)+sizeof(uint32_t);
}


void B_push_ref(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* exp=fklGetTopValue(stack);
	ssize_t offset=*(ssize_t*)(exe->code+r->cp+sizeof(char));
	FklTypeId_t type=*(FklTypeId_t*)(exe->code+r->cp+sizeof(char)+sizeof(ssize_t));
	if(!IS_MEM(exp)&&!IS_CHF(exp))
		RAISE_BUILTIN_ERROR("b.push_ref",FKL_WRONGARG,r,exe);
	FklVMmem* mem=exp->u.chf;
	if(!mem->mem)
		RAISE_BUILTIN_ERROR("b.push_ref",FKL_INVALIDACCESS,r,exe);
	stack->tp-=1;
	FklVMmem* retval=fklNewVMmem(type,mem->mem+offset);
	SET_RETURN("B_push_ref",MAKE_VM_CHF(retval,exe->heap),stack);
	r->cp+=sizeof(char)+sizeof(ssize_t)+sizeof(FklTypeId_t);
}

void B_pop(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	stack->tp-=1;
	fklStackRecycle(exe);
	runnable->cp+=1;
}

void B_pop_var(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	if(!(stack->tp>stack->bp))
		RAISE_BUILTIN_ERROR("b.pop_var",FKL_STACKERROR,runnable,exe);
	int32_t scopeOfVar=*(int32_t*)(exe->code+runnable->cp+1);
	FklSid_t idOfVar=*(FklSid_t*)(exe->code+runnable->cp+5);
	FklVMenv* curEnv=runnable->localenv;
	FklVMvalue** pValue=NULL;
	if(scopeOfVar>=0)
	{
		int32_t i=0;
		for(;i<scopeOfVar;i++)
			curEnv=curEnv->prev;
		FklVMenvNode* tmp=fklFindVMenvNode(idOfVar,curEnv);
		if(!tmp)
			tmp=fklAddVMenvNode(fklNewVMenvNode(NULL,idOfVar),curEnv);
		pValue=&tmp->value;
	}
	else
	{
		FklVMenvNode* tmp=NULL;
		while(curEnv&&!tmp)
		{
			tmp=fklFindVMenvNode(idOfVar,curEnv);
			curEnv=curEnv->prev;
		}
		if(tmp==NULL)
			RAISE_BUILTIN_ERROR("b.pop_var",FKL_SYMUNDEFINE,runnable,exe);
		pValue=&tmp->value;
	}
	FklVMvalue* topValue=fklGET_VAL(fklGetTopValue(stack),heap);
	*pValue=fklGET_VAL(topValue,heap);
	if(IS_PRC(*pValue)&&(*pValue)->u.prc->sid==0)
		(*pValue)->u.prc->sid=idOfVar;
	if(IS_DLPROC(*pValue)&&(*pValue)->u.dlproc->sid==0)
		(*pValue)->u.dlproc->sid=idOfVar;
	if(IS_FLPROC(*pValue)&&(*pValue)->u.flproc->sid==0)
		(*pValue)->u.flproc->sid=idOfVar;
	stack->tp-=1;
	fklStackRecycle(exe);
	runnable->cp+=9;
}

void B_pop_arg(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	if(!(stack->tp>stack->bp))
		RAISE_BUILTIN_ERROR("b.pop_arg",FKL_TOOFEWARG,runnable,exe);
	int32_t idOfVar=*(int32_t*)(exe->code+runnable->cp+1);
	FklVMenv* curEnv=runnable->localenv;
	FklVMvalue** pValue=NULL;
	FklVMenvNode* tmp=fklFindVMenvNode(idOfVar,curEnv);
	if(!tmp)
		tmp=fklAddVMenvNode(fklNewVMenvNode(NULL,idOfVar),curEnv);
	pValue=&tmp->value;
	FklVMvalue* topValue=fklGET_VAL(fklGetTopValue(stack),heap);
	*pValue=fklGET_VAL(topValue,heap);
	stack->tp-=1;
	fklStackRecycle(exe);
	runnable->cp+=5;
}

void B_pop_rest_arg(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklSid_t idOfVar=*(FklSid_t*)(exe->code+runnable->cp+1);
	FklVMenv* curEnv=runnable->localenv;
	FklVMvalue** pValue=NULL;
	FklVMenvNode* tmpNode=fklFindVMenvNode(idOfVar,curEnv);
	if(!tmpNode)
		tmpNode=fklAddVMenvNode(fklNewVMenvNode(NULL,idOfVar),curEnv);
	pValue=&tmpNode->value;
	FklVMvalue* obj=NULL;
	FklVMvalue* tmp=NULL;
	if(stack->tp>stack->bp)
	{
		obj=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),exe->heap);
		tmp=obj;
	}
	else obj=FKL_VM_NIL;
	while(stack->tp>stack->bp)
	{
		tmp->u.pair->car=fklGET_VAL(fklGetTopValue(stack),heap);
		stack->tp-=1;
		if(stack->tp>stack->bp)
			tmp->u.pair->cdr=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),heap);
		else break;
		tmp=tmp->u.pair->cdr;
	}
	*pValue=obj;
	fklStackRecycle(exe);
	runnable->cp+=5;
}

void B_pop_car(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* topValue=fklGET_VAL(fklGetTopValue(stack),heap);
	FklVMvalue* objValue=fklGET_VAL(fklGetValue(stack,stack->tp-2),heap);
	if(!IS_PAIR(objValue))
		RAISE_BUILTIN_ERROR("b.pop_car",FKL_WRONGARG,runnable,exe);
	objValue->u.pair->car=fklGET_VAL(topValue,heap);
	stack->tp-=1;
	fklStackRecycle(exe);
	runnable->cp+=1;
}

void B_pop_cdr(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* topValue=fklGET_VAL(fklGetTopValue(stack),heap);
	FklVMvalue* objValue=fklGET_VAL(fklGetValue(stack,stack->tp-2),heap);
	if(!IS_PAIR(objValue))
		RAISE_BUILTIN_ERROR("b.pop_cdr",FKL_WRONGARG,runnable,exe);
	objValue->u.pair->cdr=fklGET_VAL(topValue,heap);
	stack->tp-=1;
	fklStackRecycle(exe);
	runnable->cp+=1;
}

void B_pop_ref(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* val=fklGET_VAL(fklGetTopValue(stack),exe->heap);
	FklVMvalue* ref=fklGetValue(stack,stack->tp-2);
	if(!IS_REF(ref)&&!IS_CHF(ref)&&!IS_MEM(ref))
		RAISE_BUILTIN_ERROR("b.pop_ref",FKL_WRONGARG,runnable,exe);
	if(fklSET_REF(ref,val))
		RAISE_BUILTIN_ERROR("b.pop_ref",FKL_INVALIDASSIGN,runnable,exe);
	stack->tp-=1;
	stack->values[stack->tp-1]=val;
	fklStackRecycle(exe);
	runnable->cp+=1;
}

//void B_pop_env(FklVM* exe)
//{
//	FklVMstack* stack=exe->stack;
//	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
//	FklVMvalue* topValue=fklGET_VAL(fklGetTopValue(stack),exe->heap);
//	if(!IS_PRC(topValue))
//		RAISE_BUILTIN_ERROR("b.pop_env",FKL_WRONGARG,runnable,exe);
//	FklVMenv** ppEnv=&topValue->u.prc->prevEnv;
//	FklVMenv* tmpEnv=*ppEnv;
//	int32_t i=0;
//	int32_t scope=*(int32_t*)(exe->code+runnable->cp+1);
//	for(;i<scope&&tmpEnv;i++)
//	{
//		ppEnv=&tmpEnv->prev;
//		tmpEnv=*ppEnv;
//	}
//	if(tmpEnv)
//	{
//		*ppEnv=tmpEnv->prev;
//		tmpEnv->prev=NULL;
//		fklFreeVMenv(tmpEnv);
//	}
//	else
//		RAISE_BUILTIN_ERROR("b.pop_env",FKL_STACKERROR,runnable,exe);
//	runnable->cp+=5;
//}
//
//void B_swap(FklVM* exe)
//{
//	FklVMstack* stack=exe->stack;
//	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
//	FklVMheap* heap=exe->heap;
//	if(stack->tp<2)
//		RAISE_BUILTIN_ERROR("b.swap",FKL_TOOFEWARG,runnable,exe);
//	FklVMvalue* topValue=fklGET_VAL(fklGetTopValue(stack),heap);
//	FklVMvalue* otherValue=fklGET_VAL(fklGetValue(stack,stack->tp-2),heap);
//	stack->values[stack->tp-1]=fklGET_VAL(otherValue,heap);
//	stack->values[stack->tp-2]=fklGET_VAL(topValue,heap);
//	runnable->cp+=1;
//}

void B_set_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	if(stack->tptp>=stack->tpsi)
	{
		stack->tpst=(uint32_t*)realloc(stack->tpst,sizeof(uint32_t)*(stack->tpsi+16));
		FKL_ASSERT(stack->tpst,"B_set_tp",__FILE__,__LINE__);
		stack->tpsi+=16;
	}
	stack->tpst[stack->tptp]=stack->tp;
	stack->tptp+=1;
	runnable->cp+=1;
}

void B_set_bp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	SET_RETURN("B_set_bp",MAKE_VM_I32(stack->bp),stack);
	stack->bp=stack->tp;
	runnable->cp+=1;
}

void B_res_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	stack->tp=(stack->tptp)?stack->tpst[stack->tptp-1]:0;
	runnable->cp+=1;
}

void B_pop_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	stack->tptp-=1;
	if(stack->tpsi-stack->tptp>16)
	{
		stack->tpst=(uint32_t*)realloc(stack->tpst,sizeof(uint32_t)*(stack->tpsi-16));
		FKL_ASSERT(stack->tpst,"B_pop_tp",__FILE__,__LINE__);
		stack->tpsi-=16;
	}
	runnable->cp+=1;
}

void B_res_bp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	if(stack->tp>stack->bp)
		RAISE_BUILTIN_ERROR("b.res_bp",FKL_TOOMANYARG,runnable,exe);
	FklVMvalue* prevBp=fklGetTopValue(stack);
	stack->bp=GET_I32(prevBp);
	stack->tp-=1;
	fklStackRecycle(exe);
	runnable->cp+=1;
}

void B_invoke(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* tmpValue=fklGET_VAL(fklGetTopValue(stack),exe->heap);
	if(!IS_PTR(tmpValue)||(tmpValue->type!=FKL_PRC&&tmpValue->type!=FKL_CONT&&tmpValue->type!=FKL_DLPROC&&tmpValue->type!=FKL_FLPROC))
		RAISE_BUILTIN_ERROR("b.invoke",FKL_INVOKEERROR,runnable,exe);
	stack->tp-=1;
	fklStackRecycle(exe);
	runnable->cp+=1;
	switch(tmpValue->type)
	{
		case FKL_PRC:
			invokeNativeProcdure(exe,tmpValue->u.prc,runnable);
			break;
		case FKL_CONT:
			invokeContinuation(exe,tmpValue->u.cont);
			break;
		case FKL_DLPROC:
			invokeDlProc(exe,tmpValue->u.dlproc);
			break;
		case FKL_FLPROC:
			invokeFlproc(exe,tmpValue->u.flproc);
			break;
		default:
			break;
	}
}

void B_jmp_if_true(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* tmpValue=fklGET_VAL(fklGetTopValue(stack),exe->heap);
	if(tmpValue!=FKL_VM_NIL)
	{
		int32_t where=*(int32_t*)(exe->code+runnable->cp+sizeof(char));
		runnable->cp+=where;
	}
	runnable->cp+=5;
}

void B_jmp_if_false(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* tmpValue=fklGET_VAL(fklGetTopValue(stack),exe->heap);
	fklStackRecycle(exe);
	if(tmpValue==FKL_VM_NIL)
	{
		int32_t where=*(int32_t*)(exe->code+runnable->cp+sizeof(char));
		runnable->cp+=where;
	}
	runnable->cp+=5;
}

void B_jmp(FklVM* exe)
{
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int32_t where=*(int32_t*)(exe->code+runnable->cp+sizeof(char));
	runnable->cp+=where+5;
}

void B_append(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* fir=fklGET_VAL(fklGetTopValue(stack),heap);
	FklVMvalue* sec=fklGET_VAL(fklGetValue(stack,stack->tp-2),heap);
	if(sec!=FKL_VM_NIL&&!IS_PAIR(sec))
		RAISE_BUILTIN_ERROR("b.append",FKL_WRONGARG,runnable,exe);
	if(!IS_PAIR(sec))
		RAISE_BUILTIN_ERROR("b.append",FKL_WRONGARG,runnable,exe);
	FklVMvalue** lastpair=&sec;
	while(IS_PAIR(*lastpair))lastpair=&(*lastpair)->u.pair->cdr;
	*lastpair=fir;
	stack->tp-=1;
	fklStackRecycle(exe);
	stack->values[stack->tp-1]=sec;
	runnable->cp+=1;
}

void B_push_try(FklVM* exe)
{
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	int32_t cpc=0;
	FklSid_t sid=*(FklSid_t*)(exe->code+r->cp+(++cpc));
	FklVMtryBlock* tb=fklNewVMtryBlock(sid,exe->stack->tp,exe->rstack->top);
	cpc+=sizeof(FklSid_t);
	int32_t handlerNum=*(int32_t*)(exe->code+r->cp+cpc);
	cpc+=sizeof(int32_t);
	unsigned int i=0;
	for(;i<handlerNum;i++)
	{
		FklSid_t type=*(FklSid_t*)(exe->code+r->cp+cpc);
		cpc+=sizeof(FklSid_t);
		uint32_t pCpc=*(uint32_t*)(exe->code+r->cp+cpc);
		cpc+=sizeof(uint32_t);
		FklVMerrorHandler* h=fklNewVMerrorHandler(type,r->cp+cpc,pCpc);
		fklPushPtrStack(h,tb->hstack);
		cpc+=pCpc;
	}
	fklPushPtrStack(tb,exe->tstack);
	r->cp+=cpc;
}

void B_pop_try(FklVM* exe)
{
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMtryBlock* tb=fklPopPtrStack(exe->tstack);
	fklFreeVMtryBlock(tb);
	r->cp+=1;
}

void B_load_shared_obj(FklVM* exe)
{
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	unsigned int len=strlen((char*)(exe->code+r->cp+1));
	char* str=fklCopyStr((char*)(exe->code+r->cp+1));
#ifdef _WIN32
	FklDllHandle handle=LoadLibrary(str);
	if(!handle)
	{
		TCHAR szBuf[128];
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage (
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				dw,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
				(LPTSTR) &lpMsgBuf,
				0, NULL );
		wsprintf(szBuf,
				_T("%s error Message (error code=%d): %s"),
				_T("CreateDirectory"), dw, lpMsgBuf);
		LocalFree(lpMsgBuf);
		fprintf(stderr,"%s\n",szBuf);
	}
#else
	FklVMdllHandle handle=dlopen(str,RTLD_LAZY);
	if(!handle)
	{
		perror(dlerror());
		putc('\n',stderr);
	}
#endif
	FklSharedObjNode* node=(FklSharedObjNode*)malloc(sizeof(FklSharedObjNode));
	FKL_ASSERT(node,"B_load_shared_obj",__FILE__,__LINE__);
	node->dll=handle;
	node->next=GlobSharedObjs;
	GlobSharedObjs=node;
	r->cp+=2+len;
}

FklVMstack* fklNewVMstack(int32_t size)
{
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp,"fklNewVMstack",__FILE__,__LINE__);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->values=(FklVMvalue**)malloc(size*sizeof(FklVMvalue*));
	FKL_ASSERT(tmp->values,"fklNewVMstack",__FILE__,__LINE__);
	tmp->tpsi=0;
	tmp->tptp=0;
	tmp->tpst=NULL;
	return tmp;
}

void fklStackRecycle(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* currunnable=fklTopPtrStack(exe->rstack);
	if(stack->size-stack->tp>64)
	{
		stack->values=(FklVMvalue**)realloc(stack->values,sizeof(FklVMvalue*)*(stack->size-64));
		FKL_ASSERT(stack->values,"fklStackRecycle",__FILE__,__LINE__);
		if(stack->values==NULL)
		{
			fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
			fprintf(stderr,"cp=%d\n%s\n",currunnable->cp,codeName[(uint8_t)exe->code[currunnable->cp]].codeName);
			FKL_ASSERT(stack->values,"fklStackRecycle",__FILE__,__LINE__);
		}
		stack->size-=64;
	}
}

void fklDBG_printVMstack(FklVMstack* stack,FILE* fp,int mode)
{
	if(fp!=stdout)fprintf(fp,"Current stack:\n");
	if(stack->tp==0)fprintf(fp,"[#EMPTY]\n");
	else
	{
		int i=stack->tp-1;
		for(;i>=0;i--)
		{
			if(mode&&stack->bp==i)
				fputs("->",stderr);
			if(fp!=stdout)fprintf(fp,"%d:",i);
			FklVMvalue* tmp=stack->values[i];
			if(IS_REF(tmp))
				tmp=*(FklVMvalue**)GET_PTR(tmp);
			fklPrin1VMvalue(tmp,fp);
			putc('\n',fp);
		}
	}
}

void fklDBG_printVMvalue(FklVMvalue* v,FILE* fp)
{
	fklPrin1VMvalue(v,fp);
}

FklVMrunnable* fklHasSameProc(uint32_t scp,FklPtrStack* rstack)
{
	size_t i=0;
	for(;i<rstack->top;i++)
	{
		FklVMrunnable* cur=rstack->base[i];
		if(cur->scp==scp)
			return cur;
	}
	return NULL;
}

int fklIsTheLastExpress(const FklVMrunnable* runnable,const FklVMrunnable* same,const FklVM* exe)
{
	size_t c=exe->rstack->top;
	if(same==NULL)
		return 0;
	uint32_t size=0;
	for(;;)
	{
		uint8_t* code=exe->code;
		uint32_t i=runnable->cp+(code[runnable->cp]==FKL_INVOKE);
		size=runnable->scp+runnable->cpc;

		for(;i<size;i+=(code[i]==FKL_JMP)?*(int32_t*)(code+i+1)+5:1)
			if(code[i]!=FKL_POP_TP&&code[i]!=FKL_POP_TRY&&code[i]!=FKL_JMP)
				return 0;
		if(runnable==same)
			break;
		runnable=exe->rstack->base[--c];
	}
	return 1;
}

void fklDBG_printVMenv(FklVMenv* curEnv,FILE* fp)
{
	if(curEnv->num==0)
		fprintf(fp,"This ENV is empty!");
	else
	{
		fprintf(fp,"ENV:");
		for(int i=0;i<curEnv->num;i++)
		{
			FklVMvalue* tmp=curEnv->list[i]->value;
			fklPrin1VMvalue(tmp,fp);
			putc(' ',fp);
		}
	}
}

FklVMheap* fklNewVMheap()
{
	FklVMheap* tmp=(FklVMheap*)malloc(sizeof(FklVMheap));
	FKL_ASSERT(tmp,"fklNewVMheap",__FILE__,__LINE__);
	tmp->num=0;
	tmp->threshold=FKL_THRESHOLD_SIZE;
	tmp->head=NULL;
	pthread_mutex_init(&tmp->lock,NULL);
	return tmp;
}

void fklGC_mark(FklVM* exe)
{
	fklGC_markValueInStack(exe->stack);
	fklGC_markValueInCallChain(exe->rstack);
	if(exe->chan)
		fklGC_markValue(exe->chan);
}

void fklGC_markSendT(FklQueueNode* head)
{
	for(;head;head=head->next)
		fklGC_markValue(((FklVMsend*)head->data)->m);
}

void fklGC_markValue(FklVMvalue* obj)
{
	FklPtrStack* stack=fklNewPtrStack(32,16);
	fklPushPtrStack(obj,stack);
	while(!fklIsPtrStackEmpty(stack))
	{
		FklVMvalue* root=fklPopPtrStack(stack);
		root=GET_TAG(root)==FKL_REF_TAG?*((FklVMvalue**)GET_PTR(root)):root;
		if(GET_TAG(root)==FKL_PTR_TAG&&!root->mark)
		{
			root->mark=1;
			if(root->type==FKL_PAIR)
			{
				fklPushPtrStack(fklGetVMpairCar(root),stack);
				fklPushPtrStack(fklGetVMpairCdr(root),stack);
			}
			else if(root->type==FKL_PRC)
			{
				FklVMenv* curEnv=root->u.prc->prevEnv;
				for(;curEnv!=NULL;curEnv=curEnv->prev)
				{
					uint32_t i=0;
					for(;i<curEnv->num;i++)
						fklPushPtrStack(curEnv->list[i]->value,stack);
				}
			}
			else if(root->type==FKL_CONT)
			{
				uint32_t i=0;
				for(;i<root->u.cont->stack->tp;i++)
					fklPushPtrStack(root->u.cont->stack->values[i],stack);
				for(i=0;i<root->u.cont->num;i++)
				{
					FklVMenv* env=root->u.cont->state[i].localenv;
					uint32_t j=0;
					for(;j<env->num;j++)
						fklPushPtrStack(env->list[i]->value,stack);
				}
			}
			else if(root->type==FKL_CHAN)
			{
				pthread_mutex_lock(&root->u.chan->lock);
				FklQueueNode* head=root->u.chan->messages->head;
				for(;head;head=head->next)
					fklPushPtrStack(head->data,stack);
				for(head=root->u.chan->sendq->head;head;head=head->next)
					fklPushPtrStack(((FklVMsend*)head->data)->m,stack);
				pthread_mutex_unlock(&root->u.chan->lock);
			}
			else if(root->type==FKL_DLPROC&&root->u.dlproc->dll)
			{
				fklPushPtrStack(root->u.dlproc->dll,stack);
			}
		}
	}
	fklFreePtrStack(stack);
}

void fklGC_markValueInEnv(FklVMenv* curEnv)
{
	int32_t i=0;
	for(;i<curEnv->num;i++)
		fklGC_markValue(curEnv->list[i]->value);
}

void fklGC_markValueInCallChain(FklPtrStack* rstack)
{
	size_t i=0;
	for(;i<rstack->top;i++)
	{
		FklVMrunnable* cur=rstack->base[i];
		FklVMenv* curEnv=cur->localenv;
		for(;curEnv!=NULL;curEnv=curEnv->prev)
			fklGC_markValueInEnv(curEnv);
	}
}

void fklGC_markValueInStack(FklVMstack* stack)
{
	int32_t i=0;
	for(;i<stack->tp;i++)
	{
		FklVMvalue* t=stack->values[i];
		if(!IS_CHF(t))
			fklGC_markValue(t);
	}
}

void fklGC_markMessage(FklQueueNode* head)
{
	while(head!=NULL)
	{
		fklGC_markValue(head->data);
		head=head->next;
	}
}

void fklGC_sweep(FklVMheap* heap)
{
	FklVMvalue* cur=heap->head;
	while(cur!=NULL)
	{
		if(GET_TAG(cur)==FKL_PTR_TAG&&!cur->mark)
		{
			FklVMvalue* prev=cur;
			if(cur==heap->head)
				heap->head=cur->next;
			if(cur->next!=NULL)cur->next->prev=cur->prev;
			if(cur->prev!=NULL)cur->prev->next=cur->next;
			cur=cur->next;
			switch(prev->type)
			{
				case FKL_STR:
					free(prev->u.str);
					break;
				case FKL_PAIR:
					free(prev->u.pair);
					break;
				case FKL_PRC:
					fklFreeVMproc(prev->u.prc);
					break;
				case FKL_BYTS:
					free(prev->u.byts);
					break;
				case FKL_CONT:
					fklFreeVMcontinuation(prev->u.cont);
					break;
				case FKL_CHAN:
					fklFreeVMchanl(prev->u.chan);
					break;
				case FKL_FP:
					fklFreeVMfp(prev->u.fp);
					break;
				case FKL_DLL:
					fklFreeVMdll(prev->u.dll);
					break;
				case FKL_DLPROC:
					fklFreeVMdlproc(prev->u.dlproc);
					break;
				case FKL_FLPROC:
					fklFreeVMflproc(prev->u.flproc);
					break;
				case FKL_ERR:
					fklFreeVMerror(prev->u.err);
					break;
				case FKL_MEM:
				case FKL_CHF:
					free(prev->u.chf);
					break;
			}
			free(prev);
			heap->num-=1;
		}
		else
		{
			cur->mark=0;
			cur=cur->next;
		}
	}
}

FklVM* fklNewThreadVM(FklVMproc* mainCode,FklVMheap* heap)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe,"fklNewThreadVM",__FILE__,__LINE__);
	FklVMrunnable* t=fklNewVMrunnable(mainCode);
	t->localenv=fklNewVMenv(mainCode->prevEnv);
	exe->rstack=fklNewPtrStack(32,16);
	fklPushPtrStack(t,exe->rstack);
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->chan=fklNewVMvalue(FKL_CHAN,fklNewVMchanl(0),heap);
	exe->tstack=fklNewPtrStack(32,16);
	exe->stack=fklNewVMstack(0);
	exe->heap=heap;
	exe->callback=threadErrorCallBack;
	FklVM** ppVM=NULL;
	int i=0;
	for(;i<GlobVMs.num;i++)
		if(GlobVMs.VMs[i]==NULL)
		{
			ppVM=GlobVMs.VMs+i;
			break;
		}
	if(ppVM!=NULL)
	{
		exe->VMid=i;
		*ppVM=exe;
	}
	else
	{
		int32_t size=GlobVMs.num;
		GlobVMs.VMs=(FklVM**)realloc(GlobVMs.VMs,sizeof(FklVM*)*(size+1));
		FKL_ASSERT(!size||GlobVMs.VMs,"fklNewThreadVM",__FILE__,__LINE__);
		GlobVMs.VMs[size]=exe;
		GlobVMs.num+=1;
		exe->VMid=size;
	}
	return exe;
}

FklVM* fklNewThreadDlprocVM(FklVMrunnable* r,FklVMheap* heap)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe,"fklNewThreadVM",__FILE__,__LINE__);
	FklVMrunnable* t=fklNewVMrunnable(NULL);
	t->cp=r->cp;
	t->localenv=NULL;
	exe->rstack=fklNewPtrStack(32,16);
	fklPushPtrStack(t,exe->rstack);
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->chan=fklNewVMvalue(FKL_CHAN,fklNewVMchanl(0),heap);
	exe->tstack=fklNewPtrStack(32,16);
	exe->stack=fklNewVMstack(0);
	exe->heap=heap;
	exe->callback=threadErrorCallBack;
	FklVM** ppVM=NULL;
	int i=0;
	for(;i<GlobVMs.num;i++)
		if(GlobVMs.VMs[i]==NULL)
		{
			ppVM=GlobVMs.VMs+i;
			break;
		}
	if(ppVM!=NULL)
	{
		exe->VMid=i;
		*ppVM=exe;
	}
	else
	{
		int32_t size=GlobVMs.num;
		GlobVMs.VMs=(FklVM**)realloc(GlobVMs.VMs,sizeof(FklVM*)*(size+1));
		FKL_ASSERT(!size||GlobVMs.VMs,"fklNewThreadVM",__FILE__,__LINE__);
		GlobVMs.VMs[size]=exe;
		GlobVMs.num+=1;
		exe->VMid=size;
	}
	return exe;
}


void fklFreeVMstack(FklVMstack* stack)
{
	free(stack->tpst);
	free(stack->values);
	free(stack);
}

void fklFreeAllVMs()
{
	int i=1;
	FklVM* cur=GlobVMs.VMs[0];
	fklFreePtrStack(cur->tstack);
	fklFreePtrStack(cur->rstack);
	fklFreeVMstack(cur->stack);
	free(cur->code);
	free(cur);
	for(;i<GlobVMs.num;i++)
	{
		cur=GlobVMs.VMs[i];
		if(cur!=NULL)
			free(cur);
	}
	free(GlobVMs.VMs);
}

void fklFreeVMheap(FklVMheap* h)
{
	fklGC_sweep(h);
	pthread_mutex_destroy(&h->lock);
	free(h);
}

void fklJoinAllThread()
{
	int i=1;
	for(;i<GlobVMs.num;i++)
	{
		FklVM* cur=GlobVMs.VMs[i];
		if(cur)
			pthread_join(cur->tid,NULL);
	}
}

void fklCancelAllThread()
{
	int i=1;
	for(;i<GlobVMs.num;i++)
	{
		FklVM* cur=GlobVMs.VMs[i];
		if(cur)
		{
			pthread_cancel(cur->tid);
			pthread_join(cur->tid,NULL);
			if(cur->mark)
			{
				fklDeleteCallChain(cur);
				fklFreeVMstack(cur->stack);
				fklFreePtrStack(cur->rstack);
				fklFreePtrStack(cur->tstack);
			}
		}
	}
}

void fklDeleteCallChain(FklVM* exe)
{
	while(!fklIsPtrStackEmpty(exe->rstack))
	{
		FklVMrunnable* cur=fklPopPtrStack(exe->rstack);
		if(cur->localenv)
			fklFreeVMenv(cur->localenv);
		free(cur);
	}
}

void fklCreateCallChainWithContinuation(FklVM* vm,VMcontinuation* cc)
{
	FklVMstack* stack=vm->stack;
	FklVMstack* tmpStack=fklCopyStack(cc->stack);
	int32_t i=stack->bp;
	for(;i<stack->tp;i++)
	{
		if(tmpStack->tp>=tmpStack->size)
		{
			tmpStack->values=(FklVMvalue**)realloc(tmpStack->values,sizeof(FklVMvalue*)*(tmpStack->size+64));
			FKL_ASSERT(tmpStack->values,"fklCreateCallChainWithContinuation",__FILE__,__LINE__);
			tmpStack->size+=64;
		}
		tmpStack->values[tmpStack->tp]=stack->values[i];
		tmpStack->tp+=1;
	}
	fklDeleteCallChain(vm);
	while(!fklIsPtrStackEmpty(vm->tstack))
		fklFreeVMtryBlock(fklPopPtrStack(vm->tstack));
	vm->stack=tmpStack;
	fklFreeVMstack(stack);
	for(i=0;i<cc->num;i++)
	{
		FklVMrunnable* cur=(FklVMrunnable*)malloc(sizeof(FklVMrunnable));
		FKL_ASSERT(cur,"fklCreateCallChainWithContinuation",__FILE__,__LINE__);
		cur->cp=cc->state[i].cp;
		cur->localenv=cc->state[i].localenv;
		fklIncreaseVMenvRefcount(cur->localenv);
		cur->scp=cc->state[i].scp;
		cur->cpc=cc->state[i].cpc;
		cur->sid=cc->state[i].sid;
		cur->mark=cc->state[i].mark;
		fklPushPtrStack(cur,vm->rstack);
	}
	for(i=0;i<cc->tnum;i++)
	{
		FklVMtryBlock* tmp=&cc->tb[i];
		FklVMtryBlock* cur=fklNewVMtryBlock(tmp->sid,tmp->tp,tmp->rtp);
		int32_t j=0;
		FklPtrStack* hstack=tmp->hstack;
		uint32_t handlerNum=hstack->top;
		for(;j<handlerNum;j++)
		{
			FklVMerrorHandler* tmpH=hstack->base[j];
			FklVMerrorHandler* curH=fklNewVMerrorHandler(tmpH->type,tmpH->proc.scp,tmpH->proc.cpc);
			fklPushPtrStack(curH,cur->hstack);
		}
		fklPushPtrStack(cur,vm->tstack);
	}
}

void fklFreeAllSharedObj(void)
{
	FklSharedObjNode* head=GlobSharedObjs;
	pthread_mutex_lock(&GlobSharedObjsMutex);
	GlobSharedObjs=NULL;
	pthread_mutex_unlock(&GlobSharedObjsMutex);
	while(head)
	{
		FklSharedObjNode* prev=head;
		head=head->next;
#ifdef _WIN32
		FreeLibrary(prev->dll);
#else
		dlclose(prev->dll);
#endif
		free(prev);
	}
}