#include<fakeLisp/reader.h>
#include<fakeLisp/fklni.h>
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

//static pthread_mutex_t GCthreadLock=PTHREAD_MUTEX_INITIALIZER;
//static pthread_t GCthreadId;
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

inline FklVMvalue** fklAllocSpaceForLocalVar(FklVM* exe,uint32_t count)
{
	uint32_t nltp=exe->ltp+count;
	if(exe->lsize<nltp)
	{
		exe->lsize=nltp+32;
		FklVMvalue** locv=(FklVMvalue**)realloc(exe->locv,sizeof(FklVMvalue*)*exe->lsize);
		FKL_ASSERT(locv);
		if(exe->locv!=locv)
			fklUpdateAllVarRef(exe->frames,locv);
		exe->locv=locv;
	}
	FklVMvalue** r=&exe->locv[exe->ltp];
	memset(r,0,sizeof(FklVMvalue**)*count);
	exe->ltp=nltp;
	return r;
}

inline FklVMvalue** fklAllocMoreSpaceForMainFrame(FklVM* exe,uint32_t count)
{
	if(count>exe->ltp)
	{
		uint32_t nltp=count;
		if(exe->lsize<nltp)
		{
			exe->lsize=nltp+32;
			FklVMvalue** locv=(FklVMvalue**)realloc(exe->locv,sizeof(FklVMvalue*)*exe->lsize);
			FKL_ASSERT(locv);
			if(exe->locv!=locv)
				fklUpdateAllVarRef(exe->frames,locv);
			exe->locv=locv;
		}
		FklVMvalue** r=&exe->locv[exe->ltp];
		memset(r,0,sizeof(FklVMvalue**)*(count-exe->ltp));
		exe->ltp=nltp;
	}
	return exe->locv;
}

static void callCompoundProcdure(FklVM* exe,FklVMvalue* proc,FklVMframe* frame)
{
	FklVMframe* tmpFrame=fklCreateVMframeWithProcValue(proc,exe->frames);
	uint32_t lcount=proc->u.proc->lcount;
	FklVMCompoundFrameVarRef* f=&tmpFrame->u.c.lr;
	f->base=exe->ltp;
	f->loc=fklAllocSpaceForLocalVar(exe,lcount);
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

		if(pc[-1]!=FKL_OP_TAIL_CALL)
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
		uint32_t lcount=proc->u.proc->lcount;
		FklVMCompoundFrameVarRef* f=&tmpFrame->u.c.lr;
		f->base=exe->ltp;
		f->loc=fklAllocSpaceForLocalVar(exe,lcount);
		f->lcount=lcount;
		fklPushVMframe(tmpFrame,exe);
	}
}

inline static void initVMcCC(FklVMcCC* ccc,FklVMFuncK kFunc,void* ctx,size_t size)
{
	ccc->kFunc=kFunc;
	ccc->ctx=ctx;
	ccc->size=size;
}

static void dlproc_frame_print_backtrace(FklCallObjData data,FILE* fp,FklSymbolTable* table)
{
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)data;
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
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)data;
	fklGC_toGrey(c->proc,gc);
}

static void dlproc_frame_finalizer(FklCallObjData data)
{
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)data;
	free(c->ccc.ctx);
}

static void dlproc_ccc_copy(const FklVMcCC* s,FklVMcCC* d)
{
	d->kFunc=s->kFunc;
	d->size=s->size;
	if(d->size)
		d->ctx=fklCopyMemory(s->ctx,d->size);
}

static void dlproc_frame_copy(FklCallObjData d,const FklCallObjData s,FklVM* exe)
{
	FklDlprocFrameContext const* const sc=(FklDlprocFrameContext*)s;
	FklDlprocFrameContext* dc=(FklDlprocFrameContext*)d;
	FklVMgc* gc=exe->gc;
	dc->state=sc->state;
	fklSetRef(&dc->proc,sc->proc,gc);
	dlproc_ccc_copy(&sc->ccc,&dc->ccc);
}

static int dlproc_frame_end(FklCallObjData data)
{
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)data;
	return c->state==FKL_DLPROC_DONE;
}

static void dlproc_frame_step(FklCallObjData data,FklVM* exe)
{
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)data;
	FklVMdlproc* dlproc=c->proc->u.dlproc;
	switch(c->state)
	{
		case FKL_DLPROC_READY:
			c->state=FKL_DLPROC_DONE;
			dlproc->func(exe,dlproc->dll,dlproc->pd);
			break;
		case FKL_DLPROC_CCC:
			c->state=FKL_DLPROC_DONE;
			c->ccc.kFunc(exe,FKL_CC_RE,c->ccc.ctx);
			break;
		case FKL_DLPROC_DONE:
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
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)data;
	fklSetRef(&c->proc,proc,gc);
	c->state=FKL_DLPROC_READY;
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
static void B_true(BYTE_CODE_ARGS);
static void B_not(BYTE_CODE_ARGS);
static void B_eq(BYTE_CODE_ARGS);
static void B_eqv(BYTE_CODE_ARGS);
static void B_equal(BYTE_CODE_ARGS);

static void B_eqn(BYTE_CODE_ARGS);
static void B_eqn3(BYTE_CODE_ARGS);

static void B_gt(BYTE_CODE_ARGS);
static void B_gt3(BYTE_CODE_ARGS);

static void B_lt(BYTE_CODE_ARGS);
static void B_lt3(BYTE_CODE_ARGS);

static void B_ge(BYTE_CODE_ARGS);
static void B_ge3(BYTE_CODE_ARGS);

static void B_le(BYTE_CODE_ARGS);
static void B_le3(BYTE_CODE_ARGS);

