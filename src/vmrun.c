#include<fakeLisp/reader.h>
#include<fakeLisp/fklni.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/bytecode.h>
#include<string.h>
#include<math.h>
#ifdef _WIN32
#include<tchar.h>
#include<conio.h>
#include<windows.h>
#include<io.h>
#include<process.h>
#else
#include<termios.h>
#include<dlfcn.h>
#include<unistd.h>
#endif
#include<time.h>
#include<setjmp.h>

static pthread_mutex_t GCthreadLock=PTHREAD_MUTEX_INITIALIZER;
static pthread_t GCthreadId;
static int VMargc=0;
static char** VMargv=NULL;

typedef struct Graylink
{
	FklVMvalue* v;
	struct Graylink* next;
}Graylink;

Graylink* newGraylink(FklVMvalue* v,struct Graylink* next)
{
	Graylink* g=(Graylink*)malloc(sizeof(Graylink));
	FKL_ASSERT(g);
	g->v=v;
	g->next=next;
	return g;
}

void threadErrorCallBack(void* a)
{
	void** e=(void**)a;
	int* i=(int*)a;
	FklVM* exe=e[0];
	longjmp(exe->buf,i[(sizeof(void*)*2)/sizeof(int)]);
}

/*procedure call functions*/
void callNativeProcdure(FklVM* exe,FklVMproc* tmpProc,FklVMrunnable* runnable)
{
	pthread_rwlock_wrlock(&exe->rlock);
	FklVMrunnable* tmpRunnable=fklNewVMrunnable(tmpProc,exe->rhead);
	tmpRunnable->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(tmpProc->prevEnv,exe->heap),exe->heap);
	exe->rhead=tmpRunnable;
	pthread_rwlock_unlock(&exe->rlock);
}

void applyNativeProc(FklVM* exe,FklVMproc* tmpProc,FklVMrunnable* runnable)
{
	FklVMrunnable* prevProc=fklHasSameProc(tmpProc->scp,exe->rhead);
	if(fklIsTheLastExpress(runnable,prevProc,exe)&&prevProc)
		prevProc->mark=1;
	else
	{
		pthread_rwlock_wrlock(&exe->rlock);
		FklVMrunnable* tmpRunnable=fklNewVMrunnable(tmpProc,exe->rhead);
		tmpRunnable->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(tmpProc->prevEnv,exe->heap),exe->heap);
		exe->rhead=tmpRunnable;
		pthread_rwlock_unlock(&exe->rlock);
	}
}

void tailCallNativeProcdure(FklVM* exe,FklVMproc* proc,FklVMrunnable* runnable)
{
	if(runnable->scp==proc->scp)
		runnable->mark=1;
	else
	{
		pthread_rwlock_wrlock(&exe->rlock);
		FklVMrunnable* tmpRunnable=fklNewVMrunnable(proc,exe->rhead->prev);
		tmpRunnable->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(proc->prevEnv,exe->heap),exe->heap);
		exe->rhead->prev=tmpRunnable;
		pthread_rwlock_unlock(&exe->rlock);
	}
}

int fklVMcallInDlproc(FklVMvalue* proc
		,size_t argNum
		,FklVMvalue* arglist[]
		,FklVMrunnable* runnable
		,FklVM* exe
		,FklVMFuncK kFunc
		,void* ctx
		,size_t size)
{
	FklVMstack* stack=exe->stack;
	runnable->ccc=fklNewVMcCC(kFunc,ctx,size,runnable->ccc);
	fklNiSetBp(stack->tp,stack);
	for(size_t i=argNum;i>0;i--)
		fklPushVMvalue(arglist[i-1],stack);
	switch(proc->type)
	{
		case FKL_PROC:
			{
				pthread_rwlock_wrlock(&exe->rlock);
				FklVMrunnable* tmpRunnable=fklNewVMrunnable(proc->u.proc,exe->rhead);
				tmpRunnable->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(proc->u.proc->prevEnv,exe->heap),exe->heap);
				exe->rhead=tmpRunnable;
				pthread_rwlock_unlock(&exe->rlock);
			}
			break;
		default:
			exe->nextCall=proc;
			break;
	}
	longjmp(exe->buf,2);
	return 0;
}

void callContinuation(FklVM* exe,FklVMcontinuation* cc)
{
	fklCreateCallChainWithContinuation(exe,cc);
}

void callDlProc(FklVM* exe,FklVMdlproc* dlproc)
{
	dlproc->func(exe);
}

/*--------------------------*/

FklVMlist GlobVMs={0,NULL};
pthread_rwlock_t GlobVMsLock=PTHREAD_RWLOCK_INITIALIZER;
static void B_dummy(FklVM*);
static void B_push_nil(FklVM*);
static void B_push_pair(FklVM*);
static void B_push_i32(FklVM*);
static void B_push_i64(FklVM*);
static void B_push_chr(FklVM*);
static void B_push_f64(FklVM*);
static void B_push_str(FklVM*);
static void B_push_sym(FklVM*);
static void B_push_var(FklVM*);
static void B_push_top(FklVM*);
static void B_push_proc(FklVM*);
static void B_pop(FklVM*);
static void B_pop_var(FklVM*);
static void B_pop_arg(FklVM*);
static void B_pop_rest_arg(FklVM*);
static void B_set_tp(FklVM*);
static void B_set_bp(FklVM*);
static void B_call(FklVM*);
static void B_res_tp(FklVM*);
static void B_pop_tp(FklVM*);
static void B_res_bp(FklVM*);
static void B_jmp_if_true(FklVM*);
static void B_jmp_if_false(FklVM*);
static void B_jmp(FklVM*);
static void B_push_try(FklVM*);
static void B_pop_try(FklVM*);
static void B_append(FklVM*);
static void B_push_vector(FklVM*);
static void B_push_r_env(FklVM*);
static void B_pop_r_env(FklVM*);
static void B_tail_call(FklVM*);
static void B_push_big_int(FklVM*);
static void B_push_box(FklVM*);

