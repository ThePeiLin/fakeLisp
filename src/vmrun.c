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

static FklVMvalue* VMstdin=NULL;
static FklVMvalue* VMstdout=NULL;
static FklVMvalue* VMstderr=NULL;
static pthread_t GCthreadid;
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
	FKL_ASSERT(g,__func__);
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

static int envNodeCmp(const void* a,const void* b)
{
	return ((*(FklVMenvNode**)a)->id-(*(FklVMenvNode**)b)->id);
}


/*procedure invoke functions*/
void invokeNativeProcdure(FklVM* exe,FklVMproc* tmpProc,FklVMrunnable* runnable)
{
	pthread_rwlock_wrlock(&exe->rlock);
	FklVMrunnable* tmpRunnable=fklNewVMrunnable(tmpProc,exe->rhead);
	tmpRunnable->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(tmpProc->prevEnv),exe->heap);
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
		tmpRunnable->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(tmpProc->prevEnv),exe->heap);
		exe->rhead=tmpRunnable;
		pthread_rwlock_unlock(&exe->rlock);
	}
}

void tailInvokeNativeProcdure(FklVM* exe,FklVMproc* proc,FklVMrunnable* runnable)
{
	if(runnable->scp==proc->scp)
		runnable->mark=1;
	else
	{
		pthread_rwlock_wrlock(&exe->rlock);
		FklVMrunnable* tmpRunnable=fklNewVMrunnable(proc,exe->rhead->prev);
		tmpRunnable->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(proc->prevEnv),exe->heap);
		exe->rhead->prev=tmpRunnable;
		pthread_rwlock_unlock(&exe->rlock);
	}
}

void invokeContinuation(FklVM* exe,FklVMcontinuation* cc)
{
	fklCreateCallChainWithContinuation(exe,cc);
}

void invokeDlProc(FklVM* exe,FklVMdlproc* dlproc)
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
//static void B_pop_car(FklVM*);
//static void B_pop_cdr(FklVM*);
//static void B_pop_ref(FklVM*);
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
static void B_push_vector(FklVM*);
static void B_push_r_env(FklVM*);
static void B_pop_r_env(FklVM*);
static void B_tail_invoke(FklVM*);
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
//	B_pop_car,
//	B_pop_cdr,
//	B_pop_ref,
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
	B_push_vector,
	B_push_r_env,
	B_pop_r_env,
	B_tail_invoke,
	B_push_big_int,
	B_push_box,
};

FklVM* fklNewVM(FklByteCode* mainCode)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe,__func__);
	FklVMproc* tmpVMproc=NULL;
	exe->code=NULL;
	exe->size=0;
	exe->rhead=NULL;
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
		FKL_ASSERT(!size||GlobVMs.VMs,__func__);
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
	FKL_ASSERT(exe,__func__);
	exe->code=NULL;
	exe->size=0;
	exe->rhead=NULL;
	if(mainCode!=NULL)
	{
		exe->code=fklCopyMemory(mainCode->code,mainCode->size);
		exe->size=mainCode->size;
		exe->rhead=fklNewVMrunnable(fklNewVMproc(0,mainCode->size),exe->rhead);
	}
	exe->mark=1;
	exe->chan=NULL;
	exe->stack=fklNewVMstack(0);
	exe->tstack=fklNewPtrStack(32,16);
	exe->heap=fklNewVMheap();
	pthread_rwlock_init(&exe->rlock,NULL);
	exe->callback=threadErrorCallBack;
	exe->VMid=-1;
	return exe;
}

