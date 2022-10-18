#include<fakeLisp/reader.h>
#include<fakeLisp/fklni.h>
#include<fakeLisp/utils.h>
#include<fakeLisp/opcode.h>
#include<fakeLisp/bytecode.h>
#include<fakeLisp/vm.h>
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

typedef struct Greylink
{
	FklVMvalue* v;
	struct Greylink* next;
}Greylink;

Greylink* newGreylink(FklVMvalue* v,struct Greylink* next)
{
	Greylink* g=(Greylink*)malloc(sizeof(Greylink));
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
void callNativeProcdure(FklVM* exe,FklVMproc* tmpProc,FklVMframe* frame)
{
	pthread_rwlock_wrlock(&exe->rlock);
	FklVMframe* tmpFrame=fklNewVMframe(tmpProc,exe->frames);
	tmpFrame->localenv=fklNewVMvalue(FKL_TYPE_ENV,fklNewVMenv(tmpProc->prevEnv,exe->gc),exe->gc);
	exe->frames=tmpFrame;
	pthread_rwlock_unlock(&exe->rlock);
}

void applyNativeProc(FklVM* exe,FklVMproc* tmpProc,FklVMframe* frame)
{
	FklVMframe* prevProc=fklHasSameProc(tmpProc->scp,exe->frames);
	if(fklIsTheLastExpress(frame,prevProc,exe)&&prevProc)
		prevProc->mark=1;
	else
	{
		pthread_rwlock_wrlock(&exe->rlock);
		FklVMframe* tmpFrame=fklNewVMframe(tmpProc,exe->frames);
		tmpFrame->localenv=fklNewVMvalue(FKL_TYPE_ENV,fklNewVMenv(tmpProc->prevEnv,exe->gc),exe->gc);
		exe->frames=tmpFrame;
		pthread_rwlock_unlock(&exe->rlock);
	}
}

void tailCallNativeProcdure(FklVM* exe,FklVMproc* proc,FklVMframe* frame)
{
	if(frame->scp==proc->scp)
		frame->mark=1;
	else
	{
		pthread_rwlock_wrlock(&exe->rlock);
		FklVMframe* tmpFrame=fklNewVMframe(proc,exe->frames->prev);
		tmpFrame->localenv=fklNewVMvalue(FKL_TYPE_ENV,fklNewVMenv(proc->prevEnv,exe->gc),exe->gc);
		exe->frames->prev=tmpFrame;
		pthread_rwlock_unlock(&exe->rlock);
	}
}

int fklVMcallInDlproc(FklVMvalue* proc
		,size_t argNum
		,FklVMvalue* arglist[]
		,FklVMframe* frame
		,FklVM* exe
		,FklVMFuncK kFunc
		,void* ctx
		,size_t size)
{
	FklVMstack* stack=exe->stack;
	frame->ccc=fklNewVMcCC(kFunc,ctx,size,frame->ccc);
	fklNiSetBp(stack->tp,stack);
	for(size_t i=argNum;i>0;i--)
		fklPushVMvalue(arglist[i-1],stack);
	switch(proc->type)
	{
		case FKL_TYPE_PROC:
			{
				pthread_rwlock_wrlock(&exe->rlock);
				FklVMframe* tmpFrame=fklNewVMframe(proc->u.proc,exe->frames);
				tmpFrame->localenv=fklNewVMvalue(FKL_TYPE_ENV,fklNewVMenv(proc->u.proc->prevEnv,exe->gc),exe->gc);
				exe->frames=tmpFrame;
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

//static FklVMnode* newVMnode(FklVM* exe,FklVMnode* next)
//{
//	FklVMnode* t=(FklVMnode*)malloc(sizeof(FklVMnode));
//	FKL_ASSERT(t);
//	t->vm=exe;
//	t->next=next;
//	return t;
//}

//static FklVMlist list=
//{
//	NULL,
//	PTHREAD_RWLOCK_INITIALIZER,
//};
//
//static FklVMlist* GlobVMs=&list;
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
static void B_list_append(FklVM*);
static void B_push_vector(FklVM*);
static void B_push_r_env(FklVM*);
static void B_pop_r_env(FklVM*);
static void B_tail_call(FklVM*);
static void B_push_big_int(FklVM*);
static void B_push_box(FklVM*);
static void B_push_bytevector(FklVM*);
static void B_push_hash_eq(FklVM*);
static void B_push_hash_eqv(FklVM*);
static void B_push_hash_equal(FklVM*);
static void B_push_list_0(FklVM*);
static void B_push_list(FklVM*);
static void B_push_vector_0(FklVM*);
static void B_list_push(FklVM*);

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
	B_list_append,
	B_push_vector,
	B_push_r_env,
	B_pop_r_env,
	B_tail_call,
	B_push_big_int,
	B_push_box,
	B_push_bytevector,
	B_push_hash_eq,
	B_push_hash_eqv,
	B_push_hash_equal,
	B_push_list_0,
	B_push_list,
	B_push_vector_0,
	B_list_push,
};

FklVM* fklNewVM(FklByteCode* mainCode,FklVM* prev,FklVM* next)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	FklVMproc* tmpVMproc=NULL;
	exe->code=NULL;
	exe->size=0;
	exe->frames=NULL;
	exe->nextCall=NULL;
	exe->nextCallBackUp=NULL;
	exe->tid=pthread_self();
	if(mainCode!=NULL)
	{
		exe->code=fklCopyMemory(mainCode->code,mainCode->size);
		exe->size=mainCode->size;
		tmpVMproc=fklNewVMproc(0,mainCode->size);
		exe->frames=fklNewVMframe(tmpVMproc,exe->frames);
		fklFreeVMproc(tmpVMproc);
	}
	exe->mark=1;
	exe->chan=NULL;
	exe->stack=fklNewVMstack(0);
	exe->tstack=fklNewPtrStack(32,16);
	exe->gc=fklNewVMgc();
	exe->callback=NULL;
	exe->nny=0;
//	exe->thrds=1;
	pthread_rwlock_init(&exe->rlock,NULL);
	pthread_mutex_init(&exe->prev_next_lock,NULL);
	exe->prev=prev;
	exe->next=next;
	if(prev)
	{
		pthread_mutex_lock(&exe->prev->prev_next_lock);
		exe->prev->next=exe;
		pthread_mutex_unlock(&exe->prev->prev_next_lock);
	}
	if(next)
	{
		pthread_mutex_lock(&exe->next->prev_next_lock);
		exe->next->prev=exe;
		pthread_mutex_unlock(&exe->next->prev_next_lock);
	}
	return exe;
}

FklVM* fklNewTmpVM(FklByteCode* mainCode,FklVMgc* gc,FklVM* prev,FklVM* next)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	exe->code=NULL;
	exe->size=0;
	exe->frames=NULL;
	exe->nextCall=NULL;
	exe->nextCallBackUp=NULL;
	if(mainCode!=NULL)
	{
		exe->code=fklCopyMemory(mainCode->code,mainCode->size);
		exe->size=mainCode->size;
		exe->frames=fklNewVMframe(fklNewVMproc(0,mainCode->size),exe->frames);
	}
	exe->tid=pthread_self();
	exe->mark=1;
	exe->nny=0;
	exe->chan=NULL;
	exe->stack=fklNewVMstack(0);
	exe->tstack=fklNewPtrStack(32,16);
	if(gc)
		exe->gc=gc;
	else
		exe->gc=fklNewVMgc();
	pthread_rwlock_init(&exe->rlock,NULL);
	//	exe->callback=threadErrorCallBack;
	exe->callback=NULL;
//	exe->thrds=1;
	pthread_mutex_init(&exe->prev_next_lock,NULL);
	exe->prev=prev;
	exe->next=next;
	if(prev)
	{
		pthread_mutex_lock(&exe->prev->prev_next_lock);
		exe->prev->next=exe;
		pthread_mutex_unlock(&exe->prev->prev_next_lock);
	}
	if(next)
	{
		pthread_mutex_lock(&exe->next->prev_next_lock);
		exe->next->prev=exe;
		pthread_mutex_unlock(&exe->next->prev_next_lock);
	}
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
	exe->frames=NULL;
	exe->mark=0;
	pthread_rwlock_unlock(&exe->rlock);
	return (void*)state;
}

void callCallableObj(FklVMvalue* v,FklVM* exe)
{
	exe->nextCallBackUp=exe->nextCall;
	exe->nextCall=NULL;
	switch(v->type)
	{
		case FKL_TYPE_CONT:
			callContinuation(exe,v->u.cont);
			break;
		case FKL_TYPE_DLPROC:
			callDlProc(exe,v->u.dlproc);
			break;
		case FKL_TYPE_USERDATA:
			v->u.ud->t->__call(v->u.ud->data,exe);
			break;
		default:
			break;
	}
}

int fklRunVM(FklVM* exe)
{
	while(exe->frames)
	{
		if(setjmp(exe->buf)==1)
			return 255;
		if(exe->nextCall)
			callCallableObj(exe->nextCall,exe);
		FklVMframe* curframe=exe->frames;
		while(curframe->ccc)
		{
			FklVMcCC* curCCC=curframe->ccc;
			curframe->ccc=curCCC->next;
			FklVMFuncK kFunc=curCCC->kFunc;
			void* ctx=curCCC->ctx;
			free(curCCC);
			kFunc(exe,FKL_CC_RE,ctx);
		}
		if(curframe->cp>=curframe->cpc+curframe->scp)
		{
			if(curframe->mark)
			{
				curframe->cp=curframe->scp;
				curframe->mark=0;
			}
			else
			{
				pthread_rwlock_wrlock(&exe->rlock);
				FklVMframe* frame=exe->frames;
				exe->frames=frame->prev;
				free(frame);
				pthread_rwlock_unlock(&exe->rlock);
				continue;
			}
		}
		uint64_t cp=curframe->cp;
		ByteCodes[exe->code[cp]](exe);
//		fklGC_step(exe);
	}
	return 0;
}

inline void fklChangeGCstate(FklGCstate state,FklVMgc* gc)
{
	pthread_rwlock_wrlock(&gc->lock);
	gc->running=state;
	pthread_rwlock_unlock(&gc->lock);
}

inline FklGCstate fklGetGCstate(FklVMgc* gc)
{
	FklGCstate state=FKL_GC_NONE;
	pthread_rwlock_wrlock(&gc->lock);
	state=gc->running;
	pthread_rwlock_unlock(&gc->lock);
	return state;
}

inline void fklWaitGC(FklVMgc* gc)
{
	FklGCstate running=fklGetGCstate(gc);
	if(running==FKL_GC_SWEEPING||running==FKL_GC_COLLECT||running==FKL_GC_DONE)
		fklGC_joinGCthread(gc);
	fklChangeGCstate(FKL_GC_NONE,gc);
	gc->greyNum=0;
	for(Greylink* volatile* head=&gc->grey;*head;)
	{
		Greylink* cur=*head;
		*head=cur->next;
		free(cur);
	}
	for(FklVMvalue* head=gc->head;head;head=head->next)
		head->mark=FKL_MARK_W;
}

void B_dummy(FklVM* exe)
{
	FKL_ASSERT(0);
}

void B_push_nil(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	fklPushVMvalue(FKL_VM_NIL,stack);
	frame->cp+=sizeof(char);
}

void B_push_pair(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* cdr=fklNiGetArg(&ap,stack);
	FklVMvalue* car=fklNiGetArg(&ap,stack);
	fklNiReturn(fklNewVMpairV(car,cdr,stack,exe->gc),&ap,stack);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char);
}

