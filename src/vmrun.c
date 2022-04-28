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
#else
#include<termios.h>
#include<dlfcn.h>
#endif
#include<unistd.h>
#include<time.h>
#include<setjmp.h>

static FklVMvalue* VMstdin=NULL;
static FklVMvalue* VMstdout=NULL;
static FklVMvalue* VMstderr=NULL;
static pthread_t GCthreadid;
static volatile int GCtimce;
static int VMargc=0;
static char** VMargv=NULL;

typedef struct Graylink
{
	volatile FklVMvalue* v;
	volatile struct Graylink* next;
}Graylink;

Graylink* newGraylink(volatile FklVMvalue* v,volatile struct Graylink* next)
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

pthread_mutex_t GCthreadLock=PTHREAD_MUTEX_INITIALIZER;


/*procedure invoke functions*/
void invokeNativeProcdure(FklVM* exe,FklVMproc* tmpProc,FklVMrunnable* runnable)
{
	FklVMrunnable* prevProc=fklHasSameProc(tmpProc->scp,exe->rstack);
	if(fklIsTheLastExpress(runnable,prevProc,exe)&&prevProc)
		prevProc->mark=1;
	else
	{
		pthread_mutex_lock(&exe->rlock);
		FklVMrunnable* tmpRunnable=fklNewVMrunnable(tmpProc);
		tmpRunnable->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(tmpProc->prevEnv),exe->heap);
		fklPushPtrStack(tmpRunnable,exe->rstack);
		pthread_mutex_unlock(&exe->rlock);
	}
}

void invokeContinuation(FklVM* exe,VMcontinuation* cc)
{
	fklCreateCallChainWithContinuation(exe,cc);
}

void invokeDlProc(FklVM* exe,FklVMdlproc* dlproc)
{
	dlproc->func(exe);
}

/*--------------------------*/

FklVMlist GlobVMs={0,NULL};
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
static void B_pop_car(FklVM*);
static void B_pop_cdr(FklVM*);
static void B_pop_ref(FklVM*);
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
	B_pop_car,
	B_pop_cdr,
	B_pop_ref,
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
};

FklVM* fklNewVM(FklByteCode* mainCode)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe,__func__);
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
	exe->mark=1;
	exe->chan=NULL;
	exe->stack=fklNewVMstack(0);
	exe->tstack=fklNewPtrStack(32,16);
	exe->heap=fklNewVMheap();
	exe->callback=NULL;
	pthread_mutex_init(&exe->rlock,NULL);
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
		FKL_ASSERT(!size||GlobVMs.VMs,__func__);
		GlobVMs.VMs[size]=exe;
		GlobVMs.num+=1;
		exe->VMid=size;
	}
	return exe;
}

