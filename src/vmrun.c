#include<fakeLisp/opcode.h>
#include<fakeLisp/bytecode.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/common.h>
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
#include<unistd.h>
#endif
#include<time.h>
#include<setjmp.h>

static int VMargc=0;
static char** VMargv=NULL;

/*procedure call functions*/

static inline void push_old_locv(FklVM* exe,uint32_t llast,FklVMvalue** locv)
{
	if(llast)
	{
		FklVMlocvList* n=NULL;
		if(exe->old_locv_count<8)
			n=&exe->old_locv_cache[exe->old_locv_count];
		else
		{
			n=(FklVMlocvList*)malloc(sizeof(FklVMlocvList));
			FKL_ASSERT(n);
		}
		n->next=exe->old_locv_list;
		n->llast=llast;
		n->locv=locv;
		exe->old_locv_list=n;
		exe->old_locv_count++;
	}
}

FklVMvalue** fklAllocSpaceForLocalVar(FklVM* exe,uint32_t count)
{
	uint32_t nltp=exe->ltp+count;
	if(exe->llast<nltp)
	{
		uint32_t old_llast=exe->llast;
		exe->llast=nltp+FKL_VM_LOCV_INC_NUM;
		FklVMvalue** locv=fklAllocLocalVarSpaceFromGC(exe->gc,exe->llast,&exe->llast);
		FKL_ASSERT(locv);
		memcpy(locv,exe->locv,old_llast*sizeof(FklVMvalue*));
		fklUpdateAllVarRef(exe->frames,locv);
		push_old_locv(exe,old_llast,exe->locv);
		exe->locv=locv;
	}
	FklVMvalue** r=&exe->locv[exe->ltp];
	memset(r,0,sizeof(FklVMvalue**)*count);
	exe->ltp=nltp;
	return r;
}

FklVMvalue** fklAllocMoreSpaceForMainFrame(FklVM* exe,uint32_t count)
{
	if(count>exe->ltp)
	{
		uint32_t nltp=count;
		if(exe->llast<nltp)
		{
			uint32_t old_llast=exe->llast;
			exe->llast=nltp+FKL_VM_LOCV_INC_NUM;
			FklVMvalue** locv=fklAllocLocalVarSpaceFromGC(exe->gc,exe->llast,&exe->llast);
			FKL_ASSERT(locv);
			memcpy(locv,exe->locv,old_llast*sizeof(FklVMvalue*));
			fklUpdateAllVarRef(exe->frames,locv);
			push_old_locv(exe,old_llast,exe->locv);
			exe->locv=locv;
		}
		FklVMvalue** r=&exe->locv[exe->ltp];
		memset(r,0,sizeof(FklVMvalue**)*(count-exe->ltp));
		exe->ltp=nltp;
	}
	return exe->locv;
}

static void callCompoundProcdure(FklVM* exe,FklVMvalue* proc)
{
	FklVMframe* tmpFrame=fklCreateVMframeWithProcValue(proc,exe->frames);
	uint32_t lcount=FKL_VM_PROC(proc)->lcount;
	FklVMCompoundFrameVarRef* f=&tmpFrame->c.lr;
	f->base=exe->ltp;
	f->loc=fklAllocSpaceForLocalVar(exe,lcount);
	f->lcount=lcount;
	fklPushVMframe(tmpFrame,exe);
}

void fklSwapCompoundFrame(FklVMframe* a,FklVMframe* b)
{
	if(a!=b)
	{
		FklVMCompoundFrameData t=a->c;
		a->c=b->c;
		b->c=t;
	}
}

static inline int64_t get_next_code(FklInstruction* pc)
{
	if(pc->op==FKL_OP_JMP)
		return 1+pc->imm_i64;
	return 1;
}

static int is_last_expression(FklVMframe* frame)
{
	if(frame->type!=FKL_FRAME_COMPOUND)
		return 0;
	else if(!frame->c.tail)
	{
		FklInstruction* pc=fklGetCompoundFrameCode(frame);
		FklInstruction* end=fklGetCompoundFrameEnd(frame);

		if(pc[-1].op!=FKL_OP_TAIL_CALL)
			for(;pc<end&&pc->op!=FKL_OP_RET;pc+=get_next_code(pc))
				if(pc->op!=FKL_OP_JMP&&pc->op!=FKL_OP_CLOSE_REF)
					return 0;
		frame->c.tail=1;
	}
	return 1;
}

void fkl_dbg_print_link_back_trace(FklVMframe* t,FklSymbolTable* table)
{
	if(t->type==FKL_FRAME_COMPOUND)
	{
		if(t->c.sid)
			fklPrintString(fklGetSymbolWithId(t->c.sid,table)->symbol,stderr);
		else
			fputs("<lambda>",stderr);
		fprintf(stderr,"[%u,%u]",t->c.mark,t->c.tail);
	}
	else
		fputs("<obj>",stderr);

	for(FklVMframe* cur=t->prev;cur;cur=cur->prev)
	{
		fputs(" --> ",stderr);
		if(cur->type==FKL_FRAME_COMPOUND)
		{
			if(cur->c.sid)
				fklPrintString(fklGetSymbolWithId(cur->c.sid,table)->symbol,stderr);
			else
				fputs("<lambda>",stderr);
		fprintf(stderr,"[%u,%u]",cur->c.mark,cur->c.tail);
		}
		else
			fputs("<obj>",stderr);
	}
	fputc('\n',stderr);
}

static void tailCallCompoundProcdure(FklVM* exe,FklVMvalue* proc)
{
	FklVMframe* frame=exe->frames;
	FklVMframe* topframe=frame;
	topframe->c.tail=1;
	if(fklGetCompoundFrameProc(frame)==proc)
		frame->c.mark=1;
	else if((frame=fklHasSameProc(proc,frame->prev))&&(topframe->c.tail&=frame->c.tail))
	{
		frame->c.mark=1;
		fklSwapCompoundFrame(topframe,frame);
	}
	else
	{
		FklVMframe* tmpFrame=fklCreateVMframeWithProcValue(proc,exe->frames->prev);
		uint32_t lcount=FKL_VM_PROC(proc)->lcount;
		FklVMCompoundFrameVarRef* f=&tmpFrame->c.lr;
		f->base=exe->ltp;
		f->loc=fklAllocSpaceForLocalVar(exe,lcount);
		f->lcount=lcount;
		fklPushVMframe(tmpFrame,exe);
	}
}

static void cproc_frame_print_backtrace(void* data,FILE* fp,FklSymbolTable* table)
{
	FklCprocFrameContext* c=(FklCprocFrameContext*)data;
	FklVMcproc* cproc=FKL_VM_CPROC(c->proc);
	if(cproc->sid)
	{
		fprintf(fp,"at cproc:");
		fklPrintString(fklGetSymbolWithId(cproc->sid,table)->symbol
				,stderr);
		fputc('\n',fp);
	}
	else
		fputs("at <cproc>\n",fp);
}

static void cproc_frame_atomic(void* data,FklVMgc* gc)
{
	FklCprocFrameContext* c=(FklCprocFrameContext*)data;
	fklVMgcToGray(c->proc,gc);
}

static void cproc_frame_finalizer(void* data)
{
}

static void cproc_frame_copy(void* d,const void* s,FklVM* exe)
{
	FklCprocFrameContext const* const sc=(FklCprocFrameContext*)s;
	FklCprocFrameContext* dc=(FklCprocFrameContext*)d;
	*dc=*sc;
}

static int cproc_frame_end(void* data)
{
	FklCprocFrameContext* c=(FklCprocFrameContext*)data;
	return c->state==FKL_CPROC_DONE;
}

static void cproc_frame_step(void* data,FklVM* exe)
{
	FklCprocFrameContext* c=(FklCprocFrameContext*)data;
	switch(c->state)
	{
		case FKL_CPROC_READY:
			if(!c->func(exe,c))
				c->state=FKL_CPROC_DONE;
			break;
		case FKL_CPROC_DONE:
			break;
	}
}

static const FklVMframeContextMethodTable CprocContextMethodTable=
{
	.atomic=cproc_frame_atomic,
	.finalizer=cproc_frame_finalizer,
	.copy=cproc_frame_copy,
	.print_backtrace=cproc_frame_print_backtrace,
	.end=cproc_frame_end,
	.step=cproc_frame_step,
};

static inline void initCprocFrameContext(void* data,FklVMvalue* proc,FklVM* exe)
{
	FklCprocFrameContext* c=(FklCprocFrameContext*)data;
	fklSetRef(&c->proc,proc,exe->gc);
	c->state=FKL_CPROC_READY;
	c->rtp=exe->tp;
	c->context=0;
	c->func=FKL_VM_CPROC(proc)->func;
	c->pd=FKL_VM_CPROC(proc)->pd;
}