#define ARGL FklVM*
extern void SYS_car(ARGL);
extern void SYS_cdr(ARGL);
extern void SYS_cons(ARGL);
extern void SYS_append(ARGL);
extern void SYS_atom(ARGL);
extern void SYS_null(ARGL);
extern void SYS_not(ARGL);
extern void SYS_eq(ARGL);
extern void SYS_equal(ARGL);
extern void SYS_eqn(ARGL);
extern void SYS_add(ARGL);
extern void SYS_add_1(ARGL);
extern void SYS_sub(ARGL);
extern void SYS_sub_1(ARGL);
extern void SYS_mul(ARGL);
extern void SYS_div(ARGL);
extern void SYS_rem(ARGL);
extern void SYS_gt(ARGL);
extern void SYS_ge(ARGL);
extern void SYS_lt(ARGL);
extern void SYS_le(ARGL);
extern void SYS_char(ARGL);
extern void SYS_integer(ARGL);
extern void SYS_f64(ARGL);
extern void SYS_as_str(ARGL);
extern void SYS_symbol(ARGL);
extern void SYS_nth(ARGL);
extern void SYS_length(ARGL);
extern void SYS_fopen(ARGL);
extern void SYS_read(ARGL);
extern void SYS_prin1(ARGL);
extern void SYS_princ(ARGL);
extern void SYS_dlopen(ARGL);
extern void SYS_dlsym(ARGL);
extern void SYS_argv(ARGL);
extern void SYS_go(ARGL);
extern void SYS_chanl(ARGL);
extern void SYS_send(ARGL);
extern void SYS_recv(ARGL);
extern void SYS_error(ARGL);
extern void SYS_raise(ARGL);
extern void SYS_call_cc(ARGL);
extern void SYS_apply(ARGL);
extern void SYS_reverse(ARGL);
extern void SYS_i32(ARGL);
extern void SYS_i64(ARGL);
extern void SYS_fclose(ARGL);
extern void SYS_feof(ARGL);
extern void SYS_vref(ARGL);
extern void SYS_nthcdr(ARGL);
extern void SYS_char_p(ARGL);
extern void SYS_integer_p(ARGL);
extern void SYS_i32_p(ARGL);
extern void SYS_i64_p(ARGL);
extern void SYS_f64_p(ARGL);
extern void SYS_pair_p(ARGL);
extern void SYS_symbol_p(ARGL);
extern void SYS_string_p(ARGL);
extern void SYS_error_p(ARGL);
extern void SYS_procedure_p(ARGL);
extern void SYS_proc_p(ARGL);
extern void SYS_dlproc_p(ARGL);
extern void SYS_vector_p(ARGL);
extern void SYS_chanl_p(ARGL);
extern void SYS_dll_p(ARGL);
extern void SYS_vector(ARGL);
extern void SYS_getdir(ARGL);
extern void SYS_fgetc(ARGL);
extern void SYS_fwrite(ARGL);
extern void SYS_to_str(ARGL);
extern void SYS_fgets(ARGL);
extern void SYS_to_int(ARGL);
extern void SYS_to_f64(ARGL);
extern void SYS_big_int_p(ARGL);
extern void SYS_big_int(ARGL);
extern void SYS_set_car(ARGL);
extern void SYS_set_cdr(ARGL);
extern void SYS_set_nth(ARGL);
extern void SYS_set_nthcdr(ARGL);
extern void SYS_set_vref(ARGL);
extern void SYS_list_p(ARGL);
extern void SYS_box(ARGL);
extern void SYS_unbox(ARGL);
extern void SYS_set_box(ARGL);
extern void SYS_box_cas(ARGL);
extern void SYS_box_p(ARGL);
extern void SYS_fix_int_p(ARGL);

