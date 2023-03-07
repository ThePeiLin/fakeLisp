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
static void callCompoundProcdure(FklVM* exe,FklVMvalue* proc,FklVMframe* frame)
{
	FklVMframe* tmpFrame=fklCreateVMframeWithProcValue(proc,exe->frames);
	FklPrototype* pt=&exe->ptpool->pts[proc->u.proc->protoId-1];
	uint32_t lcount=pt->lcount;
	FklVMvalue** loc=(FklVMvalue**)calloc(lcount,sizeof(FklVMvalue*));
	FKL_ASSERT(loc);
	FklVMCompoundFrameVarRef* f=&tmpFrame->u.c.lr;
	f->loc=loc;
	f->lcount=lcount;
	fklPushVMframe(tmpFrame,exe);
}

inline void fklSwapCompoundFrame(FklVMframe* a,FklVMframe* b)
{
	if(a!=b)
	{
		FklVMCompoundFrameData t=a->u.c;
		a->u.c=b->u.c;
		b->u.c=t;
	}
}

static int is_last_expression(FklVMframe* frame)
{
	if(frame->type!=FKL_FRAME_COMPOUND)
		return 0;
	else if(!frame->u.c.tail)
	{
		uint8_t* pc=fklGetCompoundFrameCode(frame);
		uint8_t* end=fklGetCompoundFrameEnd(frame);

		for(;pc<end;pc+=(*pc==FKL_OP_JMP)?1+fklGetI64FromByteCode(pc+1)+sizeof(int64_t):1)
			if(*pc!=FKL_OP_JMP)
				return 0;
		frame->u.c.tail=1;
	}
	return 1;
}

//inline static void print_link_back_trace(FklVMframe* t,FklSymbolTable* table)
//{
//	if(t->type==FKL_FRAME_COMPOUND)
//	{
//		if(t->u.c.sid)
//			fklPrintString(fklGetSymbolWithId(t->u.c.sid,table)->symbol,stderr);
//		else
//			fputs("<lambda>",stderr);
//		fprintf(stderr,"[%u,%u]",t->u.c.mark,t->u.c.tail);
//	}
//	else
//		fputs("<obj>",stderr);
//
//	for(FklVMframe* cur=t->prev;cur;cur=cur->prev)
//	{
//		fputs(" --> ",stderr);
//		if(cur->type==FKL_FRAME_COMPOUND)
//		{
//			if(cur->u.c.sid)
//				fklPrintString(fklGetSymbolWithId(cur->u.c.sid,table)->symbol,stderr);
//			else
//				fputs("<lambda>",stderr);
//		fprintf(stderr,"[%u,%u]",cur->u.c.mark,cur->u.c.tail);
//		}
//		else
//			fputs("<obj>",stderr);
//	}
//	fputc('\n',stderr);
//}