static inline void callCproc(FklVM* exe,FklVMvalue* cproc)
{
	FklVMframe* f=&exe->sf;
	f->type=FKL_FRAME_OTHEROBJ;
	f->t=&CprocContextMethodTable;
	f->errorCallBack=NULL;
	initCprocFrameContext(f->data,cproc,exe);
	fklPushVMframe(f,exe);
}

/*--------------------------*/

static void B_dummy(FKL_VM_INS_FUNC_ARGL);
static void B_push_nil(FKL_VM_INS_FUNC_ARGL);
static void B_push_pair(FKL_VM_INS_FUNC_ARGL);
static void B_push_i32(FKL_VM_INS_FUNC_ARGL);
static void B_push_i64(FKL_VM_INS_FUNC_ARGL);
static void B_push_chr(FKL_VM_INS_FUNC_ARGL);
static void B_push_f64(FKL_VM_INS_FUNC_ARGL);
static void B_push_str(FKL_VM_INS_FUNC_ARGL);
static void B_push_sym(FKL_VM_INS_FUNC_ARGL);
static void B_dup(FKL_VM_INS_FUNC_ARGL);
static void B_push_proc(FKL_VM_INS_FUNC_ARGL);
static void B_drop(FKL_VM_INS_FUNC_ARGL);
static void B_pop_arg(FKL_VM_INS_FUNC_ARGL);
static void B_pop_rest_arg(FKL_VM_INS_FUNC_ARGL);
static void B_set_bp(FKL_VM_INS_FUNC_ARGL);
static void B_call(FKL_VM_INS_FUNC_ARGL);
static void B_res_bp(FKL_VM_INS_FUNC_ARGL);
static void B_jmp_if_true(FKL_VM_INS_FUNC_ARGL);
static void B_jmp_if_false(FKL_VM_INS_FUNC_ARGL);
static void B_jmp(FKL_VM_INS_FUNC_ARGL);
static void B_list_append(FKL_VM_INS_FUNC_ARGL);
static void B_push_vector(FKL_VM_INS_FUNC_ARGL);
static void B_tail_call(FKL_VM_INS_FUNC_ARGL);
static void B_push_big_int(FKL_VM_INS_FUNC_ARGL);
static void B_push_box(FKL_VM_INS_FUNC_ARGL);
static void B_push_bytevector(FKL_VM_INS_FUNC_ARGL);
static void B_push_hash_eq(FKL_VM_INS_FUNC_ARGL);
static void B_push_hash_eqv(FKL_VM_INS_FUNC_ARGL);
static void B_push_hash_equal(FKL_VM_INS_FUNC_ARGL);
static void B_push_list_0(FKL_VM_INS_FUNC_ARGL);
static void B_push_list(FKL_VM_INS_FUNC_ARGL);
static void B_push_vector_0(FKL_VM_INS_FUNC_ARGL);
static void B_list_push(FKL_VM_INS_FUNC_ARGL);
static void B_import(FKL_VM_INS_FUNC_ARGL);
static void B_push_0(FKL_VM_INS_FUNC_ARGL);
static void B_push_1(FKL_VM_INS_FUNC_ARGL);
static void B_push_i8(FKL_VM_INS_FUNC_ARGL);
static void B_push_i16(FKL_VM_INS_FUNC_ARGL);
static void B_push_i64_big(FKL_VM_INS_FUNC_ARGL);
static void B_get_loc(FKL_VM_INS_FUNC_ARGL);
static void B_put_loc(FKL_VM_INS_FUNC_ARGL);
static void B_get_var_ref(FKL_VM_INS_FUNC_ARGL);
static void B_put_var_ref(FKL_VM_INS_FUNC_ARGL);
static void B_export(FKL_VM_INS_FUNC_ARGL);
static void B_load_lib(FKL_VM_INS_FUNC_ARGL);
static void B_load_dll(FKL_VM_INS_FUNC_ARGL);
static void B_true(FKL_VM_INS_FUNC_ARGL);
static void B_not(FKL_VM_INS_FUNC_ARGL);
static void B_eq(FKL_VM_INS_FUNC_ARGL);
static void B_eqv(FKL_VM_INS_FUNC_ARGL);
static void B_equal(FKL_VM_INS_FUNC_ARGL);
static void B_eqn(FKL_VM_INS_FUNC_ARGL);
static void B_eqn3(FKL_VM_INS_FUNC_ARGL);
static void B_gt(FKL_VM_INS_FUNC_ARGL);
static void B_gt3(FKL_VM_INS_FUNC_ARGL);
static void B_lt(FKL_VM_INS_FUNC_ARGL);
static void B_lt3(FKL_VM_INS_FUNC_ARGL);
static void B_ge(FKL_VM_INS_FUNC_ARGL);
static void B_ge3(FKL_VM_INS_FUNC_ARGL);
static void B_le(FKL_VM_INS_FUNC_ARGL);
static void B_le3(FKL_VM_INS_FUNC_ARGL);
static void B_inc(FKL_VM_INS_FUNC_ARGL);
static void B_dec(FKL_VM_INS_FUNC_ARGL);
static void B_add(FKL_VM_INS_FUNC_ARGL);
static void B_sub(FKL_VM_INS_FUNC_ARGL);
static void B_mul(FKL_VM_INS_FUNC_ARGL);
static void B_div(FKL_VM_INS_FUNC_ARGL);
static void B_idiv(FKL_VM_INS_FUNC_ARGL);
static void B_mod(FKL_VM_INS_FUNC_ARGL);
static void B_add1(FKL_VM_INS_FUNC_ARGL);
static void B_mul1(FKL_VM_INS_FUNC_ARGL);
static void B_neg(FKL_VM_INS_FUNC_ARGL);
static void B_rec(FKL_VM_INS_FUNC_ARGL);
static void B_add3(FKL_VM_INS_FUNC_ARGL);
static void B_sub3(FKL_VM_INS_FUNC_ARGL);
static void B_mul3(FKL_VM_INS_FUNC_ARGL);
static void B_div3(FKL_VM_INS_FUNC_ARGL);
static void B_idiv3(FKL_VM_INS_FUNC_ARGL);
static void B_push_car(FKL_VM_INS_FUNC_ARGL);
static void B_push_cdr(FKL_VM_INS_FUNC_ARGL);
static void B_cons(FKL_VM_INS_FUNC_ARGL);
static void B_nth(FKL_VM_INS_FUNC_ARGL);
static void B_vec_ref(FKL_VM_INS_FUNC_ARGL);
static void B_str_ref(FKL_VM_INS_FUNC_ARGL);
static void B_box(FKL_VM_INS_FUNC_ARGL);
static void B_unbox(FKL_VM_INS_FUNC_ARGL);
static void B_box0(FKL_VM_INS_FUNC_ARGL);
static void B_close_ref(FKL_VM_INS_FUNC_ARGL);
static void B_ret(FKL_VM_INS_FUNC_ARGL);

