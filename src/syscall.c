#include<fakeLisp/reader.h>
#include<fakeLisp/fklni.h>
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
extern FklVMlist GlobVMs;
extern void* ThreadVMfunc(void* p);
extern void* ThreadVMdlprocFunc(void* p);
extern void* ThreadVMflprocFunc(void* p);

//syscalls

#define ARGL FklVM* exe
void SYS_car(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklNiGetArg(stack);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.car",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.car",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PAIR(obj))
		FKL_RAISE_BUILTIN_ERROR("sys.car",FKL_WRONGARG,runnable,exe);
	fklNiReturn(obj,stack);
	fklNiReturn(FKL_MAKE_VM_REF(&obj->u.pair->car),stack);
}

void SYS_cdr(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklNiGetArg(stack);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.cdr",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.cdr",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PAIR(obj))
		FKL_RAISE_BUILTIN_ERROR("sys.cdr",FKL_WRONGARG,runnable,exe);
	fklNiReturn(obj,stack);
	fklNiReturn(FKL_MAKE_VM_REF(&obj->u.pair->cdr),stack);
}

void SYS_cons(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* car=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* cdr=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.cons",FKL_TOOMANYARG,runnable,exe);
	if(!car||!cdr)
		FKL_RAISE_BUILTIN_ERROR("sys.cons",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* pair=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),exe->heap);
	pair->u.pair->car=car;
	pair->u.pair->cdr=cdr;
	FKL_SET_RETURN(__func__,pair,stack);
}

void SYS_append(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cur=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue** prev=&retval;
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap))
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
			fklVMstrCat(&(*prev)->u.str,cur->u.str);
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
	fklNiResBp(stack);
	FKL_SET_RETURN(__func__,retval,stack);
}

void SYS_eq(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fir=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* sec=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.eq",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR("sys.eq",FKL_TOOFEWARG,runnable,exe);
	FKL_SET_RETURN(__func__,fir==sec
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_eqn(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fir=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* sec=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.eqn",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR("sys.eqn",FKL_TOOFEWARG,runnable,exe);
	if((FKL_IS_F64(fir)||fklIsInt(fir))&&(FKL_IS_F64(sec)||fklIsInt(sec)))
		FKL_SET_RETURN(__func__
				,fabs((FKL_IS_F64(fir)?fir->u.f64:fklGetInt(fir))
					-(FKL_IS_F64(sec)?sec->u.f64:fklGetInt(sec)))
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
		FKL_SET_RETURN(__func__,!fklVMstrcmp(fir->u.str,sec->u.str)
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,stack);
	else
		FKL_RAISE_BUILTIN_ERROR("sys.eqn",FKL_WRONGARG,runnable,exe);
}

void SYS_equal(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fir=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* sec=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.equal",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR("sys.equal",FKL_TOOFEWARG,runnable,exe);
	FKL_SET_RETURN(__func__,(fklVMvaluecmp(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_add(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* cur=fklPopGetAndMark(stack,exe->heap);
	int64_t r64=0;
	double rd=0.0;
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap))
	{
		if(fklIsInt(cur))
			r64+=fklGetInt(cur);
		else if(FKL_IS_F64(cur))
			rd+=cur->u.f64;
		else
			FKL_RAISE_BUILTIN_ERROR("sys.add",FKL_WRONGARG,runnable,exe);
	}
	fklNiResBp(stack);
	if(rd!=0.0)
	{
		rd+=r64;
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
	}
	else
		FKL_SET_RETURN(__func__,fklMakeVMint(r64,exe->heap),stack);
}

void SYS_add_1(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* arg=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(arg)&&!fklIsInt(arg))
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64+1.0;
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&r,exe->heap),stack);
	}
	else
		FKL_SET_RETURN(__func__,fklMakeVMint(fklGetInt(arg)+1,exe->heap),stack);
}

void SYS_sub(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* prev=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* cur=fklPopGetAndMark(stack,exe->heap);
	double rd=0.0;
	int64_t r64=0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR("sys.sub",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(prev)&&!fklIsInt(prev))
		FKL_RAISE_BUILTIN_ERROR("sys.sub",FKL_WRONGARG,runnable,exe);
	if(!cur)
	{
		fklNiResBp(stack);
		if(FKL_IS_F64(prev))
		{
			rd=-prev->u.f64;
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else
			FKL_SET_RETURN(__func__,fklMakeVMint(-fklGetInt(prev),exe->heap),stack);
	}
	else
	{
		for(;cur;cur=fklPopGetAndMark(stack,exe->heap))
		{
			if(fklIsInt(cur))
				r64+=fklGetInt(cur);
			else if(FKL_IS_F64(cur))
				rd+=cur->u.f64;
			else
				FKL_RAISE_BUILTIN_ERROR("sys.sub",FKL_WRONGARG,runnable,exe);
		}
		fklNiResBp(stack);
		if(FKL_IS_F64(prev)||rd!=0.0)
		{
			rd=((FKL_IS_F64(prev))?prev->u.f64:((FKL_IS_I64(prev))?prev->u.i64:FKL_GET_I32(prev)))-rd-r64;
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else
		{
			r64=(FKL_IS_I64(prev))?prev->u.i64:FKL_GET_I32(prev)-r64;
			FKL_SET_RETURN(__func__,fklMakeVMint(r64,exe->heap),stack);
		}
	}
}

void SYS_sub_1(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* arg=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.sub_1",FKL_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(arg)&&!fklIsInt(arg))
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64-1.0;
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&r,exe->heap),stack);
	}
	else
		FKL_SET_RETURN(__func__,fklMakeVMint(fklGetInt(arg)-1,exe->heap),stack);
}

void SYS_mul(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* cur=fklPopGetAndMark(stack,exe->heap);
	double rd=1.0;
	int64_t r64=1;
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap))
	{
		if(fklIsInt(cur))
			r64*=fklGetInt(cur);
		else if(FKL_IS_F64(cur))
			rd*=cur->u.f64;
		else
			FKL_RAISE_BUILTIN_ERROR("sys.mul",FKL_WRONGARG,runnable,exe);
	}
	fklNiResBp(stack);
	if(rd!=1.0)
	{
		rd*=r64;
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
	}
	else
		FKL_SET_RETURN(__func__,fklMakeVMint(r64,exe->heap),stack);
}