static void tailCallCompoundProcdure(FklVM* exe,FklVMvalue* proc,FklVMframe* frame)
{
	FklVMframe* topframe=frame;
	topframe->u.c.tail=1;
	if(fklGetCompoundFrameProc(frame)==proc)
		frame->u.c.mark=1;
	else if((frame=fklHasSameProc(proc,frame->prev))&&(topframe->u.c.tail&=frame->u.c.tail))
	{
		frame->u.c.mark=1;
		fklSwapCompoundFrame(topframe,frame);
	}
	else
	{
		FklVMframe* tmpFrame=fklCreateVMframeWithProcValue(proc,exe->frames->prev);
		FklPrototype* pt=&exe->ptpool->pts[proc->u.proc->protoId-1];
		uint32_t lcount=pt->lcount;
		FklVMvalue** loc=(FklVMvalue**)calloc(lcount,sizeof(FklVMvalue*));
		FKL_ASSERT(loc);
		FklVMCompoundFrameVarRef* f=&tmpFrame->u.c.lr;
		f->loc=loc;
		f->lcount=lcount;
		fklPushVMframe(tmpFrame,exe);
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

static void dlproc_frame_print_backtrace(FklCallObjData data,FILE* fp,FklSymbolTable* table)
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

static void dlproc_frame_atomic(FklCallObjData data,FklVMgc* gc)
{
	DlprocFrameContext* c=(DlprocFrameContext*)data;
	fklGC_toGrey(c->proc,gc);
}

static void dlproc_frame_finalizer(FklCallObjData data)
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

static void dlproc_frame_copy(FklCallObjData d,const FklCallObjData s,FklVM* exe)
{
	DlprocFrameContext const* const sc=(DlprocFrameContext*)s;
	DlprocFrameContext* dc=(DlprocFrameContext*)d;
	FklVMgc* gc=exe->gc;
	dc->state=sc->state;
	fklSetRef(&dc->proc,sc->proc,gc);
	dlproc_ccc_copy(&sc->ccc,&dc->ccc);
}

static int dlproc_frame_end(FklCallObjData data)
{
	DlprocFrameContext* c=(DlprocFrameContext*)data;
	return c->state==DLPROC_DONE;
}

static void dlproc_frame_step(FklCallObjData data,FklVM* exe)
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

inline static void initDlprocFrameContext(FklCallObjData data,FklVMvalue* proc,FklVMgc* gc)
{
	DlprocFrameContext* c=(DlprocFrameContext*)data;
	fklSetRef(&c->proc,proc,gc);
	c->state=DLPROC_READY;
	initVMcCC(&c->ccc,NULL,NULL,0);
}

void callDlProc(FklVM* exe,FklVMvalue* dlproc)
{
	FklVMframe* f=&exe->sf;
	f->type=FKL_FRAME_OTHEROBJ;
	f->u.o.t=&DlprocContextMethodTable;
	f->errorCallBack=NULL;
	initDlprocFrameContext(f->u.o.data,dlproc,exe->gc);
	fklPushVMframe(f,exe);
}

/*--------------------------*/

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
static void B_dup(BYTE_CODE_ARGS);
static void B_push_proc(BYTE_CODE_ARGS);
static void B_drop(BYTE_CODE_ARGS);
static void B_pop_arg(BYTE_CODE_ARGS);
static void B_pop_rest_arg(BYTE_CODE_ARGS);
static void B_set_bp(BYTE_CODE_ARGS);
static void B_call(BYTE_CODE_ARGS);
static void B_res_bp(BYTE_CODE_ARGS);
static void B_jmp_if_true(BYTE_CODE_ARGS);
static void B_jmp_if_false(BYTE_CODE_ARGS);
static void B_jmp(BYTE_CODE_ARGS);
static void B_list_append(BYTE_CODE_ARGS);
static void B_push_vector(BYTE_CODE_ARGS);
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
static void B_push_0(BYTE_CODE_ARGS);
static void B_push_1(BYTE_CODE_ARGS);
static void B_push_i8(BYTE_CODE_ARGS);
static void B_push_i16(BYTE_CODE_ARGS);
static void B_push_i64_big(BYTE_CODE_ARGS);
static void B_get_loc(BYTE_CODE_ARGS);
static void B_put_loc(BYTE_CODE_ARGS);
static void B_get_var_ref(BYTE_CODE_ARGS);
static void B_put_var_ref(BYTE_CODE_ARGS);
static void B_export(BYTE_CODE_ARGS);
static void B_load_lib(BYTE_CODE_ARGS);
static void B_load_dll(BYTE_CODE_ARGS);
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
	B_dup,
	B_push_proc,
	B_drop,
	B_pop_arg,
	B_pop_rest_arg,
	B_set_bp,
	B_call,
	B_res_bp,
	B_jmp_if_true,
	B_jmp_if_false,
	B_jmp,
	B_list_append,
	B_push_vector,
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
	B_push_0,
	B_push_1,
	B_push_i8,
	B_push_i16,
	B_push_i64_big,
	B_get_loc,
	B_put_loc,
	B_get_var_ref,
	B_put_var_ref,
	B_export,
	B_load_lib,
	B_load_dll,
};

inline static void insert_to_VM_chain(FklVM* cur,FklVM* prev,FklVM* next,FklVMgc* gc)
{
	cur->prev=prev;
	cur->next=next;
	if(prev)
		prev->next=cur;
	if(next)
		next->prev=cur;
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
	exe->importingLib=NULL;
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
	exe->stack=fklCreateVMstack(32);
	exe->libNum=0;
	exe->libs=NULL;
	exe->ptpool=NULL;
	insert_to_VM_chain(exe,prev,next,exe->gc);
	return exe;
}

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
	void (*backtrace)(FklCallObjData data,FILE*,FklSymbolTable*)=f->u.o.t->print_backtrace;
	if(backtrace)
		backtrace(f->u.o.data,fp,table);
	else
		fprintf(fp,"at callable-obj\n");
}

inline void fklDoCallableObjFrameStep(FklVMframe* f,FklVM* exe)
{
	f->u.o.t->step(fklGetFrameData(f),exe);
}

inline void fklDoFinalizeObjFrame(FklVMframe* f,FklVMframe* sf)
{
	f->u.o.t->finalizer(fklGetFrameData(f));
	if(f!=sf)
		free(f);
}

static inline void close_var_ref(FklVMvarRef* ref)
{
	ref->v=*(ref->ref);
	ref->ref=&ref->v;
}

inline void fklDoUninitCompoundFrame(FklVMframe* frame)
{
	FklVMCompoundFrameVarRef* lr=&frame->u.c.lr;
	for(FklVMvarRefList* l=lr->lrefl;l;)
	{
		close_var_ref(l->ref);
		fklDestroyVMvarRef(l->ref);
		FklVMvarRefList* c=l;
		l=c->next;
		free(c);
	}
	free(lr->lref);
	free(lr->loc);
}

inline void fklDoFinalizeCompoundFrame(FklVMframe* frame)
{
	fklDoUninitCompoundFrame(frame);
	free(frame);
}