#undef ARGL

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
		SYS_char,
		SYS_integer,
		SYS_f64,
		SYS_as_str,
		SYS_symbol,
		SYS_nth,
		SYS_length,
		SYS_apply,
		SYS_call_cc,
		SYS_fopen,
		SYS_read,
		SYS_prin1,
		SYS_princ,
		SYS_dlopen,
		SYS_dlsym,
		SYS_argv,
		SYS_go,
		SYS_chanl,
		SYS_send,
		SYS_recv,
		SYS_error,
		SYS_raise,
		SYS_reverse,
		SYS_i32,
		SYS_i64,
		SYS_fclose,
		SYS_feof,
		SYS_vref,
		SYS_nthcdr,
		SYS_char_p,
		SYS_integer_p,
		SYS_i32_p,
		SYS_i64_p,
		SYS_f64_p,
		SYS_pair_p,
		SYS_symbol_p,
		SYS_string_p,
		SYS_error_p,
		SYS_procedure_p,
		SYS_proc_p,
		SYS_dlproc_p,
		SYS_vector_p,
		SYS_chanl_p,
		SYS_dll_p,
		SYS_vector,
		SYS_getdir,
		SYS_fgetc,
		SYS_fwrite,
		SYS_to_str,
		SYS_fgets,
		SYS_to_int,
		SYS_to_f64,
		SYS_big_int_p,
		SYS_big_int,
		SYS_set_car,
		SYS_set_cdr,
		SYS_set_nth,
		SYS_set_nthcdr,
		SYS_set_vref,
		SYS_list_p,
		SYS_box,
		SYS_unbox,
		SYS_set_box,
		SYS_box_cas,
		SYS_box_p,
		SYS_fix_int_p,
	};
	obj->num=FKL_NUM_OF_BUILT_IN_SYMBOL;
	obj->list=(FklVMenvNode**)malloc(sizeof(FklVMenvNode*)*FKL_NUM_OF_BUILT_IN_SYMBOL);
	FKL_ASSERT(obj->list,__func__);
	obj->list[0]=fklNewVMenvNode(FKL_VM_NIL,fklAddSymbolToGlob(fklGetBuiltInSymbol(0))->id);
	VMstdin=fklNewVMvalue(FKL_FP,fklNewVMfp(stdin),heap);
	VMstdout=fklNewVMvalue(FKL_FP,fklNewVMfp(stdout),heap);
	VMstderr=fklNewVMvalue(FKL_FP,fklNewVMfp(stderr),heap);
	obj->list[1]=fklNewVMenvNode(VMstdin,fklAddSymbolToGlob(fklGetBuiltInSymbol(1))->id);
	obj->list[2]=fklNewVMenvNode(VMstdout,fklAddSymbolToGlob(fklGetBuiltInSymbol(2))->id);
	obj->list[3]=fklNewVMenvNode(VMstderr,fklAddSymbolToGlob(fklGetBuiltInSymbol(3))->id);
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
		FKL_NI_BEGIN(exe);
		FklVMvalue* v=NULL;
		while((v=fklNiGetArg(&ap,stack)))
			fklChanlSend(fklNewVMsend(v),tmpCh);
		fklNiEnd(&ap,stack);
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
	fklFreePtrStack(exe->tstack);
	pthread_rwlock_wrlock(&exe->rlock);
	if(state!=0)
		fklDeleteCallChain(exe);
	exe->rhead=NULL;
	exe->mark=0;
	pthread_rwlock_unlock(&exe->rlock);
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
	if(!setjmp(exe->buf))
	{
		f(exe);
		FKL_NI_BEGIN(exe);
		FklVMvalue* v=NULL;
		while((v=fklNiGetArg(&ap,stack)))
			fklChanlSend(fklNewVMsend(v),ch);
		fklNiEnd(&ap,stack);
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
	fklFreeVMstack(exe->stack);
	exe->stack=NULL;
	exe->lnt=NULL;
	pthread_rwlock_wrlock(&exe->rlock);
	fklDeleteCallChain(exe);
	exe->rhead=NULL;
	fklFreePtrStack(exe->tstack);
	exe->mark=0;
	pthread_rwlock_unlock(&exe->rlock);
	return (void*)state;
}

void* ThreadVMinvokableUd(void* p)
{
	void** a=(void**)p;
	FklVM* exe=a[0];
	FklVMudata* ud=a[1];
	free(p);
	int64_t state=0;
	FklVMchanl* ch=exe->chan->u.chan;
	if(!setjmp(exe->buf))
	{
		ud->t->__invoke(exe,ud->data);
		FKL_NI_BEGIN(exe);
		FklVMvalue* v=NULL;
		while((v=fklNiGetArg(&ap,stack)))
			fklChanlSend(fklNewVMsend(v),ch);
		fklNiEnd(&ap,stack);
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
	fklFreeVMstack(exe->stack);
	exe->stack=NULL;
	exe->lnt=NULL;
	pthread_rwlock_wrlock(&exe->rlock);
	fklDeleteCallChain(exe);
	exe->rhead=NULL;
	fklFreePtrStack(exe->tstack);
	exe->mark=0;
	pthread_rwlock_unlock(&exe->rlock);
	return (void*)state;
}

int fklRunVM(FklVM* exe)
{
	while(exe->rhead)
	{
		FklVMrunnable* currunnable=exe->rhead;
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
		if(setjmp(exe->buf)==1)
			return 255;
		ByteCodes[exe->code[cp]](exe);
		fklGC_tryRun(exe);
		fklGC_tryJoin(exe->heap);
	}
	return 0;
}

int fklRunVMForRepl(FklVM* exe)
{
	while(exe->rhead)
	{
		FklVMrunnable* currunnable=exe->rhead;
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
				if(r->prev==NULL)
				{
					pthread_rwlock_unlock(&exe->rlock);
					break;
				}
				exe->rhead=r->prev;
				free(r);
				pthread_rwlock_unlock(&exe->rlock);
				continue;
			}
		}
		uint64_t cp=currunnable->cp;
		if(setjmp(exe->buf)==1)
			return 255;
		ByteCodes[exe->code[cp]](exe);
		fklGC_tryRun(exe);
		fklGC_tryJoin(exe->heap);
	}
	return 0;
}