static void B_inc(BYTE_CODE_ARGS);
static void B_dec(BYTE_CODE_ARGS);
static void B_add(BYTE_CODE_ARGS);
static void B_sub(BYTE_CODE_ARGS);
static void B_mul(BYTE_CODE_ARGS);
static void B_div(BYTE_CODE_ARGS);
static void B_idiv(BYTE_CODE_ARGS);
static void B_mod(BYTE_CODE_ARGS);
static void B_add1(BYTE_CODE_ARGS);
static void B_mul1(BYTE_CODE_ARGS);
static void B_neg(BYTE_CODE_ARGS);
static void B_rec(BYTE_CODE_ARGS);
static void B_add3(BYTE_CODE_ARGS);
static void B_sub3(BYTE_CODE_ARGS);
static void B_mul3(BYTE_CODE_ARGS);
static void B_div3(BYTE_CODE_ARGS);
static void B_idiv3(BYTE_CODE_ARGS);
static void B_push_car(BYTE_CODE_ARGS);
static void B_push_cdr(BYTE_CODE_ARGS);
static void B_cons(BYTE_CODE_ARGS);
static void B_nth(BYTE_CODE_ARGS);
static void B_vec_ref(BYTE_CODE_ARGS);
static void B_str_ref(BYTE_CODE_ARGS);
static void B_box(BYTE_CODE_ARGS);
static void B_unbox(BYTE_CODE_ARGS);
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
	B_true,
	B_not,
	B_eq,
	B_eqv,
	B_equal,

	B_eqn,
	B_eqn3,

	B_gt,
	B_gt3,

	B_lt,
	B_lt3,

	B_ge,
	B_ge3,

	B_le,
	B_le3,

	B_inc,
	B_dec,
	B_add,
	B_sub,
	B_mul,
	B_div,
	B_idiv,
	B_mod,
	B_add1,
	B_mul1,
	B_neg,
	B_rec,
	B_add3,
	B_sub3,
	B_mul3,
	B_div3,
	B_idiv3,

	B_push_car,
	B_push_cdr,
	B_cons,
	B_nth,
	B_vec_ref,
	B_str_ref,
	B_box,
	B_unbox,
};

inline static void insert_to_VM_chain(FklVM* cur,FklVM* prev,FklVM* next)
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
		,FklSymbolTable* globalSymTable)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	exe->importingLib=NULL;
	exe->frames=NULL;
	exe->gc=fklCreateVMgc();
	exe->state=FKL_VM_READY;
	if(mainCode!=NULL)
	{
		FklVMvalue* codeObj=fklCreateVMvalueNoGC(FKL_TYPE_CODE_OBJ,mainCode,exe->gc);
		exe->frames=fklCreateVMframeWithCodeObj(codeObj,exe->frames,exe->gc);
	}
	exe->symbolTable=globalSymTable;
	exe->builtinErrorTypeId=createBuiltinErrorTypeIdList();
	fklInitBuiltinErrorType(exe->builtinErrorTypeId,globalSymTable);
	exe->chan=NULL;
	exe->stack=fklCreateVMstack(32);
	exe->libNum=0;
	exe->libs=NULL;
	exe->ptpool=NULL;
	exe->locv=NULL;
	exe->ltp=0;
	exe->lsize=0;
	exe->prev=exe;
	exe->next=exe;
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

inline void fklDoUninitCompoundFrame(FklVMframe* frame,FklVM* exe)
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
	exe->ltp-=lr->lcount;
}