inline void fklDoCopyObjFrameContext(FklVMframe* s,FklVMframe* d,FklVM* exe)
{
	s->u.o.t->copy(fklGetFrameData(d),fklGetFrameData(s),exe);
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

inline static void compound_frame_loc_ref_to_gray(FklVMframe* frame,FklVMgc* gc)
{
	FklVMCompoundFrameVarRef* f=&frame->u.c.lr;
	uint32_t count=f->lcount;
	FklVMvalue** loc=f->loc;
	for(uint32_t i=0;i<count;i++)
		if(loc[i])
			fklGC_toGrey(f->loc[i],gc);
}

void fklDoAtomicFrame(FklVMframe* f,FklVMgc* gc)
{
	switch(f->type)
	{
		case FKL_FRAME_COMPOUND:
			fklGC_toGrey(fklGetCompoundFrameProc(f),gc);
			compound_frame_loc_ref_to_gray(f,gc);
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
			v->u.ud->t->__call(v->u.ud,exe);
			break;
		default:
			break;
	}
}

inline static void applyCompoundProc(FklVM* exe,FklVMvalue* proc,FklVMframe* frame)
{
	FklVMframe* prevProc=fklHasSameProc(proc,exe->frames);
	if(frame&&frame->type==FKL_FRAME_COMPOUND
			&&(frame->u.c.tail=is_last_expression(frame))
			&&prevProc
			&&(frame->u.c.tail&=prevProc->u.c.tail))
	{
		prevProc->u.c.mark=1;
		fklSwapCompoundFrame(frame,prevProc);
	}
	else
	{
		FklVMframe* tmpFrame=fklCreateVMframeWithProcValue(proc,exe->frames);
		FklPrototype* pt=&exe->ptpool->pts[proc->u.proc->protoId-1];
		uint32_t lcount=pt->lcount;
		FklVMvalue** loc=(FklVMvalue**)calloc(lcount,sizeof(FklVMvalue*));
		FKL_ASSERT(loc);
		FklVMCompoundFrameVarRef* f=&tmpFrame->u.c.lr;
		f->loc=loc;
		f->lcount=lcount;
		fklPushVMframe(tmpFrame,exe);
	}
}

void fklCallObj(FklVMvalue* proc,FklVMframe* frame,FklVM* exe)
{
	switch(proc->type)
	{
		case FKL_TYPE_PROC:
			applyCompoundProc(exe,proc,frame);
			break;
		default:
			callCallableObj(proc,exe);
			break;
	}
}

void fklTailCallObj(FklVMvalue* proc,FklVMframe* frame,FklVM* exe)
{
	exe->frames=frame->prev;
	fklDoFinalizeObjFrame(frame,&exe->sf);
	fklCallObj(proc,exe->frames,exe);
}

inline void fklCallFuncK(FklVMFuncK kf,FklVM* v,void* ctx)
{
	FklVMframe* sf=v->frames;
	FklVMframe* nf=fklCreateOtherObjVMframe(sf->u.o.t,sf->prev);
	nf->errorCallBack=sf->errorCallBack;
	fklDoCopyObjFrameContext(sf,nf,v);
	v->frames=nf;
	kf(v,FKL_CC_OK,ctx);
}

void fklCallInDlproc(FklVMvalue* proc
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
	fklNiSetBpWithTp(stack);
	for(size_t i=argNum;i>0;i--)
		fklPushVMvalue(arglist[i-1],stack);
	switch(proc->type)
	{
		case FKL_TYPE_PROC:
			callCompoundProcdure(exe,proc,exe->frames);
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
	ByteCodes[fklGetCompoundFrameOpAndInc(curframe)](exe,curframe);
}

int fklRunReplVM(FklVM* exe)
{
	int r=setjmp(exe->buf);
	if(r==1)
	{
		fklTcMutexRelease(exe->gc);
		return 255;
	}
	else if(r==2)
	{
		fklTcMutexRelease(exe->gc);
		return 0;
	}
	FklVMframe* sf=&exe->sf;
	while(exe->frames)
	{
		fklTcMutexAcquire(exe->gc);
		FklVMframe* curframe=exe->frames;
		switch(curframe->type)
		{
			case FKL_FRAME_COMPOUND:
				if(fklIsCompoundFrameReachEnd(curframe))
				{
					if(curframe->prev)
						fklDoFinalizeCompoundFrame(popFrame(exe));
					else
						longjmp(exe->buf,2);
				}
				else
					fklDoCompoundFrameStep(curframe,exe);
				break;
			case FKL_FRAME_OTHEROBJ:
				if(fklIsCallableObjFrameReachEnd(curframe))
					fklDoFinalizeObjFrame(popFrame(exe),sf);
				else
					fklDoCallableObjFrameStep(curframe,exe);
				break;
		}
		fklTcMutexRelease(exe->gc);
	}
	return 0;
}

int fklRunVM(FklVM* exe)
{
	if(setjmp(exe->buf)==1)
	{
		fklTcMutexRelease(exe->gc);
		return 255;
	}
	FklVMframe* sf=&exe->sf;
	while(exe->frames)
	{
		fklTcMutexAcquire(exe->gc);
		FklVMframe* curframe=exe->frames;
		switch(curframe->type)
		{
			case FKL_FRAME_COMPOUND:
				if(fklIsCompoundFrameReachEnd(curframe))
					fklDoFinalizeCompoundFrame(popFrame(exe));
				else
					fklDoCompoundFrameStep(curframe,exe);
				break;
			case FKL_FRAME_OTHEROBJ:
				if(fklIsCallableObjFrameReachEnd(curframe))
					fklDoFinalizeObjFrame(popFrame(exe),sf);
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

static void inline B_dummy(FklVM* exe,FklVMframe* frame)
{
	FKL_ASSERT(0);
}

static void inline B_push_nil(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(FKL_VM_NIL,exe->stack);
}

static void inline B_push_pair(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* cdr=fklNiGetArg(&ap,stack);
	FklVMvalue* car=fklNiGetArg(&ap,stack);
	fklNiReturn(fklCreateVMpairV(car,cdr,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_push_i32(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(FKL_MAKE_VM_FIX(fklGetI32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(int32_t)))),exe->stack);
}

static void inline B_push_i64(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(FKL_MAKE_VM_FIX(fklGetI64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(int64_t)))),exe->stack);
}

static void inline B_push_chr(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(FKL_MAKE_VM_CHR(*(char*)(fklGetCompoundFrameCodeAndAdd(frame,sizeof(char)))),exe->stack);
}

static void inline B_push_f64(FklVM* exe,FklVMframe* frame)
{
	fklCreateVMvalueToStack(FKL_TYPE_F64,fklGetCompoundFrameCodeAndAdd(frame,sizeof(double)),exe);
}

static void inline B_push_str(FklVM* exe,FklVMframe* frame)
{
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	fklCreateVMvalueToStack(FKL_TYPE_STR,fklCreateString(size,(char*)fklGetCompoundFrameCodeAndAdd(frame,size)),exe);
}

static void inline B_push_sym(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(FKL_MAKE_VM_SYM(fklGetSidFromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(FklSid_t)))),exe->stack);
}

static void inline B_dup(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	if(stack->tp==stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.dup",FKL_ERR_STACKERROR,exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	fklNiReturn(val,&ap,stack);
	fklNiReturn(val,&ap,stack);
	fklNiEnd(&ap,stack);
}

inline FklVMvarRef* fklMakeVMvarRefRef(FklVMvarRef* ref)
{
	ref->refc++;
	return ref;
}

inline void fklDestroyVMvarRef(FklVMvarRef* ref)
{
	if(ref->refc)
		ref->refc--;
	else
		free(ref);
}

inline FklVMvarRef* fklCreateVMvarRef(FklVMvalue** loc,uint32_t idx)
{
	FklVMvarRef* ref=(FklVMvarRef*)malloc(sizeof(FklVMvarRef));
	FKL_ASSERT(ref);
	ref->ref=&loc[idx];
	ref->v=NULL;
	ref->refc=0;
	ref->idx=idx;
	return ref;
}

inline FklVMvarRef* fklCreateClosedVMvarRef(FklVMvalue* v)
{
	FklVMvarRef* ref=(FklVMvarRef*)malloc(sizeof(FklVMvarRef));
	FKL_ASSERT(ref);
	ref->refc=0;
	ref->v=v;
	ref->ref=&ref->v;
	return ref;
}

static inline void inc_lref(FklVMCompoundFrameVarRef* lr,uint32_t lcount)
{
	if(!lr->lref)
	{
		lr->lref=(FklVMvarRef**)calloc(lcount,sizeof(FklVMvarRef*));
		FKL_ASSERT(lr->lref);
	}
}

static inline FklVMvarRef* insert_local_ref(FklVMCompoundFrameVarRef* lr,FklVMvarRef* ref)
{
	FklVMvarRefList* rl=(FklVMvarRefList*)malloc(sizeof(FklVMvarRefList));
	FKL_ASSERT(rl);
	rl->ref=fklMakeVMvarRefRef(ref);
	rl->next=lr->lrefl;
	lr->lrefl=rl;
	return ref;
}

static inline FklVMproc* createVMproc(uint8_t* spc
		,uint64_t cpc
		,FklVMvalue* codeObj
		,FklVMCompoundFrameVarRef* lr
		,uint32_t protoId
		,FklVM* exe)
{
	FklVMgc* gc=exe->gc;
	FklVMproc* proc=fklCreateVMproc(spc,cpc,codeObj,gc);
	proc->protoId=protoId;
	FklPrototype* pt=&exe->ptpool->pts[protoId-1];
	uint32_t count=pt->rcount;
	if(count)
	{
		FklVMvarRef** ref=lr->ref;
		FklVMvarRef** closure=(FklVMvarRef**)malloc(sizeof(FklVMvarRef*)*count);
		FKL_ASSERT(closure);
		for(uint32_t i=0;i<count;i++)
		{
			FklSymbolDef* c=&pt->refs[i];
			if(c->isLocal)
			{
				inc_lref(lr,lr->lcount);
				if(lr->lref[c->cidx])
					closure[i]=fklMakeVMvarRefRef(lr->lref[c->cidx]);
				else
					closure[i]=insert_local_ref(lr,fklCreateVMvarRef(lr->loc,c->cidx));
			}
			else
			{
				if(c->cidx>=lr->rcount)
					closure[i]=fklCreateClosedVMvarRef(NULL);
				else
					closure[i]=fklMakeVMvarRefRef(ref[c->cidx]);
			}
		}
		proc->closure=closure;
		proc->count=count;
	}
	return proc;
}

inline static FklVMvalue* get_compound_frame_code_obj(FklVMframe* frame)
{
	return frame->u.c.proc->u.proc->codeObj;
}

static void inline B_push_proc(FklVM* exe,FklVMframe* frame)
{
	uint32_t prototypeId=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(prototypeId)));
	uint64_t sizeOfProc=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(sizeOfProc)));
	FklVMproc* code=createVMproc(fklGetCompoundFrameCode(frame)
			,sizeOfProc
			,get_compound_frame_code_obj(frame)
			,fklGetCompoundFrameLocRef(frame)
			,prototypeId
			,exe);
	fklCreateVMvalueToStack(FKL_TYPE_PROC,code,exe);
	fklAddCompoundFrameCp(frame,sizeOfProc);
}

