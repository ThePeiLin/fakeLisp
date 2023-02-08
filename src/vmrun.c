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

static Greylink* createGreylink(FklVMvalue* v,struct Greylink* next)
{
	Greylink* g=(Greylink*)malloc(sizeof(Greylink));
	FKL_ASSERT(g);
	g->v=v;
	g->next=next;
	return g;
}

/*procedure call functions*/
static void callCompoundProcdure(FklVM* exe,FklVMproc* tmpProc,FklVMframe* frame)
{
	FklVMframe* tmpFrame=fklCreateVMframeWithProc(tmpProc,exe->frames);
	tmpFrame->u.c.localenv=fklCreateSaveVMvalue(FKL_TYPE_ENV,fklCreateVMenv(tmpProc->prevEnv,exe->gc));
	fklPushVMframe(tmpFrame,exe);
	fklAddToGC(tmpFrame->u.c.localenv,exe);
}

static void tailCallCompoundProcdure(FklVM* exe,FklVMproc* proc,FklVMframe* frame)
{
	if(fklGetCompoundFrameScp(frame)==proc->scp)
		frame->u.c.mark=1;
	else
	{
		FklVMframe* tmpFrame=fklCreateVMframeWithProc(proc,exe->frames->prev);
		tmpFrame->u.c.localenv=fklCreateSaveVMvalue(FKL_TYPE_ENV,fklCreateVMenv(proc->prevEnv,exe->gc));
		exe->frames->prev=tmpFrame;
		fklAddToGC(tmpFrame->u.c.localenv,exe);
	}
}

typedef struct
{
	FklVMFuncK kFunc;
	void* ctx;
	size_t size;
}VMcCC;

typedef enum
{
	DLPROC_READY=0,
	DLPROC_CCC,
	DLPROC_DONE,
}DlprocFrameState;

typedef struct
{
	FklVMvalue* proc;
	DlprocFrameState state;
	VMcCC ccc;
}DlprocFrameContext;

inline static void initVMcCC(VMcCC* ccc,FklVMFuncK kFunc,void* ctx,size_t size)
{
	ccc->kFunc=kFunc;
	ccc->ctx=ctx;
	ccc->size=size;
}

static void dlproc_frame_print_backtrace(void* data[6],FILE* fp,FklSymbolTable* table)
{
	DlprocFrameContext* c=(DlprocFrameContext*)data;
	FklVMdlproc* dlproc=c->proc->u.dlproc;
	if(dlproc->sid)
	{
		fprintf(fp,"at dlproc:");
		fklPrintString(fklGetSymbolWithId(dlproc->sid,table)->symbol
				,stderr);
		fputc('\n',fp);
	}
	else
		fputs("at <dlproc>\n",fp);
}

static void dlproc_frame_atomic(void* data[6],FklVMgc* gc)
{
	DlprocFrameContext* c=(DlprocFrameContext*)data;
	fklGC_toGrey(c->proc,gc);
}

static void dlproc_frame_finalizer(void* data[6])
{
	DlprocFrameContext* c=(DlprocFrameContext*)data;
	free(c->ccc.ctx);
}

static void dlproc_ccc_copy(const VMcCC* s,VMcCC* d)
{
	d->kFunc=s->kFunc;
	d->size=s->size;
	if(d->size)
		d->ctx=fklCopyMemory(s->ctx,d->size);
}

static void dlproc_frame_copy(void* const s[6],void* d[6],FklVM* exe)
{
	DlprocFrameContext* sc=(DlprocFrameContext*)s;
	DlprocFrameContext* dc=(DlprocFrameContext*)d;
	FklVMgc* gc=exe->gc;
	dc->state=sc->state;
	fklSetRef(&dc->proc,sc->proc,gc);
	dlproc_ccc_copy(&sc->ccc,&dc->ccc);
}

static int dlproc_frame_end(void* data[6])
{
	DlprocFrameContext* c=(DlprocFrameContext*)data;
	return c->state==DLPROC_DONE;
}

static void dlproc_frame_step(void* data[6],FklVM* exe)
{
	DlprocFrameContext* c=(DlprocFrameContext*)data;
	FklVMdlproc* dlproc=c->proc->u.dlproc;
	switch(c->state)
	{
		case DLPROC_READY:
			c->state=DLPROC_DONE;
			dlproc->func(exe,dlproc->dll,dlproc->pd);
			break;
		case DLPROC_CCC:
			c->state=DLPROC_DONE;
			c->ccc.kFunc(exe,FKL_CC_RE,c->ccc.ctx);
			break;
		case DLPROC_DONE:
			break;
	}
}

static const FklVMframeContextMethodTable DlprocContextMethodTable=
{
	.atomic=dlproc_frame_atomic,
	.finalizer=dlproc_frame_finalizer,
	.copy=dlproc_frame_copy,
	.print_backtrace=dlproc_frame_print_backtrace,
	.end=dlproc_frame_end,
	.step=dlproc_frame_step,
};

inline static void initDlprocFrameContext(void* data[6],FklVMvalue* proc,FklVMgc* gc)
{
	DlprocFrameContext* c=(DlprocFrameContext*)data;
	fklSetRef(&c->proc,proc,gc);
	c->state=DLPROC_READY;
	initVMcCC(&c->ccc,NULL,NULL,0);
}

void callDlProc(FklVM* exe,FklVMvalue* dlproc)
{
	FklVMframe* prev=exe->frames;
	FklVMframe* f=fklCreateOtherObjVMframe(&DlprocContextMethodTable,prev);
	initDlprocFrameContext(f->u.o.data,dlproc,exe->gc);
	fklPushVMframe(f,exe);
}

/*--------------------------*/