static FklVMinsFunc InsFuncTable[FKL_OP_LAST_OPCODE]=
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
	B_ret,
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
		,FklFuncPrototypes* pts
		,uint32_t pid)
{
	FklVM* exe=(FklVM*)malloc(sizeof(FklVM));
	FKL_ASSERT(exe);
	exe->obj_head=NULL;
	exe->obj_tail=NULL;
	exe->prev=exe;
	exe->next=exe;
	exe->pts=pts;
	exe->importingLib=NULL;
	exe->frames=NULL;
	exe->gc=fklCreateVMgc();
	if(mainCode!=NULL)
	{
		FklVMvalue* codeObj=fklCreateVMvalueCodeObj(exe,mainCode);
		exe->frames=fklCreateVMframeWithCodeObj(codeObj,exe,pid);
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
	exe->old_locv_list=NULL;
	exe->old_locv_count=0;
	exe->state=FKL_VM_READY;
	exe->notice_lock=0;
	memcpy(exe->ins_table,InsFuncTable,sizeof(InsFuncTable));
	uv_mutex_init(&exe->lock);
	return exe;
}

void* fklGetFrameData(FklVMframe* f)
{
	return f->data;
}

int fklIsCallableObjFrameReachEnd(FklVMframe* f)
{
	return f->t->end(fklGetFrameData(f));
}

void fklDoPrintBacktrace(FklVMframe* f,FILE* fp,FklSymbolTable* table)
{
	void (*backtrace)(void* data,FILE*,FklSymbolTable*)=f->t->print_backtrace;
	if(backtrace)
		backtrace(f->data,fp,table);
	else
		fprintf(fp,"at callable-obj\n");
}

void fklDoCallableObjFrameStep(FklVMframe* f,FklVM* exe)
{
	f->t->step(fklGetFrameData(f),exe);
}

void fklDoFinalizeObjFrame(FklVMframe* f,FklVMframe* sf)
{
	f->t->finalizer(fklGetFrameData(f));
	if(f!=sf)
		free(f);
}

static inline void close_var_ref(FklVMvarRef* ref)
{
	ref->v=*(ref->ref);
	atomic_store(&ref->ref,&ref->v);
}

static inline void close_all_var_ref(FklVMCompoundFrameVarRef* lr)
{
	for(FklVMvarRefList* l=lr->lrefl;l;)
	{
		close_var_ref(l->ref);
		fklDestroyVMvarRef(l->ref);
		FklVMvarRefList* c=l;
		l=c->next;
		free(c);
	}
}

void fklDoUninitCompoundFrame(FklVMframe* frame,FklVM* exe)
{
	FklVMCompoundFrameVarRef* lr=&frame->c.lr;
	close_all_var_ref(lr);
	free(lr->lref);
	exe->ltp-=lr->lcount;
}

void fklDoFinalizeCompoundFrame(FklVMframe* frame,FklVM* exe)
{
	fklDoUninitCompoundFrame(frame,exe);
	free(frame);
}

void fklDoCopyObjFrameContext(FklVMframe* s,FklVMframe* d,FklVM* exe)
{
	s->t->copy(fklGetFrameData(d),fklGetFrameData(s),exe);
}

void fklPushVMframe(FklVMframe* f,FklVM* exe)
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

void fklPopVMframe(FklVM* exe)
{
	FklVMframe* f=popFrame(exe);
	switch(f->type)
	{
		case FKL_FRAME_COMPOUND:
			fklDoFinalizeCompoundFrame(f,exe);
			break;
		case FKL_FRAME_OTHEROBJ:
			fklDoFinalizeObjFrame(f,&exe->sf);
			break;
	}
}

static inline void doAtomicFrame(FklVMframe* f,FklVMgc* gc)
{
	f->t->atomic(fklGetFrameData(f),gc);
}

void fklDoAtomicFrame(FklVMframe* f,FklVMgc* gc)
{
	switch(f->type)
	{
		case FKL_FRAME_COMPOUND:
			fklVMgcToGray(fklGetCompoundFrameProc(f),gc);
			break;
		case FKL_FRAME_OTHEROBJ:
			doAtomicFrame(f,gc);
			break;
	}
}

#define CALL_CALLABLE_OBJ(EXE,V) case FKL_TYPE_CPROC:\
		callCproc(EXE,V);\
		break;\
	case FKL_TYPE_USERDATA:\
		FKL_VM_UD(V)->t->__call(V,EXE);\
		break;\
	default:\
		break;

static inline void applyCompoundProc(FklVM* exe,FklVMvalue* proc)
{
	FklVMframe* frame=exe->frames;
	FklVMframe* prevProc=fklHasSameProc(proc,frame);
	if(frame&&frame->type==FKL_FRAME_COMPOUND
			&&(frame->c.tail=is_last_expression(frame))
			&&prevProc
			&&(frame->c.tail&=prevProc->c.tail))
	{
		prevProc->c.mark=1;
		fklSwapCompoundFrame(frame,prevProc);
	}
	else
	{
		FklVMframe* tmpFrame=fklCreateVMframeWithProcValue(proc,exe->frames);
		uint32_t lcount=FKL_VM_PROC(proc)->lcount;
		FklVMCompoundFrameVarRef* f=&tmpFrame->c.lr;
		f->base=exe->ltp;
		f->loc=fklAllocSpaceForLocalVar(exe,lcount);
		f->lcount=lcount;
		fklPushVMframe(tmpFrame,exe);
	}
}

void fklCallObj(FklVM* exe,FklVMvalue* proc)
{
	switch(proc->type)
	{
		case FKL_TYPE_PROC:
			applyCompoundProc(exe,proc);
			break;
			CALL_CALLABLE_OBJ(exe,proc);
	}
}

void fklTailCallObj(FklVM* exe,FklVMvalue* proc)
{
	FklVMframe* frame=exe->frames;
	exe->frames=frame->prev;
	fklDoFinalizeObjFrame(frame,&exe->sf);
	fklCallObj(exe,proc);
}

void fklCprocRestituteSframe(FklVM* v,uint32_t rtp)
{
	FklVMframe* sf=v->frames;
	if(&v->sf==sf)
	{
		FklVMframe* nf=fklCreateOtherObjVMframe(sf->t,sf->prev);
		nf->errorCallBack=sf->errorCallBack;
		fklDoCopyObjFrameContext(sf,nf,v);
		v->frames=nf;
		((FklCprocFrameContext*)(nf->data))->rtp=rtp;
	}
}

void fklSetTpAndPushValue(FklVM* exe,uint32_t rtp,FklVMvalue* retval)
{
	exe->tp=rtp+1;
	exe->base[rtp]=retval;
}

#define DO_STEP_VM(exe) {\
	FklVMframe* curframe=exe->frames;\
	switch(curframe->type)\
	{\
		case FKL_FRAME_COMPOUND:\
			{\
				FklInstruction* cur=curframe->c.pc++;\
				atomic_load(&ins_table[cur->op])(exe,cur);\
			}\
			break;\
		case FKL_FRAME_OTHEROBJ:\
			if(fklIsCallableObjFrameReachEnd(curframe))\
				fklDoFinalizeObjFrame(popFrame(exe),&exe->sf);\
			else\
				fklDoCallableObjFrameStep(curframe,exe);\
			break;\
	}\
	if(exe->frames==NULL)\
		exe->state=FKL_VM_EXIT;\
}

static inline void uninit_all_vm_lib(FklVMlib* libs,size_t num)
{
	for(size_t i=1;i<=num;i++)
		fklUninitVMlib(&libs[i]);
}

static inline void vm_running_thread_push(FklVM* v)
{
	FklVMqueue* q=v->vmq;
	uv_mutex_lock(&q->pre_running_lock);
	fklPushPtrQueue(v,&q->pre_running_q);
	uv_mutex_unlock(&q->pre_running_lock);
}

void fklResumeThread(FklVM* exe)
{
	exe->state=FKL_VM_READY;
}

void fklSuspendThread(FklVM* exe)
{
	uv_mutex_unlock(&exe->lock);
	exe->state=FKL_VM_WAITING;
}

static void vm_thread_cb(void* arg)
{
	FklVM* volatile exe=(FklVM*)arg;
	_Atomic(FklVMinsFunc)* const ins_table=exe->ins_table;
	for(;;)
	{
		switch(exe->state)
		{
			case FKL_VM_RUNNING:
				DO_STEP_VM(exe);
				break;
			case FKL_VM_EXIT:
				if(exe->chan)
				{
					FklVMchanl* tmpCh=FKL_VM_CHANL(exe->chan);
					FklVMvalue* v=FKL_VM_GET_TOP_VALUE(exe);
					FklVMvalue* resultBox=fklCreateVMvalueBox(exe,v);
					fklChanlSend(resultBox,tmpCh,exe);
				}
				atomic_fetch_sub(&exe->vmq->running_count,1);
				uv_mutex_unlock(&exe->lock);
				return;
				break;
			case FKL_VM_READY:
				uv_mutex_lock(&exe->lock);
				if(setjmp(exe->buf)==1)
				{
					FklVMvalue* ev=FKL_VM_POP_TOP_VALUE(exe);
					FklVMframe* frame=exe->frames;
					for(;frame;frame=frame->prev)
						if(frame->errorCallBack!=NULL&&frame->errorCallBack(frame,ev,exe))
							break;
					if(frame==NULL)
					{
						fklPrintErrBacktrace(ev,exe);
						if(exe->chan)
						{
							FklVMvalue* err=FKL_VM_GET_TOP_VALUE(exe);
							fklChanlSend(err,FKL_VM_CHANL(exe->chan),exe);
							exe->state=FKL_VM_EXIT;
							continue;
						}
						else
						{
							*(int*)(exe->loop->data)=255;
							exe->state=FKL_VM_EXIT;
							continue;
						}
					}
				}
				exe->state=FKL_VM_RUNNING;
				continue;
				break;
			case FKL_VM_WAITING:
				continue;
				break;
		}
	}
}

void fklVMworkStart(FklVM* exe,FklVMqueue* q)
{
	exe->vmq=q;
	vm_running_thread_push(exe);
}

static inline void remove_exited_thread(FklVMgc* gc)
{
	FklVM* main=gc->main_thread;
	FklVM* cur=main->next;
	while(cur!=main)
	{
		if(cur->state==FKL_VM_EXIT)
		{
			FklVM* prev=cur->prev;
			FklVM* next=cur->next;
			prev->next=next;
			next->prev=prev;

			fklDeleteCallChain(cur);
			fklUninitVMstack(cur);
			uninit_all_vm_lib(cur->libs,cur->libNum);
			free(cur->locv);
			free(cur->libs);
			free(cur);
			cur=next;
		}
		else
			cur=cur->next;
	}
}