void SYS_div(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* prev=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* cur=fklPopGetAndMark(stack,exe->heap);
	int64_t r64=1;
	double rd=1.0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(prev)&&!fklIsInt(prev))
		FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_WRONGARG,runnable,exe);
	if(!cur)
	{
		fklNiResBp(stack);
		if(FKL_IS_F64(prev))
		{
			if(prev->u.f64==0.0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			rd=1/prev->u.f64;
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else
		{
			r64=fklGetInt(prev);
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
				FKL_SET_RETURN(__func__,fklMakeVMint(r64,exe->heap),stack);
			}
		}
	}
	else
	{
		for(;cur;cur=fklPopGetAndMark(stack,exe->heap))
		{
			if(fklIsInt(cur))
				r64*=fklGetInt(cur);
			else if(FKL_IS_F64(cur))
				rd*=cur->u.f64;
			else
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_WRONGARG,runnable,exe);
		}
		if((r64)==0)
			FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
		fklNiResBp(stack);
		if(FKL_IS_F64(prev)||rd!=1.0||(fklIsInt(prev)&&fklGetInt(prev)%(r64)))
		{
			if(rd==0.0||r64==0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			rd=((double)fklGetInt(prev))/rd/r64;
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&rd,exe->heap),stack);
		}
		else
		{
			if(r64==0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			else
			{
				r64=fklGetInt(prev)/r64;
				FKL_SET_RETURN(__func__,fklMakeVMint(r64,exe->heap),stack);
			}
		}
	}
}

void SYS_rem(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fir=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* sec=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.rem",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR("sys.rem",FKL_TOOFEWARG,runnable,exe);
	if(!((FKL_IS_F64(fir)||fklIsInt(fir))&&(FKL_IS_F64(sec)||fklIsInt(sec))))
		FKL_RAISE_BUILTIN_ERROR("sys.rem",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(fir)||FKL_IS_F64(sec))
	{
		double af=(FKL_IS_F64(fir))?fir->u.f64:fklGetInt(fir);
		double as=(FKL_IS_F64(sec))?sec->u.f64:fklGetInt(sec);
		if(as==0.0)
			FKL_RAISE_BUILTIN_ERROR("sys.rem",FKL_DIVZERROERROR,runnable,exe);
		double r=fmod(af,as);
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_F64,&r,exe->heap),stack);
	}
	else
	{
		int64_t r=fklGetInt(fir)%fklGetInt(sec);
		FKL_SET_RETURN(__func__,fklMakeVMint(r,exe->heap),stack);
	}
}

void SYS_gt(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int r=1;
	FklVMvalue* cur=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.gt",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)||fklIsInt(prev))
					&&(FKL_IS_F64(cur)||fklIsInt(cur)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:fklGetInt(prev))
							-((FKL_IS_F64(cur))?cur->u.f64
								:fklGetInt(cur)))
						>0.0);
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(fklVMstrcmp(prev->u.str,cur->u.str)>0);
			else
				FKL_RAISE_BUILTIN_ERROR("sys.gt",FKL_WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap));
	fklNiResBp(stack);
	FKL_SET_RETURN(__func__,r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_ge(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int r=1;
	FklVMvalue* cur=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.ge",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)||fklIsInt(prev))
					&&(FKL_IS_F64(cur)||fklIsInt(cur)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:fklGetInt(prev))
							-((FKL_IS_F64(cur))?cur->u.f64
								:fklGetInt(cur)))
						>=0.0);
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(fklVMstrcmp(prev->u.str,cur->u.str)>=0);
			else
				FKL_RAISE_BUILTIN_ERROR("sys.ge",FKL_WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap));
	fklNiResBp(stack);
	FKL_SET_RETURN(__func__,r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_lt(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int r=1;
	FklVMvalue* cur=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.lt",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)||fklIsInt(prev))
					&&(FKL_IS_F64(cur)||fklIsInt(cur)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:fklGetInt(prev))
							-((FKL_IS_F64(cur))?cur->u.f64
								:fklGetInt(cur)))
						<0.0);
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(fklVMstrcmp(prev->u.str,cur->u.str)<0);
			else
				FKL_RAISE_BUILTIN_ERROR("sys.lt",FKL_WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap));
	fklNiResBp(stack);
	FKL_SET_RETURN(__func__,r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_le(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	int r=1;
	FklVMvalue* cur=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.le",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap))
	{
		if(prev)
		{
			if((FKL_IS_F64(prev)||fklIsInt(prev))
					&&(FKL_IS_F64(cur)||fklIsInt(prev)))
				r=((((FKL_IS_F64(prev))?prev->u.f64
								:fklGetInt(prev))
							-((FKL_IS_F64(cur))?cur->u.f64
								:fklGetInt(cur)))
						<=0.0);
			else if(FKL_IS_STR(prev)&&FKL_IS_STR(cur))
				r=(fklVMstrcmp(prev->u.str,cur->u.str)<=0);
			else
				FKL_RAISE_BUILTIN_ERROR("sys.le",FKL_WRONGARG,runnable,exe);
		}
		if(!r)
			break;
		prev=cur;
	}
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap));
	fklNiResBp(stack);
	FKL_SET_RETURN(__func__,r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,stack);
}

void SYS_char(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_STR(obj))
	{
		FklVMvalue* pindex=fklPopGetAndMark(stack,exe->heap);
		if(fklNiResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_TOOMANYARG,runnable,exe);
		if(pindex&&!fklIsInt(pindex))
			FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_WRONGARG,runnable,exe);
		if(pindex)
		{
			uint64_t index=fklGetInt(pindex);
			if(index>=obj->u.str->size)
				FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_INVALIDACCESS,runnable,exe);
			FKL_SET_RETURN(__func__,(FklVMptr)&(obj->u.str->str[index]),stack);
			FKL_SET_RETURN(__func__,FKL_MAKE_VM_MREF(1),stack);
		}
		else
		{
			FKL_SET_RETURN(__func__,(FklVMptr)&(obj->u.str->str[0]),stack);
			FKL_SET_RETURN(__func__,FKL_MAKE_VM_MREF(1),stack);
		}
	}
	else
	{
		if(fklNiResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_TOOMANYARG,runnable,exe);
		if(FKL_IS_I32(obj))
			FKL_SET_RETURN(__func__,FKL_MAKE_VM_CHR(FKL_GET_I32(obj)),stack);
		else if(FKL_IS_CHR(obj))
			FKL_SET_RETURN(__func__,obj,stack);
		else if(FKL_IS_F64(obj))
			FKL_SET_RETURN(__func__,FKL_MAKE_VM_CHR((int32_t)obj->u.f64),stack);
		else
			FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_WRONGARG,runnable,exe);
	}
}

