#define USE_CODE_NAME
#include"common.h"
#include"VMtool.h"
#include"reader.h"
#include"fakeVM.h"
#include"opcode.h"
#include"syscall.h"
#include"fakeFFI.h"
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

void threadErrorCallBack(void* a)
{
	void** e=(void**)a;
	int* i=(int*)a;
	FakeVM* exe=e[0];
	longjmp(exe->buf,i[(sizeof(void*)*2)/sizeof(int)]);
}

extern const char* builtInErrorType[NUMOFBUILTINERRORTYPE];
extern TypeId_t LastNativeTypeId;
extern TypeId_t StringTypeId;
extern TypeId_t FILEpTypeId;
static jmp_buf popRefBuf;
static int envNodeCmp(const void* a,const void* b)
{
	return ((*(VMenvNode**)a)->id-(*(VMenvNode**)b)->id);
}

extern const char* builtInSymbolList[NUMOFBUILTINSYMBOL];
pthread_rwlock_t GClock=PTHREAD_RWLOCK_INITIALIZER;

/*void ptr to VMvalue caster*/
#define ARGL uintptr_t p,VMheap* heap
#define CAST_TO_IN32 return MAKE_VM_IN32(p);
#define CAST_TO_IN64 return newVMvalue(IN64,&p,heap);
VMvalue* castShortVp    (ARGL){CAST_TO_IN32}
VMvalue* castIntVp      (ARGL){CAST_TO_IN32}
VMvalue* castUShortVp   (ARGL){CAST_TO_IN32}
VMvalue* castUintVp     (ARGL){CAST_TO_IN32}
VMvalue* castLongVp     (ARGL){CAST_TO_IN64}
VMvalue* castULongVp    (ARGL){CAST_TO_IN64}
VMvalue* castLLongVp    (ARGL){CAST_TO_IN64}
VMvalue* castULLongVp   (ARGL){CAST_TO_IN64}
VMvalue* castPtrdiff_tVp(ARGL){CAST_TO_IN64}
VMvalue* castSize_tVp   (ARGL){CAST_TO_IN64}
VMvalue* castSsize_tVp  (ARGL){CAST_TO_IN64}
VMvalue* castCharVp     (ARGL){return MAKE_VM_CHR(p);}
VMvalue* castWchar_tVp  (ARGL){CAST_TO_IN32}
VMvalue* castFloatVp    (ARGL){return newVMvalue(DBL,&p,heap);}
VMvalue* castDoubleVp   (ARGL){return newVMvalue(DBL,&p,heap);}
VMvalue* castInt8_tVp   (ARGL){CAST_TO_IN32}
VMvalue* castUint8_tVp  (ARGL){CAST_TO_IN32}
VMvalue* castInt16_tVp  (ARGL){CAST_TO_IN32}
VMvalue* castUint16_tVp (ARGL){CAST_TO_IN32}
VMvalue* castInt32_t    (ARGL){CAST_TO_IN32}
VMvalue* castUint32_tVp (ARGL){CAST_TO_IN32}
VMvalue* castInt64_tVp  (ARGL){CAST_TO_IN64}
VMvalue* castUint64_tVp (ARGL){CAST_TO_IN64}
VMvalue* castIptrVp     (ARGL){CAST_TO_IN64}
VMvalue* castUptrVp     (ARGL){CAST_TO_IN64}
VMvalue* castVptrVp     (ARGL){return newVMvalue(MEM,newVMMem(0,(uint8_t*)p),heap);}
#undef CAST_TO_IN32
#undef CAST_TO_IN64
static VMvalue* (*castVptrToVMvalueFunctionsList[])(ARGL)=
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
void invokeNativeProcdure(FakeVM* exe,VMproc* tmpProc,VMrunnable* runnable)
{
	VMrunnable* prevProc=hasSameProc(tmpProc->scp,exe->rstack);
	if(isTheLastExpress(runnable,prevProc,exe)&&prevProc)
		prevProc->mark=1;
	else
	{
		VMrunnable* tmpRunnable=newVMrunnable(tmpProc);
		tmpRunnable->localenv=newVMenv(tmpProc->prevEnv);
		pushComStack(tmpRunnable,exe->rstack);
	}
}

void invokeContinuation(FakeVM* exe,VMcontinuation* cc)
{
	createCallChainWithContinuation(exe,cc);
}

void invokeDlProc(FakeVM* exe,VMDlproc* dlproc)
{
	DllFunc dllfunc=dlproc->func;
	dllfunc(exe,&GClock);
}

void invokeFlproc(FakeVM* exe,VMFlproc* flproc)
{
	TypeId_t type=flproc->type;
	VMstack* stack=exe->stack;
	VMrunnable* curR=topComStack(exe->rstack);
	void* func=flproc->func;
	VMFuncType* ft=(VMFuncType*)GET_TYPES_PTR(getVMTypeUnion(type).all);
	uint32_t anum=ft->anum;
	TypeId_t* atypes=ft->atypes;
	ffi_type** ffiAtypes=(ffi_type**)malloc(sizeof(ffi_type*)*anum);
	FAKE_ASSERT(ffiAtypes,"invokeFlproc",__FILE__,__LINE__);
	uint32_t i=0;
	for(;i<anum;i++)
	{
		if(atypes[i]>LastNativeTypeId)
			ffiAtypes[i]=getFfiType(LastNativeTypeId);
		else
			ffiAtypes[i]=getFfiType(atypes[i]);
	}
	TypeId_t rtype=ft->rtype;
	ffi_type* ffiRtype=NULL;
	if(rtype>LastNativeTypeId)
		ffiRtype=getFfiType(LastNativeTypeId);
	else
		ffiRtype=getFfiType(rtype);
	VMvalue** args=(VMvalue**)malloc(sizeof(VMvalue*)*anum);
	FAKE_ASSERT(args,"invokeFlproc",__FILE__,__LINE__);
	for(i=0;i<anum;i++)
	{
		VMvalue* v=popVMstack(stack);
		if(v==NULL)
		{
			free(args);
			free(ffiAtypes);
			RAISE_BUILTIN_ERROR("flproc",TOOFEWARG,curR,exe);
		}
		if(IS_REF(v))
			v=*(VMvalue**)v;
		args[i]=v;
	}
	if(resBp(stack))
	{
		free(args);
		free(ffiAtypes);
		RAISE_BUILTIN_ERROR("flproc",TOOMANYARG,curR,exe);
	}
	void** pArgs=(void**)malloc(sizeof(void*)*anum);
	FAKE_ASSERT(pArgs,"invokeFlproc",__FILE__,__LINE__);
	for(i=0;i<anum;i++)
		if(castValueToVptr(atypes[i],args[i],&pArgs[i]))
		{
			free(ffiAtypes);
			free(args);
			free(pArgs);
			RAISE_BUILTIN_ERROR("flproc",WRONGARG,curR,exe);
		}
	uintptr_t retval=0x0;
	applyFF(func,anum,ffiRtype,ffiAtypes,&retval,pArgs);
	if(rtype!=0)
	{
		if(rtype==StringTypeId)
			SET_RETURN("B_invoke",newVMvalue(STR,(void*)retval,exe->heap),stack);
		else if(isFunctionTypeId(rtype))
			SET_RETURN("B_invoke",newVMvalue(FLPROC,newVMFlproc(rtype,(void*)retval,flproc->dll),exe->heap),stack);
		else if(rtype==FILEpTypeId)
			SET_RETURN("B_invoke",newVMvalue(FP,(void*)retval,exe->heap),stack);
		else
		{
			TypeId_t t=(rtype>LastNativeTypeId)?LastNativeTypeId:rtype;
			SET_RETURN("B_invoke",castVptrToVMvalueFunctionsList[t-1](retval,exe->heap),stack);
		}
	}
	for(i=0;i<anum;i++)
		free(pArgs[i]);
	free(pArgs);
	free(ffiAtypes);
	free(args);
}