inline void fklDoFinalizeCompoundFrame(FklVMframe* frame,FklVM* exe)
{
	fklDoUninitCompoundFrame(frame,exe);
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

void fklDoAtomicFrame(FklVMframe* f,FklVMgc* gc)
{
	switch(f->type)
	{
		case FKL_FRAME_COMPOUND:
			fklGC_toGrey(fklGetCompoundFrameProc(f),gc);
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
		uint32_t lcount=proc->u.proc->lcount;
		FklVMCompoundFrameVarRef* f=&tmpFrame->u.c.lr;
		f->base=exe->ltp;
		f->loc=fklAllocSpaceForLocalVar(exe,lcount);
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

inline void fklContinueDlproc(FklVMframe* frame
		,FklVMFuncK kFunc
		,void* ctx
		,size_t size)
{
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)frame->u.o.data;
	c->state=FKL_DLPROC_CCC;
	initVMcCC(&c->ccc,kFunc,ctx,size);
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
	fklContinueDlproc(frame,kFunc,ctx,size);
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

inline void fklDoCompoundFrameStep(FklVMframe* curframe,FklVM* exe)
{
	ByteCodes[fklGetCompoundFrameOpAndInc(curframe)](exe,curframe);
}

//inline FklVM* fklGetNextRunningVM(FklVMscheduler* sc)
//{
//	FklVM* r=sc->run;
//	sc->run=r->next;
//	return r;
//}

static inline void do_step_VM(FklVM* exe)
{
	FklVMframe* curframe=exe->frames;
	switch(curframe->type)
	{
		case FKL_FRAME_COMPOUND:
			if(fklIsCompoundFrameReachEnd(curframe))
				fklDoFinalizeCompoundFrame(popFrame(exe),exe);
			else
				fklDoCompoundFrameStep(curframe,exe);
			break;
		case FKL_FRAME_OTHEROBJ:
			if(fklIsCallableObjFrameReachEnd(curframe))
				fklDoFinalizeObjFrame(popFrame(exe),&exe->sf);
			else
				fklDoCallableObjFrameStep(curframe,exe);
			break;
	}
	if(exe->frames==NULL)
		exe->state=FKL_VM_EXIT;
}

static inline void uninit_all_vm_lib(FklVMlib* libs,size_t num)
{
	for(size_t i=0;i<num;i++)
		fklUninitVMlib(&libs[i]);
}

static inline FklVM* do_exit_VM(FklVM* exe)
{
	FklVM* prev=exe->prev;
	FklVM* next=exe->next;

	prev->next=next;
	next->prev=prev;
	exe->prev=exe;
	exe->next=exe;

	if(exe->chan)
	{
		FklVMchanl* tmpCh=exe->chan->u.chan;
		FklVMvalue* v=fklGetTopValue(exe->stack);
		FklVMvalue* resultBox=fklCreateVMvalueNoGC(FKL_TYPE_BOX,v,exe->gc);
		fklChanlSend(resultBox,tmpCh,exe);

		fklDeleteCallChain(exe);
		fklDestroyVMstack(exe->stack);
		uninit_all_vm_lib(exe->libs,exe->libNum);
		free(exe->locv);
		free(exe->libs);
		free(exe);
	}

	if(next==prev&&next==exe)
		return NULL;
	return next;
}

static inline int is_reach_alarmtime(clock_t alarmtime)
{
	return fklGetTicks()>=alarmtime;
}

static inline int do_alarm_VM(FklVM* exe)
{
	int r=is_reach_alarmtime(exe->alarmtime);
	if(r)
		exe->state=FKL_VM_READY;
	return r;
}

int fklRunVM(FklVM* idle)
{
	FklVM* exe=idle;
	while(exe)
	{
		switch(exe->state)
		{
			case FKL_VM_RUNNING:
				do_step_VM(exe);
				break;
			case FKL_VM_EXIT:
				exe=do_exit_VM(exe);
				continue;
				break;
			case FKL_VM_READY:
				if(setjmp(exe->buf)==1)
				{
					if(exe->chan)
					{
						FklVMvalue* err=fklGetTopValue(exe->stack);
						fklChanlSend(err,exe->chan->u.chan,exe);
						exe->state=FKL_VM_EXIT;
						continue;
					}
					else
						return 255;
				}
				exe->state=FKL_VM_RUNNING;
				continue;
				break;
			case FKL_VM_SLEEPING:
				if(do_alarm_VM(exe))
					continue;
				break;
			case FKL_VM_WAITING:
				break;
		}
		exe=exe->next;
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

static inline FklVMvarRef* insert_local_ref(FklVMCompoundFrameVarRef* lr
		,FklVMvarRef* ref
		,uint32_t cidx)
{
	FklVMvarRefList* rl=(FklVMvarRefList*)malloc(sizeof(FklVMvarRefList));
	FKL_ASSERT(rl);
	rl->ref=fklMakeVMvarRefRef(ref);
	rl->next=lr->lrefl;
	lr->lrefl=rl;
	lr->lref[cidx]=ref;
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
	FklVMproc* proc=fklCreateVMproc(spc,cpc,codeObj,gc,protoId);
	FklFuncPrototype* pt=&exe->ptpool->pts[protoId-1];
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
					closure[i]=insert_local_ref(lr,fklCreateVMvarRef(lr->loc,c->cidx),c->cidx);
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
	proc->lcount=pt->lcount;
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
	return s->base[--s->tp];
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
	if(!tmpValue)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.call",FKL_ERR_TOOFEWARG,exe);
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
	if(!tmpValue)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.call",FKL_ERR_TOOFEWARG,exe);
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
			,fklCreateVMhashTableEq(),exe);
	uint64_t kvnum=num*2;
	FklVMvalue** base=&stack->base[stack->tp-kvnum-1];
	for(uint32_t i=0;i<kvnum;i+=2)
	{
		FklVMvalue* key=base[i];
		FklVMvalue* value=base[i+1];
		fklSetVMhashTable(key,value,hash->u.hash,exe->gc);
	}
	ap-=kvnum;
	fklNiReturn(hash,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_push_hash_eqv(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	FklVMvalue* hash=fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE
			,fklCreateVMhashTableEqv(),exe);

	uint64_t kvnum=num*2;
	FklVMvalue** base=&stack->base[stack->tp-kvnum-1];
	for(uint32_t i=0;i<kvnum;i+=2)
	{
		FklVMvalue* key=base[i];
		FklVMvalue* value=base[i+1];
		fklSetVMhashTable(key,value,hash->u.hash,exe->gc);
	}
	ap-=kvnum;
	fklNiReturn(hash,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_push_hash_equal(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	FklVMvalue* hash=fklCreateVMvalueToStack(FKL_TYPE_HASHTABLE
			,fklCreateVMhashTableEqual(),exe);
	uint64_t kvnum=num*2;

	FklVMvalue** base=&stack->base[stack->tp-kvnum-1];
	for(size_t i=0;i<kvnum;i+=2)
	{
		FklVMvalue* key=base[i];
		FklVMvalue* value=base[i+1];
		fklSetVMhashTable(key,value,hash->u.hash,exe->gc);
	}
	ap-=kvnum;
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
		*pcur=fklCreateVMpairV(stack->base[i],FKL_VM_NIL,exe);
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
		*pcur=fklCreateVMpairV(stack->base[i],FKL_VM_NIL,exe);
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
	uint32_t count=plib->count;
	FklVMvalue* v=libIdx>=count?NULL:plib->loc[libIdx];
	if(v)
		*get_compound_frame_loc(frame,locIdx,exe)=v;
	else
	{
		fklAddCompoundFrameCp(frame,-sizeof(uint64_t));
		char* cstr=NULL;
		if(libIdx<plib->count)
		{
			FklFuncPrototype* pt=&exe->ptpool->pts[plib->proc->u.proc->protoId-1];
			FklSid_t sid=pt->loc[libIdx].k.id;
			cstr=fklStringToCstr(fklGetSymbolWithId(sid,exe->symbolTable)->symbol);
		}
		else
		{
			FklFuncPrototype* pt=&exe->ptpool->pts[fklGetCompoundFrameProc(frame)->u.proc->protoId-1];
			FklSid_t sid=pt->loc[locIdx].k.id;
			cstr=fklStringToCstr(fklGetSymbolWithId(sid,exe->symbolTable)->symbol);
		}
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.import",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
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
		plib->belong=1;
		plib->proc=FKL_VM_NIL;
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
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint32_t)));
	FklVMvalue* v=fklGetTopValue(exe->stack);
	*get_compound_frame_loc(frame,idx,exe)=v;
	fklNiDoSomeAfterSetLoc(v,idx,frame,exe);
}

inline static FklVMvalue* get_var_val(FklVMframe* frame,uint32_t idx,FklFuncPrototypes* ptpool,FklSid_t* psid)
{
	FklVMCompoundFrameVarRef* lr=&frame->u.c.lr;
	FklVMvalue* v=idx<lr->rcount?*(lr->ref[idx]->ref):NULL;
	if(!v)
	{
		FklVMproc* proc=fklGetCompoundFrameProc(frame)->u.proc;
		FklFuncPrototype* pt=&ptpool->pts[proc->protoId-1];
		FklSymbolDef* def=&pt->refs[idx];
		*psid=def->k.id;
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

inline static FklVMvalue* volatile* get_var_ref(FklVMframe* frame,uint32_t idx,FklFuncPrototypes* ptpool,FklSid_t* psid)
{
	FklVMCompoundFrameVarRef* lr=&frame->u.c.lr;
	FklVMvarRef** refs=lr->ref;
	FklVMvalue* volatile* v=(idx>=lr->rcount||!(refs[idx]->ref))?NULL:refs[idx]->ref;
	if(!v)
	{
		FklVMproc* proc=fklGetCompoundFrameProc(frame)->u.proc;
		FklFuncPrototype* pt=&ptpool->pts[proc->protoId-1];
		FklSymbolDef* def=&pt->refs[idx];
		*psid=def->k.id;
		return NULL;
	}
	return v;
}

static void inline B_put_var_ref(FklVM* exe,FklVMframe* frame)
{
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint32_t)));
	FklSid_t id=0;
	FklVMvalue* volatile* pv=get_var_ref(frame,idx,exe->ptpool,&id);
	if(!pv)
	{
		char* cstr=fklStringToCstr(fklGetSymbolWithId(id,exe->symbolTable)->symbol);
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.put-var-ref",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
	}
	FklVMvalue* v=fklGetTopValue(exe->stack);
	*pv=v;
	fklNiDoSomeAfterSetRef(v,idx,frame,exe);
}

static void inline B_export(FklVM* exe,FklVMframe* frame)
{
	uint32_t libId=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(libId)));
	FklVMlib* lib=&exe->libs[libId-1];
	FklVMCompoundFrameVarRef* lr=fklGetCompoundFrameLocRef(frame);
	uint32_t count=lr->lcount;
	FklVMvalue** loc=fklCopyMemory(lr->loc,sizeof(FklVMvalue*)*count);
	fklDropTop(exe->stack);
	lib->loc=loc;
	lib->count=count;
	lib->imported=1;
	lib->belong=1;
	FklVMproc* proc=lib->proc->u.proc;
	uint32_t rcount=proc->count;
	proc->count=0;
	FklVMvarRef** refs=proc->closure;
	proc->closure=NULL;
	for(uint32_t i=0;i<rcount;i++)
		fklDestroyVMvarRef(refs[i]);
	free(refs);
}

static void inline B_true(FklVM* exe,FklVMframe* frame)
{
	fklDropTop(exe->stack);
	fklPushVMvalue(FKL_MAKE_VM_FIX(1),exe->stack);
}

static void inline B_not(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* val=fklNiGetArg(&ap,stack);
	if(val==FKL_VM_NIL)
		fklNiReturn(FKL_VM_TRUE,&ap,stack);
	else
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_eq(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	fklNiReturn(fklVMvalueEq(fir,sec)
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_eqv(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	fklNiReturn((fklVMvalueEqv(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
		fklNiEnd(&ap,stack);
}

static void inline B_equal(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	fklNiReturn((fklVMvalueEqual(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_eqn(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)==0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_eqn3(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)==0
		&&!err
		&&fklVMvalueCmp(b,c,&err)==0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_gt(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_gt3(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>0
		&&!err
		&&fklVMvalueCmp(b,c,&err)>0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_lt(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_lt3(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<0
		&&!err
		&&fklVMvalueCmp(b,c,&err)<0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_ge(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_ge3(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>=0
		&&!err
		&&fklVMvalueCmp(b,c,&err)>=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_le(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_le3(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<=0
		&&!err
		&&fklVMvalueCmp(b,c,&err)<=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

static void inline B_inc(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* arg=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(arg,fklIsVMnumber,"builtin.1+",exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64+1.0;
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&r,exe),&ap,stack);
	}
	else
	{
		if(FKL_IS_BIG_INT(arg))
		{
			if(fklIsGeLeFixBigInt(arg->u.bigInt))
			{
				FklBigInt* bi=fklCreateBigInt0();
				fklSetBigInt(bi,arg->u.bigInt);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(FKL_MAKE_VM_FIX(fklBigIntToI64(arg->u.bigInt)+1),&ap,stack);
		}
		else
		{
			int64_t i=fklGetInt(arg);
			if(i==FKL_FIX_INT_MAX)
			{
				FklBigInt* bi=fklCreateBigInt(i);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(FKL_MAKE_VM_FIX(i+1),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

static void inline B_dec(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* arg=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(arg,fklIsVMnumber,"builtin.-1+",exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64-1.0;
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&r,exe),&ap,stack);
	}
	else
	{
		if(FKL_IS_BIG_INT(arg))
		{
			if(fklIsGeLeFixBigInt(arg->u.bigInt))
			{
				FklBigInt* bi=fklCreateBigInt0();
				fklSetBigInt(bi,arg->u.bigInt);
				fklAddBigIntI(bi,1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(FKL_MAKE_VM_FIX(fklBigIntToI64(arg->u.bigInt)-1),&ap,stack);
		}
		else
		{
			int64_t i=fklGetInt(arg);
			if(i==INT64_MIN)
			{
				FklBigInt* bi=fklCreateBigInt(i);
				fklSubBigIntI(bi,1);
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
			}
			else
				fklNiReturn(fklMakeVMint(i-1,exe),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

static inline FklBigInt* create_uninit_big_int(void)
{
	FklBigInt* t=(FklBigInt*)malloc(sizeof(FklBigInt));
	FKL_ASSERT(t);
	return t;
}

#define PROCESS_ADD(VAR,ERR) if(FKL_IS_FIX(VAR))\
{\
	int64_t c64=fklGetInt(VAR);\
	if(fklIsFixAddOverflow(r64,c64))\
		fklAddBigIntI(&bi,c64);\
	else\
		r64+=c64;\
}\
else if(FKL_IS_BIG_INT(VAR))\
	fklAddBigInt(&bi,VAR->u.bigInt);\
else if(FKL_IS_F64(VAR))\
	rd+=VAR->u.f64;\
else\
{\
	fklUninitBigInt(&bi);\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
}

#define PROCESS_ADD_RES() if(rd!=0.0)\
{\
	rd+=r64+fklBigIntToDouble(&bi);\
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);\
	fklUninitBigInt(&bi);\
}\
else if(FKL_IS_0_BIG_INT(&bi))\
{\
	fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);\
	fklUninitBigInt(&bi);\
}\
else\
{\
	fklAddBigIntI(&bi,r64);\
	if(fklIsGtLtFixBigInt(&bi))\
	{\
		FklBigInt* r=create_uninit_big_int();\
		*r=bi;\
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);\
	}\
	else\
	{\
		fklNiReturn(FKL_MAKE_VM_FIX(fklBigIntToI64(&bi)),&ap,stack);\
		fklUninitBigInt(&bi);\
	}\
}

static inline void B_add(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	PROCESS_ADD(a,"builtin.+");
	PROCESS_ADD(b,"builtin.+");
	PROCESS_ADD_RES();
	fklNiEnd(&ap,stack);
}

static inline void B_add3(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	PROCESS_ADD(a,"builtin.+");
	PROCESS_ADD(b,"builtin.+");
	PROCESS_ADD(c,"builtin.+");
	PROCESS_ADD_RES();
	fklNiEnd(&ap,stack);
}

#define PROCESS_SUB_RES() if(FKL_IS_F64(a)||rd!=0.0)\
{\
	rd=fklGetDouble(a)-rd-r64-fklBigIntToDouble(&bi);\
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);\
	fklUninitBigInt(&bi);\
}\
else if(FKL_IS_0_BIG_INT(&bi)&&!FKL_IS_BIG_INT(a))\
{\
	int64_t p64=fklGetInt(a);\
	if(fklIsFixAddOverflow(p64,-r64))\
	{\
		fklAddBigIntI(&bi,p64);\
		fklSubBigIntI(&bi,r64);\
		FklBigInt* r=create_uninit_big_int();\
		*r=bi;\
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);\
	}\
	else\
	{\
		r64=p64-r64;\
		fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);\
		fklUninitBigInt(&bi);\
	}\
}\
else\
{\
	fklSubBigInt(&bi,a->u.bigInt);\
	fklMulBigIntI(&bi,-1);\
	fklSubBigIntI(&bi,r64);\
	FklBigInt* r=create_uninit_big_int();\
	*r=bi;\
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);\
}

static inline void B_sub(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(a,fklIsVMnumber,"builtin.-",exe);


	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);

	FklVMvalue* b=fklNiGetArg(&ap,stack);
	PROCESS_ADD(b,"builtin.-");
	PROCESS_SUB_RES();
	fklNiEnd(&ap,stack);
}

static inline void B_sub3(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(a,fklIsVMnumber,"builtin.-",exe);

	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);

	FklVMvalue* b=fklNiGetArg(&ap,stack);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	PROCESS_ADD(b,"builtin.-");
	PROCESS_ADD(c,"builtin.-");
	PROCESS_SUB_RES();
	fklNiEnd(&ap,stack);
}

#undef PROCESS_SUB_RES

static inline void B_push_car(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.car",exe);
	fklNiReturn(obj->u.pair->car,&ap,stack);
	fklNiEnd(&ap,stack);
}

static inline void B_push_cdr(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.cdr",exe);
	fklNiReturn(obj->u.pair->cdr,&ap,stack);
	fklNiEnd(&ap,stack);
}

static inline void B_cons(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* car=fklNiGetArg(&ap,stack);
	FklVMvalue* cdr=fklNiGetArg(&ap,stack);
	fklNiReturn(fklCreateVMpairV(car,cdr,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

static inline void B_nth(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(place,fklIsInt,"builtin.nth",exe);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_INVALIDACCESS,exe);
	size_t index=fklGetUint(place);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(uint64_t i=0;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		if(FKL_IS_PAIR(objPair))
			fklNiReturn(objPair->u.pair->car,&ap,stack);
		else
			fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklNiEnd(&ap,stack);
}

static inline void B_vec_ref(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vector))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	size_t size=vector->u.vec->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INVALIDACCESS,exe);
	if(index>=vector->u.vec->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INVALIDACCESS,exe);
	fklNiReturn(vector->u.vec->base[index],&ap,stack);
	fklNiEnd(&ap,stack);
}

static inline void B_str_ref(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* str=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(!fklIsInt(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	size_t size=str->u.str->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INVALIDACCESS,exe);
	if(index>=str->u.str->size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INVALIDACCESS,exe);
	fklNiReturn(FKL_MAKE_VM_CHR(str->u.str->str[index]),&ap,stack);
	fklNiEnd(&ap,stack);
}

static inline void B_box(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BOX,obj,exe),&ap,stack);
	fklNiEnd(&ap,stack);
}

static inline void B_unbox(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* box=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(box,FKL_IS_BOX,"builtin.unbox",exe);
	fklNiReturn(box->u.box,&ap,stack);
	fklNiEnd(&ap,stack);
}

static inline void B_add1(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	PROCESS_ADD(a,"builtin.+");
	PROCESS_ADD_RES();
	fklNiEnd(&ap,stack);
}

static inline void B_neg(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	double rd=0.0;
	FKL_NI_CHECK_TYPE(a,fklIsVMnumber,"builtin.-",exe);
	if(FKL_IS_F64(a))
	{
		rd=-a->u.f64;
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
	}
	else if(FKL_IS_FIX(a))
	{
		int64_t p64=fklGetInt(a);
		if(fklIsFixMulOverflow(p64,-1))
		{
			FklBigInt* bi=fklCreateBigInt(p64);
			fklMulBigIntI(bi,-1);
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,bi,exe),&ap,stack);
		}
		else
			fklNiReturn(fklMakeVMint(-p64,exe),&ap,stack);
	}
	else
	{
		FklBigInt bi=FKL_STACK_INIT;
		fklInitBigInt0(&bi);
		fklSetBigInt(&bi,a->u.bigInt);
		fklMulBigIntI(&bi,-1);
		if(fklIsGtLtFixBigInt(&bi))
		{
			FklBigInt* r=create_uninit_big_int();
			*r=bi;
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);
		}
		else
		{
			fklNiReturn(FKL_MAKE_VM_FIX(fklBigIntToI64(&bi)),&ap,stack);
			fklUninitBigInt(&bi);
		}
	}
	fklNiEnd(&ap,stack);

}

static inline void B_rec(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	double rd=1.0;
	FKL_NI_CHECK_TYPE(a,fklIsVMnumber,"builtin.-",exe);
	if(FKL_IS_F64(a))
	{
		if(!islessgreater(a->u.f64,0.0))
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);
		rd=1/a->u.f64;
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
	}
	else
	{
		if(FKL_IS_FIX(a))
		{
			r64=fklGetInt(a);
			if(!r64)
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);
			if(r64==1)
				fklNiReturn(FKL_MAKE_VM_FIX(1),&ap,stack);
			else if(r64==-1)
				fklNiReturn(FKL_MAKE_VM_FIX(-1),&ap,stack);
			else
			{
				rd=1.0/r64;
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
			}
		}
		else
		{
			if(FKL_IS_1_BIG_INT(a->u.bigInt))
				fklNiReturn(FKL_MAKE_VM_FIX(1),&ap,stack);
			else if(FKL_IS_N_1_BIG_INT(a->u.bigInt))
				fklNiReturn(FKL_MAKE_VM_FIX(-1),&ap,stack);
			else
			{
				double bd=fklBigIntToDouble(a->u.bigInt);
				rd=1.0/bd;
				fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);
			}
		}
	}
	fklNiEnd(&ap,stack);
}

#undef PROCESS_ADD_RES
#undef PROCESS_ADD

#define PROCESS_MUL(VAR,ERR) if(FKL_IS_FIX(VAR))\
{\
	int64_t c64=fklGetInt(VAR);\
	if(fklIsFixMulOverflow(r64,c64))\
		fklMulBigIntI(&bi,c64);\
	else\
		r64*=c64;\
}\
else if(FKL_IS_BIG_INT(VAR))\
	fklMulBigInt(&bi,VAR->u.bigInt);\
else if(FKL_IS_F64(VAR))\
	rd*=VAR->u.f64;\
else\
{\
	fklUninitBigInt(&bi);\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR,FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
}

#define PROCESS_MUL_RES() if(rd!=1.0)\
{\
	rd*=r64*fklBigIntToDouble(&bi);\
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);\
	fklUninitBigInt(&bi);\
}\
else if(FKL_IS_1_BIG_INT(&bi))\
{\
	fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);\
	fklUninitBigInt(&bi);\
}\
else\
{\
	fklMulBigIntI(&bi,r64);\
	FklBigInt* r=create_uninit_big_int();\
	*r=bi;\
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);\
}

static inline void B_mul1(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	PROCESS_MUL(a,"builtin.*");
	PROCESS_MUL_RES();
	fklNiEnd(&ap,stack);
}

static inline void B_mul(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	PROCESS_MUL(a,"builtin.*");
	PROCESS_MUL(b,"builtin.*");
	PROCESS_MUL_RES();
	fklNiEnd(&ap,stack);
}

static inline void B_mul3(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	FklVMvalue* b=fklNiGetArg(&ap,stack);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	PROCESS_MUL(a,"builtin.*");
	PROCESS_MUL(b,"builtin.*");
	PROCESS_MUL(c,"builtin.*");
	PROCESS_MUL_RES();
	fklNiEnd(&ap,stack);
}

#define PROCESS_DIV_RES() if(r64==0||FKL_IS_0_BIG_INT(&bi)||!islessgreater(rd,0.0))\
{\
	fklUninitBigInt(&bi);\
	FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);\
}\
if(FKL_IS_F64(a)\
		||rd!=1.0\
		||(FKL_IS_FIX(a)\
			&&FKL_IS_1_BIG_INT(&bi)\
			&&fklGetInt(a)%(r64))\
		||(FKL_IS_BIG_INT(a)\
			&&((!FKL_IS_1_BIG_INT(&bi)&&!fklIsDivisibleBigInt(a->u.bigInt,&bi))\
				||!fklIsDivisibleBigIntI(a->u.bigInt,r64))))\
{\
	rd=fklGetDouble(a)/rd/r64/fklBigIntToDouble(&bi);\
	fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&rd,exe),&ap,stack);\
}\
else\
{\
	if(FKL_IS_BIG_INT(a)&&!FKL_IS_1_BIG_INT(&bi))\
	{\
		FklBigInt* t=fklCreateBigInt0();\
		fklSetBigInt(t,a->u.bigInt);\
		fklDivBigInt(t,&bi);\
		fklDivBigIntI(t,r64);\
		if(fklIsGtLtFixBigInt(t))\
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,t,exe),&ap,stack);\
		else\
		{\
			fklNiReturn(fklMakeVMint(fklBigIntToI64(t),exe),&ap,stack);\
			fklDestroyBigInt(t);\
		}\
	}\
	else\
	{\
		r64=fklGetInt(a)/r64;\
		fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);\
	}\
}\
fklUninitBigInt(&bi)

static inline void B_div(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(a,fklIsVMnumber,"builtin./",exe);

	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=fklNiGetArg(&ap,stack);
	PROCESS_MUL(b,"builtin./");
	PROCESS_DIV_RES();
	fklNiEnd(&ap,stack);
}

static inline void B_div3(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(a,fklIsVMnumber,"builtin./",exe);

	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=fklNiGetArg(&ap,stack);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	PROCESS_MUL(b,"builtin./");
	PROCESS_MUL(c,"builtin./");
	PROCESS_DIV_RES();
	fklNiEnd(&ap,stack);
}

#define PROCESS_IMUL(VAR) if(FKL_IS_FIX(VAR))\
{\
	int64_t c64=fklGetInt(VAR);\
	if(fklIsFixMulOverflow(r64,c64))\
		fklMulBigIntI(&bi,c64);\
	else\
		r64*=c64;\
}\
else if(FKL_IS_BIG_INT(VAR))\
	fklMulBigInt(&bi,VAR->u.bigInt);\
else\
{\
	fklUninitBigInt(&bi);\
	FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_INCORRECT_TYPE_VALUE,exe);\
}

#define PROCESS_IDIV_RES() if(r64==0||FKL_IS_0_BIG_INT(&bi))\
{\
	fklUninitBigInt(&bi);\
	FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,exe);\
}\
fklNiResBp(&ap,stack);\
if(FKL_IS_BIG_INT(a)&&!FKL_IS_1_BIG_INT(&bi))\
{\
	FklBigInt* t=fklCreateBigInt0();\
	fklSetBigInt(t,a->u.bigInt);\
	fklDivBigInt(t,&bi);\
	fklDivBigIntI(t,r64);\
	if(fklIsGtLtFixBigInt(t))\
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,t,exe),&ap,stack);\
	else\
	{\
		fklNiReturn(fklMakeVMint(fklBigIntToI64(t),exe),&ap,stack);\
		fklDestroyBigInt(t);\
	}\
}\
else\
{\
	r64=fklGetInt(a)/r64;\
	fklNiReturn(fklMakeVMint(r64,exe),&ap,stack);\
}\
fklUninitBigInt(&bi)