void SYS_integer(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.integer",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.integer",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(FKL_GET_CHR(obj)),stack);
	else if(FKL_IS_I32(obj))
		FKL_SET_RETURN(__func__,obj,stack);
	else if(FKL_IS_SYM(obj))
		FKL_SET_RETURN(__func__,fklMakeVMint(FKL_GET_SYM(obj),exe->heap),stack);
	else if(FKL_IS_I64(obj))
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,&obj->u.i64,exe->heap),stack);
	else if(FKL_IS_F64(obj))
	{
		int64_t r=(int64_t)obj->u.f64;
		FKL_SET_RETURN(__func__,fklMakeVMint(r,exe->heap),stack);
	}
	else if(FKL_IS_STR(obj))
	{
		size_t s=obj->u.str->size;
		if(s<5)
		{
			uint8_t r[4]={0};
			switch(s)
			{
				case 4:r[3]=obj->u.str->str[3];
				case 3:r[2]=obj->u.str->str[2];
				case 2:r[1]=obj->u.str->str[1];
				case 1:r[0]=obj->u.str->str[0];
			}
			FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(*(int32_t*)r),stack);
		}
		else
		{
			uint8_t r[8]={0};
			switch(s>8?8:s)
			{
				case 8:r[7]=obj->u.str->str[8];
				case 7:r[6]=obj->u.str->str[6];
				case 6:r[5]=obj->u.str->str[5];
				case 5:r[4]=obj->u.str->str[4];
				case 4:r[3]=obj->u.str->str[3];
				case 3:r[2]=obj->u.str->str[2];
				case 2:r[1]=obj->u.str->str[1];
				case 1:r[0]=obj->u.str->str[0];
			}
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,r,exe->heap),stack);
		}
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.integer",FKL_WRONGARG,runnable,exe);
}

void SYS_i32(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
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
		size_t s=obj->u.str->size;
		uint8_t r[4]={0};
		switch(s)
		{
			case 4:r[3]=obj->u.str->str[3];
			case 3:r[2]=obj->u.str->str[2];
			case 2:r[1]=obj->u.str->str[1];
			case 1:r[0]=obj->u.str->str[0];
		}
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_I32(*(int32_t*)r),stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.i32",FKL_WRONGARG,runnable,exe);
}

void SYS_i64(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
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
		size_t s=obj->u.str->size;
		int64_t r=0;
		switch(s>8?8:s)
		{
			case 8:((uint8_t*)&r)[7]=obj->u.str->str[7];
			case 7:((uint8_t*)&r)[6]=obj->u.str->str[6];
			case 6:((uint8_t*)&r)[5]=obj->u.str->str[5];
			case 5:((uint8_t*)&r)[4]=obj->u.str->str[4];
			case 4:((uint8_t*)&r)[3]=obj->u.str->str[3];
			case 3:((uint8_t*)&r)[2]=obj->u.str->str[2];
			case 2:((uint8_t*)&r)[1]=obj->u.str->str[1];
			case 1:((uint8_t*)&r)[0]=obj->u.str->str[0];
		}
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_I64,&r,exe->heap),stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.i64",FKL_WRONGARG,runnable,exe);
}