/*--------------------------*/

FakeVMlist GlobFakeVMs={0,NULL};

static void (*ByteCodes[])(FakeVM*)=
{
	B_dummy,
	B_push_nil,
	B_push_pair,
	B_push_in32,
	B_push_in64,
	B_push_chr,
	B_push_dbl,
	B_push_str,
	B_push_sym,
	B_push_byte,
	B_push_var,
	B_push_env_var,
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
	B_pop_env,
	B_swap,
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

FakeVM* newFakeVM(ByteCode* mainCode)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	FAKE_ASSERT(exe,"newFakeVM",__FILE__,__LINE__);
	VMproc* tmpVMproc=NULL;
	exe->code=NULL;
	exe->size=0;
	exe->rstack=newComStack(32);
	if(mainCode!=NULL)
	{
		exe->code=copyMemory(mainCode->code,mainCode->size);
		exe->size=mainCode->size;
		tmpVMproc=newVMproc(0,mainCode->size);
		pushComStack(newVMrunnable(tmpVMproc),exe->rstack);
		freeVMproc(tmpVMproc);
	}
	exe->argc=0;
	exe->argv=NULL;
	exe->mark=1;
	exe->chan=NULL;
	exe->stack=newVMstack(0);
	exe->tstack=newComStack(32);
	exe->heap=newVMheap();
	exe->callback=NULL;
	FakeVM** ppFakeVM=NULL;
	int i=0;
	for(;i<GlobFakeVMs.num;i++)
		if(GlobFakeVMs.VMs[i]==NULL)
			ppFakeVM=GlobFakeVMs.VMs+i;
	if(ppFakeVM!=NULL)
	{
		exe->VMid=i;
		*ppFakeVM=exe;
	}
	else
	{
		int32_t size=GlobFakeVMs.num;
		GlobFakeVMs.VMs=(FakeVM**)realloc(GlobFakeVMs.VMs,sizeof(FakeVM*)*(size+1));
		FAKE_ASSERT(!size||GlobFakeVMs.VMs,"newFakeVM",__FILE__,__LINE__);
		GlobFakeVMs.VMs[size]=exe;
		GlobFakeVMs.num+=1;
		exe->VMid=size;
	}
	return exe;
}

FakeVM* newTmpFakeVM(ByteCode* mainCode)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	FAKE_ASSERT(exe,"newTmpFakeVM",__FILE__,__LINE__);
	exe->code=NULL;
	exe->size=0;
	exe->rstack=newComStack(32);
	if(mainCode!=NULL)
	{
		exe->code=copyMemory(mainCode->code,mainCode->size);
		exe->size=mainCode->size;
		pushComStack(newVMrunnable(newVMproc(0,mainCode->size)),exe->rstack);
	}
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->chan=NULL;
	exe->stack=newVMstack(0);
	exe->tstack=newComStack(32);
	exe->heap=newVMheap();
	exe->callback=threadErrorCallBack;
	exe->VMid=-1;
	return exe;
}

void initGlobEnv(VMenv* obj,VMheap* heap)
{
	DllFunc syscallFunctionList[]=
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
		SYS_write,
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
	};
	obj->num=NUMOFBUILTINSYMBOL;
	obj->list=(VMenvNode**)malloc(sizeof(VMenvNode*)*NUMOFBUILTINSYMBOL);
	FAKE_ASSERT(obj->list,"initGlobEnv",__FILE__,__LINE__);
	obj->list[0]=newVMenvNode(VM_NIL,addSymbolToGlob(builtInSymbolList[0])->id);
	obj->list[1]=newVMenvNode(newVMvalue(FP,stdin,heap),addSymbolToGlob(builtInSymbolList[1])->id);
	obj->list[2]=newVMenvNode(newVMvalue(FP,stdout,heap),addSymbolToGlob(builtInSymbolList[2])->id);
	obj->list[3]=newVMenvNode(newVMvalue(FP,stderr,heap),addSymbolToGlob(builtInSymbolList[3])->id);
	size_t i=4;
	for(;i<NUMOFBUILTINSYMBOL;i++)
		obj->list[i]=newVMenvNode(newVMvalue(DLPROC,newVMDlproc(syscallFunctionList[i-4],NULL),heap),addSymbolToGlob(builtInSymbolList[i])->id);
	mergeSort(obj->list,obj->num,sizeof(VMenvNode*),envNodeCmp);
}

void* ThreadVMFunc(void* p)
{
	FakeVM* exe=(FakeVM*)p;
	int64_t status=runFakeVM(exe);
	VMChanl* tmpCh=exe->chan->u.chan;
	exe->chan=NULL;
	if(!status)
	{
		VMstack* stack=exe->stack;
		VMvalue* v=NULL;
		while((v=GET_VAL(popVMstack(stack),exe->heap)))
			chanlSend(newSendT(v),tmpCh);
	}
	else
	{
		char* threadErrorMessage=copyStr("error:occur in thread ");
		char* id=intToString(exe->VMid);
		threadErrorMessage=strCat(threadErrorMessage,id);
		threadErrorMessage=strCat(threadErrorMessage,"\n");
		VMvalue* err=newVMvalue(ERR,newVMerror(NULL,builtInErrorType[THREADERROR],threadErrorMessage),exe->heap);
		free(threadErrorMessage);
		free(id);
		SendT* t=newSendT(err);
		chanlSend(t,tmpCh);
	}
	freeVMstack(exe->stack);
	exe->stack=NULL;
	exe->lnt=NULL;
	if(status!=0)
		deleteCallChain(exe);
	freeComStack(exe->tstack);
	freeComStack(exe->rstack);
	exe->mark=0;
	return (void*)status;
}

