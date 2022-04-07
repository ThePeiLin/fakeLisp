#include<fakeLisp/reader.h>
#include<fakeLisp/symbol.h>
#include<fakeLisp/basicADT.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<setjmp.h>
#include<dlfcn.h>
#include<ctype.h>

extern void invokeNativeProcdure(FklVM*,FklVMproc*,FklVMrunnable*);
extern void invokeContinuation(FklVM*,VMcontinuation*);
extern void invokeDlProc(FklVM*,FklVMdlproc*);
extern void invokeFlproc(FklVM*,FklVMflproc*);
extern pthread_mutex_t GlobSharedObjsMutex;
extern FklSharedObjNode* GlobSharedObjs;
extern FklVMlist GlobVMs;
extern void* ThreadVMfunc(void* p);
extern void* ThreadVMdlprocFunc(void* p);
extern void* ThreadVMflprocFunc(void* p);

//syscalls

void SYS_car(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_cdr(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_cons(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_append(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_atom(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_null(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_not(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_eq(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_equal(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_eqn(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_add(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_add_1(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_sub(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_sub_1(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_mul(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_div(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_rem(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_gt(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_ge(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_lt(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_le(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_i8(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_int(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_f64(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_str(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_sym(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_byts(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_type(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_nth(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_length(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_file(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_read(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_getb(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_prin1(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_putb(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_princ(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_dll(FklVM*,pthread_rwlock_t*);
void SYS_dlsym(FklVM*,pthread_rwlock_t*);
void SYS_argv(FklVM*,pthread_rwlock_t*);
void SYS_go(FklVM*,pthread_rwlock_t*);
void SYS_chanl(FklVM*,pthread_rwlock_t*);
void SYS_send(FklVM*,pthread_rwlock_t*);
void SYS_recv(FklVM*,pthread_rwlock_t*);
void SYS_error(FklVM*,pthread_rwlock_t*);
void SYS_raise(FklVM*,pthread_rwlock_t*);
void SYS_call_cc(FklVM*,pthread_rwlock_t*);
void SYS_apply(FklVM*,pthread_rwlock_t*);
void SYS_newf(FklVM*,pthread_rwlock_t*);
void SYS_delf(FklVM*,pthread_rwlock_t*);
void SYS_lfdl(FklVM*,pthread_rwlock_t*);

//end



void SYS_car(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.car",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.car",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PAIR(obj))
		FKL_RAISE_BUILTIN_ERROR("sys.car",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN("SYS_car",FKL_MAKE_VM_REF(&obj->u.pair->car),stack);
}

void SYS_cdr(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.cdr",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.cdr",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PAIR(obj))
		FKL_RAISE_BUILTIN_ERROR("sys.cdr",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN("SYS_cdr",FKL_MAKE_VM_REF(&obj->u.pair->cdr),stack);
}

void SYS_cons(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* car=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* cdr=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.cons",FKL_TOOMANYARG,runnable,exe);
	if(!car||!cdr)
		FKL_RAISE_BUILTIN_ERROR("sys.cons",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* pair=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),exe->heap);
	pair->u.pair->car=car;
	pair->u.pair->cdr=cdr;
	FKL_SET_RETURN("SYS_cons",pair,stack);
}

void SYS_append(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue** prev=&retval;
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(*prev==FKL_VM_NIL)
			*prev=fklCopyVMvalue(cur,exe->heap);
		else if(FKL_IS_PAIR(*prev))
		{
			for(;FKL_IS_PAIR(*prev);prev=&(*prev)->u.pair->cdr);
			*prev=fklCopyVMvalue(cur,exe->heap);
		}
		else if(FKL_IS_STR(*prev))
		{
			if(!FKL_IS_PTR(cur))
				FKL_RAISE_BUILTIN_ERROR("sys.append",FKL_WRONGARG,runnable,exe);
			(*prev)->u.str=fklStrCat((*prev)->u.str,cur->u.str);
		}
		else if(FKL_IS_BYTS(*prev))
		{
			if(!FKL_IS_BYTS(cur))
				FKL_RAISE_BUILTIN_ERROR("sys.append",FKL_WRONGARG,runnable,exe);
			fklVMbytsCat(&(*prev)->u.byts,cur->u.byts);
		}
		else
			FKL_RAISE_BUILTIN_ERROR("sys.append",FKL_WRONGARG,runnable,exe);
	}
	fklResBp(stack);
	FKL_SET_RETURN("SYS_append",retval,stack);
}

void SYS_atom(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* arg=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.atom",FKL_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR("sys.atom",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PAIR(arg))
		FKL_SET_RETURN("SYS_atom",FKL_VM_TRUE,stack);
	else
		FKL_SET_RETURN("SYS_atom",FKL_VM_NIL,stack);
}

void SYS_null(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* arg=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.null",FKL_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR("sys.null",FKL_TOOFEWARG,runnable,exe);
	if(arg==FKL_VM_NIL)
		FKL_SET_RETURN("SYS_null",FKL_VM_TRUE,stack);
	else
		FKL_SET_RETURN("SYS_null",FKL_VM_NIL,stack);
}

void SYS_not(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* arg=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.not",FKL_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR("sys.not",FKL_TOOFEWARG,runnable,exe);
	if(arg==FKL_VM_NIL)
		FKL_SET_RETURN("SYS_not",FKL_VM_TRUE,stack);
	else
		FKL_SET_RETURN("SYS_not",FKL_VM_NIL,stack);
}

void SYS_eq(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* fir=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* sec=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.eq",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR("sys.eq",FKL_TOOFEWARG,runnable,exe);
	FKL_SET_RETURN("SYS_eq",fir==sec
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_eqn(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* fir=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* sec=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.eqn",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR("sys.eqn",FKL_TOOFEWARG,runnable,exe);
	if((FKL_IS_F64(fir)||FKL_IS_I32(fir)||FKL_IS_I64(fir))&&(FKL_IS_F64(sec)||FKL_IS_I32(sec)||FKL_IS_I64(sec)))
		FKL_SET_RETURN("SYS_eqn",((((FKL_IS_F64(fir))?fir->u.f64
							:((FKL_IS_I32(fir)?FKL_GET_I32(fir)
								:fir->u.i64)))
						-((FKL_IS_F64(sec))?sec->u.f64
							:((FKL_IS_I32(sec)?FKL_GET_I32(sec)
								:sec->u.i64))))
					==0.0)
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,stack);
	else if(FKL_IS_SYM(fir)&&FKL_IS_SYM(sec))
		FKL_SET_RETURN("SYS_eqn",fir==sec
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,stack);
	else if(FKL_IS_STR(fir)&&FKL_IS_STR(sec))
		FKL_SET_RETURN("SYS_eqn",(!strcmp(fir->u.str,sec->u.str))
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,stack);
	else if(FKL_IS_BYTS(fir)&&FKL_IS_BYTS(sec))
		FKL_SET_RETURN("SYS_eqn",fklEqVMbyts(fir->u.byts,sec->u.byts)
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,stack);
	else
		FKL_RAISE_BUILTIN_ERROR("sys.eqn",FKL_WRONGARG,runnable,exe);

}

void SYS_equal(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* fir=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* sec=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.equal",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR("sys.equal",FKL_TOOFEWARG,runnable,exe);
	FKL_SET_RETURN("SYS_equal",(fklVMvaluecmp(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_add(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	int64_t r64=0;
	double rd=0.0;
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(FKL_IS_I32(cur))
			r64+=FKL_GET_I32(cur);
		else if(FKL_IS_F64(cur))
			rd+=cur->u.f64;
		else if(FKL_IS_I64(cur))
			r64+=cur->u.i64;
		else
			FKL_RAISE_BUILTIN_ERROR("sys.add",FKL_WRONGARG,runnable,exe);
	}
	fklResBp(stack);
	if(rd!=0.0)
	{
		rd+=r64;
		FKL_SET_RETURN("SYS_add",fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
	}
	else
	{
		if(r64>INT32_MAX||r64<INT32_MIN)
			FKL_SET_RETURN("SYS_add",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
		else
			FKL_SET_RETURN("SYS_add",FKL_MAKE_VM_I32(r64),stack);
	}
}

void SYS_add_1(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* arg=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(arg)&&!FKL_IS_I32(arg)&&!FKL_IS_I64(arg))
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64+1.0;
		FKL_SET_RETURN("SYS_add_1",fklNewVMvalue(FKL_F64,&r,exe->heap),stack);
	}
	else if(FKL_IS_I64(arg))
	{
		int64_t r=arg->u.i64+1;
		FKL_SET_RETURN("SYS_add_1",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else
		FKL_SET_RETURN("SYS_add_1",FKL_MAKE_VM_I32(FKL_GET_I32(arg)+1),stack);
}

void SYS_sub(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* prev=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	double rd=0.0;
	int64_t r64=0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR("sys.sub",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(prev)&&!FKL_IS_I32(prev)&&!FKL_IS_I64(prev))
		FKL_RAISE_BUILTIN_ERROR("sys.sub",FKL_WRONGARG,runnable,exe);
	if(!cur)
	{
		fklResBp(stack);
		if(FKL_IS_F64(prev))
		{
			rd=-prev->u.f64;
			FKL_SET_RETURN("SYS_sub",fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else if(FKL_IS_I32(prev))
		{
			r64=-FKL_GET_I32(prev);
			FKL_SET_RETURN("SYS_sub",FKL_MAKE_VM_I32(r64),stack);
		}
		else
		{
			r64=-prev->u.i64;
			FKL_SET_RETURN("SYS_sub",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
		}
	}
	else
	{
		for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
		{
			if(FKL_IS_I32(cur))
				r64+=FKL_GET_I32(cur);
			else if(FKL_IS_F64(cur))
				rd+=cur->u.f64;
			else if(FKL_IS_I64(cur))
				r64+=cur->u.i64;
			else
				FKL_RAISE_BUILTIN_ERROR("sys.sub",FKL_WRONGARG,runnable,exe);
		}
		fklResBp(stack);
		if(FKL_IS_F64(prev)||rd!=0.0)
		{
			rd=((FKL_IS_F64(prev))?prev->u.f64:((FKL_IS_I64(prev))?prev->u.i64:FKL_GET_I32(prev)))-rd-r64;
			FKL_SET_RETURN("SYS_sub",fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else
		{
			r64=(FKL_IS_I64(prev))?prev->u.i64:FKL_GET_I32(prev)-r64;
			if(r64>INT32_MAX||r64<INT32_MIN)
				FKL_SET_RETURN("SYS_sub",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
			else
				FKL_SET_RETURN("SYS_sub",FKL_MAKE_VM_I32(r64),stack);
		}
	}
}

void SYS_sub_1(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* arg=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.sub_1",FKL_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(arg)&&!FKL_IS_I32(arg)&&!FKL_IS_I64(arg))
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64-1.0;
		FKL_SET_RETURN("SYS_sub_1",fklNewVMvalue(FKL_F64,&r,exe->heap),stack);
	}
	else if(FKL_IS_I64(arg))
	{
		int64_t r=arg->u.i64-1;
		FKL_SET_RETURN("SYS_sub_1",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else
		FKL_SET_RETURN("SYS_sub_1",FKL_MAKE_VM_I32(FKL_GET_I32(arg)-1),stack);
}

void SYS_mul(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	double rd=1.0;
	int64_t r64=1;
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(FKL_IS_I32(cur))
			r64*=FKL_GET_I32(cur);
		else if(FKL_IS_F64(cur))
			rd*=cur->u.f64;
		else if(FKL_IS_I64(cur))
			r64*=cur->u.i64;
		else
			FKL_RAISE_BUILTIN_ERROR("sys.mul",FKL_WRONGARG,runnable,exe);
	}
	fklResBp(stack);
	if(rd!=1.0)
	{
		rd*=r64;
		FKL_SET_RETURN("SYS_mul",fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
	}
	else
	{
		if(r64>INT32_MAX||r64<INT32_MIN)
			FKL_SET_RETURN("SYS_mul",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
		else
			FKL_SET_RETURN("SYS_mul",FKL_MAKE_VM_I32(r64),stack);
	}
}

void SYS_div(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* prev=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	int64_t r64=1;
	double rd=1.0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(prev)&&!FKL_IS_I32(prev)&&!FKL_IS_I64(prev))
		FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_WRONGARG,runnable,exe);
	if(!cur)
	{
		fklResBp(stack);
		if(FKL_IS_F64(prev))
		{
			if(prev->u.f64==0.0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			rd=1/prev->u.f64;
			FKL_SET_RETURN("SYS_sub",fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else if(FKL_IS_I64(prev))
		{
			r64=prev->u.i64;
			if(!r64)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			if(1%r64)
			{
				rd=1.0/r64;
				FKL_SET_RETURN("SYS_div",fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
			}
			else
			{
				r64=1/r64;
				FKL_SET_RETURN("SYS_div",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
			}
		}
		else
		{
			if(!FKL_GET_I32(prev))
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			if(1%FKL_GET_I32(prev))
			{
				rd=1.0/FKL_GET_I32(prev);
				FKL_SET_RETURN("SYS_div",fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
			}
			else
				FKL_SET_RETURN("SYS_div",FKL_MAKE_VM_I32(1),stack);
		}
	}
	else
	{
		for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
		{
			if(FKL_IS_I32(cur))
				r64*=FKL_GET_I32(cur);
			else if(FKL_IS_F64(cur))
				rd*=cur->u.f64;
			else if(FKL_IS_I64(cur))
				r64*=cur->u.i64;
			else
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_WRONGARG,runnable,exe);
		}
		if((r64)==0)
			FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
		fklResBp(stack);
		if(FKL_IS_F64(prev)||rd!=1.0||(FKL_IS_I32(prev)&&FKL_GET_I32(prev)%(r64))||(FKL_IS_I64(prev)&&prev->u.i64%(r64)))
		{
			if(rd==0.0||r64==0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			rd=((double)((FKL_IS_F64(prev))?prev->u.f64:(FKL_IS_I32(prev)?FKL_GET_I32(prev):prev->u.i64)))/rd/r64;
			FKL_SET_RETURN("SYS_div",fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else
		{
			if(r64==0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			//if(r64!=1)
			//{
			//	r64=(FKL_IS_I32(prev)?FKL_GET_I32(prev):*prev->u.i64)/r64;
			//	FKL_SET_RETURN("SYS_div",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
			//}
			else
			{
				r64=FKL_GET_I32(prev)/r64;
				if(r64>INT32_MAX||r64<INT32_MIN)
					FKL_SET_RETURN("SYS_div",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
				else
					FKL_SET_RETURN("SYS_div",FKL_MAKE_VM_I32(r64),stack);
			}
		}
	}
}

void SYS_rem(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* fir=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* sec=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.rem",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR("sys.rem",FKL_TOOFEWARG,runnable,exe);
	if(!((FKL_IS_F64(fir)||FKL_IS_I32(fir)||FKL_IS_I64(fir))&&(FKL_IS_F64(sec)||FKL_IS_I32(sec)||FKL_IS_I64(sec))))
		FKL_RAISE_BUILTIN_ERROR("sys.rem",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(fir)||FKL_IS_F64(sec))
	{
		double af=(FKL_IS_F64(fir))?fir->u.f64:(FKL_IS_I32(fir)?FKL_GET_I32(fir):fir->u.i64);
		double as=(FKL_IS_F64(sec))?sec->u.f64:(FKL_IS_I32(sec)?FKL_GET_I32(sec):sec->u.i64);
		if(as==0.0)
			FKL_RAISE_BUILTIN_ERROR("sys.rem",FKL_DIVZERROERROR,runnable,exe);
		double r=fmod(af,as);
		FKL_SET_RETURN("SYS_rem",fklNewVMvalue(FKL_F64,&r,exe->heap),stack);
	}
	else if(FKL_IS_I32(fir)&&FKL_IS_I32(sec))
	{
		if(!FKL_GET_I32(sec))
			FKL_RAISE_BUILTIN_ERROR("sys.rem",FKL_DIVZERROERROR,runnable,exe);
		int32_t r=FKL_GET_I32(fir)%FKL_GET_I32(sec);
		FKL_SET_RETURN("SYS_rem",FKL_MAKE_VM_I32(r),stack);
	}
	else
	{
		int64_t r=(FKL_IS_I32(fir)?FKL_GET_I32(fir):fir->u.i64)%(FKL_IS_I32(sec)?FKL_GET_I32(sec):sec->u.i64);
		FKL_SET_RETURN("SYS_rem",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
}

void SYS_gt(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int r=1;
	FklVMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.gt",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)||FKL_IS_I32(prev)||FKL_IS_I64(prev))
					&&(FKL_IS_F64(cur)||FKL_IS_I32(cur)||FKL_IS_I64(cur)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:((FKL_IS_I32(prev))?FKL_GET_I32(prev)
									:prev->u.i64))
							-((FKL_IS_F64(cur))?cur->u.f64
								:((FKL_IS_I32(cur))?FKL_GET_I32(cur)
									:cur->u.i64)))
						>0.0);
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)>0);
			else
				FKL_RAISE_BUILTIN_ERROR("sys.gt",FKL_WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap));
	fklResBp(stack);
	FKL_SET_RETURN("SYS_gt",r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_ge(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int r=1;
	FklVMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.ge",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)||FKL_IS_I32(prev)||FKL_IS_I64(prev))
					&&(FKL_IS_F64(cur)||FKL_IS_I32(cur)||FKL_IS_I64(cur)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:((FKL_IS_I32(prev))?FKL_GET_I32(prev)
									:prev->u.i64))
							-((FKL_IS_F64(cur))?cur->u.f64
								:((FKL_IS_I32(cur))?FKL_GET_I32(cur)
									:cur->u.i64)))
						>=0.0);
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)>=0);
			else
				FKL_RAISE_BUILTIN_ERROR("sys.ge",FKL_WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap));
	fklResBp(stack);
	FKL_SET_RETURN("SYS_ge",r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_lt(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int r=1;
	FklVMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.lt",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)||FKL_IS_I32(prev)||FKL_IS_I64(prev))
					&&(FKL_IS_F64(cur)||FKL_IS_I32(cur)||FKL_IS_I64(cur)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:((FKL_IS_I32(prev))?FKL_GET_I32(prev)
									:prev->u.i64))
							-((FKL_IS_F64(cur))?cur->u.f64
								:((FKL_IS_I32(cur))?FKL_GET_I32(cur)
									:cur->u.i64)))
						<0.0);
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)<0);
			else
				FKL_RAISE_BUILTIN_ERROR("sys.lt",FKL_WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap));
	fklResBp(stack);
	FKL_SET_RETURN("SYS_lt",r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_le(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int r=1;
	FklVMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.le",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)||FKL_IS_I32(prev)||FKL_IS_I64(prev))
					&&(FKL_IS_F64(cur)||FKL_IS_I32(cur)||FKL_IS_I64(cur)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:((FKL_IS_I32(prev))?FKL_GET_I32(prev)
									:prev->u.i64))
							-((FKL_IS_F64(cur))?cur->u.f64
								:((FKL_IS_I32(cur))?FKL_GET_I32(cur)
									:cur->u.i64)))
						<=0.0);
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)<=0);
			else
				FKL_RAISE_BUILTIN_ERROR("sys.le",FKL_WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap));
	fklResBp(stack);
	FKL_SET_RETURN("SYS_le",r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_i8(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.i8",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.i8",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_I32(obj))
		FKL_SET_RETURN("SYS_i8",FKL_MAKE_VM_I8(FKL_GET_I32(obj)),stack);
	else if(FKL_IS_I8(obj))
		FKL_SET_RETURN("SYS_i8",obj,stack);
	else if(FKL_IS_F64(obj))
		FKL_SET_RETURN("SYS_i8",FKL_MAKE_VM_I8((int32_t)obj->u.f64),stack);
	else if(FKL_IS_STR(obj))
		FKL_SET_RETURN("SYS_i8",FKL_MAKE_VM_I8(obj->u.str[0]),stack);
	else if(FKL_IS_BYTS(obj))
		FKL_SET_RETURN("SYS_i8",FKL_MAKE_VM_I8(obj->u.byts->str[0]),stack);
	else
		FKL_RAISE_BUILTIN_ERROR("sys.i8",FKL_WRONGARG,runnable,exe);
}

void SYS_int(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.int",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.int",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_I8(obj))
		FKL_SET_RETURN("SYS_int",FKL_MAKE_VM_I32(FKL_GET_I8(obj)),stack);
	else if(FKL_IS_I32(obj))
		FKL_SET_RETURN("SYS_int",obj,stack);
	else if(FKL_IS_SYM(obj))
		FKL_SET_RETURN("SYS_int",FKL_MAKE_VM_I32(FKL_GET_SYM(obj)),stack);
	else if(FKL_IS_I64(obj))
		FKL_SET_RETURN("SYS_int",fklNewVMvalue(FKL_I64,&obj->u.i64,exe->heap),stack);
	else if(FKL_IS_F64(obj))
	{
		int64_t r=(int64_t)obj->u.f64;
		if(r>INT32_MAX||r<INT32_MIN)
			FKL_SET_RETURN("SYS_int",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
		else
			FKL_SET_RETURN("SYS_int",FKL_MAKE_VM_I32((int32_t)r),stack);
	}
	else if(FKL_IS_STR(obj))
	{
		int64_t r=fklStringToInt(obj->u.str);
		if(r>INT32_MAX||r<INT32_MIN)
			FKL_SET_RETURN("SYS_int",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
		else
			FKL_SET_RETURN("SYS_int",FKL_MAKE_VM_I32(r),stack);
	}
	else if(FKL_IS_BYTS(obj))
	{
		size_t s=obj->u.byts->size;
		if(s<5)
		{
			uint8_t r[4]={0};
			switch(s)
			{
				case 4:r[3]=obj->u.byts->str[3];
				case 3:r[2]=obj->u.byts->str[2];
				case 2:r[1]=obj->u.byts->str[1];
				case 1:r[0]=obj->u.byts->str[0];
			}
			FKL_SET_RETURN("SYS_int",FKL_MAKE_VM_I32(*(int32_t*)r),stack);
		}
		else
		{
			uint8_t r[8]={0};
			switch(s>8?8:s)
			{
				case 8:r[7]=obj->u.byts->str[8];
				case 7:r[6]=obj->u.byts->str[6];
				case 6:r[5]=obj->u.byts->str[5];
				case 5:r[4]=obj->u.byts->str[4];
				case 4:r[3]=obj->u.byts->str[3];
				case 3:r[2]=obj->u.byts->str[2];
				case 2:r[1]=obj->u.byts->str[1];
				case 1:r[0]=obj->u.byts->str[0];
			}
			FKL_SET_RETURN("SYS_int",fklNewVMvalue(FKL_I64,r,exe->heap),stack);
		}
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.int",FKL_WRONGARG,runnable,exe);
}

void SYS_i32(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.i32",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.i32",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_I8(obj))
		FKL_SET_RETURN("SYS_i32",FKL_MAKE_VM_I32(FKL_GET_I8(obj)),stack);
	else if(FKL_IS_I32(obj))
		FKL_SET_RETURN("SYS_i32",obj,stack);
	else if(FKL_IS_SYM(obj))
		FKL_SET_RETURN("SYS_i32",FKL_MAKE_VM_I32(FKL_GET_SYM(obj)),stack);
	else if(FKL_IS_I64(obj))
		FKL_SET_RETURN("SYS_i32",FKL_MAKE_VM_I32((int32_t)obj->u.i64),stack);
	else if(FKL_IS_F64(obj))
		FKL_SET_RETURN("SYS_i32",FKL_MAKE_VM_I32((int32_t)obj->u.f64),stack);
	else if(FKL_IS_STR(obj))
	{
		int64_t r=fklStringToInt(obj->u.str);
		FKL_SET_RETURN("SYS_i32",FKL_MAKE_VM_I32((int32_t)r),stack);
	}
	else if(FKL_IS_BYTS(obj))
	{
		size_t s=obj->u.byts->size;
		uint8_t r[4]={0};
		switch(s)
		{
			case 4:r[3]=obj->u.byts->str[3];
			case 3:r[2]=obj->u.byts->str[2];
			case 2:r[1]=obj->u.byts->str[1];
			case 1:r[0]=obj->u.byts->str[0];
		}
		FKL_SET_RETURN("SYS_i32",FKL_MAKE_VM_I32(*(int32_t*)r),stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.i32",FKL_WRONGARG,runnable,exe);
}

void SYS_i64(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.i64",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.i64",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_I8(obj))
	{
		int8_t r=FKL_GET_I8(obj);
		FKL_SET_RETURN("SYS_i64",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else if(FKL_IS_I32(obj))
	{
		int32_t r=FKL_GET_I32(obj);
		FKL_SET_RETURN("SYS_i64",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else if(FKL_IS_SYM(obj))
	{
		int64_t r=FKL_GET_SYM(obj);
		FKL_SET_RETURN("SYS_i64",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else if(FKL_IS_I64(obj))
		FKL_SET_RETURN("SYS_i64",fklNewVMvalue(FKL_I64,&obj->u.i64,exe->heap),stack);
	else if(FKL_IS_F64(obj))
	{
		int64_t r=(int64_t)obj->u.f64;
		FKL_SET_RETURN("SYS_i64",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else if(FKL_IS_STR(obj))
	{
		int64_t r=fklStringToInt(obj->u.str);
		FKL_SET_RETURN("SYS_i64",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else if(FKL_IS_BYTS(obj))
	{
		size_t s=obj->u.byts->size;
		uint8_t r[8]={0};
		switch(s>8?8:s)
		{
			case 8:r[7]=obj->u.byts->str[8];
			case 7:r[6]=obj->u.byts->str[6];
			case 6:r[5]=obj->u.byts->str[5];
			case 5:r[4]=obj->u.byts->str[4];
			case 4:r[3]=obj->u.byts->str[3];
			case 3:r[2]=obj->u.byts->str[2];
			case 2:r[1]=obj->u.byts->str[1];
			case 1:r[0]=obj->u.byts->str[0];
		}
		FKL_SET_RETURN("SYS_i64",fklNewVMvalue(FKL_I64,r,exe->heap),stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.i64",FKL_WRONGARG,runnable,exe);
}

void SYS_f64(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.f64",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.f64",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNewVMvalue(FKL_F64,NULL,exe->heap);
	if(FKL_IS_I8(obj))
		retval->u.f64=(double)FKL_GET_I8(obj);
	else if(FKL_IS_I32(obj))
		retval->u.f64=(double)FKL_GET_I32(obj);
	else if(FKL_IS_F64(obj))
		retval->u.f64=obj->u.f64;
	else if(FKL_IS_STR(obj))
		retval->u.f64=fklStringToDouble(obj->u.str);
	else if(FKL_IS_BYTS(obj))
	{
		uint8_t r[8]={0};
		size_t s=obj->u.byts->size;
		switch(s>8?8:s)
		{
			case 8:r[7]=obj->u.byts->str[7];
			case 7:r[6]=obj->u.byts->str[6];
			case 6:r[5]=obj->u.byts->str[5];
			case 5:r[4]=obj->u.byts->str[4];
			case 4:r[3]=obj->u.byts->str[3];
			case 3:r[2]=obj->u.byts->str[2];
			case 2:r[1]=obj->u.byts->str[1];
			case 1:r[0]=obj->u.byts->str[0];
		}
		memcpy(&retval->u.f64,r,8);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.f64",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN("SYS_f64",retval,stack);
}

void SYS_str(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.str",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.str",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNewVMvalue(FKL_STR,NULL,exe->heap);
	if(FKL_IS_I32(obj))
		retval->u.str=fklIntToString(FKL_GET_I32(obj));
	else if(FKL_IS_F64(obj))
		retval->u.str=fklDoubleToString(obj->u.f64);
	else if(FKL_IS_I8(obj))
		retval->u.str=fklCopyStr((char[]){FKL_GET_I8(obj),'\0'});
	else if(FKL_IS_SYM(obj))
		retval->u.str=fklCopyStr(fklGetGlobSymbolWithId(FKL_GET_SYM(obj))->symbol);
	else if(FKL_IS_STR(obj))
		retval->u.str=fklCopyStr(obj->u.str);
	else if(FKL_IS_BYTS(obj))
	{
		size_t s=obj->u.byts->size;
		size_t l=(obj->u.byts->str[s-1])?s+1:s;
		char* t=(char*)malloc(sizeof(char)*l);
		FKL_ASSERT(t,"SYS_str",__FILE__,__LINE__);
		memcpy(t,obj->u.byts->str,s);
		t[l-1]='\0';
		retval->u.str=t;
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.str",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN("SYS_str",retval,stack);
}

void SYS_sym(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.sym",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.sym",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_I8(obj))
		FKL_SET_RETURN("SYS_sym",FKL_MAKE_VM_SYM(fklAddSymbolToGlob(((char[]){FKL_GET_I8(obj),'\0'}))->id),stack);
	else if(FKL_IS_SYM(obj))
		FKL_SET_RETURN("SYS_sym",obj,stack);
	else if(FKL_IS_STR(obj))
		FKL_SET_RETURN("SYS_sym",FKL_MAKE_VM_SYM(fklAddSymbolToGlob(obj->u.str)->id),stack);
	else
		FKL_RAISE_BUILTIN_ERROR("sys.sym",FKL_WRONGARG,runnable,exe);
}

void SYS_byts(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNewVMvalue(FKL_BYTS,fklNewEmptyVMbyts(),exe->heap);
	if(FKL_IS_BYTS(obj))
	{
		FklVMvalue* pstart=fklGET_VAL(fklPopVMstack(stack),exe->heap);
		FklVMvalue* psize=fklGET_VAL(fklPopVMstack(stack),exe->heap);
		if(fklResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_TOOMANYARG,runnable,exe);
		if(!FKL_IS_I32(pstart)&&!FKL_IS_I64(pstart)&&!FKL_IS_I32(psize)&&!FKL_IS_I64(psize))
			FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_WRONGARG,runnable,exe);
		if(pstart)
		{
			int64_t start=FKL_IS_I32(pstart)?FKL_GET_I32(pstart):pstart->u.i64;
			int64_t size=obj->u.byts->size-start;
			if(psize)
			{
				int64_t tsize=FKL_IS_I32(psize)?FKL_GET_I32(psize):psize->u.i64;
				if(tsize>size)
					FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_INVALIDACCESS,runnable,exe);
				size=tsize;
			}
			FklVMbyts* retvalbyts=fklNewVMbyts(size,obj->u.byts->str+start);
			free(retval->u.byts);
			retval->u.byts=retvalbyts;
		}
		else
			fklVMbytsCat(&retval->u.byts,obj->u.byts);
	}
	else
	{
		if(fklResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_TOOMANYARG,runnable,exe);
		if(FKL_IS_I32(obj))
		{
			retval->u.byts=(FklVMbyts*)realloc(retval->u.byts,sizeof(FklVMbyts)+sizeof(int32_t));
			FKL_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
			retval->u.byts->size=sizeof(int32_t);
			*(int32_t*)retval->u.byts->str=FKL_GET_I32(obj);
		}
		else if(FKL_IS_F64(obj))
		{
			retval->u.byts=(FklVMbyts*)realloc(retval->u.byts,sizeof(FklVMbyts)+sizeof(double));
			FKL_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
			retval->u.byts->size=sizeof(double);
			*(double*)retval->u.byts->str=obj->u.f64;
		}
		else if(FKL_IS_I8(obj))
		{
			retval->u.byts=(FklVMbyts*)realloc(retval->u.byts,sizeof(FklVMbyts)+sizeof(char));
			FKL_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
			retval->u.byts->size=sizeof(char);
			*(char*)retval->u.byts->str=FKL_GET_I8(obj);
		}
		else if(FKL_IS_SYM(obj))
		{
			retval->u.byts=(FklVMbyts*)realloc(retval->u.byts,sizeof(FklVMbyts)+sizeof(FklSid_t));
			retval->u.byts->size=sizeof(FklSid_t);
			FklSid_t sid=FKL_GET_SYM(obj);
			memcpy(retval->u.byts->str,&sid,sizeof(FklSid_t));
			//FklSymTabNode* n=fklGetGlobSymbolWithId(FKL_GET_SYM(obj));
			//retval->u.byts=(FklVMbyts*)realloc(retval->u.byts,sizeof(FklVMbyts)+strlen(n->symbol));
			//FKL_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
			//retval->u.byts->size=strlen(n->symbol);
			//memcpy(retval->u.byts->str,n->symbol,retval->u.byts->size);
		}
		else if(FKL_IS_STR(obj))
		{
			retval->u.byts=(FklVMbyts*)realloc(retval->u.byts,sizeof(FklVMbyts)+strlen(obj->u.str)+1);
			FKL_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
			retval->u.byts->size=strlen(obj->u.str);
			memcpy(retval->u.byts->str,obj->u.str,retval->u.byts->size);
		}
		else
			FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_WRONGARG,runnable,exe);
	}
	FKL_SET_RETURN("SYS_byts",retval,stack);
}

void SYS_type(FklVM* exe,pthread_rwlock_t* gclock)
{
	static FklSid_t a[19];
	static int hasInit=0;
	if(!hasInit)
	{
		hasInit=1;
		const char* b[]=
		{
			"nil",
			"i32",
			"i8",
			"f64",
			"i64",
			"sym",
			"str",
			"byts",
			"proc",
			"cont",
			"chan",
			"fp",
			"dll",
			"dlproc",
			"flproc",
			"err",
			"mem",
			"ref",
			"pair"
		};
		int i=0;
		for(;i<FKL_ATM;i++)
			a[i]=fklAddSymbolToGlob(b[i])->id;
	}
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.type",FKL_TOOMANYARG,runnable,exe);
	size_t type=0;
	switch(FKL_GET_TAG(obj))
	{
		case FKL_NIL_TAG:type=FKL_NIL;break;
		case FKL_I32_TAG:type=FKL_I32;break;
		case FKL_SYM_TAG:type=FKL_SYM;break;
		case FKL_I8_TAG:type=FKL_I8;break;
		case FKL_PTR_TAG:type=obj->type;break;
		default:
			FKL_RAISE_BUILTIN_ERROR("sys.type",FKL_WRONGARG,runnable,exe);
			break;
	}
	FKL_SET_RETURN("SYS_type",FKL_MAKE_VM_SYM(a[type]),stack);
}

void SYS_nth(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* place=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* objlist=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_TOOMANYARG,runnable,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_I32(place)&&!FKL_IS_I64(place))
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_WRONGARG,runnable,exe);
	FklVMvalue* retval=NULL;
	ssize_t index=FKL_IS_I32(place)?FKL_GET_I32(place):place->u.i64;
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		retval=(FKL_IS_PAIR(objPair))?FKL_MAKE_VM_REF(&objPair->u.pair->car):FKL_VM_NIL;
	}
	else if(FKL_IS_STR(objlist))
		retval=index>=strlen(objlist->u.str)?FKL_VM_NIL:FKL_MAKE_VM_CHF(fklNewVMmem(fklGetCharTypeId(),(uint8_t*)objlist->u.str+index),heap);
	else if(FKL_IS_BYTS(objlist))
		retval=index>=objlist->u.byts->size?FKL_VM_NIL:FKL_MAKE_VM_CHF(fklNewVMmem(fklGetCharTypeId(),objlist->u.byts->str+index),heap);
	else
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN("SYS_nth",retval,stack);
}

void SYS_length(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.length",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.length",FKL_TOOFEWARG,runnable,exe);
	int32_t len=0;
	if(obj==FKL_VM_NIL||FKL_IS_PAIR(obj))
		for(;FKL_IS_PAIR(obj);obj=fklGetVMpairCdr(obj))len++;
	else if(FKL_IS_STR(obj))
		len=strlen(obj->u.str);
	else if(FKL_IS_BYTS(obj))
		len=obj->u.byts->size;
	else if(FKL_IS_CHAN(obj))
		len=fklGetNumVMchanl(obj->u.chan);
	else
		FKL_RAISE_BUILTIN_ERROR("sys.length",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN("SYS_length",FKL_MAKE_VM_I32(len),stack);
}

void SYS_fopen(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* filename=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	FklVMvalue* mode=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.fopen",FKL_TOOMANYARG,runnable,exe);
	if(!mode||!filename)
		FKL_RAISE_BUILTIN_ERROR("sys.fopen",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_STR(filename)||!FKL_IS_STR(mode))
		FKL_RAISE_BUILTIN_ERROR("sys.fopen",FKL_WRONGARG,runnable,exe);
	FILE* file=fopen(filename->u.str,mode->u.str);
	FklVMvalue* obj=NULL;
	if(!file)
	{
		FKL_SET_RETURN("SYS_fopen",filename,stack);
		FKL_RAISE_BUILTIN_ERROR("sys.fopen",FKL_FILEFAILURE,runnable,exe);
	}
	else
		obj=fklNewVMvalue(FKL_FP,file,heap);
	FKL_SET_RETURN("SYS_fopen",obj,stack);
}

void SYS_fclose(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fp=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.fclose",FKL_TOOFEWARG,runnable,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR("sys.fclose",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(fp))
		FKL_RAISE_BUILTIN_ERROR("sys.fclose",FKL_WRONGARG,runnable,exe);
	if(fp->u.fp==NULL||fp->u.fp==stderr||fp->u.fp==stdin||fp->u.fp==stdout||fclose(fp->u.fp)==EOF)
		FKL_RAISE_BUILTIN_ERROR("sys.fclose",FKL_INVALIDACCESS,runnable,exe);
	fp->u.fp=NULL;
	FKL_SET_RETURN("SYS_fclose",FKL_VM_NIL,stack);
}

static char* readUntilBlank(FILE* fp,int* eof)
{
	int32_t memSize=FKL_MAX_STRING_SIZE;
	char* tmp=(char*)malloc(sizeof(char)*memSize);
	FKL_ASSERT(tmp,"readString",__FILE__,__LINE__);
	int32_t strSize=0;
	int ch=getc(fp);
	if(ch==EOF)
		*eof=1;
	for(;;)
	{
		if(ch==EOF)
			break;
		if(ch==';')
		{
			while(ch!='\n')
				ch=getc(fp);
			continue;
		}
		strSize++;
		if(strSize>memSize-1)
		{
			tmp=(char*)realloc(tmp,sizeof(char)*(memSize+FKL_MAX_STRING_SIZE));
			FKL_ASSERT(tmp,"readString",__FILE__,__LINE__);
			memSize+=FKL_MAX_STRING_SIZE;
		}
		tmp[strSize-1]=ch;
		if(isspace(ch))
			break;
		ch=getc(fp);
	}
	tmp[strSize]='\0';
	memSize=strlen(tmp)+1;
	tmp=(char*)realloc(tmp,sizeof(char)*memSize);
	FKL_ASSERT(tmp,"readString",__FILE__,__LINE__);
	return tmp;
}

void SYS_read(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* stream=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.read",FKL_TOOMANYARG,runnable,exe);
	if(stream&&!FKL_IS_FP(stream)&&!FKL_IS_STR(stream))
		FKL_RAISE_BUILTIN_ERROR("sys.read",FKL_WRONGARG,runnable,exe);
	char* tmpString=NULL;
	FILE* tmpFile=NULL;
	FklPtrStack* tokenStack=fklNewPtrStack(32,16);
	if(!stream||FKL_IS_FP(stream))
	{
		tmpFile=stream?stream->u.fp:stdin;
		int unexpectEOF=0;
		char* prev=NULL;
		tmpString=fklReadInStringPattern(tmpFile,&prev,0,&unexpectEOF,tokenStack,readUntilBlank);
		if(prev)
			free(prev);
		if(unexpectEOF)
		{
			free(tmpString);
			FKL_RAISE_BUILTIN_ERROR("sys.read",FKL_UNEXPECTEOF,runnable,exe);
		}
	}
	else
	{
		tmpString=fklCopyStr(stream->u.str);
		char* parts[]={tmpString};
		FklPtrStack* matchStateStack=fklNewPtrStack(32,16);
		fklSplitStringPartsIntoToken(parts,1,0,tokenStack,matchStateStack,NULL,NULL);
		while(!fklIsPtrStackEmpty(matchStateStack))
			free(fklPopPtrStack(matchStateStack));
		fklFreePtrStack(matchStateStack);
	}
	FklInterpreter* tmpIntpr=fklNewTmpIntpr(NULL,tmpFile);
	FklAstCptr* tmpCptr=fklCreateAstWithTokens(tokenStack,tmpIntpr);
	//FklAstCptr* tmpCptr=fklBaseCreateTree(tmpString,tmpIntpr);
	FklVMvalue* tmp=NULL;
	if(tmpCptr==NULL)
		tmp=FKL_VM_NIL;
	else
		tmp=fklCastCptrVMvalue(tmpCptr,exe->heap);
	while(!fklIsPtrStackEmpty(tokenStack))
		fklFreeToken(fklPopPtrStack(tokenStack));
	fklFreePtrStack(tokenStack);
	FKL_SET_RETURN("SYS_read",tmp,stack);
	free(tmpIntpr);
	free(tmpString);
	fklDeleteCptr(tmpCptr);
	free(tmpCptr);
}

void SYS_getb(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* size=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* file=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.getb",FKL_TOOMANYARG,runnable,exe);
	if(!file||!size)
		FKL_RAISE_BUILTIN_ERROR("sys.getb",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(file)||!FKL_IS_I32(size))
		FKL_RAISE_BUILTIN_ERROR("sys.getb",FKL_WRONGARG,runnable,exe);
	FILE* fp=file->u.fp;
	uint8_t* str=(uint8_t*)malloc(sizeof(uint8_t)*FKL_GET_I32(size));
	FKL_ASSERT(str,"B_getb",__FILE__,__LINE__);
	int32_t realRead=0;
	realRead=fread(str,sizeof(uint8_t),FKL_GET_I32(size),fp);
	if(!realRead)
	{
		free(str);
		FKL_SET_RETURN("SYS_getb",FKL_VM_NIL,stack);
	}
	else
	{
		str=(uint8_t*)realloc(str,sizeof(uint8_t)*realRead);
		FKL_ASSERT(str,"B_getb",__FILE__,__LINE__);
		FklVMvalue* tmpByts=fklNewVMvalue(FKL_BYTS,NULL,exe->heap);
		tmpByts->u.byts=(FklVMbyts*)malloc(sizeof(FklVMbyts)+FKL_GET_I32(size));
		FKL_ASSERT(tmpByts->u.byts,"SYS_getb",__FILE__,__LINE__);
		tmpByts->u.byts->size=FKL_GET_I32(size);
		memcpy(tmpByts->u.byts->str,str,FKL_GET_I32(size));
		free(str);
		FKL_SET_RETURN("SYS_getb",tmpByts,stack);
	}
}

void SYS_prin1(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* file=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.prin1",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.prin1",FKL_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR("sys.prin1",FKL_WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp:stdout;
	fklPrin1VMvalue(obj,objFile);
	FKL_SET_RETURN("SYS_prin1",obj,stack);
}

void SYS_putb(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* bt=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* file=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.putb",FKL_TOOMANYARG,runnable,exe);
	if(!file||!bt)
		FKL_RAISE_BUILTIN_ERROR("sys.putb",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(file)||!FKL_IS_BYTS(bt))
		FKL_RAISE_BUILTIN_ERROR("sys.putb",FKL_WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp;
	fwrite(bt->u.byts->str,sizeof(uint8_t),bt->u.byts->size,objFile);
	FKL_SET_RETURN("SYS_putb",bt,stack);
}

void SYS_princ(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* file=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.princ",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.princ",FKL_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR("sys.princ",FKL_WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp:stdout;
	fklPrincVMvalue(obj,objFile);
	FKL_SET_RETURN("SYS_princ",obj,stack);
}

void SYS_dll(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* dllName=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.dll",FKL_TOOMANYARG,runnable,exe);
	if(!dllName)
		FKL_RAISE_BUILTIN_ERROR("sys.dll",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_STR(dllName))
		FKL_RAISE_BUILTIN_ERROR("sys.dll",FKL_WRONGARG,runnable,exe);
	FklVMdllHandle* dll=fklNewVMdll(dllName->u.str);
	if(!dll)
	{
		FKL_SET_RETURN("SYS_dll",dllName,stack);
		FKL_RAISE_BUILTIN_ERROR("sys.dll",FKL_LOADDLLFAILD,runnable,exe);
	}
	FKL_SET_RETURN("SYS_dll",fklNewVMvalue(FKL_DLL,dll,exe->heap),stack);
}

void SYS_dlsym(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* dll=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* symbol=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.dlsym",FKL_TOOMANYARG,runnable,exe);
	if(!dll||!symbol)
		FKL_RAISE_BUILTIN_ERROR("sys.dlsym",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_STR(symbol)||!FKL_IS_DLL(dll))
		FKL_RAISE_BUILTIN_ERROR("sys.dlsym",FKL_WRONGARG,runnable,exe);
	char prefix[]="FKL_";
	size_t len=strlen(prefix)+strlen(symbol->u.str)+1;
	char* realDlFuncName=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(realDlFuncName,"B_dlsym",__FILE__,__LINE__);
	sprintf(realDlFuncName,"%s%s",prefix,symbol->u.str);
	FklVMdllFunc funcAddress=fklGetAddress(realDlFuncName,dll->u.dll);
	if(!funcAddress)
	{
		free(realDlFuncName);
		FKL_RAISE_BUILTIN_ERROR("sys.dlsym",FKL_INVALIDSYMBOL,runnable,exe);
	}
	free(realDlFuncName);
	FklVMdlproc* dlproc=fklNewVMdlproc(funcAddress,dll);
	FKL_SET_RETURN("SYS_dlsym",fklNewVMvalue(FKL_DLPROC,dlproc,heap),stack);
}

void SYS_argv(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* retval=NULL;
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.argv",FKL_TOOMANYARG,fklTopPtrStack(exe->rstack),exe);
	retval=FKL_VM_NIL;
	FklVMvalue** tmp=&retval;
	int32_t i=0;
	for(;i<exe->argc;i++,tmp=&(*tmp)->u.pair->cdr)
	{
		FklVMvalue* cur=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),exe->heap);
		*tmp=cur;
		cur->u.pair->car=fklNewVMvalue(FKL_STR,fklCopyStr(exe->argv[i]),exe->heap);
	}
	FKL_SET_RETURN("SYS_argv",retval,stack);
}

void SYS_go(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	if(exe->VMid==-1)
		FKL_RAISE_BUILTIN_ERROR("sys.go",FKL_CANTCREATETHREAD,runnable,exe);
	FklVMvalue* threadProc=fklGET_VAL(fklPopVMstack(stack),heap);
	if(!threadProc)
		FKL_RAISE_BUILTIN_ERROR("sys.go",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PROC(threadProc)&&!FKL_IS_DLPROC(threadProc)&&!FKL_IS_CONT(threadProc)&&!FKL_IS_FLPROC(threadProc))
		FKL_RAISE_BUILTIN_ERROR("sys.go",FKL_WRONGARG,runnable,exe);
	FklVM* threadVM=(FKL_IS_PROC(threadProc)||FKL_IS_CONT(threadProc))?fklNewThreadVM(threadProc->u.proc,exe->heap):fklNewThreadDlprocVM(runnable,exe->heap);
	threadVM->lnt=exe->lnt;
	threadVM->code=exe->code;
	threadVM->size=exe->size;
	FklVMstack* threadVMstack=threadVM->stack;
	FklVMvalue* prevBp=FKL_MAKE_VM_I32(threadVMstack->bp);
	FKL_SET_RETURN("SYS_go",prevBp,threadVMstack);
	threadVMstack->bp=threadVMstack->tp;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklPtrStack* comStack=fklNewPtrStack(32,16);
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
		fklPushPtrStack(cur,comStack);
	fklResBp(stack);
	while(!fklIsPtrStackEmpty(comStack))
	{
		FklVMvalue* tmp=fklPopPtrStack(comStack);
		FKL_SET_RETURN("SYS_go",tmp,threadVMstack);
	}
	fklFreePtrStack(comStack);
	FklVMvalue* chan=threadVM->chan;
	int32_t faildCode=0;
	if(FKL_IS_PROC(threadProc))
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMfunc,threadVM);
	else if(FKL_IS_CONT(threadProc))
	{
		fklCreateCallChainWithContinuation(threadVM,threadProc->u.cont);
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMfunc,threadVM);
	}
	else if(FKL_IS_DLPROC(threadProc))
	{
		void* a[2]={threadVM,threadProc->u.dlproc->func};
		void** p=(void**)fklCopyMemory(a,sizeof(a));
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMdlprocFunc,p);
	}
	else
	{
		void* a[2]={threadVM,threadProc->u.flproc};
		void** p=(void**)fklCopyMemory(a,sizeof(a));
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMflprocFunc,p);
	}
	if(faildCode)
	{
		fklDeleteCallChain(threadVM);
		fklFreeVMstack(threadVM->stack);
		threadVM->stack=NULL;
		threadVM->lnt=NULL;
		FKL_SET_RETURN("SYS_go",FKL_MAKE_VM_I32(faildCode),stack);
	}
	else
		FKL_SET_RETURN("SYS_go",chan,stack);
}

void SYS_chanl(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* maxSize=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.chanl",FKL_TOOMANYARG,runnable,exe);
	if(!maxSize)
		FKL_RAISE_BUILTIN_ERROR("sys.chanl",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_I32(maxSize))
		FKL_RAISE_BUILTIN_ERROR("sys.chanl",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN("SYS_chanl",fklNewVMvalue(FKL_CHAN,fklNewVMchanl(FKL_GET_I32(maxSize)),exe->heap),stack);
}

void SYS_send(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* message=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* ch=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.send",FKL_TOOMANYARG,runnable,exe);
	if(!message||!ch)
		FKL_RAISE_BUILTIN_ERROR("sys.send",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_CHAN(ch))
		FKL_RAISE_BUILTIN_ERROR("sys.send",FKL_WRONGARG,runnable,exe);
	FklVMsend* t=fklNewVMsend(message);
	FKL_SET_RETURN("SYS_send",ch,stack);
	fklChanlSend(t,ch->u.chan,gclock);
	fklPopVMstack(stack);
	FKL_SET_RETURN("SYS_send",message,stack);
}

void SYS_recv(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* ch=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.recv",FKL_TOOMANYARG,runnable,exe);
	if(!ch)
		FKL_RAISE_BUILTIN_ERROR("sys.recv",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_CHAN(ch))
		FKL_RAISE_BUILTIN_ERROR("sys.recv",FKL_WRONGARG,runnable,exe);
	FklVMrecv* t=fklNewVMrecv();
	FKL_SET_RETURN("SYS_recv",ch,stack);
	fklChanlRecv(t,ch->u.chan,gclock);
	fklPopVMstack(stack);
	FKL_SET_RETURN("SYS_recv",t->v,stack);
	fklFreeVMrecv(t);
}

void SYS_error(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* who=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* type=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* message=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.error",FKL_TOOMANYARG,runnable,exe);
	if(!type||!message)
		FKL_RAISE_BUILTIN_ERROR("sys.error",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(who)&&!FKL_IS_STR(who)))
		FKL_RAISE_BUILTIN_ERROR("sys.error",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN("SYS_error",fklNewVMvalue(FKL_ERR,fklNewVMerrorWithSid((FKL_IS_SYM(who))?fklGetGlobSymbolWithId(FKL_GET_SYM(who))->symbol:who->u.str,FKL_GET_SYM(type),message->u.str),exe->heap),stack);
}

void SYS_raise(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* err=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.raise",FKL_TOOMANYARG,runnable,exe);
	if(!err)
		FKL_RAISE_BUILTIN_ERROR("sys.raise",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_ERR(err))
		FKL_RAISE_BUILTIN_ERROR("sys.raise",FKL_WRONGARG,runnable,exe);
	fklRaiseVMerror(err,exe);
}

void SYS_call_cc(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* proc=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.call/cc",FKL_TOOMANYARG,runnable,exe);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR("sys.call/cc",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PTR(proc)||(proc->type!=FKL_PROC&&proc->type!=FKL_CONT&&proc->type!=FKL_DLPROC))
		FKL_RAISE_BUILTIN_ERROR("sys.call/cc",FKL_INVOKEERROR,runnable,exe);
	FklVMvalue* cc=fklNewVMvalue(FKL_CONT,fklNewVMcontinuation(stack,exe->rstack,exe->tstack),exe->heap);
	FKL_SET_RETURN("SYS_call_cc",FKL_MAKE_VM_I32(stack->bp),stack);
	stack->bp=stack->tp;
	FKL_SET_RETURN("SYS_call_cc",cc,stack);
	if(proc->type==FKL_PROC)
	{
		FklVMproc* tmpProc=proc->u.proc;
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
	else if(proc->type==FKL_CONT)
	{
		VMcontinuation* cc=proc->u.cont;
		fklCreateCallChainWithContinuation(exe,cc);
	}
	else
	{
		FklVMdllFunc dllfunc=proc->u.dlproc->func;
		dllfunc(exe,gclock);
	}
}

void SYS_apply(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* proc=fklGET_VAL(fklPopVMstack(stack),heap);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR("sys.apply",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PTR(proc)||(proc->type!=FKL_PROC&&proc->type!=FKL_CONT&&proc->type!=FKL_DLPROC&&proc->type!=FKL_FLPROC))
		FKL_RAISE_BUILTIN_ERROR("b.invoke",FKL_INVOKEERROR,runnable,exe);
	FklPtrStack* stack1=fklNewPtrStack(32,16);
	FklVMvalue* value=NULL;
	FklVMvalue* lastList=NULL;
	while((value=fklGET_VAL(fklPopVMstack(stack),heap)))
	{
		if(!fklResBp(stack))
		{
			lastList=value;
			break;
		}
		fklPushPtrStack(value,stack1);
	}
	FklPtrStack* stack2=fklNewPtrStack(32,16);
	if(!FKL_IS_PAIR(lastList)&&lastList!=FKL_VM_NIL)
	{
		fklFreePtrStack(stack1);
		fklFreePtrStack(stack2);
		FKL_RAISE_BUILTIN_ERROR("sys.apply",FKL_WRONGARG,runnable,exe);
	}
	for(;FKL_IS_PAIR(lastList);lastList=fklGetVMpairCdr(lastList))
		fklPushPtrStack(fklGetVMpairCar(lastList),stack2);
	if(lastList!=FKL_VM_NIL)
	{
		fklFreePtrStack(stack1);
		fklFreePtrStack(stack2);
		FKL_RAISE_BUILTIN_ERROR("sys.apply",FKL_WRONGARG,runnable,exe);
	}
	FKL_SET_RETURN("SYS_apply",FKL_MAKE_VM_I32(stack->bp),stack);
	stack->bp=stack->tp;
	while(!fklIsPtrStackEmpty(stack2))
	{
		FklVMvalue* t=fklPopPtrStack(stack2);
		FKL_SET_RETURN("SYS_apply",t,stack);
	}
	fklFreePtrStack(stack2);
	while(!fklIsPtrStackEmpty(stack1))
	{
		FklVMvalue* t=fklPopPtrStack(stack1);
		FKL_SET_RETURN("SYS_apply",t,stack);
	}
	fklFreePtrStack(stack1);
	switch(proc->type)
	{
		case FKL_PROC:
			invokeNativeProcdure(exe,proc->u.proc,runnable);
			break;
		case FKL_CONT:
			invokeContinuation(exe,proc->u.cont);
			break;
		case FKL_DLPROC:
			invokeDlProc(exe,proc->u.dlproc);
			break;
		case FKL_FLPROC:
			invokeFlproc(exe,proc->u.flproc);
		default:
			break;
	}
}

void SYS_newf(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* vsize=fklPopVMstack(stack);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.newf",FKL_TOOMANYARG,r,exe);
	if(!vsize)
		FKL_RAISE_BUILTIN_ERROR("sys.newf",FKL_TOOFEWARG,r,exe);
	if(!FKL_IS_I32(vsize)&&!FKL_IS_I64(vsize))
		FKL_RAISE_BUILTIN_ERROR("sys.newf",FKL_WRONGARG,r,exe);
	size_t size=FKL_IS_I32(vsize)?FKL_GET_I32(vsize):vsize->u.i64;
	uint8_t* mem=(uint8_t*)malloc(size);
	FKL_ASSERT(mem,"SYS_newf",__FILE__,__LINE__);
	FklVMvalue* retval=FKL_MAKE_VM_MEM(fklNewVMmem(0,mem),exe->heap);
	FKL_SET_RETURN("SYS_newf",retval,stack);
}

void SYS_delf(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* mem=fklPopVMstack(stack);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.delf",FKL_TOOMANYARG,r,exe);
	if(!mem)
		FKL_RAISE_BUILTIN_ERROR("sys.delf",FKL_TOOFEWARG,r,exe);
	if(!FKL_IS_MEM(mem))
		FKL_RAISE_BUILTIN_ERROR("sys.delf",FKL_WRONGARG,r,exe);
	FklVMmem* pmem=mem->u.chf;
	uint8_t* p=pmem->mem;
	free(p);
	pmem->mem=NULL;
}

void SYS_lfdl(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	FklVMvalue* vpath=fklPopVMstack(stack);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.lfdl",FKL_TOOMANYARG,r,exe);
	if(exe->VMid==-1)
		return;
	if(!vpath)
		FKL_RAISE_BUILTIN_ERROR("sys.lfdl",FKL_TOOFEWARG,r,exe);
	if(!FKL_IS_STR(vpath))
		FKL_RAISE_BUILTIN_ERROR("sys.lfdl",FKL_WRONGARG,r,exe);
	const char* path=vpath->u.str;
#ifdef _WIN32
	FklVMdllHandle handle=LoadLibrary(path);
	if(!handle)
	{
		TCHAR szBuf[128];
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage (
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				NULL,
				dw,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
				(LPTSTR) &lpMsgBuf,
				0, NULL );
		wsprintf(szBuf,
				_T("%s error Message (error code=%d): %s"),
				_T("CreateDirectory"), dw, lpMsgBuf);
		LocalFree(lpMsgBuf);
		fprintf(stderr,"%s\n",szBuf);
	}
#else
	FklVMdllHandle handle=dlopen(path,RTLD_LAZY);
	if(!handle)
	{
		perror(dlerror());
		putc('\n',stderr);
	}
#endif
	FklSharedObjNode* node=(FklSharedObjNode*)malloc(sizeof(FklSharedObjNode));
	FKL_ASSERT(node,"B_load_shared_obj",__FILE__,__LINE__);
	node->dll=handle;
	pthread_mutex_lock(&GlobSharedObjsMutex);
	node->next=GlobSharedObjs;
	GlobSharedObjs=node;
	pthread_mutex_unlock(&GlobSharedObjsMutex);
}

void SYS_reverse(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.reverse",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.reverse",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PAIR(obj)&&obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR("sys.reverse",FKL_WRONGARG,runnable,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	if(obj!=FKL_VM_NIL)
	{
		FklVMheap* heap=exe->heap;
		for(FklVMvalue* cdr=obj;cdr!=FKL_VM_NIL;cdr=fklGetVMpairCdr(cdr))
		{
			FklVMvalue* pair=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),heap);
			pair->u.pair->cdr=retval;
			pair->u.pair->car=fklGetVMpairCar(cdr);
			retval=pair;
		}
	}
	FKL_SET_RETURN("SYS_reverse",retval,stack);
}