void SYS_f64(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
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
	{
		size_t s=obj->u.str->size;
		double r=0;
		switch(s>8?8:s)
		{
			case 8:((uint8_t*)&r)[7]=obj->u.str->str[7];
			case 7:((uint8_t*)&r)[6]=obj->u.str->str[6];
			case 6:((uint8_t*)&r)[5]=obj->u.str->str[5];
			case 5:((uint8_t*)&r)[4]=obj->u.str->str[4];
			case 4:((uint8_t*)&r)[3]=obj->u.str->str[3];
			case 3:((uint8_t*)&r)[2]=obj->u.str->str[2];
			case 2:((uint8_t*)&r)[1]=obj->u.str->str[1];
			case 1:((uint8_t*)&r)[0]=obj->u.str->str[0];
		}
		retval->u.f64=r;
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.f64",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN(__func__,retval,stack);
}

void SYS_as_str(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNewVMvalue(FKL_STR,NULL,exe->heap);
	if(FKL_IS_STR(obj)||FKL_IS_VECTOR(obj))
	{
		FklVMvalue* pstart=fklPopGetAndMark(stack,exe->heap);
		FklVMvalue* psize=fklPopGetAndMark(stack,exe->heap);
		if(fklNiResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOMANYARG,runnable,exe);
		if(pstart)
		{
			if(!fklIsInt(pstart))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			int64_t start=fklGetInt(pstart);
			if((FKL_IS_STR(obj)&&start>=obj->u.str->size)
					||(FKL_IS_VECTOR(obj)&&start>=obj->u.vec->size))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_INVALIDACCESS,runnable,exe);
			int64_t size=FKL_IS_STR(obj)?obj->u.str->size-start:obj->u.vec->size-start;
			if(psize)
			{
				if(!fklIsInt(psize))
					FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
				int64_t tsize=fklGetInt(psize);
				if(tsize>size)
					FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_INVALIDACCESS,runnable,exe);
				size=tsize;
			}
			if(FKL_IS_STR(obj))
				retval->u.str=fklNewVMstr(size,obj->u.str->str+start);
			else
			{
				retval->u.str=fklNewVMstr(size,NULL);
				for(size_t i=0;i<size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[start+i];
					if(!fklIsInt(v))
						FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
					retval->u.str->str[i]=fklGetInt(v);
				}
			}
		}
		else
		{
			if(FKL_IS_STR(obj))
				retval->u.str=fklCopyVMstr(obj->u.str);
			else
			{
				retval->u.str=fklNewVMstr(obj->u.vec->size,NULL);
				FKL_ASSERT(retval->u.str,__func__);
				for(size_t i=0;i<obj->u.vec->size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[i];
					if(!fklIsInt(v))
						FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
					retval->u.str->str[i]=fklGetInt(v);
				}
			}
		}
	}
	else if(FKL_IS_CHR(obj))
	{
		retval->u.str=fklNewEmptyVMstr();
		retval->u.str->size=0;
		for(size_t i=0;obj;i++,obj=fklPopGetAndMark(stack,exe->heap))
		{
			if(!FKL_IS_CHR(obj))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			retval->u.str->size++;
			retval->u.str=(FklVMstr*)realloc(retval->u.str
					,sizeof(FklVMstr)+sizeof(char)*(i+1));
			FKL_ASSERT(retval->u.str,__func__);
			retval->u.str->str[i]=FKL_GET_CHR(obj);
		}
		fklNiResBp(stack);
	}
	else if(fklIsInt(obj))
	{
		FklVMvalue* content=fklPopGetAndMark(stack,exe->heap);
		fklNiResBp(stack);
		if(content)
		{
			if(!FKL_IS_CHR(content))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			size_t size=fklGetInt(obj);
			retval->u.str=fklNewVMstr(size,NULL);
			FKL_ASSERT(retval->u.str,__func__);
			memset(retval->u.str->str,FKL_GET_CHR(content),size);
		}
		else
		{
			if(FKL_IS_I32(obj))
			{
				int32_t r=FKL_GET_I32(obj);
				retval->u.str=fklNewVMstr(sizeof(int32_t),(char*)&r);
			}
			else
				retval->u.str=fklNewVMstr(sizeof(int64_t),(char*)&obj->u.i64);
		}
	}
	else
	{
		if(fklNiResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOMANYARG,runnable,exe);
		else if(FKL_IS_F64(obj))
			retval->u.str=fklNewVMstr(sizeof(double),(char*)&obj->u.f64);
		else if(FKL_IS_SYM(obj))
		{
			FklSid_t sid=FKL_GET_SYM(obj);
			retval->u.str=fklNewVMstr(sizeof(FklSid_t),(char*)&sid);
		}
		else
			FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
	}
	FKL_SET_RETURN(__func__,retval,stack);
}

void SYS_symbol(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.symbol",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.symbol",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_SYM(fklAddSymbolToGlob(((char[]){FKL_GET_CHR(obj),'\0'}))->id),stack);
	else if(FKL_IS_SYM(obj))
		FKL_SET_RETURN(__func__,obj,stack);
	else if(FKL_IS_STR(obj))
	{
		char* str=fklVMstrToCstr(obj->u.str);
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_SYM(fklAddSymbolToGlob(str)->id),stack);
		free(str);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.symbol",FKL_WRONGARG,runnable,exe);
}

void SYS_nth(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* place=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* objlist=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_TOOMANYARG,runnable,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_TOOFEWARG,runnable,exe);
	if(!fklIsInt(place))
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_WRONGARG,runnable,exe);
	ssize_t index=fklGetInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_INVALIDACCESS,runnable,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		if(FKL_IS_PAIR(objPair))
		{
			FKL_SET_RETURN(__func__,objPair,stack);
			FKL_SET_RETURN(__func__
					,FKL_MAKE_VM_REF(&objPair->u.pair->car),stack);
		}
		else
			FKL_SET_RETURN(__func__,FKL_VM_NIL,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_WRONGARG,runnable,exe);
}

void SYS_vref(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* vector=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* place=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_TOOMANYARG,runnable,exe);
	if(!place||!vector)
		FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_TOOFEWARG,runnable,exe);
	if(!fklIsInt(place)||(!FKL_IS_VECTOR(vector)&&!FKL_IS_STR(vector)))
		FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_WRONGARG,runnable,exe);
	ssize_t index=fklGetInt(place);
	size_t size=FKL_IS_STR(vector)?vector->u.str->size:vector->u.vec->size;
	if(index<0||index>=size)
		FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_INVALIDACCESS,runnable,exe);
	if(FKL_IS_STR(vector))
	{
		if(index>=vector->u.str->size)
			FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_INVALIDACCESS,runnable,exe);
		FKL_SET_RETURN(__func__,(FklVMptr)&(vector->u.str->str[index]),stack);
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_MREF(1),stack);
	}
	else if(FKL_IS_VECTOR(vector))
	{
		FKL_SET_RETURN(__func__,vector,stack);
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_REF(&vector->u.vec->base[index]),stack);
	}
}