FklVM* fklNewTmpVM(FklByteCode* mainCode)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe,__func__);
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
	exe->chan=NULL;
	exe->stack=fklNewVMstack(0);
	exe->tstack=fklNewPtrStack(32,16);
	exe->heap=fklNewVMheap();
	pthread_mutex_init(&exe->rlock,NULL);
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
extern void SYS_dlclose(ARGL);
extern void SYS_getdir(ARGL);
extern void SYS_fgetc(ARGL);
extern void SYS_fwrite(ARGL);
extern void SYS_to_str(ARGL);
extern void SYS_fgets(ARGL);

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
		SYS_dlclose,
		SYS_getdir,
		SYS_fgetc,
		SYS_fwrite,
		SYS_to_str,
		SYS_fgets,
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
	if(!setjmp(exe->buf))
	{
		f(exe);
		FklVMstack* stack=exe->stack;
		FklVMvalue* v=NULL;
		while((v=fklTopGet(stack)))
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
		if(currunnable->cp>=currunnable->cpc+currunnable->scp)
		{
			if(currunnable->mark)
			{
				currunnable->cp=currunnable->scp;
				currunnable->mark=0;
			}
			else
			{
				pthread_mutex_lock(&exe->rlock);
				FklVMrunnable* r=fklPopPtrStack(exe->rstack);
				free(r);
				pthread_mutex_unlock(&exe->rlock);
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

int fklRunForGenEnv(FklVM* exe)
{
	while(!fklIsPtrStackEmpty(exe->rstack))
	{
		FklVMrunnable* currunnable=fklTopPtrStack(exe->rstack);
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
				free(r);
				continue;
			}
		}
		uint64_t cp=currunnable->cp;
		if(setjmp(exe->buf)==1)
			return 255;
		ByteCodes[exe->code[cp]](exe);
	}
	return 0;
}

inline void fklGC_tryRun(FklVM* exe)
{
	if(exe->heap->running==0&&exe->heap->num>exe->heap->threshold&&!pthread_mutex_trylock(&GCthreadLock))
	{
		exe->heap->running=FKL_GC_RUNNING;
		pthread_create(&GCthreadid,NULL,fklGC_threadFunc,exe);
	}
}

inline void fklGC_tryJoin(FklVMheap* heap)
{
	if(heap->running==FKL_GC_DONE)
		fklGC_joinGCthread(heap);
}

inline void fklGC_wait(FklVMheap* h)
{
	if(h->running)
		fklGC_joinGCthread(h);
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
	fprintf(stderr,"cp=%ld stack->bp=%d\n%s\n",currunnable->cp-scp,stack->bp,fklGetOpcodeName((FklOpcode)(exe->code[currunnable->cp])));
	fklDBG_printVMenv(currunnable->localenv->u.env,stderr);
	putc('\n',stderr);
	fprintf(stderr,"Wrong byts code!\n");
	exit(EXIT_FAILURE);
}

void B_push_nil(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	fklPushVMvalue(FKL_VM_NIL,stack);
	runnable->cp+=sizeof(char);
}

void B_push_pair(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	fklNewVMvalueToStack(FKL_PAIR,fklNewVMpair(),stack,exe->heap);
	runnable->cp+=sizeof(char);
}

void B_push_i32(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	fklPushVMvalue(FKL_MAKE_VM_I32(fklGetI32FromByteCode(exe->code+runnable->cp+sizeof(char))),stack);
	runnable->cp+=sizeof(char)+sizeof(int32_t);
}

void B_push_i64(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	fklNewVMvalueToStack(FKL_I64,exe->code+r->cp+sizeof(char),stack,exe->heap);
	r->cp+=sizeof(char)+sizeof(int64_t);
}

void B_push_chr(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	fklPushVMvalue(FKL_MAKE_VM_CHR(*(char*)(exe->code+runnable->cp+1)),stack);
	runnable->cp+=sizeof(char)+sizeof(char);
}

void B_push_f64(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	fklNewVMvalueToStack(FKL_F64,exe->code+runnable->cp+sizeof(char),stack,exe->heap);
	runnable->cp+=sizeof(char)+sizeof(double);
}

void B_push_str(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	uint64_t size=fklGetU64FromByteCode(exe->code+runnable->cp+sizeof(char));
	fklNewVMvalueToStack(FKL_STR,fklNewVMstr(size,(char*)exe->code+runnable->cp+sizeof(char)+sizeof(uint64_t)),stack,exe->heap);
	runnable->cp+=sizeof(char)+sizeof(uint64_t)+size;
}

void B_push_sym(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	fklPushVMvalue(FKL_MAKE_VM_SYM(fklGetSidFromByteCode(exe->code+runnable->cp+1)),stack);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_push_var(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+runnable->cp+sizeof(char));
	FklVMvalue* curEnv=runnable->localenv;
	FklVMenvNode* tmp=NULL;
	while(curEnv&&!tmp)
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
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
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
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	uint64_t sizeOfProc=fklGetU64FromByteCode(exe->code+runnable->cp+sizeof(char));
	FklVMproc* code=fklNewVMproc(runnable->cp+sizeof(char)+sizeof(uint64_t),sizeOfProc);
	code->prevEnv=runnable->localenv;
	fklNewVMvalueToStack(FKL_PROC,code,stack,exe->heap);
	runnable->cp+=sizeof(char)+sizeof(uint64_t)+sizeOfProc;
}

void B_pop(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	stack->tp-=1;
	runnable->cp+=sizeof(char);
}

void B_pop_var(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
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
		while(curEnv&&!tmp)
		{
			tmp=fklFindVMenvNode(idOfVar,curEnv->u.env);
			curEnv=curEnv->u.env->prev;
		}
		if(tmp==NULL)
			FKL_RAISE_BUILTIN_ERROR("b.pop_var",FKL_SYMUNDEFINE,runnable,exe);
		pValue=&tmp->value;
	}
	fklSetAndPop(curEnv,pValue,stack,exe->heap);
	if(FKL_IS_PROC(*pValue)&&(*pValue)->u.proc->sid==0)
		(*pValue)->u.proc->sid=idOfVar;
	if(FKL_IS_DLPROC(*pValue)&&(*pValue)->u.dlproc->sid==0)
		(*pValue)->u.dlproc->sid=idOfVar;
	runnable->cp+=sizeof(char)+sizeof(int32_t)+sizeof(FklSid_t);
}