static inline void B_idiv(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(a,fklIsInt,"builtin./",exe);

	int64_t r64=1;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=fklNiGetArg(&ap,stack);
	PROCESS_IMUL(b);
	PROCESS_IDIV_RES();
	fklNiEnd(&ap,stack);
}

static inline void B_mod(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(!fklIsVMnumber(fir)||!fklIsVMnumber(sec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	if(FKL_IS_F64(fir)||FKL_IS_F64(sec))
	{
		double af=fklGetDouble(fir);
		double as=fklGetDouble(sec);
		if(!islessgreater(as,0.0))
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
		double r=fmod(af,as);
		fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_F64,&r,exe),&ap,stack);
	}
	else if(FKL_IS_FIX(fir)&&FKL_IS_FIX(sec))
	{
		int64_t si=fklGetInt(sec);
		int64_t r=fklGetInt(fir)%si;
		if(si==0)
			FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
		fklNiReturn(fklMakeVMint(r,exe),&ap,stack);
	}
	else
	{
		FklBigInt rem=FKL_BIG_INT_INIT;
		fklInitBigInt0(&rem);
		if(FKL_IS_BIG_INT(fir))
			fklSetBigInt(&rem,fir->u.bigInt);
		else
			fklSetBigIntI(&rem,fklGetInt(fir));
		if(FKL_IS_BIG_INT(sec))
		{
			if(FKL_IS_0_BIG_INT(sec->u.bigInt))
			{
				fklUninitBigInt(&rem);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
			}
			fklModBigInt(&rem,sec->u.bigInt);
		}
		else
		{
			int64_t si=fklGetInt(sec);
			if(si==0)
			{
				fklUninitBigInt(&rem);
				FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
			}
			fklModBigIntI(&rem,si);
		}
		if(fklIsGtLtFixBigInt(&rem))
		{
			FklBigInt* r=create_uninit_big_int();
			*r=rem;
			fklNiReturn(fklCreateVMvalueToStack(FKL_TYPE_BIG_INT,r,exe),&ap,stack);
		}
		else
		{
			fklNiReturn(fklMakeVMint(fklBigIntToI64(&rem),exe),&ap,stack);
			fklUninitBigInt(&rem);
		}
	}
	fklNiEnd(&ap,stack);

}