inline void fklGC_tryRun(FklVM* exe)
{
	pthread_rwlock_rdlock(&exe->heap->lock);
	FklGCState running=exe->heap->running;
	int valueNumCmpresult=exe->heap->num>exe->heap->threshold;
	pthread_rwlock_unlock(&exe->heap->lock);
	if(valueNumCmpresult
			&&running==0
			&&!pthread_rwlock_trywrlock(&exe->heap->lock))
	{
		exe->heap->running=FKL_GC_RUNNING;
		pthread_rwlock_unlock(&exe->heap->lock);
		pthread_create(&GCthreadid,NULL,fklGC_threadFunc,exe);
	}
}

inline void fklGC_tryJoin(FklVMheap* heap)
{
	pthread_rwlock_rdlock(&heap->lock);
	FklGCState running=heap->running;
	pthread_rwlock_unlock(&heap->lock);
	if(running==FKL_GC_DONE&&!pthread_rwlock_trywrlock(&heap->lock))
	{
		fklGC_joinGCthread(heap);
		pthread_rwlock_unlock(&heap->lock);
	}
}

inline void fklGC_wait(FklVMheap* h)
{
	pthread_rwlock_rdlock(&h->lock);
	FklGCState running=h->running;
	pthread_rwlock_unlock(&h->lock);
	if(running)
		fklGC_joinGCthread(h);
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
	fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
	fprintf(stderr,"cp=%ld stack->bp=%d\n%s\n",currunnable->cp-scp,stack->bp,fklGetOpcodeName((FklOpcode)(exe->code[currunnable->cp])));
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
	FklVMpair* pair=fklNewVMpair();
	pair->cdr=fklNiGetArg(&ap,stack);
	pair->car=fklNiGetArg(&ap,stack);
	fklNiReturn(fklNewVMvalueToStack(FKL_PAIR,pair,stack,exe->heap),&ap,stack);
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
	fklNewVMvalueToStack(FKL_STR,fklNewVMstr(size,(char*)exe->code+runnable->cp+sizeof(char)+sizeof(uint64_t)),stack,exe->heap);
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
	FklVMenvNode* tmp=NULL;
	while(!tmp&&curEnv&&curEnv!=FKL_VM_NIL)
	{
		tmp=fklFindVMenvNode(idOfVar,curEnv->u.env);
		curEnv=curEnv->u.env->prev;
	}
	if(tmp==NULL)
		FKL_RAISE_BUILTIN_ERROR("b.push_var",FKL_SYMUNDEFINE,runnable,exe);
	fklPushVMvalue(tmp->value,stack);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_push_top(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	if(stack->tp==stack->bp)
		FKL_RAISE_BUILTIN_ERROR("b.push_top",FKL_STACKERROR,runnable,exe);
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
	FklVMvalue* vmcode=fklNewVMvalueToStack(FKL_PROC,code,stack,exe->heap);
	fklSetRef(vmcode,&code->prevEnv,runnable->localenv,exe->heap);
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
		FKL_RAISE_BUILTIN_ERROR("b.pop_var",FKL_STACKERROR,runnable,exe);
	int32_t scopeOfVar=fklGetI32FromByteCode(exe->code+runnable->cp+sizeof(char));
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+runnable->cp+sizeof(char)+sizeof(int32_t));
	FklVMvalue* curEnv=runnable->localenv;
	FklVMvalue** pValue=NULL;
	if(scopeOfVar>=0)
	{
		int32_t i=0;
		for(;i<scopeOfVar;i++)
			curEnv=curEnv->u.env->prev;
		FklVMenvNode* tmp=fklFindVMenvNode(idOfVar,curEnv->u.env);
		if(!tmp)
			tmp=fklAddVMenvNode(fklNewVMenvNode(FKL_VM_NIL,idOfVar),curEnv->u.env);
		pValue=&tmp->value;
	}
	else
	{
		FklVMenvNode* tmp=NULL;
		while(!tmp&&curEnv&&curEnv!=FKL_VM_NIL)
		{
			tmp=fklFindVMenvNode(idOfVar,curEnv->u.env);
			curEnv=curEnv->u.env->prev;
		}
		if(tmp==NULL)
			FKL_RAISE_BUILTIN_ERROR("b.pop_var",FKL_SYMUNDEFINE,runnable,exe);
		pValue=&tmp->value;
	}
	fklSetRef(curEnv,pValue,fklNiGetArg(&ap,stack),exe->heap);
	fklNiEnd(&ap,stack);
	if(FKL_IS_PROC(*pValue)&&(*pValue)->u.proc->sid==0)
		(*pValue)->u.proc->sid=idOfVar;
	if(FKL_IS_DLPROC(*pValue)&&(*pValue)->u.dlproc->sid==0)
		(*pValue)->u.dlproc->sid=idOfVar;
	runnable->cp+=sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t);
}