void B_pop_arg(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	if(!(stack->tp>stack->bp))
		FKL_RAISE_BUILTIN_ERROR("b.pop_arg",FKL_TOOFEWARG,runnable,exe);
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+runnable->cp+sizeof(char));
	FklVMvalue* curEnv=runnable->localenv;
	FklVMvalue** pValue=NULL;
	FklVMenvNode* tmp=fklFindVMenvNode(idOfVar,curEnv->u.env);
	if(!tmp)
		tmp=fklAddVMenvNode(fklNewVMenvNode(FKL_VM_NIL,idOfVar),curEnv->u.env);
	pValue=&tmp->value;
	fklSetAndPop(curEnv,pValue,stack,exe->heap);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_pop_rest_arg(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
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
		tmp->u.pair->car=fklNiGetArg(&ap,stack);
		if(ap>stack->bp)
			tmp->u.pair->cdr=fklNiNewVMvalue(FKL_PAIR,fklNewVMpair(),stack,heap);
		else
			break;
		tmp=tmp->u.pair->cdr;
	}
	fklNiEnd(&ap,stack);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_pop_car(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* topValue=fklNiGetArg(&ap,stack);
	FklVMvalue* objValue=fklNiGetArg(&ap,stack);
	if(!FKL_IS_PAIR(objValue))
		FKL_RAISE_BUILTIN_ERROR("b.pop_car",FKL_WRONGARG,runnable,exe);
	if(exe->heap->running==FKL_GC_RUNNING&&FKL_IS_PTR(topValue)&&topValue->mark==FKL_MARK_W)
		fklGC_toGray(topValue,exe->heap);
	objValue->u.pair->car=topValue;
	fklNiReturn(objValue,&ap,stack);
	fklNiEnd(&ap,stack);
	runnable->cp+=sizeof(char);
}

void B_pop_cdr(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* topValue=fklNiGetArg(&ap,stack);
	FklVMvalue* objValue=fklNiGetArg(&ap,stack);
	if(!FKL_IS_PAIR(objValue))
		FKL_RAISE_BUILTIN_ERROR("b.pop_cdr",FKL_WRONGARG,runnable,exe);
	objValue->u.pair->cdr=topValue;
	fklNiReturn(objValue,&ap,stack);
	fklNiEnd(&ap,stack);
	runnable->cp+=sizeof(char);
}

