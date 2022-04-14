#include<fakeLisp/reader.h>
#include<fakeLisp/symbol.h>
#include<fakeLisp/basicADT.h>
#include<fakeLisp/vm.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<float.h>
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

static inline int isInt(FklVMvalue* p)
{
	return FKL_IS_I32(p)||FKL_IS_I64(p);
}

static inline int64_t getInt(FklVMvalue* p)
{
	return FKL_IS_I32(p)?FKL_GET_I32(p):p->u.i64;
}

static inline FklVMvalue* makeVMint(int64_t r64,FklVMheap* heap)
{
	if(r64>INT32_MAX||r64<INT32_MIN)
		return fklNewVMvalue(FKL_I64,&r64,heap);
	else
		return FKL_MAKE_VM_I32(r64);
}

static inline FklVMmemMode getMode(FklVMvalue* pmode)
{
	if(pmode)
	{
		FklSid_t sid=FKL_GET_SYM(pmode);
		if(!strcmp(fklGetGlobSymbolWithId(sid)->symbol,"atomic"))
			return FKL_MEM_ATOMIC;
		if(!strcmp(fklGetGlobSymbolWithId(sid)->symbol,"raw"))
			return FKL_MEM_RAW;
	}
	return FKL_MEM_RAW;
}

//syscalls

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
	FKL_SET_RETURN(__func__,FKL_MAKE_VM_REF(&obj->u.pair->car),stack);
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
	FKL_SET_RETURN(__func__,FKL_MAKE_VM_REF(&obj->u.pair->cdr),stack);
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
	FKL_SET_RETURN(__func__,pair,stack);
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
		else if(FKL_IS_VECTOR(*prev))
		{
			if(!FKL_IS_VECTOR(cur))
				FKL_RAISE_BUILTIN_ERROR("sys.append",FKL_WRONGARG,runnable,exe);
			fklVMvecCat(&(*prev)->u.vec,cur->u.vec);
		}
		else
			FKL_RAISE_BUILTIN_ERROR("sys.append",FKL_WRONGARG,runnable,exe);
	}
	fklResBp(stack);
	FKL_SET_RETURN(__func__,retval,stack);
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
		FKL_SET_RETURN(__func__,FKL_VM_TRUE,stack);
	else
		FKL_SET_RETURN(__func__,FKL_VM_NIL,stack);
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
		FKL_SET_RETURN(__func__,FKL_VM_TRUE,stack);
	else
		FKL_SET_RETURN(__func__,FKL_VM_NIL,stack);
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
	FKL_SET_RETURN(__func__,fir==sec
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
	if((FKL_IS_F64(fir)||isInt(fir))&&(FKL_IS_F64(sec)||isInt(sec)))
		FKL_SET_RETURN(__func__
				,fabs((FKL_IS_F64(fir)?fir->u.f64:getInt(fir))
						-(FKL_IS_F64(sec)?sec->u.f64:getInt(sec)))
				<DBL_EPSILON
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,stack);
	else if(FKL_IS_CHR(fir)&&FKL_IS_CHR(sec))
		FKL_SET_RETURN(__func__,fir==sec
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,stack);
	else if(FKL_IS_SYM(fir)&&FKL_IS_SYM(sec))
		FKL_SET_RETURN(__func__,fir==sec
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,stack);
	else if(FKL_IS_STR(fir)&&FKL_IS_STR(sec))
		FKL_SET_RETURN(__func__,(!strcmp(fir->u.str,sec->u.str))
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,stack);
	else if(FKL_IS_BYTS(fir)&&FKL_IS_BYTS(sec))
		FKL_SET_RETURN(__func__,fklEqVMbyts(fir->u.byts,sec->u.byts)
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
	FKL_SET_RETURN(__func__,(fklVMvaluecmp(fir,sec))
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
		if(isInt(cur))
			r64+=getInt(cur);
		else if(FKL_IS_F64(cur))
			rd+=cur->u.f64;
		else
			FKL_RAISE_BUILTIN_ERROR("sys.add",FKL_WRONGARG,runnable,exe);
	}
	fklResBp(stack);
	if(rd!=0.0)
	{
		rd+=r64;
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
	}
	else
		FKL_SET_RETURN(__func__,makeVMint(r64,exe->heap),stack);
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
	if(!FKL_IS_F64(arg)&&!isInt(arg))
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64+1.0;
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&r,exe->heap),stack);
	}
	else
		FKL_SET_RETURN(__func__,makeVMint(getInt(arg)+1,exe->heap),stack);
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
	if(!FKL_IS_F64(prev)&&!isInt(prev))
		FKL_RAISE_BUILTIN_ERROR("sys.sub",FKL_WRONGARG,runnable,exe);
	if(!cur)
	{
		fklResBp(stack);
		if(FKL_IS_F64(prev))
		{
			rd=-prev->u.f64;
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else
			FKL_SET_RETURN(__func__,makeVMint(-getInt(prev),exe->heap),stack);
	}
	else
	{
		for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
		{
			if(isInt(cur))
				r64+=getInt(cur);
			else if(FKL_IS_F64(cur))
				rd+=cur->u.f64;
			else
				FKL_RAISE_BUILTIN_ERROR("sys.sub",FKL_WRONGARG,runnable,exe);
		}
		fklResBp(stack);
		if(FKL_IS_F64(prev)||rd!=0.0)
		{
			rd=((FKL_IS_F64(prev))?prev->u.f64:((FKL_IS_I64(prev))?prev->u.i64:FKL_GET_I32(prev)))-rd-r64;
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else
		{
			r64=(FKL_IS_I64(prev))?prev->u.i64:FKL_GET_I32(prev)-r64;
			FKL_SET_RETURN(__func__,makeVMint(r64,exe->heap),stack);
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
	if(!FKL_IS_F64(arg)&&!isInt(arg))
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64-1.0;
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&r,exe->heap),stack);
	}
	else
		FKL_SET_RETURN(__func__,makeVMint(getInt(arg)-1,exe->heap),stack);
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
		if(isInt(cur))
			r64*=getInt(cur);
		else if(FKL_IS_F64(cur))
			rd*=cur->u.f64;
		else
			FKL_RAISE_BUILTIN_ERROR("sys.mul",FKL_WRONGARG,runnable,exe);
	}
	fklResBp(stack);
	if(rd!=1.0)
	{
		rd*=r64;
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
	}
	else
		FKL_SET_RETURN(__func__,makeVMint(r64,exe->heap),stack);
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
	if(!FKL_IS_F64(prev)&&!isInt(prev))
		FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_WRONGARG,runnable,exe);
	if(!cur)
	{
		fklResBp(stack);
		if(FKL_IS_F64(prev))
		{
			if(prev->u.f64==0.0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			rd=1/prev->u.f64;
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else
		{
			r64=getInt(prev);
			if(!r64)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			if(1%r64)
			{
				rd=1.0/r64;
				FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
			}
			else
			{
				r64=1/r64;
				FKL_SET_RETURN(__func__,makeVMint(r64,exe->heap),stack);
			}
		}
	}
	else
	{
		for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
		{
			if(isInt(cur))
				r64*=getInt(cur);
			else if(FKL_IS_F64(cur))
				rd*=cur->u.f64;
			else
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_WRONGARG,runnable,exe);
		}
		if((r64)==0)
			FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
		fklResBp(stack);
		if(FKL_IS_F64(prev)||rd!=1.0||(isInt(prev)&&getInt(prev)%(r64)))
		{
			if(rd==0.0||r64==0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			rd=((double)getInt(prev))/rd/r64;
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else
		{
			if(r64==0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			else
			{
				r64=getInt(prev)/r64;
				FKL_SET_RETURN(__func__,makeVMint(r64,exe->heap),stack);
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
	if(!((FKL_IS_F64(fir)||isInt(fir))&&(FKL_IS_F64(sec)||isInt(sec))))
		FKL_RAISE_BUILTIN_ERROR("sys.rem",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(fir)||FKL_IS_F64(sec))
	{
		double af=(FKL_IS_F64(fir))?fir->u.f64:getInt(fir);
		double as=(FKL_IS_F64(sec))?sec->u.f64:getInt(sec);
		if(as==0.0)
			FKL_RAISE_BUILTIN_ERROR("sys.rem",FKL_DIVZERROERROR,runnable,exe);
		double r=fmod(af,as);
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&r,exe->heap),stack);
	}
	else
	{
		int64_t r=getInt(fir)%getInt(sec);
		FKL_SET_RETURN(__func__,makeVMint(r,exe->heap),stack);
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
			if((FKL_IS_F64(prev)||isInt(prev))
					&&(FKL_IS_F64(cur)||isInt(cur)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:getInt(prev))
							-((FKL_IS_F64(cur))?cur->u.f64
								:getInt(cur)))
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
	FKL_SET_RETURN(__func__,r
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
			if((FKL_IS_F64(prev)||isInt(prev))
					&&(FKL_IS_F64(cur)||isInt(cur)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:getInt(prev))
							-((FKL_IS_F64(cur))?cur->u.f64
								:getInt(cur)))
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
	FKL_SET_RETURN(__func__,r
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
			if((FKL_IS_F64(prev)||isInt(prev))
					&&(FKL_IS_F64(cur)||isInt(cur)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:getInt(prev))
							-((FKL_IS_F64(cur))?cur->u.f64
								:getInt(cur)))
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
	FKL_SET_RETURN(__func__,r
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
			if((FKL_IS_F64(prev)||isInt(prev))
					&&(FKL_IS_F64(cur)||isInt(prev)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:getInt(prev))
							-((FKL_IS_F64(cur))?cur->u.f64
								:getInt(cur)))
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
	FKL_SET_RETURN(__func__,r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_char(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_I32(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_CHR(FKL_GET_I32(obj)),stack);
	else if(FKL_IS_CHR(obj))
		FKL_SET_RETURN(__func__,obj,stack);
	else if(FKL_IS_F64(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_CHR((int32_t)obj->u.f64),stack);
	else if(FKL_IS_STR(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_CHR(obj->u.str[0]),stack);
	else if(FKL_IS_BYTS(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_CHR(obj->u.byts->str[0]),stack);
	else
		FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_WRONGARG,runnable,exe);
}

void SYS_integer(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.integer",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.integer",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(FKL_GET_CHR(obj)),stack);
	else if(FKL_IS_I32(obj))
		FKL_SET_RETURN(__func__,obj,stack);
	else if(FKL_IS_SYM(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(FKL_GET_SYM(obj)),stack);
	else if(FKL_IS_I64(obj))
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,&obj->u.i64,exe->heap),stack);
	else if(FKL_IS_F64(obj))
	{
		int64_t r=(int64_t)obj->u.f64;
		FKL_SET_RETURN(__func__,makeVMint(r,exe->heap),stack);
	}
	else if(FKL_IS_STR(obj))
	{
		int64_t r=fklStringToInt(obj->u.str);
		FKL_SET_RETURN(__func__,makeVMint(r,exe->heap),stack);
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
			FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(*(int32_t*)r),stack);
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
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,r,exe->heap),stack);
		}
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.integer",FKL_WRONGARG,runnable,exe);
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
	if(FKL_IS_CHR(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(FKL_GET_CHR(obj)),stack);
	else if(FKL_IS_I32(obj))
		FKL_SET_RETURN(__func__,obj,stack);
	else if(FKL_IS_SYM(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(FKL_GET_SYM(obj)),stack);
	else if(FKL_IS_I64(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32((int32_t)obj->u.i64),stack);
	else if(FKL_IS_F64(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32((int32_t)obj->u.f64),stack);
	else if(FKL_IS_STR(obj))
	{
		int64_t r=fklStringToInt(obj->u.str);
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32((int32_t)r),stack);
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
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(*(int32_t*)r),stack);
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
	if(FKL_IS_CHR(obj))
	{
		int8_t r=FKL_GET_CHR(obj);
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else if(FKL_IS_I32(obj))
	{
		int32_t r=FKL_GET_I32(obj);
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else if(FKL_IS_SYM(obj))
	{
		int64_t r=FKL_GET_SYM(obj);
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else if(FKL_IS_I64(obj))
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,&obj->u.i64,exe->heap),stack);
	else if(FKL_IS_F64(obj))
	{
		int64_t r=(int64_t)obj->u.f64;
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else if(FKL_IS_STR(obj))
	{
		int64_t r=fklStringToInt(obj->u.str);
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
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
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,r,exe->heap),stack);
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
	if(FKL_IS_CHR(obj))
		retval->u.f64=(double)FKL_GET_CHR(obj);
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
	FKL_SET_RETURN(__func__,retval,stack);
}

void SYS_string(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNewVMvalue(FKL_STR,NULL,exe->heap);
	if(FKL_IS_STR(obj)||FKL_IS_VECTOR(obj))
	{
		FklVMvalue* pstart=fklGET_VAL(fklPopVMstack(stack),exe->heap);
		FklVMvalue* psize=fklGET_VAL(fklPopVMstack(stack),exe->heap);
		if(fklResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOMANYARG,runnable,exe);
		if(pstart)
		{
			if(!isInt(pstart))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			int64_t start=getInt(pstart);
			if((FKL_IS_STR(obj)&&start>=strlen(obj->u.str))
					||(FKL_IS_VECTOR(obj)&&start>=obj->u.vec->size))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_INVALIDACCESS,runnable,exe);
			int64_t size=FKL_IS_STR(obj)?strlen(obj->u.str)-start:obj->u.vec->size-start;
			if(psize)
			{
				if(!isInt(psize))
					FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
				int64_t tsize=getInt(psize);
				if(tsize>size)
					FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_INVALIDACCESS,runnable,exe);
				size=tsize;
			}
			if(FKL_IS_STR(obj))
			{
				retval->u.str=fklCopyMemory(obj->u.str+start,size+1);
				retval->u.str[size]='\0';
			}
			else
			{ retval->u.str=(char*)malloc(sizeof(char)*(size+1));
				for(size_t i=0;i<size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[start+i];
					if(!isInt(v))
						FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
					retval->u.str[i]=getInt(v);
				}
				retval->u.str[size]='\0';
			}
		}
		else
		{
			if(FKL_IS_STR(obj))
				retval->u.str=fklCopyStr(obj->u.str);
			else
			{
				retval->u.str=(char*)malloc(sizeof(char)*(obj->u.vec->size+1));
				FKL_ASSERT(retval->u.str,__func__);
				for(size_t i=0;i<obj->u.vec->size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[i];
					if(!isInt(v))
						FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
					retval->u.str[i]=getInt(v);
				}
				retval->u.str[obj->u.vec->size]='\0';
			}
		}
	}
	else if(FKL_IS_CHR(obj))
	{
		retval->u.str=fklCopyStr((char[]){FKL_GET_CHR(obj),'\0'});
		obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
		for(;obj;obj=fklGET_VAL(fklPopVMstack(stack),exe->heap))
		{
			if(!FKL_IS_CHR(obj))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			retval->u.str=fklStrCat(retval->u.str,(char[]){FKL_GET_CHR(obj),'\0'});
		}
		fklResBp(stack);
	}
	else if(isInt(obj))
	{
		FklVMvalue* content=fklGET_VAL(fklPopVMstack(stack),exe->heap);
		fklResBp(stack);
		if(content)
		{
			if(!FKL_IS_CHR(content))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			size_t size=getInt(obj);
			retval->u.str=(char*)malloc(sizeof(char)*(size+1));
			FKL_ASSERT(retval->u.str,__func__);
			retval->u.str[size]='\0';
			memset(retval->u.str,FKL_GET_CHR(content),size);
		}
		else
			retval->u.str=fklIntToString(getInt(obj));
	}
	else
	{
		if(fklResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOMANYARG,runnable,exe);
		else if(FKL_IS_F64(obj))
			retval->u.str=fklDoubleToString(obj->u.f64);
		else if(FKL_IS_SYM(obj))
			retval->u.str=fklCopyStr(fklGetGlobSymbolWithId(FKL_GET_SYM(obj))->symbol);
		else if(FKL_IS_BYTS(obj))
			retval->u.str=fklVMbytsToCstr(obj->u.byts);
		else
			FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
	}
	FKL_SET_RETURN(__func__,retval,stack);
}

void SYS_symbol(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.symbol",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.symbol",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_SYM(fklAddSymbolToGlob(((char[]){FKL_GET_CHR(obj),'\0'}))->id),stack);
	else if(FKL_IS_SYM(obj))
		FKL_SET_RETURN(__func__,obj,stack);
	else if(FKL_IS_STR(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_SYM(fklAddSymbolToGlob(obj->u.str)->id),stack);
	else
		FKL_RAISE_BUILTIN_ERROR("sys.symbol",FKL_WRONGARG,runnable,exe);
}

void SYS_byts(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNewVMvalue(FKL_BYTS,fklNewEmptyVMbyts(),exe->heap);
	if(FKL_IS_BYTS(obj)||FKL_IS_VECTOR(obj))
	{
		FklVMvalue* pstart=fklGET_VAL(fklPopVMstack(stack),exe->heap);
		FklVMvalue* psize=fklGET_VAL(fklPopVMstack(stack),exe->heap);
		if(fklResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_TOOMANYARG,runnable,exe);
		if(pstart)
		{
			if(!isInt(pstart))
				FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_WRONGARG,runnable,exe);
			int64_t start=getInt(pstart);
			if((FKL_IS_BYTS(obj)&&start>=obj->u.byts->size)
					||(FKL_IS_VECTOR(obj)&&start>=obj->u.vec->size))
				FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_INVALIDACCESS,runnable,exe);
			int64_t size=FKL_IS_BYTS(obj)?obj->u.byts->size-start:obj->u.vec->size-start;
			if(psize)
			{
				if(!isInt(psize))
					FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_WRONGARG,runnable,exe);
				int64_t tsize=getInt(psize);
				if(tsize>size)
					FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_INVALIDACCESS,runnable,exe);
				size=tsize;
			}
			FklVMbyts* retvalbyts=NULL;
			if(FKL_IS_BYTS(obj))
				retvalbyts=fklNewVMbyts(size,obj->u.byts->str+start);
			else
			{
				retvalbyts=fklNewVMbyts(size,NULL);
				for(size_t i=0;i<size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[start+i];
					if(!isInt(v))
						FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_WRONGARG,runnable,exe);
					retvalbyts->str[i]=getInt(v);
				}
			}
			free(retval->u.byts);
			retval->u.byts=retvalbyts;
		}
		else
		{
			if(FKL_IS_BYTS(obj))
				fklVMbytsCat(&retval->u.byts,obj->u.byts);
			else
			{
				FklVMbyts* retvalbyts=fklNewVMbyts(obj->u.vec->size,NULL);
				for(size_t i=0;i<obj->u.vec->size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[i];
					if(!isInt(v))
						FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_WRONGARG,runnable,exe);
					retvalbyts->str[i]=getInt(v);
				}
				free(retval->u.byts);
				retval->u.byts=retvalbyts;
			}
		}
	}
	else if(FKL_IS_CHR(obj))
	{
		retval->u.byts->size=0;
		for(size_t i=0;obj;i++,obj=fklGET_VAL(fklPopVMstack(stack),exe->heap))
		{
			if(!FKL_IS_CHR(obj))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			retval->u.byts->size++;
			retval->u.byts=(FklVMbyts*)realloc(retval->u.byts
					,sizeof(FklVMbyts)+sizeof(char)*(i+1));
			FKL_ASSERT(retval->u.byts,"SYS_byts");
			((char*)retval->u.byts->str)[i]=FKL_GET_CHR(obj);
		}
		fklResBp(stack);
	}
	else if(isInt(obj))
	{
		FklVMvalue* content=fklGET_VAL(fklPopVMstack(stack),exe->heap);
		fklResBp(stack);
		if(content)
		{
			if(!FKL_IS_CHR(content))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			size_t size=getInt(obj);
			retval->u.byts=(FklVMbyts*)realloc(retval->u.byts
					,sizeof(FklVMbyts)+sizeof(char)*size);
			retval->u.byts->size=size;
			FKL_ASSERT(retval->u.str,__func__);
			memset(retval->u.byts->str,FKL_GET_CHR(content),size);
		}
		else
		{
			if(FKL_IS_I32(obj))
			{
				retval->u.byts=(FklVMbyts*)realloc(retval->u.byts
						,sizeof(FklVMbyts)+sizeof(int32_t));
				FKL_ASSERT(retval->u.byts,"SYS_byts");
				retval->u.byts->size=sizeof(int32_t);
				*(int32_t*)retval->u.byts->str=FKL_GET_I32(obj);
			}
			else if(FKL_IS_I64(obj))
			{
				retval->u.byts=(FklVMbyts*)realloc(retval->u.byts
						,sizeof(FklVMbyts)+sizeof(int64_t));
				FKL_ASSERT(retval->u.byts,"SYS_byts");
				retval->u.byts->size=sizeof(int64_t);
				*(int64_t*)retval->u.byts->str=obj->u.i64;
			}
		}
	}
	else
	{
		if(fklResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_TOOMANYARG,runnable,exe);
		else if(FKL_IS_F64(obj))
		{
			retval->u.byts=(FklVMbyts*)realloc(retval->u.byts,sizeof(FklVMbyts)+sizeof(double));
			FKL_ASSERT(retval->u.byts,"SYS_byts");
			retval->u.byts->size=sizeof(double);
			*(double*)retval->u.byts->str=obj->u.f64;
		}
		else if(FKL_IS_SYM(obj))
		{
			retval->u.byts=(FklVMbyts*)realloc(retval->u.byts,sizeof(FklVMbyts)+sizeof(FklSid_t));
			retval->u.byts->size=sizeof(FklSid_t);
			FklSid_t sid=FKL_GET_SYM(obj);
			memcpy(retval->u.byts->str,&sid,sizeof(FklSid_t));
		}
		else if(FKL_IS_STR(obj))
		{
			retval->u.byts=(FklVMbyts*)realloc(retval->u.byts,sizeof(FklVMbyts)+strlen(obj->u.str)+1);
			FKL_ASSERT(retval->u.byts,"SYS_byts");
			retval->u.byts->size=strlen(obj->u.str);
			memcpy(retval->u.byts->str,obj->u.str,retval->u.byts->size);
		}
		else
			FKL_RAISE_BUILTIN_ERROR("sys.byts",FKL_WRONGARG,runnable,exe);
	}
	FKL_SET_RETURN(__func__,retval,stack);
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
	if(!isInt(place))
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_WRONGARG,runnable,exe);
	FklVMvalue* retval=NULL;
	ssize_t index=getInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_INVALIDACCESS,runnable,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		retval=(FKL_IS_PAIR(objPair))?FKL_MAKE_VM_REF(&objPair->u.pair->car):FKL_VM_NIL;
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN(__func__,retval,stack);
}

void SYS_vref(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* vector=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* place=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_TOOMANYARG,runnable,exe);
	if(!place||!vector)
		FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_TOOFEWARG,runnable,exe);
	if(!isInt(place)||(!FKL_IS_VECTOR(vector)&&!FKL_IS_STR(vector)&&!FKL_IS_BYTS(vector)))
		FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_WRONGARG,runnable,exe);
	FklVMvalue* retval=NULL;
	ssize_t index=getInt(place);
	size_t size=FKL_IS_STR(vector)?strlen(vector->u.str):
		FKL_IS_BYTS(vector)?vector->u.byts->size:
		vector->u.vec->size;
	if(index<0||index>=size)
		FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_INVALIDACCESS,runnable,exe);
	if(FKL_IS_STR(vector))
	{
		if(index>=strlen(vector->u.str))
			FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_INVALIDACCESS,runnable,exe);
		retval=FKL_MAKE_VM_CHF(fklNewVMmem(fklGetCharTypeId(),FKL_MEM_RAW,(uint8_t*)vector->u.str+index),heap);
	}
	else if(FKL_IS_BYTS(vector))
		retval=FKL_MAKE_VM_CHF(fklNewVMmem(fklGetCharTypeId(),FKL_MEM_RAW,vector->u.byts->str+index),heap);
	else if(FKL_IS_VECTOR(vector))
		retval=FKL_MAKE_VM_REF(&vector->u.vec->base[index]);
	FKL_SET_RETURN(__func__,retval,stack);
}

