#include<fakeLisp/reader.h>
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

static int VMargc=0;
static char** VMargv=NULL;

/*procedure call functions*/

#define FKL_VM_LOCV_INC_NUM (32)
inline FklVMvalue** fklAllocSpaceForLocalVar(FklVM* exe,uint32_t count)
{
	uint32_t nltp=exe->ltp+count;
	if(exe->llast<nltp)
	{
		exe->llast=nltp+FKL_VM_LOCV_INC_NUM;
		FklVMvalue** locv=(FklVMvalue**)fklRealloc(exe->locv,sizeof(FklVMvalue*)*exe->llast);
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
		if(exe->llast<nltp)
		{
			exe->llast=nltp+FKL_VM_LOCV_INC_NUM;
			FklVMvalue** locv=(FklVMvalue**)fklRealloc(exe->locv,sizeof(FklVMvalue*)*exe->llast);
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
	uint32_t lcount=FKL_VM_PROC(proc)->lcount;
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

static inline int64_t get_next_code(uint8_t* pc)
{
	if(*pc==FKL_OP_JMP)
		return 1+fklGetI64FromByteCode(pc+1)+sizeof(int64_t);
	else if(*pc==FKL_OP_CLOSE_REF)
		return 1+sizeof(uint32_t)+sizeof(uint32_t);
	return 1;
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
			for(;pc<end;pc+=get_next_code(pc))
				if(*pc!=FKL_OP_JMP&&*pc!=FKL_OP_CLOSE_REF)
					return 0;
		frame->u.c.tail=1;
	}
	return 1;
}

inline void fkl_dbg_print_link_back_trace(FklVMframe* t,FklSymbolTable* table)
{
	if(t->type==FKL_FRAME_COMPOUND)
	{
		if(t->u.c.sid)
			fklPrintString(fklGetSymbolWithId(t->u.c.sid,table)->symbol,stderr);
		else
			fputs("<lambda>",stderr);
		fprintf(stderr,"[%u,%u]",t->u.c.mark,t->u.c.tail);
	}
	else
		fputs("<obj>",stderr);

	for(FklVMframe* cur=t->prev;cur;cur=cur->prev)
	{
		fputs(" --> ",stderr);
		if(cur->type==FKL_FRAME_COMPOUND)
		{
			if(cur->u.c.sid)
				fklPrintString(fklGetSymbolWithId(cur->u.c.sid,table)->symbol,stderr);
			else
				fputs("<lambda>",stderr);
		fprintf(stderr,"[%u,%u]",cur->u.c.mark,cur->u.c.tail);
		}
		else
			fputs("<obj>",stderr);
	}
	fputc('\n',stderr);
}

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
		uint32_t lcount=FKL_VM_PROC(proc)->lcount;
		FklVMCompoundFrameVarRef* f=&tmpFrame->u.c.lr;
		f->base=exe->ltp;
		f->loc=fklAllocSpaceForLocalVar(exe,lcount);
		f->lcount=lcount;
		fklPushVMframe(tmpFrame,exe);
	}
}

static inline void dlproc_init_ccc(FklDlprocFrameContext* ccc,FklVMFuncK kFunc,void* ctx,size_t size)
{
	ccc->kFunc=kFunc;
	ccc->ctx=ctx;
	ccc->size=size;
}

static void dlproc_frame_print_backtrace(FklCallObjData data,FILE* fp,FklSymbolTable* table)
{
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)data;
	FklVMdlproc* dlproc=FKL_VM_DLPROC(c->proc);
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
	free(c->ctx);
}

static void dlproc_frame_copy(FklCallObjData d,const FklCallObjData s,FklVM* exe)
{
	FklDlprocFrameContext const* const sc=(FklDlprocFrameContext*)s;
	FklDlprocFrameContext* dc=(FklDlprocFrameContext*)d;
	FklVMgc* gc=exe->gc;
	dc->state=sc->state;
	dc->btp=sc->btp;
	fklSetRef(&dc->proc,sc->proc,gc);
	dc->kFunc=sc->kFunc;
	dc->size=sc->size;
	if(dc->size)
		dc->ctx=fklCopyMemory(sc->ctx,dc->size);
	dc->rtp=sc->rtp;

}

static int dlproc_frame_end(FklCallObjData data)
{
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)data;
	return c->state==FKL_DLPROC_DONE;
}

static void dlproc_frame_step(FklCallObjData data,FklVM* exe)
{
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)data;
	FklVMdlproc* dlproc=FKL_VM_DLPROC(c->proc);
	switch(c->state)
	{
		case FKL_DLPROC_READY:
			c->state=FKL_DLPROC_DONE;
			dlproc->func(exe,dlproc->dll,dlproc->pd);
			break;
		case FKL_DLPROC_CCC:
			c->state=FKL_DLPROC_DONE;
			c->kFunc(exe,FKL_CC_RE,c->ctx);
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

static inline void initDlprocFrameContext(FklCallObjData data,FklVMvalue* proc,FklVM* exe)
{
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)data;
	fklSetRef(&c->proc,proc,exe->gc);
	c->state=FKL_DLPROC_READY;
	c->btp=exe->tp;
	c->rtp=exe->tp;
	dlproc_init_ccc(c,NULL,NULL,0);
}

void callDlProc(FklVM* exe,FklVMvalue* dlproc)
{
	FklVMframe* f=&exe->sf;
	f->type=FKL_FRAME_OTHEROBJ;
	f->u.o.t=&DlprocContextMethodTable;
	f->errorCallBack=NULL;
	initDlprocFrameContext(f->u.o.data,dlproc,exe);
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
static void B_box0(BYTE_CODE_ARGS);

static void B_close_ref(BYTE_CODE_ARGS);

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
	B_box0,

	B_close_ref,
};

