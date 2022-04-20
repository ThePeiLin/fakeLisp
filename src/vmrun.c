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

static FklVMvalue* VMstdin=NULL;
static FklVMvalue* VMstdout=NULL;
static FklVMvalue* VMstderr=NULL;

static int VMargc=0;
static char** VMargv=NULL;

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
//static void B_push_byts(FklVM*);
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
	//B_push_byts,
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
	exe->callback=threadErrorCallBack;
	exe->VMid=-1;
	return exe;
}

#define ARGL FklVM*,pthread_rwlock_t*
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
extern void SYS_string(ARGL);
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
		SYS_string,
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
		FklVMstack* stack=exe->stack;
		FklVMvalue* v=NULL;
		while((v=fklPopAndGetVMstack(stack)))
			fklChanlSend(fklNewVMsend(v),tmpCh,&GClock);
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
		fklChanlSend(t,tmpCh,&GClock);
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
		while((v=fklPopAndGetVMstack(stack)))
			fklChanlSend(fklNewVMsend(v),ch,&GClock);
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
		fklChanlSend(t,ch,&GClock);
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
	fprintf(stderr,"cp=%ld stack->bp=%d\n%s\n",currunnable->cp-scp,stack->bp,fklGetOpcodeName((FklOpcode)(exe->code[currunnable->cp])));
	fklDBG_printVMenv(currunnable->localenv,stderr);
	putc('\n',stderr);
	fprintf(stderr,"Wrong byts code!\n");
	exit(EXIT_FAILURE);
}

void B_push_nil(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FKL_SET_RETURN("B_push_nil",FKL_VM_NIL,stack);
	runnable->cp+=1;
}

void B_push_pair(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FKL_SET_RETURN("B_push_pair",fklNewVMvalue(FKL_PAIR,fklNewVMpair(),exe->heap),stack);
	runnable->cp+=1;

}

void B_push_i32(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FKL_SET_RETURN("B_push_i32",FKL_MAKE_VM_I32(fklGetI32FromByteCode(exe->code+runnable->cp+sizeof(char))),stack);
	runnable->cp+=5;
}

void B_push_i64(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FKL_SET_RETURN("B_push_i64",fklNewVMvalue(FKL_I64,exe->code+r->cp+sizeof(char),exe->heap),stack);
	r->cp+=sizeof(char)+sizeof(int64_t);
}

void B_push_chr(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FKL_SET_RETURN("B_push_chr",FKL_MAKE_VM_CHR(*(char*)(exe->code+runnable->cp+1)),stack);
	runnable->cp+=2;
}

void B_push_f64(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FKL_SET_RETURN("B_push_f64",fklNewVMvalue(FKL_F64,exe->code+runnable->cp+sizeof(char),exe->heap),stack);
	runnable->cp+=9;
}

void B_push_str(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	uint64_t size=fklGetU64FromByteCode(exe->code+runnable->cp+sizeof(char));
	FKL_SET_RETURN("B_push_str",fklNewVMvalue(FKL_STR,fklNewVMstr(size,(char*)exe->code+runnable->cp+sizeof(char)+sizeof(uint64_t)),exe->heap),stack);
	runnable->cp+=sizeof(char)+sizeof(uint64_t)+size;
}