static void join_all_vm_thread(uv_handle_t* handle)
{
	FklVMgc* gc=uv_handle_get_data(handle);
	FklVMqueue* q=&gc->q;
	for(FklQueueNode* n=fklPopPtrQueueNode(&q->running_q);n;n=fklPopPtrQueueNode(&q->running_q))
	{
		FklVM* exe=n->data;
		if(exe->state==FKL_VM_EXIT)
		{
			uv_thread_join(&exe->tid);
			free(n);
		}
	}
}

#define NOTICE_LOCK(EXE) {uv_mutex_unlock(&(EXE)->lock);uv_mutex_lock(&(EXE)->lock);}

static void B_notice_lock_call(FKL_VM_INS_FUNC_ARGL)
{
	NOTICE_LOCK(exe);
	FklVMvalue* proc=FKL_VM_POP_ARG(exe);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.call",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsCallable(proc))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.call",FKL_ERR_CALL_ERROR,exe);
	switch(proc->type)
	{
		case FKL_TYPE_PROC:
			callCompoundProcdure(exe,proc);
			break;
			CALL_CALLABLE_OBJ(exe,proc);
	}
}

static inline void B_notice_lock_tail_call(FKL_VM_INS_FUNC_ARGL)
{
	NOTICE_LOCK(exe);
	FklVMvalue* proc=FKL_VM_POP_ARG(exe);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.tail-call",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsCallable(proc))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.tail-call",FKL_ERR_CALL_ERROR,exe);
	switch(proc->type)
	{
		case FKL_TYPE_PROC:
			tailCallCompoundProcdure(exe,proc);
			break;
			CALL_CALLABLE_OBJ(exe,proc);
	}
}

static inline void B_notice_lock_jmp(FKL_VM_INS_FUNC_ARGL)
{
	if(ins->imm_i64<0)
		NOTICE_LOCK(exe);
	exe->frames->c.pc+=ins->imm_i64;
}

static inline void B_notice_lock_ret(FKL_VM_INS_FUNC_ARGL)
{
	NOTICE_LOCK(exe);
	FklVMframe* f=exe->frames;
	if(f->c.mark)
	{
		close_all_var_ref(&f->c.lr);
		if(f->c.lr.lrefl)
		{
			f->c.lr.lrefl=NULL;
			memset(f->c.lr.lref,0,sizeof(FklVMvarRef*)*f->c.lr.lcount);
		}
		f->c.pc=f->c.spc;
		f->c.mark=0;
		f->c.tail=0;
	}
	else
		fklDoFinalizeCompoundFrame(popFrame(exe),exe);
}

#undef NOTICE_LOCK

static inline void switch_notice_lock_ins(FklPtrQueue* q)
{
	for(FklQueueNode* n=q->head;n;n=n->next)
	{
		FklVM* exe=n->data;
		if(exe->notice_lock)
			continue;
		exe->notice_lock=1;
		atomic_store(&exe->ins_table[FKL_OP_CALL],B_notice_lock_call);
		atomic_store(&exe->ins_table[FKL_OP_TAIL_CALL],B_notice_lock_tail_call);
		atomic_store(&exe->ins_table[FKL_OP_JMP],B_notice_lock_jmp);
		atomic_store(&exe->ins_table[FKL_OP_RET],B_notice_lock_ret);
	}
}

static inline void switch_un_notice_lock_ins(FklPtrQueue* q)
{
	for(FklQueueNode* n=q->head;n;n=n->next)
	{
		FklVM* exe=n->data;
		if(exe->notice_lock)
		{
			exe->notice_lock=0;
			atomic_store(&exe->ins_table[FKL_OP_CALL],B_call);
			atomic_store(&exe->ins_table[FKL_OP_TAIL_CALL],B_tail_call);
			atomic_store(&exe->ins_table[FKL_OP_JMP],B_jmp);
			atomic_store(&exe->ins_table[FKL_OP_RET],B_ret);
		}
	}
}

static inline void lock_all_vm(FklPtrQueue* q)
{
	for(FklQueueNode* n=q->head;n;n=n->next)
		uv_mutex_lock(&((FklVM*)(n->data))->lock);
}

static inline void unlock_all_vm(FklPtrQueue* q)
{
	for(FklQueueNode* n=q->head;n;n=n->next)
		uv_mutex_unlock(&((FklVM*)(n->data))->lock);
}

static inline void move_thread_objects_to_gc(FklVM* vm,FklVMgc* gc)
{
	if(vm->obj_head)
	{
		vm->obj_tail->next=gc->head;
		gc->head=vm->obj_head;
		vm->obj_head=NULL;
		vm->obj_tail=NULL;
	}
}

static inline uint32_t compute_level_idx(uint32_t llast)
{
	uint32_t l=(llast/FKL_VM_LOCV_INC_NUM)-1;
	if(l>=8)
		return 4;
	else if(l&0x4)
		return 3;
	else if(l&0x2)
		return 2;
	else if(l&0x1)
		return 1;
	else
		return 0;
}

static inline void move_thread_old_locv_to_gc(FklVM* vm,FklVMgc* gc)
{
	FklVMlocvList* cur=vm->old_locv_list;
	struct FklLocvCacheLevel* locv_cache_level=gc->locv_cache;
	uint32_t i=vm->old_locv_count;
	for(;i>FKL_VM_GC_LOCV_CACHE_NUM;i--)
	{
		uint32_t llast=cur->llast;
		FklVMvalue** locv=cur->locv;

		uint32_t idx=compute_level_idx(llast);

		struct FklLocvCacheLevel* cur_locv_cache_level=&locv_cache_level[idx];
		uint32_t num=cur_locv_cache_level->num;
		struct FklLocvCache* locvs=cur_locv_cache_level->locv;

		uint8_t i=0;
		for(;i<num;i++)
		{
			if(llast<locvs[i].llast)
				break;
		}

		if(i<FKL_VM_GC_LOCV_CACHE_NUM)
		{
			if(num==FKL_VM_GC_LOCV_CACHE_NUM)
			{
				atomic_fetch_sub(&gc->num,locvs[FKL_VM_GC_LOCV_CACHE_LAST_IDX].llast);
				free(locvs[FKL_VM_GC_LOCV_CACHE_LAST_IDX].locv);
				num--;
			}
			else
				cur_locv_cache_level->num++;
			for(uint8_t j=num
					;j>i
					;j--)
				locvs[j]=locvs[j-1];
			locvs[i].llast=llast;
			locvs[i].locv=locv;
		}
		else
		{
			atomic_fetch_sub(&gc->num,llast);
			free(locv);
		}

		FklVMlocvList* prev=cur;
		cur=cur->next;
		free(prev);
	}

	for(uint32_t j=0;j<i;j++)
	{
		FklVMlocvList* cur=&vm->old_locv_cache[j];
		uint32_t llast=cur->llast;
		FklVMvalue** locv=cur->locv;

		uint32_t idx=compute_level_idx(llast);

		struct FklLocvCacheLevel* locv_cache=&locv_cache_level[idx];
		uint32_t num=locv_cache->num;
		struct FklLocvCache* locvs=locv_cache->locv;

		uint8_t i=0;
		for(;i<num;i++)
		{
			if(llast<locvs[i].llast)
				break;
		}

		if(i<FKL_VM_GC_LOCV_CACHE_NUM)
		{
			if(num==FKL_VM_GC_LOCV_CACHE_NUM)
			{
				atomic_fetch_sub(&gc->num,locvs[FKL_VM_GC_LOCV_CACHE_LAST_IDX].llast);
				free(locvs[FKL_VM_GC_LOCV_CACHE_LAST_IDX].locv);
				num--;
			}
			else
				locv_cache->num++;
			for(uint8_t j=num
					;j>i
					;j--)
				locvs[j]=locvs[j-1];
			locvs[i].llast=llast;
			locvs[i].locv=locv;
		}
		else
		{
			atomic_fetch_sub(&gc->num,llast);
			free(locv);
		}
	}
	vm->old_locv_list=NULL;
	vm->old_locv_count=0;
}

static inline void move_all_thread_objects_and_old_locv_to_gc(FklVMgc* gc)
{
	FklVM* vm=gc->main_thread;
	move_thread_objects_to_gc(vm,gc);
	move_thread_old_locv_to_gc(vm,gc);
	for(FklVM* cur=vm->next;cur!=vm;cur=cur->next)
	{
		move_thread_objects_to_gc(cur,gc);
		move_thread_old_locv_to_gc(cur,gc);
	}
}