void B_pop_arg(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	if(!(stack->tp>stack->bp))
		FKL_RAISE_BUILTIN_ERROR("b.pop_arg",FKL_TOOFEWARG,runnable,exe);
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+runnable->cp+sizeof(char));
	FklVMvalue* curEnv=runnable->localenv;
	FklVMvalue** pValue=NULL;
	FklVMenvNode* tmp=fklFindVMenvNode(idOfVar,curEnv->u.env);
	if(!tmp)
		tmp=fklAddVMenvNode(fklNewVMenvNode(FKL_VM_NIL,idOfVar),curEnv->u.env);
	pValue=&tmp->value;
	fklSetRef(curEnv,pValue,fklNiGetArg(&ap,stack),exe->heap);
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
	FklVMvalue** pValue=NULL;
	FklVMenvNode* tmpNode=fklFindVMenvNode(idOfVar,curEnv->u.env);
	pValue=(tmpNode)?&tmpNode->value:NULL;
	FklVMvalue* obj=FKL_VM_NIL;
	FklVMvalue* tmp=NULL;
	if(ap>stack->bp)
		tmp=obj=fklNiNewVMvalue(FKL_PAIR,fklNewVMpair(),stack,exe->heap);
	if(!tmpNode)
		tmpNode=fklAddVMenvNode(fklNewVMenvNode(obj,idOfVar),curEnv->u.env);
	else
		*pValue=obj;
	if(exe->heap->running==FKL_GC_RUNNING&&FKL_IS_PTR(obj)&&obj->mark==FKL_MARK_W)
		fklGC_toGray(obj,exe->heap);
	while(ap>stack->bp)
	{
		fklSetRef(tmp,&tmp->u.pair->car,fklNiGetArg(&ap,stack),heap);
		if(ap>stack->bp)
			fklSetRef(tmp,&tmp->u.pair->cdr,fklNiNewVMvalue(FKL_PAIR,fklNewVMpair(),stack,heap),heap);
		else
			break;
		tmp=tmp->u.pair->cdr;
	}
	fklNiEnd(&ap,stack);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

//void B_pop_car(FklVM* exe)
//{
//	FKL_NI_BEGIN(exe);
//	FklVMrunnable* runnable=exe->rhead;
//	FklVMvalue* topValue=fklNiGetArg(&ap,stack);
//	FklVMvalue* objValue=fklNiGetArg(&ap,stack);
//	FKL_NI_CHECK_TYPE(objValue,FKL_IS_PAIR,"b.pop_car",runnable,exe);
//	fklSetRef(objValue,&objValue->u.pair->car,topValue,exe->heap);
//	fklNiReturn(objValue,&ap,stack);
//	fklNiEnd(&ap,stack);
//	runnable->cp+=sizeof(char);
//}
//
//void B_pop_cdr(FklVM* exe)
//{
//	FKL_NI_BEGIN(exe);
//	FklVMrunnable* runnable=exe->rhead;
//	FklVMvalue* topValue=fklNiGetArg(&ap,stack);
//	FklVMvalue* objValue=fklNiGetArg(&ap,stack);
//	FKL_NI_CHECK_TYPE(objValue,FKL_IS_PAIR,"b.pop_cdr",runnable,exe);
//	fklSetRef(objValue,&objValue->u.pair->cdr,topValue,exe->heap);
//	fklNiReturn(objValue,&ap,stack);
//	fklNiEnd(&ap,stack);
//	runnable->cp+=sizeof(char);
//}

void B_set_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	if(stack->tptp>=stack->tpsi)
	{
		stack->tpst=(uint32_t*)realloc(stack->tpst,sizeof(uint32_t)*(stack->tpsi+16));
		FKL_ASSERT(stack->tpst,__func__);
		stack->tpsi+=16;
	}
	stack->tpst[stack->tptp]=stack->tp;
	stack->tptp+=1;
	runnable->cp+=1;
}

void B_set_bp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	fklPushVMvalue(FKL_MAKE_VM_I32(stack->bp),stack);
	stack->bp=stack->tp;
	runnable->cp+=sizeof(char);
}

void B_res_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	stack->tp=(stack->tptp)?stack->tpst[stack->tptp-1]:0;
	fklStackRecycle(exe);
	runnable->cp+=sizeof(char);
}

void B_pop_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	stack->tptp-=1;
	if(stack->tpsi-stack->tptp>16)
	{
		stack->tpst=(uint32_t*)realloc(stack->tpst,sizeof(uint32_t)*(stack->tpsi-16));
		FKL_ASSERT(stack->tpst,__func__);
		stack->tpsi-=16;
	}
	runnable->cp+=sizeof(char);
}

