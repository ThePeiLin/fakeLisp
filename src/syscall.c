#include<fakeLisp/common.h>
#include<fakeLisp/fakedef.h>
#include<fakeLisp/VMtool.h>
#include<fakeLisp/reader.h>
#include<fakeLisp/fakeVM.h>
#include"utils.h"
#include<string.h>
#include<math.h>
#include<setjmp.h>
#include<dlfcn.h>

extern void invokeNativeProcdure(FklVM*,FklVMproc*,FklVMrunnable*);
extern void invokeContinuation(FklVM*,VMcontinuation*);
extern void invokeDlProc(FklVM*,FklVMDlproc*);
extern void invokeFlproc(FklVM*,FklVMFlproc*);
extern pthread_mutex_t GlobSharedObjsMutex;
extern FklSharedObjNode* GlobSharedObjs;
extern const char* builtInSymbolList[NUM_OF_BUILT_IN_SYMBOL];
extern const char* builtInErrorType[NUM_OF_BUILT_IN_ERROR_TYPE];
extern FklTypeId_t CharTypeId;
extern FklVMlist GlobVMs;
extern void* ThreadVMFunc(void* p);
extern void* ThreadVMDlprocFunc(void* p);
extern void* ThreadVMFlprocFunc(void* p);

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
void SYS_chr(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_int(FklVM* exe,pthread_rwlock_t* gclock);
void SYS_dbl(FklVM* exe,pthread_rwlock_t* gclock);
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
void SYS_clcc(FklVM*,pthread_rwlock_t*);
void SYS_apply(FklVM*,pthread_rwlock_t*);
void SYS_newf(FklVM*,pthread_rwlock_t*);
void SYS_delf(FklVM*,pthread_rwlock_t*);
void SYS_lfdl(FklVM*,pthread_rwlock_t*);

//end



void SYS_car(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.car",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.car",TOOFEWARG,runnable,exe);
	if(!IS_PAIR(obj))
		RAISE_BUILTIN_ERROR("sys.car",WRONGARG,runnable,exe);
	SET_RETURN("SYS_car",MAKE_VM_REF(&obj->u.pair->car),stack);
}

void SYS_cdr(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.cdr",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.cdr",TOOFEWARG,runnable,exe);
	if(!IS_PAIR(obj))
		RAISE_BUILTIN_ERROR("sys.cdr",WRONGARG,runnable,exe);
	SET_RETURN("SYS_cdr",MAKE_VM_REF(&obj->u.pair->cdr),stack);
}

void SYS_cons(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* car=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* cdr=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.cons",TOOMANYARG,runnable,exe);
	if(!car||!cdr)
		RAISE_BUILTIN_ERROR("sys.cons",TOOFEWARG,runnable,exe);
	FklVMvalue* pair=fklNewVMvalue(PAIR,fklNewVMpair(),exe->heap);
	pair->u.pair->car=car;
	pair->u.pair->cdr=cdr;
	SET_RETURN("SYS_cons",pair,stack);
}

void SYS_append(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* retval=VM_NIL;
	VMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue** prev=&retval;
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(*prev==VM_NIL)
			*prev=fklCopyVMvalue(cur,exe->heap);
		else if(IS_PAIR(*prev))
		{
			for(;IS_PAIR(*prev);prev=&(*prev)->u.pair->cdr);
			*prev=fklCopyVMvalue(cur,exe->heap);
		}
		else if(IS_STR(*prev))
		{
			if(!IS_PTR(cur))
				RAISE_BUILTIN_ERROR("sys.append",WRONGARG,runnable,exe);
			(*prev)->u.str=fklStrCat((*prev)->u.str,cur->u.str);
		}
		else if(IS_BYTS(*prev))
		{
			if(!IS_BYTS(*prev))
				RAISE_BUILTIN_ERROR("sys.append",WRONGARG,runnable,exe);
			fklVMBytsCat(&(*prev)->u.byts,cur->u.byts);
		}
		else
			RAISE_BUILTIN_ERROR("sys.append",WRONGARG,runnable,exe);
	}
	fklResBp(stack);
	SET_RETURN("SYS_append",retval,stack);
}

void SYS_atom(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* arg=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.atom",TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR("sys.atom",TOOFEWARG,runnable,exe);
	if(!IS_PAIR(arg))
		SET_RETURN("SYS_atom",VM_TRUE,stack);
	else
		SET_RETURN("SYS_atom",VM_NIL,stack);
}

void SYS_null(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* arg=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.null",TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR("sys.null",TOOFEWARG,runnable,exe);
	if(arg==VM_NIL)
		SET_RETURN("SYS_null",VM_TRUE,stack);
	else
		SET_RETURN("SYS_null",VM_NIL,stack);
}

void SYS_not(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* arg=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.not",TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR("sys.not",TOOFEWARG,runnable,exe);
	if(arg==VM_NIL)
		SET_RETURN("SYS_not",VM_TRUE,stack);
	else
		SET_RETURN("SYS_not",VM_NIL,stack);
}