static void vm_idler_cb(uv_idle_t* handle)
{
	FklVMgc* gc=uv_handle_get_data((uv_handle_t*)handle);
	FklVMqueue* q=&gc->q;
	if(atomic_load(&gc->num)>gc->threshold)
	{
#warning stw and gc
		switch_notice_lock_ins(&q->running_q);
		lock_all_vm(&q->running_q);

		move_all_thread_objects_and_old_locv_to_gc(gc);

		FklVM* exe=gc->main_thread;
		fklVMgcMarkAllRootToGray(exe);
		while(!fklVMgcPropagate(gc));
		FklVMvalue* white=NULL;
		fklVMgcCollect(gc,&white);
		fklVMgcSweep(white);
		fklVMgcRemoveUnusedGrayCache(gc);

		gc->threshold=gc->num+FKL_VM_GC_THRESHOLD_SIZE;

		FklPtrQueue other_running_q;
		fklInitPtrQueue(&other_running_q);

		for(FklQueueNode* n=fklPopPtrQueueNode(&q->running_q);n;n=fklPopPtrQueueNode(&q->running_q))
		{
			FklVM* exe=n->data;
			if(exe->state==FKL_VM_EXIT)
			{
				uv_thread_join(&exe->tid);
				free(n);
			}
			else
				fklPushPtrQueueNode(&other_running_q,n);
		}

		for(FklQueueNode* n=fklPopPtrQueueNode(&other_running_q);n;n=fklPopPtrQueueNode(&other_running_q))
			fklPushPtrQueueNode(&q->running_q,n);

		remove_exited_thread(gc);

		switch_un_notice_lock_ins(&q->running_q);
		unlock_all_vm(&q->running_q);
	}
	uv_mutex_lock(&q->pre_running_lock);
	for(FklQueueNode* n=fklPopPtrQueueNode(&q->pre_running_q);n;n=fklPopPtrQueueNode(&q->pre_running_q))
	{
		FklVM* exe=n->data;
		if(uv_thread_create(&exe->tid,vm_thread_cb,exe))
			abort();
		atomic_fetch_add(&q->running_count,1);
		fklPushPtrQueueNode(&q->running_q,n);
	}
	uv_mutex_unlock(&q->pre_running_lock);
	if(atomic_load(&q->running_count)==0)
		uv_close((uv_handle_t*)handle,join_all_vm_thread);
	return;
}

int fklRunVM(FklVM* volatile exe)
{
	FklVMgc* gc=exe->gc;
	int err=0;
	uv_loop_t loop;
	exe->loop=&loop;
	uv_loop_init(&loop);
	uv_handle_set_data((uv_handle_t*)&loop,&err);
	gc->main_thread=exe;
	fklVMworkStart(exe,&gc->q);

	uv_idle_t idler;
	uv_idle_init(&loop,&idler);
	uv_handle_set_data((uv_handle_t*)&idler,gc);
	uv_idle_start(&idler,vm_idler_cb);

	uv_run(&loop,UV_RUN_DEFAULT);
	uv_loop_close(&loop);
	return err;
}

void fklChangeGCstate(FklGCstate state,FklVMgc* gc)
{
	gc->running=state;
}

FklGCstate fklGetGCstate(FklVMgc* gc)
{
	FklGCstate state=FKL_GC_NONE;
	state=gc->running;
	return state;
}

static inline void B_dummy(FKL_VM_INS_FUNC_ARGL)
{
	abort();
}

static inline void B_push_nil(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
}

static inline void B_push_pair(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* cdr=FKL_VM_POP_ARG(exe);
	FklVMvalue* car=FKL_VM_POP_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe,car,cdr));
}

static inline void B_push_i32(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(ins->imm_i32));
}

static inline void B_push_i64(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(ins->imm_i64));
}

static inline void B_push_chr(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_CHR(ins->chr));
}

static inline void B_push_f64(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueF64(exe,ins->f64));
}

static inline void B_push_str(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueStr(exe,fklCopyString(ins->str)));
}

static inline void B_push_sym(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_SYM(ins->sid));
}

static inline void B_dup(FKL_VM_INS_FUNC_ARGL)
{
	if(exe->tp==exe->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.dup",FKL_ERR_STACKERROR,exe);
	FklVMvalue* val=FKL_VM_POP_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,val);
	FKL_VM_PUSH_VALUE(exe,val);
}

FklVMvarRef* fklMakeVMvarRefRef(FklVMvarRef* ref)
{
	ref->refc++;
	return ref;
}

void fklDestroyVMvarRef(FklVMvarRef* ref)
{
	if(ref->refc)
		ref->refc--;
	else
		free(ref);
}

FklVMvarRef* fklCreateVMvarRef(FklVMvalue** loc,uint32_t idx)
{
	FklVMvarRef* ref=(FklVMvarRef*)malloc(sizeof(FklVMvarRef));
	FKL_ASSERT(ref);
	ref->ref=&loc[idx];
	ref->v=NULL;
	ref->refc=0;
	ref->idx=idx;
	return ref;
}

FklVMvarRef* fklCreateClosedVMvarRef(FklVMvalue* v)
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
	return FKL_VM_PROC(frame->c.proc)->codeObj;
}

FklVMvalue* fklCreateVMvalueProcWithFrame(FklVM* exe
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

static inline void B_push_proc(FKL_VM_INS_FUNC_ARGL)
{
	uint32_t prototypeId=ins->imm;
	uint64_t sizeOfProc=ins->imm_u64;
	FKL_VM_PUSH_VALUE(exe
			,fklCreateVMvalueProcWithFrame(exe
				,exe->frames
				,sizeOfProc
				,prototypeId));
	fklAddCompoundFrameCp(exe->frames,sizeOfProc);
}

void fklDropTop(FklVM* s)
{
	s->tp-=s->tp>0;
}

#define DROP_TOP(S) (S)->tp--

static inline void B_drop(FKL_VM_INS_FUNC_ARGL)
{
	DROP_TOP(exe);
}

#define GET_COMPOUND_FRAME_LOC(F,IDX) (F)->c.lr.loc[IDX]

static inline void B_pop_arg(FKL_VM_INS_FUNC_ARGL)
{
	if(exe->tp<=exe->bp)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.pop-arg",FKL_ERR_TOOFEWARG,exe);
	FklVMvalue* v=FKL_VM_POP_ARG(exe);
	GET_COMPOUND_FRAME_LOC(exe->frames,ins->imm_u32)=v;
}

static inline void B_pop_rest_arg(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* obj=FKL_VM_NIL;
	FklVMvalue* volatile* pValue=&obj;
	for(;exe->tp>exe->bp;pValue=&FKL_VM_CDR(*pValue))
		*pValue=fklCreateVMvaluePairWithCar(exe,FKL_VM_POP_ARG(exe));
	GET_COMPOUND_FRAME_LOC(exe->frames,ins->imm_u32)=obj;
}

static inline void B_set_bp(FKL_VM_INS_FUNC_ARGL)
{
	fklSetBp(exe);
}

FklVMvalue* fklPopTopValue(FklVM* s)
{
	return s->base[--s->tp];
}

FklVMvalue* fklPopArg(FklVM* s)
{
	if(s->tp>s->bp)
		return s->base[--s->tp];
	return NULL;
}

uint32_t fklResBpIn(FklVM* exe,uint32_t n)
{
	uint32_t rtp=exe->tp-n-1;
	exe->bp=FKL_GET_FIX(exe->base[rtp]);
	return rtp;
}

int fklResBp(FklVM* exe)
{
	if(exe->tp>exe->bp)
		return 1;
	exe->bp=FKL_GET_FIX(exe->base[--exe->tp]);
	return 0;
}

static inline void B_res_bp(FKL_VM_INS_FUNC_ARGL)
{
	if(fklResBp(exe))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.res-bp",FKL_ERR_TOOMANYARG,exe);
}

static inline void B_call(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* proc=FKL_VM_POP_ARG(exe);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.call",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsCallable(proc))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.call",FKL_ERR_CALL_ERROR,exe);
	switch(proc->type)
	{
		case FKL_TYPE_PROC:
			callCompoundProcdure(exe,proc);
			break;
			CALL_CALLABLE_OBJ(exe,proc);
	}
}

static inline void B_tail_call(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* proc=FKL_VM_POP_ARG(exe);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.tail-call",FKL_ERR_TOOFEWARG,exe);
	if(!fklIsCallable(proc))
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.tail-call",FKL_ERR_CALL_ERROR,exe);
	switch(proc->type)
	{
		case FKL_TYPE_PROC:
			tailCallCompoundProcdure(exe,proc);
			break;
			CALL_CALLABLE_OBJ(exe,proc);
	}
}

#undef CALL_CALLABLE_OBJ

static inline void B_jmp_if_true(FKL_VM_INS_FUNC_ARGL)
{
	if(exe->tp&&FKL_VM_GET_TOP_VALUE(exe)!=FKL_VM_NIL)
		exe->frames->c.pc+=ins->imm_i64;
}

static inline void B_jmp_if_false(FKL_VM_INS_FUNC_ARGL)
{
	if(exe->tp&&FKL_VM_GET_TOP_VALUE(exe)==FKL_VM_NIL)
		exe->frames->c.pc+=ins->imm_i64;
}

static inline void B_jmp(FKL_VM_INS_FUNC_ARGL)
{
	exe->frames->c.pc+=ins->imm_i64;
}