static inline void insert_to_VM_chain(FklVM* cur,FklVM* prev,FklVM* next)
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
		,FklSymbolTable* globalSymTable
		,FklFuncPrototypes* pts)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	exe->prev=exe;
	exe->next=exe;
	exe->pts=pts;
	exe->importingLib=NULL;
	exe->frames=NULL;
	exe->gc=fklCreateVMgc();
	exe->state=FKL_VM_READY;
	if(mainCode!=NULL)
	{
		FklVMvalue* codeObj=fklCreateVMvalueCodeObj(exe,mainCode);
		exe->frames=fklCreateVMframeWithCodeObj(codeObj,exe);
	}
	exe->symbolTable=globalSymTable;
	exe->builtinErrorTypeId=createBuiltinErrorTypeIdList();
	fklInitBuiltinErrorType(exe->builtinErrorTypeId,globalSymTable);
	exe->chan=NULL;
	fklInitVMstack(exe);
	exe->libNum=0;
	exe->libs=NULL;
	exe->locv=NULL;
	exe->ltp=0;
	exe->llast=0;
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

static inline FklVMframe* popFrame(FklVM* exe)
{
	FklVMframe* frame=exe->frames;
	exe->frames=frame->prev;
	return frame;
}

static inline void doAtomicFrame(FklVMframe* f,FklVMgc* gc)
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

static inline void callCallableObj(FklVMvalue* v,FklVM* exe)
{
	switch(v->type)
	{
		case FKL_TYPE_DLPROC:
			callDlProc(exe,v);
			break;
		case FKL_TYPE_USERDATA:
			FKL_VM_UD(v)->t->__call(v,exe);
			break;
		default:
			break;
	}
}

static inline void applyCompoundProc(FklVM* exe,FklVMvalue* proc,FklVMframe* frame)
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
		uint32_t lcount=FKL_VM_PROC(proc)->lcount;
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

inline void fklCallFuncK(FklVMFuncK kf
		,FklVM* v
		,void* ctx
		,uint32_t rtp)
{
	FklVMframe* sf=v->frames;
	FklVMframe* nf=fklCreateOtherObjVMframe(sf->u.o.t,sf->prev);
	nf->errorCallBack=sf->errorCallBack;
	fklDoCopyObjFrameContext(sf,nf,v);
	v->frames=nf;
	v->tp=((FklDlprocFrameContext*)(nf->u.o.data))->btp;
	((FklDlprocFrameContext*)(nf->u.o.data))->rtp=rtp;
	kf(v,FKL_CC_OK,ctx);
}

inline void fklCallFuncK1(FklVMFuncK kf
		,FklVM* v
		,void* ctx
		,uint32_t rtp
		,FklVMvalue* resultBox)
{
	FklVMframe* sf=v->frames;
	FklVMframe* nf=fklCreateOtherObjVMframe(sf->u.o.t,sf->prev);
	nf->errorCallBack=sf->errorCallBack;
	fklDoCopyObjFrameContext(sf,nf,v);
	v->frames=nf;
	v->tp=((FklDlprocFrameContext*)(nf->u.o.data))->btp;
	fklPushVMvalue(v,resultBox);
	((FklDlprocFrameContext*)(nf->u.o.data))->rtp=rtp;
	kf(v,FKL_CC_OK,ctx);
}

inline void fklCallFuncK2(FklVMFuncK kf
		,FklVM* v
		,void* ctx
		,uint32_t rtp
		,FklVMvalue* a
		,FklVMvalue* b)
{
	FklVMframe* sf=v->frames;
	FklVMframe* nf=fklCreateOtherObjVMframe(sf->u.o.t,sf->prev);
	nf->errorCallBack=sf->errorCallBack;
	fklDoCopyObjFrameContext(sf,nf,v);
	v->frames=nf;
	v->tp=((FklDlprocFrameContext*)(nf->u.o.data))->btp;
	fklPushVMvalue(v,a);
	fklPushVMvalue(v,b);
	((FklDlprocFrameContext*)(nf->u.o.data))->rtp=rtp;
	kf(v,FKL_CC_OK,ctx);
}

inline void fklCallFuncK3(FklVMFuncK kf
		,FklVM* v
		,void* ctx
		,uint32_t rtp
		,FklVMvalue* a
		,FklVMvalue* b
		,FklVMvalue* c)
{
	FklVMframe* sf=v->frames;
	FklVMframe* nf=fklCreateOtherObjVMframe(sf->u.o.t,sf->prev);
	nf->errorCallBack=sf->errorCallBack;
	fklDoCopyObjFrameContext(sf,nf,v);
	v->frames=nf;
	v->tp=((FklDlprocFrameContext*)(nf->u.o.data))->btp;
	fklPushVMvalue(v,a);
	fklPushVMvalue(v,b);
	fklPushVMvalue(v,c);
	((FklDlprocFrameContext*)(nf->u.o.data))->rtp=rtp;
	kf(v,FKL_CC_OK,ctx);
}

inline void fklFuncKReturn(FklVM* exe,uint32_t rtp,FklVMvalue* retval)
{
	exe->tp=rtp;
	fklPushVMvalue(exe,retval);
}

inline void fklContinueFuncK(FklVMframe* frame
		,FklVMFuncK kFunc
		,void* ctx
		,size_t size)
{
	FklDlprocFrameContext* c=(FklDlprocFrameContext*)frame->u.o.data;
	c->state=FKL_DLPROC_CCC;
	dlproc_init_ccc(c,kFunc,ctx,size);
}

inline void fklCallInFuncK(FklVMvalue* proc
		,uint32_t argNum
		,FklVMvalue* arglist[]
		,FklVMframe* frame
		,FklVM* exe
		,FklVMFuncK kFunc
		,void* ctx
		,size_t size)
{
	fklContinueFuncK(frame,kFunc,ctx,size);
	fklSetBp(exe);
	for(int64_t i=argNum-1;i>=0;i--)
		fklPushVMvalue(exe,arglist[i]);
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
	for(size_t i=1;i<=num;i++)
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
		FklVMchanl* tmpCh=FKL_VM_CHANL(exe->chan);
		FklVMvalue* v=fklGetTopValue(exe);
		FklVMvalue* resultBox=fklCreateVMvalueBox(next,v);
		fklChanlSend(resultBox,tmpCh,exe);

		fklDeleteCallChain(exe);
		fklUninitVMstack(exe);
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

int fklRunVM(FklVM* volatile exe)
{
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
						FklVMvalue* err=fklGetTopValue(exe);
						fklChanlSend(err,FKL_VM_CHANL(exe->chan),exe);
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

static inline void B_dummy(FklVM* exe,FklVMframe* frame)
{
	FKL_ASSERT(0);
}

static inline void B_push_nil(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe,FKL_VM_NIL);
}

static inline void B_push_pair(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* cdr=fklPopArg(exe);
	FklVMvalue* car=fklPopArg(exe);
	fklPushVMvalue(exe,fklCreateVMvaluePair(exe,car,cdr));
}

static inline void B_push_i32(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe,FKL_MAKE_VM_FIX(fklGetI32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(int32_t)))));
}