//static FklVMnode* createVMnode(FklVM* exe,FklVMnode* next)
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
//
#define BYTE_CODE_ARGS FklVM*,FklVMframe*
static void B_dummy(BYTE_CODE_ARGS);
static void B_push_nil(BYTE_CODE_ARGS);
static void B_push_pair(BYTE_CODE_ARGS);
static void B_push_i32(BYTE_CODE_ARGS);
static void B_push_i64(BYTE_CODE_ARGS);
static void B_push_chr(BYTE_CODE_ARGS);
static void B_push_f64(BYTE_CODE_ARGS);
static void B_push_str(BYTE_CODE_ARGS);
static void B_push_sym(BYTE_CODE_ARGS);
static void B_push_var(BYTE_CODE_ARGS);
static void B_push_top(BYTE_CODE_ARGS);
static void B_push_proc(BYTE_CODE_ARGS);
static void B_pop(BYTE_CODE_ARGS);
static void B_pop_var(BYTE_CODE_ARGS);
static void B_pop_arg(BYTE_CODE_ARGS);
static void B_pop_rest_arg(BYTE_CODE_ARGS);
static void B_set_tp(BYTE_CODE_ARGS);
static void B_set_bp(BYTE_CODE_ARGS);
static void B_call(BYTE_CODE_ARGS);
static void B_res_tp(BYTE_CODE_ARGS);
static void B_pop_tp(BYTE_CODE_ARGS);
static void B_res_bp(BYTE_CODE_ARGS);
static void B_jmp_if_true(BYTE_CODE_ARGS);
static void B_jmp_if_false(BYTE_CODE_ARGS);
static void B_jmp(BYTE_CODE_ARGS);
static void B_list_append(BYTE_CODE_ARGS);
static void B_push_vector(BYTE_CODE_ARGS);
static void B_push_r_env(BYTE_CODE_ARGS);
static void B_pop_r_env(BYTE_CODE_ARGS);
static void B_tail_call(BYTE_CODE_ARGS);
static void B_push_big_int(BYTE_CODE_ARGS);
static void B_push_box(BYTE_CODE_ARGS);
static void B_push_bytevector(BYTE_CODE_ARGS);
static void B_push_hash_eq(BYTE_CODE_ARGS);
static void B_push_hash_eqv(BYTE_CODE_ARGS);
static void B_push_hash_equal(BYTE_CODE_ARGS);
static void B_push_list_0(BYTE_CODE_ARGS);
static void B_push_list(BYTE_CODE_ARGS);
static void B_push_vector_0(BYTE_CODE_ARGS);
static void B_list_push(BYTE_CODE_ARGS);
static void B_import(BYTE_CODE_ARGS);
static void B_import_with_symbols(BYTE_CODE_ARGS);
static void B_import_from_dll(BYTE_CODE_ARGS);
static void B_import_from_dll_with_symbols(BYTE_CODE_ARGS);
#undef BYTE_CODE_ARGS

static void (*ByteCodes[])(FklVM*,FklVMframe*)=
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
	B_import,
	B_import_with_symbols,
	B_import_from_dll,
	B_import_from_dll_with_symbols,
};

inline static void insert_to_VM_chain(FklVM* cur,FklVM* prev,FklVM* next,FklVMgc* gc)
{
	cur->prev=prev;
	cur->next=next;
	if(prev)
	{
		prev->next=cur;
	}
	if(next)
	{
		next->prev=cur;
	}
}

static FklSid_t* createBuiltinErrorTypeIdList(void)
{
	FklSid_t* r=(FklSid_t*)malloc(sizeof(FklSid_t)*FKL_BUILTIN_ERR_NUM);
	FKL_ASSERT(r);
	return r;
}

FklVM* fklCreateVM(FklByteCodelnt* mainCode
		,FklSymbolTable* publicSymbolTable
		,FklVM* prev
		,FklVM* next)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	exe->frames=NULL;
	exe->tid=pthread_self();
	exe->gc=fklCreateVMgc();
	if(mainCode!=NULL)
	{
		FklVMvalue* codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,mainCode,exe->gc);
		exe->frames=fklCreateVMframeWithCodeObj(codeObj,exe->frames,exe->gc);
		exe->codeObj=codeObj;
	}
	exe->symbolTable=publicSymbolTable;
	exe->builtinErrorTypeId=createBuiltinErrorTypeIdList();
	fklInitBuiltinErrorType(exe->builtinErrorTypeId,publicSymbolTable);
	exe->mark=1;
	exe->chan=NULL;
	exe->stack=fklCreateVMstack(0);
	exe->libNum=0;
	exe->libs=NULL;
	insert_to_VM_chain(exe,prev,next,exe->gc);
	return exe;
}

//FklVM* fklCreateTmpVM(FklByteCode* mainCode,FklVMgc* gc,FklVM* prev,FklVM* next)
//{
//	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
//	FKL_ASSERT(exe);
//	exe->code=NULL;
//	exe->size=0;
//	exe->frames=NULL;
//	exe->nextCall=NULL;
//	exe->nextCallBackUp=NULL;
//	if(mainCode!=NULL)
//	{
//		exe->code=fklCopyMemory(mainCode->code,mainCode->size);
//		exe->size=mainCode->size;
//		exe->frames=fklCreateVMframe(fklCreateVMproc(0,mainCode->size),exe->frames);
//	}
//	exe->tid=pthread_self();
//	exe->mark=1;
//	exe->nny=0;
//	exe->chan=NULL;
//	exe->stack=fklCreateVMstack(0);
//	exe->tstack=fklCreatePtrStack(32,16);
//	if(gc)
//		exe->gc=gc;
//	else
//		exe->gc=fklCreateVMgc();
//	pthread_rwlock_init(&exe->rlock,NULL);
//	//	exe->callback=threadErrorCallBack;
//	exe->callback=NULL;
////	exe->thrds=1;
//	pthread_mutex_init(&exe->prev_next_lock,NULL);
//	exe->prev=prev;
//	exe->next=next;
//	if(prev)
//	{
//		pthread_mutex_lock(&exe->prev->prev_next_lock);
//		exe->prev->next=exe;
//		pthread_mutex_unlock(&exe->prev->prev_next_lock);
//	}
//	if(next)
//	{
//		pthread_mutex_lock(&exe->next->prev_next_lock);
//		exe->next->prev=exe;
//		pthread_mutex_unlock(&exe->next->prev_next_lock);
//	}
//	return exe;
//}

inline void** fklGetFrameData(FklVMframe* f)
{
	return f->u.o.data;
}

inline int fklIsCallableObjFrameReachEnd(FklVMframe* f)
{
	return f->u.o.t->end(fklGetFrameData(f));
}

inline void fklDoPrintBacktrace(FklVMframe* f,FILE* fp,FklSymbolTable* table)
{
	void (*backtrace)(void* data[6],FILE*,FklSymbolTable*)=f->u.o.t->print_backtrace;
	if(backtrace)
		backtrace(f->u.o.data,fp,table);
	else
		fprintf(fp,"at callable-obj\n");
}

inline void fklDoCallableObjFrameStep(FklVMframe* f,FklVM* exe)
{
	f->u.o.t->step(fklGetFrameData(f),exe);
}

inline void fklDoFinalizeObjFrame(FklVMframe* f)
{
	f->u.o.t->finalizer(fklGetFrameData(f));
	free(f);
}

inline void fklDoCopyObjFrameContext(FklVMframe* s,FklVMframe* d,FklVM* exe)
{
	s->u.o.t->copy(fklGetFrameData(s),fklGetFrameData(d),exe);
}

inline void fklPushVMframe(FklVMframe* f,FklVM* exe)
{
	f->prev=exe->frames;
	exe->frames=f;
}

inline static FklVMframe* popFrame(FklVM* exe)
{
	FklVMframe* frame=exe->frames;
	exe->frames=frame->prev;
	return frame;
}

inline static void doAtomicFrame(FklVMframe* f,FklVMgc* gc)
{
	f->u.o.t->atomic(fklGetFrameData(f),gc);
}