void SYS_nthcdr(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* place=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* objlist=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.nthcdr",FKL_TOOMANYARG,runnable,exe);
	if(!place||!objlist)
		FKL_RAISE_BUILTIN_ERROR("sys.nthcdr",FKL_TOOFEWARG,runnable,exe);
	if(!fklIsInt(place))
		FKL_RAISE_BUILTIN_ERROR("sys.nthcdr",FKL_WRONGARG,runnable,exe);
	ssize_t index=fklGetInt(place);
	if(index<0)
		FKL_RAISE_BUILTIN_ERROR("sys.nthcdr",FKL_INVALIDACCESS,runnable,exe);
	if(objlist==FKL_VM_NIL||FKL_IS_PAIR(objlist))
	{
		FklVMvalue* objPair=objlist;
		int i=0;
		for(;i<index&&FKL_IS_PAIR(objPair);i++,objPair=fklGetVMpairCdr(objPair));
		if(FKL_IS_PAIR(objPair))
		{
			FKL_SET_RETURN(__func__,objPair,stack);
			FKL_SET_RETURN(__func__
					,FKL_MAKE_VM_REF(&objPair->u.pair->cdr)
					,stack);
		}
		else
			FKL_SET_RETURN(__func__,FKL_VM_NIL,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.nthcdr",FKL_WRONGARG,runnable,exe);
}


void SYS_length(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.length",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.length",FKL_TOOFEWARG,runnable,exe);
	int64_t len=0;
	if(obj==FKL_VM_NIL||FKL_IS_PAIR(obj))
		for(;FKL_IS_PAIR(obj);obj=fklGetVMpairCdr(obj))len++;
	else if(FKL_IS_STR(obj))
		len=obj->u.str->size;
	else if(FKL_IS_CHAN(obj))
		len=fklGetNumVMchanl(obj->u.chan);
	else if(FKL_IS_VECTOR(obj))
		len=obj->u.vec->size;
	else
		FKL_RAISE_BUILTIN_ERROR("sys.length",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN(__func__,fklMakeVMint(len,exe->heap),stack);
}

void SYS_fopen(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* filename=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* mode=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.fopen",FKL_TOOMANYARG,runnable,exe);
	if(!mode||!filename)
		FKL_RAISE_BUILTIN_ERROR("sys.fopen",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_STR(filename)||!FKL_IS_STR(mode))
		FKL_RAISE_BUILTIN_ERROR("sys.fopen",FKL_WRONGARG,runnable,exe);
	char* c_filename=fklVMstrToCstr(filename->u.str);
	char* c_mode=fklVMstrToCstr(mode->u.str);
	FILE* file=fopen(c_filename,c_mode);
	free(c_mode);
	FklVMvalue* obj=NULL;
	if(!file)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR("sys.fopen",c_filename,1,FKL_FILEFAILURE,runnable,exe);
	else
		obj=fklNewVMvalue(FKL_FP,fklNewVMfp(file),heap);
	free(c_filename);
	FKL_SET_RETURN(__func__,obj,stack);
}

void SYS_fclose(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fp=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
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

void SYS_read(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* stream=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
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
		char* parts[]={stream->u.str->str};
		size_t sizes[]={stream->u.str->size};
		uint32_t line=0;
		FklPtrStack* matchStateStack=fklNewPtrStack(32,16);
		fklSplitStringPartsIntoToken(parts,sizes,1,&line,tokenStack,matchStateStack,NULL,NULL);
		while(!fklIsPtrStackEmpty(matchStateStack))
			free(fklPopPtrStack(matchStateStack));
		fklFreePtrStack(matchStateStack);
	}
	FklAstCptr* tmpCptr=fklCreateAstWithTokens(tokenStack,NULL,NULL);
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

void SYS_fgets(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* psize=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* file=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.fgetb",FKL_TOOMANYARG,runnable,exe);
	if(!file||!psize)
		FKL_RAISE_BUILTIN_ERROR("sys.fgetb",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(file)||!fklIsInt(psize))
		FKL_RAISE_BUILTIN_ERROR("sys.fgetb",FKL_WRONGARG,runnable,exe);
	FklVMfp* fp=file->u.fp;
	size_t size=fklGetInt(psize);
	char* str=(char*)malloc(sizeof(char)*size);
	FKL_ASSERT(str,__func__);
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
		str=(char*)realloc(str,sizeof(char)*realRead);
		FKL_ASSERT(str,__func__);
		FklVMvalue* vmstr=fklNewVMvalue(FKL_STR,NULL,exe->heap);
		vmstr->u.str=(FklVMstr*)malloc(sizeof(FklVMstr)+fklGetInt(psize));
		FKL_ASSERT(vmstr->u.str,__func__);
		vmstr->u.str->size=fklGetInt(psize);
		memcpy(vmstr->u.str->str,str,fklGetInt(psize));
		free(str);
		FKL_SET_RETURN(__func__,vmstr,stack);
	}
}

void SYS_prin1(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* file=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.prin1",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.prin1",FKL_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR("sys.prin1",FKL_WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrin1VMvalue(obj,objFile);
	FKL_SET_RETURN(__func__,obj,stack);
}

void SYS_princ(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* file=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.princ",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.princ",FKL_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR("sys.princ",FKL_WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrincVMvalue(obj,objFile);
	FKL_SET_RETURN(__func__,obj,stack);
}

void SYS_dlopen(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* dllName=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.dlopen",FKL_TOOMANYARG,runnable,exe);
	if(!dllName)
		FKL_RAISE_BUILTIN_ERROR("sys.dlopen",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_STR(dllName))
		FKL_RAISE_BUILTIN_ERROR("sys.dlopen",FKL_WRONGARG,runnable,exe);
	char* str=fklVMstrToCstr(dllName->u.str);
	FklVMdllHandle* dll=fklNewVMdll(str);
	if(!dll)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR("sys.dlopen",str,1,FKL_LOADDLLFAILD,runnable,exe);
	free(str);
	FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_DLL,dll,exe->heap),stack);
}

void SYS_dlclose(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* dll=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.dlclose",FKL_TOOMANYARG,runnable,exe);
	if(!dll)
		FKL_RAISE_BUILTIN_ERROR("sys.dlclose",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_DLL(dll))
		FKL_RAISE_BUILTIN_ERROR("sys.dlclose",FKL_WRONGARG,runnable,exe);
	if(dll->u.dll==NULL)
		FKL_RAISE_BUILTIN_ERROR("sys.dlclose",FKL_INVALIDACCESS,runnable,exe);
	fklFreeVMdll(dll->u.dll);
	dll->u.dll=NULL;
	FKL_SET_RETURN(__func__,FKL_VM_NIL,stack);
}

void SYS_dlsym(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMheap* heap=exe->heap;
	FklVMvalue* dll=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* symbol=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.dlsym",FKL_TOOMANYARG,runnable,exe);
	if(!dll||!symbol)
		FKL_RAISE_BUILTIN_ERROR("sys.dlsym",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_STR(symbol)||!FKL_IS_DLL(dll))
		FKL_RAISE_BUILTIN_ERROR("sys.dlsym",FKL_WRONGARG,runnable,exe);
	if(!dll->u.dll)
		FKL_RAISE_BUILTIN_ERROR("sys.dlsym",FKL_INVALIDACCESS,runnable,exe);
	char prefix[]="FKL_";
	char* str=fklVMstrToCstr(symbol->u.str);
	size_t len=strlen(prefix)+strlen(str)+1;
	char* realDlFuncName=(char*)malloc(sizeof(char)*len);
	FKL_ASSERT(realDlFuncName,__func__);
	sprintf(realDlFuncName,"%s%s",prefix,str);
	FklVMdllFunc funcAddress=fklGetAddress(realDlFuncName,dll->u.dll);
	free(realDlFuncName);
	if(!funcAddress)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR("sys.dlsym",str,1,FKL_INVALIDSYMBOL,runnable,exe);
	free(str);
	FklVMdlproc* dlproc=fklNewVMdlproc(funcAddress,dll);
	FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_DLPROC,dlproc,heap),stack);
}