static void (*ByteCodes[])(FklVM*)=
{
	B_dummy,
	B_push_nil,
	B_push_pair,
	B_push_i32,
	B_push_i64,
	B_push_chr,
	B_push_f64,
	B_push_str,
	B_push_sym,
	B_push_var,
	B_push_top,
	B_push_proc,
	B_pop,
	B_pop_var,
	B_pop_arg,
	B_pop_rest_arg,
	B_set_tp,
	B_set_bp,
	B_call,
	B_res_tp,
	B_pop_tp,
	B_res_bp,
	B_jmp_if_true,
	B_jmp_if_false,
	B_jmp,
	B_push_try,
	B_pop_try,
	B_append,
	B_push_vector,
	B_push_r_env,
	B_pop_r_env,
	B_tail_call,
	B_push_big_int,
	B_push_box,
};

FklVM* fklNewVM(FklByteCode* mainCode)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	FklVMproc* tmpVMproc=NULL;
	exe->code=NULL;
	exe->size=0;
	exe->rhead=NULL;
	exe->nextCall=NULL;
	if(mainCode!=NULL)
	{
		exe->code=fklCopyMemory(mainCode->code,mainCode->size);
		exe->size=mainCode->size;
		tmpVMproc=fklNewVMproc(0,mainCode->size);
		exe->rhead=fklNewVMrunnable(tmpVMproc,exe->rhead);
		fklFreeVMproc(tmpVMproc);
	}
	exe->mark=1;
	exe->chan=NULL;
	exe->stack=fklNewVMstack(0);
	exe->tstack=fklNewPtrStack(32,16);
	exe->heap=fklNewVMheap();
	exe->callback=NULL;
	exe->nny=0;
	pthread_rwlock_init(&exe->rlock,NULL);
	FklVM** ppVM=NULL;
	int i=0;
	pthread_rwlock_wrlock(&GlobVMsLock);
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
		FKL_ASSERT(!size||GlobVMs.VMs);
		GlobVMs.VMs[size]=exe;
		GlobVMs.num+=1;
		exe->VMid=size;
	}
	pthread_rwlock_unlock(&GlobVMsLock);
	return exe;
}

FklVM* fklNewTmpVM(FklByteCode* mainCode)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	exe->code=NULL;
	exe->size=0;
	exe->rhead=NULL;
	exe->nextCall=NULL;
	if(mainCode!=NULL)
	{
		exe->code=fklCopyMemory(mainCode->code,mainCode->size);
		exe->size=mainCode->size;
		exe->rhead=fklNewVMrunnable(fklNewVMproc(0,mainCode->size),exe->rhead);
	}
	exe->mark=1;
	exe->nny=0;
	exe->chan=NULL;
	exe->stack=fklNewVMstack(0);
	exe->tstack=fklNewPtrStack(32,16);
	exe->heap=fklNewVMheap();
	pthread_rwlock_init(&exe->rlock,NULL);
	exe->callback=threadErrorCallBack;
	exe->VMid=-1;
	return exe;
}

void* ThreadVMfunc(void* p)
{
	FklVM* exe=(FklVM*)p;
	int64_t state=fklRunVM(exe);
	FklVMchanl* tmpCh=exe->chan->u.chan;
	exe->chan=NULL;
	if(!state)
	{
		FKL_NI_BEGIN(exe);
		FklVMvalue* v=NULL;
		while((v=fklNiGetArg(&ap,stack)))
			fklChanlSend(fklNewVMsend(v),tmpCh);
		fklNiEnd(&ap,stack);
	}
	fklFreeVMstack(exe->stack);
	exe->stack=NULL;
	exe->lnt=NULL;
	fklFreePtrStack(exe->tstack);
	pthread_rwlock_wrlock(&exe->rlock);
	if(state!=0)
		fklDeleteCallChain(exe);
	exe->rhead=NULL;
	exe->mark=0;
	pthread_rwlock_unlock(&exe->rlock);
	return (void*)state;
}

void callCallableObj(FklVMvalue* v,FklVM* exe)
{
	exe->nextCall=NULL;
	switch(v->type)
	{
		case FKL_CONT:
			callContinuation(exe,v->u.cont);
			break;
		case FKL_DLPROC:
			callDlProc(exe,v->u.dlproc);
			break;
		case FKL_USERDATA:
			v->u.ud->t->__call(exe,v->u.ud->data);
			break;
		default:
			break;
	}
}

int fklRunVM(FklVM* exe)
{
	while(exe->rhead)
	{
		if(setjmp(exe->buf)==1)
			return 255;
		if(exe->nextCall)
			callCallableObj(exe->nextCall,exe);
		FklVMrunnable* currunnable=exe->rhead;
		while(currunnable->ccc)
		{
			FklVMcCC* curCCC=currunnable->ccc;
			currunnable->ccc=curCCC->next;
			FklVMFuncK kFunc=curCCC->kFunc;
			void* ctx=curCCC->ctx;
			free(curCCC);
			kFunc(exe,FKL_CC_RE,ctx);
		}
		if(currunnable->cp>=currunnable->cpc+currunnable->scp)
		{
			if(currunnable->mark)
			{
				currunnable->cp=currunnable->scp;
				currunnable->mark=0;
			}
			else
			{
				pthread_rwlock_wrlock(&exe->rlock);
				FklVMrunnable* r=exe->rhead;
				exe->rhead=r->prev;
				free(r);
				pthread_rwlock_unlock(&exe->rlock);
				continue;
			}
		}
		uint64_t cp=currunnable->cp;
		ByteCodes[exe->code[cp]](exe);
		fklGC_step(exe);
	}
	return 0;
}

inline void fklChangeGCstate(FklGCstate state,FklVMheap* h)
{
	pthread_rwlock_wrlock(&h->lock);
	h->running=state;
	pthread_rwlock_unlock(&h->lock);
}

inline FklGCstate fklGetGCstate(FklVMheap* h)
{
	FklGCstate state=FKL_GC_NONE;
	pthread_rwlock_wrlock(&h->lock);
	state=h->running;
	pthread_rwlock_unlock(&h->lock);
	return state;
}

inline void fklWaitGC(FklVMheap* h)
{
	FklGCstate running=fklGetGCstate(h);
	if(running==FKL_GC_SWEEPING||running==FKL_GC_COLLECT||running==FKL_GC_DONE)
		fklGC_joinGCthread(h);
	Graylink* volatile* head=&h->gray;
	while(*head)
	{
		Graylink* cur=*head;
		*head=cur->next;
		free(cur);
	}
}