void* ThreadVMDlprocFunc(void* p)
{
	void** a=(void**)p;
	FakeVM* exe=a[0];
	DllFunc f=a[1];
	free(p);
	int64_t status=0;
	VMChanl* ch=exe->chan->u.chan;
	pthread_rwlock_rdlock(&GClock);
	if(!setjmp(exe->buf))
	{
		f(exe,&GClock);
		VMstack* stack=exe->stack;
		VMvalue* v=NULL;
		while((v=GET_VAL(popVMstack(stack),exe->heap)))
			chanlSend(newSendT(v),ch);
	}
	else
	{
		char* threadErrorMessage=copyStr("error:occur in thread ");
		char* id=intToString(exe->VMid);
		threadErrorMessage=strCat(threadErrorMessage,id);
		threadErrorMessage=strCat(threadErrorMessage,"\n");
		VMvalue* err=newVMvalue(ERR,newVMerror(NULL,builtInErrorType[THREADERROR],threadErrorMessage),exe->heap);
		free(threadErrorMessage);
		free(id);
		SendT* t=newSendT(err);
		chanlSend(t,ch);
		status=255;
	}
	pthread_rwlock_unlock(&GClock);
	freeVMstack(exe->stack);
	exe->stack=NULL;
	exe->lnt=NULL;
	deleteCallChain(exe);
	freeComStack(exe->rstack);
	freeComStack(exe->tstack);
	exe->mark=0;
	return (void*)status;
}

int runFakeVM(FakeVM* exe)
{
	while(!isComStackEmpty(exe->rstack))
	{
		VMrunnable* currunnable=topComStack(exe->rstack);
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
				VMrunnable* r=popComStack(exe->rstack);
				VMenv* tmpEnv=r->localenv;
				free(r);
				freeVMenv(tmpEnv);
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
				for(;i<GlobFakeVMs.num;i++)
				{
					if(GlobFakeVMs.VMs[i])
					{
						if(GlobFakeVMs.VMs[i]->mark)
							GC_mark(GlobFakeVMs.VMs[i]);
						else
						{
							pthread_join(GlobFakeVMs.VMs[i]->tid,NULL);
							free(GlobFakeVMs.VMs[i]);
							GlobFakeVMs.VMs[i]=NULL;
						}
					}
				}
			}
			else GC_mark(exe);
			GC_sweep(exe->heap);
			pthread_mutex_lock(&exe->heap->lock);
			exe->heap->threshold=exe->heap->num+THRESHOLD_SIZE;
			pthread_mutex_unlock(&exe->heap->lock);
			pthread_rwlock_unlock(&GClock);
		}
	}
	return 0;
}

void B_dummy(FakeVM* exe)
{
	VMrunnable* currunnable=topComStack(exe->rstack);
	uint32_t scp=currunnable->scp;
	VMstack* stack=exe->stack;
	DBG_printVMByteCode(exe->code,scp,currunnable->cpc,stderr);
	putc('\n',stderr);
	DBG_printVMstack(exe->stack,stderr,1);
	putc('\n',stderr);
	fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
	fprintf(stderr,"cp=%d stack->bp=%d\n%s\n",currunnable->cp-scp,stack->bp,codeName[(uint8_t)exe->code[currunnable->cp]].codeName);
	DBG_printVMenv(currunnable->localenv,stderr);
	putc('\n',stderr);
	fprintf(stderr,"Wrong byts code!\n");
	exit(EXIT_FAILURE);
}

void B_push_nil(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_push_nil",VM_NIL,stack);
	runnable->cp+=1;
}

void B_push_pair(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_push_pair",newVMvalue(PAIR,newVMpair(),exe->heap),stack);
	runnable->cp+=1;

}

void B_push_in32(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_push_in32",MAKE_VM_IN32(*(int32_t*)(exe->code+runnable->cp+1)),stack);
	runnable->cp+=5;
}

void B_push_in64(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* r=topComStack(exe->rstack);
	SET_RETURN("B_push_in32",newVMvalue(IN64,exe->code+r->cp+sizeof(char),exe->heap),stack);
	r->cp+=sizeof(char)+sizeof(int64_t);
}

void B_push_chr(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_push_chr",MAKE_VM_CHR(*(char*)(exe->code+runnable->cp+1)),stack);
	runnable->cp+=2;
}

void B_push_dbl(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_push_dbl",newVMvalue(DBL,exe->code+runnable->cp+1,exe->heap),stack);
	runnable->cp+=9;
}

void B_push_str(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int len=strlen((char*)(exe->code+runnable->cp+1));
	SET_RETURN("B_push_str",newVMvalue(STR,copyStr((char*)exe->code+runnable->cp+1),exe->heap),stack);
	runnable->cp+=2+len;
}

void B_push_sym(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_push_sym",MAKE_VM_SYM(*(Sid_t*)(exe->code+runnable->cp+1)),stack);
	runnable->cp+=5;
}

void B_push_byte(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int32_t size=*(int32_t*)(exe->code+runnable->cp+1);
	SET_RETURN("B_push_byte",newVMvalue(BYTS,newVMByts(*(int32_t*)(exe->code+runnable->cp+1),exe->code+runnable->cp+5),exe->heap),stack);
	runnable->cp+=5+size;
}

void B_push_var(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	Sid_t idOfVar=*(Sid_t*)(exe->code+runnable->cp+1);
	VMenv* curEnv=runnable->localenv;
	VMenvNode* tmp=NULL;
	while(curEnv&&!tmp)
	{
		tmp=findVMenvNode(idOfVar,curEnv);
		curEnv=curEnv->prev;
	}
	if(tmp==NULL)
		RAISE_BUILTIN_ERROR("b.push_var",SYMUNDEFINE,runnable,exe);
	SET_RETURN("B_push_var",tmp->value,stack);
	runnable->cp+=5;
}

void B_push_env_var(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=GET_VAL(getTopValue(stack),exe->heap);
	if(!IS_STR(topValue)&&!IS_SYM(topValue))
		RAISE_BUILTIN_ERROR("b.push_env_var",WRONGARG,runnable,exe);
	SymTabNode* stn=addSymbolToGlob(topValue->u.str);
	if(stn==NULL)
		RAISE_BUILTIN_ERROR("b.push_env_var",INVALIDSYMBOL,runnable,exe);
	Sid_t idOfVar=stn->id;
	VMenv* curEnv=runnable->localenv;
	VMenvNode* tmp=NULL;
	while(curEnv&&!tmp)
	{
		tmp=findVMenvNode(idOfVar,curEnv);
		curEnv=curEnv->prev;
	}
	if(tmp==NULL)
		RAISE_BUILTIN_ERROR("b.push_env_var",INVALIDSYMBOL,runnable,exe);
	stack->values[stack->tp-1]=tmp->value;
	runnable->cp+=1;
}