static inline void B_push_i64(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe,FKL_MAKE_VM_FIX(fklGetI64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(int64_t)))));
}

static inline void B_push_chr(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe,FKL_MAKE_VM_CHR(*(char*)(fklGetCompoundFrameCodeAndAdd(frame,sizeof(char)))));
}

static inline void B_push_f64(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe
			,fklCreateVMvalueF64(exe,fklGetF64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(double)))));
}

static inline void B_push_str(FklVM* exe,FklVMframe* frame)
{
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	fklPushVMvalue(exe
			,fklCreateVMvalueStr(exe,fklCreateString(size,(char*)fklGetCompoundFrameCodeAndAdd(frame,size))));
}

static inline void B_push_sym(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe,FKL_MAKE_VM_SYM(fklGetSidFromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(FklSid_t)))));
}

static inline void B_dup(FklVM* exe,FklVMframe* frame)
{
	if(exe->tp==exe->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.dup",FKL_ERR_STACKERROR,exe);
	FklVMvalue* val=fklPopArg(exe);
	fklPushVMvalue(exe,val);
	fklPushVMvalue(exe,val);
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

static inline FklVMvalue* get_compound_frame_code_obj(FklVMframe* frame)
{
	return FKL_VM_PROC(frame->u.c.proc)->codeObj;
}

inline FklVMvalue* fklCreateVMvalueProcWithFrame(FklVM* exe
		,FklVMframe* f
		,size_t cpc
		,uint32_t pid)
{
	FklVMvalue* codeObj=get_compound_frame_code_obj(f);
	FklFuncPrototype* pt=&exe->pts->pts[pid];
	FklVMvalue* r=fklCreateVMvalueProc(exe,fklGetCompoundFrameCode(f),cpc,codeObj,pid);
	uint32_t count=pt->rcount;
	FklVMCompoundFrameVarRef* lr=fklGetCompoundFrameLocRef(f);
	FklVMproc* proc=FKL_VM_PROC(r);
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
		proc->rcount=count;
	}
	return r;
}

static inline void B_push_proc(FklVM* exe,FklVMframe* frame)
{
	uint32_t prototypeId=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(prototypeId)));
	uint64_t sizeOfProc=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(sizeOfProc)));
	fklPushVMvalue(exe
			,fklCreateVMvalueProcWithFrame(exe
				,frame
				,sizeOfProc
				,prototypeId));
	fklAddCompoundFrameCp(frame,sizeOfProc);
}

inline void fklDropTop(FklVM* s)
{
	s->tp-=s->tp>0;
}

static inline void B_drop(FklVM* exe,FklVMframe* frame)
{
	fklDropTop(exe);
}

static inline FklVMvalue* volatile* get_compound_frame_loc(FklVMframe* frame,uint32_t idx)
{
	FklVMvalue** loc=frame->u.c.lr.loc;
	return &loc[idx];
}

static inline void B_pop_arg(FklVM* exe,FklVMframe* frame)
{
	if(exe->tp<=exe->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.pop-arg",FKL_ERR_TOOFEWARG,exe);
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint32_t)));
	FklVMvalue* v=fklPopArg(exe);
	*get_compound_frame_loc(frame,idx)=v;
}

static inline void B_pop_rest_arg(FklVM* exe,FklVMframe* frame)
{
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint32_t)));
	FklVMvalue* obj=FKL_VM_NIL;
	FklVMvalue* volatile* pValue=&obj;
	for(;exe->tp>exe->bp;pValue=&FKL_VM_CDR(*pValue))
		*pValue=fklCreateVMvaluePairWithCar(exe,fklPopArg(exe));
	*get_compound_frame_loc(frame,idx)=obj;
}

static inline void B_set_bp(FklVM* exe,FklVMframe* frame)
{
	fklSetBp(exe);
}

inline FklVMvalue* fklPopTopValue(FklVM* s)
{
	return s->base[--s->tp];
}

inline FklVMvalue* fklPopArg(FklVM* s)
{
	if(s->tp>s->bp)
		return s->base[--s->tp];
	return NULL;
}

inline int fklResBp(FklVM* exe)
{
	if(exe->tp>exe->bp)
		return 1;
	exe->bp=FKL_GET_FIX(exe->base[--exe->tp]);
	return 0;
}

static inline void B_res_bp(FklVM* exe,FklVMframe* frame)
{
	if(fklResBp(exe))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.res-bp",FKL_ERR_TOOMANYARG,exe);
}

static inline void B_call(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* tmpValue=fklPopArg(exe);
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
}

static inline void B_tail_call(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* tmpValue=fklPopArg(exe);
	if(!tmpValue)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.tail-call",FKL_ERR_TOOFEWARG,exe);
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
}

static inline void B_jmp_if_true(FklVM* exe,FklVMframe* frame)
{
	if(exe->tp&&fklGetTopValue(exe)!=FKL_VM_NIL)
		fklAddCompoundFrameCp(frame,fklGetI64FromByteCode(fklGetCompoundFrameCode(frame)));
	fklAddCompoundFrameCp(frame,sizeof(int64_t));
}

static inline void B_jmp_if_false(FklVM* exe,FklVMframe* frame)
{
	if(exe->tp&&fklGetTopValue(exe)==FKL_VM_NIL)
		fklAddCompoundFrameCp(frame,fklGetI64FromByteCode(fklGetCompoundFrameCode(frame)));
	fklAddCompoundFrameCp(frame,sizeof(int64_t));
}

static inline void B_jmp(FklVM* exe,FklVMframe* frame)
{
	fklAddCompoundFrameCp(frame
			,fklGetI64FromByteCode(fklGetCompoundFrameCode(frame))+sizeof(int64_t));
}