void B_dummy(FklVM* exe)
{
	FklVMrunnable* currunnable=exe->rhead;
	uint32_t scp=currunnable->scp;
	FklVMstack* stack=exe->stack;
	fklDBG_printByteCode(exe->code,scp,currunnable->cpc,stderr);
	putc('\n',stderr);
	fklDBG_printVMstack(exe->stack,stderr,1);
	putc('\n',stderr);
	fprintf(stderr,"stack->tp==%ld,stack->size==%ld\n",stack->tp,stack->size);
	fprintf(stderr,"cp=%ld stack->bp=%ld\n%s\n",currunnable->cp-scp,stack->bp,fklGetOpcodeName((FklOpcode)(exe->code[currunnable->cp])));
	fklDBG_printVMenv(currunnable->localenv->u.env,stderr);
	putc('\n',stderr);
	fprintf(stderr,"Wrong byts code!\n");
	exit(EXIT_FAILURE);
}

void B_push_nil(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	fklPushVMvalue(FKL_VM_NIL,stack);
	runnable->cp+=sizeof(char);
}

void B_push_pair(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* cdr=fklNiGetArg(&ap,stack);
	FklVMvalue* car=fklNiGetArg(&ap,stack);
	fklNiReturn(fklNewVMpairV(car,cdr,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
	runnable->cp+=sizeof(char);
}

void B_push_i32(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	fklPushVMvalue(FKL_MAKE_VM_I32(fklGetI32FromByteCode(exe->code+runnable->cp+sizeof(char))),stack);
	runnable->cp+=sizeof(char)+sizeof(int32_t);
}

void B_push_i64(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=exe->rhead;
	fklNewVMvalueToStack(FKL_I64,exe->code+r->cp+sizeof(char),stack,exe->heap);
	r->cp+=sizeof(char)+sizeof(int64_t);
}

void B_push_chr(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	fklPushVMvalue(FKL_MAKE_VM_CHR(*(char*)(exe->code+runnable->cp+1)),stack);
	runnable->cp+=sizeof(char)+sizeof(char);
}

void B_push_f64(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	fklNewVMvalueToStack(FKL_F64,exe->code+runnable->cp+sizeof(char),stack,exe->heap);
	runnable->cp+=sizeof(char)+sizeof(double);
}

void B_push_str(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	uint64_t size=fklGetU64FromByteCode(exe->code+runnable->cp+sizeof(char));
	fklNewVMvalueToStack(FKL_STR,fklNewString(size,(char*)exe->code+runnable->cp+sizeof(char)+sizeof(uint64_t)),stack,exe->heap);
	runnable->cp+=sizeof(char)+sizeof(uint64_t)+size;
}

void B_push_sym(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	fklPushVMvalue(FKL_MAKE_VM_SYM(fklGetSidFromByteCode(exe->code+runnable->cp+1)),stack);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_push_var(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+runnable->cp+sizeof(char));
	FklVMvalue* curEnv=runnable->localenv;
	FklVMvalue* volatile* pv=NULL;
	while(!pv&&curEnv&&curEnv!=FKL_VM_NIL)
	{
		pv=fklFindVar(idOfVar,curEnv->u.env);
		curEnv=curEnv->u.env->prev;
	}
	if(pv==NULL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.push-var",FKL_SYMUNDEFINE,runnable,exe);
	fklPushVMvalue(*pv,stack);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_push_top(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	if(stack->tp==stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.push-top",FKL_STACKERROR,runnable,exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	fklNiReturn(val,&ap,stack);
	fklNiReturn(val,&ap,stack);
	fklNiEnd(&ap,stack);
	runnable->cp+=sizeof(char);
}

void B_push_proc(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	uint64_t sizeOfProc=fklGetU64FromByteCode(exe->code+runnable->cp+sizeof(char));
	FklVMproc* code=fklNewVMproc(runnable->cp+sizeof(char)+sizeof(uint64_t),sizeOfProc);
	fklNewVMvalueToStack(FKL_PROC,code,stack,exe->heap);
	fklSetRef(&code->prevEnv,runnable->localenv,exe->heap);
	runnable->cp+=sizeof(char)+sizeof(uint64_t)+sizeOfProc;
}

void B_pop(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	pthread_rwlock_wrlock(&stack->lock);
	stack->tp-=1;
	pthread_rwlock_unlock(&stack->lock);
	runnable->cp+=sizeof(char);
}

void B_pop_var(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	if(!(stack->tp>stack->bp))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.pop-var",FKL_STACKERROR,runnable,exe);
	int32_t scopeOfVar=fklGetI32FromByteCode(exe->code+runnable->cp+sizeof(char));
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+runnable->cp+sizeof(char)+sizeof(int32_t));
	FklVMvalue* curEnv=runnable->localenv;
	FklVMvalue* volatile* pv=NULL;
	if(scopeOfVar>=0)
	{
		for(uint32_t i=0;i<scopeOfVar;i++)
			curEnv=curEnv->u.env->prev;
		pv=fklFindOrAddVar(idOfVar,curEnv->u.env);
	}
	else
	{
		while(!pv&&curEnv&&curEnv!=FKL_VM_NIL)
		{
			pv=fklFindVar(idOfVar,curEnv->u.env);
			curEnv=curEnv->u.env->prev;
		}
		if(pv==NULL)
			FKL_RAISE_BUILTIN_ERROR_CSTR("b.pop-var",FKL_SYMUNDEFINE,runnable,exe);
	}
	fklSetRef(pv,fklNiGetArg(&ap,stack),exe->heap);
	fklNiEnd(&ap,stack);
	if(FKL_IS_PROC(*pv)&&(*pv)->u.proc->sid==0)
		(*pv)->u.proc->sid=idOfVar;
	if(FKL_IS_DLPROC(*pv)&&(*pv)->u.dlproc->sid==0)
		(*pv)->u.dlproc->sid=idOfVar;
	runnable->cp+=sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t);
}

void B_pop_arg(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	if(ap<=stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.pop-arg",FKL_TOOFEWARG,runnable,exe);
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+runnable->cp+sizeof(char));
	FklVMvalue* curEnv=runnable->localenv;
	FklVMvalue* volatile* pValue=fklFindOrAddVar(idOfVar,curEnv->u.env);
	fklSetRef(pValue,fklNiGetArg(&ap,stack),exe->heap);
	fklNiEnd(&ap,stack);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_pop_rest_arg(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMheap* heap=exe->heap;
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+runnable->cp+sizeof(char));
	FklVMvalue* curEnv=runnable->localenv;
	FklVMvalue* obj=FKL_VM_NIL;
	FklVMvalue* volatile* pValue=&obj;
	for(;ap>stack->bp;pValue=&(*pValue)->u.pair->cdr)
		*pValue=fklNewVMpairV(fklNiGetArg(&ap,stack),FKL_VM_NIL,stack,heap);
	pValue=fklFindOrAddVar(idOfVar,curEnv->u.env);
	fklSetRef(pValue,obj,heap);
	fklNiEnd(&ap,stack);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_set_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	fklNiSetTp(stack);
	runnable->cp+=1;
}

void B_set_bp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	fklNiSetBp(stack->tp,stack);
	stack->bp=stack->tp;
	runnable->cp+=sizeof(char);
}

void B_res_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	fklNiResTp(stack);
	runnable->cp+=sizeof(char);
}

void B_pop_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	fklNiPopTp(stack);
	runnable->cp+=sizeof(char);
}