void B_res_bp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	if(stack->tp>stack->bp)
		FKL_RAISE_BUILTIN_ERROR("b.res_bp",FKL_TOOMANYARG,runnable,exe);
	FklVMvalue* prevBp=fklGetTopValue(stack);
	stack->bp=FKL_GET_I32(prevBp);
	pthread_rwlock_wrlock(&stack->lock);
	stack->tp-=1;
	fklStackRecycle(exe);
	pthread_rwlock_unlock(&stack->lock);
	runnable->cp+=sizeof(char);
}

void B_invoke(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* tmpValue=fklNiGetArg(&ap,stack);
	if(!FKL_IS_PROC(tmpValue)&&!FKL_IS_DLPROC(tmpValue)&&!FKL_IS_CONT(tmpValue)&&!fklIsInvokableUd(tmpValue))
		FKL_RAISE_BUILTIN_ERROR("b.invoke",FKL_INVOKEERROR,runnable,exe);
	runnable->cp+=sizeof(char);
	switch(tmpValue->type)
	{
		case FKL_PROC:
			invokeNativeProcdure(exe,tmpValue->u.proc,runnable);
			fklNiEnd(&ap,stack);
			break;
		case FKL_CONT:
			fklNiEnd(&ap,stack);
			invokeContinuation(exe,tmpValue->u.cont);
			break;
		case FKL_DLPROC:
			fklNiEnd(&ap,stack);
			invokeDlProc(exe,tmpValue->u.dlproc);
			break;
		case FKL_USERDATA:
			fklNiEnd(&ap,stack);
			tmpValue->u.ud->t->__invoke(exe,tmpValue->u.ud->data);
			break;
		default:
			break;
	}
}

void B_tail_invoke(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* tmpValue=fklNiGetArg(&ap,stack);
	if(!FKL_IS_PROC(tmpValue)&&!FKL_IS_DLPROC(tmpValue)&&!FKL_IS_CONT(tmpValue)&&!fklIsInvokableUd(tmpValue))
		FKL_RAISE_BUILTIN_ERROR("b.invoke",FKL_INVOKEERROR,runnable,exe);
	runnable->cp+=sizeof(char);
	switch(tmpValue->type)
	{
		case FKL_PROC:
			tailInvokeNativeProcdure(exe,tmpValue->u.proc,runnable);
			fklNiEnd(&ap,stack);
			break;
		case FKL_CONT:
			fklNiEnd(&ap,stack);
			invokeContinuation(exe,tmpValue->u.cont);
			break;
		case FKL_DLPROC:
			fklNiEnd(&ap,stack);
			invokeDlProc(exe,tmpValue->u.dlproc);
			break;
		case FKL_USERDATA:
			fklNiEnd(&ap,stack);
			tmpValue->u.ud->t->__invoke(exe,tmpValue->u.ud->data);
			break;
		default:
			break;
	}
}

void B_jmp_if_true(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	//FklVMvalue* tmpValue=fklGET_VAL(fklGetTopValue(stack),exe->heap);
	FklVMvalue* tmpValue=fklGetTopValue(stack);
	if(tmpValue!=FKL_VM_NIL)
		runnable->cp+=fklGetI64FromByteCode(exe->code+runnable->cp+sizeof(char));
	runnable->cp+=sizeof(char)+sizeof(int64_t);
}