void SYS_argv(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FklVMstack* stack=exe->stack;
	FklVMvalue* retval=NULL;
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.argv",FKL_TOOMANYARG,fklTopPtrStack(exe->rstack),exe);
	retval=FKL_VM_NIL;
	FklVMvalue** tmp=&retval;
	int32_t i=0;
	int argc=fklGetVMargc();
	char** argv=fklGetVMargv();
	for(;i<argc;i++,tmp=&(*tmp)->u.pair->cdr)
	{
		FklVMvalue* cur=fklNewVMvalue(FKL_PAIR,fklNewVMpair(),exe->heap);
		*tmp=cur;
		cur->u.pair->car=fklNewVMvalue(FKL_STR,fklCopyStr(argv[i]),exe->heap);
	}
	FKL_SET_RETURN(__func__,retval,stack);
}

void SYS_go(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	if(exe->VMid==-1)
		FKL_RAISE_BUILTIN_ERROR("sys.go",FKL_CANTCREATETHREAD,runnable,exe);
	FklVMvalue* threadProc=fklPopGetAndMark(stack,exe->heap);
	if(!threadProc)
		FKL_RAISE_BUILTIN_ERROR("sys.go",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PROC(threadProc)&&!FKL_IS_DLPROC(threadProc)&&!FKL_IS_CONT(threadProc))
		FKL_RAISE_BUILTIN_ERROR("sys.go",FKL_WRONGARG,runnable,exe);
	FklVM* threadVM=(FKL_IS_PROC(threadProc)||FKL_IS_CONT(threadProc))?fklNewThreadVM(threadProc->u.proc,exe->heap):fklNewThreadDlprocVM(runnable,exe->heap);
	threadVM->lnt=exe->lnt;
	threadVM->code=exe->code;
	threadVM->size=exe->size;
	FklVMstack* threadVMstack=threadVM->stack;
	FklVMvalue* prevBp=FKL_MAKE_VM_I32(threadVMstack->bp);
	FKL_SET_RETURN(__func__,prevBp,threadVMstack);
	threadVMstack->bp=threadVMstack->tp;
	FklVMvalue* cur=fklPopGetAndMark(stack,exe->heap);
	FklPtrStack* comStack=fklNewPtrStack(32,16);
	for(;cur;cur=fklPopGetAndMark(stack,exe->heap))
		fklPushPtrStack(cur,comStack);
	fklNiResBp(stack);
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

void SYS_chanl(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* maxSize=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.chanl",FKL_TOOMANYARG,runnable,exe);
	if(!maxSize)
		FKL_RAISE_BUILTIN_ERROR("sys.chanl",FKL_TOOFEWARG,runnable,exe);
	if(!fklIsInt(maxSize))
		FKL_RAISE_BUILTIN_ERROR("sys.chanl",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_CHAN,fklNewVMchanl(fklGetInt(maxSize)),exe->heap),stack);
}

void SYS_send(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* message=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* ch=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.send",FKL_TOOMANYARG,runnable,exe);
	if(!message||!ch)
		FKL_RAISE_BUILTIN_ERROR("sys.send",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_CHAN(ch))
		FKL_RAISE_BUILTIN_ERROR("sys.send",FKL_WRONGARG,runnable,exe);
	FklVMsend* t=fklNewVMsend(message);
	FKL_SET_RETURN(__func__,ch,stack);
	fklChanlSend(t,ch->u.chan);
	fklPopVMstack(stack);
	FKL_SET_RETURN(__func__,message,stack);
}

void SYS_recv(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* ch=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.recv",FKL_TOOMANYARG,runnable,exe);
	if(!ch)
		FKL_RAISE_BUILTIN_ERROR("sys.recv",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_CHAN(ch))
		FKL_RAISE_BUILTIN_ERROR("sys.recv",FKL_WRONGARG,runnable,exe);
	FklVMrecv* t=fklNewVMrecv();
	FKL_SET_RETURN(__func__,ch,stack);
	fklChanlRecv(t,ch->u.chan);
	fklPopVMstack(stack);
	FKL_SET_RETURN(__func__,t->v,stack);
	fklFreeVMrecv(t);
}

void SYS_error(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* who=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* type=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* message=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.error",FKL_TOOMANYARG,runnable,exe);
	if(!type||!message)
		FKL_RAISE_BUILTIN_ERROR("sys.error",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(who)&&!FKL_IS_STR(who)))
		FKL_RAISE_BUILTIN_ERROR("sys.error",FKL_WRONGARG,runnable,exe);
	char* str=FKL_IS_STR(who)?fklVMstrToCstr(who->u.str):NULL;
	char* msg=fklVMstrToCstr(message->u.str);
	FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_ERR,fklNewVMerrorWithSid((FKL_IS_SYM(who))?fklGetGlobSymbolWithId(FKL_GET_SYM(who))->symbol:str,FKL_GET_SYM(type),msg),exe->heap),stack);
	free(str);
	free(msg);
}

void SYS_raise(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* err=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.raise",FKL_TOOMANYARG,runnable,exe);
	if(!err)
		FKL_RAISE_BUILTIN_ERROR("sys.raise",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_ERR(err))
		FKL_RAISE_BUILTIN_ERROR("sys.raise",FKL_WRONGARG,runnable,exe);
	fklRaiseVMerror(err,exe);
}

void SYS_call_cc(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* proc=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
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
			tmpRunnable->localenv=fklNewVMvalue(FKL_ENV,fklNewVMenv(tmpProc->prevEnv),exe->heap);
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
		dllfunc(exe);
	}
}