void B_res_bp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	if(stack->tp>stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.res-bp",FKL_TOOMANYARG,runnable,exe);
	stack->bp=fklPopUintStack(stack->bps);
	runnable->cp+=sizeof(char);
}

void B_call(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* tmpValue=fklNiGetArg(&ap,stack);
	if(!fklIsCallable(tmpValue))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.call",FKL_CALL_ERROR,runnable,exe);
	runnable->cp+=sizeof(char);
	switch(tmpValue->type)
	{
		case FKL_PROC:
			callNativeProcdure(exe,tmpValue->u.proc,runnable);
			break;
		default:
			exe->nextCall=tmpValue;
			break;
	}
	fklNiEnd(&ap,stack);
}

void B_tail_call(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* tmpValue=fklNiGetArg(&ap,stack);
	if(!fklIsCallable(tmpValue))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.tail-call",FKL_CALL_ERROR,runnable,exe);
	runnable->cp+=sizeof(char);
	switch(tmpValue->type)
	{
		case FKL_PROC:
			tailCallNativeProcdure(exe,tmpValue->u.proc,runnable);
			break;
		default:
			exe->nextCall=tmpValue;
			break;
	}
	fklNiEnd(&ap,stack);
}

void B_jmp_if_true(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* tmpValue=fklGetTopValue(stack);
	if(tmpValue!=FKL_VM_NIL)
		runnable->cp+=fklGetI64FromByteCode(exe->code+runnable->cp+sizeof(char));
	runnable->cp+=sizeof(char)+sizeof(int64_t);
}

void B_jmp_if_false(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* tmpValue=fklGetTopValue(stack);
	if(tmpValue==FKL_VM_NIL)
		runnable->cp+=fklGetI64FromByteCode(exe->code+runnable->cp+sizeof(char));
	runnable->cp+=sizeof(char)+sizeof(int64_t);
}

void B_jmp(FklVM* exe)
{
	FklVMrunnable* runnable=exe->rhead;
	runnable->cp+=fklGetI64FromByteCode(exe->code+runnable->cp+sizeof(char))+sizeof(char)+sizeof(int64_t);
}