void B_push_top(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	if(stack->tp==stack->bp)
		RAISE_BUILTIN_ERROR("b.push_top",STACKERROR,runnable,exe);
	SET_RETURN("B_push_top",GET_VAL(getTopValue(stack),exe->heap),stack);
	runnable->cp+=1;
}

void B_push_proc(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	int32_t sizeOfProc=*(int32_t*)(exe->code+runnable->cp+1);
	VMproc* code=newVMproc(runnable->cp+1+sizeof(int32_t),sizeOfProc);
	increaseVMenvRefcount(runnable->localenv);
	code->prevEnv=runnable->localenv;
	VMvalue* objValue=newVMvalue(PRC,code,exe->heap);
	SET_RETURN("B_push_proc",objValue,stack);
	runnable->cp+=5+sizeOfProc;
}

void B_push_fproc(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	TypeId_t type=*(TypeId_t*)(exe->code+runnable->cp+1);
	VMheap* heap=exe->heap;
	VMvalue* sym=GET_VAL(popVMstack(stack),heap);
	VMvalue* dll=GET_VAL(popVMstack(stack),heap);
	if((!IS_SYM(sym)&&!IS_STR(sym))||(!IS_DLL(dll)))
		RAISE_BUILTIN_ERROR("b.push_fproc",WRONGARG,runnable,exe);
	char* str=IS_SYM(sym)?getGlobSymbolWithId(GET_SYM(sym))->symbol:sym->u.str;
	void* address=getAddress(str,dll->u.dll);
	if(!address)
		RAISE_BUILTIN_ERROR("b.push_fproc",INVALIDSYMBOL,runnable,exe);
	SET_RETURN("B_push_fproc",newVMvalue(FLPROC,newVMFlproc(type,address,dll),heap),stack);
	runnable->cp+=5;
}

void B_push_ptr_ref(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* r=topComStack(exe->rstack);
	ssize_t offset=*(ssize_t*)(exe->code+r->cp+sizeof(char));
	TypeId_t ptrId=*(TypeId_t*)(exe->code+r->cp+sizeof(char)+sizeof(ssize_t));
	VMvalue* value=getTopValue(stack);
	VMMem* mem=value->u.chf;
	if(!mem->mem)
		RAISE_BUILTIN_ERROR("b.push_ptr_ref",INVALIDACCESS,r,exe);
	stack->tp-=1;
	uint8_t* t=mem->mem+offset;
	VMMem* retval=newVMMem(ptrId,(uint8_t*)t);
	SET_RETURN("B_push_ptr_ref",MAKE_VM_CHF(retval,exe->heap),stack);
	r->cp+=sizeof(char)+sizeof(ssize_t)+sizeof(TypeId_t);
}

void B_push_def_ref(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* r=topComStack(exe->rstack);
	ssize_t offset=*(ssize_t*)(exe->code+r->cp+sizeof(char));
	TypeId_t typeId=*(TypeId_t*)(exe->code+r->cp+sizeof(char)+sizeof(ssize_t));
	VMvalue* value=getTopValue(stack);
	VMMem* mem=value->u.chf;
	if(!mem->mem)
		RAISE_BUILTIN_ERROR("b.push_def_ref",INVALIDACCESS,r,exe);
	stack->tp-=1;
	uint8_t* ptr=mem->mem+offset;
	VMvalue* retval=MAKE_VM_CHF(newVMMem(typeId,ptr),exe->heap);
	SET_RETURN("B_push_def_ref",retval,stack);
	r->cp+=sizeof(char)+sizeof(ssize_t)+sizeof(TypeId_t);
}

void B_push_ind_ref(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* r=topComStack(exe->rstack);
	VMvalue* index=getTopValue(stack);
	VMvalue* exp=getValue(stack,stack->tp-2);
	TypeId_t type=*(TypeId_t*)(exe->code+r->cp+sizeof(char));
	uint32_t size=*(uint32_t*)(exe->code+r->cp+sizeof(char)+sizeof(TypeId_t));
	if(!IS_MEM(exp)&&!IS_CHF(exp))
		RAISE_BUILTIN_ERROR("b.push_ind_ref",WRONGARG,r,exe);
	if(!IS_IN32(index))
		RAISE_BUILTIN_ERROR("b.push_ind_ref",WRONGARG,r,exe);
	VMMem* mem=exp->u.chf;
	if(!mem->mem)
		RAISE_BUILTIN_ERROR("b.push_ind_ref",INVALIDACCESS,r,exe);
	stack->tp-=2;
	VMvalue* retval=MAKE_VM_CHF(newVMMem(type,mem->mem+GET_IN32(index)*size),exe->heap);
	SET_RETURN("B_push_ind_ref",retval,stack);
	r->cp+=sizeof(char)+sizeof(TypeId_t)+sizeof(uint32_t);
}


void B_push_ref(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* r=topComStack(exe->rstack);
	VMvalue* exp=getTopValue(stack);
	ssize_t offset=*(ssize_t*)(exe->code+r->cp+sizeof(char));
	TypeId_t type=*(TypeId_t*)(exe->code+r->cp+sizeof(char)+sizeof(ssize_t));
	if(!IS_MEM(exp)&&!IS_CHF(exp))
		RAISE_BUILTIN_ERROR("b.push_ref",WRONGARG,r,exe);
	VMMem* mem=exp->u.chf;
	if(!mem->mem)
		RAISE_BUILTIN_ERROR("b.push_ref",INVALIDACCESS,r,exe);
	stack->tp-=1;
	VMMem* retval=newVMMem(type,mem->mem+offset);
	SET_RETURN("B_push_ref",MAKE_VM_CHF(retval,exe->heap),stack);
	r->cp+=sizeof(char)+sizeof(ssize_t)+sizeof(TypeId_t);
}

void B_pop(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=1;
}

void B_pop_var(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	if(!(stack->tp>stack->bp))
		RAISE_BUILTIN_ERROR("b.pop_var",STACKERROR,runnable,exe);
	int32_t scopeOfVar=*(int32_t*)(exe->code+runnable->cp+1);
	Sid_t idOfVar=*(Sid_t*)(exe->code+runnable->cp+5);
	VMenv* curEnv=runnable->localenv;
	VMvalue** pValue=NULL;
	if(scopeOfVar>=0)
	{
		int32_t i=0;
		for(;i<scopeOfVar;i++)
			curEnv=curEnv->prev;
		VMenvNode* tmp=findVMenvNode(idOfVar,curEnv);
		if(!tmp)
			tmp=addVMenvNode(newVMenvNode(NULL,idOfVar),curEnv);
		pValue=&tmp->value;
	}
	else
	{
		VMenvNode* tmp=NULL;
		while(curEnv&&!tmp)
		{
			tmp=findVMenvNode(idOfVar,curEnv);
			curEnv=curEnv->prev;
		}
		if(tmp==NULL)
			RAISE_BUILTIN_ERROR("b.pop_var",SYMUNDEFINE,runnable,exe);
		pValue=&tmp->value;
	}
	VMvalue* topValue=GET_VAL(getTopValue(stack),heap);
	*pValue=GET_VAL(topValue,heap);
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=9;
}