void inline fklDropTop(FklVMstack* s)
{
	s->tp-=s->tp>0;
}

static void inline B_drop(FklVM* exe,FklVMframe* frame)
{
	fklDropTop(exe->stack);
}

static inline FklVMvalue* volatile* get_compound_frame_loc(FklVMframe* frame,uint32_t idx,FklVM* exe)
{
	FklVMvalue** loc=frame->u.c.lr.loc;
	return &loc[idx];
}

static void inline B_pop_arg(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	if(ap<=stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.pop-arg",FKL_ERR_TOOFEWARG,exe);
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint32_t)));
	FklVMvalue* v=fklNiGetArg(&ap,stack);
	*get_compound_frame_loc(frame,idx,exe)=v;
	fklNiEnd(&ap,stack);
	fklNiDoSomeAfterSetLoc(v,idx,frame,exe);
}

static void inline B_pop_rest_arg(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint32_t)));
	FklVMvalue* obj=FKL_VM_NIL;
	FklVMvalue* volatile* pValue=&obj;
	for(;ap>stack->bp;pValue=&(*pValue)->u.pair->cdr)
		*pValue=fklCreateVMpairV(fklNiGetArg(&ap,stack),FKL_VM_NIL,exe);
	*get_compound_frame_loc(frame,idx,exe)=obj;
	fklNiEnd(&ap,stack);
}