void B_append(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(sec!=FKL_VM_NIL&&!FKL_IS_PAIR(sec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.append",FKL_WRONGARG,runnable,exe);
	if(sec==FKL_VM_NIL)
		fklNiReturn(fir,&ap,stack);
	else
	{
		FklVMvalue** lastcdr=&sec;
		while(FKL_IS_PAIR(*lastcdr))
		{
			lastcdr=&(*lastcdr)->u.pair->cdr;
		}
		fklSetRef(lastcdr,fir,exe->heap);
		fklNiReturn(sec,&ap,stack);
	}
	fklNiEnd(&ap,stack);
	runnable->cp+=sizeof(char);
}

void B_push_try(FklVM* exe)
{
	FklVMrunnable* r=exe->rhead;
	int32_t cpc=0;
	FklSid_t sid=fklGetSidFromByteCode(exe->code+r->cp+(++cpc));
	FklVMtryBlock* tb=fklNewVMtryBlock(sid,exe->stack->tp,exe->rhead);
	cpc+=sizeof(FklSid_t);
	uint32_t handlerNum=fklGetU32FromByteCode(exe->code+r->cp+cpc);
	cpc+=sizeof(uint32_t);
	uint32_t i=0;
	for(;i<handlerNum;i++)
	{
		uint32_t errTypeNum=fklGetU32FromByteCode(exe->code+r->cp+cpc);
		cpc+=sizeof(uint32_t);
		FklSid_t* typeIds=(FklSid_t*)malloc(sizeof(FklSid_t)*errTypeNum);
		FKL_ASSERT(typeIds);
		for(uint32_t j=0;j<errTypeNum;j++)
		{
			typeIds[j]=fklGetSidFromByteCode(exe->code+r->cp+cpc);
			cpc+=sizeof(FklSid_t);
		}
		uint64_t pCpc=fklGetU64FromByteCode(exe->code+r->cp+cpc);
		cpc+=sizeof(uint64_t);
		FklVMerrorHandler* h=fklNewVMerrorHandler(typeIds,errTypeNum,r->cp+cpc,pCpc);
		fklPushPtrStack(h,tb->hstack);
		cpc+=pCpc;
	}
	fklPushPtrStack(tb,exe->tstack);
	r->cp+=cpc;
}

void B_pop_try(FklVM* exe)
{
	FklVMrunnable* r=exe->rhead;
	FklVMtryBlock* tb=fklPopPtrStack(exe->tstack);
	fklFreeVMtryBlock(tb);
	r->cp+=sizeof(char);
}

void B_push_vector(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	uint64_t size=fklGetU64FromByteCode(exe->code+r->cp+sizeof(char));
	FklVMvalue* vec=fklNewVMvecV(size,NULL,stack,exe->heap);
	for(size_t i=size;i>0;i--)
	{
		vec->u.vec->base[i-1]=FKL_VM_NIL;
		fklSetRef(&vec->u.vec->base[i-1],fklNiGetArg(&ap,stack),exe->heap);
	}
	fklNiReturn(vec
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
	r->cp+=sizeof(char)+sizeof(uint64_t);
}

void B_push_r_env(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* n=fklNiNewVMvalue(FKL_ENV,fklNewVMenv(FKL_VM_NIL,exe->heap),stack,exe->heap);
	fklSetRef(&n->u.env->prev,r->localenv,exe->heap);
	pthread_rwlock_wrlock(&exe->rlock);
	r->localenv=n;
	pthread_rwlock_unlock(&exe->rlock);
	fklNiEnd(&ap,stack);
	r->cp+=sizeof(char);
}

void B_pop_r_env(FklVM* exe)
{
	FklVMrunnable* r=exe->rhead;
	pthread_rwlock_wrlock(&exe->rlock);
	r->localenv=r->localenv->u.env->prev;
	pthread_rwlock_unlock(&exe->rlock);
	r->cp+=sizeof(char);
}

void B_push_big_int(FklVM* exe)
{
	FklVMrunnable* r=exe->rhead;
	uint64_t num=fklGetU64FromByteCode(exe->code+r->cp+sizeof(char));
	FklBigInt* bigInt=fklNewBigIntFromMem(exe->code+r->cp+sizeof(char)+sizeof(num),sizeof(uint8_t)*num);
	fklNewVMvalueToStack(FKL_BIG_INT,bigInt,exe->stack,exe->heap);
	r->cp+=sizeof(char)+sizeof(bigInt->num)+num;
}

void B_push_box(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	FklVMvalue* box=fklNiNewVMvalue(FKL_BOX,FKL_VM_NIL,stack,exe->heap);
	fklSetRef(&box->u.box,c,exe->heap);
	fklNiReturn(box,&ap,stack);
	fklNiEnd(&ap,stack);
	r->cp+=sizeof(char);
}

FklVMstack* fklNewVMstack(int32_t size)
{
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->values=(FklVMvalue**)malloc(size*sizeof(FklVMvalue*));
	FKL_ASSERT(tmp->values);
	tmp->tps=fklNewUintStack(32,16);
	tmp->bps=fklNewUintStack(32,16);
	pthread_rwlock_init(&tmp->lock,NULL);
	return tmp;
}

void fklStackRecycle(FklVMstack* stack)
{
	if(stack->size-stack->tp>64)
	{
		stack->values=(FklVMvalue**)realloc(stack->values,sizeof(FklVMvalue*)*(stack->size-64));
		FKL_ASSERT(stack->values);
		stack->size-=64;
	}
}

void fklDBG_printVMstack(FklVMstack* stack,FILE* fp,int mode)
{
	if(fp!=stdout)fprintf(fp,"Current stack:\n");
	if(stack->tp==0)fprintf(fp,"[#EMPTY]\n");
	else
	{
		int64_t i=stack->tp-1;
		for(;i>=0;i--)
		{
			if(mode&&stack->bp==i)
				fputs("->",stderr);
			if(fp!=stdout)fprintf(fp,"%ld:",i);
			FklVMvalue* tmp=stack->values[i];
			fklPrin1VMvalue(tmp,fp);
			putc('\n',fp);
		}
	}
}

void fklDBG_printVMvalue(FklVMvalue* v,FILE* fp)
{
	fklPrin1VMvalue(v,fp);
}

FklVMrunnable* fklHasSameProc(uint32_t scp,FklVMrunnable* rhead)
{
	for(;rhead;rhead=rhead->prev)
		if(rhead->scp==scp)
			return rhead;
	return NULL;
}

int fklIsTheLastExpress(const FklVMrunnable* runnable,const FklVMrunnable* same,const FklVM* exe)
{
	if(same==NULL)
		return 0;
	uint32_t size=0;
	for(;;)
	{
		uint8_t* code=exe->code;
		uint32_t i=runnable->cp+(code[runnable->cp]==FKL_CALL||code[runnable->cp]==FKL_TAIL_CALL);
		size=runnable->scp+runnable->cpc;

		for(;i<size;i+=(code[i]==FKL_JMP)?fklGetI64FromByteCode(code+i+sizeof(char))+sizeof(char)+sizeof(int64_t):1)
			if(code[i]!=FKL_POP_TP
					&&code[i]!=FKL_POP_TRY
					&&code[i]!=FKL_JMP
					&&code[i]!=FKL_POP_R_ENV)
				return 0;
		if(runnable==same)
			break;
		runnable=runnable->prev;
	}
	return 1;
}

FklVMheap* fklNewVMheap()
{
	FklVMheap* tmp=(FklVMheap*)malloc(sizeof(FklVMheap));
	FKL_ASSERT(tmp);
	tmp->running=0;
	tmp->num=0;
	tmp->threshold=FKL_THRESHOLD_SIZE;
	tmp->head=NULL;
	tmp->gray=NULL;
	tmp->grayNum=0;
	tmp->white=NULL;
	pthread_rwlock_init(&tmp->lock,NULL);
	pthread_rwlock_init(&tmp->glock,NULL);
	return tmp;
}

void fklGC_reGray(FklVMvalue* v,FklVMheap* h)
{
	if(FKL_IS_PTR(v))
	{
		v->mark=FKL_MARK_G;
		pthread_rwlock_wrlock(&h->glock);
		h->gray=newGraylink(v,h->gray);
		h->grayNum++;
		pthread_rwlock_unlock(&h->glock);
	}
}

void fklGC_toGray(FklVMvalue* v,FklVMheap* h)
{
	if(FKL_IS_PTR(v)&&v->mark!=FKL_MARK_B)
	{
		v->mark=FKL_MARK_G;
		pthread_rwlock_wrlock(&h->glock);
		h->gray=newGraylink(v,h->gray);
		h->grayNum++;
		pthread_rwlock_unlock(&h->glock);
	}
}

void fklGC_markRootToGray(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMheap* heap=exe->heap;
	if(exe->nextCall)
		fklGC_toGray(exe->nextCall,heap);
	pthread_rwlock_rdlock(&exe->rlock);
	for(FklVMrunnable* cur=exe->rhead;cur;cur=cur->prev)
		fklGC_toGray(cur->localenv,heap);
	pthread_rwlock_unlock(&exe->rlock);
	pthread_rwlock_rdlock(&stack->lock);
	for(uint32_t i=0;i<stack->tp;i++)
	{
		FklVMvalue* value=stack->values[i];
		if(FKL_IS_PTR(value))
			fklGC_toGray(value,heap);
	}
	pthread_rwlock_unlock(&stack->lock);
	if(exe->chan)
		fklGC_toGray(exe->chan,heap);
}

void fklGC_markGlobalRoot(void)
{
	pthread_rwlock_rdlock(&GlobVMsLock);
	for(uint32_t i=0;i<GlobVMs.num;i++)
	{
		if(GlobVMs.VMs[i])
		{
			pthread_rwlock_rdlock(&GlobVMs.VMs[i]->rlock);
			unsigned mark=GlobVMs.VMs[i]->mark;
			pthread_rwlock_unlock(&GlobVMs.VMs[i]->rlock);
			if(mark)
				fklGC_markRootToGray(GlobVMs.VMs[i]);
			else
			{
				pthread_join(GlobVMs.VMs[i]->tid,NULL);
				free(GlobVMs.VMs[i]);
				GlobVMs.VMs[i]=NULL;
			}
		}
	}
	pthread_rwlock_unlock(&GlobVMsLock);
}

void fklGC_pause(FklVM* exe)
{
	if(exe->VMid!=-1)
		fklGC_markGlobalRoot();
	else
		fklGC_markRootToGray(exe);
}

void propagateMark(FklVMvalue* root,FklVMheap* heap)
{
	switch(root->type)
	{
		case FKL_PAIR:
			fklGC_toGray(root->u.pair->car,heap);
			fklGC_toGray(root->u.pair->cdr,heap);
			break;
		case FKL_PROC:
			if(root->u.proc->prevEnv)
				fklGC_toGray(root->u.proc->prevEnv,heap);
			break;
		case FKL_CONT:
			for(uint32_t i=0;i<root->u.cont->stack->tp;i++)
				fklGC_toGray(root->u.cont->stack->values[i],heap);
			for(FklVMrunnable* curr=root->u.cont->curr;curr;curr=curr->prev)
				fklGC_toGray(curr->localenv,heap);
			if(root->u.cont->nextCall)
				fklGC_toGray(root->u.cont->nextCall,heap);
			break;
		case FKL_VECTOR:
			{
				FklVMvec* vec=root->u.vec;
				for(size_t i=0;i<vec->size;i++)
					fklGC_toGray(vec->base[i],heap);
			}
			break;
		case FKL_BOX:
			fklGC_toGray(root->u.box,heap);
			break;
		case FKL_CHAN:
			{
				pthread_mutex_lock(&root->u.chan->lock);
				FklQueueNode* head=root->u.chan->messages->head;
				for(;head;head=head->next)
					fklGC_toGray(head->data,heap);
				for(head=root->u.chan->sendq->head;head;head=head->next)
					fklGC_toGray(((FklVMsend*)head->data)->m,heap);
				pthread_mutex_unlock(&root->u.chan->lock);
			}
			break;
		case FKL_DLPROC:
			if(root->u.dlproc->dll)
				fklGC_toGray(root->u.dlproc->dll,heap);
			break;
		case FKL_ENV:
			fklAtomicVMenv(root->u.env,heap);
			break;
		case FKL_USERDATA:
			if(root->u.ud->rel)
				fklGC_toGray(root->u.ud->rel,heap);
			if(root->u.ud->t->__atomic)
				root->u.ud->t->__atomic(root->u.ud->data,heap);
			break;
		case FKL_I64:
		case FKL_F64:
		case FKL_FP:
		case FKL_DLL:
		case FKL_ERR:
		case FKL_STR:
		case FKL_BIG_INT:
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

int fklGC_propagate(FklVMheap* heap)
{
	int r=0;
	pthread_rwlock_rdlock(&heap->glock);
	Graylink* g=heap->gray;
	if(!g)
		r=1;
	else
	{
		heap->grayNum--;
		heap->gray=g->next;
	}
	pthread_rwlock_unlock(&heap->glock);
	if(g)
	{
		FklVMvalue* v=g->v;
		free((void*)g);
		if(FKL_IS_PTR(v)&&v->mark==FKL_MARK_G)
		{
			v->mark=FKL_MARK_B;
			propagateMark(v,heap);
		}
	}
	return r;
}

void fklGC_collect(FklVMheap* heap)
{
	pthread_rwlock_wrlock(&heap->lock);
	FklVMvalue* head=heap->head;
	heap->head=NULL;
	heap->running=FKL_GC_SWEEPING;
	pthread_rwlock_unlock(&heap->lock);
	FklVMvalue** phead=&head;
	while(*phead)
	{
		FklVMvalue* cur=*phead;
		if(cur->mark==FKL_MARK_W)
		{
			*phead=cur->next;
			cur->next=heap->white;
			heap->white=cur;
		}
		else
		{
			cur->mark=FKL_MARK_W;
			phead=&cur->next;
		}
	}
	pthread_rwlock_wrlock(&heap->lock);
	*phead=heap->head;
	heap->head=head;
	pthread_rwlock_unlock(&heap->lock);
}

void fklGC_sweep(FklVMheap* heap)
{
	FklVMvalue* head=heap->white;
	heap->white=NULL;
	uint32_t count=0;
	FklVMvalue** phead=&head;
	while(*phead)
	{
		FklVMvalue* cur=*phead;
		*phead=cur->next;
		fklFreeVMvalue(cur);
		count++;
	}
	pthread_rwlock_wrlock(&heap->lock);
	*phead=heap->head;
	heap->head=head;
	heap->num-=count;
	heap->running=FKL_GC_DONE;
	pthread_rwlock_unlock(&heap->lock);
}

inline void fklGetGCstateAndHeapNum(FklVMheap* h,FklGCstate* s,int* cr)
{
	pthread_rwlock_rdlock(&h->lock);
	*s=h->running;
	*cr=h->num>h->threshold;
	pthread_rwlock_unlock(&h->lock);
}

//static size_t fklGetHeapNum(FklVMheap* h)
//{
//	size_t num=0;
//	pthread_rwlock_rdlock(&h->lock);
//	num=h->num;
//	pthread_rwlock_unlock(&h->lock);
//	return num;
//}

//static size_t fklGetGrayNum(FklVMheap* h)
//{
//	size_t num=0;
//	pthread_rwlock_rdlock(&h->glock);
//	num=h->grayNum;
//	pthread_rwlock_unlock(&h->glock);
//	return num;
//}

#define FKL_GC_GRAY_FACTOR (16)
#define FKL_GC_NEW_FACTOR (4)

inline void fklGC_step(FklVM* exe)
{
	FklGCstate running=FKL_GC_NONE;
	int cr=0;
	fklGetGCstateAndHeapNum(exe->heap,&running,&cr);
	static size_t grayNum=0;
	switch(running)
	{
		case FKL_GC_NONE:
			if(cr)
			{
				fklChangeGCstate(FKL_GC_MARK_ROOT,exe->heap);
				fklGC_pause(exe);
				fklChangeGCstate(FKL_GC_PROPAGATE,exe->heap);
			}
			break;
		case FKL_GC_MARK_ROOT:
			break;
		case FKL_GC_PROPAGATE:
			{
				size_t new_n=exe->heap->grayNum-grayNum;
				size_t timce=exe->heap->grayNum/FKL_GC_GRAY_FACTOR+new_n/FKL_GC_NEW_FACTOR+1;
				grayNum+=new_n;
				for(size_t i=0;i<timce;i++)
					if(fklGC_propagate(exe->heap))
					{
						fklChangeGCstate(FKL_GC_SWEEP,exe->heap);
						break;
					}
			}
			break;
		case FKL_GC_SWEEP:
			if(!pthread_mutex_trylock(&GCthreadLock))
			{
				pthread_create(&GCthreadId,NULL,fklGC_threadFunc,exe->heap);
				fklChangeGCstate(FKL_GC_COLLECT,exe->heap);
				pthread_mutex_unlock(&GCthreadLock);
			}
			break;
		case FKL_GC_COLLECT:
		case FKL_GC_SWEEPING:
			break;
		case FKL_GC_DONE:
			if(!pthread_mutex_trylock(&GCthreadLock))
			{
				fklChangeGCstate(FKL_GC_NONE,exe->heap);
				pthread_join(GCthreadId,NULL);
				pthread_mutex_unlock(&GCthreadLock);
			}
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

void* fklGC_threadFunc(void* arg)
{
	FklVMheap* heap=arg;
	fklGC_collect(heap);
	fklGC_sweep(heap);
	return NULL;
}

void fklFreeVMvalue(FklVMvalue* cur)
{
	switch(cur->type)
	{
		case FKL_STR:
			free(cur->u.str);
			break;
		case FKL_PAIR:
			free(cur->u.pair);
			break;
		case FKL_PROC:
			fklFreeVMproc(cur->u.proc);
			break;
		case FKL_CONT:
			fklFreeVMcontinuation(cur->u.cont);
			break;
		case FKL_CHAN:
			fklFreeVMchanl(cur->u.chan);
			break;
		case FKL_FP:
			fklFreeVMfp(cur->u.fp);
			break;
		case FKL_DLL:
			fklFreeVMdll(cur->u.dll);
			break;
		case FKL_DLPROC:
			fklFreeVMdlproc(cur->u.dlproc);
			break;
		case FKL_ERR:
			fklFreeVMerror(cur->u.err);
			break;
		case FKL_VECTOR:
			fklFreeVMvec(cur->u.vec);
			break;
		case FKL_USERDATA:
			if(cur->u.ud->t->__finalizer)
				cur->u.ud->t->__finalizer(cur->u.ud->data);
			fklFreeVMudata(cur->u.ud);
			break;
		case FKL_F64:
		case FKL_I64:
		case FKL_BOX:
			break;
		case FKL_ENV:
			fklFreeVMenv(cur->u.env);
			break;
		case FKL_BIG_INT:
			fklFreeBigInt(cur->u.bigInt);
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
	free((void*)cur);
}

void fklFreeAllValues(FklVMheap* heap)
{
	FklVMvalue** phead=&heap->head;
	FklVMvalue* freeDll=NULL;
	while(*phead!=NULL)
	{
		FklVMvalue* cur=*phead;
		if(FKL_IS_PTR(cur))
		{
			*phead=cur->next;
			if(FKL_IS_DLL(cur))
			{
				cur->next=freeDll;
				freeDll=cur;
			}
			else
				fklFreeVMvalue(cur);
			heap->num-=1;
		}
		else
		{
			cur->mark=FKL_MARK_W;
			phead=&cur->next;
		}
	}
	phead=&freeDll;
	while(*phead!=NULL)
	{
		FklVMvalue* cur=*phead;
		*phead=cur->next;
		fklFreeVMvalue(cur);
	}
}

FklVM* fklNewThreadVM(FklVMproc* mainCode,FklVMheap* heap)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	FklVMrunnable* t=fklNewVMrunnable(mainCode,NULL);
	t->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(mainCode->prevEnv,heap),heap);
	exe->rhead=t;
	exe->mark=1;
	exe->chan=fklNewVMvalue(FKL_CHAN,fklNewVMchanl(0),heap);
	exe->tstack=fklNewPtrStack(32,16);
	exe->stack=fklNewVMstack(0);
	exe->heap=heap;
	exe->callback=threadErrorCallBack;
	exe->nextCall=NULL;
	exe->nny=0;
	pthread_rwlock_init(&exe->rlock,NULL);
	FklVM** ppVM=NULL;
	int i=0;
	pthread_rwlock_wrlock(&GlobVMsLock);
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
		FKL_ASSERT(!size||GlobVMs.VMs);
		GlobVMs.VMs[size]=exe;
		GlobVMs.num+=1;
		exe->VMid=size;
	}
	pthread_rwlock_unlock(&GlobVMsLock);
	return exe;
}

FklVM* fklNewThreadCallableObjVM(FklVMrunnable* r,FklVMheap* heap,FklVMvalue* nextCall)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	FklVMrunnable* t=fklNewVMrunnable(NULL,NULL);
	t->cp=r->cp;
	t->localenv=NULL;
	exe->rhead=t;
	exe->mark=1;
	exe->chan=fklNewVMvalue(FKL_CHAN,fklNewVMchanl(0),heap);
	exe->tstack=fklNewPtrStack(32,16);
	exe->stack=fklNewVMstack(0);
	exe->heap=heap;
	exe->callback=threadErrorCallBack;
	exe->nextCall=nextCall;
	exe->nny=0;
	pthread_rwlock_init(&exe->rlock,NULL);
	FklVM** ppVM=NULL;
	int i=0;
	pthread_rwlock_wrlock(&GlobVMsLock);
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
		FKL_ASSERT(!size||GlobVMs.VMs);
		GlobVMs.VMs[size]=exe;
		GlobVMs.num+=1;
		exe->VMid=size;
	}
	pthread_rwlock_unlock(&GlobVMsLock);
	return exe;
}


void fklFreeVMstack(FklVMstack* stack)
{
	pthread_rwlock_destroy(&stack->lock);
	fklFreeUintStack(stack->bps);
	fklFreeUintStack(stack->tps);
	free(stack->values);
	free(stack);
}

void fklFreeAllVMs()
{
	int i=1;
	FklVM* cur=GlobVMs.VMs[0];
	fklFreePtrStack(cur->tstack);
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
	pthread_rwlock_destroy(&GlobVMsLock);
}

void fklFreeVMheap(FklVMheap* h)
{
	fklWaitGC(h);
	fklFreeAllValues(h);
	pthread_rwlock_destroy(&h->glock);
	pthread_rwlock_destroy(&h->lock);
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
				fklFreePtrStack(cur->tstack);
			}
		}
	}
}