static inline void B_list_append(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* fir=fklPopArg(exe);
	FklVMvalue* sec=fklPopArg(exe);
	if(sec==FKL_VM_NIL)
		fklPushVMvalue(exe,fir);
	else
	{
		FklVMvalue** lastcdr=&sec;
		while(FKL_IS_PAIR(*lastcdr))
			lastcdr=&FKL_VM_CDR(*lastcdr);
		if(*lastcdr!=FKL_VM_NIL)
			FKL_RAISE_BUILTIN_ERROR_CSTR("b.list-append",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklSetRef(lastcdr,fir,exe->gc);
		fklPushVMvalue(exe,sec);
	}
}

static inline void B_push_vector(FklVM* exe,FklVMframe* frame)
{
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	FklVMvalue* vec=fklCreateVMvalueVec(exe,size);
	FklVMvec* v=FKL_VM_VEC(vec);
	for(uint64_t i=size;i>0;i--)
		fklSetRef(&v->base[i-1],fklPopArg(exe),exe->gc);
	fklPushVMvalue(exe,vec);
}

static inline void B_push_big_int(FklVM* exe,FklVMframe* frame)
{
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(num)));
	FklVMvalue* r=fklCreateVMvalueBigInt(exe,NULL);
	fklInitBigIntFromMem(FKL_VM_BI(r),fklGetCompoundFrameCodeAndAdd(frame,num),sizeof(uint8_t)*num);
	fklPushVMvalue(exe,r);
}

static inline void B_push_box(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* c=fklPopArg(exe);
	FklVMvalue* box=fklCreateVMvalueBox(exe,c);
	fklPushVMvalue(exe,box);
}

static inline void B_push_bytevector(FklVM* exe,FklVMframe* frame)
{
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	fklPushVMvalue(exe,fklCreateVMvalueBvec(exe,fklCreateBytevector(size,fklGetCompoundFrameCodeAndAdd(frame,size))));
}

static inline void B_push_hash_eq(FklVM* exe,FklVMframe* frame)
{
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	FklVMvalue* hash=fklCreateVMvalueHashEq(exe);
	uint64_t kvnum=num*2;
	FklVMvalue** base=&exe->base[exe->tp-kvnum];
	FklHashTable* ht=FKL_VM_HASH(hash);
	for(uint32_t i=0;i<kvnum;i+=2)
	{
		FklVMvalue* key=base[i];
		FklVMvalue* value=base[i+1];
		fklVMhashTableSet(key,value,ht,exe->gc);
	}
	exe->tp-=kvnum;
	fklPushVMvalue(exe,hash);
}

static inline void B_push_hash_eqv(FklVM* exe,FklVMframe* frame)
{
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	FklVMvalue* hash=fklCreateVMvalueHashEqv(exe);

	uint64_t kvnum=num*2;
	FklVMvalue** base=&exe->base[exe->tp-kvnum];
	FklHashTable* ht=FKL_VM_HASH(hash);
	for(uint32_t i=0;i<kvnum;i+=2)
	{
		FklVMvalue* key=base[i];
		FklVMvalue* value=base[i+1];
		fklVMhashTableSet(key,value,ht,exe->gc);
	}
	exe->tp-=kvnum;
	fklPushVMvalue(exe,hash);
}

static inline void B_push_hash_equal(FklVM* exe,FklVMframe* frame)
{
	uint64_t num=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	FklVMvalue* hash=fklCreateVMvalueHashEqual(exe);
	uint64_t kvnum=num*2;

	FklVMvalue** base=&exe->base[exe->tp-kvnum];
	FklHashTable* ht=FKL_VM_HASH(hash);
	for(size_t i=0;i<kvnum;i+=2)
	{
		FklVMvalue* key=base[i];
		FklVMvalue* value=base[i+1];
		fklVMhashTableSet(key,value,ht,exe->gc);
	}
	exe->tp-=kvnum;
	fklPushVMvalue(exe,hash);
}

static inline void B_push_list_0(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* pair=FKL_VM_NIL;
	FklVMvalue* last=fklPopArg(exe);
	FklVMvalue** pcur=&pair;
	size_t bp=exe->bp;
	for(size_t i=bp;exe->tp>bp;exe->tp--,i++)
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,exe->base[i]);
		pcur=&FKL_VM_CDR(*pcur);
	}
	fklSetRef(pcur,last,exe->gc);
	fklResBp(exe);
	fklPushVMvalue(exe,pair);
}

static inline void B_push_list(FklVM* exe,FklVMframe* frame)
{
	uint64_t size=fklGetU64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint64_t)));
	FklVMvalue* pair=FKL_VM_NIL;
	FklVMvalue* last=fklPopArg(exe);
	size--;
	FklVMvalue** pcur=&pair;
	for(size_t i=exe->tp-size;i<exe->tp;i++)
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,exe->base[i]);
		pcur=&FKL_VM_CDR(*pcur);
	}
	exe->tp-=size;
	fklSetRef(pcur,last,exe->gc);
	fklPushVMvalue(exe,pair);
}

static inline void B_push_vector_0(FklVM* exe,FklVMframe* frame)
{
	size_t size=exe->tp-exe->bp;
	FklVMvalue* vec=fklCreateVMvalueVec(exe,size);
	FklVMvec* vv=FKL_VM_VEC(vec);
	for(size_t i=size;i>0;i--)
		fklSetRef(&vv->base[i-1],fklPopArg(exe),exe->gc);
	fklResBp(exe);
	fklPushVMvalue(exe,vec);
}

static inline void B_list_push(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* list=fklPopArg(exe);
	for(;FKL_IS_PAIR(list);list=FKL_VM_CDR(list))
		fklPushVMvalue(exe,FKL_VM_CAR(list));
	if(list!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.list-push",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
}

static inline void init_import_env(FklVMframe* frame,FklVMlib* plib,FklVM* exe)
{
	fklAddCompoundFrameCp(frame,-1);
	callCompoundProcdure(exe,plib->proc,frame);
}

static inline void B_import(FklVM* exe,FklVMframe* frame)
{
	uint32_t locIdx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(locIdx)));
	uint32_t libIdx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(libIdx)));
	FklVMlib* plib=exe->importingLib;
	uint32_t count=plib->count;
	FklVMvalue* v=libIdx>=count?NULL:plib->loc[libIdx];
	*get_compound_frame_loc(frame,locIdx)=v;
}