void SYS_nthcdr(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* place=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* objlist=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.nthcdr",FKL_TOOMANYARG,runnable,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR("sys.nthcdr",FKL_TOOFEWARG,runnable,exe);
	if(!isInt(place))
		FKL_RAISE_BUILTIN_ERROR("sys.nthcdr",FKL_WRONGARG,runnable,exe);
	FklVMvalue* retval=NULL;
	ssize_t index=getInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR("sys.nthcdr",FKL_INVALIDACCESS,runnable,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		retval=(FKL_IS_PAIR(objPair))?FKL_MAKE_VM_REF(&objPair->u.pair->cdr):FKL_VM_NIL;
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.nthcdr",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN(__func__,retval,stack);
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
	int64_t len=0;
	if(obj==FKL_VM_NIL||FKL_IS_PAIR(obj))
		for(;FKL_IS_PAIR(obj);obj=fklGetVMpairCdr(obj))len++;
	else if(FKL_IS_STR(obj))
		len=strlen(obj->u.str);
	else if(FKL_IS_BYTS(obj))
		len=obj->u.byts->size;
	else if(FKL_IS_CHAN(obj))
		len=fklGetNumVMchanl(obj->u.chan);
	else if(FKL_IS_VECTOR(obj))
		len=obj->u.vec->size;
	else
		FKL_RAISE_BUILTIN_ERROR("sys.length",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN(__func__,makeVMint(len,exe->heap),stack);
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
		FKL_SET_RETURN(__func__,filename,stack);
		FKL_RAISE_BUILTIN_ERROR("sys.fopen",FKL_FILEFAILURE,runnable,exe);
	}
	else
		obj=fklNewVMvalue(FKL_FP,fklNewVMfp(file),heap);
	FKL_SET_RETURN(__func__,obj,stack);
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
	if(fp->u.fp==NULL||fklFreeVMfp(fp->u.fp))
		FKL_RAISE_BUILTIN_ERROR("sys.fclose",FKL_INVALIDACCESS,runnable,exe);
	fp->u.fp=NULL;
	FKL_SET_RETURN(__func__,FKL_VM_NIL,stack);
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
	FklVMfp* tmpFile=NULL;
	FklPtrStack* tokenStack=fklNewPtrStack(32,16);
	if(!stream||FKL_IS_FP(stream))
	{
		tmpFile=stream?stream->u.fp:fklGetVMstdin()->u.fp;
		int unexpectEOF=0;
		size_t size=0;
		tmpString=fklReadInStringPattern(tmpFile->fp,(char**)&tmpFile->prev,&size,&tmpFile->size,0,&unexpectEOF,tokenStack,NULL);
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
		size_t sizes[]={strlen(tmpString)};
		uint32_t line=0;
		FklPtrStack* matchStateStack=fklNewPtrStack(32,16);
		fklSplitStringPartsIntoToken(parts,sizes,1,&line,tokenStack,matchStateStack,NULL,NULL);
		while(!fklIsPtrStackEmpty(matchStateStack))
			free(fklPopPtrStack(matchStateStack));
		fklFreePtrStack(matchStateStack);
	}
	FklAstCptr* tmpCptr=fklCreateAstWithTokens(tokenStack,NULL,NULL);
	//FklAstCptr* tmpCptr=fklBaseCreateTree(tmpString,tmpIntpr);
	FklVMvalue* tmp=NULL;
	if(tmpCptr==NULL)
		tmp=FKL_VM_NIL;
	else
		tmp=fklCastCptrVMvalue(tmpCptr,exe->heap);
	while(!fklIsPtrStackEmpty(tokenStack))
		fklFreeToken(fklPopPtrStack(tokenStack));
	fklFreePtrStack(tokenStack);
	FKL_SET_RETURN(__func__,tmp,stack);
	free(tmpString);
	fklDeleteCptr(tmpCptr);
	free(tmpCptr);
}

void SYS_fgetb(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* psize=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* file=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.fgetb",FKL_TOOMANYARG,runnable,exe);
	if(!file||!psize)
		FKL_RAISE_BUILTIN_ERROR("sys.fgetb",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(file)||!isInt(psize))
		FKL_RAISE_BUILTIN_ERROR("sys.fgetb",FKL_WRONGARG,runnable,exe);
	FklVMfp* fp=file->u.fp;
	size_t size=getInt(psize);
	uint8_t* str=(uint8_t*)malloc(sizeof(uint8_t)*size);
	FKL_ASSERT(str,"B_fgetb");
	int32_t realRead=0;
	if(fp->size)
	{
		if(fp->size<=size)
		{
			memcpy(str,fp->prev,fp->size);
			realRead+=fp->size;
			size-=fp->size;
			free(fp->prev);
			fp->prev=NULL;
			fp->size=0;
		}
		else
		{
			fp->size-=size;
			memcpy(str,fp->prev,size);
			realRead+=size;
			uint8_t* prev=fp->prev;
			fp->prev=fklCopyMemory(prev+size,sizeof(uint8_t)*fp->size);
			free(prev);
			size=0;
		}
	}
	if(size)
		realRead+=fread(str,sizeof(uint8_t),size,fp->fp);
	if(!realRead)
	{
		free(str);
		FKL_SET_RETURN(__func__,FKL_VM_NIL,stack);
	}
	else
	{
		str=(uint8_t*)realloc(str,sizeof(uint8_t)*realRead);
		FKL_ASSERT(str,"B_fgetb");
		FklVMvalue* tmpByts=fklNewVMvalue(FKL_BYTS,NULL,exe->heap);
		tmpByts->u.byts=(FklVMbyts*)malloc(sizeof(FklVMbyts)+getInt(psize));
		FKL_ASSERT(tmpByts->u.byts,"SYS_fgetb");
		tmpByts->u.byts->size=getInt(psize);
		memcpy(tmpByts->u.byts->str,str,getInt(psize));
		free(str);
		FKL_SET_RETURN(__func__,tmpByts,stack);
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
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrin1VMvalue(obj,objFile);
	FKL_SET_RETURN(__func__,obj,stack);
}

void SYS_fputb(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* bt=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* file=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.fputb",FKL_TOOMANYARG,runnable,exe);
	if(!file||!bt)
		FKL_RAISE_BUILTIN_ERROR("sys.fputb",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(file)||!FKL_IS_BYTS(bt))
		FKL_RAISE_BUILTIN_ERROR("sys.fputb",FKL_WRONGARG,runnable,exe);
	FILE* objFile=file->u.fp->fp;
	fwrite(bt->u.byts->str,sizeof(uint8_t),bt->u.byts->size,objFile);
	FKL_SET_RETURN(__func__,bt,stack);
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
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrincVMvalue(obj,objFile);
	FKL_SET_RETURN(__func__,obj,stack);
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
		FKL_SET_RETURN(__func__,dllName,stack);
		FKL_RAISE_BUILTIN_ERROR("sys.dll",FKL_LOADDLLFAILD,runnable,exe);
	}
	FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_DLL,dll,exe->heap),stack);
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
	FKL_ASSERT(realDlFuncName,"B_dlsym");
	sprintf(realDlFuncName,"%s%s",prefix,symbol->u.str);
	FklVMdllFunc funcAddress=fklGetAddress(realDlFuncName,dll->u.dll);
	if(!funcAddress)
	{
		free(realDlFuncName);
		FKL_RAISE_BUILTIN_ERROR("sys.dlsym",FKL_INVALIDSYMBOL,runnable,exe);
	}
	free(realDlFuncName);
	FklVMdlproc* dlproc=fklNewVMdlproc(funcAddress,dll);
	FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_DLPROC,dlproc,heap),stack);
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
	FKL_SET_RETURN(__func__,retval,stack);
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
	FKL_SET_RETURN(__func__,prevBp,threadVMstack);
	threadVMstack->bp=threadVMstack->tp;
	FklVMvalue* cur=fklGET_VAL(fklPopVMstack(stack),heap);
	FklPtrStack* comStack=fklNewPtrStack(32,16);
	for(;cur;cur=fklGET_VAL(fklPopVMstack(stack),heap))
		fklPushPtrStack(cur,comStack);
	fklResBp(stack);
	while(!fklIsPtrStackEmpty(comStack))
	{
		FklVMvalue* tmp=fklPopPtrStack(comStack);
		FKL_SET_RETURN(__func__,tmp,threadVMstack);
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
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(faildCode),stack);
	}
	else
		FKL_SET_RETURN(__func__,chan,stack);
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
	if(!isInt(maxSize))
		FKL_RAISE_BUILTIN_ERROR("sys.chanl",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_CHAN,fklNewVMchanl(getInt(maxSize)),exe->heap),stack);
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
	FKL_SET_RETURN(__func__,ch,stack);
	fklChanlSend(t,ch->u.chan,gclock);
	fklPopVMstack(stack);
	FKL_SET_RETURN(__func__,message,stack);
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
	FKL_SET_RETURN(__func__,ch,stack);
	fklChanlRecv(t,ch->u.chan,gclock);
	fklPopVMstack(stack);
	FKL_SET_RETURN(__func__,t->v,stack);
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
	FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_ERR,fklNewVMerrorWithSid((FKL_IS_SYM(who))?fklGetGlobSymbolWithId(FKL_GET_SYM(who))->symbol:who->u.str,FKL_GET_SYM(type),message->u.str),exe->heap),stack);
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
	FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(stack->bp),stack);
	stack->bp=stack->tp;
	FKL_SET_RETURN(__func__,cc,stack);
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
	FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(stack->bp),stack);
	stack->bp=stack->tp;
	while(!fklIsPtrStackEmpty(stack2))
	{
		FklVMvalue* t=fklPopPtrStack(stack2);
		FKL_SET_RETURN(__func__,t,stack);
	}
	fklFreePtrStack(stack2);
	while(!fklIsPtrStackEmpty(stack1))
	{
		FklVMvalue* t=fklPopPtrStack(stack1);
		FKL_SET_RETURN(__func__,t,stack);
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
	FklVMheap* heap=exe->heap;
	FklVMvalue* vsize=fklGET_VAL(fklPopVMstack(stack),heap);
	FklVMvalue* pmode=fklGET_VAL(fklPopVMstack(stack),heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.newf",FKL_TOOMANYARG,r,exe);
	if(!vsize)
		FKL_RAISE_BUILTIN_ERROR("sys.newf",FKL_TOOFEWARG,r,exe);
	if(!isInt(vsize))
		FKL_RAISE_BUILTIN_ERROR("sys.newf",FKL_WRONGARG,r,exe);
	if(pmode&&!FKL_IS_SYM(pmode))
		FKL_RAISE_BUILTIN_ERROR("sys.newf",FKL_WRONGARG,r,exe);
	size_t size=getInt(vsize);
	uint8_t* mem=(uint8_t*)malloc(size);
	FKL_ASSERT(mem,"SYS_newf");
	FklVMvalue* retval=FKL_MAKE_VM_MEM(fklNewVMmem(0,getMode(pmode),mem),exe->heap);
	FKL_SET_RETURN(__func__,retval,stack);
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
	pmem->mode=FKL_MEM_RAW;
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
	FKL_ASSERT(node,"B_load_shared_obj");
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
	FKL_SET_RETURN(__func__,retval,stack);
}

void SYS_feof(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fp=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(fklResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_TOOMANYARG,runnable,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(fp))
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_WRONGARG,runnable,exe);
	if(fp->u.fp==NULL)
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_INVALIDACCESS,runnable,exe);
	FKL_SET_RETURN(__func__,!fp->u.fp->size&&feof(fp->u.fp->fp)?FKL_VM_TRUE:FKL_VM_NIL,stack);
}