static void inline B_set_bp(FklVM* exe,FklVMframe* frame)
{
	fklNiSetBpWithTp(exe->stack);
}

inline FklVMvalue* fklPopTopValue(FklVMstack* s)
{
	return s->values[--s->tp];
}

static void inline B_res_bp(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	if(stack->tp>stack->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.res-bp",FKL_ERR_TOOMANYARG,exe);
	stack->bp=FKL_GET_FIX(fklPopTopValue(stack));
}

static void inline B_call(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* tmpValue=fklNiGetArg(&ap,stack);
	if(!fklIsCallable(tmpValue))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.call",FKL_ERR_CALL_ERROR,exe);
	switch(tmpValue->type)
	{
		case FKL_TYPE_PROC:
			callCompoundProcdure(exe,tmpValue,frame);
			break;
		default:
			callCallableObj(tmpValue,exe);
			break;
	}
	fklNiEnd(&ap,stack);
}

static void inline B_tail_call(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* tmpValue=fklNiGetArg(&ap,stack);
	if(!fklIsCallable(tmpValue))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.tail-call",FKL_ERR_CALL_ERROR,exe);
	switch(tmpValue->type)
	{
		case FKL_TYPE_PROC:
			tailCallCompoundProcdure(exe,tmpValue,frame);
			break;
		default:
			callCallableObj(tmpValue,exe);
			break;
	}
	fklNiEnd(&ap,stack);
}

static void inline B_jmp_if_true(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	if(stack->tp&&fklGetTopValue(stack)!=FKL_VM_NIL)
		fklAddCompoundFrameCp(frame,fklGetI64FromByteCode(fklGetCompoundFrameCode(frame)));
	fklAddCompoundFrameCp(frame,sizeof(int64_t));
}

static void inline B_jmp_if_false(FklVM* exe,FklVMframe* frame)
{
	FklVMstack* stack=exe->stack;
	if(stack->tp&&fklGetTopValue(stack)==FKL_VM_NIL)
		fklAddCompoundFrameCp(frame,fklGetI64FromByteCode(fklGetCompoundFrameCode(frame)));
	fklAddCompoundFrameCp(frame,sizeof(int64_t));
}

static void inline B_jmp(FklVM* exe,FklVMframe* frame)
{
	fklAddCompoundFrameCp(frame
			,fklGetI64FromByteCode(fklGetCompoundFrameCode(frame))+sizeof(int64_t));
}

static void inline B_list_append(FklVM* exe,FklVMframe* frame)
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
}

static void inline B_push_vector(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	FklVMvalue* vec=fklCreateVMvecV(size,NULL,exe);
	for(size_t i=size;i>0;i--)
		fklSetRef(&vec->u.vec->base[i-1],fklNiGetArg(&ap,stack),exe->gc);
	fklNiReturn(vec
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_push_big_int(FklVM* exe,FklVMframe* frame)
{
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(num)));
	FklBigInt* bigInt=fklCreateBigIntFromMem(fklGetCompoundFrameCodeAndAdd(frame,num),sizeof(uint8_t)*num);
	fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bigInt,exe);
}