void B_push_i32(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	fklPushVMvalue(FKL_MAKE_VM_I32(fklGetI32FromByteCode(exe->code+frame->cp+sizeof(char))),stack);
	frame->cp+=sizeof(char)+sizeof(int32_t);
}

void B_push_i64(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	fklNewVMvalueToStack(FKL_TYPE_I64,exe->code+frame->cp+sizeof(char),stack,exe->gc);
	frame->cp+=sizeof(char)+sizeof(int64_t);
}

void B_push_chr(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	fklPushVMvalue(FKL_MAKE_VM_CHR(*(char*)(exe->code+frame->cp+1)),stack);
	frame->cp+=sizeof(char)+sizeof(char);
}

void B_push_f64(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	fklNewVMvalueToStack(FKL_TYPE_F64,exe->code+frame->cp+sizeof(char),stack,exe->gc);
	frame->cp+=sizeof(char)+sizeof(double);
}

void B_push_str(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	uint64_t size=fklGetU64FromByteCode(exe->code+frame->cp+sizeof(char));
	fklNewVMvalueToStack(FKL_TYPE_STR,fklNewString(size,(char*)exe->code+frame->cp+sizeof(char)+sizeof(uint64_t)),stack,exe->gc);
	frame->cp+=sizeof(char)+sizeof(uint64_t)+size;
}