void SYS_apply(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* proc=fklPopGetAndMark(stack,exe->heap);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR("sys.apply",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PTR(proc)||(proc->type!=FKL_PROC&&proc->type!=FKL_CONT&&proc->type!=FKL_DLPROC))
		FKL_RAISE_BUILTIN_ERROR("b.invoke",FKL_INVOKEERROR,runnable,exe);
	FklPtrStack* stack1=fklNewPtrStack(32,16);
	FklVMvalue* value=NULL;
	FklVMvalue* lastList=NULL;
	while((value=fklPopGetAndMark(stack,exe->heap)))
	{
		if(!fklNiResBp(stack))
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
		default:
			break;
	}
}

void SYS_reverse(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
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

void SYS_feof(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fp=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_TOOMANYARG,runnable,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(fp))
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_WRONGARG,runnable,exe);
	if(fp->u.fp==NULL)
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_INVALIDACCESS,runnable,exe);
	FKL_SET_RETURN(__func__,!fp->u.fp->size&&feof(fp->u.fp->fp)?FKL_VM_TRUE:FKL_VM_NIL,stack);
}

void SYS_vector(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* fir=fklPopGetAndMark(stack,exe->heap);
	if(!fir)
		FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_STR(fir))
	{
		if(fklNiResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_TOOMANYARG,runnable,exe);
		size_t size=fir->u.str->size;
		FklVMvec* vec=fklNewVMvec(size,NULL);
		for(size_t i=0;i<size;i++)
			vec->base[i]=FKL_MAKE_VM_CHR(fir->u.str->str[i]);
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_VECTOR,vec,exe->heap),stack);
	}
	else if(fklIsInt(fir))
	{
		size_t size=fklGetInt(fir);
		if(!size)
		{
			FklPtrStack* vstack=fklNewPtrStack(32,16);
			for(FklVMvalue* content=fklPopGetAndMark(stack,exe->heap);content;content=fklPopGetAndMark(stack,exe->heap))
				fklPushPtrStack(content,vstack);
			fklNiResBp(stack);
			FklVMvec* vec=fklNewVMvec(vstack->top,(FklVMvalue**)vstack->base);
			fklFreePtrStack(vstack);
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_VECTOR,vec,exe->heap),stack);
		}
		else
		{
			FklVMvalue* content=fklPopGetAndMark(stack,exe->heap);
			if(fklNiResBp(stack))
				FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_TOOMANYARG,runnable,exe);
			FklVMvec* vec=fklNewVMvec(size,NULL);
			for(size_t i=0;i<size;i++)
				vec->base[i]=FKL_VM_NIL;
			if(content)
			{
				for(size_t i=0;i<size;i++)
					vec->base[i]=content;
			}
			FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_VECTOR,vec,exe->heap),stack);
		}
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_WRONGARG,runnable,exe);
}

void SYS_getdir(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* r=fklTopPtrStack(exe->rstack);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.getdir",FKL_TOOMANYARG,fklTopPtrStack(exe->rstack),exe);
	FklLineNumTabNode* node=fklFindLineNumTabNode(r->cp,exe->lnt);
	if(node->fid)
	{
		char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
		char* rpath=fklRealpath(filename);
		char* dir=fklGetDir(rpath);
		free(rpath);
		FklVMstr* str=fklNewVMstr(strlen(dir),dir);
		free(dir);
		FKL_SET_RETURN(__func__,fklNewVMvalue(FKL_STR,str,exe->heap),stack);
	}
	else
		FKL_SET_RETURN(__func__,FKL_VM_NIL,stack);
}

void SYS_fgetc(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* stream=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.fgetc",FKL_TOOMANYARG,runnable,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR("sys.fgetc",FKL_WRONGARG,runnable,exe);
	FklVMfp* fp=stream?stream->u.fp:fklGetVMstdin()->u.fp;
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR("sys.fgetc",FKL_INVALIDACCESS,runnable,exe);
	if(fp->size)
	{
		fp->size-=1;
		FKL_SET_RETURN(__func__,FKL_MAKE_VM_CHR(fp->prev[0]),stack);
		uint8_t* prev=fp->prev;
		fp->prev=fklCopyMemory(prev+1,sizeof(uint8_t)*fp->size);
		free(prev);
	}
	else
	{
		int ch=fgetc(fp->fp);
		if(ch==EOF)
			FKL_SET_RETURN(__func__,FKL_VM_NIL,stack);
		else
			FKL_SET_RETURN(__func__,FKL_MAKE_VM_CHR(ch),stack);
	}
}