void fklDoAtomicFrame(FklVMframe* f,FklVMgc* gc)
{
	switch(f->type)
	{
		case FKL_FRAME_COMPOUND:
			fklGC_toGrey(fklGetCompoundFrameLocalenv(f),gc);
			fklGC_toGrey(fklGetCompoundFrameCodeObj(f),gc);
			break;
		case FKL_FRAME_OTHEROBJ:
			doAtomicFrame(f,gc);
			break;
	}
}

inline static void callCallableObj(FklVMvalue* v,FklVM* exe)
{
	switch(v->type)
	{
		case FKL_TYPE_DLPROC:
			callDlProc(exe,v);
			break;
		case FKL_TYPE_USERDATA:
			v->u.ud->t->__call(v,exe);
			break;
		default:
			break;
	}
}

inline static void applyCompoundProc(FklVM* exe,FklVMproc* tmpProc,FklVMframe* frame)
{
	FklVMframe* prevProc=fklHasSameProc(tmpProc->scp,exe->frames);
	if(fklIsTheLastExpress(frame,prevProc,exe)&&prevProc)
		prevProc->u.c.mark=1;
	else
	{
		FklVMframe* tmpFrame=fklCreateVMframeWithProc(tmpProc,exe->frames);
		tmpFrame->u.c.localenv=fklCreateSaveVMvalue(FKL_TYPE_ENV,fklCreateVMenv(tmpProc->prevEnv,exe->gc));
		fklPushVMframe(tmpFrame,exe);
		fklAddToGC(tmpFrame->u.c.localenv,exe);
	}
}

void fklCallobj(FklVMvalue* proc,FklVMframe* frame,FklVM* exe)
{
	switch(proc->type)
	{
		case FKL_TYPE_PROC:
			applyCompoundProc(exe,proc->u.proc,frame);
			break;
		default:
			callCallableObj(proc,exe);
			break;
	}
}

void fklTailCallobj(FklVMvalue* proc,FklVMframe* frame,FklVM* exe)
{
	exe->frames=frame->prev;
	fklDoFinalizeObjFrame(frame);
	fklCallobj(proc,exe->frames,exe);
}

void fklVMcallInDlproc(FklVMvalue* proc
		,size_t argNum
		,FklVMvalue* arglist[]
		,FklVMframe* frame
		,FklVM* exe
		,FklVMFuncK kFunc
		,void* ctx
		,size_t size)
{
	FklVMstack* stack=exe->stack;
	DlprocFrameContext* c=(DlprocFrameContext*)frame->u.o.data;
	c->state=DLPROC_CCC;
	initVMcCC(&c->ccc,kFunc,ctx,size);
	fklNiSetBp(stack->tp,stack);
	for(size_t i=argNum;i>0;i--)
		fklPushVMvalue(arglist[i-1],stack);
	switch(proc->type)
	{
		case FKL_TYPE_PROC:
			{
				FklVMframe* tmpFrame=fklCreateVMframeWithProc(proc->u.proc,exe->frames);
				tmpFrame->u.c.localenv=fklCreateSaveVMvalue(FKL_TYPE_ENV,fklCreateVMenv(proc->u.proc->prevEnv,exe->gc));
				fklPushVMframe(tmpFrame,exe);
				fklAddToGC(tmpFrame->u.c.localenv,exe);
			}
			break;
		default:
			callCallableObj(proc,exe);
			break;
	}
}

inline int fklTcMutexTryAcquire(FklVMgc* gc)
{
	return pthread_mutex_trylock(&gc->tcMutex);
}

inline void fklTcMutexAcquire(FklVMgc* gc)
{
	pthread_mutex_lock(&gc->tcMutex);
}

inline void fklTcMutexRelease(FklVMgc* gc)
{
	pthread_mutex_unlock(&gc->tcMutex);
}

inline void fklDoCompoundFrameStep(FklVMframe* curframe,FklVM* exe)
{
	uint64_t cp=fklGetCompoundFrameCp(curframe);
	ByteCodes[fklGetCompoundFrameCode(curframe)[cp]](exe,curframe);
}

int fklRunVM(FklVM* exe)
{
	if(setjmp(exe->buf)==1)
	{
		fklTcMutexRelease(exe->gc);
		return 255;
	}
	while(exe->frames)
	{
		fklTcMutexAcquire(exe->gc);
		FklVMframe* curframe=exe->frames;
		switch(curframe->type)
		{
			case FKL_FRAME_COMPOUND:
				if(fklIsCompoundFrameReachEnd(curframe))
					free(popFrame(exe));
				else
					fklDoCompoundFrameStep(curframe,exe);
				break;
			case FKL_FRAME_OTHEROBJ:
				if(fklIsCallableObjFrameReachEnd(curframe))
					fklDoFinalizeObjFrame(popFrame(exe));
				else
					fklDoCallableObjFrameStep(curframe,exe);
				break;
		}
		fklTcMutexRelease(exe->gc);
	}
	return 0;
}

inline void fklChangeGCstate(FklGCstate state,FklVMgc* gc)
{
	gc->running=state;
}

inline FklGCstate fklGetGCstate(FklVMgc* gc)
{
	FklGCstate state=FKL_GC_NONE;
	state=gc->running;
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

static void B_dummy(FklVM* exe,FklVMframe* frame)
{
	FKL_ASSERT(0);
}

static void B_push_nil(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	fklPushVMvalue(FKL_VM_NIL,stack);
	fklIncCompoundFrameCp(frame);
}

static void B_push_pair(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* cdr=fklNiGetArg(&ap,stack);
	FklVMvalue* car=fklNiGetArg(&ap,stack);
	fklNiReturn(fklCreateVMpairV(car,cdr,exe),&ap,stack);
	fklNiEnd(&ap,stack);
	fklIncCompoundFrameCp(frame);
}

static void B_push_i32(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	fklPushVMvalue(FKL_MAKE_VM_I32(fklGetI32FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char))),stack);
	fklIncAndAddCompoundFrameCp(frame,sizeof(int32_t));
}

static void B_push_i64(FklVM* exe,FklVMframe* frame)
{
	fklCreateVMvalueToStack(FKL_TYPE_I64,fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char),exe);
	fklIncAndAddCompoundFrameCp(frame,sizeof(int64_t));
}

static void B_push_chr(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	fklPushVMvalue(FKL_MAKE_VM_CHR(*(char*)(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char))),stack);
	fklIncAndAddCompoundFrameCp(frame,sizeof(char));
}

static void B_push_f64(FklVM* exe,FklVMframe* frame)
{
	fklCreateVMvalueToStack(FKL_TYPE_F64,fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char),exe);
	fklIncAndAddCompoundFrameCp(frame,sizeof(double));
}

static void B_push_str(FklVM* exe,FklVMframe* frame)
{
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,(char*)fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char)+sizeof(uint64_t)),exe);
	fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t)+size);
}

static void B_push_sym(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	fklPushVMvalue(FKL_MAKE_VM_SYM(fklGetSidFromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char))),stack);
	fklIncAndAddCompoundFrameCp(frame,sizeof(FklSid_t));
}