void B_push_sym(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	fklPushVMvalue(FKL_MAKE_VM_SYM(fklGetSidFromByteCode(exe->code+frame->cp+1)),stack);
	frame->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_push_var(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+frame->cp+sizeof(char));
	FklVMvalue* curEnv=frame->localenv;
	FklVMvalue* volatile* pv=NULL;
	while(!pv&&curEnv&&curEnv!=FKL_VM_NIL)
	{
		pv=fklFindVar(idOfVar,curEnv->u.env);
		curEnv=curEnv->u.env->prev;
	}
	if(pv==NULL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.push-var",FKL_ERR_SYMUNDEFINE,frame,exe);
	fklPushVMvalue(*pv,stack);
	frame->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_push_top(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	if(stack->tp==stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.push-top",FKL_ERR_STACKERROR,frame,exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	fklNiReturn(val,&ap,stack);
	fklNiReturn(val,&ap,stack);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char);
}

void B_push_proc(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	uint64_t sizeOfProc=fklGetU64FromByteCode(exe->code+frame->cp+sizeof(char));
	FklVMproc* code=fklNewVMproc(frame->cp+sizeof(char)+sizeof(uint64_t),sizeOfProc);
	fklNewVMvalueToStack(FKL_TYPE_PROC,code,stack,exe->gc);
	fklSetRef(&code->prevEnv,frame->localenv,exe->gc);
	frame->cp+=sizeof(char)+sizeof(uint64_t)+sizeOfProc;
}

void B_pop(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
//	pthread_rwlock_wrlock(&stack->lock);
	stack->tp-=1;
//	pthread_rwlock_unlock(&stack->lock);
	frame->cp+=sizeof(char);
}

void B_pop_var(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	if(!(stack->tp>stack->bp))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.pop-var",FKL_ERR_STACKERROR,frame,exe);
	uint32_t scopeOfVar=fklGetU32FromByteCode(exe->code+frame->cp+sizeof(char));
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+frame->cp+sizeof(char)+sizeof(int32_t));
	FklVMvalue* curEnv=frame->localenv;
	FklVMvalue* volatile* pv=NULL;
	if(scopeOfVar)
	{
		for(uint32_t i=1;i<scopeOfVar;i++)
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
			FKL_RAISE_BUILTIN_ERROR_CSTR("b.pop-var",FKL_ERR_SYMUNDEFINE,frame,exe);
	}
	fklSetRef(pv,fklNiGetArg(&ap,stack),exe->gc);
	fklNiEnd(&ap,stack);
	if(FKL_IS_PROC(*pv)&&(*pv)->u.proc->sid==0)
		(*pv)->u.proc->sid=idOfVar;
	if(FKL_IS_DLPROC(*pv)&&(*pv)->u.dlproc->sid==0)
		(*pv)->u.dlproc->sid=idOfVar;
	frame->cp+=sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t);
}

void B_pop_arg(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	if(ap<=stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.pop-arg",FKL_ERR_TOOFEWARG,frame,exe);
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+frame->cp+sizeof(char));
	FklVMvalue* curEnv=frame->localenv;
	FklVMvalue* volatile* pValue=fklFindOrAddVar(idOfVar,curEnv->u.env);
	fklSetRef(pValue,fklNiGetArg(&ap,stack),exe->gc);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_pop_rest_arg(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMgc* gc=exe->gc;
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+frame->cp+sizeof(char));
	FklVMvalue* curEnv=frame->localenv;
	FklVMvalue* obj=FKL_VM_NIL;
	FklVMvalue* volatile* pValue=&obj;
	for(;ap>stack->bp;pValue=&(*pValue)->u.pair->cdr)
		*pValue=fklNewVMpairV(fklNiGetArg(&ap,stack),FKL_VM_NIL,stack,gc);
	pValue=fklFindOrAddVar(idOfVar,curEnv->u.env);
	fklSetRef(pValue,obj,gc);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_set_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	fklNiSetTp(stack);
	frame->cp+=1;
}

void B_set_bp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	fklNiSetBp(stack->tp,stack);
	stack->bp=stack->tp;
	frame->cp+=sizeof(char);
}

void B_res_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	fklNiResTp(stack);
	frame->cp+=sizeof(char);
}

void B_pop_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	fklNiPopTp(stack);
	frame->cp+=sizeof(char);
}

void B_res_bp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	if(stack->tp>stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.res-bp",FKL_ERR_TOOMANYARG,frame,exe);
	stack->bp=fklPopUintStack(stack->bps);
	frame->cp+=sizeof(char);
}