void B_pop_ref(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	FklVMvalue* ref=fklNiPopTop(&ap,stack);
	if(!FKL_IS_REF(ref)&&!FKL_IS_MREF(ref))
		FKL_RAISE_BUILTIN_ERROR("b.pop_ref",FKL_WRONGARG,runnable,exe);
	FklVMvalue* by=fklNiPopTop(&ap,stack);
	if(FKL_IS_REF(ref))
	{
		if(exe->heap->running==FKL_GC_RUNNING&&FKL_IS_PTR(val)&&val->mark==FKL_MARK_W)
			fklGC_toGray(val,exe->heap);
		if(fklSET_REF(ref,val))
			FKL_RAISE_BUILTIN_ERROR("b.pop_ref",FKL_INVALIDASSIGN,runnable,exe);
	}
	else
	{
		if(!FKL_IS_CHR(val)&&!FKL_IS_I32(val)&&!FKL_IS_I64(val))
			FKL_RAISE_BUILTIN_ERROR("b.pop_ref",FKL_INVALIDASSIGN,runnable,exe);
		*(char*)by=FKL_IS_CHR(val)?FKL_GET_CHR(val):fklGetInt(val);
	}
	fklNiReturn(val,&ap,stack);
	fklNiEnd(&ap,stack);
	runnable->cp+=sizeof(char);
}

void B_set_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
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
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	fklPushVMvalue(FKL_MAKE_VM_I32(stack->bp),stack);
	stack->bp=stack->tp;
	runnable->cp+=sizeof(char);
}

void B_res_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	stack->tp=(stack->tptp)?stack->tpst[stack->tptp-1]:0;
	fklStackRecycle(exe);
	runnable->cp+=sizeof(char);
}

void B_pop_tp(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
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
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	if(stack->tp>stack->bp)
		FKL_RAISE_BUILTIN_ERROR("b.res_bp",FKL_TOOMANYARG,runnable,exe);
	FklVMvalue* prevBp=fklGetTopValue(stack);
	stack->bp=FKL_GET_I32(prevBp);
	stack->tp-=1;
	fklStackRecycle(exe);
	runnable->cp+=sizeof(char);
}