void B_jmp_if_false(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=exe->rhead;
	//FklVMvalue* tmpValue=fklGET_VAL(fklGetTopValue(stack),exe->heap);
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
		FKL_RAISE_BUILTIN_ERROR("b.append",FKL_WRONGARG,runnable,exe);
	if(sec==FKL_VM_NIL)
		fklNiReturn(fir,&ap,stack);
	else
	{
		FklVMvalue** lastcdr=&sec;
		FklVMvalue* lastpair=NULL;
		while(FKL_IS_PAIR(*lastcdr))
		{
			lastpair=*lastcdr;
			lastcdr=&(*lastcdr)->u.pair->cdr;
		}
		fklSetRef(lastpair,lastcdr,fir,exe->heap);
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
		FKL_ASSERT(typeIds,__func__);
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
	FklVMvec* vec=fklNewVMvec(size,NULL);
	for(size_t i=size;i>0;i--)
		vec->base[i-1]=fklNiGetArg(&ap,stack);
	fklNiReturn(fklNiNewVMvalue(FKL_VECTOR
				,vec
				,stack
				,exe->heap)
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
	r->cp+=sizeof(char)+sizeof(uint64_t);
}

void B_push_r_env(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	pthread_rwlock_wrlock(&exe->rlock);
	FklVMvalue* n=fklNiNewVMvalue(FKL_ENV,fklNewVMenv(r->localenv),stack,exe->heap);
	fklSetRef(n,&n->u.env->prev,r->localenv,exe->heap);
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
	fklNiReturn(fklNiNewVMvalue(FKL_BOX,c,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
	r->cp+=sizeof(char);
}

FklVMstack* fklNewVMstack(int32_t size)
{
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp,__func__);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->values=(FklVMvalue**)malloc(size*sizeof(FklVMvalue*));
	FKL_ASSERT(tmp->values,__func__);
	tmp->tpsi=0;
	tmp->tptp=0;
	tmp->tpst=NULL;
	pthread_rwlock_init(&tmp->lock,NULL);
	return tmp;
}

void fklStackRecycle(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* currunnable=exe->rhead;
	if(stack->size-stack->tp>64)
	{
		stack->values=(FklVMvalue**)realloc(stack->values,sizeof(FklVMvalue*)*(stack->size-64));
		FKL_ASSERT(stack->values,__func__);
		if(stack->values==NULL)
		{
			fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
			fprintf(stderr,"cp=%ld\n%s\n",currunnable->cp,fklGetOpcodeName((FklOpcode)(exe->code[currunnable->cp])));
			FKL_ASSERT(stack->values,__func__);
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
			//if(FKL_IS_REF(tmp))
			//{
			//	tmp=*(FklVMvalue**)FKL_GET_PTR(tmp);
			//	i--;
			//}
			//else if(FKL_IS_MREF(tmp))
			//{
			//	FklVMvalue* ptr=stack->values[--i];
			//	tmp=FKL_MAKE_VM_CHR(*(char*)ptr);
			//}
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
		uint32_t i=runnable->cp+(code[runnable->cp]==FKL_INVOKE||code[runnable->cp]==FKL_TAIL_INVOKE);
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

void fklDBG_printVMenv(FklVMenv* curEnv,FILE* fp)
{
	pthread_rwlock_rdlock(&curEnv->lock);
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
	pthread_rwlock_unlock(&curEnv->lock);
}

FklVMheap* fklNewVMheap()
{
	FklVMheap* tmp=(FklVMheap*)malloc(sizeof(FklVMheap));
	FKL_ASSERT(tmp,__func__);
	tmp->running=0;
	tmp->num=0;
	tmp->threshold=FKL_THRESHOLD_SIZE;
	tmp->head=NULL;
	tmp->gray=NULL;
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
		pthread_rwlock_unlock(&h->glock);
	}
}

void fklGC_markRootToGray(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMheap* heap=exe->heap;
	pthread_rwlock_rdlock(&exe->rlock);
	for(FklVMrunnable* cur=exe->rhead;cur;cur=cur->prev)
		fklGC_toGray(cur->localenv,heap);
	pthread_rwlock_unlock(&exe->rlock);
	pthread_rwlock_rdlock(&stack->lock);
	for(uint32_t i=stack->tp;i>0;i--)
	{
		FklVMvalue* value=stack->values[i-1];
		if(FKL_IS_PTR(value))
			fklGC_toGray(value,heap);
		//else if(FKL_IS_MREF(value))
		//	i--;
		//else if(FKL_IS_REF(value))
		//	fklGC_toGray(*(FklVMvalue**)FKL_GET_PTR(value),heap);
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
			{
				FklVMenv* env=root->u.env;
				pthread_rwlock_rdlock(&env->lock);
				if(env->prev)
					fklGC_toGray(env->prev,heap);
				for(uint32_t i=0;i<env->num;i++)
					fklGC_toGray(env->list[i]->value,heap);
				pthread_rwlock_unlock(&env->lock);
			}
			break;
		case FKL_USERDATA:
			if(root->u.ud->rel)
				fklGC_toGray(root->u.ud->rel,heap);
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
			FKL_ASSERT(0,__func__);
			break;
	}
}

void fklGC_propagate(FklVMheap* heap)
{
	pthread_rwlock_rdlock(&heap->glock);
	Graylink* g=heap->gray;
	heap->gray=g->next;
	pthread_rwlock_unlock(&heap->glock);
	FklVMvalue* v=g->v;
	free((void*)g);
	if(FKL_IS_PTR(v)&&v->mark==FKL_MARK_G)
	{
		v->mark=FKL_MARK_B;
		propagateMark(v,heap);
	}
}

void fklGC_collect(FklVMheap* heap)
{
	pthread_rwlock_wrlock(&heap->lock);
	FklVMvalue* head=heap->head;
	heap->head=NULL;
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

void fklGC_sweepW(FklVMheap* heap)
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
	pthread_rwlock_unlock(&heap->lock);
}

void* fklGC_threadFunc(void* arg)
{
	FklVM* exe=arg;
	FklVMheap* heap=exe->heap;
	if(exe->VMid!=-1)
		fklGC_markGlobalRoot();
	else
		fklGC_markRootToGray(exe);
	for(;;)
	{
		pthread_rwlock_rdlock(&heap->glock);
		Graylink* g=heap->gray;
		pthread_rwlock_unlock(&heap->glock);
		if(!g)
			break;
		fklGC_propagate(heap);
	}
	fklGC_collect(heap);
	fklGC_sweepW(heap);
	pthread_rwlock_wrlock(&heap->lock);
	heap->threshold=heap->num+FKL_THRESHOLD_SIZE;
	heap->running=FKL_GC_DONE;
	pthread_rwlock_unlock(&heap->lock);
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
			FKL_ASSERT(0,__func__);
			break;
	}
	free((void*)cur);
}

void fklGC_sweep(FklVMheap* heap)
{
	FklVMvalue** phead=&heap->head;
	FklVMvalue* freeDll=NULL;
	while(*phead!=NULL)
	{
		FklVMvalue* cur=*phead;
		if(FKL_IS_PTR(cur)&&cur->mark==FKL_MARK_W)
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
	FKL_ASSERT(exe,__func__);
	FklVMrunnable* t=fklNewVMrunnable(mainCode,NULL);
	t->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(mainCode->prevEnv),heap);
	exe->rhead=t;
	exe->mark=1;
	exe->chan=fklNewVMvalue(FKL_CHAN,fklNewVMchanl(0),heap);
	exe->tstack=fklNewPtrStack(32,16);
	exe->stack=fklNewVMstack(0);
	exe->heap=heap;
	exe->callback=threadErrorCallBack;
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
		FKL_ASSERT(!size||GlobVMs.VMs,__func__);
		GlobVMs.VMs[size]=exe;
		GlobVMs.num+=1;
		exe->VMid=size;
	}
	pthread_rwlock_unlock(&GlobVMsLock);
	return exe;
}

FklVM* fklNewThreadDlprocVM(FklVMrunnable* r,FklVMheap* heap)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe,__func__);
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
		FKL_ASSERT(!size||GlobVMs.VMs,__func__);
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
	free(stack->tpst);
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
	fklGC_wait(h);
	fklGC_sweep(h);
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
		free(cur);
	}
}