static void B_push_var(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	FklSid_t idOfVar=fklGetSidFromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMvalue* curEnv=fklGetCompoundFrameLocalenv(frame);
	FklVMvalue* volatile* pv=NULL;
	while(!pv&&curEnv&&curEnv!=FKL_VM_NIL)
	{
		pv=fklFindVar(idOfVar,curEnv->u.env);
		curEnv=curEnv->u.env->prev;
	}
	if(pv==NULL)
	{
		char* cstr=fklStringToCstr(fklGetSymbolWithId(idOfVar,exe->symbolTable)->symbol);
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.push-var",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
	}
	fklPushVMvalue(*pv,stack);
	fklIncAndAddCompoundFrameCp(frame,sizeof(FklSid_t));
}

static void B_push_top(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	if(stack->tp==stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.push-top",FKL_ERR_STACKERROR,exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	fklNiReturn(val,&ap,stack);
	fklNiReturn(val,&ap,stack);
	fklNiEnd(&ap,stack);
	fklIncCompoundFrameCp(frame);
}

static void B_push_proc(FklVM* exe,FklVMframe* frame)
{
	uint64_t sizeOfProc=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMproc* code=fklCreateVMproc(fklGetCompoundFrameCp(frame)+sizeof(char)+sizeof(uint64_t),sizeOfProc,fklGetCompoundFrameCodeObj(frame),exe->gc);
	fklCreateVMvalueToStack(FKL_TYPE_PROC,code,exe);
	fklSetRef(&code->prevEnv,fklGetCompoundFrameLocalenv(frame),exe->gc);
	fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t)+sizeOfProc);
}

static void B_pop(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
//	pthread_rwlock_wrlock(&stack->lock);
	stack->tp-=1;
//	pthread_rwlock_unlock(&stack->lock);
	fklIncCompoundFrameCp(frame);
}

static void B_pop_var(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	if(!(stack->tp>stack->bp))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.pop-var",FKL_ERR_STACKERROR,exe);
	uint32_t scopeOfVar=fklGetU32FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklSid_t idOfVar=fklGetSidFromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char)+sizeof(int32_t));
	FklVMvalue* curEnv=fklGetCompoundFrameLocalenv(frame);
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
		{
			char* cstr=fklStringToCstr(fklGetSymbolWithId(idOfVar,exe->symbolTable)->symbol);
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.pop-var",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
		}
	}
	fklSetRef(pv,fklNiGetArg(&ap,stack),exe->gc);
	fklNiEnd(&ap,stack);
	fklNiDoSomeAfterSetq(*pv,idOfVar);
	fklIncAndAddCompoundFrameCp(frame,sizeof(int32_t)+sizeof(FklSid_t));
}

static void B_pop_arg(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	if(ap<=stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.pop-arg",FKL_ERR_TOOFEWARG,exe);
	FklSid_t idOfVar=fklGetSidFromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMvalue* curEnv=fklGetCompoundFrameLocalenv(frame);
	FklVMvalue* volatile* pValue=fklFindOrAddVar(idOfVar,curEnv->u.env);
	fklSetRef(pValue,fklNiGetArg(&ap,stack),exe->gc);
	fklNiEnd(&ap,stack);
	fklNiDoSomeAfterSetq(*pValue,idOfVar);
	fklIncAndAddCompoundFrameCp(frame,sizeof(FklSid_t));
}

static void B_pop_rest_arg(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMgc* gc=exe->gc;
	FklSid_t idOfVar=fklGetSidFromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMvalue* curEnv=fklGetCompoundFrameLocalenv(frame);
	FklVMvalue* obj=FKL_VM_NIL;
	FklVMvalue* volatile* pValue=&obj;
	for(;ap>stack->bp;pValue=&(*pValue)->u.pair->cdr)
		*pValue=fklCreateVMpairV(fklNiGetArg(&ap,stack),FKL_VM_NIL,exe);
	pValue=fklFindOrAddVar(idOfVar,curEnv->u.env);
	fklSetRef(pValue,obj,gc);
	fklNiEnd(&ap,stack);
	fklIncAndAddCompoundFrameCp(frame,sizeof(FklSid_t));
}

static void B_set_tp(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	fklNiSetTp(stack);
	fklIncCompoundFrameCp(frame);
}

static void B_set_bp(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	fklNiSetBp(stack->tp,stack);
	stack->bp=stack->tp;
	fklIncCompoundFrameCp(frame);
}

static void B_res_tp(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	fklNiResTp(stack);
	fklIncCompoundFrameCp(frame);
}

static void B_pop_tp(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	fklNiPopTp(stack);
	fklIncCompoundFrameCp(frame);
}

static void B_res_bp(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	if(stack->tp>stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.res-bp",FKL_ERR_TOOMANYARG,exe);
	stack->bp=fklPopUintStack(&stack->bps);
	fklIncCompoundFrameCp(frame);
}

static void B_call(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* tmpValue=fklNiGetArg(&ap,stack);
	if(!fklIsCallable(tmpValue))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.call",FKL_ERR_CALL_ERROR,exe);
	fklIncCompoundFrameCp(frame);
	switch(tmpValue->type)
	{
		case FKL_TYPE_PROC:
			callCompoundProcdure(exe,tmpValue->u.proc,frame);
			break;
		default:
			callCallableObj(tmpValue,exe);
			break;
	}
	fklNiEnd(&ap,stack);
}

static void B_tail_call(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* tmpValue=fklNiGetArg(&ap,stack);
	if(!fklIsCallable(tmpValue))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.tail-call",FKL_ERR_CALL_ERROR,exe);
	fklIncCompoundFrameCp(frame);
	switch(tmpValue->type)
	{
		case FKL_TYPE_PROC:
			tailCallCompoundProcdure(exe,tmpValue->u.proc,frame);
			break;
		default:
			callCallableObj(tmpValue,exe);
			break;
	}
	fklNiEnd(&ap,stack);
}

static void B_jmp_if_true(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* tmpValue=fklGetTopValue(stack);
	if(tmpValue!=FKL_VM_NIL)
		fklAddCompoundFrameCp(frame,fklGetI64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char)));
	fklIncAndAddCompoundFrameCp(frame,sizeof(int64_t));
}

static void B_jmp_if_false(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* tmpValue=fklGetTopValue(stack);
	if(tmpValue==FKL_VM_NIL)
		fklAddCompoundFrameCp(frame,fklGetI64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char)));
	fklIncAndAddCompoundFrameCp(frame,sizeof(int64_t));
}

static void B_jmp(FklVM* exe,FklVMframe* frame)
{
	fklIncAndAddCompoundFrameCp(frame
			,fklGetI64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char))+sizeof(int64_t));
}

static void B_list_append(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
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
			FKL_RAISE_BUILTIN_ERROR_CSTR("b.list-append",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklSetRef(lastcdr,fir,exe->gc);
		fklNiReturn(sec,&ap,stack);
	}
	fklNiEnd(&ap,stack);
	fklIncCompoundFrameCp(frame);
}