void B_pop_arg(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	if(!(stack->tp>stack->bp))
		RAISE_BUILTIN_ERROR("b.pop_arg",TOOFEWARG,runnable,exe);
	int32_t idOfVar=*(int32_t*)(exe->code+runnable->cp+1);
	VMenv* curEnv=runnable->localenv;
	VMvalue** pValue=NULL;
	VMenvNode* tmp=findVMenvNode(idOfVar,curEnv);
	if(!tmp)
		tmp=addVMenvNode(newVMenvNode(NULL,idOfVar),curEnv);
	pValue=&tmp->value;
	VMvalue* topValue=GET_VAL(getTopValue(stack),heap);
	*pValue=GET_VAL(topValue,heap);
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=5;
}

void B_pop_rest_arg(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	Sid_t idOfVar=*(Sid_t*)(exe->code+runnable->cp+1);
	VMenv* curEnv=runnable->localenv;
	VMvalue** pValue=NULL;
	VMenvNode* tmpNode=findVMenvNode(idOfVar,curEnv);
	if(!tmpNode)
		tmpNode=addVMenvNode(newVMenvNode(NULL,idOfVar),curEnv);
	pValue=&tmpNode->value;
	VMvalue* obj=NULL;
	VMvalue* tmp=NULL;
	if(stack->tp>stack->bp)
	{
		obj=newVMvalue(PAIR,newVMpair(),exe->heap);
		tmp=obj;
	}
	else obj=VM_NIL;
	while(stack->tp>stack->bp)
	{
		tmp->u.pair->car=GET_VAL(getTopValue(stack),heap);
		stack->tp-=1;
		if(stack->tp>stack->bp)
			tmp->u.pair->cdr=newVMvalue(PAIR,newVMpair(),heap);
		else break;
		tmp=tmp->u.pair->cdr;
	}
	*pValue=obj;
	stackRecycle(exe);
	runnable->cp+=5;
}

void B_pop_car(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	VMvalue* topValue=GET_VAL(getTopValue(stack),heap);
	VMvalue* objValue=GET_VAL(getValue(stack,stack->tp-2),heap);
	if(!IS_PAIR(objValue))
		RAISE_BUILTIN_ERROR("b.pop_car",WRONGARG,runnable,exe);
	objValue->u.pair->car=GET_VAL(topValue,heap);
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=1;
}

void B_pop_cdr(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	VMvalue* topValue=GET_VAL(getTopValue(stack),heap);
	VMvalue* objValue=GET_VAL(getValue(stack,stack->tp-2),heap);
	if(!IS_PAIR(objValue))
		RAISE_BUILTIN_ERROR("b.pop_cdr",WRONGARG,runnable,exe);
	objValue->u.pair->cdr=GET_VAL(topValue,heap);
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=1;
}

void B_pop_ref(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* val=GET_VAL(getTopValue(stack),exe->heap);
	VMvalue* ref=getValue(stack,stack->tp-2);
	if(!IS_REF(ref)&&!IS_CHF(ref)&&!IS_MEM(ref))
		RAISE_BUILTIN_ERROR("b.pop_ref",WRONGARG,runnable,exe);
	if(SET_REF(ref,val))
		RAISE_BUILTIN_ERROR("b.pop_ref",INVALIDASSIGN,runnable,exe);
	stack->tp-=1;
	stack->values[stack->tp-1]=val;
	stackRecycle(exe);
	runnable->cp+=1;
}

void B_pop_env(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* topValue=GET_VAL(getTopValue(stack),exe->heap);
	if(!IS_PRC(topValue))
		RAISE_BUILTIN_ERROR("b.pop_env",WRONGARG,runnable,exe);
	VMenv** ppEnv=&topValue->u.prc->prevEnv;
	VMenv* tmpEnv=*ppEnv;
	int32_t i=0;
	int32_t scope=*(int32_t*)(exe->code+runnable->cp+1);
	for(;i<scope&&tmpEnv;i++)
	{
		ppEnv=&tmpEnv->prev;
		tmpEnv=*ppEnv;
	}
	if(tmpEnv)
	{
		*ppEnv=tmpEnv->prev;
		tmpEnv->prev=NULL;
		freeVMenv(tmpEnv);
	}
	else
		RAISE_BUILTIN_ERROR("b.pop_env",STACKERROR,runnable,exe);
	runnable->cp+=5;
}

void B_swap(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	if(stack->tp<2)
		RAISE_BUILTIN_ERROR("b.swap",TOOFEWARG,runnable,exe);
	VMvalue* topValue=GET_VAL(getTopValue(stack),heap);
	VMvalue* otherValue=GET_VAL(getValue(stack,stack->tp-2),heap);
	stack->values[stack->tp-1]=GET_VAL(otherValue,heap);
	stack->values[stack->tp-2]=GET_VAL(topValue,heap);
	runnable->cp+=1;
}

void B_set_tp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	if(stack->tptp>=stack->tpsi)
	{
		stack->tpst=(uint32_t*)realloc(stack->tpst,sizeof(uint32_t)*(stack->tpsi+16));
		FAKE_ASSERT(stack->tpst,"B_set_tp",__FILE__,__LINE__);
		stack->tpsi+=16;
	}
	stack->tpst[stack->tptp]=stack->tp;
	stack->tptp+=1;
	runnable->cp+=1;
}

void B_set_bp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	SET_RETURN("B_set_bp",MAKE_VM_IN32(stack->bp),stack);
	stack->bp=stack->tp;
	runnable->cp+=1;
}

void B_res_tp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	stack->tp=(stack->tptp)?stack->tpst[stack->tptp-1]:0;
	runnable->cp+=1;
}

void B_pop_tp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	stack->tptp-=1;
	if(stack->tpsi-stack->tptp>16)
	{
		stack->tpst=(uint32_t*)realloc(stack->tpst,sizeof(uint32_t)*(stack->tpsi-16));
		FAKE_ASSERT(stack->tpst,"B_pop_tp",__FILE__,__LINE__);
		stack->tpsi-=16;
	}
	runnable->cp+=1;
}