static inline void B_load_lib(FklVM* exe,FklVMframe* frame)
{
	uint32_t libId=fklGetU32FromByteCode(fklGetCompoundFrameCode(frame));
	FklVMlib* plib=&exe->libs[libId];
	if(plib->imported)
	{
		exe->importingLib=plib;
		fklAddCompoundFrameCp(frame,sizeof(libId));
		fklPushVMvalue(exe,FKL_VM_NIL);
	}
	else
		init_import_env(frame,plib,exe);
}

static inline FklImportDllInitFunc getImportInit(FklDllHandle handle)
{
	return fklGetAddress("_fklImportInit",handle);
}

static inline void B_load_dll(FklVM* exe,FklVMframe* frame)
{
	uint32_t libId=fklGetU32FromByteCode(fklGetCompoundFrameCode(frame));
	FklVMlib* plib=&exe->libs[libId];
	if(!plib->imported)
	{
		char* realpath=fklStringToCstr(FKL_VM_STR(plib->proc));
		FklVMvalue* dll=fklCreateVMvalueDll(exe,realpath);
		FklImportDllInitFunc initFunc=NULL;
		if(dll)
			initFunc=getImportInit(FKL_VM_DLL(dll)->handle);
		else
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.load-dll",realpath,1,FKL_ERR_IMPORTFAILED,exe);
		if(!initFunc)
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.load-dll",realpath,1,FKL_ERR_IMPORTFAILED,exe);
		uint32_t tp=exe->tp;
		fklInitVMdll(dll,exe);
		plib->loc=initFunc(exe,dll,&plib->count);
		plib->imported=1;
		plib->belong=1;
		plib->proc=FKL_VM_NIL;
		free(realpath);
		exe->tp=tp;
	}
	exe->importingLib=plib;
	fklAddCompoundFrameCp(frame,sizeof(libId));
	fklPushVMvalue(exe,FKL_VM_NIL);
}

static inline void B_push_0(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe,FKL_MAKE_VM_FIX(0));
}

static inline void B_push_1(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe,FKL_MAKE_VM_FIX(1));
}

static inline void B_push_i8(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe,FKL_MAKE_VM_FIX(*(int8_t*)fklGetCompoundFrameCodeAndAdd(frame,sizeof(int8_t))));
}

static inline void B_push_i16(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe,FKL_MAKE_VM_FIX(fklGetI16FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(int16_t)))));
}

static inline void B_push_i64_big(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe,fklCreateVMvalueBigIntWithI64(exe,fklGetI64FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(int64_t)))));
}

static inline FklVMvalue* get_loc(FklVMframe* f,uint32_t idx)
{
	FklVMCompoundFrameVarRef* lr=&f->u.c.lr;
	return lr->loc[idx];
}

static inline void B_get_loc(FklVM* exe,FklVMframe* frame)
{
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(idx)));
	fklPushVMvalue(exe,get_loc(frame,idx));
}

static inline void B_put_loc(FklVM* exe,FklVMframe* frame)
{
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint32_t)));
	FklVMvalue* v=fklGetTopValue(exe);
	*get_compound_frame_loc(frame,idx)=v;
}

static inline FklVMvalue* get_var_val(FklVMframe* frame,uint32_t idx,FklFuncPrototypes* pts,FklSid_t* psid)
{
	FklVMCompoundFrameVarRef* lr=&frame->u.c.lr;
	FklVMvalue* v=idx<lr->rcount?*(lr->ref[idx]->ref):NULL;
	if(!v)
	{
		FklVMproc* proc=FKL_VM_PROC(fklGetCompoundFrameProc(frame));
		FklFuncPrototype* pt=&pts->pts[proc->protoId];
		FklSymbolDef* def=&pt->refs[idx];
		*psid=def->k.id;
		return NULL;
	}
	return v;
}

static inline void B_get_var_ref(FklVM* exe,FklVMframe* frame)
{
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(idx)));
	FklSid_t id=0;
	FklVMvalue* v=get_var_val(frame,idx,exe->pts,&id);
	if(id)
	{
		char* cstr=fklStringToCstr(fklGetSymbolWithId(id,exe->symbolTable)->symbol);
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.get-var-ref",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
	}
	fklPushVMvalue(exe,v);
}

static inline FklVMvalue* volatile* get_var_ref(FklVMframe* frame,uint32_t idx,FklFuncPrototypes* pts,FklSid_t* psid)
{
	FklVMCompoundFrameVarRef* lr=&frame->u.c.lr;
	FklVMvarRef** refs=lr->ref;
	FklVMvalue* volatile* v=(idx>=lr->rcount||!(refs[idx]->ref))?NULL:refs[idx]->ref;
	if(!v)
	{
		FklVMproc* proc=FKL_VM_PROC(fklGetCompoundFrameProc(frame));
		FklFuncPrototype* pt=&pts->pts[proc->protoId];
		FklSymbolDef* def=&pt->refs[idx];
		*psid=def->k.id;
		return NULL;
	}
	return v;
}

static inline void B_put_var_ref(FklVM* exe,FklVMframe* frame)
{
	uint32_t idx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(uint32_t)));
	FklSid_t id=0;
	FklVMvalue* volatile* pv=get_var_ref(frame,idx,exe->pts,&id);
	if(!pv)
	{
		char* cstr=fklStringToCstr(fklGetSymbolWithId(id,exe->symbolTable)->symbol);
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.put-var-ref",cstr,1,FKL_ERR_SYMUNDEFINE,exe);
	}
	FklVMvalue* v=fklGetTopValue(exe);
	*pv=v;
}

static inline void B_export(FklVM* exe,FklVMframe* frame)
{
	uint32_t libId=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(libId)));
	FklVMlib* lib=&exe->libs[libId];
	FklVMCompoundFrameVarRef* lr=fklGetCompoundFrameLocRef(frame);
	uint32_t count=lr->lcount;
	FklVMvalue** loc=fklCopyMemory(lr->loc,sizeof(FklVMvalue*)*count);
	fklDropTop(exe);
	lib->loc=loc;
	lib->count=count;
	lib->imported=1;
	lib->belong=1;
	FklVMproc* proc=FKL_VM_PROC(lib->proc);
	uint32_t rcount=proc->rcount;
	proc->rcount=0;
	FklVMvarRef** refs=proc->closure;
	proc->closure=NULL;
	for(uint32_t i=0;i<rcount;i++)
		fklDestroyVMvarRef(refs[i]);
	free(refs);
}