void SYS_fwrite(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	FklVMvalue* file=fklPopGetAndMark(stack,exe->heap);
	if(fklNiResBp(stack))
		FKL_RAISE_BUILTIN_ERROR("sys.fwrite",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.fwrite",FKL_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR("sys.fwrite",FKL_WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	if(FKL_IS_STR(obj)||FKL_IS_SYM(obj))
		fklPrincVMvalue(obj,objFile);
	else if(FKL_IS_CHR(obj))
		fputc(FKL_GET_CHR(obj),objFile);
	else if(FKL_IS_I32(obj))
	{
		int32_t r=FKL_GET_I32(obj);
		fwrite(&r,sizeof(int32_t),1,objFile);
	}
	else if(FKL_IS_I64(obj))
		fwrite(&obj->u.i64,sizeof(int64_t),1,objFile);
	else if(FKL_IS_F64(obj))
		fwrite(&obj->u.f64,sizeof(double),1,objFile);
	else
		FKL_RAISE_BUILTIN_ERROR("sys.fwrite",FKL_WRONGARG,runnable,exe);
	FKL_SET_RETURN(__func__,obj,stack);
}

void SYS_to_str(ARGL)
{
	FklVMstack* stack=exe->stack;
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);
	FklVMvalue* obj=fklPopGetAndMark(stack,exe->heap);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNewVMvalue(FKL_STR,NULL,exe->heap);
	if(FKL_IS_STR(obj)||FKL_IS_VECTOR(obj))
	{
		FklVMvalue* pstart=fklPopGetAndMark(stack,exe->heap);
		FklVMvalue* psize=fklPopGetAndMark(stack,exe->heap);
		if(fklNiResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOMANYARG,runnable,exe);
		if(pstart)
		{
			if(!fklIsInt(pstart))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			int64_t start=fklGetInt(pstart);
			if((FKL_IS_STR(obj)&&start>=obj->u.str->size)
					||(FKL_IS_VECTOR(obj)&&start>=obj->u.vec->size))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_INVALIDACCESS,runnable,exe);
			int64_t size=FKL_IS_STR(obj)?obj->u.str->size-start:obj->u.vec->size-start;
			if(psize)
			{
				if(!fklIsInt(psize))
					FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
				int64_t tsize=fklGetInt(psize);
				if(tsize>size)
					FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_INVALIDACCESS,runnable,exe);
				size=tsize;
			}
			if(FKL_IS_STR(obj))
				retval->u.str=fklNewVMstr(size,obj->u.str->str+start);
			else
			{
				retval->u.str=fklNewVMstr(size,NULL);
				for(size_t i=0;i<size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[start+i];
					if(!fklIsInt(v))
						FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
					retval->u.str->str[i]=fklGetInt(v);
				}
			}
		}
		else
		{
			if(FKL_IS_STR(obj))
				retval->u.str=fklCopyVMstr(obj->u.str);
			else
			{
				retval->u.str=fklNewVMstr(obj->u.vec->size,NULL);
				FKL_ASSERT(retval->u.str,__func__);
				for(size_t i=0;i<obj->u.vec->size;i++)
				{
					FklVMvalue* v=obj->u.vec->base[i];
					if(!fklIsInt(v))
						FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
					retval->u.str->str[i]=fklGetInt(v);
				}
			}
		}
	}
	else if(FKL_IS_CHR(obj))
	{
		retval->u.str=fklNewEmptyVMstr();
		retval->u.str->size=0;
		for(size_t i=0;obj;i++,obj=fklPopGetAndMark(stack,exe->heap))
		{
			if(!FKL_IS_CHR(obj))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			retval->u.str->size++;
			retval->u.str=(FklVMstr*)realloc(retval->u.str
					,sizeof(FklVMstr)+sizeof(char)*(i+1));
			FKL_ASSERT(retval->u.str,__func__);
			retval->u.str->str[i]=FKL_GET_CHR(obj);
		}
		fklNiResBp(stack);
	}
	else if(fklIsInt(obj))
	{
		FklVMvalue* content=fklPopGetAndMark(stack,exe->heap);
		fklNiResBp(stack);
		if(content)
		{
			if(!FKL_IS_CHR(content))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			size_t size=fklGetInt(obj);
			retval->u.str=fklNewVMstr(size,NULL);
			FKL_ASSERT(retval->u.str,__func__);
			memset(retval->u.str->str,FKL_GET_CHR(content),size);
		}
		else
		{
			char buf[32]={0};
			size_t size=snprintf(buf,32,"%ld",fklGetInt(obj));
			retval->u.str=fklNewVMstr(size,buf);
		}
	}
	else
	{
		if(fklNiResBp(stack))
			FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOMANYARG,runnable,exe);
		else if(FKL_IS_F64(obj))
		{
			char buf[64]={0};
			size_t size=snprintf(buf,64,"%lf",obj->u.f64);
			retval->u.str=fklNewVMstr(size,buf);
		}
		else if(FKL_IS_SYM(obj))
		{
			char* symbol=fklGetGlobSymbolWithId(FKL_GET_SYM(obj))->symbol;
			retval->u.str=fklNewVMstr(strlen(symbol),symbol);
		}
		else
			FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
	}
	FKL_SET_RETURN(__func__,retval,stack);
}

#define PREDICATE(condtion,err_infor) {\
	FklVMstack* stack=exe->stack;\
	FklVMrunnable* runnable=fklTopPtrStack(exe->rstack);\
	FklVMvalue* val=fklPopGetAndMark(stack,exe->heap);\
	if(fklNiResBp(stack))\
		FKL_RAISE_BUILTIN_ERROR(err_infor,FKL_TOOFEWARG,runnable,exe);\
	if(!val)\
		FKL_RAISE_BUILTIN_ERROR(err_infor,FKL_TOOFEWARG,runnable,exe);\
	if(condtion)\
		FKL_SET_RETURN(__func__,FKL_VM_TRUE,stack);\
	else\
		FKL_SET_RETURN(__func__,FKL_VM_NIL,stack);\
}

void SYS_not(ARGL) PREDICATE(val==FKL_VM_NIL,"sys.not")
void SYS_null(ARGL) PREDICATE(val==FKL_VM_NIL,"sys.null")
void SYS_atom(ARGL) PREDICATE(!FKL_IS_PAIR(val),"sys.atom?")
void SYS_char_p(ARGL) PREDICATE(FKL_IS_CHR(val),"sys.char?")
void SYS_integer_p(ARGL) PREDICATE(FKL_IS_I32(val)||FKL_IS_I64(val),"sys.integer?")
void SYS_i32_p(ARGL) PREDICATE(FKL_IS_I32(val),"sys.i32?")
void SYS_i64_p(ARGL) PREDICATE(FKL_IS_I64(val),"sys.i64?")
void SYS_f64_p(ARGL) PREDICATE(FKL_IS_F64(val),"sys.i64?")
void SYS_pair_p(ARGL) PREDICATE(FKL_IS_PAIR(val),"sys.pair?")
void SYS_symbol_p(ARGL) PREDICATE(FKL_IS_SYM(val),"sys.symbol?")
void SYS_string_p(ARGL) PREDICATE(FKL_IS_STR(val),"sys.string?")
void SYS_error_p(ARGL) PREDICATE(FKL_IS_ERR(val),"sys.error?")
void SYS_procedure_p(ARGL) PREDICATE(FKL_IS_PROC(val)||FKL_IS_DLPROC(val),"sys.procedure?")
void SYS_proc_p(ARGL) PREDICATE(FKL_IS_PROC(val),"sys.proc?")
void SYS_dlproc_p(ARGL) PREDICATE(FKL_IS_DLPROC(val),"sys.dlproc?")
void SYS_vector_p(ARGL) PREDICATE(FKL_IS_VECTOR(val),"sys.vector?")
void SYS_chanl_p(ARGL) PREDICATE(FKL_IS_CHAN(val),"sys.chanl?")
void SYS_dll_p(ARGL) PREDICATE(FKL_IS_DLL(val),"sys.dll?")

#undef ARGL
#undef PREDICATE
//end