static inline void B_list_append(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* fir=FKL_VM_POP_ARG(exe);
	FklVMvalue* sec=FKL_VM_POP_ARG(exe);
	if(sec==FKL_VM_NIL)
		FKL_VM_PUSH_VALUE(exe,fir);
	else
	{
		FklVMvalue** lastcdr=&sec;
		while(FKL_IS_PAIR(*lastcdr))
			lastcdr=&FKL_VM_CDR(*lastcdr);
		if(*lastcdr!=FKL_VM_NIL)
			FKL_RAISE_BUILTIN_ERROR_CSTR("b.list-append",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
		fklSetRef(lastcdr,fir,exe->gc);
		FKL_VM_PUSH_VALUE(exe,sec);
	}
}

static inline void B_push_vector(FKL_VM_INS_FUNC_ARGL)
{
	uint64_t size=ins->imm_u64;
	FklVMvalue* vec=fklCreateVMvalueVec(exe,size);
	FklVMvec* v=FKL_VM_VEC(vec);
	for(uint64_t i=size;i>0;i--)
		fklSetRef(&v->base[i-1],FKL_VM_POP_ARG(exe),exe->gc);
	FKL_VM_PUSH_VALUE(exe,vec);
}

static inline void B_push_big_int(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBigInt(exe,ins->bi));
}

static inline void B_push_box(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* c=FKL_VM_POP_ARG(exe);
	FklVMvalue* box=fklCreateVMvalueBox(exe,c);
	FKL_VM_PUSH_VALUE(exe,box);
}

static inline void B_push_bytevector(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBvec(exe,fklCopyBytevector(ins->bvec)));
}

static inline void B_push_hash_eq(FKL_VM_INS_FUNC_ARGL)
{
	uint64_t num=ins->imm_u64;
	FklVMvalue* hash=fklCreateVMvalueHashEq(exe);
	uint64_t kvnum=num*2;
	FklVMvalue** base=&exe->base[exe->tp-kvnum];
	FklHashTable* ht=FKL_VM_HASH(hash);
	for(uint32_t i=0;i<kvnum;i+=2)
	{
		FklVMvalue* key=base[i];
		FklVMvalue* value=base[i+1];
		fklVMhashTableSet(key,value,ht);
	}
	exe->tp-=kvnum;
	FKL_VM_PUSH_VALUE(exe,hash);
}

static inline void B_push_hash_eqv(FKL_VM_INS_FUNC_ARGL)
{
	uint64_t num=ins->imm_u64;
	FklVMvalue* hash=fklCreateVMvalueHashEqv(exe);

	uint64_t kvnum=num*2;
	FklVMvalue** base=&exe->base[exe->tp-kvnum];
	FklHashTable* ht=FKL_VM_HASH(hash);
	for(uint32_t i=0;i<kvnum;i+=2)
	{
		FklVMvalue* key=base[i];
		FklVMvalue* value=base[i+1];
		fklVMhashTableSet(key,value,ht);
	}
	exe->tp-=kvnum;
	FKL_VM_PUSH_VALUE(exe,hash);
}

static inline void B_push_hash_equal(FKL_VM_INS_FUNC_ARGL)
{
	uint64_t num=ins->imm_u64;
	FklVMvalue* hash=fklCreateVMvalueHashEqual(exe);
	uint64_t kvnum=num*2;

	FklVMvalue** base=&exe->base[exe->tp-kvnum];
	FklHashTable* ht=FKL_VM_HASH(hash);
	for(size_t i=0;i<kvnum;i+=2)
	{
		FklVMvalue* key=base[i];
		FklVMvalue* value=base[i+1];
		fklVMhashTableSet(key,value,ht);
	}
	exe->tp-=kvnum;
	FKL_VM_PUSH_VALUE(exe,hash);
}

static inline void B_push_list_0(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* pair=FKL_VM_NIL;
	FklVMvalue* last=FKL_VM_POP_ARG(exe);
	FklVMvalue** pcur=&pair;
	size_t bp=exe->bp;
	for(size_t i=bp;exe->tp>bp;exe->tp--,i++)
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,exe->base[i]);
		pcur=&FKL_VM_CDR(*pcur);
	}
	fklSetRef(pcur,last,exe->gc);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,pair);
}

static inline void B_push_list(FKL_VM_INS_FUNC_ARGL)
{
	uint64_t size=ins->imm_u64;
	FklVMvalue* pair=FKL_VM_NIL;
	FklVMvalue* last=FKL_VM_POP_ARG(exe);
	size--;
	FklVMvalue** pcur=&pair;
	for(size_t i=exe->tp-size;i<exe->tp;i++)
	{
		*pcur=fklCreateVMvaluePairWithCar(exe,exe->base[i]);
		pcur=&FKL_VM_CDR(*pcur);
	}
	exe->tp-=size;
	fklSetRef(pcur,last,exe->gc);
	FKL_VM_PUSH_VALUE(exe,pair);
}

static inline void B_push_vector_0(FKL_VM_INS_FUNC_ARGL)
{
	size_t size=exe->tp-exe->bp;
	FklVMvalue* vec=fklCreateVMvalueVec(exe,size);
	FklVMvec* vv=FKL_VM_VEC(vec);
	for(size_t i=size;i>0;i--)
		fklSetRef(&vv->base[i-1],FKL_VM_POP_ARG(exe),exe->gc);
	fklResBp(exe);
	FKL_VM_PUSH_VALUE(exe,vec);
}

static inline void B_list_push(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* list=FKL_VM_POP_ARG(exe);
	for(;FKL_IS_PAIR(list);list=FKL_VM_CDR(list))
		FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(list));
	if(list!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR_CSTR("b.list-push",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
}

static inline void B_import(FKL_VM_INS_FUNC_ARGL)
{
	uint32_t locIdx=ins->imm;
	uint32_t libIdx=ins->imm_u32;
	FklVMlib* plib=exe->importingLib;
	uint32_t count=plib->count;
	FklVMvalue* v=libIdx>=count?NULL:plib->loc[libIdx];
	GET_COMPOUND_FRAME_LOC(exe->frames,locIdx)=v;
}

static inline void B_load_lib(FKL_VM_INS_FUNC_ARGL)
{
	uint32_t libId=ins->imm_u32;
	FklVMlib* plib=&exe->libs[libId];
	if(plib->imported)
	{
		exe->importingLib=plib;
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	else
		callCompoundProcdure(exe,plib->proc);
}

static inline FklImportDllInitFunc getImportInit(uv_lib_t* handle)
{
	return fklGetAddress("_fklImportInit",handle);
}

static inline void B_load_dll(FKL_VM_INS_FUNC_ARGL)
{
	uint32_t libId=ins->imm_u32;
	FklVMlib* plib=&exe->libs[libId];
	if(!plib->imported)
	{
		FklString* realpath=FKL_VM_STR(plib->proc);
		char* errorStr=NULL;
		FklVMvalue* dll=fklCreateVMvalueDll(exe,realpath->str,&errorStr);
		FklImportDllInitFunc initFunc=NULL;
		if(dll)
			initFunc=getImportInit(&(FKL_VM_DLL(dll)->dll));
		else
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.load-dll",errorStr?errorStr:realpath->str,errorStr!=NULL,FKL_ERR_IMPORTFAILED,exe);
		if(!initFunc)
			FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.load-dll",realpath->str,0,FKL_ERR_IMPORTFAILED,exe);
		uint32_t tp=exe->tp;
		fklInitVMdll(dll,exe);
		plib->loc=initFunc(exe,dll,&plib->count);
		plib->imported=1;
		plib->belong=1;
		plib->proc=FKL_VM_NIL;
		exe->tp=tp;
	}
	exe->importingLib=plib;
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
}

static inline void B_push_0(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(0));
}

static inline void B_push_1(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(1));
}

static inline void B_push_i8(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(ins->imm_i8));
}

static inline void B_push_i16(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(ins->imm_i16));
}

static inline void B_push_i64_big(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBigIntWithI64(exe,ins->imm_i64));
}

static inline void B_get_loc(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,exe->frames->c.lr.loc[ins->imm_u32]);
}

static inline void B_put_loc(FKL_VM_INS_FUNC_ARGL)
{
	GET_COMPOUND_FRAME_LOC(exe->frames,ins->imm_u32)=FKL_VM_GET_TOP_VALUE(exe);
}