void fklDeleteCallChain(FklVM* exe)
{
	while(exe->rhead)
	{
		FklVMrunnable* cur=exe->rhead;
		exe->rhead=cur->prev;
		fklFreeVMrunnable(cur);
	}
}

void fklCreateCallChainWithContinuation(FklVM* vm,FklVMcontinuation* cc)
{
	FklVMstack* stack=vm->stack;
	FklVMstack* tmpStack=fklCopyStack(cc->stack);
	vm->nextCall=cc->nextCall;
	int32_t i=stack->bp;
	for(;i<stack->tp;i++)
	{
		if(tmpStack->tp>=tmpStack->size)
		{
			tmpStack->values=(FklVMvalue**)realloc(tmpStack->values,sizeof(FklVMvalue*)*(tmpStack->size+64));
			FKL_ASSERT(tmpStack->values);
			tmpStack->size+=64;
		}
		tmpStack->values[tmpStack->tp]=stack->values[i];
		tmpStack->tp+=1;
	}
	pthread_rwlock_wrlock(&stack->lock);
	free(stack->values);
	fklFreeUintStack(stack->tps);
	fklFreeUintStack(stack->bps);
	stack->values=tmpStack->values;
	stack->bp=tmpStack->bp;
	stack->bps=tmpStack->bps;
	stack->size=tmpStack->size;
	stack->tp=tmpStack->tp;
	stack->tps=tmpStack->tps;
	free(tmpStack);
	pthread_rwlock_unlock(&stack->lock);
	pthread_rwlock_wrlock(&vm->rlock);
	fklDeleteCallChain(vm);
	vm->rhead=NULL;
	for(FklVMrunnable* cur=cc->curr;cur;cur=cur->prev)
	{
		FklVMrunnable* r=fklNewVMrunnable(NULL,vm->rhead);
		vm->rhead=r;
		r->cp=cur->cp;
		r->localenv=cur->localenv;
		r->scp=cur->scp;
		r->cpc=cur->cpc;
		r->sid=cur->sid;
		r->mark=cur->mark;
	}
	pthread_rwlock_unlock(&vm->rlock);
	while(!fklIsPtrStackEmpty(vm->tstack))
		fklFreeVMtryBlock(fklPopPtrStack(vm->tstack));
	for(i=0;i<cc->tnum;i++)
	{
		FklVMtryBlock* tmp=&cc->tb[i];
		FklVMtryBlock* cur=fklNewVMtryBlock(tmp->sid,tmp->tp,tmp->curr);
		int32_t j=0;
		FklPtrStack* hstack=tmp->hstack;
		uint32_t handlerNum=hstack->top;
		for(;j<handlerNum;j++)
		{
			FklVMerrorHandler* tmpH=hstack->base[j];
			FklVMerrorHandler* curH=fklNewVMerrorHandler(fklCopyMemory(tmpH->typeIds,sizeof(FklSid_t)*tmpH->num),tmpH->num,tmpH->proc.scp,tmpH->proc.cpc);
			fklPushPtrStack(curH,cur->hstack);
		}
		fklPushPtrStack(cur,vm->tstack);
	}
}

void fklInitVMargs(int argc,char** argv)
{
	VMargc=argc-1;
	if(VMargc)
		VMargv=argv+1;
}

int fklGetVMargc(void)
{
	return VMargc;
}

char** fklGetVMargv(void)
{
	return VMargv;
}

void fklGC_joinGCthread(FklVMheap* h)
{
	pthread_join(GCthreadId,NULL);
	h->running=FKL_GC_NONE;
}

inline void fklPushVMvalue(FklVMvalue* v,FklVMstack* s)
{
	pthread_rwlock_wrlock(&s->lock);
	if(s->tp>=s->size)
	{
		s->values=(FklVMvalue**)realloc(s->values
				,sizeof(FklVMvalue*)*(s->size+64));
		FKL_ASSERT(s->values);
		s->size+=64;
	}
	s->values[s->tp]=v;
	s->tp+=1;
	pthread_rwlock_unlock(&s->lock);
}

void fklFreeRunnables(FklVMrunnable* h)
{
	while(h)
	{
		FklVMrunnable* cur=h;
		h=h->prev;
		cur->localenv=NULL;
		free(cur);
	}
}