void SYS_vector(FklVM* exe,pthread_rwlock_t* gclock)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fir=fklGET_VAL(fklPopVMstack(stack),exe->heap);
	if(!fir)
		FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_STR(fir)||FKL_IS_BYTS(fir))
	{
		if(fklResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_TOOMANYARG,runnable,exe);
		size_t size=FKL_IS_STR(fir)?strlen(fir->u.str):fir->u.byts->size;
		FklVMvec* vec=fklNewVMvec(size,NULL);
		if(FKL_IS_STR(fir))
		{
			for(size_t i=0;i<size;i++)
				vec->base[i]=FKL_MAKE_VM_CHR(fir->u.str[i]);
		}
		else
		{
			for(size_t i=0;i<size;i++)
				vec->base[i]=FKL_MAKE_VM_I32(fir->u.byts->str[i]);
		}
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_VECTOR,vec,exe->heap),stack);
	}
	else if(isInt(fir))
	{
		size_t size=getInt(fir);
		if(!size)
		{
			FklPtrStack* vstack=fklNewPtrStack(32,16);
			for(FklVMvalue* content=fklGET_VAL(fklPopVMstack(stack),exe->heap);content;content=fklGET_VAL(fklPopVMstack(stack),exe->heap))
				fklPushPtrStack(content,vstack);
			fklResBp(stack);
			FklVMvec* vec=fklNewVMvec(vstack->top,(FklVMvalue**)vstack->base);
			fklFreePtrStack(vstack);
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_VECTOR,vec,exe->heap),stack);
		}
		else
		{
			FklVMvalue* content=fklGET_VAL(fklPopVMstack(stack),exe->heap);
			FklVMvec* vec=fklNewVMvec(size,NULL);
			for(size_t i=0;i<size;i++)
				vec->base[i]=FKL_VM_NIL;
			if(content)
			{
				for(size_t i=0;i<size;i++)
					vec->base[i]=content;
			}
			fklResBp(stack);
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_VECTOR,vec,exe->heap),stack);
		}
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_WRONGARG,runnable,exe);
}