static inline void B_idiv3(FklVM* exe,FklVMframe* frame)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* a=fklNiGetArg(&ap,stack);
	FKL_NI_CHECK_TYPE(a,fklIsInt,"builtin./",exe);

	int64_t r64=1;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=fklNiGetArg(&ap,stack);
	FklVMvalue* c=fklNiGetArg(&ap,stack);
	PROCESS_IMUL(b);
	PROCESS_IMUL(c);
	PROCESS_IDIV_RES();
	fklNiEnd(&ap,stack);
}

#undef PROCESS_MUL
#undef PROCESS_MUL_RES
#undef PROCESS_SUB_RES

FklVMstack* fklCreateVMstack(uint32_t size)
{
	FklVMstack* tmp=(FklVMstack*)malloc(sizeof(FklVMstack));
	FKL_ASSERT(tmp);
	tmp->size=size;
	tmp->tp=0;
	tmp->bp=0;
	tmp->base=(FklVMvalue**)malloc(size*sizeof(FklVMvalue*));
	FKL_ASSERT(tmp->base);
	return tmp;
}

#define RECYCLE_NUN (128)
void fklStackRecycle(FklVMstack* stack)
{
	if(stack->size-stack->tp>RECYCLE_NUN)
	{
		stack->base=(FklVMvalue**)realloc(stack->base,sizeof(FklVMvalue*)*(stack->size-RECYCLE_NUN));
		FKL_ASSERT(stack->base);
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
			FklVMvalue* tmp=stack->base[i];
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
	return tmp;
}

void fklGC_toGrey(FklVMvalue* v,FklVMgc* gc)
{
	if(FKL_IS_PTR(v)&&v!=NULL&&v->mark!=FKL_MARK_B)
	{
		v->mark=FKL_MARK_G;
		gc->grey=createGreylink(v,gc->grey);
		gc->greyNum++;
	}
}

void fklGC_markRootToGrey(FklVM* exe)
{
	FklVMstack* stack=exe->stack;
	FklVMgc* gc=exe->gc;

	for(FklVMframe* cur=exe->frames;cur;cur=cur->prev)
		fklDoAtomicFrame(cur,gc);

	uint32_t count=exe->ltp;
	FklVMvalue** loc=exe->locv;
	for(uint32_t i=0;i<count;i++)
		fklGC_toGrey(loc[i],gc);

	for(size_t i=0;i<exe->libNum;i++)
	{
		FklVMlib* lib=&exe->libs[i];
		fklGC_toGrey(lib->proc,gc);
		if(lib->imported)
		{
			for(uint32_t i=0;i<lib->count;i++)
				fklGC_toGrey(lib->loc[i],gc);
		}
	}
	for(uint32_t i=0;i<stack->tp;i++)
	{
		FklVMvalue* value=stack->base[i];
		fklGC_toGrey(value,gc);
	}
	fklGC_toGrey(exe->chan,gc);
}

void fklGC_markAllRootToGrey(FklVM* curVM)
{
	fklGC_markRootToGrey(curVM);

	for(FklVM* cur=curVM->next;cur!=curVM;)
	{
		fklGC_markRootToGrey(cur);
		cur=cur->next;
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
	if(FKL_IS_PTR(v)&&v->mark==FKL_MARK_G)
	{
		v->mark=FKL_MARK_B;
		propagateMark(v,gc);
	}
	return gc->grey==NULL;
}

void fklGC_collect(FklVMgc* gc,FklVMvalue** pw)
{
	size_t count=0;
	FklVMvalue* head=gc->head;
	gc->head=NULL;
	gc->running=FKL_GC_SWEEPING;
	FklVMvalue** phead=&head;
	while(*phead)
	{
		FklVMvalue* cur=*phead;
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
	*phead=gc->head;
	gc->head=head;
	gc->num-=count;
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

//#define FKL_GC_GRAY_FACTOR (16)
//#define FKL_GC_NEW_FACTOR (4)

//inline void fklGC_step(FklVM* exe)
//{
//	FklGCstate running=FKL_GC_NONE;
//	int cr=0;
//	fklGetGCstateAndGCNum(exe->gc,&running,&cr);
//	static size_t greyNum=0;
//	cr=1;
//	switch(running)
//	{
//		case FKL_GC_NONE:
//			if(cr)
//			{
//				fklChangeGCstate(FKL_GC_MARK_ROOT,exe->gc);
//				fklGC_pause(exe);
//				fklChangeGCstate(FKL_GC_PROPAGATE,exe->gc);
//			}
//			break;
//		case FKL_GC_MARK_ROOT:
//			break;
//		case FKL_GC_PROPAGATE:
//			{
//				size_t create_n=exe->gc->greyNum-greyNum;
//				size_t timce=exe->gc->greyNum/FKL_GC_GRAY_FACTOR+create_n/FKL_GC_NEW_FACTOR+1;
//				greyNum+=create_n;
//				for(size_t i=0;i<timce;i++)
//					if(fklGC_propagate(exe->gc))
//					{
//						fklChangeGCstate(FKL_GC_SWEEP,exe->gc);
//						break;
//					}
//			}
//			break;
//		case FKL_GC_SWEEP:
//			if(!pthread_mutex_trylock(&GCthreadLock))
//			{
//				pthread_create(&GCthreadId,NULL,fklGC_sweepThreadFunc,exe->gc);
//				fklChangeGCstate(FKL_GC_COLLECT,exe->gc);
//				pthread_mutex_unlock(&GCthreadLock);
//			}
//			break;
//		case FKL_GC_COLLECT:
//		case FKL_GC_SWEEPING:
//			break;
//		case FKL_GC_DONE:
//			if(!pthread_mutex_trylock(&GCthreadLock))
//			{
//				fklChangeGCstate(FKL_GC_NONE,exe->gc);
//				pthread_join(GCthreadId,NULL);
//				pthread_mutex_unlock(&GCthreadLock);
//			}
//			break;
//		default:
//			FKL_ASSERT(0);
//			break;
//	}
//}

//void* fklGC_sweepThreadFunc(void* arg)
//{
//	FklVMgc* gc=arg;
//	FklVMvalue* white=NULL;
//	fklGC_collect(gc,&white);
//	fklChangeGCstate(FKL_GC_SWEEPING,gc);
//	fklGC_sweep(white);
//	fklChangeGCstate(FKL_GC_DONE,gc);
//	return NULL;
//}

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

static inline FklVMlib* copy_vm_libs(FklVMlib* libs,size_t libNum)
{
	FklVMlib* r=fklCopyMemory(libs,libNum*sizeof(FklVMlib));
	for(size_t i=0;i<libNum;i++)
		r[i].belong=0;
	return r;
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
	exe->chan=fklCreateSaveVMvalue(FKL_TYPE_CHAN,fklCreateVMchanl(0));
	exe->stack=fklCreateVMstack(32);
	exe->gc=gc;
	exe->symbolTable=table;
	exe->libNum=libNum;
	exe->builtinErrorTypeId=builtinErrorTypeId;
	exe->libs=copy_vm_libs(libs,libNum);
	exe->frames=NULL;
	exe->ptpool=prev->ptpool;
	exe->lsize=0;
	exe->ltp=0;
	exe->locv=NULL;
	exe->state=FKL_VM_READY;
	fklCallObj(nextCall,NULL,exe);
	insert_to_VM_chain(exe,prev,next);
	fklAddToGCNoGC(exe->chan,gc);
	return exe;
}


void fklDestroyVMstack(FklVMstack* stack)
{
	free(stack->base);
	free(stack);
}

void fklDestroyAllVMs(FklVM* curVM)
{
	free(curVM->builtinErrorTypeId);
	fklDestroyPrototypePool(curVM->ptpool);
	curVM->prev->next=NULL;
	curVM->prev=NULL;
	for(FklVM* cur=curVM;cur;)
	{
		uninit_all_vm_lib(cur->libs,cur->libNum);
		fklDeleteCallChain(cur);
		fklDestroyVMstack(cur->stack);
		FklVM* t=cur;
		cur=cur->next;
		free(t->locv);
		free(t->libs);
		free(t);
	}
}

void fklDestroyVMgc(FklVMgc* gc)
{
	fklDestroyAllValues(gc);
	free(gc);
}

void fklDeleteCallChain(FklVM* exe)
{
	while(exe->frames)
	{
		FklVMframe* cur=exe->frames;
		exe->frames=cur->prev;
		fklDestroyVMframe(cur,exe);
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

//void fklGC_joinGCthread(FklVMgc* gc)
//{
//	pthread_join(GCthreadId,NULL);
//	gc->running=FKL_GC_NONE;
//}

inline void fklPushVMvalue(FklVMvalue* v,FklVMstack* s)
{
	if(s->tp>=s->size)
	{
		s->base=(FklVMvalue**)realloc(s->base
				,sizeof(FklVMvalue*)*(s->size+64));
		FKL_ASSERT(s->base);
		s->size+=64;
	}
	s->base[s->tp]=v;
	s->tp+=1;
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
	lib->belong=0;
	lib->loc=NULL;
	lib->count=0;
}

inline void fklInitVMlibWithCodeObj(FklVMlib* lib
		,FklVMvalue* codeObj
		,FklVMgc* gc
		,uint32_t protoId)
{
	FklByteCode* bc=codeObj->u.code->bc;
	FklVMproc* prc=fklCreateVMproc(bc->code,bc->size,codeObj,gc,protoId);
	FklVMvalue* proc=fklCreateVMvalueNoGC(FKL_TYPE_PROC,prc,gc);
	fklInitVMlib(lib,proc);
}

void fklUninitVMlib(FklVMlib* lib)
{
	if(lib->belong)
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

inline FklFuncPrototype* fklGetCompoundFrameProcPrototype(const FklVMframe* f,FklVM* exe)
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