void B_call(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* tmpValue=fklNiGetArg(&ap,stack);
	if(!fklIsCallable(tmpValue))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.call",FKL_ERR_CALL_ERROR,frame,exe);
	frame->cp+=sizeof(char);
	switch(tmpValue->type)
	{
		case FKL_TYPE_PROC:
			callNativeProcdure(exe,tmpValue->u.proc,frame);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* tmpValue=fklNiGetArg(&ap,stack);
	if(!fklIsCallable(tmpValue))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.tail-call",FKL_ERR_CALL_ERROR,frame,exe);
	frame->cp+=sizeof(char);
	switch(tmpValue->type)
	{
		case FKL_TYPE_PROC:
			tailCallNativeProcdure(exe,tmpValue->u.proc,frame);
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
	FklVMframe* frame=exe->frames;
	FklVMvalue* tmpValue=fklGetTopValue(stack);
	if(tmpValue!=FKL_VM_NIL)
		frame->cp+=fklGetI64FromByteCode(exe->code+frame->cp+sizeof(char));
	frame->cp+=sizeof(char)+sizeof(int64_t);
}

void B_jmp_if_false(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	FklVMvalue* tmpValue=fklGetTopValue(stack);
	if(tmpValue==FKL_VM_NIL)
		frame->cp+=fklGetI64FromByteCode(exe->code+frame->cp+sizeof(char));
	frame->cp+=sizeof(char)+sizeof(int64_t);
}

void B_jmp(FklVM* exe)
{
	FklVMframe* frame=exe->frames;
	frame->cp+=fklGetI64FromByteCode(exe->code+frame->cp+sizeof(char))+sizeof(char)+sizeof(int64_t);
}

void B_list_append(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(sec==FKL_VM_NIL)
		fklNiReturn(fir,&ap,stack);
	else
	{
		FklVMvalue** lastcdr=&sec;
		while(FKL_IS_PAIR(*lastcdr))
			lastcdr=&(*lastcdr)->u.pair->cdr;
		if(*lastcdr!=FKL_VM_NIL)
			FKL_RAISE_BUILTIN_ERROR_CSTR("b.list-append",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
		fklSetRef(lastcdr,fir,exe->gc);
		fklNiReturn(sec,&ap,stack);
	}
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char);
}

void B_push_try(FklVM* exe)
{
	FklVMframe* frame=exe->frames;
	int32_t cpc=0;
	FklSid_t sid=fklGetSidFromByteCode(exe->code+frame->cp+(++cpc));
	FklVMtryBlock* tb=fklNewVMtryBlock(sid,exe->stack->tp,exe->frames);
	cpc+=sizeof(FklSid_t);
	uint32_t handlerNum=fklGetU32FromByteCode(exe->code+frame->cp+cpc);
	cpc+=sizeof(uint32_t);
	uint32_t i=0;
	for(;i<handlerNum;i++)
	{
		uint32_t errTypeNum=fklGetU32FromByteCode(exe->code+frame->cp+cpc);
		cpc+=sizeof(uint32_t);
		FklSid_t* typeIds=(FklSid_t*)malloc(sizeof(FklSid_t)*errTypeNum);
		FKL_ASSERT(typeIds);
		for(uint32_t j=0;j<errTypeNum;j++)
		{
			typeIds[j]=fklGetSidFromByteCode(exe->code+frame->cp+cpc);
			cpc+=sizeof(FklSid_t);
		}
		uint64_t pCpc=fklGetU64FromByteCode(exe->code+frame->cp+cpc);
		cpc+=sizeof(uint64_t);
		FklVMerrorHandler* h=fklNewVMerrorHandler(typeIds,errTypeNum,frame->cp+cpc,pCpc);
		fklPushPtrStack(h,tb->hstack);
		cpc+=pCpc;
	}
	fklPushPtrStack(tb,exe->tstack);
	frame->cp+=cpc;
}

void B_pop_try(FklVM* exe)
{
	FklVMframe* frame=exe->frames;
	FklVMtryBlock* tb=fklPopPtrStack(exe->tstack);
	fklFreeVMtryBlock(tb);
	frame->cp+=sizeof(char);
}

void B_push_vector(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	uint64_t size=fklGetU64FromByteCode(exe->code+frame->cp+sizeof(char));
	FklVMvalue* vec=fklNewVMvecV(size,NULL,stack,exe->gc);
	for(size_t i=size;i>0;i--)
		fklSetRef(&vec->u.vec->base[i-1],fklNiGetArg(&ap,stack),exe->gc);
	fklNiReturn(vec
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char)+sizeof(uint64_t);
}

void B_push_r_env(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* n=fklNewVMvalueToStack(FKL_TYPE_ENV,fklNewVMenv(FKL_VM_NIL,exe->gc),stack,exe->gc);
	fklSetRef(&n->u.env->prev,frame->localenv,exe->gc);
	pthread_rwlock_wrlock(&exe->rlock);
	frame->localenv=n;
	pthread_rwlock_unlock(&exe->rlock);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char);
}

void B_pop_r_env(FklVM* exe)
{
	FklVMframe* frame=exe->frames;
	pthread_rwlock_wrlock(&exe->rlock);
	frame->localenv=frame->localenv->u.env->prev;
	pthread_rwlock_unlock(&exe->rlock);
	frame->cp+=sizeof(char);
}

void B_push_big_int(FklVM* exe)
{
	FklVMframe* frame=exe->frames;
	uint64_t num=fklGetU64FromByteCode(exe->code+frame->cp+sizeof(char));
	FklBigInt* bigInt=fklNewBigIntFromMem(exe->code+frame->cp+sizeof(char)+sizeof(num),sizeof(uint8_t)*num);
	fklNewVMvalueToStack(FKL_TYPE_BIG_INT,bigInt,exe->stack,exe->gc);
	frame->cp+=sizeof(char)+sizeof(bigInt->num)+num;
}

void B_push_box(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	FklVMvalue* box=fklNewVMvalueToStack(FKL_TYPE_BOX,FKL_VM_NIL,stack,exe->gc);
	fklSetRef(&box->u.box,c,exe->gc);
	fklNiReturn(box,&ap,stack);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char);
}

void B_push_bytevector(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMframe* frame=exe->frames;
	uint64_t size=fklGetU64FromByteCode(exe->code+frame->cp+sizeof(char));
	fklNewVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklNewBytevector(size,exe->code+frame->cp+sizeof(char)+sizeof(uint64_t)),stack,exe->gc);
	frame->cp+=sizeof(char)+sizeof(uint64_t)+size;
}

void B_push_hash_eq(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	uint64_t num=fklGetU64FromByteCode(exe->code+frame->cp+sizeof(char));
	FklVMvalue* hash=fklNewVMvalueToStack(FKL_TYPE_HASHTABLE
			,fklNewVMhashTable(FKL_VM_HASH_EQ),stack,exe->gc);
	for(size_t i=0;i<num;i++)
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		FklVMvalue* key=fklNiGetArg(&ap,stack);
		fklSetVMhashTableInReverseOrder(key,value,hash->u.hash,exe->gc);
	}
	fklNiReturn(hash,&ap,stack);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char)+sizeof(uint64_t);
}

void B_push_hash_eqv(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	uint64_t num=fklGetU64FromByteCode(exe->code+frame->cp+sizeof(char));
	FklVMvalue* hash=fklNewVMvalueToStack(FKL_TYPE_HASHTABLE
			,fklNewVMhashTable(FKL_VM_HASH_EQV),stack,exe->gc);
	for(size_t i=0;i<num;i++)
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		FklVMvalue* key=fklNiGetArg(&ap,stack);
		fklSetVMhashTableInReverseOrder(key,value,hash->u.hash,exe->gc);
	}
	fklNiReturn(hash,&ap,stack);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char)+sizeof(uint64_t);
}