void B_push_sym(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FKL_SET_RETURN("B_push_sym",FKL_MAKE_VM_SYM(fklGetSidFromByteCode(exe->code+runnable->cp+1)),stack);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

//void B_push_byts(FklVM* exe)
//{
//	FklVMstack* stack=exe->stack;
//	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
//	uint64_t size=fklGetU64FromByteCode(exe->code+runnable->cp+sizeof(char));
//	FKL_SET_RETURN("B_push_byts",fklNewVMvalue(FKL_BYTS,fklNewVMbyts(size,exe->code+runnable->cp+sizeof(char)+sizeof(uint64_t)),exe->heap),stack);
//	runnable->cp+=sizeof(char)+sizeof(uint64_t)+size;
//}

void B_push_var(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+runnable->cp+sizeof(char));
	FklVMenv* curEnv=runnable->localenv;
	FklVMenvNode* tmp=NULL;
	while(curEnv&&!tmp)
	{
		tmp=fklFindVMenvNode(idOfVar,curEnv);
		curEnv=curEnv->prev;
	}
	if(tmp==NULL)
		FKL_RAISE_BUILTIN_ERROR("b.push_var",FKL_SYMUNDEFINE,runnable,exe);
	FKL_SET_RETURN("B_push_var",tmp->value,stack);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_push_top(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	if(stack->tp==stack->bp)
		FKL_RAISE_BUILTIN_ERROR("b.push_top",FKL_STACKERROR,runnable,exe);
	FklVMvalue* val=fklPopAndGetVMstack(stack);
	FKL_SET_RETURN("B_push_top",val,stack);
	FKL_SET_RETURN("B_push_top",val,stack);
	runnable->cp+=sizeof(char);
}

void B_push_proc(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	uint64_t sizeOfProc=fklGetU64FromByteCode(exe->code+runnable->cp+sizeof(char));
	FklVMproc* code=fklNewVMproc(runnable->cp+sizeof(char)+sizeof(uint64_t),sizeOfProc);
	fklIncreaseVMenvRefcount(runnable->localenv);
	code->prevEnv=runnable->localenv;
	FklVMvalue* objValue=fklNewVMvalue(FKL_PROC,code,exe->heap);
	FKL_SET_RETURN("B_push_proc",objValue,stack);
	runnable->cp+=sizeof(char)+sizeof(uint64_t)+sizeOfProc;
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
	if(!(stack->tp>stack->bp))
		FKL_RAISE_BUILTIN_ERROR("b.pop_var",FKL_STACKERROR,runnable,exe);
	int32_t scopeOfVar=fklGetI32FromByteCode(exe->code+runnable->cp+sizeof(char));
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+runnable->cp+sizeof(char)+sizeof(int32_t));
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
			FKL_RAISE_BUILTIN_ERROR("b.pop_var",FKL_SYMUNDEFINE,runnable,exe);
		pValue=&tmp->value;
	}
	FklVMvalue* topValue=fklPopAndGetVMstack(stack);
	*pValue=topValue;
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
	FklVMenv* curEnv=runnable->localenv;
	FklVMvalue** pValue=NULL;
	FklVMenvNode* tmp=fklFindVMenvNode(idOfVar,curEnv);
	if(!tmp)
		tmp=fklAddVMenvNode(fklNewVMenvNode(NULL,idOfVar),curEnv);
	pValue=&tmp->value;
	FklVMvalue* topValue=fklPopAndGetVMstack(stack);
	*pValue=topValue;
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_pop_rest_arg(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklSid_t idOfVar=fklGetSidFromByteCode(exe->code+runnable->cp+sizeof(char));
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
		tmp->u.pair->car=fklPopAndGetVMstack(stack);
		if(stack->tp>stack->bp)
			tmp->u.pair->cdr=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),heap);
		else
			break;
		tmp=tmp->u.pair->cdr;
	}
	*pValue=obj;
	fklStackRecycle(exe);
	runnable->cp+=sizeof(char)+sizeof(FklSid_t);
}

void B_pop_car(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* topValue=fklPopAndGetVMstack(stack);
	FklVMvalue* objValue=fklPopAndGetVMstack(stack);
	if(!FKL_IS_PAIR(objValue))
		FKL_RAISE_BUILTIN_ERROR("b.pop_car",FKL_WRONGARG,runnable,exe);
	objValue->u.pair->car=topValue;
	FKL_SET_RETURN(__func__,objValue,stack);
	runnable->cp+=sizeof(char);
}

void B_pop_cdr(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* topValue=fklPopAndGetVMstack(stack);
	FklVMvalue* objValue=fklPopAndGetVMstack(stack);
	if(!FKL_IS_PAIR(objValue))
		FKL_RAISE_BUILTIN_ERROR("b.pop_cdr",FKL_WRONGARG,runnable,exe);
	objValue->u.pair->cdr=topValue;
	FKL_SET_RETURN(__func__,objValue,stack);
	runnable->cp+=1;
}

static inline int64_t getInt(FklVMvalue* p)
{
	return FKL_IS_I32(p)?FKL_GET_I32(p):p->u.i64;
}

void B_pop_ref(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* val=fklPopAndGetVMstack(stack);
	FklVMvalue* ref=fklPopVMstack(stack);
	if(!FKL_IS_REF(ref)&&!FKL_IS_MREF(ref))
		FKL_RAISE_BUILTIN_ERROR("b.pop_ref",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_REF(ref))
	{
		if(fklSET_REF(ref,val))
			FKL_RAISE_BUILTIN_ERROR("b.pop_ref",FKL_INVALIDASSIGN,runnable,exe);
	}
	else
	{
		void* ptr=fklPopVMstack(stack);
		if(!FKL_IS_CHR(val)&&!FKL_IS_I32(val)&&!FKL_IS_I64(val))
			FKL_RAISE_BUILTIN_ERROR("b.pop_ref",FKL_INVALIDASSIGN,runnable,exe);
		*(char*)ptr=FKL_IS_CHR(val)?FKL_GET_CHR(val):getInt(val);
	}
	FKL_SET_RETURN(__func__,val,stack);
	runnable->cp+=1;
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
	FKL_SET_RETURN("B_set_bp",FKL_MAKE_VM_I32(stack->bp),stack);
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
		FKL_ASSERT(stack->tpst,__func__);
		stack->tpsi-=16;
	}
	runnable->cp+=1;
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
	runnable->cp+=1;
}