void B_invoke(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* tmpValue=fklNiGetArg(&ap,stack);
	if(exe->heap->running)
		fklGC_toGray(tmpValue,exe->heap);
	if(!FKL_IS_PTR(tmpValue)||(tmpValue->type!=FKL_PROC&&tmpValue->type!=FKL_CONT&&tmpValue->type!=FKL_DLPROC))
		FKL_RAISE_BUILTIN_ERROR("b.invoke",FKL_INVOKEERROR,runnable,exe);
	runnable->cp+=sizeof(char);
	fklNiEnd(&ap,stack);
	switch(tmpValue->type)
	{
		case FKL_PROC:
			invokeNativeProcdure(exe,tmpValue->u.proc,runnable);
			break;
		case FKL_CONT:
			invokeContinuation(exe,tmpValue->u.cont);
			break;
		case FKL_DLPROC:
			invokeDlProc(exe,tmpValue->u.dlproc);
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
		runnable->cp+=fklGetI64FromByteCode(exe->code+runnable->cp+sizeof(char));
	runnable->cp+=sizeof(char)+sizeof(int64_t);
}

void B_jmp_if_false(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* tmpValue=fklGET_VAL(fklGetTopValue(stack),exe->heap);
	if(tmpValue==FKL_VM_NIL)
		runnable->cp+=fklGetI64FromByteCode(exe->code+runnable->cp+sizeof(char));
	runnable->cp+=sizeof(char)+sizeof(int64_t);
}

void B_jmp(FklVM* exe)
{
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	runnable->cp+=fklGetI64FromByteCode(exe->code+runnable->cp+sizeof(char))+sizeof(char)+sizeof(int64_t);
}

void B_append(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(sec!=FKL_VM_NIL&&!FKL_IS_PAIR(sec))
		FKL_RAISE_BUILTIN_ERROR("b.append",FKL_WRONGARG,runnable,exe);
	if(!FKL_IS_PAIR(sec))
		FKL_RAISE_BUILTIN_ERROR("b.append",FKL_WRONGARG,runnable,exe);
	FklVMvalue** lastpair=&sec;
	while(FKL_IS_PAIR(*lastpair))lastpair=&(*lastpair)->u.pair->cdr;
	*lastpair=fir;
	fklNiReturn(sec,&ap,stack);
	fklNiEnd(&ap,stack);
	runnable->cp+=sizeof(char);
}

void B_push_try(FklVM* exe)
{
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	int32_t cpc=0;
	FklSid_t sid=fklGetSidFromByteCode(exe->code+r->cp+(++cpc));
	FklVMtryBlock* tb=fklNewVMtryBlock(sid,exe->stack->tp,exe->rstack->top);
	cpc+=sizeof(FklSid_t);
	uint32_t handlerNum=fklGetU32FromByteCode(exe->code+r->cp+cpc);
	cpc+=sizeof(uint32_t);
	uint32_t i=0;
	for(;i<handlerNum;i++)
	{
		FklSid_t type=fklGetSidFromByteCode(exe->code+r->cp+cpc);
		cpc+=sizeof(FklSid_t);
		uint64_t pCpc=fklGetU64FromByteCode(exe->code+r->cp+cpc);
		cpc+=sizeof(uint64_t);
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
	r->cp+=sizeof(char);
}

void B_push_vector(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	uint64_t size=fklGetU64FromByteCode(exe->code+r->cp+sizeof(char));
	FklPtrStack* base=fklNewPtrStack(32,16);
	for(size_t i=0;i<size;i++)
		fklPushPtrStack(fklNiGetArg(&ap,stack),base);
	for(size_t i=0;i<base->top/2;i++)
	{
		void* t=base->base[i];
		base->base[i]=base->base[base->top-i-1];
		base->base[base->top-i-1]=t;
	}
	//fklNewVMvalueToStackWithoutLock(FKL_VECTOR
	fklNiReturn(fklNiNewVMvalue(FKL_VECTOR
				,fklNewVMvec(size,(FklVMvalue**)base->base)
				,stack
				,exe->heap)
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
	fklFreePtrStack(base);
	r->cp+=sizeof(char)+sizeof(uint64_t);
}

void B_push_r_env(FklVM* exe)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	r->localenv=fklNiNewVMvalue(FKL_ENV,fklNewVMenv(r->localenv),stack,exe->heap);
	fklNiEnd(&ap,stack);
	r->cp+=sizeof(char);
}

void B_pop_r_env(FklVM* exe)
{
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	r->localenv=r->localenv->u.env->prev;
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
	pthread_mutex_init(&tmp->lock,NULL);
	return tmp;
}

void fklStackRecycle(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* currunnable=fklTopPtrStack(exe->rstack);
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
			if(FKL_IS_REF(tmp))
			{
				tmp=*(FklVMvalue**)FKL_GET_PTR(tmp);
				i--;
			}
			else if(FKL_IS_MREF(tmp))
			{
				FklVMvalue* ptr=stack->values[--i];
				tmp=FKL_MAKE_VM_CHR(*(char*)ptr);
			}
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

		for(;i<size;i+=(code[i]==FKL_JMP)?fklGetI64FromByteCode(code+i+sizeof(char))+sizeof(char)+sizeof(int64_t):1)
			if(code[i]!=FKL_POP_TP
					&&code[i]!=FKL_POP_TRY
					&&code[i]!=FKL_JMP
					&&code[i]!=FKL_POP_R_ENV)
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
	FKL_ASSERT(tmp,__func__);
	tmp->running=0;
	tmp->num=0;
	tmp->threshold=FKL_THRESHOLD_SIZE;
	tmp->head=NULL;
	tmp->gray=NULL;
	tmp->white=NULL;
	pthread_mutex_init(&tmp->lock,NULL);
	pthread_mutex_init(&tmp->glock,NULL);
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
		root=FKL_GET_TAG(root)==FKL_REF_TAG?*((FklVMvalue**)FKL_GET_PTR(root)):root;
		if(FKL_IS_PTR(root)&&!root->mark)
		{
			root->mark=FKL_MARK_B;
			switch(root->type)
			{
				case FKL_PAIR:
					fklPushPtrStack(root->u.pair->car,stack);
					fklPushPtrStack(root->u.pair->cdr,stack);
					break;
				case FKL_PROC:
					fklPushPtrStack(root->u.proc->prevEnv,stack);
					break;
				case FKL_CONT:
					for(uint32_t i=0;i<root->u.cont->stack->tp;i++)
						fklPushPtrStack(root->u.cont->stack->values[i],stack);
					for(uint32_t i=0;i<root->u.cont->num;i++)
						fklPushPtrStack(root->u.cont->state[i].localenv,stack);
					break;
				case FKL_VECTOR:
					{
						FklVMvec* vec=root->u.vec;
						for(size_t i=0;i<vec->size;i++)
							fklPushPtrStack(vec->base[i],stack);
					}
					break;
				case FKL_CHAN:
					{
						pthread_mutex_lock(&root->u.chan->lock);
						FklQueueNode* head=root->u.chan->messages->head;
						for(;head;head=head->next)
							fklPushPtrStack(head->data,stack);
						for(head=root->u.chan->sendq->head;head;head=head->next)
							fklPushPtrStack(((FklVMsend*)head->data)->m,stack);
						pthread_mutex_unlock(&root->u.chan->lock);
					}
					break;
				case FKL_DLPROC:
					if(root->u.dlproc->dll)
						fklPushPtrStack(root->u.dlproc->dll,stack);
					break;
				case FKL_ENV:
					{
						FklVMenv* env=root->u.env;
						if(env->prev)
							fklPushPtrStack(env->prev,stack);
						for(uint32_t i=0;i<env->num;i++)
							fklPushPtrStack(env->list[i]->value,stack);
					}
					break;
				case FKL_I64:
				case FKL_F64:
				case FKL_FP:
					break;
				default:
					FKL_ASSERT(0,__func__);
					break;
			}
		}
	}
	fklFreePtrStack(stack);
}

void fklGC_toGray(FklVMvalue* v,FklVMheap* h)
{
	if(FKL_IS_PTR(v)&&v->mark==FKL_MARK_W)
	{
		v->mark=FKL_MARK_G;
		pthread_mutex_lock(&h->glock);
		h->gray=newGraylink(v,h->gray);
		pthread_mutex_unlock(&h->glock);
	}
}

void fklGC_markRootToGray(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMheap* heap=exe->heap;
	FklPtrStack* rstack=exe->rstack;
	pthread_mutex_lock(&exe->rlock);
	for(uint32_t i=0;i<rstack->top;i++)
	{
		FklVMrunnable* cur=rstack->base[i];
		FklVMvalue* value=cur->localenv;
		fklGC_toGray(value,heap);
	}
	pthread_mutex_unlock(&exe->rlock);
	pthread_mutex_lock(&stack->lock);
	for(uint32_t i=stack->tp;i>0;i--)
	{
		FklVMvalue* value=stack->values[i-1];
		if(FKL_IS_MREF(value))
			i--;
		else if(FKL_IS_PTR(value))
			fklGC_toGray(value,heap);
		else if(FKL_IS_REF(value))
			fklGC_toGray(FKL_GET_PTR(value),heap);
	}
	pthread_mutex_unlock(&stack->lock);
	if(exe->chan)
		fklGC_toGray(exe->chan,heap);
}

void fklGC_markGlobalRoot(void)
{
	for(uint32_t i=0;i<GlobVMs.num;i++)
	{
		if(GlobVMs.VMs[i])
		{
			if(GlobVMs.VMs[i]->mark)
				fklGC_markRootToGray(GlobVMs.VMs[i]);
			else
			{
				pthread_join(GlobVMs.VMs[i]->tid,NULL);
				free(GlobVMs.VMs[i]);
				GlobVMs.VMs[i]=NULL;
			}
		}
	}
}

void fklGC_pause(FklVM* exe)
{
	if(exe->VMid!=-1)
		fklGC_markGlobalRoot();
	else
		fklGC_markRootToGray(exe);
}

void propagateMark(volatile FklVMvalue* root,FklVMheap* heap)
{
	switch(root->type)
	{
		case FKL_PAIR:
			fklGC_toGray(root->u.pair->car,heap);
			fklGC_toGray(root->u.pair->cdr,heap);
			break;
		case FKL_PROC:
			fklGC_toGray(root->u.proc->prevEnv,heap);
			break;
		case FKL_CONT:
			for(uint32_t i=0;i<root->u.cont->stack->tp;i++)
				fklGC_toGray(root->u.cont->stack->values[i],heap);
			for(uint32_t i=0;i<root->u.cont->num;i++)
				fklGC_toGray(root->u.cont->state[i].localenv,heap);
			break;
		case FKL_VECTOR:
			{
				FklVMvec* vec=root->u.vec;
				for(size_t i=0;i<vec->size;i++)
					fklGC_toGray(vec->base[i],heap);
			}
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
				pthread_mutex_lock(&env->mutex);
				if(env->prev)
					fklGC_toGray(env->prev,heap);
				for(uint32_t i=0;i<env->num;i++)
					fklGC_toGray(env->list[i]->value,heap);
				pthread_mutex_unlock(&env->mutex);
			}
			break;
		case FKL_I64:
		case FKL_F64:
		case FKL_FP:
		case FKL_DLL:
		case FKL_ERR:
		case FKL_STR:
		case FKL_USERDATA:
			break;
		default:
			FKL_ASSERT(0,__func__);
			break;
	}
}

