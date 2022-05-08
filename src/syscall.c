#include<fakeLisp/reader.h>
#include<fakeLisp/symbol.h>
#include<fakeLisp/fklni.h>
#include<fakeLisp/basicADT.h>
#include<fakeLisp/parser.h>
#include<fakeLisp/utils.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<float.h>
#include<setjmp.h>
#include<dlfcn.h>
#include<ctype.h>

extern void applyNativeProc(FklVM*,FklVMproc*,FklVMrunnable*);
extern void invokeContinuation(FklVM*,FklVMcontinuation*);
extern void invokeDlProc(FklVM*,FklVMdlproc*);
extern FklVMlist GlobVMs;
extern void* ThreadVMfunc(void* p);
extern void* ThreadVMdlprocFunc(void* p);
extern void* ThreadVMinvokableUd(void* p);

//syscalls

#define ARGL FklVM* exe
void SYS_car(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.car",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.car",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PAIR(obj))
		FKL_RAISE_BUILTIN_ERROR("sys.car",FKL_WRONGARG,runnable,exe);
	fklNiReturn(obj,&ap,stack);
	fklNiReturn(FKL_MAKE_VM_REF(&obj->u.pair->car),&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_cdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.cdr",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.cdr",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PAIR(obj))
		FKL_RAISE_BUILTIN_ERROR("sys.cdr",FKL_WRONGARG,runnable,exe);
	fklNiReturn(obj,&ap,stack);
	fklNiReturn(FKL_MAKE_VM_REF(&obj->u.pair->cdr),&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_cons(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* car=fklNiGetArg(&ap,stack);
	FklVMvalue* cdr=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.cons",FKL_TOOMANYARG,runnable,exe);
	if(!car||!cdr)
		FKL_RAISE_BUILTIN_ERROR("sys.cons",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* pair=fklNiNewVMvalue(FKL_PAIR,fklNewVMpair(),stack,exe->heap);
	fklSetRef(pair,&pair->u.pair->car,car,exe->heap);
	fklSetRef(pair,&pair->u.pair->cdr,cdr,exe->heap);
	fklNiReturn(pair,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_append(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* retval=FKL_VM_NIL;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue** prev=&retval;
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(*prev==FKL_VM_NIL)
			*prev=fklCopyVMvalue(cur,stack,exe->heap);
		else if(FKL_IS_PAIR(*prev))
		{
			for(;FKL_IS_PAIR(*prev);prev=&(*prev)->u.pair->cdr);
			*prev=fklCopyVMvalue(cur,stack,exe->heap);
		}
		else if(FKL_IS_STR(*prev))
		{
			if(!FKL_IS_PTR(cur))
				FKL_RAISE_BUILTIN_ERROR("sys.append",FKL_WRONGARG,runnable,exe);
			fklVMstrCat((FklVMstr**)&(*prev)->u.str,cur->u.str);
		}
		else if(FKL_IS_VECTOR(*prev))
		{
			if(!FKL_IS_VECTOR(cur))
				FKL_RAISE_BUILTIN_ERROR("sys.append",FKL_WRONGARG,runnable,exe);
			fklVMvecCat((FklVMvec**)&(*prev)->u.vec,cur->u.vec);
		}
		else
			FKL_RAISE_BUILTIN_ERROR("sys.append",FKL_WRONGARG,runnable,exe);
	}
	fklNiResBp(&ap,stack);
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_eq(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.eq",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR("sys.eq",FKL_TOOFEWARG,runnable,exe);
	fklNiReturn(fir==sec
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void SYS_eqn(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.eqn",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR("sys.eqn",FKL_TOOFEWARG,runnable,exe);
	if((FKL_IS_F64(fir)||fklIsInt(fir))&&(FKL_IS_F64(sec)||fklIsInt(sec)))
		fklNiReturn(fabs((FKL_IS_F64(fir)?fir->u.f64:fklGetInt(fir))
					-(FKL_IS_F64(sec)?sec->u.f64:fklGetInt(sec)))
				<DBL_EPSILON
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,&ap
				,stack);
	else if(FKL_IS_CHR(fir)&&FKL_IS_CHR(sec))
		fklNiReturn(fir==sec
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,&ap
				,stack);
	else if(FKL_IS_SYM(fir)&&FKL_IS_SYM(sec))
		fklNiReturn(fir==sec
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,&ap
				,stack);
	else if(FKL_IS_STR(fir)&&FKL_IS_STR(sec))
		fklNiReturn(!fklVMstrcmp(fir->u.str,sec->u.str)
				?FKL_VM_TRUE
				:FKL_VM_NIL
				,&ap
				,stack);
	else
		FKL_RAISE_BUILTIN_ERROR("sys.eqn",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_equal(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.equal",FKL_TOOMANYARG,runnable,exe);
	if(!fir||!sec)
		FKL_RAISE_BUILTIN_ERROR("sys.equal",FKL_TOOFEWARG,runnable,exe);
	fklNiReturn((fklVMvaluecmp(fir,sec))
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void SYS_add(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=0;
	double rd=0.0;
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(fklIsInt(cur))
			r64+=fklGetInt(cur);
		else if(FKL_IS_F64(cur))
			rd+=cur->u.f64;
		else
			FKL_RAISE_BUILTIN_ERROR("sys.add",FKL_WRONGARG,runnable,exe);
	}
	fklNiResBp(&ap,stack);
	if(rd!=0.0)
	{
		rd+=r64;
		fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
	}
	else
		fklNiReturn(fklMakeVMint(r64,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_add_1(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* arg=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(arg)&&!fklIsInt(arg))
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64+1.0;
		fklNiReturn(fklNiNewVMvalue(FKL_F64,&r,stack,exe->heap),&ap,stack);
	}
	else
		fklNiReturn(fklMakeVMint(fklGetInt(arg)+1,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_sub(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	double rd=0.0;
	int64_t r64=0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR("sys.sub",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(prev)&&!fklIsInt(prev))
		FKL_RAISE_BUILTIN_ERROR("sys.sub",FKL_WRONGARG,runnable,exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev))
		{
			rd=-prev->u.f64;
			fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
		}
		else
			fklNiReturn(fklMakeVMint(-fklGetInt(prev),stack,exe->heap),&ap,stack);
	}
	else
	{
		for(;cur;cur=fklNiGetArg(&ap,stack))
		{
			if(fklIsInt(cur))
				r64+=fklGetInt(cur);
			else if(FKL_IS_F64(cur))
				rd+=cur->u.f64;
			else
				FKL_RAISE_BUILTIN_ERROR("sys.sub",FKL_WRONGARG,runnable,exe);
		}
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev)||rd!=0.0)
		{
			rd=((FKL_IS_F64(prev))?prev->u.f64:((FKL_IS_I64(prev))?prev->u.i64:FKL_GET_I32(prev)))-rd-r64;
			fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
		}
		else
		{
			r64=(FKL_IS_I64(prev))?prev->u.i64:FKL_GET_I32(prev)-r64;
			fklNiReturn(fklMakeVMint(r64,stack,exe->heap),&ap,stack);
		}
	}
	fklNiEnd(&ap,stack);
}

void SYS_sub_1(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* arg=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.sub_1",FKL_TOOMANYARG,runnable,exe);
	if(!arg)
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(arg)&&!fklIsInt(arg))
		FKL_RAISE_BUILTIN_ERROR("sys.add_1",FKL_WRONGARG,runnable,exe);
	if(FKL_IS_F64(arg))
	{
		double r=arg->u.f64-1.0;
		fklNiReturn(fklNiNewVMvalue(FKL_F64,&r,stack,exe->heap),&ap,stack);
	}
	else
		fklNiReturn(fklMakeVMint(fklGetInt(arg)-1,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_mul(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	double rd=1.0;
	int64_t r64=1;
	for(;cur;cur=fklNiGetArg(&ap,stack))
	{
		if(fklIsInt(cur))
			r64*=fklGetInt(cur);
		else if(FKL_IS_F64(cur))
			rd*=cur->u.f64;
		else
			FKL_RAISE_BUILTIN_ERROR("sys.mul",FKL_WRONGARG,runnable,exe);
	}
	fklNiResBp(&ap,stack);
	if(rd!=1.0)
	{
		rd*=r64;
		fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
	}
	else
		fklNiReturn(fklMakeVMint(r64,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_div(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* prev=fklNiGetArg(&ap,stack);
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	int64_t r64=1;
	double rd=1.0;
	if(!prev)
		FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_F64(prev)&&!fklIsInt(prev))
		FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_WRONGARG,runnable,exe);
	if(!cur)
	{
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev))
		{
			if(prev->u.f64==0.0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			rd=1/prev->u.f64;
			fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
		}
		else
		{
			r64=fklGetInt(prev);
			if(!r64)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			if(1%r64)
			{
				rd=1.0/r64;
				fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
			}
			else
			{
				r64=1/r64;
				fklNiReturn(fklMakeVMint(r64,stack,exe->heap),&ap,stack);
			}
		}
	}
	else
	{
		for(;cur;cur=fklNiGetArg(&ap,stack))
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
		fklNiResBp(&ap,stack);
		if(FKL_IS_F64(prev)||rd!=1.0||(fklIsInt(prev)&&fklGetInt(prev)%(r64)))
		{
			if(rd==0.0||r64==0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			rd=((double)fklGetInt(prev))/rd/r64;
			fklNiReturn(fklNiNewVMvalue(FKL_F64,&rd,stack,exe->heap),&ap,stack);
		}
		else
		{
			if(r64==0)
				FKL_RAISE_BUILTIN_ERROR("sys.div",FKL_DIVZERROERROR,runnable,exe);
			else
			{
				r64=fklGetInt(prev)/r64;
				fklNiReturn(fklMakeVMint(r64,stack,exe->heap),&ap,stack);
			}
		}
	}
	fklNiEnd(&ap,stack);
}

void SYS_rem(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	FklVMvalue* sec=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
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
		fklNiReturn(fklNiNewVMvalue(FKL_F64,&r,stack,exe->heap),&ap,stack);
	}
	else
	{
		int64_t r=fklGetInt(fir)%fklGetInt(sec);
		fklNiReturn(fklMakeVMint(r,stack,exe->heap),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void SYS_gt(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.gt",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
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
	for(;cur;cur=fklNiGetArg(&ap,stack));
	fklNiResBp(&ap,stack);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void SYS_ge(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.ge",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
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
	for(;cur;cur=fklNiGetArg(&ap,stack));
	fklNiResBp(&ap,stack);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void SYS_lt(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.lt",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
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
	for(;cur;cur=fklNiGetArg(&ap,stack));
	fklNiResBp(&ap,stack);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void SYS_le(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	int r=1;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklVMvalue* prev=NULL;
	if(!cur)
		FKL_RAISE_BUILTIN_ERROR("sys.le",FKL_TOOFEWARG,runnable,exe);
	for(;cur;cur=fklNiGetArg(&ap,stack))
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
	for(;cur;cur=fklNiGetArg(&ap,stack));
	fklNiResBp(&ap,stack);
	fklNiReturn(r
			?FKL_VM_TRUE
			:FKL_VM_NIL
			,&ap
			,stack);
	fklNiEnd(&ap,stack);
}

void SYS_char(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_STR(obj))
	{
		FklVMvalue* pindex=fklNiGetArg(&ap,stack);
		if(fklNiResBp(&ap,stack))
			FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_TOOMANYARG,runnable,exe);
		if(pindex&&!fklIsInt(pindex))
			FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_WRONGARG,runnable,exe);
		if(pindex)
		{
			uint64_t index=fklGetInt(pindex);
			if(index>=obj->u.str->size)
				FKL_RAISE_BUILTIN_ERROR("sys.vref",FKL_INVALIDACCESS,runnable,exe);
			fklNiReturnMref(1,&(obj->u.str->str[index]),&ap,stack);
		}
		else
			fklNiReturnMref(1,&(obj->u.str->str[0]),&ap,stack);
	}
	else
	{
		if(fklNiResBp(&ap,stack))
			FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_TOOMANYARG,runnable,exe);
		if(FKL_IS_I32(obj))
			fklNiReturn(FKL_MAKE_VM_CHR(FKL_GET_I32(obj)),&ap,stack);
		else if(FKL_IS_CHR(obj))
			fklNiReturn(obj,&ap,stack);
		else if(FKL_IS_F64(obj))
			fklNiReturn(FKL_MAKE_VM_CHR((int32_t)obj->u.f64),&ap,stack);
		else
			FKL_RAISE_BUILTIN_ERROR("sys.char",FKL_WRONGARG,runnable,exe);
	}
	fklNiEnd(&ap,stack);
}

void SYS_integer(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.integer",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.integer",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
		fklNiReturn(FKL_MAKE_VM_I32(FKL_GET_CHR(obj)),&ap,stack);
	else if(FKL_IS_I32(obj))
		fklNiReturn(obj,&ap,stack);
	else if(FKL_IS_SYM(obj))
		fklNiReturn(fklMakeVMint(FKL_GET_SYM(obj),stack,exe->heap),&ap,stack);
	else if(FKL_IS_I64(obj))
		fklNiReturn(fklNiNewVMvalue(FKL_I64,(void*)&obj->u.i64,stack,exe->heap),&ap,stack);
	else if(FKL_IS_F64(obj))
	{
		int64_t r=(int64_t)obj->u.f64;
		fklNiReturn(fklMakeVMint(r,stack,exe->heap),&ap,stack);
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
			fklNiReturn(FKL_MAKE_VM_I32(*(int32_t*)r),&ap,stack);
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
			fklNiReturn(fklNiNewVMvalue(FKL_I64,r,stack,exe->heap),&ap,stack);
		}
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.integer",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_to_int(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.to-int",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.to-int",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
		fklNiReturn(FKL_MAKE_VM_I32(FKL_GET_CHR(obj)),&ap,stack);
	else if(FKL_IS_I32(obj))
		fklNiReturn(obj,&ap,stack);
	else if(FKL_IS_I64(obj))
		fklNiReturn(fklNiNewVMvalue(FKL_I64,(void*)&obj->u.i64,stack,exe->heap),&ap,stack);
	else if(FKL_IS_F64(obj))
	{
		int64_t r=(int64_t)obj->u.f64;
		fklNiReturn(fklMakeVMint(r,stack,exe->heap),&ap,stack);
	}
	else if(FKL_IS_STR(obj))
	{
		char* c_str=fklCharBufToStr(obj->u.str->str,obj->u.str->size);
		if(!fklIsNum(c_str))
			fklNiReturn(FKL_VM_NIL,&ap,stack);
		else
			fklNiReturn(fklMakeVMint(fklStringToInt(c_str),stack,exe->heap),&ap,stack);
		free(c_str);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.to-int",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_to_f64(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.to-f64",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.to-f64",FKL_TOOFEWARG,runnable,exe);
	if(fklIsInt(obj)||FKL_IS_CHR(obj))
	{
		double r=fklIsInt(obj)?fklGetInt(obj):FKL_GET_CHR(obj);
		fklNiReturn(fklNiNewVMvalue(FKL_I64,&r,stack,exe->heap),&ap,stack);
	}
	else if(FKL_IS_F64(obj))
		fklNiReturn(fklNiNewVMvalue(FKL_I64,(void*)&obj->u.i64,stack,exe->heap),&ap,stack);
	else if(FKL_IS_STR(obj))
	{
		char* c_str=fklCharBufToStr(obj->u.str->str,obj->u.str->size);
		if(!fklIsNum(c_str))
			fklNiReturn(FKL_VM_NIL,&ap,stack);
		else
		{
			double d=fklStringToDouble(c_str);
			fklNiReturn(fklNiNewVMvalue(FKL_F64,&d,stack,exe->heap),&ap,stack);
		}
		free(c_str);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.to-f64",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_i32(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.i32",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.i32",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
		fklNiReturn(FKL_MAKE_VM_I32(FKL_GET_CHR(obj)),&ap,stack);
	else if(FKL_IS_I32(obj))
		fklNiReturn(obj,&ap,stack);
	else if(FKL_IS_SYM(obj))
		fklNiReturn(FKL_MAKE_VM_I32(FKL_GET_SYM(obj)),&ap,stack);
	else if(FKL_IS_I64(obj))
		fklNiReturn(FKL_MAKE_VM_I32((int32_t)obj->u.i64),&ap,stack);
	else if(FKL_IS_F64(obj))
		fklNiReturn(FKL_MAKE_VM_I32((int32_t)obj->u.f64),&ap,stack);
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
		fklNiReturn(FKL_MAKE_VM_I32(*(int32_t*)r),&ap,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.i32",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_i64(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.i64",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.i64",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
	{
		int8_t r=FKL_GET_CHR(obj);
		fklNiReturn(fklNiNewVMvalue(FKL_I64,&r,stack,exe->heap),&ap,stack);
	}
	else if(FKL_IS_I32(obj))
	{
		int32_t r=FKL_GET_I32(obj);
		fklNiReturn(fklNiNewVMvalue(FKL_I64,&r,stack,exe->heap),&ap,stack);
	}
	else if(FKL_IS_SYM(obj))
	{
		int64_t r=FKL_GET_SYM(obj);
		fklNiReturn(fklNiNewVMvalue(FKL_I64,&r,stack,exe->heap),&ap,stack);
	}
	else if(FKL_IS_I64(obj))
		fklNiReturn(fklNiNewVMvalue(FKL_I64,(void*)&obj->u.i64,stack,exe->heap),&ap,stack);
	else if(FKL_IS_F64(obj))
	{
		int64_t r=(int64_t)obj->u.f64;
		fklNiReturn(fklNiNewVMvalue(FKL_I64,&r,stack,exe->heap),&ap,stack);
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
		fklNiReturn(fklNiNewVMvalue(FKL_I64,&r,stack,exe->heap),&ap,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.i64",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_f64(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.f64",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.f64",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNiNewVMvalue(FKL_F64,NULL,stack,exe->heap);
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
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_as_str(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNiNewVMvalue(FKL_STR,NULL,stack,exe->heap);
	if(FKL_IS_STR(obj)||FKL_IS_VECTOR(obj))
	{
		FklVMvalue* pstart=fklNiGetArg(&ap,stack);
		FklVMvalue* psize=fklNiGetArg(&ap,stack);
		if(fklNiResBp(&ap,stack))
			FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOMANYARG,runnable,exe);
		if(pstart)
		{
			if(!fklIsInt(pstart))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			int64_t start=fklGetInt(pstart);
			if((FKL_IS_STR(obj)&&(start<0||start>=obj->u.str->size))
					||(FKL_IS_VECTOR(obj)&&(start<0||start>=obj->u.vec->size)))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_INVALIDACCESS,runnable,exe);
			int64_t size=FKL_IS_STR(obj)?obj->u.str->size-start:obj->u.vec->size-start;
			if(psize)
			{
				if(!fklIsInt(psize))
					FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
				int64_t tsize=fklGetInt(psize);
				if(tsize<0||tsize>size)
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
		for(size_t i=0;obj;i++,obj=fklNiGetArg(&ap,stack))
		{
			if(!FKL_IS_CHR(obj))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			retval->u.str->size++;
			retval->u.str=(FklVMstr*)realloc(retval->u.str
					,sizeof(FklVMstr)+sizeof(char)*(i+1));
			FKL_ASSERT(retval->u.str,__func__);
			retval->u.str->str[i]=FKL_GET_CHR(obj);
		}
		fklNiResBp(&ap,stack);
	}
	else if(fklIsInt(obj))
	{
		FklVMvalue* content=fklNiGetArg(&ap,stack);
		fklNiResBp(&ap,stack);
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
		if(fklNiResBp(&ap,stack))
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
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_symbol(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.symbol",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.symbol",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_CHR(obj))
		fklNiReturn(FKL_MAKE_VM_SYM(fklAddSymbolToGlob(((char[]){FKL_GET_CHR(obj),'\0'}))->id),&ap,stack);
	else if(FKL_IS_SYM(obj))
		fklNiReturn(obj,&ap,stack);
	else if(FKL_IS_STR(obj))
	{
		char* str=fklVMstrToCstr(obj->u.str);
		fklNiReturn(FKL_MAKE_VM_SYM(fklAddSymbolToGlob(str)->id),&ap,stack);
		free(str);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.symbol",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_nth(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
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
			fklNiReturn(objPair,&ap,stack);
			fklNiReturn(FKL_MAKE_VM_REF(&objPair->u.pair->car),&ap,stack);
		}
		else
			fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.nth",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_vref(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* vector=fklNiGetArg(&ap,stack);
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
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
		fklNiReturnMref(1,&(vector->u.str->str[index]),&ap,stack);
	}
	else if(FKL_IS_VECTOR(vector))
	{
		fklNiReturn(vector,&ap,stack);
		fklNiReturn(FKL_MAKE_VM_REF(&vector->u.vec->base[index]),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void SYS_nthcdr(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* place=fklNiGetArg(&ap,stack);
	FklVMvalue* objlist=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
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
			fklNiReturn(objPair,&ap,stack);
			fklNiReturn(FKL_MAKE_VM_REF(&objPair->u.pair->cdr)
					,&ap
					,stack);
		}
		else
			fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.nthcdr",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}


void SYS_length(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
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
	fklNiReturn(fklMakeVMint(len,stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_fopen(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMheap* heap=exe->heap;
	FklVMvalue* filename=fklNiGetArg(&ap,stack);
	FklVMvalue* mode=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
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
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR("sys.fopen",c_filename,1,FKL_FILEFAILURE,exe);
	else
		obj=fklNiNewVMvalue(FKL_FP,fklNewVMfp(file),stack,heap);
	free(c_filename);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_fclose(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.fclose",FKL_TOOFEWARG,runnable,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR("sys.fclose",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(fp))
		FKL_RAISE_BUILTIN_ERROR("sys.fclose",FKL_WRONGARG,runnable,exe);
	if(fp->u.fp==NULL||fklFreeVMfp(fp->u.fp))
		FKL_RAISE_BUILTIN_ERROR("sys.fclose",FKL_INVALIDACCESS,runnable,exe);
	fp->u.fp=NULL;
	fklNiReturn(FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_read(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
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
	fklNiReturn(tmp,&ap,stack);
	free(tmpString);
	fklDeleteCptr(tmpCptr);
	free(tmpCptr);
	fklNiEnd(&ap,stack);
}

void SYS_fgets(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* psize=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
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
		fklNiReturn(FKL_VM_NIL,&ap,stack);
	}
	else
	{
		str=(char*)realloc(str,sizeof(char)*realRead);
		FKL_ASSERT(str,__func__);
		FklVMvalue* vmstr=fklNiNewVMvalue(FKL_STR,NULL,stack,exe->heap);
		vmstr->u.str=(FklVMstr*)malloc(sizeof(FklVMstr)+fklGetInt(psize));
		FKL_ASSERT(vmstr->u.str,__func__);
		vmstr->u.str->size=fklGetInt(psize);
		memcpy(vmstr->u.str->str,str,fklGetInt(psize));
		free(str);
		fklNiReturn(vmstr,&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void SYS_prin1(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.prin1",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.prin1",FKL_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR("sys.prin1",FKL_WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrin1VMvalue(obj,objFile);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_princ(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.princ",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.princ",FKL_TOOFEWARG,runnable,exe);
	if(file&&!FKL_IS_FP(file))
		FKL_RAISE_BUILTIN_ERROR("sys.princ",FKL_WRONGARG,runnable,exe);
	FILE* objFile=file?file->u.fp->fp:stdout;
	fklPrincVMvalue(obj,objFile);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_dlopen(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* dllName=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.dlopen",FKL_TOOMANYARG,runnable,exe);
	if(!dllName)
		FKL_RAISE_BUILTIN_ERROR("sys.dlopen",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_STR(dllName))
		FKL_RAISE_BUILTIN_ERROR("sys.dlopen",FKL_WRONGARG,runnable,exe);
	char* str=fklVMstrToCstr(dllName->u.str);
	FklVMdllHandle* dll=fklNewVMdll(str);
	if(!dll)
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR("sys.dlopen",str,1,FKL_LOADDLLFAILD,exe);
	free(str);
	FklVMvalue* rel=fklNiNewVMvalue(FKL_DLL,dll,stack,exe->heap);
	fklInitVMdll(rel);
	fklNiReturn(rel,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_dlsym(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMheap* heap=exe->heap;
	FklVMvalue* dll=fklNiGetArg(&ap,stack);
	FklVMvalue* symbol=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
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
		FKL_RAISE_BUILTIN_INVALIDSYMBOL_ERROR("sys.dlsym",str,1,FKL_INVALIDSYMBOL,exe);
	free(str);
	FklVMdlproc* dlproc=fklNewVMdlproc(funcAddress,dll);
	fklNiReturn(fklNiNewVMvalue(FKL_DLPROC,dlproc,stack,heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_argv(FklVM* exe,pthread_rwlock_t* pGClock)
{
	FKL_NI_BEGIN(exe);
	FklVMvalue* retval=NULL;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.argv",FKL_TOOMANYARG,exe->rhead,exe);
	retval=FKL_VM_NIL;
	FklVMvalue** tmp=&retval;
	int32_t i=0;
	int argc=fklGetVMargc();
	char** argv=fklGetVMargv();
	for(;i<argc;i++,tmp=&(*tmp)->u.pair->cdr)
	{
		FklVMvalue* cur=fklNiNewVMvalue(FKL_PAIR,fklNewVMpair(),stack,exe->heap);
		*tmp=cur;
		cur->u.pair->car=fklNiNewVMvalue(FKL_STR,fklNewVMstr(strlen(argv[i]),argv[i]),stack,exe->heap);
	}
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_go(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	if(exe->VMid==-1)
		FKL_RAISE_BUILTIN_ERROR("sys.go",FKL_CANTCREATETHREAD,runnable,exe);
	FklVMvalue* threadProc=fklNiGetArg(&ap,stack);
	if(!threadProc)
		FKL_RAISE_BUILTIN_ERROR("sys.go",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PROC(threadProc)&&!FKL_IS_DLPROC(threadProc)&&!FKL_IS_CONT(threadProc)&&!fklIsInvokableUd(threadProc))
		FKL_RAISE_BUILTIN_ERROR("sys.go",FKL_WRONGARG,runnable,exe);
	FklVM* threadVM=(FKL_IS_PROC(threadProc)||FKL_IS_CONT(threadProc))?fklNewThreadVM(threadProc->u.proc,exe->heap):fklNewThreadDlprocVM(runnable,exe->heap);
	threadVM->lnt=exe->lnt;
	threadVM->code=exe->code;
	threadVM->size=exe->size;
	FklVMstack* threadVMstack=threadVM->stack;
	FklVMvalue* prevBp=FKL_MAKE_VM_I32(threadVMstack->bp);
	fklPushVMvalue(prevBp,threadVMstack);
	threadVMstack->bp=threadVMstack->tp;
	FklVMvalue* cur=fklNiGetArg(&ap,stack);
	FklPtrStack* comStack=fklNewPtrStack(32,16);
	for(;cur;cur=fklNiGetArg(&ap,stack))
		fklPushPtrStack(cur,comStack);
	fklNiResBp(&ap,stack);
	while(!fklIsPtrStackEmpty(comStack))
	{
		FklVMvalue* tmp=fklPopPtrStack(comStack);
		fklPushVMvalue(tmp,threadVMstack);
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
		void* a[2]={threadVM,threadProc->u.ud};
		void** p=(void**)fklCopyMemory(a,sizeof(a));
		faildCode=pthread_create(&threadVM->tid,NULL,ThreadVMinvokableUd,p);
	}
	if(faildCode)
	{
		fklDeleteCallChain(threadVM);
		fklFreeVMstack(threadVM->stack);
		threadVM->stack=NULL;
		threadVM->lnt=NULL;
		fklNiReturn(FKL_MAKE_VM_I32(faildCode),&ap,stack);
	}
	else
		fklNiReturn(chan,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_chanl(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* maxSize=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.chanl",FKL_TOOMANYARG,runnable,exe);
	if(!maxSize)
		FKL_RAISE_BUILTIN_ERROR("sys.chanl",FKL_TOOFEWARG,runnable,exe);
	if(!fklIsInt(maxSize))
		FKL_RAISE_BUILTIN_ERROR("sys.chanl",FKL_WRONGARG,runnable,exe);
	fklNiReturn(fklNiNewVMvalue(FKL_CHAN,fklNewVMchanl(fklGetInt(maxSize)),stack,exe->heap),&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_send(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* message=fklNiGetArg(&ap,stack);
	FklVMvalue* ch=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.send",FKL_TOOMANYARG,runnable,exe);
	if(!message||!ch)
		FKL_RAISE_BUILTIN_ERROR("sys.send",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_CHAN(ch))
		FKL_RAISE_BUILTIN_ERROR("sys.send",FKL_WRONGARG,runnable,exe);
	FklVMsend* t=fklNewVMsend(message);
	fklChanlSend(t,ch->u.chan);
	fklNiReturn(message,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_recv(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* ch=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.recv",FKL_TOOMANYARG,runnable,exe);
	if(!ch)
		FKL_RAISE_BUILTIN_ERROR("sys.recv",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_CHAN(ch))
		FKL_RAISE_BUILTIN_ERROR("sys.recv",FKL_WRONGARG,runnable,exe);
	FklVMrecv* t=fklNewVMrecv();
	fklChanlRecv(t,ch->u.chan);
	fklNiReturn(t->v,&ap,stack);
	fklFreeVMrecv(t);
	fklNiEnd(&ap,stack);
}

void SYS_error(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* who=fklNiGetArg(&ap,stack);
	FklVMvalue* type=fklNiGetArg(&ap,stack);
	FklVMvalue* message=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.error",FKL_TOOMANYARG,runnable,exe);
	if(!type||!message)
		FKL_RAISE_BUILTIN_ERROR("sys.error",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_SYM(type)||!FKL_IS_STR(message)||(!FKL_IS_SYM(who)&&!FKL_IS_STR(who)))
		FKL_RAISE_BUILTIN_ERROR("sys.error",FKL_WRONGARG,runnable,exe);
	char* str=FKL_IS_STR(who)?fklVMstrToCstr(who->u.str):NULL;
	char* msg=fklVMstrToCstr(message->u.str);
	fklNiReturn(fklNiNewVMvalue(FKL_ERR,fklNewVMerrorWithSid((FKL_IS_SYM(who))?fklGetGlobSymbolWithId(FKL_GET_SYM(who))->symbol:str,FKL_GET_SYM(type),msg),stack,exe->heap),&ap,stack);
	free(str);
	free(msg);
	fklNiEnd(&ap,stack);
}

void SYS_raise(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* err=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.raise",FKL_TOOMANYARG,runnable,exe);
	if(!err)
		FKL_RAISE_BUILTIN_ERROR("sys.raise",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_ERR(err))
		FKL_RAISE_BUILTIN_ERROR("sys.raise",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
	fklRaiseVMerror(err,exe);
}

void SYS_call_cc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.call/cc",FKL_TOOMANYARG,runnable,exe);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR("sys.call/cc",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PTR(proc)||(proc->type!=FKL_PROC&&proc->type!=FKL_CONT&&proc->type!=FKL_DLPROC))
		FKL_RAISE_BUILTIN_ERROR("sys.call/cc",FKL_INVOKEERROR,runnable,exe);
	pthread_rwlock_rdlock(&exe->rlock);
	FklVMvalue* cc=fklNiNewVMvalue(FKL_CONT,fklNewVMcontinuation(ap,stack,exe->rhead,exe->tstack),stack,exe->heap);
	pthread_rwlock_unlock(&exe->rlock);
	fklNiReturn(FKL_MAKE_VM_I32(stack->bp),&ap,stack);
	stack->bp=ap;
	fklNiReturn(cc,&ap,stack);
	if(proc->type==FKL_PROC)
	{
		FklVMproc* tmpProc=proc->u.proc;
		FklVMrunnable* prevProc=fklHasSameProc(tmpProc->scp,exe->rhead);
		if(fklIsTheLastExpress(runnable,prevProc,exe)&&prevProc)
			prevProc->mark=1;
		else
		{
			FklVMrunnable* tmpRunnable=fklNewVMrunnable(tmpProc,exe->rhead);
			tmpRunnable->localenv=fklNiNewVMvalue(FKL_ENV,fklNewVMenv(tmpProc->prevEnv),stack,exe->heap);
			exe->rhead=tmpRunnable;
		}
	}
	else if(proc->type==FKL_CONT)
	{
		FklVMcontinuation* cc=proc->u.cont;
		fklCreateCallChainWithContinuation(exe,cc);
	}
	else
	{
		FklVMdllFunc dllfunc=proc->u.dlproc->func;
		dllfunc(exe);
	}
	fklNiEnd(&ap,stack);
}

void SYS_apply(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* proc=fklNiGetArg(&ap,stack);
	if(!proc)
		FKL_RAISE_BUILTIN_ERROR("sys.apply",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PROC(proc)&&!FKL_IS_CONT(proc)&&!FKL_IS_DLPROC(proc)&&!fklIsInvokableUd(proc))
		FKL_RAISE_BUILTIN_ERROR("b.invoke",FKL_INVOKEERROR,runnable,exe);
	FklPtrStack* stack1=fklNewPtrStack(32,16);
	FklVMvalue* value=NULL;
	FklVMvalue* lastList=NULL;
	while((value=fklNiGetArg(&ap,stack)))
	{
		if(!fklNiResBp(&ap,stack))
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
	fklNiReturn(FKL_MAKE_VM_I32(stack->bp),&ap,stack);
	stack->bp=ap;
	while(!fklIsPtrStackEmpty(stack2))
	{
		FklVMvalue* t=fklPopPtrStack(stack2);
		fklNiReturn(t,&ap,stack);
	}
	fklFreePtrStack(stack2);
	while(!fklIsPtrStackEmpty(stack1))
	{
		FklVMvalue* t=fklPopPtrStack(stack1);
		fklNiReturn(t,&ap,stack);
	}
	fklFreePtrStack(stack1);
	switch(proc->type)
	{
		case FKL_PROC:
			applyNativeProc(exe,proc->u.proc,runnable);
			fklNiEnd(&ap,stack);
			break;
		case FKL_CONT:
			fklNiEnd(&ap,stack);
			invokeContinuation(exe,proc->u.cont);
			break;
		case FKL_DLPROC:
			fklNiEnd(&ap,stack);
			invokeDlProc(exe,proc->u.dlproc);
			break;
		case FKL_USERDATA:
			fklNiEnd(&ap,stack);
			proc->u.ud->t->__invoke(exe,proc->u.ud->data);
			break;
		default:
			break;
	}
}

void SYS_reverse(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.reverse",FKL_TOOMANYARG,runnable,exe);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.reverse",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_PAIR(obj)&&obj!=FKL_VM_NIL)
		FKL_RAISE_BUILTIN_ERROR("sys.reverse",FKL_WRONGARG,runnable,exe);
	FklVMvalue* retval=FKL_VM_NIL;
	if(obj!=FKL_VM_NIL)
	{
		FklVMheap* heap=exe->heap;
		for(FklVMvalue* cdr=obj;cdr!=FKL_VM_NIL;cdr=cdr->u.pair->cdr)
		{
			FklVMvalue* pair=fklNiNewVMvalue(FKL_PAIR,fklNewVMpair(),stack,heap);
            fklSetRef(pair,&pair->u.pair->cdr,retval,heap);
            fklSetRef(pair,&pair->u.pair->car,cdr->u.pair->car,heap);
			retval=pair;
		}
	}
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_feof(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fp=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_TOOMANYARG,runnable,exe);
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_TOOFEWARG,runnable,exe);
	if(!FKL_IS_FP(fp))
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_WRONGARG,runnable,exe);
	if(fp->u.fp==NULL)
		FKL_RAISE_BUILTIN_ERROR("sys.feof",FKL_INVALIDACCESS,runnable,exe);
	fklNiReturn(!fp->u.fp->size&&feof(fp->u.fp->fp)?FKL_VM_TRUE:FKL_VM_NIL,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_vector(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* fir=fklNiGetArg(&ap,stack);
	if(!fir)
		FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_TOOFEWARG,runnable,exe);
	if(FKL_IS_STR(fir)||FKL_IS_VECTOR(fir))
	{
		FklVMvalue* startV=fklNiGetArg(&ap,stack);
		FklVMvalue* sizeV=fklNiGetArg(&ap,stack);
		if(fklNiResBp(&ap,stack))
			FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_TOOMANYARG,runnable,exe);
		if((startV&&!fklIsInt(startV))
				||(sizeV&&!fklIsInt(sizeV)))
			FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_WRONGARG,runnable,exe);
		int64_t start=0;
		size_t size=FKL_IS_STR(fir)?fir->u.str->size:fir->u.vec->size;
		if(startV)
		{
			int64_t tstart=fklGetInt(startV);
			if(tstart<0||tstart>size)
				FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_INVALIDACCESS,runnable,exe);
			start=tstart;
		}
		size=size-start;
		if(sizeV)
		{
			int64_t tsize=fklGetInt(sizeV);
			if(tsize<0||tsize>size)
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_INVALIDACCESS,runnable,exe);
			size=tsize;
		}
		FklVMvec* vec=fklNewVMvec(size,NULL);
		if(FKL_IS_STR(fir))
			for(size_t i=0;i<size;i++)
				vec->base[i]=FKL_MAKE_VM_CHR(fir->u.str->str[i+start]);
		else
			for(size_t i=0;i<size;i++)
				vec->base[i]=fir->u.vec->base[i+start];
		fklNiReturn(fklNiNewVMvalue(FKL_VECTOR,vec,stack,exe->heap),&ap,stack);
	}
	else if(fklIsInt(fir))
	{
		size_t size=fklGetInt(fir);
		if(!size)
		{
			FklPtrStack* vstack=fklNewPtrStack(32,16);
			for(FklVMvalue* content=fklNiGetArg(&ap,stack);content;content=fklNiGetArg(&ap,stack))
				fklPushPtrStack(content,vstack);
			fklNiResBp(&ap,stack);
			FklVMvec* vec=fklNewVMvec(vstack->top,(FklVMvalue**)vstack->base);
			fklFreePtrStack(vstack);
			fklNiReturn(fklNiNewVMvalue(FKL_VECTOR,vec,stack,exe->heap),&ap,stack);
		}
		else
		{
			FklVMvalue* content=fklNiGetArg(&ap,stack);
			if(fklNiResBp(&ap,stack))
				FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_TOOMANYARG,runnable,exe);
			FklVMvec* vec=fklNewVMvec(size,NULL);
			for(size_t i=0;i<size;i++)
				vec->base[i]=FKL_VM_NIL;
			if(content)
			{
				for(size_t i=0;i<size;i++)
					vec->base[i]=content;
			}
			fklNiReturn(fklNiNewVMvalue(FKL_VECTOR,vec,stack,exe->heap),&ap,stack);
		}
	}
//	else if(FKL_IS_VECTOR(fir))
//	{
//		FklVMvalue* startV=fklNiGetArg(&ap,stack);
//		FklVMvalue* sizeV=fklNiGetArg(&ap,stack);
//		if(fklNiResBp(&ap,stack))
//				FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_TOOMANYARG,runnable,exe);
//	}
	else
		FKL_RAISE_BUILTIN_ERROR("sys.vector",FKL_WRONGARG,runnable,exe);
	fklNiEnd(&ap,stack);
}

void SYS_getdir(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* r=exe->rhead;
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.getdir",FKL_TOOMANYARG,exe->rhead,exe);
	FklLineNumTabNode* node=fklFindLineNumTabNode(r->cp,exe->lnt);
	if(node->fid)
	{
		char* filename=fklGetGlobSymbolWithId(node->fid)->symbol;
		char* rpath=fklRealpath(filename);
		char* dir=fklGetDir(rpath);
		free(rpath);
		FklVMstr* str=fklNewVMstr(strlen(dir),dir);
		free(dir);
		fklNiReturn(fklNiNewVMvalue(FKL_STR,str,stack,exe->heap),&ap,stack);
	}
	else
	{
		const char* cwd=fklGetCwd();
		FklVMstr* str=fklNewVMstr(strlen(cwd),cwd);
		fklNiReturn(fklNiNewVMvalue(FKL_STR,str,stack,exe->heap),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void SYS_fgetc(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* stream=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
		FKL_RAISE_BUILTIN_ERROR("sys.fgetc",FKL_TOOMANYARG,runnable,exe);
	if(stream&&!FKL_IS_FP(stream))
		FKL_RAISE_BUILTIN_ERROR("sys.fgetc",FKL_WRONGARG,runnable,exe);
	FklVMfp* fp=stream?stream->u.fp:fklGetVMstdin()->u.fp;
	if(!fp)
		FKL_RAISE_BUILTIN_ERROR("sys.fgetc",FKL_INVALIDACCESS,runnable,exe);
	if(fp->size)
	{
		fp->size-=1;
		fklNiReturn(FKL_MAKE_VM_CHR(fp->prev[0]),&ap,stack);
		uint8_t* prev=fp->prev;
		fp->prev=fklCopyMemory(prev+1,sizeof(uint8_t)*fp->size);
		free(prev);
	}
	else
	{
		int ch=fgetc(fp->fp);
		if(ch==EOF)
			fklNiReturn(FKL_VM_NIL,&ap,stack);
		else
			fklNiReturn(FKL_MAKE_VM_CHR(ch),&ap,stack);
	}
	fklNiEnd(&ap,stack);
}

void SYS_fwrite(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	FklVMvalue* file=fklNiGetArg(&ap,stack);
	if(fklNiResBp(&ap,stack))
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
		fwrite((void*)&obj->u.i64,sizeof(int64_t),1,objFile);
	else if(FKL_IS_F64(obj))
		fwrite((void*)&obj->u.f64,sizeof(double),1,objFile);
	else
		FKL_RAISE_BUILTIN_ERROR("sys.fwrite",FKL_WRONGARG,runnable,exe);
	fklNiReturn(obj,&ap,stack);
	fklNiEnd(&ap,stack);
}

void SYS_to_str(ARGL)
{
	FKL_NI_BEGIN(exe);
	FklVMrunnable* runnable=exe->rhead;
	FklVMvalue* obj=fklNiGetArg(&ap,stack);
	if(!obj)
		FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_TOOFEWARG,runnable,exe);
	FklVMvalue* retval=fklNiNewVMvalue(FKL_STR,NULL,stack,exe->heap);
	if(FKL_IS_STR(obj)||FKL_IS_VECTOR(obj))
	{
		FklVMvalue* pstart=fklNiGetArg(&ap,stack);
		FklVMvalue* psize=fklNiGetArg(&ap,stack);
		if(fklNiResBp(&ap,stack))
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
		for(size_t i=0;obj;i++,obj=fklNiGetArg(&ap,stack))
		{
			if(!FKL_IS_CHR(obj))
				FKL_RAISE_BUILTIN_ERROR("sys.string",FKL_WRONGARG,runnable,exe);
			retval->u.str->size++;
			retval->u.str=(FklVMstr*)realloc(retval->u.str
					,sizeof(FklVMstr)+sizeof(char)*(i+1));
			FKL_ASSERT(retval->u.str,__func__);
			retval->u.str->str[i]=FKL_GET_CHR(obj);
		}
		fklNiResBp(&ap,stack);
	}
	else if(fklIsInt(obj))
	{
		FklVMvalue* content=fklNiGetArg(&ap,stack);
		fklNiResBp(&ap,stack);
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
		if(fklNiResBp(&ap,stack))
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
	fklNiReturn(retval,&ap,stack);
	fklNiEnd(&ap,stack);
}

#define PREDICATE(condtion,err_infor) {\
	FKL_NI_BEGIN(exe);\
	FklVMrunnable* runnable=exe->rhead;\
	FklVMvalue* val=fklNiGetArg(&ap,stack);\
	if(fklNiResBp(&ap,stack))\
		FKL_RAISE_BUILTIN_ERROR(err_infor,FKL_TOOFEWARG,runnable,exe);\
	if(!val)\
		FKL_RAISE_BUILTIN_ERROR(err_infor,FKL_TOOFEWARG,runnable,exe);\
	if(condtion)\
		fklNiReturn(FKL_VM_TRUE,&ap,stack);\
	else\
		fklNiReturn(FKL_VM_NIL,&ap,stack);\
	fklNiEnd(&ap,stack);\
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