#define PREDICATE(condtion,err_infor) {\
	FklVMstack* stack=exe->stack;\
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);\
	FklVMvalue* val=fklGET_VAL(fklPopVMstack(stack),exe->heap);\
	if(fklResBp(stack))\
		FKL_RAISE_BUILTIN_ERROR(err_infor,FKL_TOOFEWARG,runnable,exe);\
	if(!val)\
		FKL_RAISE_BUILTIN_ERROR(err_infor,FKL_TOOFEWARG,runnable,exe);\
	if(condtion)\
		FKL_SET_RETURN(__func__,FKL_VM_TRUE,stack);\
	else\
		FKL_SET_RETURN(__func__,FKL_VM_NIL,stack);\
}

#define ARGL FklVM* exe,pthread_rwlock_t* gclock
void SYS_atom(ARGL) PREDICATE(!FKL_IS_PAIR(val),"sys.atom?")
void SYS_char_p(ARGL) PREDICATE(FKL_IS_CHR(val),"sys.char?")
void SYS_integer_p(ARGL) PREDICATE(FKL_IS_I32(val)||FKL_IS_I64(val),"sys.integer?")
void SYS_i32_p(ARGL) PREDICATE(FKL_IS_I32(val),"sys.i32?")
void SYS_i64_p(ARGL) PREDICATE(FKL_IS_I64(val),"sys.i64?")
void SYS_f64_p(ARGL) PREDICATE(FKL_IS_F64(val),"sys.i64?")
void SYS_byts_p(ARGL) PREDICATE(FKL_IS_BYTS(val),"sys.byts?")
void SYS_pair_p(ARGL) PREDICATE(FKL_IS_PAIR(val),"sys.pair?")
void SYS_symbol_p(ARGL) PREDICATE(FKL_IS_SYM(val),"sys.symbol?")
void SYS_string_p(ARGL) PREDICATE(FKL_IS_STR(val),"sys.string?")
void SYS_error_p(ARGL) PREDICATE(FKL_IS_ERR(val),"sys.error?")
void SYS_procedure_p(ARGL) PREDICATE(FKL_IS_PROC(val)||FKL_IS_DLPROC(val)||FKL_IS_FLPROC(val),"sys.procedure?")
void SYS_proc_p(ARGL) PREDICATE(FKL_IS_PROC(val),"sys.proc?")
void SYS_dlproc_p(ARGL) PREDICATE(FKL_IS_DLPROC(val),"sys.dlproc?")
void SYS_flproc_p(ARGL) PREDICATE(FKL_IS_FLPROC(val),"sys.flproc?")
void SYS_vector_p(ARGL) PREDICATE(FKL_IS_VECTOR(val),"sys.vector?")
void SYS_memory_p(ARGL) PREDICATE(FKL_IS_MEM(val),"sys.memory?")
void SYS_chanl_p(ARGL) PREDICATE(FKL_IS_CHAN(val),"sys.chanl?")
void SYS_ref_p(ARGL) PREDICATE(FKL_IS_REF(val)||FKL_IS_CHF(val),"sys.ref?")
void SYS_dll_p(ARGL) PREDICATE(FKL_IS_DLL(val),"sys.dll?")

#undef ARGL
#undef PREDICATE
//end