static void inline B_push_box(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	FklVMvalue* box=fklCreateVMvalueToStack(FKL_TYPE_BOX,FKL_VM_NIL,exe);
	fklSetRef(&box->u.box,c,exe->gc);
	fklNiReturn(box,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_push_bytevector(FklVM* exe,FklVMframe* frame)
{
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	fklCreateVMvalueToStack(FKL_TYPE_BYTEVECTOR,fklCreateBytevector(size,fklGetCompoundFrameCodeAndAdd(frame,size)),exe);
}

static void inline B_push_hash_eq(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
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
}

static void inline B_push_hash_eqv(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
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
}

static void inline B_push_hash_equal(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
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
}

static void inline B_push_list_0(FklVM* exe,FklVMframe* frame)
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
}

static void inline B_push_list(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
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
}

static void inline B_push_vector_0(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	size_t size=ap-stack->bp;
	FklVMvalue* vec=fklCreateVMvecV(size,NULL,exe);
	for(size_t i=size;i>0;i--)
		fklSetRef(&vec->u.vec->base[i-1],fklNiGetArg(&ap,stack),exe->gc);
	fklNiResBp(&ap,stack);
	fklNiReturn(vec,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_list_push(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* list=fklNiGetArg(&ap,stack);
	for(;FKL_IS_PAIR(list);list=list->u.pair->cdr)
		fklNiReturn(list->u.pair->car,&ap,stack);
	if(list!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.list-push",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiEnd(&ap,stack);
}

static inline void init_import_env(FklVMframe* frame,FklVMlib* plib,FklVM* exe)
{
	fklAddCompoundFrameCp(frame,-1);
	callCompoundProcdure(exe,plib->proc,frame);
}

static void inline B_import(FklVM* exe,FklVMframe* frame)
{
	uint32_t locIdx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(locIdx)));
	uint32_t libIdx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(libIdx)));
	FklVMlib* plib=exe->importingLib;
	FklVMvalue* v=plib->loc[libIdx];
	if(v)
		*get_compound_frame_loc(frame,locIdx,exe)=v;
	else
	{
		FklPrototype* pt=&exe->ptpool->pts[plib->proc->u.proc->protoId-1];
		FklSid_t sid=pt->loc[libIdx].id;
		char* cstr=fklStringToCstr(fklGetSymbolWithId(sid,exe->symbolTable)->symbol);
		if(cstr)
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.import",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
		fklAddCompoundFrameCp(frame,sizeof(uint64_t));
	}
}

static void inline B_load_lib(FklVM* exe,FklVMframe* frame)
{
	uint32_t libId=fklGetU32FromByteCode(fklGetCompoundFrameCode(frame));
	FklVMlib* plib=&exe->libs[libId-1];
	if(plib->imported)
	{
		exe->importingLib=plib;
		fklAddCompoundFrameCp(frame,sizeof(libId));
		fklPushVMvalue(FKL_VM_NIL,exe->stack);
	}
	else
		init_import_env(frame,plib,exe);
}

static inline FklImportDllInitFunc getImportInit(FklDllHandle handle)
{
	return fklGetAddress("_fklImportInit",handle);
}

static void inline B_load_dll(FklVM* exe,FklVMframe* frame)
{
	uint32_t libId=fklGetU32FromByteCode(fklGetCompoundFrameCode(frame));
	FklVMlib* plib=&exe->libs[libId-1];
	if(!plib->imported)
	{
		char* realpath=fklStringToCstr(plib->proc->u.str);
		FklVMdll* dll=fklCreateVMdll(realpath);
		FklImportDllInitFunc initFunc=NULL;
		if(dll)
			initFunc=getImportInit(dll->handle);
		else
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.load-dll",realpath,1,FKL_ERR_IMPORTFAILED,exe);
		if(!initFunc)
		{
			fklDestroyVMdll(dll);
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.load-dll",realpath,1,FKL_ERR_IMPORTFAILED,exe);
		}
		uint32_t tp=exe->stack->tp;
		FklVMvalue* dllv=fklCreateVMvalueToStack(FKL_TYPE_DLL,dll,exe);
		fklInitVMdll(dllv,exe);
		plib->loc=initFunc(exe,dllv,&plib->count);
		plib->imported=1;
		plib->proc=dllv;
		free(realpath);
		exe->stack->tp=tp;
	}
	exe->importingLib=plib;
	fklAddCompoundFrameCp(frame,sizeof(libId));
	fklPushVMvalue(FKL_VM_NIL,exe->stack);
}

static void inline B_push_0(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(FKL_MAKE_VM_FIX(0),exe->stack);
}

static void inline B_push_1(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(FKL_MAKE_VM_FIX(1),exe->stack);
}

static void inline B_push_i8(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(FKL_MAKE_VM_FIX(*(int8_t*)fklGetCompoundFrameCodeAndAdd(frame,sizeof(int8_t))),exe->stack);
}

static void inline B_push_i16(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(FKL_MAKE_VM_FIX(fklGetI16FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(int16_t)))),exe->stack);
}