void B_invoke(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* tmpValue=fklPopAndGetVMstack(stack);
	if(!FKL_IS_PTR(tmpValue)||(tmpValue->type!=FKL_PROC&&tmpValue->type!=FKL_CONT&&tmpValue->type!=FKL_DLPROC))
		FKL_RAISE_BUILTIN_ERROR("b.invoke",FKL_INVOKEERROR,runnable,exe);
	runnable->cp+=1;
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
	fklStackRecycle(exe);
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
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fir=fklPopAndGetVMstack(stack);
	FklVMvalue* sec=fklPopAndGetVMstack(stack);
	if(sec!=FKL_VM_NIL&&!FKL_IS_PAIR(sec))
		FKL_RAISE_BUILTIN_ERROR("b.append",FKL_WRONGARG,runnable,exe);
	if(!FKL_IS_PAIR(sec))
		FKL_RAISE_BUILTIN_ERROR("b.append",FKL_WRONGARG,runnable,exe);
	FklVMvalue** lastpair=&sec;
	while(FKL_IS_PAIR(*lastpair))lastpair=&(*lastpair)->u.pair->cdr;
	*lastpair=fir;
	FKL_SET_RETURN(__func__,sec,stack);
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
	r->cp+=1;
}

void B_push_vector(FklVM* exe)
{
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMstack* stack=exe->stack;
	uint64_t size=fklGetU64FromByteCode(exe->code+r->cp+sizeof(char));
	FklVMvalue** base=&stack->values[stack->tp-size];
	stack->tp-=size;
	FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_VECTOR
				,fklNewVMvec(size,base)
				,exe->heap)
			,stack);
	r->cp+=sizeof(char)+sizeof(uint64_t);
}

void B_push_r_env(FklVM* exe)
{
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMenv* prev=r->localenv;
	r->localenv=fklNewVMenv(prev);
	fklDecreaseVMenvRefcount(prev);
	r->cp+=sizeof(char);
}

void B_pop_r_env(FklVM* exe)
{
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMenv* p=r->localenv;
	r->localenv=r->localenv->prev;
	p->prev=NULL;
	fklFreeVMenv(p);
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
				tmp=*(FklVMvalue**)FKL_GET_PTR(tmp);
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
		root=FKL_GET_TAG(root)==FKL_REF_TAG?*((FklVMvalue**)FKL_GET_PTR(root)):root;
		if(FKL_GET_TAG(root)==FKL_PTR_TAG&&!root->mark)
		{
			root->mark=1;
			if(root->type==FKL_PAIR)
			{
				fklPushPtrStack(fklGetVMpairCar(root),stack);
				fklPushPtrStack(fklGetVMpairCdr(root),stack);
			}
			else if(root->type==FKL_PROC)
			{
				FklVMenv* curEnv=root->u.proc->prevEnv;
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
			else if(root->type==FKL_VECTOR)
			{
				FklVMvec* vec=root->u.vec;
				for(size_t i=0;i<vec->size;i++)
					fklPushPtrStack(vec->base[i],stack);
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
		if(!FKL_IS_MREF(t))
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
		if(FKL_GET_TAG(cur)==FKL_PTR_TAG&&!cur->mark)
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
				case FKL_PROC:
					fklFreeVMproc(prev->u.proc);
					break;
				//case FKL_BYTS:
				//	free(prev->u.byts);
				//	break;
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
				case FKL_ERR:
					fklFreeVMerror(prev->u.err);
					break;
				case FKL_VECTOR:
					fklFreeVMvec(prev->u.vec);
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
	FKL_ASSERT(exe,__func__);
	FklVMrunnable* t=fklNewVMrunnable(mainCode);
	t->localenv=fklNewVMenv(mainCode->prevEnv);
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

void fklReleaseGC(pthread_rwlock_t* pGClock)
{
	if(!pGClock)
		pGClock=&GClock;
	pthread_rwlock_unlock(pGClock);
}

void fklLockGC(pthread_rwlock_t* pGClock)
{
	if(!pGClock)
		pGClock=&GClock;
	pthread_rwlock_rdlock(pGClock);
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