void B_res_bp(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	if(stack->tp>stack->bp)
		RAISE_BUILTIN_ERROR("b.res_bp",TOOMANYARG,runnable,exe);
	VMvalue* prevBp=getTopValue(stack);
	stack->bp=GET_IN32(prevBp);
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=1;
}

void B_invoke(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* tmpValue=GET_VAL(getTopValue(stack),exe->heap);
	if(!IS_PTR(tmpValue)||(tmpValue->type!=PRC&&tmpValue->type!=CONT&&tmpValue->type!=DLPROC&&tmpValue->type!=FLPROC))
		RAISE_BUILTIN_ERROR("b.invoke",INVOKEERROR,runnable,exe);
	stack->tp-=1;
	stackRecycle(exe);
	runnable->cp+=1;
	switch(tmpValue->type)
	{
		case PRC:
			invokeNativeProcdure(exe,tmpValue->u.prc,runnable);
			break;
		case CONT:
			invokeContinuation(exe,tmpValue->u.cont);
			break;
		case DLPROC:
			invokeDlProc(exe,tmpValue->u.dlproc);
			break;
		case FLPROC:
			invokeFlproc(exe,tmpValue->u.flproc);
			break;
		default:
			break;
	}
}

void B_jmp_if_true(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* tmpValue=GET_VAL(getTopValue(stack),exe->heap);
	if(tmpValue!=VM_NIL)
	{
		int32_t where=*(int32_t*)(exe->code+runnable->cp+sizeof(char));
		runnable->cp+=where;
	}
	runnable->cp+=5;
}

void B_jmp_if_false(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMvalue* tmpValue=GET_VAL(getTopValue(stack),exe->heap);
	stackRecycle(exe);
	if(tmpValue==VM_NIL)
	{
		int32_t where=*(int32_t*)(exe->code+runnable->cp+sizeof(char));
		runnable->cp+=where;
	}
	runnable->cp+=5;
}

void B_jmp(FakeVM* exe)
{
	VMrunnable* runnable=topComStack(exe->rstack);
	int32_t where=*(int32_t*)(exe->code+runnable->cp+sizeof(char));
	runnable->cp+=where+5;
}

void B_append(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* runnable=topComStack(exe->rstack);
	VMheap* heap=exe->heap;
	VMvalue* fir=GET_VAL(getTopValue(stack),heap);
	VMvalue* sec=GET_VAL(getValue(stack,stack->tp-2),heap);
	if(sec!=VM_NIL&&!IS_PAIR(sec))
		RAISE_BUILTIN_ERROR("b.append",WRONGARG,runnable,exe);
	if(!IS_PAIR(sec))
		RAISE_BUILTIN_ERROR("b.append",WRONGARG,runnable,exe);
	VMvalue** lastpair=&sec;
	while(IS_PAIR(*lastpair))lastpair=&(*lastpair)->u.pair->cdr;
	*lastpair=fir;
	stack->tp-=1;
	stackRecycle(exe);
	stack->values[stack->tp-1]=sec;
	runnable->cp+=1;
}

void B_push_try(FakeVM* exe)
{
	VMrunnable* r=topComStack(exe->rstack);
	int32_t cpc=0;
	Sid_t sid=*(Sid_t*)(exe->code+r->cp+(++cpc));
	VMTryBlock* tb=newVMTryBlock(sid,exe->stack->tp,exe->rstack->top);
	cpc+=sizeof(Sid_t);
	int32_t handlerNum=*(int32_t*)(exe->code+r->cp+cpc);
	cpc+=sizeof(int32_t);
	unsigned int i=0;
	for(;i<handlerNum;i++)
	{
		Sid_t type=*(Sid_t*)(exe->code+r->cp+cpc);
		cpc+=sizeof(Sid_t);
		uint32_t pCpc=*(uint32_t*)(exe->code+r->cp+cpc);
		cpc+=sizeof(uint32_t);
		VMerrorHandler* h=newVMerrorHandler(type,r->cp+cpc,pCpc);
		pushComStack(h,tb->hstack);
		cpc+=pCpc;
	}
	pushComStack(tb,exe->tstack);
	r->cp+=cpc;
}

void B_pop_try(FakeVM* exe)
{
	VMrunnable* r=topComStack(exe->rstack);
	VMTryBlock* tb=popComStack(exe->tstack);
	freeVMTryBlock(tb);
	r->cp+=1;
}

VMstack* newVMstack(int32_t size)
{
	VMstack* tmp=(VMstack*)malloc(sizeof(VMstack));
	FAKE_ASSERT(tmp,"newVMstack",__FILE__,__LINE__);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->values=(VMvalue**)malloc(size*sizeof(VMvalue*));
	FAKE_ASSERT(tmp->values,"newVMstack",__FILE__,__LINE__);
	tmp->tpsi=0;
	tmp->tptp=0;
	tmp->tpst=NULL;
	return tmp;
}

void stackRecycle(FakeVM* exe)
{
	VMstack* stack=exe->stack;
	VMrunnable* currunnable=topComStack(exe->rstack);
	if(stack->size-stack->tp>64)
	{
		stack->values=(VMvalue**)realloc(stack->values,sizeof(VMvalue*)*(stack->size-64));
		FAKE_ASSERT(stack->values,"stackRecycle",__FILE__,__LINE__);
		if(stack->values==NULL)
		{
			fprintf(stderr,"stack->tp==%d,stack->size==%d\n",stack->tp,stack->size);
			fprintf(stderr,"cp=%d\n%s\n",currunnable->cp,codeName[(uint8_t)exe->code[currunnable->cp]].codeName);
			FAKE_ASSERT(stack->values,"stackRecycle",__FILE__,__LINE__);
		}
		stack->size-=64;
	}
}

void DBG_printVMstack(VMstack* stack,FILE* fp,int mode)
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
			VMvalue* tmp=stack->values[i];
			writeVMvalue(tmp,fp,NULL);
			putc('\n',fp);
		}
	}
}

void DBG_printVMvalue(VMvalue* v,FILE* fp)
{
	writeVMvalue(v,fp,NULL);
}

void DBG_printVMByteCode(uint8_t* code,uint32_t s,uint32_t c,FILE* fp)
{
	ByteCode t={c,code+s};
	printByteCode(&t,fp);
}

VMrunnable* hasSameProc(uint32_t scp,ComStack* rstack)
{
	size_t i=0;
	for(;i<rstack->top;i++)
	{
		VMrunnable* cur=rstack->data[i];
		if(cur->scp==scp)
			return cur;
	}
	return NULL;
}