static void inline B_push_i64_big(FklVM* exe,FklVMframe* frame)
{
	fklCreateVMvalueToStack(FKL_TYPE_BIG_INT
			,fklCreateBigInt(fklGetI64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(int64_t))))
			,exe);
}

static inline FklVMvalue* get_loc(FklVMframe* f,uint32_t idx)
{
	FklVMCompoundFrameVarRef* lr=&f->u.c.lr;
	return lr->loc[idx];
}

static void inline B_get_loc(FklVM* exe,FklVMframe* frame)
{
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(idx)));
	fklPushVMvalue(get_loc(frame,idx),exe->stack);
}

static void inline B_put_loc(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint32_t)));
	FklVMvalue* v=fklNiGetArg(&ap,stack);
	*get_compound_frame_loc(frame,idx,exe)=v;
	fklNiEnd(&ap,stack);
	fklNiDoSomeAfterSetLoc(v,idx,frame,exe);
}

inline static FklVMvalue* get_var_val(FklVMframe* frame,uint32_t idx,FklPrototypePool* ptpool,FklSid_t* psid)
{
	FklVMCompoundFrameVarRef* lr=&frame->u.c.lr;
	FklVMvalue* v=idx<lr->rcount?*(lr->ref[idx]->ref):NULL;
	if(!v)
	{
		FklVMproc* proc=fklGetCompoundFrameProc(frame)->u.proc;
		FklPrototype* pt=&ptpool->pts[proc->protoId-1];
		FklSymbolDef* def=&pt->refs[idx];
		*psid=def->id;
		return NULL;
	}
	return v;
}

static void inline B_get_var_ref(FklVM* exe,FklVMframe* frame)
{
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(idx)));
	FklSid_t id=0;
	FklVMvalue* v=get_var_val(frame,idx,exe->ptpool,&id);
	if(id)
	{
		char* cstr=fklStringToCstr(fklGetSymbolWithId(id,exe->symbolTable)->symbol);
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.get-var-ref",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
	}
	fklPushVMvalue(v,exe->stack);
}

inline static FklVMvalue* volatile* get_var_ref(FklVMframe* frame,uint32_t idx,FklPrototypePool* ptpool,FklSid_t* psid)
{
	FklVMCompoundFrameVarRef* lr=&frame->u.c.lr;
	FklVMvarRef** refs=lr->ref;
	FklVMvalue* volatile* v=(idx>=lr->rcount||!(refs[idx]->ref))?NULL:refs[idx]->ref;
	if(!v)
	{
		FklVMproc* proc=fklGetCompoundFrameProc(frame)->u.proc;
		FklPrototype* pt=&ptpool->pts[proc->protoId-1];
		FklSymbolDef* def=&pt->refs[idx];
		*psid=def->id;
		return NULL;
	}
	return v;
}

static void inline B_put_var_ref(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint32_t)));
	FklSid_t id=0;
	FklVMvalue* volatile* pv=get_var_ref(frame,idx,exe->ptpool,&id);
	if(!pv)
	{
		char* cstr=fklStringToCstr(fklGetSymbolWithId(id,exe->symbolTable)->symbol);
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.put-var-ref",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
	}
	FklVMvalue* v=fklNiGetArg(&ap,stack);
	*pv=v;
	fklNiEnd(&ap,stack);
	fklNiDoSomeAfterSetRef(v,idx,frame,exe);
}

static void inline B_export(FklVM* exe,FklVMframe* frame)
{
	uint32_t libId=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(libId)));
	FklVMlib* lib=&exe->libs[libId-1];
	FklVMCompoundFrameVarRef* lr=fklGetCompoundFrameLocRef(frame);
	uint32_t count=lr->lcount;
	FklVMvalue** loc=fklCopyMemory(lr->loc,sizeof(FklVMvalue*)*count);
	lib->loc=loc;
	lib->count=count;
	lib->imported=1;
}