void B_push_hash_equal(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	uint64_t num=fklGetU64FromByteCode(exe->code+frame->cp+sizeof(char));
	FklVMvalue* hash=fklNewVMvalueToStack(FKL_TYPE_HASHTABLE
			,fklNewVMhashTable(FKL_VM_HASH_EQUAL),stack,exe->gc);
	for(size_t i=0;i<num;i++)
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		FklVMvalue* key=fklNiGetArg(&ap,stack);
		fklSetVMhashTableInReverseOrder(key,value,hash->u.hash,exe->gc);
	}
	fklNiReturn(hash,&ap,stack);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char)+sizeof(uint64_t);
}

void B_push_list_0(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* pair=FKL_VM_NIL;
	FklVMvalue* last=fklNiGetArg(&ap,stack);
	FklVMvalue** pcur=&pair;
	size_t bp=stack->bp;
	for(size_t i=bp;ap>bp;ap--,i++)
	{
		*pcur=fklNewVMpairV(stack->values[i],FKL_VM_NIL,stack,exe->gc);
		pcur=&(*pcur)->u.pair->cdr;
	}
	fklSetRef(pcur,last,exe->gc);
	fklNiResBp(&ap,stack);
	fklNiReturn(pair,&ap,stack);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char);
}

void B_push_list(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	uint64_t size=fklGetU64FromByteCode(exe->code+frame->cp+sizeof(char));
	FklVMvalue* pair=FKL_VM_NIL;
	FklVMvalue* last=fklNiGetArg(&ap,stack);
	size--;
	FklVMvalue** pcur=&pair;
	for(size_t i=ap-size;i<ap;i++)
	{
		*pcur=fklNewVMpairV(stack->values[i],FKL_VM_NIL,stack,exe->gc);
		pcur=&(*pcur)->u.pair->cdr;
	}
	ap-=size;
	fklSetRef(pcur,last,exe->gc);
	fklNiReturn(pair,&ap,stack);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char)+sizeof(uint64_t);
}

void B_push_vector_0(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	size_t size=ap-stack->bp;
	FklVMvalue* vec=fklNewVMvecV(size,NULL,stack,exe->gc);
	for(size_t i=size;i>0;i--)
		fklSetRef(&vec->u.vec->base[i-1],fklNiGetArg(&ap,stack),exe->gc);
	fklNiResBp(&ap,stack);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char);
}