void fklGC_propagate(FklVM* exe)
{
	pthread_mutex_lock(&exe->heap->glock);
	volatile Graylink* g=exe->heap->gray;
	exe->heap->gray=g->next;
	pthread_mutex_unlock(&exe->heap->glock);
	volatile FklVMvalue* v=g->v;
	free((void*)g);
	if(FKL_IS_PTR(v)&&v->mark==FKL_MARK_G)
	{
		v->mark=FKL_MARK_B;
		propagateMark(v,exe->heap);
	}
}

void fklGC_collect(FklVM* exe)
{
	pthread_mutex_lock(&exe->heap->lock);
	volatile FklVMvalue* head=exe->heap->head;
	exe->heap->head=NULL;
	pthread_mutex_unlock(&exe->heap->lock);
	volatile FklVMvalue* volatile* phead=&head;
	while(*phead)
	{
		volatile FklVMvalue* cur=*phead;
		if(cur->mark==FKL_MARK_W)
		{
			*phead=cur->next;
			cur->next=exe->heap->white;
			exe->heap->white=cur;
		}
		else
		{
			cur->mark=FKL_MARK_W;
			phead=&cur->next;
		}
	}
	pthread_mutex_lock(&exe->heap->lock);
	*phead=exe->heap->head;
	exe->heap->head=head;
	pthread_mutex_unlock(&exe->heap->lock);
}