FklVMstack* fklCreateVMstack(uint32_t size)
{
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->values=(FklVMvalue**)malloc(size*sizeof(FklVMvalue*));
	FKL_ASSERT(tmp->values);
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

FklVMframe* fklHasSameProc(FklVMvalue* proc,FklVMframe* frames)
{
	for(;frames;frames=frames->prev)
		if(frames->type==FKL_FRAME_COMPOUND&&fklGetCompoundFrameProc(frames)==proc)
			return frames;
	return NULL;
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
	//atomic_store(&v->mark,FKL_MARK_G);
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
		FklVMlib* lib=&exe->libs[i];
		fklGC_toGrey(lib->proc,gc);
		for(uint32_t i=0;i<lib->count;i++)
			if(lib->loc[i])
				fklGC_toGrey(lib->loc[i],gc);
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
	static void(* const fkl_atomic_value_method_table[FKL_TYPE_CODE_OBJ+1])(FklVMvalue*,FklVMgc*)=
	{
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
		//atomic_store(&v->mark,FKL_MARK_B);
		propagateMark(v,gc);
	}
	return gc->grey==NULL;
}

void fklGC_collect(FklVMgc* gc,FklVMvalue** pw)
{
	size_t count=0;
	//pthread_rwlock_wrlock(&gc->lock);
	FklVMvalue* head=gc->head;
	gc->head=NULL;
	gc->running=FKL_GC_SWEEPING;
	//pthread_rwlock_unlock(&gc->lock);
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
	//pthread_rwlock_wrlock(&gc->lock);
	*phead=gc->head;
	gc->head=head;
	gc->num-=count;
	//pthread_rwlock_unlock(&gc->lock);
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
	exe->importingLib=NULL;
	exe->mark=1;
	exe->chan=fklCreateSaveVMvalue(FKL_TYPE_CHAN,fklCreateVMchanl(0));
	exe->stack=fklCreateVMstack(32);
	exe->gc=gc;
	exe->symbolTable=table;
	exe->libNum=libNum;
	exe->builtinErrorTypeId=builtinErrorTypeId;
	exe->libs=fklCopyMemory(libs,libNum*sizeof(FklVMlib));
	exe->codeObj=prev->codeObj;
	exe->frames=NULL;
	exe->ptpool=prev->ptpool;
	fklCallObj(nextCall,NULL,exe);
	insert_to_VM_chain(exe,prev,next,gc);
	fklAddToGCNoGC(exe->chan,gc);
	return exe;
}


void fklDestroyVMstack(FklVMstack* stack)
{
	//pthread_rwlock_destroy(&stack->lock);
	free(stack->values);
	free(stack);
}

void fklDestroyAllVMs(FklVM* curVM)
{
	size_t libNum=curVM->libNum;
	FklVMlib* libs=curVM->libs;
	for(size_t i=0;i<libNum;i++)
		fklUninitVMlib(&libs[i]);
	free(curVM->builtinErrorTypeId);
	fklDestroyPrototypePool(curVM->ptpool);
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
	FklVMframe* sf=&exe->sf;
	while(exe->frames)
	{
		FklVMframe* cur=exe->frames;
		exe->frames=cur->prev;
		fklDestroyVMframe(cur,sf);
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
		free(cur);
	}
}

void fklInitVMlib(FklVMlib* lib,FklVMvalue* proc)
{
	lib->proc=proc;
	lib->imported=0;
	lib->loc=NULL;
	lib->count=0;
}

inline void fklInitVMlibWithCodeObj(FklVMlib* lib
		,FklVMvalue* codeObj
		,FklVMgc* gc
		,uint32_t protoId)
{
	FklByteCode* bc=codeObj->u.code->bc;
	FklVMproc* prc=fklCreateVMproc(bc->code,bc->size,codeObj,gc);
	prc->protoId=protoId;
	FklVMvalue* proc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,prc,gc);
	fklInitVMlib(lib,proc);
}

void fklUninitVMlib(FklVMlib* lib)
{
	free(lib->loc);
}

void fklDestroyVMlib(FklVMlib* lib)
{
	fklUninitVMlib(lib);
	free(lib);
}

inline unsigned int fklGetCompoundFrameMark(const FklVMframe* f)
{
	return f->u.c.mark;
}

inline unsigned int fklSetCompoundFrameMark(FklVMframe* f,unsigned int m)
{
	f->u.c.mark=m;
	return m;
}

inline FklVMCompoundFrameVarRef* fklGetCompoundFrameLocRef(FklVMframe* f)
{
	return &f->u.c.lr;
}

inline FklVMvalue* fklGetCompoundFrameProc(const FklVMframe* f)
{
	return f->u.c.proc;
}

inline FklPrototype* fklGetCompoundFrameProcPrototype(const FklVMframe* f,FklVM* exe)
{
	uint32_t pId=f->u.c.proc->u.proc->protoId;
	return &exe->ptpool->pts[pId-1];
}

inline uint8_t* fklGetCompoundFrameCodeAndAdd(FklVMframe* f,size_t a)
{
	uint8_t* r=fklGetCompoundFrameCode(f);
	f->u.c.pc+=a;
	return r;
}

inline uint8_t* fklGetCompoundFrameCode(const FklVMframe* f)
{
	return f->u.c.pc;
}

inline uint8_t fklGetCompoundFrameOpAndInc(FklVMframe* f)
{
	return *f->u.c.pc++;
}

inline void fklAddCompoundFrameCp(FklVMframe* f,int64_t a)
{
	f->u.c.pc+=a;
}

inline uint8_t* fklGetCompoundFrameEnd(const FklVMframe* f)
{
	return f->u.c.end;
}

inline FklSid_t fklGetCompoundFrameSid(const FklVMframe* f)
{
	return f->u.c.sid;
}

inline int fklIsCompoundFrameReachEnd(FklVMframe* f)
{
	if(f->u.c.pc==f->u.c.end)
	{
		if(f->u.c.mark)
		{
			f->u.c.pc=f->u.c.spc;
			f->u.c.mark=0;
			f->u.c.tail=0;
			return 0;
		}
		return 1;
	}
	return 0;
}

inline void fklInitMainProcRefs(FklVMproc* mainProc,FklVMvarRef** closure,uint32_t count)
{
	mainProc->count=count;
	mainProc->closure=fklCopyMemory(closure,sizeof(FklVMvalue*)*mainProc->count);
	for(uint32_t i=0;i<count;i++)
		fklMakeVMvarRefRef(closure[i]);
}