void B_list_push(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMframe* frame=exe->frames;
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	for(;FKL_IS_PAIR(list);list=list->u.pair->cdr)
		fklNiReturn(list->u.pair->car,&ap,stack);
	if(list!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.list-push",FKL_ERR_INCORRECT_TYPE_VALUE,frame,exe);
	fklNiEnd(&ap,stack);
	frame->cp+=sizeof(char);
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
//	pthread_rwlock_init(&tmp->lock,NULL);
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

FklVMframe* fklHasSameProc(uint32_t scp,FklVMframe* frames)
{
	for(;frames;frames=frames->prev)
		if(frames->scp==scp)
			return frames;
	return NULL;
}

int fklIsTheLastExpress(const FklVMframe* frame,const FklVMframe* same,const FklVM* exe)
{
	if(same==NULL)
		return 0;
	uint32_t size=0;
	for(;;)
	{
		uint8_t* code=exe->code;
		uint32_t i=frame->cp+(code[frame->cp]==FKL_OP_CALL||code[frame->cp]==FKL_OP_TAIL_CALL);
		size=frame->scp+frame->cpc;

		for(;i<size;i+=(code[i]==FKL_OP_JMP)?fklGetI64FromByteCode(code+i+sizeof(char))+sizeof(char)+sizeof(int64_t):1)
			if(code[i]!=FKL_OP_POP_TP
					&&code[i]!=FKL_OP_POP_TRY
					&&code[i]!=FKL_OP_JMP
					&&code[i]!=FKL_OP_POP_R_ENV)
				return 0;
		if(frame==same)
			break;
		frame=frame->prev;
	}
	return 1;
}

FklVMgc* fklNewVMgc()
{
	FklVMgc* tmp=(FklVMgc*)malloc(sizeof(FklVMgc));
	FKL_ASSERT(tmp);
	tmp->running=0;
	tmp->num=0;
	tmp->threshold=FKL_THRESHOLD_SIZE;
	tmp->head=NULL;
	tmp->grey=NULL;
	tmp->greyNum=0;
	pthread_rwlock_init(&tmp->lock,NULL);
	pthread_rwlock_init(&tmp->greylock,NULL);
	return tmp;
}

//void fklGC_reGrey(FklVMvalue* v,FklVMgc* gc)
//{
//	if(FKL_IS_PTR(v))
//	{
//		v->mark=FKL_MARK_G;
//		pthread_rwlock_wrlock(&gc->greylock);
//		h->grey=newGreylink(v,h->grey);
//		h->greyNum++;
//		pthread_rwlock_unlock(&gc->greylock);
//	}
//}

void fklGC_toGrey(FklVMvalue* v,FklVMgc* gc)
{
	//if(FKL_IS_PTR(v)&&atomic_load(&v->mark)!=FKL_MARK_B)
	if(FKL_IS_PTR(v)&&v->mark!=FKL_MARK_B)
	{
		v->mark=FKL_MARK_G;
//		atomic_store(&v->mark,FKL_MARK_G);
		pthread_rwlock_wrlock(&gc->greylock);
		gc->grey=newGreylink(v,gc->grey);
		gc->greyNum++;
		pthread_rwlock_unlock(&gc->greylock);
	}
}

void fklGC_markRootToGrey(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMgc* gc=exe->gc;
	if(exe->nextCall)
		fklGC_toGrey(exe->nextCall,gc);
	if(exe->nextCallBackUp)
		fklGC_toGrey(exe->nextCallBackUp,gc);
//	pthread_rwlock_rdlock(&exe->rlock);
	for(FklVMframe* cur=exe->frames;cur;cur=cur->prev)
		fklGC_toGrey(cur->localenv,gc);
//	pthread_rwlock_unlock(&exe->rlock);
//	pthread_rwlock_rdlock(&stack->lock);
	for(uint32_t i=0;i<stack->tp;i++)
	{
		FklVMvalue* value=stack->values[i];
		if(FKL_IS_PTR(value))
			fklGC_toGrey(value,gc);
	}
//	pthread_rwlock_unlock(&stack->lock);
	if(exe->chan)
		fklGC_toGrey(exe->chan,gc);
}

void fklGC_markAllRootToGrey(FklVM* curVM)
{
	for(FklVM* cur=curVM;cur;cur=cur->prev)
	{
		pthread_rwlock_rdlock(&cur->rlock);
		uint32_t mark=cur->mark;
		pthread_rwlock_unlock(&cur->rlock);
		if(mark)
			fklGC_markRootToGrey(cur);
		else
		{
			//pthread_join(cur->vm->tid,NULL);
			//*ph=cur->next;
			//free(cur->vm);
			//free(cur);
		}
	}
	for(FklVM* cur=curVM;cur;cur=cur->next)
	{
		pthread_rwlock_rdlock(&cur->rlock);
		uint32_t mark=cur->mark;
		pthread_rwlock_unlock(&cur->rlock);
		if(mark)
			fklGC_markRootToGrey(cur);
		else
		{
			//pthread_join(cur->vm->tid,NULL);
			//*ph=cur->next;
			//free(cur->vm);
			//free(cur);
		}
	}
}

void fklGC_pause(FklVM* exe)
{
	pthread_rwlock_rdlock(&exe->gc->lock);
	fklGC_markAllRootToGrey(exe);
	pthread_rwlock_unlock(&exe->gc->lock);
}

void propagateMark(FklVMvalue* root,FklVMgc* gc)
{
	switch(root->type)
	{
		case FKL_TYPE_PAIR:
			fklGC_toGrey(root->u.pair->car,gc);
			fklGC_toGrey(root->u.pair->cdr,gc);
			break;
		case FKL_TYPE_PROC:
			if(root->u.proc->prevEnv)
				fklGC_toGrey(root->u.proc->prevEnv,gc);
			break;
		case FKL_TYPE_CONT:
			for(uint32_t i=0;i<root->u.cont->stack->tp;i++)
				fklGC_toGrey(root->u.cont->stack->values[i],gc);
			for(FklVMframe* curr=root->u.cont->curr;curr;curr=curr->prev)
				fklGC_toGrey(curr->localenv,gc);
			if(root->u.cont->nextCall)
				fklGC_toGrey(root->u.cont->nextCall,gc);
			break;
		case FKL_TYPE_VECTOR:
			{
				FklVMvec* vec=root->u.vec;
				for(size_t i=0;i<vec->size;i++)
					fklGC_toGrey(vec->base[i],gc);
			}
			break;
		case FKL_TYPE_BOX:
			fklGC_toGrey(root->u.box,gc);
			break;
		case FKL_TYPE_CHAN:
			{
				pthread_mutex_lock(&root->u.chan->lock);
				FklQueueNode* head=root->u.chan->messages->head;
				for(;head;head=head->next)
					fklGC_toGrey(head->data,gc);
				for(head=root->u.chan->sendq->head;head;head=head->next)
					fklGC_toGrey(((FklVMsend*)head->data)->m,gc);
				pthread_mutex_unlock(&root->u.chan->lock);
			}
			break;
		case FKL_TYPE_DLPROC:
			if(root->u.dlproc->dll)
				fklGC_toGrey(root->u.dlproc->dll,gc);
			break;
		case FKL_TYPE_ENV:
			fklAtomicVMenv(root->u.env,gc);
			break;
		case FKL_TYPE_HASHTABLE:
			fklAtomicVMhashTable(root->u.hash,gc);
			break;
		case FKL_TYPE_USERDATA:
			if(root->u.ud->rel)
				fklGC_toGrey(root->u.ud->rel,gc);
			if(root->u.ud->t->__atomic)
				root->u.ud->t->__atomic(root->u.ud->data,gc);
			break;
		case FKL_TYPE_I64:
		case FKL_TYPE_F64:
		case FKL_TYPE_FP:
		case FKL_TYPE_DLL:
		case FKL_TYPE_ERR:
		case FKL_TYPE_STR:
		case FKL_TYPE_BYTEVECTOR:
		case FKL_TYPE_BIG_INT:
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

int fklGC_propagate(FklVMgc* gc)
{
	pthread_rwlock_wrlock(&gc->greylock);
	Greylink* g=gc->grey;
	FklVMvalue* v=FKL_VM_NIL;
	if(g)
	{
		gc->greyNum--;
		gc->grey=g->next;
		v=g->v;
		free(g);
	}
	pthread_rwlock_unlock(&gc->greylock);
	//if(FKL_IS_PTR(v)&&atomic_load(&v->mark)==FKL_MARK_G)
	if(FKL_IS_PTR(v)&&v->mark==FKL_MARK_G)
	{
		v->mark=FKL_MARK_B;
//		atomic_store(&v->mark,FKL_MARK_B);
		propagateMark(v,gc);
	}
	return gc->grey==NULL;
}

void fklGC_collect(FklVMgc* gc,FklVMvalue** pw)
{
	size_t count=0;
//	pthread_rwlock_wrlock(&gc->lock);
	FklVMvalue* head=gc->head;
	gc->head=NULL;
	gc->running=FKL_GC_SWEEPING;
//	pthread_rwlock_unlock(&gc->lock);
	FklVMvalue** phead=&head;
	while(*phead)
	{
		FklVMvalue* cur=*phead;
		//if(atomic_exchange(&cur->mark,FKL_MARK_W)==FKL_MARK_W)
		if(cur->mark==FKL_MARK_W)
		{
			*phead=cur->next;
			cur->next=*pw;
			*pw=cur;
			count++;
		}
		else
		{
			cur->mark=FKL_MARK_W;
			phead=&cur->next;
		}
	}
//	pthread_rwlock_wrlock(&gc->lock);
	*phead=gc->head;
	gc->head=head;
	gc->num-=count;
//	pthread_rwlock_unlock(&gc->lock);
}

void fklGC_sweep(FklVMvalue* head)
{
	FklVMvalue** phead=&head;
	while(*phead)
	{
		FklVMvalue* cur=*phead;
		*phead=cur->next;
		fklFreeVMvalue(cur);
	}
}

inline void fklGetGCstateAndGCNum(FklVMgc* gc,FklGCstate* s,int* cr)
{
	pthread_rwlock_rdlock(&gc->lock);
	*s=gc->running;
	*cr=gc->num>gc->threshold;
	pthread_rwlock_unlock(&gc->lock);
}

//static size_t fklGetHeapNum(FklVMgc* gc)
//{
//	size_t num=0;
//	pthread_rwlock_rdlock(&gc->lock);
//	num=h->num;
//	pthread_rwlock_unlock(&gc->lock);
//	return num;
//}

//static size_t fklGetGreyNum(FklVMgc* gc)
//{
//	size_t num=0;
//	pthread_rwlock_rdlock(&gc->greylock);
//	num=h->greyNum;
//	pthread_rwlock_unlock(&gc->greylock);
//	return num;
//}

#define FKL_GC_GRAY_FACTOR (16)
#define FKL_GC_NEW_FACTOR (4)

inline void fklGC_step(FklVM* exe)
{
	FklGCstate running=FKL_GC_NONE;
	int cr=0;
	fklGetGCstateAndGCNum(exe->gc,&running,&cr);
	static size_t greyNum=0;
	cr=1;
	switch(running)
	{
		case FKL_GC_NONE:
			if(cr)
			{
				fklChangeGCstate(FKL_GC_MARK_ROOT,exe->gc);
				fklGC_pause(exe);
				fklChangeGCstate(FKL_GC_PROPAGATE,exe->gc);
			}
			break;
		case FKL_GC_MARK_ROOT:
			break;
		case FKL_GC_PROPAGATE:
			{
				size_t new_n=exe->gc->greyNum-greyNum;
				size_t timce=exe->gc->greyNum/FKL_GC_GRAY_FACTOR+new_n/FKL_GC_NEW_FACTOR+1;
				greyNum+=new_n;
				for(size_t i=0;i<timce;i++)
					if(fklGC_propagate(exe->gc))
					{
						fklChangeGCstate(FKL_GC_SWEEP,exe->gc);
						break;
					}
			}
			break;
		case FKL_GC_SWEEP:
			if(!pthread_mutex_trylock(&GCthreadLock))
			{
				pthread_create(&GCthreadId,NULL,fklGC_sweepThreadFunc,exe->gc);
				fklChangeGCstate(FKL_GC_COLLECT,exe->gc);
				pthread_mutex_unlock(&GCthreadLock);
			}
			break;
		case FKL_GC_COLLECT:
		case FKL_GC_SWEEPING:
			break;
		case FKL_GC_DONE:
			if(!pthread_mutex_trylock(&GCthreadLock))
			{
				fklChangeGCstate(FKL_GC_NONE,exe->gc);
				pthread_join(GCthreadId,NULL);
				pthread_mutex_unlock(&GCthreadLock);
			}
			break;
		default:
			FKL_ASSERT(0);
			break;
	}
}

void* fklGC_sweepThreadFunc(void* arg)
{
	FklVMgc* gc=arg;
	FklVMvalue* white=NULL;
	fklGC_collect(gc,&white);
	fklChangeGCstate(FKL_GC_SWEEPING,gc);
	fklGC_sweep(white);
	fklChangeGCstate(FKL_GC_DONE,gc);
	return NULL;
}

void* fklGC_threadFunc(void* arg)
{
	FklVM* exe=arg;
	FklVMgc* gc=exe->gc;
	gc->running=FKL_GC_MARK_ROOT;
//	pthread_rwlock_rdlock(&GlobVMs->lock);
//	FklVMnode** ph=&GlobVMs->h;
//	for(;*ph&&(*ph)->vm->gc!=gc;ph=&(*ph)->next);
	fklGC_markAllRootToGrey(exe);
//	for(;*ph&&(*ph)->vm->gc==gc;)
//	{
//		FklVMnode* cur=*ph;
//		int32_t mark=cur->vm->mark;
//		if(mark)
//		{
//			fklGC_markRootToGrey(cur->vm);
//			ph=&cur->next;
//		}
//		else
//		{
//			pthread_join(cur->vm->tid,NULL);
//			*ph=cur->next;
//			free(cur->vm);
//			free(cur);
//		}
//	}
//	pthread_rwlock_unlock(&GlobVMs->lock);
	gc->running=FKL_GC_PROPAGATE;
	while(!fklGC_propagate(gc));
	FklVMvalue* white=NULL;
	gc->running=FKL_GC_COLLECT;
	fklGC_collect(gc,&white);
	gc->running=FKL_GC_SWEEP;
	gc->running=FKL_GC_SWEEPING;
	fklGC_sweep(white);
	gc->running=FKL_GC_NONE;
	return NULL;
}

void fklFreeAllValues(FklVMgc* gc)
{
	FklVMvalue** phead=&gc->head;
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
			gc->num-=1;
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

FklVM* fklNewThreadVM(FklVMproc* mainCode,FklVMgc* gc,FklVM* prev,FklVM* next)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	FklVMframe* t=fklNewVMframe(mainCode,NULL);
	t->localenv=fklNewVMvalueNoGC(FKL_TYPE_ENV,fklNewVMenv(mainCode->prevEnv,gc),gc);
	exe->frames=t;
	exe->mark=1;
	exe->chan=fklNewVMvalueNoGC(FKL_TYPE_CHAN,fklNewVMchanl(0),gc);
	exe->tstack=fklNewPtrStack(32,16);
	exe->stack=fklNewVMstack(0);
	exe->gc=gc;
	exe->callback=threadErrorCallBack;
	exe->nextCall=NULL;
	exe->nextCallBackUp=NULL;
	exe->nny=0;
//	exe->thrds=1;
	pthread_rwlock_init(&exe->rlock,NULL);
	pthread_mutex_init(&exe->prev_next_lock,NULL);
	exe->prev=prev;
	exe->next=next;
	if(prev)
	{
		pthread_mutex_lock(&exe->prev->prev_next_lock);
		exe->prev->next=exe;
		pthread_mutex_unlock(&exe->prev->prev_next_lock);
	}
	if(next)
	{
		pthread_mutex_lock(&exe->next->prev_next_lock);
		exe->next->prev=exe;
		pthread_mutex_unlock(&exe->next->prev_next_lock);
	}
	return exe;
}

FklVM* fklNewThreadCallableObjVM(FklVMframe* frame,FklVMgc* gc,FklVMvalue* nextCall,FklVM* prev,FklVM* next)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	FklVMframe* t=fklNewVMframe(NULL,NULL);
	t->cp=frame->cp;
	t->localenv=NULL;
	exe->frames=t;
	exe->mark=1;
	exe->chan=fklNewVMvalueNoGC(FKL_TYPE_CHAN,fklNewVMchanl(0),gc);
	exe->tstack=fklNewPtrStack(32,16);
	exe->stack=fklNewVMstack(0);
	exe->gc=gc;
	exe->callback=threadErrorCallBack;
	exe->nextCall=nextCall;
	exe->nextCallBackUp=NULL;
	exe->nny=0;
//	exe->thrds=1;
	pthread_rwlock_init(&exe->rlock,NULL);
	exe->next=next;
	exe->prev=prev;
	if(prev)
	{
		pthread_mutex_lock(&exe->prev->prev_next_lock);
		exe->prev->next=exe;
		pthread_mutex_unlock(&exe->prev->prev_next_lock);
	}
	if(next)
	{
		pthread_mutex_lock(&exe->next->prev_next_lock);
		exe->next->prev=exe;
		pthread_mutex_unlock(&exe->next->prev_next_lock);
	}
	return exe;
}


void fklFreeVMstack(FklVMstack* stack)
{
//	pthread_rwlock_destroy(&stack->lock);
	fklFreeUintStack(stack->bps);
	fklFreeUintStack(stack->tps);
	free(stack->values);
	free(stack);
}

//void fklFreeAllVMs(FklVM* node)
//{
//	for(FklVMnode** ph=&GlobVMs->h;*ph!=node;)
//	{
//		FklVMnode* cur=*ph;
//		*ph=cur->next;
//		if(cur->vm->mark)
//		{
//			fklDeleteCallChain(cur->vm);
//			fklFreeVMstack(cur->vm->stack);
//			fklFreePtrStack(cur->vm->tstack);
//		}
//		free(cur->vm);
//		free(cur);
//	}
//	pthread_rwlock_destroy(&GlobVMs->lock);
//}

void fklFreeAllVMs(FklVM* curVM)
{
	uint8_t* code=curVM->code;
	for(FklVM* prev=curVM->prev;prev;)
	{
		if(prev->mark)
		{
			fklDeleteCallChain(prev);
			fklFreeVMstack(prev->stack);
			fklFreePtrStack(prev->tstack);
		}
		FklVM* t=prev;
		prev=prev->prev;
		free(t);
	}
	curVM->mark=1;
	for(FklVM* cur=curVM;cur;)
	{
		if(cur->mark)
		{
			fklDeleteCallChain(cur);
			fklFreeVMstack(cur->stack);
			fklFreePtrStack(cur->tstack);
		}
		FklVM* t=cur;
		cur=cur->next;
		free(t);
	}
	free(code);
	//uint8_t* code=GlobVMs->h?GlobVMs->h->vm->code:NULL;
	//for(FklVMnode** ph=&GlobVMs->h;*ph;)
	//{
	//	FklVMnode* cur=*ph;
	//	*ph=cur->next;
	//	if(cur->vm->mark)
	//	{
	//		fklDeleteCallChain(cur->vm);
	//		fklFreeVMstack(cur->vm->stack);
	//		fklFreePtrStack(cur->vm->tstack);
	//	}
	//	free(cur->vm);
	//	free(cur);
	//}
	//free(code);
	//pthread_rwlock_destroy(&GlobVMs->lock);
}

void fklFreeVMgc(FklVMgc* gc)
{
	fklWaitGC(gc);
	fklFreeAllValues(gc);
	pthread_rwlock_destroy(&gc->greylock);
	pthread_rwlock_destroy(&gc->lock);
	free(gc);
}

void fklJoinAllThread(FklVM* cur)
{
//	pthread_t selfId=pthread_self();
//	for(FklVMnode** ph=&GlobVMs->h;*ph!=node;ph=&(*ph)->next)
//	{
//		FklVMnode* cur=*ph;
//		if(cur->vm->tid!=selfId)
//			pthread_join(cur->vm->tid,NULL);
//	}
}

//FklVMlist* fklGetGlobVMs(void)
//{
//	return GlobVMs;
//}
//
//void fklSetGlobVMs(FklVMlist* l)
//{
//	GlobVMs=l;
//}


void fklCancelAllThread()
{
//	pthread_t selfId=pthread_self();
//	for(FklVMnode** ph=&GlobVMs->h;*ph;ph=&(*ph)->next)
//	{
//		FklVMnode* cur=*ph;
//		if(cur->vm->tid!=selfId)
//		{
//			pthread_join(cur->vm->tid,NULL);
//			if(cur->vm->mark)
//			{
//				fklDeleteCallChain(cur->vm);
//				fklFreeVMstack(cur->vm->stack);
//				fklFreePtrStack(cur->vm->tstack);
//			}
//		}
//	}
}

void fklDeleteCallChain(FklVM* exe)
{
	while(exe->frames)
	{
		FklVMframe* cur=exe->frames;
		exe->frames=cur->prev;
		fklFreeVMframe(cur);
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
//	pthread_rwlock_wrlock(&stack->lock);
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
//	pthread_rwlock_unlock(&stack->lock);
	pthread_rwlock_wrlock(&vm->rlock);
	fklDeleteCallChain(vm);
	vm->frames=NULL;
	for(FklVMframe* cur=cc->curr;cur;cur=cur->prev)
	{
		FklVMframe* frame=fklNewVMframe(NULL,vm->frames);
		vm->frames=frame;
		frame->cp=cur->cp;
		frame->localenv=cur->localenv;
		frame->scp=cur->scp;
		frame->cpc=cur->cpc;
		frame->sid=cur->sid;
		frame->mark=cur->mark;
	}
	pthread_rwlock_unlock(&vm->rlock);
	while(!fklIsPtrStackEmpty(vm->tstack))
		fklFreeVMtryBlock(fklPopPtrStack(vm->tstack));
	for(i=0;i<cc->tnum;i++)
	{
		FklVMtryBlock* tmp=&cc->tb[i];
		FklVMtryBlock* cur=fklNewVMtryBlock(tmp->sid,tmp->tp,tmp->curFrame);
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

void fklGC_joinGCthread(FklVMgc* gc)
{
	pthread_join(GCthreadId,NULL);
	gc->running=FKL_GC_NONE;
}

inline void fklPushVMvalue(FklVMvalue* v,FklVMstack* s)
{
//	pthread_rwlock_wrlock(&s->lock);
	if(s->tp>=s->size)
	{
		s->values=(FklVMvalue**)realloc(s->values
				,sizeof(FklVMvalue*)*(s->size+64));
		FKL_ASSERT(s->values);
		s->size+=64;
	}
	s->values[s->tp]=v;
	s->tp+=1;
//	pthread_rwlock_unlock(&s->lock);
}

void fklFreeVMframes(FklVMframe* h)
{
	while(h)
	{
		FklVMframe* cur=h;
		h=h->prev;
		cur->localenv=NULL;
		free(cur);
	}
}