void fklCreateCallChainWithContinuation(FklVM* vm,FklVMcontinuation* cc)
{
	FklVMstack* stack=vm->stack;
	FklVMstack* tmpStack=fklCopyStack(cc->stack);
	int32_t i=stack->bp;
	for(;i<stack->tp;i++)
	{
		if(tmpStack->tp>=tmpStack->size)
		{
			tmpStack->values=(FklVMvalue**)realloc(tmpStack->values,sizeof(FklVMvalue*)*(tmpStack->size+64));
			FKL_ASSERT(tmpStack->values,__func__);
			tmpStack->size+=64;
		}
		tmpStack->values[tmpStack->tp]=stack->values[i];
		tmpStack->tp+=1;
	}
	pthread_rwlock_wrlock(&stack->lock);
	free(stack->values);
	free(stack->tpst);
	stack->values=tmpStack->values;
	stack->bp=tmpStack->bp;
	stack->size=tmpStack->size;
	stack->tp=tmpStack->tp;
	stack->tptp=tmpStack->tptp;
	stack->tpsi=tmpStack->tpsi;
	stack->tpst=tmpStack->tpst;
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

FklVMvalue* fklGetVMstdin(void)
{
	return VMstdin;
}

FklVMvalue* fklGetVMstdout(void)
{
	return VMstdout;
}

FklVMvalue* fklGetVMstderr(void)
{
	return VMstderr;
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
	void* result=NULL;
	pthread_join(GCthreadid,result);
	h->running=0;
}

inline void fklPushVMvalue(FklVMvalue* v,FklVMstack* s)
{
	pthread_rwlock_wrlock(&s->lock);
	if(s->tp>=s->size)
	{
		s->values=(FklVMvalue**)realloc(s->values
				,sizeof(FklVMvalue*)*(s->size+64));
		FKL_ASSERT(s->values,__func__);
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