static void B_push_vector(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMvalue* vec=fklCreateVMvecV(size,NULL,exe);
	for(size_t i=size;i>0;i--)
		fklSetRef(&vec->u.vec->base[i-1],fklNiGetArg(&ap,stack),exe->gc);
	fklNiReturn(vec
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
	fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t));
}

static void B_push_r_env(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* n=fklCreateVMvalueToStack(FKL_TYPE_ENV,fklCreateVMenv(FKL_VM_NIL,exe->gc),exe);
	fklSetRef(&n->u.env->prev,fklGetCompoundFrameLocalenv(frame),exe->gc);
	frame->u.c.localenv=n;
	fklNiEnd(&ap,stack);
	fklIncCompoundFrameCp(frame);
}

static void B_pop_r_env(FklVM* exe,FklVMframe* frame)
{
	frame->u.c.localenv=fklGetCompoundFrameLocalenv(frame)->u.env->prev;
	fklIncCompoundFrameCp(frame);
}

static void B_push_big_int(FklVM* exe,FklVMframe* frame)
{
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklBigInt* bigInt=fklCreateBigIntFromMem(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char)+sizeof(num),sizeof(uint8_t)*num);
	fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bigInt,exe);
	fklIncAndAddCompoundFrameCp(frame,sizeof(bigInt->num)+num);
}

static void B_push_box(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	FklVMvalue* box=fklCreateVMvalueToStack(FKL_TYPE_BOX,FKL_VM_NIL,exe);
	fklSetRef(&box->u.box,c,exe->gc);
	fklNiReturn(box,&ap,stack);
	fklNiEnd(&ap,stack);
	fklIncCompoundFrameCp(frame);
}

static void B_push_bytevector(FklVM* exe,FklVMframe* frame)
{
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(size,fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char)+sizeof(uint64_t)),exe);
	fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t)+size);
}

static void B_push_hash_eq(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMvalue* hash=fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE
			,fklCreateVMhashTable(FKL_VM_HASH_EQ),exe);
	for(size_t i=0;i<num;i++)
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		FklVMvalue* key=fklNiGetArg(&ap,stack);
		fklSetVMhashTableInReverseOrder(key,value,hash->u.hash,exe->gc);
	}
	fklNiReturn(hash,&ap,stack);
	fklNiEnd(&ap,stack);
	fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t));
}

static void B_push_hash_eqv(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMvalue* hash=fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE
			,fklCreateVMhashTable(FKL_VM_HASH_EQV),exe);
	for(size_t i=0;i<num;i++)
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		FklVMvalue* key=fklNiGetArg(&ap,stack);
		fklSetVMhashTableInReverseOrder(key,value,hash->u.hash,exe->gc);
	}
	fklNiReturn(hash,&ap,stack);
	fklNiEnd(&ap,stack);
	fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t));
}

static void B_push_hash_equal(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMvalue* hash=fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE
			,fklCreateVMhashTable(FKL_VM_HASH_EQUAL),exe);
	for(size_t i=0;i<num;i++)
	{
		FklVMvalue* value=fklNiGetArg(&ap,stack);
		FklVMvalue* key=fklNiGetArg(&ap,stack);
		fklSetVMhashTableInReverseOrder(key,value,hash->u.hash,exe->gc);
	}
	fklNiReturn(hash,&ap,stack);
	fklNiEnd(&ap,stack);
	fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t));
}

static void B_push_list_0(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* pair=FKL_VM_NIL;
	FklVMvalue* last=fklNiGetArg(&ap,stack);
	FklVMvalue** pcur=&pair;
	size_t bp=stack->bp;
	for(size_t i=bp;ap>bp;ap--,i++)
	{
		*pcur=fklCreateVMpairV(stack->values[i],FKL_VM_NIL,exe);
		pcur=&(*pcur)->u.pair->cdr;
	}
	fklSetRef(pcur,last,exe->gc);
	fklNiResBp(&ap,stack);
	fklNiReturn(pair,&ap,stack);
	fklNiEnd(&ap,stack);
	fklIncCompoundFrameCp(frame);
}

static void B_push_list(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMvalue* pair=FKL_VM_NIL;
	FklVMvalue* last=fklNiGetArg(&ap,stack);
	size--;
	FklVMvalue** pcur=&pair;
	for(size_t i=ap-size;i<ap;i++)
	{
		*pcur=fklCreateVMpairV(stack->values[i],FKL_VM_NIL,exe);
		pcur=&(*pcur)->u.pair->cdr;
	}
	ap-=size;
	fklSetRef(pcur,last,exe->gc);
	fklNiReturn(pair,&ap,stack);
	fklNiEnd(&ap,stack);
	fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t));
}

static void B_push_vector_0(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	size_t size=ap-stack->bp;
	FklVMvalue* vec=fklCreateVMvecV(size,NULL,exe);
	for(size_t i=size;i>0;i--)
		fklSetRef(&vec->u.vec->base[i-1],fklNiGetArg(&ap,stack),exe->gc);
	fklNiResBp(&ap,stack);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
	fklIncCompoundFrameCp(frame);
}

static void B_list_push(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	for(;FKL_IS_PAIR(list);list=list->u.pair->cdr)
		fklNiReturn(list->u.pair->car,&ap,stack);
	if(list!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.list-push",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiEnd(&ap,stack);
	fklIncCompoundFrameCp(frame);
}

static void B_import(FklVM* exe,FklVMframe* frame)
{
	uint64_t libId=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMlib* plib=&exe->libs[libId-1];
	if(plib->libEnv==FKL_VM_NIL)
	{
		callCompoundProcdure(exe,plib->proc->u.proc,frame);
		fklSetRef(&plib->libEnv,fklGetCompoundFrameLocalenv(exe->frames),exe->gc);
	}
	else
	{
		for(size_t i=0;i<plib->exportNum;i++)
		{
			FklVMvalue* volatile* pv=fklFindVar(plib->exports[i],plib->libEnv->u.env);
			if(pv==NULL)
			{
				char* cstr=fklStringToCstr(fklGetSymbolWithId(plib->exports[i],exe->symbolTable)->symbol);
				FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.import",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
			}
			FklVMvalue* volatile* pValue=fklFindOrAddVar(plib->exports[i],fklGetCompoundFrameLocalenv(frame)->u.env);
			fklSetRef(pValue,*pv,exe->gc);
		}
		fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t));
	}
}