int isTheLastExpress(const VMrunnable* runnable,const VMrunnable* same,const FakeVM* exe)
{
	size_t c=exe->rstack->top;
	if(same==NULL)
		return 0;
	uint32_t size=0;
	for(;;)
	{
		uint8_t* code=exe->code;
		uint32_t i=runnable->cp+(code[runnable->cp]==FAKE_INVOKE);
		size=runnable->scp+runnable->cpc;

		for(;i<size;i+=(code[i]==FAKE_JMP)?*(int32_t*)(code+i+1)+5:1)
			if(code[i]!=FAKE_POP_TP&&code[i]!=FAKE_POP_TRY&&code[i]!=FAKE_JMP)
				return 0;
		if(runnable==same)
			break;
		runnable=exe->rstack->data[--c];
	}
	return 1;
}

void DBG_printVMenv(VMenv* curEnv,FILE* fp)
{
	if(curEnv->num==0)
		fprintf(fp,"This ENV is empty!");
	else
	{
		fprintf(fp,"ENV:");
		for(int i=0;i<curEnv->num;i++)
		{
			VMvalue* tmp=curEnv->list[i]->value;
			writeVMvalue(tmp,fp,NULL);
			putc(' ',fp);
		}
	}
}

VMheap* newVMheap()
{
	VMheap* tmp=(VMheap*)malloc(sizeof(VMheap));
	FAKE_ASSERT(tmp,"newVMheap",__FILE__,__LINE__);
	tmp->num=0;
	tmp->threshold=THRESHOLD_SIZE;
	tmp->head=NULL;
	pthread_mutex_init(&tmp->lock,NULL);
	return tmp;
}

void GC_mark(FakeVM* exe)
{
	GC_markValueInStack(exe->stack);
	GC_markValueInCallChain(exe->rstack);
	if(exe->chan)
		GC_markValue(exe->chan);
}

void GC_markSendT(QueueNode* head)
{
	for(;head;head=head->next)
		GC_markValue(((SendT*)head->data)->m);
}

void GC_markValue(VMvalue* obj)
{
	ComStack* stack=newComStack(32);
	pushComStack(obj,stack);
	while(!isComStackEmpty(stack))
	{
		VMvalue* root=popComStack(stack);
		root=GET_TAG(root)==REF_TAG?*((VMvalue**)GET_PTR(root)):root;
		if(GET_TAG(root)==PTR_TAG&&!root->mark)
		{
			root->mark=1;
			if(root->type==PAIR)
			{
				pushComStack(getVMpairCar(root),stack);
				pushComStack(getVMpairCdr(root),stack);
			}
			else if(root->type==PRC)
			{
				VMenv* curEnv=root->u.prc->prevEnv;
				for(;curEnv!=NULL;curEnv=curEnv->prev)
				{
					uint32_t i=0;
					for(;i<curEnv->num;i++)
						pushComStack(curEnv->list[i]->value,stack);
				}
			}
			else if(root->type==CONT)
			{
				uint32_t i=0;
				for(;i<root->u.cont->stack->tp;i++)
					pushComStack(root->u.cont->stack->values[i],stack);
				for(i=0;i<root->u.cont->num;i++)
				{
					VMenv* env=root->u.cont->status[i].localenv;
					uint32_t j=0;
					for(;j<env->num;j++)
						pushComStack(env->list[i]->value,stack);
				}
			}
			else if(root->type==CHAN)
			{
				pthread_mutex_lock(&root->u.chan->lock);
				QueueNode* head=root->u.chan->messages->head;
				for(;head;head=head->next)
					pushComStack(head->data,stack);
				for(head=root->u.chan->sendq->head;head;head=head->next)
					pushComStack(((SendT*)head->data)->m,stack);
				pthread_mutex_unlock(&root->u.chan->lock);
			}
			else if(root->type==DLPROC&&root->u.dlproc->dll)
			{
				pushComStack(root->u.dlproc->dll,stack);
			}
			else if(root->type==FLPROC&&root->u.flproc->dll)
			{
				pushComStack(root->u.flproc->dll,stack);
			}
		}
	}
	freeComStack(stack);
}

void GC_markValueInEnv(VMenv* curEnv)
{
	int32_t i=0;
	for(;i<curEnv->num;i++)
		GC_markValue(curEnv->list[i]->value);
}

void GC_markValueInCallChain(ComStack* rstack)
{
	size_t i=0;
	for(;i<rstack->top;i++)
	{
		VMrunnable* cur=rstack->data[i];
		VMenv* curEnv=cur->localenv;
		for(;curEnv!=NULL;curEnv=curEnv->prev)
			GC_markValueInEnv(curEnv);
	}
}

void GC_markValueInStack(VMstack* stack)
{
	int32_t i=0;
	for(;i<stack->tp;i++)
	{
		VMvalue* t=stack->values[i];
		if(!IS_CHF(t))
			GC_markValue(t);
	}
}

void GC_markMessage(QueueNode* head)
{
	while(head!=NULL)
	{
		GC_markValue(head->data);
		head=head->next;
	}
}