void fklGC_sweepW(FklVM* exe)
{
	volatile FklVMvalue* head=exe->heap->white;
	exe->heap->white=NULL;
	uint32_t count=0;
	volatile FklVMvalue* volatile* phead=&head;
	while(*phead)
	{
		volatile FklVMvalue* cur=*phead;
		if(cur->mark==FKL_MARK_W)
		{
			*phead=cur->next;
			fprintf(stderr,"free %p\n",cur);
			fklFreeVMvalue(cur);
			count++;
		}
		else
			phead=&cur->next;
	}
	pthread_mutex_lock(&exe->heap->lock);
	//fprintf(stderr,"%d:sweep count:%d\n",GCtimce,count);
	*phead=exe->heap->head;
	exe->heap->head=head;
	exe->heap->num-=count;
	pthread_mutex_unlock(&exe->heap->lock);
}

void* fklGC_threadFunc(void* arg)
{
	GCtimce++;
	FklVM* exe=arg;
	if(exe->VMid!=-1)
		fklGC_markGlobalRoot();
	else
		fklGC_markRootToGray(exe);
	for(;;)
	{
		if(!exe->heap->gray)
			break;
		fklGC_propagate(exe);
	}
	fklGC_collect(exe);
	fklGC_sweepW(exe);
	exe->heap->threshold=exe->heap->num+FKL_THRESHOLD_SIZE;
	exe->heap->running=FKL_GC_DONE;
	return NULL;
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
		FklVMvalue* curEnv=cur->localenv;
		fklGC_markValue(curEnv);
	}
}