static void B_import_with_symbols(FklVM* exe,FklVMframe* frame)
{
	uint64_t libId=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMlib* plib=&exe->libs[libId-1];
	if(plib->libEnv==FKL_VM_NIL)
	{
		callCompoundProcdure(exe,plib->proc->u.proc,frame);
		fklSetRef(&plib->libEnv,fklGetCompoundFrameLocalenv(exe->frames),exe->gc);
	}
	else
	{
		uint64_t exportNum=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char)+sizeof(uint64_t));
		FklSid_t* exports=fklCopyMemory(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char)+sizeof(uint64_t)+sizeof(uint64_t)
				,sizeof(FklSid_t)*exportNum);
		for(size_t i=0;i<exportNum;i++)
		{
			FklVMvalue* volatile* pv=fklFindVar(plib->exports[i],plib->libEnv->u.env);
			if(pv==NULL)
			{
				free(exports);
				char* cstr=fklStringToCstr(fklGetSymbolWithId(plib->exports[i],exe->symbolTable)->symbol);
				FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.import-with-symbols",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
			}
			FklVMvalue* volatile* pValue=fklFindOrAddVar(exports[i],fklGetCompoundFrameLocalenv(frame)->u.env);
			fklSetRef(pValue,*pv,exe->gc);
		}
		free(exports);
		fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t)*2+exportNum*sizeof(FklSid_t));
	}
}

static inline FklImportDllInitFunc getImportInit(FklDllHandle handle)
{
	return fklGetAddress("_fklImportInit",handle);
}

static void B_import_from_dll(FklVM* exe,FklVMframe* frame)
{
	uint64_t libId=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMlib* plib=&exe->libs[libId-1];
	FKL_NI_BEGIN(exe);
	if(plib->libEnv==FKL_VM_NIL)
	{
		char* realpath=fklStringToCstr(plib->proc->u.str);
		FklVMdll* dll=fklCreateVMdll(realpath);
		FklImportDllInitFunc initFunc=NULL;
		if(dll)
			initFunc=getImportInit(dll->handle);
		else
		{
			fklDestroyVMdll(dll);
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.import-from-dll",realpath,1,FKL_ERR_IMPORTFAILED,exe);
		}
		if(!initFunc)
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.import-from-dll",realpath,1,FKL_ERR_IMPORTFAILED,exe);
		FklVMvalue* dllValue=fklCreateVMvalueToStack(FKL_TYPE_DLL,dll,exe);
		fklInitVMdll(dllValue,exe);
		FklVMenv* env=fklCreateVMenv(FKL_VM_NIL,exe->gc);
		FklVMvalue* envValue=fklCreateVMvalueToStack(FKL_TYPE_ENV,env,exe);
		fklSetRef(&plib->libEnv,envValue,exe->gc);
		fklSetRef(&plib->proc,dllValue,exe->gc);
		initFunc(exe,dllValue,plib->libEnv);
		free(realpath);
	}
	for(size_t i=0;i<plib->exportNum;i++)
	{
		FklVMvalue* volatile* pv=fklFindVar(plib->exports[i],plib->libEnv->u.env);
		if(pv==NULL)
		{
			char* cstr=fklStringToCstr(fklGetSymbolWithId(plib->exports[i],exe->symbolTable)->symbol);
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.import-from-dll",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
		}
		FklVMvalue* volatile* pValue=fklFindOrAddVar(plib->exports[i],fklGetCompoundFrameLocalenv(frame)->u.env);
		fklSetRef(pValue,*pv,exe->gc);
	}
	fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t));
	fklNiEnd(&ap,stack);
}

static void B_import_from_dll_with_symbols(FklVM* exe,FklVMframe* frame)
{
	uint64_t libId=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char));
	FklVMlib* plib=&exe->libs[libId-1];
	if(plib->libEnv==FKL_VM_NIL)
	{
		char* realpath=fklStringToCstr(plib->proc->u.str);
		FklVMdll* dll=fklCreateVMdll(realpath);
		FklImportDllInitFunc initFunc=NULL;
		if(dll)
			initFunc=getImportInit(dll->handle);
		if(!initFunc)
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.import-from-dll-wit-symbols",realpath,1,FKL_ERR_IMPORTFAILED,exe);
		FklVMvalue* dllValue=fklCreateVMvalueToStack(FKL_TYPE_DLL,dll,exe);
		fklInitVMdll(dllValue,exe);
		FklVMenv* env=fklCreateVMenv(FKL_VM_NIL,exe->gc);
		FklVMvalue* envValue=fklCreateVMvalueToStack(FKL_TYPE_ENV,env,exe);
		fklSetRef(&plib->libEnv,envValue,exe->gc);
		fklSetRef(&plib->proc,dllValue,exe->gc);
		initFunc(exe,dllValue,plib->libEnv);
		free(realpath);
	}
	uint64_t exportNum=fklGetU64FromByteCode(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char)+sizeof(uint64_t));
	FklSid_t* exports=fklCopyMemory(fklGetCompoundFrameCode(frame)+fklGetCompoundFrameCp(frame)+sizeof(char)+sizeof(uint64_t)+sizeof(uint64_t)
			,sizeof(FklSid_t)*exportNum);
	for(size_t i=0;i<exportNum;i++)
	{
		FklVMvalue* volatile* pv=fklFindVar(plib->exports[i],plib->libEnv->u.env);
		if(pv==NULL)
		{
			free(exports);
			char* cstr=fklStringToCstr(fklGetSymbolWithId(plib->exports[i],exe->symbolTable)->symbol);
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.import-from-dll-with-symbols",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
		}
		FklVMvalue* volatile* pValue=fklFindOrAddVar(exports[i],fklGetCompoundFrameLocalenv(frame)->u.env);
		fklSetRef(pValue,*pv,exe->gc);
	}
	free(exports);
	fklIncAndAddCompoundFrameCp(frame,sizeof(uint64_t)*2+exportNum*sizeof(FklSid_t));
}

FklVMstack* fklCreateVMstack(int32_t size)
{
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->values=(FklVMvalue**)malloc(size*sizeof(FklVMvalue*));
	FKL_ASSERT(tmp->values);
	tmp->tps=(FklUintStack){NULL,0,0,0};
	fklInitUintStack(&tmp->tps,32,16);
	tmp->bps=(FklUintStack){NULL,0,0,0};
	fklInitUintStack(&tmp->bps,32,16);
//	pthread_rwlock_init(&tmp->lock,NULL);
	return tmp;
}

#define RECYCLE_NUN (128)
void fklStackRecycle(FklVMstack* stack)
{
	if(stack->size-stack->tp>RECYCLE_NUN)
	{
		stack->values=(FklVMvalue**)realloc(stack->values,sizeof(FklVMvalue*)*(stack->size-RECYCLE_NUN));
		FKL_ASSERT(stack->values);
		stack->size-=RECYCLE_NUN;
	}
}
#undef RECYCLE_NUN

void fklDBG_printVMstack(FklVMstack* stack,FILE* fp,int mode,FklSymbolTable* table)
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
			fklPrin1VMvalue(tmp,fp,table);
			putc('\n',fp);
		}
	}
}

void fklDBG_printVMvalue(FklVMvalue* v,FILE* fp,FklSymbolTable* table)
{
	fklPrin1VMvalue(v,fp,table);
}