static inline void B_true(FklVM* exe,FklVMframe* frame)
{
	fklDropTop(exe);
	fklPushVMvalue(exe,FKL_MAKE_VM_FIX(1));
}

static inline void B_not(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* val=fklPopArg(exe);
	if(val==FKL_VM_NIL)
		fklPushVMvalue(exe,FKL_VM_TRUE);
	else
		fklPushVMvalue(exe,FKL_VM_NIL);
}

static inline void B_eq(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* fir=fklPopArg(exe);
	FklVMvalue* sec=fklPopArg(exe);
	fklPushVMvalue(exe
			,fklVMvalueEq(fir,sec)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_eqv(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* fir=fklPopArg(exe);
	FklVMvalue* sec=fklPopArg(exe);
	fklPushVMvalue(exe
			,(fklVMvalueEqv(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_equal(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* fir=fklPopArg(exe);
	FklVMvalue* sec=fklPopArg(exe);
	fklPushVMvalue(exe
			,(fklVMvalueEqual(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_eqn(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FklVMvalue* b=fklPopArg(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)==0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_eqn3(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FklVMvalue* b=fklPopArg(exe);
	FklVMvalue* c=fklPopArg(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)==0
		&&!err
		&&fklVMvalueCmp(b,c,&err)==0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_gt(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FklVMvalue* b=fklPopArg(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_gt3(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FklVMvalue* b=fklPopArg(exe);
	FklVMvalue* c=fklPopArg(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>0
		&&!err
		&&fklVMvalueCmp(b,c,&err)>0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_lt(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FklVMvalue* b=fklPopArg(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_lt3(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FklVMvalue* b=fklPopArg(exe);
	FklVMvalue* c=fklPopArg(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<0
		&&!err
		&&fklVMvalueCmp(b,c,&err)<0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_ge(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FklVMvalue* b=fklPopArg(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_ge3(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FklVMvalue* b=fklPopArg(exe);
	FklVMvalue* c=fklPopArg(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>=0
		&&!err
		&&fklVMvalueCmp(b,c,&err)>=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_le(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FklVMvalue* b=fklPopArg(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_le3(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FklVMvalue* b=fklPopArg(exe);
	FklVMvalue* c=fklPopArg(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<=0
		&&!err
		&&fklVMvalueCmp(b,c,&err)<=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	fklPushVMvalue(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_inc(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* arg=fklPopArg(exe);
	FKL_CHECK_TYPE(arg,fklIsVMnumber,"builtin.1+",exe);
	fklPushVMvalue(exe,fklProcessVMnumInc(exe,arg));
}

static inline void B_dec(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* arg=fklPopArg(exe);
	FKL_CHECK_TYPE(arg,fklIsVMnumber,"builtin.-1+",exe);
	fklPushVMvalue(exe,fklProcessVMnumDec(exe,arg));
}

#define PROCESS_ADD_RES() fklPushVMvalue(exe,fklProcessVMnumAddResult(exe,r64,rd,&bi))

#define PROCESS_ADD(VAR,WHO) if(fklProcessVMnumAdd(VAR,&r64,&rd,&bi))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHO,FKL_ERR_INCORRECT_TYPE_VALUE,exe)

static inline void B_add(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	FklVMvalue* b=fklPopArg(exe);
	PROCESS_ADD(a,"builtin.+");
	PROCESS_ADD(b,"builtin.+");
	PROCESS_ADD_RES();
}

static inline void B_add3(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FklVMvalue* b=fklPopArg(exe);
	FklVMvalue* c=fklPopArg(exe);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	PROCESS_ADD(a,"builtin.+");
	PROCESS_ADD(b,"builtin.+");
	PROCESS_ADD(c,"builtin.+");
	PROCESS_ADD_RES();
}

#define PROCESS_SUB_RES() fklPushVMvalue(exe,fklProcessVMnumSubResult(exe,a,r64,rd,&bi))

static inline void B_sub(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FKL_CHECK_TYPE(a,fklIsVMnumber,"builtin.-",exe);


	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);

	FklVMvalue* b=fklPopArg(exe);
	PROCESS_ADD(b,"builtin.-");
	PROCESS_SUB_RES();
}

static inline void B_sub3(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FKL_CHECK_TYPE(a,fklIsVMnumber,"builtin.-",exe);

	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);

	FklVMvalue* b=fklPopArg(exe);
	FklVMvalue* c=fklPopArg(exe);
	PROCESS_ADD(b,"builtin.-");
	PROCESS_ADD(c,"builtin.-");
	PROCESS_SUB_RES();
}

#undef PROCESS_SUB_RES

static inline void B_push_car(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* obj=fklPopArg(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.car",exe);
	fklPushVMvalue(exe,FKL_VM_CAR(obj));
}

static inline void B_push_cdr(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* obj=fklPopArg(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.cdr",exe);
	fklPushVMvalue(exe,FKL_VM_CDR(obj));
}

static inline void B_cons(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* car=fklPopArg(exe);
	FklVMvalue* cdr=fklPopArg(exe);
	fklPushVMvalue(exe,fklCreateVMvaluePair(exe,car,cdr));
}

static inline void B_nth(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* place=fklPopArg(exe);
	FklVMvalue* objlist=fklPopArg(exe);
	FKL_CHECK_TYPE(place,fklIsInt,"builtin.nth",exe);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_INVALIDACCESS,exe);
	size_t index=fklGetUint(place);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(uint64_t i=0;i<index&&FKL_IS_PAIR(objPair);i++,objPair=FKL_VM_CDR(objPair));
		if(FKL_IS_PAIR(objPair))
			fklPushVMvalue(exe,FKL_VM_CAR(objPair));
		else
			fklPushVMvalue(exe,FKL_VM_NIL);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
}

static inline void B_vec_ref(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* vec=fklPopArg(exe);
	FklVMvalue* place=fklPopArg(exe);
	if(!fklIsInt(place)||!FKL_IS_VECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklVMvec* vv=FKL_VM_VEC(vec);
	size_t size=vv->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vref",FKL_ERR_INVALIDACCESS,exe);
	fklPushVMvalue(exe,vv->base[index]);
}

static inline void B_str_ref(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* str=fklPopArg(exe);
	FklVMvalue* place=fklPopArg(exe);
	if(!fklIsInt(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklString* ss=FKL_VM_STR(str);
	size_t size=ss->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.sref",FKL_ERR_INVALIDACCESS,exe);
	fklPushVMvalue(exe,FKL_MAKE_VM_CHR(ss->str[index]));
}

static inline void B_box(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* obj=fklPopArg(exe);
	fklPushVMvalue(exe,fklCreateVMvalueBox(exe,obj));
}

static inline void B_box0(FklVM* exe,FklVMframe* frame)
{
	fklPushVMvalue(exe,fklCreateVMvalueBoxNil(exe));
}

static inline void B_unbox(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* box=fklPopArg(exe);
	FKL_CHECK_TYPE(box,FKL_IS_BOX,"builtin.unbox",exe);
	fklPushVMvalue(exe,FKL_VM_BOX(box));
}

static inline void B_add1(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	PROCESS_ADD(a,"builtin.+");
	PROCESS_ADD_RES();
}

static inline void B_neg(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	fklPushVMvalue(exe,fklProcessVMnumNeg(exe,a));
}

static inline void B_rec(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FKL_CHECK_TYPE(a,fklIsVMnumber,"builtin./",exe);
	FklVMvalue* r=fklProcessVMnumRec(exe,a);
	if(!r)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);
	fklPushVMvalue(exe,r);
}

#undef PROCESS_ADD
#undef PROCESS_ADD_RES

#define PROCESS_MUL(VAR,ERR) if(fklProcessVMnumMul(VAR,&r64,&rd,&bi))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(ERR,FKL_ERR_INCORRECT_TYPE_VALUE,exe)

#define PROCESS_MUL_RES() fklPushVMvalue(exe,fklProcessVMnumMulResult(exe,r64,rd,&bi))

static inline void B_mul1(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	PROCESS_MUL(a,"builtin.*");
	PROCESS_MUL_RES();
}

static inline void B_mul(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	FklVMvalue* b=fklPopArg(exe);
	PROCESS_MUL(a,"builtin.*");
	PROCESS_MUL(b,"builtin.*");
	PROCESS_MUL_RES();
}

static inline void B_mul3(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	FklVMvalue* b=fklPopArg(exe);
	FklVMvalue* c=fklPopArg(exe);
	PROCESS_MUL(a,"builtin.*");
	PROCESS_MUL(b,"builtin.*");
	PROCESS_MUL(c,"builtin.*");
	PROCESS_MUL_RES();
}

#define PROCESS_DIV_RES() if(r64==0||FKL_IS_0_BIG_INT(&bi)||!islessgreater(rd,0.0))\
{\
	fklUninitBigInt(&bi);\
	FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);\
}\
fklPushVMvalue(exe,fklProcessVMnumDivResult(exe,a,r64,rd,&bi))

static inline void B_div(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FKL_CHECK_TYPE(a,fklIsVMnumber,"builtin./",exe);

	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=fklPopArg(exe);
	PROCESS_MUL(b,"builtin./");
	PROCESS_DIV_RES();
}

static inline void B_div3(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FKL_CHECK_TYPE(a,fklIsVMnumber,"builtin./",exe);

	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=fklPopArg(exe);
	FklVMvalue* c=fklPopArg(exe);
	PROCESS_MUL(b,"builtin./");
	PROCESS_MUL(c,"builtin./");
	PROCESS_DIV_RES();
}

#define PROCESS_IMUL(VAR) if(fklProcessVMintMul(VAR,&r64,&bi))\
	FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_INCORRECT_TYPE_VALUE,exe)

#define PROCESS_IDIV_RES() if(r64==0||FKL_IS_0_BIG_INT(&bi))\
{\
	fklUninitBigInt(&bi);\
	FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.//",FKL_ERR_DIVZEROERROR,exe);\
}\
fklPushVMvalue(exe,fklProcessVMnumIdivResult(exe,a,r64,&bi))

static inline void B_idiv(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FKL_CHECK_TYPE(a,fklIsInt,"builtin.//",exe);

	int64_t r64=1;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=fklPopArg(exe);
	PROCESS_IMUL(b);
	PROCESS_IDIV_RES();
}

static inline void B_mod(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* fir=fklPopArg(exe);
	FklVMvalue* sec=fklPopArg(exe);
	if(!fklIsVMnumber(fir)||!fklIsVMnumber(sec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMvalue* r=fklProcessVMnumMod(exe,fir,sec);
	if(!r)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
	fklPushVMvalue(exe,r);
}

static inline void B_idiv3(FklVM* exe,FklVMframe* frame)
{
	FklVMvalue* a=fklPopArg(exe);
	FKL_CHECK_TYPE(a,fklIsInt,"builtin./",exe);

	int64_t r64=1;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=fklPopArg(exe);
	FklVMvalue* c=fklPopArg(exe);
	PROCESS_IMUL(b);
	PROCESS_IMUL(c);
	PROCESS_IDIV_RES();
}

#undef PROCESS_MUL
#undef PROCESS_MUL_RES
#undef PROCESS_SUB_RES

static inline void close_var_ref_between(FklVMvarRef** lref,uint32_t sIdx,uint32_t eIdx)
{
	if(lref)
		for(;sIdx<eIdx;sIdx++)
		{
			FklVMvarRef* ref=lref[sIdx];
			if(ref)
			{
				close_var_ref(ref);
				lref[sIdx]=NULL;
			}
		}
}

static inline void B_close_ref(FklVM* exe,FklVMframe* frame)
{
	uint32_t sIdx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(sIdx)));
	uint32_t eIdx=fklGetU32FromByteCode(fklGetCompoundFrameCodeAndAdd(frame,sizeof(eIdx)));
	close_var_ref_between(fklGetCompoundFrameLocRef(frame)->lref,sIdx,eIdx);
}

inline void fklInitVMstack(FklVM* tmp)
{
	tmp->last=FKL_VM_STACK_INC_NUM;
	tmp->size=FKL_VM_STACK_INC_SIZE;
	tmp->tp=0;
	tmp->bp=0;
	tmp->base=(FklVMvalue**)malloc(FKL_VM_STACK_INC_SIZE);
	FKL_ASSERT(tmp->base);
}

inline void fklAllocMoreStack(FklVM* s)
{
	if(s->tp>=s->last)
	{
		s->last+=FKL_VM_STACK_INC_NUM;
		s->size+=FKL_VM_STACK_INC_SIZE;
		s->base=(FklVMvalue**)fklRealloc(s->base,s->size);
		FKL_ASSERT(s->base);
	}
}

inline void fklShrinkStack(FklVM* stack)
{
	if(stack->last-stack->tp>FKL_VM_STACK_INC_NUM)
	{
		stack->last-=FKL_VM_STACK_INC_NUM;
		stack->size-=FKL_VM_STACK_INC_SIZE;
		stack->base=(FklVMvalue**)fklRealloc(stack->base,stack->size);
		FKL_ASSERT(stack->base);
	}
}

void fklDBG_printVMstack(FklVM* stack,FILE* fp,int mode,FklSymbolTable* table)
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

void fklDestroyAllValues(FklVMgc* gc)
{
	FklVMvalue** phead=&gc->head;
	FklVMvalue* destroyDll=NULL;
	while(*phead!=NULL)
	{
		FklVMvalue* cur=*phead;
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

FklVM* fklCreateThreadVM(FklVMvalue* nextCall
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
	exe->gc=prev->gc;
	exe->prev=exe;
	exe->next=exe;
	insert_to_VM_chain(exe,prev,next);
	exe->chan=fklCreateVMvalueChanl(exe,0);
	fklInitVMstack(exe);
	exe->symbolTable=table;
	exe->libNum=libNum;
	exe->builtinErrorTypeId=builtinErrorTypeId;
	exe->libs=copy_vm_libs(libs,libNum+1);
	exe->frames=NULL;
	exe->pts=prev->pts;
	exe->llast=0;
	exe->ltp=0;
	exe->locv=NULL;
	exe->state=FKL_VM_READY;
	fklCallObj(nextCall,NULL,exe);
	return exe;
}

inline void fklUninitVMstack(FklVM* s)
{
	free(s->base);
	s->base=NULL;
	s->last=0;
	s->tp=0;
	s->bp=0;
}

void fklDestroyAllVMs(FklVM* curVM)
{
	free(curVM->builtinErrorTypeId);
	fklDestroyFuncPrototypes(curVM->pts);
	curVM->prev->next=NULL;
	curVM->prev=NULL;
	for(FklVM* cur=curVM;cur;)
	{
		uninit_all_vm_lib(cur->libs,cur->libNum);
		fklDeleteCallChain(cur);
		fklUninitVMstack(cur);
		FklVM* t=cur;
		cur=cur->next;
		if(&t->gc->GcCo!=t)
		{
			free(t->locv);
			free(t->libs);
			free(t);
		}
	}
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

inline FklVMvalue** fklPushVMvalue(FklVM* s,FklVMvalue* v)
{
	fklAllocMoreStack(s);
	FklVMvalue** r=&s->base[s->tp];
	*r=v;
	s->tp+=1;
	return r;
}
#undef RECYCLE_NUN

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
		,FklVM* exe
		,uint32_t protoId)
{
	FklByteCode* bc=FKL_VM_CO(codeObj)->bc;
	FklVMvalue* proc=fklCreateVMvalueProc(exe,bc->code,bc->size,codeObj,protoId);
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
	uint32_t pId=FKL_VM_PROC(f->u.c.proc)->protoId;
	return &exe->pts->pts[pId];
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
	mainProc->rcount=count;
	mainProc->closure=fklCopyMemory(closure,sizeof(FklVMvalue*)*mainProc->rcount);
	for(uint32_t i=0;i<count;i++)
		fklMakeVMvarRefRef(closure[i]);
}

inline void fklSetBp(FklVM* s)
{
	fklPushVMvalue(s,FKL_MAKE_VM_FIX(s->bp));
	s->bp=s->tp;
}

// static inline FklVMvalue* get_value(uint32_t* ap,FklVM* s)
// {
// 	return *ap>0?s->base[--(*ap)]:NULL;
// }

// int fklDlprocResBp(uint32_t* ap,FklVM* stack)
// {
// 	if(*ap>stack->bp)
// 		return *ap-stack->bp;
// 	stack->bp=FKL_GET_FIX(get_value(ap,stack));
// 	return 0;
// }

// inline void fklDlprocSetBpWithTp(FklVM* s)
// {
// 	fklPushVMvalue(s,FKL_MAKE_VM_FIX(s->bp));
// 	s->bp=s->tp;
// }

// uint32_t fklDlprocSetBp(uint32_t nbp,FklVM* s)
// {
// 	fklPushVMvalue(s,FKL_MAKE_VM_FIX(s->bp));
// 	s->bp=nbp;
// 	return nbp;
// }

// FklVMvalue** fklDlprocReturn(FklVMvalue* v,uint32_t* ap,FklVM* s)
// {
// 	fklAllocMoreStack(s);
// 	FklVMvalue** r=&s->base[*ap];
// 	if(*ap<s->tp)
// 	{
// 		FklVMvalue* t=s->base[*ap];
// 		*r=v;
// 		s->base[s->tp]=t;
// 	}
// 	else
// 		*r=v;
// 	(*ap)++;
// 	s->tp++;
// 	return r;
// }

// inline void fklDlprocBegin(uint32_t* ap,FklVM* s)
// {
// 	*ap=s->tp;
// }

// inline void fklDlprocEnd(uint32_t* ap,FklVM* s)
// {
// 	s->tp=*ap;
// 	*ap=0;
// }

// FklVMvalue* fklDlprocGetArg(uint32_t*ap,FklVM* stack)
// {
// 	if(*ap>stack->bp)
// 		return stack->base[--(*ap)];
// 	return NULL;
// }