static inline FklVMvalue* get_var_val(FklVMframe* frame,uint32_t idx,FklFuncPrototypes* pts,FklSid_t* psid)
{
	FklVMCompoundFrameVarRef* lr=&frame->c.lr;
	FklVMvalue* v=idx<lr->rcount?*atomic_load(&lr->ref[idx]->ref):NULL;
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

static inline void B_get_var_ref(FKL_VM_INS_FUNC_ARGL)
{
	FklSid_t id=0;
	FklVMvalue* v=get_var_val(exe->frames,ins->imm_u32,exe->pts,&id);
	if(id)
	{
		FklString* str=fklGetSymbolWithId(id,exe->symbolTable)->symbol;
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.get-var-ref",str->str,0,FKL_ERR_SYMUNDEFINE,exe);
	}
	FKL_VM_PUSH_VALUE(exe,v);
}

static inline FklVMvalue* volatile* get_var_ref(FklVMframe* frame,uint32_t idx,FklFuncPrototypes* pts,FklSid_t* psid)
{
	FklVMCompoundFrameVarRef* lr=&frame->c.lr;
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

static inline void B_put_var_ref(FKL_VM_INS_FUNC_ARGL)
{
	FklSid_t id=0;
	FklVMvalue* volatile* pv=get_var_ref(exe->frames,ins->imm_u32,exe->pts,&id);
	if(!pv)
	{
		FklString* str=fklGetSymbolWithId(id,exe->symbolTable)->symbol;
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR_CSTR("b.put-var-ref",str->str,0,FKL_ERR_SYMUNDEFINE,exe);
	}
	FklVMvalue* v=FKL_VM_GET_TOP_VALUE(exe);
	*pv=v;
}

static inline void B_export(FKL_VM_INS_FUNC_ARGL)
{
	uint32_t libId=ins->imm_u32;
	FklVMlib* lib=&exe->libs[libId];
	FklVMCompoundFrameVarRef* lr=fklGetCompoundFrameLocRef(exe->frames);
	uint32_t count=lr->lcount;
	FklVMvalue** loc=fklCopyMemory(lr->loc,sizeof(FklVMvalue*)*count);
	DROP_TOP(exe);
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
	exe->importingLib=lib;
	FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
}

static inline void B_true(FKL_VM_INS_FUNC_ARGL)
{
	DROP_TOP(exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_FIX(1));
}

static inline void B_not(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* val=FKL_VM_POP_ARG(exe);
	if(val==FKL_VM_NIL)
		FKL_VM_PUSH_VALUE(exe,FKL_VM_TRUE);
	else
		FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
}

static inline void B_eq(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* fir=FKL_VM_POP_ARG(exe);
	FklVMvalue* sec=FKL_VM_POP_ARG(exe);
	FKL_VM_PUSH_VALUE(exe
			,fklVMvalueEq(fir,sec)
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_eqv(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* fir=FKL_VM_POP_ARG(exe);
	FklVMvalue* sec=FKL_VM_POP_ARG(exe);
	FKL_VM_PUSH_VALUE(exe
			,(fklVMvalueEqv(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_equal(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* fir=FKL_VM_POP_ARG(exe);
	FklVMvalue* sec=FKL_VM_POP_ARG(exe);
	FKL_VM_PUSH_VALUE(exe
			,(fklVMvalueEqual(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_eqn(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)==0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_eqn3(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	FklVMvalue* c=FKL_VM_POP_ARG(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)==0
		&&!err
		&&fklVMvalueCmp(b,c,&err)==0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_gt(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_gt3(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	FklVMvalue* c=FKL_VM_POP_ARG(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>0
		&&!err
		&&fklVMvalueCmp(b,c,&err)>0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_lt(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_lt3(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	FklVMvalue* c=FKL_VM_POP_ARG(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<0
		&&!err
		&&fklVMvalueCmp(b,c,&err)<0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_ge(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_ge3(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	FklVMvalue* c=FKL_VM_POP_ARG(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)>=0
		&&!err
		&&fklVMvalueCmp(b,c,&err)>=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.>=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_le(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_le3(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	FklVMvalue* c=FKL_VM_POP_ARG(exe);
	int err=0;
	int r=fklVMvalueCmp(a,b,&err)<=0
		&&!err
		&&fklVMvalueCmp(b,c,&err)<=0;
	if(err)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.<=",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FKL_VM_PUSH_VALUE(exe
			,r
			?FKL_VM_TRUE
			:FKL_VM_NIL);
}

static inline void B_inc(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* arg=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=fklProcessVMnumInc(exe,arg);
	if(r)
		FKL_VM_PUSH_VALUE(exe,r);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.1+",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
}

static inline void B_dec(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* arg=FKL_VM_POP_ARG(exe);
	FklVMvalue* r=fklProcessVMnumDec(exe,arg);
	if(r)
		FKL_VM_PUSH_VALUE(exe,r);
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.-1+",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
}

#define PROCESS_ADD_RES() FKL_VM_PUSH_VALUE(exe,fklProcessVMnumAddResult(exe,r64,rd,&bi))

#define PROCESS_ADD(VAR,WHERE) if(fklProcessVMnumAdd(VAR,&r64,&rd,&bi))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHERE,FKL_ERR_INCORRECT_TYPE_VALUE,exe)

static inline void B_add(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	PROCESS_ADD(a,"builtin.+");
	PROCESS_ADD(b,"builtin.+");
	PROCESS_ADD_RES();
}

static inline void B_add3(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	FklVMvalue* c=FKL_VM_POP_ARG(exe);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	PROCESS_ADD(a,"builtin.+");
	PROCESS_ADD(b,"builtin.+");
	PROCESS_ADD(c,"builtin.+");
	PROCESS_ADD_RES();
}

#define PROCESS_SUB_RES() FKL_VM_PUSH_VALUE(exe,fklProcessVMnumSubResult(exe,a,r64,rd,&bi))

static inline void B_sub(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FKL_CHECK_TYPE(a,fklIsVMnumber,"builtin.-",exe);


	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);

	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	PROCESS_ADD(b,"builtin.-");
	PROCESS_SUB_RES();
}

static inline void B_sub3(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FKL_CHECK_TYPE(a,fklIsVMnumber,"builtin.-",exe);

	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);

	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	FklVMvalue* c=FKL_VM_POP_ARG(exe);
	PROCESS_ADD(b,"builtin.-");
	PROCESS_ADD(c,"builtin.-");
	PROCESS_SUB_RES();
}

#undef PROCESS_SUB_RES

static inline void B_push_car(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.car",exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(obj));
}

static inline void B_push_cdr(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FKL_CHECK_TYPE(obj,FKL_IS_PAIR,"builtin.cdr",exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_CDR(obj));
}

static inline void B_cons(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* car=FKL_VM_POP_ARG(exe);
	FklVMvalue* cdr=FKL_VM_POP_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvaluePair(exe,car,cdr));
}

static inline void B_nth(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* place=FKL_VM_POP_ARG(exe);
	FklVMvalue* objlist=FKL_VM_POP_ARG(exe);
	FKL_CHECK_TYPE(place,fklIsVMint,"builtin.nth",exe);
	if(fklVMnumberLt0(place))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_INVALIDACCESS,exe);
	size_t index=fklGetUint(place);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		for(uint64_t i=0;i<index&&FKL_IS_PAIR(objPair);i++,objPair=FKL_VM_CDR(objPair));
		if(FKL_IS_PAIR(objPair))
			FKL_VM_PUSH_VALUE(exe,FKL_VM_CAR(objPair));
		else
			FKL_VM_PUSH_VALUE(exe,FKL_VM_NIL);
	}
	else
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.nth",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
}

static inline void B_vec_ref(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* vec=FKL_VM_POP_ARG(exe);
	FklVMvalue* place=FKL_VM_POP_ARG(exe);
	if(!fklIsVMint(place)||!FKL_IS_VECTOR(vec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vec-ref",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklVMvec* vv=FKL_VM_VEC(vec);
	size_t size=vv->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.vec-ref",FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,vv->base[index]);
}

static inline void B_str_ref(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* str=FKL_VM_POP_ARG(exe);
	FklVMvalue* place=FKL_VM_POP_ARG(exe);
	if(!fklIsVMint(place)||!FKL_IS_STR(str))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.str-ref",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	size_t index=fklGetUint(place);
	FklString* ss=FKL_VM_STR(str);
	size_t size=ss->size;
	if(fklVMnumberLt0(place)||index>=size)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.str-ref",FKL_ERR_INVALIDACCESS,exe);
	FKL_VM_PUSH_VALUE(exe,FKL_MAKE_VM_CHR(ss->str[index]));
}

static inline void B_box(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* obj=FKL_VM_POP_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBox(exe,obj));
}

static inline void B_box0(FKL_VM_INS_FUNC_ARGL)
{
	FKL_VM_PUSH_VALUE(exe,fklCreateVMvalueBoxNil(exe));
}

static inline void B_unbox(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* box=FKL_VM_POP_ARG(exe);
	FKL_CHECK_TYPE(box,FKL_IS_BOX,"builtin.unbox",exe);
	FKL_VM_PUSH_VALUE(exe,FKL_VM_BOX(box));
}

static inline void B_add1(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	int64_t r64=0;
	double rd=0.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt0(&bi);
	PROCESS_ADD(a,"builtin.+");
	PROCESS_ADD_RES();
}

static inline void B_neg(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FKL_VM_PUSH_VALUE(exe,fklProcessVMnumNeg(exe,a));
}

static inline void B_rec(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FKL_CHECK_TYPE(a,fklIsVMnumber,"builtin./",exe);
	FklVMvalue* r=fklProcessVMnumRec(exe,a);
	if(!r)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin./",FKL_ERR_DIVZEROERROR,exe);
	FKL_VM_PUSH_VALUE(exe,r);
}

#undef PROCESS_ADD
#undef PROCESS_ADD_RES

#define PROCESS_MUL(VAR,WHERE) if(fklProcessVMnumMul(VAR,&r64,&rd,&bi))\
	FKL_RAISE_BUILTIN_ERROR_CSTR(WHERE,FKL_ERR_INCORRECT_TYPE_VALUE,exe)

#define PROCESS_MUL_RES() FKL_VM_PUSH_VALUE(exe,fklProcessVMnumMulResult(exe,r64,rd,&bi))

static inline void B_mul1(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	PROCESS_MUL(a,"builtin.*");
	PROCESS_MUL_RES();
}

static inline void B_mul(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	PROCESS_MUL(a,"builtin.*");
	PROCESS_MUL(b,"builtin.*");
	PROCESS_MUL_RES();
}

static inline void B_mul3(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);
	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	FklVMvalue* c=FKL_VM_POP_ARG(exe);
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
FKL_VM_PUSH_VALUE(exe,fklProcessVMnumDivResult(exe,a,r64,rd,&bi))

static inline void B_div(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FKL_CHECK_TYPE(a,fklIsVMnumber,"builtin./",exe);

	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	PROCESS_MUL(b,"builtin./");
	PROCESS_DIV_RES();
}

static inline void B_div3(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FKL_CHECK_TYPE(a,fklIsVMnumber,"builtin./",exe);

	int64_t r64=1;
	double rd=1.0;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	FklVMvalue* c=FKL_VM_POP_ARG(exe);
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
FKL_VM_PUSH_VALUE(exe,fklProcessVMnumIdivResult(exe,a,r64,&bi))

static inline void B_idiv(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FKL_CHECK_TYPE(a,fklIsVMint,"builtin.//",exe);

	int64_t r64=1;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	PROCESS_IMUL(b);
	PROCESS_IDIV_RES();
}

static inline void B_mod(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* fir=FKL_VM_POP_ARG(exe);
	FklVMvalue* sec=FKL_VM_POP_ARG(exe);
	if(!fklIsVMnumber(fir)||!fklIsVMnumber(sec))
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_INCORRECT_TYPE_VALUE,exe);
	FklVMvalue* r=fklProcessVMnumMod(exe,fir,sec);
	if(!r)
		FKL_RAISE_BUILTIN_ERROR_CSTR("builtin.%",FKL_ERR_DIVZEROERROR,exe);
	FKL_VM_PUSH_VALUE(exe,r);
}

static inline void B_idiv3(FKL_VM_INS_FUNC_ARGL)
{
	FklVMvalue* a=FKL_VM_POP_ARG(exe);
	FKL_CHECK_TYPE(a,fklIsVMint,"builtin./",exe);

	int64_t r64=1;
	FklBigInt bi=FKL_BIG_INT_INIT;
	fklInitBigInt1(&bi);

	FklVMvalue* b=FKL_VM_POP_ARG(exe);
	FklVMvalue* c=FKL_VM_POP_ARG(exe);
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

static inline void B_close_ref(FKL_VM_INS_FUNC_ARGL)
{
	uint32_t sIdx=ins->imm;
	uint32_t eIdx=ins->imm_u32;
	close_var_ref_between(fklGetCompoundFrameLocRef(exe->frames)->lref,sIdx,eIdx);
}

static inline void B_ret(FKL_VM_INS_FUNC_ARGL)
{
	FklVMframe* f=exe->frames;
	if(f->c.mark)
	{
		close_all_var_ref(&f->c.lr);
		if(f->c.lr.lrefl)
		{
			f->c.lr.lrefl=NULL;
			memset(f->c.lr.lref,0,sizeof(FklVMvarRef*)*f->c.lr.lcount);
		}
		f->c.pc=f->c.spc;
		f->c.mark=0;
		f->c.tail=0;
	}
	else
		fklDoFinalizeCompoundFrame(popFrame(exe),exe);
}

void fklInitVMstack(FklVM* tmp)
{
	tmp->last=FKL_VM_STACK_INC_NUM;
	tmp->size=FKL_VM_STACK_INC_SIZE;
	tmp->tp=0;
	tmp->bp=0;
	tmp->base=(FklVMvalue**)malloc(FKL_VM_STACK_INC_SIZE);
	FKL_ASSERT(tmp->base);
}

void fklAllocMoreStack(FklVM* s)
{
	if(s->tp>=s->last)
	{
		s->last+=FKL_VM_STACK_INC_NUM;
		s->size+=FKL_VM_STACK_INC_SIZE;
		s->base=(FklVMvalue**)fklRealloc(s->base,s->size);
		FKL_ASSERT(s->base);
	}
}

void fklShrinkStack(FklVM* stack)
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
			if(fp!=stdout)fprintf(fp,"%"PRT64D":",i);
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
	exe->obj_head=NULL;
	exe->obj_tail=NULL;
	exe->importingLib=NULL;
	exe->gc=prev->gc;
	exe->loop=prev->loop;
	exe->prev=exe;
	exe->next=exe;
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
	exe->old_locv_list=NULL;
	exe->old_locv_count=0;
	exe->state=FKL_VM_READY;
	exe->notice_lock=0;
	memcpy(exe->ins_table,InsFuncTable,sizeof(InsFuncTable));
	uv_mutex_init(&exe->lock);
	fklCallObj(exe,nextCall);
	insert_to_VM_chain(exe,prev,next);
	return exe;
}

void fklUninitVMstack(FklVM* s)
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
		if(t->old_locv_count)
		{
			uint32_t i=t->old_locv_count;
			FklVMlocvList* cur=t->old_locv_list;
			for(;i>FKL_VM_GC_LOCV_CACHE_NUM;i--)
			{
				free(cur->locv);
				FklVMlocvList* prev=cur;
				cur=cur->next;
				free(prev);
			}
			for(uint32_t j=0;j<i;j++)
				free(t->old_locv_cache[j].locv);
		}
		if(t->obj_head)
			move_thread_objects_to_gc(t,t->gc);
		free(t->locv);
		free(t->libs);
		free(t);
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

FklVMvalue** fklPushVMvalue(FklVM* s,FklVMvalue* v)
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

void fklInitVMlibWithCodeObj(FklVMlib* lib
		,FklVMvalue* codeObj
		,FklVM* exe
		,uint32_t protoId)
{
	FklByteCode* bc=FKL_VM_CO(codeObj)->bc;
	FklVMvalue* proc=fklCreateVMvalueProc(exe,bc->code,bc->len,codeObj,protoId);
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

unsigned int fklGetCompoundFrameMark(const FklVMframe* f)
{
	return f->c.mark;
}

unsigned int fklSetCompoundFrameMark(FklVMframe* f,unsigned int m)
{
	f->c.mark=m;
	return m;
}

FklVMCompoundFrameVarRef* fklGetCompoundFrameLocRef(FklVMframe* f)
{
	return &f->c.lr;
}

FklVMvalue* fklGetCompoundFrameProc(const FklVMframe* f)
{
	return f->c.proc;
}

FklFuncPrototype* fklGetCompoundFrameProcPrototype(const FklVMframe* f,FklVM* exe)
{
	uint32_t pId=FKL_VM_PROC(f->c.proc)->protoId;
	return &exe->pts->pts[pId];
}

FklInstruction* fklGetCompoundFrameCode(const FklVMframe* f)
{
	return f->c.pc;
}

FklOpcode fklGetCompoundFrameOp(FklVMframe* f)
{
	return (f->c.pc++)->op;
}

void fklAddCompoundFrameCp(FklVMframe* f,int64_t a)
{
	f->c.pc+=a;
}

FklInstruction* fklGetCompoundFrameEnd(const FklVMframe* f)
{
	return f->c.end;
}

FklSid_t fklGetCompoundFrameSid(const FklVMframe* f)
{
	return f->c.sid;
}

void fklInitMainProcRefs(FklVMproc* mainProc,FklVMvarRef** closure,uint32_t count)
{
	mainProc->rcount=count;
	mainProc->closure=fklCopyMemory(closure,sizeof(FklVMvalue*)*mainProc->rcount);
	for(uint32_t i=0;i<count;i++)
		fklMakeVMvarRefRef(closure[i]);
}

void fklSetBp(FklVM* s)
{
	FKL_VM_PUSH_VALUE(s,FKL_MAKE_VM_FIX(s->bp));
	s->bp=s->tp;
}