FklVMframe* fklHasSameProc(uint32_t scp,FklVMframe* frames)
{
	for(;frames;frames=frames->prev)
		if(fklGetCompoundFrameScp(frames)==scp)
			return frames;
	return NULL;
}

int fklIsTheLastExpress(const FklVMframe* frame,const FklVMframe* same,const FklVM* exe)
{
	if(same==NULL)
		return 0;
	size_t size=0;
	for(;;)
	{
		if(frame->type!=FKL_FRAME_COMPOUND)
			return 0;
		uint8_t* code=fklGetCompoundFrameCode(frame);
		uint32_t i=fklGetCompoundFrameCp(frame)+(code[fklGetCompoundFrameCp(frame)]==FKL_OP_CALL||code[fklGetCompoundFrameCp(frame)]==FKL_OP_TAIL_CALL);
		size=fklGetCompoundFrameScp(frame)+fklGetCompoundFrameCpc(frame);

		for(;i<size;i+=(code[i]==FKL_OP_JMP)?fklGetI64FromByteCode(code+i+sizeof(char))+sizeof(char)+sizeof(int64_t):1)
			if(code[i]!=FKL_OP_POP_TP
					&&code[i]!=FKL_OP_JMP
					&&code[i]!=FKL_OP_POP_R_ENV)
				return 0;
		if(frame==same)
			break;
		frame=frame->prev;
	}
	return 1;
}

FklVMgc* fklCreateVMgc()
{
	FklVMgc* tmp=(FklVMgc*)malloc(sizeof(FklVMgc));
	FKL_ASSERT(tmp);
	tmp->running=0;
	tmp->num=0;
	tmp->threshold=FKL_THRESHOLD_SIZE;
	tmp->head=NULL;
	tmp->grey=NULL;
	tmp->greyNum=0;
	pthread_mutex_init(&tmp->tcMutex,NULL);
	return tmp;
}

//void fklGC_reGrey(FklVMvalue* v,FklVMgc* gc)
//{
//	if(FKL_IS_PTR(v))
//	{
//		v->mark=FKL_MARK_G;
//		pthread_rwlock_wrlock(&gc->greylock);
//		h->grey=createGreylink(v,h->grey);
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
		gc->grey=createGreylink(v,gc->grey);
		gc->greyNum++;
	}
}

void fklGC_markRootToGrey(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMgc* gc=exe->gc;
	if(exe->codeObj)
		fklGC_toGrey(exe->codeObj,gc);
	//pthread_rwlock_rdlock(&exe->rlock);
	for(FklVMframe* cur=exe->frames;cur;cur=cur->prev)
		fklDoAtomicFrame(cur,gc);
	for(size_t i=0;i<exe->libNum;i++)
	{
		fklGC_toGrey(exe->libs[i].libEnv,gc);
		fklGC_toGrey(exe->libs[i].proc,gc);
	}
	//pthread_rwlock_unlock(&exe->rlock);
	//pthread_rwlock_rdlock(&stack->lock);
	for(uint32_t i=0;i<stack->tp;i++)
	{
		FklVMvalue* value=stack->values[i];
		if(FKL_IS_PTR(value))
			fklGC_toGrey(value,gc);
	}
	//pthread_rwlock_unlock(&stack->lock);
	if(exe->chan)
		fklGC_toGrey(exe->chan,gc);
}

void fklGC_markAllRootToGrey(FklVM* curVM)
{
	for(FklVM* cur=curVM->prev;cur;)
	{
		uint32_t mark=cur->mark;
		if(mark)
		{
			fklGC_markRootToGrey(cur);
			cur=cur->prev;
		}
		else
		{
			FklVM* t=cur;
			pthread_join(cur->tid,NULL);
			if(cur->prev)
				cur->prev->next=cur->next;
			if(cur->next)
				cur->next->prev=cur->prev;
			cur=cur->prev;
			free(t->libs);
			free(t);
		}
	}
	for(FklVM* cur=curVM;cur;)
	{
		uint32_t mark=cur->mark;
		if(mark)
		{
			fklGC_markRootToGrey(cur);
			cur=cur->next;
		}
		else
		{
			FklVM* t=cur;
			pthread_join(cur->tid,NULL);
			if(cur->prev)
				cur->prev->next=cur->next;
			if(cur->next)
				cur->next->prev=cur->prev;
			cur=cur->next;
			free(t->libs);
			free(t);
		}
	}
}

void fklGC_pause(FklVM* exe)
{
	fklGC_markAllRootToGrey(exe);
}

void propagateMark(FklVMvalue* root,FklVMgc* gc)
{
	FKL_ASSERT(root->type<=FKL_TYPE_CODE_OBJ);
	static void(* const fkl_atomic_value_method_table[])(FklVMvalue*,FklVMgc*)=
	{
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		fklAtomicVMvec,
		fklAtomicVMpair,
		fklAtomicVMbox,
		NULL,
		fklAtomicVMuserdata,
		fklAtomicVMproc,
		fklAtomicVMchan,
		NULL,
		fklAtomicVMdll,
		fklAtomicVMdlproc,
		NULL,
		fklAtomicVMenv,
		fklAtomicVMhashTable,
		NULL,
	};
	void (*atomic_value_func)(FklVMvalue*,FklVMgc*)=fkl_atomic_value_method_table[root->type];
	if(atomic_value_func)
		atomic_value_func(root,gc);
}

int fklGC_propagate(FklVMgc* gc)
{
	Greylink* g=gc->grey;
	FklVMvalue* v=FKL_VM_NIL;
	if(g)
	{
		gc->greyNum--;
		gc->grey=g->next;
		v=g->v;
		free(g);
	}
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
		fklDestroyVMvalue(cur);
	}
}

inline void fklGetGCstateAndGCNum(FklVMgc* gc,FklGCstate* s,int* cr)
{
	*s=gc->running;
	*cr=gc->num>gc->threshold;
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
				size_t create_n=exe->gc->greyNum-greyNum;
				size_t timce=exe->gc->greyNum/FKL_GC_GRAY_FACTOR+create_n/FKL_GC_NEW_FACTOR+1;
				greyNum+=create_n;
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
	fklGC_markAllRootToGrey(exe);
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

void fklDestroyAllValues(FklVMgc* gc)
{
	FklVMvalue** phead=&gc->head;
	FklVMvalue* destroyDll=NULL;
	while(*phead!=NULL)
	{
		FklVMvalue* cur=*phead;
		if(FKL_IS_PTR(cur))
		{
			*phead=cur->next;
			if(FKL_IS_DLL(cur))
			{
				cur->next=destroyDll;
				destroyDll=cur;
			}
			else
				fklDestroyVMvalue(cur);
			gc->num-=1;
		}
		else
		{
			cur->mark=FKL_MARK_W;
			phead=&cur->next;
		}
	}
	phead=&destroyDll;
	while(*phead!=NULL)
	{
		FklVMvalue* cur=*phead;
		*phead=cur->next;
		fklDestroyVMvalue(cur);
	}
}

FklVM* fklCreateThreadVM(FklVMgc* gc
		,FklVMvalue* nextCall
		,FklVM* prev
		,FklVM* next
		,size_t libNum
		,FklVMlib* libs
		,FklSymbolTable* table
		,FklSid_t* builtinErrorTypeId)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	exe->mark=1;
	exe->chan=fklCreateSaveVMvalue(FKL_TYPE_CHAN,fklCreateVMchanl(0));
	exe->stack=fklCreateVMstack(0);
	exe->gc=gc;
	exe->symbolTable=table;
	exe->libNum=libNum;
	exe->builtinErrorTypeId=builtinErrorTypeId;
	exe->libs=fklCopyMemory(libs,libNum*sizeof(FklVMlib));
	exe->codeObj=prev->codeObj;
	exe->frames=NULL;
	fklCallobj(nextCall,NULL,exe);
	insert_to_VM_chain(exe,prev,next,gc);
	fklAddToGCNoGC(exe->chan,gc);
	return exe;
}