void SYS_eq(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* fir=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* sec=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.eq",TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR("sys.eq",TOOFEWARG,runnable,exe);
	SET_RETURN("SYS_eq",fir==sec
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_eqn(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* fir=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* sec=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.eqn",TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR("sys.eqn",TOOFEWARG,runnable,exe);
	if((IS_DBL(fir)||IS_I32(fir)||IS_I64(fir))&&(IS_DBL(sec)||IS_I32(sec)||IS_I64(sec)))
		SET_RETURN("SYS_eqn",((((IS_DBL(fir))?*fir->u.dbl
							:((IS_I32(fir)?GET_I32(fir)
								:*fir->u.i64)))
						-((IS_DBL(sec))?*sec->u.dbl
							:((IS_I32(sec)?GET_I32(sec)
								:*sec->u.i64))))
					==0.0)
				?VM_TRUE
				:VM_NIL
				,stack);
	else if(IS_SYM(fir)&&IS_SYM(sec))
		SET_RETURN("SYS_eqn",fir==sec
				?VM_TRUE
				:VM_NIL
				,stack);
	else if(IS_STR(fir)&&IS_STR(sec))
		SET_RETURN("SYS_eqn",(!strcmp(fir->u.str,sec->u.str))
				?VM_TRUE
				:VM_NIL
				,stack);
	else if(IS_BYTS(fir)&&IS_BYTS(sec))
		SET_RETURN("SYS_eqn",fklEqVMByts(fir->u.byts,sec->u.byts)
				?VM_TRUE
				:VM_NIL
				,stack);
	else
		RAISE_BUILTIN_ERROR("sys.eqn",WRONGARG,runnable,exe);

}

void SYS_equal(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* fir=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* sec=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.equal",TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR("sys.equal",TOOFEWARG,runnable,exe);
	SET_RETURN("SYS_equal",(fklVMvaluecmp(fir,sec))
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_add(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	int64_t r64=0;
	double rd=0.0;
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(IS_I32(cur))
			r64+=GET_I32(cur);
		else if(IS_DBL(cur))
			rd+=*cur->u.dbl;
		else if(IS_I64(cur))
			r64+=*cur->u.i64;
		else
			RAISE_BUILTIN_ERROR("sys.add",WRONGARG,runnable,exe);
	}
	fklResBp(stack);
	if(rd!=0.0)
	{
		rd+=r64;
		SET_RETURN("SYS_add",fklNewVMvalue(FKL_DBL,&rd,exe->heap),stack);
	}
	else
	{
		if(r64>INT32_MAX||r64<INT32_MIN)
			SET_RETURN("SYS_add",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
		else
			SET_RETURN("SYS_add",MAKE_VM_I32(r64),stack);
	}
}

void SYS_add_1(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* arg=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.add_1",TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR("sys.add_1",TOOFEWARG,runnable,exe);
	if(!IS_DBL(arg)&&!IS_I32(arg)&&!IS_I64(arg))
		RAISE_BUILTIN_ERROR("sys.add_1",WRONGARG,runnable,exe);
	if(IS_DBL(arg))
	{
		double r=*arg->u.dbl+1.0;
		SET_RETURN("SYS_add_1",fklNewVMvalue(FKL_DBL,&r,exe->heap),stack);
	}
	else if(IS_I64(arg))
	{
		int64_t r=*arg->u.i64+1;
		SET_RETURN("SYS_add_1",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else
		SET_RETURN("SYS_add_1",MAKE_VM_I32(GET_I32(arg)+1),stack);
}

void SYS_sub(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* prev=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	double rd=0.0;
	int64_t r64=0;
	if(!prev)
		RAISE_BUILTIN_ERROR("sys.sub",TOOFEWARG,runnable,exe);
	if(!IS_DBL(prev)&&!IS_I32(prev)&&!IS_I64(prev))
		RAISE_BUILTIN_ERROR("sys.sub",WRONGARG,runnable,exe);
	if(!cur)
	{
		fklResBp(stack);
		if(IS_DBL(prev))
		{
			rd=-(*prev->u.dbl);
			SET_RETURN("SYS_sub",fklNewVMvalue(FKL_DBL,&rd,exe->heap),stack);
		}
		else if(IS_I32(prev))
		{
			r64=-GET_I32(prev);
			SET_RETURN("SYS_sub",MAKE_VM_I32(r64),stack);
		}
		else
		{
			r64=-(*prev->u.i64);
			SET_RETURN("SYS_sub",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
		}
	}
	else
	{
		for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
		{
			if(IS_I32(cur))
				r64+=GET_I32(cur);
			else if(IS_DBL(cur))
				rd+=*cur->u.dbl;
			else if(IS_I64(cur))
				r64+=*cur->u.i64;
			else
				RAISE_BUILTIN_ERROR("sys.sub",WRONGARG,runnable,exe);
		}
		fklResBp(stack);
		if(IS_DBL(prev)||rd!=0.0)
		{
			rd=((IS_DBL(prev))?*prev->u.dbl:((IS_I64(prev))?*prev->u.i64:GET_I32(prev)))-rd-r64;
			SET_RETURN("SYS_sub",fklNewVMvalue(FKL_DBL,&rd,exe->heap),stack);
		}
		else
		{
			r64=(IS_I64(prev))?*prev->u.i64:GET_I32(prev)-r64;
			if(r64>INT32_MAX||r64<INT32_MIN)
				SET_RETURN("SYS_sub",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
			else
				SET_RETURN("SYS_sub",MAKE_VM_I32(r64),stack);
		}
	}
}

void SYS_sub_1(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* arg=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.sub_1",TOOMANYARG,runnable,exe);
	if(!arg)
		RAISE_BUILTIN_ERROR("sys.add_1",TOOFEWARG,runnable,exe);
	if(!IS_DBL(arg)&&!IS_I32(arg)&&!IS_I64(arg))
		RAISE_BUILTIN_ERROR("sys.add_1",WRONGARG,runnable,exe);
	if(IS_DBL(arg))
	{
		double r=*arg->u.dbl-1.0;
		SET_RETURN("SYS_sub_1",fklNewVMvalue(FKL_DBL,&r,exe->heap),stack);
	}
	else if(IS_I64(arg))
	{
		int64_t r=*arg->u.i64-1;
		SET_RETURN("SYS_sub_1",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else
		SET_RETURN("SYS_sub_1",MAKE_VM_I32(GET_I32(arg)-1),stack);
}

void SYS_mul(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	double rd=1.0;
	int64_t r64=1;
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(IS_I32(cur))
			r64*=GET_I32(cur);
		else if(IS_DBL(cur))
			rd*=*cur->u.dbl;
		else if(IS_I64(cur))
			r64*=*cur->u.i64;
		else
			RAISE_BUILTIN_ERROR("sys.mul",WRONGARG,runnable,exe);
	}
	fklResBp(stack);
	if(rd!=1.0)
	{
		rd*=r64;
		SET_RETURN("SYS_mul",fklNewVMvalue(FKL_DBL,&rd,exe->heap),stack);
	}
	else
	{
		if(r64>INT32_MAX||r64<INT32_MIN)
			SET_RETURN("SYS_mul",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
		else
			SET_RETURN("SYS_mul",MAKE_VM_I32(r64),stack);
	}
}

void SYS_div(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* prev=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	int64_t r64=1;
	double rd=1.0;
	if(!prev)
		RAISE_BUILTIN_ERROR("sys.div",TOOFEWARG,runnable,exe);
	if(!IS_DBL(prev)&&!IS_I32(prev)&&!IS_I64(prev))
		RAISE_BUILTIN_ERROR("sys.div",WRONGARG,runnable,exe);
	if(!cur)
	{
		fklResBp(stack);
		if(IS_DBL(prev))
		{
			if(*prev->u.dbl==0.0)
				RAISE_BUILTIN_ERROR("sys.div",DIVZERROERROR,runnable,exe);
			rd=1/(*prev->u.dbl);
			SET_RETURN("SYS_sub",fklNewVMvalue(FKL_DBL,&rd,exe->heap),stack);
		}
		else if(IS_I64(prev))
		{
			r64=*prev->u.i64;
			if(!r64)
				RAISE_BUILTIN_ERROR("sys.div",DIVZERROERROR,runnable,exe);
			if(1%r64)
			{
				rd=1.0/r64;
				SET_RETURN("SYS_div",fklNewVMvalue(FKL_DBL,&rd,exe->heap),stack);
			}
			else
			{
				r64=1/r64;
				SET_RETURN("SYS_div",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
			}
		}
		else
		{
			if(!GET_I32(prev))
				RAISE_BUILTIN_ERROR("sys.div",DIVZERROERROR,runnable,exe);
			if(1%GET_I32(prev))
			{
				rd=1.0/GET_I32(prev);
				SET_RETURN("SYS_div",fklNewVMvalue(FKL_DBL,&rd,exe->heap),stack);
			}
			else
				SET_RETURN("SYS_div",MAKE_VM_I32(1),stack);
		}
	}
	else
	{
		for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
		{
			if(IS_I32(cur))
				r64*=GET_I32(cur);
			else if(IS_DBL(cur))
				rd*=*cur->u.dbl;
			else if(IS_I64(cur))
				r64*=*cur->u.i64;
			else
				RAISE_BUILTIN_ERROR("sys.div",WRONGARG,runnable,exe);
		}
		if((r64)==0)
			RAISE_BUILTIN_ERROR("sys.div",DIVZERROERROR,runnable,exe);
		fklResBp(stack);
		if(IS_DBL(prev)||rd!=1.0||(IS_I32(prev)&&GET_I32(prev)%(r64))||(IS_I64(prev)&&*prev->u.i64%(r64)))
		{
			if(rd==0.0||r64==0)
				RAISE_BUILTIN_ERROR("sys.div",DIVZERROERROR,runnable,exe);
			rd=((double)((IS_DBL(prev))?*prev->u.dbl:(IS_I32(prev)?GET_I32(prev):*prev->u.i64)))/rd/r64;
			SET_RETURN("SYS_div",fklNewVMvalue(FKL_DBL,&rd,exe->heap),stack);
		}
		else
		{
			if(r64==0)
				RAISE_BUILTIN_ERROR("sys.div",DIVZERROERROR,runnable,exe);
			//if(r64!=1)
			//{
			//	r64=(IS_I32(prev)?GET_I32(prev):*prev->u.i64)/r64;
			//	SET_RETURN("SYS_div",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
			//}
			else
			{
				r64=GET_I32(prev)/r64;
				if(r64>INT32_MAX||r64<INT32_MIN)
					SET_RETURN("SYS_div",fklNewVMvalue(FKL_I64,&r64,exe->heap),stack);
				else
					SET_RETURN("SYS_div",MAKE_VM_I32(r64),stack);
			}
		}
	}
}

void SYS_rem(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* fir=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* sec=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.rem",TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		RAISE_BUILTIN_ERROR("sys.rem",TOOFEWARG,runnable,exe);
	if(!((IS_DBL(fir)||IS_I32(fir)||IS_I64(fir))&&(IS_DBL(sec)||IS_I32(sec)||IS_I64(sec))))
		RAISE_BUILTIN_ERROR("sys.rem",WRONGARG,runnable,exe);
	if(IS_DBL(fir)||IS_DBL(sec))
	{
		double af=(IS_DBL(fir))?*fir->u.dbl:(IS_I32(fir)?GET_I32(fir):*fir->u.i64);
		double as=(IS_DBL(sec))?*sec->u.dbl:(IS_I32(sec)?GET_I32(sec):*sec->u.i64);
		if(as==0.0)
			RAISE_BUILTIN_ERROR("sys.rem",DIVZERROERROR,runnable,exe);
		double r=fmod(af,as);
		SET_RETURN("SYS_rem",fklNewVMvalue(FKL_DBL,&r,exe->heap),stack);
	}
	else if(IS_I32(fir)&&IS_I32(sec))
	{
		if(!GET_I32(sec))
			RAISE_BUILTIN_ERROR("sys.rem",DIVZERROERROR,runnable,exe);
		int32_t r=GET_I32(fir)%GET_I32(sec);
		SET_RETURN("SYS_rem",MAKE_VM_I32(r),stack);
	}
	else
	{
		int64_t r=(IS_I32(fir)?GET_I32(fir):*fir->u.i64)%(IS_I32(sec)?GET_I32(sec):*sec->u.i64);
		SET_RETURN("SYS_rem",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
}

void SYS_gt(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	int r=1;
	VMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR("sys.gt",TOOFEWARG,runnable,exe);
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(prev)
		{
			if((IS_DBL(prev)||IS_I32(prev)||IS_I64(prev))
					&&(IS_DBL(cur)||IS_I32(cur)||IS_I64(cur)))
				r=((((IS_DBL(prev))?*prev->u.dbl
								:((IS_I32(prev))?GET_I32(prev)
									:*prev->u.i64))
							-((IS_DBL(cur))?*cur->u.dbl
								:((IS_I32(cur))?GET_I32(cur)
									:*cur->u.i64)))
						>0.0);
			else if(IS_STR(prev)&&IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)>0);
			else
				RAISE_BUILTIN_ERROR("sys.gt",WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap));
	fklResBp(stack);
	SET_RETURN("SYS_gt",r
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_ge(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	int r=1;
	VMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR("sys.ge",TOOFEWARG,runnable,exe);
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(prev)
		{
			if((IS_DBL(prev)||IS_I32(prev)||IS_I64(prev))
					&&(IS_DBL(cur)||IS_I32(cur)||IS_I64(cur)))
				r=((((IS_DBL(prev))?*prev->u.dbl
								:((IS_I32(prev))?GET_I32(prev)
									:*prev->u.i64))
							-((IS_DBL(cur))?*cur->u.dbl
								:((IS_I32(cur))?GET_I32(cur)
									:*cur->u.i64)))
						>=0.0);
			else if(IS_STR(prev)&&IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)>=0);
			else
				RAISE_BUILTIN_ERROR("sys.ge",WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap));
	fklResBp(stack);
	SET_RETURN("SYS_ge",r
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_lt(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	int r=1;
	VMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR("sys.lt",TOOFEWARG,runnable,exe);
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(prev)
		{
			if((IS_DBL(prev)||IS_I32(prev)||IS_I64(prev))
					&&(IS_DBL(cur)||IS_I32(cur)||IS_I64(cur)))
				r=((((IS_DBL(prev))?*prev->u.dbl
								:((IS_I32(prev))?GET_I32(prev)
									:*prev->u.i64))
							-((IS_DBL(cur))?*cur->u.dbl
								:((IS_I32(cur))?GET_I32(cur)
									:*cur->u.i64)))
						<0.0);
			else if(IS_STR(prev)&&IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)<0);
			else
				RAISE_BUILTIN_ERROR("sys.lt",WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap));
	fklResBp(stack);
	SET_RETURN("SYS_lt",r
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_le(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	int r=1;
	VMheap* heap=exe->heap;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		RAISE_BUILTIN_ERROR("sys.le",TOOFEWARG,runnable,exe);
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
	{
		if(prev)
		{
			if((IS_DBL(prev)||IS_I32(prev)||IS_I64(prev))
					&&(IS_DBL(cur)||IS_I32(cur)||IS_I64(cur)))
				r=((((IS_DBL(prev))?*prev->u.dbl
								:((IS_I32(prev))?GET_I32(prev)
									:*prev->u.i64))
							-((IS_DBL(cur))?*cur->u.dbl
								:((IS_I32(cur))?GET_I32(cur)
									:*cur->u.i64)))
						<=0.0);
			else if(IS_STR(prev)&&IS_STR(cur))
				r=(strcmp(prev->u.str,cur->u.str)<=0);
			else
				RAISE_BUILTIN_ERROR("sys.le",WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap));
	fklResBp(stack);
	SET_RETURN("SYS_le",r
			?VM_TRUE
			:VM_NIL
			,stack);
}

void SYS_chr(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.chr",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.chr",TOOFEWARG,runnable,exe);
	if(IS_I32(obj))
		SET_RETURN("SYS_chr",MAKE_VM_CHR(GET_I32(obj)),stack);
	else if(IS_CHR(obj))
		SET_RETURN("SYS_chr",obj,stack);
	else if(IS_DBL(obj))
		SET_RETURN("SYS_chr",MAKE_VM_CHR((int32_t)*obj->u.dbl),stack);
	else if(IS_STR(obj))
		SET_RETURN("SYS_chr",MAKE_VM_CHR(obj->u.str[0]),stack);
	else if(IS_BYTS(obj))
		SET_RETURN("SYS_chr",MAKE_VM_CHR(obj->u.byts->str[0]),stack);
	else
		RAISE_BUILTIN_ERROR("sys.chr",WRONGARG,runnable,exe);
}

void SYS_int(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.int",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.int",TOOFEWARG,runnable,exe);
	if(IS_CHR(obj))
		SET_RETURN("SYS_int",MAKE_VM_I32(GET_CHR(obj)),stack);
	else if(IS_I32(obj))
		SET_RETURN("SYS_int",obj,stack);
	else if(IS_I64(obj))
		SET_RETURN("SYS_int",fklNewVMvalue(FKL_I64,obj->u.i64,exe->heap),stack);
	else if(IS_DBL(obj))
	{
		int64_t r=(int64_t)*obj->u.dbl;
		if(r>INT32_MAX||r<INT32_MIN)
			SET_RETURN("SYS_int",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
		else
			SET_RETURN("SYS_int",MAKE_VM_I32((int32_t)r),stack);
	}
	else if(IS_STR(obj))
	{
		int64_t r=fklStringToInt(obj->u.str);
		if(r>INT32_MAX||r<INT32_MIN)
			SET_RETURN("SYS_int",fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
		else
			SET_RETURN("SYS_int",MAKE_VM_I32(r),stack);
	}
	else if(IS_BYTS(obj))
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
			SET_RETURN("SYS_int",MAKE_VM_I32(*(int32_t*)r),stack);
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
			SET_RETURN("SYS_int",fklNewVMvalue(FKL_I64,r,exe->heap),stack);
		}
	}
	else
		RAISE_BUILTIN_ERROR("sys.int",WRONGARG,runnable,exe);
}

void SYS_dbl(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.dbl",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.dbl",TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNewVMvalue(FKL_DBL,NULL,exe->heap);
	if(IS_CHR(obj))
		*retval->u.dbl=(double)GET_CHR(obj);
	else if(IS_I32(obj))
		*retval->u.dbl=(double)GET_I32(obj);
	else if(IS_DBL(obj))
		*retval->u.dbl=*obj->u.dbl;
	else if(IS_STR(obj))
		*retval->u.dbl=fklStringToDouble(obj->u.str);
	else if(IS_BYTS(obj))
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
		memcpy(retval->u.dbl,r,8);
	}
	else
		RAISE_BUILTIN_ERROR("sys.dbl",WRONGARG,runnable,exe);
	SET_RETURN("SYS_dbl",retval,stack);
}

void SYS_str(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.str",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.str",TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNewVMvalue(FKL_STR,NULL,exe->heap);
	if(IS_I32(obj))
		retval->u.str=fklIntToString(GET_I32(obj));
	else if(IS_DBL(obj))
		retval->u.str=fklDoubleToString(*obj->u.dbl);
	else if(IS_CHR(obj))
		retval->u.str=fklCopyStr((char[]){GET_CHR(obj),'\0'});
	else if(IS_SYM(obj))
		retval->u.str=fklCopyStr(fklGetGlobSymbolWithId(GET_SYM(obj))->symbol);
	else if(IS_STR(obj))
		retval->u.str=fklCopyStr(obj->u.str);
	else if(IS_BYTS(obj))
	{
		size_t s=obj->u.byts->size;
		size_t l=(obj->u.byts->str[s-1])?s+1:s;
		char* t=(char*)malloc(sizeof(char)*l);
		FAKE_ASSERT(t,"SYS_str",__FILE__,__LINE__);
		memcpy(t,obj->u.byts->str,s);
		t[l-1]='\0';
		retval->u.str=t;
	}
	else
		RAISE_BUILTIN_ERROR("sys.str",WRONGARG,runnable,exe);
	SET_RETURN("SYS_str",retval,stack);
}

void SYS_sym(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.sym",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.sym",TOOFEWARG,runnable,exe);
	if(IS_CHR(obj))
		SET_RETURN("SYS_sym",MAKE_VM_SYM(fklAddSymbolToGlob(((char[]){GET_CHR(obj),'\0'}))->id),stack);
	else if(IS_SYM(obj))
		SET_RETURN("SYS_sym",obj,stack);
	else if(IS_STR(obj))
		SET_RETURN("SYS_sym",MAKE_VM_SYM(fklAddSymbolToGlob(obj->u.str)->id),stack);
	else
		RAISE_BUILTIN_ERROR("sys.sym",WRONGARG,runnable,exe);
}

void SYS_byts(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.byts",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.byts",TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNewVMvalue(FKL_BYTS,fklNewEmptyVMByts(),exe->heap);
	if(IS_I32(obj))
	{
		retval->u.byts=(FklVMByts*)realloc(retval->u.byts,sizeof(FklVMByts)+sizeof(int32_t));
		FAKE_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
		retval->u.byts->size=sizeof(int32_t);
		*(int32_t*)retval->u.byts->str=GET_I32(obj);
	}
	else if(IS_DBL(obj))
	{
		retval->u.byts=(FklVMByts*)realloc(retval->u.byts,sizeof(FklVMByts)+sizeof(double));
		FAKE_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
		retval->u.byts->size=sizeof(double);
		*(double*)retval->u.byts->str=*obj->u.dbl;
	}
	else if(IS_CHR(obj))
	{
		retval->u.byts=(FklVMByts*)realloc(retval->u.byts,sizeof(FklVMByts)+sizeof(char));
		FAKE_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
		retval->u.byts->size=sizeof(char);
		*(char*)retval->u.byts->str=GET_CHR(obj);
	}
	else if(IS_SYM(obj))
	{
		FklSymTabNode* n=fklGetGlobSymbolWithId(GET_SYM(obj));
		retval->u.byts=(FklVMByts*)realloc(retval->u.byts,sizeof(FklVMByts)+strlen(n->symbol));
		FAKE_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
		retval->u.byts->size=strlen(n->symbol);
		memcpy(retval->u.byts->str,n->symbol,retval->u.byts->size);
	}
	else if(IS_STR(obj))
	{
		retval->u.byts=(FklVMByts*)realloc(retval->u.byts,sizeof(FklVMByts)+strlen(obj->u.str)+1);
		FAKE_ASSERT(retval->u.byts,"SYS_byts",__FILE__,__LINE__);
		retval->u.byts->size=strlen(obj->u.str);
		memcpy(retval->u.byts->str,obj->u.str,retval->u.byts->size);
	}
	else if(IS_BYTS(obj))
		fklVMBytsCat(&retval->u.byts,obj->u.byts);
	else
		RAISE_BUILTIN_ERROR("sys.byts",WRONGARG,runnable,exe);
	SET_RETURN("SYS_byts",retval,stack);
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
			"chr",
			"dbl",
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
		for(;i<ATM;i++)
			a[i]=fklAddSymbolToGlob(b[i])->id;
	}
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.type",TOOMANYARG,runnable,exe);
	size_t type=0;
	switch(GET_TAG(obj))
	{
		case NIL_TAG:type=NIL;break;
		case FKL_I32_TAG:type=FKL_I32;break;
		case FKL_SYM_TAG:type=FKL_SYM;break;
		case FKL_CHR_TAG:type=FKL_CHR;break;
		case PTR_TAG:type=obj->type;break;
		default:
			RAISE_BUILTIN_ERROR("sys.type",WRONGARG,runnable,exe);
			break;
	}
	SET_RETURN("SYS_type",MAKE_VM_SYM(a[type]),stack);
}

void SYS_nth(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* place=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* objlist=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.nth",TOOMANYARG,runnable,exe);
	if(!place||!objlist)
		RAISE_BUILTIN_ERROR("sys.nth",TOOFEWARG,runnable,exe);
	if(!IS_I32(place)&&!IS_I64(place))
		RAISE_BUILTIN_ERROR("sys.nth",WRONGARG,runnable,exe);
	FklVMvalue* retval=NULL;
	ssize_t index=IS_I32(place)?GET_I32(place):*place->u.i64;
	if(objlist==VM_NIL||IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		retval=(IS_PAIR(objPair))?MAKE_VM_REF(&objPair->u.pair->car):VM_NIL;
	}
	else if(IS_STR(objlist))
		retval=index>=strlen(objlist->u.str)?VM_NIL:MAKE_VM_CHF(fklNewVMMem(CharTypeId,(uint8_t*)objlist->u.str+index),heap);
	else if(IS_BYTS(objlist))
		retval=index>=objlist->u.byts->size?VM_NIL:MAKE_VM_CHF(fklNewVMMem(CharTypeId,objlist->u.byts->str+index),heap);
	else
		RAISE_BUILTIN_ERROR("sys.nth",WRONGARG,runnable,exe);
	SET_RETURN("SYS_nth",retval,stack);
}

void SYS_length(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.length",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.length",TOOFEWARG,runnable,exe);
	int32_t len=0;
	if(obj==VM_NIL||IS_PAIR(obj))
		for(;IS_PAIR(obj);obj=fklGetVMpairCdr(obj))len++;
	else if(IS_STR(obj))
		len=strlen(obj->u.str);
	else if(IS_BYTS(obj))
		len=obj->u.byts->size;
	else if(IS_CHAN(obj))
		len=fklGetNumVMChanl(obj->u.chan);
	else
		RAISE_BUILTIN_ERROR("sys.length",WRONGARG,runnable,exe);
	SET_RETURN("SYS_length",MAKE_VM_I32(len),stack);
}

void SYS_file(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* filename=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	FklVMvalue* mode=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.file",TOOMANYARG,runnable,exe);
	if(!mode||!filename)
		RAISE_BUILTIN_ERROR("sys.file",TOOFEWARG,runnable,exe);
	if(!IS_STR(filename)||!IS_STR(mode))
		RAISE_BUILTIN_ERROR("sys.file",WRONGARG,runnable,exe);
	FILE* file=fopen(filename->u.str,mode->u.str);
	FklVMvalue* obj=NULL;
	if(!file)
	{
		SET_RETURN("SYS_file",filename,stack);
		RAISE_BUILTIN_ERROR("sys.file",FILEFAILURE,runnable,exe);
	}
	else
		obj=fklNewVMvalue(FKL_FP,file,heap);
	SET_RETURN("SYS_file",obj,stack);
}

void SYS_read(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* stream=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.read",TOOMANYARG,runnable,exe);
	if(stream&&!IS_FP(stream)&&!IS_STR(stream))
		RAISE_BUILTIN_ERROR("sys.read",WRONGARG,runnable,exe);
	char* tmpString=NULL;
	FILE* tmpFile=NULL;
	if(!stream||IS_FP(stream))
	{
		tmpFile=stream?stream->u.fp:stdin;
		int unexpectEOF=0;
		char* prev=NULL;
		tmpString=fklReadInPattern(tmpFile,&prev,&unexpectEOF);
		if(prev)
			free(prev);
		if(unexpectEOF)
		{
			free(tmpString);
			RAISE_BUILTIN_ERROR("sys.read",UNEXPECTEOF,runnable,exe);
		}
	}
	else
		tmpString=fklCopyStr(stream->u.str);
	FklIntpr* tmpIntpr=fklNewTmpIntpr(NULL,tmpFile);
	FklAstCptr* tmpCptr=fklBaseCreateTree(tmpString,tmpIntpr);
	FklVMvalue* tmp=NULL;
	if(tmpCptr==NULL)
		tmp=VM_NIL;
	else
		tmp=fklCastCptrVMvalue(tmpCptr,exe->heap);
	SET_RETURN("SYS_read",tmp,stack);
	free(tmpIntpr);
	free(tmpString);
	fklDeleteCptr(tmpCptr);
	free(tmpCptr);
}

void SYS_getb(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* size=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* file=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.getb",TOOMANYARG,runnable,exe);
	if(!file||!size)
		RAISE_BUILTIN_ERROR("sys.getb",TOOFEWARG,runnable,exe);
	if(!IS_FP(file)||!IS_I32(size))
		RAISE_BUILTIN_ERROR("sys.getb",WRONGARG,runnable,exe);
	FILE* fp=file->u.fp;
	uint8_t* str=(uint8_t*)malloc(sizeof(uint8_t)*GET_I32(size));
	FAKE_ASSERT(str,"B_getb",__FILE__,__LINE__);
	int32_t realRead=0;
	realRead=fread(str,sizeof(uint8_t),GET_I32(size),fp);
	if(!realRead)
	{
		free(str);
		SET_RETURN("SYS_getb",VM_NIL,stack);
	}
	else
	{
		str=(uint8_t*)realloc(str,sizeof(uint8_t)*realRead);
		FAKE_ASSERT(str,"B_getb",__FILE__,__LINE__);
		FklVMvalue* tmpByts=fklNewVMvalue(FKL_BYTS,NULL,exe->heap);
		tmpByts->u.byts=(FklVMByts*)malloc(sizeof(FklVMByts)+GET_I32(size));
		FAKE_ASSERT(tmpByts->u.byts,"SYS_getb",__FILE__,__LINE__);
		tmpByts->u.byts->size=GET_I32(size);
		memcpy(tmpByts->u.byts->str,str,GET_I32(size));
		free(str);
		SET_RETURN("SYS_getb",tmpByts,stack);
	}
}

void SYS_prin1(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* file=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.prin1",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.prin1",TOOFEWARG,runnable,exe);
	if(file&&!IS_FP(file))
		RAISE_BUILTIN_ERROR("sys.prin1",WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp:stdout;
	fklPrin1VMvalue(obj,objFile,NULL);
	SET_RETURN("SYS_prin1",obj,stack);
}

void SYS_putb(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* bt=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* file=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.putb",TOOMANYARG,runnable,exe);
	if(!file||!bt)
		RAISE_BUILTIN_ERROR("sys.putb",TOOFEWARG,runnable,exe);
	if(!IS_FP(file)||!IS_BYTS(bt))
		RAISE_BUILTIN_ERROR("sys.putb",WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp;
	fwrite(bt->u.byts->str,sizeof(uint8_t),bt->u.byts->size,objFile);
	SET_RETURN("SYS_putb",bt,stack);
}

void SYS_princ(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* file=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.princ",TOOMANYARG,runnable,exe);
	if(!obj)
		RAISE_BUILTIN_ERROR("sys.princ",TOOFEWARG,runnable,exe);
	if(file&&!IS_FP(file))
		RAISE_BUILTIN_ERROR("sys.princ",WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp:stdout;
	fklPrincVMvalue(obj,objFile,NULL);
	SET_RETURN("SYS_princ",obj,stack);
}

void SYS_dll(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* dllName=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.dll",TOOMANYARG,runnable,exe);
	if(!dllName)
		RAISE_BUILTIN_ERROR("sys.dll",TOOFEWARG,runnable,exe);
	if(!IS_STR(dllName))
		RAISE_BUILTIN_ERROR("sys.dll",WRONGARG,runnable,exe);
	DllHandle* dll=fklNewVMDll(dllName->u.str);
	if(!dll)
	{
		SET_RETURN("SYS_dll",dllName,stack);
		RAISE_BUILTIN_ERROR("sys.dll",LOADDLLFAILD,runnable,exe);
	}
	SET_RETURN("SYS_dll",fklNewVMvalue(FKL_DLL,dll,exe->heap),stack);
}

void SYS_dlsym(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* dll=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* symbol=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.dlsym",TOOMANYARG,runnable,exe);
	if(!dll||!symbol)
		RAISE_BUILTIN_ERROR("sys.dlsym",TOOFEWARG,runnable,exe);
	if(!IS_STR(symbol)||!IS_DLL(dll))
		RAISE_BUILTIN_ERROR("sys.dlsym",WRONGARG,runnable,exe);
	char prefix[]="FAKE_";
	size_t len=strlen(prefix)+strlen(symbol->u.str)+1;
	char* realDlFuncName=(char*)malloc(sizeof(char)*len);
	FAKE_ASSERT(realDlFuncName,"B_dlsym",__FILE__,__LINE__);
	sprintf(realDlFuncName,"%s%s",prefix,symbol->u.str);
	FklDllFunc funcAddress=fklGetAddress(realDlFuncName,dll->u.dll);
	if(!funcAddress)
	{
		free(realDlFuncName);
		RAISE_BUILTIN_ERROR("sys.dlsym",INVALIDSYMBOL,runnable,exe);
	}
	free(realDlFuncName);
	FklVMDlproc* dlproc=fklNewVMDlproc(funcAddress,dll);
	SET_RETURN("SYS_dlsym",fklNewVMvalue(FKL_DLPROC,dlproc,heap),stack);
}

void SYS_argv(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* retval=NULL;
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.argv",TOOMANYARG,fklTopComStack(exe->rstack),exe);
	retval=VM_NIL;
	FklVMvalue** tmp=&retval;
	int32_t i=0;
	for(;i<exe->argc;i++,tmp=&(*tmp)->u.pair->cdr)
	{
		FklVMvalue* cur=fklNewVMvalue(PAIR,fklNewVMpair(),exe->heap);
		*tmp=cur;
		cur->u.pair->car=fklNewVMvalue(FKL_STR,fklCopyStr(exe->argv[i]),exe->heap);
	}
	SET_RETURN("FAKE_argv",retval,stack);
}

void SYS_go(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	if(exe->VMid==-1)
		RAISE_BUILTIN_ERROR("sys.go",CANTCREATETHREAD,runnable,exe);
	FklVMvalue* threadProc=fklGET_VAL(fklPopVMstack(stack),heap);
	if(!threadProc)
		RAISE_BUILTIN_ERROR("sys.go",TOOFEWARG,runnable,exe);
	if(!IS_PRC(threadProc)&&!IS_DLPROC(threadProc)&&!IS_CONT(threadProc)&&!IS_FLPROC(threadProc))
		RAISE_BUILTIN_ERROR("sys.go",WRONGARG,runnable,exe);
	FklVM* threadVM=(IS_PRC(threadProc)||IS_CONT(threadProc))?fklNewThreadVM(threadProc->u.prc,exe->heap):fklNewThreadDlprocVM(runnable,exe->heap);
	threadVM->lnt=exe->lnt;
	threadVM->code=exe->code;
	threadVM->size=exe->size;
	FklVMstack* threadVMstack=threadVM->stack;
	FklVMvalue* prevBp=MAKE_VM_I32(threadVMstack->bp);
	SET_RETURN("SYS_go",prevBp,threadVMstack);
	threadVMstack->bp=threadVMstack->tp;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklComStack* comStack=fklNewComStack(32);
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
		fklPushComStack(cur,comStack);
	fklResBp(stack);
	while(!fklIsComStackEmpty(comStack))
	{
		FklVMvalue* tmp=fklPopComStack(comStack);
		SET_RETURN("SYS_go",tmp,threadVMstack);
	}
	fklFreeComStack(comStack);
	FklVMvalue* chan=threadVM->chan;
	int32_t faildCode=0;
	if(IS_PRC(threadProc))
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMFunc,threadVM);
	else if(IS_CONT(threadProc))
	{
		fklCreateCallChainWithContinuation(threadVM,threadProc->u.cont);
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMFunc,threadVM);
	}
	else if(IS_DLPROC(threadProc))
	{
		void* a[2]={threadVM,threadProc->u.dlproc->func};
		void** p=(void**)fklCopyMemory(a,sizeof(a));
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMDlprocFunc,p);
	}
	else
	{
		void* a[2]={threadVM,threadProc->u.flproc};
		void** p=(void**)fklCopyMemory(a,sizeof(a));
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMFlprocFunc,p);
	}
	if(faildCode)
	{
		fklDeleteCallChain(threadVM);
		fklFreeVMstack(threadVM->stack);
		threadVM->stack=NULL;
		threadVM->lnt=NULL;
		SET_RETURN("SYS_go",MAKE_VM_I32(faildCode),stack);
	}
	else
		SET_RETURN("SYS_go",chan,stack);
}

void SYS_chanl(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* maxSize=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.chanl",TOOMANYARG,runnable,exe);
	if(!maxSize)
		RAISE_BUILTIN_ERROR("sys.chanl",TOOFEWARG,runnable,exe);
	if(!IS_I32(maxSize))
		RAISE_BUILTIN_ERROR("sys.chanl",WRONGARG,runnable,exe);
	SET_RETURN("SYS_chanl",fklNewVMvalue(FKL_CHAN,fklNewVMChanl(GET_I32(maxSize)),exe->heap),stack);
}

void SYS_send(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* message=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* ch=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.send",TOOMANYARG,runnable,exe);
	if(!message||!ch)
		RAISE_BUILTIN_ERROR("sys.send",TOOFEWARG,runnable,exe);
	if(!IS_CHAN(ch))
		RAISE_BUILTIN_ERROR("sys.send",WRONGARG,runnable,exe);
	FklSendT* t=fklNewSendT(message);
	fklChanlSend(t,ch->u.chan);
	SET_RETURN("SYS_send",message,stack);
}

void SYS_recv(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* ch=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.recv",TOOMANYARG,runnable,exe);
	if(!ch)
		RAISE_BUILTIN_ERROR("sys.recv",TOOFEWARG,runnable,exe);
	if(!IS_CHAN(ch))
		RAISE_BUILTIN_ERROR("sys.recv",WRONGARG,runnable,exe);
	FklRecvT* t=fklNewRecvT(exe);
	fklChanlRecv(t,ch->u.chan);
}

void SYS_error(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* who=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* type=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* message=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.error",TOOMANYARG,runnable,exe);
	if(!type||!message)
		RAISE_BUILTIN_ERROR("sys.error",TOOFEWARG,runnable,exe);
	if(!IS_SYM(type)||!IS_STR(message)||(!IS_SYM(who)&&!IS_STR(who)))
		RAISE_BUILTIN_ERROR("sys.error",WRONGARG,runnable,exe);
	SET_RETURN("SYS_error",fklNewVMvalue(ERR,fklNewVMerrorWithSid((IS_SYM(who))?fklGetGlobSymbolWithId(GET_SYM(who))->symbol:who->u.str,GET_SYM(type),message->u.str),exe->heap),stack);
}

void SYS_raise(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* err=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.raise",TOOMANYARG,runnable,exe);
	if(!err)
		RAISE_BUILTIN_ERROR("sys.raise",TOOFEWARG,runnable,exe);
	if(!IS_ERR(err))
		RAISE_BUILTIN_ERROR("sys.raise",WRONGARG,runnable,exe);
	fklRaiseVMerror(err,exe);
}

void SYS_clcc(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	FklVMvalue* proc=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.clcc",TOOMANYARG,runnable,exe);
	if(!proc)
		RAISE_BUILTIN_ERROR("sys.clcc",TOOFEWARG,runnable,exe);
	if(!IS_PTR(proc)||(proc->type!=FKL_PRC&&proc->type!=FKL_CONT&&proc->type!=FKL_DLPROC))
		RAISE_BUILTIN_ERROR("sys.clcc",INVOKEERROR,runnable,exe);
	FklVMvalue* cc=fklNewVMvalue(FKL_CONT,fklNewVMcontinuation(stack,exe->rstack,exe->tstack),exe->heap);
	SET_RETURN("SYS_clcc",MAKE_VM_I32(stack->bp),stack);
	stack->bp=stack->tp;
	SET_RETURN("SYS_clcc",cc,stack);
	if(proc->type==FKL_PRC)
	{
		FklVMproc* tmpProc=proc->u.prc;
		FklVMrunnable* prevProc=fklHasSameProc(tmpProc->scp,exe->rstack);
		if(fklIsTheLastExpress(runnable,prevProc,exe)&&prevProc)
			prevProc->mark=1;
		else
		{
			FklVMrunnable* tmpRunnable=fklNewVMrunnable(tmpProc);
			tmpRunnable->localenv=fklNewVMenv(tmpProc->prevEnv);
			fklPushComStack(tmpRunnable,exe->rstack);
		}
	}
	else if(proc->type==FKL_CONT)
	{
		VMcontinuation* cc=proc->u.cont;
		fklCreateCallChainWithContinuation(exe,cc);
	}
	else
	{
		FklDllFunc dllfunc=proc->u.dlproc->func;
		dllfunc(exe,gclock);
	}
}

void SYS_apply(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopComStack(exe->rstack);
	VMheap* heap=exe->heap;
	FklVMvalue* proc=fklGET_VAL(fklPopVMstack(stack),heap);
	if(!proc)
		RAISE_BUILTIN_ERROR("sys.apply",TOOFEWARG,runnable,exe);
	if(!IS_PTR(proc)||(proc->type!=FKL_PRC&&proc->type!=FKL_CONT&&proc->type!=FKL_DLPROC&&proc->type!=FLPROC))
		RAISE_BUILTIN_ERROR("b.invoke",INVOKEERROR,runnable,exe);
	FklComStack* stack1=fklNewComStack(32);
	FklVMvalue* value=NULL;
	FklVMvalue* lastList=NULL;
	while((value=fklGET_VAL(fklPopVMstack(stack),heap)))
	{
		if(!fklResBp(stack))
		{
			lastList=value;
			break;
		}
		fklPushComStack(value,stack1);
	}
	FklComStack* stack2=fklNewComStack(32);
	if(!IS_PAIR(lastList)&&lastList!=VM_NIL)
	{
		fklFreeComStack(stack1);
		fklFreeComStack(stack2);
		RAISE_BUILTIN_ERROR("sys.apply",WRONGARG,runnable,exe);
	}
	for(;IS_PAIR(lastList);lastList=fklGetVMpairCdr(lastList))
		fklPushComStack(fklGetVMpairCar(lastList),stack2);
	if(lastList!=VM_NIL)
	{
		fklFreeComStack(stack1);
		fklFreeComStack(stack2);
		RAISE_BUILTIN_ERROR("sys.apply",WRONGARG,runnable,exe);
	}
	SET_RETURN("SYS_apply",MAKE_VM_I32(stack->bp),stack);
	stack->bp=stack->tp;
	while(!fklIsComStackEmpty(stack2))
	{
		FklVMvalue* t=fklPopComStack(stack2);
		SET_RETURN("SYS_apply",t,stack);
	}
	fklFreeComStack(stack2);
	while(!fklIsComStackEmpty(stack1))
	{
		FklVMvalue* t=fklPopComStack(stack1);
		SET_RETURN("SYS_apply",t,stack);
	}
	fklFreeComStack(stack1);
	switch(proc->type)
	{
		case FKL_PRC:
			invokeNativeProcdure(exe,proc->u.prc,runnable);
			break;
		case FKL_CONT:
			invokeContinuation(exe,proc->u.cont);
			break;
		case FKL_DLPROC:
			invokeDlProc(exe,proc->u.dlproc);
			break;
		case FLPROC:
			invokeFlproc(exe,proc->u.flproc);
		default:
			break;
	}
}

void SYS_newf(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopComStack(exe->rstack);
	FklVMvalue* vsize=fklPopVMstack(stack);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.newf",TOOMANYARG,r,exe);
	if(!vsize)
		RAISE_BUILTIN_ERROR("sys.newf",TOOFEWARG,r,exe);
	if(!IS_I32(vsize)&&!IS_I64(vsize))
		RAISE_BUILTIN_ERROR("sys.newf",WRONGARG,r,exe);
	size_t size=IS_I32(vsize)?GET_I32(vsize):*vsize->u.i64;
	uint8_t* mem=(uint8_t*)malloc(size);
	FAKE_ASSERT(mem,"SYS_newf",__FILE__,__LINE__);
	FklVMvalue* retval=MAKE_VM_MEM(fklNewVMMem(0,mem),exe->heap);
	SET_RETURN("SYS_newf",retval,stack);
}

void SYS_delf(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopComStack(exe->rstack);
	FklVMvalue* mem=fklPopVMstack(stack);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.delf",TOOMANYARG,r,exe);
	if(!mem)
		RAISE_BUILTIN_ERROR("sys.delf",TOOFEWARG,r,exe);
	if(!IS_MEM(mem))
		RAISE_BUILTIN_ERROR("sys.delf",WRONGARG,r,exe);
	FklVMMem* pmem=mem->u.chf;
	uint8_t* p=pmem->mem;
	free(p);
	pmem->mem=NULL;
}

void SYS_lfdl(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopComStack(exe->rstack);
	FklVMvalue* vpath=fklPopVMstack(stack);
	if(fklResBp(stack))
		RAISE_BUILTIN_ERROR("sys.lfdl",TOOMANYARG,r,exe);
	if(exe->VMid==-1)
		return;
	if(!vpath)
		RAISE_BUILTIN_ERROR("sys.lfdl",TOOFEWARG,r,exe);
	if(!IS_STR(vpath))
		RAISE_BUILTIN_ERROR("sys.lfdl",WRONGARG,r,exe);
	const char* path=vpath->u.str;
#ifdef _WIN32
	DllHandle handle=LoadLibrary(path);
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
	DllHandle handle=dlopen(path,RTLD_LAZY);
	if(!handle)
	{
		perror(dlerror());
		putc('\n',stderr);
	}
#endif
	FklSharedObjNode* node=(FklSharedObjNode*)malloc(sizeof(FklSharedObjNode));
	FAKE_ASSERT(node,"B_load_shared_obj",__FILE__,__LINE__);
	node->dll=handle;
	pthread_mutex_lock(&GlobSharedObjsMutex);
	node->next=GlobSharedObjs;
	GlobSharedObjs=node;
	pthread_mutex_unlock(&GlobSharedObjsMutex);
}