void GC_sweep(VMheap* heap)
{
	VMvalue* cur=heap->head;
	while(cur!=NULL)
	{
		if(GET_TAG(cur)==PTR_TAG&&!cur->mark)
		{
			VMvalue* prev=cur;
			if(cur==heap->head)
				heap->head=cur->next;
			if(cur->next!=NULL)cur->next->prev=cur->prev;
			if(cur->prev!=NULL)cur->prev->next=cur->next;
			cur=cur->next;
			switch(prev->type)
			{
				case STR:
					free(prev->u.str);
					break;
				case DBL:
					free(prev->u.dbl);
					break;
				case IN64:
					free(prev->u.in64);
					break;
				case PAIR:
					free(prev->u.pair);
					break;
				case PRC:
					freeVMproc(prev->u.prc);
					break;
				case BYTS:
					free(prev->u.byts);
					break;
				case CONT:
					freeVMcontinuation(prev->u.cont);
					break;
				case CHAN:
					freeVMChanl(prev->u.chan);
					break;
				case FP:
					freeVMfp(prev->u.fp);
					break;
				case DLL:
					freeVMDll(prev->u.dll);
					break;
				case DLPROC:
					freeVMDlproc(prev->u.dlproc);
					break;
				case FLPROC:
					freeVMFlproc(prev->u.flproc);
					break;
				case ERR:
					freeVMerror(prev->u.err);
					break;
				case MEM:
				case CHF:
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

FakeVM* newThreadVM(VMproc* mainCode,VMheap* heap)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	FAKE_ASSERT(exe,"newThreadVM",__FILE__,__LINE__);
	VMrunnable* t=newVMrunnable(mainCode);
	t->localenv=newVMenv(mainCode->prevEnv);
	exe->rstack=newComStack(32);
	pushComStack(t,exe->rstack);
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->chan=newVMvalue(CHAN,newVMChanl(0),heap);
	exe->tstack=newComStack(32);
	exe->stack=newVMstack(0);
	exe->heap=heap;
	exe->callback=threadErrorCallBack;
	FakeVM** ppFakeVM=NULL;
	int i=0;
	for(;i<GlobFakeVMs.num;i++)
		if(GlobFakeVMs.VMs[i]==NULL)
		{
			ppFakeVM=GlobFakeVMs.VMs+i;
			break;
		}
	if(ppFakeVM!=NULL)
	{
		exe->VMid=i;
		*ppFakeVM=exe;
	}
	else
	{
		int32_t size=GlobFakeVMs.num;
		GlobFakeVMs.VMs=(FakeVM**)realloc(GlobFakeVMs.VMs,sizeof(FakeVM*)*(size+1));
		FAKE_ASSERT(!size||GlobFakeVMs.VMs,"newThreadVM",__FILE__,__LINE__);
		GlobFakeVMs.VMs[size]=exe;
		GlobFakeVMs.num+=1;
		exe->VMid=size;
	}
	return exe;
}

FakeVM* newThreadDlprocVM(VMrunnable* r,VMheap* heap)
{
	FakeVM* exe=(FakeVM*)malloc(sizeof(FakeVM));
	FAKE_ASSERT(exe,"newThreadVM",__FILE__,__LINE__);
	VMrunnable* t=newVMrunnable(NULL);
	t->cp=r->cp;
	t->localenv=NULL;
	exe->rstack=newComStack(32);
	pushComStack(t,exe->rstack);
	exe->mark=1;
	exe->argc=0;
	exe->argv=NULL;
	exe->chan=newVMvalue(CHAN,newVMChanl(0),heap);
	exe->tstack=newComStack(32);
	exe->stack=newVMstack(0);
	exe->heap=heap;
	exe->callback=threadErrorCallBack;
	FakeVM** ppFakeVM=NULL;
	int i=0;
	for(;i<GlobFakeVMs.num;i++)
		if(GlobFakeVMs.VMs[i]==NULL)
		{
			ppFakeVM=GlobFakeVMs.VMs+i;
			break;
		}
	if(ppFakeVM!=NULL)
	{
		exe->VMid=i;
		*ppFakeVM=exe;
	}
	else
	{
		int32_t size=GlobFakeVMs.num;
		GlobFakeVMs.VMs=(FakeVM**)realloc(GlobFakeVMs.VMs,sizeof(FakeVM*)*(size+1));
		FAKE_ASSERT(!size||GlobFakeVMs.VMs,"newThreadVM",__FILE__,__LINE__);
		GlobFakeVMs.VMs[size]=exe;
		GlobFakeVMs.num+=1;
		exe->VMid=size;
	}
	return exe;
}


void freeVMstack(VMstack* stack)
{
	free(stack->tpst);
	free(stack->values);
	free(stack);
}

void freeAllVMs()
{
	int i=1;
	FakeVM* cur=GlobFakeVMs.VMs[0];
	freeComStack(cur->tstack);
	freeComStack(cur->rstack);
	freeVMstack(cur->stack);
	free(cur->code);
	free(cur);
	for(;i<GlobFakeVMs.num;i++)
	{
		cur=GlobFakeVMs.VMs[i];
		if(cur!=NULL)
			free(cur);
	}
	free(GlobFakeVMs.VMs);
}

void freeVMheap(VMheap* h)
{
	GC_sweep(h);
	pthread_mutex_destroy(&h->lock);
	free(h);
}

void joinAllThread()
{
	int i=1;
	for(;i<GlobFakeVMs.num;i++)
	{
		FakeVM* cur=GlobFakeVMs.VMs[i];
		if(cur)
			pthread_join(cur->tid,NULL);
	}
}

void cancelAllThread()
{
	int i=1;
	for(;i<GlobFakeVMs.num;i++)
	{
		FakeVM* cur=GlobFakeVMs.VMs[i];
		if(cur)
		{
			pthread_cancel(cur->tid);
			pthread_join(cur->tid,NULL);
			if(cur->mark)
			{
				deleteCallChain(cur);
				freeVMstack(cur->stack);
				freeComStack(cur->rstack);
				freeComStack(cur->tstack);
			}
		}
	}
}

void deleteCallChain(FakeVM* exe)
{
	while(!isComStackEmpty(exe->rstack))
	{
		VMrunnable* cur=popComStack(exe->rstack);
		if(cur->localenv)
			freeVMenv(cur->localenv);
		free(cur);
	}
}

void createCallChainWithContinuation(FakeVM* vm,VMcontinuation* cc)
{
	VMstack* stack=vm->stack;
	VMstack* tmpStack=copyStack(cc->stack);
	int32_t i=stack->bp;
	for(;i<stack->tp;i++)
	{
		if(tmpStack->tp>=tmpStack->size)
		{
			tmpStack->values=(VMvalue**)realloc(tmpStack->values,sizeof(VMvalue*)*(tmpStack->size+64));
			FAKE_ASSERT(tmpStack->values,"createCallChainWithContinuation",__FILE__,__LINE__);
			tmpStack->size+=64;
		}
		tmpStack->values[tmpStack->tp]=stack->values[i];
		tmpStack->tp+=1;
	}
	deleteCallChain(vm);
	while(!isComStackEmpty(vm->tstack))
		freeVMTryBlock(popComStack(vm->tstack));
	vm->stack=tmpStack;
	freeVMstack(stack);
	for(i=0;i<cc->num;i++)
	{
		VMrunnable* cur=(VMrunnable*)malloc(sizeof(VMrunnable));
		FAKE_ASSERT(cur,"createCallChainWithContinuation",__FILE__,__LINE__);
		cur->cp=cc->status[i].cp;
		cur->localenv=cc->status[i].localenv;
		increaseVMenvRefcount(cur->localenv);
		cur->scp=cc->status[i].scp;
		cur->cpc=cc->status[i].cpc;
		cur->mark=cc->status[i].mark;
		pushComStack(cur,vm->rstack);
	}
	for(i=0;i<cc->tnum;i++)
	{
		VMTryBlock* tmp=&cc->tb[i];
		VMTryBlock* cur=newVMTryBlock(tmp->sid,tmp->tp,tmp->rtp);
		int32_t j=0;
		ComStack* hstack=tmp->hstack;
		uint32_t handlerNum=hstack->top;
		for(;j<handlerNum;j++)
		{
			VMerrorHandler* tmpH=hstack->data[j];
			VMerrorHandler* curH=newVMerrorHandler(tmpH->type,tmpH->proc.scp,tmpH->proc.cpc);
			pushComStack(curH,cur->hstack);
		}
		pushComStack(cur,vm->tstack);
	}
}