void fklDestroyVMstack(FklVMstack* stack)
{
//	pthread_rwlock_destroy(&stack->lock);
	fklUninitUintStack(&stack->bps);
	fklUninitUintStack(&stack->tps);
	free(stack->values);
	free(stack);
}

//void fklDestroyAllVMs(FklVM* node)
//{
//	for(FklVMnode** ph=&GlobVMs->h;*ph!=node;)
//	{
//		FklVMnode* cur=*ph;
//		*ph=cur->next;
//		if(cur->vm->mark)
//		{
//			fklDeleteCallChain(cur->vm);
//			fklDestroyVMstack(cur->vm->stack);
//			fklDestroyPtrStack(cur->vm->tstack);
//		}
//		free(cur->vm);
//		free(cur);
//	}
//	pthread_rwlock_destroy(&GlobVMs->lock);
//}

void fklDestroyAllVMs(FklVM* curVM)
{
	size_t libNum=curVM->libNum;
	FklVMlib* libs=curVM->libs;
	for(size_t i=0;i<libNum;i++)
		fklUninitVMlib(&libs[i]);
	free(curVM->builtinErrorTypeId);
	for(FklVM* prev=curVM->prev;prev;)
	{
		if(prev->mark)
		{
			fklDeleteCallChain(prev);
			fklDestroyVMstack(prev->stack);
		}
		FklVM* t=prev;
		prev=prev->prev;
		free(t->libs);
		free(t);
	}
	for(FklVM* cur=curVM;cur;)
	{
		if(cur->mark)
		{
			fklDeleteCallChain(cur);
			fklDestroyVMstack(cur->stack);
		}
		FklVM* t=cur;
		cur=cur->next;
		free(t->libs);
		free(t);
	}
}

void fklDestroyVMgc(FklVMgc* gc)
{
	fklWaitGC(gc);
	fklDestroyAllValues(gc);
	free(gc);
}

void fklJoinAllThread(FklVM* curVM)
{
	for(FklVM* cur=curVM->prev;cur;cur=cur->prev)
		pthread_join(cur->tid,NULL);
	for(FklVM* cur=curVM->next;cur;cur=cur->next)
		pthread_join(cur->tid,NULL);
	curVM->mark=1;
}

void fklDeleteCallChain(FklVM* exe)
{
	while(exe->frames)
	{
		FklVMframe* cur=exe->frames;
		exe->frames=cur->prev;
		fklDestroyVMframe(cur);
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

void fklDestroyVMframes(FklVMframe* h)
{
	while(h)
	{
		FklVMframe* cur=h;
		h=h->prev;
		cur->u.c.localenv=NULL;
		free(cur);
	}
}

void fklInitVMlib(FklVMlib* lib,size_t exportNum,FklSid_t* exports,FklVMvalue* proc)
{
	lib->exportNum=exportNum;
	lib->exports=exports;
	lib->proc=proc;
	lib->libEnv=FKL_VM_NIL;
}

FklVMlib* fklCreateVMlib(size_t exportNum,FklSid_t* exports,FklVMvalue* proc)
{
	FklVMlib* lib=(FklVMlib*)malloc(sizeof(FklVMlib));
	FKL_ASSERT(lib);
	fklInitVMlib(lib,exportNum,exports,proc);
	return lib;
}

void fklUninitVMlib(FklVMlib* lib)
{
	free(lib->exports);
}

void fklDestroyVMlib(FklVMlib* lib)
{
	fklUninitVMlib(lib);
	free(lib);
}

unsigned int fklGetCompoundFrameMark(const FklVMframe* f)
{
	return f->u.c.mark;
}

unsigned int fklSetCompoundFrameMark(FklVMframe* f,unsigned int m)
{
	f->u.c.mark=m;
	return m;
}

FklVMvalue* fklGetCompoundFrameLocalenv(const FklVMframe* f)
{
	return f->u.c.localenv;
}

FklVMvalue* fklSetCompoundFrameLocalenv(FklVMframe* f,FklVMvalue* env)
{
	f->u.c.localenv=env;
	return env;
}

FklVMvalue* fklGetCompoundFrameCodeObj(const FklVMframe* f)
{
	return f->u.c.codeObj;
}

uint8_t* fklGetCompoundFrameCode(const FklVMframe* f)
{
	return f->u.c.code;
}

uint64_t fklGetCompoundFrameScp(const FklVMframe* f)
{
	return f->u.c.scp;
}


uint64_t fklGetCompoundFrameCp(const FklVMframe* f)
{
	return f->u.c.cp;
}

uint64_t fklAddCompoundFrameCp(FklVMframe* f,int64_t a)
{
	f->u.c.cp+=a;
	return f->u.c.cp;
}

uint64_t fklIncCompoundFrameCp(FklVMframe* f)
{
	f->u.c.cp++;
	return f->u.c.cp;
}

uint64_t fklIncAndAddCompoundFrameCp(FklVMframe* f,int64_t a)
{
	f->u.c.cp+=1+a;
	return f->u.c.cp;
}

uint64_t fklSetCompoundFrameCp(FklVMframe* f,uint64_t a)
{
	f->u.c.cp=a;
	return f->u.c.cp;
}

uint64_t fklResetCompoundFrameCp(FklVMframe* f)
{
	f->u.c.cp=f->u.c.scp;
	return f->u.c.scp;
}

uint64_t fklGetCompoundFrameCpc(const FklVMframe* f)
{
	return f->u.c.cpc;
}

FklSid_t fklGetCompoundFrameSid(const FklVMframe* f)
{
	return f->u.c.sid;
}

int fklIsCompoundFrameReachEnd(FklVMframe* f)
{
	if(f->u.c.cp>=(f->u.c.scp+f->u.c.cpc))
	{
		if(f->u.c.mark)
		{
			f->u.c.cp=f->u.c.scp;
			f->u.c.mark=0;
			return 0;
		}
		return 1;
	}
	return 0;
}