void fklGC_markValueInStack(FklVMstack* stack)
{
	for(uint32_t i=stack->tp;i>0;i--)
	{
		FklVMvalue* t=stack->values[i-1];
		if(FKL_IS_REF(t)||FKL_IS_MREF(t))
			i--;
		else
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

void fklFreeVMvalue(volatile FklVMvalue* cur)
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
		case FKL_F64:
		case FKL_I64:
			break;
		case FKL_ENV:
			fklFreeVMenv(cur->u.env);
			break;
		default:
			FKL_ASSERT(0,__func__);
			break;
	}
	free((void*)cur);
}

void fklGC_sweep(FklVMheap* heap)
{
	volatile FklVMvalue* volatile* phead=&heap->head;
	while(*phead!=NULL)
	{
		volatile FklVMvalue* cur=*phead;
		if(FKL_IS_PTR(cur)&&cur->mark==FKL_MARK_W)
		{
			*phead=cur->next;
			fklFreeVMvalue(cur);
			heap->num-=1;
		}
		else
		{
			cur->mark=FKL_MARK_W;
			phead=&cur->next;
		}
	}
}

FklVM* fklNewThreadVM(FklVMproc* mainCode,FklVMheap* heap)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe,__func__);
	FklVMrunnable* t=fklNewVMrunnable(mainCode);
	t->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(mainCode->prevEnv),heap);
	exe->rstack=fklNewPtrStack(32,16);
	fklPushPtrStack(t,exe->rstack);
	exe->mark=1;
	exe->chan=fklNewVMvalue(FKL_CHAN,fklNewVMchanl(0),heap);
	exe->tstack=fklNewPtrStack(32,16);
	exe->stack=fklNewVMstack(0);
	exe->heap=heap;
	exe->callback=threadErrorCallBack;
	pthread_mutex_init(&exe->rlock,NULL);
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
		FKL_ASSERT(!size||GlobVMs.VMs,__func__);
		GlobVMs.VMs[size]=exe;
		GlobVMs.num+=1;
		exe->VMid=size;
	}
	return exe;
}

FklVM* fklNewThreadDlprocVM(FklVMrunnable* r,FklVMheap* heap)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe,__func__);
	FklVMrunnable* t=fklNewVMrunnable(NULL);
	t->cp=r->cp;
	t->localenv=NULL;
	exe->rstack=fklNewPtrStack(32,16);
	fklPushPtrStack(t,exe->rstack);
	exe->mark=1;
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
		FKL_ASSERT(!size||GlobVMs.VMs,__func__);
		GlobVMs.VMs[size]=exe;
		GlobVMs.num+=1;
		exe->VMid=size;
	}
	return exe;
}


void fklFreeVMstack(FklVMstack* stack)
{
	pthread_mutex_destroy(&stack->lock);
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
	fklGC_wait(h);
	fklGC_sweep(h);
	pthread_mutex_destroy(&h->glock);
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
			FKL_ASSERT(tmpStack->values,__func__);
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
		FKL_ASSERT(cur,__func__);
		cur->cp=cc->state[i].cp;
		cur->localenv=cc->state[i].localenv;
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
	pthread_mutex_unlock(&GCthreadLock);
	h->running=0;
}

inline void fklPushVMvalue(FklVMvalue* v,FklVMstack* s)
{
	pthread_mutex_lock(&s->lock);
	if(s->tp>=s->size)
	{
		s->values=(FklVMvalue**)realloc(s->values
				,sizeof(FklVMvalue*)*(s->size+64));
		FKL_ASSERT(s->values,__func__);
		s->size+=64;
	}
	s->values[s->tp]=v;
	s->tp+=1;
	pthread_mutex_unlock(&s->lock);
}
